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

/*
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
*/

#include "renderlayer.h"
#include "duke3d.h"
#include "scriplib.h"
#include "osd.h"
#include "osdcmds.h"
#include "osdfuncs.h"

// we load this in to get default button and key assignments
// as well as setting up function mappings

#define __SETUP__   // JBF 20031211
#include "_functio.h"

/*
===================
=
= CONFIG_FunctionNameToNum
=
===================
*/

hashtable_t h_gamefuncs    = { NUMGAMEFUNCTIONS<<1, NULL };

int32_t CONFIG_FunctionNameToNum(const char* func)
{
    if (!func)
        return -1;

    return hash_findcase(&h_gamefuncs, func);
}

/*
===================
=
= CONFIG_FunctionNumToName
=
===================
*/

char const* CONFIG_FunctionNumToName(int32_t func)
{
    if ((unsigned)func >= (unsigned)NUMGAMEFUNCTIONS)
        return "";
    return gamefunctions[func];
}

/*
===================
=
= CONFIG_AnalogNameToNum
=
===================
*/


int32_t CONFIG_AnalogNameToNum(const char * func)
{

    if (!Bstrcasecmp(func,"analog_turning"))
    {
        return analog_turning;
    }
    if (!Bstrcasecmp(func,"analog_strafing"))
    {
        return analog_strafing;
    }
    if (!Bstrcasecmp(func,"analog_moving"))
    {
        return analog_moving;
    }
    if (!Bstrcasecmp(func,"analog_lookingupanddown"))
    {
        return analog_lookingupanddown;
    }

    return -1;
}


const char * CONFIG_AnalogNumToName(int32_t func)
{
    switch (func)
    {
    case analog_turning:
        return "analog_turning";
    case analog_strafing:
        return "analog_strafing";
    case analog_moving:
        return "analog_moving";
    case analog_lookingupanddown:
        return "analog_lookingupanddown";
    }

    return NULL;
}


/*
===================
=
= CONFIG_SetDefaults
=
===================
*/

void CONFIG_SetDefaultKeys(const char(*keyptr)[MAXGAMEFUNCLEN], bool lazy/*=false*/)
{
    static char const s_gamefunc_[] = "gamefunc_";
    int constexpr strlen_gamefunc_ = ARRAY_SIZE(s_gamefunc_) - 1;

    if (!lazy)
    {
        Bmemset(ud.config.KeyboardKeys, 0xff, sizeof(ud.config.KeyboardKeys));
        CONTROL_ClearAllBinds();
    }

    for (int i = 0; i < ARRAY_SSIZE(gamefunctions); ++i)
    {
        if (gamefunctions[i][0] == '\0')
            continue;

        auto& key = ud.config.KeyboardKeys[i];

        int const default0 = KB_StringToScanCode(keyptr[i << 1]);
        int const default1 = KB_StringToScanCode(keyptr[(i << 1) + 1]);

        // skip the function if the default key is already used
        // or the function is assigned to another key
        if (lazy && (key[0] != 0xff || (CONTROL_KeyIsBound(default0) && Bstrlen(CONTROL_KeyBinds[default0].cmdstr) > strlen_gamefunc_
            && CONFIG_FunctionNameToNum(CONTROL_KeyBinds[default0].cmdstr + strlen_gamefunc_) >= 0)))
        {
#if 0 // defined(DEBUGGINGAIDS)
            if (key[0] != 0xff)
                LOG_F("Skipping %s bound to %s", keyptr[i << 1], CONTROL_KeyBinds[default0].cmdstr);
#endif
            continue;
        }

        key[0] = default0;
        key[1] = default1;

        if (key[0])
            CONTROL_FreeKeyBind(key[0]);

        if (key[1])
            CONTROL_FreeKeyBind(key[1]);

        if (i == gamefunc_Show_Console)
            OSD_CaptureKey(key[0]);
        else
            CONFIG_MapKey(i, key[0], 0, key[1], 0);
    }
}

