/*
 * ReactOS VBE miniport video driver - UEFI GOP framebuffer fallback
 *
 * Copyright (C) 2026 Kush 1.1
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * On UEFI-only systems there is no legacy BIOS, so the VBE/int10 path
 * used by this miniport cannot work: there is no VESA BIOS to query
 * for video modes and no VGA hardware to program.  Without a video
 * device, win32k fails with VIDEO_DRIVER_INIT_FAILURE (0xB4).
 *
 * The UEFI loader (FreeLoader) already describes the GOP framebuffer
 * in the ARC hardware tree, which the kernel copies into the registry
 * under HARDWARE\DESCRIPTION\System as a "DisplayController" node whose
 * "Configuration Data" value is a CM_FULL_RESOURCE_DESCRIPTOR containing
 * the video RAM range plus a CM_FRAMEBUF_DEVICE_DATA descriptor
 * (resolution, stride, pixel format).  This file reads that information
 * and sets up a single linear-framebuffer VBE mode for it, so the
 * framebuf display driver can drive the desktop on UEFI-only hardware.
 */

#include "vbemp.h"

/* ACCESS_MASK is not visible through <miniport.h>. */
#ifndef ACCESS_MASK
typedef ULONG ACCESS_MASK;
#endif

/* NOTE: Do NOT include <ntifs.h> or <drivers/bootvid/framebuf.h> here:
 * <ntifs.h> pulls in <ntddk.h>, which redefines enums already defined
 * by <miniport.h> (included via vbemp.h), and framebuf.h's anonymous
 * struct members are not usable through this miniport's headers.
 * Instead, declare just the registry routines and the key-information
 * types this file needs. <miniport.h> does not provide NTSYSAPI, the
 * Rtl string helpers, or the registry value types, so they are
 * declared / defined locally below. */
NTSTATUS
NTAPI
ZwOpenKey(
    _Out_ PHANDLE KeyHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ POBJECT_ATTRIBUTES ObjectAttributes);

NTSTATUS
NTAPI
ZwEnumerateKey(
    _In_ HANDLE KeyHandle,
    _In_ ULONG Index,
    _In_ ULONG KeyInformationClass,
    _Out_opt_ PVOID KeyInformation,
    _In_ ULONG Length,
    _Out_ PULONG ResultLength);

NTSTATUS
NTAPI
ZwQueryValueKey(
    _In_ HANDLE KeyHandle,
    _In_ PUNICODE_STRING ValueName,
    _In_ ULONG KeyValueInformationClass,
    _Out_opt_ PVOID KeyValueInformation,
    _In_ ULONG Length,
    _Out_ PULONG ResultLength);

NTSTATUS
NTAPI
ZwClose(
    _In_ HANDLE Handle);

/* DbgPrint is the kernel debug print used by the KUSH-DBG traces; it
 * always reaches the serial log, unlike VideoPortDebugPrint which is
 * filtered in Release builds. */
ULONG
NTAPI
DbgPrint(
    _In_ PCSTR Format,
    ...);

VOID
NTAPI
RtlInitUnicodeString(
    _Out_ PUNICODE_STRING DestinationString,
    _In_opt_ PCWSTR SourceString);

LONG
NTAPI
RtlCompareUnicodeString(
    _In_ PCUNICODE_STRING String1,
    _In_ PCUNICODE_STRING String2,
    _In_ BOOLEAN CaseInsensitive);

/* NTSTATUS codes and KEY_READ are not visible through <miniport.h>;
 * define the few values this file uses (from sdk/include/psdk/ntstatus.h
 * and sdk/include/xdk/cmtypes.h). */
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000)
#endif
#ifndef STATUS_BUFFER_OVERFLOW
#define STATUS_BUFFER_OVERFLOW ((NTSTATUS)0x80000005)
#endif
#ifndef STATUS_BUFFER_TOO_SMALL
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023)
#endif
#ifndef STATUS_INSUFFICIENT_RESOURCES
#define STATUS_INSUFFICIENT_RESOURCES ((NTSTATUS)0xC000009A)
#endif
#ifndef STATUS_DEVICE_DOES_NOT_EXIST
#define STATUS_DEVICE_DOES_NOT_EXIST ((NTSTATUS)0xC00000C0)
#endif
#ifndef STATUS_DEVICE_CONFIGURATION_ERROR
#define STATUS_DEVICE_CONFIGURATION_ERROR ((NTSTATUS)0xC0000182)
#endif
#ifndef KEY_READ
#define KEY_READ 0x20019
#endif
#ifndef REG_FULL_RESOURCE_DESCRIPTOR
#define REG_FULL_RESOURCE_DESCRIPTOR 9
#endif

