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

#define VANILLA_MAXVISPLANES	128
#define MAXVISPLANES			6144
#define VANILLA_MAXOPENINGS		( 320 * 64 )
#define MAXOPENINGS				( MAXSCREENWIDTH*64 )
#define VANILLA_MAXDRAWSEGS		( VANILLA_MAXVISPLANES << 2 )
#define MAXDRAWSEGS				( MAXVISPLANES << 2 )

// We must expand MAXSEGS to the theoretical limit of the number of solidsegs
// that can be generated in a scene by the DOOM engine. This was determined by
// Lee Killough during BOOM development to be a function of the screensize.
// The simplest thing we can do, other than fix this bug, is to let the game
// render overage and then bomb out by detecting the overflow after the 
// fact. -haleyjd
#define VANILLA_MAXSEGS			32
#define MAXSEGS					( MAXSCREENWIDTH / 2 + 1 )

typedef uint16_t				vpindex_t;
#define VPINDEX_INVALID			( ~(vpindex_t)0 )

typedef pixel_t					lighttable_t;

typedef int32_t					vertclip_t;
#define MASKEDTEXCOL_INVALID	( (vertclip_t)0x7FFFFFFF )

#define VANILLA_MAXVISSPRITES	128
#define MAXVISSPRITES			1024

#define RENDER_PERF_GRAPHING	1
#define MAXPROFILETIMES			( 35 * 8 )


//
// INTERNAL MAP TYPES
//  used by play and refresh
//

//
// Your plain vanilla vertex.
// Note: transformed values not buffered locally,
//  like some DOOM-alikes ("wt", "WebView") did.
//
typedef struct rend_vertex_s
{
	rend_fixed_t		x;
	rend_fixed_t		y;
} rend_vertex_t;

typedef struct
{
	rend_vertex_t		rend;

	fixed_t				x;
	fixed_t				y;

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
	thinker_t			thinker;	// not used for anything
	fixed_t				x;
	fixed_t				y;
	fixed_t				z;
} degenmobj_t;

typedef enum secretstate_e
{
	Secret_None,
	Secret_Undiscovered,
	Secret_Discovered
} secretstate_t;

//
// The SECTORS record, at runtime.
// Stores things/mobjs.
//
typedef struct rend_sector_s
{
	rend_fixed_t		floorheight;
	rend_fixed_t		ceilingheight;
} rend_sector_t;

typedef	struct
{
	rend_sector_t		rend;

	int32_t				index;

	fixed_t				floorheight;
	fixed_t				ceilingheight;
	int16_t				floorpic;
	int16_t				ceilingpic;
	int16_t				lightlevel;
	int16_t				special;
	int16_t				tag;

	int32_t				secretstate;

	// 0 = untraversed, 1,2 = sndlines -1
	int32_t				soundtraversed;

	// thing that made a sound (or null)
	mobj_t*				soundtarget;

	// mapblock bounding box for height changes
	int32_t				blockbox[4];

	// origin for any sounds played by the sector
	degenmobj_t			soundorg;

	// if == validcount, already checked
	// Used by algorithms that traverse the BSP. Not thread safe.
	int32_t				validcount;

	// list of mobjs in sector
	mobj_t*				thinglist;

	// thinker_t for reversable actions
	void*				specialdata;

	int32_t				linecount;
	struct line_s**		lines;	// [linecount] size
} sector_t;




//
// The SideDef.
//

typedef struct rend_side_s
{
	rend_fixed_t		textureoffset;
	rend_fixed_t		rowoffset;
} rend_side_t;