void CONFIG_SetDefaults(void)
{
    // JBF 20031211

    ud.config.scripthandle = -1;
    ud.config.ScreenWidth = 1024;
    ud.config.ScreenHeight = 768;
    ud.config.ScreenMode = 0;
    ud.config.ScreenBPP = 32;
    ud.config.useprecache = 1;
    ud.config.ForceSetup = 1;
    ud.config.AmbienceToggle = 1;
    ud.config.AutoAim = 2;
    ud.config.FXDevice = 0;
    ud.config.FXVolume = 220;
#if defined(_WIN32)
    ud.config.MixRate = 44100;
#else
    ud.config.MixRate = 48000;
#endif
    ud.config.MouseBias = 0;
#if defined(_WIN32)
    ud.config.MusicDevice = ASS_WinMM;
#else
    ud.config.MusicDevice = ASS_AutoDetect;
#endif
    ud.config.MusicToggle = 1;
    ud.config.MusicVolume = 195;
    g_myAimMode = g_player[0].ps->aim_mode = 1;
    ud.config.NumBits = 16;
    ud.config.NumChannels = 2;
    ud.config.NumVoices = 96;
    //ud.config.ReverseStereo = 0;
    ud.auto_run = 1;
    //ud.config.SmoothInput = 0;
    ud.config.SoundToggle = 1;
    //ud.config.CheckForUpdates = 0;
    ud.config.PredictionDebug = 0;
    ud.althud = 1;
    ud.automsg = 0;
    ud.autovote = 0;
    ud.brightness = 8;
    ud.camerasprite = -1;
    ud.color = 0;
    ud.m_color = 0;
    ud.crosshair = 1;
    ud.crosshairscale = 50;
    ud.obituaries = 1;
    ud.democams = 1;
    ud.detail = 1;
    ud.drawweapon = 1;
    ud.fov = 90;
    ud.fta_on = 1;
    ud.idplayers = 1;
    ud.levelstats = 0;
    ud.lockout = 0;
    ud.mouseaiming = 0;
    ud.mouseflip = 1;
    ud.msgdisptime = 120;
    ud.pwlockout[0] = '\0';
    ud.runkey_mode = 0;
    ud.screen_size = 4;
    ud.screen_tilting = 1;
    ud.showweapons = 1;
    ud.shadows = 1;
    ud.statusbarmode = 0;
    ud.statusbarscale = 100;
    ud.team = 0;
    ud.viewbob = 1;
    ud.weaponsway = 1;
    ud.weaponswitch = 3;	// new+empty
    ud.angleinterpolation = 0;
    ud.config.UseJoystick = 0;
    ud.config.UseMouse = 1;
    ud.config.VoiceToggle = 5; // bitfield, 1 = local, 2 = dummy, 4 = other players in DM
    ud.display_bonus_screen = 1;
    ud.show_level_text = 1;
    ud.configversion = 0;
    ud.weaponscale = 100;
    ud.textscale = 200;

    Bstrcpy(ud.rtsname, G_DefaultRtsFile());

    if(!CommandName)
        Bstrcpy(szPlayerName, "Player");

    Bstrcpy(ud.ridecule[0], "An inspiration for birth control.");
    Bstrcpy(ud.ridecule[1], "You're gonna die for that!");
    Bstrcpy(ud.ridecule[2], "It hurts to be you.");
    Bstrcpy(ud.ridecule[3], "Lucky Son of a Bitch.");
    Bstrcpy(ud.ridecule[4], "Hmmm....Payback time.");
    Bstrcpy(ud.ridecule[5], "You bottom dwelling scum sucker.");
    Bstrcpy(ud.ridecule[6], "Damn, you're ugly.");
    Bstrcpy(ud.ridecule[7], "Ha ha ha...Wasted!");
    Bstrcpy(ud.ridecule[8], "You suck!");
    Bstrcpy(ud.ridecule[9], "AARRRGHHHHH!!!");

    // JBF 20031211

    CONFIG_SetDefaultKeys(keydefaults);

    memset(ud.config.MouseFunctions, -1, sizeof(ud.config.MouseFunctions));
    memset(ud.config.JoystickFunctions, -1, sizeof(ud.config.JoystickFunctions));
    memset(ud.config.JoystickDigitalFunctions, -1, sizeof(ud.config.JoystickDigitalFunctions));

    CONTROL_MouseSensitivity = DEFAULTMOUSESENSITIVITY;

    for (int i = 0; i < MAXMOUSEBUTTONS; i++)
    {
        ud.config.MouseFunctions[i][0] = CONFIG_FunctionNameToNum(mousedefaults[i]);
        CONTROL_MapButton(ud.config.MouseFunctions[i][0], i, 0, controldevice_mouse);
        if (i >= 4) continue;
        ud.config.MouseFunctions[i][1] = CONFIG_FunctionNameToNum(mouseclickeddefaults[i]);
        CONTROL_MapButton(ud.config.MouseFunctions[i][1], i, 1, controldevice_mouse);
    }

    for (int i = 0; i < MAXMOUSEAXES; i++)
    {
        ud.config.MouseDigitalFunctions[i][0] = CONFIG_FunctionNameToNum(mousedigitaldefaults[i * 2]);
        ud.config.MouseDigitalFunctions[i][1] = CONFIG_FunctionNameToNum(mousedigitaldefaults[i * 2 + 1]);
        //CONTROL_MapDigitalAxis(i, ud.config.MouseDigitalFunctions[i][0], 0, controldevice_mouse);
        //CONTROL_MapDigitalAxis(i, ud.config.MouseDigitalFunctions[i][1], 1, controldevice_mouse);
        ud.config.MouseAnalogueAxes[i] = CONFIG_AnalogNameToNum(mouseanalogdefaults[i]);
        //CONTROL_MapAnalogAxis(i, ud.config.MouseAnalogueAxes[i], controldevice_mouse);
    }

#if !defined GEKKO
    //CONFIG_SetGameControllerDefaultsStandard();
#else
    for (int i = 0; i < MAXJOYBUTTONSANDHATS; i++)
    {
        ud.config.JoystickFunctions[i][0] = CONFIG_FunctionNameToNum(joystickdefaults[i]);
        ud.config.JoystickFunctions[i][1] = CONFIG_FunctionNameToNum(joystickclickeddefaults[i]);
        CONTROL_MapButton(ud.config.JoystickFunctions[i][0], i, 0, controldevice_joystick);
        CONTROL_MapButton(ud.config.JoystickFunctions[i][1], i, 1, controldevice_joystick);
    }

    for (int i = 0; i < MAXJOYAXES; i++)
    {
        ud.config.JoystickAnalogueScale[i] = DEFAULTJOYSTICKANALOGUESCALE;
        ud.config.JoystickAnalogueInvert[i] = 0;
        ud.config.JoystickAnalogueDead[i] = DEFAULTJOYSTICKANALOGUEDEAD;
        ud.config.JoystickAnalogueSaturate[i] = DEFAULTJOYSTICKANALOGUESATURATE;
        CONTROL_SetAnalogAxisScale(i, ud.config.JoystickAnalogueScale[i], controldevice_joystick);
        CONTROL_SetAnalogAxisInvert(i, 0, controldevice_joystick);

        ud.config.JoystickDigitalFunctions[i][0] = CONFIG_FunctionNameToNum(joystickdigitaldefaults[i * 2]);
        ud.config.JoystickDigitalFunctions[i][1] = CONFIG_FunctionNameToNum(joystickdigitaldefaults[i * 2 + 1]);
        CONTROL_MapDigitalAxis(i, ud.config.JoystickDigitalFunctions[i][0], 0, controldevice_joystick);
        CONTROL_MapDigitalAxis(i, ud.config.JoystickDigitalFunctions[i][1], 1, controldevice_joystick);

        ud.config.JoystickAnalogueAxes[i] = CONFIG_AnalogNameToNum(joystickanalogdefaults[i]);
        CONTROL_MapAnalogAxis(i, ud.config.JoystickAnalogueAxes[i], controldevice_joystick);
    }
#endif

    X_OnEvent(EVENT_SETDEFAULTS, g_player[myconnectindex].ps->i, myconnectindex, -1);
}

