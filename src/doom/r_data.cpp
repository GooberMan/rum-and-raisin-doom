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
//	Preparation of data for rendering,
//	generation of lookups, caching, retrieval by name.
//

#include "m_fixed.h"

#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"

#include "deh_main.h"

#include "m_container.h"

#include "i_system.h"
#include "i_swap.h"
#include "i_terminal.h"

#include "m_misc.h"

#include "p_local.h"
// NEEEEEEED ANIMATION DATA
#include "p_spec.h"

#include "r_main.h"
#include "r_data.h"
#include "r_sky.h"
#include "r_local.h"

#include "w_wad.h"

#include "z_zone.h"

#include <stdio.h>

extern "C"
{

	//
	// Graphics.
	// DOOM graphics for walls and sprites
	// is stored in vertical runs of opaque pixels (posts).
	// A column is composed of zero or more posts,
	// a patch or sprite is composed of zero or more columns.
	// 



	//
	// Texture definition.
	// Each texture is composed of one or more patches,
	// with patches being lumps stored in the WAD.
	// The lumps are referenced by number, and patched
	// into the rectangular texture space using origin
	// and possibly other attributes.
	//
	typedef PACKED_STRUCT (
	{
		short	originx;
		short	originy;
		short	patch;
		short	stepdir;
		short	colormap;
	}) mappatch_t;


	//
	// Texture definition.
	// A DOOM wall texture is a list of patches
	// which are to be combined in a predefined order.
	//
	typedef PACKED_STRUCT (
	{
		char		name[8];
		int			masked;	
		short		width;
		short		height;
		int                 obsolete;
		short		patchcount;
		mappatch_t	patches[1];
	}) maptexture_t;


	// A single patch from a texture definition,
	//  basically a rectangular area within
	//  the texture rectangle.
	typedef struct
	{
		// Block origin (allways UL),
		// which has allready accounted
		// for the internal origin of the patch.
		short	originx;	
		short	originy;
		int		patch;
	} texpatch_t;


	// A maptexturedef_t describes a rectangular texture,
	//  which is composed of one or more mappatch_t structures
	//  that arrange graphic patches.

	typedef struct texture_s texture_t;

	struct texture_s
	{
		// Next in hash table chain
		texture_t  *next;

		// Keep name for switch changing, etc.
		char		name[8];
		int32_t		padding;
		int16_t		width;
		int16_t		height;

		// Index in textures list

		int32_t		index;

		// All the patches[patchcount]
		//  are drawn back to front into the cached texture.
		int32_t		patchcount;
		texpatch_t	patches[1];
	};



	int32_t					firstflat;
	int32_t					lastflat;
	int32_t					numflats;

	int32_t					firstpatch;
	int32_t					lastpatch;
	int32_t					numpatches;

	int32_t					firstspritelump;
	int32_t					lastspritelump;
	int32_t					numspritelumps;
	patch_t**				spritepatches;

	lumpindex_t				firstcolormap = -1;
	lumpindex_t				lastcolormap = -1;
	int32_t					numcolormaps = 0;

	int						numtextures;
	texture_t**				textures;
	texture_t**				textures_hashtable;

	short**					texturecolumnlump;
	uint32_t**				texturecolumnofs;
	texturecomposite_t*		texturecomposite;

	texturecomposite_t*		flatcomposite;

	texturecomposite_t**	texturelookup;
	texturecomposite_t**	flatlookup;

	// for global animation
	int*		flattranslation;
	int*		texturetranslation;

	// needed for pre rendering
	rend_fixed_t*	spritewidth;
	rend_fixed_t*	spriteoffset;
	rend_fixed_t*	spritetopoffset;

	lighttable_t	*colormaps;
}

typedef enum compositetype_e
{
	Composite_Texture,
	Composite_Flat,
} compositetype_t;

typedef struct compositedata_s
{
	texturecomposite_t*		composite;
	compositetype_t			type;
} compositedata_t;