typedef struct side_s
{
	rend_side_t			rend;

	// add this to the calculated texture column
	fixed_t				textureoffset;

	// add this to the calculated texture top
	fixed_t				rowoffset;

	// Texture indices.
	// We do not maintain names here. 
	int16_t				toptexture;
	int16_t				bottomtexture;
	int16_t				midtexture;

	// Sector the SideDef is facing.
	sector_t*			sector;

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
	int32_t				index;

	// Vertices, from v1 to v2.
	vertex_t*			v1;
	vertex_t*			v2;

	// Precalculated v2 - v1 for side checking.
	fixed_t				dx;
	fixed_t				dy;

	// Animation related.
	int16_t				flags;
	int16_t				special;
	int16_t				tag;

	// Visual appearance: SideDefs.
	//  sidenum[1] will be -1 if one sided
	int32_t				sidenum[2]; // originally int16_t

	// Neat. Another bounding box, for the extent
	//  of the LineDef.
	fixed_t				bbox[4];

	// To aid move clipping.
	slopetype_t			slopetype;

	// Front and back sector.
	// Note: redundant? Can be retrieved from SideDefs.
	sector_t*			frontsector;
	sector_t*			backsector;

	// if == validcount, already checked
	int32_t				validcount;

	// thinker_t for reversable actions
	void*				specialdata;
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
	sector_t*			sector;
	uint32_t			numlines;
	uint32_t			firstline;
	int32_t				index;
} subsector_t;



//
// The LineSeg.
//

typedef struct rend_seg_s
{
	rend_fixed_t		offset;
} rend_seg_t;

typedef struct seg_e
{
	rend_seg_t			rend;

	vertex_t*			v1;
	vertex_t*			v2;

	fixed_t				offset;

	angle_t				angle;

	side_t*				sidedef;
	line_t*				linedef;

	// Sector references.
	// Could be retrieved from linedef, too.
	// backsector is NULL for one sided lines
	sector_t*			frontsector;
	sector_t*			backsector;
} seg_t;



//
// BSP node.
//
typedef struct rend_node_s
{
	rend_fixed_t		x;
	rend_fixed_t		y;
	rend_fixed_t		dx;
	rend_fixed_t		dy;
	rend_fixed_t		bbox[ 2 ][ 4 ];
} rend_node_t;

typedef struct divline_s
{
	fixed_t	x;
	fixed_t	y;
	fixed_t	dx;
	fixed_t	dy;
} divline_t;

typedef struct node_s
{
	rend_node_t			rend;

	// Partition line.
	// Rum and Raisin - expected to match up with divline_t. Should move that in here
	divline_t			divline;

	// Bounding box for each child.
	fixed_t				bbox[2][4];

	// If NF_SUBSECTOR its a subsector.
	uint32_t			children[2]; // originally uint16_t
} node_t;


// Blockmap entries
typedef int32_t						blockmap_t;
typedef int16_t						mapblockmap_t;
typedef uint16_t					mapblockmap_extended_t;

#define BLOCKMAP_INVALID_VANILLA	0xFFFF
#define BLOCKMAP_INVALID			-1

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
	seg_t*				curline;
	int32_t				x1;
	int32_t				x2;

	rend_fixed_t		scale1;
	rend_fixed_t		scale2;
	rend_fixed_t		scalestep;

	// 0=none, 1=bottom, 2=top, 3=both
	int32_t				silhouette;

	// do not clip sprites above this
	rend_fixed_t		bsilheight;

	// do not clip sprites below this
	rend_fixed_t		tsilheight;

	// Pointers to lists for sprite clipping,
	//  all three adjusted so [x1] is first value.
	vertclip_t*			sprtopclip;
	vertclip_t*			sprbottomclip;

	// TODO: Make this 32-bit
	vertclip_t*			maskedtexturecol;

} drawseg_t;

// A vissprite_t is a thing
//  that will be drawn during a refresh.
// I.e. a sprite object that is partly visible.
typedef struct vissprite_s
{
	// Doubly linked list.
	struct vissprite_s*	prev;
	struct vissprite_s*	next;

	int32_t				x1;
	int32_t				x2;

	// for line side calculation
	fixed_t				gx;
	fixed_t				gy;

	// global bottom / top for silhouette clipping
	fixed_t				gz;
	fixed_t				gzt;

	// horizontal position of x1
	fixed_t				startfrac;

	fixed_t				scale;

	// negative if flipped
	fixed_t				xiscale;
	fixed_t				xscale;

	fixed_t				texturemid;
	int32_t				patch;

	// for color translation and shadow draw,
	//  maxbright frames as well
	lighttable_t*		colormap;

	int32_t				mobjflags;

} vissprite_t;

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
typedef struct spriteframe_s
{
	// If false use 0 for any position.
	// Note: as eight entries are available,
	//  we might as well insert the same name eight times.
	boolean				rotate;

	// Lump to use for view angles 0-7.
	int16_t				lump[8];

	// Flip bit (1 = flip) to use for view angles 0-7.
	byte				flip[8];

} spriteframe_t;



