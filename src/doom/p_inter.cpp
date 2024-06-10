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
//	Handling interactions (i.e., collisions).
//




// Data.
#include "doomdef.h"
#include "dstrings.h"
#include "sounds.h"

#include "deh_main.h"
#include "deh_misc.h"
#include "doomstat.h"

#include "m_random.h"
#include "i_system.h"

#include "am_map.h"

#include "p_local.h"

#include "s_sound.h"

#include "p_inter.h"


#define BONUSADD	6

//
// GET STUFF
//

//
// P_GiveAmmo
// Num is the number of clip loads,
// not the individual count (0= 1/2 clip).
// Returns false if the ammo can't be picked up at all
//

bool P_GiveAmmo( player_t* player, const ammoinfo_t* ammoinfo, ammocategory_t category )
{
	if( player->ammo[ ammoinfo->index ] == player->maxammo[ ammoinfo->index ] )
	{
		return false;
	}

	int32_t giveamount = 0;
	switch( category )
	{
	case ac_clip:
		giveamount = ammoinfo->clipammo;
		break;
	case ac_box:
		giveamount = ammoinfo->boxammo;
		break;
	case ac_backpack:
		giveamount = ammoinfo->backpackammo;
		break;
	case ac_weapon:
		giveamount = ammoinfo->weaponammo;
		break;
	case ac_droppedclip:
		giveamount = ammoinfo->droppedclipammo;
		break;
	case ac_droppedweapon:
		giveamount = ammoinfo->droppedweaponammo;
		break;
	case ac_deathmatchweapon:
		giveamount = ammoinfo->deathmatchweaponammo;
		break;
	}

	giveamount = FixedToInt( FixedMul( IntToFixed( giveamount ), ammoinfo->skillmul[ gameskill ] ) );

	int32_t oldammo = player->ammo[ ammoinfo->index ];
	int32_t newamount = M_MIN( oldammo + giveamount, player->maxammo[ ammoinfo->index ] );
	player->ammo[ ammoinfo->index ] = newamount;

	// If non zero ammo, 
	// don't change up weapons,
	// player was lower on purpose.
	if (oldammo)
	{
		return true;
	}

	if( sim.mbf21_code_pointers )
	{
		const weaponinfo_t& weapon = player->Weapon();
		if( !weapon.SwitchFromOnAmmoPickup() )
		{
			return true;
		}

		for( weaponinfo_t* switchtoweapon : weaponinfo.BySwitchPriority() )
		{
			if( !switchtoweapon->DontSwitchToOnAmmoPickup()
				&& switchtoweapon->ammo == ammoinfo->index )
			{
				player->pendingweapon = switchtoweapon->index;
				break;
			}
		}
	}
	else
	{
		switch( ammoinfo->index )
		{
		case am_clip:
			if (player->readyweapon == wp_fist)
			{
				if (player->weaponowned[wp_chaingun])
				{
					player->pendingweapon = wp_chaingun;
				}
				else if (player->weaponowned[wp_pistol])
				{
					player->pendingweapon = wp_pistol;
				}
			}
			break;

		case am_shell:
			if (player->readyweapon == wp_fist
				|| player->readyweapon == wp_pistol)
			{
				if (player->weaponowned[wp_shotgun])
				player->pendingweapon = wp_shotgun;
			}
			break;

		case am_cell:
			if (player->readyweapon == wp_fist
				|| player->readyweapon == wp_pistol)
			{
				if (player->weaponowned[wp_plasma])
				player->pendingweapon = wp_plasma;
			}
			break;

		case am_misl:
			if (player->readyweapon == wp_fist)
			{
				if (player->weaponowned[wp_missile])
				player->pendingweapon = wp_missile;
			}
			break;

		default:
			break;
		}
	}

	return true;
}

