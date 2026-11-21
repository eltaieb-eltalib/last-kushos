/*
 * PROJECT:     Kush 1.1
 * LICENSE:     LGPL-2.1-or-later
 * PURPOSE:     Vista+ IP Helper notification APIs needed by Firefox 160
 *              (netwerk/system/win32/nsNotifyAddrListener delay-loads them).
 *
 * Kush has no network-change notification source yet, so registrations succeed
 * and the caller simply never receives a change. If an initial notification was
 * requested it is delivered once from a worker thread, as on Windows.
 */
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <winerror.h>

typedef VOID (WINAPI *KUSH_IPIF_CALLBACK)(PVOID CallerContext, PVOID Row, ULONG NotificationType);

typedef struct _KUSH_NOTIFY
{
    KUSH_IPIF_CALLBACK Callback;
    PVOID Context;
} KUSH_NOTIFY, *PKUSH_NOTIFY;

#define KUSH_MIB_INITIAL_NOTIFICATION 3 /* MibInitialNotification */

static DWORD WINAPI KushInitialNotifyThread(LPVOID Param)
{
    PKUSH_NOTIFY Notify = Param;
    Notify->Callback(Notify->Context, NULL, KUSH_MIB_INITIAL_NOTIFICATION);
    return 0;
}

static DWORD KushRegister(PVOID Callback, PVOID Context, BOOLEAN Initial, HANDLE *Handle)
{
    PKUSH_NOTIFY Notify;

    if (!Callback || !Handle) return ERROR_INVALID_PARAMETER;
    Notify = HeapAlloc(GetProcessHeap(), 0, sizeof(*Notify));
    if (!Notify) return ERROR_NOT_ENOUGH_MEMORY;
    Notify->Callback = (KUSH_IPIF_CALLBACK)Callback;
    Notify->Context = Context;
    *Handle = (HANDLE)Notify;

    if (Initial)
    {
        HANDLE Thread = CreateThread(NULL, 0, KushInitialNotifyThread, Notify, 0, NULL);
        if (Thread) CloseHandle(Thread);
    }
    return NO_ERROR;
}

DWORD WINAPI NotifyIpInterfaceChange(USHORT Family, PVOID Callback, PVOID CallerContext,
                                     BOOLEAN InitialNotification, HANDLE *NotificationHandle)
{
    UNREFERENCED_PARAMETER(Family);
    return KushRegister(Callback, CallerContext, InitialNotification, NotificationHandle);
}

DWORD WINAPI NotifyUnicastIpAddressChange(USHORT Family, PVOID Callback, PVOID CallerContext,
                                          BOOLEAN InitialNotification, HANDLE *NotificationHandle)
{
    UNREFERENCED_PARAMETER(Family);
    return KushRegister(Callback, CallerContext, InitialNotification, NotificationHandle);
}

DWORD WINAPI NotifyRouteChange2(USHORT Family, PVOID Callback, PVOID CallerContext,
                                BOOLEAN InitialNotification, HANDLE *NotificationHandle)
{
    UNREFERENCED_PARAMETER(Family);
    return KushRegister(Callback, CallerContext, InitialNotification, NotificationHandle);
}

DWORD WINAPI CancelMibChangeNotify2(HANDLE NotificationHandle)
{
    /* The initial-notification thread may still reference the block; leak it
     * deliberately rather than risk a use-after-free (it is 16 bytes). */
    if (!NotificationHandle) return ERROR_INVALID_PARAMETER;
    return NO_ERROR;
}

DWORD WINAPI GetIpInterfaceTable(USHORT Family, PVOID *Table)
{
    UNREFERENCED_PARAMETER(Family);
    if (!Table) return ERROR_INVALID_PARAMETER;
    *Table = NULL;
    return ERROR_NOT_SUPPORTED;
}

VOID WINAPI FreeMibTable(PVOID Memory)
{
    if (Memory) HeapFree(GetProcessHeap(), 0, Memory);
}
