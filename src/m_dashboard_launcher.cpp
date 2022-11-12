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
#include "i_thread.h"
#include "i_timer.h"
#include "i_video.h"

#include "d_iwad.h"
#include "w_wad.h"

#include "m_argv.h"
#include "m_config.h"
#include "m_container.h"
#include "m_json.h"
#include "m_misc.h"
#include "m_url.h"

#include "v_patch.h"
#include "v_video.h"

#include "glad/glad.h"
#include "SDL_opengl.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <chrono>
#include <ranges>
#include <regex>
#include <stack>
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

constexpr const char* idgames_api_url = "https://www.doomworld.com/idgames/api/api.php";

constexpr ImVec2 zero = { 0, 0 };

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

	constexpr int32_t WADEntryVersion = 1;

	PACKED_STRUCT( TGAHeader
	{
		uint8_t			id_length;
		uint8_t			colour_map_type;
		uint8_t			image_type;
		uint16_t		colour_map_first;
		uint16_t		colour_map_count;
		uint8_t			colour_map_entry_size;
		uint16_t		x_origin;
		uint16_t		y_origin;
		uint16_t		width;
		uint16_t		height;
		uint8_t			pixel_depth;
		uint8_t			image_specification;
	});

	struct WADEntry
	{
		int32_t			version;
		WADType			type;
		std::string		filename;
		std::string		full_path;
		std::string		cache_path;
		std::string		text_file;
		uint64_t		file_length;
		time_t			last_modified;
		GameMode_t		game_mode;
		GameVariant_t	game_variant;

		Image			titlepic;
		std::vector< bgra_t > titlepic_raw;
	};

	void WriteEntry( WADEntry& entry, const std::filesystem::path& stdpath )
	{
		std::string path = stdpath.string();
		std::filesystem::create_directories( path );

		std::filesystem::path infopath = path + "info.json";

		JSONElement root = JSONElement::MakeRoot();
		root.AddNumber( "version", entry.version );
		root.AddNumber( "type", (int32_t)entry.type );
		root.AddNumber( "game_mode", (int32_t)entry.game_mode );
		root.AddNumber( "game_variant", (int32_t)entry.game_variant );

		std::string converted = root.Serialise();
		FILE* infofile = fopen( infopath.string().c_str(), "wb" );
		fwrite( converted.c_str(), sizeof( char ), converted.size(), infofile );
		fclose( infofile );

		if( !entry.text_file.empty() )
		{
			std::filesystem::path textpath = path + "textfile.txt";
			FILE* textfile = fopen( textpath.string().c_str(), "wb" );
			fwrite( entry.text_file.c_str(), sizeof( char ), entry.text_file.size(), textfile );
			fclose( infofile );
		}

		if( !entry.titlepic_raw.empty() )
		{
			std::filesystem::path titlepicpath = path + "titlepic.tga";

			TGAHeader header;
			header.id_length = 0;
			header.colour_map_type = 0;
			header.image_type = 2;
			header.colour_map_first = 0;
			header.colour_map_count = 0;
			header.colour_map_entry_size = 0;
			header.x_origin = 0;
			header.y_origin = 0;
			header.width = entry.titlepic.width;
			header.height = entry.titlepic.height;
			header.pixel_depth = 32;
			header.image_specification = 8;

			FILE* tgafile = fopen( titlepicpath.string().c_str(), "wb" );
			fwrite( &header, sizeof( TGAHeader ), 1, tgafile );
			fwrite( entry.titlepic_raw.data(), sizeof( bgra_t ), entry.titlepic_raw.size(), tgafile );
			fclose( tgafile );
		}
	}

	void ReadEntry( WADEntry& entry, const std::filesystem::path& stdpath )
	{
		std::string path = stdpath.string();

		std::string infopath = path + "info.json";
		std::string infojson;
		infojson.resize( std::filesystem::file_size( std::filesystem::path( infopath ) ) );
		FILE* infofile = fopen( infopath.c_str(), "rb" );
		fread( (void*)infojson.data(), sizeof( char ), infojson.length(), infofile );
		fclose( infofile );

		JSONElement root = JSONElement::Deserialise( infojson );
		entry.version = to< int32_t >( root[ "version" ] );
		entry.type = (WADType)to< int32_t >( root[ "type" ] );
		entry.game_mode = (GameMode_t)to< int32_t >( root[ "game_mode" ] );
		entry.game_variant = (GameVariant_t)to< int32_t >( root[ "game_variant" ] );

		std::string textpath = path + "textfile.txt";
		std::filesystem::path textstdpath = std::filesystem::path( textpath );
		if( std::filesystem::exists( textstdpath ) )
		{
			entry.text_file.resize( std::filesystem::file_size( textstdpath ) );
			FILE* textfile = fopen( textpath.c_str(), "rb" );
			fread( (void*)entry.text_file.data(), sizeof( char ), entry.text_file.length(), textfile );
			fclose( textfile );
		}

		std::string titlepicpath = path + "titlepic.tga";
		if( std::filesystem::exists( std::filesystem::path( titlepicpath ) ) )
		{
			TGAHeader header;
			FILE* tgafile = fopen( titlepicpath.c_str(), "rb" );
			fread( (void*)&header, sizeof( TGAHeader ), 1, tgafile );
			entry.titlepic.width = header.width;
			entry.titlepic.height = header.height;
			entry.titlepic_raw.resize( header.width * header.height );
			fread( entry.titlepic_raw.data(), sizeof( bgra_t ), header.width * header.height, tgafile );
			fclose( tgafile );
		}
	}

	bool WADEntryExists( const std::filesystem::path& stdpath )
	{
		if( std::filesystem::exists( stdpath ) )
		{
			return std::filesystem::exists( std::filesystem::path( stdpath.string() + "info.json" ) );
		}
		return false;
	}

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

		WADEntry entry = WADEntry();
		entry.version = WADEntryVersion;
		entry.filename = filename;
		entry.full_path = path;
		entry.cache_path = cachepath;
		entry.last_modified = lastmodified;
		entry.file_length = filelength;

		if( !M_CheckParm( "-invalidatewadcache" ) && WADEntryExists( stdcachepath ) )
		{
			ReadEntry( entry, stdcachepath );
		}
		else
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

						entry.titlepic_raw.resize( patch->width * patch->height );
						entry.titlepic.width = patch->width;
						entry.titlepic.height = patch->height;

						for( int32_t x = 0; x < patch->width; ++x )
						{
							pixel_t* source = composite.data() + x * patch->height;
							bgra_t* dest = entry.titlepic_raw.data() + x;
							for( int32_t y = 0; y < patch->height; ++y )
							{
								rgb_t& entry = pallette[ *source ];
								dest->b = entry.b;
								dest->g = entry.g;
								dest->r = entry.r;
								dest->a = 0xFF;
								++source;
								dest += patch->width;
							}
						}
					}
				}

				fclose( file );
			}

			std::filesystem::path textpath = stdpath;
			textpath.replace_extension( "txt" );
			if( std::filesystem::exists( textpath ) )
			{
				std::ifstream file( textpath, std::ios::in | std::ios::binary );
				entry.text_file = std::string( std::istreambuf_iterator< char >{ file }, { } );

				size_t foundpos = 0;
				while( ( foundpos = entry.text_file.find( "%", foundpos ) ) != std::string::npos )
				{
					entry.text_file.replace( foundpos, 1, "%%" );
					foundpos += 2;
				}
			}

			WriteEntry( entry, stdcachepath );
		}

		if( !entry.titlepic_raw.empty() )
		{
			entry.titlepic.tex = I_TextureCreate( entry.titlepic.width, entry.titlepic.height, Format_BGRA8, entry.titlepic_raw.data() );
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
			, total_files( 0 )
			, parsed_files( 0 )
		{
			KickoffPopulate();
		}

		~WADList()
		{
		}

		void KickoffPopulate()
		{
			SDL_Window* window = I_GetWindow();
			SDL_GLContext glcontext = SDL_GL_GetCurrentContext();
			SDL_GL_SetAttribute( SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1 );
			threaded_context = SDL_GL_CreateContext( window );
			SDL_GL_MakeCurrent( window, glcontext );
		}

		void PopulateWADList()
		{
			std::string contents;
			bool successful = M_URLGetString( contents, idgames_api_url, "action=getcontents&name=levels/doom/&out=json" );
			JSONElement converted = JSONElement::Deserialise( contents );

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
				total_files += std::distance( dir, std::filesystem::recursive_directory_iterator{} );
			}

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

					++parsed_files;
				}
			}

			glFlush();
			SDL_GL_DeleteContext( threaded_context );

			lists_ready = !abort_work.load();
		}

		INLINE bool ListsReady() const { return lists_ready.load(); }
		INLINE double_t Progress() const
		{
			size_t total = total_files.load();
			if( total > 0 )
			{
				return parsed_files.load() / (double_t)total;
			}
			return 0;
		}

		WADEntries				IWADs;
		WADEntries				PWADs;
		SDL_GLContext			threaded_context;
		std::atomic< bool >		lists_ready;
		std::atomic< bool >		abort_work;

		std::atomic< size_t >	total_files;
		std::atomic< size_t >	parsed_files;
	};

	class IWADSelector
	{
	public:
		IWADSelector( )
			: iwads( nullptr )
		{
		}

		void Setup( WADEntries& entries, const char* defaultiwad )
		{
			iwads = &entries;
			if( !iwads->empty() )
			{
				if( defaultiwad )
				{
					std::filesystem::path iwadpath( defaultiwad );
					std::string iwadfilename = iwadpath.filename().string();

					middle = std::find_if( iwads->begin(), iwads->end(), [ iwadfilename ]( WADEntry& e )
					{
						return std::equal( iwadfilename.begin(), iwadfilename.end()
											, e.filename.begin(), e.filename.end()
											, []( char lhs, char rhs )
											{
												return tolower( lhs ) == tolower( rhs );
											} );
					} );

					if( middle == iwads->end() )
					{
						middle = iwads->begin();
					}
				}
				else
				{
					middle = iwads->begin();
				}

				left = middle;
				if( left == iwads->begin() )
				{
					left = --iwads->end();
				}
				else
				{
					--left;
				}

				right = middle;
				if( ++right == iwads->end() )
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
		INLINE WADEntry& Selected() const { return *middle; };

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

			constexpr float_t frameheight = 260.0f;

			ImVec2 framesize = { contentregion.x - arrowsize.x * 2, frameheight };

			igPushStyleVarVec2( ImGuiStyleVar_FramePadding, zero );

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

					//igCentreText( middle->filename.c_str() );
					igCentreText( "%s, %s", GameModes[ middle->game_mode ], GameVariants[ middle->game_variant ] );
				}
				else
				{
					igText( middle->full_path.c_str() );
				}
			}
			igEndChildFrame();

			igPopStyleVar( 1 );
		}
	private:
		WADEntries*				iwads;
		WADEntries::iterator	left;
		WADEntries::iterator	middle;
		WADEntries::iterator	right;
	};

	class PWADSelector
	{
	public:
		PWADSelector( )
			: pwads( nullptr )
			, current( -1 )
		{
		}

		void Setup( WADEntries& entries )
		{
			pwads = &entries;
		}

		INLINE bool Valid() const { return pwads != nullptr; }

		void Render()
		{
			ImVec2 framesize;
			igGetContentRegionAvail( &framesize );
			framesize.y -= 30;

			igPushStyleVarVec2( ImGuiStyleVar_FramePadding, zero );

			if( igBeginChildFrame( 669, framesize, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground ) )
			{
				igColumns( 2, "PWAD_Selector", false );
				igSetColumnWidth( 0, 150.0f );
				igSetNextItemWidth( 145.0f );

				auto getter = []( void* data, int idx, const char** out_text )
				{
					WADEntries* entries = (WADEntries*)data;
					if( idx < entries->size() )
					{
						WADEntry& entry = (*entries)[ idx ];
						*out_text = entry.filename.c_str();
						return true;
					}
					return false;
				};

				igPushIDPtr( this );
				igListBoxFnBoolPtr( "", &current, getter, (void*)pwads, pwads->size(), 14 );
				igPopID();

				igNextColumn();

				igGetContentRegionAvail( &framesize );
				if( igBeginChildFrame( 669, framesize, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar ) )
				{
					if( current >= 0 )
					{
						igText( (*pwads)[ current ].text_file.c_str() );
					}
				}
				igEndChildFrame();

				igColumns( 1, nullptr, false );
			}
			igEndChildFrame();

			igPopStyleVar( 1 );
		}

	private:
		WADEntries*				pwads;
		int32_t					current;
	};

	class LauncherPanel;

	static std::stack< std::shared_ptr< LauncherPanel > >	panel_stack;
	static JobThread* jobs = nullptr;

	// Probably not worth duck typing this thing, so ye olde OO vtables it is
	class LauncherPanel
	{
	public:
		LauncherPanel() = delete;

		virtual void Enter() { };
		virtual void Exit() { };
		virtual void Update() { };

		void Render()
		{
			ImVec2 windowpos = { ( igGetIO()->DisplaySize.x - windowsize.x ) * 0.5f, ( igGetIO()->DisplaySize.y - windowsize.y ) * 0.5f };
			igSetNextWindowSize( windowsize, ImGuiCond_Always );
			igSetNextWindowPos( windowpos, ImGuiCond_Always, zero );
		
			if( igBegin( name.c_str(), NULL, windowflags ) )
			{
				RenderContents();
			}
			igEnd();
		}

	protected:
		constexpr static int32_t default_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;
		constexpr static ImVec2 default_size = { 750.f, 595.f };

		LauncherPanel( const char* title, ImVec2 size = default_size, int32_t flags = default_flags )
			: name( title )
			, windowsize( size )
			, windowflags( flags )
		{
		}

		virtual void RenderContents() = 0;

	private:
		std::string				name;
		ImVec2					windowsize;
		int32_t					windowflags;
	};

	class MainPanel : public LauncherPanel
	{
	public:
		MainPanel( std::shared_ptr< IWADSelector >& iwad, std::shared_ptr< PWADSelector >& pwad )
			: LauncherPanel( "Launcher_Main" )
			, readytolaunch( false )
			, iwadselector( iwad )
			, pwadselector( pwad )
		{
		}

	protected:
		virtual void RenderContents() override
		{
			iwadselector->Render();
			pwadselector->Render();

			constexpr ImVec2 playbuttonsize = { 50.f, 25.f };
			constexpr float_t framepadding = 10;

			ImVec2 framesize;
			igGetContentRegionMax( &framesize );

			ImVec2 cursor = { framesize.x - playbuttonsize.x - framepadding, framesize.y - playbuttonsize.y };
			igSetCursorPos( cursor );

			if( igButton( "Play!", zero ) )
			{
				readytolaunch = true;
				panel_stack.pop();
			}
		}

	private:
		bool								readytolaunch;
		std::shared_ptr< IWADSelector >		iwadselector;
		std::shared_ptr< PWADSelector >		pwadselector;
	};

	class InitPanel : public LauncherPanel
	{
		constexpr static ImVec2 init_size		= { 140.f, 200.f };
		constexpr static ImVec2 loading_size	= { init_size.x * 0.8, init_size.x * 0.35 };
		constexpr static int32_t init_flags		= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
		constexpr static double_t cycle_time	= 0.5;

	public:
		InitPanel()
			: LauncherPanel( "Launcher", init_size, init_flags )
			, hasrunlauncher( false )
			, launcherfinished( false )
		{
			iwadselector = std::make_shared< IWADSelector >();
			pwadselector = std::make_shared< PWADSelector >();

			mainpanel = std::make_shared< MainPanel >( iwadselector, pwadselector );
		}

		virtual void Enter() override
		{
			if( !hasrunlauncher )
			{
				jobs->AddJob( [ this ]()
				{
					launcher.PopulateWADList();
					const char* defaultiwad = nullptr;
					int32_t iwadparam = M_CheckParmWithArgs( "-iwad", 1 );
					if( iwadparam > 0 )
					{
						defaultiwad = myargv[ iwadparam + 1 ];
					}
					iwadselector->Setup( launcher.IWADs, defaultiwad );
					pwadselector->Setup( launcher.PWADs );

					launcherfinished = true;
				} );
			}
		}

		virtual void Update() override
		{
			if( launcherfinished )
			{
				panel_stack.pop();
				panel_stack.push( mainpanel );
			}
		}

		INLINE IWADSelector* GetIWADSelector() { return iwadselector.get(); }

	protected:
		virtual void RenderContents() override
		{
			constexpr double_t pi2 = M_PI * 2.0;

			igCentreNextElement( loading_size.x );
			igSpinner( loading_size, cycle_time );
			igRoundProgressBar( launcher.Progress(), 15.f, 2.f );
			igCentreText( "Updating WAD" );
			igCentreText( "dictionary" );
		}

	private:
		bool								hasrunlauncher;
		std::atomic< bool >					launcherfinished;
		launcher::WADList					launcher;
		std::shared_ptr< IWADSelector >		iwadselector;
		std::shared_ptr< PWADSelector >		pwadselector;
		std::shared_ptr< MainPanel >		mainpanel;
	};
}

std::string M_DashboardLauncherWindow()
{
	std::string parameters;

#if USE_IMGUI
	M_DashboardApplyTheme();

	dashboardactive = Dash_Launcher;
	I_UpdateMouseGrab();
	SDL_Renderer* renderer = I_GetRenderer();

	bool readytolaunch = false;

	launcher::jobs = new JobThread;
	launcher::jobs->AddJob( []() { M_URLInit(); } );

	std::shared_ptr< launcher::InitPanel > initpanel = std::make_shared< launcher::InitPanel >();
	initpanel->Enter();

	launcher::panel_stack.push( initpanel );

	while( !launcher::panel_stack.empty() )
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
		M_DashboardApplyTheme();

		launcher::panel_stack.top()->Update();
		launcher::panel_stack.top()->Render();

		M_DashboardFinaliseRender();

		SDL_RenderPresent( renderer );
	}

	// Clear out the window before we move on
	SDL_RenderClear( renderer );
	SDL_RenderPresent( renderer );

	dashboardactive = Dash_Inactive;

	if( initpanel->GetIWADSelector()->Valid() )
	{
		parameters += "-iwad \"" + initpanel->GetIWADSelector()->Selected().full_path + "\"";
	}
#endif // USE_IMGUI

	return parameters;
}
