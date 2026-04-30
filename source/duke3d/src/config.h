//-------------------------------------------------------------------------
/*
Copyright (C) 2010 EDuke32 developers and contributors

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

#ifndef config_public_h_
#define config_public_h_

#include "function.h"

int  CONFIG_ReadSetup(void);
void CONFIG_WriteSetup(uint32_t flags);
void CONFIG_ReadSettings(void);
void CONFIG_SetDefaults(void);
void CONFIG_SetupMouse(void);
void CONFIG_SetupJoystick(void);
void CONFIG_SetDefaultKeys(const char (*keyptr)[MAXGAMEFUNCLEN], bool lazy=false);
void CONFIG_MigrateModernKeyboardDefaults(void);

void CONFIG_SetGameControllerDefaults(void);
void CONFIG_SetGameControllerDefaultsClear(void);
void CONFIG_SetSplitScreenGameControllerDefaults(int32_t controllerIndex);
int32_t CONFIG_NormalizeControllerViewCentering(int32_t viewCentering);
int32_t CONFIG_AdjustControllerViewCentering(int32_t viewCentering, int32_t direction);
char const *CONFIG_GetControllerViewCenteringName(int32_t viewCentering);

int32_t CONFIG_GetMapBestTime(char const *mapname, uint8_t const *mapmd4);
int     CONFIG_SetMapBestTime(uint8_t const *mapmd4, int32_t tm);
int32_t CONFIG_GetSplitScreenLevelProgress(int32_t addonNum, int32_t volumeNum, int32_t levelNum, int32_t *played, int32_t *secrets, int32_t *maxSecrets, int32_t *bestTime);
int     CONFIG_SetSplitScreenLevelProgress(int32_t addonNum, int32_t volumeNum, int32_t levelNum, int32_t secrets, int32_t maxSecrets, int32_t bestTime);

int32_t CONFIG_FunctionNameToNum(const char *func);

int32_t     CONFIG_AnalogNameToNum(const char *func);

void CONFIG_MapKey(int which, kb_scancode key1, kb_scancode oldkey1, kb_scancode key2, kb_scancode oldkey2);

const char * CONFIG_GetGameFuncOnKeyboard(int gameFunc);
const char * CONFIG_GetGameFuncOnMouse(int gameFunc);
const char * CONFIG_GetGameFuncOnJoystick(int gameFunc);

#endif
