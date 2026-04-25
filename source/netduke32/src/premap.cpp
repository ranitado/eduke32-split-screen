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

#include "duke3d.h"
#include "osd.h"

extern int everyothertime;
static int g_lastPlayerPal = 0;
int g_numRealPalettes;

static char precachehightile[2][MAXTILES>>3];
static int  g_precacheCount;

static void flag_precache(int32_t tile, int32_t type)
{
    if (!bitmap_test(gotpic, tile))
        g_precacheCount++;

    bitmap_set(gotpic, tile);
    bitmap_set(precachehightile[type], tile);
}

static void tloadtile(int tilenume, int type)
{
    int firstTile, lastTile;

    if ((picanm[tilenume].sf & PICANM_ANIMTYPE_MASK) == PICANM_ANIMTYPE_BACK)
    {
        firstTile = tilenume - picanm[tilenume].num;
        lastTile = tilenume;
    }
    else
    {
        firstTile = tilenume;
        lastTile = tilenume + picanm[tilenume].num;
    }

    for (; firstTile <= lastTile; firstTile++)
        flag_precache(firstTile, type);
}

static void G_DemoLoadScreen(const char* statustext, int const loadScreenTile, int percent)
{
    if (statustext == NULL)
    {
        videoClearScreen(0L);
        // g_player[myconnectindex].ps->palette = palette;
        // G_FadePalette(0,0,0,0);
        P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 0);  // JBF 20040308
    }

    if ((unsigned)loadScreenTile < (MAXTILES << 1))
    {
        rotatesprite_fs(320 << 15, 200 << 15, 65536L, 0, loadScreenTile, 0, 0, 2 + 8 + 64);
    }
    else
    {
        videoNextPage();
        return;
    }

    menutext(160, 105, 0, 0, "LOADING...");

    if (statustext)
        gametext(160, 180, statustext, 0, 2 + 8 + 16);

    Gv_SetVar(sysVarIDs.RETURN, percent , -1, -1);
    X_OnEvent(EVENT_DISPLAYLOADINGSCREEN, g_player[screenpeek].ps->i, screenpeek, -1);
    videoNextPage();
}

static void G_DoLoadScreen(const char* statustext, int percent)
{
    Gv_SetVar(sysVarIDs.RETURN, LOADSCREEN, -1, -1);
    X_OnEvent(EVENT_GETLOADTILE, g_player[screenpeek].ps->i, screenpeek, -1);
    int const loadScreenTile = Gv_GetVar(sysVarIDs.RETURN, -1, -1);

    if (ud.recstat == 2)
    {
        G_DemoLoadScreen(statustext, loadScreenTile, percent);
        return;
    }

    int const screenSize = ud.screen_size;

    P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 1);

    if (statustext == NULL)
    {
        ud.screen_size = 0;
        G_UpdateScreenArea();
        videoClearScreen(0L);
    }

    if ((unsigned)loadScreenTile < (MAXTILES << 1))
    {
        videoClearScreen(0);
        rotatesprite_fs(320 << 15, 200 << 15, 65536L, 0, loadScreenTile, 0, 0, 2 + 8 + 64);
    }
    else
    {
        videoNextPage();
        return;
    }

    if (boardfilename[0] != 0 && ud.level_number == 7 && ud.volume_number == 0)
    {
        menutext(160, 90, 0, 0, "LOADING USER MAP");
        gametextpal(160, 90 + 10, boardfilename, 14, 2);
    }
    else
    {
        menutext(160, 90, 0, 0, "LOADING");

        if (g_mapInfo[(ud.volume_number * MAXLEVELS) + ud.level_number].name != NULL)
            menutext(160, 90 + 16 + 8, 0, 0, g_mapInfo[(ud.volume_number * MAXLEVELS) + ud.level_number].name);
    }

#ifndef EDUKE32_TOUCH_DEVICES
    if (statustext)
        gametext(160, 180, statustext, 0, 2 + 8 + 16);
#endif

    if (percent != -1)
    {
        int const     width = scale(scale(xdim - 1, 288, 320), percent, 100);
        int constexpr tile = 929;
        int constexpr bits = 2 + 8 + 16;

        rotatesprite(31 << 16, 145 << 16, 65536, 0, tile, 15, 0, bits, 0, 0, width, ydim - 1);
        rotatesprite(159 << 16, 145 << 16, 65536, 0, tile, 15, 0, bits, 0, 0, width, ydim - 1);

        rotatesprite(30 << 16, 144 << 16, 65536, 0, tile, 0, 0, bits, 0, 0, width, ydim - 1);
        rotatesprite(158 << 16, 144 << 16, 65536, 0, tile, 0, 0, bits, 0, 0, width, ydim - 1);
    }

    Gv_SetVar(sysVarIDs.RETURN, percent, -1, -1);
    X_OnEvent(EVENT_DISPLAYLOADINGSCREEN, g_player[screenpeek].ps->i, screenpeek, -1);
    videoNextPage();

    if (!statustext)
    {
        KB_FlushKeyboardQueue();
        ud.screen_size = screenSize;
    }
}

static void cacheTilesForSprite(int spriteNum)
{
    if (DMFLAGS_TEST(DMFLAG_NOMONSTERS) && A_CheckEnemySprite(&sprite[spriteNum])) return;

    int const picnum = sprite[spriteNum].picnum == RESPAWN ? sprite[spriteNum].hitag : sprite[spriteNum].picnum;
    int extraTiles = 1;

    for (int j = picnum; j <= g_tile[picnum].cacherange; j++)
        tloadtile(j, 1);

#ifndef EDUKE32_STANDALONE
    switch (tileGetMapping(picnum))
    {
        case HYDRENT__:
            tloadtile(BROKEFIREHYDRENT, 1);
            for (int j = TOILETWATER; j < (TOILETWATER + 4); j++) tloadtile(j, 1);
            break;
        case TOILET__:
            tloadtile(TOILETBROKE, 1);
            for (int j = TOILETWATER; j < (TOILETWATER + 4); j++) tloadtile(j, 1);
            break;
        case STALL__:
            tloadtile(STALLBROKE, 1);
            for (int j = TOILETWATER; j < (TOILETWATER + 4); j++) tloadtile(j, 1);
            break;
        case RUBBERCAN__:
            extraTiles = 2;
            break;
        case TOILETWATER__:
            extraTiles = 4;
            break;
        case FEMPIC1__:
            extraTiles = 44;
            break;
        case LIZTROOP__:
        case LIZTROOPRUNNING__:
        case LIZTROOPSHOOT__:
        case LIZTROOPJETPACK__:
        case LIZTROOPONTOILET__:
        case LIZTROOPDUCKING__:
            for (int j = LIZTROOP; j < (LIZTROOP + 72); j++) tloadtile(j, 1);
            for (int j = HEADJIB1; j < LEGJIB1 + 3; j++) tloadtile(j, 1);
            extraTiles = 0;
            break;
        case WOODENHORSE__:
            extraTiles = 5;
            for (int j = HORSEONSIDE; j < (HORSEONSIDE + 4); j++) tloadtile(j, 1);
            break;
        case NEWBEAST__:
        case NEWBEASTSTAYPUT__:
            extraTiles = 90;
            break;
        case BOSS1__:
        case BOSS2__:
        case BOSS3__:
        case SHARK__:
            extraTiles = 30;
            break;
        case OCTABRAIN__:
        case OCTABRAINSTAYPUT__:
        case COMMANDER__:
        case COMMANDERSTAYPUT__:
            extraTiles = 38;
            break;
        case RECON__:
            extraTiles = 13;
            break;
        case PIGCOP__:
        case PIGCOPDIVE__:
            extraTiles = 61;
            break;
        case LIZMAN__:
        case LIZMANSPITTING__:
        case LIZMANFEEDING__:
        case LIZMANJUMP__:
            for (int j = LIZMANHEAD1; j < LIZMANLEG1 + 3; j++) tloadtile(j, 1);
            extraTiles = 80;
            break;
        case APLAYER__:
            extraTiles = 0;
            //if (ud.multimode > 1) // [JM] Let's precache this anyway, since we can see this in mirrors.
            {
                extraTiles = 5;
                for (int j = 1420; j < 1420 + 106; j++) tloadtile(j, 1);
            }
            break;
        case ATOMICHEALTH__:
            extraTiles = 14;
            break;
        case DRONE__:
            extraTiles = 10;
            break;
        case EXPLODINGBARREL__:
        case SEENINE__:
        case OOZFILTER__:
            extraTiles = 3;
            break;
        case NUKEBARREL__:
        case CAMERA1__:
            extraTiles = 5;
            break;
            // caching of HUD sprites for weapons that may be in the level
        case CHAINGUNSPRITE__:
            for (int j = CHAINGUN; j <= CHAINGUN + 7; j++) tloadtile(j, 1);
            break;
        case RPGSPRITE__:
            tloadtile(RPGGUN, 1);
            for (int j = RPGGUN + 1; j <= RPGGUN + 2; j++) tloadtile(j, 1);
            break;
        case FREEZESPRITE__:
            tloadtile(FREEZE, 1);
            tloadtile(FREEZE + 2, 1);
            for (int j = FREEZE + 3; j <= FREEZE + 5; j++) tloadtile(j, 1);
            break;
        case GROWSPRITEICON__:
        case SHRINKERSPRITE__:
        {
            int32_t const tile = SHRINKER;
            for (int j = tile - 2; j <= tile + 1; j++) tloadtile(j, 1);
            for (int j = SHRINKER + 2; j <= SHRINKER + 5; j++) tloadtile(j, 1);
            break;
        }
        case HBOMBAMMO__:
        case HEAVYHBOMB__:
            for (int j = HANDREMOTE; j <= HANDREMOTE + 5; j++) tloadtile(j, 1);
            break;
        case TRIPBOMBSPRITE__:
            for (int j = HANDHOLDINGLASER; j <= HANDHOLDINGLASER + 4; j++) tloadtile(j, 1);
            break;
        case SHOTGUNSPRITE__:
            tloadtile(SHOTGUNSHELL, 1);
            for (int j = SHOTGUN; j <= SHOTGUN + 6; j++) tloadtile(j, 1);
            break;
        case DEVISTATORSPRITE__:
            for (int j = DEVISTATOR; j <= DEVISTATOR + 1; j++) tloadtile(j, 1);
            break;
    }
#endif

    for (int j = picnum; j < (picnum + extraTiles); j++)
        tloadtile(j, 1);
}

