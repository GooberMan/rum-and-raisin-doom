//
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

#if !defined( __D_GAMECONF_H__ )
#define __D_GAMECONF_H__

#include "doomtype.h"
#include "d_mode.h"
#include "m_container.h"

DOOM_C_API typedef struct gameconf_s
{
	const char*		title;
	const char*		description;
	const char*		version;
	const char*		iwad;
	const char**	pwads;
	size_t			pwadscount;
	const char**	dehfiles;
	size_t			dehfilescount;
	GameVersion_t	executable;
	GameMission_t	mission;
	const char*		options;

#if defined( __cplusplus )
	constexpr auto PWADs()			{ return std::span( pwads, pwadscount ); }
	constexpr auto DEHFiles()		{ return std::span( dehfiles, dehfilescount ); }
#endif //defined( __cplusplus )
} gameconf_t;

DOOM_C_API gameconf_t* D_BuildGameConf();
DOOM_C_API void D_DestroyGameConf( gameconf_t* conf );

DOOM_C_API extern gameconf_t* gameconf;

#endif // !defined( __D_GAMECONF_H__ )
