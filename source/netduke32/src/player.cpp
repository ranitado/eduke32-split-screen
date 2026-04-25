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

// Savage Baggage Masters

#include "duke3d.h"
#include "osd.h"
#include "gamedef.h"

int g_currentweapon;
int g_gun_pos;
int g_looking_arc;
int g_weapon_offset;
int g_gs;
int g_kb;
int g_looking_angSR1;
int g_weapon_xoffset;

playerdata_t g_player[MAXPLAYERS];

uint8_t allowedColors[(MAXPALOOKUPS + CHAR_BIT - 1) / CHAR_BIT];
char pColorNames[MAXPALOOKUPS][32] = { "Auto" };

void playerColor_init(void)
{
    LOG_F(INFO, "Player Color INIT:\n");
    playerColor_add("Blue", 9);
    playerColor_add("Red", 10);
    playerColor_add("Green", 11);
    playerColor_add("Gray", 12);
    playerColor_add("Dark gray", 13);
    playerColor_add("Dark green", 14);
    playerColor_add("Brown", 15);
    playerColor_add("Dark blue", 16);
    playerColor_add("Bright red", 21);
    playerColor_add("Yellow", 23);
}

uint8_t playerColor_getNextColor(uint8_t palnum, uint8_t useauto)
{
    int seek = palnum;
    while (1)
    {
        seek++;
        if (seek >= MAXPALOOKUPS)
        {
            seek = 0;
        }

        if (TEST_ALLOWEDCOLOR(seek) || (useauto && seek == 0))
        {
            break;
        }
    }

    return (uint8_t)seek;
}

uint8_t playerColor_getPreviousColor(uint8_t palnum, uint8_t useauto)
{
    int seek = palnum;
    while (1)
    {
        seek--;
        if (seek < 0)
        {
            seek = MAXPALOOKUPS - 1;
        }

        if (TEST_ALLOWEDCOLOR(seek) || (useauto && seek == 0))
        {
            break;
        }
    }

    return (uint8_t)seek;
}

void playerColor_add(const char *name, int palnum)
{
    if (palnum <= 0 || palnum >= MAXPALOOKUPS)
    {
        LOG_F(ERROR, "Tried to add an invalid player pal: %d", palnum);
        return;
    }

    Bsprintf(pColorNames[palnum], "%s", name);
    SET_ALLOWEDCOLOR(palnum);

    LOG_F(INFO, "Added color %s (%d)", name, palnum);
}

void playerColor_delete(int palnum)
{
    int i;

    if (palnum == 0 || palnum >= MAXPALOOKUPS)
    {
        LOG_F(ERROR, "Tried to delete an invalid player pal: %d", palnum);
        return;
    }

    if (palnum > 0)
    {
        RESET_ALLOWEDCOLOR(palnum);
    }
    else if (palnum < 0) // Negative number, delete all.
    {
        for (i = 1; i < MAXPALOOKUPS; i++)
        {
            RESET_ALLOWEDCOLOR(i);
            memset(pColorNames[i], 0, sizeof(pColorNames[0]));
        }
    }
}

void P_UpdateScreenPal(DukePlayer_t *p)
{
    if (p->heat_on)
        p->palette = SLIMEPAL;
    else if (p->cursectnum < 0)
        p->palette = BASEPAL;
    else if ((sector[p->cursectnum].ceilingpicnum >= FLOORSLIME) && (sector[p->cursectnum].ceilingpicnum <= FLOORSLIME + 2))
        p->palette = SLIMEPAL;
    else
        p->palette = (sector[p->cursectnum].lotag == ST_2_UNDERWATER) ? WATERPAL : BASEPAL;

    g_restorePalette = 1;
}

static void P_IncurDamage(DukePlayer_t *p)
{
    int damage = 0L, shield_damage = 0L;

    Gv_SetVar(sysVarIDs.RETURN,0,p->i,sprite[p->i].yvel);
    X_OnEvent(EVENT_INCURDAMAGE, p->i, sprite[p->i].yvel, -1);

    if (Gv_GetVar(sysVarIDs.RETURN,p->i,sprite[p->i].yvel) == 0)

    {
        sprite[p->i].extra -= p->extra_extra8>>8;

        damage = sprite[p->i].extra - p->last_extra;

        if (damage < 0)
        {
            p->extra_extra8 = 0;

            if (p->inv_amount[GET_SHIELD] > 0)
            {
                shield_damage =  damage * (20 + (krand()%30)) / 100;
                damage -= shield_damage;

                p->inv_amount[GET_SHIELD] += shield_damage;

                if (p->inv_amount[GET_SHIELD] < 0)
                {
                    damage += p->inv_amount[GET_SHIELD];
                    p->inv_amount[GET_SHIELD] = 0;
                }
            }

            sprite[p->i].extra = p->last_extra + damage;
        }

    }

}

void P_QuickKill(DukePlayer_t *p)
{
    UNPREDICTABLE_FUNCTION;
    P_PalFrom(p, 48, 48, 48, 48);

    sprite[p->i].extra = 0;
    sprite[p->i].cstat |= 32768;
    if (ud.god == 0)
        A_DoGuts(p->i,JIBS6,8);
    return;
}

int P_FindOtherPlayer(int playerNum, int32_t* pDist)
{
    int closestPlayer = playerNum;
    int closestPlayerDist = INT32_MAX;

    for (int32_t ALL_PLAYERS(otherPlayer))
    {
        if (playerNum != otherPlayer && sprite[g_player[otherPlayer].ps->i].extra > 0)
        {
            int otherPlayerDist = klabs(g_player[otherPlayer].ps->opos.x - g_player[playerNum].ps->pos.x) +
                                  klabs(g_player[otherPlayer].ps->opos.y - g_player[playerNum].ps->pos.y) +
                                 (klabs(g_player[otherPlayer].ps->opos.z - g_player[playerNum].ps->pos.z) >> 4);

            if (otherPlayerDist < closestPlayerDist)
            {
                closestPlayer = otherPlayer;
                closestPlayerDist = otherPlayerDist;
            }
        }
    }

    *pDist = closestPlayerDist;

    return closestPlayer;
}

int A_GetHitscanRange(int i)
{
    hitdata_t hit;
    int zoff = 0;

    if (PN(i) == APLAYER) zoff = (40<<8);

    vec3_t origin;
    origin = sprite[i].xyz;
    origin.z -= zoff;

    hitscan(&origin,SECT(i),
            sintable[(SA(i)+512)&2047],
            sintable[SA(i)&2047],
            0,&hit,CLIPMASK1);

    return (FindDistance2D(hit.x-origin.x,hit.y-origin.y));
}

static vec2_t P_GetDisplayBaseXY(DukePlayer_t const* const pPlayer)
{
    return vec2_t
    {
        (fix16_to_int(g_player[screenpeek].inputBits->q16avel) >> 5) - (pPlayer->look_ang >> 1),
        (klabs(pPlayer->look_ang) / 9) - (pPlayer->hard_landing << 3) - (fix16_to_int(pPlayer->q16horiz - pPlayer->q16horizoff) >> 4),
    };
}

static void P_DisplaySpitAnim(int snum)
{
    auto const pPlayer = g_player[snum].ps;
    int const  loogCounter = pPlayer->loogcnt;

    if (loogCounter == 0)
        return;

    int const rotY = loogCounter << 2;
    for (int i = 0; i < pPlayer->numloogs; i++)
    {
        int const rotAng = klabs(sintable[((loogCounter + i) << 5) & 2047]) >> 5;
        int const rotZoom = 4096 + ((loogCounter + i) << 9);
        int const rotX = (-fix16_to_int(g_player[snum].inputBits->q16avel) >> 1) + (sintable[((loogCounter + i) << 6) & 2047] >> 10);

        rotatesprite_fs_id((pPlayer->loogiex[i] + rotX) << 16, (200 + pPlayer->loogiey[i] - rotY) << 16, rotZoom - (i << 8),
            256 - rotAng, LOOGIE, 0, 0, 2, 16 + i);
    }
}

static int P_DisplayFistAnim(int gs,int snum)
{
    DukePlayer_t const* const pPlayer = g_player[snum].ps;
    int fistInc = pPlayer->fist_incs;

    if (fistInc > 32)
        fistInc = 32;

    if (fistInc <= 0)
        return 0;

    auto const base = P_GetDisplayBaseXY(pPlayer);
    int const fistX = 222 + base.x - fistInc;
    int const fistY = 194 + base.y + (sintable[((6 + fistInc) << 7) & 2047] >> 9);
    int const fistZoom = clamp(65536 - (sintable[(512 + (fistInc << 6)) & 2047] << 2), 40920, 90612);
    int const fistPal = P_GetHudPal(pPlayer);
    int       wx[2] = { windowxy1.x, windowxy2.x };
    int const wy[2] = { windowxy1.y, windowxy2.y };

    guniqhudid = W_FIST;
    rotatesprite(fistX << 16, fistY << 16, fistZoom, 0, FIST, gs, fistPal, RS_AUTO | RS_LERP, wx[0], wy[0], wx[1], wy[1]);
    guniqhudid = 0;

    return 1;
}

#define DRAWEAP_CENTER 262144
#define weapsc(sc) scale(sc,ud.weaponscale,100)

static void G_DrawTileScaled(int x, int y, int tilenum, int shade, int orientation, int p, int uniqueID = 0)
{
    int a = 0;
    int xoff = 192;
    int const restoreid = guniqhudid;

    guniqhudid = uniqueID;

    switch (g_currentweapon)
    {
    case DEVISTATOR_WEAPON:
    case TRIPBOMB_WEAPON:
        xoff = 160;
        break;
    default:
        if (orientation & 262144)
        {
            xoff = 160;
            orientation &= ~262144;
        }
        break;
    }

    if (orientation&4)
        a = 1024;

#if defined(USE_OPENGL)
    if (videoGetRenderMode() >= 3 && usemodels && md_tilehasmodel(tilenum,p) > 0)
        y += (224-weapsc(224));
#endif
    rotatesprite(weapsc((orientation & 1024) ? x : (x << 16)) + ((xoff - weapsc(xoff)) << 16),
        weapsc((orientation & 1024) ? y : (y << 16)) + ((200 - weapsc(200)) << 16),
        weapsc(65536L), a, tilenum, shade, p, (RS_AUTO | orientation), windowxy1.x, windowxy1.y, windowxy2.x, windowxy2.y);

    guniqhudid = restoreid;
}

static void G_DrawWeaponTile(int x, int y, int tilenum, int shade, int orientation, int p, int uniqueID = 0)
{
    if (!ud.drawweapon)
        return;
    else if (ud.drawweapon == 1)
        G_DrawTileScaled(x,y,tilenum,shade,orientation,p,uniqueID);
    else if (ud.drawweapon == 2)
    {
        switch (g_currentweapon)
        {
        case PISTOL_WEAPON:
        case CHAINGUN_WEAPON:
        case RPG_WEAPON:
        case FREEZE_WEAPON:
        case SHRINKER_WEAPON:
        case GROW_WEAPON:
        case DEVISTATOR_WEAPON:
        case TRIPBOMB_WEAPON:
        case HANDREMOTE_WEAPON:
        case HANDBOMB_WEAPON:
        case SHOTGUN_WEAPON:
            rotatesprite(160<<16,(180+(g_player[screenpeek].ps->weapon_pos*g_player[screenpeek].ps->weapon_pos))<<16,scale(65536,ud.statusbarscale,100),0,g_currentweapon==GROW_WEAPON?GROWSPRITEICON:WeaponPickupSprites[g_currentweapon],0,0,RS_AUTO,windowxy1.x,windowxy1.y,windowxy2.x,windowxy2.y);
            break;
        }
    }
}

static inline void G_DrawWeaponTileUnfaded(int weaponX, int weaponY, int weaponTile, int weaponShade, int weaponBits, int p, int uniqueID = 0)
{
    G_DrawTileScaled(weaponX, weaponY, weaponTile, weaponShade, weaponBits, p, uniqueID); // skip G_DrawWeaponTile
}

int P_GetHudPal(const DukePlayer_t* pPlayer)
{
    if (sprite[pPlayer->i].pal == 1)
        return 1;

    if (pPlayer->cursectnum >= 0)
    {
        int const hudPal = sector[pPlayer->cursectnum].floorpal;
        if (!g_noFloorPal[hudPal])
            return hudPal;
    }

    return 0;
}

int P_GetKneePal(DukePlayer_t const* pPlayer)
{
    int hudPal = P_GetHudPal(pPlayer);
    return hudPal == 0 ? pPlayer->palookup : hudPal;
}

static int P_DisplayKneeAnim(int gs,int snum)
{
    static int8_t const knee_y[] = {0,-8,-16,-32,-64,-84,-108,-108,-108,-72,-32,-8};
    auto const ps = g_player[snum].ps;

    if (ps->knee_incs == 0)
        return 0;

    // Event

    if (ps->knee_incs >= ARRAY_SIZE(knee_y) || sprite[ps->i].extra <= 0)
        return 0;

    auto const base = P_GetDisplayBaseXY(ps);
    int const kneeX = 105 + base.x + (knee_y[ps->knee_incs] >> 2);
    int const kneeY = 280 + base.y + knee_y[ps->knee_incs];
    int const kneePal = P_GetKneePal(ps);

    G_DrawTileScaled(kneeX, kneeY, KNEE, gs, 4 | DRAWEAP_CENTER | RS_LERP, kneePal, W_KNEE);

    return 1;
}

static int P_DisplayKnuckleAnim(int gs,int snum)
{
    if (WW2GI)
        return 0;

    static char knuckleFrames[] = {0,1,2,2,3,3,3,2,2,1,0};
    auto const pPlayer = g_player[snum].ps;

    if (pPlayer->knuckle_incs == 0)
        return 0;

    if ((unsigned)(pPlayer->knuckle_incs >> 1) >= ARRAY_SIZE(knuckleFrames) || sprite[pPlayer->i].extra <= 0)
        return 0;

    auto const base = P_GetDisplayBaseXY(pPlayer);
    int const knuckleX = 160 + base.x;
    int const knuckleY = 180 + base.y;
    int const knuckleTile = CRACKKNUCKLES + knuckleFrames[pPlayer->knuckle_incs >> 1];
    int const knucklePal = P_GetHudPal(pPlayer);

    G_DrawTileScaled(knuckleX, knuckleY, knuckleTile, gs, RS_YFLIP | DRAWEAP_CENTER | RS_LERP, knucklePal, W_KNUCKLES);

    return 1;
}

void P_SetWeaponGamevars(int playerNum, const DukePlayer_t* const pPlayer)
{
    Gv_SetVar(sysVarIDs.WEAPON, pPlayer->curr_weapon, pPlayer->i, playerNum);
    Gv_SetVar(sysVarIDs.WORKSLIKE,
        ((unsigned)pPlayer->curr_weapon < MAX_WEAPONS) ? PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike) : -1,
        pPlayer->i, playerNum);
}

void P_FireWeapon(DukePlayer_t *p)
{
    int i, snum = sprite[p->i].yvel;

    Gv_SetVar(sysVarIDs.RETURN,0,p->i,snum);
    X_OnEvent(EVENT_DOFIRE, p->i, snum, -1);

    if (Gv_GetVar(sysVarIDs.RETURN,p->i,snum) == 0)
    {
        if (p->weapon_pos != 0) return;

        if (aplWeaponWorksLike[p->curr_weapon][snum]!=KNEE_WEAPON)
            p->ammo_amount[p->curr_weapon]--;

        if (aplWeaponFireSound[p->curr_weapon][snum] > 0)
        {
            A_PlaySound(aplWeaponFireSound[p->curr_weapon][snum],p->i);
        }

        P_SetWeaponGamevars(snum, p);

        if (X_OnEventWithReturn(EVENT_PREWEAPONSHOOT, p->i, snum, 0) == 0)
        {
            A_Shoot(p->i, aplWeaponShoots[p->curr_weapon][snum]);
            X_OnEvent(EVENT_POSTWEAPONSHOOT, p->i, snum, -1);
        }

        for (i=1;i<aplWeaponShotsPerBurst[p->curr_weapon][snum];i++)
        {
            if (aplWeaponFlags[p->curr_weapon][snum] & WEAPON_FIREEVERYOTHER)
            {
                // this makes the projectiles fire on a delay from player code
                actor[p->i].t_data[7] = (aplWeaponShotsPerBurst[p->curr_weapon][snum])<<1;
            }
            else if (X_OnEventWithReturn(EVENT_PREWEAPONSHOOT, p->i, snum, 0) == 0)
            {
                if (aplWeaponFlags[p->curr_weapon][snum] & WEAPON_AMMOPERSHOT)
                {
                    if (p->ammo_amount[p->curr_weapon] > 0)
                        p->ammo_amount[p->curr_weapon]--;
                    else break;
                }
                A_Shoot(p->i,aplWeaponShoots[p->curr_weapon][snum]);
                X_OnEvent(EVENT_POSTWEAPONSHOOT, p->i, snum, -1);
            }
        }

        if (!(aplWeaponFlags[p->curr_weapon][snum] & WEAPON_NOVISIBLE))
        {
            if (Net_InPredictableState())
            {
#ifdef POLYMER
                auto s = (uspriteptr_t)&sprite[p->i];
                vec3_t const offset = { -((sintable[(s->ang + 512) & 2047]) >> 7), -((sintable[(s->ang) & 2047]) >> 7), PHEIGHT };
                G_AddGameLight(p->i, p->cursectnum, offset, 8192, 0, 100, PWEAPON(snum, p->curr_weapon, FlashColor), PR_LIGHT_PRIO_MAX_GAME);
                practor[p->i].lightcount = 2;
#endif  // POLYMER

                g_player[snum].display.visibility = 0;
            }
        }
        if (WW2GI)
        {
            //Check for reload every shot for burst weapons
            if (/*!(aplWeaponFlags[p->curr_weapon][snum] & WEAPON_CHECKATRELOAD) && */ p->reloading == 1 ||
                    (aplWeaponReload[p->curr_weapon][snum] > aplWeaponTotalTime[p->curr_weapon][snum] && p->ammo_amount[p->curr_weapon] > 0
                        && (aplWeaponClip[p->curr_weapon][snum]) && (((p->ammo_amount[p->curr_weapon] % (aplWeaponClip[p->curr_weapon][snum])) == 0))))
            {
                p->kickback_pic = aplWeaponTotalTime[p->curr_weapon][snum];
            }
        }
    }
}

static void P_DoWeaponSpawn(int playerNum)
{
    UNPREDICTABLE_FUNCTION;

    auto const pPlayer = g_player[playerNum].ps;

    // NOTE: For the 'Spawn' member, 0 means 'none', too (originally so,
    // i.e. legacy). The check for <0 was added to the check because mod
    // authors (rightly) assumed that -1 is the no-op value.
    if (PWEAPON(playerNum, pPlayer->curr_weapon, Spawn) <= 0)  // <=0 : AMC TC beta/RC2 has WEAPONx_SPAWN -1
        return;

    int newSprite = A_Spawn(pPlayer->i, PWEAPON(playerNum, pPlayer->curr_weapon, Spawn));

    if ((PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_SPAWNTYPE3))
    {
        // like chaingun shells
        sprite[newSprite].ang += 1024;
        sprite[newSprite].ang &= 2047;
        sprite[newSprite].xvel += 32;
        sprite[newSprite].z += (3 << 8);
    }

    A_SetSprite(newSprite, CLIPMASK0);
}
void P_DisplayScubaMask(int snum)
{
    int p;

    if (sprite[g_player[snum].ps->i].pal == 1)
        p = 1;
    else if (g_player[snum].ps->cursectnum >= 0)
        p = sector[g_player[snum].ps->cursectnum].floorpal;
    else p = 0;

    if (g_player[snum].ps->scuba_on)
    {
#ifdef USE_OPENGL
        if (videoGetRenderMode() >= 3)
            G_DrawTileScaled(44, (200 - tilesiz[SCUBAMASK].y), SCUBAMASK, 0, 2 + 16 + 262144, p);
#endif

        G_DrawTileScaled(43, (200 - tilesiz[SCUBAMASK].y), SCUBAMASK, 0, 2 + 16 + 262144, p);
        G_DrawTileScaled(320 - 43, (200 - tilesiz[SCUBAMASK].y), SCUBAMASK, 0, 2 + 4 + 16 + 262144, p);
    }
}

static int8_t const access_tip_y[] = {
    0, -8, -16, -32, -64, -84, -108, -108, -108, -108, -108, -108, -108, -108, -108, -108, -96, -72, -64, -32, -16,
    /* EDuke32: */ 0, 16, 32, 48,
    // At y coord 64, the hand is already not shown.
};

