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
//  AutoMap module.
//

#ifndef __AMMAP_H__
#define __AMMAP_H__

#include "d_event.h"
#include "m_cheat.h"

// Used by ST StatusBar stuff.
#define AM_MSGHEADER (('a'<<24)+('m'<<16))
#define AM_MSGENTERED (AM_MSGHEADER | ('e'<<8))
#define AM_MSGEXITED (AM_MSGHEADER | ('x'<<8))


// Called by main loop.
DOOM_C_API doombool AM_Responder (event_t* ev);

// Called by main loop.
DOOM_C_API void AM_Ticker (void);

// Called by main loop,
// called instead of view drawer if automap active.
DOOM_C_API void AM_Drawer (void);

// Called to force the automap to quit
// if the level is completed while it is up.
DOOM_C_API void AM_Stop (void);

DOOM_C_API void AM_BindAutomapVariables( void );


DOOM_C_API extern cheatseq_t cheat_amap;

DOOM_C_API typedef struct mapstyleentry_s
{
	int32_t val;
	int32_t flags;
} mapstyleentry_t;

DOOM_C_API typedef struct mapstyledata_s
{
	mapstyleentry_t	background;
	mapstyleentry_t	grid;
	mapstyleentry_t	areamap;
	mapstyleentry_t	walls;
	mapstyleentry_t	teleporters;
	mapstyleentry_t	linesecrets;
	mapstyleentry_t	sectorsecrets;
	mapstyleentry_t	sectorsecretsundiscovered;
	mapstyleentry_t	floorchange;
	mapstyleentry_t	ceilingchange;
	mapstyleentry_t	nochange;
	
	mapstyleentry_t	things;
	mapstyleentry_t	monsters_alive;
	mapstyleentry_t	monsters_dead;
	mapstyleentry_t	items_counted;
	mapstyleentry_t items_uncounted;
	mapstyleentry_t	projectiles;
	mapstyleentry_t	puffs;
	
	mapstyleentry_t	playerarrow;
	mapstyleentry_t	crosshair;
} mapstyledata_t;

DOOM_C_API typedef enum mapstyle_e
{
	MapStyle_Custom,
	MapStyle_Original,
	MapStyle_ZDoom,

	MapStyle_Max,
} mapstyle_t;

DOOM_C_API extern int32_t	map_style;
DOOM_C_API extern int32_t	map_fill;

DOOM_C_API extern mapstyledata_t	map_styledata[ MapStyle_Max ];

#endif
