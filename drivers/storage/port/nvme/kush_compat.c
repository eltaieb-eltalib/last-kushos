/*
 * Kush 1.1 / ReactOS compatibility for the OpenFabrics NVMe miniport.
 * - StorPortQuerySystemTime: the ReactOS storport export forwards to
 *   KeQuerySystemTime, which is a macro on amd64, so it is not linkable there.
 * - memcpy_s: not provided by the kernel CRT used for drivers.
 * - ScsiPortWmi*: WMI is not supported; the driver's WMI requests are answered
 *   as "not handled" (the storage path does not need WMI).
 */
#include "precomp.h"

#undef StorPortQuerySystemTime
VOID NTAPI KushNvmeQuerySystemTime(PLARGE_INTEGER CurrentTime)
{
    KeQuerySystemTime(CurrentTime);
}

int __cdecl memcpy_s(void *dest, size_t destsz, const void *src, size_t count)
{
    if (!dest) return 22; /* EINVAL */
    if (!src || destsz < count)
    {
        RtlZeroMemory(dest, destsz);
        return src ? 34 /* ERANGE */ : 22;
    }
    RtlCopyMemory(dest, src, count);
    return 0;
}

BOOLEAN NTAPI ScsiPortWmiDispatchFunction(PSCSI_WMILIB_CONTEXT WmiLibInfo, UCHAR MinorFunction,
    PVOID DeviceContext, PSCSIWMI_REQUEST_CONTEXT RequestContext, PVOID DataPath,
    ULONG BufferSize, PVOID Buffer)
{
    UNREFERENCED_PARAMETER(WmiLibInfo); UNREFERENCED_PARAMETER(MinorFunction);
    UNREFERENCED_PARAMETER(DeviceContext); UNREFERENCED_PARAMETER(RequestContext);
    UNREFERENCED_PARAMETER(DataPath); UNREFERENCED_PARAMETER(BufferSize); UNREFERENCED_PARAMETER(Buffer);
    return FALSE;
}

VOID NTAPI ScsiPortWmiPostProcess(PSCSIWMI_REQUEST_CONTEXT RequestContext, UCHAR SrbStatus, ULONG BufferUsed)
{
    if (RequestContext)
    {
        RequestContext->ReturnStatus = (SrbStatus == SRB_STATUS_SUCCESS) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
        RequestContext->ReturnSize = BufferUsed;
    }
}