/* Layout of CM_FRAMEBUF_DEVICE_DATA (sdk/include/reactos/drivers/bootvid/
 * framebuf.h), written out explicitly here to avoid that header's
 * anonymous-struct members, which this miniport's headers do not cope
 * with. Must match the on-disk / in-registry layout exactly. */
typedef struct _GOP_FRAMEBUF_DATA
{
    USHORT Version;          /* CM_VIDEO_DEVICE_DATA.Version */
    USHORT Revision;         /* CM_VIDEO_DEVICE_DATA.Revision */
    ULONG VideoClock;        /* CM_VIDEO_DEVICE_DATA.VideoClock */
    ULONG FrameBufferOffset;
    ULONG ScreenWidth;
    ULONG ScreenHeight;
    ULONG PixelsPerScanLine;
    ULONG BitsPerPixel;
    ULONG RedMask;
    ULONG GreenMask;
    ULONG BlueMask;
    ULONG ReservedMask;
} GOP_FRAMEBUF_DATA, *PGOP_FRAMEBUF_DATA;

/* Layout of the CM resource descriptors (sdk/include/xdk/cmtypes.h),
 * written out explicitly here because <cmtypes.h> is guarded by
 * _WDMDDK_ and is not usable from a video miniport. Only the fields
 * this file reads are named. The real structs use 4-byte packing
 * (pshpack4.h in cmtypes.h), so sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR)
 * is 20, NOT 32: header 4 bytes + union 16 bytes (largest member is
 * the 16-byte Interrupt). Using the wrong size mis-indexes the
 * descriptor array and the framebuffer data that follows it.
 * Real size: sizeof(GOP_PARTIAL_RESOURCE_DESCRIPTOR) must be 20. */
#define GOP_CmResourceTypeMemory         3
#define GOP_CmResourceTypeDeviceSpecific 5

#include <pshpack4.h>
typedef struct _GOP_PARTIAL_RESOURCE_DESCRIPTOR
{
    UCHAR Type;
    UCHAR ShareDisposition;
    USHORT Flags;
    union
    {
        struct
        {
            PHYSICAL_ADDRESS Start;
            ULONG Length;
        } Memory;
        struct
        {
            ULONG DataSize;
            ULONG Reserved1;
            ULONG Reserved2;
        } DeviceSpecificData;
        UCHAR Raw[16]; /* force the union to the real 16-byte size */
    } u;
} GOP_PARTIAL_RESOURCE_DESCRIPTOR, *PGOP_PARTIAL_RESOURCE_DESCRIPTOR;

typedef struct _GOP_PARTIAL_RESOURCE_LIST
{
    USHORT Version;
    USHORT Revision;
    ULONG Count;
    GOP_PARTIAL_RESOURCE_DESCRIPTOR PartialDescriptors[1];
} GOP_PARTIAL_RESOURCE_LIST, *PGOP_PARTIAL_RESOURCE_LIST;

typedef struct _GOP_FULL_RESOURCE_DESCRIPTOR
{
    ULONG InterfaceType;
    ULONG BusNumber;
    GOP_PARTIAL_RESOURCE_LIST PartialResourceList;
} GOP_FULL_RESOURCE_DESCRIPTOR, *PGOP_FULL_RESOURCE_DESCRIPTOR;
#include <poppack.h>

/* Key information classes (subset of KEY_INFORMATION_CLASS /
 * KEY_VALUE_INFORMATION_CLASS). */
#define GopKeyBasicInformation 0
#define GopKeyValuePartialInformation 2

typedef struct _GOP_KEY_BASIC_INFORMATION
{
    LARGE_INTEGER LastWriteTime;
    ULONG TitleIndex;
    ULONG NameLength;
    WCHAR Name[1];
} GOP_KEY_BASIC_INFORMATION, *PGOP_KEY_BASIC_INFORMATION;

typedef struct _GOP_KEY_VALUE_PARTIAL_INFORMATION
{
    ULONG TitleIndex;
    ULONG Type;
    ULONG DataLength;
    UCHAR Data[1];
} GOP_KEY_VALUE_PARTIAL_INFORMATION, *PGOP_KEY_VALUE_PARTIAL_INFORMATION;

