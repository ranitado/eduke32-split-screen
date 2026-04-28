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

#include "splitscreen_input.h"

#include "build.h"
#include "cmdline.h"
#include "duke3d.h"
#include "global.h"
#include "input.h"
#include "config.h"
#include "menus.h"
#include "player.h"
#include "sounds.h"
#include "splitscreen.h"
#include "text.h"

#include "baselayer.h"

namespace
{
constexpr int DIGITAL_AXIS_DEADZONE = 12000;
constexpr int RUN_TRIGGER_DEADZONE  = 24000;
constexpr int RUN_TRIGGER_DELTA     = 6000;
constexpr int NORMALKEYMOVE     = 40;  // KEEPINSYNC player.cpp
constexpr int WALKKEYMOVE       = 50;
constexpr int RUNKEYMOVE        = NORMALKEYMOVE << 1;
constexpr float TURN_RATE       = 28.f;
constexpr float VERTICAL_AIM_FEEL_SCALE = 1.2f;
constexpr float LOOK_RATE       = TURN_RATE * 0.25f * VERTICAL_AIM_FEEL_SCALE;
constexpr float PRECISION_AIM_SENS_X = 1.f;
constexpr float PRECISION_AIM_SENS_Y = 1.f;
constexpr float AIM_REFERENCE_FPS = 60.f;
constexpr int MAX_LOCAL_PLAYERS = 4;
constexpr uint8_t WEAPON_REPEAT_INITIAL_DELAY = 12;
constexpr uint8_t WEAPON_REPEAT_INTERVAL = 4;
constexpr uint8_t WEAPON_PULSE_FRAMES = 2;
constexpr uint8_t PAUSE_PULSE_FRAMES = 3;

enum split_config_menu_page_t
{
    SPLIT_CONFIG_MENU_CLOSED,
    SPLIT_CONFIG_MENU_MAIN,
    SPLIT_CONFIG_MENU_PLAYER,
    SPLIT_CONFIG_MENU_CONTROL,
    SPLIT_CONFIG_MENU_CONTROLLER,
    SPLIT_CONFIG_MENU_DISCONNECT_CONFIRM,
};

struct split_config_menu_state_t
{
    split_config_menu_page_t page;
    int current;
};

enum : int
{
    GP_A = 0,
    GP_B,
    GP_X,
    GP_Y,
    GP_BACK,
    GP_GUIDE,
    GP_START,
    GP_LEFTSTICK,
    GP_RIGHTSTICK,
    GP_LEFTSHOULDER,
    GP_RIGHTSHOULDER,
    GP_DPAD_UP,
    GP_DPAD_DOWN,
    GP_DPAD_LEFT,
    GP_DPAD_RIGHT,
};

static input_t g_splitScreenLocalInputs[MAXPLAYERS];
static uint32_t g_splitScreenPrevButtons[MAXPLAYERS];
static uint8_t g_splitScreenPrevAxisActive[MAXPLAYERS][GAMEPAD_AXIS_COUNT][2];
static int16_t g_splitScreenRunTriggerBaseline[MAXPLAYERS][GAMEPAD_AXIS_COUNT];
static uint8_t g_splitScreenRunTriggerBaselineSet[MAXPLAYERS][GAMEPAD_AXIS_COUNT];
static uint8_t g_splitScreenPadConnected[MAXPLAYERS];
static int8_t g_splitScreenWeaponRepeatDir[MAXPLAYERS];
static uint8_t g_splitScreenWeaponRepeatDelay[MAXPLAYERS];
static int8_t g_splitScreenWeaponPulseDir[MAXPLAYERS];
static uint8_t g_splitScreenWeaponPulseFrames[MAXPLAYERS];
static uint8_t g_splitScreenPausePulseFrames[MAXPLAYERS];
static split_config_menu_state_t g_splitScreenConfigMenu[MAXPLAYERS];
static uint32_t g_splitScreenContinuePrevButtons[MAX_LOCAL_PLAYERS];
static uint32_t g_splitScreenJoinPrevButtons[MAX_LOCAL_PLAYERS];
static uint8_t g_splitScreenJoinSuppressFrames[MAXPLAYERS];
static uint64_t g_splitScreenAimLastTicks[MAXPLAYERS];

static char const * const s_splitMenuAutoAimNames[] = { "Never", "Always", "Hitscan only" };
static int32_t const s_splitMenuAutoAimValues[] = { 0, 1, 2 };
static char const * const s_splitMenuNoYesNames[] = { "Off", "On" };
static char const * const s_splitMenuEquipPickupNames[] = { "Never", "If new" };
static char const * const s_splitMenuColorNames[] = { "Auto", "Blue", "Red", "Green", "Gray", "Dark gray", "Dark green", "Brown", "Dark blue", "Bright red", "Yellow" };
static int32_t const s_splitMenuColorValues[] = { 0, 9, 10, 11, 12, 13, 14, 15, 16, 21, 23 };
static char const * const s_splitMenuTeamNames[] = { "Blue", "Red", "Green", "Gray" };

static void G_PlaySplitScreenMenuSound(int const sound)
{
    S_PlaySound(sound);
}

static bool G_HasNativeSplitScreenGamepads(void)
{
    return G_HaveSplitScreen();
}

static int G_GetFirstGamepadViewIndex(void)
{
    return ud.config.SplitScreenSeparateKeyboardMouse ? 1 : 0;
}

static int G_GetGamepadIndexForPlayer(int const playerNum)
{
    return ud.config.SplitScreenSeparateKeyboardMouse ? playerNum - 1 : playerNum;
}

static int G_GetPlayerForGamepadIndex(int const gamepadIndex)
{
    return ud.config.SplitScreenSeparateKeyboardMouse ? gamepadIndex + 1 : gamepadIndex;
}

static bool G_GamepadButtonHeld(uint32_t const buttons, int const button)
{
    return (buttons & (1u << button)) != 0;
}

static bool G_GamepadButtonPressed(uint32_t const buttons, uint32_t const previousButtons, int const button)
{
    uint32_t const mask = (1u << button);
    return (buttons & mask) != 0 && (previousButtons & mask) == 0;
}

static int G_GetSplitScreenContinueViewCount(void)
{
    return G_HasNativeSplitScreenGamepads() ? min<int32_t>(G_GetSplitScreenPlayerCount(), MAX_LOCAL_PLAYERS) : 0;
}

static void G_ClearSplitScreenContinueInputLocal(void)
{
    int const viewCount = G_GetSplitScreenContinueViewCount();
    int const firstGamepadViewIndex = G_GetFirstGamepadViewIndex();

    for (int gamepadIndex = 0; gamepadIndex < MAX_LOCAL_PLAYERS; ++gamepadIndex)
        g_splitScreenContinuePrevButtons[gamepadIndex] = 0;

    for (int viewIndex = firstGamepadViewIndex; viewIndex < viewCount; ++viewIndex)
    {
        int const playerNum = G_GetSplitScreenPlayer(viewIndex);
        int const gamepadIndex = G_GetGamepadIndexForPlayer(playerNum);
        if ((unsigned)gamepadIndex >= MAX_LOCAL_PLAYERS)
            continue;

        gamepadstate_t state {};
        g_splitScreenContinuePrevButtons[gamepadIndex] = joyGetGamepadState(gamepadIndex, &state) == 0 ? state.buttons : 0;
    }
}

static int32_t G_CheckSplitScreenContinueInputLocal(void)
{
    int const viewCount = G_GetSplitScreenContinueViewCount();
    int const firstGamepadViewIndex = G_GetFirstGamepadViewIndex();
    int32_t pressed = 0;

    for (int viewIndex = firstGamepadViewIndex; viewIndex < viewCount; ++viewIndex)
    {
        int const playerNum = G_GetSplitScreenPlayer(viewIndex);
        int const gamepadIndex = G_GetGamepadIndexForPlayer(playerNum);
        if ((unsigned)gamepadIndex >= MAX_LOCAL_PLAYERS)
            continue;

        gamepadstate_t state {};
        uint32_t const buttons = joyGetGamepadState(gamepadIndex, &state) == 0 ? state.buttons : 0;

        pressed |= (buttons & ~g_splitScreenContinuePrevButtons[gamepadIndex]) != 0;
        g_splitScreenContinuePrevButtons[gamepadIndex] = buttons;
    }

    return pressed;
}

static int G_GetControllerProfileForPlayer(int const playerNum)
{
    return clamp(playerNum, 0, MAXSPLITSCREENCONTROLLERS - 1);
}

static int32_t const (*G_GetConfiguredButtonFunctions(int const controllerProfile))[2]
{
    return controllerProfile == 0 ? ud.config.JoystickFunctions : ud.config.SplitScreenJoystickFunctions[controllerProfile - 1];
}

static int32_t const (*G_GetConfiguredDigitalFunctions(int const controllerProfile))[2]
{
    return controllerProfile == 0 ? ud.config.JoystickDigitalFunctions : ud.config.SplitScreenJoystickDigitalFunctions[controllerProfile - 1];
}

static int32_t const *G_GetConfiguredAnalogAxes(int const controllerProfile)
{
    return controllerProfile == 0 ? ud.config.JoystickAnalogueAxes : ud.config.SplitScreenJoystickAnalogueAxes[controllerProfile - 1];
}

static float const *G_GetConfiguredAnalogSensitivity(int const controllerProfile)
{
    return controllerProfile == 0 ? ud.config.JoystickAnalogueSensitivity : ud.config.SplitScreenJoystickAnalogueSensitivity[controllerProfile - 1];
}

static int32_t const *G_GetConfiguredAnalogInvert(int const controllerProfile)
{
    return controllerProfile == 0 ? ud.config.JoystickAnalogueInvert : ud.config.SplitScreenJoystickAnalogueInvert[controllerProfile - 1];
}

static int32_t const *G_GetConfiguredAnalogDead(int const controllerProfile)
{
    return controllerProfile == 0 ? ud.config.JoystickAnalogueDead : ud.config.SplitScreenJoystickAnalogueDead[controllerProfile - 1];
}

static int32_t const *G_GetConfiguredAnalogSaturate(int const controllerProfile)
{
    return controllerProfile == 0 ? ud.config.JoystickAnalogueSaturate : ud.config.SplitScreenJoystickAnalogueSaturate[controllerProfile - 1];
}

static int32_t G_GetConfiguredAimWeight(int const controllerProfile)
{
    return controllerProfile == 0 ? ud.config.JoystickAimWeight : ud.config.SplitScreenJoystickAimWeight[controllerProfile - 1];
}

static int32_t G_GetConfiguredViewCentering(int const controllerProfile)
{
    int32_t const viewCentering = controllerProfile == 0 ? ud.config.JoystickViewCentering : ud.config.SplitScreenJoystickViewCentering[controllerProfile - 1];
    return CONFIG_NormalizeControllerViewCentering(viewCentering);
}

static int32_t G_GetConfiguredAimAssist(int const controllerProfile)
{
    return controllerProfile == 0 ? ud.config.JoystickAimAssist : ud.config.SplitScreenJoystickAimAssist[controllerProfile - 1];
}

static float *G_GetMutableConfiguredAnalogSensitivity(int const controllerProfile)
{
    return controllerProfile == 0 ? ud.config.JoystickAnalogueSensitivity : ud.config.SplitScreenJoystickAnalogueSensitivity[controllerProfile - 1];
}

static int32_t *G_GetMutableConfiguredAnalogInvert(int const controllerProfile)
{
    return controllerProfile == 0 ? ud.config.JoystickAnalogueInvert : ud.config.SplitScreenJoystickAnalogueInvert[controllerProfile - 1];
}

static int32_t *G_GetMutableConfiguredViewCentering(int const controllerProfile)
{
    return controllerProfile == 0 ? &ud.config.JoystickViewCentering : &ud.config.SplitScreenJoystickViewCentering[controllerProfile - 1];
}

static bool G_GamepadButtonIndexHeld(gamepadstate_t const &state, int const button)
{
    return (unsigned)button < 32u && G_GamepadButtonHeld(state.buttons, button);
}

static bool G_GamepadButtonIndexPressed(gamepadstate_t const &state, uint32_t const previousButtons, int const button)
{
    return (unsigned)button < 32u && G_GamepadButtonPressed(state.buttons, previousButtons, button);
}

static bool G_GamepadDigitalAxisHeld(gamepadstate_t const &state, int const axis, int const polarity)
{
    if ((unsigned)axis >= GAMEPAD_AXIS_COUNT)
        return false;

    return polarity ? state.axes[axis] > DIGITAL_AXIS_DEADZONE : state.axes[axis] < -DIGITAL_AXIS_DEADZONE;
}

static bool G_GamepadRunTriggerHeld(int const playerNum, gamepadstate_t const &state, int const axis, int const polarity)
{
    if ((unsigned)playerNum >= MAXPLAYERS || (unsigned)axis >= GAMEPAD_AXIS_COUNT)
        return false;

    if (axis != GAMEPAD_AXIS_TRIGGERLEFT && axis != GAMEPAD_AXIS_TRIGGERRIGHT)
        return G_GamepadDigitalAxisHeld(state, axis, polarity);

    int16_t const value = state.axes[axis];

    if (!g_splitScreenRunTriggerBaselineSet[playerNum][axis])
    {
        g_splitScreenRunTriggerBaseline[playerNum][axis] = value;
        g_splitScreenRunTriggerBaselineSet[playerNum][axis] = 1;
        return false;
    }

    int16_t &baseline = g_splitScreenRunTriggerBaseline[playerNum][axis];
    if (polarity ? value < baseline : value > baseline)
        baseline = value;

    return polarity
        ? value > max<int>(RUN_TRIGGER_DEADZONE, baseline + RUN_TRIGGER_DELTA)
        : value < min<int>(-RUN_TRIGGER_DEADZONE, baseline - RUN_TRIGGER_DELTA);
}

static bool G_GamepadDigitalAxisPressed(int const playerNum, gamepadstate_t const &state, int const axis, int const polarity)
{
    return G_GamepadDigitalAxisHeld(state, axis, polarity) && !g_splitScreenPrevAxisActive[playerNum][axis][polarity];
}

static bool G_GamepadFunctionHeld(int const controllerProfile, gamepadstate_t const &state, int const gameFunction)
{
    if ((unsigned)gameFunction >= NUMGAMEFUNCTIONS)
        return false;

    auto const buttonFunctions = G_GetConfiguredButtonFunctions(controllerProfile);
    for (int button = 0; button < MAXJOYBUTTONSANDHATS; ++button)
        if (buttonFunctions[button][0] == gameFunction && G_GamepadButtonIndexHeld(state, button))
            return true;

    auto const digitalFunctions = G_GetConfiguredDigitalFunctions(controllerProfile);
    for (int axis = 0; axis < GAMEPAD_AXIS_COUNT; ++axis)
        for (int polarity = 0; polarity < 2; ++polarity)
            if (digitalFunctions[axis][polarity] == gameFunction && G_GamepadDigitalAxisHeld(state, axis, polarity))
                return true;

    return false;
}

static bool G_GamepadRunHeld(int const playerNum, int const controllerProfile, gamepadstate_t const &state)
{
    auto const buttonFunctions = G_GetConfiguredButtonFunctions(controllerProfile);
    for (int button = 0; button < MAXJOYBUTTONSANDHATS; ++button)
        if (buttonFunctions[button][0] == gamefunc_Run && G_GamepadButtonIndexHeld(state, button))
            return true;

    auto const digitalFunctions = G_GetConfiguredDigitalFunctions(controllerProfile);
    for (int axis = 0; axis < GAMEPAD_AXIS_COUNT; ++axis)
        for (int polarity = 0; polarity < 2; ++polarity)
            if (digitalFunctions[axis][polarity] == gamefunc_Run && G_GamepadRunTriggerHeld(playerNum, state, axis, polarity))
                return true;

    return false;
}

static bool G_GamepadFunctionPressed(int const playerNum, int const controllerProfile, gamepadstate_t const &state, int const gameFunction)
{
    if ((unsigned)gameFunction >= NUMGAMEFUNCTIONS)
        return false;

    auto const buttonFunctions = G_GetConfiguredButtonFunctions(controllerProfile);
    for (int button = 0; button < MAXJOYBUTTONSANDHATS; ++button)
        if (buttonFunctions[button][0] == gameFunction && G_GamepadButtonIndexPressed(state, g_splitScreenPrevButtons[playerNum], button))
            return true;

    auto const digitalFunctions = G_GetConfiguredDigitalFunctions(controllerProfile);
    for (int axis = 0; axis < GAMEPAD_AXIS_COUNT; ++axis)
        for (int polarity = 0; polarity < 2; ++polarity)
            if (digitalFunctions[axis][polarity] == gameFunction && G_GamepadDigitalAxisPressed(playerNum, state, axis, polarity))
                return true;

    return false;
}

static void G_UpdatePreviousGamepadState(int const playerNum, gamepadstate_t const &state)
{
    g_splitScreenPrevButtons[playerNum] = state.buttons;

    for (int axis = 0; axis < GAMEPAD_AXIS_COUNT; ++axis)
    {
        g_splitScreenPrevAxisActive[playerNum][axis][0] = G_GamepadDigitalAxisHeld(state, axis, 0);
        g_splitScreenPrevAxisActive[playerNum][axis][1] = G_GamepadDigitalAxisHeld(state, axis, 1);
    }
}

static int G_FindConfiguredAnalogAxis(int const controllerProfile, int const analogFunction, int const fallbackAxis)
{
    auto const analogAxes = G_GetConfiguredAnalogAxes(controllerProfile);

    for (int axis = 0; axis < GAMEPAD_AXIS_COUNT; ++axis)
        if (analogAxes[axis] == analogFunction)
            return axis;

    return fallbackAxis;
}

static int16_t G_ApplyConfiguredAxisSettings(int const controllerProfile, int const axis, int16_t const value)
{
    if ((unsigned)axis >= GAMEPAD_AXIS_COUNT)
        return 0;

    auto const sensitivity = G_GetConfiguredAnalogSensitivity(controllerProfile);
    auto const invert      = G_GetConfiguredAnalogInvert(controllerProfile);
    auto const dead        = G_GetConfiguredAnalogDead(controllerProfile);
    auto const saturate    = G_GetConfiguredAnalogSaturate(controllerProfile);

    int32_t adjusted = invert[axis] ? -value : value;
    int32_t const sign = adjusted < 0 ? -1 : 1;
    int32_t const magnitude = klabs(adjusted);
    int32_t const deadValue = scale(clamp(dead[axis], 0, 10000), 32767, 10000);
    int32_t const saturateValue = max(deadValue + 1, scale(clamp(saturate[axis], 1, 10000), 32767, 10000));

    if (magnitude <= deadValue)
        return 0;

    adjusted = magnitude >= saturateValue
        ? 32767
        : scale(magnitude - deadValue, 32767, saturateValue - deadValue);

    float const sensitivityScale = sensitivity[axis] > 0.f ? sensitivity[axis] / DEFAULTJOYSTICKANALOGUESENSITIVITY : 1.f;
    adjusted = (int32_t)(adjusted * sensitivityScale);

    return (int16_t)clamp(sign * adjusted, -32767, 32767);
}

static int16_t G_GetConfiguredAnalogValue(int const controllerProfile, gamepadstate_t const &state, int const analogFunction, int const fallbackAxis)
{
    int const axis = G_FindConfiguredAnalogAxis(controllerProfile, analogFunction, fallbackAxis);
    return G_ApplyConfiguredAxisSettings(controllerProfile, axis, state.axes[axis]);
}

static float G_GetPrecisionAimScale(int const controllerProfile, int const analogFunction, int const fallbackAxis, float const precisionSensitivity)
{
    int const axis = G_FindConfiguredAnalogAxis(controllerProfile, analogFunction, fallbackAxis);
    if ((unsigned)axis >= GAMEPAD_AXIS_COUNT)
        return 1.f;

    auto const sensitivity = G_GetConfiguredAnalogSensitivity(controllerProfile);
    float const configuredSensitivity = sensitivity[axis];
    if (configuredSensitivity <= precisionSensitivity)
        return 1.f;

    return precisionSensitivity / configuredSensitivity;
}

static bool G_IsSecondarySplitScreenPlayer(int const playerNum)
{
    return G_HaveSplitScreen() && playerNum != myconnectindex;
}

static void G_CloseSplitScreenConfigMenu(int const playerNum)
{
    if ((unsigned)playerNum >= MAXPLAYERS)
        return;

    g_splitScreenConfigMenu[playerNum] = { SPLIT_CONFIG_MENU_CLOSED, 0 };
}

static void G_CloseAllSplitScreenConfigMenus(void)
{
    for (int playerNum = 0; playerNum < MAXPLAYERS; ++playerNum)
        G_CloseSplitScreenConfigMenu(playerNum);
}

static int G_FindOptionIndex(int32_t const * const values, int const count, int const value)
{
    for (int i = 0; i < count; ++i)
        if (values[i] == value)
            return i;

    return 0;
}

static int G_WrapMenuIndex(int const value, int const count)
{
    if (count <= 0)
        return 0;

    int wrapped = value % count;
    return wrapped < 0 ? wrapped + count : wrapped;
}

static int G_GetEquipPickupOption(int const weaponSwitch)
{
    return (weaponSwitch & 1) != 0;
}

static void G_SetEquipPickupOption(int const playerNum, int const option)
{
    int const index = G_GetSplitScreenPlayerConfigIndex(playerNum);
    int weaponSwitch = ud.config.SplitScreenPlayerWeaponSwitch[index] & ~(1|4);
    if (option)
        weaponSwitch |= 1;

    ud.config.SplitScreenPlayerWeaponSwitch[index] = weaponSwitch;
}

static void G_ApplySplitScreenPlayerSetup(int const playerNum)
{
    if ((unsigned)playerNum >= MAXPLAYERS || g_player[playerNum].ps == nullptr)
        return;

    auto * const ps = g_player[playerNum].ps;
    int const team = G_GetLocalPlayerTeamSetting(playerNum);
    int const color = G_GetLocalPlayerColorSetting(playerNum);
    int const palette = (g_gametypeFlags[ud.coop] & GAMETYPE_TDM) ? G_GetTeamPalette(team) : color;

    Bstrncpyz(g_player[playerNum].user_name, playerNum == myconnectindex ? szPlayerName : ud.config.SplitScreenPlayerName[G_GetSplitScreenPlayerConfigIndex(playerNum)],
              sizeof(g_player[playerNum].user_name));
    g_player[playerNum].pteam = team;
    ps->team = team;
    ps->palookup = g_player[playerNum].pcolor = palette;

    if (sprite[ps->i].picnum == APLAYER && sprite[ps->i].pal != 1)
        sprite[ps->i].pal = g_player[playerNum].pcolor;
}

static void G_SaveSplitScreenPlayerConfig(int const playerNum)
{
    P_ApplyMiscInputSettings(playerNum);
    G_ApplySplitScreenPlayerSetup(playerNum);
    CONFIG_WriteSetup(0);
}

enum split_config_player_entry_t
{
    SPLIT_CONFIG_PLAYER_COLOR,
    SPLIT_CONFIG_PLAYER_TEAM,
    SPLIT_CONFIG_PLAYER_AUTOAIM,
    SPLIT_CONFIG_PLAYER_ALWAYS_RUN,
    SPLIT_CONFIG_PLAYER_EQUIP_PICKUPS,
};

static bool G_ShowSplitScreenPlayerTeamOption(void)
{
    return (g_gametypeFlags[ud.coop] & GAMETYPE_PLAYERSFRIENDLY) == 0;
}

static split_config_player_entry_t G_GetSplitScreenPlayerMenuEntry(int const index)
{
    if (index == 0)
        return SPLIT_CONFIG_PLAYER_COLOR;

    if (G_ShowSplitScreenPlayerTeamOption())
    {
        if (index == 1)
            return SPLIT_CONFIG_PLAYER_TEAM;

        if (index == 2)
            return SPLIT_CONFIG_PLAYER_AUTOAIM;

        return index == 3 ? SPLIT_CONFIG_PLAYER_ALWAYS_RUN : SPLIT_CONFIG_PLAYER_EQUIP_PICKUPS;
    }

    if (index == 1)
        return SPLIT_CONFIG_PLAYER_AUTOAIM;

    return index == 2 ? SPLIT_CONFIG_PLAYER_ALWAYS_RUN : SPLIT_CONFIG_PLAYER_EQUIP_PICKUPS;
}

static int G_GetSplitScreenConfigMenuEntryCount(split_config_menu_page_t const page)
{
    switch (page)
    {
        case SPLIT_CONFIG_MENU_MAIN:       return 3;
        case SPLIT_CONFIG_MENU_PLAYER:     return G_ShowSplitScreenPlayerTeamOption() ? 5 : 4;
        case SPLIT_CONFIG_MENU_CONTROLLER: return 5;
        case SPLIT_CONFIG_MENU_DISCONNECT_CONFIRM: return 2;
        default:                           return 0;
    }
}

static void G_SetSplitScreenConfigMenuPage(int const playerNum, split_config_menu_page_t const page)
{
    if ((unsigned)playerNum >= MAXPLAYERS)
        return;

    g_splitScreenConfigMenu[playerNum].page = page;
    g_splitScreenConfigMenu[playerNum].current = 0;
}

static void G_BackOutOfSplitScreenConfigMenu(int const playerNum)
{
    auto &menu = g_splitScreenConfigMenu[playerNum];

    if (menu.page == SPLIT_CONFIG_MENU_MAIN || menu.page == SPLIT_CONFIG_MENU_CLOSED)
        G_CloseSplitScreenConfigMenu(playerNum);
    else
        G_SetSplitScreenConfigMenuPage(playerNum, SPLIT_CONFIG_MENU_MAIN);
}

static void G_ModifySplitScreenConfigMenuEntry(int const playerNum, int const controllerProfile, int const direction)
{
    auto &menu = g_splitScreenConfigMenu[playerNum];
    int const configIndex = G_GetSplitScreenPlayerConfigIndex(playerNum);
    bool saveConfig = true;

    switch (menu.page)
    {
        case SPLIT_CONFIG_MENU_PLAYER:
            switch (G_GetSplitScreenPlayerMenuEntry(menu.current))
            {
                case SPLIT_CONFIG_PLAYER_COLOR:
                {
                    int option = G_FindOptionIndex(s_splitMenuColorValues, ARRAY_SIZE(s_splitMenuColorValues), ud.config.SplitScreenPlayerColor[configIndex]);
                    option = G_WrapMenuIndex(option + direction, ARRAY_SIZE(s_splitMenuColorValues));
                    ud.config.SplitScreenPlayerColor[configIndex] = s_splitMenuColorValues[option];
                    break;
                }
                case SPLIT_CONFIG_PLAYER_TEAM:
                    ud.config.SplitScreenPlayerTeam[configIndex] = G_WrapMenuIndex(ud.config.SplitScreenPlayerTeam[configIndex] + direction, ARRAY_SIZE(s_splitMenuTeamNames));
                    break;
                case SPLIT_CONFIG_PLAYER_AUTOAIM:
                {
                    int option = G_FindOptionIndex(s_splitMenuAutoAimValues, ARRAY_SIZE(s_splitMenuAutoAimValues), ud.config.SplitScreenPlayerAutoAim[configIndex]);
                    option = G_WrapMenuIndex(option + direction, ARRAY_SIZE(s_splitMenuAutoAimValues));
                    ud.config.SplitScreenPlayerAutoAim[configIndex] = s_splitMenuAutoAimValues[option];
                    break;
                }
                case SPLIT_CONFIG_PLAYER_ALWAYS_RUN:
                    ud.config.SplitScreenPlayerAlwaysRun[configIndex] = !ud.config.SplitScreenPlayerAlwaysRun[configIndex];
                    break;
                case SPLIT_CONFIG_PLAYER_EQUIP_PICKUPS:
                    G_SetEquipPickupOption(playerNum, !G_GetEquipPickupOption(ud.config.SplitScreenPlayerWeaponSwitch[configIndex]));
                    break;
                default:
                    saveConfig = false;
                    break;
            }
            break;

        case SPLIT_CONFIG_MENU_CONTROLLER:
            if (menu.current == 0 || menu.current == 1)
            {
                int const axis = menu.current == 0
                    ? G_FindConfiguredAnalogAxis(controllerProfile, analog_turning, CONTROLLER_AXIS_RIGHTX)
                    : G_FindConfiguredAnalogAxis(controllerProfile, analog_lookingupanddown, CONTROLLER_AXIS_RIGHTY);
                auto * const sensitivity = G_GetMutableConfiguredAnalogSensitivity(controllerProfile);
                sensitivity[axis] = clamp(sensitivity[axis] + direction * 0.1f, 1.f, 10.f);
            }
            else if (menu.current == 2)
            {
                int const axis = G_FindConfiguredAnalogAxis(controllerProfile, analog_lookingupanddown, CONTROLLER_AXIS_RIGHTY);
                auto * const invert = G_GetMutableConfiguredAnalogInvert(controllerProfile);
                invert[axis] = !invert[axis];
            }
            else if (menu.current == 3)
            {
                int32_t * const viewCentering = G_GetMutableConfiguredViewCentering(controllerProfile);
                *viewCentering = CONFIG_AdjustControllerViewCentering(*viewCentering, direction);
            }
            else
                saveConfig = false;
            break;

        default:
            saveConfig = false;
            break;
    }

    if (saveConfig)
        G_SaveSplitScreenPlayerConfig(playerNum);
}

static void G_ActivateSplitScreenConfigMenuEntry(int const playerNum, int const controllerProfile)
{
    auto &menu = g_splitScreenConfigMenu[playerNum];

    switch (menu.page)
    {
        case SPLIT_CONFIG_MENU_MAIN:
            if (menu.current == 0)
                G_SetSplitScreenConfigMenuPage(playerNum, SPLIT_CONFIG_MENU_PLAYER);
            else if (menu.current == 1)
                G_SetSplitScreenConfigMenuPage(playerNum, SPLIT_CONFIG_MENU_CONTROLLER);
            else
                G_SetSplitScreenConfigMenuPage(playerNum, SPLIT_CONFIG_MENU_DISCONNECT_CONFIRM);
            break;

        case SPLIT_CONFIG_MENU_PLAYER:
            G_ModifySplitScreenConfigMenuEntry(playerNum, controllerProfile, 1);
            break;

        case SPLIT_CONFIG_MENU_CONTROLLER:
            if (menu.current == 4)
            {
                if (controllerProfile == 0)
                    CONFIG_SetGameControllerDefaults();
                else
                    CONFIG_SetSplitScreenGameControllerDefaults(controllerProfile);

                G_SaveSplitScreenPlayerConfig(playerNum);
            }
            else
                G_ModifySplitScreenConfigMenuEntry(playerNum, controllerProfile, 1);
            break;

        case SPLIT_CONFIG_MENU_DISCONNECT_CONFIRM:
            if (menu.current == 1 && G_DisconnectSplitScreenPlayer(playerNum) == 0)
                G_CloseSplitScreenConfigMenu(playerNum);
            else
                G_SetSplitScreenConfigMenuPage(playerNum, SPLIT_CONFIG_MENU_MAIN);
            break;

        default:
            break;
    }
}

static void G_UpdateSplitScreenConfigMenu(int const playerNum, int const controllerProfile, gamepadstate_t const &state)
{
    auto &menu = g_splitScreenConfigMenu[playerNum];
    int const entryCount = G_GetSplitScreenConfigMenuEntryCount(menu.page);
    if (entryCount <= 0)
        return;

    bool const upPressed = G_GamepadButtonIndexPressed(state, g_splitScreenPrevButtons[playerNum], GP_DPAD_UP)
                        || G_GamepadDigitalAxisPressed(playerNum, state, GAMEPAD_AXIS_LEFTY, 0);
    bool const downPressed = G_GamepadButtonIndexPressed(state, g_splitScreenPrevButtons[playerNum], GP_DPAD_DOWN)
                          || G_GamepadDigitalAxisPressed(playerNum, state, GAMEPAD_AXIS_LEFTY, 1);
    bool const leftPressed = G_GamepadButtonIndexPressed(state, g_splitScreenPrevButtons[playerNum], GP_DPAD_LEFT)
                          || G_GamepadDigitalAxisPressed(playerNum, state, GAMEPAD_AXIS_LEFTX, 0);
    bool const rightPressed = G_GamepadButtonIndexPressed(state, g_splitScreenPrevButtons[playerNum], GP_DPAD_RIGHT)
                           || G_GamepadDigitalAxisPressed(playerNum, state, GAMEPAD_AXIS_LEFTX, 1);
    bool const acceptPressed = G_GamepadButtonIndexPressed(state, g_splitScreenPrevButtons[playerNum], GP_A);
    bool const backPressed = G_GamepadButtonIndexPressed(state, g_splitScreenPrevButtons[playerNum], GP_B);
    bool const canModifyCurrentEntry = (menu.page == SPLIT_CONFIG_MENU_PLAYER)
                                    || (menu.page == SPLIT_CONFIG_MENU_CONTROLLER && menu.current < 4);

    if (upPressed)
    {
        menu.current = G_WrapMenuIndex(menu.current - 1, entryCount);
        G_PlaySplitScreenMenuSound(KICK_HIT);
    }
    if (downPressed)
    {
        menu.current = G_WrapMenuIndex(menu.current + 1, entryCount);
        G_PlaySplitScreenMenuSound(KICK_HIT);
    }
    if (leftPressed && canModifyCurrentEntry)
    {
        G_ModifySplitScreenConfigMenuEntry(playerNum, controllerProfile, -1);
        G_PlaySplitScreenMenuSound(PISTOL_BODYHIT);
    }
    if (rightPressed && canModifyCurrentEntry)
    {
        G_ModifySplitScreenConfigMenuEntry(playerNum, controllerProfile, 1);
        G_PlaySplitScreenMenuSound(PISTOL_BODYHIT);
    }
    if (acceptPressed)
    {
        G_ActivateSplitScreenConfigMenuEntry(playerNum, controllerProfile);
        G_PlaySplitScreenMenuSound(PISTOL_BODYHIT);
    }
    if (backPressed)
    {
        G_BackOutOfSplitScreenConfigMenu(playerNum);
        G_PlaySplitScreenMenuSound(EXITMENUSOUND);
    }
}

static char const *G_GetIndexedName(char const * const * const names, int const count, int const index)
{
    return names[clamp(index, 0, count - 1)];
}

static void G_FormatSplitScreenConfigMenuLine(int const playerNum, int const controllerProfile, char * const buffer, int const bufferSize)
{
    auto const &menu = g_splitScreenConfigMenu[playerNum];
    int const configIndex = G_GetSplitScreenPlayerConfigIndex(playerNum);

    switch (menu.page)
    {
        case SPLIT_CONFIG_MENU_MAIN:
            if (menu.current == 0)
                Bsnprintf(buffer, bufferSize, "Player Setup");
            else if (menu.current == 1)
                Bsnprintf(buffer, bufferSize, "Controller Setup");
            else
                Bsnprintf(buffer, bufferSize, "Disconnect player");
            break;

        case SPLIT_CONFIG_MENU_PLAYER:
            switch (G_GetSplitScreenPlayerMenuEntry(menu.current))
            {
                case SPLIT_CONFIG_PLAYER_COLOR:
                {
                    int const option = G_FindOptionIndex(s_splitMenuColorValues, ARRAY_SIZE(s_splitMenuColorValues), ud.config.SplitScreenPlayerColor[configIndex]);
                    Bsnprintf(buffer, bufferSize, "Color: %s", G_GetIndexedName(s_splitMenuColorNames, ARRAY_SIZE(s_splitMenuColorNames), option));
                    break;
                }
                case SPLIT_CONFIG_PLAYER_TEAM:
                    Bsnprintf(buffer, bufferSize, "Team: %s", G_GetIndexedName(s_splitMenuTeamNames, ARRAY_SIZE(s_splitMenuTeamNames), ud.config.SplitScreenPlayerTeam[configIndex]));
                    break;
                case SPLIT_CONFIG_PLAYER_AUTOAIM:
                {
                    int const option = G_FindOptionIndex(s_splitMenuAutoAimValues, ARRAY_SIZE(s_splitMenuAutoAimValues), ud.config.SplitScreenPlayerAutoAim[configIndex]);
                    Bsnprintf(buffer, bufferSize, "Auto aim: %s", G_GetIndexedName(s_splitMenuAutoAimNames, ARRAY_SIZE(s_splitMenuAutoAimNames), option));
                    break;
                }
                case SPLIT_CONFIG_PLAYER_ALWAYS_RUN:
                    Bsnprintf(buffer, bufferSize, "Auto run: %s", s_splitMenuNoYesNames[ud.config.SplitScreenPlayerAlwaysRun[configIndex] != 0]);
                    break;
                case SPLIT_CONFIG_PLAYER_EQUIP_PICKUPS:
                    Bsnprintf(buffer, bufferSize, "Equip pickups: %s", s_splitMenuEquipPickupNames[G_GetEquipPickupOption(ud.config.SplitScreenPlayerWeaponSwitch[configIndex])]);
                    break;
                default:
                    buffer[0] = '\0';
                    break;
            }
            break;

        case SPLIT_CONFIG_MENU_CONTROLLER:
            if (menu.current == 0 || menu.current == 1)
            {
                int const axis = menu.current == 0
                    ? G_FindConfiguredAnalogAxis(controllerProfile, analog_turning, CONTROLLER_AXIS_RIGHTX)
                    : G_FindConfiguredAnalogAxis(controllerProfile, analog_lookingupanddown, CONTROLLER_AXIS_RIGHTY);
                auto const sensitivity = G_GetConfiguredAnalogSensitivity(controllerProfile);
                Bsnprintf(buffer, bufferSize, "%s sens: %.1f", menu.current == 0 ? "Horizontal" : "Vertical", sensitivity[axis]);
            }
            else if (menu.current == 2)
            {
                int const axis = G_FindConfiguredAnalogAxis(controllerProfile, analog_lookingupanddown, CONTROLLER_AXIS_RIGHTY);
                auto const invert = G_GetConfiguredAnalogInvert(controllerProfile);
                Bsnprintf(buffer, bufferSize, "Inverted aiming: %s", s_splitMenuNoYesNames[invert[axis] != 0]);
            }
            else if (menu.current == 3)
                Bsnprintf(buffer, bufferSize, "View centering: %s", CONFIG_GetControllerViewCenteringName(G_GetConfiguredViewCentering(controllerProfile)));
            else if (menu.current == 4)
                Bsnprintf(buffer, bufferSize, "Reset to defaults");
            break;

        case SPLIT_CONFIG_MENU_DISCONNECT_CONFIRM:
            if (menu.current == 0)
                Bsnprintf(buffer, bufferSize, "Cancel");
            else
                Bsnprintf(buffer, bufferSize, "Disconnect player");
            break;

        default:
            buffer[0] = '\0';
            break;
    }
}

static void G_DrawSplitScreenConfigMenuDim(splitscreen_viewport_t const &viewport)
{
    if (viewport.width <= 0 || viewport.height <= 0)
        return;

#ifdef USE_OPENGL
    if (videoGetRenderMode() >= REND_POLYMOST)
    {
        renderDisableFog();
        polymost_useColorOnly(true);
        polymostSet2dView();

        buildgl_setDisabled(GL_ALPHA_TEST);
        buildgl_setDisabled(GL_DEPTH_TEST);
        buildgl_setEnabled(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4ub(0, 0, 0, 192);
        glRecti(viewport.x, viewport.y, viewport.x + viewport.width, viewport.y + viewport.height);
        glColor4ub(255, 255, 255, 255);

        polymost_useColorOnly(false);
        renderEnableFog();
        return;
    }
#endif

    for (int y = viewport.y; y < viewport.y + viewport.height; y += 4)
    {
        for (int offset = 0; offset < 3 && y + offset < viewport.y + viewport.height; ++offset)
            renderDrawLine(viewport.x << 12, (y + offset) << 12, (viewport.x + viewport.width - 1) << 12, (y + offset) << 12, blackcol);
    }
}

static void G_DrawSplitScreenConfigMenuText(int const x, int const y, int32_t const zoom, char const * const text, int const pal,
                                            int const shade, int const flags, int const x1, int const y1, int const x2, int const y2)
{
    int32_t const orientation = (g_textstat | ROTATESPRITE_FULL16 | RS_TOPLEFT) & ~(RS_AUTO | RS_NOCLIP);
    G_ScreenText(MF_Redfont.tilenum, x << 16, y << 16, zoom, 0, 0, text, shade, pal, orientation, 0,
                 MF_Redfont.emptychar.x, MF_Redfont.emptychar.y, MF_Redfont.between.x, MF_Redfont.between.y,
                 MF_Redfont.textflags | flags, x1, y1, x2, y2);
}

static void G_DrawSplitScreenConfigMenuForPlayer(int const viewIndex, int const playerNum)
{
    if ((unsigned)playerNum >= MAXPLAYERS || g_splitScreenConfigMenu[playerNum].page == SPLIT_CONFIG_MENU_CLOSED)
        return;

    splitscreen_viewport_t viewport {};
    if (G_GetSplitScreenViewport(viewIndex, &viewport) != 0 || viewport.width <= 0 || viewport.height <= 0)
        return;

    auto &menu = g_splitScreenConfigMenu[playerNum];
    int const entryCount = G_GetSplitScreenConfigMenuEntryCount(menu.page);
    if (entryCount <= 0)
        return;

    int32_t const viewportScale = min<int32_t>(divscale16(viewport.width, 320), divscale16(viewport.height, 200));
    int32_t const textScale = clamp<int32_t>(scale(viewportScale, 3, 2), 98304, 196608);
    int32_t const textZoom = mulscale16(MF_Redfont.zoom, textScale);
    int const lineHeight = max<int>(18, mulscale16(18 << 16, textScale) >> 16);
    int const titleHeight = max<int>(22, mulscale16(22 << 16, textScale) >> 16);
    int boxWidth = min<int>(viewport.width - 12, mulscale16(300 << 16, textScale) >> 16);

    if (boxWidth < 170)
        boxWidth = max<int>(80, viewport.width - 8);

    int const boxHeight = titleHeight + entryCount * lineHeight + 16;
    int const boxX = viewport.x + (viewport.width - boxWidth) / 2;
    int const boxY = max<int>(viewport.y + 6, viewport.y + (viewport.height - boxHeight) / 2);
    int const x1 = viewport.x;
    int const y1 = viewport.y;
    int const x2 = viewport.x + viewport.width - 1;
    int const y2 = viewport.y + viewport.height - 1;

    G_DrawSplitScreenConfigMenuDim(viewport);

    char title[32];
    Bsnprintf(title, sizeof(title), "PLAYER %d", playerNum + 1);
    G_DrawSplitScreenConfigMenuText(boxX + boxWidth / 2, boxY + 6, textZoom, title, MF_Redfont.pal_selected,
                                    -12, TEXT_XCENTER, x1, y1, x2, y2);

    for (int i = 0; i < entryCount; ++i)
    {
        menu.current = clamp(menu.current, 0, entryCount - 1);
        int const savedCurrent = menu.current;
        menu.current = i;

        char line[96];
        G_FormatSplitScreenConfigMenuLine(playerNum, G_GetControllerProfileForPlayer(playerNum), line, sizeof(line));
        menu.current = savedCurrent;

        bool const selected = i == menu.current;
        int const pal = selected ? MF_Redfont.pal_selected : MF_Redfont.pal_deselected;
        int const shade = selected ? -24 : MF_Redfont.shade_deselected;
        G_DrawSplitScreenConfigMenuText(boxX + 128, boxY + titleHeight + 6 + i * lineHeight, textZoom,
                                        line, pal, shade, 0, x1, y1, x2, y2);
    }
}

static int8_t G_GetWeaponRepeatDirection(int const playerNum, bool const nextHeld, bool const prevHeld)
{
    if ((unsigned) playerNum >= MAXPLAYERS)
        return 0;

    auto &repeatDir = g_splitScreenWeaponRepeatDir[playerNum];
    auto &repeatDelay = g_splitScreenWeaponRepeatDelay[playerNum];
    int8_t const requestedDir = nextHeld == prevHeld ? 0 : (nextHeld ? 1 : -1);

    if (requestedDir == 0)
    {
        repeatDir = 0;
        repeatDelay = 0;
        return 0;
    }

    if (repeatDir != requestedDir)
    {
        repeatDir = requestedDir;
        repeatDelay = WEAPON_REPEAT_INITIAL_DELAY;
        return requestedDir;
    }

    if (repeatDelay > 0)
    {
        --repeatDelay;
        return 0;
    }

    repeatDelay = WEAPON_REPEAT_INTERVAL;
    return requestedDir;
}

static int16_t G_ScaleAxisToVelocity(int16_t const value, int const maxValue)
{
    if (!value)
        return 0;

    return clamp(scale(value, maxValue, 32767), -maxValue, maxValue);
}

static fix16_t G_ScaleAxisToAngle(int16_t const value, float const maxRate)
{
    if (!value)
        return 0;

    return fix16_from_float((value / 32767.f) * maxRate);
}

static float G_GetSplitScreenAimFrameScale(int const playerNum)
{
    if ((unsigned)playerNum >= MAXPLAYERS)
        return 1.f;

    uint64_t const now = timerGetNanoTicks();
    uint64_t const tickRate = timerGetNanoTickRate();
    uint64_t elapsedTicks = g_splitScreenAimLastTicks[playerNum] ? now - g_splitScreenAimLastTicks[playerNum] : tickRate / (uint64_t)AIM_REFERENCE_FPS;
    g_splitScreenAimLastTicks[playerNum] = now;

    if (tickRate == 0)
        return 1.f;

    double const elapsedSeconds = (double)elapsedTicks / (double)tickRate;
    return (float)clamp<double>(elapsedSeconds * AIM_REFERENCE_FPS, 0.25, 3.0);
}

static void G_ApplyConfiguredAimWeight(int const controllerProfile, input_t &input)
{
    int const aimWeight = G_GetConfiguredAimWeight(controllerProfile);
    if (!aimWeight)
        return;

    int32_t const absyaw = klabs(input.q16avel);
    int32_t const abspitch = klabs(input.q16horz);
    int const aimWeightDivisor = max(1, 8 - aimWeight);

    if (absyaw > abspitch)
    {
        fix16_t const adjustment = (fix16_t)tabledivide32_noinline(absyaw - abspitch, aimWeightDivisor);
        if (input.q16horz > 0)
            input.q16horz = fix16_max(0, input.q16horz - adjustment);
        else if (input.q16horz < 0)
            input.q16horz = fix16_min(0, input.q16horz + adjustment);
    }
    else if (abspitch > absyaw)
    {
        fix16_t const adjustment = (fix16_t)tabledivide32_noinline(abspitch - absyaw, aimWeightDivisor);
        if (input.q16avel > 0)
            input.q16avel = fix16_max(0, input.q16avel - adjustment);
        else if (input.q16avel < 0)
            input.q16avel = fix16_min(0, input.q16avel + adjustment);
    }
}

static void G_ClearSplitScreenPadInput(int const playerNum)
{
    if ((unsigned)playerNum >= MAXPLAYERS)
        return;

    g_splitScreenLocalInputs[playerNum] = {};
    g_splitScreenPrevButtons[playerNum] = 0;
    memset(g_splitScreenPrevAxisActive[playerNum], 0, sizeof(g_splitScreenPrevAxisActive[playerNum]));
    memset(g_splitScreenRunTriggerBaselineSet[playerNum], 0, sizeof(g_splitScreenRunTriggerBaselineSet[playerNum]));
    g_splitScreenPadConnected[playerNum] = 0;
    g_splitScreenWeaponRepeatDir[playerNum] = 0;
    g_splitScreenWeaponRepeatDelay[playerNum] = 0;
    g_splitScreenWeaponPulseDir[playerNum] = 0;
    g_splitScreenWeaponPulseFrames[playerNum] = 0;
    g_splitScreenPausePulseFrames[playerNum] = 0;
    g_splitScreenAimLastTicks[playerNum] = 0;
    g_splitScreenJoinSuppressFrames[playerNum] = 0;
}

static void G_FreezeSplitScreenPadInput(int const playerNum, gamepadstate_t const &state)
{
    if ((unsigned)playerNum >= MAXPLAYERS)
        return;

    g_splitScreenLocalInputs[playerNum] = {};
    g_splitScreenPadConnected[playerNum] = 0;
    g_splitScreenWeaponRepeatDir[playerNum] = 0;
    g_splitScreenWeaponRepeatDelay[playerNum] = 0;
    g_splitScreenWeaponPulseDir[playerNum] = 0;
    g_splitScreenWeaponPulseFrames[playerNum] = 0;
    g_splitScreenPausePulseFrames[playerNum] = 0;
    g_splitScreenAimLastTicks[playerNum] = 0;

    if (state.connected)
        G_UpdatePreviousGamepadState(playerNum, state);
    else
    {
        g_splitScreenPrevButtons[playerNum] = 0;
        memset(g_splitScreenPrevAxisActive[playerNum], 0, sizeof(g_splitScreenPrevAxisActive[playerNum]));
    }
}

static void G_BuildSplitScreenPadInput(int const playerNum, int const controllerProfile, gamepadstate_t const &state)
{
    auto *const pPlayer = g_player[playerNum].ps;
    if (pPlayer == nullptr)
    {
        G_ClearSplitScreenPadInput(playerNum);
        return;
    }

    auto &input = g_splitScreenLocalInputs[playerNum];
    input       = {};
    g_splitScreenPadConnected[playerNum] = 0;

    if (!state.connected)
    {
        G_CloseSplitScreenConfigMenu(playerNum);
        G_ClearSplitScreenPadInput(playerNum);
        return;
    }

    g_splitScreenPadConnected[playerNum] = 1;

    if (g_splitScreenJoinSuppressFrames[playerNum] > 0)
    {
        --g_splitScreenJoinSuppressFrames[playerNum];
        G_FreezeSplitScreenPadInput(playerNum, state);
        return;
    }

    bool const pauseEdgePressed = G_GamepadButtonPressed(state.buttons, g_splitScreenPrevButtons[playerNum], GP_START);
    if (G_IsSecondarySplitScreenPlayer(playerNum))
    {
        if (pauseEdgePressed)
        {
            if (g_splitScreenConfigMenu[playerNum].page == SPLIT_CONFIG_MENU_CLOSED)
            {
                G_SetSplitScreenConfigMenuPage(playerNum, SPLIT_CONFIG_MENU_MAIN);
                G_PlaySplitScreenMenuSound(PISTOL_BODYHIT);
            }
            else
            {
                G_CloseSplitScreenConfigMenu(playerNum);
                G_PlaySplitScreenMenuSound(EXITMENUSOUND);
            }

            G_FreezeSplitScreenPadInput(playerNum, state);
            return;
        }

        if (g_splitScreenConfigMenu[playerNum].page != SPLIT_CONFIG_MENU_CLOSED)
        {
            G_UpdateSplitScreenConfigMenu(playerNum, controllerProfile, state);
            G_FreezeSplitScreenPadInput(playerNum, state);
            return;
        }
    }

    if (pauseEdgePressed)
        g_splitScreenPausePulseFrames[playerNum] = PAUSE_PULSE_FRAMES;

    bool const pausePressed = g_splitScreenPausePulseFrames[playerNum] > 0;

    if ((pPlayer->gm & (MODE_MENU | MODE_TYPE)) != 0 || (ud.pause_on && !pausePressed))
    {
        G_UpdatePreviousGamepadState(playerNum, state);
        g_splitScreenWeaponRepeatDir[playerNum] = 0;
        g_splitScreenWeaponRepeatDelay[playerNum] = 0;
        g_splitScreenWeaponPulseDir[playerNum] = 0;
        g_splitScreenWeaponPulseFrames[playerNum] = 0;
        g_splitScreenAimLastTicks[playerNum] = 0;
        if ((pPlayer->gm & (MODE_MENU | MODE_TYPE)) != 0)
            g_splitScreenPausePulseFrames[playerNum] = 0;
        return;
    }

    bool const fireHeld       = G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Fire);
    bool const altFireHeld    = G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Alt_Fire);
    bool const firePressed    = G_GamepadFunctionPressed(playerNum, controllerProfile, state, gamefunc_Fire);
    bool const altFirePress   = G_GamepadFunctionPressed(playerNum, controllerProfile, state, gamefunc_Alt_Fire);
    bool const nextWeaponHeld = G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Next_Weapon);
    bool const prevWeaponHeld = G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Previous_Weapon);
    int8_t const weaponCycleDirection = G_GetWeaponRepeatDirection(playerNum, nextWeaponHeld, prevWeaponHeld);
    if (weaponCycleDirection != 0)
    {
        g_splitScreenWeaponPulseDir[playerNum] = weaponCycleDirection;
        g_splitScreenWeaponPulseFrames[playerNum] = WEAPON_PULSE_FRAMES;
    }

    bool const nextWeapon = g_splitScreenWeaponPulseDir[playerNum] > 0 && g_splitScreenWeaponPulseFrames[playerNum] > 0;
    bool const prevWeapon = g_splitScreenWeaponPulseDir[playerNum] < 0 && g_splitScreenWeaponPulseFrames[playerNum] > 0;
    bool const dpadSelect = G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Dpad_Select);
    bool const dpadAiming = G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Dpad_Aiming);

    bool const alwaysRun = G_GetLocalPlayerAlwaysRunSetting(playerNum) != 0;
    bool const runHeld = G_GamepadRunHeld(playerNum, controllerProfile, state);
    bool const precisionAim = alwaysRun && runHeld;
    bool const playerRunning = alwaysRun ? (ud.runkey_mode ? true : !runHeld) : runHeld;
    int const moveLimit = playerRunning ? RUNKEYMOVE : WALKKEYMOVE;
    float const aimScaleX = precisionAim ? G_GetPrecisionAimScale(controllerProfile, analog_turning, GAMEPAD_AXIS_RIGHTX, PRECISION_AIM_SENS_X) : 1.f;
    float const aimScaleY = precisionAim ? G_GetPrecisionAimScale(controllerProfile, analog_lookingupanddown, GAMEPAD_AXIS_RIGHTY, PRECISION_AIM_SENS_Y) : 1.f;
    float const aimFrameScale = G_GetSplitScreenAimFrameScale(playerNum);

    input.fvel    = (int16_t)-G_ScaleAxisToVelocity(G_GetConfiguredAnalogValue(controllerProfile, state, analog_moving, GAMEPAD_AXIS_LEFTY), moveLimit);
    input.svel    = (int16_t)-G_ScaleAxisToVelocity(G_GetConfiguredAnalogValue(controllerProfile, state, analog_strafing, GAMEPAD_AXIS_LEFTX), moveLimit);
    input.q16avel = G_ScaleAxisToAngle(G_GetConfiguredAnalogValue(controllerProfile, state, analog_turning, GAMEPAD_AXIS_RIGHTX), TURN_RATE * aimScaleX * aimFrameScale);
    input.q16horz = -G_ScaleAxisToAngle(G_GetConfiguredAnalogValue(controllerProfile, state, analog_lookingupanddown, GAMEPAD_AXIS_RIGHTY), LOOK_RATE * aimScaleY * aimFrameScale);
    G_ApplyConfiguredAimWeight(controllerProfile, input);

    input.bits |= (1u << SK_AIMMODE);
    input.bits |= ((uint32_t)playerRunning << SK_RUN);
    input.bits |= ((uint32_t)G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Jump) << SK_JUMP);
    input.bits |= ((uint32_t)(G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Crouch)
                           || G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Toggle_Crouch)) << SK_CROUCH);
    input.bits |= ((uint32_t)G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Open) << SK_OPEN);
    input.bits |= ((uint32_t)G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Quick_Kick) << SK_QUICK_KICK);
    input.bits |= ((uint32_t)G_GamepadFunctionPressed(playerNum, controllerProfile, state, gamefunc_Center_View) << SK_CENTER_VIEW);
    input.bits |= ((uint32_t)pausePressed << SK_PAUSE);
    input.bits |= ((uint32_t)G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Inventory) << SK_INVENTORY);
    input.bits |= ((uint32_t)G_GamepadFunctionHeld(controllerProfile, state, gamefunc_MedKit) << SK_MEDKIT);
    input.bits |= ((uint32_t)G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Holster_Weapon) << SK_HOLSTER);
    input.bits |= ((uint32_t)G_GamepadFunctionHeld(controllerProfile, state, gamefunc_NightVision) << SK_NIGHTVISION);
    input.bits |= ((uint32_t)G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Steroids) << SK_STEROIDS);
    input.bits |= ((uint32_t)G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Holo_Duke) << SK_HOLODUKE);
    input.bits |= ((uint32_t)G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Jetpack) << SK_JETPACK);
    input.bits |= ((uint32_t)G_GamepadFunctionHeld(controllerProfile, state, gamefunc_TurnAround) << SK_TURNAROUND);
    input.bits |= ((uint32_t)(G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Inventory_Left)
                           || (dpadSelect && (input.svel > 0 || input.q16avel < 0))) << SK_INV_LEFT);
    input.bits |= ((uint32_t)(G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Inventory_Right)
                           || (dpadSelect && (input.svel < 0 || input.q16avel > 0))) << SK_INV_RIGHT);
    input.bits |= ((uint32_t)(G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Aim_Up)
                           || (dpadAiming && input.fvel > 0)) << SK_AIM_UP);
    input.bits |= ((uint32_t)(G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Aim_Down)
                           || (dpadAiming && input.fvel < 0)) << SK_AIM_DOWN);
    input.bits |= ((uint32_t)G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Look_Left) << SK_LOOK_LEFT);
    input.bits |= ((uint32_t)G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Look_Right) << SK_LOOK_RIGHT);
    input.bits |= ((uint32_t)G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Look_Up) << SK_LOOK_UP);
    input.bits |= ((uint32_t)G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Look_Down) << SK_LOOK_DOWN);
    input.bits |= ((uint32_t)g_gameQuit << SK_GAMEQUIT);

    int weaponSelection = 0;
    for (int gameFunction = gamefunc_Weapon_10; gameFunction >= gamefunc_Weapon_1; --gameFunction)
    {
        if (G_GamepadFunctionHeld(controllerProfile, state, gameFunction))
        {
            weaponSelection = gameFunction - (gamefunc_Weapon_1 - 1);
            break;
        }
    }

    if (G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Last_Weapon))
        weaponSelection = 14;
    else if (G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Alt_Weapon))
        weaponSelection = 13;
    else if (nextWeapon)
        weaponSelection = 12;
    else if (prevWeapon)
        weaponSelection = 11;

    if (weaponSelection)
        input.bits |= ((uint32_t)weaponSelection << SK_WEAPON_BITS);
    else if (nextWeapon)
        input.bits |= (12u << SK_WEAPON_BITS);
    else if (prevWeapon)
        input.bits |= (11u << SK_WEAPON_BITS);

    if (dpadSelect)
    {
        input.fvel = 0;
        input.svel = 0;
        input.q16avel = 0;
    }
    else if (dpadAiming)
        input.fvel = 0;

    if (g_splitScreenWeaponPulseFrames[playerNum] > 0)
    {
        if (--g_splitScreenWeaponPulseFrames[playerNum] == 0)
            g_splitScreenWeaponPulseDir[playerNum] = 0;
    }

    if (g_splitScreenPausePulseFrames[playerNum] > 0)
        --g_splitScreenPausePulseFrames[playerNum];

    bool const semiautoWeapon = (PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_SEMIAUTO) != 0;
    bool const fireActive     = semiautoWeapon ? firePressed : fireHeld;
    bool const altFireActive  = semiautoWeapon ? altFirePress : altFireHeld;

    input.bits |= ((uint32_t)fireActive << SK_FIRE);
    input.extbits |= ((uint32_t)altFireActive << EK_ALT_FIRE);

    input.extbits |= ((uint32_t)(G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Move_Forward) || input.fvel > 0) << EK_MOVE_FORWARD);
    input.extbits |= ((uint32_t)(G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Move_Backward) || input.fvel < 0) << EK_MOVE_BACKWARD);
    input.extbits |= ((uint32_t)(G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Strafe_Left) || input.svel > 0) << EK_STRAFE_LEFT);
    input.extbits |= ((uint32_t)(G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Strafe_Right) || input.svel < 0) << EK_STRAFE_RIGHT);
    input.extbits |= ((uint32_t)(G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Turn_Left) || input.q16avel < 0) << EK_TURN_LEFT);
    input.extbits |= ((uint32_t)(G_GamepadFunctionHeld(controllerProfile, state, gamefunc_Turn_Right) || input.q16avel > 0) << EK_TURN_RIGHT);
    input.extbits |= ((uint32_t)(G_GetConfiguredViewCentering(controllerProfile) != 0 && !precisionAim) << EK_GAMEPAD_CENTERING);
    input.extbits |= ((uint32_t)(G_GetConfiguredAimAssist(controllerProfile) != 0) << EK_GAMEPAD_AIM_ASSIST);
    input.extbits |= ((uint32_t)precisionAim << EK_GAMEPAD_PRECISION_AIM);

    if (!ud.recstat)
        P_UpdateAngles(playerNum, input);

    G_UpdatePreviousGamepadState(playerNum, state);
}

