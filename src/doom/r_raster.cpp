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

#include "p_local.h"
#include "r_main.h"

#include "r_defs.h"
#include "r_state.h"
#include "m_misc.h"

INLINE void Rotate( rend_fixed_t& x, rend_fixed_t& y, uint32_t finerot )
{
	rend_fixed_t cos = renderfinecosine[ finerot ];
	rend_fixed_t sin = renderfinesine[ finerot ];

	rend_fixed_t ogx = x;
	rend_fixed_t ogy = y;

	x = RendFixedMul( cos, ogx ) - RendFixedMul( sin, ogy );
	y = RendFixedMul( sin, ogx ) + RendFixedMul( cos, ogy );
}

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

template< int32_t Leap, int32_t LeapLog2, typename Sampler, bool DoRotation >
requires ( LeapLog2 >= 2 && LeapLog2 <= 5 )
INLINE void R_RasteriseColumnImpl( rendercontext_t* rendercontext, int32_t x, int32_t top, int32_t count, uint32_t finerot )
{
	pixel_t*			dest			= rendercontext->planecontext.output.data + x * rendercontext->planecontext.output.pitch + top;
	pixel_t*			source			= rendercontext->planecontext.source;

	int32_t				nexty			= top;

	angle_t				angle			= RENDFINEANGLE( rendercontext->planecontext.viewangle + drs_current->xtoviewangle[ x ] );
	rend_fixed_t		anglecos		= renderfinecosine[ angle ];
	rend_fixed_t		anglesin		= renderfinesine[ angle ];

	rend_fixed_t		currdistance	= rendercontext->planecontext.raster[ top ].distance;
	rend_fixed_t		currlength		= RendFixedMul( currdistance, drs_current->distscale[ x ] );

	rend_fixed_t		xfrac			= rendercontext->planecontext.viewx + RendFixedMul( anglecos, currlength );
	rend_fixed_t		yfrac			= -rendercontext->planecontext.viewy - RendFixedMul( anglesin, currlength );
	if constexpr( DoRotation )
	{
		Rotate( xfrac, yfrac, finerot );
	}
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
		currdistance	= rendercontext->planecontext.raster[ nexty ].distance;
		currlength		= RendFixedMul( currdistance, drs_current->distscale[ x ] );
		nextxfrac		= rendercontext->planecontext.viewx + RendFixedMul( anglecos, currlength );
		nextyfrac		= -rendercontext->planecontext.viewy - RendFixedMul( anglesin, currlength );
		if constexpr( DoRotation )
		{
			Rotate( nextxfrac, nextyfrac, finerot );
		}

		xstep =	( nextxfrac - xfrac ) >> LeapLog2;
		ystep =	( nextyfrac - yfrac ) >> LeapLog2;

		if constexpr( LeapLog2 >= 5 )
		{
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
		}
		if constexpr( LeapLog2 >= 4 )
		{
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
		}
		if constexpr( LeapLog2 >= 3 )
		{
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
		}
		if constexpr( LeapLog2 >= 2 )
		{
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
		}

		xfrac = nextxfrac;
		yfrac = nextyfrac;

		count -= Leap;
	};

	if( count >= 0 )
	{
		nexty			+= count;
		currdistance	= rendercontext->planecontext.raster[ nexty ].distance;
		currlength		= RendFixedMul( currdistance, drs_current->distscale[ x ] );
		nextxfrac		= rendercontext->planecontext.viewx + RendFixedMul( anglecos, currlength );
		nextyfrac		= -rendercontext->planecontext.viewy - RendFixedMul( anglesin, currlength );
		if constexpr( DoRotation )
		{
			Rotate( nextxfrac, nextyfrac, finerot );
		}

		++count;

		xstep =	( nextxfrac - xfrac ) / count;
		ystep =	( nextyfrac - yfrac ) / count;

		do
		{
			Sampler::Sample( top, xfrac, xstep, yfrac, ystep, source, dest, rendercontext->planecontext );
		} while( top <= nexty );
	}

#if RENDER_PERF_GRAPHING
	endtime = I_GetTimeUS();
	rendercontext->planecontext.flattimetaken += ( endtime - starttime );
