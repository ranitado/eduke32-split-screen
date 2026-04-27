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

#include "duke3d.h"
#include "scriplib.h"
#include "osdcmds.h"
#include "renderlayer.h"
#include "cmdline.h"

#include "vfs.h"

#ifdef __ANDROID__
# include "android.h"
#endif

#if defined RENDERTYPESDL && defined SDL_TARGET && SDL_TARGET > 1
# include "sdl_inc.h"
#endif

// we load this in to get default button and key assignments
// as well as setting up function mappings

#define __SETUP__   // JBF 20031211
#include "_functio.h"

hashtable_t h_gamefuncs    = { NUMGAMEFUNCTIONS<<1, NULL };

static float constexpr CONFIG_CONTROLLER_AIM_SENS_X = 4.5f;
static float constexpr CONFIG_CONTROLLER_AIM_SENS_Y = 4.5f;
static int32_t constexpr CONFIG_CONTROLLER_VIEW_CENTERING_DEFAULT = 0;
static int32_t constexpr CONFIG_CONTROLLER_VIEW_CENTERING_SLOW = 1;
static int32_t constexpr CONFIG_CONTROLLER_VIEW_CENTERING_MED = 5;
static int32_t constexpr CONFIG_CONTROLLER_VIEW_CENTERING_FAST = 8;
static int32_t constexpr CONFIG_CONTROLLER_VIEW_CENTERING_LEGACY_DEFAULT = 4;
static int32_t g_controllerViewCenteringDefaultMigrated = 0;

static void CONFIG_MigrateModernControllerDefaults(void);

static bool CONFIG_GetDesktopVideoMode(int32_t const displayIndex, int32_t * const width, int32_t * const height)
{
#if !defined __ANDROID__ && defined RENDERTYPESDL && SDL_MAJOR_VERSION >= 2
    uint32_t const inited = SDL_WasInit(SDL_INIT_VIDEO);
    if (inited == 0)
        SDL_Init(SDL_INIT_VIDEO);
    else if (!(inited & SDL_INIT_VIDEO))
        SDL_InitSubSystem(SDL_INIT_VIDEO);

    int32_t const displayCount = SDL_GetNumVideoDisplays();
    int32_t const resolvedDisplay = displayCount > 0 ? clamp<int32_t>(displayIndex, 0, displayCount - 1) : 0;

    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(resolvedDisplay, &dm) == 0 && dm.w >= 320 && dm.h >= 200)
    {
        *width = dm.w;
        *height = dm.h;
        return true;
    }
#else
    UNREFERENCED_PARAMETER(displayIndex);
    UNREFERENCED_PARAMETER(width);
    UNREFERENCED_PARAMETER(height);
#endif

    return false;
}

static void CONFIG_SanitizeVideoSetup(void)
{
#if !defined __ANDROID__
    ud.setup.fullscreen = ud.setup.fullscreen != 0;

    int32_t desktopWidth = 0;
    int32_t desktopHeight = 0;
    bool const haveDesktopMode = CONFIG_GetDesktopVideoMode(r_displayindex, &desktopWidth, &desktopHeight);
    bool const invalidResolution = ud.setup.xdim < 320 || ud.setup.ydim < 200 || ud.setup.xdim > MAXXDIM || ud.setup.ydim > MAXYDIM ||
                                   (ud.setup.xdim * 10) < (ud.setup.ydim * 12);

    if (haveDesktopMode && (invalidResolution || (ud.setup.fullscreen && (ud.setup.xdim > desktopWidth || ud.setup.ydim > desktopHeight))))
    {
        ud.setup.xdim = desktopWidth;
        ud.setup.ydim = desktopHeight;
        ud.setup.fullscreen = 1;
    }
    else if (!haveDesktopMode && invalidResolution)
    {
        ud.setup.xdim = 1024;
        ud.setup.ydim = 768;
        ud.setup.fullscreen = 1;
    }
#endif
}

int32_t CONFIG_NormalizeControllerViewCentering(int32_t const viewCentering)
{
    if (viewCentering <= 0)
        return CONFIG_CONTROLLER_VIEW_CENTERING_DEFAULT;
    if (viewCentering <= 2)
        return CONFIG_CONTROLLER_VIEW_CENTERING_SLOW;
    if (viewCentering <= 6)
        return CONFIG_CONTROLLER_VIEW_CENTERING_MED;

    return CONFIG_CONTROLLER_VIEW_CENTERING_FAST;
}

static int32_t CONFIG_GetControllerViewCenteringOptionIndex(int32_t const viewCentering)
{
    switch (CONFIG_NormalizeControllerViewCentering(viewCentering))
    {
        case CONFIG_CONTROLLER_VIEW_CENTERING_SLOW: return 1;
        case CONFIG_CONTROLLER_VIEW_CENTERING_MED:  return 2;
        case CONFIG_CONTROLLER_VIEW_CENTERING_FAST: return 3;
        default:                                    return 0;
    }
}

int32_t CONFIG_AdjustControllerViewCentering(int32_t const viewCentering, int32_t const direction)
{
    static int32_t constexpr values[] = {
        CONFIG_CONTROLLER_VIEW_CENTERING_DEFAULT,
        CONFIG_CONTROLLER_VIEW_CENTERING_SLOW,
        CONFIG_CONTROLLER_VIEW_CENTERING_MED,
        CONFIG_CONTROLLER_VIEW_CENTERING_FAST,
    };

    int32_t const option = clamp<int32_t>(CONFIG_GetControllerViewCenteringOptionIndex(viewCentering) + direction, 0, ARRAY_SIZE(values) - 1);
    return values[option];
}

char const *CONFIG_GetControllerViewCenteringName(int32_t const viewCentering)
{
    static char const * const names[] = { "Off", "Slow", "Med", "Fast" };
    return names[CONFIG_GetControllerViewCenteringOptionIndex(viewCentering)];
}

static bool CONFIG_IsBlankPlayerName(char const * const name)
{
    char strippedName[64];
    return name == nullptr || OSD_StripColors(strippedName, name)[0] == '\0';
}

static void CONFIG_ClampPlayerName(char * const name)
{
    char strippedName[64];
    while (!CONFIG_IsBlankPlayerName(name) && Bstrlen(OSD_StripColors(strippedName, name)) > 10)
        name[Bstrlen(name) - 1] = '\0';
}

static void CONFIG_SetDefaultSplitScreenPlayerName(int const index)
{
    if (index == 0)
        Bstrcpy(ud.config.SplitScreenPlayerName[index], "Player");
    else
        Bsprintf(ud.config.SplitScreenPlayerName[index], "Player %d", index + 1);
}

int32_t CONFIG_FunctionNameToNum(const char *func)
{
    if (!func || func[0] == '\0')
        return -1;

    return hash_findcase(&h_gamefuncs, func);
}


static char const * CONFIG_FunctionNumToName(int32_t func)
{
    if ((unsigned)func >= (unsigned)NUMGAMEFUNCTIONS)
        return "";
    return gamefunctions[func];
}


int32_t CONFIG_AnalogNameToNum(const char *func)
{
    if (!func)
        return -1;
    else if (!Bstrcasecmp(func, "analog_turning"))
        return analog_turning;
    else if (!Bstrcasecmp(func, "analog_strafing"))
        return analog_strafing;
    else if (!Bstrcasecmp(func, "analog_moving"))
        return analog_moving;
    else if (!Bstrcasecmp(func, "analog_lookingupanddown"))
        return analog_lookingupanddown;
    else
        return -1;
}


static char const * CONFIG_AnalogNumToName(int32_t func)
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

    return "";
}


static void CONFIG_SetJoystickButtonFunction(int i, int j, int function)
{
    ud.config.JoystickFunctions[i][j] = function;
    CONTROL_MapButton(function, i, j, controldevice_joystick);
}
static void CONFIG_SetJoystickAnalogAxisSensitivity(int i, float sens)
{
    ud.config.JoystickAnalogueSensitivity[i] = sens;
    CONTROL_SetAnalogAxisSensitivity(i, sens, controldevice_joystick);
}
static void CONFIG_SetJoystickAnalogAxisInvert(int i, int invert)
{
    ud.config.JoystickAnalogueInvert[i] = invert;
    CONTROL_SetAnalogAxisInvert(i, invert);
}
static void CONFIG_SetJoystickAnalogAxisDeadSaturate(int i, int dead, int saturate)
{
    ud.config.JoystickAnalogueDead[i] = dead;
    ud.config.JoystickAnalogueSaturate[i] = saturate;
    JOYSTICK_SetDeadZone(i, dead, saturate);
}
static void CONFIG_SetJoystickDigitalAxisFunction(int i, int j, int function)
{
    ud.config.JoystickDigitalFunctions[i][j] = function;
    CONTROL_MapDigitalAxis(i, function, j);
}
static void CONFIG_SetJoystickAnalogAxisFunction(int i, int function)
{
    ud.config.JoystickAnalogueAxes[i] = function;
    CONTROL_MapAnalogAxis(i, function);
}


