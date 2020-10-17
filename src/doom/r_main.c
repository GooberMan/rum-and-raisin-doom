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
//	Rendering main loop and setup functions,
//	 utility functions (BSP, geometry, trigonometry).
//	See tables.c, too.
//





#include <stdlib.h>
#include <math.h>


#include "doomdef.h"
#include "d_loop.h"

#include "m_bbox.h"
#include "m_menu.h"

#include "r_local.h"
#include "r_sky.h"

// Need ST_HEIGHT
#include "st_stuff.h"

#include "tables.h"

#define SBARHEIGHT		( ( ( (int64_t)( ST_HEIGHT << FRACBITS ) * (int64_t)V_HEIGHTMULTIPLIER ) >> FRACBITS ) >> FRACBITS )




// Fineangles in the SCREENWIDTH wide window.
// If we define this as FINEANGLES / 4 then we get auto 90 degrees everywhere
#define FIELDOFVIEW				( FINEANGLES >> 2 )	
#define RENDERFIELDOFVIEW		( RENDERFINEANGLES >> 2 )	

int			viewangleoffset;

// increment every time a check is made
int			validcount = 1;		


lighttable_t*		fixedcolormap;
int32_t				fixedcolormapindex;
extern lighttable_t**	walllights;
extern int32_t			walllightsindex;

int			centerx;
int			centery;

fixed_t			centerxfrac;
fixed_t			centeryfrac;
fixed_t			projection;

// just for profiling purposes
int			framecount;	

int			sscount;
int			linecount;
int			loopcount;

fixed_t			viewx;
fixed_t			viewy;
fixed_t			viewz;

angle_t			viewangle;

fixed_t			viewcos;
fixed_t			viewsin;

player_t*		viewplayer;

// 0 = high, 1 = low
int			detailshift;	

//
// precalculated math tables
//
angle_t			clipangle;

// The viewangletox[viewangle + FINEANGLES/4] lookup
// maps the visible view angles to screen X coordinates,
// flattening the arc to a flat projection plane.
// There will be many angles mapped to the same X. 

// TODO: Convert everything to RENDERFINEANGLES
int			viewangletox[RENDERFINEANGLES/2];

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.
angle_t			xtoviewangle[SCREENWIDTH+1];

