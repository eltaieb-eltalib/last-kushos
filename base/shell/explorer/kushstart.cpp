/*
 * Kush Fluent Start (kushstart.cpp)
 *
 * Original Fluent-inspired Start window for Kush 1.1, drawn fully by this
 * file (no Microsoft assets). Phase 7 of docs/design/kush-fluent-plan.md.
 *
 * Design decisions (documented in README.md):
 * - Centered popup above the taskbar, 8px rounded corners, 1px gray border.
 * - Static Acrylic background: the screen area behind the window is captured
 *   once before showing, downscaled 8x, box-blurred, blended with a light
 *   tint and a 2% noise texture. No compositor needed.
 * - Pinned grid: top-level .lnk files from the user and common Programs
 *   folders (6 columns, 32px icons). "All apps" opens the classic menu.
 * - Recommended: 6 newest .lnk files from the Recent folder.
 * - Search box filters every program (.lnk) in both Programs folders.
 * - Esc or clicking outside closes; arrows + Enter work without a mouse.
 * - Power button asks the tray to show the shutdown dialog.
 */

#include "precomp.h"
#include "kushstart.h"

#define KUSH_START_CLASS L"KushFluentStart"
#define KUSH_START_MAX_ITEMS 300
#define KUSH_START_PINNED_MAX 30
#define KUSH_START_RECENT_MAX 6
#define KUSH_CORNER_RADIUS 8

/* Fluent-light palette (moves to design tokens in phase 0) */
static const COLORREF kcText = RGB(0x1B, 0x1B, 0x1B);
static const COLORREF kcTextDim = RGB(0x60, 0x60, 0x60);
static const COLORREF kcHover = RGB(0xFF, 0xFF, 0xFF);
static const COLORREF kcSelected = RGB(0xE8, 0xE8, 0xE8);
static const COLORREF kcBorder = RGB(0xBE, 0xBE, 0xBE);
static const COLORREF kcAccent = RGB(0x0D, 0x73, 0x77); /* Kush teal */
static const COLORREF kcBottomBar = RGB(0xE9, 0xE9, 0xE9);
static const COLORREF kcTint = RGB(0xF3, 0xF3, 0xF3);

typedef struct _KS_ITEM
{
    WCHAR szPath[MAX_PATH];
    WCHAR szArgs[MAX_PATH];
    WCHAR szDir[MAX_PATH];
    WCHAR szName[128];
    HICON hIcon;
    RECT rc;
} KS_ITEM;

typedef struct _KS_STATE
{
    HWND hwnd;
    HWND hwndTray;
    HWND hwndEdit;
    HFONT hFont;
    HFONT hFontBold;
    HFONT hFontSmall;
    HBITMAP hbmBack;      /* acrylic background, window-sized */
    int cx, cy;           /* window size */
    KS_ITEM pinned[KUSH_START_PINNED_MAX];
    int cPinned;
    KS_ITEM recent[KUSH_START_RECENT_MAX];
    int cRecent;
    WCHAR szRecentAge[KUSH_START_RECENT_MAX][24];
    KS_ITEM all[KUSH_START_MAX_ITEMS];
    int cAll;
    KS_ITEM view[KUSH_START_MAX_ITEMS]; /* currently shown grid items */
    int cView;
    BOOL bSearching;
    int iHot;             /* hovered view index, -1 none */
    int iSel;             /* keyboard selection, -1 none */
    int cRecentShown;     /* recent rows that fit (small screens show fewer) */
    BOOL bHotAllApps;
    BOOL bHotPower;
    RECT rcSearch;
    RECT rcAllApps;
    RECT rcPower;
    RECT rcUser;
    RECT rcGrid;
    RECT rcRecomm;
    RECT rcBottom;
    int cellW, cellH, nCols;
    WNDPROC pfnOldEdit;
    WCHAR szUser[64];
    DWORD dwOpenTick;     /* GetTickCount() when the window was shown */
} KS_STATE;

static KS_STATE g_ks;

static VOID KushDestroyData(KS_STATE *p);

/* ------------------------------------------------------------------ */

BOOL
KushStart_IsClassicRequested(VOID)
{
    DWORD dwValue = 0, cbSize = sizeof(dwValue);
    if (SHGetValueW(HKEY_CURRENT_USER, L"Software\\Kush\\Shell",
                    L"ClassicStartMenu", NULL, &dwValue, &cbSize) == ERROR_SUCCESS)
        return dwValue != 0;
    return FALSE;
}

BOOL
KushStart_IsOpen(VOID)
{
    return g_ks.hwnd != NULL && IsWindow(g_ks.hwnd);
}

BOOL
KushTaskbar_IsClassicRequested(VOID)
{
    DWORD dwValue = 0, cbSize = sizeof(dwValue);
    if (SHGetValueW(HKEY_CURRENT_USER, L"Software\\Kush\\Shell",
                    L"ClassicTaskbar", NULL, &dwValue, &cbSize) == ERROR_SUCCESS)
        return dwValue != 0;
    return FALSE;
}

/* Keep compact task-button painting out of classic and high-contrast modes. */
BOOL
KushTaskbar_UseCompactTasks(VOID)
{
    HIGHCONTRASTW hc = { sizeof(hc) };
    return !KushTaskbar_IsClassicRequested() &&
        !(SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0) &&
          (hc.dwFlags & HCF_HIGHCONTRASTON));
}

/* ---------------- background (static Acrylic) --------------------- */

static VOID
KushBoxBlur(BYTE *px, int w, int h, int radius)
{
    /* separable box blur, 3 passes ~= gaussian. px = 32bpp BGRA */
    BYTE *tmp = (BYTE *)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)w * h * 4);
    int x, y, c, i, pass;
    if (!tmp)
        return;
    for (pass = 0; pass < 3; pass++)
    {
        /* horizontal */
        for (y = 0; y < h; y++)
        {
            for (x = 0; x < w; x++)
            {
                int sum[4] = { 0, 0, 0, 0 }, n = 0;
                for (i = -radius; i <= radius; i++)
                {
                    int xx = x + i;
                    if (xx < 0) xx = 0;
                    if (xx >= w) xx = w - 1;
                    for (c = 0; c < 4; c++)
                        sum[c] += px[((SIZE_T)y * w + xx) * 4 + c];
                }
                n = 2 * radius + 1;
                for (c = 0; c < 4; c++)
                    tmp[((SIZE_T)y * w + x) * 4 + c] = (BYTE)(sum[c] / n);
            }
        }
        /* vertical */
        for (y = 0; y < h; y++)
        {
            for (x = 0; x < w; x++)
            {
                int sum[4] = { 0, 0, 0, 0 }, n = 0;
                for (i = -radius; i <= radius; i++)
                {
                    int yy = y + i;
                    if (yy < 0) yy = 0;
                    if (yy >= h) yy = h - 1;
                    for (c = 0; c < 4; c++)
                        sum[c] += tmp[((SIZE_T)yy * w + x) * 4 + c];
                }
                n = 2 * radius + 1;
                for (c = 0; c < 4; c++)
                    px[((SIZE_T)y * w + x) * 4 + c] = (BYTE)(sum[c] / n);
            }
        }
    }
    HeapFree(GetProcessHeap(), 0, tmp);
}

