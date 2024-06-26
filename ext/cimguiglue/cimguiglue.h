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
//	Additional glue for cimgui SDL/GL3 layers
//

#if !defined(_CIMGUIGLUE_H_)
#define _CIMGUIGLUE_H_

#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
	#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif // CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#if defined( __cplusplus )
extern "C" {
#endif
#include "cimgui.h"
#if defined( __cplusplus )
}
#endif

CIMGUI_API int CImGui_ImplSDL2_InitForOpenGL( void* window, void* sdl_gl_context );
CIMGUI_API int CImGui_ImplOpenGL3_Init( const char* glsl_version );
CIMGUI_API int CImGui_ImplSDL2_ProcessEvent( void* sdl_event );

CIMGUI_API void CImGui_ImplOpenGL3_NewFrame( void );
CIMGUI_API void CImGui_ImplSDL2_NewFrame( void* window );

CIMGUI_API void CImGui_ImplOpenGL3_RenderDrawData( ImDrawData* draw_data );

CIMGUI_API void igImageQuad( ImTextureID user_texture_id, const ImVec2 size, const ImVec2 uvtl, const ImVec2 uvtr, const ImVec2 uvlr, const ImVec2 uvll, const ImVec4 tint_col, const ImVec4 border_col );

CIMGUI_API void igPushScrollableArea( const char* ID, ImVec2 size );
CIMGUI_API void igPopScrollableArea( void );

CIMGUI_API void igFontConfigConstruct( ImFontConfig* config );

CIMGUI_API void igCentreNextElement( float_t width );

CIMGUI_API void igRoundProgressBar( float_t progress, ImVec2 size, float_t rounding );

CIMGUI_API void igSpinner( ImVec2 size, double_t cycletime );

CIMGUI_API void igCentreText( const char* string );

#if defined( __cplusplus )

template< typename... _args >
void igCentreText( const char* string, _args&... args )
{
	char buffer[ 1024 ];

	M_snprintf( buffer, 1024, string, args... );
	igCentreText( buffer );
}

template< typename... _args >
void igCentreText( const char* string, _args&&... args )
{
	igCentreText( string, args... );
}

#endif // defined( __cplusplus )

// Helpers macros to generate 32-bit encoded colors
#ifdef IMGUI_USE_BGRA_PACKED_COLOR
#define IM_COL32_R_SHIFT    16
#define IM_COL32_G_SHIFT    8
#define IM_COL32_B_SHIFT    0
#define IM_COL32_A_SHIFT    24
#define IM_COL32_A_MASK     0xFF000000
#else
#define IM_COL32_R_SHIFT    0
#define IM_COL32_G_SHIFT    8
#define IM_COL32_B_SHIFT    16
#define IM_COL32_A_SHIFT    24
#define IM_COL32_A_MASK     0xFF000000
#endif
#define IM_COL32(R,G,B,A)    (((ImU32)(A)<<IM_COL32_A_SHIFT) | ((ImU32)(B)<<IM_COL32_B_SHIFT) | ((ImU32)(G)<<IM_COL32_G_SHIFT) | ((ImU32)(R)<<IM_COL32_R_SHIFT))
#define IM_COL32_WHITE       IM_COL32(255,255,255,255)  // Opaque white = 0xFFFFFFFF
#define IM_COL32_BLACK       IM_COL32(0,0,0,255)        // Opaque black
#define IM_COL32_BLACK_TRANS IM_COL32(0,0,0,0)          // Transparent black = 0x00000000

#endif // _CIMGUIGLUE_H_
