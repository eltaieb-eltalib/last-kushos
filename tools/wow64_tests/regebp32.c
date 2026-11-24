/*
 * Kush 1.1: check that EBP/EBX/ESI/EDI survive registry calls in a WOW64
 * process (the Firefox installer crashed with EBP = 0xFFFFFA80 after
 * SHRestricted -> RegQueryValueEx). Output is unbuffered, one line per step.
 *   i686-w64-mingw32-gcc -O1 -fno-omit-frame-pointer -static -o regebp32.exe regebp32.c -ladvapi32
 */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#ifdef STATIC_SHELL32
#include <shlobj.h>
#endif

static volatile unsigned g_r0[4], g_r1[4];
static int g_fail;
static volatile LONG g_apc;
static VOID CALLBACK apc_fn(ULONG_PTR p) { g_apc += (LONG)p; }

#define SAVE(a) __asm__ __volatile__("movl %%ebp,%0; movl %%ebx,%1; movl %%esi,%2; movl %%edi,%3" \
    : "=m"(a[0]), "=m"(a[1]), "=m"(a[2]), "=m"(a[3]))

static void report(const char *name, long r)
{
    int bad = g_r0[0] != g_r1[0]; /* only EBP is reliable (frame pointer); GCC may use EBX/ESI/EDI for the call */
    if (bad) g_fail++;
    printf("%s %-34s ret %ld  ebp %08x->%08x ebx %08x->%08x esi %08x->%08x edi %08x->%08x\n",
           bad ? "FAIL" : "ok  ", name, r, g_r0[0], g_r1[0], g_r0[1], g_r1[1], g_r0[2], g_r1[2], g_r0[3], g_r1[3]);
    fflush(stdout);
}

typedef LONG (NTAPI *PFN_NtOpenKey)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
typedef LONG (NTAPI *PFN_NtQueryValueKey)(HANDLE, PUNICODE_STRING, int, PVOID, ULONG, PULONG);

