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
//	Intermission screens.
//


#include <stdio.h>

#include "z_zone.h"
#include "m_fixed.h"
#include "m_container.h"
#include "r_local.h"
#include "d_playsim.h"

extern "C"
{
	#include "m_misc.h"
	#include "m_random.h"

	#include "deh_main.h"
	#include "i_swap.h"
	#include "i_system.h"
	#include "i_terminal.h"

	#include "w_wad.h"

	#include "g_game.h"

	#include "s_sound.h"

	#include "doomstat.h"

	// Data.
	#include "sounds.h"

	// Needs access to LFB.
	#include "v_video.h"

	#include "wi_stuff.h"

	extern vbuffer_t blackedges;
}

//
// Data needed to add patches to full screen intermission pics.
// Patches are statistics messages, and animations.
// Loads of by-pixel layout and placement, offsets etc.
//


//
// Different vetween registered DOOM (1994) and
//  Ultimate DOOM - Final edition (retail, 1995?).
// This is supposedly ignored for commercial
//  release (aka DOOM II), which had 34 maps
//  in one episode. So there.
#define NUMEPISODES	4
#define NUMMAPS		9


// in tics
//U #define PAUSELEN		(TICRATE*2) 
//U #define SCORESTEP		100
//U #define ANIMPERIOD		32
// pixel distance from "(YOU)" to "PLAYER N"
//U #define STARDIST		10 
//U #define WK 1


// GLOBAL LOCATIONS
#define WI_TITLEY		2
#define WI_SPACINGY    		33

// SINGPLE-PLAYER STUFF
#define SP_STATSX		50
#define SP_STATSY		50

#define SP_TIMEX		16
#define SP_TIMEY		(V_VIRTUALHEIGHT-32)


// NET GAME STUFF
#define NG_STATSY		50
#define NG_STATSX		(32 + SHORT(star->width)/2 + 32*!dofrags)

#define NG_SPACINGX    		64


// DEATHMATCH STUFF
#define DM_MATRIXX		42
#define DM_MATRIXY		68

#define DM_SPACINGX		40

#define DM_TOTALSX		269

#define DM_KILLERSX		10
#define DM_KILLERSY		100
#define DM_VICTIMSX    		5
#define DM_VICTIMSY		50




typedef enum
{
    ANIM_ALWAYS,
    ANIM_RANDOM,
    ANIM_LEVEL

} animenum_t;

typedef struct
{
    int		x;
    int		y;
    
} point_t;


//
// Animation.
// Used to be anim_t but that conflicted with the one in p_spec in Visual Studio.
//
typedef struct
{
    animenum_t	type;

    // period in tics between animations
    int		period;

    // number of animation frames
    int		nanims;

    // location of animation
    point_t	loc;

    // ALWAYS: n/a,
    // RANDOM: period deviation (<256),
    // LEVEL: level
    int		data1;

    // ALWAYS: n/a,
    // RANDOM: random base period,
    // LEVEL: n/a
    int		data2; 

    // actual graphics for frames of animations
    patch_t*	p[3]; 

    // following must be initialized to zero before use!

    // next value of bcnt (used in conjunction with period)
    int		nexttic;

    // last drawn animation frame
    int		lastdrawn;

    // next frame number to animate
    int		ctr;
    
    // used by RANDOM and LEVEL when animating
    int		state;  

} interanim_t;


//
// GENERAL DATA
//

//
// Locally used stuff.
//

// States for single-player
#define SP_KILLS		0
#define SP_ITEMS		2
#define SP_SECRET		4
#define SP_FRAGS		6 
#define SP_TIME			8 
#define SP_PAR			ST_TIME

#define SP_PAUSE		1

// in seconds
#define SHOWNEXTLOCDELAY	4
//#define SHOWLASTLOCDELAY	SHOWNEXTLOCDELAY


// used to accelerate or skip a stage
static int		acceleratestage;

// wbs->pnum
static int		me;

 // specifies current state
static stateenum_t	state;

// contains information passed into intermission
static wbstartstruct_t*	wbs;

static wbplayerstruct_t* plrs;  // wbs->plyr[]

// used for general timing
static int 		cnt;  

// used for timing of background animation
static int 		bcnt;

// signals to refresh everything for one frame
static int 		firstrefresh; 

static int		cnt_kills[MAXPLAYERS];
static int		cnt_items[MAXPLAYERS];
static int		cnt_secret[MAXPLAYERS];
static int		cnt_time;
static int		cnt_par;
static int		cnt_pause;

// # of commercial levels
static int		NUMCMAPS; 


//
//	GRAPHICS
//

// %, : graphics
static patch_t*		percent;
static patch_t*		colon;

// 0-9 graphic
static patch_t*		num[10];

// minus sign
static patch_t*		wiminus;

// "Finished!" graphics
static patch_t*		finished;

// "Entering" graphic
static patch_t*		entering; 

// "secret"
static patch_t*		sp_secret;

 // "Kills", "Scrt", "Items", "Frags"
static patch_t*		kills;
static patch_t*		secret;
static patch_t*		items;
static patch_t*		frags;

// Time sucks.
static patch_t*		timepatch;
static patch_t*		par;
static patch_t*		sucks;

// "killers", "victims"
static patch_t*		killers;
static patch_t*		victims; 

// "Total", your face, your dead face
static patch_t*		total;
static patch_t*		star;
static patch_t*		bstar;

// "red P[1..MAXPLAYERS]"
static patch_t*		p[MAXPLAYERS];

// "gray P[1..MAXPLAYERS]"
static patch_t*		bp[MAXPLAYERS];

 // Name graphics of each level (centered)
static patch_t**	lnames;


