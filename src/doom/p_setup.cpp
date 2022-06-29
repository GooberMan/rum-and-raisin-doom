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

// Abusing include guards to get the types I want
//#include "m_fixed.h"

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

static sector_t* GetSectorAtNullAddress(void)
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

#include <ranges>
#include <span>
#include <type_traits>
#include <vector>

// Support for something as simple as iota is incomplete in Clang hahaha
struct iota
{
	struct iterator
	{
		int32_t val;
		constexpr int32_t operator*() noexcept			{ return val; }
		bool INLINE operator!=( const iterator& rhs ) 	{ return val != rhs.val; }
		INLINE iterator& operator++()					{ ++val; return *this; }
	};

	iota( int32_t b, int32_t e )
	{
		_begin = { b };
		_end = { e };
	}

	constexpr iterator begin() noexcept { return _begin; }
	constexpr iterator end() noexcept { return _end; }

	iterator _begin;
	iterator _end;
};

struct ReadVal
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
				return ULong( val );
			}
			else
			{
				return Long( val );
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

	byte* rawlump = (byte*)W_CacheLumpNum( lumpnum, PU_STATIC );
	_from* rhs = (_from*)( rawlump + dataoffset );
	_to* lhs = data.output;

	for( int32_t curr : iota( 0, data.count ) )
	{
		func( curr, *lhs, *rhs );
		++lhs;
		++rhs;
	}

	W_ReleaseLumpNum( lumpnum );

	return data;
}

typedef enum class NodeFormat : int32_t
{
	Vanilla,
	Extended,
	DeepBSP,
} nodeformat_t;

struct DoomMapLoader
{
	using Read = ReadVal;

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
	blockmap_t*		_blockmap;
	blockmap_t*		_blockmapend;
	blockmap_t*		_blockmapbase;
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

	void INLINE DetermineExtendedFormat( int32_t rootlump )
	{
		constexpr uint64_t Magic = 0x0000000034644E78ull;
		byte* data = (byte*)W_CacheLumpNum( rootlump + ML_NODES, PU_STATIC );

		if( *(uint64_t*)data == Magic )
		{
			_nodeformat = NodeFormat::DeepBSP;
		}
		else
		{
			_nodeformat = NodeFormat::Extended;
		}
	}

	template< typename _maptype = mapblockmap_t >
	void INLINE LoadBlockmap( int32_t lumpnum )
	{
		auto data = WadDataConvert< _maptype, blockmap_t >( lumpnum, 0, []( int32_t index, blockmap_t& out, const _maptype& in )
		{
			auto val = Read::AsIs( in );
			out = ( val == BLOCKMAP_INVALID_VANILLA ? BLOCKMAP_INVALID : val );
		} );

		_blockmap			= data.output;
		_blockmapend		= data.output + data.count;
		_blockmapbase		= _blockmap;

		_blockmaporgx		= IntToFixed( *_blockmap++ );
		_blockmaporgy		= IntToFixed( *_blockmap++ );
		_blockmapwidth		= *_blockmap++;
		_blockmapheight		= *_blockmap++;

		int32_t linkssize = sizeof(*_blocklinks) * _blockmapwidth * _blockmapheight;
		_blocklinks = (mobj_t**)Z_Malloc( linkssize, PU_LEVEL, 0 );
		memset( _blocklinks, 0, linkssize );
	}

