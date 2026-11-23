/* QEMU-only switch. Open a NEW Explorer window after changing this value. */
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
        status = RegSetValueExW(key, L"ClassicExplorer", 0, REG_DWORD,
            (BYTE*)&classic, sizeof(classic));
        RegCloseKey(key);
    }
    MessageBoxW(NULL, status == ERROR_SUCCESS ?
        (classic ? L"ClassicExplorer=1. Open a NEW Explorer window." :
                   L"ClassicExplorer=0. Open a NEW Explorer window.") :
        L"Registry change failed.", L"Kush Explorer layout rollback test", MB_OK);
    return status != ERROR_SUCCESS;
}
