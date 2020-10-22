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
//      Refresh/rendering module, shared data struct definitions.
//


#ifndef __R_DEFS__
#define __R_DEFS__


// Screenwidth.
#include "doomdef.h"

// Some more or less basic data types
// we depend on.
#include "m_fixed.h"

// We rely on the thinker data struct
// to handle sound origins in sectors.
#include "d_think.h"
// SECTORS do store MObjs anyway.
#include "p_mobj.h"

#include "i_video.h"

#include "v_patch.h"
// For vbuffer_t
// Should move it out elsewhere
#include "v_video.h"
// hu_textline_t lives in here, but we can't access it thanks to recursive includes :-(
//#include "hu_lib.h"


// Silhouette, needed for clipping Segs (mainly)
// and sprites representing things.
#define SIL_NONE				0
#define SIL_BOTTOM				1
#define SIL_TOP					2
#define SIL_BOTH				3

#define MAXVISPLANES			256
#define MAXOPENINGS				( SCREENWIDTH*32 )
#define MAXDRAWSEGS				( MAXVISPLANES << 2 )

// We must expand MAXSEGS to the theoretical limit of the number of solidsegs
// that can be generated in a scene by the DOOM engine. This was determined by
// Lee Killough during BOOM development to be a function of the screensize.
// The simplest thing we can do, other than fix this bug, is to let the game
// render overage and then bomb out by detecting the overflow after the 
// fact. -haleyjd
#define VANILLA_MAXSEGS			32
#define MAXSEGS					(SCREENWIDTH / 2 + 1)

typedef uint16_t				vpindex_t;
#define VPINDEX_INVALID			( ~(vpindex_t)0 )

typedef pixel_t					lighttable_t;

typedef int32_t					vertclip_t;
#define MASKEDTEXCOL_INVALID	( (vertclip_t)0x7FFFFFFF )


//
// INTERNAL MAP TYPES
//  used by play and refresh
//

//
// Your plain vanilla vertex.
// Note: transformed values not buffered locally,
//  like some DOOM-alikes ("wt", "WebView") did.
//
typedef struct
{
    fixed_t	x;
    fixed_t	y;
    
} vertex_t;


// Forward of LineDefs, for Sectors.
struct line_s;

// Each sector has a degenmobj_t in its center
//  for sound origin purposes.
// I suppose this does not handle sound from
//  moving objects (doppler), because
//  position is prolly just buffered, not
//  updated.
typedef struct
{
    thinker_t		thinker;	// not used for anything
    fixed_t		x;
    fixed_t		y;
    fixed_t		z;

} degenmobj_t;

//
// The SECTORS record, at runtime.
// Stores things/mobjs.
//
typedef	struct
{
	int32_t		index;

	fixed_t		floorheight;
	fixed_t		ceilingheight;
	int16_t		floorpic;
	int16_t		ceilingpic;
	int16_t		lightlevel;
	int16_t		special;
	int16_t		tag;

	// 0 = untraversed, 1,2 = sndlines -1
	int32_t		soundtraversed;

	// thing that made a sound (or null)
	mobj_t*		soundtarget	;

	// mapblock bounding box for height changes
	int32_t		blockbox[4];

	// origin for any sounds played by the sector
	degenmobj_t	soundorg;

	// if == validcount, already checked
	// Used by algorithms that traverse the BSP. Not thread safe.
	int32_t		validcount;

	// list of mobjs in sector
	mobj_t*		thinglist;

	// thinker_t for reversable actions
	void*		specialdata;

	int32_t			linecount;
	struct line_s**	lines;	// [linecount] size
} sector_t;




//
// The SideDef.
//

typedef struct
{
    // add this to the calculated texture column
    fixed_t	textureoffset;
    
    // add this to the calculated texture top
    fixed_t	rowoffset;

    // Texture indices.
    // We do not maintain names here. 
    short	toptexture;
    short	bottomtexture;
    short	midtexture;

    // Sector the SideDef is facing.
    sector_t*	sector;
    
} side_t;



//
// Move clipping aid for LineDefs.
//
typedef enum
{
    ST_HORIZONTAL,
    ST_VERTICAL,
    ST_POSITIVE,
    ST_NEGATIVE

} slopetype_t;



typedef struct line_s
{
    // Vertices, from v1 to v2.
    vertex_t*	v1;
    vertex_t*	v2;

    // Precalculated v2 - v1 for side checking.
    fixed_t	dx;
    fixed_t	dy;

    // Animation related.
    short	flags;
    short	special;
    short	tag;

    // Visual appearance: SideDefs.
    //  sidenum[1] will be -1 if one sided
    short	sidenum[2];			

    // Neat. Another bounding box, for the extent
    //  of the LineDef.
    fixed_t	bbox[4];

    // To aid move clipping.
    slopetype_t	slopetype;

    // Front and back sector.
    // Note: redundant? Can be retrieved from SideDefs.
    sector_t*	frontsector;
    sector_t*	backsector;

    // if == validcount, already checked
    int		validcount;

    // thinker_t for reversable actions
    void*	specialdata;		
} line_t;