//
// MAPTEXTURE_T CACHING
// When a texture is first needed,
//  it counts the number of composite columns
//  required in the texture and allocates space
//  for a column directory and any new columns.
// The directory will simply point inside other patches
//  if there is only one patch in a given column,
//  but any columns with multiple patches
//  will have new column_ts generated.
//



//
// R_DrawColumnInCache
// Clip and draw a column
//  from a patch into a cached post.
//
void
R_DrawColumnInCache
( column_t*	patch,
  byte*		cache,
  int		originy,
  int		cacheheight )
{
    int		count;
    int		position;
    byte*	source;

    while (patch->topdelta != 0xff)
    {
	source = (byte *)patch + 3;
	count = patch->length;
	position = originy + patch->topdelta;

	if (position < 0)
	{
	    count += position;
	    position = 0;
	}

	if (position + count > cacheheight)
	    count = cacheheight - position;

	if (count > 0)
	    memcpy (cache + position, source, count);
		
	patch = (column_t *)(  (byte *)patch + patch->length + 4); 
    }
}

//
// R_GenerateComposite
// Using the texture definition,
//  the composite texture is created from the patches,
//  and each column is cached.
//

#define COMPOSITE_ZONE ( PU_LEVEL )

void R_GenerateComposite (int texnum)
{
	column_t*			patchcol;
	int32_t				patchy = 0;
	
	texture_t* texture = textures[texnum];

	byte* block = (byte*)Z_Malloc(texturecomposite[ texnum ].size, COMPOSITE_ZONE,  &texturecomposite[texnum].data );	
	memset( block, remove_limits ? 0 : 0xFB, texturecomposite[ texnum ].size );

	// Composite the columns together.
	texpatch_t* patch = texture->patches;
		
	for( int32_t index = 0; index < texture->patchcount; index++, patch++ )
	{
		patch_t* realpatch = (patch_t*)W_CacheLumpNum (patch->patch, COMPOSITE_ZONE); // TODO: Change back to PU_CACHE when masked textures get composited
		int32_t x1 = patch->originx;
		int32_t x2 = M_MIN( x1 + SHORT(realpatch->width), texture->width );

		for ( int32_t x = M_MAX( 0, x1 ); x < x2; x++ )
		{
			patchcol = (column_t *)((byte *)realpatch
						+ LONG(realpatch->columnofs[x-x1]));

			// This little hack accounts for the fact that
			// single-patch textures in vanilla with negative
			// start offsets will actually wrap cromulently
			// with the column renderer. We need to wrap the
			// texture ourselves basically.
			patchy = patch->originy;
			while( patchy < texture->height )
			{
				R_DrawColumnInCache (patchcol,
						 block + x * texturecomposite[ texnum ].pitch,
						 patchy,
						 texture->height);
				if( texture->patchcount > 1 )
				{
					break;
				}
				patchy += realpatch->height;
			}
		}
	}
}

//
// R_GenerateLookup
//
void R_GenerateLookup (int texnum)
{
    texture_t*		texture;
    byte*		patchcount;	// patchcount[texture->width]
    texpatch_t*		patch;	
    patch_t*		realpatch;
    int			x;
    int			x1;
    int			x2;
    int			i;
    short*		collump;
    uint32_t*	colofs;
	
    texture = textures[texnum];

    collump = texturecolumnlump[texnum];
    colofs = texturecolumnofs[texnum];
    
    // Now count the number of columns
    //  that are covered by more than one patch.
    // Fill in the lump / offset, so columns
    //  with only a single patch are all done.
    patchcount = (byte *)Z_Malloc(texture->width, PU_STATIC, &patchcount);
    memset (patchcount, 0, texture->width);
    patch = texture->patches;

    for (i=0 , patch = texture->patches;
	 i<texture->patchcount;
	 i++, patch++)
    {
		realpatch = (patch_t*)W_CacheLumpNum(patch->patch, COMPOSITE_ZONE);
		x1 = patch->originx;
		x2 = x1 + SHORT(realpatch->width);
	
		if (x1 < 0)
			x = 0;
		else
			x = x1;

		if (x2 > texture->width)
			x2 = texture->width;
		for ( ; x<x2 ; x++)
		{
			patchcount[x]++;
			collump[x] = patch->patch;
			colofs[x] = LONG(realpatch->columnofs[x-x1])+3;
		}
	}
	
	for (x=0 ; x<texture->width ; x++)
	{
		if (!patchcount[x])
		{
			I_TerminalPrintf( Log_Warning, "R_GenerateLookup: column without a patch (%s)\n", texture->name );
			break;
		}
	}

	Z_Free(patchcount);
}


