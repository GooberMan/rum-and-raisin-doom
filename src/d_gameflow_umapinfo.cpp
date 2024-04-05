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
// DESCRIPTION: Sets up gameflow for UMAPINFO lumps

#include "d_gameflow.h"
#include "m_fixed.h"
#include "doomstat.h"
#include "m_container.h"
#include "m_conv.h"
#include "w_wad.h"

#include <iomanip>


typedef enum class setboolean_e
{
	None,
	False,
	True
} setboolean_t, setboolean;

#define ThingType( x ) { #x, __COUNTER__ }
namespace umapinfo
{
	static std::map< DoomString, int32_t > type_name_lookup =
	{
		ThingType( DoomPlayer ),
		ThingType( ZombieMan ),
		ThingType( ShotgunGuy ),
		ThingType( Archvile ),
		ThingType( ArchvileFire ),
		ThingType( Revenant ),
		ThingType( RevenantTracer ),
		ThingType( RevenantTracerSmoke ),
		ThingType( Fatso ),
		ThingType( FatShot ),
		ThingType( ChaingunGuy ),
		ThingType( DoomImp ),
		ThingType( Demon ),
		ThingType( Spectre ),
		ThingType( Cacodemon ),
		ThingType( BaronOfHell ),
		ThingType( BaronBall ),
		ThingType( HellKnight ),
		ThingType( LostSoul ),
		ThingType( SpiderMastermind ),
		ThingType( Arachnotron ),
		ThingType( Cyberdemon ),
		ThingType( PainElemental ),
		ThingType( WolfensteinSS ),
		ThingType( CommanderKeen ),
		ThingType( BossBrain ),
		ThingType( BossEye ),
		ThingType( BossTarget ),
		ThingType( SpawnShot ),
		ThingType( SpawnFire ),
		ThingType( ExplosiveBarrel ),
		ThingType( DoomImpBall ),
		ThingType( CacodemonBall ),
		ThingType( Rocket ),
		ThingType( PlasmaBall ),
		ThingType( BFGBall ),
		ThingType( ArachnotronPlasma ),
		ThingType( BulletPuff ),
		ThingType( Blood ),
		ThingType( TeleportFog ),
		ThingType( ItemFog ),
		ThingType( TeleportDest ),
		ThingType( BFGExtra ),
		ThingType( GreenArmor ),
		ThingType( BlueArmor ),
		ThingType( HealthBonus ),
		ThingType( ArmorBonus ),
		ThingType( BlueCard ),
		ThingType( RedCard ),
		ThingType( YellowCard ),
		ThingType( YellowSkull ),
		ThingType( RedSkull ),
		ThingType( BlueSkull ),
		ThingType( Stimpack ),
		ThingType( Medikit ),
		ThingType( Soulsphere ),
		ThingType( InvulnerabilitySphere ),
		ThingType( Berserk ),
		ThingType( BlurSphere ),
		ThingType( RadSuit ),
		ThingType( Allmap ),
		ThingType( Infrared ),
		ThingType( Megasphere ),
		ThingType( Clip ),
		ThingType( ClipBox ),
		ThingType( RocketAmmo ),
		ThingType( RocketBox ),
		ThingType( Cell ),
		ThingType( CellPack ),
		ThingType( Shell ),
		ThingType( ShellBox ),
		ThingType( Backpack ),
		ThingType( BFG9000 ),
		ThingType( Chaingun ),
		ThingType( Chainsaw ),
		ThingType( RocketLauncher ),
		ThingType( PlasmaRifle ),
		ThingType( Shotgun ),
		ThingType( SuperShotgun ),
		ThingType( TechLamp ),
		ThingType( TechLamp2 ),
		ThingType( Column ),
		ThingType( TallGreenColumn ),
		ThingType( ShortGreenColumn ),
		ThingType( TallRedColumn ),
		ThingType( ShortRedColumn ),
		ThingType( SkullColumn ),
		ThingType( HeartColumn ),
		ThingType( EvilEye ),
		ThingType( FloatingSkull ),
		ThingType( TorchTree ),
		ThingType( BlueTorch ),
		ThingType( GreenTorch ),
		ThingType( RedTorch ),
		ThingType( ShortBlueTorch ),
		ThingType( ShortGreenTorch ),
		ThingType( ShortRedTorch ),
		ThingType( Stalagtite ),
		ThingType( TechPillar ),
		ThingType( CandleStick ),
		ThingType( Candelabra ),
		ThingType( BloodyTwitch ),
		ThingType( Meat2 ),
		ThingType( Meat3 ),
		ThingType( Meat4 ),
		ThingType( Meat5 ),
		ThingType( NonsolidMeat2 ),
		ThingType( NonsolidMeat4 ),
		ThingType( NonsolidMeat3 ),
		ThingType( NonsolidMeat5 ),
		ThingType( NonsolidTwitch ),
		ThingType( DeadCacodemon ),
		ThingType( DeadMarine ),
		ThingType( DeadZombieMan ),
		ThingType( DeadDemon ),
		ThingType( DeadLostSoul ),
		ThingType( DeadDoomImp ),
		ThingType( DeadShotgunGuy ),
		ThingType( GibbedMarine ),
		ThingType( GibbedMarineExtra ),
		ThingType( HeadsOnAStick ),
		ThingType( Gibs ),
		ThingType( HeadOnAStick ),
		ThingType( HeadCandles ),
		ThingType( DeadStick ),
		ThingType( LiveStick ),
		ThingType( BigTree ),
		ThingType( BurningBarrel ),
		ThingType( HangNoGuts ),
		ThingType( HangBNoBrain ),
		ThingType( HangTLookingDown ),
		ThingType( HangTSkull ),
		ThingType( HangTLookingUp ),
		ThingType( HangTNoBrain ),
		ThingType( ColonGibs ),
		ThingType( SmallBloodPool ),
		ThingType( BrainStem ),
		ThingType( PointPusher ),
		ThingType( PointPuller ),
		ThingType( MBFHelperDog ),
		ThingType( PlasmaBall1 ),
		ThingType( PlasmaBall2 ),
		ThingType( EvilSceptre ),
		ThingType( UnholyBible ),
		ThingType( MusicChanger ),
		ThingType( Deh_Actor_145 ),
		ThingType( Deh_Actor_146 ),
		ThingType( Deh_Actor_147 ),
		ThingType( Deh_Actor_148 ),
		ThingType( Deh_Actor_149 ),
		ThingType( Deh_Actor_150 ),
		ThingType( Deh_Actor_151 ),
		ThingType( Deh_Actor_152 ),
		ThingType( Deh_Actor_153 ),
		ThingType( Deh_Actor_154 ),
		ThingType( Deh_Actor_155 ),
		ThingType( Deh_Actor_156 ),
		ThingType( Deh_Actor_157 ),
		ThingType( Deh_Actor_158 ),
		ThingType( Deh_Actor_169 ),
		ThingType( Deh_Actor_160 ),
		ThingType( Deh_Actor_161 ),
		ThingType( Deh_Actor_162 ),
		ThingType( Deh_Actor_163 ),
		ThingType( Deh_Actor_164 ),
		ThingType( Deh_Actor_165 ),
		ThingType( Deh_Actor_166 ),
		ThingType( Deh_Actor_167 ),
		ThingType( Deh_Actor_168 ),
		ThingType( Deh_Actor_169 ),
		ThingType( Deh_Actor_170 ),
		ThingType( Deh_Actor_171 ),
		ThingType( Deh_Actor_172 ),
		ThingType( Deh_Actor_173 ),
		ThingType( Deh_Actor_174 ),
		ThingType( Deh_Actor_175 ),
		ThingType( Deh_Actor_176 ),
		ThingType( Deh_Actor_177 ),
		ThingType( Deh_Actor_178 ),
		ThingType( Deh_Actor_179 ),
		ThingType( Deh_Actor_180 ),
		ThingType( Deh_Actor_181 ),
		ThingType( Deh_Actor_182 ),
		ThingType( Deh_Actor_183 ),
		ThingType( Deh_Actor_184 ),
		ThingType( Deh_Actor_185 ),
		ThingType( Deh_Actor_186 ),
		ThingType( Deh_Actor_187 ),
		ThingType( Deh_Actor_188 ),
		ThingType( Deh_Actor_189 ),
		ThingType( Deh_Actor_190 ),
		ThingType( Deh_Actor_191 ),
		ThingType( Deh_Actor_192 ),
		ThingType( Deh_Actor_193 ),
		ThingType( Deh_Actor_194 ),
		ThingType( Deh_Actor_195 ),
		ThingType( Deh_Actor_196 ),
		ThingType( Deh_Actor_197 ),
		ThingType( Deh_Actor_198 ),
		ThingType( Deh_Actor_199 ),
		ThingType( Deh_Actor_200 ),
		ThingType( Deh_Actor_201 ),
		ThingType( Deh_Actor_202 ),
		ThingType( Deh_Actor_203 ),
		ThingType( Deh_Actor_204 ),
		ThingType( Deh_Actor_205 ),
		ThingType( Deh_Actor_206 ),
		ThingType( Deh_Actor_207 ),
		ThingType( Deh_Actor_208 ),
		ThingType( Deh_Actor_209 ),
		ThingType( Deh_Actor_210 ),
		ThingType( Deh_Actor_211 ),
		ThingType( Deh_Actor_212 ),
		ThingType( Deh_Actor_213 ),
		ThingType( Deh_Actor_214 ),
		ThingType( Deh_Actor_215 ),
		ThingType( Deh_Actor_216 ),
		ThingType( Deh_Actor_217 ),
		ThingType( Deh_Actor_218 ),
		ThingType( Deh_Actor_219 ),
		ThingType( Deh_Actor_220 ),
		ThingType( Deh_Actor_221 ),
		ThingType( Deh_Actor_222 ),
		ThingType( Deh_Actor_223 ),
		ThingType( Deh_Actor_224 ),
		ThingType( Deh_Actor_225 ),
		ThingType( Deh_Actor_226 ),
		ThingType( Deh_Actor_227 ),
		ThingType( Deh_Actor_228 ),
		ThingType( Deh_Actor_229 ),
		ThingType( Deh_Actor_230 ),
		ThingType( Deh_Actor_231 ),
		ThingType( Deh_Actor_232 ),
		ThingType( Deh_Actor_233 ),
		ThingType( Deh_Actor_234 ),
		ThingType( Deh_Actor_235 ),
		ThingType( Deh_Actor_236 ),
		ThingType( Deh_Actor_237 ),
		ThingType( Deh_Actor_238 ),
		ThingType( Deh_Actor_239 ),
		ThingType( Deh_Actor_240 ),
		ThingType( Deh_Actor_241 ),
		ThingType( Deh_Actor_242 ),
		ThingType( Deh_Actor_243 ),
		ThingType( Deh_Actor_244 ),
		ThingType( Deh_Actor_245 ),
		ThingType( Deh_Actor_246 ),
		ThingType( Deh_Actor_247 ),
		ThingType( Deh_Actor_248 ),
		ThingType( Deh_Actor_249 ),
	};
}