template< typename _it, typename... _params >
void SetupCache( mapinfo_t* map, _it& itr, _params&... params )
{
	itr.template get< 0 >().Setup( map, itr.template get< 1 >(), params... );
};

typedef class interframecache_s
{
public:
	interframecache_s()
		: source( nullptr )
		, image( nullptr )
	{
	}

	constexpr patch_t* Image()			{ return image; }
	constexpr int32_t Duration()		{ return source->duration; }
	constexpr frametype_t Type()		{ return source->type; }

	template< frametype_t check >
	INLINE bool Is() const				{ return ( source->type & check ) == check; }

	void Setup( mapinfo_t* map, interlevelframe_t& interframe )
	{
		source = &interframe;

		DoomString imagelump = AsString( source->image_lump, map->episode->episode_num - 1, source->lumpname_animindex, source->lumpname_animframe );

		if( imagelump.size() > 0 )
		{
			image = (patch_t*)W_CacheLumpName( imagelump.c_str(), PU_LEVEL );
		}
	}

private:
	interlevelframe_t*					source;
	patch_t*							image;

} interframecache_t;

typedef class interimagecache_s
{
public:
	interimagecache_s()
		: image( nullptr )
	{
	}

	constexpr patch_t* Image()			{ return image; }

	template< typename... _params >
	void Setup( mapinfo_t* map, flowstring_t& lumpname, _params... params )
	{
		imagelump = AsString( lumpname, params... );

		if( imagelump.size() > 0 )
		{
			image = (patch_t*)W_CacheLumpName( imagelump.c_str(), PU_LEVEL );
		}
	}

private:
	DoomString							imagelump;
	patch_t*							image;

} interimagecache_t;

typedef class interanimcache_s
{
public:
	interanimcache_s()
		: source( nullptr )
		, conditionsmet( false )
		, fitsinframegroup( 0 )
	{
	}

	using frame_iterator = DoomVector< interframecache_t, PU_LEVEL >::iterator;
	using anim_container = DoomVector< interanimcache_s, PU_LEVEL >;

	constexpr auto& Frames()			{ return frames; }
	constexpr int32_t XPos()			{ return source->x_pos; }
	constexpr int32_t YPos()			{ return source->y_pos; }
	constexpr auto Conditions()			{ return std::span( source->conditions, source->num_conditions ); }
	constexpr auto ConditionsMet()		{ return conditionsmet; }
	constexpr auto FitsInFrameGroup()	{ return fitsinframegroup; }

	void Setup( mapinfo_t* map, interlevelanim_t& interanim, anim_container& anims )
	{
		source = &interanim;

		frames.resize( source->num_frames );

		auto og_frames = std::span( source->frames, source->num_frames );
	
		for( auto& curr : MultiRangeView( frames, og_frames ) )
		{
			SetupCache( map, curr );
		}

		auto fitsinframe = [ this ]( interframecache_t& frame ) -> bool
		{
			bool fits = true;
			if( frame.Image() != nullptr )
			{
				int32_t left	= XPos() - SHORT( frame.Image()->leftoffset );
				int32_t top		= YPos() - SHORT( frame.Image()->topoffset );
				int32_t right	= left + SHORT( frame.Image()->width );
				int32_t bottom	= top + SHORT( frame.Image()->height );

				fits &= left >= 0 && top >= 0
					&& right < V_VIRTUALWIDTH && bottom < V_VIRTUALHEIGHT;
			}

			return fits;
		};

		auto checkfitsinframegrouping = [ this, &anims ]( int32_t group ) -> bool
		{
			bool passedcheck = true;
			for( auto& curranim : anims )
			{
				if( curranim.FitsInFrameGroup() == group )
				{
					passedcheck &= !curranim.ConditionsMet();
				}
			}

			return passedcheck;
		};

		conditionsmet = frames.size() > 0;
		if( conditionsmet )
		{
			for( auto& cond : Conditions() )
			{
				switch( cond.condition )
				{
				case AnimCondition_MapNumGreater:
					conditionsmet &= ( map->map_num > cond.param );
					break;

				case AnimCondition_MapNumEqual:
					conditionsmet &= ( map->map_num == cond.param );
					break;

				case AnimCondition_MapVisited:
					conditionsmet &= wbs->plyr[ consoleplayer ].visited[ cond.param ];
					break;

				case AnimCondition_MapNotSecret:
					conditionsmet &= ( map->map_flags & Map_Secret ) == Map_None;
					break;

				case AnimCondition_SecretVisited:
					conditionsmet &= wbs->didsecret;
					break;

				case AnimCondition_FitsInFrame:
					if( cond.param > 0 )
					{
						conditionsmet &= checkfitsinframegrouping( cond.param );
					}
					fitsinframegroup = cond.param;

					for( auto& frame : Frames() )
					{
						conditionsmet &= fitsinframe( frame );
					}
					break;

				default:
					break;
				}
			}
		}
	}

private:
	interlevelanim_t*								source;
	DoomVector< interframecache_t, PU_LEVEL >		frames;
	bool											conditionsmet;
	int32_t											fitsinframegroup;

} interanimcache_t;

