//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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
//	Created by a sound utility.
//	Kept as a sample, DOOM2 sounds.
//


#include <stdlib.h>


#include "doomtype.h"
#include "sounds.h"

#include "d_mode.h"

#include "i_error.h"

#include "m_container.h"

#include "z_zone.h"

//
// Information about all the music
//

#define MUSIC(name) \
    { name, 0, NULL, NULL }

DOOM_C_API musicinfo_t S_music[] =
{
    MUSIC(NULL),
    MUSIC("e1m1"),
    MUSIC("e1m2"),
    MUSIC("e1m3"),
    MUSIC("e1m4"),
    MUSIC("e1m5"),
    MUSIC("e1m6"),
    MUSIC("e1m7"),
    MUSIC("e1m8"),
    MUSIC("e1m9"),
    MUSIC("e2m1"),
    MUSIC("e2m2"),
    MUSIC("e2m3"),
    MUSIC("e2m4"),
    MUSIC("e2m5"),
    MUSIC("e2m6"),
    MUSIC("e2m7"),
    MUSIC("e2m8"),
    MUSIC("e2m9"),
    MUSIC("e3m1"),
    MUSIC("e3m2"),
    MUSIC("e3m3"),
    MUSIC("e3m4"),
    MUSIC("e3m5"),
    MUSIC("e3m6"),
    MUSIC("e3m7"),
    MUSIC("e3m8"),
    MUSIC("e3m9"),
    MUSIC("inter"),
    MUSIC("intro"),
    MUSIC("bunny"),
    MUSIC("victor"),
    MUSIC("introa"),
    MUSIC("runnin"),
    MUSIC("stalks"),
    MUSIC("countd"),
    MUSIC("betwee"),
    MUSIC("doom"),
    MUSIC("the_da"),
    MUSIC("shawn"),
    MUSIC("ddtblu"),
    MUSIC("in_cit"),
    MUSIC("dead"),
    MUSIC("stlks2"),
    MUSIC("theda2"),
    MUSIC("doom2"),
    MUSIC("ddtbl2"),
    MUSIC("runni2"),
    MUSIC("dead2"),
    MUSIC("stlks3"),
    MUSIC("romero"),
    MUSIC("shawn2"),
    MUSIC("messag"),
    MUSIC("count2"),
    MUSIC("ddtbl3"),
    MUSIC("ampie"),
    MUSIC("theda3"),
    MUSIC("adrian"),
    MUSIC("messg2"),
    MUSIC("romer2"),
    MUSIC("tense"),
    MUSIC("shawn3"),
    MUSIC("openin"),
    MUSIC("evil"),
    MUSIC("ultima"),
    MUSIC("read_m"),
    MUSIC("dm2ttl"),
    MUSIC("dm2int") 
};


//
// Information about all the sfx
//

#define SOUND(index, name, priority) \
  { index, exe_doom_1_2, NULL, name, priority, NULL, -1, -1, 0, 0, -1, NULL }
#define SOUNDMBF(index, name, priority) \
  { index, exe_mbf, NULL, name, priority, NULL, -1, -1, 0, 0, -1, NULL }
#define SOUND_LINK(index, name, priority, link_id, pitch, volume) \
  { index, exe_doom_1_2, NULL, name, priority, &builtinsfx[link_id], pitch, volume, 0, 0, -1, NULL }

