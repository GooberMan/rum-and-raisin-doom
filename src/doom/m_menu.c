//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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
// DESCRIPTION:
//	DOOM selection menu, options, episode etc.
//	Sliders and icons. Kinda widget stuff.
//


#include <stdlib.h>
#include <ctype.h>


#include "doomdef.h"
#include "doomkeys.h"
#include "dstrings.h"

#include "d_main.h"
#include "deh_main.h"

#include "i_input.h"
#include "i_swap.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"

#include "sounds.h"

#include "m_misc.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#include "r_local.h"


#include "hu_stuff.h"
#include "st_stuff.h"

#include "g_game.h"

#include "f_wipe.h"

#include "m_argv.h"
#include "m_controls.h"
#include "p_saveg.h"
#include "p_setup.h"

#include "s_sound.h"

#include "doomstat.h"

// Data.
#include "sounds.h"

#include "m_menu.h"

#include "i_video.h"

#include "am_map.h"

#include "m_dashboard.h"
#include "cimguiglue.h"


static int32_t			dashboard_currgameskill		= sk_medium;
static int32_t			dashboard_currgameflags		= GF_None;

extern patch_t*		hu_font[HU_FONTSIZE];
extern doombool		message_dontfuckwithme;

extern doombool		chat_on;		// in heads-up code

//
// defaulted values
//
int			mouseSensitivity = 5;

// Show messages has default, 0 = off, 1 = on
int			showMessages = 1;

typedef enum statsstyle_s
{
	stats_none,
	stats_standard,
} statsstyle_t;

int32_t		stats_style = stats_none;
	

// Blocky mode, has default, 0 = high, 1 = normal
int			detailLevel = 0;
int			screenblocks = 10;

// temp for screenblocks (0-9)
int			screenSize;

// -1 = no quicksave slot picked!
int			quickSaveSlot;

 // 1 = message to be printed
int			messageToPrint;
// ...and here is the message string!
const char		*messageString;

// message x & y
int			messx;
int			messy;
int			messageLastMenuActive;

// timed message = no input from user
doombool			messageNeedsInput;

void    (*messageRoutine)(int response);

char gammamsg[5][26] =
{
    GAMMALVL0,
    GAMMALVL1,
    GAMMALVL2,
    GAMMALVL3,
    GAMMALVL4
};

// we are going to be entering a savegame string
int			saveStringEnter;              
int             	saveSlot;	// which slot to save in
int			saveCharIndex;	// which char we're editing
static doombool          joypadSave = false; // was the save action initiated by joypad?
// old save description before edit
char			saveOldString[SAVESTRINGSIZE];  

doombool			inhelpscreens;
doombool			menuactive;
extern int32_t	dashboardopensound;
extern int32_t	dashboardclosesound;
extern int32_t	window_close_behavior;


#define SKULLXOFF		-32
#define LINEHEIGHT		16

extern doombool		sendpause;
char			savegamestrings[10][SAVESTRINGSIZE];

char	endstring[160];

static doombool opldev;

typedef enum categorytofocus_e
{
	cat_none,
	cat_general,
	cat_screen,
	cat_sound,

	cat_max
} categorytofocus_t;

static doombool				debugwindow_options = false;
static categorytofocus_t	debugwindow_tofocus = cat_none;


//
// MENU TYPEDEFS
//
typedef struct
{
    // 0 = no cursor here, 1 = ok, 2 = arrows ok
    short	status;
    
    char	name[10];
    
    // choice = menu item #.
    // if status = 2,
    //   choice=0:leftarrow,1:rightarrow
    void	(*routine)(int choice);
    
    // hotkey in menu
    char	alphaKey;			
} menuitem_t;



typedef struct menu_s
{
    short		numitems;	// # of menu items
    struct menu_s*	prevMenu;	// previous menu
    menuitem_t*		menuitems;	// menu items
    void		(*routine)();	// draw routine
    short		x;
    short		y;		// x,y of menu
    short		lastOn;		// last item user was on in menu
} menu_t;

short		itemOn;			// menu item skull is on
short		skullAnimCounter;	// skull animation counter
short		whichSkull;		// which skull to draw

// graphic name of skulls
// warning: initializer-string for array of chars is too long
const char *skullName[2] = {"M_SKULL1","M_SKULL2"};

// current menudef
menu_t*	currentMenu;                          

//
// PROTOTYPES
//
static void M_NewGame(int choice);
static void M_Episode(int choice);
static void M_ChooseSkill(int choice);
static void M_LoadGame(int choice);
static void M_SaveGame(int choice);
static void M_Options(int choice);
static void M_EndGame(int choice);
static void M_ReadThis(int choice);
static void M_ReadThis2(int choice);
static void M_QuitDOOM(int choice);

static void M_ChangeMessages(int choice);
static void M_ChangeSensitivity(int choice);
static void M_SfxVol(int choice);
static void M_MusicVol(int choice);
static void M_ChangeDetail(int choice);
static void M_SizeDisplay(int choice);
static void M_Sound(int choice);

static void M_FinishReadThis(int choice);
static void M_LoadSelect(int choice);
static void M_SaveSelect(int choice);
static void M_ReadSaveStrings(void);
static void M_QuickSave(void);
static void M_QuickLoad(void);

static void M_DrawMainMenu(void);
static void M_DrawReadThis1(void);
static void M_DrawReadThis2(void);
static void M_DrawNewGame(void);
static void M_DrawEpisode(void);
static void M_DrawOptions(void);
static void M_DrawSound(void);
static void M_DrawLoad(void);
static void M_DrawSave(void);

static void M_DrawSaveLoadBorder(int x,int y);
static void M_SetupNextMenu(menu_t *menudef);
static void M_DrawThermo(int x,int y,int thermWidth,int thermDot);
static void M_WriteText(int x, int y, const char *string);
static int  M_StringWidth(const char *string);
static int  M_StringHeight(const char *string);
static void M_StartMessage(const char *string, void *routine, doombool input);
static void M_ClearMenus (void);




//
// DOOM MENU
//
enum
{
    newgame = 0,
    options,
    loadgame,
    savegame,
    readthis,
    quitdoom,
    main_end
} main_e;

menuitem_t MainMenu[]=
{
    {1,"M_NGAME",M_NewGame,'n'},
    {1,"M_OPTION",M_Options,'o'},
    {1,"M_LOADG",M_LoadGame,'l'},
    {1,"M_SAVEG",M_SaveGame,'s'},
    // Another hickup with Special edition.
    {1,"M_RDTHIS",M_ReadThis,'r'},
    {1,"M_QUITG",M_QuitDOOM,'q'}
};

menu_t  MainDef =
{
    main_end,
    NULL,
    MainMenu,
    M_DrawMainMenu,
    97,64,
    0
};


//
// EPISODE SELECT
//
enum
{
    ep1,
    ep2,
    ep3,
    ep4,
    ep_end
} episodes_e;

menuitem_t EpisodeMenu[]=
{
    {1,"M_EPI1", M_Episode,'k'},
    {1,"M_EPI2", M_Episode,'t'},
    {1,"M_EPI3", M_Episode,'i'},
    {1,"M_EPI4", M_Episode,'t'}
};

menuitem_t* gameflow_episodes;

menu_t  EpiDef =
{
    ep_end,		// # of menu items
    &MainDef,		// previous menu
    EpisodeMenu,	// menuitem_t ->
    M_DrawEpisode,	// drawing routine ->
    48,63,              // x,y
    ep1			// lastOn
};

//
// NEW GAME
//
enum
{
    killthings,
    toorough,
    hurtme,
    violence,
    nightmare,
    newg_end
} newgame_e;

menuitem_t NewGameMenu[]=
{
    {1,"M_JKILL",	M_ChooseSkill, 'i'},
    {1,"M_ROUGH",	M_ChooseSkill, 'h'},
    {1,"M_HURT",	M_ChooseSkill, 'h'},
    {1,"M_ULTRA",	M_ChooseSkill, 'u'},
    {1,"M_NMARE",	M_ChooseSkill, 'n'}
};

menu_t  NewDef =
{
    newg_end,		// # of menu items
    &EpiDef,		// previous menu
    NewGameMenu,	// menuitem_t ->
    M_DrawNewGame,	// drawing routine ->
    48,63,              // x,y
    hurtme		// lastOn
};



//
// OPTIONS MENU
//
enum
{
    endgame,
    messages,
    detail,
    scrnsize,
    option_empty1,
    mousesens,
    option_empty2,
    soundvol,
    opt_end
} options_e;

menuitem_t OptionsMenu[]=
{
    {1,"M_ENDGAM",	M_EndGame,'e'},
    {1,"M_MESSG",	M_ChangeMessages,'m'},
    {1,"M_DETAIL",	M_ChangeDetail,'g'},
    {2,"M_SCRNSZ",	M_SizeDisplay,'s'},
    {-1,"",0,'\0'},
    {2,"M_MSENS",	M_ChangeSensitivity,'m'},
    {-1,"",0,'\0'},
    {1,"M_SVOL",	M_Sound,'s'}
};

menu_t  OptionsDef =
{
    opt_end,
    &MainDef,
    OptionsMenu,
    M_DrawOptions,
    60,37,
    0
};

//
// Read This! MENU 1 & 2
//
enum
{
    rdthsempty1,
    read1_end
} read_e;

menuitem_t ReadMenu1[] =
{
    {1,"",M_ReadThis2,0}
};

menu_t  ReadDef1 =
{
    read1_end,
    &MainDef,
    ReadMenu1,
    M_DrawReadThis1,
    280,185,
    0
};

enum
{
    rdthsempty2,
    read2_end
} read_e2;

menuitem_t ReadMenu2[]=
{
    {1,"",M_FinishReadThis,0}
};

menu_t  ReadDef2 =
{
    read2_end,
    &ReadDef1,
    ReadMenu2,
    M_DrawReadThis2,
    330,175,
    0
};

//
// SOUND VOLUME MENU
//
enum
{
    sfx_vol,
    sfx_empty1,
    music_vol,
    sfx_empty2,
    sound_end
} sound_e;

menuitem_t SoundMenu[]=
{
    {2,"M_SFXVOL",M_SfxVol,'s'},
    {-1,"",0,'\0'},
    {2,"M_MUSVOL",M_MusicVol,'m'},
    {-1,"",0,'\0'}
};

menu_t  SoundDef =
{
    sound_end,
    &OptionsDef,
    SoundMenu,
    M_DrawSound,
    80,64,
    0
};

//
// LOAD GAME MENU
//
enum
{
    load1,
    load2,
    load3,
    load4,
    load5,
    load6,
    load_end
} load_e;

menuitem_t LoadMenu[]=
{
    {1,"", M_LoadSelect,'1'},
    {1,"", M_LoadSelect,'2'},
    {1,"", M_LoadSelect,'3'},
    {1,"", M_LoadSelect,'4'},
    {1,"", M_LoadSelect,'5'},
    {1,"", M_LoadSelect,'6'}
};

menu_t  LoadDef =
{
    load_end,
    &MainDef,
    LoadMenu,
    M_DrawLoad,
    80,54,
    0
};

//
// SAVE GAME MENU
//
menuitem_t SaveMenu[]=
{
    {1,"", M_SaveSelect,'1'},
    {1,"", M_SaveSelect,'2'},
    {1,"", M_SaveSelect,'3'},
    {1,"", M_SaveSelect,'4'},
    {1,"", M_SaveSelect,'5'},
    {1,"", M_SaveSelect,'6'}
};

menu_t  SaveDef =
{
    load_end,
    &MainDef,
    SaveMenu,
    M_DrawSave,
    80,54,
    0
};


//
// M_ReadSaveStrings
//  read the strings from the savegame files
//
void M_ReadSaveStrings(void)
{
    FILE   *handle;
    int     i;
    char    name[256];

    for (i = 0;i < load_end;i++)
    {
        int retval;
        M_StringCopy(name, P_SaveGameFile(i), sizeof(name));

	handle = fopen(name, "rb");
        if (handle == NULL)
        {
            M_StringCopy(savegamestrings[i], EMPTYSTRING, SAVESTRINGSIZE);
            LoadMenu[i].status = 0;
            continue;
        }
        retval = fread(&savegamestrings[i], 1, SAVESTRINGSIZE, handle);
	fclose(handle);
        LoadMenu[i].status = retval == SAVESTRINGSIZE;
    }
}


//
// M_LoadGame & Cie.
//
void M_DrawLoad(void)
{
    int             i;
	
    V_DrawPatchDirect(72, 28, 
                      W_CacheLumpName(DEH_String("M_LOADG"), PU_CACHE));

    for (i = 0;i < load_end; i++)
    {
	M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i);
	M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i]);
    }
}



//
// Draw border for the savegame description
//
void M_DrawSaveLoadBorder(int x,int y)
{
    int             i;
	
    V_DrawPatchDirect(x - 8, y + 7,
                      W_CacheLumpName(DEH_String("M_LSLEFT"), PU_CACHE));
	
    for (i = 0;i < 24;i++)
    {
	V_DrawPatchDirect(x, y + 7,
                          W_CacheLumpName(DEH_String("M_LSCNTR"), PU_CACHE));
	x += 8;
    }

    V_DrawPatchDirect(x, y + 7, 
                      W_CacheLumpName(DEH_String("M_LSRGHT"), PU_CACHE));
}



//
// User wants to load this game
//
void M_LoadSelect(int choice)
{
    char    name[256];
	
    M_StringCopy(name, P_SaveGameFile(choice), sizeof(name));

    G_LoadGame (name);
    M_ClearMenus ();
}

//
// Selected from DOOM menu
//
void M_LoadGame (int choice)
{
    if (netgame)
    {
	M_StartMessage(DEH_String(LOADNET),NULL,false);
	return;
    }
	
    M_SetupNextMenu(&LoadDef);
    M_ReadSaveStrings();
}


//
//  M_SaveGame & Cie.
//
void M_DrawSave(void)
{
    int             i;
	
    V_DrawPatchDirect(72, 28, W_CacheLumpName(DEH_String("M_SAVEG"), PU_CACHE));
    for (i = 0;i < load_end; i++)
    {
	M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i);
	M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i]);
    }
	
    if (saveStringEnter)
    {
	i = M_StringWidth(savegamestrings[saveSlot]);
	M_WriteText(LoadDef.x + i,LoadDef.y+LINEHEIGHT*saveSlot,"_");
    }
}

//
// M_Responder calls this when user is finished
//
void M_DoSave(int slot)
{
    G_SaveGame (slot,savegamestrings[slot]);
    M_ClearMenus ();

    // PICK QUICKSAVE SLOT YET?
    if (quickSaveSlot == -2)
	quickSaveSlot = slot;
}

//
// Generate a default save slot name when the user saves to
// an empty slot via the joypad.
//
static void SetDefaultSaveName(int slot)
{
    // map from IWAD or PWAD?
    if (W_IsIWADLump(maplumpinfo) && strcmp(savegamedir, ""))
    {
        M_snprintf(savegamestrings[itemOn], SAVESTRINGSIZE,
                   "%s", maplumpinfo->name);
    }
    else
    {
        char *wadname = M_StringDuplicate(W_WadNameForLump(maplumpinfo));
        char *ext = strrchr(wadname, '.');

        if (ext != NULL)
        {
            *ext = '\0';
        }

        M_snprintf(savegamestrings[itemOn], SAVESTRINGSIZE,
                   "%s (%s)", maplumpinfo->name,
                   wadname);
        free(wadname);
    }
    M_ForceUppercase(savegamestrings[itemOn]);
    joypadSave = false;
}