static bool G_CanJoinCurrentGame(void)
{
    if (g_netServer || g_netClient || G_GetSplitScreenPlayerCount() >= MAX_LOCAL_PLAYERS)
        return false;

    auto const primaryPlayer = g_player[myconnectindex].ps;
    if (primaryPlayer == nullptr || (primaryPlayer->gm & MODE_GAME) == 0 || (primaryPlayer->gm & (MODE_MENU | MODE_TYPE)) != 0 || ud.pause_on)
        return false;

    return ud.m_coop == 1 || !G_HaveSplitScreen();
}

static void G_UpdateSplitScreenJoinInputs(void)
{
    bool const canJoin = G_CanJoinCurrentGame();
    int const connectedGamepads = min<int>(joyGetConnectedGamepadCount(), MAX_LOCAL_PLAYERS);

    for (int gamepadIndex = 0; gamepadIndex < MAX_LOCAL_PLAYERS; ++gamepadIndex)
    {
        gamepadstate_t state {};
        uint32_t const buttons = gamepadIndex < connectedGamepads && joyGetGamepadState(gamepadIndex, &state) == 0 && state.connected
            ? state.buttons
            : 0;

        int const playerNum = G_GetPlayerForGamepadIndex(gamepadIndex);
        bool const startPressed = G_GamepadButtonPressed(buttons, g_splitScreenJoinPrevButtons[gamepadIndex], GP_START);

        if (canJoin && startPressed && (unsigned)playerNum < MAX_LOCAL_PLAYERS && !G_IsSplitScreenPlayerActive(playerNum))
        {
            if (G_AddSplitScreenPlayer(playerNum) >= 0)
            {
                G_CloseSplitScreenConfigMenu(playerNum);
                G_FreezeSplitScreenPadInput(playerNum, state);
                g_splitScreenJoinSuppressFrames[playerNum] = 2;
                G_PlaySplitScreenMenuSound(PISTOL_BODYHIT);
            }
        }

        g_splitScreenJoinPrevButtons[gamepadIndex] = buttons;
    }
}
}

