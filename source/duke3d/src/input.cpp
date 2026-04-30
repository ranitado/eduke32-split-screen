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

#include "global.h"
#include "baselayer.h"
#include "game.h"
#include "function.h"
#include "keyboard.h"
#include "mouse.h"
#include "joystick.h"
#include "control.h"
#include "input.h"
#include "menus.h"
#include "splitscreen.h"

static uint32_t g_menuGamepadClearButtons = 0;
static uint32_t g_menuGamepadLastButtons = 0;
static direction g_menuGamepadClearDirection = dir_None;
static int32_t g_menuGamepadClearDirectionSource = -1;
static direction g_menuGamepadRepeat = dir_None;
static int32_t g_menuGamepadRepeatSource = -1;
static int32_t g_menuGamepadRepeatClock = -1;
static uint32_t g_menuKeyboardMouseClearActions = 0;
static UserInput g_menuUserInput;
static int32_t g_menuUserInputDirectionSource = -1;
static int32_t g_menuUserInputClock = -1;
static int32_t g_menuLastAdvanceGamepadIndex = -1;
static int32_t g_menuAllGamepadsUnlockClock = 0;
static int32_t g_menuInputLockPlayer = 0;
static int32_t g_menuReturnSuppressUntilClock = 0;

enum MenuKeyboardMouseAction_t
{
    MENU_KM_RETURN = 1u << 0,
    MENU_KM_ESCAPE = 1u << 1,
};

static bool I_IsGameMenuInputRestricted(void)
{
    auto const player = g_player[myconnectindex].ps;
    return player != nullptr && (player->gm & MODE_GAME) != 0;
}

static bool I_IsGameMenuOpen(void)
{
    auto const player = g_player[myconnectindex].ps;
    return player != nullptr && (player->gm & (MODE_GAME | MODE_MENU)) == (MODE_GAME | MODE_MENU);
}

static bool I_MenuAllGamepadsAllowed(void)
{
    return I_IsGameMenuOpen() && (int32_t)timerGetTicks() >= g_menuAllGamepadsUnlockClock;
}

static bool I_MenuInputLockActive(void)
{
    return I_IsGameMenuOpen() && !I_MenuAllGamepadsAllowed();
}

static bool I_KeyboardMouseAllowedForMenu(void)
{
    return true;
}

void I_LockMenuInputToPlayer(int32_t const playerNum, int32_t const milliseconds)
{
    g_menuInputLockPlayer = clamp<int32_t>(playerNum, 0, MAXSPLITSCREENCONTROLLERS - 1);
    g_menuAllGamepadsUnlockClock = (int32_t)timerGetTicks() + max<int32_t>(milliseconds, 0);

    int32_t const lockedGamepadIndex = G_GetSplitScreenInputGamepadIndex(G_GetSplitScreenPlayerInput(g_menuInputLockPlayer));
    if ((unsigned)lockedGamepadIndex < MAXSPLITSCREENCONTROLLERS)
        joySetPrimaryGamepadIndex(lockedGamepadIndex);
}

void I_LockNonPrimaryMenuGamepads(int32_t const milliseconds)
{
    I_LockMenuInputToPlayer(myconnectindex, milliseconds);
}

static bool I_IsGamepadAssignedToNonPrimaryPlayer(int const gamepadIndex)
{
    for (int playerNum = 0; playerNum < MAXSPLITSCREENCONTROLLERS; ++playerNum)
    {
        if (playerNum == myconnectindex)
            continue;

        int32_t const assignedGamepadIndex = G_GetSplitScreenInputGamepadIndex(G_GetSplitScreenPlayerInput(playerNum));
        if (assignedGamepadIndex == gamepadIndex)
            return true;
    }

    return false;
}

static bool I_IsMenuGamepadAllowed(int const gamepadIndex)
{
    if (!I_IsGameMenuInputRestricted())
        return true;

    if (I_MenuAllGamepadsAllowed())
        return true;

    if (I_IsGameMenuOpen() && I_MenuInputLockActive())
    {
        int32_t const lockedGamepadIndex = G_GetSplitScreenInputGamepadIndex(G_GetSplitScreenPlayerInput(g_menuInputLockPlayer));
        return lockedGamepadIndex >= 0 && lockedGamepadIndex == gamepadIndex;
    }

    int32_t const assignedGamepadIndex = G_GetSplitScreenInputGamepadIndex(G_GetSplitScreenPlayerInput(myconnectindex));
    if (assignedGamepadIndex >= 0)
        return assignedGamepadIndex == gamepadIndex;

    int32_t const primaryGamepadIndex = joyGetPrimaryGamepadIndex();
    return numplayers == 1 && myconnectindex == 0 && gamepadIndex == primaryGamepadIndex && !I_IsGamepadAssignedToNonPrimaryPlayer(gamepadIndex);
}

