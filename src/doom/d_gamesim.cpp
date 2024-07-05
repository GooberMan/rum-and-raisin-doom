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
// DESCRIPTION: Sets up the active game

#include "d_gamesim.h"

#include "doomdef.h"
#include "doomstat.h"

#include "d_gameconf.h"
#include "d_gameflow.h"

#include "deh_main.h"

#include "i_terminal.h"
#include "i_system.h"

#include "p_lineaction.h"
#include "p_sectoraction.h"

#include "m_argv.h"
#include "m_container.h"
#include "m_fixed.h"

#include "w_wad.h"

#include <functional>

// Externs
extern "C"
{
	compoptions_t			comp;
	fixoptions_t			fix;
	simoptions_t			sim;
}

struct simvalues_t
{
	compoptions_t	comp;
	fixoptions_t	fix;
	simoptions_t	sim;
};

constexpr void SetBoolOptionExact( doombool& option, int32_t val )
{
	option = !!val;
}

constexpr void SetBoolOptionOpposite( doombool& option, int32_t val )
{
	option = !val;
}

struct optionfunc_t
{
	GameVersion_t										gameversion;
	std::function< void( compoptions_t&, int32_t ) >	apply;
};

static std::unordered_map< std::string, optionfunc_t > MBFOptions =
{
	{ "weapon_recoil",			{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.weapon_recoil, val ); } } },
	{ "monsters_remember",		{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.monsters_remember_prev_target, val ); } } },
	{ "monster_infighting",		{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.monster_infighting, val ); } } },
	{ "monster_backing",		{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.monsters_back_out, val ); } } },
	{ "monster_avoid_hazards",	{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.monsters_avoid_hazards, val ); } } },
	{ "monkeys",				{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.monsters_climb_steep_stairs, val ); } } },
	{ "monster_friction",		{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.monsters_affected_by_friction, val ); } } },
	{ "help_friends",			{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.monsters_help_friends, val ); } } },
	{ "player_helpers",			{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { c.num_helper_dogs = val; } } },
	{ "friend_distance",		{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { c.friend_minimum_distance = IntToFixed( val ); } } },
	{ "dog_jumping",			{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.dogs_can_jump_down, val ); } } },
	{ "comp_telefrag",			{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.telefrag_map_30, val ); } } },
	{ "comp_dropoff",			{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.dropoff_ledges, val ); } } },
	{ "comp_vile",				{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.ghost_monsters, val ); } } },
	{ "comp_pain",				{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.lost_soul_limit, val ); } } },
	{ "comp_skull",				{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.lost_souls_behind_walls, val ); } } },
	{ "comp_blazing",			{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.blazing_door_double_sounds, val ); } } },
	{ "comp_doorlight",			{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.door_tagged_light_is_abrupt, val ); } } },
	{ "comp_model",				{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.doom_linedef_trigger_method, val ); } } },
	{ "comp_god",				{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionOpposite( c.god_mode_absolute, val ); } } },
	{ "comp_falloff",			{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionOpposite( c.objects_falloff, val ); } } },
	{ "comp_floors",			{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.doom_floor_movement_method, val ); } } },
	{ "comp_skymap",			{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionOpposite( c.sky_always_renders_normally, val ); } } },
	{ "comp_pursuit",			{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.dont_give_up_pursuit, val ); } } },
	{ "comp_doorstuck",			{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.stick_on_doors, val ); } } },
	{ "comp_staylift",			{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.stay_on_lifts, val ); } } },
	{ "comp_zombie",			{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.dead_players_exit_levels, val ); } } },
	{ "comp_stairs",			{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.doom_stairbuilding_method, val ); } } },
	{ "comp_infcheat",			{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.powerup_cheats_infinite, val ); } } },
	{ "comp_zerotags",			{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.zero_tags, val ); } } },
	{ "comp_soul",				{ exe_doom_1_9,	[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.lost_souls_bounce, val ); } } },
	{ "comp_moveblock",			{ exe_mbf_dehextra,	[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.doom_movement_clipping_method, val ); } } },
	{ "comp_respawn",			{ exe_mbf21,	[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.respawn_non_map_things_at_origin, val ); } } },
	{ "comp_ledgeblock",		{ exe_mbf21,	[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.monsters_blocked_by_ledges, val ); } } },
	{ "comp_friendlyspawn",		{ exe_mbf21,	[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.friendly_inherits_source_attribs, val ); } } },
	{ "comp_voodooscroller",	{ exe_mbf21,	[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.voodoo_scrollers_move_slowly, val ); } } },
	{ "comp_reservedlineflag",	{ exe_mbf21,	[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.mbf_reserved_flag_disables_flags, val ); } } },
	{ "comp_finaldoomteleport",	{ exe_doom_1_9,	[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.finaldoom_teleport_z, val ); } } },
	{ "comp_musinfo",			{ exe_mbf,		[]( compoptions_t& c, int32_t val ) { SetBoolOptionExact( c.support_musinfo, val ); } } },
};

GameVersion_t D_UpdateFromOptionsString( compoptions_t& values, const char* options, size_t optionslen, GameVersion_t maxversion = exe_invalid )
{
	GameVersion_t minversion = exe_invalid;
	if( maxversion == exe_invalid ) maxversion = exe_doommax;

	const char* endoptions = options + optionslen;
	while( options < endoptions )
	{
		if( *options != '\n'
			&& *options != ';'
			&& *options != '[' )
		{
			char param[ 128 ] = {};
			int32_t value = 0;
			if( sscanf( options, "%127s %d", param, &value ) == 2 )
			{
				auto found = MBFOptions.find( param );
				if( found != MBFOptions.end() && found->second.gameversion <= maxversion )
				{
					minversion = M_MAX( minversion, found->second.gameversion );
					found->second.apply( values, value );
				}
			}
		}

		while( options < endoptions && *options++ != '\n' ) {};
	}

	return minversion;
}

static void UpdateFromGameconf( compoptions_t& values, gameconf_t* conf )
{
	if( conf->options && *conf->options != 0 )
	{
		size_t len = strlen( conf->options );
		D_UpdateFromOptionsString( values, conf->options, len, conf->executable );
	}
}

static GameVersion_t UpdateFromOptionsLump( compoptions_t& values )
{
	lumpindex_t optionslump = W_CheckNumForNameExcluding( "OPTIONS", wt_system );
	if( optionslump >= 0 )
	{
		const char* options = (const char*)W_CacheLumpNum( optionslump, PU_STATIC );
		return D_UpdateFromOptionsString( values, options, W_LumpLength( optionslump ) );
	}

	return exe_invalid;
}

