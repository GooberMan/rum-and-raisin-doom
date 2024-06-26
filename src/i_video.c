//
// Copyright(C) 1993-1996 Id Software, Inc.
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
// DESCRIPTION:
//	DOOM graphics stuff for SDL.
//


#include <stdlib.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <SDL2/SDL.h>

#include "m_dashboard.h"

#include "glad/glad.h"
#include <SDL2/SDL_opengl.h>

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
#include "i_terminal.h"
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

static const char *window_title = NULL;

// These are (1) the 320x200x8 paletted buffer that we draw to (i.e. the one
// that holds I_VideoBuffer), (2) the 320x200x32 RGBA intermediate buffer that
// we blit the former buffer to, (3) the intermediate 320x200 texture that we
// load the RGBA buffer to and that we render into another texture (4) which
// is upscaled by an integer factor UPSCALE using "nearest" scaling and which
// in turn is finally rendered to screen using "linear" scaling.

static SDL_Surface	*screenbuffer = NULL;
static SDL_Surface	*argbbuffer = NULL;
static SDL_Texture	*texture = NULL;
static SDL_Texture	*texture_upscaled = NULL;
static GLint		texture_upscaled_id = -1;

// The screen buffer; this is modified to draw things to the screen

pixel_t *I_VideoBuffer = NULL;
pixel_t* prev_video_buffer = NULL;

typedef struct renderbuffer_s
{
	vbuffer_t				screenbuffer;
	SDL_Rect				validregion;
} renderbuffer_t;

renderbuffer_t*		renderbuffers = NULL;
int32_t				renderbuffercount = 0;
int32_t				renderbufferswidth = 0;
int32_t				renderbuffersheight = 0;
int32_t				current_render_buffer = 0;
doombool				buffers_initialised = false;

ImGuiContext*		imgui_context = NULL;

static SDL_Rect blit_rect = {
    0,
    0,
    320,
    200
};

static uint32_t pixel_format;

// palette

static SDL_Color palette[256];
static doombool palette_to_set;

// display has been set up?

static doombool initialized = false;

// disable mouse?

static doombool nomouse = false;
int32_t usemouse = 1;

// Save screenshots in PNG format.

// 0 = original, 1 = INTERPIC
int32_t border_style = 1;

// 0 = original, 1 = dithered
int32_t border_bezel_style = 1; // Need to default to dithered until I sort out widescreen UI rendering

extern int32_t st_border_tile_style;

// Window position:

char *window_position = "center";

// SDL display number on which to run.

int32_t video_display = 0;
int32_t vsync_mode = VSync_Native;

// Screen width and height, from configuration file.

#define DEFAULT_WINDOW_WIDTH		1280
#define DEFAULT_WINDOW_HEIGHT		720
#define DEFAULT_RENDER_WIDTH		1280
#define DEFAULT_RENDER_HEIGHT		720
#define DEFAULT_RENDER_POSTSCALING	0
#define DEFAULT_FULLSCREEN			0
#define DEFAULT_RENDER_MATCH_WINDOW	1

int32_t window_width = DEFAULT_WINDOW_WIDTH;
int32_t window_height = DEFAULT_WINDOW_HEIGHT;

int32_t render_match_window = DEFAULT_RENDER_MATCH_WINDOW;
int32_t render_width = DEFAULT_RENDER_WIDTH;
int32_t render_height = DEFAULT_RENDER_HEIGHT;
int32_t render_post_scaling = DEFAULT_RENDER_POSTSCALING;
int32_t dynamic_resolution_scaling = 0;

int32_t queued_window_width = DEFAULT_WINDOW_WIDTH;
int32_t queued_window_height = DEFAULT_WINDOW_HEIGHT;
int32_t queued_fullscreen = DEFAULT_FULLSCREEN;
int32_t queued_render_width = DEFAULT_RENDER_WIDTH;
int32_t queued_render_height = DEFAULT_RENDER_HEIGHT;
int32_t queued_render_post_scaling = DEFAULT_RENDER_POSTSCALING;
int32_t queued_render_match_window = DEFAULT_RENDER_MATCH_WINDOW;
int32_t queued_num_render_buffers = 1;

