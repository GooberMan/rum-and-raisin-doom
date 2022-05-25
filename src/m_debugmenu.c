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

#include <stdio.h>
#include <string.h>

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
// Color palette at https://coolors.co/069e2d-058e3f-04773b-036016-03440c
// Modified for white text, and orange buttons etc
static ImVec4 theme_greenallthewaydown[ ImGuiCol_COUNT ] =
{
	{ 1, 1, 1, 0.8 },								// ImGuiCol_Text,
	{ 0.4, 0.4, 0.4, 0.8 },							// ImGuiCol_TextDisabled,
	{ 0.0235294, 0.619608, 0.176471, 0.8 },			// ImGuiCol_WindowBg,
	{ 0, 0, 0, 0.2 },								// ImGuiCol_ChildBg,
	{ 0.0235294, 0.619608, 0.176471, 0.901961 },	// ImGuiCol_PopupBg,
	{ 0.423529, 1.01961, 0.576471, 0.8 },			// ImGuiCol_Border,
	{ 0, 0, 0, 0.8 },								// ImGuiCol_BorderShadow,
	{ 0.0117647, 0.376471, 0.0862745, 0.4 },		// ImGuiCol_FrameBg,
	{ 0.211765, 0.576471, 0.286275, 0.4 },			// ImGuiCol_FrameBgHovered,
	{ 0.111765, 0.476471, 0.186275, 1 },			// ImGuiCol_FrameBgActive,
	{ 0.0235294, 0.619608, 0.176471, 0.8 },			// ImGuiCol_TitleBg,
	{ 0.123529, 0.719608, 0.276471, 1 },			// ImGuiCol_TitleBgActive,
	{ 0, 0.419608, 0, 0.8 },						// ImGuiCol_TitleBgCollapsed,
	{ 0, 0.419608, 0, 0.8 },						// ImGuiCol_MenuBarBg,
	{ 0.423529, 1.01961, 0.576471, 0.501961 },		// ImGuiCol_ScrollbarBg,
	{ 0.323529, 0.919608, 0.476471, 0.8 },			// ImGuiCol_ScrollbarGrab,
	{ 0.523529, 1.11961, 0.676471, 0.8 },			// ImGuiCol_ScrollbarGrabHovered,
	{ 0.423529, 1.01961, 0.576471, 1 },				// ImGuiCol_ScrollbarGrabActive,
	{ 1, 0.623529, 0.262745, 0.8 },					// ImGuiCol_CheckMark,
	{ 1, 0.623529, 0.262745, 0.8 },					// ImGuiCol_SliderGrab,
	{ 1.1, 0.723529, 0.362745, 1 },					// ImGuiCol_SliderGrabActive,
	{ 0.0156863, 0.466667, 0.231373, 0.8 },			// ImGuiCol_Button,
	{ 0.215686, 0.666667, 0.431373, 0.8 },			// ImGuiCol_ButtonHovered,
	{ 0.115686, 0.566667, 0.331373, 1 },			// ImGuiCol_ButtonActive,
	{ 0.0117647, 0.376471, 0.0862745, 0.8 },		// ImGuiCol_Header,
	{ 0.211765, 0.576471, 0.286275, 0.8 },			// ImGuiCol_HeaderHovered,
	{ 0.111765, 0.476471, 0.186275, 1 },			// ImGuiCol_HeaderActive,
	{ 0.423529, 1.01961, 0.576471, 0.8 },			// ImGuiCol_Separator,
	{ 0.623529, 1.21961, 0.776471, 0.8 },			// ImGuiCol_SeparatorHovered,
	{ 0.523529, 1.11961, 0.676471, 1 },				// ImGuiCol_SeparatorActive,
	{ 0.0156863, 0.466667, 0.231373, 0.2 },			// ImGuiCol_ResizeGrip,
	{ 0.215686, 0.666667, 0.431373, 0.2 },			// ImGuiCol_ResizeGripHovered,
	{ 0.115686, 0.566667, 0.331373, 1 },			// ImGuiCol_ResizeGripActive,
	{ 0.0156863, 0.466667, 0.231373, 0.6 },			// ImGuiCol_Tab,
	{ 0.215686, 0.666667, 0.431373, 0.6 },			// ImGuiCol_TabHovered,
	{ 0.115686, 0.566667, 0.331373, 1 },			// ImGuiCol_TabActive,
	{ 0.0156863, 0.466667, 0.231373, 0.6 },			// ImGuiCol_TabUnfocused,
	{ 0.115686, 0.566667, 0.331373, 1 },			// ImGuiCol_TabUnfocusedActive,
	{ 1, 0.623529, 0.262745, 0.8 },					// ImGuiCol_PlotLines,
	{ 1.2, 0.823529, 0.462745, 0.8 },				// ImGuiCol_PlotLinesHovered,
	{ 1, 0.623529, 0.262745, 0.8 },					// ImGuiCol_PlotHistogram,
	{ 1.2, 0.823529, 0.462745, 0.8 },				// ImGuiCol_PlotHistogramHovered,
	{ 1, 0.623529, 0.262745, 0.4 },					// ImGuiCol_TextSelectedBg,
	{ 1, 0.623529, 0.262745, 0.8 },					// ImGuiCol_DragDropTarget,
	{ 1, 1, 1, 0.8 },								// ImGuiCol_NavHighlight,
	{ 1, 1, 1, 0.8 },								// ImGuiCol_NavWindowingHighlight,
	{ 1, 1, 1, 0.2 },								// ImGuiCol_NavWindowingDimBg,
	{ 0, 0, 0, 0.6 },								// ImGuiCol_ModalWindowDimBg,
};

