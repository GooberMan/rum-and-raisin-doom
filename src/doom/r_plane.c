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


#include <stdio.h>
#include <stdlib.h>

#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"

#include "doomdef.h"
#include "doomstat.h"

#include "r_local.h"
#include "r_sky.h"
#include "r_raster.h"

#include "m_misc.h"

//
// Constants. Don't need to be in a context. Will get them off the stack at some point though.
//

rend_fixed_t			yslope[ MAXSCREENHEIGHT ];
rend_fixed_t	distscale[ MAXSCREENWIDTH ];

int32_t			span_override = Span_None;

//
// R_InitPlanes
// Only at game startup.
//
void R_InitPlanes (void)
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
void R_MapPlane( planecontext_t* planecontext, spancontext_t* spancontext, int32_t y, int32_t x1, int32_t x2 )
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
void R_ClearPlanes ( planecontext_t* context, int32_t width, int32_t height, angle_t thisangle )
{
	int32_t	i;
	angle_t	angle;

	// opening / clipping determination
	for (i=0 ; i<viewwidth ; i++)
	{
		context->floorclip[i] = viewheight;
	}
	memset( context->ceilingclip, -1, sizeof( context->ceilingclip ) );

	context->lastvisplane = context->visplanes;
	context->lastopening = context->openings;

	// texture calculation
	if( span_override == Span_Original )
	{
		memset (context->cachedheight, 0, sizeof(context->cachedheight));
	}
	else
	{
		memset (context->raster, 0, sizeof(context->raster));
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

// R&R TODO: Optimize this wow linear search this will ruin lives
visplane_t* R_FindPlane( planecontext_t* context, fixed_t height, int32_t picnum, int32_t lightlevel )
{
	visplane_t*	check;
	
	if (picnum == skyflatnum)
	{
		height = 0;			// all skys map together
		lightlevel = 0;
	}
	
	for (check= context->visplanes; check< context->lastvisplane; check++)
	{
		if (height == check->height
			&& picnum == check->picnum
			&& lightlevel == check->lightlevel)
		{
			break;
		}
	}
			
	if (check < context->lastvisplane)
	{
		return check;
	}
		
	if (context->lastvisplane - context->visplanes == MAXVISPLANES)
	{
		I_Error ("R_FindPlane: no more visplanes");
	}
		
	context->lastvisplane++;

	check->height = height;
	check->picnum = picnum;
	check->lightlevel = lightlevel;
	check->minx = render_width;
	check->maxx = -1;
	check->miny = render_height;
	check->maxy = -1;

	memset (check->top,VPINDEX_INVALID,sizeof(check->top));
		
	return check;
}


//
// R_CheckPlane
//
visplane_t* R_CheckPlane( planecontext_t* context, visplane_t* pl, int32_t start, int32_t stop )
{
	int32_t		intrl;
	int32_t		intrh;
	int32_t		unionl;
	int32_t		unionh;
	int32_t		x;
	
	if (start < pl->minx)
	{
		intrl = pl->minx;
		unionl = start;
	}
	else
	{
		unionl = pl->minx;
		intrl = start;
	}
	
	if (stop > pl->maxx)
	{
		intrh = pl->maxx;
		unionh = stop;
	}
	else
	{
		unionh = pl->maxx;
		intrh = stop;
	}

	for (x=intrl ; x<= intrh ; x++)
	{
		if (pl->top[x] != VPINDEX_INVALID)
			break;
	}

	if (x > intrh)
	{
		pl->minx = unionl;
		pl->maxx = unionh;

		// use the same one
		return pl;
	}
	
	if (context->lastvisplane - context->visplanes == MAXVISPLANES)
	{
		I_Error ("R_CheckPlane: no more visplanes");
	}

	// make a new visplane
	context->lastvisplane->height = pl->height;
	context->lastvisplane->picnum = pl->picnum;
	context->lastvisplane->lightlevel = pl->lightlevel;

	pl = context->lastvisplane++;
	pl->minx = start;
	pl->maxx = stop;

	memset (pl->top,VPINDEX_INVALID,sizeof(pl->top));
		
	return pl;
}


//
// R_MakeSpans
//
void R_MakeSpans( planecontext_t* planecontext, spancontext_t* spancontext, int32_t x, int32_t t1, int32_t b1, int32_t t2, int32_t b2 )
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
void R_ErrorCheckPlanes( rendercontext_t* context )
{
	if ( context->bspcontext.thisdrawseg - context->bspcontext.drawsegs > MAXDRAWSEGS)
	{
		I_Error ("R_DrawPlanes: drawsegs overflow (%" PRIiPTR ")",
				context->bspcontext.thisdrawseg - context->bspcontext.drawsegs);
	}

	if ( context->planecontext.lastvisplane - context->planecontext.visplanes > MAXVISPLANES)
	{
		I_Error ("R_DrawPlanes: visplane overflow (%" PRIiPTR ")",
				context->planecontext.lastvisplane - context->planecontext.visplanes);
	}

	if (context->planecontext.lastopening - context->planecontext.openings > MAXOPENINGS)
	{
		I_Error ("R_DrawPlanes: opening overflow (%" PRIiPTR ")",
				context->planecontext.lastopening - context->planecontext.openings);
	}
}
#endif

#define MAXWIDTH			(MAXSCREENWIDTH + ( MAXSCREENWIDTH >> 1) )
#define MAXHEIGHT			(MAXSCREENHEIGHT + ( MAXSCREENHEIGHT >> 1) )

extern size_t			xlookup[MAXWIDTH];
extern size_t			rowofs[MAXHEIGHT];

//
// R_DrawPlanes
// At the end of each frame.
//



void R_DrawPlanes( vbuffer_t* dest, planecontext_t* planecontext )
{
	visplane_t*		pl;
	int32_t			light;
	int32_t			x;
	int32_t			stop;
	int32_t			angle;
	int32_t			lumpnum;

	spancontext_t	spancontext;
	colcontext_t	skycontext;

	int32_t			span_type = span_override;

	if( span_override == Span_None )
	{
		span_type = M_MAX( Span_Original, M_MIN( (int32_t)( log2f( render_height * 0.02f ) + 0.5f ), Span_PolyRaster_Log2_32 ) );
	}

	skycontext.colfunc = colfuncs[ M_MIN( ( ( pspriteiscale >> detailshift ) >> 12 ), 15 ) ];

	spancontext.output = *dest;

	// Originally this would setup the column renderer for every instance of a sky found.
	// But we have our own context for it now. These are constants too, so you could cook
	// this once and forget all about it.
	skycontext.iscale = FixedToRendFixed( pspriteiscale>>detailshift );
	skycontext.scale = FixedToRendFixed( pspritescale>>detailshift );
	skycontext.texturemid = skytexturemid;

	// This isn't a constant though...
	skycontext.output = *dest;

	// TODO: Sort visplanes by height
	for (pl = planecontext->visplanes ; pl < planecontext->lastvisplane ; pl++)
	{
		if (pl->minx > pl->maxx)
			continue;
	
		// sky flat
		if (pl->picnum == skyflatnum)
		{
			for (x=pl->minx ; x <= pl->maxx ; x++)
			{
				if (pl->top[x] <= pl->bottom[x])
				{
					skycontext.yl = pl->top[x];
					skycontext.yh = pl->bottom[x];
					skycontext.x = x;

					angle = (viewangle + xtoviewangle[x])>>ANGLETOSKYSHIFT;
					// Sky is allways drawn full bright,
					//  i.e. colormaps[0] is used.
					// Because of this hack, sky is not affected
					//  by INVUL inverse mapping.
					skycontext.source = R_GetColumn(skytexture, angle, 0);
					skycontext.colfunc( &skycontext );
				}
			}
			continue;
		}

		// regular flat
		lumpnum = flattranslation[pl->picnum];
		spancontext.source = precachedflats[ lumpnum ];

		planecontext->planeheight = abs(pl->height-viewz);
		light = M_CLAMP( ( (pl->lightlevel >> LIGHTSEGSHIFT)+extralight ), 0, LIGHTLEVELS - 1 );

		planecontext->planezlightindex = light;
		planecontext->planezlight = zlight[light];

		pl->top[pl->maxx+1] = VPINDEX_INVALID;
		pl->top[pl->minx-1] = VPINDEX_INVALID;

		stop = pl->maxx + 1;

		if( span_type == Span_Original )
		{
			for (x=pl->minx ; x<= stop ; x++)
			{
				R_MakeSpans( planecontext, &spancontext, x, pl->top[x-1], pl->bottom[x-1], pl->top[x], pl->bottom[x] );
			}
		}
		else
		{
			R_RasteriseColumns( span_type, planecontext, &spancontext, pl );
		}
	}
}
