//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2005, 2006 Andrey Budko
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
//	Movement/collision utility functions,
//	as used by function in p_map.c. 
//	BLOCKMAP Iterator functions,
//	and some PIT_* functions to use for iteration.
//



#include <stdlib.h>


#include "m_bbox.h"

#include "doomdef.h"
#include "doomstat.h"

#include "d_gamesim.h"

#include "p_local.h"


// State.
#include "r_state.h"

#include "i_system.h"

//
// P_AproxDistance
// Gives an estimation of distance (not exact)
//

DOOM_C_API fixed_t P_AproxDistance( fixed_t dx, fixed_t dy )
{
    dx = abs(dx);
    dy = abs(dy);
    if (dx < dy)
	return dx+dy-(dx>>1);
    return dx+dy-(dy>>1);
}


//
// P_PointOnLineSide
// Returns 0 or 1
//
DOOM_C_API int P_PointOnLineSide( fixed_t x, fixed_t y, line_t* line )
{
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	left;
    fixed_t	right;
	
    if (!line->dx)
    {
	if (x <= line->v1->x)
	    return line->dy > 0;
	
	return line->dy < 0;
    }
    if (!line->dy)
    {
	if (y <= line->v1->y)
	    return line->dx < 0;
	
	return line->dx > 0;
    }
	
    dx = (x - line->v1->x);
    dy = (y - line->v1->y);
	
    left = FixedMul ( line->dy>>FRACBITS , dx );
    right = FixedMul ( dy , line->dx>>FRACBITS );
	
    if (right < left)
	return 0;		// front side
    return 1;			// back side
}



//
// P_BoxOnLineSide
// Considers the line to be infinite
// Returns side 0 or 1, -1 if box crosses the line.
//
DOOM_C_API int P_BoxOnLineSide( fixed_t* tmbox, line_t* ld )
{
    int		p1 = 0;
    int		p2 = 0;
	
    switch (ld->slopetype)
    {
      case ST_HORIZONTAL:
	p1 = tmbox[BOXTOP] > ld->v1->y;
	p2 = tmbox[BOXBOTTOM] > ld->v1->y;
	if (ld->dx < 0)
	{
	    p1 ^= 1;
	    p2 ^= 1;
	}
	break;
	
      case ST_VERTICAL:
	p1 = tmbox[BOXRIGHT] < ld->v1->x;
	p2 = tmbox[BOXLEFT] < ld->v1->x;
	if (ld->dy < 0)
	{
	    p1 ^= 1;
	    p2 ^= 1;
	}
	break;
	
      case ST_POSITIVE:
	p1 = P_PointOnLineSide (tmbox[BOXLEFT], tmbox[BOXTOP], ld);
	p2 = P_PointOnLineSide (tmbox[BOXRIGHT], tmbox[BOXBOTTOM], ld);
	break;
	
      case ST_NEGATIVE:
	p1 = P_PointOnLineSide (tmbox[BOXRIGHT], tmbox[BOXTOP], ld);
	p2 = P_PointOnLineSide (tmbox[BOXLEFT], tmbox[BOXBOTTOM], ld);
	break;
    }

    if (p1 == p2)
	return p1;
    return -1;
}


//
// P_PointOnDivlineSide
// Returns 0 or 1.
//
DOOM_C_API int P_PointOnDivlineSide( fixed_t x, fixed_t y, divline_t* line )
{
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	left;
    fixed_t	right;
	
    if (!line->dx)
    {
	if (x <= line->x)
	    return line->dy > 0;
	
	return line->dy < 0;
    }
    if (!line->dy)
    {
	if (y <= line->y)
	    return line->dx < 0;

	return line->dx > 0;
    }
	
    dx = (x - line->x);
    dy = (y - line->y);
	
    // try to quickly decide by looking at sign bits
    if ( (line->dy ^ line->dx ^ dx ^ dy)&0x80000000 )
    {
	if ( (line->dy ^ dx) & 0x80000000 )
	    return 1;		// (left is negative)
	return 0;
    }
	
    left = FixedMul ( line->dy>>8, dx>>8 );
    right = FixedMul ( dy>>8 , line->dx>>8 );
	
    if (right < left)
	return 0;		// front side
    return 1;			// back side
}



//
// P_MakeDivline
//
DOOM_C_API void P_MakeDivline( line_t* li, divline_t* dl )
{
    dl->x = li->v1->x;
    dl->y = li->v1->y;
    dl->dx = li->dx;
    dl->dy = li->dy;
}



