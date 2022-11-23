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
//
// DESCRIPTION:  the automap code
//

#include <stdio.h>

#include "deh_main.h"

#include "d_gameflow.h"

#include "z_zone.h"
#include "doomkeys.h"
#include "doomdef.h"
#include "st_stuff.h"
#include "p_local.h"
#include "w_wad.h"

#include "m_cheat.h"
#include "m_controls.h"
#include "m_misc.h"
#include "m_config.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"

// Needs access to LFB.
#include "v_video.h"

// State.
#include "doomstat.h"
#include "r_state.h"

// Data.
#include "dstrings.h"

#include "am_map.h"

// For use if I do walls with outsides/insides
#define REDS		(256-5*16)
#define REDRANGE	16
#define BLUES		(256-4*16+8)
#define BLUERANGE	8
#define GREENS		(7*16)
#define GREENRANGE	16
#define GRAYS		(6*16)
#define GRAYSRANGE	16
#define BROWNS		(4*16)
#define BROWNRANGE	16
#define YELLOWS		(256-32+7)
#define YELLOWRANGE	1
#define BLACK		0
#define WHITE		(256-47)

// Automap colors
#define BACKGROUND	BLACK
#define YOURCOLORS	WHITE
#define YOURRANGE	0
#define WALLCOLORS	REDS
#define WALLRANGE	REDRANGE
#define TSWALLCOLORS	GRAYS
#define TSWALLRANGE	GRAYSRANGE
#define FDWALLCOLORS	BROWNS
#define FDWALLRANGE	BROWNRANGE
#define CDWALLCOLORS	YELLOWS
#define CDWALLRANGE	YELLOWRANGE
#define THINGCOLORS	GREENS
#define THINGRANGE	GREENRANGE
#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE WALLRANGE
#define GRIDCOLORS	(GRAYS + GRAYSRANGE/2)
#define GRIDRANGE	0
#define XHAIRCOLORS	GRAYS

typedef enum mapfill_e
{
	MapFill_None,
	MapFill_FloorTex,
	MapFill_CeilTex,
} mapfill_t;

typedef enum mapmode_e
{
	MapMode_Off,
	MapMode_Overlay,
	MapMode_Exclusive,
} mapmode_t;

int32_t	map_style					= MapStyle_Original;
int32_t	map_fill					= MapFill_None;

mapstyledata_t	map_styledatacustomdefault =
{
	BACKGROUND,						0, // background
	GRIDCOLORS,						0, // grid
	GRAYS + 3,						0, // areamap
	WALLCOLORS,						0, // walls
	WALLCOLORS + WALLRANGE / 2,		0, // teleporters
	SECRETWALLCOLORS,				0, // linesecrets
	251,							0, // sectorsecrets
	251,							0, // sectorsecretsundiscovered
	FDWALLCOLORS,					0, // floorchange
	CDWALLCOLORS,					0, // ceilingchange
	TSWALLCOLORS,					0, // nochange

	THINGCOLORS,					0, // things
	214,							0, // monsters_alive
	151,							0, // monsters_dead
	196,							0, // items_counted
	202,							0, // items_uncounted
	173,							0, // projectiles
	110,							0, // puffs

	WHITE,							0, // playerarrow
	XHAIRCOLORS,					0, // crosshair
};

mapstyledata_t	map_styledata[ MapStyle_Max ] =
{
	// MapStyle_Custom
	{
		BACKGROUND,						0, // background
		GRIDCOLORS,						0, // grid
		GRAYS + 3,						0, // areamap
		WALLCOLORS,						0, // walls
		WALLCOLORS + WALLRANGE / 2,		0, // teleporters
		SECRETWALLCOLORS,				0, // linesecrets
		251,							0, // sectorsecrets
		251,							0, // sectorsecretsundiscovered
		FDWALLCOLORS,					0, // floorchange
		CDWALLCOLORS,					0, // ceilingchange
		TSWALLCOLORS,					0, // nochange

		THINGCOLORS,					0, // things
		214,							0, // monsters_alive
		151,							0, // monsters_dead
		196,							0, // items_counted
		202,							0, // items_uncounted
		173,							0, // projectiles
		110,							0, // puffs

		WHITE,							0, // playerarrow
		XHAIRCOLORS,					0, // crosshair
	},

	// MapStyle_Original
	{
		BACKGROUND,						0, // background
		GRIDCOLORS,						0, // grid
		GRAYS + 3,						0, // areamap
		WALLCOLORS,						0, // walls
		WALLCOLORS + WALLRANGE / 2,		0, // teleporters
		SECRETWALLCOLORS,				0, // linesecrets
		-1,								0, // sectorsecrets
		-1,								0, // sectorsecretsundiscovered
		FDWALLCOLORS,					0, // floorchange
		CDWALLCOLORS,					0, // ceilingchange
		TSWALLCOLORS,					0, // nochange

		THINGCOLORS,					0, // things
		THINGCOLORS,					0, // monsters_alive
		THINGCOLORS,					0, // monsters_dead
		THINGCOLORS,					0, // items_counted
		THINGCOLORS,					0, // items_uncounted
		THINGCOLORS,					0, // projectiles
		THINGCOLORS,					0, // puffs

		WHITE,							0, // playerarrow
		XHAIRCOLORS,					0, // crosshair
	},

	// MapStyle_ZDoom
	{
		139,							0, // background
		GRIDCOLORS,						0, // grid
		GRAYS + 3,						0, // areamap
		79,								0, // walls
		200,							0, // teleporters
		SECRETWALLCOLORS,				0, // linesecrets
		-1,								0, // sectorsecrets
		-1,								0, // sectorsecretsundiscovered
		FDWALLCOLORS,					0, // floorchange
		CDWALLCOLORS,					0, // ceilingchange
		TSWALLCOLORS,					0, // nochange

		THINGCOLORS,					0, // things
		THINGCOLORS,					0, // monsters_alive
		THINGCOLORS,					0, // monsters_dead
		THINGCOLORS,					0, // items_counted
		THINGCOLORS,					0, // items_uncounted
		THINGCOLORS,					0, // projectiles
		THINGCOLORS,					0, // puffs

		WHITE,							0, // playerarrow
		XHAIRCOLORS,					0, // crosshair
	},
};

// drawing stuff

#define AM_NUMMARKPOINTS 10

// scale on entry
#define INITSCALEMTOF (.2*RENDFRACUNIT)
// how much the automap moves window per tic in frame-buffer coordinates
// moves 140 pixels in 1 second
#define F_PANINC	( 4 * f_w / 320 )
// how much zoom-in per tic
// goes to 2x in 1 second
#define M_ZOOMIN        ((int) (1.02*RENDFRACUNIT))
// how much zoom-out per tic
// pulls out to 0.5x in 1 second
#define M_ZOOMOUT       ((int) (RENDFRACUNIT/1.02))

// translates between frame-buffer and map distances
#define FTOM(x) RendFixedMul( IntToRendFixed(x), scale_ftom )
#define MTOF(x) RendFixedToInt( RendFixedMul( (x) ,scale_mtof ) )
// translates between frame-buffer and map coordinates
#define CXMTOF(x)  (f_x + MTOF((x)-m_x))
#define CYMTOF(y)  (f_y + (f_h - MTOF((y)-m_y))) 

