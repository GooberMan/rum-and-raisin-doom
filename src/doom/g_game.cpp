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
// DESCRIPTION:  none
//



#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "doomtype.h"
#include "doomdef.h" 
#include "doomkeys.h"
#include "doomstat.h"

#include "deh_main.h"
#include "deh_misc.h"

#include "f_finale.h"

#include "i_input.h"
#include "i_log.h"
#include "i_swap.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"

#include "m_argv.h"
#include "m_controls.h"
#include "m_dashboard.h"
#include "m_misc.h"
#include "m_menu.h"
#include "m_random.h"

#include "p_setup.h"
#include "p_saveg.h"
#include "p_tick.h"

#include "d_main.h"
#include "d_gamesim.h"

#include "wi_stuff.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "am_map.h"

// Needs access to LFB.
#include "v_video.h"

#include "w_wad.h"

#include "z_zone.h"

#include "p_local.h" 

#include "s_sound.h"

// Data.
#include "dstrings.h"
#include "sounds.h"

// SKY handling - still the wrong place.
#include "r_data.h"
#include "r_sky.h"



#include "g_game.h"
#include "m_profile.h"


#define SAVEGAMESIZE	0x2c000

DOOM_C_API void	G_ReadDemoTiccmd (ticcmd_t* cmd); 
DOOM_C_API void	G_WriteDemoTiccmd (ticcmd_t* cmd); 
DOOM_C_API void	G_PlayerReborn (int player); 
 
DOOM_C_API void	G_DoReborn (int playernum); 
 
DOOM_C_API void	G_DoLoadLevel (void); 
DOOM_C_API void	G_DoNewGame (void); 
DOOM_C_API void	G_DoPlayDemo (void); 
DOOM_C_API void	G_DoCompleted (void); 
DOOM_C_API void	G_DoWorldDone (void); 
DOOM_C_API void	G_DoSaveGame (void); 
 
// Gamestate the last time G_Ticker was called.
extern "C"
{
	gamestate_t     oldgamestate; 
 
	gameaction_t    gameaction; 
	gamestate_t     gamestate; 
	skill_t         gameskill; 
	doombool		respawnmonsters;
	doombool		fastmonsters;
	int32_t			gameflags;

	// If non-zero, exit the level after this number of minutes.

	int             timelimit;

	doombool         paused;
	doombool			renderpaused;
	doombool         sendpause;             	// send a pause event next tic 
	doombool         sendsave;             	// send a save event next tic 
	doombool         usergame;               // ok to save / end game 
 
	doombool         timingdemo;             // if true, exit with report on completion 
	doombool         nodrawers;              // for comparative timing purposes 
	uint64_t        starttime;          	// for comparative timing purposes  	 
 
	doombool         viewactive; 
 
	int             deathmatch;           	// only if started as net death 
	doombool         netgame;                // only true if packets are broadcast 
	doombool			solonetgame;
	doombool         playeringame[MAXPLAYERS]; 
	player_t        players[MAXPLAYERS]; 

	doombool         turbodetected[MAXPLAYERS];
 
	int             consoleplayer;          // player taking events and displaying 
	int             displayplayer;          // view being displayed 
	int             levelstarttic;          // gametic at level start 
	int             totalkills, totalitems, totalsecret;    // for intermission 
 
	char           *demoname;
	doombool         demorecording;
	doombool         longtics;               // cph's doom 1.91 longtics hack
	doombool         lowres_turn;            // low resolution turning for longtics
	doombool         demoplayback; 
	doombool		netdemo; 
	byte*		demobuffer;
	byte*		demo_p;
	byte*		demoend; 
	doombool         singledemo;            	// quit after playing a demo from cmdline 

	typedef struct auditsector_s
	{
		fixed_t floor;
		fixed_t ceil;
		fixed_t light;
	} auditsector_t;

	typedef struct auditthing_s
	{
		fixed_t x;
		fixed_t y;
		fixed_t z;
	} auditthing_t;

	doombool		auditrecording = false;
	doombool		auditplaying = false;
	doombool		auditplaybackerror = false;
	char*			auditname = NULL;
	byte*			auditbuffer = NULL;
	byte*			auditbufferend = NULL;
	byte*			auditbufferpos = NULL;
}

const uint32_t AUDITIDENTIFIER		= 0xA55E55ED;
const uint32_t FRAMEIDENTIFIER		= 0xA110CA7E;
const uint32_t ENDFRAMEIDENTIFIER	= 0x7AC71E55;
const uint32_t SECTORIDENTIFIER		= 0x0FACADE5;
const uint32_t THINGIDENTIFIER		= 0xDEADBEA7;

#define AUDITBUFFERDEFAULTSIZE ( 16 << 20 ) // 16MB default

#define WRITEFORAUDIT( x ) G_WriteAuditBuffer( (byte*)&x, sizeof( x ) )
#define CHECKFORAUDIT( type, expected, errormessage, ... ) \
{ \
	type* check = (type*)auditbufferpos; \
	if( *check != expected ) \
	{ \
		auditplaybackerror = true; \
		dashboardactive = Dash_Normal; \
		S_StartSound( NULL, sfx_pdiehi ); \
		I_LogAddEntryVar( Log_Error, errormessage, __VA_ARGS__ ); \
		__debugbreak(); \
		return; \
	} \
	auditbufferpos += sizeof( type ); \
}

void G_WriteAuditBuffer( byte* data, size_t size )
{
	ptrdiff_t buffersize = auditbufferend - auditbuffer;
	ptrdiff_t currpos = auditbufferpos - auditbuffer;
	ptrdiff_t endpos = ( auditbufferpos + size ) - auditbuffer;

	if( endpos >= buffersize )
	{
		ptrdiff_t newbuffersize = buffersize + ( buffersize >> 2 );
		byte* newbuffer = (byte*)Z_Malloc( newbuffersize, PU_STATIC, NULL );
		memcpy( newbuffer, auditbuffer, buffersize );

		auditbuffer = newbuffer;
		auditbufferend = newbuffer + newbuffersize;
		auditbufferpos = newbuffer + currpos;
	}

	memcpy( auditbufferpos, data, size );
	auditbufferpos += size;
}

void G_InitAuditBufferRecording( size_t size )
{
	auditrecording = true;
	auditbuffer = (byte*)Z_Malloc( size, PU_STATIC, NULL );
	memset( auditbuffer, 0, size );
	auditbufferend = auditbuffer + size;
	auditbufferpos = auditbuffer;

	WRITEFORAUDIT( AUDITIDENTIFIER );
}

void G_InitAuditBufferPlaying( const char* filename )
{
	size_t auditnamelen = strlen( filename ) + 5;
	char* auditname = (char*)Z_Malloc( auditnamelen, PU_STATIC, NULL );
	M_snprintf( auditname, auditnamelen, "%s.aud", filename );

	byte* buffer = NULL;
	int32_t size = M_ReadFile( auditname, &buffer );

	if( size > 0 )
	{
		auditplaying = true;
		auditbuffer = auditbufferpos = buffer;
		auditbufferend = buffer + size;

		CHECKFORAUDIT( uint32_t, AUDITIDENTIFIER, "Invalid audit file %s", auditname );
	}
}