static VOID
KushBilinearUp(BYTE *dst, int dw, int dh, const BYTE *src, int sw, int sh)
{
    int x, y, c;
    for (y = 0; y < dh; y++)
    {
        int fy = (dh > 1) ? (y * (sh - 1) * 256) / (dh - 1) : 0;
        int y0 = fy >> 8, y1 = min(y0 + 1, sh - 1);
        int wy = fy & 255;
        for (x = 0; x < dw; x++)
        {
            int fx = (dw > 1) ? (x * (sw - 1) * 256) / (dw - 1) : 0;
            int x0 = fx >> 8, x1 = min(x0 + 1, sw - 1);
            int wx = fx & 255;
            const BYTE *p00 = src + ((SIZE_T)y0 * sw + x0) * 4;
            const BYTE *p01 = src + ((SIZE_T)y0 * sw + x1) * 4;
            const BYTE *p10 = src + ((SIZE_T)y1 * sw + x0) * 4;
            const BYTE *p11 = src + ((SIZE_T)y1 * sw + x1) * 4;
            BYTE *d = dst + ((SIZE_T)y * dw + x) * 4;
            for (c = 0; c < 4; c++)
            {
                int top = p00[c] * (256 - wx) + p01[c] * wx;
                int bot = p10[c] * (256 - wx) + p11[c] * wx;
                d[c] = (BYTE)((top * (256 - wy) + bot * wy) >> 16);
            }
        }
    }
}

static HBITMAP
KushCreateAcrylic(int x, int y, int w, int h)
{
    /* Capture what is behind the window BEFORE it is shown, downscale,
     * blur, blend with the light tint, add ~2% noise. */
    HDC hScreen = GetDC(NULL);
    HDC hdcBack = CreateCompatibleDC(hScreen);
    HBITMAP hbmBack = CreateCompatibleBitmap(hScreen, w, h);
    HBITMAP hOldBack = (HBITMAP)SelectObject(hdcBack, hbmBack);
    int sw = max(16, w / 8), sh = max(16, h / 8);
    HDC hdcSmall = CreateCompatibleDC(hScreen);
    HBITMAP hbmSmall = CreateCompatibleBitmap(hScreen, sw, sh);
    HBITMAP hOldSmall = (HBITMAP)SelectObject(hdcSmall, hbmSmall);
    BITMAPINFO bmi;
    BYTE *px;
    int i, n;

    BitBlt(hdcBack, 0, 0, w, h, hScreen, x, y, SRCCOPY);
    SetStretchBltMode(hdcSmall, HALFTONE); /* smooth reduction */
    SetBrushOrgEx(hdcSmall, 0, 0, NULL);
    StretchBlt(hdcSmall, 0, 0, sw, sh, hdcBack, 0, 0, w, h, SRCCOPY);

    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = sw;
    bmi.bmiHeader.biHeight = -sh; /* top-down */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    px = (BYTE *)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)sw * sh * 4);
    if (px)
    {
        GetDIBits(hdcSmall, hbmSmall, 0, sh, px, &bmi, DIB_RGB_COLORS);
        KushBoxBlur(px, sw, sh, 6);
        KushBoxBlur(px, sw, sh, 6); /* second pass kills residual banding */
        n = sw * sh * 4;
        for (i = 0; i < n; i += 4)
        {
            /* 78% light tint (noise is added later at full resolution) */
            px[i + 0] = (BYTE)((px[i + 0] * 22 + GetBValue(kcTint) * 78) / 100);
            px[i + 1] = (BYTE)((px[i + 1] * 22 + GetGValue(kcTint) * 78) / 100);
            px[i + 2] = (BYTE)((px[i + 2] * 22 + GetRValue(kcTint) * 78) / 100);
        }
        /* Bilinear upscale to full size; StretchBlt enlargement is
         * nearest-neighbour and produced visible block/diagonal banding. */
        {
            BITMAPINFO bmiBig;
            BYTE *big = (BYTE *)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)w * h * 4);
            if (big)
            {
                KushBilinearUp(big, w, h, px, sw, sh);
                for (i = 0; i < w * h * 4; i += 4)
                {
                    /* ~1.5% deterministic noise at full resolution */
                    UINT v = (UINT)i * 2654435761u;
                    int noise = (int)((v >> 13) & 3) - 1; /* -1..2 */
                    int b = big[i] + noise, g = big[i + 1] + noise, r = big[i + 2] + noise;
                    big[i] = (BYTE)min(255, max(0, b));
                    big[i + 1] = (BYTE)min(255, max(0, g));
                    big[i + 2] = (BYTE)min(255, max(0, r));
                }
                ZeroMemory(&bmiBig, sizeof(bmiBig));
                bmiBig.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmiBig.bmiHeader.biWidth = w;
                bmiBig.bmiHeader.biHeight = -h; /* top-down */
                bmiBig.bmiHeader.biPlanes = 1;
                bmiBig.bmiHeader.biBitCount = 32;
                bmiBig.bmiHeader.biCompression = BI_RGB;
                SetDIBits(hdcBack, hbmBack, 0, h, big, &bmiBig, DIB_RGB_COLORS);
                HeapFree(GetProcessHeap(), 0, big);
            }
            else
            {
                SetStretchBltMode(hdcBack, COLORONCOLOR);
                StretchBlt(hdcBack, 0, 0, w, h, hdcSmall, 0, 0, sw, sh, SRCCOPY);
            }
        }
        HeapFree(GetProcessHeap(), 0, px);
    }
    else
    {
        SetStretchBltMode(hdcBack, COLORONCOLOR);
        StretchBlt(hdcBack, 0, 0, w, h, hdcSmall, 0, 0, sw, sh, SRCCOPY);
    }

    SelectObject(hdcSmall, hOldSmall);
    DeleteObject(hbmSmall);
    DeleteDC(hdcSmall);
    SelectObject(hdcBack, hOldBack);
    DeleteDC(hdcBack);
    ReleaseDC(NULL, hScreen);
    return hbmBack;
}

/* ---------------- items ------------------------------------------- */

static VOID
KushAddItemNamed(KS_ITEM *items, int *count, int maxCount, LPCWSTR szPath, LPCWSTR szName)
{
    KS_ITEM *it;
    SHFILEINFOW shfi;
    if (*count >= maxCount)
        return;
    it = &items[*count];
    StringCchCopyW(it->szPath, _countof(it->szPath), szPath);
    it->szArgs[0] = 0;
    it->szDir[0] = 0;
    StringCchCopyW(it->szName, _countof(it->szName), szName);
    ZeroMemory(&shfi, sizeof(shfi));
    it->hIcon = NULL;
    if (SHGetFileInfoW(szPath, 0, &shfi, sizeof(shfi), SHGFI_ICON | SHGFI_LARGEICON))
        it->hIcon = shfi.hIcon;
    (*count)++;
}

static BOOL
KushIsLnk(LPCWSTR szName)
{
    SIZE_T len = wcslen(szName);
    return len > 4 && _wcsicmp(szName + len - 4, L".lnk") == 0;
}

/* Resolve a .lnk to its real target and arguments through IShellLink.
 * Returns FALSE when the link cannot be read or its target does not exist:
 * such entries are dropped instead of surfacing as dead tiles. */
/* Outcomes for a shortcut: RESOLVED (real file target: launch target+args+dir),
 * NAMESPACE (PIDL-only or unverifiable target: keep the .lnk, the shell runs
 * those), DEAD (no resolvable target and no PIDL: drop the entry). */
typedef enum _KS_LINK_KIND { KS_LINK_DEAD = 0, KS_LINK_RESOLVED, KS_LINK_NAMESPACE } KS_LINK_KIND;

