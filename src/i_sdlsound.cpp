//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2008 David Flater
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
//	System interface for sound.
//

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include <samplerate.h>

#include <filesystem>
#include <fstream>
#include <cstdio>

#include "doomtype.h"

#include "deh_str.h"

#include "i_sound.h"
#include "i_system.h"
#include "i_swap.h"
#include "i_terminal.h"

#include "m_argv.h"
#include "m_config.h"
#include "m_conv.h"
#include "m_misc.h"

#include "w_wad.h"

#include "z_zone.h"

extern "C"
{
	// Scale factor used when converting libsamplerate floating point numbers
	// to integers. Too high means the sounds can clip; too low means they
	// will be too quiet. This is an amount that should avoid clipping most
	// of the time: with all the Doom IWAD sound effects, at least. If a PWAD
	// is used, clipping might occur.

	float libsamplerate_scale = 0.65f;

	int32_t sound_resample_type = 0;
}

//#define DEBUG_DUMP_WAVS
#define NUM_CHANNELS 16

typedef struct allocated_sound_s allocated_sound_t;

struct allocated_sound_s
{
    sfxinfo_t *sfxinfo;
    Mix_Chunk chunk;
    int use_count;
    int pitch;
    allocated_sound_t *prev, *next;
};

static doombool sound_initialized = false;

static allocated_sound_t *channels_playing[NUM_CHANNELS];

static int mixer_freq;
static Uint16 mixer_format;
static int mixer_channels;
static doombool use_sfx_prefix;
static allocated_sound_t* (*ExpandSoundData)(sfxinfo_t *sfxinfo,
											byte *data,
											int samplerate,
											int length) = nullptr;

// Doubly-linked list of allocated sounds.
// When a sound is played, it is moved to the head, so that the oldest
// sounds not used recently are at the tail.

static allocated_sound_t *allocated_sounds_head = NULL;
static allocated_sound_t *allocated_sounds_tail = NULL;
static int allocated_sounds_size = 0;

constexpr const char* outputtypes[] =
{
	"linear",
	"zeroorderhold",
	"sincfastest",
	"sincmedium",
	"sincbest"
};

// Hook a sound into the linked list at the head.

static void AllocatedSoundLink(allocated_sound_t *snd)
{
    snd->prev = NULL;

    snd->next = allocated_sounds_head;
    allocated_sounds_head = snd;

    if (allocated_sounds_tail == NULL)
    {
        allocated_sounds_tail = snd;
    }
    else
    {
        snd->next->prev = snd;
    }
}

// Unlink a sound from the linked list.

static void AllocatedSoundUnlink(allocated_sound_t *snd)
{
    if (snd->prev == NULL)
    {
        allocated_sounds_head = snd->next;
    }
    else
    {
        snd->prev->next = snd->next;
    }

    if (snd->next == NULL)
    {
        allocated_sounds_tail = snd->prev;
    }
    else
    {
        snd->next->prev = snd->prev;
    }
}

static void FreeAllocatedSound(allocated_sound_t *snd)
{
    // Unlink from linked list.

    AllocatedSoundUnlink(snd);

    // Keep track of the amount of allocated sound data:

    allocated_sounds_size -= snd->chunk.alen;

    free(snd);
}

static void FreeAllAllocatedSounds()
{
	allocated_sound_t* curr = allocated_sounds_head;
	while( curr != nullptr )
	{
		allocated_sound_t* next = curr->next;
		FreeAllocatedSound( curr );
		curr = next;
	}
}

// Search from the tail backwards along the allocated sounds list, find
// and free a sound that is not in use, to free up memory.  Return true
// for success.

static doombool FindAndFreeSound(void)
{
    allocated_sound_t *snd;

    snd = allocated_sounds_tail;

    while (snd != NULL)
    {
        if (snd->use_count == 0)
        {
            FreeAllocatedSound(snd);
            return true;
        }

        snd = snd->prev;
    }

    // No available sounds to free...

    return false;
}

// Enforce SFX cache size limit.  We are just about to allocate "len"
// bytes on the heap for a new sound effect, so free up some space
// so that we keep allocated_sounds_size < snd_cachesize

