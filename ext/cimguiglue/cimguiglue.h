//
// Copyright(C) 2020-2021 Ethan Watson
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

#ifdef __cplusplus
extern "C" {
#endif

int CImGui_ImplSDL2_InitForOpenGL( void* window, void* sdl_gl_context );
int CImGui_ImplOpenGL3_Init( const char* glsl_version );
int CImGui_ImplSDL2_ProcessEvent( void* sdl_event );

void CImGui_ImplOpenGL3_NewFrame( void );
void CImGui_ImplSDL2_NewFrame( void* window );

void CImGui_ImplOpenGL3_RenderDrawData( ImDrawData* draw_data );

void igImageQuad(ImTextureID user_texture_id,const ImVec2 size,const ImVec2 uvtl,const ImVec2 uvtr,const ImVec2 uvlr,const ImVec2 uvll,const ImVec4 tint_col,const ImVec4 border_col);

#ifdef __cplusplus
}
#endif

#endif // _CIMGUIGLUE_H_
