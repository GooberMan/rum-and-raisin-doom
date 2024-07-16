//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2020-2024 Ethan Watson
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
// DEHEXTRA and beyond sprites
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
#include "m_conv.h"

extern std::unordered_map< int32_t, const char* > spritenamemap;
extern std::vector< spriteinfo_t* > allsprites;
enum
{
	NumDoomSprites = 138,
	NumBoomSprites = 139,
	NumMBFSprites = 144,
	NumMBFExtraSprites = 244,
};

GameVersion_t VersionForSpriteNum( int32_t spritenum )
{
	return spritenum < -1 ? exe_rnr24 
		: spritenum < NumDoomSprites ? exe_doom_1_2
		: spritenum < NumBoomSprites ? exe_boom_2_02
		: spritenum < NumMBFSprites ? exe_mbf
		: spritenum < NumMBFExtraSprites ? exe_mbf_dehextra
		: exe_mbf21_extended;
}

void DEH_EnsureSpriteExists( deh_context_t* context, int32_t spritenum )
{
	if( spritenum == -1 )
	{
		DEH_Error(context, "Attempting to use sprite index -1" );
	}
	else
	{
		GameVersion_t version = VersionForSpriteNum( spritenum );
		DEH_IncreaseGameVersion( context, version );
		if( spritenamemap.find( spritenum ) == spritenamemap.end() )
		{
			spriteinfo_t* newsprite = Z_MallocAs( spriteinfo_t, PU_STATIC, nullptr );
			*newsprite = { spritenum, version, M_DuplicateStringToZone( "TNT1", PU_STATIC, nullptr ) };
			allsprites.push_back( newsprite );
			spritenamemap[ spritenum ] = newsprite->sprite;
		}
	}
}

static void *DEH_DSDSpritesStart( deh_context_t* context, char* line )
{
	char section[ 16 ] = {};

	if ( sscanf( line, "%9s", section ) == 0 || strncmp( "[SPRITES]", section, 9 ) )
	{
		DEH_Warning(context, "Parse error on SPRITES start");
	}

	return NULL;
}

static void DEH_DSDSpritesParseLine( deh_context_t *context, char* line, void* tag )
{
	char *spritenum, *value;

	if (!DEH_ParseAssignment(line, &spritenum, &value))
	{
		DEH_Warning(context, "Failed to parse sprite assignment");
		return;
	}

	size_t spritelen = strlen( value );
	if( spritelen != 4 )
	{
		DEH_Warning(context, "Sprite '%s' is invalid", value );
		return;
	}

	int32_t spriteindex = 0;
	if( IsNumber( spritenum ) )
	{
		spriteindex = atoi( spritenum );
	}
	else
	{
		using iterator_t = decltype( *spritenamemap.begin() );
		auto found = std::find_if( spritenamemap.begin(), spritenamemap.end(), [&spritenum]( iterator_t& test )
		{
			return strcasecmp( test.second, spritenum ) == 0;
		} );

		if( found == spritenamemap.end() )
		{
			DEH_Warning( context, "Sprite '%s' not previously defined", spritenum );
			return;
		}

		spriteindex = found->first;
	}

	if( spriteindex == -1 )
	{
		DEH_Error(context, "Invalid sprite number: -1" );
		return;
	}

	DEH_EnsureSpriteExists( context, spriteindex );

	spriteinfo_t* found = *std::find_if( allsprites.begin(), allsprites.end(), [spriteindex]( spriteinfo_t* val )
								{
									return val->spritenum == spriteindex;
								} );

	Z_Free( (void*)found->sprite );

	found->sprite = M_DuplicateStringToZone( value, PU_STATIC, nullptr );
	spritenamemap[ spriteindex ] = found->sprite;
}

static uint32_t DEH_SpritesFNV1aHash( int32_t version, uint32_t base )
{
	for( spriteinfo_t* sprite : allsprites )
	{
		if( version >= sprite->minimumversion )
		{
			base = fnv1a32( base, sprite->spritenum );
			base = fnv1a32( base, sprite->sprite );
		}
	}

	return base;
}

deh_section_t deh_section_dsdsprites =
{
	"[SPRITES]",
	NULL,
	DEH_DSDSpritesStart,
	DEH_DSDSpritesParseLine,
	NULL,
	NULL,
	DEH_SpritesFNV1aHash,
};