#ifndef EDUKE32_STANDALONE
static void cacheDukeTiles(void)
{
    //tloadtile(sbartile(), 1);
    tloadtile(BOTTOMSTATUSBAR, 1);

    if (ud.multimode > 1)
        tloadtile(FRAGBAR, 1);

    tloadtile(VIEWSCREEN, 1);

    for (int i = STARTALPHANUM; i < ENDALPHANUM + 1; i++)
        tloadtile(i, 1);
    for (int i = BIGALPHANUM - 11; i < BIGALPHANUM + 82; i++)
        tloadtile(i, 1);
    for (int i = MINIFONT; i < MINIFONT + 93; i++)
        tloadtile(i, 1);

    for (int i = FOOTPRINTS; i < FOOTPRINTS + 3; i++)
        tloadtile(i, 1);

    for (int i = BURNING; i < BURNING + 14; i++)
        tloadtile(i, 1);
    for (int i = BURNING2; i < BURNING2 + 14; i++)
        tloadtile(i, 1);

    for (int i = CRACKKNUCKLES; i < CRACKKNUCKLES + 4; i++)
        tloadtile(i, 1);

    for (int i = FIRSTGUN; i < FIRSTGUN + 3; i++)
        tloadtile(i, 1);
    tloadtile(FIRSTGUNRELOAD, 1);
    //tloadtile(WORLDTOUR ? FIRSTGUNRELOADWIDE : FIRSTGUNRELOAD + 1, 1);
    tloadtile(FIRSTGUNRELOAD + 1, 1);
    tloadtile(FIRSTGUNRELOAD + 2, 1);
    tloadtile(FIRSTGUNRELOAD + 4, 1);
    for (int i = SHELL; i < SHELL + 2; i++)
        tloadtile(i, 1);

    for (int i = EXPLOSION2; i < EXPLOSION2 + 21; i++)
        tloadtile(i, 1);

    for (int i = COOLEXPLOSION1; i < COOLEXPLOSION1 + 21; i++)
        tloadtile(i, 1);

    tloadtile(BULLETHOLE, 1);
    tloadtile(BLOODPOOL, 1);

    for (int i = TRANSPORTERBEAM; i < (TRANSPORTERBEAM + 6); i++)
        tloadtile(i, 1);

    for (int i = SMALLSMOKE; i < (SMALLSMOKE + 4); i++)
        tloadtile(i, 1);
    for (int i = SHOTSPARK1; i < (SHOTSPARK1 + 4); i++)
        tloadtile(i, 1);

    for (int i = BLOOD; i < (BLOOD + 4); i++)
        tloadtile(i, 1);
    for (int i = JIBS1; i < (JIBS5 + 5); i++)
        tloadtile(i, 1);
    for (int i = JIBS6; i < (JIBS6 + 8); i++)
        tloadtile(i, 1);

    for (int i = SCRAP1; i < (SCRAP1 + 29); i++)
        tloadtile(i, 1);

    tloadtile(FIRELASER, 1);

    for (int i = TRANSPORTERSTAR; i < TRANSPORTERSTAR + 6; i++)
        tloadtile(i, 1);

    for (int i = FORCERIPPLE; i < (FORCERIPPLE + 9); i++)
        tloadtile(i, 1);

    for (int i = MENUSCREEN; i < DUKECAR; i++)
        tloadtile(i, 1);

    for (int i = RPG; i < RPG + 7; i++)
        tloadtile(i, 1);
    for (int i = FREEZEBLAST; i < FREEZEBLAST + 3; i++)
        tloadtile(i, 1);
    for (int i = SHRINKSPARK; i < SHRINKSPARK + 4; i++)
        tloadtile(i, 1);
    for (int i = GROWSPARK; i < GROWSPARK + 4; i++)
        tloadtile(i, 1);
    for (int i = SHRINKEREXPLOSION; i < SHRINKEREXPLOSION + 4; i++)
        tloadtile(i, 1);
    for (int i = MORTER; i < MORTER + 4; i++)
        tloadtile(i, 1);
    for (int i = 0; i <= 60; i++)
        tloadtile(i, 1);
}
#endif

static void cacheFlaggedTiles(void)
{
    for (int i = 0; i < MAXTILES; i++)
    {
        if (g_tile[i].flags & SFLAG_PROJECTILE)
            tloadtile(i, 1);

        if (A_CheckSpriteTileFlags(i, SFLAG_CACHE))
            for (int j = i; j <= g_tile[i].cacherange; j++)
                tloadtile(j, 1);
    }

#ifndef EDUKE32_STANDALONE
    cacheDukeTiles();
#endif
}

static void cacheExtraTextureMaps(int tileNum, int type)
{
#ifdef USE_OPENGL
    for (int i = 0; i < MAXPALOOKUPS - RESERVEDPALS - 1; i++)
    {
#ifdef POLYMER
        if (videoGetRenderMode() != REND_POLYMER || !polymer_havehighpalookup(0, i))
#endif
            polymost_precache(tileNum, i, type);
    }
#else
    UNREFERENCED_PARAMETER(tileNum);
    UNREFERENCED_PARAMETER(type);
#endif

#ifdef USE_GLEXT
    if (r_detailmapping)
        polymost_precache(tileNum, DETAILPAL, type);

    if (r_glowmapping)
        polymost_precache(tileNum, GLOWPAL, type);
#endif
#ifdef POLYMER
    if (videoGetRenderMode() == REND_POLYMER)
    {
        if (pr_specularmapping)
            polymost_precache(tileNum, SPECULARPAL, type);

        if (pr_normalmapping)
            polymost_precache(tileNum, NORMALPAL, type);
    }
#endif
}

