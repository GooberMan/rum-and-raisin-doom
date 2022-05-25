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
//	DOOM graphics stuff for SDL.
//


#include <stdlib.h>

#include "SDL.h"

#include "m_debugmenu.h"

#define USE_GLAD ( USE_IMGUI )
#include <glad/glad.h>

#include "SDL_opengl.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimguiglue.h"

#include "icon.c"

#include "config.h"
#include "d_loop.h"
#include "deh_str.h"
#include "doomtype.h"
#include "i_input.h"
#include "i_joystick.h"
#include "i_log.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_misc.h"
#include "tables.h"
#include "v_diskicon.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#define MATCH_WINDOW_TO_BACKBUFFER 0

// These are (1) the window (or the full screen) that our game is rendered to
// and (2) the renderer that scales the texture (see below) into this window.
// Breaking from Chocolate Doom, we only support the GL backend.

static SDL_Window *screen = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_GLContext glcontext = NULL;

// Window title

static const char *window_title = "";

// These are (1) the 320x200x8 paletted buffer that we draw to (i.e. the one
// that holds I_VideoBuffer), (2) the 320x200x32 RGBA intermediate buffer that
// we blit the former buffer to, (3) the intermediate 320x200 texture that we
// load the RGBA buffer to and that we render into another texture (4) which
// is upscaled by an integer factor UPSCALE using "nearest" scaling and which
// in turn is finally rendered to screen using "linear" scaling.

static SDL_Surface *screenbuffer = NULL;
static SDL_Surface *argbbuffer = NULL;
static SDL_Texture *texture = NULL;
static SDL_Texture *texture_upscaled = NULL;

// The screen buffer; this is modified to draw things to the screen

pixel_t *I_VideoBuffer = NULL;

typedef struct renderbuffer_s
{
	vbuffer_t		screenbuffer;
	SDL_Surface*	nativesurface;
	SDL_Rect		validregion;
} renderbuffer_t;

renderbuffer_t*		renderbuffers = NULL;
int32_t				renderbuffercount = 0;
int32_t				renderbufferswidth = 0;
int32_t				renderbuffersheight = 0;

static SDL_Rect blit_rect = {
    0,
    0,
    SCREENHEIGHT,
    SCREENWIDTH
};

static uint32_t pixel_format;

// palette

static SDL_Color palette[256];
static boolean palette_to_set;

// display has been set up?

static boolean initialized = false;

// disable mouse?

static boolean nomouse = false;
int usemouse = 1;

// Save screenshots in PNG format.

int png_screenshots = 0;

// 0 = original, 1 = INTERPIC
int border_style = 0;

// 0 = original, 1 = dithered
int border_bezel_style = 0;

// Window position:

char *window_position = "center";

// SDL display number on which to run.

int video_display = 0;

// Screen width and height, from configuration file.

#define DEFAULT_WINDOW_WIDTH	1280
#define DEFAULT_WINDOW_HEIGHT	720
#define DEFAULT_RENDER_WIDTH	1280
#define DEFAULT_RENDER_HEIGHT	800
#define DEFAULT_FULLSCREEN		0

int window_width = DEFAULT_WINDOW_WIDTH;
int window_height = DEFAULT_WINDOW_HEIGHT;

enum renderdimensions_e
{
	RMD_MatchWindow,
	RMD_OperatingSystemScale,
	RMD_Independent,
};

int32_t render_dimensions_mode = RMD_Independent;
int32_t render_width = DEFAULT_RENDER_WIDTH;
int32_t render_height = DEFAULT_RENDER_HEIGHT;

int32_t queued_window_width = DEFAULT_WINDOW_WIDTH;
int32_t queued_window_height = DEFAULT_WINDOW_HEIGHT;
int32_t queued_fullscreen = DEFAULT_FULLSCREEN;
int32_t queued_render_width = DEFAULT_RENDER_WIDTH;
int32_t queued_render_height = DEFAULT_RENDER_HEIGHT;

// Fullscreen mode, 0x0 for SDL_WINDOW_FULLSCREEN_DESKTOP.

int fullscreen_width = 0, fullscreen_height = 0;

int display_width = 0;
int display_height = 0;

// Maximum number of pixels to use for intermediate scale buffer.

static int max_scaling_buffer_pixels = 16000000;

// Run in full screen mode?  (int type for config code)

int fullscreen = DEFAULT_FULLSCREEN;

// Aspect ratio correction mode

int aspect_ratio_correct = true;
static int actualheight;

// Force integer scales for resolution-independent rendering

int integer_scaling = false;

// VGA Porch palette change emulation

int vga_porch_flash = false;

// Time to wait for the screen to settle on startup before starting the
// game (ms)

static int startup_delay = 1000;

// Grab the mouse? (int type for config code). nograbmouse_override allows
// this to be temporarily disabled via the command line.

static int grabmouse = true;
static boolean nograbmouse_override = false;

// If true, game is running as a screensaver

boolean screensaver_mode = false;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen

boolean screenvisible = true;

// If true, we display dots at the bottom of the screen to 
// indicate FPS.

static boolean display_fps_dots;

// If this is true, the screen is rendered but not blitted to the
// video buffer.

static boolean noblit;

// Callback function to invoke to determine whether to grab the 
// mouse pointer.

static grabmouse_callback_t grabmouse_callback = NULL;

// Does the window currently have focus?

static boolean window_focused = true;

// Window resize state.

static boolean need_resize = false;
static unsigned int last_resize_time;
#define RESIZE_DELAY 500

// Gamma correction level to use

int usegamma = 0;

// Joystick/gamepad hysteresis
uint64_t joywait = 0;

extern boolean debugmenuactive;

static boolean MouseShouldBeGrabbed()
{
    // never grab the mouse when in screensaver mode
   
    if (screensaver_mode)
        return false;

    // if the window doesn't have focus, never grab it

    if (!window_focused)
        return false;

	// Debug menu needs mouse. Ergo, don't grab mouse.
	if( debugmenuactive )
		return false;

    // always grab the mouse when full screen (dont want to 
    // see the mouse pointer)

    if (fullscreen)
        return true;

    // Don't grab the mouse if mouse input is disabled

    if (!usemouse || nomouse)
        return false;

    // if we specify not to grab the mouse, never grab

    if (nograbmouse_override || !grabmouse)
        return false;

    // Invoke the grabmouse callback function to determine whether
    // the mouse should be grabbed

    if (grabmouse_callback != NULL)
    {
        return grabmouse_callback();
    }
    else
    {
        return true;
    }
}

