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
//	Moving object handling. Spawn functions.
//

#include <stdio.h>

#include "doomdef.h"
#include "doomstat.h"

#include "d_gameconf.h"
#include "d_gamesim.h"

#include "hu_stuff.h"

#include "i_system.h"
#include "i_log.h"

#include "m_random.h"
#include "m_misc.h"

#include "p_local.h"

#include "s_sound.h"
#include "sounds.h"

#include "st_stuff.h"

#include "z_zone.h"

DOOM_C_API extern musinfo_t*		musinfo;

DOOM_C_API void G_PlayerReborn (int player);
DOOM_C_API mobj_t* P_SpawnMapThing (mapthing_t*	mthing);


//
// P_SetMobjState
// Returns true if the mobj is still present.
//

// Use a heuristic approach to detect infinite state cycles: Count the number
// of times the loop in P_SetMobjState() executes and exit with an error once
// an arbitrary very large limit is reached.

#define MOBJ_CYCLE_LIMIT 1000000

DOOM_C_API doombool P_SetMobjState( mobj_t* mobj, int32_t state )
{
    const state_t*	st;
    int	cycle_counter = 0;

    do
    {
		if (state == S_NULL)
		{
			mobj->state = nullptr;
			P_RemoveMobj( mobj );
			return false;
		}

		st = &states[state];
		mobj->state = st;
		mobj->tics = st->Tics();
		mobj->sprite = st->sprite;
		mobj->frame = st->frame;

		// Modified handling.
		// Call action functions when the state is set
		if (st->action.Valid())
			st->action( mobj );
	
		state = st->nextstate;

		if (cycle_counter++ > MOBJ_CYCLE_LIMIT)
		{
			I_Error("P_SetMobjState: Infinite state cycle detected!");
		}
	} while (!mobj->tics);

	return true;
}


//
// P_ExplodeMissile  
//
DOOM_C_API void P_ExplodeMissile (mobj_t* mo)
{
    mo->momx = mo->momy = mo->momz = 0;

    P_SetMobjState(mo, (statenum_t)mo->info->deathstate);

    mo->tics -= P_Random()&3;

    if (mo->tics < 1)
	mo->tics = 1;

    mo->flags &= ~MF_MISSILE;

    if (mo->info->deathsound)
	S_StartSound (mo, mo->info->deathsound);
}


//
// P_XYMovement  
//