static int P_DisplayTip(int tipShade, int playerNum)
{
    auto const pPlayer = g_player[playerNum].ps;

    if (pPlayer->tipincs == 0)
        return 0;

    switch (X_OnEventWithReturn(EVENT_DISPLAYTIP, pPlayer->i, playerNum, 0))
    {
        case 1: return 1;
        case -1: return 0;
    }

    // Report that the tipping hand has been drawn so that the otherwise
    // selected weapon is not drawn.
    if ((unsigned)pPlayer->tipincs >= ARRAY_SIZE(access_tip_y))
        return 1;

    auto const base = P_GetDisplayBaseXY(pPlayer);
    int const tipX = 170 + base.x;
    int const tipY = 240 + base.y + (access_tip_y[pPlayer->tipincs] >> 1);
    int const tipTile = TIP + ((26 - pPlayer->tipincs) >> 4);
    int const tipPal = P_GetHudPal(pPlayer);

    G_DrawTileScaled(tipX, tipY, tipTile, tipShade, DRAWEAP_CENTER | RS_LERP, tipPal, W_TIP);

    return 1;
}

static int P_DisplayAccess(int gs,int snum)
{
    auto const pSprite = g_player[snum].ps;

    if (pSprite->access_incs == 0)
        return 0;

    switch (X_OnEventWithReturn(EVENT_DISPLAYACCESS, pSprite->i, screenpeek, 0))
    {
        case 1: return 1;
        case -1: return 0;
    }

    if ((unsigned)pSprite->access_incs >= ARRAY_SIZE(access_tip_y) - 4 || sprite[pSprite->i].extra <= 0)
        return 1;

    auto const base = P_GetDisplayBaseXY(pSprite);
    int const accessX = 170 + base.x + (access_tip_y[pSprite->access_incs] >> 2);
    int const accessY = 266 + base.y + access_tip_y[pSprite->access_incs];
    int const accessPal = (pSprite->access_spritenum >= 0) ? sprite[pSprite->access_spritenum].pal : 0;

    auto const accessMode = pSprite->access_incs > 3 && (pSprite->access_incs - 3) >> 3;
    int  const accessTile = accessMode ? HANDHOLDINGLASER + (pSprite->access_incs >> 3) : HANDHOLDINGACCESS;
    int  const accessBits = accessMode ? DRAWEAP_CENTER : (RS_YFLIP | DRAWEAP_CENTER);

    G_DrawTileScaled(accessX, accessY, accessTile, gs, accessBits | RS_LERP | RS_FORCELERP, accessPal, W_ACCESSCARD);

    return 1;
}