void I_SetGrabMouseCallback(grabmouse_callback_t func)
{
    grabmouse_callback = func;
}

// Set the variable controlling FPS dots.

void I_DisplayFPSDots(boolean dots_on)
{
    display_fps_dots = dots_on;
}

static void SetShowCursor(boolean show)
{
    if (!screensaver_mode)
    {
        // When the cursor is hidden, grab the input.
        // Relative mode implicitly hides the cursor.
        SDL_SetRelativeMouseMode(!show);
        SDL_GetRelativeMouseState(NULL, NULL);
    }
}

void I_ShutdownGraphics(void)
{
    if (initialized)
    {
        SetShowCursor(true);

        SDL_QuitSubSystem(SDL_INIT_VIDEO);

        initialized = false;
    }
}



static void HandleWindowEvent(SDL_WindowEvent *event)
{
    int i;

    switch (event->event)
    {
#if 0 // SDL2-TODO
        case SDL_ACTIVEEVENT:
            // need to update our focus state
            UpdateFocus();
            break;
#endif
        case SDL_WINDOWEVENT_EXPOSED:
            palette_to_set = true;
            break;

        case SDL_WINDOWEVENT_RESIZED:
            need_resize = true;
            last_resize_time = SDL_GetTicks();
            break;

        // Don't render the screen when the window is minimized:

        case SDL_WINDOWEVENT_MINIMIZED:
            screenvisible = false;
            break;

        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
            screenvisible = true;
            break;

        // Update the value of window_focused when we get a focus event
        //
        // We try to make ourselves be well-behaved: the grab on the mouse
        // is removed if we lose focus (such as a popup window appearing),
        // and we dont move the mouse around if we aren't focused either.

        case SDL_WINDOWEVENT_FOCUS_GAINED:
            window_focused = true;
            break;

        case SDL_WINDOWEVENT_FOCUS_LOST:
            window_focused = false;
            break;

        // We want to save the user's preferred monitor to use for running the
        // game, so that next time we're run we start on the same display. So
        // every time the window is moved, find which display we're now on and
        // update the video_display config variable.

        case SDL_WINDOWEVENT_MOVED:
            i = SDL_GetWindowDisplayIndex(screen);
            if (i >= 0)
            {
                video_display = i;
            }
            break;

        default:
            break;
    }
}

static boolean ToggleFullScreenKeyShortcut(SDL_Keysym *sym)
{
    Uint16 flags = (KMOD_LALT | KMOD_RALT);
#if defined(__MACOSX__)
    flags |= (KMOD_LGUI | KMOD_RGUI);
#endif
    return (sym->scancode == SDL_SCANCODE_RETURN || 
            sym->scancode == SDL_SCANCODE_KP_ENTER) && (sym->mod & flags) != 0;
}

void I_PerformFullscreen(void)
{
    unsigned int flags = 0;

    fullscreen = queued_fullscreen;

    // TODO: Consider implementing fullscreen toggle for SDL_WINDOW_FULLSCREEN
    // (mode-changing) setup. This is hard because we have to shut down and
    // restart again.
    if (fullscreen_width != 0 || fullscreen_height != 0)
    {
        return;
    }


    if (fullscreen)
    {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    SDL_SetWindowFullscreen(screen, flags);

    if (!fullscreen)
    {
        SDL_SetWindowSize(screen, window_width, window_height);
    }
}

void I_ToggleFullScreen(void)
{
	queued_fullscreen = !fullscreen;
}

void I_SetWindowDimensions( int32_t w, int32_t h )
{
	queued_window_width = w;
	queued_window_height = h;
}

void I_SetRenderDimensions( int32_t w, int32_t h )
{
	queued_render_width = w;
	queued_render_height = h;
}

void I_GetEvent(void)
{
    extern void I_HandleKeyboardEvent(SDL_Event *sdlevent);
    extern void I_HandleMouseEvent(SDL_Event *sdlevent);
    SDL_Event sdlevent;

    SDL_PumpEvents();

    while (SDL_PollEvent(&sdlevent))
    {
#if USE_IMGUI
		CImGui_ImplSDL2_ProcessEvent( &sdlevent );
#endif // USE_IMGUI
        switch (sdlevent.type)
        {
            case SDL_KEYDOWN:
                if (ToggleFullScreenKeyShortcut(&sdlevent.key.keysym))
                {
                    I_ToggleFullScreen();
                    break;
                }
                // deliberate fall-though

            case SDL_KEYUP:
		I_HandleKeyboardEvent(&sdlevent);
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEWHEEL:
                if (usemouse && !nomouse && window_focused)
                {
                    I_HandleMouseEvent(&sdlevent);
                }
                break;

            case SDL_QUIT:
                if (screensaver_mode)
                {
                    I_Quit();
                }
                else
                {
                    event_t event;
                    event.type = ev_quit;
                    D_PostEvent(&event);
                }
                break;

            case SDL_WINDOWEVENT:
                if (sdlevent.window.windowID == SDL_GetWindowID(screen))
                {
                    HandleWindowEvent(&sdlevent.window);
                }
                break;

            default:
                break;
        }
    }
}

//
// I_StartTic
//
void I_StartTic (void)
{
    if (!initialized)
    {
        return;
    }

    I_GetEvent();

    if (usemouse && !nomouse && window_focused)
    {
        I_ReadMouse();
    }

    if (joywait < I_GetTimeTicks())
    {
        I_UpdateJoystick();
    }
}


//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
    // what is this?
}

#define MOUSE_MOVE_TO_BOTTOMRIGHT 0

static void UpdateGrab(void)
{
    static boolean currently_grabbed = false;
    boolean grab;
#if MOUSE_MOVE_TO_BOTTOMRIGHT
    int32_t screen_w, screen_h;
#endif // MOUSE_MOVE_TO_BOTTOMRIGHT

    grab = MouseShouldBeGrabbed();

    if (screensaver_mode)
    {
        // Hide the cursor in screensaver mode

        SetShowCursor(false);
    }
    else if (grab && !currently_grabbed)
    {
        SetShowCursor(false);
    }
    else if (!grab && currently_grabbed)
    {

        SetShowCursor(true);

#if MOUSE_MOVE_TO_BOTTOMRIGHT
        // When releasing the mouse from grab, warp the mouse cursor to
        // the bottom-right of the screen. This is a minimally distracting
        // place for it to appear - we may only have released the grab
        // because we're at an end of level intermission screen, for
        // example.

        SDL_GetWindowSize(screen, &screen_w, &screen_h);
        SDL_WarpMouseInWindow(screen, screen_w - 16, screen_h - 16);
        SDL_GetRelativeMouseState(NULL, NULL);
#endif // MOUSE_MOVE_TO_BOTTOMRIGHT
    }

    currently_grabbed = grab;
}

