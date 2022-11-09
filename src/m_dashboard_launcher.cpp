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

#include "m_dashboard.h"
#include "cimguiglue.h"

#include "i_log.h"
#include "i_system.h"
#include "i_video.h"

#include "d_iwad.h"
#include "w_wad.h"

#include "m_config.h"
#include "m_container.h"
#include "m_misc.h"

#include "v_patch.h"
#include "v_video.h"

#include "glad/glad.h"
#include "SDL_opengl.h"

#include <filesystem>
#include <chrono>
#include <ranges>
#include <regex>
#include <thread>
#include <cstdio>

extern "C"
{
	extern int32_t dashboardactive;

	extern int32_t frame_width;
	extern int32_t frame_adjusted_width;
	extern int32_t frame_height;
}

void M_DashboardApplyTheme();
void M_DashboardPrepareRender();
void M_DashboardFinaliseRender();

constexpr const char* GameModes[] =
{
	"Doom Shareware",
	"Doom Registered",
	"Doom II",
	"Ultimate Doom",
	"Unknown game"
};

constexpr const char* GameVariants[] =
{
	"Original",
	"FreeDoom",
	"FreeDM",
	"BFG Edition",
	"Unity Port"
};

namespace launcher
{
	struct Image
	{
		hwtexture_t*			tex;
		int32_t					width;
		int32_t					height;
	};

	enum class WADType
	{
		None,
		IWAD,
		PWAD,
	};

	struct WADEntry
	{
		WADType			type;
		DoomString		filename;
		DoomString		full_path;
		DoomString		cache_path;
		uint64_t		file_length;
		time_t			last_modified;
		GameMode_t		game_mode;
		GameVariant_t	game_variant;

		Image			titlepic;
	};

	time_t GetLastModifiedTime( const std::filesystem::path& stdpath )
	{
		auto filetime = std::filesystem::last_write_time( stdpath );
		auto utctime = std::chrono::file_clock::to_utc( filetime );
		auto systime = std::chrono::utc_clock::to_sys( utctime );
		return std::chrono::system_clock::to_time_t( systime );
	}