void CONFIG_SetDefaultKeys(const char (*keyptr)[MAXGAMEFUNCLEN], bool lazy/*=false*/)
{
    static char const s_gamefunc_[] = "gamefunc_";
    int constexpr strlen_gamefunc_  = ARRAY_SIZE(s_gamefunc_) - 1;

    if (!lazy)
    {
        Bmemset(ud.config.KeyboardKeys, 0xff, sizeof(ud.config.KeyboardKeys));
        CONTROL_ClearAllBinds();
    }

    for (int i=0; i < ARRAY_SSIZE(gamefunctions); ++i)
    {
        if (gamefunctions[i][0] == '\0')
            continue;

        auto &key = ud.config.KeyboardKeys[i];

        int const default0 = KB_StringToScanCode(keyptr[i<<1]);
        int const default1 = KB_StringToScanCode(keyptr[(i<<1)+1]);

        // skip the function if the default key is already used
        // or the function is assigned to another key
        if (lazy && (key[0] != 0xff || (CONTROL_KeyIsBound(default0) && Bstrlen(CONTROL_KeyBinds[default0].cmdstr) > strlen_gamefunc_
                        && CONFIG_FunctionNameToNum(CONTROL_KeyBinds[default0].cmdstr + strlen_gamefunc_) >= 0)))
        {
#if 0 // defined(DEBUGGINGAIDS)
            if (key[0] != 0xff)
                DLOG_F(INFO, "Skipping key '%s' bound to '%s'", keyptr[i<<1], CONTROL_KeyBinds[default0].cmdstr);
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
    ud.config.scripthandle = -1;

#ifdef __ANDROID__
    droidinput.forward_sens = 5.f;
    droidinput.gameControlsAlpha = 0.5;
    droidinput.hideStick = 0;
    droidinput.pitch_sens = 5.f;
    droidinput.quickSelectWeapon = 1;
    droidinput.strafe_sens = 5.f;
    droidinput.toggleCrouch = 1;
    droidinput.yaw_sens = 5.f;

    ud.setup.xdim = droidinfo.screen_width;
    ud.setup.ydim = droidinfo.screen_height;
#else
    int32_t desktopWidth = 0;
    int32_t desktopHeight = 0;
    if (CONFIG_GetDesktopVideoMode(0, &desktopWidth, &desktopHeight))
    {
        ud.setup.xdim = desktopWidth;
        ud.setup.ydim = desktopHeight;
    }
    else
    {
        ud.setup.xdim = 1024;
        ud.setup.ydim = 768;
    }
#endif

#ifdef USE_OPENGL
    ud.setup.bpp = 32;
#else
    ud.setup.bpp = 8;
#endif

#if defined(_WIN32)
    ud.config.MixRate = 44100;
#elif defined __ANDROID__
    ud.config.MixRate = droidinfo.audio_sample_rate;
#else
    ud.config.MixRate = 48000;
#endif

#if defined GEKKO || defined __OPENDINGUX__
    ud.config.NumVoices = 32;
    ud.camera_time = 11;
#else
    ud.config.NumVoices = 64;
    ud.camera_time    = 4;
#endif

    // currently settings.cfg is only read after the startup window launches the game,
    // and rereading binds might be fickle so just enable this
    ud.setup.usejoystick = 1;

    g_myAimMode = 1;
    g_player[0].ps->aim_mode = 1;

    ud.setup.forcesetup       = 1;
    ud.setup.noautoload       = 1;
    ud.setup.fullscreen       = 1;
    ud.setup.usemouse         = 1;

    ud.althud                 = 1;
    ud.auto_run               = 1;
    ud.automsg                = 0;
    ud.autosave               = 1;
    ud.autosavedeletion       = 1;
    ud.autovote               = 0;
    ud.brightness             = 8;
    ud.camerasprite           = -1;
    ud.cashman                = 0;
    ud.color                  = 0;
    ud.config.AmbienceToggle  = 1;
    ud.config.AutoAim         = 1;
    ud.config.CheckForUpdates = 1;
    ud.config.MasterVolume    = 255;
    ud.config.FXVolume        = 255;
    ud.config.VoiceVolume     = 255;
    ud.config.JoystickAimWeight = 4;
    ud.config.JoystickViewCentering = CONFIG_CONTROLLER_VIEW_CENTERING_DEFAULT;
    ud.config.JoystickAimAssist = 1;
    ud.config.SplitScreenSeparateKeyboardMouse = 0;
    ud.config.SplitScreenHudStyle = HUD_STYLE_SPLITSCREEN;
    for (int i = 0; i < MAXSPLITSCREENCONTROLLERPROFILES; ++i)
    {
        ud.config.SplitScreenJoystickAimWeight[i] = ud.config.JoystickAimWeight;
        ud.config.SplitScreenJoystickViewCentering[i] = ud.config.JoystickViewCentering;
        ud.config.SplitScreenJoystickAimAssist[i] = ud.config.JoystickAimAssist;
    }
    ud.config.MouseBias       = 0;
    ud.config.MusicDevice     = ASS_AutoDetect;
    ud.config.MusicToggle     = 1;
    ud.config.MusicVolume     = 195;
    ud.config.NumBits         = 16;
    ud.config.NumChannels     = 2;
#ifdef ASS_REVERSESTEREO
    ud.config.ReverseStereo   = 0;
#endif
    ud.config.ShowWeapons     = 0;
    ud.config.SoundToggle     = 1;
    ud.config.VoiceToggle     = 5;  // bitfield, 1 = local, 2 = dummy, 4 = other players in DM
    ud.config.useprecache     = 1;
    ud.configversion          = 0;
    g_controllerViewCenteringDefaultMigrated = 0;
    ud.crosshair              = 1;
    ud.crosshairscale         = 50;
    ud.default_skill          = 1;
    ud.democams               = 1;
    ud.detail                 = 0;
    ud.display_bonus_screen   = 1;
    ud.drawweapon             = 1;
    ud.fov                    = 90;
    ud.fta_on                 = 1;
    ud.god                    = 0;
    ud.hudontop               = 0;
    ud.idplayers              = 1;
    ud.kick_mode              = 0;
    ud.levelstats             = 0;
    ud.lockout                = 0;
    ud.m_marker               = 1;
    ud.m_weaponsharing        = 0;
    ud.maxautosaves           = 5;
    ud.menu_scrollbartilenum  = -1;
    ud.menu_scrollbarz        = 65536;
    ud.menu_scrollcursorz     = 65536;
    ud.menu_slidebarmargin    = 65536;
    ud.menu_slidebarz         = 65536;
    ud.menu_slidecursorz      = 65536;
    ud.menubackground         = 1;
    ud.mouseaiming            = 0;
    ud.mouseflip              = 1;
    ud.msgdisptime            = 120;
    ud.obituaries             = 1;
    ud.weaponsharing          = 0;
    ud.pwlockout[0]           = '\0';
    ud.quote_yoffset          = 0;
    ud.runkey_mode            = 0;
    ud.screen_size            = 4;
    ud.screen_tilting         = 1;
    ud.screenfade             = 1;
    ud.shadow_pal             = 4;
    ud.shadows                = 1;
    ud.show_level_text        = 1;
    ud.slidebar_paldisabled   = 1;
    ud.statusbarflags         = STATUSBAR_NOSHRINK;
    ud.statusbarmode          = 1;
    ud.statusbarscale         = 100;
    ud.team                   = 0;
    ud.textscale              = 200;
    ud.viewbob                = 1;
    ud.weaponscale            = 100;
    ud.weaponsway             = 1;
    ud.weaponswitch           = 3;  // new+empty

    for (int i = 0; i < MAXSPLITSCREENCONTROLLERS; ++i)
    {
        ud.config.SplitScreenPlayerAutoAim[i] = ud.config.AutoAim;
        ud.config.SplitScreenPlayerAlwaysRun[i] = 1;
        ud.config.SplitScreenPlayerWeaponSwitch[i] = ud.weaponswitch;
        ud.config.SplitScreenPlayerColor[i] = ud.color;
        ud.config.SplitScreenPlayerTeam[i] = i & 1;
        Bsprintf(ud.config.SplitScreenPlayerName[i], "Player %d", i + 1);
    }

    Bstrcpy(ud.rtsname, G_DefaultRtsFile());

    if (!CommandName)
        Bstrcpy(szPlayerName, "Player");

    Bstrncpyz(ud.config.SplitScreenPlayerName[0], szPlayerName, sizeof(ud.config.SplitScreenPlayerName[0]));

#ifndef EDUKE32_STANDALONE
    Bstrcpy(ud.ridecule[0], "An inspiration for birth control.");
    Bstrcpy(ud.ridecule[1], "You're gonna die for that!");
    Bstrcpy(ud.ridecule[2], "It hurts to be you.");
    Bstrcpy(ud.ridecule[3], "Lucky son of a bitch.");
    Bstrcpy(ud.ridecule[4], "Hmmm... payback time.");
    Bstrcpy(ud.ridecule[5], "You bottom dwelling scum sucker.");
    Bstrcpy(ud.ridecule[6], "Damn, you're ugly.");
    Bstrcpy(ud.ridecule[7], "Ha ha ha... wasted!");
    Bstrcpy(ud.ridecule[8], "You suck!");
    Bstrcpy(ud.ridecule[9], "AARRRGHHHHH!!!");
#endif

    CONFIG_SetDefaultKeys(keydefaults);

    memset(ud.config.MouseFunctions, -1, sizeof(ud.config.MouseFunctions));
    memset(ud.config.JoystickFunctions, -1, sizeof(ud.config.JoystickFunctions));
    memset(ud.config.JoystickDigitalFunctions, -1, sizeof(ud.config.JoystickDigitalFunctions));

    CONTROL_MouseSensitivity = DEFAULTMOUSESENSITIVITY;

    for (int i=0; i<MAXMOUSEBUTTONS; i++)
    {
        ud.config.MouseFunctions[i][0] = CONFIG_FunctionNameToNum(mousedefaults[i]);
        CONTROL_MapButton(ud.config.MouseFunctions[i][0], i, 0, controldevice_mouse);
        if (i>=4) continue;
        ud.config.MouseFunctions[i][1] = CONFIG_FunctionNameToNum(mouseclickeddefaults[i]);
        CONTROL_MapButton(ud.config.MouseFunctions[i][1], i, 1, controldevice_mouse);
    }

#if !defined GEKKO
    CONFIG_SetGameControllerDefaults();
#else
    for (int i=0; i<MAXJOYBUTTONSANDHATS; i++)
    {
        ud.config.JoystickFunctions[i][0] = CONFIG_FunctionNameToNum(joystickdefaults[i]);
        ud.config.JoystickFunctions[i][1] = CONFIG_FunctionNameToNum(joystickclickeddefaults[i]);
        CONTROL_MapButton(ud.config.JoystickFunctions[i][0], i, 0, controldevice_joystick);
        CONTROL_MapButton(ud.config.JoystickFunctions[i][1], i, 1, controldevice_joystick);
    }

    for (int i=0; i<MAXJOYAXES; i++)
    {
        ud.config.JoystickAnalogueScale[i] = DEFAULTJOYSTICKANALOGUESCALE;
        ud.config.JoystickAnalogueInvert[i] = 0;
        ud.config.JoystickAnalogueDead[i] = DEFAULTJOYSTICKANALOGUEDEAD;
        ud.config.JoystickAnalogueSaturate[i] = DEFAULTJOYSTICKANALOGUESATURATE;
        CONTROL_SetAnalogAxisScale(i, ud.config.JoystickAnalogueScale[i], controldevice_joystick);
        CONTROL_SetAnalogAxisInvert(i, 0, controldevice_joystick);

        ud.config.JoystickDigitalFunctions[i][0] = CONFIG_FunctionNameToNum(joystickdigitaldefaults[i*2]);
        ud.config.JoystickDigitalFunctions[i][1] = CONFIG_FunctionNameToNum(joystickdigitaldefaults[i*2+1]);
        CONTROL_MapDigitalAxis(i, ud.config.JoystickDigitalFunctions[i][0], 0, controldevice_joystick);
        CONTROL_MapDigitalAxis(i, ud.config.JoystickDigitalFunctions[i][1], 1, controldevice_joystick);

        ud.config.JoystickAnalogueAxes[i] = CONFIG_AnalogNameToNum(joystickanalogdefaults[i]);
        CONTROL_MapAnalogAxis(i, ud.config.JoystickAnalogueAxes[i], controldevice_joystick);
    }
#endif

    for (int i = 1; i < MAXSPLITSCREENCONTROLLERS; ++i)
        CONFIG_SetSplitScreenGameControllerDefaults(i);

    VM_OnEvent(EVENT_SETDEFAULTS, g_player[myconnectindex].ps->i, myconnectindex);
}


// generates key bindings to reflect changes to keyboard setup
void CONFIG_MapKey(int which, kb_scancode key1, kb_scancode oldkey1, kb_scancode key2, kb_scancode oldkey2)
{
    int const keys[] = { key1, key2, oldkey1, oldkey2 };
    char buf[2*MAXGAMEFUNCLEN];

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

        for (int i=NUMGAMEFUNCTIONS-1; i>=0; i--)
        {
            if (ud.config.KeyboardKeys[i][0] == keys[k] || ud.config.KeyboardKeys[i][1] == keys[k])
            {
                Bsprintf(buf, "gamefunc_%s; ", CONFIG_FunctionNumToName(i));
                Bstrcat(tempbuf,buf);
            }
        }

        int const len = Bstrlen(tempbuf);

        if (len >= 2)
        {
            tempbuf[len-2] = 0;  // cut off the trailing "; "
            CONTROL_BindKey(keys[k], tempbuf, 1, sctokeylut[match].key ? sctokeylut[match].key : "<?>");
        }
        else
        {
            CONTROL_FreeKeyBind(keys[k]);
        }
    }
}


void CONFIG_SetupMouse(void)
{
    if (ud.config.scripthandle < 0)
        return;

    char str[80];
    char temp[80];

    for (int i=0; i<MAXMOUSEBUTTONS; i++)
    {
        Bsprintf(str,"MouseButton%d",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle,"Controls", str,temp))
            ud.config.MouseFunctions[i][0] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str,"MouseButtonClicked%d",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle,"Controls", str,temp))
            ud.config.MouseFunctions[i][1] = CONFIG_FunctionNameToNum(temp);
    }

    for (int i=0; i<MAXMOUSEBUTTONS; i++)
    {
        CONTROL_MapButton(ud.config.MouseFunctions[i][0], i, 0, controldevice_mouse);
        CONTROL_MapButton(ud.config.MouseFunctions[i][1], i, 1,  controldevice_mouse);
    }
}