static void LimitTextureSize(int *w_upscale, int *h_upscale)
{
    SDL_RendererInfo rinfo;
    int orig_w, orig_h;

    orig_w = *w_upscale;
    orig_h = *h_upscale;

    // Query renderer and limit to maximum texture dimensions of hardware:
    if (SDL_GetRendererInfo(renderer, &rinfo) != 0)
    {
        I_Error("CreateUpscaledTexture: SDL_GetRendererInfo() call failed: %s",
                SDL_GetError());
    }

    while (*w_upscale * render_width > rinfo.max_texture_width)
    {
        --*w_upscale;
    }
    while (*h_upscale * render_height > rinfo.max_texture_height)
    {
        --*h_upscale;
    }

    if ((*w_upscale < 1 && rinfo.max_texture_width > 0) ||
        (*h_upscale < 1 && rinfo.max_texture_height > 0))
    {
        I_Error("CreateUpscaledTexture: Can't create a texture big enough for "
                "the whole screen! Maximum texture size %dx%d",
                rinfo.max_texture_width, rinfo.max_texture_height);
    }

    // We limit the amount of texture memory used for the intermediate buffer,
    // since beyond a certain point there are diminishing returns. Also,
    // depending on the hardware there may be performance problems with very
    // huge textures, so the user can use this to reduce the maximum texture
    // size if desired.

    if (max_scaling_buffer_pixels < render_width * render_height)
    {
        I_Error("CreateUpscaledTexture: max_scaling_buffer_pixels too small "
                "to create a texture buffer: %d < %d",
                max_scaling_buffer_pixels, render_width * render_height);
    }

    while (*w_upscale * *h_upscale * render_width * render_height
           > max_scaling_buffer_pixels)
    {
        if (*w_upscale > *h_upscale)
        {
            --*w_upscale;
        }
        else
        {
            --*h_upscale;
        }
    }

    if (*w_upscale != orig_w || *h_upscale != orig_h)
    {
        printf("CreateUpscaledTexture: Limited texture size to %dx%d "
               "(max %d pixels, max texture size %dx%d)\n",
               *w_upscale * render_width, *h_upscale * render_height,
               max_scaling_buffer_pixels,
               rinfo.max_texture_width, rinfo.max_texture_height);
    }
}

static void CreateUpscaledTexture(boolean force)
{
    int w, h;
    int h_upscale, w_upscale;
    static int h_upscale_old, w_upscale_old;

    SDL_Texture *new_texture, *old_texture;

    // Get the size of the renderer output. The units this gives us will be
    // real world pixels, which are not necessarily equivalent to the screen's
    // window size (because of highdpi).
    if (SDL_GetRendererOutputSize(renderer, &w, &h) != 0)
    {
        I_Error("Failed to get renderer output size: %s", SDL_GetError());
    }

    // When the screen or window dimensions do not match the aspect ratio
    // of the texture, the rendered area is scaled down to fit. Calculate
    // the actual dimensions of the rendered area.

    //if (w * actualheight < h * render_width)
    //{
    //    // Tall window.
	//
    //    h = w * actualheight / render_width;
    //}
    //else
    //{
    //    // Wide window.
	//
    //    w = h * render_width / actualheight;
    //}

    // Pick texture size the next integer multiple of the screen dimensions.
    // If one screen dimension matches an integer multiple of the original
    // resolution, there is no need to overscale in this direction.

    w_upscale = (w + render_width - 1) / render_width;
    h_upscale = (h + render_height - 1) / render_height;

    // Minimum texture dimensions of 320x200.

    if (w_upscale < 1)
    {
        w_upscale = 1;
    }
    if (h_upscale < 1)
    {
        h_upscale = 1;
    }

    LimitTextureSize(&w_upscale, &h_upscale);

    // Create a new texture only if the upscale factors have actually changed.

    h_upscale_old = h_upscale;
    w_upscale_old = w_upscale;

    // Set the scaling quality for rendering the upscaled texture to "linear",
    // which looks much softer and smoother than "nearest" but does a better
    // job at downscaling from the upscaled texture to screen.

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    new_texture = SDL_CreateTexture(renderer,
                                pixel_format,
                                SDL_TEXTUREACCESS_TARGET,
                                h_upscale*render_height,
                                w_upscale*render_width);

    old_texture = texture_upscaled;
    texture_upscaled = new_texture;

    if (old_texture != NULL)
    {
        SDL_DestroyTexture(old_texture);
    }
}

#define FPS_DOTS_SUPPORTED 0

//
// I_FinishUpdate
//

#pragma optimize( "", off )

