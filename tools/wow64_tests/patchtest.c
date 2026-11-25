/* patchtest: what Chromium's sandbox does to a suspended child's ntdll
 * (VirtualProtectEx PAGE_WRITECOPY, WriteProcessMemory, restore), checking
 * after every step that the parent's own copy of the page is untouched:
 * same protection, same bytes. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static void show(const char *step, PBYTE addr, const BYTE *orig, HANDLE child)
{
    MEMORY_BASIC_INFORMATION mbi, cmbi;
    BYTE now[16], cnow[16];
    SIZE_T got = 0;
    VirtualQuery(addr, &mbi, sizeof(mbi));
    VirtualQueryEx(child, addr, &cmbi, sizeof(cmbi));
    memcpy(now, addr, 16);
    ReadProcessMemory(child, addr, cnow, 16, &got);
    printf("%-26s parent prot %03lx bytes %s | child prot %03lx bytes %s\n", step,
           mbi.Protect, memcmp(now, orig, 16) ? "CHANGED" : "same",
           cmbi.Protect, memcmp(cnow, orig, 16) ? "changed" : "same");
}

int main(void)
{
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    PBYTE fn = (PBYTE)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtOpenKeyEx");
    BYTE orig[16], patch[12] = {0x48, 0xB8, 1,2,3,4,5,6,7,8, 0xFF, 0xE0}; /* mov rax,imm64; jmp rax */
    DWORD old = 0, old2 = 0;
    SIZE_T wrote = 0;
    char cmd[] = "C:\\gltest64.exe";

    if (!fn) { printf("no NtOpenKeyEx\n"); return 1; }
    memcpy(orig, fn, 16);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi))
    { printf("CreateProcess failed %lu\n", GetLastError()); return 1; }

    show("start", fn, orig, pi.hProcess);
    printf("VirtualProtectEx(WRITECOPY) %d err %lu old %lx\n",
           VirtualProtectEx(pi.hProcess, fn, sizeof(patch), PAGE_WRITECOPY, &old), GetLastError(), old);
    show("after protect", fn, orig, pi.hProcess);
    printf("WriteProcessMemory %d err %lu wrote %Iu\n",
           WriteProcessMemory(pi.hProcess, fn, patch, sizeof(patch), &wrote), GetLastError(), wrote);
    show("after write", fn, orig, pi.hProcess);
    printf("VirtualProtectEx(restore %lx) %d err %lu\n", old,
           VirtualProtectEx(pi.hProcess, fn, sizeof(patch), old, &old2), GetLastError());
    show("after restore", fn, orig, pi.hProcess);

    /* the parent must still be able to run code in that page */
    {
        typedef ULONG (WINAPI *PFN)(void);
        PFN p = (PFN)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtYieldExecution");
        printf("calling NtYieldExecution in the same page: %lx\n", p());
    }
    TerminateProcess(pi.hProcess, 0);
    printf("done\n");
    return 0;
}
