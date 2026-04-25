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
#include "osd.h"
#include "gamestructures.h"

#define LABEL(struct, memb, name, idx)                                                              \
    {                                                                                                               \
        name, idx, sizeof(struct[0].memb) | (is_unsigned<decltype(struct[0].memb)>::value ? LABEL_UNSIGNED : 0), 0, \
        offsetof(remove_pointer_t<decltype(&struct[0])>, memb)                                                      \
    }

#define MEMBER(struct, memb, idx) LABEL(struct, memb, #memb, idx)

const memberlabel_t SectorLabels[] =
{
    { "wallptr",            SECTOR_WALLPTR,         0, 0, -1 },
    { "wallnum",            SECTOR_WALLNUM,         0, 0, -1 },
    { "ceilingz",           SECTOR_CEILINGZ,        0, 0, -1 },
    { "ceilingzgoal",       SECTOR_CEILINGZGOAL,    0, 0, -1 },
    { "ceilingzvel",        SECTOR_CEILINGZVEL,     0, 0, -1 },
    { "floorz",             SECTOR_FLOORZ,          0, 0, -1 },
    { "floorzgoal",         SECTOR_FLOORZGOAL,      0, 0, -1 },
    { "floorzvel",          SECTOR_FLOORZVEL,       0, 0, -1 },
    { "ceilingstat",        SECTOR_CEILINGSTAT,     0, 0, -1 },
    { "floorstat",          SECTOR_FLOORSTAT,       0, 0, -1 },
    { "ceilingpicnum",      SECTOR_CEILINGPICNUM,   0, 0, -1 },
    { "ceilingslope",       SECTOR_CEILINGSLOPE,    0, 0, -1 },
    { "ceilingshade",       SECTOR_CEILINGSHADE,    0, 0, -1 },
    { "ceilingpal",         SECTOR_CEILINGPAL,      0, 0, -1 },
    { "ceilingxpanning",    SECTOR_CEILINGXPANNING, 0, 0, -1 },
    { "ceilingypanning",    SECTOR_CEILINGYPANNING, 0, 0, -1 },
    { "floorpicnum",        SECTOR_FLOORPICNUM,     0, 0, -1 },
    { "floorslope",         SECTOR_FLOORSLOPE,      0, 0, -1 },
    { "floorshade",         SECTOR_FLOORSHADE,      0, 0, -1 },
    { "floorpal",           SECTOR_FLOORPAL,        0, 0, -1 },
    { "floorxpanning",      SECTOR_FLOORXPANNING,   0, 0, -1 },
    { "floorypanning",      SECTOR_FLOORYPANNING,   0, 0, -1 },
    { "visibility",         SECTOR_VISIBILITY,      0, 0, -1 },
    //{ "filler",             SECTOR_ALIGNTO,         0, 0, -1 },
    { "alignto",            SECTOR_ALIGNTO,         0, 0, -1 }, // aka filler, not used
    { "lotag",              SECTOR_LOTAG,           0, 0, -1 },
    { "hitag",              SECTOR_HITAG,           0, 0, -1 },
    { "extra",              SECTOR_EXTRA,           0, 0, -1 },
    { "ceilingbunch",       SECTOR_CEILINGBUNCH,    0, 0, -1 },
    { "floorbunch",         SECTOR_FLOORBUNCH,      0, 0, -1 },
    { "",                   -1,                     0, 0, -1 }     // END OF LIST
};

const memberlabel_t WallLabels[] =
{
    { "x",          WALL_X,             0, 0, -1 },
    { "y",          WALL_Y,             0, 0, -1 },
    { "point2",     WALL_POINT2,        0, 0, -1 },
    { "nextwall",   WALL_NEXTWALL,      0, 0, -1 },
    { "nextsector", WALL_NEXTSECTOR,    0, 0, -1 },
    { "cstat",      WALL_CSTAT,         0, 0, -1 },
    { "picnum",     WALL_PICNUM,        0, 0, -1 },
    { "overpicnum", WALL_OVERPICNUM,    0, 0, -1 },
    { "shade",      WALL_SHADE,         0, 0, -1 },
    { "pal",        WALL_PAL,           0, 0, -1 },
    { "xrepeat",    WALL_XREPEAT,       0, 0, -1 },
    { "yrepeat",    WALL_YREPEAT,       0, 0, -1 },
    { "xpanning",   WALL_XPANNING,      0, 0, -1 },
    { "ypanning",   WALL_YPANNING,      0, 0, -1 },
    { "lotag",      WALL_LOTAG,         0, 0, -1 },
    { "hitag",      WALL_HITAG,         0, 0, -1 },
    { "extra",      WALL_EXTRA,         0, 0, -1 },
    { "",           -1,                 0, 0, -1 }     // END OF LIST
};

const memberlabel_t ActorLabels[] =
{
    { "x",              ACTOR_X, 0, 0, -1 },
    { "y",              ACTOR_Y, 0, 0, -1 },
    { "z",              ACTOR_Z, 0, 0, -1 },
    { "cstat",          ACTOR_CSTAT, 0, 0, -1 },
    { "picnum",         ACTOR_PICNUM, 0, 0, -1 },
    { "shade",          ACTOR_SHADE, 0, 0, -1 },
    { "pal",            ACTOR_PAL, 0, 0, -1 },
    { "clipdist",       ACTOR_CLIPDIST, 0, 0, -1 },
    //{ "detail",         ACTOR_DETAIL, 0, 0, -1 }, // aka filler, not used
    { "blend",          ACTOR_BLEND, 0, 0, -1 },
    { "xrepeat",        ACTOR_XREPEAT, 0, 0, -1 },
    { "yrepeat",        ACTOR_YREPEAT, 0, 0, -1 },
    { "xoffset",        ACTOR_XOFFSET, 0, 0, -1 },
    { "yoffset",        ACTOR_YOFFSET, 0, 0, -1 },
    { "sectnum",        ACTOR_SECTNUM, 0, 0, -1 },
    { "statnum",        ACTOR_STATNUM, 0, 0, -1 },
    { "ang",            ACTOR_ANG, 0, 0, -1 },
    { "owner",          ACTOR_OWNER, 0, 0, -1 },
    { "xvel",           ACTOR_XVEL, 0, 0, -1 },
    { "yvel",           ACTOR_YVEL, 0, 0, -1 },
    { "zvel",           ACTOR_ZVEL, 0, 0, -1 },
    { "lotag",          ACTOR_LOTAG, 0, 0, -1 },
    { "hitag",          ACTOR_HITAG, 0, 0, -1 },
    { "extra",          ACTOR_EXTRA, 0, 0, -1 },

    // actor labels...
    { "htcgg",          ACTOR_HTCGG, 0, 0, -1 },
    { "htpicnum",       ACTOR_HTPICNUM, 0, 0, -1 },
    { "htang",          ACTOR_HTANG, 0, 0, -1 },
    { "htextra",        ACTOR_HTEXTRA, 0, 0, -1 },
    { "htowner",        ACTOR_HTOWNER, 0, 0, -1 },
    { "htmovflag",      ACTOR_HTMOVFLAG, 0, 0, -1 },
    { "httempang",      ACTOR_HTTEMPANG, 0, 0, -1 },
    { "htactorstayput", ACTOR_HTSTAYPUT, 0, 0, -1 },
    { "htdispicnum",    ACTOR_HTDISPICNUM, 0, 0, -1 },
    { "httimetosleep",  ACTOR_HTTIMETOSLEEP, 0, 0, -1 },
    { "htfloorz",       ACTOR_HTFLOORZ, 0, 0, -1 },
    { "htceilingz",     ACTOR_HTCEILINGZ, 0, 0, -1 },
    { "htlastvx",       ACTOR_HTLASTVX, 0, 0, -1 },
    { "htlastvy",       ACTOR_HTLASTVY, 0, 0, -1 },
    { "htbposx",        ACTOR_HTBPOSX, 0, 0, -1 },
    { "htbposy",        ACTOR_HTBPOSY, 0, 0, -1 },
    { "htbposz",        ACTOR_HTBPOSZ, 0, 0, -1 },
    { "htg_t",          ACTOR_HTG_T, LABEL_HASPARM2, 10, -1 },
    { "htflags",        ACTOR_HTFLAGS, 0, 0, -1 },

    // model flags

    { "angoff",         ACTOR_ANGOFF, 0, 0, -1 },
    { "pitch",          ACTOR_PITCH, 0, 0, -1 },
    { "roll",           ACTOR_ROLL, 0, 0, -1 },
    { "mdxoff",         ACTOR_MDXOFF, 0, 0, -1 },
    { "mdyoff",         ACTOR_MDYOFF, 0, 0, -1 },
    { "mdzoff",         ACTOR_MDZOFF, 0, 0, -1 },
    { "mdflags",        ACTOR_MDFLAGS, 0, 0, -1 },
    { "xpanning",       ACTOR_XPANNING, 0, 0, -1 },
    { "ypanning",       ACTOR_YPANNING, 0, 0, -1 },

    { "alpha",          ACTOR_ALPHA, 0, 0, -1 },

    { "",               -1, 0, 0, -1  }     // END OF LIST
};

memberlabel_t const TsprLabels[] =
{
    // tsprite access

    LABEL(sprite, x,        "tsprx",        ACTOR_X),
    LABEL(sprite, y,        "tspry",        ACTOR_Y),
    LABEL(sprite, z,        "tsprz",        ACTOR_Z),
    LABEL(sprite, cstat,    "tsprcstat",    ACTOR_CSTAT),
    LABEL(sprite, picnum,   "tsprpicnum",   ACTOR_PICNUM),
    LABEL(sprite, shade,    "tsprshade",    ACTOR_SHADE),
    LABEL(sprite, pal,      "tsprpal",      ACTOR_PAL),
    LABEL(sprite, clipdist, "tsprclipdist", ACTOR_CLIPDIST),
    LABEL(sprite, blend,    "tsprblend",    ACTOR_BLEND),
    LABEL(sprite, xrepeat,  "tsprxrepeat",  ACTOR_XREPEAT),
    LABEL(sprite, yrepeat,  "tspryrepeat",  ACTOR_YREPEAT),
    LABEL(sprite, xoffset,  "tsprxoffset",  ACTOR_XOFFSET),
    LABEL(sprite, yoffset,  "tspryoffset",  ACTOR_YOFFSET),
    LABEL(sprite, sectnum,  "tsprsectnum",  ACTOR_SECTNUM),
    LABEL(sprite, statnum,  "tsprstatnum",  ACTOR_STATNUM),
    LABEL(sprite, ang,      "tsprang",      ACTOR_ANG),
    LABEL(sprite, owner,    "tsprowner",    ACTOR_OWNER),
    LABEL(sprite, xvel,     "tsprxvel",     ACTOR_XVEL),
    LABEL(sprite, yvel,     "tspryvel",     ACTOR_YVEL),
    LABEL(sprite, zvel,     "tsprzvel",     ACTOR_ZVEL),
    LABEL(sprite, lotag,    "tsprlotag",    ACTOR_LOTAG),
    LABEL(sprite, hitag,    "tsprhitag",    ACTOR_HITAG),
    LABEL(sprite, extra,    "tsprextra",    ACTOR_EXTRA),
};