// Generated with above, but using colours plucked from the Doom palette
static ImVec4 theme_doomedspacemarine[ ImGuiCol_COUNT ] =
{
	{ 0.858824, 0.858824, 0.858824, 0.8 },			// ImGuiCol_Text,
	{ 0.258824, 0.258824, 0.258824, 0.8 },			// ImGuiCol_TextDisabled,
	{ 0.262745, 0, 0, 0.8 },						// ImGuiCol_WindowBg,
	{ 0, 0, 0, 0.2 },								// ImGuiCol_ChildBg,
	{ 0.262745, 0, 0, 0.901961 },					// ImGuiCol_PopupBg,
	{ 0.662745, 0.4, 0.4, 0.8 },					// ImGuiCol_Border,
	{ 0, 0, 0, 0.8 },								// ImGuiCol_BorderShadow,
	{ 0.309804, 0.309804, 0.309804, 0.4 },			// ImGuiCol_FrameBg,
	{ 0.509804, 0.509804, 0.509804, 0.4 },			// ImGuiCol_FrameBgHovered,
	{ 0.409804, 0.409804, 0.409804, 1 },			// ImGuiCol_FrameBgActive,
	{ 0.262745, 0, 0, 0.8 },						// ImGuiCol_TitleBg,
	{ 0.362745, 0.1, 0.1, 1 },						// ImGuiCol_TitleBgActive,
	{ 0.0627451, 0, 0, 0.8 },						// ImGuiCol_TitleBgCollapsed,
	{ 0.0627451, 0, 0, 0.8 },						// ImGuiCol_MenuBarBg,
	{ 0.662745, 0.4, 0.4, 0.501961 },				// ImGuiCol_ScrollbarBg,
	{ 0.562745, 0.3, 0.3, 0.8 },					// ImGuiCol_ScrollbarGrab,
	{ 0.762745, 0.5, 0.5, 0.8 },					// ImGuiCol_ScrollbarGrabHovered,
	{ 0.662745, 0.4, 0.4, 1 },						// ImGuiCol_ScrollbarGrabActive,
	{ 1, 1, 0.137255, 0.8 },						// ImGuiCol_CheckMark,
	{ 1, 1, 0.137255, 0.8 },						// ImGuiCol_SliderGrab,
	{ 1.1, 1.1, 0.237255, 1 },						// ImGuiCol_SliderGrabActive,
	{ 0.247059, 0.513726, 0.184314, 0.8 },			// ImGuiCol_Button,
	{ 0.447059, 0.713726, 0.384314, 0.8 },			// ImGuiCol_ButtonHovered,
	{ 0.347059, 0.613726, 0.284314, 1 },			// ImGuiCol_ButtonActive,
	{ 0.309804, 0.309804, 0.309804, 0.8 },			// ImGuiCol_Header,
	{ 0.509804, 0.509804, 0.509804, 0.8 },			// ImGuiCol_HeaderHovered,
	{ 0.409804, 0.409804, 0.409804, 1 },			// ImGuiCol_HeaderActive,
	{ 0.662745, 0.4, 0.4, 0.8 },					// ImGuiCol_Separator,
	{ 0.862745, 0.6, 0.6, 0.8 },					// ImGuiCol_SeparatorHovered,
	{ 0.762745, 0.5, 0.5, 1 },						// ImGuiCol_SeparatorActive,
	{ 0.247059, 0.513726, 0.184314, 0.2 },			// ImGuiCol_ResizeGrip,
	{ 0.447059, 0.713726, 0.384314, 0.2 },			// ImGuiCol_ResizeGripHovered,
	{ 0.347059, 0.613726, 0.284314, 1 },			// ImGuiCol_ResizeGripActive,
	{ 0.247059, 0.513726, 0.184314, 0.6 },			// ImGuiCol_Tab,
	{ 0.447059, 0.713726, 0.384314, 0.6 },			// ImGuiCol_TabHovered,
	{ 0.347059, 0.613726, 0.284314, 1 },			// ImGuiCol_TabActive,
	{ 0.247059, 0.513726, 0.184314, 0.6 },			// ImGuiCol_TabUnfocused,
	{ 0.347059, 0.613726, 0.284314, 1 },			// ImGuiCol_TabUnfocusedActive,
	{ 1, 1, 0.137255, 0.8 },						// ImGuiCol_PlotLines,
	{ 1.2, 1.2, 0.337255, 0.8 },					// ImGuiCol_PlotLinesHovered,
	{ 1, 1, 0.137255, 0.8 },						// ImGuiCol_PlotHistogram,
	{ 1.2, 1.2, 0.337255, 0.8 },					// ImGuiCol_PlotHistogramHovered,
	{ 1, 1, 0.137255, 0.4 },						// ImGuiCol_TextSelectedBg,
	{ 1, 1, 0.137255, 0.8 },						// ImGuiCol_DragDropTarget,
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
	{ "Doomed Space Marine",		theme_doomedspacemarine },
	{ "Valve Classic",				theme_valveclassic },
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

static menuentry_t* M_FindDebugMenuCategory( const char* category_name );
static void M_RenderDebugMenuChildren( menuentry_t** children );
static void M_ThrowAnErrorBecauseThisIsInvalid( menuentry_t* cat );
static void M_RenderWindowItem( menuentry_t* cat );
static void M_RenderCategoryItem( menuentry_t* cat );
static void M_RenderButtonItem( menuentry_t* cat );
static void M_RenderCheckboxItem( menuentry_t* cat );
static void M_RenderCheckboxFlagItem( menuentry_t* cat );
static void M_RenderRadioButtonItem( menuentry_t* cat );
static void M_RenderSeparator( menuentry_t* cat );

ImGuiContext*			imgui_context;
boolean					debugmenuactive = false;
boolean					debugmenuremappingkey = false;
boolean					debugmenupausesplaysim = true;
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
	&M_RenderCheckboxFlagItem,
	&M_RenderRadioButtonItem,
	&M_RenderSeparator,
	&M_ThrowAnErrorBecauseThisIsInvalid,
};