typedef struct
{
    int x, y;
} fpoint_t;

typedef struct
{
    fpoint_t a, b;
} fline_t;

typedef struct
{
    rend_fixed_t		x,y;
} mpoint_t;

typedef struct
{
    mpoint_t a, b;
} mline_t;

typedef struct
{
    rend_fixed_t slp, islp;
} islope_t;



//
// The vector graphics for the automap.
//  A line drawing of the player pointing right,
//   starting from the middle.
//
#define R ((8*RENDPLAYERRADIUS)/7)
mline_t player_arrow[] = {
    { { -R+R/8, 0 }, { R, 0 } }, // -----
    { { R, 0 }, { R-R/2, R/4 } },  // ----->
    { { R, 0 }, { R-R/2, -R/4 } },
    { { -R+R/8, 0 }, { -R-R/8, R/4 } }, // >---->
    { { -R+R/8, 0 }, { -R-R/8, -R/4 } },
    { { -R+3*R/8, 0 }, { -R+R/8, R/4 } }, // >>--->
    { { -R+3*R/8, 0 }, { -R+R/8, -R/4 } }
};
#undef R

#define R ((8*RENDPLAYERRADIUS)/7)
mline_t cheat_player_arrow[] = {
    { { -R+R/8, 0 }, { R, 0 } }, // -----
    { { R, 0 }, { R-R/2, R/6 } },  // ----->
    { { R, 0 }, { R-R/2, -R/6 } },
    { { -R+R/8, 0 }, { -R-R/8, R/6 } }, // >----->
    { { -R+R/8, 0 }, { -R-R/8, -R/6 } },
    { { -R+3*R/8, 0 }, { -R+R/8, R/6 } }, // >>----->
    { { -R+3*R/8, 0 }, { -R+R/8, -R/6 } },
    { { -R/2, 0 }, { -R/2, -R/6 } }, // >>-d--->
    { { -R/2, -R/6 }, { -R/2+R/6, -R/6 } },
    { { -R/2+R/6, -R/6 }, { -R/2+R/6, R/4 } },
    { { -R/6, 0 }, { -R/6, -R/6 } }, // >>-dd-->
    { { -R/6, -R/6 }, { 0, -R/6 } },
    { { 0, -R/6 }, { 0, R/4 } },
    { { R/6, R/4 }, { R/6, -R/7 } }, // >>-ddt->
    { { R/6, -R/7 }, { R/6+R/32, -R/7-R/32 } },
    { { R/6+R/32, -R/7-R/32 }, { R/6+R/10, -R/7 } }
};
#undef R

#define R (RENDFRACUNIT)
mline_t triangle_guy[] = {
    { { (rend_fixed_t)(-.867*R), (rend_fixed_t)(-.5*R) }, { (rend_fixed_t)(.867*R ), (rend_fixed_t)(-.5*R) } },
    { { (rend_fixed_t)(.867*R ), (rend_fixed_t)(-.5*R) }, { (rend_fixed_t)(0      ), (rend_fixed_t)(R    ) } },
    { { (rend_fixed_t)(0      ), (rend_fixed_t)(R    ) }, { (rend_fixed_t)(-.867*R), (rend_fixed_t)(-.5*R) } }
};
#undef R

#define R (RENDFRACUNIT)
mline_t thintriangle_guy[] = {
    { { (rend_fixed_t)(-.5*R), (rend_fixed_t)(-.7*R) }, { (rend_fixed_t)(R    ), (rend_fixed_t)(0    ) } },
    { { (rend_fixed_t)(R    ), (rend_fixed_t)(0    ) }, { (rend_fixed_t)(-.5*R), (rend_fixed_t)(.7*R ) } },
    { { (rend_fixed_t)(-.5*R), (rend_fixed_t)(.7*R ) }, { (rend_fixed_t)(-.5*R), (rend_fixed_t)(-.7*R) } }
};
#undef R

static int 	cheating = 0;
static int 	grid = 0;

doombool    	automapactive = false;

// location of window on screen
static int 	f_x;
static int	f_y;

// size of window on screen
//static int 	f_w;
//static int	f_h;

#define f_w ( frame_width )
#define f_h ( frame_height - ST_BUFFERHEIGHT )

static int 	lightlev; 		// used for funky strobing effect
static pixel_t*	fb; 			// pseudo-frame buffer
static int 	amclock;

static mpoint_t m_paninc; // how far the window pans each tic (map coords)
static rend_fixed_t 	mtof_zoommul; // how far the window zooms in each tic (map coords)
static rend_fixed_t 	ftom_zoommul; // how far the window zooms in each tic (fb coords)

static rend_fixed_t 	m_x, m_y;   // LL x,y where the window is on the map (map coords)
static rend_fixed_t 	m_x2, m_y2; // UR x,y where the window is on the map (map coords)

//
// width/height of window on map (map coords)
//
static rend_fixed_t		m_w;
static rend_fixed_t		m_h;

// based on level size
static rend_fixed_t		min_x;
static rend_fixed_t		min_y; 
static rend_fixed_t		max_x;
static rend_fixed_t		max_y;

static rend_fixed_t		max_w; // max_x-min_x,
static rend_fixed_t		max_h; // max_y-min_y

// based on player size
static rend_fixed_t		min_w;
static rend_fixed_t		min_h;


static rend_fixed_t 	min_scale_mtof; // used to tell when to stop zooming out
static rend_fixed_t 	max_scale_mtof; // used to tell when to stop zooming in

// old stuff for recovery later
static rend_fixed_t old_m_w, old_m_h;
static rend_fixed_t old_m_x, old_m_y;

// old location used by the Follower routine
static mpoint_t f_oldloc;

// used by MTOF to scale from map-to-frame-buffer coords
static rend_fixed_t scale_mtof = (rend_fixed_t)INITSCALEMTOF;
// used by FTOM to scale from frame-buffer-to-map coords (=1/scale_mtof)
static rend_fixed_t scale_ftom;

static player_t *plr; // the player represented by an arrow

static patch_t *marknums[10]; // numbers used for marking by the automap
static mpoint_t markpoints[AM_NUMMARKPOINTS]; // where the points are
static int markpointnum = 0; // next point to be assigned

static int followplayer = 1; // specifies whether to follow the player around

cheatseq_t cheat_amap = CHEAT("iddt", 0);

static doombool stopped = true;

static int32_t lightnexttic = 0;
static int32_t lightticlength = 6;
static int32_t lightlevels[] = { 0, 4, 7, 10, 12, 14, 15, 15, 15, 14, 12, 10, 7, 4, 0, 0 };
static int32_t lightlevelscnt = 0;

static int32_t linewidth = 1;
static int32_t lineheight = 1;