// wrapper for CONTROL_MapKey(), generates key bindings to reflect changes to keyboard setup
void CONFIG_MapKey(int which, kb_scancode key1, kb_scancode oldkey1, kb_scancode key2, kb_scancode oldkey2)
{
    int const keys[] = { key1, key2, oldkey1, oldkey2 };
    char buf[2 * MAXGAMEFUNCLEN];

    if (which == gamefunc_Show_Console)
        OSD_CaptureKey(key1);

    for (int k = 0; (unsigned)k < ARRAY_SIZE(keys); k++)
    {
        if (keys[k] == 0xff || !keys[k])
            continue;

        int match = 0;

        for (; match < ARRAY_SSIZE(sctokeylut); ++match)
        {
            if (keys[k] == sctokeylut[match].sc)
                break;
        }

        tempbuf[0] = 0;

        for (int i = NUMGAMEFUNCTIONS - 1; i >= 0; i--)
        {
            if (ud.config.KeyboardKeys[i][0] == keys[k] || ud.config.KeyboardKeys[i][1] == keys[k])
            {
                Bsprintf(buf, "gamefunc_%s; ", CONFIG_FunctionNumToName(i));
                Bstrcat(tempbuf, buf);
            }
        }

        int const len = Bstrlen(tempbuf);

        if (len >= 2)
        {
            tempbuf[len - 2] = 0;  // cut off the trailing "; "
            CONTROL_BindKey(keys[k], tempbuf, 1, sctokeylut[match].key ? sctokeylut[match].key : "<?>");
        }
        else
        {
            CONTROL_FreeKeyBind(keys[k]);
        }
    }
}

/*
===================
=
= CONFIG_SetupMouse
=
===================
*/

void CONFIG_SetupMouse(void)
{
    if (ud.config.scripthandle < 0)
        return;

    char str[80];
    char temp[80];

    for (int i = 0; i < MAXMOUSEBUTTONS; i++)
    {
        Bsprintf(str, "MouseButton%d", i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str, temp))
            ud.config.MouseFunctions[i][0] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str, "MouseButtonClicked%d", i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str, temp))
            ud.config.MouseFunctions[i][1] = CONFIG_FunctionNameToNum(temp);
    }

    // map over the axes
    for (int i = 0; i < MAXMOUSEAXES; i++)
    {
        Bsprintf(str, "MouseAnalogAxes%d", i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str, temp))
            ud.config.MouseAnalogueAxes[i] = CONFIG_AnalogNameToNum(temp);

        Bsprintf(str, "MouseDigitalAxes%d_0", i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str, temp))
            if (CONFIG_FunctionNameToNum(temp) != -1 || (!temp[0] && CONFIG_FunctionNameToNum(temp) != -1))
                ud.config.MouseDigitalFunctions[i][0] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str, "MouseDigitalAxes%d_1", i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str, temp))
            if (CONFIG_FunctionNameToNum(temp) != -1 || (!temp[0] && CONFIG_FunctionNameToNum(temp) != -1))
                ud.config.MouseDigitalFunctions[i][1] = CONFIG_FunctionNameToNum(temp);
    }

    for (int i = 0; i < MAXMOUSEBUTTONS; i++)
    {
        CONTROL_MapButton(ud.config.MouseFunctions[i][0], i, 0, controldevice_mouse);
        CONTROL_MapButton(ud.config.MouseFunctions[i][1], i, 1, controldevice_mouse);
    }

    /*
    for (int i = 0; i < MAXMOUSEAXES; i++)
    {
        CONTROL_MapAnalogAxis(i, ud.config.MouseAnalogueAxes[i], controldevice_mouse);

        CONTROL_MapDigitalAxis(i, ud.config.MouseDigitalFunctions[i][0], 0, controldevice_mouse);
        CONTROL_MapDigitalAxis(i, ud.config.MouseDigitalFunctions[i][1], 1, controldevice_mouse);
    }
    */
}

