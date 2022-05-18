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

#include "m_misc.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#include "r_local.h"


#include "hu_stuff.h"

#include "g_game.h"

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

#include "m_debugmenu.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimguiglue.h"


typedef struct mapdetails_s
{
	int32_t				episodenum;
	int32_t				mapnum;
	const char*			name;
} mapdetails_t;

typedef struct episodedetails_s
{
	int32_t				num;
	const char*			name;
	mapdetails_t*		maps;
} episodedetails_t;

typedef struct gamedetails_s
{
	const char*			name;
	episodedetails_t*	episodes;
} gamedetails_t;

static gamedetails_t game_doomshareware =
{
	"Doom shareware", (episodedetails_t[]){
		1, "Knee-Deep In The Dead", (mapdetails_t[]){
			{ 1,	1,		HUSTR_E1M1	},
			{ 1,	2,		HUSTR_E1M2	},
			{ 1,	3,		HUSTR_E1M3	},
			{ 1,	4,		HUSTR_E1M4	},
			{ 1,	5,		HUSTR_E1M5	},
			{ 1,	6,		HUSTR_E1M6	},
			{ 1,	7,		HUSTR_E1M7	},
			{ 1,	8,		HUSTR_E1M8	},
			{ 1,	9,		HUSTR_E1M9	},
			{ -1,	-1,		NULL		}
		},
		{ -1, NULL, NULL }
	}
};

static gamedetails_t game_doomregistered =
{
	"Doom registered", (episodedetails_t[]){
		1, "Knee-Deep In The Dead", (mapdetails_t[]){
			{ 1,	1,		HUSTR_E1M1	},
			{ 1,	2,		HUSTR_E1M2	},
			{ 1,	3,		HUSTR_E1M3	},
			{ 1,	4,		HUSTR_E1M4	},
			{ 1,	5,		HUSTR_E1M5	},
			{ 1,	6,		HUSTR_E1M6	},
			{ 1,	7,		HUSTR_E1M7	},
			{ 1,	8,		HUSTR_E1M8	},
			{ 1,	9,		HUSTR_E1M9	},
			{ -1,	-1,		NULL		}
		},

		2, "The Shores of Hell", (mapdetails_t[]){
			{ 2,	1,		HUSTR_E2M1	},
			{ 2,	2,		HUSTR_E2M2	},
			{ 2,	3,		HUSTR_E2M3	},
			{ 2,	4,		HUSTR_E2M4	},
			{ 2,	5,		HUSTR_E2M5	},
			{ 2,	6,		HUSTR_E2M6	},
			{ 2,	7,		HUSTR_E2M7	},
			{ 2,	8,		HUSTR_E2M8	},
			{ 2,	9,		HUSTR_E2M9	},
			{ -1,	-1,		NULL		}
		},

		3, "Inferno", (mapdetails_t[]){
			{ 3,	1,		HUSTR_E3M1	},
			{ 3,	2,		HUSTR_E3M2	},
			{ 3,	3,		HUSTR_E3M3	},
			{ 3,	4,		HUSTR_E3M4	},
			{ 3,	5,		HUSTR_E3M5	},
			{ 3,	6,		HUSTR_E3M6	},
			{ 3,	7,		HUSTR_E3M7	},
			{ 3,	8,		HUSTR_E3M8	},
			{ 3,	9,		HUSTR_E3M9	},
			{ -1,	-1,		NULL		}
		},
		{ -1, NULL, NULL }
	}
};

