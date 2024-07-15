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
//	Status bar code.
//	Does the face/direction indicator animatin.
//	Does palette indicators as well (red pain/berserk, bright pickup)
//



#include <stdio.h>
#include <ctype.h>

#include "d_gamesim.h"

#include "i_system.h"
#include "i_video.h"
#include "z_zone.h"

#include "m_conv.h"
#include "m_misc.h"
#include "m_random.h"
#include "m_container.h"
#include "w_wad.h"

#include "deh_main.h"
#include "deh_misc.h"
#include "doomdef.h"
#include "doomkeys.h"

#include "g_game.h"

#include "p_local.h"
#include "st_stuff.h"
#include "st_lib.h"
#include "r_local.h"

#include "p_local.h"
#include "p_inter.h"

#include "am_map.h"
#include "m_cheat.h"

#include "s_sound.h"

// Needs access to LFB.
#include "v_video.h"

// State.
#include "doomstat.h"

// Data.
#include "dstrings.h"
#include "sounds.h"

#include "m_profile.h"

//
// STATUS BAR DATA
//


// Palette indices.
// For damage/bonus red-/gold-shifts
#define STARTREDPALS		1
#define STARTBONUSPALS		9
#define NUMREDPALS			8
#define NUMBONUSPALS		4
// Radiation suit, green shift.
#define RADIATIONPAL		13

// N/256*100% probability
//  that the normal face state will change
#define ST_FACEPROBABILITY		96

// For Responder
#define ST_TOGGLECHAT		KEY_ENTER

// Location of status bar
#define ST_X				0
#define ST_X2				104

#define ST_FX  			143
#define ST_FY  			169

// Should be set to patch width
//  for tall numbers later on
#define ST_TALLNUMWIDTH		(tallnum[0]->width)

// Number of status faces.
#define ST_NUMPAINFACES		5
#define ST_NUMSTRAIGHTFACES	3
#define ST_NUMTURNFACES		2
#define ST_NUMSPECIALFACES		3

#define ST_FACESTRIDE \
          (ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES)

#define ST_NUMEXTRAFACES		2

#define ST_NUMFACES \
          (ST_FACESTRIDE*ST_NUMPAINFACES+ST_NUMEXTRAFACES)

#define ST_TURNOFFSET		(ST_NUMSTRAIGHTFACES)
#define ST_OUCHOFFSET		(ST_TURNOFFSET + ST_NUMTURNFACES)
#define ST_EVILGRINOFFSET		(ST_OUCHOFFSET + 1)
#define ST_RAMPAGEOFFSET		(ST_EVILGRINOFFSET + 1)
#define ST_GODFACE			(ST_NUMPAINFACES*ST_FACESTRIDE)
#define ST_DEADFACE			(ST_GODFACE+1)

#define ST_FACESX			143
#define ST_FACESY			168

#define ST_EVILGRINCOUNT		(2*TICRATE)
#define ST_STRAIGHTFACECOUNT	(TICRATE/2)
#define ST_TURNCOUNT		(1*TICRATE)
#define ST_OUCHCOUNT		(1*TICRATE)
#define ST_RAMPAGEDELAY		(2*TICRATE)

#define ST_MUCHPAIN			20


// Location and size of statistics,
//  justified according to widget type.
// Problem is, within which space? STbar? Screen?
// Note: this could be read in by a lump.
//       Problem is, is the stuff rendered
//       into a buffer,
//       or into the frame buffer?

// AMMO number pos.
#define ST_AMMOWIDTH		3	
#define ST_AMMOX			44
#define ST_AMMOY			171

// HEALTH number pos.
#define ST_HEALTHWIDTH		3	
#define ST_HEALTHX			90
#define ST_HEALTHY			171

// Weapon pos.
#define ST_ARMSX			111
#define ST_ARMSY			172
#define ST_ARMSBGX			104
#define ST_ARMSBGY			168
#define ST_ARMSXSPACE		12
#define ST_ARMSYSPACE		10

// Frags pos.
#define ST_FRAGSX			138
#define ST_FRAGSY			171	
#define ST_FRAGSWIDTH		2

// ARMOR number pos.
#define ST_ARMORWIDTH		3
#define ST_ARMORX			221
#define ST_ARMORY			171

// Key icon positions.
#define ST_KEY0WIDTH		8
#define ST_KEY0HEIGHT		5
#define ST_KEY0X			239
#define ST_KEY0Y			171
#define ST_KEY1WIDTH		ST_KEY0WIDTH
#define ST_KEY1X			239
#define ST_KEY1Y			181
#define ST_KEY2WIDTH		ST_KEY0WIDTH
#define ST_KEY2X			239
#define ST_KEY2Y			191

// Ammunition counter.
#define ST_AMMO0WIDTH		3
#define ST_AMMO0HEIGHT		6
#define ST_AMMO0X			288
#define ST_AMMO0Y			173
#define ST_AMMO1WIDTH		ST_AMMO0WIDTH
#define ST_AMMO1X			288
#define ST_AMMO1Y			179
#define ST_AMMO2WIDTH		ST_AMMO0WIDTH
#define ST_AMMO2X			288
#define ST_AMMO2Y			191
#define ST_AMMO3WIDTH		ST_AMMO0WIDTH
#define ST_AMMO3X			288
#define ST_AMMO3Y			185

// Indicate maximum ammunition.
// Only needed because backpack exists.
#define ST_MAXAMMO0WIDTH		3
#define ST_MAXAMMO0HEIGHT		5
#define ST_MAXAMMO0X		314
#define ST_MAXAMMO0Y		173
#define ST_MAXAMMO1WIDTH		ST_MAXAMMO0WIDTH
#define ST_MAXAMMO1X		314
#define ST_MAXAMMO1Y		179
#define ST_MAXAMMO2WIDTH		ST_MAXAMMO0WIDTH
#define ST_MAXAMMO2X		314
#define ST_MAXAMMO2Y		191
#define ST_MAXAMMO3WIDTH		ST_MAXAMMO0WIDTH
#define ST_MAXAMMO3X		314
#define ST_MAXAMMO3Y		185

// pistol
#define ST_WEAPON0X			110 
#define ST_WEAPON0Y			172

// shotgun
#define ST_WEAPON1X			122 
#define ST_WEAPON1Y			172

// chain gun
#define ST_WEAPON2X			134 
#define ST_WEAPON2Y			172

// missile launcher
#define ST_WEAPON3X			110 
#define ST_WEAPON3Y			181

// plasma gun
#define ST_WEAPON4X			122 
#define ST_WEAPON4Y			181

 // bfg
#define ST_WEAPON5X			134
#define ST_WEAPON5Y			181

// WPNS title
#define ST_WPNSX			109 
#define ST_WPNSY			191

 // DETH title
#define ST_DETHX			109
#define ST_DETHY			191

//Incoming messages window location
//UNUSED
// #define ST_MSGTEXTX	   (viewwindowx)
// #define ST_MSGTEXTY	   (viewwindowy+viewheight-18)
#define ST_MSGTEXTX			0
#define ST_MSGTEXTY			0
// Dimensions given in characters.
#define ST_MSGWIDTH			52
// Or shall I say, in lines?
#define ST_MSGHEIGHT		1

#define ST_OUTTEXTX			0
#define ST_OUTTEXTY			6

// Width, in characters again.
#define ST_OUTWIDTH			52 
 // Height, in lines. 
#define ST_OUTHEIGHT		1

#define ST_MAPTITLEX \
    (V_VIRTUALWIDTH - ST_MAPWIDTH * ST_CHATFONTWIDTH)

#define ST_MAPTITLEY		0
#define ST_MAPHEIGHT		1

// main player in game
static player_t*	plyr; 

// ST_Start() has just been called
static doombool		st_firsttime;

// lump number for PLAYPAL
static int		lu_palette;

// used for timing
static unsigned int	st_clock;

// used for making messages go away
static int		st_msgcounter=0;

// used when in chat 
static st_chatstateenum_t	st_chatstate;

// whether in automap or first-person
static st_stateenum_t	st_gamestate;

// whether left-side main status bar is active
static doombool		st_statusbaron;

// whether status bar chat is active
static doombool		st_chat;

// value of st_chat before message popped up
static doombool		st_oldchat;

// whether chat window has the cursor on
static doombool		st_cursoron;

// !deathmatch
static doombool		st_notdeathmatch; 

// !deathmatch && st_statusbaron
static doombool		st_armson;

// !deathmatch
static doombool		st_fragson; 

// main bar left
static patch_t*		sbar;

// main bar right, for doom 1.0
static patch_t*		sbarr;

// 0-9, tall numbers
static patch_t*		tallnum[10];

