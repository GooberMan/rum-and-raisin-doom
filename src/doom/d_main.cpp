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
//	DOOM main program (D_DoomMain) and game loop (D_DoomLoop),
//	plus functions to determine game mode (shareware, registered),
//	parse command line parameters, configure game parameters (turbo),
//	and call the startup functions.
//


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "deh_main.h"
#include "doomdef.h"
#include "doomstat.h"

#include "dstrings.h"
#include "sounds.h"

#include "d_demoloop.h"
#include "d_gameconf.h"
#include "d_gamesim.h"
#include "d_iwad.h"

#include "z_zone.h"
#include "w_main.h"
#include "w_wad.h"
#include "w_merge.h"
#include "s_sound.h"
#include "v_diskicon.h"
#include "v_video.h"

#include "f_finale.h"
#include "f_wipe.h"

#include "m_argv.h"
#include "m_config.h"
#include "m_controls.h"
#include "m_misc.h"
#include "m_menu.h"
#include "m_dashboard.h"
#include "m_launcher.h"
#include "m_profile.h"
#include "m_url.h"

#include "p_saveg.h"
#include "p_local.h"

#include "i_endoom.h"
#include "i_input.h"
#include "i_joystick.h"
#include "i_log.h"
#include "i_system.h"
#include "i_terminal.h"
#include "i_thread.h"
#include "i_timer.h"
#include "i_video.h"

#include "g_game.h"

#include "hu_stuff.h"
#include "wi_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "net_client.h"
#include "net_dedicated.h"
#include "net_query.h"

#include "p_setup.h"
#include "r_local.h"


#include "d_main.h"

//
// D-DoomLoop()
// Not a globally visible function,
//  just included for source reference,
//  called by D_DoomMain, never exits.
// Manages timing and IO,
//  calls all ?_Responder, ?_Ticker, and ?_Drawer,
//  calls I_GetTime, I_StartFrame, and I_StartTic
//
void D_DoomLoop (void);

std::unique_ptr< JobSystem > jobs;

extern "C"
{
	static char *gamedescription;

	// Location where savegames are stored

	char *          savegamedir;

	// location of IWAD and WAD files

	doombool		devparm;	// started game with -devparm
	doombool         nomonsters;	// checkparm of -nomonsters
	doombool         respawnparm;	// checkparm of -respawn
	doombool         fastparm;	// checkparm of -fast

	//extern int soundVolume;
	//extern  int	sfxVolume;
	//extern  int	musicVolume;

	extern  doombool	inhelpscreens;

	skill_t		startskill;
	int             startepisode;
	int		startmap;
	doombool		autostart;
	int             startloadgame;

	doombool		advancedemo;

	// Store demo, do not accept any inputs
	doombool         storedemo;

	// If true, the main game loop has started.
	doombool         main_loop_started = false;

	char		wadfile[1024];		// primary wad file
	char		mapdir[1024];           // directory of development maps

	int32_t				show_endoom = 1;
	int32_t				show_text_startup = 1;
	int32_t				show_diskicon = Disk_ByCommandLine;
	int32_t				remove_limits = 1;
	int32_t				window_close_behavior = WindowClose_Ask;

	extern int32_t		enable_frame_interpolation;
	extern int32_t		maxrendercontexts;
	extern int32_t		num_render_contexts;
	extern int32_t		num_software_backbuffers;
	extern int32_t		additional_light_boost;
	extern int32_t		vertical_fov_degrees;
	extern int32_t		stats_style;
	extern doombool		rendersplitvisualise;

	doombool refreshstatusbar = true;

	vbuffer_t blackedges;
	#define BLACKEDGE_VAL 0x00
	byte blackedgesdata[ 128 ] =
	{
		BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL,
		BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL,
		BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL,
		BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL,
		BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL,
		BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL,
		BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL,
		BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL,
		BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL,
		BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL,
		BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL,
		BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL,
		BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL,
		BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL,
		BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL,
		BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL, BLACKEDGE_VAL,
	};

	gamestate_t     wipegamestate = GS_DEMOSCREEN;
	extern  doombool setsizeneeded;
	extern  int             showMessages;

	const char* reasons[] =
	{
		"level",
		"intermission",
		"finale",
		"demo",
	};

	extern int32_t voidcleartype;

	extern doombool		auditplaybackerror;

	uint64_t frametime = 0;
	uint64_t frametime_withoutpresent = 0;

	int32_t wipe_style = wipe_Melt;

	extern uint64_t synctime;

	int             demosequence;
	int             pagetic;
	const char                    *pagename;

	extern int forwardmove[2];
	extern int sidemove[2];

	demoloop_t*		demoloop;
}

const char* IWADFilename()
{
	if( !gameconf || !gameconf->iwad ) return "unknown.wad";

	const char* name = gameconf->iwad + strlen( gameconf->iwad );
	while( --name != gameconf->iwad )
	{
		if( *name == DIR_SEPARATOR )
		{
			++name;
			break;
		}
	}

	return name;
}

DOOM_C_API void D_ConnectNetGame(void);
DOOM_C_API void D_CheckNetGame(void);


//
// D_ProcessEvents
// Send all the events of the given timestamp down the responder chain
//
void D_ProcessEvents (void)
{
    event_t*	ev;
	
    // IF STORE DEMO, DO NOT ACCEPT INPUT
    if (storedemo)
        return;

    while ((ev = D_PopEvent()) != NULL)
    {
		if (M_Responder (ev))
			continue;               // menu ate the event
		G_Responder (ev);
    }
}




//
// D_Display
//  draw current display, possibly wiping it from the previous
//

// wipegamestate can be set to -1 to force a wipe on the next draw

void D_TestControls( const char* itemname, void* data )
{
	V_DrawMouseSpeedBox( testcontrols_mousespeed );
}

doombool D_Display( double_t framepercent )
{
    static  doombool		viewactivestate = false;
    static  doombool		menuactivestate = false;
    static  doombool		inhelpscreensstate = false;
    static  doombool		fullscreen = false;
    static  gamestate_t		oldgamestate = GS_INVALID;
    static  int			borderdrawcount;
    int				y;
    doombool			wipe;
    doombool			redrawsbar;
	patch_t*		pausepatch;

	doombool ispaused = renderpaused = ( paused
										|| auditplaybackerror
										|| 	( gamestate == GS_LEVEL && !demoplayback && dashboardactive && dashboardpausesplaysim && ( solonetgame || !netgame ) ) );
	renderpaused |= ( menuactive && !demoplayback );

	M_PROFILE_PUSH( __FUNCTION__, __FILE__, __LINE__ );

	redrawsbar = refreshstatusbar || voidcleartype != Void_NoClear;
	refreshstatusbar = false;

	// change the view size if needed
	if (setsizeneeded)
	{
		R_ExecuteSetViewSize( );
		oldgamestate = GS_INVALID; // force background redraw
		borderdrawcount = 3;
	}
	else
	{
		R_RenderUpdateFrameSize();
	}

	// save the current screen if about to wipe
	if (gamestate != wipegamestate)
	{
		wipe = true;
		wipe_StartScreen(0, 0, render_width, render_height);
	}
	else
	{
		wipe = false;
	}

	if (gamestate == GS_LEVEL && gametic)
	{
		HU_Erase();
	}

	// do buffered drawing
	switch (gamestate)
	{
	case GS_LEVEL:
		if (!gametic)
			break;
		if (automapactive)
		{
			AM_Drawer ();
			// Slight hack to get around the fact that we memset everything now for the automap
			redrawsbar = true;
		}
		if (wipe || (drs_current->viewheight != drs_current->frame_height && fullscreen))
			redrawsbar = true;
		if (inhelpscreensstate && !inhelpscreens)
			redrawsbar = true;              // just put away the help screen
		fullscreen = drs_current->viewheight == drs_current->frame_height;
		break;

	case GS_INTERMISSION:
		WI_Drawer ();
		break;

	case GS_FINALE:
		F_Drawer ();
		break;

	case GS_DEMOSCREEN:
		D_PageDrawer ();
		break;

	default:
		break;
	}

	// draw the view directly
	if (gamestate == GS_LEVEL && gametic)
	{
		if( !automapactive )
		{
			R_RenderPlayerView( &players[ displayplayer ], framepercent, displayplayer == consoleplayer );
		}
		ST_Drawer ( fullscreen, redrawsbar );
		HU_Drawer ();
	}

	// clean up border stuff
	if (gamestate != oldgamestate && gamestate != GS_LEVEL)
	{
		I_SetPalette( (byte*)W_CacheLumpName(DEH_String("PLAYPAL"),PU_CACHE));
	}

	// see if the border needs to be initially drawn
	if (gamestate == GS_LEVEL && oldgamestate != GS_LEVEL)
	{
		viewactivestate = false;        // view was not active
		R_FillBackScreen ();    // draw the pattern into the back screen
	}

	// see if the border needs to be updated to the screen
	if (gamestate == GS_LEVEL && !automapactive && drs_current->scaledviewwidth != drs_current->frame_width)
	{
		if (menuactive || menuactivestate || !viewactivestate)
			borderdrawcount = 3;
		if (borderdrawcount)
		{
			R_DrawViewBorder ();    // erase old menu stuff
			borderdrawcount--;
		}

	}

	menuactivestate = menuactive;
	viewactivestate = viewactive;
	inhelpscreensstate = inhelpscreens;
	oldgamestate = wipegamestate = gamestate;
    
	// draw pause pic
	if (ispaused)
	{
		if (automapactive)
		{
			y = 4;
		}
		else
		{
			y = FixedDiv( drs_current->viewwindowy, V_HEIGHTMULTIPLIER ) + 4;
		}

		pausepatch = (patch_t*)W_CacheLumpName( DEH_String("M_PAUSE"), PU_CACHE );

		V_DrawPatchDirect( ( VANILLA_SCREENWIDTH - pausepatch->width ) / 2, y, pausepatch );
	}


	// menus go directly to the screen
	M_Drawer (); // menu is drawn even on top of everything

	//NetUpdate (); // send out any new accumulation

	if( wipe )
	{
		wipe_EndScreen(0, 0, render_width, render_height);
	}

	M_PROFILE_POP( __FUNCTION__ );

	return wipe;
}

