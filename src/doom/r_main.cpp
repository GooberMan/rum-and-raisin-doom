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


#include "doomdef.h"
#include "doomtype.h"
#include "m_fixed.h"
#include "r_main.h"
#include "i_terminal.h"

extern "C"
{
	#include <stdlib.h>
	#include <math.h>
	#include <stddef.h>

	#include "d_loop.h"

	#include "m_bbox.h"
	#include "m_config.h"
	#include "m_menu.h"

	#include "cimguiglue.h"

	#include "r_local.h"
	#include "r_sky.h"

	// Need ST_HEIGHT
	#include "st_stuff.h"
	#include "m_misc.h"
	#include "w_wad.h"

	#include "z_zone.h"

	#include "i_thread.h"

	#include "tables.h"

	#define SBARHEIGHT				( ( ( (int64_t)( ST_HEIGHT << FRACBITS ) * (int64_t)V_HEIGHTMULTIPLIER ) >> FRACBITS ) >> FRACBITS )

	// Fineangles in the SCREENWIDTH wide window.
	// If we define this as FINEANGLES / 4 then we get auto 90 degrees everywhere
	#define FIELDOFVIEW				( FINEANGLES * ( field_of_view_degrees * 100 ) / 36000 )
	#define RENDERFIELDOFVIEW		( RENDERFINEANGLES * ( field_of_view_degrees * 100 ) / 36000 )

	#define MAXWIDTH				(MAXSCREENWIDTH + ( MAXSCREENWIDTH >> 1) )
	#define MAXHEIGHT				(MAXSCREENHEIGHT + ( MAXSCREENHEIGHT >> 1) )

	#define DEFAULT_RENDERCONTEXTS 4
	#define DEFAULT_MAXRENDERCONTEXTS 8

	typedef struct renderdata_s
	{
		rendercontext_t		context;
		threadhandle_t		thread;
		semaphore_t			shouldrun;
		atomicval_t			running;
		atomicval_t			shouldquit;
		atomicval_t			framewaiting;
		atomicval_t			framefinished;
		int32_t				index;
	} renderdata_t;

	renderdata_t*			renderdatas;

	int32_t					field_of_view_degrees = 90;

	int32_t					numrendercontexts = DEFAULT_MAXRENDERCONTEXTS;
	int32_t					numusablerendercontexts = DEFAULT_RENDERCONTEXTS;
	boolean					renderloadbalancing = true;
	boolean					rendersplitvisualise = false;
	boolean					renderrebalancecontexts = false;
	float_t					rebalancescale = 0.025f;
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
	rend_fixed_t			projection;

	// just for profiling purposes
	int32_t					framecount;	

	fixed_t					viewx;
	fixed_t					viewy;
	fixed_t					viewz;

	angle_t					viewangle;

	fixed_t					viewcos;
	fixed_t					viewsin;

	rend_fixed_t			viewlerp;

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
	angle_t*			xtoviewangle = NULL;

	lighttable_t*		scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
	lighttable_t*		scalelightfixed[MAXLIGHTSCALE];
	int32_t				scalelightindex[LIGHTLEVELS][MAXLIGHTSCALE];
	lighttable_t*		zlight[LIGHTLEVELS][MAXLIGHTZ];
	int32_t				zlightindex[LIGHTLEVELS][MAXLIGHTZ];
	colfunc_t			colfuncs[ COLFUNC_COUNT ];

	// bumped light from gun blasts
	int32_t				extralight;

	colfunc_t			transcolfunc;

	int32_t				fuzz_style = Fuzz_Adjusted;

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

	int32_t aspect_adjusted_render_width = 0;
	rend_fixed_t aspect_adjusted_scaled_divide = 0;
	rend_fixed_t aspect_adjusted_scaled_mul = RENDFRACUNIT;

	extern int32_t enable_frame_interpolation;
	int32_t interpolate_this_frame = 0;

	boolean		setsizeneeded;
	int		setblocks;
	int		setdetail;

	extern int32_t remove_limits;
	extern int detailLevel;
	extern int screenblocks;
	extern int numflats;
	extern int numtextures;
	extern boolean refreshstatusbar;
	extern int mouseSensitivity;
	extern boolean renderpaused;
	extern int32_t span_override;

	extern uint64_t frametime;
}

// Actually expects C++ linkage
atomicval_t	renderscratchpos = 0;
atomicval_t	renderscratchsize = 0;
byte*		renderscratch = nullptr;


#include "m_dashboard.h"
#include "m_profile.h"

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


lineside_t BSP_PointOnSide( fixed_t x, fixed_t y, node_t* node )
{
	if ( !node->divline.dx )
	{
		if ( x <= node->divline.x )
		{
			return node->divline.dy > 0 ? LS_Back : LS_Front;
		}
	
		return node->divline.dy < 0 ? LS_Back : LS_Front;
	}
	if ( !node->divline.dy )
	{
		if ( y <= node->divline.y )
		{
			return node->divline.dx < 0 ? LS_Back : LS_Front;
		}
	
		return node->divline.dx > 0 ? LS_Back : LS_Front;
	}
	
	fixed_t dx		= x - node->divline.x;
	fixed_t dy		= y - node->divline.y;

	#define SIGNBIT 0x80000000
	
	// Try to quickly decide by looking at sign bits.
	if( ( node->divline.dy ^ node->divline.dx ^ dx ^ dy ) & SIGNBIT )
	{
		if( (node->divline.dy ^ dx ) & SIGNBIT )
		{
			// (left is negative)
			return LS_Back;
		}
		return LS_Front;
	}

	rend_fixed_t left	= FixedMul( FixedToInt( node->divline.dy ), dx );
	rend_fixed_t right	= FixedMul( dy , FixedToInt( node->divline.dx ) );
	
	if (right < left)
	{
		// front side
		return LS_Front;
	}

	// back side
	return LS_Back;
}

