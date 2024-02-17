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
//	Handles WAD file header, directory, lump I/O.
//




#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"

#include "i_swap.h"
#include "i_system.h"
#include "i_terminal.h"
#include "i_video.h"
#include "m_container.h"
#include "m_misc.h"
#include "v_diskicon.h"
#include "z_zone.h"

#include "w_wad.h"

//
// GLOBALS
//
extern "C"
{
	// Location of each lump on disk.
	lumpinfo_t **lumpinfo;
	uint32_t numlumps = 0;
	doombool wadrenderlock = false;

	extern int32_t remove_limits;
}

template< typename _type >
requires std::is_enum_v< _type >
constexpr auto operator|( _type lhs, _type rhs )
{
	using underlying = std::underlying_type_t< _type >;
	return (_type)( (underlying)lhs | (underlying)rhs );
}

template< typename _type >
requires std::is_enum_v< _type >
constexpr auto operator&( _type lhs, _type rhs )
{
	using underlying = std::underlying_type_t< _type >;
	return (_type)( (underlying)lhs & (underlying)rhs );
}


std::vector< wad_file_t* > wadfiles;

const std::vector< wad_file_t* >& W_GetLoadedWADFiles()
{
	return wadfiles;
}

// Hash table for fast lookups
static lumpindex_t *lumphash;

// Variables for the reload hack: filename of the PWAD to reload, and the
// lumps from WADs before the reload file, so we can resent numlumps and
// load the file again.
static wad_file_t *reloadhandle = NULL;
static lumpinfo_t *reloadlumps = NULL;
static char *reloadname = NULL;
static int reloadlump = -1;

// Hash function used for lump names.
unsigned int W_LumpNameHash(const char *s)
{
    // This is the djb2 string hash function, modded to work on strings
    // that have a maximum length of 8.

    unsigned int result = 5381;
    unsigned int i;

    for (i=0; i < 8 && s[i] != '\0'; ++i)
    {
        result = ((result << 5) ^ result ) ^ toupper(s[i]);
    }

    return result;
}

//
// LUMP BASED ROUTINES.
//

DOOM_C_API doombool W_HasAnyLumps()
{
	return numlumps > 0;
}

//
// W_AddFile
// All files are optional, but at least one file must be
//  found (PWAD, if all required lumps are present).
// Files with a .wad extension are wadlink files
//  with multiple lumps.
// Other files are single lumps with the base filename
//  for the lump name.

