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
	Door_Raise_DR_All,
	Door_Open_W1_Player,
	Door_Close_W1_Player,
	Door_Raise_W1_Player,
	Floor_RaiseLowestCeiling_W1_Player,
	Crusher_Fast_W1_Player,
	Stairs_BuildBy8_S1_Player,
	Stairs_BuildBy8_W1_Player,
	Sector_Donut_S1_Player,
	Platform_DownWaitUp_W1_All,
	Exit_Normal_S1_Player,
	Light_SetBrightest_W1_Player,
	Light_SetTo255_W1_Player,
	Platform_Raise32ChangeTexture_S1_Player,
	Platform_Raise24ChangeTexture_S1_Player,
	Door_Close30Open_W1_Player,
	Light_Strobe_W1_Player,
	Floor_RaiseNearest_S1_Player,
	Floor_LowerHighest_W1_Player,
	Platform_RaiseNearestChangeTexture_S1_Player,
	Platform_DownWaitUp_S1_Player,
	Platform_RaiseNearestChangeTexture_W1_Player,
	Floor_LowerLowest_S1_Player,
	Floor_RaiseLowestCeiling_GR_Player,
	Crusher_W1_Player,
	Door_RaiseBlue_DR_Player,
	Door_RaiseYellow_DR_Player,
	Door_RaiseRed_DR_Player,
	Door_Raise_S1_Player,
	Floor_RaiseByTexture_W1_Player,
	Door_Open_D1_Player,
	Door_OpenBlue_D1_Player,
	Door_OpenRed_D1_Player,
	Door_OpenYellow_D1_Player,
	Light_SetTo35_W1_Player,
	Floor_LowerHighestFast_W1_Player,
	Floor_LowerLowestChangeTexture_NumericModel_W1_Player,
	Floor_LowerLowest_W1_Player,
	Teleport_Thing_W1_All,
	Ceiling_RaiseHighestCeiling_W1_Player,
	Ceiling_LowerToFloor_S1_Player,
	Door_Close_SR_Player,
	Ceiling_LowerToFloor_SR_Player,
	Ceiling_LowerToFloorPlus8_W1_Player,
	Floor_LowerHighest_SR_Player,
	Door_Open_GR_All,
	Platform_RaiseNearestChangeTexture_G1_Player,
	Scroll_WallTextureLeft_Always,
	Crusher_S1_Player,
	Door_Close_S1_Player,
	Exit_Secret_S1_Player,
	Exit_Normal_W1_Player,
	Platform_Perpetual_W1_Player,
	Platform_Stop_W1_Player,
	Floor_RaiseCrush_S1_Player,
	Floor_RaiseCrush_W1_Player,
	Crusher_Stop_W1_Player,
	Floor_Raise24_W1_Player,
	Floor_Raise24ChangeTexture_W1_Player,
	Floor_LowerLowest_SR_Player,
	Door_Open_SR_Player,
	Platform_DownWaitUp_SR_Player,
	Door_Raise_SR_Player,
	Floor_RaiseLowestCeiling_SR_player,
	Floor_RaiseCrush_SR_Player,
	Platform_Raise24ChangeTexture_SR_Player,
	Platform_Raise32ChangeTexture_SR_Player,
	Platform_RaiseNearestChangeTexture_SR_Player,
	Floor_RaiseNearest_SR_Player,
	Floor_LowerHighestFast_SR_Player,
	Floor_LowerHighestFast_S1_Player,
	Ceiling_LowerToFloorPlus8_WR_Player,
	Crusher_WR_Player,
	Crusher_Stop_WR_Player,
	Door_Close_WR_Player,
	Door_Close30Open_WR_Player,
	Crusher_Fast_WR_Player,
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
	Platform_RaiseNearestChangeTexture_WR_Player,
	Floor_RaiseByTexture_WR_Player,
	Teleport_Thing_WR_All,
	Floor_LowerHighestPlus8Fast_WR_Player,
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
	Door_RaiseFast_DR_Player,
	Door_OpenFast_D1_Player,
	Floor_RaiseNearest_W1_Player,
	Platform_DownWaitUpFast_WR_Player,
	Platform_DownWaitUpFast_W1_Player,
	Platform_DownWaitUpFast_S1_Player,
	Platform_DownWaitUpFast_SR_Player,
	Exit_Secret_W1_Player,
	Teleport_Thing_W1_Monsters,
	Teleport_Thing_WR_Monsters,
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
	Crusher_Silent_W1_Player,

	DoomActions_Max,
	DoomActions_Min = Door_Raise_DR_All,
};

