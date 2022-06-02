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

#define MAXWIDTH			(MAXSCREENWIDTH + ( MAXSCREENWIDTH >> 1) )
#define MAXHEIGHT			(MAXSCREENHEIGHT + ( MAXSCREENHEIGHT >> 1) )

extern "C"
{
	#include "doomdef.h"
	#include "doomtype.h"
	#include "m_fixed.h"
	#include "m_misc.h"
	#include "r_defs.h"
	#include "r_main.h"
	#include "r_state.h"

	extern size_t		xlookup[ MAXWIDTH ];
	extern size_t		rowofs[ MAXHEIGHT ];
	extern fixed_t		distscale[ MAXSCREENWIDTH ];
	extern fixed_t		yslope[ MAXSCREENHEIGHT ];
}

INLINE void DoSample( int32_t& spot
					, int32_t& top
					, fixed_t& xfrac
					, fixed_t& xstep
					, fixed_t& yfrac
					, fixed_t& ystep
					, pixel_t*& source
					, pixel_t*& dest
					, planecontext_t*& planecontext
					, spancontext_t*& spancontext )
{
	spot = ( (yfrac & 0x3F0000 ) >> 10) | ( (xfrac & 0x3F0000 ) >> 16);
	source = spancontext->source + planecontext->raster[ top++ ].sourceoffset;
	*dest++ = source[spot];
	xfrac += xstep;
	yfrac += ystep;
}

// Used http://www.lysator.liu.se/~mikaelk/doc/perspectivetexture/ as reference for how to implement an efficient rasteriser
// Implemented for several Log2( N ) values, select based on backbuffer width

template< int32_t Leap, int32_t LeapLog2 >
requires ( LeapLog2 >= 2 && LeapLog2 <= 5 )
INLINE void R_RasteriseColumnImpl( planecontext_t* planecontext, spancontext_t* spancontext, int32_t x, int32_t top, int32_t count )
{
	pixel_t*			dest			= spancontext->output.data + xlookup[ x ] + rowofs[ top ];
	pixel_t*			source			= spancontext->source;

	int32_t				nexty			= top;

	angle_t				angle			= (viewangle + xtoviewangle[ x ] ) >> RENDERANGLETOFINESHIFT;
	fixed_t				anglecos		= renderfinecosine[ angle ];
	fixed_t				anglesin		= renderfinesine[ angle ];

	fixed_t				currdistance	= planecontext->raster[ top ].distance;
	fixed_t				currlength		= FixedMul( currdistance, distscale[ x ] );

	fixed_t				xfrac			= viewx + FixedMul( anglecos, currlength );
	fixed_t				yfrac			= -viewy - FixedMul( anglesin, currlength );
	fixed_t				nextxfrac;
	fixed_t				nextyfrac;

	fixed_t				xstep;
	fixed_t				ystep;

	int32_t				spot;

#if RENDER_PERF_GRAPHING
	uint64_t			starttime = I_GetTimeUS();
	uint64_t			endtime;
#endif // RENDER_PERF_GRAPHING

	while( count >= Leap )
	{
		nexty			+= Leap;
		currdistance	= planecontext->raster[ nexty ].distance;
		currlength		= FixedMul ( currdistance, distscale[ x ] );
		nextxfrac		= viewx + FixedMul( anglecos, currlength );
		nextyfrac		= -viewy - FixedMul( anglesin, currlength );

		xstep =	( nextxfrac - xfrac ) >> LeapLog2;
		ystep =	( nextyfrac - yfrac ) >> LeapLog2;

		if constexpr( LeapLog2 >= 5 )
		{
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
		}
		if constexpr( LeapLog2 >= 4 )
		{
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
		}
		if constexpr( LeapLog2 >= 3 )
		{
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
		}
		if constexpr( LeapLog2 >= 2 )
		{
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
		}

		xfrac = nextxfrac;
		yfrac = nextyfrac;

		count -= Leap;
	};

	if( count >= 0 )
	{
		nexty			+= count;
		currdistance	= planecontext->raster[ nexty ].distance;
		currlength		= FixedMul ( currdistance, distscale[ x ] );
		nextxfrac		= viewx + FixedMul( anglecos, currlength );
		nextyfrac		= -viewy - FixedMul( anglesin, currlength );

		++count;

		xstep =	( nextxfrac - xfrac ) / count;
		ystep =	( nextyfrac - yfrac ) / count;

		do
		{
			DoSample( spot, top, xfrac, xstep, yfrac, ystep, source, dest, planecontext, spancontext );
		} while( top <= nexty );
	}

#if RENDER_PERF_GRAPHING
	endtime = I_GetTimeUS();
	planecontext->flattimetaken += ( endtime - starttime );
#endif // RENDER_PERF_GRAPHING

}