void CONFIG_SetupJoystick(void)
{
    int32_t i;
    char str[80];
    char temp[80];
    int32_t scale;
    double sens;

    if (ud.config.scripthandle < 0) return;

    for (i=0; i<MAXJOYBUTTONSANDHATS; i++)
    {
        Bsprintf(str,"ControllerButton%d",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle,"Controls", str,temp))
            ud.config.JoystickFunctions[i][0] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str,"ControllerButtonClicked%d",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle,"Controls", str,temp))
            ud.config.JoystickFunctions[i][1] = CONFIG_FunctionNameToNum(temp);
    }

    // map over the axes
    for (i=0; i<MAXJOYAXES; i++)
    {
        Bsprintf(str,"ControllerAnalogAxes%d",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str,temp))
            ud.config.JoystickAnalogueAxes[i] = CONFIG_AnalogNameToNum(temp);

        Bsprintf(str,"ControllerDigitalAxes%d_0",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str,temp))
            ud.config.JoystickDigitalFunctions[i][0] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str,"ControllerDigitalAxes%d_1",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str,temp))
            ud.config.JoystickDigitalFunctions[i][1] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str,"ControllerAnalogSensitivity%d",i);
        sens = ud.config.JoystickAnalogueSensitivity[i];
        SCRIPT_GetDouble(ud.config.scripthandle, "Controls", str, &sens);
        ud.config.JoystickAnalogueSensitivity[i] = sens;

        Bsprintf(str,"ControllerAnalogInvert%d",i);
        scale = ud.config.JoystickAnalogueInvert[i];
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", str,&scale);
        ud.config.JoystickAnalogueInvert[i] = scale;

        Bsprintf(str,"ControllerAnalogDead%d",i);
        scale = ud.config.JoystickAnalogueDead[i];
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", str,&scale);
        ud.config.JoystickAnalogueDead[i] = scale;

        Bsprintf(str,"ControllerAnalogSaturate%d",i);
        scale = ud.config.JoystickAnalogueSaturate[i];
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", str,&scale);
        ud.config.JoystickAnalogueSaturate[i] = scale;
    }

    SCRIPT_GetNumber(ud.config.scripthandle, "Controls", "ControllerAimWeight", &ud.config.JoystickAimWeight);
    ud.config.JoystickAimWeight = clamp(ud.config.JoystickAimWeight, 0, 8);

    SCRIPT_GetNumber(ud.config.scripthandle, "Controls", "ControllerViewCentering", &ud.config.JoystickViewCentering);
    ud.config.JoystickViewCentering = clamp(ud.config.JoystickViewCentering, 0, 8);

    SCRIPT_GetNumber(ud.config.scripthandle, "Controls", "ControllerAimAssist", &ud.config.JoystickAimAssist);
    ud.config.JoystickAimAssist = ud.config.JoystickAimAssist != 0;

    SCRIPT_GetNumber(ud.config.scripthandle, "Controls", "ViewCenteringDefaultMigrated", &g_controllerViewCenteringDefaultMigrated);
    g_controllerViewCenteringDefaultMigrated = g_controllerViewCenteringDefaultMigrated != 0;

    for (int controllerIndex = 1; controllerIndex < MAXSPLITSCREENCONTROLLERS; ++controllerIndex)
    {
        int const profile = controllerIndex - 1;
        int const controllerNumber = controllerIndex + 1;

        for (i = 0; i < MAXJOYBUTTONSANDHATS; i++)
        {
            Bsprintf(str, "SplitScreenController%dButton%d", controllerNumber, i);
            temp[0] = 0;
            if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str, temp))
                ud.config.SplitScreenJoystickFunctions[profile][i][0] = CONFIG_FunctionNameToNum(temp);

            Bsprintf(str, "SplitScreenController%dButtonClicked%d", controllerNumber, i);
            temp[0] = 0;
            if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str, temp))
                ud.config.SplitScreenJoystickFunctions[profile][i][1] = CONFIG_FunctionNameToNum(temp);
        }

        for (i = 0; i < MAXJOYAXES; i++)
        {
            Bsprintf(str, "SplitScreenController%dAnalogAxes%d", controllerNumber, i);
            temp[0] = 0;
            if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str, temp))
                ud.config.SplitScreenJoystickAnalogueAxes[profile][i] = CONFIG_AnalogNameToNum(temp);

            Bsprintf(str, "SplitScreenController%dDigitalAxes%d_0", controllerNumber, i);
            temp[0] = 0;
            if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str, temp))
                ud.config.SplitScreenJoystickDigitalFunctions[profile][i][0] = CONFIG_FunctionNameToNum(temp);

            Bsprintf(str, "SplitScreenController%dDigitalAxes%d_1", controllerNumber, i);
            temp[0] = 0;
            if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str, temp))
                ud.config.SplitScreenJoystickDigitalFunctions[profile][i][1] = CONFIG_FunctionNameToNum(temp);

            Bsprintf(str, "SplitScreenController%dAnalogSensitivity%d", controllerNumber, i);
            sens = ud.config.SplitScreenJoystickAnalogueSensitivity[profile][i];
            SCRIPT_GetDouble(ud.config.scripthandle, "Controls", str, &sens);
            ud.config.SplitScreenJoystickAnalogueSensitivity[profile][i] = sens;

            Bsprintf(str, "SplitScreenController%dAnalogInvert%d", controllerNumber, i);
            scale = ud.config.SplitScreenJoystickAnalogueInvert[profile][i];
            SCRIPT_GetNumber(ud.config.scripthandle, "Controls", str, &scale);
            ud.config.SplitScreenJoystickAnalogueInvert[profile][i] = scale;

            Bsprintf(str, "SplitScreenController%dAnalogDead%d", controllerNumber, i);
            scale = ud.config.SplitScreenJoystickAnalogueDead[profile][i];
            SCRIPT_GetNumber(ud.config.scripthandle, "Controls", str, &scale);
            ud.config.SplitScreenJoystickAnalogueDead[profile][i] = scale;

            Bsprintf(str, "SplitScreenController%dAnalogSaturate%d", controllerNumber, i);
            scale = ud.config.SplitScreenJoystickAnalogueSaturate[profile][i];
            SCRIPT_GetNumber(ud.config.scripthandle, "Controls", str, &scale);
            ud.config.SplitScreenJoystickAnalogueSaturate[profile][i] = scale;
        }

        Bsprintf(str, "SplitScreenController%dAimWeight", controllerNumber);
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", str, &ud.config.SplitScreenJoystickAimWeight[profile]);
        ud.config.SplitScreenJoystickAimWeight[profile] = clamp(ud.config.SplitScreenJoystickAimWeight[profile], 0, 8);

        Bsprintf(str, "SplitScreenController%dViewCentering", controllerNumber);
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", str, &ud.config.SplitScreenJoystickViewCentering[profile]);
        ud.config.SplitScreenJoystickViewCentering[profile] = clamp(ud.config.SplitScreenJoystickViewCentering[profile], 0, 8);

        Bsprintf(str, "SplitScreenController%dAimAssist", controllerNumber);
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", str, &ud.config.SplitScreenJoystickAimAssist[profile]);
        ud.config.SplitScreenJoystickAimAssist[profile] = ud.config.SplitScreenJoystickAimAssist[profile] != 0;
    }

    CONFIG_MigrateModernControllerDefaults();

    for (i=0; i<MAXJOYBUTTONSANDHATS; i++)
    {
        CONTROL_MapButton(ud.config.JoystickFunctions[i][0], i, 0, controldevice_joystick);
        CONTROL_MapButton(ud.config.JoystickFunctions[i][1], i, 1,  controldevice_joystick);
    }

    for (i=0; i<MAXJOYAXES; i++)
    {
        CONTROL_MapAnalogAxis(i, ud.config.JoystickAnalogueAxes[i]);
        CONTROL_MapDigitalAxis(i, ud.config.JoystickDigitalFunctions[i][0], 0);
        CONTROL_MapDigitalAxis(i, ud.config.JoystickDigitalFunctions[i][1], 1);
        CONTROL_SetAnalogAxisSensitivity(i, ud.config.JoystickAnalogueSensitivity[i], controldevice_joystick);
        CONTROL_SetAnalogAxisInvert(i, ud.config.JoystickAnalogueInvert[i]);
        JOYSTICK_SetDeadZone(i, ud.config.JoystickAnalogueDead[i], ud.config.JoystickAnalogueSaturate[i]);
    }
}

