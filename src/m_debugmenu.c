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
#include "m_config.h"

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

// Filled out on init
static ImVec4 theme_original[ ImGuiCol_COUNT ];

// Colour scheme from https://github.com/ocornut/imgui/issues/707#issuecomment-508691523
static ImVec4 theme_funkygreenhighlight[ ImGuiCol_COUNT ] =
{
	{ 0.956863, 0.945098, 0.870588, 0.8 },			// ImGuiCol_Text,
	{ 0.356863, 0.345098, 0.270588, 0.8 },			// ImGuiCol_TextDisabled,
	{ 0.145098, 0.129412, 0.192157, 0.8 },			// ImGuiCol_WindowBg,
	{ 0, 0, 0, 0.2 },								// ImGuiCol_ChildBg,
	{ 0.145098, 0.129412, 0.192157, 0.901961 },		// ImGuiCol_PopupBg,
	{ 0.545098, 0.529412, 0.592157, 0.8 },			// ImGuiCol_Border,
	{ 0, 0, 0, 0.8 },								// ImGuiCol_BorderShadow,
	{ 0.47451, 0.137255, 0.34902, 0.4 },			// ImGuiCol_FrameBg,
	{ 0.67451, 0.337255, 0.54902, 0.4 },			// ImGuiCol_FrameBgHovered,
	{ 0.57451, 0.237255, 0.44902, 1 },				// ImGuiCol_FrameBgActive,
	{ 0.145098, 0.129412, 0.192157, 0.8 },			// ImGuiCol_TitleBg,
	{ 0.245098, 0.229412, 0.292157, 1 },			// ImGuiCol_TitleBgActive,
	{ 0, 0, 0, 0.8 },								// ImGuiCol_TitleBgCollapsed,
	{ 0, 0, 0, 0.8 },								// ImGuiCol_MenuBarBg,
	{ 0.545098, 0.529412, 0.592157, 0.501961 },		// ImGuiCol_ScrollbarBg,
	{ 0.445098, 0.429412, 0.492157, 0.8 },			// ImGuiCol_ScrollbarGrab,
	{ 0.645098, 0.629412, 0.692157, 0.8 },			// ImGuiCol_ScrollbarGrabHovered,
	{ 0.545098, 0.529412, 0.592157, 1 },			// ImGuiCol_ScrollbarGrabActive,
	{ 0.780392, 0.937255, 0, 0.8 },					// ImGuiCol_CheckMark,
	{ 0.780392, 0.937255, 0, 0.8 },					// ImGuiCol_SliderGrab,
	{ 0.880392, 1.03725, 0.1, 1 },					// ImGuiCol_SliderGrabActive,
	{ 0.854902, 0.0666667, 0.368627, 0.8 },			// ImGuiCol_Button,
	{ 1.0549, 0.266667, 0.568627, 0.8 },			// ImGuiCol_ButtonHovered,
	{ 0.954902, 0.166667, 0.468627, 1 },			// ImGuiCol_ButtonActive,
	{ 0.47451, 0.137255, 0.34902, 0.8 },			// ImGuiCol_Header,
	{ 0.67451, 0.337255, 0.54902, 0.8 },			// ImGuiCol_HeaderHovered,
	{ 0.57451, 0.237255, 0.44902, 1 },				// ImGuiCol_HeaderActive,
	{ 0.545098, 0.529412, 0.592157, 0.8 },			// ImGuiCol_Separator,
	{ 0.745098, 0.729412, 0.792157, 0.8 },			// ImGuiCol_SeparatorHovered,
	{ 0.645098, 0.629412, 0.692157, 1 },			// ImGuiCol_SeparatorActive,
	{ 0.854902, 0.0666667, 0.368627, 0.2 },			// ImGuiCol_ResizeGrip,
	{ 1.0549, 0.266667, 0.568627, 0.2 },			// ImGuiCol_ResizeGripHovered,
	{ 0.954902, 0.166667, 0.468627, 1 },			// ImGuiCol_ResizeGripActive,
	{ 0.854902, 0.0666667, 0.368627, 0.6 },			// ImGuiCol_Tab,
	{ 1.0549, 0.266667, 0.568627, 0.6 },			// ImGuiCol_TabHovered,
	{ 0.954902, 0.166667, 0.468627, 1 },			// ImGuiCol_TabActive,
	{ 0.854902, 0.0666667, 0.368627, 0.6 },			// ImGuiCol_TabUnfocused,
	{ 0.954902, 0.166667, 0.468627, 1 },			// ImGuiCol_TabUnfocusedActive,
	{ 0.780392, 0.937255, 0, 0.8 },					// ImGuiCol_PlotLines,
	{ 0.980392, 1.13725, 0.2, 0.8 },				// ImGuiCol_PlotLinesHovered,
	{ 0.780392, 0.937255, 0, 0.8 },					// ImGuiCol_PlotHistogram,
	{ 0.980392, 1.13725, 0.2, 0.8 },				// ImGuiCol_PlotHistogramHovered,
	{ 0.780392, 0.937255, 0, 0.4 },					// ImGuiCol_TextSelectedBg,
	{ 0.780392, 0.937255, 0, 0.8 },					// ImGuiCol_DragDropTarget,
	{ 1, 1, 1, 0.8 },								// ImGuiCol_NavHighlight,
	{ 1, 1, 1, 0.8 },								// ImGuiCol_NavWindowingHighlight,
	{ 1, 1, 1, 0.2 },								// ImGuiCol_NavWindowingDimBg,
	{ 0, 0, 0, 0.6 },								// ImGuiCol_ModalWindowDimBg,
};

