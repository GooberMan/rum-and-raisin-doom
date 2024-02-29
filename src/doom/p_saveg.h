//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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
// DESCRIPTION:
//	Savegame I/O, archiving, persistence.
//


#ifndef __P_SAVEG__
#define __P_SAVEG__

#include <stdio.h>

typedef enum savegametype_e
{
	SaveGame_Invalid,
	SaveGame_Vanilla,
	SaveGame_LimitRemoving
} savegametype_t;

#define VERSIONSIZE 16

// maximum size of a savegame description

#define SAVESTRINGSIZE 24

// temporary filename to use while saving.

DOOM_C_API char *P_TempSaveGameFile(void);

// filename to use for a savegame slot

DOOM_C_API char *P_SaveGameFile(int slot);

// Savegame file header read/write functions

DOOM_C_API doombool P_ReadSaveGameHeader(void);
DOOM_C_API void P_WriteSaveGameHeader(char *description);

// Savegame end-of-file read/write functions

DOOM_C_API savegametype_t P_ReadSaveGameEOF(void);
DOOM_C_API void P_WriteSaveGameEOF( savegametype_t type );

DOOM_C_API savegametype_t P_ReadSaveGameType( void );

DOOM_C_API void P_ArchiveLimitRemovingData( void );
DOOM_C_API void P_UnArchiveLimitRemovingData( doombool gameflagsonly );

// Persistent storage/archiving.
// These are the load / save game routines.
DOOM_C_API void P_ArchivePlayers (void);
DOOM_C_API void P_UnArchivePlayers (void);
DOOM_C_API void P_ArchiveWorld (void);
DOOM_C_API void P_UnArchiveWorld (void);
DOOM_C_API void P_ArchiveThinkers (void);
DOOM_C_API void P_UnArchiveThinkers (void);
DOOM_C_API void P_ArchiveSpecials (void);
DOOM_C_API void P_UnArchiveSpecials (void);

DOOM_C_API extern FILE *save_stream;
DOOM_C_API extern doombool savegame_error;


#endif
