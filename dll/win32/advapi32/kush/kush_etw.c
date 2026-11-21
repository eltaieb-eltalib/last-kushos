/*
 * Kush 1.1: Event Tracing for Windows (ETW provider side) and transacted
 * registry entry points used by Firefox. Kush has no ETW consumer, so a
 * provider registers successfully and its events are dropped, which is what
 * Windows does when no trace session listens.
 */
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#include <windef.h>
#include <winbase.h>
#include <winreg.h>

typedef ULONGLONG KUSH_REGHANDLE;

ULONG WINAPI EventRegister(LPCGUID ProviderId, PVOID EnableCallback, PVOID CallbackContext, KUSH_REGHANDLE *RegHandle)
{
    UNREFERENCED_PARAMETER(ProviderId); UNREFERENCED_PARAMETER(EnableCallback); UNREFERENCED_PARAMETER(CallbackContext);
    if (RegHandle) *RegHandle = (KUSH_REGHANDLE)(ULONG_PTR)0x4B555348; /* any non-zero handle */
    return ERROR_SUCCESS;
}
ULONG WINAPI EventUnregister(KUSH_REGHANDLE RegHandle) { UNREFERENCED_PARAMETER(RegHandle); return ERROR_SUCCESS; }
ULONG WINAPI EventSetInformation(KUSH_REGHANDLE RegHandle, ULONG InformationClass, PVOID EventInformation, ULONG InformationLength)
{ UNREFERENCED_PARAMETER(RegHandle); UNREFERENCED_PARAMETER(InformationClass); UNREFERENCED_PARAMETER(EventInformation); UNREFERENCED_PARAMETER(InformationLength); return ERROR_SUCCESS; }
ULONG WINAPI EventWrite(KUSH_REGHANDLE RegHandle, PVOID EventDescriptor, ULONG UserDataCount, PVOID UserData)
{ UNREFERENCED_PARAMETER(RegHandle); UNREFERENCED_PARAMETER(EventDescriptor); UNREFERENCED_PARAMETER(UserDataCount); UNREFERENCED_PARAMETER(UserData); return ERROR_SUCCESS; }
ULONG WINAPI EventWriteTransfer(KUSH_REGHANDLE RegHandle, PVOID EventDescriptor, LPCGUID ActivityId, LPCGUID RelatedActivityId, ULONG UserDataCount, PVOID UserData)
{ UNREFERENCED_PARAMETER(RegHandle); UNREFERENCED_PARAMETER(EventDescriptor); UNREFERENCED_PARAMETER(ActivityId); UNREFERENCED_PARAMETER(RelatedActivityId); UNREFERENCED_PARAMETER(UserDataCount); UNREFERENCED_PARAMETER(UserData); return ERROR_SUCCESS; }
ULONG WINAPI EventWriteEx(KUSH_REGHANDLE RegHandle, PVOID EventDescriptor, ULONG64 Filter, ULONG Flags, LPCGUID ActivityId, LPCGUID RelatedActivityId, ULONG UserDataCount, PVOID UserData)
{ UNREFERENCED_PARAMETER(RegHandle); UNREFERENCED_PARAMETER(EventDescriptor); UNREFERENCED_PARAMETER(Filter); UNREFERENCED_PARAMETER(Flags); UNREFERENCED_PARAMETER(ActivityId); UNREFERENCED_PARAMETER(RelatedActivityId); UNREFERENCED_PARAMETER(UserDataCount); UNREFERENCED_PARAMETER(UserData); return ERROR_SUCCESS; }
BOOLEAN WINAPI EventEnabled(KUSH_REGHANDLE RegHandle, PVOID EventDescriptor) { UNREFERENCED_PARAMETER(RegHandle); UNREFERENCED_PARAMETER(EventDescriptor); return FALSE; }
BOOLEAN WINAPI EventProviderEnabled(KUSH_REGHANDLE RegHandle, UCHAR Level, ULONGLONG Keyword) { UNREFERENCED_PARAMETER(RegHandle); UNREFERENCED_PARAMETER(Level); UNREFERENCED_PARAMETER(Keyword); return FALSE; }
ULONG WINAPI EventActivityIdControl(ULONG ControlCode, LPGUID ActivityId)
{ UNREFERENCED_PARAMETER(ControlCode); if (ActivityId) ZeroMemory(ActivityId, sizeof(GUID)); return ERROR_SUCCESS; }

/* No Kernel Transaction Manager in Kush: run the operation without a transaction. */
LSTATUS WINAPI RegCreateKeyTransactedW(HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, LPWSTR lpClass, DWORD dwOptions,
                                       REGSAM samDesired, const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                       PHKEY phkResult, LPDWORD lpdwDisposition, HANDLE hTransaction, PVOID pExtendedParemeter)
{
    UNREFERENCED_PARAMETER(hTransaction); UNREFERENCED_PARAMETER(pExtendedParemeter);
    return RegCreateKeyExW(hKey, lpSubKey, Reserved, lpClass, dwOptions, samDesired, lpSecurityAttributes, phkResult, lpdwDisposition);
}
LSTATUS WINAPI RegOpenKeyTransactedW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult,
                                     HANDLE hTransaction, PVOID pExtendedParemeter)
{
    UNREFERENCED_PARAMETER(hTransaction); UNREFERENCED_PARAMETER(pExtendedParemeter);
    return RegOpenKeyExW(hKey, lpSubKey, ulOptions, samDesired, phkResult);
}

/* Kush 1.1: AddMandatoryAce (Vista) -- used by the Firefox sandbox to set
 * integrity labels. Builds a SYSTEM_MANDATORY_LABEL_ACE and appends it. */
BOOL WINAPI AddMandatoryAce(PACL pAcl, DWORD dwAceRevision, DWORD AceFlags,
                            DWORD MandatoryPolicy, PSID pLabelSid)
{
    struct { BYTE AceType, AceFlags; WORD AceSize; DWORD Mask; } *Ace;
    DWORD SidLen, Size;
    BOOL Ret;

    if (!pLabelSid || !IsValidSid(pLabelSid))
    {
        SetLastError(ERROR_INVALID_SID);
        return FALSE;
    }
    SidLen = GetLengthSid(pLabelSid);
    Size = sizeof(*Ace) + SidLen;
    Ace = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, Size);
    if (!Ace)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    Ace->AceType = 0x11; /* SYSTEM_MANDATORY_LABEL_ACE_TYPE */
    Ace->AceFlags = (BYTE)AceFlags;
    Ace->AceSize = (WORD)Size;
    Ace->Mask = MandatoryPolicy;
    CopySid(SidLen, (PBYTE)(Ace + 1), pLabelSid);
    Ret = AddAce(pAcl, dwAceRevision < ACL_REVISION ? ACL_REVISION : dwAceRevision, MAXDWORD, Ace, Size);
    HeapFree(GetProcessHeap(), 0, Ace);
    return Ret;
}
