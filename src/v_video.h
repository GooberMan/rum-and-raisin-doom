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
//	Gamma correction LUT.
//	Functions to draw patches (by post) directly to screen.
//	Functions to blit a block to the screen.
//


#ifndef __V_VIDEO__
#define __V_VIDEO__

#include "doomtype.h"

// Needed because we are refering to patches.
#include "v_patch.h"
#include "i_video.h"

//
// VIDEO
//

#define CENTERY			(SCREENHEIGHT/2)

#if defined( __cplusplus )
#define DEFAULT(x) = x
#else
#define DEFAULT(x)
#endif


DOOM_C_API extern int dirtybox[4];

DOOM_C_API extern byte *tinttable;

// haleyjd 08/28/10: implemented for Strife support
// haleyjd 08/28/10: Patch clipping callback, implemented to support Choco
// Strife.
DOOM_C_API typedef doombool (*vpatchclipfunc_t)(patch_t *, int, int);
DOOM_C_API void V_SetPatchClipCallback(vpatchclipfunc_t func);

// Allocates buffer screens, call before R_Init.
DOOM_C_API void V_Init (void);

// Draw a block from the specified source screen to the screen.

DOOM_C_API void V_CopyRect(int srcx, int srcy, vbuffer_t *source,
                int width, int height,
                int destx, int desty);

DOOM_C_API void V_TransposeBuffer( vbuffer_t* source, vbuffer_t* output, int outputmemzone );
DOOM_C_API void V_TransposeFlat( const char* flat_name, vbuffer_t* output, int outputmemzone );

DOOM_C_API void V_TileBuffer( vbuffer_t* source_buffer, int32_t x, int32_t y, int32_t width, int32_t height );
DOOM_C_API void V_FillBorder( vbuffer_t* source_buffer, int32_t miny, int32_t maxy );

DOOM_C_API void V_DrawPatch(int x, int y, patch_t *patch, byte* tranmap DEFAULT(NULL), byte* translation DEFAULT(NULL) );
DOOM_C_API void V_DrawPatchClipped(int x, int y, patch_t *patch, int clippedx, int clippedy, int clippedwidth, int clippedheight, byte* tranmap DEFAULT(NULL), byte* translation DEFAULT(NULL));
DOOM_C_API void V_DrawPatchFlipped(int x, int y, patch_t *patch, byte* tranmap DEFAULT(NULL), byte* translation DEFAULT(NULL) );
DOOM_C_API void V_DrawTLPatch(int x, int y, patch_t *patch);
DOOM_C_API void V_DrawAltTLPatch(int x, int y, patch_t * patch);
DOOM_C_API void V_DrawShadowedPatch(int x, int y, patch_t *patch);
DOOM_C_API void V_DrawXlaPatch(int x, int y, patch_t * patch);     // villsa [STRIFE]
DOOM_C_API void V_DrawPatchDirect(int x, int y, patch_t *patch);

DOOM_C_API void V_EraseRegion(int x, int y, int width, int height);

// Draw a linear block of pixels into the view buffer.

DOOM_C_API void V_DrawBlock(int x, int y, int width, int height, pixel_t *src);

DOOM_C_API void V_MarkRect(int x, int y, int width, int height);

DOOM_C_API void V_DrawFilledBox(int x, int y, int w, int h, int c);
DOOM_C_API void V_DrawHorizLine(int x, int y, int w, int c);
DOOM_C_API void V_DrawVertLine(int x, int y, int h, int c);
DOOM_C_API void V_DrawBox(int x, int y, int w, int h, int c);

// Draw a raw screen lump

DOOM_C_API void V_DrawRawScreen(pixel_t *raw);

// Temporarily switch to using a different buffer to draw graphics, etc.

DOOM_C_API void V_UseBuffer(vbuffer_t *buffer);

// Return to using the normal screen buffer to draw graphics.

DOOM_C_API void V_RestoreBuffer(void);

// Save a screenshot of the current screen to a file, named in the 
// format described in the string passed to the function, eg.
// "DOOM%02i.pcx"

DOOM_C_API void V_ScreenShot(const char *format);

// Load the lookup table for translucency calculations from the TINTTAB
// lump.

DOOM_C_API void V_LoadTintTable(void);

// villsa [STRIFE]
// Load the lookup table for translucency calculations from the XLATAB
// lump.

DOOM_C_API void V_LoadXlaTable(void);

DOOM_C_API void V_DrawMouseSpeedBox(int speed);

#endif

