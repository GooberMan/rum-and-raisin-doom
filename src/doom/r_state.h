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
//	Refresh/render internal state variables (global).
//


#ifndef __R_STATE__
#define __R_STATE__

// Need data structure definitions.
#include "d_player.h"
#include "r_data.h"


#if defined( __cplusplus )
extern "C" {
#endif // defined( __cplusplus )

//
// Refresh internal data structures,
//  for rendering.
//

extern texturecomposite_t** texturelookup;
extern texturecomposite_t** flatlookup;

// needed for pre rendering (fracs)
extern fixed_t*		spritewidth;

extern fixed_t*		spriteoffset;
extern fixed_t*		spritetopoffset;

extern lighttable_t*	colormaps;

extern int32_t		viewwidth;
extern int32_t		scaledviewwidth;
extern int32_t		viewheight;

extern int32_t		render_width;
extern int32_t		render_height;
extern int32_t		render_post_scaling;
extern int32_t		dynamic_resolution_scaling;
extern int32_t		frame_width;
extern int32_t		frame_adjusted_width;
extern int32_t		frame_height;

extern int		firstflat;

// for global animation
extern int*		flattranslation;	
extern int*		texturetranslation;	


// Sprite....
extern int32_t		firstspritelump;
extern int32_t		lastspritelump;
extern int32_t		numspritelumps;

extern patch_t**	spritepatches;


//
// Lookup tables for map data.
//
extern int32_t		numsprites;
extern spritedef_t*	sprites;

extern int32_t		numvertexes;
extern vertex_t*	vertexes;

extern int32_t		numsegs;
extern seg_t*		segs;

extern int32_t		numsectors;
extern sector_t*	sectors;
extern sectorinstance_t*	prevsectors;
extern sectorinstance_t*	currsectors;
extern sectorinstance_t*	rendsectors;

extern int32_t		numsubsectors;
extern subsector_t*	subsectors;

extern int32_t		numnodes;
extern node_t*		nodes;

extern int32_t		numlines;
extern line_t*		lines;

extern int32_t		numsides;
extern side_t*		sides;
extern sideinstance_t*		prevsides;
extern sideinstance_t*		currsides;
extern sideinstance_t*		rendsides;


//
// POV data.
//
extern fixed_t		viewx;
extern fixed_t		viewy;
extern fixed_t		viewz;

extern rend_fixed_t	viewlerp;

extern angle_t		viewangle;
extern player_t*	viewplayer;

// ?
extern angle_t		clipangle;

extern int32_t		viewangletox[ RENDERFINEANGLES/2 ];
extern angle_t*		xtoviewangle;

#if defined( __cplusplus )
}
#endif // defined( __cplusplus )

#endif
