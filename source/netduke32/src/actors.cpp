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
#include "compat.h"
#include "colmatch.h"

#define KILLIT(KX) {A_DeleteSprite(KX);goto BOLT;}

ActorData_t actor[MAXSPRITES];

extern int g_numEnvSoundsPlaying;
extern int g_noEnemies;

void G_BackupInterpolations(void)
{
    for (bssize_t i = g_interpolationCnt - 1; i >= 0; i--)
        interps[i].bakpos = *interps[i].curpos;
}

void G_UpdateInterpolations(void)  //Stick at beginning of G_DoMoveThings
{
    for (bssize_t i = g_interpolationCnt - 1; i >= 0; i--)
        interps[i].oldpos = *interps[i].curpos;
}

int G_SetInterpolation(int32_t* const posptr)
{
    if (g_interpolationCnt >= MAXINTERPOLATIONS)
        return 1;

    for (bssize_t i = 0; i < g_interpolationCnt; ++i)
        if (interps[i].curpos == posptr)
            return 0;

    interps[g_interpolationCnt].curpos = posptr;
    interps[g_interpolationCnt].oldpos = *posptr;
    g_interpolationCnt++;
    return 0;
}

void G_StopInterpolation(const int32_t* const posptr)
{
    for (bssize_t i = 0; i < g_interpolationCnt; ++i)
        if (interps[i].curpos == posptr)
        {
            g_interpolationCnt--;
            interps[i].oldpos = interps[g_interpolationCnt].oldpos;
            interps[i].bakpos = interps[g_interpolationCnt].bakpos;
            interps[i].curpos = interps[g_interpolationCnt].curpos;
        }
}

void G_DoInterpolations(int smoothRatio)
{
    if (g_interpolationLock++)
        return;

    int32_t ndelta = 0;

    for (bssize_t i = 0, j = 0; i < g_interpolationCnt; ++i)
    {
        int32_t const odelta = ndelta;
        auto const lerp = &interps[i];

        lerp->bakpos = *lerp->curpos;
        ndelta = (*lerp->curpos) - lerp->oldpos;

        if (odelta != ndelta)
            j = mulscale16(ndelta, smoothRatio);

        *lerp->curpos = lerp->oldpos + j;
    }
}

void G_RestoreInterpolations(void)  //Stick at end of drawscreen
{
    if (--g_interpolationLock)
        return;

    for (int32_t i = g_interpolationCnt - 1; i >= 0; i--)
        *interps[i].curpos = interps[i].bakpos;
}

void G_DoConveyorInterp(int32_t smoothratio)
{
    for (int32_t SPRITES_OF(STAT_EFFECTOR, i))
    {
        auto s = &sprite[i];
        auto a = &actor[i];
        auto sect = &sector[s->sectnum];

        if (s->picnum != SECTOREFFECTOR || a->t_data[4])
            continue;

        if (s->lotag == SE_24_CONVEYOR || s->lotag == SE_34)
        {
            sect->floorxpanning -= (uint8_t)mulscale16(65536 - smoothratio, sect->floorxpanning - (sect->floorxpanning - (s->yvel >> 7)));
        }
    }
}

void G_ResetConveyorInterp(void)
{
    for (int32_t SPRITES_OF(STAT_EFFECTOR, i))
    {
        auto s = &sprite[i];
        auto a = &actor[i];
        auto sect = &sector[s->sectnum];

        if (s->picnum != SECTOREFFECTOR || a->t_data[4])
            continue;

        if (s->lotag == SE_24_CONVEYOR || s->lotag == SE_34)
        {
            sect->floorxpanning = (uint8_t)a->bpos.x;
        }
    }
}

static int32_t Wall_SetInterpolation(int16_t wallNum, bool enableInterp)
{
    if (enableInterp)
        return G_SetInterpolation(&wall[wallNum].x) || G_SetInterpolation(&wall[wallNum].y);

    G_StopInterpolation(&wall[wallNum].x);
    G_StopInterpolation(&wall[wallNum].y);
    return 0;
}

// Sets interpolation for all walls of a sector.
// sectnum: Sector index
// enableInterp: True to enable interpolation, False to disable/clear it.
void Sect_SetInterpolation(int16_t sectnum, bool enableInterp)
{
    int j = sector[sectnum].wallptr, endwall = j + sector[sectnum].wallnum;

    for (; j < endwall; j++)
    {
        Wall_SetInterpolation(j, enableInterp);

        auto const nextWall = wall[j].nextwall;
        if (nextWall >= 0)
        {
            Wall_SetInterpolation(nextWall, enableInterp);
            Wall_SetInterpolation(wall[nextWall].point2, enableInterp);

            // Propagate to connected TROR walls.
            auto const trorCeilWall = yax_getnextwall(nextWall, YAX_CEILING);
            auto const trorFloorWall = yax_getnextwall(nextWall, YAX_FLOOR);

            if (trorCeilWall >= 0)
            {
                Wall_SetInterpolation(trorCeilWall, enableInterp);

                auto const nextWall = wall[trorCeilWall].nextwall;
                if (nextWall >= 0)
                {
                    Wall_SetInterpolation(nextWall, enableInterp);
                    Wall_SetInterpolation(wall[nextWall].point2, enableInterp);
                }
            }

            if (trorFloorWall >= 0)
            {
                Wall_SetInterpolation(trorFloorWall, enableInterp);

                auto const nextWall = wall[trorFloorWall].nextwall;
                if (nextWall >= 0)
                {
                    Wall_SetInterpolation(nextWall, enableInterp);
                    Wall_SetInterpolation(wall[nextWall].point2, enableInterp);
                }
            }
        }
    }
}

void G_ClearCameraView(DukePlayer_t* ps)
{
    ps->newowner = -1;
    ps->pos = ps->opos;
    ps->q16ang = ps->oq16ang;

    updatesector(ps->pos.x, ps->pos.y, &ps->cursectnum);
    P_UpdateScreenPal(ps);

    for (bssize_t SPRITES_OF(STAT_ACTOR, k))
        if (sprite[k].picnum == CAMERA1)
            sprite[k].yvel = 0;
}

int G_CheckForSpaceCeiling(int sectnum)
{
    return ((sector[sectnum].ceilingstat&1) && sector[sectnum].ceilingpal == 0 && (sector[sectnum].ceilingpicnum==MOONSKY1 || sector[sectnum].ceilingpicnum==BIGORBIT1)?1:0);
}

int G_CheckForSpaceFloor(int sectnum)
{
    return ((sector[sectnum].floorstat&1) && sector[sectnum].ceilingpal == 0 && ((sector[sectnum].floorpicnum==MOONSKY1)||(sector[sectnum].floorpicnum==BIGORBIT1))?1:0);
}

void G_AddMonsterKills(int32_t spriteNum, int32_t amount)
{
    if (oldnet_predicting || (uint32_t)spriteNum >= MAXSPRITES)
        return;

    auto pActor = &actor[spriteNum];

    ud.monsters_killed += amount;

    if (pActor->htowner >= 0)
    {
        auto killerSprite = &sprite[pActor->htowner];
        if (killerSprite->picnum == APLAYER)
        {
            int32_t playerNum = killerSprite->yvel;
            if ((playerNum >= 0) && (playerNum < MAXPLAYERS))
            {
                g_player[playerNum].ps->actors_killed += amount;
                //OSD_Printf("%s has killed a monster with a picnum of %d\n", g_player[playerNum].user_name, TrackerCast(sprite[spriteNum].picnum));
                return;
            }
        }
    }

    //OSD_Printf("A monster with a picnum of %d was killed by a non-player.\n", TrackerCast(sprite[spriteNum].picnum));
}

void P_AddAmmo(int weapon, DukePlayer_t* p, int amount)
{
    p->ammo_amount[weapon] += amount;

    if (p->ammo_amount[weapon] > p->max_ammo_amount[weapon])
        p->ammo_amount[weapon] = p->max_ammo_amount[weapon];
}

void P_AddWeaponNoSwitch(DukePlayer_t *p, int weapon)
{
    int snum = sprite[p->i].yvel;

    if (p->gotweapon[weapon] == 0)
    {
        p->gotweapon[weapon] = 1;
        if (weapon == SHRINKER_WEAPON)
            p->gotweapon[GROW_WEAPON] = 1;
    }

    if (aplWeaponSelectSound[p->curr_weapon][snum])
        S_StopEnvSound(aplWeaponSelectSound[p->curr_weapon][snum],p->i);
    if (aplWeaponSelectSound[weapon][snum])
        A_PlaySound(aplWeaponSelectSound[weapon][snum],p->i);
}

void P_SelectNextInvItem(DukePlayer_t *p)
{
    if (p->inv_amount[GET_FIRSTAID] > 0)
        p->inven_icon = 1;
    else if (p->inv_amount[GET_STEROIDS] > 0)
        p->inven_icon = 2;
    else if (p->inv_amount[GET_HOLODUKE] > 0)
        p->inven_icon = 3;
    else if (p->inv_amount[GET_JETPACK] > 0)
        p->inven_icon = 4;
    else if (p->inv_amount[GET_HEATS] > 0)
        p->inven_icon = 5;
    else if (p->inv_amount[GET_SCUBA] > 0)
        p->inven_icon = 6;
    else if (p->inv_amount[GET_BOOTS] > 0)
        p->inven_icon = 7;
    else p->inven_icon = 0;
}

static void P_ChangeWeapon(DukePlayer_t* const pPlayer, int const weaponNum)
{
    int const    playerNum = sprite[pPlayer->i].yvel;  // PASS_SNUM?
    int8_t const currentWeapon = pPlayer->curr_weapon;

    if (pPlayer->reloading)
        return;

    int eventReturn = 0;

    if (pPlayer->curr_weapon != weaponNum)
    {
        Gv_SetVar(sysVarIDs.RETURN, weaponNum, pPlayer->i, playerNum);
        X_OnEvent(EVENT_CHANGEWEAPON, pPlayer->i, playerNum, -1);
        eventReturn = Gv_GetVar(sysVarIDs.RETURN, pPlayer->i, playerNum);
    }

    if (eventReturn == -1)
        return;

    if (eventReturn != -2)
        pPlayer->curr_weapon = weaponNum;

    pPlayer->random_club_frame = 0;

    if (pPlayer->weapon_pos == 0)
    {
        pPlayer->weapon_pos = -1;
        pPlayer->last_weapon = currentWeapon;
    }
    else if ((unsigned)pPlayer->weapon_pos < 10)
    {
        pPlayer->weapon_pos = -pPlayer->weapon_pos;
        pPlayer->last_weapon = currentWeapon;
    }
    else if (pPlayer->last_weapon == weaponNum)
    {
        pPlayer->last_weapon = -1;
        pPlayer->weapon_pos = -pPlayer->weapon_pos;
    }

    if (pPlayer->holster_weapon)
    {
        pPlayer->weapon_pos = 10;
        pPlayer->holster_weapon = 0;
        pPlayer->last_weapon = -1;
    }

    if (currentWeapon != pPlayer->curr_weapon &&
        !(PWEAPON(playerNum, currentWeapon, WorksLike) == HANDREMOTE_WEAPON && PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike) == HANDBOMB_WEAPON) &&
        !(PWEAPON(playerNum, currentWeapon, WorksLike) == HANDBOMB_WEAPON && PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike) == HANDREMOTE_WEAPON))
    {
        pPlayer->last_used_weapon = currentWeapon;
    }

    pPlayer->kickback_pic = 0;

    P_SetWeaponGamevars(playerNum, pPlayer);
}

void P_AddWeapon(DukePlayer_t* pPlayer, int weaponNum, int switchWeapon)
{
    P_AddWeaponNoSwitch(pPlayer, weaponNum);

    if (switchWeapon)
        P_ChangeWeapon(pPlayer, weaponNum);
}

void P_CheckWeapon(DukePlayer_t* pPlayer)
{
    if (pPlayer->reloading || (unsigned)pPlayer->curr_weapon >= MAX_WEAPONS)
        return;

    int playerNum, weaponNum;

    if (pPlayer->wantweaponfire >= 0)
    {
        weaponNum = pPlayer->wantweaponfire;
        pPlayer->wantweaponfire = -1;

        if (weaponNum == pPlayer->curr_weapon)
            return;

        if (pPlayer->gotweapon[weaponNum] && pPlayer->ammo_amount[weaponNum] > 0)
        {
            P_AddWeapon(pPlayer, weaponNum, 1);
            return;
        }
    }

    weaponNum = pPlayer->curr_weapon;

    if (pPlayer->gotweapon[weaponNum] && (pPlayer->ammo_amount[weaponNum] > 0 || !(pPlayer->weaponswitch & 2)))
        return;

    playerNum = sprite[pPlayer->i].yvel;

    int wpnInc = 0;

    for (wpnInc = 0; wpnInc <= FREEZE_WEAPON; ++wpnInc)
    {
        weaponNum = g_player[playerNum].wchoice[wpnInc];
        if (VOLUMEONE && weaponNum > SHRINKER_WEAPON)
            continue;

        if (weaponNum == KNEE_WEAPON)
            weaponNum = FREEZE_WEAPON;
        else weaponNum--;

        if (weaponNum == KNEE_WEAPON || ((pPlayer->gotweapon[weaponNum]) && pPlayer->ammo_amount[weaponNum] > 0))
            break;
    }

    if (wpnInc == HANDREMOTE_WEAPON)
        weaponNum = KNEE_WEAPON;

    // Found the weapon

    P_ChangeWeapon(pPlayer, weaponNum);
}

#ifdef POLYMER
static void A_DeleteLight(int32_t s)
{
    if (practor[s].lightId >= 0)
        polymer_deletelight(practor[s].lightId);
    practor[s].lightId = -1;
    practor[s].lightptr = NULL;
}

void G_DeleteAllLights(void)
{
    for (int i = 0; i < MAXSPRITES; i++)
        A_DeleteLight(i);
}

void G_Polymer_UnInit(void)
{
    G_DeleteAllLights();
}

void G_InterpolateLights(int smoothratio)
{
#ifdef POLYMER
    uint16_t const unumsectors = (unsigned)numsectors;

    for (int i = 0; i < PR_MAXLIGHTS; i++)
    {
        auto& pr_light = prlights[i];

        if (!pr_light.flags.active || (unsigned)pr_light.owner >= MAXSPRITES)
            continue;

        uspriteptr_t pSprite = (uspriteptr_t)&sprite[pr_light.owner];

        if ((unsigned)pr_light.sector >= unumsectors || (unsigned)pSprite->sectnum >= unumsectors || pSprite->statnum == MAXSTATUS)
        {
            polymer_deletelight(i);
            continue;
        }

        auto pActor = &actor[pr_light.owner];
        auto& pr_actor = practor[pr_light.owner];

        pr_light.xyz = pSprite->xyz;
        pr_light.xyz -= pr_actor.lightoffset;
        pr_light.xyz -= { mulscale16(65536 - smoothratio, pSprite->x - pActor->bpos.x),
            mulscale16(65536 - smoothratio, pSprite->y - pActor->bpos.y),
            mulscale16(65536 - smoothratio, pSprite->z - pActor->bpos.z) };

        if (pSprite->picnum != SECTOREFFECTOR)
            pr_light.xy -= { sintable[(fix16_to_int(CAMERA(q16ang)) + 512) & 2047] >> 10,
            sintable[fix16_to_int(CAMERA(q16ang)) & 2047] >> 10 };

        pr_light.range = pr_actor.lightrange - mulscale16(65536 - smoothratio, pr_actor.lightrange - pr_actor.olightrange);
        pr_light.angle = pr_actor.lightang - mulscale16(65536 - smoothratio, ((pr_actor.lightang + 1024 - pr_actor.olightang) & 2047) - 1024);
    }
#else
    UNREFERENCED_PARAMETER(smoothratio);
#endif
}

void G_AddGameLight(int spriteNum, int sectNum, vec3_t const& offset, int lightRange, int lightRadius, int lightHoriz, uint32_t lightColor, int lightPrio)
{
#ifdef POLYMER
    uspriteptr_t s = (uspriteptr_t)&sprite[spriteNum];
    auto pr_actor = &practor[spriteNum];

    if (videoGetRenderMode() != REND_POLYMER || pr_lighting != 1)
        return;

    if (sectNum == -1)
    {
#ifdef DEBUGGINGAIDS
        LOG_F(ERROR, "Actor %d (picnum: %d) tried to create a dynamic light in NULL sector!", spriteNum, TrackerCast(sprite[spriteNum].picnum));
#endif
        return;
    }

    if (pr_actor->lightptr == NULL)
    {
#pragma pack(push, 1)
        _prlight pr_light;
#pragma pack(pop)
        Bmemset(&pr_light, 0, sizeof(pr_light));

        pr_light.sector = sectNum;
        pr_light.xyz = s->xyz;
        updatesector(pr_light.x, pr_light.y, &pr_light.sector);
        pr_light.xyz -= offset;

        pr_light.color[0] = lightColor & 255;
        pr_light.color[1] = (lightColor >> 8) & 255;
        pr_light.color[2] = (lightColor >> 16) & 255;

        if (s->pal)
        {
            int colidx = paletteGetClosestColorWithBlacklist(pr_light.color[0], pr_light.color[1], pr_light.color[2], 254, PaletteIndexFullbright);
            colidx = palookup[s->pal][colidx];
            pr_actor->lightcolidx = colidx;
            pr_light.color[0] = curpalette[colidx].r;
            pr_light.color[1] = curpalette[colidx].g;
            pr_light.color[2] = curpalette[colidx].b;
        }
        else
            pr_actor->lightcolidx = 0;

        pr_light.radius = lightRadius;
        pr_light.priority = lightPrio;
        pr_light.tilenum = 0;
        pr_light.owner = spriteNum;
        pr_light.horiz = lightHoriz;

        pr_light.publicflags.emitshadow = 1;
        pr_light.publicflags.negative = 0;

        pr_actor->lightrange = pr_actor->olightrange = pr_actor->lightmaxrange = pr_light.range = lightRange;
        pr_actor->lightang = pr_actor->olightang = pr_light.angle = s->ang;
        pr_actor->lightoffset = offset;

        pr_actor->lightId = polymer_addlight(&pr_light);
        if (pr_actor->lightId >= 0)
            pr_actor->lightptr = &prlights[pr_actor->lightId];
        return;
    }

    _prlight& pr_light = *pr_actor->lightptr;

    if (lightRange < pr_actor->lightmaxrange >> 1)
        pr_actor->lightmaxrange = 0;

    if (lightRange > pr_actor->lightmaxrange || lightPrio != pr_light.priority || sprite[spriteNum].xyz != pr_light.xyz + offset
        || (lightRadius && pr_light.angle != s->ang))
    {
        if (lightRange > pr_actor->lightmaxrange)
            pr_actor->lightmaxrange = lightRange;

        pr_light.xyz = sprite[spriteNum].xyz;
        pr_light.xyz -= offset;
        pr_light.sector = sectNum;
        updatesector(pr_light.x, pr_light.y, &pr_light.sector);
        pr_light.flags.invalidate = 1;
    }

    pr_actor->lightoffset = offset;
    pr_light.priority = lightPrio;
    pr_actor->olightrange = pr_actor->lightrange;
    pr_light.range = pr_actor->lightrange = lightRange;
    pr_actor->olightang = pr_actor->lightang;
    pr_light.angle = pr_actor->lightang = s->ang;
    pr_light.color[0] = lightColor & 255;
    pr_light.color[1] = (lightColor >> 8) & 255;
    pr_light.color[2] = (lightColor >> 16) & 255;
    pr_light.horiz = lightHoriz;

    if (pr_actor->lightcolidx)
    {
        int colidx = pr_actor->lightcolidx;
        pr_light.color[0] = curpalette[colidx].r;
        pr_light.color[1] = curpalette[colidx].g;
        pr_light.color[2] = curpalette[colidx].b;
    }
#else
    auto unusedParameterWarningsOnConstReferencesSuck = offset;
    UNREFERENCED_PARAMETER(unusedParameterWarningsOnConstReferencesSuck);
    UNREFERENCED_PARAMETER(lightRadius);
    UNREFERENCED_PARAMETER(spriteNum);
    UNREFERENCED_PARAMETER(sectNum);
    UNREFERENCED_PARAMETER(lightRange);
    UNREFERENCED_PARAMETER(lightHoriz);
    UNREFERENCED_PARAMETER(lightColor);
    UNREFERENCED_PARAMETER(lightPrio);
#endif
}

static void G_DoEffectorLights(void)  // STATNUM 14
{
    static int16_t lasti = -1;
    int16_t i;

    if (lasti != -1 && sprite[lasti].statnum == STAT_LIGHT)
    {
        i = lasti;
        goto in;
    }

    for (SPRITES_OF(STAT_LIGHT, i))
    {
    in:
        if ((int32_t)(totalclock - ototalclock) >= TICSPERFRAME)
        {
            lasti = i;
            return;
        }

        switch (sprite[i].lotag)
        {
            case SE_49_POINT_LIGHT:
            {
                if (!A_CheckSpriteFlags(i, SFLAG_NOLIGHT) && videoGetRenderMode() == REND_POLYMER &&
                    !(A_CheckSpriteFlags(i, SFLAG_USEACTIVATOR) && sector[sprite[i].sectnum].lotag & 16384))
                {
                    if (practor[i].lightptr == NULL)
                    {
#pragma pack(push,1)
                        _prlight mylight;
#pragma pack(pop)
                        mylight.sector = SECT(i);
                        mylight.xyz = sprite[i].xyz;
                        mylight.range = SHT(i);
                        mylight.color[0] = sprite[i].xvel;
                        mylight.color[1] = sprite[i].yvel;
                        mylight.color[2] = sprite[i].zvel;
                        mylight.radius = 0;
                        mylight.angle = SA(i);
                        mylight.horiz = SH(i);
                        mylight.minshade = sprite[i].xoffset;
                        mylight.maxshade = sprite[i].yoffset;
                        mylight.tilenum = 0;
                        mylight.publicflags.emitshadow = 0;
                        mylight.publicflags.negative = !!(CS(i) & 128);
                        mylight.owner = i;

                        if (CS(i) & 2)
                        {
                            if (CS(i) & 512)
                                mylight.priority = PR_LIGHT_PRIO_LOW;
                            else
                                mylight.priority = PR_LIGHT_PRIO_HIGH;
                        }
                        else
                            mylight.priority = PR_LIGHT_PRIO_MAX;

                        practor[i].lightId = polymer_addlight(&mylight);
                        if (practor[i].lightId >= 0)
                        {
                            practor[i].lightptr = &prlights[practor[i].lightId];
                            practor[i].lightrange = practor[i].olightrange = mylight.range;
                            practor[i].lightoffset = {};
                        }
                        break;
                    }

                    if (SHT(i) != practor[i].lightptr->range)
                    {
                        practor[i].lightptr->range = SHT(i);
                        practor[i].lightptr->flags.invalidate = 1;
                    }
                    if ((sprite[i].xvel != practor[i].lightptr->color[0]) ||
                        (sprite[i].yvel != practor[i].lightptr->color[1]) ||
                        (sprite[i].zvel != practor[i].lightptr->color[2]))
                    {
                        practor[i].lightptr->color[0] = sprite[i].xvel;
                        practor[i].lightptr->color[1] = sprite[i].yvel;
                        practor[i].lightptr->color[2] = sprite[i].zvel;
                    }
                    if ((int)!!(CS(i) & 128) != practor[i].lightptr->publicflags.negative) {
                        practor[i].lightptr->publicflags.negative = !!(CS(i) & 128);
                    }
                }
                break;
            }
            case SE_50_SPOT_LIGHT:
            {
                if (!A_CheckSpriteFlags(i, SFLAG_NOLIGHT) && videoGetRenderMode() == REND_POLYMER &&
                    !(A_CheckSpriteFlags(i, SFLAG_USEACTIVATOR) && sector[sprite[i].sectnum].lotag & 16384))
                {
                    if (practor[i].lightptr == NULL)
                    {
#pragma pack(push,1)
                        _prlight mylight;
#pragma pack(pop)

                        mylight.sector = SECT(i);
                        mylight.xyz = sprite[i].xyz;
                        mylight.range = SHT(i);
                        mylight.color[0] = sprite[i].xvel;
                        mylight.color[1] = sprite[i].yvel;
                        mylight.color[2] = sprite[i].zvel;
                        mylight.radius = (256 - (SS(i) + 128)) << 1;
                        mylight.faderadius = (int16_t)(mylight.radius * 0.75f);
                        mylight.angle = SA(i);
                        mylight.horiz = SH(i);
                        mylight.minshade = sprite[i].xoffset;
                        mylight.maxshade = sprite[i].yoffset;
                        mylight.tilenum = actor[i].htpicnum;
                        mylight.publicflags.emitshadow = !(CS(i) & 64);
                        mylight.publicflags.negative = !!(CS(i) & 128);
                        mylight.owner = i;

                        if (CS(i) & 2)
                        {
                            if (CS(i) & 512)
                                mylight.priority = PR_LIGHT_PRIO_LOW;
                            else
                                mylight.priority = PR_LIGHT_PRIO_HIGH;
                        }
                        else
                            mylight.priority = PR_LIGHT_PRIO_MAX;

                        practor[i].lightId = polymer_addlight(&mylight);
                        if (practor[i].lightId >= 0)
                        {
                            practor[i].lightptr = &prlights[practor[i].lightId];
                            practor[i].lightrange = practor[i].olightrange = mylight.range;
                            // Hack in case polymer_addlight tweaked the horiz value
                            if (practor[i].lightptr->horiz != SH(i))
                                SH(i) = practor[i].lightptr->horiz;
                            practor[i].lightoffset = {};
                        }
                        break;
                    }

                    if (SHT(i) != practor[i].lightptr->range)
                    {
                        practor[i].lightptr->range = SHT(i);
                        practor[i].lightptr->flags.invalidate = 1;
                    }
                    if ((sprite[i].xvel != practor[i].lightptr->color[0]) ||
                        (sprite[i].yvel != practor[i].lightptr->color[1]) ||
                        (sprite[i].zvel != practor[i].lightptr->color[2]))
                    {
                        practor[i].lightptr->color[0] = sprite[i].xvel;
                        practor[i].lightptr->color[1] = sprite[i].yvel;
                        practor[i].lightptr->color[2] = sprite[i].zvel;
                    }
                    if (((256 - (SS(i) + 128)) << 1) != practor[i].lightptr->radius)
                    {
                        practor[i].lightptr->radius = (256 - (SS(i) + 128)) << 1;
                        practor[i].lightptr->faderadius = (int16_t)(practor[i].lightptr->radius * 0.75f);
                        practor[i].lightptr->flags.invalidate = 1;
                    }
                    if (SA(i) != practor[i].lightptr->angle)
                    {
                        practor[i].lightptr->angle = SA(i);
                        practor[i].lightptr->flags.invalidate = 1;
                    }
                    if (SH(i) != practor[i].lightptr->horiz)
                    {
                        practor[i].lightptr->horiz = SH(i);
                        practor[i].lightptr->flags.invalidate = 1;
                    }
                    if ((int)!(CS(i) & 64) != practor[i].lightptr->publicflags.emitshadow) {
                        practor[i].lightptr->publicflags.emitshadow = !(CS(i) & 64);
                    }
                    if ((int)!!(CS(i) & 128) != practor[i].lightptr->publicflags.negative) {
                        practor[i].lightptr->publicflags.negative = !!(CS(i) & 128);
                    }
                    practor[i].lightptr->tilenum = actor[i].htpicnum;
                }

                break;
            }
        }
    }

    lasti = -1;
}

int savedFires = 0;

static void A_DoLight(int spriteNum)
{
    auto const pSprite = &sprite[spriteNum];

    if (pSprite->statnum == STAT_PLAYER)
    {
        if (practor[spriteNum].lightptr != NULL && practor[spriteNum].lightcount)
        {
            if (!(--practor[spriteNum].lightcount))
                A_DeleteLight(spriteNum);
        }
    }
    else if (((sector[pSprite->sectnum].floorz - sector[pSprite->sectnum].ceilingz) < 16) || pSprite->z > sector[pSprite->sectnum].floorz || pSprite->z > actor[spriteNum].floorz ||
        (pSprite->picnum != SECTOREFFECTOR && ((pSprite->cstat & 32768) || pSprite->yrepeat < 4)) ||
        A_CheckSpriteFlags(spriteNum, SFLAG_NOLIGHT) || (A_CheckSpriteFlags(spriteNum, SFLAG_USEACTIVATOR) && sector[pSprite->sectnum].lotag & 16384))
    {
        if (practor[spriteNum].lightptr != NULL)
            A_DeleteLight(spriteNum);
    }
    else
    {
        if (practor[spriteNum].lightptr != NULL && practor[spriteNum].lightcount)
        {
            if (!(--practor[spriteNum].lightcount))
                A_DeleteLight(spriteNum);
        }

        if (pr_lighting != 1 || r_pr_defaultlights < 1)
            return;

#ifndef EDUKE32_STANDALONE
        if (FURY)
            return;

        for (int ii = 0; ii < 2; ii++)
        {
            if (pSprite->picnum <= 0)  // oob safety
                break;

            switch (tileGetMapping(pSprite->picnum - 1 + ii))
            {
                case DIPSWITCH__:
                case DIPSWITCH2__:
                case DIPSWITCH3__:
                case PULLSWITCH__:
                case SLOTDOOR__:
                case LIGHTSWITCH__:
                case SPACELIGHTSWITCH__:
                case SPACEDOORSWITCH__:
                case FRANKENSTINESWITCH__:
                case POWERSWITCH1__:
                case LOCKSWITCH1__:
                case POWERSWITCH2__:
                case TECHSWITCH__:
                case ACCESSSWITCH__:
                case ACCESSSWITCH2__:
                {
                    if ((pSprite->cstat & 32768) || A_CheckSpriteFlags(spriteNum, SFLAG_NOLIGHT))
                    {
                        if (practor[spriteNum].lightptr != NULL)
                            A_DeleteLight(spriteNum);
                        break;
                    }

                    vec3_t const d = { -(sintable[(pSprite->ang + 512) & 2047] >> 7), -(sintable[(pSprite->ang) & 2047] >> 7), LIGHTZOFF(spriteNum) };

                    int16_t sectnum = pSprite->sectnum;
                    updatesector(pSprite->x, pSprite->y, &sectnum);

                    if ((unsigned)sectnum < MAXSECTORS && pSprite->z <= sector[sectnum].floorz && pSprite->z >= sector[sectnum].ceilingz)
                        G_AddGameLight(spriteNum, pSprite->sectnum, d, 384 - ii * 64, 0, 100, (ii == 0) ? RGB_TO_INT(172, 200, 104) : RGB_TO_INT(216, 52, 20), PR_LIGHT_PRIO_LOW);
                }
                break;
            }
        }

        switch (tileGetMapping(pSprite->picnum))
        {
            case ATOMICHEALTH__:
            case FREEZEAMMO__:
                G_AddGameLight(spriteNum, pSprite->sectnum, { 0, 0, LIGHTZOFF(spriteNum) }, LIGHTRAD2(spriteNum), 0, 100, RGB_TO_INT(128, 128, 255), PR_LIGHT_PRIO_HIGH_GAME);
                break;

            case FIRE__:
            case FIRE2__:
            case BURNING__:
            case BURNING2__:
            {
                uint32_t color;
                int32_t jj;

                static int32_t savedfires[32][4];  // sectnum x y z

                /*
                if (Actor[i].floorz - Actor[i].ceilingz < 128) break;
                if (s->z > Actor[i].floorz+2048) break;
                */

                switch (pSprite->pal)
                {
                    case 1: color = RGB_TO_INT(128, 128, 255); break;
                    case 2: color = RGB_TO_INT(255, 48, 48); break;
                    case 8: color = RGB_TO_INT(48, 255, 48); break;
                    default: color = RGB_TO_INT(240, 160, 80); break;
                }

                for (jj = savedFires - 1; jj >= 0; jj--)
                    if (savedfires[jj][0] == pSprite->sectnum && savedfires[jj][1] == (pSprite->x >> 3) &&
                        savedfires[jj][2] == (pSprite->y >> 3) && savedfires[jj][3] == (pSprite->z >> 7))
                        break;

                if (jj == -1 && savedFires < 32)
                {
                    jj = savedFires;
                    G_AddGameLight(spriteNum, pSprite->sectnum, { 0, 0, LIGHTZOFF(spriteNum) }, LIGHTRAD2(spriteNum), 0, 100, color, PR_LIGHT_PRIO_HIGH_GAME);
                    savedfires[jj][0] = pSprite->sectnum;
                    savedfires[jj][1] = pSprite->x >> 3;
                    savedfires[jj][2] = pSprite->y >> 3;
                    savedfires[jj][3] = pSprite->z >> 7;
                    savedFires++;
                }
            }
            break;

            case OOZFILTER__:
                if (pSprite->xrepeat > 4)
                    G_AddGameLight(spriteNum, pSprite->sectnum, { 0, 0, LIGHTZOFF(spriteNum) }, LIGHTRAD2(spriteNum), 0, 100, RGB_TO_INT(176, 252, 120), PR_LIGHT_PRIO_HIGH_GAME);
                break;
            case FLOORFLAME__:
            case FIREBARREL__:
            case FIREVASE__:
                G_AddGameLight(spriteNum, pSprite->sectnum, { 0, 0, LIGHTZOFF(spriteNum) << 1 }, LIGHTRAD2(spriteNum) >> 1, 0, 100, RGB_TO_INT(255, 95, 0), PR_LIGHT_PRIO_HIGH_GAME);
                break;

            case EXPLOSION2__:
                if (!practor[spriteNum].lightcount)
                {
                    // XXX: This block gets CODEDUP'd too much.
                    vec3_t const offset = { ((sintable[(pSprite->ang + 512) & 2047]) >> 6), ((sintable[(pSprite->ang) & 2047]) >> 6), LIGHTZOFF(spriteNum) };
                    G_AddGameLight(spriteNum, pSprite->sectnum, offset, LIGHTRAD(spriteNum), 0, 100,
                        RGB_TO_INT(240, 160, 80), pSprite->yrepeat > 32 ? PR_LIGHT_PRIO_HIGH_GAME : PR_LIGHT_PRIO_LOW_GAME);
                }
                break;
            case FORCERIPPLE__:
            {
                vec3_t const offset = { -((sintable[(pSprite->ang + 512) & 2047]) >> 5), -((sintable[(pSprite->ang) & 2047]) >> 5), LIGHTZOFF(spriteNum) };
                G_AddGameLight(spriteNum, pSprite->sectnum, offset, LIGHTRAD(spriteNum), 0, 100, RGB_TO_INT(80, 40, 255), PR_LIGHT_PRIO_LOW_GAME);
                practor[spriteNum].lightcount = 2;
            }
            break;
            case TRANSPORTERBEAM__:
                G_AddGameLight(spriteNum, pSprite->sectnum, { 0, 0, LIGHTZOFF(spriteNum) }, LIGHTRAD(spriteNum), 0, 100, RGB_TO_INT(80, 80, 255), PR_LIGHT_PRIO_LOW_GAME);
                practor[spriteNum].lightcount = 2;
                break;
            case GROWSPARK__:
            {
                vec3_t const offset = { ((sintable[(pSprite->ang + 512) & 2047]) >> 6), ((sintable[(pSprite->ang) & 2047]) >> 6), LIGHTZOFF(spriteNum) };
                G_AddGameLight(spriteNum, pSprite->sectnum, offset, LIGHTRAD(spriteNum), 0, 100, RGB_TO_INT(216, 52, 20), PR_LIGHT_PRIO_HIGH_GAME);
            }
            break;
            case NEON1__:
            case NEON3__:
            case NEON4__:
            case NEON5__:
            case NEON6__:
            {
                vec3_t const offset = { ((sintable[(pSprite->ang + 512) & 2047]) >> 6), ((sintable[(pSprite->ang) & 2047]) >> 6), LIGHTZOFF(spriteNum) };
                G_AddGameLight(spriteNum, pSprite->sectnum, offset, LIGHTRAD(spriteNum) >> 2, 0, 100, RGB_TO_INT(216, 52, 20), PR_LIGHT_PRIO_HIGH_GAME);
            }
            break;
            case LASERLINE__:
            {
                G_AddGameLight(spriteNum, pSprite->sectnum, { 0, 0, LIGHTZOFF(spriteNum) }, 1280, 0, 100, RGB_TO_INT(216, 52, 20), PR_LIGHT_PRIO_HIGH_GAME);
                practor[spriteNum].lightcount = 2;
            }
            break;
            case SHRINKEREXPLOSION__:
            {
                vec3_t const offset = { ((sintable[(pSprite->ang + 512) & 2047]) >> 6), ((sintable[(pSprite->ang) & 2047]) >> 6), LIGHTZOFF(spriteNum) };
                G_AddGameLight(spriteNum, pSprite->sectnum, offset, LIGHTRAD(spriteNum), 0, 100, RGB_TO_INT(176, 252, 120), PR_LIGHT_PRIO_HIGH_GAME);
            }
            break;
            case NEON2__:
            case FREEZEBLAST__:
                G_AddGameLight(spriteNum, pSprite->sectnum, { 0, 0, LIGHTZOFF(spriteNum) }, LIGHTRAD(spriteNum) << 2, 0, 100, RGB_TO_INT(72, 88, 140), PR_LIGHT_PRIO_HIGH_GAME);
                break;
            case REACTOR__:
            case REACTOR2__:
            case REACTORSPARK__:
            case REACTOR2SPARK__:
            case BOLT1__:
            case SIDEBOLT1__:
                G_AddGameLight(spriteNum, pSprite->sectnum, { 0, 0, LIGHTZOFF(spriteNum) }, LIGHTRAD(spriteNum), 0, 100, RGB_TO_INT(72, 88, 140), PR_LIGHT_PRIO_HIGH_GAME);
                practor[spriteNum].lightcount = 2;
                break;
            case COOLEXPLOSION1__:
                G_AddGameLight(spriteNum, pSprite->sectnum, { 0, 0, LIGHTZOFF(spriteNum) }, LIGHTRAD(spriteNum) << 2, 0, 100, RGB_TO_INT(128, 0, 255), PR_LIGHT_PRIO_HIGH_GAME);
                break;
            case SHRINKSPARK__:
            case CRYSTALAMMO__:
            case 679: // battlelord head thing
            case 490: // cycloid head thing
                G_AddGameLight(spriteNum, pSprite->sectnum, { 0, 0, LIGHTZOFF(spriteNum) }, LIGHTRAD(spriteNum), 0, 100, RGB_TO_INT(176, 252, 120), PR_LIGHT_PRIO_HIGH_GAME);
                break;
            case FIRELASER__:
                if (pSprite->statnum == STAT_PROJECTILE)
                    G_AddGameLight(spriteNum, pSprite->sectnum, { 0, 0, LIGHTZOFF(spriteNum) }, 64 * pSprite->yrepeat, 0, 100, RGB_TO_INT(255, 95, 0), PR_LIGHT_PRIO_LOW_GAME);
                break;
            case RPG__:
                G_AddGameLight(spriteNum, pSprite->sectnum, { 0, 0, LIGHTZOFF(spriteNum) }, LIGHTRAD3(spriteNum) << 2, 0, 100, RGB_TO_INT(255, 95, 0), PR_LIGHT_PRIO_LOW_GAME);
                break;
            case SHOTSPARK1__:
                if (AC_ACTION_COUNT(actor[spriteNum].t_data) == 0) // check for first frame of action
                {
                    vec3_t const offset = { ((sintable[(pSprite->ang + 512) & 2047]) >> 6), ((sintable[(pSprite->ang) & 2047]) >> 6), LIGHTZOFF(spriteNum) };
                    G_AddGameLight(spriteNum, pSprite->sectnum, offset, LIGHTRAD3(spriteNum) << 1, 0, 100, RGB_TO_INT(240, 160, 80), PR_LIGHT_PRIO_LOW_GAME);
                    practor[spriteNum].lightcount = 1;
                }
                break;

            case RECON__:
            {
                vec3_t const offset = { 0, 0, -2048 };
                auto& a = actor[spriteNum];

                uint32_t color = RGB_TO_INT(255, 255, 255);
                int      radius = 256;
                int      range = 8192;

                if (a.t_data[0] == 3)
                {
                    color = everyothertime & 1 ? RGB_TO_INT(255, 0, 0) : RGB_TO_INT(0, 0, 255);
                    radius <<= 1;
                    range <<= 1;
                }

                G_AddGameLight(spriteNum, pSprite->sectnum, offset, range, radius, (pSprite->xvel >> 2) - 100, color, PR_LIGHT_PRIO_HIGH_GAME);
                break;
            }
            case DRONE__:
            case TANK__:
            {
                vec3_t const offset = { 0, 0, 8192 };
                G_AddGameLight(spriteNum, pSprite->sectnum, offset, 4096, 256, 100, 255/*+(255<<8)+(255<<16)*/, PR_LIGHT_PRIO_HIGH_GAME);
                break;
            }
            case DOMELITE__:
            {
                vec3_t offset = { 0, 0, LIGHTZOFF(spriteNum) << 2 };
                if (pSprite->cstat & 8)
                    offset.z = -offset.z;
                pSprite->ang += 64;
                G_AddGameLight(spriteNum, pSprite->sectnum, offset, LIGHTRAD3(spriteNum) << 3, 384, 100, 255, PR_LIGHT_PRIO_LOW_GAME);
                break;
            }

            case FOOTPRINTS__:
            case FOOTPRINTS2__:
            case FOOTPRINTS3__:
            case FOOTPRINTS4__:
                if (pSprite->pal != 8)
                    break;
                fallthrough__;
            case BLOODPOOL__:
            {
                uint32_t color;

                switch (pSprite->pal)
                {
                    case 1:
                        color = RGB_TO_INT(72, 88, 140);
                        break;
                    case 0:
                    case 8:
                        color = RGB_TO_INT(172, 200, 104);
                        break;
                    default:
                        color = 0;
                        break;
                }

                if (color)
                    G_AddGameLight(spriteNum, pSprite->sectnum, { 0, 0, (pSprite->xrepeat << 4) }, (pSprite->xrepeat << 3), 0, 100, color, PR_LIGHT_PRIO_LOW);
            }
        }
#endif // EDUKE32_STANDALONE
    }
}

