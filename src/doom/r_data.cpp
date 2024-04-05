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


#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"

#include "deh_main.h"

#include "m_container.h"

#include "i_system.h"
#include "i_swap.h"
#include "i_terminal.h"

#include "m_conv.h"
#include "m_fixed.h"
#include "m_misc.h"

#include "p_local.h"
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

	struct patchdata_t
	{
		lumpindex_t			lump;
		int32_t				rowoffset;
		int32_t*			columnoffset;
	};

	int32_t					numspritelumps;
	patch_t**				spritepatches;

	lumpindex_t				firstcolormap = -1;
	lumpindex_t				lastcolormap = -1;
	int32_t					numcolormaps = 0;

	int32_t					numtextures;
	texture_t**				textures;

	patchdata_t**			texturepatchdata;
	texturecomposite_t*		texturecomposite;

	texturecomposite_t*		flatcomposite;

	texturecomposite_t**	texturelookup;
	texturecomposite_t**	flatlookup;

	// for global animation
	int32_t*				flattranslation;
	int32_t*				texturetranslation;

	// needed for pre rendering
	rend_fixed_t*			spritewidth;
	rend_fixed_t*			spriteoffset;
	rend_fixed_t*			spritetopoffset;

	lighttable_t			*colormaps;
	byte*					tranmap;
}

std::unordered_map< std::string, texture_t* >	texturenamelookup;

std::unordered_map< std::string, lookup_t >		flatnamelookup;
std::vector< lookup_t >							flatindexlookup;

std::unordered_map< std::string, lookup_t >		spritenamelookup;
std::vector< lookup_t >							spriteindexlookup;

constexpr size_t DoomStrLen( const char* val )
{
	int32_t size = 0;
	while( size < 8 && *val++ != 0 )
	{
		++size;
	}
	return size;
}
constexpr int32_t NumFlats()							{ return (int32_t)flatindexlookup.size(); }
constexpr std::string ClampString( const char* val )	{ return ToUpper( std::string( val, DoomStrLen( val ) ) ); }

