//-------------------------------------------------------------------------
/*
Copyright (C) 1996, 2003 - 3D Realms Entertainment
Copyright (C) 2000, 2003 - Matt Saettler (EDuke Enhancements)
Copyright (C) 2004, 2007 - EDuke32 developers

This file is part of EDuke32

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

#ifndef _duke3d_h_
# define _duke3d_h_

// Max number of players & spawns
#define MAXPLAYERS 16
#define MAXSPAWNPOINTS 64

#define VERSION "v1.3"
#define HEAD2 APPNAME " " VERSION

#define MOVEFIFOSIZ 256

#define OSDTEXT_DEFAULT   "^00"
#define OSDTEXT_BLUE      "^01"
#define OSDTEXT_DARKRED   "^10"
#define OSDTEXT_GREEN     "^11"
#define OSDTEXT_RED       "^21"
#define OSDTEXT_YELLOW    "^23"

#define OSDTEXT_BRIGHT    "^S0"

#define OSD_ERROR OSDTEXT_DARKRED OSDTEXT_BRIGHT

#define HORIZ_MIN -99
#define HORIZ_MAX 299

#define VOLUMEALL (g_Shareware==0)
#define PLUTOPAK (g_scriptVersion==14)
#define VOLUMEONE (g_Shareware==1)

#define MAXSLEEPDIST  16384
#define SLEEPTIME 24*64

#define BYTEVERSION_13  27
#define BYTEVERSION_14  116
#define BYTEVERSION_15  117
#define BYTEVERSION_NETDUKE32  192 // increase by 3, because atomic GRP adds 1, and Shareware adds 2

#define BYTEVERSION (BYTEVERSION_NETDUKE32+(PLUTOPAK?1:(VOLUMEONE<<1)))    // JBF 20040116: different data files give different versions

#define NUMPAGES 1

#define AUTO_AIM_ANGLE          48
#define RECSYNCBUFSIZ 2520   //2520 is the (LCM of 1-8)*3

#define ACTOR_FLOOR_OFFSET  (1<<8)
#define ZOFFSET2            (16<<8)
#define ZOFFSET3            (8<<8)
#define ZOFFSET4            (12<<8)
#define ZOFFSET5            (32<<8)
#define ZOFFSET6            (4<<8)

#define CAMERA(Membname) (ud.camera ## Membname)

#define MAXVOLUMES 14
#define MAXLEVELS 64
#define MAXANIMWALLS 512
#define MAXANIMATES 1024
#define MAXCYCLERS 1024
#define MAXQUOTES 16384

enum {
    MUS_FIRST_SPECIAL = MAXVOLUMES * MAXLEVELS,

    MUS_INTRO = MUS_FIRST_SPECIAL,
    MUS_BRIEFING = MUS_FIRST_SPECIAL + 1,
    MUS_LOADING = MUS_FIRST_SPECIAL + 2,
    MUS_USERMAP = MUS_FIRST_SPECIAL + 3,
};

// sector effector lotags
enum
{
    SE_0_ROTATING_SECTOR = 0,
    SE_1_PIVOT = 1,
    SE_2_EARTHQUAKE = 2,
    SE_3_RANDOM_LIGHTS_AFTER_SHOT_OUT = 3,
    SE_4_RANDOM_LIGHTS = 4,
    SE_5 = 5,
    SE_6_SUBWAY = 6,

    SE_7_TELEPORT = 7,
    SE_8_UP_OPEN_DOOR_LIGHTS = 8,
    SE_9_DOWN_OPEN_DOOR_LIGHTS = 9,
    SE_10_DOOR_AUTO_CLOSE = 10,
    SE_11_SWINGING_DOOR = 11,
    SE_12_LIGHT_SWITCH = 12,
    SE_13_EXPLOSIVE = 13,
    SE_14_SUBWAY_CAR = 14,
    SE_15_SLIDING_DOOR = 15,
    SE_16_REACTOR = 16,
    SE_17_WARP_ELEVATOR = 17,
    SE_18_INCREMENTAL_SECTOR_RISE_FALL = 18,
    SE_19_EXPLOSION_LOWERS_CEILING = 19,
    SE_20_STRETCH_BRIDGE = 20,
    SE_21_DROP_FLOOR = 21,
    SE_22_TEETH_DOOR = 22,
    SE_23_ONE_WAY_TELEPORT = 23,
    SE_24_CONVEYOR = 24,
    SE_25_PISTON = 25,
    SE_26 = 26,
    SE_27_DEMO_CAM = 27,
    SE_28_LIGHTNING = 28,
    SE_29_WAVES = 29,
    SE_30_TWO_WAY_TRAIN = 30,
    SE_31_FLOOR_RISE_FALL = 31,
    SE_32_CEILING_RISE_FALL = 32,
    SE_33_QUAKE_DEBRIS = 33,
    SE_34 = 34,  // XXX
    SE_35 = 35,  // XXX
    SE_36_PROJ_SHOOTER = 36,
    SE_40_ROR_UPPER = 40,
    SE_41_ROR_LOWER = 41,
    SE_46_ROR_PROTECTED = 46,
    SE_49_POINT_LIGHT = 49,
    SE_50_SPOT_LIGHT = 50,
    SE_130 = 130,
    SE_131 = 131,
};

enum weaponuniqhudid_t
{
    W_NONE,
    W_ACCESSCARD,
    W_CHAINGUN_BOTTOM,
    W_CHAINGUN_HACK,
    W_CHAINGUN_TOP,
    W_DEVISTATOR_LEFT,
    W_DEVISTATOR_RIGHT,
    W_DUKENUKEM,
    W_FIST,
    W_FIST2,
    W_FREEZE_BASE,
    W_FREEZE_TOP,
    W_HANDBOMB,
    W_HANDREMOTE,
    W_KNEE,
    W_KNEE2,
    W_KNUCKLES,
    W_LOOGIE,
    W_LOOGIE_END = W_LOOGIE + 63,
    W_PISTOL,
    W_PISTOL_CLIP,
    W_PISTOL_HAND,
    W_PLUTOPAK,
    W_RPG,
    W_RPG_MUZZLE,
    W_SHOTGUN,
    W_SHOTGUN_MUZZLE,
    W_SHRINKER,
    W_SHRINKER_CRYSTAL,
    W_THREEDEE,
    W_TIP,
    W_TRIPBOMB,
    W_TRIPBOMB_LEFTHAND,
    W_TRIPBOMB_RIGHTHAND,

    W_END,
};

// Only here temporarily, move to cheats.h later.
enum cheats
{
    CHEAT_CORNHOLIO,
    CHEAT_STUFF,
    CHEAT_SCOTTY,
    CHEAT_COORDS,
    CHEAT_VIEW,
    CHEAT_TIME,
    CHEAT_UNLOCK,
    CHEAT_CASHMAN,
    CHEAT_ITEMS,
    CHEAT_RATE,
    CHEAT_SKILL,
    CHEAT_BETA,
    CHEAT_HYPER,
    CHEAT_MONSTERS,
    CHEAT_RESERVED,
    CHEAT_RESERVED2,
    CHEAT_TODD,
    CHEAT_SHOWMAP,
    CHEAT_KROZ,
    CHEAT_ALLEN,
    CHEAT_CLIP,
    CHEAT_WEAPONS,
    CHEAT_INVENTORY,
    CHEAT_KEYS,
    CHEAT_DEBUG,
    CHEAT_RESERVED3,
    CHEAT_SCREAMFORME,
    CHEAT_BOOTI,
};

// NetDuke32 TO-DO: Move to cmdline.h when I get a chance.
extern const char* CommandName;

#include "compat.h"
#include "a.h"
#include "build.h"
#include "cache1d.h"
#include "pragmas.h"

#ifdef RANCID_NETWORKING
#include "mmulti_unstable.h"
#else
#include "mmulti.h"
#endif

#include "baselayer.h"
#include "function.h"
//#include "types.h"
//#include "file_lib.h"
#include "common.h"
#include "gamedefs.h"
#include "keyboard.h"
//#include "util_lib.h"
//#include "mathutil.h"
#include "function.h"
#include "fx_man.h"
#include "config.h"
#include "control.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int g_scriptVersion, g_Shareware, g_gameType;

////////// TIMING CONSTANTS //////////
// The number of 'totalclock' increments per second:
#define TICRATE             120
// The number of game state updates per second:
#define REALGAMETICSPERSEC  30
// The number of 'totalclock' increments per game state update:
// NOTE: calling a game state update a 'frame' is really weird.
// (This used to be TICRATE/GAMETICSPERSEC, which was 120/26 = 4.615~ truncated
// to 4 by integer division.)
#define TICSPERFRAME        (TICRATE/REALGAMETICSPERSEC)
// Used as a constant to satisfy all of the calculations written with ticrate =
// 26 in mind:
#define GAMETICSPERSEC      26

// #define GC (TICSPERFRAME*44)

#define STAT_DEFAULT        0
#define STAT_ACTOR          1
#define STAT_ZOMBIEACTOR    2
#define STAT_EFFECTOR       3
#define STAT_PROJECTILE     4
#define STAT_MISC           5
#define STAT_STANDABLE      6
#define STAT_LOCATOR        7
#define STAT_ACTIVATOR      8
#define STAT_TRANSPORT      9
#define STAT_PLAYER         10
#define STAT_FX             11
#define STAT_FALLER         12
#define STAT_DUMMYPLAYER    13
#define STAT_LIGHT          14

#define    ALT_IS_PRESSED ( KB_KeyPressed( sc_RightAlt ) || KB_KeyPressed( sc_LeftAlt ) )
#define    SHIFTS_IS_PRESSED ( KB_KeyPressed( sc_RightShift ) || KB_KeyPressed( sc_LeftShift ) )
#define    RANDOMSCRAP(s, i) A_InsertSprite(s->sectnum,s->x+(krand()&255)-128,s->y+(krand()&255)-128,s->z-(8<<8)-(krand()&8191),SCRAP6+(krand()&15),-8,48,48,krand()&2047,(krand()&63)+64,-512-(krand()&2047),i,5)

enum GameMode_t {
    MODE_MENU       = 1,
    MODE_DEMO       = 2,
    MODE_GAME       = 4,
    MODE_EOL        = 8,
    MODE_TYPE       = 16,
    MODE_RESTART    = 32,
    MODE_SENDTOWHOM = 64,
    MODE_END        = 128
};

// Defines the motion characteristics of an actor

enum ActorMoveFlags_t {
    face_player         = 1,
    geth                = 2,
    getv                = 4,
    random_angle        = 8,
    face_player_slow    = 16,
    spin                = 32,
    face_player_smart   = 64,
    fleeenemy           = 128,
    jumptoplayer_only   = 256,
    jumptoplayer_bits   = 257,  // NOTE: two bits set!
    seekplayer          = 512,
    furthestdir         = 1024,
    dodgebullet         = 4096,
    noclip              = 8192,
};

// Some misc #defines
#define NO       0
#define YES      1

// Defines for 'useractor' keyword

enum UserActorTypes_t {
    notenemy,
    enemy,
    enemystayput
};

// Player Actions.

enum PlayerActionFlags_t {
    pstanding       = 1,
    pwalking        = 2,
    prunning        = 4,
    pducking        = 8,
    pfalling        = 16,
    pjumping        = 32,
    phigher         = 64,
    pwalkingback    = 128,
    prunningback    = 256,
    pkicking        = 512,
    pshrunk         = 1024,
    pjetpack        = 2048,
    ponsteroids     = 4096,
    ponground       = 8192,
    palive          = 16384,
    pdead           = 32768,
    pfacing         = 65536
};

enum
{
    EXTBIT_MOVEFORWARD  = (1),
    EXTBIT_MOVEBACKWARD = (1 << 1),
    EXTBIT_STRAFELEFT   = (1 << 2),
    EXTBIT_STRAFERIGHT  = (1 << 3),
    EXTBIT_TURNLEFT     = (1 << 4),
    EXTBIT_TURNRIGHT    = (1 << 5),
    EXTBIT_ALTFIRE      = (1 << 6),
    EXTBIT_CHAT         = (1 << 7),
    EXTBIT_CHANGETEAM   = (1 << 8),
};

typedef struct {
    uint32_t bits, extbits;
    short fvel, svel;
    fix16_t q16avel, q16horz;
} input_t;

extern input_t recsync[RECSYNCBUFSIZ];

enum SectAnimType_t
{
    SECTANIM_FLOOR,
    SECTANIM_CEILING,
    SECTANIM_WALLX,
    SECTANIM_WALLY,
};

typedef struct
{
    int32_t goal, vel;
    int16_t target;
    int8_t type;
} sectanim_t;

typedef struct {
    int wallnum, tag;
} animwalltype;

struct randomseed_t
{
    int32_t global, animwall, player, playerweapon;

    randomseed_t()
    {
        global = animwall = player = playerweapon = 1996;
    }
};

extern int probey;

struct savehead {
    char name[19];
    int32_t numplr,volnum,levnum,plrskl;
    char boardfn[BMAX_PATH];
};

typedef struct {
    vec3_t pos;
    int16_t ang,sectnum;
    uint8_t pal;
} PlayerSpawn_t;

extern char g_numPlayerSprites;

#define MAXQUOTELEN 128
#define MAXUSERQUOTES 6
#define MAXUSERQUOTELEN 256 // Formerly 178.
#define USERQUOTE_LEFTOFFSET 5
#define USERQUOTE_RIGHTOFFSET 14
extern int quotebot, quotebotgoal; // game.c

typedef struct {
/*    int x;
    int y;
    int z; */
    short ang, oldang, angdir, angdif;
} spriteinterpolate;