const memberlabel_t PlayerLabels[] =
{
    { "zoom",                   PLAYER_ZOOM, 0, 0, -1 },
    { "exitx",                  PLAYER_EXITX, 0, 0, -1 },
    { "exity",                  PLAYER_EXITY, 0, 0, -1 },
    { "loogiex",                PLAYER_LOOGIEX, LABEL_HASPARM2, 64, -1 },
    { "loogiey",                PLAYER_LOOGIEY, LABEL_HASPARM2, 64, -1 },
    { "numloogs",               PLAYER_NUMLOOGS, 0, 0, -1 },
    { "loogcnt",                PLAYER_LOOGCNT, 0, 0, -1 },
    { "posx",                   PLAYER_POSX, 0, 0, -1 },
    { "posy",                   PLAYER_POSY, 0, 0, -1 },
    { "posz",                   PLAYER_POSZ, 0, 0, -1 },
    { "horiz",                  PLAYER_HORIZ, 0, 0, -1 },
    { "ohoriz",                 PLAYER_OHORIZ, 0, 0, -1 },
    { "ohorizoff",              PLAYER_OHORIZOFF, 0, 0, -1 },
    { "invdisptime",            PLAYER_INVDISPTIME, 0, 0, -1 },
    { "bobposx",                PLAYER_BOBPOSX, 0, 0, -1 },
    { "bobposy",                PLAYER_BOBPOSY, 0, 0, -1 },
    { "oposx",                  PLAYER_OPOSX, 0, 0, -1 },
    { "oposy",                  PLAYER_OPOSY, 0, 0, -1 },
    { "oposz",                  PLAYER_OPOSZ, 0, 0, -1 },
    { "pyoff",                  PLAYER_PYOFF, 0, 0, -1 },
    { "opyoff",                 PLAYER_OPYOFF, 0, 0, -1 },
    { "posxv",                  PLAYER_POSXV, 0, 0, -1 },
    { "posyv",                  PLAYER_POSYV, 0, 0, -1 },
    { "poszv",                  PLAYER_POSZV, 0, 0, -1 },
    { "last_pissed_time",       PLAYER_LAST_PISSED_TIME, 0, 0, -1 },
    { "truefz",                 PLAYER_TRUEFZ, 0, 0, -1 },
    { "truecz",                 PLAYER_TRUECZ, 0, 0, -1 },
    { "player_par",             PLAYER_PLAYER_PAR, 0, 0, -1 },
    { "visibility",             PLAYER_VISIBILITY, 0, 0, -1 },
    { "bobcounter",             PLAYER_BOBCOUNTER, 0, 0, -1 },
    { "weapon_sway",            PLAYER_WEAPON_SWAY, 0, 0, -1 },
    { "pals_time",              PLAYER_PALS_TIME, 0, 0, -1 },
    { "randomflamex",           PLAYER_RANDOMFLAMEX, 0, 0, -1 },
    { "crack_time",             PLAYER_CRACK_TIME, 0, 0, -1 },
    { "aim_mode",               PLAYER_AIM_MODE, 0, 0, -1 },
    { "ang",                    PLAYER_ANG, 0, 0, -1 },
    { "oang",                   PLAYER_OANG, 0, 0, -1 },
    { "angvel",                 PLAYER_ANGVEL, 0, 0, -1 },
    { "cursectnum",             PLAYER_CURSECTNUM, 0, 0, -1 },
    { "look_ang",               PLAYER_LOOK_ANG, 0, 0, -1 },
    { "last_extra",             PLAYER_LAST_EXTRA, 0, 0, -1 },
    { "subweapon",              PLAYER_SUBWEAPON, 0, 0, -1 },
    { "ammo_amount",            PLAYER_AMMO_AMOUNT, LABEL_HASPARM2, MAX_WEAPONS, -1 },
    { "wackedbyactor",          PLAYER_WACKEDBYACTOR, 0, 0, -1 },
    { "frag",                   PLAYER_FRAG, 0, 0, -1 },
    { "fraggedself",            PLAYER_FRAGGEDSELF, 0, 0, -1 },
    { "curr_weapon",            PLAYER_CURR_WEAPON, 0, 0, -1 },
    { "last_weapon",            PLAYER_LAST_WEAPON, 0, 0, -1 },
    { "tipincs",                PLAYER_TIPINCS, 0, 0, -1 },
    { "horizoff",               PLAYER_HORIZOFF, 0, 0, -1 },
    { "wantweaponfire",         PLAYER_WANTWEAPONFIRE, 0, 0, -1 },
    { "holoduke_amount",        PLAYER_HOLODUKE_AMOUNT, 0, 0, -1 },
    { "newowner",               PLAYER_NEWOWNER, 0, 0, -1 },
    { "hurt_delay",             PLAYER_HURT_DELAY, 0, 0, -1 },
    { "hbomb_hold_delay",       PLAYER_HBOMB_HOLD_DELAY, 0, 0, -1 },
    { "jumping_counter",        PLAYER_JUMPING_COUNTER, 0, 0, -1 },
    { "airleft",                PLAYER_AIRLEFT, 0, 0, -1 },
    { "knee_incs",              PLAYER_KNEE_INCS, 0, 0, -1 },
    { "access_incs",            PLAYER_ACCESS_INCS, 0, 0, -1 },
    { "fta",                    PLAYER_FTA, 0, 0, -1 },
    { "ftq",                    PLAYER_FTQ, 0, 0, -1 },
    { "access_wallnum",         PLAYER_ACCESS_WALLNUM, 0, 0, -1 },
    { "access_spritenum",       PLAYER_ACCESS_SPRITENUM, 0, 0, -1 },
    { "kickback_pic",           PLAYER_KICKBACK_PIC, 0, 0, -1 },
    { "got_access",             PLAYER_GOT_ACCESS, 0, 0, -1 },
    { "weapon_ang",             PLAYER_WEAPON_ANG, 0, 0, -1 },
    { "firstaid_amount",        PLAYER_FIRSTAID_AMOUNT, 0, 0, -1 },
    { "somethingonplayer",      PLAYER_SOMETHINGONPLAYER, 0, 0, -1 },
    { "on_crane",               PLAYER_ON_CRANE, 0, 0, -1 },
    { "i",                      PLAYER_I, 0, 0, -1 },
    { "one_parallax_sectnum",   PLAYER_ONE_PARALLAX_SECTNUM, 0, 0, -1 },
    { "over_shoulder_on",       PLAYER_OVER_SHOULDER_ON, 0, 0, -1 },
    { "random_club_frame",      PLAYER_RANDOM_CLUB_FRAME, 0, 0, -1 },
    { "fist_incs",              PLAYER_FIST_INCS, 0, 0, -1 },
    { "one_eighty_count",       PLAYER_ONE_EIGHTY_COUNT, 0, 0, -1 },
    { "cheat_phase",            PLAYER_CHEAT_PHASE, 0, 0, -1 },
    { "dummyplayersprite",      PLAYER_DUMMYPLAYERSPRITE, 0, 0, -1 },
    { "extra_extra8",           PLAYER_EXTRA_EXTRA8, 0, 0, -1 },
    { "quick_kick",             PLAYER_QUICK_KICK, 0, 0, -1 },
    { "heat_amount",            PLAYER_HEAT_AMOUNT, 0, 0, -1 },
    { "actorsqu",               PLAYER_ACTORSQU, 0, 0, -1 },
    { "timebeforeexit",         PLAYER_TIMEBEFOREEXIT, 0, 0, -1 },
    { "customexitsound",        PLAYER_CUSTOMEXITSOUND, 0, 0, -1 },
    { "weaprecs",               PLAYER_WEAPRECS, LABEL_HASPARM2, MAX_WEAPONS, -1 },
    { "weapreccnt",             PLAYER_WEAPRECCNT, 0, 0, -1 },
    { "interface_toggle_flag",  PLAYER_INTERFACE_TOGGLE_FLAG, 0, 0, -1 },
    { "rotscrnang",             PLAYER_ROTSCRNANG, 0, 0, -1 },
    { "orotscrnang",            PLAYER_OROTSCRNANG, 0, 0, -1 },
    { "dead_flag",              PLAYER_DEAD_FLAG, 0, 0, -1 },
    { "show_empty_weapon",      PLAYER_SHOW_EMPTY_WEAPON, 0, 0, -1 },
    { "scuba_amount",           PLAYER_SCUBA_AMOUNT, 0, 0, -1 },
    { "jetpack_amount",         PLAYER_JETPACK_AMOUNT, 0, 0, -1 },
    { "steroids_amount",        PLAYER_STEROIDS_AMOUNT, 0, 0, -1 },
    { "shield_amount",          PLAYER_SHIELD_AMOUNT, 0, 0, -1 },
    { "holoduke_on",            PLAYER_HOLODUKE_ON, 0, 0, -1 },
    { "pycount",                PLAYER_PYCOUNT, 0, 0, -1 },
    { "weapon_pos",             PLAYER_WEAPON_POS, 0, 0, -1 },
    { "frag_ps",                PLAYER_FRAG_PS, 0, 0, -1 },
    { "transporter_hold",       PLAYER_TRANSPORTER_HOLD, 0, 0, -1 },
    { "last_full_weapon",       PLAYER_LAST_FULL_WEAPON, 0, 0, -1 },
    { "footprintshade",         PLAYER_FOOTPRINTSHADE, 0, 0, -1 },
    { "boot_amount",            PLAYER_BOOT_AMOUNT, 0, 0, -1 },
    { "scream_voice",           PLAYER_SCREAM_VOICE, 0, 0, -1 },
    { "gm",                     PLAYER_GM, 0, 0, -1 },
    { "on_warping_sector",      PLAYER_ON_WARPING_SECTOR, 0, 0, -1 },
    { "footprintcount",         PLAYER_FOOTPRINTCOUNT, 0, 0, -1 },
    { "hbomb_on",               PLAYER_HBOMB_ON, 0, 0, -1 },
    { "jumping_toggle",         PLAYER_JUMPING_TOGGLE, 0, 0, -1 },
    { "rapid_fire_hold",        PLAYER_RAPID_FIRE_HOLD, 0, 0, -1 },
    { "on_ground",              PLAYER_ON_GROUND, 0, 0, -1 },
    { "name",                   PLAYER_NAME,  LABEL_ISSTRING, 32, -1 },
    { "inven_icon",             PLAYER_INVEN_ICON, 0, 0, -1 },
    { "buttonpalette",          PLAYER_BUTTONPALETTE, 0, 0, -1 },
    { "jetpack_on",             PLAYER_JETPACK_ON, 0, 0, -1 },
    { "spritebridge",           PLAYER_SPRITEBRIDGE, 0, 0, -1 },
    { "lastrandomspot",         PLAYER_LASTRANDOMSPOT, 0, 0, -1 },
    { "scuba_on",               PLAYER_SCUBA_ON, 0, 0, -1 },
    { "footprintpal",           PLAYER_FOOTPRINTPAL, 0, 0, -1 },
    { "heat_on",                PLAYER_HEAT_ON, 0, 0, -1 },
    { "holster_weapon",         PLAYER_HOLSTER_WEAPON, 0, 0, -1 },
    { "falling_counter",        PLAYER_FALLING_COUNTER, 0, 0, -1 },
    { "gotweapon",              PLAYER_GOTWEAPON, LABEL_HASPARM2, MAX_WEAPONS, -1 },
    { "palette",                PLAYER_PALETTE, 0, 0, -1 },
    { "toggle_key_flag",        PLAYER_TOGGLE_KEY_FLAG, 0, 0, -1 },
    { "knuckle_incs",           PLAYER_KNUCKLE_INCS, 0, 0, -1 },
    { "walking_snd_toggle",     PLAYER_WALKING_SND_TOGGLE, 0, 0, -1 },
    { "palookup",               PLAYER_PALOOKUP, 0, 0, -1 },
    { "hard_landing",           PLAYER_HARD_LANDING, 0, 0, -1 },
    { "max_secret_rooms",       PLAYER_MAX_SECRET_ROOMS, 0, 0, -1 },
    { "secret_rooms",           PLAYER_SECRET_ROOMS, 0, 0, -1 },
    { "pals",                   PLAYER_PALS, LABEL_HASPARM2, 3, -1 },
    { "max_actors_killed",      PLAYER_MAX_ACTORS_KILLED, 0, 0, -1 },
    { "actors_killed",          PLAYER_ACTORS_KILLED, 0, 0, -1 },
    { "return_to_center",       PLAYER_RETURN_TO_CENTER, 0, 0, -1 },
    { "runspeed",               PLAYER_RUNSPEED, 0, 0, -1 },
    { "sbs",                    PLAYER_SBS, 0, 0, -1 },
    { "reloading",              PLAYER_RELOADING, 0, 0, -1 },
    { "auto_aim",               PLAYER_AUTO_AIM, 0, 0, -1 },
    { "movement_lock",          PLAYER_MOVEMENT_LOCK, 0, 0, -1 },
    { "sound_pitch",            PLAYER_SOUND_PITCH, 0, 0, -1 },
    { "weaponswitch",           PLAYER_WEAPONSWITCH, 0, 0, -1 },
    { "team",                   PLAYER_TEAM, 0, 0, -1 },
    { "max_player_health",      PLAYER_MAX_PLAYER_HEALTH, 0, 0, -1 },
    { "max_shield_amount",      PLAYER_MAX_SHIELD_AMOUNT, 0, 0, -1 },
    { "max_ammo_amount",        PLAYER_MAX_AMMO_AMOUNT, LABEL_HASPARM2, MAX_WEAPONS, -1 },
    { "last_quick_kick",        PLAYER_LAST_QUICK_KICK, 0, 0, -1 },
    { "connected",              PLAYER_CONNECTED, 0, 0, -1 },
    { "lowsprite",              PLAYER_LOWSPRITE, 0, 0, -1 },
    { "highsprite",             PLAYER_HIGHSPRITE, 0, 0, -1 },
    { "",                       -1, 0, 0, -1 } // END OF LIST
};

const memberlabel_t ProjectileLabels[] =
{
    { "workslike",  PROJ_WORKSLIKE,     0, 0, -1 },
    { "spawns",     PROJ_SPAWNS,        0, 0, -1 },
    { "sxrepeat",   PROJ_SXREPEAT,      0, 0, -1 },
    { "syrepeat",   PROJ_SYREPEAT,      0, 0, -1 },
    { "sound",      PROJ_SOUND,         0, 0, -1 },
    { "isound",     PROJ_ISOUND,        0, 0, -1 },
    { "vel",        PROJ_VEL,           0, 0, -1 },
    { "extra",      PROJ_EXTRA,         0, 0, -1 },
    { "decal",      PROJ_DECAL,         0, 0, -1 },
    { "trail",      PROJ_TRAIL,         0, 0, -1 },
    { "txrepeat",   PROJ_TXREPEAT,      0, 0, -1 },
    { "tyrepeat",   PROJ_TYREPEAT,      0, 0, -1 },
    { "toffset",    PROJ_TOFFSET,       0, 0, -1 },
    { "tnum",       PROJ_TNUM,          0, 0, -1 },
    { "drop",       PROJ_DROP,          0, 0, -1 },
    { "cstat",      PROJ_CSTAT,         0, 0, -1 },
    { "clipdist",   PROJ_CLIPDIST,      0, 0, -1 },
    { "shade",      PROJ_SHADE,         0, 0, -1 },
    { "xrepeat",    PROJ_XREPEAT,       0, 0, -1 },
    { "yrepeat",    PROJ_YREPEAT,       0, 0, -1 },
    { "pal",        PROJ_PAL,           0, 0, -1 },
    { "extra_rand", PROJ_EXTRA_RAND,    0, 0, -1 },
    { "hitradius",  PROJ_HITRADIUS,     0, 0, -1 },
    { "velmult",    PROJ_MOVECNT,       0, 0, -1 },
    { "offset",     PROJ_OFFSET,        0, 0, -1 },
    { "bounces",    PROJ_BOUNCES,       0, 0, -1 },
    { "bsound",     PROJ_BSOUND,        0, 0, -1 },
    { "range",      PROJ_RANGE,         0, 0, -1 },
    { "flashcolor", PROJ_FLASH_COLOR,   0, 0, -1 },
    { "",           -1,                 0, 0, -1 } // END OF LIST
};