/*
===================
=
= CONFIG_SetupJoystick
=
===================
*/

void CONFIG_SetupJoystick(void)
{
    int32_t i;
    char str[80];
    char temp[80];
    int32_t scale;

    if (ud.config.scripthandle < 0) return;

    for (i = 0; i < MAXJOYBUTTONSANDHATS; i++)
    {
        Bsprintf(str, "ControllerButton%d", i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str, temp))
            ud.config.JoystickFunctions[i][0] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str, "ControllerButtonClicked%d", i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str, temp))
            ud.config.JoystickFunctions[i][1] = CONFIG_FunctionNameToNum(temp);
    }

    // map over the axes
    for (i = 0; i < MAXJOYAXES; i++)
    {
        Bsprintf(str, "ControllerAnalogAxes%d", i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str, temp))
            ud.config.JoystickAnalogueAxes[i] = CONFIG_AnalogNameToNum(temp);

        Bsprintf(str, "ControllerDigitalAxes%d_0", i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str, temp))
            ud.config.JoystickDigitalFunctions[i][0] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str, "ControllerDigitalAxes%d_1", i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str, temp))
            ud.config.JoystickDigitalFunctions[i][1] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str, "ControllerAnalogScale%d", i);
        scale = ud.config.JoystickAnalogueScale[i];
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", str, &scale);
        ud.config.JoystickAnalogueScale[i] = scale;

        Bsprintf(str, "ControllerAnalogInvert%d", i);
        scale = ud.config.JoystickAnalogueInvert[i];
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", str, &scale);
        ud.config.JoystickAnalogueInvert[i] = scale;

        Bsprintf(str, "ControllerAnalogDead%d", i);
        scale = ud.config.JoystickAnalogueDead[i];
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", str, &scale);
        ud.config.JoystickAnalogueDead[i] = scale;

        Bsprintf(str, "ControllerAnalogSaturate%d", i);
        scale = ud.config.JoystickAnalogueSaturate[i];
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", str, &scale);
        ud.config.JoystickAnalogueSaturate[i] = scale;
    }

    for (i = 0; i < MAXJOYBUTTONSANDHATS; i++)
    {
        CONTROL_MapButton(ud.config.JoystickFunctions[i][0], i, 0, controldevice_joystick);
        CONTROL_MapButton(ud.config.JoystickFunctions[i][1], i, 1, controldevice_joystick);
    }
    for (i = 0; i < MAXJOYAXES; i++)
    {
        CONTROL_MapAnalogAxis(i, ud.config.JoystickAnalogueAxes[i]);
        CONTROL_MapDigitalAxis(i, ud.config.JoystickDigitalFunctions[i][0], 0);
        CONTROL_MapDigitalAxis(i, ud.config.JoystickDigitalFunctions[i][1], 1);
        CONTROL_SetAnalogAxisScale(i, ud.config.JoystickAnalogueScale[i], controldevice_joystick);
        CONTROL_SetAnalogAxisInvert(i, ud.config.JoystickAnalogueInvert[i]);
    }
}

