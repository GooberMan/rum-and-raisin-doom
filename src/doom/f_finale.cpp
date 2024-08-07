//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2020-2022 Ethan Watson
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
//	Game completion, final screen animation.
//


#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

// Functions.
#include "deh_main.h"
#include "i_system.h"
#include "i_swap.h"
#include "z_zone.h"
#include "v_video.h"
#include "w_wad.h"
#include "s_sound.h"

// Data.
#include "d_main.h"
#include "dstrings.h"
#include "sounds.h"

#include "doomstat.h"
#include "d_gameflow.h"
#include "d_gamesim.h"
#include "p_local.h"
#include "r_state.h"
#include "m_misc.h"

typedef enum
{
    F_STAGE_TEXT,
    F_STAGE_ARTSCREEN,
    F_STAGE_CAST,
} finalestage_t;

// ?
//#include "doomstat.h"
//#include "r_local.h"
//#include "f_finale.h"

// Stage of animation:
finalestage_t finalestage;

unsigned int finalecount;

#define	TEXTSPEED	3
#define	TEXTWAIT	250

const char *finaletext;
const char *finaletiletexture;

static vbuffer_t finaletiledata;
static texturecomposite_t* finaletilecomposite;

extern "C"
{
	extern vbuffer_t blackedges;
}

static intermission_t* finaleintermission = NULL;
static endgame_t* finaleendgame = NULL;

void	F_StartCast (void);
bool	F_CastTicker (void);
doombool F_CastResponder (event_t *ev);
void	F_CastDrawer (void);

void F_SwitchFinale()
{
	finalecount = 0;
	if( finaleendgame->type & EndGame_Cast )
	{
		F_StartCast();
	}
	else if( finaleendgame->type & EndGame_AnyArtScreen )
	{
		finalecount = 0;
		finalestage = F_STAGE_ARTSCREEN;
		wipegamestate = GS_INVALID;
		S_ChangeMusicLump( &finaleendgame->music_lump, !!( finaleendgame->type & EndGame_LoopingMusic ) );
	}
}

//
// F_StartFinale
//
DOOM_C_API void F_StartIntermission( intermission_t* intermission )
{
	finaleendgame = NULL;
	finaleintermission = intermission;

	gameaction = ga_nothing;
	gamestate = GS_FINALE;
	viewactive = false;
	automapactive = false;

	S_ChangeMusicLump( &finaleintermission->music_lump, true );

	finaletext = DEH_String( finaleintermission->text.val );
	finaletiletexture = DEH_String( finaleintermission->background_lump.val );

	finalestage = F_STAGE_TEXT;
	finalecount = 0;

	finaletilecomposite = R_CacheAndGetCompositeFlat( finaletiletexture );
	finaletiledata.data = finaletilecomposite->data;
	finaletiledata.width = finaletilecomposite->width;
	finaletiledata.height = finaletilecomposite->height;
	finaletiledata.pitch = finaletilecomposite->pitch;
	finaletiledata.magic_value = vbuffer_magic;
}

DOOM_C_API void F_StartFinale( endgame_t* endgame )
{
	if( endgame->intermission )
	{
		F_StartIntermission( endgame->intermission );
		finaleendgame = endgame;
		return;
	}

	finaleintermission = nullptr;
	finaleendgame = endgame;

	gameaction = ga_nothing;
	gamestate = GS_FINALE;
	viewactive = false;
	automapactive = false;

	F_SwitchFinale();
}



DOOM_C_API doombool F_Responder (event_t *event)
{
    if (finalestage == F_STAGE_CAST)
	return F_CastResponder (event);
	
    return false;
}


