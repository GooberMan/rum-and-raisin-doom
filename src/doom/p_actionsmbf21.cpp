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

static INLINE void BulletAttack( mobj_t* source, fixed_t hspread, fixed_t vspread, uint32_t numbullets
								, uint32_t damagebase, uint32_t damagedice, damage_t damageflags )
{
	fixed_t slope = P_AimLineAttack( source, source->angle, MISSILERANGE );
	fixed_t slopespread = finetangent[ FINEANGLE( DegreesToAngle( vspread ) ) ];

	for( [[maybe_unused]] int32_t bulletnum : iota( 0, numbullets ) )
	{
		uint32_t damage = damagebase + (P_Random() % damagedice) + 1;
		angle_t hangle = source->angle + DegreesToAngle( FixedMul( P_Random() << 8, hspread ) - ( hspread >> 1 ) );
		fixed_t slopeadd = FixedMul( P_Random() << 8, slopespread ) - ( slopespread >> 1 );
		P_LineAttack( source, hangle, MISSILERANGE, slope + slopeadd, damage, damageflags );
	}
}

// External definitions
DOOM_C_API void A_FaceTarget( mobj_t* mobj );
DOOM_C_API void A_VileChaseParams( mobj_t* actor, statenum_t healstate, sfxenum_t healsound );
DOOM_C_API void P_SetPsprite( player_t* player, int position, int32_t stnum ) ;
DOOM_C_API doombool P_CheckAmmo( player_t* player );

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

	mobj_t* spawned = P_SpawnMobjEx( &mobjinfo[ type - 1 ], DegreesToAngle( angle ),
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

	mobj_t* spawned = P_SpawnMobjEx( &mobjinfo[ type - 1 ], DegreesToAngle( angle ),
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
	ARG_FIXED( mobj, hspread, 1 );
	ARG_FIXED( mobj, vspread, 2 );
	ARG_UINT( mobj, numbullets, 3 );
	ARG_UINT( mobj, damagebase, 4 );
	ARG_UINT( mobj, damagedice, 5 );

	BulletAttack( mobj, hspread, vspread, numbullets, damagebase, damagedice, damage_none );
}

DOOM_C_API void A_MonsterMeleeAttack( mobj_t* mobj )
{
	ARG_UINT( mobj, damagebase, 1 );
	ARG_UINT( mobj, damagedice, 2 );
	ARG_SOUND( mobj, sound, 3 );
	ARG_FIXED( mobj, range, 4 );

	if( !mobj->target )
	{
		return;
	}

	A_FaceTarget( mobj );

	if( P_CheckMeleeRangeEx( mobj, range ? range : mobj->MeleeRange() ) )
	{
		S_StartSound( mobj, sound );
		int32_t damage = damagebase * ( ( P_Random() % damagedice ) + 1 );
		P_DamageMobjEx( mobj->target, mobj, mobj, damage, damage_melee );
	}
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
	ARG_UINT( mobj, flags2, 3 );

	if( ( mobj->flags & flags ) == flags
		&& ( mobj->flags2 & flags2 ) == flags2 )
	{
		P_SetMobjState( mobj, state );
	}
}

DOOM_C_API void A_AddFlags( mobj_t* mobj )
{
	ARG_UINT( mobj, flags, 1 );
	ARG_UINT( mobj, flags2, 2 );

	mobj->flags |= flags;
	mobj->flags2 |= flags2;
}

DOOM_C_API void A_RemoveFlags( mobj_t* mobj )
{
	ARG_UINT( mobj, flags, 1 );
	ARG_UINT( mobj, flags2, 2 );

	mobj->flags &= ~flags;
	mobj->flags2 &= ~flags2;
}

DOOM_C_API void A_WeaponProjectile( player_t* player, pspdef_t* psp )
{
	PSPARG_MOBJTYPE( psp, type, 1 );
	PSPARG_FIXED( psp, angle, 2 );
	PSPARG_FIXED( psp, pitch, 3 );
	PSPARG_FIXED( psp, hoffset, 4 );
	PSPARG_FIXED( psp, voffset, 5 );

	angle_t hangle = player->mo->angle + DegreesToAngle( angle );
	angle_t vangle = DegreesToAngle( pitch );

	int32_t forwardlookup = FINEANGLE( hangle );

	fixed_t xoffset = FixedMul( hoffset, finesine[ forwardlookup ] );
	fixed_t yoffset = FixedMul( hoffset, finecosine[ forwardlookup ] );

	mobj_t* projectile = P_SpawnPlayerMissileEx( player->mo, type, hangle, vangle, xoffset, yoffset, voffset, true );
}

DOOM_C_API void A_WeaponBulletAttack( player_t* player, pspdef_t* psp )
{
	PSPARG_FIXED( psp, hspread, 1 );
	PSPARG_FIXED( psp, vspread, 2 );
	PSPARG_UINT( psp, numbullets, 3 );
	PSPARG_UINT( psp, damagebase, 4 );
	PSPARG_UINT( psp, damagedice, 5 );

	const weaponinfo_t& weapon = weaponinfo[ player->readyweapon ];
	damage_t damageflags = weapon.NoThrusting() ? damage_nothrust : damage_none;

	BulletAttack( player->mo, hspread, vspread, numbullets, damagebase, damagedice, damageflags );
}

DOOM_C_API void A_WeaponMeleeAttack( player_t* player, pspdef_t* psp )
{
	PSPARG_UINT( psp, damagebase, 1 );
	PSPARG_UINT( psp, damagedice, 2 );
	PSPARG_FIXED( psp, zerkfactor, 3 );
	PSPARG_SOUND( psp, sound, 4 );
	PSPARG_FIXED( psp, range, 5 );

	const weaponinfo_t& weapon = weaponinfo[ player->readyweapon ];
	damage_t damageflags = (damage_t)( ( weapon.NoThrusting() ? damage_nothrust : damage_none ) | damage_melee );

	int32_t damage = damagebase * ( ( P_Random () % damagedice ) + 1 );
	if( player->powers[ pw_strength ] )
	{
		damage *= ( zerkfactor ? zerkfactor : IntToFixed( 1 ) );
	}

	angle_t angle = player->mo->angle + ( P_SubRandom() << 18 );
	fixed_t attackrange = range ? range : player->mo->MeleeRange();
	fixed_t slope = P_AimLineAttack( player->mo, angle, attackrange );
	mobj_t* target = P_LineAttack( player->mo, angle, attackrange, slope, damage, damageflags );

	if( target )
	{
		S_StartSound( player->mo, sound );
		P_NoiseAlert( player->mo, player->mo );

		player->mo->angle = BSP_PointToAngle( player->mo->x,
												player->mo->y,
												target->x,
												target->y );
	}
}

DOOM_C_API void A_WeaponSound( player_t* player, pspdef_t* psp )
{
	PSPARG_SOUND( psp, sound, 1 );
	PSPARG_INT( psp, fullvol, 2 );

	S_StartSound( fullvol ? nullptr : player->mo, sound );
	P_NoiseAlert( player->mo, player->mo );
}

DOOM_C_API void A_WeaponJump( player_t* player, pspdef_t* psp )
{
	PSPARG_INT( psp, state, 1 );
	PSPARG_INT( psp, chance, 2 );

	if( P_Random() < chance )
	{
		P_SetPsprite( player, ps_weapon, state );
	}
}

DOOM_C_API void A_ConsumeAmmo( player_t* player, pspdef_t* psp )
{
	PSPARG_INT( psp, amount, 1 );

	const weaponinfo_t& weapon = weaponinfo[ player->readyweapon ];

	if( weapon.ammo != am_noammo )
	{
		player->ammo[ weapon.ammo ] = M_CLAMP( player->ammo[ weapon.ammo ] - amount, 0, player->maxammo[ weapon.ammo ] );
	}
}

DOOM_C_API void A_CheckAmmo( player_t* player, pspdef_t* psp )
{
	PSPARG_INT( psp, state, 1 );
	PSPARG_UINT( psp, amount, 2 );

	const weaponinfo_t& weapon = weaponinfo[ player->readyweapon ];
	int32_t checkamount = amount ? amount : weapon.ammopershot;
	if( weapon.ammo != am_noammo && player->ammo[ weapon.ammo ] < checkamount )
	{
		P_SetPsprite( player, ps_weapon, state );
	}
}

DOOM_C_API void A_RefireTo( player_t* player, pspdef_t* psp )
{
	PSPARG_INT( psp, state, 1 );
	PSPARG_INT( psp, noammocheck, 2 );

	if ( ( player->cmd.buttons & BT_ATTACK )
		&& player->pendingweapon == wp_nochange
		&& player->health )
	{
		++player->refire;
		P_SetPsprite( player, ps_weapon, state );
	}
	else if( !noammocheck )
	{
		I_LogAddEntry( Log_Error, "A_RefireTo noammocheck unimplemented" );
	}
}

DOOM_C_API void A_GunFlashTo( player_t* player, pspdef_t* psp )
{
	PSPARG_INT( psp, state, 1 );
	PSPARG_INT( psp, nothirdperson, 2 );

	P_SetPsprite( player, ps_flash, state );
	I_LogAddEntry( Log_Error, "A_GunFlashTo nothirdperson unimplemented" );
}

DOOM_C_API void A_WeaponAlert( player_t* player, pspdef_t* psp )
{
	P_NoiseAlert( player->mo, player->mo );
}