void D_SetupLoadingDisk( int32_t disk_icon_style )
{
	const char *disk_lump_name = NULL;

	if( disk_icon_style != Disk_Off )
	{
		switch( disk_icon_style )
		{
		case Disk_ByCommandLine:
			if (M_CheckParm("-cdrom") > 0)
			{
				disk_lump_name = DEH_String( "STCDROM" );
			}
			else
			{
				disk_lump_name = DEH_String( "STDISK" );
			}
			break;

		case Disk_Floppy:
			disk_lump_name = DEH_String( "STDISK" );
			break;

		case Disk_CD:
			disk_lump_name = DEH_String( "STCDROM" );
			if( W_CheckNumForName( disk_lump_name ) < 0 )
			{
				disk_lump_name = DEH_String( "STDISK" );
			}
			break;
		}
	}

	show_diskicon = disk_icon_style;
	V_EnableLoadingDisk(disk_lump_name,
						V_VIRTUALWIDTH - LOADING_DISK_W,
						V_VIRTUALHEIGHT - LOADING_DISK_H);
}

//
// Add configuration file variable bindings.
//


static const char * const chat_macro_defaults[10] =
{
    HUSTR_CHATMACRO0,
    HUSTR_CHATMACRO1,
    HUSTR_CHATMACRO2,
    HUSTR_CHATMACRO3,
    HUSTR_CHATMACRO4,
    HUSTR_CHATMACRO5,
    HUSTR_CHATMACRO6,
    HUSTR_CHATMACRO7,
    HUSTR_CHATMACRO8,
    HUSTR_CHATMACRO9
};


void D_BindVariables(void)
{
    int i;

    M_ApplyPlatformDefaults();

    I_BindInputVariables();
    I_BindVideoVariables();
    I_BindJoystickVariables();
    I_BindSoundVariables();

    M_BindBaseControls();
    M_BindWeaponControls();
    M_BindMapControls();
    M_BindMenuControls();
    M_BindChatControls(MAXPLAYERS);
	M_BindDashboardVariables();

	R_BindRenderVariables();

	AM_BindAutomapVariables();

    key_multi_msgplayer[0] = HUSTR_KEYGREEN;
    key_multi_msgplayer[1] = HUSTR_KEYINDIGO;
    key_multi_msgplayer[2] = HUSTR_KEYBROWN;
    key_multi_msgplayer[3] = HUSTR_KEYRED;

    NET_BindVariables();

    M_BindIntVariable("mouse_sensitivity",      &mouseSensitivity);
    M_BindIntVariable("sfx_volume",             &sfxVolume);
    M_BindIntVariable("music_volume",           &musicVolume);
    M_BindIntVariable("show_messages",          &showMessages);
    M_BindIntVariable("screenblocks",           &screenblocks);
    M_BindIntVariable("detaillevel",            &detailLevel);
    M_BindIntVariable("snd_channels",           &snd_channels);
    M_BindIntVariable("vanilla_savegame_limit", &vanilla_savegame_limit);
    M_BindIntVariable("vanilla_demo_limit",     &vanilla_demo_limit);
    M_BindIntVariable("show_endoom",            &show_endoom);
	M_BindIntVariable("show_text_startup",		&show_text_startup);
    M_BindIntVariable("show_diskicon",          &show_diskicon);
	M_BindIntVariable("window_close_behavior",	&window_close_behavior );

	M_BindIntVariable("stats_style",			&stats_style );

    // Multiplayer chat macros

    for (i=0; i<10; ++i)
    {
        char buf[12];

        chat_macros[i] = M_StringDuplicate(chat_macro_defaults[i]);
        M_snprintf(buf, sizeof(buf), "chatmacro%i", i);
        M_BindStringVariable(buf, &chat_macros[i]);
    }
}

//
// D_GrabMouseCallback
//
// Called to determine whether to grab the mouse pointer
//

doombool D_GrabMouseCallback(void)
{
    // Drone players don't need mouse focus

    if (drone)
        return false;

    // when menu is active or game is paused, release the mouse 
 
    if (menuactive || paused)
        return false;

    // only grab mouse when playing levels (but not demos)

    return (gamestate == GS_LEVEL) && !demoplayback && !advancedemo;
}

//
//  D_RunFrame
//

double_t CalculatePercentage()
{
	// There's a bug here. Because calculating the next tick still gets latest,
	// which is just enough to kick it over to the next tick and invalidate this
	// percentage
	double_t	currmicroseconds = fmod( synctime, 1000000.0 );
	double_t	currtickbase = floor( ( currmicroseconds / 1000000.0 ) * 35.0 );
	double_t	nexttickbase = currtickbase + 1;
	double_t	currtick = ( currtickbase * 1000000.0 ) / 35.0;
	double_t	nexttick = ( nexttickbase * 1000000.0 ) / 35.0;

	return ( currmicroseconds - currtick ) / ( nexttick - currtick );

	// This code gives negative percentages, so we need to increment percentage by
	// a frame if our closest frame is the one before. But we need to get monitor
	// refresh rate to do that correctly.
	//double_t	currframebase = floor( ( currmicroseconds / 1000000.0 ) * 60.0 );
	//double_t	currframe = ( currframebase * 1000000.0 ) / 60.0;
	//currpercentage = ( currframe - currtick ) / ( nexttick - currtick );
}

