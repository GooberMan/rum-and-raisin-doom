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
//	Do all the WAD I/O, get map description,
//	set up initial state and misc. LUTs.
//
#include <math.h>
#include <stdlib.h>

extern "C"
{
	#include "doomdata.h"
	#include "i_system.h"
	#include "i_swap.h"
	#include "m_argv.h"
	#include "m_bbox.h"
	#include "m_misc.h"
	#include "p_local.h"
	#include "r_defs.h"
	#include "w_wad.h"
	#include "z_zone.h"
}

#define MAX_DEATHMATCH_STARTS	10
sector_t* GetSectorAtNullAddress(void);

#include <ranges>
#include <span>
#include <type_traits>

struct ReadVal_Vanilla
{
	template< typename _ty >
	requires ( std::is_integral_v< _ty > && sizeof( _ty ) == 2 )
	static INLINE int16_t Short( const _ty& val )
	{
		return SDL_SwapLE16( val );
	}

	template< typename _ty >
	requires ( std::is_integral_v< _ty > && sizeof( _ty ) == 2 )
	static INLINE uint16_t UShort( const _ty& val )
	{
		return SDL_SwapLE16( val );
	}

	template< typename _ty >
	requires ( std::is_integral_v< _ty > && sizeof( _ty ) == 4 )
	static INLINE int32_t Long( const _ty& val )
	{
		return SDL_SwapLE32( val );
	}

	template< typename _ty >
	requires ( std::is_integral_v< _ty > && sizeof( _ty ) == 4 )
	static INLINE uint32_t ULong( const _ty& val )
	{
		return SDL_SwapLE32( val );
	}

	template< typename _ty >
	requires ( std::is_integral_v< _ty > )
	static INLINE auto AsIs( const _ty& val )
	{
		if constexpr( sizeof( _ty ) == 2 )
		{
			if constexpr( std::is_unsigned_v< _ty > )
			{
				return UShort( val );
			}
			else
			{
				return Short( val );
			}
		}
		else if constexpr( sizeof( _ty ) == 4 )
		{
			if constexpr( std::is_unsigned_v< _ty > )
			{
				return UInt( val );
			}
			else
			{
				return Int( val );
			}
		}
		else
		{
			return;
		}
	}
};

struct ReadVal_LimitRemoving
{
	template< typename _ty >
	requires ( std::is_integral_v< _ty > && sizeof( _ty ) == 2 )
	static INLINE uint16_t Short( const _ty& val )
	{
		return SDL_SwapLE16( *(uint16_t*)&val );
	}

	template< typename _ty >
	requires ( std::is_integral_v< _ty > && sizeof( _ty ) == 2 )
	static INLINE uint16_t UShort( const _ty& val )
	{
		return SDL_SwapLE16( *(uint16_t*)&val );
	}

	template< typename _ty >
	requires ( std::is_integral_v< _ty > && sizeof( _ty ) == 4 )
	static INLINE int32_t Long( const _ty& val )
	{
		return SDL_SwapLE32( val );
	}

	template< typename _ty >
	requires ( std::is_integral_v< _ty > && sizeof( _ty ) == 4 )
	static INLINE uint32_t ULong( const _ty& val )
	{
		return SDL_SwapLE32( val );
	}

	template< typename _ty >
	requires ( std::is_integral_v< _ty > )
	static INLINE auto AsIs( const _ty& val )
	{
		if constexpr( sizeof( _ty ) == 2 )
		{
			if constexpr( std::is_unsigned_v< _ty > )
			{
				return UShort( val );
			}
			else
			{
				return Short( val );
			}
		}
		else if constexpr( sizeof( _ty ) == 4 )
		{
			if constexpr( std::is_unsigned_v< _ty > )
			{
				return UInt( val );
			}
			else
			{
				return Int( val );
			}
		}
		else
		{
			return;
		}
	}
};

template< typename _from, typename _to, typename _functor >
auto WadDataConvert( int32_t lumpnum, size_t dataoffset, _functor&& func )
{
	struct
	{
		_to*		output;
		int32_t		count;
	} data;

	data.count = (int32_t)( ( W_LumpLength( lumpnum ) - dataoffset ) / sizeof( _from ) );
	data.output = (_to*)Z_Malloc( data.count * sizeof( _to ), PU_LEVEL, 0 );
	memset( data.output, 0, data.count * sizeof( _to ) );

	_from* rhs = ( _from* )( (byte*)W_CacheLumpNum( lumpnum, PU_STATIC ) + dataoffset );
	_to* lhs = data.output;

	for( int32_t curr : std::ranges::views::iota( 0, data.count ) )
	{
		func( curr, *lhs, *rhs );
		++lhs;
		++rhs;
	}

	W_ReleaseLumpNum( lumpnum );

	return data;
}

#pragma optimize( "", off )
#undef INLINE
#define INLINE __declspec( noinline )

typedef enum class NodeFormat : int32_t
{
	None,
	Vanilla,
	DeepBSP,
} nodeformat_t;

template< typename _reader >
struct DoomMapLoader
{
	using Read = _reader;

	int32_t			_numvertices;
	vertex_t*		_vertices;

	int32_t			_numsegs;
	seg_t*			_segs;

	int32_t			_numsectors;
	sector_t*		_sectors;

	int32_t			_numsubsectors;
	subsector_t*	_subsectors;

	int32_t			_numnodes;
	nodeformat_t	_nodeformat;
	node_t*			_nodes;

	int32_t			_numlines;
	line_t*			_lines;

	int32_t			_numsides;
	side_t*			_sides;

	int32_t			_totallines;

	fixed_t			_blockmaporgx;
	fixed_t			_blockmaporgy;
	int32_t			_blockmapwidth;
	int32_t			_blockmapheight;
	int16_t*		_blockmap;
	mobj_t**		_blocklinks;

	byte*			_rejectmatrix;

	mapthing_t		_deathmatchstarts[MAX_DEATHMATCH_STARTS];
	mapthing_t*		_deathmatch_p;
	mapthing_t		_playerstarts[MAXPLAYERS];
	boolean			_playerstartsingame[MAXPLAYERS];

	lumpinfo_t*		_maplumpinfo;