static void ReserveCacheSpace(size_t len)
{
    if (snd_cachesize <= 0)
    {
        return;
    }

    // Keep freeing sound effects that aren't currently being played,
    // until there is enough space for the new sound.

    while (allocated_sounds_size + len > snd_cachesize)
    {
        // Free a sound.  If there is nothing more to free, stop.

        if (!FindAndFreeSound())
        {
            break;
        }
    }
}

// Allocate a block for a new sound effect.

static allocated_sound_t *AllocateSound(sfxinfo_t *sfxinfo, size_t len)
{
    allocated_sound_t *snd;

    // Keep allocated sounds within the cache size.

    ReserveCacheSpace(len);

    // Allocate the sound structure and data.  The data will immediately
    // follow the structure, which acts as a header.

    do
    {
        snd = (allocated_sound_t*)malloc(sizeof(allocated_sound_t) + len);

        // Out of memory?  Try to free an old sound, then loop round
        // and try again.

        if (snd == NULL && !FindAndFreeSound())
        {
            return NULL;
        }

    } while (snd == NULL);

    // Skip past the chunk structure for the audio buffer

    snd->chunk.abuf = (byte *) (snd + 1);
    snd->chunk.alen = len;
    snd->chunk.allocated = 1;
    snd->chunk.volume = MIX_MAX_VOLUME;
    snd->pitch = NORM_PITCH;

    snd->sfxinfo = sfxinfo;
    snd->use_count = 0;

    // Keep track of how much memory all these cached sounds are using...

    allocated_sounds_size += len;

    AllocatedSoundLink(snd);

    return snd;
}

// Lock a sound, to indicate that it may not be freed.

static void LockAllocatedSound(allocated_sound_t *snd)
{
    // Increase use count, to stop the sound being freed.

    ++snd->use_count;

    //printf("++ %s: Use count=%i\n", snd->sfxinfo->name, snd->use_count);

    // When we use a sound, re-link it into the list at the head, so
    // that the oldest sounds fall to the end of the list for freeing.

    AllocatedSoundUnlink(snd);
    AllocatedSoundLink(snd);
}

// Unlock a sound to indicate that it may now be freed.

static void UnlockAllocatedSound(allocated_sound_t *snd)
{
    if (snd->use_count <= 0)
    {
        I_Error("Sound effect released more times than it was locked...");
    }

    --snd->use_count;

    //printf("-- %s: Use count=%i\n", snd->sfxinfo->name, snd->use_count);
}

// Search through the list of allocated sounds and return the one that matches
// the supplied sfxinfo entry and pitch level.

static allocated_sound_t * GetAllocatedSoundBySfxInfoAndPitch(sfxinfo_t *sfxinfo, int pitch)
{
    allocated_sound_t * p = allocated_sounds_head;

    while (p != NULL)
    {
        if (p->sfxinfo == sfxinfo && p->pitch == pitch)
        {
            return p;
        }
        p = p->next;
    }

    return NULL;
}

// Allocate a new sound chunk and pitch-shift an existing sound up-or-down
// into it.

static allocated_sound_t * PitchShift(allocated_sound_t *insnd, int pitch)
{
    allocated_sound_t * outsnd;
    Sint16 *inp, *outp;
    Sint16 *srcbuf, *dstbuf;
    Uint32 srclen, dstlen;

    srcbuf = (Sint16 *)insnd->chunk.abuf;
    srclen = insnd->chunk.alen;

    // determine ratio pitch:NORM_PITCH and apply to srclen, then invert.
    // This is an approximation of vanilla behaviour based on measurements
    dstlen = (int)((1 + (1 - (float)pitch / NORM_PITCH)) * srclen);

    // ensure that the new buffer is an even length
    if ((dstlen % 2) == 0)
    {
        dstlen++;
    }

    outsnd = AllocateSound(insnd->sfxinfo, dstlen);

    if (!outsnd)
    {
        return NULL;
    }

    outsnd->pitch = pitch;
    dstbuf = (Sint16 *)outsnd->chunk.abuf;

    // loop over output buffer. find corresponding input cell, copy over
    for (outp = dstbuf; outp < dstbuf + dstlen/2; ++outp)
    {
        inp = srcbuf + (int)((float)(outp - dstbuf) / dstlen * srclen);
        *outp = *inp;
    }

    return outsnd;
}

// When a sound stops, check if it is still playing.  If it is not,
// we can mark the sound data as CACHE to be freed back for other
// means.

