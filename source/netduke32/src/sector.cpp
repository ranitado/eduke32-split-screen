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

// PRIMITIVE

static int haltsoundhack;

int S_FindMusicSFX(int sectNum, int* sndptr)
{
    for (bssize_t SPRITES_OF_SECT(sectNum, spriteNum))
    {
        const int32_t snd = sprite[spriteNum].lotag;
        EDUKE32_STATIC_ASSERT(MAXSOUNDS >= 1000);

        if (PN(spriteNum) == MUSICANDSFX && snd && (unsigned)snd < 1000 && S_SoundIsValid(snd))  // XXX: in other places, 999
        {
            *sndptr = snd;
            return spriteNum;
        }
    }

    *sndptr = -1;
    return -1;
}

// this function activates a sector's MUSICANDSFX sprite
int A_CallSound(int sectNum, int spriteNum)
{
    if (haltsoundhack)
    {
        haltsoundhack = 0;
        return -1;
    }

    int soundNum;
    int const SFXsprite = S_FindMusicSFX(sectNum, &soundNum);

    if (SFXsprite >= 0 && soundNum)
    {
        auto const snd = g_sounds[soundNum];
        int32_t const oldFlags = snd->flags;
        if (spriteNum == -1)
            spriteNum = SFXsprite;

        if (T1(SFXsprite) == 0)
        {
            if ((snd->flags & (SF_GLOBAL | SF_DTAG)) != SF_GLOBAL)
            {
                if (snd->flags & SF_LOOP)
                    snd->flags |= SF_ONEINST_INTERNAL;

                A_PlaySound(soundNum, spriteNum);
                snd->flags = oldFlags;

                if (SHT(SFXsprite) && soundNum != SHT(SFXsprite) && SHT(SFXsprite) <= g_highestSoundIdx)
                    S_StopEnvSound(SHT(SFXsprite), T6(SFXsprite));

                T6(SFXsprite) = spriteNum;

                if ((sector[SECT(SFXsprite)].lotag & 0xff) != ST_22_SPLITTING_DOOR)
                    T1(SFXsprite) = 1;
            }
        }
        else if (SHT(SFXsprite) <= g_highestSoundIdx)
        {
            if (SHT(SFXsprite))
            {
                if (snd->flags & SF_LOOP)
                    snd->flags |= SF_ONEINST_INTERNAL;

                A_PlaySound(SHT(SFXsprite), spriteNum);
                snd->flags = oldFlags;
            }

            if ((snd->flags & SF_LOOP) || (SHT(SFXsprite) && SHT(SFXsprite) != soundNum))
                S_StopEnvSound(soundNum, T6(SFXsprite));

            T6(SFXsprite) = spriteNum;
            T1(SFXsprite) = 0;
        }

        return soundNum;
    }

    return -1;
}

int check_activator_motion(int lotag)
{
    int i, j;
    spritetype *s;

    i = headspritestat[STAT_ACTIVATOR];
    while (i >= 0)
    {
        if (sprite[i].lotag == lotag)
        {
            s = &sprite[i];

            for (j = g_animateCount-1; j >= 0; j--)
                if (s->sectnum == sectorAnims[j].target)
                    return(1);

            j = headspritestat[STAT_EFFECTOR];
            while (j >= 0)
            {
                if (s->sectnum == sprite[j].sectnum)
                    switch (sprite[j].lotag)
                    {
                    case 11:
                    case 30:
                        if (actor[j].t_data[4])
                            return(1);
                        break;
                    case 20:
                    case 31:
                    case 32:
                    case 18:
                        if (actor[j].t_data[0])
                            return(1);
                        break;
                    }

                j = nextspritestat[j];
            }
        }
        i = nextspritestat[i];
    }
    return(0);
}

int CheckDoorTile(int dapic)
{
    switch (tileGetMapping(dapic))
    {
        case DOORTILE1__:
        case DOORTILE2__:
        case DOORTILE3__:
        case DOORTILE4__:
        case DOORTILE5__:
        case DOORTILE6__:
        case DOORTILE7__:
        case DOORTILE8__:
        case DOORTILE9__:
        case DOORTILE10__:
        case DOORTILE11__:
        case DOORTILE12__:
        case DOORTILE14__:
        case DOORTILE15__:
        case DOORTILE16__:
        case DOORTILE17__:
        case DOORTILE18__:
        case DOORTILE19__:
        case DOORTILE20__:
        case DOORTILE21__:
        case DOORTILE22__:
        case DOORTILE23__:
            return 1;
    }
    return 0;
}

int isanunderoperator(int lotag)
{
    switch (lotag&0xff)
    {
    case ST_15_WARP_ELEVATOR:
    case ST_16_PLATFORM_DOWN:
    case ST_17_PLATFORM_UP:
    case ST_18_ELEVATOR_DOWN:
    case ST_19_ELEVATOR_UP:
    case ST_22_SPLITTING_DOOR:
    case ST_26_SPLITTING_ST_DOOR:
        return 1;
    }
    return 0;
}

int isanearoperator(int lotag)
{
    switch (lotag&0xff)
    {
    case ST_9_SLIDING_ST_DOOR:
    case ST_15_WARP_ELEVATOR:
    case ST_16_PLATFORM_DOWN:
    case ST_17_PLATFORM_UP:
    case ST_18_ELEVATOR_DOWN:
    case ST_19_ELEVATOR_UP:
    case ST_20_CEILING_DOOR:
    case ST_21_FLOOR_DOOR:
    case ST_22_SPLITTING_DOOR:
    case ST_23_SWINGING_DOOR:
    case ST_25_SLIDING_DOOR:
    case ST_26_SPLITTING_ST_DOOR:
    case ST_29_TEETH_DOOR://Toothed door
        return 1;
    }
    return 0;
}

int CheckPlayerInSector(int sect)
{
    for (int32_t ALL_PLAYERS(i))
        if (sprite[g_player[i].ps->i].sectnum == sect) return i;
    return -1;
}

static inline int32_t A_FP_ManhattanDist(const DukePlayer_t* pPlayer, const spritetype* pSprite)
{
    return klabs(pPlayer->opos.x - pSprite->x)
        + klabs(pPlayer->opos.y - pSprite->y)
        + ((klabs(pPlayer->opos.z - pSprite->z + (28 << 8))) >> 4);
}

int __fastcall A_FindPlayer(spritetype *s, int *d)
{
    if (ud.multimode < 2)
    {
        auto const pPlayer = g_player[myconnectindex].ps;

        if(d)
            *d = A_FP_ManhattanDist(pPlayer, s);

        return myconnectindex;
    }

    {
        int j, closest_player = 0;
        int x, closest = 0x7fffffff;

        for (ALL_PLAYERS(j))
        {
            auto const pPlayer = g_player[j].ps;
            x = A_FP_ManhattanDist(pPlayer, s);
            if (x < closest && sprite[pPlayer->i].extra > 0)
            {
                closest_player = j;
                closest = x;
            }
        }

        if(d)
            *d = closest;

        return closest_player;
    }
}

int32_t* G_GetAnimationPointer(int32_t index)
{
    int32_t target = sectorAnims[index].target;
    switch (sectorAnims[index].type)
    {
    case SECTANIM_FLOOR:    return &sector[target].floorz;
    case SECTANIM_CEILING:  return &sector[target].ceilingz;
    case SECTANIM_WALLX:    return &wall[target].x;
    case SECTANIM_WALLY:    return &wall[target].y;
    }
    
    return NULL;
}

static bool G_IsAnimatingSector(int32_t index)
{
    switch (sectorAnims[index].type)
    {
    case SECTANIM_FLOOR:
    case SECTANIM_CEILING:
        return true;
    case SECTANIM_WALLX:
    case SECTANIM_WALLY:
        return false;
    }

    return false;
}

void G_DoSectorAnimations(void)
{
    int i, j, a, p, v, datarget;

    for (i=g_animateCount-1;i>=0;i--)
    {
        datarget = sectorAnims[i].target;
        int32_t* ptr = G_GetAnimationPointer(i);

        assert(ptr != NULL);

        a = *ptr;
        v = sectorAnims[i].vel*TICSPERFRAME;

        if (a == sectorAnims[i].goal)
        {
            G_StopInterpolation(ptr);

            g_animateCount--;
            sectorAnims[i].type = sectorAnims[g_animateCount].type; //animateptr[i] = animateptr[g_animateCount];
            sectorAnims[i].goal = sectorAnims[g_animateCount].goal; //animategoal[i] = animategoal[g_animateCount];
            sectorAnims[i].vel = sectorAnims[g_animateCount].vel; //animatevel[i] = animatevel[g_animateCount];
            sectorAnims[i].target = sectorAnims[g_animateCount].target; //animatesect[i] = animatesect[g_animateCount];

            if(G_IsAnimatingSector(i)) // Only do this on sectors, not walls.
            {
                // This fixes a bug where wall or floor sprites contained in
                // elevator sectors (ST 16-19) would jitter vertically after the
                // elevator had stopped.
                if (ptr == &sector[datarget].floorz)
                    for (j = headspritesect[datarget]; j >= 0; j = nextspritesect[j])
                        if (sprite[j].statnum != 3)
                            actor[j].bpos.z = sprite[j].z;

                if (sector[datarget].lotag == ST_18_ELEVATOR_DOWN || sector[datarget].lotag == ST_19_ELEVATOR_UP)
                    if (ptr == &sector[datarget].ceilingz)
                        continue;
            }
            else
            {
                datarget = sectorofwall(datarget);
            }

            if (datarget != -1 && ((sector[datarget].lotag & 0xff) != ST_22_SPLITTING_DOOR))
                A_CallSound(datarget, -1);

            continue;
        }

        if (v > 0)
        {
            a = min(a+v, sectorAnims[i].goal);
        }
        else
        {
            a = max(a+v, sectorAnims[i].goal);
        }

        if (G_IsAnimatingSector(i)) // Only do this on sectors, not walls.
        {
            if (ptr == &sector[datarget].floorz)
            {
                for (ALL_PLAYERS(p))
                    if (g_player[p].ps->cursectnum == datarget)
                        if ((sector[datarget].floorz - g_player[p].ps->pos.z) < (64 << 8))
                            if (sprite[g_player[p].ps->i].owner >= 0)
                            {
                                g_player[p].ps->pos.z += v;
                                g_player[p].ps->vel.z = 0;
                            }

                for (j = headspritesect[datarget]; j >= 0; j = nextspritesect[j])
                    if (sprite[j].statnum != 3)
                    {
                        actor[j].bpos.z = sprite[j].z;
                        sprite[j].z += v;
                        actor[j].floorz = sector[datarget].floorz + v;
                    }
            }
        }

        *ptr = a;
    }
}

int GetAnimationGoal(int16_t target, int8_t type)
{
    int i = g_animateCount-1, j = -1;

    for (; i >= 0; i--)
        if ((target == sectorAnims[i].target) && (type == sectorAnims[i].type))
        {
            j = i;
            break;
        }

    return(j);
}

int SetAnimation(int16_t target, int8_t type, int thegoal, int thevel)
{
    int i = 0, j = g_animateCount;

    if (g_animateCount >= MAXANIMATES-1)
        return(-1);

    for (;i < g_animateCount; i++)
        if ((target == sectorAnims[i].target) && (type == sectorAnims[i].type))
        {
            j = i;
            break;
        }

    sectorAnims[j].target = target;
    sectorAnims[j].type = type;
    sectorAnims[j].goal = thegoal;

    int32_t* ptr = G_GetAnimationPointer(j);

    if (thegoal >= *ptr)
        sectorAnims[j].vel = thevel;
    else
        sectorAnims[j].vel = -thevel;

    if (j == g_animateCount) g_animateCount++;

    G_SetInterpolation(ptr);

    return(j);
}

static void G_SetupCamTile(int spriteNum, int tileNum, int smoothRatio)
{
    vec3_t const camera = G_GetCameraPosition(spriteNum, smoothRatio);
    int const    saveMirror = display_mirror;

    //if (waloff[wn] == 0) loadtile(wn);
    renderSetTarget(tileNum, tilesiz[tileNum].y, tilesiz[tileNum].x);

#if 0
    int const playerNum = screenpeek;
    int const noDraw = VM_OnEventWithReturn(EVENT_DISPLAYROOMSCAMERATILE, spriteNum, playerNum, 0);

    if (noDraw == 1)
        goto finishTileSetup;
#ifdef DEBUGGINGAIDS
    else if (EDUKE32_PREDICT_FALSE(noDraw != 0)) // event return values other than 0 and 1 are reserved
        OSD_Printf(OSD_ERROR "ERROR: EVENT_DISPLAYROOMSCAMERATILE return value must be 0 or 1, "
            "other values are reserved.\n");
#endif
#endif

    yax_preparedrawrooms();
    drawrooms(camera.x, camera.y, camera.z, SA(spriteNum), 100 + sprite[spriteNum].shade, SECT(spriteNum));
    yax_drawrooms(G_DoSpriteAnimations, SECT(spriteNum), 0, smoothRatio);

    display_mirror = 3;
    G_DoSpriteAnimations(camera.x, camera.y, camera.z, SA(spriteNum), smoothRatio);
    display_mirror = saveMirror;
    renderDrawMasks();

//finishTileSetup:
    renderRestoreTarget();
    squarerotatetile(tileNum);
    tileInvalidate(tileNum, -1, 255);
}

void G_AnimateCamSprite(int smoothRatio)
{
#ifdef DEBUG_VALGRIND_NO_SMC
    return;
#endif

    if (g_curViewscreen < 0)
        return;

    int const spriteNum = g_curViewscreen;

    if (totalclock >= T1(spriteNum) + ud.camera_time)
    {
        auto const pPlayer = g_player[screenpeek].ps;

        if (pPlayer->newowner >= 0)
            OW(spriteNum) = pPlayer->newowner;

        // EDuke32 extension: xvel of viewscreen determines active distance
        int const activeDist = sprite[spriteNum].xvel > 0 ? sprite[spriteNum].xvel : VIEWSCREEN_ACTIVE_DISTANCE;

        if (OW(spriteNum) >= 0 && dist(&sprite[pPlayer->i], &sprite[spriteNum]) < activeDist)
        {
            int const viewscrShift = G_GetViewscreenSizeShift((uspriteptr_t)&sprite[spriteNum]);
            int const viewscrTile = TILE_VIEWSCR - viewscrShift;

            if (waloff[viewscrTile] == 0)
                tileCreate(viewscrTile, tilesiz[PN(spriteNum)].x << viewscrShift, tilesiz[PN(spriteNum)].y << viewscrShift);
            else
                walock[viewscrTile] = CACHE1D_UNLOCKED;

            G_SetupCamTile(OW(spriteNum), viewscrTile, smoothRatio);
        }

        T1(spriteNum) = (int32_t)totalclock;
    }
}