void P_DisplayWeapon(int snum)
{
    auto const pPlayer = g_player[snum].ps;
    auto const pSprite = ((snum == myconnectindex) && (numplayers > 1)) ? &predicted_sprite[pPlayer->i] : &sprite[pPlayer->i];

    if (pPlayer->newowner >= 0 || ud.camerasprite >= 0 || pPlayer->over_shoulder_on > 0 || (sprite[pPlayer->i].pal != 1 && sprite[pPlayer->i].extra <= 0))
        return;

    int32_t weaponX = (160) - 90;
    int32_t weaponY = klabs(pPlayer->look_ang)/9;
    int32_t weaponYOffset = 80 - (pPlayer->weapon_pos * pPlayer->weapon_pos);
    int32_t weaponShade = pSprite->shade <= 24 ? pSprite->shade : 24;
    int32_t weaponBits = pPlayer->weapon_pos == -9 || pPlayer->weapon_pos >= 10 - 1 ? 0 : RS_LERP;
    int32_t weaponFrame = pPlayer->kickback_pic;

    if (P_DisplayFistAnim(weaponShade,snum) || P_DisplayKnuckleAnim(weaponShade,snum) || P_DisplayTip(weaponShade,snum) || P_DisplayAccess(weaponShade,snum))
        return;

    P_DisplayKneeAnim(weaponShade,snum);

    if (ud.weaponsway)
    {
        weaponX -= (sintable[((pPlayer->weapon_sway>>1)+512)&2047]/(1024+512));
        weaponYOffset -= (sprite[pPlayer->i].xrepeat < 32) ? klabs(sintable[(pPlayer->weapon_sway << 2) & 2047] >> 9)
                                               : klabs(sintable[(pPlayer->weapon_sway >> 1) & 2047] >> 10);
    }
    else weaponYOffset -= 16;

    weaponX -= 58 + pPlayer->weapon_ang;
    weaponYOffset -= (pPlayer->hard_landing<<3);

    int currentWeapon = PWEAPON(snum, (pPlayer->last_weapon >= 0) ? pPlayer->last_weapon : pPlayer->curr_weapon, WorksLike);
    g_gun_pos=weaponYOffset;
    g_looking_arc=weaponY;
    g_currentweapon=currentWeapon;
    g_weapon_xoffset=weaponX;
    g_gs=weaponShade;
    g_kb=weaponFrame;
    g_looking_angSR1=pPlayer->look_ang>>1;

    Gv_SetVar(sysVarIDs.RETURN,0,pPlayer->i, snum);
    X_OnEvent(EVENT_DISPLAYWEAPON, pPlayer->i, snum, -1);

    if (Gv_GetVar(sysVarIDs.RETURN,pPlayer->i,snum) == 0)
    {
        int const quickKickFrame = 14-pPlayer->quick_kick;
        if (quickKickFrame != 14 || pPlayer->last_quick_kick)
        {
            int const weaponPal = P_GetKneePal(pPlayer);

            if (quickKickFrame < 6 || quickKickFrame > 12)
                G_DrawTileScaled(weaponX + 80 - (pPlayer->look_ang >> 1), weaponY + 250 - weaponYOffset, KNEE, weaponShade,
                    weaponBits | 4 | DRAWEAP_CENTER, weaponPal, W_KNEE2);
            else
                G_DrawTileScaled(weaponX + 160 - 16 - (pPlayer->look_ang >> 1), weaponY + 214 - weaponYOffset, KNEE + 1,
                    weaponShade, weaponBits | 4 | DRAWEAP_CENTER, weaponPal, W_KNEE2);
        }

        if (sprite[pPlayer->i].xrepeat < 40)
        {
            static int32_t fistPos;

            int const weaponPal = P_GetHudPal(pPlayer);

            if (pPlayer->jetpack_on == 0)
            {
                int const playerXvel = sprite[pPlayer->i].xvel;
                weaponY += 32 - (playerXvel >> 3);
                fistPos += playerXvel >> 3;
            }

            currentWeapon = weaponX;
            weaponX += sintable[(fistPos) & 2047] >> 10;
            G_DrawTileScaled(weaponX + 250 - (pPlayer->look_ang >> 1), weaponY + 258 - (klabs(sintable[(fistPos) & 2047] >> 8)),
                FIST, weaponShade, weaponBits, weaponPal, W_FIST);
            weaponX = currentWeapon - (sintable[(fistPos) & 2047] >> 10);
            G_DrawTileScaled(weaponX + 40 - (pPlayer->look_ang >> 1), weaponY + 200 + (klabs(sintable[(fistPos) & 2047] >> 8)), FIST,
                weaponShade, weaponBits | 4, weaponPal, W_FIST2);
        }
        else
        {
            int const doAnim = !(sprite[pPlayer->i].pal == 1 || ud.pause_on || ud.gm & MODE_MENU);
            int const halfLookAng = pPlayer->look_ang >> 1;
            int const weaponPal = P_GetHudPal(pPlayer);

            switch (currentWeapon)
            {

            case KNEE_WEAPON:

                Gv_SetVar(sysVarIDs.RETURN, 0, pPlayer->i, snum);
                X_OnEvent(EVENT_DRAWWEAPON, pPlayer->i, snum, -1);
                if (Gv_GetVar(sysVarIDs.RETURN, pPlayer->i, snum) == 0)
                {
                    if (weaponFrame > 0)
                    {
                        int const kneePal = P_GetKneePal(pPlayer);

                        if (weaponFrame < 5 || weaponFrame > 9)
                            G_DrawTileScaled(weaponX + 220 - halfLookAng, weaponY + 250 - weaponYOffset, KNEE,
                                weaponShade, weaponBits, kneePal, W_KNEE);
                        else
                            G_DrawTileScaled(weaponX + 160 - halfLookAng, weaponY + 214 - weaponYOffset, KNEE + 1,
                                weaponShade, weaponBits, kneePal, W_KNEE);
                    }
                }
                break;

            case TRIPBOMB_WEAPON:

                Gv_SetVar(sysVarIDs.RETURN, 0, pPlayer->i, snum);
                X_OnEvent(EVENT_DRAWWEAPON, pPlayer->i, snum, -1);
                if (Gv_GetVar(sysVarIDs.RETURN, pPlayer->i, snum) == 0)
                {
                    weaponX += 8;
                    weaponYOffset -= 10;

                    if ((weaponFrame) > 6)
                        weaponY += ((weaponFrame) << 3);
                    else if ((weaponFrame) < 4)
                        G_DrawWeaponTile(weaponX + 142 - halfLookAng, weaponY + 234 - weaponYOffset, TRIPBOMB, weaponShade, weaponBits, weaponPal, W_TRIPBOMB);

                    G_DrawWeaponTile(weaponX + 130 - halfLookAng, weaponY + 249 - weaponYOffset, HANDHOLDINGLASER + ((weaponFrame) >> 2), weaponShade, weaponBits,
                        weaponPal, W_TRIPBOMB_LEFTHAND);

                    G_DrawWeaponTile(weaponX + 152 - halfLookAng, weaponY + 249 - weaponYOffset, HANDHOLDINGLASER + ((weaponFrame) >> 2), weaponShade, weaponBits | 4,
                        weaponPal, W_TRIPBOMB_RIGHTHAND);
                }
                break;

            case RPG_WEAPON:

                Gv_SetVar(sysVarIDs.RETURN, 0, pPlayer->i, snum);
                X_OnEvent(EVENT_DRAWWEAPON, pPlayer->i, snum, -1);
                if (Gv_GetVar(sysVarIDs.RETURN, pPlayer->i, snum) == 0)
                {
                    weaponX -= sintable[(768 + ((weaponFrame) << 7)) & 2047] >> 11;
                    weaponYOffset += sintable[(768 + ((weaponFrame) << 7)) & 2047] >> 11;

                    if (weaponFrame > 0)
                    {
                        int totalTime;
                        if (weaponFrame < (WW2GI ? (totalTime = PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime)) : 8))
                            G_DrawWeaponTile(weaponX + 164, (weaponY << 1) + 176 - weaponYOffset, RPGGUN + ((weaponFrame) >> 1), weaponShade, weaponBits, weaponPal,
                                W_RPG_MUZZLE);
                        else if (WW2GI)
                        {
                            totalTime = PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime);
                            int const reloadTime = PWEAPON(screenpeek, pPlayer->curr_weapon, Reload);

                            weaponYOffset -= (weaponFrame < ((reloadTime - totalTime) / 2 + totalTime))
                                ? 10 * ((weaponFrame) - totalTime)   // down
                                : 10 * (reloadTime - (weaponFrame)); // up
                        }
                    }

                    G_DrawWeaponTile(weaponX + 164, (weaponY << 1) + 176 - weaponYOffset, RPGGUN, weaponShade, weaponBits, weaponPal, W_RPG);
                }
                break;

            case SHOTGUN_WEAPON:

                Gv_SetVar(sysVarIDs.RETURN, 0, pPlayer->i, snum);
                X_OnEvent(EVENT_DRAWWEAPON, pPlayer->i, snum, -1);
                if (Gv_GetVar(sysVarIDs.RETURN, pPlayer->i, snum) == 0)
                {
                    weaponX -= 8;

                    if (WW2GI)
                    {
                        int const totalTime = PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime);
                        int const reloadTime = PWEAPON(screenpeek, pPlayer->curr_weapon, Reload);

                        if (weaponFrame > 0)
                            weaponYOffset -= sintable[(weaponFrame) << 7] >> 12;

                        if (weaponFrame > 0 && doAnim)
                            weaponX += 1 - (wrand() & 3);

                        if (weaponFrame == 0)
                        {
                            G_DrawWeaponTile(weaponX + 146 - halfLookAng, weaponY + 202 - weaponYOffset, SHOTGUN, weaponShade, weaponBits | RS_FORCELERP,
                                weaponPal, W_SHOTGUN);
                        }
                        else if (weaponFrame <= totalTime)
                        {
                            G_DrawWeaponTile(weaponX + 146 - halfLookAng, weaponY + 202 - weaponYOffset, SHOTGUN + 1, weaponShade, weaponBits | RS_FORCELERP,
                                weaponPal, W_SHOTGUN);
                        }
                        // else we are in 'reload time'
                        else
                        {
                            weaponYOffset -= (weaponFrame < ((reloadTime - totalTime) / 2 + totalTime))
                                ? 10 * ((weaponFrame) - totalTime)    // D
                                : 10 * (reloadTime - (weaponFrame));  // U

                            G_DrawWeaponTile(weaponX + 146 - halfLookAng, weaponY + 202 - weaponYOffset, SHOTGUN, weaponShade, weaponBits | RS_FORCELERP,
                                weaponPal, W_SHOTGUN);
                        }

                        break;
                    }

                    switch (weaponFrame)
                    {
                    case 1:
                    case 2:
                        G_DrawWeaponTile(weaponX + 168 - halfLookAng, weaponY + 201 - weaponYOffset, SHOTGUN + 2, -128, weaponBits | RS_FORCELERP, weaponPal,
                            W_SHOTGUN_MUZZLE);
                        fallthrough__;
                    case 0:
                    case 6:
                    case 7:
                    case 8:
                        G_DrawWeaponTile(weaponX + 146 - halfLookAng, weaponY + 202 - weaponYOffset, SHOTGUN, weaponShade, weaponBits, weaponPal, W_SHOTGUN);
                        break;

                    case 3:
                    case 4:
                        weaponYOffset -= 40;
                        weaponX += 20;

                        G_DrawWeaponTile(weaponX + 178 - halfLookAng, weaponY + 194 - weaponYOffset, SHOTGUN + 1 + ((weaponFrame-1) >> 1), -128, weaponBits,
                            weaponPal, W_SHOTGUN_MUZZLE);
                        fallthrough__;
                    case 5:
                    case 9:
                    case 10:
                    case 11:
                    case 12:
                        G_DrawWeaponTile(weaponX + 158 - halfLookAng, weaponY + 220 - weaponYOffset, SHOTGUN + 3, weaponShade, weaponBits, weaponPal, W_SHOTGUN);
                        break;

                    case 13:
                    case 14:
                    case 15:
                        G_DrawWeaponTile(32 + weaponX + 166 - halfLookAng, weaponY + 210 - weaponYOffset, SHOTGUN + 4, weaponShade, weaponBits & ~RS_LERP, weaponPal,
                            W_SHOTGUN);
                        break;

                    case 16:
                    case 17:
                    case 18:
                    case 19:
                    case 24:
                    case 25:
                    case 26:
                    case 27:
                        G_DrawWeaponTile(64 + weaponX + 170 - halfLookAng, weaponY + 196 - weaponYOffset, SHOTGUN + 5, weaponShade, weaponBits, weaponPal,
                            W_SHOTGUN);
                        break;

                    case 20:
                    case 21:
                    case 22:
                    case 23:
                        G_DrawWeaponTile(64 + weaponX + 176 - halfLookAng, weaponY + 196 - weaponYOffset, SHOTGUN + 6, weaponShade, weaponBits, weaponPal,
                            W_SHOTGUN);
                        break;


                    case 28:
                    case 29:
                    case 30:
                        G_DrawWeaponTile(32 + weaponX + 156 - halfLookAng, weaponY + 206 - weaponYOffset, SHOTGUN + 4, weaponShade, weaponBits, weaponPal,
                            W_SHOTGUN);
                        break;
                    }
                }
                break;


            case CHAINGUN_WEAPON:

                Gv_SetVar(sysVarIDs.RETURN, 0, pPlayer->i, snum);
                X_OnEvent(EVENT_DRAWWEAPON, pPlayer->i, snum, -1);
                if (Gv_GetVar(sysVarIDs.RETURN, pPlayer->i, snum) == 0)
                {
                    if (weaponFrame > 0)
                    {
                        weaponYOffset -= sintable[(weaponFrame) << 7] >> 12;

                        if (doAnim)
                            weaponX += 1 - (rand() & 3);
                    }

                    if (WW2GI)
                    {
                        int const totalTime = PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime);
                        int const reloadTime = PWEAPON(screenpeek, pPlayer->curr_weapon, Reload);

                        if (weaponFrame == 0)
                        {
                            G_DrawWeaponTile(weaponX + 178 - halfLookAng, weaponY + 233 - weaponYOffset, CHAINGUN + 1, weaponShade, weaponBits | RS_FORCELERP,
                                weaponPal, W_CHAINGUN_BOTTOM);
                        }
                        else if (weaponFrame <= totalTime)
                        {
                            G_DrawWeaponTile(weaponX + 188 - halfLookAng, weaponY + 243 - weaponYOffset, CHAINGUN + 2, weaponShade, weaponBits | RS_FORCELERP,
                                weaponPal, W_CHAINGUN_BOTTOM);
                        }
                        // else we are in 'reload time'
                        // divide reload time into fifths..
                        // 1) move weapon up/right, hand on clip (CHAINGUN - 17)
                        // 2) move weapon up/right, hand removing clip (CHAINGUN - 18)
                        // 3) hold weapon up/right, hand removed clip (CHAINGUN - 19)
                        // 4) hold weapon up/right, hand inserting clip (CHAINGUN - 18)
                        // 5) move weapon down/left, clip inserted (CHAINGUN - 17)
                        else
                        {
                            int iFifths = (reloadTime - totalTime) / 5;
                            if (iFifths < 1)
                                iFifths = 1;

                            if (weaponFrame < iFifths + totalTime)
                            {
                                // first segment
                                int const weaponOffset = 80 - 10 * (totalTime + iFifths - (weaponFrame));
                                weaponYOffset += weaponOffset;
                                weaponX += weaponOffset;
                                G_DrawWeaponTile(weaponX + 168 - halfLookAng, weaponY + 260 - weaponYOffset, CHAINGUN - 17, weaponShade,
                                    weaponBits | RS_FORCELERP, weaponPal, W_CHAINGUN_BOTTOM);
                            }
                            else if (weaponFrame < (iFifths * 2 + totalTime))
                            {
                                // second segment
                                weaponYOffset += 80; // D
                                weaponX += 80;
                                G_DrawWeaponTile(weaponX + 168 - halfLookAng, weaponY + 260 - weaponYOffset, CHAINGUN - 18, weaponShade,
                                    weaponBits | RS_FORCELERP, weaponPal, W_CHAINGUN_BOTTOM);
                            }
                            else if (weaponFrame < (iFifths * 3 + totalTime))
                            {
                                // third segment
                                // up
                                weaponYOffset += 80;
                                weaponX += 80;
                                G_DrawWeaponTile(weaponX + 168 - halfLookAng, weaponY + 260 - weaponYOffset, CHAINGUN - 19, weaponShade,
                                    weaponBits | RS_FORCELERP, weaponPal, W_CHAINGUN_BOTTOM);
                            }
                            else if (weaponFrame < (iFifths * 4 + totalTime))
                            {
                                // fourth segment
                                // down
                                weaponYOffset += 80; // D
                                weaponX += 80;
                                G_DrawWeaponTile(weaponX + 168 - halfLookAng, weaponY + 260 - weaponYOffset, CHAINGUN - 18, weaponShade,
                                    weaponBits | RS_FORCELERP, weaponPal, W_CHAINGUN_BOTTOM);
                            }
                            else
                            {
                                // up and left
                                int const weaponOffset = 10 * (reloadTime - (weaponFrame));
                                weaponYOffset += weaponOffset; // U
                                weaponX += weaponOffset;
                                G_DrawWeaponTile(weaponX + 168 - halfLookAng, weaponY + 260 - weaponYOffset, CHAINGUN - 17, weaponShade,
                                    weaponBits | RS_FORCELERP, weaponPal, W_CHAINGUN_BOTTOM);
                            }
                        }

                        break;
                    }

                    switch (weaponFrame)
                    {
                    case 0:
                        G_DrawWeaponTile(weaponX + 178 - (pPlayer->look_ang >> 1), weaponY + 233 - weaponYOffset, CHAINGUN + 1, weaponShade,
                            weaponBits | RS_FORCELERP, weaponPal, W_CHAINGUN_TOP);
                        G_DrawWeaponTile(weaponX + 168 - (pPlayer->look_ang >> 1), weaponY + 260 - weaponYOffset, CHAINGUN, weaponShade, weaponBits, weaponPal,
                            W_CHAINGUN_BOTTOM);
                        break;

                    default:
                        if (weaponFrame > PWEAPON(screenpeek, CHAINGUN_WEAPON, FireDelay) && weaponFrame < PWEAPON(screenpeek, CHAINGUN_WEAPON, TotalTime))
                        {
                            int randomOffset = doAnim ? rand() & 7 : 0;
                            G_DrawWeaponTile(randomOffset + weaponX - 4 + 140 - (pPlayer->look_ang >> 1),
                                randomOffset + weaponY - ((weaponFrame) >> 1) + 208 - weaponYOffset, CHAINGUN + 5 + ((weaponFrame - 4) / 5),
                                weaponShade, weaponBits, weaponPal);
                            if (doAnim)
                                randomOffset = rand() & 7;
                            G_DrawWeaponTile(randomOffset + weaponX - 4 + 184 - (pPlayer->look_ang >> 1),
                                randomOffset + weaponY - ((weaponFrame) >> 1) + 208 - weaponYOffset, CHAINGUN + 5 + ((weaponFrame - 4) / 5),
                                weaponShade, weaponBits, weaponPal);
                        }

                        if (weaponFrame < PWEAPON(screenpeek, CHAINGUN_WEAPON, TotalTime) - 4)
                        {
                            int const randomOffset = doAnim ? rand() & 7 : 0;
                            G_DrawWeaponTile(randomOffset + weaponX - 4 + 162 - (pPlayer->look_ang >> 1),
                                randomOffset + weaponY - ((weaponFrame) >> 1) + 208 - weaponYOffset, CHAINGUN + 5 + ((weaponFrame - 2) / 5),
                                weaponShade, weaponBits, weaponPal);
                            G_DrawWeaponTile(weaponX + 178 - (pPlayer->look_ang >> 1), weaponY + 233 - weaponYOffset, CHAINGUN + 1 + ((weaponFrame) >> 1),
                                weaponShade, weaponBits & ~RS_LERP, weaponPal, W_CHAINGUN_TOP);
                        }
                        else
                            G_DrawWeaponTile(weaponX + 178 - (pPlayer->look_ang >> 1), weaponY + 233 - weaponYOffset, CHAINGUN + 1, weaponShade,
                                weaponBits & ~RS_LERP, weaponPal, W_CHAINGUN_TOP);

                        G_DrawWeaponTile(weaponX + 168 - (pPlayer->look_ang >> 1), weaponY + 260 - weaponYOffset, CHAINGUN, weaponShade, weaponBits & ~RS_LERP,
                            weaponPal, W_CHAINGUN_BOTTOM);
                        break;
                    }
                }
                break;

            case PISTOL_WEAPON:

                Gv_SetVar(sysVarIDs.RETURN, 0, pPlayer->i, snum);
                X_OnEvent(EVENT_DRAWWEAPON, pPlayer->i, snum, -1);
                if (Gv_GetVar(sysVarIDs.RETURN, pPlayer->i, snum) == 0)
                {
                    if ((weaponFrame) < PWEAPON(screenpeek, PISTOL_WEAPON, TotalTime) + 1)
                    {
                        static uint8_t pistolFrames[] = { 0, 1, 2 };
                        int            pistolOffset = 195 - 12 + weaponX;

                        if ((weaponFrame) == PWEAPON(screenpeek, PISTOL_WEAPON, FireDelay))
                            pistolOffset -= 3;

                        G_DrawWeaponTile((pistolOffset - (pPlayer->look_ang >> 1)), (weaponY + 244 - weaponYOffset),
                            FIRSTGUN + pistolFrames[weaponFrame > 2 ? 0 : weaponFrame], weaponShade, weaponBits, weaponPal, W_PISTOL);

                        break;
                    }

                    int32_t const FIRSTGUN_5 = FIRSTGUN + 5;

                    if ((weaponFrame) < PWEAPON(screenpeek, PISTOL_WEAPON, Reload) - (WW2GI ? 40 : 17))
                        G_DrawWeaponTile(194 - (pPlayer->look_ang >> 1), weaponY + 230 - weaponYOffset, FIRSTGUN + 4, weaponShade, weaponBits & ~RS_LERP,
                            weaponPal, W_PISTOL);
                    else if ((weaponFrame) < PWEAPON(screenpeek, PISTOL_WEAPON, Reload) - (WW2GI ? 35 : 12))
                    {
                        G_DrawWeaponTile(244 - ((weaponFrame) << 3) - (pPlayer->look_ang >> 1), weaponY + 130 - weaponYOffset + ((weaponFrame) << 4), FIRSTGUN + 6,
                            weaponShade, weaponBits, weaponPal, W_PISTOL_CLIP);
                        G_DrawWeaponTile(224 - (pPlayer->look_ang >> 1), weaponY + 220 - weaponYOffset, FIRSTGUN_5, weaponShade, weaponBits, weaponPal, W_PISTOL);
                    }
                    else if ((weaponFrame) < PWEAPON(screenpeek, PISTOL_WEAPON, Reload) - (WW2GI ? 30 : 7))
                    {
                        G_DrawWeaponTile(124 + ((weaponFrame) << 1) - (pPlayer->look_ang >> 1), weaponY + 430 - weaponYOffset - ((weaponFrame) << 3), FIRSTGUN + 6,
                            weaponShade, weaponBits, weaponPal, W_PISTOL_CLIP);
                        G_DrawWeaponTile(224 - (pPlayer->look_ang >> 1), weaponY + 220 - weaponYOffset, FIRSTGUN_5, weaponShade, weaponBits, weaponPal, W_PISTOL);
                    }

                    else if ((weaponFrame) < PWEAPON(screenpeek, PISTOL_WEAPON, Reload) - (WW2GI ? 12 : 4))
                    {
                        G_DrawWeaponTile(184 - (pPlayer->look_ang >> 1), weaponY + 235 - weaponYOffset, FIRSTGUN + 8, weaponShade, weaponBits, weaponPal,
                            W_PISTOL_HAND);
                        G_DrawWeaponTile(224 - (pPlayer->look_ang >> 1), weaponY + 210 - weaponYOffset, FIRSTGUN_5, weaponShade, weaponBits, weaponPal, W_PISTOL);
                    }
                    else if ((weaponFrame) < PWEAPON(screenpeek, PISTOL_WEAPON, Reload) - (WW2GI ? 6 : 2))
                    {
                        G_DrawWeaponTile(164 - (pPlayer->look_ang >> 1), weaponY + 245 - weaponYOffset, FIRSTGUN + 8, weaponShade, weaponBits, weaponPal,
                            W_PISTOL_HAND);
                        G_DrawWeaponTile(224 - (pPlayer->look_ang >> 1), weaponY + 220 - weaponYOffset, FIRSTGUN_5, weaponShade, weaponBits, weaponPal, W_PISTOL);
                    }
                    else if ((weaponFrame) < PWEAPON(screenpeek, PISTOL_WEAPON, Reload))
                        G_DrawWeaponTile(194 - (pPlayer->look_ang >> 1), weaponY + 235 - weaponYOffset, FIRSTGUN_5, weaponShade, weaponBits, weaponPal, W_PISTOL);
                }

                break;
            case HANDBOMB_WEAPON:
            {
                Gv_SetVar(sysVarIDs.RETURN, 0, pPlayer->i, snum);
                X_OnEvent(EVENT_DRAWWEAPON, pPlayer->i, snum, -1);
                if (Gv_GetVar(sysVarIDs.RETURN, pPlayer->i, snum) == 0)
                {
                    static uint8_t pipebombFrames[] = { 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2 };

                    if (weaponFrame >= PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime) || weaponFrame >= ARRAY_SSIZE(pipebombFrames))
                        break;

                    if (weaponFrame)
                    {
                        if (WW2GI)
                        {
                            int const fireDelay = PWEAPON(screenpeek, pPlayer->curr_weapon, FireDelay);
                            int const totalTime = PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime);

                            if (weaponFrame <= fireDelay)
                            {
                                // it holds here
                                weaponYOffset -= 5 * (weaponFrame);  // D
                            }
                            else if (weaponFrame < ((totalTime - fireDelay) / 2 + fireDelay))
                            {
                                // up and left
                                int const weaponOffset = (weaponFrame) - fireDelay;
                                weaponYOffset += 10 * weaponOffset;  // U
                                weaponX += 80 * weaponOffset;
                            }
                            else if (weaponFrame < totalTime)
                            {
                                // start high
                                weaponYOffset += 240;
                                weaponYOffset -= 12 * ((weaponFrame) - fireDelay);  // D
                                // move left
                                weaponX += 90 - 5 * (totalTime - (weaponFrame));
                            }
                        }
                        else
                        {
                            if (weaponFrame < 7)       weaponYOffset -= 10 * (weaponFrame);  // D
                            else if (weaponFrame < 12) weaponYOffset += 20 * ((weaponFrame) - 10);  // U
                            else if (weaponFrame < 20) weaponYOffset -= 9 * ((weaponFrame) - 14);  // D
                        }

                        weaponYOffset += 10;
                    }

                    G_DrawWeaponTile(weaponX + 190 - halfLookAng, weaponY + 260 - weaponYOffset, HANDTHROW + pipebombFrames[(weaponFrame)], weaponShade,
                        weaponBits, weaponPal, W_HANDBOMB);
                }
            }
            break;

            case HANDREMOTE_WEAPON:
            {
                Gv_SetVar(sysVarIDs.RETURN, 0, pPlayer->i, snum);
                X_OnEvent(EVENT_DRAWWEAPON, pPlayer->i, snum, -1);
                if (Gv_GetVar(sysVarIDs.RETURN, pPlayer->i, snum) == 0)
                {
                    static uint8_t remoteFrames[] = { 0, 1, 1, 2, 1, 1, 0, 0, 0, 0, 0 };

                    if (weaponFrame >= ARRAY_SSIZE(remoteFrames))
                        break;

                    weaponX = -48;
                    G_DrawWeaponTile(weaponX + 150 - halfLookAng, weaponY + 258 - weaponYOffset, HANDREMOTE + remoteFrames[(weaponFrame)], weaponShade,
                        weaponBits | RS_FORCELERP, weaponPal, W_HANDREMOTE);
                }
            }
            break;

            case DEVISTATOR_WEAPON:

                Gv_SetVar(sysVarIDs.RETURN, 0, pPlayer->i, snum);
                X_OnEvent(EVENT_DRAWWEAPON, pPlayer->i, snum, -1);
                if (Gv_GetVar(sysVarIDs.RETURN, pPlayer->i, snum) == 0)
                {
                    if (WW2GI)
                    {
                        if (weaponFrame)
                        {
                            int32_t const totalTime = PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime);
                            int32_t const reloadTime = PWEAPON(screenpeek, pPlayer->curr_weapon, Reload);

                            if (weaponFrame < totalTime)
                            {
                                int const tileOffset = ksgn((weaponFrame) >> 2);

                                if (pPlayer->ammo_amount[pPlayer->curr_weapon] & 1)
                                {
                                    G_DrawWeaponTile(weaponX + 30 - halfLookAng, weaponY + 240 - weaponYOffset, DEVISTATOR, weaponShade, weaponBits | 4, weaponPal,
                                        W_DEVISTATOR_LEFT);
                                    G_DrawWeaponTile(weaponX + 268 - halfLookAng, weaponY + 238 - weaponYOffset, DEVISTATOR + tileOffset, -32, weaponBits, weaponPal,
                                        W_DEVISTATOR_RIGHT);
                                }
                                else
                                {
                                    G_DrawWeaponTile(weaponX + 30 - halfLookAng, weaponY + 240 - weaponYOffset, DEVISTATOR + tileOffset, -32, weaponBits | 4, weaponPal,
                                        W_DEVISTATOR_LEFT);
                                    G_DrawWeaponTile(weaponX + 268 - halfLookAng, weaponY + 238 - weaponYOffset, DEVISTATOR, weaponShade, weaponBits, weaponPal,
                                        W_DEVISTATOR_RIGHT);
                                }
                            }
                            // else we are in 'reload time'
                            else
                            {
                                weaponYOffset
                                    -= (weaponFrame < ((reloadTime - totalTime) / 2 + totalTime)) ? 10 * ((weaponFrame) - totalTime) : 10 * (reloadTime - (weaponFrame));

                                G_DrawWeaponTile(weaponX + 30 - halfLookAng, weaponY + 240 - weaponYOffset, DEVISTATOR, weaponShade, weaponBits | 4, weaponPal,
                                    W_DEVISTATOR_LEFT);
                                G_DrawWeaponTile(weaponX + 268 - halfLookAng, weaponY + 238 - weaponYOffset, DEVISTATOR, weaponShade, weaponBits, weaponPal,
                                    W_DEVISTATOR_RIGHT);
                            }
                        }
                        else
                        {
                            G_DrawWeaponTile(weaponX + 30 - halfLookAng, weaponY + 240 - weaponYOffset, DEVISTATOR, weaponShade, weaponBits | 4, weaponPal,
                                W_DEVISTATOR_LEFT);
                            G_DrawWeaponTile(weaponX + 268 - halfLookAng, weaponY + 238 - weaponYOffset, DEVISTATOR, weaponShade, weaponBits, weaponPal,
                                W_DEVISTATOR_RIGHT);
                        }
                        break;
                    }

                    if (weaponFrame <= PWEAPON(screenpeek, DEVISTATOR_WEAPON, TotalTime) && weaponFrame > 0)
                    {
                        static uint8_t const devastatorFrames[] = { 0, 4, 12, 24, 12, 4, 0 };

                        if (weaponFrame >= ARRAY_SSIZE(devastatorFrames))
                            break;

                        int const tileOffset = ksgn((weaponFrame) >> 2);

                        if (pPlayer->hbomb_hold_delay)
                        {
                            G_DrawWeaponTile(weaponX + 30 - halfLookAng, weaponY + 240 - weaponYOffset, DEVISTATOR, weaponShade, weaponBits | 4, weaponPal,
                                W_DEVISTATOR_LEFT);
                            G_DrawWeaponTile((devastatorFrames[weaponFrame] >> 1) + weaponX + 268 - halfLookAng,
                                devastatorFrames[weaponFrame] + weaponY + 238 - weaponYOffset, DEVISTATOR + tileOffset, -32, weaponBits, weaponPal,
                                W_DEVISTATOR_RIGHT);
                        }
                        else
                        {
                            G_DrawWeaponTile(-(devastatorFrames[weaponFrame] >> 1) + weaponX + 30 - halfLookAng,
                                devastatorFrames[weaponFrame] + weaponY + 240 - weaponYOffset, DEVISTATOR + tileOffset, -32, weaponBits | 4, weaponPal,
                                W_DEVISTATOR_LEFT);
                            G_DrawWeaponTile(weaponX + 268 - halfLookAng, weaponY + 238 - weaponYOffset, DEVISTATOR, weaponShade, weaponBits, weaponPal,
                                W_DEVISTATOR_RIGHT);
                        }
                    }
                    else
                    {
                        G_DrawWeaponTile(weaponX + 30 - halfLookAng, weaponY + 240 - weaponYOffset, DEVISTATOR, weaponShade, weaponBits | 4, weaponPal,
                            W_DEVISTATOR_LEFT);
                        G_DrawWeaponTile(weaponX + 268 - halfLookAng, weaponY + 238 - weaponYOffset, DEVISTATOR, weaponShade, weaponBits, weaponPal, W_DEVISTATOR_RIGHT);
                    }
                }
                break;

            case FREEZE_WEAPON:

                Gv_SetVar(sysVarIDs.RETURN, 0, pPlayer->i, snum);
                X_OnEvent(EVENT_DRAWWEAPON, pPlayer->i, snum, -1);
                if (Gv_GetVar(sysVarIDs.RETURN, pPlayer->i, snum) == 0)
                {
                    if ((weaponFrame) < (PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime) + 1) && (weaponFrame) > 0)
                    {
                        static uint8_t freezerFrames[] = { 0, 0, 1, 1, 2, 2 };

                        if (doAnim)
                        {
                            weaponX += rand() & 3;
                            weaponY += rand() & 3;
                        }
                        weaponYOffset -= 16;
                        G_DrawWeaponTile(weaponX + 210 - (pPlayer->look_ang >> 1), weaponY + 261 - weaponYOffset, FREEZE + 2, -32,
                            weaponBits & ~RS_LERP, weaponPal, W_FREEZE_BASE);
                        G_DrawWeaponTile(weaponX + 210 - (pPlayer->look_ang >> 1), weaponY + 235 - weaponYOffset, FREEZE + 3 + freezerFrames[weaponFrame % 6], -32,
                            weaponBits & ~RS_LERP, weaponPal, W_FREEZE_TOP);
                    }
                    else
                        G_DrawWeaponTile(weaponX + 210 - (pPlayer->look_ang >> 1), weaponY + 261 - weaponYOffset, FREEZE, weaponShade, weaponBits, weaponPal,
                            W_FREEZE_BASE);
                }
                break;

            case GROW_WEAPON:
            case SHRINKER_WEAPON:
                Gv_SetVar(sysVarIDs.RETURN, 0, pPlayer->i, snum);
                X_OnEvent(EVENT_DRAWWEAPON, pPlayer->i, snum, -1);
                if (Gv_GetVar(sysVarIDs.RETURN, pPlayer->i, snum) == 0)
                {
                    bool const isExpander = currentWeapon == GROW_WEAPON;

                    weaponX += 28;
                    weaponY += 18;

                    if (WW2GI)
                    {
                        if (weaponFrame == 0)
                        {
                            // the 'at rest' display
                            if (isExpander)
                            {
                                G_DrawWeaponTile(weaponX + 188 - halfLookAng, weaponY + 240 - weaponYOffset, SHRINKER - 2, weaponShade, weaponBits, weaponPal,
                                    W_SHRINKER);
                                break;
                            }
                            else if (pPlayer->ammo_amount[currentWeapon] > 0)
                            {
                                G_DrawWeaponTileUnfaded(weaponX + 184 - halfLookAng, weaponY + 240 - weaponYOffset, SHRINKER + 2,
                                    16 - (sintable[pPlayer->random_club_frame & 2047] >> 10), weaponBits, 0, W_SHRINKER_CRYSTAL);
                                G_DrawWeaponTile(weaponX + 188 - halfLookAng, weaponY + 240 - weaponYOffset, SHRINKER, weaponShade, weaponBits, weaponPal,
                                    W_SHRINKER);
                                break;
                            }
                        }
                        else
                        {
                            // the 'active' display.
                            if (doAnim)
                            {
                                weaponX += rand() & 3;
                                weaponYOffset += rand() & 3;
                            }

                            int const totalTime = PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime);
                            int const reloadTime = PWEAPON(screenpeek, pPlayer->curr_weapon, Reload);

                            if (weaponFrame < totalTime)
                            {
                                if (weaponFrame >= PWEAPON(screenpeek, pPlayer->curr_weapon, FireDelay))
                                {
                                    // after fire time.
                                    // lower weapon to reload cartridge (not clip)
                                    weaponYOffset -= (isExpander ? 15 : 10) * (totalTime - (weaponFrame));
                                }
                            }
                            // else we are in 'reload time'
                            else
                            {
                                weaponYOffset -= (weaponFrame < ((reloadTime - totalTime) / 2 + totalTime))
                                    ? (isExpander ? 5 : 10) * ((weaponFrame) - totalTime) // D
                                    : 10 * (reloadTime - (weaponFrame)); // U
                            }
                        }

                        G_DrawWeaponTileUnfaded(weaponX + 184 - halfLookAng, weaponY + 240 - weaponYOffset, SHRINKER + 3 + ((weaponFrame) & 3), -32, weaponBits,
                            isExpander ? 2 : 0, 0 /*W_SHRINKER_CRYSTAL*/);

                        G_DrawWeaponTile(weaponX + 188 - halfLookAng, weaponY + 240 - weaponYOffset, SHRINKER + (isExpander ? -1 : 1), weaponShade,
                            weaponBits, weaponPal, 0 /*W_SHRINKER*/);

                        break;
                    }

                    if ((weaponFrame) < PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime) && (weaponFrame) > 0)
                    {
                        if (doAnim)
                        {
                            weaponX += rand() & 3;
                            weaponYOffset += (rand() & 3);
                        }

                        G_DrawWeaponTileUnfaded(weaponX + 184 - halfLookAng, weaponY + 240 - weaponYOffset, SHRINKER + 3 + ((weaponFrame) & 3), -32,
                            weaponBits & ~RS_LERP, isExpander ? 2 : 0, W_SHRINKER_CRYSTAL);
                        G_DrawWeaponTile(weaponX + 188 - halfLookAng, weaponY + 240 - weaponYOffset, SHRINKER + (isExpander ? -1 : 1), weaponShade,
                            weaponBits & ~RS_LERP, weaponPal, W_SHRINKER);
                    }
                    else
                    {
                        G_DrawWeaponTileUnfaded(weaponX + 184 - halfLookAng, weaponY + 240 - weaponYOffset, SHRINKER + 2,
                            16 - (sintable[pPlayer->random_club_frame & 2047] >> 10), weaponBits | RS_FORCELERP,
                            isExpander ? 2 : 0, W_SHRINKER_CRYSTAL);
                        G_DrawWeaponTile(weaponX + 188 - halfLookAng, weaponY + 240 - weaponYOffset, SHRINKER + (isExpander ? -2 : 0), weaponShade,
                            weaponBits, weaponPal, W_SHRINKER);
                    }
                }
                break;
            }
        }
    }
    P_DisplaySpitAnim(snum);

}

