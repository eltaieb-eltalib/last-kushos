/*
 * Kush 1.1: what a 32-bit installer sees (NSIS WinVer.nsh / x64.nsh / CPU checks).
 *   i686-w64-mingw32-gcc -O2 -static -o sysinfo32.exe sysinfo32.c
 */
#include <windows.h>
#include <stdio.h>

typedef LONG (WINAPI *PFN_RtlGetVersion)(OSVERSIONINFOEXW *);
typedef BOOL (WINAPI *PFN_IsWow64Process2)(HANDLE, USHORT *, USHORT *);
typedef void (WINAPI *PFN_RtlGetNtVersionNumbers)(DWORD *, DWORD *, DWORD *);

int main(void)
{
    volatile BYTE *kusd = (volatile BYTE *)0x7FFE0000;
    OSVERSIONINFOEXW vi = { sizeof(vi) };
    HMODULE nt = GetModuleHandleA("ntdll.dll"), k32 = GetModuleHandleA("kernel32.dll");
    PFN_RtlGetVersion pRtlGetVersion = (PFN_RtlGetVersion)GetProcAddress(nt, "RtlGetVersion");
    PFN_RtlGetNtVersionNumbers pNums = (PFN_RtlGetNtVersionNumbers)GetProcAddress(nt, "RtlGetNtVersionNumbers");
    PFN_IsWow64Process2 pIW2 = (PFN_IsWow64Process2)GetProcAddress(k32, "IsWow64Process2");
    BOOL wow = FALSE;

    printf("KUSER_SHARED_DATA: %lu.%lu build %lu\n", *(ULONG *)(kusd + 0x26c), *(ULONG *)(kusd + 0x270), *(ULONG *)(kusd + 0x260));
    GetVersionExW((OSVERSIONINFOW *)&vi);
    printf("GetVersionEx: %lu.%lu build %lu\n", vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber);
    if (pRtlGetVersion) { pRtlGetVersion(&vi); printf("RtlGetVersion: %lu.%lu build %lu\n", vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber); }
    if (pNums) { DWORD a, b, c; pNums(&a, &b, &c); printf("RtlGetNtVersionNumbers: %lu.%lu build %lu\n", a, b, c & 0xffff); }
    else printf("RtlGetNtVersionNumbers: missing\n");
    printf("IsProcessorFeaturePresent(SSE2=10): %d\n", IsProcessorFeaturePresent(10));
    IsWow64Process(GetCurrentProcess(), &wow);
    printf("IsWow64Process: %d\n", wow);
    if (pIW2) { USHORT p = 0xffff, n = 0xffff; BOOL r = pIW2(GetCurrentProcess(), &p, &n); printf("IsWow64Process2: ret %d process 0x%x native 0x%x (err %lu)\n", r, p, n, GetLastError()); }
    else printf("IsWow64Process2: missing\n");
    printf("GetUserDefaultUILanguage: 0x%x\n", GetUserDefaultUILanguage());
    return 0;
}
