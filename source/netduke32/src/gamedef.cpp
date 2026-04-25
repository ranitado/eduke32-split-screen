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

#include "duke3d.h"
#include "gamedef.h"
#include "kplib.h"
#include "utils.h"

#include "osd.h"

#include "gamestructures.h"

#define LINE_NUMBER (g_lineNumber << 12)

int g_scriptVersion = 13; // 13 = 1.3D-style CON files, 14 = 1.4/1.5 style CON files

char g_scriptFileName[BMAX_PATH] = "(none)";  // file we're currently compiling
static char g_szCurrentBlockName[BMAX_PATH] = "(none)", g_szLastBlockName[BMAX_PATH] = "NULL";

int g_totalLines,g_lineNumber;
static int g_checkingIfElse,g_processingState;
char g_szBuf[1024];

// Switch/Case
intptr_t* g_caseTablePtr = NULL; // the pointer to the start of the case table in a switch statement. first entry is 'default' code.
static int g_checkingSwitch = 0, g_numCases = 0;
static bool g_switchCountPhase = false;
static bool g_checkingCase;

static bool g_skipBranch;
static bool g_dynamicTileMapping, g_dynamicSoundMapping;
static int g_labelsOnly = 0;
static int g_lastKeyword = -1;
static int g_numBraces = 0;

int g_numXStrings = 0; // Was g_numQuoteRedefinitions

struct vmofs* vmoffset;

// pointers to weapon gamevar data
intptr_t *aplWeaponClip[MAX_WEAPONS];           // number of items in magazine
intptr_t *aplWeaponReload[MAX_WEAPONS];         // delay to reload (include fire)
intptr_t *aplWeaponFireDelay[MAX_WEAPONS];      // delay to fire
intptr_t *aplWeaponHoldDelay[MAX_WEAPONS];      // delay after release fire button to fire (0 for none)
intptr_t *aplWeaponTotalTime[MAX_WEAPONS];      // The total time the weapon is cycling before next fire.
intptr_t *aplWeaponFlags[MAX_WEAPONS];          // Flags for weapon
intptr_t *aplWeaponFlashColor[MAX_WEAPONS];     // Muzzle flash color
intptr_t *aplWeaponShoots[MAX_WEAPONS];         // what the weapon shoots
intptr_t *aplWeaponSpawnTime[MAX_WEAPONS];      // the frame at which to spawn an item
intptr_t *aplWeaponSpawn[MAX_WEAPONS];          // the item to spawn
intptr_t *aplWeaponShotsPerBurst[MAX_WEAPONS];  // number of shots per 'burst' (one ammo per 'burst'
intptr_t *aplWeaponWorksLike[MAX_WEAPONS];      // What original the weapon works like
intptr_t *aplWeaponInitialSound[MAX_WEAPONS];   // Sound made when initialy firing. zero for no sound
intptr_t *aplWeaponFireSound[MAX_WEAPONS];      // Sound made when firing (each time for automatic)
intptr_t *aplWeaponSound2Time[MAX_WEAPONS];     // Alternate sound time
intptr_t *aplWeaponSound2Sound[MAX_WEAPONS];    // Alternate sound sound ID
intptr_t *aplWeaponReloadSound1[MAX_WEAPONS];   // Sound of magazine being removed
intptr_t *aplWeaponReloadSound2[MAX_WEAPONS];   // Sound of magazine being inserted
intptr_t *aplWeaponSelectSound[MAX_WEAPONS];    // Sound of weapon being selected

// Stores the IDs of system vars.
SystemVarID_t sysVarIDs;

// Events
intptr_t apScriptEvents[MAXEVENTS];
static intptr_t apScriptGameEventEnd[MAXEVENTS];
static intptr_t g_scriptEventOffset;
static intptr_t g_scriptEventBreakOffset;
static intptr_t g_scriptEventChainOffset;
int32_t g_currentEvent = -1;

char *textptr;
int g_errorCnt,g_warningCnt;

static char* C_GetLabelType(int const type)
{
    static tokenmap_t const LabelType[] =
    {
        { "action", LABEL_ACTION },
        { "actor",  LABEL_ACTOR },
        { "ai",     LABEL_AI },
        { "define", LABEL_DEFINE },
        { "event",  LABEL_EVENT },
        { "move",   LABEL_MOVE },
        { "state",  LABEL_STATE },
    };

    char x[64] = {};

    for (auto& label : LabelType)
    {
        if ((type & label.val) != label.val)
            continue;

        if (x[0]) Bstrcat(x, " or ");
        Bstrcat(x, label.token);

        if (type == label.val)
            break;
    }

    return Xstrdup(x);
}

#define NUMKEYWORDS (signed int)(sizeof(vm_keywords)/sizeof(vm_keywords[0]))

const tokenmap_t vm_keywords[] =
{
    { "definelevelname",        CON_DEFINELEVELNAME },  // defines level name
    { "actor",                  CON_ACTOR },            // defines an actor
    { "addammo",                CON_ADDAMMO },          // adds ammo to a weapon
    { "ifrnd",                  CON_IFRND },            // checks against a randomizer
    { "enda",                   CON_ENDA },             // ends an actor definition
    { "ifcansee",               CON_IFCANSEE },         // checks if the player can see an object
    { "ifhitweapon",            CON_IFHITWEAPON },      // checks if an object was hit by a weapon
    { "action",                 CON_ACTION },           // defines an action if used outside a state or actor, otherwise triggers actor to perform action
    { "ifpdistl",               CON_IFPDISTL },         // checks if player distance is less than value
    { "ifpdistg",               CON_IFPDISTG },         // checks if player distance is more than value
    { "else",                   CON_ELSE },             // used with if checks
    { "strength",               CON_STRENGTH },         // sets health
    { "break",                  CON_BREAK },            // stops processing

    { "shoot",                  CON_SHOOT },            // shoots a projectile
    { "shootvar",               CON_SHOOT },

    { "palfrom",                CON_PALFROM },          // used for player screen shading effect, sets p->pals_time and p->pals[0-2]
    { "sound",                  CON_SOUND },            // plays a sound that was defined with definesound
    { "fall",                   CON_FALL },             // causes actor to fall to sector floor height
    { "state",                  CON_STATE },            // begins defining a state if used outside a state or actor, otherwise calls a state
    { "ends",                   CON_ENDS },             // ends defining a state
    { "define",                 CON_DEFINE },           // defines a value
    { "ifai",                   CON_IFAI },             // checks if actor is currently performing a specific ai function
    { "killit",                 CON_KILLIT },           // kills an actor
    { "addweapon",              CON_ADDWEAPON },        // adds a weapon to the closest player
    { "ai",                     CON_AI },               // sets an ai function to be used by an actor
    { "addphealth",             CON_ADDPHEALTH },       // adds health to the player
    { "ifdead",                 CON_IFDEAD },           // checks if actor is dead
    { "ifsquished",             CON_IFSQUISHED },       // checks if actor has been squished
    { "sizeto",                 CON_SIZETO },           // gradually increases actor size until it matches parameters given
    { "{",                      CON_LEFTBRACE },        // used to indicate segments of code
    { "}",                      CON_RIGHTBRACE },       // used to indicate segments of code
    { "spawn",                  CON_SPAWN },            // spawns an actor
    { "move",                   CON_MOVE },
    { "ifwasweapon",            CON_IFWASWEAPON },
    { "ifaction",               CON_IFACTION },
    { "ifactioncount",          CON_IFACTIONCOUNT },
    { "resetactioncount",       CON_RESETACTIONCOUNT },
    { "debris",                 CON_DEBRIS },
    { "pstomp",                 CON_PSTOMP },         
    { "cstat",                  CON_CSTAT },
    { "ifmove",                 CON_IFMOVE },
    { "resetplayer",            CON_RESETPLAYER },
    { "ifonwater",              CON_IFONWATER },
    { "ifinwater",              CON_IFINWATER },
    { "ifcanshoottarget",       CON_IFCANSHOOTTARGET },
    { "ifcount",                CON_IFCOUNT },
    { "resetcount",             CON_RESETCOUNT },
    { "addinventory",           CON_ADDINVENTORY },
    { "ifactornotstayput",      CON_IFACTORNOTSTAYPUT },
    { "hitradius",              CON_HITRADIUS },
    { "ifp",                    CON_IFP },
    { "count",                  CON_COUNT },
    { "ifactor",                CON_IFACTOR },
    { "music",                  CON_MUSIC },
    { "include",                CON_INCLUDE },
    { "ifstrength",             CON_IFSTRENGTH },
    { "definesound",            CON_DEFINESOUND },
    { "guts",                   CON_GUTS },
    { "ifspawnedby",            CON_IFSPAWNEDBY },
    { "gamestartup",            CON_GAMESTARTUP },
    { "wackplayer",             CON_WACKPLAYER },
    { "ifgapzl",                CON_IFGAPZL },
    { "ifhitspace",             CON_IFHITSPACE },
    { "ifoutside",              CON_IFOUTSIDE },
    { "ifmultiplayer",          CON_IFMULTIPLAYER },
    { "operate",                CON_OPERATE },
    { "ifinspace",              CON_IFINSPACE },
    { "debug",                  CON_DEBUG },
    { "endofgame",              CON_ENDOFGAME },
    { "ifbulletnear",           CON_IFBULLETNEAR },
    { "ifrespawn",              CON_IFRESPAWN },
    { "iffloordistl",           CON_IFFLOORDISTL },
    { "ifceilingdistl",         CON_IFCEILINGDISTL },
    { "spritepal",              CON_SPRITEPAL },
    { "ifpinventory",           CON_IFPINVENTORY },
    { "betaname",               CON_BETANAME },
    { "cactor",                 CON_CACTOR },
    { "ifphealthl",             CON_IFPHEALTHL },
    { "definequote",            CON_DEFINEQUOTE },
    { "quote",                  CON_QUOTE },
    { "ifinouterspace",         CON_IFINOUTERSPACE },
    { "ifnotmoving",            CON_IFNOTMOVING },
    { "respawnhitag",           CON_RESPAWNHITAG },
    { "tip",                    CON_TIP },
    { "ifspritepal",            CON_IFSPRITEPAL },
    { "money",                  CON_MONEY },
    { "soundonce",              CON_SOUNDONCE },
    { "addkills",               CON_ADDKILLS },
    { "stopsound",              CON_STOPSOUND },
    { "ifawayfromwall",         CON_IFAWAYFROMWALL },
    { "ifcanseetarget",         CON_IFCANSEETARGET },

    { "soundvar",               CON_SOUND },
    { "globalsound",            CON_GLOBALSOUND },
    { "globalsoundvar",         CON_GLOBALSOUND },
    { "screensound",            CON_SCREENSOUND },

    { "lotsofglass",            CON_LOTSOFGLASS },
    { "ifgotweaponce",          CON_IFGOTWEAPONCE },
    { "getlastpal",             CON_GETLASTPAL },
    { "pkick",                  CON_PKICK },
    { "mikesnd",                CON_MIKESND },
    { "useractor",              CON_USERACTOR },
    { "sizeat",                 CON_SIZEAT },
    { "addstrength",            CON_ADDSTRENGTH },
    { "cstator",                CON_CSTATOR },
    { "mail",                   CON_MAIL },
    { "paper",                  CON_PAPER },
    { "tossweapon",             CON_TOSSWEAPON },
    { "sleeptime",              CON_SLEEPTIME },
    { "nullop",                 CON_NULLOP },
    { "definevolumename",       CON_DEFINEVOLUMENAME },
    { "defineskillname",        CON_DEFINESKILLNAME },
    { "ifnosounds",             CON_IFNOSOUNDS },
    { "clipdist",               CON_CLIPDIST },
    { "ifangdiffl",             CON_IFANGDIFFL },

    { "gamevar",                CON_GAMEVAR },
    { "var",                    CON_GAMEVAR },

    { "ifvarl",                 CON_IFVARL },
    { "ifvarvarl",              CON_IFVARVARL },
    { "ifl",                    CON_IFVARVARL },

    { "ifvarle",                CON_IFVARLE },
    { "ifvarvarle",             CON_IFVARVARLE },
    { "ifle",                   CON_IFVARVARLE },

    { "ifvarg",                 CON_IFVARG },
    { "ifvarvarg",              CON_IFVARVARG },
    { "ifg",                    CON_IFVARVARG },

    { "ifvarge",                CON_IFVARGE },
    { "ifvarvarge",             CON_IFVARVARGE },
    { "ifge",                   CON_IFVARVARGE },

    { "ifvare",                 CON_IFVARE },
    { "ifvarvare",              CON_IFVARVARE },
    { "ife",                    CON_IFVARVARE },

    { "ifvarn",                 CON_IFVARN },
    { "ifvarvarn",              CON_IFVARVARN },
    { "ifn",                    CON_IFVARVARN },

    { "ifvarand",               CON_IFVARAND },
    { "ifvarvarand",            CON_IFVARVARAND },
    { "ifand",                  CON_IFVARVARAND },

    { "ifvaror",                CON_IFVAROR },
    { "ifvarvaror",             CON_IFVARVAROR },
    { "ifor",                   CON_IFVARVAROR },

    { "ifvarxor",               CON_IFVARXOR },
    { "ifvarvarxor",            CON_IFVARVARXOR },
    { "ifxor",                  CON_IFVARVARXOR },

    { "ifvareither",            CON_IFVAREITHER },
    { "ifvarvareither",         CON_IFVARVAREITHER },
    { "ifeither",               CON_IFVARVAREITHER },

    { "ifactorsound",           CON_IFACTORSOUND },

    { "for",                    CON_FOR },

    { "whilevarn",              CON_WHILEVARN },
    { "whilevarvarn",           CON_WHILEVARVARN },
    { "whilen",                 CON_WHILEVARVARN },

    { "setvar",                 CON_SETVAR },
    { "setvarvar",              CON_SETVARVAR },
    { "set",                    CON_SETVARVAR },

    { "addvar",                 CON_ADDVAR },
    { "addvarvar",              CON_ADDVARVAR },
    { "add",                    CON_ADDVARVAR },

    { "subvar",                 CON_SUBVAR },
    { "subvarvar",              CON_SUBVARVAR },
    { "sub",                    CON_SUBVARVAR },

    { "mulvar",                 CON_MULVAR },
    { "mulvarvar",              CON_MULVARVAR },
    { "mul",                    CON_MULVARVAR },

    { "divvar",                 CON_DIVVAR },
    { "divvarvar",              CON_DIVVARVAR },
    { "div",                    CON_DIVVARVAR },

    { "modvar",                 CON_MODVAR },
    { "modvarvar",              CON_MODVARVAR },
    { "mod",                    CON_MODVARVAR },

    { "andvar",                 CON_ANDVAR },
    { "andvarvar",              CON_ANDVARVAR },
    { "and",                    CON_ANDVARVAR },

    { "orvar",                  CON_ORVAR },
    { "orvarvar",               CON_ORVARVAR },
    { "or",                     CON_ORVARVAR },

    { "xorvar",                 CON_XORVAR },
    { "xorvarvar",              CON_XORVARVAR },
    { "xor",                    CON_XORVARVAR },

    { "shiftvarl",              CON_SHIFTVARL },
    { "shiftvarvarl",           CON_SHIFTVARVARL },
    { "shiftl",                 CON_SHIFTVARVARL },

    { "shiftvarr",              CON_SHIFTVARR },
    { "shiftvarvarr",           CON_SHIFTVARVARR },
    { "shiftr",                 CON_SHIFTVARVARR },

    { "randvar",                CON_RANDVAR },
    { "randvarvar",             CON_RANDVARVAR },
    { "displayrandvar",         CON_DISPLAYRANDVAR },
    { "displayrandvarvar",      CON_DISPLAYRANDVARVAR },
    
    { "addlogvar",              CON_ADDLOGVAR },
    { "addlog",                 CON_ADDLOG },

    { "onevent",                CON_ONEVENT },
    { "appendevent",            CON_APPENDEVENT },
    { "endevent",               CON_ENDEVENT },
    
    { "spgetlotag",             CON_SPGETLOTAG },
    { "spgethitag",             CON_SPGETHITAG },
    { "sectgetlotag",           CON_SECTGETLOTAG },
    { "sectgethitag",           CON_SECTGETHITAG },
    { "ifsound",                CON_IFSOUND },
    { "gettexturefloor",        CON_GETTEXTUREFLOOR },
    { "gettextureceiling",      CON_GETTEXTURECEILING },
    { "inittimer",              CON_INITTIMER },

    { "starttrack",             CON_STARTTRACK }, 
    { "starttrackvar",          CON_STARTTRACK },

    { "enhanced",               CON_ENHANCED },
    { "getangletotarget",       CON_GETANGLETOTARGET },
    { "getactorangle",          CON_GETACTORANGLE },
    { "setactorangle",          CON_SETACTORANGLE },
    { "getplayerangle",         CON_GETPLAYERANGLE },
    { "setplayerangle",         CON_SETPLAYERANGLE },
    { "lockplayer",             CON_LOCKPLAYER },

    { "setsector",              CON_SETSECTOR },
    { "getsector",              CON_GETSECTOR },
    { "setactor",               CON_SETACTOR },
    { "getactor",               CON_GETACTOR },
    { "setactorvar",            CON_SETACTORVAR },
    { "getactorvar",            CON_GETACTORVAR },
    { "setwall",                CON_SETWALL },
    { "getwall",                CON_GETWALL },
    { "setplayer",              CON_SETPLAYER },
    { "getplayer",              CON_GETPLAYER },
    { "setuserdef",             CON_SETUSERDEF },
    { "getuserdef",             CON_GETUSERDEF },
    { "settspr",                CON_SETTSPR },
    { "gettspr",                CON_GETTSPR },
    { "getinput",               CON_GETINPUT },
    { "setinput",               CON_SETINPUT },
    { "settiledata",            CON_SETTILEDATA },
    { "gettiledata",            CON_GETTILEDATA },

    { "findnearactor",          CON_FINDNEARACTOR },
    { "findnearactorvar",       CON_FINDNEARACTORVAR },

    { "espawn",                 CON_ESPAWN },
    { "espawnvar",              CON_ESPAWNVAR },

    { "sectsetinterpolation",   CON_SECTSETINTERPOLATION },
    { "sectclearinterpolation", CON_SECTCLEARINTERPOLATION },
    
    { "sin",                    CON_SIN },
    { "cos",                    CON_COS },
    { "sqrt",                   CON_SQRT },
    { "inv",                    CON_INV },

    { "eventloadactor",         CON_EVENTLOADACTOR },
    { "myos",                   CON_MYOS },
    { "myospal",                CON_MYOSPAL },
    { "displayrand",            CON_DISPLAYRAND },
    { "myosx",                  CON_MYOSX },
    { "myospalx",               CON_MYOSPALX },
    { "gmaxammo",               CON_GMAXAMMO },
    { "smaxammo",               CON_SMAXAMMO },
    { "startlevel",             CON_STARTLEVEL },

    { "qspawn",                 CON_QSPAWN },
    { "rotatesprite",           CON_ROTATESPRITE },
    { "defineprojectile",       CON_DEFINEPROJECTILE },
    { "spriteshadow",           CON_SPRITESHADOW },
    { "findnearactor3d",        CON_FINDNEARACTOR3D },
    { "findnearactor3dvar",     CON_FINDNEARACTOR3DVAR },
    { "flash",                  CON_FLASH },
    { "qspawnvar",              CON_QSPAWNVAR },
    { "eqspawn",                CON_EQSPAWN },
    { "eqspawnvar",             CON_EQSPAWNVAR },
    { "minitext",               CON_MINITEXT },
    { "gametext",               CON_GAMETEXT },
    { "digitalnumber",          CON_DIGITALNUMBER },
    { "addweaponvar",           CON_ADDWEAPONVAR },
    { "setprojectile",          CON_SETPROJECTILE },
    { "angoff",                 CON_ANGOFF },
    { "updatesector",           CON_UPDATESECTOR },
    { "insertspriteq",          CON_INSERTSPRITEQ },
    { "angoffvar",              CON_ANGOFFVAR },
    
    { "switch",                 CON_SWITCH },
    { "case",                   CON_CASE },
    { "default",                CON_DEFAULT },
    { "endswitch",              CON_ENDSWITCH },
    { "findplayer",             CON_FINDPLAYER },
    { "findotherplayer",        CON_FINDOTHERPLAYER },
    { "activatebysector",       CON_ACTIVATEBYSECTOR },         // sectnum, spriteid
    { "operatesectors",         CON_OPERATESECTORS },           // sectnum, spriteid
    { "operaterespawns",        CON_OPERATERESPAWNS },          // lotag
    { "operateactivators",      CON_OPERATEACTIVATORS },        // lotag, player index
    { "operatemasterswitches",  CON_OPERATEMASTERSWITCHES },    // lotag
    { "checkactivatormotion",   CON_CHECKACTIVATORMOTION },     // lotag
    { "zshoot",                 CON_ZSHOOT },                   // zvar projnum

    { "dist",                   CON_DIST },                     // sprite1 sprite2
    { "distvar",                CON_DISTVAR },
    { "ldist",                  CON_LDIST },                    // sprite1 sprite2
    { "ldistvar",               CON_LDISTVAR },

    { "spritenvg",              CON_SPRITENVG },
    { "getangle",               CON_GETANGLE },

    { "hitscan",                CON_HITSCAN },
    { "time",                   CON_TIME },
    { "getplayervar",           CON_GETPLAYERVAR },
    { "setplayervar",           CON_SETPLAYERVAR },
    { "mulscale",               CON_MULSCALE },
    { "setaspect",              CON_SETASPECT },
    { "ezshoot",                CON_EZSHOOT },
    { "spritenoshade",          CON_SPRITENOSHADE },
    { "movesprite",             CON_MOVESPRITE },
    { "checkavailweapon",       CON_CHECKAVAILWEAPON },
    { "soundoncevar",           CON_SOUNDONCEVAR },
    { "updatesectorz",          CON_UPDATESECTORZ },
    { "stopallsounds",          CON_STOPALLSOUNDS },
    { "ssp",                    CON_SSP },
    { "stopsoundvar",           CON_STOPSOUNDVAR },
    { "checkavailinven",        CON_CHECKAVAILINVEN },
    { "guniqhudid",             CON_GUNIQHUDID },
    { "getprojectile",          CON_GETPROJECTILE },
    { "getthisprojectile",      CON_GETTHISPROJECTILE },
    { "setthisprojectile",      CON_SETTHISPROJECTILE },
    { "definecheat",            CON_DEFINECHEAT },
    { "cheatkeys",              CON_CHEATKEYS },
    { "userquote",              CON_USERQUOTE },
    { "precache",               CON_PRECACHE },
    { "definegamefuncname",     CON_DEFINEGAMEFUNCNAME },
    { "redefinequote",          CON_REDEFINEQUOTE },
    { "qsprintf",               CON_QSPRINTF },
    { "getpname",               CON_GETPNAME },
    { "qstrcat",                CON_QSTRCAT },
    { "qstrcpy",                CON_QSTRCPY },
    { "setsprite",              CON_SETSPRITE },
    { "rotatepoint",            CON_ROTATEPOINT },
    { "dragpoint",              CON_DRAGPOINT },
    { "getzrange",              CON_GETZRANGE },
    { "changespritestat",       CON_CHANGESPRITESTAT },
    { "getceilzofslope",        CON_GETCEILZOFSLOPE },
    { "getflorzofslope",        CON_GETFLORZOFSLOPE },
    { "neartag",                CON_NEARTAG },
    { "definegametype",         CON_DEFINEGAMETYPE },
    { "changespritesect",       CON_CHANGESPRITESECT },
    { "spriteflags",            CON_SPRITEFLAGS },
    { "savegamevar",            CON_SAVEGAMEVAR },
    { "readgamevar",            CON_READGAMEVAR },
    { "findnearsprite",         CON_FINDNEARSPRITE },
    { "findnearspritevar",      CON_FINDNEARSPRITEVAR },
    { "findnearsprite3d",       CON_FINDNEARSPRITE3D },
    { "findnearsprite3dvar",    CON_FINDNEARSPRITE3DVAR },
    { "dynamicremap",           CON_DYNAMICREMAP },
    { "dynamicsoundremap",      CON_DYNAMICSOUNDREMAP },

    { "save",                   CON_SAVE },
    { "cansee",                 CON_CANSEE },
    { "canseespr",              CON_CANSEESPR },
    { "findnearactorz",         CON_FINDNEARACTORZ },
    { "findnearactorzvar",      CON_FINDNEARACTORZVAR },
    { "findnearspritez",        CON_FINDNEARSPRITEZ },
    { "findnearspritezvar",     CON_FINDNEARSPRITEZVAR },

    { "eshoot",                 CON_ESHOOT },
    { "eshootvar",              CON_ESHOOT },

    { "zshootvar",              CON_ZSHOOT },
    { "ezshootvar",             CON_EZSHOOT },

    { "getcurraddress",         CON_GETCURRADDRESS },
    { "jump",                   CON_JUMP },
    { "qstrlen",                CON_QSTRLEN },
    { "getincangle",            CON_GETINCANGLE },
    { "quake",                  CON_QUAKE },
    { "showview",               CON_SHOWVIEW },
    { "headspritestat",         CON_HEADSPRITESTAT },
    { "prevspritestat",         CON_PREVSPRITESTAT },
    { "nextspritestat",         CON_NEXTSPRITESTAT },
    { "headspritesect",         CON_HEADSPRITESECT },
    { "prevspritesect",         CON_PREVSPRITESECT },
    { "nextspritesect",         CON_NEXTSPRITESECT },
    { "getkeyname",             CON_GETKEYNAME },
    { "qsubstr",                CON_QSUBSTR },
    { "gametextz",              CON_GAMETEXTZ },
    { "digitalnumberz",         CON_DIGITALNUMBERZ },
    { "spritenopal",            CON_SPRITENOPAL },
    { "hitradiusvar",           CON_HITRADIUSVAR },
    { "rotatesprite16",         CON_ROTATESPRITE16 },

    { "array",                  CON_GAMEARRAY },
    { "gamearray",              CON_GAMEARRAY },
    { "setarray",               CON_SETARRAY },
    { "resizearray",            CON_RESIZEARRAY },
    { "writearraytofile",       CON_WRITEARRAYTOFILE },
    { "readarrayfromfile",      CON_READARRAYFROMFILE },

    { "qgetsysstr",             CON_QGETSYSSTR },
    { "getticks",               CON_GETTICKS },
    { "savemapstate",           CON_SAVEMAPSTATE },
    { "loadmapstate",           CON_LOADMAPSTATE },
    { "clearmapstate",          CON_CLEARMAPSTATE },
    { "scriptsize",             CON_SCRIPTSIZE },
    { "setgamename",            CON_SETGAMENAME },
    { "cmenu",                  CON_CMENU },
    { "gettimedate",            CON_GETTIMEDATE },
    { "activatecheat",          CON_ACTIVATECHEAT },
    { "setgamepalette",         CON_SETGAMEPALETTE },
    { "setdefname",             CON_SETDEFNAME },
    { "setcfgname",             CON_SETCFGNAME },
    { "getarraysize",           CON_GETARRAYSIZE },
    { "savenn",                 CON_SAVENN },
    { "qstrdim",                CON_QSTRDIM },
    { "screentext",             CON_SCREENTEXT },
    { "return",                 CON_RETURN },
    { "actorsound",             CON_ACTORSOUND },
    { "stopactorsound",         CON_STOPACTORSOUND },
    { "rotatespritea",          CON_ROTATESPRITEA },
    { "damageeventtile",        CON_DAMAGEEVENTTILE },
    { "damageeventtilerange",   CON_DAMAGEEVENTTILERANGE },
    { "getteamname",            CON_GETTEAMNAME },
    { "abs",                    CON_KLABS },
    { "defstate",               CON_DEFSTATE },
    { "interpolate",            CON_INTERPOLATE },
};

