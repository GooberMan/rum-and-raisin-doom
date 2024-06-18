//
// Copyright(C) 2005-2014 Simon Howard
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
//
// Main dehacked code
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "doomtype.h"
#include "i_glob.h"
#include "i_terminal.h"
#include "i_system.h"
#include "i_terminal.h"
#include "d_iwad.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"

#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"

extern deh_section_t *deh_section_types[];
extern const char *deh_signatures[];
extern int32_t remove_limits;

static doombool deh_initialized = false;
static GameVersion_t deh_loaded_version = exe_invalid;
static uint64_t deh_session_hash = 0;
static uint64_t deh_loaded_count = 0;

// If false, dehacked cheat replacements are ignored.

doombool deh_apply_cheats = true;

void DEH_Checksum(sha1_digest_t digest)
{
    sha1_context_t sha1_context;
    unsigned int i;

    SHA1_Init(&sha1_context);

    for (i=0; deh_section_types[i] != NULL; ++i)
    {
        if (deh_section_types[i]->sha1_hash != NULL)
        {
            deh_section_types[i]->sha1_hash(&sha1_context);
        }
    }

    SHA1_Final(digest, &sha1_context);
}

// Called on startup to call the Init functions

static void InitializeSections(void)
{
    unsigned int i;

    for (i=0; deh_section_types[i] != NULL; ++i)
    {
        if (deh_section_types[i]->init != NULL)
        {
            deh_section_types[i]->init();
        }
    }
}

static void DEH_Init(void)
{
    //!
    // @category mod
    //
    // Ignore cheats in dehacked files.
    //

    if (M_CheckParm("-nocheats") > 0) 
    {
	deh_apply_cheats = false;
    }

    // Call init functions for all the section definitions.
    InitializeSections();

    deh_initialized = true;
}

// Given a section name, get the section structure which corresponds

static deh_section_t *GetSectionByName(char *name)
{
    unsigned int i;

    for (i=0; deh_section_types[i] != NULL; ++i)
    {
        if (!strcasecmp(deh_section_types[i]->name, name))
        {
            return deh_section_types[i];
        }
    }

    return NULL;
}

// Is the string passed just whitespace?

static doombool IsWhitespace(char *s)
{
    for (; *s; ++s)
    {
        if (!isspace(*s))
            return false;
    }

    return true;
}

// Strip whitespace from the start and end of a string

static char *CleanString(char *s, int32_t stripcomments)
{
    char *strending;

    // Leading whitespace

    while (*s && isspace(*s))
        ++s;

    // Trailing whitespace
   
    strending = s + strlen(s) - 1;

	if( stripcomments )
	{
		char* found = strchr( s, '/' );
		if( found && found != s )
		{
			strending = found - 1;
		}
	}

    while (strlen(s) > 0 && isspace(*strending))
    {
        *strending = '\0';
        --strending;
    }

    return s;
}

// This pattern is used a lot of times in different sections, 
// an assignment is essentially just a statement of the form:
//
// Variable Name = Value
//
// The variable name can include spaces or any other characters.
// The string is split on the '=', essentially.
//
// Returns true if read correctly

doombool DEH_ParseAssignment(char *line, char **variable_name, char **value)
{
    char *p;

    // find the equals
    
    p = strchr(line, '=');

    if (p == NULL)
    {
        return false;
    }

    // variable name at the start
    // turn the '=' into a \0 to terminate the string here

    *p = '\0';
    *variable_name = CleanString(line, false);
    
    // value immediately follows the '='
    
    *value = CleanString(p+1, true);
    
    return true;
}

static doombool CheckSignatures(const char* line)
{
    // Check all signatures to see if one matches

    for (size_t i=0; deh_signatures[i] != NULL; ++i)
    {
        if (!strcmp(deh_signatures[i], line))
        {
            return true;
        }
    }

    return false;
}

