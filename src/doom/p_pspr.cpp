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
//	Weapon sprite animation, weapon objects.
//	Action functions for weapons.
//


#include "doomdef.h"
#include "d_event.h"

#include "deh_misc.h"

#include "m_misc.h"
#include "m_random.h"

#include "p_local.h"

#include "s_sound.h"

// State.
#include "doomstat.h"

// Data.
#include "sounds.h"

#include "p_pspr.h"

//
// P_SetPsprite
//
DOOM_C_API void P_SetPsprite( player_t* player, int position, int32_t stnum ) 
{
    pspdef_t*	psp;
    const state_t* state;
	
    psp = &player->psprites[position];

	psp->lastx = psp->sx;
	psp->lasty = psp->sy;
	psp->duration = -1;
	psp->leveltime = leveltime;
	psp->viewbob = false;

    do
    {
		if (!stnum)
		{
			// object removed itself
			psp->state = nullptr;
			break;	
		}
	
		state = &states[stnum];
		psp->state = state;
		psp->tics = state->Tics();	// could be 0
		psp->duration = M_MAX( psp->tics, psp->duration );

		if (state->misc1._int)
		{
			// coordinate set
			psp->sx = state->misc1._int << FRACBITS;
			psp->sy = state->misc2._int << FRACBITS;
		}
	
		// Call action routine.
		// Modified handling.
		if (state->action.Valid())
		{
			state->action(player, psp);
			if (!psp->state)
			{
				break;
			}
		}
	
		stnum = psp->state->nextstate;
	
    } while (!psp->tics);
    // an initial state of 0 could cycle through
}



//
// P_CalcSwing
//	
fixed_t		swingx;
fixed_t		swingy;

void P_CalcSwing (player_t*	player)
{
    fixed_t	swing;
    int		angle;
	
    // OPTIMIZE: tablify this.
    // A LUT would allow for different modes,
    //  and add flexibility.

    swing = player->bob;

    angle = (FINEANGLES/70*leveltime)&FINEMASK;
    swingx = FixedMul ( swing, finesine[angle]);

    angle = (FINEANGLES/70*leveltime+FINEANGLES/2)&FINEMASK;
    swingy = -FixedMul ( swingx, finesine[angle]);
}



//
// P_BringUpWeapon
// Starts bringing the pending weapon up
// from the bottom of the screen.
// Uses player
//
void P_BringUpWeapon (player_t* player)
{
    statenum_t	newstate;
	
    if (player->pendingweapon == wp_nochange)
	player->pendingweapon = player->readyweapon;
		
    if (player->pendingweapon == wp_chainsaw)
	S_StartSound (player->mo, sfx_sawup);
		
    newstate = (statenum_t)weaponinfo[player->pendingweapon].upstate;

    player->pendingweapon = wp_nochange;
    player->psprites[ps_weapon].sy = WEAPONBOTTOM;

    P_SetPsprite (player, ps_weapon, newstate);
}

//
// P_CheckAmmo
// Returns true if there is enough ammo to shoot.
// If not, selects the next weapon to use.
//
DOOM_C_API doombool P_CheckAmmo (player_t* player)
{
	int32_t ammo	= weaponinfo[player->readyweapon].ammo;
	int32_t count	= weaponinfo[player->readyweapon].ammopershot;

    // Some do not need ammunition anyway.
    // Return if current ammunition sufficient.
    if (ammo == am_noammo || player->ammo[ammo] >= count)
	{
		return true;
	}

	do
	{
		for( weaponinfo_t* weapon : weaponinfo.BySwitchPriority() )
		{
			if( player->weaponowned[ weapon->index ]
				&& (weapon->ammo == am_noammo || player->ammo[ weapon->ammo ] > weapon->ammopershot )
				&& weapon->mingamemode <= gamemode )
			{
				player->pendingweapon = weapon->index;
				break;
			}
		}

		if( player->pendingweapon == wp_nochange )
		{
			I_Error( "P_CheckAmmo: Weapon select will infinite loop here in vanilla Doom" );
		}
	} while( player->pendingweapon == wp_nochange );

    // Now set appropriate weapon overlay.
    P_SetPsprite (player,
		  ps_weapon,
		  (statenum_t)weaponinfo[player->readyweapon].downstate);

    return false;	
}