// tall % sign
static patch_t*		tallpercent;

// 0-9, short, yellow (,different!) numbers
static patch_t*		shortnum[10];

#define TOTALCARDS NUMCARDS + 3
// 3 key-cards, 3 skulls
static patch_t*		keys[TOTALCARDS]; 

// face status patches
static patch_t*		faces[ST_NUMFACES];

// face background
static patch_t*		faceback;

 // main bar right
static patch_t*		armsbg;

// weapon ownership patches
static patch_t*		arms[6][2]; 

// ready-weapon widget
static st_number_t	w_ready;

 // in deathmatch only, summary of frags stats
static st_number_t	w_frags;

// health widget
static st_percent_t	w_health;

// arms background
static st_binicon_t	w_armsbg; 


// weapon ownership widgets
static st_multicon_t	w_arms[6];

// face status widget
static st_multicon_t	w_faces; 

// keycard widgets
static st_multicon_t	w_keyboxes[3];

// armor widget
static st_percent_t	w_armor;

// ammo widgets
static st_number_t	w_ammo[4];

// max ammo widgets
static st_number_t	w_maxammo[4]; 

// background flat for widescreen
static vbuffer_t	tileflat;

 // number of frags so far in deathmatch
static int	st_fragscount;

// used to use appopriately pained face
static int	st_oldhealth = -1;

// used for evil grin
static doombool	oldweaponsowned[NUMWEAPONS]; 

 // count until face changes
static int	st_facecount = 0;

// current face index, used by w_faces
static int	st_faceindex = 0;

// holds key-type for each key box on bar
static int	keyboxes[3]; 

// a random number per tick
static int	st_randomnumber;  

extern "C"
{
	int32_t		st_border_tile_style = STB_Flat5_4;
	const char*	st_border_tile_custom = NULL;

	cheatseq_t cheat_mus = CHEAT("idmus", 2);
	cheatseq_t cheat_god = CHEAT("iddqd", 0);
	cheatseq_t cheat_ammo = CHEAT("idkfa", 0);
	cheatseq_t cheat_ammonokey = CHEAT("idfa", 0);
	cheatseq_t cheat_noclip = CHEAT("idspispopd", 0);
	cheatseq_t cheat_commercial_noclip = CHEAT("idclip", 0);

	cheatseq_t	cheat_powerup[7] =
	{
		CHEAT("idbeholdv", 0),
		CHEAT("idbeholds", 0),
		CHEAT("idbeholdi", 0),
		CHEAT("idbeholdr", 0),
		CHEAT("idbeholda", 0),
		CHEAT("idbeholdl", 0),
		CHEAT("idbehold", 0),
	};

	cheatseq_t cheat_choppers = CHEAT("idchoppers", 0);
	cheatseq_t cheat_clev = CHEAT("idclev", 2);
	cheatseq_t cheat_mypos = CHEAT("idmypos", 0);
}

static statusbars_t* bars = nullptr;

static std::map< int32_t, int32_t > featureslookup =
{
	{ exe_doom_1_2,					features_vanilla				},
	{ exe_doom_1_666,				features_vanilla				},
	{ exe_doom_1_7,					features_vanilla				},
	{ exe_doom_1_8,					features_vanilla				},
	{ exe_doom_1_9,					features_vanilla				},
	{ exe_hacx,						features_vanilla				},
	{ exe_ultimate,					features_vanilla				},
	{ exe_final,					features_vanilla				},
	{ exe_final2,					features_vanilla				},
	{ exe_chex,						features_vanilla				},
	{ exe_limit_removing,			features_limit_removing			},
	{ exe_limit_removing_fixed,		features_limit_removing_fixed	},
	{ exe_boom_2_02,				features_boom_2_02				},
	{ exe_complevel9,				features_complevel9				},
	{ exe_mbf,						features_mbf					},
	{ exe_mbf_dehextra,				features_mbf_dehextra			},
	{ exe_mbf21,					features_mbf21					},
	{ exe_mbf21_extended,			features_mbf21_extended			},
	{ exe_rnr24,					features_rnr24					},
};

bool EvaluateConditions( const std::span< sbarcondition_t > conditions, player_t* player )
{
	bool result = true;
	int32_t currsessiontype = netgame ? M_MIN( deathmatch + 1, 2 ) : 0;
	bool compacthud = frame_width < frame_adjusted_width;

	for( sbarcondition_t& condition : conditions )
	{
		switch( condition.condition )
		{
		case sbc_weaponowned:
			result &= !!player->weaponowned[ condition.param ];
			break;

		case sbc_weaponselected:
			result &= player->readyweapon == condition.param;
			break;

		case sbc_weaponnotselected:
			result &= player->readyweapon != condition.param;
			break;

		case sbc_weaponhasammo:
			result &= weaponinfo[ condition.param ].ammo != am_noammo;
			break;

		case sbc_selectedweaponhasammo:
			result &= player->Weapon().ammo != am_noammo;
			break;

		case sbc_selectedweaponammotype:
			result &= player->Weapon().ammo == condition.param;
			break;

		case sbc_weaponslotowned:
			{
				bool owned = false;
				for( const weaponinfo_t* weapon : weaponinfo.All() )
				{
					if( weapon->slot == condition.param && player->weaponowned[ weapon->index ] )
					{
						owned = true;
						break;
					}
				}
				result &= owned;
			}
			break;

		case sbc_weaponslotnotowned:
			{
				bool notowned = true;
				for( const weaponinfo_t* weapon : weaponinfo.All() )
				{
					if( weapon->slot == condition.param && player->weaponowned[ weapon->index ] )
					{
						notowned = false;
						break;
					}
				}
				result &= notowned;
			}
			break;

		case sbc_weaponslotselected:
			result &= weaponinfo[ player->readyweapon ].slot == condition.param;
			break;

		case sbc_weaponslotnotselected:
			result &= weaponinfo[ player->readyweapon ].slot != condition.param;
			break;

		case sbc_itemowned:
			result &= !!P_EvaluateItemOwned( (itemtype_t)condition.param, player );
			break;

		case sbc_itemnotowned:
			result &= !P_EvaluateItemOwned( (itemtype_t)condition.param, player );
			break;

		case sbc_featurelevelgreaterequal:
			result &= featureslookup[ (int32_t)gameversion ] >= condition.param;
			break;

		case sbc_featurelevelless:
			result &= featureslookup[ (int32_t)gameversion ] < condition.param ;
			break;

		case sbc_sessiontypeeequal:
			result &= currsessiontype == condition.param;
			break;

		case sbc_sessiontypenotequal:
			result &= currsessiontype != condition.param;
			break;

		case sbc_modeeequal:
			result &= gamemode == ( condition.param + 1 );
			break;

		case sbc_modenotequal:
			result &= gamemode != ( condition.param + 1 );
			break;

		case sbc_hudmodeequal:
			result &= ( !!condition.param == compacthud );
			break;

		case sbc_none:
		default:
			result = false;
			break;
		}
	}

	return result;
}

int32_t ResolveNumber( sbarnumbertype_t number, int32_t param, player_t* player )
{
	int32_t result = 0;
	switch( number )
	{
	case sbn_health:
		result = player->health;
		break;

	case sbn_armor:
		result = player->armorpoints;
		break;

	case sbn_frags:
		for( int32_t p : iota( 0, MAXPLAYERS ) )
		{
			result += player->frags[ p ];
		}
		break;

	case sbn_ammo:
		result = player->ammo[ param ];
		break;

	case sbn_ammoselected:
		result = player->ammo[ player->Weapon().ammo ];
		break;

	case sbn_maxammo:
		result = player->maxammo[ param ];
		break;

	case sbn_weaponammo:
		result = player->ammo[ weaponinfo[ param ].ammo ];
		break;

	case sbn_weaponmaxammo:
		result = player->maxammo[ weaponinfo[ param ].ammo ];
		break;

	case sbn_none:
	default:
		break;
	}

	return result;
}

class SBCanvas
{
public:
	SBCanvas( statusbars_t* bars, sbarelement_t* e )
		: element( e ) { }

	virtual ~SBCanvas()
	{
		for( SBCanvas* child : children )
		{
			Z_Free( child );
		}
	}

	void ResetHierarchy()
	{
		ResetElement();
		for( SBCanvas* child : children )
		{
			child->ResetHierarchy();
		}
	}

	void TickHierarchy()
	{
		TickElement();
		for( SBCanvas* child : children )
		{
			child->TickHierarchy();
		}
	}

