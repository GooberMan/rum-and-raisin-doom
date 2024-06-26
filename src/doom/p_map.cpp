//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard, Andrey Budko
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
//	Movement, collision handling.
//	Shooting and aiming.
//

#include <stdio.h>
#include <stdlib.h>

#include "doomdef.h"

#include "d_gamesim.h"

#include "deh_misc.h"

#include "i_system.h"
#include "i_log.h"

#include "m_argv.h"
#include "m_bbox.h"
#include "m_container.h"
#include "m_misc.h"
#include "m_random.h"

#include "p_local.h"

#include "s_sound.h"

// State.
#include "doomstat.h"
#include "r_state.h"
#include "d_gameflow.h"
// Data.
#include "sounds.h"

// Spechit overrun magic value.
//
// This is the value used by PrBoom-plus.  I think the value below is 
// actually better and works with more demos.  However, I think
// it's better for the spechits emulation to be compatible with
// PrBoom-plus, at least so that the big spechits emulation list
// on Doomworld can also be used with Chocolate Doom.

#define DEFAULT_SPECHIT_MAGIC 0x01C09C98

// This is from a post by myk on the Doomworld forums, 
// outputted from entryway's spechit_magic generator for
// s205n546.lmp.  The _exact_ value of this isn't too
// important; as long as it is in the right general
// range, it will usually work.  Otherwise, we can use
// the generator (hacked doom2.exe) and provide it 
// with -spechit.

//#define DEFAULT_SPECHIT_MAGIC 0x84f968e8

extern "C"
{
	fixed_t		tmbbox[4];
	mobj_t*		tmthing;
	int		tmflags;
	fixed_t		tmx;
	fixed_t		tmy;


// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
	doombool		floatok;

	fixed_t		tmfloorz;
	fixed_t		tmceilingz;
	fixed_t		tmdropoffz;

// keep track of the line that lowers the ceiling,
// so missiles don't explode against sky hack walls
	line_t*		ceilingline;

// keep track of special lines as they are hit,
// but don't process them until the move is proven valid

	line_t**	spechit = nullptr;
	int32_t		numspechit = 0;
	size_t		spechitcount = 0;
}

static void IncreaseSpecHits()
{
	if( ( numspechit + 1 ) >= spechitcount )
	{
		line_t** oldspechit = spechit;
		size_t oldcount = spechitcount;

		spechitcount += MAXSPECIALCROSS;
		spechit = Z_MallocArrayAs( line_t*, spechitcount, PU_STATIC, nullptr );

		if( oldspechit )
		{
			memcpy( spechit, oldspechit, sizeof( line_t* ) * oldcount );
			Z_Free( oldspechit );
		}
	}
}

//
// TELEPORT MOVE
// 

//
// PIT_StompThing
//
DOOM_C_API doombool PIT_StompThing( mobj_t* thing )
{
    fixed_t	blockdist;
		
    if (!(thing->flags & MF_SHOOTABLE) )
	return true;
		
    blockdist = thing->radius + tmthing->radius;
    
    if ( abs(thing->x - tmx) >= blockdist
	 || abs(thing->y - tmy) >= blockdist )
    {
	// didn't hit it
	return true;
    }
    
    // don't clip against self
    if (thing == tmthing)
	return true;
    
    // monsters don't stomp things except on boss level
    if ( !tmthing->player && current_map->map_num != 30)
	return false;	
		
    P_DamageMobj (thing, tmthing, tmthing, 10000, damage_theworks);
	
    return true;
}


//
// P_TeleportMove
//
DOOM_C_API doombool P_TeleportMove( mobj_t* thing, fixed_t x, fixed_t y )
{
    int			xl;
    int			xh;
    int			yl;
    int			yh;
    int			bx;
    int			by;
    
    subsector_t*	newsubsec;
    
    // kill anything occupying the position
    tmthing = thing;
    tmflags = thing->flags;
	
    tmx = x;
    tmy = y;
	
    tmbbox[BOXTOP] = y + tmthing->radius;
    tmbbox[BOXBOTTOM] = y - tmthing->radius;
    tmbbox[BOXRIGHT] = x + tmthing->radius;
    tmbbox[BOXLEFT] = x - tmthing->radius;

    newsubsec = BSP_PointInSubsector( x, y );
    ceilingline = NULL;
    
    // The base floor/ceiling is from the subsector
    // that contains the point.
    // Any contacted lines the step closer together
    // will adjust them.
    tmfloorz = tmdropoffz = newsubsec->sector->floorheight;
    tmceilingz = newsubsec->sector->ceilingheight;
			
    validcount++;
    numspechit = 0;
    
    // stomp on any things contacted
	xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
	xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
	yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
	yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;

    for (bx=xl ; bx<=xh ; bx++)
	{
		for (by=yl ; by<=yh ; by++)
		{
			if (!P_BlockThingsIterator(bx,by,PIT_StompThing))
			{
				return false;
			}
		}
	}
    
    // the move is ok,
    // so link the thing into its new position
    P_UnsetThingPosition (thing);

    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;	
    thing->x = x;
    thing->y = y;

    P_SetThingPosition (thing);
	
    return true;
}


//
// MOVEMENT ITERATOR FUNCTIONS
//

static void SpechitOverrun(line_t *ld);

