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
#include "doomstat.h"

#include "d_gamesim.h"

// Some more or less basic data types
// we depend on.
#include "m_fixed.h"

// We rely on the thinker data struct
// to handle sound origins in sectors.
#include "d_think.h"

// SECTORS do store MObjs anyway.
#include "p_sectoraction.h"

#include "i_video.h"

#include "v_patch.h"
// For vbuffer_t
// Should move it out elsewhere
#include "v_video.h"
// hu_textline_t lives in here, but we can't access it thanks to recursive includes :-(
//#include "hu_lib.h"

#if defined( __cplusplus )
extern "C" {
#endif // defined( __cplusplus )

// Silhouette, needed for clipping Segs (mainly)
// and sprites representing things.
#define SIL_NONE				0
#define SIL_BOTTOM				1
#define SIL_TOP					2
#define SIL_BOTH				3

#define MAXVALUESBASE			3200


#define VANILLA_MAXOPENINGS		( 320 * 64 )
#define MAXOPENINGS				( 16384 )
#define VANILLA_MAXDRAWSEGS		( 256 )
#define MAXDRAWSEGS				( 8192 )
#define VANILLA_MAXSEGS			( 32 )
#define MAXSEGS					( 8192 )

typedef uint16_t				vpindex_t;
#define VPINDEX_INVALID			( (vpindex_t)0xFFFF )

typedef pixel_t					lighttable_t;

typedef int32_t					vertclip_t;
#define MASKEDTEXCOL_INVALID	( (vertclip_t)0x7FFFFFFF )

#define VANILLA_MAXVISSPRITES	128
#define MAXVISSPRITES			1024

#define RENDER_PERF_GRAPHING	1
#define MAXPROFILETIMES			( 35 * 4 )

typedef enum dynamicresolution_e
{
	DRS_None,
	DRS_Horizontal	= 0x1,
	DRS_Vertical	= 0x2,
	DRS_Both		= DRS_Horizontal | DRS_Vertical
} dynamicresolution_t;

//
// Render context is required for threaded rendering.
// So everything "global" goes in here. Everything.
//

typedef struct colcontext_s colcontext_t;
typedef struct rendercontext_s rendercontext_t;
typedef struct rasterregion_s rasterregion_t;
typedef struct texturecomposite_s texturecomposite_t;

typedef void (*colfunc_t)( colcontext_t* );
typedef void (*rasterfunc_t)( rendercontext_t*, rasterregion_t* firstregion, texturecomposite_t* texture );

typedef struct texturecomposite_s
{
	byte*			data;
	char			name[8];
	int32_t			namepadding;
	int32_t			size;
	int32_t			width;
	int32_t			height;
	int32_t			pitch;
	int32_t			widthmask;
	int32_t			patchcount;
	rend_fixed_t	renderheight;

	colfunc_t		wallrender;
	colfunc_t		transparentwallrender;
	rasterfunc_t	floorrender;

	int32_t			index;
} texturecomposite_t;

typedef struct sectorinstance_s
{
	// Need to compare these for BSP rejection
	texturecomposite_t*		floortex;
	texturecomposite_t*		ceiltex;
	rend_fixed_t			lightlevel;
	rend_fixed_t			floorlightlevel;
	rend_fixed_t			ceillightlevel;
	rend_fixed_t			flooroffsetx;
	rend_fixed_t			flooroffsety;
	rend_fixed_t			ceiloffsetx;
	rend_fixed_t			ceiloffsety;

	// These are ignored for BSP rejection purposes
	rend_fixed_t			floorheight;
	rend_fixed_t			ceilheight;
	rend_fixed_t			midtexfloor;
	rend_fixed_t			midtexceil;
	int8_t					clipfloor;
	int8_t					snapfloor;
	int8_t					clipceiling;
	int8_t					snapceiling;
	int8_t					activethisframe;
} sectorinstance_t;

typedef struct sideinstance_s
{
	texturecomposite_t*		toptex;
	texturecomposite_t*		midtex;
	texturecomposite_t*		bottomtex;

	rend_fixed_t			coloffset;
	rend_fixed_t			rowoffset;
} sideinstance_t;

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

typedef struct vertex_s
{
	rend_vertex_t		rend;

	fixed_t				x;
	fixed_t				y;

} vertex_t;


typedef struct side_s side_t;
typedef struct line_s line_t;
typedef struct sector_s sector_t;

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

typedef struct lineaction_s lineaction_t;

struct line_s
{
	int32_t				index;

	// Vertices, from v1 to v2.
	vertex_t*			v1;
	vertex_t*			v2;

	// Precalculated v2 - v1 for side checking.
	fixed_t				dx;
	fixed_t				dy;
	angle_t				angle;

	// Animation related.
	int16_t				flags;
	int16_t				special;
	int16_t				tag;

	lineaction_t*		action;

	// Visual appearance: SideDefs.
	//  sidenum[1] will be -1 if one sided
	int32_t				sidenum[2]; // originally int16_t

	// Neat. Another bounding box, for the extent
	//  of the LineDef.
	fixed_t				bbox[4];

	// To aid move clipping.
	slopetype_t			slopetype;

	side_t*				frontside;
	sector_t*			frontsector;

	side_t*				backside;
	sector_t*			backsector;

	// if == validcount, already checked
	int32_t				validcount;

	// thinker_t for reversable actions
	//void*				specialdata;

	// Boom additions
	byte*				transparencymap;
	lighttable_t*		topcolormap;
	lighttable_t*		bottomcolormap;
	lighttable_t*		midcolormap;
	fixed_t				scrollratex;
	fixed_t				scrollratey;

#if defined( __cplusplus )
	constexpr bool Blocking()			const { return flags & ML_BLOCKING; }
	constexpr bool BlockMonsters()		const { return flags & ML_BLOCKMONSTERS; }
	constexpr bool TwoSided()			const { return flags & ML_TWOSIDED; }
	constexpr bool TopUnpegged()		const { return flags & ML_DONTPEGTOP; }
	constexpr bool BottomUnpegged()		const { return flags & ML_DONTPEGBOTTOM; }
	constexpr bool AutomapSecret()		const { return flags & ML_SECRET; }
	constexpr bool BlocksSound()		const { return flags & ML_SOUNDBLOCK; }
	constexpr bool AutomapInvisible()	const { return flags & ML_DONTDRAW; }
	constexpr bool AutomapSeen()		const { return flags & ML_MAPPED; } // This probably needs to be atomic bool
	constexpr bool Passthrough()		const { return flags & ML_BOOM_PASSTHROUGH; }
#endif
};

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
struct sector_s
{
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
	mobj_t*				nosectorthinglist;

	// thinker_t for reversable actions
	void*				specialdata;
	void*				floorspecialdata;
	void*				ceilingspecialdata;

	int32_t				linecount;
	line_t**			lines;	// [linecount] size

	// Interpolator additions
	uint64_t			lastactivetic;
	int32_t				snapfloor;
	int32_t				snapceiling;

	// Boom additions
	fixed_t				friction;
	fixed_t				frictionpercent;

	sector_t*			floorlightsec;
	sector_t*			ceilinglightsec;

	side_t*				skyline;
	fixed_t				skyxscale;

	line_t*				transferline;

	fixed_t				flooroffsetx;
	fixed_t				flooroffsety;
	fixed_t				ceiloffsetx;
	fixed_t				ceiloffsety;

#if defined( __cplusplus )
	INLINE void*& Special()				{ return specialdata; }
	INLINE void*& FloorSpecial()		{ return sim.separate_floor_ceiling_lights ? floorspecialdata : specialdata; }
	INLINE void*& CeilingSpecial()		{ return sim.separate_floor_ceiling_lights ? ceilingspecialdata : specialdata; }

	INLINE fixed_t Friction()			{ return sim.sector_movement_modifiers && ( special & SectorFriction_Mask ) == SectorFriction_Yes ? friction : FRICTION; }
	INLINE fixed_t FrictionPercent()	{ return sim.sector_movement_modifiers && ( special & SectorFriction_Mask ) == SectorFriction_Yes ? frictionpercent : IntToFixed( 1 ); }

	constexpr fixed_t FloorEffectHeight() { return transferline ? transferline->frontsector->floorheight : floorheight; }

	INLINE fixed_t FrictionMultiplier()			{ return frictionmultipliers[ FrictionPercent() >> 13 ]; }
	INLINE fixed_t MonsterFrictionMultiplier()	{ return frictionmultipliers[ FrictionPercent() >> 13 ] << 5; }

private:
	static constexpr int32_t frictionmultipliers[] =
	{
		32,		// 0.0
		64,
		96,
		128,
		256,		// 0.5
		512,
		1024,
		1536,
		2048,		// 1.0
		1792,
		1536,
		1024,
		512,		// 1.5
		256,
		128,
		64,
		32,			// 2.0
	};

#endif // defined( __cplusplus )
};


//
// The SideDef.
//

struct side_s
{
	int32_t				index;

	// add this to the calculated texture column
	fixed_t				textureoffset;

	// add this to the calculated texture top
	fixed_t				rowoffset;

	// Texture indices.
	// We do not maintain names here. 
	int16_t				toptexture;
	int16_t				bottomtexture;
	int16_t				midtexture;

	lumpindex_t			toptextureindex;
	lumpindex_t			bottomtextureindex;
	lumpindex_t			midtextureindex;

	// Sector the SideDef is facing.
	sector_t*			sector;

};



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

typedef struct rasterregion_s rasterregion_t;

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
	rend_fixed_t		gx;
	rend_fixed_t		gy;

	// global bottom / top for silhouette clipping
	rend_fixed_t		gz;
	rend_fixed_t		gzt;

	sectorinstance_t*	sector;

	// horizontal position of x1
	rend_fixed_t		startfrac;

	rend_fixed_t		scale;
	rend_fixed_t		iscale;

	// negative if flipped
	rend_fixed_t		xiscale;
	rend_fixed_t		xscale;

	rend_fixed_t		texturemid;
	int32_t				patch;

	// for color translation and shadow draw,
	//  maxbright frames as well
	lighttable_t*		colormap;
	byte*				tranmap;

	int32_t				mobjflags;
	int32_t				mobjflags2;

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
	doombool			rotate;

	// Lump to use for view angles 0-7.
	int32_t				lump[8];

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

	int32_t*			lightsoffset;
	int32_t				lightsindex;

} wallcontext_t;