// Fullscreen mode, 0x0 for SDL_WINDOW_FULLSCREEN_DESKTOP.

int32_t display_width = 0;
int32_t display_height = 0;

// Maximum number of pixels to use for intermediate scale buffer.

static int32_t max_scaling_buffer_pixels = 16000000;

// Run in full screen mode?  (int type for config code)

int32_t fullscreen = DEFAULT_FULLSCREEN;

// Aspect ratio correction mode
int32_t actualheight;

// Force integer scales for resolution-independent rendering

int32_t integer_scaling = false;

// VGA Porch palette change emulation

int32_t vga_porch_flash = false;

// Time to wait for the screen to settle on startup before starting the
// game (ms)

static int32_t startup_delay = 1000;

// Grab the mouse? (int type for config code). nograbmouse_override allows
// this to be temporarily disabled via the command line.

int32_t grabmouse = true;
static doombool nograbmouse_override = false;

// If true, game is running as a screensaver

doombool screensaver_mode = false;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen

doombool screenvisible = true;

// If true, we display dots at the bottom of the screen to 
// indicate FPS.

static doombool display_fps_dots;

// If this is true, the screen is rendered but not blitted to the
// video buffer.

static doombool noblit;

// Callback function to invoke to determine whether to grab the 
// mouse pointer.

static grabmouse_callback_t grabmouse_callback = NULL;

// Does the window currently have focus?

static doombool window_focused = true;

// Window resize state.

static doombool need_resize = false;
static uint64_t last_resize_time = 0;
#define RESIZE_DELAY 500ull

// Gamma correction level to use

int32_t usegamma = 0;

// Joystick/gamepad hysteresis
uint64_t joywait = 0;

extern int32_t dashboardactive;

void I_SetupOpenGL( void );
void I_SetupGLAD( void );
void I_VideoSetupGLRenderPath( void );
void I_VideoResetGLFrameBuffer( void );
void I_VideoUpdateGLPalette( void* data );
void I_VideoRenderGLIntermediate( void* data );
int32_t I_VideoGetGLIntermediate( void );
void I_VideoRenderGLBackbuffer( void );

void I_VideoSetupSDLRenderPath( void );

static doombool MouseShouldBeGrabbed()
{
	// never grab the mouse when in screensaver mode
   
	if (screensaver_mode)
		return false;

	// if the window doesn't have focus, never grab it

	if (!window_focused)
		return false;

	// Debug menu needs mouse. Ergo, don't grab mouse.
	if( dashboardactive )
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

void I_DisplayFPSDots(doombool dots_on)
{
    display_fps_dots = dots_on;
}

static void SetShowCursor(doombool show)
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
            last_resize_time = I_GetTimeMS();
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
            if (i >= 0 && i != video_display)
            {
                video_display = i;
                SDL_DisplayMode desktopinfo;
                SDL_GetDesktopDisplayMode( video_display, &desktopinfo );
                display_width = desktopinfo.w;
                display_height = desktopinfo.h;
            }
            break;

        default:
            break;
    }
}

static doombool ToggleFullScreenKeyShortcut(SDL_Keysym *sym)
{
    Uint16 flags = (KMOD_LALT | KMOD_RALT);
#if defined(__MACOSX__)
    flags |= (KMOD_LGUI | KMOD_RGUI);
#endif
    return (sym->scancode == SDL_SCANCODE_RETURN || 
            sym->scancode == SDL_SCANCODE_KP_ENTER) && (sym->mod & flags) != 0;
}

#if defined( WIN32 )
#define FULLSCREEN_BORDERLESS 1
#else
#define FULLSCREEN_BORDERLESS 0
#endif

