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
// DESCRIPTION: Instrumented profiling library. Might expand in
// the future to hook in to external profilers too.
//

#include "m_profile.h"

#include "m_container.h"
#include "m_dashboard.h"

#include "i_timer.h"
#include "i_thread.h"

#include "cimguiglue.h"

#include <atomic>
#include <thread>

typedef struct profiledata_s profiledata_t;

struct profiledata_s
{
	DoomString						markername;
	DoomString						filename;
	size_t							linenumber;

	int64_t							numcalls;

	int64_t							inclusivetime;
	int64_t							exclusivetime;

	uint64_t						starttime;
	uint64_t						endtime;

	profiledata_t*					workingparent;

	DoomVector< profiledata_s >		childcalls;
};

typedef struct profilethread_s
{
	DoomString						threadname;
	profiledata_t					rootprofile;
} profilethread_t;

typedef enum profilemode_e : int32_t
{
	PM_Uninitialized,
	PM_Capturing,
	PM_SingleFrame,
} profilemode_t;

constexpr int32_t MaxProfileThreads = 128;

thread_local profilethread_t profilethread;
thread_local profiledata_t* currprofile = nullptr;

static std::atomic< profilemode_t >		profilemode;
static std::atomic< bool >				rendering;
static profilethread_t*					profilethreads[ MaxProfileThreads ];
static std::atomic< int32_t >			numprofilethreads;

static int32_t							currthreadview = 0;

static boolean							profilewindowactive = false;

constexpr auto ProfileThreads()			{ return std::span( profilethreads, numprofilethreads ); }
constexpr auto CurrProfileThread()		{ return profilethreads[ currthreadview ]; }

static void M_ProfileRenderData( profiledata_t& data )
{
	rendering = true;

	for( profiledata_t& childdata : data.childcalls )
	{
		int32_t flags = childdata.childcalls.empty() ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_None;

		bool expanded = igTreeNodeExStr( childdata.markername.c_str(), flags );
		igNextColumn();
		igText( "%d", childdata.numcalls );
		igNextColumn();
		igText( "%0.3lfms", (double)childdata.inclusivetime * 0.001 );
		igNextColumn();
		igText( "%0.3lfms", (double)childdata.exclusivetime * 0.001 );
		igNextColumn();

		if( expanded )
		{
			if( !childdata.childcalls.empty() )
			{
				M_ProfileRenderData( childdata );
			}
			igTreePop();
		}
	}

	rendering = false;
}

DOOM_C_API static void M_ProfileWindow( const char* itemname, void* data )
{
	constexpr ImVec2 zero = { 0, 0 };

	switch( profilemode.load() )
	{
	case PM_Capturing:
		if( igButton( "Capture frame", zero ) )
		{
			profilemode = PM_SingleFrame;
		}
		break;
	case PM_SingleFrame:
		if( igButton( "Resume", zero ) )
		{
			profilemode = PM_Capturing;
		}
		break;
	default:
		return;
	}

	if( numprofilethreads.load() > 1 )
	{
		igSameLine( 0, -1 );
		if( igBeginCombo( "Thread", CurrProfileThread()->threadname.c_str(), ImGuiComboFlags_None ) )
		{
			int32_t index = 0;
			for( profilethread_t* thread : ProfileThreads() )
			{
				bool selected = index == currthreadview;
				if( igSelectableBool( thread->threadname.c_str(), selected, ImGuiSelectableFlags_None, zero ) )
				{
					currthreadview = index;
				}
				++index;
			}
			igEndCombo();
		}
	}

	igPushScrollableArea( "ProfileView", zero );
	{
		ImVec2 textsize;
		ImVec2 contentsize;

		igGetContentRegionAvail( &contentsize );

		igColumns( 4, "ProfileViewColumns", false );

		igCalcTextSize( &textsize, "Call count", nullptr, false, -1.f );
		igSetColumnWidth( 1, textsize.x + 10.f );
		contentsize.x -= ( textsize.x + 10.f );

		igCalcTextSize( &textsize, "Inclusive", nullptr, false, -1.f );
		igSetColumnWidth( 2, textsize.x + 10.f );
		contentsize.x -= ( textsize.x + 10.f );

		igCalcTextSize( &textsize, "Exclusive", nullptr, false, -1.f );
		igSetColumnWidth( 3, textsize.x + 10.f );
		contentsize.x -= ( textsize.x + 10.f );

		igSetColumnWidth( 0, contentsize.x );

		igText( "Marker" );
		igNextColumn();
		igText( "Call count" );
		igNextColumn();
		igText( "Inclusive" );
		igNextColumn();
		igText( "Exclusive" );
		igEndColumns();

		igSeparator();
		igColumns( 4, "ProfileViewColumns", false );

		M_ProfileRenderData( CurrProfileThread()->rootprofile );

		igEndColumns();
	}
	igPopScrollableArea();
}

DOOM_C_API void M_ProfileInit( void )
{
	M_RegisterDashboardWindow( "Tools|Profiling", "Profiling", 500, 400, &profilewindowactive, Menu_Normal, &M_ProfileWindow );

	profilemode = PM_Capturing;
}

DOOM_C_API void M_ProfileThreadInit( const char* threadname )
{
	profilethread.threadname = threadname;

	int32_t threadindex = numprofilethreads.fetch_add( 1 );
	profilethreads[ threadindex ] = &profilethread;
}

DOOM_C_API void M_ProfileNewFrame( void )
{
	if( profilemode.load() != PM_Capturing ) return;
	while( rendering.load() ) { I_Yield(); }

	profilethread.rootprofile = profiledata_t();
	currprofile = &profilethread.rootprofile;
}

DOOM_C_API void M_ProfilePushMarker( const char* markername, const char* file, size_t line )
{
	if( profilemode.load() != PM_Capturing ) return;
	while( rendering.load() ) { I_Yield(); }

	DoomString marker = markername;
	auto found = std::find_if( currprofile->childcalls.begin(), currprofile->childcalls.end(), [ &marker ]( const profiledata_t& data )
	{
		return data.markername == marker;
	} );

	if( found != currprofile->childcalls.end() )
	{
		++found->numcalls;
		found->starttime = I_GetTimeUS();
		found->endtime = 0;
		found->workingparent = currprofile;
		currprofile = &*found;
	}
	else
	{
		profiledata_t newdata = { marker, file, line, 1, 0, 0, I_GetTimeUS(), 0, currprofile };
		currprofile = &*currprofile->childcalls.emplace( currprofile->childcalls.end(), newdata );
	}
}

DOOM_C_API void M_ProfilePopMarker( const char* markername )
{
	if( profilemode.load() != PM_Capturing ) return;
	while( rendering.load() ) { I_Yield(); }

	currprofile->endtime = I_GetTimeUS();
	int64_t totaltime = currprofile->endtime - currprofile->starttime;

	currprofile->inclusivetime += totaltime;
	currprofile->exclusivetime += totaltime;

	profiledata_t* prev = currprofile;
	currprofile = currprofile->workingparent;
	currprofile->exclusivetime -= totaltime;
	prev->workingparent = nullptr;
}
