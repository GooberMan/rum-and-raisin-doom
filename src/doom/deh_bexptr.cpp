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
// Parses BEX-formatted Action Pointer entries in dehacked files
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "info.h"

#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"

#include "m_container.h"

extern "C"
{
	// Doom actions
	void A_Light0( player_t *player, pspdef_t *psp );
	void A_WeaponReady( player_t *player, pspdef_t *psp );
	void A_Lower( player_t *player, pspdef_t *psp );
	void A_Raise( player_t *player, pspdef_t *psp );
	void A_Punch( player_t *player, pspdef_t *psp );
	void A_ReFire( player_t *player, pspdef_t *psp );
	void A_FirePistol( player_t *player, pspdef_t *psp );
	void A_Light1( player_t *player, pspdef_t *psp );
	void A_FireShotgun( player_t *player, pspdef_t *psp );
	void A_Light2( player_t *player, pspdef_t *psp );
	void A_FireShotgun2( player_t *player, pspdef_t *psp );
	void A_CheckReload( player_t *player, pspdef_t *psp );
	void A_OpenShotgun2( player_t *player, pspdef_t *psp );
	void A_LoadShotgun2( player_t *player, pspdef_t *psp );
	void A_CloseShotgun2( player_t *player, pspdef_t *psp );
	void A_FireCGun( player_t *player, pspdef_t *psp );
	void A_GunFlash( player_t *player, pspdef_t *psp );
	void A_FireMissile( player_t *player, pspdef_t *psp );
	void A_Saw( player_t *player, pspdef_t *psp );
	void A_FirePlasma( player_t *player, pspdef_t *psp );
	void A_BFGsound( player_t *player, pspdef_t *psp );
	void A_FireBFG( player_t *player, pspdef_t *psp );
	void A_BFGSpray( mobj_t* mobj );
	void A_Explode( mobj_t* mobj );
	void A_Pain( mobj_t* mobj );
	void A_PlayerScream( mobj_t* mobj );
	void A_Fall( mobj_t* mobj );
	void A_XScream( mobj_t* mobj );
	void A_Look( mobj_t* mobj );
	void A_Chase( mobj_t* mobj );
	void A_FaceTarget( mobj_t* mobj );
	void A_PosAttack( mobj_t* mobj );
	void A_Scream( mobj_t* mobj );
	void A_SPosAttack( mobj_t* mobj );
	void A_VileChase( mobj_t* mobj );
	void A_VileStart( mobj_t* mobj );
	void A_VileTarget( mobj_t* mobj );
	void A_VileAttack( mobj_t* mobj );
	void A_StartFire( mobj_t* mobj );
	void A_Fire( mobj_t* mobj );
	void A_FireCrackle( mobj_t* mobj );
	void A_Tracer( mobj_t* mobj );
	void A_SkelWhoosh( mobj_t* mobj );
	void A_SkelFist( mobj_t* mobj );
	void A_SkelMissile( mobj_t* mobj );
	void A_FatRaise( mobj_t* mobj );
	void A_FatAttack1( mobj_t* mobj );
	void A_FatAttack2( mobj_t* mobj );
	void A_FatAttack3( mobj_t* mobj );
	void A_BossDeath( mobj_t* mobj );
	void A_CPosAttack( mobj_t* mobj );
	void A_CPosRefire( mobj_t* mobj );
	void A_TroopAttack( mobj_t* mobj );
	void A_SargAttack( mobj_t* mobj );
	void A_HeadAttack( mobj_t* mobj );
	void A_BruisAttack( mobj_t* mobj );
	void A_SkullAttack( mobj_t* mobj );
	void A_Metal( mobj_t* mobj );
	void A_SpidRefire( mobj_t* mobj );
	void A_BabyMetal( mobj_t* mobj );
	void A_BspiAttack( mobj_t* mobj );
	void A_Hoof( mobj_t* mobj );
	void A_CyberAttack( mobj_t* mobj );
	void A_PainAttack( mobj_t* mobj );
	void A_PainDie( mobj_t* mobj );
	void A_KeenDie( mobj_t* mobj );
	void A_BrainPain( mobj_t* mobj );
	void A_BrainScream( mobj_t* mobj );
	void A_BrainDie( mobj_t* mobj );
	void A_BrainAwake( mobj_t* mobj );
	void A_BrainSpit( mobj_t* mobj );
	void A_SpawnSound( mobj_t* mobj );
	void A_SpawnFly( mobj_t* mobj );
	void A_BrainExplode( mobj_t* mobj );

	// MBF actions
	void A_Detonate( mobj_t* mobj );
	void A_Mushroom( mobj_t* mobj );
	void A_Spawn( mobj_t* mobj );
	void A_Turn( mobj_t* mobj );
	void A_Face( mobj_t* mobj );
	void A_Scratch( mobj_t* mobj );
	void A_PlaySound( mobj_t* mobj );
	void A_RandomJump( mobj_t* mobj );
	void A_LineEffect( mobj_t* mobj );
	void A_Die( mobj_t* mobj );

	// MBF21 Actions
	void A_SpawnObject( mobj_t* mobj );
	void A_MonsterProjectile( mobj_t* mobj );
	void A_MonsterBulletAttack( mobj_t* mobj );
	void A_MonsterMeleeAttack( mobj_t* mobj );
	void A_RadiusDamage( mobj_t* mobj );
	void A_NoiseAlert( mobj_t* mobj );
	void A_HealChase( mobj_t* mobj );
	void A_SeekTracer( mobj_t* mobj );
	void A_FindTracer( mobj_t* mobj );
	void A_ClearTracer( mobj_t* mobj );
	void A_JumpIfHealthBelow( mobj_t* mobj );
	void A_JumpIfTargetInSight( mobj_t* mobj );
	void A_JumpIfTargetCloser( mobj_t* mobj );
	void A_JumpIfTracerInSight( mobj_t* mobj );
	void A_JumpIfTracerCloser( mobj_t* mobj );
	void A_JumpIfFlagsSet( mobj_t* mobj );
	void A_AddFlags( mobj_t* mobj );
	void A_RemoveFlags( mobj_t* mobj );
	void A_WeaponProjectile( player_t* player, pspdef_t* psp );
	void A_WeaponBulletAttack( player_t* player, pspdef_t* psp );
	void A_WeaponMeleeAttack( player_t* player, pspdef_t* psp );
	void A_WeaponSound( player_t* player, pspdef_t* psp );
	void A_WeaponJump( player_t* player, pspdef_t* psp );
	void A_ConsumeAmmo( player_t* player, pspdef_t* psp );
	void A_CheckAmmo( player_t* player, pspdef_t* psp );
	void A_RefireTo( player_t* player, pspdef_t* psp );
	void A_GunFlashTo( player_t* player, pspdef_t* psp );
	void A_WeaponAlert( player_t* player, pspdef_t* psp );
}