	void RefreshHierarchy( int32_t x, int32_t y )
	{
		int32_t thisx = x + element->xpos;
		int32_t thisy = y + element->ypos;

		if( EvaluateConditions( std::span( element->conditions, element->numconditions ), &players[ displayplayer ] ) )
		{
			RefreshElement( thisx, thisy );
			for( SBCanvas* child : children )
			{
				child->RefreshHierarchy( thisx, thisy );
			}
		}
	}

	virtual void ResetElement() { };
	virtual void TickElement() { };
	virtual void RefreshElement( int32_t thisx, int32_t thisy ) { };

protected:
	inline int32_t AdjustX( int32_t x, int32_t width )
	{
		if( element->alignment & sbe_h_middle )
		{
			x -= ( width >> 1 );
		}
		else if( element->alignment & sbe_h_right )
		{
			x -= width;
		}

		return x;
	}

	inline int32_t AdjustY( int32_t y, int32_t height )
	{
		if( element->alignment & sbe_v_middle )
		{
			y -= ( height >> 1 );
		}
		else if( element->alignment & sbe_v_bottom )
		{
			y -= height;
		}

		return y;
	}

	sbarelement_t*					element;

private:
	friend class SBContainer;
	std::vector< SBCanvas* >		children;
};

class SBGraphic : public SBCanvas
{
public:
	SBGraphic( statusbars_t* bars, sbarelement_t* e )
		: SBCanvas( bars, e )
		, patch( nullptr )
		, tranmap( nullptr )
		, translation( nullptr )
	{
		if( element->patch )
		{
			lumpindex_t patchindex = W_CheckNumForName( element->patch );
			if( patchindex >= 0 )
			{
				patch = (patch_t*)W_CacheLumpNum( patchindex, PU_STATIC );
			}
		}

		if( element->tranmap )
		{
			tranmap = (byte*)W_CacheLumpName( element->tranmap, PU_STATIC );
		}

		if( element->translation )
		{
			translation = R_GetTranslation( element->translation );
		}
	}

	virtual ~SBGraphic()
	{
		if( element->patch )
		{
			lumpindex_t patchindex = W_CheckNumForName( element->patch );
			if( patchindex )
			{
				W_ReleaseLumpNum( patchindex );
			}
		}
	}

	virtual void RefreshElement( int32_t thisx, int32_t thisy ) override
	{
		if( patch != nullptr )
		{
			int32_t currx = AdjustX( thisx, patch->width );
			int32_t curry = AdjustY( thisy, patch->height );

			V_DrawPatch( currx, curry, patch, tranmap, translation ? translation->table : nullptr );
		}
	}

protected:
	patch_t*			patch;
	byte*				tranmap;
	translation_t*		translation;
};

class SBAnimation : public SBGraphic
{
public:
	SBAnimation( statusbars_t* bars, sbarelement_t* e )
		: SBGraphic( bars, e )
		, currframe( 0 )
		, ticsleft( 0 )
	{
		patches.reserve( element->numframes );
		for( sbaranimframe_t& frame : std::span( element->frames, element->numframes ) )
		{
			lumpindex_t patchindex = W_CheckNumForName( frame.lump );
			patches.push_back( patchindex >= 0 ? (patch_t*)W_CacheLumpName( frame.lump, PU_STATIC ) : nullptr );
		}
	}

	virtual ~SBAnimation()
	{
		for( sbaranimframe_t& frame : std::span( element->frames, element->numframes ) )
		{
			lumpindex_t patchindex = W_CheckNumForName( frame.lump );
			if( patchindex )
			{
				W_ReleaseLumpNum( patchindex );
			}
		}
	}

	virtual void ResetElement() override
	{
		currframe = 0;
		ticsleft = element->frames[ currframe ].tics;
		patch = patches[ currframe ];
	}

	virtual void TickElement() override
	{
		if( --ticsleft <= 0 )
		{
			if( ++currframe >= element->numframes )
			{
				currframe = 0;
			}

			patch = patches[ currframe ];
			ticsleft = element->frames[ currframe ].tics;
		}
	}

	virtual void RefreshElement( int32_t thisx, int32_t thisy ) override
	{
		patch = patches[ currframe ];
		SBGraphic::RefreshElement( thisx, thisy );
	}

private:
	int32_t currframe;
	int32_t ticsleft;
	std::vector< patch_t* > patches;
};

class SBFace : public SBGraphic
{
public:
	SBFace( statusbars_t* bars, sbarelement_t* e )
		: SBGraphic( bars, e )
		, facepatches {}
		, priority( 0 )
		, lastattackdown( -1 )
		, oldhealth( -1 )
		, lasthealthcalc( 0 )
		, faceindex( 0 )
		, facecount( 0 )
		, oldweaponowned {}
	{
		oldweaponowned.ResetWeapon();

		int32_t currface = 0;
		char lumpnamebuffer[ 16 ] = {};

		auto LoadFace = [&currface, &lumpnamebuffer, this ]()
		{
			facenames[ currface ] = lumpnamebuffer;
			facepatches[ currface++ ] = (patch_t*)W_CacheLumpName( lumpnamebuffer, PU_STATIC );
		};

		for( int32_t painface : iota( 0, ST_NUMPAINFACES ) )
		{
			for( int32_t straightface : iota( 0, ST_NUMSTRAIGHTFACES ) )
			{
				DEH_snprintf( lumpnamebuffer, 9, "STFST%d%d", painface, straightface );
				LoadFace();
			}

			DEH_snprintf( lumpnamebuffer, 9, "STFTR%d0", painface );	// turn right
			LoadFace();

			DEH_snprintf( lumpnamebuffer, 9, "STFTL%d0", painface );	// turn left
			LoadFace();

			DEH_snprintf( lumpnamebuffer, 9, "STFOUCH%d", painface );	// ouch!
			LoadFace();

			DEH_snprintf( lumpnamebuffer, 9, "STFEVL%d", painface );	// evil grin ;)
			LoadFace();

			DEH_snprintf( lumpnamebuffer, 9, "STFKILL%d", painface );	// pissed off
			LoadFace();
		}

		DEH_snprintf( lumpnamebuffer, 9, "STFGOD0" );
		LoadFace();

		DEH_snprintf( lumpnamebuffer, 9, "STFDEAD0" );
		LoadFace();
	}

	virtual ~SBFace()
	{
		for( std::string& lump : std::span( facenames ) )
		{
			W_ReleaseLumpName( lump.c_str() );
		}
	}

	virtual void ResetElement() override
	{
		player_t* player = &players[ displayplayer ];

		oldweaponowned.ResetWeapon();
		for( weaponinfo_t* weapon : weaponinfo.All() )
		{
			oldweaponowned[ weapon->index ] = player->weaponowned[ weapon->index ];
		}

		oldhealth = player->health;
		faceindex = 0;
		facecount = ST_STRAIGHTFACECOUNT;
		priority = 0;
	}

	virtual void TickElement() override
	{
		player_t* player = &players[ displayplayer ];

		if (priority < 10)
		{
			// dead
			if (!player->health)
			{
				priority = 9;
				faceindex = ST_DEADFACE;
				facecount = 1;
			}
		}

		if (priority < 9)
		{
			if (player->bonuscount)
			{
				// picking up bonus
				bool doevilgrin = false;

				for( weaponinfo_t* weapon : weaponinfo.All() )
				{
					if( oldweaponowned[ weapon->index ] != player->weaponowned[ weapon->index ] )
					{
						doevilgrin = true;
						oldweaponowned[ weapon->index ] = player->weaponowned[ weapon->index ];
					}
				}

				if( doevilgrin )
				{
					// evil grin if just picked up weapon
					priority = 8;
					facecount = ST_EVILGRINCOUNT;
					faceindex = CalcPainOffset() + ST_EVILGRINOFFSET;
				}
			}
		}
  
		if (priority < 8)
		{
			if (player->damagecount
				&& player->attacker
				&& player->attacker != player->mo)
			{
				// being attacked
				priority = 7;

				angle_t diffangle = 0;
				bool right = false;

				if( player->health - oldhealth > ST_MUCHPAIN )
				{
					facecount = ST_TURNCOUNT;
					faceindex = CalcPainOffset() + ST_OUCHOFFSET;
				}
				else
				{
					angle_t badguyangle = BSP_PointToAngle(player->mo->x,
															player->mo->y,
															player->attacker->x,
															player->attacker->y);
		
					if (badguyangle > player->mo->angle)
					{
						// whether right or left
						diffangle = badguyangle - player->mo->angle;
						right = diffangle > ANG180; 
					}
					else
					{
						// whether left or right
						diffangle = player->mo->angle - badguyangle;
						right = diffangle <= ANG180; 
					} // confusing, aint it?

					facecount = ST_TURNCOUNT;
					faceindex = CalcPainOffset();
		
					if( diffangle < ANG45 )
					{
						// head-on    
						faceindex += ST_RAMPAGEOFFSET;
					}
					else if( right )
					{
						// turn face right
						faceindex += ST_TURNOFFSET;
					}
					else
					{
						// turn face left
						faceindex += ST_TURNOFFSET+1;
					}
				}
			}
		}
  
		if( priority < 7 )
		{
			// getting hurt because of your own damn stupidity
			if (player->damagecount)
			{
				if( player->health - oldhealth > ST_MUCHPAIN )
				{
					priority = 7;
					facecount = ST_TURNCOUNT;
					faceindex = CalcPainOffset() + ST_OUCHOFFSET;
				}
				else
				{
					priority = 6;
					facecount = ST_TURNCOUNT;
					faceindex = CalcPainOffset() + ST_RAMPAGEOFFSET;
				}
			}
		}
  
		if( priority < 6 )
		{
			// rapid firing
			if( player->attackdown )
			{
				if( lastattackdown == -1 )
				{
					lastattackdown = ST_RAMPAGEDELAY;
				}
				else if( !--lastattackdown )
				{
					priority = 5;
					faceindex = CalcPainOffset() + ST_RAMPAGEOFFSET;
					facecount = 1;
					lastattackdown = 1;
				}
			}
			else
			{
				lastattackdown = -1;
			}

		}
  
		if( priority < 5 )
		{
			// invulnerability
			if( ( player->cheats & CF_GODMODE )
				|| player->powers[ pw_invulnerability ] )
			{
				priority = 4;
				faceindex = ST_GODFACE;
				facecount = 1;
			}

		}

		// look left or look right if the facecount has timed out
		if( !facecount )
		{
			faceindex = CalcPainOffset() + ( M_Random() % 3 );
			facecount = ST_STRAIGHTFACECOUNT;
			priority = 0;
		}

		--facecount;
	}

