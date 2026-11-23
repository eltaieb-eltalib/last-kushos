/* QEMU-only control for the opt-in Kush icon presentation. */
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
        status = RegSetValueExW(key, L"ClassicIcons", 0, REG_DWORD,
            (BYTE*)&classic, sizeof(classic));
        RegCloseKey(key);
    }
    MessageBoxW(NULL, status == ERROR_SUCCESS ?
        (classic ? L"ClassicIcons=1. Open a NEW Explorer / Control Panel window." :
                   L"ClassicIcons=0. Open a NEW Explorer / Control Panel window.") :
        L"The registry change failed.", L"Kush icon rollback test", MB_OK);
    return status != ERROR_SUCCESS;
}