static gamedetails_t game_doomultimate =
{
	"Ultimate Doom", (episodedetails_t[]){
		1, "Knee-Deep In The Dead", (mapdetails_t[]){
			{ 1,	1,		HUSTR_E1M1	},
			{ 1,	2,		HUSTR_E1M2	},
			{ 1,	3,		HUSTR_E1M3	},
			{ 1,	4,		HUSTR_E1M4	},
			{ 1,	5,		HUSTR_E1M5	},
			{ 1,	6,		HUSTR_E1M6	},
			{ 1,	7,		HUSTR_E1M7	},
			{ 1,	8,		HUSTR_E1M8	},
			{ 1,	9,		HUSTR_E1M9	},
			{ -1,	-1,		NULL		}
		},

		2, "The Shores of Hell", (mapdetails_t[]){
			{ 2,	1,		HUSTR_E2M1	},
			{ 2,	2,		HUSTR_E2M2	},
			{ 2,	3,		HUSTR_E2M3	},
			{ 2,	4,		HUSTR_E2M4	},
			{ 2,	5,		HUSTR_E2M5	},
			{ 2,	6,		HUSTR_E2M6	},
			{ 2,	7,		HUSTR_E2M7	},
			{ 2,	8,		HUSTR_E2M8	},
			{ 2,	9,		HUSTR_E2M9	},
			{ -1,	-1,		NULL		}
		},

		3, "Inferno", (mapdetails_t[]){
			{ 3,	1,		HUSTR_E3M1	},
			{ 3,	2,		HUSTR_E3M2	},
			{ 3,	3,		HUSTR_E3M3	},
			{ 3,	4,		HUSTR_E3M4	},
			{ 3,	5,		HUSTR_E3M5	},
			{ 3,	6,		HUSTR_E3M6	},
			{ 3,	7,		HUSTR_E3M7	},
			{ 3,	8,		HUSTR_E3M8	},
			{ 3,	9,		HUSTR_E3M9	},
			{ -1,	-1,		NULL		}
		},

		4, "Thy Flesh Consumed", (mapdetails_t[]){
			{ 4,	1,		HUSTR_E4M1	},
			{ 4,	2,		HUSTR_E4M2	},
			{ 4,	3,		HUSTR_E4M3	},
			{ 4,	4,		HUSTR_E4M4	},
			{ 4,	5,		HUSTR_E4M5	},
			{ 4,	6,		HUSTR_E4M6	},
			{ 4,	7,		HUSTR_E4M7	},
			{ 4,	8,		HUSTR_E4M8	},
			{ 4,	9,		HUSTR_E4M9	},
			{ -1,	-1,		NULL		}
		},

		5, "(Fifth episode)", (mapdetails_t[]){
			{ 5,	1,		"E5M1"		},
			{ 5,	2,		"E5M2"		},
			{ 5,	3,		"E5M3"		},
			{ 5,	4,		"E5M4"		},
			{ 5,	5,		"E5M5"		},
			{ 5,	6,		"E5M6"		},
			{ 5,	7,		"E5M7"		},
			{ 5,	8,		"E5M8"		},
			{ 5,	9,		"E5M9"		},
			{ -1,	-1,		NULL		}
		},
		{ -1, NULL, NULL }
	}
};

static gamedetails_t game_chexquest =
{
	"Chex Quest", (episodedetails_t[]){
		1, "Chex Quest", (mapdetails_t[]){
			{ 1,	1,		HUSTR_E1M1	},
			{ 1,	2,		HUSTR_E1M2	},
			{ 1,	3,		HUSTR_E1M3	},
			{ 1,	4,		HUSTR_E1M4	},
			{ 1,	5,		HUSTR_E1M5	},
			{ -1,	-1,		NULL		}
		},
		{ -1, NULL, NULL }
	}
};

static gamedetails_t game_doom2 =
{
	"Doom II - Hell On Earth", (episodedetails_t[]){
		0, "Hell On Earth", (mapdetails_t[]){
			{ 1,	1,		HUSTR_1		},
			{ 1,	2,		HUSTR_2		},
			{ 1,	3,		HUSTR_3		},
			{ 1,	4,		HUSTR_4		},
			{ 1,	5,		HUSTR_5		},
			{ 1,	6,		HUSTR_6		},
			{ 1,	7,		HUSTR_7		},
			{ 1,	8,		HUSTR_8		},
			{ 1,	9,		HUSTR_9		},
			{ 1,	10,		HUSTR_10	},
			{ 1,	11,		HUSTR_11	},
			{ 1,	12,		HUSTR_12	},
			{ 1,	13,		HUSTR_13	},
			{ 1,	14,		HUSTR_14	},
			{ 1,	15,		HUSTR_15	},
			{ 1,	16,		HUSTR_16	},
			{ 1,	17,		HUSTR_17	},
			{ 1,	18,		HUSTR_18	},
			{ 1,	19,		HUSTR_19	},
			{ 1,	20,		HUSTR_20	},
			{ 1,	21,		HUSTR_21	},
			{ 1,	22,		HUSTR_22	},
			{ 1,	23,		HUSTR_23	},
			{ 1,	24,		HUSTR_24	},
			{ 1,	25,		HUSTR_25	},
			{ 1,	26,		HUSTR_26	},
			{ 1,	27,		HUSTR_27	},
			{ 1,	28,		HUSTR_28	},
			{ 1,	29,		HUSTR_29	},
			{ 1,	30,		HUSTR_30	},
			{ 1,	31,		HUSTR_31	},
			{ 1,	32,		HUSTR_32	},
			{ -1,	-1,		NULL		}
		},
		{ -1, NULL, NULL }
	}
};

