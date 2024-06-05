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
//	Map Objects, MObj, definition and handling.
//


#ifndef __P_MOBJ__
#define __P_MOBJ__

// Basics.
#include "tables.h"
#include "m_fixed.h"

// We need the thinker_t stuff.
#include "d_think.h"
#include "d_gamesim.h"

// We need the WAD data structure for Map things,
// from the THINGS lump.
#include "doomdata.h"

// States are tied to finite states are
//  tied to animation frames.
// Needs precompiled tables/data structures.
#include "info.h"






//
// NOTES: mobj_t
//
// mobj_ts are used to tell the refresh where to draw an image,
// tell the world simulation when objects are contacted,
// and tell the sound driver how to position a sound.
//
// The refresh uses the next and prev links to follow
// lists of things in sectors as they are being drawn.
// The sprite, frame, and angle elements determine which patch_t
// is used to draw the sprite if it is visible.
// The sprite and frame values are allmost allways set
// from state_t structures.
// The statescr.exe utility generates the states.h and states.c
// files that contain the sprite/frame numbers from the
// statescr.txt source file.
// The xyz origin point represents a point at the bottom middle
// of the sprite (between the feet of a biped).
// This is the default origin position for patch_ts grabbed
// with lumpy.exe.
// A walking creature will have its z equal to the floor
// it is standing on.
//
// The sound code uses the x,y, and subsector fields
// to do stereo positioning of any sound effited by the mobj_t.
//
// The play simulation uses the blocklinks, x,y,z, radius, height
// to determine when mobj_ts are touching each other,
// touching lines in the map, or hit by trace lines (gunshots,
// lines of sight, etc).
// The mobj_t->flags element has various bit flags
// used by the simulation.
//
// Every mobj_t is linked into a single sector
// based on its origin coordinates.
// The subsector_t is found with BSP_PointInSubsector(x,y),
// and the sector_t can be found with subsector->sector.
// The sector links are only used by the rendering code,
// the play simulation does not care about them at all.
//
// Any mobj_t that needs to be acted upon by something else
// in the play world (block movement, be shot, etc) will also
// need to be linked into the blockmap.
// If the thing has the MF_NOBLOCK flag set, it will not use
// the block links. It can still interact with other things,
// but only as the instigator (missiles will run into other
// things, but nothing can run into a missile).
// Each block in the grid is 128*128 units, and knows about
// every line_t that it contains a piece of, and every
// interactable mobj_t that has its origin contained.  
//
// A valid mobj_t is a mobj_t that has the proper subsector_t
// filled in for its xy coordinates and is linked into the
// sector from which the subsector was made, or has the
// MF_NOSECTOR flag set (the subsector_t needs to be valid
// even if MF_NOSECTOR is set), and is linked into a blockmap
// block or has the MF_NOBLOCKMAP flag set.
// Links should only be modified by the P_[Un]SetThingPosition()
// functions.
// Do not change the MF_NO? flags while a thing is valid.
//
// Any questions?
//

#include "d_think.h"

//
// Misc. mobj flags
//
DOOM_C_API typedef enum
{
    // Call P_SpecialThing when touched.
    MF_SPECIAL		= 1,
    // Blocks.
    MF_SOLID		= 2,
    // Can be hit.
    MF_SHOOTABLE	= 4,
    // Don't use the sector links (invisible but touchable).
    MF_NOSECTOR		= 8,
    // Don't use the blocklinks (inert but displayable)
    MF_NOBLOCKMAP	= 16,                    

    // Not to be activated by sound, deaf monster.
    MF_AMBUSH		= 32,
    // Will try to attack right back.
    MF_JUSTHIT		= 64,
    // Will take at least one step before attacking.
    MF_JUSTATTACKED	= 128,
    // On level spawning (initial position),
    //  hang from ceiling instead of stand on floor.
    MF_SPAWNCEILING	= 256,
    // Don't apply gravity (every tic),
    //  that is, object will float, keeping current height
    //  or changing it actively.
    MF_NOGRAVITY	= 512,

    // Movement flags.
    // This allows jumps from high places.
    MF_DROPOFF		= 0x400,
    // For players, will pick up items.
    MF_PICKUP		= 0x800,
    // Player cheat. ???
    MF_NOCLIP		= 0x1000,
    // Player: keep info about sliding along walls.
    MF_SLIDE		= 0x2000,
    // Allow moves to any height, no gravity.
    // For active floaters, e.g. cacodemons, pain elementals.
    MF_FLOAT		= 0x4000,
    // Don't cross lines
    //   ??? or look at heights on teleport.
    MF_TELEPORT		= 0x8000,
    // Don't hit same species, explode on block.
    // Player missiles as well as fireballs of various kinds.
    MF_MISSILE		= 0x10000,	
    // Dropped by a demon, not level spawned.
    // E.g. ammo clips dropped by dying former humans.
    MF_DROPPED		= 0x20000,
    // Use fuzzy draw (shadow demons or spectres),
    //  temporary player invisibility powerup.
    MF_SHADOW		= 0x40000,
    // Flag: don't bleed when shot (use puff),
    //  barrels and shootable furniture shall not bleed.
    MF_NOBLOOD		= 0x80000,
    // Don't stop moving halfway off a step,
    //  that is, have dead bodies slide down all the way.
    MF_CORPSE		= 0x100000,
    // Floating to a height for a move, ???
    //  don't auto float to target's height.
    MF_INFLOAT		= 0x200000,

    // On kill, count this enemy object
    //  towards intermission kill total.
    // Happy gathering.
    MF_COUNTKILL	= 0x400000,
    
    // On picking up, count this item object
    //  towards intermission item total.
    MF_COUNTITEM	= 0x800000,

    // Special handling: skull in flight.
    // Neither a cacodemon nor a missile.
    MF_SKULLFLY		= 0x1000000,

    // Don't spawn this object
    //  in death match mode (e.g. key cards).
    MF_NOTDMATCH    	= 0x2000000,

    // Player sprites in multiplayer modes are modified
    //  using an internal color lookup table for re-indexing.
    // If 0x4 0x8 or 0xc,
    //  use a translation table for player colormaps
    MF_TRANSLATION  	= 0xc000000,
    // Hmm ???.
    MF_TRANSSHIFT		= 26,

	// MBF_Extensions
	MF_MBF_TOUCHY		= 0x10000000,

	MF_MBF_BOUNCES		= 0x20000000,

	MF_MBF_FRIEND		= 0x40000000,

	// Boom extensions
	MF_BOOM_TRANSLUCENT	= 0x80000000,
 } mobjflag_t;