namespace umapinfo
{
	typedef struct bossaction_s
	{
		int32_t				thingtype;
		int32_t				linespecial;
		int32_t				tag;
	} bossaction_t;

	typedef struct map_s
	{
		DoomString			mapname;
		DoomString			levelname;
		DoomString			author;
		DoomString			label;
		bool				labelclear;
		DoomString			levelpic;
		DoomString			next;
		DoomString			nextsecret;
		DoomString			skytexture;
		DoomString			music;
		DoomString			exitpic;
		DoomString			enterpic;
		int32_t				partime;
		setboolean_t		endgame;
		DoomString			endpic;
		setboolean_t		endbunny;
		setboolean_t		endcast;
		setboolean_t		nointermission;
		DoomString			intertext;
		bool				intertextclear;
		DoomString			intertextsecret;
		bool				intertextsecretclear;
		DoomString			interbackdrop;
		DoomString			intermusic;
		DoomString			episodepatch;
		DoomString			episodename;
		DoomString			episodekey;
		setboolean_t		episodeclear;
		setboolean_t		episode;
		std::vector< bossaction_t > bossactions;
		bool				bossactionclear;

		DoomString			full_name;
		int32_t				episode_num;
		int32_t				map_num;
	} map_t;

	static std::vector< map_t >			maps;
}