DOOM_C_API mobj_t* P_XYMovement( mobj_t* mo )
{ 	
	fixed_t 	ptryx;
	fixed_t		ptryy;
	if (!mo->momx && !mo->momy)
	{
		if (mo->flags & MF_SKULLFLY)
		{
			// the skull slammed into something
			mo->flags &= ~MF_SKULLFLY;
			mo->momx = mo->momy = mo->momz = 0;

			P_SetMobjState(mo, (statenum_t)mo->info->spawnstate);
		}
		return mo;
	}
	
	player_t* player = mo->player;
	sector_t* prevsector = player ? player->mo->subsector->sector : nullptr;

	mo->momx = M_CLAMP( mo->momx, -MAXMOVE, MAXMOVE );
	mo->momy = M_CLAMP( mo->momy, -MAXMOVE, MAXMOVE );

	fixed_t xmove = mo->momx;
	fixed_t ymove = mo->momy;

	do
	{
		fixed_t xtest = comp.doom_movement_clipping_method ? xmove : FixedAbs(xmove);
		fixed_t ytest = comp.doom_movement_clipping_method ? ymove : FixedAbs(ymove);

		if (xtest > MAXMOVE/2 || ytest > MAXMOVE/2)
		{
			ptryx = mo->x + xmove/2;
			ptryy = mo->y + ymove/2;
			xmove >>= 1;
			ymove >>= 1;
		}
		else
		{
			ptryx = mo->x + xmove;
			ptryy = mo->y + ymove;
			xmove = ymove = 0;
		}
		
		if (!P_TryMove (mo, ptryx, ptryy))
		{
			// blocked move
			if (mo->player)
			{	// try to slide along it
				P_SlideMove (mo);
			}
			else if (mo->flags & MF_MISSILE)
			{
				// explode a missile
				if (ceilingline &&
					ceilingline->backsector &&
					ceilingline->backsector->CeilingTexture()->IsSky())
				{
					doombool remove = true;
					if( fix.sky_wall_projectiles )
					{
						remove = mo->z >= ceilingline->backsector->ceilingheight;
					}

					if( remove )
					{
						// Hack to prevent missiles exploding
						// against the sky.
						// Does not handle sky floors.
						P_RemoveMobj (mo);
						return NULL;
					}
				}
				P_ExplodeMissile (mo);
			}
			else
			{
				mo->momx = mo->momy = 0;
			}
		}
	} while (xmove || ymove);

	// slow down
	if (player && player->cheats & CF_NOMOMENTUM)
	{
		// debug option for no sliding at all
		mo->momx = mo->momy = 0;
		return mo;
	}

	if (mo->flags & (MF_MISSILE | MF_SKULLFLY) )
		return mo; 	// no friction for missiles ever

	if (mo->z > mo->floorz)
		return mo;		// no friction when airborne

	if (mo->flags & MF_CORPSE)
	{
		// do not stop sliding
		//  if halfway off a step with some momentum
		if (mo->momx > FRACUNIT/4
			|| mo->momx < -FRACUNIT/4
			|| mo->momy > FRACUNIT/4
			|| mo->momy < -FRACUNIT/4)
		{
			if (mo->floorz != mo->subsector->sector->floorheight)
				return mo;
		}
	}

	if (mo->momx > -STOPSPEED
		&& mo->momx < STOPSPEED
		&& mo->momy > -STOPSPEED
		&& mo->momy < STOPSPEED
		&& (!player
			|| (player->cmd.forwardmove== 0
			&& player->cmd.sidemove == 0 ) ) )
	{
		// if in a walking frame, stop moving
		if ( player && (unsigned)(player->mo->state->statenum - S_PLAY_RUN1) < 4)
		{
			if( !P_SetMobjState (player->mo, S_PLAY) )
			{
				return mo;
			}
		}
	
		mo->momx = 0;
		mo->momy = 0;
	}
	else
	{
		fixed_t friction = mo->z == mo->subsector->sector->floorheight
						&& ( mo->flags & ( MF_CORPSE | MF_NOCLIP ) ) == 0
							? mo->subsector->sector->Friction()
							: FRICTION;
		mo->momx = FixedMul( mo->momx, friction );
		mo->momy = FixedMul( mo->momy, friction );
	}

	if( comp.support_musinfo
		&& player
		&& player->mo->subsector->sector != prevsector )
	{
		for( mobj_t* check = player->mo->subsector->sector->nosectorthinglist; check != nullptr; check = check->nosectornext )
		{
			if( check->info->type == MT_MUSICSOURCE )
			{
				musinfo->QueueTrack( check->info->doomednum - MusInfoDoomedNum );
			}
		}
	}

	return mo;
}

