//
// Copyright(C) 2005-2014 Simon Howard
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
//
// Dehacked string replacements
//

#ifndef DEH_STR_H
#define DEH_STR_H

#include <stdio.h>

#include "doomtype.h"

// Used to do dehacked text substitutions throughout the program

DOOM_C_API const char *DEH_String(const char *s) PRINTF_ARG_ATTR(1);
DOOM_C_API void DEH_printf(const char *fmt, ...) PRINTF_ATTR(1, 2);
DOOM_C_API void DEH_fprintf(FILE *fstream, const char *fmt, ...) PRINTF_ATTR(2, 3);
DOOM_C_API void DEH_snprintf(char *buffer, size_t len, const char *fmt, ...) PRINTF_ATTR(3, 4);
DOOM_C_API void DEH_AddStringReplacement(const char *from_text, const char *to_text);

DOOM_C_API const char* DEH_StringLookupMnemonic( const char* key );

#define DEH_StringMnemonic( s ) DEH_String( DEH_StringLookupMnemonic( s ) );

#if 0
// Static macro versions of the functions above

#define DEH_String(x) (x)
#define DEH_printf printf
#define DEH_fprintf fprintf
#define DEH_snprintf snprintf

#endif

#endif /* #ifndef DEH_STR_H */