// This needs to live in a header somewhere... and I think we need to 100% opt-in to it, at least while we mix C and C++ code everywhere

// You can use std::enable_if in earlier C++ versions if required...
template< typename _type >
requires std::is_enum_v< _type >
constexpr auto operator|( _type lhs, _type rhs )
{
	using underlying = std::underlying_type_t< _type >;
	return (_type)( (underlying)lhs | (underlying)rhs );
}

constexpr auto EmptyFlowString()
{
	return flowstring_t();
}

constexpr auto FlowString( const char* name, flowstringflags_t flags = FlowString_None )
{
	flowstring_t str = { name, flags };
	return str;
}

constexpr auto FlowString( DoomString& name, flowstringflags_t flags = FlowString_None )
{
	return FlowString( name.c_str(), flags );
}

constexpr auto RuntimeFlowString( const char* name )
{
	flowstring_t lump = { name, FlowString_Dehacked | FlowString_RuntimeGenerated };
	return lump;
}

constexpr auto SetBoolean( const DoomString& val )
{
	return val == "false" ? setboolean::False
		: val == "true" ? setboolean::True
		: setboolean::None;
}

static void Sanitize( DoomString& currline )
{
	std::replace( currline.begin(), currline.end(), '\t', ' ' );
	currline.erase( std::remove( currline.begin(), currline.end(), '\r' ), currline.end() );
	size_t startpos = currline.find_first_not_of( " " );
	if( startpos != DoomString::npos && startpos > 0 )
	{
		currline = currline.substr( startpos );
	}
}