extern int tempwallptr;
extern intptr_t frameplace;

extern int g_screenCapture;

extern int g_lastSaveSlot;
extern int g_restorePalette;

extern int cachecount;

//DUKE3D.H:
extern int g_cameraDistance, g_cameraClock;

extern int stereomode, stereowidth, stereopixelwidth;

extern int g_myAimMode, g_myAimStat, g_oldAimStat;

#define IFISGLMODE if (videoGetRenderMode() >= REND_POLYMOST)
#define IFISSOFTMODE if (videoGetRenderMode() < REND_POLYMOST)

#define TILE_SAVESHOT (MAXTILES-1)
#define TILE_LOADSHOT (MAXTILES-3)
#define TILE_TILT     (MAXTILES-2)
#define TILE_ANIM     (MAXTILES-4)
#define TILE_VIEWSCR  (MAXTILES-5)

enum GameEvent_t {
    EVENT_INIT,
    EVENT_SETDEFAULTS,
    EVENT_ENTERLEVEL,
    EVENT_RESETWEAPONS,
    EVENT_RESETINVENTORY,
    EVENT_HOLSTER,
    EVENT_LOOKLEFT,
    EVENT_LOOKRIGHT,
    EVENT_SOARUP,
    EVENT_SOARDOWN,
    EVENT_CROUCH,
    EVENT_JUMP,
    EVENT_RETURNTOCENTER,
    EVENT_LOOKUP,
    EVENT_LOOKDOWN,
    EVENT_AIMUP,
    EVENT_FIRE,
    EVENT_CHANGEWEAPON,
    EVENT_GETSHOTRANGE,
    EVENT_GETAUTOAIMANGLE,
    EVENT_GETLOADTILE,
    EVENT_CHEATGETSTEROIDS,
    EVENT_CHEATGETHEAT,
    EVENT_CHEATGETBOOT,
    EVENT_CHEATGETSHIELD,
    EVENT_CHEATGETSCUBA,
    EVENT_CHEATGETHOLODUKE,
    EVENT_CHEATGETJETPACK,
    EVENT_CHEATGETFIRSTAID,
    EVENT_QUICKKICK,
    EVENT_INVENTORY,
    EVENT_USENIGHTVISION,
    EVENT_USESTEROIDS,
    EVENT_INVENTORYLEFT,
    EVENT_INVENTORYRIGHT,
    EVENT_HOLODUKEON,
    EVENT_HOLODUKEOFF,
    EVENT_USEMEDKIT,
    EVENT_USEJETPACK,
    EVENT_TURNAROUND,
    EVENT_DISPLAYWEAPON,
    EVENT_FIREWEAPON,
    EVENT_SELECTWEAPON,
    EVENT_MOVEFORWARD,
    EVENT_MOVEBACKWARD,
    EVENT_TURNLEFT,
    EVENT_TURNRIGHT,
    EVENT_STRAFELEFT,
    EVENT_STRAFERIGHT,
    EVENT_WEAPKEY1,
    EVENT_WEAPKEY2,
    EVENT_WEAPKEY3,
    EVENT_WEAPKEY4,
    EVENT_WEAPKEY5,
    EVENT_WEAPKEY6,
    EVENT_WEAPKEY7,
    EVENT_WEAPKEY8,
    EVENT_WEAPKEY9,
    EVENT_WEAPKEY10,
    EVENT_DRAWWEAPON,
    EVENT_DISPLAYCROSSHAIR,
    EVENT_DISPLAYREST,
    EVENT_DISPLAYSBAR,
    EVENT_DISPLAYLEVELSTATS,
    EVENT_DISPLAYACCESS,
    EVENT_RESETPLAYER,
    EVENT_INCURDAMAGE,
    EVENT_AIMDOWN,
    EVENT_GAME,
    EVENT_PREVIOUSWEAPON,
    EVENT_NEXTWEAPON,
    EVENT_SWIMUP,
    EVENT_SWIMDOWN,
    EVENT_GETMENUTILE,
    EVENT_SPAWN,
    EVENT_LOGO,
    EVENT_EGS,
    EVENT_DOFIRE,
    EVENT_PREWEAPONSHOOT,
    EVENT_POSTWEAPONSHOOT,
    EVENT_PRESSEDFIRE,
    EVENT_USE,
    EVENT_PROCESSINPUT,
    EVENT_FAKEDOMOVETHINGS,
    EVENT_DISPLAYSTART,
    EVENT_DISPLAYROOMS,
    EVENT_KILLIT,
    EVENT_LOADACTOR,
    EVENT_DISPLAYBONUSSCREEN,
    EVENT_DISPLAYMENU,
    EVENT_DISPLAYMENUREST,
    EVENT_DISPLAYLOADINGSCREEN,
    EVENT_DISPLAYTIP,
    EVENT_ANIMATESPRITES,
    EVENT_NEWGAME,
    EVENT_PREGAME,
    EVENT_PREWORLD,
    EVENT_WORLD,
    EVENT_SOUND,
    EVENT_PRELEVEL,
    EVENT_DAMAGESPRITE,
    EVENT_POSTDAMAGESPRITE,
    EVENT_DAMAGEFLOOR,
    EVENT_DAMAGECEILING,
    EVENT_DAMAGEWALL,
    EVENT_ALTWEAPON,
    EVENT_LASTWEAPON,
    EVENT_ALTFIRE,
    EVENT_DOQUAKE,
    EVENT_CHECKTOUCHDAMAGE,
    EVENT_BOTGETSPRITESCORE,
    EVENT_FRAGGED,
    EVENT_ACTIVATESWITCH,
    MAXEVENTS
};