	constexpr auto	Vertices() const noexcept	{ return std::span( _vertices,		_numvertices ); }
	constexpr auto	Segs() const noexcept		{ return std::span( _segs,			_numsegs ); }
	constexpr auto	Sectors() const noexcept	{ return std::span( _sectors,		_numsectors ); }
	constexpr auto	SubSectors() const noexcept	{ return std::span( _subsectors,	_numsubsectors ); }
	constexpr auto	Nodes() const noexcept		{ return std::span( _nodes,			_numnodes ); }
	constexpr auto	Lines() const noexcept		{ return std::span( _lines,			_numlines ); }
	constexpr auto	Sides() const noexcept		{ return std::span( _sides,			_numsides ); }

	void INLINE LoadBlockmap( int32_t lumpnum )
	{
		auto data = WadDataConvert< int16_t, int16_t >( lumpnum, 0, []( int32_t index, int16_t& out, const int16_t& in )
		{
			out = Read::Short( in );
		} );

		_blockmap			= data.output;

		_blockmaporgx		= *_blockmap++ << FRACBITS;
		_blockmaporgy		= *_blockmap++ << FRACBITS;
		_blockmapwidth		= *_blockmap++;
		_blockmapheight		= *_blockmap++;

		int32_t linkssize = sizeof(*_blocklinks) * _blockmapwidth * _blockmapheight;
		_blocklinks = (mobj_t**)Z_Malloc( linkssize, PU_LEVEL, 0 );
		memset( _blocklinks, 0, linkssize );
	}

	void INLINE LoadVertices( int32_t lumpnum )
	{
		auto data = WadDataConvert< mapvertex_t, vertex_t >( lumpnum, 0, []( int32_t index, vertex_t& out, const mapvertex_t& in )
		{
			out.x			= Read::Short( in.x ) << FRACBITS;
			out.y			= Read::Short( in.y ) << FRACBITS;
		} );

		_numvertices		= data.count;
		_vertices			= data.output;
	}

	void INLINE LoadSectors( int32_t lumpnum )
	{
		auto data = WadDataConvert< mapsector_t, sector_t >( lumpnum, 0, []( int32_t index, sector_t& out, const mapsector_t& in )
		{
			out.index			= index;
			out.floorheight		= Read::Short( in.floorheight ) << FRACBITS;
			out.ceilingheight	= Read::Short( in.ceilingheight ) << FRACBITS;
			out.floorpic		= R_FlatNumForName( in.floorpic );
			out.ceilingpic		= R_FlatNumForName( in.ceilingpic );
			out.lightlevel		= Read::Short( in.lightlevel );
			out.special			= Read::Short( in.special );
			out.tag				= Read::Short( in.tag );
			out.thinglist		= NULL;
			out.secretstate		= out.special == 9 ? Secret_Undiscovered : Secret_None;
		} );

		_numsectors				= data.count;
		_sectors				= data.output;
	}

	void INLINE LoadSidedefs( int32_t lumpnum )
	{
		auto data = WadDataConvert< mapsidedef_t, side_t >( lumpnum, 0, [ this ]( int32_t index, side_t& out, const mapsidedef_t& in )
		{
			out.textureoffset	= Read::Short( in.textureoffset ) << FRACBITS;
			out.rowoffset		= Read::Short( in.rowoffset ) << FRACBITS;
			out.toptexture		= R_TextureNumForName( in.toptexture );
			out.bottomtexture	= R_TextureNumForName( in.bottomtexture );
			out.midtexture		= R_TextureNumForName( in.midtexture );
			out.sector			= &Sectors()[ Read::Short( in.sector ) ];
		} );

		_numsides = data.count;
		_sides = data.output;
	}

	void INLINE LoadLinedefs( int32_t lumpnum )
	{
		auto data = WadDataConvert< maplinedef_t, line_t >( lumpnum, 0, [ this ]( int32_t index, line_t& out, const maplinedef_t& in )
		{
			out.index			= index;
			out.flags			= Read::Short( in.flags );
			out.special			= Read::Short( in.special );
			out.tag				= Read::Short( in.tag );
			out.v1				= &Vertices()[ Read::Short( in.v1 ) ];
			out.v2				= &Vertices()[ Read::Short( in.v2 ) ];
			out.dx				= out.v2->x - out.v1->x;
			out.dy				= out.v2->y - out.v1->y;
	
			if( !out.dx )
			{
				out.slopetype = ST_VERTICAL;
			}
			else if ( !out.dy )
			{
				out.slopetype = ST_HORIZONTAL;
			}
			else if( FixedDiv( out.dy, out.dx ) > 0)
			{
				out.slopetype = ST_POSITIVE;
			}
			else
			{
				out.slopetype = ST_NEGATIVE;
			}
		
			if ( out.v1->x < out.v2->x )
			{
				out.bbox[ BOXLEFT ]		= out.v1->x;
				out.bbox[ BOXRIGHT ]	= out.v2->x;
			}
			else
			{
				out.bbox[ BOXLEFT ]		= out.v2->x;
				out.bbox[ BOXRIGHT ]	= out.v1->x;
			}

			if ( out.v1->y < out.v2->y )
			{
				out.bbox[ BOXBOTTOM ]	= out.v1->y;
				out.bbox[ BOXTOP ]		= out.v2->y;
			}
			else
			{
				out.bbox[ BOXBOTTOM ]	= out.v2->y;
				out.bbox[ BOXTOP ]		= out.v1->y;
			}

			// Bit of fiddling, but handling the side loading this
			// way and making line_t's sidenums int32s means we
			// keep vanilla compatibilty as well as limit removing
			// just by chaning the reader type. No other code will
			// need to change.
			auto side0 = Read::Short( in.sidenum[ 0 ] );
			auto side1 = Read::Short( in.sidenum[ 1 ] );
			constexpr auto INVALID_LINE = ( ( decltype( side0 ) )~0 );

			out.sidenum[ 0 ] = side0 != INVALID_LINE ? side0 : -1;
			out.sidenum[ 1 ] = side1 != INVALID_LINE ? side1 : -1;

			if( out.sidenum[ 0 ] != -1 )
			{
				out.frontsector = Sides()[ out.sidenum[ 0 ] ].sector;
			}
			else
			{
				out.frontsector = 0;
			}

			if( out.sidenum[ 1 ] != -1 )
			{
				out.backsector = Sides()[ out.sidenum[ 1 ] ].sector;
			}
			else
			{
				out.backsector = 0;
			}
		} );

		_numlines = data.count;
		_lines = data.output;
	}

