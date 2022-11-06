//
// Copyright(C) 2020-2022 Ethan Watson
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
// DESCRIPTION: Perspective correct rasteriser
//

#include "doomdef.h"
#include "doomtype.h"
#include "m_fixed.h"
#include "doomstat.h"
#include "m_profile.h"
#include "r_main.h"

extern "C"
{
	#include "r_defs.h"
	#include "r_state.h"
	#include "m_misc.h"
}

template< int64_t Width, int64_t Height >
INLINE void DoSample( int32_t& top
					, rend_fixed_t& xfrac
					, rend_fixed_t& xstep
					, rend_fixed_t& yfrac
					, rend_fixed_t& ystep
					, pixel_t*& source
					, pixel_t*& dest
					, planecontext_t*& planecontext )
{
	constexpr int64_t YBitIndex = FirstSetBitIndex( RightmostBit( Height ) );
	constexpr int64_t XFracMask = IntToRendFixed( Width - 1 );
	constexpr int64_t YFracMask = IntToRendFixed( Height - 1 );
	constexpr int64_t XShift = RENDFRACBITS - YBitIndex;
	constexpr int64_t YShift = RENDFRACBITS;

	int32_t spot = ( ( yfrac & YFracMask ) >> YShift ) | ( ( xfrac & XFracMask ) >> XShift );
	*dest++ = ( source + planecontext->raster[ top++ ].sourceoffset )[ spot ];
	xfrac += xstep;
	yfrac += ystep;
}

// Used http://www.lysator.liu.se/~mikaelk/doc/perspectivetexture/ as reference for how to implement an efficient rasteriser
// Implemented for several Log2( N ) values, select based on backbuffer width

template< int32_t Leap, int32_t LeapLog2, int64_t Width, int64_t Height >
requires ( LeapLog2 >= 2 && LeapLog2 <= 5 )
INLINE void R_RasteriseColumnImpl( rend_fixed_t view_x, rend_fixed_t view_y, planecontext_t* planecontext, int32_t x, int32_t top, int32_t count )
{
	pixel_t*			dest			= planecontext->output.data + x * planecontext->output.pitch + top;
	pixel_t*			source			= planecontext->source;

	int32_t				nexty			= top;

	angle_t				angle			= (viewangle + drs_current->xtoviewangle[ x ] ) >> RENDERANGLETOFINESHIFT;
	rend_fixed_t		anglecos		= FixedToRendFixed( renderfinecosine[ angle ] );
	rend_fixed_t		anglesin		= FixedToRendFixed( renderfinesine[ angle ] );

	rend_fixed_t		currdistance	= planecontext->raster[ top ].distance;
	rend_fixed_t		currlength		= RendFixedMul( currdistance, drs_current->distscale[ x ] );

	rend_fixed_t		xfrac			= view_x + RendFixedMul( anglecos, currlength );
	rend_fixed_t		yfrac			= -view_y - RendFixedMul( anglesin, currlength );
	rend_fixed_t		nextxfrac;
	rend_fixed_t		nextyfrac;

	rend_fixed_t		xstep;
	rend_fixed_t		ystep;

#if RENDER_PERF_GRAPHING
	uint64_t			starttime = I_GetTimeUS();
	uint64_t			endtime;
#endif // RENDER_PERF_GRAPHING

	while( count >= Leap )
	{
		nexty			+= Leap;
		currdistance	= planecontext->raster[ nexty ].distance;
		currlength		= RendFixedMul( currdistance, drs_current->distscale[ x ] );
		nextxfrac		= view_x + RendFixedMul( anglecos, currlength );
		nextyfrac		= -view_y - RendFixedMul( anglesin, currlength );

		xstep =	( nextxfrac - xfrac ) >> LeapLog2;
		ystep =	( nextyfrac - yfrac ) >> LeapLog2;

		if constexpr( LeapLog2 >= 5 )
		{
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
		}
		if constexpr( LeapLog2 >= 4 )
		{
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
		}
		if constexpr( LeapLog2 >= 3 )
		{
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
		}
		if constexpr( LeapLog2 >= 2 )
		{
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
		}

		xfrac = nextxfrac;
		yfrac = nextyfrac;

		count -= Leap;
	};

	if( count >= 0 )
	{
		nexty			+= count;
		currdistance	= planecontext->raster[ nexty ].distance;
		currlength		= RendFixedMul( currdistance, drs_current->distscale[ x ] );
		nextxfrac		= view_x + RendFixedMul( anglecos, currlength );
		nextyfrac		= -view_y - RendFixedMul( anglesin, currlength );

		++count;

		xstep =	( nextxfrac - xfrac ) / count;
		ystep =	( nextyfrac - yfrac ) / count;

		do
		{
			DoSample< Width, Height >( top, xfrac, xstep, yfrac, ystep, source, dest, planecontext );
		} while( top <= nexty );
	}

#if RENDER_PERF_GRAPHING
	endtime = I_GetTimeUS();
	planecontext->flattimetaken += ( endtime - starttime );
#endif // RENDER_PERF_GRAPHING

}