	WADEntry ParseWAD( const std::filesystem::path& stdpath )
	{
		std::string path = stdpath.string();
		std::string filename = stdpath.filename().string();

		time_t lastmodified = GetLastModifiedTime( stdpath );
		size_t filelength = std::filesystem::file_size( stdpath );

		std::string cachepath = configdir;
		if( cachepath.empty() ) cachepath += ".";
		if( !cachepath.ends_with( DIR_SEPARATOR_S ) ) cachepath += DIR_SEPARATOR_S;
		cachepath += "wadcache" DIR_SEPARATOR_S "local" DIR_SEPARATOR_S
					+ filename
					+ "."
					+ std::to_string( lastmodified )
					+ "."
					+ std::to_string( filelength )
					+ DIR_SEPARATOR_S;

		std::filesystem::path stdcachepath( cachepath );

		std::vector< rgba_t > titlepic_raw;

		WADEntry entry = WADEntry();
		entry.filename = filename;
		entry.full_path = path;
		entry.cache_path = cachepath;
		entry.last_modified = lastmodified;
		entry.file_length = filelength;

		if( !std::filesystem::exists( stdcachepath ) )
		{
			FILE* file = fopen( path.c_str(), "rb" );
			if( file )
			{
				wadinfo_t header;

				fread( &header, sizeof( wadinfo_t ), 1, file );

				if( !strncmp( header.identification, "IWAD", 4 ) )
				{
					entry.type = WADType::IWAD;
				}
				else if( !strncmp( header.identification, "PWAD", 4 ) )
				{
					entry.type = WADType::PWAD;
				}

				std::vector< filelump_t > lumps;
				lumps.resize( header.numlumps );

				filelump_t* titlepic	= nullptr;
				filelump_t* dmenupic	= nullptr;
				filelump_t* dmapinfo	= nullptr;
				filelump_t* umapinfo	= nullptr;
				filelump_t* playpal		= nullptr;
				filelump_t* m_chg		= nullptr;
				filelump_t* e1m1		= nullptr;
				filelump_t* e2m1		= nullptr;
				filelump_t* e3m1		= nullptr;
				filelump_t* e4m1		= nullptr;
				filelump_t* map01		= nullptr;

				fseek( file, header.infotableofs, SEEK_SET );
				fread( lumps.data(), sizeof( filelump_t ), header.numlumps, file );

				for( filelump_t& lump : lumps )
				{
					if( !strncmp( lump.name, "TITLEPIC", 8 ) )			titlepic = &lump;
					else if( !strncmp( lump.name, "DMENUPIC", 8 ) )		dmenupic = &lump;
					else if( !strncmp( lump.name, "DMAPINFO", 8 ) )		dmapinfo = &lump;
					else if( !strncmp( lump.name, "UMAPINFO", 8 ) )		umapinfo = &lump;
					else if( !strncmp( lump.name, "PLAYPAL", 8 ) )		playpal = &lump;
					else if( !strncmp( lump.name, "M_CHG", 8 ) )		m_chg = &lump;
					else if( !strncmp( lump.name, "E1M1", 8 ) )			e1m1 = &lump;
					else if( !strncmp( lump.name, "E2M1", 8 ) )			e2m1 = &lump;
					else if( !strncmp( lump.name, "E3M1", 8 ) )			e3m1 = &lump;
					else if( !strncmp( lump.name, "E4M1", 8 ) )			e4m1 = &lump;
					else if( !strncmp( lump.name, "MAP01", 8 ) )		map01 = &lump;
				}

				if( map01 != nullptr )
				{
					entry.game_mode = commercial;
				}
				else if( e4m1 != nullptr )
				{
					entry.game_mode = retail;
				}
				else if( e2m1 != nullptr || e3m1 != nullptr )
				{
					entry.game_mode = registered;
				}
				else if( e1m1 != nullptr )
				{
					entry.game_mode = entry.type == WADType::IWAD ? shareware : registered;
				}

				if( entry.type == WADType::IWAD )
				{
					if( m_chg != nullptr )
					{
						entry.game_variant = bfgedition;
					}
					else if( dmenupic != nullptr || dmapinfo != nullptr )
					{
						entry.game_variant = unityport;
					}
					else
					{
						entry.game_variant = vanilla;
					}

					if( !titlepic ) titlepic = dmenupic;
					if( titlepic && playpal )
					{
						std::vector< byte > source;
						source.resize( titlepic->size );
						fseek( file, titlepic->filepos, SEEK_SET );
						fread( source.data(), sizeof( byte ), titlepic->size, file );

						std::vector< rgb_t > pallette;
						pallette.resize( playpal->size / sizeof( rgb_t ) );
						fseek( file, playpal->filepos, SEEK_SET );
						fread( pallette.data(), sizeof( rgb_t ), playpal->size / sizeof( rgb_t ), file );

						patch_t* patch = (patch_t*)source.data();

						std::vector< pixel_t > composite;
						composite.resize( patch->width * patch->height * sizeof( pixel_t ) );

						// This is dirty
						vbuffer_t fakebuffer = vbuffer_t();
						fakebuffer.data = composite.data();
						fakebuffer.width = patch->height;
						fakebuffer.height = patch->width;
						fakebuffer.pitch = patch->height;
						fakebuffer.pixel_size_bytes = sizeof( pixel_t );
						fakebuffer.mode = VB_Transposed;
						fakebuffer.verticalscale = 1.2f;
						fakebuffer.magic_value = vbuffer_magic;

						frame_width = patch->width;
						frame_height = patch->height;
						frame_adjusted_width = patch->width * ( 1.6 / ( (double_t)patch->width / (double_t)patch->height ) );

						// WIDESCREEN HACK
						int32_t xpos = -( ( patch->width - V_VIRTUALWIDTH ) / 2 );
						V_UseBuffer( &fakebuffer );
						V_DrawPatch( xpos, 0, patch );

						titlepic_raw.resize( patch->width * patch->height );
						entry.titlepic.width = patch->width;
						entry.titlepic.height = patch->height;

						for( int32_t x = 0; x < patch->width; ++x )
						{
							pixel_t* source = composite.data() + x * patch->height;
							rgba_t* dest = titlepic_raw.data() + x;
							for( int32_t y = 0; y < patch->height; ++y )
							{
								rgb_t& entry = pallette[ *source ];
								dest->r = entry.r;
								dest->g = entry.g;
								dest->b = entry.b;
								dest->a = 0xFF;
								++source;
								dest += patch->width;
							}
						}
					}
				}

				fclose( file );
			}
		}

		if( !titlepic_raw.empty() )
		{
			entry.titlepic.tex = I_TextureCreate( entry.titlepic.width, entry.titlepic.height, titlepic_raw.data() );
		}

		return entry;
	}

	using WADEntries = std::vector< WADEntry >;

	class WADList
	{
	public:
		WADList()
			: threaded_context( nullptr )
			, lists_ready( false )
			, abort_work( false )
		{
			KickoffPopulate();
		}

		~WADList()
		{
			if( worker_thread->joinable() )
			{
				abort_work = true;
				worker_thread->join();
			}

			delete worker_thread;
		}