void AM_BindAutomapVariables( void )
{
	M_BindIntVariable( "map_style",										&map_style );
	M_BindIntVariable( "map_fill",										&map_fill );
	M_BindIntVariable( "mapcolor_background",							&map_styledata[ MapStyle_Custom ].background.val );
	M_BindIntVariable( "mapcolor_background_flags",						&map_styledata[ MapStyle_Custom ].background.flags );
	M_BindIntVariable( "mapcolor_grid",									&map_styledata[ MapStyle_Custom ].grid.val );
	M_BindIntVariable( "mapcolor_grid_flags",							&map_styledata[ MapStyle_Custom ].grid.flags );
	M_BindIntVariable( "mapcolor_areamap",								&map_styledata[ MapStyle_Custom ].areamap.val );
	M_BindIntVariable( "mapcolor_areamap_flags",						&map_styledata[ MapStyle_Custom ].areamap.flags );
	M_BindIntVariable( "mapcolor_walls",								&map_styledata[ MapStyle_Custom ].walls.val );
	M_BindIntVariable( "mapcolor_walls_flags",							&map_styledata[ MapStyle_Custom ].walls.flags );
	M_BindIntVariable( "mapcolor_teleporters",							&map_styledata[ MapStyle_Custom ].teleporters.val );
	M_BindIntVariable( "mapcolor_teleporters_flags",					&map_styledata[ MapStyle_Custom ].teleporters.flags );
	M_BindIntVariable( "mapcolor_linesecrets",							&map_styledata[ MapStyle_Custom ].linesecrets.val );
	M_BindIntVariable( "mapcolor_linesecrets_flags",					&map_styledata[ MapStyle_Custom ].linesecrets.flags );
	M_BindIntVariable( "mapcolor_sectorsecrets",						&map_styledata[ MapStyle_Custom ].sectorsecrets.val );
	M_BindIntVariable( "mapcolor_sectorsecrets_flags",					&map_styledata[ MapStyle_Custom ].sectorsecrets.flags );
	M_BindIntVariable( "mapcolor_sectorsecretsundiscovered",			&map_styledata[ MapStyle_Custom ].sectorsecretsundiscovered.val );
	M_BindIntVariable( "mapcolor_sectorsecretsundiscovered_flags",		&map_styledata[ MapStyle_Custom ].sectorsecretsundiscovered.flags );
	M_BindIntVariable( "mapcolor_floorchange",							&map_styledata[ MapStyle_Custom ].floorchange.val );
	M_BindIntVariable( "mapcolor_floorchange_flags",					&map_styledata[ MapStyle_Custom ].floorchange.flags );
	M_BindIntVariable( "mapcolor_ceilingchange",						&map_styledata[ MapStyle_Custom ].ceilingchange.val );
	M_BindIntVariable( "mapcolor_ceilingchange_flags",					&map_styledata[ MapStyle_Custom ].ceilingchange.flags );
	M_BindIntVariable( "mapcolor_nochange",								&map_styledata[ MapStyle_Custom ].nochange.val );
	M_BindIntVariable( "mapcolor_nochange_flags",						&map_styledata[ MapStyle_Custom ].nochange.flags );
	M_BindIntVariable( "mapcolor_things",								&map_styledata[ MapStyle_Custom ].things.val );
	M_BindIntVariable( "mapcolor_things_flags",							&map_styledata[ MapStyle_Custom ].things.flags );
	M_BindIntVariable( "mapcolor_monsters_alive",						&map_styledata[ MapStyle_Custom ].monsters_alive.val );
	M_BindIntVariable( "mapcolor_monsters_alive_flags",					&map_styledata[ MapStyle_Custom ].monsters_alive.flags );
	M_BindIntVariable( "mapcolor_monsters_dead",						&map_styledata[ MapStyle_Custom ].monsters_dead.val );
	M_BindIntVariable( "mapcolor_monsters_dead_flags",					&map_styledata[ MapStyle_Custom ].monsters_dead.flags );
	M_BindIntVariable( "mapcolor_items_counted",						&map_styledata[ MapStyle_Custom ].items_counted.val );
	M_BindIntVariable( "mapcolor_items_counted_flags",					&map_styledata[ MapStyle_Custom ].items_counted.flags );
	M_BindIntVariable( "mapcolor_items_uncounted",						&map_styledata[ MapStyle_Custom ].items_uncounted.val );
	M_BindIntVariable( "mapcolor_items_uncounted_flags",				&map_styledata[ MapStyle_Custom ].items_uncounted.flags );
	M_BindIntVariable( "mapcolor_projectiles",							&map_styledata[ MapStyle_Custom ].projectiles.val );
	M_BindIntVariable( "mapcolor_projectiles_flags",					&map_styledata[ MapStyle_Custom ].projectiles.flags );
	M_BindIntVariable( "mapcolor_puffs",								&map_styledata[ MapStyle_Custom ].puffs.val );
	M_BindIntVariable( "mapcolor_puffs_flags",							&map_styledata[ MapStyle_Custom ].puffs.flags );
	M_BindIntVariable( "mapcolor_playerarrow",							&map_styledata[ MapStyle_Custom ].playerarrow.val );
	M_BindIntVariable( "mapcolor_playerarrow_flags",					&map_styledata[ MapStyle_Custom ].playerarrow.flags );
	M_BindIntVariable( "mapcolor_crosshair",							&map_styledata[ MapStyle_Custom ].crosshair.val );
	M_BindIntVariable( "mapcolor_crosshair_flags",						&map_styledata[ MapStyle_Custom ].crosshair.flags );
}

// Calculates the slope and slope according to the x-axis of a line
// segment in map coordinates (with the upright y-axis n' all) so
// that it can be used with the brain-dead drawing stuff.

void
AM_getIslope
( mline_t*	ml,
  islope_t*	is )
{
    rend_fixed_t dx, dy;

    dy = ml->a.y - ml->b.y;
    dx = ml->b.x - ml->a.x;
    if (!dy) is->islp = (dx<0?-LLONG_MAX:LLONG_MAX);
    else is->islp = RendFixedDiv(dx, dy);
    if (!dx) is->slp = (dy<0?-LLONG_MAX:LLONG_MAX);
    else is->slp = RendFixedDiv(dy, dx);

}

//
//
//
void AM_activateNewScale(void)
{
    m_x += m_w/2;
    m_y += m_h/2;
    m_w = FTOM(f_w);
    m_h = FTOM(f_h);
    m_x -= m_w/2;
    m_y -= m_h/2;
    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;
}

//
//
//
void AM_saveScaleAndLoc(void)
{
    old_m_x = m_x;
    old_m_y = m_y;
    old_m_w = m_w;
    old_m_h = m_h;
}

//
//
//
void AM_restoreScaleAndLoc(void)
{

    m_w = old_m_w;
    m_h = old_m_h;
    if (!followplayer)
    {
	m_x = old_m_x;
	m_y = old_m_y;
    } else {
	m_x = FixedToRendFixed( plr->mo->x ) - m_w/2;
	m_y = FixedToRendFixed( plr->mo->y ) - m_h/2;
    }
    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;

    // Change the scaling multipliers
    scale_mtof = RendFixedDiv( IntToRendFixed( f_w ), m_w);
    scale_ftom = RendFixedDiv( RENDFRACUNIT, scale_mtof );
}

