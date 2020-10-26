
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include "cimgui.h"
#include "cimguiglue.h"

int CImGui_ImplSDL2_InitForOpenGL( void* window, void* sdl_gl_context )
{
	return ImGui_ImplSDL2_InitForOpenGL( (SDL_Window*)window, sdl_gl_context );
}

int CImGui_ImplOpenGL3_Init( const char* glsl_version )
{
	return ImGui_ImplOpenGL3_Init( glsl_version );
}

int CImGui_ImplSDL2_ProcessEvent( void* sdl_event )
{
	return ImGui_ImplSDL2_ProcessEvent( (SDL_Event*)sdl_event );
}

void CImGui_ImplOpenGL3_NewFrame( void )
{
	ImGui_ImplOpenGL3_NewFrame();
}

void CImGui_ImplSDL2_NewFrame( void* window )
{
	ImGui_ImplSDL2_NewFrame( (SDL_Window*) window );
}

void CImGui_ImplOpenGL3_RenderDrawData( ImDrawData* draw_data )
{
	ImGui_ImplOpenGL3_RenderDrawData( draw_data );
}
