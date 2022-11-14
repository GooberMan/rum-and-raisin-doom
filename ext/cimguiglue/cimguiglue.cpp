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

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include "cimgui.h"
#include "cimguiglue.h"

#include <chrono>

CIMGUI_API int CImGui_ImplSDL2_InitForOpenGL( void* window, void* sdl_gl_context )
{
	return ImGui_ImplSDL2_InitForOpenGL( (SDL_Window*)window, sdl_gl_context );
}

CIMGUI_API int CImGui_ImplOpenGL3_Init( const char* glsl_version )
{
	return ImGui_ImplOpenGL3_Init( glsl_version );
}

CIMGUI_API int CImGui_ImplSDL2_ProcessEvent( void* sdl_event )
{
	return ImGui_ImplSDL2_ProcessEvent( (SDL_Event*)sdl_event );
}

CIMGUI_API void CImGui_ImplOpenGL3_NewFrame( void )
{
	ImGui_ImplOpenGL3_NewFrame();
}

CIMGUI_API void CImGui_ImplSDL2_NewFrame( void* window )
{
	ImGui_ImplSDL2_NewFrame( (SDL_Window*) window );
}

CIMGUI_API void CImGui_ImplOpenGL3_RenderDrawData( ImDrawData* draw_data )
{
	ImGui_ImplOpenGL3_RenderDrawData( draw_data );
}

CIMGUI_API void igImageQuad( ImTextureID user_texture_id, const ImVec2 size, const ImVec2 uvtl, const ImVec2 uvtr, const ImVec2 uvlr, const ImVec2 uvll, const ImVec4 tint_col, const ImVec4 border_col )
{
	ImVec2 tl;
	ImVec2 tr;
	ImVec2 lr;
	ImVec2 ll;
	ImRect bb;

	ImGuiWindow* window = igGetCurrentWindow();
	if (window->SkipItems)
		return;

	bb.Min = bb.Max = window->DC.CursorPos;
	bb.Max.x += size.x;
	bb.Max.y += size.y;

	if (border_col.w > 0.0f)
	{
		bb.Max.x += 2;
		bb.Max.y += 2;
	}
	igItemSizeRect(bb, -1.f);
	if ( !igItemAdd(bb, 0, 0) )
		return;

	tl = bb.Min;
	tr = ImVec2( bb.Max.x, bb.Min.y );
	lr = bb.Max;
	ll = ImVec2( bb.Min.x, bb.Max.y );

	if (border_col.w > 0.0f)
	{
		ImDrawList_AddRect( window->DrawList, bb.Min, bb.Max, igGetColorU32Vec4(border_col), 0.0f, ImDrawCornerFlags_All, 1.0f);
		ImDrawList_AddImageQuad( window->DrawList, user_texture_id, tl, tr, lr, ll, uvtl, uvtr, uvlr, uvll, igGetColorU32Vec4(tint_col));
	}
	else
	{
		ImDrawList_AddImageQuad( window->DrawList, user_texture_id, tl, tr, lr, ll, uvtl, uvtr, uvlr, uvll, igGetColorU32Vec4(tint_col));
	}
}

CIMGUI_API void igPushScrollableArea( const char* ID, ImVec2 size )
{
	igPushIDStr( ID );
	igPushStyleColorU32( ImGuiCol_ChildBg, IM_COL32_BLACK_TRANS );
	igBeginChildStr( "ScrollArea", size, false, ImGuiWindowFlags_None );
}

CIMGUI_API void igPopScrollableArea( void )
{
	igEndChild();
	igPopStyleColor( 1 );
	igPopID();
}

CIMGUI_API void igFontConfigConstruct( ImFontConfig* config )
{
	new( (void*)config ) ImFontConfig();
}

CIMGUI_API void igCentreNextElement( float_t width )
{
	float_t centre = igGetWindowContentRegionWidth() * 0.5f;
	ImVec2 cursor;
	igGetCursorPos( &cursor );
	cursor.x += ( centre - width / 2 );
	igSetCursorPos( cursor );
}