static void ReleaseSoundOnChannel(int channel)
{
    allocated_sound_t *snd = channels_playing[channel];

    Mix_HaltChannel(channel);

    if (snd == NULL)
    {
        return;
    }

    channels_playing[channel] = NULL;

    UnlockAllocatedSound(snd);

    // if the sound is a pitch-shift and it's not in use, immediately
    // free it
    if (snd->pitch != NORM_PITCH && snd->use_count <= 0)
    {
        FreeAllocatedSound(snd);
    }
}

// Returns the conversion mode for libsamplerate to use.

static int SRC_ConversionMode(void)
{
    switch (sound_resample_type)
    {
        // Ascending numbers give higher quality

        case 0:
            return SRC_LINEAR;
        case 1:
            return SRC_ZERO_ORDER_HOLD;
        case 2:
            return SRC_SINC_FASTEST;
        case 3:
            return SRC_SINC_MEDIUM_QUALITY;
        case 4:
            return SRC_SINC_BEST_QUALITY;
		default:
			return -1;
    }
}

// libsamplerate-based generic sound expansion function for any sample rate
//   unsigned 8 bits --> signed 16 bits
//   mono --> stereo
//   samplerate --> mixer_freq
// Returns number of clipped samples.
// DWF 2008-02-10 with cleanups by Simon Howard.

static allocated_sound_t* ExpandSoundData_SRC(sfxinfo_t *sfxinfo,
                                   byte *data,
                                   int samplerate,
                                   int length)
{
	SRC_DATA src_data;
	float *data_in;
	uint32_t i, abuf_index=0, clipped=0;
	int32_t retn;
	int16_t *expanded;
	allocated_sound_t *snd;
	Mix_Chunk *chunk;

    src_data.input_frames = length;
    data_in = (float*)malloc(length * sizeof(float));
    src_data.data_in = data_in;
    src_data.src_ratio = (double)mixer_freq / samplerate;

    // We include some extra space here in case of rounding-up.
    src_data.output_frames = src_data.src_ratio * length + (mixer_freq / 4);
    src_data.data_out = (float*)malloc(src_data.output_frames * sizeof(float));


    if( src_data.data_in == NULL || src_data.data_out == NULL )
	{
		I_Error( "Error allocating data for sound %s in WAD %s.", sfxinfo->name, W_WadNameForLumpNum( sfxinfo->lumpnum ) );
	}

    // Convert input data to floats

    for (i=0; i<length; ++i)
    {
        // Unclear whether 128 should be interpreted as "zero" or whether a
        // symmetrical range should be assumed.  The following assumes a
        // symmetrical range.
        data_in[i] = data[i] / 127.5 - 1;
    }

    // Do the sound conversion

	retn = src_simple(&src_data, SRC_ConversionMode(), 1);
	if( retn != 0 )
	{
		I_Error( "Error resampling sound %s in WAD %s.", sfxinfo->name, W_WadNameForLumpNum( sfxinfo->lumpnum ) );
	}

    // Allocate the new chunk.

//    alen = src_data.output_frames_gen * 4;

    snd = AllocateSound(sfxinfo, src_data.output_frames_gen * 4);

    if (snd == nullptr)
    {
        return nullptr;
    }

    chunk = &snd->chunk;
    expanded = (int16_t *) chunk->abuf;

    // Convert the result back into 16-bit integers.

    for (i=0; i<src_data.output_frames_gen; ++i)
    {
        // libsamplerate does not limit itself to the -1.0 .. 1.0 range on
        // output, so a multiplier less than INT16_MAX (32767) is required
        // to avoid overflows or clipping.  However, the smaller the
        // multiplier, the quieter the sound effects get, and the more you
        // have to turn down the music to keep it in balance.

        // 22265 is the largest multiplier that can be used to resample all
        // of the Vanilla DOOM sound effects to 48 kHz without clipping
        // using SRC_SINC_BEST_QUALITY.  It is close enough (only slightly
        // too conservative) for SRC_SINC_MEDIUM_QUALITY and
        // SRC_SINC_FASTEST.  PWADs with interestingly different sound
        // effects or target rates other than 48 kHz might still result in
        // clipping--I don't know if there's a limit to it.

        // As the number of clipped samples increases, the signal is
        // gradually overtaken by noise, with the loudest parts going first.
        // However, a moderate amount of clipping is often tolerated in the
        // quest for the loudest possible sound overall.  The results of
        // using INT16_MAX as the multiplier are not all that bad, but
        // artifacts are noticeable during the loudest parts.

        float cvtval_f =
            src_data.data_out[i] * libsamplerate_scale * INT16_MAX;
        int32_t cvtval_i = cvtval_f + (cvtval_f < 0 ? -0.5 : 0.5);

        // Asymmetrical sound worries me, so we won't use -32768.
        if (cvtval_i < -INT16_MAX)
        {
            cvtval_i = -INT16_MAX;
            ++clipped;
        }
        else if (cvtval_i > INT16_MAX)
        {
            cvtval_i = INT16_MAX;
            ++clipped;
        }

        // Left and right channels

        expanded[abuf_index++] = cvtval_i;
        expanded[abuf_index++] = cvtval_i;
    }

    free(data_in);
    free(src_data.data_out);

    if (clipped > 0)
    {
        fprintf(stderr, "Sound '%s': clipped %u samples (%0.2f %%)\n", 
                        sfxinfo->name, clipped,
                        400.0 * clipped / chunk->alen);
    }

    return snd;
}