DOOM_C_API typedef enum mobjflag2_e
{
	MF2_MBF21_LOGRAV			= 0x00001,	// Lower gravity (1/8)
	MF2_MBF21_SHORTMRANGE 		= 0x00002,	// Short missile range (archvile)
	MF2_MBF21_DMGIGNORED		= 0x00004,	// Other things ignore its attacks (archvile)
	MF2_MBF21_NORADIUSDMG		= 0x00008,	// Doesn't take splash damage (cyberdemon, mastermind)
	MF2_MBF21_FORCERADIUSDMG	= 0x00010,	// Thing causes splash damage even if the target shouldn't
	MF2_MBF21_HIGHERMPROB		= 0x00020,	// Higher missile attack probability (cyberdemon)
	MF2_MBF21_RANGEHALF			= 0x00040,	// Use half distance for missile attack probability (cyberdemon, mastermind, revenant, lost soul)
	MF2_MBF21_NOTHRESHOLD		= 0x00080,	// Has no targeting threshold (archvile)
	MF2_MBF21_LONGMELEE			= 0x00100,	// Has long melee range (revenant)
	MF2_MBF21_BOSS				= 0x00200,	// Full volume see / death sound & splash immunity (from heretic)
	MF2_MBF21_MAP07BOSS1		= 0x00400,	// Tag 666 "boss" on doom 2 map 7 (mancubus)
	MF2_MBF21_MAP07BOSS2		= 0x00800,	// Tag 667 "boss" on doom 2 map 7 (arachnotron)
	MF2_MBF21_E1M8BOSS			= 0x01000,	// E1M8 boss (baron)
	MF2_MBF21_E2M8BOSS			= 0x02000,	// E2M8 boss (cyberdemon)
	MF2_MBF21_E3M8BOSS			= 0x04000,	// E3M8 boss (mastermind)
	MF2_MBF21_E4M6BOSS			= 0x08000,	// E4M6 boss (cyberdemon)
	MF2_MBF21_E4M8BOSS			= 0x10000,	// E4M8 boss (mastermind)
	MF2_MBF21_RIP				= 0x20000,	// Ripper projectile (does not disappear on impact)
	MF2_MBF21_FULLVOLSOUNDS		= 0x40000,	// Full volume see / death sounds (cyberdemon, mastermind)
} mobjflag2_t;

DOOM_C_API typedef enum mobjrnr24flag_e
{
	MF_RNR24_NORESPAWN			= 0x00000001,	// Disables nightmare respawns
} mobjrnr24flag_t;

DOOM_C_API typedef struct mobjinstance_s
{
	rend_fixed_t			x;
	rend_fixed_t			y;
	rend_fixed_t			z;
	angle_t					angle;
	spritenum_t				sprite;
	int32_t					frame;
	int32_t					teleported;
} mobjinstance_t;

#define MAX_SECTOR_OVERLAPS 32