static const vec2_t varvartable[] =
{
    //{ CON_IFVARVARA,         CON_IFVARA },
    //{ CON_IFVARVARAE,        CON_IFVARAE },
    { CON_IFVARVARAND,       CON_IFVARAND },
    //{ CON_IFVARVARB,         CON_IFVARB },
    //{ CON_IFVARVARBE,        CON_IFVARBE },
    //{ CON_IFVARVARBOTH,      CON_IFVARBOTH },
    { CON_IFVARVARE,         CON_IFVARE },
    { CON_IFVARVAREITHER,    CON_IFVAREITHER },
    { CON_IFVARVARG,         CON_IFVARG },
    { CON_IFVARVARGE,        CON_IFVARGE },
    { CON_IFVARVARL,         CON_IFVARL },
    { CON_IFVARVARLE,        CON_IFVARLE },
    { CON_IFVARVARN,         CON_IFVARN },
    { CON_IFVARVAROR,        CON_IFVAROR },
    { CON_IFVARVARXOR,       CON_IFVARXOR },

    { CON_ADDVARVAR,         CON_ADDVAR },
    { CON_ANDVARVAR,         CON_ANDVAR },
    { CON_DISPLAYRANDVARVAR, CON_DISPLAYRANDVAR },
    { CON_DIVVARVAR,         CON_DIVVAR },
    { CON_MODVARVAR,         CON_MODVAR },
    { CON_MULVARVAR,         CON_MULVAR },
    { CON_ORVARVAR,          CON_ORVAR },
    { CON_RANDVARVAR,        CON_RANDVAR },
    { CON_SETVARVAR,         CON_SETVAR },
    { CON_SHIFTVARVARL,      CON_SHIFTVARL },
    { CON_SHIFTVARVARR,      CON_SHIFTVARR },
    { CON_SUBVARVAR,         CON_SUBVAR },
    //{ CON_WHILEVARVARL,      CON_WHILEVARL },
    { CON_WHILEVARVARN,      CON_WHILEVARN },
    { CON_XORVARVAR,         CON_XORVAR },
};

static const vec2_t globalvartable[] =
{
    { CON_SETVAR,         CON_SETVAR_GLOBAL },
};

static const vec2_t playervartable[] =
{
    { CON_SETVAR,         CON_SETVAR_PLAYER },
};

static const vec2_t actorvartable[] =
{
    { CON_SETVAR,         CON_SETVAR_ACTOR },
};


uint8_t *bitptr; // pointer to bitmap of which bytecode positions contain pointers
#define BITPTR_SET(x) bitmap_set(bitptr, x)
#define BITPTR_CLEAR(x) bitmap_clear(bitptr, x)
#define BITPTR_IS_POINTER(x) bitmap_test(bitptr, x)

static hashtable_t h_keywords = { CON_END >> 1, NULL };
hashtable_t h_gamevars   = { MAXGAMEVARS>>1, NULL };
hashtable_t h_arrays     = { MAXGAMEARRAYS>>1, NULL };
hashtable_t h_labels     = { MAXLABELS>>1, NULL };
hashtable_t h_iter       = { ITER_END >> 1, NULL };

static inthashtable_t h_varvar = { NULL, INTHASH_SIZE(ARRAY_SIZE(varvartable)) };
static inthashtable_t h_globalvar = { NULL, INTHASH_SIZE(ARRAY_SIZE(globalvartable)) };
static inthashtable_t h_playervar = { NULL, INTHASH_SIZE(ARRAY_SIZE(playervartable)) };
static inthashtable_t h_actorvar = { NULL, INTHASH_SIZE(ARRAY_SIZE(actorvartable)) };

static inthashtable_t* const inttables[] = {
    &h_varvar,
    &h_globalvar,
    &h_playervar,
    &h_actorvar,
};

static hashtable_t* const tables[] = {
    &h_arrays,
    &h_gamevars,
    &h_iter,
    &h_keywords,
    &h_labels,
};

static hashtable_t* const tables_free[] = {
    &h_iter,
    &h_keywords,
};

void freehash()
{
    hash_free(&h_gamevars);
    hash_free(&h_arrays);
    hash_free(&h_labels);
}

const tokenmap_t iter_tokens[] =
{
    { "allsprites",       ITER_ALLSPRITES },
    { "allspritesbystat", ITER_ALLSPRITESBYSTAT },
    { "allspritesbysect", ITER_ALLSPRITESBYSECT },
    { "allsectors",       ITER_ALLSECTORS },
    { "allwalls",         ITER_ALLWALLS },
    { "activelights",     ITER_ACTIVELIGHTS },
    { "drawnsprites",     ITER_DRAWNSPRITES },
    { "spritesofsector",  ITER_SPRITESOFSECTOR },
    { "spritesofstatus",  ITER_SPRITESOFSTATUS },
    { "loopofwall",       ITER_LOOPOFWALL },
    { "wallsofsector",    ITER_WALLSOFSECTOR },
    { "range",            ITER_RANGE },
    // vvv alternatives go here vvv
    { "lights",           ITER_ACTIVELIGHTS },
    { "sprofsec",         ITER_SPRITESOFSECTOR },
    { "sprofstat",        ITER_SPRITESOFSTATUS },
    { "walofsec",         ITER_WALLSOFSECTOR },
};

// some keywords generate different opcodes depending on the context the keyword is used in
// keywords_for_private_opcodes[] resolves those opcodes to the publicly facing keyword that can generate them
static const tokenmap_t keywords_for_private_opcodes[] =
{
    { "getactor",  CON_GETSPRITEEXT },
    { "getactor",  CON_GETACTORSTRUCT },
    { "getactor",  CON_GETSPRITESTRUCT },

    { "setactor",  CON_SETSPRITEEXT },
    { "setactor",  CON_SETACTORSTRUCT },
    { "setactor",  CON_SETSPRITESTRUCT },

    { "getwall",   CON_GETWALLSTRUCT },
    { "setwall",   CON_SETWALLSTRUCT },

    { "getsector", CON_GETSECTORSTRUCT },
    { "setsector", CON_SETSECTORSTRUCT },

    { "getplayer", CON_GETPLAYERSTRUCT },
    { "setplayer", CON_SETPLAYERSTRUCT },
};

char const* VM_GetKeywordForID(int32_t id)
{
    // could be better, but this is used strictly for diagnostic warning and error messages
    for (tokenmap_t const& keyword : vm_keywords)
        if (keyword.val == id)
            return keyword.token;

    for (tokenmap_t const& keyword : keywords_for_private_opcodes)
        if (keyword.val == id)
            return keyword.token;

    return "<unknown instruction>";
}

static void C_SetScriptSize(int32_t newsize)
{
    for (int i = 0; i < g_scriptSize - 1; ++i)
    {
        if (BITPTR_IS_POINTER(i))
        {
            if (EDUKE32_PREDICT_FALSE(apScript[i] < (intptr_t)apScript || apScript[i] > (intptr_t)g_scriptPtr))
            {
                g_errorCnt++;
                buildprint("Internal compiler error at ", i, " (0x", hex(i), ")\n");
                VM_ScriptInfo(&apScript[i], 16);
            }
            else
                apScript[i] -= (intptr_t)apScript;
        }
    }

    G_Util_PtrToIdx2(&g_tile[0].execPtr, MAXTILES, sizeof(tiledata_t), apScript, P2I_FWD_NON0);
    G_Util_PtrToIdx2(&g_tile[0].loadPtr, MAXTILES, sizeof(tiledata_t), apScript, P2I_FWD_NON0);

    size_t old_bitptr_size = (((g_scriptSize + 7) >> 3) + 1) * sizeof(uint8_t);
    size_t new_bitptr_size = (((newsize + 7) >> 3) + 1) * sizeof(uint8_t);

    auto newscript = (intptr_t*)Xrealloc(apScript, newsize * sizeof(intptr_t));
    bitptr = (uint8_t*)Xrealloc(bitptr, new_bitptr_size);

    if (newsize > g_scriptSize)
    {
        Bmemset(&newscript[g_scriptSize], 0, (newsize - g_scriptSize) * sizeof(intptr_t));
        Bmemset(&bitptr[old_bitptr_size], 0, new_bitptr_size - old_bitptr_size);
    }

    if (apScript != newscript)
    {
        DVLOG_F(LOG_CON, "Relocated compiled code from 0x%08" PRIxPTR " to 0x%08" PRIxPTR, (uintptr_t)apScript, (uintptr_t)newscript);
        g_scriptPtr = g_scriptPtr - apScript + newscript;
        apScript = newscript;
    }

    int const smallestSize = min(g_scriptSize, newsize);

    for (int i = 0; i < smallestSize - 1; ++i)
    {
        if (BITPTR_IS_POINTER(i))
            apScript[i] += (intptr_t)apScript;
    }

    g_scriptSize = newsize;

    G_Util_PtrToIdx2(&g_tile[0].execPtr, MAXTILES, sizeof(tiledata_t), apScript, P2I_BACK_NON0);
    G_Util_PtrToIdx2(&g_tile[0].loadPtr, MAXTILES, sizeof(tiledata_t), apScript, P2I_BACK_NON0);
}

static inline bool ispecial(const char c)
{
    return (c == ' ' || c == 0x0d || c == '(' || c == ')' ||
        c == ',' || c == ';' || (c == 0x0a /*&& ++g_lineNumber*/));
}

static inline void scriptSkipLine(void)
{
    while (*textptr != 0x0a && *textptr != 0x0d && *textptr != 0)
        textptr++;
}

static inline void scriptSkipSpaces(void)
{
    while (*textptr == ' ' || *textptr == '\t')
        textptr++;
}

static inline void scriptWriteValue(int32_t const value)
{
    bitmap_clear(bitptr, g_scriptPtr - apScript);
    *g_scriptPtr++ = value;
}

// addresses passed to these functions must be within the block of memory pointed to by apScript
static inline void scriptWriteAtOffset(int32_t const value, intptr_t* const addr)
{
    bitmap_clear(bitptr, addr - apScript);
    *(addr) = value;
}

static inline void scriptWritePointer(intptr_t const value, intptr_t* const addr)
{
    bitmap_set(bitptr, addr - apScript);
    *(addr) = value;
}

int32_t C_AllocQuote(int32_t qnum)
{
    Bassert((unsigned)qnum < MAXQUOTES);

    if (apStrings[qnum] == NULL)
    {
        apStrings[qnum] = (char*)Xcalloc(MAXQUOTELEN, sizeof(uint8_t));
        return 1;
    }

    return 0;
}

static void C_SkipComments(void)
{
    do
    {
        switch (*textptr)
        {
            case '\n':
                g_lineNumber++;
                fallthrough__;
            case ' ':
            case '\t':
            case '\r':
            case 0x1a:
                textptr++;
                break;
            case '/':
                switch (textptr[1])
                {
                    case '/': // C++ style comment
                        if (g_scriptDebug > 1 && !g_errorCnt && !g_warningCnt)
                            VLOG_F(LOG_CON, "%s:%d: got comment.", g_scriptFileName, g_lineNumber);
                        scriptSkipLine();
                        continue;
                    case '*': // beginning of a C style comment
                        if (g_scriptDebug > 1 && !g_errorCnt && !g_warningCnt)
                            VLOG_F(LOG_CON, "%s:%d: got start of comment block.", g_scriptFileName, g_lineNumber);
                        do
                        {
                            if (*textptr == '\n')
                                g_lineNumber++;
                            textptr++;
                        } while (*textptr && (textptr[0] != '*' || textptr[1] != '/'));

                        if (EDUKE32_PREDICT_FALSE(!*textptr))
                        {
                            if (!(g_errorCnt || g_warningCnt) && g_scriptDebug)
                                VLOG_F(LOG_CON, "%s:%d: EOF in comment!", g_scriptFileName, g_lineNumber);
                            C_ReportError(-1);
                            LOG_F(ERROR, "%s:%d: found '/*' with no '*/'.", g_scriptFileName, g_lineNumber);
                            g_scriptActorOffset = 0;
                            g_processingState = g_numBraces = 0;
                            g_errorCnt++;
                            continue;
                        }

                        if (g_scriptDebug > 1 && !g_errorCnt && !g_warningCnt)
                            VLOG_F(LOG_CON, "%s:%d: got end of comment block.", g_scriptFileName, g_lineNumber);

                        textptr += 2;
                        continue;
                    default:
                        C_ReportError(-1);
                        LOG_F(ERROR, "%s:%d: malformed comment.", g_scriptFileName, g_lineNumber);
                        scriptSkipLine();
                        g_errorCnt++;
                        continue;
                }
                break;

            default:
                if (ispecial(*textptr))
                {
                    textptr++;
                    continue;
                }
                fallthrough__;
            case 0: // EOF
                return;
        }
    } while (1);
}

static int32_t C_SetDefName(const char* name)
{
    clearDefNamePtr();
    g_defNamePtr = dup_filename(name);
    if (g_defNamePtr)
        LOG_F(INFO, "Using DEF file: %s.", g_defNamePtr);
    return (g_defNamePtr == NULL);
}

static int C_CheckEventSync(int iEventID)
{
    if (g_scriptEventOffset || g_scriptActorOffset)
    {
        switch (iEventID)
        {
        case EVENT_ANIMATESPRITES:
        case EVENT_CHEATGETSTEROIDS:
        case EVENT_CHEATGETHEAT:
        case EVENT_CHEATGETBOOT:
        case EVENT_CHEATGETSHIELD:
        case EVENT_CHEATGETSCUBA:
        case EVENT_CHEATGETHOLODUKE:
        case EVENT_CHEATGETJETPACK:
        case EVENT_CHEATGETFIRSTAID:
        case EVENT_DISPLAYCROSSHAIR:
        case EVENT_DISPLAYREST:
        case EVENT_DISPLAYBONUSSCREEN:
        case EVENT_DISPLAYMENU:
        case EVENT_DISPLAYMENUREST:
        case EVENT_DISPLAYLOADINGSCREEN:
        case EVENT_DISPLAYSTART:
        case EVENT_DISPLAYROOMS:
        case EVENT_DISPLAYSBAR:
        case EVENT_DISPLAYLEVELSTATS:
        case EVENT_DISPLAYACCESS:
        case EVENT_DISPLAYWEAPON:
        case EVENT_DISPLAYTIP:
        case EVENT_DRAWWEAPON:
        //case EVENT_ENTERLEVEL:
        case EVENT_FAKEDOMOVETHINGS:
        case EVENT_GETLOADTILE:
        case EVENT_GETMENUTILE:
        case EVENT_INIT:
        case EVENT_LOGO:
        case EVENT_SOUND:
        case EVENT_PRELEVEL:
        case EVENT_DOQUAKE:
            return 0;
        default:
            return 1;
        }
    }
    return 1;
}

static inline int GetDefID(char const* label) { return hash_find(&h_gamevars, label); }
static inline int GetADefID(char const* label) { return hash_find(&h_arrays, label); }

static inline bool isaltok(const char c)
{
    return (isalnum(c) || c == '{' || c == '}' || c == '/' || c == '\\' || c == '*' || c == '-' || c == '_' ||
            c == '.');
}

static inline bool isdigterm(const char c)
{
    return (isspace(c) || c == '{' || c == '}' || c == '[' || c == ']' || c == '/' || c == ',' || c == ':' || c == '.' || c == ';' || c == '\0');
}

static inline bool C_IsLabelChar(const char c, int32_t const i)
{
    return (isalnum(c) || c == '_' || c == '*' || c == '?' || (i > 0 && (c == '+' || c == '-')));
}

static int32_t parse_decimal_number(void)  // (textptr)
{
    // decimal constants -- this is finicky business
    char* endptr;
    int64_t num = strtoll(textptr, &endptr, 10);  // assume long long to be int64_t

    if (EDUKE32_PREDICT_TRUE(num >= INT32_MIN && num <= INT32_MAX))
    {
        // all OK
    }
    else if (EDUKE32_PREDICT_FALSE(num > INT32_MAX && num <= UINT32_MAX))
    {
        // Number interpreted as uint32, but packed as int32 (on 32-bit archs)
        // (CON code in the wild exists that does this).  Note that such conversion
        // is implementation-defined (C99 6.3.1.3) but GCC does the 'expected' thing.
#if 0
        LOG_F(WARNING, "%s:%d: number greater than INT32_MAX converted to a negative one.",
            g_szScriptFileName, g_lineNumber);
        g_numCompilerWarnings++;
#endif
    }
    else
    {
        // out of range, this is arguably worse

        LOG_F(WARNING, "%s:%d: number out of the range of a 32-bit integer encountered.",
            g_scriptFileName, g_lineNumber);
        g_warningCnt++;
    }

    if (EDUKE32_PREDICT_FALSE(!isdigterm(*endptr)))
    {
        LOG_F(WARNING, "%s:%d: invalid character '%c' in decimal constant!", g_scriptFileName, g_lineNumber, *endptr);
        g_warningCnt++;
    }

    return (int32_t)num;
}

static int32_t parse_hex_constant(const char* hexnum)
{
    const char* vcheck = hexnum;
    if (EDUKE32_PREDICT_FALSE(!isxdigit(*vcheck)))
    {
        g_errorCnt++;
        LOG_F(ERROR, "%s:%d: malformed hex constant.", g_scriptFileName, g_lineNumber);
        return 0;
    }

    while (!isdigterm(*vcheck))
    {
        if (EDUKE32_PREDICT_FALSE(!isxdigit(*vcheck)))
        {
            LOG_F(WARNING, "%s:%d: invalid character '%c' in hex constant!", g_scriptFileName, g_lineNumber, *vcheck);
            g_warningCnt++;
            break;
        }
        ++vcheck;
    }

    uint64_t x;
    sscanf(hexnum, "%" PRIx64 "", &x);

    if (EDUKE32_PREDICT_FALSE(x > UINT32_MAX))
    {
        LOG_F(WARNING, "%s:%d: number 0x%016" PRIx64 " truncated to 32 bits.", g_scriptFileName, g_lineNumber, x);
        g_warningCnt++;
    }

    return x;
}

static inline int32_t C_GetLabelNameID(memberlabel_t const* pLabel, hashtable_t const* const table, const char* psz)
{
    // find the label psz in the table pLabel.
    // returns the ID for the label, or -1

    int const l = hash_findcase(table, psz);
    return (l >= 0) ? pLabel[l].lId : -1;
}

static inline int C_GetLabelNameOffset(hashtable_t const *table, const char *psz)
{
    // find the label psz in the table pLabel.
    // returns the offset in the array for the label, or -1

    return hash_findcase(table,psz);
}

static void C_GetNextLabelName(void)
{
    int32_t i = 0;

    if (EDUKE32_PREDICT_FALSE(g_labelCnt >= MAXLABELS))
    {
        g_errorCnt++;
        //C_ReportError(ERROR_TOOMANYLABELS);
        G_GameExit("Error: too many labels defined!");
        return;
    }

    C_SkipComments();

    if (isdigit(*textptr))
    {
        LOG_F(WARNING, "%s:%d: label starts with a digit!", g_scriptFileName, g_lineNumber);
        g_warningCnt++;
    }

    //    while (ispecial(*textptr) == 0 && *textptr!='['&& *textptr!=']' && *textptr!='\t' && *textptr!='\n' && *textptr!='\r')
    while (C_IsLabelChar(*textptr, i))
    {
        if (i < (1 << 6) - 1)
            label[(g_labelCnt << 6) + (i++)] = *textptr;
        textptr++;
    }

    label[(g_labelCnt << 6) + i] = 0;

    if (!(g_errorCnt | g_warningCnt) && g_scriptDebug > 1)
        VLOG_F(LOG_CON, "%s:%d: label '%s'.", g_scriptFileName, g_lineNumber, LAST_LABEL);
}

static int C_GetKeyword(void)
{
    C_SkipComments();

    char* temptextptr = textptr;

    if (*temptextptr == 0) // EOF
        return -2;

    while (isaltok(*temptextptr) == 0)
    {
        temptextptr++;
        if (*temptextptr == 0)
            return 0;
    }

    int i = 0;

    while (isaltok(*temptextptr))
        tempbuf[i++] = *(temptextptr++);
    tempbuf[i] = 0;

    return hash_find(&h_keywords, tempbuf);
}

static int C_GetNextKeyword(void) //Returns its code #
{
    C_SkipComments();

    if (*textptr == 0) // EOF
        return -2;

    int l = 0;
    while (isaltok(*(textptr + l)))
    {
        tempbuf[l] = textptr[l];
        l++;
    }
    tempbuf[l] = 0;

    int i;
    if (EDUKE32_PREDICT_TRUE((i = hash_find(&h_keywords, tempbuf)) >= 0))
    {
        if (i == CON_LEFTBRACE || i == CON_RIGHTBRACE || i == CON_NULLOP)
            scriptWriteValue(i | LINE_NUMBER | VM_IFELSE_MAGIC_BIT);
        else scriptWriteValue(i | LINE_NUMBER);

        textptr += l;
        if (!(g_errorCnt || g_warningCnt) && g_scriptDebug)
            VLOG_F(LOG_CON, "%s:%d: keyword '%s'.", g_scriptFileName, g_lineNumber, tempbuf);
        return i;
    }

    textptr += l;
    g_errorCnt++;

    if (EDUKE32_PREDICT_FALSE((tempbuf[0] == '{' || tempbuf[0] == '}') && tempbuf[1] != 0))
    {
        C_ReportError(-1);
        LOG_F(ERROR, "%s:%d: expected whitespace between '%c' and '%s'.", g_scriptFileName, g_lineNumber, tempbuf[0], tempbuf + 1);
    }
    else C_ReportError(ERROR_EXPECTEDKEYWORD);

    return -1;
}

static void C_GetNextVarType(int32_t type)
{
    int32_t id = 0;
    int32_t flags = 0;

    auto varptr = g_scriptPtr;

    C_SkipComments();

    if (!type && !g_labelsOnly && (isdigit(*textptr) || ((*textptr == '-') && (isdigit(*(textptr + 1))))))
    {
        scriptWriteValue(GV_FLAG_CONSTANT);

        if (tolower(textptr[1]) == 'x')  // hex constants
            scriptWriteValue(parse_hex_constant(textptr + 2));
        else
            scriptWriteValue(parse_decimal_number());

        if (!(g_errorCnt || g_warningCnt) && g_scriptDebug)
            VLOG_F(LOG_CON, "%s:%d: constant %ld in place of gamevar.", g_scriptFileName, g_lineNumber, (long)(g_scriptPtr[-1]));
#if 1
        while (!ispecial(*textptr) && *textptr != ']') textptr++;
#else
        C_GetNextLabelName();
#endif
        return;
    }
    else if (*textptr == '-'/* && !isdigit(*(textptr+1))*/)
    {
        if (EDUKE32_PREDICT_FALSE(type))
        {
            g_errorCnt++;
            C_ReportError(ERROR_SYNTAXERROR);
            C_GetNextLabelName();
            return;
        }

        if (!(g_errorCnt || g_warningCnt) && g_scriptDebug)
            VLOG_F(LOG_CON, "%s:%d: flagging gamevar as negative.", g_scriptFileName, g_lineNumber); //,Batol(textptr));

        flags = GV_FLAG_NEGATIVE;
        textptr++;
    }

    C_GetNextLabelName();

    if (EDUKE32_PREDICT_FALSE(hash_find(&h_keywords, LAST_LABEL) >= 0))
    {
        g_errorCnt++;
        C_ReportError(ERROR_ISAKEYWORD);
        return;
    }

    C_SkipComments();

    if (*textptr == '[' || *textptr == '.')     //read of array as a gamevar
    {
        flags |= GV_FLAG_ARRAY;
        if (*textptr != '.') textptr++;
        id = GetADefID(LAST_LABEL);

        if (id < 0)
        {
            id = GetDefID(LAST_LABEL);
            if ((unsigned)(id - sysVarIDs.structs) >= NUMQUICKSTRUCTS)
                id = -1;

            if (EDUKE32_PREDICT_FALSE(id < 0))
            {
                g_errorCnt++;
                C_ReportError(ERROR_NOTAGAMEARRAY);
                return;
            }

            flags &= ~GV_FLAG_ARRAY; // not an array
            flags |= GV_FLAG_STRUCT;
        }

        scriptWriteValue(id | flags);

        if ((flags & GV_FLAG_STRUCT) && id - sysVarIDs.structs == STRUCT_USERDEF)
        {
            // userdef doesn't really have an array index
            while (*textptr != '.')
            {
                if (*textptr == 0xa || *textptr == 0)
                    break;

                textptr++;
            }

            scriptWriteValue(0); // help out the VM by inserting a dummy index
        }
        else
        {
            // allow "[]" or "." but not "[."
            if (*textptr == ']' || (*textptr == '.' && textptr[-1] != '['))
            {
                scriptWriteValue(sysVarIDs.THISACTOR);
            }
            else
                C_GetNextVarType(0);

            C_SkipComments();
        }

        if (EDUKE32_PREDICT_FALSE(*textptr != ']' && *textptr != '.'))
        {
            g_errorCnt++;
            C_ReportError(ERROR_GAMEARRAYBNC);
            return;
        }

        if (*textptr != '.') textptr++;

        //writing arrays in this way is not supported because it would require too many changes to other code

        if (EDUKE32_PREDICT_FALSE(type))
        {
            g_errorCnt++;
            C_ReportError(ERROR_INVALIDARRAYWRITE);
            return;
        }

        if (flags & GV_FLAG_STRUCT)
        {
            while (*textptr != '.')
            {
                if (*textptr == 0xa || !*textptr)
                    break;

                textptr++;
            }

            if (EDUKE32_PREDICT_FALSE(*textptr != '.'))
            {
                g_errorCnt++;
                C_ReportError(ERROR_SYNTAXERROR);
                return;
            }
            textptr++;
            /// now pointing at 'xxx'
            C_GetNextLabelName();
            /*initprintf("found xxx label of \"%s\"\n",   label+(g_numLabels<<6));*/

            int32_t labelNum = -1;

            switch (id - sysVarIDs.structs)
            {
                case STRUCT_SPRITE:         labelNum = C_GetLabelNameOffset(&h_actor, Bstrtolower(LAST_LABEL)); break;
                case STRUCT_SECTOR:         labelNum = C_GetLabelNameOffset(&h_sector, Bstrtolower(LAST_LABEL)); break;
                case STRUCT_WALL:           labelNum = C_GetLabelNameOffset(&h_wall, Bstrtolower(LAST_LABEL)); break;
                case STRUCT_PLAYER:         labelNum = C_GetLabelNameOffset(&h_player, Bstrtolower(LAST_LABEL)); break;
                case STRUCT_TSPR:           labelNum = C_GetLabelNameOffset(&h_tsprite, Bstrtolower(LAST_LABEL)); break;
                case STRUCT_PROJECTILE:
                case STRUCT_THISPROJECTILE: labelNum = C_GetLabelNameOffset(&h_projectile, Bstrtolower(LAST_LABEL)); break;
                case STRUCT_USERDEF:        labelNum = C_GetLabelNameOffset(&h_userdef, Bstrtolower(LAST_LABEL)); break;
                case STRUCT_INPUT:          labelNum = C_GetLabelNameOffset(&h_input, Bstrtolower(LAST_LABEL)); break;
                case STRUCT_TILEDATA:       labelNum = C_GetLabelNameOffset(&h_tiledata, Bstrtolower(LAST_LABEL)); break;
                //case STRUCT_PALDATA:        labelNum = C_GetLabelNameOffset(&h_paldata, Bstrtolower(LAST_LABEL)); break;

                case STRUCT_ACTORVAR:
                case STRUCT_PLAYERVAR:      labelNum = GetDefID(LAST_LABEL); break;
            }

            if (labelNum == -1)
            {
                g_errorCnt++;
                C_ReportError(ERROR_NOTAMEMBER);
                return;
            }

            switch (id - sysVarIDs.structs)
            {
                case STRUCT_SPRITE:
                {
                    auto const& label = ActorLabels[labelNum];

                    scriptWriteValue(labelNum);

                    Bassert((*varptr & (MAXGAMEVARS - 1)) == sysVarIDs.structs + STRUCT_SPRITE);

                    if (label.flags & LABEL_HASPARM2)
                        C_GetNextVarType(0);
                    else if (label.offset != -1 && (label.flags & LABEL_READFUNC) == 0)
                    {
                        if (labelNum >= ACTOR_SPRITEEXT_BEGIN)
                            *varptr = (*varptr & ~(MAXGAMEVARS - 1)) + sysVarIDs.structs + STRUCT_SPRITEEXT_INTERNAL__;
                        else if (labelNum >= ACTOR_STRUCT_BEGIN)
                            *varptr = (*varptr & ~(MAXGAMEVARS - 1)) + sysVarIDs.structs + STRUCT_ACTOR_INTERNAL__;
                        else
                            *varptr = (*varptr & ~(MAXGAMEVARS - 1)) + sysVarIDs.structs + STRUCT_SPRITE_INTERNAL__;
                    }
                }
                break;

                case STRUCT_SECTOR:
                {
                    auto const& label = SectorLabels[labelNum];

                    scriptWriteValue(labelNum);

                    Bassert((*varptr & (MAXGAMEVARS - 1)) == sysVarIDs.structs + STRUCT_SECTOR);

                    if (label.offset != -1 && (label.flags & LABEL_READFUNC) == 0)
                        *varptr = (*varptr & ~(MAXGAMEVARS - 1)) + sysVarIDs.structs + STRUCT_SECTOR_INTERNAL__;
                }
                break;
                case STRUCT_WALL:
                {
                    auto const& label = WallLabels[labelNum];

                    scriptWriteValue(labelNum);

                    Bassert((*varptr & (MAXGAMEVARS - 1)) == sysVarIDs.structs + STRUCT_WALL);

                    if (label.offset != -1 && (label.flags & LABEL_READFUNC) == 0)
                        *varptr = (*varptr & ~(MAXGAMEVARS - 1)) + sysVarIDs.structs + STRUCT_WALL_INTERNAL__;
                }
                break;
                case STRUCT_PLAYER:
                {
                    auto const& label = PlayerLabels[labelNum];

                    scriptWriteValue(labelNum);

                    Bassert((*varptr & (MAXGAMEVARS - 1)) == sysVarIDs.structs + STRUCT_PLAYER);

                    if (label.flags & LABEL_HASPARM2)
                        C_GetNextVarType(0);
                    else if (label.offset != -1 && (label.flags & LABEL_READFUNC) == 0)
                        *varptr = (*varptr & ~(MAXGAMEVARS - 1)) + sysVarIDs.structs + STRUCT_PLAYER_INTERNAL__;
                }
                break;
                case STRUCT_ACTORVAR:
                case STRUCT_PLAYERVAR:
                case STRUCT_TSPR:
                case STRUCT_PROJECTILE:
                case STRUCT_THISPROJECTILE:
                case STRUCT_INPUT:
                case STRUCT_TILEDATA:
                //case STRUCT_PALDATA:
                    scriptWriteValue(labelNum);
                    break;
                case STRUCT_USERDEF:
                    scriptWriteValue(labelNum);

                    if (UserdefsLabels[labelNum].flags & LABEL_HASPARM2)
                        C_GetNextVarType(0);
                    break;
            }
        }
        return;
    }

    id = GetDefID(LAST_LABEL);
    if (id < 0)   //gamevar not found
    {
        if (EDUKE32_PREDICT_TRUE(!type && !g_labelsOnly))
        {
            //try looking for a define instead
            Bstrcpy(tempbuf, LAST_LABEL);
            id = hash_find(&h_labels, tempbuf);

            if (EDUKE32_PREDICT_TRUE(id >= 0 && labeltype[id] & LABEL_DEFINE))
            {
                if (!(g_errorCnt || g_warningCnt) && g_scriptDebug)
                    VLOG_F(LOG_CON, "%s:%d: label '%s' in place of gamevar.", g_scriptFileName, g_lineNumber, label + (id << 6));

                scriptWriteValue(GV_FLAG_CONSTANT);
                scriptWriteValue(labelcode[id]);
                return;
            }
        }

        g_errorCnt++;
        C_ReportError(ERROR_NOTAGAMEVAR);
        return;
    }

    if (EDUKE32_PREDICT_FALSE(type == GAMEVAR_READONLY && aGameVars[id].flags & GAMEVAR_READONLY))
    {
        g_errorCnt++;
        C_ReportError(ERROR_VARREADONLY);
        return;
    }
    else if (EDUKE32_PREDICT_FALSE(aGameVars[id].flags & type))
    {
        g_errorCnt++;
        C_ReportError(ERROR_VARTYPEMISMATCH);
        return;
    }

    if (g_scriptDebug > 1 && !g_errorCnt && !g_warningCnt)
        VLOG_F(LOG_CON, "%s:%d: gamevar '%s'.", g_scriptFileName, g_lineNumber, LAST_LABEL);

    scriptWriteValue(id | flags);
}

