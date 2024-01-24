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

// Naming scheme:
// <affects>_<behavior>_<triggertype><triggercount>_<triggersby>
//
// <triggertype> can be the following:
// G: Shoot with raycasters
// S: Switch
// U: Use but no switch texture change
// W: Walk
//
// <triggercount> can be the following:
// 1: Once
// R: Unlimited (repeatable)
//
// <triggersby> can be:
// Player
// Monster
//
// The only action that breaks this scheme is Scroll_WallTextureRight_Always
// This action is a special case that is always active

enum DoomActions
{
	Unknown_000,	// Unknown_000
	Door_Raise_UR_All,
	Door_Open_W1_Player,
	Door_Close_W1_Player,
	Door_Raise_W1_All,
	Floor_RaiseLowestCeiling_W1_Player,
	Ceiling_CrusherFast_W1_Player,
	Stairs_BuildBy8_S1_Player,
	Stairs_BuildBy8_W1_Player,
	Sector_Donut_S1_Player,
	Platform_DownWaitUp_W1_All_Player,
	Exit_Normal_S1_Player,
	Light_SetBrightest_W1_Player,
	Light_SetTo255_W1_Player,
	Floor_Raise32ChangeTexture_S1_Player,
	Floor_Raise24ChangeTexture_S1_Player,
	Door_Close30Open_W1_Player,
	Light_Strobe_W1_Player,
	Floor_RaiseNearest_S1_Player,
	Floor_LowerHighest_W1_Player,
	Floor_RaiseNearestChangeTexture_S1_Player,
	Platform_DownWaitUp_S1_Player,
	Floor_RaiseNearestChangeTexture_W1_Player,
	Floor_LowerLowest_S1_Player,
	Floor_RaiseLowestCeiling_GR_Player,
	Ceiling_Crusher_W1_Player,
	Door_RaiseBlue_UR_Player,
	Door_RaiseYellow_UR_Player,
	Door_RaiseRed_UR_Player,
	Door_Raise_S1_Player,
	Floor_RaiseByTexture_W1_Player,
	Door_Open_U1_All,
	Door_OpenBlue_U1_Player,
	Door_OpenRed_U1_Player,
	Door_OpenYellow_U1_Player,
	Light_SetTo35_W1_Player,
	Floor_LowerHighestFast_W1_Player,
	Floor_LowerLowestChangeTexture_NumericModel_W1_Player,
	Floor_LowerLowest_W1_Player,
	Teleport_W1_All,
	Sector_RaiseCeilingLowerFloor_W1_Player,
	Ceiling_LowerToFloor_S1_Player,
	Door_Close_SR_Player,
	Ceiling_LowerToFloor_SR_Player,
	Ceiling_LowerCrush_Player,
	Floor_LowerHighest_SR_Player,
	Door_Open_GR_All,
	Floor_RaiseNearestChangeTexture_GR_Player,
	Scroll_WallTextureLeft_Always,
	Ceiling_Crusher_S1_Player,
	Door_Close_S1_Player,
	Exit_Secret_S1_Player,
	Exit_Normal_W1_Player,
	Platform_Perpetual_W1_Player,
	Platform_Stop_W1_Player,
	Floor_RaiseCrush_S1_Player,
	Floor_RaiseCrush_W1_Player,
	Ceiling_CrusherStop_W1_Player,
	Floor_Raise24_W1_Player,
	Floor_Raise24ChangeTexture_W1_Player,
	Floor_LowerLowest_SR_Player,
	Door_Open_SR_Player,
	Platform_DownWaitUp_SR_Player,
	Door_Raise_SR_Player,
	Floor_RaiseLowestCeiling_SR_player,
	Floor_RaiseCrush_SR_Player,
	Floor_Raise24ChangeTexture_SR_Player,
	Floor_Raise32ChangeTexture_SR_Player,
	Floor_RaiseNearestChangeTexture_SR_Player,
	Floor_RaiseNearest_SR_Player,
	Floor_LowerHighestFast_SR_Player,
	Floor_LowerHighestFast_S1_Player,
	Ceiling_LowerCrush_WR_Player,
	Ceiling_Crusher_WR_Player,
	Ceiling_CrusherStop_WR_Player,
	Door_Close_WR_Player,
	Door_Close30Open_WR_Player,
	Ceiling_CrusherFast_WR_Player,
	Unknown_078,	// Unknown_078
	Light_SetTo35_WR_Player,
	Light_SetBrightest_WR_Player,
	Light_SetTo255_WR_Player,
	Floor_LowerLowest_WR_Player,
	Floor_LowerHighest_WR_Player,
	Floor_LowerLowestChangeTexture_NumericModel_WR_Player,
	Unknown_085,	// Unknown_085
	Door_Open_WR_Player,
	Platform_Perpetual_WR_Player,
	Platform_DownWaitUp_WR_All,
	Platform_Stop_WR_Player,
	Door_Raise_WR_Player,
	Floor_RaiseLowestCeiling_WR_Player,
	Floor_Raise24_WR_Player,
	Floor_Raise24ChangeTexture_WR_Player,
	Floor_RaiseCrush_WR_Player,
	Floor_RaiseNearestChangeTexture_WR_Player,
	Floor_RaiseByTexture_WR_Player,
	Teleport_WR_All_Player,
	Floor_LowerHighestFast_WR_Player,
	Door_OpenFastBlue_SR_Player,
	Stairs_BuildBy16Fast_W1_Player,
	Floor_RaiseLowestCeiling_S1_Player,
	Floor_LowerHighest_S1_Player,
	Door_Open_S1_Player,
	Light_SetLowest_W1_Player,
	Door_RaiseFast_WR_Player,
	Door_OpenFast_WR_Player,
	Door_CloseFast_WR_Player,
	Door_RaiseFast_W1_Player,
	Door_OpenFast_W1_Player,
	Door_CloseFast_W1_Player,
	Door_RaiseFast_S1_Player,
	Door_OpenFast_S1_Player,
	Door_CloseFast_S1_Player,
	Door_RaiseFast_SR_Player,
	Door_OpenFast_SR_Player,
	Door_CloseFast_SR_Player,
	Door_RaiseFast_UR_All,
	Door_RaiseFast_U1_Player,
	Floor_RaiseNearest_W1_Player,
	Platform_DownWaitUpFast_WR_Player,
	Platform_DownWaitUpFast_W1_Player,
	Platform_DownWaitUpFast_S1_Player,
	Platform_DownWaitUpFast_SR_Player,
	Exit_Secret_W1_Player,
	Teleport_W1_Monsters,
	Teleport_WR_Monsters,
	Stairs_BuildBy16Fast_S1_Player,
	Floor_RaiseNearest_WR_Player,
	Floor_RaiseNearestFast_WR_Player,
	Floor_RaiseNearestFast_W1_Player,
	Floor_RaiseNearestFast_S1_Player,
	Floor_RaiseNearestFast_SR_Player,
	Door_OpenFastBlue_S1_Player,
	Door_OpenFastRed_SR_Player,
	Door_OpenFastRed_S1_Player,
	Door_OpenFastYellow_SR_Player,
	Door_OpenFasYellow_S1_Player,
	Light_SetTo255_SR_Player,
	Light_SetTo35_SR_Player,
	Floor_Raise512_S1_Player,
	Ceiling_CrusherSilent_W1_Player,
};