// Map Object definition.
DOOM_C_API typedef struct mobj_s
{
    // List: thinker links.
    thinker_t		thinker;

    // Info for drawing: position.
    fixed_t			x;
    fixed_t			y;
    fixed_t			z;

    // More list: links in sector (if needed)
    struct mobj_s*	snext;
    struct mobj_s*	sprev;

	struct mobj_s*	nosectornext;
	struct mobj_s*	nosectorprev;

    //More drawing info: to determine current sprite.
    angle_t			angle;	// orientation
    spritenum_t		sprite;	// used to find patch_t and flip value
    int				frame;	// might be ORed with FF_FULLBRIGHT

    // Interaction info, by BLOCKMAP.
    // Links in blocks (if needed).
    struct mobj_s*	bnext;
    struct mobj_s*	bprev;
    
    struct subsector_s*	subsector;

    // The closest interval over all contacted Sectors.
    fixed_t			floorz;
    fixed_t			ceilingz;

    // For movement checking.
    fixed_t			radius;
    fixed_t			height;	

    // Momentums, used to update position.
    fixed_t			momx;
    fixed_t			momy;
    fixed_t			momz;

    // If == validcount, already checked.
    int				validcount;

    int32_t			type;
    const mobjinfo_t* info;	// &mobjinfo[mobj->type]
    
    int				tics;	// state tic counter
    const state_t* state;
    int				flags;
    int				health;

    // Movement direction, movement generation (zig-zagging).
    int				movedir;	// 0-7
    int				movecount;	// when 0, select a new dir

    // Thing being chased/attacked (or NULL),
    // also the originator for missiles.
    struct mobj_s*	target;

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    int				reactiontime;

    // If >0, the target will be chased
    // no matter what (even if shot)
    int				threshold;

    // Additional info record for player avatars only.
    // Only valid if type == MT_PLAYER
    struct player_s*	player;

    // Player number last looked for.
    int				lastlook;

    // For nightmare respawn.
    mapthing_t		spawnpoint;
	int32_t			lumpindex;
	doombool		hasspawnpoint;

    // Thing being chased/attacked for tracers.
    struct mobj_s*	tracer;	

	int32_t			flags2;
	int32_t			rnr24flags;

	uint64_t		teleporttic;

	int32_t			resurrection_count;
	int32_t			successfullineeffect;

	// For rendering purposes
	mobjinstance_t		curr;
	mobjinstance_t		prev;
	uint8_t*			tested_sector;
	uint8_t				rendered[ JOBSYSTEM_MAXJOBS ];
	struct sector_s*	overlaps[ MAX_SECTOR_OVERLAPS ];
	int32_t				numoverlaps;

#if defined( __cplusplus )
	INLINE const bool IsFriendly() const					{ return sim.mbf_mobj_flags && ( flags & MF_MBF_FRIEND ); }

	INLINE const bool ShortMissileRange() const				{ return sim.mbf21_thing_extensions && ( flags2 & MF2_MBF21_SHORTMRANGE ); }
	INLINE const bool LongMeleeRange() const				{ return sim.mbf21_thing_extensions && ( flags2 & MF2_MBF21_LONGMELEE ); }
	INLINE const bool MissileHalfRangeProbability() const	{ return sim.mbf21_thing_extensions && ( flags2 & MF2_MBF21_RANGEHALF ); }
	INLINE const bool MissileHigherProbability() const		{ return sim.mbf21_thing_extensions && ( flags2 & MF2_MBF21_HIGHERMPROB ); }
	INLINE const bool FullVolumeSounds() const				{ return sim.mbf21_thing_extensions && ( flags2 & ( MF2_MBF21_FULLVOLSOUNDS | MF2_MBF21_BOSS ) ); }
	INLINE const bool MonstersIgnoreDamage() const			{ return sim.mbf21_thing_extensions && ( flags2 & MF2_MBF21_DMGIGNORED ); }
	INLINE const bool NoTargetingThreshold() const			{ return sim.mbf21_thing_extensions && ( flags2 & MF2_MBF21_NOTHRESHOLD ); }
	INLINE const bool ForceSplashDamage() const				{ return sim.mbf21_thing_extensions && ( flags2 & MF2_MBF21_FORCERADIUSDMG ); }
	INLINE const bool NoSplashDamage() const				{ return sim.mbf21_thing_extensions && ( flags2 & ( MF2_MBF21_NORADIUSDMG | MF2_MBF21_BOSS ) ); }
	INLINE const bool LowGravity() const					{ return sim.mbf21_thing_extensions && ( flags2 & MF2_MBF21_LOGRAV ); }

	INLINE const fixed_t Speed() const						{ return fastmonsters && info->fastspeed ? info->fastspeed : info->speed; }
	INLINE const fixed_t MeleeRange() const					{ return info->meleerange ? info->meleerange : IntToFixed( 64 ); }
	INLINE const int32_t MinRespawnTics() const				{ return !sim.rnr24_thing_extensions ? 12 * TICRATE
																		: info->minrespawntics < 0 ? INT_MAX
																		: info->minrespawntics > 0 ? info->minrespawntics
																		: 12 * TICRATE; }
	INLINE const bool NoRespawn() const						{ return sim.rnr24_thing_extensions && ( rnr24flags & MF_RNR24_NORESPAWN ); }
	INLINE const int32_t RespawnDice() const				{ return sim.rnr24_thing_extensions && info->respawndice ? info->respawndice
																		: 4; }
#endif
} mobj_t;

#if defined( __cplusplus )

#include "d_think.h"

DOOM_C_API void P_MobjThinker (mobj_t* mobj);
MakeThinkFuncLookup( mobj_t, P_MobjThinker );

#endif // defined( __cplusplus )

#endif