void G_AnimateWalls(void)
{
    int i, j, t;

    if (ud.pause_on) // Halt anims on pause.
        return;

    // Use randomseed to contribute randomness to our anims, but let's not cycle it.
    g_random.animwall += randomseed;

    for (int p = g_numAnimWalls - 1; p >= 0; p--)
    {
        i = animwall[p].wallnum;
        j = wall[i].picnum;

        switch (tileGetMapping(j))
        {
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

            if ((seed_krand(&g_random.animwall)&255) < 16)
            {
                animwall[p].tag = wall[i].picnum;
                wall[i].picnum = SCREENBREAK6;
            }

            continue;

        case SCREENBREAK6__:
        case SCREENBREAK7__:
        case SCREENBREAK8__:

            if (animwall[p].tag >= 0 && wall[i].extra != FEMPIC2 && wall[i].extra != FEMPIC3)
                wall[i].picnum = animwall[p].tag;
            else
            {
                wall[i].picnum++;
                if (wall[i].picnum == (SCREENBREAK6+3))
                    wall[i].picnum = SCREENBREAK6;
            }
            continue;

        }

        if (wall[i].cstat&16)
            if ((wall[i].overpicnum >= W_FORCEFIELD)&&(wall[i].overpicnum <= W_FORCEFIELD+2))
            {

                t = animwall[p].tag;

                if (wall[i].cstat&254)
                {
                    wall[i].xpanning -= t>>10; // sintable[(t+512)&2047]>>12;
                    wall[i].ypanning -= t>>10; // sintable[t&2047]>>12;

                    if (wall[i].extra == 1)
                    {
                        wall[i].extra = 0;
                        animwall[p].tag = 0;
                    }
                    else
                        animwall[p].tag+=128;

                    if (animwall[p].tag < (128<<4))
                    {
                        if (animwall[p].tag&128)
                            wall[i].overpicnum = W_FORCEFIELD;
                        else wall[i].overpicnum = W_FORCEFIELD+1;
                    }
                    else
                    {
                        if ((seed_krand(&g_random.animwall) & 255) < 32)
                            animwall[p].tag = 128 << (seed_krand(&g_random.animwall) & 3);
                        else wall[i].overpicnum = W_FORCEFIELD+1;
                    }
                }

            }
    }
}

int G_ActivateWarpElevators(int s,int d) //Parm = sectoreffectornum
{
    int i = headspritestat[STAT_EFFECTOR], sn = sprite[s].sectnum;

    while (i >= 0)
    {
        if (SLT(i) == 17)
            if (SHT(i) == sprite[s].hitag)
                if ((klabs(sector[sn].floorz-actor[s].t_data[2]) > SP(i)) ||
                        (sector[SECT(i)].hitag == (sector[sn].hitag-d)))
                    break;
        i = nextspritestat[i];
    }

    if (i==-1)
    {
        d = 0;
        return 1; // No find
    }
    else
    {
        if (d == 0)
            A_PlaySound(ELEVATOR_OFF,s);
        else A_PlaySound(ELEVATOR_ON,s);

        // HACK!
        sector[SECT(i)].floorz = sector[sn].floorz;
        sector[SECT(i)].ceilingz = sector[sn].ceilingz;
    }


    i = headspritestat[STAT_EFFECTOR];
    while (i >= 0)
    {
        if (SLT(i) == 17)
            if (SHT(i) == sprite[s].hitag)
            {
                T1(i) = d;
                T2(i) = d; //Make all check warp
            }
        i = nextspritestat[i];
    }
    return 0;
}

void G_OperateSectors(int sn,int ii)
{
    int j=0, l, q, startwall, endwall;
    int i;
    sectortype *sptr = &sector[sn];

    switch (sptr->lotag & (uint16_t)~49152u)
    {

    case ST_30_ROTATE_RISE_BRIDGE:
        j = sector[sn].hitag;

        if (E_SpriteIsValid(j))
        {
            if (actor[j].tempang == 0 || actor[j].tempang == 256)
                A_CallSound(sn, ii);
            sprite[j].extra = (sprite[j].extra == 1) ? 3 : 1;
        }
        break;

    case ST_31_TWO_WAY_TRAIN:

        j = sector[sn].hitag;
        if (actor[j].t_data[4] == 0)
            actor[j].t_data[4] = 1;

        A_CallSound(sn,ii);
        break;

    case ST_26_SPLITTING_ST_DOOR: //The split doors
        i = GetAnimationGoal(sn, SECTANIM_CEILING);
        if (i == -1) //if the door has stopped
        {
            haltsoundhack = 1;
            sptr->lotag &= 0xff00;
            sptr->lotag |= ST_22_SPLITTING_DOOR;
            G_OperateSectors(sn,ii);
            sptr->lotag &= 0xff00;
            sptr->lotag |= ST_9_SLIDING_ST_DOOR;
            G_OperateSectors(sn,ii);
            sptr->lotag &= 0xff00;
            sptr->lotag |= ST_26_SPLITTING_ST_DOOR;
        }
        return;

    case ST_9_SLIDING_ST_DOOR:
    {
        int dax,day,dax2,day2,sp;
        int wallfind[2];

        startwall = sptr->wallptr;
        endwall = startwall+sptr->wallnum-1;

        sp = sptr->extra>>4;

        //first find center point by averaging all points
        dax = 0L, day = 0L;
        for (i=startwall;i<=endwall;i++)
        {
            dax += wall[i].x;
            day += wall[i].y;
        }
        dax /= (endwall-startwall+1);
        day /= (endwall-startwall+1);

        //find any points with either same x or same y coordinate
        //  as center (dax, day) - should be 2 points found.
        wallfind[0] = -1;
        wallfind[1] = -1;
        for (i=startwall;i<=endwall;i++)
            if ((wall[i].x == dax) || (wall[i].y == day))
            {
                if (wallfind[0] == -1)
                    wallfind[0] = i;
                else wallfind[1] = i;
            }

        for (j=0;j<2;j++)
        {
            if ((wall[wallfind[j]].x == dax) && (wall[wallfind[j]].y == day))
            {
                //find what direction door should open by averaging the
                //  2 neighboring points of wallfind[0] & wallfind[1].
                i = wallfind[j]-1;
                if (i < startwall) i = endwall;
                dax2 = ((wall[i].x+wall[wall[wallfind[j]].point2].x)>>1)-wall[wallfind[j]].x;
                day2 = ((wall[i].y+wall[wall[wallfind[j]].point2].y)>>1)-wall[wallfind[j]].y;
                if (dax2 != 0)
                {
                    dax2 = wall[wall[wall[wallfind[j]].point2].point2].x;
                    dax2 -= wall[wall[wallfind[j]].point2].x;

                    SetAnimation(wallfind[j], SECTANIM_WALLX, wall[wallfind[j]].x + dax2, sp);
                    SetAnimation(i, SECTANIM_WALLX, wall[i].x + dax2, sp);
                    SetAnimation(wall[wallfind[j]].point2, SECTANIM_WALLX, wall[wall[wallfind[j]].point2].x + dax2, sp);

                    A_CallSound(sn,ii);
                }
                else if (day2 != 0)
                {
                    day2 = wall[wall[wall[wallfind[j]].point2].point2].y;
                    day2 -= wall[wall[wallfind[j]].point2].y;

                    SetAnimation(wallfind[j], SECTANIM_WALLY, wall[wallfind[j]].y + day2, sp);
                    SetAnimation(i, SECTANIM_WALLY, wall[i].y + day2, sp);
                    SetAnimation(wall[wallfind[j]].point2, SECTANIM_WALLY, wall[wall[wallfind[j]].point2].y + day2, sp);

                    A_CallSound(sn,ii);
                }
            }
            else
            {
                i = wallfind[j]-1;
                if (i < startwall) i = endwall;
                dax2 = ((wall[i].x+wall[wall[wallfind[j]].point2].x)>>1)-wall[wallfind[j]].x;
                day2 = ((wall[i].y+wall[wall[wallfind[j]].point2].y)>>1)-wall[wallfind[j]].y;
                if (dax2 != 0)
                {
                    SetAnimation(wallfind[j], SECTANIM_WALLX, dax, sp);
                    SetAnimation(i, SECTANIM_WALLX, dax + dax2, sp);
                    SetAnimation(wall[wallfind[j]].point2, SECTANIM_WALLX, dax + dax2, sp);

                    A_CallSound(sn,ii);
                }
                else if (day2 != 0)
                {
                    SetAnimation(wallfind[j], SECTANIM_WALLY, day, sp);
                    SetAnimation(i, SECTANIM_WALLY, day + day2, sp);
                    SetAnimation(wall[wallfind[j]].point2, SECTANIM_WALLY, day + day2, sp);

                    A_CallSound(sn,ii);
                }
            }
        }

    }
    return;

    case ST_15_WARP_ELEVATOR://Warping elevators

        if (sprite[ii].picnum != APLAYER) return;
        //            if(ps[sprite[ii].yvel].select_dir == 1) return;

        i = headspritesect[sn];
        while (i >= 0)
        {
            if (PN(i)==SECTOREFFECTOR && SLT(i) == 17) break;
            i = nextspritesect[i];
        }

        if (sprite[ii].sectnum == sn)
        {
            if (G_ActivateWarpElevators(i,-1))
                G_ActivateWarpElevators(i,1);
            else if (G_ActivateWarpElevators(i,1))
                G_ActivateWarpElevators(i,-1);
            return;
        }
        else
        {
            if (sptr->floorz > SZ(i))
                G_ActivateWarpElevators(i,-1);
            else
                G_ActivateWarpElevators(i,1);
        }

        return;

    case ST_16_PLATFORM_DOWN:
    case ST_17_PLATFORM_UP:

        i = GetAnimationGoal(sn, SECTANIM_FLOOR);

        if (i == -1)
        {
            i = nextsectorneighborz(sn,sptr->floorz,1,1);
            if (i == -1)
            {
                i = nextsectorneighborz(sn,sptr->floorz,1,-1);
                if (i == -1) return;
                j = sector[i].floorz;
                SetAnimation(sn, SECTANIM_FLOOR,j,sptr->extra);
            }
            else
            {
                j = sector[i].floorz;
                SetAnimation(sn, SECTANIM_FLOOR,j,sptr->extra);
            }
            A_CallSound(sn,ii);
        }

        return;

    case ST_18_ELEVATOR_DOWN:
    case ST_19_ELEVATOR_UP:

        i = GetAnimationGoal(sn, SECTANIM_FLOOR);

        if (i==-1)
        {
            i = nextsectorneighborz(sn,sptr->floorz,1,-1);
            if (i==-1) i = nextsectorneighborz(sn,sptr->floorz,1,1);
            if (i==-1) return;
            j = sector[i].floorz;
            q = sptr->extra;
            l = sptr->ceilingz-sptr->floorz;
            SetAnimation(sn, SECTANIM_FLOOR,j,q);
            SetAnimation(sn, SECTANIM_CEILING,j+l,q);
            A_CallSound(sn,ii);
        }
        return;

    case ST_29_TEETH_DOOR:

        if (sptr->lotag&0x8000)
            j = sector[nextsectorneighborz(sn,sptr->ceilingz,1,1)].floorz;
        else
            j = sector[nextsectorneighborz(sn,sptr->ceilingz,-1,-1)].ceilingz;

        i = headspritestat[STAT_EFFECTOR]; //Effectors
        while (i >= 0)
        {
            if ((SLT(i) == 22) &&
                    (SHT(i) == sptr->hitag))
            {
                sector[SECT(i)].extra = -sector[SECT(i)].extra;

                T1(i) = sn;
                T2(i) = 1;
            }
            i = nextspritestat[i];
        }

        sptr->lotag ^= 0x8000;

        SetAnimation(sn, SECTANIM_CEILING,j,sptr->extra);

        A_CallSound(sn,ii);

        return;

    case ST_20_CEILING_DOOR:

REDODOOR:

        if (sptr->lotag&0x8000)
        {
            i = headspritesect[sn];
            while (i >= 0)
            {
                if (sprite[i].statnum == 3 && SLT(i)==9)
                {
                    j = SZ(i);
                    break;
                }
                i = nextspritesect[i];
            }
            if (i==-1)
                j = sptr->floorz;
        }
        else
        {
            j = nextsectorneighborz(sn,sptr->ceilingz,-1,-1);

            if (j >= 0) j = sector[j].ceilingz;
            else
            {
                sptr->lotag |= 32768;
                goto REDODOOR;
            }
        }

        sptr->lotag ^= 0x8000;

        SetAnimation(sn, SECTANIM_CEILING,j,sptr->extra);
        A_CallSound(sn,ii);

        return;

    case ST_21_FLOOR_DOOR:
        i = GetAnimationGoal(sn, SECTANIM_FLOOR);
        if (i >= 0)
        {
            if (sectorAnims[i].goal == sptr->ceilingz)
                sectorAnims[i].goal = sector[nextsectorneighborz(sn,sptr->ceilingz,1,1)].floorz;
            else sectorAnims[i].goal = sptr->ceilingz;
            j = sectorAnims[i].goal;
        }
        else
        {
            if (sptr->ceilingz == sptr->floorz)
                j = sector[nextsectorneighborz(sn,sptr->ceilingz,1,1)].floorz;
            else j = sptr->ceilingz;

            sptr->lotag ^= 0x8000;

            if (SetAnimation(sn, SECTANIM_FLOOR,j,sptr->extra) >= 0)
                A_CallSound(sn,ii);
        }
        return;

    case ST_22_SPLITTING_DOOR:

        // REDODOOR22:

        if ((sptr->lotag&0x8000))
        {
            q = (sptr->ceilingz+sptr->floorz)>>1;
            j = SetAnimation(sn, SECTANIM_FLOOR,q,sptr->extra);
            j = SetAnimation(sn, SECTANIM_CEILING,q,sptr->extra);
        }
        else
        {
            q = sector[nextsectorneighborz(sn,sptr->floorz,1,1)].floorz;
            j = SetAnimation(sn, SECTANIM_FLOOR,q,sptr->extra);
            q = sector[nextsectorneighborz(sn,sptr->ceilingz,-1,-1)].ceilingz;
            j = SetAnimation(sn, SECTANIM_CEILING,q,sptr->extra);
        }

        sptr->lotag ^= 0x8000;

        A_CallSound(sn,ii);

        return;

    case ST_23_SWINGING_DOOR: //Swingdoor
    {
        j = -1;

        for (SPRITES_OF(STAT_EFFECTOR, i))
            if (SLT(i) == SE_11_SWINGING_DOOR && SECT(i) == sn && !T5(i))
            {
                j = i;
                break;
            }

        if (i < 0)
            return;

        uint16_t const tag = sector[SECT(i)].lotag & 0x8000u;

        if (j >= 0)
        {
            for (SPRITES_OF(STAT_EFFECTOR, i))
                if (tag == (sector[SECT(i)].lotag & 0x8000u) && SLT(i) == SE_11_SWINGING_DOOR && sprite[j].hitag == SHT(i) && (T5(i) || T6(i)))
                    return;

            bool soundPlayed = false;
            for (SPRITES_OF(STAT_EFFECTOR, i))
            {
                if (tag == (sector[SECT(i)].lotag & 0x8000u) && SLT(i) == SE_11_SWINGING_DOOR && sprite[j].hitag == SHT(i))
                {
                    if (sector[SECT(i)].lotag & 0x8000u) sector[SECT(i)].lotag &= 0x7fff;
                    else sector[SECT(i)].lotag |= 0x8000u;

                    T5(i) = 1;
                    T4(i) = -T4(i);

                    if (!soundPlayed)
                    {
                        A_CallSound(sn, i);
                        soundPlayed = true;
                    }
                }
            }
        }
        return;
    }
    case ST_25_SLIDING_DOOR: //Subway type sliding doors

        j = headspritestat[STAT_EFFECTOR];
        while (j >= 0)//Find the sprite
        {
            if ((sprite[j].lotag) == 15 && sprite[j].sectnum == sn)
                break; //Found the sectoreffector.
            j = nextspritestat[j];
        }

        if (j < 0)
            return;

        i = headspritestat[STAT_EFFECTOR];
        while (i >= 0)
        {
            if (SHT(i)==sprite[j].hitag)
            {
                if (SLT(i) == 15)
                {
                    sector[SECT(i)].lotag ^= 0x8000; // Toggle the open or close
                    SA(i) += 1024;
                    if (T5(i)) A_CallSound(SECT(i),i);
                    A_CallSound(SECT(i),i);
                    if (sector[SECT(i)].lotag&0x8000) T5(i) = 1;
                    else T5(i) = 2;
                }
            }
            i = nextspritestat[i];
        }
        return;

    case ST_27_STRETCH_BRIDGE:  //Extended bridge

        j = headspritestat[STAT_EFFECTOR];
        while (j >= 0)
        {
            if ((sprite[j].lotag&0xff)==20 && sprite[j].sectnum == sn)  //Bridge
            {

                sector[sn].lotag ^= 0x8000;
                if (sector[sn].lotag&0x8000) //OPENING
                    actor[j].t_data[0] = 1;
                else actor[j].t_data[0] = 2;
                A_CallSound(sn,ii);
                break;
            }
            j = nextspritestat[j];
        }
        return;


    case ST_28_DROP_FLOOR:
        //activate the rest of them

        j = headspritesect[sn];
        while (j >= 0)
        {
            if (sprite[j].statnum==3 && (sprite[j].lotag&0xff)==21)
                break; //Found it
            j = nextspritesect[j];
        }

        j = sprite[j].hitag;

        l = headspritestat[STAT_EFFECTOR];
        while (l >= 0)
        {
            if ((sprite[l].lotag&0xff)==21 && !actor[l].t_data[0] &&
                    (sprite[l].hitag) == j)
                actor[l].t_data[0] = 1;
            l = nextspritestat[l];
        }
        A_CallSound(sn,ii);

        return;
    }
}

