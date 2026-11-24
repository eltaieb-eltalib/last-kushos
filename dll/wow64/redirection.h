/*
 * Wow64 filesystem and registry redirection
 *
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         wow64.dll
 * FILE:            dll/wow64/redirection.h
 * PROGRAMMER:      Marcin Jabłoński
 */

#pragma once

/* Kush 1.1: the SysWOW64 directory is no longer hardcoded (it was
 * "C:\\ReactOS\\SysWOW64"): it is derived from %SystemRoot% at run time, so
 * WOW64 works on the Live CD (X:\\reactos) and on any install path. */

typedef struct _WOW64_PATH_REDIRECTION
{
    UNICODE_STRING From;
    UNICODE_STRING To;
} WOW64_PATH_REDIRECTION, *PWOW64_PATH_REDIRECTION;
 
BOOLEAN 
RedirectPath(const WOW64_PATH_REDIRECTION* Redirection, 
             POBJECT_ATTRIBUTES ObjectAttributes);
 
BOOLEAN 
GetFileRedirect(OBJECT_ATTRIBUTES* attr);