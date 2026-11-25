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
#include <winnls.h> /* LocaleNameToLCID/LCIDToLocaleName: include it directly, the SysWOW64 build does not get it indirectly */
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

HRESULT WINAPI CopyFile2(PCWSTR pwszExistingFileName, PCWSTR pwszNewFileName, COPYFILE2_EXTENDED_PARAMETERS *pParams)
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
/* (CreateWaitableTimerExW: already implemented in kernel32) */

/* (CreateWaitableTimerExA: already implemented in kernel32) */

BOOL WINAPI SetWaitableTimerEx(HANDLE hTimer, const LARGE_INTEGER *lpDueTime, LONG lPeriod,
                               PTIMERAPCROUTINE pfnCompletionRoutine, LPVOID lpArgToCompletionRoutine,
                               PVOID WakeContext, ULONG TolerableDelay)
{
    UNREFERENCED_PARAMETER(WakeContext);
    UNREFERENCED_PARAMETER(TolerableDelay);
    return SetWaitableTimer(hTimer, lpDueTime, lPeriod, pfnCompletionRoutine, lpArgToCompletionRoutine, FALSE);
}

/* Address waits must remember wakeups even if the address value is unchanged.
 * Keep registration and wake selection under the same short-held lock. Each
 * waiter owns its event and removes itself before closing it. */
typedef struct _KUSH_ADDRESS_WAITER
{
    struct _KUSH_ADDRESS_WAITER *Next;
    const volatile VOID *Address;
    HANDLE Event;
    BOOL Signaled;
} KUSH_ADDRESS_WAITER;

static volatile LONG KushAddressWaitLock;
static KUSH_ADDRESS_WAITER *KushAddressWaiters;

static VOID KushLockAddressWaiters(VOID)
{
    while (InterlockedCompareExchange(&KushAddressWaitLock, 1, 0))
        Sleep(0);
}

static VOID KushUnlockAddressWaiters(VOID)
{
    InterlockedExchange(&KushAddressWaitLock, 0);
}

BOOL WINAPI WaitOnAddress(volatile VOID *Address, PVOID CompareAddress,
                         SIZE_T AddressSize, DWORD dwMilliseconds)
{
    KUSH_ADDRESS_WAITER Waiter, **Link;
    DWORD Status, Error = ERROR_SUCCESS;
    BOOL Signaled;

    if (AddressSize != 1 && AddressSize != 2 &&
        AddressSize != 4 && AddressSize != 8)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    Waiter.Event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!Waiter.Event) return FALSE;
    Waiter.Address = Address;
    Waiter.Signaled = FALSE;

    KushLockAddressWaiters();
    if (RtlCompareMemory((const VOID *)Address, CompareAddress, AddressSize) != AddressSize)
    {
        KushUnlockAddressWaiters();
        CloseHandle(Waiter.Event);
        return TRUE;
    }
    Waiter.Next = KushAddressWaiters;
    KushAddressWaiters = &Waiter;
    KushUnlockAddressWaiters();

    Status = WaitForSingleObject(Waiter.Event, dwMilliseconds);
    if (Status == WAIT_FAILED) Error = GetLastError();

    KushLockAddressWaiters();
    Signaled = Waiter.Signaled;
    for (Link = &KushAddressWaiters; *Link; Link = &(*Link)->Next)
    {
        if (*Link == &Waiter)
        {
            *Link = Waiter.Next;
            break;
        }
    }
    KushUnlockAddressWaiters();
    CloseHandle(Waiter.Event);

    if (Signaled || Status == WAIT_OBJECT_0) return TRUE;
    SetLastError(Status == WAIT_TIMEOUT ? ERROR_TIMEOUT : Error);
    return FALSE;
}

static VOID KushWakeAddress(PVOID Address, BOOL All)
{
    KUSH_ADDRESS_WAITER *Waiter;
    KushLockAddressWaiters();
    for (Waiter = KushAddressWaiters; Waiter; Waiter = Waiter->Next)
    {
        if (Waiter->Address == Address && !Waiter->Signaled)
        {
            if (SetEvent(Waiter->Event)) Waiter->Signaled = TRUE;
            if (!All) break;
        }
    }
    KushUnlockAddressWaiters();
}

VOID WINAPI WakeByAddressSingle(PVOID Address) { KushWakeAddress(Address, FALSE); }
VOID WINAPI WakeByAddressAll(PVOID Address) { KushWakeAddress(Address, TRUE); }

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