void R_CacheCompositeTexture( int32_t tex )
{
	int32_t animstart;
	int32_t animend;

	if ( !texturecomposite[ tex ].data )
	{
		animstart = P_GetPicAnimStart( true, tex );
		if( animstart >= 0 )
		{
			animend = animstart + P_GetPicAnimLength( true, animstart );
			for( ; animstart < animend; ++animstart )
			{
				R_GenerateComposite( animstart );
			}
		}
		else
		{
			R_GenerateComposite( tex );

			animstart = P_GetPicSwitchOpposite( tex );
			if( animstart != -1 )
			{
				R_GenerateComposite( animstart );
			}
		}
	}
}

void R_CacheCompositeFlat( int32_t flat )
{
	int32_t lump = firstflat + flat;

	byte* transposedflatdata = (byte*)Z_Malloc( 64 * 64, COMPOSITE_ZONE, &flatcomposite[ flat ].data );
	byte* originalflatdata = (byte*)W_CacheLumpNum( lump, PU_CACHE );

	byte* currsource = originalflatdata;
	for( int32_t y : iota( 0, 64 ) )
	{
		byte* currdest = transposedflatdata + y;
		for( int32_t x : iota( 0, 64 ) )
		{
			*currdest = *currsource;
			++currsource;
			currdest += 64;
		}
	}
}

texturecomposite_t* R_CacheAndGetCompositeFlat( const char* flat )
{
	int32_t index = R_FlatNumForName( flat );

	if( index >= numflats )
	{
		R_CacheCompositeTexture( index );
	}
	else
	{
		R_CacheCompositeFlat( index );
	}

	return flatlookup[ index ];
}

//
// R_GetColumn
//
byte* R_GetColumn( int32_t tex, int32_t col )
{
	return R_GetColumnComposite( texturelookup[ tex ], col );
}

DOOM_C_API byte* R_GetColumnComposite( texturecomposite_t* composite, int32_t col )
{
	col &= composite->widthmask;
	int32_t ofs = composite->pitch * col;

	if ( !composite->data )
	{
		I_Error( "R_GetColumn: Attempting to create composite during rendering." );
	}

	return composite->data + ofs;
}

thread_local byte flatcolumnhack[ 72 ] =
{
	0, 64, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0xFF, 0, 0, 0
};

byte* R_GetRawColumn( int32_t tex, int32_t col )
{
	int		lump;
	int		ofs;

	if( tex > numtextures )
	{
		// THIS IS SO NASTY. NEED TO CACHE THIS
		memcpy( flatcolumnhack + 3, R_GetColumn( tex, col ), 64 );
		return flatcolumnhack + 3;
	}
	
	col &= texturelookup[ tex ]->widthmask;
	ofs = texturecolumnofs[tex][col];
	lump = texturecolumnlump[tex][col];

	if (lump > 0)
	{
		return (byte *)W_CacheLumpNum(lump,COMPOSITE_ZONE)+ofs;
	}

	return R_GetColumn( tex, col );
}


