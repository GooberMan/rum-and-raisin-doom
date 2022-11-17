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
#include "m_zipfile.h"

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

#ifdef WIN32
#include <sys/utime.h>
#define utime _utime
#define utimbuf _utimbuf
#else
#include <utime.h>
#endif // WIN32


extern "C"
{
	extern int32_t dashboardactive;

	extern int32_t frame_width;
	extern int32_t frame_adjusted_width;
	extern int32_t frame_height;

	extern uint8_t defaultpalette[];
}

extern ImFont* font_inconsolata;
extern ImFont* font_inconsolata_medium;
extern ImFont* font_inconsolata_large;

void M_DashboardApplyTheme();
void M_DashboardPrepareRender();
void M_DashboardFinaliseRender();

constexpr ImVec2 zero = { 0, 0 };

constexpr const char* GameMissions[] =
{
	"Doom",
	"Doom II",
	"TNT: Evilution",
	"The Plutonia Experiment",
	"Chex Quest",
	"HacX",
};

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

constexpr const char* GameVersions[] =
{
	"Doom 1.2",
	"Doom 1.666",
	"Doom 1.7/1.7a",
	"Doom 1.8",
	"Doom 1.9",
	"Hacx",
	"Ultimate Doom",
	"Final Doom",
	"Final Doom (alt)",
};

constexpr const char* GameVersionsCommand[] =
{
	"1.2",
	"1.666",
	"1.7",
	"1.8",
	"1.9",
	"hacx",
	"ultimate",
	"final",
	"final2",
};

#ifdef WIN32
#define HOME_PATH ( std::string( std::getenv( "USERPROFILE" ) ) + "\\Saved Games\\Rum and Raisin Doom\\" )
#else
#define HOME_PATH ( std::string( std::getenv( "HOME" ) ) + "/.local/share/rum-and-raisin-doom/" )
#endif

constexpr const char* idgames_api_url		= "https://www.doomworld.com/idgames/api/api.php";
constexpr const char* idgames_api_folder	= "action=getcontents&out=json&name=";
constexpr const char* idgames_api_file		= "action=get&out=json&file=";

constexpr const char* cache_local			= "cache" DIR_SEPARATOR_S "local" DIR_SEPARATOR_S;
constexpr const char* cache_download		= "cache" DIR_SEPARATOR_S "download" DIR_SEPARATOR_S;
constexpr const char* cache_extracted		= "cache" DIR_SEPARATOR_S "extracted" DIR_SEPARATOR_S;

constexpr const char* WADRegexString = ".+\\.wad$";
constexpr const char* DEHRegexString = ".+\\.deh$";
static const std::regex WADRegex = std::regex( WADRegexString, std::regex_constants::icase );
static const std::regex DEHRegex = std::regex( DEHRegexString, std::regex_constants::icase );

typedef struct idgamesmirror_s
{
	const char* url;
	const char* location;
} idgamesmirror_t;

constexpr idgamesmirror_t MakeMirror( const char* url, const char* location )
{
	idgamesmirror_t mirror = { url, location };
	return mirror;
};

constexpr idgamesmirror_t idgames_mirrors[] =
{
	MakeMirror( "ftp://mirrors.syringanetworks.net/idgames/", "Idaho, US (FTP)" ),
	MakeMirror( "ftp://ftp.fu-berlin.de/pc/games/idgames/", "Berlin, DE (FTP)" ),
	MakeMirror( "https://www.quaddicted.com/files/idgames/", "Falkenstein, DE" ),
	MakeMirror( "https://ftpmirror1.infania.net/pub/idgames/", "Varberg, SE" ),
	MakeMirror( "https://youfailit.net/pub/idgames/", "New York, US" ),
	MakeMirror( "https://www.gamers.org/pub/idgames/", "Virginia, US" ),
};

constexpr const char* shareware_idgames_url		= "idstuff/doom/doom19s.zip";
constexpr const char* freedoom_url				= "https://www.github.com/freedoom/freedoom/releases/download/v0.12.1/freedoom-0.12.1.zip";
constexpr const char* hacx_idgames_url			= "themes/hacx/hacx12.zip";

constexpr auto Mirrors() { return std::span( idgames_mirrors, arrlen( idgames_mirrors ) ); }

void OpenIdgamesLocationSelector( const char* id )
{
	igOpenPopup( id, ImGuiPopupFlags_None );
}

const char* IdgamesLocationSelector( const char* id )
{
	const char* output = nullptr;

	if( igBeginPopup( id, ImGuiWindowFlags_None ) )
	{
		igText( "Choose location" );
		igSeparator();

		for( auto& loc : Mirrors() )
		{
			if( igSelectableBool( loc.location, false, ImGuiSelectableFlags_None, zero ) )
			{
				output = loc.url;
				igCloseCurrentPopup();
			}
		}

		igEndPopup();
	}

	return output;
}

bool EqualCaseInsensitive( const std::string& lhs, const std::string& rhs )
{
	return std::equal( lhs.begin(), lhs.end()
						, rhs.begin(), rhs.end()
						, []( const char l, const char r )
						{
							return tolower( l ) == tolower( r );
						} );
}

std::span< const char* > ParamArgs( const char* param )
{
	int32_t fileparam = M_CheckParm( param );
	if( fileparam > 0 )
	{
		++fileparam;
		int32_t endparam = fileparam;
		while( endparam < myargc && myargv[ endparam ][ 0 ] != '-' )
		{
			++endparam;
		}
		return std::span( &myargv[ fileparam ], (size_t)( endparam - fileparam ) );
	}
	return std::span( &myargv[ 0 ], 0 );
};

template< typename _range, typename _func >
auto Transform( _range& r, _func&& f )
{
	using func_type = decltype( std::function( f ) );
	using ret_type = decltype( f( *r.begin() ) );

	class trange
	{
	public:
		using value_type = ret_type;

		using range_iterator = typename _range::iterator;

		class iterator
		{
		public:
			iterator()
				: f( nullptr )
			{
			}

			iterator( range_iterator _i, func_type& _f )
				: it( _i )
				, f( &_f )
			{
			}

			iterator& operator++()
			{
				++it;
				return *this;
			}

			ret_type operator*()
			{
				return (*f)( *it );
			}

			bool operator!=( const iterator& rhs )
			{
				return it != rhs.it;
			}

		private:
			range_iterator it;
			func_type* f;
		};

		trange( _range& _r, func_type&& _f )
			: r( _r )
			, f( _f )
		{
		}

		INLINE iterator begin() { return iterator( r.begin(), f ); }
		INLINE iterator end()	{ return iterator( r.end(), f ); }

		INLINE size_t size() { return r.size(); }
		INLINE bool empty() { return r.empty(); }

		INLINE ret_type operator[]( size_t index )
		{
			iterator it = begin();
			for( int32_t v = 0; v < index; ++v )
			{
				++it;
			}
			return *it;
		}

	private:
		_range& r;
		func_type f;
	};

	return trange( r, std::function( f ) );
}

template< typename _range, typename _func >
auto Filter( _range& r, _func&& f )
{
	using func_type = decltype( std::function( f ) );

	class frange
	{
	public:
		using value_type = typename _range::value_type;

		using range_iterator = typename _range::iterator;

		class iterator
		{
		public:
			iterator()
				: f( nullptr )
			{
			}

			iterator( range_iterator _b, range_iterator _e, func_type& _f )
				: b( _b )
				, e( _e )
				, f( &_f )
			{
				if( b != e && !(*f)( *b ) )
				{
					this->operator++();
				}
			}

			iterator& operator++()
			{
				while( b != e )
				{
					if( ++b != e && (*f)( *b ) )
					{
						break;
					}
				}
				return *this;
			}

			value_type& operator*()
			{
				return *b;
			}

			bool operator!=( const iterator& rhs )
			{
				return b != rhs.b;
			}

		private:
			range_iterator b;
			range_iterator e;
			func_type* f;
		};

		frange( _range& _r, func_type&& _f )
			: r( _r )
			, f( _f )
		{
		}

		INLINE iterator begin() { return iterator( r.begin(), r.end(), f ); }
		INLINE iterator end()	{ return iterator( r.end(), r.end(), f ); }

		INLINE size_t size() { return r.size(); }
		INLINE bool empty() { return r.empty(); }

	private:
		_range& r;
		func_type f;
	};

	return frange( r, std::function( f ) );
}

typedef struct vec2_s
{
	float_t		x;
	float_t		y;
} vec2_t;

typedef struct triangle_s
{
	vec2_t		p1;
	vec2_t		p2;
	vec2_t		p3;
} triangle_t;

constexpr vec2_t MakeVec2( float_t x, float_t y )
{
	vec2_t out = { x, y };
	return out;
}

constexpr triangle_t MakeTriangle( const vec2_t& p1, const vec2_t& p2, const vec2_t& p3 )
{
	triangle_t out = { p1, p2, p3 };
	return out;
}

// Icons from Google converted to triangle meshes

// https://materialdesignicons.com/icon/star
constexpr triangle_t mesh_star[] =
{
	MakeTriangle( MakeVec2( 0.0,0.4333333333 ),				MakeVec2( -0.5166666667,0.7416666667 ),		MakeVec2( -0.3833333333,0.1583333333 ) ),
	MakeTriangle( MakeVec2( -0.3833333333,0.1583333333 ),	MakeVec2( -0.8333333333,-0.2333333333 ),	MakeVec2( -0.2416666667,-0.2833333333 ) ),
	MakeTriangle( MakeVec2( -0.2416666667,-0.2833333333 ),	MakeVec2( 0.0,-0.8333333333 ),				MakeVec2( 0.2333333333,-0.2833333333 ) ),
	MakeTriangle( MakeVec2( 0.2333333333,-0.2833333333 ),	MakeVec2( 0.825,-0.2333333333 ),			MakeVec2( 0.375,0.1583333333 ) ),
	MakeTriangle( MakeVec2( 0.375,0.1583333333 ),			MakeVec2( 0.5083333333,0.7416666667 ),		MakeVec2( 0.0,0.4333333333 ) ),
	MakeTriangle( MakeVec2( 0.0,0.4333333333 ),				MakeVec2( -0.3833333333,0.1583333333 ),		MakeVec2( -0.2416666667,-0.2833333333 ) ),
	MakeTriangle( MakeVec2( -0.2416666667,-0.2833333333 ),	MakeVec2( 0.2333333333,-0.2833333333 ),		MakeVec2( 0.375,0.1583333333 ) ),
	MakeTriangle( MakeVec2( 0.375,0.1583333333 ),			MakeVec2( 0.0,0.4333333333 ),				MakeVec2( -0.2416666667,-0.2833333333 ) )
};

// https://materialdesignicons.com/icon/download

constexpr auto Star() { return std::span( mesh_star, arrlen( mesh_star ) ); }

void igCentreText( const char* string )
{
	ImVec2 textsize;
	igCalcTextSize( &textsize, string, nullptr, false, -1 );
	igCentreNextElement( textsize.x );
	igText( string );
}

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

template< typename _container, typename _func >
void igTileView( _container& items, ImVec2 tilesize, _func& peritem )
{
	ImVec2 region;
	igGetContentRegionAvail( &region );
	int32_t columns = M_MAX( 1, region.x / tilesize.x );

	igColumns( columns, "##tileview", false );
	for( int32_t currcol : iota( 0, columns - 1 ) )
	{
		igSetColumnWidth( currcol, tilesize.x + 3.f );
	}

	int32_t id = 2000;
	for( auto& val : items )
	{
		if( igBeginChildEx( "##tileitem", id++, tilesize, false, ImGuiWindowFlags_None ) )
		{
			peritem( val, tilesize );
		}
		igEndChild();
		igNextColumn();
	}

	igColumns( 1, nullptr, false );
}

template< typename _container, typename _func >
void igTileView( _container& items, ImVec2 tilesize, _func&& peritem )
{
	igTileView( items, tilesize, peritem );
}

template< typename _rangeview, typename _func >
void igTileView( _rangeview&& items, ImVec2 tilesize, _func&& peritem )
{
	igTileView( items, tilesize, peritem );
}