void G_OperateRespawns(int low)
{
    int j, nexti, i = headspritestat[STAT_FX];

    while (i >= 0)
    {
        nexti = nextspritestat[i];
        if ((SLT(i) == low) && (PN(i) == RESPAWN))
        {
            if (A_CheckEnemyTile(SHT(i)) && DMFLAGS_TEST(DMFLAG_NOMONSTERS)) break;

            j = A_Spawn(i,TRANSPORTERSTAR);
            sprite[j].z -= (32<<8);

            sprite[i].extra = 66-12;   // Just a way to killit
        }
        i = nexti;
    }
}

void G_OperateActivators(int lotag, int playerNum)
{
    for (int spriteNum = g_numCyclers - 1; spriteNum >= 0; spriteNum--)
    {
        int16_t* const pCycler = &cyclers[spriteNum][0];

        if (pCycler[4] == lotag)
        {
            pCycler[5] = !pCycler[5];
            sector[pCycler[0]].floorshade = pCycler[3];
            sector[pCycler[0]].ceilingshade = pCycler[3];
            walltype* pWall = &wall[sector[pCycler[0]].wallptr];

            for (int j = sector[pCycler[0]].wallnum; j > 0; j--, pWall++)
                pWall->shade = pCycler[3];
        }
    }

    int soundPlayed = -1;

    for (bssize_t nextSprite, SPRITES_OF_STAT_SAFE(STAT_ACTIVATOR, spriteNum, nextSprite))
    {
        if (sprite[spriteNum].lotag == lotag)
        {
            if (sprite[spriteNum].picnum == ACTIVATORLOCKED)
            {
                sector[SECT(spriteNum)].lotag ^= 16384;

                if (playerNum >= 0 && playerNum < ud.multimode)
                    P_DoQuote((sector[SECT(spriteNum)].lotag & 16384) ? QUOTE_LOCKED : QUOTE_UNLOCKED, g_player[playerNum].ps);
            }
            else
            {
                switch (SHT(spriteNum))
                {
                case 1:
                    if (sector[SECT(spriteNum)].floorz != sector[SECT(spriteNum)].ceilingz)
                        continue;
                    break;
                case 2:
                    if (sector[SECT(spriteNum)].floorz == sector[SECT(spriteNum)].ceilingz)
                        continue;
                    break;
                }

                // ST_2_UNDERWATER
                if (sector[sprite[spriteNum].sectnum].lotag < 3)
                {
                    for (bssize_t SPRITES_OF_SECT(sprite[spriteNum].sectnum, foundSprite))
                    {
                        if (sprite[foundSprite].statnum == STAT_EFFECTOR)
                        {
                            switch (sprite[foundSprite].lotag)
                            {
                            case SE_36_PROJ_SHOOTER:
                            case SE_31_FLOOR_RISE_FALL:
                            case SE_32_CEILING_RISE_FALL:
                            case SE_18_INCREMENTAL_SECTOR_RISE_FALL:
                                actor[foundSprite].t_data[0] = 1 - actor[foundSprite].t_data[0];
                                A_CallSound(SECT(spriteNum), foundSprite);
                                break;
                            }
                        }
                    }
                }

                if (soundPlayed == -1 && (sector[SECT(spriteNum)].lotag & 0xff) == ST_22_SPLITTING_DOOR)
                    soundPlayed = A_CallSound(SECT(spriteNum), spriteNum);

                G_OperateSectors(SECT(spriteNum), spriteNum);
            }
        }
    }

    G_OperateRespawns(lotag);
}

void G_OperateMasterSwitches(int low)
{
    int i = headspritestat[STAT_STANDABLE];
    while (i >= 0)
    {
        if (PN(i) == MASTERSWITCH && SLT(i) == low && SP(i) == 0)
            SP(i) = 1;
        i = nextspritestat[i];
    }
}

void G_OperateForceFields(int s, int low)
{
    int i, p=g_numAnimWalls;

    for (;p>=0;p--)
    {
        i = animwall[p].wallnum;

        if (low == wall[i].lotag || low == -1)
            if (((wall[i].overpicnum >= W_FORCEFIELD)&&(wall[i].overpicnum <= W_FORCEFIELD+2))||(wall[i].overpicnum == BIGFORCE))
            {


                animwall[p].tag = 0;

                if (wall[i].cstat)
                {
                    wall[i].cstat   = 0;

                    if (s >= 0 && sprite[s].picnum == SECTOREFFECTOR &&
                            sprite[s].lotag == SE_30_TWO_WAY_TRAIN)
                        wall[i].lotag = 0;
                }
                else
                    wall[i].cstat = 85;
            }
    }
}

int P_ActivateSwitch(int snum,int w,int switchtype)
{
    int switchpal, switchpicnum;
    int i, x, lotag,hitag,picnum,correctdips = 1, numdips = 0;
    int sx,sy;

    if (w < 0) return 0;

    if (switchtype == 1) // A sprite
    {
        lotag = sprite[w].lotag;
        if (lotag == 0) return 0;
        hitag = sprite[w].hitag;
        sx = sprite[w].x;
        sy = sprite[w].y;
        picnum = sprite[w].picnum;
        switchpal = sprite[w].pal;
    }
    else // A wall
    {
        lotag = wall[w].lotag;
        if (lotag == 0) return 0;
        hitag = wall[w].hitag;
        sx = wall[w].x;
        sy = wall[w].y;
        picnum = wall[w].picnum;
        switchpal = wall[w].pal;
    }

    if (X_OnEventWithReturn(EVENT_ACTIVATESWITCH, w, snum, switchtype) == -1)
        return 0;

    //     LOG_F("P_ActivateSwitch called picnum=%i switchtype=%i\n",picnum,switchtype);
    switchpicnum = picnum;
    if ((picnum==DIPSWITCH+1)
            || (picnum==TECHSWITCH+1)
            || (picnum==ALIENSWITCH+1)
            || (picnum==DIPSWITCH2+1)
            || (picnum==DIPSWITCH3+1)
            || (picnum==PULLSWITCH+1)
            || (picnum==HANDSWITCH+1)
            || (picnum==SLOTDOOR+1)
            || (picnum==LIGHTSWITCH+1)
            || (picnum==SPACELIGHTSWITCH+1)
            || (picnum==SPACEDOORSWITCH+1)
            || (picnum==FRANKENSTINESWITCH+1)
            || (picnum==LIGHTSWITCH2+1)
            || (picnum==POWERSWITCH1+1)
            || (picnum==LOCKSWITCH1+1)
            || (picnum==POWERSWITCH2+1)
            || (picnum==LIGHTSWITCH+1)
       )
    {
        switchpicnum--;
    }
    if ((picnum > MULTISWITCH)&&(picnum <= MULTISWITCH+3))
    {
        switchpicnum = MULTISWITCH;
    }

    switch (tileGetMapping(switchpicnum))
    {
    case DIPSWITCH__:
        //    case DIPSWITCH+1:
    case TECHSWITCH__:
        //    case TECHSWITCH+1:
    case ALIENSWITCH__:
        //    case ALIENSWITCH+1:
        break;
    case ACCESSSWITCH__:
    case ACCESSSWITCH2__:
        if (g_player[snum].ps->access_incs == 0)
        {
            if (switchpal == 0)
            {
                if ((ud.got_access&1))
                    g_player[snum].ps->access_incs = 1;
                else P_DoQuote(QUOTE_NEED_BLUE_KEY,g_player[snum].ps);
            }

            else if (switchpal == 21)
            {
                if (ud.got_access&2)
                    g_player[snum].ps->access_incs = 1;
                else P_DoQuote(QUOTE_NEED_RED_KEY,g_player[snum].ps);
            }

            else if (switchpal == 23)
            {
                if (ud.got_access&4)
                    g_player[snum].ps->access_incs = 1;
                else P_DoQuote(QUOTE_NEED_YELLOW_KEY,g_player[snum].ps);
            }

            if (g_player[snum].ps->access_incs == 1)
            {
                if (switchtype == 0)
                    g_player[snum].ps->access_wallnum = w;
                else
                    g_player[snum].ps->access_spritenum = w;
            }

            return 0;
        }
        fallthrough__;
    case DIPSWITCH2__:
        //case DIPSWITCH2+1:
    case DIPSWITCH3__:
        //case DIPSWITCH3+1:
    case MULTISWITCH__:
        //case MULTISWITCH+1:
        //case MULTISWITCH+2:
        //case MULTISWITCH+3:
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
        if (check_activator_motion(lotag)) return 0;
        break;
    default:
        if (CheckDoorTile(picnum) == 0) return 0;
        break;
    }

    if (oldnet_predicting)
        return 0;

    i = headspritestat[STAT_DEFAULT];
    while (i >= 0)
    {

        if (lotag == SLT(i))
        {
            int switchpicnum=PN(i); // put it in a variable so later switches don't trigger on the result of changes
            if ((switchpicnum >= MULTISWITCH) && (switchpicnum <=MULTISWITCH+3))
            {
                sprite[i].picnum++;
                if (sprite[i].picnum > (MULTISWITCH+3))
                    sprite[i].picnum = MULTISWITCH;

            }
            switch (tileGetMapping(switchpicnum))
            {

            case DIPSWITCH__:
            case TECHSWITCH__:
            case ALIENSWITCH__:
                if (switchtype == 1 && w == i) PN(i)++;
                else if (SHT(i) == 0) correctdips++;
                numdips++;
                break;
            case ACCESSSWITCH__:
            case ACCESSSWITCH2__:
            case SLOTDOOR__:
            case LIGHTSWITCH__:
            case SPACELIGHTSWITCH__:
            case SPACEDOORSWITCH__:
            case FRANKENSTINESWITCH__:
            case LIGHTSWITCH2__:
            case POWERSWITCH1__:
            case LOCKSWITCH1__:
            case POWERSWITCH2__:
            case HANDSWITCH__:
            case PULLSWITCH__:
            case DIPSWITCH2__:
            case DIPSWITCH3__:
                sprite[i].picnum++;
                break;
            default:
                switch (tileGetMapping(switchpicnum-1))
                {

                case TECHSWITCH__:
                case DIPSWITCH__:
                case ALIENSWITCH__:
                    if (switchtype == 1 && w == i) PN(i)--;
                    else if (SHT(i) == 1) correctdips++;
                    numdips++;
                    break;
                case PULLSWITCH__:
                case HANDSWITCH__:
                case LIGHTSWITCH2__:
                case POWERSWITCH1__:
                case LOCKSWITCH1__:
                case POWERSWITCH2__:
                case SLOTDOOR__:
                case LIGHTSWITCH__:
                case SPACELIGHTSWITCH__:
                case SPACEDOORSWITCH__:
                case FRANKENSTINESWITCH__:
                case DIPSWITCH2__:
                case DIPSWITCH3__:
                    sprite[i].picnum--;
                    break;
                }
                break;
            }
        }
        i = nextspritestat[i];
    }

    for (i=numwalls-1;i>=0;i--)
    {
        x = i;
        if (lotag == wall[x].lotag)
        {
            if ((wall[x].picnum >= MULTISWITCH) && (wall[x].picnum <=MULTISWITCH+3))
            {
                wall[x].picnum++;
                if (wall[x].picnum > (MULTISWITCH+3))
                    wall[x].picnum = MULTISWITCH;

            }
            switch (tileGetMapping(wall[x].picnum))
            {
            case DIPSWITCH__:
            case TECHSWITCH__:
            case ALIENSWITCH__:
                if (switchtype == 0 && i == w) wall[x].picnum++;
                else if (wall[x].hitag == 0) correctdips++;
                numdips++;
                break;
            case ACCESSSWITCH__:
            case ACCESSSWITCH2__:
            case SLOTDOOR__:
            case LIGHTSWITCH__:
            case SPACELIGHTSWITCH__:
            case SPACEDOORSWITCH__:
            case FRANKENSTINESWITCH__:
            case LIGHTSWITCH2__:
            case POWERSWITCH1__:
            case LOCKSWITCH1__:
            case POWERSWITCH2__:
            case HANDSWITCH__:
            case PULLSWITCH__:
            case DIPSWITCH2__:
            case DIPSWITCH3__:
                wall[x].picnum++;
                break;
            default:
                switch (tileGetMapping(wall[x].picnum-1))
                {

                case TECHSWITCH__:
                case DIPSWITCH__:
                case ALIENSWITCH__:
                    if (switchtype == 0 && i == w) wall[x].picnum--;
                    else if (wall[x].hitag == 1) correctdips++;
                    numdips++;
                    break;
                case PULLSWITCH__:
                case HANDSWITCH__:
                case LIGHTSWITCH2__:
                case POWERSWITCH1__:
                case LOCKSWITCH1__:
                case POWERSWITCH2__:
                case SLOTDOOR__:
                case LIGHTSWITCH__:
                case SPACELIGHTSWITCH__:
                case SPACEDOORSWITCH__:
                case FRANKENSTINESWITCH__:
                case DIPSWITCH2__:
                case DIPSWITCH3__:
                    wall[x].picnum--;
                    break;
                }
                break;
            }
        }
    }

    if ((lotag == (short)65535) && !DMFLAGS_TEST(DMFLAG_NOEXITS))
    {
        Net_EndOfLevel(false);
        return 1;
    }

    switchpicnum = picnum;

    if ((picnum==DIPSWITCH+1)
            || (picnum==TECHSWITCH+1)
            || (picnum==ALIENSWITCH+1)
            || (picnum==DIPSWITCH2+1)
            || (picnum==DIPSWITCH3+1)
            || (picnum==PULLSWITCH+1)
            || (picnum==HANDSWITCH+1)
            || (picnum==SLOTDOOR+1)
            || (picnum==LIGHTSWITCH+1)
            || (picnum==SPACELIGHTSWITCH+1)
            || (picnum==SPACEDOORSWITCH+1)
            || (picnum==FRANKENSTINESWITCH+1)
            || (picnum==LIGHTSWITCH2+1)
            || (picnum==POWERSWITCH1+1)
            || (picnum==LOCKSWITCH1+1)
            || (picnum==POWERSWITCH2+1)
            || (picnum==LIGHTSWITCH+1)
       )
    {
        switchpicnum--;
    }
    if ((picnum > MULTISWITCH)&&(picnum <= MULTISWITCH+3))
    {
        switchpicnum = MULTISWITCH;
    }

    switch (tileGetMapping(switchpicnum))
    {
    default:
        if (CheckDoorTile(picnum) == 0) break;
        fallthrough__;
    case DIPSWITCH__:
        //case DIPSWITCH+1:
    case TECHSWITCH__:
        //case TECHSWITCH+1:
    case ALIENSWITCH__:
        //case ALIENSWITCH+1:
        if (picnum == DIPSWITCH  || picnum == DIPSWITCH+1 ||
                picnum == ALIENSWITCH || picnum == ALIENSWITCH+1 ||
                picnum == TECHSWITCH || picnum == TECHSWITCH+1)
        {
            if (picnum == ALIENSWITCH || picnum == ALIENSWITCH+1)
            {
                if (switchtype == 1)
                    S_PlaySoundXYZ(ALIEN_SWITCH1,w,sx,sy,g_player[snum].ps->pos.z);
                else S_PlaySoundXYZ(ALIEN_SWITCH1,g_player[snum].ps->i,sx,sy,g_player[snum].ps->pos.z);
            }
            else
            {
                if (switchtype == 1)
                    S_PlaySoundXYZ(SWITCH_ON,w,sx,sy,g_player[snum].ps->pos.z);
                else S_PlaySoundXYZ(SWITCH_ON,g_player[snum].ps->i,sx,sy,g_player[snum].ps->pos.z);
            }
            if (numdips != correctdips) break;
            S_PlaySoundXYZ(END_OF_LEVEL_WARN,g_player[snum].ps->i,sx,sy,g_player[snum].ps->pos.z);
        }
        fallthrough__;
    case DIPSWITCH2__:
        //case DIPSWITCH2+1:
    case DIPSWITCH3__:
        //case DIPSWITCH3+1:
    case MULTISWITCH__:
        //case MULTISWITCH+1:
        //case MULTISWITCH+2:
        //case MULTISWITCH+3:
    case ACCESSSWITCH__:
    case ACCESSSWITCH2__:
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
    case HANDSWITCH__:
        //case HANDSWITCH+1:
    case PULLSWITCH__:
        //case PULLSWITCH+1:

        if (picnum == MULTISWITCH || picnum == (MULTISWITCH+1) ||
                picnum == (MULTISWITCH+2) || picnum == (MULTISWITCH+3))
            lotag += picnum-MULTISWITCH;

        x = headspritestat[STAT_EFFECTOR];
        while (x >= 0)
        {
            if (((sprite[x].hitag) == lotag))
            {
                switch (sprite[x].lotag)
                {
                case 12:
                    sector[sprite[x].sectnum].floorpal = 0;
                    actor[x].t_data[0]++;
                    if (actor[x].t_data[0] == 2)
                        actor[x].t_data[0]++;

                    break;
                case 24:
                case 34:
                case 25:
                    actor[x].t_data[4] = !actor[x].t_data[4];
                    if (actor[x].t_data[4])
                        P_DoQuote(QUOTE_DEACTIVATED,g_player[snum].ps);
                    else P_DoQuote(QUOTE_ACTIVATED,g_player[snum].ps);
                    break;
                case 21:
                    P_DoQuote(QUOTE_ACTIVATED,g_player[screenpeek].ps);
                    break;
                }
            }
            x = nextspritestat[x];
        }

        G_OperateActivators(lotag,snum);
        G_OperateForceFields(g_player[snum].ps->i,lotag);
        G_OperateMasterSwitches(lotag);

        if (picnum == DIPSWITCH || picnum == DIPSWITCH+1 ||
                picnum == ALIENSWITCH || picnum == ALIENSWITCH+1 ||
                picnum == TECHSWITCH || picnum == TECHSWITCH+1) return 1;

        if (!hitag && CheckDoorTile(picnum) == 0)
        {
            if (switchtype == 1)
                S_PlaySoundXYZ(SWITCH_ON,w,sx,sy,g_player[snum].ps->pos.z);
            else S_PlaySoundXYZ(SWITCH_ON,g_player[snum].ps->i,sx,sy,g_player[snum].ps->pos.z);
        }
        else if (hitag && S_SoundIsValid(hitag))
        {
            if (switchtype == 1 && (g_sounds[hitag]->flags & SF_TALK) == 0)
                S_PlaySoundXYZ(hitag,w,sx,sy,g_player[snum].ps->pos.z);
            else
                A_PlaySound(hitag,g_player[snum].ps->i);
        }

        return 1;
    }
    return 0;

}