//
// P_InterceptVector
// Returns the fractional intercept point
// along the first divline.
// This is only called by the addthings
// and addlines traversers.
//
DOOM_C_API fixed_t P_InterceptVector( divline_t* v2, divline_t* v1 )
{
    fixed_t	frac;
    fixed_t	num;
    fixed_t	den;
	
    den = FixedMul (v1->dy>>8,v2->dx) - FixedMul(v1->dx>>8,v2->dy);

    if (den == 0)
	return 0;
    //	I_Error ("P_InterceptVector: parallel");
    
    num =
	FixedMul ( (v1->x - v2->x)>>8 ,v1->dy )
	+FixedMul ( (v2->y - v1->y)>>8, v1->dx );

    frac = FixedDiv (num , den);

    return frac;
}


//
// P_LineOpening
// Sets opentop and openbottom to the window
// through a two sided line.
// OPTIMIZE: keep this precalculated
//
fixed_t opentop;
fixed_t openbottom;
fixed_t openrange;
fixed_t	lowfloor;


DOOM_C_API void P_LineOpening( line_t* linedef )
{
    sector_t*	front;
    sector_t*	back;
	
    if (linedef->sidenum[1] == -1)
    {
	// single sided line
	openrange = 0;
	return;
    }
	 
    front = linedef->frontsector;
    back = linedef->backsector;
	
    if (front->ceilingheight < back->ceilingheight)
	opentop = front->ceilingheight;
    else
	opentop = back->ceilingheight;

    if (front->floorheight > back->floorheight)
    {
	openbottom = front->floorheight;
	lowfloor = back->floorheight;
    }
    else
    {
	openbottom = back->floorheight;
	lowfloor = front->floorheight;
    }
	
    openrange = opentop - openbottom;
}


//
// THING POSITION SETTING
//


//
// P_UnsetThingPosition
// Unlinks a thing from block map and sectors.
// On each position change, BLOCKMAP and other
// lookups maintaining lists ot things inside
// these structures need to be updated.
//
DOOM_C_API void P_UnsetThingPosition( mobj_t* thing )
{
    int		blockx;
    int		blocky;

    if ( ! (thing->flags & MF_NOSECTOR) )
    {
		// inert things don't need to be in blockmap?
		// unlink from subsector
		if (thing->snext)
			thing->snext->sprev = thing->sprev;

		if (thing->sprev)
			thing->sprev->snext = thing->snext;
		else
			thing->subsector->sector->thinglist = thing->snext;
    }
	else
	{
		if (thing->nosectornext)
			thing->nosectornext->nosectorprev = thing->nosectorprev;

		if (thing->nosectorprev)
			thing->nosectorprev->nosectornext = thing->nosectornext;
		else
			thing->subsector->sector->nosectorthinglist = thing->nosectornext;
	}

	if ( ! (thing->flags & MF_NOBLOCKMAP) )
    {
	// inert things don't need to be in blockmap
	// unlink from block map
	if (thing->bnext)
	    thing->bnext->bprev = thing->bprev;
	
	if (thing->bprev)
	    thing->bprev->bnext = thing->bnext;
	else
	{
	    blockx = (thing->x - bmaporgx)>>MAPBLOCKSHIFT;
	    blocky = (thing->y - bmaporgy)>>MAPBLOCKSHIFT;

	    if (blockx>=0 && blockx < bmapwidth
		&& blocky>=0 && blocky <bmapheight)
	    {
		blocklinks[blocky*bmapwidth+blockx] = thing->bnext;
	    }
	}
    }
}


//
// P_SetThingPosition
// Links a thing into both a block and a subsector
// based on it's x y.
// Sets thing->subsector properly
//
DOOM_C_API void P_SetThingPosition( mobj_t* thing )
{
    subsector_t*	ss;
    sector_t*		sec;
    int			blockx;
    int			blocky;
    mobj_t**		link;

    
    // link into subsector
    ss = BSP_PointInSubsector( thing->x, thing->y );
    thing->subsector = ss;
    
	sec = ss->sector;
    if ( ! (thing->flags & MF_NOSECTOR) )
    {
		// invisible things don't go into the sector links
	
		thing->sprev = NULL;
		thing->snext = sec->thinglist;

		if (sec->thinglist)
			sec->thinglist->sprev = thing;

		sec->thinglist = thing;
    }
	else
	{
		thing->nosectorprev = NULL;
		thing->nosectornext = sec->nosectorthinglist;
		if( sec->nosectorthinglist )
		{
			sec->nosectorthinglist->nosectorprev = thing;
		}
		sec->nosectorthinglist = thing;
	}

    
    // link into blockmap
    if ( ! (thing->flags & MF_NOBLOCKMAP) )
    {
	// inert things don't need to be in blockmap		
	blockx = (thing->x - bmaporgx)>>MAPBLOCKSHIFT;
	blocky = (thing->y - bmaporgy)>>MAPBLOCKSHIFT;

	if (blockx>=0
	    && blockx < bmapwidth
	    && blocky>=0
	    && blocky < bmapheight)
	{
	    link = &blocklinks[blocky*bmapwidth+blockx];
	    thing->bprev = NULL;
	    thing->bnext = *link;
	    if (*link)
		(*link)->bprev = thing;

	    *link = thing;
	}
	else
	{
	    // thing is off the map
	    thing->bnext = thing->bprev = NULL;
	}
    }
}