void G_AuditFrame()
{
	if( auditrecording )
	{
		WRITEFORAUDIT( FRAMEIDENTIFIER );
		WRITEFORAUDIT( gametic );
		WRITEFORAUDIT( SECTORIDENTIFIER );
		WRITEFORAUDIT( numsectors );
		
		int32_t numthings = 0;
		for( sector_t* sector = sectors; sector != ( sectors + numsectors ); ++sector )
		{
			auditsector_t sec = { sector->floorheight, sector->ceilingheight, sector->lightlevel };
			WRITEFORAUDIT( sec );

			for( mobj_t* mobj = sector->thinglist; mobj != NULL; mobj = mobj->snext )
			{
				++numthings;
			}
		}

		WRITEFORAUDIT( THINGIDENTIFIER );
		WRITEFORAUDIT( numthings );
		for( sector_t* sector = sectors; sector != ( sectors + numsectors ); ++sector )
		{
			for( mobj_t* mobj = sector->thinglist; mobj != NULL; mobj = mobj->snext )
			{
				auditthing_t thing = { mobj->x, mobj->y, mobj->z };
				WRITEFORAUDIT( thing );
			}
		}

		WRITEFORAUDIT( ENDFRAMEIDENTIFIER );
	}
	else if( auditplaying )
	{
		CHECKFORAUDIT( uint32_t, FRAMEIDENTIFIER, "Invalid frame identifier, should be %08X", FRAMEIDENTIFIER );
		CHECKFORAUDIT( uint64_t, gametic, "Invalid gametic, should be %d", gametic );
		CHECKFORAUDIT( uint32_t, SECTORIDENTIFIER, "Invalid sector identifier, should be %08X", SECTORIDENTIFIER );
		CHECKFORAUDIT( int32_t, numsectors, "Invalid sector count, should be %d", numsectors );

		int32_t numthings = 0;
		for( sector_t* sector = sectors; sector != ( sectors + numsectors ); ++sector )
		{
			CHECKFORAUDIT( fixed_t, sector->floorheight, "Sector %d floor shouldn't be %f", sector->index, sector->floorheight / 65536.0 );
			CHECKFORAUDIT( fixed_t, sector->ceilingheight, "Sector %d ceiling shouldn't be %f", sector->index, sector->ceilingheight / 65536.0 );
			CHECKFORAUDIT( int32_t, sector->lightlevel, "Sector %d light shouldn't be %d", sector->index, sector->lightlevel );

			for( mobj_t* mobj = sector->thinglist; mobj != NULL; mobj = mobj->snext )
			{
				++numthings;
			}
		}

		CHECKFORAUDIT( uint32_t, THINGIDENTIFIER, "Invalid thing identifier, should be %08X", THINGIDENTIFIER );
		CHECKFORAUDIT( int32_t, numthings, "Invalid thing count, should be %d", numthings );
		int32_t currthing = 0;
		for( sector_t* sector = sectors; sector != ( sectors + numsectors ); ++sector )
		{
			for( mobj_t* mobj = sector->thinglist; mobj != NULL; mobj = mobj->snext )
			{
				CHECKFORAUDIT( fixed_t, mobj->x, "Thing %d x shouldn't be %f", currthing, mobj->x / 65536.0 );
				CHECKFORAUDIT( fixed_t, mobj->y, "Thing %d y shouldn't be %f", currthing, mobj->y / 65536.0 );
				CHECKFORAUDIT( fixed_t, mobj->z, "Thing %d z shouldn't be %d", currthing, mobj->z / 65536.0 );
				++currthing;
			}
		}

		CHECKFORAUDIT( uint32_t, ENDFRAMEIDENTIFIER, "Invalid end frame identifier, should be %08X", ENDFRAMEIDENTIFIER );
	}
}

extern "C"
{
	doombool         testcontrols = false;    // Invoked by setup to test controls
	int             testcontrols_mousespeed;
 

 
	wbstartstruct_t wminfo;               	// parms for world map / intermission 
 
	byte		consistancy[MAXPLAYERS][BACKUPTICS]; 
 
	#define MAXPLMOVE		(forwardmove[1]) 
 
	#define TURBOTHRESHOLD	0x32

	fixed_t         forwardmove[2] = {0x19, 0x32}; 
	fixed_t         sidemove[2] = {0x18, 0x28}; 
	fixed_t         angleturn[3] = {640, 1280, 320};    // + slow turn 

	static int *weapon_keys[] = {
		&key_weapon1,
		&key_weapon2,
		&key_weapon3,
		&key_weapon4,
		&key_weapon5,
		&key_weapon6,
		&key_weapon7,
		&key_weapon8
	};

	// Set to -1 or +1 to switch to the previous or next weapon.

	static int next_weapon = 0;

	// Used for prev/next weapon keys.

	static const struct
	{
		weapontype_t weapon;
		weapontype_t weapon_num;
	} weapon_order_table[] = {
		{ wp_fist,            wp_fist },
		{ wp_chainsaw,        wp_fist },
		{ wp_pistol,          wp_pistol },
		{ wp_shotgun,         wp_shotgun },
		{ wp_supershotgun,    wp_shotgun },
		{ wp_chaingun,        wp_chaingun },
		{ wp_missile,         wp_missile },
		{ wp_plasma,          wp_plasma },
		{ wp_bfg,             wp_bfg }
	};

	#define SLOWTURNTICS	6 
 
	#define NUMKEYS		256 
	#define MAX_JOY_BUTTONS 20

	static doombool  gamekeys[NUMKEYS]; 
	doombool* gamekeydown = gamekeys;
	static int      turnheld;		// for accelerative turning 
 
	static doombool  mousearray[MAX_MOUSE_BUTTONS + 1];
	doombool* mousebuttons = &mousearray[1];  // allow [-1]

	// mouse values are used once 
	int             mousex;
	int             mousey;         

	static int      dclicktime;
	static doombool  dclickstate;
	static int      dclicks; 
	static int      dclicktime2;
	static doombool  dclickstate2;
	static int      dclicks2;

	// joystick values are repeated 
	static int      joyxmove;
	static int      joyymove;
	static int      joystrafemove;
	static doombool  joyarray[MAX_JOY_BUTTONS + 1]; 
	doombool* joybuttons = &joyarray[1];		// allow [-1] 
 
	static int      savegameslot; 
	static char     savedescription[32]; 
 
	#define	BODYQUESIZE	32

	mobj_t*		bodyque[BODYQUESIZE]; 
	int		bodyqueslot; 
 
	int             vanilla_savegame_limit = 1;
	int             vanilla_demo_limit = 1;
}
 
int G_CmdChecksum (ticcmd_t* cmd) 
{ 
    size_t		i;
    int		sum = 0; 
	 
    for (i=0 ; i< sizeof(*cmd)/4 - 1 ; i++) 
	sum += ((int *)cmd)[i]; 
		 
    return sum; 
} 

static doombool WeaponSelectable(weapontype_t weapon)
{
	if( weaponinfo[ weapon ].mingamemode > gamemode )
	{
		return false;
	}

    if (!players[consoleplayer].weaponowned[weapon])
    {
        return false;
    }

    // Can't select the fist if we have the chainsaw, unless
    // we also have the berserk pack.

    if (weapon == wp_fist
     && players[consoleplayer].weaponowned[wp_chainsaw]
     && !players[consoleplayer].powers[pw_strength])
    {
        return false;
    }

    return true;
}

static int G_NextWeapon(int direction)
{
    int32_t weapon;
    int start_i, i;

    // Find index in the table.

    if (players[consoleplayer].pendingweapon == wp_nochange)
    {
        weapon = players[consoleplayer].readyweapon;
    }
    else
    {
        weapon = players[consoleplayer].pendingweapon;
    }

    for (i=0; i<arrlen(weapon_order_table); ++i)
    {
        if (weapon_order_table[i].weapon == weapon)
        {
            break;
        }
    }

    // Switch weapon. Don't loop forever.
    start_i = i;
    do
    {
        i += direction;
        i = (i + arrlen(weapon_order_table)) % arrlen(weapon_order_table);
    } while (i != start_i && !WeaponSelectable(weapon_order_table[i].weapon));

    return weapon_order_table[i].weapon_num;
}

//
// G_BuildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer. 
// If recording a demo, write it out 
//