//
// P_ZMovement
//
DOOM_C_API mobj_t* P_ZMovement( mobj_t* mo )
{
	auto GetBounceVel = []( int32_t flags, fixed_t vel )
	{
		fixed_t newvel = -vel;
		if( flags & MF_MISSILE )
		{
			constexpr fixed_t bounceamount = FixedDiv( IntToFixed( 50 ), IntToFixed( 100 ) );
			newvel = FixedMul( newvel, bounceamount );
		}
		else if( ( flags & ( MF_FLOAT | MF_DROPOFF ) ) == ( MF_FLOAT | MF_DROPOFF ) )
		{
			constexpr fixed_t bounceamount = FixedDiv( IntToFixed( 875 ), IntToFixed( 1000 ) );
			newvel = FixedMul( newvel, bounceamount );
		}
		else if( flags & MF_FLOAT )
		{
			constexpr fixed_t bounceamount = FixedDiv( IntToFixed( 75 ), IntToFixed( 100 ) );
			newvel = FixedMul( newvel, bounceamount );
		}
		else
		{
			constexpr fixed_t bounceamount = FixedDiv( IntToFixed( 50 ), IntToFixed( 100 ) );
			newvel = FixedMul( newvel, bounceamount );
		}

		return newvel;
	};

	// check for smooth step up
	if (mo->player && mo->z < mo->floorz)
	{
		mo->player->viewheight -= mo->floorz-mo->z;

		mo->player->deltaviewheight
			= (VIEWHEIGHT - mo->player->viewheight)>>3;
	}

	// adjust height
	mo->z += mo->momz;
	
	if ( mo->flags & MF_FLOAT
		&& mo->target)
	{
		// float down towards target if too close
		if ( !(mo->flags & MF_SKULLFLY)
				&& !(mo->flags & MF_INFLOAT) )
		{
			fixed_t dist = P_AproxDistance (mo->x - mo->target->x,
											mo->y - mo->target->y);

			fixed_t delta = (mo->target->z + (mo->height>>1)) - mo->z;

			if (delta<0 && dist < -(delta*3) )
			mo->z -= FLOATSPEED;
			else if (delta>0 && dist < (delta*3) )
			mo->z += FLOATSPEED;			
		}
	
	}

	bool bounces = sim.mbf_mobj_flags && ( mo->flags & MF_MBF_BOUNCES );
	// clip movement
	if (mo->z <= mo->floorz)
	{
		if( sim.mbf_mobj_flags
			&& mo->momz != 0
			&& ( mo->flags & MF_MBF_TOUCHY ) )
		{
			P_DamageMobjEx( mo, mo, mo, 10000, damage_theworks );
			return nullptr;
		}
		// hit the floor

		if (comp.lost_souls_bounce && mo->flags & MF_SKULLFLY)
		{
			// the skull slammed into something
			mo->momz = -mo->momz;
		}
	
		if( bounces )
		{
			fixed_t gravity = GRAVITY;
			if( mo->LowGravity() )
			{
				gravity /= 8;
			}

			fixed_t floordiff = mo->floorz - mo->floorz;
			mo->momz = GetBounceVel( mo->flags, mo->momz ) - gravity;
			mo->z = mo->floorz + floordiff;
		}
		else if (mo->momz < 0)
		{
			if (mo->player && mo->momz < -GRAVITY * 8)
			{
				// Squat down.
				// Decrease viewheight for a moment
				// after hitting the ground (hard),
				// and utter appropriate sound.
				mo->player->deltaviewheight = mo->momz>>3;
				S_StartSound (mo, sfx_oof);
			}
			mo->momz = 0;
		}
		mo->z = mo->floorz;

		if (!comp.lost_souls_bounce && mo->flags & MF_SKULLFLY)
		{
			mo->momz = -mo->momz;
		}

		if ( (mo->flags & MF_MISSILE)
			 && !bounces
			 && !(mo->flags & MF_NOCLIP) )
		{
			P_ExplodeMissile (mo);
			return nullptr;
		}
	}
	else if (! (mo->flags & MF_NOGRAVITY) )
	{
		fixed_t gravity = GRAVITY;
		if (mo->momz == 0)
		{
			gravity *= 2;
		}
		if( mo->LowGravity() )
		{
			gravity /= 8;
		}

		mo->momz -= gravity;
	}
	
	if (mo->z + mo->height > mo->ceilingz)
	{
		// hit the ceiling
		if( bounces )
		{
			mo->momz = GetBounceVel( mo->flags, mo->momz );
		}
		else if (mo->momz > 0)
		{
			mo->momz = 0;
		}

		mo->z = mo->ceilingz - mo->height;

		if (mo->flags & MF_SKULLFLY)
		{	// the skull slammed into something
			mo->momz = -mo->momz;
		}
	
		if ( (mo->flags & MF_MISSILE)
			 && !bounces
			 && !(mo->flags & MF_NOCLIP) )
		{
			P_ExplodeMissile (mo);
			return nullptr;
		}
	}

	return mo;
} 