//
// P_FireWeapon.
//
DOOM_C_API void P_FireWeapon (player_t* player)
{
    statenum_t	newstate;
	
    if (!P_CheckAmmo (player))
	return;
	
    P_SetMobjState (player->mo, S_PLAY_ATK1);
    newstate = (statenum_t)weaponinfo[player->readyweapon].atkstate;
    P_SetPsprite (player, ps_weapon, newstate);

	if( !player->Weapon().Silent() )
	{
		P_NoiseAlert (player->mo, player->mo);
	}
}



//
// P_DropWeapon
// Player died, so put the weapon away.
//
DOOM_C_API void P_DropWeapon (player_t* player)
{
    P_SetPsprite (player,
		  ps_weapon,
		  (statenum_t)weaponinfo[player->readyweapon].downstate);
}



//
// A_WeaponReady
// The player can fire the weapon
// or change to another weapon at this time.
// Follows after getting weapon up,
// or after previous attack/fire sequence.
//
DOOM_C_API void
A_WeaponReady
( player_t*	player,
  pspdef_t*	psp )
{	
    statenum_t	newstate;
    int		angle;
    
    // get out of attack state
    if (player->mo->state == &states[S_PLAY_ATK1]
	|| player->mo->state == &states[S_PLAY_ATK2] )
    {
		P_SetMobjState (player->mo, S_PLAY);
    }
    
    if (player->readyweapon == wp_chainsaw
		&& psp->state == &states[S_SAW])
    {
		S_StartSound (player->mo, sfx_sawidl);
    }
    
    // check for change
    //  if player is dead, put the weapon away
    if (player->pendingweapon != wp_nochange || !player->health)
    {
		// change weapon
		//  (pending weapon should allready be validated)
		newstate = (statenum_t)weaponinfo[player->readyweapon].downstate;
		P_SetPsprite (player, ps_weapon, newstate);
		return;
    }
    
    // check for fire
    //  the missile launcher and bfg do not auto fire
    if (player->cmd.buttons & BT_ATTACK)
    {
		if ( !player->attackdown
			|| !weaponinfo[ player->readyweapon ].NoAutofire() )
		{
			player->attackdown = true;
			P_FireWeapon (player);		
			return;
		}
    }
    else
	{
		player->attackdown = false;
	}
    
    // bob the weapon based on movement speed
    angle = (128*leveltime)&FINEMASK;
    psp->sx = FRACUNIT + FixedMul (player->bob, finecosine[angle]);
    angle &= FINEANGLES/2-1;
    psp->sy = WEAPONTOP + FixedMul (player->bob, finesine[angle]);

	psp->viewbob = true;
}



//
// A_ReFire
// The player can re-fire the weapon
// without lowering it entirely.
//
DOOM_C_API void A_ReFire
( player_t*	player,
  pspdef_t*	psp )
{
    
    // check for fire
    //  (if a weaponchange is pending, let it go through instead)
    if ( (player->cmd.buttons & BT_ATTACK) 
	 && player->pendingweapon == wp_nochange
	 && player->health)
    {
	player->refire++;
	P_FireWeapon (player);
    }
    else
    {
	player->refire = 0;
	P_CheckAmmo (player);
    }
}


DOOM_C_API void
A_CheckReload
( player_t*	player,
  pspdef_t*	psp )
{
    P_CheckAmmo (player);
#if 0
    if (player->ammo[am_shell]<2)
	P_SetPsprite (player, ps_weapon, S_DSNR1);
#endif
}



//
// A_Lower
// Lowers current weapon,
//  and changes weapon at bottom.
//
DOOM_C_API void
A_Lower
( player_t*	player,
  pspdef_t*	psp )
{	
    psp->sy += LOWERSPEED;

    // Is already down.
    if (psp->sy < WEAPONBOTTOM )
	return;

    // Player is dead.
    if (player->playerstate == PST_DEAD)
    {
	psp->sy = WEAPONBOTTOM;

	// don't bring weapon back up
	return;		
    }
    
    // The old weapon has been lowered off the screen,
    // so change the weapon and start raising it
    if (!player->health)
    {
	// Player is dead, so keep the weapon off screen.
	P_SetPsprite (player,  ps_weapon, S_NULL);
	return;	
    }
	
    player->readyweapon = player->pendingweapon; 

    P_BringUpWeapon (player);
}