typedef struct bspcontext_s
{
	seg_t*				curline;
	side_t*				sidedef;
	sideinstance_t*		sideinst;
	line_t*				linedef;
	sector_t*			frontsector;
	sectorinstance_t*	frontsectorinst;
	sector_t*			backsector;
	sectorinstance_t*	backsectorinst;

	drawseg_t*			drawsegs;
	drawseg_t*			thisdrawseg;
	size_t				maxdrawsegs;

	cliprange_t*		solidsegs;
	cliprange_t*		solidsegsend;
	size_t				maxsolidsegs;

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
	lighttable_t*		colormap;
	size_t				sourceoffset;
	rend_fixed_t		distance;
} rastercache_t;

typedef struct rasterline_s
{
	vpindex_t top;
	vpindex_t bottom;
} rasterline_t;

struct rasterregion_s
{
	rasterline_t*		lines;

	rend_fixed_t		height;
	rend_fixed_t		xoffset;
	rend_fixed_t		yoffset;

	int32_t				lightlevel;
	int32_t				padding;

	int16_t				minx;
	int16_t				maxx;
	int16_t				miny;
	int16_t				maxy;

	rasterregion_t*		nextregion;
};

typedef struct planecontext_s
{
	vbuffer_t			output;
	byte*				source;

	rend_fixed_t		viewx;
	rend_fixed_t		viewy;
	angle_t				viewangle;

	int32_t				spantype;

	rasterregion_t*		floorregion;
	rasterregion_t*		ceilingregion;

	vertclip_t*			openings;
	vertclip_t*			lastopening;
	size_t				openingscount;
	size_t				maxopenings;

	vertclip_t*			floorclip;
	vertclip_t*			ceilingclip;

	// Common renderer values
	int32_t*			planezlightoffset;
	int32_t				planezlightindex;
	rend_fixed_t		planeheight;

	rastercache_t*		raster;

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

	vertclip_t*			clipbot;
	vertclip_t*			cliptop;

	vertclip_t*			mfloorclip;
	vertclip_t*			mceilingclip;
	rend_fixed_t		spryscale;
	rend_fixed_t		spryiscale;
	rend_fixed_t		sprtopscreen;

	int32_t*			spritelightoffsets;
	
	doombool*			sectorvisited;

#if RENDER_PERF_GRAPHING
	uint64_t			maskedtimetaken;
#endif // RENDER_PERF_GRAPHING
} spritecontext_t;

