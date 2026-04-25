//-------------------------------------------------------------------------
/*
Copyright (C) 2016 EDuke32 developers and contributors

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


#ifndef cmdline_h__
#define cmdline_h__

#include "compat.h"

void G_CheckCommandLine(int argc, char const* const* argv);

extern int32_t g_commandSetup;
extern int32_t g_noSetup;
extern int32_t g_noAutoLoad;
extern int32_t g_noSound;
extern int32_t g_noMusic;
extern int32_t g_noLogoAnim;
extern int32_t g_noLogo;

extern int32_t g_keepAddr;

extern const char* CommandMap;
extern const char* CommandName;
extern int32_t CommandWeaponChoice;

extern int32_t netparamcount;
extern char** netparam;

extern char firstdemofile[80];
extern int32_t userconfiles;

#endif // cmdline_h__