/*
===================
=
= CONFIG_ReadSetup
=
===================
*/
int32_t CONFIG_ReadSetup(void)
{
    int32_t dummy, i = 0;
    char commmacro[] = "CommbatMacro# ";
    char tempbuf[1024];

    CONTROL_ClearAssignments();
    CONFIG_SetDefaults();

    ud.config.setupread = 1;

    pathsearchmode = 1;
    if (buildvfs_exists(g_setupFileName) && ud.config.scripthandle < 0)  // JBF 20031211
        ud.config.scripthandle = SCRIPT_Load(g_setupFileName);
    else if (buildvfs_exists(SETUPFILENAME) && ud.config.scripthandle < 0)
    {
        Bsprintf(tempbuf,"The configuration file \"%s\" was not found. "
                 "Import configuration data from \"%s\"?",g_setupFileName,SETUPFILENAME);

        i = wm_ynbox("Import Configuration Settings", "%s", tempbuf);
        if (i) ud.config.scripthandle = SCRIPT_Load(SETUPFILENAME);
    }
    else if (buildvfs_exists("eduke32.cfg") && ud.config.scripthandle < 0)
    {
        Bsprintf(tempbuf,"The configuration file \"%s\" was not found. "
                 "Import configuration data from \"eduke32.cfg\"?",g_setupFileName);

        i = wm_ynbox("Import Configuration Settings", "%s", tempbuf);
        if (i) ud.config.scripthandle = SCRIPT_Load("eduke32.cfg");
    }
    else if (buildvfs_exists("duke3d.cfg") && ud.config.scripthandle < 0)
    {
        Bsprintf(tempbuf,"The configuration file \"%s\" was not found. "
                 "Import configuration data from \"duke3d.cfg\"?",g_setupFileName);

        i = wm_ynbox("Import Configuration Settings", "%s", tempbuf);
        if (i) ud.config.scripthandle = SCRIPT_Load("duke3d.cfg");
    }
    pathsearchmode = 0;

    if (ud.config.scripthandle < 0) return -1;

    if (ud.config.scripthandle >= 0)
    {
        char dummybuf[64];

        for (dummy = 0;dummy < 10;dummy++)
        {
            commmacro[13] = dummy+'0';
            SCRIPT_GetString(ud.config.scripthandle, "Comm Setup",commmacro,&ud.ridecule[dummy][0]);
        }

        if (!CommandName)
        {
            Bmemset(tempbuf, 0, sizeof(tempbuf));
            if (!SCRIPT_GetString(ud.config.scripthandle, "Comm Setup", "PlayerName", &tempbuf[0]))
            {
                if (tempbuf[0] != 0)
                {
                    while (Bstrlen(OSD_StripColors(dummybuf, tempbuf)) > 10)
                        tempbuf[Bstrlen(tempbuf) - 1] = '\0';

                    Bstrncpyz(szPlayerName, tempbuf, sizeof(szPlayerName));
                }
            }
        }

        SCRIPT_GetString(ud.config.scripthandle, "Comm Setup","RTSName",&ud.rtsname[0]);

        SCRIPT_GetNumber(ud.config.scripthandle, "Setup","ConfigVersion",&ud.configversion);
        SCRIPT_GetNumber(ud.config.scripthandle, "Setup","ForceSetup",&ud.config.ForceSetup);

#ifdef STARTUP_SETUP_WINDOW
        if (g_noSetup == 0 && g_modDir[0] == '/')
        {
            struct stat st;
            SCRIPT_GetString(ud.config.scripthandle, "Setup","ModDir",&g_modDir[0]);

            if (stat(g_modDir, &st) >= 0)
            {
                if ((st.st_mode & S_IFDIR) != S_IFDIR)
                {
                    LOG_F(ERROR, "Invalid mod dir in cfg!");
                    Bsprintf(g_modDir,"/");
                }
            }
        }
#endif

        if (g_grpNamePtr == NULL && g_addonNum == 0)
        {
            SCRIPT_GetStringPtr(ud.config.scripthandle, "Setup", "SelectedGRP", &g_grpNamePtr);
            if (g_grpNamePtr && !Bstrlen(g_grpNamePtr))
                g_grpNamePtr = dup_filename(G_DefaultGrpFile());
        }

        {
            tempbuf[0] = 0;
            SCRIPT_GetString(ud.config.scripthandle, "Screen Setup", "AmbientLight",&tempbuf[0]);
            if (atof(tempbuf))
            {
                r_ambientlight = atof(tempbuf);
                r_ambientlightrecip = 1.f/r_ambientlight;
            }
        }

        SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "Detail",&ud.detail);

        SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "Messages",&ud.fta_on);

        if (!NAM)
        {
            SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "Out",&ud.lockout);
            SCRIPT_GetString(ud.config.scripthandle, "Screen Setup","Password",&ud.pwlockout[0]);
        }

        SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "ScreenGamma",&ud.brightness);
        SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "ScreenHeight",&ud.config.ScreenHeight);
        SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "ScreenMode",&ud.config.ScreenMode);
        SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "ScreenSize",&ud.screen_size);
        SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "ScreenWidth",&ud.config.ScreenWidth);
        SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "Tilt",&ud.screen_tilting);

        vec2_t windowPos;
        if (!SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "WindowPosX", &windowPos.x)
            && !SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "WindowPosY", &windowPos.y))
        {
            g_windowPos = windowPos;
            g_windowPosValid = true;
        }

        SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "MaxRefreshFreq", (int32_t*)&maxrefreshfreq);

#if defined(USE_OPENGL)
        SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "GLAnimationSmoothing", &r_animsmoothing);
        SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "GLTextureMode", &gltexfiltermode);
        SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "GLUseTextureCompr", &glusetexcompr);
        SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "OSDHightile",&osdhightile);

        {
            SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "ScreenBPP", &ud.config.ScreenBPP);
            if (ud.config.ScreenBPP < 8) ud.config.ScreenBPP = 8;
        }

        {
            dummy = usemodels;
            SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "UseModels",&dummy);
            usemodels = dummy != 0;
        }

#ifdef POLYMER
        int32_t rendmode = 0;
        SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "Polymer", &rendmode);
        glrendmode = (rendmode > 0) ? REND_POLYMER : REND_POLYMOST;