static simvalues_t GetVanillaValues( GameVersion_t version, GameMode_t mode )
{
	simvalues_t values = {};

	values.comp.telefrag_map_30 = true;						// comp_telefrag
	values.comp.dropoff_ledges = true;						// comp_dropoff
	values.comp.objects_falloff = true;						// comp_falloff
	values.comp.stay_on_lifts = true;						// comp_staylift
	values.comp.stick_on_doors = true;						// comp_doorstuck
	values.comp.dont_give_up_pursuit = true;				// comp_pursuit
	values.comp.ghost_monsters = true;						// comp_vile
	values.comp.lost_soul_limit = true;						// comp_pain
	values.comp.lost_souls_behind_walls = true;				// comp_skull
	values.comp.blazing_door_double_sounds = true;			// comp_blazing
	values.comp.door_tagged_light_is_abrupt = true;			// comp_doorlight
	values.comp.god_mode_absolute = false;					// comp_god
	values.comp.powerup_cheats_infinite = false;			// comp_infcheat
	values.comp.dead_players_exit_levels = true;			// comp_zombie
	values.comp.sky_always_renders_normally = true;			// comp_skymap
	values.comp.doom_stairbuilding_method = true;			// comp_stairs
	values.comp.doom_floor_movement_method = true;			// comp_floors
	values.comp.doom_movement_clipping_method = true;		// comp_moveblock
	values.comp.doom_linedef_trigger_method = true;			// comp_model
	values.comp.zero_tags = true;							// comp_zerotags
	values.comp.pre_ultimate_boss_behavior = false;			// comp_666
	values.comp.lost_souls_bounce = ( version >= exe_ultimate );	// comp_soul
	values.comp.two_sided_texture_anims = true;				// comp_maskedanim
	values.comp.doom_sound_propagation_method = true;		// comp_sound
	values.comp.doom_ouch_face_method = true;				// comp_ouchface
	values.comp.deh_max_health_for_potions = false;			// comp_maxhealth
	values.comp.translucency_enabled = true;				// comp_translucency
	values.comp.weapon_recoil = false;						// weapon_recoil
	values.comp.player_bobbing = true;						// player_bobbing
	values.comp.monster_infighting = true;					// monster_infighting
	values.comp.monsters_remember_prev_target = false;		// monsters_remember
	values.comp.monsters_back_out = false;					// monster_backing
	values.comp.monsters_climb_steep_stairs = false;		// monkeys
	values.comp.monsters_avoid_hazards = false;				// monster_avoid_hazards
	values.comp.monsters_affected_by_friction = false;		// monster_friction
	values.comp.monsters_help_friends = false;				// help_friends
	values.comp.num_helper_dogs = 0;						// player_helpers
	values.comp.friend_minimum_distance = 0;				// friend_distance
	values.comp.dogs_can_jump_down = false;					// dog_jumping

	values.comp.finale_use_secondary_lump = ( mode == retail );	// CREDIT instead of HELP2
	values.comp.finale_allow_mouse_to_skip = false;			// Because everyone has their hands on mouse at all times
	values.comp.finale_always_allow_skip_text = false;		// Doom 1 didn't
	values.comp.finaldoom_teleport_z = false;				// Final Doom would not set teleported object's Z to the floor
	values.comp.reset_player_visited_secret = false;		// Start a new game, still thinks you've visited secret levels on intermission screen
	values.comp.noclip_cheats_work_everywhere = false;		// idspispopd and idclip everywhere
	values.comp.bfg_map02_secret_exit_to_map33 = false;		// Find that one unmarked line in MAP02 in BFG edition

	values.comp.respawn_non_map_things_at_origin = true;	// comp_respawn
	values.comp.monsters_blocked_by_ledges = true;			// comp_ledgeblock
	values.comp.friendly_inherits_source_attribs = true;	// comp_friendlyspawn
	values.comp.voodoo_scrollers_move_slowly = false;		// comp_voodooscroller
	values.comp.mbf_reserved_flag_disables_flags = true;	// comp_reservedlineflag

	if( gameconf ) UpdateFromGameconf( values.comp, gameconf );

	return values;
}

static simvalues_t GetLimitRemovingValues( GameMode_t mode )
{
	simvalues_t values = GetVanillaValues( exe_limit_removing, mode );

	values.sim.extended_saves = true;
	values.sim.extended_map_formats = true;
	values.sim.generic_specials_handling = true;
	values.sim.hud_combined_keys = true;
	values.sim.unlimited_scrollers = true;
	values.sim.unlimited_platforms = true;
	values.sim.unlimited_ceilings = true;
	values.sim.allow_skydefs = true;
	values.sim.allow_sbardefs = true;

	values.comp.finale_allow_mouse_to_skip = true;
	values.comp.finale_always_allow_skip_text = true;
	values.comp.reset_player_visited_secret = true;
	values.comp.noclip_cheats_work_everywhere = true;
	values.comp.demo4 = W_CheckNumForName("DEMO4") >= 0;

	values.comp.additive_data_blocks = true;
	values.comp.no_medusa = true;
	values.comp.arbitrary_wall_sizes = true;
	values.comp.any_texture_any_surface = true;
	values.comp.zero_length_texture_names = true;
	values.comp.multi_patch_2S_linedefs = true;
	values.comp.widescreen_assets = true;
	values.comp.tall_skies = true;
	values.comp.tall_patches = true;
	values.comp.drawpatch_unbounded = true;

	values.fix.same_sky_texture = true;
	values.fix.intercepts_overflow = true;
	values.fix.spechit_overflow = true;
	values.fix.brainspawn_targets = true;
	values.fix.really_needed_medikit = true;
	values.fix.revenant_tracer_desync = true;

	if( gameconf ) UpdateFromGameconf( values.comp, gameconf );

	return values;
}

static fixoptions_t GetAllFixed()
{
	fixoptions_t options = {};

	options.divlineside = true;
	options.findnexthighestfloor = true;
	options.findhighestfloorsurrounding = true;
	options.findhighestceilingsurrounding = true;
	options.unusable_onesided_teleports = true;
	options.same_sky_texture = true;
	options.blockthingsiterator = true;
	options.intercepts_overflow = true;
	options.donut_backsector = true;
	options.donut_multiple_sector_thinkers = true;
	options.sky_wall_projectiles = true;
	options.bad_secret_exit_loop = true;
	options.w1_lines_clearing_on_no_result = true;
	options.shortest_lower_texture_line = true;
	options.moveplane_escapes_reality = true;
	options.overzealous_changesector = true;
	options.spechit_overflow = true;
	options.missiles_on_blockmap = true;
	options.brainspawn_targets = true;
	options.findheight_ignores_same_sector = true;
	options.really_needed_medikit = true;
	options.revenant_tracer_desync = true;

	return options;
}

static simvalues_t GetLimitRemovingFixedValues( GameMode_t mode )
{
	simvalues_t values = GetLimitRemovingValues( mode );
	values.fix = GetAllFixed();
	return values;
}