//
// adds a marker at the current location
//
#define MARK_BROKEN 1

void AM_addMark(void)
{
#if !MARK_BROKEN
    markpoints[markpointnum].x = m_x + m_w/2;
    markpoints[markpointnum].y = m_y + m_h/2;
    markpointnum = (markpointnum + 1) % AM_NUMMARKPOINTS;
#else // MARK_BROKEN
	players[ consoleplayer ].message = "Markers broken for now. Come back later.";
#endif // !MARK_BROKEN
}

//
// Determines bounding box of all vertices,
// sets global variables controlling zoom range.
//
void AM_findMinMaxBoundaries(void)
{
    int i;
    rend_fixed_t a;
    rend_fixed_t b;

    min_x = min_y =  LLONG_MAX;
    max_x = max_y = -LLONG_MAX;
  
    for (i=0;i<numvertexes;i++)
    {
	if (vertexes[i].rend.x < min_x)
	    min_x = vertexes[i].rend.x;
	else if (vertexes[i].rend.x > max_x)
	    max_x = vertexes[i].rend.x;
    
	if (vertexes[i].rend.y < min_y)
	    min_y = vertexes[i].rend.y;
	else if (vertexes[i].rend.y > max_y)
	    max_y = vertexes[i].rend.y;
    }
  
    max_w = max_x - min_x;
    max_h = max_y - min_y;

    min_w = 2*PLAYERRADIUS; // const? never changed?
    min_h = 2*PLAYERRADIUS;

    a = RendFixedDiv( IntToRendFixed( f_w ), max_w );
    b = RendFixedDiv( IntToRendFixed( f_h ), max_h );
  
    min_scale_mtof = a < b ? a : b;
    max_scale_mtof = RendFixedDiv( IntToRendFixed( f_h ), 2 * RENDPLAYERRADIUS);

}


//
//
//
void AM_changeWindowLoc(void)
{
    if (m_paninc.x || m_paninc.y)
    {
	followplayer = 0;
	f_oldloc.x = LLONG_MAX;
    }

    m_x += m_paninc.x;
    m_y += m_paninc.y;

    if (m_x + m_w/2 > max_x)
	m_x = max_x - m_w/2;
    else if (m_x + m_w/2 < min_x)
	m_x = min_x - m_w/2;
  
    if (m_y + m_h/2 > max_y)
	m_y = max_y - m_h/2;
    else if (m_y + m_h/2 < min_y)
	m_y = min_y - m_h/2;

    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;
}

//
//
//
void AM_initVariables(void)
{
    int pnum;
    static event_t st_notify = { ev_keyup, AM_MSGENTERED, 0, 0 };

    automapactive = true;
    fb = I_VideoBuffer;

    f_oldloc.x = LLONG_MAX;
    amclock = 0;
    lightlev = 0;
	lightnexttic = lightticlength;
	lightlevelscnt = 0;

    m_paninc.x = m_paninc.y = 0;
    ftom_zoommul = RENDFRACUNIT;
    mtof_zoommul = RENDFRACUNIT;

    m_w = FTOM(f_w);
    m_h = FTOM(f_h);

    // find player to center on initially
    if (playeringame[consoleplayer])
    {
        plr = &players[consoleplayer];
    }
    else
    {
        plr = &players[0];

	for (pnum=0;pnum<MAXPLAYERS;pnum++)
        {
	    if (playeringame[pnum])
            {
                plr = &players[pnum];
		break;
            }
        }
    }

    m_x = FixedToRendFixed( plr->mo->x ) - m_w/2;
    m_y = FixedToRendFixed( plr->mo->y ) - m_h/2;
    AM_changeWindowLoc();

    // for saving & restoring
    old_m_x = m_x;
    old_m_y = m_y;
    old_m_w = m_w;
    old_m_h = m_h;

    // inform the status bar of the change
    ST_Responder(&st_notify);

}

//
// 
//
void AM_loadPics(void)
{
    int i;
    char namebuf[9];
  
    for (i=0;i<10;i++)
    {
	DEH_snprintf(namebuf, 9, "AMMNUM%d", i);
	marknums[i] = W_CacheLumpName(namebuf, PU_STATIC);
    }

}

void AM_unloadPics(void)
{
    int i;
    char namebuf[9];
  
    for (i=0;i<10;i++)
    {
	DEH_snprintf(namebuf, 9, "AMMNUM%d", i);
	W_ReleaseLumpName(namebuf);
    }
}

void AM_clearMarks(void)
{
    int i;

    for (i=0;i<AM_NUMMARKPOINTS;i++)
	markpoints[i].x = -1; // means empty
    markpointnum = 0;
}

//
// should be called at the start of every level
// right now, i figure it out myself
//
void AM_LevelInit(void)
{
    f_x = f_y = 0;

    AM_clearMarks();

    AM_findMinMaxBoundaries();
    scale_mtof = RendFixedDiv(min_scale_mtof, (int) (0.7*RENDFRACUNIT));
    if (scale_mtof > max_scale_mtof)
	scale_mtof = min_scale_mtof;
    scale_ftom = RendFixedDiv(RENDFRACUNIT, scale_mtof);
}




//
//
//
void AM_Stop (void)
{
    static event_t st_notify = { 0, ev_keyup, AM_MSGEXITED, 0 };

    AM_unloadPics();
    automapactive = false;
    ST_Responder(&st_notify);
    stopped = true;
}

//
//
//
void AM_Start (void)
{
    static mapinfo_t* lastmap = NULL;

    if (!stopped) AM_Stop();
    stopped = false;
    if (current_map != lastmap)
    {
		AM_LevelInit();
		lastmap = current_map;
    }
    AM_initVariables();
    AM_loadPics();
}

//
// set the window scale to the maximum size
//
void AM_minOutWindowScale(void)
{
    scale_mtof = min_scale_mtof;
    scale_ftom = RendFixedDiv(RENDFRACUNIT, scale_mtof);
    AM_activateNewScale();
}

//
// set the window scale to the minimum size
//
void AM_maxOutWindowScale(void)
{
    scale_mtof = max_scale_mtof;
    scale_ftom = RendFixedDiv(RENDFRACUNIT, scale_mtof);
    AM_activateNewScale();
}