static ImVec4 theme_purpleyellowthing[ ImGuiCol_COUNT ] =
{
	{ 0.862745, 0.882353, 0.870588, 0.8 },			// ImGuiCol_Text,
	{ 0.262745, 0.282353, 0.270588, 0.8 },			// ImGuiCol_TextDisabled,
	{ 0.121569, 0.141176, 0.129412, 0.8 },			// ImGuiCol_WindowBg,
	{ 0, 0, 0, 0.2 },								// ImGuiCol_ChildBg,
	{ 0.121569, 0.141176, 0.129412, 0.901961 },		// ImGuiCol_PopupBg,
	{ 0.521569, 0.541176, 0.529412, 0.8 },			// ImGuiCol_Border,
	{ 0, 0, 0, 0.8 },								// ImGuiCol_BorderShadow,
	{ 0.552941, 0.52549, 0.788235, 0.4 },			// ImGuiCol_FrameBg,
	{ 0.752941, 0.72549, 0.988235, 0.4 },			// ImGuiCol_FrameBgHovered,
	{ 0.652941, 0.62549, 0.888235, 1 },				// ImGuiCol_FrameBgActive,
	{ 0.121569, 0.141176, 0.129412, 0.8 },			// ImGuiCol_TitleBg,
	{ 0.221569, 0.241176, 0.229412, 1 },			// ImGuiCol_TitleBgActive,
	{ 0, 0, 0, 0.8 },								// ImGuiCol_TitleBgCollapsed,
	{ 0, 0, 0, 0.8 },								// ImGuiCol_MenuBarBg,
	{ 0.521569, 0.541176, 0.529412, 0.501961 },		// ImGuiCol_ScrollbarBg,
	{ 0.421569, 0.441176, 0.429412, 0.8 },			// ImGuiCol_ScrollbarGrab,
	{ 0.621569, 0.641176, 0.629412, 0.8 },			// ImGuiCol_ScrollbarGrabHovered,
	{ 0.521569, 0.541176, 0.529412, 1 },			// ImGuiCol_ScrollbarGrabActive,
	{ 0.92549, 0.643137, 0, 0.8 },					// ImGuiCol_CheckMark,
	{ 0.92549, 0.643137, 0, 0.8 },					// ImGuiCol_SliderGrab,
	{ 1.02549, 0.743137, 0.1, 1 },					// ImGuiCol_SliderGrabActive,
	{ 0.447059, 0.352941, 0.756863, 0.8 },			// ImGuiCol_Button,
	{ 0.647059, 0.552941, 0.956863, 0.8 },			// ImGuiCol_ButtonHovered,
	{ 0.547059, 0.452941, 0.856863, 1 },			// ImGuiCol_ButtonActive,
	{ 0.552941, 0.52549, 0.788235, 0.8 },			// ImGuiCol_Header,
	{ 0.752941, 0.72549, 0.988235, 0.8 },			// ImGuiCol_HeaderHovered,
	{ 0.652941, 0.62549, 0.888235, 1 },				// ImGuiCol_HeaderActive,
	{ 0.521569, 0.541176, 0.529412, 0.8 },			// ImGuiCol_Separator,
	{ 0.721569, 0.741176, 0.729412, 0.8 },			// ImGuiCol_SeparatorHovered,
	{ 0.621569, 0.641177, 0.629412, 1 },			// ImGuiCol_SeparatorActive,
	{ 0.447059, 0.352941, 0.756863, 0.2 },			// ImGuiCol_ResizeGrip,
	{ 0.647059, 0.552941, 0.956863, 0.2 },			// ImGuiCol_ResizeGripHovered,
	{ 0.547059, 0.452941, 0.856863, 1 },			// ImGuiCol_ResizeGripActive,
	{ 0.447059, 0.352941, 0.756863, 0.6 },			// ImGuiCol_Tab,
	{ 0.647059, 0.552941, 0.956863, 0.6 },			// ImGuiCol_TabHovered,
	{ 0.547059, 0.452941, 0.856863, 1 },			// ImGuiCol_TabActive,
	{ 0.447059, 0.352941, 0.756863, 0.6 },			// ImGuiCol_TabUnfocused,
	{ 0.547059, 0.452941, 0.856863, 1 },			// ImGuiCol_TabUnfocusedActive,
	{ 0.92549, 0.643137, 0, 0.8 },					// ImGuiCol_PlotLines,
	{ 1.12549, 0.843137, 0.2, 0.8 },				// ImGuiCol_PlotLinesHovered,
	{ 0.92549, 0.643137, 0, 0.8 },					// ImGuiCol_PlotHistogram,
	{ 1.12549, 0.843137, 0.2, 0.8 },				// ImGuiCol_PlotHistogramHovered,
	{ 0.92549, 0.643137, 0, 0.4 },					// ImGuiCol_TextSelectedBg,
	{ 0.92549, 0.643137, 0, 0.8 },					// ImGuiCol_DragDropTarget,
	{ 1, 1, 1, 0.8 },								// ImGuiCol_NavHighlight,
	{ 1, 1, 1, 0.8 },								// ImGuiCol_NavWindowingHighlight,
	{ 1, 1, 1, 0.2 },								// ImGuiCol_NavWindowingDimBg,
	{ 0, 0, 0, 0.6 },								// ImGuiCol_ModalWindowDimBg,
};