void G_RefreshLights(void)
{
#ifdef POLYMER
    if (!Numsprites || videoGetRenderMode() != REND_POLYMER)
        return;

    int statNum = 0;
    savedFires = 0;
    do
    {
        for (int SPRITES_OF(statNum++, spriteNum))
            A_DoLight(spriteNum);
    } while (statNum < MAXSTATUS);
#endif
}
#endif // POLYMER

void A_RadiusDamage(int i, int  r, int  hp1, int  hp2, int  hp3, int  hp4)
{
    spritetype *s=&sprite[i],*sj;
    walltype *wal;
    int d, q, x1, y1;
    int sectcnt, sectend, dasect, startwall, endwall, nextsect;
    int j,k,p,x,nextj;
    short sect=-1;
    char statlist[] = {STAT_DEFAULT,STAT_ACTOR,STAT_STANDABLE,
                       STAT_PLAYER,STAT_FALLER,STAT_ZOMBIEACTOR,STAT_MISC
                      };
    short *tempshort = (short *)tempbuf;

    bool const additiveDamage = (r < 0);

    r = klabs(r);

    ud.returnvar[0] = r; // Allow checking for radius damage in EVENT_DAMAGE(SPRITE/WALL/FLOOR/CEILING) events.
    ud.returnvar[1] = hp1;
    ud.returnvar[2] = hp2;
    ud.returnvar[3] = hp3;
    ud.returnvar[4] = hp4;

    if (s->picnum == RPG && s->xrepeat < 11) goto SKIPWALLCHECK;

    if (s->picnum != SHRINKSPARK)
    {
        tempshort[0] = s->sectnum;
        dasect = s->sectnum;
        sectcnt = 0;
        sectend = 1;

        do
        {
            dasect = tempshort[sectcnt++];
            if (((sector[dasect].ceilingz-s->z)>>8) < r)
            {
                d = klabs(wall[sector[dasect].wallptr].x-s->x)+klabs(wall[sector[dasect].wallptr].y-s->y);
                if (d < r)
                    Sect_DamageCeiling_Internal(i, dasect);
                else
                {
                    d = klabs(wall[wall[wall[sector[dasect].wallptr].point2].point2].x-s->x)+klabs(wall[wall[wall[sector[dasect].wallptr].point2].point2].y-s->y);
                    if (d < r)
                        Sect_DamageCeiling_Internal(i, dasect);
                }
            }

            startwall = sector[dasect].wallptr;
            endwall = startwall+sector[dasect].wallnum;
            for (x=startwall,wal=&wall[startwall];x<endwall;x++,wal++)
                if ((klabs(wal->x-s->x)+klabs(wal->y-s->y)) < r)
                {
                    nextsect = wal->nextsector;
                    if (nextsect >= 0)
                    {
                        for (dasect=sectend-1;dasect>=0;dasect--)
                            if (tempshort[dasect] == nextsect) break;
                        if (dasect < 0) tempshort[sectend++] = nextsect;
                    }
                    x1 = (((wal->x+wall[wal->point2].x)>>1)+s->x)>>1;
                    y1 = (((wal->y+wall[wal->point2].y)>>1)+s->y)>>1;
                    updatesector(x1,y1,&sect);
                    if (sect >= 0 && cansee(x1,y1,s->z,sect,s->x,s->y,s->z,s->sectnum))
                        A_DamageWall_Internal(i, x, { wal->x, wal->y, s->z }, s->picnum);
                }
        }
        while (sectcnt < sectend);
    }

SKIPWALLCHECK:

    q = -(16<<8)+(krand()&((32<<8)-1));

    for (x = 0;x<7;x++)
    {
        j = headspritestat[(unsigned char)statlist[x]];
        while (j >= 0)
        {
            nextj = nextspritestat[j];
            sj = &sprite[j];

            if (x == 0 || x >= 5 || AFLAMABLE(sj->picnum))
            {
                if (s->picnum != SHRINKSPARK || (sj->cstat&257))
                    if (dist(s, sj) < r)
                    {
                        if (A_CheckEnemySprite(sj) && !cansee(sj->x, sj->y,sj->z+q, sj->sectnum, s->x, s->y, s->z+q, s->sectnum))
                            goto BOLT;
                        A_DamageObject_Internal(j, i);
                    }
            }
            else if (sj->extra >= 0 && sj != s && (sj->picnum == TRIPBOMB || A_CheckEnemySprite(sj) || sj->picnum == QUEBALL || sj->picnum == STRIPEBALL || (sj->cstat&257) || sj->picnum == DUKELYINGDEAD))
            {
                if (s->picnum == SHRINKSPARK && sj->picnum != SHARK && (j == s->owner || sj->xrepeat < 24))
                {
                    j = nextj;
                    continue;
                }
                if (s->picnum == MORTER && j == s->owner)
                {
                    j = nextj;
                    continue;
                }

                if (sj->picnum == APLAYER) sj->z -= PHEIGHT;
                d = dist(s, sj);
                if (sj->picnum == APLAYER) sj->z += PHEIGHT;

                if (d < r && cansee(sj->x, sj->y, sj->z-(8<<8), sj->sectnum, s->x, s->y, s->z-(12<<8), s->sectnum))
                {
                    if (A_CheckSpriteFlags(j, SFLAG_DAMAGEEVENT))
                    {
                        Gv_SetVar(sysVarIDs.RETURN, j, i, -1);
                        X_OnEvent(EVENT_DAMAGESPRITE, i, -1, -1);

                        if (Gv_GetVar(sysVarIDs.RETURN, i, -1) < 0)
                            goto BOLT;
                    }

                    actor[j].htang = getangle(sj->x-s->x,sj->y-s->y);

                    if (s->picnum == RPG && sj->extra > 0)
                        actor[j].htpicnum = RPG;
                    else if (A_CheckSpriteFlags(i,SFLAG_PROJECTILE) && actor[i].projectile.workslike & PROJECTILE_RADIUS_PICNUM && sj->extra > 0)
                        actor[j].htpicnum = s->picnum;
                    else
                    {
                        if (s->picnum == SHRINKSPARK)
                            actor[j].htpicnum = SHRINKSPARK;
                        else actor[j].htpicnum = RADIUSEXPLOSION;
                    }

                    if (s->picnum != SHRINKSPARK)
                    {
                        k = (r/3);
                        int16_t damage = 0;

                        if (d < k)
                        {
                            if (hp4 == hp3) hp4++;
                            damage = hp3 + (krand()%(hp4-hp3));
                        }
                        else if (d < (k*2))
                        {
                            if (hp3 == hp2) hp3++;
                            damage = hp2 + (krand()%(hp3-hp2));
                        }
                        else if (d < r)
                        {
                            if (hp2 == hp1) hp2++;
                            damage = hp1 + (krand()%(hp2-hp1));
                        }

                        if (additiveDamage)
                            actor[j].htextra += damage;
                        else
                            actor[j].htextra = damage;

                        if (!A_CheckSpriteFlags(j, SFLAG_NODAMAGEPUSH))
                        {
                            if (sj->xvel < 0) sj->xvel = 0;
                            sj->xvel += (s->extra<<2);
                        }

                        if (A_CheckSpriteFlags(j, SFLAG_DAMAGEEVENT))
                        {
                            Gv_SetVar(sysVarIDs.RETURN, j, i, -1);
                            X_OnEvent(EVENT_POSTDAMAGESPRITE, i, -1, -1);
                        }

                        if (sj->picnum == PODFEM1 || sj->picnum == FEM1 ||
                                sj->picnum == FEM2 || sj->picnum == FEM3 ||
                                sj->picnum == FEM4 || sj->picnum == FEM5 ||
                                sj->picnum == FEM6 || sj->picnum == FEM7 ||
                                sj->picnum == FEM8 || sj->picnum == FEM9 ||
                                sj->picnum == FEM10 || sj->picnum == STATUE ||
                                sj->picnum == STATUEFLASH || sj->picnum == SPACEMARINE || sj->picnum == QUEBALL || sj->picnum == STRIPEBALL)
                            A_DamageObject_Internal(j, i);
                    }
                    else if (s->extra == 0) actor[j].htextra = 0;

                    if (sj->picnum != RADIUSEXPLOSION &&
                            s->owner >= 0 && sprite[s->owner].statnum < MAXSTATUS)
                    {
                        if (sj->picnum == APLAYER)
                        {
                            p = sj->yvel;
                            if (g_player[p].ps->newowner >= 0)
                            {
                                G_ClearCameraView(g_player[p].ps);
                            }
                        }
                        actor[j].htowner = s->owner;
                    }
                }
            }
BOLT:
            j = nextj;
        }
    }
}

int A_MoveSprite(int spritenum, int xchange, int ychange, int zchange, unsigned int cliptype)
{
    vec3_t oldpos;

    int daz;
    int retval;
    short dasectnum, cd;
    int isEnemy = A_CheckEnemySprite(&sprite[spritenum]);

    auto const pSprite = &sprite[spritenum];
    auto const pActor = &actor[spritenum];

    if (pSprite->statnum == STAT_MISC || (isEnemy && pSprite->xrepeat < 4))
    {
        pSprite->x += (xchange*TICSPERFRAME)>>2;
        pSprite->y += (ychange*TICSPERFRAME)>>2;
        pSprite->z += (zchange*TICSPERFRAME)>>2;

        if (isEnemy)
            setsprite(spritenum, &pSprite->xyz);

        return 0;
    }

    dasectnum = pSprite->sectnum;
    auto sectLotag = sector[dasectnum].lotag;

    oldpos.x = pSprite->x;
    oldpos.y = pSprite->y;

    daz = pSprite->z - ((tilesiz[pSprite->picnum].y*pSprite->yrepeat)<<1);

    if (yax_getbunch(dasectnum, (zchange > 0)) >= 0
        && (SECTORFLD(dasectnum, stat, (zchange > 0)) & yax_waltosecmask(cliptype)) == 0)
    {
        setspritez(spritenum, &pSprite->xyz);
        dasectnum = pSprite->sectnum;
    }

    if (isEnemy)
    {
        if (pSprite->xrepeat > 60)
            retval = clipmove_old(&pSprite->x,&pSprite->y,&daz,&dasectnum,((xchange*TICSPERFRAME)<<11),((ychange*TICSPERFRAME)<<11),1024L,(4<<8),(4<<8),cliptype);
        else
        {
            if (sprite[spritenum].picnum == LIZMAN)
                cd = 292L;
            else if (A_CheckSpriteFlags(spritenum, SFLAG_BADGUY))
                cd = sprite[spritenum].clipdist<<2;
            else
                cd = 192L;

            retval = clipmove_old(&pSprite->x,&pSprite->y,&daz,&dasectnum,((xchange*TICSPERFRAME)<<11),((ychange*TICSPERFRAME)<<11),cd,(4<<8),(4<<8),cliptype);
        }

        if (dasectnum < 0 || (dasectnum >= 0 &&
                              ((pActor->actorstayput >= 0 && pActor->actorstayput != dasectnum) ||
                               ((pSprite->picnum == BOSS2) && pSprite->pal == 0 && sector[dasectnum].lotag != ST_3) ||
                               ((pSprite->picnum == BOSS1 || pSprite->picnum == BOSS2) && sector[dasectnum].lotag == ST_1_ABOVE_WATER) /*||
                               (sector[dasectnum].lotag == ST_1_ABOVE_WATER && (sprite[spritenum].picnum == LIZMAN || (sprite[spritenum].picnum == LIZTROOP && sprite[spritenum].zvel == 0)))*/
                              ))
           )
        {
            pSprite->x = oldpos.x;
            pSprite->y = oldpos.y;

            /*            if (dasectnum >= 0 && sector[dasectnum].lotag == ST_1_ABOVE_WATER && sprite[spritenum].picnum == LIZMAN)
                            sprite[spritenum].ang = (krand()&2047);
                        else if ((actor[spritenum].t_data[0]&3) == 1 && sprite[spritenum].picnum != COMMANDER)
                            sprite[spritenum].ang = (krand()&2047); */

            oldpos.z = pSprite->z;
            setspritez(spritenum, &oldpos);

            if (dasectnum < 0) dasectnum = 0;
            return (16384+dasectnum);
        }

        if ((retval&49152) >= 32768 && (pActor->cgg==0))
            pSprite->ang += 768;
    }
    else
    {
        if (pSprite->statnum == STAT_PROJECTILE)
            retval =
                clipmove_old(&pSprite->x,&pSprite->y,&daz,&dasectnum,((xchange*TICSPERFRAME)<<11),((ychange*TICSPERFRAME)<<11),8L,(4<<8),(4<<8),cliptype);
        else
            retval =
                clipmove_old(&pSprite->x,&pSprite->y,&daz,&dasectnum,((xchange*TICSPERFRAME)<<11),((ychange*TICSPERFRAME)<<11),(int)(pSprite->clipdist<<2),(4<<8),(4<<8),cliptype);
    }

    if (dasectnum >= 0)
        if ((dasectnum != pSprite->sectnum))
            changespritesect(spritenum,dasectnum);

    daz = pSprite->z + ((zchange * TICSPERFRAME) >> 3);

    if ((zchange < 0 && sectLotag == ST_2_UNDERWATER) || (zchange > 0 && sectLotag == ST_1_ABOVE_WATER))
    {
        int32_t otherSect;
        if (A_CheckNoSE7Water((uspriteptr_t)&sprite[spritenum], dasectnum, sectLotag, &otherSect))
        {
            if (((daz < pActor->ceilingz) && sectLotag == ST_2_UNDERWATER) ||
                (daz > pActor->floorz && sectLotag == ST_1_ABOVE_WATER))
            {
                A_Spawn(spritenum, WATERSPLASH2);
                changespritesect(spritenum, otherSect);
                sprite[spritenum].z = daz;
                return 0;
            }
        }
    }
    
    if (((daz > pActor->ceilingz) && (daz <= pActor->floorz))
        || ((sprite[spritenum].statnum == STAT_ACTOR) && AC_MOVE_ID(pActor->t_data) && (AC_MOVFLAGS(pSprite, pActor) & noclip)))
    {
        sprite[spritenum].z = daz;
    }
    else if (retval == 0)
        return(16384+dasectnum);

    return(retval);
}

int A_SetSprite(int i,unsigned int cliptype) //The set sprite function
{
    return (A_MoveSprite(i,(sprite[i].xvel*(sintable[(sprite[i].ang+512)&2047]))>>14,
                         (sprite[i].xvel*(sintable[sprite[i].ang&2047]))>>14,sprite[i].zvel,
                         cliptype)==0);
}

int block_deletesprite = 0;

void A_DeleteSprite(int s)
{
    if (oldnet_predicting)
    {
        OSD_Printf("^08A_DeleteSprite^12: ^02ATTEMPTING TO DELETE ACTOR FROM PREDICTING STATE! FAIL.\n");
        return;
    }

    if (block_deletesprite)
    {
        OSD_Printf(OSD_ERROR "A_DeleteSprite(): tried to remove sprite %d in EVENT_EGS\n",s);
        return;
    }
    if (VM_HaveEvent(EVENT_KILLIT))
    {
        int p, pl=A_FindPlayer(&sprite[s],&p);

        Gv_SetVar(sysVarIDs.RETURN,0, -1, -1);
        X_OnEvent(EVENT_KILLIT, s, pl, p);
        if (Gv_GetVar(sysVarIDs.RETURN, -1, -1))
            return;
    }

#ifdef POLYMER
    if (practor[s].lightptr != NULL && videoGetRenderMode() == REND_POLYMER)
        A_DeleteLight(s);
#endif

    // AMBIENT_SFX_PLAYING
    if (sprite[s].picnum == MUSICANDSFX && actor[s].t_data[0] == 1)
        S_StopEnvSound(sprite[s].lotag, s);

    S_DetachSoundsFromSprite(s);

    deletesprite(s);
}

void A_AddToDeleteQueue(int spriteNum)
{
    if (g_spriteDeleteQueueSize == 0)
    {
        A_DeleteSprite(spriteNum);
        return;
    }

    auto& deleteSpriteNum = SpriteDeletionQueue[g_spriteDeleteQueuePos];

    if (deleteSpriteNum >= 0 && actor[deleteSpriteNum].flags & SFLAG_QUEUEDFORDELETE)
        A_DeleteSprite(deleteSpriteNum);

    deleteSpriteNum = spriteNum;
    actor[spriteNum].flags |= SFLAG_QUEUEDFORDELETE;
    g_spriteDeleteQueuePos = (g_spriteDeleteQueuePos + 1) % g_spriteDeleteQueueSize;
}

void A_SpawnMultiple(int sp, int pic, int n)
{
    int j;
    spritetype *s = &sprite[sp];

    for (;n>0;n--)
    {
        j = A_InsertSprite(s->sectnum,s->x,s->y,s->z-(krand()%(47<<8)),pic,-32,8,8,krand()&2047,0,0,sp,5);
        A_Spawn(-1, j);
        sprite[j].cstat = krand()&12;
    }
}

void A_DoGuts(int sp, int gtype, int n)
{
    int gutz,floorz;
    int i,a,j,sx = 32,sy = 32;
//    int pal;
    spritetype *s = &sprite[sp];

    if (A_CheckEnemySprite(s) && s->xrepeat < 16)
        sx = sy = 8;

    gutz = s->z-(8<<8);
    floorz = yax_getflorzofslope(s->sectnum,s->xy);

    if (gutz > (floorz-(8<<8)))
        gutz = floorz-(8<<8);

    if (s->picnum == COMMANDER)
        gutz -= (24<<8);

//    if (A_CheckEnemySprite(s) && s->pal == 6)
//        pal = 6;
//    else pal = 0;

    for (j=n;j>0;j--)
    {
        a = krand()&2047;
        i = A_InsertSprite(s->sectnum,s->x+(krand()&255)-128,s->y+(krand()&255)-128,gutz-(krand()&8191),gtype,-32,sx,sy,a,48+(krand()&31),-512-(krand()&2047),sp,5);
        if (PN(i) == JIBS2)
        {
            sprite[i].xrepeat >>= 2;
            sprite[i].yrepeat >>= 2;
        }
//        if (pal == 6)
//            sprite[i].pal = 6;
        sprite[i].pal = s->pal;
    }
}

void A_DoGutsDir(int sp, int gtype, int n)
{
    int gutz,floorz;
    int i,a,j,sx = 32,sy = 32;
    spritetype *s = &sprite[sp];

    if (A_CheckEnemySprite(s) && s->xrepeat < 16)
        sx = sy = 8;

    gutz = s->z-(8<<8);
    floorz = yax_getflorzofslope(s->sectnum,s->xy);

    if (gutz > (floorz-(8<<8)))
        gutz = floorz-(8<<8);

    if (s->picnum == COMMANDER)
        gutz -= (24<<8);

    for (j=n;j>0;j--)
    {
        a = krand()&2047;
        i = A_InsertSprite(s->sectnum,s->x,s->y,gutz,gtype,-32,sx,sy,a,256+(krand()&127),-512-(krand()&2047),sp,5);
        sprite[i].pal = s->pal;
    }
}

static void ms(int i)
{
    //T1,T2 and T3 are used for all the sector moving stuff!!!
    spritetype *s = &sprite[i];

    int originIdx = T2(i);
    int rotateAngle = T3(i);
    vec2_t result;

    s->x += (s->xvel*(sintable[(s->ang+512)&2047]))>>14;
    s->y += (s->xvel*(sintable[s->ang&2047]))>>14;

    int x = sector[s->sectnum].wallptr, endwall = x+sector[s->sectnum].wallnum;

    for (;x<endwall;x++)
    {
        rotatevec(g_origins[originIdx],rotateAngle&2047,&result);
        dragpoint(x,s->x+result.x,s->y+result.y, 0);

        originIdx++;
    }
}

// sleeping monsters, etc
static void G_MoveZombieActors(void)
{
    int spriteNum = headspritestat[STAT_ZOMBIEACTOR];

    while (spriteNum >= 0)
    {
        int const nextSprite = nextspritestat[spriteNum];

        int32_t    playerDist;
        auto const pSprite = &sprite[spriteNum];
        int const playerNum = A_FindPlayer(pSprite,&playerDist);
        auto const pPlayer = g_player[playerNum].ps;

        if (sprite[pPlayer->i].extra > 0)
        {
            if (playerDist < 30000)
            {
                actor[spriteNum].timetosleep++;
                if (actor[spriteNum].timetosleep >= (playerDist >> 8))
                {
                    int32_t canSeePlayer;
                    if (A_CheckEnemySprite(pSprite))
                    {
                        vec3_t const p = { pPlayer->pos.x + 64 - (krand() & 127),
                                           pPlayer->pos.y + 64 - (krand() & 127),
                                           pPlayer->pos.z - (krand() % ZOFFSET5) };

                        int16_t pSectnum = pPlayer->cursectnum;

                        updatesector(p.x,p.y,&pSectnum);

                        if (pSectnum == -1)
                        {
                            spriteNum = nextSprite;
                            continue;
                        }

                        vec3_t const s = { pSprite->x + 64 - (krand() & 127),
                                           pSprite->y + 64 - (krand() & 127),
                                           pSprite->z - (krand() % (52 << 8)) };

                        int16_t sectNum = pSprite->sectnum;

                        updatesector(s.x, s.y, &sectNum);

                        if (sectNum == -1)
                        {
                            spriteNum = nextSprite;
                            continue;
                        }

                        canSeePlayer = cansee(s.x, s.y, s.z, sectNum, p.x, p.y, p.z, pSectnum);
                    }
                    else
                        canSeePlayer = cansee(pSprite->x, pSprite->y, pSprite->z - ((krand() & 31) << 8), pSprite->sectnum, pPlayer->opos.x,
                            pPlayer->opos.y, pPlayer->opos.z - ((krand() & 31) << 8), pPlayer->cursectnum);

                    if (canSeePlayer)
                    {
                        switch (tileGetMapping(pSprite->picnum))
                        {
                        case RUBBERCAN__:
                        case EXPLODINGBARREL__:
                        case WOODENHORSE__:
                        case HORSEONSIDE__:
                        case CANWITHSOMETHING__:
                        case CANWITHSOMETHING2__:
                        case CANWITHSOMETHING3__:
                        case CANWITHSOMETHING4__:
                        case FIREBARREL__:
                        case FIREVASE__:
                        case NUKEBARREL__:
                        case NUKEBARRELDENTED__:
                        case NUKEBARRELLEAKED__:
                        case TRIPBOMB__:
                            pSprite->shade = ((sector[pSprite->sectnum].ceilingstat & CEILING_STAT_PLAX) && !A_CheckSpriteFlags(spriteNum, SFLAG_NOSHADE))
                                             ? sector[pSprite->sectnum].ceilingshade
                                             : sector[pSprite->sectnum].floorshade;
                            actor[spriteNum].timetosleep = 0;
                            changespritestat(spriteNum, STAT_STANDABLE);
                            break;
                        default:
                            actor[spriteNum].timetosleep = 0;
                            A_PlayAlertSound(spriteNum);
                            changespritestat(spriteNum, STAT_ACTOR);
                            break;
                        }
                    }
                    else actor[spriteNum].timetosleep = 0;
                }
            }
            if (A_CheckEnemySprite(pSprite) && A_CheckSpriteFlags(spriteNum,SFLAG_NOSHADE) == 0)
            {
                pSprite->shade = (sector[pSprite->sectnum].ceilingstat & CEILING_STAT_PLAX)
                                ? sector[pSprite->sectnum].ceilingshade
                                : sector[pSprite->sectnum].floorshade;
            }
        }
        spriteNum = nextSprite;
    }
}

static FORCE_INLINE int G_FindExplosionInSector(int const sectNum)
{
    for (bssize_t SPRITES_OF(STAT_MISC, i))
        if (PN(i) == EXPLOSION2 && sectNum == SECT(i))
            return i;

    return -1;
}

static FORCE_INLINE void P_Nudge(int playerNum, int spriteNum, int shiftLeft)
{
    g_player[playerNum].ps->vel.x += actor[spriteNum].htextra * (sintable[(actor[spriteNum].htang + 512) & 2047]) << shiftLeft;
    g_player[playerNum].ps->vel.y += actor[spriteNum].htextra * (sintable[actor[spriteNum].htang & 2047]) << shiftLeft;
}

int A_IncurDamage(int spriteNum)
{
    auto const pSprite = &sprite[spriteNum];
    auto const pActor = &actor[spriteNum];

    if (pActor->htextra >= 0)
    {
        if (sprite[spriteNum].extra >= 0)
        {
            if (pSprite->picnum == APLAYER)
            {
                if (ud.god && actor[spriteNum].htpicnum != SHRINKSPARK) return -1;

                int32_t playerNum = pSprite->yvel;

                if (pActor->htowner >= 0 && (sprite[pActor->htowner].picnum == APLAYER))
                {
                    if ((spriteNum != pActor->htowner) && G_BlockFriendlyFire(playerNum, sprite[pActor->htowner].yvel)) // Check if hitting ally and not damaging self.
                    {
                        // Nullify damage and cancel.
                        pActor->htowner = -1;
                        pActor->htextra = -1;
                        pActor->htpicnum = APLAYER;
                        return -1;
                    }
                }

                pSprite->extra -= pActor->htextra;

                if (pActor->htowner >= 0)
                {
                    if (pSprite->extra <= 0 && actor[spriteNum].htpicnum != FREEZEBLAST)
                    {
                        pSprite->extra = 0;

                        g_player[playerNum].ps->wackedbyactor = pActor->htowner;

                        if (sprite[pActor->htowner].picnum == APLAYER && playerNum != sprite[pActor->htowner].yvel)
                            g_player[playerNum].ps->frag_ps = sprite[pActor->htowner].yvel;

                        pActor->htowner = g_player[playerNum].ps->i;
                    }
                }

                if (!A_CheckSpriteTileFlags(pActor->htpicnum, SFLAG_NODAMAGEPUSH)) // Check if projectile can push.
                {
                    switch (tileGetMapping(pActor->htpicnum))
                    {
                        case RADIUSEXPLOSION__:
                        case RPG__:
                        case HYDRENT__:
                        case HEAVYHBOMB__:
                        case SEENINE__:
                        case OOZFILTER__:
                        case EXPLODINGBARREL__:
                            P_Nudge(playerNum, spriteNum, 2);
                            break;
                        default:
                        {
                            int const isRPG = (A_CheckSpriteTileFlags(pActor->htpicnum, SFLAG_PROJECTILE) && (actor[spriteNum].projectile.workslike & PROJECTILE_RPG));
                            P_Nudge(playerNum, spriteNum, 1 << isRPG);
                            break;
                        }
                    }
                }
            }
            else
            {
                if (pActor->htextra == 0)
                    if (pActor->htpicnum == SHRINKSPARK && pSprite->xrepeat < 24)
                        return -1;

                pSprite->extra -= pActor->htextra;
                if (pSprite->picnum != RECON && pSprite->owner >= 0 && sprite[pSprite->owner].statnum < MAXSTATUS)
                    pSprite->owner = pActor->htowner;
            }

            pActor->htextra = -1;
            return pActor->htpicnum;
        }
    }

    pActor->htextra = -1;
    return -1;
}

void A_MoveCyclers(void)
{
    int q, j, x, t, s, cshade;
    short *c;
    walltype *wal;

    for (q=g_numCyclers-1;q>=0;q--)
    {

        c = &cyclers[q][0];
        s = c[0];

        t = c[3];
        j = t+(sintable[c[1]&2047]>>10);
        cshade = c[2];

        if (j < cshade) j = cshade;
        else if (j > t)  j = t;

        c[1] += sector[s].extra;
        if (c[5])
        {
            wal = &wall[sector[s].wallptr];
            for (x = sector[s].wallnum;x>0;x--,wal++)
                if (wal->hitag != 1)
                {
                    wal->shade = j;

                    if ((wal->cstat&2) && wal->nextwall >= 0)
                        wall[wal->nextwall].shade = j;

                }
            sector[s].floorshade = sector[s].ceilingshade = j;
        }
    }
}

void A_MoveDummyPlayers(void)
{
    int i = headspritestat[STAT_DUMMYPLAYER], p, nexti;

    while (i >= 0)
    {
        nexti = nextspritestat[i];

        p = sprite[OW(i)].yvel;

        if (g_player[p].ps->on_crane >= 0 || (g_player[p].ps->cursectnum >= 0 && sector[g_player[p].ps->cursectnum].lotag != 1) || sprite[g_player[p].ps->i].extra <= 0)
        {
            g_player[p].ps->dummyplayersprite = -1;
            KILLIT(i);
        }
        else
        {
            if (g_player[p].ps->on_ground && g_player[p].ps->on_warping_sector == 1 && sector[g_player[p].ps->cursectnum].lotag == ST_1_ABOVE_WATER)
            {
                CS(i) = 257;
                SZ(i) = sector[SECT(i)].ceilingz+(27<<8);
                SA(i) = fix16_to_int(g_player[p].ps->q16ang);
                if (T1(i) == 8)
                    T1(i) = 0;
                else T1(i)++;
            }
            else
            {
                if (sector[SECT(i)].lotag != ST_2_UNDERWATER) SZ(i) = sector[SECT(i)].floorz;
                CS(i) = (uint16_t)32768;
            }
        }

        SX(i) += (g_player[p].ps->pos.x-g_player[p].ps->opos.x);
        SY(i) += (g_player[p].ps->pos.y-g_player[p].ps->opos.y);
        setspritez_old(i, sprite[i].x, sprite[i].y, sprite[i].z);

BOLT:

        i = nexti;
    }
}

static int P_Submerge(int const playerNum, DukePlayer_t* const pPlayer, int const sectNum, int const otherSect)
{
    if (pPlayer->on_ground && pPlayer->pos.z >= sector[sectNum].floorz
        && (TEST_SYNC_KEY(g_player[playerNum].inputBits->bits, SK_CROUCH) || pPlayer->vel.z > 2048))
        //        if( onfloorz && sectlotag == 1 && ps->pos.z > (sector[sect].floorz-(6<<8)) )
    {
        if (screenpeek == playerNum)
        {
            FX_StopAllSounds();
        }

#ifndef EDUKE32_STANDALONE
        if (!FURY && sprite[pPlayer->i].extra > 0)
            A_PlaySound(DUKE_UNDERWATER, pPlayer->i);
#endif

        pPlayer->opos.z = pPlayer->pos.z = sector[otherSect].ceilingz;

        if (TEST_SYNC_KEY(g_player[playerNum].inputBits->bits, SK_CROUCH))
            pPlayer->vel.z += 512;

        return 1;
    }

    return 0;
}

