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
//	New actor actions for the MBF21 standard
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

// THIS IS SO INACCURATE HOW DO OTHER PORTS DO IT
constexpr angle_t DegreesToAngle( fixed_t deg )
{
	constexpr angle_t ang1 = ANG1;
	angle_t angle = FixedToInt( deg ) * ang1 + FixedMul( deg & FRACMASK, ANG1 );
	return angle;
}

static INLINE bool ObjectInSight( mobj_t* source, mobj_t* target, angle_t fov )
{
	angle_t angle = BSP_PointToAngle( source->x, source->y, target->x, target->y ) - source->angle;
	bool withinfov = angle == 0
					|| ( angle > fov && angle < M_NEGATE( fov ) );
	return withinfov && P_CheckSight( source, target );
}

static INLINE bool ObjectInRange( mobj_t* source, mobj_t* target, fixed_t range )
{
	fixed_t dx = FixedAbs( source->x - target->x );
	fixed_t dy = FixedAbs( source->y - target->y );
	fixed_t dist = P_AproxDistance( dx, dy );
	return dist < range;
}

// External definitions
DOOM_C_API void A_VileChaseParams( mobj_t* actor, statenum_t healstate, sfxenum_t healsound );

// MBF21 actions
DOOM_C_API void A_SpawnObject( mobj_t* mobj )
{
	ARG_MOBJTYPE( mobj, type, 1 );
	ARG_FIXED( mobj, angle, 2 );
	ARG_FIXED( mobj, x_ofs, 3 );
	ARG_FIXED( mobj, y_ofs, 4 );
	ARG_FIXED( mobj, z_ofs, 5 );
	ARG_FIXED( mobj, x_vel, 6 );
	ARG_FIXED( mobj, y_vel, 7 );
	ARG_FIXED( mobj, z_vel, 8 );

	mobj_t* spawned = P_SpawnMobjEx( type, DegreesToAngle( angle ),
									mobj->x + x_ofs, mobj->y + y_ofs, mobj->z + z_ofs,
									x_vel, y_vel, z_vel );
	if( mobj->flags & MF_MISSILE )
	{
		if( spawned->flags & MF_MISSILE )
		{
			spawned->tracer = mobj->tracer;
			spawned->target = mobj->target;
		}
		else
		{
			spawned->tracer = mobj->target;
			spawned->target = mobj;
		}
	}
}

DOOM_C_API void A_MonsterProjectile( mobj_t* mobj )
{
	ARG_MOBJTYPE( mobj, type, 1 );
	ARG_FIXED( mobj, angle, 2 );
	ARG_FIXED( mobj, pitch, 3 );
	ARG_FIXED( mobj, hoffset, 4 );
	ARG_FIXED( mobj, voffset, 5 );

	angle_t asangle = DegreesToAngle( angle );
	angle_t pitchangle = DegreesToAngle( pitch );

	int32_t forwardlookup = FINEANGLE( asangle );
	int32_t vertlookup = FINEANGLE( pitchangle );

	fixed_t xoffset = FixedMul( hoffset, finesine[ forwardlookup ] );
	fixed_t yoffset = FixedMul( hoffset, finecosine[ forwardlookup ] );

	mobj_t* spawned = P_SpawnMobjEx( type, DegreesToAngle( angle ),
									mobj->x + xoffset, mobj->y + yoffset, mobj->z + voffset,
									mobjinfo[ type ].speed, 0, finetangent[ vertlookup ] );

	if( spawned->info->seesound )
	{
		S_StartSound( spawned, spawned->info->seesound );
	}

	spawned->target = mobj;
	spawned->tracer = mobj->target;

	P_CheckMissileSpawn( spawned );
}

DOOM_C_API void A_MonsterBulletAttack( mobj_t* mobj )
{
	I_LogAddEntry( Log_Error, "A_MonsterBulletAttack unimplemented" );
}

DOOM_C_API void A_MonsterMeleeAttack( mobj_t* mobj )
{
	I_LogAddEntry( Log_Error, "A_MonsterMeleeAttack unimplemented" );
}