static void GenerateTextureHashTable(void)
{
    texture_t **rover;
    int i;
    int key;

    textures_hashtable = (texture_t**)Z_Malloc(sizeof(texture_t *) * numtextures, PU_STATIC, 0);

    memset(textures_hashtable, 0, sizeof(texture_t *) * numtextures);

    // Add all textures to hash table

    for (i=0; i<numtextures; ++i)
    {
        // Store index

        textures[i]->index = i;

        // Vanilla Doom does a linear search of the texures array
        // and stops at the first entry it finds.  If there are two
        // entries with the same name, the first one in the array
        // wins. The new entry must therefore be added at the end
        // of the hash chain, so that earlier entries win.

        key = W_LumpNameHash(textures[i]->name) % numtextures;

        rover = &textures_hashtable[key];

        while (*rover != NULL)
        {
            rover = &(*rover)->next;
        }

        // Hook into hash table

        textures[i]->next = NULL;
        *rover = textures[i];
    }
}

void R_InitTextureAndFlatComposites( void )
{
	int32_t totallookup = numtextures + numflats;

	texturecomposite = (texturecomposite_t*)Z_Malloc( numtextures * sizeof( *texturecomposite ), PU_STATIC, 0 );

	texturelookup = (texturecomposite_t**)Z_Malloc( totallookup * sizeof( *texturelookup ), PU_STATIC, 0 );
	flatlookup = (texturecomposite_t**)Z_Malloc( totallookup * sizeof( *flatlookup ), PU_STATIC, 0 );

	flatcomposite = (texturecomposite_t*)Z_Malloc( numflats * sizeof(texturecomposite_t), PU_STATIC, NULL );
	int32_t index = 0;
	texturecomposite_t** texturedest = texturelookup;
	texturecomposite_t** flatdest = flatlookup + numflats;

	for( texture_t* texture : std::span( textures, numtextures ) )
	{
		texturecomposite_t& composite = texturecomposite[ index ];

		// Gotta be an easier way to find current or lower power of 2...
		int32_t mask = 1;
		while( mask * 2 <= texture->width ) mask <<= 1;

		composite.data = NULL;
		memcpy( composite.name, texture->name, sizeof( texture->name ) );
		composite.namepadding = 0;
		composite.size = texture->width * texture->height;
		composite.width = texture->width;
		composite.height = texture->height;
		composite.pitch = texture->height;
		composite.widthmask = mask - 1;
		composite.renderheight = IntToRendFixed( texture->height );
		composite.index = index;

		R_GenerateLookup( index );

		*texturedest++ = *flatdest++ = &composite;
		++index;
	}

	index = 0;
	texturedest = texturelookup + numtextures;
	flatdest = flatlookup;

	for( texturecomposite_t& composite : std::span( flatcomposite, numflats ) )
	{
		int32_t lump = firstflat + index;
		composite.data = NULL;
		strncpy( composite.name, W_GetNameForNum( lump ), 8 );
		composite.namepadding = 0;
		composite.size = 64 * 64;
		composite.width = 64;
		composite.height = 64;
		composite.pitch = 64;
		composite.widthmask = 63;
		composite.renderheight = IntToRendFixed( 64 );
		composite.index = index;

		*texturedest++ = *flatdest++ = &composite;
		++index;
	}

	// Create translation table for global animation.
	texturetranslation = (int*)Z_Malloc ( ( totallookup + 1 ) * sizeof(*texturetranslation), PU_STATIC, 0);
	for( int32_t i : iota( 0, totallookup ) )
	{
		texturetranslation[i] = i;
	}

	flattranslation = (int*)Z_Malloc ( ( totallookup + 1 ) * sizeof(*flattranslation), PU_STATIC, 0);
	for( int32_t i : iota( 0, totallookup ) )
	{
		flattranslation[i] = i;
	}

}