static int P_Emerge(int const playerNum, DukePlayer_t* const pPlayer, int const sectNum, int const otherSect)
{
    // r1449-:
    if (pPlayer->pos.z < (sector[sectNum].ceilingz + 1080) && pPlayer->vel.z <= 0)
        // r1450+, breaks submergible slime in bobsp2:
//        if (onfloorz && sectlotag == 2 && ps->pos.z <= sector[sect].ceilingz /*&& ps->vel.z == 0*/)
    {
        //        if( sprite[j].extra <= 0) break;
        if (screenpeek == playerNum)
        {
            FX_StopAllSounds();
        }

#ifndef EDUKE32_STANDALONE
        if (!FURY)
            A_PlaySound(DUKE_GASP, pPlayer->i);
#endif

        pPlayer->opos.z = pPlayer->pos.z = sector[otherSect].floorz;
        pPlayer->vel.z = 0;
        //        ps->vel.z += 1024;

        pPlayer->jumping_toggle = 1;
        pPlayer->jumping_counter = 0;

        return 1;
    }

    return 0;
}

static void P_FinishWaterChange(int const spriteNum, DukePlayer_t* const pPlayer, int const sectLotag, int const spriteOwner, int const newSector)
{
    pPlayer->bobposx = pPlayer->opos.x = pPlayer->pos.x;
    pPlayer->bobposy = pPlayer->opos.y = pPlayer->pos.y;

    if (spriteOwner < 0 || sprite[spriteOwner].owner != spriteOwner)
        pPlayer->transporter_hold = -2;

    pPlayer->cursectnum = newSector;
    changespritesect(spriteNum, newSector);

    vec3_t vect = pPlayer->pos;
    vect.z += PHEIGHT;//pPlayer->spritezoffset;
    setsprite(pPlayer->i, &vect);

    P_UpdateScreenPal(pPlayer);

    if (oldnet_predicting) // No effect spawns if we're predicting, please.
        return;

    if ((krand() & 255) < 32)
        A_Spawn(spriteNum, WATERSPLASH2);

    if (sectLotag == ST_1_ABOVE_WATER)
    {
        for (bssize_t l = 0; l < 9; l++)
            sprite[A_Spawn(pPlayer->i, WATERBUBBLE)].z += krand() & 16383;
    }
}

// Check whether sprite <s> is on/in a non-SE7 water sector.
// <othersectptr>: if not NULL, the sector on the other side.
int A_CheckNoSE7Water(uspriteptr_t const pSprite, int sectNum, int sectLotag, int32_t* pOther)
{
    if (sectLotag == ST_1_ABOVE_WATER || sectLotag == ST_2_UNDERWATER)
    {
        int const otherSect =
            yax_getneighborsect(pSprite->x, pSprite->y, sectNum, sectLotag == ST_1_ABOVE_WATER ? YAX_FLOOR : YAX_CEILING);
        int const otherLotag = (sectLotag == ST_1_ABOVE_WATER) ? ST_2_UNDERWATER : ST_1_ABOVE_WATER;

        // If submerging, the lower sector MUST have lotag 2.
        // If emerging, the upper sector MUST have lotag 1.
        // This way, the x/y coordinates where above/below water
        // changes can happen are the same.
        if (otherSect >= 0 && sector[otherSect].lotag == otherLotag)
        {
            if (pOther)
                *pOther = otherSect;
            return 1;
        }
    }

    return 0;
}

static void G_ShadePlayerSprite(int spriteNum)
{
    spritetype* s = &sprite[spriteNum];

    if (sector[s->sectnum].ceilingstat & 1)
        s->shade += (sector[s->sectnum].ceilingshade - s->shade) >> 1;
    else
        s->shade += (sector[s->sectnum].floorshade - s->shade) >> 1;
}

static void G_MoveHoloDuke(int spriteNum)
{
    spritetype* s = &sprite[spriteNum];
    DukePlayer_t* p = g_player[s->yvel].ps;
    // Holodukes
    if (p->holoduke_on == -1)
    {
        A_DeleteSprite(spriteNum);
        return;
    }

    actor[spriteNum].bpos = s->xyz;

    s->cstat = 0;

    if (s->xrepeat < PLAYER_SPRITE_XREPEAT)
    {
        s->xrepeat += 4;
        s->cstat |= 2;
    }
    else s->xrepeat = PLAYER_SPRITE_XREPEAT;

    if (s->yrepeat < PLAYER_SPRITE_YREPEAT)
        s->yrepeat += 4;
    else
    {
        s->yrepeat = PLAYER_SPRITE_YREPEAT;
        if (sector[s->sectnum].lotag != ST_2_UNDERWATER)
            A_Fall(spriteNum);
        if (s->zvel == 0 && sector[s->sectnum].lotag == ST_1_ABOVE_WATER)
            s->z += (32 << 8);
    }

    if (s->extra < 8)
    {
        s->xvel = 128;
        s->ang = fix16_to_int(p->q16ang);
        s->extra++;
        A_SetSprite(spriteNum, CLIPMASK0);
    }
    else
    {
        s->ang = 2047 - fix16_to_int(p->q16ang);
        setspritez_old(spriteNum, s->x, s->y, s->z);
    }

    G_ShadePlayerSprite(spriteNum);
}

int otherp;
void G_MovePlayerSprite(int playerNum)
{
    DukePlayer_t* p = g_player[playerNum].ps;
    spritetype *s = &sprite[p->i];

    int32_t otherx;

    if (p->newowner >= 0)  //Looking thru the camera
    {
        s->xy = p->opos.xy;

        actor[p->i].bpos.z = s->z = p->opos.z + PHEIGHT;
        s->ang = fix16_to_int(p->oq16ang);
        setspritez(p->i, &s->xyz);
    }
    else
    {
#ifdef YAX_ENABLE
        // TROR water submerge/emerge
        int const playerSectnum = s->sectnum;
        int const sectorLotag = sector[playerSectnum].lotag;
        int32_t   otherSector;

        if (A_CheckNoSE7Water((uspriteptr_t)s, playerSectnum, sectorLotag, &otherSector))
        {
            // NOTE: Compare with G_MoveTransports().
            p->on_warping_sector = 1;

            if ((sectorLotag == ST_1_ABOVE_WATER ?
                P_Submerge(playerNum, p, playerSectnum, otherSector) :
                P_Emerge(playerNum, p, playerSectnum, otherSector)) == 1)
                P_FinishWaterChange(p->i, p, sectorLotag, -1, otherSector);
        }
#endif

        if (ud.multimode > 1)
            otherp = P_FindOtherPlayer(s->yvel, &otherx);
        else
        {
            otherp = s->yvel;
            otherx = 0;
        }

        if (!oldnet_predicting && G_TileHasActor(sprite[p->i].picnum))
            A_Execute(p->i, s->yvel, otherx);

        if (ud.multimode > 1)
            if (sprite[g_player[otherp].ps->i].extra > 0)
            {
                if (s->yrepeat > 32 && sprite[g_player[otherp].ps->i].yrepeat < 32)
                {
                    if (otherx < 1400 && p->knee_incs == 0)
                    {
                        // Don't stomp teammates.
                        if (
                            ((Gametypes[ud.coop].flags & GAMETYPE_TDM) && p->team != g_player[otherp].ps->team) ||
                            (!(Gametypes[ud.coop].flags & GAMETYPE_PLAYERSFRIENDLY) && !(Gametypes[ud.coop].flags & GAMETYPE_TDM))
                            )
                        {
                            p->knee_incs = 1;
                            p->weapon_pos = -1;
                            p->actorsqu = g_player[otherp].ps->i;
                        }
                    }
                }
            }

        if (ud.god)
        {
            s->extra = p->max_player_health;
            s->cstat = 257;
            p->dead_flag = 0; // Fix for player sprite clipping into floor if resurrecting
            p->inv_amount[GET_JETPACK] = 1599;
        }

        if (s->extra > 0)
        {
            actor[p->i].htowner = p->i;

            if (ud.god == 0)
            {
                if (G_CheckForSpaceCeiling(s->sectnum) || G_CheckForSpaceFloor(s->sectnum))
                {
                    P_QuickKill(p);
                    OSD_Printf(OSD_ERROR "%s: player killed by space sector!\n", EDUKE32_FUNCTION);
                }
            }
        }
        else
        {
            // Set corpse position
            p->pos.x = s->x;
            p->pos.y = s->y;
            p->pos.z = s->z - (20 << 8);

            p->newowner = -1;

            if (p->wackedbyactor >= 0 && sprite[p->wackedbyactor].statnum < MAXSTATUS)
            {
                p->q16ang += fix16_from_int(G_GetAngleDelta(fix16_to_int(p->q16ang), getangle(sprite[p->wackedbyactor].x - p->pos.x, sprite[p->wackedbyactor].y - p->pos.y)) >> 1);
                p->q16ang &= 0x7FFFFFF;
            }
        }

        s->ang = fix16_to_int(p->q16ang);
    }

    G_ShadePlayerSprite(p->i);
}

static void G_MovePlayers(void) //Players
{
    int i = headspritestat[STAT_PLAYER], nexti;
    spritetype *s;

    while (i >= 0)
    {
        nexti = nextspritestat[i];

        s = &sprite[i];

        if (s->owner >= 0) // Players
        {
            G_MovePlayerSprite(s->yvel);
        }
        else // HoloDukes
        {
            G_MoveHoloDuke(i);
        }

        i = nexti;
    }
}

static void G_MoveFX(void)
{
    int spriteNum = headspritestat[STAT_FX];

    while (spriteNum >= 0)
    {
        auto const pSprite = &sprite[spriteNum];
        int const  nextSprite = nextspritestat[spriteNum];

        switch (tileGetMapping(pSprite->picnum))
        {
        case RESPAWN__:
            if (pSprite->extra == 66)
            {
                /*int32_t j =*/ A_Spawn(spriteNum, SHT(spriteNum));
                //                    sprite[j].pal = sprite[spriteNum].pal;
                KILLIT(spriteNum);
            }
            else if (pSprite->extra > (66 - 13))
                sprite[spriteNum].extra++;
            break;

        case MUSICANDSFX__:
        {
            int32_t const spriteHitag = (uint16_t)pSprite->hitag;
            auto const    pPlayer = g_player[screenpeek].ps;

            if (T2(spriteNum) != ud.config.SoundToggle)
            {
                // If sound playback was toggled, restart.
                T2(spriteNum) = ud.config.SoundToggle;
                T1(spriteNum) = 0;
            }

            if (pSprite->lotag >= 1000 && pSprite->lotag < 2000)
            {
                int32_t playerDist = ldist(&sprite[pPlayer->i], pSprite);

                if (playerDist < spriteHitag && T1(spriteNum) == 0)
                {
                    FX_SetReverb(pSprite->lotag - 1000);
                    T1(spriteNum) = 1;
                }
                else if (playerDist >= spriteHitag && T1(spriteNum) == 1)
                {
                    FX_SetReverb(0);
                    FX_SetReverbDelay(0);
                    T1(spriteNum) = 0;
                }
            }
            else if (pSprite->lotag < 999 && pSprite->lotag && S_SoundIsValid(pSprite->lotag) && (unsigned)sector[pSprite->sectnum].lotag < ST_9_SLIDING_ST_DOOR && ud.config.AmbienceToggle && sector[SECT(spriteNum)].floorz != sector[SECT(spriteNum)].ceilingz)
            {
                if (g_sounds[pSprite->lotag]->flags & SF_MSFX)
                {
                    const int soundPlaying = A_CheckSoundPlaying(spriteNum, pSprite->lotag);
                    const int playerDist = dist(&sprite[pPlayer->i], pSprite);

                    if (playerDist < spriteHitag && !soundPlaying && FX_VoiceAvailable(g_sounds[pSprite->lotag]->priority - 1))
                    {
                        // Start playing an ambience sound.
                        auto oldFlags = g_sounds[pSprite->lotag]->flags;
                        if (g_numEnvSoundsPlaying == ud.config.NumVoices)
                        {
                            int32_t j;

                            for (SPRITES_OF(STAT_FX, j))
                            {
                                if (j != spriteNum && S_IsAmbientSFX(j) && A_CheckSoundPlaying(j, sprite[j].lotag) && dist(&sprite[j], &sprite[pPlayer->i]) > playerDist)
                                {
                                    S_StopEnvSound(sprite[j].lotag, j);
                                    break;
                                }
                            }

                            if (j == -1)
                                goto BOLT;
                        }

                        g_sounds[pSprite->lotag]->flags |= SF_LOOP;
                        A_PlaySound(pSprite->lotag, spriteNum);
                        g_sounds[pSprite->lotag]->flags = oldFlags;
                        T1(spriteNum) = 1;  // AMBIENT_SFX_PLAYING
                    }
                    else if (playerDist >= spriteHitag && soundPlaying)
                    {
                        // Stop playing ambience sound because we're out of its range.

                        // T1 will be reset in sounds.c: CLEAR_SOUND_T0
                        // T1 = 0;
                        S_StopEnvSound(pSprite->lotag, spriteNum);
                    }
                }

                if ((g_sounds[pSprite->lotag]->flags & (SF_GLOBAL | SF_DTAG)) == SF_GLOBAL)
                {
                    // Randomly playing global sounds (flyby of planes, screams, ...)

                    if (T5(spriteNum) > 0)
                        T5(spriteNum)--;
                    else
                    {
                        for (int32_t ALL_PLAYERS(playerNum))
                        {
                            if (playerNum == myconnectindex && g_player[playerNum].ps->cursectnum == pSprite->sectnum)
                            {
                                S_PlaySound(pSprite->lotag + (unsigned)g_globalRandom % (pSprite->hitag + 1));
                                T5(spriteNum) = GAMETICSPERSEC * 40 + g_globalRandom % (GAMETICSPERSEC * 40);
                            }
                        }
                    }
                }
            }
            break;
        }
        }
    BOLT:
        spriteNum = nextSprite;
    }
}

static void G_MoveFallers(void)
{
    int i = headspritestat[STAT_FALLER], nexti, sect, j;
    spritetype *s;
    int x;

    while (i >= 0)
    {
        nexti = nextspritestat[i];
        s = &sprite[i];

        sect = s->sectnum;

        if (T1(i) == 0)
        {
            s->z -= (16<<8);
            T2(i) = s->ang;
            x = s->extra;
            IFHIT(i)
            {
                if (j == FIREEXT || j == RPG || j == RADIUSEXPLOSION || j == SEENINE || j == OOZFILTER)
                {
                    if (s->extra <= 0)
                    {
                        T1(i) = 1;
                        j = headspritestat[STAT_FALLER];
                        while (j >= 0)
                        {
                            if (sprite[j].hitag == SHT(i))
                            {
                                actor[j].t_data[0] = 1;
                                sprite[j].cstat &= (65535-64);
                                if (sprite[j].picnum == CEILINGSTEAM || sprite[j].picnum == STEAM)
                                    sprite[j].cstat |= 32768;
                            }
                            j = nextspritestat[j];
                        }
                    }
                }
                else
                {
                    actor[i].htextra = 0;
                    s->extra = x;
                }
            }
            s->ang = T2(i);
            s->z += (16<<8);
        }
        else if (T1(i) == 1)
        {
            if ((int16_t)s->lotag > 0)
            {
                s->lotag-=3;
                if ((int16_t)s->lotag <= 0)
                {
                    s->xvel = (32+(krand()&63));
                    s->zvel = -(1024+(krand()&1023));
                }
            }
            else
            {
                if (s->xvel > 0)
                {
                    s->xvel -= 8;
                    A_SetSprite(i,CLIPMASK0);
                }

                if (G_CheckForSpaceFloor(s->sectnum))
                    x = 0;
                else
                {
                    if (G_CheckForSpaceCeiling(s->sectnum))
                        x = g_spriteGravity/6;
                    else
                        x = g_spriteGravity;
                }

                if (s->z < (sector[sect].floorz - ACTOR_FLOOR_OFFSET))
                {
                    s->zvel += x;
                    if (s->zvel > ACTOR_MAXFALLINGZVEL)
                        s->zvel = ACTOR_MAXFALLINGZVEL;
                    s->z += s->zvel;
                }
                if ((sector[sect].floorz-s->z) < (16<<8))
                {
                    j = 1+(krand()&7);
                    for (x=0;x<j;x++) RANDOMSCRAP(s, i);
                    KILLIT(i);
                }
            }
        }

BOLT:
        i = nexti;
    }
}

static void G_MoveStandables(void)
{
    int i = headspritestat[STAT_STANDABLE], j, nexti, nextj, p=0, sect, switchpicnum;
    int l=0, x;
    int32_t *pData;
    spritetype *s;
    short m;

    while (i >= 0)
    {
        nexti = nextspritestat[i];

        pData = &actor[i].t_data[0];
        s = &sprite[i];
        sect = s->sectnum;

        if ((unsigned)sect >= MAXSECTORS)
            KILLIT(i);

        IFWITHIN(CRANE,CRANE+3)
        {
            //pData[0] = state
            //pData[1] = checking sector number
            //pData[2] = delay 
            //pData[3] = grabbed player ID
            //pData[4] = tempwallptr crap

            enum
            {
                CRANE_WAITING = 0,
                CRANE_MOVETOSECT = 1,
                CRANE_LOWERTOGRAB = 2,
                CRANE_GRABOBJECT = 3,
                CRANE_GRABWAIT = 4,
                CRANE_RAISEOBJECT = 5,
                CRANE_MOVETOORIGIN = 6,
                CRANE_LOWERTODROP = 7,
                CRANE_RAISEAFTERDROP = 8,
                CRANE_RESET = 9,
            };

            if (s->xvel)
                A_GetZLimits(i);

            // Prevent OOB access, shouldn't happen, but want to be sure.
            if (EDUKE32_PREDICT_FALSE(pData[3] >= MAXPLAYERS))
                pData[3] = -1;

            // Safety check in case the player died or somehow got dropped off the crane in a way that wasn't covered in this code.
            if (EDUKE32_PREDICT_FALSE(pData[3] != -1 && (g_player[pData[3]].ps->on_crane == -1 || !g_player[pData[3]].connected)))
            {
                if (s->owner == -2)
                    s->owner = -1;

                pData[3] = -1;
            }

            if (pData[0] == CRANE_WAITING)   //Waiting to check the sector
            {
                for (SPRITES_OF_SECT_SAFE(pData[1], j, nextj))
                {
                    switch (sprite[j].statnum)
                    {
                        case STAT_ACTOR:
                        case STAT_ZOMBIEACTOR:
                        case STAT_STANDABLE:
                        case STAT_PLAYER:
                        {
                            vec3_t vect = { g_origins[pData[4] + 1].x, g_origins[pData[4] + 1].y, sprite[j].z };

                            s->ang = getangle(vect.x - s->x, vect.y - s->y);
                            setsprite(j, &vect);
                            pData[0] = CRANE_MOVETOSECT;
                            goto BOLT;
                        }
                    }
                }
            }
            else if (pData[0] == CRANE_MOVETOSECT)
            {
                if (s->xvel < 184)
                {
                    s->picnum = CRANE + 1;
                    s->xvel += 8;
                }
                A_SetSprite(i, CLIPMASK0);
                if (sect == pData[1])
                    pData[0] = CRANE_LOWERTOGRAB;
            }
            else if (pData[0] == CRANE_LOWERTOGRAB || pData[0] == CRANE_LOWERTODROP)
            {
                s->z += (1024+512);

                if (pData[0] == CRANE_LOWERTOGRAB)
                {
                    if ((sector[sect].floorz - s->z) < (64<<8))
                        if (s->picnum > CRANE) s->picnum--;

                    if ((sector[sect].floorz - s->z) < (4096+1024))
                        pData[0] = CRANE_GRABOBJECT;
                }
                if (pData[0] == CRANE_LOWERTODROP)
                {
                    if ((sector[sect].floorz - s->z) < (64<<8))
                    {
                        if (s->picnum > CRANE)
                            s->picnum--;
                        else
                        {
                            if (s->owner == -2 && pData[3] != -1)
                            {
                                auto const pPlayer = g_player[pData[3]].ps;
                                A_PlaySound(DUKE_GRUNT, pPlayer->i);
                                if (pPlayer->on_crane == i)
                                {
                                    pPlayer->on_crane = -1;
                                    pData[3] = -1;
                                }
                            }

                            pData[0] = CRANE_RAISEAFTERDROP;
                            s->owner = -1;
                        }
                    }
                }
            }
            else if (pData[0] == CRANE_GRABOBJECT)
            {
                s->picnum++;
                if (s->picnum == (CRANE+2))
                {
                    p = CheckPlayerInSector(pData[1]);
                    if (p >= 0 && g_player[p].ps->on_ground && pData[3] == -1)
                    {
                        auto const pPlayer = g_player[p].ps;
                        int16_t const ang = s->ang + 1024;
                        
                        s->owner = -2; // -2 = Got player
                        pData[3] = p;

                        pPlayer->on_crane = i;
                        pPlayer->q16ang = fix16_from_int(ang);
                        sprite[pPlayer->i].ang = ang;

                        A_PlaySound(DUKE_GRUNT, pPlayer->i);
                    }
                    else
                    {
                        for (SPRITES_OF_SECT(pData[1], j))
                        {
                            switch (sprite[j].statnum)
                            {
                                case STAT_ACTOR:
                                case STAT_STANDABLE:
                                    s->owner = j;
                                    break;
                            }
                        }
                    }

                    pData[0] = CRANE_GRABWAIT; //Grabbed the sprite
                    pData[2] = 0;
                    goto BOLT;
                }
            }
            else if (pData[0] == CRANE_GRABWAIT) //Delay before going up
            {
                pData[2]++;
                if (pData[2] > 10)
                    pData[0] = CRANE_RAISEOBJECT;
            }
            else if (pData[0] == CRANE_RAISEOBJECT || pData[0] == CRANE_RAISEAFTERDROP)
            {
                if (pData[0] == CRANE_RAISEAFTERDROP && s->picnum < (CRANE + 2))
                    if ((sector[sect].floorz-s->z) > 8192)
                        s->picnum++;

                if (s->z < g_origins[pData[4]+2].x)
                {
                    pData[0] = (pData[0] == CRANE_RAISEOBJECT) ? CRANE_MOVETOORIGIN : CRANE_RESET;
                    s->xvel = 0;
                }
                else
                    s->z -= (1024+512);
            }
            else if (pData[0] == CRANE_MOVETOORIGIN)
            {
                if (s->xvel < 192)
                    s->xvel += 8;

                s->ang = getangle(g_origins[pData[4]].x - s->x, g_origins[pData[4]].y - s->y);
                A_SetSprite(i,CLIPMASK0);

                if (((s->x- g_origins[pData[4]].x)*(s->x - g_origins[pData[4]].x)+(s->y - g_origins[pData[4]].y)*(s->y - g_origins[pData[4]].y)) < (128*128))
                    pData[0] = 7;
            }
            else if (pData[0] == CRANE_RESET)
                pData[0] = CRANE_WAITING;

            setspritez_old(g_origins[pData[4]+2].y,s->x,s->y,s->z-(34<<8));

            if (s->owner != -1)
            {
                IFHIT(i)
                {
                    if (s->owner == -2 && pData[3] != -1) // Holding a player
                    {
                        auto const pPlayer = g_player[pData[3]].ps;
                        pPlayer->on_crane = -1;
                        pData[3] = -1;
                    }

                    s->owner = -1;
                    s->picnum = CRANE;
                    goto BOLT;
                }

                if (s->owner >= 0)
                {
                    setspritez(s->owner,&s->xyz);
                    s->zvel = 0;
                }
                else if (s->owner == -2 && pData[3] != -1) // Holding a player
                {
                    auto const pPlayer = g_player[pData[3]].ps;
                    pPlayer->pos.x = s->x - (sintable[(fix16_to_int(pPlayer->q16ang) + 512) & 2047] >> 6);
                    pPlayer->pos.y = s->y - (sintable[fix16_to_int(pPlayer->q16ang) & 2047] >> 6);
                    pPlayer->pos.z = s->z + (2 << 8);

                    setspritez(pPlayer->i, &pPlayer->pos);
                    pPlayer->cursectnum = sprite[pPlayer->i].sectnum;
                }
            }

            goto BOLT;
        }

        IFWITHIN(WATERFOUNTAIN,WATERFOUNTAIN+3)
        {
            if (pData[0] > 0)
            {
                if (pData[0] < 20)
                {
                    pData[0]++;

                    s->picnum++;

                    if (s->picnum == (WATERFOUNTAIN+3))
                        s->picnum = WATERFOUNTAIN+1;
                }
                else
                {
                    p = A_FindPlayer(s,&x);

                    if (x > 512)
                    {
                        pData[0] = 0;
                        s->picnum = WATERFOUNTAIN;
                    }
                    else pData[0] = 1;
                }
            }
            goto BOLT;
        }

        if (AFLAMABLE(s->picnum))
        {
            if (T1(i) == 1)
            {
                T2(i)++;
                if ((T2(i)&3) > 0) goto BOLT;

                if (s->picnum == TIRE && T2(i) == 32)
                {
                    s->cstat = 0;
                    j = A_Spawn(i,BLOODPOOL);
                    sprite[j].shade = 127;
                }
                else
                {
                    if (s->shade < 64) s->shade++;
                    else KILLIT(i);
                }

                j = s->xrepeat-(krand()&7);
                if (j < 10)
                {
                    KILLIT(i);
                }

                s->xrepeat = j;

                j = s->yrepeat-(krand()&7);
                if (j < 4)
                {
                    KILLIT(i);
                }
                s->yrepeat = j;
            }
            if (s->picnum == BOX)
            {
                A_Fall(i);
                actor[i].ceilingz = sector[s->sectnum].ceilingz;
            }
            goto BOLT;
        }

        if (s->picnum == TRIPBOMB)
        {
            //            int lTripBombControl=Gv_GetVarByLabel("TRIPBOMB_CONTROL", TRIPBOMB_TRIPWIRE, -1, -1);
            //            if(lTripBombControl & TRIPBOMB_TIMER)
            if (actor[i].t_data[6] == 1)
            {

                if (actor[i].t_data[7] >= 1)
                {
                    actor[i].t_data[7]--;
                }

                if (actor[i].t_data[7] <= 0)
                {
                    //                    s->extra = *g_tile[s->picnum].execPtr;
                    T3(i)=16;
                    actor[i].t_data[6]=3;
                    A_PlaySound(LASERTRIP_ARMING,i);
                }
                // we're on a timer....
            }
            if (T3(i) > 0 && actor[i].t_data[6] == 3)
            {
                T3(i)--;
                if (T3(i) == 8)
                {
                    A_PlaySound(LASERTRIP_EXPLODE,i);
                    for (j=0;j<5;j++) RANDOMSCRAP(s, i);
                    x = s->extra;
                    A_RadiusDamage(i, g_tripbombBlastRadius, x>>2,x>>1,x-(x>>2),x);

                    j = A_Spawn(i,EXPLOSION2);
                    sprite[j].ang = s->ang;
                    sprite[j].xvel = 348;
                    A_SetSprite(j,CLIPMASK0);

                    j = headspritestat[STAT_MISC];
                    while (j >= 0)
                    {
                        if (sprite[j].picnum == LASERLINE && s->hitag == sprite[j].hitag)
                            sprite[j].xrepeat = sprite[j].yrepeat = 0;
                        j = nextspritestat[j];
                    }
                    KILLIT(i);
                }
                goto BOLT;
            }
            else
            {
                x = s->extra;
                s->extra = 1;
                l = s->ang;
                IFHIT(i) { actor[i].t_data[6] = 3;
                        T3(i) = 16;
                      }
                s->extra = x;
                s->ang = l;
            }

            if (T1(i) < 32)
            {
                int32_t playerDist;
                A_FindPlayer(s,&playerDist);

                if (playerDist > 768)
                    T1(i)++;
                else if (T1(i) > 16)
                    T1(i)++;
            }

            if (T1(i) == 32)
            {
                l = s->ang;
                s->ang = T6(i);

                T4(i) = s->x;
                T5(i) = s->y;
                s->x += sintable[(T6(i)+512)&2047]>>9;
                s->y += sintable[(T6(i))&2047]>>9;
                s->z -= (3<<8);
                setspritez_old(i,s->x,s->y,s->z);

                int16_t hitSprite = -1;
                int32_t hitDist = A_CheckHitSprite(i, &hitSprite);
                actor[i].lastvx = hitDist;

                s->ang = l;

                //                if(lTripBombControl & TRIPBOMB_TRIPWIRE)
                if (actor[i].t_data[6] != 1)
                {
                    // we're on a trip wire
                    int16_t curSectNum;
                    while (hitDist > 0)
                    {
                        auto const lineSpr = A_Spawn(i, LASERLINE);
                        auto const pLine = &sprite[lineSpr];

                        setsprite(lineSpr, &pLine->xyz);

                        pLine->hitag = s->hitag;
                        actor[lineSpr].t_data[1] = pLine->z;

                        s->x += sintable[(T6(i) + 512) & 2047] >> 4;
                        s->y += sintable[(T6(i)) & 2047] >> 4;

                        if (hitDist < 1024)
                        {
                            pLine->xrepeat = hitDist >> 5;
                            break;
                        }
                        hitDist -= 1024;

                        curSectNum = s->sectnum;
                        updatesectorz(s->x, s->y, s->z, &curSectNum);

                        if (curSectNum < 0)
                            break;

                        // this is a hack to work around the LASERLINE sprite's art tile offset
                        changespritesect(lineSpr, curSectNum);
                    }
                }

                T1(i)++;
                s->x = T4(i);
                s->y = T5(i);
                s->z += (3<<8);
                setspritez(i, &s->xyz);
                T4(i) = 0;

                // if( m >= 0 && lTripBombControl & TRIPBOMB_TRIPWIRE)
                if (hitSprite >= 0 && actor[i].t_data[6] != 1)
                {
                    actor[i].t_data[6] = 3;
                    T3(i) = 13;
                    A_PlaySound(LASERTRIP_ARMING,i);
                }
                else T3(i) = 0;
            }

            if (T1(i) == 33)
            {
                T2(i)++;

                T4(i) = s->x;
                T5(i) = s->y;
                s->x += sintable[(T6(i)+512)&2047]>>9;
                s->y += sintable[(T6(i))&2047]>>9;
                s->z -= (3<<8);
                setspritez_old(i,s->x,s->y,s->z);

                x = A_CheckHitSprite(i,&m);

                s->x = T4(i);
                s->y = T5(i);
                s->z += (3<<8);
                setspritez_old(i,s->x,s->y,s->z);

                // if( actor[i].lastvx != x && lTripBombControl & TRIPBOMB_TRIPWIRE)
                if (actor[i].lastvx != x && actor[i].t_data[6] != 1)
                {
                    actor[i].t_data[6] = 3;
                    T3(i) = 13;
                    A_PlaySound(LASERTRIP_ARMING,i);
                }
            }
            goto BOLT;
        }


        if (s->picnum >= CRACK1 && s->picnum <= CRACK4)
        {
            if (s->hitag > 0)
            {
                pData[0] = s->cstat;
                pData[1] = s->ang;
                j = A_IncurDamage(i);
                if (j == FIREEXT || j == RPG || j == RADIUSEXPLOSION || j == SEENINE || j == OOZFILTER)
                {
                    j = headspritestat[STAT_STANDABLE];
                    while (j >= 0)
                    {
                        if (s->hitag == sprite[j].hitag && (sprite[j].picnum == OOZFILTER || sprite[j].picnum == SEENINE))
                            if (sprite[j].shade != -32)
                                sprite[j].shade = -32;
                        j = nextspritestat[j];
                    }

                    goto DETONATE;
                }
                else
                {
                    s->cstat = pData[0];
                    s->ang = pData[1];
                    s->extra = 0;
                }
            }
            goto BOLT;
        }

        if (s->picnum == FIREEXT)
        {
            j = A_IncurDamage(i);
            if (j == -1) goto BOLT;

            for (int k = 0; k < 16; k++)
            {
                j = A_InsertSprite(SECT(i), SX(i), SY(i), SZ(i) - (krand() % (48 << 8)),
                    SCRAP3 + (krand() & 3), -8, 48, 48, krand() & 2047, (krand() & 63) + 64,
                    -(krand() & 4095) - (sprite[i].zvel >> 2), i, 5);

                sprite[j].pal = 2;
            }

            j = A_Spawn(i, EXPLOSION2);
            A_PlaySound(PIPEBOMB_EXPLODE, j);
            A_PlaySound(GLASS_HEAVYBREAK, j);

            if ((int16_t)s->hitag > 0)
            {
                for (SPRITES_OF(STAT_STANDABLE, j))
                {
                    if (s->hitag == sprite[j].hitag && (sprite[j].picnum == OOZFILTER || sprite[j].picnum == SEENINE))
                        if (sprite[j].shade != -32)
                            sprite[j].shade = -32;
                }

                x = s->extra;
                A_RadiusDamage(i, g_pipebombBlastRadius,x>>2, x-(x>>1),x-(x>>2), x);
                j = A_Spawn(i, EXPLOSION2);
                A_PlaySound(PIPEBOMB_EXPLODE, j);

                goto DETONATE;
            }
            else
            {
                A_RadiusDamage(i,g_seenineBlastRadius,10,15,20,25);
                KILLIT(i);
            }
            goto BOLT;
        }

        if (s->picnum == OOZFILTER || s->picnum == SEENINE || s->picnum == SEENINEDEAD || s->picnum == (SEENINEDEAD+1))
        {
            if (s->shade != -32 && s->shade != -33)
            {
                if (s->xrepeat)
                    j = (A_IncurDamage(i) >= 0);
                else
                    j = 0;

                if (j || s->shade == -31)
                {
                    if (j) s->lotag = 0;

                    pData[3] = 1;

                    j = headspritestat[STAT_STANDABLE];
                    while (j >= 0)
                    {
                        if (s->hitag == sprite[j].hitag && (sprite[j].picnum == SEENINE || sprite[j].picnum == OOZFILTER))
                            sprite[j].shade = -32;
                        j = nextspritestat[j];
                    }
                }
            }
            else
            {
                if (s->shade == -32)
                {
                    if (s->lotag > 0)
                    {
                        s->lotag-=3;
                        if (s->lotag <= 0) s->lotag = -99;
                    }
                    else
                        s->shade = -33;
                }
                else
                {
                    if (s->xrepeat > 0)
                    {
                        T3(i)++;
                        if (T3(i) == 3)
                        {
                            if (s->picnum == OOZFILTER)
                            {
                                T3(i) = 0;
                                goto DETONATE;
                            }
                            if (s->picnum != (SEENINEDEAD+1))
                            {
                                T3(i) = 0;

                                if (s->picnum == SEENINEDEAD) s->picnum++;
                                else if (s->picnum == SEENINE)
                                    s->picnum = SEENINEDEAD;
                            }
                            else goto DETONATE;
                        }
                        goto BOLT;
                    }

DETONATE:

                    g_earthquakeTime = 16;

                    j = headspritestat[STAT_EFFECTOR];
                    while (j >= 0)
                    {
                        if (s->hitag == sprite[j].hitag)
                        {
                            if (sprite[j].lotag == SE_13_EXPLOSIVE)
                            {
                                if (actor[j].t_data[2] == 0)
                                    actor[j].t_data[2] = 1;
                            }
                            else if (sprite[j].lotag == SE_8_UP_OPEN_DOOR_LIGHTS)
                                actor[j].t_data[4] = 1;
                            else if (sprite[j].lotag == SE_18_INCREMENTAL_SECTOR_RISE_FALL)
                            {
                                if (actor[j].t_data[0] == 0)
                                    actor[j].t_data[0] = 1;
                            }
                            else if (sprite[j].lotag == SE_21_DROP_FLOOR)
                                actor[j].t_data[0] = 1;
                        }
                        j = nextspritestat[j];
                    }

                    s->z -= (32<<8);

                    if ((pData[3] == 1 && s->xrepeat) || s->lotag == -99)
                    {
                        x = s->extra;
                        A_Spawn(i,EXPLOSION2);
                        A_RadiusDamage(i,g_seenineBlastRadius,x>>2, x-(x>>1),x-(x>>2), x);
                        A_PlaySound(PIPEBOMB_EXPLODE,i);
                    }

                    if (s->xrepeat)
                        for (x=0;x<8;x++) RANDOMSCRAP(s, i);

                    KILLIT(i);
                }
            }
            goto BOLT;
        }

        if (s->picnum == MASTERSWITCH)
        {
            if (s->yvel == 1)
            {
                s->hitag--;
                if (s->hitag <= 0)
                {
                    G_OperateSectors(sect,i);

                    j = headspritesect[sect];
                    while (j >= 0)
                    {
                        if (sprite[j].statnum == 3)
                        {
                            switch (sprite[j].lotag)
                            {
                            case 2:
                            case 21:
                            case 31:
                            case 32:
                            case 36:
                                actor[j].t_data[0] = 1;
                                break;
                            case 3:
                                actor[j].t_data[4] = 1;
                                break;
                            }
                        }
                        else if (sprite[j].statnum == 6)
                        {
                            switch (tileGetMapping(sprite[j].picnum))
                            {
                            case SEENINE__:
                            case OOZFILTER__:
                                sprite[j].shade = -31;
                                break;
                            }
                        }
                        j = nextspritesect[j];
                    }
                    KILLIT(i);
                }
            }
            goto BOLT;
        }
        switchpicnum = s->picnum;
        if ((s->picnum > SIDEBOLT1) && (s->picnum <= SIDEBOLT1+3))
        {
            switchpicnum = SIDEBOLT1;
        }
        if ((s->picnum > BOLT1) && (s->picnum <= BOLT1+3))
        {
            switchpicnum = BOLT1;
        }

        switch (tileGetMapping(switchpicnum))
        {
        case VIEWSCREEN__:
        case VIEWSCREEN2__:

            if (s->xrepeat == 0)
                KILLIT(i);

            {
                int32_t    playerDist;
                int const  p = A_FindPlayer(s, &playerDist);
                auto const ps = g_player[p].ps;

                // EDuke32 extension: xvel of viewscreen determines active distance
                int const activeDist = s->xvel > 0 ? s->xvel : VIEWSCREEN_ACTIVE_DISTANCE;

                if (dist(&sprite[ps->i], s) < activeDist)
                {
                    // DOS behavior: yvel of 1 activates screen if player approaches
                    if (g_curViewscreen == -1 && s->yvel & 1)
                    {
                        g_curViewscreen = i;

                        // EDuke32 extension: for remote activation, check for connected camera and display its image
                        if (sprite[i].hitag)
                        {
                            for (bssize_t SPRITES_OF(STAT_ACTOR, otherSprite))
                            {
                                if (PN(otherSprite) == CAMERA1 && sprite[i].hitag == SLT(otherSprite))
                                {
                                    sprite[i].owner = otherSprite;
                                    break;
                                }
                            }
                        }
                    } // deactivate viewscreen in valid range if yvel is 0
                    else if (g_curViewscreen == i && !(s->yvel & 3))
                    {
                        g_curViewscreen = -1;
                        T1(i) = 0;
                        for (bssize_t ii = 0; ii < VIEWSCREENFACTOR; ii++)
                            walock[TILE_VIEWSCR - ii] = CACHE1D_UNLOCKED;
                    }
                }
                else if (g_curViewscreen == i /*&& T1 == 1*/)
                {
                    g_curViewscreen = -1;
                    s->yvel &= ~2; // VIEWSCREEN YVEL
                    T1(i) = 0;

                    for (bssize_t ii = 0; ii < VIEWSCREENFACTOR; ii++)
                        walock[TILE_VIEWSCR - ii] = CACHE1D_UNLOCKED;
                }
            }

            goto BOLT;

        case TRASH__:

            if (s->xvel == 0) s->xvel = 1;
            IFMOVING
            {
                A_Fall(i);
                if (krand()&1) s->zvel -= 256;
                if (klabs(s->xvel) < 48)
                    s->xvel += (krand()&3);
            }
            else KILLIT(i);
            break;

        case SIDEBOLT1__:
            //        case SIDEBOLT1+1:
            //        case SIDEBOLT1+2:
            //        case SIDEBOLT1+3:
            p = A_FindPlayer(s, &x);
            if (x > 20480) goto BOLT;

CLEAR_THE_BOLT2:
            if (pData[2])
            {
                pData[2]--;
                goto BOLT;
            }
            if ((s->xrepeat|s->yrepeat) == 0)
            {
                s->xrepeat=pData[0];
                s->yrepeat=pData[1];
            }
            if ((krand()&8) == 0)
            {
                pData[0]=s->xrepeat;
                pData[1]=s->yrepeat;
                pData[2] = g_globalRandom&4;
                s->xrepeat=s->yrepeat=0;
                goto CLEAR_THE_BOLT2;
            }
            s->picnum++;

            if (l&1) s->cstat ^= 2;

            if ((krand()&1) && sector[sect].floorpicnum == HURTRAIL)
                A_PlaySound(SHORT_CIRCUIT,i);

            if (s->picnum == SIDEBOLT1+4) s->picnum = SIDEBOLT1;

            goto BOLT;

        case BOLT1__:
            //        case BOLT1+1:
            //        case BOLT1+2:
            //        case BOLT1+3:
            p = A_FindPlayer(s, &x);
            if (x > 20480) goto BOLT;

            if (pData[3] == 0)
                pData[3]=sector[sect].floorshade;

CLEAR_THE_BOLT:
            if (pData[2])
            {
                pData[2]--;
                sector[sect].floorshade = 20;
                sector[sect].ceilingshade = 20;
                goto BOLT;
            }
            if ((s->xrepeat|s->yrepeat) == 0)
            {
                s->xrepeat=pData[0];
                s->yrepeat=pData[1];
            }
            else if ((krand()&8) == 0)
            {
                pData[0]=s->xrepeat;
                pData[1]=s->yrepeat;
                pData[2] = g_globalRandom&4;
                s->xrepeat=s->yrepeat=0;
                goto CLEAR_THE_BOLT;
            }
            s->picnum++;

            l = g_globalRandom&7;
            s->xrepeat=l+8;

            if (l&1) s->cstat ^= 2;

            if (s->picnum == (BOLT1+1) && (krand()&7) == 0 && sector[sect].floorpicnum == HURTRAIL)
                A_PlaySound(SHORT_CIRCUIT,i);

            if (s->picnum==BOLT1+4) s->picnum=BOLT1;

            if (s->picnum&1)
            {
                sector[sect].floorshade = 0;
                sector[sect].ceilingshade = 0;
            }
            else
            {
                sector[sect].floorshade = 20;
                sector[sect].ceilingshade = 20;
            }
            goto BOLT;

        case WATERDRIP__:

            if (pData[1])
            {
                pData[1]--;
                if (pData[1] == 0)
                    s->cstat &= 32767;
            }
            else
            {
                A_Fall(i);
                A_SetSprite(i,CLIPMASK0);
                if (s->xvel > 0) s->xvel -= 2;

                if (s->zvel == 0)
                {
                    s->cstat |= 32768;

                    if (s->pal != 2 && s->hitag == 0)
                        A_PlaySound(SOMETHING_DRIPPING,i);

                    if (sprite[s->owner].picnum != WATERDRIP)
                    {
                        KILLIT(i);
                    }
                    else
                    {
                        actor[i].bpos.z = s->z = pData[0];
                        pData[1] = 48+(krand()&31);
                    }
                }
            }


            goto BOLT;

        case DOORSHOCK__:
            j = klabs(sector[sect].ceilingz-sector[sect].floorz)>>9;
            s->yrepeat = j+4;
            s->xrepeat = 16;
            s->z = sector[sect].floorz;
            goto BOLT;

        case TOUCHPLATE__:
            if (pData[1] == 1 && s->hitag >= 0)  //Move the sector floor
            {
                x = sector[sect].floorz;

                if (pData[3] == 1)
                {
                    if (x >= pData[2])
                    {
                        sector[sect].floorz = x;
                        pData[1] = 0;
                    }
                    else
                    {
                        sector[sect].floorz += sector[sect].extra;
                        p = CheckPlayerInSector(sect);
                        if (p >= 0) g_player[p].ps->pos.z += sector[sect].extra;
                    }
                }
                else
                {
                    if (x <= s->z)
                    {
                        sector[sect].floorz = s->z;
                        pData[1] = 0;
                    }
                    else
                    {
                        sector[sect].floorz -= sector[sect].extra;
                        p = CheckPlayerInSector(sect);
                        if (p >= 0)
                            g_player[p].ps->pos.z -= sector[sect].extra;
                    }
                }
                goto BOLT;
            }

            if (pData[5] == 1) goto BOLT;

            p = CheckPlayerInSector(sect);
            if (p >= 0 && (g_player[p].ps->on_ground || s->ang == 512))
            {
                if (pData[0] == 0 && !check_activator_motion(s->lotag))
                {
                    pData[0] = 1;
                    pData[1] = 1;
                    pData[3] = !pData[3];
                    G_OperateMasterSwitches(s->lotag);
                    G_OperateActivators(s->lotag,p);
                    if (s->hitag > 0)
                    {
                        s->hitag--;
                        if (s->hitag == 0) pData[5] = 1;
                    }
                }
            }
            else pData[0] = 0;

            if (pData[1] == 1)
            {
                j = headspritestat[STAT_STANDABLE];
                while (j >= 0)
                {
                    if (j != i && sprite[j].picnum == TOUCHPLATE && sprite[j].lotag == s->lotag)
                    {
                        actor[j].t_data[1] = 1;
                        actor[j].t_data[3] = pData[3];
                    }
                    j = nextspritestat[j];
                }
            }
            goto BOLT;

        case CANWITHSOMETHING__:
        case CANWITHSOMETHING2__:
        case CANWITHSOMETHING3__:
        case CANWITHSOMETHING4__:
            A_Fall(i);
            IFHIT(i)
            {
                A_PlaySound(VENT_BUST,i);
                for (j=0;j<10;j++)
                    RANDOMSCRAP(s, i);

                if (s->lotag) A_Spawn(i,s->lotag);

                KILLIT(i);
            }
            goto BOLT;

        case EXPLODINGBARREL__:
        case WOODENHORSE__:
        case HORSEONSIDE__:
        case FLOORFLAME__:
        case FIREBARREL__:
        case FIREVASE__:
        case NUKEBARREL__:
        case NUKEBARRELDENTED__:
        case NUKEBARRELLEAKED__:
        case TOILETWATER__:
        case RUBBERCAN__:
        case STEAM__:
        case CEILINGSTEAM__:
            if (!G_TileHasActor(sprite[i].picnum))
                goto BOLT;
            p = A_FindPlayer(s, &x);
            A_Execute(i,p,x);
            goto BOLT;
        case WATERBUBBLEMAKER__:
            if (!G_TileHasActor(sprite[i].picnum))
                goto BOLT;
            p = A_FindPlayer(s, &x);
            A_Execute(i,p,x);
            goto BOLT;
        }

BOLT:
        i = nexti;
    }
}

