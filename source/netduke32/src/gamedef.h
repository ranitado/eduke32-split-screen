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

#ifndef _gamedef_h_
#define _gamedef_h_

#include "actors.h"
#include "build.h"  // hashtable_t
#include "common.h"  // tokenlist
#include "player.h"  // projectile_t

#define LABEL_CHAR sizeof(uint8_t)
#define LABEL_SHORT sizeof(uint16_t)
#define LABEL_INT sizeof(uint32_t)
#define LABEL_UNSIGNED 8
#define LABEL_HASPARM2 16
#define LABEL_ISSTRING 32
#define LABEL_READFUNC 64
#define LABEL_WRITEFUNC 128

#define MAXLABELS 16384
#define LAST_LABEL (label+(g_labelCnt<<6))

#define VM_IFELSE_MAGIC_BIT (1<<31)
#define VM_INSTMASK 0xfff

#define VM_DECODE_INST(xxx) ((int)((xxx) & VM_INSTMASK))
#define VM_DECODE_LINE_NUMBER(xxx) ((int)((xxx&~VM_IFELSE_MAGIC_BIT) >> 12))

#define C_CUSTOMERROR(Text, ...)                                                               \
    do                                                                                         \
    {                                                                                          \
        C_ReportError(-1);                                                                     \
        LOG_F(ERROR, "%s:%d: " Text, g_scriptFileName, g_lineNumber, ##__VA_ARGS__); \
        g_errorCnt++;                                                                          \
    } while (0)

#define C_CUSTOMWARNING(Text, ...)                                                               \
    do                                                                                           \
    {                                                                                            \
        C_ReportError(-1);                                                                       \
        LOG_F(WARNING, "%s:%d: " Text, g_scriptFileName, g_lineNumber, ##__VA_ARGS__); \
        g_warningCnt++;                                                                          \
    } while (0)

typedef struct SystemVarID_s {
    int32_t RETURN          = -1;
    int32_t WEAPON          = -1;
    int32_t WORKSLIKE       = -1;
    int32_t ZRANGE          = -1;
    int32_t ANGRANGE        = -1;
    int32_t AUTOAIMANGLE    = -1;
    int32_t LOTAG           = -1;
    int32_t HITAG           = -1;
    int32_t TEXTURE         = -1;
    int32_t THISACTOR       = -1;

    int32_t RESPAWN_MONSTERS    = -1;
    int32_t RESPAWN_ITEMS       = -1;
    int32_t RESPAWN_INVENTORY   = -1;
    int32_t MONSTERS_OFF        = -1;
    int32_t MARKER              = -1;
    int32_t FFIRE               = -1;

    int32_t structs = -1;
} SystemVarID_t;
extern SystemVarID_t sysVarIDs;

extern const char* EventNames[MAXEVENTS];
extern intptr_t apScriptEvents[MAXEVENTS];
extern int32_t g_currentEvent;

extern int otherp;
extern int g_currentweapon;
extern int g_gun_pos;
extern int g_looking_arc;
extern int g_currentweapon;
extern int g_weapon_xoffset;
extern int g_gs;
extern int g_kb;
extern int g_looking_angSR1;
extern char g_scriptFileName[BMAX_PATH];
extern int g_totalLines,g_lineNumber;
extern int g_errorCnt,g_warningCnt;

extern uint8_t* bitptr;

typedef struct {
    int spriteNum;
    int playerNum;
    int playerDist;
    int flags;

    union {
        spritetype* pSprite;
        uspritetype* pUSprite;
    };

    int32_t* pData;
    DukePlayer_t* pPlayer;
    ActorData_t* pActor;
} vmstate_t;
extern vmstate_t vm;

typedef struct {
    const char* token;
    int32_t val;
} tokenmap_t;
extern const tokenmap_t iter_tokens[];

struct vmofs
{
    char* fn;
    struct vmofs* next;
    int offset;
};
extern struct vmofs* vmoffset;

static inline const char* C_GetFileForOffset(int const offset)
{
    auto ofs = vmoffset;
    while (ofs->offset > offset)
        ofs = ofs->next;
    return ofs->fn;
}
#define VM_FILENAME(xxx) C_GetFileForOffset((xxx)-apScript)

#define CON_ERROR OSD_ERROR "Line %d, %s: "

extern int g_scriptSanityChecks;
extern int g_errorLineNum;
extern int g_tw;
extern const tokenmap_t vm_keywords[];

extern hashtable_t h_gamevars;
extern hashtable_t h_arrays;

void C_Compile(const char* fn);
char const* VM_GetKeywordForID(int32_t id);

// Enums
enum
{
    LABEL_ACTION = 0x01,
    LABEL_ACTOR = 0x02,
    LABEL_AI = 0x04,
    LABEL_DEFINE = 0x08,
    LABEL_EVENT = 0x10,
    LABEL_MOVE = 0x20,
    LABEL_STATE = 0x40,

    LABEL_ANY = -1,
};

// KEEPINSYNC gamevars.cpp: "special vars for struct access"
enum QuickStructureAccess_t
{
    STRUCT_SPRITE,
    STRUCT_SPRITE_INTERNAL__,
    STRUCT_ACTOR_INTERNAL__,
    STRUCT_SPRITEEXT_INTERNAL__,
    STRUCT_SECTOR,
    STRUCT_SECTOR_INTERNAL__,
    STRUCT_WALL,
    STRUCT_WALL_INTERNAL__,
    STRUCT_PLAYER,
    STRUCT_PLAYER_INTERNAL__,
    STRUCT_ACTORVAR,
    STRUCT_PLAYERVAR,
    STRUCT_TSPR,
    STRUCT_PROJECTILE,
    STRUCT_THISPROJECTILE,
    STRUCT_USERDEF,
    STRUCT_INPUT,
    STRUCT_TILEDATA,
    //STRUCT_PALDATA,
    NUMQUICKSTRUCTS,
};

enum ScriptError_t
{
    ERROR_CLOSEBRACKET,
    ERROR_EVENTONLY,
    ERROR_EXCEEDSMAXPALS,
    ERROR_EXCEEDSMAXTILES,
    ERROR_EXPECTEDKEYWORD,
    ERROR_FOUNDWITHIN,
    ERROR_ISAKEYWORD,
    ERROR_NOENDSWITCH,
    ERROR_NOTAGAMEDEF,
    ERROR_NOTAGAMEVAR,
    ERROR_NOTAGAMEARRAY,
    ERROR_NOTAMEMBER,
    ERROR_GAMEARRAYBNC,
    ERROR_GAMEARRAYBNO,
    ERROR_INVALIDARRAYWRITE,
    ERROR_OPENBRACKET,
    ERROR_PARAMUNDEFINED,
    ERROR_SYMBOLNOTRECOGNIZED,
    ERROR_SYNTAXERROR,
    ERROR_VARREADONLY,
    ERROR_VARTYPEMISMATCH,
    WARNING_BADGAMEVAR,
    WARNING_DUPLICATECASE,
    WARNING_DUPLICATEDEFINITION,
    WARNING_EVENTSYNC,
    WARNING_LABELSONLY,
    WARNING_NAMEMATCHESVAR,
    WARNING_REVEVENTSYNC
};

enum ScriptKeywords_t
{
    // ---
    // Specialized setvar keywords
    CON_SETVAR_GLOBAL,
    CON_SETVAR_PLAYER,
    CON_SETVAR_ACTOR,
    // ---

    CON_ADDAMMO,
    CON_IFRND,
    CON_ENDA,
    CON_IFCANSEE,
    CON_IFHITWEAPON,
    CON_ACTION,
    CON_IFPDISTL,
    CON_IFPDISTG,
    CON_ELSE,
    CON_STRENGTH,
    CON_BREAK,
    CON_SHOOT,
    CON_PALFROM,
    CON_SOUND,
    CON_FALL,
    CON_STATE,
    CON_ENDS,
    CON_COMMENT,
    CON_IFAI,
    CON_KILLIT,
    CON_ADDWEAPON,
    CON_AI,
    CON_ADDPHEALTH,
    CON_IFDEAD,
    CON_IFSQUISHED,
    CON_SIZETO,
    CON_LEFTBRACE,
    CON_RIGHTBRACE,
    CON_SPAWN,
    CON_MOVE,
    CON_IFWASWEAPON,
    CON_IFACTION,
    CON_IFACTIONCOUNT,
    CON_RESETACTIONCOUNT,
    CON_DEBRIS,
    CON_PSTOMP,
    CON_BLOCKCOMMENT,
    CON_CSTAT,
    CON_IFMOVE,
    CON_RESETPLAYER,
    CON_IFONWATER,
    CON_IFINWATER,
    CON_IFCANSHOOTTARGET,
    CON_IFCOUNT,
    CON_RESETCOUNT,
    CON_ADDINVENTORY,
    CON_IFACTORNOTSTAYPUT,
    CON_HITRADIUS,
    CON_IFP,
    CON_COUNT,
    CON_IFACTOR,
    CON_IFSTRENGTH,
    CON_GUTS,
    CON_IFSPAWNEDBY,
    CON_WACKPLAYER,
    CON_IFGAPZL,
    CON_IFHITSPACE,
    CON_IFOUTSIDE,
    CON_IFMULTIPLAYER,
    CON_OPERATE,
    CON_IFINSPACE,
    CON_DEBUG,
    CON_ENDOFGAME,
    CON_IFBULLETNEAR,
    CON_IFRESPAWN,
    CON_IFFLOORDISTL,
    CON_IFCEILINGDISTL,
    CON_SPRITEPAL,
    CON_IFPINVENTORY,
    CON_CACTOR,
    CON_IFPHEALTHL,
    CON_QUOTE,
    CON_IFINOUTERSPACE,
    CON_IFNOTMOVING,
    CON_RESPAWNHITAG,
    CON_TIP,
    CON_IFSPRITEPAL,
    CON_MONEY,
    CON_SOUNDONCE,
    CON_ADDKILLS,
    CON_STOPSOUND,
    CON_IFAWAYFROMWALL,
    CON_IFCANSEETARGET,
    CON_GLOBALSOUND,
    CON_SCREENSOUND,
    CON_LOTSOFGLASS,
    CON_IFGOTWEAPONCE,
    CON_GETLASTPAL,
    CON_PKICK,
    CON_MIKESND,
    CON_SIZEAT,
    CON_ADDSTRENGTH,
    CON_CSTATOR,
    CON_MAIL,
    CON_PAPER,
    CON_TOSSWEAPON,
    CON_SLEEPTIME,
    CON_NULLOP,
    CON_IFNOSOUNDS,
    CON_CLIPDIST,
    CON_IFANGDIFFL,

    CON_IFVARN,
    CON_IFVARL,
    CON_IFVARLE,
    CON_IFVARG,
    CON_IFVARGE,
    CON_IFVARAND,
    CON_IFVAROR,
    CON_IFVARXOR,
    CON_IFVAREITHER,

    CON_IFVARVARN,
    CON_IFVARVARL,
    CON_IFVARVARLE,
    CON_IFVARVARG,
    CON_IFVARVARGE,
    CON_IFVARVARAND,
    CON_IFVARVAROR,
    CON_IFVARVARXOR,
    CON_IFVARVAREITHER,
    CON_IFACTORSOUND,

    CON_SETVARVAR,
    CON_SETVAR,
    CON_ADDVARVAR,
    CON_ADDVAR,
    CON_FOR,

    CON_ADDLOGVAR,
    CON_ADDLOG,
    CON_ENDEVENT,
    CON_IFVARE,
    CON_IFVARVARE,
    CON_SPGETLOTAG,         // !DEPRECATED!
    CON_SPGETHITAG,         // !DEPRECATED!
    CON_SECTGETLOTAG,       // !DEPRECATED!
    CON_SECTGETHITAG,       // !DEPRECATED!
    CON_IFSOUND,
    CON_GETTEXTUREFLOOR,
    CON_GETTEXTURECEILING,
    CON_INITTIMER,
    CON_STARTTRACK,
    CON_RANDVAR,
    CON_GETANGLETOTARGET,
    CON_GETACTORANGLE,
    CON_SETACTORANGLE,
    CON_MULVAR,
    CON_MULVARVAR,
    CON_DIVVAR,
    CON_DIVVARVAR,
    CON_MODVAR,
    CON_MODVARVAR,
    CON_ANDVAR,
    CON_ANDVARVAR,
    CON_ORVAR,
    CON_ORVARVAR,
    CON_GETPLAYERANGLE,
    CON_SETPLAYERANGLE,
    CON_LOCKPLAYER,

    CON_SETSECTOR,
    CON_GETSECTOR,
    CON_SETSECTORSTRUCT,
    CON_GETSECTORSTRUCT,

    CON_SETACTOR,
    CON_GETACTOR,
    CON_SETACTORSTRUCT,
    CON_GETACTORSTRUCT,
    CON_GETSPRITEEXT,
    CON_SETSPRITEEXT,
    CON_GETSPRITESTRUCT,
    CON_SETSPRITESTRUCT,

    CON_SETWALL,
    CON_GETWALL,
    CON_SETWALLSTRUCT,
    CON_GETWALLSTRUCT,

    CON_SETACTORVAR,
    CON_GETACTORVAR,
    
    CON_GETPLAYER,
    CON_SETPLAYER,
    CON_GETPLAYERSTRUCT,
    CON_SETPLAYERSTRUCT,

    CON_FINDNEARACTOR,
    CON_FINDNEARACTORVAR,

    CON_ESPAWN,
    CON_SQRT,
    CON_ESPAWNVAR,
    CON_GETUSERDEF,
    CON_SETUSERDEF,
    CON_SUBVARVAR,
    CON_SUBVAR,
    CON_MYOS,
    CON_MYOSPAL,
    CON_DISPLAYRAND,
    CON_SIN,
    CON_XORVARVAR,
    CON_XORVAR,
    CON_RANDVARVAR,
    CON_MYOSX,
    CON_MYOSPALX,
    CON_GMAXAMMO,
    CON_SMAXAMMO,
    CON_STARTLEVEL,
    CON_ESHOOT,
    CON_QSPAWN,
    CON_ROTATESPRITE,
    CON_COS,
    CON_FINDNEARACTOR3D,
    CON_FINDNEARACTOR3DVAR,
    CON_FLASH,
    CON_QSPAWNVAR,
    CON_EQSPAWN,
    CON_EQSPAWNVAR,
    CON_MINITEXT,
    CON_GAMETEXT,
    CON_DIGITALNUMBER,
    CON_ADDWEAPONVAR,
    CON_SETPROJECTILE,
    CON_ANGOFF,
    CON_UPDATESECTOR,
    CON_INSERTSPRITEQ,
    CON_ANGOFFVAR,
    CON_WHILEVARN,
    CON_SWITCH,
    CON_ENDSWITCH,
    CON_FINDPLAYER,
    CON_FINDOTHERPLAYER,
    CON_ACTIVATEBYSECTOR,
    CON_OPERATESECTORS,
    CON_OPERATERESPAWNS,
    CON_OPERATEACTIVATORS,
    CON_OPERATEMASTERSWITCHES,
    CON_CHECKACTIVATORMOTION,
    CON_ZSHOOT,
    CON_DIST,
    CON_LDIST,
    CON_SHIFTVARL,
    CON_SHIFTVARR,
    CON_GETANGLE,
    CON_WHILEVARVARN,
    CON_HITSCAN,
    CON_TIME,
    CON_GETPLAYERVAR,
    CON_SETPLAYERVAR,
    CON_MULSCALE,
    CON_SETASPECT,
    CON_EZSHOOT,
    CON_MOVESPRITE,
    CON_CHECKAVAILWEAPON,
    CON_SOUNDONCEVAR,
    CON_UPDATESECTORZ,
    CON_STOPALLSOUNDS,
    CON_SSP,
    CON_STOPSOUNDVAR,
    CON_DISPLAYRANDVAR,
    CON_DISPLAYRANDVARVAR,
    CON_CHECKAVAILINVEN,
    CON_GUNIQHUDID,
    CON_GETPROJECTILE,
    CON_GETTHISPROJECTILE,
    CON_SETTHISPROJECTILE,
    CON_USERQUOTE,
    CON_REDEFINEQUOTE,
    CON_QSPRINTF,
    CON_GETPNAME,
    CON_QSTRCAT,
    CON_QSTRCPY,
    CON_SETSPRITE,
    CON_ROTATEPOINT,
    CON_DRAGPOINT,
    CON_GETZRANGE,
    CON_CHANGESPRITESTAT,
    CON_GETCEILZOFSLOPE,
    CON_GETFLORZOFSLOPE,
    CON_NEARTAG,
    CON_CHANGESPRITESECT,
    CON_SPRITEFLAGS,
    CON_SAVEGAMEVAR,
    CON_READGAMEVAR,
    CON_FINDNEARSPRITE,
    CON_FINDNEARSPRITEVAR,
    CON_FINDNEARSPRITE3D,
    CON_FINDNEARSPRITE3DVAR,
    CON_SETINPUT,
    CON_GETINPUT,
    CON_SAVE,
    CON_CANSEE,
    CON_CANSEESPR,
    CON_FINDNEARACTORZ,
    CON_FINDNEARACTORZVAR,
    CON_FINDNEARSPRITEZ,
    CON_FINDNEARSPRITEZVAR,
    CON_GETCURRADDRESS,
    CON_JUMP,
    CON_QSTRLEN,
    CON_GETINCANGLE,
    CON_QUAKE,
    CON_SHOWVIEW,
    CON_HEADSPRITESTAT,
    CON_PREVSPRITESTAT,
    CON_NEXTSPRITESTAT,
    CON_HEADSPRITESECT,
    CON_PREVSPRITESECT,
    CON_NEXTSPRITESECT,
    CON_GETKEYNAME,
    CON_QSUBSTR,
    CON_GAMETEXTZ,
    CON_DIGITALNUMBERZ,
    CON_HITRADIUSVAR,
    CON_ROTATESPRITE16,
    CON_SETARRAY,
    CON_RESIZEARRAY,
    CON_WRITEARRAYTOFILE,
    CON_READARRAYFROMFILE,
    CON_QGETSYSSTR,
    CON_GETTICKS,
    CON_GETTSPR,
    CON_SETTSPR,
    CON_SAVEMAPSTATE,
    CON_LOADMAPSTATE,
    CON_CLEARMAPSTATE,
    CON_CMENU,
    CON_GETTIMEDATE, 
    CON_ACTIVATECHEAT,
    CON_SETGAMEPALETTE,

    CON_GETARRAYSIZE,
    CON_SAVENN,
    CON_QSTRDIM,
    CON_SCREENTEXT,
    CON_RETURN,
    CON_ACTORSOUND,
    CON_ROTATESPRITEA,
    CON_GETTEAMNAME,
    CON_KLABS,
    CON_SHIFTVARVARL,
    CON_SHIFTVARVARR,
    CON_GETTILEDATA,
    CON_SETTILEDATA,
    CON_INTERPOLATE,
    CON_LDISTVAR,
    CON_DISTVAR,
    CON_SECTSETINTERPOLATION,
    CON_SECTCLEARINTERPOLATION,
    CON_STOPACTORSOUND,
    CON_INV,
    CON_OPCODE_END,
    // ---

    // ---
    // these are the keywords that don't have instructions written into the bytecode
    // ie: parsed in C_ParseCommand, but no code in VM_DoExecute.
    CON_ACTOR,
    CON_BETANAME, // !DEPRECATED!
    CON_CASE,
    CON_CHEATKEYS,
    CON_DAMAGEEVENTTILE,
    CON_DAMAGEEVENTTILERANGE,
    CON_DEFAULT,
    CON_DEFINE,
    CON_DEFINECHEAT,
    CON_DEFINEGAMEFUNCNAME,
    CON_DEFINEGAMETYPE,
    CON_DEFINELEVELNAME,
    CON_DEFINEPROJECTILE,
    CON_DEFINEQUOTE,
    CON_DEFINESKILLNAME,
    CON_DEFINESOUND,
    CON_DEFINEVOLUMENAME,
    CON_DEFSTATE,
    CON_DYNAMICREMAP,
    CON_DYNAMICSOUNDREMAP,
    CON_ENHANCED, // !DEPRECATED!
    CON_EVENTLOADACTOR,
    CON_GAMEARRAY,
    CON_GAMESTARTUP,
    CON_GAMEVAR,
    CON_INCLUDE,
    CON_MUSIC,

    CON_ONEVENT,
    CON_APPENDEVENT,

    CON_PRECACHE,
    CON_SCRIPTSIZE,
    CON_SETCFGNAME,
    CON_SETDEFNAME,
    CON_SETGAMENAME,
    CON_SPRITENOPAL,
    CON_SPRITENOSHADE,
    CON_SPRITENVG,
    CON_SPRITESHADOW,
    CON_USERACTOR,
    // ---

    CON_END
};

enum IterationTypes_t
{
    ITER_ALLSPRITES,
    ITER_ALLSECTORS,
    ITER_ALLWALLS,
    ITER_ACTIVELIGHTS,
    ITER_DRAWNSPRITES,
    // ---
    ITER_SPRITESOFSECTOR,
    ITER_SPRITESOFSTATUS,
    ITER_WALLSOFSECTOR,
    ITER_LOOPOFWALL,
    ITER_RANGE,
    ITER_ALLSPRITESBYSTAT,
    ITER_ALLSPRITESBYSECT,
    ITER_END
};
#endif