	void INLINE LoadSubsectors( int32_t lumpnum )
	{
		auto data = WadDataConvert< mapsubsector_t, subsector_t >( lumpnum, 0, []( int32_t index, subsector_t& out, const mapsubsector_t& in )
		{
			out.numlines	= Read::Short( in.numsegs );
			out.firstline	= Read::Short( in.firstseg );
		} );

		_numsubsectors = data.count;
		_subsectors = data.output;
	}

	void INLINE LoadNodes( int32_t lumpnum, size_t offset = 0 )
	{
		auto data = WadDataConvert< mapnode_t, node_t >( lumpnum, offset, []( int32_t index, node_t& out, const mapnode_t& in )
		{
			out.x			= Read::Short( in.x ) << FRACBITS;
			out.y			= Read::Short( in.y ) << FRACBITS;
			out.dx			= Read::Short( in.dx ) << FRACBITS;
			out.dy			= Read::Short( in.dy ) << FRACBITS;
			for( int32_t child : std::ranges::views::iota( 0, 2 ) )
			{
				out.children[ child ] = Read::Short( in.children[ child ] );
				for ( int32_t corner : std::ranges::views::iota( 0, 4 ) )
				{
					out.bbox[ child ][ corner ] = Read::Short( in.bbox[ child ][ corner ] ) << FRACBITS;
				}
			}
		} );

		_numnodes		= data.count;
		_nodes			= data.output;
		_nodeformat		= NodeFormat::Vanilla;
	}

	void INLINE LoadExtendedNodes( int32_t lumpnum )
	{
		constexpr uint64_t Magic = 0x0000000034644E78ull;

		uint64_t offset = 0;
		nodeformat_t format = NodeFormat::Vanilla;

		uint64_t* data = (uint64_t*)W_CacheLumpNum( lumpnum, PU_STATIC );

		if( *data == Magic )
		{
			offset = 8;
			format = NodeFormat::DeepBSP;
		}

		LoadNodes( lumpnum, offset );
		_nodeformat = format;
	}

	void INLINE LoadSegs( int32_t lumpnum )
	{
		auto data = WadDataConvert< mapseg_t, seg_t >( lumpnum, 0, [ this ]( int32_t index, seg_t& out, const mapseg_t& in )
		{
			out.v1			= &Vertices()[ Read::Short( in.v1 ) ];
			out.v2			= &Vertices()[ Read::Short( in.v2 ) ];

			out.angle		= Read::Short( in.angle ) << FRACBITS;
			out.offset		= Read::Short( in.offset ) << FRACBITS;
			int32_t linenum	= Read::Short( in.linedef );
			line_t* linedef	= &Lines()[ linenum ];
			out.linedef		= linedef;
			int32_t side	= Read::Short( in.side );

			// e6y: check for wrong indexes
			if ( (uint32_t)linedef->sidenum[ side ] >= (uint32_t)Sides().size())
			{
				I_Error("P_LoadSegs: linedef %d for seg %d references a non-existent sidedef %d (out of %d)",
						linenum, index, (uint32_t)linedef->sidenum[ side ], (int32_t)Sides().size() );
			}

			out.sidedef		= &Sides()[ linedef->sidenum[ side ] ];
			out.frontsector	= Sides()[ linedef->sidenum[ side ] ].sector;

			if( linedef->flags & ML_TWOSIDED )
			{
				int32_t sidenum = linedef->sidenum[ side ^ 1 ];

				// If the sidenum is out of range, this may be a "glass hack"
				// impassible window.  Point at side #0 (this may not be
				// the correct Vanilla behavior; however, it seems to work for
				// OTTAWAU.WAD, which is the one place I've seen this trick
				// used).

				if ( sidenum < 0 || sidenum >= Sides().size() )
				{
					out.backsector = GetSectorAtNullAddress();
				}
				else
				{
					out.backsector = Sides()[ sidenum ].sector;
				}
			}
			else
			{
				out.backsector = 0;
			}
		} );

		_numsegs = data.count;
		_segs = data.output;
	}

	void INLINE GroupLines()
	{
		// look up sector number for each subsector
		for ( subsector_t& subsector : SubSectors() )
		{
			subsector.sector = Segs()[ subsector.firstline ].sidedef->sector;
		}

		// count number of lines in each sector
		for( line_t& line : Lines() )
		{
			++_totallines;
			++line.frontsector->linecount;

			if (line.backsector && line.backsector != line.frontsector)
			{
				line.backsector->linecount++;
				_totallines++;
			}
		}

		// build line tables for each sector
		line_t** linebuffer = (line_t**)Z_Malloc( _totallines * sizeof( line_t*), PU_LEVEL, 0 );

		for( sector_t& sector : Sectors() )
		{
			// Assign the line buffer for this sector
			sector.lines = linebuffer;
			linebuffer += sector.linecount;

			// Reset linecount to zero so in the next stage we can count
			// lines into the list.
			sector.linecount = 0;
		}

		// Assign lines to sectors
		for( line_t& line : Lines() )
		{ 
			if( line.frontsector != NULL )
			{
				sector_t* sector = line.frontsector;
				sector->lines[ sector->linecount++ ] = &line;
			}

			if ( line.backsector != NULL && line.frontsector != line.backsector)
			{
				sector_t* sector = line.backsector;
				sector->lines[ sector->linecount++ ] = &line;
			}
		}

		// Generate bounding boxes for sectors
		fixed_t		bbox[4] = { 0, 0, 0, 0 };
		int32_t		block = 0;

		for( sector_t& sector : Sectors() )
		{
			M_ClearBox (bbox);

			for( line_t* line : std::span( sector.lines, sector.linecount ) )
			{
				M_AddToBox( bbox, line->v1->x, line->v1->y );
				M_AddToBox( bbox, line->v2->x, line->v2->y );
			}

			// set the degenmobj_t to the middle of the bounding box
			sector.soundorg.x = ( bbox[ BOXRIGHT ] + bbox[ BOXLEFT ] ) / 2;
			sector.soundorg.y = ( bbox[ BOXTOP ] + bbox[ BOXBOTTOM ] ) / 2;
		
			// adjust bounding box to map blocks
			block = ( bbox[ BOXTOP ] - _blockmaporgy + MAXRADIUS ) >> MAPBLOCKSHIFT;
			block = block >= _blockmapheight ? _blockmapheight - 1 : block;
			sector.blockbox[ BOXTOP ] = block;

			block = ( bbox[ BOXBOTTOM ] - _blockmaporgy - MAXRADIUS ) >> MAPBLOCKSHIFT;
			block = block < 0 ? 0 : block;
			sector.blockbox[ BOXBOTTOM ] = block;

			block = ( bbox[ BOXRIGHT ] - _blockmaporgx + MAXRADIUS ) >> MAPBLOCKSHIFT;
			block = block >= _blockmapwidth ? _blockmapwidth - 1 : block;
			sector.blockbox[ BOXRIGHT ] = block;

			block = ( bbox[ BOXLEFT ] - _blockmaporgx - MAXRADIUS ) >> MAPBLOCKSHIFT;
			block = block < 0 ? 0 : block;
			sector.blockbox[ BOXLEFT ] = block;
		}

	}