static simvalues_t GetBoomValues( GameMode_t mode )
{
	simvalues_t values = {};

	values.comp.telefrag_map_30 = true;						// comp_telefrag
	values.comp.dropoff_ledges = true;						// comp_dropoff
	values.comp.objects_falloff = true;						// comp_falloff
	values.comp.stay_on_lifts = true;						// comp_staylift
	values.comp.stick_on_doors = false;						// comp_doorstuck
	values.comp.dont_give_up_pursuit = true;				// comp_pursuit
	values.comp.ghost_monsters = false;						// comp_vile
	values.comp.lost_soul_limit = false;					// comp_pain
	values.comp.lost_souls_behind_walls = false;			// comp_skull
	values.comp.blazing_door_double_sounds = false;			// comp_blazing
	values.comp.door_tagged_light_is_abrupt = true;			// comp_doorlight
	values.comp.god_mode_absolute = true;					// comp_god
	values.comp.powerup_cheats_infinite = true;				// comp_infcheat
	values.comp.dead_players_exit_levels = true;			// comp_zombie
	values.comp.sky_always_renders_normally = false;		// comp_skymap
	values.comp.doom_stairbuilding_method = false;			// comp_stairs
	values.comp.doom_floor_movement_method = false;			// comp_floors
	values.comp.doom_movement_clipping_method = true;		// comp_moveblock
	values.comp.doom_linedef_trigger_method = false;		// comp_model
	values.comp.zero_tags = false;							// comp_zerotags
	values.comp.pre_ultimate_boss_behavior = false;			// comp_666
	values.comp.lost_souls_bounce = true;					// comp_soul
	values.comp.two_sided_texture_anims = true;				// comp_maskedanim
	values.comp.doom_sound_propagation_method = true;		// comp_sound
	values.comp.doom_ouch_face_method = true;				// comp_ouchface
	values.comp.deh_max_health_for_potions = false;			// comp_maxhealth
	values.comp.translucency_enabled = false;				// comp_translucency
	values.comp.weapon_recoil = false;						// weapon_recoil
	values.comp.player_bobbing = true;						// player_bobbing
	values.comp.monster_infighting = true;					// monster_infighting
	values.comp.monsters_remember_prev_target = true;		// monsters_remember
	values.comp.monsters_back_out = false;					// monster_backing
	values.comp.monsters_climb_steep_stairs = false;		// monkeys
	values.comp.monsters_avoid_hazards = false;				// monster_avoid_hazards
	values.comp.monsters_affected_by_friction = false;		// monster_friction
	values.comp.monsters_help_friends = false;				// help_friends
	values.comp.num_helper_dogs = 0;						// player_helpers
	values.comp.friend_minimum_distance = 0;				// friend_distance
	values.comp.dogs_can_jump_down = false;					// dog_jumping

	values.comp.finale_use_secondary_lump = ( mode == retail );	// CREDIT instead of HELP2
	values.comp.finale_allow_mouse_to_skip = true;			// Because everyone has their hands on mouse at all times
	values.comp.finale_always_allow_skip_text = true;		// Doom 1 didn't
	values.comp.finaldoom_teleport_z = false;				// Final Doom would not set teleported object's Z to the floor
	values.comp.reset_player_visited_secret = true;			// Start a new game, still thinks you've visited secret levels on intermission screen
	values.comp.noclip_cheats_work_everywhere = true;		// idspispopd and idclip everywhere
	values.comp.bfg_map02_secret_exit_to_map33 = false;		// Find that one unmarked line in MAP02 in BFG edition
	values.comp.demo4 = W_CheckNumForName("DEMO4") >= 0;		// Always attempt to play demo4 if it exists

	values.comp.additive_data_blocks = true;				// PP_START, SS_START, FF_START, etc
	values.comp.no_medusa = true;							// Composites are cleared before rendering patches
	values.comp.arbitrary_wall_sizes = true;				// Removes the 128-high requirement for textures to tile
	values.comp.any_texture_any_surface = true;				// Textures and flats work on any surface
	values.comp.zero_length_texture_names = true;			// Allow zero-length names to act like "-"
	values.comp.use_translucency = true;					// Sprites and 2S lines can use translucency maps
	values.comp.use_colormaps = true;						// Colormaps can be used to translate sectors
	values.comp.multi_patch_2S_linedefs = true;				// Now 2S linedefs can render properly. Also handles columns without data
	values.comp.widescreen_assets = true;					// Replace assets with widescreen equivalents if they exist
	values.comp.tall_skies = true;							// Sky bigger than 128 high? We've got you covered
	values.comp.tall_patches = true;						// Additive deltas
	values.comp.drawpatch_unbounded = true;					// V_DrawPatch calls will get properly clipped with negative x/y values etc

	values.comp.respawn_non_map_things_at_origin = true;	// comp_respawn
	values.comp.monsters_blocked_by_ledges = false;			// comp_ledgeblock
	values.comp.friendly_inherits_source_attribs = true;	// comp_friendlyspawn
	values.comp.voodoo_scrollers_move_slowly = false;		// comp_voodooscroller
	values.comp.mbf_reserved_flag_disables_flags = true;	// comp_reservedlineflag

	values.fix = GetAllFixed();

	values.sim.extended_saves = true;						// Generic save format for the new features
	values.sim.extended_demos = true;						// Decide on the fly if we support a demo or not
	values.sim.extended_map_formats = true;					// Different node types etc
	values.sim.animated_lump = true;						// Boom feature, replaces internal table with data
	values.sim.switches_lump = true;						// Boom feature, replaces internal table with data
	values.sim.allow_unknown_thing_types = true;			// Replace unknown things with warning objects
	values.sim.weapon_recoil = true;						// Boom "feature"
	values.sim.line_passthrough = true;						// Boom feature, don't stop using lines when one succeeds if linedef flag is set
	values.sim.hud_combined_keys = true;					// Boom feature, dependent on if resources exist in WAD
	values.sim.mobjs_slide_off_edge = true;					// Boom feature, trust me you'll miss it if it's not there
	values.sim.corpse_ignores_mobjs = true;					// Corpses with velocity still collided as if solid with other things
	values.sim.unlimited_scrollers = true;					// Vanilla limit
	values.sim.unlimited_platforms = true;					// Vanilla limit
	values.sim.unlimited_ceilings = true;					// Vanilla limit
	values.sim.generic_specials_handling = true;			// Rewritten specials, based on Boom standards but considered limit removing
	values.sim.separate_floor_ceiling_lights = true;		// Boom sector specials are allowed to work independently of each other
	values.sim.sector_movement_modifiers = true;			// Boom sectors can have variable friction, wind, currents
	values.sim.door_tagged_light = true;					// Dx with tags does the Boom lighting thing
	values.sim.boom_sector_targets = true;					// All sector targets consider their current sector
	values.sim.boom_line_specials = true;					// Comes with Boom fixes for specials by default
	values.sim.boom_sector_specials = true;					// Comes with Boom fixes for specials by default
	values.sim.boom_things = true;							// Pushers, pullers, TNT1
	values.sim.mbf_line_specials = false;					// Sky transfers, that's it
	values.sim.mbf_mobj_flags = false;						// Bouncy! Friendly! TOUCHY!
	values.sim.mbf_code_pointers = false;					// Dehacked additions
	values.sim.mbf_things = false;							// DOGS
	values.sim.mbf21_line_specials = false;					// New texture scrollers, new flags
	values.sim.mbf21_sector_specials = false;				// Insta-deaths
	values.sim.mbf21_thing_extensions = false;				// infighting, flags2, etc
	values.sim.mbf21_code_pointers = false;					// Dehacked additions
	values.sim.rnr24_line_specials = false;					// Floor/ceiling offsets, music changing, resetting exits, coloured lighting
	values.sim.rnr24_thing_extensions = false;				// New flags, nightmare respawn times
	values.sim.rnr24_code_pointers = false;					// Dehacked extensions (unlimited weapons and ammo types)
	values.sim.allow_skydefs = true;						// Generic skies
	values.sim.allow_sbardefs = true;						// Generic status bars

	if( gameconf ) UpdateFromGameconf( values.comp, gameconf );

	return values;
}

static simvalues_t GetComplevel9Values( GameMode_t mode )
{
	simvalues_t values = GetBoomValues( mode );

	values.sim.mbf_line_specials = true;

	return values;
}