typedef struct colcontext_s
{
	vbuffer_t			output;
	pixel_t*			fuzzworkingbuffer;
	byte*				source;
	rend_fixed_t		sourceheight;

	lighttable_t*		colormap;
	lighttable_t*		fuzzlightmap;
	lighttable_t*		fuzzdarkmap;
	byte*				translation;
	byte*				transparency;

	colfunc_t			colfunc;

	int32_t				x;
	int32_t				yl;
	int32_t				yh;
	rend_fixed_t		iscale;
	rend_fixed_t		texturemid;
} colcontext_t;

typedef struct player_s player_t;

typedef enum transferzone_e
{
	transfer_normalspace = 0,
	transfer_ceilingspace,
	transfer_floorspace,
} transferzone_t;

typedef struct viewpoint_s
{
	player_t*			player;
	rend_fixed_t		x;
	rend_fixed_t		y;
	rend_fixed_t		z;
	rend_fixed_t		lerp;
	rend_fixed_t		sin;
	rend_fixed_t		cos;
	lighttable_t*		colormaps;
	transferzone_t		transferzone;
	angle_t				angle;
} viewpoint_t;

typedef struct rendercontext_s
{
	// Setup
	viewpoint_t			viewpoint;
	vbuffer_t			buffer;
	vbuffer_t			viewbuffer;
	int32_t				bufferindex;

	int32_t				begincolumn;
	int32_t				endcolumn;

	pixel_t*			fuzzworkingbuffer;

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

} rendercontext_t;