static void A_DoProjectileBounce(int i)
{
    int dax, day, daz = 4096;
    spritetype *s = &sprite[i];
    int hitsect = s->sectnum;
    int k = sector[hitsect].wallptr;
    int l = wall[k].point2;

    int xvect = mulscale10(s->xvel,sintable[(s->ang+512)&2047]);
    int yvect = mulscale10(s->xvel,sintable[s->ang&2047]);
    int zvect = s->zvel;

    int daang = getangle(wall[l].x-wall[k].x,wall[l].y-wall[k].y);

    if (s->z < (actor[i].floorz+actor[i].ceilingz)>>1)
        k = sector[hitsect].ceilingheinum;
    else
        k = sector[hitsect].floorheinum;

    dax = mulscale14(k,sintable[(daang)&2047]);
    day = mulscale14(k,sintable[(daang+1536)&2047]);

    k = xvect*dax+yvect*day+zvect*daz;
    l = dax*dax+day*day+daz*daz;
    if ((klabs(k)>>14) < l)
    {
        k = divscale17(k,l);
        xvect -= mulscale16(dax,k);
        yvect -= mulscale16(day,k);
        zvect -= mulscale16(daz,k);
    }

    s->zvel = zvect;
    s->xvel = ksqrt(dmulscale8(xvect,xvect,yvect,yvect));
    s->ang = getangle(xvect,yvect);
}

static void G_WeaponHitCeilingOrFloor(int32_t i, spritetype* s, int* j)
{
    if (*j == 0)
        return;

    if (s->z < actor[i].ceilingz)
    {
        *j = 16384 | s->sectnum;
        s->zvel = -1;
    }
    else if (s->z > actor[i].floorz + ZOFFSET2 * (sector[s->sectnum].lotag == ST_1_ABOVE_WATER))
    {
        *j = 16384 | s->sectnum;

        if (sector[s->sectnum].lotag != ST_1_ABOVE_WATER)
            s->zvel = 1;
    }
}

static void P_HandleBeingSpitOn(DukePlayer_t* const ps)
{
    ps->q16horiz += F16(32);
    ps->return_to_center = 8;

    if (ps->loogcnt)
        return;

    if (!A_CheckSoundPlaying(ps->i, DUKE_LONGTERM_PAIN))
        A_PlaySound(DUKE_LONGTERM_PAIN, ps->i);

    int j = 3 + (krand() & 3);
    ps->numloogs = j;
    ps->loogcnt = 24 * 4;
    for (bssize_t x = 0; x < j; x++)
    {
        ps->loogiex[x] = krand() % xdim;
        ps->loogiey[x] = krand() % ydim;
    }
}

static void A_DoProjectileEffects(int spriteNum, const vec3_t* davect, int radiusDamage)
{
    auto const pProj = &actor[spriteNum].projectile;

    if (pProj->spawns >= 0)
    {
        int const newSpr = A_Spawn(spriteNum, pProj->spawns);

        if (davect)
            Bmemcpy(&sprite[newSpr].xyz, davect, sizeof(vec3_t));

        if (pProj->sxrepeat > 4)
            sprite[newSpr].xrepeat = pProj->sxrepeat;
        if (pProj->syrepeat > 4)
            sprite[newSpr].yrepeat = pProj->syrepeat;
    }

    if (pProj->isound >= 0)
        A_PlaySound(pProj->isound, spriteNum);

    if (!radiusDamage)
        return;

    auto const pSprite = &sprite[spriteNum];
    pSprite->extra = Proj_GetDamage(pProj);
    int const dmg = pSprite->extra;
    A_RadiusDamage(spriteNum, pProj->hitradius, dmg >> 2, dmg >> 1, dmg - (dmg >> 2), dmg);
}

static void G_MoveWeapons(void)
{
    int spriteNum = headspritestat[STAT_PROJECTILE], j=0, nexti;
    spritetype *s;

    vec3_t davect;

    while (spriteNum >= 0)
    {
        nexti = nextspritestat[spriteNum];
        s = &sprite[spriteNum];

        if ((unsigned)s->sectnum >= MAXSECTORS) KILLIT(spriteNum);

        actor[spriteNum].bpos = s->xyz;
        // here

        if (A_CheckSpriteFlags(spriteNum,SFLAG_PROJECTILE))
        {
            auto pProj = &actor[spriteNum].projectile;
            /* Custom projectiles.  This is a big hack. */

            if (pProj->pal >= 0)
                s->pal = pProj->pal;

            if (pProj->workslike & PROJECTILE_KNEE)
                KILLIT(spriteNum);

            if (pProj->workslike & PROJECTILE_RPG)
            {
                VM_UpdateAnim(spriteNum, &actor[spriteNum].t_data[0]);

                if (pProj->flashcolor)
                    G_AddGameLight(spriteNum, s->sectnum, { 0, 0, LIGHTZOFF(spriteNum) }, LIGHTRAD4(spriteNum),
                        0, 100,
                        pProj->flashcolor, PR_LIGHT_PRIO_LOW_GAME);

                if (pProj->workslike & PROJECTILE_COOLEXPLOSION1)
                {
                    s->shade++;
                    if (s->shade >= 40)
                        KILLIT(spriteNum);
                }

                if (pProj->drop)
                    s->zvel -= pProj->drop;

                if (pProj->workslike & PROJECTILE_SPIT)
                    if (s->zvel < ACTOR_MAXFALLINGZVEL)
                        s->zvel += g_spriteGravity-112;

                int32_t projVel = s->xvel;
                int32_t projZvel = s->zvel;

                if (sector[s->sectnum].lotag == ST_2_UNDERWATER)
                {
                    projVel = s->xvel>>1;
                    projZvel = s->zvel>>1;
                }

                if (pProj->trail >= 0)
                {
                    for (int32_t f = 0; f <= pProj->tnum; f++)
                    {
                        j = A_Spawn(spriteNum, pProj->trail);
                        if (pProj->toffset != 0)
                            sprite[j].z += (pProj->toffset << 8);
                        if (pProj->txrepeat >= 0)
                            sprite[j].xrepeat = pProj->txrepeat;
                        if (pProj->tyrepeat >= 0)
                            sprite[j].yrepeat = pProj->tyrepeat;
                    }
                }

                for (int32_t projMoveCnt = 1; projMoveCnt <= pProj->movecnt; projMoveCnt++)
                {
                    davect = s->xyz;
                    A_GetZLimits(spriteNum);

                    j = A_MoveSprite(spriteNum,
                                     (projVel * (sintable[(s->ang + 512) & 2047])) >> 14,
                                     (projVel * (sintable[s->ang & 2047])) >> 14, projZvel, CLIPMASK1);

                    if (j)
                        break;
                }

                if (!(pProj->workslike & PROJECTILE_BOUNCESOFFWALLS) &&
                        s->yvel >= 0 && sprite[s->yvel].sectnum < MAXSECTORS)
                    if (FindDistance2D(s->x-sprite[s->yvel].x,s->y-sprite[s->yvel].y) < 256)
                        j = 49152|s->yvel;

                actor[spriteNum].movflag = j;

                if ((unsigned)s->sectnum >= MAXSECTORS)
                {
                    KILLIT(spriteNum);
                }

                if (pProj->workslike & PROJECTILE_TIMED && pProj->range > 0)
                {
                    if (!(actor[spriteNum].t_data[8]))
                        actor[spriteNum].t_data[8] = 1;
                    else
                        actor[spriteNum].t_data[8]++;

                    if (actor[spriteNum].t_data[8] > pProj->range)
                    {
                        if (pProj->workslike & PROJECTILE_EXPLODEONTIMER)
                            A_DoProjectileEffects(spriteNum, &davect, 1);

                        KILLIT(spriteNum);
                    }
                }

                if (pProj->workslike & PROJECTILE_BOUNCESOFFWALLS)
                {
                    /*                    if(s->yvel < 1 || s->extra < 2 || (s->xvel|s->zvel) == 0)
                                Did this cause the bug with prematurely exploding projectiles? */
                    if (s->yvel < 1)
                    {
                        A_DoProjectileEffects(spriteNum, &davect, 1);
                        KILLIT(spriteNum);
                    }

                }

                if ((j & 49152) != 49152 && !(pProj->workslike & PROJECTILE_BOUNCESOFFWALLS))
                    G_WeaponHitCeilingOrFloor(spriteNum, s, &j);

                if (pProj->workslike & PROJECTILE_WATERBUBBLES &&
                        sector[s->sectnum].lotag == ST_2_UNDERWATER && rnd(140))
                    A_Spawn(spriteNum,WATERBUBBLE);

                if (j != 0)
                {
                    if (pProj->workslike & PROJECTILE_COOLEXPLOSION1)
                    {
                        s->xvel = 0;
                        s->zvel = 0;
                    }

                    if ((j&49152) == 49152)
                    {
                        j &= (MAXSPRITES-1);

                        if (pProj->workslike & PROJECTILE_BOUNCESOFFSPRITES)
                        {
                            s->yvel--;

                            int32_t const projAngle = getangle(sprite[j].x-s->x,sprite[j].y-s->y)+(sprite[j].cstat&16?0:512);
                            s->ang = ((projAngle << 1) - s->ang)&2047;

                            if (pProj->bsound >= 0)
                                A_PlaySound(pProj->bsound,spriteNum);

                            if (pProj->workslike & PROJECTILE_LOSESVELOCITY)
                            {
                                s->xvel=s->xvel>>1;
                                s->zvel=s->zvel>>1;
                            }
                            if (!(pProj->workslike & PROJECTILE_FORCEIMPACT))
                                goto BOLT;
                        }

                        A_DamageObject(j,spriteNum);

                        if (sprite[j].picnum == APLAYER)
                        {
                            int32_t playerNum = sprite[j].yvel;
                            A_PlaySound(PISTOL_BODYHIT,j);

                            if (pProj->workslike & PROJECTILE_SPIT)
                                P_HandleBeingSpitOn(g_player[playerNum].ps);
                        }

                        if (pProj->workslike & PROJECTILE_RPG_IMPACT)
                        {
                            // [JM] Commented out because the A_DamageObject above does this now, allowing it to be modified by EVENT_DAMAGESPRITE.
                            //actor[j].htowner = s->owner;
                            //actor[j].htpicnum = s->picnum;
                            //actor[j].htextra += pProj->extra;

                            A_DoProjectileEffects(spriteNum, &davect, 0);

                            if (!(pProj->workslike & PROJECTILE_FORCEIMPACT))
                                KILLIT(spriteNum);
                        }

                        if (pProj->workslike & PROJECTILE_FORCEIMPACT)
                            goto BOLT;

                    }
                    else if ((j&49152) == 32768)
                    {
                        j &= (MAXWALLS-1);

                        if (pProj->workslike & PROJECTILE_BOUNCESOFFMIRRORS &&
                                (wall[j].overpicnum == MIRROR || wall[j].picnum == MIRROR))
                        {
                            int32_t const projAngle = getangle(wall[wall[j].point2].x-wall[j].x,
                                                               wall[wall[j].point2].y-wall[j].y);
                            
                            s->ang = ((projAngle << 1) - s->ang) & 2047;
                            s->owner = spriteNum;
                            A_Spawn(spriteNum,TRANSPORTERSTAR);
                            goto BOLT;
                        }
                        else
                        {
                            setspritez(spriteNum,&davect);
                            A_DamageWall(spriteNum,j,s->xyz,s->picnum);

                            if (pProj->workslike & PROJECTILE_BOUNCESOFFWALLS)
                            {
                                if (wall[j].overpicnum != MIRROR && wall[j].picnum != MIRROR)
                                    s->yvel--;

                                int32_t const projAngle = getangle(wall[wall[j].point2].x - wall[j].x,
                                                                   wall[wall[j].point2].y - wall[j].y);

                                s->ang = ((projAngle << 1) - s->ang) & 2047;

                                if (pProj->bsound >= 0)
                                    A_PlaySound(pProj->bsound,spriteNum);

                                if (pProj->workslike & PROJECTILE_LOSESVELOCITY)
                                {
                                    s->xvel=s->xvel>>1;
                                    s->zvel=s->zvel>>1;
                                }
                                goto BOLT;
                            }
                        }
                    }
                    else if ((j&49152) == 16384)
                    {
                        setspritez(spriteNum,&davect);

                        if (s->zvel < 0)
                        {
                            if (sector[s->sectnum].ceilingstat&1 && sector[s->sectnum].ceilingpal == 0)
                                KILLIT(spriteNum);

                            Sect_DamageCeiling(spriteNum, s->sectnum);
                        }

                        if (pProj->workslike & PROJECTILE_BOUNCESOFFWALLS)
                        {
                            A_DoProjectileBounce(spriteNum);
                            A_SetSprite(spriteNum, CLIPMASK1);

                            s->yvel--;

                            if (pProj->bsound >= 0)
                                A_PlaySound(pProj->bsound,spriteNum);

                            if (pProj->workslike & PROJECTILE_LOSESVELOCITY)
                            {
                                s->xvel=s->xvel>>1;
                                s->zvel=s->zvel>>1;
                            }

                            goto BOLT;
                        }
                    }

                    if (pProj->workslike & PROJECTILE_HITSCAN)
                    {
                        if (!G_TileHasActor(sprite[spriteNum].picnum))
                            goto BOLT;

                        int32_t playerDist;
                        int32_t playerNum = A_FindPlayer(s,&playerDist);
                        A_Execute(spriteNum, playerNum, playerDist);
                        goto BOLT;
                    }

                    A_DoProjectileEffects(spriteNum, &davect, 1);
                    KILLIT(spriteNum);
                }
                goto BOLT;
            }
        }
        else
        {

            // here
            switch (tileGetMapping(s->picnum))
            {
            case RADIUSEXPLOSION__:
            case KNEE__:
                KILLIT(spriteNum);
            case TONGUE__:
            {
                T1(spriteNum) = sintable[(T2(spriteNum)) & 2047] >> 9;
                T2(spriteNum) += 32;
                if (T2(spriteNum) > 2047) KILLIT(spriteNum);

                if (sprite[s->owner].statnum == MAXSTATUS)
                    if (A_CheckEnemySprite(&sprite[s->owner]) == 0)
                        KILLIT(spriteNum);

                s->ang = sprite[s->owner].ang;
                s->x = sprite[s->owner].x;
                s->y = sprite[s->owner].y;
                if (sprite[s->owner].picnum == APLAYER)
                    s->z = sprite[s->owner].z - (34 << 8);

                int32_t segmentCnt;
                for (segmentCnt = 0; segmentCnt < T1(spriteNum); segmentCnt++)
                {
                    int32_t const newSprite = A_InsertSprite(s->sectnum,
                                              s->x + ((segmentCnt * sintable[(s->ang + 512) & 2047]) >> 9),
                                              s->y + ((segmentCnt * sintable[s->ang & 2047]) >> 9),
                                              s->z + ((segmentCnt * ksgn(s->zvel)) * klabs(s->zvel / 12)), TONGUE, -40 + (segmentCnt << 1),
                                              8, 8, 0, 0, 0, spriteNum, STAT_MISC);

                    sprite[newSprite].cstat = 128;
                    sprite[newSprite].pal = 8;
                }
                int32_t const newSprite = A_InsertSprite(s->sectnum,
                                          s->x + ((segmentCnt * sintable[(s->ang + 512) & 2047]) >> 9),
                                          s->y + ((segmentCnt * sintable[s->ang & 2047]) >> 9),
                                          s->z + ((segmentCnt * ksgn(s->zvel)) * klabs(s->zvel / 12)), INNERJAW, -40,
                                          32, 32, 0, 0, 0, spriteNum, STAT_MISC);

                sprite[newSprite].cstat = 128;
                if (T2(spriteNum) > 512 && T2(spriteNum) < (1024))
                    sprite[newSprite].picnum = INNERJAW + 1;

                goto BOLT;
            }
            case FREEZEBLAST__:
                if (s->yvel < 1 || s->extra < 2 || (s->xvel|s->zvel) == 0)
                {
                    j = A_Spawn(spriteNum,TRANSPORTERSTAR);
                    sprite[j].pal = 1;
                    sprite[j].xrepeat = 32;
                    sprite[j].yrepeat = 32;
                    KILLIT(spriteNum);
                }
                fallthrough__;
            case SHRINKSPARK__:
            case RPG__:
            case FIRELASER__:
            case SPIT__:
            case COOLEXPLOSION1__:
            {

                if (s->picnum == COOLEXPLOSION1)
                    if (!S_CheckSoundPlaying(WIERDSHOT_FLY))
                        A_PlaySound(WIERDSHOT_FLY, spriteNum);

                int32_t projVel = s->xvel;
                int32_t projZvel = s->zvel;

                if (s->picnum == RPG && sector[s->sectnum].lotag == ST_2_UNDERWATER)
                {
                    projVel = s->xvel >> 1;
                    projZvel = s->zvel >> 1;
                }

                davect = s->xyz;

                A_GetZLimits(spriteNum);

                switch (tileGetMapping(s->picnum))
                {
                    case RPG__:
                        if (actor[spriteNum].htpicnum != BOSS2 && s->xrepeat >= 10 && sector[s->sectnum].lotag != ST_2_UNDERWATER)
                        {
                            j = A_Spawn(spriteNum, SMALLSMOKE);
                            sprite[j].z += (1 << 8);
                        }
                        break;
                }

                j = A_MoveSprite(spriteNum, (projVel * (sintable[(s->ang + 512) & 2047])) >> 14, (projVel * (sintable[s->ang & 2047])) >> 14, projZvel, CLIPMASK1);

                if (s->picnum == RPG && s->yvel >= 0 && sprite[s->yvel].sectnum < MAXSECTORS)
                    if (FindDistance2D(s->x - sprite[s->yvel].x, s->y - sprite[s->yvel].y) < 256)
                        j = 49152 | s->yvel;

                actor[spriteNum].movflag = j;

                if ((unsigned)s->sectnum >= MAXSECTORS)
                    KILLIT(spriteNum);

                if ((j & 49152) != 49152 && (s->picnum != FREEZEBLAST))
                    G_WeaponHitCeilingOrFloor(spriteNum, s, &j);

                if (s->picnum == FIRELASER)
                {
                    for (int32_t segmentCnt = -3; segmentCnt < 2; segmentCnt++)
                    {
                        vec3_t const offset = { (segmentCnt * sintable[(s->ang + 512) & 2047]) >> 9,
                                                (segmentCnt * sintable[s->ang & 2047]) >> 9,
                                                (segmentCnt * ksgn(s->zvel)) * klabs(s->zvel / 24) };

                        int32_t const newSprite = A_InsertSprite(s->sectnum,
                                                  s->x + offset.x,
                                                  s->y + offset.y,
                                                  s->z + offset.z, FIRELASER, -40 + (segmentCnt << 2),
                                                  s->xrepeat, s->yrepeat, 0, 0, 0, s->owner, STAT_MISC);

                        actor[newSprite].bpos = actor[spriteNum].bpos + offset;
                        sprite[newSprite].cstat |= 128;
                        sprite[newSprite].pal = s->pal;
                    }
                }
                else if (s->picnum == SPIT) if (s->zvel < ACTOR_MAXFALLINGZVEL)
                    s->zvel += g_spriteGravity - 112;

                if (j != 0)
                {
                    if (s->picnum == COOLEXPLOSION1)
                    {
                        if ((j & 49152) == 49152 && sprite[j & (MAXSPRITES - 1)].picnum != APLAYER)
                            goto BOLT;
                        s->xvel = 0;
                        s->zvel = 0;
                    }

                    if ((j & 49152) == 49152)
                    {
                        j &= (MAXSPRITES - 1);

                        if (s->picnum == FREEZEBLAST && sprite[j].pal == 1)
                            if (A_CheckEnemySprite(&sprite[j]) || sprite[j].picnum == APLAYER)
                            {
                                j = A_Spawn(spriteNum, TRANSPORTERSTAR);
                                sprite[j].pal = 1;
                                sprite[j].xrepeat = 32;
                                sprite[j].yrepeat = 32;

                                KILLIT(spriteNum);
                            }

                        A_DamageObject(j, spriteNum);

                        if (sprite[j].picnum == APLAYER)
                        {
                            int32_t playerNum = sprite[j].yvel;
                            A_PlaySound(PISTOL_BODYHIT, j);

                            if (s->picnum == SPIT)
                                P_HandleBeingSpitOn(g_player[playerNum].ps);
                        }
                    }
                    else if ((j & 49152) == 32768)
                    {
                        j &= (MAXWALLS - 1);

                        if (s->picnum != RPG && s->picnum != FREEZEBLAST && s->picnum != SPIT && (wall[j].overpicnum == MIRROR || wall[j].picnum == MIRROR))
                        {
                            int32_t const projAngle = getangle(wall[wall[j].point2].x - wall[j].x,
                                                               wall[wall[j].point2].y - wall[j].y);

                            s->ang = ((projAngle << 1) - s->ang) & 2047;
                            s->owner = spriteNum;
                            A_Spawn(spriteNum, TRANSPORTERSTAR);
                            goto BOLT;
                        }
                        else
                        {
                            setspritez(spriteNum, &davect);
                            A_DamageWall(spriteNum, j, s->xyz, s->picnum);

                            if (s->picnum == FREEZEBLAST)
                            {
                                if (wall[j].overpicnum != MIRROR && wall[j].picnum != MIRROR)
                                {
                                    s->extra >>= 1;
                                    s->yvel--;
                                }

                                int32_t const projAngle = getangle(wall[wall[j].point2].x - wall[j].x,
                                                                   wall[wall[j].point2].y - wall[j].y);

                                s->ang = ((projAngle << 1) - s->ang) & 2047;
                                goto BOLT;
                            }
                        }
                    }
                    else if ((j & 49152) == 16384)
                    {
                        setspritez(spriteNum, &davect);

                        if (s->zvel < 0)
                        {
                            if (sector[s->sectnum].ceilingstat & 1)
                                if (sector[s->sectnum].ceilingpal == 0)
                                    KILLIT(spriteNum);

                            Sect_DamageCeiling(spriteNum, s->sectnum);
                        }

                        if (s->picnum == FREEZEBLAST)
                        {
                            A_DoProjectileBounce(spriteNum);
                            A_SetSprite(spriteNum, CLIPMASK1);
                            s->extra >>= 1;
                            if (s->xrepeat > 8)
                                s->xrepeat -= 2;
                            if (s->yrepeat > 8)
                                s->yrepeat -= 2;
                            s->yvel--;
                            goto BOLT;
                        }
                    }

                    if (s->picnum != SPIT)
                    {
                        if (s->picnum == RPG)
                        {
                            int32_t const newSprite = A_Spawn(spriteNum, EXPLOSION2);
                            sprite[newSprite].xyz = davect;

                            if (s->xrepeat < 10)
                            {
                                sprite[newSprite].xrepeat = 6;
                                sprite[newSprite].yrepeat = 6;
                            }
                            else if ((j & 49152) == 16384)
                            {
                                if (s->zvel > 0)
                                    A_Spawn(spriteNum, EXPLOSION2BOT);
                                else
                                {
                                    sprite[newSprite].cstat |= 8;
                                    sprite[newSprite].z += (48 << 8);
                                }

                            }
                            A_PlaySound(RPG_EXPLODE, spriteNum);

                            if (s->xrepeat >= 10)
                            {
                                int16_t const x = s->extra;
                                A_RadiusDamage(spriteNum, g_rpgBlastRadius, x >> 2, x >> 1, x - (x >> 2), x);
                            }
                            else
                            {
                                int16_t const x = s->extra + (g_globalRandom & 3);
                                A_RadiusDamage(spriteNum, (g_rpgBlastRadius >> 1), x >> 2, x >> 1, x - (x >> 2), x);
                            }
                        }
                        else if (s->picnum == SHRINKSPARK)
                        {
                            A_Spawn(spriteNum, SHRINKEREXPLOSION);
                            A_PlaySound(SHRINKER_HIT, spriteNum);
                            A_RadiusDamage(spriteNum, g_shrinkerBlastRadius, 0, 0, 0, 0);
                        }
                        else if (s->picnum != COOLEXPLOSION1 && s->picnum != FREEZEBLAST && s->picnum != FIRELASER)
                        {
                            int32_t const newSprite = A_Spawn(spriteNum, EXPLOSION2);
                            sprite[newSprite].xrepeat = sprite[newSprite].yrepeat = s->xrepeat >> 1;
                            if ((j & 49152) == 16384)
                            {
                                if (s->zvel < 0)
                                {
                                    sprite[newSprite].cstat |= 8;
                                    sprite[newSprite].z += (72 << 8);
                                }

                            }
                        }
                    }
                    if (s->picnum != COOLEXPLOSION1) KILLIT(spriteNum);
                }
                if (s->picnum == COOLEXPLOSION1)
                {
                    s->shade++;
                    if (s->shade >= 40) KILLIT(spriteNum);
                }
                else if (s->picnum == RPG && sector[s->sectnum].lotag == ST_2_UNDERWATER && s->xrepeat >= 10 && rnd(140))
                    A_Spawn(spriteNum, WATERBUBBLE);

                goto BOLT;
            }
            case SHOTSPARK1__:
                if (!G_TileHasActor(sprite[spriteNum].picnum))
                    goto BOLT;

                int32_t playerDist;
                int32_t playerNum = A_FindPlayer(s,&playerDist);
                A_Execute(spriteNum, playerNum, playerDist);
                goto BOLT;
            }
        }
BOLT:
        spriteNum = nexti;
    }
}

// Check prevention of teleportation *when alive*. For example, commanders and
// octabrains would be transported by SE7 (both water and normal) only if dead.
static int A_CheckNonTeleporting(int const spriteNum)
{
    int const tileNum = sprite[spriteNum].picnum;
    return !!(A_CheckSpriteFlags(spriteNum, SFLAG_NOTELEPORT) || tileNum == SHARK || tileNum == COMMANDER || tileNum == OCTABRAIN
        || (tileNum >= GREENSLIME && tileNum <= GREENSLIME + 7));
}

