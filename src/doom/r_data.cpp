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

#include "m_container.h"

#include "i_terminal.h"

#include "r_main.h"
#include "r_data.h"
#include "r_sky.h"

extern "C"
{
	#include <stdio.h>

	#include "deh_main.h"
	#include "i_swap.h"
	#include "i_system.h"
	#include "z_zone.h"

	#include "w_wad.h"

	#include "m_misc.h"
	#include "r_local.h"
	#include "p_local.h"

	// NEEEEEEED ANIMATION DATA
	#include "p_spec.h"


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



	int		firstflat;
	int		lastflat;
	int		numflats;

	int		firstpatch;
	int		lastpatch;
	int		numpatches;

	int32_t		firstspritelump;
	int32_t		lastspritelump;
	int32_t		numspritelumps;
	patch_t**	spritepatches;


	int				numtextures;
	texture_t**		textures;
	texture_t**		textures_hashtable;


	int*					texturewidthmask;
	// needed for texture pegging
	fixed_t*				textureheight;
	rend_fixed_t*			rendtextureheight;
	short**					texturecolumnlump;
	uint32_t**				texturecolumnofs;
	uint32_t**				texturecompositecolumnofs;
	texturecomposite_t*		texturecomposite;

	// for global animation
	int*		flattranslation;
	int*		texturetranslation;

	// needed for pre rendering
	fixed_t*	spritewidth;	
	fixed_t*	spriteoffset;
	fixed_t*	spritetopoffset;

	lighttable_t	*colormaps;
}

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

#define COMPOSITE_MULTIPLIER ( NUMCOLORMAPS )
#define COMPOSITE_ZONE ( PU_LEVEL )

#define COMPOSITE_HEIGHTSTEP ( 128 )
#define COMPOSITE_HEIGHTMASK ( COMPOSITE_HEIGHTSTEP - 1 )
#define COMPOSITE_HEIGHTALIGN( x ) ( ( x  + COMPOSITE_HEIGHTMASK ) & ~COMPOSITE_HEIGHTMASK )

