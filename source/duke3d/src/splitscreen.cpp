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
#include "mmulti.h"

namespace
{
static constexpr int32_t MAX_LOCAL_SPLITSCREEN_PLAYERS = 4;
static int32_t g_splitScreenConfiguredPlayerCount = 0;
static int32_t g_splitScreenActivePlayerCount = 0;

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
}

static void G_RebuildLocalConnectionChain(int32_t const playerCount)
{
    connecthead = 0;

    for (int i = 0; i < MAXPLAYERS; ++i)
    {
        connectpoint2[i] = -1;
        g_player[i].playerquitflag = i < playerCount;
    }

    for (int i = 0; i < playerCount - 1; ++i)
        connectpoint2[i] = i + 1;
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

    return viewIndex;
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

#ifdef SPLITSCREEN_MOD_HACKS
    g_fakeMultiMode = (g_splitScreenActivePlayerCount > 1) ? g_splitScreenActivePlayerCount : 0;
#endif

    G_RebuildLocalConnectionChain(g_splitScreenActivePlayerCount);
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