static void G_MoveTransports(void)
{
    int warpspriteto;
    int j, k, sect, sectlotag, nexti, nextj;
    int onfloorz;

    int i = headspritestat[STAT_TRANSPORT];
    while (i >= 0)
    {
        sect = SECT(i);
        sectlotag = sector[sect].lotag;

        nexti = nextspritestat[i];

        if (OW(i) == i)
        {
            i = nexti;
            continue;
        }

        onfloorz = T5(i);

        if (T1(i) > 0) T1(i)--;

        j = headspritesect[sect];
        while (j >= 0)
        {
            nextj = nextspritesect[j];

            switch (sprite[j].statnum)
            {
                case STAT_PLAYER:    // Player
                {
                    if (sprite[j].owner != -1)
                    {
                        int const  playerNum = sprite[j].yvel;
                        auto& thisPlayer = g_player[playerNum];
                        auto const pPlayer = thisPlayer.ps;

                        pPlayer->on_warping_sector = 1;

                        if (pPlayer->transporter_hold == 0 && pPlayer->jumping_counter == 0)
                        {
                            if (pPlayer->on_ground && sectlotag == ST_0_NO_EFFECT && onfloorz && pPlayer->jetpack_on == 0)
                            {
                                if (sprite[i].pal == 0)
                                {
                                    A_Spawn(i, TRANSPORTERBEAM);
                                    A_PlaySound(TELEPORTER, i);
                                }

                                pPlayer->q16ang = fix16_from_int(sprite[OW(i)].ang);

                                if (sprite[OW(i)].owner != OW(i))
                                {
                                    T1(i) = 13;
                                    actor[OW(i)].t_data[0] = 13;
                                    pPlayer->transporter_hold = 13;
                                }

                                pPlayer->bobposx = pPlayer->opos.x = pPlayer->pos.x = sprite[OW(i)].x;
                                pPlayer->bobposy = pPlayer->opos.y = pPlayer->pos.y = sprite[OW(i)].y;
                                pPlayer->opos.z = pPlayer->pos.z = sprite[OW(i)].z - PHEIGHT;

                                changespritesect(j, sprite[OW(i)].sectnum);
                                pPlayer->cursectnum = sprite[j].sectnum;

                                // Handle Telefragging.
                                for (int32_t ALL_PLAYERS(otherPnum))
                                {
                                    if (otherPnum == playerNum)
                                        continue;

                                    auto const otherPlayer = g_player[otherPnum].ps;
                                    if (otherPlayer->cursectnum == sprite[OW(i)].sectnum)
                                    {
                                        if (ldist(&pPlayer->pos, &otherPlayer->pos) <= (PLAYER_WALLDIST<<1))
                                        {
                                            otherPlayer->frag_ps = playerNum;
                                            sprite[otherPlayer->i].extra = 0;
                                        }
                                    }
                                }

                                if (playerNum == screenpeek) // [JM] Hack to make sure sound positions update immediately.
                                {
                                    ud.camerapos.x = pPlayer->pos.x;
                                    ud.camerapos.y = pPlayer->pos.y;
                                    ud.camerapos.z = pPlayer->pos.z;
                                    ud.cameraq16ang = pPlayer->q16ang;
                                    ud.camerasect = pPlayer->cursectnum;
                                }

                                if (sprite[i].pal == 0)
                                {
                                    k = A_Spawn(OW(i), TRANSPORTERBEAM);
                                    A_PlaySound(TELEPORTER, k);
                                }

                                break;
                            }

                            if (onfloorz == 0 && klabs(SZ(i) - pPlayer->pos.z) < 6144)
                            {
                                if (!pPlayer->jetpack_on || TEST_SYNC_KEY(thisPlayer.inputBits->bits, SK_JUMP)
                                    || TEST_SYNC_KEY(thisPlayer.inputBits->bits, SK_CROUCH))
                                {
                                    pPlayer->bobposx = pPlayer->pos.x += sprite[OW(i)].x - SX(i);
                                    pPlayer->bobposy = pPlayer->pos.y += sprite[OW(i)].y - SY(i);
                                    pPlayer->pos.z = (pPlayer->jetpack_on && (TEST_SYNC_KEY(thisPlayer.inputBits->bits, SK_JUMP)
                                        || pPlayer->jetpack_on < 11))
                                        ? sprite[OW(i)].z - 6144
                                        : sprite[OW(i)].z + 6144;

                                    actor[j].bpos = pPlayer->opos = pPlayer->pos;

                                    changespritesect(j, sprite[OW(i)].sectnum);
                                    pPlayer->cursectnum = sprite[OW(i)].sectnum;

                                    break;
                                }
                            }

                            int doWater = 0;

                            if (onfloorz)
                            {
                                if (sectlotag == ST_1_ABOVE_WATER)
                                    doWater = P_Submerge(playerNum, pPlayer, sect, sprite[OW(i)].sectnum);
                                else if (sectlotag == ST_2_UNDERWATER)
                                    doWater = P_Emerge(playerNum, pPlayer, sect, sprite[OW(i)].sectnum);

                                if (doWater == 1)
                                {
                                    pPlayer->pos.x += sprite[OW(i)].x - SX(i);
                                    pPlayer->pos.y += sprite[OW(i)].y - SY(i);

                                    P_FinishWaterChange(j, pPlayer, sectlotag, OW(i), sprite[OW(i)].sectnum);
                                }
                            }
                        }
                        else if (!(sectlotag == ST_1_ABOVE_WATER && pPlayer->on_ground == 1))
                            break;
                    }
                }
                break;

                case STAT_PROJECTILE:
                    // comment out to make RPGs pass through water: (r1450 breaks this)
                    //if (sectlotag != 0) goto JBOLT;
                case STAT_ACTOR:
                    if (sprite[j].extra > 0 && A_CheckNonTeleporting(j)) // [JM] This is lame. If SFLAG_NOTELEPORT is on, it should NEVER teleport, regardless of health.
                            goto JBOLT;
                    fallthrough__;
                case STAT_MISC:
                case STAT_FALLER:
                case STAT_DUMMYPLAYER:
                {
                    int const absZvel = klabs(sprite[j].zvel);

                    warpspriteto = 0;
                    if (absZvel && sectlotag == ST_2_UNDERWATER && sprite[j].z < (sector[sect].ceilingz + absZvel))
                        warpspriteto = 1;

                    if (absZvel && sectlotag == ST_1_ABOVE_WATER && sprite[j].z > (sector[sect].floorz - absZvel))
                        warpspriteto = 1;

                    if (sectlotag == 0 && (onfloorz || klabs(sprite[j].z-SZ(i)) < 4096))
                    {
                        if (sprite[OW(i)].owner != OW(i) && onfloorz && T1(i) > 0 && sprite[j].statnum != 5)
                        {
                            T1(i)++;
                            goto BOLT;
                        }
                        warpspriteto = 1;
                    }

                    if (warpspriteto && A_CheckSpriteFlags(j,SFLAG_DECAL)) goto JBOLT;

                    if (warpspriteto) switch (tileGetMapping(sprite[j].picnum))
                    {
                        case TRANSPORTERSTAR__:
                        case TRANSPORTERBEAM__:
                        case TRIPBOMB__:
                        case BULLETHOLE__:
                        case WATERSPLASH2__:
                        case BURNING__:
                        case BURNING2__:
                        case FIRE__:
                        case FIRE2__:
                        case TOILETWATER__:
                        case LASERLINE__:
                            goto JBOLT;

                        case PLAYERONWATER__:
                        {
                            if (sectlotag == ST_2_UNDERWATER)
                            {
                                sprite[j].cstat &= 32767;
                                break;
                            }
                        }
                        fallthrough__;

                        default:
                        {
                            if (sprite[j].statnum == STAT_MISC && !(sectlotag == ST_1_ABOVE_WATER || sectlotag == ST_2_UNDERWATER))
                                break;
                        }
                        fallthrough__;

                        case WATERBUBBLE__:
                        {
                            //                                if( rnd(192) && sprite[j].picnum == WATERBUBBLE)
                            //                                 break;

                            if (sectlotag > 0)
                            {
                                k = A_Spawn(j, WATERSPLASH2);
                                if (sectlotag == 1 && sprite[j].statnum == 4)
                                {
                                    sprite[k].xvel = sprite[j].xvel >> 1;
                                    sprite[k].ang = sprite[j].ang;
                                    A_SetSprite(k, CLIPMASK0);
                                }
                            }

                            switch (sectlotag)
                            {
                            case ST_0_NO_EFFECT:
                                if (onfloorz)
                                {
                                    if (sprite[j].statnum == 4 || (CheckPlayerInSector(sect) == -1 && CheckPlayerInSector(sprite[OW(i)].sectnum) == -1))
                                    {
                                        sprite[j].x += (sprite[OW(i)].x - SX(i));
                                        sprite[j].y += (sprite[OW(i)].y - SY(i));
                                        sprite[j].z -= SZ(i) - sector[sprite[OW(i)].sectnum].floorz;
                                        sprite[j].ang = sprite[OW(i)].ang;

                                        actor[j].bpos = sprite[j].xyz;

                                        if (sprite[i].pal == 0)
                                        {
                                            k = A_Spawn(i, TRANSPORTERBEAM);
                                            A_PlaySound(TELEPORTER, k);

                                            k = A_Spawn(OW(i), TRANSPORTERBEAM);
                                            A_PlaySound(TELEPORTER, k);
                                        }

                                        if (sprite[OW(i)].owner != OW(i))
                                        {
                                            T1(i) = 13;
                                            actor[OW(i)].t_data[0] = 13;
                                        }

                                        changespritesect(j, sprite[OW(i)].sectnum);
                                    }
                                }
                                else
                                {
                                    sprite[j].x += (sprite[OW(i)].x - SX(i));
                                    sprite[j].y += (sprite[OW(i)].y - SY(i));
                                    sprite[j].z = sprite[OW(i)].z + 4096;

                                    actor[j].bpos = sprite[j].xyz;

                                    changespritesect(j, sprite[OW(i)].sectnum);
                                }
                                break;
                            case ST_1_ABOVE_WATER:
                                sprite[j].x += (sprite[OW(i)].x - SX(i));
                                sprite[j].y += (sprite[OW(i)].y - SY(i));
                                sprite[j].z = sector[sprite[OW(i)].sectnum].ceilingz + absZvel;

                                actor[j].bpos = sprite[j].xyz;

                                changespritesect(j, sprite[OW(i)].sectnum);

                                break;
                            case ST_2_UNDERWATER:
                                sprite[j].x += (sprite[OW(i)].x - SX(i));
                                sprite[j].y += (sprite[OW(i)].y - SY(i));
                                sprite[j].z = sector[sprite[OW(i)].sectnum].floorz - absZvel;

                                actor[j].bpos = sprite[j].xyz;

                                changespritesect(j, sprite[OW(i)].sectnum);

                                break;
                            }

                            break;
                        }
                    }
                }
                break;

            }
JBOLT:
            j = nextj;
        }
BOLT:
        i = nexti;
    }
}

static short A_FindLocator(int n,int sn)
{
    int i = headspritestat[STAT_LOCATOR];

    while (i >= 0)
    {
        if ((sn == -1 || sn == SECT(i)) && n == SLT(i))
            return i;
        i = nextspritestat[i];
    }
    return -1;
}

static void G_MoveActors(void)
{
    int x, m, l;
    int32_t *t;
    int a, j, nextj, p, k;
    int i = headspritestat[STAT_ACTOR];

    while (i >= 0)
    {
        auto s = &sprite[i];
        int nexti = nextspritestat[i];
        int sect = s->sectnum;
        int switchpicnum = s->picnum;

        if (s->xrepeat == 0 || (unsigned)sect >= MAXSECTORS)
            KILLIT(i);

        t = &actor[i].t_data[0];
        actor[i].bpos = s->xyz;  

        if ((s->picnum > GREENSLIME) && (s->picnum <= GREENSLIME+7))
            switchpicnum = GREENSLIME;

        switch (tileGetMapping(switchpicnum))
        {
        case DUCK__:
        case TARGET__:
            if (s->cstat&32)
            {
                t[0]++;
                if (t[0] > 60)
                {
                    t[0] = 0;
                    s->cstat = 128+257+16;
                    s->extra = 1;
                }
            }
            else
            {
                j = A_IncurDamage(i);
                if (j >= 0)
                {
                    s->cstat = 32+128;
                    k = 1;

                    j = headspritestat[STAT_ACTOR];
                    while (j >= 0)
                    {
                        if (sprite[j].lotag == s->lotag &&
                                sprite[j].picnum == s->picnum)
                        {
                            if ((sprite[j].hitag && !(sprite[j].cstat&32)) ||
                                    (!sprite[j].hitag && (sprite[j].cstat&32))
                               )
                            {
                                k = 0;
                                break;
                            }
                        }

                        j = nextspritestat[j];
                    }

                    if (k == 1)
                    {
                        G_OperateActivators(s->lotag,-1);
                        G_OperateForceFields(i,s->lotag);
                        G_OperateMasterSwitches(s->lotag);
                    }
                }
            }
            goto BOLT;

        case RESPAWNMARKERRED__:
        case RESPAWNMARKERYELLOW__:
        case RESPAWNMARKERGREEN__:
            if (++T1(i) > g_itemRespawnTime)
                KILLIT(i);

            if (T1(i) >= (g_itemRespawnTime >> 1) && T1(i) < ((g_itemRespawnTime >> 1) + (g_itemRespawnTime >> 2)))
                PN(i) = RESPAWNMARKERYELLOW;
            else if (T1(i) > ((g_itemRespawnTime >> 1) + (g_itemRespawnTime >> 2)))
                PN(i) = RESPAWNMARKERGREEN;

            A_Fall(i);
            break;

        case HELECOPT__:
        case DUKECAR__:

            s->z += s->zvel;
            t[0]++;

            if (t[0] == 4) A_PlaySound(WAR_AMBIENCE2,i);

            if (t[0] > (26*8))
            {
                S_PlaySound(RPG_EXPLODE);
                for (j=0;j<32;j++) RANDOMSCRAP(s, i);
                g_earthquakeTime = 16;
                KILLIT(i);
            }
            else if ((t[0]&3) == 0)
                A_Spawn(i,EXPLOSION2);
            A_SetSprite(i,CLIPMASK0);
            break;
        case RAT__:
            A_Fall(i);
            IFMOVING
            {
                if ((krand()&255) < 3) A_PlaySound(RATTY,i);
                s->ang += (krand()&31)-15+(sintable[(t[0]<<8)&2047]>>11);
            }
            else
            {
                T1(i)++;
                if (T1(i) > 1)
                {
                    KILLIT(i);
                }
                else s->ang = (krand()&2047);
            }
            if (s->xvel < 128)
                s->xvel+=2;
            s->ang += (krand()&3)-6;
            break;
        case QUEBALL__:
        case STRIPEBALL__:
            if (s->xvel)
            {
                j = headspritestat[STAT_DEFAULT];
                while (j >= 0)
                {
                    nextj = nextspritestat[j];
                    if (sprite[j].picnum == POCKET && ldist(&sprite[j],s) < 52) KILLIT(i);
                    j = nextj;
                }

                j = clipmove_old(&s->x,&s->y,&s->z,&s->sectnum,
                             (((s->xvel*(sintable[(s->ang+512)&2047]))>>14)*TICSPERFRAME)<<11,
                             (((s->xvel*(sintable[s->ang&2047]))>>14)*TICSPERFRAME)<<11,
                             24L,(4<<8),(4<<8),CLIPMASK1);

                if (j&49152)
                {
                    if ((j&49152) == 32768)
                    {
                        j &= (MAXWALLS-1);
                        k = getangle(
                                wall[wall[j].point2].x-wall[j].x,
                                wall[wall[j].point2].y-wall[j].y);
                        s->ang = ((k<<1) - s->ang)&2047;
                    }
                    else if ((j&49152) == 49152)
                    {
                        j &= (MAXSPRITES-1);
                        A_DamageObject(i,j);
                    }
                }
                s->xvel --;
                if (s->xvel < 0) s->xvel = 0;
                if (s->picnum == STRIPEBALL)
                {
                    s->cstat = 257;
                    s->cstat |= 4&s->xvel;
                    s->cstat |= 8&s->xvel;
                }
            }
            else
            {
                p = A_FindPlayer(s,&x);

                if (x < 1596)
                {

                    //                        if(s->pal == 12)
                    {
                        j = G_GetAngleDelta(fix16_to_int(g_player[p].ps->q16ang),getangle(s->x-g_player[p].ps->pos.x,s->y-g_player[p].ps->pos.y));
                        if (j > -64 && j < 64 && TEST_SYNC_KEY(g_player[p].inputBits->bits, SK_OPEN))
                            if (g_player[p].ps->toggle_key_flag == 1)
                            {
                                a = headspritestat[STAT_ACTOR];
                                while (a >= 0)
                                {
                                    if (sprite[a].picnum == QUEBALL || sprite[a].picnum == STRIPEBALL)
                                    {
                                        j = G_GetAngleDelta(fix16_to_int(g_player[p].ps->q16ang),getangle(sprite[a].x-g_player[p].ps->pos.x,sprite[a].y-g_player[p].ps->pos.y));
                                        if (j > -64 && j < 64)
                                        {
                                            A_FindPlayer(&sprite[a],&l);
                                            if (x > l) break;
                                        }
                                    }
                                    a = nextspritestat[a];
                                }
                                if (a == -1)
                                {
                                    if (s->pal == 12)
                                        s->xvel = 164;
                                    else s->xvel = 140;
                                    s->ang = fix16_to_int(g_player[p].ps->q16ang);
                                    g_player[p].ps->toggle_key_flag = 2;
                                }
                            }
                    }
                }
                if (x < 512 && s->sectnum == g_player[p].ps->cursectnum)
                {
                    s->ang = getangle(s->x-g_player[p].ps->pos.x,s->y-g_player[p].ps->pos.y);
                    s->xvel = 48;
                }
            }

            break;
        case FORCESPHERE__:

            if (s->yvel == 0)
            {
                s->yvel = 1;

                for (l=512;l<(2048-512);l+= 128)
                    for (j=0;j<2048;j += 128)
                    {
                        k = A_Spawn(i,FORCESPHERE);
                        sprite[k].cstat = 257+128;
                        sprite[k].clipdist = 64;
                        sprite[k].ang = j;
                        sprite[k].zvel = sintable[l&2047]>>5;
                        sprite[k].xvel = sintable[(l+512)&2047]>>9;
                        sprite[k].owner = i;
                    }
            }

            if (t[3] > 0)
            {
                if (s->zvel < ACTOR_MAXFALLINGZVEL)
                    s->zvel += 192;
                s->z += s->zvel;
                if (s->z > sector[sect].floorz)
                    s->z = sector[sect].floorz;
                t[3]--;
                if (t[3] == 0)
                    KILLIT(i);
            }
            else if (t[2] > 10)
            {
                j = headspritestat[STAT_MISC];
                while (j >= 0)
                {
                    if (sprite[j].owner == i && sprite[j].picnum == FORCESPHERE)
                        actor[j].t_data[1] = 1+(krand()&63);
                    j = nextspritestat[j];
                }
                t[3] = 64;
            }

            goto BOLT;

        case RECON__:

            A_GetZLimits(i);

            if (sector[s->sectnum].ceilingstat&1)
                s->shade += (sector[s->sectnum].ceilingshade-s->shade)>>1;
            else s->shade += (sector[s->sectnum].floorshade-s->shade)>>1;

            if (s->z < sector[sect].ceilingz+(32<<8))
                s->z = sector[sect].ceilingz+(32<<8);

            if (ud.multimode < 2)
            {
                if (g_noEnemies == 1)
                {
                    s->cstat = (uint16_t)32768;
                    goto BOLT;
                }
                else if (g_noEnemies == 2) s->cstat = 257;
            }
            IFHIT(i)
            {
                if (s->extra < 0 && t[0] != -1)
                {
                    t[0] = -1;
                    s->extra = 0;
                }
                A_PlaySound(RECO_PAIN,i);
                RANDOMSCRAP(s, i);
            }

            if (t[0] == -1)
            {
                s->z += 1024;
                t[2]++;
                if ((t[2]&3) == 0) A_Spawn(i,EXPLOSION2);
                A_GetZLimits(i);
                s->ang += 96;
                s->xvel = 128;
                j = A_SetSprite(i,CLIPMASK0);
                if (j != 1 || s->z > actor[i].floorz)
                {
                    for (l=0;l<16;l++)
                        RANDOMSCRAP(s, i);
                    A_PlaySound(LASERTRIP_EXPLODE,i);
                    A_Spawn(i,PIGCOP);
                    G_AddMonsterKills(i, 1);
                    KILLIT(i);
                }
                goto BOLT;
            }
            else
            {
                if (s->z > actor[i].floorz-(48<<8))
                    s->z = actor[i].floorz-(48<<8);
            }

            p = A_FindPlayer(s,&x);
            j = s->owner;

            // 3 = findplayerz, 4 = shoot

            if (t[0] >= 4)
            {
                t[2]++;
                if ((t[2]&15) == 0)
                {
                    a = s->ang;
                    s->ang = actor[i].tempang;
                    A_PlaySound(RECO_ATTACK,i);
                    A_Shoot(i,FIRELASER);
                    s->ang = a;
                }
                if (t[2] > (26*3) || !cansee(s->x,s->y,s->z-(16<<8),s->sectnum, g_player[p].ps->pos.x,g_player[p].ps->pos.y,g_player[p].ps->pos.z,g_player[p].ps->cursectnum))
                {
                    t[0] = 0;
                    t[2] = 0;
                }
                else actor[i].tempang +=
                        G_GetAngleDelta(actor[i].tempang,getangle(g_player[p].ps->pos.x-s->x,g_player[p].ps->pos.y-s->y))/3;
            }
            else if (t[0] == 2 || t[0] == 3)
            {
                t[3] = 0;
                if (s->xvel > 0) s->xvel -= 16;
                else s->xvel = 0;

                if (t[0] == 2)
                {
                    l = g_player[p].ps->pos.z-s->z;
                    if (klabs(l) < (48<<8)) t[0] = 3;
                    else s->z += ksgn(g_player[p].ps->pos.z-s->z)<<10;
                }
                else
                {
                    t[2]++;
                    if (t[2] > (26*3) || !cansee(s->x,s->y,s->z-(16<<8),s->sectnum, g_player[p].ps->pos.x,g_player[p].ps->pos.y,g_player[p].ps->pos.z,g_player[p].ps->cursectnum))
                    {
                        t[0] = 1;
                        t[2] = 0;
                    }
                    else if ((t[2]&15) == 0)
                    {
                        A_PlaySound(RECO_ATTACK,i);
                        A_Shoot(i,FIRELASER);
                    }
                }
                s->ang += G_GetAngleDelta(s->ang,getangle(g_player[p].ps->pos.x-s->x,g_player[p].ps->pos.y-s->y))>>2;
            }

            if (t[0] != 2 && t[0] != 3)
            {
                l = ldist(&sprite[j],s);
                if (l <= 1524)
                {
                    a = s->ang;
                    s->xvel >>= 1;
                }
                else a = getangle(sprite[j].x-s->x,sprite[j].y-s->y);

                if (t[0] == 1 || t[0] == 4) // Found a locator and going with it
                {
                    l = dist(&sprite[j],s);

                    if (l <= 1524)
                    {
                        if (t[0] == 1) t[0] = 0;
                        else t[0] = 5;
                    }
                    else
                    {
                        // Control speed here
                        if (l > 1524)
                        {
                            if (s->xvel < 256) s->xvel += 32;
                        }
                        else
                        {
                            if (s->xvel > 0) s->xvel -= 16;
                            else s->xvel = 0;
                        }
                    }

                    if (t[0] < 2) t[2]++;

                    if (x < 6144 && t[0] < 2 && t[2] > (26*4))
                    {
                        t[0] = 2+(krand()&2);
                        t[2] = 0;
                        actor[i].tempang = s->ang;
                    }
                }

                if (t[0] == 0 || t[0] == 5)
                {
                    if (t[0] == 0)
                        t[0] = 1;
                    else t[0] = 4;
                    j = s->owner = A_FindLocator(s->hitag,-1);
                    if (j == -1)
                    {
                        s->hitag = j = actor[i].t_data[5];
                        s->owner = A_FindLocator(j,-1);
                        j = s->owner;
                        if (j == -1) KILLIT(i);
                    }
                    else s->hitag++;
                }

                t[3] = G_GetAngleDelta(s->ang,a);
                s->ang += t[3]>>3;

                if ((s->z - sprite[j].z) < -512)
                    s->z += 512;
                else if ((s->z - sprite[j].z) > 512)
                    s->z -= 512;
                else s->z = sprite[j].z;
            }

            if (!A_CheckSoundPlaying(i,RECO_ROAM))
                A_PlaySound(RECO_ROAM,i);

            A_SetSprite(i,CLIPMASK0);

            goto BOLT;

        case OOZ__:
        case OOZ2__:

            A_GetZLimits(i);

            j = (actor[i].floorz-actor[i].ceilingz)>>9;
            if (j > 255) j = 255;

            x = 25-(j>>1);
            if (x < 8) x = 8;
            else if (x > 48) x = 48;

            s->yrepeat = j;
            s->xrepeat = x;
            s->z = actor[i].floorz;

            goto BOLT;

        case GREENSLIME__:
            // #ifndef VOLUMEONE
            if (ud.multimode < 2)
            {
                if (g_noEnemies == 1)
                {
                    s->cstat = (uint16_t)32768;
                    goto BOLT;
                }
                else if (g_noEnemies == 2) s->cstat = 257;
            }
            // #endif

            t[1] += 128;

            if (sector[sect].floorstat & FLOOR_STAT_PLAX)
                KILLIT(i);

            p = A_FindPlayer(s,&x);

            if (x > 20480)
            {
                actor[i].timetosleep++;
                if (actor[i].timetosleep > SLEEPTIME)
                {
                    actor[i].timetosleep = 0;
                    changespritestat(i, STAT_ZOMBIEACTOR);
                    goto BOLT;
                }
            }

            if (t[0] == GREENSLIME_FROZEN) // FROZEN
            {
                t[3]++;
                if (t[3] > 280)
                {
                    s->pal = 0;
                    t[0] = GREENSLIME_ONFLOOR;
                    t[3] = 0;
                    goto BOLT;
                }
                A_Fall(i);
                s->cstat = 257;
                s->picnum = GREENSLIME+2;
                s->extra = 1;
                s->pal = 1;
                IFHIT(i)
                {
                    if (j == FREEZEBLAST) goto BOLT;
                    for (j=16; j >= 0 ;j--)
                    {
                        k = A_InsertSprite(SECT(i),SX(i),SY(i),SZ(i),GLASSPIECES+(j%3),-32,36,36,krand()&2047,32+(krand()&63),1024-(krand()&1023),i,5);
                        sprite[k].pal = 1;
                    }
                    A_PlaySound(GLASS_BREAKING,i);
                    KILLIT(i);
                }
                else if (x < 1024 && g_player[p].ps->quick_kick == 0)
                {
                    j = G_GetAngleDelta(fix16_to_int(g_player[p].ps->q16ang),getangle(SX(i)-g_player[p].ps->pos.x,SY(i)-g_player[p].ps->pos.y));
                    if (j > -128 && j < 128)
                        g_player[p].ps->quick_kick = 14;
                }

                goto BOLT;
            }

            // If we're close, block hitscans but not objects.
            s->cstat = (x < 1596) ? (CSTAT_SPRITE_BLOCK_HITSCAN) : (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN);

            if (t[0] == GREENSLIME_ONPLAYER) //On the player
            {
                s->cstat = 0; // Don't block anything if on player.
                setspritez_old(i,s->x,s->y,s->z);

                s->ang = fix16_to_int(g_player[p].ps->q16ang);

                if ((TEST_SYNC_KEY(g_player[p].inputBits->bits, SK_FIRE) || (g_player[p].ps->quick_kick > 0)) && sprite[g_player[p].ps->i].extra > 0)
                    if (g_player[p].ps->quick_kick > 0 || (g_player[p].ps->curr_weapon != HANDREMOTE_WEAPON && g_player[p].ps->curr_weapon != HANDBOMB_WEAPON && g_player[p].ps->curr_weapon != TRIPBOMB_WEAPON && g_player[p].ps->ammo_amount[g_player[p].ps->curr_weapon] >= 0))
                    {
                        for (x=0;x<8;x++)
                        {
                            j = A_InsertSprite(sect,s->x,s->y,s->z-(8<<8),SCRAP3+(krand()&3),-8,48,48,krand()&2047,(krand()&63)+64,-(krand()&4095)-(s->zvel>>2),i,5);
                            sprite[j].pal = 6;
                        }

                        A_PlaySound(SLIM_DYING,i);
                        A_PlaySound(SQUISHED,i);
                        if ((krand()&255) < 32)
                        {
                            j = A_Spawn(i,BLOODPOOL);
                            sprite[j].pal = 0;
                        }

                        actor[i].htowner = g_player[p].ps->i;
                        G_AddMonsterKills(i, 1);

                        t[0] = GREENSLIME_DEAD;
                        if (g_player[p].ps->somethingonplayer == i)
                            g_player[p].ps->somethingonplayer = -1;
                        KILLIT(i);
                    }

                s->x = g_player[p].ps->pos.x + (sintable[(fix16_to_int(g_player[p].ps->q16ang) + 512) & 2047] >> 7);
                s->y = g_player[p].ps->pos.y + (sintable[fix16_to_int(g_player[p].ps->q16ang) & 2047] >> 7);
                s->z = g_player[p].ps->pos.z + g_player[p].ps->pyoff - t[2] + (8 << 8);
                s->z += fix16_to_int(F16(100) - g_player[p].ps->q16horiz) << 4;

                if (t[2] > 512)
                    t[2] -= 128;

                if (t[2] < 348)
                    t[2] += 128;

                if (g_player[p].ps->newowner >= 0) // Looking through viewscreen
                    G_ClearCameraView(g_player[p].ps);

                if (t[3]>0)
                {
                    static char slimeFrames[] = {5,5,6,6,7,7,6,5};

                    s->picnum = GREENSLIME+slimeFrames[t[3]&7];

                    if (t[3] == 5)
                    {
                        sprite[g_player[p].ps->i].extra += -(5+(krand()&3));
                        A_PlaySound(SLIM_ATTACK,i);
                    }

                    if (t[3] < 7) t[3]++;
                    else t[3] = 0;

                }
                else
                {
                    s->picnum = GREENSLIME+5;
                    if (rnd(32))
                        t[3] = 1;
                }

                if (sprite[g_player[p].ps->i].extra < 1 && g_player[p].ps->somethingonplayer == i)
                {
                    g_player[p].ps->somethingonplayer = -1;
                    t[0] = GREENSLIME_TOFLOOR;
                    goto BOLT;
                }

                s->xrepeat = 20+(sintable[t[1]&2047]>>13);
                s->yrepeat = 15+(sintable[t[1]&2047]>>13);

                goto BOLT;
            }
            else if (s->xvel < 64 && x < 768)
            {
                if (g_player[p].ps->somethingonplayer == -1)
                {
                    g_player[p].ps->somethingonplayer = i;
                    if (t[0] == GREENSLIME_TOFLOOR || t[0] == GREENSLIME_ONCEILING) //Falling downward
                        t[2] = (12<<8);
                    else t[2] = -(13<<8); //Climbing up duke
                    t[0] = GREENSLIME_ONPLAYER;
                }
            }

            IFHIT(i)
            {
                A_PlaySound(SLIM_DYING,i);
                G_AddMonsterKills(i, 1);

                if (g_player[p].ps->somethingonplayer == i)
                    g_player[p].ps->somethingonplayer = -1;

                if (j == FREEZEBLAST)
                {
                    A_PlaySound(SOMETHINGFROZE,i);
                    t[0] = GREENSLIME_FROZEN;
                    t[3] = 0 ;
                    goto BOLT;
                }

                if ((krand()&255) < 32)
                {
                    j = A_Spawn(i,BLOODPOOL);
                    sprite[j].pal = 0;
                }

                for (x=0;x<8;x++)
                {
                    j = A_InsertSprite(sect,s->x,s->y,s->z-(8<<8),SCRAP3+(krand()&3),-8,48,48,krand()&2047,(krand()&63)+64,-(krand()&4095)-(s->zvel>>2),i,5);
                    sprite[j].pal = 6;
                }
                t[0] = GREENSLIME_DEAD;
                KILLIT(i);
            }
            // All weap
            if (t[0] == GREENSLIME_DONEEATING) //Shrinking down
            {
                A_Fall(i);

                s->cstat &= 65535-8;
                s->picnum = GREENSLIME+4;

                //                    if(s->yrepeat > 62)
                //                      A_DoGuts(s,JIBS6,5,myconnectindex);

                if (s->xrepeat > 32) s->xrepeat -= krand()&7;
                if (s->yrepeat > 16) s->yrepeat -= krand()&7;
                else
                {
                    s->xrepeat = 40;
                    s->yrepeat = 16;
                    t[5] = -1;
                    t[0] = GREENSLIME_ONFLOOR;
                }

                goto BOLT;
            }
            else if (t[0] != GREENSLIME_EATINGACTOR) A_GetZLimits(i);

            if (t[0] == GREENSLIME_EATINGACTOR) //On top of somebody
            {
                A_Fall(i);
                sprite[t[5]].xvel = 0;

                l = sprite[t[5]].ang;

                s->z = sprite[t[5]].z;
                s->x = sprite[t[5]].x+(sintable[(l+512)&2047]>>11);
                s->y = sprite[t[5]].y+(sintable[l&2047]>>11);

                s->picnum =  GREENSLIME+2+(g_globalRandom&1);

                if (s->yrepeat < 64) s->yrepeat+=2;
                else
                {
                    if (s->xrepeat < 32) s->xrepeat += 4;
                    else
                    {
                        t[0] = GREENSLIME_DONEEATING;
                        x = ldist(s,&sprite[t[5]]);
                        if (x < 768)
                        {
                            sprite[t[5]].xrepeat = 0;

                            // JBF 20041129: a slimer eating another enemy really ought
                            // to decrease the maximum kill count by one.

                            // [JM] Wouldn't it be a better idea to just fucking add them as killed like everything else?
                            if (sprite[t[5]].extra > 0)
                                ud.total_monsters--;
                        }
                    }
                }

                goto BOLT;
            }

            //Check randomly to see of there is an actor near
            if (rnd(32))
            {
                j = headspritesect[sect];
                while (j>=0)
                {
                    if (A_CheckSpriteFlags(j, SFLAG_GREENSLIMEFOOD))
                    {
                        if (ldist(s,&sprite[j]) < 768 && (klabs(s->z-sprite[j].z)<8192))   //Gulp them
                        {
                            t[5] = j;
                            t[0] = GREENSLIME_EATINGACTOR;
                            t[1] = 0;
                            goto BOLT;
                        }
                    }

                    j = nextspritesect[j];
                }
            }

            //Moving on the ground or ceiling

            if (t[0] == GREENSLIME_ONFLOOR || t[0] == GREENSLIME_ONCEILING)
            {
                s->picnum = GREENSLIME;

                if ((krand()&511) == 0)
                    A_PlaySound(SLIM_ROAM,i);

                if (t[0]==GREENSLIME_ONCEILING)
                {
                    s->zvel = 0;
                    s->cstat &= (65535-8);

                    if ((sector[sect].ceilingstat&1) || (actor[i].ceilingz+6144) < s->z)
                    {
                        s->z += 2048;
                        t[0] = GREENSLIME_TOFLOOR;
                        goto BOLT;
                    }
                }
                else
                {
                    s->cstat |= 8;
                    A_Fall(i);
                }

                if (everyothertime&1) A_SetSprite(i,CLIPMASK0);

                if (s->xvel > 96)
                {
                    s->xvel -= 2;
                    goto BOLT;
                }
                else
                {
                    if (s->xvel < 32) s->xvel += 4;
                    s->xvel = 64 - (sintable[(t[1]+512)&2047]>>9);

                    s->ang += G_GetAngleDelta(s->ang,
                                              getangle(g_player[p].ps->pos.x-s->x,g_player[p].ps->pos.y-s->y))>>3;
                    // TJR
                }

                s->xrepeat = 36 + (sintable[(t[1]+512)&2047]>>11);
                s->yrepeat = 16 + (sintable[t[1]&2047]>>13);

                if (rnd(4) && (sector[sect].ceilingstat&1) == 0 &&
                        klabs(actor[i].floorz-actor[i].ceilingz)
                        < (192<<8))
                {
                    s->zvel = 0;
                    t[0]++;
                }

            }

            if (t[0]==GREENSLIME_TOCEILING)
            {
                s->picnum = GREENSLIME;
                if (s->yrepeat < 40) s->yrepeat+=8;
                if (s->xrepeat > 8) s->xrepeat-=4;
                if (s->zvel > -(2048+1024))
                    s->zvel -= 348;
                s->z += s->zvel;
                if (s->z < actor[i].ceilingz+4096)
                {
                    s->z = actor[i].ceilingz+4096;
                    s->xvel = 0;
                    t[0] = GREENSLIME_ONCEILING;
                }
            }

            if (t[0]==GREENSLIME_TOFLOOR)
            {
                s->picnum = GREENSLIME+1;

                A_Fall(i);

                if (s->z > actor[i].floorz-(8<<8))
                {
                    s->yrepeat-=4;
                    s->xrepeat+=2;
                }
                else
                {
                    if (s->yrepeat < (40-4)) s->yrepeat+=8;
                    if (s->xrepeat > 8) s->xrepeat-=4;
                }

                if (s->z > actor[i].floorz-2048)
                {
                    s->z = actor[i].floorz-2048;
                    t[0] = GREENSLIME_ONFLOOR;
                    s->xvel = 0;
                }
            }
            goto BOLT;

        case BOUNCEMINE__:
        case MORTER__:
            j = A_Spawn(i,(PLUTOPAK?FRAMEEFFECT1:FRAMEEFFECT1_13));
            actor[j].t_data[0] = 3;
            fallthrough__;

        case HEAVYHBOMB__:
        {
            if ((s->cstat & 32768))
            {
                t[PIPEBOMB_RESPAWNTIME]--;
                if (t[PIPEBOMB_RESPAWNTIME] <= 0)
                {
                    A_PlaySound(TELEPORTER, i);
                    A_Spawn(i, TRANSPORTERSTAR);

                    t[PIPEBOMB_WASHIT] = 0;
                    t[PIPEBOMB_EXPLOSIONTIMER] = 0;
                    t[PIPEBOMB_INWATER] = 0;
                    t[PIPEBOMB_FUSETYPE] = 0;
                    t[PIPEBOMB_FUSETIME] = 0;

                    l = -1;
                    s->xvel = 0;
                    s->yvel = 4;
                    s->zvel = 0;
                    s->extra = G_DefaultActorHealthForTile(s->picnum);
                    s->cstat = 257;

                    actor[i].htextra = -1;
                    actor[i].htowner = i;
                    actor[i].htpicnum = s->picnum;
                }
                goto BOLT;
            }

            p = A_FindPlayer(s, &x);

            if (x < 1220) s->cstat &= ~257;
            else s->cstat |= 257;

            if (t[PIPEBOMB_WASHIT] == 0)
            {
                t[PIPEBOMB_ORIGINALOWNER] = -1;
                int temp_owner = s->owner;
                j = A_IncurDamage(i);
                if (j >= 0)
                {
                    t[PIPEBOMB_WASHIT] = 1;
                    t[PIPEBOMB_EXPLOSIONTIMER] = 0;
                    t[PIPEBOMB_ORIGINALOWNER] = temp_owner;

                    // [JM] HACK: I'd gladly use t_data here, but there's not enough space.
                    // Time to pull a Todd and use something that's otherwise completely unused for this actor.
                    // I don't like this, but it'll do. If anyone is willing to suggest a better way, I'm all ears.
                    actor[i].lastvx = s->x;
                    actor[i].lastvy = s->y;
                    t[PIPEBOMB_INITIALZ] = s->z;
                    t[PIPEBOMB_INITIALSECT] = s->sectnum;

                    l = 0;
                    s->xvel = 0;
                    goto DETONATEB;
                }
            }

            if (s->picnum != BOUNCEMINE)
            {
                A_Fall(i);

                if ((sector[sect].lotag != ST_1_ABOVE_WATER || actor[i].floorz != sector[sect].floorz) && s->z >= actor[i].floorz - (ACTOR_FLOOR_OFFSET) && s->yvel < 3)
                {
                    if (s->yvel > 0 || (s->yvel == 0 && actor[i].floorz == sector[sect].floorz))
                        A_PlaySound(PIPEBOMB_BOUNCE,i);
                    s->zvel = -((4-s->yvel)<<8);
                    if (sector[s->sectnum].lotag== 2)
                        s->zvel >>= 2;
                    s->yvel++;
                }
                if (s->z < actor[i].ceilingz)   // && sector[sect].lotag != 2 )
                {
                    s->z = actor[i].ceilingz+(3<<8);
                    s->zvel = 0;
                }
            }

            j = A_MoveSprite(i,
                             (s->xvel*(sintable[(s->ang+512)&2047]))>>14,
                             (s->xvel*(sintable[s->ang&2047]))>>14,
                             s->zvel,CLIPMASK0);

            actor[i].movflag = j;

            if (sector[SECT(i)].lotag == ST_1_ABOVE_WATER && s->zvel == 0 && actor[i].floorz == sector[sect].floorz)
            {
                s->z += (32<<8);
                if (t[PIPEBOMB_INWATER] == 0)
                {
                    t[PIPEBOMB_INWATER] = 1;
                    A_Spawn(i,WATERSPLASH2);
                }
            }
            else t[PIPEBOMB_INWATER] = 0;

            if (t[PIPEBOMB_WASHIT] == 0 && (s->picnum == BOUNCEMINE || s->picnum == MORTER) && (j || x < 844))
            {
                t[PIPEBOMB_WASHIT] = 1;
                t[PIPEBOMB_EXPLOSIONTIMER] = 0;
                l = 0;
                s->xvel = 0;
                goto DETONATEB;
            }

            if (sprite[s->owner].picnum == APLAYER)
                l = sprite[s->owner].yvel;
            else l = -1;

            if (s->xvel > 0)
            {
                s->xvel -= 5;
                if (sector[sect].lotag == ST_2_UNDERWATER)
                    s->xvel -= 10;

                if (s->xvel < 0)
                    s->xvel = 0;
                if (s->xvel&8) s->cstat ^= 4;
            }

            if ((j&49152) == 32768)
            {
                j &= (MAXWALLS-1);

                A_DamageWall(i,j,s->xyz,s->picnum);

                k = getangle(
                        wall[wall[j].point2].x-wall[j].x,
                        wall[wall[j].point2].y-wall[j].y);

                s->ang = ((k<<1) - s->ang)&2047;
                s->xvel >>= 1;
            }

DETONATEB:

            if (s->picnum == HEAVYHBOMB && t[PIPEBOMB_FUSETYPE] == FUSE_TIMED)
            {
                if (t[PIPEBOMB_FUSETIME] > 0)
                    t[PIPEBOMB_FUSETIME]--;

                if (t[PIPEBOMB_FUSETIME] == 0)
                    t[PIPEBOMB_FUSETYPE] = FUSE_EXPLODE;
            }

            if ((l >= 0 && g_player[l].ps->hbomb_on == 0 && t[PIPEBOMB_FUSETYPE] == FUSE_REMOTE) || t[PIPEBOMB_WASHIT] == 1)
                t[PIPEBOMB_FUSETYPE] = FUSE_EXPLODE;

            if (t[PIPEBOMB_FUSETYPE] == FUSE_EXPLODE)
            {
                t[PIPEBOMB_EXPLOSIONTIMER]++;

                if (t[PIPEBOMB_EXPLOSIONTIMER] == 2)
                {
                    x = s->extra;
                    m = 0;
                    switch (tileGetMapping(s->picnum))
                    {
                    case HEAVYHBOMB__:
                        m = g_pipebombBlastRadius;
                        break;
                    case MORTER__:
                        m = g_morterBlastRadius;
                        break;
                    case BOUNCEMINE__:
                        m = g_bouncemineBlastRadius;
                        break;
                    }

                    A_RadiusDamage(i, m,x>>2,x>>1,x-(x>>2),x);
                    A_Spawn(i,EXPLOSION2);
                    if (s->zvel == 0)
                        A_Spawn(i,EXPLOSION2BOT);
                    A_PlaySound(PIPEBOMB_EXPLODE,i);
                    for (x=0;x<8;x++)
                        RANDOMSCRAP(s, i);

                    if (t[PIPEBOMB_ORIGINALOWNER] > -1)
                        s->owner = t[PIPEBOMB_ORIGINALOWNER];
                }

                if (s->yrepeat)
                {
                    s->yrepeat = 0;
                    goto BOLT;
                }

                if (t[PIPEBOMB_EXPLOSIONTIMER] > 20)
                {
                    if (s->owner != i || !DMFLAGS_TEST(DMFLAG_RESPAWNITEMS) || s->picnum == MORTER || s->picnum == BOUNCEMINE)
                    {
                        KILLIT(i);
                    }
                    else
                    {
                        if (t[PIPEBOMB_WASHIT] == 1)
                        {
                            s->x = actor[i].lastvx;
                            s->y = actor[i].lastvy;
                            s->z = t[PIPEBOMB_INITIALZ];
                            changespritesect(i, t[PIPEBOMB_INITIALSECT]);
                        }

                        t[PIPEBOMB_RESPAWNTIME] = g_itemRespawnTime;
                        A_Spawn(i, RESPAWNMARKERRED);
                        s->cstat = (uint16_t)32768;
                        s->yrepeat = 9;
                        goto BOLT;
                    }
                }
            }
            else if (s->picnum == HEAVYHBOMB && x < 788 && t[PIPEBOMB_PICKUPTIMER] > 7 && s->xvel == 0)
            {
                if (cansee(s->x, s->y, s->z - (8 << 8), s->sectnum, g_player[p].ps->pos.x, g_player[p].ps->pos.y, g_player[p].ps->pos.z, g_player[p].ps->cursectnum))
                {
                    if (g_player[p].ps->ammo_amount[HANDBOMB_WEAPON] < g_player[p].ps->max_ammo_amount[HANDBOMB_WEAPON])
                    {
                        P_AddAmmo(HANDBOMB_WEAPON, g_player[p].ps, 1);
                        A_PlaySound(DUKE_GET, g_player[p].ps->i);

                        if (g_player[p].ps->gotweapon[HANDBOMB_WEAPON] == 0 || s->owner == g_player[p].ps->i)
                        {
                            P_AddWeapon(g_player[p].ps, HANDBOMB_WEAPON, (!(g_player[p].ps->weaponswitch & 1) && *aplWeaponWorksLike[g_player[p].ps->curr_weapon] != HANDREMOTE_WEAPON));
                        }

                        if (sprite[s->owner].picnum != APLAYER)
                        {
                            P_PalFrom(g_player[p].ps, 32, 0, 32, 0);
                        }

                        if (s->owner != i || !DMFLAGS_TEST(DMFLAG_RESPAWNITEMS))
                        {
                            KILLIT(i);
                        }
                        else
                        {
                            t[PIPEBOMB_RESPAWNTIME] = g_itemRespawnTime;
                            A_Spawn(i, RESPAWNMARKERRED);
                            s->cstat = (uint16_t)32768;
                        }
                    }
                }
            }

            if (t[PIPEBOMB_PICKUPTIMER] < 8) t[PIPEBOMB_PICKUPTIMER]++;
            goto BOLT;
        }
        case REACTORBURNT__:
        case REACTOR2BURNT__:
            goto BOLT;

        case REACTOR__:
        case REACTOR2__:

            if (t[4] == 1)
            {
                j = headspritesect[sect];
                while (j >= 0)
                {
                    switch (tileGetMapping(sprite[j].picnum))
                    {
                    case SECTOREFFECTOR__:
                        if (sprite[j].lotag == SE_1_PIVOT)
                        {
                            sprite[j].lotag = (int16_t) 65535;
                            sprite[j].hitag = (int16_t) 65535;
                        }
                        break;
                    case REACTOR__:
                        sprite[j].picnum = REACTORBURNT;
                        break;
                    case REACTOR2__:
                        sprite[j].picnum = REACTOR2BURNT;
                        break;
                    case REACTORSPARK__:
                    case REACTOR2SPARK__:
                        sprite[j].cstat = (uint16_t) 32768;
                        break;
                    }
                    j = nextspritesect[j];
                }
                goto BOLT;
            }

            if (t[1] >= 20)
            {
                t[4] = 1;
                goto BOLT;
            }

            p = A_FindPlayer(s,&x);

            t[2]++;
            if (t[2] == 4) t[2]=0;

            if (x < 4096)
            {
                if ((krand()&255) < 16)
                {
                    if (!A_CheckSoundPlaying(g_player[p].ps->i, DUKE_LONGTERM_PAIN))
                        A_PlaySound(DUKE_LONGTERM_PAIN,g_player[p].ps->i);

                    A_PlaySound(SHORT_CIRCUIT,i);

                    sprite[g_player[p].ps->i].extra --;
                    P_PalFrom(g_player[p].ps, 32, 32, 0, 0);
                }
                t[0] += 128;
                if (t[3] == 0)
                    t[3] = 1;
            }
            else t[3] = 0;

            if (t[1])
            {
                t[1]++;

                t[4] = s->z;
                s->z = sector[sect].floorz-(krand()%(sector[sect].floorz-sector[sect].ceilingz));

                switch (t[1])
                {
                case 3:
                    //Turn on all of those flashing sectoreffector.
                    A_RadiusDamage(i, 4096,
                                   g_impactDamage<<2,
                                   g_impactDamage<<2,
                                   g_impactDamage<<2,
                                   g_impactDamage<<2);

                    j = headspritestat[STAT_STANDABLE];
                    while (j >= 0)
                    {
                        if (sprite[j].picnum == MASTERSWITCH)
                            if (sprite[j].hitag == s->hitag)
                                if (sprite[j].yvel == 0)
                                    sprite[j].yvel = 1;
                        j = nextspritestat[j];
                    }
                    break;

                case 4:
                case 7:
                case 10:
                case 15:
                    j = headspritesect[sect];
                    while (j >= 0)
                    {
                        l = nextspritesect[j];

                        if (j != i)
                        {
                            A_DeleteSprite(j);
                            break;
                        }
                        j = l;
                    }
                    break;
                }
                for (x=0;x<16;x++)
                    RANDOMSCRAP(s, i);

                s->z = t[4];
                t[4] = 0;

            }
            else
            {
                IFHIT(i)
                {
                    for (x=0;x<32;x++)
                        RANDOMSCRAP(s, i);
                    if (s->extra < 0)
                        t[1] = 1;
                }
            }
            goto BOLT;

        case CAMERA1__:

            if (t[0] == 0)
            {
                t[1]+=8;
                if (g_damageCameras)
                {
                    IFHIT(i)
                    {
                        t[0] = 1; // static
                        s->cstat = (uint16_t)32768;
                        for (x=0;x<5;x++) RANDOMSCRAP(s, i);
                        goto BOLT;
                    }
                }

                if (s->hitag > 0)
                {
                    if (t[1]<s->hitag)
                        s->ang+=8;
                    else if (t[1]<(s->hitag*3))
                        s->ang-=8;
                    else if (t[1] < (s->hitag<<2))
                        s->ang+=8;
                    else
                    {
                        t[1]=8;
                        s->ang+=16;
                    }
                }
            }
            goto BOLT;
        }

        if (ud.multimode < 2 && A_CheckEnemySprite(s))
        {
            if (g_noEnemies == 1)
            {
                s->cstat = (uint16_t)32768;
                goto BOLT;
            }
            else if (g_noEnemies == 2)
            {
                s->cstat = 0;
                if (s->extra)
                    s->cstat = 257;
            }
        }

        if (!G_TileHasActor(sprite[i].picnum))
            goto BOLT;
        p = A_FindPlayer(s,&x);
        A_Execute(i,p,x);
BOLT:
        i = nexti;
    }

}