		void KickoffPopulate()
		{
			SDL_Window* window = I_GetWindow();
			SDL_GLContext glcontext = SDL_GL_GetCurrentContext();
			SDL_GL_SetAttribute( SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1 );
			threaded_context = SDL_GL_CreateContext( window );
			SDL_GL_MakeCurrent( window, glcontext );

			worker_thread = new std::thread( [ this ]() { this->PopulateWADList(); } );
		}

		void PopulateWADList()
		{
			SDL_Window* window = I_GetWindow();
			SDL_GL_MakeCurrent( window, threaded_context );

			constexpr const char* RegexString = ".+\\.wad";
			std::regex WADRegex = std::regex( RegexString, std::regex_constants::icase );

			auto IWADPaths = D_GetIWADPaths();

			for( auto dir : std::ranges::views::all( IWADPaths )
								| std::ranges::views::transform( []( const char* pathname )
								{
									std::error_code error;
									auto it = std::filesystem::recursive_directory_iterator( std::filesystem::path( pathname ), std::filesystem::directory_options::skip_permission_denied, error );
									if( error ) return std::filesystem::recursive_directory_iterator();
									return it;
								} ) )
			{
				for( auto file : dir )
				{
					if( abort_work.load() == true )
					{
						break;
					}

					std::string pathstring = file.path().string();

					if( std::regex_match( pathstring, WADRegex ) )
					{
						WADEntry entry = ParseWAD( file.path() );
						if( entry.type == WADType::IWAD ) IWADs.push_back( entry );
						if( entry.type == WADType::PWAD ) PWADs.push_back( entry );
					}
				}
			}

			glFlush();
			SDL_GL_DeleteContext( threaded_context );

			lists_ready = !abort_work.load();
		}

		INLINE bool ListsReady() { return lists_ready.load(); }

		WADEntries				IWADs;
		WADEntries				PWADs;
		SDL_GLContext			threaded_context;
		std::atomic< bool >		lists_ready;
		std::atomic< bool >		abort_work;
		std::thread*			worker_thread;
	};

	void igCentreNextElement( float_t width )
	{
		float_t centre = igGetWindowContentRegionWidth() * 0.5f;
		ImVec2 cursor;
		igGetCursorPos( &cursor );
		cursor.x += ( centre - width / 2 );
		igSetCursorPos( cursor );
	}

	template< typename... _args >
	void igCentreText( const char* string, _args&... args )
	{
		ImVec2 textsize;
		char buffer[ 1024 ];

		M_snprintf( buffer, 1024, string, args... );
		igCalcTextSize( &textsize, buffer, nullptr, false, -1 );
		igCentreNextElement( textsize.x );
		igText( buffer );
	}

	class IWADSelector
	{
	public:
		IWADSelector( )
			: iwads( nullptr )
		{
		}

		void Setup( WADEntries& entries )
		{
			iwads = &entries;
			if( !iwads->empty() )
			{
				left = --iwads->end();
				middle = iwads->begin();
				right = ++iwads->begin();
				if( right == iwads->end() )
				{
					right = iwads->begin();
				}
			}
			else
			{
				left = middle = right = iwads->end();
			}
		}

		INLINE bool Valid() const { return iwads != nullptr && middle != iwads->end(); }

		void CycleLeft()
		{
			right = middle;
			middle = left;
			if( left == iwads->begin() )
			{
				left = --iwads->end();
			}
			else
			{
				--left;
			}
		}

		void CycleRight()
		{
			left = middle;
			middle = right;
			if( ++right == iwads->end() )
			{
				right = iwads->begin();
			}
		}