	void INLINE LoadReject( int32_t lumpnum )
	{
		// Calculate the size that the REJECT lump *should* be.
		int32_t minlength = (int32_t)( (Sectors().size() * Sectors().size() + 7) / 8 );

		// If the lump meets the minimum length, it can be loaded directly.
		// Otherwise, we need to allocate a buffer of the correct size
		// and pad it with appropriate data.
		int32_t lumplen = W_LumpLength( lumpnum );

		if( lumplen >= minlength )
		{
			_rejectmatrix = ( byte* )W_CacheLumpNum( lumpnum, PU_LEVEL );
		}
		else
		{
			_rejectmatrix = ( byte* )Z_Malloc( minlength, PU_LEVEL, nullptr );
			W_ReadLump( lumpnum, _rejectmatrix );

			//PadRejectArray(rejectmatrix + lumplen, minlength - lumplen);

			uint32_t rejectpad[4] =
			{
				( ( (uint32_t)_totallines * 4 + 3 ) & ~3 ) + 24,		// Size
				0,													// Part of z_zone block header
				50,													// PU_LEVEL
				0x1d4a11											// DOOM_CONST_ZONEID
			};

			byte* dest = _rejectmatrix + lumplen;
			int32_t length = minlength - lumplen;
#ifdef SYS_BIG_ENDIAN
			for( int32_t index : std::iota( 0, M_MIN( length, sizeof( rejectpad ) ) ) )
			{
				byte_num = i % 4;
				*dest = (rejectpad[i / 4] >> (byte_num * 8)) & 0xff;
				++dest;
			}
#else
			memcpy( dest, rejectpad, M_MIN( length, sizeof( rejectpad ) ) );
#endif

			// We only have a limited pad size.  Print a warning if the
			// REJECT lump is too small.
			if( length > sizeof( rejectpad ) )
			{
				fprintf(stderr, "PadRejectArray: REJECT lump too short to pad! (%u > %i)\n",
								length, (int32_t)sizeof( rejectpad ) );

				// Pad remaining space with 0 (or 0xff, if specified on command line).
				byte padvalue = 0;
				if ( M_CheckParm( "-reject_pad_with_ff" ) )
				{
					padvalue = 0xff;
				}

				memset(	dest + sizeof( rejectpad ), padvalue, length - sizeof( rejectpad ) );
			}
		}
	}
};

extern "C"
{
	#include "deh_main.h"

	#include "g_game.h"

	#include "doomdef.h"

	#include "s_sound.h"

	#include "doomstat.h"

	#include "m_debugmenu.h"

	void	P_SpawnMapThing (mapthing_t*	mthing);

	//
	// MAP related Lookup tables.
	// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
	//
	int32_t		numvertexes;
	vertex_t*	vertexes;

	int32_t		numsegs;
	seg_t*		segs;

	int32_t		numsectors;
	sector_t*	sectors;

	int32_t		numsubsectors;
	subsector_t*	subsectors;

	int32_t		numnodes;
	node_t*		nodes;

	int32_t		numlines;
	line_t*		lines;

	int32_t		numsides;
	side_t*		sides;

	int32_t		totallines;

	// BLOCKMAP
	// Created from axis aligned bounding box
	// of the map, a rectangular array of
	// blocks of size ...
	// Used to speed up collision detection
	// by spatial subdivision in 2D.
	//
	// Blockmap size.
	int		bmapwidth;
	int		bmapheight;	// size in mapblocks
	short*		blockmap;	// int for larger maps
	// offsets in blockmap are from here
	short*		blockmaplump;		
	// origin of block map
	fixed_t		bmaporgx;
	fixed_t		bmaporgy;
	// for thing chains
	mobj_t**	blocklinks;		


	// REJECT
	// For fast sight rejection.
	// Speeds up enemy AI by skipping detailed
	//  LineOf Sight calculation.
	// Without special effect, this could be
	//  used as a PVS lookup as well.
	//
	byte*		rejectmatrix;


	// Maintain single and multi player starting spots.

	mapthing_t	deathmatchstarts[MAX_DEATHMATCH_STARTS];
	mapthing_t*	deathmatch_p;
	mapthing_t	playerstarts[MAXPLAYERS];
	boolean		playerstartsingame[MAXPLAYERS];

	// pointer to the current map lump info struct
	lumpinfo_t *maplumpinfo;
}

//
// P_LoadVertexes
//
void P_LoadVertexes (int lump)
{
    byte*		data;
    int			i;
    mapvertex_t*	ml;
    vertex_t*		li;

    // Determine number of lumps:
    //  total lump length / vertex record length.
    numvertexes = W_LumpLength (lump) / sizeof(mapvertex_t);

    // Allocate zone memory for buffer.
    vertexes = (vertex_t*)Z_Malloc (numvertexes*sizeof(vertex_t),PU_LEVEL,0);	

    // Load data into cache.
    data = (byte*)W_CacheLumpNum (lump, PU_STATIC);
	
    ml = (mapvertex_t *)data;
    li = vertexes;

    // Copy and convert vertex coordinates,
    // internal representation as fixed.
    for (i=0 ; i<numvertexes ; i++, li++, ml++)
    {
	li->x = SHORT(ml->x)<<FRACBITS;
	li->y = SHORT(ml->y)<<FRACBITS;
    }

    // Free buffer memory.
    W_ReleaseLumpNum(lump);
}

//
// GetSectorAtNullAddress
//
sector_t* GetSectorAtNullAddress(void)
{
    static boolean null_sector_is_initialized = false;
    static sector_t null_sector;

    if (!null_sector_is_initialized)
    {
        memset(&null_sector, 0, sizeof(null_sector));
        I_GetMemoryValue(0, &null_sector.floorheight, 4);
        I_GetMemoryValue(4, &null_sector.ceilingheight, 4);
        null_sector_is_initialized = true;
    }

    return &null_sector;
}