// Also generated with algorithm in above github
// Color palette at https://coolors.co/1c7c54-73e2a7-def4c6-1b512d-b1cf5f
static ImVec4 theme_greenallthewaydown[ ImGuiCol_COUNT ] =
{
	{ 0.45098, 0.886275, 0.654902, 0.8 },			// ImGuiCol_Text,
	{ 0, 0.286274, 0.054902, 0.8 },					// ImGuiCol_TextDisabled,
	{ 0.109804, 0.486275, 0.329412, 0.8 },			// ImGuiCol_WindowBg,
	{ 0, 0, 0, 0.2 },								// ImGuiCol_ChildBg,
	{ 0.109804, 0.486275, 0.329412, 0.901961 },		// ImGuiCol_PopupBg,
	{ 0.509804, 0.886275, 0.729412, 0.8 },			// ImGuiCol_Border,
	{ 0, 0, 0, 0.8 },								// ImGuiCol_BorderShadow,
	{ 0.105882, 0.317647, 0.176471, 0.4 },			// ImGuiCol_FrameBg,
	{ 0.305882, 0.517647, 0.376471, 0.4 },			// ImGuiCol_FrameBgHovered,
	{ 0.205882, 0.417647, 0.276471, 1 },			// ImGuiCol_FrameBgActive,
	{ 0.109804, 0.486275, 0.329412, 0.8 },			// ImGuiCol_TitleBg,
	{ 0.209804, 0.586275, 0.429412, 1 },			// ImGuiCol_TitleBgActive,
	{ 0, 0.286274, 0.129412, 0.8 },					// ImGuiCol_TitleBgCollapsed,
	{ 0, 0.286274, 0.129412, 0.8 },					// ImGuiCol_MenuBarBg,
	{ 0.509804, 0.886275, 0.729412, 0.501961 },		// ImGuiCol_ScrollbarBg,
	{ 0.409804, 0.786275, 0.629412, 0.8 },			// ImGuiCol_ScrollbarGrab,
	{ 0.609804, 0.986275, 0.829412, 0.8 },			// ImGuiCol_ScrollbarGrabHovered,
	{ 0.509804, 0.886275, 0.729412, 1 },			// ImGuiCol_ScrollbarGrabActive,
	{ 0.694118, 0.811765, 0.372549, 0.8 },			// ImGuiCol_CheckMark,
	{ 0.694118, 0.811765, 0.372549, 0.8 },			// ImGuiCol_SliderGrab,
	{ 0.794118, 0.911765, 0.472549, 1 },			// ImGuiCol_SliderGrabActive,
	{ 0.870588, 0.956863, 0.776471, 0.8 },			// ImGuiCol_Button,
	{ 1.07059, 1.15686, 0.976471, 0.8 },			// ImGuiCol_ButtonHovered,
	{ 0.970588, 1.05686, 0.876471, 1 },				// ImGuiCol_ButtonActive,
	{ 0.105882, 0.317647, 0.176471, 0.8 },			// ImGuiCol_Header,
	{ 0.305882, 0.517647, 0.376471, 0.8 },			// ImGuiCol_HeaderHovered,
	{ 0.205882, 0.417647, 0.276471, 1 },			// ImGuiCol_HeaderActive,
	{ 0.509804, 0.886275, 0.729412, 0.8 },			// ImGuiCol_Separator,
	{ 0.709804, 1.08627, 0.929412, 0.8 },			// ImGuiCol_SeparatorHovered,
	{ 0.609804, 0.986275, 0.829412, 1 },			// ImGuiCol_SeparatorActive,
	{ 0.870588, 0.956863, 0.776471, 0.2 },			// ImGuiCol_ResizeGrip,
	{ 1.07059, 1.15686, 0.976471, 0.2 },			// ImGuiCol_ResizeGripHovered,
	{ 0.970588, 1.05686, 0.876471, 1 },				// ImGuiCol_ResizeGripActive,
	{ 0.870588, 0.956863, 0.776471, 0.6 },			// ImGuiCol_Tab,
	{ 1.07059, 1.15686, 0.976471, 0.6 },			// ImGuiCol_TabHovered,
	{ 0.970588, 1.05686, 0.876471, 1 },				// ImGuiCol_TabActive,
	{ 0.870588, 0.956863, 0.776471, 0.6 },			// ImGuiCol_TabUnfocused,
	{ 0.970588, 1.05686, 0.876471, 1 },				// ImGuiCol_TabUnfocusedActive,
	{ 0.694118, 0.811765, 0.372549, 0.8 },			// ImGuiCol_PlotLines,
	{ 0.894118, 1.01176, 0.572549, 0.8 },			// ImGuiCol_PlotLinesHovered,
	{ 0.694118, 0.811765, 0.372549, 0.8 },			// ImGuiCol_PlotHistogram,
	{ 0.894118, 1.01176, 0.572549, 0.8 },			// ImGuiCol_PlotHistogramHovered,
	{ 0.694118, 0.811765, 0.372549, 0.4 },			// ImGuiCol_TextSelectedBg,
	{ 0.694118, 0.811765, 0.372549, 0.8 },			// ImGuiCol_DragDropTarget,
	{ 1, 1, 1, 0.8 },								// ImGuiCol_NavHighlight,
	{ 1, 1, 1, 0.8 },								// ImGuiCol_NavWindowingHighlight,
	{ 1, 1, 1, 0.2 },								// ImGuiCol_NavWindowingDimBg,
	{ 0, 0, 0, 0.6 },								// ImGuiCol_ModalWindowDimBg,
};