void G_BuildTiccmd (ticcmd_t* cmd, uint64_t maketic) 
{ 
	int		i;
	doombool	strafe;
	doombool	bstrafe;
	int		speed;
	int		tspeed;
	int		forward;
	int		side;

	memset( cmd, 0, sizeof( ticcmd_t ) );

	cmd->consistancy =  consistancy[ consoleplayer ][ maketic % BACKUPTICS ]; 
 
	strafe = GameKeyDown( key_strafe ) || MouseButtonDown( mousebstrafe ) || JoyButtonDown( joybstrafe );

	// fraggle: support the old "joyb_speed = 31" hack which
	// allowed an autorun effect

	speed = key_speed >= NUMKEYS
			|| ( !GameKeyDown( key_speed ) && joybspeed >= MAX_JOY_BUTTONS )
			|| ( GameKeyDown( key_speed ) && joybspeed < MAX_JOY_BUTTONS )
			|| ( joybspeed < MAX_JOY_BUTTONS && JoyButtonDown( joybspeed ) );
 
	forward = side = 0;

	// use two stage accelerative turning
	// on the keyboard and joystick
	if ( joyxmove < 0
		|| joyxmove > 0
		|| GameKeyDown( key_right )
		|| GameKeyDown( key_left ) )
	{
		turnheld += ticdup;
	}
	else
	{
		turnheld = 0;
	}

	if( turnheld < SLOWTURNTICS )
	{
		tspeed = 2;		// slow turn
	}
	else
	{
		tspeed = speed;
	}

	// let movement keys cancel each other out
	if( strafe )
	{
		if( GameKeyDown( key_right ) || joyxmove > 0 )
		{
			side += sidemove[ speed ];
		}
		if( GameKeyDown( key_left ) || joyxmove < 0 )
		{
			side -= sidemove[ speed ];
		}
	}
	else
	{
		if( GameKeyDown(  key_right ) || joyxmove > 0 )
		{
			cmd->angleturn -= angleturn[ tspeed ];
		}
		if( GameKeyDown( key_left ) || joyxmove < 0 )
		{
			cmd->angleturn += angleturn[ tspeed ];
		}
	}
 
	if( GameKeyDown( key_up )
		|| MouseButtonDown( mousebforward )
		|| joyymove < 0 )
	{
		forward += forwardmove[ speed ];
	}
	if( GameKeyDown( key_down )
		|| MouseButtonDown( mousebbackward )
		|| joyymove > 0 )
	{
		forward -= forwardmove[ speed ];
	}

	if ( GameKeyDown( key_strafeleft )
		|| JoyButtonDown( joybstrafeleft )
		|| MouseButtonDown( mousebstrafeleft )
		|| joystrafemove < 0 )
	{
		side -= sidemove[ speed ];
	}

	if ( GameKeyDown( key_straferight )
		|| JoyButtonDown( joybstraferight )
		|| MouseButtonDown( mousebstraferight )
		|| joystrafemove > 0 )
	{
		side += sidemove[ speed ];
	}

	// buttons
	cmd->chatchar = HU_dequeueChatChar();
 
	if ( GameKeyDown( key_fire )
		|| MouseButtonDown( mousebfire )
		|| JoyButtonDown( joybfire ) )
	{
		cmd->buttons |= BT_ATTACK;
	}
 
	if ( GameKeyDown( key_use )
		|| JoyButtonDown( joybuse )
		|| MouseButtonDown( mousebuse ) )
	{
		cmd->buttons |= BT_USE;
		// clear double clicks if hit use button
		dclicks = 0;
	}

	// If the previous or next weapon button is pressed, the
	// next_weapon variable is set to change weapons when
	// we generate a ticcmd.  Choose a new weapon.

	if( gamestate == GS_LEVEL && next_weapon != 0 )
	{
		i = G_NextWeapon( next_weapon );
		cmd->buttons |= BT_CHANGE;
		cmd->buttons |= i << BT_WEAPONSHIFT;
	}
	else
	{
		// Check weapon keys.
		for ( i = 0; i < arrlen( weapon_keys ); ++i )
		{
			int key = *weapon_keys[i];

			if( GameKeyDown( key ) )
			{
				cmd->buttons |= BT_CHANGE;
				cmd->buttons |= i << BT_WEAPONSHIFT;
				break;
			}
		}
	}

	next_weapon = 0;

	// Mouse stuff
	if( dclick_use )
	{
		// forward double click
		if( IsBound( mousebforward ) )
		{
			if ( mousebuttons[ mousebforward ] != dclickstate && dclicktime > 1 ) 
			{ 
				dclickstate = mousebuttons[ mousebforward ];
				if( dclickstate )
				{ 
					++dclicks;
				}
				if( dclicks == 2 )
				{
					cmd->buttons |= BT_USE;
					dclicks = 0;
				}
				else
				{
					dclicktime = 0;
				}
			}
			else
			{
				dclicktime += ticdup;
				if (dclicktime > 20)
				{
					dclicks = 0;
					dclickstate = 0;
				}
			}
		}

		// strafe double click
		bstrafe = MouseButtonDown( mousebstrafe ) || JoyButtonDown( joybstrafe );

		if( bstrafe != dclickstate2 && dclicktime2 > 1 )
		{
			dclickstate2 = bstrafe;
			if( dclickstate2 )
			{
				dclicks2++;
			}
			if( dclicks2 == 2 )
			{
				cmd->buttons |= BT_USE;
				dclicks2 = 0;
			}
			else
			{
				dclicktime2 = 0;
			}
		}
		else
		{
			dclicktime2 += ticdup;
			if( dclicktime2 > 20 )
			{
				dclicks2 = 0;
				dclickstate2 = 0;
			}
		}
	}

	forward += mousey;

	if (strafe)
	{
		side += mousex*2;
	}
	else
	{
		cmd->angleturn -= mousex*0x8;
	}

	if (mousex == 0)
	{
		// No movement in the previous frame
		testcontrols_mousespeed = 0;
	}

	mousex = mousey = 0; 

	forward = M_CLAMP( forward, -MAXPLMOVE, MAXPLMOVE );
	side = M_CLAMP( side, -MAXPLMOVE, MAXPLMOVE );

	cmd->forwardmove += forward;
	cmd->sidemove += side;

	// special buttons
	if( sendpause )
	{
		sendpause = false;
		cmd->buttons = BT_SPECIAL | BTS_PAUSE;
	}
 
	if (sendsave)
	{
		sendsave = false;
		cmd->buttons = BT_SPECIAL | BTS_SAVEGAME | ( savegameslot << BTS_SAVESHIFT );
	}

	// low-res turning

	if( lowres_turn )
	{
		static signed short carry = 0;
		signed short desired_angleturn;

		desired_angleturn = cmd->angleturn + carry;

		// round angleturn to the nearest 256 unit boundary
		// for recording demos with single byte values for turn

		cmd->angleturn = ( desired_angleturn + 128 ) & 0xff00;

		// Carry forward the error from the reduced resolution to the
		// next tic, so that successive small movements can accumulate.

		carry = desired_angleturn - cmd->angleturn;
	}
}


DOOM_C_API void P_CalcHeight (player_t* player);

//
// G_DoLoadLevel 
//
void G_DoLoadLevel (void) 
{ 
    int             i; 

    // Set the sky map.
    // First thing, we have a dummy sky texture name,
    //  a flat. The data is in the WAD only because
    //  we look for an actual index, instead of simply
    //  setting one.

    // The "Sky never changes in Doom II" bug was fixed in
    // the id Anthology version of doom2.exe for Final Doom.
    if( fix.same_sky_texture
		|| ( gamemode == commercial && ( gameversion == exe_final2 || gameversion == exe_chex ) ) )
    {
		const char* skytexturename = DEH_String( current_map->sky_texture.val );
		R_SetDefaultSky( skytexturename );
    }

    levelstarttic = gametic;        // for time calculation
    
    if (wipegamestate == GS_LEVEL)
	{
		wipegamestate = GS_INVALID;             // force a wipe 
	}

    gamestate = GS_LEVEL; 

    for (i=0 ; i<MAXPLAYERS ; i++) 
    { 
	turbodetected[i] = false;
	if (playeringame[i] && players[i].playerstate == PST_DEAD) 
	    players[i].playerstate = PST_REBORN; 
	memset (players[i].frags,0,sizeof(players[i].frags)); 
    } 

	P_SetupLevel( current_map, 0, gameskill );
	displayplayer = consoleplayer;		// view the guy you are playing    
	gameaction = ga_nothing; 
	Z_CheckHeap ();

	// clear cmd building stuff

    memset(gamekeys, 0, sizeof(gamekeys));
    joyxmove = joyymove = joystrafemove = 0;
    mousex = mousey = 0;
    sendpause = sendsave = paused = false;
    memset(mousearray, 0, sizeof(mousearray));
    memset(joyarray, 0, sizeof(joyarray));

    if (testcontrols)
    {
        players[consoleplayer].message = "Press escape to quit.";
    }

	// Having the debug UI open means you can start a game when in a paused state.
	// So we prime the view height to avoid bad visuals on level start.
	for (i=0 ; i<MAXPLAYERS ; i++) 
	{
		if( playeringame[i] )
		{
			P_CalcHeight( &players[i] );
		}
	}
} 

static void SetJoyButtons(unsigned int buttons_mask)
{
    int i;

    for (i=0; i<MAX_JOY_BUTTONS; ++i)
    {
        int button_on = (buttons_mask & (1 << i)) != 0;

        // Detect button press:

        if (!joybuttons[i] && button_on)
        {
            // Weapon cycling:

            if (i == joybprevweapon)
            {
                next_weapon = -1;
            }
            else if (i == joybnextweapon)
            {
                next_weapon = 1;
            }
        }

        joybuttons[i] = button_on;
    }
}

static void SetMouseButtons(unsigned int buttons_mask)
{
    int i;

    for (i=0; i<MAX_MOUSE_BUTTONS; ++i)
    {
        unsigned int button_on = (buttons_mask & (1 << i)) != 0;

        // Detect button press:

        if (!mousebuttons[i] && button_on)
        {
            if (i == mousebprevweapon)
            {
                next_weapon = -1;
            }
            else if (i == mousebnextweapon)
            {
                next_weapon = 1;
            }
        }

	mousebuttons[i] = button_on;
    }
}

