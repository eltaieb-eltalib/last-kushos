/* QEMU-only control for the Kush tab font measurement fallback. */
#define UNICODE
#define _UNICODE
#include <windows.h>
int WINAPI WinMain(HINSTANCE h, HINSTANCE p, LPSTR args, int show)
{
    HKEY key; DWORD classic = lstrcmpiA(args, "classic") == 0;
    LONG status = RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Kush\\Shell", 0,
        NULL, 0, KEY_SET_VALUE, NULL, &key, NULL);
    if (status == ERROR_SUCCESS)
    {
        status = RegSetValueExW(key, L"ClassicTabs", 0, REG_DWORD,
            (BYTE*)&classic, sizeof(classic));
        RegCloseKey(key);
    }
    MessageBoxW(NULL, status == ERROR_SUCCESS ?
        (classic ? L"ClassicTabs=1. Open a NEW Mouse / System window." :
                   L"ClassicTabs=0. Open a NEW Mouse / System window.") :
        L"The registry change failed.", L"Kush tab measurement rollback test", MB_OK);
    return status != ERROR_SUCCESS;
}