//
// BLOCK MAP ITERATORS
// For each line/thing in the given mapblock,
// call the passed PIT_* function.
// If the function returns false,
// exit with false without checking anything else.
//


//
// P_BlockLinesIterator
// The validcount flags are used to avoid checking lines
// that are marked in multiple mapblocks,
// so increment validcount before the first call
// to P_BlockLinesIterator, then make one or more calls
// to it.
//
doombool P_BlockLinesIterator( int x, int y, iteratelinefunc_t&& func )
{
	int32_t			offset;
	blockmap_t*		list;
	line_t*			ld;
	
	if (x<0
		|| y<0
		|| x>=bmapwidth
		|| y>=bmapheight)
	{
		return true;
	}

	offset = y*bmapwidth+x;
	
	offset = *(blockmap + offset);

	for ( list = blockmapbase+offset ; *list != BLOCKMAP_INVALID ; ++list)
	{
		blockmap_t val = *list;
#if RANGECHECK
		if( val != BLOCKMAP_INVALID && val >= numlines )
		{
			I_Error( "P_BlockLinesIterator: %d out of range", (int32_t)*list );
		}
#endif
		ld = &lines[*list];

		if (ld->validcount == validcount)
			continue; 	// line has already been checked

		ld->validcount = validcount;
		
		if ( !func(ld) )
			return false;
	}
	return true;	// everything was checked
}

DOOM_C_API doombool P_BlockLinesIterator( int32_t x, int32_t y, doombool(*func)(line_t*) )
{
	 return P_BlockLinesIterator( x, y, iteratelinefunc_t( func ) );
}

doombool P_BlockLinesIteratorConst( int x, int y, iteratelinefunc_t&& func )
{
	if (x<0
		|| y<0
		|| x>=bmapwidth
		|| y>=bmapheight)
	{
		return true;
	}

	int32_t offset = y * bmapwidth + x;
	
	offset = *(blockmap + offset);

	for( blockmap_t* list = blockmapbase + offset; *list != BLOCKMAP_INVALID ; ++list )
	{
		blockmap_t val = *list;
#if RANGECHECK
		if( val != BLOCKMAP_INVALID && val >= numlines )
		{
			I_Error( "P_BlockLinesIterator: %d out of range", (int32_t)*list );
		}
#endif
		line_t& ld = lines[ val ];

		if ( !func( &ld ) )
			return false;
	}

	return true;	// everything was checked
}

doombool P_BlockLinesIteratorConstHorizontal( iota&& xrange, iota&& yrange, iteratelinefunc_t&& func )
{
	for( int32_t y : yrange )
	{
		for( int32_t x : xrange )
		{
			if( !P_BlockLinesIteratorConst( x, y, std::forward< iteratelinefunc_t >( func ) ) )
			{
				return false;
			}
		}
	}

	return true;
}

doombool P_BlockLinesIteratorConstVertical( iota&& xrange, iota&& yrange, iteratelinefunc_t&& func )
{
	for( int32_t x : xrange )
	{
		for( int32_t y : yrange )
		{
			if( !P_BlockLinesIteratorConst( x, y, std::forward< iteratelinefunc_t >( func ) ) )
			{
				return false;
			}
		}
	}

	return true;
}

doombool P_BlockLinesIteratorConstHorizontal( fixed_t x, fixed_t y, fixed_t radius, iteratelinefunc_t&& func )
{
	int32_t xl = ( x - radius - bmaporgx ) >> MAPBLOCKSHIFT;
	int32_t xh = ( ( x + radius - bmaporgx ) >> MAPBLOCKSHIFT ) + 1;
	int32_t yl = ( y - radius - bmaporgy ) >> MAPBLOCKSHIFT;
	int32_t yh = ( ( y + radius - bmaporgy ) >> MAPBLOCKSHIFT ) + 1;

	if( xh < xl || yh < yl ) return true;

	return P_BlockLinesIteratorConstHorizontal( iota( xl, xh ), iota( yl, yh ), std::forward< iteratelinefunc_t >( func ) );
}

doombool P_BlockLinesIteratorConstVertical( fixed_t x, fixed_t y, fixed_t radius, iteratelinefunc_t&& func )
{
	int32_t xl = ( x - radius - bmaporgx ) >> MAPBLOCKSHIFT;
	int32_t xh = ( ( x + radius - bmaporgx ) >> MAPBLOCKSHIFT ) + 1;
	int32_t yl = ( y - radius - bmaporgy ) >> MAPBLOCKSHIFT;
	int32_t yh = ( ( y + radius - bmaporgy ) >> MAPBLOCKSHIFT ) + 1;

	if( xh < xl || yh < yl ) return true;

	return P_BlockLinesIteratorConstVertical( iota( xl, xh ), iota( yl, yh ), std::forward< iteratelinefunc_t >( func ) );
}


