#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stddef.h>
#include <stdio.h>
#include <wchar.h>
_Static_assert(sizeof(PROCESSENTRY32W)==568,"Win64 PROCESSENTRY32W size");
_Static_assert(offsetof(PROCESSENTRY32W,szExeFile)==44,"Win64 exe name offset");
_Static_assert(sizeof(HEAPLIST32)==32,"Win64 heap list size");
_Static_assert(sizeof(HEAPENTRY32)==56,"Win64 heap entry size");
int wmain(void)
{
    HANDLE h=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    PROCESSENTRY32W p={0}; unsigned count=0; BOOL own=FALSE,explorer=FALSE;
    wprintf(L"Win64 Toolhelp ABI probe\nPROCESSENTRY32W=%u name offset=%u\nHEAPLIST32=%u HEAPENTRY32=%u\n",(unsigned)sizeof(p),(unsigned)offsetof(PROCESSENTRY32W,szExeFile),(unsigned)sizeof(HEAPLIST32),(unsigned)sizeof(HEAPENTRY32));
    if(h==INVALID_HANDLE_VALUE){wprintf(L"FAIL snapshot error=%lu\n",GetLastError());return 2;}
    p.dwSize=sizeof(p);
    if(Process32FirstW(h,&p)) do {
        wprintf(L"%5lu  %ls\n",p.th32ProcessID,p.szExeFile);
        if(!wcscmp(p.szExeFile,L"thprobe.exe"))own=TRUE;
        if(!wcscmp(p.szExeFile,L"explorer.exe"))explorer=TRUE;
        count++; p.dwSize=sizeof(p);
    } while(Process32NextW(h,&p));
    CloseHandle(h);
    wprintf(L"\n%ls: own full name=%d Explorer full name=%d count=%u\n",own&&explorer?L"PASS":L"FAIL",own,explorer,count);
    return own&&explorer?0:1;
}