//
// P_GiveWeapon
// The weapon name may have a MF_DROPPED flag ored in.
//
bool P_GiveWeapon( player_t* player, weapontype_t weapon, doombool dropped )
{
	const weaponinfo_t& thisweapon = weaponinfo[ weapon ];
	const ammoinfo_t& thisammo = ammoinfo[ thisweapon.ammo ];
	
	if (netgame
		&& deathmatch != 2
		&& !dropped )
	{
		// leave placed weapons forever on net games
		if ( player->weaponowned[ weapon ] )
		{
			return false;
		}

		player->bonuscount += BONUSADD;
		player->weaponowned[ weapon ] = true;

		P_GiveAmmo( player, &thisammo, deathmatch == 1 ? ac_deathmatchweapon : ac_weapon );

		player->pendingweapon = weapon;

		if( player == &players[ consoleplayer ] )
		{
			S_StartSound( NULL, sfx_wpnup );
		}

		return false;
	}
	
	bool gaveammo = false;
	bool gaveweapon = false;

	if( weaponinfo[weapon].ammo != am_noammo )
	{
		gaveammo = P_GiveAmmo( player, &thisammo, dropped ? ac_droppedweapon : ac_weapon );
	}
	
	if( !player->weaponowned[ weapon ] )
	{
		gaveweapon = true;
		player->weaponowned[ weapon ] = true;
		player->pendingweapon = weapon;
	}
	
	return ( gaveweapon || gaveammo );
}

 

//
// P_GiveBody
// Returns false if the body isn't needed at all
//
DOOM_C_API doombool P_GiveBody( player_t* player, int num )
{
    if (player->health >= MAXHEALTH)
	return false;
		
    player->health += num;
    if (player->health > MAXHEALTH)
	player->health = MAXHEALTH;
    player->mo->health = player->health;
	
    return true;
}



//
// P_GiveArmor
// Returns false if the armor is worse
// than the current armor.
//
DOOM_C_API doombool P_GiveArmor( player_t* player, int armortype )
{
    int		hits;
	
    hits = armortype*100;
    if (player->armorpoints >= hits)
	return false;	// don't pick up
		
    player->armortype = armortype;
    player->armorpoints = hits;
	
    return true;
}



//
// P_GiveCard
//
DOOM_C_API bool P_GiveCard( player_t* player, card_t card )
{
    if (player->cards[card]) return false;
    
    player->bonuscount = BONUSADD;
    player->cards[card] = 1;
	return true;
}


//
// P_GivePower
//
DOOM_C_API doombool P_GivePower( player_t* player, int /*powertype_t*/ power )
{
    if (power == pw_invulnerability)
    {
	player->powers[power] = INVULNTICS;
	return true;
    }
    
    if (power == pw_invisibility)
    {
	player->powers[power] = INVISTICS;
	player->mo->flags |= MF_SHADOW;
	return true;
    }
    
    if (power == pw_infrared)
    {
	player->powers[power] = INFRATICS;
	return true;
    }
    
    if (power == pw_ironfeet)
    {
	player->powers[power] = IRONTICS;
	return true;
    }
    
    if (power == pw_strength)
    {
	P_GiveBody (player, 100);
	player->powers[power] = 1;
	return true;
    }
	
    if (player->powers[power])
	return false;	// already got it
		
    player->powers[power] = 1;
    return true;
}