int g_myAimMode = 0, g_myAimStat = 0, g_oldAimStat = 0;
int32_t mouseyaxismode = -1;

void P_GetInput(int snum, bool uncapped)
{
    static ControlInfo info;
    static int32_t     dyaw;
    int32_t turnamount;
    int32_t keymove;
    DukePlayer_t *p = g_player[snum].ps;

    if ((ud.gm&MODE_MENU) || ud.limit_hit || (ud.gm&MODE_TYPE)
        || (ud.pause_on && !KB_KeyPressed(sc_Pause))
        //|| (numplayers > 1 && (int32_t)totalclock < 10) // HACK: kill P_GetInput() for the first 10 tics of a new map in multi
        )   
    {
        if (!(ud.gm&MODE_MENU))
            CONTROL_GetInput(&info);

        localInput = {};
        localInput.bits = (((int32_t)g_gameQuit) << SK_GAMEQUIT);
        localInput.extbits |= (EXTBIT_CHAT);

        dyaw = 0;
        return;
    }

    if (multiflag == 1)
    {
        localInput = {};
        localInput.bits = 1 << SK_MULTIFLAG;
        localInput.bits |= multiwhat << (SK_MULTIFLAG + 1);
        localInput.bits |= multipos << (SK_MULTIFLAG + 2);
        multiflag = 0;
        return;
    }

    CONTROL_ProcessBinds();
    CONTROL_GetInput(&info);

    static int32_t turnheldtime;
    static int32_t lastcontroltime;
    int32_t tics = (int32_t)totalclock - lastcontroltime;
    lastcontroltime = (int32_t)totalclock;

    // [JM] Brutal shitfuck hack to replace missing MACT functionality.
    if ((info.mousex < -DIGITALAXISDEADZONE) && ud.config.MouseDigitalFunctions[0][0] >= 0) // Left
        CONTROL_ButtonState |= ((uint64_t)1 << ((uint64_t)(ud.config.MouseDigitalFunctions[0][0])));

    if ((info.mousex > DIGITALAXISDEADZONE) && ud.config.MouseDigitalFunctions[0][1] >= 0) // Right
        CONTROL_ButtonState |= ((uint64_t)1 << ((uint64_t)(ud.config.MouseDigitalFunctions[0][1])));

    if ((info.mousey < -DIGITALAXISDEADZONE) && ud.config.MouseDigitalFunctions[1][0] >= 0) // Up
        CONTROL_ButtonState |= ((uint64_t)1 << ((uint64_t)(ud.config.MouseDigitalFunctions[1][0])));

    if ((info.mousey > DIGITALAXISDEADZONE) && ud.config.MouseDigitalFunctions[1][1] >= 0) // Down
        CONTROL_ButtonState |= ((uint64_t)1 << ((uint64_t)(ud.config.MouseDigitalFunctions[1][1])));

    if (ud.mouseaiming)
        g_myAimMode = BUTTON(gamefunc_Mouse_Aiming);
    else
    {
        g_oldAimStat = g_myAimStat;
        g_myAimStat = BUTTON(gamefunc_Mouse_Aiming);
        if (g_myAimStat > g_oldAimStat)
        {
            g_myAimMode ^= 1;
            P_DoQuote(QUOTE_MOUSE_AIMING_OFF+g_myAimMode,p);
        }
    } 

    if (ud.config.MouseBias)
    {
        if (klabs(info.mousex) > klabs(info.mousey))
            info.mousey = tabledivide32_noinline(info.mousey, ud.config.MouseBias);
        else
            info.mousex = tabledivide32_noinline(info.mousex, ud.config.MouseBias);
    }

    bool running = (ud.runkey_mode) ? (BUTTON(gamefunc_Run) | ud.auto_run) : (ud.auto_run ^ BUTTON(gamefunc_Run));
    input_t tempInput = {};

    if (BUTTON(gamefunc_Strafe))
    {
        tempInput.svel = -(info.mousex + dyaw) >> 3;
        dyaw = (info.mousex + dyaw) % 8;
    }
    else
    {
        tempInput.q16avel = fix16_sadd(tempInput.q16avel, fix16_sdiv(fix16_from_int(info.mousex), F16(32)));
    }

    if (g_myAimMode)
        tempInput.q16horz = fix16_sadd(tempInput.q16horz, fix16_sdiv(fix16_from_int(info.mousey), F16(64)));
    else
        tempInput.fvel = -(info.mousey >> 3);

    if (ud.mouseflip) tempInput.q16horz = -tempInput.q16horz;

    tempInput.svel -= info.dx;
    tempInput.fvel -= info.dz >> 6;

    if (running)
    {
        turnamount = NORMALTURN << 1;
        keymove = NORMALKEYMOVE << 1;
    }
    else
    {
        turnamount = NORMALTURN;
        keymove = NORMALKEYMOVE;
    }

    if (BUTTON(gamefunc_Strafe))
    {
        if (BUTTON(gamefunc_Turn_Left) && !(g_player[snum].ps->movement_lock & 4))
            tempInput.svel -= -keymove;
        if (BUTTON(gamefunc_Turn_Right) && !(g_player[snum].ps->movement_lock & 8))
            tempInput.svel -= keymove;
    }
    else
    {
        if (BUTTON(gamefunc_Turn_Left))
        {
            turnheldtime += tics;
            tempInput.q16avel -= fix16_from_int((turnheldtime >= TURBOTURNTIME) ? (turnamount << 1) : (PREAMBLETURN << 1));
        }
        else if (BUTTON(gamefunc_Turn_Right))
        {
            turnheldtime += tics;
            tempInput.q16avel += fix16_from_int((turnheldtime >= TURBOTURNTIME) ? (turnamount << 1) : (PREAMBLETURN << 1));
        }
        else
            turnheldtime = 0;
    }

    if (BUTTON(gamefunc_Strafe_Left) && !(g_player[snum].ps->movement_lock & 4))
        tempInput.svel += keymove;
    if (BUTTON(gamefunc_Strafe_Right) && !(g_player[snum].ps->movement_lock & 8))
        tempInput.svel += -keymove;
    if (BUTTON(gamefunc_Move_Forward) && !(g_player[snum].ps->movement_lock & 1))
        tempInput.fvel += keymove;
    if (BUTTON(gamefunc_Move_Backward) && !(g_player[snum].ps->movement_lock & 2))
        tempInput.fvel += -keymove;

    if (tempInput.fvel < -MAXVEL)  tempInput.fvel = -MAXVEL;
    if (tempInput.fvel > MAXVEL)   tempInput.fvel = MAXVEL;
    if (tempInput.svel < -MAXSVEL) tempInput.svel = -MAXSVEL;
    if (tempInput.svel > MAXSVEL)  tempInput.svel = MAXSVEL;

    //q16angvel = fix16_clamp(q16angvel, F16(-MAXANGVEL), F16(MAXANGVEL));
    //q16horiz = fix16_clamp(q16horiz, F16(-MAXHORIZ), F16(MAXHORIZ));

    int weaponSelection = 0;
    for (weaponSelection = gamefunc_Weapon_10; weaponSelection >= gamefunc_Weapon_1; --weaponSelection)
    {
        if (BUTTON(weaponSelection))
        {
            weaponSelection -= (gamefunc_Weapon_1 - 1);
            break;
        }
    }

    if (BUTTON(gamefunc_Last_Weapon))
        weaponSelection = 14;
    else if (BUTTON(gamefunc_Alt_Weapon))
        weaponSelection = 13;
    else if (BUTTON(gamefunc_Next_Weapon) || (BUTTON(gamefunc_Dpad_Select) && tempInput.fvel > 0))
        weaponSelection = 12;
    else if (BUTTON(gamefunc_Previous_Weapon) || (BUTTON(gamefunc_Dpad_Select) && tempInput.fvel < 0))
        weaponSelection = 11;
    else if (weaponSelection == gamefunc_Weapon_1 - 1)
        weaponSelection = 0;

    if (weaponSelection && (localInput.bits & SK_WEAPON_MASK) == 0)
        localInput.bits |= (weaponSelection << SK_WEAPON_BITS);

    localInput.bits |= BUTTON(gamefunc_Fire)       << SK_FIRE;
    localInput.bits |= BUTTON(gamefunc_Open)       << SK_OPEN;
    localInput.bits |= BUTTON(gamefunc_Aim_Up)     << SK_AIM_UP;
    localInput.bits |= BUTTON(gamefunc_Aim_Down)   << SK_AIM_DOWN;
    localInput.bits |= BUTTON(gamefunc_Look_Left)  << SK_LOOK_LEFT;
    localInput.bits |= BUTTON(gamefunc_Look_Right) << SK_LOOK_RIGHT;
    localInput.bits |= (running)                   << SK_RUN;

    if (aplWeaponFlags[g_player[snum].ps->curr_weapon][snum] & WEAPON_SEMIAUTO && BUTTON(gamefunc_Fire))
        CONTROL_ClearButton(gamefunc_Fire);

    int const sectorLotag = p->cursectnum != -1 ? sector[p->cursectnum].lotag : 0;
    int const crouchable = sectorLotag != 2 && (sectorLotag != 1 || p->spritebridge) && !p->jetpack_on;
    static int crouch_toggle = 0;

    if (p->cheat_phase < 1)
    {
        if (BUTTON(gamefunc_Toggle_Crouch))
        {
            crouch_toggle = !crouch_toggle && crouchable;

            if (crouchable)
                CONTROL_ClearButton(gamefunc_Toggle_Crouch);
        }

        if (BUTTON(gamefunc_Crouch) || BUTTON(gamefunc_Jump) || p->jetpack_on || (!crouchable && p->on_ground))
            crouch_toggle = 0;

        int const crouching = BUTTON(gamefunc_Crouch) || BUTTON(gamefunc_Toggle_Crouch) || crouch_toggle;

        localInput.bits |= crouching << SK_CROUCH;
    }

    localInput.bits |=   BUTTON(gamefunc_Jump)            << SK_JUMP;
    localInput.bits |=   BUTTON(gamefunc_Steroids)        << SK_STEROIDS;
    localInput.bits |=   BUTTON(gamefunc_Look_Up)         << SK_LOOK_UP;
    localInput.bits |=   BUTTON(gamefunc_Look_Down)       << SK_LOOK_DOWN;
    localInput.bits |=   BUTTON(gamefunc_NightVision)     << SK_NIGHTVISION;
    localInput.bits |=   BUTTON(gamefunc_MedKit)          << SK_MEDKIT;
    localInput.bits |=   BUTTON(gamefunc_Center_View)     << SK_CENTER_VIEW;
    localInput.bits |=   BUTTON(gamefunc_Holster_Weapon)  << SK_HOLSTER;
    localInput.bits |=   BUTTON(gamefunc_Inventory_Left)  << SK_INV_LEFT;
    localInput.bits |=   KB_KeyPressed(sc_Pause)          << SK_PAUSE;
    localInput.bits |=   BUTTON(gamefunc_Quick_Kick)      << SK_QUICK_KICK;
    localInput.bits |=   g_myAimMode                      << SK_AIMMODE;
    localInput.bits |=   BUTTON(gamefunc_Holo_Duke)       << SK_HOLODUKE;
    localInput.bits |=   BUTTON(gamefunc_Jetpack)         << SK_JETPACK;
    localInput.bits |=   ((int)g_gameQuit)                << SK_GAMEQUIT;
    localInput.bits |=   BUTTON(gamefunc_Inventory_Right) << SK_INV_RIGHT;
    localInput.bits |=   BUTTON(gamefunc_TurnAround)      << SK_TURNAROUND;
    localInput.bits |=   BUTTON(gamefunc_Inventory)       << SK_INVENTORY;
    localInput.bits |=   KB_KeyPressed(sc_Escape)         << SK_ESCAPE;

    localInput.extbits = 0;
    localInput.extbits |= (BUTTON(gamefunc_Move_Forward)  || (tempInput.fvel > 0)) ? EXTBIT_MOVEFORWARD  : 0;
    localInput.extbits |= (BUTTON(gamefunc_Move_Backward) || (tempInput.fvel < 0)) ? EXTBIT_MOVEBACKWARD : 0;
    localInput.extbits |= (BUTTON(gamefunc_Strafe_Left)   || (tempInput.svel > 0)) ? EXTBIT_STRAFELEFT   : 0;
    localInput.extbits |= (BUTTON(gamefunc_Strafe_Right)  || (tempInput.svel < 0)) ? EXTBIT_STRAFERIGHT  : 0;
    localInput.extbits |= BUTTON(gamefunc_Turn_Left)                               ? EXTBIT_TURNLEFT     : 0;
    localInput.extbits |= BUTTON(gamefunc_Turn_Right)                              ? EXTBIT_TURNRIGHT    : 0;
    localInput.extbits |= BUTTON(gamefunc_Alt_Fire)                                ? EXTBIT_ALTFIRE      : 0;

    // used for changing team
    localInput.extbits |= (g_player[snum].pteam != g_player[snum].ps->team) ? EXTBIT_CHANGETEAM : 0;

    if (ud.scrollmode && ud.overhead_on)
    {
        ud.folfvel = tempInput.fvel;
        ud.folavel = fix16_to_int(tempInput.q16avel);
        localInput.fvel = 0;
        localInput.svel = 0;
        localInput.q16avel = 0;
        localInput.q16horz = 0;
        return;
    }

    localInput.q16avel = tempInput.q16avel;
    localInput.q16horz = tempInput.q16horz;

    if (!uncapped)
    {
        int32_t playerAngle = 0;
        if (numplayers > 1)
            playerAngle = fix16_to_int(predictedPlayer.q16ang);
        else
            playerAngle = fix16_to_int(p->q16ang);

        localInput.fvel = mulscale9(tempInput.fvel, sintable[(playerAngle + 2560) & 2047]) +
                          mulscale9(tempInput.svel, sintable[(playerAngle + 2048) & 2047]);
        localInput.svel = mulscale9(tempInput.fvel, sintable[(playerAngle + 2048) & 2047]) +
                          mulscale9(tempInput.svel, sintable[(playerAngle + 1536) & 2047]);

        localInput.fvel += fricxv;
        localInput.svel += fricyv;
    }
}

static void P_ConsumeKeycard(int32_t playerNum, int32_t keypal)
{
    UNPREDICTABLE_FUNCTION;

    char keyname[7];

    if (ud.multimode < 2 || (ud.multimode > 1 && !(Gametypes[ud.coop].flags & GAMETYPE_ACCESSATSTART)))
    {
        switch (keypal)
        {
        case 0:
            Bsprintf(keyname, "Blue");
            ud.got_access &= (0xffff - 0x1);
            break;
        case 21:
            Bsprintf(keyname, "Red");
            ud.got_access &= (0xffff - 0x2);
            break;
        case 23:
            Bsprintf(keyname, "Yellow");
            ud.got_access &= (0xffff - 0x4);
            break;
        }
    }

    if (ud.multimode > 1 && !(Gametypes[ud.coop].flags & GAMETYPE_ACCESSATSTART))
    {
        Bsprintf(tempbuf, "^12%s^12 has used a ^%d%s^12 key.", &g_player[playerNum].user_name[0], keypal, keyname);
        G_AddUserQuote(tempbuf);
    }
}

static int P_DoCounters(DukePlayer_t *p)
{
    int snum = sprite[p->i].yvel;

    //    j = g_player[snum].inputBits->avel;
    //    p->weapon_ang = -(j/5);

    if (p->invdisptime > 0)
        p->invdisptime--;

    if (p->tipincs > 0) p->tipincs--;

    if (p->last_pissed_time > 0)
    {
        p->last_pissed_time--;

        if (p->last_pissed_time == (26*219))
        {
            A_PlaySound(FLUSH_TOILET,p->i);
            if (snum == screenpeek || GTFLAGS(GAMETYPE_COOPSOUND))
                A_PlaySound(DUKE_PISSRELIEF,p->i);
        }

        if (p->last_pissed_time == (26*218))
        {
            p->holster_weapon = 0;
            p->weapon_pos = 10;
        }
    }

    if (p->crack_time > 0)
    {
        p->crack_time--;
        if (p->crack_time == 0)
        {
            p->knuckle_incs = 1;
            p->crack_time = 777;
        }
    }

    if (p->inv_amount[GET_STEROIDS] > 0 && p->inv_amount[GET_STEROIDS] < 400)
    {
        p->inv_amount[GET_STEROIDS]--;
        if (p->inv_amount[GET_STEROIDS] == 0)
            P_SelectNextInvItem(p);
        if (!(p->inv_amount[GET_STEROIDS]&7))
            if (snum == screenpeek || GTFLAGS(GAMETYPE_COOPSOUND))
                A_PlaySound(DUKE_HARTBEAT,p->i);
    }

    if (p->heat_on && p->inv_amount[GET_HEATS] > 0)
    {
        p->inv_amount[GET_HEATS]--;
        if (p->inv_amount[GET_HEATS] == 0)
        {
            p->heat_on = 0;
            P_SelectNextInvItem(p);
            A_PlaySound(NITEVISION_ONOFF,p->i);
            P_UpdateScreenPal(p);
        }
    }

    if (p->holoduke_on >= 0)
    {
        p->inv_amount[GET_HOLODUKE]--;
        if (p->inv_amount[GET_HOLODUKE] <= 0)
        {
            A_PlaySound(TELEPORTER,p->i);
            p->holoduke_on = -1;
            P_SelectNextInvItem(p);
        }
    }

    if (p->jetpack_on && p->inv_amount[GET_JETPACK] > 0)
    {
        p->inv_amount[GET_JETPACK]--;
        if (p->inv_amount[GET_JETPACK] <= 0)
        {
            p->jetpack_on = 0;
            P_SelectNextInvItem(p);
            A_PlaySound(DUKE_JETPACK_OFF,p->i);
            S_StopEnvSound(DUKE_JETPACK_IDLE,p->i);
            S_StopEnvSound(DUKE_JETPACK_ON,p->i);
        }
    }

    if (p->quick_kick > 0 && sprite[p->i].pal != 1)
    {
        p->last_quick_kick = p->quick_kick+1;
        p->quick_kick--;
        if (p->quick_kick == 8)
            A_Shoot(p->i,KNEE);
    }
    else if (p->last_quick_kick > 0) p->last_quick_kick--;

    if (p->access_incs && sprite[p->i].pal != 1)
    {
        p->access_incs++;
        if (sprite[p->i].extra <= 0)
            p->access_incs = 12;
        if (p->access_incs == 12)
        {
            if (p->access_spritenum >= 0)
            {
                P_ActivateSwitch(snum, p->access_spritenum,1);
                P_ConsumeKeycard(snum, sprite[p->access_spritenum].pal);
                p->access_spritenum = -1;
            }
            else
            {
                P_ActivateSwitch(snum, p->access_wallnum,0);
                P_ConsumeKeycard(snum, wall[p->access_wallnum].pal);
            }
        }

        if (p->access_incs > 20)
        {
            p->access_incs = 0;
            p->weapon_pos = 10;
            p->kickback_pic = 0;
        }
    }

    if (p->cursectnum >= 0 && p->scuba_on == 0 && sector[p->cursectnum].lotag == ST_2_UNDERWATER)
    {
        if (p->inv_amount[GET_SCUBA] > 0)
        {
            p->scuba_on = 1;
            p->inven_icon = 6;
            P_DoQuote(QUOTE_SCUBA_ON,p);
        }
        else
        {
            if (p->airleft > 0)
                p->airleft--;
            else
            {
                p->extra_extra8 += 32;
                if (p->last_extra < (p->max_player_health>>1) && (p->last_extra&3) == 0)
                    A_PlaySound(DUKE_LONGTERM_PAIN,p->i);
            }
        }
    }
    else if (p->inv_amount[GET_SCUBA] > 0 && p->scuba_on)
    {
        p->inv_amount[GET_SCUBA]--;
        if (p->inv_amount[GET_SCUBA] == 0)
        {
            p->scuba_on = 0;
            P_SelectNextInvItem(p);
        }
    }

    if (p->knuckle_incs)
    {
        p->knuckle_incs ++;
        if (p->knuckle_incs==10)
        {
            if (!WW2GI)
            {
                if ((int32_t)totalclock > 1024)
                    if (snum == screenpeek || GTFLAGS(GAMETYPE_COOPSOUND))
                    {

                        if (rand()&1)
                            A_PlaySound(DUKE_CRACK,p->i);
                        else A_PlaySound(DUKE_CRACK2,p->i);

                    }

                A_PlaySound(DUKE_CRACK_FIRST,p->i);
            }
        }
        else if (p->knuckle_incs == 22 || TEST_SYNC_KEY(g_player[snum].inputBits->bits, SK_FIRE))
            p->knuckle_incs=0;

        return 1;
    }
    return 0;
}