void D_RunFrame()
{
	uint64_t nowtime = 0;
	uint64_t tics = 0;
	static uint64_t wipestart = 0;
	static doombool wipe = false;

	int32_t prev_render_width = render_width;
	int32_t prev_render_height = render_height;
	int32_t prev_render_post_scaling = render_post_scaling;

	uint64_t start = I_GetTimeUS();
	M_ProfileNewFrame();
	M_PROFILE_PUSH( __FUNCTION__, __FILE__, __LINE__ );

	double_t currpercentage = 0.0;

	// frame syncronous IO operations
	I_StartFrame();
	I_StartTic();

	jobs->SetMaxJobs( num_render_contexts - 1 );
	jobs->NewProfileFrame();

	if (wipe)
	{
		nowtime = I_GetTimeTicks();
		tics = nowtime - wipestart;

		synctime = I_GetTimeUS();

		wipe = !wipe_ScreenWipe(wipe_style, 0, 0, render_width, render_height, tics, (rend_fixed_t)( CalculatePercentage() * ( RENDFRACUNIT ) ) );
		wipestart = nowtime;

		M_Drawer ();                            // menu is drawn even on top of wipes
	}
	else
	{
		// TODO: Callbacks or something
		if( prev_render_width != render_width || prev_render_height != render_height || prev_render_post_scaling != render_post_scaling )
		{
			R_RenderDimensionsChanged();
		}

		TryRunTics (); // will run at least one tic

		currpercentage = CalculatePercentage();

		S_UpdateSounds (players[consoleplayer].mo);// move positional sounds

		// Update display, next frame, with current state if no profiling is on
		if (screenvisible && !nodrawers)
		{
			if( (wipe = D_Display( currpercentage ) ) )
			{
				wipestart = I_GetTimeTicks() - 1;
			}
		}
	}

	M_PROFILE_POP( __FUNCTION__ );

	frametime_withoutpresent = I_GetTimeUS() - start;

	I_TerminalRender();

	I_FinishUpdate( NULL );

	frametime = I_GetTimeUS() - start;
}

//
//  D_DoomLoop
//
void D_DoomLoop (void)
{
	if (gamevariant == bfgedition &&
		(demorecording || (gameaction == ga_playdemo) || netgame))
	{
		I_TerminalPrintf( Log_Warning,	" WARNING: You are playing using one of the Doom Classic\n"
										" IWAD files shipped with the Doom 3: BFG Edition. These are\n"
										" known to be incompatible with the regular IWAD files and\n"
										" may cause demos and network games to get out of sync.\n"	);
	}

	if (demorecording)
	{
		G_BeginRecording ();
	}

    main_loop_started = true;

    I_SetWindowTitle(gamedescription);
    I_SetGrabMouseCallback(D_GrabMouseCallback);
    I_InitBuffers( num_software_backbuffers );
	R_InitContexts();

    D_SetupLoadingDisk( show_diskicon );

    TryRunTics();

    V_RestoreBuffer();

    D_StartGameLoop();

    if (testcontrols)
    {
        wipegamestate = gamestate;
    }

	I_TerminalSetMode( TM_None );

    while (1)
    {
        D_RunFrame();
    }
}



//
//  DEMO LOOP
//

//
// D_PageTicker
// Handles timing for warped projection
//
void D_PageTicker (void)
{
    if (--pagetic < 0)
	D_AdvanceDemo ();
}



//
// D_PageDrawer
//
void D_PageDrawer (void)
{
	lumpindex_t pagenum = W_GetNumForNameExcluding( pagename, comp.widescreen_assets ? wt_none : wt_widepix );
	patch_t* patch = (patch_t*)W_CacheLumpNum( pagenum, PU_CACHE );
	// WIDESCREEN HACK
	int32_t xpos = -( ( patch->width - V_VIRTUALWIDTH ) / 2 );

	V_FillBorder( &blackedges, 0, V_VIRTUALHEIGHT );
	V_DrawPatch ( xpos, 0, patch );
}


//
// D_AdvanceDemo
// Called after each demo or intro demosequence finishes
//
void D_AdvanceDemo (void)
{
    advancedemo = true;
}


//
// This cycles through the demo sequences.
// FIXME - version dependend demo numbers?
//
void D_DoAdvanceDemo (void)
{
    players[consoleplayer].playerstate = PST_LIVE;  // not reborn
    advancedemo = false;
    usergame = false;               // no save / end game here
    paused = false;
    gameaction = ga_nothing;

	demoloop->Advance();
	switch( demoloop->current->type )
	{
	case dlt_artscreen:
		gamestate = GS_DEMOSCREEN;
		pagename = DEH_String( demoloop->current->primarylumpname );
		pagetic = demoloop->current->tics;
		if( demoloop->current->secondarylumpname[0] != 0 )
		{
			lumpindex_t music = W_GetNumForName( DEH_String( demoloop->current->secondarylumpname ) );
			S_ChangeMusicLumpIndex( music, false );
		}
		break;

	case dlt_demo:
		G_DeferedPlayDemo( DEH_String( demoloop->current->primarylumpname ) );
		break;

	default:
		I_Error( "D_DoAdvanceDemo: Demo loop has a bad entry" );
		break;
	}

#if 0
    // The Ultimate Doom executable changed the demo sequence to add
    // a DEMO4 demo.  Final Doom was based on Ultimate, so also
    // includes this change; however, the Final Doom IWADs do not
    // include a DEMO4 lump, so the game bombs out with an error
    // when it reaches this point in the demo sequence.

    // However! There is an alternate version of Final Doom that
    // includes a fixed executable.

	if( comp.demo4 )
	{
		demosequence = (demosequence+1)%7;
	}
	else if( gameversion == exe_ultimate )
	{
		demosequence = (demosequence+1)%7;
	}
	else if( gameversion == exe_final)
	{
		if( W_CheckNumForName( DEH_String("demo4") ) == -1 )
		{
			demosequence = (demosequence+1)%6;
		}
		else
		{
			demosequence = (demosequence+1)%7;
		}
	}
	else
	{
		demosequence = (demosequence+1)%6;
	}
    
    switch (demosequence)
    {
      case 0:
	if ( gamemode == commercial )
	    pagetic = TICRATE * 11;
	else
	    pagetic = 170;
	gamestate = GS_DEMOSCREEN;
	pagename = DEH_String("TITLEPIC");
	if ( gamemode == commercial )
	  S_StartMusic(mus_dm2ttl);
	else
	  S_StartMusic (mus_intro);
	break;
      case 1:
	G_DeferedPlayDemo(DEH_String("demo1"));
	break;
      case 2:
	pagetic = 200;
	gamestate = GS_DEMOSCREEN;
	pagename = DEH_String("CREDIT");
	break;
      case 3:
	G_DeferedPlayDemo(DEH_String("demo2"));
	break;
      case 4:
	gamestate = GS_DEMOSCREEN;
	if ( gamemode == commercial)
	{
	    pagetic = TICRATE * 11;
	    pagename = DEH_String("TITLEPIC");
	    S_StartMusic(mus_dm2ttl);
	}
	else
	{
	    pagetic = 200;

	    if (gameversion >= exe_ultimate)
	      pagename = DEH_String("CREDIT");
	    else
	      pagename = DEH_String("HELP2");
	}
	break;
      case 5:
	G_DeferedPlayDemo(DEH_String("demo3"));
	break;
        // THE DEFINITIVE DOOM Special Edition demo
      case 6:
	G_DeferedPlayDemo(DEH_String("demo4"));
	break;
    }

    // The Doom 3: BFG Edition version of doom2.wad does not have a
    // TITLETPIC lump. Use INTERPIC instead as a workaround.
    if(!strcasecmp(pagename, DEH_String("TITLEPIC"))
        && W_CheckNumForName( DEH_String("TITLEPIC") ) < 0)
    {
		lumpindex_t dmenupic = W_CheckNumForName( "DMENUPIC" );
		if( gamevariant == bfgedition )
		{
			pagename = DEH_String( "INTERPIC" );
		}
		else if( dmenupic >= 0 )
		{
			pagename = "DMENUPIC";
		}
    }
#endif
}



//
// D_StartTitle
//
void D_StartTitle (void)
{
    gameaction = ga_nothing;
    demosequence = -1;
	demoloop->Reset();
    D_AdvanceDemo ();
}

// Strings for dehacked replacements of the startup banner
//
// These are from the original source: some of them are perhaps
// not used in any dehacked patches