enum SystemString_t {
    STR_MAPNAME,
    STR_MAPFILENAME,
    STR_PLAYERNAME,
    STR_VERSION,
    STR_GAMETYPE
};

// store global game definitions

enum SpriteFlags_t {
    SFLAG_SHADOW           = 0x00000001,
    SFLAG_NVG              = 0x00000002,
    SFLAG_NOSHADE          = 0x00000004,
    SFLAG_PROJECTILE       = 0x00000008,
    SFLAG_DECAL            = 0x00000010,
    SFLAG_BADGUY           = 0x00000020,
    SFLAG_NOPAL            = 0x00000040,
    SFLAG_NOEVENTCODE      = 0x00000080,
    SFLAG_NOLIGHT          = 0x00000100,
    SFLAG_USEACTIVATOR     = 0x00000200,
    SFLAG_SMOOTHMOVE       = 0x00002000,
    SFLAG_NOTELEPORT       = 0x00004000,
    SFLAG_BADGUYSTAYPUT    = 0x00008000,
    SFLAG_CACHE            = 0x00010000,
    SFLAG_ROTFIXED         = 0x00020000,
    SFLAG_HARDCODED_BADGUY = 0x00040000,
    SFLAG_RESERVED1        = 0x00080000, // Was SFLAG_DIDNOSE7WATER
    SFLAG_NODAMAGEPUSH     = 0x00100000,
    SFLAG_NOWATERDIP       = 0x00200000,
    SFLAG_HURTSPAWNBLOOD   = 0x00400000,
    SFLAG_GREENSLIMEFOOD   = 0x00800000,
    SFLAG_DAMAGEEVENT      = 0x04000000,
    SFLAG_QUEUEDFORDELETE  = 0x10000000,
};