typedef class interlevelcache_s
{
public:
	interlevelcache_s()
		: source( nullptr )
		, fake_backgroundframe( { } )
		, fake_backgroundanim( { } )
		, fake_background_span( std::span( &fake_background, 1 ) )
	{
	}

	constexpr auto& Background()			{ return fake_background_span; }
	constexpr auto& BackgroundAnims()		{ return background_anims; }
	constexpr auto& ForegroundAnims()		{ return foreground_anims; }

	INLINE auto AllAnims()					{ return Concat( fake_background_span, background_anims, foreground_anims ); }
	INLINE size_t AllAnimsCount()			{ return background_anims.size() + foreground_anims.size() + 1; }

	void Setup( mapinfo_t* map, interlevel_t* interlevel )
	{
		source = interlevel;

		PrimeFakeBackgroundAnim( map );

		background_anims.resize( source->num_background_anims );
		foreground_anims.resize( source->num_foreground_anims );

		auto og_background = std::span( source->background_anims, source->num_background_anims );
		auto og_foreground = std::span( source->foreground_anims, source->num_foreground_anims );

		for( auto curr : MultiRangeView( background_anims, og_background ) )
		{
			SetupCache( map, curr, background_anims );
		}

		for( auto curr : MultiRangeView( foreground_anims, og_foreground ) )
		{
			SetupCache( map, curr, foreground_anims );
		}
	}

private:
	void PrimeFakeBackgroundAnim( mapinfo_t* map )
	{
		fake_backgroundframe.image_lump = source->background_lump;
		fake_backgroundframe.type = Frame_Background;
		fake_backgroundframe.duration = -1;
		fake_backgroundframe.lumpname_animindex = fake_backgroundframe.lumpname_animframe = 0;

		fake_backgroundanim.frames = &fake_backgroundframe;
		fake_backgroundanim.num_frames = 1;
		fake_backgroundanim.x_pos = fake_backgroundanim.y_pos = 0;
		fake_backgroundanim.conditions = NULL;
		fake_backgroundanim.num_conditions = 0;

		fake_background.Setup( map, fake_backgroundanim, background_anims );
	}

	interlevel_t*									source;

	interlevelframe_t								fake_backgroundframe;
	interlevelanim_t								fake_backgroundanim;
	interanimcache_t								fake_background;

	DoomVector< interanimcache_t, PU_LEVEL >		background_anims;
	DoomVector< interanimcache_t, PU_LEVEL >		foreground_anims;
	std::span< interanimcache_t, 1 >				fake_background_span;

} interlevelcache_t;

typedef class wi_animationstate_s
{
public:
	using frame_iterator = interanimcache_t::frame_iterator;

	void Setup( interanimcache_t& cache )
	{
		x_pos = cache.XPos();
		y_pos = cache.YPos();

		frames_begin = frames_curr = cache.Frames().begin();
		frames_end = cache.Frames().end();

		duration_left = frames_curr->Duration();
		if( frames_curr->Is< Frame_RandomStart >() )
		{
			duration_left = 1 + ( M_Random() % ( duration_left - 1 ) );
		}
		else if( !frames_curr->Is< Frame_Infinite >() )
		{
			++duration_left;
		}

		if( frames_curr->Is< Frame_AdjustForWidescreen >() )
		{
			x_pos -= ( ( frames_curr->Image()->width - V_VIRTUALWIDTH ) / 2 );
		}
	}

	void Update()
	{
		if( frames_curr->Is< Frame_Infinite >() )
		{
			return;
		}

		if( duration_left > 0 )
		{
			--duration_left;
		}

		if( duration_left == 0 )
		{
			if( ++frames_curr == frames_end )
			{
				frames_curr = frames_begin;
			}
			duration_left = frames_curr->Duration();
		}
	}

	void Render()
	{
		if( frames_curr->Image() != nullptr )
		{
			V_DrawPatch( x_pos, y_pos, frames_curr->Image() );
		}
	}

private:
	int32_t duration_left;
	int32_t x_pos;
	int32_t y_pos;

	frame_iterator frames_begin;
	frame_iterator frames_end;
	frame_iterator frames_curr;

} wi_animationstate_t;

typedef class wi_animation_s
{
public:
	wi_animation_s()
		: currmap( nullptr )
		, nextmap( nullptr )
		, nextintermission( nullptr )
		, nextendgame( nullptr )
	{
	}

	void Setup( mapinfo_t* map, mapinfo_t* next )
	{
		currmap = map;
		nextmap = next;

		if( currmap->interlevel_finished )
		{
			finished.Setup( currmap, currmap->interlevel_finished );

			SetupAnims( finished, finished_anims );
		}

		if( nextmap && currmap->interlevel_entering )
		{
			entering.Setup( nextmap, currmap->interlevel_entering );

			SetupAnims( entering, entering_anims );
		}
	}

	void Update( bool entering )
	{
		for( auto& anim : entering ? entering_anims : finished_anims )
		{
			anim.Update();
		}
	}

	void Render( bool entering )
	{
		for( auto& anim : entering ? entering_anims : finished_anims )
		{
			anim.Render();
		}
	}

private:
	constexpr interlevelcache_t& Finished() { return finished; }
	constexpr interlevelcache_t& Entering() { return entering; }

	static void SetupAnims( interlevelcache_t& cache, DoomVector< wi_animationstate_t >& anims )
	{
		auto animcache = cache.AllAnims();
		size_t validanims = 0;
		size_t invalidanims = 0;
		for( auto& anim : animcache )
		{
			if( anim.ConditionsMet() )
			{
				++validanims;
			}
			else
			{
				++invalidanims;
			}
		}

		anims.resize( validanims );

		auto outputanim = anims.begin();

		for( auto& anim : animcache )
		{
			if( anim.ConditionsMet() )
			{
				outputanim->Setup( anim );
				++outputanim;
			}
		}
	}

	mapinfo_t*				currmap;
	mapinfo_t*				nextmap;
	intermission_t*			nextintermission;
	endgame_t*				nextendgame;

	interlevelcache_t		finished;
	interlevelcache_t		entering;

	DoomVector< wi_animationstate_t >	finished_anims;
	DoomVector< wi_animationstate_t >	entering_anims;

} wi_animation_t;

static wi_animation_t* animation = NULL;

//
// CODE
//

// The ticker is used to detect keys
//  because of timing issues in netgames.
boolean WI_Responder(event_t* ev)
{
    return false;
}