struct funcmapping_t
{
	actionf_t		action;
	GameVersion_t	minimum_version;
};

#define FuncType( x ) { #x, { &A_ ## x, exe_doom_1_2 } }
#define MBFFuncType( x ) { #x, { &A_ ## x, exe_mbf } }
#define MBF21FuncType( x ) { #x, { &A_ ## x, exe_mbf21 } }

static std::map< std::string, funcmapping_t > PointerLookup =
{
	{ "NULL", { nullptr, exe_invalid } },

	// Doom actions
	FuncType( Light0 ),
	FuncType( WeaponReady ),
	FuncType( Lower ),
	FuncType( Raise ),
	FuncType( Punch ),
	FuncType( ReFire ),
	FuncType( FirePistol ),
	FuncType( Light1 ),
	FuncType( FireShotgun ),
	FuncType( Light2 ),
	FuncType( FireShotgun2 ),
	FuncType( CheckReload ),
	FuncType( OpenShotgun2 ),
	FuncType( LoadShotgun2 ),
	FuncType( CloseShotgun2 ),
	FuncType( FireCGun ),
	FuncType( GunFlash ),
	FuncType( FireMissile ),
	FuncType( Saw ),
	FuncType( FirePlasma ),
	FuncType( BFGsound ),
	FuncType( FireBFG ),
	FuncType( BFGSpray ),
	FuncType( Explode ),
	FuncType( Pain ),
	FuncType( PlayerScream ),
	FuncType( Fall ),
	FuncType( XScream ),
	FuncType( Look ),
	FuncType( Chase ),
	FuncType( FaceTarget ),
	FuncType( PosAttack ),
	FuncType( Scream ),
	FuncType( SPosAttack ),
	FuncType( VileChase ),
	FuncType( VileStart ),
	FuncType( VileTarget ),
	FuncType( VileAttack ),
	FuncType( StartFire ),
	FuncType( Fire ),
	FuncType( FireCrackle ),
	FuncType( Tracer ),
	FuncType( SkelWhoosh ),
	FuncType( SkelFist ),
	FuncType( SkelMissile ),
	FuncType( FatRaise ),
	FuncType( FatAttack1 ),
	FuncType( FatAttack2 ),
	FuncType( FatAttack3 ),
	FuncType( BossDeath ),
	FuncType( CPosAttack ),
	FuncType( CPosRefire ),
	FuncType( TroopAttack ),
	FuncType( SargAttack ),
	FuncType( HeadAttack ),
	FuncType( BruisAttack ),
	FuncType( SkullAttack ),
	FuncType( Metal ),
	FuncType( SpidRefire ),
	FuncType( BabyMetal ),
	FuncType( BspiAttack ),
	FuncType( Hoof ),
	FuncType( CyberAttack ),
	FuncType( PainAttack ),
	FuncType( PainDie ),
	FuncType( KeenDie ),
	FuncType( BrainPain ),
	FuncType( BrainScream ),
	FuncType( BrainDie ),
	FuncType( BrainAwake ),
	FuncType( BrainSpit ),
	FuncType( SpawnSound ),
	FuncType( SpawnFly ),
	FuncType( BrainExplode ),

	// MBF actions
	MBFFuncType( Detonate ),
	MBFFuncType( Mushroom ),
	MBFFuncType( Spawn ),
	MBFFuncType( Turn ),
	MBFFuncType( Face ),
	MBFFuncType( Scratch ),
	MBFFuncType( PlaySound ),
	MBFFuncType( RandomJump ),
	MBFFuncType( LineEffect ),
	MBFFuncType( Die ),

	// MBF21 actions
	MBF21FuncType( SpawnObject ),
	MBF21FuncType( MonsterProjectile ),
	MBF21FuncType( MonsterBulletAttack ),
	MBF21FuncType( MonsterMeleeAttack ),
	MBF21FuncType( RadiusDamage ),
	MBF21FuncType( NoiseAlert ),
	MBF21FuncType( HealChase ),
	MBF21FuncType( SeekTracer ),
	MBF21FuncType( FindTracer ),
	MBF21FuncType( ClearTracer ),
	MBF21FuncType( JumpIfHealthBelow ),
	MBF21FuncType( JumpIfTargetInSight ),
	MBF21FuncType( JumpIfTargetCloser ),
	MBF21FuncType( JumpIfTracerInSight ),
	MBF21FuncType( JumpIfTracerCloser ),
	MBF21FuncType( JumpIfFlagsSet ),
	MBF21FuncType( AddFlags ),
	MBF21FuncType( RemoveFlags ),
};