static simvalues_t GetMBFValues( GameMode_t mode )
{
	simvalues_t values = {};

	values.comp.telefrag_map_30 = false;					// comp_telefrag
	values.comp.dropoff_ledges = true;						// comp_dropoff
	values.comp.objects_falloff = true;						// comp_falloff
	values.comp.stay_on_lifts = true;						// comp_staylift
	values.comp.stick_on_doors = false;						// comp_doorstuck
	values.comp.dont_give_up_pursuit = false;				// comp_pursuit
	values.comp.ghost_monsters = false;						// comp_vile
	values.comp.lost_soul_limit = false;					// comp_pain
	values.comp.lost_souls_behind_walls = false;			// comp_skull
	values.comp.blazing_door_double_sounds = false;			// comp_blazing
	values.comp.door_tagged_light_is_abrupt = false;		// comp_doorlight
	values.comp.god_mode_absolute = true;					// comp_god
	values.comp.powerup_cheats_infinite = true;				// comp_infcheat
	values.comp.dead_players_exit_levels = true;			// comp_zombie
	values.comp.sky_always_renders_normally = false;		// comp_skymap
	values.comp.doom_stairbuilding_method = false;			// comp_stairs
	values.comp.doom_floor_movement_method = false;			// comp_floors
	values.comp.doom_movement_clipping_method = true;		// comp_moveblock
	values.comp.doom_linedef_trigger_method = false;		// comp_model
	values.comp.zero_tags = false;							// comp_zerotags
	values.comp.pre_ultimate_boss_behavior = false;			// comp_666
	values.comp.lost_souls_bounce = true;					// comp_soul
	values.comp.two_sided_texture_anims = true;				// comp_maskedanim
	values.comp.doom_sound_propagation_method = true;		// comp_sound
	values.comp.doom_ouch_face_method = true;				// comp_ouchface
	values.comp.deh_max_health_for_potions = false;			// comp_maxhealth
	values.comp.translucency_enabled = false;				// comp_translucency
	values.comp.weapon_recoil = false;						// weapon_recoil
	values.comp.player_bobbing = true;						// player_bobbing
	values.comp.monster_infighting = true;					// monster_infighting
	values.comp.monsters_remember_prev_target = true;		// monsters_remember
	values.comp.monsters_back_out = false;					// monster_backing
	values.comp.monsters_climb_steep_stairs = false;		// monkeys
	values.comp.monsters_avoid_hazards = true;				// monster_avoid_hazards
	values.comp.monsters_affected_by_friction = true;		// monster_friction
	values.comp.monsters_help_friends = false;				// help_friends
	values.comp.num_helper_dogs = 0;						// player_helpers
	values.comp.friend_minimum_distance = IntToFixed( 128 ); // friend_distance
	values.comp.dogs_can_jump_down = true;					// dog_jumping

	values.comp.finale_use_secondary_lump = ( mode == retail );	// CREDIT instead of HELP2
	values.comp.finale_allow_mouse_to_skip = true;			// Because everyone has their hands on mouse at all times
	values.comp.finale_always_allow_skip_text = true;		// Doom 1 didn't
	values.comp.finaldoom_teleport_z = false;				// Final Doom would not set teleported object's Z to the floor
	values.comp.reset_player_visited_secret = true;			// Start a new game, still thinks you've visited secret levels on intermission screen
	values.comp.noclip_cheats_work_everywhere = true;		// idspispopd and idclip everywhere
	values.comp.bfg_map02_secret_exit_to_map33 = false;		// Find that one unmarked line in MAP02 in BFG edition
	values.comp.demo4 = W_CheckNumForName("DEMO4") >= 0;	// Always attempt to play demo4 if it exists
	values.comp.support_musinfo = true;						// comp_musinfo

	values.comp.additive_data_blocks = true;				// PP_START, SS_START, FF_START, etc
	values.comp.no_medusa = true;							// Composites are cleared before rendering patches
	values.comp.arbitrary_wall_sizes = true;				// Removes the 128-high requirement for textures to tile
	values.comp.any_texture_any_surface = true;				// Textures and flats work on any surface
	values.comp.zero_length_texture_names = true;			// Allow zero-length names to act like "-"
	values.comp.use_translucency = true;					// Sprites and 2S lines can use translucency maps
	values.comp.use_colormaps = true;						// Colormaps can be used to translate sectors
	values.comp.multi_patch_2S_linedefs = true;				// Now 2S linedefs can render properly. Also handles columns without data
	values.comp.widescreen_assets = true;					// Replace assets with widescreen equivalents if they exist
	values.comp.tall_skies = true;							// Sky bigger than 128 high? We've got you covered
	values.comp.tall_patches = true;						// Additive deltas
	values.comp.drawpatch_unbounded = true;					// V_DrawPatch calls will get properly clipped with negative x/y values etc

	values.comp.respawn_non_map_things_at_origin = true;	// comp_respawn
	values.comp.monsters_blocked_by_ledges = false;			// comp_ledgeblock
	values.comp.friendly_inherits_source_attribs = true;	// comp_friendlyspawn
	values.comp.voodoo_scrollers_move_slowly = true;		// comp_voodooscroller
	values.comp.mbf_reserved_flag_disables_flags = true;	// comp_reservedlineflag

	values.fix = GetAllFixed();

	values.sim.extended_saves = true;						// Generic save format for the new features
	values.sim.extended_demos = true;						// Decide on the fly if we support a demo or not
	values.sim.extended_map_formats = true;					// Different node types etc
	values.sim.animated_lump = true;						// Boom feature, replaces internal table with data
	values.sim.switches_lump = true;						// Boom feature, replaces internal table with data
	values.sim.allow_unknown_thing_types = true;			// Replace unknown things with warning objects
	values.sim.weapon_recoil = true;						// Boom "feature"
	values.sim.line_passthrough = true;						// Boom feature, don't stop using lines when one succeeds if linedef flag is set
	values.sim.hud_combined_keys = true;					// Boom feature, dependent on if resources exist in WAD
	values.sim.mobjs_slide_off_edge = true;					// Boom feature, trust me you'll miss it if it's not there
	values.sim.corpse_ignores_mobjs = true;					// Corpses with velocity still collided as if solid with other things
	values.sim.unlimited_scrollers = true;					// Vanilla limit
	values.sim.unlimited_platforms = true;					// Vanilla limit
	values.sim.unlimited_ceilings = true;					// Vanilla limit
	values.sim.generic_specials_handling = true;			// Rewritten specials, based on Boom standards but considered limit removing
	values.sim.separate_floor_ceiling_lights = true;		// Boom sector specials are allowed to work independently of each other
	values.sim.sector_movement_modifiers = true;			// Boom sectors can have variable friction, wind, currents
	values.sim.door_tagged_light = true;					// Dx with tags does the Boom lighting thing
	values.sim.boom_sector_targets = true;					// All sector targets consider their current sector
	values.sim.boom_line_specials = true;					// Comes with Boom fixes for specials by default
	values.sim.boom_sector_specials = true;					// Comes with Boom fixes for specials by default
	values.sim.boom_things = true;							// Pushers, pullers, TNT1
	values.sim.mbf_line_specials = true;					// Sky transfers, that's it
	values.sim.mbf_mobj_flags = true;						// Bouncy! Friendly! TOUCHY!
	values.sim.mbf_code_pointers = true;					// Dehacked additions
	values.sim.mbf_things = true;							// DOGS
	values.sim.mbf21_line_specials = false;					// New texture scrollers, new flags
	values.sim.mbf21_sector_specials = false;				// Insta-deaths
	values.sim.mbf21_thing_extensions = false;				// infighting, flags2, etc
	values.sim.mbf21_code_pointers = false;					// Dehacked additions
	values.sim.rnr24_line_specials = false;					// Floor/ceiling offsets, music changing, resetting exits, coloured lighting
	values.sim.rnr24_thing_extensions = false;				// New flags, nightmare respawn times
	values.sim.rnr24_code_pointers = false;					// Dehacked extensions (unlimited weapons and ammo types)
	values.sim.allow_skydefs = true;						// Generic skies
	values.sim.allow_sbardefs = true;						// Generic status bars

	if( gameconf ) UpdateFromGameconf( values.comp, gameconf );
	UpdateFromOptionsLump( values.comp );

	return values;
}

