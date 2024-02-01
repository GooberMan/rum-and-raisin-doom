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
//	Teleportation.
//




#include "doomdef.h"
#include "doomstat.h"

#include "s_sound.h"

#include "p_local.h"
#include "p_lineaction.h"


// Data.
#include "sounds.h"

// State.
#include "r_state.h"



//
// TELEPORTATION
//
int
EV_Teleport
( line_t*	line,
  int		side,
  mobj_t*	thing )
{
    int		i;
    int		tag;
    mobj_t*	m;
    mobj_t*	fog;
    unsigned	an;
    thinker_t*	thinker;
    sector_t*	sector;
    fixed_t	oldx;
    fixed_t	oldy;
    fixed_t	oldz;

    // don't teleport missiles
    if (thing->flags & MF_MISSILE)
	return 0;		

    // Don't teleport if hit back of line,
    //  so you can get out of teleporter.
    if (side == 1)		
	return 0;	

    
    tag = line->tag;
    for (i = 0; i < numsectors; i++)
    {
	if (sectors[ i ].tag == tag )
	{
	    thinker = thinkercap.next;
	    for (thinker = thinkercap.next;
		 thinker != &thinkercap;
		 thinker = thinker->next)
	    {
		// not a mobj
		if (thinker->function.acp1 != (actionf_p1)P_MobjThinker)
		    continue;	

		m = (mobj_t *)thinker;
		
		// not a teleportman
		if (m->type != MT_TELEPORTMAN )
		    continue;		

		sector = m->subsector->sector;
		// wrong sector
		if (sector-sectors != i )
		    continue;	

		oldx = thing->x;
		oldy = thing->y;
		oldz = thing->z;
				
		if (!P_TeleportMove (thing, m->x, m->y))
		    return 0;

                // The first Final Doom executable does not set thing->z
                // when teleporting. This quirk is unique to this
                // particular version; the later version included in
                // some versions of the Id Anthology fixed this.

                if (gameversion != exe_final)
		    thing->z = thing->floorz;

		if (thing->player)
		    thing->player->viewz = thing->z+thing->player->viewheight;

		// spawn teleport fog at source and destination
		fog = P_SpawnMobj (oldx, oldy, oldz, MT_TFOG);
		S_StartSound (fog, sfx_telept);
		an = m->angle >> ANGLETOFINESHIFT;
		fog = P_SpawnMobj (m->x+20*finecosine[an], m->y+20*finesine[an]
				   , thing->z, MT_TFOG);

		// emit sound, where?
		S_StartSound (fog, sfx_telept);
		
		// don't move for a bit
		if (thing->player)
		    thing->reactiontime = 18;	

		thing->angle = m->angle;
		thing->momx = thing->momy = thing->momz = 0;

		thing->teleporttic = gametic;

		return 1;
	    }	
	}
    }
    return 0;
}

DOOM_C_API bool EV_DoTeleportGenericThing( line_t* line, mobj_t* activator, fixed_t& outtargetx, fixed_t& outtargety, angle_t& outtargetangle )
{
	for( sector_t& sector : Sectors() )
	{
		if ( sector.tag == line->tag )
		{
			for( mobj_t* targetmobj = sector.nosectorthinglist; targetmobj != nullptr; targetmobj = targetmobj->nosectornext )
			{
				if( targetmobj->type != MT_TELEPORTMAN )
				{
					continue;
				}

				if( P_TeleportMove( activator, targetmobj->x, targetmobj->y ) )
				{
					outtargetx		= targetmobj->x;
					outtargety		= targetmobj->y;
					outtargetangle	= targetmobj->angle;

					return true;
				}

				break;
			}
		}
	}

	return false;
}

DOOM_C_API bool EV_DoTeleportGenericLine( line_t* line, mobj_t* activator, fixed_t& outtargetx, fixed_t& outtargety, angle_t& outtargetangle )
{
	for( line_t& targetline : Lines() )
	{
		if( targetline.index != line->index
			&& targetline.tag == line->tag
			&& targetline.special == line->special )
		{
			fixed_t sourcemidx = line->v1->x + ( line->dx >> 1 );
			fixed_t sourcemidy = line->v1->y + ( line->dy >> 1 );

			angle_t mobjrelativeviewangle = activator->angle - line->angle;
			angle_t mobjrelativeangle = line->angle - BSP_PointToAngle( sourcemidx, sourcemidy, activator->x, activator->y );
			fixed_t mobjrelativedist = P_AproxDistance( activator->x - sourcemidx, activator->y - sourcemidy );

			fixed_t targetmidx = targetline.v1->x + ( targetline.dx >> 1 );
			fixed_t targetmidy = targetline.v1->y + ( targetline.dy >> 1 );

			angle_t destangle = targetline.angle + mobjrelativeangle + ANG180;
			uint32_t finelookup = FINEANGLE( destangle );
			fixed_t destx = targetmidx + FixedMul( finesine[ finelookup ], mobjrelativedist );
			fixed_t desty = targetmidy + FixedMul( finecosine[ finelookup ], mobjrelativedist );

			if( P_TeleportMove( activator, destx, desty ) )
			{
				outtargetx		= destx;
				outtargety		= desty;
				outtargetangle	= targetline.angle + mobjrelativeviewangle;

				return true;
			}

			break;
		}
	}

	return false;
}

DOOM_C_API int32_t EV_DoTeleportGeneric( line_t* line, mobj_t* activator )
{
	fixed_t oldx = activator->x;
	fixed_t oldy = activator->y;
	fixed_t oldz = activator->z;
	bool teleported = false;

	fixed_t targetx = 0;
	fixed_t targety = 0;
	angle_t targetangle = 0;

	switch( line->action->param1 & tt_targetmask )
	{
	case tt_tothing:
		teleported = EV_DoTeleportGenericThing( line, activator, targetx, targety, targetangle );
		break;

	case tt_toline:
		teleported = EV_DoTeleportGenericLine( line, activator, targetx, targety, targetangle );
		break;
	}

	if( teleported )
	{
		if( gameversion != exe_final )
		{
			activator->z = activator->floorz;
		}

		if( activator->player )
		{
			activator->player->viewz = activator->z + activator->player->viewheight;
		}

		if( ( line->action->param1 & tt_silent ) != tt_silent )
		{
			// spawn teleport fog at source and destination
			mobj_t* fog = P_SpawnMobj( oldx, oldy, oldz, MT_TFOG );
			S_StartSound( fog, sfx_telept );

			angle_t angle = targetangle >> ANGLETOFINESHIFT;
			fog = P_SpawnMobj( targetx + 20 * finecosine[angle], targety + 20 * finesine[angle], activator->z, MT_TFOG );

			// emit sound, where?
			S_StartSound( fog, sfx_telept );
		
			// don't move for a bit
			if( activator->player )
			{
				activator->reactiontime = 18;
			}

			activator->momx = activator->momy = activator->momz = 0;
		}

		switch( line->action->param1 & tt_anglemask )
		{
		case tt_setangle:
		case tt_preserveangle:
			activator->angle = targetangle;
			break;
		case tt_reverseangle:
			activator->angle += ANG180;
			break;
		}

		activator->teleporttic = gametic;
	}

	return teleported;
}