wad_file_t *W_AddFileWithType(const char *filename, wadtype_t type)
{
    wadinfo_t header;
    lumpindex_t i;
    wad_file_t *wad_file;
    int32_t length;
    lumpindex_t startlump;
    filelump_t *fileinfo;
    filelump_t *filerover;
    lumpinfo_t *filelumps;
    int32_t numfilelumps;

    // If the filename begins with a ~, it indicates that we should use the
    // reload hack.
    if (filename[0] == '~')
    {
        if (reloadname != NULL)
        {
            I_Error("Prefixing a WAD filename with '~' indicates that the "
                    "WAD should be reloaded\n"
                    "on each level restart, for use by level authors for "
                    "rapid development. You\n"
                    "can only reload one WAD file, and it must be the last "
                    "file in the -file list.");
        }

        reloadname = strdup(filename);
        reloadlump = numlumps;
        ++filename;
    }

    // Open the file and add to directory
    wad_file = W_OpenFile(filename);

    if (wad_file == NULL)
    {
		I_TerminalPrintf( Log_Warning, " couldn't open %s\n", filename);
		return NULL;
    }

	wadtype_t foundtype = wt_none;

	int32_t iswad = strcasecmp( filename + M_MAX( strlen( filename ) - 3, 0 ), "wad" );
	int32_t iswidepix = strcasecmp(filename + M_MAX( strlen( filename ) - 7, 0 ), "widepix" );
    if ( iswad != 0	&& iswidepix != 0 )
    {
		// single lump file

		// fraggle: Swap the filepos and size here.  The WAD directory
		// parsing code expects a little-endian directory, so will swap
		// them back.  Effectively we're constructing a "fake WAD directory"
		// here, as it would appear on disk.

		fileinfo = (filelump_t*)Z_Malloc(sizeof(filelump_t), PU_STATIC, 0);
		fileinfo->filepos = LONG(0);
		fileinfo->size = LONG(wad_file->length);

		// Name the lump after the base of the filename (without the
		// extension).

		M_ExtractFileBase (filename, fileinfo->name);
		numfilelumps = 1;
		foundtype = type | wt_singlelump;
    }
    else
    {
		// WAD file
		W_Read(wad_file, 0, &header, sizeof(header));
		foundtype = wt_iwad;
		if (strncmp(header.identification,"IWAD",4))
		{
			foundtype = wt_pwad;
			// Homebrew levels?
			if (strncmp(header.identification,"PWAD",4))
			{
			W_CloseFile(wad_file);
			I_Error ("Wad file %s doesn't have IWAD "
				 "or PWAD id\n", filename);
			}

			// ???modifiedgame = true;
		}

		header.numlumps = LONG(header.numlumps);

		// Vanilla Doom doesn't like WADs with more than 4046 lumps
		// https://www.doomworld.com/vb/post/1010985
		if (!remove_limits && !strncmp(header.identification,"PWAD",4) && header.numlumps > 4046)
		{
				W_CloseFile(wad_file);
				I_Error ("Error: Vanilla limit for lumps in a WAD is 4046, "
						"PWAD %s has %d", filename, header.numlumps);
		}

		header.infotableofs = LONG(header.infotableofs);
		length = header.numlumps*sizeof(filelump_t);
		fileinfo = (filelump_t*)Z_Malloc(length, PU_STATIC, 0);

		foundtype = ( type | foundtype );
		wad_file->type = foundtype;

		W_Read(wad_file, header.infotableofs, fileinfo, length);
		numfilelumps = header.numlumps;
    }

    // Increase size of numlumps array to accomodate the new file.
    filelumps = (lumpinfo_t*)calloc(numfilelumps, sizeof(lumpinfo_t));
    if (filelumps == NULL)
    {
        W_CloseFile(wad_file);
        I_Error("Failed to allocate array for lumps from new file.");
    }

    startlump = numlumps;
    numlumps += numfilelumps;
    lumpinfo = (lumpinfo_t**)I_Realloc(lumpinfo, numlumps * sizeof(lumpinfo_t *));
    filerover = fileinfo;

    for (i = startlump; i < numlumps; ++i)
    {
        lumpinfo_t *lump_p = &filelumps[i - startlump];
        lump_p->wad_file = wad_file;
        lump_p->position = LONG(filerover->filepos);
        lump_p->size = LONG(filerover->size);
		lump_p->type = foundtype;
        lump_p->cache = NULL;
        strncpy(lump_p->name, filerover->name, 8);
		lump_p->namepadding = 0;
        lumpinfo[i] = lump_p;

        ++filerover;
    }

    Z_Free(fileinfo);

    if (lumphash != NULL)
    {
        Z_Free(lumphash);
        lumphash = NULL;
    }

    // If this is the reload file, we need to save some details about the
    // file so that we can close it later on when we do a reload.
    if (reloadname)
    {
        reloadhandle = wad_file;
        reloadlumps = filelumps;
    }

	wadfiles.push_back( wad_file );
    return wad_file;
}

DOOM_C_API wad_file_t *W_AddFile(const char *filename)
{
	return W_AddFileWithType(filename, wt_none);
}

//
// W_NumLumps
//
int W_NumLumps (void)
{
    return numlumps;
}



//
// W_CheckNumForName
// Returns -1 if name not found.
//

DOOM_C_API lumpindex_t W_CheckNumForNameExcluding(const char *name, wadtype_t exclude)
{
    lumpindex_t i;

    // Do we have a hash table yet?

    if (lumphash != NULL)
    {
        int hash;

        // We do! Excellent.

        hash = W_LumpNameHash(name) % numlumps;

        for (i = lumphash[hash]; i != -1; i = lumpinfo[i]->next)
        {
			bool excludelump = exclude != wt_none && ( lumpinfo[i]->type & exclude ) != 0;
            if ( !excludelump && !strncasecmp(lumpinfo[i]->name, name, 8) )
            {
                return i;
            }
        }
    }
    else
    {
        // We don't have a hash table generate yet. Linear search :-(
        //
        // scan backwards so patch lump files take precedence

        for (i = numlumps - 1; i >= 0; --i)
        {
            if (!strncasecmp(lumpinfo[i]->name, name, 8) && ( lumpinfo[i]->type ^ exclude ) == lumpinfo[i]->type )
            {
                return i;
            }
        }
    }

    // TFB. Not found.

    return -1;
}

DOOM_C_API lumpindex_t W_CheckNumForName(const char *name)
{
	return W_CheckNumForNameExcluding( name, wt_none );
}

