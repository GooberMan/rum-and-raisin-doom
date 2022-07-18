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
//	Here is a core component: drawing the floors and ceilings,
//	 while maintaining a per column clipping list only.
//	Moreover, the sky areas have to be determined.
//

#include "doomdef.h"
#include "doomtype.h"
#include "m_fixed.h"
#include "doomstat.h"

#include "r_local.h"
#include "r_raster.h"
#include "r_sky.h"

extern "C"
{
	#include <stdio.h>
	#include <stdlib.h>

	#include "i_system.h"
	#include "z_zone.h"
	#include "w_wad.h"

	#include "m_misc.h"

	//
	// Constants. Don't need to be in a context. Will get them off the stack at some point though.
	//
	#define MAXWIDTH	( MAXSCREENWIDTH + ( MAXSCREENWIDTH >> 1) )
	#define MAXHEIGHT	( MAXSCREENHEIGHT + ( MAXSCREENHEIGHT >> 1) )

	rend_fixed_t		yslope[ MAXSCREENHEIGHT ];
	rend_fixed_t		distscale[ MAXSCREENWIDTH ];

	int32_t				span_override = Span_None;

	extern size_t		xlookup[MAXWIDTH];
	extern size_t		rowofs[MAXHEIGHT];

	extern int			numflats;
	extern int			numtextures;
}

#include "m_container.h"
#include "m_profile.h"

//
// R_InitPlanes
// Only at game startup.
//
DOOM_C_API void R_InitPlanes (void)
{
  // Doh!
}


//
// R_MapPlane
//
// Uses global vars:
//  planeheight
//  ds_source
//  basexscale
//  baseyscale
//  viewx
//  viewy
//
// BASIC PRIMITIVE
//
DOOM_C_API void R_MapPlane( planecontext_t* planecontext, spancontext_t* spancontext, int32_t y, int32_t x1, int32_t x2 )
{
	angle_t		angle;
	fixed_t		distance;
	fixed_t		length;
	uint32_t	index;
	byte*		originalsource = spancontext->source;

#if RENDER_PERF_GRAPHING
	uint64_t			starttime = I_GetTimeUS();
	uint64_t			endtime;
#endif // RENDER_PERF_GRAPHING

	
#ifdef RANGECHECK
	if (x2 < x1
		|| x1 < 0
		|| x2 >= viewwidth
		|| y > viewheight)
	{
		I_Error ("R_MapPlane: %i, %i at %i",x1,x2,y);
	}
#endif

	if (planecontext->planeheight != planecontext->cachedheight[y])
	{
		planecontext->cachedheight[y] = planecontext->planeheight;
		distance = planecontext->cacheddistance[y] = FixedMul (planecontext->planeheight, RendFixedToFixed( yslope[y] ));
		spancontext->xstep = planecontext->cachedxstep[y] = FixedMul (distance, planecontext->basexscale);
		spancontext->ystep = planecontext->cachedystep[y] = FixedMul (distance, planecontext->baseyscale);
	}
	else
	{
		distance = planecontext->cacheddistance[y];
		spancontext->xstep = planecontext->cachedxstep[y];
		spancontext->ystep = planecontext->cachedystep[y];
	}
	
	length = FixedMul (distance,RendFixedToFixed(distscale[x1]));
	angle = (viewangle + xtoviewangle[x1])>>RENDERANGLETOFINESHIFT;
	spancontext->xfrac = viewx + FixedMul(renderfinecosine[angle], length);
	spancontext->yfrac = -viewy - FixedMul(renderfinesine[angle], length);

	if( fixedcolormapindex )
	{
		// TODO: This should be a real define somewhere
		spancontext->source += ( 4096 * fixedcolormapindex );
	}
	else
	{
		index = distance >> LIGHTZSHIFT;
		if (index >= MAXLIGHTZ )
			index = MAXLIGHTZ-1;

		index = zlightindex[planecontext->planezlightindex][index];

		spancontext->source += ( 4096 * index );
	}

	spancontext->y = y;
	spancontext->x1 = x1;
	spancontext->x2 = x2;

#ifdef RANGECHECK
	if (spancontext->x2 < spancontext->x1
		|| spancontext->x1<0
		|| spancontext->x2>=render_width
		|| spancontext->y>render_height)
	{
		I_Error( "R_MapPlane: Attempting to draw span %i to %i at %i",
			 spancontext->x1,spancontext->x2,spancontext->y);
	}
#endif

	// high or low detail
	spanfunc( spancontext );

	spancontext->source = originalsource;

#if RENDER_PERF_GRAPHING
	endtime = I_GetTimeUS();
	planecontext->flattimetaken += ( endtime - starttime );
#endif // RENDER_PERF_GRAPHING
}