//
// F_Ticker
//
DOOM_C_API void F_Ticker (void)
{
    size_t		i;

	doombool didskip = false;
    
    // check for skipping
    if( finalestage == F_STAGE_TEXT
		&& finaleintermission
		&& ( comp.finale_always_allow_skip_text || ( finaleintermission->type & Intermission_Skippable ) == Intermission_Skippable )
		&& finalecount > 50 )
    {
		// go on to the next level
		for (i=0 ; i<MAXPLAYERS ; i++)
		{
			if (players[i].cmd.buttons)
			{
				didskip = true;
			}
		}
	}

	// advance animation
	finalecount++;
	
	bool canprogress = finaleendgame != nullptr
						&& ( finaleendgame->type & EndGame_DoNextMap ) == EndGame_DoNextMap
						&& current_map->next_map != nullptr;

	bool shouldprogress = false;
	if( finalestage == F_STAGE_CAST )
	{
		shouldprogress = F_CastTicker();
	}
	else if( finalestage == F_STAGE_ARTSCREEN
		&& finalecount > 50 )
	{
		// go on to the next level
		for (i=0 ; i<MAXPLAYERS ; i++)
		{
			if (players[i].cmd.buttons)
			{
				shouldprogress = true;
			}
		}
	}

	doombool advance = ( canprogress && shouldprogress )
					|| didskip
					|| ( finalestage == F_STAGE_TEXT && finalecount > strlen( finaletext ) * TEXTSPEED + TEXTWAIT );
	
	if( advance )
	{
		if( !finaleendgame
			|| ( canprogress && shouldprogress ) )
		{
			gameaction = ga_worlddone;
		}
		else
		{
			F_SwitchFinale();
		}
	}
}


//
// F_TextWrite
//

#include "hu_stuff.h"
extern "C"
{
	extern	patch_t *hu_font[HU_FONTSIZE];
}

void F_TextWrite (void)
{
    int		w;
    signed int	count;
    const char *ch;
    int		c;
    int		cx;
    int		cy;

	V_TileBuffer( &finaletiledata, 0, 0, V_VIRTUALWIDTH, V_VIRTUALHEIGHT );
	V_FillBorder( &blackedges, 0, V_VIRTUALHEIGHT );

    V_MarkRect (0, 0, V_VIRTUALWIDTH, V_VIRTUALHEIGHT);
    
    // draw some of the text onto the screen
    cx = 10;
    cy = 10;
    ch = finaletext;
	
    count = ((signed int) finalecount - 10) / TEXTSPEED;
    if (count < 0)
	count = 0;
    for ( ; count ; count-- )
    {
	c = *ch++;
	if (!c)
	    break;
	if (c == '\n')
	{
	    cx = 10;
	    cy += 11;
	    continue;
	}
		
	c = toupper(c) - HU_FONTSTART;
	if (c < 0 || c> HU_FONTSIZE)
	{
	    cx += 4;
	    continue;
	}
		
	w = SHORT (hu_font[c]->width);
	if (cx+w > V_VIRTUALWIDTH)
	    break;
	V_DrawPatch(cx, cy, hu_font[c]);
	cx+=w;
    }
	
}

//
// Final DOOM 2 animation
// Casting by id Software.
//   in order of appearance
//
typedef struct
{
    const char	*name;
    mobjtype_t	type;
} castinfo_t;

castinfo_t	castorder[] = {
    {CC_ZOMBIE, MT_POSSESSED},
    {CC_SHOTGUN, MT_SHOTGUY},
    {CC_HEAVY, MT_CHAINGUY},
    {CC_IMP, MT_TROOP},
    {CC_DEMON, MT_SERGEANT},
    {CC_LOST, MT_SKULL},
    {CC_CACO, MT_HEAD},
    {CC_HELL, MT_KNIGHT},
    {CC_BARON, MT_BRUISER},
    {CC_ARACH, MT_BABY},
    {CC_PAIN, MT_PAIN},
    {CC_REVEN, MT_UNDEAD},
    {CC_MANCU, MT_FATSO},
    {CC_ARCH, MT_VILE},
    {CC_SPIDER, MT_SPIDER},
    {CC_CYBER, MT_CYBORG},
    {CC_HERO, MT_PLAYER},

    {NULL,MT_PLAYER}
};

int		castnum;
int		casttics;
const state_t*	caststate;
doombool		castdeath;
int		castframes;
int		castonmelee;
doombool		castattacking;

bool					newcast_active = false;
endgame_castmember_t*	newcast_currmember = nullptr;
endgame_castframe_t*	newcast_currframe = nullptr;
bool					newcast_alive = false;
int32_t					newcast_ticsremaining = 0;

void F_NewCastSetFrame( endgame_castframe_t* frame )
{
	newcast_currframe = frame;
	newcast_ticsremaining = newcast_currframe->duration;

	if( frame->sound )
	{
		S_StartSound( nullptr, frame->sound );
	}
}

