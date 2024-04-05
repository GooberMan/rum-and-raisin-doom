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

DOOM_C_API typedef struct lumpinfo_s
{
    wad_file_t *wad_file;
    void		*cache;

    char		name[8];
	int32_t		namepadding;
    int32_t		position;
    int32_t		size;
	wadtype_t	type;

    // Used for hash table lookups
    lumpindex_t next;
} lumpinfo_t;

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

DOOM_C_API doombool W_HasAnyLumps();
DOOM_C_API wad_file_t *W_AddFile(const char *filename);
DOOM_C_API wad_file_t *W_AddFileWithType(const char *filename, wadtype_t type);
DOOM_C_API void W_Reload(void);
DOOM_C_API void W_RemoveFile( wad_file_t* file );

DOOM_C_API lumpindex_t W_CheckNumForName(const char *name);
DOOM_C_API lumpindex_t W_CheckNumForNameExcluding(const char *name, wadtype_t exclude);
DOOM_C_API lumpindex_t W_GetNumForName(const char *name);
DOOM_C_API lumpindex_t W_GetNumForNameExcluding(const char *name, wadtype_t exclude);
DOOM_C_API const char* W_GetNameForNum( lumpindex_t num );

DOOM_C_API int W_LumpLength(lumpindex_t lump);
DOOM_C_API void W_ReadLump(lumpindex_t lump, void *dest);

DOOM_C_API void *W_CacheLumpNumTracked(const char* file, size_t line, lumpindex_t lump, int tag);
DOOM_C_API void *W_CacheLumpNameTracked(const char* file, size_t line, const char *name, int tag);

#define W_CacheLumpNum(lump, tag) W_CacheLumpNumTracked( __FILE__, __LINE__, lump, tag )
#define W_CacheLumpName(lump, name) W_CacheLumpNameTracked( __FILE__, __LINE__, lump, name )

DOOM_C_API void W_GenerateHashTable(void);

DOOM_C_API extern unsigned int W_LumpNameHash(const char *s);

DOOM_C_API void W_ReleaseLumpNum(lumpindex_t lump);
DOOM_C_API void W_ReleaseLumpName(const char *name);

DOOM_C_API const char *W_WadNameForLump(const lumpinfo_t *lump);
DOOM_C_API const char *W_WadNameForLumpNum( lumpindex_t lump );
DOOM_C_API const char *W_WadNameForLumpName( const char* name );

DOOM_C_API const char *W_WadPathForLumpNum( lumpindex_t lump );
DOOM_C_API const char *W_WadPathForLumpName( const char* name );

DOOM_C_API doombool W_IsIWADLump(const lumpinfo_t *lump);

#if defined( __cplusplus )

const std::vector< wad_file_t* >& W_GetLoadedWADFiles();

INLINE auto W_LumpIndicesFor( wad_file_t* file )
{
	struct lumpindexrange
	{
		struct iterator
		{
			wad_file_t*		wad_file;
			uint32_t		lump_index;

			INLINE int32_t operator*() noexcept				{ return lump_index; }
			INLINE bool operator!=( const iterator& rhs ) 	{ return lump_index != rhs.lump_index; }
			INLINE iterator& operator++()
			{
				Increment();
				return *this;
			}

		private:
			INLINE void Increment()
			{
				while( lump_index < numlumps )
				{
					++lump_index;
					if( lump_index < numlumps && lumpinfo[ lump_index ]->wad_file == wad_file )
					{
						break;
					}
				}
			}
		};

		lumpindexrange( wad_file_t* source_file )
			: _begin { source_file, 0 }
			, _end { source_file, numlumps }
		{
			if( lumpinfo[ 0 ]->wad_file != source_file )
			{
				++_begin;
			}
		}

		constexpr iterator begin()	{ return _begin; }
		constexpr iterator end()	{ return _end; }

	private:
		iterator _begin;
		iterator _end;
	};

	return lumpindexrange( file );
}

#endif // defined( __cplusplus )

#endif
