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
//	Simple basic typedefs, isolated here to make it easier
//	 separating modules.
//    


#ifndef __DOOMTYPE__
#define __DOOMTYPE__

#include "config.h"

#if defined(_MSC_VER) && !defined(__cplusplus) && !defined( inline )
#define inline __inline
#endif

#define PLATFORM_ARCH_NONE          0
#define PLATFORM_ARCH_x86           1
#define PLATFORM_ARCH_x64           2
#define PLATFORM_ARCH_ARM32         3
#define PLATFORM_ARCH_ARM64         4

#define PLATFORM_ARCHNAME_NONE      "<invalid architecture>"
#define PLATFORM_ARCHNAME_x86       "x86"
#define PLATFORM_ARCHNAME_x64       "x64"
#define PLATFORM_ARCHNAME_ARM32     "ARM 32-bit"
#define PLATFORM_ARCHNAME_ARM64     "ARM 64-bit"

#if defined(_M_IX86) || defined(__i386) || defined(__i386__)
#define PLATFORM_ARCH               PLATFORM_ARCH_x86
#define PLATFORM_ARCHNAME           PLATFORM_ARCHNAME_x86
#elif defined(_M_X64) || defined(__x86_64) || defined(__x86_64__) || defined(__amd64__)
#define PLATFORM_ARCH               PLATFORM_ARCH_x64
#define PLATFORM_ARCHNAME           PLATFORM_ARCHNAME_x64
#elif ( defined(M_ARM) && !defined(M_ARM64) ) || ( defined(__arm__) && !defined(__aarch64__) )
#define PLATFORM_ARCH               PLATFORM_ARCH_ARM32
#define PLATFORM_ARCHNAME           PLATFORM_ARCHNAME_ARM32
#elif defined(M_ARM64) || defined(__aarch64__)
#define PLATFORM_ARCH               PLATFORM_ARCH_ARM64
#define PLATFORM_ARCHNAME           PLATFORM_ARCHNAME_ARM64
#endif // architecture check

#ifdef __cplusplus
#define DOOM_C_API extern "C"
#else
#define DOOM_C_API
#endif // C++ check

#if defined( _MSC_VER )
#define INLINE __forceinline
#else
#define INLINE inline
#endif

#if !defined( NDEBUG )
#define EDITION_STRING PACKAGE_STRING " " PLATFORM_ARCHNAME " THIS IS A DEBUG BUILD STOP PROFILING ON A DEBUG BUILD"
#elif defined( RNR_PROFILING )
#define EDITION_STRING PACKAGE_STRING " " PLATFORM_ARCHNAME " PROFILING BUILD (See about window for explanation)"
#else
#define EDITION_STRING PACKAGE_STRING " " PLATFORM_ARCHNAME
#endif // !defined( NDEBUG )

// #define macros to provide functions missing in Windows.
// Outside Windows, we use strings.h for str[n]casecmp.


#if !HAVE_DECL_STRCASECMP || !HAVE_DECL_STRNCASECMP

#include <string.h>
#if !HAVE_DECL_STRCASECMP
#define strcasecmp stricmp
#endif
#if !HAVE_DECL_STRNCASECMP
#define strncasecmp strnicmp
#endif

#else

#include <strings.h>

#endif


//
// The packed attribute forces structures to be packed into the minimum 
// space necessary.  If this is not done, the compiler may align structure
// fields differently to optimize memory access, inflating the overall
// structure size.  It is important to use the packed attribute on certain
// structures where alignment is important, particularly data read/written
// to disk.
//

#ifdef __GNUC__

#if defined(_WIN32) && !defined(__clang__)
#define PACKEDATTR __attribute__((packed,gcc_struct))
#else
#define PACKEDATTR __attribute__((packed))
#endif

#define PRINTF_ATTR(fmt, first) __attribute__((format(printf, fmt, first)))
#define PRINTF_ARG_ATTR(x) __attribute__((format_arg(x)))
#define NORETURN __attribute__((noreturn))

#else
#if defined(_MSC_VER)
#define PACKEDATTR __pragma(pack(pop))
#else
#define PACKEDATTR
#endif
#define PRINTF_ATTR(fmt, first)
#define PRINTF_ARG_ATTR(x)
#define NORETURN
#endif

#ifdef __WATCOMC__
#define PACKEDPREFIX _Packed
#elif defined(_MSC_VER)
#define PACKEDPREFIX __pragma(pack(push,1))
#else
#define PACKEDPREFIX
#endif

#define PACKED_STRUCT(...) PACKEDPREFIX struct __VA_ARGS__ PACKEDATTR

// C99 integer types; with gcc we just use this.  Other compilers
// should add conditional statements that define the C99 types.

// What is really wanted here is stdint.h; however, some old versions
// of Solaris don't have stdint.h and only have inttypes.h (the 
// pre-standardisation version).  inttypes.h is also in the C99 
// standard and defined to include stdint.h, so include this. 

#include <inttypes.h>
#include <math.h> // for float_t and double_t
#include <stddef.h>

#if defined(__cplusplus) || defined(__bool_true_false_are_defined)

// Using the built-in type for boolean in C++ causes size mismatches in C code.
// Need to go whole hog on the C++ conversion to deal with this effectively.

DOOM_C_API typedef int32_t boolean;

#else

// [R&R] TODO: Switching to stdbool.h breaks clang.
// Some structs store booleans. This could be why.
// Investigate further.
DOOM_C_API typedef enum boolean_s
{
    false, 
    true
} boolean_t;

DOOM_C_API typedef int32_t boolean;

//#include <stdbool.h>
//typedef bool boolean;

#endif

DOOM_C_API typedef uint8_t byte;
DOOM_C_API typedef uint8_t pixel_t;
DOOM_C_API typedef int16_t dpixel_t;

DOOM_C_API typedef struct rgb_s
{
	pixel_t r;
	pixel_t g;
	pixel_t b;
} rgb_t;

DOOM_C_API typedef struct rgba_s
{
	pixel_t r;
	pixel_t g;
	pixel_t b;
	pixel_t a;
} rgba_t;

#include <limits.h>

#ifdef _WIN32

#define DIR_SEPARATOR '\\'
#define DIR_SEPARATOR_S "\\"
#define PATH_SEPARATOR ';'

#else

#define DIR_SEPARATOR '/'
#define DIR_SEPARATOR_S "/"
#define PATH_SEPARATOR ':'

#endif

#define arrlen(array) (sizeof(array) / sizeof(*array))

#endif

