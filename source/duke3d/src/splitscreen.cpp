//-------------------------------------------------------------------------
/*
Copyright (C) 2026 EDuke32 developers and contributors

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
//-------------------------------------------------------------------------

#include "splitscreen.h"

#include "build.h"
#include "cmdline.h"
#include "duke3d.h"
#include "game.h"
#include "mmulti.h"
#include "premap.h"

namespace
{
static constexpr int32_t MAX_LOCAL_SPLITSCREEN_PLAYERS = 4;
static int32_t g_splitScreenConfiguredPlayerCount = 0;
static int32_t g_splitScreenActivePlayerCount = 0;
static int32_t g_splitScreenActivePlayers[MAX_LOCAL_SPLITSCREEN_PLAYERS] = { 0, 1, 2, 3 };

static int32_t G_GetRequestedFakeMultiplayerCount(void)
{
#ifdef SPLITSCREEN_MOD_HACKS
    if (g_fakeMultiMode > 1)
        return min<int32_t>(g_fakeMultiMode, MAX_LOCAL_SPLITSCREEN_PLAYERS);
#endif

    return 1;
}

static void G_EnsureSplitScreenPlayerCountsInitialized(void)
{
    if (g_splitScreenConfiguredPlayerCount > 0 && g_splitScreenActivePlayerCount > 0)
        return;

    int32_t const requestedPlayerCount = G_GetRequestedFakeMultiplayerCount();
    g_splitScreenConfiguredPlayerCount = clamp(requestedPlayerCount, 1, MAX_LOCAL_SPLITSCREEN_PLAYERS);
    g_splitScreenActivePlayerCount = clamp(requestedPlayerCount, 1, MAX_LOCAL_SPLITSCREEN_PLAYERS);

    for (int i = 0; i < MAX_LOCAL_SPLITSCREEN_PLAYERS; ++i)
        g_splitScreenActivePlayers[i] = i;
}

static int32_t G_GetHighestActiveSplitScreenPlayer(void)
{
    int32_t highestPlayer = 0;

    for (int viewIndex = 0; viewIndex < g_splitScreenActivePlayerCount; ++viewIndex)
        highestPlayer = max(highestPlayer, g_splitScreenActivePlayers[viewIndex]);

    return highestPlayer;
}

static void G_RebuildLocalConnectionChain(void)
{
    connecthead = g_splitScreenActivePlayers[0];

    for (int i = 0; i < MAXPLAYERS; ++i)
    {
        connectpoint2[i] = -1;
        g_player[i].playerquitflag = 0;
    }

    for (int viewIndex = 0; viewIndex < g_splitScreenActivePlayerCount; ++viewIndex)
    {
        int const playerNum = g_splitScreenActivePlayers[viewIndex];
        if ((unsigned)playerNum >= MAXPLAYERS)
            continue;

        g_player[playerNum].playerquitflag = 1;
        connectpoint2[playerNum] = (viewIndex + 1 < g_splitScreenActivePlayerCount) ? g_splitScreenActivePlayers[viewIndex + 1] : -1;
    }
}

static void G_SyncSinglePlayerSettingsFromPrimaryPlayer(void)
{
    ud.auto_run = ud.config.SplitScreenPlayerAlwaysRun[0] != 0;
}

static void G_UpdateSplitScreenMultiplayerState(void)
{
#ifdef SPLITSCREEN_MOD_HACKS
    g_fakeMultiMode = (g_splitScreenActivePlayerCount > 1) ? g_splitScreenActivePlayerCount : 0;
#endif

    int32_t const localPlayerLimit = max<int32_t>(g_splitScreenActivePlayerCount, G_GetHighestActiveSplitScreenPlayer() + 1);
    ud.multimode = localPlayerLimit;
    g_mostConcurrentPlayers = localPlayerLimit;

    if (g_splitScreenActivePlayerCount == 1)
        G_SyncSinglePlayerSettingsFromPrimaryPlayer();

    G_RebuildLocalConnectionChain();
}

static void G_SetPlayerSpriteVisible(int32_t const playerNum, int32_t const visible)
{
    if ((unsigned)playerNum >= MAXPLAYERS || g_player[playerNum].ps == nullptr)
        return;

    int32_t const spriteNum = g_player[playerNum].ps->i;
    if ((unsigned)spriteNum >= MAXSPRITES)
        return;

    if (visible)
        sprite[spriteNum].cstat = CSTAT_SPRITE_BLOCK + CSTAT_SPRITE_BLOCK_HITSCAN;
    else
        sprite[spriteNum].cstat = CSTAT_SPRITE_INVISIBLE;
}

static int32_t G_FindSplitScreenPlayerSprite(int32_t const playerNum)
{
    for (int32_t spriteNum = headspritestat[STAT_PLAYER]; spriteNum >= 0; spriteNum = nextspritestat[spriteNum])
    {
        if (sprite[spriteNum].picnum == APLAYER && sprite[spriteNum].yvel == playerNum)
            return spriteNum;
    }

    return -1;
}

static int32_t G_EnsureSplitScreenPlayerSprite(int32_t const playerNum)
{
    auto * const ps = g_player[playerNum].ps;
    if (ps == nullptr)
        return -1;

    int32_t spriteNum = ps->i;
    if ((unsigned)spriteNum < MAXSPRITES && sprite[spriteNum].statnum == STAT_PLAYER
        && sprite[spriteNum].picnum == APLAYER && sprite[spriteNum].yvel == playerNum)
        return spriteNum;

    spriteNum = G_FindSplitScreenPlayerSprite(playerNum);
    if (spriteNum >= 0)
    {
        ps->i = spriteNum;
        return spriteNum;
    }

    auto * const primary = g_player[myconnectindex].ps;
    if (primary == nullptr || primary->cursectnum < 0)
        return ps->i;

    spriteNum = A_InsertSprite(primary->cursectnum, primary->pos.x, primary->pos.y, primary->pos.z,
                               APLAYER, 0, 0, 0, fix16_to_int(primary->q16ang), 0, 0, 0, STAT_PLAYER);
    if ((unsigned)spriteNum >= MAXSPRITES)
        return ps->i;

    auto &s = sprite[spriteNum];
    s.yvel = playerNum;
    s.owner = spriteNum;
    actor[spriteNum].htowner = spriteNum;
    actor[spriteNum].bpos = s.xyz;
    ps->i = spriteNum;

    return spriteNum;
}

static void G_SetJoinedSplitScreenPlayerPosition(int32_t const playerNum, vec3_t const &pos, int32_t const sectNum, int32_t const ang)
{
    DukePlayer_t * const pPlayer = g_player[playerNum].ps;
    if (pPlayer == nullptr)
        return;

    pPlayer->opos = pPlayer->pos = pos;
    pPlayer->bobpos = pPlayer->pos.xy;
    pPlayer->cursectnum = sectNum;
    pPlayer->oq16ang = pPlayer->q16ang = fix16_from_int(ang);

    int32_t const spriteNum = pPlayer->i;
    if ((unsigned)spriteNum < MAXSPRITES)
    {
        vec3_t spritePos = pPlayer->pos;
        spritePos.z += pPlayer->spritezoffset;
        setsprite(spriteNum, &spritePos);
        actor[spriteNum].bpos = pPlayer->pos;
    }
}

static void G_MoveJoinedSplitScreenPlayerToCoopSpawn(int32_t const playerNum)
{
    if (g_playerSpawnCnt > 0)
    {
        auto const &primarySpawn = g_playerSpawnPoints[0];
        G_SetJoinedSplitScreenPlayerPosition(playerNum, primarySpawn.xyz, primarySpawn.sect, primarySpawn.ang);
        return;
    }

    DukePlayer_t const * const pAnchor = g_player[myconnectindex].ps;
    if (pAnchor == nullptr || pAnchor->cursectnum < 0)
        return;

    G_SetJoinedSplitScreenPlayerPosition(playerNum, pAnchor->pos, pAnchor->cursectnum, fix16_to_int(pAnchor->q16ang));
}

static void G_SetupJoinedSplitScreenPlayer(int32_t const playerNum)
{
    if ((unsigned)playerNum >= MAXPLAYERS || g_player[playerNum].ps == nullptr)
        return;

    if (g_player[myconnectindex].ps != nullptr && playerNum != myconnectindex)
    {
        int32_t const spriteNum = G_EnsureSplitScreenPlayerSprite(playerNum);
        Bmemcpy(g_player[playerNum].ps, g_player[myconnectindex].ps, sizeof(DukePlayer_t));
        g_player[playerNum].ps->i = spriteNum;
    }

    G_EnsureSplitScreenPlayerSprite(playerNum);

    Bstrncpyz(g_player[playerNum].user_name,
              playerNum == myconnectindex ? szPlayerName : ud.config.SplitScreenPlayerName[G_GetSplitScreenPlayerConfigIndex(playerNum)],
              sizeof(g_player[playerNum].user_name));
    g_player[playerNum].ps->team = g_player[playerNum].pteam = G_GetLocalPlayerTeamSetting(playerNum);
    g_player[playerNum].ps->palookup = g_player[playerNum].pcolor = G_GetLocalPlayerColorSetting(playerNum);
    g_player[playerNum].ps->weaponswitch = G_GetLocalPlayerWeaponSwitchSetting(playerNum);
    g_player[playerNum].ps->auto_aim = G_GetLocalPlayerAutoAimSetting(playerNum);

    P_ResetPlayer(playerNum);
    P_ResetMultiPlayer(playerNum);
    g_player[playerNum].ps->actors_killed = 0;
    g_player[playerNum].ps->secret_rooms = 0;
    if (playerNum != myconnectindex)
        g_player[playerNum].ps->got_access = 0;
    g_player[playerNum].ps->gm = MODE_GAME;
    G_MoveJoinedSplitScreenPlayerToCoopSpawn(playerNum);
    G_SetPlayerSpriteVisible(playerNum, 1);
}
}

static int32_t G_GetLegacySplitScreenPlayerCount(void)
{
    G_EnsureSplitScreenPlayerCountsInitialized();
    return max<int32_t>(g_splitScreenActivePlayerCount, 1);
}

int32_t G_HaveSplitScreen(void)
{
    return G_GetLegacySplitScreenPlayerCount() > 1;
}

int32_t G_GetConfiguredSplitScreenPlayerCount(void)
{
    G_EnsureSplitScreenPlayerCountsInitialized();
    return max<int32_t>(g_splitScreenConfiguredPlayerCount, 1);
}

int32_t G_GetSplitScreenPlayerCount(void)
{
    return G_GetLegacySplitScreenPlayerCount();
}

int32_t G_GetSplitScreenPlayer(int32_t const viewIndex)
{
    if ((unsigned)viewIndex >= (unsigned)G_GetSplitScreenPlayerCount())
        return -1;

    return g_splitScreenActivePlayers[viewIndex];
}

int32_t G_GetSplitScreenViewForPlayer(int32_t const playerNum)
{
    G_EnsureSplitScreenPlayerCountsInitialized();

    for (int viewIndex = 0; viewIndex < g_splitScreenActivePlayerCount; ++viewIndex)
        if (g_splitScreenActivePlayers[viewIndex] == playerNum)
            return viewIndex;

    return -1;
}

int32_t G_IsSplitScreenPlayerActive(int32_t const playerNum)
{
    return G_GetSplitScreenViewForPlayer(playerNum) >= 0;
}

void G_SetConfiguredSplitScreenPlayerCount(int32_t const playerCount)
{
    G_EnsureSplitScreenPlayerCountsInitialized();

    g_splitScreenConfiguredPlayerCount = clamp(playerCount, 1, MAX_LOCAL_SPLITSCREEN_PLAYERS);

    if (g_splitScreenActivePlayerCount > g_splitScreenConfiguredPlayerCount)
        G_SetActiveSplitScreenPlayerCount(g_splitScreenConfiguredPlayerCount);
}

void G_SetActiveSplitScreenPlayerCount(int32_t const playerCount)
{
    G_EnsureSplitScreenPlayerCountsInitialized();

    g_splitScreenActivePlayerCount = clamp(playerCount, 1, G_GetConfiguredSplitScreenPlayerCount());

    for (int i = 0; i < g_splitScreenActivePlayerCount; ++i)
        g_splitScreenActivePlayers[i] = i;

    G_UpdateSplitScreenMultiplayerState();
}

int32_t G_AddSplitScreenPlayer(int32_t const playerNum)
{
    G_EnsureSplitScreenPlayerCountsInitialized();

    if ((unsigned)playerNum >= (unsigned)MAX_LOCAL_SPLITSCREEN_PLAYERS || playerNum == myconnectindex)
        return -1;

    if (G_IsSplitScreenPlayerActive(playerNum) || g_splitScreenActivePlayerCount >= MAX_LOCAL_SPLITSCREEN_PLAYERS)
        return -1;

    int32_t const oldActivePlayerCount = g_splitScreenActivePlayerCount;

    g_splitScreenConfiguredPlayerCount = max(g_splitScreenConfiguredPlayerCount, playerNum + 1);
    g_splitScreenActivePlayers[g_splitScreenActivePlayerCount++] = playerNum;

    if (oldActivePlayerCount == 1)
        ud.m_coop = ud.coop = 1;

    G_UpdateSplitScreenMultiplayerState();
    G_SetupJoinedSplitScreenPlayer(playerNum);
    G_UpdateSplitScreenMultiplayerState();
    G_UpdateScreenArea();
    pub = NUMPAGES;
    pus = NUMPAGES;

    return playerNum;
}

int32_t G_DisconnectSplitScreenPlayer(int32_t const playerNum)
{
    G_EnsureSplitScreenPlayerCountsInitialized();

    int32_t const viewIndex = G_GetSplitScreenViewForPlayer(playerNum);
    if (playerNum == myconnectindex || viewIndex <= 0 || g_splitScreenActivePlayerCount <= 1)
        return -1;

    DukePlayer_t *const pDisconnected = g_player[playerNum].ps;
    DukePlayer_t *const pPrimary = g_player[myconnectindex].ps;
    if (pDisconnected != nullptr && pPrimary != nullptr)
    {
        pPrimary->got_access |= pDisconnected->got_access;
        pDisconnected->got_access = 0;
    }

    for (int i = viewIndex; i + 1 < g_splitScreenActivePlayerCount; ++i)
        g_splitScreenActivePlayers[i] = g_splitScreenActivePlayers[i + 1];

    --g_splitScreenActivePlayerCount;

    G_SetPlayerSpriteVisible(playerNum, 0);
    G_UpdateSplitScreenMultiplayerState();
    G_UpdateScreenArea();
    pub = NUMPAGES;
    pus = NUMPAGES;

    return 0;
}

int32_t G_GetSplitScreenViewport(int32_t const viewIndex, splitscreen_viewport_t * const viewport)
{
    if (viewport == nullptr)
        return -1;

    auto const playerCount = G_GetSplitScreenPlayerCount();
    if ((unsigned)viewIndex >= (unsigned)playerCount)
        return -1;

    *viewport = { 0, 0, xdim, ydim };

    switch (playerCount)
    {
        case 2:
        {
            auto const halfHeight = ydim / 2;
            viewport->y = (viewIndex == 0) ? 0 : halfHeight;
            viewport->height = (viewIndex == 0) ? halfHeight : (ydim - halfHeight);
            break;
        }

        case 3:
            if (viewIndex == 0)
            {
                auto const halfHeight = ydim / 2;
                viewport->height = halfHeight;
            }
            else
            {
                auto const halfWidth = xdim / 2;
                auto const halfHeight = ydim / 2;
                viewport->x = (viewIndex == 1) ? 0 : halfWidth;
                viewport->y = halfHeight;
                viewport->width = (viewIndex == 1) ? halfWidth : (xdim - halfWidth);
                viewport->height = ydim - halfHeight;
            }
            break;

        case 4:
        {
            auto const halfWidth = xdim / 2;
            auto const halfHeight = ydim / 2;
            viewport->x = (viewIndex & 1) ? halfWidth : 0;
            viewport->y = (viewIndex >> 1) ? halfHeight : 0;
            viewport->width = (viewIndex & 1) ? (xdim - halfWidth) : halfWidth;
            viewport->height = (viewIndex >> 1) ? (ydim - halfHeight) : halfHeight;
            break;
        }

        default:
            break;
    }

    return 0;
}