//
// A SubSector.
// References a Sector.
// Basically, this is a list of LineSegs,
//  indicating the visible walls that define
//  (all or some) sides of a convex BSP leaf.
//
typedef struct subsector_s
{
    sector_t*	sector;
    short	numlines;
    short	firstline;
    
} subsector_t;



//
// The LineSeg.
//
typedef struct
{
    vertex_t*	v1;
    vertex_t*	v2;
    
    fixed_t	offset;

    angle_t	angle;

    side_t*	sidedef;
    line_t*	linedef;

    // Sector references.
    // Could be retrieved from linedef, too.
    // backsector is NULL for one sided lines
    sector_t*	frontsector;
    sector_t*	backsector;
    
} seg_t;



//
// BSP node.
//
typedef struct
{
    // Partition line.
    fixed_t	x;
    fixed_t	y;
    fixed_t	dx;
    fixed_t	dy;

    // Bounding box for each child.
    fixed_t	bbox[2][4];

    // If NF_SUBSECTOR its a subsector.
    unsigned short children[2];
    
} node_t;


//
// OTHER TYPES
//

// This could be wider for >8 bit display.
// Indeed, true color support is posibble
//  precalculating 24bpp lightmap/colormap LUT.
//  from darkening PLAYPAL to all black.
// Could even us emore than 32 levels.

//
// ?
//
typedef struct drawseg_s
{
    seg_t*		curline;
    int			x1;
    int			x2;

    fixed_t		scale1;
    fixed_t		scale2;
    fixed_t		scalestep;

    // 0=none, 1=bottom, 2=top, 3=both
    int			silhouette;

    // do not clip sprites above this
    fixed_t		bsilheight;

    // do not clip sprites below this
    fixed_t		tsilheight;
    
    // Pointers to lists for sprite clipping,
    //  all three adjusted so [x1] is first value.
    vertclip_t*	sprtopclip;		
    vertclip_t*	sprbottomclip;	

	// TODO: Make this 32-bit
    vertclip_t*		maskedtexturecol;
    
} drawseg_t;

// A vissprite_t is a thing
//  that will be drawn during a refresh.
// I.e. a sprite object that is partly visible.
typedef struct vissprite_s
{
    // Doubly linked list.
    struct vissprite_s*	prev;
    struct vissprite_s*	next;
    
    int			x1;
    int			x2;

    // for line side calculation
    fixed_t		gx;
    fixed_t		gy;		

    // global bottom / top for silhouette clipping
    fixed_t		gz;
    fixed_t		gzt;

    // horizontal position of x1
    fixed_t		startfrac;
    
    fixed_t		scale;
    
    // negative if flipped
    fixed_t		xiscale;	

    fixed_t		texturemid;
    int			patch;

    // for color translation and shadow draw,
    //  maxbright frames as well
    lighttable_t*	colormap;
   
    int			mobjflags;
    
} vissprite_t;

#define MAXVISSPRITES  	1024

//	
// Sprites are patches with a special naming convention
//  so they can be recognized by R_InitSprites.
// The base name is NNNNFx or NNNNFxFx, with
//  x indicating the rotation, x = 0, 1-7.
// The sprite and frame specified by a thing_t
//  is range checked at run time.
// A sprite is a patch_t that is assumed to represent
//  a three dimensional object and may have multiple
//  rotations pre drawn.
// Horizontal flipping is used to save space,
//  thus NNNNF2F5 defines a mirrored patch.
// Some sprites will only have one picture used
// for all views: NNNNF0
//
typedef struct
{
    // If false use 0 for any position.
    // Note: as eight entries are available,
    //  we might as well insert the same name eight times.
    boolean	rotate;

    // Lump to use for view angles 0-7.
    short	lump[8];

    // Flip bit (1 = flip) to use for view angles 0-7.
    byte	flip[8];
    
} spriteframe_t;



//
// A sprite definition:
//  a number of animation frames.
//
typedef struct
{
    int			numframes;
    spriteframe_t*	spriteframes;

} spritedef_t;