//
// G_Responder  
// Get info needed to make ticcmd_ts for the players.
// 
doombool G_Responder (event_t* ev) 
{ 
    // allow spy mode changes even during the demo
    if (gamestate == GS_LEVEL && ev->type == ev_keydown 
     && ev->data1 == key_spy && (singledemo || !deathmatch) )
    {
	// spy mode 
	do 
	{ 
	    displayplayer++; 
	    if (displayplayer == MAXPLAYERS) 
		displayplayer = 0; 
	} while (!playeringame[displayplayer] && displayplayer != consoleplayer); 
	return true; 
    }
    
    // any other key pops up menu if in demos
    if (gameaction == ga_nothing && !singledemo && 
	(demoplayback || gamestate == GS_DEMOSCREEN) 
	) 
    { 
	if (ev->type == ev_keydown ||  
	    (ev->type == ev_mouse && ev->data1) || 
	    (ev->type == ev_joystick && ev->data1) ) 
	{ 
	    M_StartControlPanel (); 
	    return true; 
	} 
	return false; 
    } 

    if (gamestate == GS_LEVEL) 
    { 
#if 0 
	if (devparm && ev->type == ev_keydown && ev->data1 == ';') 
	{ 
	    G_DeathMatchSpawnPlayer (0); 
	    return true; 
	} 
#endif 
	if (HU_Responder (ev)) 
	    return true;	// chat ate the event 
	if (ST_Responder (ev)) 
	    return true;	// status window ate it 
	if (AM_Responder (ev)) 
	    return true;	// automap ate it 
    } 
	 
    if (gamestate == GS_FINALE) 
    { 
	if (F_Responder (ev)) 
	    return true;	// finale ate the event 
    } 

    if (testcontrols && ev->type == ev_mouse)
    {
        // If we are invoked by setup to test the controls, save the 
        // mouse speed so that we can display it on-screen.
        // Perform a low pass filter on this so that the thermometer 
        // appears to move smoothly.

        testcontrols_mousespeed = abs(ev->data2);
    }

    // If the next/previous weapon keys are pressed, set the next_weapon
    // variable to change weapons when the next ticcmd is generated.

    if (ev->type == ev_keydown && ev->data1 == key_prevweapon)
    {
        next_weapon = -1;
    }
    else if (ev->type == ev_keydown && ev->data1 == key_nextweapon)
    {
        next_weapon = 1;
    }

    switch (ev->type) 
    { 
	case ev_keydown: 
		if (ev->data1 == key_pause) 
		{ 
			sendpause = true; 
		}
		else if( ev->data1 == key_toggle_autorun )
		{
			if( joybspeed != 29 )
			{
				joybspeed = 29;
				players[consoleplayer].message = AUTORUNON;
				S_StartSound( NULL, sfx_swtchn );
			}
			else
			{
				joybspeed = 2;
				players[consoleplayer].message = AUTORUNOFF;
				S_StartSound( NULL, sfx_swtchx );
			}
		}
		else if (ev->data1 <NUMKEYS) 
		{
			gamekeydown[ev->data1] = true; 
		}
				return true;    // eat key down events 
 
      case ev_keyup: 
	if (ev->data1 <NUMKEYS) 
	    gamekeydown[ev->data1] = false; 
	return false;   // always let key up events filter down 
		 
      case ev_mouse: 
        SetMouseButtons(ev->data1);
	mousex += ev->data2*(mouseSensitivity+5)/10; 
	mousey += ev->data3*(mouseSensitivity+5)/10; 
	return true;    // eat events 
 
      case ev_joystick: 
        SetJoyButtons(ev->data1);
	joyxmove = ev->data2; 
	joyymove = ev->data3; 
        joystrafemove = ev->data4;
	return true;    // eat events 
 
      default: 
	break; 
    } 
 
    return false; 
} 
 
extern "C"
{
    extern char *player_names[4];
}
 
//
// G_Ticker
// Make ticcmd_ts for the players.
//
void G_Ticker (void) 
{ 
	M_PROFILE_PUSH( __FUNCTION__, __FILE__, __LINE__ );

	if( auditplaybackerror )
	{
		return;
	}

    int		i;
    int		buf; 
    ticcmd_t*	cmd;
    
    // do player reborns if needed
    for (i=0 ; i<MAXPLAYERS ; i++) 
	if (playeringame[i] && players[i].playerstate == PST_REBORN) 
	    G_DoReborn (i);
    
    // do things to change the game state
    while (gameaction != ga_nothing) 
    { 
	switch (gameaction) 
	{ 
	  case ga_loadlevel: 
	    G_DoLoadLevel (); 
	    break; 
	  case ga_newgame: 
	    G_DoNewGame (); 
	    break; 
	  case ga_loadgame: 
	    G_DoLoadGame (); 
	    break; 
	  case ga_savegame: 
	    G_DoSaveGame (); 
	    break; 
	  case ga_playdemo: 
	    G_DoPlayDemo (); 
	    break; 
	  case ga_completed: 
	    G_DoCompleted (); 
	    break; 
	  case ga_victory: 
	    F_StartFinale( current_map->endgame ); 
	    break; 
	  case ga_worlddone: 
	    G_DoWorldDone (); 
	    break; 
	  case ga_screenshot: 
	    V_ScreenShot("DOOM%02i.%s"); 
            players[consoleplayer].message = DEH_String("screen shot");
	    gameaction = ga_nothing; 
	    break; 
	  case ga_nothing: 
	    break; 
	} 
    }
    
    // get commands, check consistancy,
    // and build new consistancy check
    buf = (gametic/ticdup)%BACKUPTICS; 
 
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (playeringame[i]) 
	{ 
	    cmd = &players[i].cmd; 

	    memcpy(cmd, &netcmds[i], sizeof(ticcmd_t));

		if (demoplayback)
		{
			G_ReadDemoTiccmd (cmd);
		}
		if (demorecording)
		{
			G_WriteDemoTiccmd (cmd);
		}
	    
	    // check for turbo cheats

            // check ~ 4 seconds whether to display the turbo message. 
            // store if the turbo threshold was exceeded in any tics
            // over the past 4 seconds.  offset the checking period
            // for each player so messages are not displayed at the
            // same time.

            if (cmd->forwardmove > TURBOTHRESHOLD)
            {
                turbodetected[i] = true;
            }

            if ((gametic & 31) == 0 
             && ((gametic >> 5) % MAXPLAYERS) == i
             && turbodetected[i])
            {
                static char turbomessage[80];
                M_snprintf(turbomessage, sizeof(turbomessage),
                           "%s is turbo!", player_names[i]);
                players[consoleplayer].message = turbomessage;
                turbodetected[i] = false;
            }

	    if (netgame && !netdemo && !(gametic%ticdup) ) 
	    { 
		if (gametic > BACKUPTICS 
		    && consistancy[i][buf] != cmd->consistancy) 
		{ 
		    I_Error ("consistency failure (%i should be %i)",
			     cmd->consistancy, consistancy[i][buf]); 
		} 
		if (players[i].mo) 
		    consistancy[i][buf] = players[i].mo->x; 
		else 
		    consistancy[i][buf] = rndindex; 
	    } 
	}
    }
    
    // check for special buttons
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (playeringame[i]) 
	{ 
	    if (players[i].cmd.buttons & BT_SPECIAL) 
	    { 
		switch (players[i].cmd.buttons & BT_SPECIALMASK) 
		{ 
		  case BTS_PAUSE: 
		    paused ^= 1; 
		    if (paused) 
			S_PauseSound (); 
		    else 
			S_ResumeSound (); 
		    break; 
					 
		  case BTS_SAVEGAME: 
		    if (!savedescription[0]) 
                    {
                        M_StringCopy(savedescription, "NET GAME",
                                     sizeof(savedescription));
                    }

		    savegameslot =  
			(players[i].cmd.buttons & BTS_SAVEMASK)>>BTS_SAVESHIFT; 
		    gameaction = ga_savegame; 
		    break; 
		} 
	    } 
	}
    }

    // Have we just finished displaying an intermission screen?

    if (oldgamestate == GS_INTERMISSION && gamestate != GS_INTERMISSION)
    {
        WI_End();
    }

    oldgamestate = gamestate;
    
    // do main actions
    switch (gamestate) 
    { 
	case GS_LEVEL:
		P_Ticker ();
		ST_Ticker ();
		AM_Ticker ();
		HU_Ticker ();
		G_AuditFrame();
		break; 
	 
	case GS_INTERMISSION:
		WI_Ticker ();
		break;

	case GS_FINALE:
		F_Ticker ();
		break;
 
	case GS_DEMOSCREEN:
		D_PageTicker ();
		break;

	default:
		break;
	}

	M_PROFILE_POP( __FUNCTION__ );

} 
 
 
//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Things
//

//
// G_InitPlayer 
// Called at the start.
// Called by the game initialization functions.
//
void G_InitPlayer (int player) 
{
    // clear everything else to defaults
    G_PlayerReborn (player); 
}
 
 

//
// G_PlayerFinishLevel
// Can when a player completes a level.
//
void G_PlayerFinishLevel(int player, doombool fullreset)
{ 
    player_t*	p; 
	 
    p = &players[player]; 
	 
    memset (p->powers, 0, sizeof (p->powers)); 
    memset (p->cards, 0, sizeof (p->cards)); 
    p->mo->flags &= ~MF_SHADOW;		// cancel invisibility 
    p->extralight = 0;			// cancel gun flashes 
    p->fixedcolormap = 0;		// cancel ir gogles 
    p->damagecount = 0;			// no palette changes 
    p->bonuscount = 0; 
	if( fullreset )
	{
		p->playerstate = PST_REBORN;
	}
} 
 