//
// A sprite definition:
//  a number of animation frames.
//
typedef struct spritedef_s
{
	int32_t				numframes;
	spriteframe_t*		spriteframes;

} spritedef_t;

typedef struct visplane_s
{
	fixed_t				height;
	int32_t				picnum;
	int32_t				lightlevel;
	int32_t				minx;
	int32_t				maxx;
	int32_t				miny;
	int32_t				maxy;
  
	// leave pads for [minx-1]/[maxx+1]
  
	vpindex_t			pad1;
	// Here lies the rub for all
	//  dynamic resize/change of resolution.
	vpindex_t			top[ MAXSCREENWIDTH ];
	vpindex_t			pad2;
	vpindex_t			pad3;
	// See above.
	vpindex_t			bottom[ MAXSCREENWIDTH ];
	vpindex_t			pad4;

} visplane_t;

//
// ClipWallSegment
// Clips the given range of columns
// and includes it in the new clip list.
//
typedef	struct cliprange_s
{
	int32_t				first;
	int32_t				last;
} cliprange_t;

typedef struct wallcontext_s
{
	angle_t				normalangle;
	angle_t				angle1;
	angle_t				angle2;
	angle_t				centerangle;
	rend_fixed_t		offset;
	rend_fixed_t		distance;
	rend_fixed_t		scale;
	rend_fixed_t		scalestep;
	rend_fixed_t		midtexturemid;
	rend_fixed_t		toptexturemid;
	rend_fixed_t		bottomtexturemid;

	lighttable_t**		lights;
	int32_t				lightsindex;

} wallcontext_t;

typedef struct bspcontext_s
{
	seg_t*				curline;
	side_t*				sidedef;
	line_t*				linedef;
	sector_t*			frontsector;
	sector_t*			backsector;

	drawseg_t			drawsegs[MAXDRAWSEGS];
	drawseg_t*			thisdrawseg;

	cliprange_t			solidsegs[MAXSEGS];
	cliprange_t*		solidsegsend;

#if RENDER_PERF_GRAPHING
	uint64_t			storetimetaken;
	uint64_t			solidtimetaken;
	uint64_t			maskedtimetaken;
	uint64_t			findvisplanetimetaken;
	uint64_t			addspritestimetaken;
	uint64_t			addlinestimetaken;
#endif // RENDER_PERF_GRAPHING
} bspcontext_t;

#define PLANE_PIXELLEAP_32			( 32 )
#define PLANE_PIXELLEAP_32_LOG2		( 5 )

#define PLANE_PIXELLEAP_16			( 16 )
#define PLANE_PIXELLEAP_16_LOG2		( 4 )

#define PLANE_PIXELLEAP_8			( 8 )
#define PLANE_PIXELLEAP_8_LOG2		( 3 )

#define PLANE_PIXELLEAP_4			( 4 )
#define PLANE_PIXELLEAP_4_LOG2		( 2 )

typedef enum spantype_e
{
	Span_None,
	Span_Original,
	Span_PolyRaster_Log2_4,
	Span_PolyRaster_Log2_8,
	Span_PolyRaster_Log2_16,
	Span_PolyRaster_Log2_32,
} spantype_t;

typedef struct rastercache_s
{
	lighttable_t**		zlight;
	size_t				sourceoffset;
	fixed_t				height;
	rend_fixed_t		distance;
} rastercache_t;