static void G_MoveMisc(void)  // STATNUM 5
{
    short i, j, nexti, sect, p;
    int l, x;
    int32_t *t;
    spritetype *s;
    int switchpicnum;

    i = headspritestat[STAT_MISC];
    while (i >= 0)
    {
        nexti = nextspritestat[i];

        t = &actor[i].t_data[0];
        s = &sprite[i];
        sect = s->sectnum;

        if ((unsigned)sect >= MAXSECTORS || s->xrepeat == 0) KILLIT(i);

        switchpicnum = s->picnum;
        if ((s->picnum > NUKEBUTTON)&&(s->picnum <= NUKEBUTTON+3))
        {
            switchpicnum = NUKEBUTTON;
        }
        if ((s->picnum > GLASSPIECES)&&(s->picnum <= GLASSPIECES+2))
        {
            switchpicnum = GLASSPIECES;
        }
        if (s->picnum ==INNERJAW+1)
        {
            switchpicnum--;
        }
        if ((s->picnum == MONEY+1) || (s->picnum == MAIL+1) || (s->picnum == PAPER+1))
            actor[i].floorz = s->z = yax_getflorzofslope(s->sectnum,s->xy);
        else switch (tileGetMapping(switchpicnum))
            {
            case NEON1__:
            case NEON2__:
            case NEON3__:
            case NEON4__:
            case NEON5__:
            case NEON6__:

                if ((g_globalRandom/(s->lotag+1)&31) > 4) s->shade = -127;
                else s->shade = 127;
                goto BOLT;

            case BLOODSPLAT1__:
            case BLOODSPLAT2__:
            case BLOODSPLAT3__:
            case BLOODSPLAT4__:

                if (t[0] == 7*26) goto BOLT;
                s->z += 16+(krand()&15);
                t[0]++;
                if ((t[0]%9) == 0) s->yrepeat++;
                goto BOLT;

            case NUKEBUTTON__:
                //        case NUKEBUTTON+1:
                //        case NUKEBUTTON+2:
                //        case NUKEBUTTON+3:

                if (t[0])
                {
                    t[0]++;
                    if (t[0] == 8) s->picnum = NUKEBUTTON+1;
                    else if (t[0] == 16)
                    {
                        s->picnum = NUKEBUTTON+2;
                        g_player[sprite[s->owner].yvel].ps->fist_incs = 1;
                    }
                    if (g_player[sprite[s->owner].yvel].ps->fist_incs == 26)
                        s->picnum = NUKEBUTTON+3;
                }
                goto BOLT;

            case FORCESPHERE__:

                l = s->xrepeat;
                if (t[1] > 0)
                {
                    t[1]--;
                    if (t[1] == 0)
                    {
                        KILLIT(i);
                    }
                }
                if (actor[s->owner].t_data[1] == 0)
                {
                    if (t[0] < 64)
                    {
                        t[0]++;
                        l += 3;
                    }
                }
                else
                    if (t[0] > 64)
                    {
                        t[0]--;
                        l -= 3;
                    }

                s->x = sprite[s->owner].x;
                s->y = sprite[s->owner].y;
                s->z = sprite[s->owner].z;
                s->ang += actor[s->owner].t_data[0];

                if (l > 64) l = 64;
                else if (l < 1) l = 1;

                s->xrepeat = l;
                s->yrepeat = l;
                s->shade = (l>>1)-48;

                for (j=t[0];j > 0;j--)
                    A_SetSprite(i,CLIPMASK0);
                goto BOLT;
            case WATERSPLASH2__:

                t[0]++;
                if (t[0] == 1)
                {
                    if (sector[sect].lotag != ST_1_ABOVE_WATER && sector[sect].lotag != ST_2_UNDERWATER)
                        KILLIT(i);

                    if (!S_CheckSoundPlaying(ITEM_SPLASH))
                        A_PlaySound(ITEM_SPLASH,i);
                }
                if (t[0] == 3)
                {
                    t[0] = 0;
                    t[1]++;
                }
                if (t[1] == 5)
                    A_DeleteSprite(i);
                goto BOLT;
            case FRAMEEFFECT1_13__:
                if (PLUTOPAK) goto BOLT;	// JBF: ideally this should never happen...
                fallthrough__;
            case FRAMEEFFECT1__:

                if (s->owner >= 0)
                {
                    t[0]++;

                    if (t[0] > 7)
                    {
                        KILLIT(i);
                    }
                    else if (t[0] > 4)
                        s->cstat |= 512+2;
                    else if (t[0] > 2)
                        s->cstat |= 2;
                    s->xoffset = sprite[s->owner].xoffset;
                    s->yoffset = sprite[s->owner].yoffset;
                }
                goto BOLT;
            case INNERJAW__:
                //        case INNERJAW+1:

                p = A_FindPlayer(s,&x);
                if (x < 512)
                {
                    P_PalFrom(g_player[p].ps, 32, 32, 0, 0);
                    sprite[g_player[p].ps->i].extra -= 4;
                }
                fallthrough__;

            case FIRELASER__:
                if (s->extra != 999)
                    s->extra = 999;
                else KILLIT(i);
                break;
            case TONGUE__:
                KILLIT(i);

            case MONEY__:
            case MAIL__:
            case PAPER__:

                s->xvel = (krand()&7)+(sintable[T1(i)&2047]>>9);
                T1(i) += (krand()&63);
                if ((T1(i)&2047) > 512 && (T1(i)&2047) < 1596)
                {
                    if (sector[sect].lotag == 2)
                    {
                        if (s->zvel < 64)
                            s->zvel += (g_spriteGravity>>5)+(krand()&7);
                    }
                    else
                        if (s->zvel < 144)
                            s->zvel += (g_spriteGravity>>5)+(krand()&7);
                }

                A_SetSprite(i,CLIPMASK0);

                if ((krand()&3) == 0)
                    setspritez_old(i,s->x,s->y,s->z);

                if ((unsigned)s->sectnum >= MAXSECTORS) KILLIT(i);
                l = yax_getflorzofslope(s->sectnum,s->xy);

                if (s->z > l)
                {
                    s->z = l;

                    A_AddToDeleteQueue(i);
                    PN(i)++;

                    j = headspritestat[STAT_MISC];
                    while (j >= 0)
                    {
                        if (sprite[j].picnum == BLOODPOOL)
                            if (ldist(s,&sprite[j]) < 348)
                            {
                                s->pal = 2;
                                break;
                            }
                        j = nextspritestat[j];
                    }
                }

                break;

            case JIBS1__:
            case JIBS2__:
            case JIBS3__:
            case JIBS4__:
            case JIBS5__:
            case JIBS6__:
            case HEADJIB1__:
            case ARMJIB1__:
            case LEGJIB1__:
            case LIZMANHEAD1__:
            case LIZMANARM1__:
            case LIZMANLEG1__:
            case DUKETORSO__:
            case DUKEGUN__:
            case DUKELEG__:
            {
                int32_t ceilZ, floorZ;

                if (s->xvel > 0) s->xvel--;
                else s->xvel = 0;

                if (++t[5] == (REALGAMETICSPERSEC * 10))
                    KILLIT(i);

                // Done so we can check sector's real Z's in case we passed over a TROR layer.
                getzsofslope(s->sectnum, s->x, s->y, &ceilZ, &floorZ);

                if ((s->zvel > 1024 && s->zvel < 1280) || s->z > floorZ || s->z < ceilZ)
                {
                    setspritez(i, &s->xyz);
                    sect = s->sectnum;
                }
                
                if ((unsigned)sect >= MAXSECTORS)
                    KILLIT(i);

                // Update Z's for TROR.
                yax_getzsofslope(sect, s->x, s->y, &ceilZ, &floorZ);

                if (ceilZ == floorZ)
                    KILLIT(i);

                if (s->z < floorZ - (2 << 8))
                {
                    if (t[1] < 2) t[1]++;
                    else if (sector[sect].lotag != ST_2_UNDERWATER)
                    {
                        t[1] = 0;
                        if (s->picnum == DUKELEG || s->picnum == DUKETORSO || s->picnum == DUKEGUN)
                        {
                            if (t[0] > 6) t[0] = 0;
                            else t[0]++;
                        }
                        else
                        {
                            if (t[0] > 2)
                                t[0] = 0;
                            else t[0]++;
                        }
                    }

                    if (s->zvel < ACTOR_MAXFALLINGZVEL)
                    {
                        if (sector[sect].lotag == 2)
                        {
                            if (s->zvel < 1024)
                                s->zvel += 48;
                            else s->zvel = 1024;
                        }
                        else s->zvel += g_spriteGravity - 50;
                    }

                    s->x += (s->xvel * sintable[(s->ang + 512) & 2047]) >> 14;
                    s->y += (s->xvel * sintable[s->ang & 2047]) >> 14;
                    s->z += s->zvel;
                }
                else
                {
                    if (t[2] == 0)
                    {
                        if ((unsigned)s->sectnum >= MAXSECTORS)
                        {
                            KILLIT(i);
                        }
#if 0
                        // [JM] This code is a fuck. I suggest removing it.
                        // Causes gibs to not splatter on slopes.
                        if ((sector[s->sectnum].floorstat & FLOOR_STAT_SLOPE))
                        {
                            KILLIT(i);
                        }
#endif
                        t[2]++;
                    }

                    s->z = floorZ - (2 << 8);
                    s->xvel = 0;

                    if (s->picnum == JIBS6)
                    {
                        t[1]++;
                        if ((t[1] & 3) == 0 && t[0] < 7)
                            t[0]++;
                        if (t[1] > 20) KILLIT(i);
                    }
                    else
                    {
                        s->picnum = JIBS6;
                        t[0] = 0;
                        t[1] = 0;
                    }

                }
                goto BOLT;
            }

            case BLOODPOOL__:
            case PUKE__:

                if (t[0] == 0)
                {
                    t[0] = 1;
                    if (sector[sect].floorstat&2)
                    {
                        KILLIT(i);
                    }
                    else A_AddToDeleteQueue(i);
                }

                A_Fall(i);

                p = A_FindPlayer(s,&x);

                s->z = actor[i].floorz-(ACTOR_FLOOR_OFFSET);

                if (t[2] < 32)
                {
                    t[2]++;
                    if (actor[i].htpicnum == TIRE)
                    {
                        if (s->xrepeat < 64 && s->yrepeat < 64)
                        {
                            s->xrepeat += krand()&3;
                            s->yrepeat += krand()&3;
                        }
                    }
                    else
                    {
                        if (s->xrepeat < 32 && s->yrepeat < 32)
                        {
                            s->xrepeat += krand()&3;
                            s->yrepeat += krand()&3;
                        }
                    }
                }

                if (x < 844 && s->xrepeat > 6 && s->yrepeat > 6)
                {
                    if (s->pal == 0 && (krand()&255) < 16 && s->picnum != PUKE)
                    {
                        if (g_player[p].ps->inv_amount[GET_BOOTS] > 0)
                            g_player[p].ps->inv_amount[GET_BOOTS]--;
                        else
                        {
                            if (!A_CheckSoundPlaying(g_player[p].ps->i,DUKE_LONGTERM_PAIN))
                                A_PlaySound(DUKE_LONGTERM_PAIN,g_player[p].ps->i);
                            sprite[g_player[p].ps->i].extra --;
                            P_PalFrom(g_player[p].ps, 32, 16, 0, 0);
                        }
                    }

                    if (t[1] == 1) goto BOLT;
                    t[1] = 1;

                    if (actor[i].htpicnum == TIRE)
                        g_player[p].ps->footprintcount = 10;
                    else g_player[p].ps->footprintcount = 3;

                    g_player[p].ps->footprintpal = s->pal;
                    g_player[p].ps->footprintshade = s->shade;

                    if (t[2] == 32)
                    {
                        s->xrepeat -= 6;
                        s->yrepeat -= 6;
                    }
                }
                else t[1] = 0;
                goto BOLT;

            case BURNING__:
            case BURNING2__:
            case FECES__:
            case WATERBUBBLE__:
            case SMALLSMOKE__:
            case EXPLOSION2__:
            case SHRINKEREXPLOSION__:
            case EXPLOSION2BOT__:
            case BLOOD__:
            case LASERSITE__:
            case FORCERIPPLE__:
            case TRANSPORTERSTAR__:
            case TRANSPORTERBEAM__:
                if (!G_TileHasActor(sprite[i].picnum))
                    goto BOLT;
                p = A_FindPlayer(s,&x);
                A_Execute(i,p,x);
                goto BOLT;

            case SHELL__:
            case SHOTGUNSHELL__:

                A_SetSprite(i,CLIPMASK0);

                if ((unsigned)sect >= MAXSECTORS || (sector[sect].floorz + 256) < s->z) KILLIT(i);

                if (sector[sect].lotag == 2)
                {
                    t[1]++;
                    if (t[1] > 8)
                    {
                        t[1] = 0;
                        t[0]++;
                        t[0] &= 3;
                    }
                    if (s->zvel < 128) s->zvel += (g_spriteGravity/13); // 8
                    else s->zvel -= 64;
                    if (s->xvel > 0)
                        s->xvel -= 4;
                    else s->xvel = 0;
                }
                else
                {
                    t[1]++;
                    if (t[1] > 3)
                    {
                        t[1] = 0;
                        t[0]++;
                        t[0] &= 3;
                    }
                    if (s->zvel < 512) s->zvel += (g_spriteGravity/3); // 52;
                    if (s->xvel > 0)
                        s->xvel --;
                    //                else KILLIT(i);
                }

                goto BOLT;

            case GLASSPIECES__:
                //        case GLASSPIECES+1:
                //        case GLASSPIECES+2:

                A_Fall(i);

                if (s->zvel > 4096) s->zvel = 4096;
                if ((unsigned)sect >= MAXSECTORS) KILLIT(i);

                if (s->z == actor[i].floorz-(ACTOR_FLOOR_OFFSET) && t[0] < 3)
                {
                    s->zvel = -((3-t[0])<<8)-(krand()&511);
                    if (sector[sect].lotag == 2)
                        s->zvel >>= 1;
                    s->xrepeat >>= 1;
                    s->yrepeat >>= 1;
                    if (rnd(96))
                        setspritez_old(i,s->x,s->y,s->z);
                    t[0]++;//Number of bounces
                }
                else if (t[0] == 3) KILLIT(i);

                if (s->xvel > 0)
                {
                    s->xvel -= 2;
                    s->cstat = ((s->xvel&3)<<2);
                }
                else s->xvel = 0;

                A_SetSprite(i,CLIPMASK0);

                goto BOLT;
            }

        IFWITHIN(SCRAP6,SCRAP5+3)
        {
            if (s->xvel > 0)
                s->xvel--;
            else s->xvel = 0;

            if (s->zvel > 1024 && s->zvel < 1280)
            {
                setspritez_old(i,s->x,s->y,s->z);
            }

            if (s->z < sector[s->sectnum].floorz-(2<<8))
            {
                if (t[1] < 1) t[1]++;
                else
                {
                    t[1] = 0;

                    if (s->picnum < SCRAP6+8)
                    {
                        if (t[0] > 6)
                            t[0] = 0;
                        else t[0]++;
                    }
                    else
                    {
                        if (t[0] > 2)
                            t[0] = 0;
                        else t[0]++;
                    }
                }

                if (s->zvel < 4096)
                    s->zvel += g_spriteGravity-50;

                int32_t xchange = (s->xvel * sintable[(s->ang + 512) & 2047]);
                int32_t ychange = (s->xvel * sintable[s->ang & 2047]);

                // Item from blimp. Prevent items from escaping map boundaries.
                if (s->picnum == SCRAP1 && s->yvel > 0)
                {
                    auto retVal = clipmove(&s->xyz, &sect, xchange, ychange, (64 << 2), (4 << 8), (4 << 8), CLIPMASK1);
                    if((retVal & 49152) == 32768 || (retVal & 49152) == 49152)
                        s->xvel = 0;

                    if (sect >= 0)
                        if ((sect != s->sectnum))
                            changespritesect(i, sect);
                }
                else
                {
                    s->x += xchange >> 14;
                    s->y += ychange >> 14;
                }

                s->z += s->zvel;
            }
            else
            {
                if (s->picnum == SCRAP1 && s->yvel > 0)
                {
                    j = A_Spawn(i,s->yvel);
                    setspritez_old(j,s->x,s->y,s->z);
                    A_GetZLimits(j);
                    sprite[j].hitag = sprite[j].lotag = 0;
                }
                KILLIT(i);
            }
            goto BOLT;
        }

BOLT:
        i = nexti;
    }
}

static void HandleSE31(int spriteNum, int setFloorZ, int spriteZ, int SEdir, int zDifference)
{
    const spritetype *pSprite = &sprite[spriteNum];
    sectortype *const pSector = &sector[sprite[spriteNum].sectnum];
    int32_t *const    pData = actor[spriteNum].t_data;
    int j;

    if (klabs(pSector->floorz - spriteZ) < sprite[spriteNum].yvel)
    {
        if (setFloorZ)
            pSector->floorz = spriteZ;

        pData[2] = SEdir;
        pData[0] = 0;
        pData[3] = pSprite->hitag;

        j = headspritesect[pSprite->sectnum];
        while (j >= 0)
        {
            if (sprite[j].zvel == 0 && sprite[j].statnum != 3 && sprite[j].statnum != 4)
            {
                actor[j].bpos.z = sprite[j].z;
                actor[j].floorz = pSector->floorz;
            }
            j = nextspritesect[j];
        }

        A_CallSound(pSprite->sectnum, spriteNum);
    }
    else
    {
        int const zChange = ksgn(zDifference) * sprite[spriteNum].yvel;

        pSector->floorz += zChange;

        j = headspritesect[pSprite->sectnum];
        while (j >= 0)
        {
            if (sprite[j].picnum == APLAYER && sprite[j].owner >= 0)
            {
                int const playerNum = sprite[j].yvel;

                if (g_player[playerNum].ps->on_ground == 1)
                {
                    g_player[playerNum].ps->pos.z += zChange;
                }
            }

            if (sprite[j].zvel == 0 && sprite[j].statnum != STAT_EFFECTOR && sprite[j].statnum != STAT_PROJECTILE)
            {
                actor[j].bpos.z = sprite[j].z;
                sprite[j].z += zChange;
                actor[j].floorz = pSector->floorz;
            }

            j = nextspritesect[j];
        }
    }
}