// https://github.com/ocornut/imgui/issues/707#issuecomment-576867100
static ImVec4 theme_valveclassic[ ImGuiCol_COUNT ] =
{
	{ 1.00f, 1.00f, 1.00f, 1.00f },					// ImGuiCol_Text,
	{ 0.50f, 0.50f, 0.50f, 1.00f },					// ImGuiCol_TextDisabled,
	{ 0.29f, 0.34f, 0.26f, 1.00f },					// ImGuiCol_WindowBg,
	{ 0.29f, 0.34f, 0.26f, 1.00f },					// ImGuiCol_ChildBg,
	{ 0.24f, 0.27f, 0.20f, 1.00f },					// ImGuiCol_PopupBg,
	{ 0.54f, 0.57f, 0.51f, 0.50f },					// ImGuiCol_Border,
	{ 0.14f, 0.16f, 0.11f, 0.52f },					// ImGuiCol_BorderShadow,
	{ 0.24f, 0.27f, 0.20f, 1.00f },					// ImGuiCol_FrameBg,
	{ 0.27f, 0.30f, 0.23f, 1.00f },					// ImGuiCol_FrameBgHovered,
	{ 0.30f, 0.34f, 0.26f, 1.00f },					// ImGuiCol_FrameBgActive,
	{ 0.24f, 0.27f, 0.20f, 1.00f },					// ImGuiCol_TitleBg,
	{ 0.29f, 0.34f, 0.26f, 1.00f },					// ImGuiCol_TitleBgActive,
	{ 0.00f, 0.00f, 0.00f, 0.51f },					// ImGuiCol_TitleBgCollapsed,
	{ 0.24f, 0.27f, 0.20f, 1.00f },					// ImGuiCol_MenuBarBg,
	{ 0.35f, 0.42f, 0.31f, 1.00f },					// ImGuiCol_ScrollbarBg,
	{ 0.28f, 0.32f, 0.24f, 1.00f },					// ImGuiCol_ScrollbarGrab,
	{ 0.25f, 0.30f, 0.22f, 1.00f },					// ImGuiCol_ScrollbarGrabHovered,
	{ 0.23f, 0.27f, 0.21f, 1.00f },					// ImGuiCol_ScrollbarGrabActive,
	{ 0.59f, 0.54f, 0.18f, 1.00f },					// ImGuiCol_CheckMark,
	{ 0.35f, 0.42f, 0.31f, 1.00f },					// ImGuiCol_SliderGrab,
	{ 0.54f, 0.57f, 0.51f, 0.50f },					// ImGuiCol_SliderGrabActive,
	{ 0.29f, 0.34f, 0.26f, 0.40f },					// ImGuiCol_Button,
	{ 0.35f, 0.42f, 0.31f, 1.00f },					// ImGuiCol_ButtonHovered,
	{ 0.54f, 0.57f, 0.51f, 0.50f },					// ImGuiCol_ButtonActive,
	{ 0.35f, 0.42f, 0.31f, 1.00f },					// ImGuiCol_Header,
	{ 0.35f, 0.42f, 0.31f, 0.6f },					// ImGuiCol_HeaderHovered,
	{ 0.54f, 0.57f, 0.51f, 0.50f },					// ImGuiCol_HeaderActive,
	{ 0.14f, 0.16f, 0.11f, 1.00f },					// ImGuiCol_Separator,
	{ 0.54f, 0.57f, 0.51f, 1.00f },					// ImGuiCol_SeparatorHovered,
	{ 0.59f, 0.54f, 0.18f, 1.00f },					// ImGuiCol_SeparatorActive,
	{ 0.19f, 0.23f, 0.18f, 0.00f },					// ImGuiCol_ResizeGrip,
	{ 0.54f, 0.57f, 0.51f, 1.00f },					// ImGuiCol_ResizeGripHovered,
	{ 0.59f, 0.54f, 0.18f, 1.00f },					// ImGuiCol_ResizeGripActive,
	{ 0.35f, 0.42f, 0.31f, 1.00f },					// ImGuiCol_Tab,
	{ 0.54f, 0.57f, 0.51f, 0.78f },					// ImGuiCol_TabHovered,
	{ 0.59f, 0.54f, 0.18f, 1.00f },					// ImGuiCol_TabActive,
	{ 0.24f, 0.27f, 0.20f, 1.00f },					// ImGuiCol_TabUnfocused,
	{ 0.35f, 0.42f, 0.31f, 1.00f },					// ImGuiCol_TabUnfocusedActive,
	{ 0.61f, 0.61f, 0.61f, 1.00f },					// ImGuiCol_PlotLines,
	{ 0.59f, 0.54f, 0.18f, 1.00f },					// ImGuiCol_PlotLinesHovered,
	{ 1.00f, 0.78f, 0.28f, 1.00f },					// ImGuiCol_PlotHistogram,
	{ 1.00f, 0.60f, 0.00f, 1.00f },					// ImGuiCol_PlotHistogramHovered,
	{ 0.59f, 0.54f, 0.18f, 1.00f },					// ImGuiCol_TextSelectedBg,
	{ 0.73f, 0.67f, 0.24f, 1.00f },					// ImGuiCol_DragDropTarget,
	{ 0.59f, 0.54f, 0.18f, 1.00f },					// ImGuiCol_NavHighlight,
	{ 1.00f, 1.00f, 1.00f, 0.70f },					// ImGuiCol_NavWindowingHighlight
	{ 0.80f, 0.80f, 0.80f, 0.20f },					// ImGuiCol_NavWindowingDimBg,
	{ 0.80f, 0.80f, 0.80f, 0.35f },					// ImGuiCol_ModalWindowDimBg,
};

