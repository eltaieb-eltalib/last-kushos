/*
 * PROJECT:     Kush 1.1
 * LICENSE:     LGPL-2.1-or-later
 * PURPOSE:     Win7 panning-feedback APIs (visual overscroll bounce), no-ops on Kush.
 */
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>

BOOL WINAPI BeginPanningFeedback(HWND hwnd)
{ UNREFERENCED_PARAMETER(hwnd); return TRUE; }

BOOL WINAPI UpdatePanningFeedback(HWND hwnd, LONG lTotalOverpanOffsetX, LONG lTotalOverpanOffsetY, BOOL fInInertia)
{ UNREFERENCED_PARAMETER(hwnd); UNREFERENCED_PARAMETER(lTotalOverpanOffsetX);
  UNREFERENCED_PARAMETER(lTotalOverpanOffsetY); UNREFERENCED_PARAMETER(fInInertia); return TRUE; }

BOOL WINAPI EndPanningFeedback(HWND hwnd, BOOL fAnimateBack)
{ UNREFERENCED_PARAMETER(hwnd); UNREFERENCED_PARAMETER(fAnimateBack); return TRUE; }