void I_FinishUpdate (void)
{
#if FPS_DOTS_SUPPORTED
	static uint32_t lasttic;
	uint32_t tics;
	int32_t i;
#endif // FPS_DOTS_SUPPORTED

	SDL_Rect Target;

	renderbuffer_t* curr = renderbuffers;
	renderbuffer_t* end = renderbuffers + renderbuffercount;

    if (!initialized)
        return;

    if (noblit)
        return;

    if (need_resize)
    {
        if (SDL_GetTicks() > last_resize_time + RESIZE_DELAY)
        {
            int flags;
            // When the window is resized (we're not in fullscreen mode),
            // save the new window size.
            flags = SDL_GetWindowFlags(screen);
            if ((flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == 0)
            {
                SDL_GetWindowSize(screen, &window_width, &window_height);

                // Adjust the window by resizing again so that the window
                // is the right aspect ratio.
                SDL_SetWindowSize(screen, window_width, window_height);
				queued_window_width = window_width;
				queued_window_height = window_height;
            }
            CreateUpscaledTexture(false);
            need_resize = false;
            palette_to_set = true;
        }
        else
        {
            return;
        }
    }

    UpdateGrab();

#if 0 // SDL2-TODO
    // Don't update the screen if the window isn't visible.
    // Not doing this breaks under Windows when we alt-tab away 
    // while fullscreen.

    if (!(SDL_GetAppState() & SDL_APPACTIVE))
        return;
#endif

    // draws little dots on the bottom of the screen

#if FPS_DOTS_SUPPORTED
    if (display_fps_dots)
    {
	i = I_GetTimeTicks();
	tics = i - lasttic;
	lasttic = i;
	if (tics > 20) tics = 20;

	for (i=0 ; i<tics*4 ; i+=4)
	    I_VideoBuffer[ (render_height-1)*render_width + i] = 0xff;
	for ( ; i<20*4 ; i+=4)
	    I_VideoBuffer[ (render_height-1)*render_width + i] = 0x0;
    }
#endif // FPS_DOTS_SUPPORTED

    // Draw disk icon before blit, if necessary.
    V_DrawDiskIcon();


	if ( palette_to_set && vga_porch_flash)
	{
		// "flash" the pillars/letterboxes with palette changes, emulating
		// VGA "porch" behaviour (GitHub issue #832)
		SDL_SetRenderDrawColor(renderer, palette[0].r, palette[0].g,
			palette[0].b, SDL_ALPHA_OPAQUE);
	}
	
	while( curr != end )
	{
		if (palette_to_set)
		{
			SDL_SetPaletteColors( curr->nativesurface->format->palette, palette, 0, 256 );
		}

		// Blit from the paletted 8-bit screen buffer to the intermediate
		// 32-bit RGBA buffer that we can load into the texture.

		if( curr->validregion.h > 0 )
		{
			SDL_LowerBlit( curr->nativesurface, &curr->validregion, argbbuffer, &curr->validregion );
		}
		++curr;
	}

	palette_to_set = false;

    // Update the intermediate texture with the contents of the RGBA buffer.

    SDL_UpdateTexture(texture, NULL, argbbuffer->pixels, argbbuffer->pitch);

    // Make sure the pillarboxes are kept clear each frame.

    SDL_RenderClear(renderer);

    // Render this intermediate texture into the upscaled texture
    // using "nearest" integer scaling.

	SDL_SetRenderTarget(renderer, texture_upscaled);
	SDL_RenderCopy(renderer, texture, NULL, NULL);

    // Finally, render this upscaled texture to screen using linear scaling.

	SDL_SetRenderTarget(renderer, NULL);

	SDL_GL_BindTexture( texture_upscaled, NULL, NULL );
	GLint whichID;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &whichID);
	SDL_GL_UnbindTexture( texture );

	if( USE_IMGUI && !debugmenuactive )
	{
		// Better transormation courtesy of Altazimuth
		Target.x = (render_width - actualheight) / 2;
		Target.y = (actualheight - render_width) / 2;
		Target.w = actualheight;
		Target.h = render_width;

		SDL_RenderCopyEx(renderer, texture_upscaled, NULL, &Target, 90.0, NULL, SDL_FLIP_VERTICAL);
	}

	// ImGui time!
#if USE_IMGUI
	CImGui_ImplOpenGL3_NewFrame();
	CImGui_ImplSDL2_NewFrame( screen );

	igGetIO()->WantCaptureKeyboard = debugmenuactive;
	igGetIO()->WantCaptureMouse = debugmenuactive;

	igNewFrame();

	static float uv0_x = 0;
	static float uv0_y = 0;
	static float uv1_x = 1;
	static float uv1_y = 1;

	static float lastwidth = 0;
	static float lastheight = 0;

	static ImVec2 backbuffersize = { 640, 480 };
	static ImVec2 backbufferpos = { 50, 50 };
	static ImVec2 logsize = { 500, 300 };
	static ImVec2 logpos = { 720, 50 };
	static ImVec2 zeropivot = { 0, 0 };

	static const ImU32 textcolors[ Log_Max ] =
	{
		IM_COL32( 0xd6, 0xcb, 0xac, 0xff ),
		IM_COL32( 0xbd, 0xbd, 0xbd, 0xff ),
		IM_COL32( 0x47, 0xae, 0x0e, 0xff ),
		IM_COL32( 0x11, 0x7e, 0xe3, 0xff ),
		IM_COL32( 0xee, 0x8e, 0x13, 0xff ),
		IM_COL32( 0xee, 0x13, 0x13, 0xff ),
	};

	if( debugmenuactive )
	{
		if( lastwidth > 0 && lastheight > 0 )
		{
			if( lastwidth != render_width || lastheight != actualheight )
			{
				backbuffersize.y = backbuffersize.x * ( (float)actualheight / (float)render_width );
				igSetNextWindowSize( backbuffersize, ImGuiCond_Always );
				lastwidth = render_width;
				lastheight = actualheight;
			}
		}
		else
		{
			backbuffersize.x = window_width * 0.6f;
			backbuffersize.y = backbuffersize.x * ( (float)actualheight / (float)render_width );
			lastwidth = render_width;
			lastheight = actualheight;

			logpos.x = window_width - logsize.x - 50.f;
			logpos.y = window_height - logsize.y - 50.f;
		}

		igSetNextWindowSize( backbuffersize, ImGuiCond_FirstUseEver );
		igSetNextWindowPos( backbufferpos, ImGuiCond_FirstUseEver, zeropivot );
		if( igBegin( "Backbuffer", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus ) )
		{
			igGetWindowSize( &backbuffersize );
			igGetWindowPos( &backbufferpos );
			// TODO: Get correct margin sizes
			ImVec2 size = { backbuffersize.x - 20, backbuffersize.y - 40 };
			glTexParameteri( GL_TEXTURE_2D,  GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			glTexParameteri( GL_TEXTURE_2D,  GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			ImVec2 uvtl = { 0, 0 };
			ImVec2 uvtr = { 0, 1 };
			ImVec2 uvll = { 1, 0 };
			ImVec2 uvlr = { 1, 1 };
			ImVec4 tint = { 1, 1, 1, 1 };
			ImVec4 border = { 0, 0, 0, 0 };
			igImageQuad( (ImTextureID)whichID, size, uvtl, uvtr, uvlr, uvll, tint, border );
		}
		igEnd();

		igPushStyleColorU32( ImGuiCol_WindowBg, IM_COL32_BLACK );
		igSetNextWindowSize( logsize, ImGuiCond_FirstUseEver );
		igSetNextWindowPos( logpos, ImGuiCond_FirstUseEver, zeropivot );
		if( igBegin( "Log", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus ) )
		{
			igGetWindowSize( &logsize );
			igGetWindowPos( &logpos );

			size_t currentry;
			size_t numentries = I_LogNumEntries();

			igColumns( 2, "", false );
			igSetColumnWidth( 0, 70.f );
			igPushStyleColorU32( ImGuiCol_Text, textcolors[ Log_Normal ] );

			for( currentry = 0; currentry < numentries; ++currentry )
			{
				igText( I_LogGetTimestamp( currentry ) );
				igNextColumn();
				igPushStyleColorU32( ImGuiCol_Text, textcolors[ I_LogGetEntryType( currentry ) ] );
				igText( I_LogGetEntryText( currentry ) );
				igPopStyleColor( 1 );
				igNextColumn();
			}

			igPopStyleColor( 1 );

			igColumns( 1, "", false );

			if( igGetScrollY() == igGetScrollMaxY() )
			{
				igSetScrollHereY( 1.f );
			}
		}
		igEnd();
		igPopStyleColor( 1 );
	}

	M_RenderDebugMenu();

	igRender();
	CImGui_ImplOpenGL3_RenderDrawData( igGetDrawData() );
#endif // USE_IMGUI
    // Draw!

    SDL_RenderPresent(renderer);
}

#pragma optimize( "", on )



//
// I_ReadScreen
//
void I_ReadScreen (pixel_t* scr)
{
    memcpy(scr, I_VideoBuffer, render_width*render_height*sizeof(*scr));
}


//
// I_SetPalette
//
void I_SetPalette (byte *doompalette)
{
    int i;

    for (i=0; i<256; ++i)
    {
        // Zero out the bottom two bits of each channel - the PC VGA
        // controller only supports 6 bits of accuracy.

        palette[i].r = gammatable[usegamma][*doompalette++] & ~3;
        palette[i].g = gammatable[usegamma][*doompalette++] & ~3;
        palette[i].b = gammatable[usegamma][*doompalette++] & ~3;
    }

    palette_to_set = true;
}

// Given an RGB value, find the closest matching palette index.

int I_GetPaletteIndex(int r, int g, int b)
{
    int best, best_diff, diff;
    int i;

    best = 0; best_diff = INT_MAX;

    for (i = 0; i < 256; ++i)
    {
        diff = (r - palette[i].r) * (r - palette[i].r)
             + (g - palette[i].g) * (g - palette[i].g)
             + (b - palette[i].b) * (b - palette[i].b);

        if (diff < best_diff)
        {
            best = i;
            best_diff = diff;
        }

        if (diff == 0)
        {
            break;
        }
    }

    return best;
}

// 
// Set the window title
//

void I_SetWindowTitle(const char *title)
{
    window_title = title;
}

//
// Call the SDL function to set the window title, based on 
// the title set with I_SetWindowTitle.
//

#if !defined( NDEBUG )
#define EDITION_STRING " THIS IS A DEBUG BUILD STOP PROFILING ON A DEBUG BUILD"
#else
#define EDITION_STRING " RELEASE BUILD"
#endif // !defined( NDEBUG )

void I_InitWindowTitle(void)
{
    char *buf;

    buf = M_StringJoin(window_title, " - ", PACKAGE_STRING EDITION_STRING, NULL);
    SDL_SetWindowTitle(screen, buf);
    free(buf);
}

// Set the application icon

void I_InitWindowIcon(void)
{
    SDL_Surface *surface;

    surface = SDL_CreateRGBSurfaceFrom((void *) icon_data, icon_w, icon_h,
                                       32, icon_w * 4,
                                       0xff << 24, 0xff << 16,
                                       0xff << 8, 0xff << 0);

    SDL_SetWindowIcon(screen, surface);
    SDL_FreeSurface(surface);
}

// Set video size to a particular scale factor (1x, 2x, 3x, etc.)

static void SetScaleFactor(int factor)
{
    // Pick 320x200 or 320x240, depending on aspect ratio correct

    window_width = factor * render_width;
    window_height = factor * actualheight;
    fullscreen = false;
}

void I_GraphicsCheckCommandLine(void)
{
    int i;

    //!
    // @category video
    // @vanilla
    //
    // Disable blitting the screen.
    //

    noblit = M_CheckParm ("-noblit");

    //!
    // @category video 
    //
    // Don't grab the mouse when running in windowed mode.
    //

    nograbmouse_override = M_ParmExists("-nograbmouse");

    // default to fullscreen mode, allow override with command line
    // nofullscreen because we love prboom

    //!
    // @category video 
    //
    // Run in a window.
    //

    if (M_CheckParm("-window") || M_CheckParm("-nofullscreen"))
    {
        fullscreen = false;
    }

    //!
    // @category video 
    //
    // Run in fullscreen mode.
    //

    if (M_CheckParm("-fullscreen"))
    {
        fullscreen = true;
    }

    //!
    // @category video 
    //
    // Disable the mouse.
    //

    nomouse = M_CheckParm("-nomouse") > 0;

    //!
    // @category video
    // @arg <x>
    //
    // Specify the screen width, in pixels. Implies -window.
    //

    i = M_CheckParmWithArgs("-width", 1);

    if (i > 0)
    {
        window_width = atoi(myargv[i + 1]);
        fullscreen = false;
    }

    //!
    // @category video
    // @arg <y>
    //
    // Specify the screen height, in pixels. Implies -window.
    //

    i = M_CheckParmWithArgs("-height", 1);

    if (i > 0)
    {
        window_height = atoi(myargv[i + 1]);
        fullscreen = false;
    }

    //!
    // @category video
    // @arg <WxY>
    //
    // Specify the dimensions of the window. Implies -window.
    //

    i = M_CheckParmWithArgs("-geometry", 1);

    if (i > 0)
    {
        int w, h, s;

        s = sscanf(myargv[i + 1], "%ix%i", &w, &h);
        if (s == 2)
        {
            window_width = w;
            window_height = h;
            fullscreen = false;
        }
    }

    //!
    // @category video
    //
    // Don't scale up the screen. Implies -window.
    //

    if (M_CheckParm("-1")) 
    {
        SetScaleFactor(1);
    }

    //!
    // @category video
    //
    // Double up the screen to 2x its normal size. Implies -window.
    //

    if (M_CheckParm("-2")) 
    {
        SetScaleFactor(2);
    }

    //!
    // @category video
    //
    // Double up the screen to 3x its normal size. Implies -window.
    //

    if (M_CheckParm("-3")) 
    {
        SetScaleFactor(3);
    }
}

// Check if we have been invoked as a screensaver by xscreensaver.

void I_CheckIsScreensaver(void)
{
    char *env;

    env = getenv("XSCREENSAVER_WINDOW");

    if (env != NULL)
    {
        screensaver_mode = true;
    }
}

// Check the display bounds of the display referred to by 'video_display' and
// set x and y to a location that places the window in the center of that
// display.
static void CenterWindow(int *x, int *y, int w, int h)
{
    SDL_Rect bounds;

    if (SDL_GetDisplayBounds(video_display, &bounds) < 0)
    {
        fprintf(stderr, "CenterWindow: Failed to read display bounds "
                        "for display #%d!\n", video_display);
        return;
    }

    *x = bounds.x + SDL_max((bounds.w - w) / 2, 0);
    *y = bounds.y + SDL_max((bounds.h - h) / 2, 0);
}

void I_GetWindowPosition(int *x, int *y, int w, int h)
{
    // Check that video_display corresponds to a display that really exists,
    // and if it doesn't, reset it.
    if (video_display < 0 || video_display >= SDL_GetNumVideoDisplays())
    {
        fprintf(stderr,
                "I_GetWindowPosition: We were configured to run on display #%d, "
                "but it no longer exists (max %d). Moving to display 0.\n",
                video_display, SDL_GetNumVideoDisplays() - 1);
        video_display = 0;
    }

    // in fullscreen mode, the window "position" still matters, because
    // we use it to control which display we run fullscreen on.

    if (fullscreen)
    {
        CenterWindow(x, y, w, h);
        return;
    }

    // in windowed mode, the desired window position can be specified
    // in the configuration file.

    if (window_position == NULL || !strcmp(window_position, ""))
    {
        *x = *y = SDL_WINDOWPOS_UNDEFINED;
    }
    else if (!strcmp(window_position, "center"))
    {
        // Note: SDL has a SDL_WINDOWPOS_CENTER, but this is useless for our
        // purposes, since we also want to control which display we appear on.
        // So we have to do this ourselves.
        CenterWindow(x, y, w, h);
    }
    else if (sscanf(window_position, "%i,%i", x, y) != 2)
    {
        // invalid format: revert to default
        fprintf(stderr, "I_GetWindowPosition: invalid window_position setting\n");
        *x = *y = SDL_WINDOWPOS_UNDEFINED;
    }
}

#ifdef __APPLE__
const char* GLSL_VERSION	= "#version 150";
int32_t GL_VERSION_MAJOR	= 3;
int32_t GL_VERSION_MINOR	= 2;
int32_t GL_CONTEXTFLAGS		= SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
int32_t GL_CONTEXTPROFILE	= SDL_GL_CONTEXT_PROFILE_CORE;
#else
const char* GLSL_VERSION	= "#version 130";
int32_t GL_VERSION_MAJOR	= 3;
int32_t GL_VERSION_MINOR	= 0;
int32_t GL_CONTEXTFLAGS		= 0;
int32_t GL_CONTEXTPROFILE	= SDL_GL_CONTEXT_PROFILE_CORE;
#endif

#ifdef __linux__
#define CHECK_GLES			1
#else
#define CHECK_GLES			0
#endif // __linux__

static void I_SetupOpenGL(void)
{
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS,			GL_CONTEXTFLAGS );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK,	GL_CONTEXTPROFILE );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION,	GL_VERSION_MAJOR );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION,	GL_VERSION_MINOR );

	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER,			1 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE,				24 );
	SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE,			8 );
 
}

