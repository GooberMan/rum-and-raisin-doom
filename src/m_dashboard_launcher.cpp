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

	class WADList
	{
		using WADEntries = std::vector< WADEntry >;

	public:
		WADList()
			: threaded_context( nullptr )
			, lists_ready( false )
		{
			KickoffPopulate();
		}

		~WADList()
		{
			if( worker_thread->joinable() )
			{
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

			lists_ready = true;
		}

		INLINE bool ListsReady() { return lists_ready.load(); }

		WADEntries				IWADs;
		WADEntries				PWADs;
		SDL_GLContext			threaded_context;
		std::atomic< bool >		lists_ready;
		std::thread*			worker_thread;
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

		SDL_RenderClear( renderer );

		M_DashboardPrepareRender();
		if( igButton( "Cool, just play", ImVec2() ) )
		{
			readytolaunch = true;
		}
		if( launcher.ListsReady() )
		{
			for( auto& iwad : launcher.IWADs )
			{
				if( iwad.titlepic.tex )
				{
					constexpr ImVec2 tl = { 0, 0 };
					constexpr ImVec2 br = { 1, 1 };
					constexpr ImVec4 tint = { 1, 1, 1, 1 };
					constexpr ImVec4 border = { 0, 0, 0, 0 };
					ImVec2 size = { iwad.titlepic.width, iwad.titlepic.height * 1.2f };
					igImage( I_TextureGetHandle( iwad.titlepic.tex ), size, tl, br, tint, border );
					igText( iwad.filename.c_str() );
					igText( "%s, %s", GameModes[ iwad.game_mode ], GameVariants[ iwad.game_variant ] );
					igNewLine();
				}
				else
				{
					igText( iwad.full_path.c_str() );
				}
			}
			igText( "PWADS:" );
			for( auto& pwad : launcher.PWADs )
			{
				igText( pwad.full_path.c_str() );
			}
		}

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
