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

extern "C"
{
	#include "m_dashboard.h"
	#include "i_system.h"
	#include "m_config.h"

	#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
	#include "cimgui.h"

	#include <stdio.h>
	#include <string.h>
}

typedef enum menuentrytype_e
{
	MET_Invalid,

	MET_Window,
	MET_Category,
	MET_Button,
	MET_Checkbox,
	MET_CheckboxFlag,
	MET_RadioButton,

	MET_Separator,

	MET_Max,
} menuentrytype_t;

typedef struct menuentry_s menuentry_t;

#define MAXCHILDREN		64
#define MAXENTRIES		256
#define MAXNAMELENGTH	127

// Filled out on init
static ImVec4 theme_original[ ImGuiCol_COUNT ];
static ImVec4 theme_originaldark[ ImGuiCol_COUNT ];
static ImVec4 theme_originallight[ ImGuiCol_COUNT ];


// Colour scheme from https://github.com/ocornut/imgui/issues/707#issuecomment-508691523
static ImVec4 theme_funkygreenhighlight[ ImGuiCol_COUNT ] =
{
	{ 0.956863f, 0.945098f, 0.870588f, 0.8f },			// ImGuiCol_Text,
	{ 0.356863f, 0.345098f, 0.270588f, 0.8f },			// ImGuiCol_TextDisabled,
	{ 0.145098f, 0.129412f, 0.192157f, 0.8f },			// ImGuiCol_WindowBg,
	{ 0.f, 0.f, 0.f, 0.2f },								// ImGuiCol_ChildBg,
	{ 0.145098f, 0.129412f, 0.192157f, 0.901961f },		// ImGuiCol_PopupBg,
	{ 0.545098f, 0.529412f, 0.592157f, 0.8f },			// ImGuiCol_Border,
	{ 0.f, 0.f, 0.f, 0.8f },								// ImGuiCol_BorderShadow,
	{ 0.47451f, 0.137255f, 0.34902f, 0.4f },			// ImGuiCol_FrameBg,
	{ 0.67451f, 0.337255f, 0.54902f, 0.4f },			// ImGuiCol_FrameBgHovered,
	{ 0.57451f, 0.237255f, 0.44902f, 1.f },				// ImGuiCol_FrameBgActive,
	{ 0.145098f, 0.129412f, 0.192157f, 0.8f },			// ImGuiCol_TitleBg,
	{ 0.245098f, 0.229412f, 0.292157f, 1.f },			// ImGuiCol_TitleBgActive,
	{ 0.f, 0.f, 0.f, 0.8f },								// ImGuiCol_TitleBgCollapsed,
	{ 0.f, 0.f, 0.f, 0.8f },								// ImGuiCol_MenuBarBg,
	{ 0.545098f, 0.529412f, 0.592157f, 0.501961f },		// ImGuiCol_ScrollbarBg,
	{ 0.445098f, 0.429412f, 0.492157f, 0.8f },			// ImGuiCol_ScrollbarGrab,
	{ 0.645098f, 0.629412f, 0.692157f, 0.8f },			// ImGuiCol_ScrollbarGrabHovered,
	{ 0.545098f, 0.529412f, 0.592157f, 1.f },			// ImGuiCol_ScrollbarGrabActive,
	{ 0.780392f, 0.937255f, 0.f, 0.8f },					// ImGuiCol_CheckMark,
	{ 0.780392f, 0.937255f, 0.f, 0.8f },					// ImGuiCol_SliderGrab,
	{ 0.880392f, 1.03725f, 0.1f, 1.f },					// ImGuiCol_SliderGrabActive,
	{ 0.854902f, 0.0666667f, 0.368627f, 0.8f },			// ImGuiCol_Button,
	{ 1.0549f, 0.266667f, 0.568627f, 0.8f },			// ImGuiCol_ButtonHovered,
	{ 0.954902f, 0.166667f, 0.468627f, 1.f },			// ImGuiCol_ButtonActive,
	{ 0.47451f, 0.137255f, 0.34902f, 0.8f },			// ImGuiCol_Header,
	{ 0.67451f, 0.337255f, 0.54902f, 0.8f },			// ImGuiCol_HeaderHovered,
	{ 0.57451f, 0.237255f, 0.44902f, 1.f },				// ImGuiCol_HeaderActive,
	{ 0.545098f, 0.529412f, 0.592157f, 0.8f },			// ImGuiCol_Separator,
	{ 0.745098f, 0.729412f, 0.792157f, 0.8f },			// ImGuiCol_SeparatorHovered,
	{ 0.645098f, 0.629412f, 0.692157f, 1.f },			// ImGuiCol_SeparatorActive,
	{ 0.854902f, 0.0666667f, 0.368627f, 0.2f },			// ImGuiCol_ResizeGrip,
	{ 1.0549f, 0.266667f, 0.568627f, 0.2f },			// ImGuiCol_ResizeGripHovered,
	{ 0.954902f, 0.166667f, 0.468627f, 1.f },			// ImGuiCol_ResizeGripActive,
	{ 0.854902f, 0.0666667f, 0.368627f, 0.6f },			// ImGuiCol_Tab,
	{ 1.0549f, 0.266667f, 0.568627f, 0.6f },			// ImGuiCol_TabHovered,
	{ 0.954902f, 0.166667f, 0.468627f, 1.f },			// ImGuiCol_TabActive,
	{ 0.854902f, 0.0666667f, 0.368627f, 0.6f },			// ImGuiCol_TabUnfocused,
	{ 0.954902f, 0.166667f, 0.468627f, 1.f },			// ImGuiCol_TabUnfocusedActive,
	{ 0.780392f, 0.937255f, 0.f, 0.8f },					// ImGuiCol_PlotLines,
	{ 0.980392f, 1.13725f, 0.2f, 0.8f },				// ImGuiCol_PlotLinesHovered,
	{ 0.780392f, 0.937255f, 0.f, 0.8f },					// ImGuiCol_PlotHistogram,
	{ 0.980392f, 1.13725f, 0.2f, 0.8f },				// ImGuiCol_PlotHistogramHovered,
	{ 0.780392f, 0.937255f, 0.f, 0.4f },					// ImGuiCol_TextSelectedBg,
	{ 0.780392f, 0.937255f, 0.f, 0.8f },					// ImGuiCol_DragDropTarget,
	{ 1.f, 1.f, 1.f, 0.8f },								// ImGuiCol_NavHighlight,
	{ 1.f, 1.f, 1.f, 0.8f },								// ImGuiCol_NavWindowingHighlight,
	{ 1.f, 1.f, 1.f, 0.2f },								// ImGuiCol_NavWindowingDimBg,
	{ 0.f, 0.f, 0.f, 0.6f },								// ImGuiCol_ModalWindowDimBg,
};