enum BoomActions : uint32_t
{
	Floor_ChangeTexture_NumericModel_SR_Player = 78,
	Scroll_WallTextureRight_Always = 85,

	Floor_Raise512_W1_Player = 142,
	Platform_Raise24ChangeTexture_W1_Player,
	Platform_Raise32ChangeTexture_W1_Player,
	Ceiling_LowerToFloor_W1_Player,
	Sector_Donut_W1_Player,
	Floor_Raise512_WR_Player,
	Platform_Raise24ChangeTexture_WR_Player,
	Platform_Raise32ChangeTexture_WR_Player,
	Crusher_Silent_WR_Player,
	Ceiling_RaiseHighestCeiling_WR_Player,
	Ceiling_LowerToFloor_WR_Player,
	Floor_ChangeTexture_W1_Player,
	Floor_ChangeTexture_WR_Player,
	Sector_Donut_WR_Player,
	Light_Strobe_WR_Player,
	Light_SetLowest_WR_Player,
	Floor_RaiseByTexture_S1_Player,
	Floor_LowerLowestChangeTexture_NumericModel_S1_Player,
	Floor_Raise24ChangeTexture_S1_Player,
	Floor_Raise24_S1_Player,
	Platform_Perpetual_S1_Player,
	Platform_Stop_S1_Player,
	Crusher_Fast_S1_Player,
	Crusher_Silent_S1_Player,
	FloorCeiling_RaiseHighestOrLowerLowest_S1_Player,
	Ceiling_LowerToFloorPlus8_S1_Player,
	Crusher_Stop_S1_Player,
	Light_SetBrightest_S1_Player,
	Light_SetTo35_S1_Player,
	Light_SetTo255_S1_Player,
	Light_Strobe_S1_Player,
	Light_SetLowest_S1_Player,
	Teleport_Thing_S1_All,
	Door_Close30Open_S1_Player,
	Floor_RaiseByTexture_SR_Player,
	Floor_LowerLowestChangeTexture_NumericModel_SR_Player,
	Floor_Raise512_SR_Player,
	Floor_Raise24ChangeTexture_SR_Player,
	Floor_Raise24_SR_Player,
	Platform_Perpetual_SR_Player,
	Platform_Stop_SR_Player,
	Crusher_Fast_SR_Player,
	Crusher_SR_Player,
	Crusher_Silent_SR_Player,
	FloorCeiling_RaiseHighestOrLowerLowest_SR_Player,
	Ceiling_LowerToFloorPlus8_SR_Player,
	Crusher_Stop_SR_Player,
	Floor_ChangeTexture_S1_Player,
	Floor_ChangeTexture_SR_Player,
	Sector_Donut_SR_Player,
	Light_SetBrightest_SR_Player,
	Light_Strobe_SR_Player,
	Light_SetLowest_SR_Player,
	Teleport_Thing_SR_All,
	Door_Close30Open_SR_Player,
	Exit_Normal_G1_Player,
	Exit_Secret_G1_Player,
	Ceiling_LowerLowerstCeiling_W1_Player,
	Ceiling_LowerHighestFloor_W1_Player,
	Ceiling_LowerLowerstCeiling_WR_Player,
	Ceiling_LowerHighestFloor_WR_Player,
	Ceiling_LowerLowerstCeiling_S1_Player,
	Ceiling_LowerHighestFloor_S1_Player,
	Ceiling_LowerLowerstCeiling_SR_Player,
	Ceiling_LowerHighestFloor_SR_Player,
	Teleport_ThingSilentPreserve_W1_All,
	Teleport_ThingSilentPreserve_WR_All,
	Teleport_ThingSilentPreserve_S1_All,
	Teleport_ThingSilentPreserve_SR_All,
	Platform_RaiseInstantCeiling_SR_Player,
	Platform_RaiseInstantCeiling_WR_Player,
	Transfer_FloorLighting_Always,
	Scroll_CeilingTexture_Accelerative_Always,
	Scroll_FloorTexture_Accelerative_Always,
	Scroll_FloorObjects_Accelerative_Always,
	Scroll_FloorTextureObjects_Accelerative_Always,
	Scroll_WallTextureBySector_Accelerative_Always,
	Floor_LowerNearest_W1_Player,
	Floor_LowerNearest_WR_Player,
	Floor_LowerNearest_S1_Player,
	Floor_LowerNearest_SR_Player,
	Transfer_Friction_Always,
	Transfer_WindByLength_Always,
	Transfer_CurrentByLength_Always,
	Transfer_WindOrCurrentByPoint_Always,
	Elevator_Up_W1_Player,
	Elevator_Up_WR_Player,
	Elevator_Up_S1_Player,
	Elevator_Up_SR_Player,
	Elevator_Down_W1_Player,
	Elevator_Down_WR_Player,
	Elevator_Down_S1_Player,
	Elevator_Down_SR_Player,
	Elevator_Call_W1_Player,
	Elevator_Call_WR_Player,
	Elevator_Call_S1_Player,
	Elevator_Call_SR_Player,
	Floor_ChangeTexture_NumericModel_W1_Player,
	Floor_ChangeTexture_NumericModel_WR_Player,
	Floor_ChangeTexture_NumericModel_S1_Player,
	Transfer_Properties_Always,
	Teleport_LineSilentPreserve_W1_All,
	Teleport_LineSilentPreserve_WR_All,
	Scroll_CeilingTexture_Displace_Always,
	Scroll_FloorTexture_Displace_Always,
	Scroll_FloorObjects_Displace_Always,
	Scroll_FloorTextureObjects_Displace_Always,
	Scroll_WallTextureBySector_Displace_Always,
	Scroll_CeilingTexture_Always,
	Scroll_FloorTexture_Always,
	Scroll_FloorObjects_Always,
	Scroll_FloorTextureObjects_Always,
	Scroll_WallTextureBySector_Always,
	Scroll_WallTextureByOffset_Always,
	Stairs_BuildBy8_WR_Player,
	Stairs_BuildBy16Fast_WR_Player,
	Stairs_BuildBy8_SR_Player,
	Stairs_BuildBy16Fast_SR_Player,
	Texture_Translucent_Always,
	Transfer_CeilingLighting_Always,
	Teleport_LineSilentReversed_W1_All,
	Teleport_LineSilentReversed_WR_All,
	Teleport_LineSilentReversed_W1_Monsters,
	Teleport_LineSilentReversed_WR_Monsters,
	Teleport_LineSilentPreserve_W1_Monsters,
	Teleport_LineSilentPreserve_WR_Monsters,
	Teleport_ThingSilent_W1_Monsters,
	Teleport_ThingSilent_WR_Monsters,