//
// User wants to save. Start string input for M_Responder
//
void M_SaveSelect(int choice)
{
    int x, y;

    // we are going to be intercepting all chars
    saveStringEnter = 1;

    // We need to turn on text input:
    x = LoadDef.x - 11;
    y = LoadDef.y + choice * LINEHEIGHT - 4;
    I_StartTextInput(x, y, x + 8 + 24 * 8 + 8, y + LINEHEIGHT - 2);

    saveSlot = choice;
    M_StringCopy(saveOldString,savegamestrings[choice], SAVESTRINGSIZE);
    if (!strcmp(savegamestrings[choice], EMPTYSTRING))
    {
        savegamestrings[choice][0] = 0;

        if (joypadSave)
        {
            SetDefaultSaveName(choice);
        }
    }
    saveCharIndex = strlen(savegamestrings[choice]);
}

//
// Selected from DOOM menu
//
void M_SaveGame (int choice)
{
    if (!usergame)
    {
	M_StartMessage(DEH_String(SAVEDEAD),NULL,false);
	return;
    }
	
    if (gamestate != GS_LEVEL)
	return;
	
    M_SetupNextMenu(&SaveDef);
    M_ReadSaveStrings();
}



//
//      M_QuickSave
//
static char tempstring[90];

void M_QuickSaveResponse(int key)
{
    if (key == key_menu_confirm)
    {
	M_DoSave(quickSaveSlot);
	S_StartSound(NULL,sfx_swtchx);
    }
}

void M_QuickSave(void)
{
    if (!usergame)
    {
	S_StartSound(NULL,sfx_oof);
	return;
    }

    if (gamestate != GS_LEVEL)
	return;
	
    if (quickSaveSlot < 0)
    {
	M_StartControlPanel();
	M_ReadSaveStrings();
	M_SetupNextMenu(&SaveDef);
	quickSaveSlot = -2;	// means to pick a slot now
	return;
    }
    DEH_snprintf(tempstring, sizeof(tempstring),
                 QSPROMPT, savegamestrings[quickSaveSlot]);
    M_StartMessage(tempstring, M_QuickSaveResponse, true);
}



//
// M_QuickLoad
//
void M_QuickLoadResponse(int key)
{
    if (key == key_menu_confirm)
    {
	M_LoadSelect(quickSaveSlot);
	S_StartSound(NULL,sfx_swtchx);
    }
}


void M_QuickLoad(void)
{
    if (netgame)
    {
	M_StartMessage(DEH_String(QLOADNET),NULL,false);
	return;
    }
	
    if (quickSaveSlot < 0)
    {
	M_StartMessage(DEH_String(QSAVESPOT),NULL,false);
	return;
    }
    DEH_snprintf(tempstring, sizeof(tempstring),
                 QLPROMPT, savegamestrings[quickSaveSlot]);
    M_StartMessage(tempstring, M_QuickLoadResponse, true);
}

void M_DrawFullScreenPage( const char* pagename )
{
	patch_t* patch = W_CacheLumpName( pagename, PU_CACHE );
	// WIDESCREEN HACK
	int32_t xpos = -( ( patch->width - V_VIRTUALWIDTH ) / 2 );

	V_DrawPatch ( xpos, 0, patch );
}


//
// Read This Menus
// Had a "quick hack to fix romero bug"
//
void M_DrawReadThis1(void)
{
    inhelpscreens = true;

    M_DrawFullScreenPage( DEH_String("HELP2") );
}



//
// Read This Menus - optional second page.
//
void M_DrawReadThis2(void)
{
    inhelpscreens = true;

    // We only ever draw the second page if this is 
    // gameversion == exe_doom_1_9 and gamemode == registered

    M_DrawFullScreenPage( DEH_String("HELP1") );
}

void M_DrawReadThisCommercial(void)
{
    inhelpscreens = true;

    M_DrawFullScreenPage( DEH_String("HELP") );
}


//
// Change Sfx & Music volumes
//
void M_DrawSound(void)
{
    V_DrawPatchDirect (60, 38, W_CacheLumpName(DEH_String("M_SVOL"), PU_CACHE));

    M_DrawThermo(SoundDef.x,SoundDef.y+LINEHEIGHT*(sfx_vol+1),
		 16,sfxVolume);

    M_DrawThermo(SoundDef.x,SoundDef.y+LINEHEIGHT*(music_vol+1),
		 16,musicVolume);
}

void M_Sound(int choice)
{
    M_SetupNextMenu(&SoundDef);
}

void M_SfxVol(int choice)
{
    switch(choice)
    {
      case 0:
	if (sfxVolume)
	    sfxVolume--;
	break;
      case 1:
	if (sfxVolume < 15)
	    sfxVolume++;
	break;
    }
	
    S_SetSfxVolume(sfxVolume * 8);
}

void M_MusicVol(int choice)
{
    switch(choice)
    {
      case 0:
	if (musicVolume)
	    musicVolume--;
	break;
      case 1:
	if (musicVolume < 15)
	    musicVolume++;
	break;
    }
	
    S_SetMusicVolume(musicVolume * 8);
}




//
// M_DrawMainMenu
//
void M_DrawMainMenu(void)
{
    V_DrawPatchDirect(94, 2,
                      W_CacheLumpName(DEH_String("M_DOOM"), PU_CACHE));
}




//
// M_NewGame
//
void M_DrawNewGame(void)
{
    V_DrawPatchDirect(96, 14, W_CacheLumpName(DEH_String("M_NEWG"), PU_CACHE));
    V_DrawPatchDirect(54, 38, W_CacheLumpName(DEH_String("M_SKILL"), PU_CACHE));
}

void M_NewGame(int choice)
{
    if (netgame && !demoplayback)
    {
	M_StartMessage(DEH_String(NEWGAME),NULL,false);
	return;
    }
	
    // Chex Quest disabled the episode select screen, as did Doom II.

    if (gamemode == commercial || gameversion == exe_chex)
	M_SetupNextMenu(&NewDef);
    else
	M_SetupNextMenu(&EpiDef);
}


//
//      M_Episode
//
int     epi;

void M_DrawEpisode(void)
{
    V_DrawPatchDirect(54, 38, W_CacheLumpName(DEH_String("M_EPISOD"), PU_CACHE));
}

void M_VerifyNightmare(int key)
{
	if (key != key_menu_confirm)
		return;
		
	episodeinfo_t* epinfo;
	
	if( gameflow_episodes )
	{
		epinfo = current_game->episodes[ epi ];
	}
	else
	{
		epinfo = D_GameflowGetEpisode( gamemode == commercial ? 0 : epi + 1 );
	}

	G_DeferedInitNew( nightmare, epinfo->first_map, false );
	M_ClearMenus();
}

void M_ChooseSkill(int choice)
{
    if (choice == nightmare)
    {
	M_StartMessage(DEH_String(NIGHTMARE),M_VerifyNightmare,true);
	return;
    }
	
	episodeinfo_t* epinfo;
	
	if( gameflow_episodes )
	{
		epinfo = current_game->episodes[ epi ];
	}
	else
	{
		epinfo = D_GameflowGetEpisode( gamemode == commercial ? 0 : epi + 1 );
	}

    G_DeferedInitNew(choice, epinfo->first_map, false);
    M_ClearMenus ();
}

void M_Episode(int choice)
{
	if( gameflow_episodes )
	{
		if( current_game->episodes[ choice ]->first_map == NULL )
		{
			M_StartMessage( DEH_String( SWSTRING ), NULL, false );
			M_SetupNextMenu( &ReadDef1 );
			return;
		}
	}
	else
	{
		if ( (gamemode == shareware)
		 && choice)
		{
			M_StartMessage(DEH_String(SWSTRING),NULL,false);
			M_SetupNextMenu(&ReadDef1);
			return;
		}
	}

	epi = choice;
	M_SetupNextMenu(&NewDef);
}



//
// M_Options
//
static const char *detailNames[2] = {"M_GDHIGH","M_GDLOW"};
static const char *msgNames[2] = {"M_MSGOFF","M_MSGON"};

void M_DrawOptions(void)
{
    V_DrawPatchDirect(108, 15, W_CacheLumpName(DEH_String("M_OPTTTL"),
                                               PU_CACHE));
	
    V_DrawPatchDirect(OptionsDef.x + 175, OptionsDef.y + LINEHEIGHT * detail,
		      W_CacheLumpName(DEH_String(detailNames[detailLevel]),
			              PU_CACHE));

    V_DrawPatchDirect(OptionsDef.x + 120, OptionsDef.y + LINEHEIGHT * messages,
                      W_CacheLumpName(DEH_String(msgNames[showMessages]),
                                      PU_CACHE));

    M_DrawThermo(OptionsDef.x, OptionsDef.y + LINEHEIGHT * (mousesens + 1),
		 10, mouseSensitivity);

    M_DrawThermo(OptionsDef.x,OptionsDef.y+LINEHEIGHT*(scrnsize+1),
		 9,screenSize);
}

void M_Options(int choice)
{
//	M_SetupNextMenu(&OptionsDef);

	dashboardactive = true;
	debugwindow_options = true;
	debugwindow_tofocus = cat_general;
}



//
//      Toggle messages on/off
//
void M_ChangeMessages(int choice)
{
    // warning: unused parameter `int choice'
    choice = 0;
    showMessages = 1 - showMessages;
	
    if (!showMessages)
	players[consoleplayer].message = DEH_String(MSGOFF);
    else
	players[consoleplayer].message = DEH_String(MSGON);

    message_dontfuckwithme = true;
}


//
// M_EndGame
//
void M_EndGameResponse(int key)
{
    if (key != key_menu_confirm)
	return;
		
    currentMenu->lastOn = itemOn;
    M_ClearMenus ();
    D_StartTitle ();
}

void M_EndGame(int choice)
{
    choice = 0;
    if (!usergame)
    {
	S_StartSound(NULL,sfx_oof);
	return;
    }
	
    if (netgame)
    {
	M_StartMessage(DEH_String(NETEND),NULL,false);
	return;
    }
	
    M_StartMessage(DEH_String(ENDGAME),M_EndGameResponse,true);
}




//
// M_ReadThis
//
void M_ReadThis(int choice)
{
    choice = 0;
    M_SetupNextMenu(&ReadDef1);
}

void M_ReadThis2(int choice)
{
    choice = 0;
    M_SetupNextMenu(&ReadDef2);
}

void M_FinishReadThis(int choice)
{
    choice = 0;
    M_SetupNextMenu(&MainDef);
}




//
// M_QuitDOOM
//
int     quitsounds[8] =
{
    sfx_pldeth,
    sfx_dmpain,
    sfx_popain,
    sfx_slop,
    sfx_telept,
    sfx_posit1,
    sfx_posit3,
    sfx_sgtatk
};

int     quitsounds2[8] =
{
    sfx_vilact,
    sfx_getpow,
    sfx_boscub,
    sfx_slop,
    sfx_skeswg,
    sfx_kntdth,
    sfx_bspact,
    sfx_sgtatk
};



void M_QuitResponse(int key)
{
    if (key != key_menu_confirm)
	return;
    if (!netgame)
    {
	if (gamemode == commercial)
	    S_StartSound(NULL,quitsounds2[(gametic>>2)&7]);
	else
	    S_StartSound(NULL,quitsounds[(gametic>>2)&7]);
	I_WaitVBL(105);
    }
    I_Quit ();
}


static const char *M_SelectEndMessage(void)
{
    const char **endmsg;

    if (logical_gamemission == doom)
    {
        // Doom 1

        endmsg = doom1_endmsg;
    }
    else
    {
        // Doom 2
        
        endmsg = doom2_endmsg;
    }

    return endmsg[gametic % NUM_QUITMESSAGES];
}


void M_QuitDOOM(int choice)
{
    DEH_snprintf(endstring, sizeof(endstring), "%s\n\n" DOSY,
                 DEH_String(M_SelectEndMessage()));

    M_StartMessage(endstring,M_QuitResponse,true);
}




void M_ChangeSensitivity(int choice)
{
    switch(choice)
    {
      case 0:
	if (mouseSensitivity)
	    mouseSensitivity--;
	break;
      case 1:
	if (mouseSensitivity < 9)
	    mouseSensitivity++;
	break;
    }
}




void M_ChangeDetail(int choice)
{
    choice = 0;
    detailLevel = 0;

	dashboardactive = true;
	debugwindow_options = true;
	debugwindow_tofocus = cat_screen;

    //if (!detailLevel)
	//players[consoleplayer].message = DEH_String(DETAILHI);
    //else
	//players[consoleplayer].message = DEH_String(DETAILLO);
}




void M_SizeDisplay(int choice)
{
	int32_t oldscreenblocks = screenblocks;

    switch(choice)
    {
      case 0:
	if (screenSize > 0)
	{
	    screenblocks--;
	    screenSize--;
	}
	break;
      case 1:
	if (screenSize < 8)
	{
	    screenblocks++;
	    screenSize++;
	}
	break;
    }
	

	if( screenblocks != oldscreenblocks )
	{
		R_SetViewSize (screenblocks, detailLevel);
	}
}




//
//      Menu Functions
//
void
M_DrawThermo
( int	x,
  int	y,
  int	thermWidth,
  int	thermDot )
{
    int		xx;
    int		i;

    xx = x;
    V_DrawPatchDirect(xx, y, W_CacheLumpName(DEH_String("M_THERML"), PU_CACHE));
    xx += 8;
    for (i=0;i<thermWidth;i++)
    {
	V_DrawPatchDirect(xx, y, W_CacheLumpName(DEH_String("M_THERMM"), PU_CACHE));
	xx += 8;
    }
    V_DrawPatchDirect(xx, y, W_CacheLumpName(DEH_String("M_THERMR"), PU_CACHE));

    V_DrawPatchDirect((x + 8) + thermDot * 8, y,
		      W_CacheLumpName(DEH_String("M_THERMO"), PU_CACHE));
}


void
M_StartMessage
( const char	*string,
  void*		routine,
  doombool	input )
{
    messageLastMenuActive = menuactive;
    messageToPrint = 1;
    messageString = string;
    messageRoutine = routine;
    messageNeedsInput = input;
    menuactive = true;
    return;
}


//
// Find string width from hu_font chars
//
int M_StringWidth(const char *string)
{
    size_t             i;
    int             w = 0;
    int             c;
	
    for (i = 0;i < strlen(string);i++)
    {
	c = toupper(string[i]) - HU_FONTSTART;
	if (c < 0 || c >= HU_FONTSIZE)
	    w += 4;
	else
	    w += SHORT (hu_font[c]->width);
    }
		
    return w;
}



//
//      Find string height from hu_font chars
//
int M_StringHeight(const char* string)
{
    size_t             i;
    int             h;
    int             height = SHORT(hu_font[0]->height);
	
    h = height;
    for (i = 0;i < strlen(string);i++)
	if (string[i] == '\n')
	    h += height;
		
    return h;
}