template< int64_t Width, int64_t Height >
INLINE void R_Prepare( int32_t y, planecontext_t* planecontext )
{
	constexpr int64_t Size = Width * Height;
	planecontext->raster[ y ].distance		= RendFixedMul( FixedToRendFixed( planecontext->planeheight ), drs_current->yslope[ y ] );

	// TODO: THIS LOGIC IS BROKEN>>>>>>>>>>>>>>>>>>
	//if( planecontext->planezlight != planecontext->raster[ y ].zlight )
	{
		if( fixedcolormapindex )
		{
			planecontext->raster[ y ].sourceoffset = fixedcolormapindex * Size;
		}
		else
		{
			int32_t lookup = (int32_t)M_CLAMP( ( planecontext->raster[ y ].distance >> RENDLIGHTZSHIFT ), 0, ( MAXLIGHTZ - 1 ) );
			int32_t lightindex = drs_current->zlightindex[ planecontext->planezlightindex * MAXLIGHTZ + lookup ];
			planecontext->raster[ y ].sourceoffset = lightindex * Size;
		}

	}
}

template< int64_t Width, int64_t Height >
INLINE void R_Prepare( rasterregion_t* region, planecontext_t* planecontext )
{
	int32_t y = region->miny;
	int32_t stop = region->maxy + 1;

	while( y < stop )
	{
		R_Prepare< Width, Height >( y++, planecontext );
	};
}

constexpr auto Lines( rasterregion_t* region )
{
	return std::span( region->lines, region->maxx - region->minx + 1 );
}

template< int32_t Leap, int32_t LeapLog2, int64_t Width, int64_t Height >
requires ( LeapLog2 >= 2 && LeapLog2 <= 5 )
INLINE void ChooseRasteriser( planecontext_t* planecontext, rasterregion_t* region )
{
	rend_fixed_t view_x = FixedToRendFixed( viewx );
	rend_fixed_t view_y = FixedToRendFixed( viewy );

	int32_t stop = region->maxx + 1;
	int32_t x = region->minx;

	for( rasterline_t& line : Lines( region ) )
	{
		if( line.top <= line.bottom )
		{
			R_RasteriseColumnImpl< Leap, LeapLog2, Width, Height >( view_x, view_y, planecontext, x, line.top, line.bottom - line.top );
		}
		++x;
	}
}

template< int32_t Leap, int32_t LeapLog2, int64_t Width, int64_t Height >
requires ( LeapLog2 >= 2 && LeapLog2 <= 5 )
INLINE void ChooseRegionWidthHeightRasteriser( planecontext_t* planecontext, rasterregion_t* thisregion )
{
	{
		planecontext->planeheight = abs( RendFixedToFixed( thisregion->height ) - viewz );
		int32_t light = M_CLAMP( ( ( thisregion->lightlevel >> LIGHTSEGSHIFT ) + extralight ), 0, LIGHTLEVELS - 1 );
	
		planecontext->planezlightindex = light;
		planecontext->planezlight = &drs_current->zlight[ light * MAXLIGHTZ ];

		R_Prepare< Width, Height >( thisregion, planecontext );
		ChooseRasteriser< Leap, LeapLog2, Width, Height >( planecontext, thisregion );
	}
}