struct GameControllerButtonSetting
{
    GameControllerButton button;
    int function;

    void apply() const
    {
        CONFIG_SetJoystickButtonFunction(button, 0, function);
    }
};
struct GameControllerAnalogAxisSetting
{
    GameControllerAxis axis;
    int function;

    void apply() const
    {
        CONFIG_SetJoystickAnalogAxisFunction(axis, function);
    }
};
struct GameControllerDigitalAxisSetting
{
    GameControllerAxis axis;
    int polarity;
    int function;

    void apply() const
    {
        CONFIG_SetJoystickDigitalAxisFunction(axis, polarity, function);
    }
};

static void CONFIG_SetGameControllerAxesModern()
{
    static GameControllerAnalogAxisSetting const analogAxes[] =
    {
        { CONTROLLER_AXIS_LEFTX, analog_strafing },
        { CONTROLLER_AXIS_LEFTY, analog_moving },
        { CONTROLLER_AXIS_RIGHTX, analog_turning },
        { CONTROLLER_AXIS_RIGHTY, analog_lookingupanddown },
    };

    CONFIG_SetJoystickAnalogAxisSensitivity(CONTROLLER_AXIS_RIGHTX, CONFIG_CONTROLLER_AIM_SENS_X);
    CONFIG_SetJoystickAnalogAxisSensitivity(CONTROLLER_AXIS_RIGHTY, CONFIG_CONTROLLER_AIM_SENS_Y);
    CONFIG_SetJoystickAnalogAxisInvert(CONTROLLER_AXIS_RIGHTY, 0);

    for (auto const & analogAxis : analogAxes)
        analogAxis.apply();
}

void CONFIG_SetGameControllerDefaults()
{
    CONFIG_SetGameControllerDefaultsClear();
    CONFIG_SetGameControllerAxesModern();

    static GameControllerButtonSetting const buttons[] =
    {
        { CONTROLLER_BUTTON_A, gamefunc_Jump },
        { CONTROLLER_BUTTON_B, gamefunc_Crouch },
        { CONTROLLER_BUTTON_Y, gamefunc_Quick_Kick },
        { CONTROLLER_BUTTON_BACK, gamefunc_Map },
        { CONTROLLER_BUTTON_RIGHTSTICK, gamefunc_Center_View },
        { CONTROLLER_BUTTON_DPAD_UP, gamefunc_NightVision },
        { CONTROLLER_BUTTON_DPAD_DOWN, gamefunc_Inventory },
        { CONTROLLER_BUTTON_LEFTSHOULDER, gamefunc_Previous_Weapon },
        { CONTROLLER_BUTTON_RIGHTSHOULDER, gamefunc_Next_Weapon },
        { CONTROLLER_BUTTON_MISC, gamefunc_Third_Person_View },
    };

    static GameControllerButtonSetting const buttonsDuke[] =
    {
        { CONTROLLER_BUTTON_X, gamefunc_Open },
        { CONTROLLER_BUTTON_DPAD_LEFT, gamefunc_Inventory_Left },
        { CONTROLLER_BUTTON_DPAD_RIGHT, gamefunc_Inventory_Right },
    };

    static GameControllerButtonSetting const buttonsFury[] =
    {
        { CONTROLLER_BUTTON_X, gamefunc_Steroids }, // Reload
        { CONTROLLER_BUTTON_DPAD_LEFT, gamefunc_MedKit },
        { CONTROLLER_BUTTON_DPAD_RIGHT, gamefunc_NightVision }, // Radar
    };

    static GameControllerDigitalAxisSetting const digitalAxes[] =
    {
        { CONTROLLER_AXIS_TRIGGERLEFT, 1, gamefunc_Run },
        { CONTROLLER_AXIS_TRIGGERRIGHT, 1, gamefunc_Fire },
    };

    for (auto const & button : buttons)
        button.apply();

    if (FURY)
    {
        for (auto const & button : buttonsFury)
            button.apply();
    }
    else
    {
        for (auto const & button : buttonsDuke)
            button.apply();
    }

    for (auto const & digitalAxis : digitalAxes)
        digitalAxis.apply();

    ud.config.JoystickAimAssist     = 1;
    ud.config.JoystickAimWeight     = 4;
    ud.config.JoystickViewCentering = CONFIG_CONTROLLER_VIEW_CENTERING_DEFAULT;
    ud.config.controllerRumble = 1;
}

static int CONFIG_SplitScreenControllerProfile(int32_t const controllerIndex)
{
    return (controllerIndex > 0 && controllerIndex < MAXSPLITSCREENCONTROLLERS) ? controllerIndex - 1 : -1;
}

static void CONFIG_SetSplitScreenJoystickButtonFunction(int profile, int button, int clicked, int function)
{
    ud.config.SplitScreenJoystickFunctions[profile][button][clicked] = function;
}

static void CONFIG_SetSplitScreenJoystickAnalogAxisSensitivity(int profile, int axis, float sens)
{
    ud.config.SplitScreenJoystickAnalogueSensitivity[profile][axis] = sens;
}

static void CONFIG_SetSplitScreenJoystickAnalogAxisInvert(int profile, int axis, int invert)
{
    ud.config.SplitScreenJoystickAnalogueInvert[profile][axis] = invert;
}

static void CONFIG_SetSplitScreenJoystickAnalogAxisDeadSaturate(int profile, int axis, int dead, int saturate)
{
    ud.config.SplitScreenJoystickAnalogueDead[profile][axis] = dead;
    ud.config.SplitScreenJoystickAnalogueSaturate[profile][axis] = saturate;
}

static void CONFIG_SetSplitScreenJoystickDigitalAxisFunction(int profile, int axis, int polarity, int function)
{
    ud.config.SplitScreenJoystickDigitalFunctions[profile][axis][polarity] = function;
}

static void CONFIG_SetSplitScreenJoystickAnalogAxisFunction(int profile, int axis, int function)
{
    ud.config.SplitScreenJoystickAnalogueAxes[profile][axis] = function;
}

void CONFIG_SetSplitScreenGameControllerDefaults(int32_t const controllerIndex)
{
    int const profile = CONFIG_SplitScreenControllerProfile(controllerIndex);
    if (profile < 0)
        return;

    for (int i = 0; i < MAXJOYBUTTONSANDHATS; i++)
    {
        CONFIG_SetSplitScreenJoystickButtonFunction(profile, i, 0, -1);
        CONFIG_SetSplitScreenJoystickButtonFunction(profile, i, 1, -1);
    }

    for (int i = 0; i < MAXJOYAXES; i++)
    {
        CONFIG_SetSplitScreenJoystickAnalogAxisSensitivity(profile, i, DEFAULTJOYSTICKANALOGUESENSITIVITY);
        CONFIG_SetSplitScreenJoystickAnalogAxisInvert(profile, i, 0);
        CONFIG_SetSplitScreenJoystickAnalogAxisDeadSaturate(profile, i, DEFAULTJOYSTICKANALOGUEDEAD, DEFAULTJOYSTICKANALOGUESATURATE);
        CONFIG_SetSplitScreenJoystickDigitalAxisFunction(profile, i, 0, -1);
        CONFIG_SetSplitScreenJoystickDigitalAxisFunction(profile, i, 1, -1);
        CONFIG_SetSplitScreenJoystickAnalogAxisFunction(profile, i, -1);
    }

    CONFIG_SetSplitScreenJoystickAnalogAxisFunction(profile, CONTROLLER_AXIS_LEFTX, analog_strafing);
    CONFIG_SetSplitScreenJoystickAnalogAxisFunction(profile, CONTROLLER_AXIS_LEFTY, analog_moving);
    CONFIG_SetSplitScreenJoystickAnalogAxisFunction(profile, CONTROLLER_AXIS_RIGHTX, analog_turning);
    CONFIG_SetSplitScreenJoystickAnalogAxisFunction(profile, CONTROLLER_AXIS_RIGHTY, analog_lookingupanddown);
    CONFIG_SetSplitScreenJoystickAnalogAxisSensitivity(profile, CONTROLLER_AXIS_RIGHTX, CONFIG_CONTROLLER_AIM_SENS_X);
    CONFIG_SetSplitScreenJoystickAnalogAxisSensitivity(profile, CONTROLLER_AXIS_RIGHTY, CONFIG_CONTROLLER_AIM_SENS_Y);
    CONFIG_SetSplitScreenJoystickAnalogAxisInvert(profile, CONTROLLER_AXIS_RIGHTY, 0);

    CONFIG_SetSplitScreenJoystickButtonFunction(profile, CONTROLLER_BUTTON_A, 0, gamefunc_Jump);
    CONFIG_SetSplitScreenJoystickButtonFunction(profile, CONTROLLER_BUTTON_B, 0, gamefunc_Crouch);
    CONFIG_SetSplitScreenJoystickButtonFunction(profile, CONTROLLER_BUTTON_Y, 0, gamefunc_Quick_Kick);
    CONFIG_SetSplitScreenJoystickButtonFunction(profile, CONTROLLER_BUTTON_BACK, 0, gamefunc_Map);
    CONFIG_SetSplitScreenJoystickButtonFunction(profile, CONTROLLER_BUTTON_RIGHTSTICK, 0, gamefunc_Center_View);
    CONFIG_SetSplitScreenJoystickButtonFunction(profile, CONTROLLER_BUTTON_DPAD_UP, 0, gamefunc_NightVision);
    CONFIG_SetSplitScreenJoystickButtonFunction(profile, CONTROLLER_BUTTON_DPAD_DOWN, 0, gamefunc_Inventory);
    CONFIG_SetSplitScreenJoystickButtonFunction(profile, CONTROLLER_BUTTON_LEFTSHOULDER, 0, gamefunc_Previous_Weapon);
    CONFIG_SetSplitScreenJoystickButtonFunction(profile, CONTROLLER_BUTTON_RIGHTSHOULDER, 0, gamefunc_Next_Weapon);
    CONFIG_SetSplitScreenJoystickButtonFunction(profile, CONTROLLER_BUTTON_MISC, 0, gamefunc_Third_Person_View);

    if (FURY)
    {
        CONFIG_SetSplitScreenJoystickButtonFunction(profile, CONTROLLER_BUTTON_X, 0, gamefunc_Steroids);
        CONFIG_SetSplitScreenJoystickButtonFunction(profile, CONTROLLER_BUTTON_DPAD_LEFT, 0, gamefunc_MedKit);
        CONFIG_SetSplitScreenJoystickButtonFunction(profile, CONTROLLER_BUTTON_DPAD_RIGHT, 0, gamefunc_NightVision);
    }
    else
    {
        CONFIG_SetSplitScreenJoystickButtonFunction(profile, CONTROLLER_BUTTON_X, 0, gamefunc_Open);
        CONFIG_SetSplitScreenJoystickButtonFunction(profile, CONTROLLER_BUTTON_DPAD_LEFT, 0, gamefunc_Inventory_Left);
        CONFIG_SetSplitScreenJoystickButtonFunction(profile, CONTROLLER_BUTTON_DPAD_RIGHT, 0, gamefunc_Inventory_Right);
    }

    CONFIG_SetSplitScreenJoystickDigitalAxisFunction(profile, CONTROLLER_AXIS_TRIGGERLEFT, 1, gamefunc_Run);
    CONFIG_SetSplitScreenJoystickDigitalAxisFunction(profile, CONTROLLER_AXIS_TRIGGERRIGHT, 1, gamefunc_Fire);
    ud.config.SplitScreenJoystickAimAssist[profile] = 1;
    ud.config.SplitScreenJoystickAimWeight[profile] = 4;
    ud.config.SplitScreenJoystickViewCentering[profile] = CONFIG_CONTROLLER_VIEW_CENTERING_DEFAULT;
}

