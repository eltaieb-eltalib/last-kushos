/*
 * PROJECT:     Kush 1.1
 * LICENSE:     LGPL-2.1-or-later
 * PURPOSE:     Win7 SHGetPropertyStoreForWindow (taskbar AppUserModelID per window).
 */
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <winerror.h>

HRESULT WINAPI SHGetPropertyStoreForWindow(HWND hwnd, const GUID *riid, void **ppv)
{
    UNREFERENCED_PARAMETER(hwnd); UNREFERENCED_PARAMETER(riid);
    if (ppv) *ppv = NULL;
    return E_NOTIMPL;
}