static ImVec4 theme_purpleyellowthing[ ImGuiCol_COUNT ] =
{
	{ 0.862745f, 0.882353f, 0.870588f, 0.8f },			// ImGuiCol_Text,
	{ 0.262745f, 0.282353f, 0.270588f, 0.8f },			// ImGuiCol_TextDisabled,
	{ 0.121569f, 0.141176f, 0.129412f, 0.8f },			// ImGuiCol_WindowBg,
	{ 0.f, 0.f, 0.f, 0.2f },								// ImGuiCol_ChildBg,
	{ 0.121569f, 0.141176f, 0.129412f, 0.901961f },		// ImGuiCol_PopupBg,
	{ 0.521569f, 0.541176f, 0.529412f, 0.8f },			// ImGuiCol_Border,
	{ 0.f, 0.f, 0.f, 0.8f },								// ImGuiCol_BorderShadow,
	{ 0.552941f, 0.52549f, 0.788235f, 0.4f },			// ImGuiCol_FrameBg,
	{ 0.752941f, 0.72549f, 0.988235f, 0.4f },			// ImGuiCol_FrameBgHovered,
	{ 0.652941f, 0.62549f, 0.888235f, 1.f },				// ImGuiCol_FrameBgActive,
	{ 0.121569f, 0.141176f, 0.129412f, 0.8f },			// ImGuiCol_TitleBg,
	{ 0.221569f, 0.241176f, 0.229412f, 1.f },			// ImGuiCol_TitleBgActive,
	{ 0.f, 0.f, 0.f, 0.8f },								// ImGuiCol_TitleBgCollapsed,
	{ 0.f, 0.f, 0.f, 0.8f },								// ImGuiCol_MenuBarBg,
	{ 0.521569f, 0.541176f, 0.529412f, 0.501961f },		// ImGuiCol_ScrollbarBg,
	{ 0.421569f, 0.441176f, 0.429412f, 0.8f },			// ImGuiCol_ScrollbarGrab,
	{ 0.621569f, 0.641176f, 0.629412f, 0.8f },			// ImGuiCol_ScrollbarGrabHovered,
	{ 0.521569f, 0.541176f, 0.529412f, 1.f },			// ImGuiCol_ScrollbarGrabActive,
	{ 0.92549f, 0.643137f, 0.f, 0.8f },					// ImGuiCol_CheckMark,
	{ 0.92549f, 0.643137f, 0.f, 0.8f },					// ImGuiCol_SliderGrab,
	{ 1.02549f, 0.743137f, 0.1f, 1.f },					// ImGuiCol_SliderGrabActive,
	{ 0.447059f, 0.352941f, 0.756863f, 0.8f },			// ImGuiCol_Button,
	{ 0.647059f, 0.552941f, 0.956863f, 0.8f },			// ImGuiCol_ButtonHovered,
	{ 0.547059f, 0.452941f, 0.856863f, 1.f },			// ImGuiCol_ButtonActive,
	{ 0.552941f, 0.52549f, 0.788235f, 0.8f },			// ImGuiCol_Header,
	{ 0.752941f, 0.72549f, 0.988235f, 0.8f },			// ImGuiCol_HeaderHovered,
	{ 0.652941f, 0.62549f, 0.888235f, 1.f },				// ImGuiCol_HeaderActive,
	{ 0.521569f, 0.541176f, 0.529412f, 0.8f },			// ImGuiCol_Separator,
	{ 0.721569f, 0.741176f, 0.729412f, 0.8f },			// ImGuiCol_SeparatorHovered,
	{ 0.621569f, 0.641177f, 0.629412f, 1.f },			// ImGuiCol_SeparatorActive,
	{ 0.447059f, 0.352941f, 0.756863f, 0.2f },			// ImGuiCol_ResizeGrip,
	{ 0.647059f, 0.552941f, 0.956863f, 0.2f },			// ImGuiCol_ResizeGripHovered,
	{ 0.547059f, 0.452941f, 0.856863f, 1.f },			// ImGuiCol_ResizeGripActive,
	{ 0.447059f, 0.352941f, 0.756863f, 0.6f },			// ImGuiCol_Tab,
	{ 0.647059f, 0.552941f, 0.956863f, 0.6f },			// ImGuiCol_TabHovered,
	{ 0.547059f, 0.452941f, 0.856863f, 1.f },			// ImGuiCol_TabActive,
	{ 0.447059f, 0.352941f, 0.756863f, 0.6f },			// ImGuiCol_TabUnfocused,
	{ 0.547059f, 0.452941f, 0.856863f, 1.f },			// ImGuiCol_TabUnfocusedActive,
	{ 0.92549f, 0.643137f, 0.f, 0.8f },					// ImGuiCol_PlotLines,
	{ 1.12549f, 0.843137f, 0.2f, 0.8f },				// ImGuiCol_PlotLinesHovered,
	{ 0.92549f, 0.643137f, 0.f, 0.8f },					// ImGuiCol_PlotHistogram,
	{ 1.12549f, 0.843137f, 0.2f, 0.8f },				// ImGuiCol_PlotHistogramHovered,
	{ 0.92549f, 0.643137f, 0.f, 0.4f },					// ImGuiCol_TextSelectedBg,
	{ 0.92549f, 0.643137f, 0.f, 0.8f },					// ImGuiCol_DragDropTarget,
	{ 1.f, 1.f, 1.f, 0.8f },								// ImGuiCol_NavHighlight,
	{ 1.f, 1.f, 1.f, 0.8f },								// ImGuiCol_NavWindowingHighlight,
	{ 1.f, 1.f, 1.f, 0.2f },								// ImGuiCol_NavWindowingDimBg,
	{ 0.f, 0.f, 0.f, 0.6f },								// ImGuiCol_ModalWindowDimBg,
};

