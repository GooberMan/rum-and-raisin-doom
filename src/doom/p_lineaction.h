//
// Copyright(C) 2024 Ethan Watson
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

#ifndef __D_LINEACTION_H__
#define __D_LINEACTION_H__

#include "doomtype.h"

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
// Actions that are always active use the following naming scheme:
// <affects>_<behavior>_Always

enum DoomActions : uint32_t
{
	Unknown_000,	// Unknown_000
	Door_Raise_UR_All,
	Door_Open_W1_Player,
	Door_Close_W1_Player,
	Door_Raise_W1_Player,
	Floor_RaiseLowestCeiling_W1_Player,
	Ceiling_CrusherFast_W1_Player,
	Stairs_BuildBy8_S1_Player,
	Stairs_BuildBy8_W1_Player,
	Sector_Donut_S1_Player,
	Platform_DownWaitUp_W1_All,
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
	Door_Open_U1_Player,
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
	Door_Open_GR_Player,
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
	Teleport_WR_All,
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
	Door_RaiseFast_UR_Player,
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
	Door_OpenFastYellow_S1_Player,
	Light_SetTo255_SR_Player,
	Light_SetTo35_SR_Player,
	Floor_Raise512_S1_Player,
	Ceiling_CrusherSilent_W1_Player,

	DoomActions_Max,
	DoomActions_Min = Door_Raise_UR_All,
};

enum BoomActions : uint32_t
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
	Texture_Translucent_Always = 260,
	Transfer_CeilingLighting_Always = 261,

	BoomActions_Max,
	BoomActions_Min = Floor_ChangeTexture_NumericModel_SR_Player
};

enum MBFActions : uint32_t
{
	Transfer_Sky_Always = 271,
	Transfer_SkyReversed_Always = 272,
};

enum MBF21Actions : uint32_t
{
	Texture_ScrollSpeedDiv8Standard_Always = 1024,
	Texture_ScrollSpeedDiv8Displacement_Always = 1025,
	Texture_ScrollSpeedDiv8Accelerative_Always = 1026,
};

enum BoomFloorActions : uint32_t
{
	Generic_Crusher							= 0x2F80,
	Generic_Stairs							= 0x3000,
	Generic_Lift							= 0x3400,
	Generic_LockedDoor						= 0x3800,
	Generic_Door							= 0x3c00,
	Generic_Ceiling							= 0x4000,
	Generic_Floor							= 0x6000,

	Generic_Min								= Generic_Crusher,
	Generic_Max								= 0x8000,
};

enum BoomGenericFlags : uint32_t
{
	Generic_Trigger_Repeatable				= 0x0001,
	Generic_Trigger_Walk					= 0x0000,
	Generic_Trigger_Switch					= 0x0002,
	Generic_Trigger_Shoot					= 0x0004,
	Generic_Trigger_Use						= 0x0006,

	Generic_Trigger_W1						= Generic_Trigger_Walk,
	Generic_Trigger_WR						= Generic_Trigger_Walk | Generic_Trigger_Repeatable,
	Generic_Trigger_S1						= Generic_Trigger_Switch,
	Generic_Trigger_SR						= Generic_Trigger_Switch | Generic_Trigger_Repeatable,
	Generic_Trigger_G1						= Generic_Trigger_Shoot,
	Generic_Trigger_GR						= Generic_Trigger_Shoot | Generic_Trigger_Repeatable,
	Generic_Trigger_U1						= Generic_Trigger_Use,
	Generic_Trigger_UR						= Generic_Trigger_Use | Generic_Trigger_Repeatable,

	Generic_Trigger_Mask					= 0x0007,
	Generic_TriggerActivate_Mask			= Generic_Trigger_Mask & ~Generic_Trigger_Repeatable,

	Generic_Speed_Slow						= 0x0000,
	Generic_Speed_Normal					= 0x0008,
	Generic_Speed_Fast						= 0x0010,
	Generic_Speed_Turbo						= 0x0018,

	Generic_Speed_Mask						= 0x0018,
	Generic_Speed_Shift						= 3,
};

enum BoomFloorFlags : uint32_t
{
	Floor_Model_Trigger						= 0x0000,
	Floor_Model_Numeric						= 0x0020,

	Floor_Model_Mask						= 0x0020,