#endif // RENDER_PERF_GRAPHING

}

INLINE void PrepareRow( int32_t y, rendercontext_t* rendercontext, size_t size )
{
	planecontext_t& planecontext = rendercontext->planecontext;

	planecontext.raster[ y ].distance = RendFixedMul( planecontext.planeheight, drs_current->yslope[ y ] );

	if( fixedcolormapindex )
	{
		planecontext.raster[ y ].sourceoffset = fixedcolormapindex * size;
		planecontext.raster[ y ].colormap = rendercontext->viewpoint.colormaps + fixedcolormapindex * 256;
	}
	else
	{
		int32_t lookup = (int32_t)M_CLAMP( ( planecontext.raster[ y ].distance >> RENDLIGHTZSHIFT ), 0, ( MAXLIGHTZ - 1 ) );
		int32_t lightindex = drs_current->zlightindex[ planecontext.planezlightindex * MAXLIGHTZ + lookup ];
		planecontext.raster[ y ].sourceoffset = lightindex * size;
		planecontext.raster[ y ].colormap = rendercontext->viewpoint.colormaps + lightindex * 256;
	}
}

constexpr auto Lines( rasterregion_t* region )
{
	return std::span( region->lines, region->maxx - region->minx + 1 );
}

template< int32_t Leap, int32_t LeapLog2, typename Sampler, bool DoRotation >
requires ( LeapLog2 >= 2 && LeapLog2 <= 5 )
INLINE void RenderRasterLines( rendercontext_t* rendercontext, rasterregion_t* region )
{
	int32_t x = region->minx;
	uint32_t finerot = RENDFINEANGLE( region->rotation );

	for( rasterline_t& line : Lines( region ) )
	{
		if( line.top <= line.bottom )
		{
			R_RasteriseColumnImpl< Leap, LeapLog2, Sampler, DoRotation >( rendercontext, x, line.top, line.bottom - line.top, finerot );
		}
		++x;
	}
}

template< int32_t Leap, int32_t LeapLog2, typename Sampler >
requires ( LeapLog2 >= 2 && LeapLog2 <= 5 )
INLINE void PrepareRender( rendercontext_t* rendercontext, rasterregion_t* thisregion, size_t texturesize )
{
	rendercontext->planecontext.viewx		= rendercontext->viewpoint.x - thisregion->xoffset;
	rendercontext->planecontext.viewy		= rendercontext->viewpoint.y - thisregion->yoffset;
	rendercontext->planecontext.viewangle	= rendercontext->viewpoint.angle;

	rendercontext->planecontext.planeheight = abs( thisregion->height - rendercontext->viewpoint.z );
	int32_t light = M_CLAMP( ( ( thisregion->lightlevel >> LIGHTSEGSHIFT ) + extralight ), 0, LIGHTLEVELS - 1 );
	
	rendercontext->planecontext.planezlightindex = light;
	rendercontext->planecontext.planezlightoffset = &drs_current->zlightoffset[ light * MAXLIGHTZ ];

	int32_t y = thisregion->miny;
	int32_t stop = thisregion->maxy + 1;

	while( y < stop )
	{
		PrepareRow( y++, rendercontext, texturesize );
	};

	if( thisregion->rotation != 0 )
	{
		RenderRasterLines< Leap, LeapLog2, Sampler, true >( rendercontext, thisregion );
	}
	else
	{
		RenderRasterLines< Leap, LeapLog2, Sampler, false >( rendercontext, thisregion );
	}
}

template< typename Sampler, size_t tetxuresize >
void RasteriseRegion( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	rendercontext->planecontext.source = texture->data;

	switch( rendercontext->planecontext.spantype )
	{
	case Span_PolyRaster_Log2_4:
		PrepareRender< PLANE_PIXELLEAP_4, PLANE_PIXELLEAP_4_LOG2, Sampler >( rendercontext, firstregion, tetxuresize );
		break;

	case Span_PolyRaster_Log2_8:
		PrepareRender< PLANE_PIXELLEAP_8, PLANE_PIXELLEAP_8_LOG2, Sampler >( rendercontext, firstregion, tetxuresize );
		break;

	case Span_PolyRaster_Log2_16:
	case Span_PolyRaster_Log2_32:
		PrepareRender< PLANE_PIXELLEAP_16, PLANE_PIXELLEAP_16_LOG2, Sampler >( rendercontext, firstregion, tetxuresize );
		break;

	//case Span_PolyRaster_Log2_32:
	//	PrepareRender< PLANE_PIXELLEAP_32, PLANE_PIXELLEAP_32_LOG2, Sampler >( rendercontext, firstregion, tetxuresize );
	//	break;
	}
}


