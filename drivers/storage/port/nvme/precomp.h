/**
 *******************************************************************************
 ** Copyright (c) 2011-2012                                                   **
 **                                                                           **
 **   Integrated Device Technology, Inc.                                      **
 **   Intel Corporation                                                       **
 **   LSI Corporation                                                         **
 **                                                                           **
 ** All rights reserved.                                                      **
 **                                                                           **
 *******************************************************************************
 **                                                                           **
 ** Redistribution and use in source and binary forms, with or without        **
 ** modification, are permitted provided that the following conditions are    **
 ** met:                                                                      **
 **                                                                           **
 **   1. Redistributions of source code must retain the above copyright       **
 **      notice, this list of conditions and the following disclaimer.        **
 **                                                                           **
 **   2. Redistributions in binary form must reproduce the above copyright    **
 **      notice, this list of conditions and the following disclaimer in the  **
 **      documentation and/or other materials provided with the distribution. **
 **                                                                           **
 ** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS   **
 ** IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, **
 ** THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR    **
 ** PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR         **
 ** CONTRIBUTORS BE LIABLE FOR ANY DIRECT,INDIRECT, INCIDENTAL, SPECIAL,      **
 ** EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,       **
 ** PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR        **
 ** PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    **
 ** LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      **
 ** NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        **
 ** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              **
 **                                                                           **
 ** The views and conclusions contained in the software and documentation     **
 ** are those of the authors and should not be interpreted as representing    **
 ** official policies, either expressed or implied, of Intel Corporation,     **
 ** Integrated Device Technology Inc., or Sandforce Corporation.              **
 **                                                                           **
 *******************************************************************************
**/

/*
 * File: precomp.h
 */

/* Kush 1.1 / ReactOS: the OFA NVMe driver targets the Windows 8 StorPort DDK. */
#undef NTDDI_VERSION
#undef _WIN32_WINNT
#undef WINVER
#define NTDDI_VERSION 0x06020000
#define _WIN32_WINNT 0x0602
#define WINVER 0x0602
#include "ntddk.h"
#include "ntddscsi.h"
#include "ntdddisk.h"
#include "stdlib.h"
#include <guiddef.h>
#include <storport.h>
/* Kush 1.1 / ReactOS: storport.h already defines the SRB types; keep scsiwmi.h
 * from pulling in srb.h again (conflicting definitions). */
#ifndef _NTSRB_
#define _NTSRB_
#endif
/* Function role types used by the MS DDK (SAL); not in the ReactOS headers. */
typedef ULONG  (NTAPI HW_FIND_ADAPTER_ROLE)(PVOID, PVOID, PVOID, PCHAR, PPORT_CONFIGURATION_INFORMATION, PBOOLEAN);
typedef BOOLEAN (NTAPI HW_INITIALIZE)(PVOID);
typedef BOOLEAN (NTAPI HW_BUILDIO)(PVOID, PSCSI_REQUEST_BLOCK);
typedef BOOLEAN (NTAPI HW_STARTIO)(PVOID, PSCSI_REQUEST_BLOCK);
typedef BOOLEAN (NTAPI HW_INTERRUPT)(PVOID);
typedef BOOLEAN (NTAPI HW_RESET_BUS)(PVOID, ULONG);
typedef SCSI_ADAPTER_CONTROL_STATUS (NTAPI HW_ADAPTER_CONTROL)(PVOID, SCSI_ADAPTER_CONTROL_TYPE, PVOID);
typedef BOOLEAN (NTAPI HW_PASSIVE_INITIALIZE_ROUTINE)(PVOID);
typedef VOID (NTAPI HW_TIMER)(PVOID);
typedef VOID (NTAPI HW_DPC_ROUTINE)(PSTOR_DPC, PVOID, PVOID, PVOID);
typedef BOOLEAN (NTAPI HW_MESSAGE_SIGNALED_INTERRUPT_ROUTINE)(PVOID, ULONG);
typedef DRIVER_INITIALIZE sp_DRIVER_INITIALIZE;
typedef VOID (NTAPI HW_TIMER_EX)(PVOID, PVOID);
#ifndef SP_INTERNAL_ADAPTER_ERROR
#define SP_INTERNAL_ADAPTER_ERROR 0x0006
#endif
#ifndef SCSIPORT_API
#define SCSIPORT_API
#endif
#if (NTDDI_VERSION > NTDDI_WIN7)
#include <srbhelper.h>
#endif
#include <scsiwmi.h>
#include <initguid.h>
#include <ata.h>
#include "nvmeMofData.h"
#include "nvme.h"
#include "nvmeReg.h"
#include "nvmeIoctl.h"
#include "nvmeStd.h"
#include "nvmePwrMgmt.h"
#include "nvmeSnti.h"
#include "nvmeSntiUtils.h"
#include "nvmeSntiTypes.h"
#include "nvmeIo.h"
#include "nvmeWmi.h"

#include "nvme_tracing.h"
/* Kush 1.1: see kush_compat.c */
VOID NTAPI KushNvmeQuerySystemTime(PLARGE_INTEGER CurrentTime);
#define StorPortQuerySystemTime(t) KushNvmeQuerySystemTime(t)