const memberlabel_t UserdefsLabels[] =
{
    { "god",                    USERDEFS_GOD,                   0, 0, -1 },
    { "warp_on",                USERDEFS_WARP_ON,               0, 0, -1 },
    { "cashman",                USERDEFS_CASHMAN,               0, 0, -1 },
    { "eog",                    USERDEFS_EOG,                   0, 0, -1 },
    { "showallmap",             USERDEFS_SHOWALLMAP,            0, 0, -1 },
    { "show_help",              USERDEFS_SHOW_HELP,             0, 0, -1 },
    { "scrollmode",             USERDEFS_SCROLLMODE,            0, 0, -1 },
    { "clipping",               USERDEFS_CLIPPING,              0, 0, -1 },
    { "user_name",              USERDEFS_USER_NAME,             LABEL_HASPARM2, MAXPLAYERS, -1 },
    { "ridecule",               USERDEFS_RIDECULE,              LABEL_HASPARM2 | LABEL_ISSTRING, 10, -1 },
    { "savegame",               USERDEFS_SAVEGAME,              LABEL_HASPARM2 | LABEL_ISSTRING, 10, -1 },
    { "pwlockout",              USERDEFS_PWLOCKOUT,             LABEL_ISSTRING, 128, -1 },
    { "rtsname;",               USERDEFS_RTSNAME,               LABEL_ISSTRING, 128, -1 },
    { "overhead_on",            USERDEFS_OVERHEAD_ON,           0, 0, -1 },
    { "last_overhead",          USERDEFS_LAST_OVERHEAD,         0, 0, -1 },
    { "showweapons",            USERDEFS_SHOWWEAPONS,           0, 0, -1 },

    { "pause_on",               USERDEFS_PAUSE_ON,              0, 0, -1 },
    { "from_bonus",             USERDEFS_FROM_BONUS,            0, 0, -1 },
    { "camerasprite",           USERDEFS_CAMERASPRITE,          0, 0, -1 },
    { "last_camsprite",         USERDEFS_LAST_CAMSPRITE,        0, 0, -1 },
    { "last_level",             USERDEFS_LAST_LEVEL,            0, 0, -1 },
    { "secretlevel",            USERDEFS_SECRETLEVEL,           0, 0, -1 },

    { "const_visibility",       USERDEFS_CONST_VISIBILITY,      0, 0, -1 },
    { "uw_framerate",           USERDEFS_UW_FRAMERATE,          0, 0, -1 },
    { "camera_time",            USERDEFS_CAMERA_TIME,           0, 0, -1 },
    { "folfvel",                USERDEFS_FOLFVEL,               0, 0, -1 },
    { "folavel",                USERDEFS_FOLAVEL,               0, 0, -1 },
    { "folx",                   USERDEFS_FOLX,                  0, 0, -1 },
    { "foly",                   USERDEFS_FOLY,                  0, 0, -1 },
    { "fola",                   USERDEFS_FOLA,                  0, 0, -1 },
    { "reccnt",                 USERDEFS_RECCNT,                0, 0, -1 },

    { "entered_name",           USERDEFS_ENTERED_NAME,          0, 0, -1 },
    { "screen_tilting",         USERDEFS_SCREEN_TILTING,        0, 0, -1 },
    { "shadows",                USERDEFS_SHADOWS,               0, 0, -1 },
    { "fta_on",                 USERDEFS_FTA_ON,                0, 0, -1 },
    { "executions",             USERDEFS_EXECUTIONS,            0, 0, -1 },
    { "auto_run",               USERDEFS_AUTO_RUN,              0, 0, -1 },
    { "coords",                 USERDEFS_COORDS,                0, 0, -1 },
    { "tickrate",               USERDEFS_TICKRATE,              0, 0, -1 },
    { "m_coop",                 USERDEFS_M_COOP,                0, 0, -1 },
    { "coop",                   USERDEFS_COOP,                  0, 0, -1 },
    { "screen_size",            USERDEFS_SCREEN_SIZE,           0, 0, -1 },
    { "lockout",                USERDEFS_LOCKOUT,               0, 0, -1 },
    { "crosshair",              USERDEFS_CROSSHAIR,             0, 0, -1 },

    { "playerai",               USERDEFS_PLAYERAI,              0, 0, -1 },
    { "respawn_monsters",       USERDEFS_RESPAWN_MONSTERS,      0, 0, -1 },
    { "respawn_items",          USERDEFS_RESPAWN_ITEMS,         0, 0, -1 },
    { "respawn_inventory",      USERDEFS_RESPAWN_INVENTORY,     0, 0, -1 },
    { "recstat",                USERDEFS_RECSTAT,               0, 0, -1 },
    { "monsters_off",           USERDEFS_MONSTERS_OFF,          0, 0, -1 },
    { "brightness",             USERDEFS_BRIGHTNESS,            0, 0, -1 },
    { "m_respawn_items",        USERDEFS_M_RESPAWN_ITEMS,       0, 0, -1 },
    { "m_respawn_monsters",     USERDEFS_M_RESPAWN_MONSTERS,    0, 0, -1 },
    { "m_respawn_inventory",    USERDEFS_M_RESPAWN_INVENTORY,   0, 0, -1 },
    { "m_recstat",              USERDEFS_M_RECSTAT,             0, 0, -1 },
    { "m_monsters_off",         USERDEFS_M_MONSTERS_OFF,        0, 0, -1 },
    { "detail",                 USERDEFS_DETAIL,                0, 0, -1 },
    { "m_ffire",                USERDEFS_M_FFIRE,               0, 0, -1 },
    { "ffire",                  USERDEFS_FFIRE,                 0, 0, -1 },
    { "m_player_skill",         USERDEFS_M_PLAYER_SKILL,        0, 0, -1 },
    { "m_level_number",         USERDEFS_M_LEVEL_NUMBER,        0, 0, -1 },
    { "m_volume_number",        USERDEFS_M_VOLUME_NUMBER,       0, 0, -1 },
    { "multimode",              USERDEFS_MULTIMODE,             0, 0, -1 },
    { "player_skill",           USERDEFS_PLAYER_SKILL,          0, 0, -1 },
    { "level_number",           USERDEFS_LEVEL_NUMBER,          0, 0, -1 },
    { "volume_number",          USERDEFS_VOLUME_NUMBER,         0, 0, -1 },
    { "user_map",               USERDEFS_USER_MAP,              0, 0, -1 },
    { "m_user_map",             USERDEFS_M_USER_MAP,            0, 0, -1 },
    { "m_marker",               USERDEFS_M_MARKER,              0, 0, -1 },
    { "marker",                 USERDEFS_MARKER,                0, 0, -1 },
    { "mouseflip",              USERDEFS_MOUSEFLIP,             0, 0, -1 },
    { "statusbarscale",         USERDEFS_STATUSBARSCALE,        0, 0, -1 },
    { "drawweapon",             USERDEFS_DRAWWEAPON,            0, 0, -1 },
    { "mouseaiming",            USERDEFS_MOUSEAIMING,           0, 0, -1 },
    { "weaponswitch",           USERDEFS_WEAPONSWITCH,          0, 0, -1 },
    { "democams",               USERDEFS_DEMOCAMS,              0, 0, -1 },
    { "color",                  USERDEFS_COLOR,                 0, 0, -1 },
    { "msgdisptime",            USERDEFS_MSGDISPTIME,           0, 0, -1 },
    { "statusbarmode",          USERDEFS_STATUSBARMODE,         0, 0, -1 },
    { "m_noexits",              USERDEFS_M_NOEXITS,             0, 0, -1 },
    { "noexits",                USERDEFS_NOEXITS,               0, 0, -1 },
    { "autovote",               USERDEFS_AUTOVOTE,              0, 0, -1 },
    { "automsg",                USERDEFS_AUTOMSG,               0, 0, -1 },
    { "idplayers",              USERDEFS_IDPLAYERS,             0, 0, -1 },
    { "team",                   USERDEFS_TEAM,                  0, 0, -1 },
    { "viewbob",                USERDEFS_VIEWBOB,               0, 0, -1 },
    { "weaponsway",             USERDEFS_WEAPONSWAY,            0, 0, -1 },
    { "angleinterpolation",     USERDEFS_ANGLEINTERPOLATION,    0, 0, -1 },
    { "obituaries",             USERDEFS_OBITUARIES,            0, 0, -1 },
    { "levelstats",             USERDEFS_LEVELSTATS,            0, 0, -1 },
    { "crosshairscale",         USERDEFS_CROSSHAIRSCALE,        0, 0, -1 },
    { "althud",                 USERDEFS_ALTHUD,                0, 0, -1 },
    { "display_bonus_screen",   USERDEFS_DISPLAY_BONUS_SCREEN,  0, 0, -1 },
    { "show_level_text",        USERDEFS_SHOW_LEVEL_TEXT,       0, 0, -1 },
    { "weaponscale",            USERDEFS_WEAPONSCALE,           0, 0, -1 },
    { "textscale",              USERDEFS_TEXTSCALE,             0, 0, -1 },

    { "return",                 USERDEFS_RETURN,                LABEL_HASPARM2, MAX_RETURN_VALUES, -1 },
    { "vm_player",              USERDEFS_VM_PLAYER,             0, 0, -1 },
    { "vm_sprite",              USERDEFS_VM_SPRITE,             0, 0, -1 },
    { "vm_distance",            USERDEFS_VM_DISTANCE,           0, 0, -1 },

    { "teampal",                USERDEFS_TEAMPAL,               LABEL_HASPARM2, MAXPLAYERS, -1 },
    { "teamfrags",              USERDEFS_TEAMFRAGS,             LABEL_HASPARM2, MAXPLAYERS, -1 },
    { "teamsuicides",           USERDEFS_TEAMSUICIDES,          LABEL_HASPARM2, MAXPLAYERS, -1 },
    { "dmflags",                USERDEFS_DMFLAGS,               0, 0, -1 },
    { "m_dmflags",              USERDEFS_M_DMFLAGS,             0, 0, -1 },
    { "draw_yxaspect",          USERDEFS_DRAW_YXASPECT,         0, 0, -1 },
    { "monsters_killed",        USERDEFS_MONSTERS_KILLED,       0, 0, -1 },
    { "limit_hit",              USERDEFS_LIMIT_HIT,             0, 0, -1 },
    { "winner",                 USERDEFS_WINNER,                0, 0, -1 },
    { "",                       -1,                             0, 0, -1 } // END OF LIST
};

const memberlabel_t InputLabels[] =
{
    { "avel",       INPUT_AVEL,     0, 0, -1 },
    { "horz",       INPUT_HORZ,     0, 0, -1 },
    { "fvel",       INPUT_FVEL,     0, 0, -1 },
    { "svel",       INPUT_SVEL,     0, 0, -1 },
    { "bits",       INPUT_BITS,     0, 0, -1 },
    { "extbits",    INPUT_EXTBITS,  0, 0, -1 },
    { "",           -1,             0, 0, -1 } // END OF LIST
};

const memberlabel_t TileDataLabels[] =
{
    // tilesiz[]
    { "xsize",      TILEDATA_XSIZE,      0, 0, -1 },
    { "ysize",      TILEDATA_YSIZE,      0, 0, -1 },

    // picanm[]
    { "animframes", TILEDATA_ANIMFRAMES, 0, 0, -1 },
    { "xoffset",    TILEDATA_XOFFSET,    0, 0, -1 },
    { "yoffset",    TILEDATA_YOFFSET,    0, 0, -1 },
    { "animspeed",  TILEDATA_ANIMSPEED,  0, 0, -1 },
    { "animtype",   TILEDATA_ANIMTYPE,   0, 0, -1 },

    // g_tile[]
    { "gameflags",  TILEDATA_GAMEFLAGS,  0, 0, -1 },
    { "",           -1,                  0, 0, -1 } // END OF LIST
};
#undef MEMBER
#undef LABEL

hashtable_t h_actor         = { ACTOR_END >> 1, NULL };
hashtable_t h_input         = { INPUT_END >> 1, NULL };
//hashtable_t h_paldata    = { PALDATA_END>>1, NULL };
hashtable_t h_player        = { PLAYER_END >> 1, NULL };
hashtable_t h_projectile    = { PROJ_END >> 1, NULL };
hashtable_t h_sector        = { SECTOR_END >> 1, NULL };
hashtable_t h_tiledata      = { TILEDATA_END >> 1, NULL };
hashtable_t h_tsprite       = { ACTOR_END >> 1, NULL };
hashtable_t h_userdef       = { USERDEFS_END >> 1, NULL };
hashtable_t h_wall          = { WALL_END >> 1, NULL };

#define STRUCT_HASH_SETUP(table, labels)                 \
    do                                                   \
    {                                                    \
        for (int i = 0; i < ARRAY_SSIZE(labels); i++)    \
            hash_add(&table, labels[i].name, i, 0);      \
        EDUKE32_STATIC_ASSERT(ARRAY_SSIZE(labels) != 0); \
    } while (0)

void VM_InitHashTables(void)
{
    for (auto table : vmStructHashTablePtrs)
        hash_init(table);

    inithashnames();

    STRUCT_HASH_SETUP(h_actor, ActorLabels);
    STRUCT_HASH_SETUP(h_input, InputLabels);
    //STRUCT_HASH_SETUP(h_paldata, PalDataLabels);
    STRUCT_HASH_SETUP(h_player, PlayerLabels);
    STRUCT_HASH_SETUP(h_projectile, ProjectileLabels);
    STRUCT_HASH_SETUP(h_sector, SectorLabels);
    STRUCT_HASH_SETUP(h_tiledata, TileDataLabels);
    STRUCT_HASH_SETUP(h_tsprite, TsprLabels);
    STRUCT_HASH_SETUP(h_userdef, UserdefsLabels);
    STRUCT_HASH_SETUP(h_wall, WallLabels);
}
#undef STRUCT_HASH_SETUP

// this is all the crap for accessing the game's structs through the CON VM
// I got a 3-4 fps gain by inlining these...

int32_t __fastcall VM_GetTileData(int const tileNum, int32_t labelNum)
{
    if (EDUKE32_PREDICT_FALSE((unsigned)tileNum >= MAXTILES))
    {
        CON_ERRPRINTF("invalid tile %d\n", tileNum);
        return -1;
    }

    auto const& tileDataLabel = TileDataLabels[labelNum];
    auto const& p = picanm[tileNum];

    switch (tileDataLabel.lId)
    {
        case TILEDATA_XSIZE:        return tilesiz[tileNum].x;
        case TILEDATA_YSIZE:        return tilesiz[tileNum].y;

        case TILEDATA_GAMEFLAGS:    return g_tile[tileNum].flags;

        case TILEDATA_ANIMFRAMES:   return p.num;
        case TILEDATA_XOFFSET:      return p.xofs;
        case TILEDATA_YOFFSET:      return p.yofs;

        case TILEDATA_ANIMSPEED:    return (p.sf & PICANM_ANIMSPEED_MASK);
        case TILEDATA_ANIMTYPE:     return (p.sf & PICANM_ANIMTYPE_MASK) >> PICANM_ANIMTYPE_SHIFT;

        default: EDUKE32_UNREACHABLE_SECTION(return -1);
    }
}

void __fastcall VM_SetTileData(int const tileNum, int const labelNum, int32_t newValue)
{
    if (EDUKE32_PREDICT_FALSE((unsigned)tileNum >= MAXTILES))
    {
        CON_ERRPRINTF("invalid tile %d\n", tileNum);
        return;
    }

    auto const& tileDataLabel = TileDataLabels[labelNum];
    auto& p = picanm[tileNum];

    switch (tileDataLabel.lId)
    {
        //case TILEDATA_XSIZE: tilesiz[tileNum].x = newValue; break;
        //case TILEDATA_YSIZE: tilesiz[tileNum].y = newValue; break;

        case TILEDATA_GAMEFLAGS: g_tile[tileNum].flags = newValue; return;

        case TILEDATA_ANIMFRAMES: p.num = newValue; return;
        case TILEDATA_XOFFSET:    p.xofs = newValue; return;
        case TILEDATA_YOFFSET:    p.yofs = newValue; return;

        case TILEDATA_ANIMSPEED: p.sf = (p.sf & ~PICANM_ANIMSPEED_MASK) | (newValue & PICANM_ANIMSPEED_MASK); return;
        case TILEDATA_ANIMTYPE:  p.sf = (p.sf & ~PICANM_ANIMTYPE_MASK) | ((newValue << PICANM_ANIMTYPE_SHIFT) & PICANM_ANIMTYPE_MASK); return;
        default: return;
    }
}