static void *DEH_BEXPtrStart( deh_context_t* context, char* line )
{
	char section[ 10 ];

	if ( sscanf( line, "%9s", section ) == 0 || strncmp( "[CODEPTR]", section, sizeof( section ) ) )
	{
		DEH_Warning(context, "Parse error on CODEPTR start");
	}

	return NULL;
}

static void DEH_BEXPtrParseLine( deh_context_t *context, char* line, void* tag )
{
	char* frame_lookup = nullptr;
	char* frame_func = nullptr;

	if( !DEH_ParseAssignment( line, &frame_lookup, &frame_func ) )
	{
		DEH_Warning( context, "'%s' is malformed" );
		return;
	}

	int32_t frame_number = 0;
	if( !sscanf( frame_lookup, "FRAME %d", &frame_number )
		&& !sscanf( frame_lookup, "Frame %d", &frame_number ) )
	{
		DEH_Warning( context, "'%s' is an invalid frame", frame_lookup );
		return;
	}

	auto found = PointerLookup.find( frame_func );
	if( found == PointerLookup.end()  )
	{
		DEH_Warning( context, "'%s' is an invalid function", frame_func );
		return;
	}

	DEH_IncreaseGameVersion( context, found->second.minimum_version );

	states[ frame_number ].action = found->second.action;
}

DOOM_C_API deh_section_t deh_section_bexptr =
{
	"[CODEPTR]",
	NULL,
	DEH_BEXPtrStart,
	DEH_BEXPtrParseLine,
	NULL,
	NULL
};