void G_CacheMapData(void)
{
    if (ud.recstat == 2 || !ud.config.useprecache)
        return;

    g_precacheCount = 0;
    Bmemset(gotpic, 0, sizeof(gotpic));
    Bmemset(precachehightile, 0, sizeof(precachehightile));

    S_TryPlaySpecialMusic(MUS_LOADING);

#if defined EDUKE32_TOUCH_DEVICES && defined USE_OPENGL
    polymost_glreset();
#endif

    uint32_t const cacheStartTime = timerGetTicks();

    cacheFlaggedTiles();

    for (int i = 0; i < numwalls; i++)
    {
        tloadtile(wall[i].picnum, 0);

        if (wall[i].overpicnum >= 0)
            tloadtile(wall[i].overpicnum, 0);
    }

    for (int i = 0; i < numsectors; i++)
    {
        tloadtile(sector[i].floorpicnum, 0);
        tloadtile(sector[i].ceilingpicnum, 0);

        for (int SPRITES_OF_SECT(i, j))
        {
            if (sprite[j].xrepeat != 0 && sprite[j].yrepeat != 0 && (sprite[j].cstat & CSTAT_SPRITE_INVISIBLE) == 0)
                cacheTilesForSprite(j);
        }
    }

    int cnt = 0;
    int cntDisplayed = -1;
    int pctDisplayed = -1;
    int i = 0;

    while (cnt < g_precacheCount)
    {
        if (bitmap_test(gotpic, i))
        {
            cnt++;

            if (waloff[i] == 0)
                tileLoad((int16_t)i);

            for (int j = 0; j < 2; j++)
            {
                if (bitmap_test(precachehightile[j], i))
                {
                    tileLoadScaled(i);

                    if (videoGetRenderMode() != REND_CLASSIC)
                        cacheExtraTextureMaps(i, j);
                }
            }

            gameHandleEvents();
            if (KB_KeyPressed(sc_Space))
                break;
        }
        i++;

        if (cntDisplayed + (i & 7) < cnt - (i & 7) && engineFPSLimit(true))
        {
            int const percentComplete = min(100, tabledivide32(100 * cntDisplayed, g_precacheCount));
            cntDisplayed = logapproach(cntDisplayed, cnt);
            pctDisplayed = logapproach(pctDisplayed, percentComplete);
            Bsprintf(tempbuf, "Loaded %d%% (%d/%d textures)\n", pctDisplayed, cntDisplayed, g_precacheCount);
            G_DoLoadScreen(tempbuf, pctDisplayed);
        }
    }

    Bmemset(gotpic, 0, sizeof(gotpic));

    LOG_F(INFO, "Cache time: %dms.", timerGetTicks() - cacheStartTime);
}

void G_UpdateScreenArea(void)
{
    int ss, x1, x2, y1, y2;

    if (ud.screen_size < 0) ud.screen_size = 0;
    if (ud.screen_size > 64) ud.screen_size = 64;
    if (ud.screen_size == 0) renderFlushPerms();

    ss = max(ud.screen_size-8,0);

    x1 = scale(ss,xdim,160);
    x2 = xdim-x1;

    y1 = ss;
    y2 = 200;

    y1 += G_GetFragbarOffset();

    if (ud.screen_size >= 8 && !(videoGetRenderMode() >= 3 && ud.screen_size == 8 && ud.statusbarmode))
        y2 -= (ss+scale(tilesiz[BOTTOMSTATUSBAR].y,ud.statusbarscale,100));

    y1 = scale(y1,ydim,200);
    y2 = scale(y2,ydim,200);

    if (videoGetRenderMode() >= 3)
        videoSetViewableArea(x1,y1,x2-1,y2);
    else videoSetViewableArea(x1,y1,x2-1,y2-1);

    G_GetCrosshairColor();
    G_SetCrosshairColor(CrosshairColors.r, CrosshairColors.g, CrosshairColors.b);

    pub = NUMPAGES;
    pus = NUMPAGES;
}

void G_RevertUnconfirmedGameSettings()
{
    ud.m_level_number = ud.level_number;
    ud.m_volume_number = ud.volume_number;
    ud.m_player_skill = ud.player_skill;
    ud.m_coop = ud.coop;
    ud.m_dmflags = ud.dmflags;

    if (!Menu_HaveUserMap())
        boardfilename[0] = 0;
}

void P_RandomSpawnPoint(int snum)
{
    DukePlayer_t *p = g_player[snum].ps;
    int i = snum;
    unsigned int dist,pdist = -1;

    if (ud.multimode > 1 && !(Gametypes[ud.coop].flags & GAMETYPE_FIXEDRESPAWN))
    {
        i = krand() % g_numPlayerSprites;
        if (Gametypes[ud.coop].flags & GAMETYPE_TDMSPAWN)
        {
            for(int32_t ALL_PLAYERS(playerNum))
            {
                auto otherp = g_player[playerNum].ps;
                if (playerNum != snum && otherp->team == p->team && sprite[otherp->i].extra > 0)
                {
                    for (int32_t pointNum = 0; pointNum < g_numPlayerSprites; pointNum++)
                    {
                        auto spawnPoint = &g_playerSpawnPoints[pointNum];
                        dist = FindDistance2D(otherp->pos.x - spawnPoint->pos.x, otherp->pos.y - spawnPoint->pos.y);
                        if (dist < pdist)
                            i = pointNum, pdist = dist;
                    }
                    break;
                }
            }
        }
    }

    // We don't have a spawn for this player in co-op, so use the default!
    if (i >= g_numPlayerSprites)
        i = 0;
 
    sprite[p->i].xyz = actor[p->i].bpos = p->opos = p->pos = g_playerSpawnPoints[i].pos;
    p->bobposx = p->pos.x;
    p->bobposy = p->pos.y;
    //p->pos.z += PHEIGHT;

    p->q16ang = fix16_from_int(g_playerSpawnPoints[i].ang);
    p->cursectnum = g_playerSpawnPoints[i].sectnum;

    // [JM] Needed to prevent perpetual gibbing after being killed by a space sector.
    changespritesect(p->i, p->cursectnum);
}

static void P_ResetStatus(int snum)
{
    DukePlayer_t *p = g_player[snum].ps;

    ud.show_help        = 0;
    ud.showallmap       = 0;
    p->dead_flag        = 0;
    p->wackedbyactor    = -1;
    p->falling_counter  = 0;
    p->quick_kick       = 0;
    p->subweapon        = 0;
    p->last_full_weapon = 0;
    p->ftq              = 0;
    p->fta              = 0;
    p->tipincs          = 0;
    p->buttonpalette    = 0;
    p->actorsqu         =-1;
    p->invdisptime      = 0;
    p->last_pissed_time = 0;
    p->holster_weapon   = 0;
    p->pycount          = 0;
    p->pyoff            = 0;
    p->opyoff           = 0;
    p->loogcnt          = 0;
    p->q16angvel        = 0;
    p->weapon_sway      = 0;
    //p->select_dir     = 0;
    p->extra_extra8     = 0;
    p->show_empty_weapon= 0;
    p->dummyplayersprite=-1;
    p->crack_time       = 0;
    p->hbomb_hold_delay = 0;
    p->transporter_hold = 0;
    p->wantweaponfire   = -1;
    p->hurt_delay       = 0;
    p->footprintcount   = 0;
    p->footprintpal     = 0;
    p->footprintshade   = 0;
    p->jumping_toggle   = 0;
    p->oq16horiz = p->q16horiz = F16(140);
    p->q16horizoff      = 0;
    p->bobcounter       = 0;
    p->on_ground        = 0;
    p->player_par       = 0;
    p->return_to_center = 9;
    p->airleft          = 15*26;
    p->rapid_fire_hold  = 0;
    p->toggle_key_flag  = 0;
    p->access_spritenum = -1;

    if (ud.multimode > 1 && (Gametypes[ud.coop].flags & GAMETYPE_ACCESSATSTART))
        ud.got_access   = 7;
    else ud.got_access  = 0;

    p->random_club_frame= 0;
    pus = 1;
    p->on_warping_sector = 0;
    p->spritebridge      = 0;
    p->sbs          = 0;
    p->palette = BASEPAL;

    if (p->inv_amount[GET_STEROIDS] < 400)
    {
        p->inv_amount[GET_STEROIDS] = 0;
        p->inven_icon = 0;
    }
    p->heat_on =            0;
    p->jetpack_on =         0;
    p->holoduke_on =       -1;

    p->look_ang          = 512 - ((ud.level_number&1)<<10);

    p->rotscrnang        = 0;
    p->orotscrnang       = 1;   // JBF 20031220
    p->newowner          =-1;
    p->jumping_counter   = 0;
    p->hard_landing      = 0;
    p->vel.x             = 0;
    p->vel.y             = 0;
    p->vel.z             = 0;
    fricxv            = 0;
    fricyv            = 0;
    p->somethingonplayer =-1;
    p->one_eighty_count  = 0;
    p->cheat_phase       = 0;

    p->on_crane          = -1;

    if ((aplWeaponWorksLike[p->curr_weapon][snum] == PISTOL_WEAPON) &&
            (aplWeaponReload[p->curr_weapon][snum] > aplWeaponTotalTime[p->curr_weapon][snum]))
        p->kickback_pic  = aplWeaponTotalTime[p->curr_weapon][snum]+1;
    else p->kickback_pic = 0;

    p->weapon_pos        = 6;
    p->walking_snd_toggle= 0;
    p->weapon_ang        = 0;

    p->knuckle_incs      = 1;
    p->fist_incs = 0;
    p->knee_incs         = 0;
    p->jetpack_on        = 0;
    p->reloading        = 0;

    p->movement_lock     = 0;

    P_UpdateScreenPal(p);
    X_OnEvent(EVENT_RESETPLAYER, p->i, snum, -1);
}