int16_t WeaponPickupSprites[MAX_WEAPONS] = { KNEE__, FIRSTGUNSPRITE__, SHOTGUNSPRITE__,
        CHAINGUNSPRITE__, RPGSPRITE__, HEAVYHBOMB__, SHRINKERSPRITE__, DEVISTATORSPRITE__,
        TRIPBOMBSPRITE__, FREEZESPRITE__, HEAVYHBOMB__, SHRINKERSPRITE__
                                         };
// this is used for player deaths
void P_DropWeapon(DukePlayer_t *p)
{
    int snum = sprite[p->i].yvel, cw = aplWeaponWorksLike[p->curr_weapon][snum];

    if (cw < 1 || cw >= MAX_WEAPONS) return;

    if (cw)
    {
        if (krand()&1)
            A_Spawn(p->i,WeaponPickupSprites[cw]);
        else switch (cw)
            {
            case RPG_WEAPON:
            case HANDBOMB_WEAPON:
                A_Spawn(p->i,EXPLOSION2);
                break;
            }
    }
}

void P_UpdatePosWhenViewingCam(DukePlayer_t* pPlayer)
{
    UNPREDICTABLE_FUNCTION; // [JM] Might not need this anymore. We shall see.

    //int const newOwner = pPlayer->newowner;
    //pPlayer->pos = sprite[newOwner].pos;
    //pPlayer->q16ang = fix16_from_int(sprite[newOwner].ang);
    pPlayer->vel.x = 0;
    pPlayer->vel.y = 0;
    sprite[pPlayer->i].xvel = 0;
    pPlayer->look_ang = 0;
    pPlayer->rotscrnang = 0;
}

static void P_TossPipebomb(int playerNum)
{
    UNPREDICTABLE_FUNCTION;

    DukePlayer_t* p = g_player[playerNum].ps;
    unsigned int sb_snum = g_player[playerNum].inputBits->bits;

    int lPipeBombControl;
    int pipeBombZvel;
    int pipeBombFwdVel;

    if (p->on_ground && TEST_SYNC_KEY(sb_snum, SK_CROUCH))
    {
        pipeBombFwdVel = 15;
        pipeBombZvel = (fix16_to_int(p->q16horiz + p->q16horizoff - F16(100)) * 20);
    }
    else
    {
        pipeBombFwdVel = 140;
        pipeBombZvel = -512 - (fix16_to_int(p->q16horiz + p->q16horizoff - F16(100)) * 20);
    }

    int pipeSpriteNum = A_InsertSprite(p->cursectnum,
        p->pos.x + (sintable[(fix16_to_int(p->q16ang) + 512) & 2047] >> 6),
        p->pos.y + (sintable[fix16_to_int(p->q16ang) & 2047] >> 6),
        p->pos.z, aplWeaponShoots[p->curr_weapon][playerNum], -16, 9, 9,
        fix16_to_int(p->q16ang), (pipeBombFwdVel + (p->hbomb_hold_delay << 5)), pipeBombZvel, p->i, 1);

    lPipeBombControl = Gv_GetVarByLabel("PIPEBOMB_CONTROL", PIPEBOMB_REMOTE, -1, playerNum);

    if (lPipeBombControl & PIPEBOMB_TIMER)
    {
        int lGrenadeLifetime = Gv_GetVarByLabel("GRENADE_LIFETIME", NAM_GRENADE_LIFETIME, -1, playerNum);
        int lGrenadeLifetimeVar = Gv_GetVarByLabel("GRENADE_LIFETIME_VAR", NAM_GRENADE_LIFETIME_VAR, -1, playerNum);
        actor[pipeSpriteNum].t_data[7] = lGrenadeLifetime
            + mulscale(krand(), lGrenadeLifetimeVar, 14)
            - lGrenadeLifetimeVar;
        actor[pipeSpriteNum].t_data[PIPEBOMB_FUSETYPE] = FUSE_TIMED;
    }
    else
        actor[pipeSpriteNum].t_data[PIPEBOMB_FUSETYPE] = FUSE_REMOTE;

    if (pipeBombFwdVel == 15)
    {
        sprite[pipeSpriteNum].yvel = 3;
        sprite[pipeSpriteNum].z += (8 << 8);
    }

    if (A_GetHitscanRange(p->i) < 512)
    {
        sprite[pipeSpriteNum].ang += 1024;
        sprite[pipeSpriteNum].zvel /= 3;
        sprite[pipeSpriteNum].xvel /= 3;
    }
}

static void P_ProcessWeapon(int playerNum)
{
    DukePlayer_t* p = g_player[playerNum].ps;
    unsigned int sb_snum = g_player[playerNum].inputBits->bits;
    auto const kb = &p->kickback_pic;
    bool shrunk = (sprite[p->i].yrepeat < 32);

    if (p->weapon_pos != 0)
    {
        if (p->weapon_pos == -9)
        {
            if (p->last_weapon >= 0)
            {
                p->weapon_pos = 10;
                //                if(p->curr_weapon == KNEE_WEAPON) *kb = 1;
                p->last_weapon = -1;
            }
            else if (p->holster_weapon == 0)
                p->weapon_pos = 10;
        }
        else p->weapon_pos--;
    }

    if (TEST_SYNC_KEY(sb_snum, SK_FIRE))
        //    if ((sb_snum&(1<<2)))
    {
        Gv_SetVar(sysVarIDs.RETURN, 0, p->i, playerNum);
        X_OnEvent(EVENT_PRESSEDFIRE, p->i, playerNum, -1);
        if (Gv_GetVar(sysVarIDs.RETURN, p->i, playerNum) != 0)
            sb_snum &= ~BIT(SK_FIRE);
    }

    if (TEST_SYNC_KEY(sb_snum, SK_HOLSTER))   // 'Holster Weapon
    {
        Gv_SetVar(sysVarIDs.RETURN, 0, p->i, playerNum);

        P_SetWeaponGamevars(playerNum, p);

        X_OnEvent(EVENT_HOLSTER, p->i, playerNum, -1);
        if (Gv_GetVar(sysVarIDs.RETURN, p->i, playerNum) == 0)
        {
            if (PWEAPON(playerNum, p->curr_weapon, WorksLike) != KNEE_WEAPON)
            {
                if (p->holster_weapon == 0 && p->weapon_pos == 0)
                {
                    p->holster_weapon = 1;
                    p->weapon_pos = -1;
                    P_DoQuote(QUOTE_WEAPON_LOWERED, p);
                }
                else if (p->holster_weapon == 1 && p->weapon_pos == -9)
                {
                    p->holster_weapon = 0;
                    p->weapon_pos = 10;
                    P_DoQuote(QUOTE_WEAPON_RAISED, p);
                }
            }

            if (aplWeaponFlags[p->curr_weapon][playerNum] & WEAPON_HOLSTER_CLEARS_CLIP)
            {
                if (p->ammo_amount[p->curr_weapon] > aplWeaponClip[p->curr_weapon][playerNum]
                    && (p->ammo_amount[p->curr_weapon] % aplWeaponClip[p->curr_weapon][playerNum]) != 0)
                {
                    p->ammo_amount[p->curr_weapon] -=
                        p->ammo_amount[p->curr_weapon] % aplWeaponClip[p->curr_weapon][playerNum];
                    (*kb) = aplWeaponTotalTime[p->curr_weapon][playerNum];
                    sb_snum &= ~BIT(SK_FIRE); // not firing...
                }
                return;
            }
        }
    }

    if (aplWeaponFlags[p->curr_weapon][playerNum] & WEAPON_GLOWS)
        p->random_club_frame += 64; // Glowing

    // this is a hack for WEAPON_FIREEVERYOTHER
    if (actor[p->i].t_data[7])
    {
        actor[p->i].t_data[7]--;
        if (p->last_weapon == -1 && actor[p->i].t_data[7] != 0 && ((actor[p->i].t_data[7] & 1) == 0))
        {
            if (X_OnEventWithReturn(EVENT_PREWEAPONSHOOT, p->i, playerNum, 0) == 0)
            {
                if (aplWeaponFlags[p->curr_weapon][playerNum] & WEAPON_AMMOPERSHOT)
                {
                    if (p->ammo_amount[p->curr_weapon] > 0)
                        p->ammo_amount[p->curr_weapon]--;
                    else
                    {
                        actor[p->i].t_data[7] = 0;
                        P_CheckWeapon(p);
                    }
                }

                if (actor[p->i].t_data[7] != 0)
                {
                    A_Shoot(p->i, aplWeaponShoots[p->curr_weapon][playerNum]);
                    X_OnEvent(EVENT_POSTWEAPONSHOOT, p->i, playerNum, -1);
                }
            }
        }
    }

    if (p->rapid_fire_hold == 1)
    {
        if (TEST_SYNC_KEY(sb_snum, SK_FIRE)) return;
        p->rapid_fire_hold = 0;
    }

    bool const doFire = (sb_snum & BIT(SK_FIRE) && (*kb) == 0);
    bool const doAltFire = g_player[playerNum].inputBits->extbits & (EXTBIT_ALTFIRE);

    if (doAltFire)
    {
        P_SetWeaponGamevars(playerNum, p);
        X_OnEvent(EVENT_ALTFIRE, p->i, playerNum, -1);
    }

    if (shrunk || p->tipincs || p->access_incs)
    {
        sb_snum &= ~BIT(SK_FIRE);
    }
    else if (shrunk == 0 && doFire && p->fist_incs == 0 &&
        p->last_weapon == -1 && (p->weapon_pos == 0 || p->holster_weapon == 1))
    {
        p->crack_time = 777;

        if (p->holster_weapon == 1)
        {
            if (p->last_pissed_time <= (26 * 218) && p->weapon_pos == -9)
            {
                p->holster_weapon = 0;
                p->weapon_pos = 10;
                P_DoQuote(QUOTE_WEAPON_RAISED, p);
            }
        }
        else
        {
            Gv_SetVar(sysVarIDs.RETURN, 0, p->i, playerNum);

            P_SetWeaponGamevars(playerNum, p);

            X_OnEvent(EVENT_FIRE, p->i, playerNum, -1);
            if (Gv_GetVar(sysVarIDs.RETURN, p->i, playerNum) == 0)
            {
                switch (aplWeaponWorksLike[p->curr_weapon][playerNum])
                {
                case HANDBOMB_WEAPON:
                    X_OnEvent(EVENT_FIREWEAPON, p->i, playerNum, -1);

                    p->hbomb_hold_delay = 0;
                    if (p->ammo_amount[p->curr_weapon] > 0)
                    {
                        (*kb) = 1;
                        if (aplWeaponInitialSound[p->curr_weapon][playerNum])
                        {
                            A_PlaySound(aplWeaponInitialSound[p->curr_weapon][playerNum], p->i);
                        }
                    }
                    break;

                case HANDREMOTE_WEAPON:
                    X_OnEvent(EVENT_FIREWEAPON, p->i, playerNum, -1);
                    p->hbomb_hold_delay = 0;
                    (*kb) = 1;
                    if (aplWeaponInitialSound[p->curr_weapon][playerNum])
                    {
                        A_PlaySound(aplWeaponInitialSound[p->curr_weapon][playerNum], p->i);
                    }
                    break;

                case SHOTGUN_WEAPON:
                    X_OnEvent(EVENT_FIREWEAPON, p->i, playerNum, -1);
                    if (p->ammo_amount[p->curr_weapon] > 0)
                    {
                        (*kb) = 1;
                        if (aplWeaponInitialSound[p->curr_weapon][playerNum])
                        {
                            A_PlaySound(aplWeaponInitialSound[p->curr_weapon][playerNum], p->i);
                        }
                    }
                    break;

                case TRIPBOMB_WEAPON:
                    if (p->ammo_amount[p->curr_weapon] > 0)
                    {
                        hitdata_t hit;

                        hitscan(&p->pos,
                            p->cursectnum, sintable[(fix16_to_int(p->q16ang) + 512) & 2047],
                            sintable[fix16_to_int(p->q16ang) & 2047], fix16_to_int(F16(100) - p->q16horiz - p->q16horizoff) * 32,
                            &hit, CLIPMASK1);

                        if (hit.sect < 0 || hit.sprite >= 0)
                            break;

                        if (hit.wall >= 0 && sector[hit.sect].lotag > 2)
                            break;

                        if (hit.wall >= 0 && wall[hit.wall].overpicnum >= 0)
                            if (wall[hit.wall].overpicnum == BIGFORCE)
                                break;

                        int spr = headspritesect[hit.sect];
                        while (spr >= 0)
                        {
                            if (sprite[spr].picnum == TRIPBOMB &&
                                klabs(sprite[spr].z - hit.z) < (12 << 8) && ((sprite[spr].x - hit.x) * (sprite[spr].x - hit.x) + (sprite[spr].y - hit.y) * (sprite[spr].y - hit.y)) < (290 * 290))
                                break;
                            spr = nextspritesect[spr];
                        }

                        if (spr == -1 && hit.wall >= 0 && (wall[hit.wall].cstat & 16) == 0)
                            if ((wall[hit.wall].nextsector >= 0 && sector[wall[hit.wall].nextsector].lotag <= 2) || (wall[hit.wall].nextsector == -1 && sector[hit.sect].lotag <= 2))
                                if (((hit.x - p->pos.x) * (hit.x - p->pos.x) + (hit.y - p->pos.y) * (hit.y - p->pos.y)) < (290 * 290))
                                {
                                    p->pos.z = p->opos.z;
                                    p->vel.z = 0;
                                    (*kb) = 1;
                                    if (aplWeaponInitialSound[p->curr_weapon][playerNum])
                                    {
                                        A_PlaySound(aplWeaponInitialSound[p->curr_weapon][playerNum], p->i);
                                    }
                                }
                    }
                    break;

                case PISTOL_WEAPON:
                case CHAINGUN_WEAPON:
                case SHRINKER_WEAPON:
                case GROW_WEAPON:
                case FREEZE_WEAPON:
                case RPG_WEAPON:
                    X_OnEvent(EVENT_FIREWEAPON, p->i, playerNum, -1);
                    if (p->ammo_amount[p->curr_weapon] > 0)
                    {
                        (*kb) = 1;
                        if (aplWeaponInitialSound[p->curr_weapon][playerNum])
                        {
                            A_PlaySound(aplWeaponInitialSound[p->curr_weapon][playerNum], p->i);
                        }
                    }
                    break;

                case DEVISTATOR_WEAPON:
                    X_OnEvent(EVENT_FIREWEAPON, p->i, playerNum, -1);
                    if (p->ammo_amount[p->curr_weapon] > 0)
                    {
                        (*kb) = 1;
                        p->hbomb_hold_delay = !p->hbomb_hold_delay;
                        if (aplWeaponInitialSound[p->curr_weapon][playerNum])
                        {
                            A_PlaySound(aplWeaponInitialSound[p->curr_weapon][playerNum], p->i);
                        }
                    }
                    break;

                case KNEE_WEAPON:
                    X_OnEvent(EVENT_FIREWEAPON, p->i, playerNum, -1);
                    if (p->quick_kick == 0 || DMFLAGS_TEST(DMFLAG_ALLOWDOUBLEKICK))
                    {
                        (*kb) = 1;
                        if (aplWeaponInitialSound[p->curr_weapon][playerNum])
                        {
                            A_PlaySound(aplWeaponInitialSound[p->curr_weapon][playerNum], p->i);
                        }
                    }
                    break;
                }
            }
        }
    }
    else if (*kb)
    {
        if (aplWeaponWorksLike[p->curr_weapon][playerNum] == HANDBOMB_WEAPON)
        {
            if (aplWeaponHoldDelay[p->curr_weapon][playerNum] && ((*kb) == aplWeaponFireDelay[p->curr_weapon][playerNum]) && TEST_SYNC_KEY(sb_snum, SK_FIRE))
            {
                p->rapid_fire_hold = 1;
                return;
            }

            (*kb)++; // Increment Kickback Pic

            if ((*kb) == aplWeaponHoldDelay[p->curr_weapon][playerNum])
            {
                p->ammo_amount[p->curr_weapon]--;

                P_TossPipebomb(playerNum);

                p->hbomb_on = 1;
            }
            else if ((*kb) < aplWeaponHoldDelay[p->curr_weapon][playerNum] && TEST_SYNC_KEY(sb_snum, SK_FIRE))
            {
                p->hbomb_hold_delay++;
            }
            else if ((*kb) > aplWeaponTotalTime[p->curr_weapon][playerNum])
            {
                int lPipeBombControl = Gv_GetVarByLabel("PIPEBOMB_CONTROL", PIPEBOMB_REMOTE, -1, playerNum);

                (*kb) = 0;

                if (lPipeBombControl == PIPEBOMB_REMOTE)
                {
                    p->curr_weapon = HANDREMOTE_WEAPON;
                    p->last_weapon = -1;
                    p->weapon_pos = 10;
                    Gv_SetVar(sysVarIDs.WEAPON, p->curr_weapon, p->i, playerNum);
                }
                else
                {
                     if (!NAM_WW2GI)
                        p->weapon_pos = 10;
                    P_CheckWeapon(p);
                }
            }
        }
        else if (aplWeaponWorksLike[p->curr_weapon][playerNum] == HANDREMOTE_WEAPON)
        {
            (*kb)++; // Increment Kickback Pic

            if ((*kb) == aplWeaponFireDelay[p->curr_weapon][playerNum])
            {
                if (aplWeaponFlags[p->curr_weapon][playerNum] & WEAPON_BOMB_TRIGGER)
                {
                    p->hbomb_on = 0;
                }
                if (aplWeaponShoots[p->curr_weapon][playerNum] != 0)
                {
                    if (!(aplWeaponFlags[p->curr_weapon][playerNum] & WEAPON_NOVISIBLE))
                    {
                        if (Net_InPredictableState())
                            g_player[playerNum].display.visibility = 0;
                    }

                    P_SetWeaponGamevars(playerNum, p);

                    A_Shoot(p->i, aplWeaponShoots[p->curr_weapon][playerNum]);
                }
            }

            if ((*kb) >= aplWeaponTotalTime[p->curr_weapon][playerNum])
            {
                int lPipeBombControl = Gv_GetVarByLabel("PIPEBOMB_CONTROL", PIPEBOMB_REMOTE, -1, playerNum);
                (*kb) = 0;
                if ((p->ammo_amount[HANDBOMB_WEAPON] > 0) && lPipeBombControl == PIPEBOMB_REMOTE)
                    P_AddWeapon(p, HANDBOMB_WEAPON, 1);
                else
                    P_CheckWeapon(p);
            }
        }
        else
        {
            // the basic weapon...
            (*kb)++; // Increment Kickback Pic

            if (aplWeaponFlags[p->curr_weapon][playerNum] & WEAPON_CHECKATRELOAD)
            {
                if (aplWeaponWorksLike[p->curr_weapon][playerNum] == TRIPBOMB_WEAPON)
                {
                    if ((*kb) >= aplWeaponTotalTime[p->curr_weapon][playerNum])
                    {
                        (*kb) = 0;
                        P_CheckWeapon(p);
                        p->weapon_pos = -9;
                    }
                }
                else if (*kb >= aplWeaponReload[p->curr_weapon][playerNum])
                    P_CheckWeapon(p);
            }
            else if (aplWeaponWorksLike[p->curr_weapon][playerNum] != KNEE_WEAPON && *kb >= aplWeaponFireDelay[p->curr_weapon][playerNum])
                P_CheckWeapon(p);

            if (aplWeaponFlags[p->curr_weapon][playerNum] & WEAPON_STANDSTILL
                && *kb < (aplWeaponFireDelay[p->curr_weapon][playerNum] + 1))
            {
                p->pos.z = p->opos.z;
                p->vel.z = 0;
            }
            if (*kb == aplWeaponSound2Time[p->curr_weapon][playerNum])
            {
                if (aplWeaponSound2Sound[p->curr_weapon][playerNum])
                {
                    A_PlaySound(aplWeaponSound2Sound[p->curr_weapon][playerNum], p->i);
                }
            }
            if (*kb == aplWeaponSpawnTime[p->curr_weapon][playerNum])
                P_DoWeaponSpawn(playerNum);

            if ((*kb) >= aplWeaponTotalTime[p->curr_weapon][playerNum])
            {
                if (/*!(aplWeaponFlags[p->curr_weapon][snum] & WEAPON_CHECKATRELOAD) && */ p->reloading == 1 ||
                    (aplWeaponReload[p->curr_weapon][playerNum] > aplWeaponTotalTime[p->curr_weapon][playerNum] && p->ammo_amount[p->curr_weapon] > 0
                        && (aplWeaponClip[p->curr_weapon][playerNum]) && (((p->ammo_amount[p->curr_weapon] % (aplWeaponClip[p->curr_weapon][playerNum])) == 0))))
                {
                    int i = aplWeaponReload[p->curr_weapon][playerNum] - aplWeaponTotalTime[p->curr_weapon][playerNum];

                    p->reloading = 1;

                    if ((*kb) != (aplWeaponTotalTime[p->curr_weapon][playerNum]))
                    {
                        if ((*kb) == (aplWeaponTotalTime[p->curr_weapon][playerNum] + 1))
                        {
                            if (aplWeaponReloadSound1[p->curr_weapon][playerNum])
                                A_PlaySound(aplWeaponReloadSound1[p->curr_weapon][playerNum], p->i);
                        }
                        else if (((*kb) == (aplWeaponReload[p->curr_weapon][playerNum] - (i / 3)) && !(aplWeaponFlags[p->curr_weapon][playerNum] & WEAPON_RELOAD_TIMING)) ||
                            ((*kb) == (aplWeaponReload[p->curr_weapon][playerNum] - i + 4) && (aplWeaponFlags[p->curr_weapon][playerNum] & WEAPON_RELOAD_TIMING)))
                        {
                            if (aplWeaponReloadSound2[p->curr_weapon][playerNum])
                                A_PlaySound(aplWeaponReloadSound2[p->curr_weapon][playerNum], p->i);
                        }
                        else if ((*kb) >= (aplWeaponReload[p->curr_weapon][playerNum]))
                        {
                            *kb = 0;
                            p->reloading = 0;
                        }
                    }
                }
                else
                {
                    if (aplWeaponFlags[p->curr_weapon][playerNum] & WEAPON_AUTOMATIC &&
                        (aplWeaponWorksLike[p->curr_weapon][playerNum] == KNEE_WEAPON ? 1 : p->ammo_amount[p->curr_weapon] > 0))
                    {
                        if (TEST_SYNC_KEY(sb_snum, SK_FIRE))
                        {
                            if (aplWeaponFlags[p->curr_weapon][playerNum] & WEAPON_RANDOMRESTART)
                                *kb = 1 + (seed_krand(&g_random.playerweapon) & 3);
                            else *kb = 1;
                        }
                        else *kb = 0;
                    }
                    else *kb = 0;

                    if (aplWeaponFlags[p->curr_weapon][playerNum] & WEAPON_RESET &&
                        ((aplWeaponWorksLike[p->curr_weapon][playerNum] == KNEE_WEAPON) ? 1 : p->ammo_amount[p->curr_weapon] > 0))
                    {
                        if (TEST_SYNC_KEY(sb_snum, SK_FIRE)) *kb = 1;
                        else *kb = 0;
                    }
                }
            }
            else if (*kb >= aplWeaponFireDelay[p->curr_weapon][playerNum] && (*kb) < aplWeaponTotalTime[p->curr_weapon][playerNum]
                && ((aplWeaponWorksLike[p->curr_weapon][playerNum] == KNEE_WEAPON) ? 1 : p->ammo_amount[p->curr_weapon] > 0))
            {
                if (aplWeaponFlags[p->curr_weapon][playerNum] & WEAPON_AUTOMATIC)
                {
                    if (!(aplWeaponFlags[p->curr_weapon][playerNum] & WEAPON_SEMIAUTO))
                    {
                        if (TEST_SYNC_KEY(sb_snum, SK_FIRE) == 0 && aplWeaponFlags[p->curr_weapon][playerNum] & WEAPON_RESET)
                            *kb = 0;
                        if (aplWeaponFlags[p->curr_weapon][playerNum] & WEAPON_FIREEVERYTHIRD)
                        {
                            if (((*(kb)) % 3) == 0)
                            {
                                P_FireWeapon(p);
                                P_DoWeaponSpawn(playerNum);
                            }
                        }
                        else if (aplWeaponFlags[p->curr_weapon][playerNum] & WEAPON_FIREEVERYOTHER)
                        {
                            P_FireWeapon(p);
                            P_DoWeaponSpawn(playerNum);
                        }
                        else
                        {
                            if (*kb == aplWeaponFireDelay[p->curr_weapon][playerNum])
                            {
                                P_FireWeapon(p);
                                //                                P_DoWeaponSpawn(playerNum);
                            }
                        }
                        if (aplWeaponFlags[p->curr_weapon][playerNum] & WEAPON_RESET &&
                            (*kb) > aplWeaponTotalTime[p->curr_weapon][playerNum] - aplWeaponHoldDelay[p->curr_weapon][playerNum] &&
                            ((aplWeaponWorksLike[p->curr_weapon][playerNum] == KNEE_WEAPON) ? 1 : p->ammo_amount[p->curr_weapon] > 0))
                        {
                            if (TEST_SYNC_KEY(sb_snum, SK_FIRE)) *kb = 1;
                            else *kb = 0;
                        }
                    }
                    else
                    {
                        if (aplWeaponFlags[p->curr_weapon][playerNum] & WEAPON_FIREEVERYOTHER)
                        {
                            P_FireWeapon(p);
                            P_DoWeaponSpawn(playerNum);
                        }
                        else
                        {
                            if (*kb == aplWeaponFireDelay[p->curr_weapon][playerNum])
                            {
                                P_FireWeapon(p);
                                //                                P_DoWeaponSpawn(playerNum);
                            }
                        }
                    }
                }
                else if (*kb == aplWeaponFireDelay[p->curr_weapon][playerNum])
                    P_FireWeapon(p);
            }
        }
    }
}