		void Render()
		{
			ImVec2 basecursor;
			ImVec2 contentregion;
			igGetCursorPos( &basecursor );
			igGetContentRegionAvail( &contentregion );

			ImVec2 arrowsize = { 20, 140 };

			ImVec2 nextcursor = basecursor;
			nextcursor.y += arrowsize.y * 0.5f;
			igSetCursorPos( nextcursor );
			if( igArrowButtonEx( "iwad_left", ImGuiDir_Left, arrowsize, ImGuiButtonFlags_None ) )
			{
				CycleLeft();
			}

			nextcursor.x += contentregion.x - arrowsize.x;
			igSetCursorPos( nextcursor );
			if( igArrowButtonEx( "iwad_right", ImGuiDir_Right, arrowsize, ImGuiButtonFlags_None ) )
			{
				CycleRight();
			}

			nextcursor = basecursor;
			nextcursor.x += arrowsize.x;
			igSetCursorPos( nextcursor );

			ImVec2 framesize = { contentregion.x - arrowsize.x * 2, 280 };

			if( igBeginChildFrame( 667, framesize, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground ) )
			{
				constexpr ImVec2 tl = { 0, 0 };
				constexpr ImVec2 br = { 1, 1 };
				constexpr ImVec4 border = { 0, 0, 0, 0 };
				constexpr ImVec4 focustint = { 1, 1, 1, 1 };
				constexpr ImVec4 nofocustint = { 0.6, 0.6, 0.6, 0.5 };
				constexpr ImVec2 left_tl = { 0.5, 0 };
				constexpr ImVec2 right_br = { 0.5, 1 };
				constexpr float_t nofocusscale = 0.4;

				ImVec2 framecursor;

				igGetCursorPos( &framecursor );

				{
					nextcursor = framecursor;
					ImVec2 imgsize = { left->titlepic.width * nofocusscale, left->titlepic.height * 1.2f * nofocusscale };
					nextcursor.y += ( framesize.y - imgsize.y ) * 0.5;
					igSetCursorPos( nextcursor );
					igImage( I_TextureGetHandle( left->titlepic.tex ), imgsize, left_tl, br, nofocustint, border );
				}

				{
					nextcursor = framecursor;
					ImVec2 imgsize = { right->titlepic.width * nofocusscale, right->titlepic.height * 1.2f * nofocusscale };
					nextcursor.x += framesize.x - imgsize.x;
					nextcursor.y += ( framesize.y - imgsize.y ) * 0.5;
					igSetCursorPos( nextcursor );
					igImage( I_TextureGetHandle( right->titlepic.tex ), imgsize, tl, right_br, nofocustint, border );
				}

				igSetCursorPos( framecursor );
				if( middle->titlepic.tex )
				{
					igCentreNextElement( middle->titlepic.width );

					ImVec2 size = { (float_t)middle->titlepic.width, middle->titlepic.height * 1.2f };
					igImage( I_TextureGetHandle( middle->titlepic.tex ), size, tl, br, focustint, border );

					igCentreText( middle->filename.c_str() );
					igCentreText( "%s, %s", GameModes[ middle->game_mode ], GameVariants[ middle->game_variant ] );
				}
				else
				{
					igText( middle->full_path.c_str() );
				}
			}
			igEndChildFrame();
		}
	private:
		WADEntries*				iwads;
		WADEntries::iterator	left;
		WADEntries::iterator	middle;
		WADEntries::iterator	right;
	};

	class LoadingIcon
	{
	public:
		void Render() { }

	private:
		Image*					spinner;
	};
}

DoomString M_DashboardLauncherWindow()
{
	DoomString parameters;

#if USE_IMGUI
	M_DashboardApplyTheme();

	dashboardactive = Dash_Launcher;
	I_UpdateMouseGrab();
	SDL_Renderer* renderer = I_GetRenderer();

	launcher::WADList launcher;
	launcher::IWADSelector iwadselector;
	bool setupselector = false;

	bool readytolaunch = false;
	while( !readytolaunch )
	{
		I_StartTic();
		I_StartFrame();
		while( event_t* ev = D_PopEvent() )
		{
			if( ev->type == ev_quit )
			{
				I_Quit();
			}
			M_DashboardResponder( ev );
		}

		if( !setupselector && launcher.ListsReady() )
		{
			iwadselector.Setup( launcher.IWADs );
			setupselector = true;
		}

		SDL_RenderClear( renderer );

		M_DashboardPrepareRender();

		constexpr ImVec2 zero = { 0, 0 };
		constexpr int32_t windowflags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;

		M_DashboardApplyTheme();

		ImVec2 windowsize = { M_MIN( igGetIO()->DisplaySize.x, 750.f ), M_MIN( igGetIO()->DisplaySize.y, 580.f ) };
		ImVec2 windowpos = { ( igGetIO()->DisplaySize.x - windowsize.x ) * 0.5f, ( igGetIO()->DisplaySize.y - windowsize.y ) * 0.5f };
		igSetNextWindowSize( windowsize, ImGuiCond_Always );
		igSetNextWindowPos( windowpos, ImGuiCond_Always, zero );
		if( igBegin( "Launcher", NULL, windowflags ) )
		{
			if( launcher.ListsReady() )
			{
				iwadselector.Render();
			}
			if( igButton( "Cool, just play", ImVec2() ) )
			{
				readytolaunch = true;
			}
		}
		igEnd();

		M_DashboardFinaliseRender();

		SDL_RenderPresent( renderer );
	}

	// Clear out the window before we move on
	SDL_RenderClear( renderer );
	SDL_RenderPresent( renderer );

	dashboardactive = Dash_Inactive;
#endif // USE_IMGUI

	return parameters;
}