int32_t __fastcall VM_GetUserdef(int32_t labelNum, int const lParm2)
{
    auto const& udLabel = UserdefsLabels[labelNum];
    if (EDUKE32_PREDICT_FALSE(udLabel.flags & LABEL_HASPARM2 && (unsigned)lParm2 >= (unsigned)udLabel.maxParm2))
    {
        CON_ERRPRINTF("%s[%d] invalid for userdef", udLabel.name, lParm2);
        return -1;
    }

    switch (udLabel.lId)
    {
        case USERDEFS_GOD:                  return ud.god;
        case USERDEFS_WARP_ON:              return ud.warp_on;
        case USERDEFS_CASHMAN:              return ud.cashman;
        case USERDEFS_EOG:                  return ud.eog;
        case USERDEFS_SHOWALLMAP:           return ud.showallmap;
        case USERDEFS_SHOW_HELP:            return ud.show_help;
        case USERDEFS_SCROLLMODE:           return ud.scrollmode;
        case USERDEFS_CLIPPING:             return ud.noclip;
        case USERDEFS_OVERHEAD_ON:          return ud.overhead_on;
        case USERDEFS_LAST_OVERHEAD:        return ud.last_overhead;
        case USERDEFS_SHOWWEAPONS:          return ud.showweapons;
        case USERDEFS_PAUSE_ON:             return ud.pause_on;
        case USERDEFS_FROM_BONUS:           return ud.from_bonus;
        case USERDEFS_CAMERASPRITE:         return ud.camerasprite;
        case USERDEFS_LAST_CAMSPRITE:       return ud.last_camsprite;
        case USERDEFS_LAST_LEVEL:           return ud.last_level;
        case USERDEFS_SECRETLEVEL:          return ud.secretlevel;
        case USERDEFS_CONST_VISIBILITY:     return ud.const_visibility;
        case USERDEFS_UW_FRAMERATE:         return ud.uw_framerate;
        case USERDEFS_CAMERA_TIME:          return ud.camera_time;
        case USERDEFS_FOLFVEL:              return ud.folfvel;
        case USERDEFS_FOLAVEL:              return ud.folavel;
        case USERDEFS_FOLX:                 return ud.folx;
        case USERDEFS_FOLY:                 return ud.foly;
        case USERDEFS_FOLA:                 return ud.fola;
        case USERDEFS_RECCNT:               return ud.reccnt;
        case USERDEFS_ENTERED_NAME:         return ud.entered_name;
        case USERDEFS_SCREEN_TILTING:       return ud.screen_tilting;
        case USERDEFS_SHADOWS:              return ud.shadows;
        case USERDEFS_FTA_ON:               return ud.fta_on;
        case USERDEFS_EXECUTIONS:           return ud.executions;
        case USERDEFS_AUTO_RUN:             return ud.auto_run;
        case USERDEFS_COORDS:               return ud.coords;
        case USERDEFS_TICKRATE:             return ud.showfps;
        case USERDEFS_M_COOP:               return ud.m_coop;
        case USERDEFS_COOP:                 return ud.coop;
        case USERDEFS_SCREEN_SIZE:          return ud.screen_size;
        case USERDEFS_LOCKOUT:              return ud.lockout;
        case USERDEFS_CROSSHAIR:            return ud.crosshair;
        case USERDEFS_PLAYERAI:             return ud.playerai;

        case USERDEFS_RESPAWN_MONSTERS:     return DMFLAGS_TEST(DMFLAG_RESPAWNMONSTERS);
        case USERDEFS_RESPAWN_ITEMS:        return DMFLAGS_TEST(DMFLAG_RESPAWNITEMS);
        case USERDEFS_RESPAWN_INVENTORY:    return DMFLAGS_TEST(DMFLAG_RESPAWNINVENTORY);

        case USERDEFS_RECSTAT:              return ud.recstat;

        case USERDEFS_MONSTERS_OFF:         return DMFLAGS_TEST(DMFLAG_NOMONSTERS);

        case USERDEFS_BRIGHTNESS:           return ud.brightness;

        case USERDEFS_M_RESPAWN_ITEMS:      return M_DMFLAGS_TEST(DMFLAG_RESPAWNITEMS);
        case USERDEFS_M_RESPAWN_MONSTERS:   return M_DMFLAGS_TEST(DMFLAG_RESPAWNMONSTERS);
        case USERDEFS_M_RESPAWN_INVENTORY:  return M_DMFLAGS_TEST(DMFLAG_RESPAWNINVENTORY);

        case USERDEFS_M_RECSTAT:            return ud.m_recstat;

        case USERDEFS_M_MONSTERS_OFF:       return M_DMFLAGS_TEST(DMFLAG_NOMONSTERS);

        case USERDEFS_DETAIL:               return ud.detail;

        case USERDEFS_M_FFIRE:              return M_DMFLAGS_TEST(DMFLAG_FRIENDLYFIRE);
        case USERDEFS_FFIRE:                return DMFLAGS_TEST(DMFLAG_FRIENDLYFIRE);

        case USERDEFS_M_PLAYER_SKILL:       return ud.m_player_skill;
        case USERDEFS_M_LEVEL_NUMBER:       return ud.m_level_number;
        case USERDEFS_M_VOLUME_NUMBER:      return ud.m_volume_number;
        case USERDEFS_MULTIMODE:            return ud.multimode;
        case USERDEFS_PLAYER_SKILL:         return ud.player_skill;
        case USERDEFS_LEVEL_NUMBER:         return ud.level_number;
        case USERDEFS_VOLUME_NUMBER:        return ud.volume_number;

        case USERDEFS_USER_MAP:             return G_HaveUserMap();
        case USERDEFS_M_USER_MAP:           return Menu_HaveUserMap();

        case USERDEFS_M_MARKER:             return M_DMFLAGS_TEST(DMFLAG_MARKERS);
        case USERDEFS_MARKER:               return DMFLAGS_TEST(DMFLAG_MARKERS);

        case USERDEFS_MOUSEFLIP:            return ud.mouseflip;
        case USERDEFS_STATUSBARSCALE:       return ud.statusbarscale;
        case USERDEFS_DRAWWEAPON:           return ud.drawweapon;
        case USERDEFS_MOUSEAIMING:          return ud.mouseaiming;
        case USERDEFS_WEAPONSWITCH:         return ud.weaponswitch;
        case USERDEFS_DEMOCAMS:             return ud.democams;
        case USERDEFS_COLOR:                return ud.color;
        case USERDEFS_MSGDISPTIME:          return ud.msgdisptime;
        case USERDEFS_STATUSBARMODE:        return ud.statusbarmode;

        case USERDEFS_M_NOEXITS:            return M_DMFLAGS_TEST(DMFLAG_NOEXITS);
        case USERDEFS_NOEXITS:              return DMFLAGS_TEST(DMFLAG_NOEXITS);

        case USERDEFS_AUTOVOTE:             return ud.autovote;
        case USERDEFS_AUTOMSG:              return ud.automsg;
        case USERDEFS_IDPLAYERS:            return ud.idplayers;
        case USERDEFS_TEAM:                 return ud.team;
        case USERDEFS_VIEWBOB:              return ud.viewbob;
        case USERDEFS_WEAPONSWAY:           return ud.weaponsway;
        case USERDEFS_ANGLEINTERPOLATION:   return ud.angleinterpolation;
        case USERDEFS_OBITUARIES:           return ud.obituaries;
        case USERDEFS_LEVELSTATS:           return ud.levelstats;
        case USERDEFS_CROSSHAIRSCALE:       return ud.crosshairscale;
        case USERDEFS_ALTHUD:               return ud.althud;
        case USERDEFS_DISPLAY_BONUS_SCREEN: return ud.display_bonus_screen;
        case USERDEFS_SHOW_LEVEL_TEXT:      return ud.show_level_text;
        case USERDEFS_WEAPONSCALE:          return ud.weaponscale;
        case USERDEFS_TEXTSCALE:            return ud.textscale;

        case USERDEFS_RETURN:
        {
            if (lParm2 == 0)
                return Gv_GetVarX(sysVarIDs.RETURN);
            else
                return ud.returnvar[lParm2 - 1];
        }
        case USERDEFS_VM_PLAYER:            return vm.playerNum;
        case USERDEFS_VM_SPRITE:            return vm.spriteNum;
        case USERDEFS_VM_DISTANCE:          return vm.playerDist;

        case USERDEFS_TEAMPAL:              return teams[lParm2].palnum;
        case USERDEFS_TEAMFRAGS:            return teams[lParm2].frags;
        case USERDEFS_TEAMSUICIDES:         return teams[lParm2].suicides;
        case USERDEFS_DMFLAGS:              return ud.dmflags;
        case USERDEFS_M_DMFLAGS:            return ud.m_dmflags;
        case USERDEFS_DRAW_YXASPECT:        return rotatesprite_yxaspect;
        case USERDEFS_MONSTERS_KILLED:      return ud.monsters_killed;
        case USERDEFS_LIMIT_HIT:            return ud.limit_hit;
        case USERDEFS_WINNER:               return ud.winner;

        default: EDUKE32_UNREACHABLE_SECTION(return -1);
    }
}

void __fastcall VM_SetUserdef(int const labelNum, int const lParm2, int32_t const newValue)
{
    auto const& udLabel = UserdefsLabels[labelNum];

    if (EDUKE32_PREDICT_FALSE(udLabel.flags & LABEL_HASPARM2 && (unsigned)lParm2 >= (unsigned)udLabel.maxParm2))
    {
        CON_ERRPRINTF("%s[%d] invalid for userdef", udLabel.name, lParm2);
        return;
    }

    switch (udLabel.lId)
    {
        case USERDEFS_GOD:                  ud.god              = newValue; return;
        case USERDEFS_WARP_ON:              ud.warp_on          = newValue; return;
        case USERDEFS_CASHMAN:              ud.cashman          = newValue; return;
        case USERDEFS_EOG:                  ud.eog              = newValue; return;
        case USERDEFS_SHOWALLMAP:           ud.showallmap       = newValue; return;
        case USERDEFS_SHOW_HELP:            ud.show_help        = newValue; return;
        case USERDEFS_SCROLLMODE:           ud.scrollmode       = newValue; return;
        case USERDEFS_CLIPPING:             ud.noclip           = newValue; return;
        case USERDEFS_OVERHEAD_ON:          ud.overhead_on      = newValue; return;
        case USERDEFS_LAST_OVERHEAD:        ud.last_overhead    = newValue; return;
        case USERDEFS_SHOWWEAPONS:          ud.showweapons      = newValue; return;
        case USERDEFS_PAUSE_ON:             ud.pause_on         = newValue; return;
        case USERDEFS_FROM_BONUS:           ud.from_bonus       = newValue; return;
        case USERDEFS_CAMERASPRITE:         ud.camerasprite     = newValue; return;
        case USERDEFS_LAST_CAMSPRITE:       ud.last_camsprite   = newValue; return;
        case USERDEFS_LAST_LEVEL:           ud.last_level       = newValue; return;
        case USERDEFS_SECRETLEVEL:          ud.secretlevel      = newValue; return;
        case USERDEFS_CONST_VISIBILITY:     ud.const_visibility = newValue; return;
        case USERDEFS_UW_FRAMERATE:         ud.uw_framerate     = newValue; return;
        case USERDEFS_CAMERA_TIME:          ud.camera_time      = newValue; return;
        case USERDEFS_FOLFVEL:              ud.folfvel          = newValue; return;
        case USERDEFS_FOLAVEL:              ud.folavel          = newValue; return;
        case USERDEFS_FOLX:                 ud.folx             = newValue; return;
        case USERDEFS_FOLY:                 ud.foly             = newValue; return;
        case USERDEFS_FOLA:                 ud.fola             = newValue; return;
        case USERDEFS_RECCNT:               ud.reccnt           = newValue; return;
        case USERDEFS_ENTERED_NAME:         ud.entered_name     = newValue; return;
        case USERDEFS_SCREEN_TILTING:       ud.screen_tilting   = newValue; return;
        case USERDEFS_SHADOWS:              ud.shadows          = newValue; return;
        case USERDEFS_FTA_ON:               ud.fta_on           = newValue; return;
        case USERDEFS_EXECUTIONS:           ud.executions       = newValue; return;
        case USERDEFS_AUTO_RUN:             ud.auto_run         = newValue; return;
        case USERDEFS_COORDS:               ud.coords           = newValue; return;
        case USERDEFS_TICKRATE:             ud.showfps          = newValue; return;
        case USERDEFS_M_COOP:               ud.m_coop           = newValue; return;
        case USERDEFS_COOP:                 ud.coop             = newValue; return;

        case USERDEFS_SCREEN_SIZE:
        {
            if (ud.screen_size != newValue)
            {
                ud.screen_size = newValue;
                G_UpdateScreenArea();
            }
            return;
        }

        case USERDEFS_LOCKOUT:              ud.lockout          = newValue; return;
        case USERDEFS_CROSSHAIR:            ud.crosshair        = newValue; return;
        case USERDEFS_PLAYERAI:             ud.playerai         = newValue; return;

        case USERDEFS_RESPAWN_MONSTERS:     DMFLAGS_SET(DMFLAG_RESPAWNMONSTERS, newValue); return;
        case USERDEFS_RESPAWN_ITEMS:        DMFLAGS_SET(DMFLAG_RESPAWNITEMS, newValue); return;
        case USERDEFS_RESPAWN_INVENTORY:    DMFLAGS_SET(DMFLAG_RESPAWNINVENTORY, newValue); return;

        case USERDEFS_RECSTAT:              ud.recstat          = newValue; return;

        case USERDEFS_MONSTERS_OFF:         DMFLAGS_SET(DMFLAG_NOMONSTERS, newValue); return;

        case USERDEFS_BRIGHTNESS:           ud.brightness       = newValue; return;

        case USERDEFS_M_RESPAWN_ITEMS:      M_DMFLAGS_SET(DMFLAG_RESPAWNITEMS, newValue); return;
        case USERDEFS_M_RESPAWN_MONSTERS:   M_DMFLAGS_SET(DMFLAG_RESPAWNMONSTERS, newValue); return;
        case USERDEFS_M_RESPAWN_INVENTORY:  M_DMFLAGS_SET(DMFLAG_RESPAWNINVENTORY, newValue); return;

        case USERDEFS_M_RECSTAT:            ud.m_recstat        = newValue; return;

        case USERDEFS_M_MONSTERS_OFF:       M_DMFLAGS_SET(DMFLAG_NOMONSTERS, newValue); return;

        case USERDEFS_DETAIL:               ud.detail           = newValue; return;

        case USERDEFS_M_FFIRE:              M_DMFLAGS_SET(DMFLAG_FRIENDLYFIRE, newValue); return;
        case USERDEFS_FFIRE:                DMFLAGS_SET(DMFLAG_FRIENDLYFIRE, newValue); return;

        case USERDEFS_M_PLAYER_SKILL:       ud.m_player_skill   = newValue; return;
        case USERDEFS_M_LEVEL_NUMBER:       ud.m_level_number   = newValue; return;
        case USERDEFS_M_VOLUME_NUMBER:      ud.m_volume_number  = newValue; return;
        case USERDEFS_MULTIMODE:            ud.multimode        = newValue; return;
        case USERDEFS_PLAYER_SKILL:         ud.player_skill     = newValue; return;
        case USERDEFS_LEVEL_NUMBER:         ud.level_number     = newValue; return;
        case USERDEFS_VOLUME_NUMBER:        ud.volume_number    = newValue; return;

        //case USERDEFS_USER_MAP:
        //case USERDEFS_M_USER_MAP:

        case USERDEFS_M_MARKER:             M_DMFLAGS_SET(DMFLAG_MARKERS, newValue); return;
        case USERDEFS_MARKER:               DMFLAGS_SET(DMFLAG_MARKERS, newValue); return;

        case USERDEFS_MOUSEFLIP:            ud.mouseflip        = newValue; return;
        case USERDEFS_STATUSBARSCALE:       ud.statusbarscale   = newValue; return;
        case USERDEFS_DRAWWEAPON:           ud.drawweapon       = newValue; return;
        case USERDEFS_MOUSEAIMING:          ud.mouseaiming      = newValue; return;
        case USERDEFS_WEAPONSWITCH:         ud.weaponswitch     = newValue; return;
        case USERDEFS_DEMOCAMS:             ud.democams         = newValue; return;
        case USERDEFS_COLOR:                ud.color            = newValue; return;
        case USERDEFS_MSGDISPTIME:          ud.msgdisptime      = newValue; return;
        case USERDEFS_STATUSBARMODE:        ud.statusbarmode    = newValue; return;

        case USERDEFS_M_NOEXITS:            M_DMFLAGS_SET(DMFLAG_NOEXITS, newValue); return;
        case USERDEFS_NOEXITS:              DMFLAGS_SET(DMFLAG_NOEXITS, newValue); return;

        case USERDEFS_AUTOVOTE:             ud.autovote             = newValue; return;
        case USERDEFS_AUTOMSG:              ud.automsg              = newValue; return;
        case USERDEFS_IDPLAYERS:            ud.idplayers            = newValue; return;
        case USERDEFS_TEAM:                 ud.team                 = newValue; return;
        case USERDEFS_VIEWBOB:              ud.viewbob              = newValue; return;
        case USERDEFS_WEAPONSWAY:           ud.weaponsway           = newValue; return;
        case USERDEFS_ANGLEINTERPOLATION:   ud.angleinterpolation   = newValue; return;
        case USERDEFS_OBITUARIES:           ud.obituaries           = newValue; return;
        case USERDEFS_LEVELSTATS:           ud.levelstats           = newValue; return;
        case USERDEFS_CROSSHAIRSCALE:       ud.crosshairscale       = newValue; return;
        case USERDEFS_ALTHUD:               ud.althud               = newValue; return;
        case USERDEFS_DISPLAY_BONUS_SCREEN: ud.display_bonus_screen = newValue; return;
        case USERDEFS_SHOW_LEVEL_TEXT:      ud.show_level_text      = newValue; return;
        case USERDEFS_WEAPONSCALE:          ud.weaponscale          = newValue; return;
        case USERDEFS_TEXTSCALE:            ud.textscale            = newValue; return;

        case USERDEFS_RETURN:
        {
            if (lParm2 == 0)
                Gv_SetVarX(sysVarIDs.RETURN, newValue);
            else
                ud.returnvar[lParm2 - 1] = newValue;
            return;
        }
        case USERDEFS_VM_PLAYER:            vm.playerNum = newValue; return; //vm.playerNum;
        case USERDEFS_VM_SPRITE:            vm.spriteNum = newValue; return; //vm.spriteNum;
        case USERDEFS_VM_DISTANCE:          vm.playerDist = newValue; return; //vm.playerDist;

        case USERDEFS_TEAMPAL:          teams[lParm2].palnum    = newValue; return;
        case USERDEFS_TEAMFRAGS:        teams[lParm2].frags     = newValue; return;
        case USERDEFS_TEAMSUICIDES:     teams[lParm2].suicides  = newValue; return;
        case USERDEFS_DMFLAGS:          ud.dmflags              = newValue; return;
        case USERDEFS_M_DMFLAGS:        ud.m_dmflags            = newValue; return;
        case USERDEFS_DRAW_YXASPECT:    rotatesprite_yxaspect   = newValue; return;
        case USERDEFS_MONSTERS_KILLED:  ud.monsters_killed      = newValue; return;
        case USERDEFS_LIMIT_HIT:        ud.limit_hit            = newValue; return;
        case USERDEFS_WINNER:           ud.winner               = newValue; return;

        default: return;
    }
}

