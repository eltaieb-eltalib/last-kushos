#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <stdio.h>
#include <string.h>
static int failed;
static void result(const char *s,BOOL ok){printf("%-22s %s (error %lu)\n",s,ok?"PASS":"FAIL",ok?0:GetLastError());if(!ok)failed++;}
int main(void){
 HANDLE h,m;DWORD n=0;char b[32]={0};void*p;LARGE_INTEGER pos;OVERLAPPED ov={0};BOOL ok;
 puts("Kush storage compatibility probe v1");
 CreateDirectoryW(L"C:\\KushStorageTest",NULL);
 h=CreateFileW(L"C:\\KushStorageTest\\probe.bin",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 result("create file",h!=INVALID_HANDLE_VALUE);if(h==INVALID_HANDLE_VALUE)return 1;
 result("write",WriteFile(h,"kush-storage-check",18,&n,NULL)&&n==18);
 result("flush",FlushFileBuffers(h));
 pos.QuadPart=0;result("seek",SetFilePointerEx(h,pos,NULL,FILE_BEGIN));
 result("read exact",ReadFile(h,b,18,&n,NULL)&&n==18&&!memcmp(b,"kush-storage-check",18));
 pos.QuadPart=4096;result("grow seek",SetFilePointerEx(h,pos,NULL,FILE_BEGIN));result("grow eof",SetEndOfFile(h));
 m=CreateFileMappingW(h,NULL,PAGE_READWRITE,0,4096,NULL);result("create mapping",m!=NULL);
 if(m){p=MapViewOfFile(m,FILE_MAP_WRITE|FILE_MAP_READ,0,0,4096);result("map view",p!=NULL);if(p){ok=!memcmp(p,"kush-storage-check",18);result("mapped read",ok);memcpy((char*)p+128,"mapped-ok",9);result("flush view",FlushViewOfFile(p,4096));UnmapViewOfFile(p);}CloseHandle(m);}
 result("exclusive lock",LockFileEx(h,LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY,0,1,0,&ov));result("unlock",UnlockFileEx(h,0,1,0,&ov));
 pos.QuadPart=128;result("mapped seek",SetFilePointerEx(h,pos,NULL,FILE_BEGIN));ZeroMemory(b,sizeof(b));result("mapped persisted",ReadFile(h,b,9,&n,NULL)&&n==9&&!memcmp(b,"mapped-ok",9));
 pos.QuadPart=32;SetFilePointerEx(h,pos,NULL,FILE_BEGIN);result("truncate",SetEndOfFile(h));CloseHandle(h);
 result("rename",MoveFileExW(L"C:\\KushStorageTest\\probe.bin",L"C:\\KushStorageTest\\renamed.bin",MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH));
 result("delete",DeleteFileW(L"C:\\KushStorageTest\\renamed.bin"));
 printf("Failures: %d\n",failed);return failed?1:0;
}