//
// P_BlockThingsIterator
//
DOOM_C_API doombool P_BlockThingsIterator( int x, int y, doombool(*func)(mobj_t*) )
{
	return P_BlockThingsIterator( x, y, iteratemobjfunc_t( func ) );
}



//
// INTERCEPT ROUTINES
//
intercept_t*	intercepts = nullptr;
intercept_t*	intercept_p =  nullptr;
size_t			interceptscount = 0;

divline_t 	trace;
doombool 	earlyout;
int			ptflags;

static void IncreaseIntercepts()
{
	if( intercept_p >= intercepts + interceptscount )
	{
		intercept_t* oldintercepts = intercepts;
		size_t oldcount = interceptscount;
		ptrdiff_t oldp = intercept_p - intercepts;

		interceptscount += MAXINTERCEPTS;
		intercepts = Z_MallocArrayAs( intercept_t, interceptscount, PU_STATIC, nullptr );
		intercept_p = intercepts + oldp;

		if( oldintercepts )
		{
			memcpy( intercepts, oldintercepts, sizeof( intercept_t ) * oldcount );
			Z_Free( oldintercepts );
		}
	}
}

static void InterceptsOverrun(int num_intercepts, intercept_t *intercept);

//
// PIT_AddLineIntercepts.
// Looks for lines in the given block
// that intercept the given trace
// to add to the intercepts list.
//
// A line is crossed if its endpoints
// are on opposite sides of the trace.
// Returns true if earlyout and a solid line hit.
//
DOOM_C_API doombool PIT_AddLineIntercepts (line_t* ld)
{
    int			s1;
    int			s2;
    fixed_t		frac;
    divline_t		dl;
	
    // avoid precision problems with two routines
    if ( trace.dx > FRACUNIT*16
	 || trace.dy > FRACUNIT*16
	 || trace.dx < -FRACUNIT*16
	 || trace.dy < -FRACUNIT*16)
    {
	s1 = P_PointOnDivlineSide (ld->v1->x, ld->v1->y, &trace);
	s2 = P_PointOnDivlineSide (ld->v2->x, ld->v2->y, &trace);
    }
    else
    {
	s1 = P_PointOnLineSide (trace.x, trace.y, ld);
	s2 = P_PointOnLineSide (trace.x+trace.dx, trace.y+trace.dy, ld);
    }
    
    if (s1 == s2)
	return true;	// line isn't crossed
    
    // hit the line
    P_MakeDivline (ld, &dl);
    frac = P_InterceptVector (&trace, &dl);

    if (frac < 0)
	return true;	// behind source
	
    // try to early out the check
    if (earlyout
	&& frac < FRACUNIT
	&& !ld->backsector)
    {
	return false;	// stop checking
    }
    
	IncreaseIntercepts();

    intercept_p->frac = frac;
    intercept_p->isaline = true;
    intercept_p->d.line = ld;
    InterceptsOverrun(intercept_p - intercepts, intercept_p);
    intercept_p++;

    return true;	// continue
}



//
// PIT_AddThingIntercepts
//
DOOM_C_API doombool PIT_AddThingIntercepts( mobj_t* thing )
{
    fixed_t		x1;
    fixed_t		y1;
    fixed_t		x2;
    fixed_t		y2;
    
    int			s1;
    int			s2;
    
    doombool		tracepositive;

    divline_t		dl;
    
    fixed_t		frac;
	
    tracepositive = (trace.dx ^ trace.dy)>0;
		
    // check a corner to corner crossection for hit
    if (tracepositive)
    {
	x1 = thing->x - thing->radius;
	y1 = thing->y + thing->radius;
		
	x2 = thing->x + thing->radius;
	y2 = thing->y - thing->radius;			
    }
    else
    {
	x1 = thing->x - thing->radius;
	y1 = thing->y - thing->radius;
		
	x2 = thing->x + thing->radius;
	y2 = thing->y + thing->radius;			
    }
    
    s1 = P_PointOnDivlineSide (x1, y1, &trace);
    s2 = P_PointOnDivlineSide (x2, y2, &trace);

    if (s1 == s2)
	return true;		// line isn't crossed
	
    dl.x = x1;
    dl.y = y1;
    dl.dx = x2-x1;
    dl.dy = y2-y1;
    
    frac = P_InterceptVector (&trace, &dl);

    if (frac < 0)
	return true;		// behind source

	IncreaseIntercepts();

    intercept_p->frac = frac;
    intercept_p->isaline = false;
    intercept_p->d.thing = thing;
    InterceptsOverrun(intercept_p - intercepts, intercept_p);
    intercept_p++;

    return true;		// keep going
}