#endif
#endif

        SCRIPT_GetNumber(ud.config.scripthandle, "Misc", "Executions",&ud.executions);

        dummy = ud.config.useprecache;
        SCRIPT_GetNumber(ud.config.scripthandle, "Misc", "UsePrecache",&dummy);
        ud.config.useprecache = dummy != 0;

        SCRIPT_GetNumber(ud.config.scripthandle, "Misc", "PredictionDebug", &ud.config.PredictionDebug);

        SCRIPT_GetNumber(ud.config.scripthandle, "Sound Setup", "FXDevice",&ud.config.FXDevice);
        //SCRIPT_GetNumber(ud.config.scripthandle, "Sound Setup", "ReverseStereo",&ud.config.ReverseStereo);

        SCRIPT_GetNumber(ud.config.scripthandle, "Controls","UseJoystick",&ud.config.UseJoystick);
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls","UseMouse",&ud.config.UseMouse);

#ifdef _WIN32
        //SCRIPT_GetNumber(ud.config.scripthandle, "Updates", "CheckForUpdates", &ud.config.CheckForUpdates);
        //SCRIPT_GetNumber(ud.config.scripthandle, "Updates", "LastUpdateCheck", &ud.config.LastUpdateCheck);
#endif

    }

    //CONFIG_SetupMouse(ud.config.scripthandle);
    //CONFIG_SetupJoystick(ud.config.scripthandle);
    ud.config.setupread = 1;
    return 0;
}

/*
===================
=
= CONFIG_WriteSetup
=
===================
*/

void CONFIG_ReadSettings(void)
{
    char* const setupFileName = Xstrdup(g_setupFileName);
    char* const p = strtok(setupFileName, ".");

    Bsprintf(tempbuf, "%s_settings.cfg", p);

    Xfree(setupFileName);

    OSD_Exec(tempbuf);

    ud.config.setupread = 2;

    return;
}

void CONFIG_WriteSettings(void) // save binds and aliases to <cfgname>_settings.cfg
{
    if (ud.config.setupread != 2) return;

    char filename[BMAX_PATH];

    {
        char* ptr = Xstrdup(g_setupFileName);
        Bsprintf(filename, "%s_settings.cfg", strtok(ptr, "."));
        Xfree(ptr);
    }

    buildvfs_FILE fp = buildvfs_fopen_write(filename);

    if (fp)
    {
        buildvfs_fputstr(fp, "// this file is automatically generated by ");
        buildvfs_fputstrptr(fp, AppProperName);
        buildvfs_fputstr(fp, "\nunbindall\n");

        for (int i = 0; i < MAXBOUNDKEYS + MAXMOUSEBUTTONS; i++)
        {
            if (CONTROL_KeyIsBound(i))
            {
                buildvfs_fputstr(fp, "bind \"");
                buildvfs_fputstrptr(fp, CONTROL_KeyBinds[i].key);
                if (CONTROL_KeyBinds[i].repeat)
                    buildvfs_fputstr(fp, "\" \"");
                else
                    buildvfs_fputstr(fp, "\" norepeat \"");
                buildvfs_fputstrptr(fp, CONTROL_KeyBinds[i].cmdstr);
                buildvfs_fputstr(fp, "\"\n");
            }
        }

        for (int i = 0; i < NUMGAMEFUNCTIONS; ++i)
        {
            char const* name = CONFIG_FunctionNumToName(i);
            if (name && name[0] != '\0' && (ud.config.KeyboardKeys[i][0] == 0xff || !ud.config.KeyboardKeys[i][0]))
            {
                buildvfs_fputstr(fp, "unbound ");
                buildvfs_fputstrptr(fp, name);
                buildvfs_fputstr(fp, "\n");
            }
        }

        OSD_WriteAliases(fp);

        if (g_crosshairSum != -1 && g_crosshairSum != DefaultCrosshairColors.r + (DefaultCrosshairColors.g << 8) + (DefaultCrosshairColors.b << 16))
        {
            buildvfs_fputstr(fp, "crosshaircolor ");
            char buf[64];
            snprintf(buf, sizeof(buf), "%d %d %d\n", CrosshairColors.r, CrosshairColors.g, CrosshairColors.b);
            buildvfs_fputstrptr(fp, buf);
        }

        OSD_WriteCvars(fp);

        buildvfs_fclose(fp);

        OSD_Printf("Wrote %s\n", filename);

        return;
    }

    OSD_Printf("Error writing %s: %s\n", filename, strerror(errno));
}

void CONFIG_WriteSetup(uint32_t flags)
{
    int32_t dummy;
    char tempbuf[1024];

    if (!ud.config.setupread) return;

    if (ud.config.scripthandle < 0)
        ud.config.scripthandle = SCRIPT_Init(g_setupFileName);

    SCRIPT_PutNumber(ud.config.scripthandle, "Misc", "Executions", ++ud.executions, false, false);
    SCRIPT_PutNumber(ud.config.scripthandle, "Setup", "ConfigVersion", BYTEVERSION_NETDUKE32, false, false);
    SCRIPT_PutNumber(ud.config.scripthandle, "Setup", "ForceSetup", ud.config.ForceSetup, false, false);

    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "ScreenBPP", ud.config.ScreenBPP, false, false);  // JBF 20040523
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "ScreenWidth", ud.config.ScreenWidth, false, false);  // JBF 20031206
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "ScreenHeight", ud.config.ScreenHeight, false, false);    // JBF 20031206
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "ScreenMode", ud.config.ScreenMode, false, false);    // JBF 20031206