//
// P_LoadSegs
//
void P_LoadSegs (int lump)
{
    byte*		data;
    int			i;
    mapseg_t*		ml;
    seg_t*		li;
    line_t*		ldef;
    int			linedef;
    int			side;
    int                 sidenum;
	
    numsegs = W_LumpLength (lump) / sizeof(mapseg_t);
    segs = (seg_t*)Z_Malloc (numsegs*sizeof(seg_t),PU_LEVEL,0);	
    memset (segs, 0, numsegs*sizeof(seg_t));
    data = (byte*)W_CacheLumpNum (lump,PU_STATIC);
	
    ml = (mapseg_t *)data;
    li = segs;
    for (i=0 ; i<numsegs ; i++, li++, ml++)
    {
	li->v1 = &vertexes[SHORT(ml->v1)];
	li->v2 = &vertexes[SHORT(ml->v2)];

	li->angle = (SHORT(ml->angle))<<FRACBITS;
	li->offset = (SHORT(ml->offset))<<FRACBITS;
	linedef = SHORT(ml->linedef);
	ldef = &lines[linedef];
	li->linedef = ldef;
	side = SHORT(ml->side);

        // e6y: check for wrong indexes
        if ((uint32_t)ldef->sidenum[side] >= (uint32_t)numsides)
        {
            I_Error("P_LoadSegs: linedef %d for seg %d references a non-existent sidedef %d",
                    linedef, i, (uint32_t)ldef->sidenum[side]);
        }

	li->sidedef = &sides[ldef->sidenum[side]];
	li->frontsector = sides[ldef->sidenum[side]].sector;

        if (ldef-> flags & ML_TWOSIDED)
        {
            sidenum = ldef->sidenum[side ^ 1];

            // If the sidenum is out of range, this may be a "glass hack"
            // impassible window.  Point at side #0 (this may not be
            // the correct Vanilla behavior; however, it seems to work for
            // OTTAWAU.WAD, which is the one place I've seen this trick
            // used).

            if (sidenum < 0 || sidenum >= numsides)
            {
                li->backsector = GetSectorAtNullAddress();
            }
            else
            {
                li->backsector = sides[sidenum].sector;
            }
        }
        else
        {
	    li->backsector = 0;
        }
    }
	
    W_ReleaseLumpNum(lump);
}


//
// P_LoadSubsectors
//
void P_LoadSubsectors (int lump)
{
    byte*		data;
    int			i;
    mapsubsector_t*	ms;
    subsector_t*	ss;
	
    numsubsectors = W_LumpLength (lump) / sizeof(mapsubsector_t);
    subsectors = (subsector_t*)Z_Malloc (numsubsectors*sizeof(subsector_t),PU_LEVEL,0);	
    data = (byte*)W_CacheLumpNum (lump,PU_STATIC);
	
    ms = (mapsubsector_t *)data;
    memset (subsectors,0, numsubsectors*sizeof(subsector_t));
    ss = subsectors;
    
    for (i=0 ; i<numsubsectors ; i++, ss++, ms++)
    {
	ss->numlines = SHORT(ms->numsegs);
	ss->firstline = SHORT(ms->firstseg);
    }
	
    W_ReleaseLumpNum(lump);
}



//
// P_LoadSectors
//
void P_LoadSectors (int lump)
{
	byte*			data;
	int32_t			index;
	mapsector_t*	ms;
	sector_t*		ss;
	
	numsectors = W_LumpLength (lump) / sizeof(mapsector_t);
	sectors = (sector_t*)Z_Malloc (numsectors*sizeof(sector_t),PU_LEVEL,0);	
	memset (sectors, 0, numsectors*sizeof(sector_t));
	data = (byte*)W_CacheLumpNum (lump,PU_STATIC);
	
	ms = (mapsector_t *)data;
	ss = sectors;
	for (index=0 ; index<numsectors ; index++, ss++, ms++)
	{
		ss->index			= index;
		ss->floorheight		= SHORT(ms->floorheight)<<FRACBITS;
		ss->ceilingheight	= SHORT(ms->ceilingheight)<<FRACBITS;
		ss->floorpic		= R_FlatNumForName(ms->floorpic);
		ss->ceilingpic		= R_FlatNumForName(ms->ceilingpic);
		ss->lightlevel		= SHORT(ms->lightlevel);
		ss->special			= SHORT(ms->special);
		ss->tag				= SHORT(ms->tag);
		ss->thinglist		= NULL;

		if( ss->special == 9 )
		{
			ss->secretstate = Secret_Undiscovered;
		}
	}

	W_ReleaseLumpNum(lump);
}


//
// P_LoadNodes
//
void P_LoadNodes (int lump)
{
    byte*	data;
    int		i;
    int		j;
    int		k;
    mapnode_t*	mn;
    node_t*	no;
	
    numnodes = W_LumpLength (lump) / sizeof(mapnode_t);
    nodes = (node_t*)Z_Malloc (numnodes*sizeof(node_t),PU_LEVEL,0);	
    data = (byte*)W_CacheLumpNum (lump,PU_STATIC);
	
    mn = (mapnode_t *)data;
    no = nodes;
    
    for (i=0 ; i<numnodes ; i++, no++, mn++)
    {
	no->x = SHORT(mn->x)<<FRACBITS;
	no->y = SHORT(mn->y)<<FRACBITS;
	no->dx = SHORT(mn->dx)<<FRACBITS;
	no->dy = SHORT(mn->dy)<<FRACBITS;
	for (j=0 ; j<2 ; j++)
	{
	    no->children[j] = SHORT(mn->children[j]);
	    for (k=0 ; k<4 ; k++)
		no->bbox[j][k] = SHORT(mn->bbox[j][k])<<FRACBITS;
	}
    }
	
    W_ReleaseLumpNum(lump);
}