	virtual void RefreshElement( int32_t thisx, int32_t thisy ) override
	{
		patch = facepatches[ faceindex ];
		SBGraphic::RefreshElement( thisx, thisy );
	}

private:
	int CalcPainOffset(void)
	{
		int32_t health = plyr->health > 100 ? 100 : plyr->health;

		if( health != oldhealth )
		{
			lasthealthcalc = ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
			oldhealth = health;
		}

		return lasthealthcalc;
	}

	std::string		facenames[ ST_NUMFACES ];
	patch_t*		facepatches[ ST_NUMFACES ];

	int32_t			priority;
	int32_t			lastattackdown;
	int32_t			oldhealth;
	int32_t			lasthealthcalc;
	int32_t			faceindex;
	int32_t			facecount;
	owneddata_t		oldweaponowned;

};

class SBFaceBackground : public SBGraphic
{
public:
	SBFaceBackground( statusbars_t* bars, sbarelement_t* e )
		: SBGraphic( bars, e )
	{
	}

	virtual ~SBFaceBackground()
	{
	}

	virtual void RefreshElement( int32_t thisx, int32_t thisy ) override
	{
		player_t* player = &players[ displayplayer ];

		patch = player->mo->translation->sbarback;
		translation = player->mo->translation->sbartranslate ? player->mo->translation : nullptr;
		SBGraphic::RefreshElement( thisx, thisy );
	}

};

class SBNumber : public SBGraphic
{
public:
	SBNumber( statusbars_t* bars, sbarelement_t* e )
		: SBGraphic( bars, e )
		, font( nullptr )
	{
		std::string numfont = ToUpper( e->numfont );
		font = std::find_if( bars->fonts, bars->fonts + bars->numfonts, [&numfont]( sbarnumfont_t& font )
							{
								return ToUpper( font.name ) == ToUpper( numfont );
							} );
	}

	virtual ~SBNumber() { }

	virtual void RefreshElement( int32_t thisx, int32_t thisy ) override
	{
		player_t* player = &players[ displayplayer ];
		int32_t number = ResolveNumber( element->numtype, element->numparam, player );
		int32_t power = ( number < 0 ? element->numlength - 1 : element->numlength );
		int32_t max = (int32_t)pow( 10.0, power ) - 1;
		int32_t numglyphs = 0;
		int32_t numnumbers = 0;

		if( number < 0 && font->minus != nullptr )
		{
			number = M_MAX( -max, number );
			numnumbers = (int32_t)log10( -number ) + 1;
			numglyphs = numnumbers + 1;
		}
		else
		{
			number = M_CLAMP( number, 0, max );
			numnumbers = numglyphs = number != 0 ? ( (int32_t)log10( number ) + 1 ) : 1;
		}

		if( element->type == sbe_percentage && font->percent != nullptr )
		{
			++numglyphs;
		}

		int32_t totalwidth = font->monowidth * numglyphs;
		if( font->type == sbf_proportional )
		{
			totalwidth = 0;
			if( number < 0 && font->minus != nullptr ) totalwidth += font->minus->width;
			int32_t tempnum = number;
			while( tempnum > 0 )
			{
				int32_t workingnum = tempnum % 10;
				totalwidth = font->numbers[ workingnum ]->width;
				tempnum /= 10;
			}
			if( element->type == sbe_percentage && font->percent != nullptr )
			{
				totalwidth += font->percent->width;
			}
		}

		int32_t xoffset = 0;
		if( element->alignment & sbe_h_middle )
		{
			xoffset -= ( totalwidth >> 1 );
		}
		else if( element->alignment & sbe_h_right )
		{
			xoffset -= totalwidth;
		}

		auto RenderGlyph = [ this, &xoffset, &thisx, &thisy ]( patch_t* glyph )
		{
			int32_t width = font->type == sbf_proportional ? glyph->width : font->monowidth;
			int32_t widthdiff = font->type != sbf_proportional ? ( glyph->width - width ) : 0;

			if( element->alignment & sbe_h_middle )
			{
				xoffset += ( ( width + widthdiff ) >> 1 );
			}
			else if( element->alignment & sbe_h_right )
			{
				xoffset += ( width + widthdiff );
			}

			patch = glyph;
			SBGraphic::RefreshElement( thisx + xoffset, thisy );

			if( element->alignment & sbe_h_middle )
			{
				xoffset += ( width - ( ( width - widthdiff ) >> 1 ) );
			}
			else if( element->alignment & sbe_h_right )
			{
				xoffset += -widthdiff;
			}
			else
			{
				xoffset += width;
			}
		};

		if( number < 0 && font->minus != nullptr )
		{
			RenderGlyph( font->minus );
			number = -number;
		}

		int32_t glyphindex = numnumbers;
		while( glyphindex > 0 )
		{
			int32_t glyphbase = (int32_t)pow( 10.0, --glyphindex );
			int32_t workingnum = number / glyphbase;
			RenderGlyph( font->numbers[ workingnum ] );
			number -= ( workingnum * glyphbase );
		}

		if( element->type == sbe_percentage && font->percent != nullptr )
		{
			RenderGlyph( font->percent );
		}
	}

private:
	sbarnumfont_t* font;
};

using SBPercentage = SBNumber;

using SBAlloc = SBCanvas*(*)( statusbars_t* bars, sbarelement_t* e, int32_t zone );

template< typename _ty >
struct SBNew
{
	static SBCanvas* Alloc( statusbars_t* bars, sbarelement_t* e, int32_t zone )
	{
		return Z_MallocAsArgs( _ty, zone, nullptr, bars, e );
	}
};

constexpr SBAlloc SBAllocFor[ sbe_max ] =
{
	&SBNew< SBCanvas >::Alloc, // sbe_canvas
	&SBNew< SBGraphic >::Alloc, // sbe_graphic
	&SBNew< SBAnimation >::Alloc, // sbe_animation
	&SBNew< SBFace >::Alloc, // sbe_face
	&SBNew< SBFaceBackground >::Alloc, // sbe_facebackground
	&SBNew< SBNumber >::Alloc, // sbe_number
	&SBNew< SBPercentage >::Alloc, // sbe_percentage
};

class SBContainer
{
public:
	SBContainer( statusbars_t* b, int32_t i )
		: offscreenbuffer {}
		, bars( b )
		, thisbar( &b->statusbars[ i ] )
		, fillflatbuffer {}
		, index( i )
		, cachedwidth( 0 )
		, cachedheight( 0 )
		, framewidth( 0 )
		, frameadjustedwidth( 0 )
		, frameheight( 0 )
	{
		const char* fillflatname = thisbar->fillflat;
		if( fillflatname == nullptr )
		{
			switch( st_border_tile_style )
			{
			case STB_WADDefined:
				fillflatname = gamemode == commercial ? DEH_String("GRNROCK") : DEH_String("FLOOR7_2");
				break;

			case STB_Flat5_4:
				fillflatname = "FLAT5_4";
				break;

			case STB_Custom:
				fillflatname = st_border_tile_custom;
				break;
			}
		}

		V_TransposeFlat( fillflatname, &fillflatbuffer, PU_STATIC );

		RefreshOffscreenBuffer();

		children.reserve( thisbar->numchildren );
		for( sbarelement_t& elem : std::span( thisbar->children, thisbar->numchildren ) )
		{
			SBCanvas* thiselem = SBAllocFor[ elem.type ]( bars, &elem, PU_STATIC );
			AllocChildren( thiselem );
			children.push_back( thiselem );
		}
	}

