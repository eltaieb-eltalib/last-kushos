#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>
static LPCWSTR Faces[] = {L"Inter",L"Inter SemiBold",L"Noto Sans Arabic",L"MS Shell Dlg 2"};
static LPCWSTR Sample = L"اللغة العربية في نظام كوش 123";
static LRESULT CALLBACK Proc(HWND w, UINT m, WPARAM a, LPARAM b) {
 if(m==WM_PAINT){ PAINTSTRUCT ps; HDC dc=BeginPaint(w,&ps); SetBkMode(dc,TRANSPARENT); RECT all;GetClientRect(w,&all);FillRect(dc,&all,(HBRUSH)GetStockObject(WHITE_BRUSH));
  for(int i=0;i<4;i++){int y=12+i*92; WCHAR name[160],real[64];HFONT f=CreateFontW(-20,0,0,0,i==1?600:400,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,Faces[i]);HFONT old=SelectObject(dc,f);GetTextFaceW(dc,64,real);wsprintfW(name,L"Requested: %s | Actual: %s",Faces[i],real);SetTextColor(dc,RGB(13,115,119));TextOutW(dc,16,y,name,lstrlenW(name));SetTextColor(dc,RGB(28,32,38));RECT r={16,y+30,600,y+68};if(i==2)DrawTextW(dc,Sample,-1,&r,DT_RIGHT|DT_RTLREADING|DT_SINGLELINE);else DrawTextW(dc,L"Kush Fluent: Aa Bb 0123456789",-1,&r,DT_SINGLELINE);SelectObject(dc,old);DeleteObject(f);}
  EndPaint(w,&ps);return 0;}
 if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(w,m,a,b);
}
int WINAPI WinMain(HINSTANCE h,HINSTANCE prev,LPSTR args,int show){WNDCLASSW wc={0};wc.lpfnWndProc=Proc;wc.hInstance=h;wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.lpszClassName=L"KushFontProbe";RegisterClassW(&wc);CreateWindowW(wc.lpszClassName,L"Kush physical font selection and RTL probe",WS_OVERLAPPEDWINDOW|WS_VISIBLE,80,65,640,420,NULL,NULL,h,NULL);MSG msg;while(GetMessageW(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return 0;}