//
//      Write a string using the hu_font
//
void
M_WriteText
( int		x,
  int		y,
  const char *string)
{
    int		w;
    const char *ch;
    int		c;
    int		cx;
    int		cy;
		

    ch = string;
    cx = x;
    cy = y;
	
    while(1)
    {
	c = *ch++;
	if (!c)
	    break;
	if (c == '\n')
	{
	    cx = x;
	    cy += 12;
	    continue;
	}
		
	c = toupper(c) - HU_FONTSTART;
	if (c < 0 || c>= HU_FONTSIZE)
	{
	    cx += 4;
	    continue;
	}
		
	w = SHORT (hu_font[c]->width);
	if (cx+w > V_VIRTUALWIDTH)
	    break;
	V_DrawPatchDirect(cx, cy, hu_font[c]);
	cx+=w;
    }
}

// These keys evaluate to a "null" key in Vanilla Doom that allows weird
// jumping in the menus. Preserve this behavior for accuracy.

static doombool IsNullKey(int key)
{
    return key == KEY_PAUSE || key == KEY_CAPSLOCK
        || key == KEY_SCRLCK || key == KEY_NUMLOCK;
}

//
// CONTROL PANEL
//

//
// M_Responder
//
doombool M_Responder (event_t* ev)
{
    int             ch;
    int             key;
    int             i;
    static  uint64_t     mousewait = 0;
    static  int     mousey = 0;
    static  int     lasty = 0;
    static  int     mousex = 0;
    static  int     lastx = 0;

    // In testcontrols mode, none of the function keys should do anything
    // - the only key is escape to quit.

    if (testcontrols)
    {
        if (ev->type == ev_quit
         || (ev->type == ev_keydown
          && (ev->data1 == key_menu_activate || ev->data1 == key_menu_quit)))
        {
            I_Quit();
            return true;
        }

        return false;
    }

    // "close" button pressed on window?
    if (ev->type == ev_quit)
    {
        // First click on close button = bring up quit confirm message.
        // Second click on close button = confirm quit

		if( dashboardactive || window_close_behavior == WindowClose_Always )
		{
			I_Quit();
		}
		else if( menuactive && messageToPrint && messageRoutine == M_QuitResponse )
        {
            M_QuitResponse(key_menu_confirm);
        }
        else
        {
            S_StartSound(NULL,sfx_swtchn);
            M_QuitDOOM(0);
        }

        return true;
    }

#if USE_IMGUI
	if( ev->type == ev_keydown && ev->data1 == key_menu_dashboard && dashboardremappingtype == Remap_None )
	{
		dashboardactive = !dashboardactive;
		S_StartSound(NULL, dashboardactive ? dashboardopensound : dashboardclosesound);
		return true;
	}

	if( dashboardactive )
	{
		return M_DashboardResponder( ev );
	}
#endif // USE_IMGUI

    // key is the key pressed, ch is the actual character typed
  
    ch = 0;
    key = -1;
	
    if (ev->type == ev_joystick)
    {
        // Simulate key presses from joystick events to interact with the menu.

        if (menuactive)
        {
            if (ev->data3 < 0)
            {
                key = key_menu_up;
                joywait = I_GetTimeTicks() + 5;
            }
            else if (ev->data3 > 0)
            {
                key = key_menu_down;
                joywait = I_GetTimeTicks() + 5;
            }
            if (ev->data2 < 0)
            {
                key = key_menu_left;
                joywait = I_GetTimeTicks() + 2;
            }
            else if (ev->data2 > 0)
            {
                key = key_menu_right;
                joywait = I_GetTimeTicks() + 2;
            }

#define JOY_BUTTON_MAPPED(x) ((x) >= 0)
#define JOY_BUTTON_PRESSED(x) (JOY_BUTTON_MAPPED(x) && (ev->data1 & (1 << (x))) != 0)

            if (JOY_BUTTON_PRESSED(joybfire))
            {
                // Simulate a 'Y' keypress when Doom show a Y/N dialog with Fire button.
                if (messageToPrint && messageNeedsInput)
                {
                    key = key_menu_confirm;
                }
                // Simulate pressing "Enter" when we are supplying a save slot name
                else if (saveStringEnter)
                {
                    key = KEY_ENTER;
                }
                else
                {
                    // if selecting a save slot via joypad, set a flag
                    if (currentMenu == &SaveDef)
                    {
                        joypadSave = true;
                    }
                    key = key_menu_forward;
                }
                joywait = I_GetTimeTicks() + 5;
            }
            if (JOY_BUTTON_PRESSED(joybuse))
            {
                // Simulate a 'N' keypress when Doom show a Y/N dialog with Use button.
                if (messageToPrint && messageNeedsInput)
                {
                    key = key_menu_abort;
                }
                // If user was entering a save name, back out
                else if (saveStringEnter)
                {
                    key = KEY_ESCAPE;
                }
                else
                {
                    key = key_menu_back;
                }
                joywait = I_GetTimeTicks() + 5;
            }
        }
        if (JOY_BUTTON_PRESSED(joybmenu))
        {
            key = key_menu_activate;
            joywait = I_GetTimeTicks() + 5;
        }
    }
    else
    {
	if (ev->type == ev_mouse && mousewait < I_GetTimeTicks())
	{
	    mousey += ev->data3;
	    if (mousey < lasty-30)
	    {
		key = key_menu_down;
		mousewait = I_GetTimeTicks() + 5;
		mousey = lasty -= 30;
	    }
	    else if (mousey > lasty+30)
	    {
		key = key_menu_up;
		mousewait = I_GetTimeTicks() + 5;
		mousey = lasty += 30;
	    }
		
	    mousex += ev->data2;
	    if (mousex < lastx-30)
	    {
		key = key_menu_left;
		mousewait = I_GetTimeTicks() + 5;
		mousex = lastx -= 30;
	    }
	    else if (mousex > lastx+30)
	    {
		key = key_menu_right;
		mousewait = I_GetTimeTicks() + 5;
		mousex = lastx += 30;
	    }
		
	    if (ev->data1&1)
	    {
		key = key_menu_forward;
		mousewait = I_GetTimeTicks() + 15;
	    }
			
	    if (ev->data1&2)
	    {
		key = key_menu_back;
		mousewait = I_GetTimeTicks() + 15;
	    }
	}
	else
	{
	    if (ev->type == ev_keydown)
	    {
		key = ev->data1;
		ch = ev->data2;
	    }
	}
    }
    
    if (key == -1)
	return false;

    // Save Game string input
    if (saveStringEnter)
    {
	switch(key)
	{
	  case KEY_BACKSPACE:
	    if (saveCharIndex > 0)
	    {
		saveCharIndex--;
		savegamestrings[saveSlot][saveCharIndex] = 0;
	    }
	    break;

          case KEY_ESCAPE:
            saveStringEnter = 0;
            I_StopTextInput();
            M_StringCopy(savegamestrings[saveSlot], saveOldString,
                         SAVESTRINGSIZE);
            break;

	  case KEY_ENTER:
	    saveStringEnter = 0;
            I_StopTextInput();
	    if (savegamestrings[saveSlot][0])
		M_DoSave(saveSlot);
	    break;

	  default:
            // Savegame name entry. This is complicated.
            // Vanilla has a bug where the shift key is ignored when entering
            // a savegame name. If vanilla_keyboard_mapping is on, we want
            // to emulate this bug by using ev->data1. But if it's turned off,
            // it implies the user doesn't care about Vanilla emulation:
            // instead, use ev->data3 which gives the fully-translated and
            // modified key input.

            if (ev->type != ev_keydown)
            {
                break;
            }
            if (vanilla_keyboard_mapping)
            {
                ch = ev->data1;
            }
            else
            {
                ch = ev->data3;
            }

            ch = toupper(ch);

            if (ch != ' '
             && (ch - HU_FONTSTART < 0 || ch - HU_FONTSTART >= HU_FONTSIZE))
            {
                break;
            }

	    if (ch >= 32 && ch <= 127 &&
		saveCharIndex < SAVESTRINGSIZE-1 &&
		M_StringWidth(savegamestrings[saveSlot]) <
		(SAVESTRINGSIZE-2)*8)
	    {
		savegamestrings[saveSlot][saveCharIndex++] = ch;
		savegamestrings[saveSlot][saveCharIndex] = 0;
	    }
	    break;
	}
	return true;
    }

    // Take care of any messages that need input
    if (messageToPrint)
    {
	if (messageNeedsInput)
        {
            if (key != ' ' && key != KEY_ESCAPE
             && key != key_menu_confirm && key != key_menu_abort)
            {
                return false;
            }
	}

	menuactive = messageLastMenuActive;
	messageToPrint = 0;
	if (messageRoutine)
	    messageRoutine(key);

	menuactive = false;
	S_StartSound(NULL,sfx_swtchx);
	return true;
    }

    if ((devparm && key == key_menu_help) ||
        (key != 0 && key == key_menu_screenshot))
    {
	G_ScreenShot ();
	return true;
    }

    // F-Keys
    if (!menuactive)
    {
	if (key == key_menu_decscreen)      // Screen size down
        {
	    if (automapactive || chat_on)
		return false;
	    M_SizeDisplay(0);
	    S_StartSound(NULL,sfx_stnmov);
	    return true;
	}
        else if (key == key_menu_incscreen) // Screen size up
        {
	    if (automapactive || chat_on)
		return false;
	    M_SizeDisplay(1);
	    S_StartSound(NULL,sfx_stnmov);
	    return true;
	}
        else if (key == key_menu_help)     // Help key
        {
	    M_StartControlPanel ();

	    if (gameversion >= exe_ultimate)
	      currentMenu = &ReadDef2;
	    else
	      currentMenu = &ReadDef1;

	    itemOn = 0;
	    S_StartSound(NULL,sfx_swtchn);
	    return true;
	}
        else if (key == key_menu_save)     // Save
        {
	    M_StartControlPanel();
	    S_StartSound(NULL,sfx_swtchn);
	    M_SaveGame(0);
	    return true;
        }
        else if (key == key_menu_load)     // Load
        {
	    M_StartControlPanel();
	    S_StartSound(NULL,sfx_swtchn);
	    M_LoadGame(0);
	    return true;
        }
        else if (key == key_menu_volume)   // Sound Volume
        {
			//M_StartControlPanel ();
			//currentMenu = &SoundDef;
			//itemOn = sfx_vol;
			dashboardactive = true;
			debugwindow_options = true;
			debugwindow_tofocus = cat_sound;
			S_StartSound(NULL,sfx_swtchn);
			return true;
		}
        else if (key == key_menu_detail)   // Detail toggle
        {
			M_ChangeDetail(0);
			S_StartSound(NULL,sfx_swtchn);
			return true;
        }
        else if (key == key_menu_qsave)    // Quicksave
        {
	    S_StartSound(NULL,sfx_swtchn);
	    M_QuickSave();
	    return true;
        }
        else if (key == key_menu_endgame)  // End game
        {
	    S_StartSound(NULL,sfx_swtchn);
	    M_EndGame(0);
	    return true;
        }
        else if (key == key_menu_messages) // Toggle messages
        {
	    M_ChangeMessages(0);
	    S_StartSound(NULL,sfx_swtchn);
	    return true;
        }
        else if (key == key_menu_qload)    // Quickload
        {
	    S_StartSound(NULL,sfx_swtchn);
	    M_QuickLoad();
	    return true;
        }
        else if (key == key_menu_quit)     // Quit DOOM
        {
	    S_StartSound(NULL,sfx_swtchn);
	    M_QuitDOOM(0);
	    return true;
        }
        else if (key == key_menu_gamma)    // gamma toggle
        {
	    usegamma++;
	    if (usegamma > 4)
		usegamma = 0;
	    players[consoleplayer].message = DEH_String(gammamsg[usegamma]);
            I_SetPalette (W_CacheLumpName (DEH_String("PLAYPAL"),PU_CACHE));
	    return true;
	}
    }

    // Pop-up menu?
    if (!menuactive)
    {
	if (key == key_menu_activate)
	{
	    M_StartControlPanel ();
	    S_StartSound(NULL,sfx_swtchn);
	    return true;
	}
	return false;
    }

    // Keys usable within menu

    if (key == key_menu_down)
    {
        // Move down to next item

        do
	{
	    if (itemOn+1 > currentMenu->numitems-1)
		itemOn = 0;
	    else itemOn++;
	    S_StartSound(NULL,sfx_pstop);
	} while(currentMenu->menuitems[itemOn].status==-1);

	return true;
    }
    else if (key == key_menu_up)
    {
        // Move back up to previous item

	do
	{
	    if (!itemOn)
		itemOn = currentMenu->numitems-1;
	    else itemOn--;
	    S_StartSound(NULL,sfx_pstop);
	} while(currentMenu->menuitems[itemOn].status==-1);

	return true;
    }
    else if (key == key_menu_left)
    {
        // Slide slider left

	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status == 2)
	{
	    S_StartSound(NULL,sfx_stnmov);
	    currentMenu->menuitems[itemOn].routine(0);
	}
	return true;
    }
    else if (key == key_menu_right)
    {
        // Slide slider right

	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status == 2)
	{
	    S_StartSound(NULL,sfx_stnmov);
	    currentMenu->menuitems[itemOn].routine(1);
	}
	return true;
    }
    else if (key == key_menu_forward)
    {
        // Activate menu item

	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status)
	{
	    currentMenu->lastOn = itemOn;
	    if (currentMenu->menuitems[itemOn].status == 2)
	    {
		currentMenu->menuitems[itemOn].routine(1);      // right arrow
		S_StartSound(NULL,sfx_stnmov);
	    }
	    else
	    {
		currentMenu->menuitems[itemOn].routine(itemOn);
		S_StartSound(NULL,sfx_pistol);
	    }
	}
	return true;
    }
    else if (key == key_menu_activate)
    {
        // Deactivate menu

	currentMenu->lastOn = itemOn;
	M_ClearMenus ();
	S_StartSound(NULL,sfx_swtchx);
	return true;
    }
    else if (key == key_menu_back)
    {
        // Go back to previous menu

	currentMenu->lastOn = itemOn;
	if (currentMenu->prevMenu)
	{
	    currentMenu = currentMenu->prevMenu;
	    itemOn = currentMenu->lastOn;
	    S_StartSound(NULL,sfx_swtchn);
	}
	return true;
    }

    // Keyboard shortcut?
    // Vanilla Doom has a weird behavior where it jumps to the scroll bars
    // when the certain keys are pressed, so emulate this.

    else if (ch != 0 || IsNullKey(key))
    {
	for (i = itemOn+1;i < currentMenu->numitems;i++)
        {
	    if (currentMenu->menuitems[i].alphaKey == ch)
	    {
		itemOn = i;
		S_StartSound(NULL,sfx_pstop);
		return true;
	    }
        }

	for (i = 0;i <= itemOn;i++)
        {
	    if (currentMenu->menuitems[i].alphaKey == ch)
	    {
		itemOn = i;
		S_StartSound(NULL,sfx_pstop);
		return true;
	    }
        }
    }

    return false;
}



//
// M_StartControlPanel
//
void M_StartControlPanel (void)
{
    // intro might call this repeatedly
    if (menuactive)
	return;
    
    menuactive = 1;
    currentMenu = &MainDef;         // JDC
    itemOn = currentMenu->lastOn;   // JDC
}

// Display OPL debug messages - hack for GENMIDI development.