//
// PIT_CheckLine
// Adjusts tmfloorz and tmceilingz as lines are contacted
//
DOOM_C_API doombool PIT_CheckLine( line_t* ld )
{
    if (tmbbox[BOXRIGHT] <= ld->bbox[BOXLEFT]
	|| tmbbox[BOXLEFT] >= ld->bbox[BOXRIGHT]
	|| tmbbox[BOXTOP] <= ld->bbox[BOXBOTTOM]
	|| tmbbox[BOXBOTTOM] >= ld->bbox[BOXTOP] )
	return true;

    if (P_BoxOnLineSide (tmbbox, ld) != -1)
	return true;
		
    // A line has been hit
    
    // The moving thing's destination position will cross
    // the given line.
    // If this should not be allowed, return false.
    // If the line is special, keep track of it
    // to process later if the move is proven ok.
    // NOTE: specials are NOT sorted by order,
    // so two special lines that are only 8 pixels apart
    // could be crossed in either order.
    
    if (!ld->backsector)
	return false;		// one sided line
		
    if (!(tmthing->flags & MF_MISSILE) )
    {
		if ( ld->flags & ML_BLOCKING )
			return false;	// explicitly blocking everything

		if ( !tmthing->player )
		{
			if( ld->BlockMonsters() )
			{
				return false;	// block monsters only
			}

			if( sim.mbf21_line_specials
				&& ld->BlockLandMonsters()
				&& !( tmthing->flags & MF_NOGRAVITY ) )
			{
				return false;
			}
		}
		else
		{
			if( sim.mbf21_line_specials
				&& ld->BlockPlayers() )
			{
				return false;
			}
		}
	}

    // set openrange, opentop, openbottom
    P_LineOpening (ld);	
	
    // adjust floor / ceiling heights
    if (opentop < tmceilingz)
    {
	tmceilingz = opentop;
	ceilingline = ld;
    }

    if (openbottom > tmfloorz)
	tmfloorz = openbottom;	

    if (lowfloor < tmdropoffz)
	tmdropoffz = lowfloor;
		
    // if contacted a special line, add it to the list
    if (ld->special)
    {
		IncreaseSpecHits();
        spechit[numspechit] = ld;
		numspechit++;

        // fraggle: spechits overrun emulation code from prboom-plus
        if (!fix.spechit_overflow && numspechit > MAXSPECIALCROSS)
        {
            SpechitOverrun(ld);
        }
    }

    return true;
}

//
// PIT_CheckThing
//
DOOM_C_API doombool PIT_CheckThing( mobj_t* thing )
{
    fixed_t		blockdist;
    doombool		solid;
    int			damage;

	int32_t testflags = MF_SOLID | MF_SPECIAL | MF_SHOOTABLE;
	if( sim.mbf_mobj_flags )
	{
		testflags |= MF_MBF_TOUCHY;
	}

	if ( !(thing->flags & testflags ) )
	{
		return true;
	}

    blockdist = thing->radius + tmthing->radius;

	if ( abs(thing->x - tmx) >= blockdist
		|| abs(thing->y - tmy) >= blockdist )
	{
		// didn't hit it
		return true;	
	}

	// don't clip against self
	if (thing == tmthing)
	{
		return true;
	}

	if( sim.mbf_mobj_flags
		&& thing->flags & MF_MBF_TOUCHY
		&& thing->health > 0
		&& tmthing->z <= thing->z + thing->height
		&& tmthing->z + tmthing->height >= thing->z )
	{
		P_DamageMobjEx( thing, tmthing, tmthing, 10000, damage_theworks );
		return true;
	}

	// check for skulls slamming into things
	if (tmthing->flags & MF_SKULLFLY)
	{
		damage = ((P_Random()%8)+1)*tmthing->info->damage;
	
		P_DamageMobj (thing, tmthing, tmthing, damage, damage_melee);
	
		tmthing->flags &= ~MF_SKULLFLY;
		tmthing->momx = tmthing->momy = tmthing->momz = 0;
	
		P_SetMobjState(tmthing, (statenum_t)tmthing->info->spawnstate);
	
		return false;		// stop moving
	}

	bool dobouncescheck = sim.mbf_mobj_flags
						&& (tmthing->flags & MF_MBF_BOUNCES)
						&& tmthing->target != nullptr;
						
	// missiles can hit other things
	if( (tmthing->flags & MF_MISSILE)
		|| dobouncescheck )
	{
		// see if it went over / under
		if (tmthing->z > thing->z + thing->height)
			return true;		// overhead
		if (tmthing->z+tmthing->height < thing->z)
			return true;		// underneath
		
		if (tmthing->target
			&& ProjectileImmunity( tmthing->target, thing ) )
		{
			// Don't hit same species as originator.
			if (thing == tmthing->target)
			{
				return true;
			}

			// sdh: Add deh_species_infighting here.  We can override the
			// "monsters of the same species cant hurt each other" behavior
			// through dehacked patches
			if (thing->type != MT_PLAYER && !deh_species_infighting)
			{
				// Explode, but do no damage.
				// Let players missile other players.
				return false;
			}
		}
	
		if (! (thing->flags & MF_SHOOTABLE) )
		{
			// didn't do any damage
			return !(thing->flags & MF_SOLID);	
		}
	
		// damage / explode
		damage = ( (P_Random()%8 ) + 1 ) * tmthing->info->damage;
		P_DamageMobj( thing, tmthing, tmthing->target, damage, damage_none );

		// don't traverse any more
		return false;
	}

	// check for special pickup
	if (thing->flags & MF_SPECIAL)
	{
		solid = (thing->flags & MF_SOLID) != 0;
		if (tmflags&MF_PICKUP)
		{
			// can remove thing
			P_TouchSpecialThing (thing, tmthing);
		}
		return !solid;
	}
	
	return !(thing->flags & MF_SOLID);
}


//
// MOVEMENT CLIPPING
//