void activatebysector(int sect,int j)
{
    UNPREDICTABLE_FUNCTION;

    int i = headspritesect[sect];
    int didit = 0;

    while (i >= 0)
    {
        if (PN(i) == ACTIVATOR)
        {
            G_OperateActivators(SLT(i),-1);
            didit = 1;
            //            return;
        }
        i = nextspritesect[i];
    }

    if (didit == 0)
        G_OperateSectors(sect,j);
}

// Returns W_FORCEFIELD if wall has a forcefield overpicnum, its overpicnum else.
static FORCE_INLINE int G_GetForcefieldPicnum(int const wallNum)
{
    int const tileNum = wall[wallNum].overpicnum;
    return tileNum == W_FORCEFIELD + 1 ? W_FORCEFIELD : tileNum;
}


static void G_BreakWall(int tileNum, int spriteNum, int wallNum)
{
    wall[wallNum].picnum = tileNum;
    A_PlaySound(VENT_BUST, spriteNum);
    A_PlaySound(GLASS_HEAVYBREAK, spriteNum);
    A_SpawnWallGlass(spriteNum, wallNum, 10);
}

void A_DamageWall_Internal(int spriteNum, int wallNum, const vec3_t& vPos, int weaponNum)
{
    int16_t sectNum = -1;
    walltype* pWall = &wall[wallNum];

    if (((unsigned)pWall->overpicnum < MAXTILES && (g_tile[pWall->overpicnum].flags & SFLAG_DAMAGEEVENT))
        || ((unsigned)pWall->picnum < MAXTILES && (g_tile[pWall->picnum].flags & SFLAG_DAMAGEEVENT)))
    {
        if (X_OnEventWithReturn(EVENT_DAMAGEWALL, spriteNum, -1, wallNum) < 0)
            return;
    }

    if (pWall->overpicnum == MIRROR && pWall->pal != 4 &&
        A_CheckSpriteFlags(spriteNum, SFLAG_PROJECTILE) &&
        (actor[spriteNum].projectile.workslike & PROJECTILE_RPG))
    {
        if (pWall->nextwall == -1 || wall[pWall->nextwall].pal != 4)
        {
#ifndef EDUKE32_STANDALONE
            A_SpawnWallGlass(spriteNum, wallNum, 70);
            A_PlaySound(GLASS_HEAVYBREAK, spriteNum);
#endif
            pWall->cstat &= ~16;
            pWall->overpicnum = MIRRORBROKE;
            return;
        }
    }

    if (pWall->overpicnum == MIRROR && pWall->pal != 4)
    {
        switch (tileGetMapping(weaponNum))
        {
        case RADIUSEXPLOSION__:
        case SEENINE__:
#ifndef EDUKE32_STANDALONE
        case HEAVYHBOMB__:
        case RPG__:
        case HYDRENT__:
        case OOZFILTER__:
        case EXPLODINGBARREL__:
#endif
            if (pWall->nextwall == -1 || wall[pWall->nextwall].pal != 4)
            {
#ifndef EDUKE32_STANDALONE
                A_SpawnWallGlass(spriteNum, wallNum, 70);
                A_PlaySound(GLASS_HEAVYBREAK, spriteNum);
#endif
                pWall->cstat &= ~16;
                pWall->overpicnum = MIRRORBROKE;
                return;
            }
        }
    }

    if ((((pWall->cstat & 16) || pWall->overpicnum == BIGFORCE) && pWall->nextsector >= 0) &&
        (sector[pWall->nextsector].floorz > vPos.z) &&
        (sector[pWall->nextsector].floorz != sector[pWall->nextsector].ceilingz))
    {
        int const switchPic = G_GetForcefieldPicnum(wallNum);

        switch (tileGetMapping(switchPic))
        {
        case FANSPRITE__:
            pWall->overpicnum = FANSPRITEBROKE;
            pWall->cstat &= 65535 - 65;
            if (pWall->nextwall >= 0)
            {
                wall[pWall->nextwall].overpicnum = FANSPRITEBROKE;
                wall[pWall->nextwall].cstat &= 65535 - 65;
            }
            A_PlaySound(VENT_BUST, spriteNum);
            A_PlaySound(GLASS_BREAKING, spriteNum);
            return;

#ifndef EDUKE32_STANDALONE
        case W_FORCEFIELD__:
            pWall->extra = 1;  // tell the forces to animate
            fallthrough__;
        case BIGFORCE__:
        {
            updatesector(vPos.x, vPos.y, &sectNum);
            if (sectNum < 0)
                return;

            int xRepeat = 32;
            int yRepeat = 32;

            if (weaponNum == -1)
                xRepeat = yRepeat = 8;
            else if (weaponNum == CHAINGUN)
            {
                xRepeat = 16 + sprite[spriteNum].xrepeat;
                yRepeat = 16 + sprite[spriteNum].yrepeat;
            }

            int const i = A_InsertSprite(sectNum, vPos.x, vPos.y, vPos.z, FORCERIPPLE, -127, xRepeat, yRepeat, 0,
                0, 0, spriteNum, 5);

            CS(i) |= 18 + 128;
            SA(i) = getangle(pWall->x - wall[pWall->point2].x, pWall->y - wall[pWall->point2].y) - 512;

            A_PlaySound(SOMETHINGHITFORCE, i);
        }
        return;

        case GLASS__:
            updatesector(vPos.x, vPos.y, &sectNum);
            if (sectNum < 0)
                return;
            pWall->overpicnum = GLASS2;
            A_SpawnWallGlass(spriteNum, wallNum, 10);
            pWall->cstat = 0;

            if (pWall->nextwall >= 0)
                wall[pWall->nextwall].cstat = 0;

            {
                int const i = A_InsertSprite(sectNum, vPos.x, vPos.y, vPos.z, SECTOREFFECTOR, 0, 0, 0,
                    fix16_to_int(g_player[0].ps->q16ang), 0, 0, spriteNum, 3);
                SLT(i) = 128;
                T2(i) = 5;
                T3(i) = wallNum;
                A_PlaySound(GLASS_BREAKING, i);
            }
            return;

        case STAINGLASS1__:
            updatesector(vPos.x, vPos.y, &sectNum);
            if (sectNum < 0)
                return;
            A_SpawnRandomGlass(spriteNum, wallNum, 80);
            pWall->cstat = 0;
            if (pWall->nextwall >= 0)
                wall[pWall->nextwall].cstat = 0;
            A_PlaySound(VENT_BUST, spriteNum);
            A_PlaySound(GLASS_BREAKING, spriteNum);
            return;
#endif
        }
    }

    switch (tileGetMapping(pWall->picnum))
    {
    case COLAMACHINE__:
    case VENDMACHINE__:
        G_BreakWall(pWall->picnum + 2, spriteNum, wallNum);
        A_PlaySound(VENT_BUST, spriteNum);
        return;

    case OJ__:
    case FEMPIC2__:
    case FEMPIC3__:

    case SCREENBREAK6__:
    case SCREENBREAK7__:
    case SCREENBREAK8__:

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
    case BORNTOBEWILDSCREEN__:
#ifndef EDUKE32_STANDALONE
        A_SpawnWallGlass(spriteNum, wallNum, 30);
        A_PlaySound(GLASS_HEAVYBREAK, spriteNum);
#endif
        pWall->picnum = W_SCREENBREAK + (krand() % 3);
        return;

    case W_TECHWALL5__:
    case W_TECHWALL6__:
    case W_TECHWALL7__:
    case W_TECHWALL8__:
    case W_TECHWALL9__:
        G_BreakWall(pWall->picnum + 1, spriteNum, wallNum);
        return;

    case W_MILKSHELF__:
        G_BreakWall(W_MILKSHELFBROKE, spriteNum, wallNum);
        return;

    case W_TECHWALL10__:
        G_BreakWall(W_HITTECHWALL10, spriteNum, wallNum);
        return;

    case W_TECHWALL1__:
    case W_TECHWALL11__:
    case W_TECHWALL12__:
    case W_TECHWALL13__:
    case W_TECHWALL14__:
        G_BreakWall(W_HITTECHWALL1, spriteNum, wallNum);
        return;

    case W_TECHWALL15__:
        G_BreakWall(W_HITTECHWALL15, spriteNum, wallNum);
        return;

    case W_TECHWALL16__:
        G_BreakWall(W_HITTECHWALL16, spriteNum, wallNum);
        return;

    case W_TECHWALL2__:
        G_BreakWall(W_HITTECHWALL2, spriteNum, wallNum);
        return;

    case W_TECHWALL3__:
        G_BreakWall(W_HITTECHWALL3, spriteNum, wallNum);
        return;

    case W_TECHWALL4__:
        G_BreakWall(W_HITTECHWALL4, spriteNum, wallNum);
        return;

    case ATM__:
        pWall->picnum = ATMBROKE;
        A_SpawnMultiple(spriteNum, MONEY, 1 + (krand() & 7));
        A_PlaySound(GLASS_HEAVYBREAK, spriteNum);
        break;

    case WALLLIGHT1__:
    case WALLLIGHT2__:
    case WALLLIGHT3__:
    case WALLLIGHT4__:
    case TECHLIGHT2__:
    case TECHLIGHT4__:
    {
#ifndef EDUKE32_STANDALONE
        A_PlaySound(rnd(128) ? GLASS_HEAVYBREAK : GLASS_BREAKING, spriteNum);
        A_SpawnWallGlass(spriteNum, wallNum, 30);
#endif

        if (pWall->picnum == WALLLIGHT1)
            pWall->picnum = WALLLIGHTBUST1;

        if (pWall->picnum == WALLLIGHT2)
            pWall->picnum = WALLLIGHTBUST2;

        if (pWall->picnum == WALLLIGHT3)
            pWall->picnum = WALLLIGHTBUST3;

        if (pWall->picnum == WALLLIGHT4)
            pWall->picnum = WALLLIGHTBUST4;

        if (pWall->picnum == TECHLIGHT2)
            pWall->picnum = TECHLIGHTBUST2;

        if (pWall->picnum == TECHLIGHT4)
            pWall->picnum = TECHLIGHTBUST4;

        if (pWall->lotag == 0)
            return;

        sectNum = pWall->nextsector;

        if (sectNum < 0)
            return;

        int darkestWall = 0;

        pWall = &wall[sector[sectNum].wallptr];

        for (bssize_t i = sector[sectNum].wallnum; i > 0; i--, pWall++)
            if (pWall->shade > darkestWall)
                darkestWall = pWall->shade;

        int const random = krand() & 1;

        for (bssize_t SPRITES_OF(STAT_EFFECTOR, i))
            if (SHT(i) == wall[wallNum].lotag && SLT(i) == SE_3_RANDOM_LIGHTS_AFTER_SHOT_OUT)
            {
                T3(i) = random;
                T4(i) = darkestWall;
                T5(i) = 1;
            }

        break;
    }
    }
}