static bool I_ShouldSuppressControlJoystickInputForMenu(void)
{
    return I_IsGameMenuInputRestricted() && !I_IsGameMenuOpen();
}

static direction I_GetMenuGamepadDirection(gamepadstate_t const &state)
{
    int constexpr axisThreshold = 8000;

    if ((state.buttons & (1u << CONTROLLER_BUTTON_DPAD_UP)) || state.axes[GAMEPAD_AXIS_LEFTY] <= -axisThreshold)
        return dir_Up;
    if ((state.buttons & (1u << CONTROLLER_BUTTON_DPAD_DOWN)) || state.axes[GAMEPAD_AXIS_LEFTY] >= axisThreshold)
        return dir_Down;
    if ((state.buttons & (1u << CONTROLLER_BUTTON_DPAD_LEFT)) || state.axes[GAMEPAD_AXIS_LEFTX] <= -axisThreshold)
        return dir_Left;
    if ((state.buttons & (1u << CONTROLLER_BUTTON_DPAD_RIGHT)) || state.axes[GAMEPAD_AXIS_LEFTX] >= axisThreshold)
        return dir_Right;

    return dir_None;
}

static bool I_GamepadHasMenuActivity(gamepadstate_t const &state)
{
    return state.buttons != 0 || I_GetMenuGamepadDirection(state) != dir_None;
}

static void I_UpdateMenuPrimaryGamepadFromActivity(void)
{
    if (!I_IsGameMenuOpen() || !I_MenuAllGamepadsAllowed())
        return;

    int32_t const currentPrimaryGamepadIndex = joyGetPrimaryGamepadIndex();
    int32_t activeGamepadIndex = -1;

    for (int gamepadIndex = 0; gamepadIndex < MAXSPLITSCREENCONTROLLERS; ++gamepadIndex)
    {
        if (!I_IsMenuGamepadAllowed(gamepadIndex))
            continue;

        gamepadstate_t state {};
        if (joyGetGamepadState(gamepadIndex, &state) != 0 || !state.connected || !I_GamepadHasMenuActivity(state))
            continue;

        if (gamepadIndex == currentPrimaryGamepadIndex)
            return;

        if (activeGamepadIndex < 0)
            activeGamepadIndex = gamepadIndex;
    }

    if (activeGamepadIndex >= 0)
        joySetPrimaryGamepadIndex(activeGamepadIndex);
}

static uint32_t I_GetAllowedMenuGamepadButtons(direction * const heldDirection = nullptr, int32_t * const advanceGamepadIndex = nullptr,
                                              int32_t * const heldDirectionGamepadIndex = nullptr)
{
    if (heldDirection != nullptr)
        *heldDirection = dir_None;
    if (advanceGamepadIndex != nullptr)
        *advanceGamepadIndex = -1;
    if (heldDirectionGamepadIndex != nullptr)
        *heldDirectionGamepadIndex = -1;

    uint32_t buttons = 0;
    for (int gamepadIndex = 0; gamepadIndex < MAXSPLITSCREENCONTROLLERS; ++gamepadIndex)
    {
        bool const allowed = I_IsMenuGamepadAllowed(gamepadIndex);
        gamepadstate_t state {};
        if (joyGetGamepadState(gamepadIndex, &state) != 0 || !state.connected)
            continue;

        direction const rawDirection = I_GetMenuGamepadDirection(state);

        if (!allowed)
            continue;

        buttons |= state.buttons;
        if (advanceGamepadIndex != nullptr && *advanceGamepadIndex < 0 && (state.buttons & (1u << CONTROLLER_BUTTON_A)))
            *advanceGamepadIndex = gamepadIndex;

        if (rawDirection == dir_None || heldDirection == nullptr || *heldDirection != dir_None)
            continue;

        *heldDirection = rawDirection;
        if (heldDirectionGamepadIndex != nullptr)
            *heldDirectionGamepadIndex = gamepadIndex;
    }

    return buttons;
}

