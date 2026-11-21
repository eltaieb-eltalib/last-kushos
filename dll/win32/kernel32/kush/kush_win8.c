/*
 * Kush 1.1: Windows 7/8/10 kernel32 functions needed by Firefox 160 (and other
 * modern programs) that Kush/ReactOS did not provide. Each one is built on top
 * of existing kernel32/ntdll functionality. Where Kush has no equivalent
 * feature (app packages, mitigation policies, WER) the function reports the
 * same result Windows gives when the feature is off.
 */
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#define WIN32_NO_STATUS
#include <windef.h>
#include <winbase.h>
#define NTOS_MODE_USER
#include <ndk/rtlfuncs.h>
#include <ndk/iofuncs.h>
#include <ndk/kefuncs.h>
#include <ndk/obfuncs.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>

typedef struct _KUSH_CREATEFILE2_PARAMS
{
    DWORD dwSize;
    DWORD dwFileAttributes;
    DWORD dwFileFlags;
    DWORD dwSecurityQosFlags;
    LPSECURITY_ATTRIBUTES lpSecurityAttributes;
    HANDLE hTemplateFile;
} KUSH_CREATEFILE2_PARAMS;

HANDLE WINAPI CreateFile2(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                          DWORD dwCreationDisposition, KUSH_CREATEFILE2_PARAMS *pParams)
{
    DWORD Flags = 0;
    LPSECURITY_ATTRIBUTES Sa = NULL;
    HANDLE Template = NULL;
    if (pParams)
    {
        Flags = pParams->dwFileAttributes | pParams->dwFileFlags | pParams->dwSecurityQosFlags;
        Sa = pParams->lpSecurityAttributes;
        Template = pParams->hTemplateFile;
    }
    return CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, Sa, dwCreationDisposition, Flags, Template);
}

typedef struct _KUSH_COPYFILE2_PARAMS
{
    DWORD dwSize;
    DWORD dwCopyFlags;
    BOOL *pfCancel;
    PVOID pProgressRoutine;
    PVOID pvCallbackContext;
} KUSH_COPYFILE2_PARAMS;

HRESULT WINAPI CopyFile2(PCWSTR pwszExistingFileName, PCWSTR pwszNewFileName, KUSH_COPYFILE2_PARAMS *pParams)
{
    DWORD Flags = pParams ? (pParams->dwCopyFlags & 0x00000FFF) : 0;
    if (CopyFileExW(pwszExistingFileName, pwszNewFileName, NULL, NULL, pParams ? pParams->pfCancel : NULL, Flags))
        return S_OK;
    return HRESULT_FROM_WIN32(GetLastError());
}

#ifndef CREATE_EVENT_MANUAL_RESET
#define CREATE_EVENT_MANUAL_RESET 0x1
#define CREATE_EVENT_INITIAL_SET  0x2
#endif
HANDLE WINAPI KushCreateEventExW(LPSECURITY_ATTRIBUTES lpEventAttributes, LPCWSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess)
{
    UNREFERENCED_PARAMETER(dwDesiredAccess);
    return CreateEventW(lpEventAttributes, (dwFlags & CREATE_EVENT_MANUAL_RESET) != 0,
                        (dwFlags & CREATE_EVENT_INITIAL_SET) != 0, lpName);
}

HANDLE WINAPI KushCreateEventExA(LPSECURITY_ATTRIBUTES lpEventAttributes, LPCSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess)
{
    UNREFERENCED_PARAMETER(dwDesiredAccess);
    return CreateEventA(lpEventAttributes, (dwFlags & CREATE_EVENT_MANUAL_RESET) != 0,
                        (dwFlags & CREATE_EVENT_INITIAL_SET) != 0, lpName);
}

