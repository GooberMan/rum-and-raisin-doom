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
//	OpenGL specific implementations of i_video routines.
//

#include "i_video.h"
#include "m_container.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif // _WIN32

#include "SDL.h"
#include "glad/glad.h"
#include "SDL_opengl.h"

extern "C"
{
#ifdef _WIN32
	// Known hacks to force usage of best GPUs available. Handy when integrated
	// GPUs are the system default and you really want to use the high powered GPU.

	// https://docs.nvidia.com/gameworks/content/technologies/desktop/optimus.htm
	_declspec(dllexport) DWORD NvOptimusEnablement = 1;

	// https://gpuopen.com/learn/amdpowerxpressrequesthighperformance/
	_declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif // _WIN32

	void I_Error( const char *error, ... );

	extern int32_t render_width;
	extern int32_t render_height;
	extern int32_t render_post_scaling;
	extern int32_t window_width;
	extern int32_t window_height;
	extern int32_t display_width;
	extern int32_t display_height;
	extern int32_t frame_width;
	extern int32_t frame_height;
	extern int32_t fullscreen;
	extern int32_t video_display;
	extern int32_t vsync_mode;
}

typedef struct vsyncsupport_s
{
	bool		supported;
	int32_t		rate;
	int32_t		interval;

	INLINE void SetSupported( int32_t i )		{ supported = true; interval = i; }
} vsyncsupport_t;

static vsyncsupport_t vsync_modes[ VSync_Max ] =
{
	{ true,		0,		0	},		// VSync_Off
	{ false,	-1,		-1	},		// VSync_Adaptive
	{ true,		-1,		1	},		// VSync_Native
	{ false,	35,		0	},		// VSync_36Hz
	{ false,	36,		0	},		// VSync_35Hz
	{ false,	40,		0	},		// VSync_40Hz
	{ false,	45,		0	},		// VSync_45Hz
	{ false,	50,		0	},		// VSync_50Hz
	{ false,	60,		0	},		// VSync_60Hz
	{ false,	70,		0	},		// VSync_70Hz
	{ false,	72,		0	},		// VSync_72Hz
	{ false,	75,		0	},		// VSync_75Hz
	{ false,	90,		0	},		// VSync_90Hz
	{ false,	100,	0	},		// VSync_100Hz
	{ false,	120,	0	},		// VSync_120Hz
	{ false,	140,	0	},		// VSync_140Hz
	{ false,	144,	0	},		// VSync_144Hz
	{ false,	150,	0	},		// VSync_150Hz
	{ false,	180,	0	},		// VSync_180Hz
	{ false,	200,	0	},		// VSync_200Hz
	{ false,	240,	0	},		// VSync_240Hz
	{ false,	280,	0	},		// VSync_280Hz
	{ false,	288,	0	},		// VSync_288Hz
	{ false,	300,	0	},		// VSync_300Hz
	{ false,	360,	0	},		// VSync_360Hz
};

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

static const char* basic_vertex_shader =
	"precision highp float;\n"
	"\n"
	"in vec3 position;\n"
	"in vec2 uv;\n"
	"\n"
	"out vec2 curruv;\n"
	"\n"
	"void main()\n"
	"{\n"
	"	gl_Position = vec4( position, 1.0 );\n"
	"	curruv = uv;\n"
	"}\n"
	"\n";

static const char* palettedecompress_fragment_shader =
	"precision highp float;\n"
	"\n"
	"uniform sampler2D	backbuffer;\n"
	"uniform sampler2D	palette;\n"
	"\n"
	"in vec2 curruv;\n"
	"\n"
	"out vec3 colour;\n"
	"\n"
	"void main()\n"
	"{\n"
	"	vec2 palettesample = vec2( texture( backbuffer, curruv ).x * 0.99609375 + 0.0009765625, 0.5 );\n"
	"	colour = texture( palette, palettesample ).xyz;\n"
	"}\n"
	"\n";

static const char* basic_fragment_shader =
	"precision highp float;\n"
	"\n"
	"uniform sampler2D	diffuse;\n"
	"uniform vec2		texturescale;\n"
	"\n"
	"in vec2 curruv;\n"
	"\n"
	"out vec3 colour;\n"
	"\n"
	"void main()\n"
	"{\n"
	"	colour = texture( diffuse, curruv * texturescale ).xyz;\n"
	"}\n"
	"\n";

