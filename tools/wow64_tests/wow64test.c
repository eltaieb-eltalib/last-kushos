/*
 * Kush 1.1: WoW64 smoke test (build as a 32-bit i386 program).
 *   i686-w64-mingw32-gcc -O2 -static -o wow64test.exe wow64test.c
 * Run on Kush: wow64test.exe   -> prints one line per check and "RESULT: n/m passed".
 * Exit code = number of failed checks (0 = all passed).
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int g_pass, g_fail;

static void check(const char *name, BOOL ok, const char *detail)
{
    printf("%s  %-34s %s\n", ok ? "PASS" : "FAIL", name, detail ? detail : "");
    if (ok) g_pass++; else g_fail++;
}

static DWORD WINAPI thread_proc(LPVOID p)
{
    *(volatile LONG *)p = 42;
    return 7;
}

static LONG WINAPI filter(EXCEPTION_POINTERS *ep)
{
    return ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION
           ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
}

int main(void)
{
    char buf[MAX_PATH * 2];
    BOOL wow = FALSE;
    SYSTEM_INFO si;

    printf("Kush WoW64 test: pointer size %u bytes\n", (unsigned)sizeof(void *));
    check("32-bit pointers", sizeof(void *) == 4, NULL);

    /* IsWow64Process */
    {
        typedef BOOL (WINAPI *PFN)(HANDLE, PBOOL);
        PFN p = (PFN)GetProcAddress(GetModuleHandleA("kernel32.dll"), "IsWow64Process");
        BOOL ok = p && p(GetCurrentProcess(), &wow) && wow;
        check("IsWow64Process == TRUE", ok, p ? NULL : "export missing");
    }

    /* Native system info (GetNativeSystemInfo) */
    GetNativeSystemInfo(&si);
    sprintf(buf, "(arch %u)", si.wProcessorArchitecture);
    check("GetNativeSystemInfo = AMD64", si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64, buf);

    /* System directory redirection: system32 should be SysWOW64 for us */
    GetSystemDirectoryA(buf, sizeof(buf));
    check("GetSystemDirectory", buf[0] != 0, buf);
    {
        char wowdir[MAX_PATH] = "";
        typedef UINT (WINAPI *PFN)(LPSTR, UINT);
        PFN p = (PFN)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetSystemWow64DirectoryA");
        if (p) p(wowdir, sizeof(wowdir));
        check("GetSystemWow64Directory", wowdir[0] != 0, wowdir);
    }

    /* Files: create, write, read back, delete */
    {
        const char *path = "C:\\wow64test.tmp";
        const char data[] = "Kush WoW64 file test 1234567890";
        char rd[64] = {0};
        DWORD n = 0;
        BOOL ok = FALSE;
        HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (h != INVALID_HANDLE_VALUE)
        {
            ok = WriteFile(h, data, sizeof(data), &n, NULL) && n == sizeof(data);
            SetFilePointer(h, 0, NULL, FILE_BEGIN);
            ok = ok && ReadFile(h, rd, sizeof(data), &n, NULL) && n == sizeof(data) && !memcmp(rd, data, sizeof(data));
            CloseHandle(h);
            DeleteFileA(path);
        }
        check("file create/write/read/delete", ok, NULL);
    }

    /* Memory */
    {
        char *p = VirtualAlloc(NULL, 1 << 20, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        BOOL ok = p != NULL;
        if (p) { memset(p, 0x5A, 1 << 20); ok = p[(1 << 20) - 1] == 0x5A; VirtualFree(p, 0, MEM_RELEASE); }
        check("VirtualAlloc 1 MB", ok, NULL);
        p = HeapAlloc(GetProcessHeap(), 0, 100000);
        check("HeapAlloc", p != NULL, NULL);
        if (p) HeapFree(GetProcessHeap(), 0, p);
    }

    /* Threads */
    {
        volatile LONG v = 0;
        DWORD code = 0;
        HANDLE t = CreateThread(NULL, 0, thread_proc, (LPVOID)&v, 0, NULL);
        BOOL ok = t && WaitForSingleObject(t, 10000) == WAIT_OBJECT_0 && GetExitCodeThread(t, &code) && code == 7 && v == 42;
        if (t) CloseHandle(t);
        check("CreateThread + wait", ok, NULL);
    }

    /* Structured exceptions (32-bit SEH chain) */
    {
        BOOL caught = FALSE;
        __try1(filter)
        {
            *(volatile int *)0 = 1;
        }
        __except1
        {
            caught = TRUE;
        }
        check("SEH catches access violation", caught, NULL);
    }

    /* Child process: run cmd /c (32-bit cmd from SysWOW64 if redirection works) */
    {
        STARTUPINFOA sa = { sizeof(sa) };
        PROCESS_INFORMATION pi;
        DWORD code = 99;
        char cmd[] = "cmd.exe /c exit 5";
        BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &sa, &pi);
        if (ok)
        {
            ok = WaitForSingleObject(pi.hProcess, 20000) == WAIT_OBJECT_0 && GetExitCodeProcess(pi.hProcess, &code) && code == 5;
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        }
        sprintf(buf, "(exit code %lu)", code);
        check("CreateProcess cmd /c exit 5", ok, buf);
    }

    /* Registry (with Wow6432Node redirection, if any) */
    {
        HKEY k;
        char v[256] = "";
        DWORD sz = sizeof(v);
        BOOL ok = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &k) == ERROR_SUCCESS;
        if (ok) { ok = RegQueryValueExA(k, "ProductName", NULL, NULL, (BYTE *)v, &sz) == ERROR_SUCCESS; RegCloseKey(k); }
        check("registry read ProductName", ok, v);
    }

    printf("RESULT: %d/%d passed\n", g_pass, g_pass + g_fail);
    return g_fail;
}