lineside_t R_PointOnSide( rend_fixed_t x, rend_fixed_t y, node_t* node )
{
	if ( !node->rend.dx )
	{
		if ( x <= node->rend.x )
		{
			return node->rend.dy > 0 ? LS_Back : LS_Front;
		}
	
		return node->rend.dy < 0 ? LS_Back : LS_Front;
	}
	if ( !node->rend.dy )
	{
		if ( y <= node->rend.y )
		{
			return node->rend.dx < 0 ? LS_Back : LS_Front;
		}
	
		return node->rend.dx > 0 ? LS_Back : LS_Front;
	}
	
	rend_fixed_t dx		= x - node->rend.x;
	rend_fixed_t dy		= y - node->rend.y;

	#define RENDSIGNBIT 0x8000000000000000ll
	
	// Try to quickly decide by looking at sign bits.
	if( ( node->rend.dy ^ node->rend.dx ^ dx ^ dy ) & RENDSIGNBIT )
	{
		if( (node->rend.dy ^ dx ) & RENDSIGNBIT )
		{
			// (left is negative)
			return LS_Back;
		}
		return LS_Front;
	}

	rend_fixed_t left	= RendFixedMul( RendFixedToInt( node->rend.dy ), dx );
	rend_fixed_t right	= RendFixedMul( dy , RendFixedToInt( node->rend.dx ) );
	
	if (right < left)
	{
		// front side
		return LS_Front;
	}

	// back side
	return LS_Back;
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




angle_t R_PointToAngle( rend_fixed_t fixed_x, rend_fixed_t fixed_y )
{
	rend_fixed_t x = fixed_x - FixedToRendFixed( viewx );
	rend_fixed_t y = fixed_y - FixedToRendFixed( viewy );

	if ( !x && !y )
	{
		return 0;
	}

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
angle_t BSP_PointToAngle( fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2 )
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


rend_fixed_t R_PointToDist( rend_fixed_t x, rend_fixed_t y )
{
	angle_t			angle;
	rend_fixed_t	dx;
	rend_fixed_t	dy;
	rend_fixed_t	temp;
	rend_fixed_t	dist;
	rend_fixed_t	frac;
	
	dx = llabs( x - FixedToRendFixed( viewx ) );
	dy = llabs( y - FixedToRendFixed( viewy ) );
	
	if (dy>dx)
	{
		temp = dx;
		dx = dy;
		dy = temp;
	}

	// Fix crashes in udm1.wad

	if (dx != 0)
	{
		frac = RendFixedDiv( dy, dx );
	}
	else
	{
		frac = 0;
	}

	rend_fixed_t lookup = frac >> ( RENDERDBITS + RENDFRACTOFRACBITS );
	
	angle = rendertantoangle[ lookup ] + ANG90;
	rend_fixed_t sine = FixedToRendFixed( renderfinesine[ angle  >> RENDERANGLETOFINESHIFT ] );

	// use as cosine
	dist = RendFixedDiv( dx, sine );
	
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
#define MAXSCALE ( 128 * ( render_width / 320 ) )
#define MAXSCALE_FIXED IntToRendFixed( MAXSCALE )

rend_fixed_t R_ScaleFromGlobalAngle( angle_t visangle, rend_fixed_t distance, angle_t view_angle, angle_t normal_angle )
{
	// TODO: Replace this with the scaling function for my visplane renderer
	rend_fixed_t	scale;
	angle_t			anglea;
	angle_t			angleb;
	rend_fixed_t	sinea;
	rend_fixed_t	sineb;
	rend_fixed_t	num;
	rend_fixed_t	den;

	scale = MAXSCALE_FIXED;

	anglea = ANG90 + ( visangle - view_angle );
	angleb = ANG90 + ( visangle - normal_angle );

	// both sines are allways positive
	sinea = FixedToRendFixed( renderfinesine[ anglea >> RENDERANGLETOFINESHIFT ] );
	sineb = FixedToRendFixed( renderfinesine[ angleb >> RENDERANGLETOFINESHIFT ] );
	num = RendFixedMul( projection, sineb ) << detailshift;
	den = RendFixedMul( distance, sinea );

	if (den >= 0 && den > RendFixedToInt( num ) )
	{
		scale = M_CLAMP( RendFixedDiv( num, den ), 256, MAXSCALE_FIXED );
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
		if ( renderfinetangent[i] > IntToFixed( 8 ) )
			t = -1;
		else if ( renderfinetangent[i] < IntToFixed( -8 ) )
			t = viewwidth+1;
		else
		{
			t = FixedMul (renderfinetangent[i], focallength);
			t = FixedToInt( (centerxfrac - t+FRACUNIT-1) );

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
	if( xtoviewangle )
	{
		Z_Free( xtoviewangle );
	}
	xtoviewangle = (angle_t*)Z_Malloc( sizeof( angle_t) * ( viewwidth + 1 ), PU_STATIC, NULL );

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


void R_InitAspectAdjustedValues()
{
	rend_fixed_t		original_perspective = RendFixedDiv( IntToRendFixed( 16 ), IntToRendFixed( 10 ) );
	rend_fixed_t		current_perspective = RendFixedDiv( IntToRendFixed( render_width ), IntToRendFixed( render_height ) );
	rend_fixed_t		perspective_mul = RendFixedDiv( original_perspective, current_perspective );

	rend_fixed_t		intermediate_width = RendFixedMul( IntToRendFixed( render_width ), perspective_mul );
	aspect_adjusted_render_width = RendFixedToInt( intermediate_width ) + ( ( intermediate_width & ( RENDFRACUNIT >> 1 ) ) >> ( RENDFRACBITS - 1) );
	aspect_adjusted_scaled_divide = IntToRendFixed( aspect_adjusted_render_width ) / 320;
	aspect_adjusted_scaled_mul = RendFixedDiv( RENDFRACUNIT, aspect_adjusted_scaled_divide );
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
			scale = FixedDiv( IntToFixed( aspect_adjusted_render_width / 2 ), ( j + 1 ) << LIGHTZSHIFT );
			scale >>= LIGHTSCALESHIFT;
			if( LIGHTSCALEMUL != RENDFRACUNIT )
			{
				scale = RendFixedToInt( RendFixedMul( IntToRendFixed( scale ), LIGHTSCALEMUL ) );
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
	colfuncs[ 1 ]	= remove_limits ? &R_LimitRemovingDrawColumn : &R_DrawColumn;
	colfuncs[ 2 ]	= remove_limits ? &R_LimitRemovingDrawColumn : &R_DrawColumn;
	colfuncs[ 3 ]	= remove_limits ? &R_LimitRemovingDrawColumn : &R_DrawColumn;
	colfuncs[ 4 ]	= remove_limits ? &R_LimitRemovingDrawColumn : &R_DrawColumn;
	colfuncs[ 5 ]	= remove_limits ? &R_LimitRemovingDrawColumn : &R_DrawColumn;
	colfuncs[ 6 ]	= remove_limits ? &R_LimitRemovingDrawColumn : &R_DrawColumn;
	colfuncs[ 7 ]	= remove_limits ? &R_LimitRemovingDrawColumn : &R_DrawColumn;
	colfuncs[ 8 ]	= remove_limits ? &R_LimitRemovingDrawColumn : &R_DrawColumn;
	colfuncs[ 9 ]	= remove_limits ? &R_LimitRemovingDrawColumn : &R_DrawColumn;
	colfuncs[ 10 ]	= remove_limits ? &R_LimitRemovingDrawColumn : &R_DrawColumn;
	colfuncs[ 11 ]	= remove_limits ? &R_LimitRemovingDrawColumn : &R_DrawColumn;
	colfuncs[ 12 ]	= remove_limits ? &R_LimitRemovingDrawColumn : &R_DrawColumn;
	colfuncs[ 13 ]	= remove_limits ? &R_LimitRemovingDrawColumn : &R_DrawColumn;
	colfuncs[ 14 ]	= remove_limits ? &R_LimitRemovingDrawColumn : &R_DrawColumn;
	colfuncs[ 15 ]	= remove_limits ? &R_LimitRemovingDrawColumn : &R_DrawColumn;
#endif
#endif //R_DRAWCOLUMN_SIMDOPTIMISED

	int32_t currexpand = COLFUNC_PIXELEXPANDERS;

	while( currexpand > 0 )
	{
		--currexpand;
#if !R_DRAWCOLUMN_SIMDOPTIMISED
		colfuncs[ currexpand ] = remove_limits ? &R_LimitRemovingDrawColumn : &R_DrawColumn;
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
	transcolfunc = colfuncs[ COLFUNC_TRANSLATEINDEX ] = colfuncs[ COLFUNC_NUM + COLFUNC_TRANSLATEINDEX ] = &R_DrawTranslatedColumn;
}


void R_ResetContext( rendercontext_t* context, int32_t leftclip, int32_t rightclip )
{
	M_PROFILE_FUNC();

	context->begincolumn = context->spritecontext.leftclip = leftclip;
	context->endcolumn = context->spritecontext.rightclip = rightclip;

	R_ClearClipSegs( &context->bspcontext, leftclip, rightclip );
	R_ClearDrawSegs( &context->bspcontext );
	R_ClearPlanes( &context->planecontext, viewwidth, viewheight, viewangle );
	R_ClearSprites( &context->spritecontext );
}

void R_RenderViewContext( rendercontext_t* rendercontext )
{
	M_PROFILE_FUNC();

	colcontext_t	skycontext;
	int32_t			x;
	int32_t			angle;

	int32_t			currtime;
	byte*			output		= rendercontext->buffer.data + rendercontext->buffer.pitch * rendercontext->begincolumn;
	size_t			outputsize	= rendercontext->buffer.pitch * (rendercontext->endcolumn - rendercontext->begincolumn );

	rendercontext->starttime = I_GetTimeUS();
#if RENDER_PERF_GRAPHING
	rendercontext->bspcontext.storetimetaken = 0;
	rendercontext->bspcontext.solidtimetaken = 0;
	rendercontext->bspcontext.maskedtimetaken = 0;
	rendercontext->bspcontext.findvisplanetimetaken = 0;
	rendercontext->bspcontext.addspritestimetaken = 0;
	rendercontext->bspcontext.addlinestimetaken = 0;

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
		skycontext.iscale = FixedToRendFixed( pspriteiscale>>detailshift );
		skycontext.texturemid = skytexturemid;
		skycontext.output = rendercontext->buffer;
		skycontext.yl = 0;
		skycontext.yh = render_height - ST_BUFFERHEIGHT;
		skycontext.sourceheight = texturelookup[ skytexture ]->renderheight;

		for ( x = rendercontext->begincolumn; x < rendercontext->endcolumn; ++x )
		{
			angle = ( viewangle + xtoviewangle[ x ] ) >> ANGLETOSKYSHIFT;

			skycontext.x = x;
			skycontext.source = R_GetColumn( skytexture, angle, 0 );
			skycontext.colfunc( &skycontext );
		}
	}
		
	memset( rendercontext->spritecontext.sectorvisited, 0, sizeof( boolean ) * numsectors );

	rendercontext->planecontext.output = rendercontext->viewbuffer;
	rendercontext->planecontext.spantype = span_override;
	if( span_override == Span_None )
	{
		rendercontext->planecontext.spantype = M_MAX( Span_PolyRaster_Log2_4, M_MIN( (int32_t)( log2f( render_height * 0.02f ) + 0.5f ), Span_PolyRaster_Log2_32 ) );
	}
	if( rendercontext->planecontext.spantype == Span_Original )
	{
		I_Error( "I broke the span renderer, it's on its way out now." );
	}

	{
		M_PROFILE_NAMED( "R_RenderBSPNode" );
		R_RenderBSPNode( &rendercontext->viewbuffer, &rendercontext->bspcontext, &rendercontext->planecontext, &rendercontext->spritecontext, numnodes-1 );
	}
	R_ErrorCheckPlanes( rendercontext );
	R_DrawPlanes( &rendercontext->viewbuffer, &rendercontext->planecontext );
	{
		M_PROFILE_NAMED( "R_DrawMasked" );
		R_DrawMasked( &rendercontext->viewbuffer, &rendercontext->spritecontext, &rendercontext->bspcontext );
	}

	rendercontext->endtime = I_GetTimeUS();
	rendercontext->timetaken = rendercontext->endtime - rendercontext->starttime;

#if RENDER_PERF_GRAPHING
	rendercontext->bspcontext.addlinestimetaken -= rendercontext->bspcontext.solidtimetaken;

	rendercontext->frametimes[ rendercontext->nextframetime ] = (float_t)rendercontext->timetaken / 1000.f;
	rendercontext->walltimes[ rendercontext->nextframetime ] = (float_t)( rendercontext->bspcontext.solidtimetaken + rendercontext->bspcontext.maskedtimetaken ) / 1000.f;
	rendercontext->flattimes[ rendercontext->nextframetime ] = (float_t)rendercontext->planecontext.flattimetaken / 1000.f;
	rendercontext->spritetimes[ rendercontext->nextframetime ] = (float_t)rendercontext->spritecontext.maskedtimetaken / 1000.f;
	rendercontext->findvisplanetimes[ rendercontext->nextframetime ] = (float_t)rendercontext->bspcontext.findvisplanetimetaken / 1000.f;
	rendercontext->addspritestimes[ rendercontext->nextframetime ] = (float_t)rendercontext->bspcontext.addspritestimetaken / 1000.f;
	rendercontext->addlinestimes[ rendercontext->nextframetime ] = (float_t)rendercontext->bspcontext.addlinestimetaken / 1000.f;
	rendercontext->everythingelsetimes[ rendercontext->nextframetime ] = (float_t)( rendercontext->timetaken
																					- rendercontext->bspcontext.solidtimetaken 
																					- rendercontext->bspcontext.maskedtimetaken
																					- rendercontext->bspcontext.findvisplanetimetaken
																					- rendercontext->bspcontext.addspritestimetaken
																					- rendercontext->bspcontext.addlinestimetaken
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

	char threadname[ 256 ] = { 0 };
	sprintf( threadname, "Render context %d", data->index );
	M_ProfileThreadInit( threadname );

	I_AtomicExchange( &data->running, 1 );

	while( !I_AtomicLoad( &data->shouldquit ) )
	{
		I_SemaphoreAcquire( data->shouldrun );

		M_ProfileNewFrame();

		R_RenderViewContext( &data->context );

		I_AtomicExchange( &data->framefinished, 1 );
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

	renderdatas = (renderdata_t*)Z_Malloc( sizeof( renderdata_t ) * numrendercontexts, PU_STATIC, NULL );
	memset( renderdatas, 0, sizeof( renderdata_t ) * numrendercontexts );

	for( currcontext = 0; currcontext < numrendercontexts; ++currcontext )
	{
		renderdatas[ currcontext ].index = currcontext;
		renderdatas[ currcontext ].context.bufferindex = 0;
		renderdatas[ currcontext ].context.buffer = renderdatas[ currcontext ].context.viewbuffer = *I_GetRenderBuffer( 0 );

		renderdatas[ currcontext ].context.begincolumn = renderdatas[ currcontext ].context.spritecontext.leftclip = M_MAX( currstart, 0 );
		currstart += incrementby;
		renderdatas[ currcontext ].context.endcolumn = renderdatas[ currcontext ].context.spritecontext.rightclip = M_MIN( currstart, render_width );

		renderdatas[ currcontext ].context.starttime = 0;
		renderdatas[ currcontext ].context.endtime = 1;
		renderdatas[ currcontext ].context.timetaken = 1;

		renderdatas[ currcontext ].context.planecontext.rasterregions = ( rasterregion_t** )Z_Malloc( sizeof( rasterregion_t* ) * ( numflats + numtextures ), PU_STATIC, NULL );

		R_ResetContext( &renderdatas[ currcontext ].context, renderdatas[ currcontext ].context.begincolumn, renderdatas[ currcontext ].context.endcolumn );

		if( renderthreaded && currcontext < numrendercontexts - 1 )
		{
			renderdatas[ currcontext ].shouldrun = I_SemaphoreCreate( 0 );
			renderdatas[ currcontext ].thread = I_ThreadCreate( &R_ContextThreadFunc, &renderdatas[ currcontext ] );
		}

		if( numsectors > 0 )
		{
			renderdatas[ currcontext ].context.spritecontext.sectorvisited = (boolean*)Z_Malloc( sizeof( boolean ) * numsectors, PU_LEVEL, NULL );
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
			renderdatas[ currcontext ].context.spritecontext.sectorvisited = (boolean*)Z_Malloc( sizeof( boolean ) * numsectors, PU_LEVEL, NULL );
#if RENDER_PERF_GRAPHING
			memset( renderdatas[ currcontext ].context.frametimes, 0, sizeof( renderdatas[ currcontext ].context.frametimes) );
			memset( renderdatas[ currcontext ].context.walltimes, 0, sizeof( renderdatas[ currcontext ].context.walltimes) );
			memset( renderdatas[ currcontext ].context.flattimes, 0, sizeof( renderdatas[ currcontext ].context.flattimes) );
			memset( renderdatas[ currcontext ].context.spritetimes, 0, sizeof( renderdatas[ currcontext ].context.spritetimes) );
			memset( renderdatas[ currcontext ].context.findvisplanetimes, 0, sizeof( renderdatas[ currcontext ].context.findvisplanetimes) );
			memset( renderdatas[ currcontext ].context.addspritestimes, 0, sizeof( renderdatas[ currcontext ].context.addspritestimes) );
			memset( renderdatas[ currcontext ].context.addlinestimes, 0, sizeof( renderdatas[ currcontext ].context.addlinestimes) );
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

void
R_SetViewSize
( int		blocks,
  int		detail )
{
    setsizeneeded = true;
    setblocks = blocks;
    setdetail = detail;
	R_RebalanceContexts();
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

DOOM_C_API void R_ExecuteSetViewSize (void)
{
	int32_t				i;
	int32_t				j;
	int32_t				level;
	int32_t				startmap;
	int32_t				colfuncbase;

	rend_fixed_t		original_perspective	= RendFixedDiv( IntToRendFixed( 16 ), IntToRendFixed( 10 ) );
	rend_fixed_t		current_perspective		= RendFixedDiv( IntToRendFixed( render_width ), IntToRendFixed( render_height ) );
	rend_fixed_t		perspective_mul			= RendFixedDiv( original_perspective, current_perspective );

	rend_fixed_t		perspectivecorrectscale;

	rend_fixed_t		tan_fov = RendFixedDiv( current_perspective >> 1, original_perspective >> 1 );

	// Widescreen support needs this
	float_t		float_tan_fov = (float)tan_fov / (float)RENDFRACUNIT;
	float_t		float_fov = ( atanf( float_tan_fov ) * 2.f ) / 3.1415926f * 180.f;

	field_of_view_degrees = (int32_t)float_fov;
	//int32_t		new_fov = FixedToInt( rendertantoangle[ tan_fov >> RENDERDBITS ] );

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
		viewheight = (setblocks * ( render_height - SBARHEIGHT ) / 10 );// & ~7;
	}

	detailshift = setdetail;
	viewwidth = scaledviewwidth >> detailshift;

	centery = viewheight /2;
	centerx = viewwidth /2;
	centerxfrac = IntToFixed( centerx );
	centeryfrac = IntToFixed( centery );
	projection = RendFixedMul( FixedToRendFixed( centerxfrac ), perspective_mul );

	colfuncbase = COLFUNC_NUM * ( detailshift );
	transcolfunc = colfuncs[ colfuncbase + COLFUNC_TRANSLATEINDEX ];

	R_InitBuffer (scaledviewwidth, viewheight);
	
	R_InitTextureMapping ();

	// psprite scales
	perspectivecorrectscale = ( RendFixedMul( IntToRendFixed( render_width ), perspective_mul ) / V_VIRTUALWIDTH );

	pspritescale	= RendFixedToFixed( RendFixedMul( IntToRendFixed( viewwidth ) / render_width, perspectivecorrectscale ) );
	pspriteiscale	= RendFixedToFixed( RendFixedDiv( IntToRendFixed( render_width ) / viewwidth, perspectivecorrectscale ) );

	// thing clipping
	for (i=0 ; i<viewwidth ; i++)
	{
		screenheightarray[i] = viewheight;
	}

	// planes
	if( yslope )
	{
		Z_Free( yslope );
	}
	yslope = (rend_fixed_t*)Z_Malloc( sizeof( rend_fixed_t ) * viewheight, PU_STATIC, NULL );

	for (i=0 ; i<viewheight ; i++)
	{
		rend_fixed_t dy = IntToRendFixed( i- viewheight / 2 ) + RENDFRACUNIT / 2;
		dy = llabs( dy );
		yslope[ i ] = RendFixedMul( RendFixedDiv( IntToRendFixed( ( viewwidth << detailshift ) / 2 ), dy ), perspective_mul );
	}
	
	if( distscale )
	{
		Z_Free( distscale );
	}
	distscale = (rend_fixed_t*)Z_Malloc( sizeof( rend_fixed_t ) * viewwidth, PU_STATIC, NULL );

	for ( i=0 ; i<viewwidth ; i++ )
	{
		rend_fixed_t cosadj = FixedToRendFixed( abs( renderfinecosine[ xtoviewangle[ i ] >> RENDERANGLETOFINESHIFT ] ) );
		distscale[ i ] = RendFixedDiv( RENDFRACUNIT, cosadj );
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
#if !defined( NDEBUG )
		igText( "YOU ARE PROFILING A DEBUG BUILD, THIS WILL GIVE YOU WRONG RESULTS" );
		igNewLine();
#endif // !defined( NDEBUG );

		igSliderInt( "Time scale (ms)", &performancegraphscale, 5, 100, "%dms", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput );
		igText( "Total frame time: %0.3fms (%d FPS)", (float)( frametime * 0.001 ), (int32_t)( ( 1.0 / frametime ) * 1000000.0 ) );
		igNewLine();

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
	ptrdiff_t findvisplanetimesoff = offsetof( renderdata_t, context.findvisplanetimes );
	ptrdiff_t addspritestimesoff = offsetof( renderdata_t, context.addspritestimes );
	ptrdiff_t addlinestimesoff = offsetof( renderdata_t, context.addlinestimes );
	ptrdiff_t elsetimesoff = offsetof( renderdata_t, context.everythingelsetimes );
	ptrdiff_t avgtimeoff = offsetof( renderdata_t, context.frameaverage );
	ptrdiff_t nexttimeoff = offsetof( renderdata_t, context.nextframetime );

	if( igBeginTabBar( "Threading tabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_NoCloseWithMiddleMouseButton ) )
	{
		R_RenderGraphTab( "Overall",			-1,				-1,					frametimesoff,				MAXPROFILETIMES,	nexttimeoff );
		R_RenderGraphTab( "Walls",				avgtimeoff,		frametimesoff,		walltimesoff,				MAXPROFILETIMES,	nexttimeoff );
		R_RenderGraphTab( "Flats",				avgtimeoff,		frametimesoff,		flattimesoff,				MAXPROFILETIMES,	nexttimeoff );
		R_RenderGraphTab( "Sprites",			avgtimeoff,		frametimesoff,		spritetimesoff,				MAXPROFILETIMES,	nexttimeoff );
		R_RenderGraphTab( "Find Visplanes",		avgtimeoff,		frametimesoff,		findvisplanetimesoff,		MAXPROFILETIMES,	nexttimeoff );
		R_RenderGraphTab( "Add Lines",			avgtimeoff,		frametimesoff,		addlinestimesoff,			MAXPROFILETIMES,	nexttimeoff );
		R_RenderGraphTab( "Add Sprites",		avgtimeoff,		frametimesoff,		addspritestimesoff,			MAXPROFILETIMES,	nexttimeoff );
		R_RenderGraphTab( "Everything else",	avgtimeoff,		frametimesoff,		elsetimesoff,				MAXPROFILETIMES,	nexttimeoff );

		igEndTabBar();
	}
}
#endif // RENDER_PERF_GRAPHING

static void R_RenderThreadingOptionsWindow( const char* name, void* data )
{
	bool WorkingBool = I_AtomicLoad( &renderthreadCPUmelter ) != 0;

	igText( "Performance options" );
	igSeparator();
	int32_t oldcount = numusablerendercontexts;
	igSliderInt( "Running threads", &numusablerendercontexts, 1, numrendercontexts, "%d", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput );
	if( numusablerendercontexts != oldcount )
	{
		R_RebalanceContexts();
	}

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

	boolean oldbalance = renderloadbalancing;
	igCheckbox( "Load balancing", (bool*)&renderloadbalancing );
	if( oldbalance != renderloadbalancing )
	{
		R_RebalanceContexts();
	}
	igSliderFloat( "Balancing scale", &rebalancescale, 0.001f, 0.1f, "%0.3f", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput );
	igCheckbox( "SIMD columns", (bool*)&renderSIMDcolumns );
	igNewLine();
	igText( "Debug options" );
	igSeparator();
	igCheckbox( "Visualise split", (bool*)&rendersplitvisualise );
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

    R_InitData ();
    I_TerminalPrintf( Log_None, "." );
    R_InitPointToAngle ();
    I_TerminalPrintf( Log_None, "." );
    R_InitTables ();
    // viewwidth / viewheight / detailLevel are set by the defaults
    I_TerminalPrintf( Log_None, "." );

    R_SetViewSize (screenblocks, detailLevel);
    R_InitPlanes ();
    I_TerminalPrintf( Log_None, "." );
    R_InitLightTables ();
    I_TerminalPrintf( Log_None, "." );
    R_InitSkyMap ();
    R_InitTranslationTables ();
    I_TerminalPrintf( Log_None, "." );

	renderscratchsize = 16 * 1024 * 1024;
	renderscratch = (byte*)Z_Malloc( (int32_t)renderscratchsize, PU_STATIC, nullptr );
	renderscratchpos = 0;

	M_RegisterDashboardRadioButton( "Render|Clear style|None", NULL, &voidcleartype, Void_NoClear );
	M_RegisterDashboardRadioButton( "Render|Clear style|Black", NULL, &voidcleartype, Void_Black );
	M_RegisterDashboardRadioButton( "Render|Clear style|Whacky", NULL, &voidcleartype, Void_Whacky );
	M_RegisterDashboardRadioButton( "Render|Clear style|Sky", NULL, &voidcleartype, Void_Sky );

	M_RegisterDashboardWindow( "Render|Threading|Options", "Render Threading Options", 500, 500, &debugwindow_renderthreadingoptions, Menu_Normal, &R_RenderThreadingOptionsWindow );
#if RENDER_PERF_GRAPHING
	M_RegisterDashboardWindow( "Render|Threading|Graphs", "Render Graphs", 500, 550, &debugwindow_renderthreadinggraphs, Menu_Overlay, &R_RenderThreadingGraphsWindow );
#endif //RENDER_PERF_GRAPHING
	
    framecount = 0;
}

void R_RenderDimensionsChanged( void )
{
	refreshstatusbar = true;
	V_RestoreBuffer();
	R_InitLightTables();
	R_InitPointToAngle();
	R_InitTables();
	// Any other buffers?
	R_SetViewSize( screenblocks, detailLevel );
	R_ExecuteSetViewSize();
	R_RebalanceContexts();
}

//
// R_PointInSubsector
//
subsector_t* BSP_PointInSubsector( fixed_t x, fixed_t y )
{
	node_t*		node;
	uint32_t	nodenum;

	// single subsector is a special case
	if ( !numnodes )
	{
		return subsectors;
	}

	nodenum = numnodes - 1;

	while ( !( nodenum & NF_SUBSECTOR ) )
	{
		node = &nodes[ nodenum ];
		nodenum = node->children[ BSP_PointOnSide( x, y, node ) ];
	}

	return &subsectors[ nodenum & ~NF_SUBSECTOR ];
}

//
// R_SetupFrame
//
double_t Lerp( double_t from, double_t to, double_t percent )
{
	return from + ( to - from ) * percent;
}

extern "C"
{
	extern boolean demoplayback;
}

int32_t R_Responder( event_t* ev ) //__attribute__ ((optnone))
{
	if( ev->type == ev_mouse )
	{
		return ev->data2 * ( mouseSensitivity + 5 ) / 10; 
	}
	return 0;
}

int32_t R_PeekEvents() //__attribute__ ((optnone))
{
	int32_t mouselookx = 0;

	event_t* curr = D_PeekEvent( NULL );
	while( curr != NULL )
	{
		mouselookx += R_Responder( curr );
		curr = D_PeekEvent( curr );
	}

	return mouselookx;
}

void R_SetupFrame( player_t* player, double_t framepercent, boolean isconsoleplayer ) //__attribute__ ((optnone))
{
	M_PROFILE_FUNC();

	R_InitColFuncs();

	int32_t		i;
	int32_t		currcontext;
	int32_t		currstart;
	int32_t		desiredwidth;

	double_t	lastframetime;
	double_t	percentagedebt;

	double_t	idealpercentage = 1.0 / (double_t)numusablerendercontexts;

	renderscratchpos = 0;
	viewplayer = player;
	extralight = player->extralight;

	interpolate_this_frame = enable_frame_interpolation != 0 && !renderpaused;

	if( interpolate_this_frame )
	{
		viewlerp	= framepercent * ( RENDFRACUNIT - 1 );

		rend_fixed_t adjustedviewx = RendFixedLerp( player->mo->prev.x, player->mo->curr.x, viewlerp );
		rend_fixed_t adjustedviewy = RendFixedLerp( player->mo->prev.y, player->mo->curr.y, viewlerp );
		rend_fixed_t adjustedviewz = RendFixedLerp( player->prevviewz, player->currviewz, viewlerp );

		{
			viewx = RendFixedToFixed( adjustedviewx );
			viewy = RendFixedToFixed( adjustedviewy );
			viewz = RendFixedToFixed( adjustedviewz );
			viewangle = player->mo->curr.angle + viewangleoffset;
		}

		if( !demoplayback && isconsoleplayer && player->playerstate != PST_DEAD )
		{
			int64_t mouseamount = R_PeekEvents();
			int64_t newangle = viewangle;
			newangle -= ( ( mouseamount * 0x8 ) << FRACBITS );

			viewangle = (angle_t)( newangle & ANG_MAX );
		}
		else
		{
			int64_t start	= player->mo->prev.angle;
			int64_t end		= player->mo->curr.angle;
			int64_t path	= end - start;
			if( llabs( path ) > ANG180 )
			{
				constexpr int64_t ANG360 = (int64_t)ANG_MAX + 1ll;
				if( end > start )
				{
					start += ANG360;
				}
				else
				{
					end += ANG360;
				}
			}
			rend_fixed_t result = RendFixedLerp( start, end, viewlerp );
			viewangle = (angle_t)( result & ANG_MAX );
		}

		bool selectcurr = ( viewlerp >= ( RENDFRACUNIT >> 1 ) );

		for( int32_t index : iota( 0, numsectors ) )
		{
			rendsectors[ index ].floorheight	= RendFixedLerp( prevsectors[ index ].floorheight, currsectors[ index ].floorheight, viewlerp );
			rendsectors[ index ].ceilheight		= RendFixedLerp( prevsectors[ index ].ceilheight, currsectors[ index ].ceilheight, viewlerp );
			rendsectors[ index ].lightlevel		= selectcurr ? currsectors[ index ].lightlevel : prevsectors[ index ].lightlevel;
			rendsectors[ index ].floortex		= selectcurr ? currsectors[ index ].floortex : prevsectors[ index ].floortex;
			rendsectors[ index ].ceiltex		= selectcurr ? currsectors[ index ].ceiltex : prevsectors[ index ].ceiltex;
		}

		for( int32_t index : iota( 0, numsides ) )
		{
			rendsides[ index ].coloffset		= RendFixedLerp( prevsides[ index ].coloffset, currsides[ index ].coloffset, viewlerp );
			rendsides[ index ].rowoffset		= RendFixedLerp( prevsides[ index ].rowoffset, currsides[ index ].rowoffset, viewlerp );
			rendsides[ index ].toptex			= selectcurr ? currsides[ index ].toptex : prevsides[ index ].toptex;
			rendsides[ index ].midtex			= selectcurr ? currsides[ index ].midtex : prevsides[ index ].midtex;
			rendsides[ index ].bottomtex		= selectcurr ? currsides[ index ].bottomtex : prevsides[ index ].bottomtex;
		}
	}
	else
	{
		viewx = player->mo->x;
		viewy = player->mo->y;
		viewz = player->viewz;
		viewangle = player->mo->angle + viewangleoffset;

		memcpy( rendsectors, currsectors, sizeof( sectorinstance_t ) * numsectors );
		memcpy( rendsides, currsides, sizeof( sideinstance_t ) * numsides );
	}


	viewsin = renderfinesine[ viewangle >> RENDERANGLETOFINESHIFT ];
	viewcos = renderfinecosine[ viewangle >> RENDERANGLETOFINESHIFT ];
	
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

	for( currcontext = 0; currcontext < numusablerendercontexts; ++currcontext )
	{
		vbuffer_t buffer = *I_GetRenderBuffer( 0 );
		renderdatas[ currcontext ].context.buffer = buffer;

		buffer.data += buffer.pitch * viewwindowx + viewwindowy;
		buffer.width = viewheight;
		buffer.height = viewwidth;

		renderdatas[ currcontext ].context.viewbuffer = buffer;
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

		double_t average = lastframetime / numusablerendercontexts;
		double_t timepercentages[ 16 ] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		double_t oldwidthpercentages[ 16 ] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		double_t growth[ 16 ] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		double_t widthpercentages[ 16 ] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

		double_t total = 0;

		int32_t overbudgetcount = 0;
		int32_t underbudgetcount = 0;

		for( currcontext = 0; currcontext < numusablerendercontexts; ++currcontext )
		{
			timepercentages[ currcontext ] = renderdatas[ currcontext ].context.timetaken / lastframetime;
			if( timepercentages[ currcontext ] > idealpercentage ) ++overbudgetcount;
			else ++underbudgetcount;
		}

		double_t totalshuffle = idealpercentage * (double_t)rebalancescale * numusablerendercontexts;
		double_t removeamount = ( totalshuffle / ( numusablerendercontexts - 2 ) ) * ( numusablerendercontexts - 3 );
		double_t addamount = totalshuffle - removeamount;

		removeamount /= overbudgetcount;
		addamount /= underbudgetcount;

		for( currcontext = 0; currcontext < numusablerendercontexts; ++currcontext )
		{
			oldwidthpercentages[ currcontext ] = (double_t)( renderdatas[ currcontext ].context.endcolumn - renderdatas[ currcontext ].context.begincolumn ) / viewwidth;

			double_t growamount = timepercentages[ currcontext ] > idealpercentage	? -removeamount
																					: addamount;
			growth[ currcontext ] = growamount;
			widthpercentages[ currcontext ] = oldwidthpercentages[ currcontext ] + growamount;

			total += widthpercentages[ currcontext ];
		}

		for( currcontext = 0; currcontext < numusablerendercontexts - 1; ++currcontext )
		{
			desiredwidth = (int32_t)( viewwidth * widthpercentages[ currcontext ] );

			R_ResetContext( &renderdatas[ currcontext ].context, M_MAX( currstart, 0 ), M_MIN( currstart + desiredwidth, viewwidth ) );
			currstart += desiredwidth;
		}

		R_ResetContext( &renderdatas[ numusablerendercontexts - 1 ].context, M_MAX( currstart, 0 ), viewwidth );
	}

	if( !renderloadbalancing || renderrebalancecontexts )
	{
		currstart = 0;

		desiredwidth = viewwidth / numusablerendercontexts;
		for( currcontext = 0; currcontext < numusablerendercontexts; ++currcontext )
		{
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

void R_RenderPlayerView(player_t* player, double_t framepercent, boolean isconsoleplayer)
{
	M_PROFILE_FUNC();

	int32_t currcontext;
	uint64_t looptimestart;
	uint64_t looptimeend;

	byte* outputcolumn;
	byte* endcolumn;

	R_SetupFrame(player, framepercent, isconsoleplayer);

	// NetUpdate can cause lump loads, so we wait until rendering is done before doing it again.
	// This is a change from the vanilla renderer.
	NetUpdate ();

	wadrenderlock = true;

	if( rendersplitvisualise ) looptimestart = I_GetTimeUS();

	atomicval_t finishedcontexts = 0;

	for( currcontext = 0; currcontext < numusablerendercontexts; ++currcontext )
	{
		if( renderthreaded && currcontext < numusablerendercontexts - 1 )
		{
			I_AtomicExchange( &renderdatas[ currcontext ].framewaiting, 1 );
			I_SemaphoreRelease( renderdatas[ currcontext ].shouldrun );
		}
		else
		{
			R_RenderViewContext( &renderdatas[ currcontext ].context );
			++finishedcontexts;
		}
	}

	{
		M_PROFILE_NAMED( "Wait on threads" );

		while( renderthreaded && finishedcontexts != numusablerendercontexts )
		{
			for( currcontext = 0; currcontext < numusablerendercontexts; ++currcontext )
			{
				finishedcontexts += I_AtomicExchange( &renderdatas[ currcontext ].framefinished, 0 );
			}
			//I_Sleep( 0 );
		}
	}

	if( rendersplitvisualise )
	{
		looptimeend = I_GetTimeUS();

		for( currcontext = 1; currcontext < numusablerendercontexts; ++currcontext )
		{
			outputcolumn = renderdatas[ currcontext ].context.viewbuffer.data + renderdatas[ currcontext ].context.begincolumn * renderdatas[ currcontext ].context.viewbuffer.pitch;
			endcolumn = outputcolumn + viewheight;

			while( outputcolumn != endcolumn )
			{
				*outputcolumn = 249;
				++outputcolumn;
			}
		}
	}

	wadrenderlock = false;
}
