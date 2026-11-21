/*
 * Kush 1.1: CM_Register_Notification / CM_Unregister_Notification (Windows 8+).
 * Kush has no PnP notification broker for user mode; the call fails cleanly
 * with CR_CALL_NOT_IMPLEMENTED and callers (Firefox) fall back to polling.
 */
#include <windef.h>
#include <winbase.h>

#define CR_SUCCESS              0x00000000
#define CR_INVALID_POINTER      0x00000003
#define CR_CALL_NOT_IMPLEMENTED 0x00000034

DWORD WINAPI CM_Register_Notification(PVOID pFilter, PVOID pContext, PVOID pCallback, PVOID *pNotifyContext)
{
    UNREFERENCED_PARAMETER(pFilter); UNREFERENCED_PARAMETER(pContext); UNREFERENCED_PARAMETER(pCallback);
    if (!pNotifyContext) return CR_INVALID_POINTER;
    *pNotifyContext = NULL;
    return CR_CALL_NOT_IMPLEMENTED;
}

DWORD WINAPI CM_Unregister_Notification(PVOID NotifyContext)
{
    UNREFERENCED_PARAMETER(NotifyContext);
    return CR_SUCCESS;
}