void P_ResetWeapons(int snum)
{
    int weapon;
    DukePlayer_t *p = g_player[snum].ps;

    for (weapon = PISTOL_WEAPON; weapon < MAX_WEAPONS; weapon++)
        p->gotweapon[weapon] = 0;
    for (weapon = PISTOL_WEAPON; weapon < MAX_WEAPONS; weapon++)
        p->ammo_amount[weapon] = 0;

    p->weapon_pos = 6;
    p->kickback_pic = 5;
    p->curr_weapon = PISTOL_WEAPON;
    p->gotweapon[PISTOL_WEAPON] = 1;
    p->gotweapon[KNEE_WEAPON] = 1;
    p->ammo_amount[PISTOL_WEAPON] = 48;
    p->gotweapon[HANDREMOTE_WEAPON] = 1;
    p->last_weapon = -1;
    p->last_used_weapon = -1;

    p->show_empty_weapon= 0;
    p->last_pissed_time = 0;
    p->holster_weapon = 0;

    Gv_SetVar(sysVarIDs.WEAPON, p->curr_weapon, p->i, snum);
    X_OnEvent(EVENT_RESETWEAPONS, p->i, snum, -1);
}

void P_ResetInventory(int snum)
{
    DukePlayer_t *p = g_player[snum].ps;

    p->inven_icon       = 0;
    p->scuba_on         = 0;
    p->heat_on          = 0;
    p->jetpack_on       = 0;
    p->holoduke_on      = -1;
    p->inven_icon       = 0;

    for (int i = 0; i < GET_MAX; i++)
        p->inv_amount[i] = 0;

    p->inv_amount[GET_SHIELD] = g_startArmorAmount;
    X_OnEvent(EVENT_RESETINVENTORY, p->i, snum, -1);
}

static void resetpremapglobals(void)
{
    int i;

    randomseed = 1996;
    g_random = randomseed_t();

    screenpeek = myconnectindex;
    parallaxtype = 0;
    tempwallptr = 0;
    g_curViewscreen = -1;

    ud.total_monsters = 0;
    ud.monsters_killed = 0;
    ud.secret_rooms = 0;
    ud.max_secret_rooms = 0;
    ud.limit_hit = 0;
    ud.pause_on = 0;
    ud.camerasprite = -1;
    ud.eog = 0;
    ud.playerid_time = 0;

    g_numAnimWalls = 0;
    g_numCyclers = 0;
    g_animateCount = 0;
    g_earthquakeTime = 0;
    g_interpolationCnt = 0;
    g_spriteDeleteQueuePos = 0;

    for (i = 0; i < g_spriteDeleteQueueSize; i++)
        SpriteDeletionQueue[i] = -1;

    startofdynamicinterpolations = 0;
}

static void resetprestat(int snum,int g)
{
    DukePlayer_t *p = g_player[snum].ps;

    p->hbomb_on                 = 0;
    p->cheat_phase              = 0;
    p->pals.f                   = 0;
    p->toggle_key_flag          = 0;
    p->actors_killed            = 0;
    p->lastrandomspot           = 0;
    p->weapon_pos               = 6;
    p->kickback_pic             = 5;
    p->last_weapon              = -1;
    p->weapreccnt               = 0;
    p->interface_toggle_flag    = 0;
    p->show_empty_weapon        = 0;
    p->holster_weapon           = 0;
    p->last_pissed_time         = 0;
    p->one_parallax_sectnum     = -1;
    g_player[snum].display.visibility = ud.const_visibility;

    // If starting a new game, restarting a level, or we're not preserving inventory, reset weapons and inventory.
    // With MODE_RESTART not being set after refusing an offer to load a saved game,
    // MODE_EOL is checked for 1-player games. MODE_RESTART is set for Survival mode.
    if (ud.last_level == -1 ||
        (((g&MODE_EOL) != MODE_EOL) && ud.multimode < 2)
        || ((g&MODE_RESTART) == MODE_RESTART)
        || (!(Gametypes[ud.coop].flags & GAMETYPE_PRESERVEINVENTORYEOL) && ud.multimode > 1))
    {
        P_ResetWeapons(snum);
        P_ResetInventory(snum);
    }
    else if (p->curr_weapon == HANDREMOTE_WEAPON)
    {
        p->ammo_amount[HANDBOMB_WEAPON]++;
        p->curr_weapon = HANDBOMB_WEAPON;
        Gv_SetVar(sysVarIDs.WEAPON, p->curr_weapon, p->i, snum);
    }

    p->timebeforeexit   = 0;
    p->customexitsound  = 0;

    g_player[snum].fraggedby_id = g_player[snum].fragged_id = 0;
    g_player[snum].fraggedby_time = g_player[snum].fragged_time = 0;
}