void F_NewCastSetMember( endgame_castmember_t* member )
{
	newcast_currmember = member;
	newcast_alive = true;
	F_NewCastSetFrame( newcast_currmember->alive_frames );
}

void F_NewCastKillCurrentMember()
{
	newcast_alive = false;
	F_NewCastSetFrame( newcast_currmember->death_frames );
}

void F_StartNewCast()
{
	lumpindex_t bossbacknum = W_GetNumForNameExcluding( finaleendgame->primary_image_lump.Resolve(), comp.widescreen_assets ? wt_none : wt_widepix );
	W_CacheLumpNum( bossbacknum, PU_LEVEL );

	for( endgame_castmember_t& member : std::span( finaleendgame->cast_members, finaleendgame->num_cast_members ) )
	{
		for( endgame_castframe_t& frame : std::span( member.alive_frames, member.num_alive_frames ) )
		{
			W_CacheLumpName( frame.lump.Resolve(), PU_LEVEL );
			const char* tranmap = frame.tranmap.Resolve();
			if( tranmap ) W_CacheLumpName( tranmap, PU_LEVEL );
		}

		for( endgame_castframe_t& frame : std::span( member.death_frames, member.num_death_frames ) )
		{
			W_CacheLumpName( frame.lump.Resolve(), PU_LEVEL );
			const char* tranmap = frame.tranmap.Resolve();
			if( tranmap ) W_CacheLumpName( tranmap, PU_LEVEL );
		}
	}

	F_NewCastSetMember( finaleendgame->cast_members );
}

bool F_NewCastTicker()
{
	bool looped = false;

	if( --newcast_ticsremaining <= 0 )
	{
		endgame_castframe_t* currbegin = newcast_alive ? newcast_currmember->alive_frames
														: newcast_currmember->death_frames;
		endgame_castframe_t* currend = currbegin + ( newcast_alive ? newcast_currmember->num_alive_frames
																	: newcast_currmember->num_death_frames );

		endgame_castframe_t* nextframe = newcast_currframe + 1;
		if( newcast_alive
			|| nextframe != currend )
		{
			F_NewCastSetFrame( nextframe != currend ? nextframe : currbegin );
		}
		else
		{
			endgame_castmember_t* nextmember = newcast_currmember + 1;
			if( nextmember == finaleendgame->cast_members + finaleendgame->num_cast_members )
			{
				looped = true;
				nextmember = ( finaleendgame->type & EndGame_DoNextMap ) != EndGame_DoNextMap || current_map->next_map == nullptr
								? finaleendgame->cast_members
								: nullptr;
			}

			if( nextmember )
			{
				F_NewCastSetMember( nextmember );
			}
		}
	}

	return looped;
}

doombool F_NewCastResponder( event_t* ev )
{
	bool canrespond = ev->type == ev_keydown;

	if( comp.finale_allow_mouse_to_skip )
	{
		canrespond |= ( ev->type == ev_mouse && ev->data5 != 0 );
	}

	if ( !canrespond )
	{
		return false;
	}

	if( newcast_alive )
	{
		newcast_alive = false;
		F_NewCastSetFrame( newcast_currmember->death_frames );
	}
	return true;
}

void F_CastPrint( const char *text );

void F_NewCastDrawer()
{
	lumpindex_t bossbacknum = W_GetNumForNameExcluding( finaleendgame->primary_image_lump.Resolve(), comp.widescreen_assets ? wt_none : wt_widepix );
	patch_t* bossback = (patch_t*)W_CacheLumpNum( bossbacknum, PU_LEVEL );
	int32_t xpos = -( ( bossback->width - V_VIRTUALWIDTH ) / 2 );
	V_DrawPatch( xpos, 0, bossback );

	F_CastPrint( DEH_StringLookupMnemonic( newcast_currmember->name_mnemonic.Resolve() ) );
			
	patch_t* framepatch = (patch_t*)W_CacheLumpName( newcast_currframe->lump.Resolve(), PU_LEVEL );
	translation_t* translation = R_GetTranslation( newcast_currframe->translation.Resolve() );
	const char* tranmapname = newcast_currframe->tranmap.Resolve();
	byte* tranmap = tranmapname ? (byte*)W_CacheLumpName( tranmapname, PU_LEVEL ) : nullptr;

	if( newcast_currframe->flipped )
	{
		V_DrawPatchFlipped( V_VIRTUALWIDTH >> 1, 170, framepatch, tranmap, translation ? translation->table : nullptr );
	}
	else
	{
		V_DrawPatch( V_VIRTUALWIDTH >> 1, 170, framepatch, tranmap, translation ? translation->table : nullptr );
	}
}