	BoomActions_Max,
	BoomActions_Min = Floor_Raise512_W1_Player,

};

enum MBFActions : uint32_t
{
	Unknown_270 = 270,

	Transfer_Sky_Always,
	Transfer_SkyReversed_Always,

	MBFActions_Max,
	MBFActions_Min = Transfer_Sky_Always
};

enum MBF21Actions : uint32_t
{
	Scroll_WallTextureBySectorDiv8_Always = 1024,
	Scroll_WallTextureBySectorDiv8_Displacement_Always,
	Scroll_WallTextureBySectorDiv8_Accelerative_Always,

	MBF21Actions_Max,
	MBF21Actions_Min = Scroll_WallTextureBySectorDiv8_Always,
};

enum ZokumBSPActions : uint32_t
{
	Zokum_DoNotRender = 998,			// Do not render this linedef.
	Zokum_NoBlockmap = 999,				// Do not add this linedef to the blockmap.
	Zokum_RemoteScroll = 1048,			// Remote scroll nearest wall with same tag.
	Zokum_ChangeStartVertex = 1078,		// Change start vertex of all with same tag to be same as this line. Make line non-render. Also copies sidedefs.
	Zokum_ChangeEndVertex = 1079,		// Change end vertex of all with same tag to be the same as this line. Make line non-render. Also copies sidedefs.
	Zokum_RotateDegrees = 1080,			// Rotate the rendered wall N degrees, where degrees is taken from tag.
	Zokum_RotateDegreesHard = 1081,		// Set the wall rotation to a hardcoded degree, degree taken from tag.
	Zokum_RotateAngleT = 1082,			// Rotate the rendered wall N BAMs, BAMs taken from tag.
	Zokum_RotateAngleTHard = 1083,		// Set the wall rotation to a hardoced BAM, taken from tag.
	Zokum_DoNotRenderBackSeg = 1084,	// Do not render seg on the second side of a linedef.
	Zokum_DoNotRenderFrontSeg = 1085,	// Do not render seg on the front side of a lindedef.
	Zokum_DoNotRenderAnySeg = 1086,		// Do not render segs on any side of a linedef.
};