	~SBContainer()
	{
		for( SBCanvas* child : children )
		{
			Z_Free( child );
		}
	}

	void ResetHierarchy()
	{
		for( SBCanvas* child : children )
		{
			child->ResetHierarchy();
		}
	}

	void TickHierarchy()
	{
		for( SBCanvas* child : children )
		{
			child->TickHierarchy();
		}
	}

	void RefreshHierarchy()
	{
		int32_t oldframewidth = frame_width;
		int32_t oldframeadjustedwidth = frame_adjusted_width;
		int32_t oldframeheight = frame_height;
		if( !thisbar->fullscreen )
		{
			frame_width = framewidth;
			frame_adjusted_width = frameadjustedwidth;
			frame_height = 200;
			V_UseBuffer( &offscreenbuffer );

			int32_t xpos = -( ( offscreenbuffer.width - 320 ) >> 1 );
			int32_t ypos = 0;
			V_TileBuffer( &fillflatbuffer, xpos, ypos, offscreenbuffer.width, offscreenbuffer.height );
		}

		for( SBCanvas* child : children )
		{
			child->RefreshHierarchy( 0, 0 );
		}

		if( !thisbar->fullscreen )
		{
			frame_width = oldframewidth;
			frame_adjusted_width = oldframeadjustedwidth;
			frame_height = oldframeheight;
			V_RestoreBuffer();

			int32_t xpos = -( ( offscreenbuffer.width - 320 ) >> 1 );
			int32_t ypos = 200 - thisbar->height;
			V_TileBuffer( &offscreenbuffer, xpos, ypos, offscreenbuffer.width, offscreenbuffer.height );
		}
	}

private:
	void AllocChildren( SBCanvas* canvas )
	{
		for( sbarelement_t& elem : std::span( canvas->element->children, canvas->element->numchildren ) )
		{
			SBCanvas* thiselem = SBAllocFor[ elem.type ]( bars, &elem, PU_STATIC );
			AllocChildren( thiselem );
			canvas->children.push_back( thiselem );
		}
	}

	void RefreshOffscreenBuffer()
	{
		if( thisbar->fullscreen
			|| ( cachedwidth == render_width && cachedheight == render_height ) )
		{
			return;
		}

		if( offscreenbuffer.data != nullptr )
		{
			Z_Free( offscreenbuffer.data );
		}

		cachedwidth = render_width;
		cachedheight = render_height;

		double_t aspectratio = ( (double_t)render_width / ( (double_t)render_height * ( render_post_scaling ? 1.2 : 1.0 ) ) );
		framewidth = (int32_t)( 240.0 * aspectratio );
		frameadjustedwidth = 320;
		frameheight = thisbar->height;

		offscreenbuffer.data = (pixel_t*)Z_MallocZero( framewidth * frameheight * sizeof( pixel_t ), PU_STATIC, &offscreenbuffer.data );
		offscreenbuffer.width = framewidth;
		offscreenbuffer.height = frameheight;
		offscreenbuffer.pitch = frameheight;
		offscreenbuffer.mode = VB_Transposed;
		offscreenbuffer.pixel_size_bytes = sizeof( pixel_t );
		offscreenbuffer.verticalscale = 1.2f;
		offscreenbuffer.magic_value = vbuffer_magic;
	}

	std::vector< SBCanvas* >	children;
	vbuffer_t					offscreenbuffer;
	statusbars_t*				bars;
	statusbarbase_t*			thisbar;
	vbuffer_t					fillflatbuffer;
	int32_t						index;
	int32_t						cachedwidth;
	int32_t						cachedheight;
	int32_t						framewidth;
	int32_t						frameadjustedwidth;
	int32_t						frameheight;
};

std::vector< SBContainer* > statusbars;
int32_t currstatusbar;

void ST_InitSBars()
{
	statusbars.reserve( bars->numstatusbars );
	for( int32_t index : iota( 0, (int32_t)bars->numstatusbars ) )
	{
		SBContainer* container = Z_MallocAsArgs( SBContainer, PU_STATIC, nullptr, bars, index );
		statusbars.push_back( container );
	}
}

//
// STATUS BAR CODE
//
DOOM_C_API void ST_Stop(void);

DOOM_C_API void ST_refreshBackground(void)
{
	M_PROFILE_PUSH( __FUNCTION__, __FILE__, __LINE__ );

	if (st_statusbaron)
	{
		V_FillBorder( &tileflat, ST_Y, V_VIRTUALHEIGHT );

		// WIDESCREEN HACK
		int32_t xpos = -( ( sbar->width - V_VIRTUALWIDTH ) / 2 );
		V_DrawPatch(ST_X + xpos, ST_Y, sbar);

		// draw right side of bar if needed (Doom 1.0)
		if (sbarr)
			V_DrawPatch(ST_ARMSBGX, ST_Y, sbarr);
		
		if (netgame)
			V_DrawPatch(ST_FX, ST_Y, faceback);
	}

	M_PROFILE_POP( __FUNCTION__ );
}