//
// F_StartCast
//
void F_StartCast (void)
{
    wipegamestate = GS_INVALID;		// force a screen wipe
    finalestage = F_STAGE_CAST;
    S_ChangeMusicLump( &finaleendgame->music_lump, !!( finaleendgame->type & EndGame_LoopingMusic ) );

	newcast_active = ( finaleendgame->type & EndGame_BuiltInCast ) != EndGame_BuiltInCast;

	if( newcast_active )
	{
		F_StartNewCast();
		return;
	}

	castnum = 0;
	caststate = &states[mobjinfo[castorder[castnum].type].seestate];
	casttics = caststate->tics;
	castdeath = false;
	castframes = 0;
	castonmelee = 0;
	castattacking = false;
}

//
// F_CastTicker
//
bool F_CastTicker (void)
{
	if( newcast_active )
	{
		return F_NewCastTicker();
	}

    int		st;
    int		sfx;
	
    if (--casttics > 0)
	{
		return false;			// not time to change state yet
	}
		
	bool looped = false;
    if (caststate->tics == -1 || caststate->nextstate == S_NULL)
    {
		// switch from deathstate to next monster
		castnum++;
		castdeath = false;
		if (castorder[castnum].name == NULL)
		{
			castnum = 0;
			looped = true;
		}
		if (mobjinfo[castorder[castnum].type].seesound)
			S_StartSound (NULL, mobjinfo[castorder[castnum].type].seesound);
		caststate = &states[mobjinfo[castorder[castnum].type].seestate];
		castframes = 0;
    }
    else
    {
		// just advance to next state in animation
		if (caststate == &states[S_PLAY_ATK1])
			goto stopattack;	// Oh, gross hack!
		st = caststate->nextstate;
		caststate = &states[st];
		castframes++;
	
		// sound hacks....
		switch (st)
		{
		  case S_PLAY_ATK1:	sfx = sfx_dshtgn; break;
		  case S_POSS_ATK2:	sfx = sfx_pistol; break;
		  case S_SPOS_ATK2:	sfx = sfx_shotgn; break;
		  case S_VILE_ATK2:	sfx = sfx_vilatk; break;
		  case S_SKEL_FIST2:	sfx = sfx_skeswg; break;
		  case S_SKEL_FIST4:	sfx = sfx_skepch; break;
		  case S_SKEL_MISS2:	sfx = sfx_skeatk; break;
		  case S_FATT_ATK8:
		  case S_FATT_ATK5:
		  case S_FATT_ATK2:	sfx = sfx_firsht; break;
		  case S_CPOS_ATK2:
		  case S_CPOS_ATK3:
		  case S_CPOS_ATK4:	sfx = sfx_shotgn; break;
		  case S_TROO_ATK3:	sfx = sfx_claw; break;
		  case S_SARG_ATK2:	sfx = sfx_sgtatk; break;
		  case S_BOSS_ATK2:
		  case S_BOS2_ATK2:
		  case S_HEAD_ATK2:	sfx = sfx_firsht; break;
		  case S_SKULL_ATK2:	sfx = sfx_sklatk; break;
		  case S_SPID_ATK2:
		  case S_SPID_ATK3:	sfx = sfx_shotgn; break;
		  case S_BSPI_ATK2:	sfx = sfx_plasma; break;
		  case S_CYBER_ATK2:
		  case S_CYBER_ATK4:
		  case S_CYBER_ATK6:	sfx = sfx_rlaunc; break;
		  case S_PAIN_ATK3:	sfx = sfx_sklatk; break;
		  default: sfx = 0; break;
		}
		
		if (sfx)
			S_StartSound (NULL, sfx);
    }
	
    if (castframes == 12)
    {
		// go into attack frame
		castattacking = true;
		if (castonmelee)
			caststate=&states[mobjinfo[castorder[castnum].type].meleestate];
		else
			caststate=&states[mobjinfo[castorder[castnum].type].missilestate];
		castonmelee ^= 1;
		if (caststate == &states[S_NULL])
		{
			if (castonmelee)
			caststate=
				&states[mobjinfo[castorder[castnum].type].meleestate];
			else
			caststate=
				&states[mobjinfo[castorder[castnum].type].missilestate];
		}
	}
	
	if (castattacking)
	{
		if (castframes == 24
			||	caststate == &states[mobjinfo[castorder[castnum].type].seestate] )
		{
			stopattack:
			castattacking = false;
			castframes = 0;
			caststate = &states[mobjinfo[castorder[castnum].type].seestate];
		}
    }
	
    casttics = caststate->tics;
    if (casttics == -1)
	{
		casttics = 15;
	}

	return looped;
}