bool P_HandleTouch( const mobjinfo_t* info, player_t* player, bool dropped, int32_t& outsound )
{
	bool handle = info->pickupammotype == am_noammo
				&& info->pickupweapontype == wp_nochange
				&& info->pickupitemtype == item_noitem;

	const char* mnemonic = info->pickupstringmnemonic;

	if( info->pickupammotype != am_noammo )
	{
		handle |= P_GiveAmmo( player, &ammoinfo[ info->pickupammotype ], (ammocategory_t)info->pickupammocategory );
	}

	if( info->pickupweapontype != wp_nochange )
	{
		handle |= P_GiveWeapon( player, (weapontype_t)info->pickupweapontype, dropped );
	}

	switch( info->pickupitemtype )
	{
	case item_bluecard:
		handle |= P_GiveCard( player, it_bluecard );
		break;

	case item_yellowcard:
		handle |= P_GiveCard( player, it_yellowcard );
		break;

	case item_redcard:
		handle |= P_GiveCard( player, it_redcard );
		break;

	case item_blueskull:
		handle |= P_GiveCard( player, it_blueskull );
		break;

	case item_yellowskull:
		handle |= P_GiveCard( player, it_yellowskull );
		break;

	case item_redskull:
		handle |= P_GiveCard( player, it_redskull );
		break;

	case item_backpack:
		handle |= true;
		if( !player->backpack )
		{
			for( ammoinfo_t* ammo : ammoinfo.All() )
			{
				player->maxammo[ ammo->index ] = ammo->maxupgradedammo;
			}
			player->backpack = true;
		}
		for( ammoinfo_t* ammo : ammoinfo.All() )
		{
			P_GiveAmmo( player, ammo, ac_backpack );
		}
		break;

	case item_healthbonus:
		handle |= true;
		player->health++;
		if (player->health > deh_max_health)
		{
			player->health = deh_max_health;
		}
		player->mo->health = player->health;
		break;

	case item_stimpack:
		handle |= P_GiveBody( player, 10 );
		break;

	case item_medikit:
		{
			bool updatemnemonic = fix.really_needed_medikit && player->health < 25;
			bool gavehealth = P_GiveBody( player, 25 );
			handle |= gavehealth;
			if( updatemnemonic )
			{
				mnemonic = "GOTMEDINEED";
			}
		}
		break;

	case item_soulsphere:
		handle |= true;
		player->health += deh_soulsphere_health;
		if( player->health > deh_max_soulsphere )
		{
			player->health = deh_max_soulsphere;
		}
		player->mo->health = player->health;
		break;

	case item_megasphere:
		handle |= true;
		player->health = deh_megasphere_health;
		player->mo->health = player->health;
		P_GiveArmor( player, 2 );
		break;

	case item_armorbonus:
		handle |= true;
		player->armorpoints++;
		if( player->armorpoints > deh_max_armor )
		{
			player->armorpoints = deh_max_armor;
		}
		if( !player->armortype )
		{
			player->armortype = 1;
		}
		break;

	case item_greenarmor:
		handle |= P_GiveArmor( player, deh_green_armor_class );
		break;

	case item_bluearmor:
		handle |= P_GiveArmor( player, deh_blue_armor_class );
		break;

	case item_areamap:
		handle |= P_GivePower( player, pw_allmap );
		break;

	case item_lightamp:
		handle |= P_GivePower( player, pw_infrared );
		break;

	case item_berserk:
		{
			bool gaveberserk = P_GivePower( player, pw_strength );
			handle |= gaveberserk;
			if( gaveberserk && player->readyweapon != wp_fist )
			{
				player->pendingweapon = wp_fist;
			}
		}
		break;

	case item_invisibility:
		handle |= P_GivePower( player, pw_invisibility );
		break;

	case item_radsuit:
		handle |= P_GivePower( player, pw_ironfeet );
		break;

	case item_invulnerability:
		handle |= P_GivePower( player, pw_invulnerability );
		break;

	case item_noitem:
		break;
	}

	if( handle )
	{
		player->message = DEH_StringMnemonic( mnemonic );
		outsound = info->pickupsound;
	}

	return handle;
}