//
// P_LoadThings
//
void P_LoadThings (int lump)
{
    byte               *data;
    int			i;
    mapthing_t         *mt;
    mapthing_t          spawnthing;
    int			numthings;
    boolean		spawn;

    data = (byte*)W_CacheLumpNum (lump,PU_STATIC);
    numthings = W_LumpLength (lump) / sizeof(mapthing_t);
	
    mt = (mapthing_t *)data;
    for (i=0 ; i<numthings ; i++, mt++)
    {
	spawn = true;

	// Do not spawn cool, new monsters if !commercial
	if (gamemode != commercial)
	{
	    switch (SHORT(mt->type))
	    {
	      case 68:	// Arachnotron
	      case 64:	// Archvile
	      case 88:	// Boss Brain
	      case 89:	// Boss Shooter
	      case 69:	// Hell Knight
	      case 67:	// Mancubus
	      case 71:	// Pain Elemental
	      case 65:	// Former Human Commando
	      case 66:	// Revenant
	      case 84:	// Wolf SS
		spawn = false;
		break;
	    }
	}
	if (spawn == false)
	    break;

	// Do spawn all other stuff. 
	spawnthing.x = SHORT(mt->x);
	spawnthing.y = SHORT(mt->y);
	spawnthing.angle = SHORT(mt->angle);
	spawnthing.type = SHORT(mt->type);
	spawnthing.options = SHORT(mt->options);
	
	P_SpawnMapThing(&spawnthing);
    }

    if (!deathmatch)
    {
        for (i = 0; i < MAXPLAYERS; i++)
        {
            if (playeringame[i] && !playerstartsingame[i])
            {
                I_Error("P_LoadThings: Player %d start missing (vanilla crashes here)", i + 1);
            }
            playerstartsingame[i] = false;
        }
    }

    W_ReleaseLumpNum(lump);
}


//
// P_LoadLineDefs
// Also counts secret lines for intermissions.
//
void P_LoadLineDefs (int lump)
{
    byte*		data;
    int			i;
    maplinedef_t*	mld;
    line_t*		ld;
    vertex_t*		v1;
    vertex_t*		v2;
	
    numlines = W_LumpLength (lump) / sizeof(maplinedef_t);
    lines = (line_t*)Z_Malloc (numlines*sizeof(line_t),PU_LEVEL,0);	
    memset (lines, 0, numlines*sizeof(line_t));
    data = (byte*)W_CacheLumpNum (lump,PU_STATIC);
	
    mld = (maplinedef_t *)data;
    ld = lines;
    for (i=0 ; i<numlines ; i++, mld++, ld++)
    {
	ld->flags = SHORT(mld->flags);
	ld->special = SHORT(mld->special);
	ld->tag = SHORT(mld->tag);
	v1 = ld->v1 = &vertexes[SHORT(mld->v1)];
	v2 = ld->v2 = &vertexes[SHORT(mld->v2)];
	ld->dx = v2->x - v1->x;
	ld->dy = v2->y - v1->y;
	
	if (!ld->dx)
	    ld->slopetype = ST_VERTICAL;
	else if (!ld->dy)
	    ld->slopetype = ST_HORIZONTAL;
	else
	{
	    if (FixedDiv (ld->dy , ld->dx) > 0)
		ld->slopetype = ST_POSITIVE;
	    else
		ld->slopetype = ST_NEGATIVE;
	}
		
	if (v1->x < v2->x)
	{
	    ld->bbox[BOXLEFT] = v1->x;
	    ld->bbox[BOXRIGHT] = v2->x;
	}
	else
	{
	    ld->bbox[BOXLEFT] = v2->x;
	    ld->bbox[BOXRIGHT] = v1->x;
	}

	if (v1->y < v2->y)
	{
	    ld->bbox[BOXBOTTOM] = v1->y;
	    ld->bbox[BOXTOP] = v2->y;
	}
	else
	{
	    ld->bbox[BOXBOTTOM] = v2->y;
	    ld->bbox[BOXTOP] = v1->y;
	}

	ld->sidenum[0] = SHORT(mld->sidenum[0]);
	ld->sidenum[1] = SHORT(mld->sidenum[1]);

	if (ld->sidenum[0] != -1)
	    ld->frontsector = sides[ld->sidenum[0]].sector;
	else
	    ld->frontsector = 0;

	if (ld->sidenum[1] != -1)
	    ld->backsector = sides[ld->sidenum[1]].sector;
	else
	    ld->backsector = 0;
    }

    W_ReleaseLumpNum(lump);
}


//
// P_LoadSideDefs
//
void P_LoadSideDefs (int lump)
{
    byte*		data;
    int			i;
    mapsidedef_t*	msd;
    side_t*		sd;
	
    numsides = W_LumpLength (lump) / sizeof(mapsidedef_t);
    sides = (side_t*)Z_Malloc (numsides*sizeof(side_t),PU_LEVEL,0);	
    memset (sides, 0, numsides*sizeof(side_t));
    data = (byte*)W_CacheLumpNum (lump,PU_STATIC);
	
    msd = (mapsidedef_t *)data;
    sd = sides;
    for (i=0 ; i<numsides ; i++, msd++, sd++)
    {
	sd->textureoffset = SHORT(msd->textureoffset)<<FRACBITS;
	sd->rowoffset = SHORT(msd->rowoffset)<<FRACBITS;
	sd->toptexture = R_TextureNumForName(msd->toptexture);
	sd->bottomtexture = R_TextureNumForName(msd->bottomtexture);
	sd->midtexture = R_TextureNumForName(msd->midtexture);
	sd->sector = &sectors[SHORT(msd->sector)];
    }

    W_ReleaseLumpNum(lump);
}


//
// P_LoadBlockMap
//
void P_LoadBlockMap (int lump)
{
    int i;
    int count;
    int lumplen;

    lumplen = W_LumpLength(lump);
    count = lumplen / 2;
	
    blockmaplump = (short*)Z_Malloc(lumplen, PU_LEVEL, NULL);
    W_ReadLump(lump, blockmaplump);
    blockmap = blockmaplump + 4;

    // Swap all short integers to native byte ordering.
  
    for (i=0; i<count; i++)
    {
	blockmaplump[i] = SHORT(blockmaplump[i]);
    }
		
    // Read the header

    bmaporgx = blockmaplump[0]<<FRACBITS;
    bmaporgy = blockmaplump[1]<<FRACBITS;
    bmapwidth = blockmaplump[2];
    bmapheight = blockmaplump[3];
	
    // Clear out mobj chains

    count = sizeof(*blocklinks) * bmapwidth * bmapheight;
    blocklinks = (mobj_t**)Z_Malloc(count, PU_LEVEL, 0);
    memset(blocklinks, 0, count);
}



