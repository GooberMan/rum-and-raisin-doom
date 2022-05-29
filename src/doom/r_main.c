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
//	Rendering main loop and setup functions,
//	 utility functions (BSP, geometry, trigonometry).
//	See tables.c, too.
//





#include <stdlib.h>
#include <math.h>
#include <stddef.h>

#include "doomdef.h"
#include "d_loop.h"

#include "m_bbox.h"
#include "m_config.h"
#include "m_menu.h"
#include "m_debugmenu.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

#include "r_local.h"
#include "r_sky.h"

// Need ST_HEIGHT
#include "st_stuff.h"
#include "m_misc.h"
#include "w_wad.h"

#include "z_zone.h"

#include "i_thread.h"

#include "tables.h"

#define SBARHEIGHT		( ( ( (int64_t)( ST_HEIGHT << FRACBITS ) * (int64_t)V_HEIGHTMULTIPLIER ) >> FRACBITS ) >> FRACBITS )




int32_t	field_of_view_degrees = 90;

// Fineangles in the SCREENWIDTH wide window.
// If we define this as FINEANGLES / 4 then we get auto 90 degrees everywhere
#define FIELDOFVIEW				( FINEANGLES * ( field_of_view_degrees * 100 ) / 36000 )
#define RENDERFIELDOFVIEW		( RENDERFINEANGLES * ( field_of_view_degrees * 100 ) / 36000 )

typedef struct renderdata_s
{
	rendercontext_t		context;
	threadhandle_t		thread;
	atomicval_t			running;
	atomicval_t			shouldquit;
	atomicval_t			framewaiting;
	atomicval_t			framefinished;
} renderdata_t;

renderdata_t*			renderdatas;

#define DEFAULT_RENDERCONTEXTS 4

int32_t					numrendercontexts = DEFAULT_RENDERCONTEXTS;
int32_t					numusablerendercontexts = DEFAULT_RENDERCONTEXTS;
boolean					renderloadbalancing = false;
boolean					rendersplitvisualise = false;
boolean					renderrebalancecontexts = false;
boolean					renderthreaded = true;
boolean					renderSIMDcolumns = false;
atomicval_t				renderthreadCPUmelter = 0;
int32_t					performancegraphscale = 20;

int32_t					viewangleoffset;

// increment every time a check is made
int32_t					validcount = 1;		


lighttable_t*			fixedcolormap;
int32_t					fixedcolormapindex;

int32_t					centerx;
int32_t					centery;

fixed_t					centerxfrac;
fixed_t					centeryfrac;
fixed_t					projection;

// just for profiling purposes
int32_t					framecount;	

fixed_t					viewx;
fixed_t					viewy;
fixed_t					viewz;

angle_t					viewangle;

fixed_t					viewcos;
fixed_t					viewsin;

player_t*				viewplayer;

// 0 = high, 1 = low
int32_t					detailshift;	

//
// precalculated math tables
//
angle_t					clipangle;

// The viewangletox[viewangle + FINEANGLES/4] lookup
// maps the visible view angles to screen X coordinates,
// flattening the arc to a flat projection plane.
// There will be many angles mapped to the same X. 

int32_t				viewangletox[RENDERFINEANGLES/2];

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.
angle_t				xtoviewangle[MAXSCREENWIDTH+1];