// Respond to keyboard input events,
//  intercept cheats.
DOOM_C_API doombool ST_Responder (event_t* ev)
{
  int		i;
    
  // Filter automap on/off.
  if (ev->type == ev_keyup
      && ((ev->data1 & 0xffff0000) == AM_MSGHEADER))
  {
    switch(ev->data1)
    {
      case AM_MSGENTERED:
	st_gamestate = AutomapState;
	st_firsttime = true;
	break;
	
      case AM_MSGEXITED:
	//	fprintf(stderr, "AM exited\n");
	st_gamestate = FirstPersonState;
	break;
    }
  }

  // if a user keypress...
  else if (ev->type == ev_keydown)
  {
    if (!netgame && gameskill != sk_nightmare)
    {
      // 'dqd' cheat for toggleable god mode
      if (cht_CheckCheat(&cheat_god, ev->data2))
      {
	plyr->cheats ^= CF_GODMODE;
	if (plyr->cheats & CF_GODMODE)
	{
	  if (plyr->mo)
	    plyr->mo->health = 100;
	  
	  plyr->health = deh_god_mode_health;
	  plyr->message = DEH_String(STSTR_DQDON);
	}
	else 
	  plyr->message = DEH_String(STSTR_DQDOFF);
      }
      // 'fa' cheat for killer fucking arsenal
      else if (cht_CheckCheat(&cheat_ammonokey, ev->data2))
      {
	plyr->armorpoints = deh_idfa_armor;
	plyr->armortype = deh_idfa_armor_class;
	
	for (i=0;i<NUMWEAPONS;i++)
	  plyr->weaponowned[i] = true;
	
	for (i=0;i<NUMAMMO;i++)
	  plyr->ammo[i] = plyr->maxammo[i];
	
	plyr->message = DEH_String(STSTR_FAADDED);
      }
      // 'kfa' cheat for key full ammo
      else if (cht_CheckCheat(&cheat_ammo, ev->data2))
      {
	plyr->armorpoints = deh_idkfa_armor;
	plyr->armortype = deh_idkfa_armor_class;
	
	for (i=0;i<NUMWEAPONS;i++)
	  plyr->weaponowned[i] = true;
	
	for (i=0;i<NUMAMMO;i++)
	  plyr->ammo[i] = plyr->maxammo[i];
	
	for (i=0;i<NUMCARDS;i++)
	  plyr->cards[i] = true;
	
	plyr->message = DEH_String(STSTR_KFAADDED);
      }
      // 'mus' cheat for changing music
      else if (cht_CheckCheat(&cheat_mus, ev->data2))
      {
	
	char	buf[3];
	int		musnum;
	
	plyr->message = DEH_String(STSTR_MUS);
	cht_GetParam(&cheat_mus, buf);

        // Note: The original v1.9 had a bug that tried to play back
        // the Doom II music regardless of gamemode.  This was fixed
        // in the Ultimate Doom executable so that it would work for
        // the Doom 1 music as well.

	if (gamemode == commercial || gameversion < exe_ultimate)
	{
	  musnum = mus_runnin + (buf[0]-'0')*10 + buf[1]-'0' - 1;
	  
	  if (((buf[0]-'0')*10 + buf[1]-'0') > 35
       && gameversion >= exe_doom_1_8)
	    plyr->message = DEH_String(STSTR_NOMUS);
	  else
	    S_ChangeMusic(musnum, 1);
	}
	else
	{
	  musnum = mus_e1m1 + (buf[0]-'1')*9 + (buf[1]-'1');
	  
	  if (((buf[0]-'1')*9 + buf[1]-'1') > 31)
	    plyr->message = DEH_String(STSTR_NOMUS);
	  else
	    S_ChangeMusic(musnum, 1);
	}
      }
      else if ( ( ( comp.noclip_cheats_work_everywhere || logical_gamemission == doom )
                 && cht_CheckCheat(&cheat_noclip, ev->data2))
             || ( ( comp.noclip_cheats_work_everywhere || logical_gamemission != doom )
                 && cht_CheckCheat(&cheat_commercial_noclip,ev->data2)))
      {	
        // Noclip cheat.
        // For Doom 1, use the idspipsopd cheat; for all others, use
        // idclip

	plyr->cheats ^= CF_NOCLIP;
	
	if (plyr->cheats & CF_NOCLIP)
	  plyr->message = DEH_String(STSTR_NCON);
	else
	  plyr->message = DEH_String(STSTR_NCOFF);
      }
      // 'behold?' power-up cheats
      for (i=0;i<6;i++)
      {
	if (cht_CheckCheat(&cheat_powerup[i], ev->data2))
	{
	  if (!plyr->powers[i])
	    P_GivePower( plyr, i);
	  else if (i!=pw_strength)
	    plyr->powers[i] = 1;
	  else
	    plyr->powers[i] = 0;
	  
	  plyr->message = DEH_String(STSTR_BEHOLDX);
	}
      }
      
      // 'behold' power-up menu
      if (cht_CheckCheat(&cheat_powerup[6], ev->data2))
      {
	plyr->message = DEH_String(STSTR_BEHOLD);
      }
      // 'choppers' invulnerability & chainsaw
      else if (cht_CheckCheat(&cheat_choppers, ev->data2))
      {
	plyr->weaponowned[wp_chainsaw] = true;
	plyr->powers[pw_invulnerability] = true;
	plyr->message = DEH_String(STSTR_CHOPPERS);
      }
      // 'mypos' for player position
      else if (cht_CheckCheat(&cheat_mypos, ev->data2))
      {
        static char buf[ST_MSGWIDTH];
        M_snprintf(buf, sizeof(buf), "ang=0x%x;x,y=(0x%x,0x%x)",
                   players[consoleplayer].mo->angle,
                   players[consoleplayer].mo->x,
                   players[consoleplayer].mo->y);
        plyr->message = buf;
      }
    }
    
    // 'clev' change-level cheat
    if (!netgame && cht_CheckCheat(&cheat_clev, ev->data2))
    {
		char		buf[3];
		int		epsd;
		int		map;

		cht_GetParam(&cheat_clev, buf);

		if (gamemode == commercial)
		{
			epsd = 1;
			map = (buf[0] - '0')*10 + buf[1] - '0';
		}
		else
		{
			epsd = buf[0] - '0';
			map = buf[1] - '0';
		}

		episodeinfo_t* epinfo = D_GameflowGetEpisode( epsd );
		mapinfo_t* mapinfo = D_GameflowGetMap( epinfo, map );

		if( !mapinfo )
		{
			return false;
		}

		// So be it.
		plyr->message = DEH_String(STSTR_CLEV);
		G_DeferedInitNew(gameskill, mapinfo, GF_None);
    }
  }
  return false;
}



DOOM_C_API int ST_calcPainOffset(void)
{
    int		health;
    static int	lastcalc;
    static int	oldhealth = -1;
    
    health = plyr->health > 100 ? 100 : plyr->health;

    if (health != oldhealth)
    {
	lastcalc = ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
	oldhealth = health;
    }
    return lastcalc;
}


//
// This is a not-very-pretty routine which handles
//  the face states and their timing.
// the precedence of expressions is:
//  dead > evil grin > turned head > straight ahead
//
DOOM_C_API void ST_updateFaceWidget(void)
{
    int		i;
    angle_t	badguyangle;
    angle_t	diffang;
    static int	lastattackdown = -1;
    static int	priority = 0;
    doombool	doevilgrin;

    if (priority < 10)
    {
	// dead
	if (!plyr->health)
	{
	    priority = 9;
	    st_faceindex = ST_DEADFACE;
	    st_facecount = 1;
	}
    }

    if (priority < 9)
    {
	if (plyr->bonuscount)
	{
	    // picking up bonus
	    doevilgrin = false;

	    for (i=0;i<NUMWEAPONS;i++)
	    {
		if (oldweaponsowned[i] != plyr->weaponowned[i])
		{
		    doevilgrin = true;
		    oldweaponsowned[i] = plyr->weaponowned[i];
		}
	    }
	    if (doevilgrin) 
	    {
		// evil grin if just picked up weapon
		priority = 8;
		st_facecount = ST_EVILGRINCOUNT;
		st_faceindex = ST_calcPainOffset() + ST_EVILGRINOFFSET;
	    }
	}

    }
  
    if (priority < 8)
    {
	if (plyr->damagecount
	    && plyr->attacker
	    && plyr->attacker != plyr->mo)
	{
	    // being attacked
	    priority = 7;
	    
	    if (plyr->health - st_oldhealth > ST_MUCHPAIN)
	    {
		st_facecount = ST_TURNCOUNT;
		st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
	    }
	    else
	    {
		badguyangle = BSP_PointToAngle(plyr->mo->x,
					      plyr->mo->y,
					      plyr->attacker->x,
					      plyr->attacker->y);
		
		if (badguyangle > plyr->mo->angle)
		{
		    // whether right or left
		    diffang = badguyangle - plyr->mo->angle;
		    i = diffang > ANG180; 
		}
		else
		{
		    // whether left or right
		    diffang = plyr->mo->angle - badguyangle;
		    i = diffang <= ANG180; 
		} // confusing, aint it?

		
		st_facecount = ST_TURNCOUNT;
		st_faceindex = ST_calcPainOffset();
		
		if (diffang < ANG45)
		{
		    // head-on    
		    st_faceindex += ST_RAMPAGEOFFSET;
		}
		else if (i)
		{
		    // turn face right
		    st_faceindex += ST_TURNOFFSET;
		}
		else
		{
		    // turn face left
		    st_faceindex += ST_TURNOFFSET+1;
		}
	    }
	}
    }
  
    if (priority < 7)
    {
	// getting hurt because of your own damn stupidity
	if (plyr->damagecount)
	{
	    if (plyr->health - st_oldhealth > ST_MUCHPAIN)
	    {
		priority = 7;
		st_facecount = ST_TURNCOUNT;
		st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
	    }
	    else
	    {
		priority = 6;
		st_facecount = ST_TURNCOUNT;
		st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
	    }

	}

    }
  
    if (priority < 6)
    {
	// rapid firing
	if (plyr->attackdown)
	{
	    if (lastattackdown==-1)
		lastattackdown = ST_RAMPAGEDELAY;
	    else if (!--lastattackdown)
	    {
		priority = 5;
		st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
		st_facecount = 1;
		lastattackdown = 1;
	    }
	}
	else
	    lastattackdown = -1;

    }
  
    if (priority < 5)
    {
	// invulnerability
	if ((plyr->cheats & CF_GODMODE)
	    || plyr->powers[pw_invulnerability])
	{
	    priority = 4;

	    st_faceindex = ST_GODFACE;
	    st_facecount = 1;

	}

    }

    // look left or look right if the facecount has timed out
    if (!st_facecount)
    {
	st_faceindex = ST_calcPainOffset() + (st_randomnumber % 3);
	st_facecount = ST_STRAIGHTFACECOUNT;
	priority = 0;
    }

    st_facecount--;

}