#if USE_GLAD
static void I_SetupGLAD(void)
{
	int32_t retval;
	// GLAD handles symbol loading for all platforms we care about, so let it sort it out instead
	// of following SDL's advice to use its GL_ProcAddress function.
	retval = gladLoadGL();
	if ( !retval )
	{
		I_Error( "GLAD could not find any OpenGL version" );
	}

	if( !GLAD_GL_VERSION_3_0 )
	{
#if CHECK_GLES
		// Attempt to see if we're embedded, this will be nice
		retval = gladLoadGLES2Loader( (GLADloadproc)&SDL_GL_GetProcAddress );

		if ( !retval )
		{
			I_Error( "GLAD could not find any OpenGL ES version" );
		}

		if( !GLAD_GL_ES_VERSION_3_0 )
		{
			I_Error( "GLAD could not find any OpenGL ES 3" );
		}

		GLSL_VERSION = "#version 300 es";
		GL_CONTEXTPROFILE = SDL_GL_CONTEXT_PROFILE_ES;
#else // !CHECK_GLES
		I_Error( "GLAD could not find OpenGL 3.0.\nVersion detected: %d.%d\nVersion string: %s\nHardware vendor: %s\nHardware driver: %s"
					, GLVersion.major, GLVersion.minor
					, (const char*) glGetString( GL_VERSION )
					, (const char*) glGetString( GL_VENDOR )
					, (const char*) glGetString( GL_RENDERER ) );
#endif // CHECK_GLES
	}
}
#else // !USE_GLAD
#define I_SetupGLAD()
#endif // USE_GLAD