static KS_LINK_KIND
KushResolveLink(LPCWSTR szLnk, LPWSTR szTarget, DWORD cchTarget,
                LPWSTR szArgs, DWORD cchArgs, LPWSTR szDir, DWORD cchDir)
{
    IShellLinkW *psl = NULL;
    IPersistFile *ppf = NULL;
    WCHAR szRaw[MAX_PATH], szExpanded[MAX_PATH];
    KS_LINK_KIND kind = KS_LINK_DEAD;
    HRESULT hr;

    szTarget[0] = 0;
    szArgs[0] = 0;
    szDir[0] = 0;
    hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                          IID_IShellLinkW, (LPVOID *)&psl);
    if (FAILED(hr) || !psl)
        return KS_LINK_DEAD;
    hr = psl->QueryInterface(IID_IPersistFile, (LPVOID *)&ppf);
    if (SUCCEEDED(hr) && ppf)
    {
        hr = ppf->Load(szLnk, STGM_READ);
        if (SUCCEEDED(hr))
        {
            hr = psl->GetPath(szRaw, _countof(szRaw), NULL, SLGP_RAWPATH);
            if (SUCCEEDED(hr) && szRaw[0] &&
                ExpandEnvironmentStringsW(szRaw, szExpanded, _countof(szExpanded)) &&
                GetFileAttributesW(szExpanded) != INVALID_FILE_ATTRIBUTES)
            {
                WCHAR szWork[MAX_PATH];
                StringCchCopyW(szTarget, cchTarget, szExpanded);
                psl->GetArguments(szArgs, cchArgs);
                if (SUCCEEDED(psl->GetWorkingDirectory(szWork, _countof(szWork))) &&
                    szWork[0] &&
                    ExpandEnvironmentStringsW(szWork, szExpanded, _countof(szExpanded)))
                    StringCchCopyW(szDir, cchDir, szExpanded);
                kind = KS_LINK_RESOLVED;
            }
            else
            {
                /* no usable file target: a PIDL means a shell namespace
                 * object (Control Panel items, folders, advertised links) -
                 * those stay, launched through the .lnk itself */
                PIDLIST_ABSOLUTE pidl = NULL;
                if (SUCCEEDED(psl->GetIDList(&pidl)) && pidl)
                {
                    CoTaskMemFree(pidl);
                    kind = KS_LINK_NAMESPACE;
                }
            }
        }
        ppf->Release();
    }
    psl->Release();
    return kind;
}

static VOID
KushAddItem(KS_ITEM *items, int *count, int maxCount, LPCWSTR szPath)
{
    KS_ITEM *it;
    LPCWSTR szFile;
    SHFILEINFOW shfi;
    WCHAR szTarget[MAX_PATH], szArgs[MAX_PATH];
    if (*count >= maxCount)
        return;
    it = &items[*count];
    it->szArgs[0] = 0;
    it->szDir[0] = 0;
    if (KushIsLnk(PathFindFileNameW(szPath)))
    {
        WCHAR szDir[MAX_PATH];
        KS_LINK_KIND kind = KushResolveLink(szPath, szTarget, _countof(szTarget),
                                            szArgs, _countof(szArgs),
                                            szDir, _countof(szDir));
        if (kind == KS_LINK_DEAD)
            return; /* dead shortcut: never show it */
        if (kind == KS_LINK_RESOLVED)
        {
            StringCchCopyW(it->szPath, _countof(it->szPath), szTarget);
            StringCchCopyW(it->szArgs, _countof(it->szArgs), szArgs);
            StringCchCopyW(it->szDir, _countof(it->szDir), szDir);
        }
        else
            StringCchCopyW(it->szPath, _countof(it->szPath), szPath); /* namespace: launch the link */
    }
    else
    {
        StringCchCopyW(it->szPath, _countof(it->szPath), szPath);
    }
    szFile = PathFindFileNameW(szPath);
    StringCchCopyW(it->szName, _countof(it->szName), szFile);
    PathRemoveExtensionW(it->szName);
    ZeroMemory(&shfi, sizeof(shfi));
    it->hIcon = NULL;
    /* the shortcut's own icon first (honors custom icon locations),
     * fall back to the resolved target's icon */
    if (!SHGetFileInfoW(szPath, 0, &shfi, sizeof(shfi), SHGFI_ICON | SHGFI_LARGEICON) ||
        !shfi.hIcon)
    {
        if (SHGetFileInfoW(it->szPath, 0, &shfi, sizeof(shfi), SHGFI_ICON | SHGFI_LARGEICON))
            it->hIcon = shfi.hIcon;
    }
    else
        it->hIcon = shfi.hIcon;
    (*count)++;
}

static VOID
KushEnumFolder(LPCWSTR szFolder, int depth, BOOL bTopOnly, KS_ITEM *items, int *count, int maxCount)
{
    WCHAR szMask[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE hFind;

    StringCchPrintfW(szMask, _countof(szMask), L"%s\\*", szFolder);
    hFind = FindFirstFileW(szMask, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return;
    do
    {
        WCHAR szFull[MAX_PATH];
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L".."))
            continue;
        StringCchPrintfW(szFull, _countof(szFull), L"%s\\%s", szFolder, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (!bTopOnly && depth < 3)
                KushEnumFolder(szFull, depth + 1, bTopOnly, items, count, maxCount);
        }
        else if (KushIsLnk(fd.cFileName))
        {
            if (bTopOnly && _wcsicmp(fd.cFileName, L"desktop.ini") == 0)
                continue;
            KushAddItem(items, count, maxCount, szFull);
        }
    } while (FindNextFileW(hFind, &fd) && *count < maxCount);
    FindClose(hFind);
}

static VOID
KushFormatAge(const FILETIME *ft, LPWSTR szOut, SIZE_T cchOut)
{
    FILETIME ftNow;
    ULARGE_INTEGER uNow, uThen;
    ULONGLONG mins;
    GetSystemTimeAsFileTime(&ftNow);
    uNow.LowPart = ftNow.dwLowDateTime; uNow.HighPart = ftNow.dwHighDateTime;
    uThen.LowPart = ft->dwLowDateTime; uThen.HighPart = ft->dwHighDateTime;
    mins = (uNow.QuadPart > uThen.QuadPart) ? (uNow.QuadPart - uThen.QuadPart) / 600000000ULL : 0;
    if (mins < 1)
        StringCchCopyW(szOut, cchOut, L"now");
    else if (mins < 60)
        StringCchPrintfW(szOut, cchOut, L"%um ago", (UINT)mins);
    else if (mins < 60 * 24)
        StringCchPrintfW(szOut, cchOut, L"%uh ago", (UINT)(mins / 60));
    else
        StringCchPrintfW(szOut, cchOut, L"%ud ago", (UINT)(mins / (60 * 24)));
}