bool P_HandleTouchVanilla( int32_t sprite, player_t* player, bool dropped, int32_t& outsound )
{
    // Identify by sprite.
    switch( sprite )
    {
	// armor
    case SPR_ARM1:
		if (!P_GiveArmor (player, deh_green_armor_class))
			return false;
		player->message = DEH_String(GOTARMOR);
		break;
		
    case SPR_ARM2:
		if (!P_GiveArmor (player, deh_blue_armor_class))
			return false;
		player->message = DEH_String(GOTMEGA);
		break;
	
	// bonus items
    case SPR_BON1:
		player->health++;		// can go over 100%
		if (player->health > deh_max_health)
			player->health = deh_max_health;
		player->mo->health = player->health;
		player->message = DEH_String(GOTHTHBONUS);
		break;
	
    case SPR_BON2:
		player->armorpoints++;		// can go over 100%
		if (player->armorpoints > deh_max_armor && gameversion > exe_doom_1_2)
			player->armorpoints = deh_max_armor;
			// deh_green_armor_class only applies to the green armor shirt;
			// for the armor helmets, armortype 1 is always used.
		if (!player->armortype)
			player->armortype = 1;
		player->message = DEH_String(GOTARMBONUS);
		break;
	
    case SPR_SOUL:
		player->health += deh_soulsphere_health;
		if (player->health > deh_max_soulsphere)
			player->health = deh_max_soulsphere;
		player->mo->health = player->health;
		player->message = DEH_String(GOTSUPER);
		if (gameversion > exe_doom_1_2)
			outsound = sfx_getpow;
		break;
	
    case SPR_MEGA:
		if (gamemode != commercial)
			return false;
		player->health = deh_megasphere_health;
		player->mo->health = player->health;
			// We always give armor type 2 for the megasphere; dehacked only 
			// affects the MegaArmor.
		P_GiveArmor (player, 2);
		player->message = DEH_String(GOTMSPHERE);
		if (gameversion > exe_doom_1_2)
			outsound = sfx_getpow;
		break;
	
	// cards
	// leave cards for everyone
    case SPR_BKEY:
		if (!player->cards[it_bluecard])
			player->message = DEH_String(GOTBLUECARD);
		P_GiveCard (player, it_bluecard);
		if (!netgame)
			break;
		return false;
	
    case SPR_YKEY:
		if (!player->cards[it_yellowcard])
			player->message = DEH_String(GOTYELWCARD);
		P_GiveCard (player, it_yellowcard);
		if (!netgame)
			break;
		return false;
	
    case SPR_RKEY:
		if (!player->cards[it_redcard])
			player->message = DEH_String(GOTREDCARD);
		P_GiveCard (player, it_redcard);
		if (!netgame)
			break;
		return false;
	
    case SPR_BSKU:
		if (!player->cards[it_blueskull])
			player->message = DEH_String(GOTBLUESKUL);
		P_GiveCard (player, it_blueskull);
		if (!netgame)
			break;
		return false;
	
    case SPR_YSKU:
		if (!player->cards[it_yellowskull])
			player->message = DEH_String(GOTYELWSKUL);
		P_GiveCard (player, it_yellowskull);
		if (!netgame)
			break;
		return false;
	
    case SPR_RSKU:
		if (!player->cards[it_redskull])
			player->message = DEH_String(GOTREDSKULL);
		P_GiveCard (player, it_redskull);
		if (!netgame)
			break;
		return false;
	
	// medikits, heals
    case SPR_STIM:
		if (!P_GiveBody (player, 10))
			return false;
		player->message = DEH_String(GOTSTIM);
		break;
	
    case SPR_MEDI:
	{
		int32_t originalhealth = player->health;
		if (!P_GiveBody (player, 25))
			return false;

		if( ( fix.really_needed_medikit ? originalhealth : player->health ) < 25)
			player->message = DEH_String(GOTMEDINEED);
		else
			player->message = DEH_String(GOTMEDIKIT);
		break;
	}
	
	// power ups
    case SPR_PINV:
		if (!P_GivePower (player, pw_invulnerability))
			return false;
		player->message = DEH_String(GOTINVUL);
		if (gameversion > exe_doom_1_2)
			outsound = sfx_getpow;
		break;
	
    case SPR_PSTR:
		if (!P_GivePower (player, pw_strength))
			return false;
		player->message = DEH_String(GOTBERSERK);
		if (player->readyweapon != wp_fist)
			player->pendingweapon = wp_fist;
		if (gameversion > exe_doom_1_2)
			outsound = sfx_getpow;
		break;
	
    case SPR_PINS:
		if (!P_GivePower (player, pw_invisibility))
			return false;
		player->message = DEH_String(GOTINVIS);
		if (gameversion > exe_doom_1_2)
			outsound = sfx_getpow;
		break;
	
    case SPR_SUIT:
		if (!P_GivePower (player, pw_ironfeet))
			return false;
		player->message = DEH_String(GOTSUIT);
		if (gameversion > exe_doom_1_2)
			outsound = sfx_getpow;
		break;
	
    case SPR_PMAP:
		if (!P_GivePower (player, pw_allmap))
			return false;
		player->message = DEH_String(GOTMAP);
		if (gameversion > exe_doom_1_2)
			outsound = sfx_getpow;
		break;
	
    case SPR_PVIS:
		if (!P_GivePower (player, pw_infrared))
			return false;
		player->message = DEH_String(GOTVISOR);
		if (gameversion > exe_doom_1_2)
			outsound = sfx_getpow;
		break;
	
	// ammo
    case SPR_CLIP:
		if (dropped)
		{
			if (!P_GiveAmmo (player, &ammoinfo[ am_clip ], ac_droppedclip ))
			return false;
		}
		else
		{
			if (!P_GiveAmmo (player, &ammoinfo[ am_clip ], ac_clip ))
			return false;
		}
		player->message = DEH_String(GOTCLIP);
		break;
	
    case SPR_AMMO:
		if (!P_GiveAmmo (player, &ammoinfo[ am_clip ], ac_box ))
			return false;
		player->message = DEH_String(GOTCLIPBOX);
		break;
	
    case SPR_ROCK:
		if (!P_GiveAmmo (player, &ammoinfo[ am_misl ], ac_clip ))
			return false;
		player->message = DEH_String(GOTROCKET);
		break;
	
    case SPR_BROK:
		if (!P_GiveAmmo (player, &ammoinfo[ am_misl ], ac_box ))
			return false;
		player->message = DEH_String(GOTROCKBOX);
		break;
	
    case SPR_CELL:
		if (!P_GiveAmmo (player, &ammoinfo[ am_cell ], ac_clip ))
			return false;
		player->message = DEH_String(GOTCELL);
		break;
	
    case SPR_CELP:
		if (!P_GiveAmmo (player, &ammoinfo[ am_cell ], ac_box ))
			return false;
		player->message = DEH_String(GOTCELLBOX);
		break;
	
    case SPR_SHEL:
		if (!P_GiveAmmo (player, &ammoinfo[ am_shell ], ac_clip ))
			return false;
		player->message = DEH_String(GOTSHELLS);
		break;
	
    case SPR_SBOX:
		if (!P_GiveAmmo (player, &ammoinfo[ am_shell ], ac_box ))
			return false;
		player->message = DEH_String(GOTSHELLBOX);
		break;
	
    case SPR_BPAK:
		if (!player->backpack)
		{
			for (int32_t i=0 ; i<NUMAMMO ; i++)
			player->maxammo[i] *= 2;
			player->backpack = true;
		}
		for (int32_t i=0 ; i<NUMAMMO ; i++)
			P_GiveAmmo (player, &ammoinfo[ i ], ac_clip );
		player->message = DEH_String(GOTBACKPACK);
		break;
	
	// weapons
    case SPR_BFUG:
		if (!P_GiveWeapon (player, wp_bfg, false) )
			return false;
		player->message = DEH_String(GOTBFG9000);
		outsound = sfx_wpnup;	
		break;
	
    case SPR_MGUN:
		if (!P_GiveWeapon(player, wp_chaingun, dropped))
			return false;
		player->message = DEH_String(GOTCHAINGUN);
		outsound = sfx_wpnup;	
		break;
	
    case SPR_CSAW:
		if (!P_GiveWeapon (player, wp_chainsaw, false) )
			return false;
		player->message = DEH_String(GOTCHAINSAW);
		outsound = sfx_wpnup;	
		break;
	
    case SPR_LAUN:
		if (!P_GiveWeapon (player, wp_missile, false) )
			return false;
		player->message = DEH_String(GOTLAUNCHER);
		outsound = sfx_wpnup;	
		break;
	
    case SPR_PLAS:
		if (!P_GiveWeapon (player, wp_plasma, false) )
			return false;
		player->message = DEH_String(GOTPLASMA);
		outsound = sfx_wpnup;	
		break;
	
    case SPR_SHOT:
		if (!P_GiveWeapon(player, wp_shotgun, dropped))
			return false;
		player->message = DEH_String(GOTSHOTGUN);
		outsound = sfx_wpnup;	
		break;
		
    case SPR_SGN2:
		if (!P_GiveWeapon(player, wp_supershotgun, dropped))
			return false;
		player->message = DEH_String(GOTSHOTGUN2);
		outsound = sfx_wpnup;	
		break;
		
    default:
		I_Error ("P_SpecialThing: Unknown gettable thing");
		break;
	}

	return true;
}