static const char *banners[] =
{
    // doom2.wad
    "                         "
    "DOOM 2: Hell on Earth v%i.%i"
    "                           ",
    // doom2.wad v1.666
    "                         "
    "DOOM 2: Hell on Earth v%i.%i66"
    "                          ",
    // doom1.wad
    "                            "
    "DOOM Shareware Startup v%i.%i"
    "                           ",
    // doom.wad
    "                            "
    "DOOM Registered Startup v%i.%i"
    "                           ",
    // Registered DOOM uses this
    "                          "
    "DOOM System Startup v%i.%i"
    "                          ",
    // Doom v1.666
    "                          "
    "DOOM System Startup v%i.%i66"
    "                          "
    // doom.wad (Ultimate DOOM)
    "                         "
    "The Ultimate DOOM Startup v%i.%i"
    "                        ",
    // tnt.wad
    "                     "
    "DOOM 2: TNT - Evilution v%i.%i"
    "                           ",
    // plutonia.wad
    "                   "
    "DOOM 2: Plutonia Experiment v%i.%i"
    "                           ",
};

//
// Get game name: if the startup banner has been replaced, use that.
// Otherwise, use the name given
// 

static char *GetGameName(const char *gamename)
{
    size_t i;

    for (i=0; i<arrlen(banners); ++i)
    {
        const char *deh_sub;
        // Has the banner been replaced?

        deh_sub = DEH_String(banners[i]);

        if (deh_sub != banners[i])
        {
            size_t gamename_size;
            int version;
            char *deh_gamename;

            // Has been replaced.
            // We need to expand via printf to include the Doom version number
            // We also need to cut off spaces to get the basic name

            gamename_size = strlen(deh_sub) + 10;
            deh_gamename = (char*)malloc(gamename_size);
            if (deh_gamename == NULL)
            {
                I_Error("GetGameName: Failed to allocate new string");
            }
            version = G_VanillaVersionCode();
            DEH_snprintf(deh_gamename, gamename_size, banners[i],
                         version / 100, version % 100);

            while (deh_gamename[0] != '\0' && isspace(deh_gamename[0]))
            {
                memmove(deh_gamename, deh_gamename + 1, gamename_size - 1);
            }

            while (deh_gamename[0] != '\0' && isspace(deh_gamename[strlen(deh_gamename)-1]))
            {
                deh_gamename[strlen(deh_gamename) - 1] = '\0';
            }

            return deh_gamename;
        }
    }

    return M_StringDuplicate(gamename);
}

static void SetMissionForPackName(const char *pack_name)
{
    int i;
    static const struct
    {
        const char *name;
        GameMission_t mission;
    } packs[] = {
        { "doom2",    doom2 },
        { "tnt",      pack_tnt },
        { "plutonia", pack_plut },
    };

    for (i = 0; i < arrlen(packs); ++i)
    {
        if (!strcasecmp(pack_name, packs[i].name))
        {
            gamemission = packs[i].mission;
            return;
        }
    }

    I_TerminalPrintf( Log_Error, "Valid mission packs are:\n" );

    for (i = 0; i < arrlen(packs); ++i)
    {
        I_TerminalPrintf( Log_Error, "\t%s\n", packs[i].name );
    }

    I_Error("Unknown mission pack name: %s", pack_name);
}

//
// Find out what version of Doom is playing.
//

void D_IdentifyVersion(void)
{
    // gamemission is set up by the D_FindIWAD function.  But if 
    // we specify '-iwad', we have to identify using 
    // IdentifyIWADByName.  However, if the iwad does not match
    // any known IWAD name, we may have a dilemma.  Try to 
    // identify by its contents.

    if (gamemission == none)
    {
        unsigned int i;

        for (i=0; i<numlumps; ++i)
        {
            if (!strncasecmp(lumpinfo[i]->name, "MAP01", 8))
            {
                gamemission = doom2;
                break;
            } 
            else if (!strncasecmp(lumpinfo[i]->name, "E1M1", 8))
            {
                gamemission = doom;
                break;
            }
			else if( remove_limits &&
					( !strncasecmp(lumpinfo[i]->name, "E2M1", 8)
					|| !strncasecmp(lumpinfo[i]->name, "E3M1", 8)
					|| !strncasecmp(lumpinfo[i]->name, "E4M1", 8) ) )
			{
				gamemission = doom;
				break;
			}
        }

        if (gamemission == none)
        {
            // Still no idea.  I don't think this is going to work.

            I_Error("Unknown or invalid IWAD file.");
        }
    }

    // Make sure gamemode is set up correctly

    if (logical_gamemission == doom)
    {
        // Doom 1.  But which version?

        if (W_CheckNumForName("E4M1") > 0)
        {
            // Ultimate Doom

            gamemode = retail;
        } 
        else if (W_CheckNumForName("E3M1") > 0)
        {
            gamemode = registered;
        }
        else
        {
            gamemode = shareware;
        }
    }
    else
    {
        int p;

        // Doom 2 of some kind.
        gamemode = commercial;

        // We can manually override the gamemission that we got from the
        // IWAD detection code. This allows us to eg. play Plutonia 2
        // with Freedoom and get the right level names.

        //!
        // @category compat
        // @arg <pack>
        //
        // Explicitly specify a Doom II "mission pack" to run as, instead of
        // detecting it based on the filename. Valid values are: "doom2",
        // "tnt" and "plutonia".
        //
        p = M_CheckParmWithArgs("-pack", 1);
        if (p > 0)
        {
            SetMissionForPackName(myargv[p + 1]);
        }
    }
}

static void D_AddWidescreenPacks()
{
	M_DashboardSetLicenceInUse( Licence_WidePix, false );
	if( !M_CheckParm( "-nowidepix" ) && gamevariant != unityport )
	{
		const char* widescreenpackname = NULL;

		if( gamemission == doom )
		{
			if( gamevariant == freedoom )
			{
				widescreenpackname = "freedoom1.widepix";
			}
			else
			{
				switch( gamemode )
				{
				case retail:					widescreenpackname = "doom.ultimate.widepix"; break;
				case registered:				widescreenpackname = "doom.registered.widepix"; break;
				case shareware:					widescreenpackname = "doom.shareware.widepix"; break;
				default:						break;
				}
			}
		}
		else if( gamemission == doom2 )
		{
			widescreenpackname = gamevariant == freedoom ? "freedoom2.widepix" : "doom2.widepix";
		}
		else if( gamemission == pack_tnt )	widescreenpackname = "tnt.widepix";
		else if( gamemission == pack_plut )	widescreenpackname = "plutonia.widepix";
		else if( gamemission == pack_chex )	widescreenpackname = "chex.widepix";
		else if( gamemission == pack_hacx )	widescreenpackname = "hacx.widepix";

		const char* widescreenfilename = widescreenpackname ? D_FindWADByName( widescreenpackname ) : NULL;
		if( widescreenfilename )
		{
			I_TerminalPrintf( Log_Startup, " Adding %s\n", widescreenfilename );
			W_AddFileWithType( widescreenfilename, (wadtype_t)( wt_system | wt_widepix ) );
			free( (void*)widescreenfilename );

			M_DashboardSetLicenceInUse( Licence_WidePix, true );
		}
	}
}

static void D_AddExtendedAssets()
{
	const char* boomres = D_FindWADByName( "boomres.wad" );
	if( boomres )
	{
		I_TerminalPrintf( Log_Startup, " Adding boomres.wad\n" );
		W_AddFileWithType( boomres, (wadtype_t)( wt_system | wt_boomassets ) );
		free( (void*)boomres );
	}
}

// Set the gamedescription string

