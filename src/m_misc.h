//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2020 Ethan Watson
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//      Miscellaneous.
//    


#ifndef __M_MISC__
#define __M_MISC__

#include <stdio.h>
#include <stdarg.h>

#include "doomtype.h"

// The ternary actually optimises pretty well on modern architectures, by an order of magnitude compared to bit twiddling operations.
#define M_MIN( x, y ) ( x < y ? x : y )
#define M_MAX( x, y ) ( x > y ? x : y )
#define M_CLAMP( val, minval, maxval ) M_MAX( minval, M_MIN( maxval, val ) )

#define M_BITMASK( numbits ) ( ( 1 << numbits ) - 1 )
#define M_BITMASK64( numbits ) ( 1ll << numbits ) - 1 )
#define M_NEGATE( unsignedval ) ( ~unsignedval + 1 )

DOOM_C_API doombool M_WriteFile(const char *name, const void *source, int length);
DOOM_C_API int M_ReadFile(const char *name, byte **buffer);
DOOM_C_API void M_MakeDirectory(const char *dir);
DOOM_C_API char *M_TempFile(const char *s);
DOOM_C_API doombool M_FileExists(const char *file);
DOOM_C_API char *M_FileCaseExists(const char *file);
DOOM_C_API long M_FileLength(FILE *handle);
DOOM_C_API doombool M_StrToInt(const char *str, int *result);
DOOM_C_API char *M_DirName(const char *path);
DOOM_C_API const char *M_BaseName(const char *path);
DOOM_C_API void M_ExtractFileBase(const char *path, char *dest);
DOOM_C_API void M_ForceUppercase(char *text);
DOOM_C_API void M_ForceLowercase(char *text);
DOOM_C_API const char *M_StrCaseStr(const char *haystack, const char *needle);
DOOM_C_API char *M_StringDuplicate(const char *orig);
DOOM_C_API doombool M_StringCopy(char *dest, const char *src, size_t dest_size);
DOOM_C_API doombool M_StringConcat(char *dest, const char *src, size_t dest_size);
DOOM_C_API char *M_StringReplace(const char *haystack, const char *needle, const char *replacement);
DOOM_C_API char *M_StringJoin(const char *s, ...);
DOOM_C_API doombool M_StringStartsWith(const char *s, const char *prefix);
DOOM_C_API doombool M_StringEndsWith(const char *s, const char *suffix);
DOOM_C_API int M_vsnprintf(char *buf, size_t buf_len, const char *s, va_list args);
DOOM_C_API int M_snprintf(char *buf, size_t buf_len, const char *s, ...) PRINTF_ATTR(3, 4);
DOOM_C_API char *M_OEMToUTF8(const char *ansi);

#endif