static void M_DrawOPLDev(void)
{
    extern void I_OPL_DevMessages(char *, size_t);
    char debug[1024];
    char *curr, *p;
    int line;

    I_OPL_DevMessages(debug, sizeof(debug));
    curr = debug;
    line = 0;

    for (;;)
    {
        p = strchr(curr, '\n');

        if (p != NULL)
        {
            *p = '\0';
        }

        M_WriteText(0, line * 8, curr);
        ++line;

        if (p == NULL)
        {
            break;
        }

        curr = p + 1;
    }
}

//
// M_Drawer
// Called after the view has been rendered,
// but before it has been blitted.
//
void M_Drawer (void)
{
    static short	x;
    static short	y;
    unsigned int	i;
    unsigned int	max;
    char		string[80];
    const char          *name;
    int			start;

    inhelpscreens = false;
    
    // Horiz. & Vertically center string and print it.
    if (messageToPrint)
    {
	start = 0;
	y = V_VIRTUALHEIGHT/2 - M_StringHeight(messageString) / 2;
	while (messageString[start] != '\0')
	{
	    doombool foundnewline = false;

            for (i = 0; messageString[start + i] != '\0'; i++)
            {
                if (messageString[start + i] == '\n')
                {
                    M_StringCopy(string, messageString + start,
                                 sizeof(string));
                    if (i < sizeof(string))
                    {
                        string[i] = '\0';
                    }

                    foundnewline = true;
                    start += i + 1;
                    break;
                }
            }

            if (!foundnewline)
            {
                M_StringCopy(string, messageString + start, sizeof(string));
                start += strlen(string);
            }

	    x = V_VIRTUALWIDTH/2 - M_StringWidth(string) / 2;
	    M_WriteText(x, y, string);
	    y += SHORT(hu_font[0]->height);
	}

	return;
    }

    if (opldev)
    {
        M_DrawOPLDev();
    }

    if (!menuactive)
	return;

    if (currentMenu->routine)
	currentMenu->routine();         // call Draw routine
    
    // DRAW MENU
    x = currentMenu->x;
    y = currentMenu->y;
    max = currentMenu->numitems;

    for (i=0;i<max;i++)
    {
        name = DEH_String(currentMenu->menuitems[i].name);

	if (name[0] && W_CheckNumForName(name) > 0)
	{
	    V_DrawPatchDirect (x, y, W_CacheLumpName(name, PU_CACHE));
	}
	y += LINEHEIGHT;
    }

    
    // DRAW SKULL
    V_DrawPatchDirect(x + SKULLXOFF, currentMenu->y - 5 + itemOn*LINEHEIGHT,
		      W_CacheLumpName(DEH_String(skullName[whichSkull]),
				      PU_CACHE));
}


//
// M_ClearMenus
//
void M_ClearMenus (void)
{
    menuactive = 0;
    // if (!netgame && usergame && paused)
    //       sendpause = true;
}




//
// M_SetupNextMenu
//
void M_SetupNextMenu(menu_t *menudef)
{
    currentMenu = menudef;
    itemOn = currentMenu->lastOn;
}


//
// M_Ticker
//
void M_Ticker (void)
{
    if (--skullAnimCounter <= 0)
    {
	whichSkull ^= 1;
	skullAnimCounter = 8;
    }
}

// Just a dump of the Doom 1/2 PLAYPAL, nothing fancy
uint8_t defaultpalette[] =
{
		0x00, 0x00, 0x00,		0x1F, 0x17, 0x0B,		0x17, 0x0F, 0x07,		0x4B, 0x4B, 0x4B,
		0xFF, 0xFF, 0xFF,		0x1B, 0x1B, 0x1B,		0x13, 0x13, 0x13,		0x0B, 0x0B, 0x0B,
		0x07, 0x07, 0x07,		0x2F, 0x37, 0x1F,		0x23, 0x2B, 0x0F,		0x17, 0x1F, 0x07,
		0x0F, 0x17, 0x00,		0x4F, 0x3B, 0x2B,		0x47, 0x33, 0x23,		0x3F, 0x2B, 0x1B,
		0xFF, 0xB7, 0xB7,		0xF7, 0xAB, 0xAB,		0xF3, 0xA3, 0xA3,		0xEB, 0x97, 0x97,
		0xE7, 0x8F, 0x8F,		0xDF, 0x87, 0x87,		0xDB, 0x7B, 0x7B,		0xD3, 0x73, 0x73,
		0xCB, 0x6B, 0x6B,		0xC7, 0x63, 0x63,		0xBF, 0x5B, 0x5B,		0xBB, 0x57, 0x57,
		0xB3, 0x4F, 0x4F,		0xAF, 0x47, 0x47,		0xA7, 0x3F, 0x3F,		0xA3, 0x3B, 0x3B,
		0x9B, 0x33, 0x33,		0x97, 0x2F, 0x2F,		0x8F, 0x2B, 0x2B,		0x8B, 0x23, 0x23,
		0x83, 0x1F, 0x1F,		0x7F, 0x1B, 0x1B,		0x77, 0x17, 0x17,		0x73, 0x13, 0x13,
		0x6B, 0x0F, 0x0F,		0x67, 0x0B, 0x0B,		0x5F, 0x07, 0x07,		0x5B, 0x07, 0x07,
		0x53, 0x07, 0x07,		0x4F, 0x00, 0x00,		0x47, 0x00, 0x00,		0x43, 0x00, 0x00,
		0xFF, 0xEB, 0xDF,		0xFF, 0xE3, 0xD3,		0xFF, 0xDB, 0xC7,		0xFF, 0xD3, 0xBB,
		0xFF, 0xCF, 0xB3,		0xFF, 0xC7, 0xA7,		0xFF, 0xBF, 0x9B,		0xFF, 0xBB, 0x93,
		0xFF, 0xB3, 0x83,		0xF7, 0xAB, 0x7B,		0xEF, 0xA3, 0x73,		0xE7, 0x9B, 0x6B,
		0xDF, 0x93, 0x63,		0xD7, 0x8B, 0x5B,		0xCF, 0x83, 0x53,		0xCB, 0x7F, 0x4F,
		0xBF, 0x7B, 0x4B,		0xB3, 0x73, 0x47,		0xAB, 0x6F, 0x43,		0xA3, 0x6B, 0x3F,
		0x9B, 0x63, 0x3B,		0x8F, 0x5F, 0x37,		0x87, 0x57, 0x33,		0x7F, 0x53, 0x2F,
		0x77, 0x4F, 0x2B,		0x6B, 0x47, 0x27,		0x5F, 0x43, 0x23,		0x53, 0x3F, 0x1F,
		0x4B, 0x37, 0x1B,		0x3F, 0x2F, 0x17,		0x33, 0x2B, 0x13,		0x2B, 0x23, 0x0F,
		0xEF, 0xEF, 0xEF,		0xE7, 0xE7, 0xE7,		0xDF, 0xDF, 0xDF,		0xDB, 0xDB, 0xDB,
		0xD3, 0xD3, 0xD3,		0xCB, 0xCB, 0xCB,		0xC7, 0xC7, 0xC7,		0xBF, 0xBF, 0xBF,
		0xB7, 0xB7, 0xB7,		0xB3, 0xB3, 0xB3,		0xAB, 0xAB, 0xAB,		0xA7, 0xA7, 0xA7,
		0x9F, 0x9F, 0x9F,		0x97, 0x97, 0x97,		0x93, 0x93, 0x93,		0x8B, 0x8B, 0x8B,
		0x83, 0x83, 0x83,		0x7F, 0x7F, 0x7F,		0x77, 0x77, 0x77,		0x6F, 0x6F, 0x6F,
		0x6B, 0x6B, 0x6B,		0x63, 0x63, 0x63,		0x5B, 0x5B, 0x5B,		0x57, 0x57, 0x57,
		0x4F, 0x4F, 0x4F,		0x47, 0x47, 0x47,		0x43, 0x43, 0x43,		0x3B, 0x3B, 0x3B,
		0x37, 0x37, 0x37,		0x2F, 0x2F, 0x2F,		0x27, 0x27, 0x27,		0x23, 0x23, 0x23,
		0x77, 0xFF, 0x6F,		0x6F, 0xEF, 0x67,		0x67, 0xDF, 0x5F,		0x5F, 0xCF, 0x57,
		0x5B, 0xBF, 0x4F,		0x53, 0xAF, 0x47,		0x4B, 0x9F, 0x3F,		0x43, 0x93, 0x37,
		0x3F, 0x83, 0x2F,		0x37, 0x73, 0x2B,		0x2F, 0x63, 0x23,		0x27, 0x53, 0x1B,
		0x1F, 0x43, 0x17,		0x17, 0x33, 0x0F,		0x13, 0x23, 0x0B,		0x0B, 0x17, 0x07,
		0xBF, 0xA7, 0x8F,		0xB7, 0x9F, 0x87,		0xAF, 0x97, 0x7F,		0xA7, 0x8F, 0x77,
		0x9F, 0x87, 0x6F,		0x9B, 0x7F, 0x6B,		0x93, 0x7B, 0x63,		0x8B, 0x73, 0x5B,
		0x83, 0x6B, 0x57,		0x7B, 0x63, 0x4F,		0x77, 0x5F, 0x4B,		0x6F, 0x57, 0x43,
		0x67, 0x53, 0x3F,		0x5F, 0x4B, 0x37,		0x57, 0x43, 0x33,		0x53, 0x3F, 0x2F,
		0x9F, 0x83, 0x63,		0x8F, 0x77, 0x53,		0x83, 0x6B, 0x4B,		0x77, 0x5F, 0x3F,
		0x67, 0x53, 0x33,		0x5B, 0x47, 0x2B,		0x4F, 0x3B, 0x23,		0x43, 0x33, 0x1B,
		0x7B, 0x7F, 0x63,		0x6F, 0x73, 0x57,		0x67, 0x6B, 0x4F,		0x5B, 0x63, 0x47,
		0x53, 0x57, 0x3B,		0x47, 0x4F, 0x33,		0x3F, 0x47, 0x2B,		0x37, 0x3F, 0x27,
		0xFF, 0xFF, 0x73,		0xEB, 0xDB, 0x57,		0xD7, 0xBB, 0x43,		0xC3, 0x9B, 0x2F,
		0xAF, 0x7B, 0x1F,		0x9B, 0x5B, 0x13,		0x87, 0x43, 0x07,		0x73, 0x2B, 0x00,
		0xFF, 0xFF, 0xFF,		0xFF, 0xDB, 0xDB,		0xFF, 0xBB, 0xBB,		0xFF, 0x9B, 0x9B,
		0xFF, 0x7B, 0x7B,		0xFF, 0x5F, 0x5F,		0xFF, 0x3F, 0x3F,		0xFF, 0x1F, 0x1F,
		0xFF, 0x00, 0x00,		0xEF, 0x00, 0x00,		0xE3, 0x00, 0x00,		0xD7, 0x00, 0x00,
		0xCB, 0x00, 0x00,		0xBF, 0x00, 0x00,		0xB3, 0x00, 0x00,		0xA7, 0x00, 0x00,
		0x9B, 0x00, 0x00,		0x8B, 0x00, 0x00,		0x7F, 0x00, 0x00,		0x73, 0x00, 0x00,
		0x67, 0x00, 0x00,		0x5B, 0x00, 0x00,		0x4F, 0x00, 0x00,		0x43, 0x00, 0x00,
		0xE7, 0xE7, 0xFF,		0xC7, 0xC7, 0xFF,		0xAB, 0xAB, 0xFF,		0x8F, 0x8F, 0xFF,
		0x73, 0x73, 0xFF,		0x53, 0x53, 0xFF,		0x37, 0x37, 0xFF,		0x1B, 0x1B, 0xFF,
		0x00, 0x00, 0xFF,		0x00, 0x00, 0xE3,		0x00, 0x00, 0xCB,		0x00, 0x00, 0xB3,
		0x00, 0x00, 0x9B,		0x00, 0x00, 0x83,		0x00, 0x00, 0x6B,		0x00, 0x00, 0x53,
		0xFF, 0xFF, 0xFF,		0xFF, 0xEB, 0xDB,		0xFF, 0xD7, 0xBB,		0xFF, 0xC7, 0x9B,
		0xFF, 0xB3, 0x7B,		0xFF, 0xA3, 0x5B,		0xFF, 0x8F, 0x3B,		0xFF, 0x7F, 0x1B,
		0xF3, 0x73, 0x17,		0xEB, 0x6F, 0x0F,		0xDF, 0x67, 0x0F,		0xD7, 0x5F, 0x0B,
		0xCB, 0x57, 0x07,		0xC3, 0x4F, 0x00,		0xB7, 0x47, 0x00,		0xAF, 0x43, 0x00,
		0xFF, 0xFF, 0xFF,		0xFF, 0xFF, 0xD7,		0xFF, 0xFF, 0xB3,		0xFF, 0xFF, 0x8F,
		0xFF, 0xFF, 0x6B,		0xFF, 0xFF, 0x47,		0xFF, 0xFF, 0x23,		0xFF, 0xFF, 0x00,
		0xA7, 0x3F, 0x00,		0x9F, 0x37, 0x00,		0x93, 0x2F, 0x00,		0x87, 0x23, 0x00,
		0x4F, 0x3B, 0x27,		0x43, 0x2F, 0x1B,		0x37, 0x23, 0x13,		0x2F, 0x1B, 0x0B,
		0x00, 0x00, 0x53,		0x00, 0x00, 0x47,		0x00, 0x00, 0x3B,		0x00, 0x00, 0x2F,
		0x00, 0x00, 0x23,		0x00, 0x00, 0x17,		0x00, 0x00, 0x0B,		0x00, 0x00, 0x00,
		0xFF, 0x9F, 0x43,		0xFF, 0xE7, 0x4B,		0xFF, 0x7B, 0xFF,		0xFF, 0x00, 0xFF,
		0xCF, 0x00, 0xCF,		0x9F, 0x00, 0x9B,		0x6F, 0x00, 0x6B,		0xA7, 0x6B, 0x6B,
};

typedef enum resolutioncat_e
{
	res_named,

	res_4_3,
	res_16_9,
	res_21_9,
	res_32_9,

	res_post_4_3,
	res_post_16_9,
	res_post_21_9,
	res_post_32_9,
	
	res_unknown,
} resolutioncat_t;

static const char* resolutioncatstrings[] =
{
	"Make it look like",

	"4:3 native",
	"16:9 native",
	"21:9 native",
	"32:9 native",

	"4:3 post-scaled",
	"16:9 post-scaled",
	"21:9 post-scaled",
	"32:9 post-scaled",
};

typedef struct windowsizes_s
{
	int32_t			width;
	int32_t			height;
	resolutioncat_t	category;
	int32_t			postscaling;
	const char*		asstring;
} windowsizes_t;

#define WINDOWDIM( w, h, c )				{ w, h, c, 0, #w "x" #h }
#define WINDOWDIM_SCALED( w, h, c )			{ w, h, c, 1, #w "x" #h " post-scaled" }
#define WINDOWDIM_NAMED_SCALED( w, h, n )	{ w, h, res_named, 1, n }
#define WINDOW_MINWIDTH 800
#define WINDOW_MINHEIGHT 600

static windowsizes_t window_size_match =
{
	-1,
	-1,
	res_unknown,
	0,
	"Match window"
};

