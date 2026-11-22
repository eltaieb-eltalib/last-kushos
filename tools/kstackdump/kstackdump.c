/*
 * Kush 1.1 diagnostic tool: dump the user-mode stacks of every thread of the
 * processes whose image name matches argv[1] (default firefox.exe).
 *
 * For each thread: state from GetThreadContext (RIP/RSP), then every qword on
 * the stack that points into a loaded module, printed as module+RVA. Output goes
 * to OutputDebugString (=> Kush serial log) and stdout. RVAs can be resolved on
 * the host with tools/breakpad_sym.py (Mozilla symbols) or nm (Kush DLLs).
 *
 * Build: x86_64-w64-mingw32-gcc -O2 -static -o kstackdump.exe kstackdump.c -lpsapi
 */
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>

typedef struct { ULONG_PTR Base, Size; char Name[64]; } MODINFO;
static MODINFO g_mods[512];
static int g_nmods;

static void out(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 2] = 0;
    strcat(buf, "\n");
    OutputDebugStringA(buf);
    fputs(buf, stdout);
}

static const MODINFO *find_mod(ULONG_PTR a)
{
    int i;
    for (i = 0; i < g_nmods; i++)
        if (a >= g_mods[i].Base && a < g_mods[i].Base + g_mods[i].Size) return &g_mods[i];
    return NULL;
}

static void load_mods(DWORD pid)
{
    MODULEENTRY32 me;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    g_nmods = 0;
    if (snap == INVALID_HANDLE_VALUE) { out("KSTACK modules: snapshot failed %lu", GetLastError()); return; }
    me.dwSize = sizeof(me);
    if (Module32First(snap, &me))
    {
        do
        {
            if (g_nmods < 512)
            {
                g_mods[g_nmods].Base = (ULONG_PTR)me.modBaseAddr;
                g_mods[g_nmods].Size = me.modBaseSize;
                lstrcpynA(g_mods[g_nmods].Name, me.szModule, sizeof(g_mods[0].Name));
                g_nmods++;
            }
        } while (Module32Next(snap, &me));
    }
    CloseHandle(snap);
}

static void dump_thread(HANDLE proc, DWORD tid)
{
    CONTEXT ctx;
    HANDLE th = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION, FALSE, tid);
    static ULONG_PTR stack[4096];
    SIZE_T got = 0;
    const MODINFO *m;
    int i, n = 0;

    if (!th) { out("KSTACK  tid %lu: OpenThread failed %lu", tid, GetLastError()); return; }
    SuspendThread(th);
    memset(&ctx, 0, sizeof(ctx));
    ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    if (!GetThreadContext(th, &ctx))
    {
        out("KSTACK  tid %lu: GetThreadContext failed %lu", tid, GetLastError());
        ResumeThread(th); CloseHandle(th); return;
    }
    m = find_mod(ctx.Rip);
    out("KSTACK  tid %lu rip %p %s+0x%Ix rsp %p", tid, (void *)ctx.Rip,
        m ? m->Name : "?", m ? ctx.Rip - m->Base : 0, (void *)ctx.Rsp);

    /* Read up to 32 KB of stack, page by page (stop at the first unreadable page). */
    {
        ULONG_PTR a = ctx.Rsp & ~(ULONG_PTR)7;
        SIZE_T total = 0;
        while (total < sizeof(stack))
        {
            SIZE_T chunk = 0x1000 - (a & 0xfff), r = 0;
            if (chunk > sizeof(stack) - total) chunk = sizeof(stack) - total;
            if (!ReadProcessMemory(proc, (LPCVOID)a, (BYTE *)stack + total, chunk, &r) || !r) break;
            total += r; a += r;
        }
        got = total / sizeof(ULONG_PTR);
    }
    for (i = 0; i < (int)got && n < 40; i++)
    {
        m = find_mod(stack[i]);
        if (m)
        {
            out("KSTACK    +%04x %s+0x%Ix", i * 8, m->Name, stack[i] - m->Base);
            n++;
        }
    }
    ResumeThread(th);
    CloseHandle(th);
}

int main(int argc, char **argv)
{
    const char *target = argc > 1 ? argv[1] : "firefox.exe";
    PROCESSENTRY32 pe;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) { out("KSTACK snapshot failed %lu", GetLastError()); return 1; }

    pe.dwSize = sizeof(pe);
    if (Process32First(snap, &pe))
    {
        do
        {
            if (_stricmp(pe.szExeFile, target) == 0)
            {
                THREADENTRY32 te;
                HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe.th32ProcessID);
                out("KSTACK ==== process %s pid %lu threads %lu", pe.szExeFile, pe.th32ProcessID, pe.cntThreads);
                if (!proc) { out("KSTACK OpenProcess failed %lu", GetLastError()); continue; }
                load_mods(pe.th32ProcessID);
                {
                    int i;
                    for (i = 0; i < g_nmods; i++)
                        if (!_stricmp(g_mods[i].Name, "xul.dll") || !_stricmp(g_mods[i].Name, "ntdll.dll") ||
                            !_stricmp(g_mods[i].Name, "user32.dll") || !_stricmp(g_mods[i].Name, "kernel32.dll") ||
                            !_stricmp(g_mods[i].Name, "nss3.dll") || !_stricmp(g_mods[i].Name, "mozglue.dll"))
                            out("KSTACK  module %s base %p", g_mods[i].Name, (void *)g_mods[i].Base);
                }
                te.dwSize = sizeof(te);
                if (Thread32First(snap, &te))
                {
                    do
                    {
                        if (te.th32OwnerProcessID == pe.th32ProcessID) dump_thread(proc, te.th32ThreadID);
                    } while (Thread32Next(snap, &te));
                }
                CloseHandle(proc);
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    out("KSTACK done");
    return 0;
}