#if USE_IMGUI
static void I_SetupDearImGui(void)
{
	int32_t retval;

	retval = CImGui_ImplSDL2_InitForOpenGL( screen, glcontext );
	if ( !retval )
	{
		I_Error( "ImGui_ImplSDL2_InitForOpenGL failed to initialise" );
	}

	retval = CImGui_ImplOpenGL3_Init( GLSL_VERSION );
	if ( !retval )
	{
		I_Error( "CImGui_ImplOpenGL3_Init failed to initialise" );
	}
}
#else // !USE_IMGUI
#define I_SetupDearImGui()
#endif // USE_IMGUI

static void SetVideoMode(void)
{
	int32_t w, h;
	int32_t x, y;
	//int32_t bpp;
	int32_t window_flags = 0, renderer_flags = 0;
	int32_t drivercount = SDL_GetNumRenderDrivers();
	int32_t testdriver = -1;
	int32_t requesteddriver = -1;
	SDL_RendererInfo info;

	SDL_DisplayMode mode;

    w = window_width;
    h = window_height;

    // In windowed mode, the window can be resized while the game is
    // running.
    window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;

    // Set the highdpi flag - this makes a big difference on Macs with
    // retina displays, especially when using small window sizes.
    window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;

    if (fullscreen)
    {
        if (fullscreen_width == 0 && fullscreen_height == 0)
        {
            // This window_flags means "Never change the screen resolution!
            // Instead, draw to the entire screen by scaling the texture
            // appropriately".
            window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        }
        else
        {
            w = fullscreen_width;
            h = fullscreen_height;
            window_flags |= SDL_WINDOW_FULLSCREEN;
        }
    }

    // Running without window decorations is potentially useful if you're
    // playing in three window mode and want to line up three game windows
    // next to each other on a single desktop.
    // Deliberately not documented because I'm not sure how useful this is yet.
    if (M_ParmExists("-borderless"))
    {
        window_flags |= SDL_WINDOW_BORDERLESS;
    }

    I_GetWindowPosition(&x, &y, w, h);

    // Create window and renderer contexts. We set the window title
    // later anyway and leave the window position "undefined". If
    // "window_flags" contains the fullscreen flag (see above), then
    // w and h are ignored.

    if (screen == NULL)
    {
        screen = SDL_CreateWindow(NULL, x, y, w, h, window_flags);

        if (screen == NULL)
        {
            I_Error("Error creating window for video startup: %s",
            SDL_GetError());
        }

        pixel_format = SDL_GetWindowPixelFormat(screen);

#if MATCH_WINDOW_TO_BACKBUFFER
        SDL_SetWindowMinimumSize(screen, render_width, actualheight);
#endif // MATCH_WINDOW_TO_BACKBUFFER

        I_InitWindowTitle();
        I_InitWindowIcon();
    }

	for( testdriver = 0; testdriver < drivercount; ++testdriver )
	{
		SDL_GetRenderDriverInfo( testdriver, &info );
		if( strcasecmp( info.name, "opengl" ) == 0 )
		{
			requesteddriver = testdriver;
			break;
		}
	}


    // The SDL_RENDERER_TARGETTEXTURE flag is required to render the
    // intermediate texture into the upscaled texture.
    renderer_flags = SDL_RENDERER_TARGETTEXTURE;
	
    if (SDL_GetCurrentDisplayMode(video_display, &mode) != 0)
    {
        I_Error("Could not get display mode for video display #%d: %s",
        video_display, SDL_GetError());
    }

	display_width = mode.w;
	display_height = mode.h;

    // Turn on vsync if we aren't in a -timedemo
    if (!singletics && mode.refresh_rate > 0)
    {
        renderer_flags |= SDL_RENDERER_PRESENTVSYNC;
    }

    if (renderer != NULL)
    {
        SDL_DestroyRenderer(renderer);
        // all associated textures get destroyed
        texture = NULL;
        texture_upscaled = NULL;
    }

	I_SetupOpenGL();

    renderer = SDL_CreateRenderer(screen, requesteddriver, renderer_flags);

    if (renderer == NULL)
    {
        I_Error("Error creating renderer for screen window: %s",
                SDL_GetError());
    }

	glcontext = SDL_GL_GetCurrentContext();
	if( glcontext == NULL )
	{
		I_Error( "SDL not initialised in OpenGL mode" );
	}

	I_SetupGLAD();

    // Force integer scales for resolution-independent rendering.

#if SDL_VERSION_ATLEAST(2, 0, 5)
    SDL_RenderSetIntegerScale(renderer, integer_scaling);
#endif

    // Blank out the full screen area in case there is any junk in
    // the borders that won't otherwise be overwritten.

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

	I_SetupDearImGui();
}