// Copypasta of the imgui button code, but with our own custom framing
template< typename _func >
bool igButtonCustomContents( const char* textid, const ImVec2& size, ImGuiButtonFlags flags, _func&& contents )
{
	ImGuiWindow* window = igGetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext* g = igGetCurrentContext();
	const ImGuiStyle* style = &g->Style;
	const ImGuiID id = ImGuiWindow_GetIDStr( window, textid, nullptr );

	ImVec2 pos = window->DC.CursorPos;
	if( (flags & ImGuiButtonFlags_AlignTextBaseLine) && style->FramePadding.y < window->DC.CurrLineTextBaseOffset )
	{
		pos.y += window->DC.CurrLineTextBaseOffset - style->FramePadding.y;
	}

	ImRect bb;
	bb.Min = pos;
	bb.Max = pos;
	bb.Max.x += size.x;
	bb.Max.y += size.y;

	igItemSizeVec2( size, style->FramePadding.y );

	if( !igItemAdd( bb, id, 0 ) )
	{
		return false;
	}

	if( window->DC.ItemFlags & ImGuiItemFlags_ButtonRepeat )
	{
		flags |= ImGuiButtonFlags_Repeat;
	}
	bool hovered = false;
	bool held = false;
	bool pressed = igButtonBehavior( bb, id, &hovered, &held, flags );

	// Render
	const ImU32 col = igGetColorU32Col( (held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button, 1.0f );
	igRenderNavHighlight( bb, id, ImGuiNavHighlightFlags_TypeDefault );
	igRenderFrame( bb.Min, bb.Max, col, true, style->FrameRounding );

	ImVec2 nextcursor = window->DC.CursorPos;

	const ImVec2& padding = style->FramePadding;
	bb.Min.x += padding.x;
	bb.Min.y += padding.y;
	bb.Max.x -= padding.x;
	bb.Max.y -= padding.y;
	
	igIndent( padding.x );

	ImVec2 childsize = { bb.Max.x - bb.Min.x, bb.Max.y - bb.Min.y };
	window->DC.CursorPos = bb.Min;

	igPushClipRect( bb.Min, bb.Max, true );
	contents( childsize );
	igPopClipRect();

	igUnindent( padding.x );

	window->DC.CursorPos = nextcursor;

	return pressed;
}

void igMesh( const triangle_t* triangles, size_t count, ImVec2 size, ImU32 colour )
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

	vec2_t extents = { size.x * 0.5f, size.y * 0.5f };
	vec2_t cursor = { iconcursor.x + extents.x, iconcursor.y + extents.y };

	auto scale_translate = [ &extents, cursor ]( const vec2_t& val )
	{
		ImVec2 out = { cursor.x + val.x * extents.x, cursor.y + val.y * extents.y };
		return out;
	};


	ImDrawList* drawlist = igGetWindowDrawList();

	for( auto& tri : std::span( triangles, count ) )
	{
		ImDrawList_AddTriangleFilled( drawlist, scale_translate( tri.p1 ), scale_translate( tri.p2 ), scale_translate( tri.p3 ), colour );
	}
}

// Convert the rating to a scalar between 0-1
void igRating( const char* textid, double_t rating, int32_t totalstars, ImVec2 size, ImU32 colour_front, ImU32 colour_back )
{
	ImGuiWindow* window = igGetCurrentWindow();
	if (window->SkipItems)
		return;

	ImGuiContext* g = igGetCurrentContext();
	const ImGuiStyle* style = &g->Style;
	const ImGuiID id = ImGuiWindow_GetIDStr( window, textid, nullptr );

	ImVec2 pos;
	igGetCursorScreenPos( &pos );
	ImVec2 curr = pos;

	constexpr float_t spacing = 0;

	ImRect bb;
	bb.Min = bb.Max = pos;
	bb.Max.x += size.x * ( rating * totalstars ) + spacing * totalstars;
	bb.Max.y += size.y;

	igPushClipRect( bb.Min, bb.Max, true );
	for( int32_t currstar : iota( 0, totalstars ) )
	{
		igSetCursorScreenPos( curr );
		igMesh( mesh_star, arrlen( mesh_star ), size, colour_front );
		curr.x += size.x + spacing;
	}
	igPopClipRect();

	bb.Min = bb.Max = pos;
	bb.Min.x += size.x * ( rating * totalstars ) + spacing * totalstars;
	bb.Max.x += ( size.x + spacing ) * totalstars;
	bb.Max.y += size.y;

	curr = pos;
	igPushClipRect( bb.Min, bb.Max, true );
	for( int32_t currstar : iota( 0, totalstars ) )
	{
		igSetCursorScreenPos( curr );
		igMesh( mesh_star, arrlen( mesh_star ), size, colour_back );
		curr.x += size.x + spacing;
	}
	igPopClipRect();
}

void igDownloadProgressBar( const char* textid, ImVec2 totalsize, ImVec2 spinnersize, float_t spinnerspeed, float_t barheight, const char* currentlydoing, float_t progress )
{
	ImVec2 cursorpos;
	igGetCursorPos( &cursorpos );

	igSpinner( spinnersize, spinnerspeed );

	cursorpos.x += totalsize.y + 5.f;
	igSetCursorPos( cursorpos );
	igText( currentlydoing );

	cursorpos.y += totalsize.y - barheight;
	igSetCursorPos( cursorpos );
	ImVec2 progsize = { totalsize.x - totalsize.y - 5.f, barheight };
	igRoundProgressBar( progress, progsize, 2.f );
}

namespace launcher
{
	struct Image
	{
		hwtexture_t*			tex;
		int32_t					width;
		int32_t					height;
	};

	enum class DoomFileType
	{
		None,
		IWAD,
		PWAD,
		Dehacked,
	};

	constexpr int32_t DoomFileEntryVersion = 1;

	struct LaunchOptions
	{
		static constexpr int32_t demo_name_length = 128;

		int32_t		game_version;

		int32_t		start_episode;
		int32_t		start_map;
		int32_t		start_skill;
		bool		solo_net;

		bool		fast_monsters;
		bool		no_monsters;
		bool		respawn_monsters;
		bool		no_widepix;

		bool		record_demo;
		char		demo_name[ demo_name_length ];
	};

	constexpr LaunchOptions default_launch_options =
	{
		-1,			// game_version
		0,			// start_episode
		0,			// start_map
		-1,			// start_skill
		false,		// solo_net
		false,		// fast_monsters
		false,		// no_monsters
		false,		// respawn_monsters
		false,		// no_widepix
		false,		// record_demo
		{}			// demo_name
	};

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

	struct DoomFileEntry
	{
		int32_t			version;
		DoomFileType	type;
		std::string		filename;
		std::string		full_path;
		std::string		cache_path;
		std::string		text_file;
		uint64_t		file_length;
		time_t			last_modified;
		GameMode_t		game_mode;
		GameMission_t	game_mission;
		GameVariant_t	game_variant;
		int32_t			is_voices_iwad;
		int32_t			should_merge;
		int32_t			has_dehacked_lump;

		Image			titlepic;
		std::vector< bgra_t > titlepic_raw;
	};

	void WriteEntry( DoomFileEntry& entry, const std::filesystem::path& stdpath )
	{
		std::string path = stdpath.string();
		std::filesystem::create_directories( path );

		std::filesystem::path infopath = path + "info.json";

		JSONElement root = JSONElement::MakeRoot();
		root.AddNumber( "version", entry.version );
		root.AddNumber( "type", (int32_t)entry.type );
		root.AddNumber( "game_mode", (int32_t)entry.game_mode );
		root.AddNumber( "game_mission", (int32_t)entry.game_mission );
		root.AddNumber( "game_variant", (int32_t)entry.game_variant );
		root.AddNumber( "is_voices_iwad", (int32_t)entry.is_voices_iwad );
		root.AddNumber( "should_merge", (int32_t)entry.should_merge );
		root.AddNumber( "has_dehacked_lump", (int32_t)entry.has_dehacked_lump );

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

	void ReadEntry( DoomFileEntry& entry, const std::filesystem::path& stdpath )
	{
		std::string path = stdpath.string();

		std::string infopath = path + "info.json";
		std::string infojson;
		infojson.resize( std::filesystem::file_size( std::filesystem::path( infopath ) ) );
		FILE* infofile = fopen( infopath.c_str(), "rb" );
		fread( (void*)infojson.data(), sizeof( char ), infojson.length(), infofile );
		fclose( infofile );

		JSONElement root			= JSONElement::Deserialise( infojson );
		entry.version				= to< int32_t >( root[ "version" ] );
		entry.type					= (DoomFileType)to< int32_t >( root[ "type" ] );
		entry.game_mode				= (GameMode_t)to< int32_t >( root[ "game_mode" ] );
		entry.game_mission			= (GameMission_t)to< int32_t >( root[ "game_mission" ] );
		entry.game_variant			= (GameVariant_t)to< int32_t >( root[ "game_variant" ] );
		entry.is_voices_iwad		= to< int32_t >( root[ "is_voices_iwad" ] );
		entry.should_merge			= to< int32_t >( root[ "should_merge" ] );
		entry.has_dehacked_lump		= to< int32_t >( root[ "has_dehacked_lump" ] );

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

	bool DoomFileEntryExists( const std::filesystem::path& stdpath )
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
#if WIN32
		auto utctime = std::chrono::file_clock::to_utc( filetime );
		auto systime = std::chrono::utc_clock::to_sys( utctime );
#else
		auto systime = std::chrono::file_clock::to_sys( filetime );
#endif
		return std::chrono::system_clock::to_time_t( systime );
	}

	DoomFileEntry ParseDoomFile( const std::filesystem::path& stdpath )
	{
		std::string path = stdpath.string();
		std::string filename = stdpath.filename().string();

		time_t lastmodified = GetLastModifiedTime( stdpath );
		size_t filelength = std::filesystem::file_size( stdpath );

		std::string cachepath = HOME_PATH
								+ cache_local
								+ filename
								+ "."
								+ std::to_string( lastmodified )
								+ "."
								+ std::to_string( filelength )
								+ DIR_SEPARATOR_S;

		std::filesystem::path stdcachepath( cachepath );

		DoomFileEntry entry = DoomFileEntry();
		entry.filename = filename;
		entry.full_path = path;
		entry.cache_path = cachepath;
		entry.last_modified = lastmodified;
		entry.file_length = filelength;

		if( !M_CheckParm( "-invalidatewadcache" ) && DoomFileEntryExists( stdcachepath ) )
		{
			ReadEntry( entry, stdcachepath );
		}

		if( M_CheckParm( "-invalidatewadcache" ) || entry.version != DoomFileEntryVersion )
		{
			if( std::regex_match( filename, DEHRegex ) )
			{
				entry.type = DoomFileType::Dehacked;
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
						entry.type = DoomFileType::IWAD;
					}
					else if( !strncmp( header.identification, "PWAD", 4 ) )
					{
						entry.type = DoomFileType::PWAD;
					}

					std::vector< filelump_t > lumps;
					lumps.resize( header.numlumps );

					filelump_t* lfreedoom	= nullptr;
					filelump_t* ldeutex		= nullptr;
					filelump_t* lhacxr		= nullptr;
					filelump_t* lloading	= nullptr;
					filelump_t* ltitle		= nullptr;
					filelump_t* lstartup	= nullptr;
					filelump_t* lendstrf	= nullptr;
					filelump_t* lv_start	= nullptr;
					filelump_t* lv_end		= nullptr;
					filelump_t* ls_start	= nullptr;
					filelump_t* ls_end		= nullptr;
					filelump_t* lss_start	= nullptr;
					filelump_t* lss_end		= nullptr;
					filelump_t* lf_start	= nullptr;
					filelump_t* lf_end		= nullptr;
					filelump_t* lff_start	= nullptr;
					filelump_t* lff_end		= nullptr;
					filelump_t* dehacked	= nullptr;
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
						if( !strncmp( lump.name, "FREEDOOM", 8 ) )			lfreedoom = &lump;
						else if( !strncmp( lump.name, "_DEUTEX_", 8 ) )		ldeutex = &lump;
						else if( !strncmp( lump.name, "HACX-R", 8 ) )		lhacxr = &lump;
						else if( !strncmp( lump.name, "LOADING", 8 ) )		lloading = &lump;
						else if( !strncmp( lump.name, "TITLE", 8 ) )		ltitle = &lump;
						else if( !strncmp( lump.name, "STARTUP", 8 ) )		lstartup = &lump;
						else if( !strncmp( lump.name, "ENDSTRF", 8 ) )		lendstrf = &lump;
						else if( !strncmp( lump.name, "V_START", 8 ) )		lv_start = &lump;
						else if( !strncmp( lump.name, "V_END", 8 ) )		lv_end = &lump;
						else if( !strncmp( lump.name, "S_START", 8 ) )		ls_start = &lump;
						else if( !strncmp( lump.name, "S_END", 8 ) )		ls_end = &lump;
						else if( !strncmp( lump.name, "SS_START", 8 ) )		lss_start = &lump;
						else if( !strncmp( lump.name, "SS_END", 8 ) )		lss_end = &lump;
						else if( !strncmp( lump.name, "F_START", 8 ) )		lf_start = &lump;
						else if( !strncmp( lump.name, "F_END", 8 ) )		lf_end = &lump;
						else if( !strncmp( lump.name, "FF_START", 8 ) )		lff_start = &lump;
						else if( !strncmp( lump.name, "FF_END", 8 ) )		lff_end = &lump;
						else if( !strncmp( lump.name, "DEHACKED", 8 ) )		dehacked = &lump;
						else if( !strncmp( lump.name, "TITLEPIC", 8 ) )		titlepic = &lump;
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
						entry.game_mode = entry.type == DoomFileType::IWAD ? shareware : registered;
					}

					if( entry.type == DoomFileType::PWAD )
					{
						// BFG edition hack, since it's actually a PWAD
						if( ( EqualCaseInsensitive( entry.filename, "doom.wad" ) || EqualCaseInsensitive( entry.filename, "doom2.wad" ) )
							&& dmenupic && m_chg )
						{
							entry.type = DoomFileType::IWAD;
							titlepic = dmenupic;
						}
						else if( EqualCaseInsensitive( entry.filename, "chex.wad" ) && ldeutex )
						{
							entry.type = DoomFileType::IWAD;
						}

						if( ls_start || lss_start || ls_end || lss_end
							|| lf_start || lf_end || lff_start || lff_end )
						{
							entry.should_merge = 1;
						}
					}

					if( entry.type == DoomFileType::IWAD )
					{
						if( lloading && ltitle )
						{
							entry.game_mission = heretic;
						}
						else if( lstartup && ltitle )
						{
							entry.game_mission = hexen;
						}
						else if( lendstrf )
						{
							entry.game_mission = strife;
						}
						else if( lv_start && lv_end )
						{
							entry.game_mission = strife;
							entry.is_voices_iwad = 1;
						}
						else if( map01 )
						{
							entry.game_mission = doom2;
							if( lhacxr ) entry.game_mission = pack_hacx;
							else if( EqualCaseInsensitive( entry.filename, "tnt.wad" ) ) entry.game_mission = pack_tnt;
							else if( EqualCaseInsensitive( entry.filename, "plutonia.wad" ) ) entry.game_mission = pack_plut;
						}
						else if( e1m1 )
						{
							entry.game_mission = doom;
							if( EqualCaseInsensitive( entry.filename, "chex.wad" ) && ldeutex ) entry.game_mission = pack_chex;
						}

						if( lfreedoom != nullptr )
						{
							entry.game_variant = freedoom;
						}
						else if( m_chg != nullptr )
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
					}

					if( !titlepic ) titlepic = dmenupic;
					if( titlepic )
					{
						std::vector< byte > source;
						source.resize( titlepic->size );
						fseek( file, titlepic->filepos, SEEK_SET );
						fread( source.data(), sizeof( byte ), titlepic->size, file );

						std::vector< rgb_t > pallette;
						if( !playpal )
						{
							pallette.resize( 256 );
							memcpy( pallette.data(), defaultpalette, sizeof( rgb_t ) * 256 );
						}
						else
						{
							pallette.resize( playpal->size / sizeof( rgb_t ) );
							fseek( file, playpal->filepos, SEEK_SET );
							fread( pallette.data(), sizeof( rgb_t ), playpal->size / sizeof( rgb_t ), file );
						}

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
			}

			WriteEntry( entry, stdcachepath );
		}

		if( !entry.titlepic_raw.empty() && !entry.titlepic.tex )
		{
			entry.titlepic.tex = I_TextureCreate( entry.titlepic.width, entry.titlepic.height, Format_BGRA8, entry.titlepic_raw.data() );
		}

		return entry;
	}

