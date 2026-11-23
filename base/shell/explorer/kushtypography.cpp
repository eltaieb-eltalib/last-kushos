/* Kush Fluent typography. UI settings only, no font-engine changes. */
#include "precomp.h"

static BOOL KushReadDword(HKEY key, LPCWSTR name, DWORD *value)
{
    DWORD type = 0, bytes = sizeof(*value);
    return RegQueryValueExW(key, name, NULL, &type, (BYTE*)value, &bytes) == ERROR_SUCCESS &&
        type == REG_DWORD && bytes == sizeof(*value);
}

static BOOL KushReadBinary(HKEY key, LPCWSTR name, VOID *data, DWORD bytes)
{
    DWORD type = 0, size = bytes;
    return RegQueryValueExW(key, name, NULL, &type, (BYTE*)data, &size) == ERROR_SUCCESS &&
        type == REG_BINARY && size == bytes;
}

static VOID KushSetFace(LOGFONTW *font, LONG height, LONG weight)
{
    font->lfHeight = height;
    font->lfWidth = 0;
    font->lfWeight = weight;
    font->lfCharSet = DEFAULT_CHARSET;
    font->lfQuality = ANTIALIASED_QUALITY;
    StringCchCopyW(font->lfFaceName, LF_FACESIZE, L"Inter");
}

VOID KushApplyTypography()
{
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Kush\\Shell", 0, NULL, 0,
                       KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS)
        return;

    DWORD classic = 0, applied = 0, refresh = 0;
    KushReadDword(key, L"ClassicFonts", &classic);
    KushReadDword(key, L"TypographyApplied", &applied);
    KushReadDword(key, L"TypographyNeedsRefresh", &refresh);
    NONCLIENTMETRICSW metrics = {sizeof(metrics)};
    LOGFONTW icon = {0};
    if (classic)
    {
        if ((applied || refresh) &&
            KushReadBinary(key, L"OriginalNonClientMetrics", &metrics, sizeof(metrics)) &&
            KushReadBinary(key, L"OriginalIconFont", &icon, sizeof(icon)))
        {
            NONCLIENTMETRICSW current = {sizeof(current)};
            if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(current), &current, 0))
            { RegCloseKey(key); return; }
            /* Restore fonts without undoing the current window-theme sizes. */
            current.lfCaptionFont = metrics.lfCaptionFont;
            current.lfSmCaptionFont = metrics.lfSmCaptionFont;
            current.lfMenuFont = metrics.lfMenuFont;
            current.lfStatusFont = metrics.lfStatusFont;
            current.lfMessageFont = metrics.lfMessageFont;
            BOOL ok = SystemParametersInfoW(SPI_SETNONCLIENTMETRICS, sizeof(current), &current,
                                            SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
            ok = SystemParametersInfoW(SPI_SETICONTITLELOGFONT, sizeof(icon), &icon,
                                        SPIF_UPDATEINIFILE | SPIF_SENDCHANGE) && ok;
            if (ok)
            {
                applied = 0;
                RegSetValueExW(key, L"TypographyApplied", 0, REG_DWORD, (BYTE*)&applied, sizeof(applied));
                RegSetValueExW(key, L"TypographyNeedsRefresh", 0, REG_DWORD, (BYTE*)&applied, sizeof(applied));
            }
        }
        RegCloseKey(key);
        return;
    }
    /* Apply once, so later changes in Display settings are not overwritten. */
    if ((applied && !refresh) ||
        !SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0) ||
        !SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(icon), &icon, 0))
    {
        RegCloseKey(key);
        return;
    }
    /* Do not install a substitute face when the shipped font is unavailable. */
    WCHAR fontPath[MAX_PATH];
    GetWindowsDirectoryW(fontPath, _countof(fontPath));
    StringCchCatW(fontPath, _countof(fontPath), L"\\Fonts\\Inter-Regular.ttf");
    if (GetFileAttributesW(fontPath) == INVALID_FILE_ATTRIBUTES)
    {
        RegCloseKey(key);
        return;
    }
    /* Save before changing either setting. Keep the first original across opt-outs. */
    NONCLIENTMETRICSW savedMetrics;
    LOGFONTW savedIcon;
    BOOL saved = KushReadBinary(key, L"OriginalNonClientMetrics", &savedMetrics, sizeof(savedMetrics)) &&
                 KushReadBinary(key, L"OriginalIconFont", &savedIcon, sizeof(savedIcon));
    if (!saved &&
        (RegSetValueExW(key, L"OriginalNonClientMetrics", 0, REG_BINARY, (BYTE*)&metrics, sizeof(metrics)) != ERROR_SUCCESS ||
         RegSetValueExW(key, L"OriginalIconFont", 0, REG_BINARY, (BYTE*)&icon, sizeof(icon)) != ERROR_SUCCESS))
    {
        RegCloseKey(key);
        return;
    }
    HDC screen = GetDC(NULL);
    int dpi = screen ? GetDeviceCaps(screen, LOGPIXELSY) : 96;
    if (screen) ReleaseDC(NULL, screen);
    LONG height = -MulDiv(9, dpi, 72);
    KushSetFace(&metrics.lfCaptionFont, height, FW_SEMIBOLD);
    KushSetFace(&metrics.lfSmCaptionFont, height, FW_NORMAL);
    KushSetFace(&metrics.lfMenuFont, height, FW_NORMAL);
    KushSetFace(&metrics.lfStatusFont, height, FW_NORMAL);
    KushSetFace(&metrics.lfMessageFont, height, FW_NORMAL);
    KushSetFace(&icon, height, FW_NORMAL);
    BOOL ok = SystemParametersInfoW(SPI_SETNONCLIENTMETRICS, sizeof(metrics), &metrics,
                                    SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    ok = SystemParametersInfoW(SPI_SETICONTITLELOGFONT, sizeof(icon), &icon,
                                SPIF_UPDATEINIFILE | SPIF_SENDCHANGE) && ok;
    /* Mark even partial application for reliable restoration at the next opt-out. */
    applied = 1;
    RegSetValueExW(key, L"TypographyApplied", 0, REG_DWORD, (BYTE*)&applied, sizeof(applied));
    refresh = 0;
    RegSetValueExW(key, L"TypographyNeedsRefresh", 0, REG_DWORD, (BYTE*)&refresh, sizeof(refresh));
    DbgPrint("KUSH-DBG typography: Inter 9pt applied=%d dpi=%d\n", ok, dpi);
    RegCloseKey(key);
}

