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
boolean AM_Responder (event_t* ev);

// Called by main loop.
void AM_Ticker (void);

// Called by main loop,
// called instead of view drawer if automap active.
void AM_Drawer (void);

// Called to force the automap to quit
// if the level is completed while it is up.
void AM_Stop (void);

void AM_BindAutomapVariables( void );


extern cheatseq_t cheat_amap;

typedef struct mapstyledata_s
{
	int32_t	background;
	int32_t	grid;
	int32_t	areamap;
	int32_t	walls;
	int32_t	teleporters;
	int32_t	linesecrets;
	int32_t	sectorsecrets;
	int32_t	floorchange;
	int32_t	ceilingchange;
	int32_t	nochange;
	
	int32_t	things;
	int32_t	monsters_alive;
	int32_t	monsters_dead;
	int32_t	items_counted;
	int32_t items_uncounted;
	int32_t	projectiles;
	int32_t	puffs;
	
	int32_t	playerarrow;
	int32_t	crosshair;
} mapstyledata_t;

typedef enum mapstyle_e
{
	MapStyle_Custom,
	MapStyle_Original,
	MapStyle_ZDoom,

	MapStyle_Max,
} mapstyle_t;

extern int32_t	map_style;
extern int32_t	map_fill;

extern mapstyledata_t	map_styledata[ MapStyle_Max ];

#endif
