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

#include "r_defs.h"
#include "r_state.h"
#include "m_misc.h"

template< int64_t Width, int64_t Height >
struct PreSizedSample
{
	static INLINE void Sample( int32_t& top
						, rend_fixed_t& xfrac
						, rend_fixed_t& xstep
						, rend_fixed_t& yfrac
						, rend_fixed_t& ystep
						, pixel_t*& source
						, pixel_t*& dest
						, planecontext_t& planecontext )
	{
		constexpr int64_t YBitIndex = FirstSetBitIndex( RightmostBit( Height ) );
		constexpr int64_t XFracMask = IntToRendFixed( Width - 1 );
		constexpr int64_t YFracMask = IntToRendFixed( Height - 1 );
		constexpr int64_t XShift = RENDFRACBITS - YBitIndex;
		constexpr int64_t YShift = RENDFRACBITS;

		int32_t spot = ( ( yfrac & YFracMask ) >> YShift ) | ( ( xfrac & XFracMask ) >> XShift );
		*dest++ = ( source + planecontext.raster[ top++ ].sourceoffset )[ spot ];
		xfrac += xstep;
		yfrac += ystep;
	}
};

template< int64_t Width, int64_t Height >
struct PreSizedSampleUntranslated
{
	static INLINE void Sample( int32_t& top
						, rend_fixed_t& xfrac
						, rend_fixed_t& xstep
						, rend_fixed_t& yfrac
						, rend_fixed_t& ystep
						, pixel_t*& source
						, pixel_t*& dest
						, planecontext_t& planecontext )
	{
		constexpr int64_t YBitIndex = FirstSetBitIndex( RightmostBit( Height ) );
		constexpr int64_t XFracMask = IntToRendFixed( Width - 1 );
		constexpr int64_t YFracMask = IntToRendFixed( Height - 1 );
		constexpr int64_t XShift = RENDFRACBITS - YBitIndex;
		constexpr int64_t YShift = RENDFRACBITS;

		const rastercache_t& thisraster = planecontext.raster[ top ];
		int32_t spot = ( ( yfrac & YFracMask ) >> YShift ) | ( ( xfrac & XFracMask ) >> XShift );
		*dest++ = thisraster.colormap[ source [ spot ] ];
		++top;
		xfrac += xstep;
		yfrac += ystep;
	}
};

// Used http://www.lysator.liu.se/~mikaelk/doc/perspectivetexture/ as reference for how to implement an efficient rasteriser
// Implemented for several Log2( N ) values, select based on backbuffer width

template< int32_t Leap, int32_t LeapLog2, typename Sampler >
requires ( LeapLog2 >= 2 && LeapLog2 <= 5 )
INLINE void R_RasteriseColumnImpl( rendercontext_t& rendercontext, int32_t x, int32_t top, int32_t count )
{
	pixel_t*			dest			= rendercontext.planecontext.output.data + x * rendercontext.planecontext.output.pitch + top;
	pixel_t*			source			= rendercontext.planecontext.source;

	int32_t				nexty			= top;

	angle_t				angle			= (rendercontext.planecontext.viewangle + drs_current->xtoviewangle[ x ] ) >> RENDERANGLETOFINESHIFT;
	rend_fixed_t		anglecos		= renderfinecosine[ angle ];
	rend_fixed_t		anglesin		= renderfinesine[ angle ];

	rend_fixed_t		currdistance	= rendercontext.planecontext.raster[ top ].distance;
	rend_fixed_t		currlength		= RendFixedMul( currdistance, drs_current->distscale[ x ] );

	rend_fixed_t		xfrac			= rendercontext.planecontext.viewx + RendFixedMul( anglecos, currlength );
	rend_fixed_t		yfrac			= -rendercontext.planecontext.viewy - RendFixedMul( anglesin, currlength );
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
		currdistance	= rendercontext.planecontext.raster[ nexty ].distance;
		currlength		= RendFixedMul( currdistance, drs_current->distscale[ x ] );
		nextxfrac		= rendercontext.planecontext.viewx + RendFixedMul( anglecos, currlength );
		nextyfrac		= -rendercontext.planecontext.viewy - RendFixedMul( anglesin, currlength );

		xstep =	( nextxfrac - xfrac ) >> LeapLog2;
		ystep =	( nextyfrac - yfrac ) >> LeapLog2;

		if constexpr( LeapLog2 >= 5 )
		{
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
		}
		if constexpr( LeapLog2 >= 4 )
		{
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
		}
		if constexpr( LeapLog2 >= 3 )
		{
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
		}
		if constexpr( LeapLog2 >= 2 )
		{
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
		}

		xfrac = nextxfrac;
		yfrac = nextyfrac;

		count -= Leap;
	};

	if( count >= 0 )
	{
		nexty			+= count;
		currdistance	= rendercontext.planecontext.raster[ nexty ].distance;
		currlength		= RendFixedMul( currdistance, drs_current->distscale[ x ] );
		nextxfrac		= rendercontext.planecontext.viewx + RendFixedMul( anglecos, currlength );
		nextyfrac		= -rendercontext.planecontext.viewy - RendFixedMul( anglesin, currlength );

		++count;

		xstep =	( nextxfrac - xfrac ) / count;
		ystep =	( nextyfrac - yfrac ) / count;

		do
		{
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext.planecontext );
		} while( top <= nexty );
	}