static void D_SetGameDescription(void)
{
    if (logical_gamemission == doom)
    {
        // Doom 1.  But which version?

        if (gamevariant == freedoom)
        {
            gamedescription = GetGameName("Freedoom: Phase 1");
        }
        else if (gamemode == retail)
        {
            // Ultimate Doom

            gamedescription = GetGameName("The Ultimate DOOM");
        }
        else if (gamemode == registered)
        {
            gamedescription = GetGameName("DOOM Registered");
        }
        else if (gamemode == shareware)
        {
            gamedescription = GetGameName("DOOM Shareware");
        }
    }
    else
    {
        // Doom 2 of some kind.  But which mission?

        if (gamevariant == freedm)
        {
            gamedescription = GetGameName("FreeDM");
        }
        else if (gamevariant == freedoom)
        {
            gamedescription = GetGameName("Freedoom: Phase 2");
        }
        else if (logical_gamemission == doom2)
        {
            gamedescription = GetGameName("DOOM 2: Hell on Earth");
        }
        else if (logical_gamemission == pack_plut)
        {
            gamedescription = GetGameName("DOOM 2: Plutonia Experiment"); 
        }
        else if (logical_gamemission == pack_tnt)
        {
            gamedescription = GetGameName("DOOM 2: TNT - Evilution");
        }
    }

    if (gamedescription == NULL)
    {
        gamedescription = M_StringDuplicate("Unknown");
    }
}

static doombool D_AddFile(char *filename)
{
    wad_file_t *handle;

	if( !filename )
	{
		I_Error( "Attempting to add a null filename." );
	}

    I_TerminalPrintf( Log_Startup, " Adding %s\n", filename );
    handle = W_AddFile(filename);

    return handle != NULL;
}

// Copyright message banners
// Some dehacked mods replace these.  These are only displayed if they are 
// replaced by dehacked.

static const char *copyright_banners[] =
{
    "===========================================================================\n"
    "ATTENTION:  This version of DOOM has been modified.  If you would like to\n"
    "get a copy of the original game, call 1-800-IDGAMES or see the readme file.\n"
    "        You will not receive technical support for modified games.\n"
    "                      press enter to continue\n"
    "===========================================================================\n",

    "===========================================================================\n"
    "                 Commercial product - do not distribute!\n"
    "         Please report software piracy to the SPA: 1-800-388-PIR8\n"
    "===========================================================================\n",

    "===========================================================================\n"
    "                                Shareware!\n"
    "===========================================================================\n"
};

// Prints a message only if it has been modified by dehacked.

void PrintDehackedBanners(void)
{
    size_t i;

    for (i=0; i<arrlen(copyright_banners); ++i)
    {
        const char *deh_s;

        deh_s = DEH_String(copyright_banners[i]);

        if (deh_s != copyright_banners[i])
        {
            I_TerminalPrintf( Log_Startup, "%s", deh_s );

            // Make sure the modified banner always ends in a newline character.
            // If it doesn't, add a newline.  This fixes av.wad.

            if (deh_s[strlen(deh_s) - 1] != '\n')
            {
                I_TerminalPrintf( Log_None, "\n" );
            }
        }
    }
}

static struct 
{
    const char *description;
    const char *cmdline;
    GameVersion_t version;
} gameversions[] = {
    {"Doom 1.2",             "1.2",        exe_doom_1_2},
    {"Doom 1.666",           "1.666",      exe_doom_1_666},
    {"Doom 1.7/1.7a",        "1.7",        exe_doom_1_7},
    {"Doom 1.8",             "1.8",        exe_doom_1_8},
    {"Doom 1.9",             "1.9",        exe_doom_1_9},
    {"Doom 1.9",             "doom1.9",    exe_doom_1_9},
    {"Hacx",                 "hacx",       exe_hacx},
    {"Ultimate Doom",        "ultimate",   exe_ultimate},
    {"Final Doom",           "final",      exe_final},
    {"Final Doom (alt)",     "final2",     exe_final2},
    {"Chex Quest",           "chex",       exe_chex},
    {"Limit Removing",       "limitremoving", exe_limit_removing},
    {"Bug Fixed Limit Removing", "bugfixed", exe_limit_removing_fixed},
    {"Boom 2.02",            "boom2.02",   exe_boom_2_02},
    {"-complevel 9",         "complevel9", exe_complevel9},
    {"MBF",                  "mbf",        exe_mbf},
	{"MBF + DEHEXTRA",       "mbfextra",   exe_mbf_dehextra},
    {"MBF21",                "mbf21",      exe_mbf21},
    {"MBF21 Extended",       "mbf21ex",    exe_mbf21_extended},
    {"R&R24",                "rnr24",      exe_rnr24},
    { NULL,                  NULL,         exe_invalid},
};

// Initialize the game version

static void InitGameVersion(void)
{
    byte *demolump;
    char demolumpname[6];
    int demoversion;
    int p;
    int i;
    doombool status;

    //! 
    // @arg <version>
    // @category compat
    //
    // Emulate a specific version of Doom.  Valid values are "1.2", 
    // "1.666", "1.7", "1.8", "1.9", "ultimate", "final", "final2",
    // "hacx" and "chex".
    //

    p = M_CheckParmWithArgs("-gameversion", 1);

    if (p)
    {
        for (i=0; gameversions[i].description != NULL; ++i)
        {
            if (!strcmp(myargv[p+1], gameversions[i].cmdline))
            {
                gameversion = gameversions[i].version;
				remove_limits = gameversion > exe_chex;
                break;
            }
        }
        
        if (gameversions[i].description == NULL) 
        {
            I_TerminalPrintf( Log_Error, "Supported game versions:\n" );

            for (i=0; gameversions[i].description != NULL; ++i)
            {
				I_TerminalPrintf( Log_Error, 	"\t%s (%s)\n", gameversions[i].cmdline,
												gameversions[i].description);
            }
            
            I_Error("Unknown game version '%s'", myargv[p+1]);
        }
    }
    else
    {
        // Determine automatically

        if (gamemission == pack_chex)
        {
            // chex.exe - identified by iwad filename

            gameversion = exe_chex;
        }
        else if (gamemission == pack_hacx)
        {
            // hacx.exe: identified by iwad filename

            gameversion = exe_hacx;
        }
        //else if (gamemode == shareware || gamemode == registered
        //      || (gamemode == commercial && gamemission == doom2))
        //{
        //    // original
        //    gameversion = exe_doom_1_9;
		//
        //    // Detect version from demo lump
        //    for (i = 1; i <= 3; ++i)
        //    {
        //        M_snprintf(demolumpname, 6, "demo%i", i);
        //        if (W_CheckNumForName(demolumpname) > 0)
        //        {
        //            demolump = W_CacheLumpName(demolumpname, PU_STATIC);
        //            demoversion = demolump[0];
        //            W_ReleaseLumpName(demolumpname);
        //            status = true;
        //            switch (demoversion)
        //            {
        //                case 0:
        //                case 1:
        //                case 2:
        //                case 3:
        //                case 4:
        //                    gameversion = exe_doom_1_2;
        //                    break;
        //                case 106:
        //                    gameversion = exe_doom_1_666;
        //                    break;
        //                case 107:
        //                    gameversion = exe_doom_1_7;
        //                    break;
        //                case 108:
        //                    gameversion = exe_doom_1_8;
        //                    break;
        //                case 109:
        //                    gameversion = exe_doom_1_9;
        //                    break;
        //                default:
        //                    status = false;
        //                    break;
        //            }
        //            if (status)
        //            {
        //                break;
        //            }
        //        }
        //    }
        //}
        //else if (gamemode == retail)
        //{
        //    gameversion = exe_ultimate;
        //}
        //else if (gamemode == commercial)
        //{
        //    // Final Doom: tnt or plutonia
        //    // Defaults to emulating the first Final Doom executable,
        //    // which has the crash in the demo loop; however, having
        //    // this as the default should mean that it plays back
        //    // most demos correctly.
		//
        //    gameversion = exe_final;
        //}
    }

    // Deathmatch 2.0 did not exist until Doom v1.4
    if (gameversion <= exe_doom_1_2 && deathmatch == 2)
    {
        deathmatch = 1;
    }
    
    // The original exe does not support retail - 4th episode not supported

    if (gameversion < exe_ultimate && gamemode == retail)
    {
        gamemode = registered;
    }

    // EXEs prior to the Final Doom exes do not support Final Doom.

    if (gameversion < exe_final && gamemode == commercial
     && (gamemission == pack_tnt || gamemission == pack_plut))
    {
        gamemission = doom2;
    }
}

// Function called at exit to display the ENDOOM screen