/* Set the same UI settings that Display Properties normally applies with a theme. */
static VOID KushWindowPalette(HKEY key, BOOL classic)
{
    static const INT indices[] = {COLOR_BTNFACE, COLOR_BTNSHADOW, COLOR_3DDKSHADOW,
        COLOR_3DLIGHT, COLOR_BTNHIGHLIGHT, COLOR_ACTIVECAPTION, COLOR_INACTIVECAPTION,
        COLOR_GRADIENTACTIVECAPTION, COLOR_GRADIENTINACTIVECAPTION, COLOR_CAPTIONTEXT,
        COLOR_INACTIVECAPTIONTEXT, COLOR_HIGHLIGHT, COLOR_HIGHLIGHTTEXT, COLOR_MENU,
        COLOR_MENUBAR, COLOR_MENUHILIGHT};
    static LPCWSTR names[] = {L"ButtonFace", L"ButtonShadow", L"ButtonDkShadow",
        L"ButtonLight", L"ButtonHilight", L"ActiveTitle", L"InactiveTitle",
        L"GradientActiveTitle", L"GradientInactiveTitle", L"TitleText",
        L"InactiveTitleText", L"Hilight", L"HilightText", L"Menu",
        L"MenuBar", L"MenuHilight"};
    COLORREF colors[] = {RGB(247,248,250), RGB(202,208,216), RGB(180,187,197),
        RGB(247,248,250), RGB(255,255,255), RGB(243,246,250), RGB(246,247,249),
        RGB(243,246,250), RGB(246,247,249), RGB(30,35,41), RGB(117,123,132),
        RGB(13,115,119), RGB(255,255,255), RGB(255,255,255), RGB(247,248,250),
        RGB(221,240,239)};
    COLORREF saved[_countof(colors)];
    BOOL haveColors = KushReadBinary(key, L"OriginalWindowColors", saved, sizeof(saved));
    if (classic)
    {
        if (!haveColors) return;
        CopyMemory(colors, saved, sizeof(colors));
    }
    else if (!haveColors)
    {
        for (UINT i = 0; i < _countof(indices); ++i) saved[i] = GetSysColor(indices[i]);
        if (RegSetValueExW(key, L"OriginalWindowColors", 0, REG_BINARY,
            (BYTE*)saved, sizeof(saved)) != ERROR_SUCCESS) return;
    }
    SetSysColors(_countof(indices), indices, colors);
    HKEY colorsKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Colors", 0, NULL, 0,
        KEY_SET_VALUE, NULL, &colorsKey, NULL) == ERROR_SUCCESS)
    {
        for (UINT i = 0; i < _countof(colors); ++i)
        {
            WCHAR value[32];
            StringCchPrintfW(value, _countof(value), L"%u %u %u",
                GetRValue(colors[i]), GetGValue(colors[i]), GetBValue(colors[i]));
            RegSetValueExW(colorsKey, names[i], 0, REG_SZ, (BYTE*)value,
                (lstrlenW(value) + 1) * sizeof(WCHAR));
        }
        RegCloseKey(colorsKey);
    }
    NONCLIENTMETRICSW metrics = {sizeof(metrics)};
    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) return;
    INT original[2];
    BOOL haveMetrics = KushReadBinary(key, L"OriginalCaptionSize", original, sizeof(original));
    if (!classic && !haveMetrics)
    {
        original[0] = metrics.iCaptionWidth; original[1] = metrics.iCaptionHeight;
        if (RegSetValueExW(key, L"OriginalCaptionSize", 0, REG_BINARY,
            (BYTE*)original, sizeof(original)) != ERROR_SUCCESS) return;
    }
    if (classic && !haveMetrics) return;
    HDC screen = GetDC(NULL);
    int dpi = screen ? GetDeviceCaps(screen, LOGPIXELSY) : 96;
    if (screen) ReleaseDC(NULL, screen);
    metrics.iCaptionWidth = classic ? original[0] : MulDiv(28, dpi, 96);
    metrics.iCaptionHeight = classic ? original[1] : MulDiv(28, dpi, 96);
    SystemParametersInfoW(SPI_SETNONCLIENTMETRICS, sizeof(metrics), &metrics,
        SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
}