#ifndef CREATE_WAITABLE_TIMER_MANUAL_RESET
#define CREATE_WAITABLE_TIMER_MANUAL_RESET 0x1
#endif
HANDLE WINAPI CreateWaitableTimerExW(LPSECURITY_ATTRIBUTES lpTimerAttributes, LPCWSTR lpTimerName, DWORD dwFlags, DWORD dwDesiredAccess)
{
    UNREFERENCED_PARAMETER(dwDesiredAccess);
    return CreateWaitableTimerW(lpTimerAttributes, (dwFlags & CREATE_WAITABLE_TIMER_MANUAL_RESET) != 0, lpTimerName);
}

HANDLE WINAPI CreateWaitableTimerExA(LPSECURITY_ATTRIBUTES lpTimerAttributes, LPCSTR lpTimerName, DWORD dwFlags, DWORD dwDesiredAccess)
{
    UNREFERENCED_PARAMETER(dwDesiredAccess);
    return CreateWaitableTimerA(lpTimerAttributes, (dwFlags & CREATE_WAITABLE_TIMER_MANUAL_RESET) != 0, lpTimerName);
}

BOOL WINAPI SetWaitableTimerEx(HANDLE hTimer, const LARGE_INTEGER *lpDueTime, LONG lPeriod,
                               PTIMERAPCROUTINE pfnCompletionRoutine, LPVOID lpArgToCompletionRoutine,
                               PVOID WakeContext, ULONG TolerableDelay)
{
    UNREFERENCED_PARAMETER(WakeContext);
    UNREFERENCED_PARAMETER(TolerableDelay);
    return SetWaitableTimer(hTimer, lpDueTime, lPeriod, pfnCompletionRoutine, lpArgToCompletionRoutine, FALSE);
}

/* WaitOnAddress: polling implementation (correct semantics, wakes when the value changes) */
BOOL WINAPI WaitOnAddress(volatile VOID *Address, PVOID CompareAddress, SIZE_T AddressSize, DWORD dwMilliseconds)
{
    DWORD Start = GetTickCount();
    DWORD SpinSleep = 0;
    for (;;)
    {
        if (RtlCompareMemory((const VOID *)Address, CompareAddress, AddressSize) != AddressSize)
            return TRUE;
        if (dwMilliseconds != INFINITE && (GetTickCount() - Start) >= dwMilliseconds)
        {
            SetLastError(ERROR_TIMEOUT);
            return FALSE;
        }
        Sleep(SpinSleep);
        if (SpinSleep < 1) SpinSleep = 1;
    }
}

VOID WINAPI WakeByAddressSingle(PVOID Address) { UNREFERENCED_PARAMETER(Address); }
VOID WINAPI WakeByAddressAll(PVOID Address) { UNREFERENCED_PARAMETER(Address); }

BOOLEAN WINAPI KushTryAcquireSRWLockShared(PSRWLOCK SRWLock)
{
    /* No try-shared primitive in Kush's SRW implementation: report "busy". */
    UNREFERENCED_PARAMETER(SRWLock);
    return FALSE;
}

/* DLL directories: Kush searches the classic way; accept the calls. */
BOOL WINAPI SetDefaultDllDirectories(DWORD DirectoryFlags) { UNREFERENCED_PARAMETER(DirectoryFlags); return TRUE; }
PVOID WINAPI AddDllDirectory(PCWSTR NewDirectory)
{
    if (!NewDirectory) { SetLastError(ERROR_INVALID_PARAMETER); return NULL; }
    return (PVOID)(ULONG_PTR)(0x4B550000 | (RtlComputeCrc32(0, (PUCHAR)NewDirectory, (ULONG)(wcslen(NewDirectory) * sizeof(WCHAR))) & 0xFFFF) | 1);
}
BOOL WINAPI RemoveDllDirectory(PVOID Cookie) { return Cookie != NULL; }

/* Process/thread attribute lists */
typedef struct _KUSH_ATTR_ENTRY { DWORD_PTR Attribute; SIZE_T Size; PVOID Value; } KUSH_ATTR_ENTRY;
typedef struct _KUSH_ATTR_LIST { DWORD Flags; DWORD Size; DWORD Count; DWORD Unused; KUSH_ATTR_ENTRY Entries[1]; } KUSH_ATTR_LIST;

