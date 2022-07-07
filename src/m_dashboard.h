//
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

#ifndef __M_DASHBOARD_H__
#define __M_DASHBOARD_H__

#include "doomtype.h"

#if defined( __APPLE__ )
// And you can come back out after you think about what you've done.
#define USE_IMGUI 0
#else
#define USE_IMGUI 1
#endif

#if defined( __cplusplus )
extern "C" {
#endif

typedef enum remapping_e
{
	Remap_None,
	Remap_Key,
	Remap_Mouse,
	Remap_Joystick,

	Remap_Max,
} remapping_t;

typedef enum menuproperties_e
{
	Menu_Normal			= 0x00000000,
	Menu_Overlay		= 0x00000001,
} menuproperties_t;

typedef void (*menufunc_t)( const char* itemname, void* data );

void M_InitDashboard( void );
void M_BindDashboardVariables( void );
void M_RenderDashboard( void );

// menuname can be categorised with pipes, ie Edit|Preferences|Some pref category

void M_RegisterDashboardCategory( const char* full_path );
void M_RegisterDashboardButton( const char* full_path, const char* tooltip, menufunc_t callback, void* callbackdata );
void M_RegisterDashboardWindow( const char* full_path, const char* caption, int32_t initialwidth, int32_t initialheight, boolean* active, menuproperties_t properties, menufunc_t callback );
void M_RegisterDashboardCheckbox( const char* full_path, const char* tooltip, boolean* value );
void M_RegisterDashboardCheckboxFlag( const char* full_path, const char* tooltip, int32_t* value, int32_t flagsval );
void M_RegisterDashboardRadioButton( const char* full_path, const char* tooltip, int32_t* value, int32_t selectedval );
void M_RegisterDashboardSeparator( const char* category_path );

#if defined( __cplusplus )
}
#endif

#endif // !defined( __M_DASHBOARD_H__ )
