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

#ifndef splitscreen_input_h_
#define splitscreen_input_h_

#include "compat.h"
#include "player.h"

extern void G_UpdateSplitScreenLocalInputs(void);
extern void G_FillSplitScreenInputsForTic(void);
extern void G_DrawSplitScreenConfigMenus(void);
extern int32_t G_CopySplitScreenLocalInput(int32_t playerNum, input_t *input);

#endif