int g_numObituaries = 0;
int g_numSelfObituaries = 0;

static int P_HandleSpecialTimers(int playerNum) // Ret: 0 = Pass, 1 = Halt ProcessInput
{
    UNPREDICTABLE_FUNCTION(0);

    DukePlayer_t* p = g_player[playerNum].ps;

    p->player_par++;

    if (p->timebeforeexit > 1 && ((p->last_extra > 0) || ud.limit_hit == 1))
    {
        p->timebeforeexit--;
        if (p->timebeforeexit == GAMETICSPERSEC * 5 && (ud.limit_hit == 0))
        {
            FX_StopAllSounds();

            if (p->customexitsound >= 0)
            {
                S_PlaySound(p->customexitsound);
                P_DoQuote(QUOTE_WEREGONNAFRYYOURASS, p);
            }
        }
        else if (p->timebeforeexit == 1)
        {
            Net_EndOfLevel(false);
            return 1;
        }
    }

    if (p->loogcnt > 0) p->loogcnt--;
    else p->loogcnt = 0;

    if (p->pals.f > 0)
        p->pals.f--;

    if (p->fta > 0)
    {
        p->fta--;
        if (p->fta == 0)
        {
            pub = NUMPAGES;
            pus = NUMPAGES;
            p->ftq = 0;
        }
    }

    if (g_levelTextTime > 0)
        g_levelTextTime--;

    return 0;
}

static int P_HandleFist(int playerNum)
{
    UNPREDICTABLE_FUNCTION(0);

    DukePlayer_t* p = g_player[playerNum].ps;

    if (p->fist_incs)
    {
        // the fist puching the end-of-level thing...
        p->fist_incs++;
        if (p->fist_incs == 28)
        {
            if (ud.recstat == 1)
                G_CloseDemoWrite();

            S_PlaySound(PIPEBOMB_EXPLODE);
            P_PalFrom(p, 48, 64, 64, 64);
        }
        if (p->fist_incs > 42)
        {
            Net_EndOfLevel((p->buttonpalette && ud.from_bonus == 0));
            p->fist_incs = 0;

            return 1;
        }
    }

    return 0;
}

static void P_HandleFragged(int snum)
{
    UNPREDICTABLE_FUNCTION;

    DukePlayer_t* p = g_player[snum].ps;
    DukePlayer_t* frag_p = g_player[p->frag_ps].ps;
    int playerSprite = g_player[snum].ps->i;

    if (ud.limit_hit == 1 || (X_OnEventWithReturn(EVENT_FRAGGED, playerSprite, snum, frag_p->i) == -1))
        return;

    if (GTFLAGS(GAMETYPE_LIMITEDLIVES))
        p->lives--;

    p->deaths++;

    if (p->frag_ps != snum)
    {
        if (GTFLAGS(GAMETYPE_TDM) && frag_p->team == g_player[snum].ps->team)
        {
            frag_p->fraggedself++;
            teams[frag_p->team].suicides++;
        }
        else
        {
            frag_p->frag++;
            g_player[p->frag_ps].frags[snum]++;
            teams[frag_p->team].frags++;
        }

        G_AddKillToFeed(snum, p->frag_ps);

        if (ud.obituaries)
        {
            //DynamicTileMap[actor[p->i].picnum]
            Bsprintf(tempbuf, apStrings[OBITQUOTEINDEX + (krand() % g_numObituaries)],
                &g_player[p->frag_ps].user_name[0],
                &g_player[snum].user_name[0]);

            G_AddUserQuote(tempbuf);
        }
        else krand();

        if (GTFLAGS(GAMETYPE_TDM))
        {
            int fraggedbyteam = frag_p->team;

            if ((teams[fraggedbyteam].frags - teams[fraggedbyteam].suicides) >= ud.fraglimit && ud.fraglimit != 0)
            {
                // Fraglimit hit
                Bsprintf(tempbuf, "^%02d%s team ^08wins!", teams[fraggedbyteam].palnum % 100, teams[fraggedbyteam].name);
                G_AddUserQuote(tempbuf);

                S_PlaySound(PIPEBOMB_EXPLODE);

                for (int32_t ALL_PLAYERS(i))
                {
                    if (g_player[i].ps->team == fraggedbyteam) // Flash screens of all winning players
                        P_PalFrom(g_player[i].ps, 48, 64, 64, 64);
                }

                ud.from_bonus = 0;
                ud.limit_hit = 1;
                ud.winner = fraggedbyteam;
                frag_p->timebeforeexit = GAMETICSPERSEC * 8;
            }
        }
        else if (!GTFLAGS(GAMETYPE_COOP) && (frag_p->frag - frag_p->fraggedself) >= ud.fraglimit && ud.fraglimit != 0)
        {
            // Fraglimit hit
            Bsprintf(tempbuf, "^08%s ^08wins!", &g_player[p->frag_ps].user_name[0]);
            G_AddUserQuote(tempbuf);

            S_PlaySound(PIPEBOMB_EXPLODE);
            P_PalFrom(frag_p, 48, 64, 64, 64);

            ud.from_bonus = 0;
            ud.limit_hit = 1;
            ud.winner = p->frag_ps;
            frag_p->timebeforeexit = GAMETICSPERSEC * 8;
        }
    }
    else
    {
        if (actor[playerSprite].htpicnum != APLAYERTOP)
        {
            if (GTFLAGS(GAMETYPE_TDM))
            {
                teams[p->team].suicides++;
            }

            p->fraggedself++;
            if ((unsigned)p->wackedbyactor < MAXSPRITES && A_CheckEnemyTile(sprite[p->wackedbyactor].picnum))
                Bsprintf(tempbuf, apStrings[OBITQUOTEINDEX + (krand() % g_numObituaries)], "A monster", &g_player[snum].user_name[0]);
            else if (actor[playerSprite].htpicnum == NUKEBUTTON)
                Bsprintf(tempbuf, "^02%s^02 tried to leave", &g_player[snum].user_name[0]);
            else
            {
                // random suicide death string
                Bsprintf(tempbuf, apStrings[SUICIDEQUOTEINDEX + (krand() % g_numSelfObituaries)], &g_player[snum].user_name[0]);
            }
        }
        else Bsprintf(tempbuf, "^02%s^02 switched to team ^%02d%s", &g_player[snum].user_name[0], teams[p->team].palnum % 100, teams[p->team].name);

        if (ud.obituaries)
        {
            G_AddUserQuote(tempbuf);
        }
    }

    p->frag_ps = snum;
    pus = NUMPAGES;
}

static void P_HandleDeath(int snum)
{
    DukePlayer_t* p = g_player[snum].ps;
    int playerSprite = g_player[snum].ps->i;

    spritetype* s = &sprite[playerSprite];

    if (p->dead_flag == 0)
    {
        if (s->pal != 1) // Not frozen
        {
            if (!oldnet_predicting)
            {
                oldnet_predictcontext = NOT_PREDICTABLE;
                P_PalFrom(p, 63, 63, 0, 0);
                oldnet_predictcontext = snum;
            }

            p->pos.z -= (16 << 8);
            s->z -= (16 << 8);

            p->dead_flag = (512 - ((seed_krand(&g_random.player) & 1) << 10) + (seed_krand(&g_random.player) & 255) - 512) & 2047;
            if (p->dead_flag == 0)
                p->dead_flag++;

            if((s->cstat & 32768) == 0)
                s->cstat = 0;
        }

        if (ud.recstat == 1 && ud.multimode < 2)
            G_CloseDemoWrite();

        p->jetpack_on = 0;
        p->holoduke_on = -1;

        S_StopEnvSound(DUKE_JETPACK_IDLE, playerSprite);
        if (p->scream_voice > FX_Ok)
        {
            FX_StopSound(p->scream_voice);
            S_Cleanup();
            // S_TestSoundCallback(DUKE_SCREAM);
            p->scream_voice = FX_Ok;
        }

        if (ud.multimode > 1 && (s->pal != 1 || (s->cstat & 32768)))
            P_HandleFragged(snum);
    }
}

static void P_HandleDamagingFloors(int snum)
{
    UNPREDICTABLE_FUNCTION;

    DukePlayer_t* p = g_player[snum].ps;
    int playerSprite = g_player[snum].ps->i;

    spritetype* s = &sprite[playerSprite];

    int truefdist = klabs(p->pos.z - p->truefz);
    int floorPic = sector[s->sectnum].floorpicnum;
    int useBoot = 0;

    if (floorPic == PURPLELAVA || sector[s->sectnum].ceilingpicnum == PURPLELAVA)
    {
        if (p->inv_amount[GET_BOOTS] > 0)
        {
            p->inv_amount[GET_BOOTS]--;
            p->inven_icon = 7;
            if (p->inv_amount[GET_BOOTS] <= 0)
                P_SelectNextInvItem(p);
        }
        else
        {
            if (!A_CheckSoundPlaying(playerSprite, DUKE_LONGTERM_PAIN))
                A_PlaySound(DUKE_LONGTERM_PAIN, playerSprite);

            P_PalFrom(p, 32, 0, 8, 0);
            s->extra--;
        }
    }

    if (p->on_ground && truefdist <= PHEIGHT + (16 << 8))
    {
        switch (tileGetMapping(floorPic))
        {
        case HURTRAIL__:
            if (rnd(32))
            {
                if (p->inv_amount[GET_BOOTS] > 0)
                    useBoot = 1;
                else
                {
                    if (!A_CheckSoundPlaying(playerSprite, DUKE_LONGTERM_PAIN))
                        A_PlaySound(DUKE_LONGTERM_PAIN, playerSprite);

                    P_PalFrom(p, 32, 64, 64, 64);
                    s->extra -= 1 + (krand() & 3);
                    if (!A_CheckSoundPlaying(playerSprite, SHORT_CIRCUIT))
                        A_PlaySound(SHORT_CIRCUIT, playerSprite);
                }
            }
            break;
        case FLOORSLIME__:
            if (rnd(16))
            {
                if (p->inv_amount[GET_BOOTS] > 0)
                    useBoot = 1;
                else
                {
                    if (!A_CheckSoundPlaying(playerSprite, DUKE_LONGTERM_PAIN))
                        A_PlaySound(DUKE_LONGTERM_PAIN, playerSprite);

                    P_PalFrom(p, 32, 0, 8, 0);
                    s->extra -= 1 + (krand() & 3);
                }
            }
            break;
        case FLOORPLASMA__:
            if (rnd(32))
            {
                if (p->inv_amount[GET_BOOTS] > 0)
                    useBoot = 1;
                else
                {
                    if (!A_CheckSoundPlaying(playerSprite, DUKE_LONGTERM_PAIN))
                        A_PlaySound(DUKE_LONGTERM_PAIN, playerSprite);

                    P_PalFrom(p, 32, 8, 0, 0);
                    s->extra -= 1 + (krand() & 3);
                }
            }
            break;
        }
    }

    if (useBoot)
    {
        P_DoQuote(QUOTE_BOOTS_ON, p);
        p->inv_amount[GET_BOOTS] -= 2;
        if (p->inv_amount[GET_BOOTS] <= 0)
            P_SelectNextInvItem(p);
    }
}

static bool P_DoSpriteBridge(int32_t spriteNum, DukePlayer_t* pPlayer)
{
    if ((sprite[spriteNum].cstat & 33) == 33)
    {
        pPlayer->footprintcount = 0;
        pPlayer->spritebridge = 1;
        pPlayer->sbs = spriteNum;
        return true;
    }
    else if (A_CheckEnemySprite(&sprite[spriteNum]) && sprite[spriteNum].xrepeat > 24 && klabs(sprite[pPlayer->i].z - sprite[spriteNum].z) < (84 << 8))
    {
        int ang = getangle(sprite[spriteNum].x - pPlayer->pos.x, sprite[spriteNum].y - pPlayer->pos.y);
        pPlayer->vel.x -= sintable[(ang + 512) & 2047] << 4;
        pPlayer->vel.y -= sintable[ang & 2047] << 4;
    }

    return false;
}

static void P_CheckTouchDamage(DukePlayer_t* p, int j)
{
    UNPREDICTABLE_FUNCTION;

    if ((j = X_OnEventWithReturn(EVENT_CHECKTOUCHDAMAGE, p->i, sprite[p->i].yvel, j)) == -1)
        return;

    if ((j & 49152) == 49152)
    {
        j &= (MAXSPRITES - 1);

        if (sprite[j].picnum == CACTUS)
        {

            if (p->hurt_delay < 8)
            {
                sprite[p->i].extra -= 5;

                p->hurt_delay = 16;

                P_PalFrom(p, 32, 32, 0, 0);
                A_PlaySound(DUKE_LONGTERM_PAIN, p->i);
            }

        }
        return;
    }

    if ((j & 49152) != 32768) return;
    j &= (MAXWALLS - 1);

    if (p->hurt_delay > 0) p->hurt_delay--;
    else if (wall[j].cstat & 85)
    {
        int switchpicnum = wall[j].overpicnum;
        if ((switchpicnum > W_FORCEFIELD) && (switchpicnum <= W_FORCEFIELD + 2))
            switchpicnum = W_FORCEFIELD;

        switch (tileGetMapping(switchpicnum))
        {
        case W_FORCEFIELD__:
            //        case W_FORCEFIELD+1:
            //        case W_FORCEFIELD+2:
            sprite[p->i].extra -= 5;

            p->hurt_delay = 16;

            P_PalFrom(p, 32, 32, 0, 0);

            p->vel.x = -(sintable[(fix16_to_int(p->q16ang) + 512) & 2047] << 8);
            p->vel.y = -(sintable[fix16_to_int(p->q16ang) & 2047] << 8);
            A_PlaySound(DUKE_LONGTERM_PAIN, p->i);

            A_DamageWall(p->i, j,
                { p->pos.x + (sintable[(fix16_to_int(p->q16ang) + 512) & 2047] >> 9),
                  p->pos.y + (sintable[fix16_to_int(p->q16ang) & 2047] >> 9),
                  p->pos.z }, -1);

            break;

        case BIGFORCE__:
            p->hurt_delay = 26;
            A_DamageWall(p->i, j,
                { p->pos.x + (sintable[(fix16_to_int(p->q16ang) + 512) & 2047] >> 9),
                  p->pos.y + (sintable[fix16_to_int(p->q16ang) & 2047] >> 9),
                  p->pos.z }, -1);
            break;

        }
    }
}