//
// P_TraverseIntercepts
// Returns true if the traverser function returns true
// for all lines.
// 
doombool P_TraverseIntercepts( traverser_t func, fixed_t maxfrac )
{
    int			count;
    fixed_t		dist;
    intercept_t*	scan;
    intercept_t*	in;
	
    count = intercept_p - intercepts;
    
    in = 0;			// shut up compiler warning
	
    while (count--)
    {
	dist = INT_MAX;
	for (scan = intercepts ; scan<intercept_p ; scan++)
	{
	    if (scan->frac < dist)
	    {
		dist = scan->frac;
		in = scan;
	    }
	}
	
	if (dist > maxfrac)
	    return true;	// checked everything in range		

#if 0  // UNUSED
    {
	// don't check these yet, there may be others inserted
	in = scan = intercepts;
	for ( scan = intercepts ; scan<intercept_p ; scan++)
	    if (scan->frac > maxfrac)
		*in++ = *scan;
	intercept_p = in;
	return false;
    }
#endif

        if ( !func (in) )
	    return false;	// don't bother going farther

	in->frac = INT_MAX;
    }
	
    return true;		// everything was traversed
}

DOOM_C_API extern fixed_t bulletslope;

// Intercepts Overrun emulation, from PrBoom-plus.
// Thanks to Andrey Budko (entryway) for researching this and his 
// implementation of Intercepts Overrun emulation in PrBoom-plus
// which this is based on.

typedef struct
{
    int len;
    void *addr;
    doombool int16_array;
} intercepts_overrun_t;

// Intercepts memory table.  This is where various variables are located
// in memory in Vanilla Doom.  When the intercepts table overflows, we
// need to write to them.
//
// Almost all of the values to overwrite are 32-bit integers, except for
// playerstarts, which is effectively an array of 16-bit integers and
// must be treated differently.

static intercepts_overrun_t intercepts_overrun[] =
{
    {4,   NULL,                          false},
    {4,   NULL, /* &earlyout, */         false},
    {4,   NULL, /* &intercept_p, */      false},
    {4,   &lowfloor,                     false},
    {4,   &openbottom,                   false},
    {4,   &opentop,                      false},
    {4,   &openrange,                    false},
    {4,   NULL,                          false},
    {120, NULL, /* &activeplats, */      false},
    {8,   NULL,                          false},
    {4,   &bulletslope,                  false},
    {4,   NULL, /* &swingx, */           false},
    {4,   NULL, /* &swingy, */           false},
    {4,   NULL,                          false},
    {40,  &playerstarts,                 true},
    {4,   NULL, /* &blocklinks, */       false},
    {4,   &bmapwidth,                    false},
    {4,   NULL, /* &blockmap, */         false},
    {4,   &bmaporgx,                     false},
    {4,   &bmaporgy,                     false},
    {4,   NULL, /* &blockmaplump, */     false},
    {4,   &bmapheight,                   false},
    {0,   NULL,                          false},
};

// Overwrite a specific memory location with a value.

static void InterceptsMemoryOverrun(int location, int value)
{
    int i, offset;
    int index;
    void *addr;

    i = 0;
    offset = 0;

    // Search down the array until we find the right entry

    while (intercepts_overrun[i].len != 0)
    {
        if (offset + intercepts_overrun[i].len > location)
        {
            addr = intercepts_overrun[i].addr;

            // Write the value to the memory location.
            // 16-bit and 32-bit values are written differently.

            if (addr != NULL)
            {
                if (intercepts_overrun[i].int16_array)
                {
                    index = (location - offset) / 2;
                    ((short *) addr)[index] = value & 0xffff;
                    ((short *) addr)[index + 1] = (value >> 16) & 0xffff;
                }
                else
                {
                    index = (location - offset) / 4;
                    ((int *) addr)[index] = value;
                }
            }

            break;
        }

        offset += intercepts_overrun[i].len;
        ++i;
    }
}

// Emulate overruns of the intercepts[] array.

static void InterceptsOverrun(int num_intercepts, intercept_t *intercept)
{
    int location;

    if (fix.intercepts_overflow || num_intercepts <= MAXINTERCEPTS)
    {
        // No overrun

        return;
    }

    location = (num_intercepts - MAXINTERCEPTS - 1) * 12;

    // Overwrite memory that is overwritten in Vanilla Doom, using
    // the values from the intercept structure.
    //
    // Note: the ->d.{thing,line} member should really have its
    // address translated into the correct address value for 
    // Vanilla Doom.

    InterceptsMemoryOverrun(location, intercept->frac);
    InterceptsMemoryOverrun(location + 4, intercept->isaline);
    InterceptsMemoryOverrun(location + 8, (intptr_t) intercept->d.thing);
}