static void prelevel(char g)
{
    int i, nexti, j, startwall, endwall, lotaglist;
    int lotags[MAXSPRITES];
    int switchpicnum;
    extern char ror_protectedsectors[MAXSECTORS];

    clearbufbyte(show2dsector,sizeof(show2dsector),0L);
    clearbufbyte(show2dwall,sizeof(show2dwall),0L);
    clearbufbyte(show2dsprite,sizeof(show2dsprite),0L);

    Bmemset(ror_protectedsectors, 0, MAXSECTORS);

    resetpremapglobals();

    for (ALL_PLAYERS(j))
    {
        resetprestat(j, g);
    }

    g_numClouds = 0;
    G_SetupGlobalPsky();

    X_OnEvent(EVENT_PRELEVEL, -1, -1, -1);

    for (i=0;i<numsectors;i++)
    {
        sector[i].extra = 256;

        switch (sector[i].lotag)
        {
        case ST_20_CEILING_DOOR:
        case ST_22_SPLITTING_DOOR:
            if (sector[i].floorz > sector[i].ceilingz)
                sector[i].lotag |= 32768;
            continue;
        }

        if (sector[i].ceilingstat&1)
        {
            if (sector[i].ceilingpicnum == CLOUDYSKIES && g_numClouds < ARRAY_SSIZE(g_cloudSect))
                g_cloudSect[g_numClouds++] = i;

            for (ALL_PLAYERS(j))
            {
                if (g_player[j].ps->one_parallax_sectnum == -1)
                    g_player[j].ps->one_parallax_sectnum = i;
            }
        }

        if (sector[i].lotag == 32767) //Found a secret room
        {
            ud.max_secret_rooms++;
            continue;
        }

        if (sector[i].lotag == -1)
        {
            for (ALL_PLAYERS(j))
            {
                g_player[j].ps->exitx = wall[sector[i].wallptr].x;
                g_player[j].ps->exity = wall[sector[i].wallptr].y;
            }
            continue;
        }
    }

    i = headspritestat[STAT_DEFAULT];
    while (i >= 0)
    {
        nexti = nextspritestat[i];
        A_ResetVars(i);
        A_LoadActor(i);
        X_OnEvent(EVENT_LOADACTOR, i, -1, -1);
        if (sprite[i].lotag == -1 && (sprite[i].cstat&16))
        {
            for (ALL_PLAYERS(j))
            {
                g_player[j].ps->exitx = SX(i);
                g_player[j].ps->exity = SY(i);
            }
        }
        else switch (tileGetMapping(PN(i)))
            {
            case GPSPEED__:
                sector[SECT(i)].extra = SLT(i);
                A_DeleteSprite(i);
                break;

            case CYCLER__:
                if (g_numCyclers >= MAXCYCLERS)
                {
                    Bsprintf(tempbuf,"\nToo many cycling sectors (%d max).",MAXCYCLERS);
                    G_GameExit(tempbuf);
                }
                cyclers[g_numCyclers][0] = SECT(i);
                cyclers[g_numCyclers][1] = SLT(i);
                cyclers[g_numCyclers][2] = SS(i);
                cyclers[g_numCyclers][3] = sector[SECT(i)].floorshade;
                cyclers[g_numCyclers][4] = SHT(i);
                cyclers[g_numCyclers][5] = (SA(i) == 1536);
                g_numCyclers++;
                A_DeleteSprite(i);
                break;

            case SECTOREFFECTOR__:
            case ACTIVATOR__:
            case TOUCHPLATE__:
            case ACTIVATORLOCKED__:
            case MUSICANDSFX__:
            case LOCATORS__:
            case MASTERSWITCH__:
            case RESPAWN__:
                sprite[i].cstat = 0;
                break;
            }
        i = nexti;
    }

    for (i=0;i < MAXSPRITES;i++)
    {
        if (sprite[i].statnum < MAXSTATUS)
        {
            if (PN(i) == SECTOREFFECTOR && SLT(i) == 14)
                continue;
            A_Spawn(-1,i);
        }
    }

    for (i=0;i < MAXSPRITES;i++)
        if (sprite[i].statnum < MAXSTATUS)
        {
            if (PN(i) == SECTOREFFECTOR && SLT(i) == 14)
                A_Spawn(-1,i);

            if (sprite[i].cstat & 2048)
                actor[i].flags |= SFLAG_NOSHADE;
        }

    lotaglist = 0;

    i = headspritestat[STAT_DEFAULT];
    while (i >= 0)
    {
        switch (tileGetMapping(PN(i) - 1))
        {
            case DIPSWITCH__:
            case DIPSWITCH2__:
            case PULLSWITCH__:
            case HANDSWITCH__:
            case SLOTDOOR__:
            case LIGHTSWITCH__:
            case SPACELIGHTSWITCH__:
            case SPACEDOORSWITCH__:
            case FRANKENSTINESWITCH__:
            case LIGHTSWITCH2__:
            case POWERSWITCH1__:
            case LOCKSWITCH1__:
            case POWERSWITCH2__:
                for (j = 0; j < lotaglist; j++)
                    if (SLT(i) == lotags[j])
                        break;

                if (j == lotaglist)
                {
                    lotags[lotaglist] = SLT(i);
                    lotaglist++;
                    if (lotaglist > MAXSPRITES - 1)
                        G_GameExit("\nToo many switches.");

                    j = headspritestat[STAT_EFFECTOR];
                    while (j >= 0)
                    {
                        if (sprite[j].lotag == SE_12_LIGHT_SWITCH && sprite[j].hitag == SLT(i))
                            actor[j].t_data[0] = 1;
                        j = nextspritestat[j];
                    }
                }
                break;
        }
        i = nextspritestat[i];
    }

    g_mirrorCount = 0;

    for (i = 0; i < numwalls; i++)
    {
        walltype *wal;
        wal = &wall[i];

        if (wal->overpicnum == MIRROR && (wal->cstat&32) != 0)
        {
            j = wal->nextsector;

            if (g_mirrorCount > 63)
                G_GameExit("\nToo many mirrors (64 max.)");
            if ((j >= 0) && sector[j].ceilingpicnum != MIRROR)
            {
                sector[j].ceilingpicnum = MIRROR;
                sector[j].floorpicnum = MIRROR;
                g_mirrorWall[g_mirrorCount] = i;
                g_mirrorSector[g_mirrorCount] = j;
                g_mirrorCount++;
                continue;
            }
        }

        if (g_numAnimWalls >= MAXANIMWALLS)
        {
            Bsprintf(tempbuf,"\nToo many 'anim' walls (%d max).",MAXANIMWALLS);
            G_GameExit(tempbuf);
        }

        animwall[g_numAnimWalls].tag = 0;
        animwall[g_numAnimWalls].wallnum = 0;
        switchpicnum = wal->overpicnum;
        if ((wal->overpicnum > W_FORCEFIELD)&&(wal->overpicnum <= W_FORCEFIELD+2))
        {
            switchpicnum = W_FORCEFIELD;
        }
        switch (tileGetMapping(switchpicnum))
        {
        case FANSHADOW__:
        case FANSPRITE__:
            wall->cstat |= 65;
            animwall[g_numAnimWalls].wallnum = i;
            g_numAnimWalls++;
            break;

        case W_FORCEFIELD__:
            if (wal->overpicnum==W_FORCEFIELD__)
                for (j=0;j<3;j++)
                    tloadtile(W_FORCEFIELD+j, 0);
            if (wal->shade > 31)
                wal->cstat = 0;
            else wal->cstat |= 85+256;


            if (wal->lotag && wal->nextwall >= 0)
                wall[wal->nextwall].lotag =
                    wal->lotag;
            fallthrough__;

        case BIGFORCE__:

            animwall[g_numAnimWalls].wallnum = i;
            g_numAnimWalls++;

            continue;
        }

        wal->extra = -1;

        switch (tileGetMapping(wal->picnum))
        {
        case WATERTILE2__:
            for (j=0;j<3;j++)
                tloadtile(wal->picnum+j, 0);
            break;

        case TECHLIGHT2__:
        case TECHLIGHT4__:
            tloadtile(wal->picnum, 0);
            break;
        case W_TECHWALL1__:
        case W_TECHWALL2__:
        case W_TECHWALL3__:
        case W_TECHWALL4__:
            animwall[g_numAnimWalls].wallnum = i;
            //                animwall[g_numAnimWalls].tag = -1;
            g_numAnimWalls++;
            break;
        case SCREENBREAK6__:
        case SCREENBREAK7__:
        case SCREENBREAK8__:
            for (j=SCREENBREAK6;j<SCREENBREAK9;j++)
                tloadtile(j, 0);
            animwall[g_numAnimWalls].wallnum = i;
            animwall[g_numAnimWalls].tag = -1;
            g_numAnimWalls++;
            break;

        case FEMPIC1__:
        case FEMPIC2__:
        case FEMPIC3__:

            wal->extra = wal->picnum;
            animwall[g_numAnimWalls].tag = -1;
            if (ud.lockout)
            {
                if (wal->picnum == FEMPIC1)
                    wal->picnum = BLANKSCREEN;
                else wal->picnum = SCREENBREAK6;
            }

            animwall[g_numAnimWalls].wallnum = i;
            animwall[g_numAnimWalls].tag = wal->picnum;
            g_numAnimWalls++;
            break;

        case SCREENBREAK1__:
        case SCREENBREAK2__:
        case SCREENBREAK3__:
        case SCREENBREAK4__:
        case SCREENBREAK5__:

        case SCREENBREAK9__:
        case SCREENBREAK10__:
        case SCREENBREAK11__:
        case SCREENBREAK12__:
        case SCREENBREAK13__:
        case SCREENBREAK14__:
        case SCREENBREAK15__:
        case SCREENBREAK16__:
        case SCREENBREAK17__:
        case SCREENBREAK18__:
        case SCREENBREAK19__:

            animwall[g_numAnimWalls].wallnum = i;
            animwall[g_numAnimWalls].tag = wal->picnum;
            g_numAnimWalls++;
            break;
        }
    }

    //Invalidate textures in sector behind mirror
    for (i=0;i<g_mirrorCount;i++)
    {
        startwall = sector[g_mirrorSector[i]].wallptr;
        endwall = startwall + sector[g_mirrorSector[i]].wallnum;
        for (j=startwall;j<endwall;j++)
        {
            wall[j].picnum = MIRROR;
            wall[j].overpicnum = MIRROR;
            if (wall[g_mirrorWall[i]].pal == 4)
                wall[j].pal = 4;
        }
    }
}