enum BoomActions
{
	Floor_ChangeTexture_NumericModel_SR_Player = 78,
	Scroll_WallTextureRight_Always = 85,
	Floor_Raise512_W1_Player = 142,
	Floor_Raise512_WR_Player = 147,
	Floor_ChangeTexture_W1_Player = 153,
	Floor_ChangeTexture_WR_Player = 154,
	Floor_RaiseByTexture_S1_Player = 158,
	Floor_LowerLowestChangeTexture_NumericModel_S1_Player = 159,
	Floor_Raise24ChangeTextureSlow_S1_Player = 160,
	Floor_Raise24_S1_Player = 161,
	Door_Close30Open_S1_Player = 175,
	Floor_RaiseByTexture_SR_Player = 176,
	Floor_LowerLowestChangeTexture_NumericModel_SR_Player = 177,
	Floor_Raise512_SR_Player = 178,
	Floor_Raise24ChangeTextureSlow_SR_Player = 179,
	Floor_Raise24_SR_Player = 180,
	Floor_ChangeTexture_S1_Player = 189,
	Floor_ChangeTexture_SR_Player = 190,
	Door_Close30Open_SR_Player = 196,
	Transfer_FloorLighting_Always = 213,
	Floor_LowerNearest_W1_Player = 219,
	Floor_LowerNearest_WR_Player = 220,
	Floor_LowerNearest_S1_Player = 221,
	Floor_LowerNearest_SR_Player = 222,
	Transfer_Friction_Always = 223,
	Transfer_WindByLength_Always = 224,
	Transfer_CurrentByLength_Always = 225,
	Transfer_WindOrCurrentByPoint_Always = 226,
	Floor_ChangeTexture_NumericModel_W1_Player = 239,
	Floor_ChangeTexture_NumericModel_WR_Player = 240,
	Floor_ChangeTexture_NumericModel_S1_Player = 241,
	Transfer_Properties_Always = 242,
	Scroll_CeilingTexture_Always = 250,
	Scroll_FloorTexture_Always = 251,
	Scroll_FloorObjects_Always = 252,
	Scroll_FloorTextureObjects_Always = 253,
	Scroll_WallTextureBySector_Always = 254,
	Scroll_WallTextureByOffset_Always = 255,
	Textue_Translucent_Always = 260,
	Transfer_CeilingLighting_Always = 261,
	Transfer_Sky_Always = 271,
	Transfer_SkyReversed_Always = 272,
};

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

DOOM_C_API fixed_t P_FindNextHighestFloor( sector_t* sec, int currentheight );

DOOM_C_API fixed_t P_FindLowestCeilingSurrounding(sector_t* sec);
DOOM_C_API fixed_t P_FindHighestCeilingSurrounding(sector_t* sec);

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



DOOM_C_API typedef struct
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
} plat_t;



#define PLATWAIT		3
#define PLATSPEED		FRACUNIT
#define MAXPLATS		30


DOOM_C_API extern plat_t*	activeplats[MAXPLATS];

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



DOOM_C_API typedef struct
{
	thinker_t		thinker;
	vldoor_e		type;
	sector_t*		sector;
	fixed_t			topheight;
	fixed_t			speed;

	// 1 = up, 0 = waiting at top, -1 = down
	int				direction;

	// tics to wait at the top
	int				topwait;
	// (keep in case a door going down is reset)
	// when it reaches 0, start going down
	int				topcountdown;
} vldoor_t;



#define VDOORSPEED		FRACUNIT*2
#define VDOORWAIT		150

DOOM_C_API void EV_VerticalDoor( line_t* line, mobj_t* thing );

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
    silentCrushAndRaise

} ceiling_e;



DOOM_C_API typedef struct
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

} ceiling_t;





#define CEILSPEED		FRACUNIT
#define CEILWAIT		150
#define MAXCEILINGS		30

DOOM_C_API extern ceiling_t*	activeceilings[MAXCEILINGS];

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
    raiseFloor512
    
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
    int		direction;
    int		newspecial;
    short	texture;
    fixed_t	floordestheight;
    fixed_t	speed;

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

#endif