//
// W_GetNumForName
// Calls W_CheckNumForName, but bombs out if not found.
//
lumpindex_t W_GetNumForNameExcluding(const char *name, wadtype_t exclude)
{
    lumpindex_t i;

    i = W_CheckNumForNameExcluding(name, exclude);

    if (i < 0)
    {
        I_Error ("W_GetNumForName: %s not found!", name);
    }
 
    return i;
}

lumpindex_t W_GetNumForName(const char *name)
{
	return W_GetNumForNameExcluding( name, wt_none );
}

const char* W_GetNameForNum( lumpindex_t num )
{
	return lumpinfo[ num ]->name;
}

//
// W_LumpLength
// Returns the buffer size needed to load the given lump.
//
int W_LumpLength(lumpindex_t lump)
{
    if (lump >= numlumps)
    {
	I_Error ("W_LumpLength: %i >= numlumps", lump);
    }

    return lumpinfo[lump]->size;
}



//
// W_ReadLump
// Loads the lump into the given buffer,
//  which must be >= W_LumpLength().
//
void W_ReadLump(lumpindex_t lump, void *dest)
{
    int c;
    lumpinfo_t *l;

    if (lump >= numlumps)
    {
        I_Error ("W_ReadLump: %i >= numlumps", lump);
    }

    l = lumpinfo[lump];

    V_BeginRead(l->size);

    c = W_Read(l->wad_file, l->position, dest, l->size);

    if (c < l->size)
    {
        I_Error("W_ReadLump: only read %i of %i on lump %i",
                c, l->size, lump);
    }
}




//
// W_CacheLumpNum
//
// Load a lump into memory and return a pointer to a buffer containing
// the lump data.
//
// 'tag' is the type of zone memory buffer to allocate for the lump
// (usually PU_STATIC or PU_CACHE).  If the lump is loaded as 
// PU_STATIC, it should be released back using W_ReleaseLumpNum
// when no longer needed (do not use Z_ChangeTag).
//

void *W_CacheLumpNumTracked(const char* file, size_t line, lumpindex_t lumpnum, int tag)
{
    byte *result;
    lumpinfo_t *lump;
	doombool changed;

	if ((unsigned)lumpnum >= numlumps)
	{
		I_Error ("W_CacheLumpNum: %i >= numlumps", lumpnum);
	}

    lump = lumpinfo[lumpnum];

    // Get the pointer to return.  If the lump is in a memory-mapped
    // file, we can just return a pointer to within the memory-mapped
    // region.  If the lump is in an ordinary file, we may already
    // have it cached; otherwise, load it into memory.

    if (lump->wad_file->mapped != NULL)
    {
        // Memory mapped file, return from the mmapped region.

        result = lump->wad_file->mapped + lump->position;
    }
    else if (lump->cache != NULL)
    {
        // Already cached, so just switch the zone tag.

        result = (byte*)lump->cache;
        changed = Z_ChangeTag(lump->cache, tag);

		if( changed && wadrenderlock )
		{
			I_Error ( "W_CacheLumpNum: %i changed zone during render", lumpnum );
		}
    }
    else
    {
        // Not yet loaded, so load it now
		if( wadrenderlock )
		{
			I_Error ( "W_CacheLumpNum: %i requested during render", lumpnum );
		}

		lump->cache = Z_MallocTracked(file, line, W_LumpLength(lumpnum), tag, &lump->cache, NULL);
		W_ReadLump (lumpnum, lump->cache);
		result = (byte*)lump->cache;
	}
	
    return result;
}



//
// W_CacheLumpName
//
void *W_CacheLumpNameTracked(const char* file, size_t line, const char *name, int tag)
{
    return W_CacheLumpNumTracked( file, line, W_GetNumForName(name), tag );
}

// 
// Release a lump back to the cache, so that it can be reused later 
// without having to read from disk again, or alternatively, discarded
// if we run out of memory.
//
// Back in Vanilla Doom, this was just done using Z_ChangeTag 
// directly, but now that we have WAD mmap, things are a bit more
// complicated ...
//

void W_ReleaseLumpNum(lumpindex_t lumpnum)
{
    lumpinfo_t *lump;

    if ((unsigned)lumpnum >= numlumps)
    {
	I_Error ("W_ReleaseLumpNum: %i >= numlumps", lumpnum);
    }

    lump = lumpinfo[lumpnum];

    if (lump->wad_file->mapped != NULL)
    {
        // Memory-mapped file, so nothing needs to be done here.
    }
    else
    {
        Z_ChangeTag(lump->cache, PU_CACHE);
    }
}