// Use this when you want to start a new game, or new level.
void G_NewGame(uint32_t flags)
{
    if (!Menu_HaveUserMap()) // Clear usermap if we're not going to one.
        boardfilename[0] = 0;

    if ((myconnectindex == connecthead) && !(flags & NEWGAME_NOSEND))
        Net_SendNewGame(flags);

    if ((myconnectindex != connecthead) && !(flags & (NEWGAME_FROMSERVER | NEWGAME_NOSEND)))
        return;

    if(flags & NEWGAME_RESETALL) // Clear all player inventory
        for (int32_t ALL_PLAYERS(i))
        {
            P_ResetWeapons(i);
            P_ResetInventory(i);
        }

    G_MakeNewGamePreparations(ud.m_volume_number, ud.m_level_number, ud.m_player_skill);
    if (G_EnterLevel(MODE_GAME))
        G_BackToMenu(); // Failed to enter map, back to menu.
}

// Sets necessary game variables when starting a new game, or advancing to a new map.
void G_MakeNewGamePreparations(int vn, int ln, int sk)
{
    int i,j;

    gameHandleEvents();

    if (g_skillSoundID >= 0 && ud.config.FXDevice >= 0 && ud.config.SoundToggle)
    {
        while (S_CheckSoundPlaying(g_skillSoundID))
        {
            gameHandleEvents();
            S_Cleanup();
        }
    }

    g_skillSoundID = -1;

    // Unpause music if it was paused before.
    if (ud.config.MusicToggle)
        S_PauseMusic(false);

    S_PauseSounds(false);
    S_StopAllSounds();
    S_Cleanup();

    ready2send = 0;
    Net_WaitForPlayers();

    if (ud.m_recstat != 2 && ud.last_level >= 0 && ud.multimode > 1 && (ud.coop&GAMETYPE_SCORESHEET))
        G_BonusScreen(1);

    if (ln == 0 && vn == 3 && ud.multimode < 2 && ud.lockout == 0)
    {
        S_PlaySpecialMusicOrNothing(MUS_BRIEFING);

        renderFlushPerms();
        videoSetViewableArea(0,0,xdim-1,ydim-1);
        videoClearViewableArea(0L);
        videoNextPage();

        G_PlayAnim("vol41a.anm",6);
        videoClearViewableArea(0L);
        videoNextPage();

        G_PlayAnim("vol42a.anm",7);
        G_PlayAnim("vol43a.anm",9);
        videoClearViewableArea(0L);
        videoNextPage();

        FX_StopAllSounds();
    }

    g_showShareware = 26*34;

    ud.level_number =   ln;
    ud.volume_number =  vn;
    ud.player_skill =   sk;
    ud.secretlevel =    0;
    ud.from_bonus = 0;
    ud.limit_hit = 0;

    ud.last_level = -1;
    g_lastSaveSlot = -1;

    //AddLog("Newgame");
    Gv_ResetVars();
    Gv_InitWeaponPointers();
    Gv_RefreshPointers();
    Gv_ResetSystemDefaults();

    for (i=0;i<(MAXVOLUMES*MAXLEVELS);i++)
        if (g_mapInfo[i].savedstate)
        {
            Xfree(g_mapInfo[i].savedstate);
            g_mapInfo[i].savedstate = NULL;
        }

    ud.gm = 0; // [JM] Needed?
    // Set up some player-specific info
    for (ALL_PLAYERS(j))
    {
        DukePlayer_t *p = g_player[j].ps;
        p->zoom = 768;
        if (!(Gametypes[ud.m_coop].flags & GAMETYPE_COOP)) // If gametype isn't coop
        {
            // [JM] Maybe do a P_ResetWeapons here instead?

            // Give starting weapons.
            for (i = 0; i < MAX_WEAPONS; i++)
            {
                if (aplWeaponWorksLike[i][0] == PISTOL_WEAPON)
                {
                    p->curr_weapon = i;
                    p->gotweapon[i] = 1;
                    p->ammo_amount[i] = 48;
                    Gv_SetVar(sysVarIDs.WEAPON, p->curr_weapon, p->i, 0);
                }
                else if (aplWeaponWorksLike[i][0] == KNEE_WEAPON)
                    p->gotweapon[i] = 1;
                else if (aplWeaponWorksLike[i][0] == HANDREMOTE_WEAPON)
                    p->gotweapon[i] = 1;
            }
            p->last_weapon = -1;
        }
    }

    display_mirror = 0;

    if (ud.multimode > 1)
    {
        if (numplayers < 2)
        {
            connecthead = 0;
            for (i = 0; i < MAXPLAYERS; i++)
                connectpoint2[i] = G_GetNextPlayer(i);
        }
    }
    else
    {
        connecthead = 0;
        connectpoint2[0] = -1;
    }

    X_OnEvent(EVENT_NEWGAME, g_player[screenpeek].ps->i, screenpeek, -1);
}

static void prepare_spawns(void)
{
    int i, nexti;
    spritetype *s;

    // Insert brown arrow player. Gets deleted by the next loop.
    A_InsertSprite(g_player[0].ps->cursectnum, g_player[0].ps->pos.x, g_player[0].ps->pos.y, g_player[0].ps->pos.z,
        APLAYER, 0, 0, 0, fix16_to_int(g_player[0].ps->q16ang), 0, 0, 0, 10);

    g_numPlayerSprites = 0;
    i = headspritestat[STAT_PLAYER];
    while (i >= 0)
    {
        nexti = nextspritestat[i];
        s = &sprite[i];

        if (g_numPlayerSprites == MAXSPAWNPOINTS)
        {
            LOG_F(WARNING, "Too many player sprites! (max %d.) Ignoring the rest.", MAXSPAWNPOINTS);
            break;
        }

        // Get position
        auto spawnPoint = &g_playerSpawnPoints[g_numPlayerSprites];
        spawnPoint->pos = s->xyz;
        spawnPoint->ang = s->ang;
        spawnPoint->sectnum = s->sectnum;
        spawnPoint->pal = s->pal;
        g_numPlayerSprites++;

        A_DeleteSprite(i); // Purge the APLAYER sprite, we don't need it anymore.
        i = nexti;
    }
}