typedef struct themedata_s
{
	const char*		name;
	ImVec4*			colors;
} themedata_t;

static themedata_t themes[] =
{
	{ "OG ImGui",					theme_original },
	{ "Funky Green Highlight",		theme_funkygreenhighlight },
	{ "Purpley-yellowey Thing",		theme_purpleyellowthing },
	{ "Green All The Way Down",		theme_greenallthewaydown },
	{ "Valve Classic",				theme_valveclassic },
	{ NULL, NULL }
};

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

ImGuiContext*			imgui_context;
static int32_t			debugmenu_theme = 1;

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
	char themefullpath[ MAXNAMELENGTH + 1 ];
	themedata_t* thistheme;

	imgui_context = igCreateContext( NULL );
	memcpy( theme_original, igGetStyle()->Colors, sizeof( theme_original ) );

	memset( entries, 0, sizeof( entries ) );

	nextentry = entries;
	rootentry = nextentry++;

	sprintf( rootentry->name, "Rum and Raisin Doom Debug menu" );
	rootentry->type = MET_Category;
	rootentry->freechild = rootentry->children;
	rootentry->enabled = true;
	rootentry->comparison = -1;

	// Core layout. Add top level categories here
	{
		M_RegisterDebugMenuButton( "Close debug", &M_OnDebugMenuCloseButton );
		M_FindDebugMenuCategory( "Core" );
		M_FindDebugMenuCategory( "Render" );
		M_FindDebugMenuCategory( "Map" );
	}

	thistheme = themes;
	while( thistheme->name != NULL && thistheme->colors != NULL )
	{
		memset( themefullpath, 0, sizeof( themefullpath ) );
		sprintf( themefullpath, "Core|Theme|%s", thistheme->name );
		M_RegisterDebugMenuRadioButton( themefullpath, &debugmenu_theme, (int32_t)( thistheme - themes ) );
		++thistheme;
	}

	M_RegisterDebugMenuWindow( "Core|About...", &aboutwindow_open, &M_AboutWindow );
	M_RegisterDebugMenuSeparator( "Core" );
	M_RegisterDebugMenuButton( "Core|Quit " PACKAGE_NAME, &M_OnDebugMenuCoreQuit );

}