int32_t __fastcall VM_GetActiveProjectile(int const spriteNum, int32_t labelNum)
{
    auto const& projLabel = ProjectileLabels[labelNum];

    if (EDUKE32_PREDICT_FALSE((unsigned)spriteNum >= MAXSPRITES))
    {
        CON_ERRPRINTF("%s invalid for projectile %d\n", projLabel.name, spriteNum);
        return -1;
    }

    auto const& proj = actor[spriteNum].projectile;
    switch (projLabel.lId)
    {
        case PROJ_WORKSLIKE:    return proj.workslike;
        case PROJ_SPAWNS:       return proj.spawns;
        case PROJ_SXREPEAT:     return proj.sxrepeat;
        case PROJ_SYREPEAT:     return proj.syrepeat;
        case PROJ_SOUND:        return proj.sound;
        case PROJ_ISOUND:       return proj.isound;
        case PROJ_VEL:          return proj.vel;
        case PROJ_EXTRA:        return proj.extra;
        case PROJ_DECAL:        return proj.decal;
        case PROJ_TRAIL:        return proj.trail;
        case PROJ_TXREPEAT:     return proj.txrepeat;
        case PROJ_TYREPEAT:     return proj.tyrepeat;
        case PROJ_TOFFSET:      return proj.toffset;
        case PROJ_TNUM:         return proj.tnum;
        case PROJ_DROP:         return proj.drop;
        case PROJ_CSTAT:        return proj.cstat;
        case PROJ_CLIPDIST:     return proj.clipdist;
        case PROJ_SHADE:        return proj.shade;
        case PROJ_XREPEAT:      return proj.xrepeat;
        case PROJ_YREPEAT:      return proj.yrepeat;
        case PROJ_PAL:          return proj.pal;
        case PROJ_EXTRA_RAND:   return proj.extra_rand;
        case PROJ_HITRADIUS:    return proj.hitradius;
        case PROJ_MOVECNT:      return proj.movecnt;
        case PROJ_OFFSET:       return proj.offset;
        case PROJ_BOUNCES:      return proj.bounces;
        case PROJ_BSOUND:       return proj.bsound;
        case PROJ_RANGE:        return proj.range;
        default: EDUKE32_UNREACHABLE_SECTION(return -1);
    }
}

void __fastcall VM_SetActiveProjectile(int const spriteNum, int const labelNum, int32_t const newValue)
{
    auto const& projLabel = ProjectileLabels[labelNum];

    if (EDUKE32_PREDICT_FALSE((unsigned)spriteNum >= MAXSPRITES))
    {
        CON_ERRPRINTF("%s invalid for projectile %d\n", projLabel.name, spriteNum);
        return;
    }

    auto& proj = actor[spriteNum].projectile;
    switch (projLabel.lId)
    {
        case PROJ_WORKSLIKE:    proj.workslike  = newValue; return;
        case PROJ_SPAWNS:       proj.spawns     = newValue; return;
        case PROJ_SXREPEAT:     proj.sxrepeat   = newValue; return;
        case PROJ_SYREPEAT:     proj.syrepeat   = newValue; return;
        case PROJ_SOUND:        proj.sound      = newValue; return;
        case PROJ_ISOUND:       proj.isound     = newValue; return;
        case PROJ_VEL:          proj.vel        = newValue; return;
        case PROJ_EXTRA:        proj.extra      = newValue; return;
        case PROJ_DECAL:        proj.decal      = newValue; return;
        case PROJ_TRAIL:        proj.trail      = newValue; return;
        case PROJ_TXREPEAT:     proj.txrepeat   = newValue; return;
        case PROJ_TYREPEAT:     proj.tyrepeat   = newValue; return;
        case PROJ_TOFFSET:      proj.toffset    = newValue; return;
        case PROJ_TNUM:         proj.tnum       = newValue; return;
        case PROJ_DROP:         proj.drop       = newValue; return;
        case PROJ_CSTAT:        proj.cstat      = newValue; return;
        case PROJ_CLIPDIST:     proj.clipdist   = newValue; return;
        case PROJ_SHADE:        proj.shade      = newValue; return;
        case PROJ_XREPEAT:      proj.xrepeat    = newValue; return;
        case PROJ_YREPEAT:      proj.yrepeat    = newValue; return;
        case PROJ_PAL:          proj.pal        = newValue; return;
        case PROJ_EXTRA_RAND:   proj.extra_rand = newValue; return;
        case PROJ_HITRADIUS:    proj.hitradius  = newValue; return;
        case PROJ_MOVECNT:      proj.movecnt    = newValue; return;
        case PROJ_OFFSET:       proj.offset     = newValue; return;
        case PROJ_BOUNCES:      proj.bounces    = newValue; return;
        case PROJ_BSOUND:       proj.bsound     = newValue; return;
        case PROJ_RANGE:        proj.range      = newValue; return;
        default: EDUKE32_UNREACHABLE_SECTION(return);
    }
}

