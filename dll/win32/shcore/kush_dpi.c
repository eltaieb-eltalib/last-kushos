/*
 * Kush 1.1: DPI functions used by Firefox. Kush has a single DPI (96) and no
 * per-monitor DPI awareness, so report 96 DPI and accept awareness requests.
 */
#include <windef.h>
#include <winbase.h>

HRESULT WINAPI KushGetDpiForMonitor(HMONITOR monitor, int type, UINT *x, UINT *y)
{
    UNREFERENCED_PARAMETER(monitor); UNREFERENCED_PARAMETER(type);
    if (!x || !y) return E_INVALIDARG;
    *x = 96; *y = 96;
    return S_OK;
}

HRESULT WINAPI KushSetProcessDpiAwareness(int value)
{
    UNREFERENCED_PARAMETER(value);
    return S_OK;
}

HRESULT WINAPI KushGetProcessDpiAwareness(HANDLE process, int *value)
{
    UNREFERENCED_PARAMETER(process);
    if (!value) return E_INVALIDARG;
    *value = 0; /* PROCESS_DPI_UNAWARE */
    return S_OK;
}

HRESULT WINAPI KushGetScaleFactorForMonitor(HMONITOR monitor, int *scale)
{
    UNREFERENCED_PARAMETER(monitor);
    if (!scale) return E_INVALIDARG;
    *scale = 100; /* SCALE_100_PERCENT */
    return S_OK;
}
