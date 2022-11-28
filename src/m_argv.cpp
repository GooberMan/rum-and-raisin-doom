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
//


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL_stdinc.h>

#include "doomtype.h"

#include "d_iwad.h"

#include "i_system.h"

#include "m_argv.h"
#include "m_container.h"
#include "m_misc.h"

extern "C"
{
	int						myargc = 0;
	const char**			myargv = nullptr;
	int						originalargc = 0;
	const char**			originalargv = nullptr;
}

// This is a bit of a bodge until I properly rewrite this file in C++
std::vector< const char* >	argvlist;
std::vector< DoomString >	argvstorage;

//
// M_CheckParm
// Checks for the given parameter
// in the program's command line arguments.
// Returns the argument number (1 to argc-1)
// or 0 if not present
//

int M_CheckParmWithArgs(const char *check, int num_args)
{
    int i;

    for (i = 1; i < myargc - num_args; i++)
    {
	if (!strcasecmp(check, myargv[i]))
	    return i;
    }

    return 0;
}

//
// M_ParmExists
//
// Returns true if the given parameter exists in the program's command
// line arguments, false if not.
//

doombool M_ParmExists(const char *check)
{
    return M_CheckParm(check) != 0;
}

int M_CheckParm(const char *check)
{
    return M_CheckParmWithArgs(check, 0);
}

#define MAXARGVS        100

#if defined(_WIN32)
enum
{
    FILETYPE_UNKNOWN = 0x0,
    FILETYPE_IWAD =    0x2,
    FILETYPE_PWAD =    0x4,
    FILETYPE_DEH =     0x8,
};

static int GuessFileType(const char *name)
{
    int ret = FILETYPE_UNKNOWN;
    const char *base;
    char *lower;
    static doombool iwad_found = false;

    base = M_BaseName(name);
    lower = M_StringDuplicate(base);
    M_ForceLowercase(lower);

    // only ever add one argument to the -iwad parameter

    if (iwad_found == false && D_IsIWADName(lower))
    {
        ret = FILETYPE_IWAD;
        iwad_found = true;
    }
    else if (M_StringEndsWith(lower, ".wad") ||
             M_StringEndsWith(lower, ".lmp"))
    {
        ret = FILETYPE_PWAD;
    }
    else if (M_StringEndsWith(lower, ".deh") ||
//           M_StringEndsWith(lower, ".bex") ||
             M_StringEndsWith(lower, ".hhe") ||
             M_StringEndsWith(lower, ".seh"))
    {
        ret = FILETYPE_DEH;
    }

    free(lower);

    return ret;
}

typedef struct
{
    const char *str;
    int type, stable;
} argument_t;

static int CompareByFileType(const void *a, const void *b)
{
    const argument_t *arg_a = (const argument_t *) a;
    const argument_t *arg_b = (const argument_t *) b;

    const int ret = arg_a->type - arg_b->type;

    return ret ? ret : (arg_a->stable - arg_b->stable);
}

void M_AddLooseFiles(void)
{
    int i, types = 0;
    const char **newargv;
    argument_t *arguments;

    if (myargc < 2)
    {
        return;
    }

    // allocate space for up to three additional regular parameters

    arguments = (argument_t*)malloc((myargc + 3) * sizeof(*arguments));
    memset(arguments, 0, (myargc + 3) * sizeof(*arguments));

    // check the command line and make sure it does not already
    // contain any regular parameters or response files
    // but only fully-qualified LFS or UNC file paths

    for (i = 1; i < myargc; i++)
    {
        const char *arg = myargv[i];
        int type;

        if (strlen(arg) < 3 ||
            arg[0] == '-' ||
            ((!isalpha(arg[0]) || arg[1] != ':' || arg[2] != '\\') &&
            (arg[0] != '\\' || arg[1] != '\\')))
        {
            free(arguments);
            return;
        }

        type = GuessFileType(arg);
        arguments[i].str = arg;
        arguments[i].type = type;
        arguments[i].stable = i;
        types |= type;
    }

    // add space for one additional regular parameter
    // for each discovered file type in the new argument  list
    // and sort parameters right before their corresponding file paths

    if (types & FILETYPE_IWAD)
    {
        arguments[myargc].str = M_StringDuplicate("-iwad");
        arguments[myargc].type = FILETYPE_IWAD - 1;
        myargc++;
    }
    if (types & FILETYPE_PWAD)
    {
        arguments[myargc].str = M_StringDuplicate("-merge");
        arguments[myargc].type = FILETYPE_PWAD - 1;
        myargc++;
    }
    if (types & FILETYPE_DEH)
    {
        arguments[myargc].str = M_StringDuplicate("-deh");
        arguments[myargc].type = FILETYPE_DEH - 1;
        myargc++;
    }

    newargv = (const char**)malloc( myargc * sizeof( *newargv ) );

    // sort the argument list by file type, except for the zeroth argument
    // which is the executable invocation itself

    SDL_qsort(arguments + 1, myargc - 1, sizeof(*arguments), CompareByFileType);

    newargv[0] = myargv[0];

    for (i = 1; i < myargc; i++)
    {
        newargv[i] = arguments[i].str;
    }

    free(arguments);

    myargv = newargv;
}
#endif

// Return the name of the executable used to start the program:

const char *M_GetExecutableName(void)
{
    return M_BaseName(myargv[0]);
}

std::span< const char* > M_ParamArgs( const char* param )
{
	int32_t startparam = M_CheckParm( param );
	if( startparam > 0 )
	{
		++startparam;
		int32_t endparam = startparam;
		while( endparam < myargc && myargv[ endparam ][ 0 ] != '-' )
		{
			++endparam;
		}
		return std::span( &myargv[ startparam ], (size_t)( endparam - startparam ) );
	}
	return std::span( &myargv[ 0 ], 0 );
}

void M_ReplaceFileParameters( std::vector< DoomString > newargs )
{
	originalargc = myargc;
	originalargv = myargv;

	size_t totalargs = myargc + newargs.size();

	argvlist.reserve( totalargs );
	argvstorage = newargs;

	bool skip = false;

	for( int32_t currarg : iota( 0, myargc ) )
	{
		if( !strcasecmp( myargv[ currarg ], "-iwad" )
			|| !strcasecmp( myargv[ currarg ], "-file" )
			|| !strcasecmp( myargv[ currarg ], "-deh" ) )
		{
			skip = true;
		}
		else if( myargv[ currarg ][ 0 ] == '-' )
		{
			skip = false;
		}

		if( !skip ) argvlist.push_back( myargv[ currarg ] );
	}

	for( DoomString& currarg : argvstorage )
	{
		argvlist.push_back( currarg.c_str() );
	}

	myargc = argvlist.size();
	myargv = argvlist.data();
}
