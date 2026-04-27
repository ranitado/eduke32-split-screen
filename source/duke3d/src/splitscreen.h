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

#ifndef splitscreen_h_
#define splitscreen_h_

#include "compat.h"

typedef struct splitscreen_viewport_s
{
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} splitscreen_viewport_t;

extern int32_t G_HaveSplitScreen(void);
extern int32_t G_GetConfiguredSplitScreenPlayerCount(void);
extern int32_t G_GetSplitScreenPlayerCount(void);
extern int32_t G_GetSplitScreenPlayer(int32_t viewIndex);
extern int32_t G_GetSplitScreenViewForPlayer(int32_t playerNum);
extern int32_t G_IsSplitScreenPlayerActive(int32_t playerNum);
extern int32_t G_AddSplitScreenPlayer(int32_t playerNum);
extern int32_t G_DisconnectSplitScreenPlayer(int32_t playerNum);
extern int32_t G_GetSplitScreenViewport(int32_t viewIndex, splitscreen_viewport_t *viewport);
extern void G_SetConfiguredSplitScreenPlayerCount(int32_t playerCount);
extern void G_SetActiveSplitScreenPlayerCount(int32_t playerCount);

#endif
