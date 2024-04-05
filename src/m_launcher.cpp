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

#include "m_launcher.h"

#include "m_argv.h"
#include "m_container.h"

#include "i_system.h"
#include "i_video.h"

#include "cimguiglue.h"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <iomanip>

#if defined( WIN32 )
#define popen _popen
#define pclose _pclose
#endif

static bool schedule_launcher = false;

std::string M_DashboardLauncherWindow();
void M_DashboardPrepareRender();
void M_DashboardFinaliseRender();

DoomString M_PerformCommandLineLauncher( DoomString& command )
{
#if USE_IMGUI
	dashboardactive = Dash_Launcher;
	I_UpdateMouseGrab();

	SDL_Renderer* renderer = I_GetRenderer();

	I_StartTic();
	I_StartFrame();
	SDL_RenderClear( renderer );
	M_DashboardPrepareRender();

	constexpr const char* DisplayText = "Running launcher...";
	constexpr int32_t DisplayFlags = ( ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs );

	ImVec2 size;
	igCalcTextSize( &size, DisplayText, nullptr, false, -1 );
	size.x += 10;
	size.y += 4;
	ImVec2 cursor = { ( igGetIO()->DisplaySize.x - size.x ) * 0.5, ( igGetIO()->DisplaySize.y - size.y ) * 0.5 };

	igSetNextWindowSize( size, ImGuiCond_Always );
	igSetNextWindowPos( cursor, ImGuiCond_Always, ImVec2() );

	if( igBegin( DisplayText, nullptr, DisplayFlags ) )
	{
		igPushStyleColor_U32( ImGuiCol_Text, IM_COL32_WHITE );
		igText( DisplayText );
		igPopStyleColor( 1 );
	}
	igEnd();

	M_DashboardFinaliseRender();
	SDL_RenderPresent( renderer );

	dashboardactive = Dash_Inactive;
#endif // USE_IMGUI

	DoomString result;
	FILE* pipe = popen( command.c_str(), "r" );
	if( pipe )
	{
		std::array< char, 1024 > buffer;
		while( pipe )
		{
			fgets( buffer.data(), buffer.size(), pipe );
			if( feof( pipe ) )
			{
				break;
			}
			result += buffer.data();
		}
		pclose( pipe );
	}

	std::replace( result.begin(), result.end(), '\n', ' ' );

	return result;
}

void M_ScheduleLauncher()
{
	schedule_launcher = true;
}

doombool M_IsLauncherScheduled()
{
	return schedule_launcher;
}

doombool M_PerformLauncher()
{
	int32_t launcher_arg = M_CheckParm( "-launcher" );

	if( schedule_launcher || launcher_arg )
	{
		DoomString launcher_with_params;

		for( int32_t param = launcher_arg + 1; launcher_arg > 0 && param != myargc && myargv[ param ][ 0 ] != '-'; ++param )
		{
			if( !launcher_with_params.empty() )
			{
				launcher_with_params += ' ';
			}

			const char* thisparam = myargv[ param ];
			if( !launcher_with_params.empty() && strstr( thisparam, " " ) != nullptr )
			{
				launcher_with_params += '\"';
				launcher_with_params += thisparam;
				launcher_with_params += '\"';
			}
			else
			{
				launcher_with_params += thisparam;
			}
		}

		DoomString launcher_args;

		if( !launcher_with_params.empty() )
		{
			launcher_args = M_PerformCommandLineLauncher( launcher_with_params );
		}
		else
		{
			launcher_args = M_DashboardLauncherWindow().c_str();
		}

		std::vector< DoomString > params;
		DoomString curr_param;

		DoomIStringStream argsstream( launcher_args );
		while( true )
		{
			argsstream >> std::quoted( curr_param, '\"', '\0' );
			if( curr_param.empty() )
			{
				break;
			}
			params.push_back( curr_param );
			curr_param = "";
			if( argsstream.eof() )
			{
				break;
			}
		}

		if( !params.empty() )
		{
			M_ReplaceFileParameters( params );
		}

		return true;
	}

	return false;
}