static void insert_player_sprite(int pNum)
{
    auto const spawnPoint = &g_playerSpawnPoints[pNum % g_numPlayerSprites]; // Wrap spawn point index if more players than spawns.

    int32_t spriteIndex = A_InsertSprite(spawnPoint->sectnum, spawnPoint->pos.x, spawnPoint->pos.y, spawnPoint->pos.z,
        APLAYER, 0, 0, 0, spawnPoint->ang, 0, 0, 0, 10);

    spritetype* s = &sprite[spriteIndex];
    auto p = g_player[pNum].ps;

    s->owner = spriteIndex;
    s->pal = p->palookup = spawnPoint->pal;
    s->shade = 0;
    s->xrepeat = PLAYER_SPRITE_XREPEAT;
    s->yrepeat = PLAYER_SPRITE_YREPEAT;
    s->cstat = 1 + 256;
    s->xoffset = 0;
    s->clipdist = 64;

    // Either reset health to max if starting an episode, inventory isn't to be preserved, or was dead in the last level.
    if ((ud.last_level == -1) || (!(Gametypes[ud.coop].flags & GAMETYPE_PRESERVEINVENTORYEOL) && ud.multimode > 1) || p->last_extra <= 0)
    {
        p->last_extra = p->max_player_health;
        s->extra = p->max_player_health;
        p->runspeed = g_playerFriction;
    }
    else s->extra = p->last_extra; // Use health from previous level.

    // Set sprite yvel to player index
    s->yvel = pNum;

    // Find a palette that doesn't match another player if color set to auto (0) and not in TDM.
    if ((g_player[pNum].pcolor == 0) && ud.multimode > 1 && !(Gametypes[ud.coop].flags & GAMETYPE_TDM))
    {
        if (s->pal == 0) // Spawn point isn't colored.
        {
            int colorfound = 0;
            int currentpal = playerColor_getNextColor(0, 1);

            // Iterate through all colors
            while (!colorfound)
            {
                for (int k = 0; k <= MAXPLAYERS; k++)
                {
                    // Iterated through all players and hit nothing, our color is found.
                    if (k == MAXPLAYERS)
                    {
                        colorfound = 1;
                        g_lastPlayerPal = currentpal;
                        break;
                    }

                    // Disregard non-existent or disconnected players.
                    if (g_player[k].ps == NULL || !g_player[k].connected) continue;

                    if (currentpal == g_player[k].ps->palookup)
                    {
                        // Color used, advance to the next color.
                        currentpal = playerColor_getNextColor(currentpal, 1);
                        break;
                    }
                }
                
                if (currentpal == 0) break;
            }

            if (!colorfound) // We hit our limit, let's just cycle through all the colors, starting from the last used.
            {
                currentpal = playerColor_getNextColor(g_lastPlayerPal, 0);
                g_lastPlayerPal = currentpal;
            }
            
            s->pal = p->palookup = currentpal;
        }
        else
        {
            p->palookup = s->pal; // Take on color of spawn point.
        }
    }
    else
    {
        int k = g_player[pNum].pcolor;

        if (Gametypes[ud.coop].flags & GAMETYPE_TDM)
        {
            k = teams[g_player[pNum].pteam].palnum;
            p->team = g_player[pNum].pteam;
        }
        s->pal = p->palookup = k;
    }

    p->i = spriteIndex;
    p->frag_ps = pNum;
    actor[spriteIndex].htowner = spriteIndex;

    actor[spriteIndex].bpos.x = p->bobposx = p->opos.x = p->pos.x = s->x;
    actor[spriteIndex].bpos.y = p->bobposy = p->opos.y = p->pos.y = s->y;
    actor[spriteIndex].bpos.z = p->opos.z = p->pos.z = s->z;
    p->oq16ang = p->q16ang = fix16_from_int(s->ang);

    p->cursectnum = s->sectnum;
}

static void resetpspritevars(char g)
{
    short i;
    DukeStatus_t tsbar[MAXPLAYERS];

    UNREFERENCED_PARAMETER(g);

    if (ud.recstat != 2)
    {
        for(ALL_PLAYERS(i))
        {
            // Kept to keep old behavior. Rest of the block that was here was useless.
            if (ud.multimode > 1 && (Gametypes[ud.coop].flags & GAMETYPE_PRESERVEINVENTORYEOL) && ud.last_level >= 0)
            {
                tsbar[i].inven_icon = g_player[i].ps->inven_icon;
                tsbar[i].inv_amount[GET_STEROIDS] = g_player[i].ps->inv_amount[GET_STEROIDS];
            }

            // Reset status for player
            P_ResetStatus(i);

            // Kept to keep old behavior. Rest of the block that was here was useless.
            if (ud.multimode > 1 && (Gametypes[ud.coop].flags & GAMETYPE_PRESERVEINVENTORYEOL) && ud.last_level >= 0)
            {
                g_player[i].ps->inven_icon = tsbar[i].inven_icon;
                g_player[i].ps->inv_amount[GET_STEROIDS] = tsbar[i].inv_amount[GET_STEROIDS];
            }
        }
    }

    // Prepare spawn points
    prepare_spawns();
    
    g_lastPlayerPal = 0;
    for(ALL_PLAYERS(i))
    {
        insert_player_sprite(i);
    }
}

void G_ResetTimers(void)
{
    totalclock = cloudtotalclock = ototalclock = lockclock = 0;
    ready2send = 1;
    g_levelTextTime = 85;
    g_moveThingsCount = 0;
}

void clearfifo(void)
{
    int i = 0;

    // Black-hole any leftover inputs.
    // Prevents being spun around at map start if you move the mouse while loading.
    ControlInfo blackHole;
    CONTROL_GetInput(&blackHole);

    Net_ClearFIFO();

    localInput = {};
    Bmemset(&inputfifo, 0, sizeof(inputfifo));

    for (ALL_PLAYERS(i))
    {
        if (g_player[i].inputBits != NULL)
            Bmemset(g_player[i].inputBits,0,sizeof(input_t));

        g_player[i].gotvote = 0;
    }
}

int32_t G_FindLevelByFile(const char *fn)
{
    int32_t volume, level;

    for (volume=0; volume<MAXVOLUMES; volume++)
    {
        for (level=0; level<MAXLEVELS; level++)
        {
            if (g_mapInfo[(volume*MAXLEVELS)+level].filename != NULL)
                if (!Bstrcasecmp(fn, g_mapInfo[(volume*MAXLEVELS)+level].filename))
                    return ((volume * MAXLEVELS) + level);
        }
    }
    return MAXLEVELS*MAXVOLUMES;
}

static int G_TryMapHack(const char* mhkfile)
{
    int const failure = engineLoadMHK(mhkfile);

    if (!failure)
        LOG_F(INFO, "Loaded map hack file \"%s\"", mhkfile);

    return failure;
}

static void G_LoadMapHack(char* outbuf, const char* filename)
{
    if (filename != NULL)
        Bstrcpy(outbuf, filename);

    append_ext_UNSAFE(outbuf, ".mhk");

    if (G_TryMapHack(outbuf) && usermaphacks != NULL)
    {
        auto pMapInfo = (usermaphack_t*)bsearch(&g_loadedMapHack, usermaphacks, num_usermaphacks,
            sizeof(usermaphack_t), compare_usermaphacks);
        if (pMapInfo)
            G_TryMapHack(pMapInfo->mhkfile);
    }
}

// levnamebuf should have at least size BMAX_PATH
void G_SetupFilenameBasedMusic(char* nameBuf, const char* fileName)
{
    char* p;
    char const* exts[] = {
#ifdef HAVE_FLAC
        "flac",
#endif
#ifdef HAVE_VORBIS
        "ogg",
#endif
#ifdef HAVE_XMP
        "xm",
        "mod",
        "it",
        "s3m",
        "mtm",
#endif
        "mid"
    };

    Bstrncpy(nameBuf, fileName, BMAX_PATH);

    Bcorrectfilename(nameBuf, 0);

    if (NULL == (p = Bstrrchr(nameBuf, '.')))
    {
        p = nameBuf + Bstrlen(nameBuf);
        p[0] = '.';
    }

    for (auto& ext : exts)
    {
        buildvfs_kfd kFile;

        Bmemcpy(p + 1, ext, Bstrlen(ext) + 1);

        if ((kFile = kopen4loadfrommod(nameBuf, 0)) != buildvfs_kfd_invalid)
        {
            kclose(kFile);
            realloc_copy(&g_mapInfo[USERMAPMUSICFAKESLOT].musicfn, nameBuf);
            return;
        }
    }

    char const* usermapMusic = g_mapInfo[MUS_USERMAP].musicfn;
    if (usermapMusic != nullptr)
    {
        realloc_copy(&g_mapInfo[USERMAPMUSICFAKESLOT].musicfn, usermapMusic);
        return;
    }

    char const* e1l8 = g_mapInfo[7].musicfn;
    if (e1l8 != nullptr)
    {
        realloc_copy(&g_mapInfo[USERMAPMUSICFAKESLOT].musicfn, e1l8);
        return;
    }
}