enum RNR24Actions : uint32_t
{
	Offset_FloorTexture_Always = 2048,
	Offset_CeilingTexture_Always,
	Offset_FloorCeilingTexture_Always,
	Rotate_FloorTexture_Always,
	Rotate_CeilingTexture_Always,
	Rotate_FloorCeilingTexture_Always,
	OffsetRotate_FloorTexture_Always,
	OffsetRotate_CeilingTexture_Always,
	OffsetRotate_FloorCeilingTexture_Always,

	Music_ChangeLooping_W1_Player,
	Music_ChangeLooping_WR_Player,
	Music_ChangeLooping_S1_Player,
	Music_ChangeLooping_SR_Player,
	Music_ChangeLooping_G1_Player,
	Music_ChangeLooping_GR_Player,
	Music_ChangeOnce_W1_Player,
	Music_ChangeOnce_WR_Player,
	Music_ChangeOnce_S1_Player,
	Music_ChangeOnce_SR_Player,
	Music_ChangeOnce_G1_Player,
	Music_ChangeOnce_GR_Player,

	Exit_ResetNormal_W1_Player,
	Exit_ResetNormal_S1_Player,
	Exit_ResetNormal_G1_Player,
	Exit_ResetSecret_W1_Player,
	Exit_ResetSecret_S1_Player,
	Exit_ResetSecret_G1_Player,

	Tint_SetTo_Always,
	Tint_SetTo_W1_Player,
	Tint_SetTo_WR_Player,
	Tint_SetTo_S1_Player,
	Tint_SetTo_SR_Player,
	Tint_SetTo_G1_Player,
	Tint_SetTo_GR_Player,

	Scroll_WallTextureBothSides_Left_Always,
	Scroll_WallTextureBothSides_Right_Always,
	Scroll_WallTextureBothSides_SectorDiv8_Always,
	Scroll_WallTextureBothSides_SectorDiv8Displacement_Always,
	Scroll_WallTextureBothSides_SectorDiv8Accelerative_Always,

	Music_ChangeLooping_Reset_W1_Player,
	Music_ChangeLooping_Reset_WR_Player,
	Music_ChangeLooping_Reset_S1_Player,
	Music_ChangeLooping_Reset_SR_Player,
	Music_ChangeLooping_Reset_G1_Player,
	Music_ChangeLooping_Reset_GR_Player,
	Music_ChangeOnce_Reset_W1_Player,
	Music_ChangeOnce_Reset_WR_Player,
	Music_ChangeOnce_Reset_S1_Player,
	Music_ChangeOnce_Reset_SR_Player,
	Music_ChangeOnce_Reset_G1_Player,
	Music_ChangeOnce_Reset_GR_Player,

	RNR24Actions_Max,
	RNR24Actions_Min = Offset_FloorTexture_Always,
};

enum ActionArrayValues : uint32_t
{
	Actions_BuiltIn_Count	= (uint32_t)MBFActions_Max - (uint32_t)DoomActions_Min + 1,
	Actions_MBF21_Count		= (uint32_t)MBF21Actions_Max - (uint32_t)MBF21Actions_Min,
	Actions_RNR24_Count		= (uint32_t)RNR24Actions_Max - (uint32_t)RNR24Actions_Min,
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

enum BoomStairsFlags : uint32_t
{
	Stairs_AllowMonsters_No					= 0x0000,
	Stairs_AllowMonsters_Yes				= 0x0020,