static simvalues_t GetMBF21Values( GameMode_t mode )
{
	simvalues_t values = {};

	values.comp.telefrag_map_30 = true;						// comp_telefrag
	values.comp.dropoff_ledges = true;						// comp_dropoff
	values.comp.objects_falloff = true;						// comp_falloff
	values.comp.stay_on_lifts = true;						// comp_staylift
	values.comp.stick_on_doors = false;						// comp_doorstuck
	values.comp.dont_give_up_pursuit = true;				// comp_pursuit
	values.comp.ghost_monsters = true;						// comp_vile
	values.comp.lost_soul_limit = true;						// comp_pain
	values.comp.lost_souls_behind_walls = true;				// comp_skull
	values.comp.blazing_door_double_sounds = true;			// comp_blazing
	values.comp.door_tagged_light_is_abrupt = false;		// comp_doorlight
	values.comp.god_mode_absolute = false;					// comp_god
	values.comp.powerup_cheats_infinite = false;			// comp_infcheat
	values.comp.dead_players_exit_levels = true;			// comp_zombie
	values.comp.sky_always_renders_normally = true;			// comp_skymap
	values.comp.doom_stairbuilding_method = true;			// comp_stairs
	values.comp.doom_floor_movement_method = true;			// comp_floors
	values.comp.doom_movement_clipping_method = true;		// comp_moveblock
	values.comp.doom_linedef_trigger_method = true;			// comp_model
	values.comp.zero_tags = true;							// comp_zerotags
	values.comp.pre_ultimate_boss_behavior = false;			// comp_666
	values.comp.lost_souls_bounce = true;					// comp_soul
	values.comp.two_sided_texture_anims = true;				// comp_maskedanim
	values.comp.doom_sound_propagation_method = true;		// comp_sound
	values.comp.doom_ouch_face_method = true;				// comp_ouchface
	values.comp.deh_max_health_for_potions = false;			// comp_maxhealth
	values.comp.translucency_enabled = true;				// comp_translucency
	values.comp.weapon_recoil = false;						// weapon_recoil
	values.comp.player_bobbing = true;						// player_bobbing
	values.comp.monster_infighting = true;					// monster_infighting
	values.comp.monsters_remember_prev_target = false;		// monsters_remember
	values.comp.monsters_back_out = false;					// monster_backing
	values.comp.monsters_climb_steep_stairs = false;		// monkeys
	values.comp.monsters_avoid_hazards = false;				// monster_avoid_hazards
	values.comp.monsters_affected_by_friction = true;		// monster_friction
	values.comp.monsters_help_friends = false;				// help_friends
	values.comp.num_helper_dogs = 0;						// player_helpers
	values.comp.friend_minimum_distance = IntToFixed( 128 ); // friend_distance
	values.comp.dogs_can_jump_down = false;					// dog_jumping

	values.comp.finale_use_secondary_lump = ( mode == retail );	// CREDIT instead of HELP2
	values.comp.finale_allow_mouse_to_skip = true;			// Because everyone has their hands on mouse at all times
	values.comp.finale_always_allow_skip_text = true;		// Doom 1 didn't
	values.comp.finaldoom_teleport_z = false;				// Final Doom would not set teleported object's Z to the floor
	values.comp.reset_player_visited_secret = false;		// Start a new game, still thinks you've visited secret levels on intermission screen
	values.comp.noclip_cheats_work_everywhere = true;		// idspispopd and idclip everywhere
	values.comp.bfg_map02_secret_exit_to_map33 = false;		// Find that one unmarked line in MAP02 in BFG edition
	values.comp.demo4 = W_CheckNumForName("DEMO4") >= 0;	// Always attempt to play demo4 if it exists
	values.comp.support_musinfo = true;						// comp_musinfo

	values.comp.additive_data_blocks = true;				// PP_START, SS_START, FF_START, etc
	values.comp.no_medusa = true;							// Composites are cleared before rendering patches
	values.comp.arbitrary_wall_sizes = true;				// Removes the 128-high requirement for textures to tile
	values.comp.any_texture_any_surface = true;				// Textures and flats work on any surface
	values.comp.zero_length_texture_names = true;			// Allow zero-length names to act like "-"
	values.comp.use_translucency = true;					// Sprites and 2S lines can use translucency maps
	values.comp.use_colormaps = true;						// Colormaps can be used to translate sectors
	values.comp.multi_patch_2S_linedefs = true;				// Now 2S linedefs can render properly. Also handles columns without data
	values.comp.widescreen_assets = true;					// Replace assets with widescreen equivalents if they exist
	values.comp.tall_skies = true;							// Sky bigger than 128 high? We've got you covered
	values.comp.tall_patches = true;						// Additive deltas
	values.comp.drawpatch_unbounded = true;					// V_DrawPatch calls will get properly clipped with negative x/y values etc

	values.comp.respawn_non_map_things_at_origin = true;	// comp_respawn
	values.comp.monsters_blocked_by_ledges = true;			// comp_ledgeblock
	values.comp.friendly_inherits_source_attribs = true;	// comp_friendlyspawn
	values.comp.voodoo_scrollers_move_slowly = false;		// comp_voodooscroller
	values.comp.mbf_reserved_flag_disables_flags = true;	// comp_reservedlineflag

	values.fix = GetAllFixed();

	values.sim.extended_saves = true;						// Generic save format for the new features
	values.sim.extended_demos = true;						// Decide on the fly if we support a demo or not
	values.sim.extended_map_formats = true;					// Different node types etc
	values.sim.animated_lump = true;						// Boom feature, replaces internal table with data
	values.sim.switches_lump = true;						// Boom feature, replaces internal table with data
	values.sim.allow_unknown_thing_types = true;			// Replace unknown things with warning objects
	values.sim.weapon_recoil = true;						// Boom "feature"
	values.sim.line_passthrough = true;						// Boom feature, don't stop using lines when one succeeds if linedef flag is set
	values.sim.hud_combined_keys = true;					// Boom feature, dependent on if resources exist in WAD
	values.sim.mobjs_slide_off_edge = true;					// Boom feature, trust me you'll miss it if it's not there
	values.sim.corpse_ignores_mobjs = true;					// Corpses with velocity still collided as if solid with other things
	values.sim.unlimited_scrollers = true;					// Vanilla limit
	values.sim.unlimited_platforms = true;					// Vanilla limit
	values.sim.unlimited_ceilings = true;					// Vanilla limit
	values.sim.generic_specials_handling = true;			// Rewritten specials, based on Boom standards but considered limit removing
	values.sim.separate_floor_ceiling_lights = true;		// Boom sector specials are allowed to work independently of each other
	values.sim.sector_movement_modifiers = true;			// Boom sectors can have variable friction, wind, currents
	values.sim.door_tagged_light = true;					// Dx with tags does the Boom lighting thing
	values.sim.boom_sector_targets = true;					// All sector targets consider their current sector
	values.sim.boom_line_specials = true;					// Comes with Boom fixes for specials by default
	values.sim.boom_sector_specials = true;					// Comes with Boom fixes for specials by default
	values.sim.boom_things = true;							// Pushers, pullers, TNT1
	values.sim.mbf_line_specials = true;					// Sky transfers, that's it
	values.sim.mbf_mobj_flags = true;						// Bouncy! Friendly! TOUCHY!
	values.sim.mbf_code_pointers = true;					// Dehacked additions
	values.sim.mbf_things = true;							// DOGS
	values.sim.mbf21_line_specials = true;					// New texture scrollers, new flags
	values.sim.mbf21_sector_specials = true;				// Insta-deaths
	values.sim.mbf21_thing_extensions = true;				// infighting, flags2, etc
	values.sim.mbf21_code_pointers = true;					// Dehacked additions
	values.sim.rnr24_line_specials = false;					// Floor/ceiling offsets, music changing, resetting exits, coloured lighting
	values.sim.rnr24_thing_extensions = false;				// New flags, nightmare respawn times
	values.sim.rnr24_code_pointers = false;					// Dehacked extensions (unlimited weapons and ammo types)
	values.sim.allow_skydefs = true;						// Generic skies
	values.sim.allow_sbardefs = true;						// Generic status bars

	if( gameconf ) UpdateFromGameconf( values.comp, gameconf );
	UpdateFromOptionsLump( values.comp );

	return values;
}

