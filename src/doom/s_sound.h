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


#ifndef __S_SOUND__
#define __S_SOUND__

#include "p_mobj.h"
#include "sounds.h"
#include "d_gameflow.h"

//
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//  allocates channel buffer, sets S_sfx lookup.
//

DOOM_C_API void S_Init(int sfxVolume, int musicVolume);


// Shut down sound 

DOOM_C_API void S_Shutdown(void);



//
// Per level startup code.
// Kills playing sounds at start of level,
//  determines music if any, changes music.
//

DOOM_C_API void S_Start(void);

//
// Start sound for thing at <origin>
//  using <sound_id> from sounds.h
//

DOOM_C_API void S_StartSound(void *origin, int sound_id);

// Stop sound for thing at <origin>
DOOM_C_API void S_StopSound(mobj_t *origin);


// Start music using <music_id> from sounds.h
DOOM_C_API void S_StartMusic(int music_id);

// Start music using <music_id> from sounds.h,
//  and set whether looping
DOOM_C_API void S_ChangeMusic(int music_id, int looping);

DOOM_C_API void S_ChangeMusicLump( flowstring_t* lump, int32_t looping );

// query if music is playing
DOOM_C_API doombool S_MusicPlaying(void);

// Stops the music fer sure.
DOOM_C_API void S_StopMusic(void);

// Stop and resume music, during game PAUSE.
DOOM_C_API void S_PauseSound(void);
DOOM_C_API void S_ResumeSound(void);


//
// Updates music & sounds
//
DOOM_C_API void S_UpdateSounds(mobj_t *listener);

DOOM_C_API void S_SetMusicVolume(int volume);
DOOM_C_API void S_SetSfxVolume(int volume);

DOOM_C_API extern int snd_channels;

#endif

