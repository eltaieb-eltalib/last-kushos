#include <windows.h>
#include <stdio.h>
#include <pseh/pseh2.h>
__attribute__((noinline)) static void *makep(HANDLE h){return HeapAlloc(h,0,64);}
__attribute__((noinline)) static int common(void*p){RaiseException(0xE0133755,0,0,NULL);return *(int*)p;}
__attribute__((noinline)) static LONG filt(void*p,EXCEPTION_POINTERS*e){return p&&(e->ExceptionRecord->ExceptionCode==0xE0133755)?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH;}
__attribute__((noinline)) static int probe(HANDLE h){
#ifdef FIXED_LOCAL
void * volatile p=NULL;
#else
void*p=NULL;
#endif
int rc=0;BOOL b=GetCurrentThreadId()!=0;_SEH2_TRY{p=makep(h);rc=common(p);}_SEH2_EXCEPT(filt(p,_SEH2_GetExceptionInformation())){rc=p?42:0;}_SEH2_END;if(b)SetLastError(rc);return rc;}
int main(void){char b[128];int rc=probe(GetProcessHeap());sprintf(b,"SEH local filter returned %d (expected 42)",rc);MessageBoxA(NULL,b,"SEH spill regression",MB_OK);return rc!=42;}