#define C_GetNextVar() C_GetNextVarType(0)

static inline void C_GetManyVarsType(int type, int num)
{
    int i;
    for (i=num-1;i>=0;i--)
        C_GetNextVarType(type);
}

static inline void C_GetManyVars(int num)
{
    C_GetManyVarsType(0,num);
}

// returns:
//  -1 on EOF or wrong type or error
//   0 if literal value
//   LABEL_* (>0) if that type and matched
//
// *g_scriptPtr will contain the value OR 0 if wrong type or error
static int32_t C_GetNextValue(int32_t type)
{
    C_SkipComments();

    if (*textptr == 0) // EOF
        return -1;

    int32_t l = 0;

    while (isaltok(*(textptr + l)))
    {
        tempbuf[l] = textptr[l];
        l++;
    }
    tempbuf[l] = 0;

    if (EDUKE32_PREDICT_FALSE(hash_find(&h_keywords, tempbuf /*label+(g_numLabels<<6)*/) >= 0))
    {
        g_errorCnt++;
        C_ReportError(ERROR_ISAKEYWORD);
        textptr += l;
    }

    int32_t i = hash_find(&h_labels, tempbuf);

    if (i >= 0)
    {
        if (EDUKE32_PREDICT_TRUE(labeltype[i] & type))
        {
            if (g_scriptDebug > 1 && !g_errorCnt && !g_warningCnt)
            {
                char* gl = C_GetLabelType(labeltype[i]);
                VLOG_F(LOG_CON, "%s:%d: %s label '%s'.", g_scriptFileName, g_lineNumber, gl, label + (i << 6));
                Xfree(gl);
            }

            scriptWriteValue(labelcode[i]);

            textptr += l;
            return labeltype[i];
        }

        scriptWriteValue(0);
        textptr += l;
        char* const el = C_GetLabelType(type);
        char* const gl = C_GetLabelType(labeltype[i]);
        C_ReportError(-1);
        LOG_F(WARNING, "%s:%d: expected %s, found %s.", g_scriptFileName, g_lineNumber, el, gl);
        g_warningCnt++;
        Xfree(el);
        Xfree(gl);
        return -1;  // valid label name, but wrong type
    }

    if (EDUKE32_PREDICT_FALSE(isdigit(*textptr) == 0 && *textptr != '-'))
    {
        C_ReportError(ERROR_PARAMUNDEFINED);
        g_errorCnt++;
        scriptWriteValue(0);
        textptr += l;
        if (!l) textptr++;
        return -1; // error!
    }

    if (EDUKE32_PREDICT_FALSE(isdigit(*textptr) && g_labelsOnly))
    {
        C_ReportError(WARNING_LABELSONLY);
        g_warningCnt++;
    }

    if (textptr[0] == '0' && tolower(textptr[1]) == 'x')
        scriptWriteValue(parse_hex_constant(textptr + 2));
    else
        scriptWriteValue(parse_decimal_number());

    if (g_scriptDebug > 1 && !g_errorCnt && !g_warningCnt)
        VLOG_F(LOG_CON, "%s:%d: constant %ld.", g_scriptFileName, g_lineNumber, (long)g_scriptPtr[-1]);

    textptr += l;

    return 0;   // literal value
}

static inline void C_BitOrNextValue(int32_t* valptr)
{
    C_GetNextValue(LABEL_DEFINE);
    g_scriptPtr--;
    *valptr |= *g_scriptPtr;
}

static inline void C_FinishBitOr(int32_t value)
{
    scriptWriteValue(value);
}


static void C_FillEventBreakStackWithJump(intptr_t* breakPtr, intptr_t destination)
{
    while (breakPtr)
    {
        breakPtr = apScript + (intptr_t)breakPtr;
        intptr_t const tempPtr = *breakPtr;
        scriptWriteAtOffset(destination, breakPtr);
        breakPtr = (intptr_t*)tempPtr;
    }
}

static void scriptUpdateOpcodeForVariableType(intptr_t* ins)
{
    int opcode = -1;

    if (ins[1] < MAXGAMEVARS)
    {
        switch (aGameVars[ins[1] & (MAXGAMEVARS - 1)].flags & (GAMEVAR_USER_MASK | GAMEVAR_PTR_MASK))
        {
            case 0:
                opcode = inthash_find(&h_globalvar, *ins & VM_INSTMASK);
                break;
            case GAMEVAR_PERACTOR:
                opcode = inthash_find(&h_actorvar, *ins & VM_INSTMASK);
                break;
            case GAMEVAR_PERPLAYER:
                opcode = inthash_find(&h_playervar, *ins & VM_INSTMASK);
                break;
        }
    }

    if (opcode != -1)
    {
        if (g_scriptDebug > 1 && !g_errorCnt && !g_warningCnt)
        {
            VLOG_F(LOG_CON, "%s:%d: %s -> %s for var %s", g_scriptFileName, g_lineNumber,
                VM_GetKeywordForID(*ins & VM_INSTMASK), VM_GetKeywordForID(opcode), aGameVars[ins[1] & (MAXGAMEVARS - 1)].szLabel);
        }

        scriptWriteAtOffset(opcode | LINE_NUMBER, ins);
    }
}

static int C_GetStructureIndexes(bool const labelsonly, hashtable_t const* const table)
{
    C_SkipComments();

    if (EDUKE32_PREDICT_FALSE(*textptr != '[' && *textptr != '.'))
    {
        g_errorCnt++;
        C_ReportError(ERROR_SYNTAXERROR);
        return -1;
    }

    if (*textptr != '.') textptr++;

    C_SkipComments();

    if (*textptr == ']' || *textptr == '.')
    {
        scriptWriteValue(sysVarIDs.THISACTOR);
    }
    else
    {
        g_labelsOnly = labelsonly;
        C_GetNextVar();
        g_labelsOnly = 0;
    }

    if (*textptr != '.') textptr++;

    C_SkipComments();

    // now get name of .xxx

    if (EDUKE32_PREDICT_FALSE(*textptr++ != '.'))
    {
        g_errorCnt++;
        C_ReportError(ERROR_SYNTAXERROR);
        return -1;
    }

    if (!table)
        return 0;

    // .xxx

    C_GetNextLabelName();

    int const labelNum = C_GetLabelNameOffset(table, Bstrtolower(LAST_LABEL));

    if (EDUKE32_PREDICT_FALSE(labelNum == -1))
    {
        g_errorCnt++;
        C_ReportError(ERROR_NOTAMEMBER);
        return -1;
    }

    return labelNum;
}

static bool C_ParseCommand(bool loop = false);

static bool C_CheckMalformedBranch(intptr_t lastScriptPtr)
{
    switch (C_GetKeyword())
    {
        case CON_RIGHTBRACE:
        case CON_ENDA:
        case CON_ENDEVENT:
        case CON_ENDS:
        case CON_ELSE:
            g_scriptPtr = lastScriptPtr + apScript;
            g_skipBranch = true;
            C_ReportError(-1);
            g_warningCnt++;
            LOG_F(WARNING, "%s:%d: malformed '%s' branch", g_scriptFileName, g_lineNumber,
                VM_GetKeywordForID(*(g_scriptPtr)&VM_INSTMASK));
            return true;
    }
    return false;
}

static bool C_CheckEmptyBranch(int tw, intptr_t lastScriptPtr)
{
    // ifrnd and the others actually do something when the condition is executed
    if ((Bstrncmp(VM_GetKeywordForID(tw), "if", 2) && tw != CON_ELSE) ||
        tw == CON_IFRND || tw == CON_IFHITWEAPON || tw == CON_IFCANSEE || tw == CON_IFCANSEETARGET ||
        tw == CON_IFPDISTL || tw == CON_IFPDISTG || tw == CON_IFGOTWEAPONCE)
    {
        g_skipBranch = false;
        return false;
    }

    if ((*(g_scriptPtr)&VM_INSTMASK) != CON_NULLOP || (*(g_scriptPtr)&VM_IFELSE_MAGIC_BIT) == 0)
        g_skipBranch = false;

    if (EDUKE32_PREDICT_FALSE(g_skipBranch))
    {
        C_ReportError(-1);
        g_warningCnt++;
        g_scriptPtr = lastScriptPtr + apScript;
        LOG_F(WARNING, "%s:%d: empty '%s' branch", g_scriptFileName, g_lineNumber,
            VM_GetKeywordForID(*(g_scriptPtr)&VM_INSTMASK));
        scriptWriteAtOffset(CON_NULLOP | LINE_NUMBER | VM_IFELSE_MAGIC_BIT, g_scriptPtr);
        return true;
    }

    return false;
}

static int C_CountCaseStatements()
{
    char* const    temptextptr = textptr;
    int const      backupLineNumber = g_lineNumber;
    int const      backupNumCases = g_numCases;
    intptr_t const casePtrOffset = g_caseTablePtr - apScript;
    intptr_t const scriptPtrOffset = g_scriptPtr - apScript;
    bool const     backupSwitchCount = g_switchCountPhase;

    g_numCases = 0;
    g_caseTablePtr = NULL;

    g_switchCountPhase = true;
    C_ParseCommand(true);
    g_switchCountPhase = backupSwitchCount;

    // since we processed the endswitch, we need to re-increment g_checkingSwitch
    g_checkingSwitch++;

    int const numCases = g_numCases;

    textptr = temptextptr;
    g_lineNumber = backupLineNumber;
    g_numCases = backupNumCases;
    g_caseTablePtr = apScript + casePtrOffset;
    g_scriptPtr = apScript + scriptPtrOffset;

    return numCases;
}

void C_AddFileOffset(int const offset, const char* fileName)
{
    auto newofs = (struct vmofs*)Xcalloc(1, sizeof(struct vmofs));
    newofs->offset = offset;
    newofs->fn = Xstrdup(fileName);
    newofs->next = vmoffset;
    vmoffset = newofs;
}

static void C_Include(const char* confile)
{
    buildvfs_kfd fp = kopen4loadfrommod(confile, g_loadFromGroupOnly);

    if (EDUKE32_PREDICT_FALSE(fp == buildvfs_kfd_invalid))
    {
        g_errorCnt++;
        LOG_F(ERROR, "%s:%d: could not find file %s", g_scriptFileName, g_lineNumber, confile);
        return;
    }

    int32_t const len = kfilelength(fp);
    char* mptr = (char*)Xmalloc(len + 1);

    VLOG_F(LOG_CON, "Including: %s (%d bytes)", confile, len);

    C_AddFileOffset((g_scriptPtr - apScript), confile);

    kread(fp, mptr, len);
    kclose(fp);

    mptr[len] = 0;

    if (*textptr == '"') // skip past the closing quote if it's there so we don't screw up the next line
        textptr++;

    char* const origtptr = textptr;
    char parentScriptFileName[BMAX_PATH];

    Bstrcpy(parentScriptFileName, g_scriptFileName);
    Bstrcpy(g_scriptFileName, confile);

    int const temp_ScriptLineNumber = g_lineNumber;
    g_lineNumber = 1;

    int const temp_ifelse_check = g_checkingIfElse;
    g_checkingIfElse = 0;

    textptr = mptr;

    C_SkipComments();
    C_ParseCommand(true);

    C_AddFileOffset((g_scriptPtr - apScript), parentScriptFileName);

    Bstrcpy(g_scriptFileName, parentScriptFileName);

    g_totalLines += g_lineNumber;
    g_lineNumber = temp_ScriptLineNumber;
    g_checkingIfElse = temp_ifelse_check;

    textptr = origtptr;

    Xfree(mptr);
}

#ifdef _WIN32
static void check_filename_case(const char* fn)
{
    static char buf[BMAX_PATH];

    // .zip isn't case sensitive, and calling kopen4load on files in .zips is slow
    Bstrcpy(buf, fn);
    kzfindfilestart(buf);

    if (!kzfindfile(buf))
    {
        buildvfs_kfd fp;
        if ((fp = kopen4loadfrommod(fn, g_loadFromGroupOnly)) != buildvfs_kfd_invalid)
            kclose(fp);
    }
}
#else
static void check_filename_case(const char* fn) { UNREFERENCED_PARAMETER(fn); }
#endif

static void C_DefineMusic(int volumeNum, int levelNum, const char* fileName)
{
    Bassert((unsigned)volumeNum < MAXVOLUMES + 1);
    Bassert((unsigned)levelNum < MAXLEVELS);

    if (strcmp(fileName, "/.") == 0)
        return;

    map_t* const pMapInfo = &g_mapInfo[(MAXLEVELS * volumeNum) + levelNum];

    Xfree(pMapInfo->musicfn);
    pMapInfo->musicfn = dup_filename(fileName);
    check_filename_case(pMapInfo->musicfn);
}