//
// P_NightmareRespawn
//
DOOM_C_API void P_NightmareRespawn( mobj_t* mobj )
{
    fixed_t		x;
    fixed_t		y;
    fixed_t		z; 
    subsector_t*	ss; 
    mobj_t*		mo;
    mapthing_t*		mthing;

	if( !comp.respawn_non_map_things_at_origin && !mobj->hasspawnpoint )
	{
		x = mobj->x;
		y = mobj->y;
	}
	else
	{
		x = mobj->spawnpoint.x << FRACBITS; 
		y = mobj->spawnpoint.y << FRACBITS; 
	}

    // somthing is occupying it's position?
    if (!P_CheckPosition (mobj, x, y, CP_CorpseChecksAlways) )
	{
		return;	// no respwan
	}

    // spawn a teleport fog at old spot
    // because of removal of the body?
    mo = P_SpawnMobj (mobj->x,
		      mobj->y,
		      mobj->subsector->sector->floorheight , MT_TFOG); 
    // initiate teleport sound
    S_StartSound (mo, sfx_telept);

    // spawn a teleport fog at the new spot
    ss = BSP_PointInSubsector( x, y );

    mo = P_SpawnMobj (x, y, ss->sector->floorheight , MT_TFOG); 

    S_StartSound (mo, sfx_telept);

    // spawn the new monster
    mthing = &mobj->spawnpoint;
	
    // spawn it
    if (mobj->info->flags & MF_SPAWNCEILING)
	z = ONCEILINGZ;
    else
	z = ONFLOORZ;

    // inherit attributes from deceased one
    mo = P_SpawnMobj (x,y,z, mobj->type);
    mo->spawnpoint = mobj->spawnpoint;	
	mo->hasspawnpoint = true;
    mo->prev.angle = mo->curr.angle = mo->angle = ANG45 * (mthing->angle/45);

    if (mthing->options & MTF_AMBUSH)
	mo->flags |= MF_AMBUSH;

    mo->reactiontime = 18;
	
    // remove the old monster,
    P_RemoveMobj (mobj);
}


//
// P_MobjThinker
//
DOOM_C_API void P_MobjThinker( mobj_t* mobj )
{
	// momentum movement
	if (mobj->momx
		|| mobj->momy
		|| (mobj->flags&MF_SKULLFLY) )
	{
		P_XYMovement (mobj);

		if (!mobj->thinker.function.Valid())
		{
			return;
		}
	}
	if ( (mobj->z != mobj->floorz)
		|| mobj->momz )
	{
		P_ZMovement (mobj);
	
		if (!mobj->thinker.function.Valid())
		{
			return;
		}
	}

	// Player handles special sectors in its own thinker
	if( sim.generic_specials_handling && !mobj->player )
	{
		P_MobjInSectorGeneric( mobj );
		if (!mobj->thinker.function.Valid())
		{
			return;
		}
	}

	// cycle through states,
	// calling action functions at transitions
	if (mobj->tics != -1)
	{
		mobj->tics--;
		
		// you can cycle through multiple states in a tic
		if (!mobj->tics)
		{
			if (!P_SetMobjState (mobj, mobj->state->nextstate) )
			{
				return;		// freed itself
			}
		}
	}
	else
	{
		// check for nightmare respawn
		if ( !(mobj->flags & MF_COUNTKILL)
			|| mobj->NoRespawn() )
			return;

		if (!respawnmonsters)
			return;

		mobj->movecount++;

		if (mobj->movecount < mobj->MinRespawnTics())
			return;

		if ( leveltime&31 )
			return;

		if (P_Random () > mobj->RespawnDice())
			return;

		P_NightmareRespawn (mobj);
	}

}

DOOM_C_API mobj_t* P_SpawnMobjEx( const mobjinfo_t* typeinfo, angle_t angle,
									fixed_t x, fixed_t y, fixed_t z,
									fixed_t forwardvel, fixed_t rightvel, fixed_t upvel )
{
	mobj_t*	mobj;
	const state_t* st;
	
	mobj = (mobj_t*)Z_Malloc( sizeof(*mobj), PU_LEVEL, NULL );
	memset (mobj, 0, sizeof (*mobj));
	
	mobj->type = typeinfo->type;
	mobj->info = typeinfo;
	mobj->x = x;
	mobj->y = y;
	mobj->angle = angle;
	mobj->radius = typeinfo->radius;
	mobj->height = typeinfo->height;
	mobj->flags = typeinfo->flags;
	mobj->flags2 = typeinfo->flags2;
	mobj->rnr24flags = typeinfo->rnr24flags;
	mobj->health = typeinfo->spawnhealth;

	int32_t forwardlookup = FINEANGLE( angle );
	int32_t rightlookup = FINEANGLE( angle - ANG90 );

	mobj->momx = FixedMul( forwardvel, finecosine[ forwardlookup ] ) + FixedMul( forwardvel, finesine[ rightlookup ] );
	mobj->momy = FixedMul( rightvel, finecosine[ rightlookup ] ) + FixedMul( rightvel, finesine[ forwardlookup ] );
	mobj->momz = upvel;

	if (gameskill != sk_nightmare)
	{
		mobj->reactiontime = typeinfo->reactiontime;
	}

	mobj->lastlook = P_Random () % MAXPLAYERS;
	// do not set the state with P_SetMobjState,
	// because action routines can not be called yet
	st = &states[typeinfo->spawnstate];

	mobj->state = st;
	mobj->tics = st->tics;
	mobj->sprite = st->sprite;
	mobj->frame = st->frame;

	// set subsector and/or block links
	P_SetThingPosition( mobj );
	
	mobj->floorz = mobj->subsector->sector->floorheight;
	mobj->ceilingz = mobj->subsector->sector->ceilingheight;

	if (z == ONFLOORZ)
		mobj->z = mobj->floorz;
	else if (z == ONCEILINGZ)
		mobj->z = mobj->ceilingz - mobj->info->height;
	else 
		mobj->z = z;

	mobj->lumpindex = -1;

	mobj->curr.x = FixedToRendFixed( mobj->x );
	mobj->curr.y = FixedToRendFixed( mobj->y );
	mobj->curr.z = FixedToRendFixed( mobj->z );
	mobj->curr.angle = mobj->angle;
	mobj->curr.frame = -1;
	mobj->curr.sprite = SPR_INVALID;

	mobj->prev = mobj->curr;

	mobj->curr.frame = mobj->frame;
	mobj->curr.sprite = mobj->sprite;

	mobj->thinker.function = P_MobjThinker;

	mobj->tested_sector = (uint8_t*)Z_MallocZero( sizeof( uint8_t ) * numsectors, PU_LEVEL, nullptr );
	mobj->translation = mobj->info->translation;
	
	P_AddThinker( &mobj->thinker );

	return mobj;
}