void R_RasteriseRegion16x16( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 16, 16 >, 16 * 16 >( rendercontext, firstregion, texture );
}

void R_RasteriseRegion16x32( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 16, 32 >, 16 * 32 >( rendercontext, firstregion, texture );
}

void R_RasteriseRegion16x64( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 16, 64 >, 16 * 64 >( rendercontext, firstregion, texture );
}


void R_RasteriseRegion16x128( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 16, 128 >, 16 * 128 >( rendercontext, firstregion, texture );
}



void R_RasteriseRegion32x16( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 32, 16 >, 32 * 16 >( rendercontext, firstregion, texture );
}


void R_RasteriseRegion32x32( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 32, 32 >, 32 * 32 >( rendercontext, firstregion, texture );
}


void R_RasteriseRegion32x64( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 32, 16 >, 32 * 64 >( rendercontext, firstregion, texture );
}


void R_RasteriseRegion32x128( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 32, 16 >, 32 * 128 >( rendercontext, firstregion, texture );
}



void R_RasteriseRegion64x16( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 64, 16 >, 64 * 16 >( rendercontext, firstregion, texture );
}


void R_RasteriseRegion64x32( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 64, 32 >, 64 * 64 >( rendercontext, firstregion, texture );
}


void R_RasteriseRegion64x64( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 64, 64 >, 64 * 128 >( rendercontext, firstregion, texture );
}


void R_RasteriseRegion64x128( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 64, 128 >, 64 * 16 >( rendercontext, firstregion, texture );
}



void R_RasteriseRegion128x16( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 128, 16 >, 128 * 16 >( rendercontext, firstregion, texture );
}


void R_RasteriseRegion128x32( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 128, 32 >, 128 * 32 >( rendercontext, firstregion, texture );
}


void R_RasteriseRegion128x64( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 128, 64 >, 128 * 64 >( rendercontext, firstregion, texture );
}


void R_RasteriseRegion128x128( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 128, 128 >, 128 * 128 >( rendercontext, firstregion, texture );
}



void R_RasteriseRegion256x16( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 256, 16 >, 256 * 16 >( rendercontext, firstregion, texture );
}


void R_RasteriseRegion256x32( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 256, 32 >, 256 * 32 >( rendercontext, firstregion, texture );
}


void R_RasteriseRegion256x64( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 256, 64 >, 256 * 64 >( rendercontext, firstregion, texture );
}


void R_RasteriseRegion256x128( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 256, 128 >, 256 * 128 >( rendercontext, firstregion, texture );
}



void R_RasteriseRegion512x16( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 512, 16 >, 512 * 16 >( rendercontext, firstregion, texture );
}


void R_RasteriseRegion512x32( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 512, 32 >, 512 * 32 >( rendercontext, firstregion, texture );
}


void R_RasteriseRegion512x64( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 512, 64 >, 512 * 64 >( rendercontext, firstregion, texture );
}


void R_RasteriseRegion512x128( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 512, 128 >, 512 * 128 >( rendercontext, firstregion, texture );
}



void R_RasteriseRegion1024x16( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 1024, 16 >, 1024 * 16 >( rendercontext, firstregion, texture );
}


void R_RasteriseRegion1024x32( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 1024, 32 >, 1024 * 32 >( rendercontext, firstregion, texture );
}


void R_RasteriseRegion1024x64( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 1024, 64 >, 1024 * 64 >( rendercontext, firstregion, texture );
}


void R_RasteriseRegion1024x128( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture )
{
	M_PROFILE_FUNC();
	RasteriseRegion< PreSizedSampleUntranslated< 1024, 128 >, 1024 * 128 >( rendercontext, firstregion, texture );
}
