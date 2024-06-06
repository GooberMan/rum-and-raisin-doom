//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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

#if !defined( __M_ERROR_H__ )
#define __M_ERROR_H__

#include "doomtype.h"

DOOM_C_API void I_ErrorInit( void );
DOOM_C_API void I_ErrorUpdate( void );
DOOM_C_API void I_Error(const char *error, ...) NORETURN PRINTF_ATTR(1, 2);

#define I_ErrorIf( cond, ... ) if( cond ) { I_Error( __VA_ARGS__ ); }

#endif //!defined( __M_ERROR_H__ )