// Draws "<Levelname> Finished!"
void WI_drawLF(void)
{
    int y = WI_TITLEY;

    if (gamemode != commercial || wbs->last < NUMCMAPS)
    {
        // draw <LevelName> 
        V_DrawPatch((V_VIRTUALWIDTH - SHORT(lnames[wbs->last]->width))/2,
                    y, lnames[wbs->last]);

        // draw "Finished!"
        y += (5*SHORT(lnames[wbs->last]->height))/4;

        V_DrawPatch((V_VIRTUALWIDTH - SHORT(finished->width)) / 2, y, finished);
    }
    else if (wbs->last == NUMCMAPS)
    {
        // MAP33 - draw "Finished!" only
        V_DrawPatch((V_VIRTUALWIDTH - SHORT(finished->width)) / 2, y, finished);
    }
    else if (wbs->last > NUMCMAPS)
    {
        // > MAP33.  Doom bombs out here with a Bad V_DrawPatch error.
        // I'm pretty sure that doom2.exe is just reading into random
        // bits of memory at this point, but let's try to be accurate
        // anyway.  This deliberately triggers a V_DrawPatch error.

        patch_t tmp = { V_VIRTUALWIDTH, V_VIRTUALHEIGHT, 1, 1, 
                        { 0, 0, 0, 0, 0, 0, 0, 0 } };

        V_DrawPatch(0, y, &tmp);
    }
}



// Draws "Entering <LevelName>"
void WI_drawEL(void)
{
    int y = WI_TITLEY;

    // draw "Entering"
    V_DrawPatch((V_VIRTUALWIDTH - SHORT(entering->width))/2,
		y,
                entering);

    // draw level
    y += (5*SHORT(lnames[wbs->next]->height))/4;

    V_DrawPatch((V_VIRTUALWIDTH - SHORT(lnames[wbs->next]->width))/2,
		y, 
                lnames[wbs->next]);

}

//
// Draws a number.
// If digits > 0, then use that many digits minimum,
//  otherwise only use as many as necessary.
// Returns new x position.
//

int
WI_drawNum
( int		x,
  int		y,
  int		n,
  int		digits )
{

    int		fontwidth = SHORT(num[0]->width);
    int		neg;
    int		temp;

    if (digits < 0)
    {
	if (!n)
	{
	    // make variable-length zeros 1 digit long
	    digits = 1;
	}
	else
	{
	    // figure out # of digits in #
	    digits = 0;
	    temp = n;

	    while (temp)
	    {
		temp /= 10;
		digits++;
	    }
	}
    }

    neg = n < 0;
    if (neg)
	n = -n;

    // if non-number, do not draw it
    if (n == 1994)
	return 0;

    // draw the new number
    while (digits--)
    {
	x -= fontwidth;
	V_DrawPatch(x, y, num[ n % 10 ]);
	n /= 10;
    }

    // draw a minus sign if necessary
    if (neg && wiminus)
	V_DrawPatch(x-=8, y, wiminus);

    return x;

}

void
WI_drawPercent
( int		x,
  int		y,
  int		p )
{
    if (p < 0)
	return;

    V_DrawPatch(x, y, percent);
    WI_drawNum(x, y, p, -1);
}



//
// Display level completion time and par,
//  or "sucks" message if overflow.
//
void
WI_drawTime
( int		x,
  int		y,
  int		t )
{

    int		div;
    int		n;

    if (t<0)
	return;

    if (t <= 61*59)
    {
	div = 1;

	do
	{
	    n = (t / div) % 60;
	    x = WI_drawNum(x, y, n, 2) - SHORT(colon->width);
	    div *= 60;

	    // draw
	    if (div==60 || t / div)
		V_DrawPatch(x, y, colon);
	    
	} while (t / div);
    }
    else
    {
	// "sucks"
	V_DrawPatch(x - SHORT(sucks->width), y, sucks); 
    }
}


void WI_End(void)
{
    void WI_unloadData(void);
    WI_unloadData();
}

void WI_initNoState(void)
{
    state = NoState;
    acceleratestage = 0;
    cnt = 10;
}

void WI_updateNoState(void) {

    if (!--cnt)
    {
        // Don't call WI_End yet.  G_WorldDone doesnt immediately 
        // change gamestate, so WI_Drawer is still going to get
        // run until that happens.  If we do that after WI_End
        // (which unloads all the graphics), we're in trouble.
	//WI_End();
	G_WorldDone();
    }

}

static boolean		snl_pointeron = false;


void WI_initShowNextLoc(void)
{
    state = ShowNextLoc;
    acceleratestage = 0;
    cnt = SHOWNEXTLOCDELAY * TICRATE;
}

void WI_updateShowNextLoc(void)
{
    if (!--cnt || acceleratestage)
	WI_initNoState();
    else
	snl_pointeron = (cnt & 31) < 20;
}

void WI_drawShowNextLoc(void)
{

	// Well then
    if ( (gamemode != commercial)
	 || wbs->next != 30)
	WI_drawEL();  

}

void WI_drawNoState(void)
{
    snl_pointeron = true;
    WI_drawShowNextLoc();
}

int WI_fragSum(int playernum)
{
    int		i;
    int		frags = 0;
    
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (playeringame[i]
	    && i!=playernum)
	{
	    frags += plrs[playernum].frags[i];
	}
    }

	
    // JDC hack - negative frags.
    frags -= plrs[playernum].frags[playernum];
    // UNUSED if (frags < 0)
    // 	frags = 0;

    return frags;
}



static int		dm_state;
static int		dm_frags[MAXPLAYERS][MAXPLAYERS];
static int		dm_totals[MAXPLAYERS];



