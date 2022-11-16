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

#include "r_data.h"
#include "i_system.h"

#if defined( __cplusplus )
extern "C" {
#endif // __cplusplus

#include "d_player.h"


//
// POV related.
//
extern fixed_t		viewcos;
extern fixed_t		viewsin;


extern int32_t		validcount;


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
#define LIGHTSCALEMUL		( drs_current->frame_adjusted_light_mul )
#define MAXLIGHTZ			128
#define LIGHTZSHIFT			20
#define RENDLIGHTZSHIFT		( RENDFRACBITS + 4 )

extern int32_t			extralight;
extern lighttable_t*	fixedcolormap;
extern int32_t			fixedcolormapindex;

// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.
#define NUMLIGHTCOLORMAPS		32
#define INVULCOLORMAPINDEX		32
#define UNKNOWNCOLORMAPINDEX	33
#define NUMCOLORMAPS			34


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

typedef enum lineside_e
{
	LS_Front,
	LS_Back,
} lineside_t;

// Utility functions.
//
// Any function starting with BSP_ is used by the playsim.
// R_ functions are used by the renderer and operate in higher precision.

lineside_t BSP_PointOnSide( fixed_t x, fixed_t y, node_t* node );
lineside_t R_PointOnSide( rend_fixed_t x, rend_fixed_t y, node_t* node );

int
R_PointOnSegSide
( fixed_t	x,
  fixed_t	y,
  seg_t*	line );

angle_t R_PointToAngle( rend_fixed_t fixed_x, rend_fixed_t fixed_y );
angle_t BSP_PointToAngle( fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2 );

rend_fixed_t R_PointToDist( rend_fixed_t x, rend_fixed_t y );

rend_fixed_t R_ScaleFromGlobalAngle( angle_t visangle, rend_fixed_t distance, angle_t view_angle, angle_t normal_angle );

subsector_t* BSP_PointInSubsector( fixed_t x, fixed_t y );

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
void R_RenderPlayerView (player_t *player, double_t framepercent, boolean isconsoleplayer);

// Called by startup code.
void R_Init (void);

// Called when render_width and render_height change
void R_RenderDimensionsChanged( void );

// Called every frame for dynamic resolution scaling
void R_RenderUpdateFrameSize( void );

// Called when +/- viewport size is required. subframe is used by dynamic resolution scaling
void R_ExecuteSetViewSize( void );

// Called after display system initialised
void R_InitContexts( void );

// Called after a new map load
void R_RefreshContexts( void );

// Teleport? Spawn? Let's assume render context balances are out of whack
void R_RebalanceContexts( void );

// Called by M_Responder.
void R_SetViewSize (int blocks, int detail);

#if defined( __cplusplus )
}
#endif // __cplusplus

#if defined( __cplusplus )

#include "m_container.h"
#include "i_thread.h"

template< typename _ty, typename... _args >
INLINE _ty* R_AllocateScratch( atomicval_t numinstances, const _args&... args )
{
	// TODO: per-thread scratchpad???
	extern atomicval_t	renderscratchpos;
	extern atomicval_t	renderscratchsize;
	extern byte*		renderscratch;

	atomicval_t numbytes = AlignTo< 16 >( sizeof( _ty ) * numinstances );
	atomicval_t pos = I_AtomicIncrement( &renderscratchpos, numbytes );
	if( pos + numbytes > renderscratchsize )
	{
		I_Error( "R_AllocateScratch: No more scratchpad memory available" );
	}

	_ty* output = (_ty*)( renderscratch + pos );

	if constexpr( sizeof...( _args ) == 1 )
	{
		for( auto& curr : std::span( output, numinstances ) )
		{
			curr = std::get< 0 >( std::forward_as_tuple( args... ) );
		}
	}
	else if constexpr( sizeof...( _args ) > 1 )
	{
		static_assert( "R_AllocateScratch only allows 1 variadic arg for now, will expand later if I want to construct objects with parameter packs." );
	}

	return output;
}

#endif // defined( __cplusplus )

#endif