DOOM_C_API const char* I_VideoGetGLSLVersion()
{
	return GLSL_VERSION;
}

DOOM_C_API void I_SetupOpenGL( void )
{
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS,			GL_CONTEXTFLAGS );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK,	GL_CONTEXTPROFILE );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION,	GL_VERSION_MAJOR );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION,	GL_VERSION_MINOR );

	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER,			1 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE,				24 );
	SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE,			8 );
}

DOOM_C_API void I_SetupGLAD( void )
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

template< typename _elem >
struct RawVector2
{
	using element_type = _elem;

	element_type	x;
	element_type	y;
};

template< typename _elem >
struct RawVector3
{
	using element_type = _elem;

	element_type	x;
	element_type	y;
	element_type	z;
};

template< typename _elem >
struct RawVector4
{
	using element_type = _elem;

	element_type	x;
	element_type	y;
	element_type	z;
	element_type	w;
};

// Brace initialisers for discrete elements do not work in MSVC
// on std::array constexpr when using structs, so for readability
// we have a MakeVec function
template< size_t num > struct rawvecselect { };
template<> struct rawvecselect< 2 > { using type = RawVector2< float_t >; };
template<> struct rawvecselect< 3 > { using type = RawVector3< float_t >; };
template<> struct rawvecselect< 4 > { using type = RawVector4< float_t >; };
template< size_t num > using rawvecselect_t = typename rawvecselect< num >::type;

template< typename... _args >
constexpr auto MakeRawVec( _args... args )
{
	rawvecselect_t< sizeof...( args ) > val = { args... };
	return val;
}

struct BackbufferVertex
{
	RawVector3< float >		pos;
	RawVector2< float >		uv;

	static constexpr size_t PosOffset() noexcept;
	static constexpr size_t UVOffset() noexcept;
};

constexpr size_t BackbufferVertex::PosOffset() noexcept		{ return offsetof( BackbufferVertex, pos ); }
constexpr size_t BackbufferVertex::UVOffset() noexcept		{ return offsetof( BackbufferVertex, uv ); }

static constexpr std::array< BackbufferVertex, 4 > NormalQuadVertices =
{
	MakeRawVec( -1.0f, -1.0f, 0.0f ),	MakeRawVec( 0.0f, 0.0f ),
	MakeRawVec( -1.0f,  1.0f, 0.0f ),	MakeRawVec( 0.0f, 1.0f ),
	MakeRawVec(  1.0f, -1.0f, 0.0f ),	MakeRawVec( 1.0f, 0.0f ),
	MakeRawVec(  1.0f,  1.0f, 0.0f ),	MakeRawVec( 1.0f, 1.0f ),
};

static constexpr std::array< BackbufferVertex, 4 > TransposedQuadVertices =
{
	MakeRawVec( -1.0f, -1.0f, 0.0f ),	MakeRawVec( 1.0f, 0.0f ),
	MakeRawVec( -1.0f,  1.0f, 0.0f ),	MakeRawVec( 0.0f, 0.0f ),
	MakeRawVec(  1.0f, -1.0f, 0.0f ),	MakeRawVec( 1.0f, 1.0f ),
	MakeRawVec(  1.0f,  1.0f, 0.0f ),	MakeRawVec( 0.0f, 1.0f ),
};

struct VertexBuffer
{
	GLuint vbo;
	GLuint vba;
};

static VertexBuffer normal_quad			= { 0, 0 };
static VertexBuffer transposed_quad		= { 0, 0 };

template< typename _vert, size_t len >
static void GenerateBuffer( VertexBuffer& output, const std::array< _vert, len >& vertices )
{
	glGenVertexArrays( 1, &output.vba );
	glGenBuffers( 1, &output.vbo );
	glBindVertexArray( output.vba );
	glBindBuffer( GL_ARRAY_BUFFER, output.vbo );
	glBufferData( GL_ARRAY_BUFFER, len * sizeof( _vert ), vertices.data(), GL_STATIC_DRAW );
	glEnableVertexAttribArray( 0 );
	glEnableVertexAttribArray( 1 );
	glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sizeof( _vert ), (void*)_vert::PosOffset() );
	glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, sizeof( _vert ), (void*)_vert::UVOffset() );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	glBindVertexArray( 0 );
}