typedef struct planecontext_s
{
	void*				visplanelookup;

	visplane_t			visplanes[MAXVISPLANES];
	visplane_t*			lastvisplane;
	visplane_t*			floorplane;
	visplane_t*			ceilingplane;

	vertclip_t			openings[MAXOPENINGS];
	vertclip_t*			lastopening;

	vertclip_t			floorclip[ MAXSCREENWIDTH ];
	vertclip_t			ceilingclip[ MAXSCREENWIDTH ];

	// Common renderer values
	lighttable_t**		planezlight;
	int32_t				planezlightindex;
	fixed_t				planeheight;

	// New renderer values
	rastercache_t		raster[ MAXSCREENHEIGHT ];

	// Original renderer values
	fixed_t				basexscale;
	fixed_t				baseyscale;

	int32_t				spanstart[ MAXSCREENHEIGHT ];
	int32_t				spanstop[ MAXSCREENHEIGHT ];

	fixed_t				cachedheight[ MAXSCREENHEIGHT ];
	fixed_t				cacheddistance[ MAXSCREENHEIGHT ];
	fixed_t				cachedxstep[ MAXSCREENHEIGHT ];
	fixed_t				cachedystep[ MAXSCREENHEIGHT ];

#if RENDER_PERF_GRAPHING
	uint64_t			flattimetaken;
#endif // RENDER_PERF_GRAPHING
} planecontext_t;

typedef struct spritecontext_s
{
	int32_t				leftclip;
	int32_t				rightclip;

	// The +1 accounts for the overflow sprite
	vissprite_t			vissprites[MAXVISSPRITES + 1];

	vissprite_t*		nextvissprite;

	vissprite_t			vsprsortedhead;

	vertclip_t*			mfloorclip;
	vertclip_t*			mceilingclip;
	rend_fixed_t		spryscale;
	rend_fixed_t		sprtopscreen;

	lighttable_t**		spritelights;
	
	boolean*			sectorvisited;

#if RENDER_PERF_GRAPHING
	uint64_t			maskedtimetaken;
#endif // RENDER_PERF_GRAPHING
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
	vbuffer_t			output;
	byte*				source;

	lighttable_t*		colormap;
	byte*				translation;

	colfunc_t			colfunc;

	int32_t				x;
	int32_t				yl;
	int32_t				yh;
	rend_fixed_t		scale;
	rend_fixed_t		iscale;
	rend_fixed_t		texturemid;
} colcontext_t;

typedef struct spancontext_s
{
	vbuffer_t			output;
	byte*				source;

	int32_t				y; 
	int32_t				x1; 
	int32_t				x2;

	fixed_t				xfrac; 
	fixed_t				yfrac; 
	fixed_t				xstep; 
	fixed_t				ystep;
} spancontext_t;

typedef struct rendercontext_s
{
	// Setup
	vbuffer_t			buffer;
	int32_t				bufferindex;

	int32_t				begincolumn;
	int32_t				endcolumn;

	uint64_t			starttime;
	uint64_t			endtime;
	uint64_t			timetaken;

#if RENDER_PERF_GRAPHING
	float_t				frametimes[ MAXPROFILETIMES ];
	float_t				walltimes[ MAXPROFILETIMES ];
	float_t				flattimes[ MAXPROFILETIMES ];
	float_t				spritetimes[ MAXPROFILETIMES ];
	float_t				findvisplanetimes[ MAXPROFILETIMES ];
	float_t				addspritestimes[ MAXPROFILETIMES ];
	float_t				addlinestimes[ MAXPROFILETIMES ];
	float_t				everythingelsetimes[ MAXPROFILETIMES ];
	float_t				frameaverage;
	uint64_t			nextframetime;
#endif // RENDER_PERF_GRAPHING

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

typedef enum voidclear_e
{
	Void_NoClear,
	Void_Black,
	Void_Whacky,
	Void_Sky,

	Void_Count,
} voidclear_t;

typedef enum borderstyle_e
{
	Border_Original,
	Border_Interpic,

	Border_Count,
} borderstyle_t;

typedef enum bezelstyle_e
{
	Bezel_Original,
	Bezel_Dithered,

	Bezel_Count,
} bezelstyle_t;

typedef enum fuzzstyle_e
{
	Fuzz_Original,
	Fuzz_Adjusted,
	Fuzz_Heatwave,

	Fuzz_Count,
} fuzzstyle_t;

typedef enum disciconstyle_e
{
	Disk_Off,
	Disk_ByCommandLine,
	Disk_Floppy,
	Disk_CD,

	Disk_Count,
} disciconstyle_t;

#endif