//
// A_Raise
//
DOOM_C_API void
A_Raise
( player_t*	player,
  pspdef_t*	psp )
{
    statenum_t	newstate;
	
    psp->sy -= RAISESPEED;

    if (psp->sy > WEAPONTOP )
	return;
    
    psp->sy = WEAPONTOP;
    
    // The weapon has been raised all the way,
    //  so change to the ready state.
    newstate = (statenum_t)weaponinfo[player->readyweapon].readystate;

    P_SetPsprite (player, ps_weapon, newstate);
}



//
// A_GunFlash
//
DOOM_C_API void
A_GunFlash
( player_t*	player,
  pspdef_t*	psp ) 
{
    P_SetMobjState (player->mo, S_PLAY_ATK2);
    P_SetPsprite (player,ps_flash,(statenum_t)weaponinfo[player->readyweapon].flashstate);
}



//
// WEAPON ATTACKS
//


//
// A_Punch
//
DOOM_C_API void
A_Punch
( player_t*	player,
  pspdef_t*	psp ) 
{
	const weaponinfo_t& weapon = weaponinfo[player->readyweapon];
	damage_t damageflags = (damage_t)( ( weapon.NoThrusting() ? damage_nothrust : damage_none ) | damage_melee );

    angle_t	angle;
    int		damage;
    int		slope;
	
    damage = (P_Random ()%10+1)<<1;

    if (player->powers[pw_strength])	
	damage *= 10;

    angle = player->mo->angle;
    angle += P_SubRandom() << 18;
    slope = P_AimLineAttack (player->mo, angle, player->mo->MeleeRange());
    P_LineAttack (player->mo, angle, player->mo->MeleeRange(), slope, damage, damageflags);

    // turn to face target
    if (linetarget)
    {
		S_StartSound (player->mo, sfx_punch);
		player->mo->angle = BSP_PointToAngle (player->mo->x,
							 player->mo->y,
							 linetarget->x,
							 linetarget->y);
    }
}


//
// A_Saw
//
DOOM_C_API void
A_Saw
( player_t*	player,
  pspdef_t*	psp ) 
{
	const weaponinfo_t& weapon = weaponinfo[player->readyweapon];
	damage_t damageflags = (damage_t)( ( weapon.NoThrusting() ? damage_nothrust : damage_none ) | damage_melee );

    angle_t	angle;
    int		damage;
    int		slope;

    damage = 2*(P_Random ()%10+1);
    angle = player->mo->angle;
    angle += P_SubRandom() << 18;
    
    // use meleerange + 1 se the puff doesn't skip the flash
    slope = P_AimLineAttack (player->mo, angle, player->mo->MeleeRange() + 1 );
    P_LineAttack (player->mo, angle, player->mo->MeleeRange() + 1, slope, damage, damageflags);

    if (!linetarget)
    {
		S_StartSound (player->mo, sfx_sawful);
		return;
    }
    S_StartSound (player->mo, sfx_sawhit);
	
    // turn to face target
    angle = BSP_PointToAngle (player->mo->x, player->mo->y,
			     linetarget->x, linetarget->y);
    if (angle - player->mo->angle > ANG180)
    {
	if ((signed int) (angle - player->mo->angle) < -ANG90/20)
	    player->mo->angle = angle + ANG90/21;
	else
	    player->mo->angle -= ANG90/20;
    }
    else
    {
	if (angle - player->mo->angle > ANG90/20)
	    player->mo->angle = angle - ANG90/21;
	else
	    player->mo->angle += ANG90/20;
    }
    player->mo->flags |= MF_JUSTATTACKED;
}

// Doom does not check the bounds of the ammo array.  As a result,
// it is possible to use an ammo type > 4 that overflows into the
// maxammo array and affects that instead.  Through dehacked, for
// example, it is possible to make a weapon that decreases the max
// number of ammo for another weapon.  Emulate this.

static void DecreaseAmmo(player_t *player, int ammonum, int amount)
{
    if (ammonum < NUMAMMO)
    {
        player->ammo[ammonum] -= amount;
    }
    else
    {
        player->maxammo[ammonum - NUMAMMO] -= amount;
    }
}


//
// A_FireMissile
//
DOOM_C_API void
A_FireMissile
( player_t*	player,
  pspdef_t*	psp ) 
{
    DecreaseAmmo(player, weaponinfo[player->readyweapon].ammo, weaponinfo[player->readyweapon].ammopershot);
    P_SpawnPlayerMissile (player->mo, MT_ROCKET);
}


