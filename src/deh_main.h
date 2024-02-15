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
// Dehacked entrypoint and common code
//

#ifndef DEH_MAIN_H
#define DEH_MAIN_H

#include "doomtype.h"
#include "deh_str.h"
#include "sha1.h"

#include "d_gameflow.h"
#include "d_mode.h"

// These are the limits that dehacked uses (from dheinit.h in the dehacked
// source).  If these limits are exceeded, it does not generate an error, but
// a warning is displayed.

#define DEH_VANILLA_NUMSFX 107

DOOM_C_API void DEH_ParseCommandLine(void);
DOOM_C_API int DEH_LoadFile(const char *filename);
DOOM_C_API void DEH_AutoLoadPatches(const char *path);
DOOM_C_API int DEH_LoadLump(int lumpnum, doombool allow_long, doombool allow_error);
DOOM_C_API int DEH_LoadLumpByName(const char *name, doombool allow_long, doombool allow_error);
DOOM_C_API GameVersion_t DEH_GetLoadedGameVersion();

DOOM_C_API doombool DEH_ParseAssignment(char *line, char **variable_name, char **value);

DOOM_C_API void DEH_Checksum(sha1_digest_t digest);

DOOM_C_API int32_t DEH_ParTime( mapinfo_t* map );

DOOM_C_API extern doombool deh_apply_cheats;

#endif /* #ifndef DEH_MAIN_H */