//
// P_TouchSpecialThing
//
DOOM_C_API void P_TouchSpecialThing( mobj_t* special, mobj_t* toucher )
{
    player_t*	player;
    fixed_t	delta;
    int		sound;
		
    delta = special->z - toucher->z;

    if (delta > toucher->height
	|| delta < -8*FRACUNIT)
    {
	// out of reach
	return;
    }
    
	
	sound = sfx_itemup;	
	player = toucher->player;

	// Dead thing touching.
	// Can happen with a sliding player corpse.
	if( toucher->health <= 0 )
	{
		return;
	}

	bool handled = false;
	if( sim.rnr24_code_pointers )
	{
		handled = P_HandleTouch( special->info, player, ( special->flags & MF_DROPPED ) != 0, sound );
	}
	else
	{
		handled = P_HandleTouchVanilla( special->sprite, player, ( special->flags & MF_DROPPED ) != 0, sound );
	}

	if( handled )
	{
		if( special->CountItem() )
		{
			player->itemcount++;
			++session.total_found_items_global;
			++session.total_found_items[ player - players ];
		}

		if( special->RemoveOnPickup() )
		{
			P_RemoveMobj( special );
		}

		player->bonuscount += BONUSADD;
		if( player == &players[ consoleplayer ] )
		{
			S_StartSound (NULL, sound);
		}
	}
}