//
// A_FireBFG
//
DOOM_C_API void
A_FireBFG
( player_t*	player,
  pspdef_t*	psp ) 
{
    DecreaseAmmo(player, weaponinfo[player->readyweapon].ammo, 
                 weaponinfo[player->readyweapon].ammopershot);
    P_SpawnPlayerMissile (player->mo, MT_BFG);
}



//
// A_FirePlasma
//
DOOM_C_API void
A_FirePlasma
( player_t*	player,
  pspdef_t*	psp ) 
{
    DecreaseAmmo(player, weaponinfo[player->readyweapon].ammo, weaponinfo[player->readyweapon].ammopershot);

    P_SetPsprite (player,
		  ps_flash,
		  (statenum_t)(weaponinfo[player->readyweapon].flashstate+(P_Random ()&1)) );

    P_SpawnPlayerMissile (player->mo, MT_PLASMA);
}



//
// P_BulletSlope
// Sets a slope so a near miss is at aproximately
// the height of the intended target
//
extern "C"
{
	fixed_t		bulletslope;
}


void P_BulletSlope (mobj_t*	mo)
{
    angle_t	an;
    
    // see which target is to be aimed at
    an = mo->angle;
    bulletslope = P_AimLineAttack (mo, an, 16*64*FRACUNIT);

    if (!linetarget)
    {
	an += 1<<26;
	bulletslope = P_AimLineAttack (mo, an, 16*64*FRACUNIT);
	if (!linetarget)
	{
	    an -= 2<<26;
	    bulletslope = P_AimLineAttack (mo, an, 16*64*FRACUNIT);
	}
    }
}


//
// P_GunShot
//
void P_GunShot( mobj_t* mo, doombool accurate, damage_t damageflags )
{
    angle_t	angle;
    int		damage;
	
    damage = 5*(P_Random ()%3+1);
    angle = mo->angle;

    if (!accurate)
	{
		angle += P_SubRandom() << 18;
	}

    P_LineAttack(mo, angle, MISSILERANGE, bulletslope, damage, damageflags);
}


//
// A_FirePistol
//
DOOM_C_API void
A_FirePistol
( player_t*	player,
  pspdef_t*	psp ) 
{
	const weaponinfo_t& weapon = weaponinfo[player->readyweapon];
    S_StartSound (player->mo, sfx_pistol);

    P_SetMobjState (player->mo, S_PLAY_ATK2);
    DecreaseAmmo(player, weapon.ammo, weapon.ammopershot);

    P_SetPsprite (player,
		  ps_flash,
		  (statenum_t)weapon.flashstate);

    P_BulletSlope (player->mo);
    P_GunShot(player->mo, !player->refire, weapon.NoThrusting() ? damage_nothrust : damage_none );
}


//
// A_FireShotgun
//
DOOM_C_API void
A_FireShotgun
( player_t*	player,
  pspdef_t*	psp ) 
{
	const weaponinfo_t& weapon = weaponinfo[player->readyweapon];

    S_StartSound (player->mo, sfx_shotgn);
    P_SetMobjState (player->mo, S_PLAY_ATK2);

    DecreaseAmmo(player, weapon.ammo, weapon.ammopershot);

    P_SetPsprite (player,
		  ps_flash,
		  (statenum_t)weapon.flashstate);

    P_BulletSlope (player->mo);
	
	damage_t damageflags = weapon.NoThrusting() ? damage_nothrust : damage_none;
    for( int32_t i = 0; i < 7; ++i)
	{
		P_GunShot(player->mo, false, damageflags);
	}
}



//
// A_FireShotgun2
//
DOOM_C_API void
A_FireShotgun2
( player_t*	player,
  pspdef_t*	psp ) 
{
	const weaponinfo_t& weapon = weaponinfo[player->readyweapon];
	damage_t damageflags = weapon.NoThrusting() ? damage_nothrust : damage_none;

    int		i;
    angle_t	angle;
    int		damage;
		
	
    S_StartSound (player->mo, sfx_dshtgn);
    P_SetMobjState (player->mo, S_PLAY_ATK2);

    DecreaseAmmo(player, weapon.ammo, weapon.ammopershot);

    P_SetPsprite (player,
		  ps_flash,
		  (statenum_t)weapon.flashstate);

    P_BulletSlope (player->mo);
	
    for (i=0 ; i<20 ; i++)
    {
		damage = 5*(P_Random ()%3+1);
		angle = player->mo->angle;
		angle += P_SubRandom() << ANGLETOFINESHIFT;
		P_LineAttack (player->mo,
				  angle,
				  MISSILERANGE,
				  bulletslope + (P_SubRandom() << 5), damage, damageflags);
    }
}