static simvalues_t GetRNR24Values( GameMode_t mode )
{
	simvalues_t values = {};

	values.comp.telefrag_map_30 = true;						// comp_telefrag
	values.comp.dropoff_ledges = true;						// comp_dropoff
	values.comp.objects_falloff = true;						// comp_falloff
	values.comp.stay_on_lifts = true;						// comp_staylift
	values.comp.stick_on_doors = false;						// comp_doorstuck
	values.comp.dont_give_up_pursuit = true;				// comp_pursuit
	values.comp.ghost_monsters = true;						// comp_vile
	values.comp.lost_soul_limit = true;						// comp_pain
	values.comp.lost_souls_behind_walls = true;				// comp_skull
	values.comp.blazing_door_double_sounds = true;			// comp_blazing
	values.comp.door_tagged_light_is_abrupt = false;		// comp_doorlight
	values.comp.god_mode_absolute = false;					// comp_god
	values.comp.powerup_cheats_infinite = false;			// comp_infcheat
	values.comp.dead_players_exit_levels = true;			// comp_zombie
	values.comp.sky_always_renders_normally = true;			// comp_skymap
	values.comp.doom_stairbuilding_method = true;			// comp_stairs
	values.comp.doom_floor_movement_method = true;			// comp_floors
	values.comp.doom_movement_clipping_method = true;		// comp_moveblock
	values.comp.doom_linedef_trigger_method = true;			// comp_model
	values.comp.zero_tags = true;							// comp_zerotags
	values.comp.pre_ultimate_boss_behavior = false;			// comp_666
	values.comp.lost_souls_bounce = true;					// comp_soul
	values.comp.two_sided_texture_anims = true;				// comp_maskedanim
	values.comp.doom_sound_propagation_method = true;		// comp_sound
	values.comp.doom_ouch_face_method = true;				// comp_ouchface
	values.comp.deh_max_health_for_potions = false;			// comp_maxhealth
	values.comp.translucency_enabled = true;				// comp_translucency
	values.comp.weapon_recoil = false;						// weapon_recoil
	values.comp.player_bobbing = true;						// player_bobbing
	values.comp.monster_infighting = true;					// monster_infighting
	values.comp.monsters_remember_prev_target = false;		// monsters_remember
	values.comp.monsters_back_out = false;					// monster_backing
	values.comp.monsters_climb_steep_stairs = false;		// monkeys
	values.comp.monsters_avoid_hazards = false;				// monster_avoid_hazards
	values.comp.monsters_affected_by_friction = true;		// monster_friction
	values.comp.monsters_help_friends = false;				// help_friends
	values.comp.num_helper_dogs = 0;						// player_helpers
	values.comp.friend_minimum_distance = IntToFixed( 128 ); // friend_distance
	values.comp.dogs_can_jump_down = false;					// dog_jumping

	values.comp.finale_use_secondary_lump = ( mode == retail );	// CREDIT instead of HELP2
	values.comp.finale_allow_mouse_to_skip = true;			// Because everyone has their hands on mouse at all times
	values.comp.finale_always_allow_skip_text = true;		// Doom 1 didn't
	values.comp.finaldoom_teleport_z = false;				// Final Doom would not set teleported object's Z to the floor
	values.comp.reset_player_visited_secret = false;		// Start a new game, still thinks you've visited secret levels on intermission screen
	values.comp.noclip_cheats_work_everywhere = true;		// idspispopd and idclip everywhere
	values.comp.bfg_map02_secret_exit_to_map33 = false;		// Find that one unmarked line in MAP02 in BFG edition
	values.comp.demo4 = W_CheckNumForName("DEMO4") >= 0;	// Always attempt to play demo4 if it exists
	values.comp.support_musinfo = true;						// comp_musinfo

	values.comp.additive_data_blocks = true;				// PP_START, SS_START, FF_START, etc
	values.comp.no_medusa = true;							// Composites are cleared before rendering patches
	values.comp.arbitrary_wall_sizes = true;				// Removes the 128-high requirement for textures to tile
	values.comp.any_texture_any_surface = true;				// Textures and flats work on any surface
	values.comp.zero_length_texture_names = true;			// Allow zero-length names to act like "-"
	values.comp.use_translucency = true;					// Sprites and 2S lines can use translucency maps
	values.comp.use_colormaps = true;						// Colormaps can be used to translate sectors
	values.comp.multi_patch_2S_linedefs = true;				// Now 2S linedefs can render properly. Also handles columns without data
	values.comp.widescreen_assets = true;					// Replace assets with widescreen equivalents if they exist
	values.comp.tall_skies = true;							// Sky bigger than 128 high? We've got you covered
	values.comp.tall_patches = true;						// Additive deltas
	values.comp.drawpatch_unbounded = true;					// V_DrawPatch calls will get properly clipped with negative x/y values etc

	values.comp.respawn_non_map_things_at_origin = true;	// comp_respawn
	values.comp.monsters_blocked_by_ledges = true;			// comp_ledgeblock
	values.comp.friendly_inherits_source_attribs = true;	// comp_friendlyspawn
	values.comp.voodoo_scrollers_move_slowly = false;		// comp_voodooscroller
	values.comp.mbf_reserved_flag_disables_flags = true;	// comp_reservedlineflag

	values.fix = GetAllFixed();

	values.sim.extended_saves = true;						// Generic save format for the new features
	values.sim.extended_demos = true;						// Decide on the fly if we support a demo or not
	values.sim.extended_map_formats = true;					// Different node types etc
	values.sim.animated_lump = true;						// Boom feature, replaces internal table with data
	values.sim.switches_lump = true;						// Boom feature, replaces internal table with data
	values.sim.allow_unknown_thing_types = true;			// Replace unknown things with warning objects
	values.sim.weapon_recoil = true;						// Boom "feature"
	values.sim.line_passthrough = true;						// Boom feature, don't stop using lines when one succeeds if linedef flag is set
	values.sim.hud_combined_keys = true;					// Boom feature, dependent on if resources exist in WAD
	values.sim.mobjs_slide_off_edge = true;					// Boom feature, trust me you'll miss it if it's not there
	values.sim.corpse_ignores_mobjs = true;					// Corpses with velocity still collided as if solid with other things
	values.sim.unlimited_scrollers = true;					// Vanilla limit
	values.sim.unlimited_platforms = true;					// Vanilla limit
	values.sim.unlimited_ceilings = true;					// Vanilla limit
	values.sim.generic_specials_handling = true;			// Rewritten specials, based on Boom standards but considered limit removing
	values.sim.separate_floor_ceiling_lights = true;		// Boom sector specials are allowed to work independently of each other
	values.sim.sector_movement_modifiers = true;			// Boom sectors can have variable friction, wind, currents
	values.sim.door_tagged_light = true;					// Dx with tags does the Boom lighting thing
	values.sim.boom_sector_targets = true;					// All sector targets consider their current sector
	values.sim.boom_line_specials = true;					// Comes with Boom fixes for specials by default
	values.sim.boom_sector_specials = true;					// Comes with Boom fixes for specials by default
	values.sim.boom_things = true;							// Pushers, pullers, TNT1
	values.sim.mbf_line_specials = true;					// Sky transfers, that's it
	values.sim.mbf_mobj_flags = true;						// Bouncy! Friendly! TOUCHY!
	values.sim.mbf_code_pointers = true;					// Dehacked additions
	values.sim.mbf_things = true;							// DOGS
	values.sim.mbf21_line_specials = true;					// New texture scrollers, new flags
	values.sim.mbf21_sector_specials = true;				// Insta-deaths
	values.sim.mbf21_thing_extensions = true;				// infighting, flags2, etc
	values.sim.mbf21_code_pointers = true;					// Dehacked additions
	values.sim.rnr24_line_specials = true;					// Floor/ceiling offsets, music changing, resetting exits, coloured lighting
	values.sim.rnr24_thing_extensions = true;				// New flags, nightmare respawn times
	values.sim.rnr24_code_pointers = true;					// Dehacked extensions (unlimited weapons and ammo types)
	values.sim.allow_skydefs = true;						// Generic skies
	values.sim.allow_sbardefs = true;						// Generic status bars

	if( gameconf ) UpdateFromGameconf( values.comp, gameconf );
	UpdateFromOptionsLump( values.comp );

	return values;
}