static direction I_GetPrimaryControllerMenuDirectionFallback(void)
{
    int32_t const primaryGamepadIndex = joyGetPrimaryGamepadIndex();
    if (primaryGamepadIndex < 0 || !I_IsMenuGamepadAllowed(primaryGamepadIndex))
        return dir_None;

    bool const oldSuppressJoystickInput = CONTROL_SuppressJoystickInput;
    UserInput fallback {};

    CONTROL_SuppressJoystickInput = false;
    CONTROL_GetUserInput(&fallback);
    CONTROL_SuppressJoystickInput = oldSuppressJoystickInput;

    return fallback.dir;
}

int32_t I_MenuGamepadActivity(void)
{
    I_UpdateMenuPrimaryGamepadFromActivity();

    for (int gamepadIndex = 0; gamepadIndex < MAXSPLITSCREENCONTROLLERS; ++gamepadIndex)
    {
        if (!I_IsMenuGamepadAllowed(gamepadIndex))
            continue;

        gamepadstate_t state {};
        if (joyGetGamepadState(gamepadIndex, &state) != 0 || !state.connected)
            continue;

        if (I_GamepadHasMenuActivity(state))
            return 1;
    }

    return 0;
}

static direction I_FilterMenuGamepadDirection(direction const heldDirection, int32_t const directionGamepadIndex)
{
    if (heldDirection == dir_None)
    {
        g_menuGamepadClearDirection = dir_None;
        g_menuGamepadClearDirectionSource = -1;
        g_menuGamepadRepeat = dir_None;
        g_menuGamepadRepeatSource = -1;
        g_menuGamepadRepeatClock = -1;
        return dir_None;
    }

    if (g_menuGamepadClearDirection != dir_None)
    {
        if (heldDirection == g_menuGamepadClearDirection && directionGamepadIndex == g_menuGamepadClearDirectionSource)
        {
            g_menuGamepadRepeat = heldDirection;
            g_menuGamepadRepeatSource = directionGamepadIndex;
            g_menuGamepadRepeatClock = (int32_t)timerGetTicks() + 500;
            return dir_None;
        }

        g_menuGamepadClearDirection = dir_None;
        g_menuGamepadClearDirectionSource = -1;
    }

    int32_t const now = (int32_t)timerGetTicks();
    if (heldDirection != g_menuGamepadRepeat || directionGamepadIndex != g_menuGamepadRepeatSource)
    {
        g_menuGamepadRepeat = heldDirection;
        g_menuGamepadRepeatSource = directionGamepadIndex;
        g_menuGamepadRepeatClock = now + 500;
        return heldDirection;
    }

    if (g_menuGamepadRepeatClock < 0 || now < g_menuGamepadRepeatClock)
        return dir_None;

    g_menuGamepadRepeatClock = now + 60;
    return heldDirection;
}

static bool I_MenuGamepadButtonActive(uint32_t const buttons, int const button)
{
    uint32_t const mask = 1u << button;

    if ((buttons & mask) == 0)
    {
        g_menuGamepadClearButtons &= ~mask;
        return false;
    }

    return (g_menuGamepadClearButtons & mask) == 0;
}

static bool I_ApplyMenuGamepadButtonFilter(bool &trigger, uint32_t const buttons, int const button)
{
    uint32_t const mask = 1u << button;
    bool const held = (buttons & mask) != 0;
    bool const active = I_MenuGamepadButtonActive(buttons, button);

    if (held && !active)
    {
        trigger = false;
        return false;
    }
    else
        trigger = trigger || active;

    return active;
}

static bool I_MenuKeyboardMouseActionActive(uint32_t const action, bool const held)
{
    if (!held)
    {
        g_menuKeyboardMouseClearActions &= ~action;
        return false;
    }

    return (g_menuKeyboardMouseClearActions & action) == 0;
}

static void I_ClearMenuGamepadInput(void)
{
    g_menuGamepadClearButtons |= g_menuGamepadLastButtons;
    g_menuGamepadLastButtons = 0;

    if (g_menuUserInput.dir != dir_None)
    {
        g_menuGamepadClearDirection = g_menuUserInput.dir;
        g_menuGamepadClearDirectionSource = g_menuUserInputDirectionSource;
        g_menuGamepadRepeat = g_menuUserInput.dir;
        g_menuGamepadRepeatSource = g_menuUserInputDirectionSource;
        g_menuGamepadRepeatClock = (int32_t)timerGetTicks() + 500;
    }
}

