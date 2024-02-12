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

#include "d_player.h"


DOOM_C_API extern int32_t		validcount;


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

DOOM_C_API extern int32_t			extralight;
DOOM_C_API extern lighttable_t*		fixedcolormap;
DOOM_C_API extern int32_t			fixedcolormapindex;

// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.
#define NUMLIGHTCOLORMAPS		32
#define INVULCOLORMAPINDEX		32
#define UNKNOWNCOLORMAPINDEX	33
#define NUMCOLORMAPS			34


DOOM_C_API extern int32_t	fuzz_style;

//
// Function pointers to switch refresh/drawing functions.
// Used to select shadow mode etc.
//
DOOM_C_API extern colfunc_t transcolfunc;
DOOM_C_API extern colfunc_t fuzzfuncs[ Fuzz_Count ];

typedef enum lineside_e
{
	LS_Front,
	LS_Back,
} lineside_t;

// Utility functions.
//
// Any function starting with BSP_ is used by the playsim.
// R_ functions are used by the renderer and operate in higher precision.

DOOM_C_API lineside_t BSP_PointOnSide( fixed_t x, fixed_t y, node_t* node );
DOOM_C_API lineside_t R_PointOnSide( rend_fixed_t x, rend_fixed_t y, node_t* node );

DOOM_C_API doombool R_PointOnSegSide( rend_fixed_t x, rend_fixed_t y, seg_t* line );

DOOM_C_API angle_t R_PointToAngle( const viewpoint_t* viewpoint, rend_fixed_t fixed_x, rend_fixed_t fixed_y );
DOOM_C_API angle_t BSP_PointToAngle( fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2 );

DOOM_C_API rend_fixed_t R_PointToDist( const viewpoint_t* viewpoint, rend_fixed_t x, rend_fixed_t y );

DOOM_C_API rend_fixed_t R_ScaleFromGlobalAngle( angle_t visangle, rend_fixed_t distance, angle_t view_angle, angle_t normal_angle );

DOOM_C_API subsector_t* BSP_PointInSubsector( fixed_t x, fixed_t y );

DOOM_C_API void R_AddPointToBox( int x, int y, fixed_t* box );


DOOM_C_API void R_BindRenderVariables( void );

//
// REFRESH - the actual rendering functions.
//

// Called by G_Drawer.
DOOM_C_API void R_RenderPlayerView (player_t *player, double_t framepercent, doombool isconsoleplayer);

// Called by startup code.
DOOM_C_API void R_Init (void);

// Called when render_width and render_height change
DOOM_C_API void R_RenderDimensionsChanged( void );

// Called every frame for dynamic resolution scaling
DOOM_C_API void R_RenderUpdateFrameSize( void );

// Called when +/- viewport size is required. subframe is used by dynamic resolution scaling
DOOM_C_API void R_ExecuteSetViewSize( void );

// Called after display system initialised
DOOM_C_API void R_InitContexts( void );

// Called after a new map load
DOOM_C_API void R_RefreshContexts( void );

// Teleport? Spawn? Let's assume render context balances are out of whack
DOOM_C_API void R_RebalanceContexts( void );

// Called by M_Responder.
DOOM_C_API void R_SetViewSize (int blocks, int detail);

#if defined( __cplusplus )

#include "m_container.h"
#include "i_thread.h"

template< typename _ty, typename... _args >
INLINE _ty* R_AllocateScratchSingle( const _args&... args )
{
	// TODO: per-thread scratchpad???
	extern std::atomic< atomicval_t >	renderscratchpos;
	extern byte*						renderscratch;

	constexpr atomicval_t numbytes = AlignTo< 16 >( sizeof( _ty ) );
	atomicval_t pos = renderscratchpos.fetch_add( numbytes );

	_ty* output = (_ty*)( renderscratch + pos );

	if constexpr( sizeof...( _args ) == 1 )
	{
		*output = std::get< 0 >( std::forward_as_tuple( args... ) );
	}
	else if constexpr( sizeof...( _args ) > 1 )
	{
		static_assert( "R_AllocateScratch only allows 1 variadic arg for now, will expand later if I want to construct objects with parameter packs." );
	}

	return output;
}

template< typename _ty, typename... _args >
INLINE _ty* R_AllocateScratch( atomicval_t numinstances, const _args&... args )
{
	// TODO: per-thread scratchpad???
	extern std::atomic< atomicval_t >	renderscratchpos;
	extern std::atomic< atomicval_t >	renderscratchsize;
	extern byte*						renderscratch;

	constexpr atomicval_t sizeofsingleobj = AlignTo< 16 >( sizeof( _ty ) );

	atomicval_t numbytes = sizeofsingleobj * numinstances;
	atomicval_t pos = renderscratchpos.fetch_add( numbytes );
	if( pos + numbytes > renderscratchsize.load() )
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