DOOM_C_API typedef struct hwtexture_s
{
	GLuint	to;
	GLint	internalformat;
	GLenum	format;
	int32_t	width;
	int32_t	height;
} hwtexture_t;

struct FrameBuffer
{
	GLuint		fbo;
	hwtexture_t	rgb;
};

static hwtexture_t	doom_backbuffer;
static hwtexture_t	doom_palette;
static FrameBuffer	doom_rgbexpanded;

static void UpdateTexture( hwtexture_t& tex, void* data )
{
	glBindTexture( GL_TEXTURE_2D, tex.to );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ROW_LENGTH, tex.width );
	glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, tex.width, tex.height, tex.format, GL_UNSIGNED_BYTE, data );
	glBindTexture( GL_TEXTURE_2D, 0 );
}

static void DestroyTexture( hwtexture_t& tex )
{
	glDeleteTextures( 1, &tex.to );
	memset( &tex, 0, sizeof( hwtexture_t ) );
}

static void GenerateTexture( hwtexture_t& output, GLint internalformat, GLenum format, int32_t width, int32_t height, void* data = nullptr )
{
	glGenTextures( 1, &output.to );
	output.internalformat = internalformat;
	output.format = format;
	output.width = width;
	output.height = height;
	glBindTexture( GL_TEXTURE_2D, output.to );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexImage2D( GL_TEXTURE_2D, 0, internalformat, width, height, 0, format, GL_UNSIGNED_BYTE, data );
	glBindTexture( GL_TEXTURE_2D, 0 );
}

