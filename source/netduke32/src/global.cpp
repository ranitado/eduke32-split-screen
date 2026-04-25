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

#define global_c_
#include "global.h"
#include "duke3d.h"

// Pre-initialized globals, the rest is in global.h
int g_spriteGravity = 176; // JBF: g_spriteGravity modified to default to Atomic ed. default when using 1.3d CONs
int g_actorRespawnTime = 768;
int g_itemRespawnTime = 768;
int g_timerTicsPerSecond = TICRATE;
int g_scriptSize = 16384;
int g_playerFriction = 0xcc00;
int g_networkBroadcastMode = NETMODE_OFFLINE;
int g_numFreezeBounces = 3;
short g_spriteDeleteQueueSize=64;

char EpisodeNames[MAXVOLUMES][33] = { "L.A. MELTDOWN", "LUNAR APOCALYPSE", "SHRAPNEL CITY" };
char g_numVolumes = 3;

char g_skillNames[5][33] = { "PIECE OF CAKE", "LET'S ROCK", "COME GET SOME", "DAMN I'M GOOD" };

int16_t g_blimpSpawnItems[15] =
{
    RPGSPRITE__,
    CHAINGUNSPRITE__,
    DEVISTATORAMMO__,
    RPGAMMO__,
    RPGAMMO__,
    JETPACK__,
    SHIELD__,
    FIRSTAID__,
    STEROIDS__,
    RPGAMMO__,
    RPGAMMO__,
    RPGSPRITE__,
    RPGAMMO__,
    FREEZESPRITE__,
    FREEZEAMMO__
};

char CheatKeys[2] = { sc_D, sc_N };
char g_setupFileName[BMAX_PATH] = SETUPFILENAME;