//
// P_SpawnMobj
//
DOOM_C_API mobj_t* P_SpawnMobj( fixed_t x, fixed_t y, fixed_t z, int32_t type )
{
	return P_SpawnMobjEx( &mobjinfo[ type ], 0, x, y, z, 0, 0, 0 );
}


//
// P_RemoveMobj
//
extern "C"
{
	mapthing_t	itemrespawnque[ITEMQUESIZE];
	int		itemrespawntime[ITEMQUESIZE];
	int		iquehead;
	int		iquetail;
}

DOOM_C_API void P_RemoveMobj (mobj_t* mobj)
{
    if ((mobj->flags & MF_SPECIAL)
	&& !(mobj->flags & MF_DROPPED)
	&& (mobj->type != MT_INV)
	&& (mobj->type != MT_INS))
    {
	itemrespawnque[iquehead] = mobj->spawnpoint;
	itemrespawntime[iquehead] = leveltime;
	iquehead = (iquehead+1)&(ITEMQUESIZE-1);

	// lose one off the end?
	if (iquehead == iquetail)
	    iquetail = (iquetail+1)&(ITEMQUESIZE-1);
    }
	
    // unlink from sector and block lists
    P_UnsetThingPosition (mobj);
    
    // stop any playing sound
    S_StopSound (mobj);
    
    // free block
    P_RemoveThinker (&mobj->thinker);
}




//
// P_RespawnSpecials
//
DOOM_C_API void P_RespawnSpecials (void)
{
    fixed_t		x;
    fixed_t		y;
    fixed_t		z;
    
    subsector_t*	ss; 
    mobj_t*		mo;
    mapthing_t*		mthing;

    // only respawn items in deathmatch
    if (deathmatch != 2)
	return;	// 

    // nothing left to respawn?
    if (iquehead == iquetail)
	return;		

    // wait at least 30 seconds
    if (leveltime - itemrespawntime[iquetail] < 30*TICRATE)
	return;			

    mthing = &itemrespawnque[iquetail];
	
    x = mthing->x << FRACBITS; 
    y = mthing->y << FRACBITS; 
	  
    // spawn a teleport fog at the new spot
    ss = BSP_PointInSubsector( x, y );
    mo = P_SpawnMobj (x, y, ss->sector->floorheight , MT_IFOG); 
    S_StartSound (mo, sfx_itmbk);

    // find which type to spawn
	const mobjinfo_t* spawninfo = mapobjects[ mthing->type ];
	bool unknown = !spawninfo
				|| ( spawninfo->minimumversion >= exe_boom_2_02 && !sim.boom_things )
				|| ( spawninfo->minimumversion >= exe_mbf && !sim.mbf_things );
	if( unknown )
	{
        I_Error("P_RespawnSpecials: Failed to find mobj type with doomednum "
                "%d when respawning thing. This would cause a buffer overrun "
                "in vanilla Doom", mthing->type);
    }

    // spawn it
    if (spawninfo->flags & MF_SPAWNCEILING)
	z = ONCEILINGZ;
    else
	z = ONFLOORZ;

    mo = P_SpawnMobjEx( spawninfo, 0, x, y , z, 0, 0, 0 );
    mo->spawnpoint = *mthing;
	mo->hasspawnpoint = true;
    mo->prev.angle = mo->curr.angle = mo->angle = ANG45 * (mthing->angle/45);

    // pull it from the que
    iquetail = (iquetail+1)&(ITEMQUESIZE-1);
}




