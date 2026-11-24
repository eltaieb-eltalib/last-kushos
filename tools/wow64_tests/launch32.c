/*
 * Kush 1.1: a 32-bit program starting a 64-bit one, the way installers do.
 *   i686-w64-mingw32-gcc -O1 -municode -o launch32.exe launch32.c -lshell32
 */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

int wmain(void)
{
    STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi;
    wchar_t cmd[] = L"C:\\envdump64.exe C:\\env_cp32.txt";
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    DWORD code = 99;
    BOOL ok = CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    printf("CreateProcessW = %d (err %lu)\n", ok, GetLastError());
    if (ok) { WaitForSingleObject(pi.hProcess, 60000); GetExitCodeProcess(pi.hProcess, &code); printf("exit code %lx\n", code); }
    sei.fMask = SEE_MASK_NOCLOSEPROCESS; sei.lpFile = L"C:\\envdump64.exe"; sei.lpParameters = L"C:\\env_se32.txt"; sei.nShow = SW_SHOW;
    ok = ShellExecuteExW(&sei);
    printf("ShellExecuteExW = %d (err %lu)\n", ok, GetLastError());
    if (ok && sei.hProcess) { WaitForSingleObject(sei.hProcess, 60000); GetExitCodeProcess(sei.hProcess, &code); printf("exit code %lx\n", code); }
    return 0;
}