void A_DamageWall(int spriteNum, int wallNum, const vec3_t& vPos, int weaponNum)
{
    ud.returnvar[0] = -1;
    A_DamageWall_Internal(spriteNum, wallNum, vPos, weaponNum);
}

void Sect_DamageFloor_Internal(int const spriteNum, int const sectNum)
{
    int16_t tileNum = sector[sectNum].floorpicnum;
    if (g_tile[tileNum].flags & SFLAG_DAMAGEEVENT)
    {
        if (X_OnEventWithReturn(EVENT_DAMAGEFLOOR, spriteNum, -1, sectNum) < 0)
            return;
    }
}

void Sect_DamageFloor(int const spriteNum, int const sectNum)
{
    ud.returnvar[0] = -1;
    Sect_DamageFloor_Internal(spriteNum, sectNum);
}

void Sect_DamageCeiling_Internal(int const spriteNum, int const sectNum)
{
    int16_t tileNum = sector[sectNum].ceilingpicnum;
    if (g_tile[tileNum].flags & SFLAG_DAMAGEEVENT)
    {
        if (X_OnEventWithReturn(EVENT_DAMAGECEILING, spriteNum, -1, sectNum) < 0)
            return;
    }

#ifndef EDUKE32_STANDALONE
    int16_t* const pPicnum = &sector[sectNum].ceilingpicnum;
    switch (tileGetMapping(*pPicnum))
    {
        case WALLLIGHT1__: *pPicnum = WALLLIGHTBUST1; goto GLASSBREAK_CODE;
        case WALLLIGHT2__: *pPicnum = WALLLIGHTBUST2; goto GLASSBREAK_CODE;
        case WALLLIGHT3__: *pPicnum = WALLLIGHTBUST3; goto GLASSBREAK_CODE;
        case WALLLIGHT4__: *pPicnum = WALLLIGHTBUST4; goto GLASSBREAK_CODE;
        case TECHLIGHT2__: *pPicnum = TECHLIGHTBUST2; goto GLASSBREAK_CODE;
        case TECHLIGHT4__: *pPicnum = TECHLIGHTBUST4;
        {
            // [JM] Will refactor when I backport EVENT_DAMAGEHPLANE.
        GLASSBREAK_CODE:
            A_SpawnCeilingGlass(g_player[myconnectindex].ps->i, sectNum, 10);
            A_PlaySound(GLASS_BREAKING, spriteNum);

            if (sector[sectNum].hitag == 0)
            {
                for (bssize_t SPRITES_OF_SECT(sectNum, i))
                {
                    if (PN(i) == SECTOREFFECTOR && SLT(i) == SE_12_LIGHT_SWITCH)
                    {
                        for (bssize_t SPRITES_OF(STAT_EFFECTOR, j))
                            if (sprite[j].hitag == SHT(i))
                                actor[j].t_data[3] = 1;
                        break;
                    }
                }
            }

            int j = krand() & 1;

            for (bssize_t SPRITES_OF(STAT_EFFECTOR, i))
            {
                if (SHT(i) == sector[sectNum].hitag && SLT(i) == SE_3_RANDOM_LIGHTS_AFTER_SHOT_OUT)
                {
                    T3(i) = j;
                    T5(i) = 1;
                }
            }
        }
    }
#endif
}

void Sect_DamageCeiling(int const spriteNum, int const sectNum)
{
    ud.returnvar[0] = -1;
    Sect_DamageCeiling_Internal(spriteNum, sectNum);
}

// hard coded props... :(
void A_DamageObject_Internal(int i,int sn)
{
    short j;
    int k, p, radiusDamage=0;
    spritetype *s;

    i &= (MAXSPRITES-1);

    if (A_CheckSpriteFlags(i, SFLAG_DAMAGEEVENT))
    {
        Gv_SetVar(sysVarIDs.RETURN, i, sn, -1);
        X_OnEvent(EVENT_DAMAGESPRITE, sn, -1, -1);

        if (Gv_GetVar(sysVarIDs.RETURN, sn, -1) < 0)
            return;
    }

    if (A_CheckSpriteFlags(sn,SFLAG_PROJECTILE))
        if (actor[sn].projectile.workslike & PROJECTILE_RPG)
            radiusDamage = 1;

    switch (tileGetMapping(PN(i)))
    {
    case OCEANSPRITE1__:
    case OCEANSPRITE2__:
    case OCEANSPRITE3__:
    case OCEANSPRITE4__:
    case OCEANSPRITE5__:
        A_Spawn(i,SMALLSMOKE);
        A_DeleteSprite(i);
        break;
    case QUEBALL__:
    case STRIPEBALL__:
        if (sprite[sn].picnum == QUEBALL || sprite[sn].picnum == STRIPEBALL)
        {
            sprite[sn].xvel = (sprite[i].xvel>>1)+(sprite[i].xvel>>2);
            sprite[sn].ang -= (SA(i)<<1)+1024;
            SA(i) = getangle(SX(i)-sprite[sn].x,SY(i)-sprite[sn].y)-512;
            if (g_sounds[POOLBALLHIT]->playing < 2)
                A_PlaySound(POOLBALLHIT,i);
        }
        else
        {
            if (krand()&3)
            {
                sprite[i].xvel = 164;
                sprite[i].ang = sprite[sn].ang;
            }
            else
            {
                A_SpawnWallGlass(i,-1,3);
                A_DeleteSprite(i);
            }
        }
        break;
    case TREE1__:
    case TREE2__:
    case TIRE__:
    case CONE__:
    case BOX__:
    {
        if (radiusDamage == 1)
            if (T1(i) == 0)
            {
                CS(i) &= ~257;
                T1(i) = 1;
                A_Spawn(i,BURNING);
            }
        switch (tileGetMapping(sprite[sn].picnum))
        {
        case RADIUSEXPLOSION__:
        case RPG__:
        case FIRELASER__:
        case HYDRENT__:
        case HEAVYHBOMB__:
            if (T1(i) == 0)
            {
                CS(i) &= ~257;
                T1(i) = 1;
                A_Spawn(i,BURNING);
            }
            break;
        }
        break;
    }
    case CACTUS__:
    {
        switch (tileGetMapping(sprite[sn].picnum))
        {
            case RADIUSEXPLOSION__:
            case RPG__:
            case FIRELASER__:
            case HYDRENT__:
            case HEAVYHBOMB__:
                radiusDamage = 1;
                break;
        }

        if (radiusDamage == 1)
        {
            for (bssize_t k = 64; k > 0; k--)
            {
                int newSprite =
                    A_InsertSprite(SECT(i), SX(i), SY(i), SZ(i) - (krand() % (48 << 8)), SCRAP3 + (krand() & 3), -8, 48, 48,
                        krand() & 2047, (krand() & 63) + 64, -(krand() & 4095) - (sprite[i].zvel >> 2), i, 5);
                sprite[newSprite].pal = 8;
            }
            //        case CACTUSBROKE:
            if (PN(i) == CACTUS)
                PN(i) = CACTUSBROKE;
            CS(i) &= ~257;
        }
        break;
    }
    case HANGLIGHT__:
    case GENERICPOLE2__:
        for (k=6;k>0;k--)
            A_InsertSprite(SECT(i),SX(i),SY(i),SZ(i)-(8<<8),SCRAP1+(krand()&15),-8,48,48,krand()&2047,(krand()&63)+64,-(krand()&4095)-(sprite[i].zvel>>2),i,5);
        A_PlaySound(GLASS_HEAVYBREAK,i);
        A_DeleteSprite(i);
        break;


    case FANSPRITE__:
        PN(i) = FANSPRITEBROKE;
        CS(i) &= (65535-257);
        if (sector[SECT(i)].floorpicnum == FANSHADOW)
            sector[SECT(i)].floorpicnum = FANSHADOWBROKE;

        A_PlaySound(GLASS_HEAVYBREAK,i);
        s = &sprite[i];
        for (j=16;j>0;j--) RANDOMSCRAP(s, i);

        break;
    case WATERFOUNTAIN__:
        //    case WATERFOUNTAIN+1:
        //    case WATERFOUNTAIN+2:
        PN(i) = WATERFOUNTAINBROKE;
        A_Spawn(i,TOILETWATER);
        break;
    case SATELITE__:
    case FUELPOD__:
    case SOLARPANNEL__:
    case ANTENNA__:
        if (sprite[sn].extra != G_DefaultActorHealthForTile(SHOTSPARK1))
        {
            for (j=15;j>0;j--)
                A_InsertSprite(SECT(i),SX(i),SY(i),sector[SECT(i)].floorz-(12<<8)-(j<<9),SCRAP1+(krand()&15),-8,64,64,
                               krand()&2047,(krand()&127)+64,-(krand()&511)-256,i,5);
            A_Spawn(i,EXPLOSION2);
            A_DeleteSprite(i);
        }
        break;
    case BOTTLE1__:
    case BOTTLE2__:
    case BOTTLE3__:
    case BOTTLE4__:
    case BOTTLE5__:
    case BOTTLE6__:
    case BOTTLE8__:
    case BOTTLE10__:
    case BOTTLE11__:
    case BOTTLE12__:
    case BOTTLE13__:
    case BOTTLE14__:
    case BOTTLE15__:
    case BOTTLE16__:
    case BOTTLE17__:
    case BOTTLE18__:
    case BOTTLE19__:
    case WATERFOUNTAINBROKE__:
    case DOMELITE__:
    case SUSHIPLATE1__:
    case SUSHIPLATE2__:
    case SUSHIPLATE3__:
    case SUSHIPLATE4__:
    case SUSHIPLATE5__:
    case WAITTOBESEATED__:
    case VASE__:
    case STATUEFLASH__:
    case STATUE__:
        if (PN(i) == BOTTLE10)
            A_SpawnMultiple(i, MONEY, 4+(krand()&3));
        else if (PN(i) == STATUE || PN(i) == STATUEFLASH)
        {
            A_SpawnRandomGlass(i,-1,40);
            A_PlaySound(GLASS_HEAVYBREAK,i);
        }
        else if (PN(i) == VASE)
            A_SpawnWallGlass(i,-1,40);

        A_PlaySound(GLASS_BREAKING,i);
        SA(i) = krand()&2047;
        A_SpawnWallGlass(i,-1,8);
        A_DeleteSprite(i);
        break;
    case FETUS__:
        PN(i) = FETUSBROKE;
        A_PlaySound(GLASS_BREAKING,i);
        A_SpawnWallGlass(i,-1,10);
        break;
    case FETUSBROKE__:
        for (j=48;j>0;j--)
        {
            A_Shoot(i,BLOODSPLAT1);
            SA(i) += 333;
        }
        A_PlaySound(GLASS_HEAVYBREAK,i);
        A_PlaySound(SQUISHED,i);
        fallthrough__;
    case BOTTLE7__:
        A_PlaySound(GLASS_BREAKING,i);
        A_SpawnWallGlass(i,-1,10);
        A_DeleteSprite(i);
        break;
    case HYDROPLANT__:
        PN(i) = BROKEHYDROPLANT;
        A_PlaySound(GLASS_BREAKING,i);
        A_SpawnWallGlass(i,-1,10);
        break;

    case FORCESPHERE__:
        sprite[i].xrepeat = 0;
        actor[OW(i)].t_data[0] = 32;
        actor[OW(i)].t_data[1] = !actor[OW(i)].t_data[1];
        actor[OW(i)].t_data[2] ++;
        A_Spawn(i,EXPLOSION2);
        break;

    case BROKEHYDROPLANT__:
        A_PlaySound(GLASS_BREAKING, i);
        A_SpawnWallGlass(i, -1, 5);
        A_DeleteSprite(i);
        break;

    case TOILET__:
        PN(i) = TOILETBROKE;
        CS(i) |= (krand()&1)<<2;
        CS(i) &= ~257;
        A_Spawn(i,TOILETWATER);
        A_PlaySound(GLASS_BREAKING,i);
        break;

    case STALL__:
        PN(i) = STALLBROKE;
        CS(i) |= (krand()&1)<<2;
        CS(i) &= ~257;
        A_Spawn(i,TOILETWATER);
        A_PlaySound(GLASS_HEAVYBREAK,i);
        break;

    case HYDRENT__:
        PN(i) = BROKEFIREHYDRENT;
        A_Spawn(i,TOILETWATER);

        //            for(k=0;k<5;k++)
        //          {
        //            j = A_InsertSprite(SECT(i),SX(i),SY(i),SZ(i)-(krand()%(48<<8)),SCRAP3+(krand()&3),-8,48,48,krand()&2047,(krand()&63)+64,-(krand()&4095)-(sprite[i].zvel>>2),i,5);
        //          sprite[j].pal = 2;
        //    }
        A_PlaySound(GLASS_HEAVYBREAK,i);
        break;

    case GRATE1__:
        PN(i) = BGRATE1;
        CS(i) &= (65535-256-1);
        A_PlaySound(VENT_BUST,i);
        break;

    case CIRCLEPANNEL__:
        PN(i) = CIRCLEPANNELBROKE;
        CS(i) &= (65535-256-1);
        A_PlaySound(VENT_BUST,i);
        break;
    case PANNEL1__:
    case PANNEL2__:
        PN(i) = BPANNEL1;
        CS(i) &= (65535-256-1);
        A_PlaySound(VENT_BUST,i);
        break;
    case PANNEL3__:
        PN(i) = BPANNEL3;
        CS(i) &= (65535-256-1);
        A_PlaySound(VENT_BUST,i);
        break;
    case PIPE1__:
    case PIPE2__:
    case PIPE3__:
    case PIPE4__:
    case PIPE5__:
    case PIPE6__:
        switch (tileGetMapping(PN(i)))
        {
        case PIPE1__:
            PN(i)=PIPE1B;
            break;
        case PIPE2__:
            PN(i)=PIPE2B;
            break;
        case PIPE3__:
            PN(i)=PIPE3B;
            break;
        case PIPE4__:
            PN(i)=PIPE4B;
            break;
        case PIPE5__:
            PN(i)=PIPE5B;
            break;
        case PIPE6__:
            PN(i)=PIPE6B;
            break;
        }

        j = A_Spawn(i,STEAM);
        sprite[j].z = sector[SECT(i)].floorz-(32<<8);
        break;

    case MONK__:
    case LUKE__:
    case INDY__:
    case JURYGUY__:
        A_PlaySound(SLT(i),i);
        A_Spawn(i,SHT(i));
        fallthrough__;

    case SPACEMARINE__:
        sprite[i].extra -= sprite[sn].extra;
        if (sprite[i].extra > 0) break;
        SA(i) = krand()&2047;
        A_Shoot(i,BLOODSPLAT1);
        SA(i) = krand()&2047;
        A_Shoot(i,BLOODSPLAT2);
        SA(i) = krand()&2047;
        A_Shoot(i,BLOODSPLAT3);
        SA(i) = krand()&2047;
        A_Shoot(i,BLOODSPLAT4);
        SA(i) = krand()&2047;
        A_Shoot(i,BLOODSPLAT1);
        SA(i) = krand()&2047;
        A_Shoot(i,BLOODSPLAT2);
        SA(i) = krand()&2047;
        A_Shoot(i,BLOODSPLAT3);
        SA(i) = krand()&2047;
        A_Shoot(i,BLOODSPLAT4);
        A_DoGuts(i,JIBS1,1);
        A_DoGuts(i,JIBS2,2);
        A_DoGuts(i,JIBS3,3);
        A_DoGuts(i,JIBS4,4);
        A_DoGuts(i,JIBS5,1);
        A_DoGuts(i,JIBS3,6);
        S_PlaySound(SQUISHED);
        A_DeleteSprite(i);
        break;
    case CHAIR1__:
    case CHAIR2__:
        PN(i) = BROKENCHAIR;
        CS(i) = 0;
        break;
    case CHAIR3__:
    case MOVIECAMERA__:
    case SCALE__:
    case VACUUM__:
    case CAMERALIGHT__:
    case IVUNIT__:
    case POT1__:
    case POT2__:
    case POT3__:
    case TRIPODCAMERA__:
        A_PlaySound(GLASS_HEAVYBREAK,i);
        s = &sprite[i];
        for (j=16;j>0;j--) RANDOMSCRAP(s, i);
        A_DeleteSprite(i);
        break;
    case PLAYERONWATER__:
        i = OW(i);
        fallthrough__;
    default:
        if ((sprite[i].cstat&16) && SHT(i) == 0 && SLT(i) == 0 && sprite[i].statnum == 0)
            break;

        if ((sprite[sn].picnum == FREEZEBLAST || sprite[sn].owner != i) && sprite[i].statnum != STAT_PROJECTILE)
        {
            if (A_CheckEnemySprite(&sprite[i]) == 1)
            {
                if (sprite[sn].picnum == RPG) // This is what doubles RPG damage on direct hit.
                    sprite[sn].extra <<= 1;

                // Spawns blood from the projectile doing the damage.
                if ((PN(i) != DRONE) && (PN(i) != ROTATEGUN) && (PN(i) != COMMANDER) && (PN(i) < GREENSLIME || PN(i) > GREENSLIME+7))
                    if (sprite[sn].picnum != FREEZEBLAST)
                        if (!A_CheckSpriteFlags(i, SFLAG_BADGUY) || A_CheckSpriteFlags(i, SFLAG_HURTSPAWNBLOOD))
                        {
                            j = A_Spawn(sn,JIBS6);
                            if (sprite[sn].pal == 6)
                                sprite[j].pal = 6;
                            sprite[j].z += (4<<8);
                            sprite[j].xvel = 16;
                            sprite[j].xrepeat = sprite[j].yrepeat = 24;
                            sprite[j].ang += 32-(krand()&63);
                        }

                j = sprite[sn].owner;

                if (j >= 0 && sprite[j].picnum == APLAYER && PN(i) != ROTATEGUN && PN(i) != DRONE)
                    if (g_player[sprite[j].yvel].ps->curr_weapon == SHOTGUN_WEAPON)
                    {
                        A_Shoot(i,BLOODSPLAT3);
                        A_Shoot(i,BLOODSPLAT1);
                        A_Shoot(i,BLOODSPLAT2);
                        A_Shoot(i,BLOODSPLAT4);
                    }

                if (!A_CheckSpriteFlags(i, SFLAG_NODAMAGEPUSH))
                {
                    if (sprite[i].extra > 0)
                    {
                        if ((sprite[i].cstat&48) == 0)
                            SA(i) = (sprite[sn].ang+1024)&2047;
                        sprite[i].xvel = -(sprite[sn].extra<<2);
                        j = SECT(i);
                        pushmove_old(&SX(i),&SY(i),&SZ(i),&j,128L,(4L<<8),(4L<<8),CLIPMASK0);
                        if (j != SECT(i) && j >= 0 && j < MAXSECTORS)
                            changespritesect(i,j);
                    }
                }

                if (sprite[i].statnum == STAT_ZOMBIEACTOR)
                {
                    changespritestat(i, STAT_ACTOR);
                    actor[i].timetosleep = SLEEPTIME;
                }
                if ((sprite[i].xrepeat < 24 || PN(i) == SHARK) && sprite[sn].picnum == SHRINKSPARK) return;
            }

            if (sprite[i].statnum != STAT_ZOMBIEACTOR)
            {
                if (sprite[sn].picnum == FREEZEBLAST && ((PN(i) == APLAYER && sprite[i].pal == 1) || (g_freezerSelfDamage == 0 && sprite[sn].owner == i)))
                    return;

                actor[i].htpicnum = sprite[sn].picnum;
                actor[i].htextra += sprite[sn].extra;
                actor[i].htang = sprite[sn].ang;
                actor[i].htowner = sprite[sn].owner;

                if (A_CheckSpriteFlags(i, SFLAG_DAMAGEEVENT))
                {
                    Gv_SetVar(sysVarIDs.RETURN, i, sn, -1);
                    X_OnEvent(EVENT_POSTDAMAGESPRITE, sn, -1, -1);
                }
            }

            if (sprite[i].statnum == STAT_PLAYER)
            {
                p = sprite[i].yvel;
                if (g_player[p].ps->newowner >= 0)
                {
                    G_ClearCameraView(g_player[p].ps);
                }

                if (sprite[i].xrepeat < 24 && sprite[sn].picnum == SHRINKSPARK)
                    return;

                if (sprite[actor[i].htowner].picnum != APLAYER)
                    if (ud.player_skill >= 3)
                        sprite[sn].extra += (sprite[sn].extra>>1);
            }

        }
        break;
    }
}

