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
//	Player related stuff.
//	Bobbing POV/weapon, movement.
//	Pending weapon.
//




#include "doomdef.h"
#include "d_event.h"

#include "m_misc.h"

#include "p_local.h"

#include "doomstat.h"

#include "m_conv.h"

// Index of the special effects (INVUL inverse) map.
#define INVERSECOLORMAP		32

extern "C"
{
	extern int32_t view_bobbing_percent;
}

//
// Movement.
//

// 16 pixels of bob
#define MAXBOB	0x100000	

doombool		onground;


//
// P_Thrust
// Moves the given origin along a given angle.
//
DOOM_C_API void P_Thrust( player_t* player,  angle_t angle, fixed_t move ) 
{
	uint32_t finelookup = FINEANGLE( angle );

	player->mo->momx += FixedMul( move, finecosine[ finelookup ] );
	player->mo->momy += FixedMul( move, finesine[ finelookup ] );
}




//
// P_CalcHeight
// Calculate the walking / running height adjustment
//
DOOM_C_API void P_CalcHeight (player_t* player) 
{
    int		angle;
    fixed_t	bob;
    
    // Regular movement bobbing
    // (needs to be calculated for gun swing
    // even if not on ground)
    // OPTIMIZE: tablify angle
    // Note: a LUT allows for effects
    //  like a ramp with low health.
    player->bob =
	FixedMul (player->mo->momx, player->mo->momx)
	+ FixedMul (player->mo->momy,player->mo->momy);
    
    player->bob >>= 2;

    if (player->bob>MAXBOB)
	player->bob = MAXBOB;

    if ((player->cheats & CF_NOMOMENTUM) || !onground)
    {
	player->viewz = player->mo->z + VIEWHEIGHT;

	if (player->viewz > player->mo->ceilingz-4*FRACUNIT)
	    player->viewz = player->mo->ceilingz-4*FRACUNIT;

	player->viewz = player->mo->z + player->viewheight;
	return;
    }
		
    angle = (FINEANGLES/20*leveltime)&FINEMASK;
    bob = FixedMul ( player->bob/2, finesine[angle]);

    
    // move viewheight
    if (player->playerstate == PST_LIVE)
    {
	player->viewheight += player->deltaviewheight;

	if (player->viewheight > VIEWHEIGHT)
	{
	    player->viewheight = VIEWHEIGHT;
	    player->deltaviewheight = 0;
	}

	if (player->viewheight < VIEWHEIGHT/2)
	{
	    player->viewheight = VIEWHEIGHT/2;
	    if (player->deltaviewheight <= 0)
		player->deltaviewheight = 1;
	}
	
	if (player->deltaviewheight)	
	{
	    player->deltaviewheight += FRACUNIT/4;
	    if (!player->deltaviewheight)
		player->deltaviewheight = 1;
	}
    }

	fixed_t percent = FixedDiv( IntToFixed( view_bobbing_percent ), IntToFixed( 100 ) );
	player->viewz = player->mo->z + player->viewheight + FixedMul( bob, percent );

    if (player->viewz > player->mo->ceilingz-4*FRACUNIT)
	player->viewz = player->mo->ceilingz-4*FRACUNIT;
}



//
// P_MovePlayer
//
DOOM_C_API void P_MovePlayer (player_t* player)
{
	ticcmd_t*		cmd;
	
	cmd = &player->cmd;
	
	player->mo->angle += (cmd->angleturn<<FRACBITS);
	if( cmd->pitchturn == 0x7FFF )
	{
		player->viewpitch = 0;
	}
	else
	{
		player->viewpitch += (cmd->pitchturn << FRACBITS);
		if( player->viewpitch > ANG180 ) player->viewpitch = M_CLAMP( player->viewpitch, Negate( MaxViewPitchAngle ), 0xFFFFFFFFu );
		else player->viewpitch = M_CLAMP( player->viewpitch, 0, MaxViewPitchAngle );
	}

	// Do not let the player control movement
	//  if not onground.
	onground = (player->mo->z <= player->mo->floorz);
	bool onsectorground = player->mo->z <= player->mo->subsector->sector->floorheight;
	int32_t frictionmultiplier = 2048;

	if( onsectorground
		&& player->anymomentumframes != 0
		&& !( player->mo->flags & MF_NOCLIP ) )
	{
		// Look up table to accommodate:
		// * Slippery surfaces have a harsher effect on how you're allowed to accelerate
		// * Sticky surfaces have a less harsh effect
		frictionmultiplier = player->mo->subsector->sector->FrictionMultiplier();
	}
	
	if (cmd->forwardmove && onground)
	{
		P_Thrust (player, player->mo->angle, cmd->forwardmove * frictionmultiplier);
	}

	if (cmd->sidemove && onground)
	{
		P_Thrust (player, player->mo->angle-ANG90, cmd->sidemove * frictionmultiplier);
	}

	if( player->mo->momx != 0 || player->mo->momy != 0 )
	{
		++player->anymomentumframes;
	}
	else
	{
		player->anymomentumframes = 0;
	}

	if ( (cmd->forwardmove || cmd->sidemove) 
		&& player->mo->state == &states[S_PLAY] )
	{
		P_SetMobjState (player->mo, S_PLAY_RUN1);
	}
}	