static windowsizes_t window_sizes_scaled[] =
{
	// It doesn't make much sense to allow lower resolutions than 800x600 for Rum and Raisin.
	// Be warned that the Dashboard will be basically impossible to use if you add smaller options.
	WINDOWDIM( 800,			600,		res_4_3		),
	WINDOWDIM( 960,			720,		res_4_3		),
	WINDOWDIM( 1024,		768,		res_4_3		),
	WINDOWDIM( 1280,		960,		res_4_3		),
	WINDOWDIM( 1600,		1200,		res_4_3		),
	WINDOWDIM( 1920,		1440,		res_4_3		),
	WINDOWDIM( 2560,		1920,		res_4_3		),
	WINDOWDIM( 3840,		2880,		res_4_3		),
	WINDOWDIM( 1280,		720,		res_16_9	),
	WINDOWDIM( 1600,		900,		res_16_9	),
	WINDOWDIM( 1920,		1080,		res_16_9	),
	WINDOWDIM( 2560,		1440,		res_16_9	),
	WINDOWDIM( 3840,		2160,		res_16_9	),
	WINDOWDIM( 1600,		686,		res_21_9	),
	WINDOWDIM( 1920,		822,		res_21_9	),
	WINDOWDIM( 2560,		1080,		res_21_9	),
	WINDOWDIM( 3440,		1440,		res_21_9	),
	WINDOWDIM( 5160,		2160,		res_21_9	),
	WINDOWDIM( 2560,		720,		res_32_9	),
	WINDOWDIM( 3200,		900,		res_32_9	),
	WINDOWDIM( 3840,		1080,		res_32_9	),
	WINDOWDIM( 5120,		1440,		res_32_9	),
	WINDOWDIM( 7680,		2160,		res_32_9	),
};
static int32_t window_sizes_scaled_count = sizeof( window_sizes_scaled ) / sizeof( *window_sizes_scaled );
static int32_t window_width_working;
static int32_t window_height_working;
static const char* window_dimensions_current = NULL;

static windowsizes_t render_sizes[] =
{
	WINDOWDIM_SCALED(	320,		200,	res_post_4_3	),
	WINDOWDIM_SCALED(	426,		200,	res_post_16_9	),
	WINDOWDIM_SCALED(	560,		200,	res_post_21_9	),
	WINDOWDIM_SCALED(	854,		200,	res_post_32_9	),
	WINDOWDIM_SCALED(	640,		400,	res_post_4_3	),
	WINDOWDIM_SCALED(	854,		400,	res_post_16_9	),
	WINDOWDIM_SCALED(	1120,		400,	res_post_21_9	),
	WINDOWDIM_SCALED(	1706,		400,	res_post_32_9	),
	WINDOWDIM_SCALED(	1280,		800,	res_post_4_3	),
	WINDOWDIM_SCALED(	1706,		800,	res_post_16_9	),
	WINDOWDIM_SCALED(	2240,		800,	res_post_21_9	),
	WINDOWDIM_SCALED(	3414,		800,	res_post_32_9	),
	WINDOWDIM_SCALED(	1440,		900,	res_post_4_3	),
	WINDOWDIM_SCALED(	1920,		900,	res_post_16_9	),
	WINDOWDIM_SCALED(	2520,		900,	res_post_21_9	),
	WINDOWDIM_SCALED(	3840,		900,	res_post_32_9	),
	WINDOWDIM_SCALED(	1600,		1000,	res_post_4_3	),
	WINDOWDIM_SCALED(	2132,		1000,	res_post_16_9	),
	WINDOWDIM_SCALED(	2800,		1000,	res_post_21_9	),
	WINDOWDIM_SCALED(	4266,		1000,	res_post_32_9	),
	WINDOWDIM_SCALED(	1920,		1200,	res_post_4_3	),
	WINDOWDIM_SCALED(	2560,		1200,	res_post_16_9	),
	WINDOWDIM_SCALED(	3360,		1200,	res_post_21_9	),
	WINDOWDIM_SCALED(	5120,		1200,	res_post_32_9	),
	WINDOWDIM_SCALED(	2560,		1600,	res_post_4_3	),
	WINDOWDIM_SCALED(	3414,		1600,	res_post_16_9	),
	WINDOWDIM_SCALED(	4480,		1600,	res_post_21_9	),
	WINDOWDIM_SCALED(	2880,		1800,	res_post_4_3	),
	WINDOWDIM_SCALED(	3840,		1800,	res_post_16_9	),
	WINDOWDIM_SCALED(	5040,		1800,	res_post_21_9	),
	WINDOWDIM_SCALED(	3200,		2000,	res_post_4_3	),
	WINDOWDIM_SCALED(	4266,		2000,	res_post_16_9	),
	WINDOWDIM_SCALED(	5600,		2000,	res_post_21_9	),

	WINDOWDIM(			320,		240,	res_4_3		),
	WINDOWDIM(			426,		240,	res_16_9	),
	WINDOWDIM(			560,		240,	res_21_9	),
	WINDOWDIM(			854,		240,	res_32_9	),
	WINDOWDIM(			640,		480,	res_4_3		),
	WINDOWDIM(			854,		480,	res_16_9	),
	WINDOWDIM(			1120,		480,	res_21_9	),
	WINDOWDIM(			1706,		480,	res_32_9	),
	WINDOWDIM(			960,		720,	res_4_3		),
	WINDOWDIM(			1280,		720,	res_16_9	),
	WINDOWDIM(			1680,		720,	res_21_9	),
	WINDOWDIM(			2560,		720,	res_32_9	),
	WINDOWDIM(			1200,		900,	res_4_3		),
	WINDOWDIM(			1600,		900,	res_16_9	),
	WINDOWDIM(			2100,		900,	res_21_9	),
	WINDOWDIM(			3200,		900,	res_32_9	),
	WINDOWDIM(			1280,		960,	res_4_3		),
	WINDOWDIM(			1706,		960,	res_16_9	),
	WINDOWDIM(			2240,		960,	res_21_9	),
	WINDOWDIM(			3414,		960,	res_32_9	),
	WINDOWDIM(			1440,		1080,	res_4_3		),
	WINDOWDIM(			1920,		1080,	res_16_9	),
	WINDOWDIM(			2520,		1080,	res_21_9	),
	WINDOWDIM(			3840,		1080,	res_32_9	),
	WINDOWDIM(			1600,		1200,	res_4_3		),
	WINDOWDIM(			2132,		1200,	res_16_9	),
	WINDOWDIM(			2800,		1200,	res_21_9	),
	WINDOWDIM(			4266,		1200,	res_32_9	),
	WINDOWDIM(			1920,		1440,	res_4_3		),
	WINDOWDIM(			2560,		1440,	res_16_9	),
	WINDOWDIM(			3360,		1440,	res_21_9	),
	WINDOWDIM(			5120,		1440,	res_32_9	),
	WINDOWDIM(			2560,		1820,	res_4_3		),
	WINDOWDIM(			3414,		1820,	res_16_9	),
	WINDOWDIM(			4480,		1820,	res_21_9	),
	WINDOWDIM(			2880,		2160,	res_4_3		),
	WINDOWDIM(			3840,		2160,	res_16_9	),
	WINDOWDIM(			5040,		2160,	res_21_9	),
	WINDOWDIM(			3200,		2400,	res_4_3		),
	WINDOWDIM(			4266,		2400,	res_16_9	),
	WINDOWDIM(			5600,		2400,	res_21_9	),

	WINDOWDIM_NAMED_SCALED( 320,	200,	"Vanilla" ),
	WINDOWDIM_NAMED_SCALED( 426,	200,	"Vanilla Widescreen" ),
	WINDOWDIM_NAMED_SCALED( 640,	400,	"Crispy Doom" ),
	WINDOWDIM_NAMED_SCALED( 854,	400,	"Crispy Doom Widescreen" ),
	WINDOWDIM_NAMED_SCALED( 960,	600,	"Unity Port 4:3" ),
	WINDOWDIM_NAMED_SCALED( 1280,	600,	"Unity Port 16:9" ),
};

static int32_t render_sizes_count = sizeof( render_sizes ) / sizeof( *render_sizes );
static const char* render_dimensions_current = NULL;

static const char* disk_icon_strings[] =
{
	"Off",
	"Command line switchable",
	"Disk",
	"CD ROM",
};
static int32_t disk_icon_strings_count = arrlen( disk_icon_strings );

static const char* sound_resample_quality_strings[] =
{
	"Linear",
	"Zero Order Hold",
	"Sinc (Low)",
	"Sinc (Medium)",
	"Sinc (High)"
};
static int32_t sound_resample_quality_strings_count = arrlen( sound_resample_quality_strings );

static const ImVec2 zerosize = { 0, 0 };
static const ImVec2 unmapbuttonsize = { 22, 22 };

static void M_DashboardDoColour( const char* itemname, int32_t* colourindex, int32_t defaultval, byte* palette )
{
	byte* paletteentry;
	ImU32 colour;
	ImVec2 coloursize;
	ImVec2 popupspacing;
	ImVec2 cursorpos;
	int32_t paletteindex;

	coloursize.x = coloursize.y = 22.f;
	popupspacing.x = popupspacing.y = 2.f;

	igPushID_Ptr( colourindex );

	igText( itemname );
	igNextColumn();

	if( *colourindex != -1 )
	{
		paletteentry = palette + *colourindex * 3;
		colour = IM_COL32( paletteentry[ 0 ], paletteentry[ 1 ], paletteentry[ 2 ], 255 );
	}
	else
	{
		colour = IM_COL32_BLACK;
	}

	igGetCursorScreenPos( &cursorpos );
	igPushStyleColor_U32( ImGuiCol_Button, colour );
	igPushStyleColor_U32( ImGuiCol_Border, IM_COL32_BLACK );
	igPushStyleVar_Float( ImGuiStyleVar_FrameBorderSize, 2.f );
	if( igButton( " ", coloursize ) )
	{
		igOpenPopup_Str( "colorpicker", ImGuiPopupFlags_None );
	}
	if( *colourindex == -1 )
	{
		ImGuiWindow* window = igGetCurrentWindow();
		ImVec2 start = { cursorpos.x + 1, cursorpos.y + 1 };
		ImVec2 end = { cursorpos.x + coloursize.x - 2, cursorpos.y + coloursize.y - 2 };
		ImDrawList_AddLine( window->DrawList, start, end, IM_COL32( 255, 0, 0, 255 ), 2.f );

		float_t temp = end.y;
		end.y = start.y;
		start.y = temp;
		ImDrawList_AddLine( window->DrawList, start, end, IM_COL32( 255, 0, 0, 255 ), 2.f );
	}

	igPopStyleVar( 1 );
	igPopStyleColor( 2 );

	if( igBeginPopup( "colorpicker", ImGuiWindowFlags_None ) )
	{
		paletteentry = palette;

		if( igButton( "Clear", zerosize ) )
		{
			*colourindex = -1;
		}
		igSameLine( 0, -1 );
		if( igButton( "Default", zerosize ) )
		{
			*colourindex = defaultval;
		}

		igPushStyleVar_Vec2( ImGuiStyleVar_ItemSpacing, popupspacing );

		for( paletteindex = 0; paletteindex < 256; ++paletteindex )
		{
			igPushID_Ptr( paletteentry );

			colour = IM_COL32( paletteentry[ 0 ], paletteentry[ 1 ], paletteentry[ 2 ], 255 );
			igPushStyleColor_U32( ImGuiCol_Button, colour );
			igPushStyleColor_U32( ImGuiCol_Border, paletteindex == *colourindex ? IM_COL32_WHITE : IM_COL32_BLACK );
			igPushStyleVar_Float( ImGuiStyleVar_FrameBorderSize, 1.f );
			if( igButton( " ", coloursize ) )
			{
				*colourindex = paletteindex;
			}

			if( ( paletteindex & 0xF ) != 0xF )
			{
				igSameLine( 0, -1 );
			}
			igPopStyleVar( 1 );
			igPopStyleColor( 2 );

			igPopID();

			paletteentry += 3;
		}

		igPopStyleVar( 1 );

		igEndPopup();
	}

	igPopID();
}

static void M_DashboardDoMapColour( const char* itemname, mapstyleentry_t* curr, mapstyleentry_t* defaultval, byte* palette )
{
	M_DashboardDoColour( itemname, &curr->val, defaultval->val, palette );
	igSameLine( 0, -1 );
	igPushID_Ptr( curr );
	igCheckboxFlags_IntPtr( "Pulsating", &curr->flags, 0x1 );
	igPopID();
}

typedef struct controldesc_s
{
	const char* name;
	int32_t*	value;
} controldesc_t;

typedef struct controlsection_s
{
	const char*		name;
	controldesc_t*	descs;
} controlsection_t;

static controlsection_t keymappings[] =
{
	{
		"Movement",
		(controldesc_t[]){	{	"Forward",				&key_up },
							{	"Back",					&key_down },
							{	"Turn Left",			&key_left },
							{	"Turn Right",			&key_right },
							{	"Strafe Left",			&key_strafeleft },
							{	"Strafe Right",			&key_straferight },
							{	"Fire",					&key_fire },
							{	"Use",					&key_use },
							{	"Strafe",				&key_strafe },
							{	"Run/Walk",				&key_speed },
							{	"Toggle autorun",		&key_toggle_autorun },
							{	NULL,					NULL }, },
	},
	{
		"Weapons",
		(controldesc_t[]){	{	"Weapon 1",				&key_weapon1 },
							{	"Weapon 2",				&key_weapon2 },
							{	"Weapon 3",				&key_weapon3 },
							{	"Weapon 4",				&key_weapon4 },
							{	"Weapon 5",				&key_weapon5 },
							{	"Weapon 6",				&key_weapon6 },
							{	"Weapon 7",				&key_weapon7 },
							{	"Weapon 8",				&key_weapon8 },
							{	"Previous Weapon",		&key_prevweapon },
							{	"Next Weapon",			&key_nextweapon },
							{	NULL,					NULL }, },
	},
	{
		"Automap",
		(controldesc_t[]){	{	"Toggle Map",			&key_map_toggle },
							{	"North",				&key_map_north },
							{	"South",				&key_map_south },
							{	"East",					&key_map_east },
							{	"West",					&key_map_west },
							{	"Zoom In",				&key_map_zoomin },
							{	"Zoom Out",				&key_map_zoomout },
							{	"Max zoom",				&key_map_maxzoom },
							{	"Follow",				&key_map_follow },
							{	"Toggle Grid",			&key_map_grid },
							{	"Add Mark",				&key_map_mark },
							{	"Remove Mark",			&key_map_clearmark },
							{	NULL,					NULL }, },
	},
	{
		"Menu",
		(controldesc_t[]){	{	"Toggle menu",			&key_menu_activate },
							{	"Up",					&key_menu_up },
							{	"Down",					&key_menu_down },
							{	"Left",					&key_menu_left },
							{	"Right",				&key_menu_right },
							{	"Back",					&key_menu_back },
							{	"Forward",				&key_menu_forward },
							{	"Confirm",				&key_menu_confirm },
							{	"Abort",				&key_menu_abort },
							{	NULL,					NULL }, },
	},
	{
		"Shortcuts",
		(controldesc_t[]){	{	"Toggle help screen",	&key_menu_help },
							{	"Toggle volume screen",	&key_menu_volume },
							{	"Quit game",			&key_menu_quit },
							{	"Load game",			&key_menu_load },
							{	"Save game",			&key_menu_save },
							{	"Quick load",			&key_menu_qload },
							{	"Quick save",			&key_menu_qsave },
							{	"End Game",				&key_menu_endgame },
							{	"Toggle detail",		&key_menu_detail },
							{	"Toggle messages",		&key_menu_messages },
							{	"Toggle gamma",			&key_menu_gamma },
							{	"Increase screen size",	&key_menu_incscreen },
							{	"Decrease screen size",	&key_menu_decscreen },
							{	"Screenshot",			&key_menu_screenshot },
							{	"Display last message",	&key_message_refresh },
							{	"Pause",				&key_pause },
							{	"Toggle dashboard",		&key_menu_dashboard },
							{	NULL,					NULL }, },
	},
	{	"Demos",
		(controldesc_t[]){	{	"End demo recording",	&key_demo_quit },
							{	"Spy on other players",	&key_spy },
							{	NULL,					NULL }, },
	},
	{
		"Multiplayer",
		(controldesc_t[]){	{	"Message all",			&key_multi_msg },
							{	"Message player 1",		&key_multi_msgplayer[0] },
							{	"Message player 2",		&key_multi_msgplayer[1] },
							{	"Message player 3",		&key_multi_msgplayer[2] },
							{	"Message player 4",		&key_multi_msgplayer[3] },
							{	NULL,					NULL }, },
	},
	{ NULL,				NULL }
};

