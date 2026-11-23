/* Native Explorer presentation only. Read once when creating a new band. */
#pragma once
#include <windef.h>
#include <winbase.h>
#include <winreg.h>
#include <winuser.h>
#include <wingdi.h>

static __inline BOOL KushModernExplorer(void)
{
    HIGHCONTRASTW highContrast = { sizeof(highContrast) };
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(highContrast), &highContrast, 0) &&
        (highContrast.dwFlags & HCF_HIGHCONTRASTON))
        return FALSE;
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Kush\\Shell", 0, KEY_QUERY_VALUE, &key))
        return TRUE;
    const WCHAR *names[] = { L"ClassicExplorer", L"ClassicWindows" };
    BOOL modern = TRUE;
    for (UINT i = 0; i < ARRAYSIZE(names); ++i)
    {
        DWORD value = 0, type = 0, size = sizeof(value);
        if (RegQueryValueExW(key, names[i], NULL, &type, (BYTE*)&value, &size) == ERROR_SUCCESS &&
            type == REG_DWORD && size == sizeof(value) && value)
            modern = FALSE;
    }
    RegCloseKey(key);
    return modern;
}

static __inline int KushExplorerPixels(HWND window, int pixels)
{
    HDC dc = GetDC(window);
    int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
    if (dc) ReleaseDC(window, dc);
    return MulDiv(pixels, dpi > 0 ? dpi : 96, 96);
}