static void D_Endoom(void)
{
    byte *endoom;

    // Don't show ENDOOM if we have it disabled, or we're running
    // in screensaver or control test mode. Only show it once the
    // game has actually started.

    if (!show_endoom || !main_loop_started
     || screensaver_mode || M_CheckParm("-testcontrols") > 0)
    {
        return;
    }

    endoom = (byte*)W_CacheLumpName(DEH_String("ENDOOM"), PU_STATIC);

    I_Endoom(endoom);
}

// Load dehacked patches needed for certain IWADs.
static void LoadIwadDeh(void)
{
    DEH_LoadLumpByName("DEHACKED", (gameversion == exe_hacx), (gamevariant == freedoom || gamevariant == freedm));

    // Chex Quest needs a separate Dehacked patch which must be downloaded
    // and installed next to the IWAD.
    if (gameversion == exe_chex)
    {
        char *chex_deh = NULL;
        char *dirname;

        // Look for chex.deh in the same directory as the IWAD file.
        dirname = M_DirName( gameconf->iwad );
        chex_deh = M_StringJoin(dirname, DIR_SEPARATOR_S, "chex.deh", NULL);
        free(dirname);

        // If the dehacked patch isn't found, try searching the WAD
        // search path instead.  We might find it...
        if (!M_FileExists(chex_deh))
        {
            free(chex_deh);
            chex_deh = D_FindWADByName("chex.deh");
        }

        // Still not found?
        if (chex_deh == NULL)
        {
            I_Error("Unable to find Chex Quest dehacked file (chex.deh).\n"
                    "The dehacked file is required in order to emulate\n"
                    "chex.exe correctly.  It can be found in your nearest\n"
                    "/idgames repository mirror at:\n\n"
                    "   utils/exe_edit/patches/chexdeh.zip");
        }

        if (!DEH_LoadFile(chex_deh))
        {
            I_Error("Failed to load chex.deh needed for emulating chex.exe.");
        }
    }
}

static void G_CheckDemoStatusAtExit (void)
{
    G_CheckDemoStatus();
}

//
// D_DoomMain
//