static void G_MoveEffectors(void)   //STATNUM 3
{
    int q=0,  m, x, st, j;
    int32_t *pData,l;
    int spriteNum = headspritestat[STAT_EFFECTOR], nextk, p, sh, nextj;
    short k;
    walltype *wal;

    fricxv = fricyv = 0;

    while (spriteNum >= 0)
    {
        int const  nextSprite = nextspritestat[spriteNum];
        auto const pSprite = &sprite[spriteNum];
        sectortype* const pSector = &sector[pSprite->sectnum];

        st = pSprite->lotag;
        sh = pSprite->hitag;

        pData = &actor[spriteNum].t_data[0];

        switch (st)
        {
        case SE_0_ROTATING_SECTOR:
        {
            int zchange = 0;

            zchange = 0;

            j = pSprite->owner;

            if ((uint16_t)sprite[j].lotag == UINT16_MAX)
                KILLIT(spriteNum);

            q = pSector->extra>>3;
            l = 0;

            if (pSector->lotag == ST_30_ROTATE_RISE_BRIDGE)
            {
                q >>= 2;

                if (sprite[spriteNum].extra == 1)
                {
                    if (actor[spriteNum].tempang < 256)
                    {
                        actor[spriteNum].tempang += 4;
                        if (actor[spriteNum].tempang >= 256)
                            A_CallSound(pSprite->sectnum, spriteNum);
                        if (pSprite->clipdist) l = 1;
                        else l = -1;
                    }
                    else actor[spriteNum].tempang = 256;

                    if (pSector->floorz > pSprite->z)   //z's are touching
                    {
                        pSector->floorz -= 512;
                        zchange = -512;
                        if (pSector->floorz < pSprite->z)
                            pSector->floorz = pSprite->z;
                    }

                    else if (pSector->floorz < pSprite->z)   //z's are touching
                    {
                        pSector->floorz += 512;
                        zchange = 512;
                        if (pSector->floorz > pSprite->z)
                            pSector->floorz = pSprite->z;
                    }
                }
                else if (sprite[spriteNum].extra == 3)
                {
                    if (actor[spriteNum].tempang > 0)
                    {
                        actor[spriteNum].tempang -= 4;
                        if (actor[spriteNum].tempang <= 0)
                            A_CallSound(pSprite->sectnum, spriteNum);
                        if (pSprite->clipdist) l = -1;
                        else l = 1;
                    }
                    else actor[spriteNum].tempang = 0;

                    if (pSector->floorz > T4(spriteNum))   //z's are touching
                    {
                        pSector->floorz -= 512;
                        zchange = -512;
                        if (pSector->floorz < T4(spriteNum))
                            pSector->floorz = T4(spriteNum);
                    }

                    else if (pSector->floorz < T4(spriteNum))   //z's are touching
                    {
                        pSector->floorz += 512;
                        zchange = 512;
                        if (pSector->floorz > T4(spriteNum))
                            pSector->floorz = T4(spriteNum);
                    }
                }  
            }
            else
            {
                if (actor[j].t_data[0] == 0) break;
                if (actor[j].t_data[0] == 2) KILLIT(spriteNum);

                l = (sprite[j].ang > 1024) ? -1 : 1;

                if (pData[3] == 0)
                    pData[3] = ldist(pSprite,&sprite[j]);
                pSprite->xvel = pData[3];
                pSprite->x = sprite[j].x;
                pSprite->y = sprite[j].y;
            }

            pSprite->ang += (l * q);
            pData[2] += (l * q);

            if (l && (pSector->floorstat&64))
            {
                for (ALL_PLAYERS(p))
                {
                    if (g_player[p].ps->cursectnum == pSprite->sectnum && g_player[p].ps->on_ground == 1)
                    {
                        g_player[p].ps->q16ang += fix16_from_int(l*q);
                        g_player[p].ps->q16ang &= 0x7FFFFFF;

                        g_player[p].ps->pos.z += zchange;

                        vec2_t r;
                        rotatepoint(sprite[j].xy,g_player[p].ps->pos.xy,(q*l),&r);

                        g_player[p].ps->bobposx += r.x-g_player[p].ps->pos.x;
                        g_player[p].ps->bobposy += r.y-g_player[p].ps->pos.y;

                        g_player[p].ps->pos.x = r.x;
                        g_player[p].ps->pos.y = r.y;

                        if (sprite[g_player[p].ps->i].extra <= 0)
                        {
                            sprite[g_player[p].ps->i].x = r.x;
                            sprite[g_player[p].ps->i].y = r.y;
                        }
                    }
                }

                p = headspritesect[pSprite->sectnum];
                while (p >= 0)
                {
                    if (sprite[p].statnum != STAT_EFFECTOR && sprite[p].statnum != STAT_PROJECTILE)
                        if (sprite[p].picnum != LASERLINE)
                        {
                            if (sprite[p].picnum == APLAYER && sprite[p].owner >= 0)
                            {
                                p = nextspritesect[p];
                                continue;
                            }

                            sprite[p].ang += (l*q);
                            sprite[p].ang &= 2047;

                            sprite[p].z += zchange;

                            rotatepoint(sprite[j].xy,sprite[p].xy,(q*l),&sprite[p].xy);
                        }
                    p = nextspritesect[p];
                }

            }

            ms(spriteNum);
        }

        break;
        case SE_1_PIVOT: //Nothing for now used as the pivot
            if (pSprite->owner == -1) //Init
            {
                pSprite->owner = spriteNum;

                j = headspritestat[STAT_EFFECTOR];
                while (j >= 0)
                {
                    if (sprite[j].lotag == 19 && sprite[j].hitag == sh)
                    {
                        pData[0] = 0;
                        break;
                    }
                    j = nextspritestat[j];
                }
            }

            break;
        case SE_6_SUBWAY:
            k = pSector->extra;

            if (pData[4] > 0)
            {
                pData[4]--;
                if (pData[4] >= (k-(k>>3)))
                    pSprite->xvel -= (k>>5);
                if (pData[4] > ((k>>1)-1) && pData[4] < (k-(k>>3)))
                    pSprite->xvel = 0;
                if (pData[4] < (k>>1))
                    pSprite->xvel += (k>>5);
                if (pData[4] < ((k>>1)-(k>>3)))
                {
                    pData[4] = 0;
                    pSprite->xvel = k;
                }
            }
            else pSprite->xvel = k;

            j = headspritestat[STAT_EFFECTOR];
            while (j >= 0)
            {
                if ((sprite[j].lotag == 14) && (sh == sprite[j].hitag) && (actor[j].t_data[0] == pData[0]))
                {
                    sprite[j].xvel = pSprite->xvel;
                    //                        if( t[4] == 1 )
                    {
                        if (actor[j].t_data[5] == 0)
                            actor[j].t_data[5] = dist(&sprite[j], pSprite);
                        x = ksgn(dist(&sprite[j], pSprite)-actor[j].t_data[5]);
                        if (sprite[j].extra)
                            x = -x;
                        pSprite->xvel += x;
                    }
                    actor[j].t_data[4] = pData[4];
                }
                j = nextspritestat[j];
            }
            x = 0;
            fallthrough__;

        case SE_14_SUBWAY_CAR:
            if (pSprite->owner==-1)
                pSprite->owner = A_FindLocator((short)pData[3],(short)pData[0]);

            if (pSprite->owner == -1)
            {
                Bsprintf(tempbuf,"Could not find any locators for SE# 6 and 14 with a hitag of %d.\n", pData[3]);
                G_GameExit(tempbuf);
            }

            j = ldist(&sprite[pSprite->owner], pSprite);

            if (j < 1024L)
            {
                if (st==6)
                    if (sprite[pSprite->owner].hitag&1)
                        pData[4]=pSector->extra; //Slow it down
                pData[3]++;
                pSprite->owner = A_FindLocator(pData[3], pData[0]);
                if (pSprite->owner==-1)
                {
                    pData[3]=0;
                    pSprite->owner = A_FindLocator(0, pData[0]);
                }
            }

            if (pSprite->xvel)
            {
                x = getangle(sprite[pSprite->owner].x-pSprite->x,sprite[pSprite->owner].y-pSprite->y);
                q = G_GetAngleDelta(pSprite->ang,x)>>3;

                pData[2] += q;
                pSprite->ang += q;

                if (pSprite->xvel == pSector->extra)
                {
                    if ((pSector->floorstat&1) == 0 && (pSector->ceilingstat&1) == 0)
                    {
                        if (!A_CheckSoundPlaying(spriteNum,actor[spriteNum].lastvx))
                            A_PlaySound(actor[spriteNum].lastvx, spriteNum);
                    }
                    else if (!DMFLAGS_TEST(DMFLAG_NOMONSTERS) && pSector->floorpal == 0 && (pSector->floorstat & 1) && rnd(8))
                    {
                        p = A_FindPlayer(pSprite,&x);
                        if (x < 20480)
                        {
                            j = pSprite->ang;
                            pSprite->ang = getangle(pSprite->x-g_player[p].ps->pos.x,pSprite->y-g_player[p].ps->pos.y);
                            A_Shoot(spriteNum,RPG);
                            pSprite->ang = j;
                        }
                    }
                }

                if (pSprite->xvel <= 64 && (pSector->floorstat&1) == 0 && (pSector->ceilingstat&1) == 0)
                    S_StopEnvSound(actor[spriteNum].lastvx, spriteNum);

                if ((pSector->floorz-pSector->ceilingz) < (108<<8))
                {
                    if (ud.noclip == 0 && pSprite->xvel >= 192)
                        for (ALL_PLAYERS(p))
                        if (sprite[g_player[p].ps->i].extra > 0)
                        {
                            k = g_player[p].ps->cursectnum;
                            updatesector(g_player[p].ps->pos.x,g_player[p].ps->pos.y,&k);
                            if ((k == -1 && ud.noclip == 0) || (k == pSprite->sectnum && g_player[p].ps->cursectnum != pSprite->sectnum))
                            {
                                g_player[p].ps->pos.x = pSprite->x;
                                g_player[p].ps->pos.y = pSprite->y;
                                g_player[p].ps->cursectnum = pSprite->sectnum;

                                setspritez_old(g_player[p].ps->i,pSprite->x,pSprite->y,pSprite->z);
                                P_QuickKill(g_player[p].ps);
                            }
                        }
                }

                m = (pSprite->xvel*sintable[(pSprite->ang+512)&2047])>>14;
                x = (pSprite->xvel*sintable[pSprite->ang&2047])>>14;

                // Move player spawns with sector
                for (p = 0; p < g_numPlayerSprites; p++)
                {
                    if (g_playerSpawnPoints[p].sectnum == pSprite->sectnum)
                    {
                        g_playerSpawnPoints[p].pos.x += m;
                        g_playerSpawnPoints[p].pos.y += x;
                    }
                }

                for (ALL_PLAYERS(p))
                {
                    if (sector[g_player[p].ps->cursectnum].lotag != ST_2_UNDERWATER)
                    {
                        if (pSprite->sectnum == sprite[g_player[p].ps->i].sectnum)
                        {
                            rotatepoint(pSprite->xy, g_player[p].ps->pos.xy, q, &g_player[p].ps->pos.xy);

                            g_player[p].ps->pos.x += m;
                            g_player[p].ps->pos.y += x;

                            g_player[p].ps->bobposx += m;
                            g_player[p].ps->bobposy += x;

                            g_player[p].ps->q16ang += fix16_from_int(q);

                            if (sprite[g_player[p].ps->i].extra <= 0)
                            {
                                sprite[g_player[p].ps->i].x = g_player[p].ps->pos.x;
                                sprite[g_player[p].ps->i].y = g_player[p].ps->pos.y;
                            }
                        }
                    }
                }

                j = headspritesect[pSprite->sectnum];
                while (j >= 0)
                {
                    if (sprite[j].statnum != STAT_PLAYER && sector[sprite[j].sectnum].lotag != ST_2_UNDERWATER && sprite[j].picnum != SECTOREFFECTOR && sprite[j].picnum != LOCATORS)
                    {
                        rotatepoint(pSprite->xy,sprite[j].xy,q,&sprite[j].xy);

                        sprite[j].x+= m;
                        sprite[j].y+= x;

                        sprite[j].ang+=q;
                    }
                    j = nextspritesect[j];
                }

                ms(spriteNum);
                //setspritez_old(spriteNum,pSprite->x,pSprite->y,pSprite->z); // BREAKS

                if ((pSector->floorz-pSector->ceilingz) < (108<<8))
                {
                    if (ud.noclip == 0 && pSprite->xvel >= 192)
                        for (ALL_PLAYERS(p))
                        if (sprite[g_player[p].ps->i].extra > 0)
                        {
                            k = g_player[p].ps->cursectnum;
                            updatesector(g_player[p].ps->pos.x,g_player[p].ps->pos.y,&k);
                            if ((k == -1 && ud.noclip == 0) || (k == pSprite->sectnum && g_player[p].ps->cursectnum != pSprite->sectnum))
                            {
                                g_player[p].ps->opos.x = g_player[p].ps->pos.x = pSprite->x;
                                g_player[p].ps->opos.y = g_player[p].ps->pos.y = pSprite->y;
                                g_player[p].ps->cursectnum = pSprite->sectnum;

                                setspritez_old(g_player[p].ps->i,pSprite->x,pSprite->y,pSprite->z);
                                P_QuickKill(g_player[p].ps);
                            }
                        }

                    j = headspritesect[sprite[OW(spriteNum)].sectnum];
                    while (j >= 0)
                    {
                        l = nextspritesect[j];
                        if (sprite[j].statnum == 1 && A_CheckEnemySprite(&sprite[j]) && sprite[j].picnum != SECTOREFFECTOR && sprite[j].picnum != LOCATORS)
                        {
                            k = sprite[j].sectnum;
                            updatesector(sprite[j].x,sprite[j].y,&k);
                            if (sprite[j].extra >= 0 && k == pSprite->sectnum)
                            {
                                A_DoGutsDir(j,JIBS6,72);
                                A_PlaySound(SQUISHED, spriteNum);
                                A_DeleteSprite(j);
                            }
                        }
                        j = l;
                    }
                }
            }

            break;

        case SE_30_TWO_WAY_TRAIN:
            if (pSprite->owner == -1)
            {
                pData[3] = !pData[3];
                pSprite->owner = A_FindLocator(pData[3], pData[0]);

                int32_t locDist = ldist(&sprite[pSprite->owner], pSprite);
                if (locDist < (2048 - 128))
                {
                    pData[5] = locDist >> 1;
                    pData[6] = locDist >> 4;
                }
                else
                {
                    pData[5] = 2048;
                    pData[6] = 128;
                }
            }
            else
            {
                if (pData[4] == 1) // Starting to go
                {
                    if (ldist(&sprite[pSprite->owner], pSprite) < pData[5]-pData[6])
                        pData[4] = 2;
                    else
                    {
                        if (pSprite->xvel == 0)
                            G_OperateActivators(pSprite->hitag+(!pData[3]),-1);
                        if (pSprite->xvel < 256)
                            pSprite->xvel += 16;
                    }
                }
                if (pData[4] == 2)
                {
                    l = ldist(&sprite[pSprite->owner], pSprite);

                    if (l <= pData[6])
                        pSprite->xvel = 0;

                    if (pSprite->xvel > 0)
                        pSprite->xvel -= 16;
                    else
                    {
                        pSprite->xvel = 0;
                        G_OperateActivators(pSprite->hitag+(short)pData[3],-1);
                        pSprite->owner = -1;
                        pSprite->ang += 1024;
                        pData[4] = 0;
                        G_OperateForceFields(spriteNum,pSprite->hitag);

                        j = headspritesect[pSprite->sectnum];
                        while (j >= 0)
                        {
                            if (sprite[j].picnum != SECTOREFFECTOR && sprite[j].picnum != LOCATORS)
                            {
                                actor[j].bpos.x = sprite[j].x;
                                actor[j].bpos.y = sprite[j].y;
                            }
                            j = nextspritesect[j];
                        }

                    }
                }
            }

            if (pSprite->xvel)
            {
                l = (pSprite->xvel*sintable[(pSprite->ang+512)&2047])>>14;
                x = (pSprite->xvel*sintable[pSprite->ang&2047])>>14;

                if (!ud.noclip && ((pSector->floorz - pSector->ceilingz) < (108 << 8)))
                {
                    for (ALL_PLAYERS(p))
                    {
                        if (sprite[g_player[p].ps->i].extra > 0)
                        {
                            k = g_player[p].ps->cursectnum;
                            updatesector(g_player[p].ps->pos.x, g_player[p].ps->pos.y, &k);
                            if ((k == -1 && ud.noclip == 0) || (k == pSprite->sectnum && g_player[p].ps->cursectnum != pSprite->sectnum))
                            {
                                g_player[p].ps->pos.x = pSprite->x;
                                g_player[p].ps->pos.y = pSprite->y;
                                g_player[p].ps->cursectnum = pSprite->sectnum;

                                setspritez(g_player[p].ps->i, &pSprite->xyz);
                                P_QuickKill(g_player[p].ps);
                            }
                        }
                    }
                }

                for (ALL_PLAYERS(p))
                {
                    if (sprite[g_player[p].ps->i].sectnum == pSprite->sectnum)
                    {
                        g_player[p].ps->pos.x += l;
                        g_player[p].ps->pos.y += x;

                        g_player[p].ps->bobposx += l;
                        g_player[p].ps->bobposy += x;
                    }
                }

                // Move player spawns with sector
                for (p = 0; p < g_numPlayerSprites; p++)
                {
                    if (g_playerSpawnPoints[p].sectnum == pSprite->sectnum)
                    {
                        g_playerSpawnPoints[p].pos.x += l;
                        g_playerSpawnPoints[p].pos.y += x;
                    }
                }

                j = headspritesect[pSprite->sectnum];
                while (j >= 0)
                {
                    if (sprite[j].picnum != SECTOREFFECTOR && sprite[j].picnum != LOCATORS)
                    {
                        actor[j].bpos.x = sprite[j].x;
                        actor[j].bpos.y = sprite[j].y;

                        sprite[j].x += l;
                        sprite[j].y += x;
                    }
                    j = nextspritesect[j];
                }

                ms(spriteNum);
                //setspritez_old(spriteNum,pSprite->x,pSprite->y,pSprite->z); // BREAKS

                if ((pSector->floorz-pSector->ceilingz) < (108<<8))
                {
                    if (ud.noclip == 0)
                        for (ALL_PLAYERS(p))
                        if (sprite[g_player[p].ps->i].extra > 0)
                        {
                            k = g_player[p].ps->cursectnum;
                            updatesector(g_player[p].ps->pos.x,g_player[p].ps->pos.y,&k);
                            if ((k == -1 && ud.noclip == 0) || (k == pSprite->sectnum && g_player[p].ps->cursectnum != pSprite->sectnum))
                            {
                                g_player[p].ps->pos.x = pSprite->x;
                                g_player[p].ps->pos.y = pSprite->y;

                                g_player[p].ps->opos.x = g_player[p].ps->pos.x;
                                g_player[p].ps->opos.y = g_player[p].ps->pos.y;

                                g_player[p].ps->cursectnum = pSprite->sectnum;

                                setspritez(g_player[p].ps->i, &pSprite->xyz);
                                P_QuickKill(g_player[p].ps);
                            }
                        }

                    j = headspritesect[sprite[OW(spriteNum)].sectnum];
                    while (j >= 0)
                    {
                        l = nextspritesect[j];
                        if (sprite[j].statnum == 1 && A_CheckEnemySprite(&sprite[j]) && sprite[j].picnum != SECTOREFFECTOR && sprite[j].picnum != LOCATORS)
                        {
                            //                    if(sprite[j].sectnum != pSprite->sectnum)
                            {
                                k = sprite[j].sectnum;
                                updatesector(sprite[j].x,sprite[j].y,&k);
                                if (sprite[j].extra >= 0 && k == pSprite->sectnum)
                                {
                                    A_DoGutsDir(j,JIBS6,24);
                                    A_PlaySound(SQUISHED,j);
                                    A_DeleteSprite(j);
                                }
                            }

                        }
                        j = l;
                    }
                }
            }

            break;


        case SE_2_EARTHQUAKE://Quakes
            if (pData[4] > 0 && pData[0] == 0)
            {
                if (pData[4] < sh)
                    pData[4]++;
                else pData[0] = 1;
            }

            if (pData[0] > 0)
            {
                pData[0]++;

                pSprite->xvel = 3;

                if (pData[0] > 96)
                {
                    pData[0] = -1; //Stop the quake
                    pData[4] = -1;
                    KILLIT(spriteNum);
                }
                else
                {
                    if ((pData[0]&31) ==  8)
                    {
                        g_earthquakeTime = 48;
                        A_PlaySound(EARTHQUAKE,g_player[screenpeek].ps->i);
                    }

                    if (klabs(pSector->floorheinum-pData[5]) < 8)
                        pSector->floorheinum = pData[5];
                    else pSector->floorheinum += (ksgn(pData[5]-pSector->floorheinum)<<4);
                }

                m = (pSprite->xvel*sintable[(pSprite->ang+512)&2047])>>14;
                x = (pSprite->xvel*sintable[pSprite->ang&2047])>>14;


                for (ALL_PLAYERS(p))
                if (g_player[p].ps->cursectnum == pSprite->sectnum && g_player[p].ps->on_ground)
                {
                    g_player[p].ps->pos.x += m;
                    g_player[p].ps->pos.y += x;

                    g_player[p].ps->bobposx += m;
                    g_player[p].ps->bobposy += x;
                }

                j = headspritesect[pSprite->sectnum];
                while (j >= 0)
                {
                    nextj = nextspritesect[j];

                    if (sprite[j].picnum != SECTOREFFECTOR)
                    {
                        sprite[j].x+=m;
                        sprite[j].y+=x;
                        setspritez_old(j,sprite[j].x,sprite[j].y,sprite[j].z);
                    }
                    j = nextj;
                }
                ms(spriteNum);
                setspritez_old(spriteNum,pSprite->x,pSprite->y,pSprite->z);
            }
            break;

            //Flashing sector lights after reactor EXPLOSION2

        case SE_3_RANDOM_LIGHTS_AFTER_SHOT_OUT:

            if (pData[4] == 0) break;
            p = A_FindPlayer(pSprite,&x);

            //    if(t[5] > 0) { t[5]--; break; }

            if ((g_globalRandom/(sh+1)&31) < 4 && !pData[2])
            {
                //       t[5] = 4+(g_globalRandom&7);
                pSector->ceilingpal = pSprite->owner>>8;
                pSector->floorpal = pSprite->owner&0xff;
                pData[0] = pSprite->shade + (g_globalRandom&15);
            }
            else
            {
                //       t[5] = 4+(g_globalRandom&3);
                pSector->ceilingpal = pSprite->pal;
                pSector->floorpal = pSprite->pal;
                pData[0] = pData[3];
            }

            pSector->ceilingshade = pData[0];
            pSector->floorshade = pData[0];

            wal = &wall[pSector->wallptr];

            for (x=pSector->wallnum;x > 0;x--,wal++)
            {
                if (wal->hitag != 1)
                {
                    wal->shade = pData[0];
                    if ((wal->cstat&2) && wal->nextwall >= 0)
                    {
                        wall[wal->nextwall].shade = wal->shade;
                    }
                }
            }

            break;

        case SE_4_RANDOM_LIGHTS:

            if ((g_globalRandom/(sh+1)&31) < 4)
            {
                pData[1] = pSprite->shade + (g_globalRandom&15);//Got really bright
                pData[0] = pSprite->shade + (g_globalRandom&15);
                pSector->ceilingpal = pSprite->owner>>8;
                pSector->floorpal = pSprite->owner&0xff;
                j = 1;
            }
            else
            {
                pData[1] = pData[2];
                pData[0] = pData[3];

                pSector->ceilingpal = pSprite->pal;
                pSector->floorpal = pSprite->pal;

                j = 0;
            }

            pSector->floorshade = pData[1];
            pSector->ceilingshade = pData[1];

            wal = &wall[pSector->wallptr];

            for (x=pSector->wallnum;x > 0; x--,wal++)
            {
                if (j) wal->pal = (pSprite->owner&0xff);
                else wal->pal = pSprite->pal;

                if (wal->hitag != 1)
                {
                    wal->shade = pData[0];
                    if ((wal->cstat&2) && wal->nextwall >= 0)
                        wall[wal->nextwall].shade = wal->shade;
                }
            }

            j = headspritesect[SECT(spriteNum)];
            while (j >= 0)
            {
                if (sprite[j].cstat&16 && A_CheckSpriteFlags(j,SFLAG_NOSHADE) == 0)
                {
                    if (pSector->ceilingstat&1)
                        sprite[j].shade = pSector->ceilingshade;
                    else sprite[j].shade = pSector->floorshade;
                }

                j = nextspritesect[j];
            }

            if (pData[4]) KILLIT(spriteNum);

            break;

            //BOSS
        case SE_5:
            p = A_FindPlayer(pSprite,&x);
            if (x < 8192)
            {
                j = pSprite->ang;
                pSprite->ang = getangle(pSprite->x-g_player[p].ps->pos.x,pSprite->y-g_player[p].ps->pos.y);
                A_Shoot(spriteNum,FIRELASER);
                pSprite->ang = j;
            }

            if (pSprite->owner==-1) //Start search
            {
                pData[4]=0;
                l = 0x7fffffff;
                while (1) //Find the shortest dist
                {
                    pSprite->owner = A_FindLocator((short)pData[4],-1); //t[0] hold sectnum

                    if (pSprite->owner==-1) break;

                    m = ldist(&sprite[g_player[p].ps->i],&sprite[pSprite->owner]);

                    if (l > m)
                    {
                        q = pSprite->owner;
                        l = m;
                    }

                    pData[4]++;
                }

                pSprite->owner = q;
                pSprite->zvel = ksgn(sprite[q].z-pSprite->z)<<4;
            }

            if (ldist(&sprite[pSprite->owner], pSprite) < 1024)
            {
                short ta;
                ta = pSprite->ang;
                pSprite->ang = getangle(g_player[p].ps->pos.x-pSprite->x,g_player[p].ps->pos.y-pSprite->y);
                pSprite->ang = ta;
                pSprite->owner = -1;
                goto BOLT;

            }
            else pSprite->xvel=256;

            x = getangle(sprite[pSprite->owner].x-pSprite->x,sprite[pSprite->owner].y-pSprite->y);
            q = G_GetAngleDelta(pSprite->ang,x)>>3;
            pSprite->ang += q;

            if (rnd(32))
            {
                pData[2]+=q;
                pSector->ceilingshade = 127;
            }
            else
            {
                pData[2] +=
                    G_GetAngleDelta(pData[2]+512,getangle(g_player[p].ps->pos.x-pSprite->x,g_player[p].ps->pos.y-pSprite->y))>>2;
                pSector->ceilingshade = 0;
            }
            IFHIT(spriteNum)
            {
                pData[3]++;
                if (pData[3] == 5)
                {
                    pSprite->zvel += 1024;
                    P_DoQuote(QUOTE_WASTED,g_player[myconnectindex].ps);
                }
            }

            pSprite->z += pSprite->zvel;
            pSector->ceilingz += pSprite->zvel;
            sector[pData[0]].ceilingz += pSprite->zvel;
            ms(spriteNum);
            setspritez_old(spriteNum,pSprite->x,pSprite->y,pSprite->z);
            break;


        case SE_8_UP_OPEN_DOOR_LIGHTS:
        case SE_9_DOWN_OPEN_DOOR_LIGHTS:

            // work only if its moving

            j = -1;

            if (actor[spriteNum].t_data[4])
            {
                actor[spriteNum].t_data[4]++;
                if (actor[spriteNum].t_data[4] > 8) KILLIT(spriteNum);
                j = 1;
            }
            else j = GetAnimationGoal(pSprite->sectnum, ((pSector->lotag & 0xff) == ST_21_FLOOR_DOOR) ? SECTANIM_FLOOR : SECTANIM_CEILING);

            if (j >= 0)
            {
                short sn;

                if ((pSector->lotag&0x8000) || actor[spriteNum].t_data[4])
                    x = -pData[3];
                else
                    x = pData[3];

                if (st == 9) x = -x;

                j = headspritestat[STAT_EFFECTOR];
                while (j >= 0)
                {
                    if (((sprite[j].lotag) == st) && (sprite[j].hitag) == sh)
                    {
                        sn = sprite[j].sectnum;
                        m = sprite[j].shade;

                        wal = &wall[sector[sn].wallptr];

                        for (l=sector[sn].wallnum;l>0;l--,wal++)
                        {
                            if (wal->hitag != 1)
                            {
                                wal->shade+=x;

                                if (wal->shade < m)
                                    wal->shade = m;
                                else if (wal->shade > actor[j].t_data[2])
                                    wal->shade = actor[j].t_data[2];

                                if (wal->nextwall >= 0)
                                    if (wall[wal->nextwall].hitag != 1)
                                        wall[wal->nextwall].shade = wal->shade;
                            }
                        }

                        sector[sn].floorshade   += x;
                        sector[sn].ceilingshade += x;

                        if (sector[sn].floorshade < m)
                            sector[sn].floorshade = m;
                        else if (sector[sn].floorshade > actor[j].t_data[0])
                            sector[sn].floorshade = actor[j].t_data[0];

                        if (sector[sn].ceilingshade < m)
                            sector[sn].ceilingshade = m;
                        else if (sector[sn].ceilingshade > actor[j].t_data[1])
                            sector[sn].ceilingshade = actor[j].t_data[1];

                    }
                    j = nextspritestat[j];
                }
            }
            break;
        case SE_10_DOOR_AUTO_CLOSE:

            if ((pSector->lotag&0xff) == ST_27_STRETCH_BRIDGE || (pSector->floorz > pSector->ceilingz && (pSector->lotag&0xff) != ST_23_SWINGING_DOOR) || pSector->lotag == (short) 32791)
            {
                j = 1;

                if ((pSector->lotag&0xff) != ST_27_STRETCH_BRIDGE)
                    for (ALL_PLAYERS(p))
                    if (pSector->lotag != ST_30_ROTATE_RISE_BRIDGE && pSector->lotag != ST_31_TWO_WAY_TRAIN && pSector->lotag != ST_0_NO_EFFECT)
                        if (pSprite->sectnum == sprite[g_player[p].ps->i].sectnum)
                            j = 0;

                if (j == 1)
                {
                    if (pData[0] > sh)
                        switch (sector[pSprite->sectnum].lotag)
                        {
                        case ST_20_CEILING_DOOR:
                        case ST_21_FLOOR_DOOR:
                        case ST_22_SPLITTING_DOOR:
                        case ST_26_SPLITTING_ST_DOOR:
                            if (GetAnimationGoal(pSprite->sectnum, SECTANIM_CEILING) >= 0)
                                break;
                            fallthrough__;
                        default:
                            activatebysector(pSprite->sectnum, spriteNum);
                            pData[0] = 0;
                            break;
                        }
                    else pData[0]++;
                }
            }
            else pData[0]=0;
            break;
        case SE_11_SWINGING_DOOR: //Swingdoor

            if (pData[5] > 0)
            {
                pData[5]--;
                break;
            }

            if (pData[4])
            {
                int endwall = pSector->wallptr+pSector->wallnum;

                for (j=pSector->wallptr;j<endwall;j++)
                {
                    k = headspritestat[STAT_ACTOR];
                    while (k >= 0)
                    {
                        if (sprite[k].extra > 0 && A_CheckEnemySprite(&sprite[k]) && clipinsidebox(sprite[k].xy,j,256L) == 1)
                            goto BOLT;
                        k = nextspritestat[k];
                    }

                    k = headspritestat[STAT_PLAYER];
                    while (k >= 0)
                    {
                        p = A_FindPlayer(pSprite, &x);
                        if (sprite[k].owner >= 0 && clipinsidebox(sprite[k].xy,j, sprite[g_player[p].ps->i].clipdist << 1) == 1)
                        {
                            pData[5] = 8; // Delay
                            goto BOLT;
                        }
                        k = nextspritestat[k];
                    }
                }

                k = (SP(spriteNum)>>3)*pData[3];
                pData[2]+=k;
                pData[4]+=k;
                ms(spriteNum);

                if (pData[4] <= -511 || pData[4] >= 512)
                {
                    pData[4] = 0;
                    pData[2] &= 0xffffff00;
                    ms(spriteNum);
                    break;
                }
            }
            break;
        case SE_12_LIGHT_SWITCH:
            if (pData[0] == 3 || pData[3] == 1)   //Lights going off
            {
                pSector->floorpal = 0;
                pSector->ceilingpal = 0;

                wal = &wall[pSector->wallptr];
                for (j = pSector->wallnum;j > 0; j--, wal++)
                    if (wal->hitag != 1)
                    {
                        wal->shade = pData[1];
                        wal->pal = 0;
                    }

                pSector->floorshade = pData[1];
                pSector->ceilingshade = pData[2];
                pData[0]=0;

                j = headspritesect[SECT(spriteNum)];
                while (j >= 0)
                {
                    if (sprite[j].cstat&16 && A_CheckSpriteFlags(j,SFLAG_NOSHADE) == 0)
                    {
                        if (pSector->ceilingstat&1)
                            sprite[j].shade = pSector->ceilingshade;
                        else sprite[j].shade = pSector->floorshade;
                    }
                    j = nextspritesect[j];

                }

                if (pData[3] == 1) KILLIT(spriteNum);
            }
            if (pData[0] == 1)   //Lights flickering on
            {
                if (pSector->floorshade > pSprite->shade)
                {
                    pSector->floorpal = pSprite->pal;
                    pSector->ceilingpal = pSprite->pal;

                    pSector->floorshade -= 2;
                    pSector->ceilingshade -= 2;

                    wal = &wall[pSector->wallptr];
                    for (j=pSector->wallnum;j>0;j--,wal++)
                        if (wal->hitag != 1)
                        {
                            wal->pal = pSprite->pal;
                            wal->shade -= 2;
                        }
                }
                else pData[0] = 2;

                j = headspritesect[SECT(spriteNum)];
                while (j >= 0)
                {
                    if (sprite[j].cstat&16)
                    {
                        if (pSector->ceilingstat&1 && A_CheckSpriteFlags(j,SFLAG_NOSHADE) == 0)
                            sprite[j].shade = pSector->ceilingshade;
                        else sprite[j].shade = pSector->floorshade;
                    }
                    j = nextspritesect[j];
                }
            }
            break;


        case SE_13_EXPLOSIVE:
            if (pData[2])
            {
                j = (SP(spriteNum)<<5)|1;

                if (pSprite->ang == 512)
                {
                    if (pSprite->owner)
                    {
                        if (klabs(pData[0]-pSector->ceilingz) >= j)
                            pSector->ceilingz += ksgn(pData[0]-pSector->ceilingz)*j;
                        else pSector->ceilingz = pData[0];
                    }
                    else
                    {
                        if (klabs(pData[1]-pSector->floorz) >= j)
                            pSector->floorz += ksgn(pData[1]-pSector->floorz)*j;
                        else pSector->floorz = pData[1];
                    }
                }
                else
                {
                    if (klabs(pData[1]-pSector->floorz) >= j)
                        pSector->floorz += ksgn(pData[1]-pSector->floorz)*j;
                    else pSector->floorz = pData[1];
                    if (klabs(pData[0]-pSector->ceilingz) >= j)
                        pSector->ceilingz += ksgn(pData[0]-pSector->ceilingz)*j;
                    pSector->ceilingz = pData[0];
                }

                if (pData[3] == 1)
                {
                    //Change the shades

                    pData[3]++;
                    pSector->ceilingstat ^= 1;

                    if (pSprite->ang == 512)
                    {
                        wal = &wall[pSector->wallptr];
                        for (j=pSector->wallnum;j>0;j--,wal++)
                            wal->shade = pSprite->shade;

                        pSector->floorshade = pSprite->shade;

                        if (g_player[0].ps->one_parallax_sectnum >= 0)
                        {
                            pSector->ceilingpicnum =
                                sector[g_player[0].ps->one_parallax_sectnum].ceilingpicnum;
                            pSector->ceilingshade  =
                                sector[g_player[0].ps->one_parallax_sectnum].ceilingshade;
                        }
                    }
                }
                pData[2]++;
                if (pData[2] > 256)
                    KILLIT(spriteNum);
            }


            if (pData[2] == 4 && pSprite->ang != 512)
                for (x=0;x<7;x++) RANDOMSCRAP(pSprite, spriteNum);
            break;


        case SE_15_SLIDING_DOOR:

            if (pData[4])
            {
                pSprite->xvel = 16;

                if (pData[4] == 1) //Opening
                {
                    if (pData[3] >= (SP(spriteNum)>>3))
                    {
                        pData[4] = 0; //Turn off the sliders
                        A_CallSound(pSprite->sectnum, spriteNum);
                        break;
                    }
                    pData[3]++;
                }
                else if (pData[4] == 2)
                {
                    if (pData[3]<1)
                    {
                        pData[4] = 0;
                        A_CallSound(pSprite->sectnum, spriteNum);
                        break;
                    }
                    pData[3]--;
                }

                ms(spriteNum);
                setspritez_old(spriteNum,pSprite->x,pSprite->y,pSprite->z);
            }
            break;

        case SE_16_REACTOR: //Reactor

            pData[2]+=32;
            if (pSector->floorz<pSector->ceilingz) pSprite->shade=0;

            else if (pSector->ceilingz < pData[3])
            {

                //The following code check to see if
                //there is any other sprites in the sector.
                //If there isn't, then kill this sectoreffector
                //itself.....

                j = headspritesect[pSprite->sectnum];
                while (j >= 0)
                {
                    if (sprite[j].picnum == REACTOR || sprite[j].picnum == REACTOR2)
                        break;
                    j = nextspritesect[j];
                }
                if (j == -1)
                {
                    KILLIT(spriteNum);
                }
                else pSprite->shade=1;
            }

            if (pSprite->shade) pSector->ceilingz+=1024;
            else pSector->ceilingz-=512;

            ms(spriteNum);
            setspritez_old(spriteNum,pSprite->x,pSprite->y,pSprite->z);

            break;

        case SE_17_WARP_ELEVATOR:
            q = pData[0] * (SP(spriteNum) << 2);

            pSector->ceilingz += q;
            pSector->floorz += q;

            j = headspritesect[pSprite->sectnum];
            while (j >= 0)
            {
                if (sprite[j].statnum == STAT_PLAYER && sprite[j].owner >= 0)
                {
                    p = sprite[j].yvel;
                    auto const pPlayer = g_player[p].ps;
                    pPlayer->pos.z += q;
                    pPlayer->truefz += q;
                    pPlayer->truecz += q;
                }

                if (sprite[j].statnum != STAT_EFFECTOR)
                {
                    sprite[j].z += q;
                }

                actor[j].floorz = pSector->floorz;
                actor[j].ceilingz = pSector->ceilingz;

                j = nextspritesect[j];
            }

            if (pData[0])  //If in motion
            {
                if (klabs(pSector->floorz-pData[2]) <= SP(spriteNum))
                {
                    G_ActivateWarpElevators(spriteNum,0);
                    break;
                }

                if (pData[0]==-1)
                {
                    if (pSector->floorz > pData[3])
                        break;
                }
                else if (pSector->ceilingz < pData[4]) break;

                if (pData[1] == 0) break;
                pData[1] = 0;

                j = headspritestat[STAT_EFFECTOR];
                while (j >= 0)
                {
                    if (spriteNum != j && (sprite[j].lotag) == 17)
                        if ((pSector->hitag-pData[0]) ==
                                (sector[sprite[j].sectnum].hitag)
                                && sh == (sprite[j].hitag))
                            break;
                    j = nextspritestat[j];
                }

                if (j == -1) break;

                k = headspritesect[pSprite->sectnum];
                while (k >= 0)
                {
                    nextk = nextspritesect[k];

                    if (sprite[k].statnum == STAT_PLAYER && sprite[k].owner >= 0)
                    {
                        p = sprite[k].yvel;
                        auto const pPlayer = g_player[p].ps;

                        pPlayer->pos.x += sprite[j].x-pSprite->x;
                        pPlayer->pos.y += sprite[j].y-pSprite->y;

                        pPlayer->pos.z = sector[sprite[j].sectnum].floorz-(pSector->floorz-pPlayer->pos.z);

                        actor[k].floorz = sector[sprite[j].sectnum].floorz;
                        actor[k].ceilingz = sector[sprite[j].sectnum].ceilingz;

                        pPlayer->bobposx = pPlayer->opos.x = pPlayer->pos.x;
                        pPlayer->bobposy = pPlayer->opos.y = pPlayer->pos.y;

                        pPlayer->truefz = actor[k].floorz;
                        pPlayer->truecz = actor[k].ceilingz;
                        pPlayer->bobcounter = 0;

                        changespritesect(k,sprite[j].sectnum);
                        pPlayer->cursectnum = sprite[j].sectnum;
                    }
                    else if (sprite[k].statnum != STAT_EFFECTOR)
                    {
                        sprite[k].x += sprite[j].x-pSprite->x;
                        sprite[k].y += sprite[j].y-pSprite->y;

                        actor[k].bpos.z -= sprite[k].z;

                        sprite[k].z = sector[sprite[j].sectnum].floorz-
                                      (pSector->floorz-sprite[k].z);

                        actor[k].bpos.x = sprite[k].x;
                        actor[k].bpos.y = sprite[k].y;
                        actor[k].bpos.z += sprite[k].z;

                        changespritesect(k,sprite[j].sectnum);
                        setspritez_old(k,sprite[k].x,sprite[k].y,sprite[k].z);

                        actor[k].floorz = sector[sprite[j].sectnum].floorz;
                        actor[k].ceilingz = sector[sprite[j].sectnum].ceilingz;

                    }
                    k = nextk;
                }
            }
            break;

        case SE_18_INCREMENTAL_SECTOR_RISE_FALL:
            if (pData[0])
            {
                if (pSprite->pal)
                {
                    if (pSprite->ang == 512)
                    {
                        pSector->ceilingz -= pSector->extra;
                        if (pSector->ceilingz <= pData[1])
                        {
                            pSector->ceilingz = pData[1];
                            KILLIT(spriteNum);
                        }
                    }
                    else
                    {
                        pSector->floorz += pSector->extra;
                        j = headspritesect[pSprite->sectnum];
                        while (j >= 0)
                        {
                            if (sprite[j].picnum == APLAYER && sprite[j].owner >= 0)
                                if (g_player[sprite[j].yvel].ps->on_ground == 1)
                                    g_player[sprite[j].yvel].ps->pos.z += pSector->extra;
                            if (sprite[j].zvel == 0 && sprite[j].statnum != 3 && sprite[j].statnum != 4)
                            {
                                actor[j].bpos.z = sprite[j].z += pSector->extra;
                                actor[j].floorz = pSector->floorz;
                            }
                            j = nextspritesect[j];
                        }
                        if (pSector->floorz >= pData[1])
                        {
                            pSector->floorz = pData[1];
                            KILLIT(spriteNum);
                        }
                    }
                }
                else
                {
                    if (pSprite->ang == 512)
                    {
                        pSector->ceilingz += pSector->extra;
                        if (pSector->ceilingz >= pSprite->z)
                        {
                            pSector->ceilingz = pSprite->z;
                            KILLIT(spriteNum);
                        }
                    }
                    else
                    {
                        pSector->floorz -= pSector->extra;
                        j = headspritesect[pSprite->sectnum];
                        while (j >= 0)
                        {
                            if (sprite[j].picnum == APLAYER && sprite[j].owner >= 0)
                                if (g_player[sprite[j].yvel].ps->on_ground == 1)
                                    g_player[sprite[j].yvel].ps->pos.z -= pSector->extra;
                            if (sprite[j].zvel == 0 && sprite[j].statnum != 3 && sprite[j].statnum != 4)
                            {
                                actor[j].bpos.z = sprite[j].z -= pSector->extra;
                                actor[j].floorz = pSector->floorz;
                            }
                            j = nextspritesect[j];
                        }
                        if (pSector->floorz <= pSprite->z)
                        {
                            pSector->floorz = pSprite->z;
                            KILLIT(spriteNum);
                        }
                    }
                }

                pData[2]++;
                if (pData[2] >= pSprite->hitag)
                {
                    pData[2] = 0;
                    pData[0] = 0;
                }
            }
            break;

        case SE_19_EXPLOSION_LOWERS_CEILING: //Battlestar galactia shields

            if (pData[0])
            {
                if (pData[0] == 1)
                {
                    pData[0]++;
                    x = pSector->wallptr;
                    q = x+pSector->wallnum;
                    for (j=x;j<q;j++)
                        if (wall[j].overpicnum == BIGFORCE)
                        {
                            wall[j].cstat &= (128+32+8+4+2);
                            wall[j].overpicnum = 0;
                            if (wall[j].nextwall >= 0)
                            {
                                wall[wall[j].nextwall].overpicnum = 0;
                                wall[wall[j].nextwall].cstat &= (128+32+8+4+2);
                            }
                        }
                }

                if (pSector->ceilingz < pSector->floorz)
                    pSector->ceilingz += SP(spriteNum);
                else
                {
                    pSector->ceilingz = pSector->floorz;

                    j = headspritestat[STAT_EFFECTOR];
                    while (j >= 0)
                    {
                        if (sprite[j].lotag == SE_0_ROTATING_SECTOR && sprite[j].hitag==sh)
                        {
                            q = sprite[sprite[j].owner].sectnum;
                            sector[sprite[j].sectnum].floorpal = sector[sprite[j].sectnum].ceilingpal =
                                                                     sector[q].floorpal;
                            sector[sprite[j].sectnum].floorshade = sector[sprite[j].sectnum].ceilingshade =
                                                                       sector[q].floorshade;

                            actor[sprite[j].owner].t_data[0] = 2;
                        }
                        j = nextspritestat[j];
                    }
                    KILLIT(spriteNum);
                }
            }
            else //Not hit yet
            {
                if (G_FindExplosionInSector(pSprite->sectnum) >= 0)
                {
                    P_DoQuote(QUOTE_UNLOCKED,g_player[myconnectindex].ps);

                    l = headspritestat[STAT_EFFECTOR];
                    while (l >= 0)
                    {
                        x = sprite[l].lotag&0x7fff;
                        switch (x)
                        {
                        case SE_0_ROTATING_SECTOR:
                            if (sprite[l].hitag == sh)
                            {
                                q = sprite[l].sectnum;
                                sector[q].floorshade =
                                    sector[q].ceilingshade =
                                        sprite[sprite[l].owner].shade;
                                sector[q].floorpal =
                                    sector[q].ceilingpal =
                                        sprite[sprite[l].owner].pal;
                            }
                            break;

                        case SE_1_PIVOT:
                        case SE_12_LIGHT_SWITCH:
                        // case SE_18_INCREMENTAL_SECTOR_RISE_FALL:
                        case SE_19_EXPLOSION_LOWERS_CEILING:

                            if (sh == sprite[l].hitag)
                                if (actor[l].t_data[0] == 0)
                                {
                                    actor[l].t_data[0] = 1; //Shut them all on
                                    sprite[l].owner = spriteNum;
                                }

                            break;
                        }
                        l = nextspritestat[l];
                    }
                }
            }

            break;

        case SE_20_STRETCH_BRIDGE: //Extend-o-bridge

            if (pData[0] == 0) break;
            if (pData[0] == 1) pSprite->xvel = 8;
            else pSprite->xvel = -8;

            if (pSprite->xvel)   //Moving
            {
                x = (pSprite->xvel*sintable[(pSprite->ang+512)&2047])>>14;
                l = (pSprite->xvel*sintable[pSprite->ang&2047])>>14;

                pData[3] += pSprite->xvel;

                pSprite->x += x;
                pSprite->y += l;

                if (pData[3] <= 0 || (pData[3]>>6) >= (SP(spriteNum)>>6))
                {
                    pSprite->x -= x;
                    pSprite->y -= l;
                    pData[0] = 0;
                    A_CallSound(pSprite->sectnum, spriteNum);
                    break;
                }

                j = headspritesect[pSprite->sectnum];
                while (j >= 0)
                {
                    nextj = nextspritesect[j];

                    if (sprite[j].statnum != 3 && sprite[j].zvel == 0)
                    {
                        sprite[j].x += x;
                        sprite[j].y += l;
                        setspritez_old(j,sprite[j].x, sprite[j].y, sprite[j].z);
                        if (sector[sprite[j].sectnum].floorstat&2)
                            if (sprite[j].statnum == 2)
                                A_Fall(j);
                    }
                    j = nextj;
                }

                dragpoint((short)pData[1],wall[pData[1]].x+x,wall[pData[1]].y+l, 0);
                dragpoint((short)pData[2],wall[pData[2]].x+x,wall[pData[2]].y+l, 0);

                for (ALL_PLAYERS(p))
                if (g_player[p].ps->cursectnum == pSprite->sectnum && g_player[p].ps->on_ground)
                {
                    g_player[p].ps->pos.x += x;
                    g_player[p].ps->pos.y += l;

                    g_player[p].ps->opos.x = g_player[p].ps->pos.x;
                    g_player[p].ps->opos.y = g_player[p].ps->pos.y;

                    setspritez_old(g_player[p].ps->i,g_player[p].ps->pos.x,g_player[p].ps->pos.y,g_player[p].ps->pos.z+PHEIGHT);
                }

                pSector->floorxpanning-=x>>3;
                pSector->floorypanning-=l>>3;

                pSector->ceilingxpanning-=x>>3;
                pSector->ceilingypanning-=l>>3;
            }

            break;

        case SE_21_DROP_FLOOR: // Cascading effect
        {
            if (pData[0] == 0) break;

            int32_t* zptr = (pSprite->ang == 1536) ? &pSector->ceilingz : &pSector->floorz;

            if (pData[0] == 1)   //Decide if the s->sectnum should go up or down
            {
                pSprite->zvel = ksgn(pSprite->z - *zptr) * (SP(spriteNum) << 4);
                pData[0]++;
            }

            if (pSector->extra == 0)
            {
                *zptr += pSprite->zvel;

                if (klabs(*zptr - pSprite->z) < 1024)
                {
                    *zptr = pSprite->z;
                    KILLIT(spriteNum); //All done   // SE_21_KILLIT, see sector.c
                }
            }
            else pSector->extra--;
            break;
        }

        case SE_22_TEETH_DOOR:

            if (pData[1])
            {
                if (GetAnimationGoal(pData[0], SECTANIM_CEILING) >= 0)
                    pSector->ceilingz += pSector->extra*9;
                else pData[1] = 0;
            }
            break;

        case SE_24_CONVEYOR:
        case SE_34:

            if (pData[4]) break;

            x = (SP(spriteNum)*sintable[(pSprite->ang+512)&2047])>>18;
            l = (SP(spriteNum)*sintable[pSprite->ang&2047])>>18;

            k = 0;

            j = headspritesect[pSprite->sectnum];
            while (j >= 0)
            {
                nextj = nextspritesect[j];
                if (sprite[j].zvel >= 0)
                    switch (sprite[j].statnum)
                    {
                    case 5:
                        switch (tileGetMapping(sprite[j].picnum))
                        {
                        case BLOODPOOL__:
                        case PUKE__:
                        case FOOTPRINTS__:
                        case FOOTPRINTS2__:
                        case FOOTPRINTS3__:
                        case FOOTPRINTS4__:
                        case BULLETHOLE__:
                        case BLOODSPLAT1__:
                        case BLOODSPLAT2__:
                        case BLOODSPLAT3__:
                        case BLOODSPLAT4__:
                            sprite[j].xrepeat = sprite[j].yrepeat = 0;
                            j = nextj;
                            continue;
                        case LASERLINE__:
                            j = nextj;
                            continue;
                        }
                        fallthrough__;
                    case 6:
                        if (sprite[j].picnum == TRIPBOMB) break;
                        fallthrough__;
                    case 1:
                    case 0:
                        if (
                            sprite[j].picnum == BOLT1 ||
                            sprite[j].picnum == BOLT1+1 ||
                            sprite[j].picnum == BOLT1+2 ||
                            sprite[j].picnum == BOLT1+3 ||
                            sprite[j].picnum == SIDEBOLT1 ||
                            sprite[j].picnum == SIDEBOLT1+1 ||
                            sprite[j].picnum == SIDEBOLT1+2 ||
                            sprite[j].picnum == SIDEBOLT1+3 ||
                            A_CheckSwitchTile(j)
                        )
                            break;

                        if (!(sprite[j].picnum >= CRANE && sprite[j].picnum <= (CRANE+3)))
                        {
                            if (sprite[j].z > (actor[j].floorz-(16<<8)))
                            {
                                actor[j].bpos.x = sprite[j].x;
                                actor[j].bpos.y = sprite[j].y;

                                sprite[j].x += x>>2;
                                sprite[j].y += l>>2;

                                setspritez_old(j,sprite[j].x,sprite[j].y,sprite[j].z);

                                if (sector[sprite[j].sectnum].floorstat&2)
                                    if (sprite[j].statnum == 2)
                                        A_Fall(j);
                            }
                        }
                        break;
                    }
                j = nextj;
            }

            p = myconnectindex;
            if (g_player[p].ps->cursectnum == pSprite->sectnum && g_player[p].ps->on_ground)
                if (klabs(g_player[p].ps->pos.z-g_player[p].ps->truefz) < PHEIGHT+(9<<8))
                {
                    fricxv += x<<3;
                    fricyv += l<<3;
                }

            pSector->floorxpanning += SP(spriteNum)>>7;
            actor[spriteNum].bpos.x = pSector->floorxpanning;

            break;

        case SE_35:
            if (pSector->ceilingz > pSprite->z)
                for (j = 0;j < 8;j++)
                {
                    pSprite->ang += krand()&511;
                    k = A_Spawn(spriteNum,SMALLSMOKE);
                    sprite[k].xvel = 96+(krand()&127);
                    A_SetSprite(k,CLIPMASK0);
                    setspritez_old(k, sprite[k].x, sprite[k].y, sprite[k].z);
                    if (rnd(16))
                        A_Spawn(spriteNum,EXPLOSION2);
                }

            switch (pData[0])
            {
            case 0:
                pSector->ceilingz += pSprite->yvel;
                if (pSector->ceilingz > pSector->floorz)
                    pSector->floorz = pSector->ceilingz;
                if (pSector->ceilingz > pSprite->z+(32<<8))
                    pData[0]++;
                break;
            case 1:
                pSector->ceilingz-=(pSprite->yvel<<2);
                if (pSector->ceilingz < pData[4])
                {
                    pSector->ceilingz = pData[4];
                    pData[0] = 0;
                }
                break;
            }
            break;

        case SE_25_PISTON: //PISTONS

            if (pData[4] == 0) break;

            if (pSector->floorz <= pSector->ceilingz)
                pSprite->shade = 0;
            else if (pSector->ceilingz <= pData[3])
                pSprite->shade = 1;

            if (pSprite->shade)
            {
                pSector->ceilingz += SP(spriteNum)<<4;
                if (pSector->ceilingz > pSector->floorz)
                    pSector->ceilingz = pSector->floorz;
            }
            else
            {
                pSector->ceilingz -= SP(spriteNum)<<4;
                if (pSector->ceilingz < pData[3])
                    pSector->ceilingz = pData[3];
            }

            break;

        case SE_26:

            pSprite->xvel = 32;
            l = (pSprite->xvel*sintable[(pSprite->ang+512)&2047])>>14;
            x = (pSprite->xvel*sintable[pSprite->ang&2047])>>14;

            pSprite->shade++;
            if (pSprite->shade > 7)
            {
                pSprite->x = pData[3];
                pSprite->y = pData[4];
                pSector->floorz -= ((pSprite->zvel*pSprite->shade)-pSprite->zvel);
                pSprite->shade = 0;
            }
            else
                pSector->floorz += pSprite->zvel;

            j = headspritesect[pSprite->sectnum];
            while (j >= 0)
            {
                nextj = nextspritesect[j];
                if (sprite[j].statnum != 3 && sprite[j].statnum != 10)
                {
                    actor[j].bpos.x = sprite[j].x;
                    actor[j].bpos.y = sprite[j].y;

                    sprite[j].x += l;
                    sprite[j].y += x;

                    sprite[j].z += pSprite->zvel;
                    setspritez_old(j, sprite[j].x, sprite[j].y, sprite[j].z);
                }
                j = nextj;
            }

            p = myconnectindex;
            if (sprite[g_player[p].ps->i].sectnum == pSprite->sectnum && g_player[p].ps->on_ground)
            {
                fricxv += l<<5;
                fricyv += x<<5;
            }

            for (ALL_PLAYERS(p))
                if (sprite[g_player[p].ps->i].sectnum == pSprite->sectnum && g_player[p].ps->on_ground)
                    g_player[p].ps->pos.z += pSprite->zvel;

            ms(spriteNum);
            setspritez_old(spriteNum, pSprite->x, pSprite->y, pSprite->z);

            break;


        case SE_27_DEMO_CAM:

            if (ud.recstat == 0 || !ud.democams) break;

            actor[spriteNum].tempang = pSprite->ang;

            p = A_FindPlayer(pSprite,&x);
            if (sprite[g_player[p].ps->i].extra > 0 && myconnectindex == screenpeek)
            {
                if (pData[0] < 0)
                {
                    ud.camerasprite = spriteNum;
                    pData[0]++;
                }
                else if (ud.recstat == 2 && g_player[p].ps->newowner == -1)
                {
                    if (cansee(pSprite->x,pSprite->y,pSprite->z,SECT(spriteNum),g_player[p].ps->pos.x,g_player[p].ps->pos.y,g_player[p].ps->pos.z,g_player[p].ps->cursectnum))
                    {
                        if (x < (int)((unsigned)sh))
                        {
                            ud.camerasprite = spriteNum;
                            pData[0] = 999;
                            pSprite->ang += G_GetAngleDelta(pSprite->ang,getangle(g_player[p].ps->pos.x-pSprite->x,g_player[p].ps->pos.y-pSprite->y))>>3;
                            SP(spriteNum) = 100+((pSprite->z-g_player[p].ps->pos.z)/257);

                        }
                        else if (pData[0] == 999)
                        {
                            if (ud.camerasprite == spriteNum)
                                pData[0] = 0;
                            else pData[0] = -10;
                            ud.camerasprite = spriteNum;

                        }
                    }
                    else
                    {
                        pSprite->ang = getangle(g_player[p].ps->pos.x-pSprite->x,g_player[p].ps->pos.y-pSprite->y);

                        if (pData[0] == 999)
                        {
                            if (ud.camerasprite == spriteNum)
                                pData[0] = 0;
                            else pData[0] = -20;
                            ud.camerasprite = spriteNum;
                        }
                    }
                }
            }
            break;
        case SE_28_LIGHTNING:
            if (pData[5] > 0)
            {
                pData[5]--;
                break;
            }

            if (T1(spriteNum) == 0)
            {
                p = A_FindPlayer(pSprite,&x);
                if (x > 15500)
                    break;
                T1(spriteNum) = 1;
                T2(spriteNum) = 64 + (krand()&511);
                T3(spriteNum) = 0;
            }
            else
            {
                T3(spriteNum)++;
                if (T3(spriteNum) > T2(spriteNum))
                {
                    T1(spriteNum) = 0;
                    g_player[screenpeek].display.visibility = ud.const_visibility;
                    break;
                }
                else if (T3(spriteNum) == (T2(spriteNum)>>1))
                    A_PlaySound(THUNDER, spriteNum);
                else if (T3(spriteNum) == (T2(spriteNum)>>3))
                    A_PlaySound(LIGHTNING_SLAP, spriteNum);
                else if (T3(spriteNum) == (T2(spriteNum)>>2))
                {
                    j = headspritestat[STAT_DEFAULT];
                    while (j >= 0)
                    {
                        if (sprite[j].picnum == NATURALLIGHTNING && sprite[j].hitag == pSprite->hitag)
                            sprite[j].cstat |= 32768;
                        j = nextspritestat[j];
                    }
                }
                else if (T3(spriteNum) > (T2(spriteNum)>>3) && T3(spriteNum) < (T2(spriteNum)>>2))
                {
                    if (cansee(pSprite->x,pSprite->y,pSprite->z,pSprite->sectnum,g_player[screenpeek].ps->pos.x,g_player[screenpeek].ps->pos.y,g_player[screenpeek].ps->pos.z,g_player[screenpeek].ps->cursectnum))
                        j = 1;
                    else j = 0;

                    if (rnd(192) && (T3(spriteNum)&1))
                    {
                        if (j)
                            g_player[screenpeek].display.visibility = 0;
                    }
                    else if (j)
                        g_player[screenpeek].display.visibility = ud.const_visibility;

                    j = headspritestat[STAT_DEFAULT];
                    while (j >= 0)
                    {
                        if (sprite[j].picnum == NATURALLIGHTNING && sprite[j].hitag == pSprite->hitag)
                        {
                            if (rnd(32) && (T3(spriteNum)&1))
                            {
                                sprite[j].cstat &= 32767;
                                A_Spawn(j,SMALLSMOKE);

                                p = A_FindPlayer(pSprite,&x);
                                x = ldist(&sprite[g_player[p].ps->i], &sprite[j]);
                                if (x < 768)
                                {
                                    if (!A_CheckSoundPlaying(g_player[p].ps->i,DUKE_LONGTERM_PAIN))
                                        A_PlaySound(DUKE_LONGTERM_PAIN,g_player[p].ps->i);
                                    A_PlaySound(SHORT_CIRCUIT,g_player[p].ps->i);
                                    sprite[g_player[p].ps->i].extra -= 8+(krand()&7);
                                    P_PalFrom(g_player[p].ps, 32, 16, 0, 0);
                                }
                                break;
                            }
                            else sprite[j].cstat |= 32768;
                        }

                        j = nextspritestat[j];
                    }
                }
            }
            break;
        case SE_29_WAVES:
            pSprite->hitag += 64;
            l = mulscale12((int)pSprite->yvel,sintable[pSprite->hitag&2047]);
            pSector->floorz = pSprite->z + l;
            break;
        case SE_31_FLOOR_RISE_FALL: // True Drop Floor
            if (pData[0] == 1)
            {
                // Choose dir

                if (pData[3] > 0)
                {
                    pData[3]--;
                    break;
                }

                if (pData[2] == 1) // Retract
                {
                    if (SA(spriteNum) != 1536)
                        HandleSE31(spriteNum, 1, pSprite->z, 0, pSprite->z - pSector->floorz);
                    else
                        HandleSE31(spriteNum, 1, pData[1], 0, pData[1] - pSector->floorz);

                    break;
                }

                if ((pSprite->ang & 2047) == 1536)
                    HandleSE31(spriteNum, 0, pSprite->z, 1, pSprite->z - pSector->floorz);
                else
                    HandleSE31(spriteNum, 0, pData[1], 1, pData[1] - pSprite->z);
            }
            break;

        case SE_32_CEILING_RISE_FALL: // True Drop Ceiling
            if (pData[0] == 1)
            {
                // Choose dir

                if (pData[2] == 1) // Retract
                {
                    if (SA(spriteNum) != 1536)
                    {
                        if (klabs(pSector->ceilingz - pSprite->z) <
                                (SP(spriteNum)<<1))
                        {
                            pSector->ceilingz = pSprite->z;
                            A_CallSound(pSprite->sectnum, spriteNum);
                            pData[2] = 0;
                            pData[0] = 0;
                        }
                        else pSector->ceilingz +=
                                ksgn(pSprite->z-pSector->ceilingz)*SP(spriteNum);
                    }
                    else
                    {
                        if (klabs(pSector->ceilingz - pData[1]) <
                                (SP(spriteNum)<<1))
                        {
                            pSector->ceilingz = pData[1];
                            A_CallSound(pSprite->sectnum, spriteNum);
                            pData[2] = 0;
                            pData[0] = 0;
                        }
                        else pSector->ceilingz +=
                                ksgn(pData[1]-pSector->ceilingz)*SP(spriteNum);
                    }
                    break;
                }

                if ((pSprite->ang&2047) == 1536)
                {
                    if (klabs(pSector->ceilingz-pSprite->z) <
                            (SP(spriteNum)<<1))
                    {
                        pData[0] = 0;
                        pData[2] = !pData[2];
                        A_CallSound(pSprite->sectnum, spriteNum);
                        pSector->ceilingz = pSprite->z;
                    }
                    else pSector->ceilingz +=
                            ksgn(pSprite->z-pSector->ceilingz)*SP(spriteNum);
                }
                else
                {
                    if (klabs(pSector->ceilingz-pData[1]) < (SP(spriteNum)<<1))
                    {
                        pData[0] = 0;
                        pData[2] = !pData[2];
                        A_CallSound(pSprite->sectnum, spriteNum);
                    }
                    else pSector->ceilingz -= ksgn(pSprite->z-pData[1])*SP(spriteNum);
                }
            }
            break;

        case SE_33_QUAKE_DEBRIS:
            if (g_earthquakeTime > 0 && (krand()&7) == 0)
                RANDOMSCRAP(pSprite, spriteNum);
            break;
        case SE_36_PROJ_SHOOTER:

            if (pData[0])
            {
                if (pData[0] == 1)
                    A_Shoot(spriteNum, pSector->extra);
                else if (pData[0] == 26*5)
                    pData[0] = 0;
                pData[0]++;
            }
            break;

        case 128: //SE to control glass breakage

            wal = &wall[pData[2]];

            if (wal->cstat|32)
            {
                wal->cstat &= (255-32);
                wal->cstat |= 16;
                if (wal->nextwall >= 0)
                {
                    wall[wal->nextwall].cstat &= (255-32);
                    wall[wal->nextwall].cstat |= 16;
                }
            }
            else break;

            wal->overpicnum++;
            if (wal->nextwall >= 0)
                wall[wal->nextwall].overpicnum++;

            if (pData[0] < pData[1]) pData[0]++;
            else
            {
                wal->cstat &= (128+32+8+4+2);
                if (wal->nextwall >= 0)
                    wall[wal->nextwall].cstat &= (128+32+8+4+2);
                KILLIT(spriteNum);
            }
            break;

        case SE_130:
            if (pData[0] > 80)
            {
                KILLIT(spriteNum);
            }
            else pData[0]++;

            x = pSector->floorz-pSector->ceilingz;

            if (rnd(64))
            {
                k = A_Spawn(spriteNum, EXPLOSION2);
                sprite[k].xrepeat = sprite[k].yrepeat = 2+(krand()&7);
                sprite[k].z = pSector->floorz-(krand()%x);
                sprite[k].ang += 256-(krand()%511);
                sprite[k].xvel = krand()&127;
                A_SetSprite(k,CLIPMASK0);
            }
            break;
        case SE_131:
            if (pData[0] > 40)
            {
                KILLIT(spriteNum);
            }
            else pData[0]++;

            x = pSector->floorz-pSector->ceilingz;

            if (rnd(32))
            {
                k = A_Spawn(spriteNum, EXPLOSION2);
                sprite[k].xrepeat = sprite[k].yrepeat = 2+(krand()&3);
                sprite[k].z = pSector->floorz-(krand()%x);
                sprite[k].ang += 256-(krand()%511);
                sprite[k].xvel = krand()&127;
                A_SetSprite(k,CLIPMASK0);
            }
            break;
        case SE_49_POINT_LIGHT:
        case SE_50_SPOT_LIGHT:
            changespritestat(spriteNum, STAT_LIGHT);
            break;
        }
BOLT:
        spriteNum = nextSprite;
    }

    //Sloped sin-wave floors!
    for (SPRITES_OF(STAT_EFFECTOR, spriteNum))
    {
        auto const s = &sprite[spriteNum];

        if (s->lotag == SE_29_WAVES)
        {
            auto const sc = (usectorptr_t)&sector[s->sectnum];

            if (sc->wallnum == 4)
            {
                auto const pWall = &wall[sc->wallptr + 2];
                if (pWall->nextsector >= 0)
                    alignflorslope(s->sectnum, pWall->x, pWall->y, sector[pWall->nextsector].floorz);
            }
        }
    }
}