#if RENDER_PERF_GRAPHING
	endtime = I_GetTimeUS();
	rendercontext.planecontext.flattimetaken += ( endtime - starttime );
#endif // RENDER_PERF_GRAPHING

}

INLINE void PrepareRow( int32_t y, rendercontext_t& rendercontext, size_t size )
{
	planecontext_t& planecontext = rendercontext.planecontext;

	planecontext.raster[ y ].distance = RendFixedMul( planecontext.planeheight, drs_current->yslope[ y ] );

	if( fixedcolormapindex )
	{
		planecontext.raster[ y ].sourceoffset = fixedcolormapindex * size;
		planecontext.raster[ y ].colormap = rendercontext.viewpoint.colormaps + fixedcolormapindex * 256;
	}
	else
	{
		int32_t lookup = (int32_t)M_CLAMP( ( planecontext.raster[ y ].distance >> RENDLIGHTZSHIFT ), 0, ( MAXLIGHTZ - 1 ) );
		int32_t lightindex = drs_current->zlightindex[ planecontext.planezlightindex * MAXLIGHTZ + lookup ];
		planecontext.raster[ y ].sourceoffset = lightindex * size;
		planecontext.raster[ y ].colormap = rendercontext.viewpoint.colormaps + lightindex * 256;
	}
}

constexpr auto Lines( rasterregion_t* region )
{
	return std::span( region->lines, region->maxx - region->minx + 1 );
}

template< int32_t Leap, int32_t LeapLog2, typename Sampler >
requires ( LeapLog2 >= 2 && LeapLog2 <= 5 )
INLINE void RenderRasterLines( rendercontext_t& rendercontext, rasterregion_t* region )
{
	int32_t x = region->minx;

	for( rasterline_t& line : Lines( region ) )
	{
		if( line.top <= line.bottom )
		{
			R_RasteriseColumnImpl< Leap, LeapLog2, Sampler >( rendercontext, x, line.top, line.bottom - line.top );
		}
		++x;
	}
}

template< int32_t Leap, int32_t LeapLog2, typename Sampler >
requires ( LeapLog2 >= 2 && LeapLog2 <= 5 )
INLINE void PrepareRender( rendercontext_t& rendercontext, rasterregion_t* thisregion, size_t texturesize )
{
	rendercontext.planecontext.viewx		= rendercontext.viewpoint.x - thisregion->xoffset;
	rendercontext.planecontext.viewy		= rendercontext.viewpoint.y - thisregion->yoffset;
	rendercontext.planecontext.viewangle	= rendercontext.viewpoint.angle;

	rendercontext.planecontext.planeheight = abs( thisregion->height - rendercontext.viewpoint.z );
	int32_t light = M_CLAMP( ( ( thisregion->lightlevel >> LIGHTSEGSHIFT ) + extralight ), 0, LIGHTLEVELS - 1 );
	
	rendercontext.planecontext.planezlightindex = light;
	rendercontext.planecontext.planezlightoffset = &drs_current->zlightoffset[ light * MAXLIGHTZ ];

	int32_t y = thisregion->miny;
	int32_t stop = thisregion->maxy + 1;

	while( y < stop )
	{
		PrepareRow( y++, rendercontext, texturesize );
	};

	RenderRasterLines< Leap, LeapLog2, Sampler >( rendercontext, thisregion );
}

