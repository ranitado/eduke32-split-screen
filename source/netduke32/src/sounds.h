//-------------------------------------------------------------------------
/*
Copyright (C) 2010 EDuke32 developers and contributors

This file is part of EDuke32.

EDuke32 is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
//-------------------------------------------------------------------------

//****************************************************************************
//
// sounds.h
//
//****************************************************************************

#ifndef _sounds_public_
#define _sounds_public_

#include "build.h"
#include "compat.h"
#include "sounds_common.h"
#include "dnames.h"

#define MAXSOUNDS           16384
#define MAXSOUNDINSTANCES   8
#define LOUDESTVOLUME       111
#define MUSIC_ID            -65536
#define MAXVOICES           128

typedef struct
{
    int16_t owner;
    int16_t handle;
    uint16_t dist;

    int16_t detached;
    int16_t last_sect;
    vec3_t last_pos;
} voiceinfo_t;

typedef struct
{
    voiceinfo_t* voices;

    char* ptr;
    char* filename;
    int32_t len;
    fix16_t volume;
    int16_t minpitch, maxpitch, distOffset;
    uint8_t  playing;
    uint8_t flags;
    char    priority;
    char    lock;
} sound_t;

EDUKE32_STATIC_ASSERT(MAXSOUNDINSTANCES <= sizeof(((sound_t*)0)->playing) << 3);

extern voiceinfo_t nullvoice;
extern sound_t nullsound;
extern sound_t** g_sounds;
extern int16_t g_skillSoundID; // [JM] Maybe remove in the future?
extern int32_t g_numEnvSoundsPlaying,g_highestSoundIdx;

// Definitions
int32_t S_DefineSound(int sndidx, const char* name, int minpitch, int maxpitch, int priority, int type, int distance, float volume);
int32_t S_DefineMusic(const char* ID, const char* name);

// Loading
int32_t S_LoadSound(uint32_t num);
void S_PrecacheSounds(void);

// Sound state checking
int A_CheckSoundPlaying(int spriteNum, int soundNum);
int A_CheckAnySoundPlaying(int spriteNum);
int S_CheckSoundPlaying(int soundNum);

// Sound Playback
int A_PlaySound(int soundNum, int spriteNum);
int S_PlaySound(int num);
int S_PlaySound3D(int num, int spriteNum, const vec3_t& pos);
int32_t S_PlaySoundXYZ(int32_t num, int32_t i, int32_t x, int32_t y, int32_t z);
void S_MenuSound(void);
void S_StopEnvSound(int soundNum, int spriteNum);
void S_StopAllSounds(void);
void S_PauseSounds(bool paused);
void S_ChangeSoundPitch(int soundNum, int spriteNum, int pitchoffset);
void S_ChangeAllSoundPitches(int pitchOffset);

// Startup, shutdown, allocation, callbacks, updating, and cleanup
void S_AllocIndexes(int sndidx);
void S_SoundStartup(void);
void S_SoundShutdown(void);
void S_Callback(intptr_t num);
void S_Cleanup(void);
void S_Update(void);
void S_DetachSoundsFromSprite(int spriteNum);

// Music Playback
void S_MusicShutdown(void);
void S_MusicStartup(void);
void S_MusicVolume(int32_t volume);
void S_PauseMusic(int32_t onf);
bool S_TryPlayLevelMusic(unsigned int m);
void S_PlayLevelMusicOrNothing(unsigned int m);
int S_TryPlaySpecialMusic(unsigned int m);
void S_PlaySpecialMusicOrNothing(unsigned int m);
void S_ContinueLevelMusic(void);
void S_RestartMusic(void);
void S_StopMusic(void);

// Inline functions
static FORCE_INLINE bool S_SoundIsValid(int soundNum)
{
    return (unsigned)soundNum <= (unsigned)g_highestSoundIdx && g_sounds[soundNum] && g_sounds[soundNum] != &nullsound && g_sounds[soundNum]->ptr;
}

static FORCE_INLINE bool S_IsAmbientSFX(int const spriteNum) { return (sprite[spriteNum].picnum == MUSICANDSFX && sprite[spriteNum].lotag < 999); }

#endif