static boolean aboutwindow_open = false;

#define CHECK_ELEMENTS() { if( (nextentry - entries) > MAXENTRIES ) I_Error( "M_DebugMenu: More than %d elements total added", MAXENTRIES ); }
#define CHECK_CHILDREN( elem ) { if( (elem->freechild - elem->children) > MAXCHILDREN ) I_Error( "M_DebugMenu: Too many child elements added to %s", elem->name ); }

static void M_OnDebugMenuCloseButton( const char* itemname, void* data )
{
	//S_StartSound(NULL, debugmenuclosesound);
	debugmenuactive = false;
}

static void M_OnDebugMenuCoreQuit( const char* itemname, void* data )
{
	I_Quit();
}

static void M_AboutWindow( const char* itemname, void* data )
{
	igText( PACKAGE_STRING );
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

void M_InitDebugMenu( void )
{
	char themefullpath[ MAXNAMELENGTH + 1 ];
	themedata_t* thistheme;

#if USE_IMGUI
	imgui_context = igCreateContext( NULL );

	memcpy( theme_original, igGetStyle()->Colors, sizeof( theme_original ) );

	memset( entries, 0, sizeof( entries ) );
#endif // USE_IMGUI

	nextentry = entries;
	rootentry = nextentry++;

	sprintf( rootentry->name, "Rum and Raisin Doom Debug menu" );
	rootentry->type = MET_Category;
	rootentry->freechild = rootentry->children;
	rootentry->enabled = true;
	rootentry->comparison = -1;

	// Core layout. Add top level categories here
	{
		M_RegisterDebugMenuButton( "Close debug", NULL, &M_OnDebugMenuCloseButton, NULL );
		M_FindDebugMenuCategory( "Core" );
		M_FindDebugMenuCategory( "Game" );
		M_FindDebugMenuCategory( "Render" );
		M_FindDebugMenuCategory( "Map" );
	}

	thistheme = themes;
	while( thistheme->name != NULL && thistheme->colors != NULL )
	{
		memset( themefullpath, 0, sizeof( themefullpath ) );
		sprintf( themefullpath, "Core|Theme|%s", thistheme->name );
		M_RegisterDebugMenuRadioButton( themefullpath, NULL, &debugmenu_theme, (int32_t)( thistheme - themes ) );
		++thistheme;
	}

	M_RegisterDebugMenuCheckbox( "Core|Pause While Active", "Multiplayer ignores this", &debugmenupausesplaysim );
	M_RegisterDebugMenuWindow( "Core|About", "About " PACKAGE_NAME, 0, 0, &aboutwindow_open, Menu_Normal, &M_AboutWindow );
	M_RegisterDebugMenuSeparator( "Core" );
	M_RegisterDebugMenuButton( "Core|Quit", "Yes, this means quit the game", &M_OnDebugMenuCoreQuit, NULL );

}

void M_BindDebugMenuVariables( void )
{
	M_BindIntVariable("debugmenu_theme",			&debugmenu_theme);
	M_BindIntVariable("debugmenu_pausesplaysim",	&debugmenupausesplaysim );
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
		callback( cat->name, cat->data );
	}
	M_RenderTooltip( cat );
}