	Floor_AllowMonsters_No					= 0x0000,
	Floor_AllowMonsters_Yes					= 0x0020,

	Floor_AllowMonsters_Mask				= 0x0020,

	Floor_Direction_Down					= 0x0000,
	Floor_Direction_Up						= 0x0040,

	Floor_Direction_Mask					= 0x0040,

	Floor_Target_HighestNeighborFloor		= 0x0000,
	Floor_Target_LowestNeighborFloor		= 0x0080,
	Floor_Target_NearestNeighborFloor		= 0x0100,
	Floor_Target_LowestNeighborCeiling		= 0x0180,
	Floor_Target_Ceiling					= 0x0200,
	Floor_Target_ShortestLowerTexture		= 0x0280,
	Floor_Target_24							= 0x0300,
	Floor_Target_32							= 0x0380,

	Floor_Target_Mask						= 0x0380,

	Floor_Change_None						= 0x0000,
	Floor_Change_ClearSectorType			= 0x0400,
	Floor_Change_CopyTextureOnly			= 0x0800,
	Floor_Change_CopyTextureSectorType		= 0x0C00,

	Floor_Change_Mask						= 0x0C00,

	Floor_Crush_No							= 0x0000,
	Floor_Crush_Yes							= 0x1000,

	Floor_Crush_Mask						= 0x1000,
};

enum BoomCeilingFlags : uint32_t
{
	Ceiling_Model_Trigger					= 0x0000,
	Ceiling_Model_Numeric					= 0x0020,

	Ceiling_Model_Mask						= 0x0020,

	Ceiling_AllowMonsters_No				= 0x0000,
	Ceiling_AllowMonsters_Yes				= 0x0020,

	Ceiling_AllowMonsters_Mask				= 0x0020,

	Ceiling_Direction_Down					= 0x0000,
	Ceiling_Direction_Up					= 0x0040,

	Ceiling_Direction_Mask					= 0x0040,

	Ceiling_Target_HighestNeighborCeiling	= 0x0000,
	Ceiling_Target_LowestNeighborCeiling	= 0x0080,
	Ceiling_Target_NearestNeighborCeiling	= 0x0100,
	Ceiling_Target_LowestNeighborFloor		= 0x0180,
	Ceiling_Target_Floor					= 0x0200,
	Ceiling_Target_ShortestLowerTexture		= 0x0280,
	Ceiling_Target_24						= 0x0300,
	Ceiling_Target_32						= 0x0380,

	Ceiling_Target_Mask						= 0x0380,

	Ceiling_Change_None						= 0x0000,
	Ceiling_Change_ClearSectorType			= 0x0400,
	Ceiling_Change_CopyTextureOnly			= 0x0800,
	Ceiling_Change_CopyTextureSectorType	= 0x0C00,

	Ceiling_Change_Mask						= 0x0C00,

	Ceiling_Crush_No						= 0x0000,
	Ceiling_Crush_Yes						= 0x1000,

	Ceiling_Crush_Mask						= 0x1000,
};

enum BoomDoorFlags : uint32_t
{
	Door_Type_OpenWaitClose					= 0x0000,
	Door_Type_Open							= 0x0020,
	Door_Type_CloseWaitOpen					= 0x0040,
	Door_Type_Close							= 0x0060,

	Door_Type_Mask							= 0x0060,
	Door_Type_Shift							= 5,

	Door_AllowMonsters_No					= 0x0000,
	Door_AllowMonsters_Yes					= 0x0080,

	Door_AllowMonsters_Mask					= 0x0080,

	Door_Delay_1Second						= 0x0000,
	Door_Delay_4Seconds						= 0x0100,
	Door_Delay_9Seconds						= 0x0200,
	Door_Delay_30Seconds					= 0x0300,

	Door_Delay_Mask							= 0x0300,
	Door_Delay_Shift						= 8,
};

enum BoomLockedDoorFlags : uint32_t
{
	LockedDoor_Type_OpenWaitClose			= 0x0000,
	LockedDoor_Type_Open					= 0x0020,

	LockedDoor_Type_Mask					= 0x0020,

