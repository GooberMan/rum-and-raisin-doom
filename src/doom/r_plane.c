//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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

#include "m_misc.h"

//
// opening
//
// TODO: MOVE TO CONTEXTS

// Here comes the obnoxious "visplane".
visplane_t		visplanes[MAXVISPLANES];
visplane_t*		lastvisplane;
visplane_t*		floorplane;
visplane_t*		ceilingplane;

// ?
vertclip_t		openings[MAXOPENINGS];
vertclip_t*		lastopening;


//
// Clip values are the solid pixel bounding the range.
//  floorclip starts out SCREENHEIGHT
//  ceilingclip starts out -1
//
vertclip_t			floorclip[SCREENWIDTH];
vertclip_t			ceilingclip[SCREENWIDTH];

//
// spanstart holds the start of a plane span
// initialized to 0 at start
//
int32_t			spanstart[SCREENHEIGHT];
int32_t			spanstop[SCREENHEIGHT];

//
// texture mapping
//

lighttable_t**		planezlight;
int32_t			planezlightindex;
fixed_t			planeheight;

fixed_t			yslope[SCREENHEIGHT];
fixed_t			distscale[SCREENWIDTH];
fixed_t			basexscale;
fixed_t			baseyscale;

fixed_t			cachedheight[SCREENHEIGHT];
fixed_t			cacheddistance[SCREENHEIGHT];
fixed_t			cachedxstep[SCREENHEIGHT];
fixed_t			cachedystep[SCREENHEIGHT];
// END MOVE TO CONTEXTS


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
void R_MapPlane( spancontext_t* context, int y, int x1, int x2 )
{
	angle_t		angle;
	fixed_t		distance;
	fixed_t		length;
	uint32_t	index;
	byte*		originalsource = context->source;
	
#ifdef RANGECHECK
	if (x2 < x1
		|| x1 < 0
		|| x2 >= viewwidth
		|| y > viewheight)
	{
		I_Error ("R_MapPlane: %i, %i at %i",x1,x2,y);
	}
#endif

	// TODO: planeheight needs to go straight in to the context...
	if (planeheight != cachedheight[y])
	{
		cachedheight[y] = planeheight;
		distance = cacheddistance[y] = FixedMul (planeheight, yslope[y]);
		context->xstep = cachedxstep[y] = FixedMul (distance,basexscale);
		context->ystep = cachedystep[y] = FixedMul (distance,baseyscale);
	}
	else
	{
		distance = cacheddistance[y];
		context->xstep = cachedxstep[y];
		context->ystep = cachedystep[y];
	}
	
	length = FixedMul (distance,distscale[x1]);
	angle = (viewangle + xtoviewangle[x1])>>RENDERANGLETOFINESHIFT;
	context->xfrac = viewx + FixedMul(renderfinecosine[angle], length);
	context->yfrac = -viewy - FixedMul(renderfinesine[angle], length);

	if( fixedcolormapindex )
	{
		// TODO: This should be a real define somewhere
		context->source += ( 4096 * fixedcolormapindex );
	}
	else
	{
		index = distance >> LIGHTZSHIFT;
		if (index >= MAXLIGHTZ )
			index = MAXLIGHTZ-1;

		index = zlightindex[planezlightindex][index];

		context->source += ( 4096 * index );
	}

    context->y = y;
    context->x1 = x1;
    context->x2 = x2;

    // high or low detail
    spanfunc( context );	

	context->source = originalsource;
}


//
// R_ClearPlanes
// At begining of frame.
//
void R_ClearPlanes ( planecontext_t* context, int32_t width, int32_t height, int32_t thisangle )
{
	int32_t	i;
	angle_t	angle;

	// opening / clipping determination
	for (i=0 ; i<viewwidth ; i++)
	{
		floorclip[i] = viewheight;
		ceilingclip[i] = -1;
	}

	lastvisplane = visplanes;
	lastopening = openings;

	// texture calculation
	memset (cachedheight, 0, sizeof(cachedheight));

	// left to right mapping
	angle = (viewangle-ANG90)>>RENDERANGLETOFINESHIFT;
	
	// scale will be unit scale at SCREENWIDTH/2 distance
	basexscale = FixedDiv (renderfinecosine[angle],centerxfrac);
	baseyscale = -FixedDiv (renderfinesine[angle],centerxfrac);

	if( context )
	{
		// opening / clipping determination
		for (i=0 ; i<width ; i++)
		{
			context->floorclip[i] = height;
			context->ceilingclip[i] = -1;
		}

		context->lastvisplane = context->visplanes;
		context->lastopening = context->openings;

		// texture calculation
		memset (context->cachedheight, 0, sizeof(context->cachedheight));

		// left to right mapping
		angle = (thisangle - ANG90)>>RENDERANGLETOFINESHIFT;
	
		// scale will be unit scale at SCREENWIDTH/2 distance
		context->basexscale = FixedDiv (renderfinecosine[angle],centerxfrac);
		context->baseyscale = -FixedDiv (renderfinesine[angle],centerxfrac);
	}
}




//
// R_FindPlane
//