INLINE void R_Prepare( int32_t y, visplane_t* visplane, planecontext_t* planecontext )
{
	planecontext->raster[ y ].distance		= FixedMul ( planecontext->planeheight, yslope[ y ] );

	// TODO: THIS LOGIC IS BROKEN>>>>>>>>>>>>>>>>>>
	//if( planecontext->planezlight != planecontext->raster[ y ].zlight )
	{
		if( fixedcolormapindex )
		{
			planecontext->raster[ y ].sourceoffset = fixedcolormapindex * 4096;
		}
		else
		{
			int32_t lightindex = M_CLAMP( ( planecontext->raster[ y ].distance >> LIGHTZSHIFT ), 0, ( MAXLIGHTZ - 1 ) );
			lightindex = zlightindex[ planecontext->planezlightindex ][ lightindex ];
			planecontext->raster[ y ].sourceoffset = lightindex * 4096;
		}

	}
}

INLINE void R_Prepare( visplane_t* visplane, planecontext_t* planecontext )
{
	int32_t y = visplane->miny;
	int32_t stop = visplane->maxy + 1;

	while( y < stop )
	{
		R_Prepare( y++, visplane, planecontext );
	};
}

DOOM_C_API void R_RasteriseColumns( spantype_t spantype, planecontext_t* planecontext, spancontext_t* spancontext, visplane_t* visplane )
{
	int32_t stop = visplane->maxx + 1;

	R_Prepare( visplane, planecontext );
	switch( spantype )
	{
	case Span_PolyRaster_Log2_4:
		for ( int32_t x = visplane->minx; x <= stop ; x++ )
		{
			if( visplane->top[ x ] <= visplane->bottom[ x ] )
			{
				R_RasteriseColumnImpl< PLANE_PIXELLEAP_4, PLANE_PIXELLEAP_4_LOG2 >( planecontext, spancontext, x, visplane->top[ x ], visplane->bottom[ x ] - visplane->top[ x ] );
			}
		}
		break;

	case Span_PolyRaster_Log2_8:
		for ( int32_t x = visplane->minx; x <= stop ; x++ )
		{
			if( visplane->top[ x ] <= visplane->bottom[ x ] )
			{
				R_RasteriseColumnImpl< PLANE_PIXELLEAP_8, PLANE_PIXELLEAP_8_LOG2 >( planecontext, spancontext, x, visplane->top[ x ], visplane->bottom[ x ] - visplane->top[ x ] );
			}
		}
		break;

	case Span_PolyRaster_Log2_16:
		for ( int32_t x = visplane->minx; x <= stop ; x++ )
		{
			if( visplane->top[ x ] <= visplane->bottom[ x ] )
			{
				R_RasteriseColumnImpl< PLANE_PIXELLEAP_16, PLANE_PIXELLEAP_16_LOG2 >( planecontext, spancontext, x, visplane->top[ x ], visplane->bottom[ x ] - visplane->top[ x ] );
			}
		}
		break;

	case Span_PolyRaster_Log2_32:
		for ( int32_t x = visplane->minx; x <= stop ; x++ )
		{
			if( visplane->top[ x ] <= visplane->bottom[ x ] )
			{
				R_RasteriseColumnImpl< PLANE_PIXELLEAP_32, PLANE_PIXELLEAP_32_LOG2 >( planecontext, spancontext, x, visplane->top[ x ], visplane->bottom[ x ] - visplane->top[ x ] );
			}
		}
		break;
	}
}