#ifdef DEBUG_DUMP_WAVS

// Debug code to dump resampled sound effects to WAV files for analysis.

static void WriteWAV(char *filename, byte *data,
                     uint32_t length, int samplerate)
{
    FILE *wav;
    unsigned int i;
    unsigned short s;

    wav = fopen(filename, "wb");

    // Header

    fwrite("RIFF", 1, 4, wav);
    i = LONG(36 + samplerate);
    fwrite(&i, 4, 1, wav);
    fwrite("WAVE", 1, 4, wav);

    // Subchunk 1

    fwrite("fmt ", 1, 4, wav);
    i = LONG(16);
    fwrite(&i, 4, 1, wav);           // Length
    s = SHORT(1);
    fwrite(&s, 2, 1, wav);           // Format (PCM)
    s = SHORT(2);
    fwrite(&s, 2, 1, wav);           // Channels (2=stereo)
    i = LONG(samplerate);
    fwrite(&i, 4, 1, wav);           // Sample rate
    i = LONG(samplerate * 2 * 2);
    fwrite(&i, 4, 1, wav);           // Byte rate (samplerate * stereo * 16 bit)
    s = SHORT(2 * 2);
    fwrite(&s, 2, 1, wav);           // Block align (stereo * 16 bit)
    s = SHORT(16);
    fwrite(&s, 2, 1, wav);           // Bits per sample (16 bit)

    // Data subchunk

    fwrite("data", 1, 4, wav);
    i = LONG(length);
    fwrite(&i, 4, 1, wav);           // Data length
    fwrite(data, 1, length, wav);    // Data

    fclose(wav);
}

#endif

// Load and convert a sound effect
// Returns true if successful

// This is copypasta code, need to find a real place for it to live
static time_t GetLastModifiedTime( const std::filesystem::path& stdpath )
{
	auto filetime = std::filesystem::last_write_time( stdpath );
#if WIN32
	auto utctime = std::chrono::file_clock::to_utc( filetime );
	auto systime = std::chrono::utc_clock::to_sys( utctime );
#else
	auto systime = std::chrono::file_clock::to_sys( filetime );
#endif
	return std::chrono::system_clock::to_time_t( systime );
}

typedef struct sfxpath_s
{
	sfxinfo_t*				sfxinfo;
	std::filesystem::path	path;
	std::string				pathstring;

	std::filesystem::path	folder;
} sfxpath_t;

sfxpath_t GetPath( sfxinfo_t* sfxinfo )
{
	sfxpath_t output = { sfxinfo };

	const char* WADFile = W_WadPathForLumpNum( sfxinfo->lumpnum );
	std::filesystem::path WADFilePath = std::filesystem::absolute( WADFile );

	time_t lastmodified = GetLastModifiedTime( WADFilePath );
	size_t filelength = std::filesystem::file_size( WADFilePath );

	output.pathstring = configdir;
	output.pathstring += "cache" DIR_SEPARATOR_S "sound" DIR_SEPARATOR_S;
	output.pathstring += M_BaseName( WADFile );
	output.pathstring += "." + std::to_string( lastmodified ) + "." + std::to_string( filelength ) + DIR_SEPARATOR_S;
	output.pathstring += outputtypes[ sound_resample_type ];
	output.pathstring += DIR_SEPARATOR_S;

	output.folder = std::filesystem::path( output.pathstring );

	output.pathstring += ToLower( std::string( W_GetNameForNum( sfxinfo->lumpnum ) ) ) + ".dat";

	output.path = std::filesystem::path( output.pathstring );

	return output;
}

