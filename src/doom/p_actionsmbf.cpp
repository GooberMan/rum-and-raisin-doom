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
	MISC_FIXED( mobj, definedangle, 1 );
	MISC_FIXED( mobj, definedspeed, 2 );

	fixed_t angle = definedangle ? definedangle : IntToFixed( 16 );
	fixed_t speed = definedspeed ? definedspeed : DoubleToFixed( 0.5 );

	mobj_t dummymobj = *mobj;

	for( int32_t ballnum : iota( 0, 8 ) )
	{
		angle_t spawnangle = ballnum * ANG45;
		int32_t lookup = FINEANGLE( spawnangle );
		const fixed_t& sinval = finesine[ lookup ];
		const fixed_t& cosval = finecosine[ lookup ];

		dummymobj.x = mobj->x + FixedMul( speed, cosval );
		dummymobj.y = mobj->y + FixedMul( speed, sinval );
		dummymobj.z = mobj->z + angle;
		dummymobj.angle = spawnangle;

		mobj_t* spawned = P_SpawnMissile( mobj, &dummymobj, MT_FATSHOT );
		spawned->flags &= ~MF_NOGRAVITY;
	}
}

DOOM_C_API void A_Spawn( mobj_t* mobj )
{
	MISC_MOBJTYPE( mobj, type, 1 );
	MISC_FIXED( mobj, zoffset, 2 );
	
	P_SpawnMobj( mobj->x, mobj->y, mobj->z + zoffset, type );
}

DOOM_C_API void A_Turn( mobj_t* mobj )
{
	MISC_ANGLE( mobj, angle, 1 );

	mobj->angle += angle;
}

DOOM_C_API void A_Face( mobj_t* mobj )
{
	MISC_ANGLE( mobj, angle, 1 );

	mobj->angle = angle;
}

DOOM_C_API void A_Scratch( mobj_t* mobj )
{
	MISC_INT( mobj, damage, 1 );
	MISC_SOUND( mobj, sound, 2 );

	if( !mobj->target )
	{
		return;
	}

	A_FaceTarget( mobj );
	if( P_CheckMeleeRange( mobj ) )
	{
		S_StartSound( mobj, sound );
		// [MBF] Is damage randomised???
		P_DamageMobj( mobj->target, mobj, mobj, damage );
		return;
	}
}

DOOM_C_API void A_PlaySound( mobj_t* mobj )
{
	MISC_SOUND( mobj, sound, 1 );
	MISC_INT( mobj, fullvolume, 2 );

	S_StartSound( fullvolume ? nullptr : mobj, sound );
}

DOOM_C_API void A_RandomJump( mobj_t* mobj )
{
	MISC_STATENUM( mobj, state, 1 );
	MISC_INT( mobj, probability, 2 );

	if( P_Random() < probability )
	{
		P_SetMobjState( mobj, state );
	}
}

DOOM_C_API void A_LineEffect( mobj_t* mobj )
{
	MISC_INT( mobj, special, 1 );
	MISC_INT( mobj, tag, 2 );

	if( !mobj->successfullineeffect )
	{
		line_t dummyline = {};
		dummyline.special = special;
		dummyline.tag = tag;
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
	P_DamageMobjEx( mobj, mobj, mobj, 10000, damage_theworks );
}
