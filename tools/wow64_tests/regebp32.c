/*
 * Kush 1.1: check that EBP/EBX/ESI/EDI survive registry calls in a WOW64
 * process (the Firefox installer crashed with EBP = 0xFFFFFA80 after
 * SHRestricted -> RegQueryValueEx). Output is unbuffered, one line per step.
 *   i686-w64-mingw32-gcc -O1 -fomit-frame-pointer -static -o regebp32.exe regebp32.c -ladvapi32
 */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

static volatile unsigned g_r0[4], g_r1[4];
static int g_fail;

#define SAVE(a) __asm__ __volatile__("movl %%ebp,%0; movl %%ebx,%1; movl %%esi,%2; movl %%edi,%3" \
    : "=m"(a[0]), "=m"(a[1]), "=m"(a[2]), "=m"(a[3]))

static void report(const char *name, long r)
{
    int bad = memcmp((void *)g_r0, (void *)g_r1, sizeof(g_r0)) != 0;
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
    printf("RESULT: %d failures\n", g_fail);
    return g_fail;
}
