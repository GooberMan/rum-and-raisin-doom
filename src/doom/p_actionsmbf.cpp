//
// Copyright(C) 2024 Ethan Watson
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
//	New actor actions for the MBF standard
//

#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"
#include "p_lineaction.h"

#include "m_random.h"

#include "i_system.h"
#include "i_log.h"

#include "r_local.h"

#include "s_sound.h"

// External definitions
DOOM_C_API void A_FaceTarget( mobj_t* mobj );

// MBF actions
DOOM_C_API void A_Detonate( mobj_t* mobj )
{
	P_RadiusAttack( mobj, mobj->target, mobj->info->damage );
}

DOOM_C_API void A_Mushroom( mobj_t* mobj )
{
	I_LogAddEntry( Log_Error, "A_Mushroom unimplemented" );
}

DOOM_C_API void A_Spawn( mobj_t* mobj )
{
	// [MBF] Unclear from spec what misc2 does exactly???
	P_SpawnMobj( mobj->x, mobj->y, mobj->z + mobj->state->misc2._fixed, mobj->state->misc1._mobjtype );
}

DOOM_C_API void A_Turn( mobj_t* mobj )
{
	mobj->angle += mobj->state->misc1._angle;
}

DOOM_C_API void A_Face( mobj_t* mobj )
{
	mobj->angle = mobj->state->misc1._angle;
}

DOOM_C_API void A_Scratch( mobj_t* mobj )
{
	if( !mobj->target )
	{
		return;
	}

	A_FaceTarget( mobj );
	if( P_CheckMeleeRange( mobj ) )
	{
		S_StartSound( mobj, mobj->state->misc2._int );
		// [MBF] Is damage randomised???
		P_DamageMobj( mobj->target, mobj, mobj, mobj->state->misc1._int );
		return;
	}
}

DOOM_C_API void A_RandomJump( mobj_t* mobj )
{
	if( P_Random() < mobj->state->misc2._int )
	{
		P_SetMobjState( mobj, mobj->state->misc1._statenum );
	}
}

DOOM_C_API void A_LineEffect( mobj_t* mobj )
{
	if( !mobj->successfullineeffect )
	{
		line_t dummyline = {};
		dummyline.special = mobj->state->misc1._int;
		dummyline.tag = mobj->state->misc2._int;
		lineaction_t* action = dummyline.action = P_GetLineActionFor( &dummyline );

		player_t dummyplayer = {};
		player_t* oldplayer = mobj->player;
		mobj->player = oldplayer ? oldplayer : &dummyplayer;

		if( dummyline.action->Handle( &dummyline, mobj, action->trigger, 0 ) )
		{
			// Action gets cleared out if it is a once-only line
			mobj->successfullineeffect = dummyline.action == nullptr;
		}

		mobj->player = oldplayer;
	}
}

DOOM_C_API void A_Die( mobj_t* mobj )
{
	if( mobj->health > 0 )
	{
		mobj->health = 0;
		P_KillMobj( mobj, mobj );
	}
}