static bool C_ParseCommand(bool loop /*= false*/)
{
    int i, j = 0, k = 0, tw;

    do
    {
        if (quitevent)
        {
            LOG_F(INFO, "Aborted.");
            G_Shutdown();
            Bexit(EXIT_SUCCESS);
        }

        if (EDUKE32_PREDICT_FALSE(g_errorCnt > 63 || (*textptr == '\0') || (*(textptr + 1) == '\0')))
            return 1;

        if ((g_scriptPtr - apScript) > (g_scriptSize - 4096) && g_caseTablePtr == NULL)
            C_SetScriptSize(g_scriptSize << 1);

        if (g_scriptDebug)
            C_ReportError(-1);

        if (g_checkingSwitch > 0)
        {
            //Bsprintf(g_szBuf,"PC(): '%.25s'",textptr);
            //AddLog(g_szBuf);
        }

        //    Bsprintf(tempbuf,"%s",vm_keywords[tw]);
        //    AddLog(tempbuf);

        int const otw = g_lastKeyword;
        C_SkipComments();

        switch (g_lastKeyword = tw = C_GetNextKeyword())
        {
            default:
            case -1:
            case -2:
                return 1; //End
            case CON_DEFSTATE:
                if (EDUKE32_PREDICT_FALSE(g_processingState || g_scriptActorOffset))
                {
                    C_ReportError(ERROR_FOUNDWITHIN);
                    g_errorCnt++;
                    continue;
                }
                goto DO_DEFSTATE;
            case CON_STATE:
                if (!g_scriptActorOffset && g_processingState == 0)
                {
DO_DEFSTATE:
                    C_GetNextLabelName();
                    g_scriptPtr--;
                    labelcode[g_labelCnt] = g_scriptPtr - apScript;
                    labeltype[g_labelCnt] = LABEL_STATE;

                    g_processingState = 1;
                    Bsprintf(g_szCurrentBlockName, "%s", LAST_LABEL);

                    if (EDUKE32_PREDICT_FALSE(hash_find(&h_keywords, LAST_LABEL) >= 0))
                    {
                        g_errorCnt++;
                        C_ReportError(ERROR_ISAKEYWORD);
                        continue;
                    }

                    if (EDUKE32_PREDICT_FALSE(hash_find(&h_gamevars, LAST_LABEL) >= 0))
                    {
                        g_warningCnt++;
                        C_ReportError(WARNING_NAMEMATCHESVAR);
                    }

                    hash_add(&h_labels, LAST_LABEL, g_labelCnt, 0);
                    g_labelCnt++;
                    continue;
                }

                C_GetNextLabelName();

                if (EDUKE32_PREDICT_FALSE((j = hash_find(&h_labels, LAST_LABEL)) < 0))
                {
                    C_ReportError(-1);
                    LOG_F(ERROR, "%s:%d: error: state '%s' not found.", g_scriptFileName, g_lineNumber, LAST_LABEL);
                    g_errorCnt++;
                    g_scriptPtr++;
                    continue;
                }

                if (EDUKE32_PREDICT_FALSE((labeltype[j] & LABEL_STATE) != LABEL_STATE))
                {
                    char* gl = (char*)C_GetLabelType(labeltype[j]);
                    C_ReportError(-1);
                    LOG_F(WARNING, "%s:%d: expected state, found %s.", g_scriptFileName, g_lineNumber, gl);
                    g_warningCnt++;
                    Xfree(gl);
                    scriptWriteAtOffset(CON_NULLOP | LINE_NUMBER, &g_scriptPtr[-1]); // get rid of the state, leaving a nullop to satisfy if conditions
                    continue;  // valid label name, but wrong type
                }

                if (g_scriptDebug > 1 && !g_errorCnt && !g_warningCnt)
                    VLOG_F(LOG_CON, "%s:%d: debug: state label '%s'.", g_scriptFileName, g_lineNumber, label + (j << 6));

                // 'state' type labels are always script addresses, as far as I can see
                scriptWritePointer((intptr_t)(apScript + labelcode[j]), g_scriptPtr++);
                continue;

            case CON_ENDS:
                if (EDUKE32_PREDICT_FALSE(g_processingState == 0))
                {
                    C_ReportError(-1);
                    LOG_F(ERROR, "%s:%d: error: found 'ends' without open 'state'.", g_scriptFileName, g_lineNumber);
                    g_errorCnt++;
                }

                if (EDUKE32_PREDICT_FALSE(g_numBraces > 0))
                {
                    //C_ReportError(ERROR_NOTTOPLEVEL);
                    g_errorCnt++;
                }

                if (EDUKE32_PREDICT_FALSE(g_checkingSwitch > 0))
                {
                    C_ReportError(ERROR_NOENDSWITCH);
                    g_errorCnt++;

                    g_checkingSwitch = 0; // can't be checking anymore...
                }

                g_processingState = 0;
                Bsprintf(g_szCurrentBlockName, "(none)");
                continue;

            case CON_SETTHISPROJECTILE:
            case CON_SETPROJECTILE:
                if (!C_CheckEventSync(g_currentEvent))
                    C_ReportError(WARNING_EVENTSYNC);
                fallthrough__;
            case CON_GETTHISPROJECTILE:
            case CON_GETPROJECTILE:
            {
                int const labelNum = C_GetStructureIndexes(tw == CON_SETTHISPROJECTILE || tw == CON_GETTHISPROJECTILE, &h_projectile);

                if (labelNum == -1)
                    continue;

                scriptWriteValue(labelNum);

                switch (tw)
                {
                    case CON_SETPROJECTILE:
                    case CON_SETTHISPROJECTILE:
                        C_GetNextVar();
                        break;
                    default:
                        C_GetNextVarType(GAMEVAR_READONLY);
                        break;
                }
                continue;
            }

            case CON_GAMEVAR:
            {
                // syntax: gamevar <var1> <initial value> <flags>
                // defines var1 and sets initial value.
                // flags are used to define usage
                // (see top of this files for flags)

                //Skip comments before calling the check in order to align the textptr onto the label
                C_SkipComments();
                if (EDUKE32_PREDICT_FALSE(isdigit(*textptr) || (*textptr == '-')))
                {
                    g_errorCnt++;
                    C_ReportError(ERROR_SYNTAXERROR);
                    scriptSkipLine();
                    continue;
                }

                g_scriptPtr--;

                C_GetNextLabelName();

                if (EDUKE32_PREDICT_FALSE(hash_find(&h_keywords, LAST_LABEL) >= 0))
                {
                    g_warningCnt++;
                    //C_ReportError(WARNING_VARMASKSKEYWORD);
                    hash_delete(&h_keywords, LAST_LABEL);
                }

                int32_t defaultValue = 0;
                int32_t varFlags = 0;

                if (C_GetKeyword() == -1)
                {
                    C_GetNextValue(LABEL_DEFINE); // get initial value
                    defaultValue = *(--g_scriptPtr);

                    j = 0;

                    while (C_GetKeyword() == -1)
                        C_BitOrNextValue(&j);

                    C_FinishBitOr(j);
                    varFlags = *(--g_scriptPtr);

                    if (EDUKE32_PREDICT_FALSE((*(g_scriptPtr)&GAMEVAR_USER_MASK) == (GAMEVAR_PERPLAYER | GAMEVAR_PERACTOR)))
                    {
                        g_warningCnt++;
                        varFlags ^= GAMEVAR_PERPLAYER;
                        C_ReportError(WARNING_BADGAMEVAR);
                    }
                }

                Gv_NewVar(LAST_LABEL, defaultValue, varFlags);
                continue;
            }

            case CON_GAMEARRAY:
            {
                //Skip comments before calling the check in order to align the textptr onto the label
                C_SkipComments();
                if (EDUKE32_PREDICT_FALSE(isdigit(*textptr) || (*textptr == '-')))
                {
                    g_errorCnt++;
                    C_ReportError(ERROR_SYNTAXERROR);
                    scriptSkipLine();
                    continue;
                }
                C_GetNextLabelName();

                if (EDUKE32_PREDICT_FALSE(hash_find(&h_keywords, LAST_LABEL) >= 0))
                {
                    g_warningCnt++;
                    //C_ReportError(WARNING_ARRAYMASKSKEYWORD);
                    hash_delete(&h_keywords, LAST_LABEL);
                }

                i = hash_find(&h_gamevars, LAST_LABEL);
                if (EDUKE32_PREDICT_FALSE(i >= 0))
                {
                    g_warningCnt++;
                    C_ReportError(WARNING_NAMEMATCHESVAR);
                }

                C_GetNextValue(LABEL_DEFINE);

                char const* const arrayName = LAST_LABEL;
                int32_t arrayFlags = 0;

                while (C_GetKeyword() == -1)
                    C_BitOrNextValue(&arrayFlags);

                C_FinishBitOr(arrayFlags);

                arrayFlags = g_scriptPtr[-1];
                g_scriptPtr--;

                Gv_NewArray(arrayName, NULL, g_scriptPtr[-1], arrayFlags);

                g_scriptPtr -= 2; // no need to save in script...
                continue;
            }


            case CON_DEFINE:
            {
                C_GetNextLabelName();

                if (hash_find(&h_keywords, LAST_LABEL) >= 0)
                {
                    g_errorCnt++;
                    C_ReportError(ERROR_ISAKEYWORD);
                    continue;
                }

                i = hash_find(&h_gamevars, LAST_LABEL);
                if (i >= 0)
                {
                    g_warningCnt++;
                    C_ReportError(WARNING_NAMEMATCHESVAR);
                }

                C_GetNextValue(LABEL_DEFINE);

                i = hash_find(&h_labels, LAST_LABEL);
                if (i >= 0)
                {
                    if (EDUKE32_PREDICT_FALSE(labelcode[i] != g_scriptPtr[-1]))
                    {
                        g_warningCnt++;
                        LOG_F(WARNING, "%s:%d: ignored redefinition of '%s' to %d (old: %d).", g_scriptFileName,
                            g_lineNumber, LAST_LABEL, (int32_t)(g_scriptPtr[-1]), labelcode[i]);
                    }
                }
                else
                {
                    hash_add(&h_labels, LAST_LABEL, g_labelCnt, 0);

                    if ((unsigned)g_scriptPtr[-1] < MAXTILES && g_dynamicTileMapping)
                        G_ProcessDynamicNameMapping(LAST_LABEL, g_dynTileList, g_scriptPtr[-1]);

                    labeltype[g_labelCnt] = LABEL_DEFINE;
                    labelcode[g_labelCnt++] = g_scriptPtr[-1];
                }
                g_scriptPtr -= 2;
                continue;
            }

            case CON_PALFROM:
                for (j = 3; j >= 0; j--)
                {
                    if (C_GetKeyword() == -1)
                        C_GetNextValue(LABEL_DEFINE);
                    else break;
                }

                while (j > -1)
                {
                    scriptWriteValue(0);
                    j--;
                }
                continue;

            case CON_MOVE:
                if (g_scriptActorOffset || g_processingState)
                {
                    if (EDUKE32_PREDICT_FALSE((C_GetNextValue(LABEL_MOVE | LABEL_DEFINE) == 0) && (g_scriptPtr[-1] != 0) && (g_scriptPtr[-1] != 1)))
                    {
                        C_ReportError(-1);
                        scriptWriteAtOffset(0, &g_scriptPtr[-1]);
                        LOG_F(WARNING, "%s:%d: expected a move, found a constant.", g_scriptFileName, g_lineNumber);
                        g_warningCnt++;
                    }

                    j = 0;
                    while (C_GetKeyword() == -1)
                        C_BitOrNextValue(&j);

                    C_FinishBitOr(j);
                }
                else
                {
                    g_scriptPtr--;
                    scriptWriteValue(CON_MOVE);

                    C_GetNextLabelName();
                    // Check to see it's already defined

                    if (EDUKE32_PREDICT_FALSE(hash_find(&h_keywords, LAST_LABEL) >= 0))
                    {
                        g_errorCnt++;
                        C_ReportError(ERROR_ISAKEYWORD);
                        continue;
                    }

                    if (EDUKE32_PREDICT_FALSE(hash_find(&h_gamevars, LAST_LABEL) >= 0))
                    {
                        g_warningCnt++;
                        C_ReportError(WARNING_NAMEMATCHESVAR);
                    }

                    if (EDUKE32_PREDICT_FALSE((i = hash_find(&h_labels, LAST_LABEL)) >= 0))
                    {
                        g_warningCnt++;
                        LOG_F(WARNING, "%s:%d: duplicate move '%s' ignored.", g_scriptFileName, g_lineNumber, LAST_LABEL);
                    }
                    else
                    {
                        hash_add(&h_labels, LAST_LABEL, g_labelCnt, 0);
                        labeltype[g_labelCnt] = LABEL_MOVE;
                        labelcode[g_labelCnt++] = g_scriptPtr - apScript;
                    }

                    for (j = 1; j >= 0; j--)
                    {
                        if (C_GetKeyword() != -1) break;
                        C_GetNextValue(LABEL_DEFINE);
                    }

                    for (k = j; k >= 0; k--)
                        scriptWriteValue(0);

                    scriptWriteValue(CON_END);
                }
                continue;

            case CON_MUSIC:
            {
                // NOTE: this doesn't get stored in the PCode...

                // music 1 stalker.mid dethtoll.mid streets.mid watrwld1.mid snake1.mid
                //    thecall.mid ahgeez.mid dethtoll.mid streets.mid watrwld1.mid snake1.mid
                g_scriptPtr--;
                C_GetNextValue(LABEL_DEFINE); // Volume Number (0/4)
                g_scriptPtr--;

                k = *g_scriptPtr - 1;  // 0-based volume number. -1 or MAXVOLUMES: "special"
                if (k == -1)
                    k = MAXVOLUMES;

                if (EDUKE32_PREDICT_FALSE((unsigned)k >= MAXVOLUMES + 1)) // if it's not background or special music
                {
                    g_errorCnt++;
                    C_ReportError(-1);
                    LOG_F(ERROR, "%s:%d: volume number must be between 0 and MAXVOLUMES+1=%d.",
                        g_scriptFileName, g_lineNumber, MAXVOLUMES + 1);
                    continue;
                }

                i = 0;
                // get the file name...
                while (C_GetKeyword() == -1)
                {
                    C_SkipComments();

                    j = 0;
                    tempbuf[j] = '/';
                    while (isaltok(*(textptr + j)))
                    {
                        tempbuf[j + 1] = textptr[j];
                        j++;
                    }
                    tempbuf[j + 1] = '\0';

                    C_DefineMusic(k, i, tempbuf);

                    textptr += j;

                    if (i >= MAXLEVELS)
                        break;
                    i++;
                }
            }
            continue;

            case CON_INCLUDE:
                g_scriptPtr--;

                C_SkipComments();

                j = 0;
                while (isaltok(*textptr))
                {
                    tempbuf[j] = *(textptr++);
                    j++;
                }
                tempbuf[j] = '\0';

                C_Include(tempbuf);
                continue;

#if 0
            case CON_INCLUDEDEFAULT:
                C_SkipComments();
                C_Include(G_DefaultConFile());
                continue;
#endif

            case CON_AI:
                if (g_scriptActorOffset || g_processingState)
                {
                    C_GetNextValue(LABEL_AI);
                }
                else
                {
                    g_scriptPtr--;
                    scriptWriteValue(CON_AI);

                    C_GetNextLabelName();

                    if (EDUKE32_PREDICT_FALSE(hash_find(&h_keywords, LAST_LABEL) >= 0))
                    {
                        g_errorCnt++;
                        C_ReportError(ERROR_ISAKEYWORD);
                        continue;
                    }

                    i = hash_find(&h_gamevars, LAST_LABEL);
                    if (EDUKE32_PREDICT_FALSE(i >= 0))
                    {
                        g_warningCnt++;
                        C_ReportError(WARNING_NAMEMATCHESVAR);
                    }

                    i = hash_find(&h_labels, LAST_LABEL);
                    if (EDUKE32_PREDICT_FALSE(i >= 0))
                    {
                        g_warningCnt++;
                        LOG_F(WARNING, "%s:%d: duplicate ai '%s' ignored.", g_scriptFileName, g_lineNumber, LAST_LABEL);
                    }
                    else
                    {
                        labeltype[g_labelCnt] = LABEL_AI;
                        hash_add(&h_labels, LAST_LABEL, g_labelCnt, 0);
                        labelcode[g_labelCnt++] = g_scriptPtr - apScript;
                    }

                    for (j = 0; j < 3; j++)
                    {
                        if (C_GetKeyword() != -1) break;
                        if (j == 1)
                            C_GetNextValue(LABEL_ACTION);
                        else if (j == 2)
                        {
                            if (EDUKE32_PREDICT_FALSE((C_GetNextValue(LABEL_MOVE | LABEL_DEFINE) == 0) &&
                                (g_scriptPtr[-1] != 0) && (g_scriptPtr[-1] != 1)))
                            {
                                C_ReportError(-1);
                                scriptWriteAtOffset(0, &g_scriptPtr[-1]);
                                LOG_F(WARNING, "%s:%d: expected a move, found a constant.", g_scriptFileName, g_lineNumber);
                                g_warningCnt++;
                            }

                            k = 0;
                            while (C_GetKeyword() == -1)
                                C_BitOrNextValue(&k);

                            C_FinishBitOr(k);
                        }
                    }

                    for (k = j; k < 3; k++)
                        scriptWriteValue(0);

                    scriptWriteValue(CON_END);
                }
                continue;

            case CON_ACTION:
                if (g_scriptActorOffset || g_processingState)
                {
                    C_GetNextValue(LABEL_ACTION);
                }
                else
                {
                    g_scriptPtr--;
                    scriptWriteValue(CON_ACTION);

                    C_GetNextLabelName();
                    // Check to see it's already defined

                    if (EDUKE32_PREDICT_FALSE(hash_find(&h_keywords, LAST_LABEL) >= 0))
                    {
                        g_errorCnt++;
                        C_ReportError(ERROR_ISAKEYWORD);
                        continue;
                    }

                    i = hash_find(&h_gamevars, LAST_LABEL);
                    if (EDUKE32_PREDICT_FALSE(i >= 0))
                    {
                        g_warningCnt++;
                        C_ReportError(WARNING_NAMEMATCHESVAR);
                    }

                    i = hash_find(&h_labels, LAST_LABEL);
                    if (EDUKE32_PREDICT_FALSE(i >= 0))
                    {
                        g_warningCnt++;
                        LOG_F(WARNING, "%s:%d: duplicate action '%s' ignored.", g_scriptFileName, g_lineNumber, LAST_LABEL);
                    }
                    else
                    {
                        labeltype[g_labelCnt] = LABEL_ACTION;
                        labelcode[g_labelCnt] = g_scriptPtr - apScript;
                        hash_add(&h_labels, LAST_LABEL, g_labelCnt, 0);
                        g_labelCnt++;
                    }

                    for (j = ACTION_PARAM_COUNT - 1; j >= 0; j--)
                    {
                        if (C_GetKeyword() != -1) break;
                        C_GetNextValue(LABEL_DEFINE);
                    }
                    for (k = j; k >= 0; k--)
                        scriptWriteValue(0);

                    scriptWriteValue(CON_END);
                }
                continue;

            case CON_ACTOR:
            case CON_USERACTOR:
            case CON_EVENTLOADACTOR:
                if (EDUKE32_PREDICT_FALSE(g_processingState || g_scriptActorOffset))
                {
                    C_ReportError(ERROR_FOUNDWITHIN);
                    g_errorCnt++;
                }

                g_numBraces = 0;
                g_scriptPtr--;
                g_scriptActorOffset = g_scriptPtr - apScript;

                if (tw == CON_USERACTOR)
                {
                    C_GetNextValue(LABEL_DEFINE);
                    g_scriptPtr--;
                }

                // save the actor name w/o consuming it
                C_SkipComments();
                j = 0;
                while (isaltok(*(textptr + j)))
                {
                    g_szCurrentBlockName[j] = textptr[j];
                    j++;
                }
                g_szCurrentBlockName[j] = 0;

                j = hash_find(&h_labels, g_szCurrentBlockName);

                if (j != -1)
                    labeltype[j] |= LABEL_ACTOR;

                if (tw == CON_USERACTOR)
                {
                    j = *g_scriptPtr;

                    if (EDUKE32_PREDICT_FALSE(j > 6 || (j & 3) == 3))
                    {
                        C_ReportError(-1);
                        LOG_F(WARNING, "%s:%d: invalid useractor type. Must be 0, 1, 2"
                            " (notenemy, enemy, enemystayput) or have 4 added (\"doesn't move\").",
                            g_scriptFileName, g_lineNumber);
                        g_warningCnt++;
                        j = 0;
                    }
                }

                C_GetNextValue(LABEL_ACTOR);
                g_scriptPtr--;

                if (EDUKE32_PREDICT_FALSE((unsigned)*g_scriptPtr >= MAXTILES))
                {
                    C_ReportError(ERROR_EXCEEDSMAXTILES);
                    g_errorCnt++;
                    continue;
                }

                if (tw == CON_EVENTLOADACTOR)
                {
                    g_tile[*g_scriptPtr].loadPtr = apScript + g_scriptActorOffset;
                    g_checkingIfElse = 0;
                    continue;
                }

                g_tile[*g_scriptPtr].execPtr = apScript + g_scriptActorOffset;

                if (tw == CON_USERACTOR)
                {
                    if (j & 1) g_tile[*g_scriptPtr].flags |= SFLAG_BADGUY;
                    if (j & 2) g_tile[*g_scriptPtr].flags |= (SFLAG_BADGUY | SFLAG_BADGUYSTAYPUT);
                    if (j & 4) g_tile[*g_scriptPtr].flags |= SFLAG_ROTFIXED;
                }

                for (j = 0; j < 4; j++)
                {
                    scriptWriteAtOffset(0, apScript + g_scriptActorOffset + j);
                    if (j == 3)
                    {
                        j = 0;
                        while (C_GetKeyword() == -1)
                            C_BitOrNextValue(&j);

                        C_FinishBitOr(j);
                        break;
                    }
                    else
                    {
                        if (C_GetKeyword() != -1)
                        {
                            for (i = 4 - j; i; i--)
                            {
                                scriptWriteValue(0);
                            }
                            break;
                        }
                        switch (j)
                        {
                            case 0:
                                C_GetNextValue(LABEL_DEFINE);
                                break;
                            case 1:
                                C_GetNextValue(LABEL_ACTION);
                                break;
                            case 2:
                                // XXX: LABEL_MOVE|LABEL_DEFINE, what is this shit? compatibility?
                                // yep, it sure is :(
                                if (EDUKE32_PREDICT_FALSE((C_GetNextValue(LABEL_MOVE | LABEL_DEFINE) == 0) && (g_scriptPtr[-1] != 0) && (g_scriptPtr[-1] != 1)))
                                {
                                    C_ReportError(-1);
                                    scriptWriteAtOffset(0, &g_scriptPtr[-1]);
                                    LOG_F(WARNING, "%s:%d: expected a move, found a constant.", g_scriptFileName, g_lineNumber);
                                    g_warningCnt++;
                                }
                                break;
                        }

                        if (g_scriptPtr[-1] >= (intptr_t)apScript && g_scriptPtr[-1] < (intptr_t)&apScript[g_scriptSize])
                            scriptWritePointer(g_scriptPtr[-1], apScript + g_scriptActorOffset + j);
                        else
                            scriptWriteAtOffset(g_scriptPtr[-1], apScript + g_scriptActorOffset + j);
                    }
                }
                g_checkingIfElse = 0;
                continue;

            case CON_ONEVENT:
            case CON_APPENDEVENT:
                if (EDUKE32_PREDICT_FALSE(g_processingState || g_scriptActorOffset))
                {
                    C_ReportError(ERROR_FOUNDWITHIN);
                    g_errorCnt++;
                }

                g_numBraces = 0;
                g_scriptPtr--;
                g_scriptEventOffset = g_scriptActorOffset = g_scriptPtr - apScript;

                C_SkipComments();
                j = 0;
                while (isaltok(*(textptr + j)))
                {
                    g_szCurrentBlockName[j] = textptr[j];
                    j++;
                }
                g_szCurrentBlockName[j] = 0;
                //        g_labelsOnly = 1;
                C_GetNextValue(LABEL_EVENT);
                g_labelsOnly = 0;
                g_scriptPtr--;
                j = *g_scriptPtr;  // type of event
                g_currentEvent = j;
                //Bsprintf(g_szBuf,"Adding Event for %d at %lX",j, g_parsingEventPtr);
                //AddLog(g_szBuf);
                if (EDUKE32_PREDICT_FALSE((unsigned)j > MAXEVENTS - 1))
                {
                    LOG_F(ERROR, "%s:%d: invalid event ID.", g_scriptFileName, g_lineNumber);
                    g_errorCnt++;
                    continue;
                }
                // if event has already been declared then store previous script location
                if (!apScriptEvents[j])
                {
                    apScriptEvents[j] = g_scriptEventOffset;
                }
                else if (tw == CON_ONEVENT)
                {
                    g_scriptEventChainOffset = apScriptEvents[j];
                    apScriptEvents[j] = g_scriptEventOffset;
                }
                else // if (tw == CON_APPENDEVENT)
                {
                    auto previous_event_end = apScript + apScriptGameEventEnd[j];
                    scriptWriteAtOffset(CON_JUMP | LINE_NUMBER, previous_event_end++);
                    scriptWriteAtOffset(GV_FLAG_CONSTANT, previous_event_end++);
                    C_FillEventBreakStackWithJump((intptr_t*)*previous_event_end, g_scriptEventOffset);
                    scriptWriteAtOffset(g_scriptEventOffset, previous_event_end++);
                }

                g_checkingIfElse = 0;

                continue;

            case CON_FALL:
            case CON_TIP:
            case CON_KILLIT:
            case CON_RESETACTIONCOUNT:
            case CON_PSTOMP:
            case CON_RESETPLAYER:
            case CON_RESETCOUNT:
            case CON_WACKPLAYER:
            case CON_OPERATE:
            case CON_RESPAWNHITAG:
            case CON_GETLASTPAL:
            case CON_PKICK:
            case CON_MIKESND:
            case CON_TOSSWEAPON:
            case CON_INSERTSPRITEQ:
                if (!C_CheckEventSync(g_currentEvent))
                {
                    C_ReportError(WARNING_EVENTSYNC);
                    g_warningCnt++;
                }
                fallthrough__;
            case CON_STOPALLSOUNDS:
            case CON_SPGETLOTAG: // !DEPRECATED!
            case CON_SPGETHITAG: // !DEPRECATED!
            case CON_SECTGETLOTAG: // !DEPRECATED!
            case CON_SECTGETHITAG: // !DEPRECATED!
            case CON_GETTEXTUREFLOOR:
            case CON_GETTEXTURECEILING:
            case CON_FLASH:
            case CON_SAVEMAPSTATE:
            case CON_LOADMAPSTATE:
                // No Parameters
                continue;

            case CON_NULLOP:
                if (C_GetKeyword() != CON_ELSE)
                    LOG_F(WARNING, "%s:%d: found 'nullop' without 'else'", g_scriptFileName, g_lineNumber);
                continue;

            case CON_QSPRINTF:
                C_GetManyVars(2);

                j = 0;

                while (C_GetKeyword() == -1 && j < 32)
                    C_GetNextVar(), j++;

                scriptWriteValue(CON_NULLOP | LINE_NUMBER);
                continue;

            case CON_ESPAWN:
            case CON_QSPAWN:
            case CON_EQSPAWN:
            case CON_STRENGTH:
            case CON_ADDPHEALTH:
            case CON_SPAWN:
            case CON_CSTAT:
            case CON_COUNT:
            case CON_ENDOFGAME:
            case CON_SPRITEPAL:
            case CON_CACTOR:
            case CON_MONEY:
            case CON_ADDKILLS:
            case CON_DEBUG:
            case CON_ADDSTRENGTH:
            case CON_CSTATOR:
            case CON_MAIL:
            case CON_PAPER:
            case CON_SLEEPTIME:
            case CON_CLIPDIST:
            case CON_LOTSOFGLASS:
            case CON_SAVENN:
            case CON_SAVE:
                if (!C_CheckEventSync(g_currentEvent))
                {
                    C_ReportError(WARNING_EVENTSYNC);
                    g_warningCnt++;
                }
                fallthrough__;
            case CON_ANGOFF:
            case CON_QUOTE:
            case CON_SOUNDONCE:
            case CON_STOPSOUND:
                C_GetNextValue(LABEL_DEFINE);
                if (tw == CON_CSTAT)
                {
                    if (EDUKE32_PREDICT_FALSE(g_scriptPtr[-1] == 32767))
                    {
                        g_scriptPtr[-1] = 32768;
                        C_ReportError(-1);
                        LOG_F(WARNING, "%s:%d: tried to set cstat 32767, using 32768 instead.", g_scriptFileName, g_lineNumber);
                        g_warningCnt++;
                    }
                    else if (EDUKE32_PREDICT_FALSE((g_scriptPtr[-1] & 48) == 48))
                    {
                        i = g_scriptPtr[-1];
                        g_scriptPtr[-1] ^= 48;
                        C_ReportError(-1);
                        LOG_F(WARNING, "%s:%d: tried to set cstat %d, using %d instead.", g_scriptFileName, g_lineNumber, i, (int32_t)(g_scriptPtr[-1]));
                        g_warningCnt++;
                    }
                }
                continue;

            case CON_HITRADIUSVAR:
                if (!C_CheckEventSync(g_currentEvent))
                {
                    C_ReportError(WARNING_EVENTSYNC);
                    g_warningCnt++;
                }
                C_GetManyVars(5);
                continue;
            case CON_HITRADIUS:
                C_GetNextValue(LABEL_DEFINE);
                C_GetNextValue(LABEL_DEFINE);
                C_GetNextValue(LABEL_DEFINE);
                fallthrough__;
            case CON_ADDAMMO:
            case CON_ADDWEAPON:
            case CON_SIZETO:
            case CON_SIZEAT:
            case CON_DEBRIS:
            case CON_ADDINVENTORY:
            case CON_GUTS:
                if (!C_CheckEventSync(g_currentEvent))
                {
                    C_ReportError(WARNING_EVENTSYNC);
                    g_warningCnt++;
                }
                C_GetNextValue(LABEL_DEFINE);
                C_GetNextValue(LABEL_DEFINE);
                continue;

            case CON_ELSE:
            {
                if (EDUKE32_PREDICT_FALSE(!g_checkingIfElse))
                {
                    g_scriptPtr--;
                    auto const tempscrptr = g_scriptPtr;
                    g_warningCnt++;
                    C_ReportError(-1);

                    LOG_F(WARNING, "%s:%d: found 'else' with no 'if'", g_scriptFileName, g_lineNumber);

                    if (C_GetKeyword() == CON_LEFTBRACE)
                    {
                        C_GetNextKeyword();
                        g_numBraces++;

                        C_ParseCommand(true);
                    }
                    else C_ParseCommand();

                    g_scriptPtr = tempscrptr;

                    continue;
                }

                intptr_t const lastScriptPtr = &g_scriptPtr[-1] - apScript;

                g_skipBranch = false;
                g_checkingIfElse--;

                if (C_CheckMalformedBranch(lastScriptPtr))
                    continue;

                intptr_t const offset = (unsigned)(g_scriptPtr - apScript);

                g_scriptPtr++; //Leave a spot for the fail location

                C_ParseCommand();

                if (C_CheckEmptyBranch(tw, lastScriptPtr))
                    continue;

                auto const tempscrptr = (intptr_t*)apScript + offset;
                scriptWritePointer((intptr_t)g_scriptPtr, tempscrptr);

                continue;
            }

            case CON_SETSECTOR:
                if (!C_CheckEventSync(g_currentEvent))
                {
                    C_ReportError(WARNING_EVENTSYNC);
                    g_warningCnt++;
                }
                fallthrough__;
            case CON_GETSECTOR:
            {
                intptr_t* const ins = &g_scriptPtr[-1];
                int const labelNum = C_GetStructureIndexes(1, &h_sector);

                if (labelNum == -1)
                    continue;

                Bassert((*ins & VM_INSTMASK) == tw);

                auto const& label = SectorLabels[labelNum];

                int32_t const opcode = (tw == CON_GETSECTOR) ? CON_GETSECTORSTRUCT : CON_SETSECTORSTRUCT;
                int32_t const labelFunc = (tw == CON_GETSECTOR) ? LABEL_READFUNC : LABEL_WRITEFUNC;
                if (label.offset != -1 && (label.flags & labelFunc) == 0)
                    *ins = opcode | LINE_NUMBER;

                scriptWriteValue(labelNum);

                C_GetNextVarType(tw == CON_GETSECTOR ? GAMEVAR_READONLY : 0);

                continue;
            }

            case CON_FINDNEARACTOR:
            case CON_FINDNEARACTOR3D:
            case CON_FINDNEARSPRITE:
            case CON_FINDNEARSPRITE3D:
            case CON_FINDNEARACTORZ:
            case CON_FINDNEARSPRITEZ:
            {
                // syntax findnearactor <type> <maxdist> <getvar>
                // gets the sprite ID of the nearest actor within max dist
                // that is of <type> into <getvar>
                // -1 for none found

                C_GetNextValue(LABEL_DEFINE); // get <type>
                C_GetNextValue(LABEL_DEFINE); // get maxdist

                switch (tw)
                {
                    case CON_FINDNEARACTORZ:
                    case CON_FINDNEARSPRITEZ:
                        C_GetNextValue(LABEL_DEFINE);
                    default:
                        break;
                }

                // target var
                // get the ID of the DEF
                C_GetNextVarType(GAMEVAR_READONLY);
                continue;
            }

            case CON_FINDNEARACTORVAR:
            case CON_FINDNEARACTOR3DVAR:
            case CON_FINDNEARSPRITEVAR:
            case CON_FINDNEARSPRITE3DVAR:
            case CON_FINDNEARACTORZVAR:
            case CON_FINDNEARSPRITEZVAR:
            {
                C_GetNextValue(LABEL_DEFINE); // get <type>

                // get the ID of the DEF
                C_GetNextVar();
                switch (tw)
                {
                    case CON_FINDNEARACTORZVAR:
                    case CON_FINDNEARSPRITEZVAR:
                        C_GetNextVar();
                    default:
                        break;
                }
                // target var
                // get the ID of the DEF
                C_GetNextVarType(GAMEVAR_READONLY);
                continue;
            }

            case CON_SQRT:
            {
                // syntax sqrt <invar> <outvar>
                // gets the sqrt of invar into outvar

                // get the ID of the DEF
                C_GetNextVar();
                // target var
                // get the ID of the DEF
                C_GetNextVarType(GAMEVAR_READONLY);
                continue;
            }

            case CON_SETWALL:
                if (!C_CheckEventSync(g_currentEvent))
                {
                    C_ReportError(WARNING_EVENTSYNC);
                    g_warningCnt++;
                }
                fallthrough__;
            case CON_GETWALL:
            {
                intptr_t* const ins = &g_scriptPtr[-1];
                int const labelNum = C_GetStructureIndexes(1, &h_wall);

                if (labelNum == -1)
                    continue;

                Bassert((*ins & VM_INSTMASK) == tw);

                auto const& label = WallLabels[labelNum];

                int32_t const opcode = (tw == CON_GETWALL) ? CON_GETWALLSTRUCT : CON_SETWALLSTRUCT;
                int32_t const labelFunc = (tw == CON_GETWALL) ? LABEL_READFUNC : LABEL_WRITEFUNC;
                if (label.offset != -1 && (label.flags & labelFunc) == 0)
                    *ins = opcode | LINE_NUMBER;

                scriptWriteValue(labelNum);

                C_GetNextVarType(tw == CON_GETWALL ? GAMEVAR_READONLY : 0);
                continue;
            }

            case CON_SETPLAYER:
                if (!C_CheckEventSync(g_currentEvent))
                {
                    C_ReportError(WARNING_EVENTSYNC);
                    g_warningCnt++;
                }
                fallthrough__;
            case CON_GETPLAYER:
            {
                intptr_t* const ins = &g_scriptPtr[-1];
                int const labelNum = C_GetStructureIndexes(1, &h_player);

                if (labelNum == -1)
                    continue;

                Bassert((*ins & VM_INSTMASK) == tw);

                auto const& label = PlayerLabels[labelNum];

                int32_t const opcode = (tw == CON_GETPLAYER) ? CON_GETPLAYERSTRUCT : CON_SETPLAYERSTRUCT;
                int32_t const labelFunc = (tw == CON_GETPLAYER) ? LABEL_READFUNC : LABEL_WRITEFUNC;
                if (label.offset != -1 && (label.flags & (labelFunc | LABEL_HASPARM2)) == 0)
                    *ins = opcode | LINE_NUMBER;

                scriptWriteValue(labelNum);

                if (label.flags & LABEL_HASPARM2)
                    C_GetNextVar();

                C_GetNextVarType(tw == CON_GETPLAYER ? GAMEVAR_READONLY : 0);
                continue;
            }

            case CON_SETINPUT:
                if (!C_CheckEventSync(g_currentEvent))
                {
                    C_ReportError(WARNING_EVENTSYNC);
                    g_warningCnt++;
                }
                fallthrough__;
            case CON_GETINPUT:
            {
                int const labelNum = C_GetStructureIndexes(1, &h_input);

                if (labelNum == -1)
                    continue;

                scriptWriteValue(labelNum);

                C_GetNextVarType(tw == CON_GETINPUT ? GAMEVAR_READONLY : 0);
                continue;
            }

            case CON_SETTILEDATA:
                if (!C_CheckEventSync(g_currentEvent))
                {
                    C_ReportError(WARNING_EVENTSYNC);
                    g_warningCnt++;
                }
                fallthrough__;
            case CON_GETTILEDATA:
            {
                int const labelNum = C_GetStructureIndexes(0, &h_tiledata);

                if (labelNum == -1)
                    continue;

                scriptWriteValue(labelNum);

                C_GetNextVarType((tw == CON_GETTILEDATA) ? GAMEVAR_READONLY : 0);
                continue;
            }

            case CON_SETUSERDEF:
            case CON_GETUSERDEF:
            {
                // now get name of .xxx
                while (*textptr != '.')
                {
                    if (*textptr == 0xa || !*textptr)
                        break;

                    textptr++;
                }

                if (EDUKE32_PREDICT_FALSE(*textptr != '.'))
                {
                    g_errorCnt++;
                    C_ReportError(ERROR_SYNTAXERROR);
                    continue;
                }
                textptr++;
                C_GetNextLabelName();

                int const labelNum = C_GetLabelNameOffset(&h_userdef, Bstrtolower(LAST_LABEL));

                if (EDUKE32_PREDICT_FALSE(labelNum == -1))
                {
                    g_errorCnt++;
                    C_ReportError(ERROR_NOTAMEMBER);
                    continue;
                }
                scriptWriteValue(labelNum);

                if (UserdefsLabels[labelNum].flags & LABEL_HASPARM2)
                    C_GetNextVar();

                C_GetNextVarType((tw == CON_GETUSERDEF) ? GAMEVAR_READONLY : 0);
                continue;
            }

            case CON_SETACTORVAR:
            case CON_SETPLAYERVAR:
                if (!C_CheckEventSync(g_currentEvent))
                {
                    C_ReportError(WARNING_EVENTSYNC);
                    g_warningCnt++;
                }
                fallthrough__;
            case CON_GETACTORVAR:
            case CON_GETPLAYERVAR:
            {
                // syntax [gs]etactorvar[<var>].<varx> <VAR>
                // gets the value of the per-actor variable varx into VAR

                if (C_GetStructureIndexes(1, NULL) == -1)
                    continue;

                if (g_scriptPtr[-1] == sysVarIDs.THISACTOR) // convert to "setvarvar"
                {
                    g_scriptPtr--;
                    g_scriptPtr[-1] = CON_SETVARVAR;
                    if (tw == CON_SETACTORVAR || tw == CON_SETPLAYERVAR)
                    {
                        tw = inthash_find(&h_varvar, tw);
                        goto setvarvar;
                    }
                    else
                    {
                        g_scriptPtr++;
                        C_GetNextVar();
                        g_scriptPtr -= 2;
                        C_GetNextVarType(GAMEVAR_READONLY);
                        g_scriptPtr++;
                    }
                    continue;
                }

                /// now pointing at 'xxx'

                C_GetNextLabelName();

                if (EDUKE32_PREDICT_FALSE(hash_find(&h_keywords, LAST_LABEL) >= 0))
                {
                    g_errorCnt++;
                    C_ReportError(ERROR_ISAKEYWORD);
                    continue;
                }

                i = GetDefID(LAST_LABEL);

                if (EDUKE32_PREDICT_FALSE(i < 0))
                {
                    g_errorCnt++;
                    C_ReportError(ERROR_NOTAGAMEVAR);
                    continue;
                }
                if (EDUKE32_PREDICT_FALSE(aGameVars[i].flags & GAMEVAR_READONLY))
                {
                    g_errorCnt++;
                    C_ReportError(ERROR_VARREADONLY);
                    continue;
                }

                switch (tw)
                {
                    case CON_SETACTORVAR:
                        if (EDUKE32_PREDICT_FALSE(!(aGameVars[i].flags & GAMEVAR_PERACTOR)))
                        {
                            g_errorCnt++;
                            C_ReportError(-1);
                            LOG_F(ERROR, "%s:%d: variable '%s' is not per-actor.", g_scriptFileName, g_lineNumber, LAST_LABEL);
                            continue;
                        }
                        break;
                    case CON_SETPLAYERVAR:
                        if (EDUKE32_PREDICT_FALSE(!(aGameVars[i].flags & GAMEVAR_PERPLAYER)))
                        {
                            g_errorCnt++;
                            C_ReportError(-1);
                            LOG_F(ERROR, "%s:%d: variable '%s' is not per-player.", g_scriptFileName, g_lineNumber, LAST_LABEL);
                            continue;
                        }
                        break;
                }

                scriptWriteValue(i); // the ID of the DEF (offset into array...)

                switch (tw)
                {
                    case CON_GETACTORVAR:
                    case CON_GETPLAYERVAR:
                        C_GetNextVarType(GAMEVAR_READONLY);
                        break;
                    default:
                        C_GetNextVar();
                        break;
                }
                continue;
            }

            case CON_SETACTOR:
                // Event sync check moved below, so we can check if we're setting a sync-safe value.
                fallthrough__;
            case CON_GETACTOR:
            {
                intptr_t* const ins = &g_scriptPtr[-1];
                int const labelNum = C_GetStructureIndexes(1, &h_actor);

                if (labelNum == -1)
                    continue;

                Bassert((*ins & VM_INSTMASK) == tw);

                auto const& label = ActorLabels[labelNum];

                if (tw == CON_GETACTOR)
                {
                    if (label.offset != -1 && (label.flags & (LABEL_READFUNC | LABEL_HASPARM2)) == 0)
                    {
                        if (labelNum >= ACTOR_SPRITEEXT_BEGIN)
                            *ins = CON_GETSPRITEEXT | LINE_NUMBER;
                        else if (labelNum >= ACTOR_STRUCT_BEGIN)
                            *ins = CON_GETACTORSTRUCT | LINE_NUMBER;
                        else
                            *ins = CON_GETSPRITESTRUCT | LINE_NUMBER;
                    }
                }
                else
                {
                    if (label.offset != -1 && (label.flags & (LABEL_WRITEFUNC | LABEL_HASPARM2)) == 0)
                    {
                        if (labelNum >= ACTOR_SPRITEEXT_BEGIN)
                            *ins = CON_SETSPRITEEXT | LINE_NUMBER;
                        else if (labelNum >= ACTOR_STRUCT_BEGIN)
                            *ins = CON_SETACTORSTRUCT | LINE_NUMBER;
                        else
                            *ins = CON_SETSPRITESTRUCT | LINE_NUMBER;
                    }

                    // Check if were setting something that isn't sync safe in a local event
                    if (!C_CheckEventSync(g_currentEvent))
                    {
                        switch (label.lId)
                        {
                            case ACTOR_ALPHA: break;
                            default:
                                C_ReportError(WARNING_EVENTSYNC);
                                g_warningCnt++;
                                break;
                        }
                    }
                }

                scriptWriteValue(labelNum);

                if (label.flags & LABEL_HASPARM2)
                    C_GetNextVar();

                C_GetNextVarType((tw == CON_GETACTOR) ? GAMEVAR_READONLY : 0);
                continue;
            }

            case CON_GETTSPR:
            case CON_SETTSPR:
            {
                if (g_currentEvent != EVENT_ANIMATESPRITES)
                {
                    C_ReportError(-1);
                    LOG_F(WARNING, "%s:%d: found `%s' outside of EVENT_ANIMATESPRITES", g_scriptFileName, g_lineNumber, tempbuf);
                    g_warningCnt++;
                }

                int const labelNum = C_GetStructureIndexes(1, &h_tsprite);

                if (labelNum == -1)
                    continue;

                scriptWriteValue(labelNum);

                C_GetNextVarType((tw == CON_GETTSPR) ? GAMEVAR_READONLY : 0);
                continue;
            }

            case CON_GETTICKS:
                if (C_CheckEventSync(g_currentEvent))
                {
                    C_ReportError(WARNING_REVEVENTSYNC);
                    g_warningCnt++;
                }
                fallthrough__;
            case CON_GETCURRADDRESS:
            case CON_INV:
                C_GetNextVarType(GAMEVAR_READONLY);
                continue;

            case CON_SHOOT:
            case CON_ESHOOT:
            case CON_ESPAWNVAR:
            case CON_QSPAWNVAR:
            case CON_EQSPAWNVAR:
            case CON_OPERATERESPAWNS:
            case CON_OPERATEMASTERSWITCHES:
            case CON_CHECKACTIVATORMOTION:
            case CON_TIME:
            case CON_INITTIMER:
            case CON_LOCKPLAYER:
            case CON_QUAKE:
                if (!C_CheckEventSync(g_currentEvent))
                {
                    C_ReportError(WARNING_EVENTSYNC);
                    g_warningCnt++;
                }
                fallthrough__;
            case CON_JUMP:
            case CON_CMENU:
            case CON_SOUND:
            case CON_GLOBALSOUND:
            case CON_SCREENSOUND:
            case CON_STOPSOUNDVAR:
            case CON_SOUNDONCEVAR:
            case CON_ANGOFFVAR:
            case CON_CHECKAVAILWEAPON:
            case CON_CHECKAVAILINVEN:
            case CON_GUNIQHUDID:
            case CON_SAVEGAMEVAR:
            case CON_READGAMEVAR:
            case CON_USERQUOTE:
            case CON_STARTTRACK:
            case CON_CLEARMAPSTATE:
            case CON_ACTIVATECHEAT:
            case CON_SETGAMEPALETTE:
            case CON_SECTCLEARINTERPOLATION:
            case CON_SECTSETINTERPOLATION:
                C_GetNextVar();
                continue;

            case CON_ENHANCED: // !DEPRECATED!
            {
                // don't store in pCode...
                g_scriptPtr--;
                //printf("We are enhanced, baby...\n");
                C_GetNextValue(LABEL_DEFINE);
                g_scriptPtr--;
                if (*g_scriptPtr > BYTEVERSION_NETDUKE32)
                {
                    g_warningCnt++;
                    LOG_F(WARNING, "%s:%d: need build %d, found build %d", g_scriptFileName, g_lineNumber, k, BYTEVERSION_NETDUKE32);
                }
                continue;
            }

            case CON_DYNAMICREMAP:
                g_scriptPtr--;
                if (EDUKE32_PREDICT_FALSE(g_dynamicTileMapping))
                {
                    LOG_F(WARNING, "%s:%d: duplicate dynamicremap statement", g_scriptFileName, g_lineNumber);
                    g_warningCnt++;
                }
#ifdef USE_DNAMES
#ifdef DEBUGGINGAIDS
                else
                    VLOG_F(LOG_CON, "Using dynamic tile remapping");
#endif
                g_dynamicTileMapping = 1;
#else
                else
                {
                    LOG_F(WARNING, "%s:%d: dynamic tile remapping is disabled in this build", g_scriptFileName, g_lineNumber);
                    g_warningCnt++;
                }
#endif
                continue;

            case CON_DYNAMICSOUNDREMAP:
                g_scriptPtr--;
                if (EDUKE32_PREDICT_FALSE(g_dynamicSoundMapping))
                {
                    LOG_F(WARNING, "%s:%d: duplicate dynamicsoundremap statement", g_scriptFileName, g_lineNumber);
                    g_warningCnt++;
                }
                else
#ifdef USE_DNAMES
#ifdef DEBUGGINGAIDS
                    VLOG_F(LOG_CON, "Using dynamic sound remapping");
#endif

                g_dynamicSoundMapping = 1;
#else
                {
                    LOG_F(WARNING, "%s:%d: dynamic sound remapping is disabled in this build", g_scriptFileName, g_lineNumber);
                    g_warningCnt++;
                }
#endif
                continue;

            case CON_RANDVAR:
                if (!C_CheckEventSync(g_currentEvent))
                {
                    C_ReportError(WARNING_EVENTSYNC);
                    g_warningCnt++;
                }
                fallthrough__;
            case CON_ADDVAR:
            case CON_ANDVAR:
            case CON_DISPLAYRANDVAR:
            case CON_DIVVAR:
            case CON_MODVAR:
            case CON_MULVAR:
            case CON_ORVAR:
            case CON_SETVAR:
            case CON_SHIFTVARL:
            case CON_SHIFTVARR:
            case CON_SUBVAR:
            case CON_XORVAR:
setvar:
            {
                auto ins = &g_scriptPtr[-1];

                C_GetNextVarType(GAMEVAR_READONLY);
                C_GetNextValue(LABEL_DEFINE);

                // replace divides and multiplies by 0 with an error asking if the user is stupid
                if (ins[2] == 0 && (tw == CON_MODVAR || tw == CON_MULVAR || tw == CON_DIVVAR))
                {
                    g_errorCnt++;
                    C_ReportError(-1);
                    LOG_F(ERROR, "%s:%d: divide or multiply by zero! What are you doing?", g_scriptFileName, g_lineNumber);
                    continue;
                }
                else if (tw == CON_DIVVAR || tw == CON_MULVAR)
                {
                    auto const i = ins[2];
                    // replace multiplies or divides by 1 with nullop
                    if (i == 1)
                    {
                        int constexpr const opcode = CON_NULLOP;

                        if (!g_errorCnt && !g_warningCnt && g_scriptDebug > 1)
                        {
                            VLOG_F(LOG_CON, "%s:%d: %s -> %s", g_scriptFileName, g_lineNumber,
                                VM_GetKeywordForID(tw), VM_GetKeywordForID(opcode));
                        }

                        scriptWriteAtOffset(opcode | LINE_NUMBER, ins);
                        g_scriptPtr = &ins[1];
                    }
                    // replace multiplies or divides by -1 with inversion
                    else if (i == -1)
                    {
                        int constexpr const opcode = CON_INV;

                        if (g_scriptDebug > 1 && !g_errorCnt && !g_warningCnt)
                        {
                            VLOG_F(LOG_CON, "%s:%d: %s -> %s", g_scriptFileName, g_lineNumber,
                                VM_GetKeywordForID(tw), VM_GetKeywordForID(opcode));
                        }

                        scriptWriteAtOffset(opcode | LINE_NUMBER, ins);
                        g_scriptPtr--;
                    }
                }
                // replace instructions with special versions for specific var types
                scriptUpdateOpcodeForVariableType(ins);
                continue;
            }

            case CON_SETARRAY:
                C_GetNextLabelName();
                i = GetADefID(LAST_LABEL);
                if (i > (-1))
                {
                    bitptr[(g_scriptPtr - apScript) >> 3] &= ~(1 << ((g_scriptPtr - apScript) & 7));
                    *g_scriptPtr++ = i;
                }
                else
                    C_ReportError(ERROR_NOTAGAMEARRAY);
                C_SkipComments();// skip comments and whitespace
                if (*textptr != '[')
                {
                    g_errorCnt++;
                    C_ReportError(ERROR_GAMEARRAYBNO);
                    return 1;
                }
                textptr++;
                C_GetNextVar();
                C_SkipComments();// skip comments and whitespace
                if (*textptr != ']')
                {
                    g_errorCnt++;
                    C_ReportError(ERROR_GAMEARRAYBNC);
                    return 1;
                }
                textptr++;
                C_GetNextVar();
                continue;
            case CON_GETARRAYSIZE:
            case CON_RESIZEARRAY:
                C_GetNextLabelName();
                i = GetADefID(LAST_LABEL);
                if (i > (-1))
                {
                    bitptr[(g_scriptPtr - apScript) >> 3] &= ~(1 << ((g_scriptPtr - apScript) & 7));
                    *g_scriptPtr++ = i;
                }
                else
                    C_ReportError(ERROR_NOTAGAMEARRAY);
                C_SkipComments();
                C_GetNextVar();
                continue;

            case CON_RANDVARVAR:
                if (!C_CheckEventSync(g_currentEvent))
                {
                    C_ReportError(WARNING_EVENTSYNC);
                    g_warningCnt++;
                }
                fallthrough__;
            case CON_ADDVARVAR:
            case CON_ANDVARVAR:
            case CON_DISPLAYRANDVARVAR:
            case CON_DIVVARVAR:
            case CON_MODVARVAR:
            case CON_MULVARVAR:
            case CON_ORVARVAR:
            case CON_SETVARVAR:
            case CON_SHIFTVARVARL:
            case CON_SHIFTVARVARR:
            case CON_SUBVARVAR:
            case CON_XORVARVAR:
            {
setvarvar:
                auto ins = &g_scriptPtr[-1];
                auto tptr = textptr;
                int const lnum = g_lineNumber;

                C_GetNextVarType(GAMEVAR_READONLY);
                C_GetNextVar();

                int const opcode = inthash_find(&h_varvar, *ins & VM_INSTMASK);

                if (ins[2] == GV_FLAG_CONSTANT && opcode != -1)
                {
                    if (g_scriptDebug > 1 && !g_errorCnt && !g_warningCnt)
                    {
                        VLOG_F(LOG_CON, "%s:%d: %s -> %s", g_scriptFileName, g_lineNumber,
                            VM_GetKeywordForID(*ins & VM_INSTMASK), VM_GetKeywordForID(opcode));
                    }

                    tw = opcode;
                    scriptWriteAtOffset(opcode | LINE_NUMBER, ins);
                    g_scriptPtr = &ins[1];
                    textptr = tptr;
                    g_lineNumber = lnum;
                    goto setvar;
                }

                continue;
            }

            case CON_SIN:
            case CON_COS:
                C_GetNextVarType(GAMEVAR_READONLY);
                C_GetNextVar();
                continue;

            case CON_SMAXAMMO:
            case CON_ADDWEAPONVAR:
            case CON_ACTIVATEBYSECTOR:
            case CON_OPERATESECTORS:
            case CON_OPERATEACTIVATORS:
            case CON_SSP:
                if (!C_CheckEventSync(g_currentEvent))
                {
                    C_ReportError(WARNING_EVENTSYNC);
                    g_warningCnt++;
                }
                fallthrough__;
            case CON_GMAXAMMO:
            case CON_DIST:
            case CON_LDIST:
            case CON_GETINCANGLE:
            case CON_GETANGLE:
            case CON_MULSCALE:
            case CON_SETASPECT:
                // get the ID of the DEF
                switch (tw)
                {
                    case CON_DIST:
                    case CON_LDIST:
                    case CON_GETANGLE:
                    case CON_GETINCANGLE:
                        C_GetNextVarType(GAMEVAR_READONLY);
                        break;
                    default:
                        C_GetNextVar();
                        break;
                }

                // get the ID of the DEF
                if (tw == CON_GMAXAMMO)
                    C_GetNextVarType(GAMEVAR_READONLY);
                else C_GetNextVar();

                switch (tw)
                {
                    case CON_DIST:
                    case CON_LDIST:
                    case CON_GETANGLE:
                    case CON_GETINCANGLE:
                        C_GetNextVar();
                        break;
                    case CON_MULSCALE:
                        C_GetManyVars(2);
                        break;
                }
                continue;

            case CON_DRAGPOINT:
            case CON_GETKEYNAME:
                C_GetManyVars(3);
                continue;

            case CON_GETFLORZOFSLOPE:
            case CON_GETCEILZOFSLOPE:
                C_GetManyVars(3);
                C_GetNextVarType(GAMEVAR_READONLY);
                continue;

            case CON_INTERPOLATE:
                C_GetManyVars(2);
                C_GetNextVarType(GAMEVAR_READONLY);
                continue;

            case CON_LDISTVAR:
                C_GetManyVars(4);
                C_GetNextVarType(GAMEVAR_READONLY);
                continue;

            case CON_DISTVAR:
                C_GetManyVars(6);
                C_GetNextVarType(GAMEVAR_READONLY);
                continue;

            case CON_DEFINEPROJECTILE:
            {
                int32_t y, z;

                if (EDUKE32_PREDICT_FALSE(g_processingState || g_scriptActorOffset))
                {
                    C_ReportError(ERROR_FOUNDWITHIN);
                    g_errorCnt++;
                }

                g_scriptPtr--;

                C_GetNextValue(LABEL_DEFINE);
                j = g_scriptPtr[-1];
                g_scriptPtr--;

                C_GetNextValue(LABEL_DEFINE);
                y = g_scriptPtr[-1];
                g_scriptPtr--;

                C_GetNextValue(LABEL_DEFINE);
                z = g_scriptPtr[-1];
                g_scriptPtr--;

                if (EDUKE32_PREDICT_FALSE((unsigned)j >= MAXTILES))
                {
                    C_ReportError(ERROR_EXCEEDSMAXTILES);
                    g_errorCnt++;
                    continue;
                }

                C_DefineProjectile(j, y, z);
                continue;
            }

            case CON_DAMAGEEVENTTILE:
            {
                if (EDUKE32_PREDICT_FALSE(g_processingState || g_scriptActorOffset))
                {
                    C_ReportError(ERROR_FOUNDWITHIN);
                    g_errorCnt++;
                }

                g_scriptPtr--;

                C_GetNextValue(LABEL_DEFINE);
                j = g_scriptPtr[-1];
                g_scriptPtr--;

                if (EDUKE32_PREDICT_FALSE((unsigned)j >= MAXTILES))
                {
                    C_ReportError(ERROR_EXCEEDSMAXTILES);
                    g_errorCnt++;
                    continue;
                }

                g_tile[j].flags |= SFLAG_DAMAGEEVENT;

                continue;
            }

            case CON_DAMAGEEVENTTILERANGE:
            {
                if (EDUKE32_PREDICT_FALSE(g_processingState || g_scriptActorOffset))
                {
                    C_ReportError(ERROR_FOUNDWITHIN);
                    g_errorCnt++;
                }

                g_scriptPtr--;

                C_GetNextValue(LABEL_DEFINE);
                i = g_scriptPtr[-1];
                g_scriptPtr--;

                C_GetNextValue(LABEL_DEFINE);
                j = g_scriptPtr[-1];
                g_scriptPtr--;

                if (EDUKE32_PREDICT_FALSE((unsigned)i >= MAXTILES || (unsigned)j >= MAXTILES))
                {
                    C_ReportError(ERROR_EXCEEDSMAXTILES);
                    g_errorCnt++;
                    continue;
                }

                for (tiledata_t* t = g_tile + i, *t_end = g_tile + j; t <= t_end; ++t)
                    t->flags |= SFLAG_DAMAGEEVENT;

                continue;
            }

            case CON_SPRITEFLAGS:
            {
                if (!g_scriptActorOffset && g_processingState == 0)
                {
                    g_scriptPtr--;
                    auto tmpscrptr = g_scriptPtr;

                    C_GetNextValue(LABEL_DEFINE);
                    j = g_scriptPtr[-1];

                    int32_t flags = 0;
                    do
                        C_BitOrNextValue(&flags);
                    while (C_GetKeyword() == -1);

                    g_scriptPtr = tmpscrptr;

                    if (EDUKE32_PREDICT_FALSE((unsigned)j >= MAXTILES))
                    {
                        C_ReportError(ERROR_EXCEEDSMAXTILES);
                        g_errorCnt++;
                        continue;
                    }

                    g_tile[j].flags = flags;
                }
                else C_GetNextVar();
                continue;
            }

            case CON_PRECACHE:
            case CON_SPRITENOPAL:
            case CON_SPRITENOSHADE:
            case CON_SPRITENVG:
            case CON_SPRITESHADOW:
                if (EDUKE32_PREDICT_FALSE(g_processingState || g_scriptActorOffset))
                {
                    C_ReportError(ERROR_FOUNDWITHIN);
                    g_errorCnt++;
                    scriptSkipLine();
                    continue;
                }

                g_scriptPtr--;

                C_GetNextValue(LABEL_DEFINE);
                g_scriptPtr--;
                j = *g_scriptPtr;

                if (EDUKE32_PREDICT_FALSE((unsigned)j >= MAXTILES))
                {
                    C_ReportError(ERROR_EXCEEDSMAXTILES);
                    g_errorCnt++;
                    scriptSkipLine();
                    continue;
                }

                switch (tw)
                {
                    case CON_SPRITESHADOW:
                        g_tile[*g_scriptPtr].flags |= SFLAG_SHADOW;
                        break;
                    case CON_SPRITENVG:
                        g_tile[*g_scriptPtr].flags |= SFLAG_NVG;
                        break;
                    case CON_SPRITENOSHADE:
                        g_tile[*g_scriptPtr].flags |= SFLAG_NOSHADE;
                        break;
                    case CON_SPRITENOPAL:
                        g_tile[*g_scriptPtr].flags |= SFLAG_NOPAL;
                        break;
                    case CON_PRECACHE:
                        C_GetNextValue(LABEL_DEFINE);
                        g_scriptPtr--;
                        i = *g_scriptPtr;
                        if (EDUKE32_PREDICT_FALSE((unsigned)i >= MAXTILES))
                        {
                            C_ReportError(ERROR_EXCEEDSMAXTILES);
                            g_errorCnt++;
                            scriptSkipLine();
                            continue;
                        }
                        g_tile[j].cacherange = i;

                        C_GetNextValue(LABEL_DEFINE);
                        g_scriptPtr--;
                        if (*g_scriptPtr)
                            g_tile[j].flags |= SFLAG_CACHE;

                        break;
                }
                continue;

            case CON_IFACTORSOUND:
            case CON_IFVARVARG:
            case CON_IFVARVARGE:
            case CON_IFVARVARL:
            case CON_IFVARVARLE:
            case CON_IFVARVARE:
            case CON_IFVARVARN:
            case CON_IFVARVARAND:
            case CON_IFVARVAROR:
            case CON_IFVARVARXOR:
            case CON_IFVARVAREITHER:
            case CON_WHILEVARVARN:
            {
                auto const ins = &g_scriptPtr[-1];
                auto const lastScriptPtr = &g_scriptPtr[-1] - apScript;
                auto const lasttextptr = textptr;
                int const lnum = g_lineNumber;

                g_skipBranch = false;

                C_GetNextVar();
                auto const var = g_scriptPtr;
                C_GetNextVar();

                if (*var == GV_FLAG_CONSTANT)
                {
                    int const opcode = inthash_find(&h_varvar, tw);

                    if (opcode != -1)
                    {
                        if (g_scriptDebug > 1 && !g_errorCnt && !g_warningCnt)
                        {
                            VLOG_F(LOG_CON, "%s:%d: replacing %s with %s", g_scriptFileName, g_lineNumber,
                                VM_GetKeywordForID(*ins & VM_INSTMASK), VM_GetKeywordForID(opcode));
                        }

                        scriptWriteAtOffset(opcode | LINE_NUMBER, ins);
                        tw = opcode;
                        g_scriptPtr = &ins[1];
                        textptr = lasttextptr;
                        g_lineNumber = lnum;
                        goto ifvar;
                    }
                }

                if (C_CheckMalformedBranch(lastScriptPtr))
                    continue;

                auto const offset = g_scriptPtr - apScript;
                g_scriptPtr++; // Leave a spot for the fail location

                C_ParseCommand();

                if (C_CheckEmptyBranch(tw, lastScriptPtr))
                    continue;

                auto const tempscrptr = apScript + offset;
                scriptWritePointer((intptr_t)g_scriptPtr, tempscrptr);

                if (tw != CON_WHILEVARVARN /*&& tw != CON_WHILEVARVARL*/)
                {
                    j = C_GetKeyword();

                    if (j == CON_ELSE)
                        g_checkingIfElse++;
                }
                continue;
            }

            case CON_IFVARL:
            case CON_IFVARLE:
            case CON_IFVARG:
            case CON_IFVARGE:
            case CON_IFVARE:
            case CON_IFVARN:
            case CON_IFVARAND:
            case CON_IFVAROR:
            case CON_IFVARXOR:
            case CON_IFVAREITHER:
            case CON_WHILEVARN:
            {
ifvar:
                auto const ins = &g_scriptPtr[-1];
                auto const lastScriptPtr = &g_scriptPtr[-1] - apScript;

                g_skipBranch = false;

                C_GetNextVar();
                C_GetNextValue(LABEL_DEFINE);

                if (C_CheckMalformedBranch(lastScriptPtr))
                    continue;

                scriptUpdateOpcodeForVariableType(ins);

                auto const offset = g_scriptPtr - apScript;
                g_scriptPtr++; //Leave a spot for the fail location

                C_ParseCommand();

                if (C_CheckEmptyBranch(tw, lastScriptPtr))
                    continue;

                auto const tempscrptr = apScript + offset;
                scriptWritePointer((intptr_t)g_scriptPtr, tempscrptr);

                if (tw != CON_WHILEVARN /*&& tw != CON_WHILEVARL*/)
                {
                    j = C_GetKeyword();

                    if (j == CON_ELSE)
                        g_checkingIfElse++;
                }

                continue;
            }

            case CON_FOR:  // special-purpose iteration
            {
                C_GetNextVarType(GAMEVAR_READONLY);
                C_GetNextLabelName();

                int const iterType = hash_find(&h_iter, LAST_LABEL);

                if (EDUKE32_PREDICT_FALSE(iterType < 0))
                {
                    C_CUSTOMERROR("unknown iteration type '%s'.", LAST_LABEL);
                    return 1;
                }

                scriptWriteValue(iterType);

                if (iterType >= ITER_SPRITESOFSECTOR)
                    C_GetNextVar();

                intptr_t const offset = g_scriptPtr - apScript;
                g_scriptPtr++; //Leave a spot for the location to jump to after completion

                C_ParseCommand();

                // write relative offset
                auto const tscrptr = (intptr_t*)apScript + offset;
                scriptWriteAtOffset((g_scriptPtr - apScript) - offset, tscrptr);
                continue;
            }

            case CON_ADDLOGVAR:

                // syntax: addlogvar <var>

                // prints the line number in the log file.
                /*        *g_scriptPtr=g_lineNumber;
                        g_scriptPtr++; */

                        // get the ID of the DEF
                C_GetNextVar();
                continue;

            case CON_ROTATESPRITE16:
            case CON_ROTATESPRITE:
                if (g_scriptEventOffset == 0 && g_processingState == 0)
                {
                    C_ReportError(ERROR_EVENTONLY);
                    g_errorCnt++;
                }

                // syntax:
                // int x, int y, int z, short a, short tilenum, signed char shade, char orientation, x1, y1, x2, y2
                // myospal adds char pal

                // get the ID of the DEFs

                C_GetManyVars(12);
                continue;

            case CON_ROTATESPRITEA:
                if (g_scriptEventOffset == 0 && g_processingState == 0)
                {
                    C_ReportError(ERROR_EVENTONLY);
                    g_errorCnt++;
                }

                C_GetManyVars(13);
                continue;

            case CON_SHOWVIEW:
                if (g_scriptEventOffset == 0 && g_processingState == 0)
                {
                    C_ReportError(ERROR_EVENTONLY);
                    g_errorCnt++;
                }

                C_GetManyVars(10);
                continue;

            case CON_GETZRANGE:
                C_GetManyVars(4);
                C_GetManyVarsType(GAMEVAR_READONLY, 4);
                C_GetManyVars(2);
                continue;

            case CON_HITSCAN:
            case CON_CANSEE:
                // get the ID of the DEF
                C_GetManyVars(tw == CON_CANSEE ? 8 : 7);
                C_GetManyVarsType(GAMEVAR_READONLY, tw == CON_CANSEE ? 1 : 6);
                if (tw == CON_HITSCAN) C_GetNextVar();
                continue;

            case CON_CANSEESPR:
                C_GetManyVars(2);
                C_GetNextVarType(GAMEVAR_READONLY);
                continue;

            case CON_ROTATEPOINT:
            case CON_NEARTAG:
                C_GetManyVars(5);
                C_GetManyVarsType(GAMEVAR_READONLY, 2);
                if (tw == CON_NEARTAG)
                {
                    C_GetManyVarsType(GAMEVAR_READONLY, 2);
                    C_GetManyVars(2);
                }
                continue;

            case CON_GETTIMEDATE:
                C_GetManyVarsType(GAMEVAR_READONLY, 8);
                continue;

            case CON_MOVESPRITE:
            case CON_SETSPRITE:
                if (!C_CheckEventSync(g_currentEvent))
                {
                    C_ReportError(WARNING_EVENTSYNC);
                    g_warningCnt++;
                }
                C_GetManyVars(4);
                if (tw == CON_MOVESPRITE)
                {
                    C_GetNextVar();
                    C_GetNextVarType(GAMEVAR_READONLY);
                }
                continue;

            case CON_MINITEXT:
            case CON_GAMETEXT:
            case CON_GAMETEXTZ:
            case CON_DIGITALNUMBER:
            case CON_DIGITALNUMBERZ:
            case CON_SCREENTEXT:
                if (g_scriptEventOffset == 0 && g_processingState == 0)
                {
                    C_ReportError(ERROR_EVENTONLY);
                    g_errorCnt++;
                }

                switch (tw)
                {
                    case CON_SCREENTEXT:
                        C_GetManyVars(8);
                        fallthrough__;
                    case CON_GAMETEXTZ:
                    case CON_DIGITALNUMBERZ:
                        C_GetManyVars(1);
                        fallthrough__;
                    case CON_GAMETEXT:
                    case CON_DIGITALNUMBER:
                        C_GetManyVars(6);
                        fallthrough__;
                    default:
                        C_GetManyVars(5);
                        break;
                }
                continue;

            case CON_UPDATESECTOR:
            case CON_UPDATESECTORZ:
                C_GetManyVars(2);
                if (tw == CON_UPDATESECTORZ)
                    C_GetNextVar();
                C_GetNextVarType(GAMEVAR_READONLY);
                continue;

            case CON_MYOS:
            case CON_MYOSPAL:
            case CON_MYOSX:
            case CON_MYOSPALX:
                if (g_scriptEventOffset == 0 && g_processingState == 0)
                {
                    C_ReportError(ERROR_EVENTONLY);
                    g_errorCnt++;
                }

                // syntax:
                // int x, int y, short tilenum, signed char shade, char orientation
                // myospal adds char pal

                C_GetManyVars(5);
                if (tw == CON_MYOSPAL || tw == CON_MYOSPALX)
                {
                    // Parse: pal

                    // get the ID of the DEF
                    C_GetNextVar();
                }
                continue;

            case CON_FINDPLAYER:
            case CON_FINDOTHERPLAYER:
            case CON_DISPLAYRAND:
            case CON_KLABS:
                // syntax: displayrand <var>
                // gets rand (not game rand) into <var>

                // Get The ID of the DEF
                C_GetNextVarType(GAMEVAR_READONLY);
                continue;

            case CON_SWITCH:
            {
                g_checkingSwitch++; // allow nesting (if other things work)
                C_GetNextVar();

                intptr_t const tempoffset = (unsigned)(g_scriptPtr - apScript);

                scriptWriteValue(0); // leave spot for end location (for after processing)
                scriptWriteValue(0); // count of case statements

                auto const backupCaseScriptPtr = g_caseTablePtr;
                g_caseTablePtr = g_scriptPtr;        // the first case's pointer.

                int const backupNumCases = g_numCases;

                scriptWriteValue(0); // leave spot for 'default' location (null if none)

        //                temptextptr=textptr;
                        // probably does not allow nesting...

                j = C_CountCaseStatements();
                //        initprintf("Done Counting Case Statements for switch %d: found %d.\n", g_checkingSwitch,j);
                g_scriptPtr += j * 2;
                C_SkipComments();
                g_scriptPtr -= j * 2; // allocate buffer for the table
                auto tempscrptr = (intptr_t*)(apScript + tempoffset);

                //AddLog(g_szBuf);

                if (j < 0)
                {
                    return 1;
                }

                if (tempscrptr)
                {
                    // save count of cases
                    scriptWriteAtOffset(j, &tempscrptr[1]);
                }
                else
                {
                    //Bsprintf(g_szBuf,"ERROR::%s %d",__FILE__,__LINE__);
                    //AddLog(g_szBuf);
                }

                while (j--)
                {
                    // leave room for statements

                    scriptWriteValue(0); // value check
                    scriptWriteValue(0); // code offset
                    C_SkipComments();
                }

                g_numCases = 0;
                C_ParseCommand(true);
                tempscrptr = (intptr_t*)(apScript + tempoffset);

                //Bsprintf(g_szBuf,"SWITCHXX: '%.22s'",textptr);
                //AddLog(g_szBuf);
                // done processing switch.  clean up.
                if (g_checkingSwitch < 1)
                {
                    //    Bsprintf(g_szBuf,"ERROR::%s %d: g_checkingSwitch=%d",__FILE__,__LINE__, g_checkingSwitch);
                    //    AddLog(g_szBuf);
                }
                g_numCases = 0;

                if (tempscrptr)
                {
                    for (i = 3; i < 3 + tempscrptr[1] * 2 - 2; i += 2)  // sort them
                    {
                        intptr_t t = tempscrptr[i];
                        int n = i;

                        for (j = i + 2; j < 3 + tempscrptr[1] * 2; j += 2)
                        {
                            if (tempscrptr[j] < t)
                            {
                                t = tempscrptr[j];
                                n = j;
                            }
                        }

                        if (n != i)
                        {
                            swapptr(&tempscrptr[i], &tempscrptr[n]);
                            swapptr(&tempscrptr[i + 1], &tempscrptr[n + 1]);
                        }
                    }
                    //            for (j=3;j<3+tempscrptr[1]*2;j+=2)initprintf("%5d %8x\n",tempscrptr[j],tempscrptr[j+1]);

                    // save 'end' location
                    scriptWriteAtOffset((intptr_t)g_scriptPtr - (intptr_t)apScript, tempscrptr);
                }
                else
                {
                    //Bsprintf(g_szBuf,"ERROR::%s %d",__FILE__,__LINE__);
                    //AddLog(g_szBuf);
                }
                g_caseTablePtr = backupCaseScriptPtr;
                g_numCases = backupNumCases;
                //AddLog("End of Switch statement");
            }
            continue;

            case CON_CASE:
            case CON_DEFAULT:
            {
                if (EDUKE32_PREDICT_FALSE(g_checkingSwitch < 1))
                {
                    g_errorCnt++;
                    C_ReportError(-1);
                    LOG_F(ERROR, "%s:%d: found '%s' statement when not in switch", g_scriptFileName,
                        g_lineNumber, tw == CON_CASE ? "case" : "default");
                    g_scriptPtr--;
                    return 1;
                }

                intptr_t tempoffset = 0;
                intptr_t* tempscrptr = g_scriptPtr;

                g_checkingCase = true;
repeatcase:
                g_scriptPtr--;

                C_SkipComments();

                if (tw == CON_CASE)
                {
                    g_numCases++;
                    C_GetNextValue(LABEL_ANY);
                    j = *(--g_scriptPtr);
                }

                C_SkipComments();

                if (*textptr == ':')
                    textptr++;

                C_SkipComments();

                if (g_caseTablePtr)
                {
                    if (tw == CON_DEFAULT)
                    {
                        if (EDUKE32_PREDICT_FALSE(g_caseTablePtr[0] != 0))
                        {
                            // duplicate default statement
                            g_errorCnt++;
                            C_ReportError(-1);
                            LOG_F(ERROR, "%s:%d: multiple 'default' statements found in switch", g_scriptFileName, g_lineNumber);
                        }
                        g_caseTablePtr[0] = (intptr_t)(g_scriptPtr - apScript);   // save offset
                    }
                    else
                    {
                        for (i = (g_numCases / 2) - 1; i >= 0; i--)
                            if (EDUKE32_PREDICT_FALSE(g_caseTablePtr[i * 2 + 1] == j))
                            {
                                g_warningCnt++;
                                C_ReportError(WARNING_DUPLICATECASE);
                                break;
                            }
                        g_caseTablePtr[g_numCases++] = j;
                        g_caseTablePtr[g_numCases] = (intptr_t)((intptr_t*)g_scriptPtr - apScript);
                    }
                }

                j = C_GetKeyword();

                if (j == CON_CASE || j == CON_DEFAULT)
                {
                    //AddLog("Found Repeat Case");
                    C_GetNextKeyword();    // eat keyword
                    tw = j;
                    goto repeatcase;
                }

                tempoffset = (unsigned)(tempscrptr - apScript);

                while (C_ParseCommand() == 0)
                {
                    j = C_GetKeyword();

                    if (j == CON_CASE || j == CON_DEFAULT)
                    {
                        C_GetNextKeyword();    // eat keyword
                        tempscrptr = (intptr_t*)(apScript + tempoffset);
                        tw = j;
                        goto repeatcase;
                    }
                }

                continue;
            }

            case CON_ENDSWITCH:
                //AddLog("End Switch");
                if (g_caseTablePtr)
                {
                    if (EDUKE32_PREDICT_FALSE(g_checkingCase))
                    {
                        g_errorCnt++;
                        C_ReportError(-1);
                        LOG_F(ERROR, "%s:%d: found 'endswitch' before 'break' or 'return'", g_scriptFileName, g_lineNumber);
                    }
                }

                if (EDUKE32_PREDICT_FALSE(--g_checkingSwitch < 0))
                {
                    g_errorCnt++;
                    C_ReportError(-1);
                    LOG_F(ERROR, "%s:%d: found 'endswitch' without matching 'switch'", g_scriptFileName, g_lineNumber);
                }
                return 1;      // end of block

            case CON_ZSHOOT:
            case CON_EZSHOOT:
            case CON_CHANGESPRITESTAT:
            case CON_CHANGESPRITESECT:
                if (!C_CheckEventSync(g_currentEvent))
                {
                    g_warningCnt++;
                    C_ReportError(WARNING_EVENTSYNC);
                }
                fallthrough__;
            case CON_GETPNAME:
            case CON_GETTEAMNAME:
            case CON_STARTLEVEL:
            case CON_QSTRCAT:
            case CON_QSTRCPY:
            case CON_QSTRLEN:
            case CON_QGETSYSSTR:
            case CON_HEADSPRITESTAT:
            case CON_PREVSPRITESTAT:
            case CON_NEXTSPRITESTAT:
            case CON_HEADSPRITESECT:
            case CON_PREVSPRITESECT:
            case CON_NEXTSPRITESECT:
            case CON_ACTORSOUND:
            case CON_STOPACTORSOUND:
                C_GetManyVars(2);
                continue;
            case CON_QSTRDIM:
                C_GetNextVarType(GAMEVAR_READONLY);
                C_GetNextVarType(GAMEVAR_READONLY);
                C_GetManyVars(16);
                continue;
            case CON_QSUBSTR:
                C_GetManyVars(4);
                continue;
            case CON_SETACTORANGLE:
            case CON_SETPLAYERANGLE:
                if (!C_CheckEventSync(g_currentEvent))
                {
                    g_warningCnt++;
                    C_ReportError(WARNING_EVENTSYNC);
                }
                fallthrough__;
            case CON_GETANGLETOTARGET:
            case CON_GETACTORANGLE:
            case CON_GETPLAYERANGLE:
                // Syntax:   <command> <var>

                // get the ID of the DEF
                C_GetNextVar();
                continue;

            case CON_ADDLOG:
                // syntax: addlog

                // prints the line number in the log file.
                /*        *g_scriptPtr=g_lineNumber;
                        g_scriptPtr++; */
                continue;

            case CON_IFRND:
            case CON_IFPINVENTORY:
                if (!C_CheckEventSync(g_currentEvent))
                {
                    C_ReportError(WARNING_EVENTSYNC);
                    g_warningCnt++;
                }
                fallthrough__;
            case CON_IFACTION:
            case CON_IFACTIONCOUNT:
            case CON_IFACTOR:
            case CON_IFAI:
            case CON_IFANGDIFFL:
            case CON_IFCEILINGDISTL:
            case CON_IFCOUNT:
                //case CON_IFCUTSCENE:
            case CON_IFFLOORDISTL:
            case CON_IFGAPZL:
            case CON_IFGOTWEAPONCE:
            case CON_IFMOVE:
            case CON_IFP:
            case CON_IFPDISTG:
            case CON_IFPDISTL:
            case CON_IFPHEALTHL:
                //case CON_IFPLAYERSL:
            case CON_IFSOUND:
            case CON_IFSPAWNEDBY:
            case CON_IFSPRITEPAL:
            case CON_IFSTRENGTH:
            case CON_IFWASWEAPON:
            {
                auto const lastScriptPtr = &g_scriptPtr[-1] - apScript;

                g_skipBranch = false;

                switch (tw)
                {
                    //case CON_IFCUTSCENE:
                        //C_GetNextVar();
                        //break;
                    case CON_IFAI:
                        C_GetNextValue(LABEL_AI);
                        break;
                    case CON_IFACTION:
                        C_GetNextValue(LABEL_ACTION);
                        break;
                    case CON_IFMOVE:
                        if (EDUKE32_PREDICT_FALSE((C_GetNextValue(LABEL_MOVE | LABEL_DEFINE) == 0) && (g_scriptPtr[-1] != 0) && (g_scriptPtr[-1] != 1)))
                        {
                            C_ReportError(-1);
                            g_scriptPtr[-1] = 0;
                            LOG_F(WARNING, "%s:%d: expected a move, found a constant.", g_scriptFileName, g_lineNumber);
                            g_warningCnt++;
                        }
                        break;
                    case CON_IFPINVENTORY:
                        C_GetNextValue(LABEL_DEFINE);
                        C_GetNextValue(LABEL_DEFINE);
                        break;
                    case CON_IFP:
                        j = 0;
                        do
                            C_BitOrNextValue(&j);
                        while (C_GetKeyword() == -1);
                        C_FinishBitOr(j);
                        break;
                    case CON_IFSOUND:
                    default:
                        C_GetNextValue(LABEL_DEFINE);
                        break;
                }

                if (C_CheckMalformedBranch(lastScriptPtr))
                    continue;

                intptr_t const offset = (unsigned)(g_scriptPtr - apScript);

                g_scriptPtr++; //Leave a spot for the fail location

                C_ParseCommand();

                if (C_CheckEmptyBranch(tw, lastScriptPtr))
                    continue;

                auto const tempscrptr = (intptr_t*)apScript + offset;
                scriptWritePointer((intptr_t)g_scriptPtr, tempscrptr);

                j = C_GetKeyword();

                if (j == CON_ELSE)
                    g_checkingIfElse++;

                continue;
            }

            case CON_IFACTORNOTSTAYPUT:
            case CON_IFAWAYFROMWALL:
            case CON_IFBULLETNEAR:
            case CON_IFCANSEE:
            case CON_IFCANSEETARGET:
            case CON_IFCANSHOOTTARGET:
                //case CON_IFCLIENT:
            case CON_IFDEAD:
            case CON_IFHITSPACE:
            case CON_IFHITWEAPON:
            case CON_IFINOUTERSPACE:
            case CON_IFINSPACE:
            case CON_IFINWATER:
            case CON_IFMULTIPLAYER:
            case CON_IFNOSOUNDS:
            case CON_IFNOTMOVING:
            case CON_IFONWATER:
            case CON_IFOUTSIDE:
                //case CON_IFPLAYBACKON:
            case CON_IFRESPAWN:
                //case CON_IFSERVER:
            case CON_IFSQUISHED:
            {
                auto const lastScriptPtr = &g_scriptPtr[-1] - apScript;

                g_skipBranch = false;

                if (C_CheckMalformedBranch(lastScriptPtr))
                    continue;

                intptr_t const offset = (unsigned)(g_scriptPtr - apScript);

                g_scriptPtr++; //Leave a spot for the fail location

                C_ParseCommand();

                if (C_CheckEmptyBranch(tw, lastScriptPtr))
                    continue;

                auto const tempscrptr = (intptr_t*)apScript + offset;
                scriptWritePointer((intptr_t)g_scriptPtr, tempscrptr);

                j = C_GetKeyword();

                if (j == CON_ELSE)
                    g_checkingIfElse++;

                continue;
            }

            case CON_LEFTBRACE:
                if (EDUKE32_PREDICT_FALSE(!(g_processingState || g_scriptActorOffset || g_scriptEventOffset)))
                {
                    g_errorCnt++;
                    C_ReportError(ERROR_SYNTAXERROR);
                }
                g_numBraces++;

                C_ParseCommand(true);
                continue;

            case CON_RIGHTBRACE:
                g_numBraces--;

                if ((g_scriptPtr[-2] & VM_IFELSE_MAGIC_BIT) &&
                    ((g_scriptPtr[-2] & VM_INSTMASK) == CON_LEFTBRACE)) // rewrite "{ }" into "nullop"
                {
                    //            initprintf("%s:%d: rewriting empty braces '{ }' as 'nullop' from right\n",g_szScriptFileName,g_lineNumber);
                    g_scriptPtr[-2] = CON_NULLOP | LINE_NUMBER | VM_IFELSE_MAGIC_BIT;
                    g_scriptPtr -= 2;

                    if (C_GetKeyword() != CON_ELSE && (g_scriptPtr[-2] & VM_INSTMASK) != CON_ELSE)
                        g_skipBranch = true;
                    else g_skipBranch = false;

                    j = C_GetKeyword();

                    if (g_checkingIfElse && j != CON_ELSE)
                        g_checkingIfElse--;

                    return 1;
                }

                if (EDUKE32_PREDICT_FALSE(g_numBraces < 0))
                {
                    if (g_checkingSwitch)
                    {
                        C_ReportError(ERROR_NOENDSWITCH);
                    }

                    C_ReportError(-1);
                    LOG_F(ERROR, "%s:%d: found more '}' than '{'.", g_scriptFileName, g_lineNumber);
                    g_errorCnt++;
                }

                if (g_checkingIfElse && j != CON_ELSE)
                    g_checkingIfElse--;

                return 1;

            case CON_BETANAME:
                g_scriptPtr--;
                j = 0;
                scriptSkipLine();
                continue;

            case CON_DEFINEVOLUMENAME:
                g_scriptPtr--;

                C_GetNextValue(LABEL_DEFINE);
                g_scriptPtr--;
                j = *g_scriptPtr;

                scriptSkipSpaces();

                if (EDUKE32_PREDICT_FALSE((unsigned)j > MAXVOLUMES - 1))
                {
                    LOG_F(ERROR, "%s:%d: volume number exceeds maximum volume count.",
                        g_scriptFileName, g_lineNumber);
                    g_errorCnt++;
                    scriptSkipLine();
                    continue;
                }

                i = 0;

                while (*textptr != 0x0a && *textptr != 0x0d && *textptr != 0)
                {
                    EpisodeNames[j][i] = *textptr;
                    textptr++, i++;
                    if (EDUKE32_PREDICT_FALSE(i >= (signed)sizeof(EpisodeNames[j])))
                    {
                        LOG_F(WARNING, "%s:%d: truncating volume name to %d characters.",
                            g_scriptFileName, g_lineNumber, (int32_t)sizeof(EpisodeNames[j]) - 1);
                        i--;
                        g_warningCnt++;
                        scriptSkipLine();
                        break;
                    }
                }
                g_numVolumes = j + 1;
                EpisodeNames[j][i] = '\0';
                continue;

            case CON_DEFINEGAMEFUNCNAME:
                g_scriptPtr--;
                C_GetNextValue(LABEL_DEFINE);
                g_scriptPtr--;
                j = *g_scriptPtr;

                scriptSkipSpaces();

                if (EDUKE32_PREDICT_FALSE((unsigned)j > NUMGAMEFUNCTIONS - 1))
                {
                    LOG_F(ERROR, "%s:%d: function number exceeds number of game functions.",
                        g_scriptFileName, g_lineNumber);
                    g_errorCnt++;
                    scriptSkipLine();
                    continue;
                }

                i = 0;

                hash_delete(&h_gamefuncs, gamefunctions[j]);

                while (*textptr != 0x0a && *textptr != 0x0d && *textptr != 0)
                {
                    gamefunctions[j][i] = *textptr;
                    textptr++, i++;
                    if (EDUKE32_PREDICT_FALSE(i >= MAXGAMEFUNCLEN))
                    {
                        LOG_F(WARNING, "%s:%d: truncating function name to %d characters.",
                            g_scriptFileName, g_lineNumber, MAXGAMEFUNCLEN - 1);
                        i--;
                        g_warningCnt++;
                        scriptSkipLine();
                        break;
                    }
                    if (EDUKE32_PREDICT_FALSE(*textptr != 0x0a && *textptr != 0x0d && ispecial(*textptr)))
                    {
                        LOG_F(WARNING, "%s:%d: invalid character in function name.",
                            g_scriptFileName, g_lineNumber);
                        g_warningCnt++;
                        scriptSkipLine();
                        break;
                    }
                }
                gamefunctions[j][i] = '\0';
                hash_add(&h_gamefuncs, gamefunctions[j], j, 0);

                continue;

            case CON_DEFINESKILLNAME:
                g_scriptPtr--;

                C_GetNextValue(LABEL_DEFINE);
                g_scriptPtr--;
                j = *g_scriptPtr;

                scriptSkipSpaces();

                if (j < 0 || j > 4)
                {
                    LOG_F(ERROR, "%s:%d: skill number exceeds maximum skill count.", g_scriptFileName, g_lineNumber);
                    g_errorCnt++;
                    scriptSkipLine();
                    continue;
                }

                i = 0;

                while (*textptr != 0x0a && *textptr != 0x0d && *textptr != 0)
                {
                    g_skillNames[j][i] = toupper(*textptr);
                    textptr++, i++;
                    if (EDUKE32_PREDICT_FALSE(i >= (signed)sizeof(g_skillNames[j])))
                    {
                        LOG_F(WARNING, "%s:%d: truncating skill name to %d characters.",
                              g_scriptFileName, g_lineNumber, (int32_t)sizeof(g_skillNames[j]) - 1);
                        i--;
                        g_warningCnt++;
                        scriptSkipLine();
                        break;
                    }
                }

                if (EDUKE32_PREDICT_FALSE(i == 0))
                {
                    LOG_F(WARNING, "%s:%d: empty skill name.",
                          g_scriptFileName, g_lineNumber);
                    g_skillNames[j][i++] = ' ';
                }

                g_skillNames[j][i] = '\0';
                continue;

            case CON_SETGAMENAME:
            {
                char gamename[32];
                g_scriptPtr--;

                C_SkipComments();

                i = 0;

                while (*textptr != 0x0a && *textptr != 0x0d && *textptr != 0)
                {
                    gamename[i] = *textptr;
                    textptr++, i++;
                    if (EDUKE32_PREDICT_FALSE(i >= (signed)sizeof(gamename)))
                    {
                        LOG_F(WARNING, "%s:%d: truncating game name to %d characters.",
                              g_scriptFileName, g_lineNumber, (int32_t)sizeof(gamename) - 1);
                        i--;
                        g_warningCnt++;
                        scriptSkipLine();
                        break;
                    }
                }
                gamename[i] = '\0';
                g_gameNamePtr = Xstrdup(gamename);
                G_UpdateAppTitle();
            }
            continue;

            case CON_SETDEFNAME:
            {
                g_scriptPtr--;
                while (isaltok(*textptr) == 0)
                {
                    if (*textptr == 0x0a) g_lineNumber++;
                    textptr++;
                    if (*textptr == 0) break;
                }
                j = 0;
                while (isaltok(*textptr))
                {
                    tempbuf[j] = *(textptr++);
                    j++;
                }
                tempbuf[j] = '\0';
                C_SetDefName(tempbuf);
            }
            continue;

            case CON_SETCFGNAME:
            {
                g_scriptPtr--;
                while (isaltok(*textptr) == 0)
                {
                    if (*textptr == 0x0a) g_lineNumber++;
                    textptr++;
                    if (*textptr == 0) break;
                }
                j = 0;
                while (isaltok(*textptr))
                {
                    tempbuf[j] = *(textptr++);
                    j++;
                }
                tempbuf[j] = '\0';
                if (Bstrcmp(g_setupFileName, SETUPFILENAME) == 0) // not set to something else via -cfg
                {
                    char temp[BMAX_PATH];
                    struct stat st;
                    if (stat(g_modDir, &st) < 0)
                    {
                        if (errno == ENOENT)     // path doesn't exist
                        {
                            if (Bmkdir(g_modDir, S_IRWXU) < 0)
                            {
                                LOG_F(ERROR, "Failed to create configuration file directory %s", g_modDir);
                                continue;
                            }
                            else LOG_F(INFO, "Created configuration file directory %s", g_modDir);
                        }
                        else
                        {
                            // another type of failure
                            continue;
                        }
                    }
                    else if ((st.st_mode & S_IFDIR) != S_IFDIR)
                    {
                        // directory isn't a directory
                        continue;
                    }
                    Bstrcpy(temp, tempbuf);
                    CONFIG_WriteSetup(0);
                    Bsnprintf(g_setupFileName, sizeof(g_setupFileName), "%s/", g_modDir);
                    Bstrcat(g_setupFileName, temp);
                    CONFIG_ReadSetup();
                }
                LOG_F(INFO, "Using config file '%s'.", g_setupFileName);
                continue;
            }

            case CON_DEFINEGAMETYPE:
                g_scriptPtr--;
                C_GetNextValue(LABEL_DEFINE);
                g_scriptPtr--;
                j = *g_scriptPtr;

                C_GetNextValue(LABEL_DEFINE);
                g_scriptPtr--;
                Gametypes[j].flags = *g_scriptPtr;

                C_SkipComments();

                if (j < 0 || j > MAXGAMETYPES - 1)
                {
                    LOG_F(ERROR, "%s:%d: gametype number exceeds maximum gametype count.", g_scriptFileName, g_lineNumber);
                    g_errorCnt++;
                    scriptSkipLine();
                    continue;
                }
                g_numGametypes = j + 1;

                i = 0;

                while (*textptr != 0x0a && *textptr != 0x0d && *textptr != 0)
                {
                    Gametypes[j].name[i] = toupper(*textptr);
                    textptr++, i++;
                    if (EDUKE32_PREDICT_FALSE(i >= (signed)sizeof(Gametypes[j].name)))
                    {
                        LOG_F(WARNING, "%s:%d: truncating gametype name to %d characters.",
                              g_scriptFileName, g_lineNumber, (int32_t)sizeof(Gametypes[j].name) - 1);
                        i--;
                        g_warningCnt++;
                        scriptSkipLine();
                        break;
                    }
                }
                Gametypes[j].name[i] = '\0';
                continue;

            case CON_DEFINELEVELNAME:
                g_scriptPtr--;
                C_GetNextValue(LABEL_DEFINE);
                g_scriptPtr--;
                j = *g_scriptPtr;
                C_GetNextValue(LABEL_DEFINE);
                g_scriptPtr--;
                k = *g_scriptPtr;
                C_SkipComments();

                if (EDUKE32_PREDICT_FALSE((unsigned)j > MAXVOLUMES - 1))
                {
                    LOG_F(ERROR, "%s:%d: volume number exceeds maximum volume count.", g_scriptFileName, g_lineNumber);
                    g_errorCnt++;
                    scriptSkipLine();
                    continue;
                }
                if (EDUKE32_PREDICT_FALSE((unsigned)k > MAXLEVELS - 1))
                {
                    LOG_F(ERROR, "%s:%d: level number exceeds maximum number of levels per episode.", g_scriptFileName, g_lineNumber);
                    g_errorCnt++;
                    scriptSkipLine();
                    continue;
                }

                i = 0;

                tempbuf[i] = '/';

                while (*textptr != ' ' && *textptr != '\t' && *textptr != 0x0a)
                {
                    tempbuf[i + 1] = *textptr;
                    textptr++, i++;
                    if (EDUKE32_PREDICT_FALSE(i >= BMAX_PATH))
                    {
                        LOG_F(ERROR, "%s:%d: level file name exceeds limit of %d characters.", g_scriptFileName, g_lineNumber, BMAX_PATH);
                        g_errorCnt++;
                        scriptSkipSpaces();
                        break;
                    }
                }
                tempbuf[i + 1] = '\0';

                Bcorrectfilename(tempbuf, 0);

                if (g_mapInfo[j * MAXLEVELS + k].filename == NULL)
                    g_mapInfo[j * MAXLEVELS + k].filename = (char*)Xcalloc(Bstrlen(tempbuf) + 1, sizeof(uint8_t));
                else if ((Bstrlen(tempbuf) + 1) > sizeof(g_mapInfo[j * MAXLEVELS + k].filename))
                    g_mapInfo[j * MAXLEVELS + k].filename = (char*)Xrealloc(g_mapInfo[j * MAXLEVELS + k].filename, (Bstrlen(tempbuf) + 1));

                Bstrcpy(g_mapInfo[j * MAXLEVELS + k].filename, tempbuf);

                C_SkipComments();

                g_mapInfo[j * MAXLEVELS + k].partime =
                    (((*(textptr + 0) - '0') * 10 + (*(textptr + 1) - '0')) * REALGAMETICSPERSEC * 60) +
                    (((*(textptr + 3) - '0') * 10 + (*(textptr + 4) - '0')) * REALGAMETICSPERSEC);

                textptr += 5;
                scriptSkipSpaces();

                // cheap hack, 0.99 doesn't have the 3D Realms time
                if (*(textptr + 2) == ':')
                {
                    g_mapInfo[j * MAXLEVELS + k].designertime =
                        (((*(textptr + 0) - '0') * 10 + (*(textptr + 1) - '0')) * REALGAMETICSPERSEC * 60) +
                        (((*(textptr + 3) - '0') * 10 + (*(textptr + 4) - '0')) * REALGAMETICSPERSEC);

                    textptr += 5;
                    scriptSkipSpaces();
                }
                else if (g_scriptVersion == 10) g_scriptVersion = 9;

                i = 0;

                while (*textptr != 0x0a && *textptr != 0x0d && *textptr != 0)
                {
                    tempbuf[i] = *textptr;
                    textptr++, i++;
                    if (EDUKE32_PREDICT_FALSE(i >= 32))
                    {
                        LOG_F(WARNING, "%s:%d: truncating level name to %d characters.",
                            g_scriptFileName, g_lineNumber, 31);
                        i--;
                        g_warningCnt++;
                        scriptSkipLine();
                        break;
                    }
                }

                tempbuf[i] = '\0';

                if (g_mapInfo[j * MAXLEVELS + k].name == NULL)
                    g_mapInfo[j * MAXLEVELS + k].name = (char*)Xcalloc(Bstrlen(tempbuf) + 1, sizeof(uint8_t));
                else if ((Bstrlen(tempbuf) + 1) > sizeof(g_mapInfo[j * MAXLEVELS + k].name))
                    g_mapInfo[j * MAXLEVELS + k].name = (char*)Xrealloc(g_mapInfo[j * MAXLEVELS + k].name, (Bstrlen(tempbuf) + 1));

                /*         initprintf("level name string len: %d\n",Bstrlen(tempbuf)); */

                Bstrcpy(g_mapInfo[j * MAXLEVELS + k].name, tempbuf);

                continue;

            case CON_DEFINEQUOTE:
            case CON_REDEFINEQUOTE:
                if (tw == CON_DEFINEQUOTE)
                {
                    g_scriptPtr--;
                }

                C_GetNextValue(LABEL_DEFINE);

                k = g_scriptPtr[-1];

                if (EDUKE32_PREDICT_FALSE((unsigned)k >= MAXQUOTES))
                {
                    LOG_F(ERROR, "%s:%d: quote number exceeds limit of %d.", g_scriptFileName, g_lineNumber, MAXQUOTES);
                    g_errorCnt++;
                    scriptSkipLine();
                    continue;
                }
                else
                {
                    C_AllocQuote(k);
                }

                if (tw == CON_DEFINEQUOTE)
                    g_scriptPtr--;

                i = 0;

                scriptSkipSpaces();

                if (tw == CON_REDEFINEQUOTE && !g_switchCountPhase)
                {
                    if (apXStrings[g_numXStrings] == NULL)
                        apXStrings[g_numXStrings] = (char*)Xcalloc(MAXQUOTELEN, sizeof(uint8_t));
                }

                while (*textptr != 0x0a && *textptr != 0x0d && *textptr != 0)
                {
                    /*
                    if (*textptr == '%' && *(textptr+1) == 's')
                    {
                    LOG_F(ERROR, "%s:%d: quote text contains string identifier.",g_szScriptFileName,g_lineNumber);
                    g_numCompilerErrors++;
                    while (*textptr != 0x0a && *textptr != 0x0d && *textptr != 0) textptr++;
                    break;
                    }
                    */
                    if (!g_switchCountPhase)
                    {
                        if (tw == CON_DEFINEQUOTE)
                            *(apStrings[k] + i) = *textptr;
                        else
                            *(apXStrings[g_numXStrings] + i) = *textptr;
                    }

                    textptr++, i++;
                    if (EDUKE32_PREDICT_FALSE(i >= MAXQUOTELEN))
                    {
                        LOG_F(WARNING, "%s:%d: truncating quote text to %d characters.", g_scriptFileName, g_lineNumber, MAXQUOTELEN - 1);
                        i--;
                        g_warningCnt++;
                        scriptSkipLine();
                        break;
                    }
                }

                if (!g_switchCountPhase)
                {
                    if (tw == CON_DEFINEQUOTE)
                    {
                        if ((unsigned)k < MAXQUOTES)
                            *(apStrings[k] + i) = '\0';
                    }
                    else
                    {
                        *(apXStrings[g_numXStrings] + i) = '\0';
                        scriptWriteValue(g_numXStrings++);
                    }
                }
                continue;

            case CON_CHEATKEYS:
                g_scriptPtr--;
                C_GetNextValue(LABEL_DEFINE);
                CheatKeys[0] = *(g_scriptPtr - 1);
                C_GetNextValue(LABEL_DEFINE);
                CheatKeys[1] = *(g_scriptPtr - 1);
                g_scriptPtr -= 2;
                continue;

            case CON_DEFINECHEAT:
                g_scriptPtr--;
                C_GetNextValue(LABEL_DEFINE);
                k = *(g_scriptPtr - 1);

                if (k > 25)
                {
                    LOG_F(ERROR, "%s:%d: cheat redefinition attempts to redefine nonexistant cheat.", g_scriptFileName, g_lineNumber);
                    g_errorCnt++;
                    scriptSkipLine();
                    continue;
                }
                g_scriptPtr--;
                i = 0;
                while (*textptr == ' ' || *textptr == '\t')
                    textptr++;
                while (*textptr != 0x0a && *textptr != 0x0d && *textptr != 0 && *textptr != ' ')
                {
                    CheatStrings[k][i] = *textptr;
                    textptr++, i++;
                    if (EDUKE32_PREDICT_FALSE(i >= (signed)sizeof(CheatStrings[k])))
                    {
                        LOG_F(WARNING, "%s:%d: truncating cheat string to %d characters.",
                              g_scriptFileName, g_lineNumber, (signed)sizeof(CheatStrings[k]) - 1);
                        i--;
                        g_warningCnt++;
                        scriptSkipLine();
                        break;
                    }
                }
                CheatStrings[k][i] = '\0';
                continue;

            case CON_DEFINESOUND:
            {
                g_scriptPtr--;
                C_GetNextValue(LABEL_DEFINE);

                // Ideally we could keep the value of i from C_GetNextValue() instead of having to hash_find() again.
                // This depends on tempbuf remaining in place after C_GetNextValue():
                j = hash_find(&h_labels, tempbuf);

                k = g_scriptPtr[-1];
                if (EDUKE32_PREDICT_FALSE((unsigned)k >= MAXSOUNDS - 1))
                {
                    LOG_F(ERROR, "%s:%d: sound index exceeds limit of %d.", g_scriptFileName, g_lineNumber, MAXSOUNDS - 1);
                    g_errorCnt++;
                    k = MAXSOUNDS - 1;
                }

                S_AllocIndexes(k);

                if (EDUKE32_PREDICT_TRUE(g_sounds[k] == &nullsound))
                    g_sounds[k] = (sound_t*)Xcalloc(1, sizeof(sound_t));
                else
                {
                    LOG_F(WARNING, "%s:%d: overwriting existing definition for sound #%d (%s)", g_scriptFileName, g_lineNumber, k, g_sounds[k]->filename);
                    g_warningCnt++;
                }

                g_scriptPtr--;
                i = 0;
                C_SkipComments();

                char filename[BMAX_PATH];

                if (*textptr == '\"')
                {
                    textptr++;
                    while (*textptr && *textptr != '\"')
                    {
                        filename[i++] = *textptr++;
                        if (EDUKE32_PREDICT_FALSE(i >= BMAX_PATH - 1))
                        {
                            LOG_F(ERROR, "%s:%d: sound filename exceeds limit of %d characters.", g_scriptFileName, g_lineNumber, BMAX_PATH - 1);
                            g_errorCnt++;
                            C_SkipComments();
                            break;
                        }
                    }
                    textptr++;
                }
                else while (*textptr != ' ' && *textptr != '\t' && *textptr != '\r' && *textptr != '\n')
                {
                    filename[i++] = *textptr++;
                    if (EDUKE32_PREDICT_FALSE(i >= BMAX_PATH - 1))
                    {
                        LOG_F(ERROR, "%s:%d: sound filename exceeds limit of %d characters.", g_scriptFileName, g_lineNumber, BMAX_PATH - 1);
                        g_errorCnt++;
                        C_SkipComments();
                        break;
                    }
                }
                filename[i] = '\0';

                check_filename_case(filename);

                C_GetNextValue(LABEL_DEFINE);
                int minpitch = g_scriptPtr[-1];
                C_GetNextValue(LABEL_DEFINE);
                int maxpitch = g_scriptPtr[-1];
                C_GetNextValue(LABEL_DEFINE);
                int priority = g_scriptPtr[-1];

                C_GetNextValue(LABEL_DEFINE);
                int type = g_scriptPtr[-1];

                C_GetNextValue(LABEL_DEFINE);
                int distance = g_scriptPtr[-1];

                g_scriptPtr -= 5;

                float volume = 1.0;

                S_DefineSound(k, filename, minpitch, maxpitch, priority, type, distance, volume);

                if (k > g_highestSoundIdx)
                    g_highestSoundIdx = k;

                if (g_dynamicSoundMapping && j >= 0 && (labeltype[j] & LABEL_DEFINE))
                    G_ProcessDynamicNameMapping(label + (j << 6), g_dynSoundList, k);
                continue;
            }

            case CON_ENDEVENT:

                if (EDUKE32_PREDICT_FALSE(!g_scriptEventOffset))
                {
                    C_ReportError(-1);
                    LOG_F(ERROR, "%s:%d: found 'endevent' without open 'onevent'.", g_scriptFileName, g_lineNumber);
                    g_errorCnt++;
                }
                if (EDUKE32_PREDICT_FALSE(g_numBraces > 0))
                {
                    //C_ReportError(ERROR_NOTTOPLEVEL);
                    g_errorCnt++;
                }
                // if event has already been declared then put a jump in instead
                if (g_scriptEventChainOffset)
                {
                    g_scriptPtr--;
                    scriptWriteValue(CON_JUMP | LINE_NUMBER);
                    scriptWriteValue(GV_FLAG_CONSTANT);
                    scriptWriteValue(g_scriptEventChainOffset);
                    scriptWriteValue(CON_ENDEVENT | LINE_NUMBER);

                    C_FillEventBreakStackWithJump((intptr_t*)g_scriptEventBreakOffset, g_scriptEventChainOffset);

                    g_scriptEventChainOffset = 0;
                }
                else
                {
                    // pad space for the next potential appendevent
                    apScriptGameEventEnd[g_currentEvent] = &g_scriptPtr[-1] - apScript;
                    scriptWriteValue(CON_ENDEVENT | LINE_NUMBER);
                    scriptWriteValue(g_scriptEventBreakOffset);
                    scriptWriteValue(CON_ENDEVENT | LINE_NUMBER);
                }

                g_scriptEventBreakOffset = g_scriptEventOffset = g_scriptActorOffset = 0;
                g_currentEvent = -1;
                Bsprintf(g_szCurrentBlockName, "(none)");
                continue;

            case CON_ENDA:
                if (EDUKE32_PREDICT_FALSE(!g_scriptActorOffset || g_scriptEventOffset))
                {
                    C_ReportError(-1);
                    LOG_F(ERROR, "%s:%d: found 'enda' without open 'actor'.", g_scriptFileName, g_lineNumber);
                    g_errorCnt++;
                    g_scriptEventOffset = 0;
                }
                if (EDUKE32_PREDICT_FALSE(g_numBraces > 0))
                {
                    //C_ReportError(ERROR_NOTTOPLEVEL);
                    g_errorCnt++;
                }
                g_scriptActorOffset = 0;
                Bsprintf(g_szCurrentBlockName, "(none)");
                continue;

            case CON_RETURN:
                if (g_checkingSwitch)
                {
                    g_checkingCase = false;
                    return 1;
                }
                continue;

            case CON_BREAK:
                if (g_checkingSwitch)
                {
                    if (EDUKE32_PREDICT_FALSE(otw == CON_BREAK))
                    {
                        C_ReportError(-1);
                        LOG_F(WARNING, "%s:%d: duplicate 'break'.", g_scriptFileName, g_lineNumber);
                        g_warningCnt++;
                        g_scriptPtr--;
                        continue;
                    }

                    g_checkingCase = false;
                    return 1;
                }
                else if (g_scriptEventOffset)
                {
                    g_scriptPtr--;
                    scriptWriteValue(CON_JUMP | LINE_NUMBER);
                    scriptWriteValue(GV_FLAG_CONSTANT);
                    scriptWriteValue(g_scriptEventBreakOffset);
                    g_scriptEventBreakOffset = &g_scriptPtr[-1] - apScript;
                }
                continue;

            case CON_SCRIPTSIZE:
                g_scriptPtr--;
                C_GetNextValue(LABEL_DEFINE);
                j = g_scriptPtr[-1];
                g_scriptPtr--;
                C_SkipComments();
                C_SetScriptSize(j);
                continue;

            case CON_GAMESTARTUP:
            {
                int params[30];

                g_scriptPtr--;
                for (j = 0; j < 30; j++)
                {
                    C_GetNextValue(LABEL_DEFINE);
                    g_scriptPtr--;
                    params[j] = *g_scriptPtr;

                    if (j != 25) continue;

                    if (C_GetKeyword() != -1)
                    {
                        //                LOG_F("Duke Nukem 3D v1.3D style CON files detected.\n");
                        break;
                    }
                    else
                    {
                        g_scriptVersion = 14;
                        //                LOG_F("Duke Nukem 3D v1.4+ style CON files detected.\n");
                    }

                }

                /*
                v1.3d                   v1.5
                DEFAULTVISIBILITY       DEFAULTVISIBILITY
                GENERICIMPACTDAMAGE     GENERICIMPACTDAMAGE
                MAXPLAYERHEALTH         MAXPLAYERHEALTH
                STARTARMORHEALTH        STARTARMORHEALTH
                RESPAWNACTORTIME        RESPAWNACTORTIME
                RESPAWNITEMTIME         RESPAWNITEMTIME
                RUNNINGSPEED            RUNNINGSPEED
                RPGBLASTRADIUS          GRAVITATIONALCONSTANT
                PIPEBOMBRADIUS          RPGBLASTRADIUS
                SHRINKERBLASTRADIUS     PIPEBOMBRADIUS
                TRIPBOMBBLASTRADIUS     SHRINKERBLASTRADIUS
                MORTERBLASTRADIUS       TRIPBOMBBLASTRADIUS
                BOUNCEMINEBLASTRADIUS   MORTERBLASTRADIUS
                SEENINEBLASTRADIUS      BOUNCEMINEBLASTRADIUS
                MAXPISTOLAMMO           SEENINEBLASTRADIUS
                MAXSHOTGUNAMMO          MAXPISTOLAMMO
                MAXCHAINGUNAMMO         MAXSHOTGUNAMMO
                MAXRPGAMMO              MAXCHAINGUNAMMO
                MAXHANDBOMBAMMO         MAXRPGAMMO
                MAXSHRINKERAMMO         MAXHANDBOMBAMMO
                MAXDEVISTATORAMMO       MAXSHRINKERAMMO
                MAXTRIPBOMBAMMO         MAXDEVISTATORAMMO
                MAXFREEZEAMMO           MAXTRIPBOMBAMMO
                CAMERASDESTRUCTABLE     MAXFREEZEAMMO
                NUMFREEZEBOUNCES        MAXGROWAMMO
                FREEZERHURTOWNER        CAMERASDESTRUCTABLE
                                        NUMFREEZEBOUNCES
                                        FREEZERHURTOWNER
                                        QSIZE
                                        TRIPBOMBLASERMODE
                */

                j = 0;
                ud.const_visibility = params[j++];
                g_impactDamage = params[j++];
                g_player[0].ps->max_player_health = g_player[0].ps->max_shield_amount = params[j++];
                g_startArmorAmount = params[j++];
                g_actorRespawnTime = params[j++];
                g_itemRespawnTime = params[j++];
                g_playerFriction = params[j++];

                if (g_scriptVersion == 14)
                    g_spriteGravity = params[j++];

                g_rpgBlastRadius = params[j++];
                g_pipebombBlastRadius = params[j++];
                g_shrinkerBlastRadius = params[j++];
                g_tripbombBlastRadius = params[j++];
                g_morterBlastRadius = params[j++];
                g_bouncemineBlastRadius = params[j++];
                g_seenineBlastRadius = params[j++];
                g_player[0].ps->max_ammo_amount[PISTOL_WEAPON] = params[j++];
                g_player[0].ps->max_ammo_amount[SHOTGUN_WEAPON] = params[j++];
                g_player[0].ps->max_ammo_amount[CHAINGUN_WEAPON] = params[j++];
                g_player[0].ps->max_ammo_amount[RPG_WEAPON] = params[j++];
                g_player[0].ps->max_ammo_amount[HANDBOMB_WEAPON] = params[j++];
                g_player[0].ps->max_ammo_amount[SHRINKER_WEAPON] = params[j++];
                g_player[0].ps->max_ammo_amount[DEVISTATOR_WEAPON] = params[j++];
                g_player[0].ps->max_ammo_amount[TRIPBOMB_WEAPON] = params[j++];
                g_player[0].ps->max_ammo_amount[FREEZE_WEAPON] = params[j++];

                if (g_scriptVersion == 14)
                    g_player[0].ps->max_ammo_amount[GROW_WEAPON] = params[j++];

                g_damageCameras = params[j++];
                g_numFreezeBounces = params[j++];
                g_freezerSelfDamage = params[j++];

                if (g_scriptVersion == 14)
                {
                    g_spriteDeleteQueueSize = params[j++];
                    if (g_spriteDeleteQueueSize > 1024)
                        g_spriteDeleteQueueSize = 1024;
                    else if (g_spriteDeleteQueueSize < 0)
                        g_spriteDeleteQueueSize = 0;

                    g_tripbombLaserMode = params[j++];
                }
                continue;
            }
        }
    } while (loop);
    return 0;
}