DOOM_C_API void D_DoomMain (void)
{
    int32_t p;
    char file[256];
    char demolumpname[9];
    uint32_t numiwadlumps;

	I_ErrorInit();

	blackedges.data8bithandle = NULL;
	blackedges.dataARGBhandle = NULL;
	blackedges.datatexture = NULL;
	blackedges.data = blackedgesdata;
	blackedges.user = NULL;
	blackedges.width = 1;
	blackedges.height = 128;
	blackedges.pitch = 128;
	blackedges.pixel_size_bytes = 1;
	blackedges.mode = VB_Transposed;
	blackedges.verticalscale = 1.0f;
	blackedges.magic_value = vbuffer_magic;

	// Auto-detect the configuration dir.
	if( M_CheckParm( "-portable" ) || M_FileExists( "rnr_portable.dat" ) )
	{
		M_SetConfigDir( "." DIR_SEPARATOR_S );
	}
	else
	{
		M_SetConfigDir(NULL);
	}

	// Load configuration files before initialising other subsystems.
	//DEH_printf("M_LoadDefaults: Load system defaults.\n");
	M_SetConfigFilenames("default.cfg", PROGRAM_PREFIX "doom.cfg");
	D_BindVariables();
	M_LoadDefaults();

	// Save configuration at exit.
	I_AtExit(M_SaveDefaults, false);

	I_SetWindowTitle(gamedescription);
	I_GraphicsCheckCommandLine();
	I_InitTimer();
	I_InitGraphics();
	I_TerminalInit();
	M_URLInit();
	M_InitDashboard();

	// This needs to come way early now since we need to parse WADs to build a game configuration...
	Z_Init();

	gameconf = D_BuildGameConf();

	if( !gameconf || !gameconf->iwad )
	{
		M_ScheduleLauncher();
	}

	// Before we go in to terminal mode, we want to allow the user to configure all options
	M_DashboardFirstLaunch();
	if( M_PerformLauncher() )
	{
		D_DestroyGameConf( gameconf );
		gameconf = D_BuildGameConf();
	}

	if( show_text_startup )
	{
		I_TerminalSetMode( TM_ImmediateRender );
	}
	I_TerminalPrintBanner( Log_Startup, PACKAGE_STRING, TXT_COLOR_YELLOW, PACKAGE_BANNER_BACK );
	DEH_printf( "M_LoadDefaults: Load system defaults.\n" );

	M_ProfileInit();
	M_ProfileThreadInit( "Main" );

    I_AtExit(D_Endoom, false);

	DEH_printf( "Z_Init: Init zone memory allocation daemon. \n" );

    //!
    // @category net
    //
    // Start a dedicated server, routing packets but not participating
    // in the game itself.
    //

    if (M_CheckParm("-dedicated") > 0)
    {
        I_TerminalPrintf( Log_Startup, "Dedicated server mode.\n" );
        NET_DedicatedServer();

        // Never returns
    }

    //!
    // @category net
    //
    // Query the Internet master server for a global list of active
    // servers.
    //

    if (M_CheckParm("-search"))
    {
        NET_MasterQuery();
        exit(0);
    }

    //!
    // @arg <address>
    // @category net
    //
    // Query the status of the server running on the given IP
    // address.
    //

    p = M_CheckParmWithArgs("-query", 1);

    if (p)
    {
        NET_QueryAddress(myargv[p+1]);
        exit(0);
    }

    //!
    // @category net
    //
    // Search the local LAN for running servers.
    //

    if (M_CheckParm("-localsearch"))
    {
        NET_LANQuery();
        exit(0);
    }

    //!
    // @category game
    // @vanilla
    //
    // Disable monsters.
    //
	
    nomonsters = M_CheckParm ("-nomonsters");

    //!
    // @category game
    // @vanilla
    //
    // Monsters respawn after being killed.
    //

    respawnparm = M_CheckParm ("-respawn");

    //!
    // @category game
    // @vanilla
    //
    // Monsters move faster.
    //

    fastparm = M_CheckParm ("-fast");

    //!
    // @vanilla
    //
    // Developer mode. F1 saves a screenshot in the current working
    // directory.
    //

    devparm = M_CheckParm ("-devparm");

    I_DisplayFPSDots(devparm);

    //!
    // @category net
    // @vanilla
    //
    // Start a deathmatch game.
    //

    if (M_CheckParm ("-deathmatch"))
	deathmatch = 1;

    //!
    // @category net
    // @vanilla
    //
    // Start a deathmatch 2.0 game.  Weapons do not stay in place and
    // all items respawn after 30 seconds.
    //

    if (M_CheckParm ("-altdeath"))
	deathmatch = 2;

    if (devparm)
	DEH_printf(D_DEVSTR);
    
    // find which dir to use for config files

    //!
    // @category game
    // @arg <x>
    // @vanilla
    //
    // Turbo mode.  The player's speed is multiplied by x%.  If unspecified,
    // x defaults to 200.  Values are rounded up to 10 and down to 400.
    //

    if ( (p=M_CheckParm ("-turbo")) )
    {
	int     scale = 200;
	
	if (p<myargc-1)
	    scale = atoi (myargv[p+1]);
	if (scale < 10)
	    scale = 10;
	if (scale > 400)
	    scale = 400;
        DEH_printf("turbo scale: %i%%\n", scale);
	forwardmove[0] = forwardmove[0]*scale/100;
	forwardmove[1] = forwardmove[1]*scale/100;
	sidemove[0] = sidemove[0]*scale/100;
	sidemove[1] = sidemove[1]*scale/100;
    }
    
    // init subsystems
    DEH_printf("V_Init: allocate screens.\n");
    V_Init ();

    // None found?

    if(gameconf->iwad == NULL)
    {
        I_Error("Game mode indeterminate.  No IWAD file was found.  Try\n"
                "specifying one with the '-iwad' command line parameter.\n");
    }

    modifiedgame = false;

    DEH_printf("W_Init: Init WADfiles.\n");
    D_AddFile((char*)gameconf->iwad);
	gamemission = gameconf->mission;
    numiwadlumps = numlumps;

    W_CheckCorrectIWAD(doom);

    // Now that we've loaded the IWAD, we can figure out what gamemission
    // we're playing and which version of Vanilla Doom we need to emulate.
    D_IdentifyVersion();

    // Check which IWAD variant we are using.

    if (W_CheckNumForName("FREEDOOM") >= 0)
    {
        if (W_CheckNumForName("FREEDM") >= 0)
        {
            gamevariant = freedm;
        }
        else
        {
            gamevariant = freedoom;
        }
    }
	// DMENUPIC is used by the Unity IWADs. Thus, we can't rely on it
	// any more to check for BFG Edition IWADs.
	// We can check for any of M_ACPT, M_CAN, M_EXITO, and M_CHG here
	// though as they're specific to the BFG frontend.
	// M_CHG is for changing games, so that one seems super specific
	// enough for our needs.
	else if( W_CheckNumForName("DMENUPIC") >= 0 || W_CheckNumForName( "DMAPINFO" ) >= 0 )
	{
		if (W_CheckNumForName("M_CHG") >= 0)
		{
			gamevariant = bfgedition;
		}
		else
		{
			gamevariant = unityport;
		}
	}

	InitGameVersion();

	D_AddWidescreenPacks();
	D_AddExtendedAssets();

    // Doom 3: BFG Edition includes modified versions of the classic
    // IWADs which can be identified by an additional DMENUPIC lump.
    // Furthermore, the M_GDHIGH lumps have been modified in a way that
    // makes them incompatible to Vanilla Doom and the modified version
    // of doom2.wad is missing the TITLEPIC lump.
    // We specifically check for DMENUPIC here, before PWADs have been
    // loaded which could probably include a lump of that name.

    if (gamevariant == bfgedition)
    {
        I_TerminalPrintf( Log_Startup, "BFG Edition: Using workarounds as needed.\n" );

        // BFG Edition changes the names of the secret levels to
        // censor the Wolfenstein references. It also has an extra
        // secret level (MAP33). In Vanilla Doom (meaning the DOS
        // version), MAP33 overflows into the Plutonia level names
        // array, so HUSTR_33 is actually PHUSTR_1.
        DEH_AddStringReplacement(HUSTR_31, "level 31: idkfa");
        DEH_AddStringReplacement(HUSTR_32, "level 32: keen");
        DEH_AddStringReplacement(PHUSTR_1, "level 33: betray");

        // The BFG edition doesn't have the "low detail" menu option (fair
        // enough). But bizarrely, it reuses the M_GDHIGH patch as a label
        // for the options menu (says "Fullscreen:"). Why the perpetrators
        // couldn't just add a new graphic lump and had to reuse this one,
        // I don't know.
        //
        // The end result is that M_GDHIGH is too wide and causes the game
        // to crash. As a workaround to get a minimum level of support for
        // the BFG edition IWADs, use the "ON"/"OFF" graphics instead.
        DEH_AddStringReplacement("M_GDHIGH", "M_MSGON");
        DEH_AddStringReplacement("M_GDLOW", "M_MSGOFF");

        // The BFG edition's "Screen Size:" graphic has also been changed
        // to say "Gamepad:". Fortunately, it (along with the original
        // Doom IWADs) has an unused graphic that says "Display". So we
        // can swap this in instead, and it kind of makes sense.
        DEH_AddStringReplacement("M_SCRNSZ", "M_DISP");
    }

	if( gamevariant == unityport && gamemode == commercial )
	{
		DEH_AddStringReplacement(HUSTR_31, "level 31: idkfa");
		DEH_AddStringReplacement(HUSTR_32, "level 32: keen");
	}

    //!
    // @category mod
    //
    // Disable auto-loading of .wad and .deh files.
    //
    //if (!M_ParmExists("-noautoload") && gamemode != shareware)
    //{
    //    char *autoload_dir;
	//
    //    // common auto-loaded files for all Doom flavors
	//
    //    if (gamemission < pack_chex)
    //    {
    //        autoload_dir = M_GetAutoloadDir("doom-all");
    //        DEH_AutoLoadPatches(autoload_dir);
    //        W_AutoLoadWADs(autoload_dir);
    //        free(autoload_dir);
    //    }
	//
    //    // auto-loaded files per IWAD
	//
    //    autoload_dir = M_GetAutoloadDir( IWADFilename() );
    //    DEH_AutoLoadPatches(autoload_dir);
    //    W_AutoLoadWADs(autoload_dir);
    //    free(autoload_dir);
    //}

	// Load PWAD files.
	modifiedgame = gameconf->pwadscount != 0;
	for( const char* pwad : gameconf->PWADs() )
	{
		I_TerminalPrintf( Log_Startup, " Adding %s\n", M_BaseName( pwad ) );
		W_AddFile( pwad );
	}

    // Debug:
//    W_PrintDirectory();

    //!
    // @arg <demo>
    // @category demo
    // @vanilla
    //
    // Play back the demo named demo.lmp.
    //

    p = M_CheckParmWithArgs ("-playdemo", 1);

    if (!p)
    {
        //!
        // @arg <demo>
        // @category demo
        // @vanilla
        //
        // Play back the demo named demo.lmp, determining the framerate
        // of the screen.
        //
	p = M_CheckParmWithArgs("-timedemo", 1);

    }

	if( !p )
	{
		p = M_CheckParmWithArgs( "-playaudit", 1 );
	}

	if( !p )
	{
		p = M_CheckParmWithArgs( "-auditdemo", 1 );
	}

    if (p)
    {
        char *uc_filename = strdup(myargv[p + 1]);
        M_ForceUppercase(uc_filename);

        // With Vanilla you have to specify the file without extension,
        // but make that optional.
        if (M_StringEndsWith(uc_filename, ".LMP"))
        {
            M_StringCopy(file, myargv[p + 1], sizeof(file));
        }
        else
        {
            DEH_snprintf(file, sizeof(file), "%s.lmp", myargv[p+1]);
        }

        free(uc_filename);

        if (D_AddFile(file))
        {
            M_StringCopy(demolumpname, lumpinfo[numlumps - 1]->name,
                         sizeof(demolumpname));
        }
        else
        {
            // If file failed to load, still continue trying to play
            // the demo in the same way as Vanilla Doom.  This makes
            // tricks like "-playdemo demo1" possible.

            M_StringCopy(demolumpname, myargv[p + 1], sizeof(demolumpname));
        }

        I_TerminalPrintf( Log_Startup, "Playing demo %s.\n", file );
    }

    I_AtExit(G_CheckDemoStatusAtExit, true);

    // Generate the WAD hash table.  Speed things up a bit.
    W_GenerateHashTable();

	D_InitStateTables();
	D_InitItemTables();
	D_InitSoundTables();

	// Load DEHACKED lumps of all kinds in WAD order
	for( lumpindex_t lumpindex = 0; lumpindex < numlumps; ++lumpindex )
	{
		if (!strncmp(lumpinfo[ lumpindex ]->name, "DEHACKED", 8))
		{
			DEH_LoadLump( lumpindex, true, true );
			D_InitStateLookupTables();
			D_InitItemLookupTables();
		}
	}

	// And command line after for thrills
	for( const char* dehfile : gameconf->DEHFiles() )
	{
		DEH_LoadFile( dehfile );
		D_InitStateLookupTables();
		D_InitItemLookupTables();
	}

	// With gameflow and dehacked parsed, we can finally set up the gamesim correctly
	D_RegisterGamesim();

	if( M_ParmExists( "-blackvoid" ) ) voidcleartype = Void_Black;
	if( M_ParmExists( "-whackyvoid" ) ) voidcleartype = Void_Whacky;
	if( M_ParmExists( "-skyvoid" ) ) voidcleartype = Void_Sky;

	rendersplitvisualise  = M_ParmExists( "-rendersplitvisualise" );

	maxrendercontexts = M_MIN( (int32_t)I_ThreadGetHardwareCount(), JobSystem::MaxJobs );
	p = M_CheckParmWithArgs( "-numrendercontexts", 1 );
	if( p )
	{
		M_StrToInt( myargv[p + 1], &num_render_contexts );
	}

	if( num_render_contexts <= 0 )
	{
		num_render_contexts = maxrendercontexts;
	}
	else
	{
		num_render_contexts = M_CLAMP( num_render_contexts, 1, maxrendercontexts );
	}

    // Set the gamedescription string. This is only possible now that
    // we've finished loading Dehacked patches.
    D_SetGameDescription();

    savegamedir = M_GetSaveGameDir( IWADFilename() );

    // Check for -file in shareware
    if (modifiedgame && (gamevariant != freedoom))
    {
	// These are the lumps that will be checked in IWAD,
	// if any one is not present, execution will be aborted.
	char name[23][9]=
	{
	    "e2m1","e2m2","e2m3","e2m4","e2m5","e2m6","e2m7","e2m8","e2m9",
	    "e3m1","e3m3","e3m3","e3m4","e3m5","e3m6","e3m7","e3m8","e3m9",
	    "dphoof","bfgga0","heada1","cybra1","spida1d1"
	};
	int i;
	
	if ( gamemode == shareware)
	    I_Error(DEH_String("\nYou cannot -file with the shareware "
			       "version. Register!"));

	// Check for fake IWAD with right name,
	// but w/o all the lumps of the registered version. 
	if (gamemode == registered)
	    for (i = 0;i < 23; i++)
		if (W_CheckNumForName(name[i])<0)
		    I_Error(DEH_String("\nThis is not the registered version."));
    }

    if ( !comp.additive_data_blocks
		&& ( W_CheckNumForName("SS_START") >= 0
			|| W_CheckNumForName("FF_END") >= 0) )
    {
        I_PrintDivider();
		I_TerminalPrintf( Log_Warning,	" WARNING: The loaded WAD file contains modified sprites or\n"
										" floor textures.  You may want to use the '-merge' command\n"
										" line option instead of '-file'.\n");
    }

    I_PrintStartupBanner(gamedescription);
    PrintDehackedBanners();

    DEH_printf("I_Init: Setting up machine state.\n");
    I_CheckIsScreensaver();
    I_InitJoystick();
    I_InitSound(true);
    I_InitMusic();

	size_t maxnumjobs = (int32_t)I_ThreadGetHardwareCount();
	jobs.reset( new JobSystem( maxnumjobs - 1 ) );

    I_TerminalPrintf( Log_Startup, "NET_Init: Init network subsystem.\n" );
    NET_Init ();

    // Initial netgame startup. Connect to server etc.
    D_ConnectNetGame();

    // get skill / episode / map from parms
    startskill = sk_medium;
    startepisode = current_episode->episode_num;
    startmap = current_episode->first_map->map_num;
    autostart = false;

    //!
    // @category game
    // @arg <skill>
    // @vanilla
    //
    // Set the game skill, 1-5 (1: easiest, 5: hardest).  A skill of
    // 0 disables all monsters.
    //

    p = M_CheckParmWithArgs("-skill", 1);

    if (p)
    {
	startskill = (skill_t)( myargv[p+1][0]-'1' );
	autostart = true;
    }

    //!
    // @category game
    // @arg <n>
    // @vanilla
    //
    // Start playing on episode n (1-4)
    //

    p = M_CheckParmWithArgs("-episode", 1);

    if (p)
    {
	startepisode = myargv[p+1][0]-'0';
	startmap = 1;
	autostart = true;
    }
	
    timelimit = 0;

    //! 
    // @arg <n>
    // @category net
    // @vanilla
    //
    // For multiplayer games: exit each level after n minutes.
    //

    p = M_CheckParmWithArgs("-timer", 1);

    if (p)
    {
	timelimit = atoi(myargv[p+1]);
    }

    //!
    // @category net
    // @vanilla
    //
    // Austin Virtual Gaming: end levels after 20 minutes.
    //

    p = M_CheckParm ("-avg");

    if (p)
    {
	timelimit = 20;
    }

    //!
    // @category game
    // @arg [<x> <y> | <xy>]
    // @vanilla
    //
    // Start a game immediately, warping to ExMy (Doom 1) or MAPxy
    // (Doom 2)
    //

    p = M_CheckParmWithArgs("-warp", 1);

    if (p)
    {
        if (gamemode == commercial)
            startmap = atoi (myargv[p+1]);
        else
        {
            startepisode = myargv[p+1][0]-'0';

            if (p + 2 < myargc)
            {
                startmap = myargv[p+2][0]-'0';
            }
            else
            {
                startmap = 1;
            }
        }
        autostart = true;
    }

    // Undocumented:
    // Invoked by setup to test the controls.

    //p = M_CheckParm("-testcontrols");
	//
    //if ( p > 0)
    //{
    //    startepisode = 1;
    //    startmap = 1;
    //    autostart = true;
    //    testcontrols = true;
	//
	//	M_RegisterDashboardWindow( "Test|Mouse", "Mouse testing", 400, 200, &testcontrols, Menu_Overlay, &D_TestControls );
    //}

    // Check for load game parameter
    // We do this here and save the slot number, so that the network code
    // can override it or send the load slot to other players.

    //!
    // @category game
    // @arg <s>
    // @vanilla
    //
    // Load the game in slot s.
    //

    p = M_CheckParmWithArgs("-loadgame", 1);
    
    if (p)
    {
        startloadgame = atoi(myargv[p+1]);
    }
    else
    {
        // Not loading a game
        startloadgame = -1;
    }

    DEH_printf("M_Init: Init miscellaneous info.\n");
    M_Init ();

    DEH_printf("R_Init: Init DOOM refresh daemon - ");
    R_Init ();

    DEH_printf("\nP_Init: Init Playloop state.\n");
    P_Init ();

    DEH_printf("S_Init: Setting up sound.\n");
    S_Init (sfxVolume * 8, musicVolume * 8);

    DEH_printf("D_CheckNetGame: Checking network game status.\n");
    D_CheckNetGame ();

    DEH_printf("HU_Init: Setting up heads up display.\n");
    HU_Init ();

    DEH_printf("ST_Init: Init status bar.\n");
    ST_Init ();

	demoloop = D_CreateDemoLoop();

    // If Doom II without a MAP01 lump, this is a store demo.
    // Moved this here so that MAP01 isn't constantly looked up
    // in the main loop.

    if (gamemode == commercial && W_CheckNumForName("map01") < 0)
        storedemo = true;

    //!
    // @arg <x>
    // @category demo
    // @vanilla
    //
    // Record a demo named x.lmp.
    //

    p = M_CheckParmWithArgs("-record", 1);

	if (p)
	{
		G_RecordDemo (myargv[p+1]);
		autostart = true;
	}

	if( ( p = M_CheckParmWithArgs( "-recordaudit", 1 ) ) )
	{
		G_RecordDemo( myargv[ p + 1 ] );
		autostart = true;
	}


    p = M_CheckParmWithArgs("-playdemo", 1);
	if (!p)
	{
		p = M_CheckParmWithArgs("-playaudit", 1);
	}
	if( !p )
	{
		p = M_CheckParmWithArgs( "-auditdemo", 1 );
	}

    if (p)
    {
	singledemo = true;              // quit after one demo
	G_DeferedPlayDemo (demolumpname);
	D_DoomLoop ();  // never returns
    }
	
    p = M_CheckParmWithArgs("-timedemo", 1);
    if (p)
    {
	G_TimeDemo (demolumpname);
	D_DoomLoop ();  // never returns
    }
	
    if (startloadgame >= 0)
    {
        M_StringCopy(file, P_SaveGameFile(startloadgame), sizeof(file));
	G_LoadGame(file);
    }

	episodeinfo_t* epinfo = D_GameflowGetEpisode( startepisode );
	mapinfo_t* mapinfo = D_GameflowGetMap( epinfo, startmap );
	
    if (gameaction != ga_loadgame )
    {
	if (autostart || netgame)
	    G_InitNew (startskill, mapinfo, GF_None, 0);
	else
	    D_StartTitle ();                // start up intro loop
    }

    D_DoomLoop ();  // never returns
}

