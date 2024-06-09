//
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
// DESCRIPTION: Defines the gameflow types for Doom, Doom 2, TNT
//              and Plutonia.
//

#ifndef __D_PLAYSIM_H__
#define __D_PLAYSIM_H__

#include "doomtype.h"

DOOM_C_API typedef struct fixoptions_s
{
	doombool		divlineside;							// Checks against X when it should check against Y
	doombool		findnexthighestfloor;					// Uses the worst algorithm known to humankind to do a minmax
	doombool		findhighestfloorsurrounding;			// Minimum value of -500 hardcoded. Hope you're not making deep low maps...
	doombool		findhighestceilingsurrounding;			// Minimum value of 0 hardcoded. This codebase...
	doombool		unusable_onesided_teleports;			// Vanilla would wipe W1 specials at all times, even if things like teleports didn't trigger
	doombool		same_sky_texture;						// Notorious bug where sky doesn't change between Doom II episodes
	doombool		blockthingsiterator;					// Iterating all items of a linked list famously doesn't work if the list changes
	doombool		intercepts_overflow;					// Stomps over the end of a statically-sized array
	doombool		donut_backsector;						// Donut don't give no hoots about invalid back sectors
	doombool		donut_multiple_sector_thinkers;			// And it didn't check if a thinker existed on the ring sector before creating new ones
	doombool		sky_wall_projectiles;					// The sky checking code when shooting projectiles would cause them to just disappear in to normal walls
	doombool		bad_secret_exit_loop;					// Secret exit on any other map than the hardcoded maps causes the current level to loop
	doombool		w1s1_lines_clearing_on_no_result;		// Vanilla will indiscriminately clear a W1/S1 line even if they do nothing
	doombool		shortest_lower_texture_line;			// Would assume 0 was a valid texture, so never any shorter than AASHITTY
	doombool		moveplane_escapes_reality;				// Independent floor and ceiling thinkers (or the donut bug) causes floors and ceilings to clip through each other
	doombool		overzealous_changesector;				// Sector blockmap overlaps your mobj? That's clearly going to affect that mobj.
	doombool		spechit_overflow;						// Just keep adding things to a static array, it'll be fine
	doombool		missiles_on_blockmap;					// Spawning a missile would just assume it cand do whatever it wants to the mobj's x/y/z coordinates
	doombool		brainspawn_targets;						// Vanilla would just merrily spawn from a null pointer; and would stomp over the stack of an array
	doombool		findheight_ignores_same_sector;			// Find Lowest/Highest/Nearest floors or ceilings never checked if the line's other sector was the same
} fixoptions_t;

DOOM_C_API typedef struct simoptions_s
{
	doombool		extended_saves;							// Generic save format for the new features
	doombool		extended_demos;							// Decide on the fly if we support a demo or not
	doombool		extended_map_formats;					// Different node types etc
	doombool		animated_lump;							// Boom feature, replaces internal table with data
	doombool		switches_lump;							// Boom feature, replaces internal table with data
	doombool		allow_unknown_thing_types;				// Replace unknown things with warning objects
	doombool		weapon_recoil;							// Boom "feature"
	doombool		line_passthrough;						// Boom feature, don't stop using lines when one succeeds if linedef flag is set
	doombool		hud_combined_keys;						// Boom feature, dependent on if resources exist in WAD
	doombool		mobjs_slide_off_edge;					// Boom feature, trust me you'll miss it if it's not there
	doombool		corpse_ignores_mobjs;					// Corpses with velocity still collided as if solid with other things
	doombool		unlimited_scrollers;					// Vanilla limit
	doombool		unlimited_platforms;					// Vanilla limit
	doombool		unlimited_ceilings;						// Vanilla limit
	doombool		generic_specials_handling;				// Rewritten specials, based on Boom standards but considered limit removing
	doombool		separate_floor_ceiling_lights;			// Boom sector specials are allowed to work independently of each other
	doombool		sector_movement_modifiers;				// Boom sectors can have variable friction, wind, currents
	doombool		door_tagged_light;						// Dx with tags does the Boom lighting thing
	doombool		boom_sector_targets;					// All sector targets consider their current sector
	doombool		boom_line_specials;						// Comes with Boom fixes for specials by default
	doombool		boom_sector_specials;					// Comes with Boom fixes for specials by default
	doombool		boom_things;							// Pushers, pullers, TNT1
	doombool		mbf_line_specials;						// Sky transfers, that's it
	doombool		mbf_mobj_flags;							// Bouncy! Friendly! TOUCHY!
	doombool		mbf_code_pointers;						// Dehacked additions
	doombool		mbf_things;								// DOGS
	doombool		mbf21_line_specials;					// New texture scrollers, new flags
	doombool		mbf21_sector_specials;					// Insta-deaths
	doombool		mbf21_thing_extensions;					// infighting, flags2, etc
	doombool		mbf21_code_pointers;					// Dehacked additions
	doombool		rnr24_line_specials;					// Floor/ceiling offsets, music changing, resetting exits, coloured lighting
	doombool		rnr24_thing_extensions;					// New flags, nightmare respawn times
	doombool		rnr24_code_pointers;					// Dehacked extensions (unlimited weapons and ammo types)
	doombool		allow_skydefs;							// Generic skies
} simoptions_t;