static allocated_sound_t* LoadFromCache( sfxpath_t& path )
{
	if( std::filesystem::exists( path.path ) )
	{
		size_t sounddatalen = std::filesystem::file_size( path.path );
		byte* sounddata = (byte*)malloc( sounddatalen );
		FILE* input = fopen( path.pathstring.c_str(), "rb" );
		fread( sounddata, 1, sounddatalen, input );
		fclose( input );

		allocated_sound_t* sound = (allocated_sound_t*)sounddata;
		sound->sfxinfo = path.sfxinfo;
		sound->prev = nullptr;
		sound->next = nullptr;
		sound->chunk.abuf = (Uint8*)( sound + 1 );

		allocated_sounds_size += sounddatalen;

		AllocatedSoundLink( sound );

		return sound;
	}

	return nullptr;
}

static void SaveToCache( sfxpath_t& path, allocated_sound_t* data )
{
	std::filesystem::create_directories( path.folder );
	FILE* output = fopen( path.pathstring.c_str(), "wb" );
	fwrite( data, 1, sizeof( allocated_sound_t ) + data->chunk.alen, output );
	fclose( output );
}

static allocated_sound_t* CacheSFX(sfxinfo_t *sfxinfo)
{
	int32_t lumpnum;
	uint32_t lumplen;
	int32_t samplerate;
	uint32_t length;
	byte *data;

	// need to load the sound

	sfxpath_t path = GetPath( sfxinfo );

	allocated_sound_t* sound = LoadFromCache( path );
	if( !sound )
	{
		lumpnum = sfxinfo->lumpnum;
		data = (byte*)W_CacheLumpNum(lumpnum, PU_STATIC);
		lumplen = W_LumpLength(lumpnum);

		// Check the header, and ensure this is a valid sound

		if (lumplen < 8
		 || data[0] != 0x03 || data[1] != 0x00)
		{
			// Invalid sound

			return nullptr;
		}

		// 16 bit sample rate field, 32 bit length field

		samplerate = (data[3] << 8) | data[2];
		length = (data[7] << 24) | (data[6] << 16) | (data[5] << 8) | data[4];

		// If the header specifies that the length of the sound is greater than
		// the length of the lump itself, this is an invalid sound lump

		// We also discard sound lumps that are less than 49 samples long,
		// as this is how DMX behaves - although the actual cut-off length
		// seems to vary slightly depending on the sample rate.  This needs
		// further investigation to better understand the correct
		// behavior.

		if (length > lumplen - 8 || length <= 48)
		{
			return nullptr;
		}

		// The DMX sound library seems to skip the first 16 and last 16
		// bytes of the lump - reason unknown.

		data += 16;
		length -= 32;

		// Sample rate conversion

		sound = ExpandSoundData(sfxinfo, data + 8, samplerate, length);
		if ( !sound )
		{
			return nullptr;
		}

#ifdef DEBUG_DUMP_WAVS
		{
			char filename[16];
			allocated_sound_t * snd;

			M_snprintf(filename, sizeof(filename), "%s.wav",
					   DEH_String(sfxinfo->name));
			snd = GetAllocatedSoundBySfxInfoAndPitch(sfxinfo, NORM_PITCH);
			WriteWAV(filename, snd->chunk.abuf, snd->chunk.alen,mixer_freq);
		}
#endif
		// don't need the original lump any more
		W_ReleaseLumpNum(lumpnum);

		SaveToCache( path, sound );
	}

	return sound;
}

