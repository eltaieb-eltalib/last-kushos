/*
 * Kush 1.1: what Firefox does to a profile directory (lock file, write
 * checks); "Profile Missing" means one of these failed.
 *   x86_64-w64-mingw32-gcc -O1 -o proflock64.exe proflock64.c
 *   proflock64.exe C:\ffprofile
 */
#include <windows.h>
#include <stdio.h>

int wmain(int argc, wchar_t **argv)
{
    const wchar_t *dir = argc > 1 ? argv[1] : L"C:\\ffprofile";
    wchar_t p[MAX_PATH];
    HANDLE h; DWORD a; OVERLAPPED ov = {0};

    a = GetFileAttributesW(dir);
    printf("GetFileAttributes(dir) = 0x%lx (err %lu)\n", a, GetLastError());

    swprintf(p, MAX_PATH, L"%ls\\parent.lock", dir);
    h = CreateFileW(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    printf("CreateFile(parent.lock, DELETE_ON_CLOSE) = %p (err %lu)\n", h, GetLastError());
    if (h != INVALID_HANDLE_VALUE)
    {
        BOOL r = LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &ov);
        printf("LockFileEx = %d (err %lu)\n", r, GetLastError());
        CloseHandle(h);
        printf("after close, lock exists = %d\n", GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES);
    }
    swprintf(p, MAX_PATH, L"%ls\\test.tmp", dir);
    h = CreateFileW(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    printf("CreateFile(test.tmp) = %p (err %lu)\n", h, GetLastError());
    if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); printf("DeleteFile = %d (err %lu)\n", DeleteFileW(p), GetLastError()); }
    swprintf(p, MAX_PATH, L"%ls\\sub", dir);
    printf("CreateDirectory(sub) = %d (err %lu)\n", CreateDirectoryW(p, NULL), GetLastError());
    printf("RemoveDirectory(sub) = %d (err %lu)\n", RemoveDirectoryW(p), GetLastError());
    return 0;
}