controldesc_t mousemappings[] =
{
	{ "Fire",			&mousebfire },
	{ "Enable strafe",	&mousebstrafe },
	{ "Forward",		&mousebforward },
	{ "Backward",		&mousebbackward },
	{ "Use",			&mousebuse },
	{ "Strafe left",	&mousebstrafeleft },
	{ "Strafe right",	&mousebstraferight },
	{ "Prev weapon",	&mousebprevweapon },
	{ "Next weapon",	&mousebnextweapon },
	{ NULL,				NULL },
};

static const char* vsync_strings[ VSync_Max ] =
{
	"Off",				// VSync_Off
	"Adaptive",			// VSync_Adaptive
	"Native",			// VSync_Native
	"35Hz",				// VSync_35Hz
	"36Hz",				// VSync_36Hz
	"40Hz",				// VSync_40Hz
	"45Hz",				// VSync_45Hz
	"50Hz",				// VSync_50Hz
	"60Hz",				// VSync_60Hz
	"70Hz",				// VSync_70Hz
	"72Hz",				// VSync_72Hz
	"75Hz",				// VSync_75Hz
	"90Hz",				// VSync_90Hz
	"100Hz",			// VSync_100Hz
	"120Hz",			// VSync_120Hz
	"140Hz",			// VSync_140Hz
	"144Hz",			// VSync_144Hz
	"150Hz",			// VSync_150Hz
	"180Hz",			// VSync_180Hz
	"200Hz",			// VSync_200Hz
	"240Hz",			// VSync_240Hz
	"280Hz",			// VSync_280Hz
	"288Hz",			// VSync_288Hz
	"300Hz",			// VSync_300Hz
	"360Hz",			// VSync_360Hz
};

sessionstats_t session;

#define GS_Stat_OriginalMonsters	0x0000000000000001ull
#define GS_Stat_AllMonsters			0x0000000000000002ull
#define GS_Stat_AllItems			0x0000000000000004ull
#define GS_Stat_AllSecrets			0x0000000000000008ull
#define GS_Stat_KillsOriginal		0x0000000000000010ull
#define GS_Stat_KillsResurrected	0x0000000000000020ull
#define GS_Stat_KillsAll			0x0000000000000040ull
#define GS_Stat_FoundItems			0x0000000000000100ull
#define GS_Stat_FoundSecrets		0x0000000000000200ull
#define GS_Stat_Time				0x0000000000001000ull
#define GS_Stats					0x000000000000FFFFull

#define GS_Player_Current			0x0000000000000000ull
#define GS_Player_One				0x0000000001000000ull
#define GS_Player_Two				0x0000000002000000ull
#define GS_Player_Three				0x0000000004000000ull
#define GS_Player_Four				0x0000000008000000ull
#define GS_Players					0x000000000F000000ull

#define GS_Label_Time				0x0000010000000000ull
#define GS_Label_Kills				0x0000020000000000ull
#define GS_Label_Items				0x0000040000000000ull
#define GS_Label_Secrets			0x0000080000000000ull
#define GS_Labels					0x0000FF0000000000ull

#define GS_Layout_Spacing			0x0001000000000000ull
#define GS_Layout_NewLine			0x0002000000000000ull
#define GS_Layout_Colon				0x0004000000000000ull
#define GS_Layout_Slash				0x0008000000000000ull
#define GS_Layouts					0x00FF000000000000ull

#define GS_Modifier_Remaining		0x0100000000000000ull
#define GS_Modifier_Brackets		0x0200000000000000ull
#define GS_Modifier_ShortForm		0x0400000000000000ull
#define GS_Modifiers				0xFF00000000000000ull

static uint64_t statreport[] =
{
	GS_Label_Time | GS_Modifier_ShortForm,
	GS_Layout_Colon,
	GS_Layout_Spacing,
	GS_Stat_Time,

	GS_Layout_NewLine,

	GS_Label_Kills | GS_Modifier_ShortForm,
	GS_Layout_Colon,
	GS_Layout_Spacing,
	GS_Stat_KillsAll,
	GS_Layout_Slash,
	GS_Stat_AllMonsters,

	GS_Layout_Spacing,
	GS_Layout_Spacing,

	GS_Label_Items | GS_Modifier_ShortForm,
	GS_Layout_Colon,
	GS_Layout_Spacing,
	GS_Stat_FoundItems,
	GS_Layout_Slash,
	GS_Stat_AllItems,

	GS_Layout_Spacing,
	GS_Layout_Spacing,

	GS_Label_Secrets | GS_Modifier_ShortForm,
	GS_Layout_Colon,
	GS_Layout_Spacing,
	GS_Stat_FoundSecrets,
	GS_Layout_Slash,
	GS_Stat_AllSecrets,
};
static int32_t numstatreports = arrlen( statreport );

void M_DashboardDropShadowText( const char* text )
{
	ImVec2 cursor;
	ImVec2 newcursor;
	igGetCursorPos( &cursor );
	newcursor = cursor;
	newcursor.x += 2;
	newcursor.y += 2;
	igSetCursorPos( newcursor );
	igPushStyleColor_U32( ImGuiCol_Text, IM_COL32_BLACK );
	igText( text );
	igPopStyleColor( 1 );
	igSetCursorPos( cursor );
	igText( text );
}

void M_DashboardGameStatsContents( double_t scale )
{
	float_t oldscale = igGetCurrentWindow()->FontWindowScale;

	igSetWindowFontScale( scale * 0.9 );

	int32_t numlines = 1;
	for( int32_t ind = 0; ind < numstatreports; ++ind )
	{
		if( statreport[ ind ] & GS_Layout_NewLine ) ++numlines;
	}

	//for( int32_t ind = numlines; ind < 4; ++ind )
	//{
	//	igNewLine();
	//}

	char buffer[ 512 ];
	char* dest = buffer;

	char workingbuffer[ 64 ];

	for( int32_t ind = 0; ind < numstatreports; ++ind )
	{
		switch( statreport[ ind ] & GS_Layouts )
		{
		case GS_Layout_Spacing:
			*dest++ = ' ';
			break;
		case GS_Layout_NewLine:
			M_DashboardDropShadowText( buffer );
			dest = buffer;
			buffer[ 0 ] = 0;
			break;
		case GS_Layout_Colon:
			*dest++ = ':';
			break;
		case GS_Layout_Slash:
			*dest++ = '/';
			break;
		default:
			break;
		}

		const char* formatstring = "%s";
		if( statreport[ ind ] & GS_Modifier_Remaining ) formatstring = "Remaining %s";
		else if( statreport[ ind ] & GS_Modifier_Brackets ) formatstring = "(%s)";

		workingbuffer[ 0 ] = 0;

		uint32_t milliseconds;
		uint32_t seconds;
		uint32_t minutes;

		switch( statreport[ ind ] & GS_Stats )
		{
		case GS_Stat_OriginalMonsters:
			M_snprintf( workingbuffer, 64, "%d", session.start_total_monsters );
			break;
		case GS_Stat_AllMonsters:
			M_snprintf( workingbuffer, 64, "%d", session.start_total_monsters + session.resurrected_monsters );
			break;
		case GS_Stat_AllItems:
			M_snprintf( workingbuffer, 64, "%d", session.start_total_items );
			break;
		case GS_Stat_AllSecrets:
			M_snprintf( workingbuffer, 64, "%d", session.start_total_secrets );
			break;
		case GS_Stat_KillsOriginal:
			M_snprintf( workingbuffer, 64, "%d", session.total_monster_kills_global );
			break;
		case GS_Stat_KillsResurrected:
			M_snprintf( workingbuffer, 64, "%d", session.total_resurrected_monster_kills_global );
			break;
		case GS_Stat_KillsAll:
			M_snprintf( workingbuffer, 64, "%d", session.total_monster_kills_global + session.total_resurrected_monster_kills_global );
			break;
		case GS_Stat_FoundItems:
			M_snprintf( workingbuffer, 64, "%d", session.total_found_items_global );
			break;
		case GS_Stat_FoundSecrets:
			M_snprintf( workingbuffer, 64, "%d", session.total_found_secrets_global );
			break;
		case GS_Stat_Time:
			milliseconds = ( ( session.level_time % 35 ) * 10000 ) / 350;
			seconds = ( session.level_time / 35 ) % 60;
			minutes = ( session.level_time / 35 ) / 60;
			M_snprintf( workingbuffer, 64, "%d:%02d.%03d", minutes, seconds, milliseconds );
			break;
		default:
			break;
		}

		bool shortform = statreport[ ind ] & GS_Modifier_ShortForm;

		switch( statreport[ ind ] & GS_Labels )
		{
		case GS_Label_Time:
			M_snprintf( workingbuffer, 64, "Time" );
			break;
		case GS_Label_Kills:
			M_snprintf( workingbuffer, 64, "Kills" );
			break;
		case GS_Label_Items:
			M_snprintf( workingbuffer, 64, "Items" );
			break;
		case GS_Label_Secrets:
			M_snprintf( workingbuffer, 64, "Secrets" );
			break;
		default:
			break;
		}

		if( ( statreport[ ind ] & GS_Labels ) && shortform )
		{
			workingbuffer[ 1 ] = 0;
		}

		if( workingbuffer[ 0 ] != 0 )
		{
			dest += M_snprintf( dest, 512, formatstring, workingbuffer );
		}
	}

	if( buffer[ 0 ] )
	{
		M_DashboardDropShadowText( buffer );
	}

	igSetWindowFontScale( oldscale );
}

void M_DashboardGameStatsWindow( )
{
	if( gamestate != GS_LEVEL || stats_style == stats_none || automapactive || demoplayback || dashboardactive )
	{
		return;
	}
	ImGuiIO* IO = igGetIO();
	double_t globalscale = IO->DisplaySize.y / 240.0;
	double_t scalemod = 0.75;
	double_t currscale = globalscale * scalemod;

	ImGuiStyle original = *igGetStyle();
	ImGuiStyle_ScaleAllSizes( igGetStyle(), 0 );

	#define STATS_WIDTH 210
	#define STATS_HEIGHT 25

	ImVec2 windowsize = { STATS_WIDTH * currscale, STATS_HEIGHT * currscale };
	ImVec2 windowpos = { ( IO->DisplaySize.x - ( 320 * globalscale ) ) * 0.5,
						 IO->DisplaySize.y - ( ( screenblocks > 10 ? 0 : 32 * 1.2 /*ST_HEIGHT*/ ) + STATS_HEIGHT * scalemod ) * globalscale };
	igSetNextWindowSize( windowsize, ImGuiCond_Always );
	igSetNextWindowPos( windowpos, ImGuiCond_Always, zerosize );

	#define STATS_FLAGS ( ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs )
	if( igBegin( "Stats for nerds", NULL, STATS_FLAGS ) )
	{
		M_DashboardGameStatsContents( currscale );
	}
	igEnd();

	*igGetStyle() = original;
}

static float columwidth = 200.f;

