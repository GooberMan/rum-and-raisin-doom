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
//	System specific interface stuff.
//


#ifndef __R_MAIN__
#define __R_MAIN__

#include "d_player.h"
#include "r_data.h"




//
// POV related.
//
extern fixed_t		viewcos;
extern fixed_t		viewsin;

extern int32_t		viewwindowx;
extern int32_t		viewwindowy;

extern int32_t		centerx;
extern int32_t		centery;

extern fixed_t		centerxfrac;
extern fixed_t		centeryfrac;
extern fixed_t		projection;

extern int32_t		validcount;

extern int32_t		aspect_adjusted_render_width;
extern fixed_t		aspect_adjusted_scaled_divide;
extern fixed_t		aspect_adjusted_scaled_mul;


//
// Lighting LUT.
// Used for z-depth cuing per column/row,
//  and other lighting effects (sector ambient, flash).
//

// Lighting constants.
// Now why not 32 levels here?
#define LIGHTLEVELS			16
#define LIGHTSEGSHIFT		4

#define MAXLIGHTSCALE		48
#define LIGHTSCALESHIFT		12
#define RENDLIGHTSCALESHIFT	( RENDFRACBITS - 4 )
#define LIGHTSCALEDIVIDE	aspect_adjusted_scaled_divide
#define LIGHTSCALEMUL		aspect_adjusted_scaled_mul
#define MAXLIGHTZ			128
#define LIGHTZSHIFT			20
#define RENDLIGHTZSHIFT		( RENDFRACBITS + 4 )

extern lighttable_t*	scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
extern int32_t			scalelightindex[LIGHTLEVELS][MAXLIGHTSCALE];
extern lighttable_t*	scalelightfixed[MAXLIGHTSCALE];
extern lighttable_t*	zlight[LIGHTLEVELS][MAXLIGHTZ];
extern int32_t			zlightindex[LIGHTLEVELS][MAXLIGHTZ];

extern int32_t			extralight;
extern lighttable_t*	fixedcolormap;
extern int32_t			fixedcolormapindex;

// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.
#define NUMLIGHTCOLORMAPS		32
#define INVULCOLORMAPINDEX		32
#define UNKNOWNCOLORMAPINDEX	33
#define NUMCOLORMAPS			34


// Blocky/low detail mode.
//B remove this?
//  0 = high, 1 = low
extern int32_t	detailshift;
extern int32_t	fuzz_style;


//
// Function pointers to switch refresh/drawing functions.
// Used to select shadow mode etc.
//
#define COLFUNC_PIXELEXPANDERS 16
#define COLFUNC_FUZZBASEINDEX 16
#define COLFUNC_TRANSLATEINDEX ( COLFUNC_FUZZBASEINDEX + Fuzz_Count )
#define COLFUNC_NUM ( COLFUNC_TRANSLATEINDEX + 1 )
#define COLFUNC_COUNT ( COLFUNC_NUM * 2 )

extern colfunc_t colfuncs[ COLFUNC_COUNT ];

extern colfunc_t transcolfunc;

// No shadow effects on floors.
extern spanfunc_t spanfunc;


//
// Utility functions.
int
R_PointOnSide
( fixed_t	x,
  fixed_t	y,
  node_t*	node );

int
R_PointOnSegSide
( fixed_t	x,
  fixed_t	y,
  seg_t*	line );

angle_t
R_PointToAngle
( fixed_t	x,
  fixed_t	y );

angle_t
R_PointToAngle2
( fixed_t	x1,
  fixed_t	y1,
  fixed_t	x2,
  fixed_t	y2 );

fixed_t
R_PointToDist
( fixed_t	x,
  fixed_t	y );


fixed_t R_ScaleFromGlobalAngle (angle_t visangle, fixed_t distance, fixed_t view_angle, fixed_t normal_angle);

subsector_t*
R_PointInSubsector
( fixed_t	x,
  fixed_t	y );

void
R_AddPointToBox
( int		x,
  int		y,
  fixed_t*	box );


void R_BindRenderVariables( void );

//
// REFRESH - the actual rendering functions.
//

// Called by G_Drawer.
void R_RenderPlayerView (player_t *player);

// Called by startup code.
void R_Init (void);

// Called when render_width and render_height change
void R_RenderDimensionsChanged( void );

// Called after display system initialised
void R_InitContexts( void );

// Called after a new map load
void R_RefreshContexts( void );

// Teleport? Spawn? Let's assume render context balances are out of whack
void R_RebalanceContexts( void );

// Called by M_Responder.
void R_SetViewSize (int blocks, int detail);

#endif