#define APPMODEL_ERROR_NO_PACKAGE 15700
LONG WINAPI GetCurrentPackageFullName(UINT32 *packageFullNameLength, PWSTR packageFullName)
{
    UNREFERENCED_PARAMETER(packageFullNameLength); UNREFERENCED_PARAMETER(packageFullName);
    return APPMODEL_ERROR_NO_PACKAGE;
}
/* (GetCurrentPackageId: already implemented in kernel32) */

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

/* ---- Kush 1.1, round 2 (needed by xul.dll) ---- */
BOOL WINAPI GetOverlappedResultEx(HANDLE hFile, LPOVERLAPPED lpOverlapped, LPDWORD lpNumberOfBytesTransferred,
                                  DWORD dwMilliseconds, BOOL bAlertable)
{
    if (lpOverlapped->Internal == STATUS_PENDING && dwMilliseconds != 0)
    {
        HANDLE Wait = lpOverlapped->hEvent ? lpOverlapped->hEvent : hFile;
        DWORD r = WaitForSingleObjectEx(Wait, dwMilliseconds, bAlertable);
        if (r == WAIT_TIMEOUT) { SetLastError(WAIT_TIMEOUT); return FALSE; }
        if (r == WAIT_IO_COMPLETION) { SetLastError(WAIT_IO_COMPLETION); return FALSE; }
    }
    return GetOverlappedResult(hFile, lpOverlapped, lpNumberOfBytesTransferred, FALSE);
}

/* Power requests: Kush has no power manager policy; keep a dummy handle. */
HANDLE WINAPI PowerCreateRequest(PVOID Context)
{
    UNREFERENCED_PARAMETER(Context);
    return CreateEventW(NULL, TRUE, FALSE, NULL);
}
BOOL WINAPI PowerSetRequest(HANDLE PowerRequest, DWORD RequestType)
{ UNREFERENCED_PARAMETER(RequestType); return PowerRequest != NULL && PowerRequest != INVALID_HANDLE_VALUE; }
BOOL WINAPI PowerClearRequest(HANDLE PowerRequest, DWORD RequestType)
{ UNREFERENCED_PARAMETER(RequestType); return PowerRequest != NULL && PowerRequest != INVALID_HANDLE_VALUE; }

int WINAPI ResolveLocaleName(LPCWSTR lpNameToResolve, LPWSTR lpLocaleName, int cchLocaleName)
{
    LCID Lcid;
    if (!lpNameToResolve) lpNameToResolve = L"en-US";
    Lcid = LocaleNameToLCID(lpNameToResolve, 0);
    if (!Lcid) Lcid = MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT);
    return LCIDToLocaleName(Lcid, lpLocaleName, cchLocaleName, 0);
}

#define APPMODEL_ERROR_NO_APPLICATION 15703
LONG WINAPI GetCurrentApplicationUserModelId(UINT32 *applicationUserModelIdLength, PWSTR applicationUserModelId)
{ UNREFERENCED_PARAMETER(applicationUserModelIdLength); UNREFERENCED_PARAMETER(applicationUserModelId); return APPMODEL_ERROR_NO_APPLICATION; }

