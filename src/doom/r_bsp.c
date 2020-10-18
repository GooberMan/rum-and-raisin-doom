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
//	BSP traversal, handling of LineSegs for rendering.
//




#include "doomdef.h"

#include "m_bbox.h"

#include "i_system.h"

#include "r_main.h"
#include "r_plane.h"
#include "r_things.h"

// State.
#include "doomstat.h"
#include "r_state.h"
#include "m_misc.h"


void R_StoreWallRange( bspcontext_t* bspcontext, planecontext_t* planecontext, int32_t start, int32_t stop );

//
// R_ClearClipSegs
//
void R_ClearClipSegs ( bspcontext_t* context, int32_t mincol, int32_t maxcol )
{
	context->solidsegs[0].first		= -0x7fffffff;
	context->solidsegs[0].last		= mincol - 1;
	context->solidsegs[1].first		= maxcol;
	context->solidsegs[1].last		= 0x7fffffff;
	context->solidsegsend = context->solidsegs + 2;
}

//
// R_ClearDrawSegs
//
void R_ClearDrawSegs ( bspcontext_t* context )
{
	context->thisdrawseg = context->drawsegs;
}

//
// R_ClipSolidWallSegment
// Does handle solid walls,
//  e.g. single sided LineDefs (middle texture)
//  that entirely block the view.
// 
void R_ClipSolidWallSegment( bspcontext_t* bspcontext, planecontext_t* planecontext, int32_t first, int32_t last )
{
	cliprange_t*	next;
	cliprange_t*	start;

	// Find the first range that touches the range
	//  (adjacent pixels are touching).
	start = bspcontext->solidsegs;
	while (start->last < first-1)
	{
		++start;
	}

	if (first < start->first)
	{
		if (last < start->first-1)
		{
			// Post is entirely visible (above start),
			//  so insert a new clippost.
			R_StoreWallRange( bspcontext, planecontext, first, last );
			next = bspcontext->solidsegsend;
			bspcontext->solidsegsend++;

			while (next != start)
			{
				*next = *(next-1);
				--next;
			}
			next->first = first;
			next->last = last;
			return;
		}

		// There is a fragment above *start.
		R_StoreWallRange( bspcontext, planecontext, first, start->first - 1 );
		// Now adjust the clip size.
		start->first = first;
	}

	// Bottom contained in start?
	if (last <= start->last)
	{
		return;
	}
		
	next = start;
	while (last >= (next+1)->first-1)
	{
		// There is a fragment between two posts.
		R_StoreWallRange( bspcontext, planecontext, next->last + 1, (next+1)->first - 1 );
		++next;
	
		if (last <= next->last)
		{
			// Bottom is contained in next.
			// Adjust the clip size.
			start->last = next->last;
			goto crunch;
		}
	}
	
	// There is a fragment after *next.
	R_StoreWallRange( bspcontext, planecontext, next->last + 1, last );
	// Adjust the clip size.
	start->last = last;
	
	// Remove start+1 to next from the clip list,
	// because start now covers their area.
crunch:
	if (next == start)
	{
		// Post just extended past the bottom of one post.
		return;
	}

	while (next++ != bspcontext->solidsegsend)
	{
		// Remove a post.
		*++start = *next;
	}

	bspcontext->solidsegsend = start+1;
}



//
// R_ClipPassWallSegment
// Clips the given range of columns,
//  but does not includes it in the clip list.
// Does handle windows,
//  e.g. LineDefs with upper and lower texture.
//
void R_ClipPassWallSegment( bspcontext_t* bspcontext, planecontext_t* planecontext, int32_t first, int32_t last )
{
	cliprange_t*	start;

	// Find the first range that touches the range
	//  (adjacent pixels are touching).
	start = bspcontext->solidsegs;
	while (start->last < first-1)
	{
		++start;
	}

	if (first < start->first)
	{
		if (last < start->first-1)
		{
			// Post is entirely visible (above start).
			R_StoreWallRange( bspcontext, planecontext, first, last );
			return;
		}
		
		// There is a fragment above *start.
		R_StoreWallRange( bspcontext, planecontext, first, start->first - 1 );
	}

	// Bottom contained in start?
	if (last <= start->last)
	{
		return;
	}

	while (last >= (start+1)->first-1)
	{
		// There is a fragment between two posts.
		R_StoreWallRange( bspcontext, planecontext, start->last + 1, (start+1)->first - 1 );
		++start;
	
		if (last <= start->last)
		{
			return;
		}
	}
	
	// There is a fragment after *next.
	R_StoreWallRange( bspcontext, planecontext, start->last + 1, last );
}

//
// R_AddLine
// Clips the given segment
// and adds any visible pieces to the line list.
//