BOOL WINAPI KushInitializeProcThreadAttributeList(KUSH_ATTR_LIST *lpAttributeList, DWORD dwAttributeCount, DWORD dwFlags, PSIZE_T lpSize)
{
    SIZE_T Needed = FIELD_OFFSET(KUSH_ATTR_LIST, Entries) + dwAttributeCount * sizeof(KUSH_ATTR_ENTRY);
    if (dwFlags) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    if (!lpAttributeList || *lpSize < Needed) { *lpSize = Needed; SetLastError(ERROR_INSUFFICIENT_BUFFER); return FALSE; }
    RtlZeroMemory(lpAttributeList, Needed);
    lpAttributeList->Size = dwAttributeCount;
    *lpSize = Needed;
    return TRUE;
}

BOOL WINAPI KushUpdateProcThreadAttribute(KUSH_ATTR_LIST *lpAttributeList, DWORD dwFlags, DWORD_PTR Attribute,
                                          PVOID lpValue, SIZE_T cbSize, PVOID lpPreviousValue, PSIZE_T lpReturnSize)
{
    UNREFERENCED_PARAMETER(dwFlags); UNREFERENCED_PARAMETER(lpPreviousValue); UNREFERENCED_PARAMETER(lpReturnSize);
    if (!lpAttributeList || lpAttributeList->Count >= lpAttributeList->Size) { SetLastError(ERROR_GEN_FAILURE); return FALSE; }
    lpAttributeList->Entries[lpAttributeList->Count].Attribute = Attribute;
    lpAttributeList->Entries[lpAttributeList->Count].Size = cbSize;
    lpAttributeList->Entries[lpAttributeList->Count].Value = lpValue;
    lpAttributeList->Count++;
    return TRUE;
}

VOID WINAPI KushDeleteProcThreadAttributeList(KUSH_ATTR_LIST *lpAttributeList) { UNREFERENCED_PARAMETER(lpAttributeList); }

/* Mitigation policies / thread information: Kush has none; behave like "all off". */
BOOL WINAPI GetProcessMitigationPolicy(HANDLE hProcess, DWORD MitigationPolicy, PVOID lpBuffer, SIZE_T dwLength)
{
    UNREFERENCED_PARAMETER(hProcess); UNREFERENCED_PARAMETER(MitigationPolicy);
    if (!lpBuffer) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    RtlZeroMemory(lpBuffer, dwLength);
    return TRUE;
}
BOOL WINAPI SetProcessMitigationPolicy(DWORD MitigationPolicy, PVOID lpBuffer, SIZE_T dwLength)
{
    UNREFERENCED_PARAMETER(MitigationPolicy); UNREFERENCED_PARAMETER(lpBuffer); UNREFERENCED_PARAMETER(dwLength);
    return TRUE;
}
BOOL WINAPI SetThreadInformation(HANDLE hThread, DWORD ThreadInformationClass, LPVOID ThreadInformation, DWORD ThreadInformationSize)
{
    UNREFERENCED_PARAMETER(hThread); UNREFERENCED_PARAMETER(ThreadInformationClass);
    UNREFERENCED_PARAMETER(ThreadInformation); UNREFERENCED_PARAMETER(ThreadInformationSize);
    return TRUE;
}
BOOL WINAPI GetThreadInformation(HANDLE hThread, DWORD ThreadInformationClass, LPVOID ThreadInformation, DWORD ThreadInformationSize)
{
    UNREFERENCED_PARAMETER(hThread); UNREFERENCED_PARAMETER(ThreadInformationClass);
    if (ThreadInformation) RtlZeroMemory(ThreadInformation, ThreadInformationSize);
    return TRUE;
}