static void P_DoJetpack(int const playerNum, int const playerBits, int const playerShrunk, int const sectorLotag, int const floorZ)
{
    auto p = g_player[playerNum].ps;

    p->on_ground = 0;
    p->jumping_counter = 0;
    p->hard_landing = 0;
    p->falling_counter = 0;

    if (p->jetpack_on < 11)
    {
        p->jetpack_on++;
        p->pos.z -= (p->jetpack_on << 7); //Goin up
    }
    else if (p->jetpack_on == 11 && !A_CheckSoundPlaying(p->i, DUKE_JETPACK_IDLE))
        A_PlaySound(DUKE_JETPACK_IDLE, p->i);

    int const zAdjust = playerShrunk ? 512 : 2048;

    if (TEST_SYNC_KEY(playerBits, SK_JUMP))         //A (soar high)
    {
        // jump
        Gv_SetVar(sysVarIDs.RETURN, 0, p->i, playerNum);
        X_OnEvent(EVENT_SOARUP, p->i, playerNum, -1);
        if (Gv_GetVar(sysVarIDs.RETURN, p->i, playerNum) == 0)
        {
            p->pos.z -= zAdjust;
            p->crack_time = 777;
        }
    }

    if (TEST_SYNC_KEY(playerBits, SK_CROUCH))   //Z (soar low)
    {
        // crouch
        Gv_SetVar(sysVarIDs.RETURN, 0, p->i, playerNum);
        X_OnEvent(EVENT_SOARDOWN, p->i, playerNum, -1);
        if (Gv_GetVar(sysVarIDs.RETURN, p->i, playerNum) == 0)
        {
            p->pos.z += zAdjust;
            p->crack_time = 777;
        }
    }

    int const Zdiff = (playerShrunk == 0 && (sectorLotag == ST_0_NO_EFFECT || sectorLotag == ST_2_UNDERWATER)) ? 32 : 16;

    if (sectorLotag != ST_2_UNDERWATER && p->scuba_on == 1)
        p->scuba_on = 0;

    if (p->pos.z > (floorZ - (Zdiff << 8)))
        p->pos.z += ((floorZ - (Zdiff << 8)) - p->pos.z) >> 1;

    if (p->pos.z < (actor[p->i].ceilingz + (18 << 8)))
        p->pos.z = actor[p->i].ceilingz + (18 << 8);
}