/* Maximum registry tree depth to search for the DisplayController node. */
#define GOP_MAX_DEPTH 6

/* Forward declarations */
static NTSTATUS
GopParseConfigData(
    _In_ PVOID ConfigData,
    _In_ ULONG ConfigDataLength,
    _Out_ PPHYSICAL_ADDRESS FrameBufferBase,
    _Out_ PULONG FrameBufferSize,
    _Out_ PULONG ScreenWidth,
    _Out_ PULONG ScreenHeight,
    _Out_ PULONG PixelsPerScanLine,
    _Out_ PULONG BitsPerPixel,
    _Out_ PULONG RedMask,
    _Out_ PULONG GreenMask,
    _Out_ PULONG BlueMask);

static NTSTATUS
GopTryKey(
    _In_ HANDLE KeyHandle,
    _Out_ PPHYSICAL_ADDRESS FrameBufferBase,
    _Out_ PULONG FrameBufferSize,
    _Out_ PULONG ScreenWidth,
    _Out_ PULONG ScreenHeight,
    _Out_ PULONG PixelsPerScanLine,
    _Out_ PULONG BitsPerPixel,
    _Out_ PULONG RedMask,
    _Out_ PULONG GreenMask,
    _Out_ PULONG BlueMask);

static NTSTATUS
GopTryDisplayController(
    _In_ HANDLE DisplayKey,
    _Out_ PPHYSICAL_ADDRESS FrameBufferBase,
    _Out_ PULONG FrameBufferSize,
    _Out_ PULONG ScreenWidth,
    _Out_ PULONG ScreenHeight,
    _Out_ PULONG PixelsPerScanLine,
    _Out_ PULONG BitsPerPixel,
    _Out_ PULONG RedMask,
    _Out_ PULONG GreenMask,
    _Out_ PULONG BlueMask);

static NTSTATUS
GopSearchKey(
    _In_ HANDLE ParentKey,
    _In_ ULONG Depth,
    _Out_ PPHYSICAL_ADDRESS FrameBufferBase,
    _Out_ PULONG FrameBufferSize,
    _Out_ PULONG ScreenWidth,
    _Out_ PULONG ScreenHeight,
    _Out_ PULONG PixelsPerScanLine,
    _Out_ PULONG BitsPerPixel,
    _Out_ PULONG RedMask,
    _Out_ PULONG GreenMask,
    _Out_ PULONG BlueMask);

/*
 * Convert a color mask (a contiguous run of 1 bits) to its size
 * (number of bits) and field position (number of trailing zero bits).
 */
static VOID
GopMaskToSizePos(
    _In_ ULONG Mask,
    _Out_ PUCHAR Size,
    _Out_ PUCHAR Position)
{
    UCHAR Pos = 0;
    UCHAR Count = 0;
    ULONG M = Mask;

    while (M && !(M & 1))
    {
        M >>= 1;
        Pos++;
    }
    while (M & 1)
    {
        M >>= 1;
        Count++;
    }
    *Size = Count;
    *Position = Pos;
}

/*
 * Parse a CM_FULL_RESOURCE_DESCRIPTOR ("Configuration Data" value) and
 * extract the GOP framebuffer parameters.
 */
