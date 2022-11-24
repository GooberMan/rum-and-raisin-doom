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
// DESCRIPTION:
//	Configuration file for imgui
//

#if !defined( __CIMGUIGLUE_CONFIG_H__ )
#define __CIMGUIGLUE_CONFIG_H__

//#define IMGUI_DISABLE_DEMO_WINDOWS

#if defined( __linux__ ) && defined( __arm__ )
// Assuming Raspberry Pi
#define IMGUI_IMPL_OPENGL_ES3 1
#endif // Raspberry Pi check

#endif // __CIMGUIGLUE_CONFIG_H__