static void GetSfxLumpName(sfxinfo_t *sfx, char *buf, size_t buf_len)
{
    // Linked sfx lumps? Get the lump number for the sound linked to.

    if (sfx->link != NULL)
    {
        sfx = sfx->link;
    }

    // Doom adds a DS* prefix to sound lumps; Heretic and Hexen don't
    // do this.

    if (use_sfx_prefix)
    {
        M_snprintf(buf, buf_len, "ds%s", DEH_String(sfx->name));
    }
    else
    {
        M_StringCopy(buf, DEH_String(sfx->name), buf_len);
    }
}

static void I_SDL_PrecacheSounds(sfxinfo_t *sounds, int num_sounds)
{
    char namebuf[9];
    int i;

    for (i=0; i<num_sounds; ++i)
    {
        GetSfxLumpName(&sounds[i], namebuf, sizeof(namebuf));

        sounds[i].lumpnum = W_CheckNumForName(namebuf);

        if (sounds[i].lumpnum != -1)
        {
            CacheSFX(&sounds[i]);
        }
    }
}

static void I_SDL_ChangeSoundQuality( int32_t sound_quality, sfxinfo_t* sounds, int num_sounds )
{
	if( sound_quality != sound_resample_type )
	{
		sound_resample_type = sound_quality;

		for( int32_t index = 0; index < NUM_CHANNELS; ++index )
		{
			ReleaseSoundOnChannel( index );
		}

		FreeAllAllocatedSounds();

		for( int32_t index = 0; index < num_sounds; ++index )
		{
			if( sounds[ index ].lumpnum != -1 )
			{
				CacheSFX( &sounds[ index ] );
			}
		}
	}
}

// Load a SFX chunk into memory and ensure that it is locked.

static doombool LockSound(sfxinfo_t *sfxinfo)
{
    // If the sound isn't loaded, load it now
    if (GetAllocatedSoundBySfxInfoAndPitch(sfxinfo, NORM_PITCH) == NULL)
    {
        if (!CacheSFX(sfxinfo))
        {
            return false;
        }
    }

    LockAllocatedSound(GetAllocatedSoundBySfxInfoAndPitch(sfxinfo, NORM_PITCH));

    return true;
}

//
// Retrieve the raw data lump index
//  for a given SFX name.
//

static int I_SDL_GetSfxLumpNum(sfxinfo_t *sfx)
{
    char namebuf[9];

    GetSfxLumpName(sfx, namebuf, sizeof(namebuf));

    return W_GetNumForName(namebuf);
}

static void I_SDL_UpdateSoundParams(int handle, int vol, int sep)
{
    int left, right;

    if (!sound_initialized || handle < 0 || handle >= NUM_CHANNELS)
    {
        return;
    }

    left = ((254 - sep) * vol) / 127;
    right = ((sep) * vol) / 127;

    if (left < 0) left = 0;
    else if ( left > 255) left = 255;
    if (right < 0) right = 0;
    else if (right > 255) right = 255;

    Mix_SetPanning(handle, left, right);
}

//
// Starting a sound means adding it
//  to the current list of active sounds
//  in the internal channels.
// As the SFX info struct contains
//  e.g. a pointer to the raw data,
//  it is ignored.
// As our sound handling does not handle
//  priority, it is ignored.
// Pitching (that is, increased speed of playback)
//  is set, but currently not used by mixing.
//

static int I_SDL_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep, int pitch)
{
    allocated_sound_t *snd;

    if (!sound_initialized || channel < 0 || channel >= NUM_CHANNELS)
    {
        return -1;
    }

    // Release a sound effect if there is already one playing
    // on this channel

    ReleaseSoundOnChannel(channel);

    // Get the sound data

    if (!LockSound(sfxinfo))
    {
        return -1;
    }

    snd = GetAllocatedSoundBySfxInfoAndPitch(sfxinfo, pitch);

    if (snd == NULL)
    {
        allocated_sound_t *newsnd;
        // fetch the base sound effect, un-pitch-shifted
        snd = GetAllocatedSoundBySfxInfoAndPitch(sfxinfo, NORM_PITCH);

        if (snd == NULL)
        {
            return -1;
        }

        if (snd_pitchshift)
        {
            newsnd = PitchShift(snd, pitch);

            if (newsnd)
            {
                LockAllocatedSound(newsnd);
                UnlockAllocatedSound(snd);
                snd = newsnd;
            }
        }
    }
    else
    {
        LockAllocatedSound(snd);
    }

    // play sound

    Mix_PlayChannel(channel, &snd->chunk, 0);

    channels_playing[channel] = snd;

    // set separation, etc.

    I_SDL_UpdateSoundParams(channel, vol, sep);

    return channel;
}