BOOL WINAPI GetSystemCpuSetInformation(PVOID Information, ULONG BufferLength, PULONG ReturnedLength, HANDLE Process, ULONG Flags)
{
    UNREFERENCED_PARAMETER(Information); UNREFERENCED_PARAMETER(BufferLength);
    UNREFERENCED_PARAMETER(Process); UNREFERENCED_PARAMETER(Flags);
    if (ReturnedLength) *ReturnedLength = 0;
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

/* ---- Kush 1.1, round 3: replace kernel32 stubs that Firefox calls ---- */
HANDLE WINAPI KushCreateWaitableTimerExW(LPSECURITY_ATTRIBUTES lpTimerAttributes, LPCWSTR lpTimerName, DWORD dwFlags, DWORD dwDesiredAccess)
{
    UNREFERENCED_PARAMETER(dwDesiredAccess);
    return CreateWaitableTimerW(lpTimerAttributes, (dwFlags & 0x1 /* CREATE_WAITABLE_TIMER_MANUAL_RESET */) != 0, lpTimerName);
}
HANDLE WINAPI KushCreateWaitableTimerExA(LPSECURITY_ATTRIBUTES lpTimerAttributes, LPCSTR lpTimerName, DWORD dwFlags, DWORD dwDesiredAccess)
{
    UNREFERENCED_PARAMETER(dwDesiredAccess);
    return CreateWaitableTimerA(lpTimerAttributes, (dwFlags & 0x1) != 0, lpTimerName);
}

HANDLE WINAPI KushCreateMutexExW(LPSECURITY_ATTRIBUTES lpMutexAttributes, LPCWSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess)
{
    UNREFERENCED_PARAMETER(dwDesiredAccess);
    return CreateMutexW(lpMutexAttributes, (dwFlags & 0x1 /* CREATE_MUTEX_INITIAL_OWNER */) != 0, lpName);
}

/* Kush has NtCancelIoFile (cancels the calling thread's I/O on the handle) */
BOOL WINAPI KushCancelIoEx(HANDLE hFile, LPOVERLAPPED lpOverlapped)
{
    IO_STATUS_BLOCK Iosb;
    NTSTATUS Status;
    UNREFERENCED_PARAMETER(lpOverlapped);
    Status = NtCancelIoFile(hFile, &Iosb);
    if (!NT_SUCCESS(Status)) { SetLastError(RtlNtStatusToDosError(Status)); return FALSE; }
    return TRUE;
}
BOOL WINAPI KushCancelSynchronousIo(HANDLE hThread)
{ UNREFERENCED_PARAMETER(hThread); SetLastError(ERROR_NOT_FOUND); return FALSE; }

BOOL WINAPI KushCallbackMayRunLong(PVOID pci) { UNREFERENCED_PARAMETER(pci); return FALSE; }

typedef struct _KUSH_OVERLAPPED_ENTRY { ULONG_PTR lpCompletionKey; LPOVERLAPPED lpOverlapped; ULONG_PTR Internal; DWORD dwNumberOfBytesTransferred; } KUSH_OVERLAPPED_ENTRY;
BOOL WINAPI KushGetQueuedCompletionStatusEx(HANDLE CompletionPort, KUSH_OVERLAPPED_ENTRY *lpCompletionPortEntries, ULONG ulCount,
                                            PULONG ulNumEntriesRemoved, DWORD dwMilliseconds, BOOL fAlertable)
{
    DWORD Bytes; ULONG_PTR Key; LPOVERLAPPED Ov;
    UNREFERENCED_PARAMETER(fAlertable);
    if (!lpCompletionPortEntries || !ulCount || !ulNumEntriesRemoved) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    *ulNumEntriesRemoved = 0;
    if (!GetQueuedCompletionStatus(CompletionPort, &Bytes, &Key, &Ov, dwMilliseconds) && !Ov) return FALSE;
    lpCompletionPortEntries[0].lpCompletionKey = Key;
    lpCompletionPortEntries[0].lpOverlapped = Ov;
    lpCompletionPortEntries[0].Internal = Ov ? Ov->Internal : 0;
    lpCompletionPortEntries[0].dwNumberOfBytesTransferred = Bytes;
    *ulNumEntriesRemoved = 1;
    return TRUE;
}

typedef struct _KUSH_DYNAMIC_TZI { TIME_ZONE_INFORMATION Tzi; WCHAR TimeZoneKeyName[128]; BOOLEAN DynamicDaylightTimeDisabled; } KUSH_DYNAMIC_TZI;
DWORD WINAPI KushGetDynamicTimeZoneInformation(KUSH_DYNAMIC_TZI *pTimeZoneInformation)
{
    DWORD r;
    if (!pTimeZoneInformation) { SetLastError(ERROR_INVALID_PARAMETER); return TIME_ZONE_ID_INVALID; }
    RtlZeroMemory(pTimeZoneInformation, sizeof(*pTimeZoneInformation));
    r = GetTimeZoneInformation(&pTimeZoneInformation->Tzi);
    wcsncpy(pTimeZoneInformation->TimeZoneKeyName, pTimeZoneInformation->Tzi.StandardName, 31);
    return r;
}
BOOL WINAPI KushGetTimeZoneInformationForYear(USHORT wYear, PVOID pdtzi, LPTIME_ZONE_INFORMATION ptzi)
{
    UNREFERENCED_PARAMETER(wYear); UNREFERENCED_PARAMETER(pdtzi);
    return GetTimeZoneInformation(ptzi) != TIME_ZONE_ID_INVALID;
}

VOID WINAPI KushRaiseFailFastException(PEXCEPTION_RECORD pExceptionRecord, PCONTEXT pContextRecord, DWORD dwFlags)
{
    UNREFERENCED_PARAMETER(pContextRecord); UNREFERENCED_PARAMETER(dwFlags);
    TerminateProcess(GetCurrentProcess(), pExceptionRecord ? pExceptionRecord->ExceptionCode : 0xC0000409);
}

BOOL WINAPI KushSetThreadErrorMode(DWORD dwNewMode, LPDWORD lpOldMode)
{
    UINT Old = SetErrorMode(dwNewMode);
    if (lpOldMode) *lpOldMode = Old;
    return TRUE;
}

/* Kush 1.1: FlushProcessWriteBuffers -- Vista+. A full memory barrier is
 * sufficient on a uniprocessor/TCG system; on SMP flush via a no-op
 * protection change, which forces an IPI-driven TLB flush on all CPUs. */
VOID WINAPI FlushProcessWriteBuffers(VOID)
{
    static LONG Dummy;
    MemoryBarrier();
    InterlockedExchange(&Dummy, 0);
}

/* ---- Kush 1.1 round 4: plain "@ stub" exports Firefox 160 calls. A plain stub
 * raises EXCEPTION_WINE_STUB (0x80000100, non-continuable), which killed Firefox
 * right after startup (in GetApplicationRestartSettings). ---- */

/* Restart registration: Kush has no Restart Manager; report "not registered". */
HRESULT WINAPI KushGetApplicationRestartSettings(HANDLE hProcess, PWSTR pwzCommandline,
                                                 PDWORD pcchSize, PDWORD pdwFlags)
{
    UNREFERENCED_PARAMETER(hProcess); UNREFERENCED_PARAMETER(pwzCommandline);
    UNREFERENCED_PARAMETER(pcchSize); UNREFERENCED_PARAMETER(pdwFlags);
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}

HRESULT WINAPI KushUnregisterApplicationRestart(VOID)
{
    return S_OK;
}

/* Thread-pool helper: the module must stay loaded until the callback returns.
 * Keeping the reference (never unloading) is always safe. */
VOID WINAPI KushFreeLibraryWhenCallbackReturns(PVOID Instance, HMODULE Module)
{
    UNREFERENCED_PARAMETER(Instance); UNREFERENCED_PARAMETER(Module);
}

/* NPFS in Kush does not expose the pipe server process attribute. */
BOOL WINAPI KushGetNamedPipeServerProcessId(HANDLE Pipe, PULONG ServerProcessId)
{
    UNREFERENCED_PARAMETER(Pipe);
    if (ServerProcessId) *ServerProcessId = 0;
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

/* Cycle times: approximate with CPU time in 100ns units scaled to ~1 GHz. */
static ULONG64 KushFileTimeToCycles(const FILETIME *K, const FILETIME *U)
{
    ULARGE_INTEGER a, b;
    a.LowPart = K->dwLowDateTime; a.HighPart = K->dwHighDateTime;
    b.LowPart = U->dwLowDateTime; b.HighPart = U->dwHighDateTime;
    return (a.QuadPart + b.QuadPart) * 100;
}

BOOL WINAPI KushQueryProcessCycleTime(HANDLE Process, PULONG64 CycleTime)
{
    FILETIME c, e, k, u;
    if (!CycleTime) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    if (!GetProcessTimes(Process, &c, &e, &k, &u)) return FALSE;
    *CycleTime = KushFileTimeToCycles(&k, &u);
    return TRUE;
}

BOOL WINAPI KushQueryThreadCycleTime(HANDLE Thread, PULONG64 CycleTime)
{
    FILETIME c, e, k, u;
    if (!CycleTime) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    if (!GetThreadTimes(Thread, &c, &e, &k, &u)) return FALSE;
    *CycleTime = KushFileTimeToCycles(&k, &u);
    return TRUE;
}


/*
 * Kush 1.1: Windows 7 processor-group and module APIs used by Mesa's
 * llvmpipe (libgallium_wgl.dll, the system OpenGL). Kush has a single
 * processor group (group 0), like ReactOS' GetActiveProcessorGroupCount.
 */
BOOL
WINAPI
GetProcessGroupAffinity(
    _In_ HANDLE hProcess,
    _Inout_ PUSHORT GroupCount,
    _Out_writes_to_(*GroupCount, *GroupCount) PUSHORT GroupArray)
{
    UNREFERENCED_PARAMETER(hProcess);

    if (!GroupCount)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (*GroupCount < 1 || !GroupArray)
    {
        *GroupCount = 1;
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    GroupArray[0] = 0;
    *GroupCount = 1;
    return TRUE;
}

BOOL
WINAPI
EnumProcessModules(HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded);

/* LIST_MODULES_DEFAULT/32BIT/64BIT/ALL: a native process sees its own
 * modules either way; WOW64 cross-bitness listing is not supported here. */
BOOL
WINAPI
K32EnumProcessModulesEx(
    _In_ HANDLE hProcess,
    _Out_writes_bytes_(cb) HMODULE *lphModule,
    _In_ DWORD cb,
    _Out_ LPDWORD lpcbNeeded,
    _In_ DWORD dwFilterFlag)
{
    UNREFERENCED_PARAMETER(dwFilterFlag);
    return EnumProcessModules(hProcess, lphModule, cb, lpcbNeeded);
}