void R_AddLine( bspcontext_t* bspcontext, planecontext_t* planecontext, seg_t* line )
{
	int32_t		x1;
	int32_t		x2;
	angle_t		angle1;
	angle_t		angle2;
	angle_t		span;
	angle_t		tspan;

	bspcontext->curline = line;

	// OPTIMIZE: quickly reject orthogonal back sides.
	angle1 = R_PointToAngle (line->v1->x, line->v1->y);
	angle2 = R_PointToAngle (line->v2->x, line->v2->y);

	// Clip to view edges.
	// OPTIMIZE: make constant out of 2*clipangle (FIELDOFVIEW).
	span = angle1 - angle2;

	// Back side? I.e. backface culling?
	if (span >= ANG180)
	{
		return;
	}

	// Global angle needed by segcalc.
	rw_angle1 = angle1;
	angle1 -= viewangle;
	angle2 -= viewangle;
	
	tspan = angle1 + clipangle;
	if (tspan > 2*clipangle)
	{
		tspan -= 2*clipangle;

		// Totally off the left edge?
		if (tspan >= span)
			return;
	
		angle1 = clipangle;
	}
	tspan = clipangle - angle2;
	if (tspan > 2*clipangle)
	{
		tspan -= 2*clipangle;

		// Totally off the left edge?
		if (tspan >= span)
			return;	
		angle2 = M_NEGATE( clipangle );
	}

	// The seg is in the view range,
	// but not necessarily visible.
	angle1 = (angle1+ANG90)>>RENDERANGLETOFINESHIFT;
	angle2 = (angle2+ANG90)>>RENDERANGLETOFINESHIFT;
	x1 = viewangletox[angle1];
	x2 = viewangletox[angle2];

	// Does not cross a pixel?
	if (x1 == x2)
	{
		return;
	}
	
	bspcontext->backsector = line->backsector;

	// Single sided line?
	if (!bspcontext->backsector)
	{
		goto clipsolid;
	}

	// Closed door.
	if (bspcontext->backsector->ceilingheight <= bspcontext->frontsector->floorheight
		|| bspcontext->backsector->floorheight >= bspcontext->frontsector->ceilingheight)
	{
		goto clipsolid;
	}

	// Window.
	if (bspcontext->backsector->ceilingheight != bspcontext->frontsector->ceilingheight
		|| bspcontext->backsector->floorheight != bspcontext->frontsector->floorheight)
	{
		goto clippass;
	}

	// Reject empty lines used for triggers
	//  and special events.
	// Identical floor and ceiling on both sides,
	// identical light levels on both sides,
	// and no middle texture.
	if (bspcontext->backsector->ceilingpic == bspcontext->frontsector->ceilingpic
		&& bspcontext->backsector->floorpic == bspcontext->frontsector->floorpic
		&& bspcontext->backsector->lightlevel == bspcontext->frontsector->lightlevel
		&& bspcontext->curline->sidedef->midtexture == 0)
	{
		return;
	}

clippass:
	R_ClipPassWallSegment( bspcontext, planecontext, x1, x2-1 );
	return;

clipsolid:
	R_ClipSolidWallSegment( bspcontext, planecontext, x1, x2-1 );
}


//
// R_CheckBBox
// Checks BSP node/subtree bounding box.
// Returns true
//  if some part of the bbox might be visible.
//
int32_t	checkcoord[12][4] =
{
	{3,0,2,1},
	{3,0,2,0},
	{3,1,2,0},
	{0},
	{2,0,2,1},
	{0,0,0,0},
	{3,1,3,0},
	{0},
	{2,0,3,1},
	{2,1,3,1},
	{2,1,3,0}
};


