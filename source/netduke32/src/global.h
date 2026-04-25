#ifndef global_h_
#define global_h_

#include "build.h"
#include "compat.h"
#include "duke3d.h"
#include "mmulti.h"
#include "quotes.h"
#include "sector.h"
#include "sounds.h"

#ifdef global_c_
#define G_EXTERN
#else
#define G_EXTERN extern
#endif

G_EXTERN int quickExit;

#define MAXINTERPOLATIONS MAXSPRITES

typedef struct
{
    int32_t* curpos;
    int32_t oldpos, bakpos;
} interp_int32_t;

G_EXTERN int32_t g_interpolationCnt;
G_EXTERN int32_t g_interpolationLock;
G_EXTERN interp_int32_t interps[MAXINTERPOLATIONS];

// Tile Data
G_EXTERN char* g_tileLabels[MAXTILES];
G_EXTERN tiledata_t g_tile[MAXTILES];

// Pre-initialized
#ifndef global_c_
extern int g_spriteGravity;
extern int g_actorRespawnTime;
extern int g_itemRespawnTime;
extern int g_timerTicsPerSecond;
extern int g_scriptSize;
extern int g_playerFriction;
extern int g_numFreezeBounces;
extern short g_spriteDeleteQueueSize;
extern char g_numVolumes;

extern char EpisodeNames[MAXVOLUMES][33];
extern char g_skillNames[5][33];
extern int16_t g_blimpSpawnItems[15];
extern char CheatKeys[2];
extern char g_setupFileName[BMAX_PATH];

// Multiplayer
extern int g_networkBroadcastMode; // 0: master/slave, 1: peer-to-peer
#endif

#ifdef __cplusplus
extern "C" {
#endif
// RNG
G_EXTERN short g_globalRandom;
G_EXTERN randomseed_t g_random;
G_EXTERN ClockTicks ototalclock, lockclock;

G_EXTERN short neartagsector, neartagwall, neartagsprite;
G_EXTERN short SpriteDeletionQueue[1024], g_spriteDeleteQueuePos;
G_EXTERN int neartaghitdist;
G_EXTERN int g_startArmorAmount;
G_EXTERN int g_musicSize;
G_EXTERN int g_musicIndex;
G_EXTERN int g_currentMenu;
G_EXTERN int g_currentFrameRate;
G_EXTERN int g_impactDamage;
G_EXTERN int g_scriptDebug;
G_EXTERN int g_showShareware;
G_EXTERN int g_damageCameras;
G_EXTERN int g_freezerSelfDamage;
G_EXTERN int g_tripbombLaserMode;
G_EXTERN int g_rpgBlastRadius;
G_EXTERN int g_pipebombBlastRadius;
G_EXTERN int g_tripbombBlastRadius;
G_EXTERN int g_shrinkerBlastRadius;
G_EXTERN int g_morterBlastRadius;
G_EXTERN int g_bouncemineBlastRadius;
G_EXTERN int g_seenineBlastRadius;
G_EXTERN int g_gameQuit;
G_EXTERN int g_doQuickSave;
G_EXTERN int everyothertime;
G_EXTERN int fricxv, fricyv;
G_EXTERN int totalmemory;
G_EXTERN unsigned int g_moveThingsCount;
G_EXTERN char g_loadFromGroupOnly;
G_EXTERN char g_earthquakeTime;
G_EXTERN char g_RTSPlaying;
G_EXTERN char pus, pub;

G_EXTERN vec2_t g_origins[2048];

G_EXTERN animwalltype animwall[MAXANIMWALLS];
G_EXTERN short g_numAnimWalls;

G_EXTERN sectanim_t sectorAnims[MAXANIMATES];
G_EXTERN int g_animateCount;

G_EXTERN int startofdynamicinterpolations;

G_EXTERN short g_curViewscreen;
G_EXTERN short g_mirrorWall[64], g_mirrorSector[64], g_mirrorCount;

G_EXTERN char buf[1024], tempbuf[2048], packbuf[576], menutextbuf[128];
G_EXTERN char typebuflen, typebuf[141];

G_EXTERN short cyclers[MAXCYCLERS][6], g_numCyclers;

// CON VM
G_EXTERN char *apStrings[MAXQUOTES], *apXStrings[MAXQUOTES];
G_EXTERN intptr_t *insptr;
G_EXTERN intptr_t g_scriptActorOffset;
G_EXTERN intptr_t *apScript;
G_EXTERN intptr_t *g_scriptPtr;

G_EXTERN int32_t g_labelCnt;
G_EXTERN int32_t* labelcode;
G_EXTERN uint8_t* labeltype;
G_EXTERN char *label;

// Clouds
G_EXTERN short g_numClouds, g_cloudSect[256];
G_EXTERN int16_t g_cloudX;
G_EXTERN int16_t g_cloudY;
G_EXTERN ClockTicks cloudtotalclock;

// Structs
G_EXTERN map_t g_mapInfo[(MAXVOLUMES + 1) * MAXLEVELS]; // +1 volume for "intro", "briefing" music
G_EXTERN PlayerSpawn_t g_playerSpawnPoints[MAXSPAWNPOINTS];
G_EXTERN DukeStatus_t sbar;
#ifdef POLYMER
G_EXTERN practor_t practor[MAXSPRITES];
#endif

// Multiplayer/Demos
G_EXTERN int screenpeek;
G_EXTERN char szPlayerName[32];
G_EXTERN char ready2send;
G_EXTERN char g_numPlayerSprites;
G_EXTERN signed char multiwho, multipos, multiwhat, multiflag;
G_EXTERN input_t localInput;
G_EXTERN input_t recsync[RECSYNCBUFSIZ];
G_EXTERN input_t inputfifo[MOVEFIFOSIZ][MAXPLAYERS];
#ifdef __cplusplus
}
#endif

#endif