	Stairs_AllowMonsters_Mask				= 0x0020,

	Stairs_Target_4							= 0x0000,
	Stairs_Target_8							= 0x0040,
	Stairs_Target_16						= 0x0080,
	Stairs_Target_24						= 0x00C0,

	Stairs_Target_Mask						= 0x00C0,
	Stairs_Target_Shift						= 6,

	Stairs_Direction_Down					= 0x0000,
	Stairs_Direction_Up						= 0x0100,

	Stairs_Direction_Mask					= 0x0100,

	Stairs_WithMinusTargets_Mask			= Stairs_Target_Mask | Stairs_Direction_Mask,

	Stairs_IgnoreTexture_No					= 0x0000,
	Stairs_IgnoreTexture_Yes				= 0x0200,

	Stairs_IgnoreTexture_Mask				= 0x0200,
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
	Floor_Target_Shift						= 7,

	Floor_Change_None						= 0x0000,
	Floor_Change_ClearSectorType			= 0x0400,
	Floor_Change_CopyTextureOnly			= 0x0800,
	Floor_Change_CopyTextureSectorType		= 0x0C00,

	Floor_Change_Mask						= 0x0C00,
	Floor_Change_Shift						= 10,

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
	Ceiling_Target_HighestNeighborFloor		= 0x0180,
	Ceiling_Target_Floor					= 0x0200,
	Ceiling_Target_ShortestLowerTexture		= 0x0280,
	Ceiling_Target_24						= 0x0300,
	Ceiling_Target_32						= 0x0380,

	Ceiling_Target_Mask						= 0x0380,
	Ceiling_Target_Shift					= 7,

	Ceiling_Change_None						= 0x0000,
	Ceiling_Change_ClearSectorType			= 0x0400,
	Ceiling_Change_CopyTextureOnly			= 0x0800,
	Ceiling_Change_CopyTextureSectorType	= 0x0C00,

	Ceiling_Change_Mask						= 0x0C00,
	Ceiling_Change_Shift					= 10,

	Ceiling_Crush_No						= 0x0000,
	Ceiling_Crush_Yes						= 0x1000,

	Ceiling_Crush_Mask						= 0x1000,
};

enum BoomCrusherFlags : uint32_t
{
	Crusher_AllowMonsters_No				= 0x0000,
	Crusher_AllowMonsters_Yes				= 0x0020,

	Crusher_AllowMonsters_Mask				= 0x0020,

	Crusher_Silent_No						= 0x0000,
	Crusher_Silent_Yes						= 0x0040,

	Crusher_Silent_Mask						= 0x0040,
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
	LockedDoor_Lock_BlueSkull				= 0x0140,
	LockedDoor_Lock_YellowSkull				= 0x0180,
	LockedDoor_Lock_All						= 0x01C0,

	LockedDoor_Lock_Mask					= 0x01C0,
	LockedDoor_Lock_Shift					= 6,

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

	LT_ActivationTypeMask					= 0x0F,
	LT_AnimatedActivationTypeMask			= 0x1F,

	LT_FrontSide							= 0x40,
	LT_BackSide								= 0x80,
	LT_BothSides							= LT_FrontSide | LT_BackSide,
	LT_SidesMask							= 0xC0,

	LT_UseFront								= LT_Use | LT_FrontSide,
	LT_SwitchFront							= LT_Switch | LT_FrontSide,
	LT_GunFront								= LT_Gun | LT_FrontSide,
	LT_WalkFront							= LT_Walk | LT_FrontSide,
	LT_WalkBoth								= LT_Walk | LT_BothSides,

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
	LL_AllOfEither							= 0x100,

	LL_ColorMask							= 0x007,
	LL_CardMask								= 0x01F,
	LL_TypeMask								= LL_ColorMask | LL_CardMask,
	LL_ComboMask							= 0x1E0,

	LL_BlueCard								= LL_Blue | LL_Card,
	LL_YellowCard							= LL_Yellow | LL_Card,
	LL_RedCard								= LL_Red | LL_Card,
	LL_BlueSkull							= LL_Blue | LL_Skull,
	LL_YellowSkull							= LL_Yellow | LL_Skull,
	LL_RedSkull								= LL_Red | LL_Skull,

