/*
 * PROJECT:     Kush 1.1
 * LICENSE:     LGPL-2.1-or-later
 * PURPOSE:     Win8 AppContainer APIs used by the Firefox sandbox launcher.
 *              Kush has no AppContainer support; callers fall back.
 */
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <winerror.h>

HRESULT WINAPI DeriveAppContainerSidFromAppContainerName(LPCWSTR pszAppContainerName, PVOID *ppsidAppContainerSid)
{
    UNREFERENCED_PARAMETER(pszAppContainerName);
    if (ppsidAppContainerSid) *ppsidAppContainerSid = NULL;
    return E_NOTIMPL;
}
