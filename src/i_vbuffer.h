//
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
// DESCRIPTION: Video buffer abstraction
//

#ifndef __I_RENDERBUFFER__
#define __I_RENDERBUFFER__

#include "doomtype.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <SDL2/SDL.h>

typedef enum vbuffermode_e
{
	VB_Normal,
	VB_Transposed,
} vbuffermode_t;

enum
{
	vbuffer_magic = 0x7ac71e55,
};

// TODO: Should align 16
typedef struct vbuffer_s
{
	SDL_Surface*	data8bithandle;
	SDL_Surface*	dataARGBhandle;
	SDL_Texture*	datatexture;
	pixel_t*		data;
	void*			user;
	int32_t			width;
	int32_t			height;
	int32_t			pitch;
	int32_t			pixel_size_bytes;
	vbuffermode_t	mode;
	float_t			verticalscale;
	int32_t			magic_value;
} vbuffer_t;

#ifdef __cplusplus
}
#endif

#endif // __I_RENDERBUFFER__