	LL_AnyCard								= LL_AnyOf | LL_Card,
	LL_AnySkull								= LL_AnyOf | LL_Skull,
	LL_AnyKey								= LL_AnyOf | LL_Card | LL_Skull,
	LL_AnyBlue								= LL_AnyOf | LL_Blue,
	LL_AnyYellow							= LL_AnyOf | LL_Yellow,
	LL_AnyRed								= LL_AnyOf | LL_Red,

	LL_AllBlue								= LL_AllColorsOf | LL_Blue,
	LL_AllYellow							= LL_AllColorsOf | LL_Yellow,
	LL_AllRed								= LL_AllColorsOf | LL_Red,

	LL_AllCards								= LL_AllOf | LL_Card,
	LL_AllSkulls							= LL_AllOf | LL_Skull,
	LL_AllKeys								= LL_AllOf | LL_Card | LL_Skull,

	LL_AllCardsOrSkulls						= LL_AllOfEither | LL_Card | LL_Skull,
} linelock_t;

#if defined( __cplusplus )
#include <type_traits>

using underlyinglinetrigger_t = std::underlying_type_t< linetrigger_t >;

constexpr linetrigger_t operator|( const linetrigger_t lhs, const linetrigger_t rhs )
{
	return (linetrigger_t)( (underlyinglinetrigger_t)lhs | (underlyinglinetrigger_t)rhs );
};

constexpr linetrigger_t operator&( const linetrigger_t lhs, const linetrigger_t rhs )
{
	return (linetrigger_t)( (underlyinglinetrigger_t)lhs & (underlyinglinetrigger_t)rhs );
};

constexpr linetrigger_t operator^( const linetrigger_t lhs, const linetrigger_t rhs )
{
	return (linetrigger_t)( (underlyinglinetrigger_t)lhs ^ (underlyinglinetrigger_t)rhs );
};

using underlyinglinelock_t = std::underlying_type_t< linelock_t >;

constexpr linelock_t operator|( const linelock_t lhs, const linelock_t rhs )
{
	return (linelock_t)( (underlyinglinelock_t)lhs | (underlyinglinelock_t)rhs );
};

constexpr linelock_t operator&( const linelock_t lhs, const linelock_t rhs )
{
	return (linelock_t)( (underlyinglinelock_t)lhs & (underlyinglinelock_t)rhs );
};

constexpr linelock_t operator^( const linelock_t lhs, const linelock_t rhs )
{
	return (linelock_t)( (underlyinglinelock_t)lhs ^ (underlyinglinelock_t)rhs );
};
#endif // defined( __cplusplus )

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

	// Generalised parameters interpreted differently by different actions
	int32_t						param1;
	int32_t						param2;
	int32_t						param3;
	int32_t						param4;
	int32_t						param5;
	int32_t						param6;

#if defined(__cplusplus)
	constexpr linetrigger_t ActivationType() const			{ return trigger & LT_ActivationTypeMask; }
	constexpr linetrigger_t AnimatedActivationType()const	{ return trigger & LT_AnimatedActivationTypeMask; }

	void DisplayLockReason( player_t* player );

	INLINE bool Handle(line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside)
	{
		if(precondition(line, activator, activationtype, activationside))
		{
			action(line, activator);
			return true;
		}

		if( activator->player
			&& lock != LL_None
			&& ( activationtype & LT_ActivationTypeMask ) == ( trigger & LT_ActivationTypeMask ) )
		{
			DisplayLockReason( activator->player );
		}
		return false;
	}
#endif // defined(__cplusplus)
};

#define ACTIONPARAM( line, name, index )			line->action->param ## index
#define ACTIONPARAM_INT( line, name, index )		const auto& name = *(int32_t*) &ACTIONPARAM( line, name, index )
#define ACTIONPARAM_FIXED( line, name, index )		const auto& name = *(fixed_t*) &ACTIONPARAM( line, name, index )
#define ACTIONPARAM_BOOL( line, name, index )		const bool name = !!( ACTIONPARAM( line, name, index ) )

DOOM_C_API lineaction_t* P_GetLineActionFor( line_t* line );

#endif // __D_LINEACTION_H__