void M_BindDebugMenuVariables( void )
{
	M_BindIntVariable("debugmenu_theme",	&debugmenu_theme);
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
	igCheckbox( cat->name, cat->data );
}

static void M_RenderRadioButtonItem( menuentry_t* cat )
{
	igRadioButtonIntPtr( cat->name, (int32_t*)cat->data, cat->comparison );
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

	memcpy( igGetStyle()->Colors, themes[ debugmenu_theme ].colors, sizeof( igGetStyle()->Colors ) );

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
	menuentry_t* category = M_FindDebugMenuCategoryFromFullPath( full_path );
	menuentry_t* currentry = nextentry++;
	const char* actualname = M_GetActualName( full_path );

	sprintf( currentry->name, actualname );
	currentry->type = MET_Checkbox;
	currentry->freechild = currentry->children;
	currentry->data = value;
	currentry->enabled = true;
	currentry->comparison = -1;
	currentry->parent = category;

	*category->freechild = currentry;
	category->freechild++;
}

void M_RegisterDebugMenuRadioButton( const char* full_path, int32_t* value, int32_t selectedval )
{
	menuentry_t* category = M_FindDebugMenuCategoryFromFullPath( full_path );
	menuentry_t* currentry = nextentry++;
	const char* actualname = M_GetActualName( full_path );

	sprintf( currentry->name, actualname );
	currentry->type = MET_RadioButton;
	currentry->freechild = currentry->children;
	currentry->data = value;
	currentry->enabled = true;
	currentry->comparison = selectedval;
	currentry->parent = category;

	*category->freechild = currentry;
	category->freechild++;
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