void A_DamageObject(int i, int sn)
{
    ud.returnvar[0] = -1;
    A_DamageObject_Internal(i, sn);
}

void allignwarpelevators(void)
{
    int j, i = headspritestat[STAT_EFFECTOR];

    while (i >= 0)
    {
        if (SLT(i) == 17 && SS(i) > 16)
        {
            j = headspritestat[STAT_EFFECTOR];
            while (j >= 0)
            {
                if ((sprite[j].lotag) == 17 && i != j && (SHT(i)) == (sprite[j].hitag))
                {
                    sector[sprite[j].sectnum].floorz = sector[SECT(i)].floorz;
                    sector[sprite[j].sectnum].ceilingz = sector[SECT(i)].ceilingz;
                }

                j = nextspritestat[j];
            }
        }
        i = nextspritestat[i];
    }
}

static int P_CheckDetonatorSpecialCase(DukePlayer_t* const pPlayer, int weaponNum)
{
    if (weaponNum == HANDBOMB_WEAPON && pPlayer->ammo_amount[HANDBOMB_WEAPON] == 0)
    {
        int spriteNum = headspritestat[STAT_ACTOR];

        while (spriteNum >= 0)
        {
            if (sprite[spriteNum].picnum == HEAVYHBOMB && sprite[spriteNum].owner == pPlayer->i)
                return 1;

            spriteNum = nextspritestat[spriteNum];
        }
    }

    return 0;
}