// R&R TODO: Optimize this wow linear search this will ruin lives
visplane_t*
R_FindPlane
( fixed_t	height,
  int		picnum,
  int		lightlevel )
{
    visplane_t*	check;
	
    if (picnum == skyflatnum)
    {
	height = 0;			// all skys map together
	lightlevel = 0;
    }
	
    for (check=visplanes; check<lastvisplane; check++)
    {
	if (height == check->height
	    && picnum == check->picnum
	    && lightlevel == check->lightlevel)
	{
	    break;
	}
    }
    
			
    if (check < lastvisplane)
	return check;
		
    if (lastvisplane - visplanes == MAXVISPLANES)
	I_Error ("R_FindPlane: no more visplanes");
		
    lastvisplane++;

    check->height = height;
    check->picnum = picnum;
    check->lightlevel = lightlevel;
    check->minx = SCREENWIDTH;
    check->maxx = -1;
    
    memset (check->top,VPINDEX_INVALID,sizeof(check->top));
		
    return check;
}


//
// R_CheckPlane
//
visplane_t*
R_CheckPlane
( visplane_t*	pl,
  int		start,
  int		stop )
{
    int		intrl;
    int		intrh;
    int		unionl;
    int		unionh;
    int		x;
	
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
	if (pl->top[x] != VPINDEX_INVALID)
	    break;

    if (x > intrh)
    {
	pl->minx = unionl;
	pl->maxx = unionh;

	// use the same one
	return pl;		
    }
	
    // make a new visplane
    lastvisplane->height = pl->height;
    lastvisplane->picnum = pl->picnum;
    lastvisplane->lightlevel = pl->lightlevel;
    
    if (lastvisplane - visplanes == MAXVISPLANES)
	I_Error ("R_CheckPlane: no more visplanes");

    pl = lastvisplane++;
    pl->minx = start;
    pl->maxx = stop;

    memset (pl->top,VPINDEX_INVALID,sizeof(pl->top));
		
    return pl;
}


//
// R_MakeSpans
//
void R_MakeSpans( spancontext_t* context, int x, int t1, int b1, int t2, int b2 )
{
	while (t1 < t2 && t1<=b1)
	{
		R_MapPlane( context, t1,spanstart[t1],x-1 );
		t1++;
	}
	while (b1 > b2 && b1>=t1)
	{
		R_MapPlane( context, b1,spanstart[b1],x-1 );
		b1--;
	}
	
	while (t2 < t1 && t2<=b2)
	{
		spanstart[t2] = x;
		t2++;
	}
	while (b2 > b1 && b2>=t2)
	{
		spanstart[b2] = x;
		b2--;
	}
}

//
// R_DrawPlanes
// At the end of each frame.
//
void R_DrawPlanes (void)
{
	visplane_t*		pl;
	int32_t			light;
	int32_t			x;
	int32_t			stop;
	int32_t			angle;
	int32_t			lumpnum;

	spancontext_t	spancontext;
	colcontext_t	skycontext;
	extern vbuffer_t* dest_buffer;

	colfunc_t		restore = colfunc;

	colfunc = colfuncs[ M_MIN( ( ( pspriteiscale >> detailshift ) >> 12 ), 15 ) ];

	spancontext.output = *dest_buffer;

	// Originally this would setup the column renderer for every instance of a sky found.
	// But we have our own context for it now. These are constants too, so you could cook
	// this once and forget all about it.
	skycontext.iscale = pspriteiscale>>detailshift;
	skycontext.scale = pspritescale>>detailshift;
	skycontext.texturemid = skytexturemid;

	// This isn't a constant though...
	skycontext.output = *dest_buffer;

#ifdef RANGECHECK
    if (ds_p - drawsegs > MAXDRAWSEGS)
	I_Error ("R_DrawPlanes: drawsegs overflow (%" PRIiPTR ")",
		 ds_p - drawsegs);
    
    if (lastvisplane - visplanes > MAXVISPLANES)
	I_Error ("R_DrawPlanes: visplane overflow (%" PRIiPTR ")",
		 lastvisplane - visplanes);
    
    if (lastopening - openings > MAXOPENINGS)
	I_Error ("R_DrawPlanes: opening overflow (%" PRIiPTR ")",
		 lastopening - openings);
#endif

    for (pl = visplanes ; pl < lastvisplane ; pl++)
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
					colfunc( &skycontext );
				}
			}
			continue;
		}
	
		// regular flat
		lumpnum = flattranslation[pl->picnum];
		spancontext.source = precachedflats[ lumpnum ];

		planeheight = abs(pl->height-viewz);
		light = (pl->lightlevel >> LIGHTSEGSHIFT)+extralight;

		if (light >= LIGHTLEVELS)
			light = LIGHTLEVELS-1;

		if (light < 0)
			light = 0;

		planezlightindex = light;
		planezlight = zlight[light];

		pl->top[pl->maxx+1] = VPINDEX_INVALID;
		pl->top[pl->minx-1] = VPINDEX_INVALID;
		
		stop = pl->maxx + 1;

		for (x=pl->minx ; x<= stop ; x++)
		{
			R_MakeSpans( &spancontext, x, pl->top[x-1], pl->bottom[x-1], pl->top[x], pl->bottom[x] );
		}
	
	}

	colfunc = restore;
}