//
// Handle events (user inputs) in automap mode
//
doombool
AM_Responder
( event_t*	ev )
{

    int rc;
    static int bigstate=0;
    static char buffer[20];
    int key;

    rc = false;

    if (ev->type == ev_joystick && joybautomap >= 0
        && (ev->data1 & (1 << joybautomap)) != 0)
    {
        joywait = I_GetTimeTicks() + 5;

        if (!automapactive)
        {
            AM_Start ();
            viewactive = false;
        }
        else
        {
            bigstate = 0;
            viewactive = true;
            AM_Stop ();
        }

        return true;
    }

    if (!automapactive)
    {
	if (ev->type == ev_keydown && ev->data1 == key_map_toggle)
	{
	    AM_Start ();
	    viewactive = false;
	    rc = true;
	}
    }
    else if (ev->type == ev_keydown)
    {
	rc = true;
        key = ev->data1;

        if (key == key_map_east)          // pan right
        {
            if (!followplayer) m_paninc.x = FTOM(F_PANINC);
            else rc = false;
        }
        else if (key == key_map_west)     // pan left
        {
            if (!followplayer) m_paninc.x = -FTOM(F_PANINC);
            else rc = false;
        }
        else if (key == key_map_north)    // pan up
        {
            if (!followplayer) m_paninc.y = FTOM(F_PANINC);
            else rc = false;
        }
        else if (key == key_map_south)    // pan down
        {
            if (!followplayer) m_paninc.y = -FTOM(F_PANINC);
            else rc = false;
        }
        else if (key == key_map_zoomout)  // zoom out
        {
            mtof_zoommul = M_ZOOMOUT;
            ftom_zoommul = M_ZOOMIN;
        }
        else if (key == key_map_zoomin)   // zoom in
        {
            mtof_zoommul = M_ZOOMIN;
            ftom_zoommul = M_ZOOMOUT;
        }
        else if (key == key_map_toggle)
        {
            bigstate = 0;
            viewactive = true;
            AM_Stop ();
        }
        else if (key == key_map_maxzoom)
        {
            bigstate = !bigstate;
            if (bigstate)
            {
                AM_saveScaleAndLoc();
                AM_minOutWindowScale();
            }
            else AM_restoreScaleAndLoc();
        }
        else if (key == key_map_follow)
        {
            followplayer = !followplayer;
            f_oldloc.x = LLONG_MAX;
            if (followplayer)
                plr->message = DEH_String(AMSTR_FOLLOWON);
            else
                plr->message = DEH_String(AMSTR_FOLLOWOFF);
        }
        else if (key == key_map_grid)
        {
            grid = !grid;
            if (grid)
                plr->message = DEH_String(AMSTR_GRIDON);
            else
                plr->message = DEH_String(AMSTR_GRIDOFF);
        }
        else if (key == key_map_mark)
        {
            M_snprintf(buffer, sizeof(buffer), "%s %d",
                       DEH_String(AMSTR_MARKEDSPOT), markpointnum);
            plr->message = buffer;
            AM_addMark();
        }
        else if (key == key_map_clearmark)
        {
            AM_clearMarks();
            plr->message = DEH_String(AMSTR_MARKSCLEARED);
        }
        else
        {
            rc = false;
        }

        if ((!deathmatch || gameversion <= exe_doom_1_8)
         && cht_CheckCheat(&cheat_amap, ev->data2))
        {
            rc = false;
            cheating = (cheating + 1) % 3;
        }
    }
    else if (ev->type == ev_keyup)
    {
        rc = false;
        key = ev->data1;

        if (key == key_map_east)
        {
            if (!followplayer) m_paninc.x = 0;
        }
        else if (key == key_map_west)
        {
            if (!followplayer) m_paninc.x = 0;
        }
        else if (key == key_map_north)
        {
            if (!followplayer) m_paninc.y = 0;
        }
        else if (key == key_map_south)
        {
            if (!followplayer) m_paninc.y = 0;
        }
        else if (key == key_map_zoomout || key == key_map_zoomin)
        {
            mtof_zoommul = RENDFRACUNIT;
            ftom_zoommul = RENDFRACUNIT;
        }
    }

    return rc;

}


//
// Zooming
//
void AM_changeWindowScale(void)
{

    // Change the scaling multipliers
    scale_mtof = RendFixedMul(scale_mtof, mtof_zoommul);
    scale_ftom = RendFixedDiv(RENDFRACUNIT, scale_mtof);

    if (scale_mtof < min_scale_mtof)
	AM_minOutWindowScale();
    else if (scale_mtof > max_scale_mtof)
	AM_maxOutWindowScale();
    else
	AM_activateNewScale();
}


//
//
//
void AM_doFollowPlayer(void)
{

    if (f_oldloc.x != FixedToRendFixed( plr->mo->x ) || f_oldloc.y != FixedToRendFixed( plr->mo->y ) )
    {
	m_x = FTOM( MTOF( FixedToRendFixed( plr->mo->x ) ) ) - m_w/2;
	m_y = FTOM( MTOF( FixedToRendFixed( plr->mo->y ) ) ) - m_h/2;
	m_x2 = m_x + m_w;
	m_y2 = m_y + m_h;
	f_oldloc.x = FixedToRendFixed( plr->mo->x );
	f_oldloc.y = FixedToRendFixed( plr->mo->y );

	//  m_x = FTOM(MTOF(plr->mo->x - m_w/2));
	//  m_y = FTOM(MTOF(plr->mo->y - m_h/2));
	//  m_x = plr->mo->x - m_w/2;
	//  m_y = plr->mo->y - m_h/2;

    }

}

//
//
//
void AM_updateLightLev(void)
{
   
	// Change light level
	if (amclock>lightnexttic)
	{
		lightlev = lightlevels[lightlevelscnt++];
		if (lightlevelscnt == arrlen(lightlevels))
		{
			lightlevelscnt = 0;
		}
		lightnexttic = amclock + lightticlength - (amclock % lightticlength);
	}

}


//
// Updates on Game Tick
//
void AM_Ticker (void)
{

	if (!automapactive)
	{
		return;
	}

	amclock++;

	// HACK HACK HACK HACK HACK HACK HACK HACK HACK HACK
	AM_changeWindowScale();
	AM_findMinMaxBoundaries();

	if (followplayer)
	{
		AM_doFollowPlayer();
	}

	// Change the zoom if necessary
	if (ftom_zoommul != RENDFRACUNIT)
	{
		AM_changeWindowScale();
	}

	// Change x,y location
	if (m_paninc.x || m_paninc.y)
	{
		AM_changeWindowLoc();
	}

	// Update light level
	AM_updateLightLev();

}


//
// Clear automap frame buffer.
//

#define render_pitch render_height
void AM_clearFB(int color)
{
	// TODO: THIS IS BROKEN THIS IS BROKEN THIS IS BROKEN
    memset(fb, color, f_w*render_pitch*sizeof(*fb));
}