void WI_initDeathmatchStats(void)
{

    int		i;
    int		j;

    state = StatCount;
    acceleratestage = 0;
    dm_state = 1;

    cnt_pause = TICRATE;

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (playeringame[i])
	{
	    for (j=0 ; j<MAXPLAYERS ; j++)
		if (playeringame[j])
		    dm_frags[i][j] = 0;

	    dm_totals[i] = 0;
	}
    }
}



void WI_updateDeathmatchStats(void)
{

    int		i;
    int		j;
    
    boolean	stillticking;

    if (acceleratestage && dm_state != 4)
    {
	acceleratestage = 0;

	for (i=0 ; i<MAXPLAYERS ; i++)
	{
	    if (playeringame[i])
	    {
		for (j=0 ; j<MAXPLAYERS ; j++)
		    if (playeringame[j])
			dm_frags[i][j] = plrs[i].frags[j];

		dm_totals[i] = WI_fragSum(i);
	    }
	}
	

	S_StartSound(0, sfx_barexp);
	dm_state = 4;
    }

    
    if (dm_state == 2)
    {
	if (!(bcnt&3))
	    S_StartSound(0, sfx_pistol);
	
	stillticking = false;

	for (i=0 ; i<MAXPLAYERS ; i++)
	{
	    if (playeringame[i])
	    {
		for (j=0 ; j<MAXPLAYERS ; j++)
		{
		    if (playeringame[j]
			&& dm_frags[i][j] != plrs[i].frags[j])
		    {
			if (plrs[i].frags[j] < 0)
			    dm_frags[i][j]--;
			else
			    dm_frags[i][j]++;

			if (dm_frags[i][j] > 99)
			    dm_frags[i][j] = 99;

			if (dm_frags[i][j] < -99)
			    dm_frags[i][j] = -99;
			
			stillticking = true;
		    }
		}
		dm_totals[i] = WI_fragSum(i);

		if (dm_totals[i] > 99)
		    dm_totals[i] = 99;
		
		if (dm_totals[i] < -99)
		    dm_totals[i] = -99;
	    }
	    
	}
	if (!stillticking)
	{
	    S_StartSound(0, sfx_barexp);
	    dm_state++;
	}

    }
    else if (dm_state == 4)
    {
	if (acceleratestage)
	{
	    S_StartSound(0, sfx_slop);

	    if ( gamemode == commercial)
		WI_initNoState();
	    else
		WI_initShowNextLoc();
	}
    }
    else if (dm_state & 1)
    {
	if (!--cnt_pause)
	{
	    dm_state++;
	    cnt_pause = TICRATE;
	}
    }
}



void WI_drawDeathmatchStats(void)
{

    int		i;
    int		j;
    int		x;
    int		y;
    int		w;

    WI_drawLF();

    // draw stat titles (top line)
    V_DrawPatch(DM_TOTALSX-SHORT(total->width)/2,
		DM_MATRIXY-WI_SPACINGY+10,
		total);
    
    V_DrawPatch(DM_KILLERSX, DM_KILLERSY, killers);
    V_DrawPatch(DM_VICTIMSX, DM_VICTIMSY, victims);

    // draw P?
    x = DM_MATRIXX + DM_SPACINGX;
    y = DM_MATRIXY;

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (playeringame[i])
	{
	    V_DrawPatch(x-SHORT(p[i]->width)/2,
			DM_MATRIXY - WI_SPACINGY,
			p[i]);
	    
	    V_DrawPatch(DM_MATRIXX-SHORT(p[i]->width)/2,
			y,
			p[i]);

	    if (i == me)
	    {
		V_DrawPatch(x-SHORT(p[i]->width)/2,
			    DM_MATRIXY - WI_SPACINGY,
			    bstar);

		V_DrawPatch(DM_MATRIXX-SHORT(p[i]->width)/2,
			    y,
			    star);
	    }
	}
	else
	{
	    // V_DrawPatch(x-SHORT(bp[i]->width)/2,
	    //   DM_MATRIXY - WI_SPACINGY, bp[i]);
	    // V_DrawPatch(DM_MATRIXX-SHORT(bp[i]->width)/2,
	    //   y, bp[i]);
	}
	x += DM_SPACINGX;
	y += WI_SPACINGY;
    }

    // draw stats
    y = DM_MATRIXY+10;
    w = SHORT(num[0]->width);

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	x = DM_MATRIXX + DM_SPACINGX;

	if (playeringame[i])
	{
	    for (j=0 ; j<MAXPLAYERS ; j++)
	    {
		if (playeringame[j])
		    WI_drawNum(x+w, y, dm_frags[i][j], 2);

		x += DM_SPACINGX;
	    }
	    WI_drawNum(DM_TOTALSX+w, y, dm_totals[i], 2);
	}
	y += WI_SPACINGY;
    }
}

static int	cnt_frags[MAXPLAYERS];
static int	dofrags;
static int	ng_state;

void WI_initNetgameStats(void)
{

    int i;

    state = StatCount;
    acceleratestage = 0;
    ng_state = 1;

    cnt_pause = TICRATE;

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (!playeringame[i])
	    continue;

	cnt_kills[i] = cnt_items[i] = cnt_secret[i] = cnt_frags[i] = 0;

	dofrags += WI_fragSum(i);
    }

    dofrags = !!dofrags;
}