void W_ReleaseLumpName(const char *name)
{
    W_ReleaseLumpNum(W_GetNumForName(name));
}

#if 0

//
// W_Profile
//
int		info[2500][10];
int		profilecount;

void W_Profile (void)
{
    int		i;
    memblock_t*	block;
    void*	ptr;
    char	ch;
    FILE*	f;
    int		j;
    char	name[9];
	
	
    for (i=0 ; i<numlumps ; i++)
    {	
	ptr = lumpinfo[i].cache;
	if (!ptr)
	{
	    ch = ' ';
	    continue;
	}
	else
	{
	    block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
	    if (block->tag < PU_PURGELEVEL)
		ch = 'S';
	    else
		ch = 'P';
	}
	info[i][profilecount] = ch;
    }
    profilecount++;
	
    f = fopen ("waddump.txt","w");
    name[8] = 0;

    for (i=0 ; i<numlumps ; i++)
    {
	memcpy (name,lumpinfo[i].name,8);

	for (j=0 ; j<8 ; j++)
	    if (!name[j])
		break;

	for ( ; j<8 ; j++)
	    name[j] = ' ';

	fprintf (f,"%s ",name);

	for (j=0 ; j<profilecount ; j++)
	    fprintf (f,"    %c",info[i][j]);

	fprintf (f,"\n");
    }
    fclose (f);
}


#endif

// Generate a hash table for fast lookups

void W_GenerateHashTable(void)
{
    lumpindex_t i;

    // Free the old hash table, if there is one:
    if (lumphash != NULL)
    {
        Z_Free(lumphash);
    }

    // Generate hash table
    if (numlumps > 0)
    {
        lumphash = (lumpindex_t*)Z_Malloc(sizeof(lumpindex_t) * numlumps, PU_STATIC, NULL);

        for (i = 0; i < numlumps; ++i)
        {
            lumphash[i] = -1;
        }

        for (i = 0; i < numlumps; ++i)
        {
            unsigned int hash;

            hash = W_LumpNameHash(lumpinfo[i]->name) % numlumps;

            // Hook into the hash table

            lumpinfo[i]->next = lumphash[hash];
            lumphash[hash] = i;
        }
    }

    // All done!
}

// The Doom reload hack. The idea here is that if you give a WAD file to -file
// prefixed with the ~ hack, that WAD file will be reloaded each time a new
// level is loaded. This lets you use a level editor in parallel and make
// incremental changes to the level you're working on without having to restart
// the game after every change.
// But: the reload feature is a fragile hack...
void W_Reload(void)
{
    char *filename;
    lumpindex_t i;

    if (reloadname == NULL)
    {
        return;
    }

    // We must free any lumps being cached from the PWAD we're about to reload:
    for (i = reloadlump; i < numlumps; ++i)
    {
        if (lumpinfo[i]->cache != NULL)
        {
            Z_Free(lumpinfo[i]->cache);
        }
    }

    // Reset numlumps to remove the reload WAD file:
    numlumps = reloadlump;

    // Now reload the WAD file.
    filename = reloadname;

    W_CloseFile(reloadhandle);
    free(reloadlumps);

    reloadname = NULL;
    reloadlump = -1;
    reloadhandle = NULL;
    W_AddFile(filename);
    free(filename);

    // The WAD directory has changed, so we have to regenerate the
    // fast lookup hashtable:
    W_GenerateHashTable();
}

const char *W_WadNameForLump(const lumpinfo_t *lump)
{
	return M_BaseName(lump->wad_file->path);
}

const char *W_WadNameForLumpNum( lumpindex_t lump )
{
	return M_BaseName( lumpinfo[ lump ]->wad_file->path );
}

const char *W_WadNameForLumpName( const char* name )
{
	return M_BaseName( lumpinfo[ W_GetNumForName( name ) ]->wad_file->path );
}

const char *W_WadPathForLumpNum( lumpindex_t lump )
{
	return lumpinfo[ lump ]->wad_file->path;
}

const char *W_WadPathForLumpName( const char* name )
{
	return lumpinfo[ W_GetNumForName( name ) ]->wad_file->path;
}

doombool W_IsIWADLump(const lumpinfo_t *lump)
{
	return ( lump->type & wt_iwad ) == wt_iwad;
}