// Generated with abovef, but using colours plucked from the Doom palette
static ImVec4 theme_doomedspacemarine[ ImGuiCol_COUNT ] =
{
	{ 0.858824f, 0.858824f, 0.858824f, 0.8f },			// ImGuiCol_Text,
	{ 0.258824f, 0.258824f, 0.258824f, 0.8f },			// ImGuiCol_TextDisabled,
	{ 0.262745f, 0.f, 0.f, 0.8f },						// ImGuiCol_WindowBg,
	{ 0.f, 0.f, 0.f, 0.2f },								// ImGuiCol_ChildBg,
	{ 0.262745f, 0.f, 0.f, 0.901961f },					// ImGuiCol_PopupBg,
	{ 0.662745f, 0.4f, 0.4f, 0.8f },					// ImGuiCol_Border,
	{ 0.f, 0.f, 0.f, 0.8f },								// ImGuiCol_BorderShadow,
	{ 0.309804f, 0.309804f, 0.309804f, 0.4f },			// ImGuiCol_FrameBg,
	{ 0.509804f, 0.509804f, 0.509804f, 0.4f },			// ImGuiCol_FrameBgHovered,
	{ 0.409804f, 0.409804f, 0.409804f, 1.f },			// ImGuiCol_FrameBgActive,
	{ 0.262745f, 0.f, 0.f, 0.8f },						// ImGuiCol_TitleBg,
	{ 0.362745f, 0.1f, 0.1f, 1.f },						// ImGuiCol_TitleBgActive,
	{ 0.0627451f, 0.f, 0.f, 0.8f },						// ImGuiCol_TitleBgCollapsed,
	{ 0.0627451f, 0.f, 0.f, 0.8f },						// ImGuiCol_MenuBarBg,
	{ 0.662745f, 0.4f, 0.4f, 0.501961f },				// ImGuiCol_ScrollbarBg,
	{ 0.562745f, 0.3f, 0.3f, 0.8f },					// ImGuiCol_ScrollbarGrab,
	{ 0.762745f, 0.5f, 0.5f, 0.8f },					// ImGuiCol_ScrollbarGrabHovered,
	{ 0.662745f, 0.4f, 0.4f, 1.f },						// ImGuiCol_ScrollbarGrabActive,
	{ 1.f, 1.f, 0.137255f, 0.8f },						// ImGuiCol_CheckMark,
	{ 1.f, 1.f, 0.137255f, 0.8f },						// ImGuiCol_SliderGrab,
	{ 1.1f, 1.1f, 0.237255f, 1.f },						// ImGuiCol_SliderGrabActive,
	{ 0.247059f, 0.513726f, 0.184314f, 0.8f },			// ImGuiCol_Button,
	{ 0.447059f, 0.713726f, 0.384314f, 0.8f },			// ImGuiCol_ButtonHovered,
	{ 0.347059f, 0.613726f, 0.284314f, 1.f },			// ImGuiCol_ButtonActive,
	{ 0.309804f, 0.309804f, 0.309804f, 0.8f },			// ImGuiCol_Header,
	{ 0.509804f, 0.509804f, 0.509804f, 0.8f },			// ImGuiCol_HeaderHovered,
	{ 0.409804f, 0.409804f, 0.409804f, 1.f },			// ImGuiCol_HeaderActive,
	{ 0.662745f, 0.4f, 0.4f, 0.8f },					// ImGuiCol_Separator,
	{ 0.862745f, 0.6f, 0.6f, 0.8f },					// ImGuiCol_SeparatorHovered,
	{ 0.762745f, 0.5f, 0.5f, 1.f },						// ImGuiCol_SeparatorActive,
	{ 0.247059f, 0.513726f, 0.184314f, 0.2f },			// ImGuiCol_ResizeGrip,
	{ 0.447059f, 0.713726f, 0.384314f, 0.2f },			// ImGuiCol_ResizeGripHovered,
	{ 0.347059f, 0.613726f, 0.284314f, 1.f },			// ImGuiCol_ResizeGripActive,
	{ 0.247059f, 0.513726f, 0.184314f, 0.6f },			// ImGuiCol_Tab,
	{ 0.447059f, 0.713726f, 0.384314f, 0.6f },			// ImGuiCol_TabHovered,
	{ 0.347059f, 0.613726f, 0.284314f, 1.f },			// ImGuiCol_TabActive,
	{ 0.247059f, 0.513726f, 0.184314f, 0.6f },			// ImGuiCol_TabUnfocused,
	{ 0.347059f, 0.613726f, 0.284314f, 1.f },			// ImGuiCol_TabUnfocusedActive,
	{ 1.f, 1.f, 0.137255f, 0.8f },						// ImGuiCol_PlotLines,
	{ 1.2f, 1.2f, 0.337255f, 0.8f },					// ImGuiCol_PlotLinesHovered,
	{ 1.f, 1.f, 0.137255f, 0.8f },						// ImGuiCol_PlotHistogram,
	{ 1.2f, 1.2f, 0.337255f, 0.8f },					// ImGuiCol_PlotHistogramHovered,
	{ 1.f, 1.f, 0.137255f, 0.4f },						// ImGuiCol_TextSelectedBg,
	{ 1.f, 1.f, 0.137255f, 0.8f },						// ImGuiCol_DragDropTarget,
	{ 1.f, 1.f, 1.f, 0.8f },								// ImGuiCol_NavHighlight,
	{ 1.f, 1.f, 1.f, 0.8f },								// ImGuiCol_NavWindowingHighlight,
	{ 1.f, 1.f, 1.f, 0.2f },								// ImGuiCol_NavWindowingDimBg,
	{ 0.f, 0.f, 0.f, 0.6f },								// ImGuiCol_ModalWindowDimBg,
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

// https://github.com/ocornut/imgui/issues/707#issuecomment-622934113
static ImVec4 theme_didntaskforthis[ ImGuiCol_COUNT ] =
{
	{ 0.92f, 0.92f, 0.92f, 1.00f },					// ImGuiCol_Text,
	{ 0.44f, 0.44f, 0.44f, 1.00f },					// ImGuiCol_TextDisabled,
	{ 0.06f, 0.06f, 0.06f, 1.00f },					// ImGuiCol_WindowBg,
	{ 0.00f, 0.00f, 0.00f, 0.00f },					// ImGuiCol_ChildBg,
	{ 0.08f, 0.08f, 0.08f, 0.94f },					// ImGuiCol_PopupBg,
	{ 0.51f, 0.36f, 0.15f, 1.00f },					// ImGuiCol_Border,
	{ 0.00f, 0.00f, 0.00f, 0.00f },					// ImGuiCol_BorderShadow,
	{ 0.11f, 0.11f, 0.11f, 1.00f },					// ImGuiCol_FrameBg,
	{ 0.51f, 0.36f, 0.15f, 1.00f },					// ImGuiCol_FrameBgHovered,
	{ 0.78f, 0.55f, 0.21f, 1.00f },					// ImGuiCol_FrameBgActive,
	{ 0.51f, 0.36f, 0.15f, 1.00f },					// ImGuiCol_TitleBg,
	{ 0.91f, 0.64f, 0.13f, 1.00f },					// ImGuiCol_TitleBgActive,
	{ 0.00f, 0.00f, 0.00f, 0.51f },					// ImGuiCol_TitleBgCollapsed,
	{ 0.11f, 0.11f, 0.11f, 1.00f },					// ImGuiCol_MenuBarBg,
	{ 0.06f, 0.06f, 0.06f, 0.53f },					// ImGuiCol_ScrollbarBg,
	{ 0.21f, 0.21f, 0.21f, 1.00f },					// ImGuiCol_ScrollbarGrab,
	{ 0.47f, 0.47f, 0.47f, 1.00f },					// ImGuiCol_ScrollbarGrabHovered,
	{ 0.81f, 0.83f, 0.81f, 1.00f },					// ImGuiCol_ScrollbarGrabActive,
	{ 0.78f, 0.55f, 0.21f, 1.00f },					// ImGuiCol_CheckMark,
	{ 0.91f, 0.64f, 0.13f, 1.00f },					// ImGuiCol_SliderGrab,
	{ 0.91f, 0.64f, 0.13f, 1.00f },					// ImGuiCol_SliderGrabActive,
	{ 0.51f, 0.36f, 0.15f, 1.00f },					// ImGuiCol_Button,
	{ 0.91f, 0.64f, 0.13f, 1.00f },					// ImGuiCol_ButtonHovered,
	{ 0.78f, 0.55f, 0.21f, 1.00f },					// ImGuiCol_ButtonActive,
	{ 0.51f, 0.36f, 0.15f, 1.00f },					// ImGuiCol_Header,
	{ 0.91f, 0.64f, 0.13f, 1.00f },					// ImGuiCol_HeaderHovered,
	{ 0.93f, 0.65f, 0.14f, 1.00f },					// ImGuiCol_HeaderActive,
	{ 0.21f, 0.21f, 0.21f, 1.00f },					// ImGuiCol_Separator,
	{ 0.91f, 0.64f, 0.13f, 1.00f },					// ImGuiCol_SeparatorHovered,
	{ 0.78f, 0.55f, 0.21f, 1.00f },					// ImGuiCol_SeparatorActive,
	{ 0.21f, 0.21f, 0.21f, 1.00f },					// ImGuiCol_ResizeGrip,
	{ 0.91f, 0.64f, 0.13f, 1.00f },					// ImGuiCol_ResizeGripHovered,
	{ 0.78f, 0.55f, 0.21f, 1.00f },					// ImGuiCol_ResizeGripActive,
	{ 0.51f, 0.36f, 0.15f, 1.00f },					// ImGuiCol_Tab,
	{ 0.91f, 0.64f, 0.13f, 1.00f },					// ImGuiCol_TabHovered,
	{ 0.78f, 0.55f, 0.21f, 1.00f },					// ImGuiCol_TabActive,
	{ 0.07f, 0.10f, 0.15f, 0.97f },					// ImGuiCol_TabUnfocused,
	{ 0.14f, 0.26f, 0.42f, 1.00f },					// ImGuiCol_TabUnfocusedActive,
	{ 0.61f, 0.61f, 0.61f, 1.00f },					// ImGuiCol_PlotLines,
	{ 1.00f, 0.43f, 0.35f, 1.00f },					// ImGuiCol_PlotLinesHovered,
	{ 0.90f, 0.70f, 0.00f, 1.00f },					// ImGuiCol_PlotHistogram,
	{ 1.00f, 0.60f, 0.00f, 1.00f },					// ImGuiCol_PlotHistogramHovered,
	{ 0.26f, 0.59f, 0.98f, 0.35f },					// ImGuiCol_TextSelectedBg,
	{ 1.00f, 1.00f, 0.00f, 0.90f },					// ImGuiCol_DragDropTarget,
	{ 0.26f, 0.59f, 0.98f, 1.00f },					// ImGuiCol_NavHighlight,
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
	{ "Doomed Space Marine",		theme_doomedspacemarine },
	{ "OG ImGui",					theme_original },
	{ "OG ImGui Dark",				theme_originaldark },
	{ "OG ImGui Light",				theme_originallight },
	{ "Funky Green Highlight",		theme_funkygreenhighlight },
	{ "Purpley-yellowey Thing",		theme_purpleyellowthing },
	{ "Valve Classic",				theme_valveclassic },
	{ "Didn't Ask For This",		theme_didntaskforthis },
	{ NULL, NULL }
};

typedef struct menuentry_s
{
	char				name[ MAXNAMELENGTH + 1 ]; // +1 for overflow
	char				caption[ MAXNAMELENGTH + 1 ]; // +1 for overflow
	char				tooltip[ MAXNAMELENGTH + 1 ]; // +1 for overflow
	menuentry_t*		parent;
	menuentry_t*		children[ MAXCHILDREN + 1 ]; // +1 for overflow
	menuentry_t**		freechild;
	menufunc_t			callback;
	void*				data;
	ImVec2				dimensions;
	boolean				enabled;
	int32_t				comparison;
	menuentrytype_t		type;
	menuproperties_t	properties;
	
} menuentry_t;

typedef void (*renderchild_t)( menuentry_t* );

static menuentry_t* M_FindDashboardCategory( const char* category_name );
static void M_RenderDashboardChildren( menuentry_t** children );
static void M_ThrowAnErrorBecauseThisIsInvalid( menuentry_t* cat );
static void M_RenderWindowItem( menuentry_t* cat );
static void M_RenderCategoryItem( menuentry_t* cat );
static void M_RenderButtonItem( menuentry_t* cat );
static void M_RenderCheckboxItem( menuentry_t* cat );
static void M_RenderCheckboxFlagItem( menuentry_t* cat );
static void M_RenderRadioButtonItem( menuentry_t* cat );
static void M_RenderSeparator( menuentry_t* cat );

extern "C"
{
	ImGuiContext*			imgui_context;
	boolean					dashboardactive = false;
	int32_t					dashboardremappingkey = Remap_None;
	int32_t					dashboardpausesplaysim = true;
	int32_t					dashboardopensound = 0;
	int32_t					dashboardclosesound = 0;
}

static int32_t			dashboard_theme = 0;

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
	&M_RenderCheckboxFlagItem,
	&M_RenderRadioButtonItem,
	&M_RenderSeparator,
	&M_ThrowAnErrorBecauseThisIsInvalid,
};