static gamedetails_t game_evilution =
{
	"TNT - Evilution", (episodedetails_t[]){
		0, "Evilution", (mapdetails_t[]){
			{ 1,	1,		THUSTR_1	},
			{ 1,	2,		THUSTR_2	},
			{ 1,	3,		THUSTR_3	},
			{ 1,	4,		THUSTR_4	},
			{ 1,	5,		THUSTR_5	},
			{ 1,	6,		THUSTR_6	},
			{ 1,	7,		THUSTR_7	},
			{ 1,	8,		THUSTR_8	},
			{ 1,	9,		THUSTR_9	},
			{ 1,	10,		THUSTR_10	},
			{ 1,	11,		THUSTR_11	},
			{ 1,	12,		THUSTR_12	},
			{ 1,	13,		THUSTR_13	},
			{ 1,	14,		THUSTR_14	},
			{ 1,	15,		THUSTR_15	},
			{ 1,	16,		THUSTR_16	},
			{ 1,	17,		THUSTR_17	},
			{ 1,	18,		THUSTR_18	},
			{ 1,	19,		THUSTR_19	},
			{ 1,	20,		THUSTR_20	},
			{ 1,	21,		THUSTR_21	},
			{ 1,	22,		THUSTR_22	},
			{ 1,	23,		THUSTR_23	},
			{ 1,	24,		THUSTR_24	},
			{ 1,	25,		THUSTR_25	},
			{ 1,	26,		THUSTR_26	},
			{ 1,	27,		THUSTR_27	},
			{ 1,	28,		THUSTR_28	},
			{ 1,	29,		THUSTR_29	},
			{ 1,	30,		THUSTR_30	},
			{ 1,	31,		THUSTR_31	},
			{ 1,	32,		THUSTR_32	},
			{ 1,	33,		"MAP33"		},
			{ 1,	34,		"MAP34"		},
			{ 1,	35,		"MAP35"		},
			{ -1,	-1,		NULL		}
		},
		{ -1, NULL, NULL }
	}
};

static gamedetails_t game_plutonia =
{
	"The Plutonia Experiment", (episodedetails_t[]){
		0, "The Plutonia Experiment", (mapdetails_t[]){
			{ 1,	1,		PHUSTR_1	},
			{ 1,	2,		PHUSTR_2	},
			{ 1,	3,		PHUSTR_3	},
			{ 1,	4,		PHUSTR_4	},
			{ 1,	5,		PHUSTR_5	},
			{ 1,	6,		PHUSTR_6	},
			{ 1,	7,		PHUSTR_7	},
			{ 1,	8,		PHUSTR_8	},
			{ 1,	9,		PHUSTR_9	},
			{ 1,	10,		PHUSTR_10	},
			{ 1,	11,		PHUSTR_11	},
			{ 1,	12,		PHUSTR_12	},
			{ 1,	13,		PHUSTR_13	},
			{ 1,	14,		PHUSTR_14	},
			{ 1,	15,		PHUSTR_15	},
			{ 1,	16,		PHUSTR_16	},
			{ 1,	17,		PHUSTR_17	},
			{ 1,	18,		PHUSTR_18	},
			{ 1,	19,		PHUSTR_19	},
			{ 1,	20,		PHUSTR_20	},
			{ 1,	21,		PHUSTR_21	},
			{ 1,	22,		PHUSTR_22	},
			{ 1,	23,		PHUSTR_23	},
			{ 1,	24,		PHUSTR_24	},
			{ 1,	25,		PHUSTR_25	},
			{ 1,	26,		PHUSTR_26	},
			{ 1,	27,		PHUSTR_27	},
			{ 1,	28,		PHUSTR_28	},
			{ 1,	29,		PHUSTR_29	},
			{ 1,	30,		PHUSTR_30	},
			{ 1,	31,		PHUSTR_31	},
			{ 1,	32,		PHUSTR_32	},
			{ -1,	-1,		NULL		}
		},
		{ -1, NULL, NULL }
	}
};

static gamedetails_t*	debugmenu_currgame			= NULL;
static int32_t			debugmenu_currgameepisodes	= 0;
static int32_t			debugmenu_currgameskill		= sk_medium;
static int32_t			debugmenu_currgameflags		= GF_None;

extern patch_t*		hu_font[HU_FONTSIZE];
extern boolean		message_dontfuckwithme;

extern boolean		chat_on;		// in heads-up code

//
// defaulted values
//
int			mouseSensitivity = 5;

// Show messages has default, 0 = off, 1 = on
int			showMessages = 1;
	

// Blocky mode, has default, 0 = high, 1 = normal
int			detailLevel = 0;
int			screenblocks = 9;

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
boolean			messageNeedsInput;

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
static boolean          joypadSave = false; // was the save action initiated by joypad?
// old save description before edit
char			saveOldString[SAVESTRINGSIZE];  

boolean			inhelpscreens;
boolean			menuactive;
int32_t			debugmenuclosesound = sfx_swtchx;

#define SKULLXOFF		-32
#define LINEHEIGHT		16

extern boolean		sendpause;
char			savegamestrings[10][SAVESTRINGSIZE];

char	endstring[160];

static boolean opldev;

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
static void M_StartMessage(const char *string, void *routine, boolean input);
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




//
// Read This Menus
// Had a "quick hack to fix romero bug"
//
void M_DrawReadThis1(void)
{
    inhelpscreens = true;

    V_DrawPatchDirect(0, 0, W_CacheLumpName(DEH_String("HELP2"), PU_CACHE));
}