//
// G_PlayerReborn
// Called after a player dies 
// almost everything is cleared and initialized 
//
void G_PlayerReborn (int player) 
{ 
    player_t*	p; 
    int		i; 
    int		frags[MAXPLAYERS]; 
    int		killcount;
    int		itemcount;
    int		secretcount; 
	 
    memcpy (frags,players[player].frags,sizeof(frags)); 
    killcount = players[player].killcount; 
    itemcount = players[player].itemcount; 
    secretcount = players[player].secretcount; 
	 
    p = &players[player]; 
    memset (p, 0, sizeof(*p)); 
 
    memcpy (players[player].frags, frags, sizeof(players[player].frags)); 
    players[player].killcount = killcount; 
    players[player].itemcount = itemcount; 
    players[player].secretcount = secretcount; 
 
    p->usedown = p->attackdown = true;	// don't do anything immediately 
    p->playerstate = PST_LIVE;       
    p->health = deh_initial_health;     // Use dehacked value

	p->weaponowned.Reset();
	p->readyweapon = p->pendingweapon = wp_pistol; 
	p->weaponowned[wp_fist] = true; 
	p->weaponowned[wp_pistol] = true; 
	p->ammo[am_clip] = deh_initial_bullets; 
	 
	for (i=0 ; i<NUMAMMO ; i++)
	{
		p->maxammo[i] = maxammo[i];
	}
		 
	if( player == displayplayer )
	{
		R_RebalanceContexts();
	}

	if( p->visitedlevels )
	{
		Z_Free( p->visitedlevels );
	}

	size_t buffersize = sizeof( doombool ) * ( current_episode->highest_map_num + 1 );
	p->visitedlevels = (doombool*)Z_Malloc( buffersize, PU_STATIC, &p->visitedlevels );
	memset( p->visitedlevels, 0, buffersize );
}

//
// G_CheckSpot  
// Returns false if the player cannot be respawned
// at the given mapthing_t spot  
// because something is occupying it 
//
DOOM_C_API void P_SpawnPlayer (mapthing_t* mthing); 
 
doombool
G_CheckSpot
( int		playernum,
  mapthing_t*	mthing ) 
{ 
    fixed_t		x;
    fixed_t		y; 
    subsector_t*	ss; 
    mobj_t*		mo; 
    int			i;
	
    if (!players[playernum].mo)
    {
	// first spawn of level, before corpses
	for (i=0 ; i<playernum ; i++)
	    if (players[i].mo->x == mthing->x << FRACBITS
		&& players[i].mo->y == mthing->y << FRACBITS)
		return false;	
	return true;
    }
		
    x = mthing->x << FRACBITS; 
    y = mthing->y << FRACBITS; 
	 
    if (!P_CheckPosition (players[playernum].mo, x, y) ) 
	return false; 
 
    // flush an old corpse if needed 
    if (bodyqueslot >= BODYQUESIZE) 
	P_RemoveMobj (bodyque[bodyqueslot%BODYQUESIZE]); 
    bodyque[bodyqueslot%BODYQUESIZE] = players[playernum].mo; 
    bodyqueslot++; 

	// spawn a teleport fog
	ss = BSP_PointInSubsector( x, y );

    // The code in the released source looks like this:
    //
    //    an = ( ANG45 * (((unsigned int) mthing->angle)/45) )
    //         >> ANGLETOFINESHIFT;
    //    mo = P_SpawnMobj (x+20*finecosine[an], y+20*finesine[an]
    //                     , ss->sector->floorheight
    //                     , MT_TFOG);
    //
    // But 'an' can be a signed value in the DOS version. This means that
    // we get a negative index and the lookups into finecosine/finesine
    // end up dereferencing values in finetangent[].
    // A player spawning on a deathmatch start facing directly west spawns
    // "silently" with no spawn fog. Emulate this.
    //
    // This code is imported from PrBoom+.

    {
        fixed_t xa, ya;
        signed int an;

        // This calculation overflows in Vanilla Doom, but here we deliberately
        // avoid integer overflow as it is undefined behavior, so the value of
        // 'an' will always be positive.
        an = (ANG45 >> ANGLETOFINESHIFT) * ((signed int) mthing->angle / 45);

        switch (an)
        {
            case 4096:  // -4096:
                xa = finetangent[2048];    // finecosine[-4096]
                ya = finetangent[0];       // finesine[-4096]
                break;
            case 5120:  // -3072:
                xa = finetangent[3072];    // finecosine[-3072]
                ya = finetangent[1024];    // finesine[-3072]
                break;
            case 6144:  // -2048:
                xa = finesine[0];          // finecosine[-2048]
                ya = finetangent[2048];    // finesine[-2048]
                break;
            case 7168:  // -1024:
                xa = finesine[1024];       // finecosine[-1024]
                ya = finetangent[3072];    // finesine[-1024]
                break;
            case 0:
            case 1024:
            case 2048:
            case 3072:
                xa = finecosine[an];
                ya = finesine[an];
                break;
            default:
                I_Error("G_CheckSpot: unexpected angle %d\n", an);
                xa = ya = 0;
                break;
        }
        mo = P_SpawnMobj(x + 20 * xa, y + 20 * ya,
                         ss->sector->floorheight, MT_TFOG);
    }

    if (players[consoleplayer].viewz != 1) 
	S_StartSound (mo, sfx_telept);	// don't start sound on first frame 
 
    return true; 
} 


//
// G_DeathMatchSpawnPlayer 
// Spawns a player at one of the random death match spots 
// called at level load and each death 
//
void G_DeathMatchSpawnPlayer (int playernum) 
{ 
    int             i,j; 
    int				selections; 
	 
    selections = deathmatch_p - deathmatchstarts; 
    if (selections < 4) 
	I_Error ("Only %i deathmatch spots, 4 required", selections); 
 
    for (j=0 ; j<20 ; j++) 
    { 
	i = P_Random() % selections; 
	if (G_CheckSpot (playernum, &deathmatchstarts[i]) ) 
	{ 
	    deathmatchstarts[i].type = playernum+1; 
	    P_SpawnPlayer (&deathmatchstarts[i]); 
	    return; 
	} 
    } 
 
    // no good spot, so the player will probably get stuck 
    P_SpawnPlayer (&playerstarts[playernum]); 
} 

//
// G_DoReborn 
// 
void G_DoReborn (int playernum) 
{ 
    int                             i; 
	 
    if (!netgame)
    {
	// reload the level from scratch
	gameaction = ga_loadlevel;  
    }
    else 
    {
	// respawn at the start

	// first dissasociate the corpse 
	players[playernum].mo->player = NULL;   
		 
	// spawn at random spot if in death match 
	if (deathmatch) 
	{ 
	    G_DeathMatchSpawnPlayer (playernum); 
	    return; 
	} 
		 
	if (G_CheckSpot (playernum, &playerstarts[playernum]) ) 
	{ 
	    P_SpawnPlayer (&playerstarts[playernum]); 
	    return; 
	}
	
	// try to spawn at one of the other players spots 
	for (i=0 ; i<MAXPLAYERS ; i++)
	{
	    if (G_CheckSpot (playernum, &playerstarts[i]) ) 
	    { 
		playerstarts[i].type = playernum+1;	// fake as other player 
		P_SpawnPlayer (&playerstarts[i]); 
		playerstarts[i].type = i+1;		// restore 
		return; 
	    }	    
	    // he's going to be inside something.  Too bad.
	}
	P_SpawnPlayer (&playerstarts[playernum]); 
    } 
} 
 
 
void G_ScreenShot (void) 
{ 
    gameaction = ga_screenshot; 
} 
 


// DOOM Par Times
int pars[4][10] = 
{ 
    {0}, 
    {0,30,75,120,90,165,180,180,30,165}, 
    {0,90,90,90,120,90,360,240,30,170}, 
    {0,90,45,90,150,90,90,165,30,135} 
}; 

// DOOM II Par Times
int cpars[32] =
{
    30,90,120,120,90,150,120,120,270,90,	//  1-10
    210,150,150,150,210,150,420,150,210,150,	// 11-20
    240,150,180,150,150,300,330,420,300,180,	// 21-30
    120,30					// 31-32
};
 

//
// G_DoCompleted 
//
doombool		secretexit; 
doombool		rebornexit;
extern char*	pagename; 
 
void G_ExitLevel( doombool resetplayers ) 
{ 
    secretexit = false;
	rebornexit = resetplayers || ( gameflags & GF_PistolStarts ) == GF_PistolStarts;
    gameaction = ga_completed; 
} 

// Here's for the german edition.
void G_SecretExitLevel( doombool resetplayers ) 
{ 
#if 0
    // IF NO WOLF3D LEVELS, NO SECRET EXIT!
    if ( (gamemode == commercial)
      && (W_CheckNumForName("map31")<0))
	secretexit = false;
    else
#endif
	secretexit = true; 
	rebornexit = resetplayers || ( gameflags & GF_PistolStarts ) == GF_PistolStarts;
    gameaction = ga_completed; 
} 
 