//
// P_PathTraverse
// Traces a line from x1,y1 to x2,y2,
// calling the traverser function for each.
// Returns true if the traverser function returns true
// for all lines.
//
DOOM_C_API doombool P_PathTraverse( fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, int flags, doombool(*trav)(intercept_t *) )
{
    fixed_t	xt1;
    fixed_t	yt1;
    fixed_t	xt2;
    fixed_t	yt2;
    
    fixed_t	xstep;
    fixed_t	ystep;
    
    fixed_t	partial;
    
    fixed_t	xintercept;
    fixed_t	yintercept;
    
    int		mapx;
    int		mapy;
    
    int		mapxstep;
    int		mapystep;

    int		count;
		
    earlyout = (flags & PT_EARLYOUT) != 0;
		
    validcount++;
    intercept_p = intercepts;
	
    if ( ((x1-bmaporgx)&(MAPBLOCKSIZE-1)) == 0)
	x1 += FRACUNIT;	// don't side exactly on a line
    
    if ( ((y1-bmaporgy)&(MAPBLOCKSIZE-1)) == 0)
	y1 += FRACUNIT;	// don't side exactly on a line

    trace.x = x1;
    trace.y = y1;
    trace.dx = x2 - x1;
    trace.dy = y2 - y1;

    x1 -= bmaporgx;
    y1 -= bmaporgy;
    xt1 = x1>>MAPBLOCKSHIFT;
    yt1 = y1>>MAPBLOCKSHIFT;

    x2 -= bmaporgx;
    y2 -= bmaporgy;
    xt2 = x2>>MAPBLOCKSHIFT;
    yt2 = y2>>MAPBLOCKSHIFT;

    if (xt2 > xt1)
    {
	mapxstep = 1;
	partial = FRACUNIT - ((x1>>MAPBTOFRAC)&(FRACUNIT-1));
	ystep = FixedDiv (y2-y1,abs(x2-x1));
    }
    else if (xt2 < xt1)
    {
	mapxstep = -1;
	partial = (x1>>MAPBTOFRAC)&(FRACUNIT-1);
	ystep = FixedDiv (y2-y1,abs(x2-x1));
    }
    else
    {
	mapxstep = 0;
	partial = FRACUNIT;
	ystep = 256*FRACUNIT;
    }	

    yintercept = (y1>>MAPBTOFRAC) + FixedMul (partial, ystep);

	
    if (yt2 > yt1)
    {
	mapystep = 1;
	partial = FRACUNIT - ((y1>>MAPBTOFRAC)&(FRACUNIT-1));
	xstep = FixedDiv (x2-x1,abs(y2-y1));
    }
    else if (yt2 < yt1)
    {
	mapystep = -1;
	partial = (y1>>MAPBTOFRAC)&(FRACUNIT-1);
	xstep = FixedDiv (x2-x1,abs(y2-y1));
    }
    else
    {
	mapystep = 0;
	partial = FRACUNIT;
	xstep = 256*FRACUNIT;
    }	
    xintercept = (x1>>MAPBTOFRAC) + FixedMul (partial, xstep);
    
    // Step through map blocks.
    // Count is present to prevent a round off error
    // from skipping the break.
    mapx = xt1;
    mapy = yt1;
	
    for (count = 0 ; count < 64 ; count++)
    {
	if (flags & PT_ADDLINES)
	{
	    if (!P_BlockLinesIterator (mapx, mapy,PIT_AddLineIntercepts))
		return false;	// early out
	}
	
	if (flags & PT_ADDTHINGS)
	{
	    if (!P_BlockThingsIterator (mapx, mapy,PIT_AddThingIntercepts))
		return false;	// early out
	}
		
	if (mapx == xt2
	    && mapy == yt2)
	{
	    break;
	}
	
	if ( (yintercept >> FRACBITS) == mapy)
	{
	    yintercept += ystep;
	    mapx += mapxstep;
	}
	else if ( (xintercept >> FRACBITS) == mapx)
	{
	    xintercept += xstep;
	    mapy += mapystep;
	}
		
    }
    // go through the sorted list
    return P_TraverseIntercepts ( trav, FRACUNIT );
}


//
// P_BlockThingsIterator
//
doombool P_BlockThingsIterator( int32_t x, int32_t y, iteratemobjfunc_t&& func )
{
	if ( x < 0
		|| y < 0
		|| x >= bmapwidth
		|| y >= bmapheight)
	{
		return true;
	}

	for( mobj_t* mobj = blocklinks[ y * bmapwidth + x ]; mobj; )
	{
		mobj_t* bnext = mobj->bnext;
		if ( !func( mobj ) )
		{
			return false;
		}
		mobj = fix.blockthingsiterator ? bnext : mobj->bnext;
	}

	return true;
}