//
// KillMobj
//
DOOM_C_API void P_KillMobj( mobj_t* source, mobj_t* target )
{
	int32_t	item;
	mobj_t*	mo;
	
	target->flags &= ~(MF_SHOOTABLE|MF_FLOAT|MF_SKULLFLY);

	if (target->type != MT_SKULL)
	target->flags &= ~MF_NOGRAVITY;

	target->flags |= MF_CORPSE|MF_DROPOFF;
	target->height >>= 2;

	if( target->flags & MF_COUNTKILL )
	{
		if( target->resurrection_count > 0 ) ++session.total_resurrected_monster_kills_global;
		else ++session.total_monster_kills_global;
	}

	if (source && source->player)
	{
		// count for intermission
		if (target->flags & MF_COUNTKILL)
		{
			source->player->killcount++;
			if( target->resurrection_count > 0 ) ++session.total_resurrected_monster_kills[ source->player - players ];
			else ++session.total_monster_kills[ source->player - players ];
		}

		if (target->player)
			source->player->frags[target->player-players]++;

	}
	else if (!netgame && (target->flags & MF_COUNTKILL) )
	{
		// count all monster deaths,
		// even those caused by other monsters
		players[0].killcount++;
		if( target->resurrection_count > 0 ) ++session.total_resurrected_monster_kills[ 0 ];
		else ++session.total_monster_kills[ 0 ];
	}
    
    if (target->player)
    {
	// count environment kills against you
	if (!source)	
	    target->player->frags[target->player-players]++;
			
	target->flags &= ~MF_SOLID;
	target->player->playerstate = PST_DEAD;
	P_DropWeapon (target->player);

	if (target->player == &players[consoleplayer]
	    && automapactive)
	{
	    // don't die in auto map,
	    // switch view prior to dying
	    AM_Stop ();
	}
	
    }

    if (target->health < -target->info->spawnhealth 
	&& target->info->xdeathstate)
    {
	P_SetMobjState (target, (statenum_t)target->info->xdeathstate);
    }
    else
	P_SetMobjState (target, (statenum_t)target->info->deathstate);
    target->tics -= P_Random()&3;

    if (target->tics < 1)
	target->tics = 1;
		
    //	I_StartSound (&actor->r, actor->info->deathsound);

    // In Chex Quest, monsters don't drop items.

    if (gameversion == exe_chex)
    {
        return;
    }

    // Drop stuff.
    // This determines the kind of object spawned
    // during the death frame of a thing.
	item = target->info->dropthing;

	if( item >= 0 )
	{
		mo = P_SpawnMobj(target->x, target->y, ONFLOORZ, item);
		mo->flags |= MF_DROPPED;	// special versions of items
	}
}