void G_DoCompleted (void) 
{ 
    int             i; 
	 
    gameaction = ga_nothing; 
 
	for (i=0 ; i<MAXPLAYERS ; i++) 
	{
		if (playeringame[i]) 
		{
			G_PlayerFinishLevel(i, rebornexit);        // take away cards and stuff 
		}
	}
	 
	if (automapactive)
	{
		AM_Stop ();
	}
	
	if( !secretexit
		&& current_map->endgame
		&& ( current_map->endgame->type & EndGame_StraightToVictory )
		&& !( gameflags & GF_LoopOneLevel ) )
	{
		gameaction = ga_victory;
		return;
	}

	for (i=0 ; i<MAXPLAYERS ; i++) 
	{
		players[i].didsecret |= ( current_map->map_flags & Map_Secret );

		if( players[i].visitedlevels ) players[i].visitedlevels[ current_map->map_num ] = true;
	}

	wminfo.currmap = current_map;
	wminfo.didsecret = players[consoleplayer].didsecret;

	if( !( gameflags & GF_LoopOneLevel ) )
	{
		wminfo.nextmap = secretexit	? current_map->secret_map
									: current_map->next_map;
	}
	else
	{
		wminfo.nextmap = current_map;
	}

	wminfo.maxkills = totalkills; 
	wminfo.maxitems = totalitems; 
	wminfo.maxsecret = totalsecret; 
	wminfo.maxfrags = 0; 
	wminfo.partime = DEH_ParTime( current_map ) * TICRATE;
	wminfo.pnum = consoleplayer; 
 
	size_t buffersize = sizeof( doombool ) * ( current_episode->highest_map_num + 1 );

	for (i=0 ; i<MAXPLAYERS ; i++) 
	{ 
		wminfo.plyr[i].in = playeringame[i]; 
		wminfo.plyr[i].skills = players[i].killcount; 
		wminfo.plyr[i].sitems = players[i].itemcount; 
		wminfo.plyr[i].ssecret = players[i].secretcount; 
		wminfo.plyr[i].stime = leveltime; 
		memcpy (wminfo.plyr[i].frags, players[i].frags, sizeof(wminfo.plyr[i].frags));

		if( players[i].visitedlevels )
		{
			wminfo.plyr[i].visited = (doombool*)Z_Malloc( buffersize, PU_LEVEL, &wminfo.plyr[ i ].visited );
			memcpy( wminfo.plyr[ i ].visited, players[ i ].visitedlevels, buffersize );
		}
	}
 
	gamestate = GS_INTERMISSION;
	viewactive = false;
	automapactive = false;

	WI_Start (&wminfo);
} 


//
// G_WorldDone 
//
void G_WorldDone (void) 
{ 
    gameaction = ga_worlddone;

	if( !( gameflags & GF_LoopOneLevel ) )
	{
		if( !secretexit && current_map->endgame )
		{
			F_StartFinale( current_map->endgame );
		}
		else
		{
			intermission_t* inter = secretexit ? current_map->secret_map_intermission : current_map->next_map_intermission;

			if( inter )
			{
				F_StartIntermission( inter );
			}
		}
	}
} 
 
void G_DoWorldDone (void) 
{
	int32_t curr;

    gamestate = GS_LEVEL; 

	mapinfo_t* mapinfo = NULL;

	if( !( gameflags & GF_LoopOneLevel ) )
	{
		mapinfo = secretexit	? current_map->secret_map
								: current_map->next_map;
	}
	else
	{
		mapinfo = current_map;
	}

	D_GameflowSetCurrentMap( mapinfo );

	G_DoLoadLevel ();
	gameaction = ga_nothing;
	viewactive = true;
} 
 


//
// G_InitFromSavegame
// Can be called by the startup code or the menu task. 
//
extern "C"
{
	extern doombool setsizeneeded;
	extern doombool message_dontfuckwithme;
}

char	savename[256];


void G_LoadGame (char* name) 
{ 
    M_StringCopy(savename, name, sizeof(savename));
    gameaction = ga_loadgame; 
} 

void G_DoLoadGame (void) 
{ 
    int savedleveltime;

    gameaction = ga_nothing; 
	gameflags = GF_None;

    save_stream = fopen(savename, "rb");

    if (save_stream == NULL)
    {
        I_Error("Could not load savegame %s", savename);
    }

	savegametype_t type = P_ReadSaveGameType();
	if( type == SaveGame_Invalid )
	{
		I_Error( "Bad savegame" );
	}

	if( sim.extended_saves && type == SaveGame_LimitRemoving )
	{
		P_UnArchiveLimitRemovingData( true );
	}

    savegame_error = false;

    if (!P_ReadSaveGameHeader())
    {
        fclose(save_stream);
        return;
    }

	savedleveltime = leveltime;

	// load a base level 
	G_InitNew (gameskill, current_map, (gameflags_t)gameflags, 0); 

	leveltime = savedleveltime;

    // dearchive all the modifications
    P_UnArchivePlayers (); 
    P_UnArchiveWorld (); 
    P_UnArchiveThinkers (); 
    P_UnArchiveSpecials (); 
 
	if ( P_ReadSaveGameEOF() != SaveGame_Vanilla )
	{
		I_Error ("Bad savegame");
	}

	if( sim.extended_saves && type == SaveGame_LimitRemoving )
	{
		P_UnArchiveLimitRemovingData( false );
	}

	if( !sim.extended_saves && type == SaveGame_LimitRemoving )
	{
		players[consoleplayer].message = "Saving will erase limit-removing data.";
		message_dontfuckwithme = true;
	}

    fclose(save_stream);

	P_UpdateInstanceData();

    
    if (setsizeneeded) R_ExecuteSetViewSize( );
    
    // draw the pattern into the back screen
    R_FillBackScreen ();   
} 
 

//
// G_SaveGame
// Called by the menu task.
// Description is a 24 byte text string 
//
void
G_SaveGame
( int	slot,
  char*	description )
{
	savegameslot = slot;
	M_StringCopy(savedescription, description, sizeof(savedescription));
	sendsave = true;
}

void G_DoSaveGame (void) 
{ 
    char *savegame_file;
    char *temp_savegame_file;
    char *recovery_savegame_file;

    recovery_savegame_file = NULL;
    temp_savegame_file = P_TempSaveGameFile();
    savegame_file = P_SaveGameFile(savegameslot);

    // Open the savegame file for writing.  We write to a temporary file
    // and then rename it at the end if it was successfully written.
    // This prevents an existing savegame from being overwritten by
    // a corrupted one, or if a savegame buffer overrun occurs.
    save_stream = fopen(temp_savegame_file, "wb");

    if (save_stream == NULL)
    {
        // Failed to save the game, so we're going to have to abort. But
        // to be nice, save to somewhere else before we call I_Error().
        recovery_savegame_file = M_TempFile("recovery.dsg");
        save_stream = fopen(recovery_savegame_file, "wb");
        if (save_stream == NULL)
        {
            I_Error("Failed to open either '%s' or '%s' to write savegame.",
                    temp_savegame_file, recovery_savegame_file);
        }
    }

    savegame_error = false;

    P_WriteSaveGameHeader(savedescription);

    P_ArchivePlayers ();
    P_ArchiveWorld ();
    P_ArchiveThinkers ();
    P_ArchiveSpecials ();

    P_WriteSaveGameEOF( SaveGame_Vanilla );

    // Enforce the same savegame size limit as in Vanilla Doom,
    // except if the vanilla_savegame_limit setting is turned off.

    if ( ( vanilla_savegame_limit && !sim.extended_saves ) && ftell(save_stream) > SAVEGAMESIZE)
    {
        I_Error("Savegame buffer overrun");
    }

	if( sim.extended_saves )
	{
		P_ArchiveLimitRemovingData();
		P_WriteSaveGameEOF( SaveGame_LimitRemoving );
	}

    // Finish up, close the savegame file.

    fclose(save_stream);

    if (recovery_savegame_file != NULL)
    {
        // We failed to save to the normal location, but we wrote a
        // recovery file to the temp directory. Now we can bomb out
        // with an error.
        I_Error("Failed to open savegame file '%s' for writing.\n"
                "But your game has been saved to '%s' for recovery.",
                temp_savegame_file, recovery_savegame_file);
    }

    // Now rename the temporary savegame file to the actual savegame
    // file, overwriting the old savegame if there was one there.

    remove(savegame_file);
    rename(temp_savegame_file, savegame_file);

    gameaction = ga_nothing;
    M_StringCopy(savedescription, "", sizeof(savedescription));

	if( sim.extended_saves ) players[consoleplayer].message = "Limit-removing game saved.";
	else if( gameflags & GF_VanillaIncompatibleFlags ) players[consoleplayer].message = "Game saved without limit-removing data.";
	else players[consoleplayer].message = DEH_String(GGSAVED);

    // draw the pattern into the back screen
    R_FillBackScreen ();
}
 

