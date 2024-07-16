//
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
//
// Parses "Frame" sections in dehacked files
//

#include <stdio.h>
#include <stdlib.h>

#include "doomtype.h"
#include "d_items.h"
#include "info.h"

#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
#include "deh_mapping.h"

#include "m_container.h"
#include <functional>

DEH_BEGIN_MAPPING(state_mapping, state_t)
  DEH_MAPPING("Sprite number",    sprite)
  DEH_MAPPING("Sprite subnumber", frame)
  DEH_MAPPING("Duration",         tics)
  DEH_MAPPING("Next frame",       nextstate)
  DEH_MAPPING("Unknown 1",        misc1)
  DEH_MAPPING("Unknown 2",        misc2)
  MBF21_MAPPING("MBF21 Bits",     mbf21flags)
  MBF21_MAPPING("Args1",          arg1._int)
  MBF21_MAPPING("Args2",          arg2._int)
  MBF21_MAPPING("Args3",          arg3._int)
  MBF21_MAPPING("Args4",          arg4._int)
  MBF21_MAPPING("Args5",          arg5._int)
  MBF21_MAPPING("Args6",          arg6._int)
  MBF21_MAPPING("Args7",          arg7._int)
  MBF21_MAPPING("Args8",          arg8._int)
  RNR_MAPPING("Tranmap",          tranmaplump)
  DEH_UNSUPPORTED_MAPPING("Codep frame")
DEH_END_MAPPING

DOOM_C_API void DEH_BexHandleFrameBitsMBF21( deh_context_t* context, const char* value, state_t* state );

// Thing dependencies
DOOM_C_API void A_Spawn( mobj_t* mobj );
DOOM_C_API void A_SpawnObject( mobj_t* mobj );
DOOM_C_API void A_MonsterProjectile( mobj_t* mobj );
DOOM_C_API void A_WeaponProjectile( player_t* player, pspdef_t* psp );

// State dependencies
DOOM_C_API void A_RandomJump( mobj_t* mobj );
DOOM_C_API void A_HealChase( mobj_t* mobj );
DOOM_C_API void A_JumpIfHealthBelow( mobj_t* mobj );
DOOM_C_API void A_JumpIfTargetInSight( mobj_t* mobj );
DOOM_C_API void A_JumpIfTargetCloser( mobj_t* mobj );
DOOM_C_API void A_JumpIfTracerInSight( mobj_t* mobj );
DOOM_C_API void A_JumpIfTracerCloser( mobj_t* mobj );
DOOM_C_API void A_JumpIfFlagsSet( mobj_t* mobj );
DOOM_C_API void A_WeaponJump( player_t* player, pspdef_t* psp );
DOOM_C_API void A_CheckAmmo( player_t* player, pspdef_t* psp );
DOOM_C_API void A_RefireTo( player_t* player, pspdef_t* psp );
DOOM_C_API void A_GunFlashTo( player_t* player, pspdef_t* psp );

// Sound dependencies
DOOM_C_API void A_MonsterMeleeAttack( mobj_t* mobj );
DOOM_C_API void A_HealChase( mobj_t* mobj );
DOOM_C_API void A_WeaponMeleeAttack( player_t* player, pspdef_t* psp );
DOOM_C_API void A_WeaponSound( player_t* player, pspdef_t* psp );

using resolvefunc_t = std::function< void( deh_context_t*, state_t* ) >;
using resolvemap_t = std::map< actionf_v, resolvefunc_t >;

mobjinfo_t* DEH_GetThing( deh_context_t* context, int32_t thing_number );
state_t* DEH_GetState( deh_context_t* context, int32_t frame_number );
sfxinfo_t* DEH_GetSound(deh_context_t* context, int32_t sound_number );
void DEH_EnsureSpriteExists( deh_context_t* context, int32_t spritenum );
const char* DEH_ResolveNameForAction( actionf_v func );

static const resolvemap_t thingresolvers =
{
	{ (actionf_v)&A_Spawn,					[]( deh_context_t* context, state_t* state ) { DEH_GetThing( context, state->misc1._int - 1 ); }	},
	{ (actionf_v)&A_SpawnObject,			[]( deh_context_t* context, state_t* state ) { DEH_GetThing( context, state->arg1._int - 1 ); }		},
	{ (actionf_v)&A_MonsterProjectile,		[]( deh_context_t* context, state_t* state ) { DEH_GetThing( context, state->arg1._int - 1 ); }		},
	{ (actionf_v)&A_WeaponProjectile,		[]( deh_context_t* context, state_t* state ) { DEH_GetThing( context, state->arg1._int - 1 ); }		},
};