//
// R_InitTextures
// Initializes the texture list
//  with the textures from the world map.
//
void R_InitTextures (void)
{
    maptexture_t*	mtexture;
    texture_t*		texture;
    mappatch_t*		mpatch;
    texpatch_t*		patch;

    int			i;
    int			j;

    int32_t*	maptex;
    int32_t*	maptex2;
    int32_t*	maptex1;
    
    char		name[9];
    char*		names;
    char*		name_p;
    
    int32_t*	patchlookup;
    
    int			nummappatches;
    int			offset;
    int			maxoff;
    int			maxoff2;
    int			numtextures1;
    int			numtextures2;

    int*		directory;
    
    int			temp1;
    int			temp2;
    int			temp3;

    
    // Load the patch names from pnames.lmp.
    name[8] = 0;
    names = (char*)W_CacheLumpName(DEH_String("PNAMES"), PU_STATIC);
    nummappatches = LONG ( *((int *)names) );
    name_p = names + 4;
    patchlookup = (int32_t*)Z_Malloc(nummappatches*sizeof(*patchlookup), PU_STATIC, NULL);

    for (i = 0; i < nummappatches; i++)
    {
        M_StringCopy(name, name_p + i * 8, sizeof(name));
        patchlookup[i] = W_CheckNumForName(name);
    }
    W_ReleaseLumpName(DEH_String("PNAMES"));

    // Load the map texture definitions from textures.lmp.
    // The data is contained in one or two lumps,
    //  TEXTURE1 for shareware, plus TEXTURE2 for commercial.
    maptex = maptex1 = (int32_t*)W_CacheLumpName(DEH_String("TEXTURE1"), PU_STATIC);
    numtextures1 = LONG(*maptex);
    maxoff = W_LumpLength (W_GetNumForName (DEH_String("TEXTURE1")));
    directory = maptex+1;
	
    if (W_CheckNumForName (DEH_String("TEXTURE2")) != -1)
    {
	maptex2 = (int32_t*)W_CacheLumpName(DEH_String("TEXTURE2"), PU_STATIC);
	numtextures2 = LONG(*maptex2);
	maxoff2 = W_LumpLength (W_GetNumForName (DEH_String("TEXTURE2")));
    }
    else
    {
	maptex2 = NULL;
	numtextures2 = 0;
	maxoff2 = 0;
    }
    numtextures = numtextures1 + numtextures2;
	
    textures = (texture_t**)Z_Malloc(numtextures * sizeof(*textures), PU_STATIC, 0);
    texturecolumnlump = (short**)Z_Malloc (numtextures * sizeof(*texturecolumnlump), PU_STATIC, 0);
    texturecolumnofs = (uint32_t**)Z_Malloc (numtextures * sizeof(*texturecolumnofs), PU_STATIC, 0);

    //	Really complex printing shit...
    temp1 = W_GetNumForName (DEH_String("S_START"));  // P_???????
    temp2 = W_GetNumForName (DEH_String("S_END")) - 1;
    temp3 = ((temp2-temp1+63)/64) + ((numtextures+63)/64);

	constexpr int32_t ExtraDots = 5;
	DoomString output;
	output.reserve( temp3 + ExtraDots + 2 );

	output.append( "[" );
	for (i = 0; i < temp3 + ExtraDots; i++)
	{
		output.append( " ");
	}
	output.append( "]");
	I_TerminalPrintf( Log_None, output.c_str() );
	output.clear();
	output.reserve( temp3 + ExtraDots + 1 );
	for (i = 0; i < temp3 + ExtraDots + 1; i++)
	{
		output.append( "\b");
	}
	I_TerminalPrintf( Log_None, output.c_str() );
	
    for (i=0 ; i<numtextures ; i++, directory++)
    {
		if (!(i&63))
			I_TerminalPrintf( Log_None, "." );

		if (i == numtextures1)
		{
			// Start looking in second texture file.
			maptex = maptex2;
			maxoff = maxoff2;
			directory = maptex+1;
		}
		
		offset = LONG(*directory);

		if (offset > maxoff)
			I_Error ("R_InitTextures: bad texture directory");
	
		mtexture = (maptexture_t *) ( (byte *)maptex + offset);

		texture = textures[i] = (texture_t*)Z_Malloc(sizeof(texture_t) + sizeof(texpatch_t)*(SHORT(mtexture->patchcount)-1), PU_STATIC, 0);
	
		texture->width = SHORT(mtexture->width);
		texture->height = SHORT(mtexture->height);
		texture->patchcount = SHORT(mtexture->patchcount);
	
		memcpy (texture->name, mtexture->name, sizeof(texture->name));
		texture->padding = 0;
		mpatch = &mtexture->patches[0];
		patch = &texture->patches[0];

		for (j=0 ; j<texture->patchcount ; j++, mpatch++, patch++)
		{
			patch->originx = SHORT(mpatch->originx);
			patch->originy = SHORT(mpatch->originy);
			patch->patch = patchlookup[SHORT(mpatch->patch)];
			if (patch->patch == -1)
			{
			I_Error ("R_InitTextures: Missing patch in texture %s",
				 texture->name);
			}
		}		
		texturecolumnlump[i] = (short*)Z_Malloc (texture->width*sizeof(**texturecolumnlump), PU_STATIC,0);
		texturecolumnofs[i] = (uint32_t*)Z_Malloc (texture->width*sizeof(**texturecolumnofs), PU_STATIC,0);
    }

	Z_Free(patchlookup);

	W_ReleaseLumpName(DEH_String("TEXTURE1"));
	if (maptex2)
	{
		W_ReleaseLumpName(DEH_String("TEXTURE2"));
	}

	GenerateTextureHashTable();
}