void P_ProcessInput(int snum)
{
    SET_PREDICTION_CONTEXT(snum);

    DukePlayer_t* p = g_player[snum].ps;

    input_t* pInput = g_player[snum].inputBits;
    uint32_t playerBits = pInput->bits;

    int playerSprite = p->i;
    spritetype* s = &sprite[playerSprite];

    int shrunk = (s->yrepeat < 32);
    int floorZOffset;

    int ceilZ, highZhit, floorZ, lowZhit;

    int x, y;

    int doubvel;
    bool doFloatbob = false;

    p->orotscrnang = p->rotscrnang;

    X_OnEvent(EVENT_PROCESSINPUT, playerSprite, snum, -1);

    if (p->cheat_phase > 0)
        playerBits = 0;

    if (p->cursectnum == -1)
    {
        if (s->extra > 0 && ud.noclip == 0)
        {
            P_QuickKill(p);
            A_PlaySound(SQUISHED, playerSprite);
        }

        p->cursectnum = 0;
    }

    int16_t sectorLotag = sector[p->cursectnum].lotag;
    p->spritebridge = 0;
    p->sbs = 0;
    p->lowsprite = -1;
    p->highsprite = -1;

    getzrange(&p->pos, p->cursectnum, &ceilZ, &highZhit, &floorZ, &lowZhit, 163L, CLIPMASK0);
    yax_getzsofslope(p->cursectnum, p->pos.x, p->pos.y, &p->truecz, &p->truefz);

    int const trueFloorDist = klabs(p->pos.z - p->truefz);

    /* wtf is this supposed to do? */
    if ((lowZhit & 49152) == 16384 && sectorLotag == ST_1_ABOVE_WATER && trueFloorDist > PHEIGHT + (16 << 8))
        sectorLotag = 0;

    // HACK HACK HACK: Big brutal hack to adjust player z if TROR water is too shallow.
    bool noSE7Water = false;
    int32_t waterFloor = 0;
    int32_t waterZhit = 0;
    int32_t otherSect;
    if ((sectorLotag == ST_1_ABOVE_WATER) && A_CheckNoSE7Water((uspriteptr_t)s, p->cursectnum, sectorLotag, &otherSect))
    {
        int32_t dummy;
        getzrange(&p->pos, otherSect, &dummy, &dummy, &waterFloor, &waterZhit, 163L, CLIPMASK0);

        // HACK: Force truefz to the floor of the other sector. Need this to prevent camera bullshit.
        yax_getzsofslope(otherSect, p->pos.x, p->pos.y, &dummy, &p->truefz);
        noSE7Water = true;
    }

    // HACK: Force actor floorz to waterFloor in TROR water to prevent sprite fuckery.
    actor[playerSprite].floorz = (noSE7Water) ? waterFloor : floorZ;
    actor[playerSprite].ceilingz = ceilZ;

    p->oq16horiz = p->q16horiz;
    p->oq16horizoff = p->q16horizoff;

    // [JM] I feel like this behavior could be re-implemented in P_GetInput, and this horizoff shit could go bye-bye.
    if (p->aim_mode == 0 && p->on_ground && sectorLotag != ST_2_UNDERWATER && (sector[p->cursectnum].floorstat & FLOOR_STAT_SLOPE))
    {
        x = p->pos.x + (sintable[(fix16_to_int(p->q16ang) + 512) & 2047] >> 5);
        y = p->pos.y + (sintable[fix16_to_int(p->q16ang) & 2047] >> 5);
        auto tempsect = p->cursectnum;
        updatesector(x, y, &tempsect);
        if (tempsect >= 0)
        {
            int florZOfSlope = getflorzofslope(p->cursectnum, x, y);
            if (p->cursectnum == tempsect)
                p->q16horizoff += fix16_from_int(mulscale16(p->truefz - florZOfSlope, 160));
            else if (klabs(getflorzofslope(tempsect, x, y) - florZOfSlope) <= (4 << 8))
                p->q16horizoff += fix16_from_int(mulscale16(p->truefz - florZOfSlope, 160));
        }
    }

    if (p->q16horizoff > 0)
    {
        p->q16horizoff -= ((p->q16horizoff >> 3) + fix16_one);
        p->q16horizoff = max(p->q16horizoff, 0);
    }
    else if (p->q16horizoff < 0)
    {
        p->q16horizoff += (((-p->q16horizoff) >> 3) + fix16_one);
        p->q16horizoff = min(p->q16horizoff, 0);
    }

    if (highZhit >= 0 && (highZhit & 49152) == 49152)
    {
        p->highsprite = highZhit & (MAXSPRITES - 1);

        if (sprite[p->highsprite].statnum == STAT_ACTOR && sprite[p->highsprite].extra >= 0)
        {
            //highZhit = 0; // Not used later...
            ceilZ = p->truecz;
        }
    }

    if (lowZhit >= 0 && (lowZhit & 49152) == 49152)
    {
        p->lowsprite = lowZhit & (MAXSPRITES - 1);

        if (P_DoSpriteBridge(p->lowsprite, p))
        {
            auto spr_sect_lotag = sector[sprite[p->lowsprite].sectnum].lotag;
            if (spr_sect_lotag != ST_2_UNDERWATER) // Underwater sprite bridges shouldn't clear tag.
            {
                sectorLotag = 0;
                if (noSE7Water) // HACK: If we're on a sprite bridge, we're not doing TROR water, so bail out of that, shall we?
                {
                    noSE7Water = false;
                    actor[playerSprite].floorz = floorZ;
                }
            }
        }
    }
    else if (waterZhit >= 0 && (waterZhit & 49152) == 49152)
    {
        p->lowsprite = waterZhit & (MAXSPRITES - 1);
        P_DoSpriteBridge(p->lowsprite, p);
    }

    if (s->extra > 0) P_IncurDamage(p);
    else
    {
        s->extra = 0;
        p->inv_amount[GET_SHIELD] = 0;
    }

    p->last_extra = s->extra;

    if (P_HandleFist(snum))
        return;

    if (P_HandleSpecialTimers(snum))
        return;

    if (s->extra <= 0)
    {
        P_HandleDeath(snum);

        if (sectorLotag == ST_2_UNDERWATER)
        {
            if (p->on_warping_sector == 0)
            {
                if (klabs(p->pos.z - floorZ) > (PHEIGHT >> 1))
                    p->pos.z += 348;
            }
            else
            {
                s->z -= 512;
                s->zvel = -348;
            }

            clipmove_old(&p->pos.x, &p->pos.y,
                &p->pos.z, &p->cursectnum,
                0, 0, PLAYER_WALLDIST, (4L << 8), (4L << 8), CLIPMASK0);
        }

        p->opos.x = p->pos.x;
        p->opos.y = p->pos.y;
        p->opos.z = p->pos.z;
        p->oq16ang = p->q16ang;
        p->opyoff = p->pyoff;

        p->q16horiz = F16(100);
        p->q16horizoff = 0;

        updatesector(p->pos.x, p->pos.y, &p->cursectnum);
        pushmove(&p->pos, &p->cursectnum, 128, (4L << 8), PLAYER_STEP_HEIGHT, CLIPMASK0);

        if (floorZ > ceilZ + (16 << 8) && s->pal != 1)
        {
            p->rotscrnang = (p->dead_flag + ((floorZ + p->pos.z) >> 7)) & 2047;
        }

        p->on_warping_sector = 0;

        return;
    }

    if (p->transporter_hold > 0)
    {
        p->transporter_hold--;
        if (p->transporter_hold == 0 && p->on_warping_sector)
            p->transporter_hold = 2;
    }
    if (p->transporter_hold < 0)
        p->transporter_hold++;

    if (p->newowner >= 0) // Using viewscreen
    {
        P_UpdatePosWhenViewingCam(p);

        P_DoCounters(p);

        if (*aplWeaponWorksLike[p->curr_weapon] == HANDREMOTE_WEAPON)
            P_ProcessWeapon(snum);

        return;
    }

    doubvel = TICSPERFRAME;

    if (p->rotscrnang)
    {
        p->rotscrnang -= (p->rotscrnang >> 1);

        if (p->rotscrnang && !(p->rotscrnang >> 1))
            p->rotscrnang -= ksgn(p->rotscrnang);
    }

    p->olook_ang = p->look_ang;

    if (p->look_ang)
    {
        p->look_ang -= (p->look_ang >> 2);

        if (p->look_ang && !(p->look_ang >> 2))
            p->look_ang -= ksgn(p->look_ang);
    }

    if (TEST_SYNC_KEY(playerBits, SK_LOOK_LEFT))
    {
        // look_left
        Gv_SetVar(sysVarIDs.RETURN, 0, playerSprite, snum);
        X_OnEvent(EVENT_LOOKLEFT, playerSprite, snum, -1);
        if (Gv_GetVar(sysVarIDs.RETURN, playerSprite, snum) == 0)
        {
            p->look_ang -= 152;
            p->rotscrnang += 24;
        }
    }

    if (TEST_SYNC_KEY(playerBits, SK_LOOK_RIGHT))
    {
        // look_right
        Gv_SetVar(sysVarIDs.RETURN, 0, playerSprite, snum);
        X_OnEvent(EVENT_LOOKRIGHT, playerSprite, snum, -1);
        if (Gv_GetVar(sysVarIDs.RETURN, playerSprite, snum) == 0)
        {
            p->look_ang += 152;
            p->rotscrnang -= 24;
        }
    }

    if (p->on_crane >= 0)
    {
        // [JM] Record opos/oq16ang here, to prevent a jitterfuck mess.
        p->opos = p->pos;
        p->oq16ang = p->q16ang;
        p->opyoff = p->pyoff;
        goto HORIZONLY;
    }

    if (s->xvel < 32 || p->on_ground == 0 || p->bobcounter == 1024)
    {
        if ((p->weapon_sway & 2047) > (1024 + 96))
            p->weapon_sway -= 96;
        else if ((p->weapon_sway & 2047) < (1024 - 96))
            p->weapon_sway += 96;
        else p->weapon_sway = 1024;
    }
    else p->weapon_sway = p->bobcounter;

    s->xvel = ksqrt((p->pos.x - p->bobposx) * (p->pos.x - p->bobposx) + (p->pos.y - p->bobposy) * (p->pos.y - p->bobposy));
    if (p->on_ground)
        p->bobcounter += sprite[playerSprite].xvel >> 1;

    if (ud.noclip == 0 && (sector[p->cursectnum].floorpicnum == MIRROR || p->cursectnum < 0 || p->cursectnum >= MAXSECTORS))
    {
        p->pos.x = p->opos.x;
        p->pos.y = p->opos.y;
    }
    else
    {
        p->opos.x = p->pos.x;
        p->opos.y = p->pos.y;
    }

    p->bobposx = p->pos.x;
    p->bobposy = p->pos.y;

    p->opos.z = p->pos.z;
    p->opyoff = p->pyoff;
    p->oq16ang = p->q16ang;

    if (p->one_eighty_count < 0)
    {
        p->one_eighty_count += 128;
        p->q16ang += F16(128);
    }

    updatesector(p->pos.x, p->pos.y, &p->cursectnum);
    if (!ud.noclip)
        pushmove(&p->pos, &p->cursectnum, PLAYER_WALLDIST, (4L << 8), PLAYER_STEP_HEIGHT, CLIPMASK0);

    // Shrinking code

    floorZOffset = PLAYER_ZOFF_NORMAL;

    if (sectorLotag == ST_2_UNDERWATER)
    {
        // under water
        p->jumping_counter = 0;
        doFloatbob = true;

        if (!A_CheckSoundPlaying(playerSprite, DUKE_UNDERWATER))
            A_PlaySound(DUKE_UNDERWATER, playerSprite);

        if (TEST_SYNC_KEY(playerBits, SK_JUMP))
        {
            Gv_SetVar(sysVarIDs.RETURN, 0, playerSprite, snum);
            X_OnEvent(EVENT_SWIMUP, playerSprite, snum, -1);
            if (Gv_GetVar(sysVarIDs.RETURN, playerSprite, snum) == 0)
            {
                // jump
                if (p->vel.z > 0)
                    p->vel.z = 0;

                p->vel.z -= 348;

                if (p->vel.z < -(256 * 6))
                    p->vel.z = -(256 * 6);
            }
        }
        else if (TEST_SYNC_KEY(playerBits, SK_CROUCH))
        {
            Gv_SetVar(sysVarIDs.RETURN, 0, playerSprite, snum);
            X_OnEvent(EVENT_SWIMDOWN, playerSprite, snum, -1);
            if (Gv_GetVar(sysVarIDs.RETURN, playerSprite, snum) == 0)
            {
                // crouch
                if (p->vel.z < 0)
                    p->vel.z = 0;

                p->vel.z += 348;

                if (p->vel.z > (256 * 6))
                    p->vel.z = (256 * 6);
            }
        }
        else
        {
            // normal view
            if (p->vel.z < 0)
                p->vel.z = min(0, p->vel.z + 256);

            if (p->vel.z > 0)
                p->vel.z = max(0, p->vel.z - 256);
        }

        if (p->vel.z > 2048)
            p->vel.z >>= 1;

        p->pos.z += p->vel.z;

        if (p->pos.z > (floorZ - (15 << 8)))
            p->pos.z += ((floorZ - (15 << 8)) - p->pos.z) >> 1;

        if (p->pos.z < ceilZ)
        {
            p->pos.z = ceilZ;
            p->vel.z = 0;
        }

        if ((p->on_warping_sector == 0 || ceilZ != p->truecz) && p->pos.z < ceilZ + PMINHEIGHT)
        {
            p->pos.z = ceilZ + PMINHEIGHT;
            p->vel.z = 0;
        }

        if (!oldnet_predicting && p->scuba_on && (krand() & 255) < 8)
        {
            int spr = A_Spawn(playerSprite, WATERBUBBLE);
            sprite[spr].x +=
                sintable[(fix16_to_int(p->q16ang) + 512 + 64 - (g_globalRandom & 128)) & 2047] >> 6;
            sprite[spr].y +=
                sintable[(fix16_to_int(p->q16ang) + 64 - (g_globalRandom & 128)) & 2047] >> 6;
            sprite[spr].xrepeat = 3;
            sprite[spr].yrepeat = 2;
            sprite[spr].z = p->pos.z + (8 << 8);
        }
    }
    else if (p->jetpack_on)
    {
        doFloatbob = true;
        P_DoJetpack(snum, playerBits, shrunk, sectorLotag, floorZ);
    }
    else if (sectorLotag != ST_2_UNDERWATER)
    {
        if (p->airleft != 15 * 26)
            p->airleft = 15 * 26; //Aprox twenty seconds.

        if (p->scuba_on == 1)
            p->scuba_on = 0;

        if (sectorLotag == ST_1_ABOVE_WATER) //&& p->spritebridge == 0)
        {
            floorZOffset = PLAYER_ZOFF_SHRUNK;

            // Sinking into water
            if (shrunk == 0)
            {
                floorZOffset = PLAYER_ZOFF_WATER;
                doFloatbob = true;
            }

            if (shrunk == 0 && (noSE7Water || (trueFloorDist <= PHEIGHT)) && !oldnet_predicting)
            {
                if (p->on_ground == 1)
                {
                    if (p->dummyplayersprite == -1 && (yax_getbunch(p->cursectnum, YAX_FLOOR) == -1))
                    {
                        p->dummyplayersprite = A_Spawn(playerSprite, PLAYERONWATER);
                        sprite[p->dummyplayersprite].pal = sprite[playerSprite].pal;
                    }

                    p->footprintcount = 6;
                    if (sector[p->cursectnum].floorpicnum == FLOORSLIME)
                        p->footprintpal = 8;
                    else p->footprintpal = 0;
                    p->footprintshade = 0;
                }
            }
        }
        else
        {
            if (p->footprintcount > 0 && p->on_ground && !oldnet_predicting)
            {
                if (p->cursectnum >= 0 && (sector[p->cursectnum].floorstat & 2) != 2)
                {
                    int spr;

                    for (spr = headspritesect[p->cursectnum]; spr >= 0; spr = nextspritesect[spr])
                    {
                        if (sprite[spr].picnum == FOOTPRINTS || sprite[spr].picnum == FOOTPRINTS2 || sprite[spr].picnum == FOOTPRINTS3 || sprite[spr].picnum == FOOTPRINTS4)
                            if (klabs(sprite[spr].x - p->pos.x) < 384)
                                if (klabs(sprite[spr].y - p->pos.y) < 384)
                                    break;
                    }

                    if (spr < 0)
                    {
                        if (sector[p->cursectnum].lotag == ST_0_NO_EFFECT && sector[p->cursectnum].hitag == 0)
                        {
                            if (yax_getbunch(p->cursectnum, YAX_FLOOR) < 0 || (sector[p->cursectnum].floorstat & 512))
                            {
                                switch (krand() & 3)
                                {
                                case 0: spr = A_Spawn(playerSprite, FOOTPRINTS); break;
                                case 1: spr = A_Spawn(playerSprite, FOOTPRINTS2); break;
                                case 2: spr = A_Spawn(playerSprite, FOOTPRINTS3); break;
                                default: spr = A_Spawn(playerSprite, FOOTPRINTS4); break;
                                }
                                sprite[spr].pal = p->footprintpal;
                                sprite[spr].shade = p->footprintshade;
                                p->footprintcount--;
                            }
                        }
                    }
                }
            }
        }

        if (p->pos.z < (floorZ - floorZOffset))  //falling
        {
            // not jumping or crouching
            if (!TEST_SYNC_KEY(playerBits, SK_JUMP) && !TEST_SYNC_KEY(playerBits, SK_CROUCH) && p->on_ground && (sector[p->cursectnum].floorstat & FLOOR_STAT_SLOPE) && p->pos.z >= (floorZ - floorZOffset - ZOFFSET2))
                p->pos.z = floorZ - floorZOffset;
            else
            {
                p->on_ground = 0;
                p->vel.z += (g_spriteGravity + 80); // (TICSPERFRAME<<6);

                if (p->vel.z >= ACTOR_MAXFALLINGZVEL)
                    p->vel.z = ACTOR_MAXFALLINGZVEL;

                if (p->vel.z > 2400 && p->falling_counter < 255)
                {
                    p->falling_counter++;
                    if (p->falling_counter == 38)
                        p->scream_voice = A_PlaySound(DUKE_SCREAM, playerSprite);
                }

                if ((p->pos.z + p->vel.z) >= (floorZ - floorZOffset) && p->cursectnum >= 0)   // hit the ground
                {
                    if (sector[p->cursectnum].lotag != ST_1_ABOVE_WATER)
                    {
                        if (p->falling_counter > 62) P_QuickKill(p);

                        else if (p->falling_counter > 9)
                        {
                            s->extra -= p->falling_counter - (krand() & 3);
                            if (s->extra <= 0)
                            {
                                A_PlaySound(SQUISHED, playerSprite);
                                P_PalFrom(p, 63, 63, 0, 0);
                            }
                            else
                            {
                                A_PlaySound(DUKE_LAND, playerSprite);
                                A_PlaySound(DUKE_LAND_HURT, playerSprite);
                            }

                            P_PalFrom(p, 32, 16, 0, 0);
                        }
                        else if (p->vel.z > 2048) A_PlaySound(DUKE_LAND, playerSprite);
                    }
                }
            }
        }
        else
        {
            p->falling_counter = 0;
            if (p->scream_voice > FX_Ok)
            {
                FX_StopSound(p->scream_voice);
                p->scream_voice = FX_Ok;
            }

            if (sectorLotag != ST_1_ABOVE_WATER && sectorLotag != ST_2_UNDERWATER && p->on_ground == 0 && p->vel.z > (ACTOR_MAXFALLINGZVEL >> 1))
                p->hard_landing = p->vel.z >> 10;

            p->on_ground = 1;

            if (floorZOffset == PLAYER_ZOFF_NORMAL)
            {
                //Smooth on the ground
                int Zdiff = ((floorZ - floorZOffset) - p->pos.z) >> 1;

                if (klabs(Zdiff) < 256)
                    Zdiff = 0;
                p->pos.z += Zdiff;

                p->vel.z -= 768;

                if (p->vel.z < 0)
                    p->vel.z = 0;
            }
            else if (p->jumping_counter == 0)
            {
                p->pos.z += ((floorZ - (floorZOffset >> 1)) - p->pos.z) >> 1; //Smooth on the water
                if (p->on_warping_sector == 0 && p->pos.z > floorZ - PLAYER_MINWATERZDIST)
                {
                    p->pos.z = floorZ - PLAYER_MINWATERZDIST;
                    p->vel.z >>= 1;
                }
            }

            // HACK: If we're in TROR water, and the water is too shallow, clamp and prevent floating bob.
            if (noSE7Water && (p->pos.z > (waterFloor - PLAYER_ZOFF_NORMAL)))
            {
                p->pos.z = waterFloor - PLAYER_ZOFF_NORMAL;
                doFloatbob = false;
            }

            if (TEST_SYNC_KEY(playerBits, SK_CROUCH))
            {
                // crouching
                Gv_SetVar(sysVarIDs.RETURN, 0, playerSprite, snum);
                X_OnEvent(EVENT_CROUCH, playerSprite, snum, -1);
                if (Gv_GetVar(sysVarIDs.RETURN, playerSprite, snum) == 0)
                {
                    p->pos.z += PLAYER_CROUCH_INC;
                    p->crack_time = 777;
                }
            }

            // jumping
            if (!TEST_SYNC_KEY(playerBits, SK_JUMP) && p->jumping_toggle == 1)
                p->jumping_toggle = 0;
            else if (TEST_SYNC_KEY(playerBits, SK_JUMP) && p->jumping_toggle == 0)
            {
                if (p->jumping_counter == 0)
                    if ((floorZ - ceilZ) > (56 << 8))
                    {
                        Gv_SetVar(sysVarIDs.RETURN, 0, playerSprite, snum);
                        X_OnEvent(EVENT_JUMP, playerSprite, snum, -1);
                        if (Gv_GetVar(sysVarIDs.RETURN, playerSprite, snum) == 0)
                        {
                            p->jumping_counter = 1;
                            p->jumping_toggle = 1;
                        }
                    }
            }

            if (p->jumping_counter && !TEST_SYNC_KEY(playerBits, SK_JUMP))
                p->jumping_toggle = 0;
        }

        if (p->jumping_counter)
        {
            if (!TEST_SYNC_KEY(playerBits, SK_JUMP) && p->jumping_toggle == 1)
                p->jumping_toggle = 0;

            if (p->jumping_counter < (1024 + 256))
            {
                if (sectorLotag == ST_1_ABOVE_WATER && p->jumping_counter > 768)
                {
                    p->jumping_counter = 0;
                    p->vel.z = -512;
                }
                else
                {
                    p->vel.z -= (sintable[(2048 - 128 + p->jumping_counter) & 2047]) / 12;

                    // HACK: Prevent eyes from going below TROR water when we try jumping.
                    if (noSE7Water && (p->pos.z + p->vel.z) > floorZ)
                        p->vel.z = 0;

                    p->jumping_counter += 180;
                    p->on_ground = 0;
                }
            }
            else
            {
                p->jumping_counter = 0;
                p->vel.z = 0;
            }
        }

        p->pos.z += p->vel.z;

        if (p->pos.z < (ceilZ + (4 << 8)))
        {
            p->jumping_counter = 0;
            if (p->vel.z < 0)
                p->vel.x = p->vel.y = 0;
            p->vel.z = 128;
            p->pos.z = ceilZ + (4 << 8);
        }
    }

    if (doFloatbob)
    {
        p->pycount += 32;
        p->pycount &= 2047;
        p->pyoff = sintable[p->pycount] >> 7;
    }

    // Halt on Kicking/Tripmines/Nukebuttons
    if (p->fist_incs ||
        p->transporter_hold > 2 ||
        p->hard_landing ||
        p->access_incs > 0 ||
        p->knee_incs > 0 ||
        (*aplWeaponWorksLike[p->curr_weapon] == TRIPBOMB_WEAPON &&
            p->kickback_pic > 1 &&
            p->kickback_pic < 4))
    {
        doubvel = 0;
        p->vel.x = 0;
        p->vel.y = 0;
    }
    else if (pInput->q16avel)            //p->ang += syncangvel * constant
    {
        //ENGINE calculates angvel for you
        fix16_t inputAng = pInput->q16avel;

        p->q16angvel = (sectorLotag == ST_2_UNDERWATER) ? fix16_mul(inputAng - (inputAng >> 3), fix16_from_int(ksgn(doubvel)))
            : fix16_mul(inputAng, fix16_from_int(ksgn(doubvel)));
        p->q16ang += p->q16angvel;
        p->q16ang &= 0x7FFFFFF;
        p->crack_time = 777;
    }

    if (p->spritebridge == 0)
        P_HandleDamagingFloors(snum);

    if (pInput->extbits & (EXTBIT_MOVEFORWARD))
        X_OnEvent(EVENT_MOVEFORWARD, playerSprite, snum, -1);

    if (pInput->extbits & (EXTBIT_MOVEBACKWARD))
        X_OnEvent(EVENT_MOVEBACKWARD, playerSprite, snum, -1);

    if (pInput->extbits & (EXTBIT_STRAFELEFT))
        X_OnEvent(EVENT_STRAFELEFT, playerSprite, snum, -1);

    if (pInput->extbits & (EXTBIT_STRAFERIGHT))
        X_OnEvent(EVENT_STRAFERIGHT, playerSprite, snum, -1);

    if (pInput->extbits & (EXTBIT_TURNLEFT) || pInput->q16avel < 0)
        X_OnEvent(EVENT_TURNLEFT, playerSprite, snum, -1);

    if (pInput->extbits & (EXTBIT_TURNRIGHT) || pInput->q16avel > 0)
        X_OnEvent(EVENT_TURNRIGHT, playerSprite, snum, -1);

    if (p->vel.x || p->vel.y || pInput->fvel || pInput->svel)
    {
        p->crack_time = 777;

        int const checkWalkSound = sintable[p->bobcounter & 2047] >> 12;

        if ((trueFloorDist < PHEIGHT + (8 << 8) || p->spritebridge) && (checkWalkSound == 1 || checkWalkSound == 3))
        {
            if (/*p->spritebridge == 0 &&*/ p->walking_snd_toggle == 0 && p->on_ground)
            {
                int floorPic;
                switch (sectorLotag)
                {
                case ST_0_NO_EFFECT:

                    floorPic = (lowZhit >= 0 && (lowZhit & 49152) == 49152)
                        ? TrackerCast(sprite[lowZhit & (MAXSPRITES - 1)].picnum)
                        : TrackerCast(sector[p->cursectnum].floorpicnum);

                    switch (tileGetMapping(floorPic))
                    {
                    case PANNEL1__:
                    case PANNEL2__:
                        A_PlaySound(DUKE_WALKINDUCTS, playerSprite);
                        p->walking_snd_toggle = 1;
                        break;
                    }
                    break;
                case ST_1_ABOVE_WATER:
                    if ((krand() & 1) == 0)
                        A_PlaySound(DUKE_ONWATER, playerSprite);
                    p->walking_snd_toggle = 1;
                    break;
                }
            }
        }
        else if (p->walking_snd_toggle > 0)
            p->walking_snd_toggle--;

        if (p->jetpack_on == 0 && p->inv_amount[GET_STEROIDS] > 0 && p->inv_amount[GET_STEROIDS] < 400)
            doubvel <<= 1;

        p->vel.x += ((pInput->fvel * doubvel) << 6);
        p->vel.y += ((pInput->svel * doubvel) << 6);

        if ((aplWeaponWorksLike[p->curr_weapon][snum] == KNEE_WEAPON && p->kickback_pic > 10 && p->on_ground) || (p->on_ground && TEST_SYNC_KEY(playerBits, SK_CROUCH)))
        {
            p->vel.x = mulscale(p->vel.x, p->runspeed - 0x2000, 16);
            p->vel.y = mulscale(p->vel.y, p->runspeed - 0x2000, 16);
        }
        else
        {
            if (sectorLotag == ST_2_UNDERWATER)
            {
                p->vel.x = mulscale(p->vel.x, p->runspeed - 0x1400, 16);
                p->vel.y = mulscale(p->vel.y, p->runspeed - 0x1400, 16);
            }
            else
            {
                p->vel.x = mulscale(p->vel.x, p->runspeed, 16);
                p->vel.y = mulscale(p->vel.y, p->runspeed, 16);
            }
        }

        if (klabs(p->vel.x) < 2048 && klabs(p->vel.y) < 2048)
            p->vel.x = p->vel.y = 0;

        if (shrunk)
        {
            p->vel.x = mulscale16(p->vel.x, p->runspeed - (p->runspeed >> 1) + (p->runspeed >> 2));
            p->vel.y = mulscale16(p->vel.y, p->runspeed - (p->runspeed >> 1) + (p->runspeed >> 2));
        }
    }

HORIZONLY:

    //if (p->cursectnum >= 0 && sector[p->cursectnum].lotag == ST_2_UNDERWATER) k = 0;
    //else k = 1;

    if (ud.noclip)
    {
        p->pos.x += p->vel.x >> 14;
        p->pos.y += p->vel.y >> 14;
        updatesector(p->pos.x, p->pos.y, &p->cursectnum);
        changespritesect(playerSprite, p->cursectnum);
    }
    else
    {
        int const playerSectNum = p->cursectnum;
        
        // This updatesectorz conflicts with Duke3D's way of teleporting through water,
        // so make it a bit conditional... OTOH, this way we have an ugly z jump when
        // changing from above water to underwater
        if (playerSectNum >= 0)
        {
            int16_t   ceilingBunch, floorBunch;
            yax_getbunches(playerSectNum, &ceilingBunch, &floorBunch);

            if (
                (floorBunch >= 0 && !(sector[playerSectNum].floorstat & 512) && !(sector[playerSectNum].lotag == ST_1_ABOVE_WATER && p->on_ground))
                || (ceilingBunch >= 0 && !(sector[playerSectNum].ceilingstat & 512))
               )
            {
                p->cursectnum += MAXSECTORS;  // skip initial z check, restored by updatesectorz
                updatesectorz(p->pos.x, p->pos.y, p->pos.z, &p->cursectnum);
            }
        }

        int moveResult = clipmove(&p->pos, &p->cursectnum,
            p->vel.x, p->vel.y, PLAYER_WALLDIST, PMINHEIGHT, (sectorLotag == ST_1_ABOVE_WATER || p->spritebridge == 1) ? (4L << 8) : PLAYER_STEP_HEIGHT, CLIPMASK0);

        if (p->jetpack_on == 0 && sectorLotag != ST_2_UNDERWATER && sectorLotag != ST_1_ABOVE_WATER && shrunk)
            p->pos.z += 32 << 8;

        if (moveResult)
            P_CheckTouchDamage(p, moveResult);
    }

    if (p->jetpack_on == 0)
    {
        if (s->xvel > 16)
        {
            if (sectorLotag != ST_1_ABOVE_WATER && sectorLotag != ST_2_UNDERWATER && p->on_ground)
            {
                p->pycount += 52;
                p->pycount &= 2047;
                p->pyoff = klabs(s->xvel * sintable[p->pycount]) / 1596;
            }
        }
        else if (sectorLotag != ST_2_UNDERWATER && sectorLotag != ST_1_ABOVE_WATER)
            p->pyoff = 0;
    }

    // RBG***
    if (p->cursectnum >= 0)
    {
        sprite[p->i].xyz = p->pos;
        sprite[p->i].z += PHEIGHT;

        if (p->cursectnum != s->sectnum)
            changespritesect(p->i, p->cursectnum);
    }

    if (p->cursectnum >= 0 && sectorLotag < ST_3)
    {
        auto const pSector = (usectorptr_t)&sector[p->cursectnum];

        // TRAIN_SECTOR_TO_SE_INDEX
        if ((!ud.noclip && pSector->lotag == ST_31_TWO_WAY_TRAIN) &&
            ((unsigned)pSector->hitag < MAXSPRITES && sprite[pSector->hitag].xvel && actor[pSector->hitag].t_data[0] == 0))
        {
            P_QuickKill(p);
            return;
        }
    }

    if (p->cursectnum >= 0 && trueFloorDist < PHEIGHT && p->on_ground && sectorLotag != 1 && shrunk == 0 && sector[p->cursectnum].lotag == ST_1_ABOVE_WATER)
        if (!A_CheckSoundPlaying(playerSprite, DUKE_ONWATER))
            A_PlaySound(DUKE_ONWATER, playerSprite);

    p->on_warping_sector = 0;

    int mashedPotato = 0;
    if (!ud.noclip)
        mashedPotato = ((pushmove(&p->pos, &p->cursectnum, PLAYER_WALLDIST, (4L << 8), (4L << 8), CLIPMASK0) < 0) && (A_GetFurthestAngle(p->i, 8) < 512));

    if (!ud.noclip)
    {
        if (klabs(actor[playerSprite].floorz - actor[playerSprite].ceilingz) < (48 << 8) || mashedPotato)
        {
            if (!(sector[s->sectnum].lotag & 0x8000) && (isanunderoperator(sector[s->sectnum].lotag) ||
                isanearoperator(sector[s->sectnum].lotag)))
                activatebysector(s->sectnum, playerSprite);

            if (mashedPotato)
            {
                P_QuickKill(p);
                return;
            }
        }
        else if (klabs(floorZ - ceilZ) < (32 << 8) && isanunderoperator(sector[p->cursectnum].lotag))
            activatebysector(p->cursectnum, playerSprite);
    }

    // center_view
    if (p->return_to_center > 0)
        p->return_to_center--;

    if (TEST_SYNC_KEY(playerBits, SK_CENTER_VIEW) || p->hard_landing)
    {
        Gv_SetVar(sysVarIDs.RETURN, 0, playerSprite, snum);
        X_OnEvent(EVENT_RETURNTOCENTER, playerSprite, snum, -1);
        if (Gv_GetVar(sysVarIDs.RETURN, playerSprite, snum) == 0)
        {
            p->return_to_center = 9;
        }
    }

    if (TEST_SYNC_KEY(playerBits, SK_LOOK_UP))
    {
        // look_up
        Gv_SetVar(sysVarIDs.RETURN, 0, playerSprite, snum);
        X_OnEvent(EVENT_LOOKUP, playerSprite, snum, -1);
        if (Gv_GetVar(sysVarIDs.RETURN, playerSprite, snum) == 0)
        {
            p->return_to_center = 9;
            if (TEST_SYNC_KEY(playerBits, SK_RUN)) p->q16horiz += fix16_from_int(12);     // running
            p->q16horiz += fix16_from_int(12);
            // horizRecenter here?
        }
    }
    else if (TEST_SYNC_KEY(playerBits, SK_LOOK_DOWN))
    {
        // look_down
        Gv_SetVar(sysVarIDs.RETURN, 0, playerSprite, snum);
        X_OnEvent(EVENT_LOOKDOWN, playerSprite, snum, -1);
        if (Gv_GetVar(sysVarIDs.RETURN, playerSprite, snum) == 0)
        {
            p->return_to_center = 9;
            if (TEST_SYNC_KEY(playerBits, SK_RUN)) p->q16horiz -= fix16_from_int(12);
            p->q16horiz -= fix16_from_int(12);
            // horizRecenter here?
        }
    }
    else if (TEST_SYNC_KEY(playerBits, SK_AIM_UP))
    {
        // aim_up
        Gv_SetVar(sysVarIDs.RETURN, 0, playerSprite, snum);
        X_OnEvent(EVENT_AIMUP, playerSprite, snum, -1);
        if (Gv_GetVar(sysVarIDs.RETURN, playerSprite, snum) == 0)
        {
            // running
            if (TEST_SYNC_KEY(playerBits, SK_RUN)) p->q16horiz += fix16_from_int(6);
            p->q16horiz += fix16_from_int(6);
            // horizRecenter here?
        }
    }
    else if (TEST_SYNC_KEY(playerBits, SK_AIM_DOWN))
    {
        // aim_down
        Gv_SetVar(sysVarIDs.RETURN, 0, playerSprite, snum);
        X_OnEvent(EVENT_AIMDOWN, playerSprite, snum, -1);
        if (Gv_GetVar(sysVarIDs.RETURN, playerSprite, snum) == 0)
        {
            // running
            if (TEST_SYNC_KEY(playerBits, SK_RUN)) p->q16horiz -= fix16_from_int(6);
            p->q16horiz -= fix16_from_int(6);
            // horizRecenter here?
        }
    }

    if (p->return_to_center > 0)
        if (TEST_SYNC_KEY(playerBits, SK_LOOK_UP) == 0 && TEST_SYNC_KEY(playerBits, SK_LOOK_DOWN) == 0)
        {
            p->q16horiz += F16(33) - fix16_div(p->q16horiz, F16(3));
            // horizRecenter here?
        }

    if (p->hard_landing > 0)
    {
        p->hard_landing--;
        p->q16horiz -= fix16_from_int(p->hard_landing << 4);
    }

    p->q16horiz += pInput->q16horz;
    p->q16horiz = fix16_clamp(p->q16horiz, F16(HORIZ_MIN), F16(HORIZ_MAX));
    p->q16horizoff = fix16_clamp(p->q16horizoff, F16(HORIZ_MIN), F16(HORIZ_MAX));

    //Shooting code/changes

    if (p->show_empty_weapon > 0)
    {
        p->show_empty_weapon--;
        if (p->show_empty_weapon == 0 && (p->weaponswitch & 2) && p->ammo_amount[p->curr_weapon] <= 0)
        {
            if (p->last_full_weapon == GROW_WEAPON)
                p->subweapon |= (1 << GROW_WEAPON);
            else if (p->last_full_weapon == SHRINKER_WEAPON)
                p->subweapon &= ~(1 << GROW_WEAPON);
            P_AddWeapon(p, p->last_full_weapon, 1);
            return;
        }
    }

    if (p->knee_incs > 0)
    {
        p->q16horiz -= F16(48);
        p->return_to_center = 9;
        // horizRecenter here?
        if (++p->knee_incs > 15)
        {
            p->knee_incs = 0;
            p->holster_weapon = 0;
            if (p->weapon_pos < 0)
                p->weapon_pos = -p->weapon_pos;
            
            if (!oldnet_predicting && p->actorsqu >= 0 && dist(&sprite[playerSprite], &sprite[p->actorsqu]) < 1400 && sprite[p->actorsqu].statnum != MAXSTATUS)
            {
                // P_StompEnemy
                A_DoGuts(p->actorsqu, JIBS6, 7);
                A_Spawn(p->actorsqu, BLOODPOOL);
                A_PlaySound(SQUISHED, p->actorsqu);

                if (G_CheckFemSprite(p->actorsqu))
                {
                    if (sprite[p->actorsqu].yvel)
                        G_OperateRespawns(sprite[p->actorsqu].yvel);
                }

                if (sprite[p->actorsqu].picnum == APLAYER)
                {
                    P_QuickKill(g_player[sprite[p->actorsqu].yvel].ps);
                    g_player[sprite[p->actorsqu].yvel].ps->frag_ps = snum;
                }
                else if (A_CheckEnemySprite(&sprite[p->actorsqu]))
                {
                    actor[p->actorsqu].htowner = p->i;
                    G_AddMonsterKills(p->actorsqu, 1);
                    A_DeleteSprite(p->actorsqu);
                }
                else A_DeleteSprite(p->actorsqu);
            }

            p->actorsqu = -1;
        }
        else if (p->actorsqu >= 0)
        {
            int angleDelta = G_GetAngleDelta(fix16_to_int(p->q16ang), getangle(sprite[p->actorsqu].x - p->pos.x, sprite[p->actorsqu].y - p->pos.y)) >> 2;
            p->q16ang += fix16_from_int(angleDelta);
        }
    }

    if (P_DoCounters(p)) return;

    P_ProcessWeapon(snum);
}