void P_HandleSharedKeys(int snum)
{
    SET_PREDICTION_CONTEXT(snum);

    int k;
    unsigned int sb_snum = g_player[snum].inputBits->bits;// , j;
    DukePlayer_t *p = g_player[snum].ps;

    if (p->cheat_phase == 1) return;

    // 1<<0  =  jump
    // 1<<1  =  crouch
    // 1<<2  =  fire
    // 1<<3  =  aim up
    // 1<<4  =  aim down
    // 1<<5  =  run
    // 1<<6  =  look left
    // 1<<7  =  look right
    // 15<<8 = !weapon selection (bits 8-11)
    // 1<<12 = !steroids
    // 1<<13 =  look up
    // 1<<14 =  look down
    // 1<<15 = !nightvis
    // 1<<16 = !medkit
    // 1<<17 =  (multiflag==1) ? changes meaning of bits 18 and 19
    // 1<<18 =  centre view
    // 1<<19 = !holster weapon
    // 1<<20 = !inventory left
    // 1<<21 = !pause
    // 1<<22 = !quick kick
    // 1<<23 =  aim mode
    // 1<<24 = !holoduke
    // 1<<25 = !jetpack
    // 1<<26 =  g_gameQuit
    // 1<<27 = !inventory right
    // 1<<28 = !turn around
    // 1<<29 = !open
    // 1<<30 = !inventory
    // 1<<31 = !escape

    int const aimMode = p->aim_mode;

    p->aim_mode = (sb_snum >> SK_AIMMODE) & 1;
    if (p->aim_mode < aimMode)
        p->return_to_center = 9;

    if (TEST_SYNC_KEY(sb_snum, SK_QUICK_KICK) && p->quick_kick == 0)
    {
        if (p->curr_weapon != KNEE_WEAPON || p->kickback_pic == 0 || DMFLAGS_TEST(DMFLAG_ALLOWDOUBLEKICK))
        {
            Gv_SetVar(sysVarIDs.RETURN, 0, g_player[snum].ps->i, snum);
            X_OnEvent(EVENT_QUICKKICK, g_player[snum].ps->i, snum, -1);
            if (Gv_GetVar(sysVarIDs.RETURN, g_player[snum].ps->i, snum) == 0)
            {
                p->quick_kick = 14;
                if (p->fta == 0 || p->ftq == QUOTE_MIGHTY_FOOT)
                    P_DoQuote(QUOTE_MIGHTY_FOOT, p);
            }
        }
    }

    uint32_t weaponNum = sb_snum & ((15u << SK_WEAPON_BITS) | BIT(SK_STEROIDS) | BIT(SK_NIGHTVISION) | BIT(SK_MEDKIT) | BIT(SK_QUICK_KICK) | \
        BIT(SK_HOLSTER) | BIT(SK_INV_LEFT) | BIT(SK_PAUSE) | BIT(SK_HOLODUKE) | BIT(SK_JETPACK) | BIT(SK_INV_RIGHT) | \
        BIT(SK_TURNAROUND) | BIT(SK_OPEN) | BIT(SK_INVENTORY) | BIT(SK_ESCAPE));
    sb_snum = weaponNum & ~p->interface_toggle_flag;
    p->interface_toggle_flag |= sb_snum | ((sb_snum & 0xf00) ? 0xf00 : 0);
    p->interface_toggle_flag &= weaponNum | ((weaponNum & 0xf00) ? 0xf00 : 0);

    if (sb_snum && TEST_SYNC_KEY(sb_snum, SK_MULTIFLAG) == 0)
    {
        if (TEST_SYNC_KEY(sb_snum, SK_PAUSE))
        {
            KB_ClearKeyDown(sc_Pause);
            if (ud.pause_on)
                ud.pause_on = 0;
            else ud.pause_on = 1+SHIFTS_IS_PRESSED;
            if (ud.pause_on)
            {
                S_PauseMusic(1);
                S_PauseSounds(true);
            }
            else
            {
                if (ud.config.MusicToggle) S_PauseMusic(0);
                S_PauseSounds(false);
                pub = NUMPAGES;
                pus = NUMPAGES;
            }
        }

        if (ud.pause_on)
            return;

        if (sprite[p->i].extra <= 0)
            return; // if dead...

        if (TEST_SYNC_KEY(sb_snum, SK_INVENTORY) && p->newowner == -1)	// inventory button generates event for selected item
        {
            Gv_SetVar(sysVarIDs.RETURN,0,g_player[snum].ps->i,snum);
            X_OnEvent(EVENT_INVENTORY,g_player[snum].ps->i,snum, -1);
            if (Gv_GetVar(sysVarIDs.RETURN,g_player[snum].ps->i,snum) == 0)
            {
                switch (p->inven_icon)
                {
                    case ICON_JETPACK:  sb_snum |= BIT(SK_JETPACK);     break;
                    case ICON_HOLODUKE: sb_snum |= BIT(SK_HOLODUKE);    break;
                    case ICON_HEATS:    sb_snum |= BIT(SK_NIGHTVISION); break;
                    case ICON_FIRSTAID: sb_snum |= BIT(SK_MEDKIT);      break;
                    case ICON_STEROIDS: sb_snum |= BIT(SK_STEROIDS);    break;
                }
            }
        }

        if (TEST_SYNC_KEY(sb_snum, SK_NIGHTVISION))
        {
            Gv_SetVar(sysVarIDs.RETURN,0,g_player[snum].ps->i,snum);
            X_OnEvent(EVENT_USENIGHTVISION,g_player[snum].ps->i,snum, -1);
            if (Gv_GetVar(sysVarIDs.RETURN,g_player[snum].ps->i,snum) == 0
                    &&  p->inv_amount[GET_HEATS] > 0)
            {
                p->heat_on = !p->heat_on;
                P_UpdateScreenPal(p);
                p->inven_icon = 5;
                A_PlaySound(NITEVISION_ONOFF,p->i);
                P_DoQuote(QUOTE_NVG_OFF-(p->heat_on),p);
            }
        }

        if (TEST_SYNC_KEY(sb_snum, SK_STEROIDS))
        {
            Gv_SetVar(sysVarIDs.RETURN,0,g_player[snum].ps->i,snum);
            X_OnEvent(EVENT_USESTEROIDS,g_player[snum].ps->i,snum, -1);
            if (Gv_GetVar(sysVarIDs.RETURN,g_player[snum].ps->i,snum) == 0)
            {
                if (p->inv_amount[GET_STEROIDS] == 400)
                {
                    p->inv_amount[GET_STEROIDS]--;
                    A_PlaySound(DUKE_TAKEPILLS,p->i);
                    P_DoQuote(QUOTE_USED_STEROIDS,p);
                }
                if (p->inv_amount[GET_STEROIDS] > 0)
                    p->inven_icon = 2;
            }
            return;		// is there significance to returning?
        }

        if (p->newowner == -1 && (TEST_SYNC_KEY(sb_snum, SK_INV_LEFT) || TEST_SYNC_KEY(sb_snum, SK_INV_RIGHT)))
        {
            p->invdisptime = GAMETICSPERSEC * 2;

            int const inventoryRight = !!(TEST_SYNC_KEY(sb_snum, SK_INV_RIGHT));

            int32_t inventoryIcon = p->inven_icon;

            int i = 0;

CHECKINV1:
            if (i < 9)
            {
                i++;

                switch (inventoryIcon)
                {
                    case ICON_JETPACK:
                    case ICON_SCUBA:
                    case ICON_STEROIDS:
                    case ICON_HOLODUKE:
                    case ICON_HEATS:
                        if (p->inv_amount[icon_to_inv[inventoryIcon]] > 0 && i > 1)
                            break;
                        if (inventoryRight)
                            inventoryIcon++;
                        else
                            inventoryIcon--;
                        goto CHECKINV1;
                    case ICON_NONE:
                    case ICON_FIRSTAID:
                        if (p->inv_amount[GET_FIRSTAID] > 0 && i > 1)
                            break;
                        inventoryIcon = inventoryRight ? 2 : 7;
                        goto CHECKINV1;
                    case ICON_BOOTS:
                        if (p->inv_amount[GET_BOOTS] > 0 && i > 1)
                            break;
                        inventoryIcon = inventoryRight ? 1 : 6;
                        goto CHECKINV1;
                }
            }
            else inventoryIcon = 0;

            if (TEST_SYNC_KEY(sb_snum, SK_INV_LEFT))   // Inventory_Left
            {
                Gv_SetVar(sysVarIDs.RETURN, inventoryIcon, g_player[snum].ps->i, snum);
                X_OnEvent(EVENT_INVENTORYLEFT, g_player[snum].ps->i, snum, -1);
                inventoryIcon = Gv_GetVar(sysVarIDs.RETURN, g_player[snum].ps->i, snum);
            }
            if (TEST_SYNC_KEY(sb_snum, SK_INV_RIGHT))   // Inventory_Right
            {
                Gv_SetVar(sysVarIDs.RETURN, inventoryIcon, g_player[snum].ps->i, snum);
                X_OnEvent(EVENT_INVENTORYRIGHT, g_player[snum].ps->i, snum, -1);
                inventoryIcon = Gv_GetVar(sysVarIDs.RETURN, g_player[snum].ps->i, snum);
            }

            if (inventoryIcon >= 1)
            {
                p->inven_icon = inventoryIcon;

                static const int32_t invQuotes[8] = { QUOTE_MEDKIT, QUOTE_STEROIDS, QUOTE_HOLODUKE,
                    QUOTE_JETPACK, QUOTE_NVG, QUOTE_SCUBA, QUOTE_BOOTS, 0 };

                if (inventoryIcon - 1 < ARRAY_SSIZE(invQuotes))
                    P_DoQuote(invQuotes[inventoryIcon - 1], p);
            }
        }

        weaponNum = ((sb_snum & SK_WEAPON_MASK) >> SK_WEAPON_BITS) - 1;

        Gv_SetVar(sysVarIDs.RETURN, weaponNum,p->i,snum);

        switch ((int32_t)weaponNum)
        {
        case -1:
            break;
        default:
            X_OnEvent(EVENT_WEAPKEY1+weaponNum,p->i,snum, -1);
            break;
        case 10:
            X_OnEvent(EVENT_PREVIOUSWEAPON,p->i,snum, -1);
            break;
        case 11:
            X_OnEvent(EVENT_NEXTWEAPON,p->i,snum, -1);
            break;
        case 12:
            X_OnEvent(EVENT_ALTWEAPON, p->i, snum, -1);
            break;
        case 13:
            X_OnEvent(EVENT_LASTWEAPON, p->i, snum, -1);
            break;
        }

        if ((unsigned int) Gv_GetVar(sysVarIDs.RETURN,p->i,snum) != weaponNum)
            weaponNum = (unsigned int) Gv_GetVar(sysVarIDs.RETURN,p->i,snum);

        // NOTE: it is assumed that the above events return either -1 or a
        // valid weapon index. Presumably, neither other negative numbers nor
        // positive ones >= MAX_WEAPONS are allowed. However, the code below is
        // a bit inconsistent in checking "j".

        if (p->reloading == 1)
            weaponNum = -1;
        else if ((uint32_t)weaponNum < MAX_WEAPONS && p->kickback_pic == 1 && p->weapon_pos == 1)
        {
            p->wantweaponfire = weaponNum;
            p->kickback_pic = 0;
        }
        if ((int32_t)weaponNum != -1 && p->last_pissed_time <= (GAMETICSPERSEC * 218) && p->show_empty_weapon == 0 &&
            p->kickback_pic == 0 && p->quick_kick == 0 && sprite[p->i].xrepeat > 32 && p->access_incs == 0 &&
            p->knee_incs == 0)
        {
            //            if(  ( p->weapon_pos == 0 || ( p->holster_weapon && p->weapon_pos == -9 ) ))
            {
                if (weaponNum >= 12) // hack
                    weaponNum++;
                if (weaponNum == 10 || weaponNum == 11)
                {
                    int currentWeapon = p->curr_weapon;

                    weaponNum = (weaponNum == 10 ? -1 : 1);  // JBF: prev (-1) or next (1) weapon choice
                    int i = currentWeapon;

                    while ((currentWeapon >= 0 && currentWeapon < 11) || (PLUTOPAK && currentWeapon == GROW_WEAPON))
                    {
                        // this accounts for the expander when handling next/previous

                        switch (currentWeapon)
                        {
                        case DEVISTATOR_WEAPON:
                            if ((int32_t)weaponNum == -1)
                            {
                                if (PLUTOPAK)
                                    currentWeapon = GROW_WEAPON;
                                else
                                    currentWeapon--;
                            }
                            else
                                currentWeapon++;
                            break;

                        case GROW_WEAPON:
                            currentWeapon = ((int32_t)weaponNum == -1) ? SHRINKER_WEAPON : DEVISTATOR_WEAPON;
                            break;

                        case SHRINKER_WEAPON:
                            if ((int32_t)weaponNum == 1)
                            {
                                if (PLUTOPAK)
                                    currentWeapon = GROW_WEAPON;
                                else
                                    currentWeapon++;
                            }
                            else
                                currentWeapon--;
                            break;

                        case KNEE_WEAPON:
                            if ((int32_t)weaponNum == -1)
                            {
#if 0
                                if (WORLDTOUR)
                                    currentWeapon = FLAMETHROWER_WEAPON;
                                else
#endif
                                    currentWeapon = FREEZE_WEAPON;
                            }
                            else
                                currentWeapon++;
                            break;
#if 0
                        case FLAMETHROWER_WEAPON:
                            currentWeapon = ((int32_t)weaponNum == -1) ? FREEZE_WEAPON : KNEE_WEAPON;
                            break;
#endif

                        case FREEZE_WEAPON:
                            if ((int32_t)weaponNum == 1)
                            {
#if 0
                                if (WORLDTOUR)
                                    currentWeapon = FLAMETHROWER_WEAPON;
                                else
#endif
                                    currentWeapon = KNEE_WEAPON;
                            }
                            else
                                currentWeapon--;
                            break;

                        case HANDREMOTE_WEAPON:
                            i = currentWeapon = HANDBOMB_WEAPON;
                            fallthrough__;
                        default:
                            currentWeapon += weaponNum;
                            break;
                        }

                        if (((p->gotweapon[currentWeapon]) && p->ammo_amount[currentWeapon] > 0) || P_CheckDetonatorSpecialCase(p, currentWeapon))
                        {
                            weaponNum = currentWeapon;
                            break;
                        }

                        if (i == currentWeapon) // absolutely no weapons, so use foot
                        {
                            weaponNum = KNEE_WEAPON;
                            break;
                        }
                    }

                    if (weaponNum == SHRINKER_WEAPON)
                        p->subweapon &= ~(1 << GROW_WEAPON);
                    else if (weaponNum == GROW_WEAPON)
                        p->subweapon |= (1 << GROW_WEAPON);
#if 0
                    else if (weaponNum == FREEZE_WEAPON)
                        pPlayer->subweapon &= ~(1 << FLAMETHROWER_WEAPON);
                    else if (weaponNum == FLAMETHROWER_WEAPON)
                        pPlayer->subweapon |= (1 << FLAMETHROWER_WEAPON);
#endif
                }

                // last used weapon will depend on subweapon
                if (weaponNum >= 13) // alt weapon, last used weapon
                {
                    uint32_t const weaponNumSwitch = (weaponNum == 14) ? p->last_used_weapon : p->curr_weapon;
                    switch (weaponNumSwitch)
                    {
                    case HANDREMOTE_WEAPON:
                        weaponNum = HANDBOMB_WEAPON;
                        break;
                    case GROW_WEAPON:
                        weaponNum = SHRINKER_WEAPON;
                        break;
                    default:
                        weaponNum = weaponNumSwitch;
                        break;
                    }
                }

                k = -1;

                P_SetWeaponGamevars(snum, p);

                Gv_SetVar(sysVarIDs.RETURN, weaponNum,p->i,snum);
                X_OnEvent(EVENT_SELECTWEAPON,p->i,snum, -1);
                weaponNum = Gv_GetVar(sysVarIDs.RETURN, p->i, snum);

                if ((int32_t)weaponNum != -1 && weaponNum <= MAX_WEAPONS)
                {
                    if (P_CheckDetonatorSpecialCase(p, weaponNum))
                    {
                        p->gotweapon[HANDBOMB_WEAPON] = 1;
                        weaponNum = HANDREMOTE_WEAPON;
                    }

                    if (weaponNum == SHRINKER_WEAPON && PLUTOPAK)   // JBF 20040116: so we don't select the grower with v1.3d
                    {
                        if (screenpeek == snum) pus = NUMPAGES;

                        if (p->curr_weapon != GROW_WEAPON && p->curr_weapon != SHRINKER_WEAPON)
                        {
                            if (p->ammo_amount[GROW_WEAPON] > 0)
                            {
                                if ((p->subweapon&(1<<GROW_WEAPON)) == (1<<GROW_WEAPON))
                                    weaponNum = GROW_WEAPON;
                                else if (p->ammo_amount[SHRINKER_WEAPON] == 0)
                                {
                                    weaponNum = GROW_WEAPON;
                                    p->subweapon |= (1<<GROW_WEAPON);
                                }
                            }
                            else if (p->ammo_amount[SHRINKER_WEAPON] > 0)
                                p->subweapon &= ~(1<<GROW_WEAPON);
                        }
                        else if (p->curr_weapon == SHRINKER_WEAPON)
                        {
                            p->subweapon |= (1<<GROW_WEAPON);
                            weaponNum = GROW_WEAPON;
                        }
                        else
                            p->subweapon &= ~(1<<GROW_WEAPON);
                    }

                    if (p->holster_weapon)
                    {
                        sb_snum |= BIT(SK_HOLSTER);
                        p->weapon_pos = -9;
                    }
                    else if ((uint32_t)weaponNum < MAX_WEAPONS && p->gotweapon[weaponNum] && (uint32_t)p->curr_weapon != weaponNum)
                        switch (weaponNum)
                        {
                        case PISTOL_WEAPON:
                        case SHOTGUN_WEAPON:
                        case CHAINGUN_WEAPON:
                        case RPG_WEAPON:
                        case DEVISTATOR_WEAPON:
                        case FREEZE_WEAPON:
                        case GROW_WEAPON:
                        case SHRINKER_WEAPON:
                            if (p->ammo_amount[weaponNum] == 0 && p->show_empty_weapon == 0)
                            {
                                p->show_empty_weapon = 32;
                                p->last_full_weapon = p->curr_weapon;
                            }
                            fallthrough__;
                        case KNEE_WEAPON:
                            P_AddWeapon(p, weaponNum, 1);
                            break;

                        case HANDREMOTE_WEAPON:
                            if (k >= 0) // Found in list of [1]'s
                            {
                                p->curr_weapon = HANDREMOTE_WEAPON;
                                p->last_weapon = -1;
                                p->weapon_pos = 10;
                                Gv_SetVar(sysVarIDs.WEAPON, p->curr_weapon, p->i, snum);
                            }
                            break;

                        case HANDBOMB_WEAPON:
                        case TRIPBOMB_WEAPON:
                            if (p->ammo_amount[weaponNum] > 0 && p->gotweapon[weaponNum])
                                P_AddWeapon(p, weaponNum, 1);
                            break;
                        }
                }

            }
        }

        if (TEST_SYNC_KEY(sb_snum, SK_HOLODUKE) && (p->newowner == -1) && !oldnet_predicting)
        {
            if (p->holoduke_on == -1)
            {
                Gv_SetVar(sysVarIDs.RETURN,0,g_player[snum].ps->i,snum);
                X_OnEvent(EVENT_HOLODUKEON,g_player[snum].ps->i,snum, -1);
                if (Gv_GetVar(sysVarIDs.RETURN,g_player[snum].ps->i,snum) == 0)
                {
                    if (p->inv_amount[GET_HOLODUKE] > 0)
                    {
                        p->inven_icon = 3;

                        if (p->cursectnum > -1)
                        {
                            int i = p->holoduke_on = A_InsertSprite(p->cursectnum,
                                                                p->pos.x,
                                                                p->pos.y,
                                                                p->pos.z+(30<<8),APLAYER,-64,0,0,fix16_to_int(p->q16ang),0,0,-1,10);
                            T4(i) = T5(i) = 0;
                            SP(i) = snum;
                            sprite[i].extra = 0;
                            P_DoQuote(QUOTE_HOLODUKE_ON,p);
                            A_PlaySound(TELEPORTER,p->holoduke_on);
                        }
                    }
                    else P_DoQuote(QUOTE_HOLODUKE_NOT_FOUND,p);
                }
            }
            else
            {
                Gv_SetVar(sysVarIDs.RETURN,0,g_player[snum].ps->i,snum);
                X_OnEvent(EVENT_HOLODUKEOFF,g_player[snum].ps->i,snum, -1);
                if (Gv_GetVar(sysVarIDs.RETURN,g_player[snum].ps->i,snum) == 0)
                {
                    A_PlaySound(TELEPORTER,p->holoduke_on);
                    p->holoduke_on = -1;
                    P_DoQuote(QUOTE_HOLODUKE_OFF,p);
                }
            }
        }

        if (TEST_SYNC_KEY(sb_snum, SK_MEDKIT))
        {
            Gv_SetVar(sysVarIDs.RETURN,0,g_player[snum].ps->i,snum);
            X_OnEvent(EVENT_USEMEDKIT,g_player[snum].ps->i,snum, -1);
            if (Gv_GetVar(sysVarIDs.RETURN,g_player[snum].ps->i,snum) == 0)
            {
                if (p->inv_amount[GET_FIRSTAID] > 0 && sprite[p->i].extra < p->max_player_health)
                {
                    int healthDiff = p->max_player_health-sprite[p->i].extra;

                    if (p->inv_amount[GET_FIRSTAID] > healthDiff)
                    {
                        p->inv_amount[GET_FIRSTAID] -= healthDiff;
                        sprite[p->i].extra = p->max_player_health;
                        p->inven_icon = 1;
                    }
                    else
                    {
                        sprite[p->i].extra += p->inv_amount[GET_FIRSTAID];
                        p->inv_amount[GET_FIRSTAID] = 0;
                        P_SelectNextInvItem(p);
                    }
                    A_PlaySound(DUKE_USEMEDKIT,p->i);
                }
            }
        }

        if (TEST_SYNC_KEY(sb_snum, SK_JETPACK) && p->newowner == -1)
        {
            Gv_SetVar(sysVarIDs.RETURN,0,g_player[snum].ps->i,snum);
            X_OnEvent(EVENT_USEJETPACK,g_player[snum].ps->i,snum, -1);
            if (Gv_GetVar(sysVarIDs.RETURN,g_player[snum].ps->i,snum) == 0)
            {
                if (p->inv_amount[GET_JETPACK] > 0)
                {
                    p->jetpack_on = !p->jetpack_on;
                    if (p->jetpack_on)
                    {
                        p->inven_icon = 4;
                        if (p->scream_voice > FX_Ok)
                        {
                            FX_StopSound(p->scream_voice);
                           // S_TestSoundCallback(DUKE_SCREAM);
                            S_Cleanup();
                            p->scream_voice = FX_Ok;
                        }

                        A_PlaySound(DUKE_JETPACK_ON,p->i);

                        P_DoQuote(QUOTE_JETPACK_ON,p);
                    }
                    else
                    {
                        p->hard_landing = 0;
                        p->vel.z = 0;
                        A_PlaySound(DUKE_JETPACK_OFF,p->i);
                        S_StopEnvSound(DUKE_JETPACK_IDLE,p->i);
                        S_StopEnvSound(DUKE_JETPACK_ON,p->i);
                        P_DoQuote(QUOTE_JETPACK_OFF,p);
                    }
                }
                else P_DoQuote(QUOTE_JETPACK_NOT_FOUND,p);
            }
        }

        if (TEST_SYNC_KEY(sb_snum, SK_TURNAROUND) && p->one_eighty_count == 0)
        {
            Gv_SetVar(sysVarIDs.RETURN,0,p->i,snum);
            X_OnEvent(EVENT_TURNAROUND,p->i,snum, -1);
            if (Gv_GetVar(sysVarIDs.RETURN,p->i,snum) == 0)
            {
                p->one_eighty_count = -1024;
            }
        }
    }
}