	using DoomFileEntries = std::vector< DoomFileEntry >;

	class DoomFileList
	{
	public:
		DoomFileList()
			: lists_ready( false )
			, abort_work( false )
			, total_files( 0 )
			, parsed_files( 0 )
		{
		}

		~DoomFileList()
		{
			for( DoomFileEntry& entry : IWADs )
			{
				if( entry.titlepic.tex ) I_TextureDestroy( entry.titlepic.tex );
			}

			for( DoomFileEntry& entry : PWADs )
			{
				if( entry.titlepic.tex ) I_TextureDestroy( entry.titlepic.tex );
			}
		}

		void PopulateDoomFileList()
		{
			IWADs.clear();
			PWADs.clear();
			DEHs.clear();

			auto IWADPaths = D_GetIWADPaths();
			std::string CachePath = HOME_PATH + cache_extracted;
			const char* CacheCStr = CachePath.c_str();

			// Should be a range adapter...
			std::vector< const char* > Views;
			Views.insert( Views.end(), IWADPaths.begin(), IWADPaths.end() );
			Views.push_back( CacheCStr );

			for( auto dir : Transform( Views, []( const char* pathname )
							{
								std::error_code error;
								auto it = std::filesystem::recursive_directory_iterator( std::filesystem::path( pathname ), std::filesystem::directory_options::skip_permission_denied, error );
								if( error ) return std::filesystem::recursive_directory_iterator();
								return it;
							} ) )
			{
				total_files += std::distance( dir, std::filesystem::recursive_directory_iterator{} );
			}

			for( auto dir : Transform( Views, []( const char* pathname )
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

					if( std::regex_match( pathstring, WADRegex )
						|| std::regex_match( pathstring, DEHRegex ) )
					{
						DoomFileEntry entry = ParseDoomFile( file.path() );
						if( entry.type == DoomFileType::IWAD && entry.game_mission != heretic && entry.game_mission != hexen && entry.game_mission != strife )
						{
							if( !entry.titlepic_raw.empty() && !entry.titlepic.tex )
							{
								entry.titlepic.tex = I_TextureCreate( entry.titlepic.width, entry.titlepic.height, Format_BGRA8, entry.titlepic_raw.data() );
							}
							IWADs.push_back( entry );
						}
						if( entry.type == DoomFileType::PWAD ) PWADs.push_back( entry );
						if( entry.type == DoomFileType::Dehacked ) DEHs.push_back( entry );
					}

					++parsed_files;
				}
			}

			glFlush();

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

		DoomFileEntries			IWADs;
		DoomFileEntries			PWADs;
		DoomFileEntries			DEHs;
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

		void Setup( DoomFileEntries& entries, const char* defaultiwad )
		{
			iwads = &entries;
			SetupIterators( defaultiwad );
		}

		void Add( DoomFileEntry& entry )
		{
			std::string current = middle->filename;
			iwads->push_back( entry );
			SetupIterators( current.c_str() );
		}

		void SetupIterators( const char* defaultiwad )
		{
			if( !iwads->empty() )
			{
				if( defaultiwad )
				{
					std::filesystem::path iwadpath( defaultiwad );
					std::string iwadfilename = iwadpath.filename().string();

					middle = std::find_if( iwads->begin(), iwads->end(), [ iwadfilename ]( DoomFileEntry& e )
					{
						return EqualCaseInsensitive( iwadfilename, e.filename );
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
		INLINE DoomFileEntry& Selected() const { return *middle; };

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

			constexpr ImVec2 arrowsize = { 30, 70 };
			constexpr float_t frameheight = 260.0f;
			constexpr float_t arrow_y = ( frameheight - arrowsize.y ) * 0.5;

			ImVec2 nextcursor = basecursor;
			nextcursor.y += arrow_y;
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
					if( middle->game_variant == freedoom )
					{
						igCentreText( "Freedoom: Phase %d", middle->game_mode == commercial ? 2 : 1 );
					}
					else if( !( middle->game_mission == doom || middle->game_mission == doom2 ) )
					{
						igCentreText( "%s, %s", GameMissions[ middle->game_mission ], GameVariants[ middle->game_variant ] );
					}
					else
					{
						igCentreText( "%s, %s", GameModes[ middle->game_mode ], GameVariants[ middle->game_variant ] );
					}
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
		DoomFileEntries*				iwads;
		DoomFileEntries::iterator	left;
		DoomFileEntries::iterator	middle;
		DoomFileEntries::iterator	right;
	};

	class DoomFileSelector
	{
	public:
		DoomFileSelector( )
			: pwads( { nullptr } )
			, dehs( { nullptr } )
			, current( -1 )
		{
		}

		void SetupPWADs( DoomFileEntries& entries, std::span< const char* > toselect )
		{
			SetupSelection( pwads, entries, toselect );
		}

		void SetupDEHs( DoomFileEntries& entries, std::span< const char* > toselect )
		{
			SetupSelection( dehs, entries, toselect );
		}

		void Add( DoomFileEntry& entry )
		{
			if( entry.type == DoomFileType::PWAD ) pwads.container->push_back( entry );
			if( entry.type == DoomFileType::Dehacked ) dehs.container->push_back( entry );
		}

		void ClearAllSelected( )
		{
			pwads.selected.clear();
			dehs.selected.clear();
		}

		void Select( DoomFileEntry& entry )
		{
			if( entry.type == DoomFileType::PWAD )
			{
				int32_t index = Select( pwads, entry );
				if( index >= 0 )
				{
					DoomFileEntry& local = pwads.container->at( index );
					if( !local.titlepic_raw.empty() && !local.titlepic.tex )
					{
						local.titlepic.tex = I_TextureCreate( local.titlepic.width, local.titlepic.height, Format_BGRA8, local.titlepic_raw.data() );
					}
				}
			}
			if( entry.type == DoomFileType::Dehacked ) Select( dehs, entry );
		}

		bool Deselect( const DoomFileEntry& entry )
		{
			if( entry.type == DoomFileType::PWAD ) return Deselect( pwads, entry );
			if( entry.type == DoomFileType::Dehacked ) return Deselect( dehs, entry );
			return false;
		}

		int32_t ShuffleSelectedDEHForward( int32_t index )
		{
			return ShuffleForward( dehs, index );
		}

		int32_t ShuffleSelectedDEHBackward( int32_t index )
		{
			return ShuffleBackward( dehs, index );
		}

		int32_t ShuffleSelectedPWADForward( int32_t index )
		{
			return ShuffleForward( pwads, index );
		}

		int32_t ShuffleSelectedPWADBackward( int32_t index )
		{
			return ShuffleBackward( pwads, index );
		}

		INLINE bool Valid() const { return pwads.container != nullptr; }

		INLINE auto& AllDEHs() { return *dehs.container; }
		INLINE auto SelectedDEHs() { return Transform( dehs.selected, [ this ]( int32_t index ) -> DoomFileEntry&
									{
										return dehs.container->at( index );
									} ); }

		INLINE auto& AllPWADs() { return *pwads.container; }
		INLINE auto SelectedPWADs() { return Transform( pwads.selected, [ this ]( int32_t index ) -> DoomFileEntry&
									{
										return pwads.container->at( index );
									} ); }

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
					DoomFileEntries* entries = (DoomFileEntries*)data;
					if( idx < entries->size() )
					{
						DoomFileEntry& entry = (*entries)[ idx ];
						*out_text = entry.filename.c_str();
						return true;
					}
					return false;
				};

				igPushIDPtr( this );
				igListBoxFnBoolPtr( "", &current, getter, (void*)pwads.container, pwads.container->size(), 14 );
				igPopID();

				igNextColumn();

				igGetContentRegionAvail( &framesize );
				if( igBeginChildFrame( 671, framesize, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar ) )
				{
					if( current >= 0 )
					{
						igText( pwads.container->at( current ).text_file.c_str() );
					}
				}
				igEndChildFrame();

				igColumns( 1, nullptr, false );
			}
			igEndChildFrame();

			igPopStyleVar( 1 );
		}

	private:
		struct Selection
		{
			DoomFileEntries*		container;
			std::vector< int32_t >	selected;
		};

		static void SetupSelection( Selection& selection, DoomFileEntries& entries, std::span< const char* > toselect )
		{
			selection.container = &entries;

			for( const char* currfile : toselect )
			{
				std::string entryname = std::filesystem::path( currfile ).filename().string();

				for( int32_t index : iota( 0, selection.container->size() ) )
				{
					DoomFileEntry& file = selection.container->at( index );

					if( EqualCaseInsensitive( entryname, file.filename ) )
					{
						selection.selected.push_back( index );
						break;
					}
				}
			}
		}

		static int32_t Select( Selection& selection, DoomFileEntry& entry )
		{
			for( int32_t index : iota( 0, selection.container->size() ) )
			{
				DoomFileEntry& file = selection.container->at( index );

				if( EqualCaseInsensitive( entry.full_path, file.full_path )
					&& std::find( selection.selected.begin(), selection.selected.end(), index ) == selection.selected.end() ) 
				{
					selection.selected.push_back( index );
					return index;
				}
			}

			return -1;
		}

		static bool Deselect( Selection& selection, const DoomFileEntry& entry )
		{
			auto found = std::find_if( selection.container->begin(), selection.container->end(), [ &entry ]( const DoomFileEntry& file )
				{
					return EqualCaseInsensitive( entry.filename, file.filename );
				} );
			if( found != selection.container->end() )
			{
				int32_t index = std::distance( selection.container->begin(), found );
				auto foundindex = std::find( selection.selected.begin(), selection.selected.end(), index );
				if( foundindex != selection.selected.end() )
				{
					selection.selected.erase( foundindex );
					return true;
				}
			}

			return false;
		}

		static int32_t ShuffleForward( Selection& selection, int32_t index )
		{
			if( index < selection.selected.size() - 1 )
			{
				std::swap( selection.selected[ index ], selection.selected[ index + 1 ] );
				++index;
			}

			return index;
		}

		static int32_t ShuffleBackward( Selection& selection, int32_t index )
		{
			if( index >= 1 && index < selection.selected.size() )
			{
				std::swap( selection.selected[ index ], selection.selected[ index - 1 ] );
				--index;
			}
			return index;
		}

		Selection				pwads;
		Selection				dehs;

		int32_t					current;
	};

	// Probably not worth duck typing this thing, so ye olde OO vtables it is
	class LauncherPanel
	{
	public:
		LauncherPanel() = delete;
		virtual ~LauncherPanel() { }

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

	static std::stack< LauncherPanel* >	panel_stack;
	static JobThread* jobs = nullptr;
	static SDL_GLContext jobs_context = nullptr;

	void PushPanel( LauncherPanel* panel )
	{
		if( !panel_stack.empty() )
		{
			// panel_stack.top()->Exit();
		}
		panel_stack.push( panel );
		panel->Enter();
	}

	template< typename _ty >
	void PushPanel( std::shared_ptr< _ty > panel )
	{
		PushPanel( panel.get() );
	}

	void PopPanel( )
	{
		if( !panel_stack.empty() )
		{
			// panel_stack.top()->Exit();
			panel_stack.pop();
			if( !panel_stack.empty() )
			{
				panel_stack.top()->Enter();
			}
		}
	}

	void PopPanelToRoot( )
	{
		while( panel_stack.size() > 1 )
		{
			// panel_stack.top()->Exit();
			panel_stack.pop();
		}

		if( panel_stack.size() > 1 )
		{
			panel_stack.top()->Enter();
		}
	}

	void PopPanelsAndReplaceRoot( LauncherPanel* panel )
	{
		while( !panel_stack.empty() )
		{
			// panel_stack.top()->Exit();
			panel_stack.pop();
		}

		PushPanel( panel );
	}

	template< typename _ty >
	void PopPanelsAndReplaceRoot( std::shared_ptr< _ty > panel )
	{
		PopPanelsAndReplaceRoot( panel.get() );
	}

	class IdgamesFilePanel : public LauncherPanel
	{
	public:
		IdgamesFilePanel( std::shared_ptr< IWADSelector >& iwad, std::shared_ptr< DoomFileSelector >& pwad
						, const std::string& path, const std::string& name
						, const size_t sz, const size_t ag
						, const std::string& t, const std::string& a, double_t r )
			: LauncherPanel( "Launcher_IdgamesBrowser" )
			, iwadselector( iwad )
			, doomfileselector( pwad )
			, fullpath( path )
			, filename( name )
			, title( t )
			, author( a )
			, size( sz )
			, age( ag )
			, rating( r )
			, votes( 0 )
			, downloaded( false )
			, extracted( false )
			, fileready( false )
			, localfilesystemop( false )
			, currentlydoing( "Nothing" )
			, progress( 0.0 )
			, shouldcancel( false )
		{
			if( !fullpath.ends_with( '/' ) )
			{
				fullpath += '/';
			}

			query = idgames_api_file + fullpath + filename;

			downloadpath = HOME_PATH + cache_download + fullpath + filename + "_" + to< std::string >( size ) + "_" + to< std::string >( age ) + DIR_SEPARATOR_S;
			std::replace( downloadpath.begin(), downloadpath.end(), '/', DIR_SEPARATOR );
			std::filesystem::path downloadfile = downloadpath + filename;
			downloaded = std::filesystem::exists( downloadfile );

			extractedpath = HOME_PATH + cache_extracted + fullpath + filename + DIR_SEPARATOR;
			std::replace( extractedpath.begin(), extractedpath.end(), '/', DIR_SEPARATOR );
			extracted = std::filesystem::exists( extractedpath );
		}

		virtual ~IdgamesFilePanel() { }

		virtual void Enter() override
		{
			if( !fileready.load() )
			{
				RefreshFile();
			}
		}

		constexpr const std::string& Filename() const	{ return filename; }
		constexpr const std::string& Title() const		{ return title; }
		constexpr const std::string& Author() const		{ return author; }
		constexpr const double_t Rating() const			{ return rating; }
		constexpr const bool Downloaded() const			{ return downloaded; }
		constexpr const bool Extracted() const			{ return extracted; }

	protected:
		void RefreshFile()
		{
			fileready = false;

			jobs->AddJob( [ this ]()
			{
				bool successful = false;
				std::string listing;
				while( !successful )
				{
					listing.clear();
					successful = M_URLGetString( listing, idgames_api_url, query.c_str() );
				}
				JSONElement asjson = JSONElement::Deserialise( listing.c_str() );
				const JSONElement& content = asjson[ "content" ];

				date			= to< std::string >( content[ "date" ] );
				email			= to< std::string >( content[ "email" ] );
				description		= to< std::string >( content[ "description" ] );
				credits			= to< std::string >( content[ "credits" ] );
				base			= to< std::string >( content[ "base" ] );
				buildtime		= to< std::string >( content[ "buildtime" ] );
				editors			= to< std::string >( content[ "editors" ] );
				bugs			= to< std::string >( content[ "bugs" ] );
				textfile		= to< std::string >( content[ "textfile" ] );
				size			= to< size_t >( content[ "size" ] );
				age				= to< size_t >( content[ "age" ] );
				rating			= to< double_t >( content[ "rating" ] );
				votes			= to< size_t >( content[ "votes" ] );

				downloadpath = HOME_PATH + cache_download + fullpath + filename + "_" + to< std::string >( size ) + "_" + to< std::string >( age ) + DIR_SEPARATOR_S;
				std::replace( downloadpath.begin(), downloadpath.end(), '/', DIR_SEPARATOR );
				std::filesystem::path downloadfile = downloadpath + filename;
				downloaded = std::filesystem::exists( downloadfile );

				extractedpath = HOME_PATH + cache_extracted + fullpath + filename + DIR_SEPARATOR;
				std::replace( extractedpath.begin(), extractedpath.end(), '/', DIR_SEPARATOR );
				extracted = std::filesystem::exists( extractedpath );

				if( extracted )
				{
					PerformGetDoomFileEntries( false );
				}

				fileready = true;
			} );

		}

		void DownloadAndExtractFile( std::string location )
		{
			localfilesystemop = true;
			currentlydoing = "Downloading file";
			shouldcancel = false;

			jobs->AddJob( [ this, location ]()
			{
				std::vector< byte > data;
				data.reserve( size );
				int64_t filedate = 0;
				std::string error;

				auto progressfunc = std::function( [this]( ptrdiff_t current, ptrdiff_t total ) -> bool
				{
					progress = (double_t)current / (double_t)total;
					return !shouldcancel.load();
				} );

				bool successful = false;
				while( !successful && !shouldcancel.load() )
				{
					progress = 0;
					successful = M_URLGetBytes( data, filedate, error, location.c_str(), nullptr, progressfunc );
				}

				if( shouldcancel.load() )
				{
					localfilesystemop = false;
					currentlydoing = "Nothing";
					return;
				}

				currentlydoing = "Writing file to disk";
				progress = 0;

				std::filesystem::path downloadto = downloadpath;
				std::filesystem::create_directories( downloadto );

				std::string downloadtozip = downloadpath + filename;
				FILE* zipfile = fopen( downloadtozip.c_str(), "wb" );
				fwrite( (void*)data.data(), sizeof( char ), data.size(), zipfile );
				fclose( zipfile );

				utimbuf zipfiletime = { filedate, filedate };
				utime( downloadtozip.c_str(), &zipfiletime );

				downloaded = true;

				currentlydoing = "Extracting files";

				PerformExtractFile();

				localfilesystemop = false;
				currentlydoing = "Nothing";

			} );
		}

		void PerformGetDoomFileEntries( bool addtoselector )
		{
			iwads.clear();
			pwads.clear();
			dehfiles.clear();

			std::error_code error;
			auto directoryiterator = std::filesystem::recursive_directory_iterator( std::filesystem::path( extractedpath ), std::filesystem::directory_options::skip_permission_denied, error );
			for( auto& file : directoryiterator )
			{
				std::string filepath = file.path().string();
				if( std::regex_match( filepath, WADRegex )
					|| std::regex_match( filepath, DEHRegex ) )
				{
					DoomFileEntry entry = ParseDoomFile( file.path() );
					if( entry.type == DoomFileType::IWAD )
					{
						if( !entry.titlepic_raw.empty() && !entry.titlepic.tex )
						{
							entry.titlepic.tex = I_TextureCreate( entry.titlepic.width, entry.titlepic.height, Format_BGRA8, entry.titlepic_raw.data() );
						}

						if( addtoselector ) iwadselector->Add( entry );
						iwads.push_back( entry );
					}
					else if( entry.type == DoomFileType::PWAD )
					{
						if( addtoselector ) doomfileselector->Add( entry );
						pwads.push_back( entry );
					}
					else if( entry.type == DoomFileType::Dehacked )
					{
						if( addtoselector ) doomfileselector->Add( entry );
						dehfiles.push_back( entry );
					}
				}
			}
		}

		void PerformExtractFile()
		{
			auto progressfunc = std::function( [this]( ptrdiff_t current, ptrdiff_t total ) -> bool
			{
				progress = (double_t)current / (double_t)total;
				return true;
			} );

			std::string zippath = downloadpath + filename;

			M_ZipExtractAllFromFile( zippath.c_str(), extractedpath.c_str(), progressfunc );

			PerformGetDoomFileEntries( true );

			extracted = true;
		}

		void ExtractFile( )
		{
			localfilesystemop = true;
			currentlydoing = "Extracting files";

			jobs->AddJob( [ this ]()
			{
				PerformExtractFile();

				localfilesystemop = false;
				currentlydoing = "Nothing";

			} );
		}

		virtual void RenderContents() override
		{
			constexpr ImVec2 backbuttonsize = { 50.f, 25.f };
			constexpr ImVec2 refreshbuttonsize = { 90.f, 25.f };
			constexpr float_t framepadding = 10;

			if( fileready.load() && !localfilesystemop.load() )
			{
				Image found = Image();
				for( DoomFileEntry& iwad : iwads )
				{
					if( iwad.titlepic.tex != nullptr )
					{
						found = iwad.titlepic;
						break;
					}
				}
				if( !found.tex )
				{
					for( DoomFileEntry& pwad : pwads )
					{
						if( pwad.titlepic.tex != nullptr )
						{
							found = pwad.titlepic;
							break;
						}
					}
				}

				if( found.tex )
				{
					ImVec2 regionavail;
					igGetContentRegionAvail( &regionavail );

					igColumns( 2, "idgames_file_columns", false );
					igSetColumnWidth( 0, 320 );
					igSetColumnWidth( 1, regionavail.x - 320 );
					igColumns( 1, nullptr, false );

					igColumns( 2, "idgames_file_columns", false );

					igGetContentRegionAvail( &regionavail );
					float_t scale = regionavail.x / found.width;

					constexpr ImVec4 tint = { 1, 1, 1, 1 };
					constexpr ImVec4 border = { 0, 0, 0, 0 };
					constexpr ImVec2 tl = { 0, 0 };
					constexpr ImVec2 br = { 1, 1 };

					ImVec2 imagesize = { found.width * scale, found.height * scale };

					igImage( I_TextureGetHandle( found.tex ), imagesize, tl, br, tint, border );

					igNextColumn();
				}
			}

			ImVec2 cursorpos;

			igPushFont( font_inconsolata_large );
			igCentreText( !title.empty() ? title.c_str() : filename.c_str() );
			igPopFont();

			igCentreText( "by %s", author.c_str() );

			constexpr ImVec2 starsize = { 20.f, 25.f };
			igCentreNextElement( starsize.x * 5 );

			igRating( "file_rating", rating / 5.0, 5, starsize, igGetColorU32Col( ImGuiCol_Text, 1.0f ), igGetColorU32Col( ImGuiCol_TextDisabled, 1.0f ) );
			igGetCursorPos( &cursorpos );
			cursorpos.y += 4;
			igSetCursorPos( cursorpos );

			if( fileready.load() )
			{
				constexpr ImVec2 downloadsize = { 160.f, 50.f };
				constexpr ImVec2 selectsize = { 250.f, 50.f };
				constexpr ImVec2 fileopspinnersize = { 20.f, 50.f };
				constexpr ImVec2 fileopprogresssize = { 300.f, 20.f };

				if( !localfilesystemop )
				{
					igPushFont( font_inconsolata_medium );
					if( !downloaded )
					{
						igCentreNextElement( downloadsize.x );
						if( igButton( "Download", downloadsize ) )
						{
							OpenIdgamesLocationSelector( "idgamesfile" );
						}
					}
					else if( !extracted )
					{
						igCentreNextElement( downloadsize.x );
						if( igButton( "Extract", downloadsize ) )
						{
							ExtractFile();
						}
					}
					else
					{
						igCentreNextElement( selectsize.x );
						if( igButton( "Select for play", selectsize ) )
						{
							doomfileselector->ClearAllSelected();

							for( auto& deh : dehfiles )
							{
								doomfileselector->Select( deh );
							}

							for( auto& pwad : pwads )
							{
								doomfileselector->Select( pwad );
							}

							while( panel_stack.size() > 1 )
							{
								PopPanelToRoot();
							}
						}
					}
					igPopFont();

					const char* url = IdgamesLocationSelector( "idgamesfile" );
					if( url )
					{
						DownloadAndExtractFile( url + fullpath + filename );
					}
				}
				else
				{
					ImVec2 framesize;
					igGetContentRegionAvail( &framesize );
					igGetCursorPos( &cursorpos );

					cursorpos.x += framesize.x * 0.25f;
					igSetCursorPos( cursorpos );
					igSpinner( fileopspinnersize, 0.5 );

					cursorpos.x += fileopspinnersize.y + 5.f;
					igSetCursorPos( cursorpos );
					igText( currentlydoing.load() );

					cursorpos.y += fileopspinnersize.y - fileopprogresssize.y;
					igSetCursorPos( cursorpos );
					ImVec2 progsize = { framesize.x * 0.5f - 5.f, fileopprogresssize.y };
					igRoundProgressBar( progress.load(), progsize, 2.f );
				}
			}

			igColumns( 1, nullptr, false );

			ImVec2 framesize;
			igGetContentRegionAvail( &framesize );
			framesize.y -= backbuttonsize.y + 8;

			if( igBeginChildFrame( 999, framesize, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground ) )
			{
				if( !fileready.load() )
				{
					constexpr ImVec2 loadingsize = { 112.0f, 70.0f };

					ImVec2 content;
					igGetContentRegionAvail( &content );
					igGetCursorPos( &cursorpos );

					cursorpos.x += ( content.x - loadingsize.x ) * 0.5f;
					cursorpos.y += ( content.y - loadingsize.y ) * 0.5f;
					igSetCursorPos( cursorpos );
					igSpinner( loadingsize, 0.5 );
					igCentreText( "Fetching file details" );
				}
				else
				{
					igTextWrapped( textfile.c_str() );
				}
			}
			igEndChildFrame();

			if( fileready.load() )
			{
				ImVec2 cursorpos;
				igGetCursorPos( &cursorpos );
				igGetContentRegionMax( &framesize );
				cursorpos.y = framesize.y - backbuttonsize.y;

				if( localfilesystemop.load() )
				{
					cursorpos.x = framesize.x - backbuttonsize.x - framepadding;
					igSetCursorPos( cursorpos );
					if( igButton( "Cancel", backbuttonsize ) )
					{
						shouldcancel = true;
					}
				}
				else
				{
					cursorpos.x = framesize.x - backbuttonsize.x - framepadding - refreshbuttonsize.x - framepadding;
					igSetCursorPos( cursorpos );
					if( igButton( "Refresh", refreshbuttonsize ) )
					{
						RefreshFile();
					}

					cursorpos.x = framesize.x - backbuttonsize.x - framepadding;
					igSetCursorPos( cursorpos );
					if( igButton( "Back", backbuttonsize ) )
					{
						PopPanel();
					}
				}
			}
		}

	private:
		std::shared_ptr< IWADSelector >		iwadselector;
		std::shared_ptr< DoomFileSelector >	doomfileselector;
		DoomFileEntries						iwads;
		DoomFileEntries						pwads;
		DoomFileEntries						dehfiles;

		std::string							fullpath;
		std::string							filename;
		std::string							title;
		std::string							date;
		std::string							author;
		std::string							email;
		std::string							description;
		std::string							credits;
		std::string							base;
		std::string							buildtime;
		std::string							editors;
		std::string							bugs;
		std::string							textfile;
		size_t								size;
		size_t								age;
		double_t							rating;
		size_t								votes;

		std::string							query;
		std::string							downloadpath;
		std::string							extractedpath;
		bool								downloaded;
		bool								extracted;
		std::atomic< bool >					fileready;
		std::atomic< bool >					localfilesystemop;
		std::atomic< const char* >			currentlydoing;
		std::atomic< double_t >				progress;
		std::atomic< bool >					shouldcancel;
	};

	class IdgamesFilter
	{
	public:
		IdgamesFilter()
			: appliedfilters( "<no filter>" )
			, downloaded( false )
			, min_rating( 0 )
		{
		}

		void Render()
		{
			if( igButton( "Filter options", zero ) )
			{
				igOpenPopup( "filter_options", ImGuiPopupFlags_None );
			}

			igSameLine( 0, -1 );
			igText( appliedfilters.c_str() );
			igSpacing();

			if( igBeginPopup( "filter_options", ImGuiWindowFlags_None ) )
			{
				bool refresh = false;
				refresh |= igCheckbox( "Downloaded", &downloaded );
				refresh |= igSliderFloat( "Rating", &min_rating, 0.f, 5.f, "%0.1f", ImGuiSliderFlags_AlwaysClamp );
				igEndPopup();

				if( refresh ) UpdateApplied();
			}
		}

		auto Filter( std::vector< std::shared_ptr< IdgamesFilePanel > >& files )
		{
			return ::Filter( files, [ this ]( std::shared_ptr< IdgamesFilePanel >& file )
					{
						return ( !downloaded || file->Downloaded() )
							&& ( file->Rating() >= min_rating );
					} );
		}

	private:
		void UpdateApplied()
		{
			appliedfilters.clear();

			auto append = [ this ]( const std::string& str )
			{
				if( !appliedfilters.empty() )
				{
					appliedfilters += ", ";
				}
				appliedfilters += str;
			};

			if( downloaded )
			{
				append( "Downloaded" );
			}
			if( min_rating > 0 )
			{
				append( "Minimum rating: " + to< std::string >( min_rating ) );
			}
		}

		std::string		appliedfilters;
		bool			downloaded;
		float_t			min_rating;
	};

	class IdgamesBrowserPanel : public LauncherPanel
	{
	public:
		IdgamesBrowserPanel( std::shared_ptr< IdgamesFilter >& filt, std::shared_ptr< IWADSelector >& iwad, std::shared_ptr< DoomFileSelector >& pwad
							, const std::string& folder, const std::string& nicename, bool refresh = true )
			: LauncherPanel( "Launcher_IdgamesBrowser" )
			, filter( filt )
			, iwadselector( iwad )
			, doomfileselector( pwad )
			, foldername( folder )
			, foldernicename( nicename )
			, listready( !refresh )
			, norefresh( false )
		{
			if( !foldername.ends_with( '/' ) )
			{
				foldername += '/';
			}
			query = idgames_api_folder + foldername;
		}

		virtual ~IdgamesBrowserPanel() { }

		void DisallowRefresh()
		{
			norefresh = true;
		}

		void AddFolder( const std::string& name, const std::string& nicename )
		{
			folders.push_back( std::make_shared< IdgamesBrowserPanel >( filter, iwadselector, doomfileselector, foldername + name, nicename ) );
		}

		virtual void Enter() override
		{
			if( !listready.load() )
			{
				RefreshList();
			}
		}

		constexpr const std::string& Name()			{ return foldernicename; }

	protected:
		void RefreshList()
		{
			if( norefresh )
			{
				return;
			}

			listready = false;
			folders.clear();
			files.clear();

			jobs->AddJob( [ this ]()
			{
				bool successful = false;
				std::string listing;
				while( !successful )
				{
					listing.clear();
					successful = M_URLGetString( listing, idgames_api_url, query.c_str() );
				}

				JSONElement asjson = JSONElement::Deserialise( listing.c_str() );
				const JSONElement& content = asjson[ "content" ];
				const JSONElement& dirs = content[ "dir" ];
				if( dirs.HasChildren() )
				{
					for( auto& currdir : dirs.Children() )
					{
						const std::string& fulldir = to< std::string >( currdir[ "name" ] );
						if( fulldir.empty() )
						{
							continue;
						}
						std::string nicename = fulldir;
						if( nicename.ends_with( '/' ) )
						{
							nicename.erase( nicename.size() - 1, 1 );
							if( nicename.empty() )
							{
								nicename = "[root]";
							}
							else
							{
								size_t found = nicename.find_last_of( '/' );
								if( found != std::string::npos )
								{
									nicename = nicename.substr( found + 1 );
								}

							}
						}
						folders.push_back( std::make_shared< IdgamesBrowserPanel >( filter, iwadselector, doomfileselector, fulldir, nicename ) );
					}
				}

				const JSONElement& idgamesfiles = content[ "file" ];
				if( idgamesfiles.HasChildren() )
				{
					for( auto& currfile : idgamesfiles.Children() )
					{
						auto& file = *files.insert( files.end(), std::make_shared< IdgamesFilePanel >(	iwadselector, doomfileselector
																										, to< std::string >( currfile[ "dir" ] )
																										, to< std::string >( currfile[ "filename" ] )
																										, to< size_t >( currfile[ "size" ] )
																										, to< size_t >( currfile[ "age" ] )
																										, to< std::string >( currfile[ "title" ] )
																										, to< std::string >( currfile[ "author" ] )
																										, to< double_t >( currfile[ "rating" ] ) ) );
					}
				}

				listready = true;
			} );
		}

		virtual void RenderContents() override
		{
			constexpr ImVec2 backbuttonsize = { 50.f, 25.f };
			constexpr ImVec2 refreshbuttonsize = { 90.f, 25.f };
			constexpr float_t framepadding = 10;

			ImVec2 framesize;
			igGetContentRegionMax( &framesize );
			framesize.y -= backbuttonsize.y + 98;

			igPushFont( font_inconsolata_large );
			igCentreText( foldernicename.c_str() );
			igPopFont();
			igSpacing();

			if( !norefresh )
			{
				filter->Render();
				igSpacing();
			}

			ImVec2 cursorpos;
			igGetCursorPos( &cursorpos );

			if( igBeginChildFrame( 999, framesize, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground ) )
			{
				if( listready )
				{
					constexpr ImVec2 foldertilesize = { 176.f, 40.f };
					constexpr ImVec2 filetilesize = { 176.f, 80.f };

					igPushStyleVarFloat( ImGuiStyleVar_FrameRounding, 3.f );

					igTileView( folders, foldertilesize, [this]( const std::shared_ptr< IdgamesBrowserPanel >& folder, const ImVec2& size )
					{
						if( igButton( folder->Name().c_str(), size ) )
						{
							PushPanel( folder );
						}
					} );

					if( !folders.empty() && !files.empty() )
					{
						igNewLine();
					}

					igTileView( filter->Filter( files ), filetilesize, [this]( const std::shared_ptr< IdgamesFilePanel >& file, const ImVec2& size )
					{
						if( igButtonCustomContents( file->Filename().c_str(), size, ImGuiButtonFlags_None, [this, &file]( const ImVec2& size )
						{
							constexpr ImVec2 starsize = { 16.f, 20.f };
							ImVec2 ratingpos;
							igGetCursorPos( &ratingpos );
							ratingpos.y += filetilesize.y - starsize.y - 4;

							igTextWrapped( !file->Title().empty() ? file->Title().c_str() : file->Filename().c_str() );
							igSetCursorPos( ratingpos );
							igRating( "starcount", file->Rating() / 5.0, 5, starsize, igGetColorU32Col( ImGuiCol_Text, 1.0f ), igGetColorU32Col( ImGuiCol_TextDisabled, 1.0f ) );
						} ) )
						{
							PushPanel( file );
						}
					} );

					igPopStyleVar( 1 );
				}
				else
				{
					constexpr ImVec2 loadingsize = { 112.0f, 70.0f };

					ImVec2 content;
					igGetContentRegionAvail( &content );
					igGetCursorPos( &cursorpos );

					cursorpos.x += ( content.x - loadingsize.x ) * 0.5f;
					cursorpos.y += ( content.y - loadingsize.y ) * 0.5f;
					igSetCursorPos( cursorpos );
					igSpinner( loadingsize, 0.5 );
					igCentreText( "Fetching directory" );
				}
			}
			igEndChildFrame();

			if( listready )
			{
				ImVec2 cursorpos;
				igGetContentRegionMax( &framesize );
				cursorpos.y = framesize.y - backbuttonsize.y;

				if( !norefresh )
				{
					cursorpos.x = framesize.x - backbuttonsize.x - framepadding - refreshbuttonsize.x - framepadding;
					igSetCursorPos( cursorpos );
					if( igButton( "Refresh", refreshbuttonsize ) )
					{
						RefreshList();
					}
				}

				cursorpos.x = framesize.x - backbuttonsize.x - framepadding;
				igSetCursorPos( cursorpos );
				if( igButton( "Back", backbuttonsize ) )
				{
					PopPanel();
				}
			}
		}

	private:
		std::shared_ptr< IdgamesFilter >						filter;
		std::shared_ptr< IWADSelector >							iwadselector;
		std::shared_ptr< DoomFileSelector >						doomfileselector;
		std::vector< std::shared_ptr< IdgamesBrowserPanel > >	folders;
		std::vector< std::shared_ptr< IdgamesFilePanel > >		files;
		std::string												foldername;
		std::string												foldernicename;
		std::string												query;
		std::atomic< bool >										listready;
		bool													norefresh;
	};

	class FileSelectorPanel : public LauncherPanel
	{
	public:
		FileSelectorPanel( std::shared_ptr< IWADSelector >& iwad, std::shared_ptr< DoomFileSelector >& pwad )
			: LauncherPanel( "Launcher_FileSelector" )
			, iwadselector( iwad )
			, doomfileselector( pwad )
			, dehselected( -1 )
			, pwadselected( -1 )
		{
		}

		virtual ~FileSelectorPanel() { }

		virtual void Enter() override
		{
			dehselected = -1;
			pwadselected = -1;
		}

	protected:
		virtual void RenderContents() override
		{
			constexpr ImVec2 backbuttonsize = { 50.f, 25.f };
			constexpr float_t framepadding = 10.f;

			ImVec2 contentsize;
			igGetContentRegionMax( &contentsize );

			igPushFont( font_inconsolata_large );
			igCentreText( "Change selected files" );
			igPopFont();
			igNewLine();

			igColumns( 5, "fileselectorpanel_columns", false );
			igSetColumnWidth( 0, 200 );
			igSetColumnWidth( 1, 40 );
			igSetColumnWidth( 2, 200 );
			igSetColumnWidth( 3, 40 );
			igColumns( 1, nullptr, false );

			igColumns( 5, "fileselectorpanel_columns", false );
			
			// Dehacked
			{
				igText( "Dehacked files" );

				ImVec2 availsize;
				igGetContentRegionAvail( &availsize );
				availsize.y -= backbuttonsize.y + framepadding;

				int32_t id = ImGuiWindow_GetIDStr( igGetCurrentWindow(), "dehacked_selected_files", nullptr );

				if( igBeginChildFrame( id, availsize, ImGuiWindowFlags_NoResize ) )
				{
					int32_t index = 0;
					for( DoomFileEntry& entry : doomfileselector->SelectedDEHs() )
					{
						if( igSelectableBool( entry.filename.c_str(), dehselected == index, ImGuiSelectableFlags_None, zero ) )
						{
							dehselected = ( dehselected == index ? -1 : index );
						}
						++index;
					}
				}
				igEndChildFrame();

				igNextColumn();
				igNewLine();

				igGetContentRegionAvail( &availsize );
				availsize.y -= backbuttonsize.y + framepadding;

				ImVec2 cursorpos;
				igGetCursorPos( &cursorpos );

				constexpr ImVec2 buttonsize = { 20.f, 20.f };
				if( igButtonEx( "+##dehacked_add", buttonsize, ImGuiButtonFlags_None ) )
				{
					igOpenPopup( "dehacked_add_popup", ImGuiPopupFlags_None );
				}

				bool isdisabled = false;
				if( dehselected < 0 || dehselected >= doomfileselector->SelectedDEHs().size() )
				{
					igPushItemFlag( ImGuiItemFlags_Disabled, true );
					isdisabled = true;
				}

				if( igArrowButtonEx( "dehacked_up", ImGuiDir_Up, buttonsize, ImGuiButtonFlags_None ) )
				{
					dehselected = doomfileselector->ShuffleSelectedDEHBackward( dehselected );
				}
				if( igArrowButtonEx( "dehacked_down", ImGuiDir_Down, buttonsize, ImGuiButtonFlags_None ) )
				{
					dehselected = doomfileselector->ShuffleSelectedDEHForward( dehselected );
				}
				if( igButtonEx( "X##dehacked_remove", buttonsize, ImGuiButtonFlags_None ) )
				{
					if( doomfileselector->Deselect( doomfileselector->SelectedDEHs()[ dehselected ] )
						&& dehselected >= doomfileselector->SelectedDEHs().size() )
					{
						dehselected = doomfileselector->SelectedDEHs().size() - 1;
					}
				}

				if( isdisabled )
				{
					igPopItemFlag();
				}

				constexpr ImVec2 popupsize = { 150.f, 300.f };
				igSetNextWindowSize( popupsize, ImGuiCond_Always );
				if( igBeginPopup( "dehacked_add_popup", ImGuiWindowFlags_None ) )
				{
					for( DoomFileEntry& entry : doomfileselector->AllDEHs() )
					{
						bool found = false;
						for( DoomFileEntry& deh : doomfileselector->SelectedDEHs() )
						{
							if( EqualCaseInsensitive( entry.filename, deh.filename ) )
							{
								found = true;
								break;
							}
						}

						if( !found && igSelectableBool( entry.filename.c_str(), false, ImGuiSelectableFlags_DontClosePopups, zero ) )
						{
							doomfileselector->Select( entry );
						}
					}
					igEndPopup();
				}

				igNextColumn();
			}

			// WADs
			{
				igText( "PWAD files" );

				ImVec2 availsize;
				igGetContentRegionAvail( &availsize );
				availsize.y -= backbuttonsize.y + framepadding;

				int32_t id = ImGuiWindow_GetIDStr( igGetCurrentWindow(), "pwad_selected_files", nullptr );

				if( igBeginChildFrame( id, availsize, ImGuiWindowFlags_NoResize ) )
				{
					int32_t index = 0;
					for( DoomFileEntry& entry : doomfileselector->SelectedPWADs() )
					{
						if( igSelectableBool( entry.filename.c_str(), pwadselected == index, ImGuiSelectableFlags_None, zero ) )
						{
							pwadselected = ( pwadselected == index ? -1 : index );
						}
						++index;
					}
				}
				igEndChildFrame();

				igNextColumn();
				igNewLine();

				igGetContentRegionAvail( &availsize );
				availsize.y -= backbuttonsize.y + framepadding;

				ImVec2 cursorpos;
				igGetCursorPos( &cursorpos );

				constexpr ImVec2 buttonsize = { 20.f, 20.f };
				if( igButtonEx( "+##pwads_add", buttonsize, ImGuiButtonFlags_None ) )
				{
					igOpenPopup( "pwads_add_popup", ImGuiPopupFlags_None );
				}

				bool isdisabled = false;
				if( pwadselected < 0 || pwadselected >= doomfileselector->SelectedPWADs().size() )
				{
					igPushItemFlag( ImGuiItemFlags_Disabled, true );
					isdisabled = true;
				}

				if( igArrowButtonEx( "pwads_up", ImGuiDir_Up, buttonsize, ImGuiButtonFlags_None ) )
				{
					pwadselected = doomfileselector->ShuffleSelectedPWADBackward( pwadselected );
				}
				if( igArrowButtonEx( "pwads_down", ImGuiDir_Down, buttonsize, ImGuiButtonFlags_None ) )
				{
					pwadselected = doomfileselector->ShuffleSelectedPWADForward( pwadselected );
				}

				if( igButtonEx( "X##pwads_remove", buttonsize, ImGuiButtonFlags_None ) )
				{
					if( doomfileselector->Deselect( doomfileselector->SelectedPWADs()[ pwadselected ] )
						&& pwadselected >= doomfileselector->SelectedPWADs().size() )
					{
						pwadselected = doomfileselector->SelectedPWADs().size() - 1;
					}
				}

				if( isdisabled )
				{
					igPopItemFlag();
				}

				constexpr ImVec2 popupsize = { 150.f, 300.f };
				igSetNextWindowSize( popupsize, ImGuiCond_Always );
				if( igBeginPopup( "pwads_add_popup", ImGuiWindowFlags_None ) )
				{
					for( DoomFileEntry& entry : doomfileselector->AllPWADs() )
					{
						bool found = false;
						for( DoomFileEntry& pwad : doomfileselector->SelectedPWADs() )
						{
							if( EqualCaseInsensitive( entry.filename, pwad.filename ) )
							{
								found = true;
								break;
							}
						}

						if( !found && igSelectableBool( entry.filename.c_str(), false, ImGuiSelectableFlags_DontClosePopups, zero ) )
						{
							doomfileselector->Select( entry );
						}
					}
					igEndPopup();
				}

				igNextColumn();
			}

			{
				igNewLine();
				igText( "Active titlepic" );

				Image found = { };
				for( auto& entry : doomfileselector->SelectedPWADs() )
				{
					if( entry.titlepic.tex != nullptr )
					{
						found = entry.titlepic;
					}
				}

				if( found.tex == nullptr )
				{
					found = iwadselector->Selected().titlepic;
				}

				if( found.tex != nullptr )
				{
					ImVec2 imageavail;
					igGetContentRegionAvail( &imageavail );

					float_t scale = imageavail.x / found.width;

					constexpr ImVec4 tint = { 1, 1, 1, 1 };
					constexpr ImVec4 border = { 0, 0, 0, 0 };
					constexpr ImVec2 tl = { 0, 0 };
					constexpr ImVec2 br = { 1, 1 };

					ImVec2 imagesize = { found.width * scale, found.height * scale };

					igImage( I_TextureGetHandle( found.tex ), imagesize, tl, br, tint, border );
				}
			}

			igColumns( 1, nullptr, false );

			ImVec2 cursorpos;

			cursorpos.x = contentsize.x - backbuttonsize.x - framepadding;
			cursorpos.y = contentsize.y - backbuttonsize.y;

			igSetCursorPos( cursorpos );
			if( igButton( "Back", backbuttonsize ) )
			{
				PopPanel();
			}
		}

	private:
		std::shared_ptr< IWADSelector >							iwadselector;
		std::shared_ptr< DoomFileSelector >						doomfileselector;
		int32_t													dehselected;
		int32_t													pwadselected;
	};

	class FreeIWADPanel;

	class DownloadMoreMapsPanel : public LauncherPanel
	{
	public:
		DownloadMoreMapsPanel( std::shared_ptr< FreeIWADPanel > free, std::shared_ptr< IWADSelector >& iwad, std::shared_ptr< DoomFileSelector >& pwad )
			: LauncherPanel( "Launcher_DownloadMore")
			, freeiwads( free )
			, iwadselector( iwad )
			, doomfileselector( pwad )
		{
			filter = std::make_shared< IdgamesFilter >();

			auto doomfolder = std::make_shared< IdgamesBrowserPanel >( filter, iwadselector, doomfileselector, "levels/doom/", "Doom" );
			auto doom2folder = std::make_shared< IdgamesBrowserPanel >( filter, iwadselector, doomfileselector, "levels/doom2/", "Doom II" );

			idgamesfolders.push_back( doomfolder );
			idgamesfolders.push_back( doom2folder );
		}

	protected:
		virtual void RenderContents() override
		{
			constexpr ImVec2 backbuttonsize = { 50.f, 25.f };
			constexpr float_t framepadding = 10.f;
			constexpr ImVec2 foldertilesize = { 176.f, 40.f };
			constexpr float_t folderspacing = 10.f;
			constexpr float_t foldertotalwidth = foldertilesize.x * 2 + folderspacing;

			ImVec2 framesize;
			igGetContentRegionMax( &framesize );

			igPushFont( font_inconsolata_large );
			igCentreText( "Download more maps" );
			igPopFont();
			igNewLine();

			igPushFont( font_inconsolata_medium );
			igCentreText( "Browse the idgames archive" );
			igSpacing();

			ImVec2 cursor;
			igGetCursorPos( &cursor );

			cursor.x += ( framesize.x - foldertotalwidth ) * 0.5;
			igSetCursorPos( cursor );
			if( igButton( "Doom", foldertilesize ) )
			{
				PushPanel( idgamesfolders[ 0 ] );
			}
			cursor.x += foldertilesize.x + folderspacing;
			igSetCursorPos( cursor );
			if( igButton( "Doom II", foldertilesize ) )
			{
				PushPanel( idgamesfolders[ 1 ] );
			}
			igNewLine();
			igNewLine();

			igCentreText( "Need some IWADs?" );
			igSpacing();
			igCentreNextElement( foldertilesize.x );
			if( igButton( "Free IWADs", foldertilesize ) )
			{
				PushPanel( freeiwads );
			}
			igPopFont();

			cursor.x = framesize.x - backbuttonsize.x - framepadding;
			cursor.y = framesize.y - backbuttonsize.y;

			igSetCursorPos( cursor );
			if( igButton( "Back", backbuttonsize ) )
			{
				PopPanel();
			}
		}

	private:
		std::shared_ptr< IdgamesFilter >		filter;
		std::shared_ptr< FreeIWADPanel >		freeiwads;
		std::shared_ptr< IWADSelector >			iwadselector;
		std::shared_ptr< DoomFileSelector >		doomfileselector;

		std::vector< std::shared_ptr< IdgamesBrowserPanel > >	idgamesfolders;
	};

	class MainPanel : public LauncherPanel
	{
	public:
		MainPanel( LaunchOptions& options, std::shared_ptr< FreeIWADPanel >& free, std::shared_ptr< IWADSelector >& iwad, std::shared_ptr< DoomFileSelector >& pwad )
			: LauncherPanel( "Launcher_Main" )
			, readytolaunch( false )
			, iwadselector( iwad )
			, doomfileselector( pwad )
			, launchoptions( &options )
		{
			downloadmaps = std::make_shared< DownloadMoreMapsPanel >( free, iwadselector, doomfileselector );

			fileanddehselector = std::make_shared< FileSelectorPanel >( iwadselector, doomfileselector );
		}

		virtual ~MainPanel() { }

	protected:
		virtual void RenderContents() override
		{
			iwadselector->Render();

			ImVec2 framesize;
			igGetContentRegionMax( &framesize );

			// Column width doesn't really get committed until after the column end call.
			// As such, setting off then on fixes first frame visual glitches.
			igColumns( 2, "mainpanelcolumns", false );
			igSetColumnWidth( 0, framesize.x - 180 );
			igColumns( 1, "", false );

			igColumns( 2, "mainpanelcolumns", false );

			igText( "Executable version" );
			igSameLine( 0, -1 );
			igSetNextItemWidth( 150 );
			if( igBeginCombo( "", launchoptions->game_version < 0 ? "Limit removing" : GameVersions[ launchoptions->game_version ], ImGuiComboFlags_None ) )
			{
				if( igSelectableBool( "Limit removing", launchoptions->game_version < 0, ImGuiSelectableFlags_None, zero ) ) launchoptions->game_version = -1;

				for( int32_t index : iota( 0, arrlen( GameVersions ) ) )
				{
					if( igSelectableBool( GameVersions[ index ], launchoptions->game_version == index, ImGuiSelectableFlags_None, zero ) )
					{
						launchoptions->game_version = index;
					}
				}
				igEndCombo();
			}

			int32_t mode = iwadselector->Selected().game_mode;
			if( mode != commercial )
			{
				int32_t episode_count = mode == shareware ? 1 : mode == registered ? 3 : 4;

				igSetNextItemWidth( 50 );
				igSliderInt( "Episode", &launchoptions->start_episode, 0, episode_count, NULL, ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput );
				igSameLine( 0, 10 );
			}
			igSetNextItemWidth( 100 );
			igSliderInt( "Map", &launchoptions->start_map, 0, mode != commercial ? 9 : 32, NULL, ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput );

			igSetNextItemWidth( 50 );
			igSliderInt( "Start skill", &launchoptions->start_skill, -1, 5, NULL, ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput );

			igCheckbox( "Solo net", &launchoptions->solo_net );

			igCheckbox( "Fast monsters", &launchoptions->fast_monsters );
			igCheckbox( "No monsters", &launchoptions->no_monsters );
			igCheckbox( "Respawn monsters", &launchoptions->respawn_monsters );

			igCheckbox( "Don't load widepix", &launchoptions->no_widepix );

			igNextColumn();
			igText( "Selected files" );

			for( auto& entry : doomfileselector->SelectedPWADs() )
			{
				if( entry.titlepic.tex != nullptr )
				{
					ImVec2 imageavail;
					igGetContentRegionAvail( &imageavail );

					float_t scale = imageavail.x / entry.titlepic.width;

					constexpr ImVec4 tint = { 1, 1, 1, 1 };
					constexpr ImVec4 border = { 0, 0, 0, 0 };
					constexpr ImVec2 tl = { 0, 0 };
					constexpr ImVec2 br = { 1, 1 };

					ImVec2 imagesize = { entry.titlepic.width * scale, entry.titlepic.height * scale };

					igImage( I_TextureGetHandle( entry.titlepic.tex ), imagesize, tl, br, tint, border );
					igSpacing();

					break;
				}
			}

			igSpacing();

			ImVec2 frameavail;
			igGetContentRegionAvail( &frameavail );

			constexpr float_t framepadding = 10;
			constexpr float_t buttonheight = 25.f;
			constexpr ImVec2 playbuttonsize = { 50.f, buttonheight };
			constexpr ImVec2 pwadssize = { 160.f, buttonheight };
			constexpr ImVec2 idgamessize = { 140.f, buttonheight };

			int32_t id = ImGuiWindow_GetIDStr( igGetCurrentWindow(), "mainpanelfiles", nullptr );
			ImVec2 fileframesize = { frameavail.x, frameavail.y - buttonheight - 15 };

			if( igBeginChildFrame( id, fileframesize, ImGuiWindowFlags_NoResize ) )
			{
				if( !doomfileselector->SelectedDEHs().empty() )
				{
					for( auto& entry : doomfileselector->SelectedDEHs() )
					{
						igText( entry.filename.c_str() );
					}

					igSpacing();
				}
				for( auto& entry : doomfileselector->SelectedPWADs() )
				{
					igText( entry.filename.c_str() );
				}
			}
			igEndChildFrame();

			igNextColumn();

			igColumns( 1, nullptr, false );

			ImVec2 cursor;

			cursor = { framesize.x - framepadding - playbuttonsize.x - framepadding - pwadssize.x - framepadding - idgamessize.x, framesize.y - buttonheight };
			igSetCursorPos( cursor );
			if( igButton( "Download more maps", idgamessize ) )
			{
				PushPanel( downloadmaps );
			}

			cursor = { framesize.x - framepadding - playbuttonsize.x - framepadding - pwadssize.x, framesize.y - buttonheight };
			igSetCursorPos( cursor );
			if( igButton( "Change selected files", pwadssize ) )
			{
				PushPanel( fileanddehselector );
			}

			cursor = { framesize.x - framepadding - playbuttonsize.x, framesize.y - buttonheight };
			igSetCursorPos( cursor );

			if( igButton( "Play!", playbuttonsize ) )
			{
				readytolaunch = true;
				PopPanel();
			}
		}

	private:
		bool									readytolaunch;
		std::shared_ptr< IWADSelector >			iwadselector;
		std::shared_ptr< DoomFileSelector >		doomfileselector;
		std::shared_ptr< DownloadMoreMapsPanel >	downloadmaps;
		std::shared_ptr< FileSelectorPanel >	fileanddehselector;
		LaunchOptions*							launchoptions;
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
			, forcenoiwad( false )
			, hasrunpopulate( false )
			, filelistfinished( false )
			, launchoptions( default_launch_options )
		{
			iwadselector = std::make_shared< IWADSelector >();
			doomfileselector = std::make_shared< DoomFileSelector >();

			freeiwadpanel = std::make_shared< FreeIWADPanel >( (InitPanel*)this );
			mainpanel = std::make_shared< MainPanel >( launchoptions, freeiwadpanel, iwadselector, doomfileselector );
		}

		virtual ~InitPanel() { }

		inline void ForceNoIWAD()
		{
			forcenoiwad = true;
		}

		inline void Repopulate()
		{
			forcenoiwad = false;
			hasrunpopulate = false;
		}

		virtual void Enter() override
		{
			if( forcenoiwad )
			{
				return;
			}

			if( !hasrunpopulate || filelist.IWADs.empty() )
			{
				filelistfinished = false;
				jobs->AddJob( [ this ]()
				{
					filelist.PopulateDoomFileList();
					const char* defaultiwad = nullptr;
					int32_t iwadparam = M_CheckParmWithArgs( "-iwad", 1 );
					if( iwadparam > 0 )
					{
						defaultiwad = myargv[ iwadparam + 1 ];
					}

					std::span< const char* > fileparams = ParamArgs( "-file" );
					std::span< const char* > dehparams = ParamArgs( "-deh" );

					iwadselector->Setup( filelist.IWADs, defaultiwad );
					doomfileselector->SetupPWADs( filelist.PWADs, fileparams );
					doomfileselector->SetupDEHs( filelist.DEHs, dehparams );

					filelistfinished = true;
				} );
				hasrunpopulate = true;
			}
		}

		virtual void Update() override
		{
			if( forcenoiwad )
			{
				forcenoiwad = false;
				PopPanelsAndReplaceRoot( freeiwadpanel );
				return;
			}

			if( filelistfinished )
			{
				if( filelist.IWADs.empty() )
				{
					PopPanelsAndReplaceRoot( freeiwadpanel );
				}
				else
				{
					PopPanelsAndReplaceRoot( mainpanel );
				}
			}
		}

		INLINE IWADSelector* GetIWADSelector() { return iwadselector.get(); }
		INLINE DoomFileSelector* GetDoomFileSelector() { return doomfileselector.get(); }
		INLINE LaunchOptions& GetLaunchOptions() { return launchoptions; }

	protected:
		virtual void RenderContents() override
		{
			constexpr double_t pi2 = M_PI * 2.0;
			constexpr ImVec2 barsize = { 0.f, 15.f };

			igCentreNextElement( loading_size.x );
			igSpinner( loading_size, cycle_time );

			igRoundProgressBar( filelist.Progress(), barsize, 2.f );

			igCentreText( "Updating WAD" );
			igCentreText( "dictionary" );
		}

	private:
		bool								forcenoiwad;
		bool								hasrunpopulate;
		std::atomic< bool >					filelistfinished;
		launcher::DoomFileList				filelist;
		std::shared_ptr< IWADSelector >		iwadselector;
		std::shared_ptr< DoomFileSelector >	doomfileselector;
		std::shared_ptr< MainPanel >		mainpanel;
		std::shared_ptr< FreeIWADPanel >	freeiwadpanel;
		LaunchOptions						launchoptions;
	};

	class FreeIWADPanel : public LauncherPanel
	{
	public:
		FreeIWADPanel( InitPanel* init )
			: LauncherPanel ( "Launcher_NoIWAD" )
			, initpanel( init )
			, noiwadmode( false )
			, downloadingshareware( false )
			, downloadingfreedoom( false )
			, downloadinghacx( false )
			, sharewareextracted( false )
			, freedoomextracted( false )
			, hacxextracted( false )
			, downloadfinished( true )
			, currentlydoing( "Nothing" )
			, progress( 0 )
		{
		}

		INLINE bool SetNoIWADMode() { noiwadmode = true; }

		virtual void Enter() override
		{
			if( !noiwadmode )
			{
				sharewareextracted = ExtractedFolderExists( "iwads/doomshareware/" );
				freedoomextracted = ExtractedFolderExists( "iwads/freedoom/" );
				hacxextracted = ExtractedFolderExists( "iwads/hacx/" );
			}
		}

	protected:
		virtual void RenderContents() override
		{
			if( noiwadmode )
			{
				igPushFont( font_inconsolata_large );
				igCentreText( "No data found" );
				igPopFont();
				igNewLine();
				igNewLine();

				igPushFont( font_inconsolata_medium );
				igTextWrapped( "Rum and Raisin Doom needs a valid WAD file from one of the supported games to run.\n\nIf you do not have a WAD file, you can download one of these free files:" );
				igPopFont();
				igNewLine();
			}
			else
			{
				igPushFont( font_inconsolata_large );
				igCentreText( "Free IWAD downloads" );
				igPopFont();
				igNewLine();
				igNewLine();

				igPushFont( font_inconsolata_medium );
				igCentreText( "The following free IWADs are available for download:" );
				igPopFont();
				igNewLine();
			}

			constexpr ImVec2 buttonsize = { 200.f, 50.f };

			igPushFont( font_inconsolata_medium );

			HandleGame( "Doom shareware", "iwads/doomshareware/", shareware_idgames_url, true, true, downloadingshareware, sharewareextracted );
			HandleGame( "Freedoom", "iwads/freedoom/", freedoom_url, false, false, downloadingfreedoom, freedoomextracted );
			HandleGame( "HacX", "iwads/hacx/", hacx_idgames_url, true, false, downloadinghacx, hacxextracted );

			if( sharewareextracted || freedoomextracted || hacxextracted )
			{
				igNewLine();
				igCentreNextElement( buttonsize.x );

				bool disabled = !downloadfinished;
				igPushItemFlag( ImGuiItemFlags_Disabled, disabled );

				if( igButton( "Continue", buttonsize ) )
				{
					noiwadmode = false;
					initpanel->Repopulate();
					PopPanelsAndReplaceRoot( initpanel );
				}

				igPopItemFlag();
			}

			igPopFont();
		}

		void HandleGame( const char* name, const char* cachefoldername, const char* url, bool isidgames, bool isshareware, std::atomic< bool >& downloading, std::atomic< bool >& extracted )
		{
			constexpr ImVec2 buttonsize = { 250.f, 50.f };
			constexpr ImVec2 downloadingsize = { 450.f, 50.f };
			constexpr ImVec2 spinnersize = { 20.f, 50.f };

			if( downloading )
			{
				igPushFont( font_inconsolata );
				igCentreNextElement( downloadingsize.x );
				igDownloadProgressBar( cachefoldername, downloadingsize, spinnersize, 0.5, 20.f, currentlydoing.load(), progress.load() );
				igPopFont();
			}
			else if( extracted )
			{
				igCentreText( "%s downloaded!", name );
			}
			else
			{
				igCentreNextElement( buttonsize.x );

				bool disabled = !downloadfinished;
				igPushItemFlag( ImGuiItemFlags_Disabled, disabled );

				if( isidgames )
				{
					if( igButton( name, buttonsize ) )
					{
						OpenIdgamesLocationSelector( name );
					}

					const char* location = IdgamesLocationSelector( name );
					if( location )
					{
						PerformDownload( cachefoldername, std::string( location ) + url, isshareware, downloading, extracted );
					}
				}
				else
				{
					if( igButton( name, buttonsize ) )
					{
						PerformDownload( cachefoldername, url, isshareware, downloading, extracted );
					}
				}

				igPopItemFlag();
			}

			igSpacing();
		}

		bool ExtractedFolderExists( const char* cachefoldername )
	{
			std::string extractedloc = HOME_PATH + cache_extracted;
			extractedloc += cachefoldername;
			std::replace( extractedloc.begin(), extractedloc.end(), '/', DIR_SEPARATOR );
			std::filesystem::path extractedpath = extractedloc;

			return std::filesystem::exists( extractedpath );
		}

		void PerformDownload( const char* cachefoldername, const std::string& loc, bool isshareware, std::atomic< bool >& downloading, std::atomic< bool >& extracted )
		{
			if( !downloadfinished )
			{
				return;
			}

			currentlydoing = "Downloading file";
			downloadfinished = false;
			downloading = true;
			extracted = false;

			jobs->AddJob( [ this, cachefoldername, loc, isshareware, &downloading, &extracted ]()
			{
				std::string cachefolder = cachefoldername;

				std::filesystem::path filenamepath = loc;
				std::string filename = filenamepath.filename().string();

				std::string downloadloc = HOME_PATH + cache_download + cachefolder;
				std::replace( downloadloc.begin(), downloadloc.end(), '/', DIR_SEPARATOR );
				std::filesystem::path downloadpath = downloadloc;

				std::string extractedloc = HOME_PATH + cache_extracted + cachefolder;
				std::replace( extractedloc.begin(), extractedloc.end(), '/', DIR_SEPARATOR );
				std::filesystem::path extractedpath = extractedloc;

				std::vector< byte > data;
				int64_t filedate = 0;
				std::string error;

				auto progressfunc = std::function( [this]( ptrdiff_t current, ptrdiff_t total ) -> bool
				{
					progress = (double_t)current / (double_t)total;
					return true;
				} );

				progress = 0;
				bool successful = M_URLGetBytes( data, filedate, error, loc.c_str(), nullptr, progressfunc );
				if( !successful )
				{
					currentlydoing = "Failed to download";
					downloading = false;
					downloadfinished = true;
					return;
				}

				currentlydoing = "Writing file to disk";
				progress = 0;

				std::filesystem::create_directories( downloadpath );

				std::string downloadtozip = downloadloc + filename;
				FILE* zipfile = fopen( downloadtozip.c_str(), "wb" );
				fwrite( (void*)data.data(), sizeof( char ), data.size(), zipfile );
				fclose( zipfile );

				utimbuf zipfiletime = { filedate, filedate };
				utime( downloadtozip.c_str(), &zipfiletime );

				currentlydoing = isshareware ? "Extracting original archive" : "Extracting files";

				std::filesystem::create_directories( extractedpath );
				M_ZipExtractAllFromFile( downloadtozip.c_str(), extractedloc.c_str(), progressfunc );

				if( isshareware )
				{
					std::string fullextractedloc = HOME_PATH + cache_extracted + cachefolder + "final" DIR_SEPARATOR_S;
					std::replace( fullextractedloc.begin(), fullextractedloc.end(), '/', DIR_SEPARATOR );
					std::filesystem::path fullextractedpath = fullextractedloc;

					currentlydoing = "Extracting final files";
					std::filesystem::create_directories( fullextractedpath );
					M_ZipExtractFromICE( extractedloc.c_str(), fullextractedloc.c_str(), progressfunc );
				}

				currentlydoing = "Nothing";
				extracted = true;
				downloading = false;
				downloadfinished = true;
			} );
		}

	private:
		InitPanel*							initpanel;
		bool								noiwadmode;
		std::atomic< bool >					downloadingshareware;
		std::atomic< bool >					downloadingfreedoom;
		std::atomic< bool >					downloadinghacx;
		std::atomic< bool >					sharewareextracted;
		std::atomic< bool >					freedoomextracted;
		std::atomic< bool >					hacxextracted;
		std::atomic< bool >					downloadfinished;
		std::atomic< const char* >			currentlydoing;
		std::atomic< double_t >				progress;
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

	SDL_Window* window = I_GetWindow();
	SDL_GLContext glcontext = SDL_GL_GetCurrentContext();
	SDL_GL_SetAttribute( SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1 );
	launcher::jobs_context = SDL_GL_CreateContext( window );
	SDL_GL_MakeCurrent( window, glcontext );

	launcher::jobs = new JobThread;
	launcher::jobs->AddJob( []()
		{
			SDL_GL_MakeCurrent( I_GetWindow(), launcher::jobs_context );
			M_URLInit();
		} );

	std::shared_ptr< launcher::InitPanel > initpanel = std::make_shared< launcher::InitPanel >();
	if( M_CheckParm( "-forcenoiwad" ) )
	{
		initpanel->ForceNoIWAD();
	}

	launcher::PushPanel( initpanel );

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
		parameters += " -iwad \"" + initpanel->GetIWADSelector()->Selected().full_path + "\"";
	}

	if( !initpanel->GetDoomFileSelector()->SelectedPWADs().empty() )
	{
		int32_t filecount = 0;
		int32_t mergecount = 0;
		for( launcher::DoomFileEntry& curr : initpanel->GetDoomFileSelector()->SelectedPWADs() )
		{
			if( curr.should_merge ) ++mergecount;
			else ++filecount;
		}

		if( mergecount )
		{
			parameters += " -merge";
			for( launcher::DoomFileEntry& curr : initpanel->GetDoomFileSelector()->SelectedPWADs() )
			{
				if( curr.should_merge ) parameters += " \"" + curr.full_path + "\"";
			}
		}

		if( filecount )
		{
			parameters += " -file";
			for( launcher::DoomFileEntry& curr : initpanel->GetDoomFileSelector()->SelectedPWADs() )
			{
				if( !curr.should_merge ) parameters += " \"" + curr.full_path + "\"";
			}
		}
	}

	if( !initpanel->GetDoomFileSelector()->SelectedDEHs().empty() )
	{
		parameters += " -deh";
		for( launcher::DoomFileEntry& curr : initpanel->GetDoomFileSelector()->SelectedDEHs() )
		{
			parameters += " \"" + curr.full_path + "\"";
		}
	}

	launcher::LaunchOptions& options = initpanel->GetLaunchOptions();

	if( options.game_version >= 0 )
	{
		parameters += " -gameversion ";
		parameters += GameVersionsCommand[ options.game_version ];
	}

	if( options.start_map > 0 )
	{
		if( initpanel->GetIWADSelector()->Selected().game_mode == commercial )
		{
			parameters += " -warp " + to< std::string >( options.start_map );
		}
		else if( options.start_episode > 0 )
		{
			parameters += " -warp " + to< std::string >( options.start_episode ) + " " + to< std::string >( options.start_map );
		}
	}

	if( options.start_skill >= 0 )
	{
		parameters += " -skill " + to< std::string >( options.start_skill );
	}

	if( options.solo_net )
	{
		parameters += " -solo-net";
	}

	if( options.fast_monsters )
	{
		parameters += " fast";
	}

	if( options.no_monsters )
	{
		parameters += " -nomonsters";
	}

	if( options.respawn_monsters )
	{
		parameters += " -respawn";
	}

	if( options.no_widepix )
	{
		parameters += " -nowidepix";
	}

	if( options.record_demo && options.demo_name[ 0 ] != 0 )
	{
	}

	SDL_GL_DeleteContext( launcher::jobs_context );
#endif // USE_IMGUI

	return parameters;
}