//
// R_InitFlats
//
void R_InitFlats (void)
{
	firstflat = W_GetNumForName (DEH_String("F_START")) + 1;
	lastflat = W_GetNumForName (DEH_String("F_END")) - 1;
	numflats = lastflat - firstflat + 1;
}


//
// R_InitSpriteLumps
// Finds the width and hoffset of all sprites in the wad,
//  so the sprite does not need to be cached completely
//  just for having the header info ready during rendering.
//
void R_InitSpriteLumps (void)
{
    int		i;
    patch_t	*patch;
	
    firstspritelump = W_GetNumForName (DEH_String("S_START")) + 1;
    lastspritelump = W_GetNumForName (DEH_String("S_END")) - 1;
    
    numspritelumps = lastspritelump - firstspritelump + 1;
    spritewidth = (rend_fixed_t*)Z_Malloc (numspritelumps*sizeof(*spritewidth), PU_STATIC, 0);
    spriteoffset = (rend_fixed_t*)Z_Malloc (numspritelumps*sizeof(*spriteoffset), PU_STATIC, 0);
    spritetopoffset = (rend_fixed_t*)Z_Malloc (numspritelumps*sizeof(*spritetopoffset), PU_STATIC, 0);
	spritepatches = (patch_t**)Z_Malloc(numspritelumps * sizeof(*spritepatches), PU_STATIC, 0 );
	
    for (i=0 ; i< numspritelumps ; i++)
    {
		if (!(i&63))
			I_TerminalPrintf( Log_None, "." );

		patch = (patch_t*)W_CacheLumpNum (firstspritelump+i, PU_STATIC);
		spritewidth[i] = IntToRendFixed( SHORT(patch->width) );
		spriteoffset[i] = IntToRendFixed( SHORT(patch->leftoffset) );
		spritetopoffset[i] = IntToRendFixed( SHORT(patch->topoffset) );

		spritepatches[i] = patch;
    }
}



//
// R_InitColormaps
//
void R_InitColormaps (void)
{
    int	lump;

    // Load in the light tables, 
    //  256 byte align tables.
    lump = W_GetNumForName(DEH_String("COLORMAP"));
    colormaps = (lighttable_t*)W_CacheLumpNum(lump, PU_STATIC);

	if( remove_limits )
	{
		firstcolormap = W_CheckNumForName( "C_START" ) + 1;
		lastcolormap = W_CheckNumForName( "C_END" ) - 1;
		numcolormaps = M_MAX( 0, lastcolormap - firstcolormap + 1 );
	}
}



//
// R_InitData
// Locates all the lumps
//  that will be used by all views
// Must be called after W_Init.
//
void R_InitData (void)
{
    R_InitTextures ();
    //I_TerminalPrintf( Log_None, "." );
    R_InitFlats ();
    //I_TerminalPrintf( Log_None, "." );
	R_InitTextureAndFlatComposites();
    //I_TerminalPrintf( Log_None, "." );
    R_InitSpriteLumps ();
    //I_TerminalPrintf( Log_None, "." );
    R_InitColormaps ();
}