doombool P_BlockThingsIteratorHorizontal( iota&& xrange, iota&& yrange, iteratemobjfunc_t&& func )
{
	for( int32_t y : yrange )
	{
		for( int32_t x : xrange )
		{
			if( !P_BlockThingsIterator( x, y, std::forward< iteratemobjfunc_t >( func ) ) )
			{
				return false;
			}
		}
	}

	return true;
}

doombool P_BlockThingsIteratorVertical( iota&& xrange, iota&& yrange, iteratemobjfunc_t&& func )
{
	for( int32_t x : xrange )
	{
		for( int32_t y : yrange )
		{
			if( !P_BlockThingsIterator( x, y, std::forward< iteratemobjfunc_t >( func ) ) )
			{
				return false;
			}
		}
	}

	return true;
}

doombool P_BlockThingsIteratorHorizontal( fixed_t x, fixed_t y, fixed_t radius, iteratemobjfunc_t&& func )
{
	int32_t xl = ( x - radius - bmaporgx ) >> MAPBLOCKSHIFT;
	int32_t xh = ( ( x + radius - bmaporgx ) >> MAPBLOCKSHIFT ) + 1;
	int32_t yl = ( y - radius - bmaporgy ) >> MAPBLOCKSHIFT;
	int32_t yh = ( ( y + radius - bmaporgy ) >> MAPBLOCKSHIFT ) + 1;

	if( xh < xl || yh < yl ) return true;

	return P_BlockThingsIteratorHorizontal( iota( xl, xh ), iota( yl, yh ), std::forward< iteratemobjfunc_t >( func ) );
}

doombool P_BlockThingsIteratorVertical( fixed_t x, fixed_t y, fixed_t radius, iteratemobjfunc_t&& func )
{
	int32_t xl = ( x - radius - bmaporgx ) >> MAPBLOCKSHIFT;
	int32_t xh = ( ( x + radius - bmaporgx ) >> MAPBLOCKSHIFT ) + 1;
	int32_t yl = ( y - radius - bmaporgy ) >> MAPBLOCKSHIFT;
	int32_t yh = ( ( y + radius - bmaporgy ) >> MAPBLOCKSHIFT ) + 1;

	if( xh < xl || yh < yl ) return true;

	return P_BlockThingsIteratorVertical( iota( xl, xh ), iota( yl, yh ), std::forward< iteratemobjfunc_t >( func ) );
}

DOOM_C_API doombool P_BBoxOverlapsSector( sector_t* sector, fixed_t* bbox )
{
	auto Test = [&bbox]( const fixed_t& coeff1, const fixed_t& coeff2, const fixed_t& cross, const int32_t horiz, const int32_t vert )
	{
		return FixedMul( coeff1, bbox[ horiz ] ) + FixedMul( coeff2, bbox[ vert ] ) + cross;
	};

	for( line_t* line : Lines( *sector ) )
	{
		vertex_t* v1 = line->v1;
		vertex_t* v2 = line->v2;
		if( line->frontsector != sector )
		{
			std::swap( v1, v2 );
		}

		if( ( ( v1->x > bbox[ BOXRIGHT ] ) & ( v2->x > bbox[ BOXRIGHT ] ) )
			|| ( ( v1->x < bbox[ BOXLEFT ] ) & ( v2->x < bbox[ BOXLEFT ] ) )
			|| ( ( v1->y > bbox[ BOXTOP ] ) & ( v2->y > bbox[ BOXTOP ] ) )
			|| ( ( v1->y < bbox[ BOXBOTTOM ] ) & ( v2->y < bbox[ BOXBOTTOM ] ) ) )
		{
			continue;
		}

		fixed_t coeff1 = v2->y - v1->y;
		fixed_t coeff2 = v1->x - v2->x;
		fixed_t cross = FixedMul( v2->x, v1->y ) - FixedMul( v1->x, v2->y );

		fixed_t bl = Test( coeff1, coeff2, cross, BOXLEFT, BOXBOTTOM );
		fixed_t tl = Test( coeff1, coeff2, cross, BOXLEFT, BOXTOP );
		fixed_t br = Test( coeff1, coeff2, cross, BOXRIGHT, BOXBOTTOM );
		fixed_t tr = Test( coeff1, coeff2, cross, BOXRIGHT, BOXTOP );

		bool allpositive = ( bl > 0 ) & ( tl > 0 ) & ( br > 0 ) & ( tr > 0 );
		bool allnegative = ( bl < 0 ) & ( tl < 0 ) & ( br < 0 ) & ( tr < 0 );

		if( !( allpositive | allnegative ) )
		{
			return true;
		}
	}

	return false;
}

DOOM_C_API doombool P_MobjOverlapsSector( sector_t* sector, mobj_t* mobj, fixed_t radius )
{
	if( mobj->subsector->sector == sector )
	{
		return true;
	}

	radius = !radius ? mobj->radius : radius;

	fixed_t bbox[ 4 ] =
	{
		mobj->y + radius,	// BOXTOP
		mobj->y - radius,	// BOXBOTTOM
		mobj->x - radius,	// BOXLEFT
		mobj->x + radius,	// BOXRIGHT
	};

	return P_BBoxOverlapsSector( sector, bbox );
}