lighttable_t*		scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t*		scalelightfixed[MAXLIGHTSCALE];
int32_t				scalelightindex[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t*		zlight[LIGHTLEVELS][MAXLIGHTZ];
int32_t				zlightindex[LIGHTLEVELS][MAXLIGHTZ];
colfunc_t			colfuncs[ COLFUNC_COUNT ];

// bumped light from gun blasts
int					extralight;


colfunc_t			colfunc;
colfunc_t			fuzzcolfunc;
colfunc_t			transcolfunc;

spanfunc_t			spanfunc;



//
// R_AddPointToBox
// Expand a given bbox
// so that it encloses a given point.
//
void
R_AddPointToBox
( int		x,
  int		y,
  fixed_t*	box )
{
    if (x< box[BOXLEFT])
	box[BOXLEFT] = x;
    if (x> box[BOXRIGHT])
	box[BOXRIGHT] = x;
    if (y< box[BOXBOTTOM])
	box[BOXBOTTOM] = y;
    if (y> box[BOXTOP])
	box[BOXTOP] = y;
}


//
// R_PointOnSide
// Traverse BSP (sub) tree,
//  check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
int
R_PointOnSide
( fixed_t	x,
  fixed_t	y,
  node_t*	node )
{
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	left;
    fixed_t	right;
	
    if (!node->dx)
    {
	if (x <= node->x)
	    return node->dy > 0;
	
	return node->dy < 0;
    }
    if (!node->dy)
    {
	if (y <= node->y)
	    return node->dx < 0;
	
	return node->dx > 0;
    }
	
    dx = (x - node->x);
    dy = (y - node->y);
	
    // Try to quickly decide by looking at sign bits.
    if ( (node->dy ^ node->dx ^ dx ^ dy)&0x80000000 )
    {
	if  ( (node->dy ^ dx) & 0x80000000 )
	{
	    // (left is negative)
	    return 1;
	}
	return 0;
    }

    left = FixedMul ( node->dy>>FRACBITS , dx );
    right = FixedMul ( dy , node->dx>>FRACBITS );
	
    if (right < left)
    {
	// front side
	return 0;
    }
    // back side
    return 1;			
}


int
R_PointOnSegSide
( fixed_t	x,
  fixed_t	y,
  seg_t*	line )
{
    fixed_t	lx;
    fixed_t	ly;
    fixed_t	ldx;
    fixed_t	ldy;
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	left;
    fixed_t	right;
	
    lx = line->v1->x;
    ly = line->v1->y;
	
    ldx = line->v2->x - lx;
    ldy = line->v2->y - ly;
	
    if (!ldx)
    {
	if (x <= lx)
	    return ldy > 0;
	
	return ldy < 0;
    }
    if (!ldy)
    {
	if (y <= ly)
	    return ldx < 0;
	
	return ldx > 0;
    }
	
    dx = (x - lx);
    dy = (y - ly);
	
    // Try to quickly decide by looking at sign bits.
    if ( (ldy ^ ldx ^ dx ^ dy)&0x80000000 )
    {
	if  ( (ldy ^ dx) & 0x80000000 )
	{
	    // (left is negative)
	    return 1;
	}
	return 0;
    }

    left = FixedMul ( ldy>>FRACBITS , dx );
    right = FixedMul ( dy , ldx>>FRACBITS );
	
    if (right < left)
    {
	// front side
	return 0;
    }
    // back side
    return 1;			
}


//
// R_PointToAngle
// To get a global angle from cartesian coordinates,
//  the coordinates are flipped until they are in
//  the first octant of the coordinate system, then
//  the y (<=x) is scaled and divided by x to get a
//  tangent (slope) value which is looked up in the
//  tantoangle[] table.

//




angle_t
R_PointToAngle
( fixed_t	x,
  fixed_t	y )
{	
    x -= viewx;
    y -= viewy;
    
    if ( (!x) && (!y) )
	return 0;

    if (x>= 0)
    {
	// x >=0
	if (y>= 0)
	{
	    // y>= 0

	    if (x>y)
	    {
		// octant 0
		return rendertantoangle[ SlopeDiv_Render(y,x) ];
	    }
	    else
	    {
		// octant 1
		return ANG90-1-rendertantoangle[ SlopeDiv_Render(x,y) ];
	    }
	}
	else
	{
	    // y<0
	    y = -y;

	    if (x>y)
	    {
		// octant 8
		return -rendertantoangle[ SlopeDiv_Render(y,x) ];
	    }
	    else
	    {
		// octant 7
		return ANG270+rendertantoangle[ SlopeDiv_Render(x,y) ];
	    }
	}
    }
    else
    {
	// x<0
	x = -x;

	if (y>= 0)
	{
	    // y>= 0
	    if (x>y)
	    {
		// octant 3
		return ANG180-1-rendertantoangle[ SlopeDiv_Render(y,x) ];
	    }
	    else
	    {
		// octant 2
		return ANG90+ rendertantoangle[ SlopeDiv_Render(x,y) ];
	    }
	}
	else
	{
	    // y<0
	    y = -y;

	    if (x>y)
	    {
		// octant 4
		return ANG180+rendertantoangle[ SlopeDiv_Render(y,x) ];
	    }
	    else
	    {
		 // octant 5
		return ANG270-1-rendertantoangle[ SlopeDiv_Render(x,y) ];
	    }
	}
    }
    return 0;
}

// Called by playsim. Let's bring it back to normal tantoangle
angle_t
R_PointToAngle2
( fixed_t	x1,
  fixed_t	y1,
  fixed_t	x2,
  fixed_t	y2 )
{	
    int32_t x = x2 - x1;
    int32_t y = y2 - y1;
    
    if ( (!x) && (!y) )
		return 0;

    if (x>= 0)
    {
		// x >=0
		if (y>= 0)
		{
			// y>= 0

			if (x>y)
			{
				// octant 0
				return tantoangle[ SlopeDiv_Playsim(y,x)];
			}
			else
			{
				// octant 1
				return ANG90-1-tantoangle[ SlopeDiv_Playsim(x,y)];
			}
		}
		else
		{
			// y<0
			y = -y;

			if (x>y)
			{
				// octant 8
				return -tantoangle[SlopeDiv_Playsim(y,x)];
			}
			else
			{
				// octant 7
				return ANG270+tantoangle[ SlopeDiv_Playsim(x,y)];
			}
		}
	}
	else
	{
		// x<0
		x = -x;

		if (y>= 0)
		{
			// y>= 0
			if (x>y)
			{
				// octant 3
				return ANG180-1-tantoangle[ SlopeDiv_Playsim(y,x)];
			}
			else
			{
				// octant 2
				return ANG90+ tantoangle[ SlopeDiv_Playsim(x,y)];
			}
		}
		else
		{
			// y<0
			y = -y;

			if (x>y)
			{
				// octant 4
				return ANG180+tantoangle[ SlopeDiv_Playsim(y,x)];
			}
			else
			{
				 // octant 5
				return ANG270-1-tantoangle[ SlopeDiv_Playsim(x,y)];
			}
		}
	}
	return 0;
}


fixed_t
R_PointToDist
( fixed_t	x,
  fixed_t	y )
{
    int		angle;
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	temp;
    fixed_t	dist;
    fixed_t     frac;
	
    dx = abs(x - viewx);
    dy = abs(y - viewy);
	
    if (dy>dx)
    {
	temp = dx;
	dx = dy;
	dy = temp;
    }

    // Fix crashes in udm1.wad

    if (dx != 0)
    {
        frac = FixedDiv(dy, dx);
    }
    else
    {
	frac = 0;
    }
	
    angle = (rendertantoangle[frac>>RENDERDBITS]+ANG90) >> RENDERANGLETOFINESHIFT;

    // use as cosine
    dist = FixedDiv (dx, renderfinesine[angle] );	
	
    return dist;
}


#define PI acos(-1.0)


//
// R_InitPointToAngle
//
void R_InitPointToAngle (void)
{
    // UNUSED - now getting from tables.c
#if RENDERSLOPEQUALITYSHIFT > 0
	int32_t		i;
	angle_t		t;
	float_t		f;
	//
	// slope (tangent) to angle lookup
	//
	for (i=0 ; i<=RENDERSLOPERANGE ; i++)
	{
		f = atan( (float)i/RENDERSLOPERANGE )/(PI*2);
		t = 0xffffffff*f;
		rendertantoangle[i] = t;
	}
#endif
}


//
// R_ScaleFromGlobalAngle
// Returns the texture mapping scale
//  for the current line (horizontal span)
//  at the given angle.
// rw_distance must be calculated first.
//

// R_ScaleFromGlobalAngle is a function of screenwidth
// Defining the original maximum as a function of your
// new width is the correct way to go about things.
// 1/5th of the screenwidth is the way to go.
#define MAXSCALE ( 64 * ( SCREENWIDTH / 320 ) )
#define MAXSCALE_FIXED ( MAXSCALE << FRACBITS )

fixed_t R_ScaleFromGlobalAngle (angle_t visangle, fixed_t distance)
{
    fixed_t		scale;
    angle_t		anglea;
    angle_t		angleb;
    int32_t		sinea;
    int32_t		sineb;
    fixed_t		num;
    int32_t		den;

	scale = MAXSCALE_FIXED;

    anglea = ANG90 + (visangle-viewangle);
    angleb = ANG90 + (visangle-rw_normalangle);

    // both sines are allways positive
    sinea = renderfinesine[anglea>>RENDERANGLETOFINESHIFT];
    sineb = renderfinesine[angleb>>RENDERANGLETOFINESHIFT];
    num = FixedMul(projection,sineb)<<detailshift;
    den = FixedMul(distance,sinea);

    if (den >= 0 && den > num>>FRACBITS)
    {
		scale = FixedDiv (num, den);

		if (scale > MAXSCALE_FIXED)
			scale = MAXSCALE_FIXED;
		else if (scale < 256)
			scale = 256;
    }
	
    return scale;
}



//
// R_InitTables
//

void R_InitTables (void)
{
#if RENDERQUALITYSHIFT > 0
    int32_t		i;
    float_t		a;
    float_t		fv;
    int32_t		t;
    
    // viewangle tangent table
    for (i=0 ; i< RENDERFINETANGENTCOUNT ; i++)
    {
		a = (i-RENDERFINEANGLES/4+0.5)*PI*2/RENDERFINEANGLES;
		fv = FRACUNIT*tan (a);
		t = fv;
		renderfinetangent[i] = t;
    }
    
    // finesine table
    for (i=0 ; i < RENDERFINESINECOUNT ; i++)
    {
		// OPTIMIZE: mirror...
		a = (i+0.5)*PI*2/RENDERFINEANGLES;
		t = FRACUNIT*sin (a);
		renderfinesine[i] = t;
    }
#endif // RENDERQUALITYSHIFT > 0
}



//
// R_InitTextureMapping
//
void R_InitTextureMapping (void)
{
    int			i;
    int			x;
    int			t;
    fixed_t		focallength;
    
    // Use tangent table to generate viewangletox:
    //  viewangletox will give the next greatest x
    //  after the view angle.
    //
    // Calc focallength
    //  so FIELDOFVIEW angles covers SCREENWIDTH.
    focallength = FixedDiv (centerxfrac,
			    renderfinetangent[RENDERFINEANGLES/4+RENDERFIELDOFVIEW/2] );
	
    for (i=0 ; i<RENDERFINEANGLES/2 ; i++)
    {
		if (renderfinetangent[i] > FRACUNIT*2)
			t = -1;
		else if (renderfinetangent[i] < -FRACUNIT*2)
			t = viewwidth+1;
		else
		{
			t = FixedMul (renderfinetangent[i], focallength);
			t = (centerxfrac - t+FRACUNIT-1)>>FRACBITS;

			if (t < -1)
			t = -1;
			else if (t>viewwidth+1)
			t = viewwidth+1;
		}
		viewangletox[i] = t;
    }
    
    // Scan viewangletox[] to generate xtoviewangle[]:
    //  xtoviewangle will give the smallest view angle
    //  that maps to x.	
    for (x=0;x<=viewwidth;x++)
    {
	i = 0;
	while (viewangletox[i]>x)
	    i++;
	xtoviewangle[x] = (i<<RENDERANGLETOFINESHIFT)-ANG90;
    }

    // Take out the fencepost cases from viewangletox.
    for (i=0 ; i<FINEANGLES/2 ; i++)
    {
		if (viewangletox[i] == -1)
			viewangletox[i] = 0;
		else if (viewangletox[i] == viewwidth+1)
			viewangletox[i]  = viewwidth;
	}
	
	clipangle = xtoviewangle[0];
}



//
// R_InitLightTables
// Only inits the zlight table,
//  because the scalelight table changes with view size.
//
#define DISTMAP		2

void R_InitLightTables (void)
{
	int		i;
	int		j;
	int		level;
	int		startmap; 	
	int		scale;
    
	// Calculate the light levels to use
	//  for each level / distance combination.
	for (i=0 ; i< LIGHTLEVELS ; i++)
	{
		startmap = ((LIGHTLEVELS-1-i)*2)*NUMLIGHTCOLORMAPS/LIGHTLEVELS;
		for (j=0 ; j<MAXLIGHTZ ; j++)
		{
			scale = FixedDiv ((SCREENWIDTH/2*FRACUNIT), (j+1)<<LIGHTZSHIFT);
			scale >>= LIGHTSCALESHIFT;
#if SCREENWIDTH != 320
			scale = FixedDiv( scale << FRACBITS, LIGHTSCALEDIVIDE ) >> FRACBITS;
#endif // SCREENWIDTH != 320

			level = startmap - scale/DISTMAP;

			if (level < 0)
				level = 0;

			if (level >= NUMLIGHTCOLORMAPS)
				level = NUMLIGHTCOLORMAPS-1;

			zlightindex[i][j] = level;
			zlight[i][j] = colormaps + level*256;
		}
	}
}

byte detailmaps[16][256];

#define HAX 0
void R_InitColFuncs( void )
{
#if R_DRAWCOLUMN_SIMDOPTIMISED
	colfuncs[ 0 ] = &R_DrawColumn_OneSample;

#if HAX
	colfuncs[ 1 ] = &R_DrawColumn_OneSample;
	colfuncs[ 2 ] = &R_DrawColumn_OneSample;
	colfuncs[ 3 ] = &R_DrawColumn_OneSample;
	colfuncs[ 4 ] = &R_DrawColumn_OneSample;
	colfuncs[ 5 ] = &R_DrawColumn_OneSample;
	colfuncs[ 6 ] = &R_DrawColumn_OneSample;
	colfuncs[ 7 ] = &R_DrawColumn_OneSample;
	colfuncs[ 8 ] = &R_DrawColumn_OneSample;
	colfuncs[ 9 ] = &R_DrawColumn_OneSample;
	colfuncs[ 10 ] = &R_DrawColumn_OneSample;
	colfuncs[ 11 ] = &R_DrawColumn_OneSample;
	colfuncs[ 12 ] = &R_DrawColumn_OneSample;
	colfuncs[ 13 ] = &R_DrawColumn_OneSample;
	colfuncs[ 14 ] = &R_DrawColumn_OneSample;
	colfuncs[ 15 ] = &R_DrawColumn;
#else //!HAX
	colfuncs[ 1 ] = &R_DrawColumn;
	colfuncs[ 2 ] = &R_DrawColumn;
	colfuncs[ 3 ] = &R_DrawColumn;
	colfuncs[ 4 ] = &R_DrawColumn;
	colfuncs[ 5 ] = &R_DrawColumn;
	colfuncs[ 6 ] = &R_DrawColumn;
	colfuncs[ 7 ] = &R_DrawColumn;
	colfuncs[ 8 ] = &R_DrawColumn;
	colfuncs[ 9 ] = &R_DrawColumn;
	colfuncs[ 10 ] = &R_DrawColumn;
	colfuncs[ 11 ] = &R_DrawColumn;
	colfuncs[ 12 ] = &R_DrawColumn;
	colfuncs[ 13 ] = &R_DrawColumn;
	colfuncs[ 14 ] = &R_DrawColumn;
	colfuncs[ 15 ] = &R_DrawColumn;
#endif
#endif //R_DRAWCOLUMN_SIMDOPTIMISED

	int32_t currexpand = COLFUNC_PIXELEXPANDERS;

	while( currexpand > 0 )
	{
		--currexpand;
#if !R_DRAWCOLUMN_SIMDOPTIMISED
		colfuncs[ currexpand ] = &R_DrawColumn;
#endif // R_DRAWCOLUMN_SIMDOPTIMISED
		colfuncs[ COLFUNC_NUM + currexpand ] = &R_DrawColumnLow;
	}

	memset( detailmaps[ 0 ], 247, 256 );
	memset( detailmaps[ 1 ], 110, 256 );
	memset( detailmaps[ 2 ], 108, 256 );
	memset( detailmaps[ 3 ], 106, 256 );
	memset( detailmaps[ 4 ], 104, 256 );
	memset( detailmaps[ 5 ], 102, 256 );
	memset( detailmaps[ 6 ], 100, 256 );
	memset( detailmaps[ 7 ], 98, 256 );
	memset( detailmaps[ 8 ], 96, 256 );
	memset( detailmaps[ 9 ], 94, 256 );
	memset( detailmaps[ 10 ], 92, 256 );
	memset( detailmaps[ 11 ], 90, 256 );
	memset( detailmaps[ 12 ], 88, 256 );
	memset( detailmaps[ 13 ], 86, 256 );
	memset( detailmaps[ 14 ], 84, 256 );
	memset( detailmaps[ 15 ], 82, 256 );

	colfuncs[ COLFUNC_FUZZINDEX ] = colfuncs[ COLFUNC_NUM + COLFUNC_FUZZINDEX ] = &R_DrawFuzzColumn;
	colfuncs[ COLFUNC_TRANSLATEINDEX ] = colfuncs[ COLFUNC_NUM + COLFUNC_TRANSLATEINDEX ] = &R_DrawTranslatedColumn;
}


//
// R_SetViewSize
// Do not really change anything here,
//  because it might be in the middle of a refresh.
// The change will take effect next refresh.
//
boolean		setsizeneeded;
int		setblocks;
int		setdetail;


void
R_SetViewSize
( int		blocks,
  int		detail )
{
    setsizeneeded = true;
    setblocks = blocks;
    setdetail = detail;
}

typedef enum detail_e
{
	DT_NATIVE,
	DT_CRISPY,
	DT_ORIGINALHIGH,
	DT_ORIGINALLOW,
	DT_VIRTUAL,

	DT_COUNT
} detail_t;

//
// R_ExecuteSetViewSize
//
void R_ExecuteSetViewSize (void)
{
    fixed_t	cosadj;
    fixed_t	dy;
    int		i;
    int		j;
    int		level;
    int		startmap; 	
	int		colfuncbase;

    setsizeneeded = false;

	//switch( setdetail )
	//{
	//case DT_NATIVE:
	//case DT_VIRTUAL:
	//	scaledviewwidth = SCREENWIDTH;
	//	viewheight = SCREENHEIGHT;
	//	detailshift = 0;
	//	break;
	//case DT_CRISPY:
	//	scaledviewwidth = 640;
	//	viewheight = 400;
	//	detailshift = 0;
	//case DT_ORIGINALHIGH:
	//	scaledviewwidth = 320;
	//	viewheight = 200;
	//	detailshift = 1;
	//	break;
	//case DT_ORIGINALLOW:
	//	scaledviewwidth = 160;
	//	viewheight = 200;
	//	detailshift = 1;
	//	break;
	//}
	//
    //viewwidth = scaledviewwidth>>detailshift;
	//
    //if (setblocks < 11)
    //{
	//	scaledviewwidth = setblocks*viewwidth/10;
	//	viewheight = (setblocks*(SCREENHEIGHT-SBARHEIGHT)/10)&~7;
    //}

   if (setblocks == 11)
    {
	scaledviewwidth = SCREENWIDTH;
	viewheight = SCREENHEIGHT;
    }
    else
    {
	scaledviewwidth = setblocks*SCREENWIDTH/10;
	viewheight = (setblocks*(SCREENHEIGHT-SBARHEIGHT)/10)&~7;
    }
    
    detailshift = setdetail;
    viewwidth = scaledviewwidth>>detailshift;
    
    centery = viewheight/2;
    centerx = viewwidth/2;
    centerxfrac = centerx<<FRACBITS;
    centeryfrac = centery<<FRACBITS;
    projection = centerxfrac;

	colfuncbase = COLFUNC_NUM * ( detailshift );
	colfunc = colfuncs[ colfuncbase + 15 ];
	fuzzcolfunc = colfuncs[ colfuncbase + COLFUNC_FUZZINDEX ];
	transcolfunc = colfuncs[ colfuncbase + COLFUNC_TRANSLATEINDEX ];

    if (!detailshift)
    {
		spanfunc = R_DrawSpan;
    }
    else
    {
		spanfunc = R_DrawSpanLow;
    }

    R_InitBuffer (scaledviewwidth, viewheight);
	
    R_InitTextureMapping ();
    
    // psprite scales
    pspritescale = FixedMul( FRACUNIT*viewwidth/SCREENWIDTH, V_WIDTHMULTIPLIER );
    pspriteiscale = FixedDiv( FRACUNIT*SCREENWIDTH/viewwidth, V_WIDTHMULTIPLIER );
    
    // thing clipping
    for (i=0 ; i<viewwidth ; i++)
	screenheightarray[i] = viewheight;
    
    // planes
    for (i=0 ; i<viewheight ; i++)
    {
	dy = ((i-viewheight/2)<<FRACBITS)+FRACUNIT/2;
	dy = abs(dy);
	yslope[i] = FixedDiv ( (viewwidth<<detailshift)/2*FRACUNIT, dy);
    }
	
    for (i=0 ; i<viewwidth ; i++)
    {
	cosadj = abs(renderfinecosine[xtoviewangle[i]>>RENDERANGLETOFINESHIFT]);
	distscale[i] = FixedDiv (FRACUNIT,cosadj);
    }
    
    // Calculate the light levels to use
    //  for each level / scale combination.
    for (i=0 ; i< LIGHTLEVELS ; i++)
    {
	startmap = ((LIGHTLEVELS-1-i)*2)*NUMLIGHTCOLORMAPS/LIGHTLEVELS;
	for (j=0 ; j<MAXLIGHTSCALE ; j++)
	{
	    level = startmap - j*SCREENWIDTH/(viewwidth<<detailshift)/DISTMAP;

	    if (level < 0)
		level = 0;

	    if (level >= NUMLIGHTCOLORMAPS)
		level = NUMLIGHTCOLORMAPS-1;

	    scalelight[i][j] = colormaps + level*256;
		scalelightindex[i][j] = level;
	}
    }
}



//
// R_Init
//

#include "z_zone.h"

rendercontext_t* rendercontext;


void R_Init (void)
{
	R_InitColFuncs();
	rendercontext = Z_Malloc( sizeof( *rendercontext ), PU_STATIC, NULL );

    R_InitData ();
    printf (".");
    R_InitPointToAngle ();
    printf (".");
    R_InitTables ();
    // viewwidth / viewheight / detailLevel are set by the defaults
    printf (".");

    R_SetViewSize (screenblocks, detailLevel);
    R_InitPlanes ();
    printf (".");
    R_InitLightTables ();
    printf (".");
    R_InitSkyMap ();
    R_InitTranslationTables ();
    printf (".");
	
    framecount = 0;
}


//
// R_PointInSubsector
//
subsector_t*
R_PointInSubsector
( fixed_t	x,
  fixed_t	y )
{
    node_t*	node;
    int		side;
    int		nodenum;

    // single subsector is a special case
    if (!numnodes)				
	return subsectors;
		
    nodenum = numnodes-1;

    while (! (nodenum & NF_SUBSECTOR) )
    {
	node = &nodes[nodenum];
	side = R_PointOnSide (x, y, node);
	nodenum = node->children[side];
    }
	
    return &subsectors[nodenum & ~NF_SUBSECTOR];
}

void R_ResetContext( rendercontext_t* context )
{
	R_ClearClipSegs( &context->bspcontext, 0, viewwidth );
	R_ClearDrawSegs( &context->bspcontext );
	R_ClearPlanes( &context->planecontext, viewwidth, viewheight, viewangle );
	R_ClearSprites( &context->spritecontext );
}


//
// R_SetupFrame
//
void R_SetupFrame (player_t* player)
{		
	int32_t		i;

	viewplayer = player;
	viewx = player->mo->x;
	viewy = player->mo->y;
	viewangle = player->mo->angle + viewangleoffset;
	extralight = player->extralight;

	viewz = player->viewz;
    
	viewsin = renderfinesine[viewangle>>RENDERANGLETOFINESHIFT];
	viewcos = renderfinecosine[viewangle>>RENDERANGLETOFINESHIFT];
	
	sscount = 0;
	
	fixedcolormapindex = player->fixedcolormap;
	if (player->fixedcolormap)
	{
		fixedcolormap =
			colormaps
			+ player->fixedcolormap*256;
	
		walllights = scalelightfixed;
		walllightsindex = fixedcolormapindex;

		for (i=0 ; i<MAXLIGHTSCALE ; i++)
			scalelightfixed[i] = fixedcolormap;
	}
	else
	{
		fixedcolormap = 0;
	}

	R_ResetContext( rendercontext );

	framecount++;
	validcount++;
}


//
// R_RenderView
//
void R_RenderPlayerView (player_t* player)
{	
    R_SetupFrame (player);

    // check for new console commands.
    NetUpdate ();

    // The head node is the last node output.
    R_RenderBSPNode ( &rendercontext->bspcontext, numnodes-1);
    
    // Check for new console commands.
    NetUpdate ();
    
    R_DrawPlanes ();
    
    // Check for new console commands.
    NetUpdate ();
    
    R_DrawMasked ();

    // Check for new console commands.
    NetUpdate ();				
}