static void I_DeinitRenderBuffers( void )
{
	renderbuffer_t* curr = renderbuffers;
	renderbuffer_t* end = renderbuffers + renderbuffercount;

	if( renderbuffers != NULL )
	{
		SDL_DestroyTexture( texture );

		SDL_FreeSurface( argbbuffer );
		while( curr != end )
		{
			SDL_FreeSurface( curr->nativesurface );
			++curr;
		};

		Z_Free( renderbuffers );
		renderbuffers = NULL;
		renderbuffercount = 0;
		renderbufferswidth = 0;
		renderbuffersheight = 0;
	}
}

static void I_RefreshRenderBuffers( int32_t numbuffers, int32_t width, int32_t height )
{
	renderbuffer_t* curr;
	renderbuffer_t* end;
	uint32_t rmask, gmask, bmask, amask;
	int32_t bpp;

	SDL_Rect region = { 0, 0, height, width };

	if( renderbuffercount != numbuffers
		|| renderbufferswidth != width
		|| renderbuffersheight != height )
	{
		I_DeinitRenderBuffers();

		if( numbuffers == 0 )
		{
			I_Error( "I_RefreshRenderBuffers: You can't have zero buffers. Be sensible." );
		}

		SDL_PixelFormatEnumToMasks( pixel_format, &bpp, &rmask, &gmask, &bmask, &amask );

		renderbuffers = curr = Z_Malloc( sizeof( renderbuffer_t) * numbuffers, PU_STATIC, NULL );
		renderbuffercount = numbuffers;
		end = curr + numbuffers;

		while( curr != end )
		{
			// Transposed aw yiss
			curr->nativesurface = SDL_CreateRGBSurface( 0, height, width, 8, 0, 0, 0, 0 );
			SDL_FillRect( curr->nativesurface, NULL, 0 );

			curr->screenbuffer.data = curr->nativesurface->pixels;
			curr->screenbuffer.width = height;
			curr->screenbuffer.height = width;
			curr->screenbuffer.pitch = curr->nativesurface->pitch;
			curr->screenbuffer.pixel_size_bytes = 1;
			curr->screenbuffer.magic_value = vbuffer_magic;

			SDL_SetPaletteColors( curr->nativesurface->format->palette, palette, 0, 256 );

			curr->validregion = region;

			++curr;
		}

		screenbuffer = renderbuffers->nativesurface;

		// Format of argbbuffer must match the screen pixel format because we
		// import the surface data into the texture.
		argbbuffer = SDL_CreateRGBSurface( 0, height, width, bpp, rmask, gmask, bmask, amask );
		SDL_FillRect( argbbuffer, NULL, 0 );

		queued_render_height = render_height = blit_rect.w = height;
		queued_render_width = render_width = blit_rect.h = width;

		if (aspect_ratio_correct == 1)
		{
			actualheight = ( render_height * 1200 / 1000 );
		}
		else
		{
			actualheight = render_height;
		}

		// Set the scaling quality for rendering the intermediate texture into
		// the upscaled texture to "nearest", which is gritty and pixelated and
		// resembles software scaling pretty well.

		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

		// Create the intermediate texture that the RGBA surface gets loaded into.
		// The SDL_TEXTUREACCESS_STREAMING flag means that this texture's content
		// is going to change frequently.

		texture = SDL_CreateTexture(renderer,
									pixel_format,
									SDL_TEXTUREACCESS_STREAMING,
									render_height, render_width);

		CreateUpscaledTexture( true );

		// Important: Set the "logical size" of the rendering context. At the same
		// time this also defines the aspect ratio that is preserved while scaling
		// and stretching the texture into the window.

		SDL_RenderSetLogicalSize(renderer,
								render_width,
								actualheight);


		// REMOVE THIS ANCIENT CODE ALREADY JEEBUS

		// The actual 320x200 canvas that we draw to. This is the pixel buffer of
		// the 8-bit paletted screen buffer that gets blit on an intermediate
		// 32-bit RGBA screen buffer that gets loaded into a texture that gets
		// finally rendered into our window or full screen in I_FinishUpdate().

		I_VideoBuffer = screenbuffer->pixels;
		V_RestoreBuffer();

		// Clear the screen to black.

		memset(I_VideoBuffer, 0, render_width * render_height * sizeof(*I_VideoBuffer));

	}

}