static bool CONFIG_PrimaryControllerUsesOldModernDefaults(void)
{
    return ud.config.JoystickFunctions[CONTROLLER_BUTTON_A][0] == gamefunc_Open
        && ud.config.JoystickFunctions[CONTROLLER_BUTTON_B][0] == gamefunc_Inventory
        && ud.config.JoystickFunctions[CONTROLLER_BUTTON_RIGHTSTICK][0] == gamefunc_Toggle_Crouch
        && ud.config.JoystickFunctions[CONTROLLER_BUTTON_LEFTSHOULDER][0] == gamefunc_Crouch
        && ud.config.JoystickFunctions[CONTROLLER_BUTTON_RIGHTSHOULDER][0] == gamefunc_Alt_Fire
        && ud.config.JoystickDigitalFunctions[CONTROLLER_AXIS_TRIGGERLEFT][1] == gamefunc_Jump
        && ud.config.JoystickDigitalFunctions[CONTROLLER_AXIS_TRIGGERRIGHT][1] == gamefunc_Fire;
}

static bool CONFIG_SplitControllerUsesOldModernDefaults(int const profile)
{
    return ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_A][0] == gamefunc_Open
        && ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_B][0] == gamefunc_Inventory
        && ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_RIGHTSTICK][0] == gamefunc_Toggle_Crouch
        && ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_LEFTSHOULDER][0] == gamefunc_Crouch
        && ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_RIGHTSHOULDER][0] == gamefunc_Alt_Fire
        && ud.config.SplitScreenJoystickDigitalFunctions[profile][CONTROLLER_AXIS_TRIGGERLEFT][1] == gamefunc_Jump
        && ud.config.SplitScreenJoystickDigitalFunctions[profile][CONTROLLER_AXIS_TRIGGERRIGHT][1] == gamefunc_Fire;
}

static bool CONFIG_IsLegacyAimSensitivity(float const sens)
{
    int const scaled = Blrintf(sens * 10.f);
    return scaled == 14 || scaled == 20 || scaled == 22 || scaled == 23 || scaled == 25 || scaled == 30 || scaled == 35 || scaled == 40 || scaled == 42 || scaled == 43 || scaled == 45 || scaled == 47 || scaled == 50;
}

static void CONFIG_MigrateAimSensitivityDefaults(int32_t const * const analogAxes, float * const sensitivity)
{
    if (analogAxes[CONTROLLER_AXIS_RIGHTX] == analog_turning && CONFIG_IsLegacyAimSensitivity(sensitivity[CONTROLLER_AXIS_RIGHTX]))
        sensitivity[CONTROLLER_AXIS_RIGHTX] = CONFIG_CONTROLLER_AIM_SENS_X;

    if (analogAxes[CONTROLLER_AXIS_RIGHTY] == analog_lookingupanddown && CONFIG_IsLegacyAimSensitivity(sensitivity[CONTROLLER_AXIS_RIGHTY]))
        sensitivity[CONTROLLER_AXIS_RIGHTY] = CONFIG_CONTROLLER_AIM_SENS_Y;
}

static void CONFIG_MigrateViewCenteringDefault(int32_t &viewCentering)
{
    if (viewCentering == CONFIG_CONTROLLER_VIEW_CENTERING_LEGACY_DEFAULT)
        viewCentering = CONFIG_CONTROLLER_VIEW_CENTERING_DEFAULT;
}

static void CONFIG_MigrateViewCenteringDefaultsOnce(void)
{
    if (g_controllerViewCenteringDefaultMigrated)
        return;

    CONFIG_MigrateViewCenteringDefault(ud.config.JoystickViewCentering);

    for (int profile = 0; profile < MAXSPLITSCREENCONTROLLERPROFILES; ++profile)
        CONFIG_MigrateViewCenteringDefault(ud.config.SplitScreenJoystickViewCentering[profile]);

    g_controllerViewCenteringDefaultMigrated = 1;
}

static void CONFIG_NormalizeControllerViewCenteringSettings(void)
{
    ud.config.JoystickViewCentering = CONFIG_NormalizeControllerViewCentering(ud.config.JoystickViewCentering);

    for (int profile = 0; profile < MAXSPLITSCREENCONTROLLERPROFILES; ++profile)
        ud.config.SplitScreenJoystickViewCentering[profile] = CONFIG_NormalizeControllerViewCentering(ud.config.SplitScreenJoystickViewCentering[profile]);
}

static bool CONFIG_PrimaryControllerUsesPreAutoRunTriggerDefaults(void)
{
    return ud.config.JoystickFunctions[CONTROLLER_BUTTON_A][0] == gamefunc_Jump
        && ud.config.JoystickFunctions[CONTROLLER_BUTTON_B][0] == gamefunc_Crouch
        && ud.config.JoystickFunctions[CONTROLLER_BUTTON_Y][0] == gamefunc_Quick_Kick
        && ud.config.JoystickFunctions[CONTROLLER_BUTTON_BACK][0] == gamefunc_Map
        && ud.config.JoystickFunctions[CONTROLLER_BUTTON_LEFTSTICK][0] == gamefunc_Run
        && ud.config.JoystickFunctions[CONTROLLER_BUTTON_RIGHTSTICK][0] == gamefunc_Center_View
        && ud.config.JoystickFunctions[CONTROLLER_BUTTON_LEFTSHOULDER][0] == gamefunc_Previous_Weapon
        && ud.config.JoystickFunctions[CONTROLLER_BUTTON_RIGHTSHOULDER][0] == gamefunc_Next_Weapon
        && ud.config.JoystickDigitalFunctions[CONTROLLER_AXIS_TRIGGERLEFT][1] == gamefunc_Alt_Fire
        && ud.config.JoystickDigitalFunctions[CONTROLLER_AXIS_TRIGGERRIGHT][1] == gamefunc_Fire;
}

static bool CONFIG_SplitControllerUsesPreAutoRunTriggerDefaults(int const profile)
{
    return ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_A][0] == gamefunc_Jump
        && ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_B][0] == gamefunc_Crouch
        && ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_Y][0] == gamefunc_Quick_Kick
        && ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_BACK][0] == gamefunc_Map
        && ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_LEFTSTICK][0] == gamefunc_Run
        && ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_RIGHTSTICK][0] == gamefunc_Center_View
        && ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_LEFTSHOULDER][0] == gamefunc_Previous_Weapon
        && ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_RIGHTSHOULDER][0] == gamefunc_Next_Weapon
        && ud.config.SplitScreenJoystickDigitalFunctions[profile][CONTROLLER_AXIS_TRIGGERLEFT][1] == gamefunc_Alt_Fire
        && ud.config.SplitScreenJoystickDigitalFunctions[profile][CONTROLLER_AXIS_TRIGGERRIGHT][1] == gamefunc_Fire;
}

static bool CONFIG_PrimaryControllerUsesAutoRunTriggerDefaultsWithLeftStickRun(void)
{
    return ud.config.JoystickFunctions[CONTROLLER_BUTTON_A][0] == gamefunc_Jump
        && ud.config.JoystickFunctions[CONTROLLER_BUTTON_B][0] == gamefunc_Crouch
        && ud.config.JoystickFunctions[CONTROLLER_BUTTON_Y][0] == gamefunc_Quick_Kick
        && ud.config.JoystickFunctions[CONTROLLER_BUTTON_BACK][0] == gamefunc_Map
        && ud.config.JoystickFunctions[CONTROLLER_BUTTON_LEFTSTICK][0] == gamefunc_Run
        && ud.config.JoystickFunctions[CONTROLLER_BUTTON_RIGHTSTICK][0] == gamefunc_Center_View
        && ud.config.JoystickFunctions[CONTROLLER_BUTTON_LEFTSHOULDER][0] == gamefunc_Previous_Weapon
        && ud.config.JoystickFunctions[CONTROLLER_BUTTON_RIGHTSHOULDER][0] == gamefunc_Next_Weapon
        && ud.config.JoystickDigitalFunctions[CONTROLLER_AXIS_TRIGGERLEFT][1] == gamefunc_Run
        && ud.config.JoystickDigitalFunctions[CONTROLLER_AXIS_TRIGGERRIGHT][1] == gamefunc_Fire;
}

