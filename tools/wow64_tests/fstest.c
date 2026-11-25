/* fstest: what an InstallShield setup does before extracting its MSI to
 * %TEMP%: free space, a large file (seek + SetEndOfFile + write at the end),
 * and mapping views of the (large) setup executable itself.
 * Usage: fstest32 [setup.exe] */
#include <windows.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    WCHAR temp[MAX_PATH], file[MAX_PATH];
    DWORD spc = 0, bps = 0, freec = 0, totc = 0;
    ULARGE_INTEGER availb, totb, freeb;
    HANDLE h;
    LARGE_INTEGER pos;
    DWORD w = 0;
    BOOL ok;

    GetTempPathW(MAX_PATH, temp);
    printf("GetTempPath: %ls\n", temp);
    ok = GetDiskFreeSpaceW(temp, &spc, &bps, &freec, &totc);
    printf("GetDiskFreeSpaceW: %d err %lu  spc %lu bps %lu free %lu total %lu  => %I64u MB free\n",
           ok, ok ? 0 : GetLastError(), spc, bps, freec, totc,
           (unsigned long long)spc * bps * freec >> 20);
    ok = GetDiskFreeSpaceExW(temp, &availb, &totb, &freeb);
    printf("GetDiskFreeSpaceExW: %d err %lu  avail %I64u MB total %I64u MB\n",
           ok, ok ? 0 : GetLastError(), availb.QuadPart >> 20, totb.QuadPart >> 20);

    swprintf(file, MAX_PATH, L"%lsfstest_big.tmp", temp);
    h = CreateFileW(file, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    printf("CreateFile(%ls): %p err %lu\n", file, h, h == INVALID_HANDLE_VALUE ? GetLastError() : 0);
    if (h != INVALID_HANDLE_VALUE)
    {
        pos.QuadPart = 700LL << 20;
        ok = SetFilePointerEx(h, pos, NULL, FILE_BEGIN);
        printf("SetFilePointerEx(700MB): %d err %lu\n", ok, ok ? 0 : GetLastError());
        ok = SetEndOfFile(h);
        printf("SetEndOfFile: %d err %lu\n", ok, ok ? 0 : GetLastError());
        ok = WriteFile(h, "KUSH", 4, &w, NULL);
        printf("WriteFile at 700MB: %d err %lu wrote %lu, size now %lu\n", ok, ok ? 0 : GetLastError(), w,
               GetFileSize(h, NULL));
        CloseHandle(h);
        DeleteFileW(file);
    }

    if (argc > 1)
    {
        HANDLE f = CreateFileA(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        HANDLE m;
        DWORD hi = 0, lo;
        printf("open %s: %p err %lu\n", argv[1], f, f == INVALID_HANDLE_VALUE ? GetLastError() : 0);
        if (f == INVALID_HANDLE_VALUE) return 0;
        lo = GetFileSize(f, &hi);
        printf("GetFileSize: %lu (hi %lu)\n", lo, hi);
        m = CreateFileMappingW(f, NULL, PAGE_READONLY, 0, 0, NULL);
        printf("CreateFileMapping: %p err %lu\n", m, m ? 0 : GetLastError());
        if (m)
        {
            PVOID v = MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
            printf("MapViewOfFile(whole %lu MB): %p err %lu\n", lo >> 20, v, v ? 0 : GetLastError());
            if (v) { printf("  first bytes %02x %02x\n", ((PBYTE)v)[0], ((PBYTE)v)[1]); UnmapViewOfFile(v); }
            v = MapViewOfFile(m, FILE_MAP_READ, 0, 600u << 20, 1 << 20);
            printf("MapViewOfFile(600MB, 1MB): %p err %lu\n", v, v ? 0 : GetLastError());
            if (v) UnmapViewOfFile(v);
            CloseHandle(m);
        }
        pos.QuadPart = (LONGLONG)lo - 16;
        ok = SetFilePointerEx(f, pos, NULL, FILE_BEGIN);
        {
            char b[16]; DWORD r = 0;
            BOOL rok = ReadFile(f, b, 16, &r, NULL);
            printf("seek to end-16 %d, ReadFile %d read %lu err %lu\n", ok, rok, r, rok ? 0 : GetLastError());
        }
        CloseHandle(f);
    }
    return 0;
}