//
// P_CheckPosition
// This is purely informative, nothing is modified
// (except things picked up).
// 
// in:
//  a mobj_t (can be valid or invalid)
//  a position to be checked
//   (doesn't need to be related to the mobj_t->x,y)
//
// during:
//  special things are touched if MF_PICKUP
//  early out on solid lines?
//
// out:
//  newsubsec
//  floorz
//  ceilingz
//  tmdropoffz
//   the lowest point contacted
//   (monsters won't move to a dropoff)
//  speciallines[]
//  numspeciallines
//
DOOM_C_API doombool P_CheckPosition( mobj_t* thing, fixed_t x, fixed_t y, checkposflags_t flags )
{
    int			xl;
    int			xh;
    int			yl;
    int			yh;
    int			bx;
    int			by;
    subsector_t*	newsubsec;

    tmthing = thing;
    tmflags = thing->flags;
	
    tmx = x;
    tmy = y;
	
    tmbbox[BOXTOP] = y + tmthing->radius;
    tmbbox[BOXBOTTOM] = y - tmthing->radius;
    tmbbox[BOXRIGHT] = x + tmthing->radius;
    tmbbox[BOXLEFT] = x - tmthing->radius;

    newsubsec = BSP_PointInSubsector( x, y );
    ceilingline = NULL;
    
    // The base floor / ceiling is from the subsector
    // that contains the point.
    // Any contacted lines the step closer together
    // will adjust them.
    tmfloorz = tmdropoffz = newsubsec->sector->floorheight;
    tmceilingz = newsubsec->sector->ceilingheight;
			
    validcount++;
    numspechit = 0;

    if ( tmflags & MF_NOCLIP )
	{
		return true;
	}

	bool checkthings = !sim.corpse_ignores_mobjs
					|| ( tmthing->flags & MF_CORPSE ) != MF_CORPSE
					|| ( flags & CP_CorpseChecksAlways ) == CP_CorpseChecksAlways;
	if( checkthings )
	{
		// Check things first, possibly picking things up.
		// The bounding box is extended by MAXRADIUS
		// because mobj_ts are grouped into mapblocks
		// based on their origin point, and can overlap
		// into adjacent blocks by up to MAXRADIUS units.
		xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
		xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
		yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
		yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;

		for (bx=xl ; bx<=xh ; bx++)
		{
			for (by=yl ; by<=yh ; by++)
			{
				if (!P_BlockThingsIterator(bx,by,PIT_CheckThing))
				{
					return false;
				}
			}
		}
	}
    
    // check lines
	xl = (tmbbox[BOXLEFT] - bmaporgx)>>MAPBLOCKSHIFT;
	xh = (tmbbox[BOXRIGHT] - bmaporgx)>>MAPBLOCKSHIFT;
	yl = (tmbbox[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
	yh = (tmbbox[BOXTOP] - bmaporgy)>>MAPBLOCKSHIFT;

    for (bx=xl ; bx<=xh ; bx++)
	for (by=yl ; by<=yh ; by++)
	    if (!P_BlockLinesIterator (bx,by,PIT_CheckLine))
		return false;

    return true;
}


//
// P_TryMove
// Attempt to move to a new position,
// crossing special lines unless MF_TELEPORT is set.
//
DOOM_C_API doombool P_TryMove( mobj_t* thing, fixed_t x, fixed_t y )
{
    fixed_t	oldx;
    fixed_t	oldy;
    int		side;
    int		oldside;
    line_t*	ld;

    floatok = false;
    if (!P_CheckPosition (thing, x, y))
	{
		return false;		// solid wall or thing
	}
    
    if ( !(thing->flags & MF_NOCLIP) )
    {
		if (tmceilingz - tmfloorz < thing->height)
			return false;	// doesn't fit

		floatok = true;
	
		if ( !(thing->flags&MF_TELEPORT) 
			 &&tmceilingz - thing->z < thing->height)
			return false;	// mobj must lower itself to fit

		if ( !(thing->flags&MF_TELEPORT)
			 && tmfloorz - thing->z > 24*FRACUNIT )
			return false;	// too big a step up

		doombool hasvelocity = thing->momx != 0 || thing->momy != 0;

		if( !sim.mobjs_slide_off_edge || !hasvelocity )
		{
			if ( !(thing->flags&(MF_DROPOFF|MF_FLOAT))
				 && tmfloorz - tmdropoffz > 24*FRACUNIT )
				return false;	// don't stand over a dropoff
		}
    }
    
    // the move is ok,
    // so link the thing into its new position
    P_UnsetThingPosition (thing);

    oldx = thing->x;
    oldy = thing->y;
    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;	
    thing->x = x;
    thing->y = y;

    P_SetThingPosition (thing);
    
    // if any special lines were hit, do the effect
    if (! (thing->flags&(MF_TELEPORT|MF_NOCLIP)) )
    {
		while (numspechit--)
		{
			// see if the line was crossed
			ld = spechit[numspechit];
			side = P_PointOnLineSide (thing->x, thing->y, ld);
			oldside = P_PointOnLineSide (oldx, oldy, ld);
			if (side != oldside)
			{
				if (ld->special)
				{
					P_CrossSpecialLine (ld, oldside, thing);
				}
			}
		}
    }

    return true;
}


//
// P_ThingHeightClip
// Takes a valid thing and adjusts the thing->floorz,
// thing->ceilingz, and possibly thing->z.
// This is called for all nearby monsters
// whenever a sector changes height.
// If the thing doesn't fit,
// the z will be set to the lowest value
// and false will be returned.
//
DOOM_C_API doombool P_ThingHeightClip( mobj_t* thing )
{
    doombool		onfloor;
	
    onfloor = (thing->z == thing->floorz);
	
    P_CheckPosition (thing, thing->x, thing->y);	
    // what about stranding a monster partially off an edge?
	
    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;
	
    if (onfloor)
    {
	// walking monsters rise and fall with the floor
	thing->z = thing->floorz;
    }
    else
    {
	// don't adjust a floating monster unless forced to
	if (thing->z+thing->height > thing->ceilingz)
	    thing->z = thing->ceilingz - thing->height;
    }
	
    if (thing->ceilingz - thing->floorz < thing->height)
	return false;
		
    return true;
}



//
// SLIDE MOVE
// Allows the player to slide along any angled walls.
//
fixed_t		bestslidefrac;
fixed_t		secondslidefrac;

line_t*		bestslideline;
line_t*		secondslideline;

mobj_t*		slidemo;

fixed_t		tmxmove;
fixed_t		tmymove;



//
// P_HitSlideLine
// Adjusts the xmove / ymove
// so that the next move will slide along the wall.
//
DOOM_C_API void P_HitSlideLine( line_t* ld )
{
    int			side;

    angle_t		lineangle;
    angle_t		moveangle;
    angle_t		deltaangle;
    
    fixed_t		movelen;
    fixed_t		newlen;
	
	
    if (ld->slopetype == ST_HORIZONTAL)
    {
	tmymove = 0;
	return;
    }
    
    if (ld->slopetype == ST_VERTICAL)
    {
	tmxmove = 0;
	return;
    }
	
    side = P_PointOnLineSide (slidemo->x, slidemo->y, ld);
	
    lineangle = BSP_PointToAngle (0,0, ld->dx, ld->dy);

    if (side == 1)
	lineangle += ANG180;

    moveangle = BSP_PointToAngle (0,0, tmxmove, tmymove);
    deltaangle = moveangle-lineangle;

    if (deltaangle > ANG180)
	deltaangle += ANG180;
    //	I_Error ("SlideLine: ang>ANG180");

    lineangle >>= ANGLETOFINESHIFT;
    deltaangle >>= ANGLETOFINESHIFT;
	
    movelen = P_AproxDistance (tmxmove, tmymove);
    newlen = FixedMul (movelen, finecosine[deltaangle]);

    tmxmove = FixedMul (newlen, finecosine[lineangle]);	
    tmymove = FixedMul (newlen, finesine[lineangle]);	
}


//
// PTR_SlideTraverse
//
DOOM_C_API doombool PTR_SlideTraverse( intercept_t* in )
{
    line_t*	li;
	
    if (!in->isaline)
	I_Error ("PTR_SlideTraverse: not a line?");
		
    li = in->d.line;
    
    if ( ! (li->flags & ML_TWOSIDED) )
    {
	if (P_PointOnLineSide (slidemo->x, slidemo->y, li))
	{
	    // don't hit the back side
	    return true;		
	}
	goto isblocking;
    }

    // set openrange, opentop, openbottom
    P_LineOpening (li);
    
    if (openrange < slidemo->height)
	goto isblocking;		// doesn't fit
		
    if (opentop - slidemo->z < slidemo->height)
	goto isblocking;		// mobj is too high

    if (openbottom - slidemo->z > 24*FRACUNIT )
	goto isblocking;		// too big a step up

    // this line doesn't block movement
    return true;		
	
    // the line does block movement,
    // see if it is closer than best so far
  isblocking:		
    if (in->frac < bestslidefrac)
    {
	secondslidefrac = bestslidefrac;
	secondslideline = bestslideline;
	bestslidefrac = in->frac;
	bestslideline = li;
    }
	
    return false;	// stop
}



//
// P_SlideMove
// The momx / momy move is bad, so try to slide
// along a wall.
// Find the first line hit, move flush to it,
// and slide along it
//
// This is a kludgy mess.
//
DOOM_C_API void P_SlideMove( mobj_t* mo )
{
    fixed_t		leadx;
    fixed_t		leady;
    fixed_t		trailx;
    fixed_t		traily;
    fixed_t		newx;
    fixed_t		newy;
    int			hitcount;
		
    slidemo = mo;
    hitcount = 0;
    
  retry:
    if (++hitcount == 3)
	goto stairstep;		// don't loop forever

    
    // trace along the three leading corners
    if (mo->momx > 0)
    {
	leadx = mo->x + mo->radius;
	trailx = mo->x - mo->radius;
    }
    else
    {
	leadx = mo->x - mo->radius;
	trailx = mo->x + mo->radius;
    }
	
    if (mo->momy > 0)
    {
	leady = mo->y + mo->radius;
	traily = mo->y - mo->radius;
    }
    else
    {
	leady = mo->y - mo->radius;
	traily = mo->y + mo->radius;
    }
		
    bestslidefrac = FRACUNIT+1;
	
    P_PathTraverse ( leadx, leady, leadx+mo->momx, leady+mo->momy,
		     PT_ADDLINES, PTR_SlideTraverse );
    P_PathTraverse ( trailx, leady, trailx+mo->momx, leady+mo->momy,
		     PT_ADDLINES, PTR_SlideTraverse );
    P_PathTraverse ( leadx, traily, leadx+mo->momx, traily+mo->momy,
		     PT_ADDLINES, PTR_SlideTraverse );
    
    // move up to the wall
    if (bestslidefrac == FRACUNIT+1)
    {
	// the move most have hit the middle, so stairstep
      stairstep:
	if (!P_TryMove (mo, mo->x, mo->y + mo->momy))
	    P_TryMove (mo, mo->x + mo->momx, mo->y);
	return;
    }

    // fudge a bit to make sure it doesn't hit
    bestslidefrac -= 0x800;	
    if (bestslidefrac > 0)
    {
	newx = FixedMul (mo->momx, bestslidefrac);
	newy = FixedMul (mo->momy, bestslidefrac);
	
	if (!P_TryMove (mo, mo->x+newx, mo->y+newy))
	    goto stairstep;
    }
    
    // Now continue along the wall.
    // First calculate remainder.
    bestslidefrac = FRACUNIT-(bestslidefrac+0x800);
    
    if (bestslidefrac > FRACUNIT)
	bestslidefrac = FRACUNIT;
    
    if (bestslidefrac <= 0)
	return;
    
    tmxmove = FixedMul (mo->momx, bestslidefrac);
    tmymove = FixedMul (mo->momy, bestslidefrac);

    P_HitSlideLine( bestslideline );	// clip the moves

    mo->momx = tmxmove;
    mo->momy = tmymove;
		
    if (!P_TryMove (mo, mo->x+tmxmove, mo->y+tmymove))
    {
	goto retry;
    }
}


//
// P_LineAttack
//
extern "C"
{
	mobj_t*		linetarget;	// who got hit (or NULL)
	mobj_t*		shootthing;

	// Height if not aiming up or down
	// ???: use slope for monsters?
	fixed_t		shootz;	

	int32_t		la_damage;
	damage_t	la_damageflags;
	fixed_t		attackrange;

	fixed_t		aimslope;

	// slopes to top and bottom of target
	extern fixed_t	topslope;
	extern fixed_t	bottomslope;
}

//
// PTR_AimTraverse
// Sets linetaget and aimslope when a target is aimed at.
//
DOOM_C_API doombool PTR_AimTraverse( intercept_t* in )
{
    line_t*		li;
    mobj_t*		th;
    fixed_t		slope;
    fixed_t		thingtopslope;
    fixed_t		thingbottomslope;
    fixed_t		dist;
		
    if (in->isaline)
    {
	li = in->d.line;
	
	if ( !(li->flags & ML_TWOSIDED) )
	    return false;		// stop
	
	// Crosses a two sided line.
	// A two sided line will restrict
	// the possible target ranges.
	P_LineOpening (li);
	
	if (openbottom >= opentop)
	    return false;		// stop
	
	dist = FixedMul (attackrange, in->frac);

        if (li->backsector == NULL
         || li->frontsector->floorheight != li->backsector->floorheight)
	{
	    slope = FixedDiv (openbottom - shootz , dist);
	    if (slope > bottomslope)
		bottomslope = slope;
	}
		
	if (li->backsector == NULL
         || li->frontsector->ceilingheight != li->backsector->ceilingheight)
	{
	    slope = FixedDiv (opentop - shootz , dist);
	    if (slope < topslope)
		topslope = slope;
	}
		
	if (topslope <= bottomslope)
	    return false;		// stop
			
	return true;			// shot continues
    }
    
    // shoot a thing
    th = in->d.thing;
    if (th == shootthing)
	return true;			// can't shoot self
    
    if (!(th->flags&MF_SHOOTABLE))
	return true;			// corpse or something

    // check angles to see if the thing can be aimed at
    dist = FixedMul (attackrange, in->frac);
    thingtopslope = FixedDiv (th->z+th->height - shootz , dist);

    if (thingtopslope < bottomslope)
	return true;			// shot over the thing

    thingbottomslope = FixedDiv (th->z - shootz, dist);

    if (thingbottomslope > topslope)
	return true;			// shot under the thing
    
    // this thing can be hit!
    if (thingtopslope > topslope)
	thingtopslope = topslope;
    
    if (thingbottomslope < bottomslope)
	thingbottomslope = bottomslope;

    aimslope = (thingtopslope+thingbottomslope)/2;
    linetarget = th;

    return false;			// don't go any farther
}


//
// PTR_ShootTraverse
//
DOOM_C_API doombool PTR_ShootTraverse( intercept_t* in )
{
    fixed_t		x;
    fixed_t		y;
    fixed_t		z;
    fixed_t		frac;
    
    line_t*		li;
    
    mobj_t*		th;

    fixed_t		slope;
    fixed_t		dist;
    fixed_t		thingtopslope;
    fixed_t		thingbottomslope;
		
    if (in->isaline)
    {
		li = in->d.line;
	
		if (li->special)
			P_ShootSpecialLine (shootthing, li);

		if ( !(li->flags & ML_TWOSIDED) )
			goto hitline;
	
		// crosses a two sided line
		P_LineOpening (li);
		
		dist = FixedMul (attackrange, in->frac);

		// e6y: emulation of missed back side on two-sided lines.
		// backsector can be NULL when emulating missing back side.

		if (li->backsector == NULL)
		{
			slope = FixedDiv (openbottom - shootz , dist);
			if (slope > aimslope)
				goto hitline;

			slope = FixedDiv (opentop - shootz , dist);
			if (slope < aimslope)
				goto hitline;
		}
		else
		{
			if (li->frontsector->floorheight != li->backsector->floorheight)
			{
				slope = FixedDiv (openbottom - shootz , dist);
				if (slope > aimslope)
					goto hitline;
			}

			if (li->frontsector->ceilingheight != li->backsector->ceilingheight)
			{
				slope = FixedDiv (opentop - shootz , dist);
				if (slope < aimslope)
					goto hitline;
			}
		}

		// shot continues
		return true;
	
		// hit line
	hitline:
		// position a bit closer
		frac = in->frac - FixedDiv (4*FRACUNIT,attackrange);
		x = trace.x + FixedMul (trace.dx, frac);
		y = trace.y + FixedMul (trace.dy, frac);
		z = shootz + FixedMul (aimslope, FixedMul(frac, attackrange));

		if (li->frontsector->CeilingTexture()->IsSky())
		{
			// don't shoot the sky!
			if (z > li->frontsector->ceilingheight)
			return false;

			// it's a sky hack wall
			if	(li->backsector && li->backsector->CeilingTexture()->IsSky())
			{
				doombool impact = false;
				if( fix.sky_wall_projectiles )
				{
					impact = z <= li->backsector->ceilingheight;
				}

				if( !impact )
				{
					return false;
				}
			}
		}

		// Spawn bullet puffs.
		P_SpawnPuff (x,y,z);
	
		// don't go any farther
		return false;	
    }
    
    // shoot a thing
    th = in->d.thing;
    if (th == shootthing)
	return true;		// can't shoot self
    
    if (!(th->flags&MF_SHOOTABLE))
	return true;		// corpse or something
		
    // check angles to see if the thing can be aimed at
    dist = FixedMul (attackrange, in->frac);
    thingtopslope = FixedDiv (th->z+th->height - shootz , dist);

    if (thingtopslope < aimslope)
	return true;		// shot over the thing

    thingbottomslope = FixedDiv (th->z - shootz, dist);

    if (thingbottomslope > aimslope)
	return true;		// shot under the thing

    
    // hit thing
    // position a bit closer
    frac = in->frac - FixedDiv (10*FRACUNIT,attackrange);

    x = trace.x + FixedMul (trace.dx, frac);
    y = trace.y + FixedMul (trace.dy, frac);
    z = shootz + FixedMul (aimslope, FixedMul(frac, attackrange));

    // Spawn bullet puffs or blod spots,
    // depending on target type.
    if (in->d.thing->flags & MF_NOBLOOD)
		P_SpawnPuff (x,y,z);
    else
		P_SpawnBlood (x,y,z, la_damage);

    if (la_damage)
	{
		P_DamageMobj (th, shootthing, shootthing, la_damage, la_damageflags);
	}

    // don't go any farther
    return false;
	
}


//
// P_AimLineAttack
//
DOOM_C_API fixed_t P_AimLineAttack( mobj_t* t1, angle_t angle, fixed_t distance )
{
    fixed_t	x2;
    fixed_t	y2;

    t1 = P_SubstNullMobj(t1);
	
    angle >>= ANGLETOFINESHIFT;
    shootthing = t1;
    
    x2 = t1->x + (distance>>FRACBITS)*finecosine[angle];
    y2 = t1->y + (distance>>FRACBITS)*finesine[angle];
    shootz = t1->z + (t1->height>>1) + 8*FRACUNIT;

    // can't shoot outside view angles
	// Note: Chocolate Doom used the SCREENHEIGHT/SCREENWIDTH defines, which
	// I wrapped in to render width and height to handle arbitrary resolutions.
	// But I do believe that'll break vanilla compatbility. So forcing vanilla
	// values right here.

    topslope = (VANILLA_SCREENHEIGHT/2)*FRACUNIT/(VANILLA_SCREENWIDTH/2);
    bottomslope = -(VANILLA_SCREENHEIGHT/2)*FRACUNIT/(VANILLA_SCREENWIDTH/2);

    
    attackrange = distance;
    linetarget = NULL;
	
    P_PathTraverse ( t1->x, t1->y,
		     x2, y2,
		     PT_ADDLINES|PT_ADDTHINGS,
		     PTR_AimTraverse );
		
    if (linetarget)
	return aimslope;

    return 0;
}
 

//
// P_LineAttack
// If damage == 0, it is just a test trace
// that will leave linetarget set.
//
DOOM_C_API mobj_t* P_LineAttack( mobj_t* t1, angle_t angle, fixed_t distance, fixed_t slope, int damage, damage_t damageflags )
{
    fixed_t	x2;
    fixed_t	y2;
	
    angle >>= ANGLETOFINESHIFT;
    shootthing = t1;
    la_damage = damage;
	la_damageflags = damageflags;
    x2 = t1->x + (distance>>FRACBITS)*finecosine[angle];
    y2 = t1->y + (distance>>FRACBITS)*finesine[angle];
    shootz = t1->z + (t1->height>>1) + 8*FRACUNIT;
    attackrange = distance;
    aimslope = slope;
		
    P_PathTraverse( t1->x, t1->y,
					x2, y2,
					PT_ADDLINES|PT_ADDTHINGS,
					PTR_ShootTraverse );

	return linetarget;
}
 


//
// USE LINES
//
mobj_t*		usething;

DOOM_C_API doombool PTR_UseTraverse( intercept_t* in )
{
    int		side;
	
    if (!in->d.line->special)
    {
	P_LineOpening (in->d.line);
	if (openrange <= 0)
	{
	    S_StartSound (usething, sfx_noway);
	    
	    // can't use through a wall
	    return false;	
	}
	// not a special line, but keep checking
	return true ;		
    }
	
    side = 0;
    if (P_PointOnLineSide (usething->x, usething->y, in->d.line) == 1)
	side = 1;
    
    //	return false;		// don't use back side
	
    P_UseSpecialLine (usething, in->d.line, side);

	if( sim.boom_line_specials )
	{
		return in->d.line->Passthrough();
	}
    // can't use for than one special line in a row
    return false;
}


//
// P_UseLines
// Looks for special lines in front of the player to activate.
//
DOOM_C_API void P_UseLines( player_t* player )
{
    int		angle;
    fixed_t	x1;
    fixed_t	y1;
    fixed_t	x2;
    fixed_t	y2;
	
    usething = player->mo;
		
    angle = player->mo->angle >> ANGLETOFINESHIFT;

    x1 = player->mo->x;
    y1 = player->mo->y;
    x2 = x1 + (USERANGE>>FRACBITS)*finecosine[angle];
    y2 = y1 + (USERANGE>>FRACBITS)*finesine[angle];
	
    P_PathTraverse ( x1, y1, x2, y2, PT_ADDLINES, PTR_UseTraverse );
}


//
// RADIUS ATTACK
//
mobj_t*		bombsource;
mobj_t*		bombspot;
int32_t		bombdamage;
damage_t	bombdamageflags;


//
// PIT_RadiusAttack
// "bombsource" is the creature
// that caused the explosion at "bombspot".
//
DOOM_C_API doombool PIT_RadiusAttack( mobj_t* thing )
{
	fixed_t	dx;
	fixed_t	dy;
	fixed_t	dist;
	
	if (!(thing->flags & MF_SHOOTABLE) )
	{
		return true;
	}

    // Boss spider and cyborg
    // take no damage from concussion.
	if( SplashImmunity( bombsource, bombspot, thing ) )
	{
		return true;
	}

    dx = abs(thing->x - bombspot->x);
    dy = abs(thing->y - bombspot->y);
    
    dist = dx>dy ? dx : dy;
    dist = (dist - thing->radius) >> FRACBITS;

    if (dist < 0)
	dist = 0;

    if (dist >= bombdamage)
	return true;	// out of range

    if ( P_CheckSight (thing, bombspot) )
    {
		// must be in direct path
		P_DamageMobj (thing, bombspot, bombsource, bombdamage - dist, bombdamageflags);
    }
    
    return true;
}


//
// P_RadiusAttack
// Source is the creature that caused the explosion at spot.
//
DOOM_C_API void P_RadiusAttack( mobj_t* spot, mobj_t* source, int32_t damage )
{
    int		x;
    int		y;
    
    int		xl;
    int		xh;
    int		yl;
    int		yh;
    
    fixed_t	dist;
	
    dist = (damage+MAXRADIUS)<<FRACBITS;
	yh = (spot->y + dist - bmaporgy)>>MAPBLOCKSHIFT;
	yl = (spot->y - dist - bmaporgy)>>MAPBLOCKSHIFT;
	xh = (spot->x + dist - bmaporgx)>>MAPBLOCKSHIFT;
	xl = (spot->x - dist - bmaporgx)>>MAPBLOCKSHIFT;

    bombspot = spot;
    bombsource = source;
    bombdamage = damage;
	bombdamageflags = damage_none;
	
    for (y=yl ; y<=yh ; y++)
	for (x=xl ; x<=xh ; x++)
	    P_BlockThingsIterator (x, y, PIT_RadiusAttack );
}

DOOM_C_API void P_RadiusAttackDistance( mobj_t* spot, mobj_t* source, int32_t damage, fixed_t distance )
{
	int32_t yh = (spot->y + distance - bmaporgy) >> MAPBLOCKSHIFT;
	int32_t yl = (spot->y - distance - bmaporgy) >> MAPBLOCKSHIFT;
	int32_t xh = (spot->x + distance - bmaporgx) >> MAPBLOCKSHIFT;
	int32_t xl = (spot->x - distance - bmaporgx) >> MAPBLOCKSHIFT;

	P_BlockThingsIteratorHorizontal( iota( xl, xh + 1 ), iota( yl, yh + 1 ), [ &spot, &source, &damage, &distance ]( mobj_t* mobj ) -> bool
	{
		if( (mobj->flags & MF_SHOOTABLE)
			&& ( source->ForceSplashDamage()
				|| ( mobj->type != MT_CYBORG
					&& mobj->type != MT_SPIDER
					&& !source->NoSplashDamage()
					)
				) )
		{
			fixed_t dx = FixedAbs( mobj->x - spot->x );
			fixed_t dy = FixedAbs( mobj->y - spot->y );

			fixed_t dist = dx > dy ? dx : dy;
			dist = M_MAX( ( ( dist - mobj->radius ) >> FRACBITS ), 0 );

			if( dist < distance && P_CheckSight( mobj, spot ))
			{
				// must be in direct path
				P_DamageMobj( mobj, spot, source, FixedMul( damage, FixedDiv( dist, distance ) ), damage_none );
			}
		}

		return true;
	} );
}

DOOM_C_API mobj_t* P_FindTracerTarget( mobj_t* source, fixed_t distance, angle_t fov )
{
	mobj_t* target = nullptr;
	fixed_t currbestdist = distance + 1;

	auto findtarget = [ &source, &target, &currbestdist, &fov ]( mobj_t* mobj ) -> bool
	{
		constexpr int32_t requiredflags = MF_SHOOTABLE | MF_COUNTKILL;
		if( mobj != source
			&& (mobj->flags & requiredflags) == requiredflags
			&& !( netgame && !deathmatch && mobj->player ) )
		{
			fixed_t dx = FixedAbs( mobj->x - source->x );
			fixed_t dy = FixedAbs( mobj->y - source->y );

			fixed_t dist = P_AproxDistance( dx, dy );
			angle_t angle = BSP_PointToAngle( source->x, source->y, mobj->x, mobj->y ) - mobj->angle;
			bool withinfov = angle == 0
							|| ( angle > fov && angle < M_NEGATE( fov ) );

			if( dist < currbestdist && withinfov && P_CheckSight( mobj, source ) )
			{
				target = mobj;
				currbestdist = dist;
			}
		}
		return true;
	};

	int32_t thisx = ( source->x - bmaporgx ) >> MAPBLOCKSHIFT;
	int32_t thisy = ( source->y - bmaporgy ) >> MAPBLOCKSHIFT;
	P_BlockThingsIterator( thisx, thisy, findtarget );
	if( target )
	{
		return target;
	}

	int32_t yh = (source->y + distance - bmaporgy) >> MAPBLOCKSHIFT;
	int32_t yl = (source->y - distance - bmaporgy) >> MAPBLOCKSHIFT;
	int32_t xh = (source->x + distance - bmaporgx) >> MAPBLOCKSHIFT;
	int32_t xl = (source->x - distance - bmaporgx) >> MAPBLOCKSHIFT;

	for( int32_t y : iota( yl, yh + 1 ) )
	{
		for( int32_t x : iota( xl, xh + 1 ) )
		{
			if( x == thisx && y == thisy )
			{
				continue;
			}
			P_BlockThingsIterator( x, y, findtarget );
		}
	}

	return target;
}

//
// P_ChangeSector
//
DOOM_C_API doombool P_ChangeSectorFixed( sector_t* sector, doombool crunch )
{
	doombool doesntfit = false;

	P_BlockThingsIteratorVertical( iota( sector->blockbox[ BOXLEFT ], sector->blockbox[ BOXRIGHT ] + 1 ),
							iota( sector->blockbox[ BOXBOTTOM ], sector->blockbox[ BOXTOP ] + 1 ),
							[ sector, &doesntfit, crunch ]( mobj_t* mobj ) -> bool
	{
		if( fix.overzealous_changesector && !P_MobjOverlapsSector( sector, mobj ) )
		{
			return true;
		}

		if( P_ThingHeightClip( mobj ) )
		{
			return true;
		}

		// crunch bodies to giblets
		if( mobj->health <= 0 )
		{
			P_SetMobjState( mobj, S_GIBS );

			if( gameversion > exe_doom_1_2 )
			{
				mobj->flags &= ~MF_SOLID;
			}

			mobj->height = 0;
			mobj->radius = 0;

			// keep checking
			return true;
		}

		// crunch dropped items
		if( mobj->flags & MF_DROPPED )
		{
			P_RemoveMobj( mobj );
	
			// keep checking
			return true;
		}

		if ( !( mobj->flags & MF_SHOOTABLE ) )
		{
			// assume it is bloody gibs or something
			return true;
		}

		doesntfit = true;

		if( crunch && !( leveltime & 3 ) )
		{
			P_DamageMobj( mobj, NULL, NULL, 10, damage_none );

			// spray blood in a random direction
			mobj_t* blood = P_SpawnMobj( mobj->x, mobj->y, mobj->z + (mobj->height >> 1), MT_BLOOD );
			blood->momx = P_SubRandom() << 12;
			blood->momy = P_SubRandom() << 12;
		}

		// keep checking (crush other things)	
		return true;
	} );

	return doesntfit;
}

//
// SECTOR HEIGHT CHANGING
// After modifying a sectors floor or ceiling height,
// call this routine to adjust the positions
// of all things that touch the sector.
//
// If anything doesn't fit anymore, true will be returned.
// If crunch is true, they will take damage
//  as they are being crushed.
// If Crunch is false, you should set the sector height back
//  the way it was and call P_ChangeSector again
//  to undo the changes.
//
doombool		crushchange;
doombool		nofit;


//
// PIT_ChangeSector
//
DOOM_C_API doombool PIT_ChangeSector( mobj_t* thing )
{
    mobj_t*	mo;
	
    if (P_ThingHeightClip (thing))
    {
	// keep checking
	return true;
    }
    

    // crunch bodies to giblets
    if (thing->health <= 0)
    {
	P_SetMobjState (thing, S_GIBS);

    if (gameversion > exe_doom_1_2)
	    thing->flags &= ~MF_SOLID;
	thing->height = 0;
	thing->radius = 0;

	// keep checking
	return true;		
    }

    // crunch dropped items
    if (thing->flags & MF_DROPPED)
    {
	P_RemoveMobj (thing);
	
	// keep checking
	return true;		
    }

    if (! (thing->flags & MF_SHOOTABLE) )
    {
	// assume it is bloody gibs or something
	return true;			
    }
    
    nofit = true;

    if (crushchange && !(leveltime&3) )
    {
	P_DamageMobj(thing,NULL,NULL,10,damage_none);

	// spray blood in a random direction
	mo = P_SpawnMobj (thing->x,
			  thing->y,
			  thing->z + thing->height/2, MT_BLOOD);
	
	mo->momx = P_SubRandom() << 12;
	mo->momy = P_SubRandom() << 12;
    }

    // keep checking (crush other things)	
    return true;	
}

DOOM_C_API doombool P_ChangeSector( sector_t* sector, doombool crunch )
{
	if( fix.overzealous_changesector || fix.spechit_overflow )
	{
		return P_ChangeSectorFixed( sector, crunch );
	}

    int		x;
    int		y;
	
    nofit = false;
    crushchange = crunch;
	
    // re-check heights for all things near the moving sector
    for (x=sector->blockbox[BOXLEFT] ; x<= sector->blockbox[BOXRIGHT] ; x++)
	for (y=sector->blockbox[BOXBOTTOM];y<= sector->blockbox[BOXTOP] ; y++)
	    P_BlockThingsIterator (x, y, PIT_ChangeSector);
	
	
    return nofit;
}

// Code to emulate the behavior of Vanilla Doom when encountering an overrun
// of the spechit array.  This is by Andrey Budko (e6y) and comes from his
// PrBoom plus port.  A big thanks to Andrey for this.

static void SpechitOverrun(line_t *ld)
{
    static unsigned int baseaddr = 0;
    unsigned int addr;
   
    if (baseaddr == 0)
    {
        int p;

        // This is the first time we have had an overrun.  Work out
        // what base address we are going to use.
        // Allow a spechit value to be specified on the command line.

        //!
        // @category compat
        // @arg <n>
        //
        // Use the specified magic value when emulating spechit overruns.
        //

        p = M_CheckParmWithArgs("-spechit", 1);
        
        if (p > 0)
        {
            M_StrToInt(myargv[p+1], (int *) &baseaddr);
        }
        else
        {
            baseaddr = DEFAULT_SPECHIT_MAGIC;
        }
    }
    
    // Calculate address used in doom2.exe

    addr = baseaddr + (ld - lines) * 0x3E;

    switch(numspechit)
    {
        case 9: 
        case 10:
        case 11:
        case 12:
            tmbbox[numspechit-9] = addr;
            break;
        case 13: 
            crushchange = addr; 
            break;
        case 14: 
            nofit = addr; 
            break;
        default:
            I_LogAddEntryVar( Log_Error, "SpechitOverrun: Warning: unable to emulate"
                            "an overrun where numspechit=%i",
                            numspechit);
            break;
    }
}