//
// P_SpawnPlayer
// Called when a player is spawned on the level.
// Most of the player structure stays unchanged
//  between levels.
//
DOOM_C_API void P_SpawnPlayer (mapthing_t* mthing)
{
    player_t*		p;
    fixed_t		x;
    fixed_t		y;
    fixed_t		z;

    mobj_t*		mobj;

    int			i;

    if (mthing->type == 0)
    {
        return;
    }

    // not playing?
    if (!playeringame[mthing->type-1])
	return;					
		
    p = &players[mthing->type-1];

    if (p->playerstate == PST_REBORN)
	G_PlayerReborn (mthing->type-1);

    x 		= mthing->x << FRACBITS;
    y 		= mthing->y << FRACBITS;
    z		= ONFLOORZ;
    mobj	= P_SpawnMobj(x,y,z, MT_PLAYER);

    // set color translations for player sprites
    if (mthing->type > 1)
	{
		int32_t translationindex = ( mthing->type - 1 );
		mobj->flags |= translationindex << MF_TRANSSHIFT;
		mobj->translation = R_GetTranslation( gameconf->playertranslations[ translationindex ] );
	}
	else
	{
		mobj->translation = R_GetTranslation( gameconf->playertranslations[ 0 ] );
	}

    mobj->prev.angle = mobj->curr.angle = mobj->angle = ANG45 * (mthing->angle/45);
    mobj->player = p;
    mobj->health = p->health;

    p->mo = mobj;
    p->playerstate = PST_LIVE;	
    p->refire = 0;
    p->message = NULL;
    p->damagecount = 0;
    p->bonuscount = 0;
    p->extralight = 0;
    p->fixedcolormap = 0;
    p->viewheight = VIEWHEIGHT;
	p->viewz = mobj->z + p->viewheight;
	p->prevviewz = p->currviewz = FixedToRendFixed( p->viewz );
	p->anymomentumframes = 0;

    // setup gun psprite
    P_SetupPsprites (p);
    
    // give all cards in death match mode
    if (deathmatch)
	for (i=0 ; i<NUMCARDS ; i++)
	    p->cards[i] = true;
			
    if (mthing->type-1 == consoleplayer)
    {
	// wake up the status bar
	ST_Start ();
	// wake up the heads up text
	HU_Start ();		
    }
}