template< int32_t Leap, int32_t LeapLog2, int64_t Width >
requires ( LeapLog2 >= 2 && LeapLog2 <= 5 )
INLINE void ChooseRegionWidthRasteriser( planecontext_t* planecontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	switch( texture->height )
	{
	case 16:
		ChooseRegionWidthHeightRasteriser< Leap, LeapLog2, Width, 16 >( planecontext, firstregion );
		break;
	case 32:
		ChooseRegionWidthHeightRasteriser< Leap, LeapLog2, Width, 32 >( planecontext, firstregion );
		break;
	case 64:
		ChooseRegionWidthHeightRasteriser< Leap, LeapLog2, Width, 64 >( planecontext, firstregion );
		break;
	case 128:
		ChooseRegionWidthHeightRasteriser< Leap, LeapLog2, Width, 128 >( planecontext, firstregion );
		break;
	case 256:
		ChooseRegionWidthHeightRasteriser< Leap, LeapLog2, Width, 256 >( planecontext, firstregion );
		break;
	default:
		ChooseRegionWidthHeightRasteriser< Leap, LeapLog2, Width, 64 >( planecontext, firstregion );
		break;
	}
}

template< int32_t Leap, int32_t LeapLog2 >
requires ( LeapLog2 >= 2 && LeapLog2 <= 5 )
INLINE void ChooseRegionRasteriser( planecontext_t* planecontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	if( !remove_limits )
	{
		ChooseRegionWidthHeightRasteriser< Leap, LeapLog2, 64, 64 >( planecontext, firstregion );
	}
	else
	{
		switch( texture->width )
		{
		case 8:
			ChooseRegionWidthRasteriser< Leap, LeapLog2, 8 >( planecontext, firstregion, texture );
			break;
		case 16:
			ChooseRegionWidthRasteriser< Leap, LeapLog2, 16 >( planecontext, firstregion, texture );
			break;
		case 32:
			ChooseRegionWidthRasteriser< Leap, LeapLog2, 32 >( planecontext, firstregion, texture );
			break;
		case 64:
			ChooseRegionWidthRasteriser< Leap, LeapLog2, 64 >( planecontext, firstregion, texture );
			break;
		case 128:
			ChooseRegionWidthRasteriser< Leap, LeapLog2, 128 >( planecontext, firstregion, texture );
			break;
		case 256:
			ChooseRegionWidthRasteriser< Leap, LeapLog2, 256 >( planecontext, firstregion, texture );
			break;
		case 512:
			ChooseRegionWidthRasteriser< Leap, LeapLog2, 512 >( planecontext, firstregion, texture );
			break;
		case 1024:
			ChooseRegionWidthRasteriser< Leap, LeapLog2, 1024 >( planecontext, firstregion, texture );
			break;
		default:
			ChooseRegionWidthRasteriser< Leap, LeapLog2, 64 >( planecontext, firstregion, texture );
			break;
		}
	}
}

DOOM_C_API void R_RasteriseRegion( planecontext_t* planecontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	planecontext->source = texture->data;

	switch( planecontext->spantype )
	{
	case Span_PolyRaster_Log2_4:
		ChooseRegionRasteriser< PLANE_PIXELLEAP_4, PLANE_PIXELLEAP_4_LOG2 >( planecontext, firstregion, texture );
		break;

	case Span_PolyRaster_Log2_8:
		ChooseRegionRasteriser< PLANE_PIXELLEAP_8, PLANE_PIXELLEAP_8_LOG2 >( planecontext, firstregion, texture );
		break;

	case Span_PolyRaster_Log2_16:
		ChooseRegionRasteriser< PLANE_PIXELLEAP_16, PLANE_PIXELLEAP_16_LOG2 >( planecontext, firstregion, texture );
		break;

	case Span_PolyRaster_Log2_32:
		ChooseRegionRasteriser< PLANE_PIXELLEAP_32, PLANE_PIXELLEAP_32_LOG2 >( planecontext, firstregion, texture );
		break;
	}
}