static bool CONFIG_SplitControllerUsesAutoRunTriggerDefaultsWithLeftStickRun(int const profile)
{
    return ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_A][0] == gamefunc_Jump
        && ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_B][0] == gamefunc_Crouch
        && ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_Y][0] == gamefunc_Quick_Kick
        && ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_BACK][0] == gamefunc_Map
        && ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_LEFTSTICK][0] == gamefunc_Run
        && ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_RIGHTSTICK][0] == gamefunc_Center_View
        && ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_LEFTSHOULDER][0] == gamefunc_Previous_Weapon
        && ud.config.SplitScreenJoystickFunctions[profile][CONTROLLER_BUTTON_RIGHTSHOULDER][0] == gamefunc_Next_Weapon
        && ud.config.SplitScreenJoystickDigitalFunctions[profile][CONTROLLER_AXIS_TRIGGERLEFT][1] == gamefunc_Run
        && ud.config.SplitScreenJoystickDigitalFunctions[profile][CONTROLLER_AXIS_TRIGGERRIGHT][1] == gamefunc_Fire;
}

static void CONFIG_MigrateModernControllerDefaults(void)
{
    if (CONFIG_PrimaryControllerUsesOldModernDefaults())
        CONFIG_SetGameControllerDefaults();
    else if (CONFIG_PrimaryControllerUsesPreAutoRunTriggerDefaults())
    {
        CONFIG_SetJoystickDigitalAxisFunction(CONTROLLER_AXIS_TRIGGERLEFT, 1, gamefunc_Run);
        CONFIG_SetJoystickButtonFunction(CONTROLLER_BUTTON_LEFTSTICK, 0, -1);
    }
    else if (CONFIG_PrimaryControllerUsesAutoRunTriggerDefaultsWithLeftStickRun())
        CONFIG_SetJoystickButtonFunction(CONTROLLER_BUTTON_LEFTSTICK, 0, -1);

    CONFIG_MigrateAimSensitivityDefaults(ud.config.JoystickAnalogueAxes, ud.config.JoystickAnalogueSensitivity);

    for (int controllerIndex = 1; controllerIndex < MAXSPLITSCREENCONTROLLERS; ++controllerIndex)
    {
        int const profile = controllerIndex - 1;
        if (CONFIG_SplitControllerUsesOldModernDefaults(profile))
            CONFIG_SetSplitScreenGameControllerDefaults(controllerIndex);
        else if (CONFIG_SplitControllerUsesPreAutoRunTriggerDefaults(profile))
        {
            CONFIG_SetSplitScreenJoystickDigitalAxisFunction(profile, CONTROLLER_AXIS_TRIGGERLEFT, 1, gamefunc_Run);
            CONFIG_SetSplitScreenJoystickButtonFunction(profile, CONTROLLER_BUTTON_LEFTSTICK, 0, -1);
        }
        else if (CONFIG_SplitControllerUsesAutoRunTriggerDefaultsWithLeftStickRun(profile))
            CONFIG_SetSplitScreenJoystickButtonFunction(profile, CONTROLLER_BUTTON_LEFTSTICK, 0, -1);

        CONFIG_MigrateAimSensitivityDefaults(ud.config.SplitScreenJoystickAnalogueAxes[profile], ud.config.SplitScreenJoystickAnalogueSensitivity[profile]);
    }

    CONFIG_MigrateViewCenteringDefaultsOnce();
    CONFIG_NormalizeControllerViewCenteringSettings();
}

void CONFIG_SetGameControllerDefaultsClear()
{
    for (int i=0; i<MAXJOYBUTTONSANDHATS; i++)
    {
        CONFIG_SetJoystickButtonFunction(i, 0, -1);
        CONFIG_SetJoystickButtonFunction(i, 1, -1);
    }

    for (int i=0; i<MAXJOYAXES; i++)
    {
        CONFIG_SetJoystickAnalogAxisSensitivity(i, DEFAULTJOYSTICKANALOGUESENSITIVITY);
        CONFIG_SetJoystickAnalogAxisInvert(i, 0);
        CONFIG_SetJoystickAnalogAxisDeadSaturate(i, DEFAULTJOYSTICKANALOGUEDEAD, DEFAULTJOYSTICKANALOGUESATURATE);

        CONFIG_SetJoystickDigitalAxisFunction(i, 0, -1);
        CONFIG_SetJoystickDigitalAxisFunction(i, 1, -1);

        CONFIG_SetJoystickAnalogAxisFunction(i, -1);
    }
}

int CONFIG_ReadSetup(void)
{
    char tempbuf[1024];

    CONTROL_ClearAssignments();
    CONFIG_SetDefaults();

    ud.config.setupread = 1;
    pathsearchmode = 1;

    if (ud.config.scripthandle < 0)
    {
        if (buildvfs_exists(g_setupFileName))  // JBF 20031211
            ud.config.scripthandle = SCRIPT_Load(g_setupFileName);
#if !defined(EDUKE32_TOUCH_DEVICES) && !defined(EDUKE32_STANDALONE)
        else if (buildvfs_exists(SETUPFILENAME))
        {
            int const i = wm_ynbox("Import Configuration Settings",
                                   "Configuration file %s not found. "
                                   "Import configuration data from %s?",
                                   g_setupFileName, SETUPFILENAME);
            if (i)
                ud.config.scripthandle = SCRIPT_Load(SETUPFILENAME);
        }
#endif
    }

    pathsearchmode = 0;

    if (ud.config.scripthandle < 0)
        return -1;

    char commmacro[] = "CommbatMacro# ";

    for (int i = 0; i < MAXRIDECULE; i++)
    {
        commmacro[13] = i+'0';
        SCRIPT_GetString(ud.config.scripthandle, "Comm Setup",commmacro,&ud.ridecule[i][0]);
    }

    if (!CommandName)
    {
        Bmemset(tempbuf, 0, sizeof(tempbuf));
        SCRIPT_GetString(ud.config.scripthandle, "Comm Setup","PlayerName",&tempbuf[0]);

        CONFIG_ClampPlayerName(tempbuf);

        if (!CONFIG_IsBlankPlayerName(tempbuf))
            Bstrncpyz(szPlayerName, tempbuf, sizeof(szPlayerName));
    }

    SCRIPT_GetString(ud.config.scripthandle, "Comm Setup","RTSName",&ud.rtsname[0]);

    SCRIPT_GetNumber(ud.config.scripthandle, "Setup", "ConfigVersion", &ud.configversion);
    SCRIPT_GetNumber(ud.config.scripthandle, "Setup", "ForceSetup", &ud.setup.forcesetup);
    SCRIPT_GetNumber(ud.config.scripthandle, "Setup", "NoAutoLoad", &ud.setup.noautoload);

    int32_t cachesize;
    SCRIPT_GetNumber(ud.config.scripthandle, "Setup", "CacheSize", &cachesize);

    if (cachesize > MAXCACHE1DSIZE)
        MAXCACHE1DSIZE = cachesize;

    if (g_noSetup == 0 && g_modDir[0] == '/')
    {
        SCRIPT_GetString(ud.config.scripthandle, "Setup","ModDir",&g_modDir[0]);

        if (!buildvfs_isdir(g_modDir))
        {
            LOG_F(WARNING, "Invalid user directory specified in cfg file: %s", g_modDir);
            Bsprintf(g_modDir,"/");
        }
    }

    if (g_grpNamePtr == NULL && g_addonNum == 0)
    {
        SCRIPT_GetStringPtr(ud.config.scripthandle, "Setup", "SelectedGRP", &g_grpNamePtr);
        if (g_grpNamePtr && !Bstrlen(g_grpNamePtr))
            g_grpNamePtr = dup_filename(G_DefaultGrpFile());
    }

    if (!NAM_WW2GI)
    {
        SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "Out", &ud.lockout);
        SCRIPT_GetString(ud.config.scripthandle, "Screen Setup", "Password", &ud.pwlockout[0]);
    }

    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "MaxRefreshFreq", (int32_t *)&maxrefreshfreq);
    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "ScreenBPP", &ud.setup.bpp);
    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "ScreenDisplay", &r_displayindex);
    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "ScreenHeight", &ud.setup.ydim);
    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "ScreenMode", &ud.setup.fullscreen);
    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "ScreenWidth", &ud.setup.xdim);
    vec2_t windowPos;
    if (!SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "WindowPosX", &windowPos.x)
        && !SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "WindowPosY", &windowPos.y))
    {
        g_windowPos = windowPos;
        g_windowPosValid = true;
    }

    if (ud.setup.bpp < 8) ud.setup.bpp = 32;

    CONFIG_SanitizeVideoSetup();

#ifdef POLYMER
    int32_t rendmode = 0;
    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "Polymer", &rendmode);
    glrendmode = (rendmode > 0) ? REND_POLYMER : REND_POLYMOST;
#endif

    SCRIPT_GetNumber(ud.config.scripthandle, "Misc", "Executions", &ud.executions);

#ifdef _WIN32
    SCRIPT_GetNumber(ud.config.scripthandle, "Updates", "CheckForUpdates", &ud.config.CheckForUpdates);
    SCRIPT_GetNumber(ud.config.scripthandle, "Updates", "LastUpdateCheck", &ud.config.LastUpdateCheck);
