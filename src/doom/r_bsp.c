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
//	BSP traversal, handling of LineSegs for rendering.
//




#include "doomdef.h"

#include "m_bbox.h"

#include "i_system.h"

#include "r_main.h"
#include "r_plane.h"
#include "r_segs.h"
#include "r_things.h"

// State.
#include "doomstat.h"
#include "r_state.h"
#include "m_misc.h"
#include "m_profile.h"


//
// R_ClearClipSegs
//
void R_ClearClipSegs ( bspcontext_t* context, int32_t mincol, int32_t maxcol )
{
	context->solidsegs[0].first		= INT_MIN;
	context->solidsegs[0].last		= mincol - 1;
	context->solidsegs[1].first		= maxcol;
	context->solidsegs[1].last		= INT_MAX;
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
void R_ClipSolidWallSegment( vbuffer_t* dest, bspcontext_t* bspcontext, planecontext_t* planecontext, wallcontext_t* wallcontext, int32_t first, int32_t last )
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
			R_StoreWallRange( dest, bspcontext, planecontext, wallcontext, first, last );
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
		R_StoreWallRange( dest, bspcontext, planecontext, wallcontext, first, start->first - 1 );
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
		R_StoreWallRange( dest, bspcontext, planecontext, wallcontext, next->last + 1, (next+1)->first - 1 );
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
	R_StoreWallRange( dest, bspcontext, planecontext, wallcontext, next->last + 1, last );
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
void R_ClipPassWallSegment( vbuffer_t* dest, bspcontext_t* bspcontext, planecontext_t* planecontext, wallcontext_t* wallcontext, int32_t first, int32_t last )
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
			R_StoreWallRange( dest, bspcontext, planecontext, wallcontext, first, last );
			return;
		}
		
		// There is a fragment above *start.
		R_StoreWallRange( dest, bspcontext, planecontext, wallcontext, first, start->first - 1 );
	}

	// Bottom contained in start?
	if (last <= start->last)
	{
		return;
	}

	while (last >= (start+1)->first-1)
	{
		// There is a fragment between two posts.
		R_StoreWallRange( dest, bspcontext, planecontext, wallcontext, start->last + 1, (start+1)->first - 1 );
		++start;
	
		if (last <= start->last)
		{
			return;
		}
	}
	
	// There is a fragment after *next.
	R_StoreWallRange( dest, bspcontext, planecontext, wallcontext, start->last + 1, last );
}

//
// R_AddLine
// Clips the given segment
// and adds any visible pieces to the line list.
//