static const resolvemap_t stateresolvers =
{
	{ (actionf_v)&A_RandomJump,				[]( deh_context_t* context, state_t* state ) { DEH_GetState( context, state->arg1._int ); }			},
	{ (actionf_v)&A_HealChase,				[]( deh_context_t* context, state_t* state ) { DEH_GetState( context, state->arg1._int ); }			},
	{ (actionf_v)&A_JumpIfHealthBelow,		[]( deh_context_t* context, state_t* state ) { DEH_GetState( context, state->arg1._int ); }			},
	{ (actionf_v)&A_JumpIfTargetInSight,	[]( deh_context_t* context, state_t* state ) { DEH_GetState( context, state->arg1._int ); }			},
	{ (actionf_v)&A_JumpIfTargetCloser,		[]( deh_context_t* context, state_t* state ) { DEH_GetState( context, state->arg1._int ); }			},
	{ (actionf_v)&A_JumpIfTracerInSight,	[]( deh_context_t* context, state_t* state ) { DEH_GetState( context, state->arg1._int ); }			},
	{ (actionf_v)&A_JumpIfTracerCloser,		[]( deh_context_t* context, state_t* state ) { DEH_GetState( context, state->arg1._int ); }			},
	{ (actionf_v)&A_JumpIfFlagsSet,			[]( deh_context_t* context, state_t* state ) { DEH_GetState( context, state->arg1._int ); }			},
	{ (actionf_v)&A_WeaponJump,				[]( deh_context_t* context, state_t* state ) { DEH_GetState( context, state->arg1._int ); }			},
	{ (actionf_v)&A_CheckAmmo,				[]( deh_context_t* context, state_t* state ) { DEH_GetState( context, state->arg1._int ); }			},
	{ (actionf_v)&A_RefireTo,				[]( deh_context_t* context, state_t* state ) { DEH_GetState( context, state->arg1._int ); }			},
	{ (actionf_v)&A_GunFlashTo,				[]( deh_context_t* context, state_t* state ) { DEH_GetState( context, state->arg1._int ); }			},
};

static const resolvemap_t soundresolvers =
{
	{ (actionf_v)&A_MonsterMeleeAttack,		[]( deh_context_t* context, state_t* state ) { DEH_GetSound( context, state->arg3._int ); }			},
	{ (actionf_v)&A_HealChase,				[]( deh_context_t* context, state_t* state ) { DEH_GetSound( context, state->arg2._int ); }			},
	{ (actionf_v)&A_WeaponMeleeAttack,		[]( deh_context_t* context, state_t* state ) { DEH_GetSound( context, state->arg4._int ); }			},
	{ (actionf_v)&A_WeaponSound,			[]( deh_context_t* context, state_t* state ) { DEH_GetSound( context, state->arg1._int ); }			},
};

extern std::unordered_map< int32_t, state_t* > statemap;
extern std::vector< state_t* > allstates;

state_t* DEH_GetState( deh_context_t* context, int32_t frame_number )
{
	if( frame_number == -1 )
	{
		DEH_Error(context, "Invalid frame number: -1" );
		return nullptr;
	}

	GameVersion_t version =  frame_number < -1 ? exe_rnr24
							: frame_number < ( NUMSTATES_VANILLA - 1 ) ? exe_doom_1_2
							: frame_number < NUMSTATES_VANILLA ? exe_limit_removing
							: frame_number < NUMSTATES_BOOM ? exe_boom_2_02
							: frame_number < NUMSTATES_MBF ? exe_mbf
							: frame_number < NUMSTATES_PRBOOMPLUS ? exe_mbf_dehextra
							: frame_number >= S_MINDEHACKED && frame_number < S_MAXDEHACKED ? exe_mbf_dehextra
							: exe_mbf21_extended;
	DEH_IncreaseGameVersion( context, version );

	auto foundstate = statemap.find( frame_number );
	if( foundstate == statemap.end() )
	{
		state_t* newstate = Z_MallocAs( state_t, PU_STATIC, nullptr );
		*newstate =
		{
			frame_number,				// statenum
			version,					// minimumersion
			SPR_TNT1,					// sprite
			0,							// frame
			-1,							// tics
			nullptr,					// action
			(statenum_t)frame_number,	// nextstate
		};
		allstates.push_back( newstate );
		statemap[ frame_number ] = newstate;
		return newstate;
	}
	else
	{
		return foundstate->second;
	}
}

static void *DEH_FrameStart(deh_context_t *context, char *line)
{
    int frame_number = 0;
    
    if (sscanf(line, "Frame %i", &frame_number) != 1)
    {
        DEH_Warning(context, "Parse error on section start");
        return NULL;
    }
    
	return DEH_GetState( context, frame_number );
}

// Simulate a frame overflow: Doom has 967 frames in the states[] array, but
// DOS dehacked internally only allocates memory for 966.  As a result, 
// attempts to set frame 966 (the last frame) will overflow the dehacked
// array and overwrite the weaponinfo[] array instead.
//
// This is noticable in Batman Doom where it is impossible to switch weapons
// away from the fist once selected.