//
// P_SpawnMapThing
// The fields of the mapthing should
// already be in host byte order.
//
DOOM_C_API mobj_t* P_SpawnMapThing (mapthing_t* mthing)
{
    int			bit;
    mobj_t*		mobj;
    fixed_t		x;
    fixed_t		y;
    fixed_t		z;
		
    // count deathmatch start positions
    if (mthing->type == 11)
    {
		if (deathmatch_p < &deathmatchstarts[10])
		{
			memcpy (deathmatch_p, mthing, sizeof(*mthing));
			deathmatch_p++;
		}
		return nullptr;
    }

    if (mthing->type == -1 || mthing->type == 0)
    {
        // Thing type 0 is actually "player -1 start".  
        // For some reason, Vanilla Doom accepts/ignores this.

        return nullptr;
    }
	
    // check for players specially
    if (mthing->type >= 1 && mthing->type <= 4)
    {
		// save spots for respawning in network games
		playerstarts[mthing->type-1] = *mthing;
		playerstartsingame[mthing->type-1] = true;
		if (!deathmatch)
			P_SpawnPlayer (mthing);

		return nullptr;
    }

    // check for apropriate skill level
    if( ( !netgame && (mthing->options & MTF_MULTIPLAYER_ONLY) )
		|| ( netgame && (mthing->options & MTF_BOOM_NOT_IN_DEATHMATCH) && deathmatch == 1 )
		|| ( netgame && (mthing->options & MTF_BOOM_NOT_IN_COOP) && deathmatch == 0 ) )
	{
		return nullptr;
	}

    if (gameskill == sk_baby)
	{
		bit = 1;
	}
    else if (gameskill == sk_nightmare)
	{
		bit = 4;
	}
    else
	{
		bit = 1<<(gameskill-1);
	}

    if (!(mthing->options & bit) )
	{
		return nullptr;
	}
	
    // find which type to spawn
	const mobjinfo_t* spawninfo = mapobjects[ mthing->type ];
	bool unknown = !spawninfo
				|| ( spawninfo->minimumversion >= exe_boom_2_02 && !sim.boom_things )
				|| ( spawninfo->minimumversion >= exe_mbf && !sim.mbf_things );
	if( unknown )
	{
		if( sim.allow_unknown_thing_types )
		{
			I_LogAddEntryVar( Log_Error, "P_SpawnMapThing: Unknown type %i at (%i, %i)",
				 mthing->type,
				 mthing->x, mthing->y );
			return nullptr;
		}

		I_Error ("P_SpawnMapThing: Unknown type %i at (%i, %i)",
			 mthing->type,
			 mthing->x, mthing->y);
	}

    // don't spawn keycards and players in deathmatch
    if (deathmatch && spawninfo->flags & MF_NOTDMATCH)
	return nullptr;
		
	// don't spawn any monsters if -nomonsters
	if (nomonsters
		&& ( spawninfo->type == MT_SKULL
				|| (spawninfo->flags & MF_COUNTKILL)) )
	{
		return nullptr;
	}
    
    // spawn it
    x = mthing->x << FRACBITS;
    y = mthing->y << FRACBITS;

    if (spawninfo->flags & MF_SPAWNCEILING)
	z = ONCEILINGZ;
    else
	z = ONFLOORZ;
    
    mobj = P_SpawnMobjEx( spawninfo, 0, x, y , z, 0, 0, 0 );
    mobj->spawnpoint = *mthing;
	mobj->hasspawnpoint = true;

    if (mobj->tics > 0)
	{
		mobj->tics = 1 + (P_Random () % mobj->tics);
	}

	if (mobj->flags & MF_COUNTKILL)
	{
		++session.start_total_monsters;
		totalkills++;
	}
	if (mobj->flags & MF_COUNTITEM)
	{
		++session.start_total_items;
		totalitems++;
	}
		
    mobj->prev.angle = mobj->curr.angle = mobj->angle = ANG45 * (mthing->angle/45);

    if (mthing->options & MTF_AMBUSH)
	{
		mobj->flags |= MF_AMBUSH;
	}

	return mobj;
}



//
// GAME SPAWN FUNCTIONS
//


//
// P_SpawnPuff
//
extern "C"
{
	extern fixed_t attackrange;
}

DOOM_C_API void P_SpawnPuff( fixed_t x, fixed_t y, fixed_t z )
{
    mobj_t*	th;
	
    z += (P_SubRandom() << 10);

    th = P_SpawnMobj (x,y,z, MT_PUFF);
    th->momz = FRACUNIT;
    th->tics -= P_Random()&3;

    if (th->tics < 1)
	th->tics = 1;
	
    // don't make punches spark on the wall
    if (attackrange == MELEERANGE)
	P_SetMobjState (th, S_PUFF3);
}



//
// P_SpawnBlood
// 
DOOM_C_API void P_SpawnBlood( fixed_t x, fixed_t y, fixed_t z, int damage )
{
    mobj_t*	th;
	
    z += (P_SubRandom() << 10);
    th = P_SpawnMobj (x,y,z, MT_BLOOD);
    th->momz = FRACUNIT*2;
    th->tics -= P_Random()&3;

    if (th->tics < 1)
	th->tics = 1;
		
    if (damage <= 12 && damage >= 9)
	P_SetMobjState (th,S_BLOOD2);
    else if (damage < 9)
	P_SetMobjState (th,S_BLOOD3);
}



