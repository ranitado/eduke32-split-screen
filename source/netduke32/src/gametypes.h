#ifndef GAMETYPES_H
#define GAMETYPES_H

#include "player.h"

#define MAXGAMETYPES 16
#define GTFLAGS(x) (Gametypes[ud.coop].flags & (x))

enum GametypeFlags_t {
    GAMETYPE_COOP                   = (1 << 0), // Cooperative mode
    GAMETYPE_WEAPSTAY               = (1 << 1), // Weapon stay enabled
    GAMETYPE_FRAGBAR                = (1 << 2), // Enable the frag bar
    GAMETYPE_SCORESHEET             = (1 << 3), // Show the scoreboard at level end
    GAMETYPE_DMSWITCHES             = (1 << 4), // Enable DM-only switches
    GAMETYPE_COOPSPAWN              = (1 << 5), // Use cooperative mode spawn points
    GAMETYPE_ACCESSCARDSPRITES      = (1 << 6), // Access cards spawn in map
    GAMETYPE_COOPVIEW               = (1 << 7), // Can view other players' perspectives
    GAMETYPE_COOPSOUND              = (1 << 8), // Can hear other players' sounds
    GAMETYPE_OTHERPLAYERSINMAP      = (1 << 9), // Other players show in automap
    GAMETYPE_ALLOWITEMRESPAWN       = (1 << 10), // Allow item respawning
    GAMETYPE_MARKEROPTION           = (1 << 11), // Allow enabling respawn markers
    GAMETYPE_PLAYERSFRIENDLY        = (1 << 12), // All players are allies
    GAMETYPE_FIXEDRESPAWN           = (1 << 13), // Always respawn at the same point
    GAMETYPE_ACCESSATSTART          = (1 << 14), // All access cards at start
    GAMETYPE_PRESERVEINVENTORYEOL   = (1 << 15), // Inventory is preserved betwen level transitions
    GAMETYPE_TDM                    = (1 << 16), // Team deathmatch
    GAMETYPE_TDMSPAWN               = (1 << 17), // Respawn near teammates
    GAMETYPE_LIMITEDLIVES           = (1 << 18), // Lives are limited. Die more than ud.fraglmit, you cannot respawn.
};

enum DMFlags_t {
    DMFLAG_FRIENDLYFIRE             = (1 << 0), // Friendly fire
    DMFLAG_NOMONSTERS               = (1 << 1), // No monsters
    DMFLAG_RESPAWNMONSTERS          = (1 << 2), // Respawn monsters
    DMFLAG_RESPAWNITEMS             = (1 << 3), // Respawn items
    DMFLAG_RESPAWNINVENTORY         = (1 << 4), // Respawn inventory
    DMFLAG_MARKERS                  = (1 << 5), // Enable respawn markers
    DMFLAG_NOEXITS                  = (1 << 6), // Disable exits
    DMFLAG_WEAPONSTAY               = (1 << 7), // Weapons stay
    DMFLAG_NOPLAYERIDS              = (1 << 8), // Disable player IDs
    DMFLAG_NOFOVCHANGE              = (1 << 9), // Disable FOV changes
    DMFLAG_NOWEAPONICONS            = (1 << 10), // Disable overhead weapon icons
    DMFLAG_ALLOWVISIBILITYCHANGE    = (1 << 11), // Allow visibility changes
    DMFLAG_NODMWEAPONSPAWNS         = (1 << 12), // Disable DM weapon spawns in cooperative.
    DMFLAG_ALLOWDOUBLEKICK          = (1 << 13), // Restore the Duke3D v1.3D double-kick bug.
    DMFLAG_KEEPITEMSACROSSEPISODES  = (1 << 14), // Keep inventory across episodes in cooperative.
};

#define DEFAULT_DMFLAGS (DMFLAG_FRIENDLYFIRE | DMFLAG_MARKERS)

static inline void DMFLAGS_SET(int32_t flags, bool state)
{
    if (state)
        ud.dmflags |= (flags);
    else
        ud.dmflags &= ~(flags);
}

static inline void M_DMFLAGS_SET(int32_t flags, bool state)
{
    if (state)
        ud.m_dmflags |= (flags);
    else
        ud.m_dmflags &= ~(flags);
}

static inline bool DMFLAGS_TEST(int32_t flags)
{
    return (ud.dmflags & (flags));
}

static inline bool M_DMFLAGS_TEST(int32_t flags)
{
    return (ud.m_dmflags & (flags));
}

typedef struct {
    char name[33];
    int32_t flags;
} Gametype_t;
extern Gametype_t Gametypes[MAXGAMETYPES];
extern uint8_t g_numGametypes;

typedef struct {
    char name[32];
    uint8_t palnum;
    int16_t frags;
    int16_t suicides;
    int8_t blocked;
} Team_t;
extern Team_t teams[MAXPLAYERS];
extern uint8_t numTeams;

// Inline Functions

// Returns true if attacking a friendly while friendly fire is disabled.
static inline bool G_BlockFriendlyFire(int32_t p1, int32_t p2)
{
    if (DMFLAGS_TEST(DMFLAG_FRIENDLYFIRE))
        return false;

    if (GTFLAGS(GAMETYPE_PLAYERSFRIENDLY))
        return true;

    if (GTFLAGS(GAMETYPE_TDM) && (g_player[p1].ps->team == g_player[p2].ps->team))
        return true;

    return false;
}

// Formerly clearfrags(), clears gametype stats such as frags, suicides, etc.
static inline void G_ClearGametypeStats(void)
{
    int32_t i;
    playerdata_t* p;

    for (i = 0; i < MAXPLAYERS; i++)
    {
        p = &g_player[i];

        if (p->ps != NULL)
        {
            p->ps->deaths = p->ps->frag = p->ps->fraggedself = 0;
            p->ps->lives = max(1, ud.fraglimit);
            Bmemset(p->frags, 0, sizeof(p->frags));
        }
    }

    for (i = 0; i < numTeams; i++)
    {
        teams[i].frags = 0;
        teams[i].suicides = 0;
        teams[i].blocked = 0;
    }
}

#endif