void WI_updateNetgameStats(void)
{

    int		i;
    int		fsum;
    
    boolean	stillticking;

    if (acceleratestage && ng_state != 10)
    {
	acceleratestage = 0;

	for (i=0 ; i<MAXPLAYERS ; i++)
	{
	    if (!playeringame[i])
		continue;

	    cnt_kills[i] = (plrs[i].skills * 100) / wbs->maxkills;
	    cnt_items[i] = (plrs[i].sitems * 100) / wbs->maxitems;
	    cnt_secret[i] = (plrs[i].ssecret * 100) / wbs->maxsecret;

	    if (dofrags)
		cnt_frags[i] = WI_fragSum(i);
	}
	S_StartSound(0, sfx_barexp);
	ng_state = 10;
    }

    if (ng_state == 2)
    {
	if (!(bcnt&3))
	    S_StartSound(0, sfx_pistol);

	stillticking = false;

	for (i=0 ; i<MAXPLAYERS ; i++)
	{
	    if (!playeringame[i])
		continue;

	    cnt_kills[i] += 2;

	    if (cnt_kills[i] >= (plrs[i].skills * 100) / wbs->maxkills)
		cnt_kills[i] = (plrs[i].skills * 100) / wbs->maxkills;
	    else
		stillticking = true;
	}
	
	if (!stillticking)
	{
	    S_StartSound(0, sfx_barexp);
	    ng_state++;
	}
    }
    else if (ng_state == 4)
    {
	if (!(bcnt&3))
	    S_StartSound(0, sfx_pistol);

	stillticking = false;

	for (i=0 ; i<MAXPLAYERS ; i++)
	{
	    if (!playeringame[i])
		continue;

	    cnt_items[i] += 2;
	    if (cnt_items[i] >= (plrs[i].sitems * 100) / wbs->maxitems)
		cnt_items[i] = (plrs[i].sitems * 100) / wbs->maxitems;
	    else
		stillticking = true;
	}
	if (!stillticking)
	{
	    S_StartSound(0, sfx_barexp);
	    ng_state++;
	}
    }
    else if (ng_state == 6)
    {
	if (!(bcnt&3))
	    S_StartSound(0, sfx_pistol);

	stillticking = false;

	for (i=0 ; i<MAXPLAYERS ; i++)
	{
	    if (!playeringame[i])
		continue;

	    cnt_secret[i] += 2;

	    if (cnt_secret[i] >= (plrs[i].ssecret * 100) / wbs->maxsecret)
		cnt_secret[i] = (plrs[i].ssecret * 100) / wbs->maxsecret;
	    else
		stillticking = true;
	}
	
	if (!stillticking)
	{
	    S_StartSound(0, sfx_barexp);
	    ng_state += 1 + 2*!dofrags;
	}
    }
    else if (ng_state == 8)
    {
	if (!(bcnt&3))
	    S_StartSound(0, sfx_pistol);

	stillticking = false;

	for (i=0 ; i<MAXPLAYERS ; i++)
	{
	    if (!playeringame[i])
		continue;

	    cnt_frags[i] += 1;

	    if (cnt_frags[i] >= (fsum = WI_fragSum(i)))
		cnt_frags[i] = fsum;
	    else
		stillticking = true;
	}
	
	if (!stillticking)
	{
	    S_StartSound(0, sfx_pldeth);
	    ng_state++;
	}
    }
    else if (ng_state == 10)
    {
	if (acceleratestage)
	{
	    S_StartSound(0, sfx_sgcock);
	    if ( gamemode == commercial )
		WI_initNoState();
	    else
		WI_initShowNextLoc();
	}
    }
    else if (ng_state & 1)
    {
	if (!--cnt_pause)
	{
	    ng_state++;
	    cnt_pause = TICRATE;
	}
    }
}



void WI_drawNetgameStats(void)
{
    int		i;
    int		x;
    int		y;
    int		pwidth = SHORT(percent->width);

    WI_drawLF();

    // draw stat titles (top line)
    V_DrawPatch(NG_STATSX+NG_SPACINGX-SHORT(kills->width),
		NG_STATSY, kills);

    V_DrawPatch(NG_STATSX+2*NG_SPACINGX-SHORT(items->width),
		NG_STATSY, items);

    V_DrawPatch(NG_STATSX+3*NG_SPACINGX-SHORT(secret->width),
		NG_STATSY, secret);
    
    if (dofrags)
	V_DrawPatch(NG_STATSX+4*NG_SPACINGX-SHORT(frags->width),
		    NG_STATSY, frags);

    // draw stats
    y = NG_STATSY + SHORT(kills->height);

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (!playeringame[i])
	    continue;

	x = NG_STATSX;
	V_DrawPatch(x-SHORT(p[i]->width), y, p[i]);

	if (i == me)
	    V_DrawPatch(x-SHORT(p[i]->width), y, star);

	x += NG_SPACINGX;
	WI_drawPercent(x-pwidth, y+10, cnt_kills[i]);	x += NG_SPACINGX;
	WI_drawPercent(x-pwidth, y+10, cnt_items[i]);	x += NG_SPACINGX;
	WI_drawPercent(x-pwidth, y+10, cnt_secret[i]);	x += NG_SPACINGX;

	if (dofrags)
	    WI_drawNum(x, y+10, cnt_frags[i], -1);

	y += WI_SPACINGY;
    }

}

static int	sp_state;

void WI_initStats(void)
{
    state = StatCount;
    acceleratestage = 0;
    sp_state = 1;
    cnt_kills[0] = cnt_items[0] = cnt_secret[0] = -1;
    cnt_time = cnt_par = -1;
    cnt_pause = TICRATE;
}