//
// P_DeathThink
// Fall on your face when dying.
// Decrease POV height to floor height.
//
#define ANG5   	(ANG90/18)

DOOM_C_API void P_DeathThink (player_t* player)
{
    angle_t		angle;
    angle_t		delta;

    P_MovePsprites (player);
	
    // fall to the ground
    if (player->viewheight > 6*FRACUNIT)
	player->viewheight -= FRACUNIT;

    if (player->viewheight < 6*FRACUNIT)
	player->viewheight = 6*FRACUNIT;

    player->deltaviewheight = 0;
    onground = (player->mo->z <= player->mo->floorz);
    P_CalcHeight (player);
	
    if (player->attacker && player->attacker != player->mo)
    {
	angle = BSP_PointToAngle (player->mo->x,
				 player->mo->y,
				 player->attacker->x,
				 player->attacker->y);
	
	delta = angle - player->mo->angle;
	
	if (delta < ANG5 || delta > (unsigned)-ANG5)
	{
	    // Looking at killer,
	    //  so fade damage flash down.
	    player->mo->angle = angle;

	    if (player->damagecount)
		player->damagecount--;
	}
	else if (delta < ANG180)
	    player->mo->angle += ANG5;
	else
	    player->mo->angle -= ANG5;
    }
    else if (player->damagecount)
	player->damagecount--;
	

    if (player->cmd.buttons & BT_USE)
	player->playerstate = PST_REBORN;
}