#define METHOD 1

#if !METHOD
static void P_AddToSector( sector_t* thissector, mobj_t* thismobj )
{
	sectormobj_t* newsecmobj = currsecthings->Allocate< sectormobj_t >();
	sectormobj_t* prevhead = (sectormobj_t*)I_AtomicExchange( &currsectors[ thissector->index ].sectormobjs, (atomicval_t)newsecmobj );
	*newsecmobj = { thismobj, prevhead };
}

static void P_AddIfOverlaps( sector_t* thissector, mobj_t* thismobj, fixed_t radius )
{
	if( P_MobjOverlapsSector( thissector, thismobj, radius ) )
	{
		P_AddToSector( thissector, thismobj );
	}
}

DOOM_C_API void P_SortMobj( mobj_t* mobj )
{
	M_PROFILE_FUNC();

	memset( mobj->tested_sector, 0, sizeof(uint8_t) * numsectors );

	P_AddToSector( mobj->subsector->sector, mobj );
	mobj->tested_sector[ mobj->subsector->sector->index ] = true;

	fixed_t radius = mobj->curr.sprite != -1 ? M_MAX( mobj->radius, sprites[ mobj->curr.sprite ].maxradius )
											: mobj->radius;

	P_BlockLinesIteratorConstHorizontal( mobj->x, mobj->y, radius, [ mobj, radius ]( line_t* line ) -> bool
	{
		if( line->frontsector && !mobj->tested_sector[ line->frontsector->index ] )
		{
			P_AddIfOverlaps( line->frontsector, mobj, radius );
			mobj->tested_sector[ line->frontsector->index ] = true;
		}

		if( line->backsector && !mobj->tested_sector[ line->backsector->index ] )
		{
			P_AddIfOverlaps( line->backsector, mobj, radius );
			mobj->tested_sector[ line->backsector->index ] = true;
		}

		return true;
	} );
}

#else

static void P_AddToSector( sector_t* thissector, mobj_t* thismobj )
{
	sectormobj_t* newsecmobj = currsecthings->Allocate< sectormobj_t >();
	sectormobj_t* prevhead = (sectormobj_t*)I_AtomicExchange( &currsectors[ thissector->index ].sectormobjs, (atomicval_t)newsecmobj );
	*newsecmobj = { thismobj, prevhead };
}

static void P_AddOverlap( mobj_t* thismobj, sector_t* thissector )
{
	if( thismobj->numoverlaps < MAX_SECTOR_OVERLAPS )
	{
		thismobj->overlaps[ thismobj->numoverlaps++ ] = thissector;
	}
}

static void P_AddIfOverlaps( mobj_t* thismobj, sector_t* thissector, fixed_t radius )
{
	if( P_MobjOverlapsSector( thissector, thismobj, radius ) )
	{
		P_AddOverlap( thismobj, thissector );
	}
}

DOOM_C_API void P_SortMobj( mobj_t* mobj )
{
	M_PROFILE_FUNC();

	if( !mobj->numoverlaps
		|| FixedToRendFixed( mobj->x ) != mobj->prev.x
		|| FixedToRendFixed( mobj->y ) != mobj->prev.y
		|| mobj->sprite != mobj->prev.sprite
		|| mobj->frame != mobj->prev.frame )
	{
		mobj->numoverlaps = 0;
		memset( mobj->tested_sector, 0, sizeof(uint8_t) * numsectors );

		P_AddOverlap( mobj, mobj->subsector->sector );
		mobj->tested_sector[ mobj->subsector->sector->index ] = true;

		fixed_t radius = mobj->curr.sprite != -1 ? M_MAX( mobj->radius, sprites[ mobj->curr.sprite ].maxradius )
												: mobj->radius;

		P_BlockLinesIteratorConstHorizontal( mobj->x, mobj->y, radius, [ mobj, radius ]( line_t* line ) -> bool
		{
			if( line->frontsector && !mobj->tested_sector[ line->frontsector->index ] )
			{
				P_AddIfOverlaps( mobj, line->frontsector, radius );
				mobj->tested_sector[ line->frontsector->index ] = true;
			}

			if( line->backsector && !mobj->tested_sector[ line->backsector->index ] )
			{
				P_AddIfOverlaps( mobj, line->backsector, radius );
				mobj->tested_sector[ line->backsector->index ] = true;
			}

			return true;
		} );
	}

	for( sector_t* sector : std::span( mobj->overlaps, mobj->numoverlaps ) )
	{
		P_AddToSector( sector, mobj );
	}
}

#endif // METHOD