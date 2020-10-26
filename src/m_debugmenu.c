//
// Copyright(C) 2020 Ethan Watson
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

#include "m_debugmenu.h"
#include "i_system.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

typedef enum
{
	MET_Invalid,

	MET_Window,
	MET_Category,
	MET_Button,
	MET_Checkbox,
	MET_RadioButton,

	MET_Separator,

	MET_Max,
} menuentrytype_e;

typedef struct menuentry_s menuentry_t;

#define MAXCHILDREN		32
#define MAXENTRIES		128
#define MAXNAMELENGTH	127

typedef struct menuentry_s
{
	char				name[ MAXNAMELENGTH + 1 ]; // +1 for overflow
	menuentry_t*		parent;
	menuentry_t*		children[ MAXCHILDREN + 1 ]; // +1 for overflow
	menuentry_t**		freechild;
	menufunc_t			callback;
	void*				data;
	boolean				enabled;
	int32_t				comparison;
	menuentrytype_e		type;
	
} menuentry_t;

typedef void (*renderchild_t)( menuentry_t* );

static menuentry_t* M_FindDebugMenuCategory( const char* category_name );
static void M_RenderDebugMenuChildren( menuentry_t** children );
static void M_ThrowAnErrorBecauseThisIsInvalid( menuentry_t* cat );
static void M_RenderWindowItem( menuentry_t* cat );
static void M_RenderCategoryItem( menuentry_t* cat );
static void M_RenderButtonItem( menuentry_t* cat );
static void M_RenderCheckboxItem( menuentry_t* cat );
static void M_RenderRadioButtonItem( menuentry_t* cat );
static void M_RenderSeparator( menuentry_t* cat );


static menuentry_t		entries[ MAXENTRIES + 1 ]; // +1 for overflow
static menuentry_t*		rootentry;
static menuentry_t*		nextentry;

static renderchild_t	childfuncs[] =
{
	&M_ThrowAnErrorBecauseThisIsInvalid,
	&M_RenderWindowItem,
	&M_RenderCategoryItem,
	&M_RenderButtonItem,
	&M_RenderCheckboxItem,
	&M_RenderRadioButtonItem,
	&M_RenderSeparator,
	&M_ThrowAnErrorBecauseThisIsInvalid,
};

static boolean aboutwindow_open = false;

static void M_OnDebugMenuCloseButton( const char* itemname )
{
	//S_StartSound(NULL, debugmenuclosesound);
	debugmenuactive = false;
}

static void M_OnDebugMenuCoreQuit( const char* itemname )
{
	I_Quit();
}

static void M_AboutWindow( const char* itemname )
{
	igText( PACKAGE_STRING );
	igText( "\"Haha software render go BRRRRR!\"" );
	igNewLine();
	igText( "A fork of Chocolate Doom focused on speed for modern architectures." );
	igNewLine();
	igText( "Copyright(C) 1993-1996 Id Software, Inc." );
	igText( "Copyright(C) 1993-2008 Raven Software" );
	igText( "Copyright(C) 2005-2014 Simon Howard" );
	igText( "Copyright(C) 2020 Ethan Watson" );
	igNewLine();
	igText( "This program is free software; you can redistribute it and/or" );
	igText( "modify it under the terms of the GNU General Public License" );
	igText( "as published by the Free Software Foundation; either version 2" );
	igText( "of the License, or (at your option) any later version." );
	igNewLine();
	igText( "This program is distributed in the hope that it will be useful," );
	igText( "but WITHOUT ANY WARRANTY; without even the implied warranty of" );
	igText( "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the" );
	igText( "GNU General Public License for more details." );
	igNewLine();
	igText( "https://github.com/GooberMan/rum-and-raisin-doom" );
}

void M_InitDebugMenu( void )
{
	memset( entries, 0, sizeof( entries ) );

	nextentry = entries;
	rootentry = nextentry++;

	sprintf( rootentry->name, "R&R Doom Debug" );
	rootentry->type = MET_Category;
	rootentry->freechild = rootentry->children;
	rootentry->enabled = true;
	rootentry->comparison = -1;

	M_RegisterDebugMenuButton( "Close debug", &M_OnDebugMenuCloseButton );

	M_FindDebugMenuCategory( "File" );
	M_FindDebugMenuCategory( "Render" );
	M_FindDebugMenuCategory( "Map" );

	M_RegisterDebugMenuWindow( "File|About " PACKAGE_NAME "...", &aboutwindow_open, &M_AboutWindow );
	M_RegisterDebugMenuSeparator( "File" );
	M_RegisterDebugMenuButton( "File|Quit " PACKAGE_NAME, &M_OnDebugMenuCoreQuit );
}

static void M_ThrowAnErrorBecauseThisIsInvalid( menuentry_t* cat )
{
	I_Error( "Invalid debug menu entry, whoops" );
}

static void M_RenderWindowItem( menuentry_t* cat )
{
	igCheckbox( cat->name, cat->data );
}

static void M_RenderCategoryItem( menuentry_t* cat )
{
	if( igBeginMenu( cat->name, true ) )
	{
		M_RenderDebugMenuChildren( cat->children );
		igEndMenu();
	}
}

static void M_RenderButtonItem( menuentry_t* cat )
{
	menufunc_t callback;
	if( igMenuItemBool( cat->name, "", false, true ) )
	{
		callback = cat->callback;
		callback( cat->name );
	}
}

static void M_RenderCheckboxItem( menuentry_t* cat )
{
	igMenuItemBool( cat->name, "", cat->data ? *(boolean*)cat->data : false, cat->enabled );
}

