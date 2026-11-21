/*
 * PROJECT:     Kush 1.1
 * LICENSE:     LGPL-2.1-or-later
 * PURPOSE:     Win8+ pointer/rotation APIs delay-loaded by Firefox 160 (xul.dll).
 *              Kush has no touch digitizer or rotation sensor support.
 */
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <winerror.h>

#define KUSH_AR_NOSENSOR 0x10

BOOL WINAPI GetAutoRotationState(PULONG State)
{
    if (!State) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    *State = KUSH_AR_NOSENSOR;
    return TRUE;
}

BOOL WINAPI GetPointerFrameTouchInfo(UINT32 PointerId, UINT32 *Count, PVOID TouchInfo)
{
    UNREFERENCED_PARAMETER(PointerId); UNREFERENCED_PARAMETER(TouchInfo);
    if (Count) *Count = 0;
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
}

BOOL WINAPI InjectSyntheticPointerInput(HANDLE Device, PVOID PointerInfo, UINT32 Count)
{
    UNREFERENCED_PARAMETER(Device); UNREFERENCED_PARAMETER(PointerInfo); UNREFERENCED_PARAMETER(Count);
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}