int G_EnterLevel(int g)
{
    int i;

    // G_ApplyMenuToGameFlags?
    // ---------------------------------
    vote = votedata_t();

    if ((g & MODE_DEMO) != MODE_DEMO)
    {
        ud.recstat = (ud.m_recstat == 2) ? 0 : ud.m_recstat;
    }

    ud.coop = ud.m_coop;
    ud.dmflags = ud.m_dmflags;

    if (ud.player_skill > 3)
        DMFLAGS_SET(DMFLAG_RESPAWNMONSTERS, true);

    if (!GTFLAGS(GAMETYPE_PLAYERSFRIENDLY | GAMETYPE_TDM))
        DMFLAGS_SET(DMFLAG_FRIENDLYFIRE, false);

    if (!GTFLAGS(GAMETYPE_ALLOWITEMRESPAWN))
        DMFLAGS_SET(DMFLAG_RESPAWNITEMS | DMFLAG_RESPAWNINVENTORY, false);

    if (!GTFLAGS(GAMETYPE_MARKEROPTION))
        DMFLAGS_SET(DMFLAG_MARKERS, false);

    if(GTFLAGS(GAMETYPE_COOP))
        DMFLAGS_SET(DMFLAG_NOEXITS, false);

    if (!GTFLAGS(GAMETYPE_COOP))
        DMFLAGS_SET(DMFLAG_NODMWEAPONSPAWNS, false);

    if (DMFLAGS_TEST(DMFLAG_WEAPONSTAY)) // [JM] Needed this hack for backwards compat.
        Gametypes[ud.coop].flags |= GAMETYPE_WEAPSTAY;
    else
        Gametypes[ud.coop].flags &= ~(GAMETYPE_WEAPSTAY);

    Gv_RefreshPointers();
    // ---------------------------------

    S_PauseSounds(false);
    S_StopAllSounds();
    FX_SetReverb(0);

    if (boardfilename[0] != 0 && ud.m_level_number == 7 && ud.m_volume_number == 0)
    {
        int32_t volume, level;

        Bcorrectfilename(boardfilename,0);

        volume = level = G_FindLevelByFile(boardfilename);

        if (level != MAXLEVELS*MAXVOLUMES)
        {
            level &= MAXLEVELS-1;
            volume = (volume - level) / MAXLEVELS;

            ud.level_number = ud.m_level_number = level;
            ud.volume_number = ud.m_volume_number = volume;
            boardfilename[0] = 0;
        }
    }

    int const mapidx = (ud.volume_number*MAXLEVELS)+ud.level_number;

    Bassert((unsigned)mapidx < ARRAY_SIZE(g_mapInfo));

    auto& m = g_mapInfo[mapidx];

    if (VOLUMEONE || !Menu_HaveUserMap())
    {
        if (m.name == NULL || m.filename == NULL)
        {
            OSD_Printf(OSDTEXT_RED "Map E%dL%d not defined!\n", ud.volume_number + 1, ud.level_number + 1);
            return 1;
        }
    }

#ifdef POLYMER
    G_DeleteAllLights();
#endif

    i = ud.screen_size;
    ud.screen_size = 0;
    G_DoLoadScreen(NULL, -1);
    G_UpdateScreenArea();
    ud.screen_size = i;

    G_UpdateAppTitle(Menu_HaveUserMap() ? boardfilename : m.name);

    auto& p0 = *g_player[0].ps;
    int16_t playerAngle;

    char levelName[BMAX_PATH];

    if (!VOLUMEONE && G_HaveUserMap())
    {
        if (engineLoadBoard(boardfilename, 0, &p0.pos, &playerAngle, &p0.cursectnum) < 0)
        {
            OSD_Printf(OSD_ERROR "Map \"%s\" not found or invalid map version!\n", boardfilename);
            return 1;
        }

        G_LoadMapHack(levelName, boardfilename);
        G_SetupFilenameBasedMusic(levelName, boardfilename);
    }
    else if (engineLoadBoard(m.filename, VOLUMEONE, &p0.pos, &playerAngle, &p0.cursectnum) < 0)
    {
        OSD_Printf(OSD_ERROR "Map \"%s\" not found or invalid map version!\n", m.filename);
        return 1;
    }
    else
    {
        G_LoadMapHack(levelName, m.filename);
    }

    g_player[0].ps->q16ang = fix16_from_int(playerAngle);

    g_precacheCount = 0;
    clearbufbyte(gotpic,sizeof(gotpic),0L);
    clearbufbyte(precachehightile, sizeof(precachehightile), 0l);
    //clearbufbyte(actor,sizeof(actor),0l); // JBF 20040531: yes? no?

    prelevel(g);
    allignwarpelevators();
    resetpspritevars(g);

    //cachedebug = 0;
    automapping = 0;

    ud.playerbest = CONFIG_GetMapBestTime(Menu_HaveUserMap() ? boardfilename : m.filename, g_loadedMapHack.md4);

    G_CacheMapData();

    if ((g&MODE_GAME) || (g&MODE_EOL))
        ud.gm = MODE_GAME;
    else if (g&MODE_RESTART)
    {
        if (ud.recstat == 2)
            ud.gm = MODE_DEMO;
        else ud.gm = MODE_GAME;
    }

    if ((ud.recstat == 1) && (g&MODE_RESTART) != MODE_RESTART)
        G_OpenDemoWrite();

    if (VOLUMEONE)
    {
        if (ud.level_number == 0 && ud.recstat != 2) P_DoQuote(QUOTE_F1HELP,g_player[myconnectindex].ps);
    }

    for (ALL_PLAYERS(i))
    switch (tileGetMapping(sector[sprite[g_player[i].ps->i].sectnum].floorpicnum))
    {
    case HURTRAIL__:
    case FLOORSLIME__:
    case FLOORPLASMA__:
        P_ResetWeapons(i);
        P_ResetInventory(i);
        g_player[i].ps->gotweapon[PISTOL_WEAPON] = 0;
        g_player[i].ps->ammo_amount[PISTOL_WEAPON] = 0;
        g_player[i].ps->curr_weapon = KNEE_WEAPON;
        g_player[i].ps->kickback_pic = 0;
        Gv_SetVar(sysVarIDs.WEAPON, g_player[i].ps->curr_weapon, g_player[i].ps->i, i);
        break;
    }

    //PREMAP.C - replace near the my's at the end of the file

    //g_player[myconnectindex].ps->palette = palette;
    //G_FadePalette(0,0,0,0);
    P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 0);    // JBF 20040308

    P_UpdateScreenPal(g_player[myconnectindex].ps);
    renderFlushPerms();

    everyothertime = 0;
    g_globalRandom = 0;

    ud.last_level = ud.level_number+1;

    G_BackupInterpolations();

    g_restorePalette = 1;

    // NEED these just before starting, after setting up everything else.
    clearfifo();
    G_ClearGametypeStats();
    DukeBot_ResetAll();

    Net_WaitForPlayers();

    G_FadePalette(0,0,0,0);
    G_UpdateScreenArea();
    videoClearViewableArea(0L);
    G_DrawBackground();
    G_DrawRooms(myconnectindex,65536);

    g_player[myconnectindex].ps->over_shoulder_on = 0;

    G_ResetTimers();  // Here we go

    if (ud.recstat != 2)
    {
        if (Menu_HaveUserMap())
        {
            S_PlayLevelMusicOrNothing(USERMAPMUSICFAKESLOT);
        }
        else if (g_mapInfo[g_musicIndex].musicfn == NULL || m.musicfn == NULL ||
            strcmp(g_mapInfo[g_musicIndex].musicfn, m.musicfn) || g_musicSize == 0 || ud.last_level == -1)
        {
            S_PlayLevelMusicOrNothing(mapidx);
        }
        else
        {
            S_ContinueLevelMusic();
        }
    }

    //Bsprintf(g_szBuf,"G_EnterLevel L=%d V=%d",ud.level_number, ud.volume_number);
    //AddLog(g_szBuf);
    // variables are set by pointer...

    Bmemcpy(&currentboardfilename[0],&boardfilename[0],BMAX_PATH);

    for (int ALL_PLAYERS(i))
    {
        if (!X_OnEventWithReturn(EVENT_ENTERLEVEL, g_player[i].ps->i, i, 0))
            break;
    }

    OSD_Printf(OSDTEXT_YELLOW "E%dL%d: %s\n",ud.volume_number+1,ud.level_number+1,m.name);
    return 0;
}

void G_FreeMapState(int mapnum)
{
    int j;

    for (j=0;j<g_gameVarCount;j++)
    {
        if (aGameVars[j].flags & GAMEVAR_NORESET) continue;
        if (aGameVars[j].flags & GAMEVAR_PERPLAYER)
        {
            if (g_mapInfo[mapnum].savedstate->vars[j])
                Xfree(g_mapInfo[mapnum].savedstate->vars[j]);
        }
        else if (aGameVars[j].flags & GAMEVAR_PERACTOR)
        {
            if (g_mapInfo[mapnum].savedstate->vars[j])
                Xfree(g_mapInfo[mapnum].savedstate->vars[j]);
        }
    }
    Xfree(g_mapInfo[mapnum].savedstate);
    g_mapInfo[mapnum].savedstate = NULL;
}