boolean R_CheckBBox( bspcontext_t* context, fixed_t* bspcoord )
{
	int32_t		boxx;
	int32_t		boxy;
	int32_t		boxpos;

	fixed_t		x1;
	fixed_t		y1;
	fixed_t		x2;
	fixed_t		y2;

	angle_t		angle1;
	angle_t		angle2;
	angle_t		span;
	angle_t		tspan;

	cliprange_t*	start;

	int32_t			sx1;
	int32_t			sx2;

	// Find the corners of the box
	// that define the edges from current viewpoint.
	if (viewx <= bspcoord[BOXLEFT])
		boxx = 0;
	else if (viewx < bspcoord[BOXRIGHT])
		boxx = 1;
	else
		boxx = 2;
		
	if (viewy >= bspcoord[BOXTOP])
		boxy = 0;
	else if (viewy > bspcoord[BOXBOTTOM])
		boxy = 1;
	else
		boxy = 2;
		
	boxpos = (boxy<<2)+boxx;
	if (boxpos == 5)
	{
		return true;
	}
	
	x1 = bspcoord[checkcoord[boxpos][0]];
	y1 = bspcoord[checkcoord[boxpos][1]];
	x2 = bspcoord[checkcoord[boxpos][2]];
	y2 = bspcoord[checkcoord[boxpos][3]];

	// check clip list for an open space
	angle1 = R_PointToAngle (x1, y1) - viewangle;
	angle2 = R_PointToAngle (x2, y2) - viewangle;
	
	span = angle1 - angle2;

	// Sitting on a line?
	if (span >= ANG180)
		return true;

	tspan = angle1 + clipangle;

	if (tspan > 2*clipangle)
	{
		tspan -= 2*clipangle;

		// Totally off the left edge?
		if (tspan >= span)
			return false;	

		angle1 = clipangle;
	}
	tspan = clipangle - angle2;
	if (tspan > 2*clipangle)
	{
		tspan -= 2*clipangle;

		// Totally off the left edge?
		if (tspan >= span)
			return false;
	
		angle2 = M_NEGATE( clipangle );
	}


	// Find the first clippost
	//  that touches the source post
	//  (adjacent pixels are touching).
	angle1 = (angle1+ANG90)>>RENDERANGLETOFINESHIFT;
	angle2 = (angle2+ANG90)>>RENDERANGLETOFINESHIFT;
	sx1 = viewangletox[angle1];
	sx2 = viewangletox[angle2];

	// Does not cross a pixel.
	if (sx1 == sx2)
		return false;
	sx2--;
	
	start = context->solidsegs;
	while (start->last < sx2)
	{
		start++;
	}

	if (sx1 >= start->first
		&& sx2 <= start->last)
	{
		// The clippost contains the new span.
		return false;
	}

	return true;
}



//
// R_Subsector
// Determine floor/ceiling planes.
// Add sprites of things in sector.
// Draw one or more line segments.
//
void R_Subsector( bspcontext_t* bspcontext, planecontext_t* planecontext, int32_t num )
{
	int32_t			count;
	seg_t*			line;
	subsector_t*	sub;
	
#ifdef RANGECHECK
	if (num>=numsubsectors)
	{
		I_Error ("R_Subsector: ss %i with numss = %i",
				num,
				numsubsectors);
	}
#endif

	sscount++;
	sub = &subsectors[num];
	bspcontext->frontsector = sub->sector;
	count = sub->numlines;
	line = &segs[sub->firstline];

	if (bspcontext->frontsector->floorheight < viewz)
	{
		planecontext->floorplane = R_FindPlane( planecontext, bspcontext->frontsector->floorheight, bspcontext->frontsector->floorpic, bspcontext->frontsector->lightlevel);
	}
	else
	{
		planecontext->floorplane = NULL;
	}

	if (bspcontext->frontsector->ceilingheight > viewz 
		|| bspcontext->frontsector->ceilingpic == skyflatnum)
	{
		planecontext->ceilingplane = R_FindPlane( planecontext, bspcontext->frontsector->ceilingheight, bspcontext->frontsector->ceilingpic, bspcontext->frontsector->lightlevel );
	}
	else
	{
		planecontext->ceilingplane = NULL;
	}
		
	R_AddSprites( bspcontext->frontsector );

	while (count--)
	{
		R_AddLine( bspcontext, planecontext, line );
		line++;
	}

	// check for solidsegs overflow - extremely unsatisfactory!
	if( bspcontext->solidsegsend > &bspcontext->solidsegs[ VANILLA_MAXSEGS ] )
	{
		I_Error("R_Subsector: solidsegs overflow (vanilla may crash here)\n");
	}
}




//
// RenderBSPNode
// Renders all subsectors below a given node,
//  traversing subtree recursively.
// Just call with BSP root.
void R_RenderBSPNode ( bspcontext_t* bspcontext, planecontext_t* planecontext, int32_t bspnum )
{
	node_t*		bsp;
	int32_t		side;

	// Found a subsector?
	if (bspnum & NF_SUBSECTOR)
	{
		if (bspnum == -1)
			R_Subsector ( bspcontext, planecontext, 0 );
		else
			R_Subsector ( bspcontext, planecontext, bspnum&(~NF_SUBSECTOR) );
		return;
	}
		
	bsp = &nodes[bspnum];

	// Decide which side the view point is on.
	side = R_PointOnSide (viewx, viewy, bsp);

	// Recursively divide front space.
	R_RenderBSPNode ( bspcontext, planecontext, bsp->children[side]);

	// Possibly divide back space.
	if ( R_CheckBBox( bspcontext, bsp->bbox[side^1] ) )
	{
		R_RenderBSPNode ( bspcontext, planecontext, bsp->children[side^1]);
	}
}


