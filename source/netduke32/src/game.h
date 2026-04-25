#ifndef _GAME_H
#define _GAME_H

#include "actors.h"
#include "gamevars.h"

#define IDFSIZE 479985668
#define IDFILENAME "DUKE3D.IDF"

#define MAX_RETURN_VALUES 6

#define USERMAPMUSICFAKEVOLUME MAXVOLUMES
#define USERMAPMUSICFAKELEVEL (MAXLEVELS-1)
#define USERMAPMUSICFAKESLOT ((USERMAPMUSICFAKEVOLUME * MAXLEVELS) + USERMAPMUSICFAKELEVEL)

#define VIEWSCREEN_ACTIVE_DISTANCE 2048
#if defined(GEKKO) || defined(__OPENDINGUX__)
    #define VIEWSCREENFACTOR 0
#elif defined(__ANDROID__)
    #define VIEWSCREENFACTOR 1
#else
    #define VIEWSCREENFACTOR 2
#endif

// Structs
typedef struct {
    vec3_t camerapos;
    int32_t const_visibility, uw_framerate;
    int32_t camera_time, folfvel, folavel, folx, foly, fola;
    int32_t reccnt, crosshairscale;

    int32_t runkey_mode, statusbarscale, mouseaiming, weaponswitch, drawweapon;   // JBF 20031125
    int32_t democams, color, m_color, msgdisptime, statusbarmode;
    int32_t autovote, automsg, idplayers, showweapons;
    int32_t team, viewbob, weaponsway, althud, weaponscale, textscale;

    int32_t entered_name, screen_tilting, shadows, fta_on, executions, auto_run;
    int32_t coords, showfps, levelstats, m_coop, coop, screen_size, lockout, crosshair;
    int32_t playerai, angleinterpolation, obituaries;

    int32_t recstat, brightness;
    int32_t m_recstat, detail;
    int32_t m_player_skill, m_level_number, m_volume_number, multimode;
    int32_t player_skill, level_number, volume_number, mouseflip;

    int32_t music_episode, music_level;

    int32_t monsters_killed, total_monsters;
    int32_t playerbest;

    int32_t configversion;

    int32_t fov;

    int32_t fraglimit;

    int32_t got_access;

    int32_t m_dmflags, dmflags;

    int32_t playerid_time;
    int8_t playerid_num;

    fix16_t cameraq16ang, cameraq16horiz;
    int16_t camerasect;
    int16_t pause_on, from_bonus;
    int16_t camerasprite, last_camsprite;
    int16_t last_level, secretlevel;

    uint8_t max_secret_rooms, secret_rooms;
    uint8_t gm;

    char overhead_on, last_overhead;
    char god, warp_on, cashman, eog, showallmap;
    char show_help, scrollmode, noclip;
    char ridecule[10][40];
    char savegame[10][22];
    char pwlockout[128], rtsname[128];
    char display_bonus_screen;
    char show_level_text;
    char wchoice[MAX_WEAPONS];
    char limit_hit, winner;

    int32_t returnvar[MAX_RETURN_VALUES - 1];

    struct
    {
        int AutoAim;
        int MouseBias;
        //int SmoothInput;

        // JBF 20031211: Store the input settings because
        // (currently) jmact can't regurgitate them
        int MouseFunctions[MAXMOUSEBUTTONS][2];
        int MouseDigitalFunctions[MAXMOUSEAXES][2];
        int MouseAnalogueAxes[MAXMOUSEAXES];
        int JoystickFunctions[MAXJOYBUTTONSANDHATS][2];
        int JoystickDigitalFunctions[MAXJOYAXES][2];
        int JoystickAnalogueAxes[MAXJOYAXES];
        int JoystickAnalogueScale[MAXJOYAXES];
        int JoystickAnalogueInvert[MAXJOYAXES];
        int JoystickAnalogueDead[MAXJOYAXES];
        int JoystickAnalogueSaturate[MAXJOYAXES];
        uint8_t KeyboardKeys[NUMGAMEFUNCTIONS][2];

        //
        // Sound variables
        //
        int FXDevice;
        int MusicDevice;
        int FXVolume;
        int MusicVolume;
        int SoundToggle;
        int MusicToggle;
        int VoiceToggle;
        int AmbienceToggle;

        int NumVoices;
        int NumChannels;
        int NumBits;
        int MixRate;

        //int ReverseStereo;

        // Setup Variables
        int UseJoystick;
        int UseMouse;
        int ScreenMode;
        int ScreenWidth;
        int ScreenHeight;
        int ScreenBPP;
        int ForceSetup;

        int scripthandle;
        int setupread;

        // Misc
        //int CheckForUpdates;
        int LastUpdateCheck;
        int useprecache;
        int PredictionDebug;
    } config;
} user_defs;
extern user_defs ud;

// Externs
extern char boardfilename[BMAX_PATH];
extern char currentboardfilename[BMAX_PATH];
extern int32_t MAXCACHE1DSIZE;
extern int32_t playerswhenstarted;
extern int32_t r_pr_defaultlights;

// Forward declarations
const char* G_DefaultRtsFile(void);
void P_DoQuote(int q, DukePlayer_t* p);