//
// G_InitNew
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayer, playeringame[] should be set. 
//
skill_t			d_skill; 
mapinfo_t*		d_mapinfo = NULL;
gameflags_t		d_gameflags = GF_None;
 
void G_DeferedInitNew( skill_t	skill, mapinfo_t* mapinfo, gameflags_t flags) 
{ 
    d_skill = skill; 
	d_mapinfo = mapinfo;
	d_gameflags = flags;
    gameaction = ga_newgame; 

	if( demorecording && ( gameflags & GF_VanillaIncompatibleFlags ) )
	{
		I_Error( "G_DeferedInitNew: Vanilla-incompatible gameflags specified, this will break demos." );
	}
} 


void G_DoNewGame (void) 
{
    demoplayback = false; 
    netdemo = false;
    netgame = false;
    deathmatch = false;
    playeringame[1] = playeringame[2] = playeringame[3] = 0;
    respawnparm = false;
    fastparm = false;
    nomonsters = false;
    consoleplayer = 0;
    G_InitNew (d_skill, d_mapinfo, d_gameflags, 0); 
    gameaction = ga_nothing; 
} 


void G_InitNew( skill_t skill, mapinfo_t* mapinfo, gameflags_t flags, int32_t randomseed )
{
    const char *skytexturename;
    int             i;

	if( mapinfo )
	{
		D_GameflowSetCurrentMap( mapinfo );
	}

	if (paused)
	{
		paused = false;
		S_ResumeSound ();
	}

	skill = M_MIN( skill, sk_nightmare );

    M_ClearRandom ( randomseed );

	respawnmonsters = (skill == sk_nightmare || respawnparm );
	fastmonsters = (skill == sk_nightmare || fastparm );

#if 0 // FIX ME FIX ME FIX ME
	if (fastparm || (skill == sk_nightmare && gameskill != sk_nightmare) )
	{
		for (i=S_SARG_RUN1 ; i<=S_SARG_PAIN2 ; i++)
		{
			states[i].tics >>= 1;
		}
		mobjinfo[MT_BRUISERSHOT].speed = 20*FRACUNIT;
		mobjinfo[MT_HEADSHOT].speed = 20*FRACUNIT;
		mobjinfo[MT_TROOPSHOT].speed = 20*FRACUNIT;
	}
	else if (skill != sk_nightmare && gameskill == sk_nightmare)
	{
		for (i=S_SARG_RUN1 ; i<=S_SARG_PAIN2 ; i++)
		{
			states[i].tics <<= 1;
		}
		mobjinfo[MT_BRUISERSHOT].speed = 15*FRACUNIT;
		mobjinfo[MT_HEADSHOT].speed = 10*FRACUNIT;
		mobjinfo[MT_TROOPSHOT].speed = 10*FRACUNIT;
	}
#endif // 0 // FIX ME FIX ME FIX ME

	// force players to be initialized upon first level load
	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		players[i].playerstate = PST_REBORN;
	}

    usergame = true;                // will be set false if a demo
    paused = false;
    demoplayback = false;
    automapactive = false;
    viewactive = true;
    gameskill = skill;
	gameflags = flags;

    viewactive = true;

    // Set the sky to use.
    //
    // Note: This IS broken, but it is how Vanilla Doom behaves.
    // See http://doomwiki.org/wiki/Sky_never_changes_in_Doom_II.
    //
    // Because we set the sky here at the start of a game, not at the
    // start of a level, the sky texture never changes unless we
    // restore from a saved game.  This was fixed before the Doom
    // source release, but this IS the way Vanilla DOS Doom behaves.

	skytexturename = DEH_String( current_map->sky_texture.val );
	R_SetDefaultSky( skytexturename );

	G_DoLoadLevel();
}


//
// DEMO RECORDING 
// 
#define DEMOMARKER		0x80


void G_ReadDemoTiccmd (ticcmd_t* cmd) 
{ 
    if (*demo_p == DEMOMARKER) 
    {
	// end of demo data stream 
	G_CheckDemoStatus (); 
	return; 
    } 
    cmd->forwardmove = ((signed char)*demo_p++); 
    cmd->sidemove = ((signed char)*demo_p++); 

    // If this is a longtics demo, read back in higher resolution

    if (longtics)
    {
        cmd->angleturn = *demo_p++;
        cmd->angleturn |= (*demo_p++) << 8;
    }
    else
    {
        cmd->angleturn = ((unsigned char) *demo_p++)<<8; 
    }

    cmd->buttons = (unsigned char)*demo_p++; 
} 

// Increase the size of the demo buffer to allow unlimited demos

static void IncreaseDemoBuffer(void)
{
    int current_length;
    byte *new_demobuffer;
    byte *new_demop;
    int new_length;

    // Find the current size

    current_length = demoend - demobuffer;
    
    // Generate a new buffer twice the size
    new_length = current_length * 2;
    
    new_demobuffer = (byte*)Z_Malloc(new_length, PU_STATIC, 0);
    new_demop = new_demobuffer + (demo_p - demobuffer);

    // Copy over the old data

    memcpy(new_demobuffer, demobuffer, current_length);

    // Free the old buffer and point the demo pointers at the new buffer.

    Z_Free(demobuffer);

    demobuffer = new_demobuffer;
    demo_p = new_demop;
    demoend = demobuffer + new_length;
}

void G_WriteDemoTiccmd (ticcmd_t* cmd) 
{ 
    byte *demo_start;

    if (gamekeydown[key_demo_quit])           // press q to end demo recording 
	G_CheckDemoStatus (); 

    demo_start = demo_p;

    *demo_p++ = cmd->forwardmove; 
    *demo_p++ = cmd->sidemove; 

    // If this is a longtics demo, record in higher resolution
 
    if (longtics)
    {
        *demo_p++ = (cmd->angleturn & 0xff);
        *demo_p++ = (cmd->angleturn >> 8) & 0xff;
    }
    else
    {
        *demo_p++ = cmd->angleturn >> 8; 
    }

    *demo_p++ = cmd->buttons; 

    // reset demo pointer back
    demo_p = demo_start;

    if (demo_p > demoend - 16)
    {
        if (vanilla_demo_limit && !sim.extended_demos)
        {
            // no more space 
            G_CheckDemoStatus (); 
            return; 
        }
        else
        {
            // Vanilla demo limit disabled: unlimited
            // demo lengths!

            IncreaseDemoBuffer();
        }
    } 
	
    G_ReadDemoTiccmd (cmd);         // make SURE it is exactly the same 
} 
 
 
 
//
// G_RecordDemo
//
void G_RecordDemo (const char *name)
{
    size_t demoname_size;
    int i;
    int maxsize;

    usergame = false;
    demoname_size = strlen(name) + 5;
    demoname = (char*)Z_Malloc(demoname_size, PU_STATIC, NULL);
    M_snprintf(demoname, demoname_size, "%s.lmp", name);
    maxsize = 0x20000;

    //!
    // @arg <size>
    // @category demo
    // @vanilla
    //
    // Specify the demo buffer size (KiB)
    //

    i = M_CheckParmWithArgs("-maxdemo", 1);
    if (i)
	maxsize = atoi(myargv[i+1])*1024;
    demobuffer = (byte*)Z_Malloc (maxsize,PU_STATIC,NULL); 
    demoend = demobuffer + maxsize;
	
    demorecording = true; 
	if( !!M_CheckParmWithArgs( "-recordaudit", 1 ) )
	{
		G_InitAuditBufferRecording( AUDITBUFFERDEFAULTSIZE );
		auditname = (char*)Z_Malloc( demoname_size, PU_STATIC, NULL );
		M_snprintf( auditname, demoname_size, "%s.aud", name );
	}
}

// Get the demo version code appropriate for the version set in gameversion.
demoversion_t G_VanillaVersionCode(void)
{
    switch (gameversion)
    {
        case exe_doom_1_666:
            return demo_doom_1_666;
        case exe_doom_1_7:
            return demo_doom_1_7;
        case exe_doom_1_8:
            return demo_doom_1_8;
        case exe_doom_1_9:
        default:  // All other versions are variants on v1.9:
            return demo_doom_1_9;
    }
}

void G_BeginRecording (void) 
{ 
    int             i; 

    demo_p = demobuffer;

    //!
    // @category demo
    //
    // Record a high resolution "Doom 1.91" demo.
    //

    longtics = D_NonVanillaRecord(M_ParmExists("-longtics"),
                                  "Doom 1.91 demo format");

    // If not recording a longtics demo, record in low res
    lowres_turn = !longtics;

    if (longtics)
    {
        *demo_p++ = DOOM_191_VERSION;
    }
    else if (gameversion > exe_doom_1_2)
    {
        *demo_p++ = G_VanillaVersionCode();
    }

    *demo_p++ = gameskill; 
    *demo_p++ = current_episode->episode_num; 
    *demo_p++ = current_map->map_num; 
    if (longtics || gameversion > exe_doom_1_2)
    {
        *demo_p++ = deathmatch; 
        *demo_p++ = respawnparm;
        *demo_p++ = fastparm;
        *demo_p++ = nomonsters;
        *demo_p++ = consoleplayer;
    }
	 
    for (i=0 ; i<MAXPLAYERS ; i++) 
	*demo_p++ = playeringame[i]; 		 
} 
 