//
// R_FlatNumForName
// Retrieval, get a flat number for a flat name.
//
int R_FlatNumForName(const char *name)
{
	int32_t index = W_CheckNumForName (name);

	if( !remove_limits )
	{
		if( index != -1 )
		{
			return index - firstflat;
		}

		I_Error( "R_FlatNumForName: %s not found", name );
		return -1;
	}

	if( index != -1 )
	{
		if( index >= firstflat && index <= lastflat )
		{
			return index - firstflat;
		}
		else if( W_LumpLength( index ) == 4096 )
		{
			I_LogAddEntryVar( Log_Warning, "Flat %.8s found outside of markers, code does not handle this yet", name );
			// TODO: Handle bad flat numbers, although this only happens with -merge...
			return 0;
		}
	}

	index = R_CheckTextureNumForName (name);

	if( index == -1 )
	{
		I_Error ("R_FlatNumForName: %s not found in textures", name );
	}

	return index + numflats;
}




//
// R_CheckTextureNumForName
// Check whether texture is available.
// Filter out NoTexture indicator.
//
int R_CheckTextureNumForName(const char *name)
{
    texture_t *texture;
    int key;

    // "NoTexture" marker.
    if( name[0] == '-' )
	{
		return 0;
	}

	if( remove_limits && name[ 0 ] == 0 )
	{
		return 0;
	}
		
	key = W_LumpNameHash( name ) % numtextures;

	texture = textures_hashtable[ key ];

	while( texture != NULL )
	{
		if( !strncasecmp( texture->name, name, 8 ) )
		{
			return texture->index;
		}

		texture = texture->next;
	}

	return -1;
}

const char* R_TextureNameForNum( int32_t tex )
{
	return textures[ tex ]->name;
}


//
// R_TextureNumForName
// Calls R_CheckTextureNumForName,
//  aborts with error message.
//
int R_TextureNumForName(const char *name)
{
    int		i;
	
    i = R_CheckTextureNumForName (name);

	if( i != -1 )
	{
		return i;
	}

	if( !remove_limits )
	{
		I_Error ("R_TextureNumForName: %s not found", name );
		return -1;
	}

	i = W_CheckNumForName( name );

	if( i == -1 || i < firstflat || i > lastflat )
	{
		I_LogAddEntryVar( Log_Warning, "R_TextureNumForName: %s not found in textures or flats, replacing with %s", name, textures[ 0 ]->name );
		return 0;
	}

	return numtextures + i - firstflat;
}

DOOM_C_API lighttable_t* R_GetColormapForNum( lumpindex_t colormapnum )
{
	if( colormapnum >= firstcolormap && colormapnum <= lastcolormap )
	{
		return (lighttable_t*)W_CacheLumpNum( colormapnum, PU_LEVEL );
	}

	return nullptr;
}


//
// R_PrecacheLevel
// Preloads all relevant graphics for the level.
//