static void ToLower( DoomString& str )
{
	std::transform( str.begin(), str.end(), str.begin(),
					[]( unsigned char c ){ return std::tolower( c ); } );
}

static void HandleIntertext( DoomString& targetstring, bool& targetclear
							, DoomStringStream& lumpstream
							, DoomString& currline
							, DoomIStringStream& currlinestream
							, DoomString& content )
{
	if( content == "clear" )
	{
		targetclear = true;
	}
	else
	{
		targetstring = content;

		DoomString comma;
		currlinestream >> std::quoted( comma );
		while( comma == "," && std::getline( lumpstream, currline ) )
		{
			comma.clear();

			Sanitize( currline );

			currlinestream = DoomIStringStream( currline );

			currlinestream >> std::quoted( content );
			currlinestream >> std::quoted( comma );

			targetstring += "\n";
			targetstring += content;
		}
	}
}

static void ParseMap( DoomStringStream& lumpstream, DoomString& currline )
{
	umapinfo::map_t& newmap = *umapinfo::maps.insert( umapinfo::maps.end(), umapinfo::map_t() );

	DoomString lhs;
	DoomString middle;
	DoomString rhs;

	do
	{
		Sanitize( currline );

		DoomIStringStream currlinestream( currline );

		currlinestream >> std::quoted( lhs );
		currlinestream >> std::quoted( middle );
		currlinestream >> std::quoted( rhs );

		ToLower( lhs );

		if( lhs == "}" )
		{
			break;
		}
		else if( lhs == "map" )
		{
			newmap.mapname = middle;
		}
		else if( lhs == "levelname" )
		{
			newmap.levelname = rhs;
		}
		else if( lhs == "label" )
		{
			if( rhs == "clear" )
			{
				newmap.labelclear = true;
			}
			else
			{
				newmap.label = rhs;
			}
		}
		else if( lhs == "author" )
		{
			newmap.author = rhs;
		}
		else if( lhs == "levelpic" )
		{
			newmap.levelpic = rhs;
		}
		else if( lhs == "next" )
		{
			newmap.next = rhs;
		}
		else if( lhs == "nextsecret" )
		{
			newmap.nextsecret = rhs;
		}
		else if( lhs == "skytexture" )
		{
			newmap.skytexture = rhs;
		}
		else if( lhs == "music" )
		{
			newmap.music = rhs;
		}
		else if( lhs == "exitpic" )
		{
			newmap.exitpic = rhs;
		}
		else if( lhs == "enterpic" )
		{
			newmap.enterpic = rhs;
		}
		else if( lhs == "partime" )
		{
			newmap.partime = to< int32_t >( rhs );
		}
		else if( lhs == "endgame" )
		{
			newmap.endgame = SetBoolean( rhs );
		}
		else if( lhs == "endpic" )
		{
			newmap.endpic = rhs;
		}
		else if( lhs == "endbunny" )
		{
			newmap.endbunny = SetBoolean( rhs );
		}
		else if( lhs == "endcast" )
		{
			newmap.endcast = SetBoolean( rhs );
		}
		else if( lhs == "nointermission" )
		{
			newmap.nointermission = SetBoolean( rhs );
		}
		else if( lhs == "intertext" )
		{
			HandleIntertext( newmap.intertext, newmap.intertextclear, lumpstream, currline, currlinestream, rhs );
		}
		else if( lhs == "intertextsecret" )
		{
			HandleIntertext( newmap.intertextsecret, newmap.intertextsecretclear, lumpstream, currline, currlinestream, rhs );
		}
		else if( lhs == "interbackdrop" )
		{
			newmap.interbackdrop = rhs;
		}
		else if( lhs == "intermusic" )
		{
			newmap.intermusic = rhs;
		}
		else if( lhs == "episode" )
		{
			if( rhs == "clear" )
			{
				newmap.episodeclear = setboolean::True;
			}
			else
			{
				DoomString epname;
				DoomString comma;
				DoomString key;

				currlinestream >> std::quoted( comma );
				currlinestream >> std::quoted( epname );
				currlinestream >> std::quoted( comma );
				currlinestream >> std::quoted( key );

				newmap.episode = setboolean::True;
				newmap.episodepatch = rhs;
				newmap.episodename = epname;
				newmap.episodekey = key;
			}
		}
		else if( lhs == "bossaction" )
		{
			if( rhs == "clear" )
			{
				newmap.bossactionclear = true;
			}
			else
			{
				auto found = umapinfo::type_name_lookup.find( rhs );
				if( found != umapinfo::type_name_lookup.end() )
				{
					DoomString linetype;
					DoomString tag;

					currlinestream >> std::quoted( linetype );
					currlinestream >> std::quoted( tag );

					umapinfo::bossaction_t& action = *newmap.bossactions.insert( newmap.bossactions.end(), umapinfo::bossaction_t() );

					action.thingtype = found->second;
					action.linespecial = to< int32_t >( linetype );
					action.tag = to< int32_t >( tag );
				}
			}
		}
	} while( std::getline( lumpstream, currline ) );

}