/* Use the existing theme service and APIs, never replace nonclient behavior. */
VOID KushApplyWindowTheme()
{
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Kush\\Shell", 0, NULL, 0,
        KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS) return;
    DWORD classic = 0, applied = 0;
    KushReadDword(key, L"ClassicWindows", &classic);
    KushReadDword(key, L"WindowThemeApplied", &applied);
    if (!classic && applied) { RegCloseKey(key); return; }
    HMODULE lib = LoadLibraryW(L"uxtheme.dll");
    typedef HRESULT (WINAPI *OPEN)(LPCWSTR,LPCWSTR,LPCWSTR,HANDLE*,DWORD);
    typedef HRESULT (WINAPI *APPLY)(HANDLE,UINT,HWND);
    typedef HRESULT (WINAPI *CLOSE)(HANDLE);
    OPEN open = lib ? (OPEN)GetProcAddress(lib, MAKEINTRESOURCEA(2)) : NULL;
    APPLY apply = lib ? (APPLY)GetProcAddress(lib, MAKEINTRESOURCEA(4)) : NULL;
    CLOSE close = lib ? (CLOSE)GetProcAddress(lib, MAKEINTRESOURCEA(3)) : NULL;
    HRESULT hr = E_FAIL;
    /* Initialize the existing per-process callback before BOTH enable and
     * disable. A first GUI call during ApplyTheme(NULL) can otherwise reload
     * the old style in the middle of that API's metrics restoration. */
    if (apply)
    {
        HWND bootstrap = CreateWindowExW(0, L"STATIC", L"", WS_POPUP,
            0, 0, 0, 0, NULL, NULL, GetModuleHandleW(NULL), NULL);
        if (bootstrap)
        {
            DefWindowProcW(bootstrap, WM_NULL, 0, 0);
            DestroyWindow(bootstrap);
        }
    }
    if (apply && classic) hr = apply(NULL, 0x21, NULL);
    else if (open && apply && close)
    {
        WCHAR path[MAX_PATH]; GetWindowsDirectoryW(path, _countof(path));
        StringCchCatW(path, _countof(path), L"\\Resources\\Themes\\Modern\\kushfluent.msstyles");
        HANDLE theme = NULL;
        hr = open(path, L"Light", L"Regular Fonts", &theme, 0);
        if (SUCCEEDED(hr)) { hr = apply(theme, 0x21, NULL); close(theme); }
    }
    if (SUCCEEDED(hr))
    {
        KushWindowPalette(key, classic);
        /* Native theme changes can restore system fonts too. Let the following
         * typography pass reapply the user's font choice without stale state. */
        DWORD refreshFonts = 1;
        RegSetValueExW(key, L"TypographyNeedsRefresh", 0, REG_DWORD,
            (BYTE*)&refreshFonts, sizeof(refreshFonts));
        applied = !classic;
        RegSetValueExW(key, L"WindowThemeApplied", 0, REG_DWORD, (BYTE*)&applied, sizeof(applied));
    }
    DbgPrint("KUSH-DBG window theme: classic=%u hr=%08lx\n", classic, hr);
    if (lib) FreeLibrary(lib);
    RegCloseKey(key);
}