static void I_ClearMenuKeyboardMouseInput(void)
{
    if (KB_KeyPressed(sc_Escape))
        g_menuKeyboardMouseClearActions |= MENU_KM_ESCAPE;

    if (MOUSE_GetButtons() & M_RIGHTBUTTON)
        g_menuKeyboardMouseClearActions |= MENU_KM_RETURN;

    if (g_menuUserInput.dir != dir_None)
    {
        KB_ClearKeyDown(sc_UpArrow);
        KB_ClearKeyDown(sc_kpad_8);
        KB_ClearKeyDown(sc_DownArrow);
        KB_ClearKeyDown(sc_kpad_2);
        KB_ClearKeyDown(sc_LeftArrow);
        KB_ClearKeyDown(sc_kpad_4);
        KB_ClearKeyDown(sc_RightArrow);
        KB_ClearKeyDown(sc_kpad_6);
    }

    if (g_menuUserInput.b_advance)
    {
        KB_ClearKeyDown(sc_kpad_Enter);
        KB_ClearKeyDown(sc_Enter);
        MOUSE_ClearButton(M_LEFTBUTTON);
    }

    // Return/Escape are one-shot menu actions. Do not clear their physical state:
    // OS key repeat would otherwise recreate a fresh Escape press while held.
}

static UserInput *I_GetUserInput(void)
{
    int32_t const inputClock = (int32_t)g_frameCounter;
    if (g_menuUserInputClock == inputClock)
        return &g_menuUserInput;

    bool const oldSuppressJoystickInput = CONTROL_SuppressJoystickInput;
    bool const restrictGamepadInput = I_IsGameMenuInputRestricted();
    bool const suppressControlJoystickInput = I_ShouldSuppressControlJoystickInputForMenu();
    UserInput info {};

    I_UpdateMenuPrimaryGamepadFromActivity();

    CONTROL_SuppressJoystickInput = suppressControlJoystickInput;
    info = *CONTROL_GetUserInput(nullptr);
    CONTROL_SuppressJoystickInput = oldSuppressJoystickInput;

    bool const allowKeyboardMouse = I_KeyboardMouseAllowedForMenu();
    if (!allowKeyboardMouse)
    {
        info.b_advance = false;
        info.b_return = false;
        info.b_escape = false;
    }

    bool const keyboardEscapeHeld = allowKeyboardMouse && KB_KeyPressed(sc_Escape) != 0;
    bool const keyboardEscapeActive = I_MenuKeyboardMouseActionActive(MENU_KM_ESCAPE, keyboardEscapeHeld);
    bool const mouseReturnHeld = allowKeyboardMouse && (MOUSE_GetButtons() & M_RIGHTBUTTON) != 0;
    bool const mouseReturnActive = I_MenuKeyboardMouseActionActive(MENU_KM_RETURN, mouseReturnHeld);

    if (keyboardEscapeHeld && !keyboardEscapeActive)
    {
        if (!mouseReturnActive)
            info.b_return = false;

        info.b_escape = false;
    }

    if (mouseReturnHeld && !mouseReturnActive && !keyboardEscapeActive)
        info.b_return = false;

    direction heldDirection = dir_None;
    int32_t heldDirectionGamepadIndex = -1;
    int32_t advanceGamepadIndex = -1;
    uint32_t const buttons = I_GetAllowedMenuGamepadButtons(&heldDirection, &advanceGamepadIndex, &heldDirectionGamepadIndex);
    if (heldDirection == dir_None && restrictGamepadInput)
    {
        heldDirection = I_GetPrimaryControllerMenuDirectionFallback();
        if (heldDirection != dir_None)
            heldDirectionGamepadIndex = joyGetPrimaryGamepadIndex();
    }

    direction const direction = I_FilterMenuGamepadDirection(heldDirection, heldDirectionGamepadIndex);

    g_menuGamepadLastButtons = buttons;
    g_menuUserInputDirectionSource = -1;

    if (direction != dir_None)
    {
        info.dir = direction;
        g_menuUserInputDirectionSource = heldDirectionGamepadIndex;
    }

    bool const gamepadAdvanceActive = I_ApplyMenuGamepadButtonFilter(info.b_advance, buttons, CONTROLLER_BUTTON_A);
    I_ApplyMenuGamepadButtonFilter(info.b_return, buttons, CONTROLLER_BUTTON_B);
    I_ApplyMenuGamepadButtonFilter(info.b_escape, buttons, CONTROLLER_BUTTON_START);

    if ((int32_t)timerGetTicks() < g_menuReturnSuppressUntilClock)
        info.b_return = false;

    if (info.b_advance)
        g_menuLastAdvanceGamepadIndex = gamepadAdvanceActive ? advanceGamepadIndex : -1;

    g_menuUserInput = info;
    g_menuUserInputClock = inputClock;

    return &g_menuUserInput;
}

