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

// This is fairly evil, and won't survive the transition to C++
extern "C"
{
	void A_Light0();
	void A_WeaponReady();
	void A_Lower();
	void A_Raise();
	void A_Punch();
	void A_ReFire();
	void A_FirePistol();
	void A_Light1();
	void A_FireShotgun();
	void A_Light2();
	void A_FireShotgun2();
	void A_CheckReload();
	void A_OpenShotgun2();
	void A_LoadShotgun2();
	void A_CloseShotgun2();
	void A_FireCGun();
	void A_GunFlash();
	void A_FireMissile();
	void A_Saw();
	void A_FirePlasma();
	void A_BFGsound();
	void A_FireBFG();
	void A_BFGSpray();
	void A_Explode();
	void A_Pain();
	void A_PlayerScream();
	void A_Fall();
	void A_XScream();
	void A_Look();
	void A_Chase();
	void A_FaceTarget();
	void A_PosAttack();
	void A_Scream();
	void A_SPosAttack();
	void A_VileChase();
	void A_VileStart();
	void A_VileTarget();
	void A_VileAttack();
	void A_StartFire();
	void A_Fire();
	void A_FireCrackle();
	void A_Tracer();
	void A_SkelWhoosh();
	void A_SkelFist();
	void A_SkelMissile();
	void A_FatRaise();
	void A_FatAttack1();
	void A_FatAttack2();
	void A_FatAttack3();
	void A_BossDeath();
	void A_CPosAttack();
	void A_CPosRefire();
	void A_TroopAttack();
	void A_SargAttack();
	void A_HeadAttack();
	void A_BruisAttack();
	void A_SkullAttack();
	void A_Metal();
	void A_SpidRefire();
	void A_BabyMetal();
	void A_BspiAttack();
	void A_Hoof();
	void A_CyberAttack();
	void A_PainAttack();
	void A_PainDie();
	void A_KeenDie();
	void A_BrainPain();
	void A_BrainScream();
	void A_BrainDie();
	void A_BrainAwake();
	void A_BrainSpit();
	void A_SpawnSound();
	void A_SpawnFly();
	void A_BrainExplode();
}

#define FuncType( x ) { #x, { &A_ ## x } }

static std::map< DoomString, actionf_t > PointerLookup =
{
	{ "NULL", { nullptr } },
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
	if( !sscanf( frame_lookup, "FRAME %d", &frame_number ) )
	{
		DEH_Warning( context, "'%s' is an invalid frame", frame_lookup );
		return;
	}

	auto found = PointerLookup.find( frame_func );
	if( found == PointerLookup.end() )
	{
		DEH_Warning( context, "'%s' is an invalid function", frame_func );
		return;
	}

	states[ frame_number ].action = found->second;
}

DOOM_C_API deh_section_t deh_section_bexptr =
{
	"[CODEPTR]",
	NULL,
	DEH_BEXPtrStart,
	DEH_BEXPtrParseLine,
	NULL,
	NULL,
	&deh_allow_bex,
};