static NTSTATUS
GopParseConfigData(
    _In_ PVOID ConfigData,
    _In_ ULONG ConfigDataLength,
    _Out_ PPHYSICAL_ADDRESS FrameBufferBase,
    _Out_ PULONG FrameBufferSize,
    _Out_ PULONG ScreenWidth,
    _Out_ PULONG ScreenHeight,
    _Out_ PULONG PixelsPerScanLine,
    _Out_ PULONG BitsPerPixel,
    _Out_ PULONG RedMask,
    _Out_ PULONG GreenMask,
    _Out_ PULONG BlueMask)
{
    PGOP_FULL_RESOURCE_DESCRIPTOR FullDesc;
    PGOP_PARTIAL_RESOURCE_LIST ResList;
    PGOP_PARTIAL_RESOURCE_DESCRIPTOR Descriptor;
    PGOP_FRAMEBUF_DATA FbData = NULL;
    PHYSICAL_ADDRESS VramBase;
    ULONG VramSize = 0;
    ULONG i;

    if (!ConfigData ||
        (ConfigDataLength < sizeof(GOP_FULL_RESOURCE_DESCRIPTOR)))
    {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    DbgPrint("KUSH-GOP: GopParseConfigData len %lu\n", ConfigDataLength);
    FullDesc = (PGOP_FULL_RESOURCE_DESCRIPTOR)ConfigData;
    ResList = &FullDesc->PartialResourceList;
    DbgPrint("KUSH-GOP: ResList Count %lu\n", ResList->Count);

    /* Sanity: the list must fit in the buffer. */
    if (ConfigDataLength <
        (FIELD_OFFSET(GOP_FULL_RESOURCE_DESCRIPTOR, PartialResourceList) +
         FIELD_OFFSET(GOP_PARTIAL_RESOURCE_LIST, PartialDescriptors) +
         ResList->Count * sizeof(GOP_PARTIAL_RESOURCE_DESCRIPTOR)))
    {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    VramBase.QuadPart = 0;

    for (i = 0; i < ResList->Count; i++)
    {
        Descriptor = &ResList->PartialDescriptors[i];

        if (Descriptor->Type == GOP_CmResourceTypeMemory)
        {
            /* First memory descriptor is the video RAM range. */
            if (VramBase.QuadPart == 0)
            {
                VramBase = Descriptor->u.Memory.Start;
                VramSize = Descriptor->u.Memory.Length;
            }
        }
        else if (Descriptor->Type == GOP_CmResourceTypeDeviceSpecific)
        {
            if (Descriptor->u.DeviceSpecificData.DataSize >=
                sizeof(GOP_FRAMEBUF_DATA))
            {
                /* The framebuffer data follows the descriptor. Verify
                 * it lies inside the supplied buffer before reading it. */
                if (((PUCHAR)(Descriptor + 1) + sizeof(GOP_FRAMEBUF_DATA) <=
                     (PUCHAR)ConfigData + ConfigDataLength))
                {
                    FbData = (PGOP_FRAMEBUF_DATA)(Descriptor + 1);
                }
                else
                {
                    DbgPrint("KUSH-GOP: framebuffer data past end of buffer\n");
                }
            }
        }
    }

    if ((VramBase.QuadPart == 0) || (VramSize == 0) || !FbData)
        return STATUS_DEVICE_DOES_NOT_EXIST;

    if ((FbData->ScreenWidth == 0) || (FbData->ScreenHeight == 0) ||
        (FbData->BitsPerPixel == 0) || (FbData->PixelsPerScanLine == 0))
    {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    *FrameBufferBase = VramBase;
    FrameBufferBase->QuadPart += FbData->FrameBufferOffset;
    *FrameBufferSize = FbData->ScreenHeight *
                       FbData->PixelsPerScanLine *
                       ((FbData->BitsPerPixel + 7) / 8);
    *ScreenWidth = FbData->ScreenWidth;
    *ScreenHeight = FbData->ScreenHeight;
    *PixelsPerScanLine = FbData->PixelsPerScanLine;
    *BitsPerPixel = FbData->BitsPerPixel;
    *RedMask = FbData->RedMask;
    *GreenMask = FbData->GreenMask;
    *BlueMask = FbData->BlueMask;

    return STATUS_SUCCESS;
}

/*
 * Try to read the GOP framebuffer "Configuration Data" from one registry key.
 */
static NTSTATUS
GopTryKey(
    _In_ HANDLE KeyHandle,
    _Out_ PPHYSICAL_ADDRESS FrameBufferBase,
    _Out_ PULONG FrameBufferSize,
    _Out_ PULONG ScreenWidth,
    _Out_ PULONG ScreenHeight,
    _Out_ PULONG PixelsPerScanLine,
    _Out_ PULONG BitsPerPixel,
    _Out_ PULONG RedMask,
    _Out_ PULONG GreenMask,
    _Out_ PULONG BlueMask)
{
    UNICODE_STRING ValueName;
    PGOP_KEY_VALUE_PARTIAL_INFORMATION ValueInfo = NULL;
    ULONG ValueLength = 0;
    NTSTATUS Status;

    RtlInitUnicodeString(&ValueName, L"Configuration Data");

    /* Query the value size first. */
    Status = ZwQueryValueKey(KeyHandle,
                             &ValueName,
                             GopKeyValuePartialInformation,
                             NULL,
                             0,
                             &ValueLength);
    if ((Status != STATUS_BUFFER_TOO_SMALL) &&
        (Status != STATUS_BUFFER_OVERFLOW))
    {
        return STATUS_DEVICE_DOES_NOT_EXIST;
    }

    ValueInfo = VideoPortAllocatePool(NULL, VpPagedPool, ValueLength, TAG_VBE);
    if (!ValueInfo)
        return STATUS_INSUFFICIENT_RESOURCES;

    Status = ZwQueryValueKey(KeyHandle,
                             &ValueName,
                             GopKeyValuePartialInformation,
                             ValueInfo,
                             ValueLength,
                             &ValueLength);
    if (!NT_SUCCESS(Status))
    {
        VideoPortFreePool(NULL, ValueInfo);
        return Status;
    }

    if (ValueInfo->Type != REG_FULL_RESOURCE_DESCRIPTOR)
    {
        VideoPortFreePool(NULL, ValueInfo);
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    DbgPrint("KUSH-GOP: GopTryKey value type %lu len %lu\n", ValueInfo->Type, ValueInfo->DataLength);
    Status = GopParseConfigData(ValueInfo->Data,
                                ValueInfo->DataLength,
                                FrameBufferBase,
                                FrameBufferSize,
                                ScreenWidth,
                                ScreenHeight,
                                PixelsPerScanLine,
                                BitsPerPixel,
                                RedMask,
                                GreenMask,
                                BlueMask);

    VideoPortFreePool(NULL, ValueInfo);
    return Status;
}

/*
 * Try every numbered child of a DisplayController key.
 * The "Configuration Data" value lives on the numbered instance key
 * (e.g. ...\DisplayController\0), not on the type key itself.
 */
static NTSTATUS
GopTryDisplayController(
    _In_ HANDLE DisplayKey,
    _Out_ PPHYSICAL_ADDRESS FrameBufferBase,
    _Out_ PULONG FrameBufferSize,
    _Out_ PULONG ScreenWidth,
    _Out_ PULONG ScreenHeight,
    _Out_ PULONG PixelsPerScanLine,
    _Out_ PULONG BitsPerPixel,
    _Out_ PULONG RedMask,
    _Out_ PULONG GreenMask,
    _Out_ PULONG BlueMask)
{
    PGOP_KEY_BASIC_INFORMATION KeyInfo = NULL;
    ULONG KeyInfoLength = 256;
    ULONG Index;
    NTSTATUS Status;
    NTSTATUS BestStatus = STATUS_DEVICE_DOES_NOT_EXIST;
    UNICODE_STRING SubKeyName;
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE SubKeyHandle;
    WCHAR NameBuffer[64];

    DbgPrint("KUSH-GOP: GopTryDisplayController entry\n");
    /* First try the key itself (in case data is stored directly). */
    Status = GopTryKey(DisplayKey,
                       FrameBufferBase,
                       FrameBufferSize,
                       ScreenWidth,
                       ScreenHeight,
                       PixelsPerScanLine,
                       BitsPerPixel,
                       RedMask,
                       GreenMask,
                       BlueMask);
    if (NT_SUCCESS(Status))
        return STATUS_SUCCESS;

    KeyInfo = VideoPortAllocatePool(NULL, VpPagedPool, KeyInfoLength, TAG_VBE);
    if (!KeyInfo)
        return STATUS_INSUFFICIENT_RESOURCES;

    for (Index = 0; ; Index++)
    {
        ULONG ResultLength = 0;

        Status = ZwEnumerateKey(DisplayKey,
                                Index,
                                GopKeyBasicInformation,
                                KeyInfo,
                                KeyInfoLength,
                                &ResultLength);
        if (Status == STATUS_BUFFER_TOO_SMALL ||
            Status == STATUS_BUFFER_OVERFLOW)
        {
            VideoPortFreePool(NULL, KeyInfo);
            KeyInfoLength = ResultLength + 16;
            KeyInfo = VideoPortAllocatePool(NULL, VpPagedPool, KeyInfoLength, TAG_VBE);
            if (!KeyInfo)
                return STATUS_INSUFFICIENT_RESOURCES;
            Status = ZwEnumerateKey(DisplayKey,
                                    Index,
                                    GopKeyBasicInformation,
                                    KeyInfo,
                                    KeyInfoLength,
                                    &ResultLength);
        }
        if (!NT_SUCCESS(Status))
            break; /* No more subkeys. */

        if (KeyInfo->NameLength >= sizeof(NameBuffer))
            continue;
        VideoPortMoveMemory(NameBuffer, KeyInfo->Name, KeyInfo->NameLength);
        NameBuffer[KeyInfo->NameLength / sizeof(WCHAR)] = UNICODE_NULL;
        RtlInitUnicodeString(&SubKeyName, NameBuffer);

        InitializeObjectAttributes(&ObjectAttributes,
                                   &SubKeyName,
                                   OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                                   DisplayKey,
                                   NULL);
        Status = ZwOpenKey(&SubKeyHandle, KEY_READ, &ObjectAttributes);
        if (!NT_SUCCESS(Status))
            continue;

        DbgPrint("KUSH-GOP: trying DisplayController child '%ls'\n", NameBuffer);
        Status = GopTryKey(SubKeyHandle,
                           FrameBufferBase,
                           FrameBufferSize,
                           ScreenWidth,
                           ScreenHeight,
                           PixelsPerScanLine,
                           BitsPerPixel,
                           RedMask,
                           GreenMask,
                           BlueMask);
        ZwClose(SubKeyHandle);
        DbgPrint("KUSH-GOP: child '%ls' GopTryKey status 0x%lx\n", NameBuffer, Status);
        if (NT_SUCCESS(Status))
        {
            VideoPortFreePool(NULL, KeyInfo);
            return STATUS_SUCCESS;
        }
        BestStatus = Status;
    }

    VideoPortFreePool(NULL, KeyInfo);
    return BestStatus;
}

/*
 * Recursively search the HARDWARE description tree for a
 * "DisplayController" key and read its GOP framebuffer data.
 */
static NTSTATUS
GopSearchKey(
    _In_ HANDLE ParentKey,
    _In_ ULONG Depth,
    _Out_ PPHYSICAL_ADDRESS FrameBufferBase,
    _Out_ PULONG FrameBufferSize,
    _Out_ PULONG ScreenWidth,
    _Out_ PULONG ScreenHeight,
    _Out_ PULONG PixelsPerScanLine,
    _Out_ PULONG BitsPerPixel,
    _Out_ PULONG RedMask,
    _Out_ PULONG GreenMask,
    _Out_ PULONG BlueMask)
{
    PGOP_KEY_BASIC_INFORMATION KeyInfo = NULL;
    ULONG KeyInfoLength = 256;
    ULONG Index;
    NTSTATUS Status;
    UNICODE_STRING SubKeyName;
    UNICODE_STRING DisplayName;
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE SubKeyHandle;
    WCHAR NameBuffer[64];

    if (Depth > GOP_MAX_DEPTH)
        return STATUS_DEVICE_DOES_NOT_EXIST;

    RtlInitUnicodeString(&DisplayName, L"DisplayController");

    KeyInfo = VideoPortAllocatePool(NULL, VpPagedPool, KeyInfoLength, TAG_VBE);
    if (!KeyInfo)
        return STATUS_INSUFFICIENT_RESOURCES;

    for (Index = 0; ; Index++)
    {
        ULONG ResultLength = 0;

        Status = ZwEnumerateKey(ParentKey,
                                Index,
                                GopKeyBasicInformation,
                                KeyInfo,
                                KeyInfoLength,
                                &ResultLength);
        if (Status == STATUS_BUFFER_TOO_SMALL ||
            Status == STATUS_BUFFER_OVERFLOW)
        {
            VideoPortFreePool(NULL, KeyInfo);
            KeyInfoLength = ResultLength + 16;
            KeyInfo = VideoPortAllocatePool(NULL, VpPagedPool, KeyInfoLength, TAG_VBE);
            if (!KeyInfo)
                return STATUS_INSUFFICIENT_RESOURCES;
            Status = ZwEnumerateKey(ParentKey,
                                    Index,
                                    GopKeyBasicInformation,
                                    KeyInfo,
                                    KeyInfoLength,
                                    &ResultLength);
        }
        if (!NT_SUCCESS(Status))
            break; /* No more subkeys. */

        if (KeyInfo->NameLength >= sizeof(NameBuffer))
            continue;
        VideoPortMoveMemory(NameBuffer, KeyInfo->Name, KeyInfo->NameLength);
        NameBuffer[KeyInfo->NameLength / sizeof(WCHAR)] = UNICODE_NULL;
        RtlInitUnicodeString(&SubKeyName, NameBuffer);
        DbgPrint("KUSH-GOP: search depth %lu, key '%ls'\n", Depth, NameBuffer);

        InitializeObjectAttributes(&ObjectAttributes,
                                   &SubKeyName,
                                   OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                                   ParentKey,
                                   NULL);
        Status = ZwOpenKey(&SubKeyHandle, KEY_READ, &ObjectAttributes);
        if (!NT_SUCCESS(Status))
            continue;

        /* Is this a DisplayController node? */
        if (RtlCompareUnicodeString(&SubKeyName, &DisplayName, TRUE) == 0)
        {
            Status = GopTryDisplayController(SubKeyHandle,
                                             FrameBufferBase,
                                             FrameBufferSize,
                                             ScreenWidth,
                                             ScreenHeight,
                                             PixelsPerScanLine,
                                             BitsPerPixel,
                                             RedMask,
                                             GreenMask,
                                             BlueMask);
            ZwClose(SubKeyHandle);
            if (NT_SUCCESS(Status))
            {
                VideoPortFreePool(NULL, KeyInfo);
                return STATUS_SUCCESS;
            }
            continue;
        }

        /* Otherwise recurse into this subkey. */
        Status = GopSearchKey(SubKeyHandle,
                              Depth + 1,
                              FrameBufferBase,
                              FrameBufferSize,
                              ScreenWidth,
                              ScreenHeight,
                              PixelsPerScanLine,
                              BitsPerPixel,
                              RedMask,
                              GreenMask,
                              BlueMask);
        ZwClose(SubKeyHandle);
        if (NT_SUCCESS(Status))
        {
            VideoPortFreePool(NULL, KeyInfo);
            return STATUS_SUCCESS;
        }
    }

    VideoPortFreePool(NULL, KeyInfo);
    return STATUS_DEVICE_DOES_NOT_EXIST;
}

/*
 * VBETryGopFramebuffer
 *
 * Look for the UEFI GOP framebuffer in the HARDWARE registry tree. If
 * found, set up the device extension and a single linear-framebuffer
 * VBE mode for it and return TRUE. Otherwise return FALSE and the
 * caller falls back to the normal VBE/int10 path.
 */
BOOLEAN
NTAPI
VBETryGopFramebuffer(
   PVBE_DEVICE_EXTENSION DeviceExtension)
{
    UNICODE_STRING KeyPath;
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE RootKey = NULL;
    NTSTATUS Status;
    PHYSICAL_ADDRESS FrameBufferBase;
    ULONG FrameBufferSize;
    ULONG ScreenWidth, ScreenHeight, PixelsPerScanLine, BitsPerPixel;
    ULONG RedMask, GreenMask, BlueMask;
    ULONG BytesPerPixel;
    PVBE_MODEINFO ModeInfo;
    UCHAR RedSize, RedPos, GreenSize, GreenPos, BlueSize, BluePos;

    DbgPrint("KUSH-GOP: VBETryGopFramebuffer entry\n");
    RtlInitUnicodeString(
        &KeyPath,
        L"\\Registry\\Machine\\HARDWARE\\DESCRIPTION\\System");
    InitializeObjectAttributes(&ObjectAttributes,
                               &KeyPath,
                               OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                               NULL,
                               NULL);
    Status = ZwOpenKey(&RootKey, KEY_READ, &ObjectAttributes);
    DbgPrint("KUSH-GOP: ZwOpenKey HARDWARE tree status 0x%lx\n", Status);
    if (!NT_SUCCESS(Status))
    {
        VideoPortDebugPrint(Error, "GOP: cannot open HARDWARE tree (0x%lx)\n", Status);
        return FALSE;
    }

    Status = GopSearchKey(RootKey,
                          0,
                          &FrameBufferBase,
                          &FrameBufferSize,
                          &ScreenWidth,
                          &ScreenHeight,
                          &PixelsPerScanLine,
                          &BitsPerPixel,
                          &RedMask,
                          &GreenMask,
                          &BlueMask);
    ZwClose(RootKey);
    DbgPrint("KUSH-GOP: GopSearchKey status 0x%lx\n", Status);
    if (!NT_SUCCESS(Status))
    {
        VideoPortDebugPrint(Error, "GOP: no framebuffer found in HARDWARE tree\n");
        return FALSE;
    }

    /* Only 32bpp (and 24bpp) linear framebuffers are supported here. */
    if (BitsPerPixel != 32 && BitsPerPixel != 24)
    {
        VideoPortDebugPrint(Error, "GOP: unsupported bpp %lu\n", BitsPerPixel);
        return FALSE;
    }

    BytesPerPixel = (BitsPerPixel + 7) / 8;

    DbgPrint("KUSH-GOP: framebuffer 0x%I64X, %lux%lu, stride %lu px, %lu bpp, size %lu\n",
             FrameBufferBase.QuadPart, ScreenWidth, ScreenHeight,
             PixelsPerScanLine, BitsPerPixel, FrameBufferSize);
    VideoPortDebugPrint(Trace, "GOP: framebuffer 0x%I64X, %lux%lu, stride %lu px, %lu bpp\n",
                        FrameBufferBase.QuadPart, ScreenWidth, ScreenHeight,
                        PixelsPerScanLine, BitsPerPixel);

    /* Allocate the single mode. */
    ModeInfo = VideoPortAllocatePool(DeviceExtension,
                                     VpPagedPool,
                                     sizeof(VBE_MODEINFO),
                                     TAG_VBE);
    if (!ModeInfo)
        return FALSE;
    DeviceExtension->ModeInfo = ModeInfo;
    DeviceExtension->ModeNumbers =
        VideoPortAllocatePool(DeviceExtension,
                              VpPagedPool,
                              sizeof(USHORT),
                              TAG_VBE);
    if (!DeviceExtension->ModeNumbers)
    {
        VideoPortFreePool(DeviceExtension, ModeInfo);
        DeviceExtension->ModeInfo = NULL;
        return FALSE;
    }

    /* Convert the pixel masks to (size, position) pairs. */
    GopMaskToSizePos(RedMask, &RedSize, &RedPos);
    GopMaskToSizePos(GreenMask, &GreenSize, &GreenPos);
    GopMaskToSizePos(BlueMask, &BlueSize, &BluePos);

    /* Fill in the single GOP mode (VBE 3.0 style, linear framebuffer). */
    VideoPortZeroMemory(ModeInfo, sizeof(VBE_MODEINFO));
    ModeInfo->ModeAttributes = VBE_MODEATTR_LINEAR;
    ModeInfo->BytesPerScanLine = (USHORT)(PixelsPerScanLine * BytesPerPixel);
    ModeInfo->XResolution = (USHORT)ScreenWidth;
    ModeInfo->YResolution = (USHORT)ScreenHeight;
    ModeInfo->NumberOfPlanes = 1;
    ModeInfo->BitsPerPixel = (UCHAR)BitsPerPixel;
    ModeInfo->MemoryModel = VBE_MEMORYMODEL_DIRECTCOLOR;
    ModeInfo->RedMaskSize = RedSize;
    ModeInfo->RedFieldPosition = RedPos;
    ModeInfo->GreenMaskSize = GreenSize;
    ModeInfo->GreenFieldPosition = GreenPos;
    ModeInfo->BlueMaskSize = BlueSize;
    ModeInfo->BlueFieldPosition = BluePos;
    ModeInfo->PhysBasePtr = FrameBufferBase.LowPart;
    ModeInfo->LinBytesPerScanLine = (USHORT)(PixelsPerScanLine * BytesPerPixel);
    ModeInfo->LinRedMaskSize = RedSize;
    ModeInfo->LinRedFieldPosition = RedPos;
    ModeInfo->LinGreenMaskSize = GreenSize;
    ModeInfo->LinGreenFieldPosition = GreenPos;
    ModeInfo->LinBlueMaskSize = BlueSize;
    ModeInfo->LinBlueFieldPosition = BluePos;

    DeviceExtension->ModeNumbers[0] = 0;
    DeviceExtension->ModeCount = 1;
    DeviceExtension->CurrentMode = 0;

    /* Report VBE 3.0 so the mode query and mapping use the Lin* fields. */
    DeviceExtension->VbeInfo.Version = 0x300;
    DeviceExtension->VbeInfo.TotalMemory = (USHORT)(FrameBufferSize / (64 * 1024));

    /* Store the full 64-bit framebuffer address (PhysBasePtr is 32-bit). */
    DeviceExtension->GopMode = TRUE;
    DeviceExtension->GopFrameBuffer = FrameBufferBase;
    DeviceExtension->GopFrameBufferSize = FrameBufferSize;
    DbgPrint("KUSH-GOP: GOP mode set up OK, returning TRUE\n");

    return TRUE;
}