#ifdef POLYMER
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "Polymer", glrendmode == REND_POLYMER, FALSE, FALSE);
#endif

    SCRIPT_PutString(ud.config.scripthandle, "Setup", "SelectedGRP", g_grpNamePtr);

#ifdef STARTUP_SETUP_WINDOW
    if (g_noSetup == 0)
        SCRIPT_PutString(ud.config.scripthandle, "Setup", "ModDir", &g_modDir[0]);
#endif

    // exit early after only updating the values that can be changed from the startup window
    if (flags & 1)
    {
        SCRIPT_Save(ud.config.scripthandle, g_setupFileName);
        SCRIPT_Free(ud.config.scripthandle);
        return;
    }

    SCRIPT_PutNumber(ud.config.scripthandle, "Controls","UseJoystick",ud.config.UseJoystick,false,false);
    SCRIPT_PutNumber(ud.config.scripthandle, "Controls","UseMouse",ud.config.UseMouse,false,false);
    
    SCRIPT_PutNumber(ud.config.scripthandle, "Misc", "PredictionDebug", ud.config.PredictionDebug, false, false);
    SCRIPT_PutNumber(ud.config.scripthandle, "Misc", "UsePrecache",ud.config.useprecache,false,false);

    {
        Bsprintf(tempbuf,"%.2f",r_ambientlight);
        SCRIPT_PutString(ud.config.scripthandle, "Screen Setup", "AmbientLight",tempbuf);
    }

    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "Detail",ud.detail,false,false);
#if defined(USE_OPENGL)
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "GLAnimationSmoothing",r_animsmoothing,false,false);
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "GLTextureMode",gltexfiltermode,false,false);
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "GLUseTextureCompr",glusetexcompr,false,false);
#endif

    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "MaxRefreshFreq",maxrefreshfreq,false,false);

    if (g_windowPosValid)
    {
        SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "WindowPosX", g_windowPos.x, FALSE, FALSE);
        SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "WindowPosY", g_windowPos.y, FALSE, FALSE);
    }

    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "Messages",ud.fta_on,false,true);

    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "OSDHightile",osdhightile,false,false);

    if (!NAM)
    {
        SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "Out",ud.lockout,false,false);
        SCRIPT_PutString(ud.config.scripthandle, "Screen Setup", "Password",ud.pwlockout);
    }

    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "ScreenGamma",ud.brightness,false,false);
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "ScreenSize",ud.screen_size,false,false);
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "Tilt",ud.screen_tilting,false,false);

#if defined(USE_OPENGL)
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "UseModels",usemodels,false,false);
#endif

    //SCRIPT_PutNumber(ud.config.scripthandle, "Sound Setup", "ReverseStereo",ud.config.ReverseStereo,false,false);

#ifdef _WIN32
    //SCRIPT_PutNumber(ud.config.scripthandle, "Updates", "CheckForUpdates", ud.config.CheckForUpdates, false, false);
    //SCRIPT_PutNumber(ud.config.scripthandle, "Updates", "LastUpdateCheck", ud.config.LastUpdateCheck, false, false);