static void SetGame( gameflow_t& game )
{
	current_game = &game;
	current_episode = game.episodes[ 0 ];
	current_map = game.episodes[ 0 ]->first_map;
}

static GameVersion_t DetermineFromDemo( byte* demo )
{
	byte demoversion = *demo++;

	doombool boomdemo = demoversion == demo_boom_2_02
					|| demoversion == demo_mbf
					|| demoversion == demo_complevel11;

	if( boomdemo )
	{
		const byte boomsig[6] = { 0x1D, 'B', 'o', 'o', 'm', 0xE6 };
		const byte mbfsig[6] = { 0x1D, 'M', 'B', 'F', 0xE6, 0x00 };
		byte sig[6] = { demo[0], demo[1], demo[2], demo[3], demo[4], demo[5] };

		doombool isboom = demoversion == demo_boom_2_02 && memcmp( (void*)sig, (void*)boomsig, 6 ) == 0;
		doombool ismbf = memcmp( (void*)sig, (void*)mbfsig, 6 ) == 0;

		if( !isboom && !ismbf )
		{
			return exe_invalid;
		}
	}

	switch( demoversion )
	{
	case demo_doom_1_666:
		return exe_doom_1_666;
	case demo_doom_1_7:
		return exe_doom_1_7;
	case demo_doom_1_8:
		return exe_doom_1_8;
	case demo_doom_1_9:
	case demo_doom_1_91:
		return exe_doom_1_9;
	case demo_boom_2_02:
		return exe_boom_2_02;
	case demo_complevel11:
		return exe_mbf;
	case demo_mbf:
		return exe_mbf;
	default:
		break;
	}

	return exe_invalid;
}

static GameVersion_t DetermineFromBoomLumps()
{
	if( W_CheckNumForNameExcluding( "ANIMATED", wt_system ) >= 0
		|| W_CheckNumForNameExcluding( "SWITCHES", wt_system ) >= 0
		|| W_CheckNumForNameExcluding( "C_BEGIN", wt_system ) >= 0
		|| W_CheckNumForNameExcluding( "C_END", wt_system ) >= 0 )
	{
		return exe_boom_2_02;
	}

	return exe_invalid;
}

static GameVersion_t DetermineFromMBFLumps()
{
	if( W_CheckNumForNameExcluding( "OPTIONS", wt_system ) >= 0 )
	{
		return exe_mbf;
	}

	return exe_invalid;
}

static GameVersion_t DetermineFromSector( mapsector_t& sector )
{
	if( sector.special >= MBF21SectorVal_Min )
	{
		return exe_mbf21;
	}

	if( sector.special >= BoomSectorVal_Min )
	{
		return exe_boom_2_02;
	}

	return exe_limit_removing;
}

static GameVersion_t DetermineFromLinedef( maplinedef_t& line )
{
	if( !( line.flags & ML_VANILLAONLY ) )
	{
		if( line.flags & ( ML_MBF21_BLOCKLANDMONSTER | ML_MBF21_BLOCKPLAYERS ) )
		{
			return exe_mbf21;
		}

		if( line.flags & ML_BOOM_PASSTHROUGH )
		{
			return exe_boom_2_02;
		}
	}

	if( line.special >= DoomActions_Min && line.special < DoomActions_Max
		&& line.special != Unknown_078 && line.special != Unknown_085 )
	{
		return exe_limit_removing;
	}

	if( !comp.strict_boom_feature_matching
		&& line.special == Scroll_WallTextureRight_Always )
	{
		return exe_limit_removing;
	}

	if( line.special == Floor_ChangeTexture_NumericModel_SR_Player
		|| line.special == Scroll_WallTextureRight_Always
		|| ( line.special >= BoomActions_Min && line.special < BoomActions_Max ) )
	{
		return exe_boom_2_02;
	}

	if( line.special >= MBFActions_Min && line.special < MBFActions_Max )
	{
		return exe_complevel9;
	}

	if( line.special >= MBF21Actions_Min && line.special < MBF21Actions_Max )
	{
		return exe_mbf21;
	}

	if( line.special >= RNR24Actions_Min && line.special < RNR24Actions_Max )
	{
		return exe_rnr24;
	}

	if( line.special >= Generic_Min && line.special < Generic_Max )
	{
		return exe_boom_2_02;
	}

	return exe_invalid;
}

static GameVersion_t DetermineFromThing( mapthing_t& thing )
{
	uint32_t flags = ( thing.options & MTF_MBF_RUBBISHFLAGCHECK ) ? ( thing.options & 31 ) : thing.options;

	if( flags & ( MTF_MBF_FRIENDLY ) )
	{
		return exe_mbf;
	}

	if( comp.strict_boom_feature_matching
		&& ( flags & ( MTF_BOOM_NOT_IN_DEATHMATCH | MTF_BOOM_NOT_IN_COOP ) ) )
	{
		return exe_boom_2_02;
	}

	return exe_invalid;
}