void R_AddLine( vbuffer_t* dest, bspcontext_t* bspcontext, planecontext_t* planecontext, seg_t* line )
{
	wallcontext_t	wallcontext;

	bspcontext->curline = line;

	// OPTIMIZE: quickly reject orthogonal back sides.
	angle_t angle1 = R_PointToAngle( line->v1->rend.x, line->v1->rend.y );
	angle_t angle2 = R_PointToAngle( line->v2->rend.x, line->v2->rend.y );

	// Clip to view edges.
	// OPTIMIZE: make constant out of 2*clipangle (FIELDOFVIEW).
	angle_t span = angle1 - angle2;

	// Back side? I.e. backface culling?
	if (span >= ANG180)
	{
		return;
	}

	// Global angle needed by segcalc.
	wallcontext.angle1 = angle1;
	wallcontext.angle2 = angle2;
	angle1 -= viewangle;
	angle2 -= viewangle;
	
	angle_t tspan = angle1 + clipangle;
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
	angle1 = ( angle1 + ANG90 ) >> RENDERANGLETOFINESHIFT;
	angle2 = ( angle2 + ANG90 ) >> RENDERANGLETOFINESHIFT;
	int32_t x1 = viewangletox[ angle1 ];
	int32_t x2 = viewangletox[ angle2 ];

	// Does not cross a pixel?
	if (x1 == x2)
	{
		return;
	}
	
	bspcontext->backsector = line->backsector;
	bspcontext->backsectorinst = line->backsector ? &rendsectors[ line->backsector->index ] : NULL;

	// Single sided line?
	if (!bspcontext->backsector)
	{
		goto clipsolid;
	}

	// Closed door.
	if (bspcontext->backsectorinst->ceilheight <= bspcontext->frontsectorinst->floorheight
		|| bspcontext->backsectorinst->floorheight >= bspcontext->frontsectorinst->ceilheight)
	{
		goto clipsolid;
	}

	// Window.
	if (bspcontext->backsectorinst->ceilheight != bspcontext->frontsectorinst->ceilheight
		|| bspcontext->backsectorinst->floorheight != bspcontext->frontsectorinst->floorheight)
	{
		goto clippass;
	}

	// Reject empty lines used for triggers
	//  and special events.
	// Identical floor and ceiling on both sides,
	// identical light levels on both sides,
	// and no middle texture.
	if (bspcontext->backsectorinst->ceiltex == bspcontext->frontsectorinst->ceiltex
		&& bspcontext->backsectorinst->floortex == bspcontext->frontsectorinst->floortex
		&& bspcontext->backsectorinst->lightlevel == bspcontext->frontsectorinst->lightlevel
		&& bspcontext->curline->sidedef->midtexture == 0)
	{
		return;
	}

clippass:
	R_ClipPassWallSegment( dest, bspcontext, planecontext, &wallcontext, x1, x2-1 );
	return;

clipsolid:
	R_ClipSolidWallSegment( dest, bspcontext, planecontext, &wallcontext, x1, x2-1 );
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


boolean R_CheckBBox( bspcontext_t* context, rend_fixed_t* bspcoord )
{
	int32_t			boxx;
	int32_t			boxy;
	int32_t			boxpos;

	rend_fixed_t	x1;
	rend_fixed_t	y1;
	rend_fixed_t	x2;
	rend_fixed_t	y2;

	angle_t			angle1;
	angle_t			angle2;
	angle_t			span;
	angle_t			tspan;

	cliprange_t*	start;

	int32_t			sx1;
	int32_t			sx2;

	// Find the corners of the box
	// that define the edges from current viewpoint.
	if( FixedToRendFixed( viewx ) <= bspcoord[ BOXLEFT ] )
	{
		boxx = 0;
	}
	else if( FixedToRendFixed( viewx ) < bspcoord[ BOXRIGHT ] )
	{
		boxx = 1;
	}
	else
	{
		boxx = 2;
	}
		
	if( FixedToRendFixed( viewy ) >= bspcoord[ BOXTOP ] )
	{
		boxy = 0;
	}
	else if( FixedToRendFixed( viewy ) > bspcoord[ BOXBOTTOM ] )
	{
		boxy = 1;
	}
	else
	{
		boxy = 2;
	}
		
	boxpos = ( boxy << 2 ) + boxx;
	if ( boxpos == 5 )
	{
		return true;
	}
	
	x1 = bspcoord[ checkcoord[ boxpos ][ 0 ] ];
	y1 = bspcoord[ checkcoord[ boxpos ][ 1 ] ];
	x2 = bspcoord[ checkcoord[ boxpos ][ 2 ] ];
	y2 = bspcoord[ checkcoord[ boxpos ][ 3 ] ];

	// check clip list for an open space
	angle1 = R_PointToAngle( x1, y1 ) - viewangle;
	angle2 = R_PointToAngle( x2, y2 ) - viewangle;
	
	span = angle1 - angle2;

	// Sitting on a line?
	if( span >= ANG180 )
		return true;

	tspan = angle1 + clipangle;

	if( tspan > 2 * clipangle )
	{
		tspan -= 2 * clipangle;

		// Totally off the left edge?
		if( tspan >= span )
			return false;

		angle1 = clipangle;
	}

	tspan = clipangle - angle2;
	if( tspan > 2 * clipangle )
	{
		tspan -= 2 * clipangle;

		// Totally off the left edge?
		if( tspan >= span )
			return false;
	
		angle2 = M_NEGATE( clipangle );
	}

	// Find the first clippost
	//  that touches the source post
	//  (adjacent pixels are touching).
	angle1 = ( angle1 + ANG90 ) >> RENDERANGLETOFINESHIFT;
	angle2 = ( angle2 + ANG90 ) >> RENDERANGLETOFINESHIFT;
	sx1 = viewangletox[ angle1 ];
	sx2 = viewangletox[ angle2 ];

	// Does not cross a pixel.
	if( sx1 == sx2 )
		return false;
	--sx2;
	
	start = context->solidsegs;
	while( start->last < sx2 )
	{
		start++;
	}

	if( sx1 >= start->first && sx2 <= start->last )
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
void R_Subsector( vbuffer_t* dest, bspcontext_t* bspcontext, planecontext_t* planecontext, spritecontext_t* spritecontext, int32_t num )
{
	M_PROFILE_PUSH( __FUNCTION__, __FILE__, __LINE__ );
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

	sub = &subsectors[num];
	bspcontext->frontsector = sub->sector;
	bspcontext->frontsectorinst = &rendsectors[ sub->sector->index ];
	count = sub->numlines;
	line = &segs[sub->firstline];

#if RENDER_PERF_GRAPHING
	uint64_t		startaddsprites = I_GetTimeUS();
#endif // RENDER_PERF_GRAPHING
		
	R_AddSprites( spritecontext, bspcontext->frontsector );

#if RENDER_PERF_GRAPHING
	uint64_t		endaddsprites = I_GetTimeUS();
	bspcontext->addspritestimetaken += ( endaddsprites - startaddsprites );

	uint64_t		startaddlines = I_GetTimeUS();
#endif // RENDER_PERF_GRAPHIC

	while (count--)
	{
		R_AddLine( dest, bspcontext, planecontext, line );
		line++;
	}

#if RENDER_PERF_GRAPHING
	uint64_t		endaddlines = I_GetTimeUS();
	bspcontext->addlinestimetaken += ( endaddlines - startaddlines );
#endif // RENDER_PERF_GRAPHING

	// check for solidsegs overflow - extremely unsatisfactory!
	if( !remove_limits && bspcontext->solidsegsend > &bspcontext->solidsegs[ VANILLA_MAXSEGS ] )
	{
		I_Error("R_Subsector: solidsegs overflow (vanilla may crash here)\n");
	}

	M_PROFILE_POP( __FUNCTION__ );
}




//
// RenderBSPNode
// Renders all subsectors below a given node,
//  traversing subtree recursively.
// Just call with BSP root.
void R_RenderBSPNode ( vbuffer_t* dest, bspcontext_t* bspcontext, planecontext_t* planecontext, spritecontext_t* spritecontext, int32_t bspnum )
{
	node_t*		bsp;
	int32_t		side;

	// Found a subsector?
	if( bspnum & NF_SUBSECTOR )
	{
		if( bspnum == -1 )
			R_Subsector ( dest, bspcontext, planecontext, spritecontext, 0 );
		else
			R_Subsector ( dest, bspcontext, planecontext, spritecontext, bspnum & ( ~NF_SUBSECTOR ) );
		return;
	}
		
	bsp = &nodes[ bspnum ];

	// Decide which side the view point is on.
	side = R_PointOnSide( FixedToRendFixed( viewx ), FixedToRendFixed( viewy ), bsp);

	// Recursively divide front space.
	R_RenderBSPNode( dest, bspcontext, planecontext, spritecontext, bsp->children[ side ] );

	// Possibly divide back space.
	if ( R_CheckBBox( bspcontext, bsp->rend.bbox[ side ^ LS_Back ] ) )
	{
		R_RenderBSPNode( dest, bspcontext, planecontext, spritecontext, bsp->children[ side ^ 1 ] );
	}
}