//
// G_PlayDemo 
//

static const char *defdemoname;
 
void G_DeferedPlayDemo(const char *name)
{ 
    defdemoname = name; 
    gameaction = ga_playdemo; 
} 

// Generate a string describing a demo version

static const char *DemoVersionDescription(int version)
{
    static char resultbuf[16];

    switch (version)
    {
        case 104:
            return "v1.4";
        case 105:
            return "v1.5";
        case 106:
            return "v1.6/v1.666";
        case 107:
            return "v1.7/v1.7a";
        case 108:
            return "v1.8";
        case 109:
            return "v1.9";
        case 111:
            return "v1.91 hack demo?";
        default:
            break;
    }

    // Unknown version.  Perhaps this is a pre-v1.4 IWAD?  If the version
    // byte is in the range 0-4 then it can be a v1.0-v1.2 demo.

    if (version >= 0 && version <= 4)
    {
        return "v1.0/v1.1/v1.2";
    }
    else
    {
        M_snprintf(resultbuf, sizeof(resultbuf),
                   "%i.%i (unknown)", version / 100, version % 100);
        return resultbuf;
    }
}

void G_DoPlayDemo (void)
{
    int i, lumpnum;
    int demoversion;
    doombool olddemo = false;

    lumpnum = W_GetNumForName(defdemoname);
    gameaction = ga_nothing;
    demobuffer = (byte*)W_CacheLumpNum(lumpnum, PU_STATIC);
    demo_p = demobuffer;

    demoversion = *demo_p++;

    if (demoversion >= 0 && demoversion <= 4)
    {
        olddemo = true;
        demo_p--;
    }

	// Using https://www.doomworld.com/forum/topic/72033-boom-mbf-demo-header-format/ as documentation
	doombool boomdemo = sim.extended_demos
					&& ( demoversion == demo_boom_2_02 || demoversion == demo_mbf || demoversion == demo_complevel11 );
	if( boomdemo )
	{
		const byte boomsig[6] = { 0x1D, 'B', 'o', 'o', 'm', 0xE6 };
		const byte mbfsig[6] = { 0x1D, 'M', 'B', 'F', 0xE6, 0x00 };
		byte sig[6] = { demo_p[0], demo_p[1], demo_p[2], demo_p[3], demo_p[4], demo_p[5] };
		demo_p += 6;

		doombool isboom = memcmp( (void*)sig, (void*)boomsig, 6 ) == 0;
		doombool ismbf = memcmp( (void*)sig, (void*)mbfsig, 6 ) == 0;

		if( !isboom && !ismbf )
		{
			I_Error( "Malformed Boom demo encountered" );
		}

		longtics = demoversion == demo_complevel11;

		int32_t compatibility = *demo_p++;
	}

    // Longtics demos use the modified format that is generated by cph's
    // hacked "v1.91" doom exe. This is a non-vanilla extension.
    if( D_NonVanillaPlayback(demoversion == DOOM_191_VERSION, lumpnum,
							"Doom 1.91 demo format"))
    {
        longtics = true;
    }
    else if ( !boomdemo && demoversion != G_VanillaVersionCode() &&
             !(gameversion <= exe_doom_1_2 && olddemo))
    {
        const char *message = "Demo is from a different game version!\n"
                              "(read %i, should be %i)\n"
                              "\n"
                              "*** You may need to upgrade your version "
                                  "of Doom to v1.9. ***\n"
                              "    See: https://www.doomworld.com/classicdoom"
                                        "/info/patches.php\n"
                              "    This appears to be %s.";

        I_Error(message, demoversion, G_VanillaVersionCode(),
                         DemoVersionDescription(demoversion));
    }

	skill_t skill = (skill_t)*demo_p++;
	int32_t episode = *demo_p++;
	int32_t map = *demo_p++;
	int32_t random_seed = 0;
	episodeinfo_t* epinfo = D_GameflowGetEpisode( episode );
	mapinfo_t* mapinfo = D_GameflowGetMap( epinfo, map );
    if (!olddemo)
    {
		if( boomdemo )
		{
			deathmatch = *demo_p++;
			consoleplayer = *demo_p++;

			int32_t monsters_remember = *demo_p++;
			int32_t variable_friction = *demo_p++;
			int32_t weapon_recoil = *demo_p++;
			int32_t allow_pushers = *demo_p++;
			int32_t unused = *demo_p++;
			int32_t player_bobbing = *demo_p++;

			respawnparm = *demo_p++;
			fastparm = *demo_p++;
			nomonsters = *demo_p++;

			int32_t insurance = *demo_p++;
			random_seed = *demo_p++;
			random_seed |= (*demo_p++ << 8);
			random_seed |= (*demo_p++ << 16);
			random_seed |= (*demo_p++ << 24);

			demo_p += 50;
		}
		else
		{
			deathmatch = *demo_p++;
			respawnparm = *demo_p++;
			fastparm = *demo_p++;
			nomonsters = *demo_p++;
			consoleplayer = *demo_p++;
		}
    }
    else
    {
        deathmatch = 0;
        respawnparm = 0;
        fastparm = 0;
        nomonsters = 0;
        consoleplayer = 0;
    }
	
        
    for (i=0 ; i<MAXPLAYERS ; i++)
	{
		playeringame[i] = *demo_p++; 
	}

	if( boomdemo )
	{
		demo_p += 28;
	}

	if (playeringame[1] || M_CheckParm("-solo-net") > 0
						|| M_CheckParm("-netdemo") > 0)
	{
		netgame = true;
		netdemo = true;
		solonetgame = M_CheckParm("-solo-net") > 0 && !playeringame[1];
	}

	int32_t auditparam = M_CheckParmWithArgs( "-playaudit", 1 );
	if( auditparam )
	{
		G_InitAuditBufferPlaying( myargv[ auditparam + 1 ] );
	}
	else
	{
		auditparam = M_CheckParmWithArgs( "-auditdemo", 1 );
		if( auditparam )
		{
			G_InitAuditBufferRecording( AUDITBUFFERDEFAULTSIZE );
			int32_t demoname_size = strlen(defdemoname) + 5;
			auditname = (char*)Z_Malloc( demoname_size, PU_STATIC, NULL );
			M_snprintf( auditname, demoname_size, "%s.aud", defdemoname );
		}
	}

    G_InitNew (skill, mapinfo, GF_None, random_seed); 
    starttime = I_GetTimeTicks(); 

    usergame = false; 
    demoplayback = true; 
} 

//
// G_TimeDemo 
//
void G_TimeDemo (char* name) 
{
    //!
    // @category video
    // @vanilla
    //
    // Disable rendering the screen entirely.
    //

    nodrawers = M_CheckParm ("-nodraw");

    timingdemo = true; 
    singletics = true; 

    defdemoname = name; 
    gameaction = ga_playdemo; 
} 

/* 
=================== 
= 
= G_CheckDemoStatus 
= 
= Called after a death or level completion to allow demos to be cleaned up 
= Returns true if a new demo loop action will take place 
=================== 
*/ 
 
doombool G_CheckDemoStatus (void) 
{ 
	uint64_t             endtime; 
	 
	if (timingdemo) 
	{ 
		float fps;
		int realtics;

		endtime = I_GetTimeTicks(); 
		realtics = endtime - starttime;
		fps = ((float) gametic * TICRATE) / realtics;

		// Prevent recursive calls
		timingdemo = false;
		demoplayback = false;

		I_Error ("timed %i gametics in %i realtics (%f fps)",
					gametic, realtics, fps);
	} 
	 
	if (demoplayback) 
	{ 
		W_ReleaseLumpName(defdemoname);
		demoplayback = false; 
		netdemo = false;
		netgame = false;
		deathmatch = false;
		playeringame[1] = playeringame[2] = playeringame[3] = 0;
		respawnparm = false;
		fastparm = false;
		nomonsters = false;
		consoleplayer = 0;

		if (singledemo) 
			I_Quit (); 
		else 
			D_AdvanceDemo (); 

		return true; 
	} 
 
	if (demorecording) 
	{ 
		*demo_p++ = DEMOMARKER; 
		M_WriteFile (demoname, demobuffer, demo_p - demobuffer); 
		Z_Free (demobuffer); 
		demorecording = false; 

		if( !auditrecording )
		{
			I_Error ("Demo %s recorded",demoname);
		}
	} 

	if( auditrecording )
	{
		M_WriteFile( auditname, auditbuffer, auditbufferpos - auditbuffer ); 
		Z_Free( auditbuffer ); 
		I_Error( "Audit %s recorded", auditname ); 
	}

	return false; 
} 
 
 
 