void G_ClearSplitScreenContinueInput(void)
{
    G_ClearSplitScreenContinueInputLocal();
}

int32_t G_CheckSplitScreenContinueInput(void)
{
    return G_CheckSplitScreenContinueInputLocal();
}

void G_DrawSplitScreenConfigMenus(void)
{
    if (!G_HaveSplitScreen())
        return;

    auto const primaryPlayer = g_player[myconnectindex].ps;
    if (primaryPlayer != nullptr && (primaryPlayer->gm & (MODE_MENU | MODE_TYPE)) != 0)
        return;

    int const playerCount = min<int32_t>(G_GetSplitScreenPlayerCount(), MAX_LOCAL_PLAYERS);
    for (int viewIndex = 0; viewIndex < playerCount; ++viewIndex)
    {
        int const playerNum = G_GetSplitScreenPlayer(viewIndex);
        if (!G_IsSecondarySplitScreenPlayer(playerNum))
            continue;

        G_DrawSplitScreenConfigMenuForPlayer(viewIndex, playerNum);
    }

    videoSetViewableArea(0, 0, xdim - 1, ydim - 1);
}

void G_UpdateSplitScreenLocalInputs(void)
{
    M_UpdateDeveloperModeToggle();
    G_UpdateSplitScreenJoinInputs();

    if (!G_HasNativeSplitScreenGamepads())
    {
        G_CloseAllSplitScreenConfigMenus();

        for (int playerNum = 0; playerNum < MAX_LOCAL_PLAYERS; ++playerNum)
            G_ClearSplitScreenPadInput(playerNum);

        return;
    }

    for (int playerNum = MAX_LOCAL_PLAYERS; playerNum < MAXPLAYERS; ++playerNum)
        g_splitScreenLocalInputs[playerNum] = {};

    int const playerCount = min<int32_t>(G_GetSplitScreenPlayerCount(), MAX_LOCAL_PLAYERS);
    int const firstGamepadViewIndex = G_GetFirstGamepadViewIndex();
    if (firstGamepadViewIndex > 0)
        G_ClearSplitScreenPadInput(myconnectindex);

    auto const primaryPlayer = g_player[myconnectindex].ps;
    if (primaryPlayer != nullptr && (primaryPlayer->gm & (MODE_MENU | MODE_TYPE)) != 0)
    {
        G_CloseAllSplitScreenConfigMenus();

        for (int viewIndex = firstGamepadViewIndex; viewIndex < playerCount; ++viewIndex)
        {
            int const playerNum = G_GetSplitScreenPlayer(viewIndex);
            if ((unsigned)playerNum >= MAXPLAYERS)
                continue;

            gamepadstate_t state {};
            if (joyGetGamepadState(G_GetGamepadIndexForPlayer(playerNum), &state) == 0)
                G_FreezeSplitScreenPadInput(playerNum, state);
            else
                G_ClearSplitScreenPadInput(playerNum);
        }

        for (int playerNum = 0; playerNum < MAX_LOCAL_PLAYERS; ++playerNum)
            if (!G_IsSplitScreenPlayerActive(playerNum))
                G_ClearSplitScreenPadInput(playerNum);

        return;
    }

    for (int viewIndex = firstGamepadViewIndex; viewIndex < playerCount; ++viewIndex)
    {
        int const playerNum = G_GetSplitScreenPlayer(viewIndex);
        if ((unsigned)playerNum >= MAXPLAYERS)
            continue;

        int const gamepadIndex = G_GetGamepadIndexForPlayer(playerNum);
        gamepadstate_t state {};
        if (joyGetGamepadState(gamepadIndex, &state) == 0)
            G_BuildSplitScreenPadInput(playerNum, G_GetControllerProfileForPlayer(playerNum), state);
        else
            G_ClearSplitScreenPadInput(playerNum);
    }

    for (int playerNum = 0; playerNum < MAX_LOCAL_PLAYERS; ++playerNum)
    {
        if (!G_IsSplitScreenPlayerActive(playerNum))
        {
            G_CloseSplitScreenConfigMenu(playerNum);
            G_ClearSplitScreenPadInput(playerNum);
        }
    }
}

