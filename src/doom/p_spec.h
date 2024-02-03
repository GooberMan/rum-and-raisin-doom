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
//	Implements special effects:
//	Texture animation, height or lighting changes
//	 according to adjacent sectors, respective
//	 utility functions, etc.
//


#ifndef __P_SPEC__
#define __P_SPEC__

#include "r_defs.h"

#define FLATSCROLL_SCALE 0x800 // (1/32)
//
// End-level timer (-TIMER option)
//
DOOM_C_API extern	doombool levelTimer;
DOOM_C_API extern	int	levelTimeCount;


//      Define values for map objects
#define MO_TELEPORTMAN          14


// at game start
DOOM_C_API void    P_InitPicAnims (void);

// will return -1 if this is not the start of an animation
DOOM_C_API int32_t P_GetPicAnimStart( doombool istexture, int32_t animframe );
DOOM_C_API int32_t P_GetPicAnimLength( doombool istexture, int32_t start );

DOOM_C_API int32_t P_GetPicSwitchOpposite( int32_t tex );

// at map load
DOOM_C_API void    P_SpawnSpecials (void);

// every tic
DOOM_C_API void    P_UpdateSpecials (void);

// when needed
DOOM_C_API doombool P_UseSpecialLine( mobj_t* thing, line_t* line, int side );

DOOM_C_API void P_ShootSpecialLine( mobj_t* thing, line_t* line );

DOOM_C_API void P_CrossSpecialLine( line_t* line, int side, mobj_t* thing );

DOOM_C_API void    P_PlayerInSpecialSector (player_t* player);

// These should be inlined
DOOM_C_API int twoSided( int sector, int line );
DOOM_C_API sector_t* getSector( int currentSector, int line, int side );
DOOM_C_API side_t* getSide( int currentSector, int line, int side );

DOOM_C_API fixed_t P_FindLowestFloorSurrounding(sector_t* sec);
DOOM_C_API fixed_t P_FindHighestFloorSurrounding(sector_t* sec);
DOOM_C_API fixed_t P_FindNextLowestFloorSurrounding( sector_t* sec );
DOOM_C_API fixed_t P_FindNextHighestFloorSurrounding( sector_t* sec );

DOOM_C_API fixed_t P_FindNextHighestFloor( sector_t* sec );

DOOM_C_API fixed_t P_FindLowestCeilingSurrounding(sector_t* sec);
DOOM_C_API fixed_t P_FindHighestCeilingSurrounding(sector_t* sec);
DOOM_C_API fixed_t P_FindNextLowestCeilingSurrounding(sector_t* sec);

DOOM_C_API int P_FindSectorFromLineTag( line_t* line, int start );

DOOM_C_API int P_FindMinSurroundingLight( sector_t* sector, int max );

DOOM_C_API sector_t* getNextSector( line_t* line, sector_t* sec );


//
// SPECIAL
//
DOOM_C_API int EV_DoDonut(line_t* line);



//
// P_LIGHTS
//
DOOM_C_API typedef struct
{
    thinker_t	thinker;
    sector_t*	sector;
    int		count;
    int		maxlight;
    int		minlight;
    
} fireflicker_t;



DOOM_C_API typedef struct
{
    thinker_t	thinker;
    sector_t*	sector;
    int		count;
    int		maxlight;
    int		minlight;
    int		maxtime;
    int		mintime;
    
} lightflash_t;



DOOM_C_API typedef struct
{
    thinker_t	thinker;
    sector_t*	sector;
    int		count;
    int		minlight;
    int		maxlight;
    int		darktime;
    int		brighttime;
    
} strobe_t;




DOOM_C_API typedef struct
{
    thinker_t	thinker;
    sector_t*	sector;
    int		minlight;
    int		maxlight;
    int		direction;

} glow_t;


#define GLOWSPEED			8
#define STROBEBRIGHT		5
#define FASTDARK			15
#define SLOWDARK			35

DOOM_C_API void    P_SpawnFireFlicker (sector_t* sector);
DOOM_C_API void    T_LightFlash (lightflash_t* flash);
DOOM_C_API void    P_SpawnLightFlash (sector_t* sector);
DOOM_C_API void    T_StrobeFlash (strobe_t* flash);

DOOM_C_API void		P_SpawnStrobeFlash( sector_t* sector, int fastOrSlow, int inSync );

DOOM_C_API void    EV_StartLightStrobing(line_t* line);
DOOM_C_API void    EV_TurnTagLightsOff(line_t* line);

DOOM_C_API void EV_LightTurnOn( line_t* line, int bright );

DOOM_C_API void    T_Glow(glow_t* g);
DOOM_C_API void    P_SpawnGlowingLight(sector_t* sector);




//
// P_SWITCH
//
DOOM_C_API typedef struct
{
    char	name1[9];
    char	name2[9];
    short	episode;
    
} switchlist_t;


