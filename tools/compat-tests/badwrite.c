#include <windows.h>
#include <stdio.h>
static HANDLE eventh;static DWORD flags;static BOOL queried;
static DWORD WINAPI query(void*x){queried=GetHandleInformation(eventh,&flags);return 0;}
int main(void){DWORD n=0,err,wait;BOOL ok;HANDLE t;char text[512];FILE*f=fopen("A:\\badwrite.txt","w");eventh=CreateEventW(NULL,TRUE,FALSE,NULL);SetLastError(0);ok=WriteFile(eventh,"x",1,&n,NULL);err=GetLastError();t=CreateThread(NULL,0,query,NULL,0,NULL);wait=WaitForSingleObject(t,750);sprintf(text,"WriteFile(Event): result=%d error=%lu\nExpected: result=0, error=6\nHandle query after failed write: %s\nQuery result=%d flags=%lu",ok,err,wait==WAIT_OBJECT_0?"RETURNED":"BLOCKED (lock leaked)",queried,flags);if(f){fprintf(f,"%s\n",text);fclose(f);}MessageBoxA(NULL,text,"Non-file WriteFile regression",MB_OK);return wait==WAIT_OBJECT_0?0:1;}