//
// Automap clipping of lines.
//
// Based on Cohen-Sutherland clipping algorithm but with a slightly
// faster reject and precalculated slopes.  If the speed is needed,
// use a hash algorithm to handle  the common cases.
//
doombool AM_clipMline( mline_t* ml, fline_t* fl )
{
	enum
	{
		LEFT	= 1,
		RIGHT	= 2,
		BOTTOM	= 4,
		TOP		= 8,
	};
    
    register int	outcode1 = 0;
    register int	outcode2 = 0;
    register int	outside;
    
    fpoint_t	tmp;
    int		dx;
    int		dy;

    
#define DOOUTCODE(oc, mx, my) \
    (oc) = 0; \
    if ((my) < 0) (oc) |= TOP; \
    else if ((my) >= f_h) (oc) |= BOTTOM; \
    if ((mx) < 0) (oc) |= LEFT; \
    else if ((mx) >= f_w) (oc) |= RIGHT;

    
    // do trivial rejects and outcodes
    if (ml->a.y > m_y2)
	outcode1 = TOP;
    else if (ml->a.y < m_y)
	outcode1 = BOTTOM;

    if (ml->b.y > m_y2)
	outcode2 = TOP;
    else if (ml->b.y < m_y)
	outcode2 = BOTTOM;
    
    if (outcode1 & outcode2)
	return false; // trivially outside

    if (ml->a.x < m_x)
	outcode1 |= LEFT;
    else if (ml->a.x > m_x2)
	outcode1 |= RIGHT;
    
    if (ml->b.x < m_x)
	outcode2 |= LEFT;
    else if (ml->b.x > m_x2)
	outcode2 |= RIGHT;
    
    if (outcode1 & outcode2)
	return false; // trivially outside

    // transform to frame-buffer coordinates.
    fl->a.x = CXMTOF(ml->a.x);
    fl->a.y = CYMTOF(ml->a.y);
    fl->b.x = CXMTOF(ml->b.x);
    fl->b.y = CYMTOF(ml->b.y);

    DOOUTCODE(outcode1, fl->a.x, fl->a.y);
    DOOUTCODE(outcode2, fl->b.x, fl->b.y);

    if (outcode1 & outcode2)
	return false;

    while (outcode1 | outcode2)
    {
	// may be partially inside box
	// find an outside point
	if (outcode1)
	    outside = outcode1;
	else
	    outside = outcode2;
	
	// clip to each side
	if (outside & TOP)
	{
	    dy = fl->a.y - fl->b.y;
	    dx = fl->b.x - fl->a.x;
	    tmp.x = fl->a.x + (dx*(fl->a.y))/dy;
	    tmp.y = 0;
	}
	else if (outside & BOTTOM)
	{
	    dy = fl->a.y - fl->b.y;
	    dx = fl->b.x - fl->a.x;
	    tmp.x = fl->a.x + (dx*(fl->a.y-f_h))/dy;
	    tmp.y = f_h-1;
	}
	else if (outside & RIGHT)
	{
	    dy = fl->b.y - fl->a.y;
	    dx = fl->b.x - fl->a.x;
	    tmp.y = fl->a.y + (dy*(f_w-1 - fl->a.x))/dx;
	    tmp.x = f_w-1;
	}
	else if (outside & LEFT)
	{
	    dy = fl->b.y - fl->a.y;
	    dx = fl->b.x - fl->a.x;
	    tmp.y = fl->a.y + (dy*(-fl->a.x))/dx;
	    tmp.x = 0;
	}
        else
        {
            tmp.x = 0;
            tmp.y = 0;
        }

	if (outside == outcode1)
	{
	    fl->a = tmp;
	    DOOUTCODE(outcode1, fl->a.x, fl->a.y);
	}
	else
	{
	    fl->b = tmp;
	    DOOUTCODE(outcode2, fl->b.x, fl->b.y);
	}
	
	if (outcode1 & outcode2)
	    return false; // trivially outside
    }

    return true;
}
#undef DOOUTCODE


//
// Classic Bresenham w/ whatever optimizations needed for speed
//
void
AM_drawFline
( fline_t*	fl,
  int		color )
{
	register int x;
	register int y;
	register int dx;
	register int dy;
	register int sx;
	register int sy;
	register int ax;
	register int ay;
	register int d;
    
	static int fuck = 0;

	// For debugging only
	if ( fl->a.x < 0 || fl->a.x >= f_w
		|| fl->a.y < 0 || fl->a.y >= f_h
		|| fl->b.x < 0 || fl->b.x >= f_w
		|| fl->b.y < 0 || fl->b.y >= f_h)
	{
		DEH_fprintf(stderr, "fuck %d \r", fuck++);
		return;
	}

	#define PUTDOT(xx,yy,cc) fb[(xx)*render_pitch+(yy)]=(cc)

	dx = fl->b.x - fl->a.x;
	ax = 2 * (dx<0 ? -dx : dx);
	sx = dx<0 ? -1 : 1;

	dy = fl->b.y - fl->a.y;
	ay = 2 * (dy<0 ? -dy : dy);
	sy = dy<0 ? -1 : 1;

	x = fl->a.x;
	y = fl->a.y;

	if (ax > ay)
	{
		d = ay - ax/2;
		while (1)
		{
			PUTDOT(x,y,color);
			if (x == fl->b.x) return;
			if (d>=0)
			{
				y += sy;
				d -= ax;
			}
			x += sx;
			d += ay;
		}
	}
	else
	{
		d = ax - ay/2;
		while (1)
		{
			PUTDOT(x, y, color);
			if (y == fl->b.y) return;
			if (d >= 0)
			{
			x += sx;
			d -= ay;
			}
			y += sy;
			d += ax;
		}
	}
}


//
// Clip lines, draw visible part sof lines.
//
void AM_drawMline( mline_t* ml, int color )
{
	static fline_t fl;

	int32_t ax = 0;
	int32_t ay = 0;
	int32_t bx = 0;
	int32_t by = 0;
	int32_t loopx = 0;
	int32_t loopy = 0;

	if (AM_clipMline(ml, &fl))
	{
		ax = fl.a.x;
		ay = fl.a.y;
		bx = fl.b.x;
		by = fl.b.y;
		for( loopx = 0; loopx < linewidth; ++loopx )
		{
			// Need to clamp to frame buffer thanks to line thickness
			fl.a.x = M_CLAMP( ax + loopx - ( linewidth >> 1 ), 0, f_w - 1 );
			fl.b.x = M_CLAMP( bx + loopx - ( linewidth >> 1 ), 0, f_w - 1 );
			for( loopy = 0; loopy < lineheight; ++loopy )
			{
				fl.a.y = M_CLAMP( ay + loopy - ( lineheight >> 1 ), 0, f_h - 1 );
				fl.b.y = M_CLAMP( by + loopy - ( lineheight >> 1 ), 0, f_h - 1 );
				AM_drawFline(&fl, color); // draws it on frame buffer using fb coords
			}
		}
	}
}



//
// Draws flat (floor/ceiling tile) aligned grid lines.
//
void AM_drawGrid(int color)
{
	rend_fixed_t x, y;
	rend_fixed_t start, end;
	mline_t ml;

	// Figure out start of vertical gridlines
	start = m_x;
	if ( ( start - FixedToRendFixed( bmaporgx ) ) % ( IntToRendFixed( MAPBLOCKUNITS ) ) )
		start += ( IntToRendFixed( MAPBLOCKUNITS ) )
			- ( (start- FixedToRendFixed( bmaporgx ) ) % ( IntToRendFixed( MAPBLOCKUNITS ) ) );
	end = m_x + m_w;

	// draw vertical gridlines
	ml.a.y = m_y;
	ml.b.y = m_y+m_h;
	for (x=start; x<end; x+=( IntToRendFixed( MAPBLOCKUNITS ) ) )
	{
		ml.a.x = x;
		ml.b.x = x;
		AM_drawMline(&ml, color);
	}

	// Figure out start of horizontal gridlines
	start = m_y;
	if ( ( start - FixedToRendFixed( bmaporgy ) ) % ( IntToRendFixed( MAPBLOCKUNITS ) ) )
		start += ( IntToRendFixed( MAPBLOCKUNITS ) )
			- ( ( start - FixedToRendFixed( bmaporgy ) ) % ( IntToRendFixed( MAPBLOCKUNITS ) ) );
	end = m_y + m_h;

	// draw horizontal gridlines
	ml.a.x = m_x;
	ml.b.x = m_x + m_w;
	for (y=start; y<end; y+=( IntToRendFixed( MAPBLOCKUNITS ) ) )
	{
		ml.a.y = y;
		ml.b.y = y;
		AM_drawMline(&ml, color);
	}

}