DOOM_C_API void ST_updateWidgets(void)
{
    static int	largeammo = 1994; // means "n/a"
    int		i;

    // must redirect the pointer if the ready weapon has changed.
    //  if (w_ready.data != plyr->readyweapon)
    //  {
    if (weaponinfo[plyr->readyweapon].ammo == am_noammo)
	w_ready.num = &largeammo;
    else
	w_ready.num = &plyr->ammo[weaponinfo[plyr->readyweapon].ammo];
    //{
    // static int tic=0;
    // static int dir=-1;
    // if (!(tic&15))
    //   plyr->ammo[weaponinfo[plyr->readyweapon].ammo]+=dir;
    // if (plyr->ammo[weaponinfo[plyr->readyweapon].ammo] == -100)
    //   dir = 1;
    // tic++;
    // }
    w_ready.data = plyr->readyweapon;

    // if (*w_ready.on)
    //  STlib_updateNum(&w_ready, true);
    // refresh weapon change
    //  }

    // update keycard multiple widgets
    for (i=0;i<3;i++)
    {
		keyboxes[ i ] = plyr->cards[ i ] ? i : -1;

		if ( plyr->cards[ 3 + i ] )
		{
			keyboxes[ i ] = ( keyboxes[ i ] == -1 || keys[ 6 + i ] == NULL ? 3 : 6 ) + i;
		}
    }

    // refresh everything if this is him coming back to life
    ST_updateFaceWidget();

    // used by the w_armsbg widget
    st_notdeathmatch = !deathmatch;
    
    // used by w_arms[] widgets
    st_armson = st_statusbaron && !deathmatch; 

    // used by w_frags widget
    st_fragson = deathmatch && st_statusbaron; 
    st_fragscount = 0;

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (i != consoleplayer)
	    st_fragscount += plyr->frags[i];
	else
	    st_fragscount -= plyr->frags[i];
    }

    // get rid of chat window if up because of message
    if (!--st_msgcounter)
	st_chat = st_oldchat;

}

DOOM_C_API void ST_Ticker (void)
{
	if( bars )
	{
		for( SBContainer* sbar : statusbars )
		{
			sbar->TickHierarchy();
		}
	}
	else
	{
		st_clock++;
		st_randomnumber = M_Random();
		ST_updateWidgets();
		st_oldhealth = plyr->health;
	}
}

static int st_palette = 0;

DOOM_C_API void ST_doPaletteStuff(void)
{

    int		palette;
    byte*	pal;
    int		cnt;
    int		bzc;

    cnt = plyr->damagecount;

    if (plyr->powers[pw_strength])
    {
	// slowly fade the berzerk out
  	bzc = 12 - (plyr->powers[pw_strength]>>6);

	if (bzc > cnt)
	    cnt = bzc;
    }
	
    if (cnt)
    {
	palette = (cnt+7)>>3;
	
	if (palette >= NUMREDPALS)
	    palette = NUMREDPALS-1;

	palette += STARTREDPALS;
    }

    else if (plyr->bonuscount)
    {
	palette = (plyr->bonuscount+7)>>3;

	if (palette >= NUMBONUSPALS)
	    palette = NUMBONUSPALS-1;

	palette += STARTBONUSPALS;
    }

    else if ( plyr->powers[pw_ironfeet] > 4*32
	      || plyr->powers[pw_ironfeet]&8)
	palette = RADIATIONPAL;
    else
	palette = 0;

    // In Chex Quest, the player never sees red.  Instead, the
    // radiation suit palette is used to tint the screen green,
    // as though the player is being covered in goo by an
    // attacking flemoid.

    if (gameversion == exe_chex
     && palette >= STARTREDPALS && palette < STARTREDPALS + NUMREDPALS)
    {
        palette = RADIATIONPAL;
    }

    if (palette != st_palette)
    {
	st_palette = palette;
	pal = (byte *) W_CacheLumpNum (lu_palette, PU_CACHE)+palette*768;
	I_SetPalette (pal);
    }

}

DOOM_C_API void ST_drawWidgets(doombool refresh)
{
	M_PROFILE_PUSH( __FUNCTION__, __FILE__, __LINE__ );

    int		i;

    // used by w_arms[] widgets
    st_armson = st_statusbaron && !deathmatch;

    // used by w_frags widget
    st_fragson = deathmatch && st_statusbaron; 

    STlib_updateNum(&w_ready, refresh);

    for (i=0;i<4;i++)
    {
	STlib_updateNum(&w_ammo[i], refresh);
	STlib_updateNum(&w_maxammo[i], refresh);
    }

    STlib_updatePercent(&w_health, refresh);
    STlib_updatePercent(&w_armor, refresh);

    STlib_updateBinIcon(&w_armsbg, refresh);

    for (i=0;i<6;i++)
	STlib_updateMultIcon(&w_arms[i], refresh);

    STlib_updateMultIcon(&w_faces, refresh);

    for (i=0;i<3;i++)
	STlib_updateMultIcon(&w_keyboxes[i], refresh);

    STlib_updateNum(&w_frags, refresh);

	M_PROFILE_POP( __FUNCTION__ );

}

DOOM_C_API extern int screenblocks;

DOOM_C_API void ST_doRefresh(void)
{
    st_firsttime = false;

	if( bars != nullptr )
	{
		int32_t barindex = automapactive ? 0 : M_MAX( 0, screenblocks - 10 );
		statusbars[ barindex ]->RefreshHierarchy();
	}
	else
	{
		// draw status bar background to off-screen buff
		ST_refreshBackground();

		// and refresh all widgets
		ST_drawWidgets(true);
	}
}

DOOM_C_API void ST_diffDraw(void)
{
    // update all widgets
    ST_drawWidgets(false);
}

DOOM_C_API void ST_Drawer (doombool fullscreen, doombool refresh)
{
	M_PROFILE_PUSH( __FUNCTION__, __FILE__, __LINE__ );

	if( bars )
	{
		st_statusbaron = true;
	}
	else
	{
		st_statusbaron = (!fullscreen) || automapactive;
	}
    st_firsttime = st_firsttime || refresh;

    // Do red-/gold-shifts from damage/items
    ST_doPaletteStuff();

	ST_doRefresh();
    //// If just after ST_Start(), refresh all
    //if (st_firsttime) ST_doRefresh();
    //// Otherwise, update as little as possible
    //else ST_diffDraw();

	M_PROFILE_POP( __FUNCTION__ );
}

typedef void (*load_callback_t)(const char *lumpname, patch_t **variable);

// Iterates through all graphics to be loaded or unloaded, along with
// the variable they use, invoking the specified callback function.

static void ST_loadUnloadGraphics(load_callback_t callback)
{

    int		i;
    int		j;
    int		facenum;
    
    char	namebuf[9];

    // Load the numbers, tall and short
    for (i=0;i<10;i++)
    {
	DEH_snprintf(namebuf, 9, "STTNUM%d", i);
        callback(namebuf, &tallnum[i]);

	DEH_snprintf(namebuf, 9, "STYSNUM%d", i);
        callback(namebuf, &shortnum[i]);
    }

    // Load percent key.
    //Note: why not load STMINUS here, too?

    callback(DEH_String("STTPRCNT"), &tallpercent);

    // key cards
    for (i=0;i<TOTALCARDS;i++)
    {
		if( !sim.hud_combined_keys && i >= NUMCARDS )
		{
			break;
		}
		DEH_snprintf(namebuf, 9, "STKEYS%d", i);
		if( i >= NUMCARDS && W_CheckNumForName( namebuf ) < 0 )
		{
			keys[ i ] = NULL;
		}
		else
		{
			callback(namebuf, &keys[i]);
		}
    }

    // arms background
    callback(DEH_String("STARMS"), &armsbg);

    // arms ownership widgets
    for (i=0; i<6; i++)
    {
	DEH_snprintf(namebuf, 9, "STGNUM%d", i+2);

	// gray #
        callback(namebuf, &arms[i][0]);

	// yellow #
	arms[i][1] = shortnum[i+2]; 
    }

    // face backgrounds for different color players
    DEH_snprintf(namebuf, 9, "STFB%d", consoleplayer);
    callback(namebuf, &faceback);

    // status bar background bits
    if (W_CheckNumForName("STBAR") >= 0)
    {
        callback(DEH_String("STBAR"), &sbar);
        sbarr = NULL;
    }
    else
    {
        callback(DEH_String("STMBARL"), &sbar);
        callback(DEH_String("STMBARR"), &sbarr);
    }

    // face states
    facenum = 0;
    for (i=0; i<ST_NUMPAINFACES; i++)
    {
	for (j=0; j<ST_NUMSTRAIGHTFACES; j++)
	{
	    DEH_snprintf(namebuf, 9, "STFST%d%d", i, j);
            callback(namebuf, &faces[facenum]);
            ++facenum;
	}
	DEH_snprintf(namebuf, 9, "STFTR%d0", i);	// turn right
        callback(namebuf, &faces[facenum]);
        ++facenum;
	DEH_snprintf(namebuf, 9, "STFTL%d0", i);	// turn left
        callback(namebuf, &faces[facenum]);
        ++facenum;
	DEH_snprintf(namebuf, 9, "STFOUCH%d", i);	// ouch!
        callback(namebuf, &faces[facenum]);
        ++facenum;
	DEH_snprintf(namebuf, 9, "STFEVL%d", i);	// evil grin ;)
        callback(namebuf, &faces[facenum]);
        ++facenum;
	DEH_snprintf(namebuf, 9, "STFKILL%d", i);	// pissed off
        callback(namebuf, &faces[facenum]);
        ++facenum;
    }

    callback(DEH_String("STFGOD0"), &faces[facenum]);
    ++facenum;
    callback(DEH_String("STFDEAD0"), &faces[facenum]);
    ++facenum;
}