static void GenerateFrameBuffer( FrameBuffer& output, int32_t width, int32_t height )
{
	glGenFramebuffers( 1, &output.fbo );
	glBindFramebuffer( GL_FRAMEBUFFER, output.fbo );
	GenerateTexture( output.rgb, GL_RGB8, GL_RGB, width, height );
//	glBindTexture( GL_TEXTURE_2D, output.rgb.to ); // I don't think we need this
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output.rgb.to, 0 );
	GLenum drawbuffers = GL_COLOR_ATTACHMENT0;
	glDrawBuffers( 1, &drawbuffers );

	if( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
	{
		I_Error( "Frame buffer did not finish creating" );
	}

	glBindTexture( GL_TEXTURE_2D, 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
}

struct ShaderProgram
{
	GLuint	vsh;
	GLuint	fsh;
	GLuint	program;

	GLint	poslocation;
	GLint	uvlocation;

	GLint	texture1location;
	GLint	texture2location;
	GLint	texturescalelocation;
};

static ShaderProgram translate_backbuffer;
static ShaderProgram copy_backbuffer;

GLuint CreateShader( GLenum type, const char* source )
{
	GLuint output = glCreateShader( type );

	std::string combinedshader = GLSL_VERSION;
	combinedshader += "\n\n";
	combinedshader += source;
	const char* combinedshadercstr = combinedshader.c_str();

	glShaderSource( output, 1, &combinedshadercstr, NULL );
	glCompileShader( output );

	GLint result;
	glGetShaderiv( output, GL_COMPILE_STATUS, &result );
	if( !result )
	{
		char log[ 1024 ] = { 0 };
		glGetShaderInfoLog( output, 1024, NULL, log );
		I_Error( "Shader did not compile\n\n%s\n\n%s", GLSL_VERSION, log );
	}

	return output;
}

void GenerateProgram( ShaderProgram& output, const char* vertex, const char* fragment, const char* texture1uniform, const char* texture2uniform )
{
	output.vsh = CreateShader( GL_VERTEX_SHADER, vertex );
	output.fsh = CreateShader( GL_FRAGMENT_SHADER, fragment );

	output.program = glCreateProgram();

	glAttachShader( output.program, output.vsh );
	glAttachShader( output.program, output.fsh );
	glLinkProgram( output.program );

	GLint result;
	glGetProgramiv( output.program, GL_LINK_STATUS, &result );
	if( !result )
	{
		char log[ 1024 ] = { 0 };
		glGetProgramInfoLog( output.program, 1024, NULL, log );
		I_Error( "Program did not link\n\n%s", log );
	}

	output.poslocation = glGetAttribLocation( output.program, "position" );
	output.uvlocation = glGetAttribLocation( output.program, "uv" );

	output.texture1location = glGetUniformLocation( output.program, texture1uniform );
	output.texture2location = texture2uniform ? glGetUniformLocation( output.program, texture2uniform ) : -1;
	output.texturescalelocation = glGetUniformLocation( output.program, "texturescale" );
}

void SetupVSync()
{
	SDL_DisplayMode mode;
	SDL_GetDesktopDisplayMode( video_display, &mode );

	vsync_modes[ VSync_Native ].rate = mode.refresh_rate;

	switch( mode.refresh_rate )
	{
	case 50:
		vsync_modes[ VSync_50Hz ].SetSupported( 1 );
		break;
	case 60:
		vsync_modes[ VSync_60Hz ].SetSupported( 1 );
		break;
	case 70:
		vsync_modes[ VSync_70Hz ].SetSupported( 1 );
		vsync_modes[ VSync_35Hz ].SetSupported( 2 );
		break;
	case 72:
		vsync_modes[ VSync_72Hz ].SetSupported( 1 );
		vsync_modes[ VSync_36Hz ].SetSupported( 2 );
		break;
	case 75:
		vsync_modes[ VSync_75Hz ].SetSupported( 1 );
		break;
	case 90:
		vsync_modes[ VSync_90Hz ].SetSupported( 1 );
		vsync_modes[ VSync_45Hz ].SetSupported( 2 );
		break;
	case 100:
		vsync_modes[ VSync_100Hz ].SetSupported( 1 );
		vsync_modes[ VSync_50Hz ].SetSupported( 2 );
		break;
	case 120:
		vsync_modes[ VSync_120Hz ].SetSupported( 1 );
		vsync_modes[ VSync_60Hz ].SetSupported( 2 );
		vsync_modes[ VSync_40Hz ].SetSupported( 3 );
		break;
	case 140:
		vsync_modes[ VSync_140Hz ].SetSupported( 1 );
		vsync_modes[ VSync_70Hz ].SetSupported( 2 );
		vsync_modes[ VSync_35Hz ].SetSupported( 4 );
		break;
	case 144:
		vsync_modes[ VSync_144Hz ].SetSupported( 1 );
		vsync_modes[ VSync_72Hz ].SetSupported( 2 );
		vsync_modes[ VSync_36Hz ].SetSupported( 4 );
		break;
	case 150:
		vsync_modes[ VSync_150Hz ].SetSupported( 1 );
		vsync_modes[ VSync_75Hz ].SetSupported( 2 );
		vsync_modes[ VSync_50Hz ].SetSupported( 3 );
		break;
	case 180:
		vsync_modes[ VSync_180Hz ].SetSupported( 1 );
		vsync_modes[ VSync_90Hz ].SetSupported( 2 );
		vsync_modes[ VSync_60Hz ].SetSupported( 3 );
		vsync_modes[ VSync_45Hz ].SetSupported( 4 );
		break;
	case 200:
		vsync_modes[ VSync_200Hz ].SetSupported( 1 );
		vsync_modes[ VSync_100Hz ].SetSupported( 2 );
		vsync_modes[ VSync_50Hz ].SetSupported( 4 );
		break;
	case 240:
		vsync_modes[ VSync_240Hz ].SetSupported( 1 );
		vsync_modes[ VSync_120Hz ].SetSupported( 2 );
		vsync_modes[ VSync_90Hz ].SetSupported( 3 );
		vsync_modes[ VSync_60Hz ].SetSupported( 4 );
		vsync_modes[ VSync_40Hz ].SetSupported( 6 );
		break;
	case 280:
		vsync_modes[ VSync_280Hz ].SetSupported( 1 );
		vsync_modes[ VSync_140Hz ].SetSupported( 2 );
		vsync_modes[ VSync_70Hz ].SetSupported( 4 );
		vsync_modes[ VSync_40Hz ].SetSupported( 7 );
		vsync_modes[ VSync_35Hz ].SetSupported( 8 );
		break;
	case 288:
		vsync_modes[ VSync_288Hz ].SetSupported( 1 );
		vsync_modes[ VSync_144Hz ].SetSupported( 2 );
		vsync_modes[ VSync_72Hz ].SetSupported( 4 );
		vsync_modes[ VSync_36Hz ].SetSupported( 8 );
		break;
	case 300:
		vsync_modes[ VSync_300Hz ].SetSupported( 1 );
		vsync_modes[ VSync_150Hz ].SetSupported( 2 );
		vsync_modes[ VSync_100Hz ].SetSupported( 3 );
		vsync_modes[ VSync_75Hz ].SetSupported( 4 );
		vsync_modes[ VSync_60Hz ].SetSupported( 5 );
		vsync_modes[ VSync_50Hz ].SetSupported( 6 );
		break;
	case 360:
		vsync_modes[ VSync_360Hz ].SetSupported( 1 );
		vsync_modes[ VSync_180Hz ].SetSupported( 2 );
		vsync_modes[ VSync_120Hz ].SetSupported( 3 );
		vsync_modes[ VSync_90Hz ].SetSupported( 4 );
		vsync_modes[ VSync_72Hz ].SetSupported( 5 );
		vsync_modes[ VSync_60Hz ].SetSupported( 6 );
		vsync_modes[ VSync_45Hz ].SetSupported( 8 );
		vsync_modes[ VSync_40Hz ].SetSupported( 9 );
		vsync_modes[ VSync_36Hz ].SetSupported( 10 );
		break;
	default:
		break;
	}

	if( gladHasExtension( "GLX_EXT_swap_control_tear" ) )
	{
		vsync_modes[ VSync_Adaptive ].SetSupported( -1 );
	}

	if( !vsync_modes[ vsync_mode ].supported )
	{
		vsync_mode = VSync_Native;
	}

	SDL_GL_SetSwapInterval( vsync_modes[ vsync_mode ].interval );
}

DOOM_C_API boolean I_VideoSetVSync( vsync_t vsyncval )
{
	if( vsync_modes[ vsync_mode ].supported )
	{
		vsync_mode = vsyncval;
		SDL_GL_SetSwapInterval( vsync_modes[ vsync_mode ].interval );

		return true;
	}

	return false;
}

DOOM_C_API vsync_t I_VideoGetVSync( void )
{
	return (vsync_t)vsync_mode;
}

DOOM_C_API boolean I_VideoSupportsVSync( vsync_t vsyncval )
{
	return vsync_modes[ vsyncval ].supported;
}

DOOM_C_API int64_t I_GetTargetRefreshRate( void )
{
	return vsync_modes[ vsync_mode ].rate;
}

DOOM_C_API void I_VideoSetupGLRenderPath( void )
{
	SetupVSync();
	
	GenerateBuffer( normal_quad, NormalQuadVertices );
	GenerateBuffer( transposed_quad, TransposedQuadVertices );

	GenerateTexture( doom_palette, GL_RGBA8, GL_RGBA, 256, 1 );
	GenerateTexture( doom_backbuffer, GL_R8, GL_RED, render_height, render_width );
	GenerateFrameBuffer( doom_rgbexpanded, render_height, render_width );

	GenerateProgram( translate_backbuffer, basic_vertex_shader, palettedecompress_fragment_shader, "backbuffer", "palette" );
	GenerateProgram( copy_backbuffer, basic_vertex_shader, basic_fragment_shader, "diffuse", NULL );
}

DOOM_C_API void I_VideoResetGLFrameBuffer( void )
{
	glDeleteFramebuffers( 1, &doom_rgbexpanded.fbo );
	glDeleteTextures( 1, &doom_rgbexpanded.rgb.to );
	glDeleteTextures( 1, &doom_backbuffer.to );

	GenerateTexture( doom_backbuffer, GL_R8, GL_RED, render_height, render_width );
	GenerateFrameBuffer( doom_rgbexpanded, render_height, render_width );
}

DOOM_C_API void I_VideoUpdateGLPalette( void* data )
{
	UpdateTexture( doom_palette, data );
}

DOOM_C_API void I_VideoRenderGLIntermediate( void* data )
{
	UpdateTexture( doom_backbuffer, data );

	// SDL doesn't clean up after itself very well, so we unfortunately need to restore the program state after
	GLint sdl_prog;
	glGetIntegerv( GL_CURRENT_PROGRAM, &sdl_prog );

	glActiveTexture( GL_TEXTURE0 );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );

	glActiveTexture( GL_TEXTURE0 + 1 );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );

	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, doom_backbuffer.to );
	glActiveTexture( GL_TEXTURE1 );
	glBindTexture( GL_TEXTURE_2D, doom_palette.to );
	glActiveTexture( GL_TEXTURE0 );

	glBindFramebuffer( GL_FRAMEBUFFER, doom_rgbexpanded.fbo );
	glViewport( 0, 0, doom_rgbexpanded.rgb.width, doom_rgbexpanded.rgb.height );
	glBindVertexArray( normal_quad.vba );
	glUseProgram( translate_backbuffer.program );
	glUniform1i( translate_backbuffer.texture1location, 0 );
	glUniform1i( translate_backbuffer.texture2location, 1 );

	glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );

	glUseProgram( sdl_prog );
	glBindVertexArray( 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	glActiveTexture( GL_TEXTURE1 );
	glBindTexture( GL_TEXTURE_2D, 0 );
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, 0 );
}