typedef struct umapinfo_gameinfo_s
{
	std::map< int32_t, episodeinfo_t >				episodes;
	std::map< DoomString, mapinfo_t >					maps;
	std::map< DoomString, interlevel_t >				interlevels;
	std::map< DoomString, intermission_t >			intermission_normal;
	std::map< DoomString, intermission_t >			intermission_secret;

	std::map< int32_t, std::vector< mapinfo_t* > >	episodemaps;
	std::map< DoomString, std::vector< bossaction_t > > mapbossactions;

	std::map< DoomString, endgame_t >					endgames;
	std::map< DoomString, intermission_t >			intermissions;
	std::map< DoomString, intermission_t >			secretintermissions;

	std::vector< episodeinfo_t* >								episodelist;

	gameflow_t													game;
} umapinfo_gameinfo_t;

static umapinfo_gameinfo_t umapinfogame;

static DoomString GetMapName( mapinfo_t* map )
{
	return AsDoomString( map->data_lump, map->episode->episode_num, map->map_num );
}

static void BuildNewGameInfo()
{
	// Instead of being a data container format, UMAPINFO is more of a command system
	// for data modification. So we have to always look up existing data and conditionally
	// modify data only if it's defined. This function's gonna get big and messy.

	bool clearepisodes = std::find_if( umapinfo::maps.begin(), umapinfo::maps.end(), []( auto& v ) { return v.episodeclear == setboolean::True; } ) != umapinfo::maps.end();
	int32_t newepisodenum = 0;
	int32_t newmapnum = 0;

	// Copy over old episode and map infos
	for( auto& epi : std::span( current_game->episodes, current_game->num_episodes ) )
	{
		for( auto& map : std::span( epi->all_maps, epi->num_maps ) )
		{
			DoomString mapname = GetMapName( map );
			mapinfo_t& copiedmap = umapinfogame.maps[ mapname ];
			copiedmap = *map;
			
			if( map->endgame != nullptr )
			{
				umapinfogame.endgames[ mapname ] = *map->endgame;
				if( map->endgame->intermission != nullptr )
				{
					umapinfogame.intermissions[ mapname ] = *map->endgame->intermission;
				}
				else if( map->next_map_intermission )
				{
					umapinfogame.intermissions[ mapname ] = *map->next_map_intermission;
				}

				if( map->secret_map_intermission )
				{
					umapinfogame.secretintermissions[ mapname ] = *map->secret_map_intermission;
				}
			}
		}

		if( !clearepisodes )
		{
			episodeinfo_t& newepisode = umapinfogame.episodes[ epi->episode_num ];
			newepisode = *epi;
			newepisodenum = M_MAX( newepisodenum, epi->episode_num );
			umapinfogame.episodelist.push_back( &newepisode );

			for( auto& map : std::span( epi->all_maps, epi->num_maps ) )
			{
				mapinfo_t& copiedmap = umapinfogame.maps[ GetMapName( map ) ];
				umapinfogame.episodemaps[ epi->episode_num ].push_back( &copiedmap );
			}
		}
	}

	for( auto& map : umapinfo::maps )
	{
		bool setfirstmap = false;
		if( map.episode == setboolean::True )
		{
			if( map.mapname.size() >= 2
				&& std::toupper( map.mapname[0] ) == 'E'
				&& std::isdigit( map.mapname[1] ) )
			{
				newepisodenum = map.mapname[1] - '0' - 1;
			}
			map.episode_num = ++newepisodenum;
			newmapnum = 0;

			episodeinfo_t& newepisode = umapinfogame.episodes[ map.episode_num ];
			newepisode.name = FlowString( map.episodename );
			newepisode.name_patch_lump = FlowString( map.episodepatch );
			newepisode.episode_num = map.episode_num;
			setfirstmap = true;
			umapinfogame.episodelist.push_back( &newepisode );
		}
		map.episode_num = newepisodenum;
		map.full_name = map.label + map.levelname;

		episodeinfo_t& mapepisode = umapinfogame.episodes[ map.episode_num ];
		mapinfo_t& newmap = umapinfogame.maps[ map.mapname ];

		if( newmap.map_num > 0 )
		{
			map.map_num = newmap.map_num;
		}
		else
		{
			map.map_num = ++newmapnum;
		}

		if( setfirstmap ) umapinfogame.episodes[ map.episode_num ].first_map = &newmap;

		auto& episodemaps = umapinfogame.episodemaps[ mapepisode.episode_num ];
		bool addtolist = std::find_if( episodemaps.begin(), episodemaps.end(), [ &newmap ]( auto& v ) { return v == &newmap; } ) == episodemaps.end();
		if( addtolist )
		{
			episodemaps.push_back( &newmap );
			mapepisode.highest_map_num = M_MAX( mapepisode.highest_map_num, map.map_num );
		}

		newmap.data_lump = FlowString( map.mapname );
		newmap.name = FlowString( map.full_name );
		newmap.name_patch_lump = FlowString( map.levelpic );
		newmap.authors = FlowString( map.author );
		newmap.episode = &mapepisode;
		newmap.map_num = map.map_num;
		// newmap.map_flags
		if( !map.music.empty() ) newmap.music_lump[ 0 ] = FlowString( map.music );
		if( !map.skytexture.empty() ) newmap.sky_texture = FlowString( map.skytexture );
		if( map.partime ) newmap.par_time = map.partime;
		if( !map.next.empty() ) newmap.next_map = &umapinfogame.maps[ map.next ];
		if( !map.nextsecret.empty() )
		{
			newmap.secret_map = &umapinfogame.maps[ map.nextsecret ];
			if( newmap.secret_map != newmap.next_map )
			{
				newmap.secret_map->map_flags = newmap.map_flags | Map_Secret;
			}
		}

		if( map.bossactionclear )
		{
			newmap.boss_actions = nullptr;
			newmap.num_boss_actions = 0;
		}
		if( !map.bossactions.empty() )
		{
			std::vector< bossaction_t >& mapactions = umapinfogame.mapbossactions[ map.mapname ];

			for( auto& sourceaction : map.bossactions )
			{
				bossaction_t& targetaction = *mapactions.insert( mapactions.end(), bossaction_t() );
				targetaction.thing_type = sourceaction.thingtype;
				targetaction.mbf21_flag_type = 0;
				targetaction.line_special = sourceaction.linespecial;
				targetaction.tag = sourceaction.tag;
			}

			newmap.boss_actions = mapactions.data();
			newmap.num_boss_actions = mapactions.size();
		}

		if( map.endgame == setboolean::False )
		{
			newmap.endgame = nullptr;
			newmap.map_flags = (mapflags_t)( newmap.map_flags & ~(int32_t)Map_GenericEndOfGame );
		}
		else if( map.endgame == setboolean::True )
		{
			newmap.endgame = &umapinfogame.endgames[ map.mapname ];
			newmap.map_flags = newmap.map_flags | Map_GenericEndOfGame;
		}

		if( !map.endpic.empty() )
		{
			newmap.endgame = &umapinfogame.endgames[ map.mapname ];
			newmap.map_flags = newmap.map_flags | Map_GenericEndOfGame;
			newmap.endgame->type = EndGame_Pic;
			newmap.endgame->primary_image_lump = FlowString( map.endpic );
			newmap.endgame->secondary_image_lump = EmptyFlowString();
			newmap.endgame->music_lump = FlowString( gamemode == commercial ? "read_m" : "victor" );
		}

		if( map.endbunny == setboolean::True )
		{
			newmap.endgame = &umapinfogame.endgames[ map.mapname ];
			newmap.map_flags = newmap.map_flags | Map_GenericEndOfGame;
			newmap.endgame->type = EndGame_Bunny;
			newmap.endgame->primary_image_lump = FlowString( "PFUB2" );
			newmap.endgame->secondary_image_lump = FlowString( "PFUB1" );
			newmap.endgame->music_lump = RuntimeFlowString( "bunny" );
		}

		if( map.endcast == setboolean::True )
		{
			newmap.endgame = &umapinfogame.endgames[ map.mapname ];
			newmap.map_flags = newmap.map_flags | Map_GenericEndOfGame;
			newmap.endgame->type = EndGame_Cast;
			newmap.endgame->primary_image_lump = EmptyFlowString( );
			newmap.endgame->secondary_image_lump = EmptyFlowString( );
			newmap.endgame->music_lump = RuntimeFlowString( "evil" );
		}

		if( map.nointermission == setboolean::True && newmap.endgame != nullptr )
		{
			newmap.map_flags = newmap.map_flags | Map_NoInterlevel;
			newmap.interlevel_finished = newmap.interlevel_entering = nullptr;
		}

		if( !map.exitpic.empty() )
		{
			interlevel_t* newinter = newmap.interlevel_finished = &umapinfogame.interlevels[ map.exitpic ];
			newinter->type = Interlevel_None;
			newinter->music_lump = RuntimeFlowString( ( gamemode == commercial ) ? "dm2int" : "inter" );
			newinter->background_lump = FlowString( map.exitpic );

			if( map.enterpic.empty() )
			{
				newmap.interlevel_entering = newinter;
			}
		}

		if( !map.enterpic.empty() )
		{
			interlevel_t* newinter = newmap.interlevel_entering= &umapinfogame.interlevels[ map.enterpic ];
			newinter->type = Interlevel_None;
			newinter->music_lump = RuntimeFlowString( ( gamemode == commercial ) ? "dm2int" : "inter" );
			newinter->background_lump = FlowString( map.enterpic );
		}

		intermission_t*& next_map_intermission = ( newmap.endgame != nullptr ) ? newmap.endgame->intermission : newmap.next_map_intermission;
		if( map.intertextclear )
		{
			next_map_intermission = nullptr;
		}

		if( map.intertextsecretclear )
		{
			newmap.secret_map_intermission = nullptr;
		}

		if( !map.intertext.empty() )
		{
			intermission_t& newinter = umapinfogame.intermissions[ map.mapname ];
			intermissiontype_t type = ( gamemode != commercial && newmap.endgame != nullptr ) ? Intermission_None : Intermission_Skippable;
			newinter.type = type;
			newinter.text = FlowString( map.intertext );

			next_map_intermission = &newinter;
		}

		if( !map.intertextsecret.empty() )
		{
			intermission_t& newinter = umapinfogame.secretintermissions[ map.mapname ];
			intermissiontype_t type = ( gamemode != commercial && newmap.endgame != nullptr ) ? Intermission_None : Intermission_Skippable;
			newinter.type = type;
			newinter.text = FlowString( map.intertextsecret );

			newmap.secret_map_intermission = &newinter;
		}

		if( !map.interbackdrop.empty() )
		{
			if( next_map_intermission ) next_map_intermission->background_lump = FlowString( map.interbackdrop );
			if( newmap.secret_map_intermission ) newmap.secret_map_intermission->background_lump = FlowString( map.interbackdrop );
		}

		if( !map.intermusic.empty () )
		{
			if( next_map_intermission ) next_map_intermission->music_lump = FlowString( map.intermusic );
			if( newmap.secret_map_intermission ) newmap.secret_map_intermission->music_lump = FlowString( map.intermusic );
		}

		if( next_map_intermission )
		{
			if( next_map_intermission->background_lump.val == nullptr ) next_map_intermission->background_lump = FlowString( "FLOOR4_8" );
			if( next_map_intermission->music_lump.val == nullptr ) next_map_intermission->music_lump = RuntimeFlowString( ( gamemode == commercial ) ? "read_m" : "victor" );
		}
		if( newmap.secret_map_intermission)
		{
			if( newmap.secret_map_intermission->background_lump.val == nullptr ) newmap.secret_map_intermission->background_lump = FlowString( "FLOOR4_8" );
			if( newmap.secret_map_intermission->music_lump.val == nullptr ) newmap.secret_map_intermission->music_lump = RuntimeFlowString( ( gamemode == commercial ) ? "read_m" : "victor" );
		}
	}

	for( auto& ep : umapinfogame.episodelist )
	{
		ep->all_maps = umapinfogame.episodemaps[ ep->episode_num ].data();
		ep->num_maps = umapinfogame.episodemaps[ ep->episode_num ].size();
	}

	umapinfogame.game = *current_game;
	umapinfogame.game.episodes = umapinfogame.episodelist.data();
	umapinfogame.game.num_episodes = umapinfogame.episodelist.size();

	current_game = &umapinfogame.game;
}

void D_GameflowParseUMAPINFO( int32_t lumpnum )
{
	DoomString lumptext( (size_t)W_LumpLength( lumpnum ) + 1, 0 );
	W_ReadLump( lumpnum, (void*)lumptext.c_str() );
	DoomStringStream lumpstream( lumptext );
	DoomString currline;

	while( std::getline( lumpstream, currline ) )
	{
		Sanitize( currline );
		if( currline.find( "map", 0 ) == 0 )
		{
			ParseMap( lumpstream, currline );
		}
	}

	BuildNewGameInfo();
}