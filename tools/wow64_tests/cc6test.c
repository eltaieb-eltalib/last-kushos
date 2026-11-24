/*
 * Kush 1.1: create Common Controls 6 windows from a program whose manifest asks
 * for comctl32 v6 (activation context class redirection). The 7-Zip SFX stub of
 * the Firefox installer deadlocked because creating its progress dialog failed.
 *   i686-w64-mingw32-windres cc6test.rc -O coff -o cc6test.res
 *   i686-w64-mingw32-gcc -O1 -o cc6test.exe cc6test.c cc6test.res -lcomctl32 -luser32
 */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

int main(void)
{
    static const char *classes[] = { "msctls_progress32", "Button", "Static", "Edit", "SysListView32",
                                     "msctls_trackbar32", "SysAnimate32", "msctls_statusbar32", "ComboBox" };
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES | ICC_PROGRESS_CLASS };
    HWND parent;
    unsigned i;
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("InitCommonControlsEx = %d\n", InitCommonControlsEx(&icc));
    parent = CreateWindowExA(0, "Static", "parent", WS_OVERLAPPEDWINDOW, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    printf("parent = %p (err %lu)\n", (void *)parent, GetLastError());
    for (i = 0; i < sizeof(classes) / sizeof(classes[0]); i++)
    {
        HWND h;
        SetLastError(0);
        h = CreateWindowExA(0, classes[i], "x", WS_CHILD | WS_VISIBLE, 0, 0, 100, 20, parent, (HMENU)(ULONG_PTR)(100 + i), NULL, NULL);
        printf("%-22s -> %p (err %lu)\n", classes[i], (void *)h, GetLastError());
    }
    return 0;
}