//static void DEH_FrameOverflow(deh_context_t *context, char *varname, int value)
//{
//    if (!strcasecmp(varname, "Duration"))
//    {
//        weaponinfo[0].ammo = value;
//    }
//    else if (!strcasecmp(varname, "Codep frame")) 
//    {
//        weaponinfo[0].upstate = value;
//    }
//    else if (!strcasecmp(varname, "Next frame")) 
//    {
//        weaponinfo[0].downstate = value;
//    }
//    else if (!strcasecmp(varname, "Unknown 1"))
//    {
//        weaponinfo[0].readystate = value;
//    }
//    else if (!strcasecmp(varname, "Unknown 2"))
//    {
//        weaponinfo[0].atkstate = value;
//    }
//    else
//    {
//        DEH_Error(context, "Unable to simulate frame overflow: field '%s'",
//                  varname);
//    }
//}

static void DEH_FrameParseLine(deh_context_t *context, char *line, void *tag)
{
    state_t *state;
    char *variable_name, *value;
    int ivalue;
    
    if (tag == NULL)
       return;

    state = (state_t *) tag;

    // Parse the assignment

    if (!DEH_ParseAssignment(line, &variable_name, &value))
    {
        // Failed to parse

        DEH_Warning(context, "Failed to parse assignment");
        return;
    }
    
	if( strcmp( variable_name, "Tranmap" ) == 0 )
	{
		DEH_IncreaseGameVersion( context, exe_rnr24 );
		state->tranmaplump = M_DuplicateStringToZone( value, PU_STATIC, nullptr );
		M_ForceUppercase( (char*)state->tranmaplump );
	}
	else
	{
		// all other values are integers

		ivalue = atoi(value);
    
		// TODO: Work out how to retain overflow behavior
		//if (state == &states[NUMSTATES_VANILLA - 1])
		//{
		//    DEH_FrameOverflow(context, variable_name, ivalue);
		//}
		//else
		if( strcmp( variable_name, "MBF21 Bits" ) == 0
			&& !( isdigit( value[ 0 ] ) 
				|| ( value[ 0 ] == '-' && isdigit( value[ 1 ] ) ) )
			)
		{
			DEH_BexHandleFrameBitsMBF21( context, value, state );
		}
		else if( strcmp( variable_name, "Next frame" ) == 0 )
		{
			[[maybe_unused]] state_t* newstate = DEH_GetState( context, ivalue );
			DEH_SetMapping(context, &state_mapping, state, variable_name, ivalue);
		}
		{
			// set the appropriate field
			DEH_SetMapping(context, &state_mapping, state, variable_name, ivalue);
		}
	}
}

static void HandleResolver( deh_context_t* context, state_t* state, const resolvemap_t& map )
{
	auto found = map.find( state->action.Value() );
	if( found != map.end() )
	{
		found->second( context, state );
	}
}

void DEH_ResolveAllForFrame( deh_context_t* context, state_t* state )
{
	HandleResolver( context, state, thingresolvers );
	HandleResolver( context, state, stateresolvers );
	HandleResolver( context, state, soundresolvers );
	DEH_EnsureSpriteExists( context, state->sprite );

}

static void DEH_FrameSHA1Sum(sha1_context_t *context)
{
    int i;

    for (i=0; i < NUMSTATES; ++i)
    {
        DEH_StructSHA1Sum(context, &state_mapping, (void*)&states[i]);
    }
}

template< typename fnv >
static typename fnv::value_type DEH_FrameFNV1aHash( int32_t version, typename fnv::value_type base )
{
	for( state_t* state : allstates )
	{
		if( version >= state->minimumversion )
		{
			base = fnv::calc( base, state->statenum );
			base = fnv::calc( base, DEH_ResolveNameForAction( state->action.Value() ) );
			base = fnv::calc( base, state->sprite );
			base = fnv::calc( base, state->frame );
			base = fnv::calc( base, state->tics );
			base = fnv::calc( base, state->nextstate );
			base = fnv::calc( base, state->misc1._int );
			base = fnv::calc( base, state->misc2._int );

			if( version >= exe_mbf21 )
			{
				base = fnv::calc( base, state->mbf21flags );
				base = fnv::calc( base, state->arg1._int );
				base = fnv::calc( base, state->arg2._int );
				base = fnv::calc( base, state->arg3._int );
				base = fnv::calc( base, state->arg4._int );
				base = fnv::calc( base, state->arg5._int );
				base = fnv::calc( base, state->arg6._int );
				base = fnv::calc( base, state->arg7._int );
				base = fnv::calc( base, state->arg8._int );
			}

			if( version >= exe_rnr24 )
			{
				base = fnv::calc( base, state->tranmaplump ? state->tranmaplump : "null" );
			}
		}
	}

	return base;
}

deh_section_t deh_section_frame =
{
    "Frame",
    NULL,
    DEH_FrameStart,
    DEH_FrameParseLine,
    (deh_section_end_t)DEH_ResolveAllForFrame,
    DEH_FrameSHA1Sum,
	DEH_FrameFNV1aHash< fnv1a< uint32_t > >,
	DEH_FrameFNV1aHash< fnv1a< uint64_t > >,
};