static void M_RenderCheckboxItem( menuentry_t* cat )
{
	igCheckbox( cat->name, cat->data );
	M_RenderTooltip( cat );
}

static void M_RenderCheckboxFlagItem( menuentry_t* cat )
{
	igCheckboxFlags( cat->name, cat->data, *(uint32_t*)&cat->comparison );
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
	int32_t windowflags = ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoCollapse;
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
			isopen = *(boolean*)thiswindow->data && ( debugmenuactive || ( thiswindow->properties & Menu_Overlay ) == Menu_Overlay );
			if( isopen )
			{
				igPushIDPtr( thiswindow );
				{
					igSetNextWindowSize( thiswindow->dimensions, ImGuiCond_FirstUseEver );
					if( igBegin( thiswindow->caption, thiswindow->data, windowflags ) )
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
				currcategory = M_FindOrCreateDebugMenuCategory( tokbuffer, currcategory );
				memset( tokbuffer, 0, sizeof( tokbuffer ) );
			}
			start = curr + 1;
		}
		++curr;
	}

	if( curr != start )
	{
		strncpy( tokbuffer, &category_name[ start ], ( curr - start ) );
		currcategory = M_FindOrCreateDebugMenuCategory( tokbuffer, currcategory );
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

void M_RegisterDebugMenuCategory( const char* full_path )
{
	M_FindDebugMenuCategory( full_path );

	CHECK_ELEMENTS();
}

void M_RegisterDebugMenuButton( const char* full_path, const char* tooltip, menufunc_t callback, void* callbackdata )
{
	menuentry_t* category = M_FindDebugMenuCategoryFromFullPath( full_path );
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

void M_RegisterDebugMenuWindow( const char* full_path, const char* caption, int32_t initialwidth, int32_t initialheight, boolean* active, menuproperties_t properties, menufunc_t callback )
{
	menuentry_t* category = M_FindDebugMenuCategoryFromFullPath( full_path );
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

void M_RegisterDebugMenuCheckbox( const char* full_path, const char* tooltip, boolean* value )
{
	menuentry_t* category = M_FindDebugMenuCategoryFromFullPath( full_path );
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

void M_RegisterDebugMenuCheckboxFlag( const char* full_path, const char* tooltip, int32_t* value, int32_t flagsval )
{
	menuentry_t* category = M_FindDebugMenuCategoryFromFullPath( full_path );
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

void M_RegisterDebugMenuRadioButton( const char* full_path, const char* tooltip, int32_t* value, int32_t selectedval )
{
	menuentry_t* category = M_FindDebugMenuCategoryFromFullPath( full_path );
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

	CHECK_CHILDREN( category );
	CHECK_ELEMENTS();
}