#include <windows.h>
#include <stdio.h>
#include <string.h>
static unsigned scan(DWORD flags, DWORD *error, unsigned *seen) {
    WIN32_FIND_DATAW d; HANDLE h; unsigned count=0;
    *seen=0; SetLastError(0);
    h=FindFirstFileExW(L"C:\\FindFetchTest\\*", FindExInfoBasic, &d, FindExSearchNameMatch, NULL, flags);
    if(h==INVALID_HANDLE_VALUE){*error=GetLastError();return 0;}
    do { if(!wcscmp(d.cFileName,L"alpha.txt"))*seen|=1;
         if(!wcscmp(d.cFileName,L"beta.txt"))*seen|=2;
         if(!wcscmp(d.cFileName,L"subdir"))*seen|=4;
         ++count;
    }while(FindNextFileW(h,&d));
    *error=GetLastError(); FindClose(h); return count;
}
int main(void) {
    HANDLE h; DWORD e0,e2,eb,em; unsigned s0,s2,sb,nb,n0,n2; WIN32_FIND_DATAW d; char msg[768];FILE*f;
    CreateDirectoryW(L"C:\\FindFetchTest",NULL); CreateDirectoryW(L"C:\\FindFetchTest\\subdir",NULL);
    h=CreateFileW(L"C:\\FindFetchTest\\alpha.txt",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL); if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);
    h=CreateFileW(L"C:\\FindFetchTest\\beta.txt",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL); if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);
    n0=scan(0,&e0,&s0);n2=scan(2,&e2,&s2);nb=scan(0x80000000,&eb,&sb);
    h=FindFirstFileExW(L"C:\\FindFetchTest\\missing*",FindExInfoBasic,&d,FindExSearchNameMatch,NULL,2);em=GetLastError();if(h!=INVALID_HANDLE_VALUE)FindClose(h);
    sprintf(msg,"Normal enumeration: count=%u known=%u end=%lu\nLarge-fetch enumeration: count=%u known=%u end=%lu\nUnknown flag: count=%u error=%lu (expected 87)\nLarge-fetch missing pattern: error=%lu (expected 2)\nResult: %s",n0,s0,e0,n2,s2,e2,nb,eb,em,(n0==n2&&s0==7&&s2==7&&e0==18&&e2==18&&eb==87&&em==2)?"PASS":"FAIL");
    f=fopen("A:\\findfetch.txt","w");if(f){fprintf(f,"%s\n",msg);fclose(f);}MessageBoxA(NULL,msg,"Directory large-fetch regression",MB_OK);return (n0==n2&&s2==7&&e2==18&&eb==87&&em==2)?0:1;
}
