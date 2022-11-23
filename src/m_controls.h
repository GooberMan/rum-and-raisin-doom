//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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

#ifndef __M_CONTROLS_H__
#define __M_CONTROLS_H__

#include "doomtype.h"

DOOM_C_API extern boolean* gamekeydown;
DOOM_C_API extern boolean* mousebuttons;
DOOM_C_API extern boolean* joybuttons;

#define IsBound( key )				( key >= 0 )
#define GameKeyDown( key )			( IsBound( key ) && gamekeydown[ key ] )
#define MouseButtonDown( button )	( IsBound( button ) && mousebuttons[ button ] )
#define JoyButtonDown( button )		( IsBound( button ) && joybuttons[ button ] )
 
DOOM_C_API extern int key_right;
DOOM_C_API extern int key_left;

DOOM_C_API extern int key_up;
DOOM_C_API extern int key_down;
DOOM_C_API extern int key_strafeleft;
DOOM_C_API extern int key_straferight;
DOOM_C_API extern int key_fire;
DOOM_C_API extern int key_use;
DOOM_C_API extern int key_strafe;
DOOM_C_API extern int key_speed;
DOOM_C_API extern int key_toggle_autorun;

DOOM_C_API extern int key_jump;
 
DOOM_C_API extern int key_flyup;
DOOM_C_API extern int key_flydown;
DOOM_C_API extern int key_flycenter;
DOOM_C_API extern int key_lookup;
DOOM_C_API extern int key_lookdown;
DOOM_C_API extern int key_lookcenter;
DOOM_C_API extern int key_invleft;
DOOM_C_API extern int key_invright;
DOOM_C_API extern int key_useartifact;

// villsa [STRIFE] strife keys
DOOM_C_API extern int key_usehealth;
DOOM_C_API extern int key_invquery;
DOOM_C_API extern int key_mission;
DOOM_C_API extern int key_invpop;
DOOM_C_API extern int key_invkey;
DOOM_C_API extern int key_invhome;
DOOM_C_API extern int key_invend;
DOOM_C_API extern int key_invuse;
DOOM_C_API extern int key_invdrop;

DOOM_C_API extern int key_message_refresh;
DOOM_C_API extern int key_pause;

DOOM_C_API extern int key_multi_msg;
DOOM_C_API extern int key_multi_msgplayer[8];

DOOM_C_API extern int key_weapon1;
DOOM_C_API extern int key_weapon2;
DOOM_C_API extern int key_weapon3;
DOOM_C_API extern int key_weapon4;
DOOM_C_API extern int key_weapon5;
DOOM_C_API extern int key_weapon6;
DOOM_C_API extern int key_weapon7;
DOOM_C_API extern int key_weapon8;

DOOM_C_API extern int key_arti_quartz;
DOOM_C_API extern int key_arti_urn;
DOOM_C_API extern int key_arti_bomb;
DOOM_C_API extern int key_arti_tome;
DOOM_C_API extern int key_arti_ring;
DOOM_C_API extern int key_arti_chaosdevice;
DOOM_C_API extern int key_arti_shadowsphere;
DOOM_C_API extern int key_arti_wings;
DOOM_C_API extern int key_arti_torch;
DOOM_C_API extern int key_arti_morph;

DOOM_C_API extern int key_arti_all;
DOOM_C_API extern int key_arti_health;
DOOM_C_API extern int key_arti_poisonbag;
DOOM_C_API extern int key_arti_blastradius;
DOOM_C_API extern int key_arti_teleport;
DOOM_C_API extern int key_arti_teleportother;
DOOM_C_API extern int key_arti_egg;
DOOM_C_API extern int key_arti_invulnerability;

DOOM_C_API extern int key_demo_quit;
DOOM_C_API extern int key_spy;
DOOM_C_API extern int key_prevweapon;
DOOM_C_API extern int key_nextweapon;

DOOM_C_API extern int key_map_north;
DOOM_C_API extern int key_map_south;
DOOM_C_API extern int key_map_east;
DOOM_C_API extern int key_map_west;
DOOM_C_API extern int key_map_zoomin;
DOOM_C_API extern int key_map_zoomout;
DOOM_C_API extern int key_map_toggle;
DOOM_C_API extern int key_map_maxzoom;
DOOM_C_API extern int key_map_follow;
DOOM_C_API extern int key_map_grid;
DOOM_C_API extern int key_map_mark;
DOOM_C_API extern int key_map_clearmark;

// menu keys:

DOOM_C_API extern int key_menu_activate;
DOOM_C_API extern int key_menu_up;
DOOM_C_API extern int key_menu_down;
DOOM_C_API extern int key_menu_left;
DOOM_C_API extern int key_menu_right;
DOOM_C_API extern int key_menu_back;
DOOM_C_API extern int key_menu_forward;
DOOM_C_API extern int key_menu_confirm;
DOOM_C_API extern int key_menu_abort;

DOOM_C_API extern int key_menu_help;
DOOM_C_API extern int key_menu_save;
DOOM_C_API extern int key_menu_load;
DOOM_C_API extern int key_menu_volume;
DOOM_C_API extern int key_menu_detail;
DOOM_C_API extern int key_menu_qsave;
DOOM_C_API extern int key_menu_endgame;
DOOM_C_API extern int key_menu_messages;
DOOM_C_API extern int key_menu_qload;
DOOM_C_API extern int key_menu_quit;
DOOM_C_API extern int key_menu_gamma;

DOOM_C_API extern int key_menu_incscreen;
DOOM_C_API extern int key_menu_decscreen;
DOOM_C_API extern int key_menu_screenshot;

DOOM_C_API extern int key_menu_dashboard;

DOOM_C_API extern int mousebfire;
DOOM_C_API extern int mousebstrafe;
DOOM_C_API extern int mousebforward;

DOOM_C_API extern int mousebjump;

DOOM_C_API extern int mousebstrafeleft;
DOOM_C_API extern int mousebstraferight;
DOOM_C_API extern int mousebbackward;
DOOM_C_API extern int mousebuse;

DOOM_C_API extern int mousebprevweapon;
DOOM_C_API extern int mousebnextweapon;
DOOM_C_API extern int mousebinvleft;
DOOM_C_API extern int mousebinvright;

DOOM_C_API extern int joybfire;
DOOM_C_API extern int joybstrafe;
DOOM_C_API extern int joybuse;
DOOM_C_API extern int joybspeed;

DOOM_C_API extern int joybjump;

DOOM_C_API extern int joybstrafeleft;
DOOM_C_API extern int joybstraferight;

DOOM_C_API extern int joybprevweapon;
DOOM_C_API extern int joybnextweapon;

DOOM_C_API extern int joybmenu;
DOOM_C_API extern int joybautomap;

DOOM_C_API extern int dclick_use;

DOOM_C_API void M_BindBaseControls(void);
DOOM_C_API void M_BindHereticControls(void);
DOOM_C_API void M_BindHexenControls(void);
DOOM_C_API void M_BindStrifeControls(void);
DOOM_C_API void M_BindWeaponControls(void);
DOOM_C_API void M_BindMapControls(void);
DOOM_C_API void M_BindMenuControls(void);
DOOM_C_API void M_BindChatControls(unsigned int num_players);

DOOM_C_API void M_ApplyPlatformDefaults(void);

#endif /* #ifndef __M_CONTROLS_H__ */