//
// F_CastResponder
//

doombool F_CastResponder (event_t* ev)
{
	if( newcast_active )
	{
		return F_NewCastResponder( ev );
	}
		
	doombool canrespond = ev->type == ev_keydown;

	if( comp.finale_allow_mouse_to_skip )
	{
		canrespond |= ev->type == ev_keyup || ( ev->type == ev_mouse && ev->data5 != 0 );
	}

	if ( !canrespond )
	{
		return false;
	}

    if (castdeath)
	return true;			// already in dying frames
		
    // go into death frame
    castdeath = true;
    caststate = &states[mobjinfo[castorder[castnum].type].deathstate];
    casttics = caststate->tics;
    castframes = 0;
    castattacking = false;
    if (mobjinfo[castorder[castnum].type].deathsound)
	S_StartSound (NULL, mobjinfo[castorder[castnum].type].deathsound);
	
    return true;
}


void F_CastPrint (const char *text)
{
    const char *ch;
    int		c;
    int		cx;
    int		w;
    int		width;
    
    // find width
    ch = text;
    width = 0;
	
    while (ch)
    {
	c = *ch++;
	if (!c)
	    break;
	c = toupper(c) - HU_FONTSTART;
	if (c < 0 || c> HU_FONTSIZE)
	{
	    width += 4;
	    continue;
	}
		
	w = SHORT (hu_font[c]->width);
	width += w;
    }
    
    // draw it
    cx = V_VIRTUALWIDTH/2-width/2;
    ch = text;
    while (ch)
    {
	c = *ch++;
	if (!c)
	    break;
	c = toupper(c) - HU_FONTSTART;
	if (c < 0 || c> HU_FONTSIZE)
	{
	    cx += 4;
	    continue;
	}
		
	w = SHORT (hu_font[c]->width);
	V_DrawPatch(cx, 180, hu_font[c]);
	cx+=w;
    }
	
}


//
// F_CastDrawer
//

void F_CastDrawer (void)
{
	if( newcast_active )
	{
		return F_NewCastDrawer();
	}
		
    spritedef_t*	sprdef;
    spriteframe_t*	sprframe;
    int			lump;
    doombool		flip;
    patch_t*		patch;
    
	// WIDESCREEN HACK
	lumpindex_t bossbacknum = W_GetNumForNameExcluding( finaleendgame->primary_image_lump.Resolve(), comp.widescreen_assets ? wt_none : wt_widepix );
	patch_t* bossback = (patch_t*)W_CacheLumpNum(bossbacknum, PU_LEVEL);
	int32_t xpos = -( ( bossback->width - V_VIRTUALWIDTH ) / 2 );

    // erase the entire screen to a background
    V_DrawPatch (xpos, 0, bossback);

    F_CastPrint (DEH_String(castorder[castnum].name));
    
	extern std::unordered_map< int32_t, spritedef_t > sprites;
    // draw the current frame in the middle of the screen
    sprdef = &sprites[caststate->sprite];
    sprframe = &sprdef->spriteframes[ caststate->frame & FF_FRAMEMASK];
    lump = sprframe->lump[0];
    flip = (doombool)sprframe->flip[0];
			
    patch = spritepatches[ lump ];
    if (flip)
	V_DrawPatchFlipped(V_VIRTUALWIDTH/2, 170, patch);
    else
	V_DrawPatch(V_VIRTUALWIDTH/2, 170, patch);
}