template< int32_t Leap, int32_t LeapLog2, int64_t Width >
requires ( LeapLog2 >= 2 && LeapLog2 <= 5 )
INLINE void ChooseRegionWidthRasteriser( rendercontext_t& rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	switch( texture->height )
	{
	case 16:
		PrepareRender< Leap, LeapLog2, PreSizedSampleUntranslated< Width, 16 > >( rendercontext, firstregion, Width * 16 );
		break;
	case 32:
		PrepareRender< Leap, LeapLog2, PreSizedSampleUntranslated< Width, 32 > >( rendercontext, firstregion, Width * 32 );
		break;
	case 64:
		PrepareRender< Leap, LeapLog2, PreSizedSampleUntranslated< Width, 64 > >( rendercontext, firstregion, Width * 64 );
		break;
	case 128:
		PrepareRender< Leap, LeapLog2, PreSizedSampleUntranslated< Width, 128 > >( rendercontext, firstregion, Width * 128 );
		break;
	case 256:
		PrepareRender< Leap, LeapLog2, PreSizedSampleUntranslated< Width, 256 > >( rendercontext, firstregion, Width * 256 );
		break;
	default:
		PrepareRender< Leap, LeapLog2, PreSizedSampleUntranslated< Width, 64 > >( rendercontext, firstregion, Width * 64 );
		break;
	}
}

template< int32_t Leap, int32_t LeapLog2 >
requires ( LeapLog2 >= 2 && LeapLog2 <= 5 )
INLINE void ChooseRegionRasteriser( rendercontext_t& rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	if( !remove_limits )
	{
		PrepareRender< Leap, LeapLog2, PreSizedSampleUntranslated< 64, 64 > >( rendercontext, firstregion, 64 * 64 );
	}
	else
	{
		switch( texture->width )
		{
		case 8:
			ChooseRegionWidthRasteriser< Leap, LeapLog2, 8 >( rendercontext, firstregion, texture );
			break;
		case 16:
			ChooseRegionWidthRasteriser< Leap, LeapLog2, 16 >( rendercontext, firstregion, texture );
			break;
		case 32:
			ChooseRegionWidthRasteriser< Leap, LeapLog2, 32 >( rendercontext, firstregion, texture );
			break;
		case 64:
			ChooseRegionWidthRasteriser< Leap, LeapLog2, 64 >( rendercontext, firstregion, texture );
			break;
		case 128:
			ChooseRegionWidthRasteriser< Leap, LeapLog2, 128 >( rendercontext, firstregion, texture );
			break;
		case 256:
			ChooseRegionWidthRasteriser< Leap, LeapLog2, 256 >( rendercontext, firstregion, texture );
			break;
		case 512:
			ChooseRegionWidthRasteriser< Leap, LeapLog2, 512 >( rendercontext, firstregion, texture );
			break;
		case 1024:
			ChooseRegionWidthRasteriser< Leap, LeapLog2, 1024 >( rendercontext, firstregion, texture );
			break;
		default:
			ChooseRegionWidthRasteriser< Leap, LeapLog2, 64 >( rendercontext, firstregion, texture );
			break;
		}
	}
}

void R_RasteriseRegion( rendercontext_t& rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	rendercontext.planecontext.source = texture->data;

	switch( rendercontext.planecontext.spantype )
	{
	case Span_PolyRaster_Log2_4:
		ChooseRegionRasteriser< PLANE_PIXELLEAP_4, PLANE_PIXELLEAP_4_LOG2 >( rendercontext, firstregion, texture );
		break;

	case Span_PolyRaster_Log2_8:
		ChooseRegionRasteriser< PLANE_PIXELLEAP_8, PLANE_PIXELLEAP_8_LOG2 >( rendercontext, firstregion, texture );
		break;

	case Span_PolyRaster_Log2_16:
		ChooseRegionRasteriser< PLANE_PIXELLEAP_16, PLANE_PIXELLEAP_16_LOG2 >( rendercontext, firstregion, texture );
		break;

	case Span_PolyRaster_Log2_32:
		ChooseRegionRasteriser< PLANE_PIXELLEAP_32, PLANE_PIXELLEAP_32_LOG2 >( rendercontext, firstregion, texture );
		break;
	}
}