#endif

    SCRIPT_GetNumber(ud.config.scripthandle, "Game Setup", "PerPlayerPickups", &ud.m_weaponsharing);
    ud.m_weaponsharing = ud.m_weaponsharing != 0;
    ud.weaponsharing = ud.m_weaponsharing;
    SCRIPT_GetNumber(ud.config.scripthandle, "Game Setup", "HudStyle", &ud.config.SplitScreenHudStyle);
    ud.config.SplitScreenHudStyle = clamp(ud.config.SplitScreenHudStyle, 0, HUD_STYLE_COUNT - 1);

    SCRIPT_GetNumber(ud.config.scripthandle, "Controls", "UseJoystick", &ud.setup.usejoystick);
    SCRIPT_GetNumber(ud.config.scripthandle, "Controls", "UseMouse", &ud.setup.usemouse);
    SCRIPT_GetNumber(ud.config.scripthandle, "Controls", "SeparateKeyboardMouseAndControllers", &ud.config.SplitScreenSeparateKeyboardMouse);
    if (g_splitScreenResumeSeparateKeyboardMouse >= 0)
        ud.config.SplitScreenSeparateKeyboardMouse = g_splitScreenResumeSeparateKeyboardMouse;
    // The split-screen mod has no visible "Use Controller" option, so avoid
    // trapping existing configs in keyboard/mouse-only mode.
    ud.setup.usejoystick = 1;

    if (!CommandName)
        Bstrncpyz(ud.config.SplitScreenPlayerName[0], szPlayerName, sizeof(ud.config.SplitScreenPlayerName[0]));

    for (int i = 0; i < MAXSPLITSCREENCONTROLLERS; ++i)
    {
        int const playerNumber = i + 1;

        Bsprintf(tempbuf, "SplitScreenPlayer%dAutoAim", playerNumber);
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", tempbuf, &ud.config.SplitScreenPlayerAutoAim[i]);

        Bsprintf(tempbuf, "SplitScreenPlayer%dAlwaysRun", playerNumber);
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", tempbuf, &ud.config.SplitScreenPlayerAlwaysRun[i]);

        Bsprintf(tempbuf, "SplitScreenPlayer%dWeaponSwitch", playerNumber);
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", tempbuf, &ud.config.SplitScreenPlayerWeaponSwitch[i]);

        Bsprintf(tempbuf, "SplitScreenPlayer%dColor", playerNumber);
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", tempbuf, &ud.config.SplitScreenPlayerColor[i]);

        Bsprintf(tempbuf, "SplitScreenPlayer%dTeam", playerNumber);
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", tempbuf, &ud.config.SplitScreenPlayerTeam[i]);

        Bsprintf(tempbuf, "SplitScreenPlayer%dName", playerNumber);
        SCRIPT_GetString(ud.config.scripthandle, "Controls", tempbuf, ud.config.SplitScreenPlayerName[i]);

        CONFIG_ClampPlayerName(ud.config.SplitScreenPlayerName[i]);
        if (CONFIG_IsBlankPlayerName(ud.config.SplitScreenPlayerName[i]))
        {
            if (i == 0 && !CONFIG_IsBlankPlayerName(szPlayerName))
                Bstrncpyz(ud.config.SplitScreenPlayerName[i], szPlayerName, sizeof(ud.config.SplitScreenPlayerName[i]));
            else
                CONFIG_SetDefaultSplitScreenPlayerName(i);
        }

        ud.config.SplitScreenPlayerAutoAim[i] = clamp(ud.config.SplitScreenPlayerAutoAim[i], 0, 2);
        ud.config.SplitScreenPlayerAlwaysRun[i] = ud.config.SplitScreenPlayerAlwaysRun[i] != 0;
        ud.config.SplitScreenPlayerWeaponSwitch[i] = clamp(ud.config.SplitScreenPlayerWeaponSwitch[i], 0, 7);
        ud.config.SplitScreenPlayerColor[i] = clamp(ud.config.SplitScreenPlayerColor[i], 0, MAXPALOOKUPS - 1);
        ud.config.SplitScreenPlayerTeam[i] = clamp(ud.config.SplitScreenPlayerTeam[i], 0, 3);
    }

    ud.config.AutoAim = ud.config.SplitScreenPlayerAutoAim[0];

    if (!CommandName)
    {
        if (CONFIG_IsBlankPlayerName(ud.config.SplitScreenPlayerName[0]))
            CONFIG_SetDefaultSplitScreenPlayerName(0);
        Bstrncpyz(szPlayerName, ud.config.SplitScreenPlayerName[0], sizeof(szPlayerName));
    }

    // restore localization
    char locale[64] = "en";
    SCRIPT_GetString(ud.config.scripthandle, "Misc", "Locale", &locale[0]);
    localeSetCurrent(locale);

    ud.config.setupread = 1;
    return 0;
}

void CONFIG_ReadSettings(void)
{
    char *dummy = NULL;
    char *const setupFileName = Xstrdup(g_setupFileName);
    char *const p = Bstrtoken(setupFileName, ".", &dummy, 1);

    if (!p || !Bstrcmp(g_setupFileName, SETUPFILENAME))
        Bsprintf(tempbuf, "settings.cfg");
    else
        Bsprintf(tempbuf, "%s_settings.cfg", p);

    Xfree(setupFileName);

    OSD_Exec(tempbuf);

    ud.config.setupread = 2;

    return;
}

void CONFIG_WriteSettings(void) // save binds and aliases to <cfgname>_settings.cfg
{
    if (ud.config.setupread != 2) return;

    char *dummy = NULL;
    char filename[BMAX_PATH];

    if (!Bstrcmp(g_setupFileName, SETUPFILENAME))
        Bsprintf(filename, "settings.cfg");
    else
        Bsprintf(filename, "%s_settings.cfg", Bstrtoken(g_setupFileName, ".", &dummy, 1));

    buildvfs_FILE fp = buildvfs_fopen_write(filename);

    if (fp)
    {
        buildvfs_fputstr(fp, "// this file is automatically generated by ");
        buildvfs_fputstrptr(fp, AppProperName);
        buildvfs_fputstr(fp,"\nunbindall\n");

        for (int i=0; i<MAXBOUNDKEYS+MAXMOUSEBUTTONS; i++)
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

        for (int i=0; i<NUMGAMEFUNCTIONS; ++i)
        {
            char const * name = CONFIG_FunctionNumToName(i);
            if (name && name[0] != '\0' && (ud.config.KeyboardKeys[i][0] == 0xff || !ud.config.KeyboardKeys[i][0]))
            {
                buildvfs_fputstr(fp, "unbound ");
                buildvfs_fputstrptr(fp, name);
                buildvfs_fputstr(fp, "\n");
            }
        }

        OSD_WriteAliases(fp);

        if (g_crosshairSum != -1 && g_crosshairSum != DefaultCrosshairColors.r+(DefaultCrosshairColors.g<<8)+(DefaultCrosshairColors.b<<16))
        {
            buildvfs_fputstr(fp, "crosshaircolor ");
            char buf[64];
            snprintf(buf, sizeof(buf), "%d %d %d\n", CrosshairColors.r, CrosshairColors.g, CrosshairColors.b);
            buildvfs_fputstrptr(fp, buf);
        }

        OSD_WriteCvars(fp);

        buildvfs_fclose(fp);

        LOG_F(INFO, "Wrote %s", filename);

        return;
    }

    LOG_F(ERROR, "Unable to write %s: %s.", filename, strerror(errno));
}

void CONFIG_WriteSetup(uint32_t flags)
{
    if (!ud.config.setupread) return;

    CONFIG_NormalizeControllerViewCenteringSettings();

    if (ud.config.scripthandle < 0)
        ud.config.scripthandle = SCRIPT_Init(g_setupFileName);

    SCRIPT_PutNumber(ud.config.scripthandle, "Misc", "Executions", ud.executions, FALSE, FALSE);

    SCRIPT_PutNumber(ud.config.scripthandle, "Setup", "CacheSize", MAXCACHE1DSIZE, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Setup", "ConfigVersion", BYTEVERSION_EDUKE32, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Setup", "ForceSetup", ud.setup.forcesetup, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Setup", "NoAutoLoad", ud.setup.noautoload, FALSE, FALSE);

#ifdef POLYMER
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "Polymer", glrendmode == REND_POLYMER, FALSE, FALSE);
#endif

    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "ScreenBPP", ud.setup.bpp, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "ScreenDisplay", r_displayindex, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "ScreenHeight", ud.setup.ydim, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "ScreenMode", ud.setup.fullscreen, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "ScreenWidth", ud.setup.xdim, FALSE, FALSE);

    if (g_grpNamePtr && !g_addonNum)
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

    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "MaxRefreshFreq", maxrefreshfreq, FALSE, FALSE);

    if (g_windowPosValid)
    {
        SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "WindowPosX", g_windowPos.x, FALSE, FALSE);
        SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "WindowPosY", g_windowPos.y, FALSE, FALSE);
    }

    if (!NAM_WW2GI)
    {
        SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "Out",ud.lockout,FALSE,FALSE);
        SCRIPT_PutString(ud.config.scripthandle, "Screen Setup", "Password",ud.pwlockout);
    }