//
// F_BunnyScroll
//
void F_BunnyScroll (void)
{
	char	name[10];
	int		stage;
	static int	laststage;

	lumpindex_t p1num = W_CheckNumForNameExcluding( DEH_String( finaleendgame->primary_image_lump.val ), comp.widescreen_assets ? wt_none : wt_widepix );
	lumpindex_t p2num = W_CheckNumForNameExcluding( DEH_String( finaleendgame->secondary_image_lump.val ), comp.widescreen_assets ? wt_none : wt_widepix );

	patch_t* p1 = (patch_t*)W_CacheLumpNum( p1num, PU_LEVEL );
	patch_t* p2 = (patch_t*)W_CacheLumpNum( p2num, PU_LEVEL );
	int32_t totalwidth = p1->width + p2->width;
	// WIDESCREEN HACK
	int32_t overlap = ( totalwidth - ( V_VIRTUALWIDTH * 2 ) ) / 2;

	stage = 6;
	while( stage >= 0 )
	{
		DEH_snprintf( name, 10, "END%i", stage );
		W_CacheLumpName( name, PU_LEVEL );
		--stage;
	}

    V_MarkRect (0, 0, V_VIRTUALWIDTH, V_VIRTUALHEIGHT);
	
	int32_t scrolled = (V_VIRTUALWIDTH - ((signed int) finalecount-230)/2);
    if (scrolled > V_VIRTUALWIDTH)
		scrolled = V_VIRTUALWIDTH;
    if (scrolled < 0)
		scrolled = 0;
		
	V_DrawPatchClipped( -overlap, 0, p1, scrolled, 0, p1->width - scrolled, V_VIRTUALHEIGHT );
	V_DrawPatchClipped( overlap + p2->width - scrolled, 0, p2, 0, 0, p2->width - ( p2->width - scrolled ), V_VIRTUALHEIGHT );
	
    if (finalecount < 1130)
	{
		return;
	}

	const char* endpatchstring = finaleendgame->bunny_end_overlay.Resolve();
	if( endpatchstring != nullptr )
	{
		if (finalecount < 1180)
		{
			DEH_snprintf(name, 10, endpatchstring, 0);
			V_DrawPatch(finaleendgame->bunny_end_x,
						finaleendgame->bunny_end_y, 
						(patch_t*)W_CacheLumpName(name, PU_CACHE));
			laststage = 0;
			return;
		}
	
		stage = M_MIN( (finalecount-1180) / 5, M_MAX( finaleendgame->bunny_end_count - 1, 0 ) );

		if (stage > laststage)
		{
			if( finaleendgame->bunny_end_sound != 0 && finaleendgame->bunny_end_sound != -1 )
			{
				S_StartSound( nullptr, finaleendgame->bunny_end_sound );
			}
			laststage = stage;
		}
	
		DEH_snprintf(name, 10, endpatchstring, stage);
		V_DrawPatch(finaleendgame->bunny_end_x,
					finaleendgame->bunny_end_y,
					(patch_t*)W_CacheLumpName( name, PU_CACHE ));
	}
}

static void F_ArtScreenDrawer(void)
{
	if( finaleendgame->type & EndGame_Bunny )
	{
		F_BunnyScroll();
	}
	else
	{
		const char* lumpname = finaleendgame->primary_image_lump.val;
		if( finaleendgame->type & EndGame_Ultimate && gameversion >= exe_ultimate )
		{
			lumpname = finaleendgame->secondary_image_lump.val;
		}

		lumpname = DEH_String( lumpname );

		// WIDESCREEN HACK
		lumpindex_t artnum = W_GetNumForNameExcluding( lumpname, comp.widescreen_assets ? wt_none : wt_widepix );
		patch_t* art = (patch_t*)W_CacheLumpNum( artnum, PU_LEVEL );
		int32_t xpos = -( ( art->width - V_VIRTUALWIDTH ) / 2 );

		V_DrawPatch( xpos, 0, art );
    }
}

//
// F_Drawer
//
DOOM_C_API void F_Drawer (void)
{
    switch (finalestage)
    {
        case F_STAGE_CAST:
            F_CastDrawer();
            break;
        case F_STAGE_TEXT:
            F_TextWrite();
            break;
        case F_STAGE_ARTSCREEN:
            F_ArtScreenDrawer();
            break;
    }
}