// Descriptions pulled from https://www.doomworld.com/forum/topic/72033-boom-mbf-demo-header-format/?tab=comments#comment-1350909
DOOM_C_API typedef struct compoptions_s
{
	doombool		telefrag_map_30;						// comp_telefrag			- Any monster can telefrag on MAP30
	doombool		dropoff_ledges;							// comp_dropoff				- Some objects never hang over tall ledges
	doombool		objects_falloff;						// comp_falloff				- Objects don't fall under their own weight
	doombool		stay_on_lifts;							// comp_staylift			- Monsters randomly walk off of moving lifts
	doombool		stick_on_doors;							// comp_doorstuck			- Monsters get stuck on doortracks
	doombool		dont_give_up_pursuit;					// comp_pursuit				- Monsters don't give up pursuit of targets
	doombool		ghost_monsters;							// comp_vile				- Arch-Vile resurrects invincible ghosts
	doombool		lost_soul_limit;						// comp_pain				- Pain Elementals limited to 21 lost souls
	doombool		lost_souls_behind_walls;				// comp_skull				- Lost souls get stuck behind walls
	doombool		blazing_door_double_sounds;				// comp_blazing				- Blazing doors make double closing sounds
	doombool		door_tagged_light_is_abrupt;			// comp_doorlight			- Tagged doors don't trigger special lighting
	doombool		god_mode_absolute;						// comp_god					- God mode isn't absolute
	doombool		powerup_cheats_infinite;				// comp_infcheat			- Powerup cheats are not infinite duration
	doombool		dead_players_exit_levels;				// comp_zombie				- Dead players can exit levels
	doombool		sky_always_renders_normally;			// comp_skymap				- Sky is unaffected by invulnerability
	doombool		doom_stairbuilding_method;				// comp_stairs				- Use exactly Doom's stairbuilding method
	doombool		doom_floor_movement_method;				// comp_floors				- Use exactly Doom's floor motion behavior
	doombool		doom_movement_clipping_method;			// comp_moveblock			- Use exactly Doom's movement clipping code
	doombool		doom_linedef_trigger_method;			// comp_model				- Use exactly Doom's linedef trigger model
	doombool		zero_tags;								// comp_zerotags			- Linedef effects work with sector tag = 0
	doombool		pre_ultimate_boss_behavior;				// comp_666					- Emulate pre-Ultimate BossDeath behaviour
	doombool		lost_souls_bounce;						// comp_soul				- Lost souls don't bounce off flat surfaces
	doombool		two_sided_texture_anims;				// comp_maskedanim			- 2S middle textures do not animate
	doombool		doom_sound_propagation_method;			// comp_sound				- Retain quirks in Doom's sound code
	doombool		doom_ouch_face_method;					// comp_ouchface			- Use Doom's buggy "Ouch" face code
	doombool		deh_max_health_for_potions;				// comp_maxhealth			- Max Health in DEH applies only to potions
	doombool		translucency_enabled;					// comp_translucency		- No predefined translucency for some things
	doombool		weapon_recoil;							// weapon_recoil			- Weapon recoil
	doombool		player_bobbing;							// player_bobbing			- View bobbing
	doombool		monster_infighting;						// monster_infighting		- Monster Infighting When Provoked
	doombool		monsters_remember_prev_target;			// monsters_remember		- Remember Previous Enemy
	doombool		monsters_back_out;						// monster_backing			- Monster Backing Out
	doombool		monsters_climb_steep_stairs;			// monkeys					- Climb Steep Stairs
	doombool		monsters_avoid_hazards;					// monster_avoid_hazards	- Intelligently Avoid Hazards
	doombool		monsters_affected_by_friction;			// monster_friction			- Affected by Friction
	doombool		monsters_help_friends;					// help_friends				- Rescue Dying Friends
	int32_t			num_helper_dogs;						// player_helpers			- Number Of Single-Player Helper Dogs
	int32_t			friend_minimum_distance;				// friend_distance			- Distance Friends Stay Away
	doombool		dogs_can_jump_down;						// dog_jumping				- Allow dogs to jump down

	// R&R additions
	doombool		finale_use_secondary_lump;				// CREDIT instead of HELP2
	doombool		finale_allow_mouse_to_skip;				// Because everyone has their hands on mouse at all times
	doombool		finale_always_allow_skip_text;			// Doom 1 didn't
	doombool		finaldoom_teleport_z;					// comp_finaldoomteleport	- Final Doom would not set teleported object's Z to the floor
	doombool		reset_player_visited_secret;			// Start a new game, still thinks you've visited secret levels on intermission screen
	doombool		noclip_cheats_work_everywhere;			// idspispopd and idclip everywhere
	doombool		bfg_map02_secret_exit_to_map33;			// Find that one unmarked line in MAP02 in BFG edition
	doombool		demo4;									// Always attempt to play demo4 if it exists
	doombool		support_musinfo;						// comp_musinfo				- The ever-persistent musinfo lump can be selectively turned on

	// R&R render additions
	doombool		additive_data_blocks;					// PP_START, SS_START, FF_START, etc
	doombool		no_medusa;								// Composites are cleared before rendering patches
	doombool		arbitrary_wall_sizes;					// Removes the 128-high requirement for textures to tile
	doombool		any_texture_any_surface;				// Textures and flats work on any surface
	doombool		zero_length_texture_names;				// Allow zero-length names to act like "-"
	doombool		use_translucency;						// Sprites and 2S lines can use translucency maps
	doombool		use_colormaps;							// Colormaps can be used to translate sectors
	doombool		multi_patch_2S_linedefs;				// Now 2S linedefs can render properly. Also handles columns without data
	doombool		widescreen_assets;						// Replace assets with widescreen equivalents if they exist
	doombool		strict_boom_feature_matching;			// When disabled, line 85 (texture scrolling) and thing DM flags will be considered limit removing for version matching
	doombool		tall_skies;								// Sky bigger than 128 high? We've got you covered
	doombool		tall_patches;							// Additive deltas
	doombool		drawpatch_unbounded;					// V_DrawPatch calls will get properly clipped with negative x/y values etc

	// MBF21 additions
	doombool		respawn_non_map_things_at_origin;		// comp_respawn
	doombool		monsters_blocked_by_ledges;				// comp_ledgeblock
	doombool		friendly_inherits_source_attribs;		// comp_friendlyspawn
	doombool		voodoo_scrollers_move_slowly;			// comp_voodooscroller
	doombool		mbf_reserved_flag_disables_flags;		// comp_reservedlineflag
} compoptions_t;

DOOM_C_API typedef struct setupoptions_s
{
	doombool		correct_opl3_by_default;
} setupoptions_t;

DOOM_C_API typedef struct wadoptions_s
{
	doombool		mapinfo_lumps;
	doombool		extended_map_datatypes;
	doombool		unlimited_lumps;
} wadoptions_t;

DOOM_C_API typedef struct savesimoptions_s
{
	doombool		large_size;
	doombool		extended_data;
} savesimoptions_t;

DOOM_C_API extern compoptions_t			comp;
DOOM_C_API extern fixoptions_t			fix;
DOOM_C_API extern simoptions_t			sim;

// This is designed to analyse the IWAD and register the base gameflow.
// It also sets up the current playsim and render options.
DOOM_C_API void D_RegisterGamesim();

#endif // __D_PLAYSIM_H__