void WI_updateStats(void)
{
    if (acceleratestage && sp_state != 10)
    {
	acceleratestage = 0;
	cnt_kills[0] = (plrs[me].skills * 100) / wbs->maxkills;
	cnt_items[0] = (plrs[me].sitems * 100) / wbs->maxitems;
	cnt_secret[0] = (plrs[me].ssecret * 100) / wbs->maxsecret;
	cnt_time = plrs[me].stime / TICRATE;
	cnt_par = wbs->partime / TICRATE;
	S_StartSound(0, sfx_barexp);
	sp_state = 10;
    }

    if (sp_state == 2)
    {
	cnt_kills[0] += 2;

	if (!(bcnt&3))
	    S_StartSound(0, sfx_pistol);

	if (cnt_kills[0] >= (plrs[me].skills * 100) / wbs->maxkills)
	{
	    cnt_kills[0] = (plrs[me].skills * 100) / wbs->maxkills;
	    S_StartSound(0, sfx_barexp);
	    sp_state++;
	}
    }
    else if (sp_state == 4)
    {
	cnt_items[0] += 2;

	if (!(bcnt&3))
	    S_StartSound(0, sfx_pistol);

	if (cnt_items[0] >= (plrs[me].sitems * 100) / wbs->maxitems)
	{
	    cnt_items[0] = (plrs[me].sitems * 100) / wbs->maxitems;
	    S_StartSound(0, sfx_barexp);
	    sp_state++;
	}
    }
    else if (sp_state == 6)
    {
	cnt_secret[0] += 2;

	if (!(bcnt&3))
	    S_StartSound(0, sfx_pistol);

	if (cnt_secret[0] >= (plrs[me].ssecret * 100) / wbs->maxsecret)
	{
	    cnt_secret[0] = (plrs[me].ssecret * 100) / wbs->maxsecret;
	    S_StartSound(0, sfx_barexp);
	    sp_state++;
	}
    }

    else if (sp_state == 8)
    {
	if (!(bcnt&3))
	    S_StartSound(0, sfx_pistol);

	cnt_time += 3;

	if (cnt_time >= plrs[me].stime / TICRATE)
	    cnt_time = plrs[me].stime / TICRATE;

	cnt_par += 3;

	if (cnt_par >= wbs->partime / TICRATE)
	{
	    cnt_par = wbs->partime / TICRATE;

	    if (cnt_time >= plrs[me].stime / TICRATE)
	    {
		S_StartSound(0, sfx_barexp);
		sp_state++;
	    }
	}
    }
    else if (sp_state == 10)
    {
	if (acceleratestage)
	{
	    S_StartSound(0, sfx_sgcock);

	    if (gamemode == commercial)
		WI_initNoState();
	    else
		WI_initShowNextLoc();
	}
    }
    else if (sp_state & 1)
    {
	if (!--cnt_pause)
	{
	    sp_state++;
	    cnt_pause = TICRATE;
	}
    }

}

void WI_drawStats(void)
{
    // line height
    int lh;	

    lh = (3*SHORT(num[0]->height))/2;

    WI_drawLF();

    V_DrawPatch(SP_STATSX, SP_STATSY, kills);
    WI_drawPercent(V_VIRTUALWIDTH - SP_STATSX, SP_STATSY, cnt_kills[0]);

    V_DrawPatch(SP_STATSX, SP_STATSY+lh, items);
    WI_drawPercent(V_VIRTUALWIDTH - SP_STATSX, SP_STATSY+lh, cnt_items[0]);

    V_DrawPatch(SP_STATSX, SP_STATSY+2*lh, sp_secret);
    WI_drawPercent(V_VIRTUALWIDTH - SP_STATSX, SP_STATSY+2*lh, cnt_secret[0]);

    V_DrawPatch(SP_TIMEX, SP_TIMEY, timepatch);
    WI_drawTime(V_VIRTUALWIDTH/2 - SP_TIMEX, SP_TIMEY, cnt_time);

    if (wbs->epsd < 3)
    {
        V_DrawPatch(V_VIRTUALWIDTH/2 + SP_TIMEX, SP_TIMEY, par);
        WI_drawTime(V_VIRTUALWIDTH - SP_TIMEX, SP_TIMEY, cnt_par);
    }

}

void WI_checkForAccelerate(void)
{
    int   i;
    player_t  *player;

    // check for button presses to skip delays
    for (i=0, player = players ; i<MAXPLAYERS ; i++, player++)
    {
	if (playeringame[i])
	{
	    if (player->cmd.buttons & BT_ATTACK)
	    {
		if (!player->attackdown)
		    acceleratestage = 1;
		player->attackdown = true;
	    }
	    else
		player->attackdown = false;
	    if (player->cmd.buttons & BT_USE)
	    {
		if (!player->usedown)
		    acceleratestage = 1;
		player->usedown = true;
	    }
	    else
		player->usedown = false;
	}
    }
}



// Updates stuff each tick
void WI_Ticker(void)
{
    // counter for general background animation
    bcnt++;  

    if (bcnt == 1)
    {
	// intermission music
  	if ( gamemode == commercial )
	  S_ChangeMusic(mus_dm2int, true);
	else
	  S_ChangeMusic(mus_inter, true); 
    }

    WI_checkForAccelerate();

	animation->Update( state != StatCount );

    switch (state)
    {
      case StatCount:
	if (deathmatch) WI_updateDeathmatchStats();
	else if (netgame) WI_updateNetgameStats();
	else WI_updateStats();
	break;
	
      case ShowNextLoc:
	WI_updateShowNextLoc();
	break;
	
      case NoState:
	WI_updateNoState();
	break;
    }

}

typedef void (*load_callback_t)(const char *lumpname, patch_t **variable);

// Common load/unload function.  Iterates over all the graphics
// lumps to be loaded/unloaded into memory.

