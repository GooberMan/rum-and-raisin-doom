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
//	WAD I/O functions.
//


#ifndef __W_WAD__
#define __W_WAD__

#include <stdio.h>

#include "doomtype.h"
#include "w_file.h"


//
// TYPES
//

//
// WADFILE I/O related stuff.
//

DOOM_C_API typedef struct lumpinfo_s lumpinfo_t;
DOOM_C_API typedef int32_t lumpindex_t;

DOOM_C_API struct lumpinfo_s
{
    wad_file_t *wad_file;
    void		*cache;

    char		name[8];
	int32_t		namepadding;
    int32_t		position;
    int32_t		size;

    // Used for hash table lookups
    lumpindex_t next;
};

DOOM_C_API typedef PACKED_STRUCT (
wadinfo_e {
	// Should be "IWAD" or "PWAD".
	char		identification[4];
	int			numlumps;
	int			infotableofs;
} ) wadinfo_t;


DOOM_C_API typedef PACKED_STRUCT (
filelump_e {
	int			filepos;
	int			size;
	char		name[8];
} ) filelump_t;

DOOM_C_API extern lumpinfo_t **lumpinfo;
DOOM_C_API extern uint32_t numlumps;

DOOM_C_API extern doombool wadrenderlock;

DOOM_C_API wad_file_t *W_AddFile(const char *filename);
DOOM_C_API void W_Reload(void);

DOOM_C_API lumpindex_t W_CheckNumForName(const char *name);
DOOM_C_API lumpindex_t W_GetNumForName(const char *name);
DOOM_C_API const char* W_GetNameForNum( lumpindex_t num );

DOOM_C_API int W_LumpLength(lumpindex_t lump);
DOOM_C_API void W_ReadLump(lumpindex_t lump, void *dest);

DOOM_C_API void *W_CacheLumpNum(lumpindex_t lump, int tag);
DOOM_C_API void *W_CacheLumpName(const char *name, int tag);

DOOM_C_API void W_GenerateHashTable(void);

DOOM_C_API extern unsigned int W_LumpNameHash(const char *s);

DOOM_C_API void W_ReleaseLumpNum(lumpindex_t lump);
DOOM_C_API void W_ReleaseLumpName(const char *name);

DOOM_C_API const char *W_WadNameForLump(const lumpinfo_t *lump);
DOOM_C_API const char *W_WadNameForLumpNum( lumpindex_t lump );
DOOM_C_API const char *W_WadNameForLumpName( const char* name );

DOOM_C_API doombool W_IsIWADLump(const lumpinfo_t *lump);

#endif