static void M_DashboardControlsRemapping(	const char* itemname,
											controldesc_t* descs,
											int32_t remappingtype,
											int32_t unboundkey
											)
{
	controldesc_t*		currdesc;

	igPushID_Ptr( descs );
	if( igCollapsingHeader_TreeNodeFlags( itemname, ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		igColumns( 2, "", false );
		igSetColumnWidth( 0, columwidth );

		for( currdesc = descs; currdesc->name != NULL; ++currdesc )
		{
			igPushID_Ptr( currdesc->name );
			igText( currdesc->name );
			igNextColumn();
			M_DashboardKeyRemap( remappingtype, currdesc->value, currdesc->name );

			if( *currdesc->value != unboundkey && currdesc->value != &key_menu_dashboard )
			{
				igSameLine( 0, -1 );
				if( igButton( "X", unmapbuttonsize ) )
				{
					*currdesc->value = unboundkey;
				}
			}
			igNextColumn();
			igPopID();
		}

		igNewLine();
		igColumns( 1, "", false );
	}
	igPopID();
}

windowsizes_t* M_DashboardResolutionPicker( void* id, const char* preview, windowsizes_t* sizes, int32_t numsizes, int32_t currwidth, int32_t currheight, windowsizes_t* special, doombool specialselected )
{
	windowsizes_t* set = NULL;

	igPushID_Ptr( id );

	if( igBeginCombo( "", preview, ImGuiComboFlags_None ) )
	{
		if( special && igSelectable_Bool( special->asstring, specialselected, ImGuiSelectableFlags_None, zerosize ) )
		{
			set = special;
		}

		for( int32_t currcat = 0; currcat < res_unknown; ++currcat )
		{
			int32_t catcount = 0;
			for( int32_t currsize = 0; currsize < numsizes; ++currsize )
			{
				if( sizes[ currsize ].category == currcat )
				{
					++catcount;
				}
			}

			if( catcount && igBeginMenu( resolutioncatstrings[ currcat ], true ) )
			{
				for( int32_t currsize = 0; currsize < numsizes; ++currsize )
				{
					if( sizes[ currsize ].category == currcat )
					{
						doombool selected = currwidth == sizes[ currsize ].width && currheight == sizes[ currsize ].height;
						if( igSelectable_Bool( sizes[ currsize ].asstring, selected, ImGuiSelectableFlags_None, zerosize ) )
						{
							set = &sizes[ currsize ];
						}
					}
				}

				igEndMenu();
			}
		}

		igEndCombo();
	}
	igPopID();

	return set;
}

void M_DashboardOptionsWindow( const char* itemname, void* data )
{
	extern int32_t fullscreen;
	extern int32_t window_width;
	extern int32_t window_height;
	extern int32_t display_width;
	extern int32_t display_height;
	extern int32_t border_style;
	extern int32_t border_bezel_style;
	extern int32_t fuzz_style;
	extern int32_t show_endoom;
	extern int32_t show_text_startup;
	extern int32_t show_diskicon;
	extern int32_t enable_frame_interpolation;
	extern int32_t grabmouse;
	extern int32_t novert;
	extern int32_t snd_pitchshift;
	extern int32_t num_software_backbuffers;
	extern int32_t num_render_contexts;
	extern int32_t maxrendercontexts;
	extern int32_t vsync_mode;
	extern int32_t wipe_style;
	extern int32_t render_match_window;
	extern int32_t dynamic_resolution_scaling;
	extern int32_t additional_light_boost;
	extern int32_t sound_resample_type;

	controlsection_t*	currsection;

	bool WorkingBool = false;
	int32_t WorkingInt = 0;
	int32_t additionaltabflag = 0;

	bool doresize = false;

	int32_t index;
	bool selected;

	byte* palette = NULL;

	mapstyledata_t* style = map_styledata;

	if( igBeginTabBar( "Doom Options tabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_NoCloseWithMiddleMouseButton ) )
	{
		additionaltabflag = debugwindow_tofocus == cat_general ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
		if( igBeginTabItem( "General", NULL, ImGuiTabItemFlags_NoCloseWithMiddleMouseButton | additionaltabflag ) )
		{
			igColumns( 2, "", false );
			igSetColumnWidth( 0, columwidth );

			igText( "Frame interpolation" );
			igNextColumn();
			WorkingBool = !!enable_frame_interpolation;
			igPushID_Ptr( &enable_frame_interpolation );
			if( igCheckbox( "", &WorkingBool ) )
			{
				enable_frame_interpolation = (int32_t)WorkingBool;
			}
			igPopID();
			igNextColumn();

			igText( "Show text startup" );
			igNextColumn();
			WorkingBool = !!show_text_startup;
			igPushID_Ptr( &show_text_startup );
			if( igCheckbox( "", &WorkingBool ) )
			{
				show_text_startup = (int32_t)WorkingBool;
			}
			igPopID();
			igNextColumn();

			igText( "Show ENDOOM" );
			igNextColumn();
			WorkingBool = !!show_endoom;
			igPushID_Ptr( &show_endoom );
			if( igCheckbox( "", &WorkingBool ) )
			{
				show_endoom = (int32_t)WorkingBool;
			}
			igPopID();
			igNextColumn();

			igText( "Disk icon" );
			igNextColumn();
			igPushID_Ptr( &show_diskicon );
			if( igBeginCombo( "", disk_icon_strings[ show_diskicon ], ImGuiComboFlags_None ) )
			{
				for( index = 0; index < disk_icon_strings_count; ++index )
				{
					selected = index == show_diskicon;
					if( igSelectable_Bool( disk_icon_strings[ index ], selected, ImGuiSelectableFlags_None, zerosize ) )
					{
						D_SetupLoadingDisk( index );
					}
				}
				igEndCombo();
			}
			igPopID();
			igNextColumn();

			igText( "Ask on window close" );
			igNextColumn();
			WorkingBool = window_close_behavior == WindowClose_Ask;
			igPushID_Ptr( &window_close_behavior );
			if( igCheckbox( "", &WorkingBool ) )
			{
				window_close_behavior = WorkingBool ? WindowClose_Ask : WindowClose_Always;
			}
			igPopID();
			igNextColumn();

			igColumns( 1, "", false );
			igEndTabItem();
		}
		if( igBeginTabItem( "Keyboard", NULL, ImGuiTabItemFlags_NoCloseWithMiddleMouseButton ) )
		{
			igPushScrollableArea( "Keyboard", zerosize );

			for( currsection = keymappings; currsection->name != NULL; ++currsection )
			{
				M_DashboardControlsRemapping( currsection->name, currsection->descs, Remap_Key, 0 );
			}

			igPopScrollableArea();
			igEndTabItem();
		}

		if( igBeginTabItem( "Mouse", NULL, ImGuiTabItemFlags_NoCloseWithMiddleMouseButton ) )
		{
			igPushScrollableArea( "Mouse", zerosize );

			igPushID_Str( "Mouse settings" );
			if( igCollapsingHeader_TreeNodeFlags( "Settings", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen ) )
			{
				igCheckbox( "Grab in windowed mode", (bool*)&grabmouse );
				igCheckbox( "Disallow vertical movement", (bool*)&novert );
				igNewLine();

				igSliderInt( "Sensitivity", &mouseSensitivity, 0, 30, NULL, ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput );

				WorkingInt = (int32_t)( mouse_acceleration * 10.f );
				if( igSliderInt( "Acceleration", &WorkingInt, 10, 50, NULL, ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput ) )
				{
					mouse_acceleration = (float_t)WorkingInt * 0.1f;
				}
				igSliderInt( "Threshold", &mouse_threshold, 0, 32, NULL, ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput );
				igNewLine();
			}
			igPopID();

			M_DashboardControlsRemapping( "Bindings", mousemappings, Remap_Mouse, -1 );

			igPopScrollableArea();
			igEndTabItem();
		}

		additionaltabflag = debugwindow_tofocus == cat_screen ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
		if( igBeginTabItem( "Screen", NULL, ImGuiTabItemFlags_NoCloseWithMiddleMouseButton | additionaltabflag ) )
		{
			igPushScrollableArea( "Screen", zerosize );

			igPushID_Str( "Window settings" );
			if( igCollapsingHeader_TreeNodeFlags( "Window", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen ) )
			{
				igColumns( 2, "", false );
				igSetColumnWidth( 0, columwidth );

				igText( "Windowed dimensions" );
				igNextColumn();
				igText( "%dx%d", window_width, window_height );
				igNextColumn();
				igText( "Fullscreen dimensions" );
				igNextColumn();
				igText( "%dx%d", display_width, display_height );
				igNextColumn();

				igSeparatorEx( ImGuiSeparatorFlags_Horizontal | ImGuiSeparatorFlags_SpanAllColumns );

				igText( "Predefined window sizes" );

				igNextColumn();
				igPushItemWidth( 180.f );
				windowsizes_t* newresolution = M_DashboardResolutionPicker( &window_dimensions_current, window_dimensions_current
																			, window_sizes_scaled, window_sizes_scaled_count
																			, window_width, window_height, NULL, false );
				if( newresolution )
				{
					window_dimensions_current = newresolution->asstring;
					window_width_working = newresolution->width;
					window_height_working = newresolution->height;
					I_SetWindowDimensions( window_width_working, window_height_working );
				}
				igPopItemWidth();

				igNextColumn();
				igText( "Custom window size" );
				igSameLine( 0, -1 );

				igNextColumn();
				igPushItemWidth( 80.f );
				if( igInputInt( "w", &window_width_working, 16, 160, ImGuiInputTextFlags_None ) )
				{
					window_width_working = M_MAX( WINDOW_MINWIDTH, window_width_working );
					window_height_working = M_MAX( WINDOW_MINHEIGHT, window_width_working * 300 / 400 );
				}
				igSameLine( 0, -1 );
				if( igInputInt( "h", &window_height_working, 12, 120, ImGuiInputTextFlags_None ) )
				{
					window_height_working = M_MAX( WINDOW_MINHEIGHT, window_height_working );
					window_width_working = M_MAX( WINDOW_MINWIDTH, window_height_working * 400 / 300 );
				}
				igSameLine( 0, -1 );
				igPopItemWidth();
				if( igButton( "Apply", zerosize ) )
				{
					I_SetWindowDimensions( window_width_working, window_height_working );

					window_dimensions_current = NULL;
					for( index = 0; index < window_sizes_scaled_count; ++index )
					{
						if( window_width == window_sizes_scaled[ index ].width && window_height == window_sizes_scaled[ index ].height )
						{
							window_dimensions_current = window_sizes_scaled[ index ].asstring;
						}
					}
				}
				igNextColumn();

				igText( "Full screen" );
				igNextColumn();
				WorkingBool = !!fullscreen;
				igPushID_Ptr( &fullscreen );
				if( igCheckbox( "", &WorkingBool ) )
				{
					I_ToggleFullScreen();
				}
				igPopID();

				igColumns( 1, "", false );
			}
			igPopID();

			igPushID_Str( "Backbuffer settings" );
			if( igCollapsingHeader_TreeNodeFlags( "Backbuffer", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen ) )
			{
				igColumns( 2, "", false );
				igSetColumnWidth( 0, columwidth );

				igText( "Size" );
				igNextColumn();
				igPushItemWidth( 180.f );

				windowsizes_t* newresolution = M_DashboardResolutionPicker( &render_dimensions_current, render_dimensions_current
																			, render_sizes, render_sizes_count
																			, render_width, render_height, &window_size_match, render_match_window );
				if( newresolution )
				{
					const char* newstring = newresolution->asstring;
					for( index = 0; index < render_sizes_count; ++index )
					{
						if( render_sizes[ index ].category == res_named && newresolution->width == render_sizes[ index ].width && newresolution->height == render_sizes[ index ].height )
						{
							newstring = render_sizes[ index ].asstring;
						}
					}
					render_dimensions_current = newstring;

					if( newresolution->width < 0 || newresolution->height < 0 )
					{
						I_SetRenderMatchWindow();
					}
					else
					{
						I_SetRenderDimensions( newresolution->width, newresolution->height, newresolution->postscaling );
					}
					R_RebalanceContexts();
				}

				igPopItemWidth();
				igNextColumn();

				igText( "Chain length" );
				igNextColumn();
				igPushID_Ptr( &fullscreen );
				if( igSliderInt( "", &num_software_backbuffers, 1, 3, "%d", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput ) )
				{
					I_SetNumBuffers( num_software_backbuffers );
				}
				igPopID();
				igNextColumn();

				igColumns( 1, "", false );
			}
			igPopID();

			igPushID_Str( "Performance settings" );
			if( igCollapsingHeader_TreeNodeFlags( "Performance", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen ) )
			{
				igColumns( 2, "", false );
				igSetColumnWidth( 0, columwidth );

				int32_t oldcount = num_render_contexts;
				igText( "Running threads" );
				igNextColumn();
				igPushID_Ptr( &num_render_contexts );
				igSliderInt( "", &num_render_contexts, 1, maxrendercontexts, "%d", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput );
				if( num_render_contexts != oldcount )
				{
					R_RebalanceContexts();
				}
				igPopID();
				igNextColumn();

				igText( "Dynamic resolution scaling" );
				igNextColumn();
				igPushID_Ptr( &dynamic_resolution_scaling );
				WorkingBool = dynamic_resolution_scaling != 0;
				if( igCheckbox( "", &WorkingBool ) )
				{
					dynamic_resolution_scaling = WorkingBool ? DRS_Both : DRS_None;
				}
				if( igIsItemHovered( ImGuiHoveredFlags_None ) )
				{
					igBeginTooltip();
					igText( "This feature is currently in its early stages.\nIt looks like garbage for <1080p resolutions." );
					igEndTooltip();
				}
				igPopID();
				igNextColumn();

				igText( "Vsync" );
				igNextColumn();
				igPushID_Ptr( &vsync_mode );
				if( igBeginCombo( "", vsync_strings[ vsync_mode ], ImGuiComboFlags_None ) )
				{
					for( index = 0; index < VSync_Max; ++index )
					{
						selected = index == vsync_mode;
						if( I_VideoSupportsVSync( index )
							&& igSelectable_Bool( vsync_strings[ index ], selected, ImGuiSelectableFlags_None, zerosize ) )
						{
							I_VideoSetVSync( index );
						}
					}

					igEndCombo();
				}
				igPopID();
				igNextColumn();

				igColumns( 1, "", false );
			}
			igPopID();

			igPopScrollableArea();
			igEndTabItem();
		}

		additionaltabflag = debugwindow_tofocus == cat_sound ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
		if( igBeginTabItem( "Sound", NULL, ImGuiTabItemFlags_NoCloseWithMiddleMouseButton | additionaltabflag ) )
		{
			igPushScrollableArea( "Sound", zerosize );

			igColumns( 2, "", false );
			igSetColumnWidth( 0, columwidth );

			igText( "Pitch shifting" );
			igNextColumn();
			igPushID_Ptr( &snd_pitchshift );
			igCheckbox( "", (bool*)&snd_pitchshift );
			igPopID();
			igNextColumn();

			igText( "Resample quality" );
			igNextColumn();
			igPushID_Ptr( &sound_resample_type );
			if( igBeginCombo( "", sound_resample_quality_strings[ sound_resample_type ], ImGuiComboFlags_None ) )
			{
				for( int32_t index = 0; index < sound_resample_quality_strings_count; ++index )
				{
					if( igSelectable_Bool( sound_resample_quality_strings[ index ], index == sound_resample_type, ImGuiSelectableFlags_None, zerosize ) )
					{
						I_ChangeSoundQuality( index, S_sfx, NUMSFX );
					}
				}
				igEndCombo();
			}
			if( igIsItemHovered( ImGuiHoveredFlags_None ) )
			{
				igBeginTooltip();
				igText( "This can take a while the first time you select any option." );
				igEndTooltip();
			}
			igPopID();
			igNextColumn();

			igText( "Effects" );
			igNextColumn();
			igPushID_Ptr( &sfxVolume );
			igPushItemWidth( 200.f );
			if( igSliderInt( "", &sfxVolume, 0, 15, NULL, ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput ) )
			{
				S_SetSfxVolume( sfxVolume * 8 );
			}
			igPopID();
			igNextColumn();

			igText( "Music" );
			igNextColumn();
			igPushID_Ptr( &musicVolume );
			igPushItemWidth( 200.f );
			if( igSliderInt( "", &musicVolume, 0, 15, NULL, ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput ) )
			{
				S_SetMusicVolume( musicVolume * 8 );
			}
			igPopID();
			igNextColumn();

			igColumns( 1, "", false );

			igPopScrollableArea();
			igEndTabItem();
		}

		if( igBeginTabItem( "View", NULL, ImGuiTabItemFlags_NoCloseWithMiddleMouseButton ) )
		{
			igPushScrollableArea( "View", zerosize );

			igColumns( 2, "", false );
			igSetColumnWidth( 0, columwidth );

			igText( "Gamma level" );
			igNextColumn();
			igPushID_Ptr( &usegamma );
			igPushItemWidth( 200.f );
			if( igSliderInt( "", &usegamma, 0, 4, NULL, ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput) )
			{
				players[ consoleplayer ].message = DEH_String( gammamsg[ usegamma ] );
				if( numlumps > 0 )
				{
					I_SetPalette( W_CacheLumpName( DEH_String( "PLAYPAL" ), PU_CACHE ) );
				}
			}
			igPopItemWidth();
			igPopID();

			igNextColumn();
			igText( "Light boost" );
			igNextColumn();
			igPushID_Ptr( &additional_light_boost );
			igPushItemWidth( 200.f );
			igSliderInt( "", &additional_light_boost, 0, 8, NULL, ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput);
			igPopItemWidth();
			igPopID();

			extern int32_t vertical_fov_degrees;
			igNextColumn();
			igText( "Vertical FOV" );
			igNextColumn();
			igPushID_Ptr( &vertical_fov_degrees );
			igPushItemWidth( 200.f );
			if( igSliderInt( "", &vertical_fov_degrees, 50, 120, NULL, ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput) )
			{
				R_ExecuteSetViewSize();
			}
			igPopItemWidth();
			igPopID();

			igNextColumn();
			igText( "Screen size" );
			igNextColumn();
			igPushID_Ptr( &screenblocks );
			igPushItemWidth( 200.f );
			if( igSliderInt( "", &screenblocks, 10, 11, NULL, ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput ) )
			{
				screenSize = screenblocks - 3;
				R_SetViewSize (screenblocks, 0);
			}
			if( igIsItemHovered( ImGuiHoveredFlags_None ) )
			{
				igBeginTooltip();
				igText( "Shrinking lower than the status bar is currently broken.\nReturning when dynamic resolution scaling gets better." );
				igEndTooltip();
			}
			igPopItemWidth();
			igPopID();

			igNextColumn();
			igText( "Border style" );
			igNextColumn();

			igPushID_Ptr( &border_style );
			doresize |= igRadioButton_IntPtr( "Original", &border_style, 0 );
			igSameLine( 0, -1 );
			doresize |= igRadioButton_IntPtr( "INTERPIC", &border_style, 1 );
			igPopID();

			igNextColumn();
			igText( "Border bezels" );
			igNextColumn();

			igPushID_Ptr( &border_bezel_style );
			doresize |= igRadioButton_IntPtr( "Original", &border_bezel_style, 0 );
			igSameLine( 0, -1 );
			doresize |= igRadioButton_IntPtr( "Dithered", &border_bezel_style, 1 );
			igPopID();

			if( doresize )
			{
				R_SetViewSize (screenblocks, 0);
			}

			igNextColumn();
			igText( "Status bar border" );
			igNextColumn();

			int32_t style = ST_GetBorderTileStyle();

			igPushID_Ptr( &style );
			WorkingBool = false;
			WorkingBool |= igRadioButton_IntPtr( "IWAD defined", &style, 0 );
			igSameLine( 0, -1 );
			WorkingBool |= igRadioButton_IntPtr( "FLAT5_4", &style, 1 );
			igPopID();

			if( WorkingBool )
			{
				ST_SetBorderTileStyle( style, NULL );
			}

			igNextColumn();
			igText( "Fuzz style" );
			igNextColumn();
			igPushID_Ptr( &fuzz_style );
			igRadioButton_IntPtr( "Original", &fuzz_style, Fuzz_Original );
			igSameLine( 0, -1 );
			igRadioButton_IntPtr( "Adjusted", &fuzz_style, Fuzz_Adjusted );
			igSameLine( 0, -1 );
			igRadioButton_IntPtr( "Heatwave", &fuzz_style, Fuzz_Heatwave );
			if( igIsItemHovered( ImGuiHoveredFlags_None ) )
			{
				igBeginTooltip();
				igText( "Not implemented yet, defers to Adjusted" );
				igEndTooltip();
			}
			igPopID();

			igNextColumn();
			igText( "Wipe style" );
			igNextColumn();
			igPushID_Ptr( &wipe_style );
			igRadioButton_IntPtr( "Melt", &wipe_style, wipe_Melt );
			igPopID();

			igNextColumn();
			igText( "Messages" );
			igNextColumn();
			WorkingBool = !!showMessages;
			igPushID_Ptr( &showMessages );
			if( igCheckbox( "", &WorkingBool ) )
			{
				showMessages = (int32_t)WorkingBool;
				players[consoleplayer].message = DEH_String( !showMessages ? MSGOFF : MSGON );
				message_dontfuckwithme = true;
			}
			igPopID();
			igNextColumn();

			igText( "Display stats" );
			igNextColumn();
			WorkingBool = stats_style == stats_standard;
			igPushID_Ptr( &stats_style );
			if( igCheckbox( "", &WorkingBool ) )
			{
				stats_style = WorkingBool ? stats_standard : stats_none;
			}
			igPopID();
			igNextColumn();

			igColumns( 1, "", false );

			igPopScrollableArea();
			igEndTabItem();
		}

		if( igBeginTabItem( "Automap", NULL, ImGuiTabItemFlags_NoCloseWithMiddleMouseButton ) )
		{
			igPushScrollableArea( "Automap", zerosize );

			igColumns( 2, "automapcol", false );
			igSetColumnWidth( 0, columwidth );

			igText( "Style" );
			igNextColumn();

			igPushID_Ptr( &map_style );
			igRadioButton_IntPtr( "Custom", &map_style, MapStyle_Custom );
			igSameLine( 0, -1 );
			igRadioButton_IntPtr( "Original", &map_style, MapStyle_Original );
			igSameLine( 0, -1 );
			igRadioButton_IntPtr( "ZDoom", &map_style, MapStyle_ZDoom );
			igPopID();

			igNextColumn();

			if( map_style == MapStyle_Custom )
			{
				if( numlumps > 0 )
				{
					palette = (byte*)W_CacheLumpName( DEH_String( "PLAYPAL" ), PU_CACHE );
				}
				else
				{
					palette = defaultpalette;
				}

				extern mapstyledata_t map_styledatacustomdefault;

				igPushID_Ptr( &style->background );
				igColumns( 1, "", false );
				if( igCollapsingHeader_TreeNodeFlags( "Standard", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen ) )
				{
					igColumns( 2, "automapcol", false );

					M_DashboardDoMapColour( "Background", &style->background, &map_styledatacustomdefault.background, palette );
					igNextColumn();
					M_DashboardDoMapColour( "Grid", &style->grid, &map_styledatacustomdefault.grid, palette );
					igNextColumn();
					M_DashboardDoMapColour( "Computer Area Map", &style->areamap, &map_styledatacustomdefault.areamap, palette );
					igNextColumn();
					M_DashboardDoMapColour( "Walls", &style->walls, &map_styledatacustomdefault.walls, palette );
					igNextColumn();
					M_DashboardDoMapColour( "Teleporters", &style->teleporters, &map_styledatacustomdefault.teleporters, palette );
					igNextColumn();
					M_DashboardDoMapColour( "Hidden Lines", &style->linesecrets, &map_styledatacustomdefault.linesecrets, palette );
					igNextColumn();
					M_DashboardDoMapColour( "Secrets (found)", &style->sectorsecrets, &map_styledatacustomdefault.sectorsecrets, palette );
					igNextColumn();
					M_DashboardDoMapColour( "Secrets (undiscovered)", &style->sectorsecretsundiscovered, &map_styledatacustomdefault.sectorsecretsundiscovered, palette );
					igNextColumn();
					M_DashboardDoMapColour( "Floor Height Diff", &style->floorchange, &map_styledatacustomdefault.floorchange, palette );
					igNextColumn();
					M_DashboardDoMapColour( "Ceiling Height Diff", &style->ceilingchange, &map_styledatacustomdefault.ceilingchange, palette );
					igNextColumn();
					M_DashboardDoMapColour( "Player Arrow", &style->playerarrow, &map_styledatacustomdefault.playerarrow, palette );
					igNextColumn();
					M_DashboardDoMapColour( "Crosshair", &style->crosshair, &map_styledatacustomdefault.crosshair, palette );
					igNextColumn();
				}
				igPopID();

				igPushID_Ptr( &style->nochange );
				igColumns( 1, "", false );
				if( igCollapsingHeader_TreeNodeFlags( "With cheat on", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen ) )
				{
					igColumns( 2, "automapcol", false );
					M_DashboardDoMapColour( "No Height Diff", &style->nochange, &map_styledatacustomdefault.nochange, palette );
					igNextColumn();
					M_DashboardDoMapColour( "Thing", &style->things, &map_styledatacustomdefault.things, palette );
					igNextColumn();
					M_DashboardDoMapColour( "Living Monsters", &style->monsters_alive, &map_styledatacustomdefault.monsters_alive, palette );
					igNextColumn();
					M_DashboardDoMapColour( "Dead Monsters", &style->monsters_dead, &map_styledatacustomdefault.monsters_dead, palette );
					igNextColumn();
					M_DashboardDoMapColour( "Items (Counted)", &style->items_counted, &map_styledatacustomdefault.items_counted, palette );
					igNextColumn();
					M_DashboardDoMapColour( "Items (Uncounted)", &style->items_uncounted, &map_styledatacustomdefault.items_uncounted, palette );
					igNextColumn();
					M_DashboardDoMapColour( "Projectiles", &style->projectiles, &map_styledatacustomdefault.projectiles, palette );
					igNextColumn();
					M_DashboardDoMapColour( "Puffs", &style->puffs, &map_styledatacustomdefault.puffs, palette );
					igNextColumn();
				}
				igPopID();
			}

			igColumns( 1, "", false );

			igPopScrollableArea();
			igEndTabItem();
		}

		igEndTabBar();
	}

	debugwindow_tofocus = cat_none;
}

static void M_DashboardNewGame( const char* itemname, void* data )
{
	mapinfo_t* map = (mapinfo_t*)data;

	G_DeferedInitNew( dashboard_currgameskill, map, dashboard_currgameflags );
}

//static void M_DashboardRegisterEpisodeMaps( const char* category, episodedetails_t* episode, menufunc_t callback )
//{
//	char namebuffer[ 128 ];
//
//	mapdetails_t* curr = episode->maps;
//	
//	while( curr->mapnum != -1 )
//	{
//		memset( namebuffer, 0, sizeof( namebuffer ) );
//		sprintf( namebuffer, "%s|%s", category, DEH_String( curr->name ) );
//		M_RegisterDashboardButton( namebuffer, NULL, callback, curr );
//		++curr;
//	}
//}

static void M_DashboardRegisterEpisodeMaps( const char* category, episodeinfo_t* episode, menufunc_t callback )
{
	char namebuffer[ 256 ];

	for( int32_t mapind = 0 ; mapind < episode->num_maps; ++mapind )
	{
		mapinfo_t* curr = episode->all_maps[ mapind ];

		memset( namebuffer, 0, sizeof( namebuffer ) );
		sprintf( namebuffer, "%s|%s", category, DEH_String( curr->name.val ) );
		M_RegisterDashboardButton( namebuffer, NULL, callback, curr );
		++curr;
	}
}

void M_DashboardOptionsInit()
{
	extern int window_width;
	extern int window_height;
	extern int32_t render_match_window;

	int32_t index;

	window_width_working = window_width;
	window_height_working = window_height;

	for( index = 0; index < window_sizes_scaled_count; ++index )
	{
		if( window_width == window_sizes_scaled[ index ].width && window_height == window_sizes_scaled[ index ].height )
		{
			window_dimensions_current = window_sizes_scaled[ index ].asstring;
		}
	}

	if( !window_dimensions_current )
	{
		window_dimensions_current = "Custom";
	}

	if( render_match_window )
	{
		render_dimensions_current = window_size_match.asstring;
	}
	else
	{
		for( index = 0; index < render_sizes_count; ++index )
		{
			if( render_width == render_sizes[ index ].width && render_height == render_sizes[ index ].height )
			{
				render_dimensions_current = render_sizes[ index ].asstring;
			}
		}
	}
}

//
// M_Init
//
void M_Init (void)
{
	char episodestem[ 256 ];

    currentMenu = &MainDef;
    menuactive = 0;
    itemOn = currentMenu->lastOn;
    whichSkull = 0;
    skullAnimCounter = 10;
    screenSize = screenblocks - 3;
    messageToPrint = 0;
    messageString = NULL;
    messageLastMenuActive = menuactive;
    quickSaveSlot = -1;

    // Here we could catch other version dependencies,
    //  like HELP1/2, and four episodes.

    // The same hacks were used in the original Doom EXEs.

    if (gameversion >= exe_ultimate)
    {
        MainMenu[readthis].routine = M_ReadThis2;
        ReadDef2.prevMenu = NULL;
    }

    if (gameversion >= exe_final && gameversion <= exe_final2)
    {
        ReadDef2.routine = M_DrawReadThisCommercial;
    }

    if (gamemode == commercial)
    {
        MainMenu[readthis] = MainMenu[quitdoom];
        MainDef.numitems--;
        MainDef.y += 8;
        NewDef.prevMenu = &MainDef;
        ReadDef1.routine = M_DrawReadThisCommercial;
        ReadDef1.x = 330;
        ReadDef1.y = 165;
        ReadMenu1[rdthsempty1].routine = M_FinishReadThis;
    }

	if( current_game->num_episodes > 0 )
	{
		gameflow_episodes = Z_Malloc( sizeof( menuitem_t ) * current_game->num_episodes, PU_STATIC, NULL );

		for( int32_t curr = 0; curr < current_game->num_episodes; ++curr )
		{
			episodeinfo_t* currep = current_game->episodes[ curr ];
			menuitem_t* curritem = &gameflow_episodes[ curr ];

			curritem->alphaKey = tolower( currep->name.val[ 0 ] );
			if( currep->name_patch_lump.val )
			{
				M_StringCopy( curritem->name, currep->name_patch_lump.val, 10 );
			}
			curritem->routine = M_Episode;
			curritem->status = 1;
		}

		EpiDef.menuitems = gameflow_episodes;
		EpiDef.numitems = current_game->num_episodes;
	}
	else
	{
		// Versions of doom.exe before the Ultimate Doom release only had
		// three episodes; if we're emulating one of those then don't try
		// to show episode four. If we are, then do show episode four
		// (should crash if missing).
		if (gameversion < exe_ultimate)
		{
			EpiDef.numitems--;
		}
		// chex.exe shows only one episode.
		else if (gameversion == exe_chex)
		{
			EpiDef.numitems = 1;
		}
	}

    opldev = M_CheckParm("-opldev") > 0;

	M_DashboardOptionsInit();

	extern int32_t sleeponzerotics;

	dashboardopensound = sfx_swtchn;
	dashboardclosesound = sfx_swtchx;

	M_RegisterDashboardCategory( "Game|New" );

	M_RegisterDashboardRadioButton( "Game|New|No skill", NULL, &dashboard_currgameskill, -1 );
	M_RegisterDashboardRadioButton( "Game|New|I'm too young to die", NULL, &dashboard_currgameskill, 0 );
	M_RegisterDashboardRadioButton( "Game|New|Hey, not too rough", NULL, &dashboard_currgameskill, 1 );
	M_RegisterDashboardRadioButton( "Game|New|Hurt me plenty", NULL, &dashboard_currgameskill, 2 );
	M_RegisterDashboardRadioButton( "Game|New|Ultra violence", NULL, &dashboard_currgameskill, 3 );
	M_RegisterDashboardRadioButton( "Game|New|Nightmare", NULL, &dashboard_currgameskill, 4 );
	M_RegisterDashboardSeparator( "Game|New" );
	M_RegisterDashboardCheckboxFlag( "Game|New|Pistol Starts", NULL, &dashboard_currgameflags, GF_PistolStarts );
	M_RegisterDashboardCheckboxFlag( "Game|New|Loop one level", NULL, &dashboard_currgameflags, GF_LoopOneLevel );
	M_RegisterDashboardSeparator( "Game|New" );

	if( current_game->num_episodes == 1 )
	{
		M_DashboardRegisterEpisodeMaps( "Game|New", current_game->episodes[ 0 ], &M_DashboardNewGame );
	}
	else
	{
		for( int32_t epindex = 0; epindex < current_game->num_episodes; ++epindex  )
		{
			memset( episodestem, 0, sizeof( episodestem ) );
			sprintf( episodestem, "Game|New|%s", current_game->episodes[ epindex ]->name.val );
			M_DashboardRegisterEpisodeMaps( episodestem, current_game->episodes[ epindex ], &M_DashboardNewGame );
		}
	}

	M_RegisterDashboardWindow( "Game|Options", "Game Options", 500, 500, &debugwindow_options, Menu_Normal, &M_DashboardOptionsWindow );
	M_RegisterDashboardCheckboxFlag( "Render|Renderer in lockstep with game", "Just means we can run zero tics really", &sleeponzerotics, 0x1 );

}