//
// Read This Menus - optional second page.
//
void M_DrawReadThis2(void)
{
    inhelpscreens = true;

    // We only ever draw the second page if this is 
    // gameversion == exe_doom_1_9 and gamemode == registered

    V_DrawPatchDirect(0, 0, W_CacheLumpName(DEH_String("HELP1"), PU_CACHE));
}

void M_DrawReadThisCommercial(void)
{
    inhelpscreens = true;

    V_DrawPatchDirect(0, 0, W_CacheLumpName(DEH_String("HELP"), PU_CACHE));
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
		
    G_DeferedInitNew(nightmare,epi+1,1, false);
    M_ClearMenus ();
}

void M_ChooseSkill(int choice)
{
    if (choice == nightmare)
    {
	M_StartMessage(DEH_String(NIGHTMARE),M_VerifyNightmare,true);
	return;
    }
	
    G_DeferedInitNew(choice,epi+1,1, false);
    M_ClearMenus ();
}

void M_Episode(int choice)
{
    if ( (gamemode == shareware)
	 && choice)
    {
	M_StartMessage(DEH_String(SWSTRING),NULL,false);
	M_SetupNextMenu(&ReadDef1);
	return;
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
    M_SetupNextMenu(&OptionsDef);
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
    detailLevel = 1 - detailLevel;

    R_SetViewSize (screenblocks, detailLevel);

    if (!detailLevel)
	players[consoleplayer].message = DEH_String(DETAILHI);
    else
	players[consoleplayer].message = DEH_String(DETAILLO);
}




void M_SizeDisplay(int choice)
{
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
	

    R_SetViewSize (screenblocks, detailLevel);
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
  boolean	input )
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

static boolean IsNullKey(int key)
{
    return key == KEY_PAUSE || key == KEY_CAPSLOCK
        || key == KEY_SCRLCK || key == KEY_NUMLOCK;
}

//
// CONTROL PANEL
//

static boolean M_DebugResponder( event_t* ev )
{
	return true;
}

//
// M_Responder
//
boolean M_Responder (event_t* ev)
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

		if( debugmenuactive )
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
	if (ev->type == ev_keydown && ev->data1 == key_menu_debug)
	{
		debugmenuactive = !debugmenuactive;
		S_StartSound(NULL, debugmenuactive ? sfx_swtchn : sfx_swtchx);
		return true;
	}
#endif // USE_IMGUI

	if( debugmenuactive )
	{
		return M_DebugResponder( ev );
	}

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
	    M_StartControlPanel ();
	    currentMenu = &SoundDef;
	    itemOn = sfx_vol;
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
	    boolean foundnewline = false;

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

static boolean debugwindow_options = false;

typedef struct windowsizes_s
{
	int32_t			width;
	int32_t			height;
	const char*		asstring;
} windowsizes_t;

#define WINDOWDIM( w, h )	{ w, h, #w "x" #h }
#define WINDOW_MINWIDTH 800
#define WINDOW_MINHEIGHT 600

static windowsizes_t window_sizes_scaled[] =
{
	// It honeslty doesn't make much sense to allow lower resolutions than 800x600 for Rum and Raisin.
	// Uncomment if your port needs them. But be warned that the ImGui overlay will be basically
	// impossible to use.

	//WINDOWDIM( 320,		240		),
	//WINDOWDIM( 512,		400		),
	//WINDOWDIM( 640,		480		),
	WINDOWDIM( 800,		600		),
	WINDOWDIM( 960,		720		),
	WINDOWDIM( 1024,	768		),
	WINDOWDIM( 1280,	960		),
	WINDOWDIM( 1600,	1200	),
	WINDOWDIM( 1920,	1440	),
	WINDOWDIM( 2560,	1920	),
	WINDOWDIM( 3840,	2880	),
	WINDOWDIM( 1280,	720		),
	WINDOWDIM( 1600,	900		),
	WINDOWDIM( 1920,	1080	),
	WINDOWDIM( 1600,	686		),
	WINDOWDIM( 1920,	822		),
	WINDOWDIM( 2380,	1020	),
};
static int32_t window_sizes_scaled_count = sizeof( window_sizes_scaled ) / sizeof( *window_sizes_scaled );
static int32_t window_width_working;
static int32_t window_height_working;
static const char* window_dimensions_current = NULL;

static windowsizes_t render_sizes[] =
{
	WINDOWDIM( 320,		200 ),
	WINDOWDIM( 640,		400 ),
	WINDOWDIM( 1280,	800 ),
	WINDOWDIM( 1706,	800 ),
	WINDOWDIM( 1920,	900 ),
	WINDOWDIM( 1600,	1000 ),
	WINDOWDIM( 2132,	1000 ),
	WINDOWDIM( 2800,	1000 ),
	WINDOWDIM( 1920,	1200 ),
	WINDOWDIM( 2560,	1200 ),
	WINDOWDIM( 2560,	1600 ),

};
static int32_t render_sizes_count = sizeof( render_sizes ) / sizeof( *render_sizes );
static int32_t render_width_working;
static int32_t render_height_working;
static const char* render_dimensions_current = NULL;

static const char* span_override_strings[] =
{
	"None",
	"Original",
	"Polyraster Log2(4)",
	"Polyraster Log2(8)",
	"Polyraster Log2(16)",
	"Polyraster Log2(32)",
};
static int32_t span_override_strings_count = sizeof( span_override_strings ) / sizeof( *span_override_strings );

static void M_DebugMenuDoColour( const char* itemname, int32_t* colourindex, byte* palette )
{
	byte* paletteentry;
	ImU32 colour;
	ImVec2 coloursize;
	ImVec2 popupspacing;
	int32_t paletteindex;

	coloursize.x = coloursize.y = 22.f;
	popupspacing.x = popupspacing.y = 2.f;

	igPushIDPtr( colourindex );

	igText( itemname );
	igNextColumn();

	paletteentry = palette + *colourindex * 3;
	colour = IM_COL32( paletteentry[ 0 ], paletteentry[ 1 ], paletteentry[ 2 ], 255 );

	igPushStyleColorU32( ImGuiCol_Button, colour );
	igPushStyleColorU32( ImGuiCol_Border, IM_COL32_BLACK );
	igPushStyleVarFloat( ImGuiStyleVar_FrameBorderSize, 2.f );
	if( igButton( " ", coloursize ) )
	{
		igOpenPopup( "colorpicker", ImGuiPopupFlags_None );
	}
	igPopStyleVar( 1 );
	igPopStyleColor( 2 );

	if( igBeginPopup( "colorpicker", ImGuiWindowFlags_None ) )
	{
		paletteentry = palette;

		igPushStyleVarVec2( ImGuiStyleVar_ItemSpacing, popupspacing );

		for( paletteindex = 0; paletteindex < 256; ++paletteindex )
		{
			igPushIDPtr( paletteentry );

			colour = IM_COL32( paletteentry[ 0 ], paletteentry[ 1 ], paletteentry[ 2 ], 255 );
			igPushStyleColorU32( ImGuiCol_Button, colour );
			igPushStyleColorU32( ImGuiCol_Border, paletteindex == *colourindex ? IM_COL32_WHITE : IM_COL32_BLACK );
			igPushStyleVarFloat( ImGuiStyleVar_FrameBorderSize, 1.f );
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

static void M_DebugMenuDoMapColour( const char* itemname, int32_t* colourindex, int32_t* isblinking, byte* palette )
{
	M_DebugMenuDoColour( itemname, colourindex, palette );
	igSameLine( 0, -1 );
	igPushIDPtr( colourindex );
	igCheckboxFlags( "Pulsating", isblinking, 0x1 );
	igPopID();
}

static void M_DebugMenuOptionsWindow( const char* itemname, void* data )
{
	extern int fullscreen;
	extern int window_width;
	extern int window_height;
	extern int display_width;
	extern int display_height;
	extern int border_style;
	extern int border_bezel_style;
	extern int32_t fuzz_style;
	extern int32_t span_override;

	bool WorkingBool = false;
	int32_t WorkingInt = 0;

	bool doresize = false;

	int32_t currsizeindex = 0;
	int32_t index;
	bool selected;
	ImVec2 zerosize = { 0, 0 };

	byte* palette = NULL;

	mapstyledata_t* style = map_styledata;

	if( igBeginTabBar( "Doom Options tabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_NoCloseWithMiddleMouseButton ) )
	{
		if( igBeginTabItem( "Mouse", NULL, ImGuiTabItemFlags_NoCloseWithMiddleMouseButton ) )
		{
			igSliderInt( "Sensitivity", &mouseSensitivity, 0, 30, NULL, ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput );

			WorkingInt = (int32_t)( mouse_acceleration * 10.f );
			if( igSliderInt( "Acceleration", &WorkingInt, 10, 50, NULL, ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput ) )
			{
				mouse_acceleration = (float_t)WorkingInt * 0.1f;
			}
			igSliderInt( "Threshold", &mouse_threshold, 0, 32, NULL, ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput );

			igEndTabItem();
		}

		if( igBeginTabItem( "Screen", NULL, ImGuiTabItemFlags_NoCloseWithMiddleMouseButton ) )
		{
			igColumns( 2, "", false );
			igSetColumnWidth( 0, 200.f );

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
			igPushIDPtr( &window_dimensions_current );
			if( igBeginCombo( "", window_dimensions_current, ImGuiComboFlags_None ) )
			{
				for( index = 0; index < window_sizes_scaled_count; ++index )
				{
					selected = window_width == window_sizes_scaled[ index ].width && window_height == window_sizes_scaled[ index ].height;
					if( igSelectableBool( window_sizes_scaled[ index ].asstring, selected, ImGuiSelectableFlags_None, zerosize ) )
					{
						window_dimensions_current = window_sizes_scaled[ index ].asstring;
						window_width_working = window_sizes_scaled[ index ].width;
						window_height_working = window_sizes_scaled[ index ].height;
						I_SetWindowDimensions( window_width_working, window_height_working );
					}
				}
				igEndCombo();
			}
			igPopID();
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
			igPushIDPtr( &fullscreen );
			if( igCheckbox( "", &WorkingBool ) )
			{
				I_ToggleFullScreen();
			}
			igPopID();

			igColumns( 1, "", false );
			igNewLine();

			igColumns( 2, "", false );
			igSetColumnWidth( 0, 200.f );

			igText( "Separate backbuffer size" );
			igNextColumn();
			igText( "<coming soon>" );
			igNextColumn();

			igText( "Predefined backbuffer sizes" );
			igNextColumn();
			igPushItemWidth( 180.f );
			igPushIDPtr( &render_dimensions_current );
			if( igBeginCombo( "", render_dimensions_current, ImGuiComboFlags_None ) )
			{
				for( index = 0; index < render_sizes_count; ++index )
				{
					selected = render_width == render_sizes[ index ].width && render_height == render_sizes[ index ].height;
					if( igSelectableBool( render_sizes[ index ].asstring, selected, ImGuiSelectableFlags_None, zerosize ) )
					{
						render_dimensions_current = render_sizes[ index ].asstring;
						render_width_working = render_sizes[ index ].width;
						render_height_working = render_sizes[ index ].height;
						I_SetRenderDimensions( render_width_working, render_height_working );
					}
				}
				igEndCombo();
			}
			igPopID();
			igPopItemWidth();
			igNextColumn();

			igText( "Custom backbuffer size" );
			igNextColumn();
			igText( "<coming soon>" );
			igNextColumn();

			igColumns( 1, "", false );
			igEndTabItem();
		}

		if( igBeginTabItem( "Sound", NULL, ImGuiTabItemFlags_NoCloseWithMiddleMouseButton ) )
		{
			if( igSliderInt( "Effects", &sfxVolume, 0, 15, NULL, ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput ) )
			{
				S_SetSfxVolume( sfxVolume * 8 );
			}
			if( igSliderInt( "Music", &musicVolume, 0, 15, NULL, ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput ) )
			{
				S_SetSfxVolume( sfxVolume * 8 );
			}

			igEndTabItem();
		}

		if( igBeginTabItem( "View", NULL, ImGuiTabItemFlags_NoCloseWithMiddleMouseButton ) )
		{
			igColumns( 2, "", false );
			igSetColumnWidth( 0, 200.f );

			igText( "Screen size" );
			igNextColumn();
			igPushIDPtr( &screenblocks );
			igPushItemWidth( 200.f );
			if( igSliderInt( "", &screenblocks, 3, 11, NULL, ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput ) )
			{
				screenSize = screenblocks - 3;
				R_SetViewSize (screenblocks, detailLevel);
			}
			igPopItemWidth();
			igPopID();

			igNextColumn();
			igText( "Border style" );
			igNextColumn();

			igPushIDPtr( &border_style );
			doresize |= igRadioButtonIntPtr( "Original", &border_style, 0 );
			igSameLine( 0, -1 );
			doresize |= igRadioButtonIntPtr( "INTERPIC", &border_style, 1 );
			igPopID();

			igNextColumn();
			igText( "Border bezels" );
			igNextColumn();

			igPushIDPtr( &border_bezel_style );
			doresize |= igRadioButtonIntPtr( "Original", &border_bezel_style, 0 );
			igSameLine( 0, -1 );
			doresize |= igRadioButtonIntPtr( "Dithered", &border_bezel_style, 1 );
			igPopID();

			if( doresize )
			{
				R_SetViewSize (screenblocks, detailLevel);
			}

			igNextColumn();
			igText( "Fuzz style" );
			igNextColumn();
			igPushIDPtr( &fuzz_style );
			igRadioButtonIntPtr( "Original", &fuzz_style, Fuzz_Original );
			igSameLine( 0, -1 );
			igRadioButtonIntPtr( "Adjusted", &fuzz_style, Fuzz_Adjusted );
			igSameLine( 0, -1 );
			igRadioButtonIntPtr( "Heatwave", &fuzz_style, Fuzz_Heatwave );
			if( igIsItemHovered( ImGuiHoveredFlags_None ) )
			{
				igBeginTooltip();
				igText( "Not implemented yet, defers to Adjusted" );
				igEndTooltip();
			}
			igPopID();

			igNextColumn();
			igText( "Span func override" );
			igNextColumn();
			igPushIDPtr( &span_override );
			if( igBeginCombo( "", span_override_strings[ span_override ], ImGuiComboFlags_None ) )
			{
				for( index = 0; index < span_override_strings_count; ++index )
				{
					selected = index == span_override;
					if( igSelectableBool( span_override_strings[ index ], selected, ImGuiSelectableFlags_None, zerosize ) )
					{
						span_override = index;
					}
				}
				igEndCombo();
			}
			igPopID();

			igNextColumn();
			igText( "Low detail" );
			igNextColumn();
			WorkingBool = !!detailLevel;
			igPushIDPtr( &detailLevel );
			if( igCheckbox( "", &WorkingBool ) )
			{
				detailLevel = (int32_t)WorkingBool;
				players[consoleplayer].message = DEH_String( !detailLevel ? DETAILHI : DETAILLO );
				R_SetViewSize (screenblocks, detailLevel);
			}
			igPopID();

			igNextColumn();
			igText( "Messages" );
			igNextColumn();
			WorkingBool = !!showMessages;
			igPushIDPtr( &showMessages );
			if( igCheckbox( "", &WorkingBool ) )
			{
				showMessages = (int32_t)WorkingBool;
				players[consoleplayer].message = DEH_String( !showMessages ? MSGOFF : MSGON );
				message_dontfuckwithme = true;
			}
			igPopID();

			igColumns( 1, "", false );

			igEndTabItem();
		}

		if( igBeginTabItem( "Automap", NULL, ImGuiTabItemFlags_NoCloseWithMiddleMouseButton ) )
		{
			igColumns( 2, "", false );
			igSetColumnWidth( 0, 200.f );

			igText( "Style" );
			igNextColumn();

			igPushIDPtr( &map_style );
			igRadioButtonIntPtr( "Custom", &map_style, MapStyle_Custom );
			igSameLine( 0, -1 );
			igRadioButtonIntPtr( "Original", &map_style, MapStyle_Original );
			igSameLine( 0, -1 );
			igRadioButtonIntPtr( "ZDoom", &map_style, MapStyle_ZDoom );
			igPopID();

			igNextColumn();

			if( map_style == MapStyle_Custom )
			{
				palette = (byte*)W_CacheLumpName( DEH_String( "PLAYPAL" ), PU_CACHE );

				M_DebugMenuDoMapColour( "Background", &style->background, &style->background_flags, palette );
				igNextColumn();
				M_DebugMenuDoMapColour( "Grid", &style->grid, &style->grid_flags, palette );
				igNextColumn();
				M_DebugMenuDoMapColour( "Computer Area Map", &style->areamap, &style->areamap_flags, palette );
				igNextColumn();
				M_DebugMenuDoMapColour( "Walls", &style->walls, &style->walls_flags, palette );
				igNextColumn();
				M_DebugMenuDoMapColour( "Teleporters", &style->teleporters, &style->teleporters_flags, palette );
				igNextColumn();
				M_DebugMenuDoMapColour( "Hidden Lines", &style->linesecrets, &style->linesecrets_flags, palette );
				igNextColumn();
				M_DebugMenuDoMapColour( "Secret Sectors", &style->sectorsecrets, &style->sectorsecrets_flags, palette );
				igNextColumn();
				M_DebugMenuDoMapColour( "Floor Height Diff", &style->floorchange, &style->floorchange_flags, palette );
				igNextColumn();
				M_DebugMenuDoMapColour( "Ceiling Height Diff", &style->ceilingchange, &style->ceilingchange_flags, palette );
				igNextColumn();
				M_DebugMenuDoMapColour( "No Height Diff", &style->nochange, &style->nochange_flags, palette );
				igNextColumn();
				M_DebugMenuDoMapColour( "Thing", &style->things, &style->things_flags, palette );
				igNextColumn();
				M_DebugMenuDoMapColour( "Living Monsters", &style->monsters_alive, &style->monsters_alive_flags, palette );
				igNextColumn();
				M_DebugMenuDoMapColour( "Dead Monsters", &style->monsters_dead, &style->monsters_dead_flags, palette );
				igNextColumn();
				M_DebugMenuDoMapColour( "Items (Counted)", &style->items_counted, &style->items_counted_flags, palette );
				igNextColumn();
				M_DebugMenuDoMapColour( "Items (Uncounted)", &style->items_uncounted, &style->items_uncounted_flags, palette );
				igNextColumn();
				M_DebugMenuDoMapColour( "Projectiles", &style->projectiles, &style->projectiles_flags, palette );
				igNextColumn();
				M_DebugMenuDoMapColour( "Puffs", &style->puffs, &style->puffs_flags, palette );
				igNextColumn();
				M_DebugMenuDoMapColour( "Player Arrow", &style->playerarrow, &style->playerarrow_flags, palette );
				igNextColumn();
				M_DebugMenuDoMapColour( "Crosshair", &style->crosshair, &style->crosshair_flags, palette );
				igNextColumn();
			}

			igColumns( 1, "", false );

			igEndTabItem();
		}

		igEndTabBar();
	}
}

static void M_DebugMenuNewGame( const char* itemname, void* data )
{
	mapdetails_t* map = (mapdetails_t*)data;

	G_DeferedInitNew( debugmenu_currgameskill, map->episodenum, map->mapnum, debugmenu_currgameflags );
}

static void M_DebugMenuRegisterEpisodeMaps( const char* category, episodedetails_t* episode, menufunc_t callback )
{
	char namebuffer[ 128 ];

	mapdetails_t* curr = episode->maps;
	
	while( curr->mapnum != -1 )
	{
		memset( namebuffer, 0, sizeof( namebuffer ) );
		sprintf( namebuffer, "%s|%s", category, DEH_String( curr->name ) );
		M_RegisterDebugMenuButton( namebuffer, NULL, callback, curr );
		++curr;
	}
}

//
// M_Init
//
void M_Init (void)
{
	extern int window_width;
	extern int window_height;

	int32_t index;

	episodedetails_t* workingepisode;

	char episodestem[ 128 ];

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

    opldev = M_CheckParm("-opldev") > 0;

	window_width_working = window_width;
	window_height_working = window_height;

	for( index = 0; index < window_sizes_scaled_count; ++index )
	{
		if( window_width == window_sizes_scaled[ index ].width && window_height == window_sizes_scaled[ index ].height )
		{
			window_dimensions_current = window_sizes_scaled[ index ].asstring;
		}
	}

	for( index = 0; index < render_sizes_count; ++index )
	{
		if( render_width == render_sizes[ index ].width && render_height == render_sizes[ index ].height )
		{
			render_dimensions_current = render_sizes[ index ].asstring;
		}
	}


	switch( gamemission )
	{
	case doom:
		switch( gamemode )
		{
		case shareware:
			debugmenu_currgame = &game_doomshareware;
			break;
		case registered:
			debugmenu_currgame = &game_doomregistered;
			break;
		default:
			// Just assume full range for everything else
			debugmenu_currgame = &game_doomultimate;
			break;
		}
		break;
	case doom2:
		debugmenu_currgame = &game_doom2;
		break;
	case pack_tnt:
		debugmenu_currgame = &game_evilution;
		break;
	case pack_plut:
		debugmenu_currgame = &game_plutonia;
		break;
	case pack_chex:
		debugmenu_currgame = &game_chexquest;
		break;
	default:
		// Just assume Doom 2 style for everything else
		debugmenu_currgame = &game_doom2;
		break;
	}

	workingepisode = debugmenu_currgame->episodes;
	while( workingepisode->num != -1 )
	{
		++workingepisode;
	};
	debugmenu_currgameepisodes = workingepisode - debugmenu_currgame->episodes;

	M_RegisterDebugMenuCategory( "Game|New" );

	M_RegisterDebugMenuRadioButton( "Game|New|No skill", NULL, &debugmenu_currgameskill, -1 );
	M_RegisterDebugMenuRadioButton( "Game|New|I'm too young to die", NULL, &debugmenu_currgameskill, 0 );
	M_RegisterDebugMenuRadioButton( "Game|New|Hey, not too rough", NULL, &debugmenu_currgameskill, 1 );
	M_RegisterDebugMenuRadioButton( "Game|New|Hurt me plenty", NULL, &debugmenu_currgameskill, 2 );
	M_RegisterDebugMenuRadioButton( "Game|New|Ultra violence", NULL, &debugmenu_currgameskill, 3 );
	M_RegisterDebugMenuRadioButton( "Game|New|Nightmare", NULL, &debugmenu_currgameskill, 4 );
	M_RegisterDebugMenuSeparator( "Game|New" );
	M_RegisterDebugMenuCheckboxFlag( "Game|New|Pistol Starts", NULL, &debugmenu_currgameflags, GF_PistolStarts );
	M_RegisterDebugMenuCheckboxFlag( "Game|New|Loop one level", NULL, &debugmenu_currgameflags, GF_LoopOneLevel );
	M_RegisterDebugMenuSeparator( "Game|New" );

	if( debugmenu_currgameepisodes == 1 )
	{
		M_DebugMenuRegisterEpisodeMaps( "Game|New", debugmenu_currgame->episodes, &M_DebugMenuNewGame );
	}
	else
	{
		workingepisode = debugmenu_currgame->episodes;
		while( workingepisode->num != -1 )
		{
			memset( episodestem, 0, sizeof( episodestem ) );
			sprintf( episodestem, "Game|New|%s", workingepisode->name );
			M_DebugMenuRegisterEpisodeMaps( episodestem, workingepisode, &M_DebugMenuNewGame );

			++workingepisode;
		};
	}

	M_RegisterDebugMenuWindow( "Game|Options", "Game Options", 500, 500, &debugwindow_options, &M_DebugMenuOptionsWindow );
}

