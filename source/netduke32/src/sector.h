#ifndef SECTOR_H
#define SECTOR_H

#include "actors.h"  // ActorData_t
#include "macros.h"
#include "player.h"  // PlayerSpawn_t

// sector lotags
enum
{
    ST_0_NO_EFFECT              = 0,
    ST_1_ABOVE_WATER            = 1,
    ST_2_UNDERWATER             = 2,
    ST_3                        = 3,
    // ^^^ maybe not complete substitution in code
    ST_9_SLIDING_ST_DOOR        = 9,
    ST_15_WARP_ELEVATOR         = 15,
    ST_16_PLATFORM_DOWN         = 16,
    ST_17_PLATFORM_UP           = 17,
    ST_18_ELEVATOR_DOWN         = 18,
    ST_19_ELEVATOR_UP           = 19,
    ST_20_CEILING_DOOR          = 20,
    ST_21_FLOOR_DOOR            = 21,
    ST_22_SPLITTING_DOOR        = 22,
    ST_23_SWINGING_DOOR         = 23,
    ST_25_SLIDING_DOOR          = 25,
    ST_26_SPLITTING_ST_DOOR     = 26,
    ST_27_STRETCH_BRIDGE        = 27,
    ST_28_DROP_FLOOR            = 28,
    ST_29_TEETH_DOOR            = 29,
    ST_30_ROTATE_RISE_BRIDGE    = 30,
    ST_31_TWO_WAY_TRAIN         = 31,
    // left: ST 32767, 65534, 65535
};

typedef struct {
// this needs to have a copy of everything related to the map/actor state
// see savegame.c
    short numwalls;
    walltype wall[MAXWALLS];
    short numsectors;
    sectortype sector[MAXSECTORS];
    spritetype sprite[MAXSPRITES];
    spriteext_t spriteext[MAXSPRITES];

    int32_t numsprites;
    short tailspritefree;
    short headspritesect[MAXSECTORS+1];
    short prevspritesect[MAXSPRITES];
    short nextspritesect[MAXSPRITES];
    short headspritestat[MAXSTATUS+1];
    short prevspritestat[MAXSPRITES];
    short nextspritestat[MAXSPRITES];
    short g_numCyclers;
    short cyclers[MAXCYCLERS][6];
    PlayerSpawn_t g_playerSpawnPoints[MAXSPAWNPOINTS];
    short g_numAnimWalls;
    short SpriteDeletionQueue[1024],g_spriteDeleteQueuePos;
    animwalltype animwall[MAXANIMWALLS];
    vec2_t origins[2048];
    short g_mirrorWall[64], g_mirrorSector[64], g_mirrorCount;
    char show2dsector[(MAXSECTORS+7)>>3];
    short g_numClouds,g_cloudSect[256],g_cloudX,g_cloudY;
    int g_pskyidx;
    ActorData_t actor[MAXSPRITES];
    short pskyoff[MAXPSKYTILES], pskybits;

    int g_animateCount;
    sectanim_t sectorAnims[MAXANIMATES];

    char g_numPlayerSprites;
    char g_earthquakeTime;
    ClockTicks lockclock;
    int randomseed, g_globalRandom;
    char scriptptrs[MAXSPRITES];
    intptr_t *vars[MAXGAMEVARS];
} mapstate_t;

typedef struct {
    int partime, designertime;
    char *name, *filename, *musicfn, *alt_musicfn;
    mapstate_t *savedstate;
} map_t;

// Function Declarations
int32_t A_CheckHitSprite(int spriteNum, int16_t* hitSprite);

#endif // SECTOR_H