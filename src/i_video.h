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

// Screen width and height.
// Every compiler will do literal calculations at compile time these days, so let's be always correct about it.
// Multiply to big values so that integer divides don't lose information. Convert SCREENHEIGHT to be 16:10 correct,
// and SCREENHEIGHT_4_3 to be 4:3 correct.

#define MAXSCREENWIDTH		3200
#define MAXSCREENHEIGHT		( MAXSCREENWIDTH * 6250 / 10000 )

#define SCREENWIDTH  2560
#define SCREENHEIGHT ( SCREENWIDTH * 6250 / 10000 )
#define SCREENHEIGHT_4_3 ( SCREENWIDTH * 7500 / 10000 )

#define VANILLA_SCREENWIDTH 320
#define VANILLA_SCREENHEIGHT 200

#define V_VIRTUALWIDTH 320
#define V_VIRTUALHEIGHT 200

// TODO: We need to redo this to be pure integer math. Fixed/float results in inaccuracies at different resolutions :-(
#define V_WIDTHSTEP ( ( V_VIRTUALWIDTH << FRACBITS ) / aspect_adjusted_render_width )
#define V_WIDTHMULTIPLIER ( ( aspect_adjusted_render_width << FRACBITS ) / V_VIRTUALWIDTH )

#define V_HEIGHTSTEP ( ( V_VIRTUALHEIGHT << FRACBITS ) / render_height )
#define V_HEIGHTMULTIPLIER ( ( render_height << FRACBITS ) / V_VIRTUALHEIGHT )

typedef boolean (*grabmouse_callback_t)(void);

// Called by D_DoomMain,
// determines the hardware configuration
// and sets up the video mode
void I_InitGraphics( void );
void I_InitBuffers( int32_t numbuffers );
void I_UpdateMouseGrab( void );

vbuffer_t* I_GetRenderBuffer( int32_t index );
void I_SetRenderBufferValidColumns( int32_t index, int32_t begin, int32_t end );

SDL_Window* I_GetWindow( void );
SDL_Renderer* I_GetRenderer( void );
int32_t I_GetRefreshRate( void );

void I_GraphicsCheckCommandLine(void);

void I_ShutdownGraphics(void);

void I_ToggleFullScreen(void);
void I_SetWindowDimensions( int32_t w, int32_t h );
void I_SetRenderDimensions( int32_t w, int32_t h );

// Takes full 8 bit values.
void I_SetPalette (byte* palette);
int I_GetPaletteIndex(int r, int g, int b);

void I_UpdateNoBlit (void);
void I_FinishUpdate( vbuffer_t* activebuffer );

void I_ReadScreen (pixel_t* scr);

void I_SetWindowTitle(const char *title);

void I_CheckIsScreensaver(void);
void I_SetGrabMouseCallback(grabmouse_callback_t func);

void I_DisplayFPSDots(boolean dots_on);
void I_BindVideoVariables(void);

void I_InitWindowTitle(void);
void I_InitWindowIcon(void);

// Called before processing any tics in a frame (just after displaying a frame).
// Time consuming syncronous operations are performed here (joystick reading).

void I_StartFrame (void);

// Called before processing each tic in a frame.
// Quick syncronous operations are performed here.

void I_StartTic (void);

// Enable the loading disk image displayed when reading from disk.

void I_EnableLoadingDisk(int xoffs, int yoffs);

extern boolean screenvisible;

extern int32_t vanilla_keyboard_mapping;
extern boolean screensaver_mode;
extern int32_t usegamma;
extern pixel_t *I_VideoBuffer;

extern int32_t screen_width;
extern int32_t screen_height;
extern int32_t fullscreen;
extern int32_t aspect_ratio_correct;
extern int32_t integer_scaling;
extern int32_t vga_porch_flash;

extern char *window_position;
void I_GetWindowPosition(int *x, int *y, int w, int h);

// Joystic/gamepad hysteresis
extern uint64_t joywait;

#endif