void G_FillSplitScreenInputsForTic(void)
{
    if (!G_HasNativeSplitScreenGamepads())
        return;

    int const playerCount = min<int32_t>(G_GetSplitScreenPlayerCount(), MAX_LOCAL_PLAYERS);

    for (int viewIndex = 1; viewIndex < playerCount; ++viewIndex)
    {
        int const playerNum = G_GetSplitScreenPlayer(viewIndex);
        if ((unsigned)playerNum >= MAXPLAYERS || g_player[playerNum].ps == nullptr)
            continue;

        auto const &localPlayerInput = g_splitScreenLocalInputs[playerNum];
        auto &      input            = inputfifo[0][playerNum];
        auto const *pPlayer          = g_player[playerNum].ps;
        auto const q16ang            = fix16_to_int(pPlayer->q16ang);

        input = localPlayerInput;
        input.fvel = mulscale9(localPlayerInput.fvel, sintable[(q16ang + 2560) & 2047]) +
            mulscale9(localPlayerInput.svel, sintable[(q16ang + 2048) & 2047]);
        input.svel = mulscale9(localPlayerInput.fvel, sintable[(q16ang + 2048) & 2047]) +
            mulscale9(localPlayerInput.svel, sintable[(q16ang + 1536) & 2047]);

        if (!FURY)
        {
            input.fvel += pPlayer->fric.x;
            input.svel += pPlayer->fric.y;
        }
    }
}

int32_t G_CopySplitScreenLocalInput(int32_t const playerNum, input_t * const input)
{
    if (input == nullptr || !G_HasNativeSplitScreenGamepads() || (unsigned)playerNum >= MAXPLAYERS || !g_splitScreenPadConnected[playerNum])
        return -1;

    *input = g_splitScreenLocalInputs[playerNum];
    return 0;
}
