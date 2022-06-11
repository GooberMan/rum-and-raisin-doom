//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2020 Ethan Watson
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
//	Rendering of moving objects, sprites.
//


#ifndef __R_THINGS__
#define __R_THINGS__

// For vertclip_t
// Remove when you've made a render context
#include "r_defs.h"

// Constant arrays used for psprite clipping
//  and initializing clipping.
// Does not need to be in a threaded context
extern vertclip_t	negonearray[ MAXSCREENWIDTH ];
extern vertclip_t	screenheightarray[ MAXSCREENWIDTH ];

extern fixed_t		pspritescale;
extern fixed_t		pspriteiscale;
// End global constants

void R_DrawMaskedColumn( spritecontext_t* spritecontext, colcontext_t* context, column_t* column );

void R_SortVisSprites( spritecontext_t* spritecontext );

void R_AddSprites( spritecontext_t* spritecontext, sector_t* sec );
void R_InitSprites(const char **namelist);
void R_ClearSprites ( spritecontext_t* spritecontext );
void R_DrawMasked ( vbuffer_t* dest, spritecontext_t* spritecontext, bspcontext_t* bspcontext );

#endif