int32_t __fastcall VM_GetPlayer(int const playerNum, int32_t labelNum, int const lParm2)
{
    auto& thisPlayer = g_player[playerNum];
    auto& ps = *thisPlayer.ps;
    auto const& playerLabel = PlayerLabels[labelNum];

    if ((thisPlayer.ps == NULL && playerLabel.lId != PLAYER_CONNECTED) && g_scriptSanityChecks)
    {
        CON_ERRPRINTF("tried to access NULL player! %d\n", playerNum);
        return -1;
    }

    switch (playerLabel.lId)
    {
        case PLAYER_ZOOM:               return ps.zoom;

        case PLAYER_EXITX:              return ps.exitx;
        case PLAYER_EXITY:              return ps.exity;

        case PLAYER_LOOGIEX:            return ps.loogiex[lParm2];
        case PLAYER_LOOGIEY:            return ps.loogiey[lParm2];
        case PLAYER_NUMLOOGS:           return ps.numloogs;
        case PLAYER_LOOGCNT:            return ps.loogcnt;

        case PLAYER_POSX:               return ps.pos.x;
        case PLAYER_POSY:               return ps.pos.y;
        case PLAYER_POSZ:               return ps.pos.z;

        case PLAYER_HORIZ:              return fix16_to_int(ps.q16horiz);
        case PLAYER_OHORIZ:             return fix16_to_int(ps.oq16horiz);
        case PLAYER_OHORIZOFF:          return fix16_to_int(ps.oq16horizoff);

        case PLAYER_INVDISPTIME:        return ps.invdisptime;

        case PLAYER_BOBPOSX:            return ps.bobposx;
        case PLAYER_BOBPOSY:            return ps.bobposy;

        case PLAYER_OPOSX:              return ps.opos.x;
        case PLAYER_OPOSY:              return ps.opos.y;
        case PLAYER_OPOSZ:              return ps.opos.z;

        case PLAYER_PYOFF:              return ps.pyoff;
        case PLAYER_OPYOFF:             return ps.opyoff;

        case PLAYER_POSXV:              return ps.vel.x;
        case PLAYER_POSYV:              return ps.vel.y;
        case PLAYER_POSZV:              return ps.vel.z;

        case PLAYER_LAST_PISSED_TIME:   return ps.last_pissed_time;

        case PLAYER_TRUEFZ:             return ps.truefz;
        case PLAYER_TRUECZ:             return ps.truecz;

        case PLAYER_PLAYER_PAR:         return ps.player_par;
        case PLAYER_VISIBILITY:         return thisPlayer.display.visibility;
        case PLAYER_BOBCOUNTER:         return ps.bobcounter;
        case PLAYER_WEAPON_SWAY:        return ps.weapon_sway;
        case PLAYER_PALS_TIME:          return ps.pals.f;
        case PLAYER_RANDOMFLAMEX:       return ps.randomflamex;
        case PLAYER_CRACK_TIME:         return ps.crack_time;
        case PLAYER_AIM_MODE:           return ps.aim_mode;

        case PLAYER_ANG:                return fix16_to_int(ps.q16ang);
        case PLAYER_OANG:               return fix16_to_int(ps.oq16ang);
        case PLAYER_ANGVEL:             return fix16_to_int(ps.q16angvel);

        case PLAYER_CURSECTNUM:         return ps.cursectnum;
        case PLAYER_LOOK_ANG:           return ps.look_ang;

        case PLAYER_LAST_EXTRA:         return ps.last_extra;
        case PLAYER_SUBWEAPON:          return ps.subweapon;
        case PLAYER_AMMO_AMOUNT:        return ps.ammo_amount[lParm2];
        case PLAYER_WACKEDBYACTOR:      return ps.wackedbyactor;
        case PLAYER_FRAG:               return ps.frag;
        case PLAYER_FRAGGEDSELF:        return ps.fraggedself;

        case PLAYER_CURR_WEAPON:        return ps.curr_weapon;
        case PLAYER_LAST_WEAPON:        return ps.last_weapon;

        case PLAYER_TIPINCS:            return ps.tipincs;
        case PLAYER_HORIZOFF:           return fix16_to_int(ps.q16horizoff);

        case PLAYER_WANTWEAPONFIRE:         return ps.wantweaponfire;
        case PLAYER_HOLODUKE_AMOUNT:        return ps.inv_amount[GET_HOLODUKE];
        case PLAYER_NEWOWNER:               return ps.newowner;
        case PLAYER_HURT_DELAY:             return ps.hurt_delay;
        case PLAYER_HBOMB_HOLD_DELAY:       return ps.hbomb_hold_delay;
        case PLAYER_JUMPING_COUNTER:        return ps.jumping_counter;
        case PLAYER_AIRLEFT:                return ps.airleft;
        case PLAYER_KNEE_INCS:              return ps.knee_incs;
        case PLAYER_ACCESS_INCS:            return ps.access_incs;
        case PLAYER_FTA:                    return ps.fta;
        case PLAYER_FTQ:                    return ps.ftq;
        case PLAYER_ACCESS_WALLNUM:         return ps.access_wallnum;
        case PLAYER_ACCESS_SPRITENUM:       return ps.access_spritenum;
        case PLAYER_KICKBACK_PIC:           return ps.kickback_pic;
        case PLAYER_GOT_ACCESS:             return ud.got_access; // MAKE ALIAS IN UD!
        case PLAYER_WEAPON_ANG:             return ps.weapon_ang;
        case PLAYER_FIRSTAID_AMOUNT:        return ps.inv_amount[GET_FIRSTAID];
        case PLAYER_SOMETHINGONPLAYER:      return ps.somethingonplayer;
        case PLAYER_ON_CRANE:               return ps.on_crane;
        case PLAYER_I:                      return ps.i;
        case PLAYER_ONE_PARALLAX_SECTNUM:   return ps.one_parallax_sectnum;
        case PLAYER_OVER_SHOULDER_ON:       return ps.over_shoulder_on;
        case PLAYER_RANDOM_CLUB_FRAME:      return ps.random_club_frame;
        case PLAYER_FIST_INCS:              return ps.fist_incs;
        case PLAYER_ONE_EIGHTY_COUNT:       return ps.one_eighty_count;
        case PLAYER_CHEAT_PHASE:            return ps.cheat_phase;
        case PLAYER_DUMMYPLAYERSPRITE:      return ps.dummyplayersprite;
        case PLAYER_EXTRA_EXTRA8:           return ps.extra_extra8;
        case PLAYER_QUICK_KICK:             return ps.quick_kick;
        case PLAYER_HEAT_AMOUNT:            return ps.inv_amount[GET_HEATS];
        case PLAYER_ACTORSQU:               return ps.actorsqu;
        case PLAYER_TIMEBEFOREEXIT:         return ps.timebeforeexit;
        case PLAYER_CUSTOMEXITSOUND:        return ps.customexitsound;
        case PLAYER_WEAPRECS:               return ps.weaprecs[lParm2];
        case PLAYER_WEAPRECCNT:             return ps.weapreccnt;
        case PLAYER_INTERFACE_TOGGLE_FLAG:  return ps.interface_toggle_flag;
        case PLAYER_ROTSCRNANG:             return ps.rotscrnang;
        case PLAYER_OROTSCRNANG:            return ps.orotscrnang;
        case PLAYER_DEAD_FLAG:              return ps.dead_flag;
        case PLAYER_SHOW_EMPTY_WEAPON:      return ps.show_empty_weapon;
        case PLAYER_SCUBA_AMOUNT:           return ps.inv_amount[GET_SCUBA];
        case PLAYER_JETPACK_AMOUNT:         return ps.inv_amount[GET_JETPACK];
        case PLAYER_STEROIDS_AMOUNT:        return ps.inv_amount[GET_STEROIDS];
        case PLAYER_SHIELD_AMOUNT:          return ps.inv_amount[GET_SHIELD];
        case PLAYER_HOLODUKE_ON:            return ps.holoduke_on;
        case PLAYER_PYCOUNT:                return ps.pycount;
        case PLAYER_WEAPON_POS:             return ps.weapon_pos;
        case PLAYER_FRAG_PS:                return ps.frag_ps;
        case PLAYER_TRANSPORTER_HOLD:       return ps.transporter_hold;
        case PLAYER_LAST_FULL_WEAPON:       return ps.last_full_weapon;
        case PLAYER_FOOTPRINTSHADE:         return ps.footprintshade;
        case PLAYER_BOOT_AMOUNT:            return ps.inv_amount[GET_BOOTS];
        case PLAYER_SCREAM_VOICE:           return ps.scream_voice;
        case PLAYER_GM:                     return ud.gm;
        case PLAYER_ON_WARPING_SECTOR:      return ps.on_warping_sector;
        case PLAYER_FOOTPRINTCOUNT:         return ps.footprintcount;
        case PLAYER_HBOMB_ON:               return ps.hbomb_on;
        case PLAYER_JUMPING_TOGGLE:         return ps.jumping_toggle;
        case PLAYER_RAPID_FIRE_HOLD:        return ps.rapid_fire_hold;
        case PLAYER_ON_GROUND:              return ps.on_ground;
        case PLAYER_INVEN_ICON:             return ps.inven_icon;
        case PLAYER_BUTTONPALETTE:          return ps.buttonpalette;
        case PLAYER_JETPACK_ON:             return ps.jetpack_on;
        case PLAYER_SPRITEBRIDGE:           return ps.spritebridge;
        case PLAYER_LASTRANDOMSPOT:         return ps.lastrandomspot;
        case PLAYER_SCUBA_ON:               return ps.scuba_on;
        case PLAYER_FOOTPRINTPAL:           return ps.footprintpal;

        case PLAYER_HEAT_ON:                return ps.heat_on;

        case PLAYER_HOLSTER_WEAPON:         return ps.holster_weapon;
        case PLAYER_FALLING_COUNTER:        return ps.falling_counter;
        case PLAYER_GOTWEAPON:              return ps.gotweapon[lParm2];
        case PLAYER_TOGGLE_KEY_FLAG:        return ps.toggle_key_flag;
        case PLAYER_KNUCKLE_INCS:           return ps.knuckle_incs;
        case PLAYER_WALKING_SND_TOGGLE:     return ps.walking_snd_toggle;
        case PLAYER_PALOOKUP:               return ps.palookup;
        case PLAYER_HARD_LANDING:           return ps.hard_landing;
        case PLAYER_MAX_SECRET_ROOMS:       return ud.max_secret_rooms; // DEPRECATE & MAKE ALIAS IN UD!
        case PLAYER_SECRET_ROOMS:           return ud.secret_rooms;     // DEPRECATE & MAKE ALIAS IN UD!

        case PLAYER_PALS:
        {
            switch (lParm2)
            {
                case 0: return ps.pals.r;
                case 1: return ps.pals.g;
                case 2: return ps.pals.b;
            }
            return 0;
        }

        case PLAYER_MAX_ACTORS_KILLED:      return ud.total_monsters; // DEPRECATE & MAKE ALIAS IN UD!
        case PLAYER_ACTORS_KILLED:          return ps.actors_killed;

        case PLAYER_RETURN_TO_CENTER:       return ps.return_to_center;
        case PLAYER_RUNSPEED:               return ps.runspeed;
        case PLAYER_SBS:                    return ps.sbs;
        case PLAYER_RELOADING:              return ps.reloading;
        case PLAYER_AUTO_AIM:               return ps.auto_aim;
        case PLAYER_MOVEMENT_LOCK:          return ps.movement_lock;

        case PLAYER_SOUND_PITCH:            return ps.sound_pitch;

        case PLAYER_WEAPONSWITCH:           return ps.weaponswitch;

        case PLAYER_TEAM:                   return ps.team;
        case PLAYER_MAX_PLAYER_HEALTH:      return ps.max_player_health;
        case PLAYER_MAX_SHIELD_AMOUNT:      return ps.max_shield_amount;
        case PLAYER_MAX_AMMO_AMOUNT:        return ps.max_ammo_amount[lParm2];
        case PLAYER_LAST_QUICK_KICK:        return ps.last_quick_kick;
        case PLAYER_CONNECTED:              return thisPlayer.connected; // Read-only.

        case PLAYER_LOWSPRITE:              return ps.lowsprite; // [JM] Lowest sprite touching. Read-only.
        case PLAYER_HIGHSPRITE:             return ps.highsprite; // [JM] Lowest sprite touching. // Read-only.

        default: EDUKE32_UNREACHABLE_SECTION(return -1);
    }
}

void __fastcall VM_SetPlayer(int const playerNum, int const labelNum, int const lParm2, int32_t const newValue)
{
    auto const& playerLabel = PlayerLabels[labelNum];

    auto& thisPlayer = g_player[playerNum];
    auto& ps = *thisPlayer.ps;

    if ((thisPlayer.ps == NULL) && g_scriptSanityChecks)
    {
        CON_ERRPRINTF("tried to access NULL player! %d\n", playerNum);
        return;
    }

    switch (playerLabel.lId)
    {
        case PLAYER_ZOOM:               ps.zoom                 = newValue; return;

        case PLAYER_EXITX:              ps.exitx                = newValue; return;
        case PLAYER_EXITY:              ps.exity                = newValue; return;

        case PLAYER_LOOGIEX:            ps.loogiex[lParm2]      = newValue; return;
        case PLAYER_LOOGIEY:            ps.loogiey[lParm2]      = newValue; return;
        case PLAYER_NUMLOOGS:           ps.numloogs             = newValue; return;
        case PLAYER_LOOGCNT:            ps.loogcnt              = newValue; return;

        case PLAYER_POSX:               ps.pos.x                = newValue; return;
        case PLAYER_POSY:               ps.pos.y                = newValue; return;
        case PLAYER_POSZ:               ps.pos.z                = newValue; return;

        case PLAYER_HORIZ:              ps.q16horiz             = fix16_from_int(newValue); return;
        case PLAYER_OHORIZ:             ps.oq16horiz            = fix16_from_int(newValue); return;
        case PLAYER_OHORIZOFF:          ps.oq16horizoff         = fix16_from_int(newValue); return;

        case PLAYER_INVDISPTIME:        ps.invdisptime          = newValue; return;

        case PLAYER_BOBPOSX:            ps.bobposx              = newValue; return;
        case PLAYER_BOBPOSY:            ps.bobposy              = newValue; return;

        case PLAYER_OPOSX:              ps.opos.x               = newValue; return;
        case PLAYER_OPOSY:              ps.opos.y               = newValue; return;
        case PLAYER_OPOSZ:              ps.opos.z               = newValue; return;

        case PLAYER_PYOFF:              ps.pyoff                = newValue; return;
        case PLAYER_OPYOFF:             ps.opyoff               = newValue; return;

        case PLAYER_POSXV:              ps.vel.x                = newValue; return;
        case PLAYER_POSYV:              ps.vel.y                = newValue; return;
        case PLAYER_POSZV:              ps.vel.z                = newValue; return;

        case PLAYER_LAST_PISSED_TIME:   ps.last_pissed_time     = newValue; return;

        case PLAYER_TRUEFZ:             ps.truefz               = newValue; return;
        case PLAYER_TRUECZ:             ps.truecz               = newValue; return;

        case PLAYER_PLAYER_PAR:         ps.player_par           = newValue; return;
        case PLAYER_VISIBILITY:         thisPlayer.display.visibility = newValue; return;
        case PLAYER_BOBCOUNTER:         ps.bobcounter           = newValue; return;
        case PLAYER_WEAPON_SWAY:        ps.weapon_sway          = newValue; return;
        case PLAYER_PALS_TIME:          ps.pals.f               = newValue; return;
        case PLAYER_RANDOMFLAMEX:       ps.randomflamex         = newValue; return;
        case PLAYER_CRACK_TIME:         ps.crack_time           = newValue; return;
        case PLAYER_AIM_MODE:           ps.aim_mode             = newValue; return;

        case PLAYER_ANG:                ps.q16ang               = fix16_from_int(newValue); return;
        case PLAYER_OANG:               ps.oq16ang              = fix16_from_int(newValue); return;
        case PLAYER_ANGVEL:             ps.q16angvel            = fix16_from_int(newValue); return;

        case PLAYER_CURSECTNUM:         ps.cursectnum           = newValue; return;
        case PLAYER_LOOK_ANG:           ps.look_ang             = newValue; return;
        case PLAYER_LAST_EXTRA:         ps.last_extra           = newValue; return;
        case PLAYER_SUBWEAPON:          ps.subweapon            = newValue; return;
        case PLAYER_AMMO_AMOUNT:        ps.ammo_amount[lParm2]  = newValue; return;
        case PLAYER_WACKEDBYACTOR:      ps.wackedbyactor        = newValue; return;
        case PLAYER_FRAG:               ps.frag                 = newValue; return;
        case PLAYER_FRAGGEDSELF:        ps.fraggedself          = newValue; return;

        case PLAYER_CURR_WEAPON:
        {
            ps.curr_weapon = newValue;
            Gv_SetVar(sysVarIDs.WEAPON, ps.curr_weapon, vm.spriteNum, playerNum);
            return;
        }
        case PLAYER_LAST_WEAPON:            ps.last_weapon              = newValue; return;

        case PLAYER_TIPINCS:                ps.tipincs                  = newValue; return;
        case PLAYER_HORIZOFF:               ps.q16horizoff              = fix16_from_int(newValue); return;

        case PLAYER_WANTWEAPONFIRE:         ps.wantweaponfire           = newValue; return;
        case PLAYER_HOLODUKE_AMOUNT:        ps.inv_amount[GET_HOLODUKE] = newValue; return;
        case PLAYER_NEWOWNER:               ps.newowner                 = newValue; return;
        case PLAYER_HURT_DELAY:             ps.hurt_delay               = newValue; return;
        case PLAYER_HBOMB_HOLD_DELAY:       ps.hbomb_hold_delay         = newValue; return;
        case PLAYER_JUMPING_COUNTER:        ps.jumping_counter          = newValue; return;
        case PLAYER_AIRLEFT:                ps.airleft                  = newValue; return;
        case PLAYER_KNEE_INCS:              ps.knee_incs                = newValue; return;
        case PLAYER_ACCESS_INCS:            ps.access_incs              = newValue; return;
        case PLAYER_FTA:                    ps.fta                      = newValue; return;
        case PLAYER_FTQ:                    ps.ftq                      = newValue; return;
        case PLAYER_ACCESS_WALLNUM:         ps.access_wallnum           = newValue; return;
        case PLAYER_ACCESS_SPRITENUM:       ps.access_spritenum         = newValue; return;
        case PLAYER_KICKBACK_PIC:           ps.kickback_pic             = newValue; return;
        case PLAYER_GOT_ACCESS:             ud.got_access               = newValue; return; // SHOULD MAKE ALIAS IN UD LABELS!
        case PLAYER_WEAPON_ANG:             ps.weapon_ang               = newValue; return;
        case PLAYER_FIRSTAID_AMOUNT:        ps.inv_amount[GET_FIRSTAID] = newValue; return;
        case PLAYER_SOMETHINGONPLAYER:      ps.somethingonplayer        = newValue; return;
        case PLAYER_ON_CRANE:               ps.on_crane                 = newValue; return;
        case PLAYER_I:                      ps.i                        = newValue; return;
        case PLAYER_ONE_PARALLAX_SECTNUM:   ps.one_parallax_sectnum     = newValue; return;
        case PLAYER_OVER_SHOULDER_ON:       ps.over_shoulder_on         = newValue; return;
        case PLAYER_RANDOM_CLUB_FRAME:      ps.random_club_frame        = newValue; return;
        case PLAYER_FIST_INCS:              ps.fist_incs                = newValue; return;
        case PLAYER_ONE_EIGHTY_COUNT:       ps.one_eighty_count         = newValue; return;
        case PLAYER_CHEAT_PHASE:            ps.cheat_phase              = newValue; return;
        case PLAYER_DUMMYPLAYERSPRITE:      ps.dummyplayersprite        = newValue; return;
        case PLAYER_EXTRA_EXTRA8:           ps.extra_extra8             = newValue; return;
        case PLAYER_QUICK_KICK:             ps.quick_kick               = newValue; return;
        case PLAYER_HEAT_AMOUNT:            ps.inv_amount[GET_HEATS]    = newValue; return;
        case PLAYER_ACTORSQU:               ps.actorsqu                 = newValue; return;
        case PLAYER_TIMEBEFOREEXIT:         ps.timebeforeexit           = newValue; return;
        case PLAYER_CUSTOMEXITSOUND:        ps.customexitsound          = newValue; return;
        case PLAYER_WEAPRECS:               ps.weaprecs[lParm2]         = newValue; return;
        case PLAYER_WEAPRECCNT:             ps.weapreccnt               = newValue; return;
        case PLAYER_INTERFACE_TOGGLE_FLAG:  ps.interface_toggle_flag    = newValue; return;
        case PLAYER_ROTSCRNANG:             ps.rotscrnang               = newValue; return;
        case PLAYER_OROTSCRNANG:            ps.orotscrnang              = newValue; return;
        case PLAYER_DEAD_FLAG:              ps.dead_flag                = newValue; return;
        case PLAYER_SHOW_EMPTY_WEAPON:      ps.show_empty_weapon        = newValue; return;
        case PLAYER_SCUBA_AMOUNT:           ps.inv_amount[GET_SCUBA]    = newValue; return;
        case PLAYER_JETPACK_AMOUNT:         ps.inv_amount[GET_JETPACK]  = newValue; return;
        case PLAYER_STEROIDS_AMOUNT:        ps.inv_amount[GET_STEROIDS] = newValue; return;
        case PLAYER_SHIELD_AMOUNT:          ps.inv_amount[GET_SHIELD]   = newValue; return;
        case PLAYER_HOLODUKE_ON:            ps.holoduke_on              = newValue; return;
        case PLAYER_PYCOUNT:                ps.pycount                  = newValue; return;
        case PLAYER_WEAPON_POS:             ps.weapon_pos               = newValue; return;
        case PLAYER_FRAG_PS:                ps.frag_ps                  = newValue; return;
        case PLAYER_TRANSPORTER_HOLD:       ps.transporter_hold         = newValue; return;
        case PLAYER_LAST_FULL_WEAPON:       ps.last_full_weapon         = newValue; return;
        case PLAYER_FOOTPRINTSHADE:         ps.footprintshade           = newValue; return;
        case PLAYER_BOOT_AMOUNT:            ps.inv_amount[GET_BOOTS]    = newValue; return;
        case PLAYER_SCREAM_VOICE:           ps.scream_voice             = newValue; return;
        case PLAYER_GM:                     ud.gm                       = newValue; return;
        case PLAYER_ON_WARPING_SECTOR:      ps.on_warping_sector        = newValue; return;
        case PLAYER_FOOTPRINTCOUNT:         ps.footprintcount           = newValue; return;
        case PLAYER_HBOMB_ON:               ps.hbomb_on                 = newValue; return;
        case PLAYER_JUMPING_TOGGLE:         ps.jumping_toggle           = newValue; return;
        case PLAYER_RAPID_FIRE_HOLD:        ps.rapid_fire_hold          = newValue; return;
        case PLAYER_ON_GROUND:              ps.on_ground                = newValue; return;
        case PLAYER_INVEN_ICON:             ps.inven_icon               = newValue; return;
        case PLAYER_BUTTONPALETTE:          ps.buttonpalette            = newValue; return;
        case PLAYER_JETPACK_ON:             ps.jetpack_on               = newValue; return;
        case PLAYER_SPRITEBRIDGE:           ps.spritebridge             = newValue; return;
        case PLAYER_LASTRANDOMSPOT:         ps.lastrandomspot           = newValue; return;
        case PLAYER_SCUBA_ON:               ps.scuba_on                 = newValue; return;
        case PLAYER_FOOTPRINTPAL:           ps.footprintpal             = newValue; return;

        case PLAYER_HEAT_ON:
        {
            if (ps.heat_on != newValue)
            {
                ps.heat_on = newValue;
                P_UpdateScreenPal(&ps);
            }
            return;
        }

        case PLAYER_HOLSTER_WEAPON:         ps.holster_weapon           = newValue; return;
        case PLAYER_FALLING_COUNTER:        ps.falling_counter          = newValue; return;
        case PLAYER_GOTWEAPON:              ps.gotweapon[lParm2]        = newValue; return;
        case PLAYER_TOGGLE_KEY_FLAG:        ps.toggle_key_flag          = newValue; return;
        case PLAYER_KNUCKLE_INCS:           ps.knuckle_incs             = newValue; return;
        case PLAYER_WALKING_SND_TOGGLE:     ps.walking_snd_toggle       = newValue; return;
        case PLAYER_PALOOKUP:               ps.palookup                 = newValue; return;
        case PLAYER_HARD_LANDING:           ps.hard_landing             = newValue; return;
        case PLAYER_MAX_SECRET_ROOMS:       ud.max_secret_rooms         = newValue; return; // DEPRECATE & MAKE ALIAS IN UD!
        case PLAYER_SECRET_ROOMS:           ud.secret_rooms             = newValue; return; // DEPRECATE & MAKE ALIAS IN UD!

        case PLAYER_PALS:
        {
            switch (lParm2)
            {
                case 0: ps.pals.r = newValue; break;
                case 1: ps.pals.g = newValue; break;
                case 2: ps.pals.b = newValue; break;
            }
            return;
        }

        case PLAYER_MAX_ACTORS_KILLED:      ud.total_monsters           = newValue; return; // DEPRECATE & MAKE ALIAS IN UD!
        case PLAYER_ACTORS_KILLED:          ps.actors_killed            = newValue; return;

        case PLAYER_RETURN_TO_CENTER:       ps.return_to_center         = newValue; return;
        case PLAYER_RUNSPEED:               ps.runspeed                 = newValue; return;
        case PLAYER_SBS:                    ps.sbs                      = newValue; return;
        case PLAYER_RELOADING:              ps.reloading                = newValue; return;
        case PLAYER_AUTO_AIM:               ps.auto_aim                 = newValue; return;
        case PLAYER_MOVEMENT_LOCK:          ps.movement_lock            = newValue; return;

        case PLAYER_SOUND_PITCH:
            ps.sound_pitch = newValue;
            if (playerNum == screenpeek)
                S_ChangeAllSoundPitches(newValue);
            return;

        case PLAYER_WEAPONSWITCH:           ps.weaponswitch             = newValue; return;

        case PLAYER_TEAM:                   ps.team                     = newValue; return;
        case PLAYER_MAX_PLAYER_HEALTH:      ps.max_player_health        = newValue; return;
        case PLAYER_MAX_SHIELD_AMOUNT:      ps.max_shield_amount        = newValue; return;
        case PLAYER_MAX_AMMO_AMOUNT:        ps.max_ammo_amount[lParm2]  = newValue; return;
        case PLAYER_LAST_QUICK_KICK:        ps.last_quick_kick          = newValue; return;
        case PLAYER_CONNECTED:              return; // Read-only.

        case PLAYER_LOWSPRITE:              return; // [JM] Lowest sprite touching. Read-only.
        case PLAYER_HIGHSPRITE:             return; // [JM] Lowest sprite touching. // Read-only.

        default: EDUKE32_UNREACHABLE_SECTION(return);
    }
}