lighttable_t*		scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t*		scalelightfixed[MAXLIGHTSCALE];
int32_t				scalelightindex[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t*		zlight[LIGHTLEVELS][MAXLIGHTZ];
int32_t				zlightindex[LIGHTLEVELS][MAXLIGHTZ];
colfunc_t			colfuncs[ COLFUNC_COUNT ];

// bumped light from gun blasts
int32_t				extralight;

colfunc_t			transcolfunc;

spanfunc_t			spanfunc;

int32_t				fuzz_style = Fuzz_Original;

const byte whacky_void_indices[] =
{
	251,
	249,
	118,
	200,
};
const int32_t num_whacky_void_indices = sizeof( whacky_void_indices ) / sizeof( *whacky_void_indices );
const uint64_t whacky_void_microseconds = 1200000;
int32_t voidcleartype = Void_NoClear;


//
// R_AddPointToBox
// Expand a given bbox
// so that it encloses a given point.
//
void
R_AddPointToBox
( int		x,
  int		y,
  fixed_t*	box )
{
    if (x< box[BOXLEFT])
	box[BOXLEFT] = x;
    if (x> box[BOXRIGHT])
	box[BOXRIGHT] = x;
    if (y< box[BOXBOTTOM])
	box[BOXBOTTOM] = y;
    if (y> box[BOXTOP])
	box[BOXTOP] = y;
}


//
// R_PointOnSide
// Traverse BSP (sub) tree,
//  check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
int
R_PointOnSide
( fixed_t	x,
  fixed_t	y,
  node_t*	node )
{
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	left;
    fixed_t	right;
	
    if (!node->dx)
    {
	if (x <= node->x)
	    return node->dy > 0;
	
	return node->dy < 0;
    }
    if (!node->dy)
    {
	if (y <= node->y)
	    return node->dx < 0;
	
	return node->dx > 0;
    }
	
    dx = (x - node->x);
    dy = (y - node->y);
	
    // Try to quickly decide by looking at sign bits.
    if ( (node->dy ^ node->dx ^ dx ^ dy)&0x80000000 )
    {
	if  ( (node->dy ^ dx) & 0x80000000 )
	{
	    // (left is negative)
	    return 1;
	}
	return 0;
    }

    left = FixedMul ( node->dy>>FRACBITS , dx );
    right = FixedMul ( dy , node->dx>>FRACBITS );
	
    if (right < left)
    {
	// front side
	return 0;
    }
    // back side
    return 1;			
}


int
R_PointOnSegSide
( fixed_t	x,
  fixed_t	y,
  seg_t*	line )
{
    fixed_t	lx;
    fixed_t	ly;
    fixed_t	ldx;
    fixed_t	ldy;
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	left;
    fixed_t	right;
	
    lx = line->v1->x;
    ly = line->v1->y;
	
    ldx = line->v2->x - lx;
    ldy = line->v2->y - ly;
	
    if (!ldx)
    {
	if (x <= lx)
	    return ldy > 0;
	
	return ldy < 0;
    }
    if (!ldy)
    {
	if (y <= ly)
	    return ldx < 0;
	
	return ldx > 0;
    }
	
    dx = (x - lx);
    dy = (y - ly);
	
    // Try to quickly decide by looking at sign bits.
    if ( (ldy ^ ldx ^ dx ^ dy)&0x80000000 )
    {
	if  ( (ldy ^ dx) & 0x80000000 )
	{
	    // (left is negative)
	    return 1;
	}
	return 0;
    }

    left = FixedMul ( ldy>>FRACBITS , dx );
    right = FixedMul ( dy , ldx>>FRACBITS );
	
    if (right < left)
    {
	// front side
	return 0;
    }
    // back side
    return 1;			
}


//
// R_PointToAngle
// To get a global angle from cartesian coordinates,
//  the coordinates are flipped until they are in
//  the first octant of the coordinate system, then
//  the y (<=x) is scaled and divided by x to get a
//  tangent (slope) value which is looked up in the
//  tantoangle[] table.

//




angle_t
R_PointToAngle
( fixed_t	x,
  fixed_t	y )
{	
    x -= viewx;
    y -= viewy;
    
    if ( (!x) && (!y) )
	return 0;

    if (x>= 0)
    {
	// x >=0
	if (y>= 0)
	{
	    // y>= 0

	    if (x>y)
	    {
		// octant 0
		return rendertantoangle[ SlopeDiv_Render(y,x) ];
	    }
	    else
	    {
		// octant 1
		return ANG90-1-rendertantoangle[ SlopeDiv_Render(x,y) ];
	    }
	}
	else
	{
	    // y<0
	    y = -y;

	    if (x>y)
	    {
		// octant 8
			return M_NEGATE(rendertantoangle[ SlopeDiv_Render(y,x) ]);
	    }
	    else
	    {
		// octant 7
		return ANG270+rendertantoangle[ SlopeDiv_Render(x,y) ];
	    }
	}
    }
    else
    {
	// x<0
	x = -x;

	if (y>= 0)
	{
	    // y>= 0
	    if (x>y)
	    {
		// octant 3
		return ANG180-1-rendertantoangle[ SlopeDiv_Render(y,x) ];
	    }
	    else
	    {
		// octant 2
		return ANG90+ rendertantoangle[ SlopeDiv_Render(x,y) ];
	    }
	}
	else
	{
	    // y<0
	    y = -y;

	    if (x>y)
	    {
		// octant 4
		return ANG180+rendertantoangle[ SlopeDiv_Render(y,x) ];
	    }
	    else
	    {
		 // octant 5
		return ANG270-1-rendertantoangle[ SlopeDiv_Render(x,y) ];
	    }
	}
    }
    return 0;
}

// Called by playsim. Let's bring it back to normal tantoangle
angle_t
R_PointToAngle2
( fixed_t	x1,
  fixed_t	y1,
  fixed_t	x2,
  fixed_t	y2 )
{	
    int32_t x = x2 - x1;
    int32_t y = y2 - y1;
    
    if ( (!x) && (!y) )
		return 0;

    if (x>= 0)
    {
		// x >=0
		if (y>= 0)
		{
			// y>= 0

			if (x>y)
			{
				// octant 0
				return tantoangle[ SlopeDiv_Playsim(y,x)];
			}
			else
			{
				// octant 1
				return ANG90-1-tantoangle[ SlopeDiv_Playsim(x,y)];
			}
		}
		else
		{
			// y<0
			y = -y;

			if (x>y)
			{
				// octant 8
				return M_NEGATE(tantoangle[SlopeDiv_Playsim(y,x)]);
			}
			else
			{
				// octant 7
				return ANG270+tantoangle[ SlopeDiv_Playsim(x,y)];
			}
		}
	}
	else
	{
		// x<0
		x = -x;

		if (y>= 0)
		{
			// y>= 0
			if (x>y)
			{
				// octant 3
				return ANG180-1-tantoangle[ SlopeDiv_Playsim(y,x)];
			}
			else
			{
				// octant 2
				return ANG90+ tantoangle[ SlopeDiv_Playsim(x,y)];
			}
		}
		else
		{
			// y<0
			y = -y;

			if (x>y)
			{
				// octant 4
				return ANG180+tantoangle[ SlopeDiv_Playsim(y,x)];
			}
			else
			{
				 // octant 5
				return ANG270-1-tantoangle[ SlopeDiv_Playsim(x,y)];
			}
		}
	}
	return 0;
}


fixed_t
R_PointToDist
( fixed_t	x,
  fixed_t	y )
{
    int		angle;
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	temp;
    fixed_t	dist;
    fixed_t     frac;
	
    dx = abs(x - viewx);
    dy = abs(y - viewy);
	
    if (dy>dx)
    {
	temp = dx;
	dx = dy;
	dy = temp;
    }

    // Fix crashes in udm1.wad

    if (dx != 0)
    {
        frac = FixedDiv(dy, dx);
    }
    else
    {
	frac = 0;
    }
	
    angle = (rendertantoangle[frac>>RENDERDBITS]+ANG90) >> RENDERANGLETOFINESHIFT;

    // use as cosine
    dist = FixedDiv (dx, renderfinesine[angle] );	
	
    return dist;
}

void R_BindRenderVariables( void )
{
	M_BindIntVariable("fuzz_style",			&fuzz_style);
}

#define PI acos(-1.0)

//
// R_InitPointToAngle
//
void R_InitPointToAngle (void)
{
#if RENDERSLOPEQUALITYSHIFT > 0
	int32_t		i;
	angle_t		t;
	float_t		f;
	//
	// slope (tangent) to angle lookup
	//
	for (i=0 ; i<=RENDERSLOPERANGE ; i++)
	{
		f = (float_t)( atan( (float_t)i/RENDERSLOPERANGE )/(PI*2) );
		t = (angle_t)( 0xffffffff*f );
		rendertantoangle[i] = t;
	}
#endif
}


//
// R_ScaleFromGlobalAngle
// Returns the texture mapping scale
//  for the current line (horizontal span)
//  at the given angle.
// rw_distance must be calculated first.
//

// R_ScaleFromGlobalAngle is a function of screenwidth
// Defining the original maximum as a function of your
// new width is the correct way to go about things.
// 1/5th of the screenwidth is the way to go.
#define MAXSCALE ( 64 * ( render_width / 320 ) )
#define MAXSCALE_FIXED ( MAXSCALE << FRACBITS )

fixed_t R_ScaleFromGlobalAngle (angle_t visangle, fixed_t distance, fixed_t view_angle, fixed_t normal_angle)
{
	// TODO: Replace this with the scaling function for my visplane renderer
    fixed_t		scale;
    angle_t		anglea;
    angle_t		angleb;
    int32_t		sinea;
    int32_t		sineb;
    fixed_t		num;
    int32_t		den;

	scale = MAXSCALE_FIXED;

    anglea = ANG90 + (visangle - view_angle);
    angleb = ANG90 + (visangle - normal_angle);

    // both sines are allways positive
    sinea = renderfinesine[anglea>>RENDERANGLETOFINESHIFT];
    sineb = renderfinesine[angleb>>RENDERANGLETOFINESHIFT];
    num = FixedMul(projection,sineb)<<detailshift;
    den = FixedMul(distance,sinea);

    if (den >= 0 && den > num>>FRACBITS)
    {
		scale = FixedDiv (num, den);

		if (scale > MAXSCALE_FIXED)
			scale = MAXSCALE_FIXED;
		else if (scale < 256)
			scale = 256;
    }
	
    return scale;
}



//
// R_InitTables
//

void R_InitTables (void)
{
#if RENDERQUALITYSHIFT > 0
    int32_t		i;
    float_t		a;
    float_t		fv;
    int32_t		t;
    
    // viewangle tangent table
    for (i=0 ; i< RENDERFINETANGENTCOUNT ; i++)
    {
		a = (float_t)( (i-RENDERFINEANGLES/4+0.5)*PI*2/RENDERFINEANGLES );
		fv = (float_t)( FRACUNIT*tan(a) );
		t = (int32_t)fv;
		renderfinetangent[i] = t;
    }
    
    // finesine table
    for (i=0 ; i < RENDERFINESINECOUNT ; i++)
    {
		// OPTIMIZE: mirror...
		a = (float_t)( (i+0.5)*PI*2/RENDERFINEANGLES );
		t = (int32_t)( FRACUNIT*sin (a) );
		renderfinesine[i] = t;
    }
#endif // RENDERQUALITYSHIFT > 0
}



//
// R_InitTextureMapping
//
void R_InitTextureMapping (void)
{
    int			i;
    int			x;
    int			t;
    fixed_t		focallength;
    
    // Use tangent table to generate viewangletox:
    //  viewangletox will give the next greatest x
    //  after the view angle.
    //
    // Calc focallength
    //  so FIELDOFVIEW angles covers SCREENWIDTH.
    focallength = FixedDiv (centerxfrac,
			    renderfinetangent[ RENDERFINEANGLES / 4 + RENDERFIELDOFVIEW / 2 ] );
	
    for (i=0 ; i<RENDERFINEANGLES/2 ; i++)
    {
		if (renderfinetangent[i] > FRACUNIT*8)
			t = -1;
		else if (renderfinetangent[i] < -FRACUNIT*8)
			t = viewwidth+1;
		else
		{
			t = FixedMul (renderfinetangent[i], focallength);
			t = (centerxfrac - t+FRACUNIT-1)>>FRACBITS;

			if (t < -1)
			t = -1;
			else if (t>viewwidth+1)
			t = viewwidth+1;
		}
		viewangletox[i] = t;
    }
    
    // Scan viewangletox[] to generate xtoviewangle[]:
    //  xtoviewangle will give the smallest view angle
    //  that maps to x.	
    for (x=0;x<=viewwidth;x++)
    {
	i = 0;
	while (viewangletox[i]>x)
	    i++;
	xtoviewangle[x] = (i<<RENDERANGLETOFINESHIFT)-ANG90;
    }

    // Take out the fencepost cases from viewangletox.
    for (i=0 ; i<FINEANGLES/2 ; i++)
    {
		if (viewangletox[i] == -1)
			viewangletox[i] = 0;
		else if (viewangletox[i] == viewwidth+1)
			viewangletox[i]  = viewwidth;
	}
	
	clipangle = xtoviewangle[0];
}


int32_t aspect_adjusted_render_width = 0;
fixed_t aspect_adjusted_scaled_divide = 0;
fixed_t aspect_adjusted_scaled_mul = FRACUNIT;

void R_InitAspectAdjustedValues()
{
	fixed_t		original_perspective = FixedDiv( 16 * FRACUNIT, 10 * FRACUNIT );
	fixed_t		current_perspective = FixedDiv( render_width << FRACBITS, render_height << FRACBITS );
	fixed_t		perspective_mul = FixedDiv( original_perspective, current_perspective );

	fixed_t		intermediate_width = FixedMul( render_width << FRACBITS, perspective_mul );
	aspect_adjusted_render_width = ( intermediate_width >> FRACBITS ) + ( ( intermediate_width & 0x00008000 ) >> 15 ) * 1;
	aspect_adjusted_scaled_divide = ( aspect_adjusted_render_width << FRACBITS ) / 320;
	aspect_adjusted_scaled_mul = FixedDiv( FRACUNIT, aspect_adjusted_scaled_divide );
}


//
// R_InitLightTables
// Only inits the zlight table,
//  because the scalelight table changes with view size.
//
#define DISTMAP		2

void R_InitLightTables (void)
{
	int		i;
	int		j;
	int		level;
	int		startmap; 	
	int		scale;

	// Calculate the light levels to use
	//  for each level / distance combination.
	for (i=0 ; i< LIGHTLEVELS ; i++)
	{
		startmap = ((LIGHTLEVELS-1-i)*2)*NUMLIGHTCOLORMAPS/LIGHTLEVELS;
		for (j=0 ; j<MAXLIGHTZ ; j++)
		{
			scale = FixedDiv ( ( aspect_adjusted_render_width / 2 * FRACUNIT ), ( j + 1 ) << LIGHTZSHIFT );
			scale >>= LIGHTSCALESHIFT;
			if( LIGHTSCALEMUL != FRACUNIT )
			{
				scale = FixedMul( scale << FRACBITS, LIGHTSCALEMUL ) >> FRACBITS;
			}

			level = startmap - scale/DISTMAP;

			if (level < 0)
				level = 0;

			if (level >= NUMLIGHTCOLORMAPS)
				level = NUMLIGHTCOLORMAPS-1;

			zlightindex[i][j] = level;
			zlight[i][j] = colormaps + level*256;
		}
	}
}

byte detailmaps[16][256];
byte lightlevelmaps[32][256];

#define HAX 0
void R_InitColFuncs( void )
{
	int32_t lightlevel = 0;

#if R_DRAWCOLUMN_SIMDOPTIMISED
	colfuncs[ 0 ] = &R_DrawColumn_OneSample;

#if HAX
	colfuncs[ 1 ] = &R_DrawColumn_OneSample;
	colfuncs[ 2 ] = &R_DrawColumn_OneSample;
	colfuncs[ 3 ] = &R_DrawColumn_OneSample;
	colfuncs[ 4 ] = &R_DrawColumn_OneSample;
	colfuncs[ 5 ] = &R_DrawColumn_OneSample;
	colfuncs[ 6 ] = &R_DrawColumn_OneSample;
	colfuncs[ 7 ] = &R_DrawColumn_OneSample;
	colfuncs[ 8 ] = &R_DrawColumn_OneSample;
	colfuncs[ 9 ] = &R_DrawColumn_OneSample;
	colfuncs[ 10 ] = &R_DrawColumn_OneSample;
	colfuncs[ 11 ] = &R_DrawColumn_OneSample;
	colfuncs[ 12 ] = &R_DrawColumn_OneSample;
	colfuncs[ 13 ] = &R_DrawColumn_OneSample;
	colfuncs[ 14 ] = &R_DrawColumn_OneSample;
	colfuncs[ 15 ] = &R_DrawColumn;
#else //!HAX
	colfuncs[ 1 ] = &R_DrawColumn;
	colfuncs[ 2 ] = &R_DrawColumn;
	colfuncs[ 3 ] = &R_DrawColumn;
	colfuncs[ 4 ] = &R_DrawColumn;
	colfuncs[ 5 ] = &R_DrawColumn;
	colfuncs[ 6 ] = &R_DrawColumn;
	colfuncs[ 7 ] = &R_DrawColumn;
	colfuncs[ 8 ] = &R_DrawColumn;
	colfuncs[ 9 ] = &R_DrawColumn;
	colfuncs[ 10 ] = &R_DrawColumn;
	colfuncs[ 11 ] = &R_DrawColumn;
	colfuncs[ 12 ] = &R_DrawColumn;
	colfuncs[ 13 ] = &R_DrawColumn;
	colfuncs[ 14 ] = &R_DrawColumn;
	colfuncs[ 15 ] = &R_DrawColumn;
#endif
#endif //R_DRAWCOLUMN_SIMDOPTIMISED

	int32_t currexpand = COLFUNC_PIXELEXPANDERS;

	while( currexpand > 0 )
	{
		--currexpand;
#if !R_DRAWCOLUMN_SIMDOPTIMISED
		colfuncs[ currexpand ] = &R_DrawColumn;
#endif // R_DRAWCOLUMN_SIMDOPTIMISED
		colfuncs[ COLFUNC_NUM + currexpand ] = &R_DrawColumnLow;
	}

	memset( detailmaps[ 0 ], 247, 256 );
	memset( detailmaps[ 1 ], 110, 256 );
	memset( detailmaps[ 2 ], 108, 256 );
	memset( detailmaps[ 3 ], 106, 256 );
	memset( detailmaps[ 4 ], 104, 256 );
	memset( detailmaps[ 5 ], 102, 256 );
	memset( detailmaps[ 6 ], 100, 256 );
	memset( detailmaps[ 7 ], 98, 256 );
	memset( detailmaps[ 8 ], 96, 256 );
	memset( detailmaps[ 9 ], 94, 256 );
	memset( detailmaps[ 10 ], 92, 256 );
	memset( detailmaps[ 11 ], 90, 256 );
	memset( detailmaps[ 12 ], 88, 256 );
	memset( detailmaps[ 13 ], 86, 256 );
	memset( detailmaps[ 14 ], 84, 256 );
	memset( detailmaps[ 15 ], 82, 256 );

	while( lightlevel < 32 )
	{
		memset( lightlevelmaps[ lightlevel ], 80 + lightlevel, 256 );
		++lightlevel;
	};

	colfuncs[ COLFUNC_FUZZBASEINDEX + Fuzz_Original ] = colfuncs[ COLFUNC_NUM + COLFUNC_FUZZBASEINDEX + Fuzz_Original ] = &R_DrawFuzzColumn;
	colfuncs[ COLFUNC_FUZZBASEINDEX + Fuzz_Adjusted ] = colfuncs[ COLFUNC_NUM + COLFUNC_FUZZBASEINDEX + Fuzz_Adjusted ] = &R_DrawAdjustedFuzzColumn;
	colfuncs[ COLFUNC_FUZZBASEINDEX + Fuzz_Heatwave ] = colfuncs[ COLFUNC_NUM + COLFUNC_FUZZBASEINDEX + Fuzz_Heatwave ] = &R_DrawHeatwaveFuzzColumn;
	colfuncs[ COLFUNC_TRANSLATEINDEX ] = colfuncs[ COLFUNC_NUM + COLFUNC_TRANSLATEINDEX ] = &R_DrawTranslatedColumn;
}


void R_ResetContext( rendercontext_t* context, int32_t leftclip, int32_t rightclip )
{
	context->begincolumn = context->spritecontext.leftclip = leftclip;
	context->endcolumn = context->spritecontext.rightclip = rightclip;

	R_ClearClipSegs( &context->bspcontext, leftclip, rightclip );
	R_ClearDrawSegs( &context->bspcontext );
	R_ClearPlanes( &context->planecontext, viewwidth, viewheight, viewangle );
	R_ClearSprites( &context->spritecontext );
}

void R_RenderViewContext( rendercontext_t* rendercontext )
{
	colcontext_t	skycontext;
	int32_t			x;
	int32_t			angle;

	int32_t			currtime;
	byte*			output = rendercontext->buffer.data + rendercontext->buffer.pitch * rendercontext->begincolumn;
	size_t			outputsize = rendercontext->buffer.pitch * (rendercontext->endcolumn - rendercontext->begincolumn );

	rendercontext->starttime = I_GetTimeUS();
#if RENDER_PERF_GRAPHING
	rendercontext->bspcontext.storetimetaken = 0;
	rendercontext->bspcontext.solidtimetaken = 0;
	rendercontext->bspcontext.maskedtimetaken = 0;

	rendercontext->planecontext.flattimetaken = 0;

	rendercontext->spritecontext.maskedtimetaken = 0;
#endif

	if( voidcleartype == Void_Black )
	{
		memset( output, 0, outputsize );
	}
	else if( voidcleartype == Void_Whacky )
	{
		memset( output, whacky_void_indices[ ( rendercontext->starttime % whacky_void_microseconds ) / ( whacky_void_microseconds / num_whacky_void_indices ) ], outputsize );
	}
	else if( voidcleartype == Void_Sky )
	{
		skycontext.colfunc = colfuncs[ M_MIN( ( ( pspriteiscale >> detailshift ) >> 12 ), 15 ) ];
		skycontext.iscale = pspriteiscale>>detailshift;
		skycontext.scale = pspritescale>>detailshift;
		skycontext.texturemid = skytexturemid;
		skycontext.output = rendercontext->buffer;
		skycontext.yl = 0;
		skycontext.yh = render_height - ST_BUFFERHEIGHT;

		for ( x = rendercontext->begincolumn; x < rendercontext->endcolumn; ++x )
		{
			angle = ( viewangle + xtoviewangle[ x ] ) >> ANGLETOSKYSHIFT;

			skycontext.x = x;
			skycontext.source = R_GetColumn( skytexture, angle, 0 );
			skycontext.colfunc( &skycontext );
		}
	}
		
	memset( rendercontext->spritecontext.sectorvisited, 0, sizeof( boolean ) * numsectors );

	R_RenderBSPNode( &rendercontext->buffer, &rendercontext->bspcontext, &rendercontext->planecontext, &rendercontext->spritecontext, numnodes-1 );
	R_ErrorCheckPlanes( rendercontext );
	R_DrawPlanes( &rendercontext->buffer, &rendercontext->planecontext );
	R_DrawMasked( &rendercontext->buffer, &rendercontext->spritecontext, &rendercontext->bspcontext );

	rendercontext->endtime = I_GetTimeUS();
	rendercontext->timetaken = rendercontext->endtime - rendercontext->starttime;

#if RENDER_PERF_GRAPHING
	rendercontext->frametimes[ rendercontext->nextframetime ] = (float_t)rendercontext->timetaken / 1000.f;
	rendercontext->walltimes[ rendercontext->nextframetime ] = (float_t)( rendercontext->bspcontext.solidtimetaken + rendercontext->bspcontext.maskedtimetaken ) / 1000.f;
	rendercontext->flattimes[ rendercontext->nextframetime ] = (float_t)rendercontext->planecontext.flattimetaken / 1000.f;
	rendercontext->spritetimes[ rendercontext->nextframetime ] = (float_t)rendercontext->spritecontext.maskedtimetaken / 1000.f;
	rendercontext->everythingelsetimes[ rendercontext->nextframetime ] = (float_t)( rendercontext->timetaken
																					- rendercontext->bspcontext.solidtimetaken 
																					- rendercontext->bspcontext.maskedtimetaken
																					- rendercontext->planecontext.flattimetaken
																					- rendercontext->spritecontext.maskedtimetaken ) / 1000.f;

	rendercontext->nextframetime = ( rendercontext->nextframetime + 1 ) % MAXPROFILETIMES;

	rendercontext->frameaverage = 0;
	for( currtime = 0; currtime < MAXPROFILETIMES; ++currtime )
	{
		rendercontext->frameaverage += rendercontext->frametimes[ currtime ];
	}
	rendercontext->frameaverage /= MAXPROFILETIMES;
#endif
}

#include "hu_lib.h"
#include "hu_stuff.h"
extern patch_t*		hu_font[HU_FONTSIZE];

int32_t R_ContextThreadFunc( void* userdata )
{
	renderdata_t* data = (renderdata_t*)userdata;
	int32_t ret = 0;

	I_AtomicExchange( &data->running, 1 );

	while( !I_AtomicLoad( &data->shouldquit ) )
	{
		if( I_AtomicExchange( &data->framewaiting, 0 ) )
		{
			R_RenderViewContext( &data->context );
			I_AtomicExchange( &data->framefinished, 1 );
		}

		if( I_AtomicLoad( &renderthreadCPUmelter ) == 0 )
		{
			I_Sleep( 1 );
		}
	}

	I_AtomicExchange( &data->running, 0 );

	return ret;
}

void R_InitContexts( void )
{
	int32_t currcontext;
	int32_t currstart;
	int32_t incrementby;

	currstart = 0;
	incrementby = render_width / numrendercontexts;

	renderdatas = Z_Malloc( sizeof( renderdata_t ) * numrendercontexts, PU_STATIC, NULL );
	memset( renderdatas, 0, sizeof( renderdata_t ) * numrendercontexts );

	for( currcontext = 0; currcontext < numrendercontexts; ++currcontext )
	{
		renderdatas[ currcontext ].context.bufferindex = 0;
		renderdatas[ currcontext ].context.buffer = *I_GetRenderBuffer( 0 );

		renderdatas[ currcontext ].context.begincolumn = renderdatas[ currcontext ].context.spritecontext.leftclip = M_MAX( currstart, 0 );
		currstart += incrementby;
		renderdatas[ currcontext ].context.endcolumn = renderdatas[ currcontext ].context.spritecontext.rightclip = M_MIN( currstart, render_width );

		renderdatas[ currcontext ].context.starttime = 0;
		renderdatas[ currcontext ].context.endtime = 1;
		renderdatas[ currcontext ].context.timetaken = 1;

		R_ResetContext( &renderdatas[ currcontext ].context, renderdatas[ currcontext ].context.begincolumn, renderdatas[ currcontext ].context.endcolumn );

		renderdatas[ currcontext ].context.debugtime = Z_Malloc( sizeof( hu_textline_t ), PU_STATIC, NULL );
		renderdatas[ currcontext ].context.debugpercent = Z_Malloc( sizeof( hu_textline_t ), PU_STATIC, NULL );

		HUlib_initTextLine( renderdatas[ currcontext ].context.debugtime, ( FixedDiv( renderdatas[ currcontext ].context.begincolumn << FRACBITS, V_WIDTHMULTIPLIER ) >> FRACBITS ) + 4, V_VIRTUALHEIGHT - ST_HEIGHT - 17, hu_font, HU_FONTSTART);
		HUlib_initTextLine( renderdatas[ currcontext ].context.debugpercent, ( FixedDiv( renderdatas[ currcontext ].context.begincolumn << FRACBITS, V_WIDTHMULTIPLIER ) >> FRACBITS ) + 4, V_VIRTUALHEIGHT - ST_HEIGHT - 9, hu_font, HU_FONTSTART);

		if( renderthreaded && currcontext < numrendercontexts - 1 )
		{
			renderdatas[ currcontext ].thread = I_ThreadCreate( &R_ContextThreadFunc, &renderdatas[ currcontext ] );
		}

		if( numsectors > 0 )
		{
			renderdatas[ currcontext ].context.spritecontext.sectorvisited = Z_Malloc( sizeof( boolean ) * numsectors, PU_LEVEL, NULL );
		}
	}
}

void R_RefreshContexts( void )
{
	int32_t currcontext;
	if( renderdatas )
	{
		for( currcontext = 0; currcontext < numrendercontexts; ++currcontext )
		{
			renderdatas[ currcontext ].context.spritecontext.sectorvisited = Z_Malloc( sizeof( boolean ) * numsectors, PU_LEVEL, NULL );
#if RENDER_PERF_GRAPHING
			memset( renderdatas[ currcontext ].context.frametimes, 0, sizeof( renderdatas[ currcontext ].context.frametimes) );
			memset( renderdatas[ currcontext ].context.walltimes, 0, sizeof( renderdatas[ currcontext ].context.walltimes) );
			memset( renderdatas[ currcontext ].context.flattimes, 0, sizeof( renderdatas[ currcontext ].context.flattimes) );
			memset( renderdatas[ currcontext ].context.spritetimes, 0, sizeof( renderdatas[ currcontext ].context.spritetimes) );
			memset( renderdatas[ currcontext ].context.everythingelsetimes, 0, sizeof( renderdatas[ currcontext ].context.everythingelsetimes) );
#endif // RENDER_PERF_GRAPHING
		}
	}
	renderrebalancecontexts = true;
}

void R_RebalanceContexts( void )
{
	renderrebalancecontexts = true;
}

//
// R_SetViewSize
// Do not really change anything here,
//  because it might be in the middle of a refresh.
// The change will take effect next refresh.
//
boolean		setsizeneeded;
int		setblocks;
int		setdetail;


void
R_SetViewSize
( int		blocks,
  int		detail )
{
    setsizeneeded = true;
    setblocks = blocks;
    setdetail = detail;
}

typedef enum detail_e
{
	DT_NATIVE,
	DT_CRISPY,
	DT_ORIGINALHIGH,
	DT_ORIGINALLOW,
	DT_VIRTUAL,

	DT_COUNT
} detail_t;

//
// R_ExecuteSetViewSize
//

void R_ExecuteSetViewSize (void)
{
	fixed_t		cosadj;
	fixed_t		dy;
	int32_t		i;
	int32_t		j;
	int32_t		level;
	int32_t		startmap;
	int32_t		colfuncbase;

	fixed_t		original_perspective = FixedDiv( 16 * FRACUNIT, 10 * FRACUNIT );
	fixed_t		current_perspective = FixedDiv( render_width << FRACBITS, render_height << FRACBITS );
	fixed_t		perspective_mul = FixedDiv( original_perspective, current_perspective );

	fixed_t		perspectivecorrectscale;

	fixed_t		tan_fov = FixedDiv( current_perspective >> 1, original_perspective >> 1 );
	float_t		float_tan_fov = (float)tan_fov / 65536.f;
	float_t		float_fov = ( atanf( float_tan_fov ) * 2.f ) / 3.1415926f * 180.f;
	field_of_view_degrees = (int32_t)float_fov;
	//int32_t		new_fov = ( rendertantoangle[ tan_fov >> RENDERDBITS ] ) >> FRACBITS;

	R_InitAspectAdjustedValues();

	setsizeneeded = false;

	//switch( setdetail )
	//{
	//case DT_NATIVE:
	//case DT_VIRTUAL:
	//	scaledviewwidth = SCREENWIDTH;
	//	viewheight = SCREENHEIGHT;
	//	detailshift = 0;
	//	break;
	//case DT_CRISPY:
	//	scaledviewwidth = 640;
	//	viewheight = 400;
	//	detailshift = 0;
	//case DT_ORIGINALHIGH:
	//	scaledviewwidth = 320;
	//	viewheight = 200;
	//	detailshift = 1;
	//	break;
	//case DT_ORIGINALLOW:
	//	scaledviewwidth = 160;
	//	viewheight = 200;
	//	detailshift = 1;
	//	break;
	//}
	//
    //viewwidth = scaledviewwidth>>detailshift;
	//
    //if (setblocks < 11)
    //{
	//	scaledviewwidth = setblocks*viewwidth/10;
	//	viewheight = (setblocks*(SCREENHEIGHT-SBARHEIGHT)/10)&~7;
    //}

	if (setblocks == 11)
	{
		scaledviewwidth = render_width;
		viewheight = render_height;
	}
	else
	{
		scaledviewwidth = setblocks * render_width / 10;
		viewheight = (setblocks * ( render_height - SBARHEIGHT ) / 10 ) & ~7;
	}

	detailshift = setdetail;
	viewwidth = scaledviewwidth >> detailshift;

	centery = viewheight /2;
	centerx = viewwidth /2;
	centerxfrac = centerx << FRACBITS;
	centeryfrac = centery << FRACBITS;
	projection = FixedMul( centerxfrac, perspective_mul );

	colfuncbase = COLFUNC_NUM * ( detailshift );
	transcolfunc = colfuncs[ colfuncbase + COLFUNC_TRANSLATEINDEX ];

	if (!detailshift)
	{
		spanfunc = &R_DrawSpan;
	}
	else
	{
		spanfunc = &R_DrawSpanLow;
	}

	R_InitBuffer (scaledviewwidth, viewheight);
	
	R_InitTextureMapping ();

	// psprite scales
	perspectivecorrectscale = ( FixedMul( render_width << FRACBITS, perspective_mul ) / V_VIRTUALWIDTH );

	pspritescale = FixedMul( FRACUNIT * viewwidth / render_width, perspectivecorrectscale );
	pspriteiscale = FixedDiv( FRACUNIT * render_width / viewwidth, perspectivecorrectscale );

	// thing clipping
	for (i=0 ; i<viewwidth ; i++)
	{
		screenheightarray[i] = viewheight;
	}

	// planes
	for (i=0 ; i<viewheight ; i++)
	{
		dy = ( ( i- viewheight / 2 ) << FRACBITS ) + FRACUNIT / 2;
		dy = abs( dy );
		yslope[ i ] = FixedMul( FixedDiv ( ( viewwidth << detailshift ) / 2 * FRACUNIT, dy ), perspective_mul );
	}
	
	for ( i=0 ; i<viewwidth ; i++ )
	{
		cosadj = abs( renderfinecosine[ xtoviewangle[ i ] >> RENDERANGLETOFINESHIFT ] );
		distscale[ i ] = FixedDiv ( FRACUNIT, cosadj );
	}

	// Calculate the light levels to use
	//  for each level / scale combination.
	for ( i=0 ; i < LIGHTLEVELS ; i++ )
	{
		startmap = ( (LIGHTLEVELS - 1 - i ) * 2 ) * NUMLIGHTCOLORMAPS / LIGHTLEVELS;
		for ( j=0 ; j < MAXLIGHTSCALE ; j++ )
		{
			level = startmap - j * render_width / ( viewwidth << detailshift ) / DISTMAP;

			if (level < 0)
			{
				level = 0;
			}

			if (level >= NUMLIGHTCOLORMAPS)
			{
				level = NUMLIGHTCOLORMAPS-1;
			}

			scalelight[ i ][ j ] = colormaps + level * 256;
			scalelightindex[ i ][ j ] = level;
		}
	}
}

static boolean debugwindow_renderthreadinggraphs = false;
static boolean debugwindow_renderthreadingoptions = false;

#if RENDER_PERF_GRAPHING
static void R_RenderGraphTab( const char* graphname, ptrdiff_t frameavgoffs, ptrdiff_t backgroundoffs, ptrdiff_t dataoffs, int32_t datalen, ptrdiff_t nextentryoffs )
{
	int32_t currcontext = 0;
	int32_t currtime;

	byte* context;
	int32_t nextentry;

	float_t* data;
	float_t frameavg;
	float_t times[ MAXPROFILETIMES ];
	float_t average;
	char overlay[ 64 ];
	ImVec2 objsize;
	ImVec2 cursorpos;
	ImVec4 nobackground = { 0, 0, 0, 0 };
	ImVec4 barcolor = igGetStyle()->Colors[ ImGuiCol_TextDisabled ];

	memset( times, 0, sizeof( times ) );

	if( igBeginTabItem( graphname, NULL, ImGuiTabItemFlags_NoCloseWithMiddleMouseButton ) )
	{
		igSliderInt( "Time scale (ms)", &performancegraphscale, 5, 100, "%dms", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput );
		igNewLine();
#if !defined( NDEBUG )
		igText( "YOU ARE PROFILING A DEBUG BUILD, THIS WILL GIVE YOU WRONG RESULTS" );
		igNewLine();
#endif // !defined( NDEBUG );

		igBeginColumns( "Performance columns", M_MIN( 2, numusablerendercontexts ), ImGuiColumnsFlags_NoBorder );
		for( currcontext = 0; currcontext < numusablerendercontexts; ++currcontext )
		{
			igText( "Context %d", currcontext );

			// This is downright ugly. Don't ever do this
			context = (byte*)&renderdatas[ currcontext ];
			nextentry = *(int32_t*)( context + nextentryoffs );
			frameavg = frameavgoffs >= 0 ? *(float_t*)( context + frameavgoffs ) : -1.f;
			objsize.x = igGetColumnWidth( igGetColumnIndex() );
			objsize.y = fminf( objsize.x * 0.75f, 180.f );

			if( backgroundoffs >= 0 )
			{
				igGetCursorPos( &cursorpos );
				data = (float_t*)( context + backgroundoffs );
				for( currtime = 0; currtime < datalen; ++currtime )
				{
					times[ currtime ] = data[ ( nextentry + currtime ) % datalen ];
				}

				igPushStyleColorVec4( ImGuiCol_PlotHistogram, barcolor );
				igPlotHistogramFloatPtr( "Frametime", times, datalen, 0, NULL, 0.f, (float_t)performancegraphscale, objsize, sizeof(float_t) );
				igPopStyleColor( 1 );

				igSetCursorPos( cursorpos );
			}

			data = (float_t*)( context + dataoffs );

			average = 0;
			for( currtime = 0; currtime < datalen; ++currtime )
			{
				times[ currtime ] = data[ ( nextentry + currtime ) % datalen ];
				average += times[ currtime ];
			}
			average /= datalen;
			if( frameavg < 0.f )
			{
				sprintf( overlay, "avg: %0.2fms", average );
			}
			else
			{
				sprintf( overlay, "avg: %0.2fms / %0.2fms", average, frameavg );
			}

			if( backgroundoffs >= 0 ) igPushStyleColorVec4( ImGuiCol_FrameBg, nobackground );
			igPlotHistogramFloatPtr( "Frametime", times, datalen, 0, overlay, 0.f, (float_t)performancegraphscale, objsize, sizeof(float_t) );
			if( backgroundoffs >= 0 ) igPopStyleColor( 1 );

			igNewLine();
			igNextColumn();
		}
		igEndColumns();

		igEndTabItem();
	}
}

static void R_RenderThreadingGraphsWindow( const char* name, void* data )
{
	// This is downright ugly. Don't ever do this
	ptrdiff_t frametimesoff = offsetof( renderdata_t, context.frametimes );
	ptrdiff_t walltimesoff = offsetof( renderdata_t, context.walltimes );
	ptrdiff_t flattimesoff = offsetof( renderdata_t, context.flattimes );
	ptrdiff_t spritetimesoff = offsetof( renderdata_t, context.spritetimes );
	ptrdiff_t elsetimesoff = offsetof( renderdata_t, context.everythingelsetimes );
	ptrdiff_t avgtimeoff = offsetof( renderdata_t, context.frameaverage );
	ptrdiff_t nexttimeoff = offsetof( renderdata_t, context.nextframetime );

	if( igBeginTabBar( "Threading tabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_NoCloseWithMiddleMouseButton ) )
	{
		R_RenderGraphTab( "Overall",			-1,				-1,					frametimesoff,		MAXPROFILETIMES,	nexttimeoff );
		R_RenderGraphTab( "Walls",				avgtimeoff,		frametimesoff,		walltimesoff,		MAXPROFILETIMES,	nexttimeoff );
		R_RenderGraphTab( "Flats",				avgtimeoff,		frametimesoff,		flattimesoff,		MAXPROFILETIMES,	nexttimeoff );
		R_RenderGraphTab( "Sprites",			avgtimeoff,		frametimesoff,		spritetimesoff,		MAXPROFILETIMES,	nexttimeoff );
		R_RenderGraphTab( "Everything else",	avgtimeoff,		frametimesoff,		elsetimesoff,		MAXPROFILETIMES,	nexttimeoff );

		igEndTabBar();
	}
}
#endif // RENDER_PERF_GRAPHING

static void R_RenderThreadingOptionsWindow( const char* name, void* data )
{
	boolean WorkingBool = I_AtomicLoad( &renderthreadCPUmelter ) != 0;

	igText( "Performance options" );
	igSeparator();
	igSliderInt( "Running threads", &numusablerendercontexts, 1, numrendercontexts, "%d", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput );

	igPushIDPtr( &renderthreadCPUmelter );
	if( igCheckbox( "CPU melter (nosleep)", &WorkingBool ) )
	{
		I_AtomicExchange( &renderthreadCPUmelter, (atomicval_t)!!WorkingBool );
	}
	if( igIsItemHovered( ImGuiHoveredFlags_None ) )
	{
		igBeginTooltip();
		igText( "For when you absolutely, positively don't care\n"
				"about power usage or balancing CPU resources.\n"
				"Brute force eliminates main thread stall. Don't\n"
				"complain if your CPU overheats, it was your own\n"
				"choice to turn this option on." );
		igEndTooltip();
	}
	igPopID();

	igCheckbox( "Load balancing", &renderloadbalancing );
	igCheckbox( "SIMD columns", &renderSIMDcolumns );
	igNewLine();
	igText( "Debug options" );
	igSeparator();
	igCheckbox( "Visualise split", &rendersplitvisualise );
	if( igSliderInt( "Horizontal FOV", &field_of_view_degrees, 60, 160, "%d", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput ) )
	{
		R_ExecuteSetViewSize();
	}
}

//
// R_Init
//
void R_Init (void)
{
	R_InitAspectAdjustedValues();

	R_InitColFuncs();

    R_InitData ();
    printf (".");
    R_InitPointToAngle ();
    printf (".");
    R_InitTables ();
    // viewwidth / viewheight / detailLevel are set by the defaults
    printf (".");

    R_SetViewSize (screenblocks, detailLevel);
    R_InitPlanes ();
    printf (".");
    R_InitLightTables ();
    printf (".");
    R_InitSkyMap ();
    R_InitTranslationTables ();
    printf (".");

	M_RegisterDebugMenuRadioButton( "Render|Clear style|None", NULL, &voidcleartype, Void_NoClear );
	M_RegisterDebugMenuRadioButton( "Render|Clear style|Black", NULL, &voidcleartype, Void_Black );
	M_RegisterDebugMenuRadioButton( "Render|Clear style|Whacky", NULL, &voidcleartype, Void_Whacky );
	M_RegisterDebugMenuRadioButton( "Render|Clear style|Sky", NULL, &voidcleartype, Void_Sky );

	M_RegisterDebugMenuWindow( "Render|Threading|Options", "Render Threading Options", 500, 500, &debugwindow_renderthreadingoptions, Menu_Normal, &R_RenderThreadingOptionsWindow );
#if RENDER_PERF_GRAPHING
	M_RegisterDebugMenuWindow( "Render|Threading|Graphs", "Render Graphs", 500, 550, &debugwindow_renderthreadinggraphs, Menu_Overlay, &R_RenderThreadingGraphsWindow );
#endif //RENDER_PERF_GRAPHING
	
    framecount = 0;
}

void R_RenderDimensionsChanged( void )
{
	extern int detailLevel;
	extern int screenblocks;
	extern boolean refreshstatusbar;

	refreshstatusbar = true;
	V_RestoreBuffer();
	R_InitLightTables();
	R_InitPointToAngle();
	R_InitTables();
	// Any other buffers?
	R_SetViewSize( screenblocks, detailLevel );
	R_ExecuteSetViewSize();
}

//
// R_PointInSubsector
//
subsector_t*
R_PointInSubsector
( fixed_t	x,
  fixed_t	y )
{
    node_t*	node;
    int		side;
    int		nodenum;

    // single subsector is a special case
    if (!numnodes)				
	return subsectors;
		
    nodenum = numnodes-1;

    while (! (nodenum & NF_SUBSECTOR) )
    {
	node = &nodes[nodenum];
	side = R_PointOnSide (x, y, node);
	nodenum = node->children[side];
    }
	
    return &subsectors[nodenum & ~NF_SUBSECTOR];
}

//
// R_SetupFrame
//
void R_SetupFrame (player_t* player)
{		
	int32_t		i;
	int32_t		currcontext;
	int32_t		currstart;
	int32_t		desiredwidth;

	double_t	lastframetime;
	double_t	currpercentage;
	double_t	percentagedebt;

	double_t	idealpercentage = 1.0 / (double_t)numusablerendercontexts;

	viewplayer = player;
	viewx = player->mo->x;
	viewy = player->mo->y;
	viewangle = player->mo->angle + viewangleoffset;
	extralight = player->extralight;

	viewz = player->viewz;

	viewsin = renderfinesine[viewangle>>RENDERANGLETOFINESHIFT];
	viewcos = renderfinecosine[viewangle>>RENDERANGLETOFINESHIFT];
	
	fixedcolormapindex = player->fixedcolormap;
	if (player->fixedcolormap)
	{
		fixedcolormap =
			colormaps
			+ player->fixedcolormap*256;
	
		//walllights = scalelightfixed;
		//walllightsindex = fixedcolormapindex;

		for (i=0 ; i<MAXLIGHTSCALE ; i++)
			scalelightfixed[i] = fixedcolormap;
	}
	else
	{
		fixedcolormap = 0;
	}

	if( renderloadbalancing && !renderrebalancecontexts )
	{
		lastframetime = 0;
		percentagedebt = 0;
		currstart = 0;
		for( currcontext = 0; currcontext < numusablerendercontexts; ++currcontext )
		{
			lastframetime += renderdatas[ currcontext ].context.timetaken;
		}

		for( currcontext = 0; currcontext < numusablerendercontexts; ++currcontext )
		{
			currpercentage = renderdatas[ currcontext ].context.timetaken / lastframetime;
			if( currpercentage > idealpercentage )
			{
				currpercentage -= 0.05f;
				percentagedebt += 0.05f;
			}
			else if( currpercentage < idealpercentage && percentagedebt > 0 )
			{
				currpercentage += percentagedebt;
				percentagedebt = 0;
			}

			desiredwidth = (int32_t)( viewwidth * currpercentage );

			R_ResetContext( &renderdatas[ currcontext ].context, M_MAX( currstart, 0 ), M_MIN( currstart + desiredwidth, viewwidth ) );
			currstart += desiredwidth;
		}
	}

	if( !renderloadbalancing || renderrebalancecontexts )
	{
		currstart = 0;

		desiredwidth = viewwidth / numusablerendercontexts;
		for( currcontext = 0; currcontext < numusablerendercontexts; ++currcontext )
		{
			renderdatas[ currcontext ].context.buffer = *I_GetRenderBuffer( 0 );
			renderdatas[ currcontext ].context.begincolumn = renderdatas[ currcontext ].context.spritecontext.leftclip = M_MAX( currstart, 0 );
			currstart += desiredwidth;
			renderdatas[ currcontext ].context.endcolumn = renderdatas[ currcontext ].context.spritecontext.rightclip = M_MIN( currstart, viewwidth );

			R_ResetContext( &renderdatas[ currcontext ].context, renderdatas[ currcontext ].context.begincolumn, renderdatas[ currcontext ].context.endcolumn );
		}

		renderrebalancecontexts = false;
	}

	framecount++;
}

//
// R_RenderView
//
#define MAXWIDTH			(MAXSCREENWIDTH + ( MAXSCREENWIDTH >> 1) )
#define MAXHEIGHT			(MAXSCREENHEIGHT + ( MAXSCREENHEIGHT >> 1) )

extern size_t			xlookup[MAXWIDTH];
extern size_t			rowofs[MAXHEIGHT];

static void R_DrawMsg( const char* msg, hu_textline_t* line )
{
	const char* working = msg;
	HUlib_clearTextLine( line );
	while( *working )
	{
		HUlib_addCharToTextLine( line, *working++ );
	}
	HUlib_drawTextLine( line, false );
}

void R_RenderPlayerView (player_t* player)
{	
	int32_t currcontext;
	int32_t finishedcontexts;
	uint64_t looptimestart;
	uint64_t looptimeend;

	byte* outputcolumn;
	byte* endcolumn;

	double_t totaltime = 0;
	hu_textline_t* debugtime;
	hu_textline_t* debugpercent;

	hu_textline_t debugtotaltime;
	HUlib_initTextLine( &debugtotaltime, 1, V_VIRTUALHEIGHT - ST_HEIGHT - 30, hu_font, HU_FONTSTART);

	char outputbuffer[ HU_MAXLINELENGTH+1 ];

	R_SetupFrame (player);

	// NetUpdate can cause lump loads, so we wait until rendering is done before doing it again.
	// This is a change from the vanilla renderer.
	NetUpdate ();

	wadrenderlock = true;

	if( rendersplitvisualise ) looptimestart = I_GetTimeUS();

	finishedcontexts = 0;

	for( currcontext = 0; currcontext < numusablerendercontexts; ++currcontext )
	{
		if( renderthreaded && currcontext < numusablerendercontexts - 1 )
		{
			I_AtomicExchange( &renderdatas[ currcontext ].framewaiting, 1 );
		}
		else
		{
			R_RenderViewContext( &renderdatas[ currcontext ].context );
			++finishedcontexts;
		}
	}

	while( renderthreaded && finishedcontexts != numusablerendercontexts )
	{
		for( currcontext = 0; currcontext < numusablerendercontexts; ++currcontext )
		{
			finishedcontexts += I_AtomicExchange( &renderdatas[ currcontext ].framefinished, 0 );
		}
		//I_Sleep( 0 );
	}

	if( rendersplitvisualise )
	{
		looptimeend = I_GetTimeUS();

		memset( outputbuffer, 0, sizeof( outputbuffer ) );
		sprintf( outputbuffer, "Loop time: %0.1fms", (looptimeend - looptimestart ) / 1000.0 );
		R_DrawMsg( outputbuffer, &debugtotaltime );

		for( currcontext = 0; currcontext < numusablerendercontexts; ++currcontext )
		{
			totaltime += renderdatas[ currcontext ].context.timetaken;
		}

		for( currcontext = 0; currcontext < numusablerendercontexts; ++currcontext )
		{
			debugtime = (hu_textline_t*)renderdatas[ currcontext ].context.debugtime;
			debugpercent = (hu_textline_t*)renderdatas[ currcontext ].context.debugpercent;

			memset( outputbuffer, 0, sizeof( outputbuffer ) );
			sprintf( outputbuffer, "%0.1fms", ( renderdatas[ currcontext ].context.timetaken / 1000.0 ) );
			R_DrawMsg( outputbuffer, debugtime );

			memset( outputbuffer, 0, sizeof( outputbuffer ) );
			sprintf( outputbuffer, "%0.1f%%", ( renderdatas[ currcontext ].context.timetaken / totaltime ) * 100.0 );
			R_DrawMsg( outputbuffer, debugpercent );

			if( currcontext > 0 )
			{
				outputcolumn = I_VideoBuffer + xlookup[ renderdatas[ currcontext ].context.begincolumn + 1 ] + rowofs[ 0 ];
				endcolumn = outputcolumn + viewheight;

				while( outputcolumn != endcolumn )
				{
					*outputcolumn = 249;
					++outputcolumn;
				}
			}
		}
	}

	wadrenderlock = false;

	// And now, back to your regular programming.
	NetUpdate ();
}