#ifdef _WIN32
    SCRIPT_PutNumber(ud.config.scripthandle, "Updates", "CheckForUpdates", ud.config.CheckForUpdates, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Updates", "LastUpdateCheck", ud.config.LastUpdateCheck, FALSE, FALSE);
#endif

    SCRIPT_PutNumber(ud.config.scripthandle, "Game Setup", "PerPlayerPickups", ud.m_weaponsharing, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Game Setup", "HudStyle", ud.config.SplitScreenHudStyle, FALSE, FALSE);

    if (ud.setup.usemouse)
    {
        for (int i=0; i<MAXMOUSEBUTTONS; i++)
        {
            Bsprintf(buf, "MouseButton%d", i);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.MouseFunctions[i][0]));

            if (i >= (MAXMOUSEBUTTONS-2)) continue;

            Bsprintf(buf, "MouseButtonClicked%d", i);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.MouseFunctions[i][1]));
        }
    }

    if (ud.setup.usejoystick)
    {
        for (int dummy=0; dummy<MAXJOYBUTTONSANDHATS; dummy++)
        {
            Bsprintf(buf, "ControllerButton%d", dummy);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.JoystickFunctions[dummy][0]));

            Bsprintf(buf, "ControllerButtonClicked%d", dummy);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.JoystickFunctions[dummy][1]));
        }
        for (int dummy=0; dummy<MAXJOYAXES; dummy++)
        {
            Bsprintf(buf, "ControllerAnalogAxes%d", dummy);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_AnalogNumToName(ud.config.JoystickAnalogueAxes[dummy]));

            Bsprintf(buf, "ControllerDigitalAxes%d_0", dummy);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.JoystickDigitalFunctions[dummy][0]));

            Bsprintf(buf, "ControllerDigitalAxes%d_1", dummy);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.JoystickDigitalFunctions[dummy][1]));

            Bsprintf(buf, "ControllerAnalogSensitivity%d", dummy);
            SCRIPT_PutDouble(ud.config.scripthandle, "Controls", buf, ud.config.JoystickAnalogueSensitivity[dummy], FALSE);

            Bsprintf(buf, "ControllerAnalogInvert%d", dummy);
            SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.JoystickAnalogueInvert[dummy], FALSE, FALSE);

            Bsprintf(buf, "ControllerAnalogDead%d", dummy);
            SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.JoystickAnalogueDead[dummy], FALSE, FALSE);

            Bsprintf(buf, "ControllerAnalogSaturate%d", dummy);
            SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.JoystickAnalogueSaturate[dummy], FALSE, FALSE);
        }

        SCRIPT_PutNumber(ud.config.scripthandle, "Controls", "ControllerAimWeight", ud.config.JoystickAimWeight, FALSE, FALSE);
        SCRIPT_PutNumber(ud.config.scripthandle, "Controls", "ControllerViewCentering", ud.config.JoystickViewCentering, FALSE, FALSE);
        SCRIPT_PutNumber(ud.config.scripthandle, "Controls", "ControllerAimAssist", ud.config.JoystickAimAssist, FALSE, FALSE);
    }

    for (int controllerIndex = 1; controllerIndex < MAXSPLITSCREENCONTROLLERS; ++controllerIndex)
    {
        int const profile = controllerIndex - 1;
        int const controllerNumber = controllerIndex + 1;

        for (int dummy=0; dummy<MAXJOYBUTTONSANDHATS; dummy++)
        {
            Bsprintf(buf, "SplitScreenController%dButton%d", controllerNumber, dummy);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.SplitScreenJoystickFunctions[profile][dummy][0]));

            Bsprintf(buf, "SplitScreenController%dButtonClicked%d", controllerNumber, dummy);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.SplitScreenJoystickFunctions[profile][dummy][1]));
        }

        for (int dummy=0; dummy<MAXJOYAXES; dummy++)
        {
            Bsprintf(buf, "SplitScreenController%dAnalogAxes%d", controllerNumber, dummy);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_AnalogNumToName(ud.config.SplitScreenJoystickAnalogueAxes[profile][dummy]));

            Bsprintf(buf, "SplitScreenController%dDigitalAxes%d_0", controllerNumber, dummy);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.SplitScreenJoystickDigitalFunctions[profile][dummy][0]));

            Bsprintf(buf, "SplitScreenController%dDigitalAxes%d_1", controllerNumber, dummy);
            SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.SplitScreenJoystickDigitalFunctions[profile][dummy][1]));

            Bsprintf(buf, "SplitScreenController%dAnalogSensitivity%d", controllerNumber, dummy);
            SCRIPT_PutDouble(ud.config.scripthandle, "Controls", buf, ud.config.SplitScreenJoystickAnalogueSensitivity[profile][dummy], FALSE);

            Bsprintf(buf, "SplitScreenController%dAnalogInvert%d", controllerNumber, dummy);
            SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.SplitScreenJoystickAnalogueInvert[profile][dummy], FALSE, FALSE);

            Bsprintf(buf, "SplitScreenController%dAnalogDead%d", controllerNumber, dummy);
            SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.SplitScreenJoystickAnalogueDead[profile][dummy], FALSE, FALSE);

            Bsprintf(buf, "SplitScreenController%dAnalogSaturate%d", controllerNumber, dummy);
            SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.SplitScreenJoystickAnalogueSaturate[profile][dummy], FALSE, FALSE);
        }

        Bsprintf(buf, "SplitScreenController%dAimWeight", controllerNumber);
        SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.SplitScreenJoystickAimWeight[profile], FALSE, FALSE);

        Bsprintf(buf, "SplitScreenController%dViewCentering", controllerNumber);
        SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.SplitScreenJoystickViewCentering[profile], FALSE, FALSE);

        Bsprintf(buf, "SplitScreenController%dAimAssist", controllerNumber);
        SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.SplitScreenJoystickAimAssist[profile], FALSE, FALSE);
    }

    SCRIPT_PutNumber(ud.config.scripthandle, "Controls", "UseJoystick", ud.setup.usejoystick, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Controls", "UseMouse", ud.setup.usemouse, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Controls", "SeparateKeyboardMouseAndControllers", ud.config.SplitScreenSeparateKeyboardMouse, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Controls", "ViewCenteringDefaultMigrated", g_controllerViewCenteringDefaultMigrated, FALSE, FALSE);
    if (!CommandName && !CONFIG_IsBlankPlayerName(szPlayerName))
        Bstrncpyz(ud.config.SplitScreenPlayerName[0], szPlayerName, sizeof(ud.config.SplitScreenPlayerName[0]));

    ud.config.AutoAim = clamp(ud.config.AutoAim, 0, 2);
    ud.config.SplitScreenPlayerAutoAim[0] = ud.config.AutoAim;

    for (int i = 0; i < MAXSPLITSCREENCONTROLLERS; ++i)
    {
        int const playerNumber = i + 1;

        CONFIG_ClampPlayerName(ud.config.SplitScreenPlayerName[i]);
        if (CONFIG_IsBlankPlayerName(ud.config.SplitScreenPlayerName[i]))
            CONFIG_SetDefaultSplitScreenPlayerName(i);

        Bsprintf(buf, "SplitScreenPlayer%dAutoAim", playerNumber);
        SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.SplitScreenPlayerAutoAim[i], FALSE, FALSE);

        Bsprintf(buf, "SplitScreenPlayer%dAlwaysRun", playerNumber);
        SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.SplitScreenPlayerAlwaysRun[i], FALSE, FALSE);

        Bsprintf(buf, "SplitScreenPlayer%dWeaponSwitch", playerNumber);
        SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.SplitScreenPlayerWeaponSwitch[i], FALSE, FALSE);

        Bsprintf(buf, "SplitScreenPlayer%dColor", playerNumber);
        SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.SplitScreenPlayerColor[i], FALSE, FALSE);

        Bsprintf(buf, "SplitScreenPlayer%dTeam", playerNumber);
        SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.SplitScreenPlayerTeam[i], FALSE, FALSE);

        Bsprintf(buf, "SplitScreenPlayer%dName", playerNumber);
        SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, ud.config.SplitScreenPlayerName[i]);
    }

    if (!CommandName)
    {
        Bstrncpyz(szPlayerName, ud.config.SplitScreenPlayerName[0], sizeof(szPlayerName));
        SCRIPT_PutString(ud.config.scripthandle, "Comm Setup","PlayerName",&ud.config.SplitScreenPlayerName[0][0]);
    }

    SCRIPT_PutString(ud.config.scripthandle, "Comm Setup","RTSName",&ud.rtsname[0]);

    char commmacro[] = "CommbatMacro# ";

    for (int dummy = 0; dummy < MAXRIDECULE; dummy++)
    {
        commmacro[13] = dummy+'0';
        SCRIPT_PutString(ud.config.scripthandle, "Comm Setup",commmacro,&ud.ridecule[dummy][0]);
    }

    // save localization
    const char* locale = localeGetCurrent();
    SCRIPT_PutString(ud.config.scripthandle, "Misc", "Locale", locale ? locale : "en");

    SCRIPT_Save(ud.config.scripthandle, g_setupFileName);

    if ((flags & 2) == 0)
        SCRIPT_Free(ud.config.scripthandle);

    LOG_F(INFO, "Wrote %s",g_setupFileName);
    CONFIG_WriteSettings();
    Bfflush(NULL);
}

char const * CONFIG_GetGameFuncOnKeyboard(int gameFunc)
{
    const char * string0 = KB_ScanCodeToString(ud.config.KeyboardKeys[gameFunc][0]);
    return string0[0] == '\0' ? KB_ScanCodeToString(ud.config.KeyboardKeys[gameFunc][1]) : string0;
}

const char mbtn_to_name[MAXMOUSEBUTTONS][2][32] = {
    {"Left Mouse",       "Double Left Mouse"},
    {"Right Mouse",      "Double Right Mouse"},
    {"Middle Mouse",     "Double Middle Mouse"},
    {"Mouse 4",   "Double Mouse 4"},
    {"Wheel Up",   ""},
    {"Wheel Down", ""},
    {"Mouse 5",   "Double Mouse 5"},
    {"Mouse 6",   "Double Mouse 6"},
    {"Mouse 7",   "Double Mouse 7"},
    {"Mouse 8",   "Double Mouse 8"},
};

char const * CONFIG_GetGameFuncOnMouse(int gameFunc)
{
    for (int j = 0; j < 2; ++j)
        for (int i = 0; i < MAXMOUSEBUTTONS; ++i)
            if (ud.config.MouseFunctions[i][j] == gameFunc)
                return mbtn_to_name[i][j];

    return "";
}

char const * CONFIG_GetGameFuncOnJoystick(int gameFunc)
{
    for (int j = 0; j < 2; ++j)
        for (int i = 0; i < joystick.numButtons; ++i)
            if (ud.config.JoystickFunctions[i][j] == gameFunc)
                return joyGetName(1, i);

    for (int i = 0; i < joystick.numAxes; ++i)
        for (int j = 0; j < 2; ++j)
            if (ud.config.JoystickDigitalFunctions[i][j] == gameFunc)
                return joyGetName(0, i);

    return "";
}

static const char *CONFIG_GetMapEntryName(char m[], char const * const mapname)
{
    strcpy(m, mapname);

    char *p = strrchr(m, '/');
    if (!p) p = strrchr(m, '\\');
    if (p) Bmemmove(m, p, Bstrlen(p)+1);
    for (p=m; *p; p++) *p = tolower(*p);

    // cheap hack because SCRIPT_GetNumber doesn't like the slashes
    p = m;
    while (*p == '/') p++;

    return p;
}

static void CONFIG_GetMD4EntryName(char m[], uint8_t const * const md4)
{
    sprintf(m, "MD4_%08x%08x%08x%08x",
            B_BIG32(B_UNBUF32(&md4[0])), B_BIG32(B_UNBUF32(&md4[4])),
            B_BIG32(B_UNBUF32(&md4[8])), B_BIG32(B_UNBUF32(&md4[12])));
}

int32_t CONFIG_GetMapBestTime(char const * const mapname, uint8_t const * const mapmd4)
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

int CONFIG_SetMapBestTime(uint8_t const * const mapmd4, int32_t tm)
{
    if (ud.config.scripthandle < 0 && (ud.config.scripthandle = SCRIPT_Init(g_setupFileName)) < 0)
        return -1;

    char m[37];

    CONFIG_GetMD4EntryName(m, mapmd4);
    SCRIPT_PutNumber(ud.config.scripthandle, "MapTimes", m, tm, FALSE, FALSE);

    return 0;
}
