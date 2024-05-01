//
// Copyright(C) 2020-2024 Ethan Watson
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

#include "d_demoloop.h"

#include "doomstat.h"

#include "d_gamesim.h"

#include "f_wipe.h"

#include "m_jsonlump.h"

#include "w_wad.h"

#include "z_zone.h"

constexpr demoloopentry_t registeredtitlepicentry =
{
	"TITLEPIC",
	0,
	"D_INTRO",
	0,
	170,
	dlt_artscreen,
	wipe_Melt,
};

constexpr demoloopentry_t commercialtitlepicentry =
{
	"TITLEPIC",
	0,
	"D_DM2TTL",
	0,
	385,
	dlt_artscreen,
	wipe_Melt,
};

constexpr demoloopentry_t registereddmenupicentry =
{
	"DMENUPIC",
	0,
	"D_INTRO",
	0,
	170,
	dlt_artscreen,
	wipe_Melt,
};

constexpr demoloopentry_t commercialdmenupicentry =
{
	"DMENUPIC",
	0,
	"D_DM2TTL",
	0,
	385,
	dlt_artscreen,
	wipe_Melt,
};

constexpr demoloopentry_t registeredinterpicentry =
{
	"INTERPIC",
	0,
	"D_INTRO",
	0,
	170,
	dlt_artscreen,
	wipe_Melt,
};

constexpr demoloopentry_t commercialinterpicentry =
{
	"INTERPIC",
	0,
	"D_DM2TTL",
	0,
	385,
	dlt_artscreen,
	wipe_Melt,
};

constexpr demoloopentry_t creditspicentry =
{
	"CREDIT",
	0,
	"",
	0,
	200,
	dlt_artscreen,
	wipe_Melt,
};

constexpr demoloopentry_t help2picentry =
{
	"HELP2",
	0,
	"",
	0,
	200,
	dlt_artscreen,
	wipe_Melt,
};

constexpr demoloopentry_t demo1entry =
{
	"DEMO1",
	0,
	"",
	0,
	0,
	dlt_demo,
	wipe_Melt
};

constexpr demoloopentry_t demo2entry =
{
	"DEMO2",
	0,
	"",
	0,
	0,
	dlt_demo,
	wipe_Melt
};

constexpr demoloopentry_t demo3entry =
{
	"DEMO3",
	0,
	"",
	0,
	0,
	dlt_demo,
	wipe_Melt
};

constexpr demoloopentry_t demo4entry =
{
	"DEMO4",
	0,
	"",
	0,
	0,
	dlt_demo,
	wipe_Melt
};

constexpr demoloopentry_t registereddemoloop[] =
{
	registeredtitlepicentry,
	demo1entry,
	creditspicentry,
	demo2entry,
	help2picentry,
	demo3entry,
};

constexpr demoloopentry_t retaildemoloop[] =
{
	registeredtitlepicentry,
	demo1entry,
	creditspicentry,
	demo2entry,
	creditspicentry,
	demo3entry,
	demo4entry,
};

constexpr demoloopentry_t commercialdemoloop[] =
{
	commercialtitlepicentry,
	demo1entry,
	creditspicentry,
	demo2entry,
	commercialtitlepicentry,
	demo3entry,
};

constexpr demoloopentry_t finaldemoloop[] =
{
	commercialtitlepicentry,
	demo1entry,
	creditspicentry,
	demo2entry,
	commercialtitlepicentry,
	demo3entry,
	demo4entry,
};

demoloop_t* D_CreateDemoLoop()
{
	demoloop_t* demoloop = (demoloop_t*)Z_MallocZero( sizeof( demoloop_t ), PU_STATIC, nullptr );

	auto LoadFromLump = [&demoloop]( const JSONElement& elem, const JSONLumpVersion& version ) -> jsonlumpresult_t
	{
		const JSONElement& entries = elem[ "entries" ];
		if( !entries.IsArray() || entries.Children().empty() )
		{
			return jl_parseerror;
		}

		std::vector< demoloopentry_t > entrystorage;
		entrystorage.reserve( entries.Children().size() );

		for( const JSONElement& currentryjson : entries.Children() )
		{
			const JSONElement& primarylump = currentryjson[ "primarylump" ];
			const JSONElement& secondarylump = currentryjson[ "secondarylump" ];
			const JSONElement& tics = currentryjson[ "tics" ];
			const JSONElement& type = currentryjson[ "type" ];

			size_t finalentry = entrystorage.size();
			entrystorage.push_back( {} );
			demoloopentry_t& thisentry = entrystorage[ finalentry ];

			snprintf( thisentry.primarylumpname, 10, "%8s", to< std::string >( primarylump ).c_str() );
			snprintf( thisentry.secondarylumpname, 10, "%8s", to< std::string >( secondarylump ).c_str() );
			thisentry.tics = to< uint64_t >( tics );
			thisentry.type = to< demolooptype_t >( type );
		}

		demoloopentry_t* allentries = (demoloopentry_t*)Z_MallocZero( sizeof(demoloopentry_t) * entrystorage.size(), PU_STATIC, nullptr );
		memcpy( allentries, entrystorage.data(), sizeof(demoloopentry_t) * entrystorage.size() );

		demoloop->SetEntries( allentries, entrystorage.size() );

		return jl_success;
	};

	if( M_ParseJSONLump( "DEMOLOOP", "demoloop", LoadFromLump ) != jl_success )
	{
		switch( gamemode )
		{
		case shareware:
		case registered:
			demoloop->SetEntries( registereddemoloop );
			break;
		case retail:
			demoloop->SetEntries( retaildemoloop );
			break;
		case commercial:
			if( comp.demo4 ) demoloop->SetEntries( finaldemoloop );
			else demoloop->SetEntries( commercialdemoloop );
			break;
		default:
			demoloop->SetEntries( registereddemoloop );
			break;
		}
	}

	return demoloop;
}