static void ST_loadCallback(const char *lumpname, patch_t **variable)
{
    *variable = (patch_t*)W_CacheLumpName(lumpname, PU_STATIC);
}

DOOM_C_API void ST_loadGraphics(void)
{
    ST_loadUnloadGraphics(ST_loadCallback);
}

DOOM_C_API st_bordertile_t ST_GetBorderTileStyle()
{
	return (st_bordertile_t)st_border_tile_style;
}

DOOM_C_API void ST_SetBorderTileStyle( st_bordertile_t mode, const char* flatname )
{
	st_border_tile_style = mode;
	st_border_tile_custom = flatname;

	switch( mode )
	{
	case STB_WADDefined:
		V_TransposeFlat( gamemode == commercial ? DEH_String("GRNROCK") : DEH_String("FLOOR7_2"), &tileflat, PU_STATIC );
		break;
	case STB_Flat5_4:
		V_TransposeFlat( "FLAT5_4", &tileflat, PU_STATIC );
		break;
	case STB_Custom:
		V_TransposeFlat( flatname, &tileflat, PU_STATIC );
		break;
	default:
		break;
	}
}

DOOM_C_API void ST_loadData(void)
{
    lu_palette = W_GetNumForName (DEH_String("PLAYPAL"));
    ST_loadGraphics();

	memset( &tileflat, 0, sizeof( vbuffer_t ) );

	ST_SetBorderTileStyle( (st_bordertile_t)st_border_tile_style, st_border_tile_custom );
}

static void ST_unloadCallback(const char *lumpname, patch_t **variable)
{
    W_ReleaseLumpName(lumpname);
    *variable = NULL;
}

DOOM_C_API void ST_unloadGraphics(void)
{
    ST_loadUnloadGraphics(ST_unloadCallback);
}

DOOM_C_API void ST_unloadData(void)
{
    ST_unloadGraphics();
}

DOOM_C_API void ST_initData(void)
{

    int		i;

    st_firsttime = true;
    plyr = &players[consoleplayer];

    st_clock = 0;
    st_chatstate = StartChatState;
    st_gamestate = FirstPersonState;

    st_statusbaron = true;
    st_oldchat = st_chat = false;
    st_cursoron = false;

    st_faceindex = 0;
    st_palette = -1;

    st_oldhealth = -1;

    for (i=0;i<NUMWEAPONS;i++)
	oldweaponsowned[i] = plyr->weaponowned[i];

    for (i=0;i<3;i++)
	keyboxes[i] = -1;

    STlib_init();

}



DOOM_C_API void ST_createWidgets(void)
{

    int i;

    // ready weapon ammo
    STlib_initNum(&w_ready,
		  ST_AMMOX,
		  ST_AMMOY,
		  tallnum,
		  &plyr->ammo[weaponinfo[plyr->readyweapon].ammo],
		  &st_statusbaron,
		  ST_AMMOWIDTH );

    // the last weapon type
    w_ready.data = plyr->readyweapon; 

    // health percentage
    STlib_initPercent(&w_health,
		      ST_HEALTHX,
		      ST_HEALTHY,
		      tallnum,
		      &plyr->health,
		      &st_statusbaron,
		      tallpercent);

    // arms background
    STlib_initBinIcon(&w_armsbg,
		      ST_ARMSBGX,
		      ST_ARMSBGY,
		      armsbg,
		      &st_notdeathmatch,
		      &st_statusbaron);

    // weapons owned
    for(i=0;i<6;i++)
    {
        STlib_initMultIcon(&w_arms[i],
                           ST_ARMSX+(i%3)*ST_ARMSXSPACE,
                           ST_ARMSY+(i/3)*ST_ARMSYSPACE,
                           arms[i],
                           &plyr->weaponowned[i+1],
                           &st_armson);
    }

    // frags sum
    STlib_initNum(&w_frags,
		  ST_FRAGSX,
		  ST_FRAGSY,
		  tallnum,
		  &st_fragscount,
		  &st_fragson,
		  ST_FRAGSWIDTH);

    // faces
    STlib_initMultIcon(&w_faces,
		       ST_FACESX,
		       ST_FACESY,
		       faces,
		       &st_faceindex,
		       &st_statusbaron);

    // armor percentage - should be colored later
    STlib_initPercent(&w_armor,
		      ST_ARMORX,
		      ST_ARMORY,
		      tallnum,
		      &plyr->armorpoints,
		      &st_statusbaron, tallpercent);

    // keyboxes 0-2
    STlib_initMultIcon(&w_keyboxes[0],
		       ST_KEY0X,
		       ST_KEY0Y,
		       keys,
		       &keyboxes[0],
		       &st_statusbaron);
    
    STlib_initMultIcon(&w_keyboxes[1],
		       ST_KEY1X,
		       ST_KEY1Y,
		       keys,
		       &keyboxes[1],
		       &st_statusbaron);

    STlib_initMultIcon(&w_keyboxes[2],
		       ST_KEY2X,
		       ST_KEY2Y,
		       keys,
		       &keyboxes[2],
		       &st_statusbaron);

    // ammo count (all four kinds)
    STlib_initNum(&w_ammo[0],
		  ST_AMMO0X,
		  ST_AMMO0Y,
		  shortnum,
		  &plyr->ammo[0],
		  &st_statusbaron,
		  ST_AMMO0WIDTH);

    STlib_initNum(&w_ammo[1],
		  ST_AMMO1X,
		  ST_AMMO1Y,
		  shortnum,
		  &plyr->ammo[1],
		  &st_statusbaron,
		  ST_AMMO1WIDTH);

    STlib_initNum(&w_ammo[2],
		  ST_AMMO2X,
		  ST_AMMO2Y,
		  shortnum,
		  &plyr->ammo[2],
		  &st_statusbaron,
		  ST_AMMO2WIDTH);
    
    STlib_initNum(&w_ammo[3],
		  ST_AMMO3X,
		  ST_AMMO3Y,
		  shortnum,
		  &plyr->ammo[3],
		  &st_statusbaron,
		  ST_AMMO3WIDTH);

    // max ammo count (all four kinds)
    STlib_initNum(&w_maxammo[0],
		  ST_MAXAMMO0X,
		  ST_MAXAMMO0Y,
		  shortnum,
		  &plyr->maxammo[0],
		  &st_statusbaron,
		  ST_MAXAMMO0WIDTH);

    STlib_initNum(&w_maxammo[1],
		  ST_MAXAMMO1X,
		  ST_MAXAMMO1Y,
		  shortnum,
		  &plyr->maxammo[1],
		  &st_statusbaron,
		  ST_MAXAMMO1WIDTH);

    STlib_initNum(&w_maxammo[2],
		  ST_MAXAMMO2X,
		  ST_MAXAMMO2Y,
		  shortnum,
		  &plyr->maxammo[2],
		  &st_statusbaron,
		  ST_MAXAMMO2WIDTH);
    
    STlib_initNum(&w_maxammo[3],
		  ST_MAXAMMO3X,
		  ST_MAXAMMO3Y,
		  shortnum,
		  &plyr->maxammo[3],
		  &st_statusbaron,
		  ST_MAXAMMO3WIDTH);

}

static doombool	st_stopped = true;


DOOM_C_API void ST_Start (void)
{

    if (!st_stopped)
	ST_Stop();

	if( bars )
	{
		plyr = &players[ displayplayer ];
		for( SBContainer* sbar : statusbars )
		{
			sbar->ResetHierarchy();
		}
	}
	else
	{
		ST_initData();
		ST_createWidgets();
	}
    st_stopped = false;

}

DOOM_C_API void ST_Stop (void)
{
    if (st_stopped)
	return;

    I_SetPalette( (byte*)W_CacheLumpNum(lu_palette, PU_CACHE));

    st_stopped = true;
}

DOOM_C_API void ST_Init (void)
{
	lumpindex_t sbarlump = sim.allow_sbardefs ? W_CheckNumForName( "SBARDEF" ) : -1;
	if( bars == nullptr && sbarlump >= 0 )
	{
		bars = STlib_LoadSBARDEF();
	}

	if( bars == nullptr )
	{
		ST_loadData();
	}
	else
	{
		ST_InitSBars();
	}
}