static void M_RenderRadioButtonItem( menuentry_t* cat )
{
	igMenuItemBool( cat->name, "", *(int32_t*)cat->data == cat->comparison, cat->enabled );
}

static void M_RenderSeparator( menuentry_t* cat )
{
	igSeparator( );
}

static void M_RenderDebugMenuChildren( menuentry_t** children )
{
	menuentry_t* child;

	while( *children )
	{
		child = *children;
		childfuncs[ child->type ]( child );
		++children;
	}
}

void M_RenderDebugMenu( void )
{
	menuentry_t* thiswindow;
	menufunc_t callback;
	boolean isopen;
	int32_t windowflags = ImGuiWindowFlags_NoFocusOnAppearing;
	if( !debugmenuactive ) windowflags |= ImGuiWindowFlags_NoInputs;

	thiswindow = entries;

	if( debugmenuactive )
	{
		if( igBeginMainMenuBar() )
		{
			M_RenderDebugMenuChildren( rootentry->children );
		}
		igEndMainMenuBar();
	}

	while( thiswindow != nextentry )
	{
		if( thiswindow->type == MET_Window )
		{
			isopen = *(boolean*)thiswindow->data;
			if( isopen )
			{
				if( igBegin( thiswindow->name, thiswindow->data, windowflags ) )
				{
					callback = thiswindow->callback;
					callback( thiswindow->name );
				}
				igEnd();
			}
		}

		++thiswindow;
	}
}

static menuentry_t* M_FindOrCreateDebugMenuCategory( const char* category_name, menuentry_t* potential_parent )
{
	menuentry_t* currentry = entries;
	size_t len = strlen( category_name );

	while( currentry != nextentry )
	{
		if( strncasecmp( currentry->name, category_name, len ) == 0 )
		{
			return currentry;
		}
		++currentry;
	}

	currentry = nextentry++;

	sprintf( currentry->name, category_name );
	currentry->type = MET_Category;
	currentry->freechild = currentry->children;
	currentry->enabled = true;
	currentry->comparison = -1;
	currentry->parent = potential_parent;

	*potential_parent->freechild = currentry;
	potential_parent->freechild++;

	return currentry;
}

static menuentry_t* M_FindDebugMenuCategory( const char* category_name )
{
	char tokbuffer[ MAXNAMELENGTH + 1 ];
	const char* token = NULL;

	menuentry_t* currcategory = rootentry;

	if( !category_name )
	{
		return rootentry;
	}

	memset( tokbuffer, 0, sizeof( tokbuffer ) );
	strcpy( tokbuffer, category_name );

	token = strtok( tokbuffer, "|" );
	while( token )
	{
		currcategory = M_FindOrCreateDebugMenuCategory( token, currcategory );
		token = strtok( NULL, "|" );
	}

	return currcategory;
}

static menuentry_t* M_FindDebugMenuCategoryFromFullPath( const char* full_path )
{
	char categoryname[ MAXNAMELENGTH + 1 ];
	menuentry_t* category = NULL;
	menuentry_t* newentry = NULL;

	size_t namelen = strlen( full_path );
	const char* actualname = full_path + namelen - 1;

	memset( categoryname, 0, sizeof( categoryname ) );

	while( actualname > full_path )
	{
		if( *actualname == '|' )
		{
			++actualname;
			break;
		}
		--actualname;
	}

	if( actualname != full_path )
	{
		strncpy( categoryname, full_path, (size_t)( actualname - full_path - 1 ) );
	}

	category = M_FindDebugMenuCategory( categoryname );

	return category;
}

static const char* M_GetActualName( const char* full_path )
{
	size_t namelen = strlen( full_path );
	const char* actualname = full_path + namelen - 1;

	while( actualname > full_path )
	{
		if( *actualname == '|' )
		{
			++actualname;
			break;
		}
		--actualname;
	}

	return actualname;
}

void M_RegisterDebugMenuButton( const char* full_path, menufunc_t callback )
{
	menuentry_t* category = M_FindDebugMenuCategoryFromFullPath( full_path );
	menuentry_t* currentry = nextentry++;
	const char* actualname = M_GetActualName( full_path );

	sprintf( currentry->name, actualname );
	currentry->type = MET_Button;
	currentry->freechild = currentry->children;
	currentry->enabled = true;
	currentry->comparison = -1;
	currentry->parent = category;
	currentry->callback = callback;

	*category->freechild = currentry;
	category->freechild++;
}

void M_RegisterDebugMenuWindow( const char* full_path, boolean* active, menufunc_t callback )
{
	menuentry_t* category = M_FindDebugMenuCategoryFromFullPath( full_path );
	menuentry_t* currentry = nextentry++;
	const char* actualname = M_GetActualName( full_path );

	sprintf( currentry->name, actualname );
	currentry->type = MET_Window;
	currentry->freechild = currentry->children;
	currentry->data = active;
	currentry->enabled = true;
	currentry->comparison = -1;
	currentry->parent = category;
	currentry->callback = callback;

	*category->freechild = currentry;
	category->freechild++;
}

void M_RegisterDebugMenuCheckbox( const char* full_path, boolean* value )
{
}

void M_RegisterDebugMenuRadioButton( const char* full_path, int32_t* value, int32_t selectedval )
{
}

void M_RegisterDebugMenuSeparator( const char* category_path )
{
	menuentry_t* category = M_FindDebugMenuCategory( category_path );
	menuentry_t* currentry = nextentry++;

	sprintf( currentry->name, "__separator__%d", ( currentry - entries ) );
	currentry->type = MET_Separator;
	currentry->freechild = currentry->children;
	currentry->enabled = true;
	currentry->comparison = -1;
	currentry->parent = category;

	*category->freechild = currentry;
	category->freechild++;
}