static void I_SDL_StopSound(int handle)
{
    if (!sound_initialized || handle < 0 || handle >= NUM_CHANNELS)
    {
        return;
    }

    // Sound data is no longer needed; release the
    // sound data being used for this channel

    ReleaseSoundOnChannel(handle);
}


static doombool I_SDL_SoundIsPlaying(int handle)
{
    if (!sound_initialized || handle < 0 || handle >= NUM_CHANNELS)
    {
        return false;
    }

    return Mix_Playing(handle);
}

//
// Periodically called to update the sound system
//

static void I_SDL_UpdateSound(void)
{
    int i;

    // Check all channels to see if a sound has finished

    for (i=0; i<NUM_CHANNELS; ++i)
    {
        if (channels_playing[i] && !I_SDL_SoundIsPlaying(i))
        {
            // Sound has finished playing on this channel,
            // but sound data has not been released to cache

            ReleaseSoundOnChannel(i);
        }
    }
}

static void I_SDL_ShutdownSound(void)
{
    if (!sound_initialized)
    {
        return;
    }

    Mix_CloseAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    sound_initialized = false;
}

// Calculate slice size, based on snd_maxslicetime_ms.
// The result must be a power of two.

static int GetSliceSize(void)
{
    int limit;
    int n;

    limit = (snd_samplerate * snd_maxslicetime_ms) / 1000;

    // Try all powers of two, not exceeding the limit.

    for (n=0;; ++n)
    {
        // 2^n <= limit < 2^n+1 ?

        if ((1 << (n + 1)) > limit)
        {
            return (1 << n);
        }
    }

    // Should never happen?

    return 1024;
}

static doombool I_SDL_InitSound(doombool _use_sfx_prefix)
{
    int i;

    // SDL 2.0.6 has a bug that makes it unusable.
    if (SDL_COMPILEDVERSION == SDL_VERSIONNUM(2, 0, 6))
    {
        I_Error(
            "I_SDL_InitSound: "
            "You are trying to launch with SDL 2.0.6 which has a known bug "
            "that makes the game crash. Please either downgrade to "
            "SDL 2.0.5 or upgrade to 2.0.7. See the following bug for some "
            "additional context:\n"
            "<https://github.com/chocolate-doom/chocolate-doom/issues/945>");
    }

    use_sfx_prefix = _use_sfx_prefix;

    // No sounds yet
    for (i=0; i<NUM_CHANNELS; ++i)
    {
        channels_playing[i] = NULL;
    }

    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        I_Error( "Unable to set up sound." );
    }

    if (Mix_OpenAudio(snd_samplerate, AUDIO_S16SYS, 2, GetSliceSize()) < 0)
    {
        I_Error( "Error initialising SDL_mixer: %s", Mix_GetError() );
    }

    Mix_QuerySpec(&mixer_freq, &mixer_format, &mixer_channels);

    if (SRC_ConversionMode() < 0)
    {
        I_Error("I_SDL_InitSound: Invalid value for sound_resample_type: %i",
                sound_resample_type);
    }

    ExpandSoundData = ExpandSoundData_SRC;

    Mix_AllocateChannels(NUM_CHANNELS);

    SDL_PauseAudio(0);

    sound_initialized = true;

    return true;
}

static snddevice_t sound_sdl_devices[] = 
{
    SNDDEVICE_SB,
    SNDDEVICE_PAS,
    SNDDEVICE_GUS,
    SNDDEVICE_WAVEBLASTER,
    SNDDEVICE_SOUNDCANVAS,
    SNDDEVICE_AWE32,
};

DOOM_C_API sound_module_t sound_sdl_module = 
{
    sound_sdl_devices,
    arrlen(sound_sdl_devices),
    I_SDL_InitSound,
    I_SDL_ShutdownSound,
    I_SDL_GetSfxLumpNum,
    I_SDL_UpdateSound,
    I_SDL_UpdateSoundParams,
    I_SDL_StartSound,
    I_SDL_StopSound,
    I_SDL_SoundIsPlaying,
    I_SDL_PrecacheSounds,
	I_SDL_ChangeSoundQuality,
};