	void INLINE LoadExtendedBlockmap( int32_t lumpnum )
	{
		switch( _nodeformat )
		{
		using enum NodeFormat;
		case Extended:
		case DeepBSP:
			LoadBlockmap< mapblockmap_extended_t >( lumpnum );
			if( _blockmapwidth * _blockmapheight > 0xFFFF )
			{
				int32_t numindices = _blockmapwidth * _blockmapheight;
				_blockmapbase = _blockmap + numindices;

//#define DOING_REVERSE_ENGINEERING
#ifdef DOING_REVERSE_ENGINEERING
				ptrdiff_t listentries = _blockmapend - _blockmap;
				int32_t numactualentries = 0;
				for( blockmap_t* curr = _blockmapbase, *end = _blockmapend; curr != end; ++curr )
				{
					if( *curr == BLOCKMAP_INVALID )
					{
						++numactualentries;
					}
				}
				int32_t numuniqueentries = 0;
				std::vector< int32_t > found;
				found.reserve( numindices );
				for( blockmap_t* curr = _blockmap, *end = _blockmap + numindices; curr != end; ++curr )
				{
					if( std::find( found.begin(), found.end(), *curr ) == found.end() )
					{
						found.push_back( *curr );
						++numuniqueentries;
					}
				}
#endif
			}
			break;
		case Vanilla:
		default:
			LoadBlockmap( lumpnum );
			break;
		}
	}

	void INLINE LoadVertices( int32_t lumpnum )
	{
		auto data = WadDataConvert< mapvertex_t, vertex_t >( lumpnum, 0, []( int32_t index, vertex_t& out, const mapvertex_t& in )
		{
			out.x			= IntToFixed( Read::AsIs( in.x ) );
			out.y			= IntToFixed( Read::AsIs( in.y ) );

			out.rend.x		= FixedToRendFixed( out.x );
			out.rend.y		= FixedToRendFixed( out.y );
		} );

		_numvertices		= data.count;
		_vertices			= data.output;
	}

	void INLINE LoadSectors( int32_t lumpnum )
	{
		auto data = WadDataConvert< mapsector_t, sector_t >( lumpnum, 0, []( int32_t index, sector_t& out, const mapsector_t& in )
		{
			out.index			= index;
			out.floorheight		= IntToFixed( Read::AsIs( in.floorheight ) );
			out.ceilingheight	= IntToFixed( Read::AsIs( in.ceilingheight ) );
			out.floorpic		= R_FlatNumForName( in.floorpic );
			out.ceilingpic		= R_FlatNumForName( in.ceilingpic );
			out.lightlevel		= Read::AsIs( in.lightlevel );
			out.special			= Read::AsIs( in.special );
			out.tag				= Read::AsIs( in.tag );
			out.thinglist		= NULL;
			out.secretstate		= out.special == 9 ? Secret_Undiscovered : Secret_None;

			out.rend.floorheight = FixedToRendFixed( out.floorheight );
			out.rend.ceilingheight = FixedToRendFixed( out.ceilingheight );
		} );

		_numsectors				= data.count;
		_sectors				= data.output;
	}

	void INLINE LoadSidedefs( int32_t lumpnum )
	{
		auto data = WadDataConvert< mapsidedef_t, side_t >( lumpnum, 0, [ this ]( int32_t index, side_t& out, const mapsidedef_t& in )
		{
			out.textureoffset	= IntToFixed( Read::AsIs( in.textureoffset ) );
			out.rowoffset		= IntToFixed( Read::AsIs( in.rowoffset ) );
			out.toptexture		= R_TextureNumForName( in.toptexture );
			out.bottomtexture	= R_TextureNumForName( in.bottomtexture );
			out.midtexture		= R_TextureNumForName( in.midtexture );
			out.sector			= &Sectors()[ Read::AsIs( in.sector ) ];

			out.rend.textureoffset = FixedToRendFixed( out.textureoffset );
			out.rend.rowoffset = FixedToRendFixed( out.rowoffset );
		} );

		_numsides = data.count;
		_sides = data.output;
	}