#define NUM_DEFAULT_CONS    4
static const char *defaultcons[NUM_DEFAULT_CONS] =
{
    "EDUKE.CON",
    "GAME.CON",
    "USER.CON",
    "DEFS.CON"
};

void copydefaultcons(void)
{
    int i, fs, fpi;
    FILE *fpo;

    for (i=0;i<NUM_DEFAULT_CONS;i++)
    {
        fpi = kopen4loadfrommod(defaultcons[i] , 1);
        if (fpi < 0) continue;

        fpo = fopenfrompath(defaultcons[i],"wb");

        if (fpo == NULL)
        {
            kclose(fpi);
            continue;
        }

        fs = kfilelength(fpi);

        kread(fpi,&actor[0],fs);
        if (fwrite(&actor[0],fs,1,fpo)==0)LOG_F(ERROR, "Failed to restore default CONs.");

        kclose(fpi);
        fclose(fpo);
    }
}

/* Anything added with C_AddDefinition() cannot be overwritten in the CONs */

static void C_AddDefinition(const char *lLabel,int lValue,int lType)
{
    Bstrcpy(LAST_LABEL, lLabel);
    labeltype[g_labelCnt] = lType;
    hash_add(&h_labels, LAST_LABEL, g_labelCnt, 0);
    labelcode[g_labelCnt++] = lValue;
}


