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
// Dehacked I/O code (does all reads from dehacked files)
//

#ifndef DEH_IO_H
#define DEH_IO_H

#include "deh_defs.h"

DOOM_C_API deh_context_t *DEH_OpenFile(const char *filename);
DOOM_C_API deh_context_t *DEH_OpenLump(int lumpnum);
DOOM_C_API void DEH_CloseFile(deh_context_t *context);
DOOM_C_API int DEH_GetChar(deh_context_t *context);
DOOM_C_API char *DEH_ReadLine(deh_context_t *context, doombool extended);
DOOM_C_API void DEH_Error(deh_context_t *context, const char *msg, ...) PRINTF_ATTR(2, 3);
DOOM_C_API void DEH_Warning(deh_context_t *context, const char *msg, ...) PRINTF_ATTR(2, 3);
DOOM_C_API doombool DEH_HadError(deh_context_t *context);

#endif /* #ifndef DEH_IO_H */