int32_t A_CheckHitSprite(int spriteNum, int16_t* hitSprite)
{
    hitdata_t hitData;
    int32_t   zOffset = 0;

    if (A_CheckEnemySprite(&sprite[spriteNum]))
        zOffset = (42 << 8);
    else if (sprite[spriteNum].picnum == APLAYER)
        zOffset = (39 << 8);

    sprite[spriteNum].z -= zOffset;
    hitscan((const vec3_t*)&sprite[spriteNum], sprite[spriteNum].sectnum, sintable[(sprite[spriteNum].ang + 512) & 2047],
        sintable[sprite[spriteNum].ang & 2047], 0, &hitData, CLIPMASK1);
    sprite[spriteNum].z += zOffset;

    if (hitSprite)
        *hitSprite = hitData.sprite;

    if (hitData.wall >= 0 && (wall[hitData.wall].cstat & 16) && A_CheckEnemySprite(&sprite[spriteNum]))
        return 1 << 30;

    return FindDistance2D(hitData.x - sprite[spriteNum].x, hitData.y - sprite[spriteNum].y);
}

static int P_FindWall(DukePlayer_t *p,int16_t *hitw)
{
    hitdata_t hit;
    hitscan(&p->pos,p->cursectnum,
            sintable[(fix16_to_int(p->q16ang)+512)&2047],
            sintable[fix16_to_int(p->q16ang)&2047],
            0,&hit,CLIPMASK0);

    *hitw = hit.wall;

    if (hit.wall < 0)
        return INT32_MAX;

    return (FindDistance2D(hit.x-p->pos.x,hit.y-p->pos.y));
}

static void G_ClearCameras(DukePlayer_t* p)
{
    G_ClearCameraView(p);

    if (KB_KeyPressed(sc_Escape))
        KB_ClearKeyDown(sc_Escape);
}

static void P_DoSectorInteractions(int32_t playerNum)
{
    UNPREDICTABLE_FUNCTION;

    DukePlayer_t* p = g_player[playerNum].ps;

    if (p->cursectnum > -1)
    {
        switch (sector[p->cursectnum].lotag)
        {

        case 32767:
            sector[p->cursectnum].lotag = 0;
            P_DoQuote(QUOTE_FOUND_SECRET, p);
            ud.secret_rooms++;
            if (ud.multimode > 1 && GTFLAGS(GAMETYPE_COOP))
            {
                Bsprintf(tempbuf, "^12%s^12 has found a secret!", &g_player[playerNum].user_name[0]);
                G_AddUserQuote(tempbuf);
            }
            return;
        case -1:
            sector[p->cursectnum].lotag = 0;
            Net_EndOfLevel(false);
            return;
        case -2:
            sector[p->cursectnum].lotag = 0;
            p->timebeforeexit = GAMETICSPERSEC * 8;
            p->customexitsound = sector[p->cursectnum].hitag;
            return;
        default:
            if (sector[p->cursectnum].lotag >= 10000 && sector[p->cursectnum].lotag < 16383)
            {
                if (playerNum == screenpeek || (Gametypes[ud.coop].flags & GAMETYPE_COOPSOUND))
                    A_PlaySound(sector[p->cursectnum].lotag - 10000, p->i);
                sector[p->cursectnum].lotag = 0;
            }
            break;

        }
    }
}

static void P_CycleCameras(int32_t playerNum, int32_t viewscreenSprite)
{
    DukePlayer_t* p = g_player[playerNum].ps;

    // Try to find a camera sprite for the viewscreen.
    for (bssize_t SPRITES_OF(STAT_ACTOR, i))
    {
        if (PN(i) == CAMERA1 && SP(i) == 0 && sprite[viewscreenSprite].hitag == SLT(i))
        {
            sprite[i].yvel = 1;  // Using this camera
            A_PlaySound(MONITOR_ACTIVE, p->i);
            sprite[viewscreenSprite].owner = i;
            sprite[viewscreenSprite].yvel = 1;  // VIEWSCREEN_YVEL
            g_curViewscreen = viewscreenSprite; // g_curViewScreen

            int const playerSectnum = p->cursectnum;
            p->cursectnum = SECT(i);
            P_UpdateScreenPal(p);
            p->cursectnum = playerSectnum;
            p->newowner = i;

            P_UpdatePosWhenViewingCam(p);

            return;
        }
    }

    G_ClearCameras(p);
}

void P_CheckSectors(int snum)
{
    int i = -1,oldz;
    DukePlayer_t *p = g_player[snum].ps;
    short j,hitscanwall;

    P_DoSectorInteractions(snum);

    //After this point the the player effects the map with space

    if (sprite[p->i].extra <= 0)
        return;

    if (TEST_SYNC_KEY(g_player[snum].inputBits->bits, SK_OPEN))
    {
        Gv_SetVar(sysVarIDs.RETURN,0,p->i,snum);
        X_OnEvent(EVENT_USE, p->i, snum, -1);
        if (Gv_GetVar(sysVarIDs.RETURN,p->i,snum) != 0)
            g_player[snum].inputBits->bits &= ~BIT(SK_OPEN);
    }

    if (ud.cashman && TEST_SYNC_KEY(g_player[snum].inputBits->bits, SK_OPEN))
        A_SpawnMultiple(p->i, MONEY, 2);

    // Clear viewscreen if we try to move.
    if (p->newowner >= 0)
    {
        if (klabs(g_player[snum].inputBits->svel) > 768 || klabs(g_player[snum].inputBits->fvel) > 768)
        {
            G_ClearCameras(p);
            return;
        }
    }

    if (!TEST_SYNC_KEY(g_player[snum].inputBits->bits, SK_OPEN) && !TEST_SYNC_KEY(g_player[snum].inputBits->bits, SK_ESCAPE))
    {
        p->toggle_key_flag = 0;
    }
    else if (!p->toggle_key_flag)
    {
        if (TEST_SYNC_KEY(g_player[snum].inputBits->bits, SK_ESCAPE))
        {
            if (p->newowner >= 0)
            {
                G_ClearCameras(p);
                return;
            }
            return;
        }

        neartagsprite = -1;
        p->toggle_key_flag = 1;
        hitscanwall = -1;

        i = P_FindWall(p,&hitscanwall);

        if (i < 1280 && hitscanwall >= 0 && wall[hitscanwall].overpicnum == MIRROR)
            if (wall[hitscanwall].lotag > 0 && !A_CheckSoundPlaying(p->i,wall[hitscanwall].lotag) && snum == screenpeek)
            {
                A_PlaySound(wall[hitscanwall].lotag,p->i);
                return;
            }

        if (hitscanwall >= 0 && (wall[hitscanwall].cstat & CSTAT_WALL_MASKED))
            switch (wall[hitscanwall].overpicnum)
            {
            default:
                if (wall[hitscanwall].lotag)
                    return;
            }

        if (p->newowner >= 0)
            neartag(p->opos.x,p->opos.y,p->opos.z,sprite[p->i].sectnum,fix16_to_int(p->oq16ang),&neartagsector,&neartagwall,&neartagsprite,&neartaghitdist,1280L,1, NULL);
        else
        {
            neartag(p->pos.x,p->pos.y,p->pos.z,sprite[p->i].sectnum,fix16_to_int(p->oq16ang),&neartagsector,&neartagwall,&neartagsprite,&neartaghitdist,1280L,1, NULL);

            if (neartagsprite == -1 && neartagwall == -1 && neartagsector == -1)
                neartag(p->pos.x,p->pos.y,p->pos.z+(8<<8),sprite[p->i].sectnum, fix16_to_int(p->oq16ang),&neartagsector,&neartagwall,&neartagsprite,&neartaghitdist,1280L,1, NULL);

            if (neartagsprite == -1 && neartagwall == -1 && neartagsector == -1)
                neartag(p->pos.x,p->pos.y,p->pos.z+(16<<8),sprite[p->i].sectnum, fix16_to_int(p->oq16ang),&neartagsector,&neartagwall,&neartagsprite,&neartaghitdist,1280L,1, NULL);

            if (neartagsprite == -1 && neartagwall == -1 && neartagsector == -1)
            {
                neartag(p->pos.x,p->pos.y,p->pos.z+(16<<8),sprite[p->i].sectnum, fix16_to_int(p->oq16ang),&neartagsector,&neartagwall,&neartagsprite,&neartaghitdist,1280L,3, NULL);
                if (neartagsprite >= 0)
                {
                    if (G_CheckFemSprite(neartagsprite))
                        return;
                }

                neartagsprite = -1;
                neartagwall = -1;
                neartagsector = -1;
            }
        }

        if (p->newowner == -1 && neartagsprite == -1 && neartagsector == -1 && neartagwall == -1)
            if (isanunderoperator(sector[sprite[p->i].sectnum].lotag))
                neartagsector = sprite[p->i].sectnum;

        if (neartagsector >= 0 && (sector[neartagsector].lotag&16384))
            return;

        if (neartagsprite == -1 && neartagwall == -1)
            if (sector[p->cursectnum].lotag == ST_2_UNDERWATER)
            {
                oldz = A_CheckHitSprite(p->i,&neartagsprite);
                if (oldz > 1280) neartagsprite = -1;
            }

        if (neartagsprite >= 0)
        {
            if (P_ActivateSwitch(snum,neartagsprite,1)) return;

            switch (tileGetMapping(sprite[neartagsprite].picnum))
            {
                case TOILET__:
                case STALL__:
                    if (p->last_pissed_time == 0)
                    {
                        if (ud.lockout == 0) A_PlaySound(DUKE_URINATE,p->i);

                        p->last_pissed_time = 26*220;
                        p->transporter_hold = 29*2;
                        if (p->holster_weapon == 0)
                        {
                            p->holster_weapon = 1;
                            p->weapon_pos = -1;
                        }
                        if (sprite[p->i].extra <= (p->max_player_health-(p->max_player_health/10)))
                        {
                            sprite[p->i].extra += p->max_player_health/10;
                            p->last_extra = sprite[p->i].extra;
                        }
                        else if (sprite[p->i].extra < p->max_player_health)
                            sprite[p->i].extra = p->max_player_health;
                    }
                    else if (!A_CheckSoundPlaying(neartagsprite,FLUSH_TOILET))
                        A_PlaySound(FLUSH_TOILET,neartagsprite);
                    return;

                case NUKEBUTTON__:

                    P_FindWall(p,&j);
                    if (j >= 0 && wall[j].overpicnum == 0)
                        if (actor[neartagsprite].t_data[0] == 0)
                        {
                            if (DMFLAGS_TEST(DMFLAG_NOEXITS) && ud.multimode > 1)
                            {
                                actor[p->i].htpicnum = NUKEBUTTON;
                                actor[p->i].htextra = 250;
                            }
                            else
                            {
                                actor[neartagsprite].t_data[0] = 1;
                                sprite[neartagsprite].owner = p->i;
                                p->buttonpalette = sprite[neartagsprite].pal;
                                if (p->buttonpalette)
                                    ud.secretlevel = sprite[neartagsprite].lotag;
                                else ud.secretlevel = 0;
                            }
                        }
                    return;
                case WATERFOUNTAIN__:
                    if (actor[neartagsprite].t_data[0] != 1)
                    {
                        actor[neartagsprite].t_data[0] = 1;
                        sprite[neartagsprite].owner = p->i;

                        if (sprite[p->i].extra < p->max_player_health)
                        {
                            sprite[p->i].extra++;
                            A_PlaySound(DUKE_DRINKING,p->i);
                        }
                    }
                    return;
                case PLUG__:
                    A_PlaySound(SHORT_CIRCUIT,p->i);
                    sprite[p->i].extra -= 2+(krand()&3);
                    P_PalFrom(p, 32, 48, 48, 64);
                    break;
                case VIEWSCREEN__:
                case VIEWSCREEN2__:
                    P_CycleCameras(snum, neartagsprite);
                    return;
            }
        }

        if (TEST_SYNC_KEY(g_player[snum].inputBits->bits, SK_OPEN) == 0)
            return;

        if (p->newowner >= 0)
        {
            G_ClearCameras(p);
            return;
        }

        // OOF... OOF... WHERE IS IT?!
        if (neartagwall == -1 && neartagsector == -1 && neartagsprite == -1)
        {
            if (klabs(A_GetHitscanRange(p->i)) < 512)
            {
                if ((krand() & 255) < 16)
                    A_PlaySound(DUKE_SEARCH2, p->i);
                else A_PlaySound(DUKE_SEARCH, p->i);
                return;
            }
        }

        if (neartagwall >= 0)
        {
            if (wall[neartagwall].lotag > 0 && CheckDoorTile(wall[neartagwall].picnum))
            {
                if (hitscanwall == neartagwall || hitscanwall == -1)
                    P_ActivateSwitch(snum,neartagwall,0);
                return;
            }
            else if (p->newowner >= 0)
            {
                G_ClearCameras(p);
                return;
            }
        }

        if (neartagsector >= 0 && (sector[neartagsector].lotag&16384) == 0 && isanearoperator(sector[neartagsector].lotag))
        {
            i = headspritesect[neartagsector];
            while (i >= 0)
            {
                if (PN(i) == ACTIVATOR || PN(i) == MASTERSWITCH)
                    return;
                i = nextspritesect[i];
            }
            G_OperateSectors(neartagsector,p->i);
        }
        else if ((sector[sprite[p->i].sectnum].lotag&16384) == 0)
        {
            if (isanunderoperator(sector[sprite[p->i].sectnum].lotag))
            {
                i = headspritesect[sprite[p->i].sectnum];
                while (i >= 0)
                {
                    if (PN(i) == ACTIVATOR || PN(i) == MASTERSWITCH) return;
                    i = nextspritesect[i];
                }
                G_OperateSectors(sprite[p->i].sectnum,p->i);
            }
            else P_ActivateSwitch(snum,neartagwall,0);
        }
    }
}