static boolean aboutwindow_open = false;
static boolean licenseswindow_open = false;

#define CHECK_ELEMENTS() { if( (nextentry - entries) > MAXENTRIES ) I_Error( "M_Dashboard: More than %d elements total added", MAXENTRIES ); }
#define CHECK_CHILDREN( elem ) { if( (elem->freechild - elem->children) > MAXCHILDREN ) I_Error( "M_Dashboard: Too many child elements added to %s", elem->name ); }

// Blatantly assuming this will get linked in just fine
DOOM_C_API void S_StartSound( void *origin, int sound_id );

static void M_OnDashboardCloseButton( const char* itemname, void* data )
{
	S_StartSound( NULL, dashboardclosesound );
	dashboardactive = false;
}

static void M_OnDashboardCoreQuit( const char* itemname, void* data )
{
	I_Quit();
}

static void M_AboutWindow( const char* itemname, void* data )
{
	igText( EDITION_STRING );
	igText( "\"Haha software render go BRRRRR!\"" );
	igNewLine();
	igText( "A fork of Chocolate Doom focused on speed for modern architectures." );
	igNewLine();
	igText( "Copyright(C) 1993-1996 Id Software, Inc." );
	igText( "Copyright(C) 1993-2008 Raven Software" );
	igText( "Copyright(C) 2005-2014 Simon Howard" );
	igText( "Copyright(C) 2020-2022 Ethan Watson" );
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

typedef struct licenses_s
{
	const char* software;
	const char* license;
} licenses_t;

static licenses_t licences[] =
{
	{ "Dear ImGui",
		"The MIT License (MIT)\n"
		"\n"
		"Copyright (c) 2014-2022 Omar Cornut\n"
		"\n"
		"Permission is hereby granted, free of charge, to any person obtaining a copy\n"
		"of this software and associated documentation files (the \"Software\"), to deal\n"
		"in the Software without restriction, including without limitation the rights\n"
		"to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
		"copies of the Software, and to permit persons to whom the Software is\n"
		"furnished to do so, subject to the following conditions:\n"
		"\n"
		"The above copyright notice and this permission notice shall be included in all\n"
		"copies or substantial portions of the Software.\n"
		"\n"
		"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
		"IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
		"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
		"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
		"LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
		"OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
		"SOFTWARE."
	},
	{ "cimgui",
		"The MIT License (MIT)\n"
		"\n"
		"Copyright (c) 2015 Stephan Dilly\n"
		"\n"
		"Permission is hereby granted, free of charge, to any person obtaining a copy\n"
		"of this software and associated documentation files (the \"Software\"), to deal\n"
		"in the Software without restriction, including without limitation the rights\n"
		"to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
		"copies of the Software, and to permit persons to whom the Software is\n"
		"furnished to do so, subject to the following conditions:\n"
		"\n"
		"The above copyright notice and this permission notice shall be included in all\n"
		"copies or substantial portions of the Software.\n"
		"\n"
		"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
		"IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
		"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
		"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
		"LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
		"OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
		"SOFTWARE."
	},
};

static void M_LicensesWindow( const char* itemname, void* data )
{
	ImVec2 zerosize = { 0, 0 };
	int32_t index;

	igPushIDPtr( &licenseswindow_open );
	for( index = 0; index < arrlen( licences ); ++index )
	{
		igPushIDPtr( licences[ index ].software );
		if( igCollapsingHeaderTreeNodeFlags( licences[ index ].software, ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen ) )
		{
			igText( licences[ index ].license );
		}
		igPopID();
	}
	igPopID();
}

void M_InitDashboard( void )
{
	char themefullpath[ MAXNAMELENGTH + 1 ];
	themedata_t* thistheme;

#if USE_IMGUI
	imgui_context = igCreateContext( NULL );

	igStyleColorsClassic( NULL );
	memcpy( theme_original, igGetStyle()->Colors, sizeof( theme_original ) );
	igStyleColorsDark( NULL );
	memcpy( theme_originaldark, igGetStyle()->Colors, sizeof( theme_originaldark ) );
	igStyleColorsLight( NULL );
	memcpy( theme_originallight, igGetStyle()->Colors, sizeof( theme_originallight ) );


	memset( entries, 0, sizeof( entries ) );
#endif // USE_IMGUI

	nextentry = entries;
	rootentry = nextentry++;

	sprintf( rootentry->name, "Rum and Raisin Doom Dashboard" );
	rootentry->type = MET_Category;
	rootentry->freechild = rootentry->children;
	rootentry->enabled = true;
	rootentry->comparison = -1;

	// Core layout. Add top level categories here
	{
		M_RegisterDashboardButton( "Close dashboard", NULL, &M_OnDashboardCloseButton, NULL );
		M_FindDashboardCategory( "Core" );
		M_FindDashboardCategory( "Game" );
		M_FindDashboardCategory( "Render" );
		M_FindDashboardCategory( "Map" );
	}

	thistheme = themes;
	while( thistheme->name != NULL && thistheme->colors != NULL )
	{
		memset( themefullpath, 0, sizeof( themefullpath ) );
		sprintf( themefullpath, "Core|Theme|%s", thistheme->name );
		M_RegisterDashboardRadioButton( themefullpath, NULL, &dashboard_theme, (int32_t)( thistheme - themes ) );
		++thistheme;
	}

	M_RegisterDashboardCheckbox( "Core|Pause While Active", "Multiplayer ignores this", (boolean*)&dashboardpausesplaysim );
	M_RegisterDashboardWindow( "Core|About", "About " PACKAGE_NAME, 0, 0, &aboutwindow_open, Menu_Normal, &M_AboutWindow );
	M_RegisterDashboardWindow( "Core|Licenses", "Licenses", 0, 0, &licenseswindow_open, Menu_Normal, &M_LicensesWindow );
	M_RegisterDashboardSeparator( "Core" );
	M_RegisterDashboardButton( "Core|Quit", "Yes, this means quit the game", &M_OnDashboardCoreQuit, NULL );

}

void M_BindDashboardVariables( void )
{
	M_BindIntVariable("dashboard_theme",			&dashboard_theme);
	M_BindIntVariable("dashboard_pausesplaysim",	&dashboardpausesplaysim );
}

static void M_RenderTooltip( menuentry_t* cat )
{
	if( cat->tooltip[ 0 ] && igIsItemHovered( ImGuiHoveredFlags_None ) )
	{
		igBeginTooltip();
		igText( cat->tooltip );
		igEndTooltip();
	}
}

static void M_ThrowAnErrorBecauseThisIsInvalid( menuentry_t* cat )
{
	I_Error( "Invalid debug menu entry, whoops" );
}

static void M_RenderWindowItem( menuentry_t* cat )
{
	igCheckbox( cat->name, (bool*)cat->data );
}

static void M_RenderCategoryItem( menuentry_t* cat )
{
	if( igBeginMenu( cat->name, true ) )
	{
		M_RenderDashboardChildren( cat->children );
		igEndMenu();
	}
}

static void M_RenderButtonItem( menuentry_t* cat )
{
	menufunc_t callback;
	if( igMenuItemBool( cat->name, "", false, true ) )
	{
		callback = cat->callback;
		callback( cat->name, cat->data );
	}
	M_RenderTooltip( cat );
}

static void M_RenderCheckboxItem( menuentry_t* cat )
{
	igCheckbox( cat->name, (bool*)cat->data );
	M_RenderTooltip( cat );
}

static void M_RenderCheckboxFlagItem( menuentry_t* cat )
{
	igCheckboxFlags( cat->name, (uint32_t*)cat->data, *(uint32_t*)&cat->comparison );
	M_RenderTooltip( cat );
}

static void M_RenderRadioButtonItem( menuentry_t* cat )
{
	igRadioButtonIntPtr( cat->name, (int32_t*)cat->data, cat->comparison );
	M_RenderTooltip( cat );
}

static void M_RenderSeparator( menuentry_t* cat )
{
	igSeparator( );
}

static void M_RenderDashboardChildren( menuentry_t** children )
{
	menuentry_t* child;

	while( *children )
	{
		child = *children;
		childfuncs[ child->type ]( child );
		++children;
	}
}

static int32_t M_GetDashboardWindowsOpened( menuentry_t** children )
{
	menuentry_t* child = NULL;
	int32_t windowcount = 0;

	while( *children )
	{
		child = *children;

		if( child->type == MET_Window && *(boolean*)child->data )
		{
			++windowcount;
		}

		if( child->children[ 0 ] )
		{
			windowcount += M_GetDashboardWindowsOpened( child->children );
		}

		++children;
	}

	return windowcount;
}


static int32_t M_RenderDashboardWindowActivationItems( menuentry_t** children )
{
	menuentry_t* child = NULL;
	int32_t windowcount = 0;

	while( *children )
	{
		child = *children;

		if( child->type == MET_Window && *(boolean*)child->data )
		{
			++windowcount;
			if( igMenuItemBool( child->caption, "", false, true ) )
			{
				igSetWindowFocusStr( child->caption );
			}
		}

		if( child->children[ 0 ] )
		{
			windowcount += M_RenderDashboardWindowActivationItems( child->children );
		}

		++children;
	}

	return windowcount;
}

void M_RenderDashboard( void )
{
	menuentry_t* thiswindow;
	menufunc_t callback;
	int32_t renderedwindowscount = 0;
	int32_t windowflags = ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoCollapse;
	if( !dashboardactive ) windowflags |= ImGuiWindowFlags_NoInputs;

	memcpy( igGetStyle()->Colors, themes[ dashboard_theme ].colors, sizeof( igGetStyle()->Colors ) );

	thiswindow = entries;

	if( dashboardactive )
	{
		if( igBeginMainMenuBar() )
		{
			M_RenderDashboardChildren( rootentry->children );

			if( igBeginMenu( "Windows", true ) )
			{
				if( igMenuItemBool( "Backbuffer", "", false, true ) ) igSetWindowFocusStr( "Backbuffer" );
				if( igMenuItemBool( "Log", "", false, true ) ) igSetWindowFocusStr( "Log" );

				if( M_GetDashboardWindowsOpened( rootentry->children ) > 0 )
				{
					igSeparator();

					M_RenderDashboardWindowActivationItems( rootentry->children );
				}

				igEndMenu();
			}
		}
		igEndMainMenuBar();
	}

	while( thiswindow != nextentry )
	{
		if( thiswindow->type == MET_Window )
		{
			bool isopen = *(bool*)thiswindow->data && ( dashboardactive || ( thiswindow->properties & Menu_Overlay ) == Menu_Overlay );
			if( isopen )
			{
				igPushIDPtr( thiswindow );
				{
					igSetNextWindowSize( thiswindow->dimensions, ImGuiCond_FirstUseEver );
					if( igBegin( thiswindow->caption, (bool*)thiswindow->data, windowflags ) )
					{
						callback = thiswindow->callback;
						callback( thiswindow->name, NULL );
					}
					igEnd();
				}
				igPopID();
			}
		}

		++thiswindow;
	}
}

static menuentry_t* M_FindOrCreateDashboardCategory( const char* category_name, menuentry_t* potential_parent )
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

static menuentry_t* M_FindDashboardCategory( const char* category_name )
{
	size_t start = 0;
	size_t curr = 0;
	size_t end = strlen( category_name );

	char tokbuffer[ MAXNAMELENGTH + 1 ];
	memset( tokbuffer, 0, sizeof( tokbuffer ) );

	menuentry_t* currcategory = rootentry;

	if( !category_name )
	{
		return rootentry;
	}

	while( curr != end )
	{
		if( category_name[ curr ] == '|' )
		{
			if( curr != start )
			{
				strncpy( tokbuffer, &category_name[ start ], ( curr - start ) );
				currcategory = M_FindOrCreateDashboardCategory( tokbuffer, currcategory );
				memset( tokbuffer, 0, sizeof( tokbuffer ) );
			}
			start = curr + 1;
		}
		++curr;
	}

	if( curr != start )
	{
		strncpy( tokbuffer, &category_name[ start ], ( curr - start ) );
		currcategory = M_FindOrCreateDashboardCategory( tokbuffer, currcategory );
	}

	return currcategory;
}

static menuentry_t* M_FindDashboardCategoryFromFullPath( const char* full_path )
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

	category = M_FindDashboardCategory( categoryname );

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

void M_RegisterDashboardCategory( const char* full_path )
{
	M_FindDashboardCategory( full_path );

	CHECK_ELEMENTS();
}

void M_RegisterDashboardButton( const char* full_path, const char* tooltip, menufunc_t callback, void* callbackdata )
{
	menuentry_t* category = M_FindDashboardCategoryFromFullPath( full_path );
	menuentry_t* currentry = nextentry++;
	const char* actualname = M_GetActualName( full_path );

	sprintf( currentry->name, actualname );
	if( tooltip ) sprintf( currentry->tooltip, tooltip );
	currentry->type = MET_Button;
	currentry->freechild = currentry->children;
	currentry->data = callbackdata;
	currentry->enabled = true;
	currentry->comparison = -1;
	currentry->parent = category;
	currentry->callback = callback;

	*category->freechild = currentry;
	category->freechild++;

	CHECK_CHILDREN( category );
	CHECK_ELEMENTS();
}

void M_RegisterDashboardWindow( const char* full_path, const char* caption, int32_t initialwidth, int32_t initialheight, boolean* active, menuproperties_t properties, menufunc_t callback )
{
	menuentry_t* category = M_FindDashboardCategoryFromFullPath( full_path );
	menuentry_t* currentry = nextentry++;
	const char* actualname = M_GetActualName( full_path );

	sprintf( currentry->name, actualname );
	sprintf( currentry->caption, caption );
	currentry->type = MET_Window;
	currentry->properties = properties;
	currentry->freechild = currentry->children;
	currentry->data = active;
	currentry->dimensions.x = (float_t)initialwidth;
	currentry->dimensions.y = (float_t)initialheight;
	currentry->enabled = true;
	currentry->comparison = -1;
	currentry->parent = category;
	currentry->callback = callback;

	*category->freechild = currentry;
	category->freechild++;

	CHECK_CHILDREN( category );
	CHECK_ELEMENTS();
}

void M_RegisterDashboardCheckbox( const char* full_path, const char* tooltip, boolean* value )
{
	menuentry_t* category = M_FindDashboardCategoryFromFullPath( full_path );
	menuentry_t* currentry = nextentry++;
	const char* actualname = M_GetActualName( full_path );

	sprintf( currentry->name, actualname );
	if( tooltip ) sprintf( currentry->tooltip, tooltip );
	currentry->type = MET_Checkbox;
	currentry->freechild = currentry->children;
	currentry->data = value;
	currentry->enabled = true;
	currentry->comparison = -1;
	currentry->parent = category;

	*category->freechild = currentry;
	category->freechild++;

	CHECK_CHILDREN( category );
	CHECK_ELEMENTS();
}

void M_RegisterDashboardCheckboxFlag( const char* full_path, const char* tooltip, int32_t* value, int32_t flagsval )
{
	menuentry_t* category = M_FindDashboardCategoryFromFullPath( full_path );
	menuentry_t* currentry = nextentry++;
	const char* actualname = M_GetActualName( full_path );

	sprintf( currentry->name, actualname );
	if( tooltip ) sprintf( currentry->tooltip, tooltip );
	currentry->type = MET_CheckboxFlag;
	currentry->freechild = currentry->children;
	currentry->data = value;
	currentry->enabled = true;
	currentry->comparison = flagsval;
	currentry->parent = category;

	*category->freechild = currentry;
	category->freechild++;

	CHECK_CHILDREN( category );
	CHECK_ELEMENTS();
}

void M_RegisterDashboardRadioButton( const char* full_path, const char* tooltip, int32_t* value, int32_t selectedval )
{
	menuentry_t* category = M_FindDashboardCategoryFromFullPath( full_path );
	menuentry_t* currentry = nextentry++;
	const char* actualname = M_GetActualName( full_path );

	sprintf( currentry->name, actualname );
	if( tooltip ) sprintf( currentry->tooltip, tooltip );
	currentry->type = MET_RadioButton;
	currentry->freechild = currentry->children;
	currentry->data = value;
	currentry->enabled = true;
	currentry->comparison = selectedval;
	currentry->parent = category;

	*category->freechild = currentry;
	category->freechild++;

	CHECK_CHILDREN( category );
	CHECK_ELEMENTS();
}

void M_RegisterDashboardSeparator( const char* category_path )
{
	menuentry_t* category = M_FindDashboardCategory( category_path );
	menuentry_t* currentry = nextentry++;

	sprintf( currentry->name, "__separator__%" PRId64, ( currentry - entries ) );
	currentry->type = MET_Separator;
	currentry->freechild = currentry->children;
	currentry->enabled = true;
	currentry->comparison = -1;
	currentry->parent = category;

	*category->freechild = currentry;
	category->freechild++;

	CHECK_CHILDREN( category );
	CHECK_ELEMENTS();
}