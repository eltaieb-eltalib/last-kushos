/*
 * Kush 1.1: Kernel Transaction Manager client (ktmw32.dll). Kush has no
 * transaction manager; creating a transaction fails with ERROR_NOT_SUPPORTED,
 * which callers (Firefox) handle by working without a transaction.
 */
#include <windef.h>
#include <winbase.h>

HANDLE WINAPI CreateTransaction(LPSECURITY_ATTRIBUTES lpTransactionAttributes, LPGUID UOW, DWORD CreateOptions,
                                DWORD IsolationLevel, DWORD IsolationFlags, DWORD Timeout, LPWSTR Description)
{
    UNREFERENCED_PARAMETER(lpTransactionAttributes); UNREFERENCED_PARAMETER(UOW); UNREFERENCED_PARAMETER(CreateOptions);
    UNREFERENCED_PARAMETER(IsolationLevel); UNREFERENCED_PARAMETER(IsolationFlags); UNREFERENCED_PARAMETER(Timeout);
    UNREFERENCED_PARAMETER(Description);
    SetLastError(ERROR_NOT_SUPPORTED);
    return INVALID_HANDLE_VALUE;
}

BOOL WINAPI CommitTransaction(HANDLE TransactionHandle)
{
    UNREFERENCED_PARAMETER(TransactionHandle);
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
}

BOOL WINAPI RollbackTransaction(HANDLE TransactionHandle)
{
    UNREFERENCED_PARAMETER(TransactionHandle);
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
}
