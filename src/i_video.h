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
//	System specific interface stuff.
//


#ifndef __I_VIDEO__
#define __I_VIDEO__

#include "doomtype.h"
#include "i_vbuffer.h"

DOOM_C_API typedef enum vsync_e
{
	VSync_Off,
	VSync_Native,
	VSync_Adaptive,
	VSync_35Hz,
	VSync_36Hz,
	VSync_40Hz,
	VSync_45Hz,
	VSync_50Hz,
	VSync_60Hz,
	VSync_70Hz,
	VSync_72Hz,
	VSync_75Hz,
	VSync_90Hz,
	VSync_100Hz,
	VSync_120Hz,
	VSync_140Hz,
	VSync_144Hz,
	VSync_150Hz,
	VSync_180Hz,
	VSync_200Hz,
	VSync_240Hz,
	VSync_280Hz,
	VSync_288Hz,
	VSync_300Hz,
	VSync_360Hz,

	VSync_Max,
} vsync_t;

DOOM_C_API typedef enum renderdimensions_e
{
	RD_None,
	RD_MatchWindow,
	RD_Independent,
} renderdimensions_t;

// Screen width and height.
// Every compiler will do literal calculations at compile time these days, so let's be always correct about it.
// Multiply to big values so that integer divides don't lose information. Convert SCREENHEIGHT to be 16:10 correct,
// and SCREENHEIGHT_4_3 to be 4:3 correct.

#define VANILLA_SCREENWIDTH 320
#define VANILLA_SCREENHEIGHT 200

#define V_VIRTUALWIDTH 320
#define V_VIRTUALHEIGHT 200

// TODO: We need to redo this to be pure integer math. Fixed/float results in inaccuracies at different resolutions :-(
#define V_WIDTHSTEP ( ( V_VIRTUALWIDTH << FRACBITS ) / aspect_adjusted_render_width )
#define V_WIDTHMULTIPLIER ( ( aspect_adjusted_render_width << FRACBITS ) / V_VIRTUALWIDTH )

#define V_HEIGHTSTEP ( ( V_VIRTUALHEIGHT << FRACBITS ) / render_height )
#define V_HEIGHTMULTIPLIER ( ( render_height << FRACBITS ) / V_VIRTUALHEIGHT )

DOOM_C_API typedef boolean (*grabmouse_callback_t)(void);

// Called by D_DoomMain,
// determines the hardware configuration
// and sets up the video mode
DOOM_C_API void I_InitGraphics( void );
DOOM_C_API void I_InitBuffers( int32_t numbuffers );
DOOM_C_API void I_UpdateMouseGrab( void );

DOOM_C_API vbuffer_t* I_GetRenderBuffer( int32_t index );
DOOM_C_API vbuffer_t* I_GetCurrentRenderBuffer( void );
DOOM_C_API void I_SetRenderBufferValidColumns( int32_t index, int32_t begin, int32_t end );
DOOM_C_API void I_SetNumBuffers( int32_t count );

DOOM_C_API SDL_Window* I_GetWindow( void );
DOOM_C_API SDL_Renderer* I_GetRenderer( void );
DOOM_C_API int32_t I_GetRefreshRate( void );

DOOM_C_API void I_GraphicsCheckCommandLine(void);

DOOM_C_API void I_ShutdownGraphics(void);

DOOM_C_API void I_ToggleFullScreen(void);
DOOM_C_API void I_SetWindowDimensions( int32_t w, int32_t h );
DOOM_C_API void I_SetRenderDimensions( int32_t w, int32_t h, int32_t s );

DOOM_C_API boolean I_VideoSetVSync( vsync_t vsyncval );
DOOM_C_API vsync_t I_VideoGetVSync( void );
DOOM_C_API boolean I_VideoSupportsVSync( vsync_t vsyncval );

// Takes full 8 bit values.
DOOM_C_API void I_SetPalette (byte* palette);
DOOM_C_API int I_GetPaletteIndex(int r, int g, int b);

DOOM_C_API void I_FinishUpdate( vbuffer_t* activebuffer );

DOOM_C_API void I_ReadScreen (pixel_t* scr);

DOOM_C_API void I_SetWindowTitle(const char *title);

DOOM_C_API void I_CheckIsScreensaver(void);
DOOM_C_API void I_SetGrabMouseCallback(grabmouse_callback_t func);

DOOM_C_API void I_DisplayFPSDots(boolean dots_on);
DOOM_C_API void I_BindVideoVariables(void);

DOOM_C_API void I_InitWindowTitle(void);
DOOM_C_API void I_InitWindowIcon(void);

// Called before processing any tics in a frame (just after displaying a frame).
// Time consuming syncronous operations are performed here (joystick reading).

DOOM_C_API void I_StartFrame (void);

// Called before processing each tic in a frame.
// Quick syncronous operations are performed here.

DOOM_C_API void I_StartTic (void);

// Enable the loading disk image displayed when reading from disk.

DOOM_C_API void I_EnableLoadingDisk(int xoffs, int yoffs);

DOOM_C_API void I_VideoClearBuffer( float_t r, float_t g, float_t b, float_t a );

DOOM_C_API extern boolean screenvisible;

DOOM_C_API extern int32_t vanilla_keyboard_mapping;
DOOM_C_API extern boolean screensaver_mode;
DOOM_C_API extern int32_t usegamma;
DOOM_C_API extern pixel_t *I_VideoBuffer;

DOOM_C_API extern int32_t screen_width;
DOOM_C_API extern int32_t screen_height;
DOOM_C_API extern int32_t fullscreen;
DOOM_C_API extern int32_t aspect_ratio_correct;
DOOM_C_API extern int32_t integer_scaling;
DOOM_C_API extern int32_t vga_porch_flash;

DOOM_C_API extern char *window_position;
DOOM_C_API void I_GetWindowPosition(int *x, int *y, int w, int h);

// Joystic/gamepad hysteresis
DOOM_C_API extern uint64_t joywait;

#endif