// Macro for quickly defining events
const char* EventNames[MAXEVENTS];
#define C_AddEvent(eventid) C_AddDefinition(#eventid, eventid, LABEL_DEFINE | LABEL_EVENT); EventNames[eventid] = #eventid

static void C_AddDefaultDefinitions(void)
{
    C_AddEvent(EVENT_AIMDOWN);
    C_AddEvent(EVENT_AIMUP);
    C_AddEvent(EVENT_ANIMATESPRITES);
    C_AddEvent(EVENT_CHANGEWEAPON);
    C_AddEvent(EVENT_CHEATGETBOOT);
    C_AddEvent(EVENT_CHEATGETFIRSTAID);
    C_AddEvent(EVENT_CHEATGETHEAT);
    C_AddEvent(EVENT_CHEATGETHOLODUKE);
    C_AddEvent(EVENT_CHEATGETJETPACK);
    C_AddEvent(EVENT_CHEATGETSCUBA);
    C_AddEvent(EVENT_CHEATGETSHIELD);
    C_AddEvent(EVENT_CHEATGETSTEROIDS);
    C_AddEvent(EVENT_CROUCH);
    C_AddEvent(EVENT_DISPLAYCROSSHAIR);
    C_AddEvent(EVENT_DISPLAYREST);
    C_AddEvent(EVENT_DISPLAYBONUSSCREEN);
    C_AddEvent(EVENT_DISPLAYMENU);
    C_AddEvent(EVENT_DISPLAYMENUREST);
    C_AddEvent(EVENT_DISPLAYLOADINGSCREEN);
    C_AddEvent(EVENT_DISPLAYSTART);
    C_AddEvent(EVENT_DISPLAYROOMS);
    C_AddEvent(EVENT_DISPLAYSBAR);
    C_AddEvent(EVENT_DISPLAYLEVELSTATS);
    C_AddEvent(EVENT_DISPLAYACCESS);
    C_AddEvent(EVENT_DISPLAYWEAPON);
    C_AddEvent(EVENT_DISPLAYTIP);
    C_AddEvent(EVENT_DOFIRE);
    C_AddEvent(EVENT_PREWEAPONSHOOT);
    C_AddEvent(EVENT_POSTWEAPONSHOOT);
    C_AddEvent(EVENT_DRAWWEAPON);
    C_AddEvent(EVENT_EGS);
    C_AddEvent(EVENT_ENTERLEVEL);
    C_AddEvent(EVENT_FAKEDOMOVETHINGS);
    C_AddEvent(EVENT_FIRE);
    C_AddEvent(EVENT_FIREWEAPON);
    C_AddEvent(EVENT_GAME);
    C_AddEvent(EVENT_GETAUTOAIMANGLE);
    C_AddEvent(EVENT_GETLOADTILE);
    C_AddEvent(EVENT_GETMENUTILE);
    C_AddEvent(EVENT_GETSHOTRANGE);
    C_AddEvent(EVENT_HOLODUKEOFF);
    C_AddEvent(EVENT_HOLODUKEON);
    C_AddEvent(EVENT_HOLSTER);
    C_AddEvent(EVENT_INCURDAMAGE);
    C_AddEvent(EVENT_INIT);
    C_AddEvent(EVENT_INVENTORY);
    C_AddEvent(EVENT_INVENTORYLEFT);
    C_AddEvent(EVENT_INVENTORYRIGHT);
    C_AddEvent(EVENT_JUMP);
    C_AddEvent(EVENT_LOGO);
    C_AddEvent(EVENT_LOOKDOWN);
    C_AddEvent(EVENT_LOOKLEFT);
    C_AddEvent(EVENT_LOOKRIGHT);
    C_AddEvent(EVENT_LOOKUP);
    C_AddEvent(EVENT_MOVEBACKWARD);
    C_AddEvent(EVENT_MOVEFORWARD);
    C_AddEvent(EVENT_NEXTWEAPON);
    C_AddEvent(EVENT_PREVIOUSWEAPON);
    C_AddEvent(EVENT_PRESSEDFIRE);
    C_AddEvent(EVENT_PROCESSINPUT);
    C_AddEvent(EVENT_QUICKKICK);
    C_AddEvent(EVENT_RESETINVENTORY);
    C_AddEvent(EVENT_RESETPLAYER);
    C_AddEvent(EVENT_RESETWEAPONS);
    C_AddEvent(EVENT_RETURNTOCENTER);
    C_AddEvent(EVENT_SELECTWEAPON);
    C_AddEvent(EVENT_SETDEFAULTS);
    C_AddEvent(EVENT_SOARDOWN);
    C_AddEvent(EVENT_SOARUP);
    C_AddEvent(EVENT_SPAWN);
    C_AddEvent(EVENT_STRAFELEFT);
    C_AddEvent(EVENT_STRAFERIGHT);
    C_AddEvent(EVENT_SWIMDOWN);
    C_AddEvent(EVENT_SWIMUP);
    C_AddEvent(EVENT_TURNAROUND);
    C_AddEvent(EVENT_TURNLEFT);
    C_AddEvent(EVENT_TURNRIGHT);
    C_AddEvent(EVENT_USE);
    C_AddEvent(EVENT_USEJETPACK);
    C_AddEvent(EVENT_USEMEDKIT);
    C_AddEvent(EVENT_USENIGHTVISION);
    C_AddEvent(EVENT_USESTEROIDS);
    C_AddEvent(EVENT_WEAPKEY10);
    C_AddEvent(EVENT_WEAPKEY1);
    C_AddEvent(EVENT_WEAPKEY2);
    C_AddEvent(EVENT_WEAPKEY3);
    C_AddEvent(EVENT_WEAPKEY4);
    C_AddEvent(EVENT_WEAPKEY5);
    C_AddEvent(EVENT_WEAPKEY6);
    C_AddEvent(EVENT_WEAPKEY7);
    C_AddEvent(EVENT_WEAPKEY8);
    C_AddEvent(EVENT_WEAPKEY9);
    C_AddEvent(EVENT_KILLIT);
    C_AddEvent(EVENT_LOADACTOR);
    C_AddEvent(EVENT_NEWGAME);
    C_AddEvent(EVENT_PREGAME);
    C_AddEvent(EVENT_PREWORLD);
    C_AddEvent(EVENT_WORLD);
    C_AddEvent(EVENT_SOUND);
    C_AddEvent(EVENT_PRELEVEL);
    C_AddEvent(EVENT_DAMAGESPRITE);
    C_AddEvent(EVENT_POSTDAMAGESPRITE);
    C_AddEvent(EVENT_DAMAGEFLOOR);
    C_AddEvent(EVENT_DAMAGECEILING);
    C_AddEvent(EVENT_DAMAGECEILING);
    C_AddEvent(EVENT_ALTWEAPON);
    C_AddEvent(EVENT_LASTWEAPON);
    C_AddEvent(EVENT_ALTFIRE);
    C_AddEvent(EVENT_DOQUAKE);
    C_AddEvent(EVENT_CHECKTOUCHDAMAGE);
    C_AddEvent(EVENT_BOTGETSPRITESCORE);
    C_AddEvent(EVENT_FRAGGED);
    C_AddEvent(EVENT_ACTIVATESWITCH);

    C_AddDefinition("STR_MAPNAME",          STR_MAPNAME,        LABEL_DEFINE);
    C_AddDefinition("STR_MAPFILENAME",      STR_MAPFILENAME,    LABEL_DEFINE);
    C_AddDefinition("STR_PLAYERNAME",       STR_PLAYERNAME,     LABEL_DEFINE);
    C_AddDefinition("STR_VERSION",          STR_VERSION,        LABEL_DEFINE);
    C_AddDefinition("STR_GAMETYPE",         STR_GAMETYPE,       LABEL_DEFINE);

    C_AddDefinition("MAX_WEAPONS",          MAX_WEAPONS,        LABEL_DEFINE);
    C_AddDefinition("MAXSPRITES",           MAXSPRITES,         LABEL_DEFINE);
    C_AddDefinition("MAXSPRITESONSCREEN",   MAXSPRITESONSCREEN, LABEL_DEFINE);
    C_AddDefinition("MAXSTATUS",            MAXSTATUS,          LABEL_DEFINE);
    C_AddDefinition("MAXTILES",             MAXTILES,           LABEL_DEFINE);
    C_AddDefinition("MAXLEVELS",            MAXLEVELS,          LABEL_DEFINE);
    C_AddDefinition("MAXVOLUMES",           MAXVOLUMES,         LABEL_DEFINE);
    C_AddDefinition("SPECIALMUS",           (MAXVOLUMES + 1),   LABEL_DEFINE);
    C_AddDefinition("MAXSECTORS",           MAXSECTORS,         LABEL_DEFINE);

    C_AddDefinition("CLIPMASK0",            CLIPMASK0,          LABEL_DEFINE);
    C_AddDefinition("CLIPMASK1",            CLIPMASK1,          LABEL_DEFINE);

    C_AddDefinition("GAMEARRAY_BOOLEAN",    GAMEARRAY_BITMAP,   LABEL_DEFINE);
    C_AddDefinition("GAMEARRAY_INT16",      GAMEARRAY_INT16,    LABEL_DEFINE);
    C_AddDefinition("GAMEARRAY_INT8",       GAMEARRAY_INT8,     LABEL_DEFINE);
    C_AddDefinition("GAMEARRAY_RESTORE",    GAMEARRAY_RESTORE,  LABEL_DEFINE);
    C_AddDefinition("GAMEARRAY_UINT16",     GAMEARRAY_UINT16,   LABEL_DEFINE);
    C_AddDefinition("GAMEARRAY_UINT8",      GAMEARRAY_UINT8,    LABEL_DEFINE);

    C_AddDefinition("GAMEVAR_NODEFAULT",            GAMEVAR_NODEFAULT,                      LABEL_DEFINE);
    C_AddDefinition("GAMEVAR_NOMULTI",              GAMEVAR_NOMULTI,                        LABEL_DEFINE);
    C_AddDefinition("GAMEVAR_NORESET",              GAMEVAR_NORESET,                        LABEL_DEFINE);
    C_AddDefinition("GAMEVAR_PERACTOR",             GAMEVAR_PERACTOR,                       LABEL_DEFINE);
    C_AddDefinition("GAMEVAR_PERPLAYER",            GAMEVAR_PERPLAYER,                      LABEL_DEFINE);
    C_AddDefinition("GAMEVAR_NOMULTI_PERPLAYER",    GAMEVAR_NOMULTI | GAMEVAR_PERPLAYER,    LABEL_DEFINE);
    C_AddDefinition("GAMEVAR_NOMULTI_PERACTOR",     GAMEVAR_NOMULTI | GAMEVAR_PERACTOR,     LABEL_DEFINE);

    C_AddDefinition("STAT_DEFAULT",     STAT_ACTOR,         LABEL_DEFINE);
    C_AddDefinition("STAT_ACTOR",       STAT_ACTOR,         LABEL_DEFINE);
    C_AddDefinition("STAT_ZOMBIEACTOR", STAT_ZOMBIEACTOR,   LABEL_DEFINE);
    C_AddDefinition("STAT_EFFECTOR",    STAT_EFFECTOR,      LABEL_DEFINE);
    C_AddDefinition("STAT_PROJECTILE",  STAT_PROJECTILE,    LABEL_DEFINE);
    C_AddDefinition("STAT_MISC",        STAT_MISC,          LABEL_DEFINE);
    C_AddDefinition("STAT_STANDABLE",   STAT_STANDABLE,     LABEL_DEFINE);
    C_AddDefinition("STAT_LOCATOR",     STAT_LOCATOR,       LABEL_DEFINE);
    C_AddDefinition("STAT_ACTIVATOR",   STAT_ACTIVATOR,     LABEL_DEFINE);
    C_AddDefinition("STAT_TRANSPORT",   STAT_TRANSPORT,     LABEL_DEFINE);
    C_AddDefinition("STAT_PLAYER",      STAT_PLAYER,        LABEL_DEFINE);
    C_AddDefinition("STAT_FX",          STAT_FX,            LABEL_DEFINE);
    C_AddDefinition("STAT_FALLER",      STAT_FALLER,        LABEL_DEFINE);
    C_AddDefinition("STAT_DUMMYPLAYER", STAT_DUMMYPLAYER,   LABEL_DEFINE);
    C_AddDefinition("STAT_LIGHT",       STAT_LIGHT,         LABEL_DEFINE);

    C_AddDefinition("noclip",   noclip, LABEL_DEFINE);

    C_AddDefinition("NO", 0, LABEL_DEFINE | LABEL_ACTION | LABEL_AI | LABEL_MOVE);

    C_AddDefinition("PROJ_BOUNCES",     PROJ_BOUNCES,       LABEL_DEFINE);
    C_AddDefinition("PROJ_BSOUND",      PROJ_BSOUND,        LABEL_DEFINE);
    C_AddDefinition("PROJ_CLIPDIST",    PROJ_CLIPDIST,      LABEL_DEFINE);
    C_AddDefinition("PROJ_CSTAT",       PROJ_CSTAT,         LABEL_DEFINE);
    C_AddDefinition("PROJ_DECAL",       PROJ_DECAL,         LABEL_DEFINE);
    C_AddDefinition("PROJ_DROP",        PROJ_DROP,          LABEL_DEFINE);
    C_AddDefinition("PROJ_EXTRA",       PROJ_EXTRA,         LABEL_DEFINE);
    C_AddDefinition("PROJ_EXTRA_RAND",  PROJ_EXTRA_RAND,    LABEL_DEFINE);
    C_AddDefinition("PROJ_FLASH_COLOR", PROJ_FLASH_COLOR,   LABEL_DEFINE);
    C_AddDefinition("PROJ_HITRADIUS",   PROJ_HITRADIUS,     LABEL_DEFINE);
    C_AddDefinition("PROJ_ISOUND",      PROJ_ISOUND,        LABEL_DEFINE);
    C_AddDefinition("PROJ_OFFSET",      PROJ_OFFSET,        LABEL_DEFINE);
    C_AddDefinition("PROJ_PAL",         PROJ_PAL,           LABEL_DEFINE);
    C_AddDefinition("PROJ_RANGE",       PROJ_RANGE,         LABEL_DEFINE);
    C_AddDefinition("PROJ_SHADE",       PROJ_SHADE,         LABEL_DEFINE);
    C_AddDefinition("PROJ_SOUND",       PROJ_SOUND,         LABEL_DEFINE);
    C_AddDefinition("PROJ_SPAWNS",      PROJ_SPAWNS,        LABEL_DEFINE);
    C_AddDefinition("PROJ_SXREPEAT",    PROJ_SXREPEAT,      LABEL_DEFINE);
    C_AddDefinition("PROJ_SYREPEAT",    PROJ_SYREPEAT,      LABEL_DEFINE);
    C_AddDefinition("PROJ_TNUM",        PROJ_TNUM,          LABEL_DEFINE);
    C_AddDefinition("PROJ_TOFFSET",     PROJ_TOFFSET,       LABEL_DEFINE);
    C_AddDefinition("PROJ_TRAIL",       PROJ_TRAIL,         LABEL_DEFINE);
    C_AddDefinition("PROJ_TXREPEAT",    PROJ_TXREPEAT,      LABEL_DEFINE);
    C_AddDefinition("PROJ_TYREPEAT",    PROJ_TYREPEAT,      LABEL_DEFINE);
    C_AddDefinition("PROJ_VEL_MULT",    PROJ_MOVECNT,       LABEL_DEFINE);
    C_AddDefinition("PROJ_VEL",         PROJ_VEL,           LABEL_DEFINE);
    C_AddDefinition("PROJ_WORKSLIKE",   PROJ_WORKSLIKE,     LABEL_DEFINE);
    C_AddDefinition("PROJ_XREPEAT",     PROJ_XREPEAT,       LABEL_DEFINE);
    C_AddDefinition("PROJ_YREPEAT",     PROJ_YREPEAT,       LABEL_DEFINE);

    C_AddDefinition("SFLAG_BADGUY",             SFLAG_BADGUY,           LABEL_DEFINE);
    C_AddDefinition("SFLAG_DAMAGEEVENT",        SFLAG_DAMAGEEVENT,      LABEL_DEFINE);
    C_AddDefinition("SFLAG_GREENSLIMEFOOD",     SFLAG_GREENSLIMEFOOD,   LABEL_DEFINE);
    C_AddDefinition("SFLAG_HURTSPAWNBLOOD",     SFLAG_HURTSPAWNBLOOD,   LABEL_DEFINE);
    //C_AddDefinition("SFLAG_NOCLIP",             SFLAG_NOCLIP,           LABEL_DEFINE);
    C_AddDefinition("SFLAG_NODAMAGEPUSH",       SFLAG_NODAMAGEPUSH,     LABEL_DEFINE);
    C_AddDefinition("SFLAG_NOEVENTS",           SFLAG_NOEVENTCODE,      LABEL_DEFINE);
    C_AddDefinition("SFLAG_NOLIGHT",            SFLAG_NOLIGHT,          LABEL_DEFINE);
    C_AddDefinition("SFLAG_NOPAL",              SFLAG_NOPAL,            LABEL_DEFINE);
    C_AddDefinition("SFLAG_NOSHADE",            SFLAG_NOSHADE,          LABEL_DEFINE);
    C_AddDefinition("SFLAG_NOTELEPORT",         SFLAG_NOTELEPORT,       LABEL_DEFINE);
    C_AddDefinition("SFLAG_NOWATERDIP",         SFLAG_NOWATERDIP,       LABEL_DEFINE);
    C_AddDefinition("SFLAG_NVG",                SFLAG_NVG,              LABEL_DEFINE);
    //C_AddDefinition("SFLAG_REALCLIPDIST",       SFLAG_REALCLIPDIST,     LABEL_DEFINE);
    C_AddDefinition("SFLAG_SHADOW",             SFLAG_SHADOW,           LABEL_DEFINE);
    C_AddDefinition("SFLAG_SMOOTHMOVE",         SFLAG_SMOOTHMOVE,       LABEL_DEFINE);
    C_AddDefinition("SFLAG_USEACTIVATOR",       SFLAG_USEACTIVATOR,     LABEL_DEFINE);
    //C_AddDefinition("SFLAG_WAKEUPBADGUYS",      SFLAG_WAKEUPBADGUYS,    LABEL_DEFINE);
    //C_AddDefinition("SFLAG_NOWATERSECTOR",      SFLAG_NOWATERSECTOR,    LABEL_DEFINE);
    C_AddDefinition("SFLAG_QUEUEDFORDELETE",    SFLAG_QUEUEDFORDELETE,  LABEL_DEFINE);
}