void R_GenerateComposite (int texnum)
{
	byte*				block;
	byte*				source;
	byte*				target;
	texture_t*			texture;
	texpatch_t*			patch;	
	patch_t*			realpatch;
	lighttable_t*		currmap;
	int32_t				x;
	int32_t				x1;
	int32_t				x2;
	int32_t				index;
	column_t*			patchcol;
	int16_t*			collump;
	uint32_t*			colofs;
	int32_t				patchy = 0;
	
	texture = textures[texnum];

	block = (byte*)Z_Malloc(texturecomposite[ texnum ].size * COMPOSITE_MULTIPLIER, COMPOSITE_ZONE,  &texturecomposite[texnum].data );	
	memset( block, remove_limits ? 0 : 0xFB, texturecomposite[ texnum ].size * COMPOSITE_MULTIPLIER );

    collump = texturecolumnlump[texnum];
    colofs = texturecompositecolumnofs[texnum];
    
    // Composite the columns together.
    patch = texture->patches;
		
    for (index=0; index<texture->patchcount; index++, patch++)
    {
		realpatch = (patch_t*)W_CacheLumpNum (patch->patch, COMPOSITE_ZONE); // TODO: Change back to PU_CACHE when masked textures get composited
		x1 = patch->originx;
		x2 = x1 + SHORT(realpatch->width);

		if (x1<0)
			x = 0;
		else
			x = x1;
	
		if (x2 > texture->width)
			x2 = texture->width;

		for ( ; x<x2 ; x++)
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
						 block + colofs[x],
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

	for( index = 1; index < NUMCOLORMAPS; ++index )
	{
		currmap = colormaps + index * 256;

		source = block;
		target = block + texturecomposite[ texnum ].size * index;

		while( source < block + texturecomposite[ texnum ].size )
		{
			*target++ = currmap[ *source++ ];
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

    // Composited texture not created yet.
    texturecomposite[ texnum ].data = NULL;
    texturecomposite[ texnum ].size = 0;

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
	
	colofs = texturecompositecolumnofs[texnum];

	for (x=0 ; x<texture->width ; x++)
	{
		if (!patchcount[x])
		{
			I_TerminalPrintf( Log_Warning, "R_GenerateLookup: column without a patch (%s)\n",
				texture->name);
			return;
		}
	
		// Pre-caching textures breaks on TNT and PLUTONIA with a 64KB limit. Them sky textures are chonkers.

		// TODO: Remove colofs and just use a pitch value on the texture...
		colofs[x] = texturecomposite[ texnum ].size;
		texturecomposite[ texnum ].size += COMPOSITE_HEIGHTALIGN( texture->height );
    }

    Z_Free(patchcount);
}


void R_CacheComposite( int32_t tex )
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

//
// R_GetColumn
//
byte* R_GetColumn( int32_t tex, int32_t col, int32_t colormapindex )
{
	int		ofs;
	
	col &= texturewidthmask[tex];
	ofs = texturecomposite[ tex ].size * colormapindex + texturecompositecolumnofs[tex][col];

	if ( !texturecomposite[tex].data )
	{
		R_GenerateComposite (tex);
	}

	return texturecomposite[tex].data + ofs;
}

byte* R_GetRawColumn( int32_t tex, int32_t col )
{
	int		lump;
	int		ofs;
	
	col &= texturewidthmask[ tex];
	ofs = texturecolumnofs[tex][col];
	lump = texturecolumnlump[tex][col];
    
	if (lump > 0)
	{
		return (byte *)W_CacheLumpNum(lump,COMPOSITE_ZONE)+ofs;
	}

	return R_GetColumn( tex, col, 0 );
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
    
    int			totalwidth;
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
    texturecomposite = (texturecomposite_t*)Z_Malloc (numtextures * sizeof(*texturecomposite), PU_STATIC, 0);
	texturecompositecolumnofs = (uint32_t**)Z_Malloc (numtextures * sizeof(*texturecompositecolumnofs), PU_STATIC, 0);
    texturewidthmask = (int32_t*)Z_Malloc (numtextures * sizeof(*texturewidthmask), PU_STATIC, 0);
    textureheight = (fixed_t*)Z_Malloc (numtextures * sizeof(*textureheight), PU_STATIC, 0);
	rendtextureheight = (rend_fixed_t*)Z_Malloc( numtextures * sizeof( *rendtextureheight ), PU_STATIC, 0 );

    totalwidth = 0;
    
    //	Really complex printing shit...
    temp1 = W_GetNumForName (DEH_String("S_START"));  // P_???????
    temp2 = W_GetNumForName (DEH_String("S_END")) - 1;
    temp3 = ((temp2-temp1+63)/64) + ((numtextures+63)/64);

	DoomString output;
	output.reserve( temp3 + 8 );

	output.append( "[" );
	for (i = 0; i < temp3 + 6; i++)
	{
		output.append( " ");
	}
	output.append( "]");
	I_TerminalPrintf( Log_None, output.c_str() );
	output.clear();
	output.reserve( temp3 + 7 );
	for (i = 0; i < temp3 + 7; i++)
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

		texturecomposite[ i ].data = NULL;
		memcpy( texturecomposite[ i ].name, mtexture->name, sizeof( texture->name ) );
		texturecomposite[ i ].namepadding = 0;
		texturecomposite[ i ].size = 0;
		texturecomposite[ i ].width = texture->width;
		texturecomposite[ i ].height = texture->height;
		texturecomposite[ i ].pitch = texture->height;

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
		texturecompositecolumnofs[i] = (uint32_t*)Z_Malloc (texture->width*sizeof(**texturecompositecolumnofs), PU_STATIC,0);

		j = 1;
		while (j*2 <= texture->width)
			j<<=1;

		texturewidthmask[i] = j-1;
		textureheight[i] = IntToFixed( texture->height );
		rendtextureheight[ i ] = IntToRendFixed( texture->height );
		
		totalwidth += texture->width;
    }

    Z_Free(patchlookup);

    W_ReleaseLumpName(DEH_String("TEXTURE1"));
    if (maptex2)
        W_ReleaseLumpName(DEH_String("TEXTURE2"));
    
    // Precalculate whatever possible.	

    for (i=0 ; i<numtextures ; i++)
	{
		R_GenerateLookup (i);
	}
    
    // Create translation table for global animation.
    texturetranslation = (int*)Z_Malloc ((numtextures+1)*sizeof(*texturetranslation), PU_STATIC, 0);
    
    for (i=0 ; i<numtextures ; i++)
	texturetranslation[i] = i;

    GenerateTextureHashTable();
}



//
// R_InitFlats
//
void R_InitFlats (void)
{
	int		i;
	
	firstflat = W_GetNumForName (DEH_String("F_START")) + 1;
	lastflat = W_GetNumForName (DEH_String("F_END")) - 1;
	numflats = lastflat - firstflat + 1;
	
	// Create translation table for global animation.
	flattranslation = (int*)Z_Malloc ((numflats+1)*sizeof(*flattranslation), PU_STATIC, 0);

	for (i=0 ; i<numflats ; i++)
	{
		flattranslation[i] = i;
	}

	// This needs to be static, and allocated elsewhere
	precachedflats = (texturecomposite_t*)Z_Malloc( numflats * sizeof(texturecomposite_t), PU_STATIC, NULL );

	for (i=0 ; i<numflats ; i++)
	{
		int32_t lump = firstflat + i;
		precachedflats[ i ].data = NULL;
		strncpy( precachedflats[ i ].name, W_GetNameForNum( lump ), 8 );
		precachedflats[ i ].namepadding = 0;
		precachedflats[ i ].size = 64 * 64;
		precachedflats[ i ].width = 64;
		precachedflats[ i ].height = 64;
		precachedflats[ i ].pitch = 64;
	}
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
    spritewidth = (fixed_t*)Z_Malloc (numspritelumps*sizeof(*spritewidth), PU_STATIC, 0);
    spriteoffset = (fixed_t*)Z_Malloc (numspritelumps*sizeof(*spriteoffset), PU_STATIC, 0);
    spritetopoffset = (fixed_t*)Z_Malloc (numspritelumps*sizeof(*spritetopoffset), PU_STATIC, 0);
	spritepatches = (patch_t**)Z_Malloc(numspritelumps * sizeof(*spritepatches), PU_STATIC, 0 );
	
    for (i=0 ; i< numspritelumps ; i++)
    {
		if (!(i&63))
			I_TerminalPrintf( Log_None, "." );

		patch = (patch_t*)W_CacheLumpNum (firstspritelump+i, PU_STATIC);
		spritewidth[i] = IntToFixed( SHORT(patch->width) );
		spriteoffset[i] = IntToFixed( SHORT(patch->leftoffset) );
		spritetopoffset[i] = IntToFixed( SHORT(patch->topoffset) );

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
    printf (".");
    R_InitFlats ();
    printf (".");
    R_InitSpriteLumps ();
    printf (".");
    R_InitColormaps ();
}



//
// R_FlatNumForName
// Retrieval, get a flat number for a flat name.
//
int R_FlatNumForName(const char *name)
{
    int		i;
    char	namet[9];

    i = W_CheckNumForName (name);

    if (i == -1)
    {
	namet[8] = 0;
	memcpy (namet, name,8);
	I_Error ("R_FlatNumForName: %s not found",namet);
    }
    return i - firstflat;
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
    if (name[0] == '-')		
	return 0;
		
    key = W_LumpNameHash(name) % numtextures;

    texture=textures_hashtable[key]; 
    
    while (texture != NULL)
    {
	if (!strncasecmp (texture->name, name, 8) )
	    return texture->index;

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

    if (i==-1)
    {
	I_Error ("R_TextureNumForName: %s not found",
		 name);
    }
    return i;
}




//
// R_PrecacheLevel
// Preloads all relevant graphics for the level.
//
int		flatmemory;
int		texturememory;
//int		spritememory;

texturecomposite_t* precachedflats;

void R_PrecacheLevel (void)
{
    char*		flatpresent;
    char*		texturepresent;
    //char*		spritepresent;

    int			i;
    int			j;
    int			lump;

    texture_t*		texture;

	byte*			baseflatdata;
	byte*			outputflatdata;
	int				currmapindex;
	lighttable_t*	currmap;
	int				currflatbyte;

	int32_t			animstart;
	int32_t			animend;
	int32_t			thisanim;

	// Precache flats.
	flatpresent = (char*)Z_Malloc(numflats, PU_STATIC, NULL);
	memset (flatpresent,0,numflats);	

	for (i=0 ; i<numsectors ; i++)
	{
		flatpresent[sectors[i].floorpic] = 1;
		flatpresent[sectors[i].ceilingpic] = 1;

		animstart = P_GetPicAnimStart( false, sectors[i].floorpic );
		if( animstart >= 0 )
		{
			animend = animstart + P_GetPicAnimLength( false, animstart );
			for( thisanim = animstart; thisanim < animend; ++thisanim )
			{
				flatpresent[ thisanim ] = 1;
			}
		}

		animstart = P_GetPicAnimStart( false, sectors[i].ceilingpic );
		if( animstart >= 0 )
		{
			animend = animstart + P_GetPicAnimLength( false, animstart );
			for( thisanim = animstart; thisanim < animend; ++thisanim )
			{
				flatpresent[ thisanim ] = 1;
			}
		}
	}
	
	flatmemory = 0;

	for (i=0 ; i<numflats ; i++)
	{
		precachedflats[ i ].data = NULL;
		lump = firstflat + i;

		if (flatpresent[i])
		{
			flatmemory += lumpinfo[lump]->size;
			baseflatdata = (byte*)W_CacheLumpNum(lump, PU_CACHE);

			outputflatdata = (byte*)Z_Malloc(lumpinfo[lump]->size * NUMCOLORMAPS, COMPOSITE_ZONE, &precachedflats[ i ].data );

			for( currmapindex = 0; currmapindex < NUMCOLORMAPS; ++currmapindex )
			{
				currmap = colormaps + currmapindex * 256;
				for( currflatbyte = 0; currflatbyte < lumpinfo[lump]->size; ++currflatbyte)
				{
					*outputflatdata = currmap[ baseflatdata[ currflatbyte ] ];
					++outputflatdata;
				}
			}
		}
	}

	Z_Free(flatpresent);

	// Precache textures.
	texturepresent = (char*)Z_Malloc(numtextures, PU_STATIC, NULL);
	memset (texturepresent,0, numtextures);
	
	for (i=0 ; i<numsides ; i++)
	{
		texturepresent[sides[i].toptexture] = 1;
		texturepresent[sides[i].midtexture] = 1;
		texturepresent[sides[i].bottomtexture] = 1;
	}

	// Sky texture is always present.
	// Note that F_SKY1 is the name used to
	//  indicate a sky floor/ceiling as a flat,
	//  while the sky texture is stored like
	//  a wall texture, with an episode dependend
	//  name.
	texturepresent[skytexture] = 1;
	
	texturememory = 0;
	for (i=0 ; i<numtextures ; i++)
	{
		if (!texturepresent[i])
			continue;

		texture = textures[i];
	
		for (j=0 ; j<texture->patchcount ; j++)
		{
			lump = texture->patches[j].patch;
			texturememory += lumpinfo[lump]->size;
			W_CacheLumpNum( lump , PU_LEVEL ); // TODO: Switch to cache after masked textures get a SIMD renderer
		}

		R_CacheComposite( i );
	}

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




