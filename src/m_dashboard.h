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

#if defined( __APPLE__ )
// And you can come back out after you think about what you've done.
#define USE_IMGUI 0
#else
#define USE_IMGUI 1
#endif

#include "doomtype.h"
#include "d_event.h"

DOOM_C_API typedef enum remapping_e
{
	Remap_None,
	Remap_Key,
	Remap_Mouse,
	Remap_Joystick,

	Remap_Max,
} remapping_t;

DOOM_C_API typedef enum menuproperties_e
{
	Menu_Normal			= 0x00000000,
	Menu_Overlay		= 0x00000001,
	Menu_AllowDetach	= 0x00000002,
} menuproperties_t;

DOOM_C_API typedef enum dashboardmode_e
{
	Dash_Inactive,
	Dash_Normal,
	Dash_FirstSession,
	Dash_Launcher,
} dashboardmode_t;

DOOM_C_API typedef enum licence_e
{
	Licence_SDL,
	Licence_DearImGui,
	Licence_cimgui,
	Licence_WidePix,
	Licence_InconsolataFont,
} licence_t;

DOOM_C_API typedef void (*menufunc_t)( const char* itemname, void* data );

DOOM_C_API void M_InitDashboard( void );
DOOM_C_API void M_DashboardSetLicenceInUse( licence_t licence, boolean inuse );
DOOM_C_API void M_BindDashboardVariables( void );
DOOM_C_API void M_DashboardFirstLaunch( void );
DOOM_C_API void M_DashboardKeyRemap( remapping_t type, int32_t* key, const char* mappingname );
DOOM_C_API boolean M_DashboardResponder( event_t* ev );
DOOM_C_API void M_RenderDashboardLogContents( void );
DOOM_C_API void M_RenderDashboard( int32_t windowwidth, int32_t windowheight, int32_t backbufferid );


// menuname can be categorised with pipes, ie Edit|Preferences|Some pref category

DOOM_C_API void M_RegisterDashboardCategory( const char* full_path );
DOOM_C_API void M_RegisterDashboardButton( const char* full_path, const char* tooltip, menufunc_t callback, void* callbackdata );
DOOM_C_API void M_RegisterDashboardWindow( const char* full_path, const char* caption, int32_t initialwidth, int32_t initialheight, boolean* active, menuproperties_t properties, menufunc_t callback );
DOOM_C_API void M_RegisterDashboardCheckbox( const char* full_path, const char* tooltip, boolean* value );
DOOM_C_API void M_RegisterDashboardCheckboxFlag( const char* full_path, const char* tooltip, int32_t* value, int32_t flagsval );
DOOM_C_API void M_RegisterDashboardRadioButton( const char* full_path, const char* tooltip, int32_t* value, int32_t selectedval );
DOOM_C_API void M_RegisterDashboardSeparator( const char* category_path );

#endif // !defined( __M_DASHBOARD_H__ )