static VOID
KushLoadRecent(KS_STATE *p)
{
    WCHAR szFolder[MAX_PATH], szMask[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE hFind;
    p->cRecent = 0;
    if (!SHGetSpecialFolderPathW(NULL, szFolder, CSIDL_RECENT, FALSE))
        return;
    StringCchPrintfW(szMask, _countof(szMask), L"%s\\*.lnk", szFolder);
    hFind = FindFirstFileW(szMask, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return;
    /* simple insertion of the 6 newest */
    do
    {
        /* append, then sort by write time desc below */
        if (KushIsLnk(fd.cFileName) && p->cRecent < KUSH_START_RECENT_MAX)
        {
            WCHAR szFull[MAX_PATH];
            StringCchPrintfW(szFull, _countof(szFull), L"%s\\%s", szFolder, fd.cFileName);
            KushAddItem(p->recent, &p->cRecent, KUSH_START_RECENT_MAX, szFull);
            KushFormatAge(&fd.ftLastWriteTime,
                          p->szRecentAge[p->cRecent - 1],
                          _countof(p->szRecentAge[p->cRecent - 1]));
            /* stash write time in the unused rc field for sorting below */
            p->recent[p->cRecent - 1].rc.left = (LONG)fd.ftLastWriteTime.dwLowDateTime;
            p->recent[p->cRecent - 1].rc.top = (LONG)fd.ftLastWriteTime.dwHighDateTime;
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    /* sort newest first (times stashed in rc.left/top) */
    {
        int a, b;
        for (a = 0; a < p->cRecent; a++)
            for (b = a + 1; b < p->cRecent; b++)
            {
                ULARGE_INTEGER ua, ub;
                ua.LowPart = (DWORD)p->recent[a].rc.left; ua.HighPart = (DWORD)p->recent[a].rc.top;
                ub.LowPart = (DWORD)p->recent[b].rc.left; ub.HighPart = (DWORD)p->recent[b].rc.top;
                if (ub.QuadPart > ua.QuadPart)
                {
                    KS_ITEM ti = p->recent[a]; p->recent[a] = p->recent[b]; p->recent[b] = ti;
                    WCHAR ta[24];
                    StringCchCopyW(ta, 24, p->szRecentAge[a]);
                    StringCchCopyW(p->szRecentAge[a], 24, p->szRecentAge[b]);
                    StringCchCopyW(p->szRecentAge[b], 24, ta);
                }
            }
        for (a = 0; a < p->cRecent; a++)
            ZeroMemory(&p->recent[a].rc, sizeof(RECT));
    }
}

static VOID
KushFreeItems(KS_ITEM *items, int *count)
{
    int i;
    for (i = 0; i < *count; i++)
    {
        if (items[i].hIcon)
            DestroyIcon(items[i].hIcon);
        items[i].hIcon = NULL;
    }
    *count = 0;
}

static VOID
KushLoadItems(KS_STATE *p)
{
    WCHAR szUser[MAX_PATH], szCommon[MAX_PATH];
    p->cPinned = p->cAll = p->cView = p->cRecent = 0;
    /* shortcut resolution below uses COM; already initialized in the shell,
     * so S_FALSE is the expected outcome */
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SHGetSpecialFolderPathW(NULL, szUser, CSIDL_PROGRAMS, FALSE))
    {
        KushEnumFolder(szUser, 0, TRUE, p->pinned, &p->cPinned, KUSH_START_PINNED_MAX);
        KushEnumFolder(szUser, 0, FALSE, p->all, &p->cAll, KUSH_START_MAX_ITEMS);
    }
    if (SHGetSpecialFolderPathW(NULL, szCommon, CSIDL_COMMON_PROGRAMS, FALSE))
    {
        KushEnumFolder(szCommon, 0, TRUE, p->pinned, &p->cPinned, KUSH_START_PINNED_MAX);
        KushEnumFolder(szCommon, 0, FALSE, p->all, &p->cAll, KUSH_START_MAX_ITEMS);
    }
    /* Live images and fresh profiles can leave the pinned grid nearly
     * empty. Fall back to built-in tools that actually exist on this
     * system; nothing is invented, every pin resolves to a real file. */
    if (p->cPinned < 4)
    {
        static const struct { LPCWSTR szFile; LPCWSTR szName; int idIcon; } builtins[] =
        {
            { L"cmd.exe",      L"Command Prompt", IDI_KUSH_CMD },
            { L"notepad.exe",  L"Notepad",        IDI_KUSH_NOTEPAD },
            { L"wordpad.exe",  L"WordPad",        IDI_KUSH_WORDPAD },
            { L"mspaint.exe",  L"Paint",          IDI_KUSH_PAINT },
            { L"calc.exe",     L"Calculator",     IDI_KUSH_CALC },
            { L"control.exe",  L"Control Panel",  IDI_KUSH_FILES },
        };
        WCHAR szDir[MAX_PATH];
        if (GetSystemDirectoryW(szDir, _countof(szDir)))
        {
            int bi;
            for (bi = 0; bi < (int)_countof(builtins) && p->cPinned < KUSH_START_PINNED_MAX; bi++)
            {
                WCHAR szPath[MAX_PATH];
                int k;
                BOOL dup = FALSE;
                StringCchPrintfW(szPath, _countof(szPath), L"%s\\%s", szDir, builtins[bi].szFile);
                if (GetFileAttributesW(szPath) == INVALID_FILE_ATTRIBUTES)
                    continue;
                for (k = 0; k < p->cPinned; k++)
                    if (_wcsicmp(p->pinned[k].szName, builtins[bi].szName) == 0) { dup = TRUE; break; }
                if (dup)
                    continue;
                {
                    int cBefore = p->cPinned;
                    HICON hFluent;
                    KushAddItemNamed(p->pinned, &p->cPinned, KUSH_START_PINNED_MAX,
                                     szPath, builtins[bi].szName);
                    /* Kush 1.1 Fluent: the cohesive MIT-licensed icon set
                     * replaces the legacy per-app pictures for built-ins. */
                    hFluent = (HICON)LoadImageW(hExplorerInstance,
                                                MAKEINTRESOURCEW(builtins[bi].idIcon),
                                                IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
                    if (hFluent && p->cPinned > cBefore)
                    {
                        if (p->pinned[p->cPinned - 1].hIcon)
                            DestroyIcon(p->pinned[p->cPinned - 1].hIcon);
                        p->pinned[p->cPinned - 1].hIcon = hFluent;
                    }
                }
                for (k = 0; k < p->cAll; k++)
                    if (_wcsicmp(p->all[k].szName, builtins[bi].szName) == 0) { dup = TRUE; break; }
                if (!dup)
                {
                    int cBefore = p->cAll;
                    HICON hFluent;
                    KushAddItemNamed(p->all, &p->cAll, KUSH_START_MAX_ITEMS,
                                     szPath, builtins[bi].szName);
                    /* own handle for the all[] copy: arrays free their own
                     * icons, sharing one would double-free */
                    hFluent = (HICON)LoadImageW(hExplorerInstance,
                                                MAKEINTRESOURCEW(builtins[bi].idIcon),
                                                IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
                    if (hFluent && p->cAll > cBefore)
                    {
                        if (p->all[p->cAll - 1].hIcon)
                            DestroyIcon(p->all[p->cAll - 1].hIcon);
                        p->all[p->cAll - 1].hIcon = hFluent;
                    }
                }
            }
        }
    }
    /* Cohesive set (Kush 1.1 Fluent): any enumerated shortcut whose target
     * is one of the well-known system tools gets the matching Fluent icon
     * too, so search results never fall back to legacy pictures. */
    {
        static const struct { LPCWSTR szExe; int idIcon; } known[] =
        {
            { L"\\explorer.exe", IDI_KUSH_EXPLORER },
            { L"\\cmd.exe",     IDI_KUSH_CMD },
            { L"\\notepad.exe", IDI_KUSH_NOTEPAD },
            { L"\\wordpad.exe", IDI_KUSH_WORDPAD },
            { L"\\mspaint.exe", IDI_KUSH_PAINT },
            { L"\\calc.exe",    IDI_KUSH_CALC },
            { L"\\control.exe", IDI_KUSH_FILES },
        };
        int i, j, a;
        for (a = 0; a < 2; a++)
        {
            KS_ITEM *items = a ? p->all : p->pinned;
            int count = a ? p->cAll : p->cPinned;
            for (i = 0; i < count; i++)
            {
                for (j = 0; j < (int)_countof(known); j++)
                {
                    SIZE_T len = wcslen(items[i].szPath), exl = wcslen(known[j].szExe);
                    if (len > exl && _wcsicmp(items[i].szPath + len - exl, known[j].szExe) == 0)
                    {
                        HICON hFluent = (HICON)LoadImageW(hExplorerInstance,
                                                          MAKEINTRESOURCEW(known[j].idIcon),
                                                          IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
                        if (hFluent)
                        {
                            if (items[i].hIcon)
                                DestroyIcon(items[i].hIcon);
                            items[i].hIcon = hFluent;
                        }
                        break;
                    }
                }
            }
        }
    }
    KushLoadRecent(p);
    /* default view = pinned */
    memcpy(p->view, p->pinned, sizeof(KS_ITEM) * p->cPinned);
    p->cView = p->cPinned;
    p->bSearching = FALSE;
}

static VOID
KushApplyFilter(KS_STATE *p, LPCWSTR szText)
{
    int i;
    if (!szText || !*szText)
    {
        memcpy(p->view, p->pinned, sizeof(KS_ITEM) * p->cPinned);
        p->cView = p->cPinned;
        p->bSearching = FALSE;
        return;
    }
    p->cView = 0;
    for (i = 0; i < p->cAll && p->cView < KUSH_START_MAX_ITEMS; i++)
    {
        if (StrStrIW(p->all[i].szName, szText))
        {
            /* share the icon handle; do not double-free: mark view icons NULL
             * when copying from all[] */
            p->view[p->cView] = p->all[i];
            p->view[p->cView].hIcon = p->all[i].hIcon;
            p->cView++;
        }
    }
    p->bSearching = TRUE;
}

/* ---------------- launching --------------------------------------- */

static VOID
KushLaunch(KS_STATE *p, const KS_ITEM *it)
{
    SHELLEXECUTEINFOW sei;
    HWND hwndTray = p->hwndTray;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_ASYNCOK;
    sei.lpVerb = L"open";
    sei.lpFile = it->szPath;
    if (it->szArgs[0])
        sei.lpParameters = it->szArgs;
    if (it->szDir[0])
        sei.lpDirectory = it->szDir;
    sei.nShow = SW_SHOWNORMAL;
    KushStart_Hide();
    if (!ShellExecuteExW(&sei))
    {
        /* SEE_MASK_FLAG_NO_UI suppresses the shell's own error UI; surface
         * the failure ourselves so a dead tile is never silently ignored. */
        DWORD dwErr = GetLastError();
        WCHAR szMsg[2 * MAX_PATH];
        StringCchPrintfW(szMsg, _countof(szMsg),
                         L"Kush Start: launch failed (error %lu):\n%s",
                         dwErr, it->szPath);
        MessageBoxW(NULL, szMsg, L"Kush Start", MB_OK | MB_ICONERROR);
    }
    if (hwndTray)
        PostMessageW(hwndTray, WM_NULL, 0, 0);
}

static VOID
KushOpenAllApps(KS_STATE *p)
{
    HWND hwndTray = p->hwndTray;
    KushStart_Hide();
    if (hwndTray)
        PostMessageW(hwndTray, TWM_OPENCLASSICSTARTMENU, 0, 0);
}

static VOID
KushPower(KS_STATE *p)
{
    HWND hwndTray = p->hwndTray;
    KushStart_Hide();
    if (hwndTray)
        PostMessageW(hwndTray, WM_COMMAND, TRAYCMD_SHUTDOWN_DIALOG, 0);
}

/* ---------------- layout ------------------------------------------ */

static VOID
KushLayout(KS_STATE *p)
{
    int w = p->cx, h = p->cy;
    int margin = 12;
    int searchH = 34;
    int headH = 26;
    int bottomH = 56;
    int gridTop;
    int recomH;

    SetRect(&p->rcSearch, margin + 8, margin + 6, w - margin - 8, margin + 6 + searchH);
    gridTop = p->rcSearch.bottom + 10;
    /* "Pinned" header + All apps link */
    SetRect(&p->rcAllApps, w - margin - 110, gridTop, w - margin - 8, gridTop + headH);
    gridTop += headH + 4;

    p->nCols = 6;
    p->cellW = (w - 2 * margin - 16) / p->nCols;
    p->cellH = 76;

    /* keep at least one grid row; use the rest for recent items */
    {
        int availH = (h - bottomH) - gridTop - 8;
        int maxRows = (availH - p->cellH - headH - 8) / 32;
        int rows = p->bSearching ? 0 : min(p->cRecent, min(3, max(0, maxRows)));
        p->cRecentShown = rows;
        recomH = (p->bSearching || rows == 0) ? 0 : (headH + 4 + rows * 32 + 8);
    }
    SetRect(&p->rcBottom, 0, h - bottomH, w, h);
    {
        int gridBottom = p->rcBottom.top - recomH - 8;
        if (!p->bSearching && p->cView > 0)
        {
            /* Pinned view is known-small: fit the grid to its real rows so
             * no dead band sits between the pins and Recommended. Search
             * keeps the full stretch so many results stay visible. */
            int rowsFit = (p->cView + p->nCols - 1) / p->nCols;
            int fitted = gridTop + rowsFit * p->cellH;
            if (fitted < gridBottom)
                gridBottom = fitted;
        }
        SetRect(&p->rcGrid, margin + 8, gridTop, w - margin - 8, gridBottom);
        if (!p->bSearching)
            SetRect(&p->rcRecomm, margin + 8, gridBottom + 4, w - margin - 8, gridBottom + 4 + recomH);
        else
            SetRectEmpty(&p->rcRecomm);
    }
    SetRect(&p->rcPower, w - margin - 48, p->rcBottom.top + 10, w - margin - 8, p->rcBottom.top + 46);
    SetRect(&p->rcUser, margin + 8, p->rcBottom.top + 8, margin + 260, p->rcBottom.top + 48);

    if (p->hwndEdit)
        MoveWindow(p->hwndEdit, p->rcSearch.left + 22, p->rcSearch.top + 6,
                   (p->rcSearch.right - p->rcSearch.left) - 36,
                   (p->rcSearch.bottom - p->rcSearch.top) - 12, TRUE);
}

/* ---------------- painting ---------------------------------------- */

static VOID
KushRoundRect(HDC hdc, const RECT *prc, int radius, COLORREF fill, COLORREF pen)
{
    HBRUSH hBrush = CreateSolidBrush(fill);
    HPEN hPen = CreatePen(PS_SOLID, 1, pen);
    HBRUSH hOldB = (HBRUSH)SelectObject(hdc, hBrush);
    HPEN hOldP = (HPEN)SelectObject(hdc, hPen);
    RoundRect(hdc, prc->left, prc->top, prc->right, prc->bottom, radius * 2, radius * 2);
    SelectObject(hdc, hOldP);
    SelectObject(hdc, hOldB);
    DeleteObject(hPen);
    DeleteObject(hBrush);
}

static VOID
KushPaintItem(KS_STATE *p, HDC hdc, KS_ITEM *it, BOOL bHot, BOOL bSel, BOOL bGrid)
{
    RECT rc = it->rc;
    if (bSel)
        KushRoundRect(hdc, &rc, 4, kcSelected, kcAccent);
    else if (bHot)
        KushRoundRect(hdc, &rc, 4, kcHover, kcBorder);
    if (bGrid)
    {
        int ix = rc.left + (rc.right - rc.left - 32) / 2;
        int iy = rc.top + 8;
        RECT rcText;
        if (it->hIcon)
            DrawIconEx(hdc, ix, iy, it->hIcon, 32, 32, 0, NULL, DI_NORMAL);
        SetRect(&rcText, rc.left + 2, iy + 36, rc.right - 2, rc.bottom - 2);
        SelectObject(hdc, p->hFont);
        SetTextColor(hdc, kcText);
        DrawTextW(hdc, it->szName, -1, &rcText,
                  DT_CENTER | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    else
    {
        int iy = rc.top + (rc.bottom - rc.top - 24) / 2;
        RECT rcText, rcTime;
        if (it->hIcon)
            DrawIconEx(hdc, rc.left + 6, iy, it->hIcon, 24, 24, 0, NULL, DI_NORMAL);
        SetRect(&rcText, rc.left + 38, rc.top, rc.right - 90, rc.bottom);
        SelectObject(hdc, p->hFont);
        SetTextColor(hdc, kcText);
        DrawTextW(hdc, it->szName, -1, &rcText,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        SetRect(&rcTime, rc.right - 88, rc.top, rc.right - 6, rc.bottom);
        SelectObject(hdc, p->hFontSmall);
        SetTextColor(hdc, kcTextDim);
        /* find this item's age string */
        {
            int idx = (int)(it - p->recent);
            LPCWSTR szAge = (idx >= 0 && idx < p->cRecent) ? p->szRecentAge[idx] : L"";
            DrawTextW(hdc, szAge, -1, &rcTime,
                      DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
    }
}

static VOID
KushPaint(KS_STATE *p, HDC hdc)
{
    RECT rcClient;
    HDC hdcMem;
    HBITMAP hbm, hOld;
    int i;
    WCHAR szHeader[64];

    GetClientRect(p->hwnd, &rcClient);
    hdcMem = CreateCompatibleDC(hdc);
    hbm = CreateCompatibleBitmap(hdc, p->cx, p->cy);
    hOld = (HBITMAP)SelectObject(hdcMem, hbm);

    /* acrylic background */
    if (p->hbmBack)
    {
        HDC hdcBack = CreateCompatibleDC(hdcMem);
        HBITMAP hOldB = (HBITMAP)SelectObject(hdcBack, p->hbmBack);
        BitBlt(hdcMem, 0, 0, p->cx, p->cy, hdcBack, 0, 0, SRCCOPY);
        SelectObject(hdcBack, hOldB);
        DeleteDC(hdcBack);
    }
    else
    {
        HBRUSH hB = CreateSolidBrush(kcTint);
        FillRect(hdcMem, &rcClient, hB);
        DeleteObject(hB);
    }

    SetBkMode(hdcMem, TRANSPARENT);

    /* search box */
    KushRoundRect(hdcMem, &p->rcSearch, 4, RGB(0xFF, 0xFF, 0xFF), kcBorder);
    /* search magnifier: drawn as a GDI vector glyph (lens + handle) instead of
     * an ICO - ReactOS DrawIconEx mangles 32bpp-alpha icons. Fluent gray. */
    {
        int cy0 = p->rcSearch.top + (p->rcSearch.bottom - p->rcSearch.top) / 2;
        int ix = p->rcSearch.left + 4, iy = cy0 - 8;
        HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0x60, 0x60, 0x60));
        HPEN hOldPen = (HPEN)SelectObject(hdcMem, hPen);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcMem, GetStockObject(NULL_BRUSH));
        Ellipse(hdcMem, ix + 1, iy + 1, ix + 11, iy + 11);
        MoveToEx(hdcMem, ix + 10, iy + 10, NULL);
        LineTo(hdcMem, ix + 14, iy + 14);
        SelectObject(hdcMem, hOldBrush);
        SelectObject(hdcMem, hOldPen);
        DeleteObject(hPen);
    }

    /* section header */
    SelectObject(hdcMem, p->hFontBold);
    SetTextColor(hdcMem, kcText);
    StringCchCopyW(szHeader, _countof(szHeader), p->bSearching ? L"Search results" : L"Pinned");
    {
        RECT rcH = { p->rcGrid.left, p->rcAllApps.top, p->rcAllApps.left - 8, p->rcAllApps.bottom };
        DrawTextW(hdcMem, szHeader, -1, &rcH, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    /* All apps > */
    {
        RECT rcA = p->rcAllApps;
        if (p->bHotAllApps)
            KushRoundRect(hdcMem, &rcA, 4, kcHover, kcBorder);
        SelectObject(hdcMem, p->hFont);
        SetTextColor(hdcMem, kcText);
        DrawTextW(hdcMem, L"All apps >", -1, &rcA, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    /* grid */
    for (i = 0; i < p->cView; i++)
        KushPaintItem(p, hdcMem, &p->view[i], i == p->iHot, i == p->iSel, TRUE);

    /* recommended */
    if (!p->bSearching && p->cRecentShown > 0)
    {
        RECT rcH = { p->rcRecomm.left, p->rcRecomm.top, p->rcRecomm.right, p->rcRecomm.top + 26 };
        SelectObject(hdcMem, p->hFontBold);
        SetTextColor(hdcMem, kcText);
        DrawTextW(hdcMem, L"Recommended", -1, &rcH, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        for (i = 0; i < p->cRecentShown; i++)
            KushPaintItem(p, hdcMem, &p->recent[i], FALSE, FALSE, FALSE);
    }

    /* bottom bar */
    {
        RECT rcB = p->rcBottom;
        HBRUSH hB = CreateSolidBrush(kcBottomBar);
        FillRect(hdcMem, &rcB, hB);
        DeleteObject(hB);
    }
    /* user circle with initial */
    {
        int cx0 = p->rcUser.left + 18, cy0 = (p->rcUser.top + p->rcUser.bottom) / 2;
        HBRUSH hB = CreateSolidBrush(kcAccent);
        HBRUSH hOldB = (HBRUSH)SelectObject(hdcMem, hB);
        HPEN hP = CreatePen(PS_SOLID, 1, kcAccent);
        HPEN hOldP = (HPEN)SelectObject(hdcMem, hP);
        RECT rcN;
        WCHAR szInitial[2] = { p->szUser[0] ? p->szUser[0] : L'K', 0 };
        Ellipse(hdcMem, cx0 - 14, cy0 - 14, cx0 + 14, cy0 + 14);
        SelectObject(hdcMem, hOldP);
        DeleteObject(hP);
        SelectObject(hdcMem, p->hFontBold);
        SetTextColor(hdcMem, RGB(0xFF, 0xFF, 0xFF));
        SetRect(&rcN, cx0 - 14, cy0 - 10, cx0 + 14, cy0 + 10);
        DrawTextW(hdcMem, szInitial, 1, &rcN, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdcMem, hOldB);
        DeleteObject(hB);
        SetRect(&rcN, cx0 + 22, p->rcUser.top, p->rcUser.right, p->rcUser.bottom);
        SelectObject(hdcMem, p->hFont);
        SetTextColor(hdcMem, kcText);
        DrawTextW(hdcMem, p->szUser, -1, &rcN, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    /* power button: Fluent UI System Icon (MIT), hover ring kept */
    {
        RECT rcP = p->rcPower;
        int cx0 = (rcP.left + rcP.right) / 2, cy0 = (rcP.top + rcP.bottom) / 2;
        HICON hIco;
        if (p->bHotPower)
            KushRoundRect(hdcMem, &rcP, 4, kcHover, kcBorder);
        hIco = (HICON)LoadImageW(hExplorerInstance, MAKEINTRESOURCEW(IDI_KUSH_POWER),
                                 IMAGE_ICON, 20, 20, LR_DEFAULTCOLOR);
        if (hIco)
        {
            DrawIconEx(hdcMem, cx0 - 10, cy0 - 10, hIco, 20, 20, 0, NULL, DI_NORMAL);
            DestroyIcon(hIco);
        }
    }

    /* 1px outer border */
    {
        HPEN hPen = CreatePen(PS_SOLID, 1, kcBorder);
        HPEN hOldP = (HPEN)SelectObject(hdcMem, hPen);
        HBRUSH hOldB = (HBRUSH)SelectObject(hdcMem, GetStockObject(NULL_BRUSH));
        RoundRect(hdcMem, 0, 0, p->cx, p->cy, KUSH_CORNER_RADIUS * 2, KUSH_CORNER_RADIUS * 2);
        SelectObject(hdcMem, hOldB);
        SelectObject(hdcMem, hOldP);
        DeleteObject(hPen);
    }

    BitBlt(hdc, 0, 0, p->cx, p->cy, hdcMem, 0, 0, SRCCOPY);
    SelectObject(hdcMem, hOld);
    DeleteObject(hbm);
    DeleteDC(hdcMem);
}

/* ---------------- input ------------------------------------------- */

static VOID
KushGridRects(KS_STATE *p)
{
    int i;
    for (i = 0; i < p->cView; i++)
    {
        int row = i / p->nCols, col = i % p->nCols;
        int x = p->rcGrid.left + col * p->cellW;
        int y = p->rcGrid.top + row * p->cellH;
        if (y + p->cellH > p->rcGrid.bottom)
        {
            /* no room: collapse out of view */
            SetRectEmpty(&p->view[i].rc);
            continue;
        }
        SetRect(&p->view[i].rc, x, y, x + p->cellW, y + p->cellH);
    }
    if (!p->bSearching)
    {
        for (i = 0; i < p->cRecentShown; i++)
        {
            int y = p->rcRecomm.top + 26 + i * 32;
            SetRect(&p->recent[i].rc, p->rcRecomm.left, y, p->rcRecomm.right, y + 30);
        }
    }
}

static int
KushHitGrid(KS_STATE *p, int x, int y)
{
    int i;
    POINT pt = { x, y };
    for (i = 0; i < p->cView; i++)
        if (!IsRectEmpty(&p->view[i].rc) && PtInRect(&p->view[i].rc, pt))
            return i;
    return -1;
}

static int
KushHitRecent(KS_STATE *p, int x, int y)
{
    int i;
    POINT pt = { x, y };
    if (p->bSearching)
        return -1;
    for (i = 0; i < p->cRecentShown; i++)
        if (!IsRectEmpty(&p->recent[i].rc) && PtInRect(&p->recent[i].rc, pt))
            return i;
    return -1;
}

static VOID
KushMoveSel(KS_STATE *p, int dx, int dy)
{
    int i = p->iSel;
    if (p->cView == 0)
        return;
    if (i < 0)
        i = 0;
    else
    {
        i += dx + dy * p->nCols;
        if (i < 0) i = 0;
        if (i >= p->cView) i = p->cView - 1;
    }
    p->iSel = i;
    InvalidateRect(p->hwnd, NULL, FALSE);
}

static LRESULT CALLBACK
KushEditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_KEYDOWN)
    {
        if (wParam == VK_ESCAPE)
        {
            KushStart_Hide();
            return 0;
        }
        if (wParam == VK_RETURN)
        {
            /* Windows-11 behaviour: Enter in the search box launches the
             * top result immediately. */
            if (g_ks.cView > 0)
                KushLaunch(&g_ks, &g_ks.view[0]);
            return 0;
        }
        if (wParam == VK_DOWN)
        {
            g_ks.iSel = 0;
            SetFocus(g_ks.hwnd);
            InvalidateRect(g_ks.hwnd, NULL, FALSE);
            return 0;
        }
    }
    if (uMsg == WM_CHAR && wParam == VK_ESCAPE)
        return 0;
    if (uMsg == WM_GETDLGCODE)
        return DLGC_WANTALLKEYS;
    return CallWindowProcW(g_ks.pfnOldEdit, hwnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK
KushStartProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    KS_STATE *p = &g_ks;
    switch (uMsg)
    {
    case WM_CREATE:
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        KushPaint(p, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            /* Clicking the Start button (or anywhere on the tray) must close
             * the menu via the tray's toggle path, not race a reopen: close
             * through a posted message so the tray's click handler runs our
             * toggle first. Any other focus loss closes immediately.
             * Guard: activation shuffle left over from the window's own
             * opening (e.g. after a modal dialog owned the foreground) must
             * not kill a menu that just opened. */
            if (GetTickCount() - p->dwOpenTick < 500)
            {
                DbgPrint("KUSH-DBG WA_INACTIVE within open guard, ignored (lParam=%p)\n", (PVOID)lParam);
                return 0;
            }
            if ((HWND)lParam == p->hwndTray && lParam != NULL)
                PostMessageW(hwnd, WM_APP + 41, 0, 0);
            else
                KushStart_Hide();
            return 0;
        }
        break;

    case WM_APP + 41: /* deferred close after tray activation */
        if (GetTickCount() - p->dwOpenTick < 500)
        {
            DbgPrint("KUSH-DBG deferred close within open guard, ignored\n");
            return 0;
        }
        KushStart_Hide();
        return 0;

    case WM_COMMAND:
        if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == p->hwndEdit)
        {
            WCHAR szText[128];
            GetWindowTextW(p->hwndEdit, szText, _countof(szText));
            KushApplyFilter(p, szText);
            p->iSel = -1;
            p->iHot = -1;
            KushLayout(p);
            KushGridRects(p);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        break;

    case WM_MOUSEMOVE:
    {
        int x = (short)LOWORD(lParam), y = (short)HIWORD(lParam);
        int iHot = KushHitGrid(p, x, y);
        int iRec = KushHitRecent(p, x, y);
        POINT pt = { x, y };
        BOOL bAll = PtInRect(&p->rcAllApps, pt);
        BOOL bPow = PtInRect(&p->rcPower, pt);
        if (iHot != p->iHot || iRec >= 0 || bAll != p->bHotAllApps || bPow != p->bHotPower)
        {
            p->iHot = iHot;
            p->bHotAllApps = bAll;
            p->bHotPower = bPow;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }

    case KUSH_WM_FOCUSSEARCH:
        if (p->hwndEdit)
            SetFocus(p->hwndEdit);
        return 0;

    case WM_MOUSELEAVE:
        p->iHot = -1;
        p->bHotAllApps = FALSE;
        p->bHotPower = FALSE;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_LBUTTONUP:
    {
        int x = (short)LOWORD(lParam), y = (short)HIWORD(lParam);
        int i = KushHitGrid(p, x, y);
        int iRec = KushHitRecent(p, x, y);
        POINT pt = { x, y };
        if (i >= 0)
            KushLaunch(p, &p->view[i]);
        else if (iRec >= 0)
            KushLaunch(p, &p->recent[iRec]);
        else if (PtInRect(&p->rcAllApps, pt))
            KushOpenAllApps(p);
        else if (PtInRect(&p->rcPower, pt))
            KushPower(p);
        return 0;
    }

    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_ESCAPE:
            KushStart_Hide();
            return 0;
        case VK_LEFT:  KushMoveSel(p, -1, 0); return 0;
        case VK_RIGHT: KushMoveSel(p, +1, 0); return 0;
        case VK_UP:    KushMoveSel(p, 0, -1); return 0;
        case VK_DOWN:  KushMoveSel(p, 0, +1); return 0;
        case VK_RETURN:
            if (p->iSel >= 0 && p->iSel < p->cView)
                KushLaunch(p, &p->view[p->iSel]);
            return 0;
        }
        break;

    case WM_CHAR:
        /* typing while the grid has focus goes to the search box */
        if (p->hwndEdit && wParam >= 32)
        {
            SetFocus(p->hwndEdit);
            PostMessageW(p->hwndEdit, WM_CHAR, wParam, lParam);
        }
        return 0;

    case WM_DESTROY:
        if (p->hbmBack)
        {
            DeleteObject(p->hbmBack);
            p->hbmBack = NULL;
        }
        /* free the item icons and fonts created in KushStart_Toggle;
         * view[] entries share icons with pinned[]/all[] and are not
         * owners, so they are not freed separately. */
        KushDestroyData(p);
        if (p->hFont) { DeleteObject(p->hFont); p->hFont = NULL; }
        if (p->hFontBold) { DeleteObject(p->hFontBold); p->hFontBold = NULL; }
        if (p->hFontSmall) { DeleteObject(p->hFontSmall); p->hFontSmall = NULL; }
        if (p->hwnd)
        {
            HWND hwndTray = p->hwndTray;
            p->hwnd = NULL;
            if (hwndTray)
                PostMessageW(hwndTray, TWM_KUSHSTARTCLOSED, 0, 0);
        }
        return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

/* ---------------- public ------------------------------------------ */

static VOID
KushDestroyData(KS_STATE *p)
{
    KushFreeItems(p->pinned, &p->cPinned);
    KushFreeItems(p->all, &p->cAll);
    KushFreeItems(p->recent, &p->cRecent);
    p->cView = 0;
}

VOID
KushStart_Hide(VOID)
{
    if (g_ks.hwnd && IsWindow(g_ks.hwnd))
        DestroyWindow(g_ks.hwnd);
    g_ks.hwnd = NULL;
}

BOOL
KushStart_Toggle(HWND hTray)
{
    RECT wa, rcTray;
    int x, y, w, h;
    DWORD cbName;

    if (KushStart_IsOpen())
    {
        KushStart_Hide();
        return TRUE;
    }

    ZeroMemory(&g_ks, sizeof(g_ks));
    g_ks.hwndTray = hTray;
    g_ks.iHot = -1;
    g_ks.iSel = -1;

    cbName = _countof(g_ks.szUser);
    if (!GetUserNameW(g_ks.szUser, &cbName))
        StringCchCopyW(g_ks.szUser, _countof(g_ks.szUser), L"Kush");

    /* Kush 1.1 Fluent: Inter is the Latin UI face (SIL OFL 1.1). GDI falls
     * back to the default GUI font when Inter is not installed. */
    g_ks.hFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Inter");
    g_ks.hFontBold = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Inter SemiBold");
    g_ks.hFontSmall = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Inter");

    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    w = min(640, (wa.right - wa.left) - 16);

    /* Load items first so the window height can fit the real content
     * instead of always consuming nearly the whole work area. */
    KushLoadItems(&g_ks);
    {
        int rows = (g_ks.cPinned + 5) / 6;
        /* Must mirror KushLayout exactly: search(12+6+34) + 10 + header(26+4)
         * + rows*76 + 4 + recent(26+4+n*32+8) + 8 + bottom bar 56. */
        int contentH = 12 + 6 + 34 + 10 + 26 + 4 + rows * 76 + 4 + 8 + 56;
        if (g_ks.cRecent > 0)
            contentH += 26 + 4 + min(g_ks.cRecent, 3) * 32 + 8;
        h = min(max(contentH, 240), (wa.bottom - wa.top) - 16);
    }
    x = wa.left + ((wa.right - wa.left) - w) / 2;
    y = wa.bottom - h - 8;
    if (GetWindowRect(hTray, &rcTray) && rcTray.top > wa.top)
        y = rcTray.top - h - 8; /* sit just above a bottom taskbar */
    if (y < wa.top)
        y = wa.top + 4;

    g_ks.cx = w;
    g_ks.cy = h;
    g_ks.hbmBack = KushCreateAcrylic(x, y, w, h);

    {
        WNDCLASSEXW wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = KushStartProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.lpszClassName = KUSH_START_CLASS;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassExW(&wc); /* fine if already registered */
    }

    /* No WS_EX_NOACTIVATE: the window must take activation so that Esc,
     * arrow keys and the outside-click close (WM_ACTIVATE) all work. */
    g_ks.hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                                KUSH_START_CLASS, L"Kush Start",
                                WS_POPUP,
                                x, y, w, h,
                                hTray, NULL, GetModuleHandleW(NULL), NULL);
    if (!g_ks.hwnd)
    {
        KushDestroyData(&g_ks);
        if (g_ks.hbmBack) { DeleteObject(g_ks.hbmBack); g_ks.hbmBack = NULL; }
        return FALSE;
    }

    /* rounded corners (8px) */
    {
        HRGN hrgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, KUSH_CORNER_RADIUS * 2, KUSH_CORNER_RADIUS * 2);
        SetWindowRgn(g_ks.hwnd, hrgn, TRUE); /* region now owned by the system */
    }

    /* search box */
    g_ks.hwndEdit = CreateWindowExW(0, L"EDIT", L"",
                                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                    0, 0, 10, 10,
                                    g_ks.hwnd, NULL, GetModuleHandleW(NULL), NULL);
    if (g_ks.hwndEdit)
    {
        SendMessageW(g_ks.hwndEdit, WM_SETFONT, (WPARAM)g_ks.hFont, TRUE);
        g_ks.pfnOldEdit = (WNDPROC)SetWindowLongPtrW(g_ks.hwndEdit, GWLP_WNDPROC, (LONG_PTR)KushEditProc);
    }

    KushLayout(&g_ks);
    KushGridRects(&g_ks);

    ShowWindow(g_ks.hwnd, SW_SHOW);
    SetForegroundWindow(g_ks.hwnd);
    g_ks.dwOpenTick = GetTickCount();
    DbgPrint("KUSH-DBG Toggle: opened hwnd=%p tick=%lu\n", g_ks.hwnd, g_ks.dwOpenTick);
    if (g_ks.hwndEdit)
        SetFocus(g_ks.hwndEdit);
    /* Foreground activation can complete asynchronously; re-assert focus on
     * the search box once the queue settles so typing always lands there. */
    PostMessageW(g_ks.hwnd, KUSH_WM_FOCUSSEARCH, 0, 0);
    return TRUE;
}