void I_PerformFullscreen(void)
{
    fullscreen = queued_fullscreen;

#if FULLSCREEN_BORDERLESS
    if (fullscreen)
    {
        // We should be toggling the SDL_WINDOW_POPUP_MENU flag here to get around GL drivers.
        // But since SDL_SetWindowFlags isn't exposed to us, well, it's the old +1 hack
        #define GL_FULLSCREENWINDOWHACK 1
        SDL_SetWindowBordered( screen, false );
        SDL_SetWindowSize(screen, display_width + GL_FULLSCREENWINDOWHACK, display_height + GL_FULLSCREENWINDOWHACK);
        SDL_SetWindowPosition(screen, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    else
    {
        SDL_SetWindowSize(screen, window_width, window_height);
        SDL_SetWindowBordered( screen, true );
        SDL_SetWindowPosition(screen, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
#else // !FULLSCREEN_BORDERLESS
	Uint32 flags = 0;

    if (fullscreen)
    {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    SDL_SetWindowFullscreen(screen, flags);

    if (!fullscreen)
    {
        SDL_SetWindowSize(screen, window_width, window_height);
        SDL_SetWindowPosition(screen, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
#endif // FULLSCREEN_BORDERLESS
}

void I_ToggleFullScreen(void)
{
	queued_fullscreen = !fullscreen;

	if( render_match_window )
	{
		queued_render_width = ( queued_fullscreen ? display_width : window_width );
		queued_render_height = ( queued_fullscreen ? display_height : window_height ) / ( render_post_scaling ? 1.2 : 1.0 );
	}
}

void I_SetWindowDimensions( int32_t w, int32_t h )
{
	queued_window_width = w;
	queued_window_height = h;
	if( render_match_window )
	{
		queued_render_width = w;
		queued_render_height = h;
		queued_render_post_scaling = 0;
	}
}

void I_SetRenderDimensions( int32_t w, int32_t h, int32_t s )
{
	queued_render_width = w;
	queued_render_height = h;
	queued_render_post_scaling = s;
	queued_render_match_window = false;
}

void I_SetRenderMatchWindow( void )
{
	queued_render_width = ( fullscreen ? display_width : window_width );
	queued_render_height = ( fullscreen ? display_height : window_height );
	queued_render_post_scaling = 0;
	queued_render_match_window = true;
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

	I_ErrorUpdate();

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


#define MOUSE_MOVE_TO_BOTTOMRIGHT 0

void I_UpdateMouseGrab( void )
{
    static doombool currently_grabbed = false;
    doombool grab;
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
		I_TerminalPrintf( Log_Startup,	"CreateUpscaledTexture: Limited texture size to %dx%d "
										"(max %d pixels, max texture size %dx%d)\n",
										*w_upscale * render_width, *h_upscale * render_height,
										max_scaling_buffer_pixels,
										rinfo.max_texture_width, rinfo.max_texture_height);
    }
}

static void CreateUpscaledTexture( )
{
    int w, h;
    int h_upscale, w_upscale;

    SDL_Texture *new_texture;

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

    new_texture = SDL_CreateTexture(renderer,
                                pixel_format,
                                SDL_TEXTUREACCESS_TARGET,
                                h_upscale*render_height,
                                w_upscale*render_width);

	if( texture_upscaled != NULL)
	{
		SDL_DestroyTexture( texture_upscaled );
	}

	texture_upscaled = new_texture;

	SDL_GL_BindTexture( texture_upscaled, NULL, NULL );
	glGetIntegerv( GL_TEXTURE_BINDING_2D, &texture_upscaled_id );
	SDL_GL_UnbindTexture( texture_upscaled );

}

#define FPS_DOTS_SUPPORTED 0

//
// I_FinishUpdate
//

void I_FinishUpdate( vbuffer_t* activebuffer )
{
#if FPS_DOTS_SUPPORTED
	static uint32_t lasttic;
	uint32_t tics;
	int32_t i;
#endif // FPS_DOTS_SUPPORTED

	SDL_Rect Target;

	renderbuffer_t* curr = NULL;
	if( activebuffer == NULL )
	{
		curr = &renderbuffers[ current_render_buffer ];
		activebuffer = &curr->screenbuffer;
	}
	else
	{
		for( int32_t ind = 0; ind < renderbuffercount; ++ind )
		{
			if( activebuffer == &renderbuffers[ ind ].screenbuffer )
			{
				curr = &renderbuffers[ ind ];
			}
		}
	}

    if (!initialized)
        return;

    if (noblit)
        return;

    if (need_resize)
    {
        if (I_GetTimeMS() > last_resize_time + RESIZE_DELAY)
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
				SDL_SetWindowPosition(screen, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
				queued_window_width = window_width;
				queued_window_height = window_height;
				if( render_match_window )
				{
					queued_render_width =		( fullscreen ? display_width : window_width );
					queued_render_height =		( fullscreen ? display_height : window_height ) / ( render_post_scaling ? 1.2 : 1.0 );
				}
            }
            CreateUpscaledTexture();
            need_resize = false;
            palette_to_set = true;
        }
        else
        {
            return;
        }
    }

    I_UpdateMouseGrab();

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

	int32_t actualwindowwidth = 0;
	int32_t actualwindowheight = 0;

	SDL_GetWindowSize( screen, &actualwindowwidth, &actualwindowheight );

	int32_t render_path = 1;

	SDL_RenderSetLogicalSize( renderer, actualwindowwidth, actualwindowheight );

	if( render_path == 0 )
	{
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

		if ( palette_to_set && vga_porch_flash )
		{
			// "flash" the pillars/letterboxes with palette changes, emulating
			// VGA "porch" behaviour (GitHub issue #832)
			SDL_SetRenderDrawColor(renderer, palette[0].r, palette[0].g,
				palette[0].b, SDL_ALPHA_OPAQUE);
		}
	
		if (palette_to_set)
		{
			SDL_SetPaletteColors( curr->screenbuffer.data8bithandle->format->palette, palette, 0, 256 );
		}

		// Blit from the paletted 8-bit screen buffer to the intermediate
		// 32-bit RGBA buffer that we can load into the texture.

		if( curr->validregion.h > 0 )
		{
			SDL_LowerBlit( curr->screenbuffer.data8bithandle, &curr->validregion, curr->screenbuffer.dataARGBhandle, &curr->validregion );
		}

		palette_to_set = false;

		// Update the intermediate texture with the contents of the RGBA buffer.

		SDL_UpdateTexture( activebuffer->datatexture, NULL, activebuffer->dataARGBhandle->pixels, activebuffer->dataARGBhandle->pitch );

		// Make sure the pillarboxes are kept clear each frame.

		SDL_RenderClear(renderer);

		// Render this intermediate texture into the upscaled texture
		// using "nearest" integer scaling.

		SDL_SetRenderTarget( renderer, texture_upscaled );
		SDL_RenderCopy(renderer, activebuffer->datatexture, NULL, NULL);

		// Finally, render this upscaled texture to screen using linear scaling.

		SDL_SetRenderTarget(renderer, NULL);

		if( !dashboardactive )
		{
			if( activebuffer->mode == VB_Transposed )
			{
				int32_t activewidth = activebuffer->height;
				int32_t activeheight = activebuffer->width * activebuffer->verticalscale;

				// Better transormation courtesy of Altazimuth
				Target.x = (activewidth - activeheight) / 2;
				Target.y = (activeheight - activewidth) / 2;
				Target.w = activeheight;
				Target.h = activewidth;

				SDL_RenderCopyEx( renderer, texture_upscaled, NULL, &Target, 90.0, NULL, SDL_FLIP_VERTICAL );
			}
			else
			{
				SDL_RenderCopy( renderer, texture_upscaled, NULL, NULL );
			}

		}

		M_RenderDashboard( actualwindowwidth, actualwindowheight, texture_upscaled_id );

	}
	else
	{
		if( palette_to_set )
		{
			I_VideoUpdateGLPalette( (void*)palette );
		}

		I_VideoRenderGLIntermediate( curr->screenbuffer.data );

		if( !dashboardactive )
		{
			I_VideoRenderGLBackbuffer();
		}

		GLuint fonttexture = (GLuint)igGetIO()->Fonts->TexID;
		glBindTexture( GL_TEXTURE_2D, fonttexture );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glBindTexture( GL_TEXTURE_2D, 0 );

		M_RenderDashboard( actualwindowwidth, actualwindowheight, I_VideoGetGLIntermediate() );
	}

	SDL_RenderPresent(renderer);

	if( ++current_render_buffer >= renderbuffercount )
	{
		current_render_buffer = 0;
	}

	screenbuffer = renderbuffers[ current_render_buffer ].screenbuffer.data8bithandle;
	prev_video_buffer = I_VideoBuffer;
	I_VideoBuffer = screenbuffer->pixels;
	V_RestoreBuffer();
}


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
	extern int32_t remove_limits;
	int32_t entry;

	// Zero out the bottom two bits of each channel - the PC VGA
	// controller only supports 6 bits of accuracy.
	Uint8 bottommask = ~( remove_limits ? 0 : 0x3 );

	for( entry = 0; entry < 256; ++entry )
	{
		palette[ entry ].r = gammatable[ usegamma ][ *doompalette++ ] & bottommask;
		palette[ entry ].g = gammatable[ usegamma ][ *doompalette++ ] & bottommask;
		palette[ entry ].b = gammatable[ usegamma ][ *doompalette++ ] & bottommask;
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
	if( screen != NULL )
	{
		I_InitWindowTitle();
	}
}

//
// Call the SDL function to set the window title, based on 
// the title set with I_SetWindowTitle.
//

void I_InitWindowTitle(void)
{
    char *buf;

	if( window_title != NULL )
	{
		buf = M_StringJoin(window_title, " - ", EDITION_STRING, NULL);
		SDL_SetWindowTitle(screen, buf);
		free(buf);
	}
	else
	{
		SDL_SetWindowTitle( screen, EDITION_STRING );
	}
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
        SDL_DisplayMode desktopinfo;
        SDL_GetDesktopDisplayMode( video_display, &desktopinfo );
        display_width = desktopinfo.w;
        display_height = desktopinfo.h;
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


#if USE_IMGUI
const char* I_VideoGetGLSLVersion();

static void I_SetupDearImGui(void)
{
	int32_t retval;

	imgui_context = igCreateContext( NULL );
	imgui_context->IO.IniFilename = M_StringJoin( configdir, "dashboard.ini", NULL );
	//imgui_context->IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	igGetStyle()->AntiAliasedLines = false;
	igGetStyle()->AntiAliasedFill = false;

	retval = CImGui_ImplSDL2_InitForOpenGL( screen, glcontext );
	if ( !retval )
	{
		I_Error( "ImGui_ImplSDL2_InitForOpenGL failed to initialise" );
	}

	retval = CImGui_ImplOpenGL3_Init( I_VideoGetGLSLVersion() );
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

    if (SDL_GetCurrentDisplayMode(video_display, &mode) != 0)
    {
        I_Error("Could not get display mode for video display #%d: %s",
        video_display, SDL_GetError());
    }
	
	display_width = mode.w;
	display_height = mode.h;

	if( render_match_window )
	{
		queued_render_width  = render_width =		( fullscreen ? display_width : window_width );
		queued_render_height = render_height =		( fullscreen ? display_height : window_height ) / ( render_post_scaling ? 1.2 : 1.0 );
	}

    w = window_width;
    h = window_height;

    // In windowed mode, the window can be resized while the game is
    // running.
    window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;

    // Set the highdpi flag - this makes a big difference on Macs with
    // retina displays, especially when using small window sizes.
    window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;

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

        if( fullscreen )
        {
            queued_fullscreen = fullscreen;
            I_PerformFullscreen();
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

	I_VideoSetupGLRenderPath();

	CreateUpscaledTexture();

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

	if( texture != NULL )
	{
		SDL_DestroyTexture( texture );
		texture = NULL;
	}

	if( argbbuffer != NULL )
	{
		SDL_FreeSurface( argbbuffer );
		argbbuffer = NULL;
	}

	if( renderbuffers != NULL )
	{
		while( curr != end )
		{
			SDL_FreeSurface( curr->screenbuffer.data8bithandle );
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

		// Format of argbbuffer must match the screen pixel format because we
		// import the surface data into the texture.
		argbbuffer = SDL_CreateRGBSurface( 0, height, width, bpp, rmask, gmask, bmask, amask );
		SDL_FillRect( argbbuffer, NULL, 0 );

		// Create the intermediate texture that the RGBA surface gets loaded into.
		// The SDL_TEXTUREACCESS_STREAMING flag means that this texture's content
		// is going to change frequently.

		texture = SDL_CreateTexture(renderer,
									pixel_format,
									SDL_TEXTUREACCESS_STREAMING,
									render_height, render_width);

		renderbuffers = curr = Z_Malloc( sizeof( renderbuffer_t ) * numbuffers, PU_STATIC, NULL );
		renderbuffercount = numbuffers;
		end = curr + numbuffers;

		while( curr != end )
		{
			// Transposed aw yiss
			curr->screenbuffer.data8bithandle = SDL_CreateRGBSurface( 0, height, width, 8, 0, 0, 0, 0 );
			SDL_FillRect( curr->screenbuffer.data8bithandle, NULL, 0 );

			curr->screenbuffer.dataARGBhandle = argbbuffer;
			curr->screenbuffer.datatexture = texture;
			curr->screenbuffer.data = curr->screenbuffer.data8bithandle->pixels;
			curr->screenbuffer.user = NULL;
			curr->screenbuffer.width = height;
			curr->screenbuffer.height = width;
			// I BROKE PITCH SOMEWHERE IN THE GL RENDERPATH
			curr->screenbuffer.pitch = height; //curr->screenbuffer.data8bithandle->pitch;
			curr->screenbuffer.pixel_size_bytes = 1;
			curr->screenbuffer.mode = VB_Transposed;
			curr->screenbuffer.verticalscale = 1.2f;
			curr->screenbuffer.magic_value = vbuffer_magic;

			SDL_SetPaletteColors( curr->screenbuffer.data8bithandle->format->palette, palette, 0, 256 );

			curr->validregion = region;

			// Clear the buffer to black.
			memset( curr->screenbuffer.data8bithandle->pixels, 0, render_width * render_height * sizeof( pixel_t ) );
			++curr;
		}

		current_render_buffer = 0;
		screenbuffer = renderbuffers[ current_render_buffer ].screenbuffer.data8bithandle;

		// REMOVE THIS ANCIENT CODE ALREADY JEEBUS

		// The actual 320x200 canvas that we draw to. This is the pixel buffer of
		// the 8-bit paletted screen buffer that gets blit on an intermediate
		// 32-bit RGBA screen buffer that gets loaded into a texture that gets
		// finally rendered into our window or full screen in I_FinishUpdate().

		prev_video_buffer = renderbuffers[ renderbuffercount - 1 ].screenbuffer.data8bithandle->pixels;
		I_VideoBuffer = screenbuffer->pixels;
		V_RestoreBuffer();
	}

}

void I_SetNumBuffers( int32_t count )
{
	queued_num_render_buffers = count;
}

vbuffer_t* I_GetRenderBuffer( int32_t index )
{
	return &renderbuffers[ index ].screenbuffer;
}

vbuffer_t* I_GetCurrentRenderBuffer( void )
{
	return &renderbuffers[ current_render_buffer ].screenbuffer;
}

void I_SetRenderBufferValidColumns( int32_t index, int32_t begin, int32_t end )
{
	renderbuffers[ index ].validregion.y = begin;
	renderbuffers[ index ].validregion.h = end - begin;
}

SDL_Window* I_GetWindow( void )
{
	return screen;
}

SDL_Renderer* I_GetRenderer( void )
{
	return renderer;
}

int64_t I_GetRefreshRate( void )
{
	SDL_DisplayMode current;
	SDL_GetCurrentDisplayMode( video_display, &current );

	return (int64_t)current.refresh_rate;
}

void I_InitGraphics( void )
{
	char *env;
	SDL_Event dummy;

	queued_window_width = window_width;
	queued_window_height = window_height;
	queued_fullscreen = fullscreen;
	queued_render_width  = render_width;
	queued_render_height = render_height;
	queued_render_post_scaling = render_post_scaling;
	queued_render_match_window = render_match_window;

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

	if( render_post_scaling )
	{
		actualheight = (int32_t)( render_height * 1.2 );
	}
	else
	{
		actualheight = render_height;
	}

    // Create the game window; this may switch graphic modes depending
    // on configuration.
    SetVideoMode();

	// Important: Set the "logical size" of the rendering context. At the same
	// time this also defines the aspect ratio that is preserved while scaling
	// and stretching the texture into the window.
	SDL_RenderSetLogicalSize( renderer, render_width, actualheight );

	// Set the scaling quality for rendering the intermediate texture into
	// the upscaled texture to "nearest", which is gritty and pixelated and
	// resembles software scaling pretty well.
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    // SDL2-TODO UpdateFocus();
    I_UpdateMouseGrab();

    // On some systems, it takes a second or so for the screen to settle
    // after changing modes.  We include the option to add a delay when
    // setting the screen mode, so that the game doesn't start immediately
    // with the player unable to see anything.

    if (fullscreen && !screensaver_mode)
    {
        SDL_Delay(startup_delay);
    }

    // clear out any events waiting at the start and center the mouse
  
    while ( SDL_PollEvent( &dummy ) )
	{
#if USE_IMGUI
		CImGui_ImplSDL2_ProcessEvent( &dummy );
#endif // USE_IMGUI
	}

	// Call I_ShutdownGraphics on quit
    I_AtExit(I_ShutdownGraphics, true);

    initialized = true;
}

void I_InitBuffers( int32_t numbuffers )
{
    byte *doompal;
    // Set the palette

    doompal = W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE);
    I_SetPalette(doompal);

	queued_num_render_buffers = numbuffers;
	queued_render_width  = render_width;
	queued_render_height = render_height;
	queued_render_post_scaling = render_post_scaling;
	queued_render_match_window = render_match_window;
	I_RefreshRenderBuffers( numbuffers, render_width, render_height );

	buffers_initialised = true;
}

//
// I_StartFrame
//
void I_StartFrame (void)
{
	if( buffers_initialised )
	{
		if( queued_render_post_scaling != render_post_scaling )
		{
			render_post_scaling = queued_render_post_scaling;

			if ( render_post_scaling == 1 )
			{
				actualheight = (int32_t)( render_height * 1.2 );
			}
			else
			{
				actualheight = render_height;
			}
		}

		if( queued_render_width != render_width
			|| queued_render_height != render_height
			|| queued_num_render_buffers != renderbuffercount
			|| queued_render_match_window != render_match_window )
		{
			render_height = blit_rect.w = queued_render_height;
			render_width  = blit_rect.h = queued_render_width;
			render_match_window = queued_render_match_window;

			if ( render_post_scaling == 1 )
			{
				actualheight = (int32_t)( render_height * 1.2 );
			}
			else
			{
				actualheight = render_height;
			}

			I_VideoResetGLFrameBuffer();

			I_RefreshRenderBuffers( queued_num_render_buffers, queued_render_width, queued_render_height );

			CreateUpscaledTexture();
		}
	}

	if( queued_window_width != window_width
		|| queued_window_height != window_height )
	{
		if( !fullscreen )
		{
			SDL_SetWindowSize( screen, queued_window_width, queued_window_height );
			SDL_SetWindowPosition(screen, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
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
    M_BindIntVariable("integer_scaling",           &integer_scaling);
    M_BindIntVariable("vga_porch_flash",           &vga_porch_flash);
    M_BindIntVariable("startup_delay",             &startup_delay);
    M_BindIntVariable("max_scaling_buffer_pixels", &max_scaling_buffer_pixels);
    M_BindIntVariable("window_width",              &window_width);
    M_BindIntVariable("window_height",             &window_height);
    M_BindIntVariable("grabmouse",                 &grabmouse);
    M_BindStringVariable("window_position",        &window_position);
    M_BindIntVariable("usegamma",                  &usegamma);

	M_BindIntVariable("render_width",				&render_width);
	M_BindIntVariable("render_height",				&render_height);
    M_BindIntVariable("render_post_scaling",		&render_post_scaling);
	M_BindIntVariable("dynamic_resolution_scaling",	&dynamic_resolution_scaling);
	M_BindIntVariable("render_match_window",		&render_match_window);
	M_BindIntVariable("vsync_mode",					&vsync_mode);

	// TODO: Move these to R_BindRenderVariables now that it exists
    M_BindIntVariable("border_style",              &border_style);
    M_BindIntVariable("border_bezel_style",        &border_bezel_style);
	M_BindIntVariable("st_border_tile_style",		&st_border_tile_style );
}
