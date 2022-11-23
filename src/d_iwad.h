//
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
//     Find IWAD and initialize according to IWAD type.
//


#ifndef __D_IWAD__
#define __D_IWAD__

#include "doomtype.h"

#include "d_mode.h"

#define IWAD_MASK_DOOM    ((1 << doom)           \
                         | (1 << doom2)          \
                         | (1 << pack_tnt)       \
                         | (1 << pack_plut)      \
                         | (1 << pack_chex)      \
                         | (1 << pack_hacx))
#define IWAD_MASK_HERETIC (1 << heretic)
#define IWAD_MASK_HEXEN   (1 << hexen)
#define IWAD_MASK_STRIFE  (1 << strife)

DOOM_C_API typedef struct
{
    const char *name;
    GameMission_t mission;
    GameMode_t mode;
    const char *description;
} iwad_t;

DOOM_C_API doombool D_IsIWADName(const char *name);
DOOM_C_API char *D_FindWADByName(const char *filename);
DOOM_C_API char *D_TryFindWADByName(const char *filename);
DOOM_C_API char *D_FindIWAD(int mask, GameMission_t *mission);
DOOM_C_API const iwad_t **D_FindAllIWADs(int mask);
DOOM_C_API const char *D_SaveGameIWADName(GameMission_t gamemission);
DOOM_C_API const char *D_SuggestIWADName(GameMission_t mission, GameMode_t mode);
DOOM_C_API const char *D_SuggestGameName(GameMission_t mission, GameMode_t mode);

#if defined( __cplusplus )

#include "m_container.h"

std::span< const char* > D_GetIWADPaths();

#endif // defined( __cplusplus )

#endif