static char const* C_ScriptVersionString(int32_t version)
{
#ifdef EDUKE32_STANDALONE
    UNREFERENCED_PARAMETER(version);
#else
    switch (version)
    {
        case 9:
            return ", v0.99 compatibility mode";
        case 10:
            return ", v1.0 compatibility mode";
        case 11:
            return ", v1.1 compatibility mode";
        case 13:
            return ", v1.3D compatibility mode";
    }
#endif
    return "";
}

void C_PrintStats(void)
{
    VLOG_F(LOG_CON, "%d/%d labels, %d/%d variables, %d/%d arrays", g_labelCnt, MAXLABELS,
        g_gameVarCount, MAXGAMEVARS, g_gameArrayCount, MAXGAMEARRAYS);

    int cnt = g_numXStrings, cnt2 = 0, cnt3 = 0;

    for (auto& ptr : apStrings)
        if (ptr)
            cnt++;

    for (auto& apScriptEvent : apScriptEvents)
        if (apScriptEvent)
            cnt2++;

    for (auto& tile : g_tile)
        if (tile.execPtr)
            cnt3++;

    VLOG_F(LOG_CON, "%d strings, %d events, %d actors", cnt, cnt2, cnt3);
}

// TODO: add some kind of mapping between the table and the struct holding the tokens
void scriptInitTables()
{
    for (auto table : tables)
        hash_init(table);

    for (auto table : inttables)
        inthash_init(table);

    for (auto& keyword : vm_keywords)
        hash_add(&h_keywords, keyword.token, keyword.val, 0);

    for (auto& iter_token : iter_tokens)
        hash_add(&h_iter, iter_token.token, iter_token.val, 0);

    for (auto& varvar : varvartable)
        inthash_add(&h_varvar, varvar.x, varvar.y, 0);

    for (auto& globalvar : globalvartable)
        inthash_add(&h_globalvar, globalvar.x, globalvar.y, 0);

    for (auto& playervar : playervartable)
        inthash_add(&h_playervar, playervar.x, playervar.y, 0);

    for (auto& actorvar : actorvartable)
        inthash_add(&h_actorvar, actorvar.x, actorvar.y, 0);
}