#ifdef __cplusplus
extern "C" {
#endif

void M32RunScript(const char* s); // Needed, because reasons.
void G_InitTimer(int32_t ticspersec);

#ifdef __cplusplus
}
#endif

// Inline Functions
static inline void G_MaybeAllocPlayer(int32_t playerNum)
{
    if (g_player[playerNum].ps != NULL)
        return;

    g_player[playerNum].ps = (DukePlayer_t*)Xcalloc(1, sizeof(DukePlayer_t));
    g_player[playerNum].inputBits = (input_t*)Xcalloc(1, sizeof(input_t));

    // Propagate initial settings for player to all.
    if(playerNum != 0)
        memcpy(g_player[playerNum].ps, g_player[0].ps, sizeof(DukePlayer_t));
}

static inline int32_t G_GetNextPlayer(int32_t pNum)
{
    for (int32_t i = pNum + 1; i < MAXPLAYERS; i++)
    {
        if ((g_player[i].ps != NULL) && g_player[i].connected)
            return i;
    }

    return -1;
}
#define ALL_PLAYERS(i) i = 0; i != -1; i = G_GetNextPlayer(i)

static inline int G_HaveUserMap(void)
{
    return (boardfilename[0] != 0 && ud.level_number == 7 && ud.volume_number == 0);
}

static inline int Menu_HaveUserMap(void)
{
    return (boardfilename[0] != 0 && ud.m_level_number == 7 && ud.m_volume_number == 0);
}

static inline int32_t G_GetLogoFlags(void)
{
    return Gv_GetVarByLabel("LOGO_FLAGS", 255, -1, -1);
}

static inline int G_GetMusicIdx(const char* str)
{
    int32_t lev, ep;
    signed char b1, b2;

    int numMatches = sscanf(str, "%c%d%c%d", &b1, &ep, &b2, &lev);

    if (numMatches != 4 || Btoupper(b1) != 'E' || Btoupper(b2) != 'L')
        return -1;

    if ((unsigned)--lev >= MAXLEVELS)
        return -2;

    if (ep == 0)
        return (MAXVOLUMES * MAXLEVELS) + lev;

    if ((unsigned)--ep >= MAXVOLUMES)
        return -2;

    return (ep * MAXLEVELS) + lev;
}

static inline bool G_CheckFemSprite(int32_t spriteNum)
{
    switch (tileGetMapping(sprite[spriteNum].picnum))
    {
        case FEM1__:
        case FEM2__:
        case FEM3__:
        case FEM4__:
        case FEM5__:
        case FEM6__:
        case FEM7__:
        case FEM8__:
        case FEM9__:
        case FEM10__:
        case PODFEM1__:
        case NAKED1__:
        case STATUE__:
        case TOUGHGAL__:
            return true;
    }

    return false;
}

static inline vec3_t G_GetCameraPosition(int32_t const i, int32_t const smoothratio)
{
    auto const cs = (uspriteptr_t)&sprite[i];
    const ActorData_t* const ca = &actor[i];

    return { ca->bpos.x + mulscale16(cs->x - ca->bpos.x, smoothratio),
             ca->bpos.y + mulscale16(cs->y - ca->bpos.y, smoothratio),
             ca->bpos.z + mulscale16(cs->z - ca->bpos.z, smoothratio) };
}

template <typename T>
static inline int G_GetViewscreenSizeShift(T const* spr)
{
#if VIEWSCREENFACTOR == 0
    UNREFERENCED_PARAMETER(spr);
    return VIEWSCREENFACTOR;
#else
    static CONSTEXPR int const mask = (1 << VIEWSCREENFACTOR) - 1;
    const int rem = (spr->xrepeat & mask) | (spr->yrepeat & mask);

    for (int i = 0; i < VIEWSCREENFACTOR; i++)
        if (rem & (1 << i))
            return i;

    return VIEWSCREENFACTOR;
#endif
}

static inline int32_t calc_smoothratio(ClockTicks const totalclk, ClockTicks const ototalclk)
{
    if (ud.pause_on || ud.show_help /*|| ud.gm & MODE_MENU*/)
        return 65536;

    return calc_smoothratio(totalclk, ototalclk, REALGAMETICSPERSEC);
}

static inline int32_t get_xdim(int32_t x)
{
    const int32_t screenwidth = scale(240 << 16, xdim, ydim);
    return scale((x << 16) + (screenwidth >> 1) - (160 << 16), xdim, screenwidth);
}
static inline int32_t get_ydim(int32_t y)
{
    y = mulscale16((y << 16) + rotatesprite_y_offset - (200 << 15), rotatesprite_yxaspect) + (200 << 15);
    return scale(y, ydim, 200 << 16);
}

static inline int32_t get_xdim_16(int32_t x)
{
    const int32_t screenwidth = scale(240 << 16, xdim, ydim);
    return scale(x + (screenwidth >> 1) - (160 << 16), xdim, screenwidth);
}
static inline int32_t get_ydim_16(int32_t y)
{
    y = mulscale16(y + rotatesprite_y_offset - (200 << 15), rotatesprite_yxaspect) + (200 << 15);
    return scale(y, ydim, 200 << 16);
}

#endif