vbuffer_t* I_GetRenderBuffer( int32_t index )
{
	return &renderbuffers[ index ].screenbuffer;
}

void I_SetRenderBufferValidColumns( int32_t index, int32_t begin, int32_t end )
{
	renderbuffers[ index ].validregion.y = begin;
	renderbuffers[ index ].validregion.h = end - begin;
}

void I_InitGraphics( int32_t numbuffers )
{
	queued_window_width = window_width;
	queued_window_height = window_height;
	queued_fullscreen = fullscreen;
	queued_render_width = render_width;
	queued_render_height = render_height;

    SDL_Event dummy;
    byte *doompal;
    char *env;

    // Pass through the XSCREENSAVER_WINDOW environment variable to 
    // SDL_WINDOWID, to embed the SDL window into the Xscreensaver
    // window.

    env = getenv("XSCREENSAVER_WINDOW");

    if (env != NULL)
    {
        char winenv[30];
        unsigned int winid;

        sscanf(env, "0x%x", &winid);
        M_snprintf(winenv, sizeof(winenv), "SDL_WINDOWID=%u", winid);

        putenv(winenv);
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) 
    {
        I_Error("Failed to initialize video: %s", SDL_GetError());
    }

    // When in screensaver mode, run full screen and auto detect
    // screen dimensions (don't change video mode)
    if (screensaver_mode)
    {
        fullscreen = true;
    }

	if( render_dimensions_mode == RMD_MatchWindow )
	{
		render_width = window_width;
		render_height = window_height;
		// TODO: Scale back down for aspect ratio stuff... although I think I might go ahead and baleete all that.
	}

	if (aspect_ratio_correct == 1)
	{
		actualheight = ( render_height * 1200 / 1000 );
	}
	else
	{
		actualheight = render_height;
	}

    // Create the game window; this may switch graphic modes depending
    // on configuration.
    SetVideoMode();

    // Set the palette

    doompal = W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE);
    I_SetPalette(doompal);

	I_RefreshRenderBuffers( numbuffers, render_width, render_height );

    // SDL2-TODO UpdateFocus();
    UpdateGrab();

    // On some systems, it takes a second or so for the screen to settle
    // after changing modes.  We include the option to add a delay when
    // setting the screen mode, so that the game doesn't start immediately
    // with the player unable to see anything.

    if (fullscreen && !screensaver_mode)
    {
        SDL_Delay(startup_delay);
    }

    // clear out any events waiting at the start and center the mouse
  
    while (SDL_PollEvent(&dummy))
	{
#if USE_IMGUI
		CImGui_ImplSDL2_ProcessEvent( &dummy );
#endif // USE_IMGUI
	}

    initialized = true;

    // Call I_ShutdownGraphics on quit

    I_AtExit(I_ShutdownGraphics, true);
}

//
// I_StartFrame
//
void I_StartFrame (void)
{
	if( queued_render_width != render_width
		|| queued_render_height != render_height )
	{
		I_RefreshRenderBuffers( renderbuffercount, queued_render_width, queued_render_height );
	}

	if( queued_window_width != window_width
		|| queued_window_height != window_height )
	{
		if( !fullscreen )
		{
			SDL_SetWindowSize( screen, queued_window_width, queued_window_height );
		}
		window_width = queued_window_width;
		window_height = queued_window_height;
	}

	if( queued_fullscreen != fullscreen )
	{
		I_PerformFullscreen();
	}
}

// Bind all variables controlling video options into the configuration
// file system.
void I_BindVideoVariables(void)
{
    M_BindIntVariable("use_mouse",                 &usemouse);
    M_BindIntVariable("fullscreen",                &fullscreen);
    M_BindIntVariable("video_display",             &video_display);
    M_BindIntVariable("aspect_ratio_correct",      &aspect_ratio_correct);
    M_BindIntVariable("integer_scaling",           &integer_scaling);
    M_BindIntVariable("vga_porch_flash",           &vga_porch_flash);
    M_BindIntVariable("startup_delay",             &startup_delay);
    M_BindIntVariable("fullscreen_width",          &fullscreen_width);
    M_BindIntVariable("fullscreen_height",         &fullscreen_height);
    M_BindIntVariable("max_scaling_buffer_pixels", &max_scaling_buffer_pixels);
    M_BindIntVariable("window_width",              &window_width);
    M_BindIntVariable("window_height",             &window_height);
    M_BindIntVariable("grabmouse",                 &grabmouse);
    M_BindStringVariable("window_position",        &window_position);
    M_BindIntVariable("usegamma",                  &usegamma);
    M_BindIntVariable("png_screenshots",           &png_screenshots);

	M_BindIntVariable("render_width",				&render_width);
	M_BindIntVariable("render_height",				&render_height);
	M_BindIntVariable("render_dimensions_mode",		&render_dimensions_mode);

	// TODO: Move these to R_BindRenderVariables now that it exists
    M_BindIntVariable("border_style",              &border_style);
    M_BindIntVariable("border_bezel_style",        &border_bezel_style);
}