void A_PlayAlertSound(int i)
{
    if (sprite[i].extra > 0)
        switch (tileGetMapping(PN(i)))
        {
        case LIZTROOPONTOILET__:
        case LIZTROOPJUSTSIT__:
        case LIZTROOPSHOOT__:
        case LIZTROOPJETPACK__:
        case LIZTROOPDUCKING__:
        case LIZTROOPRUNNING__:
        case LIZTROOP__:
            A_PlaySound(PRED_RECOG,i);
            break;
        case LIZMAN__:
        case LIZMANSPITTING__:
        case LIZMANFEEDING__:
        case LIZMANJUMP__:
            A_PlaySound(CAPT_RECOG,i);
            break;
        case PIGCOP__:
        case PIGCOPDIVE__:
            A_PlaySound(PIG_RECOG,i);
            break;
        case RECON__:
            A_PlaySound(RECO_RECOG,i);
            break;
        case DRONE__:
            A_PlaySound(DRON_RECOG,i);
            break;
        case COMMANDER__:
        case COMMANDERSTAYPUT__:
            A_PlaySound(COMM_RECOG,i);
            break;
        case ORGANTIC__:
            A_PlaySound(TURR_RECOG,i);
            break;
        case OCTABRAIN__:
        case OCTABRAINSTAYPUT__:
            A_PlaySound(OCTA_RECOG,i);
            break;
        case BOSS1__:
            S_PlaySound(BOS1_RECOG);
            break;
        case BOSS2__:
            if (sprite[i].pal == 1)
                S_PlaySound(BOS2_RECOG);
            else S_PlaySound(WHIPYOURASS);
            break;
        case BOSS3__:
            if (sprite[i].pal == 1)
                S_PlaySound(BOS3_RECOG);
            else S_PlaySound(RIPHEADNECK);
            break;
        case BOSS4__:
        case BOSS4STAYPUT__:
            if (sprite[i].pal == 1)
                S_PlaySound(BOS4_RECOG);
            S_PlaySound(BOSS4_FIRSTSEE);
            break;
        case GREENSLIME__:
            A_PlaySound(SLIM_RECOG,i);
            break;
        }
}


int A_CheckSpriteFlags(int iActor, int iType)
{
    if ((g_tile[sprite[iActor].picnum].flags^actor[iActor].flags) & iType) return 1;
    return 0;
}

int A_CheckSpriteTileFlags(int iPicnum, int iType)
{
    if (g_tile[iPicnum].flags & iType) return 1;
    return 0;
}

int A_CheckEnemyTile(int pn)
{
    return ((g_tile[pn].flags & (SFLAG_HARDCODED_BADGUY | SFLAG_BADGUY)) != 0);
}

int A_CheckEnemySprite(const void *s)
{
    return(A_CheckEnemyTile(((uspriteptr_t)s)->picnum));
}

int A_CheckSwitchTile(int i)
{
    int j;
    //MULTISWITCH has 4 states so deal with it separately
    if ((PN(i) >= MULTISWITCH) && (PN(i) <=MULTISWITCH+3)) return 1;
    // ACCESSSWITCH and ACCESSSWITCH2 are only active in 1 state so deal with them separately
    if ((PN(i) == ACCESSSWITCH) || (PN(i) == ACCESSSWITCH2)) return 1;
    //loop to catch both states of switches
    for (j=1;j>=0;j--)
    {
        switch (tileGetMapping(PN(i)-j))
        {
        case HANDPRINTSWITCH__:
            //case HANDPRINTSWITCH+1:
        case ALIENSWITCH__:
            //case ALIENSWITCH+1:
        case MULTISWITCH__:
            //case MULTISWITCH+1:
            //case MULTISWITCH+2:
            //case MULTISWITCH+3:
            //case ACCESSSWITCH:
            //case ACCESSSWITCH2:
        case PULLSWITCH__:
            //case PULLSWITCH+1:
        case HANDSWITCH__:
            //case HANDSWITCH+1:
        case SLOTDOOR__:
            //case SLOTDOOR+1:
        case LIGHTSWITCH__:
            //case LIGHTSWITCH+1:
        case SPACELIGHTSWITCH__:
            //case SPACELIGHTSWITCH+1:
        case SPACEDOORSWITCH__:
            //case SPACEDOORSWITCH+1:
        case FRANKENSTINESWITCH__:
            //case FRANKENSTINESWITCH+1:
        case LIGHTSWITCH2__:
            //case LIGHTSWITCH2+1:
        case POWERSWITCH1__:
            //case POWERSWITCH1+1:
        case LOCKSWITCH1__:
            //case LOCKSWITCH1+1:
        case POWERSWITCH2__:
            //case POWERSWITCH2+1:
        case DIPSWITCH__:
            //case DIPSWITCH+1:
        case DIPSWITCH2__:
            //case DIPSWITCH2+1:
        case TECHSWITCH__:
            //case TECHSWITCH+1:
        case DIPSWITCH3__:
            //case DIPSWITCH3+1:
            return 1;
        }
    }
    return 0;
}

static void G_RecordOldSpritePos(void)
{
    int statNum = 0;
    do
    {
        int spriteNum = headspritestat[statNum++];

        while (spriteNum >= 0)
        {
            int const nextSprite = nextspritestat[spriteNum];
            actor[spriteNum].bpos = sprite[spriteNum].xyz;

            spriteNum = nextSprite;
        }
    } while (statNum < MAXSTATUS);
}

void G_MoveWorld(void)
{
    X_OnEvent(EVENT_PREWORLD, -1, -1, 0);

    if (VM_HaveEvent(EVENT_PREGAME))
    {
        int i, p, j, k = MAXSTATUS - 1, pl;

        do
        {
            i = headspritestat[k];

            while (i >= 0)
            {
                j = nextspritestat[i];

                if (A_CheckSpriteFlags(i, SFLAG_NOEVENTCODE))
                {
                    i = j;
                    continue;
                }

                pl = A_FindPlayer(&sprite[i], &p);
                X_OnEvent(EVENT_PREGAME, i, pl, p);
                i = j;
            }
        } while (k--);
    }

    G_RecordOldSpritePos();

#ifdef POLYMER
    G_DoEffectorLights();
#endif

    G_MoveZombieActors();     //ST 2
    G_MoveWeapons();          //ST 4

    G_MovePlayers();          //ST 10

    G_MoveFallers();          //ST 12
    G_MoveMisc();             //ST 5

    G_MoveActors();           //ST 1
    G_MoveEffectors();        //ST 3

    // [JM] Ideally, anything that moves another object should be after everything else
    // to prevent rubber-banding BS and jitter, so that's why these have been moved.
    G_MoveStandables();       //ST 6
    G_MoveTransports();       //ST 9

    X_OnEvent(EVENT_WORLD, -1, -1, 0);

    if (VM_HaveEvent(EVENT_GAME))
    {
        int i, p, j, k = MAXSTATUS-1, pl;

        do
        {
            i = headspritestat[k];

            while (i >= 0)
            {
                j = nextspritestat[i];

                if (A_CheckSpriteFlags(i, SFLAG_NOEVENTCODE))
                {
                    i = j;
                    continue;
                }

                pl = A_FindPlayer(&sprite[i],&p);
                X_OnEvent(EVENT_GAME,i, pl, p);
                i = j;
            }
        }
        while (k--);
    }

    G_RefreshLights();
    G_DoSectorAnimations();
    G_MoveFX();               //ST 11
}