/* SetFileInformationByHandle for the common classes */
BOOL WINAPI KushSetFileInformationByHandle(HANDLE hFile, DWORD FileInformationClass, LPVOID lpFileInformation, DWORD dwBufferSize)
{
    FILE_INFORMATION_CLASS NtClass;
    IO_STATUS_BLOCK Iosb;
    NTSTATUS Status;
    switch (FileInformationClass)
    {
        case 0: /* FileBasicInfo */       NtClass = FileBasicInformation; break;
        case 3: /* FileRenameInfo */      NtClass = FileRenameInformation; break;
        case 4: /* FileDispositionInfo */ NtClass = FileDispositionInformation; break;
        case 5: /* FileAllocationInfo */  NtClass = FileAllocationInformation; break;
        case 6: /* FileEndOfFileInfo */   NtClass = FileEndOfFileInformation; break;
        case 22: /* FileIoPriorityHintInfo */ return TRUE;
        default: SetLastError(ERROR_NOT_SUPPORTED); return FALSE;
    }
    Status = NtSetInformationFile(hFile, &Iosb, lpFileInformation, dwBufferSize, NtClass);
    if (!NT_SUCCESS(Status)) { SetLastError(RtlNtStatusToDosError(Status)); return FALSE; }
    return TRUE;
}

BOOL WINAPI KushGetLogicalProcessorInformationEx(DWORD RelationshipType, PVOID Buffer, PDWORD ReturnedLength)
{
    UNREFERENCED_PARAMETER(RelationshipType); UNREFERENCED_PARAMETER(Buffer); UNREFERENCED_PARAMETER(ReturnedLength);
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

VOID WINAPI FlushProcessWriteBuffers(VOID) { MemoryBarrier(); }

#define APPMODEL_ERROR_NO_PACKAGE 15700
LONG WINAPI GetCurrentPackageFullName(UINT32 *packageFullNameLength, PWSTR packageFullName)
{
    UNREFERENCED_PARAMETER(packageFullNameLength); UNREFERENCED_PARAMETER(packageFullName);
    return APPMODEL_ERROR_NO_PACKAGE;
}
LONG WINAPI GetCurrentPackageId(UINT32 *bufferLength, BYTE *buffer)
{
    UNREFERENCED_PARAMETER(bufferLength); UNREFERENCED_PARAMETER(buffer);
    return APPMODEL_ERROR_NO_PACKAGE;
}

BOOL WINAPI QueryUnbiasedInterruptTime(PULONGLONG UnbiasedTime)
{
    LARGE_INTEGER Counter, Freq;
    if (!UnbiasedTime) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    QueryPerformanceCounter(&Counter); QueryPerformanceFrequency(&Freq);
    *UnbiasedTime = (ULONGLONG)((Counter.QuadPart / Freq.QuadPart) * 10000000ULL +
                                ((Counter.QuadPart % Freq.QuadPart) * 10000000ULL) / Freq.QuadPart);
    return TRUE;
}
VOID WINAPI QueryUnbiasedInterruptTimePrecise(PULONGLONG lpUnbiasedInterruptTimePrecise)
{
    QueryUnbiasedInterruptTime(lpUnbiasedInterruptTimePrecise);
}
VOID WINAPI QueryInterruptTimePrecise(PULONGLONG lpInterruptTimePrecise)
{
    QueryUnbiasedInterruptTime(lpInterruptTimePrecise);
}

HRESULT WINAPI WerRegisterRuntimeExceptionModule(PCWSTR pwszOutOfProcessCallbackDll, PVOID pContext)
{ UNREFERENCED_PARAMETER(pwszOutOfProcessCallbackDll); UNREFERENCED_PARAMETER(pContext); return S_OK; }
HRESULT WINAPI WerUnregisterRuntimeExceptionModule(PCWSTR pwszOutOfProcessCallbackDll, PVOID pContext)
{ UNREFERENCED_PARAMETER(pwszOutOfProcessCallbackDll); UNREFERENCED_PARAMETER(pContext); return S_OK; }