DOOM_C_API typedef enum
{
    top,
    middle,
    bottom

} bwhere_e;


DOOM_C_API typedef struct
{
	line_t*			line;
	bwhere_e		where;
	int				btexture;
	int				btimer;
	degenmobj_t*	soundorg;
} button_t;


 // max # of wall switches in a level
#define MAXSWITCHES		50

 // 4 players, 4 buttons each at once, max.
#define MAXBUTTONS		16

 // 1 second, in ticks. 
#define BUTTONTIME      35

DOOM_C_API extern button_t	buttonlist[MAXBUTTONS]; 

DOOM_C_API void P_ChangeSwitchTexture( line_t* line, int useAgain );

DOOM_C_API void P_InitSwitchList(void);


//
// P_PLATS
//
DOOM_C_API typedef enum
{
    up,
    down,
    waiting,
    in_stasis

} plat_e;



DOOM_C_API typedef enum
{
    perpetualRaise,
    downWaitUpStay,
    raiseAndChange,
    raiseToNearestAndChange,
    blazeDWUS
} plattype_e;

DOOM_C_API typedef struct platdata_e
{
	thinker_t	thinker;
	sector_t*	sector;
	fixed_t		speed;
	fixed_t		low;
	fixed_t		high;
	int			wait;
	int			count;
	plat_e		status;
	plat_e		oldstatus;
	doombool	crush;
	int			tag;
	plattype_e	type;

	// Limit removing linked list
	struct platdata_e*	prevactive;
	struct platdata_e*  nextactive;
} plat_t;



#define PLATWAIT		3
#define PLATSPEED		FRACUNIT
#define MAXPLATS		30


DOOM_C_API extern plat_t*	activeplats[MAXPLATS];
DOOM_C_API extern plat_t*	activeplatshead;

DOOM_C_API void    T_PlatRaise(plat_t*	plat);

DOOM_C_API int EV_DoPlat( line_t* line, plattype_e type, int amount );

DOOM_C_API void    P_AddActivePlat(plat_t* plat);
DOOM_C_API void    P_RemoveActivePlat(plat_t* plat);
DOOM_C_API void    EV_StopPlat(line_t* line);
DOOM_C_API void    P_ActivateInStasis(int tag);


//
// P_DOORS
//
DOOM_C_API typedef enum
{
    vld_normal,
    vld_close30ThenOpen,
    vld_close,
    vld_open,
    vld_raiseIn5Mins,
    vld_blazeRaise,
    vld_blazeOpen,
    vld_blazeClose

} vldoor_e;

DOOM_C_API typedef enum sectordir_e
{
	sd_down = -1,
	sd_none = 0,
	sd_up = 1,

	sd_close = sd_down,
	sd_open = sd_up,
	sd_raisein5nonsense = 2,
} sectordir_t;

DOOM_C_API typedef struct
{
	thinker_t		thinker;
	vldoor_e		type;
	sector_t*		sector;
	fixed_t			topheight;
	fixed_t			speed;

	// 1 = up, 0 = waiting at top, -1 = down
	sectordir_t		direction;

	// tics to wait at the top
	int32_t			topwait;
	// (keep in case a door going down is reset)
	// when it reaches 0, start going down
	int32_t			topcountdown;

	// Generic door extensions
	sectordir_t		nextdirection;
	doombool		blazing;
	doombool		keepclosingoncrush;
	doombool		dontrecloseoncrush;
	
	int32_t			lighttag;
	int32_t			lightmin;
	int32_t			lightmax;
} vldoor_t;



#define VDOORSPEED		FRACUNIT*2
#define VDOORWAIT		150

DOOM_C_API doombool EV_VerticalDoor( line_t* line, mobj_t* thing );

DOOM_C_API int EV_DoDoor( line_t* line, vldoor_e type );
DOOM_C_API int EV_DoLockedDoor( line_t* line, vldoor_e type, mobj_t* thing );

DOOM_C_API void    T_VerticalDoor (vldoor_t* door);
DOOM_C_API void    P_SpawnDoorCloseIn30 (sector_t* sec);

DOOM_C_API void P_SpawnDoorRaiseIn5Mins( sector_t* sec, int secnum );



#if 0 // UNUSED
//
//      Sliding doors...
//
typedef enum
{
    sd_opening,
    sd_waiting,
    sd_closing

} sd_e;



typedef enum
{
    sdt_openOnly,
    sdt_closeOnly,
    sdt_openAndClose

} sdt_e;




typedef struct
{
    thinker_t	thinker;
    sdt_e	type;
    line_t*	line;
    int		frame;
    int		whichDoorIndex;
    int		timer;
    sector_t*	frontsector;
    sector_t*	backsector;
    sd_e	 status;

} slidedoor_t;



