/* bigio: large single I/O calls and large allocations in a 32-bit process
 * (what a setup does when it extracts a big file), plus the free address
 * space below 2 GB. Usage: bigio32 dir\ setup.exe */
#include <windows.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    static const SIZE_T sizes[] = { 16u << 20, 64u << 20, 128u << 20, 256u << 20, 512u << 20, 768u << 20, 1024u << 20 };
    MEMORY_BASIC_INFORMATION mbi;
    PBYTE p = NULL;
    SIZE_T biggest = 0, totalfree = 0;
    char path[MAX_PATH];
    unsigned i;

    /* free address space */
    for (p = (PBYTE)0x10000; p < (PBYTE)0x7FFF0000 && VirtualQuery(p, &mbi, sizeof(mbi)); p = (PBYTE)mbi.BaseAddress + mbi.RegionSize)
    {
        if (mbi.State == MEM_FREE)
        {
            totalfree += mbi.RegionSize;
            if (mbi.RegionSize > biggest) biggest = mbi.RegionSize;
        }
        else if (mbi.RegionSize >= (16u << 20))
            printf("  used %p + %Iu MB state %lx type %lx\n", mbi.BaseAddress, mbi.RegionSize >> 20, mbi.State, mbi.Type);
    }
    printf("free below 2GB: %Iu MB, biggest free block %Iu MB\n", totalfree >> 20, biggest >> 20);

    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
        PVOID v = VirtualAlloc(NULL, sizes[i], MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        PVOID h = HeapAlloc(GetProcessHeap(), 0, sizes[i]);
        printf("alloc %4Iu MB: VirtualAlloc %p (err %lu)  HeapAlloc %p\n", sizes[i] >> 20, v, v ? 0 : GetLastError(), h);
        if (v) VirtualFree(v, 0, MEM_RELEASE);
        if (h) HeapFree(GetProcessHeap(), 0, h);
    }

    if (argc > 1)
    {
        HANDLE f;
        sprintf(path, "%sbigio.tmp", argv[1]);
        f = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        for (i = 0; f != INVALID_HANDLE_VALUE && i < 5; i++)
        {
            PVOID buf = VirtualAlloc(NULL, sizes[i], MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            DWORD n = 0;
            BOOL ok = buf && WriteFile(f, buf, (DWORD)sizes[i], &n, NULL);
            printf("WriteFile %4Iu MB in one call: %d wrote %lu err %lu\n", sizes[i] >> 20, ok, n, ok ? 0 : GetLastError());
            if (buf) VirtualFree(buf, 0, MEM_RELEASE);
        }
        if (f != INVALID_HANDLE_VALUE) { CloseHandle(f); DeleteFileA(path); }
    }
    if (argc > 2)
    {
        HANDLE f = CreateFileA(argv[2], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        for (i = 0; f != INVALID_HANDLE_VALUE && i < 5; i++)
        {
            PVOID buf = VirtualAlloc(NULL, sizes[i], MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            DWORD n = 0;
            BOOL ok;
            SetFilePointer(f, 0, NULL, FILE_BEGIN);
            ok = buf && ReadFile(f, buf, (DWORD)sizes[i], &n, NULL);
            printf("ReadFile  %4Iu MB in one call: %d read %lu err %lu\n", sizes[i] >> 20, ok, n, ok ? 0 : GetLastError());
            if (buf) VirtualFree(buf, 0, MEM_RELEASE);
        }
        if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    }
    return 0;
}