int32_t __fastcall VM_GetPlayerInput(int const playerNum, int32_t labelNum)
{
    if (((unsigned)playerNum >= MAXPLAYERS || g_player[playerNum].ps == NULL || !g_player[playerNum].connected) && g_scriptSanityChecks)
    {
        CON_ERRPRINTF("invalid player %d\n", playerNum);
        return -1;
    }

    auto& i = *g_player[playerNum].inputBits;
    auto const& inputLabel = InputLabels[labelNum];

    switch (inputLabel.lId)
    {
        case INPUT_AVEL:    return fix16_to_int(i.q16avel);
        case INPUT_HORZ:    return fix16_to_int(i.q16horz);
        case INPUT_FVEL:    return i.fvel;
        case INPUT_SVEL:    return i.svel;
        case INPUT_BITS:    return i.bits;
        case INPUT_EXTBITS: return i.extbits;
        default: EDUKE32_UNREACHABLE_SECTION(return -1);
    }
}

void __fastcall VM_SetPlayerInput(int const playerNum, int const labelNum, int32_t const newValue)
{
    if (((unsigned)playerNum >= MAXPLAYERS || g_player[playerNum].ps == NULL || !g_player[playerNum].connected) && g_scriptSanityChecks)
    {
        CON_ERRPRINTF("invalid player %d\n", playerNum);
        return;
    }

    auto& i = *g_player[playerNum].inputBits;
    auto const& inputLabel = InputLabels[labelNum];

    switch (inputLabel.lId)
    {
        case INPUT_AVEL:    i.q16avel   = fix16_from_int(newValue); return;
        case INPUT_HORZ:    i.q16horz   = fix16_from_int(newValue); return;
        case INPUT_FVEL:    i.fvel      = newValue; return;
        case INPUT_SVEL:    i.svel      = newValue; return;
        case INPUT_BITS:    i.bits      = newValue; return;
        case INPUT_EXTBITS: i.extbits   = newValue; return;
        default: EDUKE32_UNREACHABLE_SECTION(return);
    }
}

int32_t __fastcall VM_GetWall(int const wallNum, int32_t labelNum)
{
    auto const& w = wall[wallNum];
    auto const& wallLabel = WallLabels[labelNum];

    switch (wallLabel.lId)
    {
        case WALL_X:            return w.x;
        case WALL_Y:            return w.y;

        case WALL_POINT2:       return w.point2;
        case WALL_NEXTWALL:     return w.nextwall;
        case WALL_NEXTSECTOR:   return w.nextsector;
        case WALL_CSTAT:        return w.cstat;
        case WALL_PICNUM:       return w.picnum;
        case WALL_OVERPICNUM:   return w.overpicnum;
        case WALL_SHADE:        return w.shade;
        case WALL_PAL:          return w.pal;
        case WALL_XREPEAT:      return w.xrepeat;
        case WALL_YREPEAT:      return w.yrepeat;
        case WALL_XPANNING:     return w.xpanning;
        case WALL_YPANNING:     return w.ypanning;
        case WALL_LOTAG:        return w.lotag;
        case WALL_HITAG:        return w.hitag;
        case WALL_EXTRA:        return w.extra;

        default: EDUKE32_UNREACHABLE_SECTION(return -1);
    }
}

void __fastcall VM_SetWall(int const wallNum, int const labelNum, int32_t const newValue)
{
    auto& w = wall[wallNum];
    auto const& wallLabel = WallLabels[labelNum];

    switch (wallLabel.lId)
    {
        case WALL_X:            w.x             = newValue; return;
        case WALL_Y:            w.y             = newValue; return;

        case WALL_POINT2:       w.point2        = newValue; return;
        case WALL_NEXTWALL:     w.nextwall      = newValue; return;
        case WALL_NEXTSECTOR:   w.nextsector    = newValue; return;
        case WALL_CSTAT:        w.cstat         = newValue; return;
        case WALL_PICNUM:       w.picnum        = newValue; return;
        case WALL_OVERPICNUM:   w.overpicnum    = newValue; return;
        case WALL_SHADE:        w.shade         = newValue; return;
        case WALL_PAL:          w.pal           = newValue; return;
        case WALL_XREPEAT:      w.xrepeat       = newValue; return;
        case WALL_YREPEAT:      w.yrepeat       = newValue; return;
        case WALL_XPANNING:     w.xpanning      = newValue; return;
        case WALL_YPANNING:     w.ypanning      = newValue; return;
        case WALL_LOTAG:        w.lotag         = newValue; return;
        case WALL_HITAG:        w.hitag         = newValue; return;
        case WALL_EXTRA:        w.extra         = newValue; return;

        default: EDUKE32_UNREACHABLE_SECTION(return);
    }
}

int32_t __fastcall VM_GetSector(int const sectNum, int32_t labelNum)
{
    auto const& sectLabel = SectorLabels[labelNum];
    auto& s = sector[sectNum];

    switch (sectLabel.lId)
    {
        case SECTOR_WALLPTR:        return s.wallptr;
        case SECTOR_WALLNUM:        return s.wallnum;

        case SECTOR_CEILINGZ:       return s.ceilingz;
        case SECTOR_CEILINGZVEL:    return (GetAnimationGoal(sectNum, SECTANIM_CEILING) == -1) ? 0 : s.extra;
        case SECTOR_CEILINGZGOAL:   return GetAnimationGoal(sectNum, SECTANIM_CEILING);

        case SECTOR_FLOORZ:         return s.floorz;
        case SECTOR_FLOORZVEL:      return (GetAnimationGoal(sectNum, SECTANIM_FLOOR) == -1) ? 0 : s.extra;
        case SECTOR_FLOORZGOAL:     return GetAnimationGoal(sectNum, SECTANIM_FLOOR);

        case SECTOR_CEILINGSTAT:        return s.ceilingstat;
        case SECTOR_FLOORSTAT:          return s.floorstat;

        case SECTOR_CEILINGPICNUM:      return s.ceilingpicnum;
        case SECTOR_CEILINGSLOPE:       return s.ceilingheinum;
        case SECTOR_CEILINGSHADE:       return s.ceilingshade;
        case SECTOR_CEILINGPAL:         return s.ceilingpal;
        case SECTOR_CEILINGXPANNING:    return s.ceilingxpanning;
        case SECTOR_CEILINGYPANNING:    return s.ceilingypanning;

        case SECTOR_FLOORPICNUM:        return s.floorpicnum;
        case SECTOR_FLOORSLOPE:         return s.floorheinum;
        case SECTOR_FLOORSHADE:         return s.floorshade;
        case SECTOR_FLOORPAL:           return s.floorpal;
        case SECTOR_FLOORXPANNING:      return s.floorxpanning;
        case SECTOR_FLOORYPANNING:      return s.floorypanning;

        case SECTOR_VISIBILITY:         return s.visibility;
        case SECTOR_ALIGNTO:            return s.fogpal;
        case SECTOR_LOTAG:              return s.lotag;
        case SECTOR_HITAG:              return s.hitag;
        case SECTOR_EXTRA:              return s.extra;

        case SECTOR_CEILINGBUNCH:
        case SECTOR_FLOORBUNCH:
#ifdef YAX_ENABLE
            return yax_getbunch(sectNum, labelNum == SECTOR_FLOORBUNCH);
#else
            return -1;
#endif

        default: EDUKE32_UNREACHABLE_SECTION(return -1);
    }
}