static void C_ReplaceQuoteSubstring(const size_t q, char const* const query, char const* const replacement)
{
    size_t querylength = Bstrlen(query);

    for (bssize_t i = MAXQUOTELEN - querylength - 2; i >= 0; i--)
        if (Bstrncmp(&apStrings[q][i], query, querylength) == 0)
        {
            Bmemset(tempbuf, 0, sizeof(tempbuf));
            Bstrncpy(tempbuf, apStrings[q], i);
            Bstrcat(tempbuf, replacement);
            Bstrcat(tempbuf, &apStrings[q][i + querylength]);
            Bstrncpyz(apStrings[q], tempbuf, MAXQUOTELEN);
            i = MAXQUOTELEN - querylength - 2;
        }
}

extern int g_numObituaries;
extern int g_numSelfObituaries;
static void C_InitQuotes(void)
{
    for (int i = 127; i >= 0; i--)
        C_AllocQuote(i);

    // C_ReplaceQuoteSubstring in mainline
    char const* const OpenGameFunc = gamefunctions[gamefunc_Open];
    C_ReplaceQuoteSubstring(QUOTE_DEAD, "SPACE", OpenGameFunc);
    C_ReplaceQuoteSubstring(QUOTE_DEAD, "OPEN", OpenGameFunc);
    C_ReplaceQuoteSubstring(QUOTE_DEAD, "USE", OpenGameFunc);

    // most of these are based on Blood, obviously
    const char* PlayerObituaries[] =
    {
        "^02%s^02 beat %s^02 like a cur",
        "^02%s^02 broke %s",
        "^02%s^02 body bagged %s",
        "^02%s^02 boned %s^02 like a fish",
        "^02%s^02 castrated %s",
        "^02%s^02 creamed %s",
        "^02%s^02 crushed %s",
        "^02%s^02 destroyed %s",
        "^02%s^02 diced %s",
        "^02%s^02 disemboweled %s",
        "^02%s^02 erased %s",
        "^02%s^02 eviscerated %s",
        "^02%s^02 flailed %s",
        "^02%s^02 flattened %s",
        "^02%s^02 gave AnAl MaDnEsS to %s",
        "^02%s^02 gave %s^02 Anal Justice",
        "^02%s^02 hosed %s",
        "^02%s^02 hurt %s^02 real bad",
        "^02%s^02 killed %s",
        "^02%s^02 made dog meat out of %s",
        "^02%s^02 made mincemeat out of %s",
        "^02%s^02 manhandled %s",
        "^02%s^02 massacred %s",
        "^02%s^02 mutilated %s",
        "^02%s^02 murdered %s",
        "^02%s^02 neutered %s",
        "^02%s^02 reamed %s",
        "^02%s^02 ripped %s^02 a new orifice",
        "^02%s^02 rocked %s",
        "^02%s^02 sent %s^02 to hell",
        "^02%s^02 shredded %s",
        "^02%s^02 slaughtered %s",
        "^02%s^02 sliced %s",
        "^02%s^02 smacked %s around",
        "^02%s^02 smashed %s",
        "^02%s^02 snuffed %s",
        "^02%s^02 sodomized %s",
        "^02%s^02 splattered %s",
        "^02%s^02 sprayed %s",
        "^02%s^02 squashed %s",
        "^02%s^02 throttled %s",
        "^02%s^02 toasted %s",
        "^02%s^02 vented %s",
        "^02%s^02 ventilated %s",
        "^02%s^02 wasted %s",
        "^02%s^02 wrecked %s",
    };

    const char* PlayerSelfObituaries[] =
    {
        "^02%s^02 is excrement",
        "^02%s^02 is hamburger",
        "^02%s^02 suffered scrotum separation",
        "^02%s^02 volunteered for population control",
        "^02%s^02 has suicided",
        "^02%s^02 bled out",
        "^02%s^02 died of ligma",
    };

    EDUKE32_STATIC_ASSERT(OBITQUOTEINDEX + ARRAY_SIZE(PlayerObituaries) - 1 < MAXQUOTES);
    EDUKE32_STATIC_ASSERT(SUICIDEQUOTEINDEX + ARRAY_SIZE(PlayerSelfObituaries) - 1 < MAXQUOTES);

    g_numObituaries = ARRAY_SIZE(PlayerObituaries);
    for (int i = g_numObituaries - 1; i >= 0; i--)
    {
        if (apStrings[i + OBITQUOTEINDEX] == NULL)
        {
            apStrings[i + OBITQUOTEINDEX] = (char*)Xcalloc(MAXQUOTELEN, sizeof(char));
            Bstrcpy(apStrings[i + OBITQUOTEINDEX], PlayerObituaries[i]);
        }
    }

    g_numSelfObituaries = ARRAY_SIZE(PlayerSelfObituaries);
    for (int i = g_numSelfObituaries - 1; i >= 0; i--)
    {
        if (apStrings[i + SUICIDEQUOTEINDEX] == NULL)
        {
            apStrings[i + SUICIDEQUOTEINDEX] = (char*)Xcalloc(MAXQUOTELEN, sizeof(char));
            Bstrcpy(apStrings[i + SUICIDEQUOTEINDEX], PlayerSelfObituaries[i]);
        }
    }
}

static int C_GetLabelIndex(int32_t val, int type)
{
    for (int i = 0; i < g_labelCnt; i++)
        if (labelcode[i] == val && (labeltype[i] & type) != 0)
            return i;

    for (int i = 0; i < g_labelCnt; i++)
        if (labelcode[i] == val)
            return i;

    return -1;
}

void C_Compile(const char* fileName)
{
    Bmemset(apScriptEvents, 0, sizeof(apScriptEvents));
    Bmemset(apScriptGameEventEnd, 0, sizeof(apScriptGameEventEnd));

    for (auto& i : g_tile)
        Bmemset(&i, 0, sizeof(tiledata_t));

    scriptInitTables();
    VM_InitHashTables();

    Gv_Init();
    C_InitProjectiles();

    buildvfs_kfd kFile = kopen4loadfrommod(fileName, g_loadFromGroupOnly);

    if (kFile == buildvfs_kfd_invalid) // JBF: was 0
    {
        if (g_loadFromGroupOnly == 1 || numgroupfiles == 0)
        {
#ifndef EDUKE32_STANDALONE
            char const* gf = G_GrpFile();
            Bsprintf(tempbuf, "Required game data was not found.  A valid copy of '%s' or other compatible data is needed to run EDuke32.\n\n"
                "You must copy '%s' to your game directory before continuing!", gf, gf);
            G_GameExit(tempbuf);
#else
            G_GameExit();
#endif
        }
        else
        {
            Bsprintf(tempbuf, "Unable to load %s: file not found.", fileName);
            G_GameExit(tempbuf);
        }

        //g_loadFromGroupOnly = 1;
        return; //Not there
        }

    int const kFileLen = kfilelength(kFile);

    VLOG_F(LOG_CON, "Compiling: %s (%d bytes)", fileName, kFileLen);

    C_AddFileOffset(0, fileName);

    uint32_t const startcompiletime = timerGetTicks();

    char* mptr = (char*)Xmalloc(kFileLen + 1);
    mptr[kFileLen] = 0;

    textptr = (char*)mptr;
    kread(kFile, (char*)textptr, kFileLen);
    kclose(kFile);

    Xfree(apScript);

    apScript = (intptr_t*)Xcalloc(1, g_scriptSize * sizeof(intptr_t));
    bitptr = (uint8_t*)Xcalloc(1, (((g_scriptSize + 7) >> 3) + 1) * sizeof(uint8_t));

    g_errorCnt = 0;
    g_labelCnt = 0;
    g_lineNumber = 1;
    g_scriptPtr = apScript + 3;  // move permits constants 0 and 1; moveptr[1] would be script[2] (reachable?)
    g_totalLines = 0;
    g_warningCnt = 0;

    Bstrcpy(g_scriptFileName, fileName);

    C_AddDefaultDefinitions();
    C_ParseCommand(true);

    for (char* m : g_scriptModules)
    {
        C_Include(m);
        Xfree(m);
    }
    g_scriptModules.clear();

    if (g_errorCnt > 63)
        LOG_F(ERROR, "Excessive script errors.");

    //*script = (intptr_t) g_scriptPtr;

    DO_FREE_AND_NULL(mptr);

    if (g_warningCnt || g_errorCnt)
    {
        VLOG_F(g_errorCnt ? loguru::Verbosity_ERROR : g_warningCnt ? loguru::Verbosity_WARNING : loguru::Verbosity_INFO, "Found %d warning(s), %d error(s).", g_warningCnt, g_errorCnt);

        if (g_errorCnt)
        {
            Bsprintf(buf, "Error compiling CON files.");
            G_GameExit(buf);
        }
    }

    for (intptr_t i : apScriptGameEventEnd)
    {
        if (!i)
            continue;

        auto const eventEnd = apScript + i;
        auto breakPtr = (intptr_t*)*(eventEnd + 2);

        while (breakPtr)
        {
            breakPtr = apScript + (intptr_t)breakPtr;
            scriptWriteAtOffset(CON_ENDEVENT | LINE_NUMBER, breakPtr - 2);
            breakPtr = (intptr_t*)*breakPtr;
        }
    }

    g_totalLines += g_lineNumber;

    C_SetScriptSize(g_scriptPtr - apScript + 8);

    VLOG_F(LOG_CON, "Compiled %d bytes in %ums%s", (int)((intptr_t)g_scriptPtr - (intptr_t)apScript),
        timerGetTicks() - startcompiletime, C_ScriptVersionString(g_scriptVersion));

    for (auto i : tables_free)
        hash_free(i);

    for (auto i : inttables)
        inthash_free(i);

    freehashnames();

    if (g_scriptDebug)
        C_PrintStats();

    C_InitQuotes();

#if MICROPROFILE_ENABLED != 0
    for (int i = 0; i < MAXEVENTS; i++)
    {
        if (VM_HaveEvent(i))
        {
            g_eventTokens[i] = MicroProfileGetToken("CON VM Events", EventNames[i], MP_AUTO, MicroProfileTokenTypeCpu);
            g_eventCounterTokens[i] = MicroProfileGetCounterToken(EventNames[i]);
        }
    }

#if 0
    for (int i = 0; i < CON_END; i++)
    {
        Bassert(VM_GetKeywordForID(i) != nullptr);
        g_instTokens[i] = MicroProfileGetToken("CON VM Instructions", VM_GetKeywordForID(i), MP_AUTO, MicroProfileTokenTypeCpu);
    }
#endif

    for (int i = 0; i < MAXSTATUS; i++)
    {
        Bsprintf(tempbuf, "statnum%d", i);
        g_statnumTokens[i] = MicroProfileGetToken("CON VM Actors", tempbuf, MP_AUTO, MicroProfileTokenTypeCpu);
    }
#endif

    for (int i = 0; i < MAXTILES; i++)
    {
        int const index = C_GetLabelIndex(i, LABEL_ANY);

        tempbuf[0] = 0;
        g_tileLabels[i] = nullptr;

        if (index != -1)
        {
            g_tileLabels[i] = label + (index << 6);
            Bsprintf(tempbuf, "%s (%d)", label + (index << 6), i);
        }
        else if (G_TileHasActor(i))
            Bsprintf(tempbuf, "unnamed (%d)", i);

#if MICROPROFILE_ENABLED != 0
        if (tempbuf[0])
            g_actorTokens[i] = MicroProfileGetToken("CON VM Actors", tempbuf, MP_AUTO, MicroProfileTokenTypeCpu);
#endif
    }
}

void C_ReportError(int iError)
{
    if (Bstrcmp(g_szCurrentBlockName,g_szLastBlockName))
    {
        if (g_scriptEventOffset || g_processingState || g_scriptActorOffset)
            LOG_F(INFO, "%s: In %s `%s':",g_scriptFileName,g_scriptEventOffset?"event":g_scriptActorOffset?"actor":"state",g_szCurrentBlockName);
        else LOG_F(INFO, "%s: At top level:",g_scriptFileName);
        Bstrcpy(g_szLastBlockName,g_szCurrentBlockName);
    }
    switch (iError)
    {
    case ERROR_CLOSEBRACKET:
        LOG_F(ERROR, "%s:%d: found more `}' than `{' before `%s'.",g_scriptFileName,g_lineNumber,tempbuf);
        break;
    case ERROR_EVENTONLY:
        LOG_F(ERROR, "%s:%d: command `%s' only valid during events.",g_scriptFileName,g_lineNumber,tempbuf);
        break;
    case ERROR_EXCEEDSMAXPALS:
        LOG_F(ERROR, "%s:%d: `%s' value exceeds MAXPALOOKUPS.  Maximum is %d.", g_scriptFileName, g_lineNumber, tempbuf, MAXPALOOKUPS - 1);
        break;
    case ERROR_EXCEEDSMAXTILES:
        LOG_F(ERROR, "%s:%d: `%s' value exceeds MAXTILES.  Maximum is %d.",g_scriptFileName,g_lineNumber,tempbuf,MAXTILES-1);
        break;
    case ERROR_EXPECTEDKEYWORD:
        LOG_F(ERROR, "%s:%d: expected a keyword but found `%s'.",g_scriptFileName,g_lineNumber,tempbuf);
        break;
    case ERROR_FOUNDWITHIN:
        LOG_F(ERROR, "%s:%d: found `%s' within %s.",g_scriptFileName,g_lineNumber,tempbuf,g_scriptEventOffset?"an event":g_scriptActorOffset?"an actor":"a state");
        break;
    case ERROR_ISAKEYWORD:
        LOG_F(ERROR, "%s:%d: symbol `%s' is a keyword.",g_scriptFileName,g_lineNumber,LAST_LABEL);
        break;
    case ERROR_NOENDSWITCH:
        LOG_F(ERROR, "%s:%d: did not find `endswitch' before `%s'.",g_scriptFileName,g_lineNumber,LAST_LABEL);
        break;
    case ERROR_NOTAGAMEDEF:
        LOG_F(ERROR, "%s:%d: symbol `%s' is not a game definition.",g_scriptFileName,g_lineNumber,LAST_LABEL);
        break;
    case ERROR_NOTAGAMEVAR:
        LOG_F(ERROR, "%s:%d: symbol `%s' is not a game variable.",g_scriptFileName,g_lineNumber,LAST_LABEL);
        break;
    case ERROR_NOTAGAMEARRAY:
        LOG_F(ERROR, "%s:%d: symbol `%s' is not a game array.",g_scriptFileName,g_lineNumber,LAST_LABEL);
        break;
    case ERROR_NOTAMEMBER:
        LOG_F(ERROR, "%s:%d: symbol '%s' is not a valid structure member.", g_scriptFileName, g_lineNumber, LAST_LABEL);
        break;
    case ERROR_GAMEARRAYBNC:
        LOG_F(ERROR, "%s:%d: square brackets for index of game array not closed, expected ] found %c",g_scriptFileName,g_lineNumber,*textptr);
        break;
    case ERROR_GAMEARRAYBNO:
        LOG_F(ERROR, "%s:%d: square brackets for index of game array not opened, expected [ found %c",g_scriptFileName,g_lineNumber,*textptr);
        break;
    case ERROR_INVALIDARRAYWRITE:
        LOG_F(ERROR, "%s:%d: arrays can only be written using setarray %c",g_scriptFileName,g_lineNumber,*textptr);
        break;
    case ERROR_OPENBRACKET:
        LOG_F(ERROR, "%s:%d: found more `{' than `}' before `%s'.",g_scriptFileName,g_lineNumber,tempbuf);
        break;
    case ERROR_PARAMUNDEFINED:
        LOG_F(ERROR, "%s:%d: parameter `%s' is undefined.",g_scriptFileName,g_lineNumber,tempbuf);
        break;
    case ERROR_SYMBOLNOTRECOGNIZED:
        LOG_F(ERROR, "%s:%d: symbol `%s' is not recognized.",g_scriptFileName,g_lineNumber,LAST_LABEL);
        break;
    case ERROR_SYNTAXERROR:
        LOG_F(ERROR, "%s:%d: syntax error.",g_scriptFileName,g_lineNumber);
        break;
    case ERROR_VARREADONLY:
        LOG_F(ERROR, "%s:%d: variable `%s' is read-only.",g_scriptFileName,g_lineNumber,LAST_LABEL);
        break;
    case ERROR_VARTYPEMISMATCH:
        LOG_F(ERROR, "%s:%d: variable `%s' is of the wrong type.",g_scriptFileName,g_lineNumber,LAST_LABEL);
        break;
    case WARNING_BADGAMEVAR:
        LOG_F(WARNING, "%s:%d: variable `%s' should be either per-player OR per-actor, not both.",g_scriptFileName,g_lineNumber,LAST_LABEL);
        break;
    case WARNING_DUPLICATECASE:
        LOG_F(WARNING, "%s:%d: duplicate case ignored.",g_scriptFileName,g_lineNumber);
        break;
    case WARNING_DUPLICATEDEFINITION:
        LOG_F(WARNING, "%s:%d: duplicate game definition `%s' ignored.",g_scriptFileName,g_lineNumber,LAST_LABEL);
        break;
    case WARNING_EVENTSYNC:
        LOG_F(WARNING, "%s:%d: found `%s' within a local event.",g_scriptFileName,g_lineNumber,tempbuf);
        break;
    case WARNING_LABELSONLY:
        LOG_F(WARNING, "%s:%d: expected a label, found a constant.",g_scriptFileName,g_lineNumber);
        break;
    case WARNING_NAMEMATCHESVAR:
        LOG_F(WARNING, "%s:%d: symbol `%s' already used for game variable.",g_scriptFileName,g_lineNumber,LAST_LABEL);
        break;
    case WARNING_REVEVENTSYNC:
        LOG_F(WARNING, "%s:%d: found `%s' outside of a local event.",g_scriptFileName,g_lineNumber,tempbuf);
        break;
    }
}
