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

typedef struct lumpinfo_s lumpinfo_t;
typedef int32_t lumpindex_t;

struct lumpinfo_s
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


extern lumpinfo_t **lumpinfo;
extern uint32_t numlumps;

extern boolean wadrenderlock;

wad_file_t *W_AddFile(const char *filename);
void W_Reload(void);

lumpindex_t W_CheckNumForName(const char *name);
lumpindex_t W_GetNumForName(const char *name);
const char* W_GetNameForNum( lumpindex_t num );

int W_LumpLength(lumpindex_t lump);
void W_ReadLump(lumpindex_t lump, void *dest);

void *W_CacheLumpNum(lumpindex_t lump, int tag);
void *W_CacheLumpName(const char *name, int tag);

void W_GenerateHashTable(void);

extern unsigned int W_LumpNameHash(const char *s);

void W_ReleaseLumpNum(lumpindex_t lump);
void W_ReleaseLumpName(const char *name);

const char *W_WadNameForLump(const lumpinfo_t *lump);
boolean W_IsIWADLump(const lumpinfo_t *lump);

#endif