void __fastcall VM_SetSector(int const sectNum, int const labelNum, int32_t newValue)
{
    auto const& sectLabel = SectorLabels[labelNum];
    auto& s = sector[sectNum];

    switch (sectLabel.lId)
    {
        case SECTOR_WALLPTR: s.wallptr = newValue; return;
        case SECTOR_WALLNUM: s.wallnum = newValue; return;

        case SECTOR_CEILINGZ: s.ceilingz = newValue; return;
        case SECTOR_CEILINGZVEL:
        {
            s.extra = newValue;
            int const goal = GetAnimationGoal(sectNum, SECTANIM_CEILING);
            if (goal != -1)
                SetAnimation(sectNum, SECTANIM_CEILING, goal, s.extra);

            return;
        }
        case SECTOR_CEILINGZGOAL: SetAnimation(sectNum, SECTANIM_CEILING, newValue, s.extra); return;

        case SECTOR_FLOORZ: s.floorz = newValue; return;
        case SECTOR_FLOORZVEL:
        {
            s.extra = newValue;
            int const goal = GetAnimationGoal(sectNum, SECTANIM_FLOOR);
            if (goal != -1)
                SetAnimation(sectNum, SECTANIM_FLOOR, goal, s.extra);

            return;
        }
        case SECTOR_FLOORZGOAL: SetAnimation(sectNum, SECTANIM_FLOOR, newValue, s.extra); return;

        case SECTOR_CEILINGSTAT:        s.ceilingstat       = newValue; return;
        case SECTOR_FLOORSTAT:          s.floorstat         = newValue; return;

        case SECTOR_CEILINGPICNUM:      s.ceilingpicnum     = newValue; return;
        case SECTOR_CEILINGSLOPE:       s.ceilingheinum     = newValue; return;
        case SECTOR_CEILINGSHADE:       s.ceilingshade      = newValue; return;
        case SECTOR_CEILINGPAL:         s.ceilingpal        = newValue; return;
        case SECTOR_CEILINGXPANNING:    s.ceilingxpanning   = newValue; return;
        case SECTOR_CEILINGYPANNING:    s.ceilingypanning   = newValue; return;

        case SECTOR_FLOORPICNUM:        s.floorpicnum       = newValue; return;
        case SECTOR_FLOORSLOPE:         s.floorheinum       = newValue; return;
        case SECTOR_FLOORSHADE:         s.floorshade        = newValue; return;
        case SECTOR_FLOORPAL:           s.floorpal          = newValue; return;
        case SECTOR_FLOORXPANNING:      s.floorxpanning     = newValue; return;
        case SECTOR_FLOORYPANNING:      s.floorypanning     = newValue; return;

        case SECTOR_VISIBILITY:         s.visibility        = newValue; return;
        case SECTOR_ALIGNTO:            s.fogpal            = newValue; return;
        case SECTOR_LOTAG:              s.lotag             = newValue; return;
        case SECTOR_HITAG:              s.hitag             = newValue; return;
        case SECTOR_EXTRA:              s.extra             = newValue; return;

        case SECTOR_CEILINGBUNCH:
        case SECTOR_FLOORBUNCH:
            break;

        default: EDUKE32_UNREACHABLE_SECTION(return);
    }
}

int32_t __fastcall VM_GetSprite(int const spriteNum, int32_t labelNum, int const lParm2)
{
    auto const& actorLabel = ActorLabels[labelNum];
    auto const& act = actor[spriteNum];
    auto const& spr = sprite[spriteNum];
    auto const& ext = spriteext[spriteNum];

    switch (actorLabel.lId)
    {
        case ACTOR_X:               return spr.x;
        case ACTOR_Y:               return spr.y;
        case ACTOR_Z:               return spr.z;

        case ACTOR_CSTAT:           return spr.cstat;
        case ACTOR_PICNUM:          return spr.picnum;
        case ACTOR_SHADE:           return spr.shade;
        case ACTOR_PAL:             return spr.pal;
        case ACTOR_CLIPDIST:        return spr.clipdist;

        case ACTOR_BLEND:           return spr.blend;

        case ACTOR_XREPEAT:         return spr.xrepeat;
        case ACTOR_YREPEAT:         return spr.yrepeat;

        case ACTOR_XOFFSET:         return spr.xoffset;
        case ACTOR_YOFFSET:         return spr.yoffset;

        case ACTOR_SECTNUM:         return spr.sectnum;
        case ACTOR_STATNUM:         return spr.statnum;

        case ACTOR_ANG:             return spr.ang;
        case ACTOR_OWNER:           return spr.owner;

        case ACTOR_XVEL:            return spr.xvel;
        case ACTOR_YVEL:            return spr.yvel;
        case ACTOR_ZVEL:            return spr.zvel;

        case ACTOR_LOTAG:           return spr.lotag;
        case ACTOR_HITAG:           return spr.hitag;

        case ACTOR_EXTRA:           return spr.extra;

        case ACTOR_HTCGG:           return act.cgg;
        case ACTOR_HTPICNUM:        return act.htpicnum;
        case ACTOR_HTANG:           return act.htang;
        case ACTOR_HTEXTRA:         return act.htextra;
        case ACTOR_HTOWNER:         return act.htowner;
        case ACTOR_HTMOVFLAG:       return act.movflag;
        case ACTOR_HTTEMPANG:       return act.tempang;
        case ACTOR_HTSTAYPUT:       return act.actorstayput;
        case ACTOR_HTDISPICNUM:     return act.dispicnum;
        case ACTOR_HTTIMETOSLEEP:   return act.timetosleep;
        case ACTOR_HTFLOORZ:        return act.floorz;
        case ACTOR_HTCEILINGZ:      return act.ceilingz;
        case ACTOR_HTLASTVX:        return act.lastvx;
        case ACTOR_HTLASTVY:        return act.lastvy;
        case ACTOR_HTBPOSX:         return act.bpos.x;
        case ACTOR_HTBPOSY:         return act.bpos.y;
        case ACTOR_HTBPOSZ:         return act.bpos.z;
        case ACTOR_HTG_T:           return act.t_data[lParm2];
        case ACTOR_HTFLAGS:         return act.flags;

        case ACTOR_ANGOFF:          return ext.mdangoff;
        case ACTOR_PITCH:           return ext.mdpitch;
        case ACTOR_ROLL:            return ext.mdroll;
        case ACTOR_MDXOFF:          return ext.mdpivot_offset.x;
        case ACTOR_MDYOFF:          return ext.mdpivot_offset.y;
        case ACTOR_MDZOFF:          return ext.mdpivot_offset.z;
        case ACTOR_MDFLAGS:         return ext.flags;
        case ACTOR_XPANNING:        return ext.xpanning;
        case ACTOR_YPANNING:        return ext.ypanning;
        case ACTOR_ALPHA:           return (uint8_t)(ext.alpha * 255.0f);

        default: EDUKE32_UNREACHABLE_SECTION(return -1);
    }
}

void __fastcall VM_SetSprite(int const spriteNum, int const labelNum, int const lParm2, int32_t const newValue)
{
    auto const& actorLabel = ActorLabels[labelNum];
    auto& act = actor[spriteNum];
    auto& spr = sprite[spriteNum];
    auto& ext = spriteext[spriteNum];

    switch (actorLabel.lId)
    {
        case ACTOR_X:               spr.x                   = newValue; return;
        case ACTOR_Y:               spr.y                   = newValue; return;
        case ACTOR_Z:               spr.z                   = newValue; return;

        case ACTOR_CSTAT:           spr.cstat               = newValue; return;
        case ACTOR_PICNUM:          spr.picnum              = newValue; return;
        case ACTOR_SHADE:           spr.shade               = newValue; return;
        case ACTOR_PAL:             spr.pal                 = newValue; return;
        case ACTOR_CLIPDIST:        spr.clipdist            = newValue; return;

        case ACTOR_BLEND:           spr.blend               = newValue; return;

        case ACTOR_XREPEAT:         spr.xrepeat             = newValue; return;
        case ACTOR_YREPEAT:         spr.yrepeat             = newValue; return;

        case ACTOR_XOFFSET:         spr.xoffset             = newValue; return;
        case ACTOR_YOFFSET:         spr.yoffset             = newValue; return;

        case ACTOR_SECTNUM:         changespritesect(spriteNum, newValue); return;
        case ACTOR_STATNUM:         changespritestat(spriteNum, newValue); return;

        case ACTOR_ANG:             spr.ang                 = newValue; return;
        case ACTOR_OWNER:           spr.owner               = newValue; return;

        case ACTOR_XVEL:            spr.xvel                = newValue; return;
        case ACTOR_YVEL:            spr.yvel                = newValue; return;
        case ACTOR_ZVEL:            spr.zvel                = newValue; return;

        case ACTOR_LOTAG:           spr.lotag               = newValue; return;
        case ACTOR_HITAG:           spr.hitag               = newValue; return;

        case ACTOR_EXTRA:           spr.extra               = newValue; return;

        case ACTOR_HTCGG:           act.cgg                 = newValue; return;
        case ACTOR_HTPICNUM:        act.htpicnum            = newValue; return;
        case ACTOR_HTANG:           act.htang               = newValue; return;
        case ACTOR_HTEXTRA:         act.htextra             = newValue; return;
        case ACTOR_HTOWNER:         act.htowner             = newValue; return;
        case ACTOR_HTMOVFLAG:       act.movflag             = newValue; return;
        case ACTOR_HTTEMPANG:       act.tempang             = newValue; return;
        case ACTOR_HTSTAYPUT:       act.actorstayput        = newValue; return;
        case ACTOR_HTDISPICNUM:     act.dispicnum           = newValue; return;
        case ACTOR_HTTIMETOSLEEP:   act.timetosleep         = newValue; return;
        case ACTOR_HTFLOORZ:        act.floorz              = newValue; return;
        case ACTOR_HTCEILINGZ:      act.ceilingz            = newValue; return;
        case ACTOR_HTLASTVX:        act.lastvx              = newValue; return;
        case ACTOR_HTLASTVY:        act.lastvy              = newValue; return;
        case ACTOR_HTBPOSX:         act.bpos.x              = newValue; return;
        case ACTOR_HTBPOSY:         act.bpos.y              = newValue; return;
        case ACTOR_HTBPOSZ:         act.bpos.z              = newValue; return;
        case ACTOR_HTG_T:           act.t_data[lParm2]      = newValue; return;
        case ACTOR_HTFLAGS:         act.flags               = newValue; return;

        case ACTOR_ANGOFF:          ext.mdangoff            = newValue; return;
        case ACTOR_PITCH:           ext.mdpitch             = newValue; return;
        case ACTOR_ROLL:            ext.mdroll              = newValue; return;
        case ACTOR_MDXOFF:          ext.mdpivot_offset.x    = newValue; return;
        case ACTOR_MDYOFF:          ext.mdpivot_offset.y    = newValue; return;
        case ACTOR_MDZOFF:          ext.mdpivot_offset.z    = newValue; return;
        case ACTOR_MDFLAGS:         ext.flags               = newValue; return;
        case ACTOR_XPANNING:        ext.xpanning            = newValue; return;
        case ACTOR_YPANNING:        ext.ypanning            = newValue; return;
        case ACTOR_ALPHA:           ext.alpha               = (float)newValue * (1.f / 255.0f); return;

        default: EDUKE32_UNREACHABLE_SECTION(return);
    }
}

int32_t __fastcall VM_GetProjectile(int const tileNum, int32_t labelNum)
{
    auto const& projLabel = ProjectileLabels[labelNum];

    if (EDUKE32_PREDICT_FALSE((unsigned)tileNum >= MAXTILES || g_tile[tileNum].proj == NULL))
    {
        CON_ERRPRINTF("invalid projectile %d\n", tileNum);
        return -1;
    }

    auto const& proj = *g_tile[tileNum].proj;
    switch (projLabel.lId)
    {
        case PROJ_WORKSLIKE:    return proj.workslike;
        case PROJ_SPAWNS:       return proj.spawns;
        case PROJ_SXREPEAT:     return proj.sxrepeat;
        case PROJ_SYREPEAT:     return proj.syrepeat;
        case PROJ_SOUND:        return proj.sound;
        case PROJ_ISOUND:       return proj.isound;
        case PROJ_VEL:          return proj.vel;
        case PROJ_EXTRA:        return proj.extra;
        case PROJ_DECAL:        return proj.decal;
        case PROJ_TRAIL:        return proj.trail;
        case PROJ_TXREPEAT:     return proj.txrepeat;
        case PROJ_TYREPEAT:     return proj.tyrepeat;
        case PROJ_TOFFSET:      return proj.toffset;
        case PROJ_TNUM:         return proj.tnum;
        case PROJ_DROP:         return proj.drop;
        case PROJ_CSTAT:        return proj.cstat;
        case PROJ_CLIPDIST:     return proj.clipdist;
        case PROJ_SHADE:        return proj.shade;
        case PROJ_XREPEAT:      return proj.xrepeat;
        case PROJ_YREPEAT:      return proj.yrepeat;
        case PROJ_PAL:          return proj.pal;
        case PROJ_EXTRA_RAND:   return proj.extra_rand;
        case PROJ_HITRADIUS:    return proj.hitradius;
        case PROJ_MOVECNT:      return proj.movecnt;
        case PROJ_OFFSET:       return proj.offset;
        case PROJ_BOUNCES:      return proj.bounces;
        case PROJ_BSOUND:       return proj.bsound;
        case PROJ_RANGE:        return proj.range;
        case PROJ_FLASH_COLOR:  return proj.flashcolor;
        default: EDUKE32_UNREACHABLE_SECTION(return -1);
    }
}

void __fastcall VM_SetProjectile(int const tileNum, int const labelNum, int32_t const newValue)
{
    auto const& projLabel = ProjectileLabels[labelNum];

    if (EDUKE32_PREDICT_FALSE((unsigned)tileNum >= MAXTILES || g_tile[tileNum].proj == NULL))
    {
        CON_ERRPRINTF("invalid projectile %d\n", tileNum);
        return;
    }

    auto& proj = *g_tile[tileNum].proj;
    switch (projLabel.lId)
    {
        case PROJ_WORKSLIKE:    proj.workslike  = newValue; return;
        case PROJ_SPAWNS:       proj.spawns     = newValue; return;
        case PROJ_SXREPEAT:     proj.sxrepeat   = newValue; return;
        case PROJ_SYREPEAT:     proj.syrepeat   = newValue; return;
        case PROJ_SOUND:        proj.sound      = newValue; return;
        case PROJ_ISOUND:       proj.isound     = newValue; return;
        case PROJ_VEL:          proj.vel        = newValue; return;
        case PROJ_EXTRA:        proj.extra      = newValue; return;
        case PROJ_DECAL:        proj.decal      = newValue; return;
        case PROJ_TRAIL:        proj.trail      = newValue; return;
        case PROJ_TXREPEAT:     proj.txrepeat   = newValue; return;
        case PROJ_TYREPEAT:     proj.tyrepeat   = newValue; return;
        case PROJ_TOFFSET:      proj.toffset    = newValue; return;
        case PROJ_TNUM:         proj.tnum       = newValue; return;
        case PROJ_DROP:         proj.drop       = newValue; return;
        case PROJ_CSTAT:        proj.cstat      = newValue; return;
        case PROJ_CLIPDIST:     proj.clipdist   = newValue; return;
        case PROJ_SHADE:        proj.shade      = newValue; return;
        case PROJ_XREPEAT:      proj.xrepeat    = newValue; return;
        case PROJ_YREPEAT:      proj.yrepeat    = newValue; return;
        case PROJ_PAL:          proj.pal        = newValue; return;
        case PROJ_EXTRA_RAND:   proj.extra_rand = newValue; return;
        case PROJ_HITRADIUS:    proj.hitradius  = newValue; return;
        case PROJ_MOVECNT:      proj.movecnt    = newValue; return;
        case PROJ_OFFSET:       proj.offset     = newValue; return;
        case PROJ_BOUNCES:      proj.bounces    = newValue; return;
        case PROJ_BSOUND:       proj.bsound     = newValue; return;
        case PROJ_RANGE:        proj.range      = newValue; return;
        case PROJ_FLASH_COLOR:  proj.flashcolor = newValue; return;
        default: EDUKE32_UNREACHABLE_SECTION(return);
    }
}