//
// P_GroupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
void P_GroupLines (void)
{
    line_t**		linebuffer;
    int			i;
    int			j;
    line_t*		li;
    sector_t*		sector;
    subsector_t*	ss;
    seg_t*		seg;
    fixed_t		bbox[4];
    int			block;
	
    // look up sector number for each subsector
    ss = subsectors;
    for (i=0 ; i<numsubsectors ; i++, ss++)
    {
	seg = &segs[ss->firstline];
	ss->sector = seg->sidedef->sector;
    }

    // count number of lines in each sector
    li = lines;
    totallines = 0;
    for (i=0 ; i<numlines ; i++, li++)
    {
	totallines++;
	li->frontsector->linecount++;

	if (li->backsector && li->backsector != li->frontsector)
	{
	    li->backsector->linecount++;
	    totallines++;
	}
    }

    // build line tables for each sector	
    linebuffer = (line_t**)Z_Malloc (totallines*sizeof(line_t *), PU_LEVEL, 0);

    for (i=0; i<numsectors; ++i)
    {
        // Assign the line buffer for this sector

        sectors[i].lines = linebuffer;
        linebuffer += sectors[i].linecount;

        // Reset linecount to zero so in the next stage we can count
        // lines into the list.

        sectors[i].linecount = 0;
    }

    // Assign lines to sectors

    for (i=0; i<numlines; ++i)
    { 
        li = &lines[i];

        if (li->frontsector != NULL)
        {
            sector = li->frontsector;

            sector->lines[sector->linecount] = li;
            ++sector->linecount;
        }

        if (li->backsector != NULL && li->frontsector != li->backsector)
        {
            sector = li->backsector;

            sector->lines[sector->linecount] = li;
            ++sector->linecount;
        }
    }
    
    // Generate bounding boxes for sectors
	
    sector = sectors;
    for (i=0 ; i<numsectors ; i++, sector++)
    {
	M_ClearBox (bbox);

	for (j=0 ; j<sector->linecount; j++)
	{
            li = sector->lines[j];

            M_AddToBox (bbox, li->v1->x, li->v1->y);
            M_AddToBox (bbox, li->v2->x, li->v2->y);
	}

	// set the degenmobj_t to the middle of the bounding box
	sector->soundorg.x = (bbox[BOXRIGHT]+bbox[BOXLEFT])/2;
	sector->soundorg.y = (bbox[BOXTOP]+bbox[BOXBOTTOM])/2;
		
	// adjust bounding box to map blocks
	block = (bbox[BOXTOP]-bmaporgy+MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block >= bmapheight ? bmapheight-1 : block;
	sector->blockbox[BOXTOP]=block;

	block = (bbox[BOXBOTTOM]-bmaporgy-MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block < 0 ? 0 : block;
	sector->blockbox[BOXBOTTOM]=block;

	block = (bbox[BOXRIGHT]-bmaporgx+MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block >= bmapwidth ? bmapwidth-1 : block;
	sector->blockbox[BOXRIGHT]=block;

	block = (bbox[BOXLEFT]-bmaporgx-MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block < 0 ? 0 : block;
	sector->blockbox[BOXLEFT]=block;
    }
	
}

// Pad the REJECT lump with extra data when the lump is too small,
// to simulate a REJECT buffer overflow in Vanilla Doom.

static void PadRejectArray(byte *array, uint32_t len)
{
    uint32_t i;
    uint32_t byte_num;
    byte *dest;
    uint32_t padvalue;

    // Values to pad the REJECT array with:

    uint32_t rejectpad[4] =
    {
        0,                                    // Size
        0,                                    // Part of z_zone block header
        50,                                   // PU_LEVEL
        0x1d4a11                              // DOOM_CONST_ZONEID
    };

    rejectpad[0] = ((totallines * 4 + 3) & ~3) + 24;

    // Copy values from rejectpad into the destination array.

    dest = array;

    for (i=0; i<len && i<sizeof(rejectpad); ++i)
    {
        byte_num = i % 4;
        *dest = (rejectpad[i / 4] >> (byte_num * 8)) & 0xff;
        ++dest;
    }

    // We only have a limited pad size.  Print a warning if the
    // REJECT lump is too small.

    if (len > sizeof(rejectpad))
    {
        fprintf(stderr, "PadRejectArray: REJECT lump too short to pad! (%u > %i)\n",
                        len, (int) sizeof(rejectpad));

        // Pad remaining space with 0 (or 0xff, if specified on command line).

        if (M_CheckParm("-reject_pad_with_ff"))
        {
            padvalue = 0xff;
        }
        else
        {
            padvalue = 0xf00;
        }

        memset(array + sizeof(rejectpad), padvalue, len - sizeof(rejectpad));
    }
}

static void P_LoadReject(int lumpnum)
{
    int minlength;
    int lumplen;

    // Calculate the size that the REJECT lump *should* be.

    minlength = (numsectors * numsectors + 7) / 8;

    // If the lump meets the minimum length, it can be loaded directly.
    // Otherwise, we need to allocate a buffer of the correct size
    // and pad it with appropriate data.

    lumplen = W_LumpLength(lumpnum);

    if (lumplen >= minlength)
    {
        rejectmatrix = (byte*)W_CacheLumpNum(lumpnum, PU_LEVEL);
    }
    else
    {
        rejectmatrix = (byte*)Z_Malloc(minlength, PU_LEVEL, &rejectmatrix);
        W_ReadLump(lumpnum, rejectmatrix);

        PadRejectArray(rejectmatrix + lumplen, minlength - lumplen);
    }
}

enum LoadingCode : int32_t
{
	Original,
	RnRVanilla,
	RnRLimitRemoving
};

int32_t loading_code = LoadingCode::RnRVanilla;

//
// P_SetupLevel
//
DOOM_C_API void
P_SetupLevel
( int		episode,
  int		map,
  int		playermask,
  skill_t	skill)
{
    int		i;
    char	lumpname[9];
    int		lumpnum;
	
    totalkills = totalitems = totalsecret = wminfo.maxfrags = 0;
    wminfo.partime = 180;
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	players[i].killcount = players[i].secretcount 
	    = players[i].itemcount = 0;
    }

    // Initial height of PointOfView
    // will be set by player think.
    players[consoleplayer].viewz = 1; 

    // Make sure all sounds are stopped before Z_FreeTags.
    S_Start ();			

    Z_FreeTags (PU_LEVEL, PU_PURGELEVEL-1);

    // UNUSED W_Profile ();
    P_InitThinkers ();

    // if working with a devlopment map, reload it
    W_Reload ();

	// find map name
	if ( gamemode == commercial)
	{
		DEH_snprintf(lumpname, 9, "MAP%02d", map);
	}
	else
	{
		DEH_snprintf( lumpname, 9, "E%dM%d", episode, map );
	}

    lumpnum = W_GetNumForName (lumpname);
	
    maplumpinfo = lumpinfo[lumpnum];

    leveltime = 0;
	
	switch( loading_code )
	{
	using enum LoadingCode;
	case Original:
		// note: most of this ordering is important	
		P_LoadBlockMap (lumpnum+ML_BLOCKMAP);
		P_LoadVertexes (lumpnum+ML_VERTEXES);
		P_LoadSectors (lumpnum+ML_SECTORS);
		P_LoadSideDefs (lumpnum+ML_SIDEDEFS);
		P_LoadLineDefs (lumpnum+ML_LINEDEFS);
		P_LoadSubsectors (lumpnum+ML_SSECTORS);
		P_LoadNodes (lumpnum+ML_NODES);
		P_LoadSegs (lumpnum+ML_SEGS);
		P_GroupLines ();
		P_LoadReject (lumpnum+ML_REJECT);
		break;

	case RnRVanilla:
		{
			DoomMapLoader< ReadVal_Vanilla > loader = DoomMapLoader< ReadVal_Vanilla >();

			loader.LoadBlockmap( lumpnum + ML_BLOCKMAP );
			loader.LoadVertices( lumpnum + ML_VERTEXES );
			loader.LoadSectors( lumpnum + ML_SECTORS );
			loader.LoadSidedefs( lumpnum + ML_SIDEDEFS );
			loader.LoadLinedefs( lumpnum + ML_LINEDEFS );
			loader.LoadSubsectors( lumpnum + ML_SSECTORS );
			loader.LoadNodes( lumpnum + ML_NODES );
			loader.LoadSegs( lumpnum + ML_SEGS );
			loader.GroupLines();
			loader.LoadReject( lumpnum + ML_REJECT );

			bmaporgx		= loader._blockmaporgx;
			bmaporgy		= loader._blockmaporgy;
			bmapwidth		= loader._blockmapwidth;
			bmapheight		= loader._blockmapheight;
			blockmap		= loader._blockmap;
			blocklinks		= loader._blocklinks;

			numvertexes		= loader._numvertices;
			vertexes		= loader._vertices;

			numsectors		= loader._numsectors;
			sectors			= loader._sectors;

			numsides		= loader._numsides;
			sides			= loader._sides;

			numlines		= loader._numlines;
			lines			= loader._lines;

			numsubsectors	= loader._numsubsectors;
			subsectors		= loader._subsectors;

			numnodes		= loader._numnodes;
			nodes			= loader._nodes;

			numsegs			= loader._numsegs;
			segs			= loader._segs;

			rejectmatrix	= loader._rejectmatrix;
		}
		break;

	case RnRLimitRemoving:
		{
			DoomMapLoader< ReadVal_LimitRemoving > loader = DoomMapLoader< ReadVal_LimitRemoving >();

			loader.LoadBlockmap( lumpnum + ML_BLOCKMAP );
			loader.LoadVertices( lumpnum + ML_VERTEXES );
			loader.LoadSectors( lumpnum + ML_SECTORS );
			loader.LoadSidedefs( lumpnum + ML_SIDEDEFS );
			loader.LoadLinedefs( lumpnum + ML_LINEDEFS );
			loader.LoadSubsectors( lumpnum + ML_SSECTORS );
			loader.LoadExtendedNodes( lumpnum + ML_NODES );
			loader.LoadSegs( lumpnum + ML_SEGS );
			loader.GroupLines();
			loader.LoadReject( lumpnum + ML_REJECT );

			bmaporgx		= loader._blockmaporgx;
			bmaporgy		= loader._blockmaporgy;
			bmapwidth		= loader._blockmapwidth;
			bmapheight		= loader._blockmapheight;
			blockmap		= loader._blockmap;
			blocklinks		= loader._blocklinks;

			numvertexes		= loader._numvertices;
			vertexes		= loader._vertices;

			numsectors		= loader._numsectors;
			sectors			= loader._sectors;

			numsides		= loader._numsides;
			sides			= loader._sides;

			numlines		= loader._numlines;
			lines			= loader._lines;

			numsubsectors	= loader._numsubsectors;
			subsectors		= loader._subsectors;

			numnodes		= loader._numnodes;
			nodes			= loader._nodes;

			numsegs			= loader._numsegs;
			segs			= loader._segs;

			rejectmatrix	= loader._rejectmatrix;
		}
		break;

	default:
		break;
	}

    bodyqueslot = 0;
    deathmatch_p = deathmatchstarts;
    P_LoadThings (lumpnum+ML_THINGS);
    
    // if deathmatch, randomly spawn the active players
    if (deathmatch)
    {
	for (i=0 ; i<MAXPLAYERS ; i++)
	    if (playeringame[i])
	    {
		players[i].mo = NULL;
		G_DeathMatchSpawnPlayer (i);
	    }
			
    }

    // clear special respawning que
    iquehead = iquetail = 0;		
	
    // set up world state
    P_SpawnSpecials ();
	
    // build subsector connect matrix
    //	UNUSED P_ConnectSubsectors ();

	R_RefreshContexts();
	R_PrecacheLevel();

    //printf ("free memory: 0x%x\n", Z_FreeMemory());

}


//
// P_Init
//
DOOM_C_API void P_Init (void)
{
    P_InitSwitchList ();
    P_InitPicAnims ();
    R_InitSprites (sprnames);

	if( M_CheckParm( "-removelimits" ) )
	{
		loading_code = LoadingCode::RnRLimitRemoving;
	}

	M_RegisterDebugMenuRadioButton( "Map|Load Path|Original", NULL, &loading_code, LoadingCode::Original );
	M_RegisterDebugMenuRadioButton( "Map|Load Path|R&R Vanilla", NULL, &loading_code, LoadingCode::RnRVanilla );
	M_RegisterDebugMenuRadioButton( "Map|Load Path|R&R Limit Removing", NULL, &loading_code, LoadingCode::RnRLimitRemoving );
}