static void WI_loadUnloadData(load_callback_t callback)
{
    int i, j;
    char name[9];
    interanim_t *a;

	if (gamemode == commercial)
	{
		for (i=0 ; i<NUMCMAPS ; i++)
		{
			DEH_snprintf(name, 9, "CWILV%2.2d", i);
				callback(name, &lnames[i]);
		}
	}
	else
	{
		for (i=0 ; i<NUMMAPS ; i++)
		{
			DEH_snprintf(name, 9, "WILV%d%d", wbs->epsd, i);
				callback(name, &lnames[i]);
		}
	}

    // More hacks on minus sign.
    if (W_CheckNumForName(DEH_String("WIMINUS")) > 0)
        callback(DEH_String("WIMINUS"), &wiminus);
    else
        wiminus = NULL;

    for (i=0;i<10;i++)
    {
		 // numbers 0-9
		DEH_snprintf(name, 9, "WINUM%d", i);
			callback(name, &num[i]);
    }

    // percent sign
    callback(DEH_String("WIPCNT"), &percent);

    // "finished"
    callback(DEH_String("WIF"), &finished);

    // "entering"
    callback(DEH_String("WIENTER"), &entering);

    // "kills"
    callback(DEH_String("WIOSTK"), &kills);

    // "scrt"
    callback(DEH_String("WIOSTS"), &secret);

     // "secret"
    callback(DEH_String("WISCRT2"), &sp_secret);

	// french wad uses WIOBJ (?)
	if (W_CheckNumForName(DEH_String("WIOBJ")) >= 0)
	{
		// "items"
		if (netgame && !deathmatch)
			callback(DEH_String("WIOBJ"), &items);
		else
			callback(DEH_String("WIOSTI"), &items);
	}
	else
	{
		callback(DEH_String("WIOSTI"), &items);
	}

    // "frgs"
    callback(DEH_String("WIFRGS"), &frags);

    // ":"
    callback(DEH_String("WICOLON"), &colon);

    // "time"
    callback(DEH_String("WITIME"), &timepatch);

    // "sucks"
    callback(DEH_String("WISUCKS"), &sucks);

    // "par"
    callback(DEH_String("WIPAR"), &par);

    // "killers" (vertical)
    callback(DEH_String("WIKILRS"), &killers);

    // "victims" (horiz)
    callback(DEH_String("WIVCTMS"), &victims);

    // "total"
    callback(DEH_String("WIMSTT"), &total);

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
		// "1,2,3,4"
		DEH_snprintf(name, 9, "STPB%d", i);
			callback(name, &p[i]);

		// "1,2,3,4"
		DEH_snprintf(name, 9, "WIBP%d", i+1);
			callback(name, &bp[i]);
    }

}

static void WI_loadCallback(const char *name, patch_t **variable)
{
    *variable = (patch_t*)W_CacheLumpName(name, PU_LEVEL);
}

void WI_loadData(void)
{
    if (gamemode == commercial)
    {
	NUMCMAPS = 32;
	lnames = (patch_t **) Z_Malloc(sizeof(patch_t*) * NUMCMAPS,
				       PU_LEVEL, NULL);
    }
    else
    {
	lnames = (patch_t **) Z_Malloc(sizeof(patch_t*) * NUMMAPS,
				       PU_LEVEL, NULL);
    }

    WI_loadUnloadData(WI_loadCallback);

    // These two graphics are special cased because we're sharing
    // them with the status bar code

    // your face
    star = (patch_t*)W_CacheLumpName(DEH_String("STFST01"), PU_STATIC);

    // dead face
    bstar = (patch_t*)W_CacheLumpName(DEH_String("STFDEAD0"), PU_STATIC);
}

static void WI_unloadCallback(const char *name, patch_t **variable)
{
    //W_ReleaseLumpName(name);
    *variable = NULL;
}

void WI_unloadData(void)
{
    WI_loadUnloadData(WI_unloadCallback);

    // We do not free these lumps as they are shared with the status
    // bar code.
   
    // W_ReleaseLumpName("STFST01");
    // W_ReleaseLumpName("STFDEAD0");
}

void WI_Drawer (void)
{
	V_FillBorder( &blackedges, 0, V_VIRTUALHEIGHT );
	animation->Render( state != StatCount );

    switch (state)
    {
      case StatCount:
	if (deathmatch)
	    WI_drawDeathmatchStats();
	else if (netgame)
	    WI_drawNetgameStats();
	else
	    WI_drawStats();
	break;
	
      case ShowNextLoc:
	WI_drawShowNextLoc();
	break;
	
      case NoState:
	WI_drawNoState();
	break;
    }
}


void WI_initVariables(wbstartstruct_t* wbstartstruct)
{

    wbs = wbstartstruct;

#ifdef RANGECHECKING
    if (gamemode != commercial)
    {
      if (gameversion >= exe_ultimate)
	RNGCHECK(wbs->epsd, 0, 3);
      else
	RNGCHECK(wbs->epsd, 0, 2);
    }
    else
    {
	RNGCHECK(wbs->last, 0, 8);
	RNGCHECK(wbs->next, 0, 8);
    }
    RNGCHECK(wbs->pnum, 0, MAXPLAYERS);
    RNGCHECK(wbs->pnum, 0, MAXPLAYERS);
#endif

    acceleratestage = 0;
    cnt = bcnt = 0;
    firstrefresh = 1;
    me = wbs->pnum;
    plrs = wbs->plyr;

    if (!wbs->maxkills)
	wbs->maxkills = 1;

    if (!wbs->maxitems)
	wbs->maxitems = 1;

    if (!wbs->maxsecret)
	wbs->maxsecret = 1;

    if ( gameversion < exe_ultimate )
      if (wbs->epsd > 2)
	wbs->epsd -= 3;
}

void WI_Start(wbstartstruct_t* wbstartstruct)
{
    WI_initVariables(wbstartstruct);
    WI_loadData();

	animation = Z_Malloc< wi_animation_t >( PU_LEVEL, NULL );
	animation->Setup( (mapinfo_t*)wbs->currmap, (mapinfo_t*)wbs->nextmap );

    if (deathmatch)
	WI_initDeathmatchStats();
    else if (netgame)
	WI_initNetgameStats();
    else
	WI_initStats();
}
