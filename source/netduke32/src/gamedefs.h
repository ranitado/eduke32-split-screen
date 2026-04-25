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

//****************************************************************************
//
// gamedefs.h
//
// common defines between the game and the setup program
//
//****************************************************************************

#ifndef _gamedefs_public_
#define _gamedefs_public_
#ifdef __cplusplus
extern "C" {
#endif

//****************************************************************************
//
// DEFINES
//
//****************************************************************************

// KEEPINSYNC mact/include/_control.h, build/src/sdlayer.cpp
#define MAXJOYBUTTONS 32
#define MAXJOYBUTTONSANDHATS (MAXJOYBUTTONS+4)

// KEEPINSYNC mact/include/_control.h, build/src/sdlayer.cpp
#define MAXMOUSEAXES 2

// KEEPINSYNC mact/include/_control.h, build/src/sdlayer.cpp
#define MAXJOYAXES 9
#define MAXJOYDIGITAL (MAXJOYAXES*2)

// default mouse scale
#define DEFAULTMOUSEANALOGUESCALE           65536

#ifdef __cplusplus
};
#endif
#endif

