//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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
//	The not so system specific sound interface.
//


#ifndef __I_SOUND__
#define __I_SOUND__

#include "doomtype.h"

// so that the individual game logic and sound driver code agree
#define NORM_PITCH 127

//
// SoundFX struct.
//
DOOM_C_API typedef struct sfxinfo_struct	sfxinfo_t;

DOOM_C_API struct sfxinfo_struct
{
	int32_t soundnum;
	int32_t minimumversion;

    // tag name, used for hexen.
    const char *tagname;

    // lump name.  If we are running with use_sfx_prefix=true, a
    // 'DS' (or 'DP' for PC speaker sounds) is prepended to this.

    char name[9];

    // Sfx priority
    int priority;

    // referenced sound if a link
    sfxinfo_t *link;

    // pitch if a link (Doom), whether to pitch-shift (Hexen)
    int pitch;

    // volume if a link
    int volume;

    // this is checked every second to see if sound
    // can be thrown out (if 0, then decrement, if -1,
    // then throw out, if > 0, then it is in use)
    int usefulness;

    // lump number of sfx
    int lumpnum;

    // Maximum number of channels that the sound can be played on 
    // (Heretic)
    int numchannels;

    // data used by the low level code
    void *driver_data;
};

//
// MusicInfo struct.
//
DOOM_C_API typedef struct
{
    // up to 6-character name
    const char *name;

    // lump number of music
    int lumpnum;

    // music data
    void *data;

    // music handle once registered
    void *handle;

} musicinfo_t;

DOOM_C_API typedef enum 
{
    SNDDEVICE_NONE = 0,
    SNDDEVICE_PCSPEAKER = 1,
    SNDDEVICE_ADLIB = 2,
    SNDDEVICE_SB = 3,
    SNDDEVICE_PAS = 4,
    SNDDEVICE_GUS = 5,
    SNDDEVICE_WAVEBLASTER = 6,
    SNDDEVICE_SOUNDCANVAS = 7,
    SNDDEVICE_GENMIDI = 8,
    SNDDEVICE_AWE32 = 9,
    SNDDEVICE_CD = 10,
} snddevice_t;

// Interface for sound modules

DOOM_C_API typedef struct
{
    // List of sound devices that this sound module is used for.

    snddevice_t *sound_devices;
    int num_sound_devices;

    // Initialise sound module
    // Returns true if successfully initialised

    doombool (*Init)(doombool use_sfx_prefix);

    // Shutdown sound module

    void (*Shutdown)(void);

    // Returns the lump index of the given sound.

    int (*GetSfxLumpNum)(sfxinfo_t *sfxinfo);

    // Called periodically to update the subsystem.

    void (*Update)(void);

    // Update the sound settings on the given channel.

    void (*UpdateSoundParams)(int channel, int vol, int sep);

    // Start a sound on a given channel.  Returns the channel id
    // or -1 on failure.

    int (*StartSound)(sfxinfo_t *sfxinfo, int channel, int vol, int sep, int pitch);

    // Stop the sound playing on the given channel.

    void (*StopSound)(int channel);

    // Query if a sound is playing on the given channel

    doombool (*SoundIsPlaying)(int channel);

    // Called on startup to precache sound effects (if necessary)

    void (*CacheSounds)(sfxinfo_t *sounds, int num_sounds);

	void (*ChangeSoundQuality)( int sound_quality, sfxinfo_t* sounds, int num_sounds );

} sound_module_t;

DOOM_C_API void I_InitSound(doombool use_sfx_prefix);
DOOM_C_API void I_ShutdownSound(void);
DOOM_C_API int I_GetSfxLumpNum(sfxinfo_t *sfxinfo);
DOOM_C_API void I_UpdateSound(void);
DOOM_C_API void I_UpdateSoundParams(int channel, int vol, int sep);
DOOM_C_API int I_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep, int pitch);
DOOM_C_API void I_StopSound(int channel);
DOOM_C_API doombool I_SoundIsPlaying(int channel);
DOOM_C_API void I_PrecacheSounds(sfxinfo_t *sounds, int num_sounds);
DOOM_C_API void I_ChangeSoundQuality( int32_t sound_quality, sfxinfo_t* sounds, int num_sounds );

// Interface for music modules

DOOM_C_API typedef struct
{
    // List of sound devices that this music module is used for.

    snddevice_t *sound_devices;
    int num_sound_devices;

    // Initialise the music subsystem

    doombool (*Init)(void);

    // Shutdown the music subsystem

    void (*Shutdown)(void);

    // Set music volume - range 0-127

    void (*SetMusicVolume)(int volume);

    // Pause music

    void (*PauseMusic)(void);

    // Un-pause music

    void (*ResumeMusic)(void);

    // Register a song handle from data
    // Returns a handle that can be used to play the song

    void *(*RegisterSong)(void *data, int len);

    // Un-register (free) song data

    void (*UnRegisterSong)(void *handle);

    // Play the song

    void (*PlaySong)(void *handle, doombool looping);

    // Stop playing the current song.

    void (*StopSong)(void);

    // Query if music is playing.

    doombool (*MusicIsPlaying)(void);

    // Invoked periodically to poll.

    void (*Poll)(void);
} music_module_t;

DOOM_C_API void I_InitMusic(void);
DOOM_C_API void I_ShutdownMusic(void);
DOOM_C_API void I_SetMusicVolume(int volume);
DOOM_C_API void I_PauseSong(void);
DOOM_C_API void I_ResumeSong(void);
DOOM_C_API void *I_RegisterSong(void *data, int len);
DOOM_C_API void I_UnRegisterSong(void *handle);
DOOM_C_API void I_PlaySong(void *handle, doombool looping);
DOOM_C_API void I_StopSong(void);
DOOM_C_API doombool I_MusicIsPlaying(void);

DOOM_C_API extern int snd_sfxdevice;
DOOM_C_API extern int snd_musicdevice;
DOOM_C_API extern int snd_samplerate;
DOOM_C_API extern int snd_cachesize;
DOOM_C_API extern int snd_maxslicetime_ms;
DOOM_C_API extern char *snd_musiccmd;
DOOM_C_API extern int snd_pitchshift;

DOOM_C_API void I_BindSoundVariables(void);

// DMX version to emulate for OPL emulation:
DOOM_C_API typedef enum {
    opl_doom1_1_666,    // Doom 1 v1.666
    opl_doom2_1_666,    // Doom 2 v1.666, Hexen, Heretic
    opl_doom_1_9        // Doom v1.9, Strife
} opl_driver_ver_t;

DOOM_C_API void I_SetOPLDriverVer(opl_driver_ver_t ver);

#endif