//
// R_ClearPlanes
// At begining of frame.
//
DOOM_C_API void R_ClearPlanes ( planecontext_t* context, int32_t width, int32_t height, angle_t thisangle )
{
	M_PROFILE_FUNC();

	int32_t	i;
	angle_t	angle;

	{
		M_PROFILE_NAMED( "Clip clear" );
		// opening / clipping determination
		for (i=0 ; i<viewwidth ; i++)
		{
			context->floorclip[i] = viewheight;
		}
		memset( context->ceilingclip, -1, sizeof( context->ceilingclip ) );
	}

	context->lastopening = context->openings;

	memset( context->rasterregions, 0, sizeof( rasterregion_t* ) * ( numflats + numtextures ) );

	// texture calculation
	if( span_override == Span_Original )
	{
		memset (context->cachedheight, 0, sizeof(context->cachedheight));
	}

	// left to right mapping
	angle = (thisangle-ANG90)>>RENDERANGLETOFINESHIFT;
	
	// scale will be unit scale at SCREENWIDTH/2 distance
	context->basexscale = FixedDiv (renderfinecosine[angle],centerxfrac);
	context->baseyscale = -FixedDiv (renderfinesine[angle],centerxfrac);
}

//
// R_FindPlane
//

DOOM_C_API rasterregion_t* R_AddNewRasterRegion( planecontext_t* context, int32_t picnum, fixed_t height, int32_t lightlevel, int32_t start, int32_t stop )
{
	constexpr rasterline_t defaultline = { VPINDEX_INVALID, 0 };
	int16_t width = stop - start + 1;

	rasterregion_t* region = R_AllocateScratch< rasterregion_t >( 1 );
	region->height = FixedToRendFixed( height );
	region->lightlevel = lightlevel;
	region->minx = start;
	region->maxx = stop;
	region->miny = render_height;
	region->maxy = -1;

	if( width > 0 )
	{
		region->lines = R_AllocateScratch< rasterline_t >( width, defaultline );
	}
	else
	{
		region->lines = nullptr;
	}

	region->nextregion = context->rasterregions[ picnum ];
	context->rasterregions[ picnum ] = region;

	return region;
}

//
// R_MakeSpans
//
DOOM_C_API void R_MakeSpans( planecontext_t* planecontext, spancontext_t* spancontext, int32_t x, int32_t t1, int32_t b1, int32_t t2, int32_t b2 )
{
	while (t1 < t2 && t1<=b1)
	{
		R_MapPlane( planecontext, spancontext, t1, planecontext->spanstart[t1], x-1 );
		t1++;
	}
	while (b1 > b2 && b1>=t1)
	{
		R_MapPlane( planecontext, spancontext, b1, planecontext->spanstart[b1], x-1 );
		b1--;
	}
	
	while (t2 < t1 && t2<=b2)
	{
		planecontext->spanstart[t2] = x;
		t2++;
	}
	while (b2 > b1 && b2>=t2)
	{
		planecontext->spanstart[b2] = x;
		b2--;
	}
}

#ifdef RANGECHECK
DOOM_C_API void R_ErrorCheckPlanes( rendercontext_t* context )
{
	M_PROFILE_FUNC();

	if ( context->bspcontext.thisdrawseg - context->bspcontext.drawsegs > MAXDRAWSEGS)
	{
		I_Error ("R_DrawPlanes: drawsegs overflow (%" PRIiPTR ")",
				context->bspcontext.thisdrawseg - context->bspcontext.drawsegs);
	}

	if (context->planecontext.lastopening - context->planecontext.openings > MAXOPENINGS)
	{
		I_Error ("R_DrawPlanes: opening overflow (%" PRIiPTR ")",
				context->planecontext.lastopening - context->planecontext.openings);
	}
}
#endif