CIMGUI_API void igRoundProgressBar( float_t progress, ImVec2 size, float_t rounding )
{
	ImGuiWindow* window = igGetCurrentWindow();
	if (window->SkipItems)
		return;

	if( size.x <= 0 )
	{
		size.x = igGetWindowContentRegionWidth();
	}
	ImGuiContext* context = igGetCurrentContext();
	if( context->NextItemData.Flags & ImGuiNextItemDataFlags_HasWidth )
	{
		size.x = context->NextItemData.Width;
	}

	ImRect bb;
	bb.Min = bb.Max = window->DC.CursorPos;
	bb.Max.x += size.x;
	bb.Max.y += size.y;

	igItemSizeRect( bb, -1.f );
	if ( !igItemAdd(bb, 0, 0) )
		return;

	ImDrawList_AddRectFilled( window->DrawList, bb.Min, bb.Max, igGetColorU32Col( ImGuiCol_FrameBg, 1.0f ), rounding, ImDrawCornerFlags_All );

	bb.Min.x += rounding + 1;
	bb.Min.y += rounding + 1;
	bb.Max.x -= rounding + 1;
	bb.Max.y -= rounding + 1;

	bb.Max.x = bb.Min.x + ( bb.Max.x - bb.Min.x ) * progress;

	ImDrawList_AddRectFilled( window->DrawList, bb.Min, bb.Max, igGetColorU32Col( ImGuiCol_PlotHistogram, 1.0f ), 0, ImDrawCornerFlags_None );
}

CIMGUI_API void igSpinner( ImVec2 size, double_t cycletime )
{
	ImGuiWindow* window = igGetCurrentWindow();
	if (window->SkipItems)
		return;

	ImVec2 iconcursor;
	igGetCursorScreenPos( &iconcursor );

	float maxdimension = size.x > size.y ? size.x : size.y;

	ImRect bb;
	bb.Min = bb.Max = window->DC.CursorPos;
	bb.Max.x += maxdimension;
	bb.Max.y += maxdimension;

	igItemSizeRect( bb, -1.f );
	if ( !igItemAdd(bb, 0, 0) )
		return;

	constexpr double_t pi2 = 3.14159265358979323846264338327950288 * 2.0;

	auto now = std::chrono::system_clock::now();
	auto epoch = now.time_since_epoch();
	auto microseconds = std::chrono::duration_cast< std::chrono::microseconds >( epoch );

	double_t rotationpercent = fmod( microseconds.count() / 1000000.0, cycletime ) / cycletime;
	double_t angle = rotationpercent * pi2;

	iconcursor.x += maxdimension * 0.5f;
	iconcursor.y += maxdimension * 0.5f;

	auto rotate = [ &angle, &iconcursor ]( ImVec2& vec )
	{
		double_t currsin = sin( angle );
		double_t currcos = cos( angle );
		ImVec2 ogvec = vec;
		vec.x = iconcursor.x
					+ currcos * ogvec.x
					- currsin * ogvec.y;
		vec.y = iconcursor.y
					+ currsin * ogvec.x
					+ currcos * ogvec.y;
	};

	ImVec2 points[] =
	{
		{ -size.x * 0.025f, -size.y * 0.025f },		{ 0, size.y * 0.5f },		{ size.x * 0.5f, 0 },
		{ size.x * 0.025f, size.y * 0.025f },		{ 0, -size.y * 0.5f },		{ -size.x * 0.5f, 0 },
	};

	rotate( points[ 0 ] );
	rotate( points[ 1 ] );
	rotate( points[ 2 ] );
	rotate( points[ 3 ] );
	rotate( points[ 4 ] );
	rotate( points[ 5 ] );

	ImDrawList* drawlist = igGetWindowDrawList();
	ImDrawList_AddTriangleFilled( drawlist, points[ 0 ], points[ 1 ], points[ 2 ], igGetColorU32Col( ImGuiCol_PlotHistogram, 1.0f ) );
	ImDrawList_AddTriangleFilled( drawlist, points[ 3 ], points[ 4 ], points[ 5 ], igGetColorU32Col( ImGuiCol_PlotHistogram, 1.0f ) );
}