#endif

    // MOUSE
    for (int i = 0; i < MAXMOUSEBUTTONS; i++)
    {
        if (CONFIG_FunctionNumToName(ud.config.MouseFunctions[i][0]))
        {
            Bsprintf(buf, "MouseButton%d", i);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.MouseFunctions[i][0]));
        }

        if (i >= (MAXMOUSEBUTTONS - 2)) continue;

        if (CONFIG_FunctionNumToName(ud.config.MouseFunctions[i][1]))
        {
            Bsprintf(buf, "MouseButtonClicked%d", i);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.MouseFunctions[i][1]));
        }
    }

    for (int i = 0; i < MAXMOUSEAXES; i++)
    {
        if (CONFIG_AnalogNumToName(ud.config.MouseAnalogueAxes[i]))
        {
            Bsprintf(buf, "MouseAnalogAxes%d", i);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_AnalogNumToName(ud.config.MouseAnalogueAxes[i]));
        }

        if (CONFIG_FunctionNumToName(ud.config.MouseDigitalFunctions[i][0]))
        {
            Bsprintf(buf, "MouseDigitalAxes%d_0", i);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.MouseDigitalFunctions[i][0]));
        }

        if (CONFIG_FunctionNumToName(ud.config.MouseDigitalFunctions[i][1]))
        {
            Bsprintf(buf, "MouseDigitalAxes%d_1", i);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.MouseDigitalFunctions[i][1]));
        }
    }

    // JOYSTICK
    for (int dummy = 0; dummy < MAXJOYBUTTONSANDHATS; dummy++)
    {
        if (CONFIG_FunctionNumToName(ud.config.JoystickFunctions[dummy][0]))
        {
            Bsprintf(buf, "ControllerButton%d", dummy);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.JoystickFunctions[dummy][0]));
        }

        if (CONFIG_FunctionNumToName(ud.config.JoystickFunctions[dummy][1]))
        {
            Bsprintf(buf, "ControllerButtonClicked%d", dummy);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.JoystickFunctions[dummy][1]));
        }
    }
    for (int dummy = 0; dummy < MAXJOYAXES; dummy++)
    {
        if (CONFIG_AnalogNumToName(ud.config.JoystickAnalogueAxes[dummy]))
        {
            Bsprintf(buf, "ControllerAnalogAxes%d", dummy);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_AnalogNumToName(ud.config.JoystickAnalogueAxes[dummy]));
        }

        if (CONFIG_FunctionNumToName(ud.config.JoystickDigitalFunctions[dummy][0]))
        {
            Bsprintf(buf, "ControllerDigitalAxes%d_0", dummy);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.JoystickDigitalFunctions[dummy][0]));
        }

        if (CONFIG_FunctionNumToName(ud.config.JoystickDigitalFunctions[dummy][1]))
        {
            Bsprintf(buf, "ControllerDigitalAxes%d_1", dummy);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.JoystickDigitalFunctions[dummy][1]));
        }

        Bsprintf(buf, "ControllerAnalogScale%d", dummy);
        SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.JoystickAnalogueScale[dummy], FALSE, FALSE);

        Bsprintf(buf, "ControllerAnalogInvert%d", dummy);
        SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.JoystickAnalogueInvert[dummy], FALSE, FALSE);

        Bsprintf(buf, "ControllerAnalogDead%d", dummy);
        SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.JoystickAnalogueDead[dummy], FALSE, FALSE);

        Bsprintf(buf, "ControllerAnalogSaturate%d", dummy);
        SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.JoystickAnalogueSaturate[dummy], FALSE, FALSE);
    }

    if(!CommandName && szPlayerName[0] != '\0')
        SCRIPT_PutString(ud.config.scripthandle, "Comm Setup","PlayerName",&szPlayerName[0]);

    SCRIPT_PutString(ud.config.scripthandle, "Comm Setup","RTSName",&ud.rtsname[0]);

    {
        char commmacro[] = "CommbatMacro# ";

        for (dummy = 0;dummy < 10;dummy++)
        {
            commmacro[13] = dummy+'0';
            SCRIPT_PutString(ud.config.scripthandle, "Comm Setup",commmacro,&ud.ridecule[dummy][0]);
        }
    }

    SCRIPT_Save(ud.config.scripthandle, g_setupFileName);
    SCRIPT_Free(ud.config.scripthandle);
    OSD_Printf("Wrote %s\n",g_setupFileName);
    CONFIG_WriteSettings();
    Bfflush(NULL);
}

static const char* CONFIG_GetMapEntryName(char m[], char const* const mapname)
{
    strcpy(m, mapname);

    char* p = strrchr(m, '/');
    if (!p) p = strrchr(m, '\\');
    if (p) Bmemmove(m, p, Bstrlen(p) + 1);
    for (p = m; *p; p++) *p = tolower(*p);

    // cheap hack because SCRIPT_GetNumber doesn't like the slashes
    p = m;
    while (*p == '/') p++;

    return p;
}

static void CONFIG_GetMD4EntryName(char m[], uint8_t const* const md4)
{
    sprintf(m, "MD4_%08x%08x%08x%08x",
        B_BIG32(B_UNBUF32(&md4[0])), B_BIG32(B_UNBUF32(&md4[4])),
        B_BIG32(B_UNBUF32(&md4[8])), B_BIG32(B_UNBUF32(&md4[12])));
}

int32_t CONFIG_GetMapBestTime(char const* const mapname, uint8_t const* const mapmd4)
{
    if (!ud.config.setupread || ud.config.scripthandle < 0)
        return -1;

    char m[37];

    CONFIG_GetMD4EntryName(m, mapmd4);

    int32_t t = -1;
    if (SCRIPT_GetNumber(ud.config.scripthandle, "MapTimes", m, &t))
    {
        // fall back to map filenames
        char m2[BMAX_PATH];
        auto p = CONFIG_GetMapEntryName(m2, mapname);

        SCRIPT_GetNumber(ud.config.scripthandle, "MapTimes", p, &t);
    }

    return t;
}

int32_t CONFIG_SetMapBestTime(uint8_t const* const mapmd4, int32_t tm)
{
    if (ud.config.scripthandle < 0 && (ud.config.scripthandle = SCRIPT_Init(g_setupFileName)) < 0)
        return -1;

    char m[37];

    CONFIG_GetMD4EntryName(m, mapmd4);
    SCRIPT_PutNumber(ud.config.scripthandle, "MapTimes", m, tm, FALSE, FALSE);

    return 0;
}

/*
 * vim:ts=4:sw=4:
 */