int main(void)
{
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    PFN_NtOpenKey pOpen = (PFN_NtOpenKey)GetProcAddress(nt, "NtOpenKey");
    PFN_NtQueryValueKey pQuery = (PFN_NtQueryValueKey)GetProcAddress(nt, "NtQueryValueKey");
    static WCHAR kpath[] = L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    static WCHAR vgood[] = L"ProductName", vbad[] = L"NoSuchValueKush";
    UNICODE_STRING ks = { sizeof(kpath) - 2, sizeof(kpath), kpath };
    UNICODE_STRING vg = { sizeof(vgood) - 2, sizeof(vgood), vgood }, vb = { sizeof(vbad) - 2, sizeof(vbad), vbad };
    OBJECT_ATTRIBUTES oa = { sizeof(oa), NULL, &ks, OBJ_CASE_INSENSITIVE, NULL, NULL };
    static BYTE buf[512];
    static char abuf[256];
    static WCHAR wbuf[256];
    HANDLE h = NULL; HKEY k = NULL;
    ULONG rl = 0; DWORD sz, type;
    long r;

    setvbuf(stdout, NULL, _IONBF, 0);
#ifdef STATIC_SHELL32
    /* variant: shell32 imported statically (loaded during process init) */
    { char p[MAX_PATH]; SAVE(g_r0); r = SHGetFolderPathA(NULL, 0x1a, NULL, 0, p); SAVE(g_r1); report("SHGetFolderPathA (static)", r); }
#endif
    SAVE(g_r0); r = pOpen(&h, KEY_READ, &oa); SAVE(g_r1); report("NtOpenKey", r);
    SAVE(g_r0); r = pQuery(h, &vg, 2, buf, sizeof(buf), &rl); SAVE(g_r1); report("NtQueryValueKey existing", r);
    SAVE(g_r0); r = pQuery(h, &vb, 2, buf, sizeof(buf), &rl); SAVE(g_r1); report("NtQueryValueKey missing", r);
    SAVE(g_r0); r = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &k); SAVE(g_r1); report("RegOpenKeyExA", r);
    sz = sizeof(wbuf);
    SAVE(g_r0); r = RegQueryValueExW(k, L"ProductName", NULL, &type, (BYTE *)wbuf, &sz); SAVE(g_r1); report("RegQueryValueExW existing", r);
    sz = sizeof(wbuf);
    SAVE(g_r0); r = RegQueryValueExW(k, L"NoSuchValueKush", NULL, &type, (BYTE *)wbuf, &sz); SAVE(g_r1); report("RegQueryValueExW missing", r);
    sz = sizeof(abuf);
    SAVE(g_r0); r = RegQueryValueExA(k, "ProductName", NULL, &type, (BYTE *)abuf, &sz); SAVE(g_r1); report("RegQueryValueExA existing", r);
    sz = sizeof(abuf);
    SAVE(g_r0); r = RegQueryValueExA(k, "NoSuchValueKush", NULL, &type, (BYTE *)abuf, &sz); SAVE(g_r1); report("RegQueryValueExA missing", r);
    SAVE(g_r0); r = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\NoSuchKeyKush", 0, KEY_READ, &k); SAVE(g_r1); report("RegOpenKeyExA missing key", r);
    sz = sizeof(abuf);
    SAVE(g_r0); r = RegQueryValueExA(HKEY_LOCAL_MACHINE, "NoSuchValueKush", NULL, &type, (BYTE *)abuf, &sz); SAVE(g_r1); report("RegQueryValueExA HKLM predefined", r);
    sz = sizeof(abuf);
    SAVE(g_r0); r = RegQueryValueExA(HKEY_CURRENT_USER, "NoSuchValueKush", NULL, &type, (BYTE *)abuf, &sz); SAVE(g_r1); report("RegQueryValueExA HKCU predefined", r);
    sz = sizeof(wbuf);
    SAVE(g_r0); r = RegQueryValueExW(HKEY_LOCAL_MACHINE, L"NoSuchValueKush", NULL, &type, (BYTE *)wbuf, &sz); SAVE(g_r1); report("RegQueryValueExW HKLM predefined", r);
    SAVE(g_r0); r = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", 0, KEY_READ, &k); SAVE(g_r1); report("RegOpenKeyExA Policies\\Explorer", r);
    SAVE(g_r0); QueueUserAPC(apc_fn, GetCurrentThread(), 5); r = SleepEx(0, TRUE); SAVE(g_r1); report("user APC (SleepEx alertable)", r);
    printf("     apc ran: %ld\n", g_apc);
    SAVE(g_r0); QueueUserAPC(apc_fn, GetCurrentThread(), 7); r = WaitForSingleObjectEx(GetCurrentProcess(), 10, TRUE); SAVE(g_r1); report("user APC (WaitForSingleObjectEx)", r);
    printf("     apc ran: %ld\n", g_apc);
    SAVE(g_r0); r = (long)LoadLibraryA("shell32.dll"); SAVE(g_r1); report("LoadLibraryA shell32", r);
    SAVE(g_r0); r = RegQueryValueExA(HKEY_LOCAL_MACHINE, "NoSuchValueKush", NULL, &type, (BYTE *)abuf, &sz); SAVE(g_r1); report("RegQueryValueExA after shell32", r);
    SAVE(g_r0); r = SleepEx(50, TRUE); SAVE(g_r1); report("SleepEx(50, alertable) after shell32", r);
    {
        HKEY pk = NULL;
        typedef DWORD (WINAPI *PFN_SHRestricted)(DWORD);
        PFN_SHRestricted pSHR = (PFN_SHRestricted)GetProcAddress(GetModuleHandleA("shell32.dll"), (LPCSTR)100);
        DWORD i;
        RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", 0, KEY_READ, &pk);
        sz = sizeof(abuf);
        SAVE(g_r0); r = RegQueryValueExA(pk, "NoDrives", NULL, &type, (BYTE *)abuf, &sz); SAVE(g_r1); report("RegQueryValueExA Policies NoDrives", r);
        sz = sizeof(abuf);
        SAVE(g_r0); r = RegQueryValueExA(pk, "NoSimpleStartMenu", NULL, &type, (BYTE *)abuf, &sz); SAVE(g_r1); report("RegQueryValueExA Policies NoSimpleSM", r);
        for (i = 1; pSHR && i < 0x50; i++)
        {
            char name[40];
            sprintf(name, "SHRestricted(0x%lx)", 0x40000000 | i);
            SAVE(g_r0); r = pSHR(0x40000000 | i); SAVE(g_r1); report(name, r);
        }
    }
    printf("RESULT: %d failures\n", g_fail);
    return g_fail;
}