sfxinfo_t builtinsfx[] =
{
	// S_sfx[0] needs to be a dummy for odd reasons.
	SOUND(0,"none",   0),
	SOUND(1,"pistol", 64),
	SOUND(2,"shotgn", 64),
	SOUND(3,"sgcock", 64),
	SOUND(4,"dshtgn", 64),
	SOUND(5,"dbopn",  64),
	SOUND(6,"dbcls",  64),
	SOUND(7,"dbload", 64),
	SOUND(8,"plasma", 64),
	SOUND(9,"bfg",    64),
	SOUND(10,"sawup",  64),
	SOUND(11,"sawidl", 118),
	SOUND(12,"sawful", 64),
	SOUND(13,"sawhit", 64),
	SOUND(14,"rlaunc", 64),
	SOUND(15,"rxplod", 70),
	SOUND(16,"firsht", 70),
	SOUND(17,"firxpl", 70),
	SOUND(18,"pstart", 100),
	SOUND(19,"pstop",  100),
	SOUND(20,"doropn", 100),
	SOUND(21,"dorcls", 100),
	SOUND(22,"stnmov", 119),
	SOUND(23,"swtchn", 78),
	SOUND(24,"swtchx", 78),
	SOUND(25,"plpain", 96),
	SOUND(26,"dmpain", 96),
	SOUND(27,"popain", 96),
	SOUND(28,"vipain", 96),
	SOUND(29,"mnpain", 96),
	SOUND(30,"pepain", 96),
	SOUND(31,"slop",   78),
	SOUND(32,"itemup", 78),
	SOUND(33,"wpnup",  78),
	SOUND(34,"oof",    96),
	SOUND(35,"telept", 32),
	SOUND(36,"posit1", 98),
	SOUND(37,"posit2", 98),
	SOUND(38,"posit3", 98),
	SOUND(39,"bgsit1", 98),
	SOUND(40,"bgsit2", 98),
	SOUND(41,"sgtsit", 98),
	SOUND(42,"cacsit", 98),
	SOUND(43,"brssit", 94),
	SOUND(44,"cybsit", 92),
	SOUND(45,"spisit", 90),
	SOUND(46,"bspsit", 90),
	SOUND(47,"kntsit", 90),
	SOUND(48,"vilsit", 90),
	SOUND(49,"mansit", 90),
	SOUND(50,"pesit",  90),
	SOUND(51,"sklatk", 70),
	SOUND(52,"sgtatk", 70),
	SOUND(53,"skepch", 70),
	SOUND(54,"vilatk", 70),
	SOUND(55,"claw",   70),
	SOUND(56,"skeswg", 70),
	SOUND(57,"pldeth", 32),
	SOUND(58,"pdiehi", 32),
	SOUND(59,"podth1", 70),
	SOUND(60,"podth2", 70),
	SOUND(61,"podth3", 70),
	SOUND(62,"bgdth1", 70),
	SOUND(63,"bgdth2", 70),
	SOUND(64,"sgtdth", 70),
	SOUND(65,"cacdth", 70),
	SOUND(66,"skldth", 70),
	SOUND(67,"brsdth", 32),
	SOUND(68,"cybdth", 32),
	SOUND(69,"spidth", 32),
	SOUND(70,"bspdth", 32),
	SOUND(71,"vildth", 32),
	SOUND(72,"kntdth", 32),
	SOUND(73,"pedth",  32),
	SOUND(74,"skedth", 32),
	SOUND(75,"posact", 120),
	SOUND(76,"bgact",  120),
	SOUND(77,"dmact",  120),
	SOUND(78,"bspact", 100),
	SOUND(79,"bspwlk", 100),
	SOUND(80,"vilact", 100),
	SOUND(81,"noway",  78),
	SOUND(82,"barexp", 60),
	SOUND(83,"punch",  64),
	SOUND(84,"hoof",   70),
	SOUND(85,"metal",  70),
	SOUND_LINK(86,"chgun", 64, sfx_pistol, 150, 0),
	SOUND(87,"tink",   60),
	SOUND(88,"bdopn",  100),
	SOUND(89,"bdcls",  100),
	SOUND(90,"itmbk",  100),
	SOUND(91,"flame",  32),
	SOUND(92,"flamst", 32),
	SOUND(93,"getpow", 60),
	SOUND(94,"bospit", 70),
	SOUND(95,"boscub", 70),
	SOUND(96,"bossit", 70),
	SOUND(97,"bospn",  70),
	SOUND(98,"bosdth", 70),
	SOUND(99,"manatk", 70),
	SOUND(100,"mandth", 70),
	SOUND(101,"sssit",  70),
	SOUND(102,"ssdth",  70),
	SOUND(103,"keenpn", 70),
	SOUND(104,"keendt", 70),
	SOUND(105,"skeact", 70),
	SOUND(106,"skesit", 70),
	SOUND(107,"skeatk", 70),
	SOUND(108,"radio",  60),
	SOUNDMBF(109,"dgsit",  98),
	SOUNDMBF(110,"dgatk",  70),
	SOUNDMBF(111,"dgact",  120),
	SOUNDMBF(112,"dgdth",  70),
	SOUNDMBF(113,"dgpain", 96),
	SOUND(114,"secret",  100),
};

std::vector< sfxinfo_t* > allsfx;
std::unordered_map< int32_t, sfxinfo_t* > sfxmap;
DoomSoundLookup sfxinfos;

DOOM_C_API void D_InitSoundTables()
{
	allsfx.reserve( arrlen( builtinsfx ) + 200 );

	for( sfxinfo_t& soundinfo : std::span( builtinsfx ) )
	{
		sfxinfo_t* thissound = Z_MallocAs( sfxinfo_t, PU_STATIC, nullptr );
		*thissound = soundinfo;
		allsfx.push_back( thissound );
		sfxmap[ thissound->soundnum ] = thissound;
	}

	char name[16] = "fre000";

	for( int32_t dehextrasound : iota( 500, 700 ) )
	{
		int32_t soundindex = dehextrasound - 500;
		name[3] = '0' + ( soundindex / 100 );
		name[4] = '0' + ( ( soundindex / 10 ) % 10 );
		name[5] = '0' + ( soundindex % 10 );

		sfxinfo_t* thissound = Z_MallocAs( sfxinfo_t, PU_STATIC, nullptr );
		*thissound = {};

		thissound->soundnum = dehextrasound;
		thissound->minimumversion = exe_mbf_dehextra;
		strncpy( thissound->name, name, 6 );
		thissound->priority = 127;
		thissound->pitch = -1;
		thissound->volume = -1;
		allsfx.push_back( thissound );
		sfxmap[ thissound->soundnum ] = thissound;
	}
}

sfxinfo_t& DoomSoundLookup::Fetch( int32_t soundnum )
{
	auto found = sfxmap.find( soundnum );
	if( found == sfxmap.end() )
	{
		I_Error( "Invalid sound %d", soundnum );
	}

	return *found->second;
}