//
// P_CheckMissileSpawn
// Moves the missile forward a bit
//  and possibly explodes it right there.
//
DOOM_C_API void P_CheckMissileSpawn( mobj_t* th )
{
    th->tics -= P_Random()&3;
    if (th->tics < 1)
	{
		th->tics = 1;
	}
    
	if( fix.missiles_on_blockmap )
	{
		// move a little forward so an angle can
		// be computed if it immediately explodes
		fixed_t newx = th->x + (th->momx>>1);
		fixed_t newy = th->y + (th->momy>>1);
		//fixed_t newz = th->z + (th->momz>>1);

		if( !P_TryMove( th, newx, newy ) )
		{
			P_ExplodeMissile (th);
		}
		else
		{
			P_ZMovement( th );
		}
	}
	else
	{
		// move a little forward so an angle can
		// be computed if it immediately explodes
		th->x += (th->momx>>1);
		th->y += (th->momy>>1);
		th->z += (th->momz>>1);

		if (!P_TryMove (th, th->x, th->y))
			P_ExplodeMissile (th);
	}
}

// Certain functions assume that a mobj_t pointer is non-NULL,
// causing a crash in some situations where it is NULL.  Vanilla
// Doom did not crash because of the lack of proper memory 
// protection. This function substitutes NULL pointers for
// pointers to a dummy mobj, to avoid a crash.

DOOM_C_API mobj_t *P_SubstNullMobj( mobj_t *mobj )
{
    if (mobj == NULL)
    {
        static mobj_t dummy_mobj = {};

        mobj = &dummy_mobj;
    }

    return mobj;
}

//
// P_SpawnMissile
//
DOOM_C_API mobj_t* P_SpawnMissile( mobj_t* source, mobj_t* dest, int32_t type )
{
    mobj_t*	th;
    angle_t	an;
    int		dist;

    th = P_SpawnMobj (source->x,
		      source->y,
		      source->z + 4*8*FRACUNIT, type);
    
    if (th->info->seesound)
	S_StartSound (th, th->info->seesound);

    th->target = source;	// where it came from
    an = BSP_PointToAngle (source->x, source->y, dest->x, dest->y);

    // fuzzy player
    if (dest->flags & MF_SHADOW)
	an += P_SubRandom() << 20;

    th->angle = an;
    an >>= ANGLETOFINESHIFT;
    th->momx = FixedMul (th->Speed(), finecosine[an]);
    th->momy = FixedMul (th->Speed(), finesine[an]);
	
    dist = P_AproxDistance (dest->x - source->x, dest->y - source->y);
    dist = dist / th->Speed();

    if (dist < 1)
	dist = 1;

    th->momz = (dest->z - source->z) / dist;
    P_CheckMissileSpawn (th);
	
    return th;
}


//
// P_SpawnPlayerMissile
// Tries to aim at a nearby monster
//
DOOM_C_API mobj_t* P_SpawnPlayerMissileEx( mobj_t* source, int32_t type, angle_t angle, angle_t pitch
										, fixed_t xoffset, fixed_t yoffset, fixed_t zoffset, doombool settracer )
{
	constexpr fixed_t AimDistance = IntToFixed( 1024 );

	// see which target is to be aimed at
	angle_t an = angle;
	fixed_t slope = P_AimLineAttack( source, an, AimDistance );

	if (!linetarget)
	{
		an += 1<<26;
		slope = P_AimLineAttack( source, an, AimDistance );

		if (!linetarget)
		{
			an -= 2<<26;
			slope = P_AimLineAttack( source, an, AimDistance );
		}

		if (!linetarget)
		{
			an = source->angle;
			slope = pitch ? finetangent[ FINEANGLE( pitch + ANG90 ) ] : 0;
		}
	}

	fixed_t x = xoffset + source->x;
	fixed_t y = yoffset + source->y;
	fixed_t z = zoffset + source->z + IntToFixed( 32 );
	
	mobj_t* th = P_SpawnMobj( x,y,z, type );

	if( th->info->seesound )
	{
		S_StartSound( th, th->info->seesound );
	}

	th->target = source;
	if( settracer )
	{
		th->tracer = linetarget;
	}

	th->angle = an;
	th->momx = FixedMul( th->Speed(), finecosine[ FINEANGLE( an ) ] );
	th->momy = FixedMul( th->Speed(), finesine[ FINEANGLE( an ) ] );
	th->momz = FixedMul( th->Speed(), slope );

	P_CheckMissileSpawn( th );
	return th;
}

DOOM_C_API mobj_t* P_SpawnPlayerMissile( mobj_t* source, int32_t type )
{
	return P_SpawnPlayerMissileEx( source, type, source->angle, 0, 0, 0, 0, false );
}