DOOM_C_API int32_t I_VideoGetGLIntermediate( void )
{
	return doom_rgbexpanded.rgb.to;
}

DOOM_C_API void I_VideoRenderGLBackbuffer( void )
{
	// SDL doesn't clean up after itself very well, so we unfortunately need to restore the program state after
	GLint sdl_prog;
	glGetIntegerv( GL_CURRENT_PROGRAM, &sdl_prog );

	const int32_t& this_width	= fullscreen ? display_width : window_width;
	const int32_t& this_height	= fullscreen ? display_height : window_height;

	int32_t actual_height = render_post_scaling ? render_height * 1.2 : render_height;

	int32_t viewport_x = 0;
	int32_t viewport_y = 0;
	int32_t viewport_width = this_width;
	int32_t viewport_height = this_height;

	double_t windowratio = (double_t)this_width / (double_t)this_height;
	double_t renderratio = (double_t)render_width / (double_t)actual_height;

	double_t diff = windowratio - renderratio;

	constexpr double_t diffdelta = 0.01; // So that we don't get out-by-one pixel errors when buffer size doesn't match window size

	if( diff > diffdelta )
	{
		viewport_width = ( renderratio / windowratio ) * this_width;
		viewport_x = ( this_width - viewport_width ) >> 1;
	}
	else if( diff < -diffdelta )
	{
		viewport_height = ( windowratio / renderratio ) * this_height;
		viewport_y = ( this_height - viewport_height ) >> 1;
	}


	glViewport( 0, 0, this_width, this_height );
	I_VideoClearBuffer( 0, 0, 0, 0 );

	glActiveTexture( GL_TEXTURE0 );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );

	glBindTexture( GL_TEXTURE_2D, doom_rgbexpanded.rgb.to );
	glViewport( viewport_x, viewport_y, viewport_width, viewport_height );
	glBindVertexArray( transposed_quad.vba );
	glUseProgram( copy_backbuffer.program );
	glUniform1i( copy_backbuffer.texture1location, 0 );

	auto scale = MakeRawVec( (float_t)frame_width / (float_t)render_width, (float_t)frame_height / (float_t)render_height );
	glUniform2fv( copy_backbuffer.texturescalelocation, 1, (float_t*)&scale );

	glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );

	glUseProgram( sdl_prog );
	glBindVertexArray( 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	glActiveTexture( GL_TEXTURE1 );
	glBindTexture( GL_TEXTURE_2D, 0 );
}

DOOM_C_API void I_VideoClearBuffer( float_t r, float_t g, float_t b, float_t a )
{
	glClearColor( r, g, b, a );
	glClear( GL_COLOR_BUFFER_BIT );
}

constexpr GLint FormatToGLInternal[] =
{
	GL_RGB8,
	GL_RGBA8,
	GL_RGB8,
	GL_RGBA8,
};

constexpr GLenum FormatToGLFormat[] =
{
	GL_RGB,
	GL_RGBA,
	GL_BGR,
	GL_BGRA,
};

DOOM_C_API hwtexture_t* I_TextureCreate( int32_t width, int32_t height, hwtextureformat_t format, void* data )
{
	hwtexture_t* tex = new hwtexture_t;
	GenerateTexture( *tex, FormatToGLInternal[ format ], FormatToGLFormat[ format ], width, height, nullptr );
	UpdateTexture( *tex, data );
	return tex;
}

DOOM_C_API void* I_TextureGetHandle( hwtexture_t* tex )
{
	return (void*)tex->to;
}

DOOM_C_API void I_TextureDestroy( hwtexture_t* tex )
{
	DestroyTexture( *tex );
}