	LockedDoor_Lock_Any						= 0x0000,
	LockedDoor_Lock_RedCard					= 0x0040,
	LockedDoor_Lock_BlueCard				= 0x0080,
	LockedDoor_Lock_YellowCard				= 0x00C0,
	LockedDoor_Lock_RedSkull				= 0x0100,
	LockedDoor_Lock_BlueSkyll				= 0x0140,
	LockedDoor_Lock_YellowSkull				= 0x0180,
	LockedDoor_Lock_All						= 0x01C0,

	LockedDoor_TypeAgnostic_No				= 0x0000,
	LockedDoor_TypeAgnostic_Yes				= 0x0200,

	LockedDoor_TypeAgnostic_Mask			= 0x0200,
};

enum BoomLiftFlags : uint32_t
{
	Lift_AllowMonsters_No					= 0x0000,
	Lift_AllowMonsters_Yes					= 0x0020,

	Lift_AllowMonsters_Mask					= 0x0020,

	Lift_Delay_1Second						= 0x0000,
	Lift_Delay_3Seconds						= 0x0040,
	Lift_Delay_5Seconds						= 0x0060,
	Lift_Delay_10Seconds					= 0x0070,

	Lift_Delay_Mask							= 0x00C0,
	Lift_Delay_Shift						= 6,

	Lift_Target_LowestNeighborFloor			= 0x0000,
	Lift_Target_NearestNeighborFloor		= 0x0100,
	Lift_Target_LowestNeighborCeiling		= 0x0200,
	Lift_Target_PerpetualLowHighFloor		= 0x0300,

	Lift_Target_Mask						= 0x0300,
	Lift_Target_Shift						= 8,
};

enum BoomSpeeds : uint32_t
{
	Speed_Slow,
	Speed_Normal,
	Speed_Fast,
	Speed_Turbo,
};

typedef enum linetrigger_e : uint32_t
{
	LT_None									= 0x00,
	LT_Use									= 0x01,
	LT_Walk									= 0x02,
	LT_Hitscan								= 0x04,
	LT_Missile								= 0x08, // Unused but reserved

	// Animation can combine with the above
	LT_AnimateSwitch						= 0x10,

	LT_Switch								= LT_Use | LT_AnimateSwitch,
	LT_Gun									= LT_Hitscan | LT_AnimateSwitch,

	//LT_MonstersCanTrigger					= 0x40, // Do we need this with precon funcs?

	LT_ActivationTypeMask					= 0x0F,
} linetrigger_t;

typedef enum linelock_e : uint32_t
{
	LL_None									= 0x000,

	LL_Blue									= 0x001,
	LL_Yellow								= 0x002,
	LL_Red									= 0x004,

	LL_Card									= 0x008,
	LL_Skull								= 0x010,

	LL_AnyOf								= 0x020,
	LL_AllColorsOf							= 0x040,
	LL_AllOf								= 0x080,

	LL_ColorMask							= 0x007,
	LL_CardMask								= 0x01F,
	LL_ComboMask							= 0x0E0,

	LL_BlueCard								= LL_Blue | LL_Card,
	LL_YellowCard							= LL_Yellow | LL_Card,
	LL_RedCard								= LL_Red | LL_Card,
	LL_BlueSkull							= LL_Blue | LL_Skull,
	LL_YellowSkull							= LL_Yellow | LL_Skull,
	LL_RedSkull								= LL_Red | LL_Skull,
} linelock_t;

typedef struct lineaction_s lineaction_t;
typedef struct line_s line_t;
typedef struct mobj_s mobj_t;

using linepreconfunc_t = bool(*)(line_t*, mobj_t*, linetrigger_t, int32_t);
using linefunc_t = void(*)(line_t*, mobj_t*);

struct lineaction_s
{
	linepreconfunc_t			precondition;
	linefunc_t					action;
	linetrigger_t				trigger;
	linelock_t					lock;
	int32_t						speed;
	int32_t						delay;
	int32_t						param1;		// Generalised parameters interpreted differently by different actions
	int32_t						param2;		// Generalised parameters interpreted differently by different actions

#if defined(__cplusplus)
	INLINE bool Handle(line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside)
	{
		if(precondition(line, activator, activationtype, activationside))
		{
			action(line, activator);
			return true;
		}

		return false;
	}
#endif // defined(__cplusplus)
};

DOOM_C_API lineaction_t* P_GetLineActionFor( line_t* line );

#endif // __D_LINEACTION_H__