//
// P_PlayerThink
//
DOOM_C_API void P_PlayerThink (player_t* player)
{
    ticcmd_t*		cmd;
    weapontype_t	newweapon;
	
    // fixme: do this in the cheat code
    if (player->cheats & CF_NOCLIP)
	player->mo->flags |= MF_NOCLIP;
    else
	player->mo->flags &= ~MF_NOCLIP;
    
    // chain saw run forward
    cmd = &player->cmd;
    if (player->mo->flags & MF_JUSTATTACKED)
    {
	cmd->angleturn = 0;
	cmd->forwardmove = 0xc800/512;
	cmd->sidemove = 0;
	player->mo->flags &= ~MF_JUSTATTACKED;
    }
			
	
    if (player->playerstate == PST_DEAD)
    {
	P_DeathThink (player);
	return;
    }
    
    // Move around.
    // Reactiontime is used to prevent movement
    //  for a bit after a teleport.
    if (player->mo->reactiontime)
	player->mo->reactiontime--;
    else
	P_MovePlayer (player);
    
    P_CalcHeight (player);

    if (player->mo->subsector->sector->special)
	{
		P_PlayerInSpecialSector (player);
	}
    
    // Check for weapon change.

    // A special event has no other buttons.
    if (cmd->buttons & BT_SPECIAL)
	cmd->buttons = 0;			
		
    if (cmd->buttons & BT_CHANGE)
    {
		if( sim.rnr24_code_pointers )
		{
			int32_t newslot = ( (cmd->buttons & BT_WEAPONMASK) >> BT_WEAPONSHIFT ) + 1;
			if( newslot == 10 )
			{
				newslot = 0;
			}

			const weaponinfo_t* firstinslot = player->Weapon().slot == newslot ? weaponinfo.NextInSlot( &player->Weapon() )
																				: weaponinfo.NextInSlot( newslot );
			if( firstinslot != nullptr )
			{
				const weaponinfo_t* nextinslot = firstinslot;
				do
				{
					// Basic requirements to be a valid weapon and not the current weapon
					if( gamemode >= nextinslot->mingamemode
						&& player->weaponowned[ nextinslot->index ]
						&& player->readyweapon != nextinslot->index )
					{
						bool allowswitch = nextinslot->allowswitchifowneditem == -1
										&& nextinslot->noswitchifowneditem == -1
										&& nextinslot->allowswitchifownedweapon == -1
										&& nextinslot->noswitchifownedweapon == -1;
						bool disallowswitch = false;

						if( !allowswitch )
						{
							if( nextinslot->allowswitchifowneditem != -1 && P_EvaluateItemOwned( (itemtype_t)nextinslot->allowswitchifowneditem, player ) )
							{
								allowswitch = true;
							}
							if( !allowswitch && nextinslot->noswitchifowneditem != -1 && P_EvaluateItemOwned( (itemtype_t)nextinslot->noswitchifowneditem, player ) )
							{
								disallowswitch = true;
							}

							if( !allowswitch && !disallowswitch && nextinslot->allowswitchifownedweapon != -1 && player->weaponowned[ nextinslot->allowswitchifownedweapon ] )
							{
								allowswitch = true;
							}
							if( !allowswitch && !disallowswitch && nextinslot->noswitchifownedweapon != -1 && player->weaponowned[ nextinslot->noswitchifownedweapon ] )
							{
								disallowswitch = true;
							}
						}

						if( allowswitch && !disallowswitch )
						{
							player->pendingweapon = nextinslot->index;
						}
						break;
					}
					nextinslot = weaponinfo.NextInSlot( nextinslot );
				} while( nextinslot != firstinslot );
			}
		}
		else
		{
			// The actual changing of the weapon is done
			//  when the weapon psprite can do it
			//  (read: not in the middle of an attack).
			newweapon = (weapontype_t)( (cmd->buttons&BT_WEAPONMASK)>>BT_WEAPONSHIFT );
			if( newweapon < 8 )
			{
				if (newweapon == wp_fist
					&& player->weaponowned[wp_chainsaw]
					&& !(player->readyweapon == wp_chainsaw
					 && player->powers[pw_strength]))
				{
					newweapon = wp_chainsaw;
				}
	
				if ( (gamemode == commercial)
					&& newweapon == wp_shotgun 
					&& player->weaponowned[wp_supershotgun]
					&& player->readyweapon != wp_supershotgun)
				{
					newweapon = wp_supershotgun;
				}
	

				if (player->weaponowned[newweapon]
					&& newweapon != player->readyweapon)
				{
					// Do not go to plasma or BFG in shareware,
					//  even if cheated.
					if ((newweapon != wp_plasma
					 && newweapon != wp_bfg)
					|| (gamemode != shareware) )
					{
					player->pendingweapon = newweapon;
					}
				}
			}
		}
    }
    
    // check for use
    if (cmd->buttons & BT_USE)
    {
	if (!player->usedown)
	{
	    P_UseLines (player);
	    player->usedown = true;
	}
    }
    else
	player->usedown = false;
    
    // cycle psprites
    P_MovePsprites (player);
    
    // Counters, time dependend power ups.

    // Strength counts up to diminish fade.
    if (player->powers[pw_strength])
	player->powers[pw_strength]++;	
		
    if (player->powers[pw_invulnerability])
	player->powers[pw_invulnerability]--;

    if (player->powers[pw_invisibility])
	if (! --player->powers[pw_invisibility] )
	    player->mo->flags &= ~MF_SHADOW;
			
    if (player->powers[pw_infrared])
	player->powers[pw_infrared]--;
		
    if (player->powers[pw_ironfeet])
	player->powers[pw_ironfeet]--;
		
    if (player->damagecount)
	player->damagecount--;
		
    if (player->bonuscount)
	player->bonuscount--;

    
    // Handling colormaps.
    if (player->powers[pw_invulnerability])
    {
	if (player->powers[pw_invulnerability] > 4*32
	    || (player->powers[pw_invulnerability]&8) )
	    player->fixedcolormap = INVERSECOLORMAP;
	else
	    player->fixedcolormap = 0;
    }
    else if (player->powers[pw_infrared])	
    {
	if (player->powers[pw_infrared] > 4*32
	    || (player->powers[pw_infrared]&8) )
	{
	    // almost full bright
	    player->fixedcolormap = 1;
	}
	else
	    player->fixedcolormap = 0;
    }
    else
	player->fixedcolormap = 0;
}


DOOM_C_API doombool P_EvaluateItemOwned( itemtype_t item, player_t* player )
{
	switch( item )
	{
	case item_bluecard:
	case item_yellowcard:
	case item_redcard:
	case item_blueskull:
	case item_yellowskull:
	case item_redskull:
		return player->cards[ item - item_bluecard ] != 0;

	case item_backpack:
		return player->maxammo[ player->readyweapon ] == weaponinfo[ player->readyweapon ].AmmoInfo().maxupgradedammo;

	case item_greenarmor:
	case item_bluearmor:
		return player->armortype == ( item - item_greenarmor + 1 );

	case item_areamap:
		return player->powers[ pw_allmap ] != 0;

	case item_lightamp:
		return player->powers[ pw_infrared ] != 0;

	case item_berserk:
		return player->powers[ pw_strength ] != 0;

	case item_invisibility:
		return player->powers[ pw_invisibility ] != 0;

	case item_radsuit:
		return player->powers[ pw_ironfeet ] != 0;

	case item_invulnerability:
		return player->powers[ pw_invulnerability ] != 0;

	case item_healthbonus:
	case item_stimpack:
	case item_medikit:
	case item_soulsphere:
	case item_megasphere:
	case item_armorbonus:
	case item_noitem:
	case item_messageonly:
	default:
		break;
	}

	return false;
}