extern char g_szBuf[1024];

#define NAM_GRENADE_LIFETIME       120
#define NAM_GRENADE_LIFETIME_VAR    30

extern int g_timerTicsPerSecond;

#define TRIPBOMB_TRIPWIRE           1
#define TRIPBOMB_TIMER              2

#define PIPEBOMB_REMOTE             1
#define PIPEBOMB_TIMER              2

// logo control

enum LogoFlags_t {
    LOGO_ENABLED           = 1,
    LOGO_PLAYANIM          = 2,
    LOGO_PLAYMUSIC         = 4,
    LOGO_3DRSCREEN         = 8,
    LOGO_TITLESCREEN       = 16,
    LOGO_DUKENUKEM         = 32,
    LOGO_THREEDEE          = 64,
    LOGO_PLUTOPAKSPRITE    = 128,
    LOGO_SHAREWARESCREENS  = 256,
    LOGO_TENSCREEN         = 512
};

enum ScreenTextFlags_t {
    TEXT_XRIGHT = 0x00000001,
    TEXT_XCENTER = 0x00000002,
    TEXT_YBOTTOM = 0x00000004,
    TEXT_YCENTER = 0x00000008,
    TEXT_INTERNALSPACE = 0x00000010,
    TEXT_TILESPACE = 0x00000020,
    TEXT_INTERNALLINE = 0x00000040,
    TEXT_TILELINE = 0x00000080,
    TEXT_XOFFSETZERO = 0x00000100,
    TEXT_XJUSTIFY = 0x00000200,
    TEXT_YOFFSETZERO = 0x00000400,
    TEXT_YJUSTIFY = 0x00000800,
    TEXT_LINEWRAP = 0x00001000,
    TEXT_UPPERCASE = 0x00002000,
    TEXT_INVERTCASE = 0x00004000,
    TEXT_IGNOREESCAPE = 0x00008000,
    TEXT_LITERALESCAPE = 0x00010000,
    TEXT_BACKWARDS = 0x00020000,
    TEXT_GAMETEXTNUMHACK = 0x00040000,
    TEXT_DIGITALNUMBER = 0x00080000,
    TEXT_BIGALPHANUM = 0x00100000,
    TEXT_GRAYFONT = 0x00200000,
};

