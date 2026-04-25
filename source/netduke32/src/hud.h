#ifndef HUD_H
#define HUD_H

#include "gametypes.h"
#include "player.h"

// Definitions
#define CROSSHAIR_PAL (MAXPALOOKUPS-RESERVEDPALS-1)

#define POLYMOSTTRANS (1)
#define POLYMOSTTRANS2 (1|32)

// Externals
extern int32_t althud_numbertile;
extern int32_t althud_numberpal;
extern int32_t althud_shadows;
extern int32_t althud_flashing;

extern int32_t hud_glowingquotes;
extern int32_t hud_showmapname;
extern int32_t hud_fragbarstyle;

extern palette_t CrosshairColors;
extern palette_t DefaultCrosshairColors;
extern int32_t g_crosshairSum;

extern int32_t g_levelTextTime;

// Forward Declarations
void G_SetStatusBarScale(int sc);
void G_DrawFrags(void);
void G_DrawInventory(DukePlayer_t* p);
void G_DrawStatusBar(int snum);
void G_DrawLevelStats();

void G_GetCrosshairColor(void);
void G_SetCrosshairColor(int32_t r, int32_t g, int32_t b);
void G_DrawCrosshair(void);

void G_DisplayPlayerId(void);
void G_AddKillToFeed(int victim, int attacker);

void G_PrintLevelName(void);

// Inline Functions
static inline int32_t sbarsc(int32_t sc)
{
    return scale(sc, ud.statusbarscale, 100);
}

static inline int32_t sbarx(int32_t x)
{
    if (ud.screen_size == 4) return sbarsc(x << 16);
    return (((320 << 16) - sbarsc(320 << 16)) >> 1) + sbarsc(x << 16);
}

static inline int32_t sbarxr(int32_t x)
{
    if (ud.screen_size == 4) return (320 << 16) - sbarsc(x << 16);
    return (((320 << 16) - sbarsc(320 << 16)) >> 1) + sbarsc(x << 16);
}

static inline int32_t sbary(int32_t y)
{
    return (200 << 16) - sbarsc(200 << 16) + sbarsc(y << 16);
}

int32_t inline sbarx16(int32_t x)
{
    if (ud.screen_size == 4) return sbarsc(x);
    return (((320 << 16) - sbarsc(320 << 16)) >> 1) + sbarsc(x);
}

int32_t inline sbary16(int32_t y)
{
    return (200 << 16) - sbarsc(200 << 16) + sbarsc(y);
}

static inline int32_t textsc(int sc)
{
    // prevent ridiculousness to a degree
    if (xdim <= 320) return sc;
    else if (xdim <= 640) return scale(sc, min(200, ud.textscale), 100);
    else if (xdim <= 800) return scale(sc, min(300, ud.textscale), 100);
    else if (xdim <= 1024) return scale(sc, min(350, ud.textscale), 100);
    return scale(sc, ud.textscale, 100);
}

static inline int32_t G_GetFragbarOffset()
{
    if (!GTFLAGS(GAMETYPE_FRAGBAR) || hud_fragbarstyle != 1 || ud.screen_size <= 0 || ud.multimode <= 1)
        return 0;

    return 8 + (8 * ((playerswhenstarted - 1) >> 2));
}

#endif // HUD_H