	template< typename _maptype = maplinedef_t >
	void INLINE LoadLinedefs( int32_t lumpnum )
	{
		auto data = WadDataConvert< _maptype, line_t >( lumpnum, 0, [ this ]( int32_t index, line_t& out, const _maptype& in )
		{
			out.index			= index;
			out.flags			= Read::AsIs( in.flags );
			out.special			= Read::AsIs( in.special );
			out.tag				= Read::AsIs( in.tag );
			out.v1				= &Vertices()[ Read::AsIs( in.v1 ) ];
			out.v2				= &Vertices()[ Read::AsIs( in.v2 ) ];
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

			// Sanity checking like this allows code to be reused
			// for vanilla _and_ limit removing
			auto side0 = Read::AsIs( in.sidenum[ 0 ] );
			auto side1 = Read::AsIs( in.sidenum[ 1 ] );
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

	void INLINE LoadExtendedLinedefs( int32_t lumpnum )
	{
		LoadLinedefs< maplinedef_limitremoving_t >( lumpnum );
	}

	template< typename _maptype = mapsubsector_t >
	void INLINE LoadSubsectors( int32_t lumpnum )
	{
		auto data = WadDataConvert< _maptype, subsector_t >( lumpnum, 0, []( int32_t index, subsector_t& out, const _maptype& in )
		{
			out.numlines	= Read::AsIs( in.numsegs );
			out.firstline	= Read::AsIs( in.firstseg );
			out.index		= index;
		} );

		_numsubsectors = data.count;
		_subsectors = data.output;
	}

	void INLINE LoadExtendedSubsectors( int32_t lumpnum )
	{
		if( _nodeformat == NodeFormat::DeepBSP )
		{
			LoadSubsectors< mapsubsector_deepbsp_t >( lumpnum );
		}
		else
		{
			LoadSubsectors< mapsubsector_limitremoving_t >( lumpnum );
		}
	}

	template< typename _maptype = mapnode_t >
	void INLINE LoadNodes( int32_t lumpnum, size_t offset = 0 )
	{
		auto data = WadDataConvert< _maptype, node_t >( lumpnum, offset, []( int32_t index, node_t& out, const _maptype& in )
		{
			out.divline.x			= IntToFixed( Read::AsIs( in.x ) );
			out.divline.y			= IntToFixed( Read::AsIs( in.y ) );
			out.divline.dx			= IntToFixed( Read::AsIs( in.dx ) );
			out.divline.dy			= IntToFixed( Read::AsIs( in.dy ) );
			for( int32_t child : iota( 0, 2 ) )
			{
				auto childval = Read::AsIs( in.children[ child ] );

				if constexpr( sizeof( decltype( childval ) ) == 2 )
				{
					if( childval & NF_SUBSECTOR_VANILLA )
					{
						out.children[ child ] = ( childval & ~NF_SUBSECTOR_VANILLA );
						out.children[ child ] |= NF_SUBSECTOR;
					}
					else
					{
						out.children[ child ] = childval;
					}
				}
				else
				{
					out.children[ child ] = childval;
				}

				for ( int32_t corner : iota( 0, 4 ) )
				{
					out.bbox[ child ][ corner ] = IntToFixed( Read::AsIs( in.bbox[ child ][ corner ] ) );
				}
			}

			out.rend.x			= FixedToRendFixed( out.divline.x );
			out.rend.y			= FixedToRendFixed( out.divline.y );
			out.rend.dx			= FixedToRendFixed( out.divline.dx );
			out.rend.dy			= FixedToRendFixed( out.divline.dy );
			for( int32_t child : iota( 0, 2 ) )
			{
				for ( int32_t corner : iota( 0, 4 ) )
				{
					out.rend.bbox[ child ][ corner ] = FixedToRendFixed( out.bbox[ child ][ corner ] );
				}
			}

		} );

		_numnodes		= data.count;
		_nodes			= data.output;
	}

	void INLINE LoadExtendedNodes( int32_t lumpnum )
	{
		constexpr uint64_t Magic = 0x0000000034644E78ull;
		byte* data = (byte*)W_CacheLumpNum( lumpnum, PU_STATIC );

		if( _nodeformat == NodeFormat::DeepBSP )
		{
			LoadNodes< mapnode_deepbsp_t >( lumpnum, 8 );
		}
		else
		{
			LoadNodes( lumpnum );
		}
	}

	template< typename _maptype = mapseg_t >
	void INLINE LoadSegs( int32_t lumpnum )
	{
		auto data = WadDataConvert< _maptype, seg_t >( lumpnum, 0, [ this ]( int32_t index, seg_t& out, const _maptype& in )
		{
			out.v1			= &Vertices()[ Read::AsIs( in.v1 ) ];
			out.v2			= &Vertices()[ Read::AsIs( in.v2 ) ];

			out.angle		= IntToFixed( Read::AsIs( in.angle ) );
			out.offset		= IntToFixed( Read::AsIs( in.offset ) );
			int32_t linenum	= Read::AsIs( in.linedef );
			line_t* linedef	= &Lines()[ linenum ];
			out.linedef		= linedef;
			int32_t side	= Read::AsIs( in.side );

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

				if ( sidenum < 0 || sidenum >= (int32_t)Sides().size() )
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

			out.rend.offset = FixedToRendFixed( out.offset );
		} );

		_numsegs = data.count;
		_segs = data.output;
	}

	void INLINE LoadExtendedSegs( int32_t lumpnum )
	{
		if( _nodeformat == NodeFormat::DeepBSP )
		{
			LoadSegs< mapseg_deepbsp_t >( lumpnum );
		}
		else
		{
			LoadSegs( lumpnum );
		}
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
	int32_t			bmapwidth;
	int32_t			bmapheight;	// size in mapblocks
	blockmap_t*		blockmap;	// int for larger maps
	// offsets in blockmap are from here
	blockmap_t*		blockmapbase;
	// origin of block map
	fixed_t			bmaporgx;
	fixed_t			bmaporgy;
	// for thing chains
	mobj_t**		blocklinks;		


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

enum LoadingCode : int32_t
{
	RnRVanilla,
	RnRLimitRemoving
};

int32_t loading_code = LoadingCode::RnRVanilla;

//
// P_LoadThings
//
static void P_LoadThings (int lump)
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
	
	DoomMapLoader loader = DoomMapLoader();

	switch( loading_code )
	{
	using enum LoadingCode;
	case RnRVanilla:
		{
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
		}
		break;

	case RnRLimitRemoving:
		{
			loader.DetermineExtendedFormat( lumpnum );
			loader.LoadExtendedBlockmap( lumpnum + ML_BLOCKMAP );
			loader.LoadVertices( lumpnum + ML_VERTEXES );
			loader.LoadSectors( lumpnum + ML_SECTORS );
			loader.LoadSidedefs( lumpnum + ML_SIDEDEFS );
			loader.LoadExtendedLinedefs( lumpnum + ML_LINEDEFS );
			// Reordering for limit removing, subsectors are independent
			// of other data but need different structs depending on
			// extended node type.
			loader.LoadExtendedSubsectors( lumpnum + ML_SSECTORS );
			loader.LoadExtendedNodes( lumpnum + ML_NODES );
			loader.LoadExtendedSegs( lumpnum + ML_SEGS );
			loader.GroupLines();
			loader.LoadReject( lumpnum + ML_REJECT );
		}
		break;

	default:
		I_Error( "Unknown loading path for map" );
		break;
	}

	bmaporgx		= loader._blockmaporgx;
	bmaporgy		= loader._blockmaporgy;
	bmapwidth		= loader._blockmapwidth;
	bmapheight		= loader._blockmapheight;
	blockmap		= loader._blockmap;
	blockmapbase	= loader._blockmapbase;
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

	// The original code officially doesn't work right now. But it's about time to delete it anyway...
	//M_RegisterDebugMenuRadioButton( "Map|Load Path|Original", NULL, &loading_code, LoadingCode::Original );
	M_RegisterDebugMenuRadioButton( "Map|Load Path|Vanilla", NULL, &loading_code, LoadingCode::RnRVanilla );
	M_RegisterDebugMenuRadioButton( "Map|Load Path|Limit Removing", NULL, &loading_code, LoadingCode::RnRLimitRemoving );
}