typedef struct drsdata_s
{
	int32_t				generated;
	int32_t				frame_width;
	int32_t				frame_height;
	int32_t				frame_adjusted_width;
	double_t			percentage;
	rend_fixed_t		frame_adjusted_light_mul;
	int32_t				frame_blocks; // 32 pixel wide blocks, just like vanilla

	angle_t				clipangle;
	int32_t				centerx;
	int32_t				centery;
	rend_fixed_t		centerxfrac;
	rend_fixed_t		centeryfrac;
	rend_fixed_t		xprojection;
	rend_fixed_t		yprojection;
	int32_t				viewwidth;
	int32_t				scaledviewwidth;
	int32_t				viewheight;
	int32_t				viewwindowx;
	int32_t				viewwindowy;
	rend_fixed_t		pspritescalex;
	rend_fixed_t		pspriteiscalex;
	rend_fixed_t		pspritescaley;
	rend_fixed_t		pspriteiscaley;

	rend_fixed_t		skyscaley;
	rend_fixed_t		skyiscaley;

	int32_t*			viewangletox;
	angle_t*			xtoviewangle;
	int32_t*			scalelightindex;
	int32_t*			scalelightoffset;
	int32_t*			zlightoffset;
	int32_t*			zlightindex;
	vertclip_t*			negonearray;
	vertclip_t*			screenheightarray;
	rend_fixed_t*		yslope;
	rend_fixed_t*		distscale;

} drsdata_t;

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

typedef enum windowclose_e
{
	WindowClose_Always,
	WindowClose_Ask,
} windowclose_t;

#if defined( __cplusplus )
}

namespace constants
{
	static constexpr double_t pi = 3.1415926535897932384626433832795;
	static constexpr double_t degtorad = pi / 180.0;
	static constexpr double_t radtodeg = 180.0 / pi;
}

struct RegionRange
{
	struct iterator
	{
		constexpr rasterregion_t* operator*()				{ return _val; }
		constexpr bool operator!=( const iterator& it )		{ return _val != it._val; }
		constexpr iterator& operator++()
		{
			do
			{
				_val = _val->nextregion;
				if( _val == nullptr ) break;
				if( _val->minx <= _val->maxx ) break;
			} while( true );

			return *this;
		}

		rasterregion_t* _val;
	};

	RegionRange( rasterregion_t* r )
		: _begin { r }
		, _end { nullptr }
	{
	}

	constexpr iterator begin() { return _begin; }
	constexpr iterator end() { return _end; }

	iterator _begin;
	iterator _end;
};

// Mobj iterators that come with built-in fixes for deleted list nodes
struct mobjsector
{
	using basetype_t = sector_t;

	static constexpr mobj_t* First( sector_t* sector ) { return sector->thinglist; }
	static constexpr mobj_t* Next( mobj_t* mobj ) { return mobj->snext; }
};

struct mobjnosector
{
	using basetype_t = sector_t;

	static constexpr mobj_t* First( sector_t* sector ) { return sector->nosectorthinglist; }
	static constexpr mobj_t* Next( mobj_t* mobj ) { return mobj->nosectornext; }
};

struct mobjblockmap
{
	using basetype_t = mobj_t;

	static constexpr mobj_t* First( mobj_t* mobj ) { return mobj; }
	static constexpr mobj_t* Next( mobj_t* mobj ) { return mobj->bnext; }
};

template< typename getter >
class mobjrange
{
public:
	using basetype_t = typename getter::basetype_t;

	struct iterator
	{
		constexpr mobj_t* operator*() noexcept				{ return val; }
		constexpr bool operator!=( const iterator& rhs ) 	{ return val != rhs.val; }
		constexpr iterator& operator++()					{ val = next; if( next ) next = getter::Next( val ); return *this; }

		mobj_t* val;
		mobj_t* next;
	};

	constexpr mobjrange( basetype_t* base )
		: basetype( base )
	{
	}

	constexpr iterator begin()		{ mobj_t* first = getter::First( basetype ); return { first, first ? getter::Next( first ) : nullptr }; }
	constexpr iterator end()		{ return {}; }

private:
	basetype_t* basetype;
};

constexpr auto SectorMobjs( sector_t* sector )		{ return mobjrange< mobjsector >( sector ); }
constexpr auto SectorMobjs( sector_t& sector )		{ return mobjrange< mobjsector >( &sector ); }
constexpr auto NoSectorMobjs( sector_t* sector )	{ return mobjrange< mobjnosector >( sector ); }
constexpr auto NoSectorMobjs( sector_t& sector )	{ return mobjrange< mobjnosector >( &sector ); }
constexpr auto BlockmapMobjs( mobj_t* mobj )		{ return mobjrange< mobjblockmap >( mobj ); }

#endif // defined( __cplusplus )

#endif
