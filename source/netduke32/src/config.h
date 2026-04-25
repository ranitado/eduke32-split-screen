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

#ifndef _config_public
#define _config_public

#define SETUPNAMEPARM "SETUPFILE"

int32_t CONFIG_ReadSetup( void );
void CONFIG_GetSetupFilename( void );
void CONFIG_WriteSetup(uint32_t flags);
void CONFIG_ReadSettings(void);
void CONFIG_SetupMouse( void );
void CONFIG_SetupJoystick( void );
void CONFIG_SetDefaultKeys(const char(*keyptr)[MAXGAMEFUNCLEN], bool lazy = false);

int32_t CONFIG_GetMapBestTime(char const* const mapname, uint8_t const* const mapmd4);
int32_t CONFIG_SetMapBestTime(uint8_t const* const mapmd4, int32_t tm);

void CONFIG_MapKey(int32_t which, kb_scancode key1, kb_scancode oldkey1, kb_scancode key2, kb_scancode oldkey2);

const char* CONFIG_AnalogNumToName(int32_t func);
int32_t CONFIG_AnalogNameToNum(const char* func);

char const* CONFIG_FunctionNumToName(int32_t func);
int32_t CONFIG_FunctionNameToNum(const char* func);

#endif