int32_t AM_lookupColour( int32_t paletteindex, doombool blinking )
{
	return colormaps[ ( blinking ? lightlev * 256 : 0 ) + paletteindex ];
}

//
// Determines visible lines, draws them.
// This is LineDef based, not LineSeg based.
//

#define AM_CanDraw( type ) ( type.val >= 0 )

void AM_DrawLine( line_t* line, mapstyledata_t* style )
{
	mline_t l;

	l.a.x = line->v1->rend.x;
	l.a.y = line->v1->rend.y;
	l.b.x = line->v2->rend.x;
	l.b.y = line->v2->rend.y;

	if (cheating || (line->flags & ML_MAPPED))
	{
		if ((line->flags & ML_DONTDRAW) && !cheating)
		{
			return;
		}

		if( AM_CanDraw( style->sectorsecretsundiscovered ) &&	( line->frontsector->secretstate == Secret_Undiscovered
																|| line->backsector && line->backsector->secretstate == Secret_Undiscovered ) )
		{
			AM_drawMline(&l, AM_lookupColour( style->sectorsecretsundiscovered.val, style->sectorsecretsundiscovered.flags ) );
		}
		else if( AM_CanDraw( style->sectorsecrets ) &&	( line->frontsector->secretstate == Secret_Discovered
														|| line->backsector && line->backsector->secretstate == Secret_Discovered ) )
		{
			AM_drawMline(&l, AM_lookupColour( style->sectorsecrets.val, style->sectorsecrets.flags ) );
		}
		else if( !line->backsector)
		{
			if( AM_CanDraw( style->walls ) ) AM_drawMline(&l, AM_lookupColour( style->walls.val, style->walls.flags ) );
		}
		else if ( line->special == 39) // teleporters
		{
			if( AM_CanDraw( style->teleporters ) ) AM_drawMline(&l, AM_lookupColour( style->teleporters.val, style->teleporters.flags ) );
		}
		else if (line->flags & ML_SECRET) // secret door
		{
			if ( AM_CanDraw( style->linesecrets ) && cheating)
			{
				AM_drawMline(&l, AM_lookupColour( style->linesecrets.val, style->linesecrets.flags ) );
			}
			else if( AM_CanDraw( style->walls ) )
			{
				AM_drawMline(&l, AM_lookupColour( style->walls.val, style->walls.flags ) );
			}
		}
		else if ( line->backsector->floorheight != line->frontsector->floorheight)
		{
			if( AM_CanDraw( style->floorchange ) ) AM_drawMline(&l, AM_lookupColour( style->floorchange.val, style->floorchange.flags ) ); // floor level change
		}
		else if ( line->backsector->ceilingheight != line->frontsector->ceilingheight)
		{
			if( AM_CanDraw( style->ceilingchange ) ) AM_drawMline(&l, AM_lookupColour( style->ceilingchange.val, style->ceilingchange.flags ) ); // ceiling level change
		}
		else if ( cheating)
		{
			if( AM_CanDraw( style->nochange ) ) AM_drawMline(&l, AM_lookupColour( style->nochange.val, style->nochange.flags ) );
		}
	}
	else if ( AM_CanDraw( style->areamap ) && plr->powers[ pw_allmap ] && !(line->flags & ML_DONTDRAW) )
	{
		AM_drawMline(&l, AM_lookupColour( style->areamap.val, style->areamap.flags ) );
	}
}


mapstyledata_t	map_stylenotblockmap =
{
	111,						0, // background
	111,						0, // grid
	111,						0, // areamap
	111,						0, // walls
	111,						0, // teleporters
	111,						0, // linesecrets
	111,						0, // sectorsecrets
	111,						0, // sectorsecretsundiscovered
	111,						0, // floorchange
	111,						0, // ceilingchange
	111,						0, // nochange

	THINGCOLORS,					0, // things
	214,							0, // monsters_alive
	151,							0, // monsters_dead
	196,							0, // items_counted
	202,							0, // items_uncounted
	173,							0, // projectiles
	110,							0, // puffs

	WHITE,							0, // playerarrow
	XHAIRCOLORS,					0, // crosshair
};

void AM_drawWalls( mapstyledata_t* style )
{
	for ( int32_t index = 0 ; index < numlines; ++index )
	{
		AM_DrawLine( &lines[ index ], style );
	}

#if 0
	// Blockmap visualiser, might turn it in to a real option IDK

	int32_t bmapx = ( plr->mo->x - bmaporgx ) >> MAPBLOCKSHIFT;
	int32_t bmapy = ( plr->mo->y - bmaporgy ) >> MAPBLOCKSHIFT;
	int32_t offset = bmapy * bmapwidth + bmapx;

	offset = *(blockmap + offset);

	for ( int32_t index = 0 ; index < numlines; ++index )
	{
		doombool inlist = false;
		for( blockmap_t* list = blockmapbase + offset; *list != BLOCKMAP_INVALID; ++list )
		{
			if( *list == index )
			{
				inlist = true;
				break;
			}
		}

		AM_DrawLine( &lines[ index ], inlist ? style : &map_stylenotblockmap );
	}
#endif
}


//
// Rotation in 2D.
// Used to rotate player arrow line character.
//
void
AM_rotate
( rend_fixed_t*	x,
  rend_fixed_t*	y,
  angle_t	a )
{
    rend_fixed_t tmpx;

    tmpx =
	RendFixedMul(*x, FixedToRendFixed( renderfinecosine[a>>RENDERANGLETOFINESHIFT] ))
	- RendFixedMul(*y, FixedToRendFixed( renderfinesine[a>>RENDERANGLETOFINESHIFT] ));
    
    *y   =
	RendFixedMul(*x, FixedToRendFixed( renderfinesine[a>>RENDERANGLETOFINESHIFT] ))
	+ RendFixedMul(*y, FixedToRendFixed( renderfinecosine[a>>RENDERANGLETOFINESHIFT] ));

    *x = tmpx;
}