// Parses a dehacked file by reading from the context
static void DEH_ParseContext(deh_context_t *context)
{
    deh_section_t *current_section = NULL;
    char section_name[20];
    void *tag = NULL;
    doombool extended;
    char *line;
    doombool cangetnewsection = true;

    // Read the header and check it matches the signature

	line = DEH_ReadLine(context, false);

    if (CheckSignatures(line))
    {
		line = DEH_ReadLine(context, false);
    }

    // Read the file

    while (!DEH_HadError(context))
    {
        // end of file?

        if (line == NULL)
        {
            return;
        }

        while (line[0] != '\0' && isspace(line[0]))
            ++line;

		doombool iswhitespace = IsWhitespace(line);
        if( *line != '#'
			&& !iswhitespace )
        {
            sscanf(line, "%19s", section_name);
            deh_section_t* newsection = cangetnewsection ? GetSectionByName(section_name) : NULL;

            if( newsection != NULL )
            {
                if (current_section != NULL
                    && current_section->end != NULL)
                {
                    // end of section
                    current_section->end(context, tag);
                }

                current_section = newsection;
                tag = current_section->start(context, line);
                cangetnewsection = false;
            }
			else if (current_section != NULL)
            {
                // parse this line

                current_section->line_parser(context, line, tag);
            }
            else
            {
				if( strncmp( line, "Doom version", 12 ) == 0 )
				{
					int32_t version_number = 0;
					sscanf( line, "Doom version = %i", &version_number );
					if( version_number == 2021 )
					{
						DEH_IncreaseGameVersion( context, exe_mbf21_extended );
					}
					else if( version_number == 2024 )
					{
						DEH_IncreaseGameVersion( context, exe_rnr24 );
					}
				}
				//if( strncmp( line, "Patch format", 12 ) == 0 )
				//{
				//	continue;
				//}
            }
        }

        cangetnewsection |= iswhitespace;

		// Read the next line. We only allow the special extended parsing
        // for the BEX [STRINGS] section.
        extended = current_section != NULL
                && !strcasecmp(current_section->name, "[STRINGS]");
        line = DEH_ReadLine(context, extended);
	}
}

// Parses a dehacked file

int DEH_LoadFile(const char *filename)
{
    deh_context_t *context;

    if (!deh_initialized)
    {
        DEH_Init();
    }

    I_TerminalPrintf( Log_Startup, " Applying %s\n", filename);

    context = DEH_OpenFile(filename);

    if (context == NULL)
    {
        fprintf(stderr, "DEH_LoadFile: Unable to open %s\n", filename);
        return 0;
    }

    DEH_ParseContext(context);

    if (DEH_HadError(context))
    {
        I_Error("Error parsing dehacked file");
    }

	GameVersion_t version = DEH_GameVersion( context );
	deh_loaded_version = M_MAX( version, deh_loaded_version );

	++deh_loaded_count;

    DEH_CloseFile(context);

    return 1;
}

// Load all dehacked patches from the given directory.
void DEH_AutoLoadPatches(const char *path)
{
    const char *filename;
    glob_t *glob;

    glob = I_StartMultiGlob(path, GLOB_FLAG_NOCASE|GLOB_FLAG_SORTED,
                            "*.deh", "*.hhe", "*.seh", NULL);
    for (;;)
    {
        filename = I_NextGlob(glob);
        if (filename == NULL)
        {
            break;
        }
        I_TerminalPrintf( Log_Startup, " [autoload]");
        DEH_LoadFile(filename);
    }

    I_EndGlob(glob);
}

// Load dehacked file from WAD lump.
// If allow_long is set, allow long strings and cheats just for this lump.

int DEH_LoadLump(int lumpnum, doombool allow_long, doombool allow_error)
{
    deh_context_t *context;

    if (!deh_initialized)
    {
        DEH_Init();
    }

	deh_loaded_version = M_MAX( exe_limit_removing, deh_loaded_version );

    context = DEH_OpenLump(lumpnum);

    if (context == NULL)
    {
        fprintf(stderr, "DEH_LoadFile: Unable to open lump %i\n", lumpnum);
        return 0;
    }

    I_TerminalPrintf( Log_Startup, " Applying %s:%s\n", M_BaseName( lumpinfo[ lumpnum ]->wad_file->path ), lumpinfo[ lumpnum ]->name );

    DEH_ParseContext(context);

    // If there was an error while parsing, abort with an error, but allow
    // errors to just be ignored if allow_error=true.
    if (!allow_error && DEH_HadError(context))
    {
        I_Error("Error parsing dehacked lump");
    }

	GameVersion_t version = DEH_GameVersion( context );
	deh_loaded_version = M_MAX( version, deh_loaded_version );

    DEH_CloseFile(context);

    return 1;
}

int DEH_LoadLumpByName(const char *name, doombool allow_long, doombool allow_error)
{
    int lumpnum;

    lumpnum = W_CheckNumForName(name);

    if (lumpnum == -1)
    {
        fprintf(stderr, "DEH_LoadLumpByName: '%s' lump not found\n", name);
        return 0;
    }

    return DEH_LoadLump(lumpnum, allow_long, allow_error);
}

// Check the command line for -deh argument, and others.
void DEH_ParseCommandLine(void)
{
    char *filename;
    int p;

    //!
    // @arg <files>
    // @category mod
    //
    // Load the given dehacked patch(es)
    //

    p = M_CheckParm("-deh");

    if (p > 0)
    {
        ++p;

        while (p < myargc && myargv[p][0] != '-')
        {
            filename = D_TryFindWADByName(myargv[p]);
            DEH_LoadFile(filename);
            free(filename);
            ++p;
        }
    }
}

DOOM_C_API GameVersion_t DEH_GetLoadedGameVersion()
{
	return deh_loaded_version;
}