//
// A_FireCGun
//
DOOM_C_API void
A_FireCGun
( player_t*	player,
  pspdef_t*	psp ) 
{
	const weaponinfo_t& weapon = weaponinfo[player->readyweapon];

    S_StartSound (player->mo, sfx_pistol);

    if (!player->ammo[weapon.ammo])
	{
		return;
	}
		
    P_SetMobjState (player->mo, S_PLAY_ATK2);
    DecreaseAmmo(player, weapon.ammo, weapon.ammopershot);

    P_SetPsprite( player, ps_flash, weapon.flashstate + psp->state->statenum - S_CHAIN1 );

    P_BulletSlope (player->mo);
	
    P_GunShot(player->mo, !player->refire, weapon.NoThrusting() ? damage_nothrust : damage_none);
}



//
// ?
//
DOOM_C_API void A_Light0 (player_t *player, pspdef_t *psp)
{
    player->extralight = 0;
}

DOOM_C_API void A_Light1 (player_t *player, pspdef_t *psp)
{
    player->extralight = 1;
}

DOOM_C_API void A_Light2 (player_t *player, pspdef_t *psp)
{
    player->extralight = 2;
}


//
// A_BFGSpray
// Spawn a BFG explosion on every monster in view
//
DOOM_C_API void A_BFGSpray (mobj_t* mo) 
{
    int			i;
    int			j;
    int			damage;
    angle_t		an;
	
    // offset angles from its attack angle
    for (i=0 ; i<40 ; i++)
    {
		an = mo->angle - ANG90/2 + ANG90/40*i;

		// mo->target is the originator (player)
		//  of the missile
		P_AimLineAttack (mo->target, an, 16*64*FRACUNIT);

		if (!linetarget)
			continue;

		P_SpawnMobj (linetarget->x,
				 linetarget->y,
				 linetarget->z + (linetarget->height>>2),
				 MT_EXTRABFG);
	
		damage = 0;
		for (j=0;j<15;j++)
			damage += (P_Random()&7) + 1;

		P_DamageMobj (linetarget, mo->target,mo->target, damage, damage_none);
    }
}


//
// A_BFGsound
//
DOOM_C_API void
A_BFGsound
( player_t*	player,
  pspdef_t*	psp )
{
    S_StartSound (player->mo, sfx_bfg);
}



//
// P_SetupPsprites
// Called at start of level for each player.
//
DOOM_C_API void P_SetupPsprites (player_t* player) 
{
    int	i;
	
    // remove all psprites
    for (i=0 ; i<NUMPSPRITES ; i++)
	player->psprites[i].state = NULL;
		
    // spawn the gun
    player->pendingweapon = player->readyweapon;
    P_BringUpWeapon (player);

    for (i=0 ; i<NUMPSPRITES ; i++)
	{
		player->psprites[i].lastx = player->psprites[i].sx;
		player->psprites[i].lasty = player->psprites[i].sy;
		player->psprites[i].duration = player->psprites[i].tics;
	}
}




//
// P_MovePsprites
// Called every tic by player thinking routine.
//
DOOM_C_API void P_MovePsprites (player_t* player) 
{
	int		i;
	pspdef_t*	psp;
	
	psp = &player->psprites[0];
	for (i=0 ; i<NUMPSPRITES ; i++, psp++)
	{
		// a null state means not active
		if ( psp->state != nullptr )	
		{
			// drop tic count and possibly change state

			// a -1 tic count never changes
			if (psp->tics != -1)	
			{
				psp->tics--;
				if (!psp->tics)
				{
					P_SetPsprite (player, i, psp->state->nextstate);
				}
			}
		}
	}

	player->psprites[ps_flash].sx = player->psprites[ps_weapon].sx;
	player->psprites[ps_flash].sy = player->psprites[ps_weapon].sy;
}