DOOM_C_API void A_RadiusDamage( mobj_t* mobj )
{
	ARG_UINT( mobj, damage, 1 );
	ARG_UINT( mobj, radius, 2 );

	P_RadiusAttackDistance( mobj, mobj->target, damage, IntToFixed( radius ) );
}

DOOM_C_API void A_NoiseAlert( mobj_t* mobj )
{
	P_NoiseAlert( mobj->target, mobj );
}

DOOM_C_API void A_HealChase( mobj_t* mobj )
{
	ARG_STATENUM( mobj, state, 1 );
	ARG_SOUND( mobj, sound, 2 );

	A_VileChaseParams( mobj, state, sound );
}

DOOM_C_API void A_SeekTracer( mobj_t* mobj )
{
	I_LogAddEntry( Log_Error, "A_SeekTracer unimplemented" );
}

DOOM_C_API void A_FindTracer( mobj_t* mobj )
{
	ARG_FIXED( mobj, fov, 1 );
	ARG_UINT( mobj, rangeblocks, 2 );

	if( mobj->tracer == nullptr )
	{
		angle_t fovangle = DegreesToAngle( fov );
		fixed_t range = FixedMul( rangeblocks ? rangeblocks : 10, MAPBLOCKSIZE >> 1 );
		mobj->tracer = P_FindTracerTarget( mobj, range, fovangle );
	}
}

DOOM_C_API void A_ClearTracer( mobj_t* mobj )
{
	mobj->tracer = nullptr;
}

DOOM_C_API void A_JumpIfHealthBelow( mobj_t* mobj )
{
	ARG_STATENUM( mobj, state, 1 );
	ARG_INT( mobj, health, 2 );

	if( mobj->health < health )
	{
		P_SetMobjState( mobj, state );
	}
}

DOOM_C_API void A_JumpIfTargetInSight( mobj_t* mobj )
{
	ARG_STATENUM( mobj, state, 1 );
	ARG_FIXED( mobj, fov, 2 );

	if( mobj->target && ObjectInSight( mobj, mobj->target, DegreesToAngle( fov ) ) )
	{
		P_SetMobjState( mobj, state );
	}
}

DOOM_C_API void A_JumpIfTargetCloser( mobj_t* mobj )
{
	ARG_STATENUM( mobj, state, 1 );
	ARG_FIXED( mobj, distance, 2 );

	if( mobj->target && ObjectInRange( mobj, mobj->target, distance ) )
	{
		P_SetMobjState( mobj, state );
	}
}

DOOM_C_API void A_JumpIfTracerInSight( mobj_t* mobj )
{
	ARG_STATENUM( mobj, state, 1 );
	ARG_FIXED( mobj, fov, 2 );

	if( mobj->tracer && ObjectInSight( mobj, mobj->tracer, DegreesToAngle( fov ) ) )
	{
		P_SetMobjState( mobj, state );
	}
}

DOOM_C_API void A_JumpIfTracerCloser( mobj_t* mobj )
{
	ARG_STATENUM( mobj, state, 1 );
	ARG_FIXED( mobj, distance, 2 );

	if( mobj->tracer && ObjectInRange( mobj, mobj->tracer, distance ) )
	{
		P_SetMobjState( mobj, state );
	}
}

DOOM_C_API void A_JumpIfFlagsSet( mobj_t* mobj )
{
	ARG_STATENUM( mobj, state, 1 );
	ARG_UINT( mobj, flags, 2 );
	ARG_UINT( mobj, flags2, 2 );

	if( ( mobj->flags & flags ) == flags
		&& ( mobj->flags2 & flags2 ) == flags2 )
	{
		P_SetMobjState( mobj, state );
	}
}

DOOM_C_API void A_AddFlags( mobj_t* mobj )
{
	ARG_STATENUM( mobj, state, 1 );
	ARG_UINT( mobj, flags, 2 );
	ARG_UINT( mobj, flags2, 2 );

	mobj->flags |= flags;
	mobj->flags2 |= flags2;
}

DOOM_C_API void A_RemoveFlags( mobj_t* mobj )
{
	ARG_STATENUM( mobj, state, 1 );
	ARG_UINT( mobj, flags, 2 );
	ARG_UINT( mobj, flags2, 2 );

	mobj->flags &= ~flags;
	mobj->flags2 &= ~flags2;
}