static GameVersion_t DetermineFromMap( lumpindex_t mapindex )
{
	GameVersion_t version = exe_invalid;

	lumpindex_t sectorsindex = mapindex + ML_SECTORS;
	size_t sectorssize = W_LumpLength( sectorsindex );
	size_t numsectors = sectorssize / sizeof( mapsector_t );
	mapsector_t* sectors = (mapsector_t*)W_CacheLumpNum( sectorsindex, PU_CACHE );

	for( mapsector_t& sector : std::span( sectors, numsectors ) )
	{
		version = M_MAX( version, DetermineFromSector( sector ) );
	}

	lumpindex_t linesindex = mapindex + ML_LINEDEFS;
	size_t linessize = W_LumpLength( linesindex );
	size_t numlines = linessize / sizeof( maplinedef_t );
	maplinedef_t* lines = (maplinedef_t*)W_CacheLumpNum( linesindex, PU_CACHE );

	for( maplinedef_t& line : std::span( lines, numlines ) )
	{
		version = M_MAX( version, DetermineFromLinedef( line ) );
	}

	lumpindex_t thingsindex = mapindex + ML_THINGS;
	size_t thingssize = W_LumpLength( thingsindex );
	size_t numthings = thingssize / sizeof( mapthing_t );
	mapthing_t* things = (mapthing_t*)W_CacheLumpNum( thingsindex, PU_CACHE );

	for( mapthing_t& thing : std::span( things, numthings ) )
	{
		version = M_MAX( version, DetermineFromThing( thing ) );
	}

	return version;
}

static const std::map< std::string, GameVersion_t > complvlversions =
{
	{ "vanilla", exe_doom_1_9 },
	{ "boom", exe_complevel9 },
	{ "mbf", exe_mbf },
	{ "mbf21", exe_mbf21 },
};

static GameVersion_t DetermineFromComplvlLump( lumpindex_t lump )
{
	const char* complvl = (const char*)W_CacheLumpNum( lump, PU_CACHE );
	auto found = complvlversions.find( complvl );
	if( found != complvlversions.end() )
	{
		return found->second;
	}

	return exe_invalid;
}

#define VERSION_INCREASE( versionval, newversion ) \
versionval = M_MAX( versionval, newversion ); \
if( versionval >= exe_mbf21 ) return versionval

static GameVersion_t DetermineGameExecutable()
{
	GameVersion_t version = exe_limit_removing;

	VERSION_INCREASE( version, DEH_GetLoadedGameVersion() );

	lumpindex_t complvl = W_CheckNumForNameExcluding( "COMPLVL", wt_system );
	if( complvl >= 0 )
	{
		VERSION_INCREASE( version, DetermineFromComplvlLump( complvl ) );
	}

	constexpr const char* demos[] =
	{
		"DEMO1",
		"DEMO2",
		"DEMO3",
		"DEMO4"
	};

	for( const char* demoname : std::span( demos ) )
	{
		lumpindex_t demolump = W_CheckNumForName( demoname );
		if( demolump >= 0 )
		{
			VERSION_INCREASE( version, DetermineFromDemo( (byte*)W_CacheLumpNum( demolump, PU_CACHE ) ) );
		}
	}

	VERSION_INCREASE( version, DetermineFromBoomLumps() );
	VERSION_INCREASE( version, DetermineFromMBFLumps() );

	for( episodeinfo_t* episode : D_GetEpisodes() )
	{
		for( mapinfo_t* map : D_GetMapsFor( episode ) )
		{
			lumpindex_t maplump = W_CheckNumForName( AsDoomString( map->data_lump, episode->episode_num, map->map_num ).c_str() );
			if( maplump >= 0 )
			{
				VERSION_INCREASE( version, DetermineFromMap( maplump ) );
			}
		}
	}

	return version;
}

static void SetDefaultGameflow()
{
	extern gameflow_t doom_shareware;
	extern gameflow_t doom_registered;
	extern gameflow_t doom_registered_pre_v1_9;
	extern gameflow_t doom_ultimate;
	extern gameflow_t doom_2;
	extern gameflow_t doom_2_bfg;
	extern gameflow_t doom_tnt;
	extern gameflow_t doom_plutonia;
	extern gameflow_t doom_chex;
	extern gameflow_t doom_hacx;

	switch( gamemission )
	{
	case doom:
		switch( gamemode )
		{
		case registered:
			if( gameversion < exe_ultimate )
			{
				SetGame( doom_registered_pre_v1_9 );
			}
			else
			{
				SetGame( doom_registered );
			}
			break;
		case retail:
			SetGame( doom_ultimate );
			break;
		case shareware:
		default:
			SetGame( doom_shareware );
			break;
		}
		break;
	case doom2:
		if( gamevariant == bfgedition ) SetGame( doom_2_bfg );
		else SetGame( doom_2 );
		break;
	case pack_tnt:
		SetGame( doom_tnt );
		break;
	case pack_plut:
		SetGame( doom_plutonia );
		break;
	case pack_chex:
		SetGame( doom_chex );
		break;
	case pack_hacx:
		SetGame( doom_hacx );
		break;
	default:
		I_Error( "Unsupported game configuration" );
		break;
	}
}

 static void SetGamesimOptions( GameVersion_t version )
{
	simvalues_t values;

	switch( version )
	{
	case exe_doom_1_2:
	case exe_doom_1_666:
	case exe_doom_1_7:
	case exe_doom_1_8:
	case exe_doom_1_9:
	case exe_hacx:
	case exe_ultimate:
	case exe_final:
	case exe_final2:
	case exe_chex:
		I_TerminalPrintf( Log_Startup, " Applying vanilla compatibility\n" );
		values = GetVanillaValues( version, gamemode );
		break;

	case exe_limit_removing:
		I_TerminalPrintf( Log_Startup, " Applying limit-removing compatibility\n" );
		values = GetLimitRemovingValues( gamemode );
		break;

	case exe_limit_removing_fixed:
		I_TerminalPrintf( Log_Startup, " Applying limit-removing (plus fixes) compatibility\n" );
		values = GetLimitRemovingFixedValues( gamemode );
		break;

	case exe_boom_2_02:
		I_TerminalPrintf( Log_Startup, " Applying Boom compatibility\n" );
		values = GetBoomValues( gamemode );
		break;

	case exe_complevel9:
		I_TerminalPrintf( Log_Startup, " Applying -complevel 9 compatibility\n" );
		values = GetComplevel9Values( gamemode );
		break;

	case exe_mbf:
		I_TerminalPrintf( Log_Startup, " Applying MBF compatibility\n" );
		values = GetMBFValues( gamemode );
		break;

	case exe_mbf_dehextra:
		I_TerminalPrintf( Log_Startup, " Applying MBF + DEHEXTRA compatibility\n" );
		values = GetMBFValues( gamemode );
		break;

	case exe_mbf21:
		I_TerminalPrintf( Log_Startup, " Applying MBF21 compatibility\n" );
		values = GetMBF21Values( gamemode );
		break;

	case exe_mbf21_extended:
		I_TerminalPrintf( Log_Startup, " Applying MBF21 Extended compatibility\n" );
		values = GetMBF21Values( gamemode );
		break;

	case exe_rnr24:
		I_TerminalPrintf( Log_Startup, " Applying R&R24 compatibility\n" );
		values = GetRNR24Values( gamemode );
		break;

	default:
		I_TerminalPrintf( Log_Warning, " Indeterminable version, applying vanilla compatibility\n" );
		values = GetVanillaValues( version, gamemode );
		break;
	}

	comp = values.comp;
	fix = values.fix;
	sim = values.sim;
}

void D_RegisterGamesim()
{
	SetDefaultGameflow();

	D_GameflowCheckAndParseMapinfos();

	int32_t overridden = M_CheckParmWithArgs("-gameversion", 1);
	if( !overridden )
	{
		gameversion = DetermineGameExecutable();
	}
	else
	{
		if( gameversion < DEH_GetLoadedGameVersion() )
		{
			I_Error( "Dehacked loaded in more features than the current gameversion supports." );
		}
	}

	SetGamesimOptions( gameversion );
}