static void FillLookup( const char* L_START, const char* L_END
						, const char* LL_START, const char* LL_END
						, std::function< void( int32_t, int32_t ) > addfunc ) 
{
	for( wad_file_t* file : W_GetLoadedWADFiles() )
	{
		int32_t startindex = -1;

		for( int32_t lumpindex : W_LumpIndicesFor( file ) )
		{
			lumpinfo_t* lump = lumpinfo[ lumpindex ];
			if( startindex >= 0 )
			{
				if( strcasecmp( lump->name, L_END ) == 0
					|| strcasecmp( lump->name, LL_END ) == 0 )
				{
					addfunc( startindex, lumpindex );
					startindex = -1;
				}
			}
			else
			{
				if( strcasecmp( lump->name, L_START ) == 0
					|| strcasecmp( lump->name, LL_START ) == 0 )
				{
					startindex = lumpindex + 1;
				}
			}
		}
	}
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
void R_DrawColumnInCache( column_t* patch, byte* cache, int originy, int cacheheight )
{
	int		count = 0;
	int		position = 0;
	byte*	source = nullptr;

	int32_t baseoffset = -1;
	while (patch->topdelta != 0xff)
	{
		source = (byte *)patch + 3;
		count = patch->length;
		if( comp.tall_patches && patch->topdelta <= baseoffset )
		{
			baseoffset += patch->topdelta;
		}
		else
		{
			baseoffset = patch->topdelta;
		}

		position = originy + baseoffset;

		if (position < 0)
		{
			count += position;
			position = 0;
		}

		if (position + count > cacheheight)
			count = cacheheight - position;

		if (count > 0)
		{
			memcpy (cache + position, source, count);
		}
		
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
	memset( block, comp.no_medusa ? 0 : 0xFB, texturecomposite[ texnum ].size );

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
	texture_t* texture = textures[ texnum ];
	patchdata_t* data = texturepatchdata[ texnum ];

	// Vanilla requires a valid patch in every column of a texture.
	// Multi-patch 2S sidedef support removes this requirement.
	byte* patchcount = (byte *)Z_Malloc( texture->width, PU_STATIC, &patchcount );
	memset( patchcount, 0, texture->width );

	for( texpatch_t& patch : std::span( texture->patches, texture->patchcount ) )
	{
		patch_t* realpatch = (patch_t*)W_CacheLumpNum( patch.patch, COMPOSITE_ZONE );
		int32_t x1 = patch.originx;
		int32_t x2 = M_MIN( texture->width, x1 + SHORT( realpatch->width ) );
		x1 = M_CLAMP( x1, 0, texture->width );

		data->lump = patch.patch;
		data->rowoffset = patch.originy;
		for( int32_t x : iota( x1, x2 ) )
		{
			data->columnoffset[ x ] = LONG( realpatch->columnofs[ x - x1 ] );
			++patchcount[ x ];
		}

		++data;
	}
	
	if( !comp.multi_patch_2S_linedefs )
	{
		for( int32_t x = 0; x < texture->width; ++x )
		{
			if( !patchcount[ x ] )
			{
				I_TerminalPrintf( Log_Warning, "R_GenerateLookup: column without a patch (%s)\n", texture->name );
				break;
			}
		}
	}

	Z_Free( patchcount );
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
	auto& info = flatindexlookup[ flat ];

	int32_t lump = info.lumpindex;

	byte* transposedflatdata = (byte*)Z_Malloc( 64 * 64, COMPOSITE_ZONE, &flatcomposite[ flat ].data );
	if( W_LumpLength( lump ) == 0 )
	{
		memset( transposedflatdata, 0, 64 * 64 );
	}
	else
	{
		byte* originalflatdata = (byte*)W_CacheLumpNum( lump, PU_CACHE );

		byte* currsource = originalflatdata;
		for( int32_t y : iota( 0, 64 ) )
		{
			byte* currdest = transposedflatdata + y;
			for( [[maybe_unused]] int32_t x : iota( 0, 64 ) )
			{
				*currdest = *currsource;
				++currsource;
				currdest += 64;
			}
		}
	}
}

texturecomposite_t* R_CacheAndGetCompositeFlat( const char* flat )
{
	int32_t index = R_FlatNumForName( flat );

	if( index >= NumFlats() )
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
		I_Error( "R_GetColumnComposite: Attempting to create composite during rendering." );
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

int32_t R_GetPatchYOffset( int32_t tex, int32_t patch )
{
	if( tex > numtextures || patch >= texturelookup[ tex ]->patchcount )
	{
		return 0;
	}

	return texturepatchdata[ tex ][ patch ].rowoffset;
}

column_t* R_GetRawColumn( int32_t tex, int32_t patch, int32_t col )
{
	if( tex > numtextures )
	{
		// THIS IS SO NASTY. NEED TO CACHE THIS
		memcpy( flatcolumnhack + 3, R_GetColumn( tex, col ), 64 );
		return (column_t*)flatcolumnhack;
	}
	
	if( patch < texturelookup[ tex ]->patchcount )
	{
		col &= texturelookup[ tex ]->widthmask;

		patchdata_t& data = texturepatchdata[ tex ][ patch ];
		int32_t ofs = data.columnoffset[ col ];

		if( data.lump > 0 && ofs >= 0 )
		{
			byte* loaded = (byte*)W_CacheLumpNum( data.lump, COMPOSITE_ZONE );
			column_t* column = (column_t*)( loaded + ofs );
			return column;
		}
	}

	return nullptr;
}

constexpr colfunc_t colfuncsforheight[] =
{
	&R_DrawColumn_Colormap_16,
	&R_DrawColumn_Colormap_32,
	&R_DrawColumn_Colormap_64,
	&R_DrawColumn_Colormap_128,
	&R_DrawColumn_Colormap_256,
	&R_DrawColumn_Colormap_512,
};

constexpr colfunc_t transcolfuncsforheight[] =
{
	&R_DrawColumn_Transparent_16,
	&R_DrawColumn_Transparent_32,
	&R_DrawColumn_Transparent_64,
	&R_DrawColumn_Transparent_128,
	&R_DrawColumn_Transparent_256,
	&R_DrawColumn_Transparent_512,
};

constexpr rasterfunc_t floorfuncs[ 7 ][ 4 ] =
{
	{	&R_RasteriseRegion16x16,	&R_RasteriseRegion16x32,	&R_RasteriseRegion16x64,	&R_RasteriseRegion16x128	},
	{	&R_RasteriseRegion32x16,	&R_RasteriseRegion32x32,	&R_RasteriseRegion32x64,	&R_RasteriseRegion32x128	},
	{	&R_RasteriseRegion64x16,	&R_RasteriseRegion64x32,	&R_RasteriseRegion64x64,	&R_RasteriseRegion64x128	},
	{	&R_RasteriseRegion128x16,	&R_RasteriseRegion128x32,	&R_RasteriseRegion128x64,	&R_RasteriseRegion128x128	},
	{	&R_RasteriseRegion256x16,	&R_RasteriseRegion256x32,	&R_RasteriseRegion256x64,	&R_RasteriseRegion256x128	},
	{	&R_RasteriseRegion512x16,	&R_RasteriseRegion512x32,	&R_RasteriseRegion512x64,	&R_RasteriseRegion512x128	},
	{	&R_RasteriseRegion1024x16,	&R_RasteriseRegion1024x32,	&R_RasteriseRegion1024x64,	&R_RasteriseRegion1024x128	},
};

constexpr int32_t maxcolfuncs = 5;

void R_InitTextureAndFlatComposites( void )
{
	size_t totallookup = numtextures + NumFlats();

	texturecomposite = (texturecomposite_t*)Z_Malloc( numtextures * sizeof( *texturecomposite ), PU_STATIC, 0 );

	texturelookup = (texturecomposite_t**)Z_Malloc( totallookup * sizeof( *texturelookup ), PU_STATIC, 0 );
	flatlookup = (texturecomposite_t**)Z_Malloc( totallookup * sizeof( *flatlookup ), PU_STATIC, 0 );

	flatcomposite = (texturecomposite_t*)Z_Malloc( NumFlats() * sizeof(texturecomposite_t), PU_STATIC, NULL );
	int32_t index = 0;
	texturecomposite_t** texturedest = texturelookup;
	texturecomposite_t** flatdest = flatlookup + NumFlats();

	for( texture_t* texture : std::span( textures, numtextures ) )
	{
		texturecomposite_t& composite = texturecomposite[ index ];

		// Gotta be an easier way to find current or lower power of 2...
		int32_t mask = 1;
		while( mask * 2 <= texture->width ) mask <<= 1;

		if( !comp.arbitrary_wall_sizes )
		{
			composite.wallrender = &R_DrawColumn_Colormap_128;
			composite.transparentwallrender = &R_DrawColumn_Transparent_128;
			composite.midtexrender = &R_SpriteDrawColumn_Colormap;
			composite.transparentmidtexrender = &R_SpriteDrawColumn_Transparent;
			composite.floorrender = &R_RasteriseRegion64x64;
		}
		else
		{
			double_t logheightdouble = log2( texture->height ) - 4.0;
			int32_t logheight = (int32_t)logheightdouble;

			if( texture->height > 128 )
			{
				composite.midtexrender = &R_LimitRemovingSpriteDrawColumn_Colormap;
				composite.transparentmidtexrender = &R_LimitRemovingSpriteDrawColumn_Transparent;
			}
			else
			{
				composite.midtexrender = &R_SpriteDrawColumn_Colormap;
				composite.transparentmidtexrender = &R_SpriteDrawColumn_Transparent;
			}

			if( logheightdouble != (double_t)logheight || logheight < 0 || logheight >= maxcolfuncs )
			{
				composite.wallrender = &R_LimitRemovingDrawColumn_Colormap;
				composite.transparentwallrender = &R_LimitRemovingDrawColumn_Transparent;
			}
			else
			{
				composite.wallrender = colfuncsforheight[ logheight ];
				composite.transparentwallrender = transcolfuncsforheight[ logheight ];
			}

			double_t logwidthdouble = log2( texture->width ) - 4.0;
			int32_t logwidth = (int32_t)logwidthdouble;

			if( logwidth < 0 || logwidth > 6
				|| logheight < 0 || logheight > 3 )
			{
				composite.floorrender = &R_RasteriseRegion64x64;
			}
			else
			{
				composite.floorrender = floorfuncs[ logwidth ][ logheight ];
			}
		}

		composite.data = NULL;
		memcpy( composite.name, texture->name, sizeof( texture->name ) );
		composite.namepadding = 0;
		composite.size = texture->width * texture->height;
		composite.width = texture->width;
		composite.height = texture->height;
		composite.pitch = texture->height;
		composite.widthmask = mask - 1;
		composite.patchcount = texture->patchcount;
		composite.renderheight = IntToRendFixed( texture->height );
		composite.index = index;

		R_GenerateLookup( index );

		*texturedest++ = *flatdest++ = &composite;
		++index;
	}

	index = 0;
	texturedest = texturelookup + numtextures;
	flatdest = flatlookup;

	for( texturecomposite_t& composite : std::span( flatcomposite, NumFlats() ) )
	{
		auto& entrydetails = flatindexlookup[ index ];
		lumpinfo_t* lumpentry = lumpinfo[ entrydetails.lumpindex ];

		composite.data = NULL;
		composite.wallrender = &R_DrawColumn_Colormap_64;
		composite.transparentwallrender = &R_DrawColumn_Transparent_64;
		composite.floorrender = &R_RasteriseRegion64x64;
		strncpy( composite.name, lumpentry->name, 8 );
		composite.namepadding = 0;
		composite.size = 64 * 64;
		composite.width = 64;
		composite.height = 64;
		composite.pitch = 64;
		composite.widthmask = 63;
		composite.patchcount = 1;
		composite.renderheight = IntToRendFixed( 64 );
		composite.index = index;

		*texturedest++ = *flatdest++ = &composite;
		++index;
	}

	// Create translation table for global animation.
	texturetranslation = (int*)Z_Malloc ( ( totallookup + 1 ) * sizeof(*texturetranslation), PU_STATIC, 0);
	for( int32_t i : iota( 0, (int32_t)totallookup ) )
	{
		texturetranslation[i] = i;
	}

	flattranslation = (int*)Z_Malloc ( ( totallookup + 1 ) * sizeof(*flattranslation), PU_STATIC, 0);
	for( int32_t i : iota( 0, (int32_t)totallookup ) )
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

	std::vector< lookup_t > patchlookup;
	patchlookup.reserve( nummappatches );

	if( !comp.additive_data_blocks )
	{
		for (i = 0; i < nummappatches; i++)
		{
			M_StringCopy(name, name_p + i * 8, sizeof(name));
			patchlookup.push_back( { W_CheckNumForNameExcluding(name, wt_system), i } );
		}
	}
	else
	{
		std::unordered_map< std::string, lookup_t > patchnamelookup;
		int32_t numpatchentries = 0;

		FillLookup( "P_START", "P_END", "PP_START", "PP_END", [&patchnamelookup, &numpatchentries]( int32_t begin, int32_t end )
		{
			for( int32_t curr = begin; curr < end; ++curr )
			{
				std::string lumpname = ClampString( lumpinfo[ curr ]->name );
				// Need to account for lump overrides outside of markers
				auto found = patchnamelookup.find( lumpname );
				if( found == patchnamelookup.end() )
				{
					patchnamelookup[ lumpname ] = { curr, numpatchentries++ };
				}
				else
				{
					found->second.lumpindex = curr;
				}
			}
		} );

		for( int32_t mappatch = 0; mappatch < nummappatches; ++mappatch )
		{
			std::string patchname = ClampString( name_p );
			auto found = patchnamelookup.find( patchname );
			if( found == patchnamelookup.end() )
			{
				// Some WADs have their patches sitting outside of markers. Let's try to find it.
				lookup_t lookup = { W_CheckNumForName( patchname.c_str() ), numpatchentries++ };
				patchnamelookup[ patchname ] = lookup;
				patchlookup.push_back( lookup );
			}
			else
			{
				patchlookup.push_back( found->second );
			}
			name_p += 8;
		}
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
	texturepatchdata = (patchdata_t**)Z_Malloc( numtextures * sizeof( *texturepatchdata ), PU_STATIC, 0 );

    //	Really complex printing shit...
    temp1 = W_GetNumForName(DEH_String("S_START"));  // P_???????
    temp2 = W_GetNumForName(DEH_String("S_END")) - 1;
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

		texture = texturenamelookup[ ClampString( mtexture->name ) ] = textures[i] = (texture_t*)Z_Malloc(sizeof(texture_t) + sizeof(texpatch_t)*(SHORT(mtexture->patchcount)-1), PU_STATIC, 0);

		texture->index = i;
		texture->width = SHORT(mtexture->width);
		texture->height = SHORT(mtexture->height);
		texture->patchcount = SHORT(mtexture->patchcount);
	
		memcpy (texture->name, mtexture->name, sizeof(texture->name));
		texture->padding = 0;
		mpatch = &mtexture->patches[0];
		patch = &texture->patches[0];

		patchdata_t* patchdata = texturepatchdata[ i ] = (patchdata_t*)Z_Malloc( sizeof(patchdata_t) * texture->patchcount, PU_STATIC, nullptr );
		for (j=0 ; j<texture->patchcount ; j++, mpatch++, patch++)
		{
			patch->originx = SHORT(mpatch->originx);
			patch->originy = SHORT(mpatch->originy);
			patch->patch = patchlookup[SHORT(mpatch->patch)].lumpindex;
			if (patch->patch == -1)
			{
				I_Error ("R_InitTextures: Missing patch in texture %s",
					 texture->name);
			}
			patchdata->lump = patch->patch;
			patchdata->columnoffset = (int32_t*)Z_Malloc( sizeof(int32_t) * texture->width, PU_STATIC, nullptr );
			memset( patchdata->columnoffset, -1, sizeof( int32_t ) * texture->width );
			++patchdata;
		}
	}

	W_ReleaseLumpName(DEH_String("TEXTURE1"));
	if (maptex2)
	{
		W_ReleaseLumpName(DEH_String("TEXTURE2"));
	}
}



//
// R_InitFlats
//
void AddLumpRangeToFlats( int32_t begin, int32_t end )
{
	for( int32_t currflat = begin; currflat < end; ++currflat )
	{
		std::string lumpname = ClampString( lumpinfo[ currflat ]->name );

		int32_t numflats = NumFlats();
		flatindexlookup.push_back( { currflat, numflats } );
		flatnamelookup[ lumpname ] = { currflat, numflats };
	}
}

void R_InitFlats (void)
{
	if( comp.additive_data_blocks )
	{
		const char* F_START = DEH_String("F_START");
		const char* FF_START = "FF_START";

		const char* F_END = DEH_String("F_END");
		const char* FF_END = "FF_END";

		FillLookup( F_START, F_END, FF_START, FF_END, &AddLumpRangeToFlats );
	}
	else
	{
		int32_t firstflat = W_GetNumForNameExcluding(DEH_String("F_START"), wt_system) + 1;
		int32_t lastflat = W_GetNumForNameExcluding(DEH_String("F_END"), wt_system);
		AddLumpRangeToFlats( firstflat, lastflat + 1 );
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
	numspritelumps = 0;
	
	if( !comp.additive_data_blocks )
	{
		int32_t firstspritelump = W_GetNumForNameExcluding(DEH_String("S_START"), wt_system) + 1;
		int32_t lastspritelump = W_GetNumForNameExcluding(DEH_String("S_END"), wt_system) - 1;
		spriteindexlookup.reserve( lastspritelump - firstspritelump + 1 );

		for( int32_t curr = firstspritelump; curr <= lastspritelump; ++curr )
		{
			std::string spritename = ClampString( lumpinfo[ curr ]->name );
			spritenamelookup[ spritename ] = { curr, numspritelumps };
			spriteindexlookup.push_back( { curr, numspritelumps } );
			++numspritelumps;
		}
	}
	else
	{
		FillLookup( DEH_String( "S_START" ), DEH_String( "S_END" ), "SS_START", "SS_END", []( int32_t begin, int32_t end )
		{
			for( int32_t curr = begin; curr < end; ++curr )
			{
				std::string spritename = ClampString( lumpinfo[ curr ]->name );
				auto found = spritenamelookup.find( spritename );
				if( found == spritenamelookup.end() )
				{
					spritenamelookup[ spritename ] = { curr, numspritelumps };
					spriteindexlookup.push_back( { curr, numspritelumps } );
					++numspritelumps;
				}
				else
				{
					found->second.lumpindex = curr;
					spriteindexlookup[ found->second.compositeindex ].lumpindex = curr;
				}
			}
		} );
	}

    spritewidth = (rend_fixed_t*)Z_Malloc (numspritelumps*sizeof(*spritewidth), PU_STATIC, 0);
    spriteoffset = (rend_fixed_t*)Z_Malloc (numspritelumps*sizeof(*spriteoffset), PU_STATIC, 0);
    spritetopoffset = (rend_fixed_t*)Z_Malloc (numspritelumps*sizeof(*spritetopoffset), PU_STATIC, 0);
	spritepatches = (patch_t**)Z_Malloc(numspritelumps * sizeof(*spritepatches), PU_STATIC, 0 );
	
	for( lookup_t& lookup : spriteindexlookup )
	{
		if( !( lookup.compositeindex & 63 ) )
		{
			I_TerminalPrintf( Log_None, "." );
		}

		patch_t* patch = (patch_t*)W_CacheLumpNum( lookup.lumpindex, PU_STATIC );
		spritewidth[ lookup.compositeindex ] = IntToRendFixed( SHORT(patch->width) );
		spriteoffset[ lookup.compositeindex ] = IntToRendFixed( SHORT(patch->leftoffset) );
		spritetopoffset[ lookup.compositeindex ] = IntToRendFixed( SHORT(patch->topoffset) );

		spritepatches[ lookup.compositeindex ] = patch;
    }
}



//
// R_InitColormaps
//

constexpr double_t DefaultTranmapPercent = 0.65625;

constexpr pixel_t LerpPixel( pixel_t from, pixel_t to, double_t percent )
{
	return (pixel_t)( (int32_t)from + (int32_t)( (int32_t)to - (int32_t)from ) * percent );
}

byte* GenerateTranmapLinear( double_t percent, zonetag_t tag )
{
	byte* output = (byte*)Z_MallocZero( 256 * 256, tag, nullptr );

	rgb_t* playpal = (rgb_t*)W_CacheLumpName( "PLAYPAL", PU_CACHE );

	byte* dest = output;
	rgb_t* from = playpal;

	for( [[maybe_unused]] int32_t fromindex : iota( 0, 256 ) )
	{
		rgb_t* to = playpal;
		for( [[maybe_unused]] int32_t toindex : iota( 0, 256 ) )
		{
			rgb_t mixed =
			{
				LerpPixel( from->r, to->r, percent ),
				LerpPixel( from->g, to->g, percent ),
				LerpPixel( from->b, to->b, percent )
			};

			int32_t bestdist = INT_MAX;
			byte bestmatch = 0;

			rgb_t* palentry = playpal;
			for( int32_t palindex : iota( 0, 256 ) )
			{
				int32_t rdist = (int32_t)mixed.r - palentry->r;
				int32_t gdist = (int32_t)mixed.g - palentry->g;
				int32_t bdist = (int32_t)mixed.b - palentry->b;

				int32_t dist = (rdist * rdist) + (gdist * gdist) + (bdist * bdist);
				if( dist < bestdist )
				{
					bestdist = dist;
					bestmatch = palindex;
				}
				if( dist == 0 )
				{
					break;
				}

				++palentry;
			}

			*dest++ = bestmatch;
			++to;
		}

		++from;
	}

	return output;
}

void R_InitColormaps (void)
{
    int	lump;

    // Load in the light tables, 
    //  256 byte align tables.
    lump = W_GetNumForName(DEH_String("COLORMAP"));
    colormaps = (lighttable_t*)W_CacheLumpNum(lump, PU_STATIC);

	if( comp.use_colormaps )
	{
		firstcolormap = W_CheckNumForName( "C_START" ) + 1;
		lastcolormap = W_CheckNumForName( "C_END" ) - 1;
		numcolormaps = M_MAX( 0, lastcolormap - firstcolormap + 1 );

		tranmap = GenerateTranmapLinear( DefaultTranmapPercent, PU_STATIC );
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
	std::string lookup = ClampString( name );
	auto found = flatnamelookup.find( lookup );

	if( found != flatnamelookup.end() )
	{
		return found->second.compositeindex;
	}

	if( !comp.any_texture_any_surface )
	{
		I_Error( "R_FlatNumForName: %s not found", name );
		return -1;
	}

	int32_t index = R_CheckTextureNumForName( name );

	if( index == -1 )
	{
		I_LogAddEntryVar( Log_Warning, "R_FlatNumForName: %s not found in textures or flats, replacing with %s", ClampString( name ).c_str(), flatcomposite[ 0 ].name );
	}

	return index + NumFlats();
}

DOOM_C_API int32_t R_GetNumFlats()
{
	return NumFlats();
}

DOOM_C_API int R_SpriteNumForName( const char* name )
{
	std::string lookup = ClampString( name );
	auto found = spritenamelookup.find( lookup );

	if( found != spritenamelookup.end() )
	{
		return found->second.compositeindex;
	}

	return -1;
}

const std::vector< lookup_t >& R_GetSpriteLumps()
{
	return spriteindexlookup;
}

//
// R_CheckTextureNumForName
// Check whether texture is available.
// Filter out NoTexture indicator.
//
int R_CheckTextureNumForName(const char *name)
{
	// "NoTexture" marker.
	if( name[0] == '-' )
	{
		return 0;
	}

	if( comp.zero_length_texture_names && name[ 0 ] == 0 )
	{
		return 0;
	}

	auto found = texturenamelookup.find( ClampString( name ) );
	if( found != texturenamelookup.end() )
	{
		return found->second->index;
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

	if( !comp.any_texture_any_surface )
	{
		I_Error ("R_TextureNumForName: %s not found", name );
		return -1;
	}

	auto found = flatnamelookup.find( ClampString( name ) );

	if( found == flatnamelookup.end() )
	{
		I_LogAddEntryVar( Log_Warning, "R_TextureNumForName: %s not found in textures or flats, replacing with %s", ClampString( name ).c_str(), textures[ 0 ]->name );
		return 0;
	}

	return numtextures + found->second.compositeindex;
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

	flatpresent = (char*)Z_Malloc(NumFlats(), PU_STATIC, NULL);
	memset (flatpresent,0,NumFlats());
	texturepresent = (char*)Z_Malloc(numtextures, PU_STATIC, NULL);
	memset (texturepresent,0, numtextures);

	// Check flats and textures for presence, jamming in to appropriate array as necessary
	auto MarkFlatPresence = [ &texturepresent, &flatpresent ]( int16_t flatnum )
	{
		if( flatnum < NumFlats() )
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
			int32_t texnum = flatnum - NumFlats();
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
	for( i=0 ; i< NumFlats() ; i++ )
	{
		flatcomposite[ i ].data = NULL;

		if( flatpresent[ i ] )
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
			W_CacheLumpNum( lump , PU_LEVEL );
		}

		R_CacheCompositeTexture( i );
	}

	Z_Free(flatpresent);
	Z_Free(texturepresent);
}