//
// R_DrawPlanes
// At the end of each frame.
//

constexpr auto Lines( rasterregion_t* region )
{
	return std::span( region->lines, region->maxx - region->minx + 1 );
}

auto Surfaces( planecontext_t* context )
{
	return std::span( context->rasterregions, numflats + numtextures );
}

DOOM_C_API void R_DrawPlanes( vbuffer_t* dest, planecontext_t* planecontext )
{
	M_PROFILE_FUNC();

	spancontext_t	spancontext;
	colcontext_t	skycontext;

	int32_t			span_type = span_override;

	if( span_override == Span_None )
	{
		span_type = M_MAX( Span_PolyRaster_Log2_4, M_MIN( (int32_t)( log2f( render_height * 0.02f ) + 0.5f ), Span_PolyRaster_Log2_32 ) );
	}

	if( span_type == Span_Original )
	{
		I_Error( "I broke the span renderer, it's on its way out now." );
	}

	skycontext.colfunc = colfuncs[ M_MIN( ( ( pspriteiscale >> detailshift ) >> 12 ), 15 ) ];

	spancontext.output = *dest;
	planecontext->output = *dest;

	// Originally this would setup the column renderer for every instance of a sky found.
	// But we have our own context for it now. These are constants too, so you could cook
	// this once and forget all about it.
	skycontext.iscale = FixedToRendFixed( pspriteiscale>>detailshift );
	skycontext.scale = FixedToRendFixed( pspritescale>>detailshift );
	skycontext.texturemid = skytexturemid;

	// This isn't a constant though...
	skycontext.output = *dest;
	skycontext.sourceheight = texturelookup[ skytexture ]->renderheight;

	// TODO: Sort visplanes by height
	int32_t picnum = -1;
	for( rasterregion_t* region : Surfaces( planecontext ) )
	{
		++picnum;

		// REALLY need an enumerate range so that we can get an index as well as the data
		if( region != nullptr )
		{
			// sky flat
			if( picnum == skyflatnum )
			{
				for( rasterregion_t* thisregion : RegionRange( region ) )
				{
					int32_t x = thisregion->minx;

					for( rasterline_t& line : Lines( thisregion ) )
					{
						if ( line.top <= line.bottom )
						{
							skycontext.yl = line.top;
							skycontext.yh = line.bottom;
							skycontext.x = x;

							int32_t angle = ( viewangle + xtoviewangle[x] ) >> ANGLETOSKYSHIFT;
							// Sky is allways drawn full bright,
							//  i.e. colormaps[0] is used.
							// Because of this hack, sky is not affected
							//  by INVUL inverse mapping.
							skycontext.source = R_GetColumn( skytexture, angle, 0 );
							skycontext.colfunc( &skycontext );
						}
						++x;
					}
				}
			}
			else
			{
				// regular flat
				int32_t lumpnum = flattranslation[ picnum ];

				R_RasteriseRegionRange( (spantype_t)span_type, planecontext, region, flatlookup[ lumpnum ] );

				//planecontext->source = spancontext.source = flatlookup[ lumpnum ]->data;
				//
				//for( rasterregion_t* thisregion : RegionRange( region ) )
				//{
				//	planecontext->planeheight = abs( RendFixedToFixed( thisregion->height ) - viewz );
				//	int32_t light = M_CLAMP( ( ( thisregion->lightlevel >> LIGHTSEGSHIFT ) + extralight ), 0, LIGHTLEVELS - 1 );
				//
				//	planecontext->planezlightindex = light;
				//	planecontext->planezlight = zlight[ light ];
				//
				//	if( span_type == Span_Original )
				//	{
				//		I_Error( "I broke the span renderer, it's on its way out now." );
				//		//int32_t stop = pl->maxx + 1;
				//		//for (x=pl->minx ; x<= stop ; x++)
				//		//{
				//		//	R_MakeSpans( planecontext, &spancontext, x, pl->top[x-1], pl->bottom[x-1], pl->top[x], pl->bottom[x] );
				//		//}
				//	}
				//	else
				//	{
				//		R_RasteriseColumns( (spantype_t)span_type, planecontext, thisregion );
				//	}
				//}
			}
		}
	}
}