//
// P_DamageMobj
// Damages both enemies and players
// "inflictor" is the thing that caused the damage
//  creature or missile, can be NULL (slime, etc)
// "source" is the thing to target after taking damage
//  creature or NULL
// Source and inflictor are the same for melee attacks.
// Source can be NULL for slime, barrel explosions
// and other environmental stuff.
//
DOOM_C_API void P_DamageMobjEx( mobj_t* target, mobj_t* inflictor, mobj_t* source, int32_t damage, damage_t flags )
{
	unsigned	ang;
	int		saved;
	player_t*	player;
	fixed_t	thrust;
	int		temp;
	
	if ( !( flags & damage_alsononshootables )
		&& !(target->flags & MF_SHOOTABLE) )
	{
		return;	// shouldn't happen...
	}

	if (target->health <= 0)
	{
		return;
	}

	if ( target->flags & MF_SKULLFLY )
	{
		target->momx = target->momy = target->momz = 0;
	}
	
	player = target->player;
	if (player && gameskill == sk_baby)
	{
		damage >>= 1; 	// take half damage in trainer mode
	}

	// Some close combat weapons should not
	// inflict thrust and push the victim out of reach,
	// thus kick away unless using the chainsaw.
	if ( !( flags & damage_nothrust )
		&& inflictor
		&& !(target->flags & MF_NOCLIP))
	{
		ang = BSP_PointToAngle ( inflictor->x,
					inflictor->y,
					target->x,
					target->y);
		
		thrust = damage*(FRACUNIT>>3)*100/target->info->mass;

		// make fall forwards sometimes
		if ( damage < 40
				&& damage > target->health
				&& target->z - inflictor->z > 64*FRACUNIT
				&& (P_Random ()&1) )
		{
			ang += ANG180;
			thrust *= 4;
		}
		
		ang >>= ANGLETOFINESHIFT;
		target->momx += FixedMul (thrust, finecosine[ang]);
		target->momy += FixedMul (thrust, finesine[ang]);
	}

	// player specific
	if (player)
	{
		// end of game hell hack
		doombool hellhacksector = ( target->subsector->sector->special & ~DSS_Mask ) == 0
								&& target->subsector->sector->special == DSS_20DamageAndEnd;

		if( hellhacksector
			&& damage >= target->health)
		{
			damage = target->health - 1;
		}

		// Below certain threshold,
		// ignore damage in GOD mode, or with INVUL power.
		if ( damage < 1000
				&& ( (player->cheats&CF_GODMODE)
				|| player->powers[pw_invulnerability] ) )
		{
			return;
		}
	
		if( !(flags & damage_ignorearmor )
			&& player->armortype )
		{
			if (player->armortype == 1)
				saved = damage/3;
			else
				saved = damage/2;

			if (player->armorpoints <= saved)
			{
				// armor is used up
				saved = player->armorpoints;
				player->armortype = 0;
			}
			player->armorpoints -= saved;
			damage -= saved;
		}

		player->health -= damage; 	// mirror mobj health here for Dave

		if (player->health < 0)
		{
			player->health = 0;
		}
	
		player->attacker = source;
		player->damagecount += damage;	// add damage after armor / invuln

		if (player->damagecount > 100)
			player->damagecount = 100;	// teleport stomp does 10k points...
	
		temp = damage < 100 ? damage : 100;

		if (player == &players[consoleplayer])
			I_Tactile (40,10,40+temp*2);
	}

	// do the damage	
	target->health -= damage;
	if (target->health <= 0)
	{
		P_KillMobj (source, target);
		return;
	}

	if ( (P_Random () < target->info->painchance)
		&& !(target->flags&MF_SKULLFLY) )
	{
		target->flags |= MF_JUSTHIT;	// fight back!
	
		P_SetMobjState (target, (statenum_t)target->info->painstate);
	}
			
	target->reactiontime = 0;		// we're awake now...	

	bool infightingimmunity = InfightingImmunity( source, target );

	bool ignorevile = source
					&& ( source->type == MT_VILE
						|| source->MonstersIgnoreDamage()
						);

	bool nothreshold = ( target && ( !target->threshold || target->type == MT_VILE) )
						|| ( source && source->NoTargetingThreshold() );

	if( !infightingimmunity
		&& nothreshold
		&& source && (source != target || gameversion <= exe_doom_1_2)
		&& !ignorevile )
	{
		// if not intent on another player,
		// chase after this one
		target->target = source;
		target->threshold = BASETHRESHOLD;
		if (target->state == &states[target->info->spawnstate]
			&& target->info->seestate != S_NULL)
			P_SetMobjState (target, (statenum_t)target->info->seestate);
	}
}

DOOM_C_API void P_DamageMobj( mobj_t* target, mobj_t* inflictor, mobj_t* source, int32_t damage, damage_t flags )
{
	P_DamageMobjEx( target, inflictor, source, damage, flags );
}