//
// Now what is a visplane, anyway?
// 
typedef struct
{
  fixed_t		height;
  int			picnum;
  int			lightlevel;
  int			minx;
  int			maxx;
  
  // leave pads for [minx-1]/[maxx+1]
  
  vpindex_t		pad1;
  // Here lies the rub for all
  //  dynamic resize/change of resolution.
  vpindex_t		top[SCREENWIDTH];
  vpindex_t		pad2;
  vpindex_t		pad3;
  // See above.
  vpindex_t		bottom[SCREENWIDTH];
  vpindex_t		pad4;

} visplane_t;

//
// ClipWallSegment
// Clips the given range of columns
// and includes it in the new clip list.
//
typedef	struct
{
    int	first;
    int last;
    
} cliprange_t;

typedef struct wallcontext_s
{
	angle_t			normalangle;
	angle_t			angle1;
	angle_t			centerangle;
	fixed_t			offset;
	fixed_t			distance;
	fixed_t			scale;
	fixed_t			scalestep;
	fixed_t			midtexturemid;
	fixed_t			toptexturemid;
	fixed_t			bottomtexturemid;

	lighttable_t**	lights;
	int32_t			lightsindex;

} wallcontext_t;

typedef struct bspcontext_s
{
	seg_t*			curline;
	side_t*			sidedef;
	line_t*			linedef;
	sector_t*		frontsector;
	sector_t*		backsector;

	drawseg_t		drawsegs[MAXDRAWSEGS];
	drawseg_t*		thisdrawseg;

	cliprange_t		solidsegs[MAXSEGS];
	cliprange_t*	solidsegsend;

} bspcontext_t;

typedef struct planecontext_s
{
	visplane_t		visplanes[MAXVISPLANES];
	visplane_t*		lastvisplane;
	visplane_t*		floorplane;
	visplane_t*		ceilingplane;

	vertclip_t		openings[MAXOPENINGS];
	vertclip_t*		lastopening;

	vertclip_t		floorclip[SCREENWIDTH];
	vertclip_t		ceilingclip[SCREENWIDTH];

	int32_t			spanstart[SCREENHEIGHT];
	int32_t			spanstop[SCREENHEIGHT];

	lighttable_t**	planezlight;
	int32_t			planezlightindex;
	fixed_t			planeheight;

	fixed_t			basexscale;
	fixed_t			baseyscale;

	fixed_t			cachedheight[SCREENHEIGHT];
	fixed_t			cacheddistance[SCREENHEIGHT];
	fixed_t			cachedxstep[SCREENHEIGHT];
	fixed_t			cachedystep[SCREENHEIGHT];

} planecontext_t;

typedef struct spritecontext_s
{
	int32_t			leftclip;
	int32_t			rightclip;

	// The +1 accounts for the overflow sprite
	vissprite_t		vissprites[MAXVISSPRITES + 1];

	vissprite_t*	nextvissprite;

	vissprite_t		vsprsortedhead;

	vertclip_t*		mfloorclip;
	vertclip_t*		mceilingclip;
	fixed_t			spryscale;
	fixed_t			sprtopscreen;

	lighttable_t**	spritelights;
	
	boolean*		sectorvisited;

} spritecontext_t;

//
// Render context is required for threaded rendering.
// So everything "global" goes in here. Everything.
//

typedef struct colcontext_s colcontext_t;
typedef struct spancontext_s spancontext_t;

typedef void (*colfunc_t)( colcontext_t* );
typedef void (*spanfunc_t)( spancontext_t* );
typedef void (*planefunction_t)(int top, int bottom);

typedef struct colcontext_s
{
	vbuffer_t		output;
	byte*			source;

	lighttable_t*	colormap;
	byte*			translation;

	colfunc_t		colfunc;

	int32_t			x;
	int32_t			yl;
	int32_t			yh;
	fixed_t			scale;
	fixed_t			iscale;
	fixed_t			texturemid;
} colcontext_t;

typedef struct spancontext_s
{
	vbuffer_t		output;
	byte*			source;

	int32_t			y; 
	int32_t			x1; 
	int32_t			x2;

	fixed_t			xfrac; 
	fixed_t			yfrac; 
	fixed_t			xstep; 
	fixed_t			ystep;
} spancontext_t;

typedef struct rendercontext_s
{
	// Setup
	vbuffer_t			buffer;

	int32_t				begincolumn;
	int32_t				endcolumn;

	uint64_t			starttime;
	uint64_t			endtime;
	uint64_t			timetaken;

	bspcontext_t		bspcontext;
	planecontext_t		planecontext;
	spritecontext_t		spritecontext;

	// Functions
	colfunc_t			colfunc;
	colfunc_t			fuzzcolfunc;
	colfunc_t			transcolfunc;
	
	spanfunc_t			spanfunc;

	// Debug
	void*				debugtime;
	void*				debugpercent;

} rendercontext_t;

#endif