typedef struct
{
    char	frontFrame1[9];
    char	frontFrame2[9];
    char	frontFrame3[9];
    char	frontFrame4[9];
    char	backFrame1[9];
    char	backFrame2[9];
    char	backFrame3[9];
    char	backFrame4[9];
    
} slidename_t;



typedef struct
{
    int             frontFrames[4];
    int             backFrames[4];

} slideframe_t;



// how many frames of animation
#define SNUMFRAMES		4

#define SDOORWAIT		35*3
#define SWAITTICS		4

// how many diff. types of anims
#define MAXSLIDEDOORS	5                            

void P_InitSlidingDoorFrames(void);

void
EV_SlidingDoor
( line_t*	line,
  mobj_t*	thing );
#endif



//
// P_CEILNG
//
DOOM_C_API typedef enum
{
    lowerToFloor,
    raiseToHighest,
    lowerAndCrush,
    crushAndRaise,
    fastCrushAndRaise,
    silentCrushAndRaise,
	genericCeiling
} ceiling_e;



DOOM_C_API typedef struct ceilingdata_e
{
	thinker_t	thinker;
	ceiling_e	type;
	sector_t*	sector;
	fixed_t		bottomheight;
	fixed_t		topheight;
	fixed_t		speed;
	doombool	crush;

	// 1 = up, 0 = waiting, -1 = down
	int			direction;

	// ID
	int			tag;
	int			olddirection;

	// Generic functionality
	struct ceilingdata_e*	nextactive;
	struct ceilingdata_e*	prevactive;
	fixed_t					normalspeed;
	fixed_t					crushingspeed;
	int16_t					newspecial;
	int16_t					newtexture;
	int32_t					sound;
	doombool				perpetual;
} ceiling_t;





#define CEILSPEED		FRACUNIT
#define CEILWAIT		150
#define MAXCEILINGS		30

DOOM_C_API extern ceiling_t*	activeceilings[MAXCEILINGS];
DOOM_C_API extern ceiling_t*	activeceilingshead;

DOOM_C_API int EV_DoCeiling( line_t* line, ceiling_e type );

DOOM_C_API void    T_MoveCeiling (ceiling_t* ceiling);
DOOM_C_API void    P_AddActiveCeiling(ceiling_t* c);
DOOM_C_API void    P_RemoveActiveCeiling(ceiling_t* c);
DOOM_C_API int	EV_CeilingCrushStop(line_t* line);
DOOM_C_API void    P_ActivateInStasisCeiling(line_t* line);


//
// P_FLOOR
//
DOOM_C_API typedef enum
{
    // lower floor to highest surrounding floor
    lowerFloor,
    
    // lower floor to lowest surrounding floor
    lowerFloorToLowest,
    
    // lower floor to highest surrounding floor VERY FAST
    turboLower,
    
    // raise floor to lowest surrounding CEILING
    raiseFloor,
    
    // raise floor to next highest surrounding floor
    raiseFloorToNearest,

    // raise floor to shortest height texture around it
    raiseToTexture,
    
    // lower floor to lowest surrounding floor
    //  and change floorpic
    lowerAndChange,
  
    raiseFloor24,
    raiseFloor24AndChange,
    raiseFloorCrush,

     // raise to next highest floor, turbo-speed
    raiseFloorTurbo,       
    donutRaise,
    raiseFloor512,
    
	genericFloor
} floor_e;




DOOM_C_API typedef enum
{
    build8,	// slowly build by 8
    turbo16	// quickly build by 16
    
} stair_e;



DOOM_C_API typedef struct
{
    thinker_t	thinker;
    floor_e	type;
    doombool	crush;
    sector_t*	sector;
    int32_t		direction;
    int16_t		newspecial;
    int16_t		texture;
    fixed_t		floordestheight;
    fixed_t		speed;

} floormove_t;



#define FLOORSPEED		FRACUNIT

DOOM_C_API typedef enum
{
    ok,
    crushed,
    pastdest
} result_e;

DOOM_C_API result_e T_MovePlane( sector_t* sector, fixed_t speed,  fixed_t dest, doombool crush, int floorOrCeiling, int direction );

DOOM_C_API int EV_BuildStairs( line_t* line, stair_e type );

DOOM_C_API int EV_DoFloor( line_t* line, floor_e floortype );

DOOM_C_API void T_MoveFloor( floormove_t* floor);

//
// P_TELEPT
//
DOOM_C_API int EV_Teleport( line_t*	line, int side, mobj_t* thing );

// Generic functionality

DOOM_C_API typedef struct elevator_e
{
	thinker_t	thinker;
	sector_t*	sector;
	fixed_t		speed;
	fixed_t		floordest;
	fixed_t		ceilingdest;
	sectordir_t	direction;
} elevator_t;

