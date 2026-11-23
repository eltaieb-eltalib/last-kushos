#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
static volatile LONG posted,sent,paints,timers,sendfail,done;
static HWND target;
static LRESULT CALLBACK Proc(HWND h,UINT m,WPARAM w,LPARAM l)
{
 if(m==WM_APP+1){InterlockedIncrement(&posted);InvalidateRect(h,NULL,FALSE);return 1;}
 if(m==WM_APP+2){InterlockedIncrement(&sent);return 1;}
 if(m==WM_TIMER){InterlockedIncrement(&timers);return 0;}
 if(m==WM_PAINT){PAINTSTRUCT p;HDC dc=BeginPaint(h,&p);TextOutA(dc,10,10,"Queue regression probe",22);EndPaint(h,&p);InterlockedIncrement(&paints);return 0;}
 return DefWindowProcW(h,m,w,l);
}
static DWORD WINAPI Sender(void *unused)
{
 unsigned i;DWORD_PTR result;(void)unused;
 for(i=0;i<20;i++)if(!SendMessageTimeoutW(target,WM_APP+2,0,0,SMTO_ABORTIFHUNG,1500,&result))InterlockedIncrement(&sendfail);
 InterlockedExchange(&done,1);return 0;
}
int main(void)
{
 WNDCLASSW c={0};MSG m;HANDLE worker;DWORD start,wait;DWORD qs[5],wr[5],elapsed[5];BOOL filtered,pass;
 c.lpfnWndProc=Proc;c.hInstance=GetModuleHandleW(NULL);c.lpszClassName=L"KushQueueProbe";
 if(!RegisterClassW(&c)){printf("FAIL register %lu\n",GetLastError());return 2;}
 target=CreateWindowW(c.lpszClassName,L"Queue regression probe",WS_OVERLAPPEDWINDOW|WS_VISIBLE,150,130,400,180,NULL,NULL,c.hInstance,NULL);
 if(!target){printf("FAIL create %lu\n",GetLastError());return 2;}
 UpdateWindow(target);
 for(unsigned i=0;i<20;i++)PostMessageW(target,WM_APP+1,0,0);
 filtered=PeekMessageW(&m,NULL,WM_APP+100,WM_APP+101,PM_REMOVE);
 SetTimer(target,1,30,NULL);worker=CreateThread(NULL,0,Sender,NULL,0,NULL);start=GetTickCount();
 while(GetTickCount()-start<5000 && (posted<20||sent<20||timers<20||!done)){
  while(PeekMessageW(&m,NULL,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}Sleep(2);
 }
 KillTimer(target,1);DestroyWindow(target);
 while(PeekMessageW(&m,NULL,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}
 for(unsigned i=0;i<5;i++){
 while(PeekMessageW(&m,NULL,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}
 qs[i]=GetQueueStatus(QS_ALLINPUT);start=GetTickCount();wr[i]=MsgWaitForMultipleObjectsEx(0,NULL,200,QS_ALLINPUT,MWMO_INPUTAVAILABLE);elapsed[i]=GetTickCount()-start;
 }
 wait=wr[4];
 for(unsigned i=0;i<5;i++)printf("cycle %u queue=%08lx wait=%lu elapsed=%lu ms\n",i,qs[i],wr[i],elapsed[i]);
 printf("Queue regression probe\nposted=%ld/20 sent=%ld/20 timer=%ld paint=%ld\nfiltered peek returned=%d send failures=%ld worker done=%ld\nempty queue wait=%lu elapsed=%lu ms (timeout=258)\n",posted,sent,timers,paints,filtered,sendfail,done,wait,GetTickCount()-start);
 pass=!filtered&&posted==20&&sent==20&&timers>=20&&paints>0&&sendfail==0&&done&&wait==WAIT_TIMEOUT;
 printf("%s\n",pass?"PASS":"FAIL");if(worker)CloseHandle(worker);return pass?0:1;
}
