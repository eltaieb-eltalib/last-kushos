#include <windows.h>
#include <stdio.h>
static HANDLE readh,writeh,started,done;static DWORD typ,elapsed;
static DWORD WINAPI reader(void*x){char b;DWORD n;SetEvent(started);ReadFile(readh,&b,1,&n,NULL);return 0;}
static DWORD WINAPI query(void*x){DWORD t=GetTickCount();typ=GetFileType(readh);elapsed=GetTickCount()-t;SetEvent(done);return 0;}
int main(void){HANDLE r,q;DWORD n,s;FILE*f=fopen("A:\\pipetest.txt","w");CreatePipe(&readh,&writeh,NULL,0);started=CreateEvent(NULL,TRUE,FALSE,NULL);done=CreateEvent(NULL,TRUE,FALSE,NULL);r=CreateThread(NULL,0,reader,NULL,0,NULL);WaitForSingleObject(started,1000);Sleep(50);q=CreateThread(NULL,0,query,NULL,0,NULL);s=WaitForSingleObject(done,300);fprintf(f,"GetFileType during pending pipe read: %s\n",s==WAIT_OBJECT_0?"returned without blocking":"BLOCKED (>300ms), incorrect");fflush(f);WriteFile(writeh,"x",1,&n,NULL);WaitForSingleObject(r,1000);WaitForSingleObject(q,1000);fprintf(f,"type%lu expected3, elapsed%lu ms expected<300\n",typ,elapsed);fclose(f);return 0;}