int32_t I_CheckAllInput(void)
{
    return KB_KeyWaiting() || MOUSE_GetButtons() || I_GetAllowedMenuGamepadButtons()
#if defined EDUKE32_IOS
        || g_mouseClickState == MOUSE_PRESSED
#endif
        ;
}

void I_BeginMenuInputFrame(void)
{
    g_menuUserInputClock = -1;
}

void I_ClearAllInput(void)
{
    direction heldDirection = dir_None;
    int32_t heldDirectionGamepadIndex = -1;

    KB_FlushKeyboardQueue();
    KB_ClearKeysDown();
    MOUSE_ClearAllButtons();
    JOYSTICK_ClearAllButtons();
    CONTROL_ClearAllButtons();
    g_menuGamepadClearButtons = I_GetAllowedMenuGamepadButtons(&heldDirection, nullptr, &heldDirectionGamepadIndex);
    g_menuGamepadLastButtons = 0;
    g_menuGamepadClearDirection = heldDirection;
    g_menuGamepadClearDirectionSource = heldDirectionGamepadIndex;
    g_menuGamepadRepeat = heldDirection;
    g_menuGamepadRepeatSource = g_menuGamepadClearDirectionSource;
    g_menuGamepadRepeatClock = heldDirection == dir_None ? -1 : (int32_t)timerGetTicks() + 500;
    g_menuKeyboardMouseClearActions = 0;
    g_menuUserInput = {};
    g_menuUserInputDirectionSource = -1;
    g_menuUserInputClock = -1;
    g_menuLastAdvanceGamepadIndex = -1;
    mouseAdvanceClickState();
}

void I_ClearLast(void)
{
    bool const consumedReturn = g_menuUserInput.b_return != 0;
    UserInput clearInfo {};
    clearInfo.dir = g_menuUserInput.dir;
    clearInfo.b_advance = g_menuUserInput.b_advance;
    CONTROL_ClearUserInput(&clearInfo);
    I_ClearMenuKeyboardMouseInput();
    I_ClearMenuGamepadInput();
    if (consumedReturn)
        g_menuReturnSuppressUntilClock = (int32_t)timerGetTicks() + 180;
    g_menuUserInput = {};
    g_menuUserInputClock = -1;
}

int32_t I_TextSubmit(void)
{
    // FIXME: this is needed because the menu code does some weird shit.
    // for some reason, if the mouse cursor is hidden, activating menu items is handled immediately on button press,
    // but if the mouse cursor is visible, activating menu items is handled on button release instead...
    // ...except for the right mouse button, which is handled immediately on button press in both cases.
    if (!MOUSEINACTIVECONDITIONAL(1) || !MOUSEACTIVECONDITIONAL(1))
    {
        MOUSE_ClearButton(M_LEFTBUTTON);
        g_menuUserInputClock = -1;
    }

    return I_GetUserInput()->b_advance;
}
int32_t I_ReturnTrigger(void) { return I_GetUserInput()->b_return; }
int32_t I_AdvanceTrigger(void)
{
    if (I_TextSubmit())
        return 1;

    if (KB_KeyPressed(sc_Space))
    {
        g_menuLastAdvanceGamepadIndex = -1;
        return 1;
    }

    return 0;
}

int32_t I_GetMenuAdvanceGamepadIndex(void) { return g_menuLastAdvanceGamepadIndex; }

void I_TextSubmitClear(void) { I_ClearAllInput(); }

void I_AdvanceTriggerClear(void)
{
    I_ClearLast();
    KB_ClearKeyDown(sc_Space);
}

int32_t I_GeneralTrigger(void)
{
    I_BeginMenuInputFrame();

#if !defined GEKKO && !defined EDUKE32_TOUCH_DEVICES
    int32_t const mouseAdvance = MOUSE_GetButtons() & M_LEFTBUTTON;
#endif

    return
#if !defined GEKKO && !defined EDUKE32_TOUCH_DEVICES
        mouseAdvance ||
#endif
        I_AdvanceTrigger() || I_ReturnTrigger() || I_EscapeTrigger();
}