extern int g_numRealPalettes;

#define MAXCHEATLEN 20
#define NUMCHEATCODES (signed int)(sizeof(CheatStrings)/sizeof(CheatStrings[0]))

extern char CheatStrings[][MAXCHEATLEN];

extern int g_numXStrings;

// key bindings stuff
typedef struct {
    const char *name;
    int id;
} keydef_t;

extern keydef_t keynames[];
extern const char *mousenames[];
extern hashtable_t h_gamefuncs;

#include "funct.h"

#ifdef __cplusplus
}
#endif

enum loguru_verbosities_game
{
    LOG_VM = LOG_ENGINE_MAX,
    LOG_CON,
    LOG_GAME_MAX,
};

#include "_rts.h"
#include "actors.h"
#include "chatpipe.h"
#include "common_game.h"
#include "cmdline.h"
#include "dnames.h"
#include "dukebot.h"
#include "game.h"
#include "gamedef.h"
#include "gameexec.h"
#include "gametypes.h"
#include "gamevars.h"
#include "global.h"
#include "hud.h"
#include "inv.h"
#include "macros.h"
#include "music.h"
#include "net_predict.h"
#include "oldnet.h"
#include "player.h"
#include "premap.h"
#include "projectiles.h"
#include "quotes.h"
#include "rts.h"
#include "screens.h"
#include "sector.h"
#include "sounds.h"
#include "sync.h"

static inline int32_t gameHandleEvents(void)
{
    Net_GetPackets();
    return handleevents();
}

static inline int32_t G_TileHasActor(int const tileNum)
{
    return g_tile[tileNum].execPtr != NULL;
}

static inline int32_t G_DefaultActorHealthForTile(int const tileNum)
{
    return G_TileHasActor(tileNum) ? *g_tile[tileNum].execPtr : 0;
}

#endif

