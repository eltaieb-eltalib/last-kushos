/*
 * Kush 1.1: dump what a (64-bit) process received from its parent, to compare
 * a 64-bit child started by a 64-bit parent with one started by a 32-bit
 * (WOW64) parent.   x86_64-w64-mingw32-gcc -O1 -municode -o envdump64.exe envdump64.c
 *   envdump64.exe <outfile>
 */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

int wmain(int argc, wchar_t **argv)
{
    FILE *f = _wfopen(argc > 1 ? argv[1] : L"C:\\envdump.txt", L"w");
    wchar_t buf[2048];
    static const wchar_t *vars[] = { L"SystemDrive", L"SystemRoot", L"windir", L"TEMP", L"TMP", L"APPDATA",
        L"LOCALAPPDATA", L"USERPROFILE", L"ProgramFiles", L"ProgramW6432", L"CommonProgramFiles",
        L"ProgramData", L"ALLUSERSPROFILE", L"PROCESSOR_ARCHITECTURE", L"PROCESSOR_ARCHITEW6432", L"PATH", L"COMSPEC" };
    STARTUPINFOW si; PRTL_USER_PROCESS_PARAMETERS pp = NtCurrentTeb()->ProcessEnvironmentBlock->ProcessParameters;
    unsigned i, n = 0; wchar_t *env, *e;

    if (!f) return 1;
    fwprintf(f, L"CommandLine: [%ls]\n", GetCommandLineW());
    GetCurrentDirectoryW(2048, buf); fwprintf(f, L"CurrentDirectory: [%ls]\n", buf);
    GetModuleFileNameW(NULL, buf, 2048); fwprintf(f, L"ModuleFileName: [%ls]\n", buf);
    for (i = 0; i < sizeof(vars) / sizeof(vars[0]); i++)
    {
        DWORD r = GetEnvironmentVariableW(vars[i], buf, 2048);
        fwprintf(f, L"%%%ls%% = %ls\n", vars[i], r ? buf : L"<missing>");
    }
    env = GetEnvironmentStringsW();
    for (e = env; *e; e += wcslen(e) + 1) n++;
    fwprintf(f, L"environment entries: %u, first: [%ls]\n", n, env);
    GetStartupInfoW(&si);
    fwprintf(f, L"STARTUPINFO cb %lu flags %lx show %u desktop [%ls] title [%ls]\n", si.cb, si.dwFlags, si.wShowWindow,
             si.lpDesktop ? si.lpDesktop : L"(null)", si.lpTitle ? si.lpTitle : L"(null)");
    fwprintf(f, L"std handles: in %p out %p err %p\n", GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), GetStdHandle(STD_ERROR_HANDLE));
    fwprintf(f, L"ProcessParameters %p: ImagePath [%.*ls] CmdLine len %u\n", pp,
             pp->ImagePathName.Length / 2, pp->ImagePathName.Buffer, pp->CommandLine.Length);
    fwprintf(f, L"RESULT: ok\n");
    fclose(f);
    return 0;
}
