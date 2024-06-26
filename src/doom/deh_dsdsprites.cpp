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
		DEH_IncreaseGameVersion( context, VersionForSpriteNum( spritenum ) );
		spritenamemap[ spritenum ] = "TNT1";
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
		DEH_Error(context, "Invalid sound number: -1" );
		return;
	}

	DEH_EnsureSpriteExists( context, spriteindex );

	char* newstring = (char*)Z_MallocZero( spritelen + 1, PU_STATIC, nullptr );
	memcpy( newstring, value, spritelen );

	spritenamemap[ spriteindex ] = newstring;
}

deh_section_t deh_section_dsdsprites =
{
	"[SPRITES]",
	NULL,
	DEH_DSDSpritesStart,
	DEH_DSDSpritesParseLine,
	NULL,
	NULL
};