void
AM_drawLineCharacter
( mline_t*	lineguy,
  int		lineguylines,
  rend_fixed_t	scale,
  angle_t	angle,
  int		color,
  rend_fixed_t	x,
  rend_fixed_t	y )
{
    int		i;
    mline_t	l;

    for (i=0;i<lineguylines;i++)
    {
	l.a.x = lineguy[i].a.x;
	l.a.y = lineguy[i].a.y;

	if (scale)
	{
	    l.a.x = RendFixedMul(scale, l.a.x);
	    l.a.y = RendFixedMul(scale, l.a.y);
	}

	if (angle)
	    AM_rotate(&l.a.x, &l.a.y, angle);

	l.a.x += x;
	l.a.y += y;

	l.b.x = lineguy[i].b.x;
	l.b.y = lineguy[i].b.y;

	if (scale)
	{
	    l.b.x = RendFixedMul(scale, l.b.x);
	    l.b.y = RendFixedMul(scale, l.b.y);
	}

	if (angle)
	    AM_rotate(&l.b.x, &l.b.y, angle);
	
	l.b.x += x;
	l.b.y += y;

	AM_drawMline(&l, color);
    }
}

void AM_drawPlayers( mapstyledata_t* style )
{
    int		i;
    player_t*	p;
    static int 	their_colors[] = { GREENS, GRAYS, BROWNS, REDS };
    int		their_color = -1;
    int		color;

	if( AM_CanDraw( style->playerarrow ) )
	{

		if (!netgame)
		{
			if (cheating)
			{
				AM_drawLineCharacter
				(cheat_player_arrow, arrlen(cheat_player_arrow), 0,
				 plr->mo->angle, AM_lookupColour( style->playerarrow.val, style->playerarrow.flags ), FixedToRendFixed( plr->mo->x ), FixedToRendFixed( plr->mo->y ) );
			}
			else
			{
				AM_drawLineCharacter
				(player_arrow, arrlen(player_arrow), 0, plr->mo->angle,
				 AM_lookupColour( style->playerarrow.val, style->playerarrow.flags ), FixedToRendFixed( plr->mo->x ), FixedToRendFixed( plr->mo->y ) );
			}
			return;
		}

		for (i=0;i<MAXPLAYERS;i++)
		{
			their_color++;
			p = &players[i];

			if ( (deathmatch && !singledemo) && p != plr)
				continue;

			if (!playeringame[i])
				continue;

			if (p->powers[pw_invisibility])
				color = 246; // *close* to black
			else
				color = their_colors[their_color];
	
			AM_drawLineCharacter
				(player_arrow, arrlen(player_arrow), 0, p->mo->angle,
				 AM_lookupColour( color, style->playerarrow.flags ), FixedToRendFixed( p->mo->x ), FixedToRendFixed( p->mo->y ) );
		}
	}

}

void AM_drawThings( mapstyledata_t* style )
{
    int		i;
    mobj_t*	t;
	int32_t	colour;

	for (i=0;i<numsectors;i++)
	{
		t = sectors[i].thinglist;
		while (t)
		{
			colour = -1;

			if( t->flags & MF_CORPSE )
			{
				if( AM_CanDraw( style->monsters_dead ) ) colour = AM_lookupColour( style->monsters_dead.val, style->monsters_dead.flags );
			}
			else if( t->flags & MF_COUNTKILL )
			{
				if( AM_CanDraw( style->monsters_alive ) ) colour = AM_lookupColour( style->monsters_alive.val, style->monsters_alive.flags );
			}
			else if( t->flags & MF_COUNTITEM )
			{
				if( AM_CanDraw( style->items_counted ) ) colour =  AM_lookupColour( style->items_counted.val, style->items_counted.flags );
			}
			else if( t->flags & MF_SPECIAL )
			{
				if( AM_CanDraw( style->items_uncounted ) ) colour =  AM_lookupColour( style->items_uncounted.val, style->items_uncounted.flags );
			}
			else if( t->flags & MF_MISSILE )
			{
				if( AM_CanDraw( style->projectiles ) ) colour = AM_lookupColour( style->projectiles.val, style->projectiles.flags );
			}
			else if( AM_CanDraw( style->things ) )
			{
				colour = AM_lookupColour( style->things.val, style->things.flags );
			}

			if( colour >= 0 )
			{
				AM_drawLineCharacter( thintriangle_guy, arrlen(thintriangle_guy), IntToRendFixed( 16 ), t->angle, AM_lookupColour( colour, false ), FixedToRendFixed( t->x ), FixedToRendFixed( t->y ) );
			}
			t = t->snext;
		}
	}
}

void AM_drawMarks(void)
{
    int i, fx, fy, w, h;

    for (i=0;i<AM_NUMMARKPOINTS;i++)
    {
	if (markpoints[i].x != -1)
	{
	    //      w = SHORT(marknums[i]->width);
	    //      h = SHORT(marknums[i]->height);
	    w = 5; // because something's wrong with the wad, i guess
	    h = 6; // because something's wrong with the wad, i guess
	    fx = CXMTOF(markpoints[i].x);
	    fy = CYMTOF(markpoints[i].y);
	    if (fx >= f_x && fx <= f_w - w && fy >= f_y && fy <= f_h - h)
		V_DrawPatch(fx, fy, marknums[i]);
	}
    }

}

void AM_drawCrosshair(int color)
{
	// HACK to give us a dot at proper scale

	int32_t loopx;
	fline_t fl;
	fl.a.x = fl.b.x = ( f_w >> 1 ) - ( linewidth >> 1 );
	fl.a.y = fl.b.y = ( f_h >> 1 ) - ( lineheight >> 1 );
	fl.b.y += lineheight;

	for( loopx = 0; loopx < linewidth; ++loopx )
	{
		AM_drawFline( &fl, color );
		++fl.a.x;
		++fl.b.x;
	}

}

void AM_Drawer (void)
{
	mapstyledata_t* style = &map_styledata[ map_style ];

	if (!automapactive) return;

	// TODO: FIX THIS HACK
	fb = I_VideoBuffer;

	linewidth = RendFixedToInt( RendFixedDiv( IntToRendFixed( 1 ), FixedToRendFixed( V_WIDTHSTEP ) ) );
	lineheight = RendFixedToInt( RendFixedDiv( IntToRendFixed( 1 ), FixedToRendFixed( V_HEIGHTSTEP ) ) );

	AM_clearFB( AM_CanDraw( style->background ) ? AM_lookupColour( style->background.val, style->background.flags )
												: AM_lookupColour( map_styledata[ MapStyle_Original ].background.val, map_styledata[ MapStyle_Original ].background.flags ) );
	if (grid)
	{
		AM_drawGrid( AM_CanDraw( style->grid )	? AM_lookupColour( style->grid.val, style->grid.flags )
												: AM_lookupColour( map_styledata[ MapStyle_Original ].grid.val, map_styledata[ MapStyle_Original ].grid.flags ) );
	}
	AM_drawWalls( style );
	AM_drawPlayers( style );
	if (cheating==2)
	{
		AM_drawThings( style );
	}
	AM_drawCrosshair( AM_CanDraw( style->crosshair )	? AM_lookupColour( style->crosshair.val, style->crosshair.flags )
														: AM_lookupColour( map_styledata[ MapStyle_Original ].crosshair.val, map_styledata[ MapStyle_Original ].crosshair.flags ) );
	
	AM_drawMarks();

	V_MarkRect(f_x, f_y, f_w, f_h);

}