DOOM_C_API typedef enum dooraise_e
{
	door_noraise,
	door_raiselower,
} doorraise_t;

DOOM_C_API typedef enum sectortargettype_e
{
	stt_nosearch = 0,
	stt_highestneighborfloor,
	stt_lowestneighborfloor,
	stt_nexthighestneighborfloor,
	stt_nextlowestneighborfloor,
	stt_highestneighborceiling,
	stt_lowestneighborceiling,
	stt_nextlowestneighborceiling,
	stt_floor,
	stt_ceiling,
	stt_shortestlowertexture,
	stt_perpetual,
	stt_highestneighborfloor_noaddifmatch,	// Vanilla behavior, just like stt_highestneighborfloor but
											// won't add the extra value specified if dest == source
	stt_lineactivator,						// Special case for elevators
} sectortargettype_t;

DOOM_C_API typedef enum sectorchangetype_e
{
	sct_none,
	sct_zerospecial,
	sct_copytexture,
	sct_copyboth,
} sectorchangetype_t;

DOOM_C_API typedef enum sectorchangemodel_e
{
	scm_trigger,
	scm_numeric,
} sectorchangemodel_t;

DOOM_C_API typedef enum sectorcrushing_e
{
	sc_nocrush,
	sc_crush
} sectorcrushing_t;

DOOM_C_API typedef enum ceilingsound_e
{
	cs_none			= 0x0,
	cs_movement		= 0x1,
	cs_endstop		= 0x2,
	cs_both			= cs_movement | cs_endstop,
} ceilingsound_t;

DOOM_C_API typedef enum exittype_e
{
	exit_normal,
	exit_secret,
} exittype_t;

DOOM_C_API typedef enum lightset_e
{
	lightset_value,
	lightset_highestsurround_firsttagged,
	lightset_lowestsurround,
	lightset_highestsurround,
} lightset_t;

DOOM_C_API typedef enum teleporttype_e
{
	tt_tothing			= 0x00,
	tt_toline			= 0x01,

	tt_targetmask		= 0x01,

	tt_setangle			= 0x00,
	tt_preserveangle	= 0x02,
	tt_reverseangle		= 0x04,

	tt_anglemask		= 0x06,

	tt_silent			= 0x08,
} teleporttype_t;

DOOM_C_API void		T_VerticalDoorGeneric( vldoor_t* door );
DOOM_C_API void		T_RaisePlatGeneric( plat_t* plat );
DOOM_C_API void		T_MoveFloorGeneric( floormove_t* floor );
DOOM_C_API void		T_MoveCeilingGeneric( ceiling_t* ceiling );
DOOM_C_API void		T_MoveElevatorGeneric( elevator_t* elevator );

DOOM_C_API int32_t	EV_DoVanillaPlatformRaiseGeneric( line_t* line, mobj_t* activator );
DOOM_C_API int32_t	EV_DoPerpetualLiftGeneric( line_t* line, mobj_t* activator );
DOOM_C_API int32_t	EV_StopAnyLiftGeneric( line_t* line, mobj_t* activator );
DOOM_C_API int32_t	EV_DoLiftGeneric( line_t* line, mobj_t* activator );
DOOM_C_API int32_t	EV_DoDoorGeneric( line_t* line, mobj_t* activator );
DOOM_C_API int32_t	EV_DoTeleportGeneric( line_t* line, mobj_t* activator );
DOOM_C_API int32_t	EV_DoExitGeneric( line_t* line, mobj_t* activator );
DOOM_C_API int32_t	EV_DoLightSetGeneric( line_t* line, mobj_t* activator );
DOOM_C_API int32_t	EV_DoFloorGeneric( line_t* line, mobj_t* activator );
DOOM_C_API int32_t	EV_DoDonutGeneric( line_t* line, mobj_t* activator );
DOOM_C_API int32_t	EV_DoCeilingGeneric( line_t* line, mobj_t* activator );
DOOM_C_API int32_t	EV_DoCrusherGeneric( line_t* line, mobj_t* activator );
DOOM_C_API int32_t	EV_StopAnyCeilingGeneric( line_t* line, mobj_t* activator );
DOOM_C_API int32_t	EV_DoElevatorGeneric( line_t* line, mobj_t* activator );
DOOM_C_API int32_t	EV_DoBoomFloorCeilingGeneric( line_t* line, mobj_t* activator );
DOOM_C_API int32_t	EV_DoStairsGeneric( line_t* line, mobj_t* activator );
DOOM_C_API int32_t	EV_DoLightStrobeGeneric( line_t* line, mobj_t* activator );

#if defined( __cplusplus )

MakeThinkFuncLookup( vldoor_t, T_VerticalDoorGeneric );
MakeThinkFuncLookup( ceiling_t, T_MoveCeilingGeneric );

#endif // defined( __cplusplus )

#endif