void I_GeneralTriggerClear(void)
{
    I_AdvanceTriggerClear();
//    I_ReturnTriggerClear();

#if !defined GEKKO
    CONTROL_ClearButton(gamefunc_Open);
    CONTROL_ClearButton(gamefunc_Fire);
#endif
}

int32_t I_EscapeTrigger(void) { return I_GetUserInput()->b_escape; }

int32_t I_MenuUp(void)
{
    return I_GetUserInput()->dir == dir_Up;
}

int32_t I_MenuDown(void)
{
    return I_GetUserInput()->dir == dir_Down;
}

int32_t I_MenuLeft(void) { return I_GetUserInput()->dir == dir_Left; }
int32_t I_MenuRight(void) { return I_GetUserInput()->dir == dir_Right; }

void I_MenuUpClear(void)
{
    I_ClearLast();
    CONTROL_ClearButton(gamefunc_Move_Forward);
}

void I_MenuDownClear(void)
{
    I_ClearLast();
    CONTROL_ClearButton(gamefunc_Move_Backward);
}

void I_MenuLeftClear(void)
{
    I_ClearLast();
    CONTROL_ClearButton(gamefunc_Turn_Left);
    CONTROL_ClearButton(gamefunc_Strafe_Left);
}

void I_MenuRightClear(void)
{
    I_ClearLast();
    CONTROL_ClearButton(gamefunc_Turn_Right);
    CONTROL_ClearButton(gamefunc_Strafe_Right);
}

int32_t I_MenuTabLeft(void)
{
    uint32_t const buttons = I_GetAllowedMenuGamepadButtons();
    g_menuGamepadLastButtons = buttons;
    return I_MenuGamepadButtonActive(buttons, CONTROLLER_BUTTON_LEFTSHOULDER);
}

int32_t I_MenuTabRight(void)
{
    uint32_t const buttons = I_GetAllowedMenuGamepadButtons();
    g_menuGamepadLastButtons = buttons;
    return I_MenuGamepadButtonActive(buttons, CONTROLLER_BUTTON_RIGHTSHOULDER);
}

int32_t I_SliderLeft(void) { return I_MenuLeft() || /*MOUSEACTIVECONDITIONAL*/(MOUSE_GetButtons() & M_WHEELDOWN); }
int32_t I_SliderRight(void) { return I_MenuRight() || /*MOUSEACTIVECONDITIONAL*/(MOUSE_GetButtons() & M_WHEELUP); }

int32_t I_PanelUp(void) { return I_MenuUp() || I_MenuLeft() || KB_KeyPressed(sc_PgUp); }
int32_t I_PanelDown(void) { return I_MenuDown() || I_MenuRight() || KB_KeyPressed(sc_PgDn); }

void I_PanelUpClear(void)
{
    I_ClearLast();
    KB_ClearKeyDown(sc_PgUp);
}

void I_PanelDownClear(void)
{
    I_ClearLast();
    KB_ClearKeyDown(sc_PgDn);
}


int32_t I_EnterText(char *t, int32_t maxlength, int32_t flags)
{
    char ch;
    int32_t inputloc = Bstrlen(typebuf);

    while ((ch = KB_GetCh()) != 0)
    {
        if (ch == asc_BackSpace)
        {
            if (inputloc > 0)
            {
                inputloc--;
                *(t+inputloc) = 0;
            }
        }
        else
        {
            if (ch == asc_Enter)
            {
                I_AdvanceTriggerClear();
                return 1;
            }
            else if (ch == asc_Escape)
            {
                I_ReturnTriggerClear();
                return -1;
            }
            else if (ch >= 32 && inputloc < maxlength && ch < 127)
            {
                if (!(flags & INPUT_NUMERIC) || (ch >= '0' && ch <= '9'))
                {
                    // JBF 20040508: so we can have numeric only if we want
                    *(t+inputloc) = ch;
                    *(t+inputloc+1) = 0;
                    inputloc++;
                }
            }
        }
    }

    if (I_TextSubmit())
    {
        I_TextSubmitClear();
        return 1;
    }
    if (I_ReturnTrigger())
    {
        I_ReturnTriggerClear();
        return -1;
    }

    return 0;
}