void R_PrecacheLevel (void)
{
    char*		flatpresent;
    char*		texturepresent;
    //char*		spritepresent;

    int			i;
    int			j;
    int			lump;

    texture_t*		texture;

	flatpresent = (char*)Z_Malloc(numflats, PU_STATIC, NULL);
	memset (flatpresent,0,numflats);
	texturepresent = (char*)Z_Malloc(numtextures, PU_STATIC, NULL);
	memset (texturepresent,0, numtextures);

	// Check flats and textures for presence, jamming in to appropriate array as necessary
	auto MarkFlatPresence = [ &texturepresent, &flatpresent ]( int16_t flatnum )
	{
		if( flatnum < numflats )
		{
			flatpresent[ flatnum ] = 1;
			int32_t animstart = P_GetPicAnimStart( false, flatnum );
			if( animstart >= 0 )
			{
				int32_t animend = animstart + P_GetPicAnimLength( false, animstart );
				for( int32_t thisanim : iota( animstart, animend ) )
				{
					flatpresent[ thisanim ] = 1;
				}
			}
		}
		else
		{
			int32_t texnum = flatnum - numflats;
			texturepresent[ texnum ] = 1;
			int32_t animstart = P_GetPicAnimStart( true, texnum );
			if( animstart >= 0 )
			{
				int32_t animend = animstart + P_GetPicAnimLength( true, animstart );
				for( int32_t thisanim : iota( animstart, animend ) )
				{
					texturepresent[ thisanim ] = 1;
				}
			}
		}
	};

	for (i=0 ; i<numsectors ; i++)
	{
		MarkFlatPresence( sectors[ i ].floorpic );
		MarkFlatPresence( sectors[ i ].ceilingpic );
	}

	auto MarkTexturePresence = [ &texturepresent, &flatpresent ]( int16_t texnum )
	{
		if( texnum < numtextures )
		{
			texturepresent[ texnum ] = 1;
			int32_t animstart = P_GetPicAnimStart( true, texnum );
			if( animstart >= 0 )
			{
				int32_t animend = animstart + P_GetPicAnimLength( true, animstart );
				for( int32_t thisanim : iota( animstart, animend ) )
				{
					texturepresent[ thisanim ] = 1;
				}
			}
		}
		else
		{
			int32_t flatnum = texnum - numtextures;
			flatpresent[ texnum - numtextures ] = 1;
			int32_t animstart = P_GetPicAnimStart( false, flatnum );
			if( animstart >= 0 )
			{
				int32_t animend = animstart + P_GetPicAnimLength( false, animstart );
				for( int32_t thisanim : iota( animstart, animend ) )
				{
					flatpresent[ thisanim ] = 1;
				}
			}
		}
	};

	for (i=0 ; i<numsides ; i++)
	{
		MarkTexturePresence( sides[ i ].toptexture );
		MarkTexturePresence( sides[ i ].midtexture );
		MarkTexturePresence( sides[ i ].bottomtexture );
	}

	// Sky texture is always present.
	// Note that F_SKY1 is the name used to
	//  indicate a sky floor/ceiling as a flat,
	//  while the sky texture is stored like
	//  a wall texture, with an episode dependend
	//  name.
	texturepresent[skytexture] = 1;
	
	// Precache flats.
	for (i=0 ; i<numflats ; i++)
	{
		flatcomposite[ i ].data = NULL;
		lump = firstflat + i;

		if (flatpresent[i])
		{
			R_CacheCompositeFlat( i );
		}
	}

	// Precache textures.
	for (i=0 ; i<numtextures ; i++)
	{
		if (!texturepresent[i])
			continue;

		texture = textures[i];
	
		for (j=0 ; j<texture->patchcount ; j++)
		{
			lump = texture->patches[j].patch;
			W_CacheLumpNum( lump , PU_LEVEL ); // TODO: Switch to cache after masked textures get a SIMD renderer
		}

		R_CacheCompositeTexture( i );
	}

	Z_Free(flatpresent);
	Z_Free(texturepresent);

/*
	// Precache sprites.
	spritepresent = Z_Malloc(numsprites, PU_STATIC, NULL);
	memset (spritepresent,0, numsprites);
	
	for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
	{
		if (th->function.acp1 == (actionf_p1)P_MobjThinker)
			spritepresent[((mobj_t *)th)->sprite] = 1;
	}
	
	spritememory = 0;
	for (i=0 ; i<numsprites ; i++)
	{
		if (!spritepresent[i])
			continue;

		for (j=0 ; j<sprites[i].numframes ; j++)
		{
			sf = &sprites[i].spriteframes[j];
			for (k=0 ; k<8 ; k++)
			{
				lump = firstspritelump + sf->lump[k];
				spritememory += lumpinfo[lump]->size;
				W_CacheLumpNum(lump , PU_STATIC);
			}
		}
	}

	Z_Free(spritepresent);
*/
}




