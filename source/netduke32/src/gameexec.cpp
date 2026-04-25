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

#include <time.h>

#include "duke3d.h"
#include "gamedef.h"
#include "scriplib.h"

#include "osdcmds.h"
#include "osd.h"

#include "gamestructures.h"

void G_RestoreMapState(mapstate_t *save);
void G_SaveMapState(mapstate_t *save);

vmstate_t vm;

int g_errorLineNum;
int g_tw;

static int X_DoExecute(void);

// Replace with mainline versions later.
#if defined _MSC_VER
#define VM_ABORT_IF(condition, fmt, ...)  \
    if (EDUKE32_PREDICT_FALSE(condition)) \
    {                                     \
        CON_ERRPRINTF(fmt, __VA_ARGS__);  \
        break;              \
    }
#else
#define VM_ABORT_IF(condition, ...)       \
    if (EDUKE32_PREDICT_FALSE(condition)) \
    {                                     \
        CON_ERRPRINTF(__VA_ARGS__);       \
        break;              \
    }
#endif

void VM_ScriptInfo(intptr_t const* const ptr, int const range)
{
    if (!ptr || (g_currentEvent == -1 && insptr == nullptr))
        return;

    int foundInst = 0;
    int lastLine = 0;
    char buf[128];

    for (auto pScript = max<intptr_t const*>(ptr - (range >> 2), apScript),
        p_end = min<intptr_t const*>(ptr + (range >> 1) + (range >> 2), apScript + g_scriptSize);
        pScript < p_end;
        ++pScript)
    {
        auto& v = *pScript;
        int const lineNum = VM_DECODE_LINE_NUMBER(v);
        int const vmInst = VM_DECODE_INST(v);

        if (lineNum && lineNum >= lastLine && lineNum < g_totalLines && (unsigned)vmInst < CON_OPCODE_END)
        {
            lastLine = lineNum;

            if (foundInst == 1)
                VLOG_F(LOG_VM, "%s", buf);
            else if (foundInst == 2)
                LOG_F(ERROR, "%s", buf);

            foundInst = 1;

            if (lineNum == VM_DECODE_LINE_NUMBER(g_tw) || vmInst == VM_DECODE_INST(g_tw))
                foundInst++;

            Bsprintf(buf, "%8s:%5d: [0x%04x] %s", VM_FILENAME(pScript), lineNum, int32_t((pScript - apScript) * sizeof(intptr_t)), VM_GetKeywordForID(vmInst));
        }
        else if (foundInst)
        {
            char buf2[16];

            if (bitmap_test(bitptr, pScript - apScript))
                Bsprintf(buf2, " [0x%04x]", int32_t((pScript - apScript) * sizeof(intptr_t)));
            else Bsprintf(buf2, " %d", int32_t(*pScript));

            Bstrcat(buf, buf2);
        }
    }

    if (ptr == insptr)
    {
        if (vm.pUSprite)
            VLOG_F(LOG_VM, "current actor: %d (%d)", vm.spriteNum, vm.pUSprite->picnum);
    }
}

void X_OnEvent(int iEventID, int iActor, int iPlayer, int lDist)
{
    ERROR_CONTEXT("VM event execution", &EventNames[iEventID][0]);

    if (iEventID<0 || iEventID >= MAXEVENTS)
    {
        OSD_Printf(CON_ERROR "invalid event ID",g_errorLineNum,VM_GetKeywordForID(g_tw));
        return;
    }

    if (!VM_HaveEvent(iEventID))
    {
        //Bsprintf(g_szBuf,"No event found for %d",iEventID);
        //AddLog(g_szBuf);
        return;
    }

    {
        int og_i=vm.spriteNum, og_p=vm.playerNum, okillit_flag=vm.flags;
        int og_x=vm.playerDist;// *og_t=vm.pData;
        int oevent = g_currentEvent;
        intptr_t* oinsptr = insptr;
        int32_t* og_t = vm.pData;
        spritetype *og_sp=vm.pSprite;

        vm.spriteNum = iActor&(MAXSPRITES-1);
        vm.playerNum = iPlayer&(MAXPLAYERS-1);
        vm.playerDist = lDist;    // ?
        vm.pSprite = &sprite[vm.spriteNum];
        vm.pData = &actor[vm.spriteNum].t_data[0];

        g_currentEvent = iEventID;
        vm.flags = 0;

        insptr = apScript + apScriptEvents[iEventID]; 

        while (!X_DoExecute());

        if (vm.flags & VM_KILL)
        {
            // if player was set to squish, first stop that...
            if (vm.playerNum >= 0)
            {
                if (g_player[vm.playerNum].ps->actorsqu == vm.spriteNum)
                    g_player[vm.playerNum].ps->actorsqu = -1;
            }
            A_DeleteSprite(vm.spriteNum);
        }

        // restore old values...
        vm.spriteNum = og_i;
        vm.playerNum = og_p;
        vm.playerDist = og_x;
        vm.pSprite = og_sp;
        vm.pData = og_t;
        vm.flags = okillit_flag;
        g_currentEvent = oevent;
        insptr = oinsptr;

        //AddLog("End of Execution");
    }
}

int X_OnEventWithReturn(int iEventID, int iActor, int iPlayer, int nReturn)
{
    if (iEventID < 0 || iEventID >= MAXEVENTS)
    {
        OSD_Printf(CON_ERROR "invalid event ID", g_errorLineNum, VM_GetKeywordForID(g_tw));
        return nReturn;
    }

    int oldReturn = Gv_GetVar(sysVarIDs.RETURN, iActor, iPlayer);

    Gv_SetVar(sysVarIDs.RETURN, nReturn, iActor, iPlayer);
    X_OnEvent(iEventID, iActor, iPlayer, -1);

    int newReturn = Gv_GetVar(sysVarIDs.RETURN, iActor, iPlayer);

    Gv_SetVar(sysVarIDs.RETURN, oldReturn, iActor, iPlayer);

    return newReturn;
}

static int A_CheckSquished(int i, int p)
{
    auto const pSector = (usectorptr_t)&sector[SECT(i)];
    auto const pSprite = &sprite[i];
    auto const pActor = &actor[i];

    if (pSector->lotag == ST_23_SWINGING_DOOR || (pSprite->picnum == APLAYER && ud.noclip) ||
        (pSector->lotag == ST_1_ABOVE_WATER && !A_CheckNoSE7Water((uspriteptr_t)pSprite, pSprite->sectnum, pSector->lotag, NULL)))
        return 0;

    int32_t floorZ = pSector->floorz;
    int32_t ceilZ = pSector->ceilingz;
#ifdef YAX_ENABLE
    int16_t cb, fb;

    yax_getbunches(pSprite->sectnum, &cb, &fb);

    if (cb >= 0 && (pSector->ceilingstat & 512) == 0)  // if ceiling non-blocking...
        ceilZ -= ZOFFSET5;  // unconditionally don't squish... yax_getneighborsect is slowish :/
    if (fb >= 0 && (pSector->floorstat & 512) == 0)
        floorZ += ZOFFSET5;
#endif

    if (pSprite->pal == 1 ? (floorZ - ceilZ >= ZOFFSET5 || (pSector->lotag & 32768u)) : (floorZ - ceilZ >= ZOFFSET4))
        return 0;

    P_DoQuote(QUOTE_SQUISHED, g_player[p].ps);

    if (A_CheckEnemySprite(pSprite))
        pSprite->xvel = 0;

#ifndef EDUKE32_STANDALONE
    if (EDUKE32_PREDICT_FALSE(pSprite->pal == 1)) // frozen
    {
        pActor->htpicnum = SHOTSPARK1;
        pActor->htextra = 1;
        return 0;
    }
#endif

    return 1;
}

static void P_ForceAngle(DukePlayer_t *p)
{
    int n = 128-(krand()&255);

    p->q16horiz += F16(64);
    p->return_to_center = 9;
    p->look_ang = n>>1;
    p->rotscrnang = n>>1;
}

static int A_Dodge(spritetype *s)
{
    int bx,by,bxvect,byvect,d,i;
    int mx = s->x, my = s->y;
    int mxvect = sintable[(s->ang+512)&2047];
    int myvect = sintable[s->ang&2047];

    if (A_CheckEnemySprite(s) && s->extra <= 0) // hack
        return 0;

    for (i=headspritestat[STAT_PROJECTILE];i>=0;i=nextspritestat[i]) //weapons list
    {
        if (OW(i) == i || SECT(i) != s->sectnum)
            continue;

        bx = SX(i)-mx;
        by = SY(i)-my;
        bxvect = sintable[(SA(i)+512)&2047];
        byvect = sintable[SA(i)&2047];

        if (mxvect*bx + myvect*by >= 0)
            if (bxvect*bx + byvect*by < 0)
            {
                d = bxvect*by - byvect*bx;
                if (klabs(d) < 65536*64)
                {
                    s->ang -= 512+(krand()&1024);
                    return 1;
                }
            }
    }
    return 0;
}

// Also used in player.cpp
int A_GetFurthestAngle(int const spriteNum, int const angDiv)
{
    auto const pSprite = (uspriteptr_t)&sprite[spriteNum];

    if (pSprite->picnum != APLAYER && ((actor[spriteNum].t_data[0] & 63) > 2))
        return (pSprite->ang + 1024) & 2047;

    int       furthestAngle = 0;
    int const angIncs = tabledivide32_noinline(2048, angDiv);
    int32_t   greatestDist = INT32_MIN;
    hitdata_t hit;

    for (native_t j = pSprite->ang; j < (2048 + pSprite->ang); j += angIncs)
    {
        vec3_t origin = *(const vec3_t*)pSprite;
        origin.z -= (8 << 8);//ZOFFSET3;
        hitscan(&origin, pSprite->sectnum, sintable[(j + 512) & 2047], sintable[j & 2047], 0, &hit, CLIPMASK1);

        int const hitDist = klabs(hit.x - pSprite->x) + klabs(hit.y - pSprite->y);

        if (hitDist > greatestDist)
        {
            greatestDist = hitDist;
            furthestAngle = j;
        }
    }

    return furthestAngle & 2047;
}

static int A_FurthestVisiblePoint(int iActor,spritetype *ts,int *dax,int *day)
{
    if ((actor[iActor].t_data[0]&63)) return -1;
    {
        short angincs;
        int d, da;//, d, cd, ca,tempx,tempy,cx,cy;
        int j;
        spritetype *s = &sprite[iActor];
        hitdata_t hit;

        if (ud.multimode < 2 && ud.player_skill < 3)
            angincs = 2048/2;
        else angincs = 2048/(1+(krand()&1));

        for (j=ts->ang;j<(2048+ts->ang);j+=(angincs-(krand()&511)))
        {
            vec3_t origin = ts->xyz;
            origin.z -= ZOFFSET2;

            hitscan(&origin, ts->sectnum, sintable[(j + 512) & 2047], sintable[j & 2047], 16384 - (krand() & 32767), &hit, CLIPMASK1);

            d = klabs(hit.x-ts->x)+klabs(hit.y-ts->y);
            da = klabs(hit.x-s->x)+klabs(hit.y-s->y);

            if (d < da && hit.sect > -1)
                if (cansee(hit.x, hit.y, hit.z,hit.sect,s->x,s->y,s->z-(16<<8),s->sectnum))
                {
                    *dax = hit.x;
                    *day = hit.y;
                    return hit.sect;
                }
        }
        return -1;
    }
}

void A_GetZLimits(int iActor)
{
    spritetype *s = &sprite[iActor];

    if (s->statnum == STAT_PLAYER || s->statnum == STAT_STANDABLE || s->statnum == STAT_ZOMBIEACTOR || s->statnum == STAT_ACTOR || s->statnum == STAT_PROJECTILE)
    {
        int hz,lz,zr = 127L;
        int32_t cstat = s->cstat;

        s->cstat = 0;

        if (s->statnum == 4)
            zr = 4L;

        getzrange_old(s->x,s->y,s->z-(ACTOR_FLOOR_OFFSET),s->sectnum,&actor[iActor].ceilingz,&hz,&actor[iActor].floorz,&lz,zr,CLIPMASK0);

        s->cstat = cstat;

        if ((lz&49152) == 49152 && (sprite[lz&(MAXSPRITES-1)].cstat&48) == 0)
        {
            lz &= (MAXSPRITES-1);
            if (A_CheckEnemySprite(&sprite[lz]) && sprite[lz].pal != 1)
            {
                if (s->statnum != STAT_PROJECTILE)
                {
                    actor[iActor].dispicnum = -4; // No shadows on actors
                    s->xvel = -256;
                    A_SetSprite(iActor,CLIPMASK0);
                }
            }
            else if (sprite[lz].picnum == APLAYER && A_CheckEnemySprite(s))
            {
                actor[iActor].dispicnum = -4; // No shadows on actors
                s->xvel = -256;
                A_SetSprite(iActor,CLIPMASK0);
            }
            else if (s->statnum == STAT_PROJECTILE && sprite[lz].picnum == APLAYER)
                if (s->owner == lz)
                {
                    actor[iActor].ceilingz = sector[s->sectnum].ceilingz;
                    actor[iActor].floorz   = sector[s->sectnum].floorz;
                }
        }
    }
    else
    {
        actor[iActor].ceilingz = sector[s->sectnum].ceilingz;
        actor[iActor].floorz   = sector[s->sectnum].floorz;
    }
}

void A_Fall(int iActor)
{
    spritetype *s = &sprite[iActor];
    int hz,lz,c = g_spriteGravity;
    int32_t cstat;

    if (G_CheckForSpaceFloor(s->sectnum))
        c = 0;
    else
    {
        if (G_CheckForSpaceCeiling(s->sectnum) || sector[s->sectnum].lotag == ST_2_UNDERWATER)
            c = g_spriteGravity/6;
    }

    if ((s->statnum == 1 || s->statnum == 10 || s->statnum == 2 || s->statnum == 6))
    {
        cstat = s->cstat;
        s->cstat = 0;
        getzrange_old(s->x, s->y, s->z - (ACTOR_FLOOR_OFFSET), s->sectnum, &actor[iActor].ceilingz, &hz, &actor[iActor].floorz, &lz, 127L, CLIPMASK0);
        s->cstat = cstat;
    }
    else
    {
        actor[iActor].ceilingz = sector[s->sectnum].ceilingz;
        actor[iActor].floorz   = sector[s->sectnum].floorz;
    }

    int fbunch = (sector[s->sectnum].floorstat & 512) ? -1 : yax_getbunch(s->sectnum, YAX_FLOOR);

    if (s->z < actor[iActor].floorz-(ACTOR_FLOOR_OFFSET) || fbunch >= 0)
    {
        if (sector[s->sectnum].lotag == ST_2_UNDERWATER && s->zvel > 3122)
            s->zvel = 3144;

        s->z += s->zvel = min(ACTOR_MAXFALLINGZVEL, s->zvel+c);
    }

    if (fbunch >= 0)
        setspritez(iActor, &s->xyz);
    else if (s->z >= actor[iActor].floorz-(ACTOR_FLOOR_OFFSET))
    {
        s->z = actor[iActor].floorz - ACTOR_FLOOR_OFFSET;
        s->zvel = 0;
    }
}

int G_GetAngleDelta(int a,int na)
{
    a &= 2047;
    na &= 2047;

    if (klabs(a-na) < 1024)
    {
//        OSD_Printf("G_GetAngleDelta() returning %d\n",na-a);
        return (na-a);
    }

    if (na > 1024) na -= 2048;
    if (a > 1024) a -= 2048;

    na -= 2048;
    a -= 2048;
//    OSD_Printf("G_GetAngleDelta() returning %d\n",na-a);
    return (na-a);
}

static int A_GetVerticalVel(ActorData_t const* const pActor)
{
    int32_t moveScriptOfs = AC_MOVE_ID(pActor->t_data);

    return ((unsigned)moveScriptOfs < (unsigned)g_scriptSize - 1) ? apScript[moveScriptOfs + 1] : 0;
}

static int32_t A_GetWaterZOffset(int const spriteNum)
{
    auto const pSprite = (uspriteptr_t)&sprite[spriteNum];
    auto const pActor = &actor[spriteNum];

    if (sector[pSprite->sectnum].lotag == ST_1_ABOVE_WATER)
    {
        if (A_CheckSpriteFlags(spriteNum, SFLAG_NOWATERDIP))
            return 0;

        // fix for flying/jumping monsters getting stuck in water
        if ((AC_MOVFLAGS(pSprite, pActor) & jumptoplayer_only) || (G_TileHasActor(pSprite->picnum) && A_GetVerticalVel(pActor) != 0))
            return 0;

        return ACTOR_ONWATER_ADDZ;// -((int32_t)pActor->waterzoffset << 8);
    }

    return 0;
}

static void VM_Fall(int const spriteNum, spritetype* const pSprite)
{
    int spriteGravity = g_spriteGravity;

    pSprite->xoffset = pSprite->yoffset = 0;

    if (sector[pSprite->sectnum].lotag == ST_2_UNDERWATER || EDUKE32_PREDICT_FALSE(G_CheckForSpaceCeiling(pSprite->sectnum)))
        spriteGravity = g_spriteGravity / 6;
    else if (EDUKE32_PREDICT_FALSE(G_CheckForSpaceFloor(pSprite->sectnum)))
        spriteGravity = 0;

    if (!actor[spriteNum].cgg-- || (sector[pSprite->sectnum].floorstat & 2))
        actor[spriteNum].cgg = 3;

    A_GetZLimits(spriteNum);

    if (pSprite->z < actor[spriteNum].floorz - ACTOR_FLOOR_OFFSET)
    {
        // Free fall.
        pSprite->zvel = min(pSprite->zvel + spriteGravity, ACTOR_MAXFALLINGZVEL);
        int newZ = pSprite->z + pSprite->zvel;

#ifdef YAX_ENABLE
        if (yax_getbunch(pSprite->sectnum, YAX_FLOOR) >= 0 && (sector[pSprite->sectnum].floorstat & 512) == 0)
            setspritez(spriteNum, &pSprite->xyz);
        else
#endif
            if (newZ > actor[spriteNum].floorz - ACTOR_FLOOR_OFFSET)
                newZ = actor[spriteNum].floorz - ACTOR_FLOOR_OFFSET;

        pSprite->z = newZ;
        return;
    }

    // Preliminary new z position of the actor.
    int newZ = actor[spriteNum].floorz - ACTOR_FLOOR_OFFSET;

    if (A_CheckEnemySprite(pSprite) || (pSprite->picnum == APLAYER && pSprite->owner >= 0))
    {
        if (pSprite->zvel > 3084 && pSprite->extra <= 1)
        {
            // I'm guessing this DRONE check is from a beta version of the game
            // where they crashed into the ground when killed
#ifndef EDUKE32_STANDALONE
            if (!FURY && !(pSprite->picnum == APLAYER && pSprite->extra > 0) && pSprite->pal != 1 && pSprite->picnum != DRONE)
            {
                A_DoGuts(spriteNum, JIBS6, 15);
                A_PlaySound(SQUISHED, spriteNum);
                A_Spawn(spriteNum, BLOODPOOL);
            }
#endif
            actor[spriteNum].htpicnum = SHOTSPARK1;
            actor[spriteNum].htextra = 1;
            pSprite->zvel = 0;
        }
        else if (pSprite->zvel > 2048 && sector[pSprite->sectnum].lotag != ST_1_ABOVE_WATER)
        {
            int16_t newsect = pSprite->sectnum;

            pushmove(&pSprite->xyz, &newsect, 128, 4 << 8, 4 << 8, CLIPMASK0);
            if ((unsigned)newsect < MAXSECTORS)
                changespritesect(spriteNum, newsect);

#ifndef EDUKE32_STANDALONE
            if (!FURY)
                A_PlaySound(THUD, spriteNum);
#endif
        }
    }

    if (sector[pSprite->sectnum].lotag == ST_1_ABOVE_WATER && actor[spriteNum].floorz == yax_getflorzofslope(pSprite->sectnum, pSprite->xy))
    {
        pSprite->z = newZ + A_GetWaterZOffset(spriteNum);
        return;
    }

    pSprite->z = newZ;
    pSprite->zvel = 0;
}

static inline void X_AlterAng(int a)
{
    int const elapsedTics = (AC_COUNT(vm.pData)) & 31;

    auto const moveptr = apScript + AC_MOVE_ID(vm.pData);
    auto& hvel = moveptr[0];
    auto& vvel = moveptr[1];

    vm.pSprite->xvel += (hvel - vm.pSprite->xvel) / 5;
    if (vm.pSprite->zvel < 648)
        vm.pSprite->zvel += ((vvel << 4) - vm.pSprite->zvel) / 5;

    if (A_CheckEnemySprite(vm.pSprite) && vm.pSprite->extra <= 0) // hack
        return;

    if (a & seekplayer)
    {
        int aang = vm.pSprite->ang, angdif, goalang;
        int j = g_player[vm.playerNum].ps->holoduke_on;

        // NOTE: looks like 'owner' is set to target sprite ID...

        if (j >= 0 && cansee(sprite[j].x, sprite[j].y, sprite[j].z, sprite[j].sectnum, vm.pSprite->x, vm.pSprite->y, vm.pSprite->z, vm.pSprite->sectnum))
            vm.pSprite->owner = j;
        else vm.pSprite->owner = g_player[vm.playerNum].ps->i;

        if (sprite[vm.pSprite->owner].picnum == APLAYER)
            goalang = getangle(actor[vm.spriteNum].lastvx - vm.pSprite->x, actor[vm.spriteNum].lastvy - vm.pSprite->y);
        else
            goalang = getangle(sprite[vm.pSprite->owner].x - vm.pSprite->x, sprite[vm.pSprite->owner].y - vm.pSprite->y);

        if (vm.pSprite->xvel && vm.pSprite->picnum != DRONE)
        {
            angdif = G_GetAngleDelta(aang, goalang);

            if (elapsedTics < 2)
            {
                if (klabs(angdif) < 256)
                {
                    j = 128 - (krand() & 256);
                    vm.pSprite->ang += j;
                    if (A_GetHitscanRange(vm.spriteNum) < 844)
                        vm.pSprite->ang -= j;
                }
            }
            else if (elapsedTics > 18 && elapsedTics < 26) // choose
            {
                if (klabs(angdif >> 2) < 128) vm.pSprite->ang = goalang;
                else vm.pSprite->ang += angdif >> 2;
            }
        }
        else vm.pSprite->ang = goalang;
    }

    if (elapsedTics < 1)
    {
        int j = 2;
        if (a & furthestdir)
        {
            vm.pSprite->ang = A_GetFurthestAngle(vm.spriteNum, j);
            vm.pSprite->owner = g_player[vm.playerNum].ps->i;
        }

        if (a & fleeenemy)
        {
            vm.pSprite->ang = A_GetFurthestAngle(vm.spriteNum, j); // += angdif; //  = G_GetAngleDelta(aang,goalang)>>1;
        }
    }
}

static void X_Move(void)
{
    int l;
    int goalang, angdif;
    int daxvel;
    int deadflag = (A_CheckEnemySprite(vm.pSprite) && vm.pSprite->extra <= 0);

    auto const movflagsptr = &AC_MOVFLAGS(vm.pSprite, &actor[vm.spriteNum]);
    int const movflags = (*movflagsptr == (remove_pointer_t<decltype(movflagsptr)>) - 1) ? 0 : *movflagsptr;

    AC_COUNT(vm.pData)++;

    if (AC_MOVE_ID(vm.pData) == 0 || movflags == 0)
    {
        if (deadflag || (actor[vm.spriteNum].bpos.xy != vm.pSprite->xy))
            setsprite(vm.spriteNum, &vm.pSprite->xyz);

        return;
    }

    if (movflags & face_player && !deadflag)
    {
        if (g_player[vm.playerNum].ps->newowner >= 0)
            goalang = getangle(g_player[vm.playerNum].ps->opos.x - vm.pSprite->x, g_player[vm.playerNum].ps->opos.y - vm.pSprite->y);
        else goalang = getangle(g_player[vm.playerNum].ps->pos.x - vm.pSprite->x, g_player[vm.playerNum].ps->pos.y - vm.pSprite->y);
        angdif = G_GetAngleDelta(vm.pSprite->ang, goalang) >> 2;
        if ((angdif > -8 && angdif < 0) || (angdif < 8 && angdif > 0))
            angdif *= 2;
        vm.pSprite->ang += angdif;
    }

    if (movflags & spin && !deadflag)
        vm.pSprite->ang += sintable[((AC_COUNT(vm.pData)<<3)&2047)]>>6;

    if (movflags & face_player_slow && !deadflag)
    {
        if (g_player[vm.playerNum].ps->newowner >= 0)
            goalang = getangle(g_player[vm.playerNum].ps->opos.x - vm.pSprite->x, g_player[vm.playerNum].ps->opos.y - vm.pSprite->y);
        else goalang = getangle(g_player[vm.playerNum].ps->pos.x - vm.pSprite->x, g_player[vm.playerNum].ps->pos.y - vm.pSprite->y);
        angdif = G_GetAngleDelta(vm.pSprite->ang, goalang) >> 4;
        if ((angdif > -8 && angdif < 0) || (angdif < 8 && angdif > 0))
            angdif *= 2;
        vm.pSprite->ang += angdif;
    }

    if (((movflags & jumptoplayer_bits) == jumptoplayer_bits) && !deadflag)
    {
        if (AC_COUNT(vm.pData) < 16)
            vm.pSprite->zvel -= (sintable[(512+(AC_COUNT(vm.pData)<<4))&2047]>>5);
    }

    if (movflags & face_player_smart && !deadflag)
    {
        int newx = g_player[vm.playerNum].ps->pos.x + (g_player[vm.playerNum].ps->vel.x / 768);
        int newy = g_player[vm.playerNum].ps->pos.y + (g_player[vm.playerNum].ps->vel.y / 768);

        goalang = getangle(newx - vm.pSprite->x, newy - vm.pSprite->y);
        angdif = G_GetAngleDelta(vm.pSprite->ang, goalang) >> 2;
        if ((angdif > -8 && angdif < 0) || (angdif < 8 && angdif > 0))
            angdif *= 2;
        vm.pSprite->ang += angdif;
    }

    if (EDUKE32_PREDICT_FALSE((unsigned)AC_MOVE_ID(vm.pData) >= (unsigned)g_scriptSize - 1))
    {
        AC_MOVE_ID(vm.pData) = 0;
        VLOG_F(LOG_VM, "clearing bad moveptr for actor %d (%d)", vm.spriteNum, vm.pUSprite->picnum);
        return;
    }

    auto const moveptr = apScript + AC_MOVE_ID(vm.pData);
    auto& hvel = moveptr[0];
    auto& vvel = moveptr[1];

    if (movflags & geth)
        vm.pSprite->xvel += (hvel - vm.pSprite->xvel) >> 1;

    if (movflags & getv)
        vm.pSprite->zvel += (16 * vvel - vm.pSprite->zvel) >> 1;

    if (movflags & dodgebullet && !deadflag)
        A_Dodge(vm.pSprite);

    if (vm.pSprite->picnum != APLAYER)
        X_AlterAng(movflags);

    if (vm.pSprite->xvel > -6 && vm.pSprite->xvel < 6)
        vm.pSprite->xvel = 0;

    int isEnemy = A_CheckEnemySprite(vm.pSprite);

    if (vm.pSprite->xvel || vm.pSprite->zvel)
    {
        if (isEnemy && vm.pSprite->picnum != ROTATEGUN)
        {
            if ((vm.pSprite->picnum == DRONE || vm.pSprite->picnum == COMMANDER) && vm.pSprite->extra > 0)
            {
                if (vm.pSprite->picnum == COMMANDER)
                {
                    actor[vm.spriteNum].floorz = l = yax_getflorzofslope(vm.pSprite->sectnum, vm.pSprite->xy);
                    if (vm.pSprite->z > (l - (8 << 8)))
                    {
                        if (vm.pSprite->z > (l - (8 << 8))) vm.pSprite->z = l - (8 << 8);
                        vm.pSprite->zvel = 0;
                    }

                    actor[vm.spriteNum].ceilingz = l = yax_getceilzofslope(vm.pSprite->sectnum, vm.pSprite->xy);
                    if ((vm.pSprite->z - l) < (80 << 8))
                    {
                        vm.pSprite->z = l + (80 << 8);
                        vm.pSprite->zvel = 0;
                    }
                }
                else
                {
                    if (vm.pSprite->zvel > 0)
                    {
                        actor[vm.spriteNum].floorz = l = yax_getflorzofslope(vm.pSprite->sectnum, vm.pSprite->xy);
                        if (vm.pSprite->z > (l - (30 << 8)))
                            vm.pSprite->z = l - (30 << 8);
                    }
                    else
                    {
                        actor[vm.spriteNum].ceilingz = l = yax_getceilzofslope(vm.pSprite->sectnum, vm.pSprite->xy);
                        if ((vm.pSprite->z - l) < (50 << 8))
                        {
                            vm.pSprite->z = l + (50 << 8);
                            vm.pSprite->zvel = 0;
                        }
                    }
                }
            }
            else if (vm.pSprite->picnum != ORGANTIC)
            {
                if (vm.pSprite->zvel > 0 && actor[vm.spriteNum].floorz < vm.pSprite->z)
                    vm.pSprite->z = actor[vm.spriteNum].floorz;
                if (vm.pSprite->zvel < 0)
                {
                    l = yax_getceilzofslope(vm.pSprite->sectnum, vm.pSprite->xy);
                    if ((vm.pSprite->z - l) < (66 << 8))
                    {
                        vm.pSprite->z = l + (66 << 8);
                        vm.pSprite->zvel >>= 1;
                    }
                }
            }
        }
        else if (vm.pSprite->picnum == APLAYER)
            if ((vm.pSprite->z - actor[vm.spriteNum].ceilingz) < (32 << 8))
                vm.pSprite->z = actor[vm.spriteNum].ceilingz + (32 << 8);

        daxvel = vm.pSprite->xvel;
        angdif = vm.pSprite->ang;

        if (isEnemy && vm.pSprite->picnum != ROTATEGUN)
        {
            if (vm.playerDist < 960 && vm.pSprite->xrepeat > 16)
            {

                daxvel = -(1024 - vm.playerDist);
                angdif = getangle(g_player[vm.playerNum].ps->pos.x - vm.pSprite->x, g_player[vm.playerNum].ps->pos.y - vm.pSprite->y);

                if (vm.playerDist < 512)
                {
                    g_player[vm.playerNum].ps->vel.x = 0;
                    g_player[vm.playerNum].ps->vel.y = 0;
                }
                else
                {
                    g_player[vm.playerNum].ps->vel.x = mulscale(g_player[vm.playerNum].ps->vel.x, g_player[vm.playerNum].ps->runspeed - 0x2000, 16);
                    g_player[vm.playerNum].ps->vel.y = mulscale(g_player[vm.playerNum].ps->vel.y, g_player[vm.playerNum].ps->runspeed - 0x2000, 16);
                }
            }
            else if (vm.pSprite->picnum != DRONE && vm.pSprite->picnum != SHARK && vm.pSprite->picnum != COMMANDER)
            {
                if (g_player[vm.playerNum].ps->actorsqu == vm.spriteNum)
                    return;

                if (!A_CheckSpriteFlags(vm.spriteNum, SFLAG_SMOOTHMOVE))
                {
                    if (AC_COUNT(vm.pData) & 1)
                        return;

                    daxvel <<= 1;
                }
            }
        }

        actor[vm.spriteNum].movflag = A_MoveSprite(vm.spriteNum, (daxvel*(sintable[(angdif+512)&2047]))>>14,
                                               (daxvel*(sintable[angdif&2047]))>>14,vm.pSprite->zvel, (movflags & noclip) ? 0 : CLIPMASK0);
    }

    if (isEnemy)
    {
        if (sector[vm.pSprite->sectnum].ceilingstat & 1)
            vm.pSprite->shade += (sector[vm.pSprite->sectnum].ceilingshade - vm.pSprite->shade) >> 1;
        else vm.pSprite->shade += (sector[vm.pSprite->sectnum].floorshade - vm.pSprite->shade) >> 1;

        if (sector[vm.pSprite->sectnum].floorpicnum == MIRROR)
            A_DeleteSprite(vm.spriteNum);
    }
}

void addweapon_maybeswitch(DukePlayer_t *ps, int32_t weap)
{
    if ((ps->weaponswitch & 1) && (ps->weaponswitch & 4))
    {
        int32_t snum = sprite[ps->i].yvel;
        int32_t i, w, new_wchoice = -1, curr_wchoice = -1;

        for (i=0; i<10 && (new_wchoice < 0 || curr_wchoice < 0); i++)
        {
            w = g_player[snum].wchoice[i];

            if (w == 0) w = 9;
            else w--;

            if (w == ps->curr_weapon)
                curr_wchoice = i;
            if (w == weap)
                new_wchoice = i;
        }

        P_AddWeapon(ps, weap, (new_wchoice < curr_wchoice));
    }

    P_AddWeapon(ps, weap, (ps->weaponswitch & 1));
}

void addweapon_addammo_common(DukePlayer_t *ps, int32_t weap, int32_t amount)
{
    P_AddAmmo(weap, ps, amount);

    if (ps->curr_weapon == KNEE_WEAPON && ps->gotweapon[weap])
        addweapon_maybeswitch(ps, weap);
}

static void VM_AddWeapon(int32_t weap, int32_t amount, DukePlayer_t *ps)
{
    if ((unsigned)weap >= MAX_WEAPONS)
    {
        CON_ERRPRINTF("Invalid weapon ID %d\n", weap);
        return;
    }

    if (!ps->gotweapon[weap])
    {
        addweapon_maybeswitch(ps, weap);
    }
    else if (ps->ammo_amount[weap] >= ps->max_ammo_amount[weap])
    {
        vm.flags |= VM_NOEXECUTE;
        return;
    }

    addweapon_addammo_common(ps, weap, amount);
}

static void VM_AddInventory(DukePlayer_t* const pPlayer, int const itemNum, int const nAmount)
{
    switch (itemNum)
    {
        case GET_STEROIDS:
        case GET_SCUBA:
        case GET_HOLODUKE:
        case GET_JETPACK:
        case GET_HEATS:
        case GET_FIRSTAID:
        case GET_BOOTS:
            pPlayer->inven_icon = inv_to_icon[itemNum];
            pPlayer->inv_amount[itemNum] = nAmount;
            break;

        case GET_SHIELD:
        {
            int16_t& shield_amount = pPlayer->inv_amount[GET_SHIELD];
            shield_amount = min(shield_amount + nAmount, pPlayer->max_shield_amount);
            break;
        }

        case GET_ACCESS:
        {
            char keyname[7];

            switch (vm.pSprite->pal)
            {
                case  0:
                    Bsprintf(keyname, "Blue");
                    ud.got_access |= 1;
                    break;
                case 21:
                    Bsprintf(keyname, "Red");
                    ud.got_access |= 2;
                    break;
                case 23:
                    Bsprintf(keyname, "Yellow");
                    ud.got_access |= 4;
                    break;
            }

            if (ud.multimode > 1)
            {
                Bsprintf(tempbuf, "^12%s^12 has acquired a ^%d%s^12 key!", &g_player[vm.playerNum].user_name[0], TrackerCast(vm.pSprite->pal), keyname);
                G_AddUserQuote(tempbuf);
            }

            break;
        }

        default: CON_ERRPRINTF("invalid inventory item %d", itemNum); break;
    }
}

static int G_StartTrackSlot(int const volumeNum, int const levelNum)
{
    if ((unsigned)volumeNum <= MAXVOLUMES && (unsigned)levelNum < MAXLEVELS)
    {
        int trackNum = MAXLEVELS * volumeNum + levelNum;

        return S_TryPlaySpecialMusic(trackNum);
    }

    return 1;
}

static int G_StartTrackSlotWrap(int const volumeNum, int const levelNum)
{
    if (EDUKE32_PREDICT_FALSE(G_StartTrackSlot(volumeNum, levelNum)))
    {
        CON_ERRPRINTF("invalid level %d or null music for volume %d level %d\n", levelNum, volumeNum, levelNum);
        return 1;
    }

    return 0;
}

static void G_ShowView(vec3_t vec, fix16_t a, fix16_t horiz, int sect, int ix1, int iy1, int ix2, int iy2, int unbiasedp)
{
    int x1 = min(ix1, ix2);
    int x2 = max(ix1, ix2);
    int y1 = min(iy1, iy2);
    int y2 = max(iy1, iy2);

    if (!unbiasedp)
    {
        // The showview command has a rounding bias towards zero,
        // e.g. floor((319*1680)/320) == 1674
        x1 = scale(x1, xdim, 320);
        y1 = scale(y1, ydim, 200);
        x2 = scale(x2, xdim, 320);
        y2 = scale(y2, ydim, 200);
    }
    else
    {
        // This will map the maximum 320-based coordinate to the
        // maximum real screen coordinate:
        // floor((319*1679)/319) == 1679
        x1 = scale(x1, xdim - 1, 319);
        y1 = scale(y1, ydim - 1, 199);
        x2 = scale(x2, xdim - 1, 319);
        y2 = scale(y2, ydim - 1, 199);
    }

    horiz = fix16_clamp(horiz, F16(HORIZ_MIN), F16(HORIZ_MAX));

    int const viewingRange = viewingrange;
    int const yxAspect = yxaspect;

    videoSetViewableArea(x1, y1, x2, y2);
    renderSetAspect(viewingRange, yxAspect);
    int const smoothratio = calc_smoothratio(totalclock, ototalclock);
    G_DoInterpolations(smoothratio);
    if (!display_mirror)
        G_HandleMirror(vec.x, vec.y, vec.z, a, horiz, smoothratio);
    yax_preparedrawrooms();
    renderDrawRoomsQ16(vec.x, vec.y, vec.z, a, horiz, sect);
    yax_drawrooms(G_DoSpriteAnimations, sect, 0, smoothratio);

    display_mirror = 2;
    G_DoSpriteAnimations(vec.x, vec.y, vec.z, fix16_to_int(a), smoothratio);
    display_mirror = 0;
    renderDrawMasks();
    G_RestoreInterpolations();
    G_UpdateScreenArea();
    renderSetAspect(viewingRange, yxAspect);
}

static inline void SetArray(int const arrayNum, int const arrayIndex, int const newValue)
{
    auto& arr = aGameArrays[arrayNum];

#if 0 // this needs to be a compile time check, not something done at runtime...
    if (EDUKE32_PREDICT_FALSE(arr.flags & GAMEARRAY_READONLY))
    {
        LOG_F(ERROR, "Tried to set value in read-only array '%s'.", arr.szLabel);
        vm.flags |= VM_RETURN;
        return;
    }
#endif

    switch (arr.flags & GAMEARRAY_TYPE_MASK)
    {
        case 0: arr.pValues[arrayIndex] = newValue; break;
        case GAMEARRAY_INT16: ((int16_t*)arr.pValues)[arrayIndex] = newValue; break;
        case GAMEARRAY_INT8: ((int8_t*)arr.pValues)[arrayIndex] = newValue; break;
        case GAMEARRAY_UINT16: ((uint16_t*)arr.pValues)[arrayIndex] = newValue; break;
        case GAMEARRAY_UINT8: ((int8_t*)arr.pValues)[arrayIndex] = newValue; break;
        case GAMEARRAY_BITMAP:
        {
            uint32_t const mask = pow2char[arrayIndex & 7];
            uint8_t& value = ((uint8_t*)arr.pValues)[arrayIndex >> 3];
            value = (value & ~mask) | (-!!newValue & mask);
            break;
        }
    }
}

static void ResizeArray(int const arrayNum, int const newSize)
{
    auto& arr = aGameArrays[arrayNum];

    int const oldSize = arr.size;

    if ((newSize == oldSize) | (newSize < 0))
        return;
#if 0
    OSD_Printf(OSDTEXT_GREEN "CON_RESIZEARRAY: resizing array %s from %d to %d\n",
        array.szLabel, array.size, newSize);
#endif
    if (newSize == 0)
    {
        Xaligned_free(arr.pValues);
        arr.pValues = nullptr;
        arr.size = 0;
        return;
    }

    size_t const oldBytes = Gv_GetArrayAllocSizeForCount(arrayNum, oldSize);
    size_t const newBytes = Gv_GetArrayAllocSizeForCount(arrayNum, newSize);

    auto const oldArray = arr.pValues;
    auto const newArray = (intptr_t*)Xaligned_alloc(ARRAY_ALIGNMENT, newBytes);

    if (oldSize != 0)
        Bmemcpy(newArray, oldArray, min(oldBytes, newBytes));

    if (newSize > oldSize)
        Bmemset((char*)newArray + oldBytes, 0, newBytes - oldBytes);

    arr.pValues = newArray;
    arr.size = newSize;

    Xaligned_free(oldArray);
}

static int X_DoExecute(void)
{
    auto branch = [&](int const x)
    {
        if (x || VM_DECODE_INST(*(insptr = (intptr_t*)insptr[1])) == CON_ELSE)
        {
            insptr += 2;
            X_DoExecute();
        }
    };

    int j, l, s, tw = *insptr;

    if (vm.flags & (VM_RETURN | VM_KILL | VM_NOEXECUTE)) return 1;

    //      Bsprintf(g_szBuf,"Parsing: %d",*insptr);
    //      AddLog(g_szBuf);

    g_errorLineNum = VM_DECODE_LINE_NUMBER(tw);

    g_tw = tw;
    tw = VM_DECODE_INST(tw);

    switch (tw)
    {
        case CON_LEFTBRACE:
            insptr++;
            while (!X_DoExecute());
            break;

        case CON_RIGHTBRACE:
            insptr++;
            return 1;

        case CON_ELSE:
            insptr = (intptr_t*)insptr[1];
            break;

        case CON_SETVAR_GLOBAL:
            insptr++;
            aGameVars[*insptr].lValue = insptr[1];
            insptr += 2;
            break;

        case CON_SETVAR_ACTOR:
            insptr++;
            aGameVars[*insptr].pValues[vm.spriteNum & (MAXSPRITES - 1)] = insptr[1];
            insptr += 2;
            break;

        case CON_SETVAR_PLAYER:
            insptr++;
            aGameVars[*insptr].pValues[vm.playerNum & (MAXPLAYERS - 1)] = insptr[1];
            insptr += 2;
            break;

        case CON_REDEFINEQUOTE:
            insptr++;
            {
                int const strIndex = *insptr++;
                int const XstrIndex = *insptr++;

                Bstrcpy(apStrings[strIndex], apXStrings[XstrIndex]);
                break;
            }

    case CON_SETTHISPROJECTILE:
    case CON_GETTHISPROJECTILE:
        insptr++;
        {
            int const spriteNum = (*insptr++ != sysVarIDs.THISACTOR) ? Gv_GetVarX(insptr[-1]) : vm.spriteNum;
            int const labelNum = *insptr++;
            int const lVar2 = *insptr++;

            if(tw == CON_SETTHISPROJECTILE)
                VM_SetActiveProjectile(spriteNum, labelNum, Gv_GetVarX(lVar2));
            else
                Gv_SetVarX(lVar2, VM_GetActiveProjectile(spriteNum, labelNum));

            break;
        }

    case CON_IFRND:
        branch(rnd(*(++insptr)));
        break;

    case CON_IFCANSHOOTTARGET:
        if (vm.playerDist > 1024)
        {
            short temphit, sclip = 768, angdif = 16;

            if (A_CheckEnemySprite(vm.pSprite) && vm.pSprite->xrepeat > 56)
            {
                sclip = 3084;
                angdif = 48;
            }

            j = A_CheckHitSprite(vm.spriteNum,&temphit);
            if (j == (1<<30))
            {
                branch(1);
                break;
            }
            if (j > sclip)
            {
                if (temphit >= 0 && sprite[temphit].picnum == vm.pSprite->picnum)
                    j = 0;
                else
                {
                    vm.pSprite->ang += angdif;
                    j = A_CheckHitSprite(vm.spriteNum,&temphit);
                    vm.pSprite->ang -= angdif;
                    if (j > sclip)
                    {
                        if (temphit >= 0 && sprite[temphit].picnum == vm.pSprite->picnum)
                            j = 0;
                        else
                        {
                            vm.pSprite->ang -= angdif;
                            j = A_CheckHitSprite(vm.spriteNum,&temphit);
                            vm.pSprite->ang += angdif;
                            if (j > 768)
                            {
                                if (temphit >= 0 && sprite[temphit].picnum == vm.pSprite->picnum)
                                    j = 0;
                                else j = 1;
                            }
                            else j = 0;
                        }
                    }
                    else j = 0;
                }
            }
            else j =  0;
        }
        else j = 1;

        branch(j);
        break;

    case CON_IFCANSEETARGET:
        j = cansee(vm.pSprite->x,vm.pSprite->y,vm.pSprite->z-((krand()&41)<<8),vm.pSprite->sectnum,g_player[vm.playerNum].ps->pos.x,g_player[vm.playerNum].ps->pos.y,g_player[vm.playerNum].ps->pos.z/*-((krand()&41)<<8)*/,sprite[g_player[vm.playerNum].ps->i].sectnum);
        branch(j);
        if (j) actor[vm.spriteNum].timetosleep = SLEEPTIME;
        break;

    case CON_IFACTORNOTSTAYPUT:
        branch(actor[vm.spriteNum].actorstayput == -1);
        break;

    case CON_IFCANSEE:
    {
        spritetype *s = &sprite[g_player[vm.playerNum].ps->i];

        // select sprite for monster to target
        // if holoduke is on, let them target holoduke first.
        //
        if (g_player[vm.playerNum].ps->holoduke_on >= 0)
        {
            s = &sprite[g_player[vm.playerNum].ps->holoduke_on];
            j = cansee(vm.pSprite->x,vm.pSprite->y,vm.pSprite->z-(krand()&((32<<8)-1)),vm.pSprite->sectnum,
                       s->x,s->y,s->z,s->sectnum);

            if (j == 0)
            {
                // they can't see player's holoduke
                // check for player...
                s = &sprite[g_player[vm.playerNum].ps->i];
            }
        }

        // can they see player, (or player's holoduke)
        j = cansee(vm.pSprite->x,vm.pSprite->y,vm.pSprite->z-(krand()&((47<<8))),vm.pSprite->sectnum,
                   s->x,s->y,s->z-(24<<8),s->sectnum);

        if (j == 0)
        {
            // they can't see it.

            {
                // search around for target player
                // also modifies 'target' x&y if found..

                j = 1;
                if (A_FurthestVisiblePoint(vm.spriteNum,s,&actor[vm.spriteNum].lastvx,&actor[vm.spriteNum].lastvy) == -1)
                    j = 0;
            }
        }
        else
        {
            // else, they did see it.
            // save where we were looking...
            actor[vm.spriteNum].lastvx = s->x;
            actor[vm.spriteNum].lastvy = s->y;
        }

        if (j && (vm.pSprite->statnum == STAT_ACTOR || vm.pSprite->statnum == STAT_STANDABLE))
            actor[vm.spriteNum].timetosleep = SLEEPTIME;

        branch(j);
        break;
    }

    case CON_IFHITWEAPON:
        branch(A_IncurDamage(vm.spriteNum) >= 0);
        break;

    case CON_IFSQUISHED:
        branch(A_CheckSquished(vm.spriteNum, vm.playerNum));
        break;

    case CON_IFDEAD:
//        j = vm.pSprite->extra;
//        if (vm.pSprite->picnum == APLAYER)
//            j--;
        branch(vm.pSprite->extra <= 0);
        break;

    case CON_AI:
        insptr++;
        // Following changed to use pointersizes
        AC_AI_ID(vm.pData) = *insptr++;                         // Ai
        AC_ACTION_ID(vm.pData) = *(apScript + AC_AI_ID(vm.pData));  // Action

        // NOTE: "if" check added in r1155. It used to be movflags pointer though.
        if (AC_AI_ID(vm.pData))
            AC_MOVE_ID(vm.pData) = *(apScript + AC_AI_ID(vm.pData) + 1);  // move

        vm.pSprite->hitag = *(apScript + AC_AI_ID(vm.pData) + 2);  // move flags

        AC_COUNT(vm.pData) = 0;
        AC_ACTION_COUNT(vm.pData) = 0;
        AC_CURFRAME(vm.pData) = 0;

        if ((int)!A_CheckEnemySprite(vm.pSprite) | (int)(vm.pSprite->extra > 0))  // hack
            if (vm.pSprite->hitag & random_angle)
                vm.pSprite->ang = krand() & 2047;
        break;

    case CON_ACTION:
        insptr++;
        AC_ACTION_COUNT(vm.pData) = 0;
        AC_CURFRAME(vm.pData) = 0;
        AC_ACTION_ID(vm.pData) = *insptr++;
        break;

    case CON_IFPDISTL:
        insptr++;
        branch(vm.playerDist < *insptr);
        if (vm.playerDist > MAXSLEEPDIST && actor[vm.spriteNum].timetosleep == 0)
            actor[vm.spriteNum].timetosleep = SLEEPTIME;
        break;

    case CON_IFPDISTG:
        insptr++;
        branch(vm.playerDist > *insptr);
        if (vm.playerDist > MAXSLEEPDIST && actor[vm.spriteNum].timetosleep == 0)
            actor[vm.spriteNum].timetosleep = SLEEPTIME;
        break;

    case CON_ADDSTRENGTH:
        insptr++;
        vm.pSprite->extra += *insptr++;
        break;

    case CON_STRENGTH:
        insptr++;
        vm.pSprite->extra = *insptr++;
        break;

    case CON_IFGOTWEAPONCE:
        insptr++;

        // [JM] I fucking hate this code.
        if ((Gametypes[ud.coop].flags & GAMETYPE_WEAPSTAY) && ud.multimode > 1)
        {
            // HACK: Don't do this for pipebombs and tripmines, the weapon is also ammo.
            if (vm.pSprite->picnum == HBOMBAMMO || vm.pSprite->picnum == TRIPBOMBSPRITE)
            {
                // To-do: Make this a sprite or weapon flag, not a hardcoded picnum check.
                branch(0);
            }
            else if (*insptr == 0) // Param is 0, check if recently picked up.
            {
                // To-do: Make this block do fuck all (return false),
                //        and make a much more reliable, less shitty check in addweapon.

                for (j=0;j < g_player[vm.playerNum].ps->weapreccnt;j++)
                    if (g_player[vm.playerNum].ps->weaprecs[j] == vm.pSprite->picnum)
                        break; // Yes

                // If we have this weapon already, and it's not dropped, return true.
                branch(j < g_player[vm.playerNum].ps->weapreccnt && vm.pSprite->owner == vm.spriteNum);
            }
            else if (g_player[vm.playerNum].ps->weapreccnt < 16) // Param is non-zero, and we've picked up under 16 weapons?
            {
                // To-do: Just check if param is 1, and remove the line underneath this one.
                //        This weapreccnt shit is wack.

                // Add weapon to recent pickup list.
                g_player[vm.playerNum].ps->weaprecs[g_player[vm.playerNum].ps->weapreccnt++] = vm.pSprite->picnum;

                branch(vm.pSprite->owner == vm.spriteNum); // Return true if map spawned, not dropped.
            }
            else branch(0); // If we're at 16 weapons or above, just give up it seems.
        }
        else branch(0);
        break;

    case CON_GETLASTPAL:
        insptr++;
        if (vm.pSprite->picnum == APLAYER)
            vm.pSprite->pal = g_player[vm.pSprite->yvel].ps->palookup;
        else
        {
            if (vm.pSprite->pal == 1 && vm.pSprite->extra == 0) // hack for frozen
                vm.pSprite->extra++;
            vm.pSprite->pal = actor[vm.spriteNum].tempang;
        }
        actor[vm.spriteNum].tempang = 0;
        break;

    case CON_TOSSWEAPON:
        insptr++;
        P_DropWeapon(g_player[vm.pSprite->yvel].ps);
        break;

    case CON_NULLOP:
        insptr++;
        break;

    case CON_MIKESND:
        insptr++;
        if ((vm.pSprite->yvel < 0 || (unsigned)vm.pSprite->yvel > (unsigned)g_highestSoundIdx) && g_scriptSanityChecks)
        {
            OSD_Printf(CON_ERROR "Invalid sound %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),TrackerCast(vm.pSprite->yvel));
            insptr++;
            break;
        }
        if (!A_CheckSoundPlaying(vm.spriteNum,vm.pSprite->yvel))
            A_PlaySound(vm.pSprite->yvel,vm.spriteNum);
        break;

    case CON_PKICK:
        insptr++;

        if (ud.multimode > 1 && vm.pSprite->picnum == APLAYER)
        {
            if (g_player[otherp].ps->quick_kick == 0)
                g_player[otherp].ps->quick_kick = 14;
        }
        else if (vm.pSprite->picnum != APLAYER && g_player[vm.playerNum].ps->quick_kick == 0)
            g_player[vm.playerNum].ps->quick_kick = 14;
        break;

    case CON_SIZETO:
        insptr++;

        j = (*insptr++-vm.pSprite->xrepeat)<<1;
        if (PLUTOPAK)
        {
            vm.pSprite->xrepeat += ksgn(j);

            if ((vm.pSprite->picnum == APLAYER && vm.pSprite->yrepeat < 36) || *insptr < vm.pSprite->yrepeat || ((vm.pSprite->yrepeat*(tilesiz[vm.pSprite->picnum].y+8))<<2) < (actor[vm.spriteNum].floorz - actor[vm.spriteNum].ceilingz))
            {
                j = ((*insptr)-vm.pSprite->yrepeat)<<1;
                if (klabs(j)) vm.pSprite->yrepeat += ksgn(j);
            }
        }
        else
        {
            if (klabs(j) > 2)
                vm.pSprite->xrepeat += ksgn(j);

            j = ((*insptr)-vm.pSprite->yrepeat)<<1;

            if (klabs(j) > 2)
                vm.pSprite->yrepeat += ksgn(j);
        }

        insptr++;

        break;

    case CON_SIZEAT:
        insptr++;
        vm.pSprite->xrepeat = (char) *insptr++;
        vm.pSprite->yrepeat = (char) *insptr++;
        break;

    case CON_SOUNDONCE:
        insptr++;
        if ((*insptr < 0 || (unsigned)*insptr > (unsigned)g_highestSoundIdx) && g_scriptSanityChecks)
        {
            OSD_Printf(CON_ERROR "Invalid sound %d\n", g_errorLineNum, VM_GetKeywordForID(g_tw), (int)*insptr++);
            break;
        }
        if (!A_CheckSoundPlaying(vm.spriteNum,*insptr++))
            A_PlaySound(*(insptr-1),vm.spriteNum);
        break;

    case CON_IFACTORSOUND:
        insptr++;
        {
            int const spriteNum = Gv_GetVarX(*insptr++);
            int const soundNum = Gv_GetVarX(*insptr++);

            VM_ABORT_IF((unsigned)soundNum > (unsigned)g_highestSoundIdx, "invalid sound #%d", soundNum);

            insptr--;
            branch(A_CheckSoundPlaying(spriteNum, soundNum));
            break;
        }

    case CON_IFSOUND:
        insptr++;
        VM_ABORT_IF((unsigned)*insptr > (unsigned)g_highestSoundIdx, "invalid sound #%d", (int32_t)*insptr);
        branch(S_CheckSoundPlaying(*insptr));
        break;

    case CON_STOPSOUND:
        insptr++;
        if ((*insptr < 0 || (unsigned)*insptr > (unsigned)g_highestSoundIdx) && g_scriptSanityChecks)
        {
            OSD_Printf(CON_ERROR "Invalid sound %d\n", g_errorLineNum, VM_GetKeywordForID(g_tw), (int)*insptr);
            insptr++;
            break;
        }
        if (A_CheckSoundPlaying(vm.spriteNum,*insptr))
            S_StopEnvSound((short)*insptr,vm.spriteNum);
        insptr++;
        break;

    case CON_STOPACTORSOUND:
        insptr++;
        {
            int const spriteNum = Gv_GetVarX(*insptr++);
            int const soundNum = Gv_GetVarX(*insptr++);

            if (((unsigned)soundNum > (unsigned)g_highestSoundIdx) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "Invalid sound %d\n", g_errorLineNum, VM_GetKeywordForID(g_tw), soundNum);
                insptr++;
                break;
            }

            if (A_CheckSoundPlaying(spriteNum, soundNum))
                S_StopEnvSound(soundNum, spriteNum);

            break;
        }

    case CON_ACTORSOUND:
        insptr++;
        {
            int const spriteNum = (*insptr++ != sysVarIDs.THISACTOR) ? Gv_GetVarX(*(insptr - 1)) : vm.spriteNum;
            int const soundNum = Gv_GetVarX(*insptr++);

            if ((unsigned)soundNum >= MAXSOUNDS)
            {
                CON_ERRPRINTF("invalid sound %d\n", soundNum);
            }

            A_PlaySound(soundNum, spriteNum);

            break;
        }

    case CON_GLOBALSOUND:
        insptr++;
        {
            int const soundNum = Gv_GetVarX(*insptr++);
            if ((soundNum < 0 || (unsigned)soundNum > (unsigned)g_highestSoundIdx) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "Invalid sound %d\n", g_errorLineNum, VM_GetKeywordForID(g_tw), soundNum);
                break;
            }

            if ((Gametypes[ud.coop].flags & GAMETYPE_COOPSOUND) && !A_CheckEnemySprite(vm.pSprite))
                A_PlaySound(soundNum, g_player[vm.playerNum].ps->i);
            else if (vm.playerNum == screenpeek || (Gametypes[ud.coop].flags & GAMETYPE_COOPSOUND))
                A_PlaySound(soundNum, g_player[screenpeek].ps->i);

            break;
        }

    case CON_SOUND:
        insptr++;
        {
            int const soundNum = Gv_GetVarX(*insptr++);
            if ((soundNum < 0 || (unsigned)soundNum >(unsigned)g_highestSoundIdx) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "Invalid sound %d\n", g_errorLineNum, VM_GetKeywordForID(g_tw), soundNum);
                break;
            }

            A_PlaySound(soundNum, vm.spriteNum);
            break;
        }

    case CON_SCREENSOUND:
        insptr++;
        {
            int const soundNum = Gv_GetVarX(*insptr++);
            if ((soundNum < 0 || (unsigned)soundNum >(unsigned)g_highestSoundIdx) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "Invalid sound %d\n", g_errorLineNum, VM_GetKeywordForID(g_tw), soundNum);
                break;
            }

            S_PlaySound(soundNum);
            break;
        }

    case CON_TIP:
        insptr++;
        g_player[vm.playerNum].ps->tipincs = 26;
        break;

    case CON_FALL:
        insptr++;
        VM_Fall(vm.spriteNum, vm.pSprite);
        break;

    case CON_ENDA:
    case CON_BREAK:
    case CON_ENDS:
        return 1;

    case CON_ADDAMMO:
        insptr++;
        {
            int32_t weap=*insptr++, amount=*insptr++;
            DukePlayer_t *ps = g_player[vm.playerNum].ps;

            if ((unsigned)weap >= MAX_WEAPONS)
            {
                CON_ERRPRINTF("Invalid weapon ID %d\n", weap);
                break;
            }

            if (ps->ammo_amount[weap] >= ps->max_ammo_amount[weap])
            {
                vm.flags |= VM_NOEXECUTE;
                break;
            }

            addweapon_addammo_common(ps, weap, amount);
        }
        break;

    case CON_RETURN:
        vm.flags |= VM_RETURN;
        break;

    case CON_MONEY:
        insptr++;
        A_SpawnMultiple(vm.spriteNum, MONEY, *insptr++);
        break;

    case CON_MAIL:
        insptr++;
        A_SpawnMultiple(vm.spriteNum, MAIL, *insptr++);
        break;

    case CON_SLEEPTIME:
        insptr++;
        actor[vm.spriteNum].timetosleep = (short)*insptr++;
        break;

    case CON_PAPER:
        insptr++;
        A_SpawnMultiple(vm.spriteNum, PAPER, *insptr++);
        break;

    case CON_ADDKILLS:
        insptr++;
        G_AddMonsterKills(vm.spriteNum, *insptr++);
        actor[vm.spriteNum].actorstayput = -1;
        break;

    case CON_LOTSOFGLASS:
        insptr++;
        A_SpawnGlass(vm.spriteNum,*insptr++);
        break;

    case CON_KILLIT:
        insptr++;
        vm.flags |= VM_KILL;
        break;

    case CON_ADDWEAPON:
        insptr++;
        {
            int32_t weap=*insptr++, amount=*insptr++;
            VM_AddWeapon(weap, amount, g_player[vm.playerNum].ps);
        }
        break;

    case CON_DEBUG:
        insptr++;
        VLOG_F(LOG_VM, "%d", (int)*insptr++);
        break;

    case CON_ENDOFGAME:
        insptr++;
        g_player[vm.playerNum].ps->timebeforeexit = *insptr++;
        g_player[vm.playerNum].ps->customexitsound = -1;
        ud.eog = 1;
        break;

    case CON_ADDPHEALTH:
        insptr++;

        if (g_player[vm.playerNum].ps->newowner >= 0)
        {
            G_ClearCameraView(g_player[vm.playerNum].ps);
        }

        j = sprite[g_player[vm.playerNum].ps->i].extra;

        if (vm.pSprite->picnum != ATOMICHEALTH)
        {
            if (j > g_player[vm.playerNum].ps->max_player_health && *insptr > 0)
            {
                insptr++;
                break;
            }
            else
            {
                if (j > 0)
                    j += *insptr;
                if (j > g_player[vm.playerNum].ps->max_player_health && *insptr > 0)
                    j = g_player[vm.playerNum].ps->max_player_health;
            }
        }
        else
        {
            if (j > 0)
                j += *insptr;
            if (j > (g_player[vm.playerNum].ps->max_player_health<<1))
                j = (g_player[vm.playerNum].ps->max_player_health<<1);
        }

        if (j < 0) j = 0;

        if (ud.god == 0)
        {
            if (*insptr > 0)
            {
                if ((j - *insptr) < (g_player[vm.playerNum].ps->max_player_health>>2) &&
                        j >= (g_player[vm.playerNum].ps->max_player_health>>2))
                    A_PlaySound(DUKE_GOTHEALTHATLOW,g_player[vm.playerNum].ps->i);

                g_player[vm.playerNum].ps->last_extra = j;
            }

            sprite[g_player[vm.playerNum].ps->i].extra = j;
        }

        insptr++;
        break;

    case CON_STATE:
    {
        auto tempscrptr = &insptr[2];
        insptr = (intptr_t*)insptr[1];
        while (!X_DoExecute());
        insptr = tempscrptr;
    }
    break;

    case CON_MOVE:
        insptr++;
        AC_COUNT(vm.pData) = 0;
        AC_MOVE_ID(vm.pData) = *insptr++;
        vm.pSprite->hitag = *insptr++;

        if (!A_CheckEnemySprite(vm.pSprite) || (vm.pSprite->extra > 0)) // hack
            if (vm.pSprite->hitag & random_angle)
                vm.pSprite->ang = krand() & 2047;
        break;

    case CON_ADDWEAPONVAR:
        insptr++;
        {
            int32_t weap=Gv_GetVarX(*insptr++), amount=Gv_GetVarX(*insptr++);
            VM_AddWeapon(weap, amount, g_player[vm.playerNum].ps);
        }
        break;

    case CON_ACTIVATEBYSECTOR:
    case CON_OPERATESECTORS:
    case CON_OPERATEACTIVATORS:
    case CON_SETASPECT:
    case CON_SSP:
        insptr++;
        {
            int var1 = Gv_GetVarX(*insptr++), var2;
            if (tw == CON_OPERATEACTIVATORS && *insptr == sysVarIDs.THISACTOR)
            {
                var2 = vm.playerNum;
                insptr++;
            }
            else var2 = Gv_GetVarX(*insptr++);

            switch (tw)
            {
            case CON_ACTIVATEBYSECTOR:
                if ((var1<0 || var1>=numsectors) && g_scriptSanityChecks) {OSD_Printf(CON_ERROR "Invalid sector %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),var1);break;}
                activatebysector(var1, var2);
                break;
            case CON_OPERATESECTORS:
                if ((var1<0 || var1>=numsectors) && g_scriptSanityChecks) {OSD_Printf(CON_ERROR "Invalid sector %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),var1);break;}
                G_OperateSectors(var1, var2);
                break;
            case CON_OPERATEACTIVATORS:
                if ((var2<0 || var2>=ud.multimode) && g_scriptSanityChecks) {OSD_Printf(CON_ERROR "Invalid player %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),var2);break;}
                G_OperateActivators(var1, var2);
                break;
            case CON_SETASPECT:
                renderSetAspect(var1, var2);
                break;
            case CON_SSP:
                if ((var1<0 || var1>=MAXSPRITES) && g_scriptSanityChecks) { OSD_Printf(CON_ERROR "Invalid sprite %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),var1);break;}
                A_SetSprite(var1, var2);
                break;
            }
            break;
        }

    case CON_CANSEESPR:
        insptr++;
        {
            int lVar1 = Gv_GetVarX(*insptr++), lVar2 = Gv_GetVarX(*insptr++), res;

            if ((lVar1<0 || lVar1>=MAXSPRITES || lVar2<0 || lVar2>=MAXSPRITES) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "Invalid sprite %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),lVar1<0||lVar1>=MAXSPRITES?lVar1:lVar2);
                res=0;
            }
            else res=cansee(sprite[lVar1].x,sprite[lVar1].y,sprite[lVar1].z,sprite[lVar1].sectnum,
                                sprite[lVar2].x,sprite[lVar2].y,sprite[lVar2].z,sprite[lVar2].sectnum);

            Gv_SetVarX(*insptr++, res);
            break;
        }

    case CON_OPERATERESPAWNS:
    case CON_OPERATEMASTERSWITCHES:
    case CON_CHECKACTIVATORMOTION:
        insptr++;
        {
            int var1 = Gv_GetVarX(*insptr++);

            switch (tw)
            {
            case CON_OPERATERESPAWNS:
                G_OperateRespawns(var1);
                break;
            case CON_OPERATEMASTERSWITCHES:
                G_OperateMasterSwitches(var1);
                break;
            case CON_CHECKACTIVATORMOTION:
                Gv_SetVarX(sysVarIDs.RETURN, check_activator_motion(var1));
                break;
            }
            break;
        }

    case CON_INSERTSPRITEQ:
        insptr++;
        A_AddToDeleteQueue(vm.spriteNum);
        break;

    case CON_QSTRLEN:
        insptr++;
        {
            int i=*insptr++;
            j=Gv_GetVarX(*insptr++);
            if ((apStrings[j] == NULL) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "null quote %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),j);
                Gv_SetVarX(i,-1);
                break;
            }
            Gv_SetVarX(i,Bstrlen(apStrings[j]));
            break;
        }

    case CON_QSTRDIM:
        insptr++;
        {
            vec2_t dim = { 0, 0, };

            int32_t w = *insptr++;
            int32_t h = *insptr++;

            int32_t tilenum = Gv_GetVarX(*insptr++);
            int32_t x = Gv_GetVarX(*insptr++), y = Gv_GetVarX(*insptr++), z = Gv_GetVarX(*insptr++);
            int32_t blockangle = Gv_GetVarX(*insptr++);
            int32_t q = Gv_GetVarX(*insptr++);
            int32_t orientation = Gv_GetVarX(*insptr++);
            int32_t xspace = Gv_GetVarX(*insptr++), yline = Gv_GetVarX(*insptr++);
            int32_t xbetween = Gv_GetVarX(*insptr++), ybetween = Gv_GetVarX(*insptr++);
            int32_t f = Gv_GetVarX(*insptr++);
            int32_t x1 = Gv_GetVarX(*insptr++), y1 = Gv_GetVarX(*insptr++);
            int32_t x2 = Gv_GetVarX(*insptr++), y2 = Gv_GetVarX(*insptr++);

            orientation &= (ROTATESPRITE_MAX - 1);

            if (tilenum < 0 || tilenum + 255 >= MAXTILES)
                CON_ERRPRINTF("invalid base tilenum %d\n", tilenum);
            else if ((unsigned)q >= MAXQUOTES)
                CON_ERRPRINTF("invalid quote ID %d\n", q);
            else if ((apStrings[q] == NULL))
                CON_ERRPRINTF("null quote %d\n", q);
            else
                dim = G_ScreenTextSize(tilenum, x, y, z, blockangle, apStrings[q], orientation, xspace, yline, xbetween, ybetween, f, x1, y1, x2, y2);

            Gv_SetVarX(w, dim.x);
            Gv_SetVarX(h, dim.y);
            break;
        }

    case CON_HEADSPRITESTAT:
        insptr++;
        {
            int i=*insptr++;
            j=Gv_GetVarX(*insptr++);
            if ((j < 0 || j > MAXSTATUS) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "invalid status list %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),j);
                break;
            }
            Gv_SetVarX(i,headspritestat[j]);
            break;
        }

    case CON_PREVSPRITESTAT:
        insptr++;
        {
            int i=*insptr++;
            j=Gv_GetVarX(*insptr++);
            if ((j < 0 || j >= MAXSPRITES) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "invalid sprite ID %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),j);
                break;
            }
            Gv_SetVarX(i,prevspritestat[j]);
            break;
        }

    case CON_NEXTSPRITESTAT:
        insptr++;
        {
            int i=*insptr++;
            j=Gv_GetVarX(*insptr++);
            if ((j < 0 || j >= MAXSPRITES) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "invalid sprite ID %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),j);
                break;
            }
            Gv_SetVarX(i,nextspritestat[j]);
            break;
        }

    case CON_HEADSPRITESECT:
        insptr++;
        {
            int i=*insptr++;
            j=Gv_GetVarX(*insptr++);
            if ((j < 0 || j > numsectors) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "invalid sector %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),j);
                break;
            }
            Gv_SetVarX(i,headspritesect[j]);
            break;
        }

    case CON_PREVSPRITESECT:
        insptr++;
        {
            int i=*insptr++;
            j=Gv_GetVarX(*insptr++);
            if ((j < 0 || j >= MAXSPRITES) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "invalid sprite ID %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),j);
                break;
            }
            Gv_SetVarX(i,prevspritesect[j]);
            break;
        }

    case CON_NEXTSPRITESECT:
        insptr++;
        {
            int i=*insptr++;
            j=Gv_GetVarX(*insptr++);
            if ((j < 0 || j >= MAXSPRITES) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "invalid sprite ID %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),j);
                break;
            }
            Gv_SetVarX(i,nextspritesect[j]);
            break;
        }

    case CON_GETKEYNAME:
        insptr++;
        {
            int i = Gv_GetVarX(*insptr++),
                    f=Gv_GetVarX(*insptr++);
            j=Gv_GetVarX(*insptr++);
            if ((i<0 || i>=MAXQUOTES) && g_scriptSanityChecks)
                OSD_Printf(CON_ERROR "invalid quote ID %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),i);
            else if ((apStrings[i] == NULL) && g_scriptSanityChecks)
                OSD_Printf(CON_ERROR "null quote %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),i);
            else if ((f<0 || f>=NUMGAMEFUNCTIONS) && g_scriptSanityChecks)
                OSD_Printf(CON_ERROR "invalid function %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),f);
            else
            {
                if (j<2)
                    Bstrcpy(tempbuf,KB_ScanCodeToString(ud.config.KeyboardKeys[f][j]));
                else
                {
                    Bstrcpy(tempbuf,KB_ScanCodeToString(ud.config.KeyboardKeys[f][0]));
                    if (!*tempbuf)
                        Bstrcpy(tempbuf,KB_ScanCodeToString(ud.config.KeyboardKeys[f][1]));
                }
            }

            if (*tempbuf)
                Bstrcpy(apStrings[i],tempbuf);
            break;
        }
    case CON_QSUBSTR:
        insptr++;
        {
            char *s1,*s2;
            int q1,q2,st,ln;

            q1 = Gv_GetVarX(*insptr++),
                 q2 = Gv_GetVarX(*insptr++);
            st = Gv_GetVarX(*insptr++);
            ln = Gv_GetVarX(*insptr++);

            if ((q1<0 || q1>=MAXQUOTES) && g_scriptSanityChecks)       OSD_Printf(CON_ERROR "invalid quote ID %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),q1);
            else if ((apStrings[q1] == NULL) && g_scriptSanityChecks) OSD_Printf(CON_ERROR "null quote %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),q1);
            else if ((q2<0 || q2>=MAXQUOTES) && g_scriptSanityChecks)  OSD_Printf(CON_ERROR "invalid quote ID %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),q2);
            else if ((apStrings[q2] == NULL) && g_scriptSanityChecks) OSD_Printf(CON_ERROR "null quote %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),q2);
            else
            {
                s1=apStrings[q1];
                s2=apStrings[q2];
                while (*s2&&st--)s2++;
                while ((*s1=*s2)&&ln--) {s1++;s2++;}
                *s1=0;
            }
            break;
        }

    case CON_GETPNAME:
    case CON_QSTRCAT:
    case CON_QSTRCPY:
    case CON_QGETSYSSTR:
    case CON_CHANGESPRITESECT:
        insptr++;
        {
            int i = Gv_GetVarX(*insptr++), j;
            if (tw == CON_GETPNAME && *insptr == sysVarIDs.THISACTOR)
            {
                j = vm.playerNum;
                insptr++;
            }
            else if (tw == CON_GETTEAMNAME && *insptr == sysVarIDs.THISACTOR)
            {
                j = g_player[vm.playerNum].ps->team;
                insptr++;
            }
            else j = Gv_GetVarX(*insptr++);

            switch (tw)
            {
            case CON_GETPNAME:
                if ((apStrings[i] == NULL) && g_scriptSanityChecks)
                {
                    OSD_Printf(CON_ERROR "null quote %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),i);
                    break;
                }
                if (g_player[j].user_name[0])
                    Bstrcpy(apStrings[i],g_player[j].user_name);
                else Bsprintf(apStrings[i],"%d",j);
                break;
            case CON_GETTEAMNAME:
                if ((apStrings[i] == NULL) && g_scriptSanityChecks)
                {
                    OSD_Printf(CON_ERROR "null quote %d\n", g_errorLineNum, VM_GetKeywordForID(g_tw), i);
                    break;
                }
                if (teams[j].name[0])
                    Bstrcpy(apStrings[i], teams[j].name);
                else Bsprintf(apStrings[i], "Team %d", j);
                break;
            case CON_QGETSYSSTR:
                if ((apStrings[i] == NULL) && g_scriptSanityChecks)
                {
                    OSD_Printf(CON_ERROR "null quote %d %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),i,j);
                    break;
                }
                switch (j)
                {
                case STR_MAPNAME:
                    Bstrcpy(apStrings[i],g_mapInfo[ud.volume_number*MAXLEVELS + ud.level_number].name);
                    break;
                case STR_MAPFILENAME:
                    Bstrcpy(apStrings[i],g_mapInfo[ud.volume_number*MAXLEVELS + ud.level_number].filename);
                    break;
                case STR_PLAYERNAME:
                    Bstrcpy(apStrings[i],g_player[vm.playerNum].user_name);
                    break;
                case STR_VERSION:
                    Bsprintf(tempbuf,HEAD2 " %s",s_buildRev);
                    Bstrcpy(apStrings[i],tempbuf);
                    break;
                case STR_GAMETYPE:
                    Bstrcpy(apStrings[i],Gametypes[ud.coop].name);
                    break;
                default:
                    OSD_Printf(CON_ERROR "unknown str ID %d %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),i,j);
                }
                break;
            case CON_QSTRCAT:
                if ((apStrings[i] == NULL || apStrings[j] == NULL) && g_scriptSanityChecks)
                {
                    OSD_Printf(CON_ERROR "null quote %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),apStrings[i] ? j : i);
                    break;
                }
                Bstrncat(apStrings[i],apStrings[j],(MAXQUOTELEN-1)-Bstrlen(apStrings[i]));
                break;
            case CON_QSTRCPY:
                if ((apStrings[i] == NULL || apStrings[j] == NULL) && g_scriptSanityChecks)
                {
                    OSD_Printf(CON_ERROR "null quote %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),apStrings[i] ? j : i);
                    break;
                }
                Bstrcpy(apStrings[i],apStrings[j]);
                break;
            case CON_CHANGESPRITESECT:
                if ((i<0 || i>=MAXSPRITES) && g_scriptSanityChecks) {OSD_Printf(CON_ERROR "Invalid sprite %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),i);break;}
                if ((j<0 || j>=numsectors) && g_scriptSanityChecks) {OSD_Printf(CON_ERROR "Invalid sector %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),j);break;}
                changespritesect(i,j);
                break;
            }
            break;
        }

    case CON_CHANGESPRITESTAT:
        insptr++;
        {
            int i = Gv_GetVarX(*insptr++);
            j = Gv_GetVarX(*insptr++);

            if ((i < 0 || i >= MAXSPRITES) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "Invalid sprite: %d\n", g_errorLineNum, VM_GetKeywordForID(g_tw), i);
                break;
            }
            if ((j < 0 || j >= MAXSTATUS) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "Invalid statnum: %d\n", g_errorLineNum, VM_GetKeywordForID(g_tw), j);
                break;
            }
            if (sprite[i].statnum == j) break;

            /* initialize actor data when changing to an actor statnum because there's usually
               garbage left over from being handled as movflags hard coded object */

            if (sprite[i].statnum > STAT_ZOMBIEACTOR && (j == STAT_ACTOR || j == STAT_ZOMBIEACTOR))
            {
                actor[i].lastvx = 0;
                actor[i].lastvy = 0;
                actor[i].timetosleep = 0;
                actor[i].cgg = 0;
                actor[i].movflag = 0;
                actor[i].tempang = 0;
                actor[i].dispicnum = 0;
                T1(i) = T2(i) = T3(i) = T4(i) = T5(i) = T6(i) = T7(i) = T8(i) = T9(i) = 0;
                actor[i].flags = 0;
                sprite[i].hitag = 0;

                if (G_TileHasActor(sprite[i].picnum))
                {
                    auto actorptr = g_tile[sprite[i].picnum].execPtr;
                    // offsets
                    AC_ACTION_ID(actor[i].t_data) = actorptr[1];
                    AC_MOVE_ID(actor[i].t_data) = actorptr[2];
                    AC_MOVFLAGS(&sprite[i], pActor) = actorptr[3];  // ai bits (movflags)
                }
            }
            changespritestat(i, j);
            break;
        }

    case CON_STARTLEVEL:
        insptr++; // skip command
        {
            // from 'level' cheat in game.c (about line 6250)
            int volnume=Gv_GetVarX(*insptr++), levnume=Gv_GetVarX(*insptr++);

            if ((volnume > MAXVOLUMES-1 || volnume < 0) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "invalid volume (%d)\n",g_errorLineNum,VM_GetKeywordForID(g_tw),volnume);
                break;
            }

            if ((levnume > MAXLEVELS-1 || levnume < 0) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "invalid level (%d)\n",g_errorLineNum,VM_GetKeywordForID(g_tw),levnume);
                break;
            }

            ud.m_volume_number = volnume;
            ud.m_level_number = levnume;
            ud.display_bonus_screen = 0;
#if 0
            G_NewGame(NEWGAME_NORMAL); // [JM] Doing it this way is busted in the Duke64 TC for some dumb fucking reason.
#else
            ud.volume_number = ud.m_volume_number;
            ud.level_number = ud.m_level_number;
            ud.gm = MODE_EOL;
#endif

            break;
        }

    case CON_MYOSX:
    case CON_MYOSPALX:
    case CON_MYOS:
    case CON_MYOSPAL:
        insptr++;
        {
            int x=Gv_GetVarX(*insptr++), y=Gv_GetVarX(*insptr++), tilenum=Gv_GetVarX(*insptr++);
            int shade=Gv_GetVarX(*insptr++), orientation=Gv_GetVarX(*insptr++);

            switch (tw)
            {
            case CON_MYOS:
                VM_DrawTile(x,y,tilenum,shade,orientation);
                break;
            case CON_MYOSPAL:
            {
                int pal=Gv_GetVarX(*insptr++);
                VM_DrawTilePal(x,y,tilenum,shade,orientation,pal);
                break;
            }
            case CON_MYOSX:
                VM_DrawTileSmall(x,y,tilenum,shade,orientation);
                break;
            case CON_MYOSPALX:
            {
                int pal=Gv_GetVarX(*insptr++);
                VM_DrawTilePalSmall(x,y,tilenum,shade,orientation,pal);
                break;
            }
            }
            break;
        }

    case CON_SWITCH:
        insptr++; // p-code
        {
            // command format:
            // variable ID to check
            // script offset to 'end'
            // count of case statements
            // script offset to default case (null if none)
            // For each case: value, ptr to code
            //AddLog("Processing Switch...");
            int lValue=Gv_GetVarX(*insptr++), lEnd=*insptr++, lCases=*insptr++;
            intptr_t *lpDefault = insptr++, *lpCases = insptr; //, *lTempInsPtr;
            int bMatched=0, lCheckCase;
            int left,right;
            insptr+=lCases*2;
            // lTempInsPtr=insptr; // UNUSED
            //Bsprintf(g_szBuf,"lEnd= %d *lpDefault=%d",lEnd,*lpDefault);
            //AddLog(g_szBuf);

            //Bsprintf(g_szBuf,"Checking %d cases for %d",lCases, lValue);
            //AddLog(g_szBuf);
            left=0;right=lCases-1;
            while (!bMatched)
            {
                //Bsprintf(g_szBuf,"Checking #%d Value= %d",lCheckCase, lpCases[lCheckCase*2]);
                //AddLog(g_szBuf);
                lCheckCase=(left+right)/2;
//                LOG_F("(%2d..%2d..%2d) [%2d..%2d..%2d]==%2d\n",left,lCheckCase,right,lpCases[left*2],lpCases[lCheckCase*2],lpCases[right*2],lValue);
                if (lpCases[lCheckCase*2] > lValue)
                    right=lCheckCase-1;
                else if (lpCases[lCheckCase*2] < lValue)
                    left =lCheckCase+1;
                else if (lpCases[lCheckCase*2] == lValue)
                {
                    //AddLog("Found Case Match");
                    //Bsprintf(g_szBuf,"insptr=%d. lCheckCase=%d, offset=%d, &apScript[0]=%d",
                    //            (int)insptr,(int)lCheckCase,lpCases[lCheckCase*2+1],(int)&apScript[0]);
                    //AddLog(g_szBuf);
                    // fake a 2-d Array
                    insptr=(intptr_t*)(lpCases[lCheckCase*2+1] + &apScript[0]);
                    //Bsprintf(g_szBuf,"insptr=%d. ",     (int)insptr);
                    //AddLog(g_szBuf);
                    while (!X_DoExecute());
                    //AddLog("Done Executing Case");
                    bMatched=1;
                }
                if (right-left < 0)
                    break;
            }
            if (!bMatched)
            {
                if (*lpDefault)
                {
                    //AddLog("No Matching Case: Using Default");
                    insptr=(intptr_t*)(*lpDefault + &apScript[0]);
                    while (!X_DoExecute());
                }
                else
                {
                    //AddLog("No Matching Case: No Default to use");
                }
            }
            insptr=(intptr_t *)(lEnd + (intptr_t)&apScript[0]);
            //Bsprintf(g_szBuf,"insptr=%d. ",     (int)insptr);
            //AddLog(g_szBuf);
            //AddLog("Done Processing Switch");

            break;
        }

    case CON_ENDSWITCH:
        insptr++;
        fallthrough__;
    case CON_ENDEVENT:
        return 1;

    case CON_FOR:  // special-purpose iteration
        insptr++;
        {
            int const returnVar = *insptr++;
            int const iterType = *insptr++;
            int const nIndex = iterType <= ITER_DRAWNSPRITES ? 0 : Gv_GetVarX(*insptr++);

            auto const pEnd = insptr + *insptr;
            auto const pNext = ++insptr;

            auto execute = [&](int const index) {
                Gv_SetVarX(returnVar, index);
                insptr = pNext;
                X_DoExecute();
                return !!(vm.flags & VM_RETURN);
            };

            auto spriteexecute = [&](int const index) {
                {
                    //MICROPROFILE_SCOPE_TOKEN(g_statnumTokens[sprite[index].statnum]);
                    execute(index);
                }
                return !!(vm.flags & VM_RETURN);
            };

            switch (iterType)
            {
                case ITER_ALLSPRITES:
                    for (native_t jj = 0; jj < MAXSPRITES; ++jj)
                    {
                        if (sprite[jj].statnum == MAXSTATUS)
                            continue;

                        if (spriteexecute(jj))
                            return 1;
                    }
                    break;

                case ITER_ALLSPRITESBYSTAT:
                    for (native_t statNum = 0; statNum < MAXSTATUS; ++statNum)
                    {
                        for (native_t kk, SPRITES_OF_STAT_SAFE(statNum, jj, kk))
                        {
                            if (spriteexecute(jj))
                                return 1;
                        }
                    }
                    break;

                case ITER_ALLSPRITESBYSECT:
                    for (native_t sectNum = 0; sectNum < numsectors; ++sectNum)
                    {
                        for (native_t kk, SPRITES_OF_SECT_SAFE(sectNum, jj, kk))
                        {
                            if (spriteexecute(jj))
                                return 1;
                        }
                    }
                    break;

                case ITER_ALLSECTORS:
                    for (native_t jj = 0; jj < numsectors; ++jj)
                        if (execute(jj))
                            return 1;
                    break;

                case ITER_ALLWALLS:
                    for (native_t jj = 0; jj < numwalls; ++jj)
                        if (execute(jj))
                            return 1;
                    break;

                case ITER_ACTIVELIGHTS:
#ifdef POLYMER
                    for (native_t jj = 0; jj < PR_MAXLIGHTS; ++jj)
                    {
                        if (!prlights[jj].flags.active)
                            continue;

                        if (execute(jj))
                            return 1;
                    }
#endif
                    break;

                case ITER_DRAWNSPRITES:
                    for (native_t jj = 0; jj < spritesortcnt; jj++)
                        if (execute(jj))
                            return 1;
                    break;

                case ITER_SPRITESOFSECTOR:
                    if ((unsigned)nIndex >= MAXSECTORS)
                        goto badindex;

                    for (native_t kk, SPRITES_OF_SECT_SAFE(nIndex, jj, kk))
                    {
                        if (spriteexecute(jj))
                            return 1;
                    }
                    break;

                case ITER_SPRITESOFSTATUS:
                    if ((unsigned)nIndex >= MAXSTATUS)
                        goto badindex;

                    for (native_t kk, SPRITES_OF_STAT_SAFE(nIndex, jj, kk))
                    {
                        if (spriteexecute(jj))
                            return 1;
                    }
                    break;

                case ITER_WALLSOFSECTOR:
                    if ((unsigned)nIndex >= MAXSECTORS)
                        goto badindex;

                    for (native_t jj = sector[nIndex].wallptr, endwall = jj + sector[nIndex].wallnum - 1; jj <= endwall; jj++)
                        if (execute(jj))
                            return 1;
                    break;

                case ITER_LOOPOFWALL:
                    if ((unsigned)nIndex >= (unsigned)numwalls)
                        goto badindex;
                    {
                        int jj = nIndex;
                        do
                        {
                            if (execute(jj))
                                return 1;

                            jj = wall[jj].point2;
                        } while (jj != nIndex);
                    }
                    break;

                case ITER_RANGE:
                    for (native_t jj = 0; jj < nIndex; jj++)
                        if (execute(jj))
                            return 1;
                    break;
                badindex:
                    VLOG_F(LOG_VM, "%s:%d: for %s: index %d out of range!", VM_FILENAME(insptr), VM_DECODE_LINE_NUMBER(g_tw), iter_tokens[iterType].token, nIndex);
                    vm.flags |= VM_RETURN;
                    break;
            }
            insptr = pEnd;
        }
        break;

    case CON_DISPLAYRAND:
        insptr++;
        Gv_SetVarX(*insptr++, rand());
        break;

    case CON_DRAGPOINT:
        insptr++;
        {
            int wallnum = Gv_GetVarX(*insptr++), newx = Gv_GetVarX(*insptr++), newy = Gv_GetVarX(*insptr++);

            if ((wallnum<0 || wallnum>=numwalls) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "Invalid wall %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),wallnum);
                break;
            }
            dragpoint(wallnum,newx,newy,0);
            break;
        }

    case CON_DIST:
    case CON_LDIST:
        insptr++;
        {
            int distvar = *insptr++, xvar = Gv_GetVarX(*insptr++), yvar = Gv_GetVarX(*insptr++), distx=0;

            if ((xvar < 0 || yvar < 0 || xvar >= MAXSPRITES || yvar >= MAXSPRITES) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "invalid sprite\n",g_errorLineNum,VM_GetKeywordForID(g_tw));
                break;
            }
            if (tw == CON_DIST) distx = dist(&sprite[xvar],&sprite[yvar]);
            else distx = ldist(&sprite[xvar],&sprite[yvar]);

            Gv_SetVarX(distvar, distx);
            break;
        }

    case CON_GETINCANGLE:
    case CON_GETANGLE:
        insptr++;
        {
            int angvar = *insptr++;
            int xvar = Gv_GetVarX(*insptr++);
            int yvar = Gv_GetVarX(*insptr++);

            if (tw==CON_GETANGLE)
            {
                Gv_SetVarX(angvar, getangle(xvar,yvar));
                break;
            }
            Gv_SetVarX(angvar, G_GetAngleDelta(xvar,yvar));
            break;
        }

    case CON_MULSCALE:
        insptr++;
        {
            int var1 = *insptr++, var2 = Gv_GetVarX(*insptr++);
            int var3 = Gv_GetVarX(*insptr++), var4 = Gv_GetVarX(*insptr++);

            Gv_SetVarX(var1, mulscale(var2, var3, var4));
            break;
        }

    case CON_INITTIMER:
        insptr++;
        G_InitTimer(Gv_GetVarX(*insptr++));
        break;

    case CON_TIME:
        insptr += 2;
        break;

    case CON_ESPAWNVAR:
    case CON_EQSPAWNVAR:
    case CON_QSPAWNVAR:
        insptr++;
        {
            int lIn=Gv_GetVarX(*insptr++);
            if ((vm.pSprite->sectnum < 0 || vm.pSprite->sectnum >= numsectors) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "Invalid sector %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),TrackerCast(vm.pSprite->sectnum));
                break;
            }
            j = A_Spawn(vm.spriteNum, lIn);
            switch (tw)
            {
            case CON_EQSPAWNVAR:
                if (j != -1)
                    A_AddToDeleteQueue(j);
                fallthrough__;
            case CON_ESPAWNVAR:
                Gv_SetVarX(sysVarIDs.RETURN, j);
                break;
            case CON_QSPAWNVAR:
                if (j != -1)
                    A_AddToDeleteQueue(j);
                break;
            }
            break;
        }

    case CON_ESPAWN:
    case CON_EQSPAWN:
    case CON_QSPAWN:
        insptr++;

        if ((vm.pSprite->sectnum < 0 || vm.pSprite->sectnum >= numsectors) && g_scriptSanityChecks)
        {
            OSD_Printf(CON_ERROR "Invalid sector %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),TrackerCast(vm.pSprite->sectnum));
            insptr++;
            break;
        }

        j = A_Spawn(vm.spriteNum,*insptr++);

        switch (tw)
        {
        case CON_EQSPAWN:
            if (j != -1)
                A_AddToDeleteQueue(j);
            fallthrough__;
        case CON_ESPAWN:
            Gv_SetVarX(sysVarIDs.RETURN, j);
            break;
        case CON_QSPAWN:
            if (j != -1)
                A_AddToDeleteQueue(j);
            break;
        }
        break;

    case CON_SHOOT:
    case CON_ESHOOT:
    insptr++;
    {
        if ((vm.pSprite->sectnum < 0 || vm.pSprite->sectnum >= numsectors) && g_scriptSanityChecks)
        {
            OSD_Printf(CON_ERROR "Invalid sector %d\n", g_errorLineNum, VM_GetKeywordForID(g_tw), TrackerCast(vm.pSprite->sectnum));
            break;
        }

        int j = Gv_GetVarX(*insptr++);
        j = A_Shoot(vm.spriteNum, j);

        if (tw == CON_ESHOOT)
            Gv_SetVarX(sysVarIDs.RETURN, j);

        break;
    }

    case CON_EZSHOOT:
    case CON_ZSHOOT:
    insptr++;
    {
        if ((vm.pSprite->sectnum < 0 || vm.pSprite->sectnum >= numsectors) && g_scriptSanityChecks)
        {
            OSD_Printf(CON_ERROR "Invalid sector %d\n", g_errorLineNum, VM_GetKeywordForID(g_tw), TrackerCast(vm.pSprite->sectnum));
            break;
        }

        int const zvel = (int16_t)Gv_GetVarX(*insptr++);
        int j = A_Shoot(vm.spriteNum, Gv_GetVarX(*insptr++), zvel);

        if (tw == CON_EZSHOOT)
            Gv_SetVarX(sysVarIDs.RETURN, j);

        break;
    }

    case CON_CMENU:
        insptr++;
        j=Gv_GetVarX(*insptr++);
        ChangeToMenu(j);
        break;

    case CON_STOPSOUNDVAR:
    case CON_SOUNDONCEVAR:
        insptr++;
        j=Gv_GetVarX(*insptr++);

        if ((j < 0 || (unsigned)j > (unsigned)g_highestSoundIdx) && g_scriptSanityChecks)
        {
            OSD_Printf(CON_ERROR "Invalid sound %d\n", g_errorLineNum, VM_GetKeywordForID(g_tw), j);
            break;
        }

        switch (tw)
        {
        case CON_SOUNDONCEVAR:
            if (!A_CheckSoundPlaying(vm.spriteNum,j))
                A_PlaySound((short)j,vm.spriteNum);
            break;
        case CON_STOPSOUNDVAR:
            if (A_CheckSoundPlaying(vm.spriteNum,j))
                S_StopEnvSound((short)j,vm.spriteNum);
            break;
        }
        break;

    case CON_GUNIQHUDID:
        insptr++;
        {
            j=Gv_GetVarX(*insptr++);
            if (j >= 0 && j < MAXUNIQHUDID-1)
                guniqhudid = j;
            else
                OSD_Printf(CON_ERROR "Invalid ID %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),j);
            break;
        }

    case CON_SAVEGAMEVAR:
    case CON_READGAMEVAR:
    {
        int32_t i=0;
        insptr++;
        if (ud.config.scripthandle < 0)
        {
            insptr++;
            break;
        }
        switch (tw)
        {
        case CON_SAVEGAMEVAR:
            i=Gv_GetVarX(*insptr);
            SCRIPT_PutNumber(ud.config.scripthandle, "Gamevars",aGameVars[*insptr++].szLabel,i,false,false);
            break;
        case CON_READGAMEVAR:
            SCRIPT_GetNumber(ud.config.scripthandle, "Gamevars",aGameVars[*insptr].szLabel,&i);
            Gv_SetVarX(*insptr++, i);
            break;
        }
        break;
    }

    case CON_SHOWVIEW:
        insptr++;
        {
            vec3_t vec = { Gv_GetVarX(*insptr++), Gv_GetVarX(*insptr++), Gv_GetVarX(*insptr++) };
            fix16_t a = fix16_from_int(Gv_GetVarX(*insptr++));
            fix16_t horiz = fix16_from_int(Gv_GetVarX(*insptr++));

            int sect=Gv_GetVarX(*insptr++);
            int ix1=Gv_GetVarX(*insptr++);
            int iy1=Gv_GetVarX(*insptr++);
            int ix2=Gv_GetVarX(*insptr++);
            int iy2=Gv_GetVarX(*insptr++);

            G_ShowView(vec, a, horiz, sect, ix1, iy1, ix2, iy2, 0);
            break;
        }

    case CON_ROTATESPRITE16:
    case CON_ROTATESPRITE:
    case CON_ROTATESPRITEA:
        insptr++;
        {
            int32_t x = Gv_GetVarX(*insptr++), y = Gv_GetVarX(*insptr++), z = Gv_GetVarX(*insptr++);
            int32_t a = Gv_GetVarX(*insptr++), tilenum = Gv_GetVarX(*insptr++), shade = Gv_GetVarX(*insptr++);
            int32_t pal = Gv_GetVarX(*insptr++), orientation = Gv_GetVarX(*insptr++);
            int32_t alpha = (tw == CON_ROTATESPRITEA) ? Gv_GetVarX(*insptr++) : 0;
            int32_t x1 = Gv_GetVarX(*insptr++), y1 = Gv_GetVarX(*insptr++);
            int32_t x2 = Gv_GetVarX(*insptr++), y2 = Gv_GetVarX(*insptr++);

            if (tw != CON_ROTATESPRITE16 && !(orientation & ROTATESPRITE_FULL16))
            {
                x<<=16;
                y<<=16;
            }

            int32_t blendidx = 0;
            NEG_ALPHA_TO_BLEND(alpha, blendidx, orientation);

            rotatesprite_(x, y, z, a, tilenum, shade, pal, 2 | orientation, alpha, blendidx, x1, y1, x2, y2);
            break;
        }

    case CON_MINITEXT:
    case CON_GAMETEXT:
    case CON_GAMETEXTZ:
    case CON_DIGITALNUMBER:
    case CON_DIGITALNUMBERZ:
        insptr++;
        {
            int tilenum = (tw == CON_GAMETEXT || tw == CON_GAMETEXTZ || tw == CON_DIGITALNUMBER || tw == CON_DIGITALNUMBERZ)?Gv_GetVarX(*insptr++):0;
            int x=Gv_GetVarX(*insptr++), y=Gv_GetVarX(*insptr++), q=Gv_GetVarX(*insptr++);
            int shade=Gv_GetVarX(*insptr++), pal=Gv_GetVarX(*insptr++);

            if (tw == CON_GAMETEXT || tw == CON_GAMETEXTZ || tw == CON_DIGITALNUMBER || tw == CON_DIGITALNUMBERZ)
            {
                int orientation=Gv_GetVarX(*insptr++);
                int x1=Gv_GetVarX(*insptr++), y1=Gv_GetVarX(*insptr++);
                int x2=Gv_GetVarX(*insptr++), y2=Gv_GetVarX(*insptr++);
                int z=65536;

                if (tw == CON_GAMETEXT || tw == CON_GAMETEXTZ)
                {
                    int z=65536;
                    if ((apStrings[q] == NULL) && g_scriptSanityChecks)
                    {
                        OSD_Printf(CON_ERROR "null quote %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),q);
                        if (tw == CON_GAMETEXTZ)
                            insptr++;
                        break;
                    }
                    if (tw == CON_GAMETEXTZ)
                        z = Gv_GetVarX(*insptr++);
                    G_PrintGameText(0,tilenum,x>>1,y,apStrings[q],shade,pal,orientation,x1,y1,x2,y2,z);
                    break;
                }
                if (tw == CON_DIGITALNUMBERZ)
                    z= Gv_GetVarX(*insptr++);
                G_DrawTXDigiNumZ(tilenum,x,y,q,shade,pal,orientation,x1,y1,x2,y2,z);
                break;
            }

            if ((apStrings[q] == NULL) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "null quote %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),q);
                break;
            }
            minitextshade(x,y,apStrings[q],shade,pal,26);
            break;
        }

    case CON_SCREENTEXT:
        insptr++;
        {
            int32_t tilenum = Gv_GetVarX(*insptr++);
            int32_t x = Gv_GetVarX(*insptr++), y = Gv_GetVarX(*insptr++), z = Gv_GetVarX(*insptr++);
            int32_t blockangle = Gv_GetVarX(*insptr++), charangle = Gv_GetVarX(*insptr++);
            int32_t q = Gv_GetVarX(*insptr++);
            int32_t shade = Gv_GetVarX(*insptr++), pal = Gv_GetVarX(*insptr++);
            int32_t orientation = Gv_GetVarX(*insptr++);
            int32_t alpha = Gv_GetVarX(*insptr++);
            int32_t xspace = Gv_GetVarX(*insptr++), yline = Gv_GetVarX(*insptr++);
            int32_t xbetween = Gv_GetVarX(*insptr++), ybetween = Gv_GetVarX(*insptr++);
            int32_t f = Gv_GetVarX(*insptr++);
            int32_t x1 = Gv_GetVarX(*insptr++), y1 = Gv_GetVarX(*insptr++);
            int32_t x2 = Gv_GetVarX(*insptr++), y2 = Gv_GetVarX(*insptr++);

            if (tilenum < 0 || tilenum + 255 >= MAXTILES)
            {
                CON_ERRPRINTF("invalid base tilenum %d\n", tilenum);
                break;
            }

            if ((unsigned)q >= MAXQUOTES)
            {
                CON_ERRPRINTF("invalid quote ID %d\n", q);
                break;
            }

            if ((apStrings[q] == NULL))
            {
                CON_ERRPRINTF("null quote %d\n", q);
                break;
            }

            orientation &= (ROTATESPRITE_MAX - 1);

            G_ScreenText(tilenum, x, y, z, blockangle, charangle, apStrings[q], shade, pal, orientation, alpha, xspace, yline, xbetween, ybetween, f, x1, y1, x2, y2);
            break;
        }

    case CON_ANGOFF:
        insptr++;
        spriteext[vm.spriteNum].mdangoff=*insptr++;
        break;

    case CON_GETZRANGE:
        insptr++;
        {
            int x=Gv_GetVarX(*insptr++), y=Gv_GetVarX(*insptr++), z=Gv_GetVarX(*insptr++);
            int sectnum=Gv_GetVarX(*insptr++);
            int ceilzvar=*insptr++, ceilhitvar=*insptr++, florzvar=*insptr++, florhitvar=*insptr++;
            int walldist=Gv_GetVarX(*insptr++), clipmask=Gv_GetVarX(*insptr++);
            int ceilz, ceilhit, florz, florhit;

            if ((sectnum<0 || sectnum>=numsectors) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "Invalid sector %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),sectnum);
                break;
            }
            getzrange_old(x, y, z, sectnum, &ceilz, &ceilhit, &florz, &florhit, walldist, clipmask);
            Gv_SetVarX(ceilzvar, ceilz);
            Gv_SetVarX(ceilhitvar, ceilhit);
            Gv_SetVarX(florzvar, florz);
            Gv_SetVarX(florhitvar, florhit);
            break;
        }

    case CON_SECTSETINTERPOLATION:
    case CON_SECTCLEARINTERPOLATION:
        insptr++;
        {
            int const sectnum = Gv_GetVarX(*insptr++);

            if ((sectnum < 0 || sectnum >= numsectors) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "Invalid sector %d\n", g_errorLineNum, VM_GetKeywordForID(g_tw), sectnum);
                break;
            }

            Sect_SetInterpolation(sectnum, (tw == CON_SECTSETINTERPOLATION));

            break;
        }

    case CON_HITSCAN:
        insptr++;
        {
            struct
            {
                vec3_t  origin;
                int32_t sectnum;
                vec3_t  vect;
            } v;

            v.origin.x = Gv_GetVarX(*insptr++);
            v.origin.y = Gv_GetVarX(*insptr++);
            v.origin.z = Gv_GetVarX(*insptr++);
            v.sectnum = Gv_GetVarX(*insptr++);
            v.vect.x = Gv_GetVarX(*insptr++);
            v.vect.y = Gv_GetVarX(*insptr++);
            v.vect.z = Gv_GetVarX(*insptr++);

            int hitsectvar=*insptr++, hitwallvar=*insptr++, hitspritevar=*insptr++;
            int hitxvar=*insptr++, hityvar=*insptr++, hitzvar=*insptr++, cliptype=Gv_GetVarX(*insptr++);

            if ((v.sectnum<0 || v.sectnum>=numsectors) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "Invalid sector %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),v.sectnum);
                break;
            }

            hitdata_t hit;
            hitscan(&v.origin, v.sectnum, v.vect.x, v.vect.y, v.vect.z, &hit, cliptype);
            Gv_SetVarX(hitsectvar, hit.sect);
            Gv_SetVarX(hitwallvar, hit.wall);
            Gv_SetVarX(hitspritevar, hit.sprite);
            Gv_SetVarX(hitxvar, hit.x);
            Gv_SetVarX(hityvar, hit.y);
            Gv_SetVarX(hitzvar, hit.z);
            break;
        }

    case CON_CANSEE:
        insptr++;
        {
            int x1=Gv_GetVarX(*insptr++), y1=Gv_GetVarX(*insptr++), z1=Gv_GetVarX(*insptr++);
            int sect1=Gv_GetVarX(*insptr++);
            int x2=Gv_GetVarX(*insptr++), y2=Gv_GetVarX(*insptr++), z2=Gv_GetVarX(*insptr++);
            int sect2=Gv_GetVarX(*insptr++), rvar=*insptr++;

            if ((sect1<0 || sect1>=numsectors || sect2<0 || sect2>=numsectors) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "Invalid sector\n",g_errorLineNum,VM_GetKeywordForID(g_tw));
                Gv_SetVarX(rvar, 0);
            }

            Gv_SetVarX(rvar, cansee(x1,y1,z1,sect1,x2,y2,z2,sect2));
            break;
        }

    case CON_ROTATEPOINT:
        insptr++;
        {
            vec2_t pivot;
            pivot.x = Gv_GetVarX(*insptr++);
            pivot.y = Gv_GetVarX(*insptr++);

            vec2_t point;
            point.x = Gv_GetVarX(*insptr++);
            point.y = Gv_GetVarX(*insptr++);

            int daang=Gv_GetVarX(*insptr++);
            int x2var=*insptr++, y2var=*insptr++;

            vec2_t point2;

            rotatepoint(pivot,point,daang,&point2);
            Gv_SetVarX(x2var, point2.x);
            Gv_SetVarX(y2var, point2.y);
            break;
        }

    case CON_NEARTAG:
        insptr++;
        {
            //             neartag(int x, int y, int z, short sectnum, short ang,  //Starting position & angle
            //                     short *neartagsector,   //Returns near sector if sector[].tag != 0
            //                     short *neartagwall,     //Returns near wall if wall[].tag != 0
            //                     short *neartagsprite,   //Returns near sprite if sprite[].tag != 0
            //                     int *neartaghitdist,   //Returns actual distance to object (scale: 1024=largest grid size)
            //                     int neartagrange,      //Choose maximum distance to scan (scale: 1024=largest grid size)
            //                     char tagsearch)         //1-lotag only, 2-hitag only, 3-lotag&hitag

            int x=Gv_GetVarX(*insptr++), y=Gv_GetVarX(*insptr++), z=Gv_GetVarX(*insptr++);
            int sectnum=Gv_GetVarX(*insptr++), ang=Gv_GetVarX(*insptr++);
            int neartagsectorvar=*insptr++, neartagwallvar=*insptr++, neartagspritevar=*insptr++, neartaghitdistvar=*insptr++;
            int neartagrange=Gv_GetVarX(*insptr++), tagsearch=Gv_GetVarX(*insptr++);

            if ((sectnum<0 || sectnum>=numsectors) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "Invalid sector %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),sectnum);
                break;
            }
            neartag(x, y, z, sectnum, ang, &neartagsector, &neartagwall, &neartagsprite, &neartaghitdist, neartagrange, tagsearch, NULL);

            Gv_SetVarX(neartagsectorvar, neartagsector);
            Gv_SetVarX(neartagwallvar, neartagwall);
            Gv_SetVarX(neartagspritevar, neartagsprite);
            Gv_SetVarX(neartaghitdistvar, neartaghitdist);
            break;
        }

    case CON_GETTIMEDATE:
        insptr++;
        {
            int v1=*insptr++,v2=*insptr++,v3=*insptr++,v4=*insptr++,v5=*insptr++,v6=*insptr++,v7=*insptr++,v8=*insptr++;
            time_t rawtime;
            struct tm * ti;

            time(&rawtime);
            ti=localtime(&rawtime);
            // LOG_F("Time&date: %s\n",asctime (ti));

            Gv_SetVarX(v1, ti->tm_sec);
            Gv_SetVarX(v2, ti->tm_min);
            Gv_SetVarX(v3, ti->tm_hour);
            Gv_SetVarX(v4, ti->tm_mday);
            Gv_SetVarX(v5, ti->tm_mon);
            Gv_SetVarX(v6, ti->tm_year+1900);
            Gv_SetVarX(v7, ti->tm_wday);
            Gv_SetVarX(v8, ti->tm_yday);
            break;
        }

    case CON_MOVESPRITE:
    case CON_SETSPRITE:
        insptr++;
        {
            int spritenum = Gv_GetVarX(*insptr++);
            vec3_t vect = { Gv_GetVarX(*insptr++), Gv_GetVarX(*insptr++), Gv_GetVarX(*insptr++) };

            if (tw == CON_SETSPRITE)
            {
                if ((spritenum < 0 || spritenum >= MAXSPRITES) && g_scriptSanityChecks)
                {
                    OSD_Printf(CON_ERROR "invalid sprite ID %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),spritenum);
                    break;
                }
                setspritez(spritenum, &vect);
                break;
            }

            {
                int cliptype = Gv_GetVarX(*insptr++);

                if ((spritenum < 0 || spritenum >= MAXSPRITES) && g_scriptSanityChecks)
                {
                    OSD_Printf(CON_ERROR "invalid sprite ID %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),spritenum);
                    insptr++;
                    break;
                }
                Gv_SetVarX(*insptr++, A_MoveSprite(spritenum, vect.x, vect.y, vect.z, cliptype));
                break;
            }
        }

    case CON_GETFLORZOFSLOPE:
    case CON_GETCEILZOFSLOPE:
        insptr++;
        {
            int sectnum = Gv_GetVarX(*insptr++);
            vec2_t pos = { Gv_GetVarX(*insptr++), Gv_GetVarX(*insptr++) };

            if ((sectnum<0 || sectnum>=numsectors) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "Invalid sector %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),sectnum);
                insptr++;
                break;
            }

            if (tw == CON_GETFLORZOFSLOPE)
            {
                Gv_SetVarX(*insptr++, yax_getflorzofslope(sectnum,pos));
                break;
            }
            Gv_SetVarX(*insptr++, yax_getceilzofslope(sectnum,pos));
            break;
        }

    case CON_INTERPOLATE:
    {
        insptr++;
        
        int32_t curVal = Gv_GetVarX(*insptr++);
        int32_t oldVal = Gv_GetVarX(*insptr++);

        int32_t smoothRatio = calc_smoothratio(totalclock, ototalclock);

        Gv_SetVarX(*insptr++, (oldVal + mulscale16((int)(curVal - oldVal), smoothRatio)));
        break;
    }

    case CON_LDISTVAR:
    {
        insptr++;

        vec2_t pos1 = { Gv_GetVarX(*insptr++), Gv_GetVarX(*insptr++) };
        vec2_t pos2 = { Gv_GetVarX(*insptr++), Gv_GetVarX(*insptr++) };

        Gv_SetVarX(*insptr++, ldist(&pos1, &pos2));
        break;
    }

    case CON_DISTVAR:
    {
        insptr++;

        vec3_t pos1 = { Gv_GetVarX(*insptr++), Gv_GetVarX(*insptr++), Gv_GetVarX(*insptr++) };
        vec3_t pos2 = { Gv_GetVarX(*insptr++), Gv_GetVarX(*insptr++), Gv_GetVarX(*insptr++) };

        Gv_SetVarX(*insptr++, dist(&pos1, &pos2));
        break;
    }

    case CON_UPDATESECTOR:
    case CON_UPDATESECTORZ:
        insptr++;
        {
            int x=Gv_GetVarX(*insptr++), y=Gv_GetVarX(*insptr++);
            int z=(tw==CON_UPDATESECTORZ)?Gv_GetVarX(*insptr++):0;
            int var=*insptr++;
            short w=sprite[vm.spriteNum].sectnum;

            if (tw==CON_UPDATESECTOR) updatesector(x,y,&w);
            else updatesectorz(x,y,z,&w);

            Gv_SetVarX(var, w);
            break;
        }

    case CON_SPAWN:
        insptr++;
        if (vm.pSprite->sectnum >= 0 && vm.pSprite->sectnum < MAXSECTORS)
            A_Spawn(vm.spriteNum,*insptr);
        insptr++;
        break;

    case CON_IFWASWEAPON:
        insptr++;
        branch(actor[vm.spriteNum].htpicnum == *insptr);
        break;

    case CON_IFAI:
        branch(AC_AI_ID(vm.pData) == *(++insptr));
        break;

    case CON_IFACTION:
        branch(AC_ACTION_ID(vm.pData) == *(++insptr));
        break;

    case CON_IFACTIONCOUNT:
        branch(AC_ACTION_COUNT(vm.pData) >= *(++insptr));
        break;

    case CON_RESETACTIONCOUNT:
        insptr++;
        AC_ACTION_COUNT(vm.pData) = 0;
        break;

    case CON_DEBRIS:
        insptr++;
        {
            int dnum = *insptr++;

            if (vm.pSprite->sectnum >= 0 && vm.pSprite->sectnum < MAXSECTORS)
                for (j=(*insptr)-1;j>=0;j--)
                {
                    if (vm.pSprite->picnum == BLIMP && dnum == SCRAP1)
                        s = 0;
                    else s = (krand()%3);

                    l = A_InsertSprite(vm.pSprite->sectnum,
                                       vm.pSprite->x+(krand()&255)-128,vm.pSprite->y+(krand()&255)-128,vm.pSprite->z-(8<<8)-(krand()&8191),
                                       dnum+s,vm.pSprite->shade,32+(krand()&15),32+(krand()&15),
                                       krand()&2047,(krand()&127)+32,
                                       -(krand()&2047),vm.spriteNum,5);
                    if (vm.pSprite->picnum == BLIMP && dnum == SCRAP1)
                        sprite[l].yvel = g_blimpSpawnItems[j%14];
                    else sprite[l].yvel = -1;
                    sprite[l].pal = vm.pSprite->pal;
                }
            insptr++;
        }
        break;

    case CON_COUNT:
        insptr++;
        vm.pData[0] = (short) *insptr++;
        break;

    case CON_CSTATOR:
        insptr++;
        vm.pSprite->cstat |= (short) *insptr++;
        break;

    case CON_CLIPDIST:
        insptr++;
        vm.pSprite->clipdist = (short) *insptr++;
        break;

    case CON_CSTAT:
        insptr++;
        vm.pSprite->cstat = (short) *insptr++;
        break;
    case CON_SAVENN:
    case CON_SAVE:
        insptr++;
        {
            time_t curtime;

            g_lastSaveSlot = *insptr++;

            if (g_lastSaveSlot > 9)
                break;
            if ((tw == CON_SAVE) || !(ud.savegame[g_lastSaveSlot][0]))
            {
                curtime = time(NULL);
                Bstrcpy(tempbuf,asctime(localtime(&curtime)));
                clearbuf(ud.savegame[g_lastSaveSlot],sizeof(ud.savegame[g_lastSaveSlot]),0);
                Bsprintf(ud.savegame[g_lastSaveSlot],"Auto");
//            for (j=0;j<13;j++)
//                Bmemcpy(&ud.savegame[g_lastSaveSlot][j+4],&tempbuf[j+3],sizeof(tempbuf[j+3]));
//            ud.savegame[g_lastSaveSlot][j+4] = '\0';
                Bmemcpy(&ud.savegame[g_lastSaveSlot][4],&tempbuf[3],sizeof(tempbuf[0])*13);
                ud.savegame[g_lastSaveSlot][17] = '\0';
            }
            OSD_Printf("Saving to slot %d\n",g_lastSaveSlot);

            KB_FlushKeyboardQueue();

            g_screenCapture = 1;
            G_DrawRooms(myconnectindex,65536);
            g_screenCapture = 0;
            if (ud.multimode > 1)
                G_SavePlayer(-1-(g_lastSaveSlot));
            else G_SavePlayer(g_lastSaveSlot);

            break;
        }

    case CON_QUAKE:
        insptr++;
        g_earthquakeTime = (char)Gv_GetVarX(*insptr++);
        A_PlaySound(EARTHQUAKE,g_player[screenpeek].ps->i);
        break;

    case CON_IFMOVE:
        insptr++;
        branch(vm.pData[1] == *insptr);
        break;

    case CON_RESETPLAYER:
    {
        insptr++;

        //AddLog("resetplayer");
        if (ud.multimode < 2)
        {
            if (g_lastSaveSlot >= 0 && ud.recstat != 2)
            {
                ud.gm = MODE_MENU;
                KB_ClearKeyDown(sc_Space);
                ChangeToMenu(15000);
            }
            else ud.gm = MODE_RESTART;
            vm.flags |= VM_NOEXECUTE;
        }
        else
        {
            auto const p = g_player[vm.playerNum].ps;
            P_RandomSpawnPoint(vm.playerNum);

            vm.pSprite->cstat = 257;
            vm.pSprite->shade = -12;
            vm.pSprite->clipdist = 64;
            vm.pSprite->xrepeat = PLAYER_SPRITE_XREPEAT;
            vm.pSprite->yrepeat = PLAYER_SPRITE_YREPEAT;
            vm.pSprite->owner = vm.spriteNum;
            vm.pSprite->xoffset = 0;
            vm.pSprite->pal = g_player[vm.playerNum].ps->palookup;

            p->last_extra = vm.pSprite->extra = p->max_player_health;
            p->wantweaponfire = -1;
            p->q16horiz = F16(100);
            p->on_crane = -1;
            p->frag_ps = vm.playerNum;
            p->q16horizoff = 0;
            p->opyoff = 0;
            p->wackedbyactor = -1;
            p->inv_amount[GET_SHIELD] = g_startArmorAmount;
            p->dead_flag = 0;
            p->pals.f = 0;
            p->footprintcount = 0;
            p->weapreccnt = 0;
            p->fta = 0;
            p->ftq = 0;
            p->vel.x = p->vel.y = 0;
            p->rotscrnang = 0;
            p->runspeed = g_playerFriction;
            p->falling_counter = 0;
            p->knee_incs = 0;
            p->actorsqu = -1;
            p->lowsprite = -1;
            p->highsprite = -1;

            actor[vm.spriteNum].htextra = -1;
            actor[vm.spriteNum].htowner = vm.spriteNum;
            actor[vm.spriteNum].htpicnum = vm.pSprite->picnum;

            actor[vm.spriteNum].cgg = 0;
            actor[vm.spriteNum].movflag = 0;
            actor[vm.spriteNum].tempang = 0;
            actor[vm.spriteNum].actorstayput = -1;
            actor[vm.spriteNum].dispicnum = 0;

            P_ResetInventory(vm.playerNum);
            P_ResetWeapons(vm.playerNum);

            p->reloading = 0;
            p->movement_lock = 0;

            if (GTFLAGS(GAMETYPE_LIMITEDLIVES))
            {
                Bsprintf(apStrings[QUOTE_RESERVED4], "You have %d %s left!", p->lives, (p->lives == 1) ? "life" : "lives");
                P_DoQuote(QUOTE_RESERVED4, p);
            }

            X_OnEvent(EVENT_RESETPLAYER, p->i, vm.playerNum, -1);
            if (vm.playerNum == myconnectindex)
            {
                g_cameraDistance = 0;
                g_cameraClock = (int32_t)totalclock;
            }
        }
        P_UpdateScreenPal(g_player[vm.playerNum].ps);
        //AddLog("EOF: resetplayer");
    }
    break;

    case CON_IFONWATER:
        branch(klabs(vm.pSprite->z-sector[vm.pSprite->sectnum].floorz) < (32<<8) && sector[vm.pSprite->sectnum].lotag == ST_1_ABOVE_WATER);
        break;

    case CON_IFINWATER:
        branch(sector[vm.pSprite->sectnum].lotag == ST_2_UNDERWATER);
        break;

    case CON_IFCOUNT:
        insptr++;
        branch(vm.pData[0] >= *insptr);
        break;

    case CON_IFACTOR:
        branch(vm.pSprite->picnum == *(++insptr));
        break;

    case CON_RESETCOUNT:
        insptr++;
        vm.pData[0] = 0;
        break;

    case CON_ADDINVENTORY:
        insptr += 2;

        VM_AddInventory(g_player[vm.playerNum].ps, insptr[-1], *insptr);

        insptr++;
        break;

    case CON_HITRADIUSVAR:
        insptr++;
        {
            int v1=Gv_GetVarX(*insptr++),v2=Gv_GetVarX(*insptr++),v3=Gv_GetVarX(*insptr++);
            int v4=Gv_GetVarX(*insptr++),v5=Gv_GetVarX(*insptr++);
            A_RadiusDamage(vm.spriteNum,v1,v2,v3,v4,v5);
        }
        break;

    case CON_HITRADIUS:
        A_RadiusDamage(vm.spriteNum,*(insptr+1),*(insptr+2),*(insptr+3),*(insptr+4),*(insptr+5));
        insptr+=6;
        break;

    case CON_IFP:
    {
//        insptr++;

        l = *(++insptr);
        j = 0;

        s = sprite[g_player[vm.playerNum].ps->i].xvel;

        if ((l & pducking) && g_player[vm.playerNum].ps->on_ground && TEST_SYNC_KEY(g_player[vm.playerNum].inputBits->bits, SK_CROUCH))
            j = 1;
        else if ((l & pfalling) && g_player[vm.playerNum].ps->jumping_counter == 0 && !g_player[vm.playerNum].ps->on_ground &&
                 g_player[vm.playerNum].ps->vel.z > 2048)
            j = 1;
        else if ((l & pjumping) && g_player[vm.playerNum].ps->jumping_counter > 348)
            j = 1;
        else if ((l & pstanding) && s >= 0 && s < 8)
            j = 1;
        else if ((l & pwalking) && s >= 8 && !TEST_SYNC_KEY(g_player[vm.playerNum].inputBits->bits, SK_RUN))
            j = 1;
        else if ((l & prunning) && s >= 8 && TEST_SYNC_KEY(g_player[vm.playerNum].inputBits->bits, SK_RUN))
            j = 1;
        else if ((l & phigher) && g_player[vm.playerNum].ps->pos.z < (vm.pSprite->z-(48<<8)))
            j = 1;
        else if ((l & pwalkingback) && s <= -8 && !TEST_SYNC_KEY(g_player[vm.playerNum].inputBits->bits, SK_RUN))
            j = 1;
        else if ((l & prunningback) && s <= -8 && TEST_SYNC_KEY(g_player[vm.playerNum].inputBits->bits, SK_RUN))
            j = 1;
        else if ((l & pkicking) && (g_player[vm.playerNum].ps->quick_kick > 0 || (g_player[vm.playerNum].ps->curr_weapon == KNEE_WEAPON && g_player[vm.playerNum].ps->kickback_pic > 0)))
            j = 1;
        else if ((l & pshrunk) && sprite[g_player[vm.playerNum].ps->i].xrepeat < 32)
            j = 1;
        else if ((l & pjetpack) && g_player[vm.playerNum].ps->jetpack_on)
            j = 1;
        else if ((l & ponsteroids) && g_player[vm.playerNum].ps->inv_amount[GET_STEROIDS] > 0 && g_player[vm.playerNum].ps->inv_amount[GET_STEROIDS] < 400)
            j = 1;
        else if ((l & ponground) && g_player[vm.playerNum].ps->on_ground)
            j = 1;
        else if ((l & palive) && sprite[g_player[vm.playerNum].ps->i].xrepeat > 32 && sprite[g_player[vm.playerNum].ps->i].extra > 0 && g_player[vm.playerNum].ps->timebeforeexit == 0)
            j = 1;
        else if ((l & pdead) && sprite[g_player[vm.playerNum].ps->i].extra <= 0)
            j = 1;
        else if ((l & pfacing))
        {
            if (vm.pSprite->picnum == APLAYER && ud.multimode > 1)
                j = G_GetAngleDelta(fix16_to_int(g_player[otherp].ps->q16ang),getangle(g_player[vm.playerNum].ps->pos.x-g_player[otherp].ps->pos.x,g_player[vm.playerNum].ps->pos.y-g_player[otherp].ps->pos.y));
            else
                j = G_GetAngleDelta(fix16_to_int(g_player[vm.playerNum].ps->q16ang),getangle(vm.pSprite->x-g_player[vm.playerNum].ps->pos.x,vm.pSprite->y-g_player[vm.playerNum].ps->pos.y));

            if (j > -128 && j < 128)
                j = 1;
            else
                j = 0;
        }
        branch((intptr_t) j);
    }
    break;

    case CON_IFSTRENGTH:
        insptr++;
        branch(vm.pSprite->extra <= *insptr);
        break;

    case CON_GUTS:
        insptr += 2;
        A_DoGuts(vm.spriteNum,*(insptr-1),*insptr);
        insptr++;
        break;

    case CON_IFSPAWNEDBY:
        insptr++;
        branch(actor[vm.spriteNum].htpicnum == *insptr);
        break;

    case CON_WACKPLAYER:
        insptr++;
        P_ForceAngle(g_player[vm.playerNum].ps);
        return 0;

    case CON_FLASH:
        insptr++;
        sprite[vm.spriteNum].shade = -127;
        g_player[vm.playerNum].display.visibility = -127;
        return 0;

    case CON_SAVEMAPSTATE:
        if (g_mapInfo[ud.volume_number*MAXLEVELS+ud.level_number].savedstate == NULL)
            g_mapInfo[ud.volume_number*MAXLEVELS+ud.level_number].savedstate = (mapstate_t*)Xcalloc(1,sizeof(mapstate_t));
        G_SaveMapState(g_mapInfo[ud.volume_number*MAXLEVELS+ud.level_number].savedstate);
        insptr++;
        return 0;

    case CON_LOADMAPSTATE:
        if (g_mapInfo[ud.volume_number*MAXLEVELS+ud.level_number].savedstate)
            G_RestoreMapState(g_mapInfo[ud.volume_number*MAXLEVELS+ud.level_number].savedstate);
        insptr++;
        return 0;

    case CON_CLEARMAPSTATE:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        if ((j < 0 || j >= MAXVOLUMES*MAXLEVELS) && g_scriptSanityChecks)
        {
            OSD_Printf(CON_ERROR "Invalid map number: %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),j);
            return 0;
        }
        if (g_mapInfo[j].savedstate)
            G_FreeMapState(j);
        return 0;

    case CON_STOPALLSOUNDS:
        insptr++;
        if (screenpeek == vm.playerNum)
            FX_StopAllSounds();
        return 0;

    case CON_IFGAPZL:
        insptr++;
        branch(((actor[vm.spriteNum].floorz - actor[vm.spriteNum].ceilingz) >> 8) < *insptr);
        break;

    case CON_IFHITSPACE:
    {
        int hitSpace = TEST_SYNC_KEY(g_player[vm.playerNum].inputBits->bits, SK_OPEN);

        // SURVIVAL MODE: This had to go here instead of resetplayer, otherwise we'll get buggy behaviour when space is struck.
        if (hitSpace && vm.pSprite->picnum == APLAYER && vm.pSprite->owner >= 0 && vm.pSprite->yvel == vm.playerNum && vm.pSprite->extra <= 0)
        {
            auto const p = g_player[vm.playerNum].ps;
            if (GTFLAGS(GAMETYPE_LIMITEDLIVES))
            {
                // Make & change to p->lives later!
                bool allDead = true;
                for (int ALL_PLAYERS(i))
                {
                    if (g_player[i].ps->lives > 0)
                    {
                        allDead = false;
                        break;
                    }
                }

                if (allDead)
                    ud.gm = MODE_RESTART;

                if (p->lives <= 0)
                {
                    Bstrcpy(apStrings[QUOTE_RESERVED4], "You are out of lives!");
                    P_DoQuote(QUOTE_RESERVED4, g_player[vm.playerNum].ps);
                    hitSpace = 0;
                }
            }
        }

        branch(hitSpace);
        break;
    }

    case CON_IFOUTSIDE:
        branch(sector[vm.pSprite->sectnum].ceilingstat&1);
        break;

    case CON_IFMULTIPLAYER:
        branch(ud.multimode > 1);
        break;

    case CON_OPERATE:
        insptr++;
        if (sector[vm.pSprite->sectnum].lotag == ST_0_NO_EFFECT)
        {
            neartag(vm.pSprite->x,vm.pSprite->y,vm.pSprite->z-(32<<8),vm.pSprite->sectnum,vm.pSprite->ang,&neartagsector,&neartagwall,&neartagsprite,&neartaghitdist,768L,1, NULL);
            if (neartagsector >= 0 && isanearoperator(sector[neartagsector].lotag))
                if ((sector[neartagsector].lotag&0xff) == 23 || sector[neartagsector].floorz == sector[neartagsector].ceilingz)
                    if ((sector[neartagsector].lotag&16384) == 0)
                        if ((sector[neartagsector].lotag&32768) == 0)
                        {
                            j = headspritesect[neartagsector];
                            while (j >= 0)
                            {
                                if (sprite[j].picnum == ACTIVATOR)
                                    break;
                                j = nextspritesect[j];
                            }
                            if (j == -1)
                                G_OperateSectors(neartagsector,vm.spriteNum);
                        }
        }
        break;

    case CON_IFINSPACE:
        branch(G_CheckForSpaceCeiling(vm.pSprite->sectnum));
        break;

    case CON_SPRITEPAL:
        insptr++;
        if (vm.pSprite->picnum != APLAYER)
            actor[vm.spriteNum].tempang = vm.pSprite->pal;
        vm.pSprite->pal = *insptr++;
        break;

    case CON_CACTOR:
        insptr++;
        vm.pSprite->picnum = *insptr++;
        break;

    case CON_IFBULLETNEAR:
        branch(A_Dodge(vm.pSprite) == 1);
        break;

    case CON_IFRESPAWN:
        if (A_CheckEnemySprite(vm.pSprite))
            branch(DMFLAGS_TEST(DMFLAG_RESPAWNMONSTERS));
        else if (A_CheckInventorySprite(vm.pSprite))
            branch(DMFLAGS_TEST(DMFLAG_RESPAWNINVENTORY));
        else if (tileGetMapping(vm.pSprite->picnum) == ACCESSCARD__) // Shim for poorly-concieved CON code. Keys should never respawn.
            branch(0);
        else
            branch(DMFLAGS_TEST(DMFLAG_RESPAWNITEMS));
        break;

    case CON_IFFLOORDISTL:
        insptr++;
        branch((actor[vm.spriteNum].floorz - vm.pSprite->z) <= ((*insptr)<<8));
        break;

    case CON_IFCEILINGDISTL:
        insptr++;
        branch((vm.pSprite->z - actor[vm.spriteNum].ceilingz) <= ((*insptr)<<8));
        break;

    case CON_PALFROM:
        insptr++;
        {
            palette_t const pal = { uint8_t(insptr[1]), uint8_t(insptr[2]), uint8_t(insptr[3]), uint8_t(insptr[0]) };
            insptr += 4;
            P_PalFrom(g_player[vm.playerNum].ps, pal.f, pal.r, pal.g, pal.b);
        }
        break;

    case CON_QSPRINTF:
        insptr++;
        {
            int const outputQuote = Gv_GetVarX(*insptr++);
            int const inputQuote = Gv_GetVarX(*insptr++);

            VM_ABORT_IF((apStrings[inputQuote] == nullptr) | (apStrings[outputQuote] == nullptr), "null quote %d", apStrings[inputQuote] ? outputQuote : inputQuote);

            auto& inBuf = apStrings[inputQuote];

            int32_t arg[32];
            char    outBuf[MAXQUOTELEN];

            int const quoteLen = Bstrlen(inBuf);

            int inputPos = 0;
            int outputPos = 0;
            int argIdx = 0;

            while (VM_DECODE_INST(*insptr) != CON_NULLOP && argIdx < 32)
                arg[argIdx++] = Gv_GetVarX(*insptr++);

            int numArgs = argIdx;

            insptr++;  // skip the NOP

            argIdx = 0;

            do
            {
                while (inputPos < quoteLen && outputPos < MAXQUOTELEN && inBuf[inputPos] != '%')
                    outBuf[outputPos++] = inBuf[inputPos++];

                if (inBuf[inputPos] == '%')
                {
                    inputPos++;
                    switch (inBuf[inputPos])
                    {
                        case 'l':
                            if (inBuf[inputPos + 1] != 'd')
                            {
                                // write the % and l
                                outBuf[outputPos++] = inBuf[inputPos - 1];
                                outBuf[outputPos++] = inBuf[inputPos++];
                                break;
                            }
                            inputPos++;
                            fallthrough__;
                        case 'd':
                        {
                            if (argIdx >= numArgs)
                                goto finish_qsprintf;

                            char buf[16];
                            Bsprintf(buf, "%d", arg[argIdx++]);

                            int const bufLen = Bstrlen(buf);
                            Bmemcpy(&outBuf[outputPos], buf, bufLen);
                            outputPos += bufLen;
                            inputPos++;
                        }
                        break;

                        case '0':
                        {
                            if (argIdx >= numArgs)
                                goto finish_qsprintf;

                            if (!(isdigit(inBuf[inputPos + 1]) && inBuf[inputPos + 2] == 'd'))
                            {
                                outBuf[outputPos++] = inBuf[inputPos - 1];
                                outBuf[outputPos++] = inBuf[inputPos++];
                                break;
                            }

                            char buf[16];
                            Bsprintf(buf, "%d", arg[argIdx++]);
                            int const bufLen = Bstrlen(buf);

                            int pad = inBuf[inputPos + 1] - '0';
                            int numZeroes = pad - bufLen;
                            if (numZeroes > 0)
                            {
                                for(int i = 0; i < numZeroes; i++)
                                    outBuf[outputPos++] = '0';
                            }
                            
                            Bmemcpy(&outBuf[outputPos], buf, bufLen);
                            outputPos += bufLen;
                            inputPos+=3;
                        }
                        break;

                        case '%':
                        {
                            outBuf[outputPos++] = inBuf[inputPos++]; // Skip over doubled % signs, like native printf functions.
                        }
                        break;

                        case 's':
                        {
                            if (argIdx >= numArgs)
                                goto finish_qsprintf;

                            int const argLen = Bstrlen(apStrings[arg[argIdx]]);

                            Bmemcpy(&outBuf[outputPos], apStrings[arg[argIdx]], argLen);
                            outputPos += argLen;
                            argIdx++;
                            inputPos++;
                        }
                        break;

                        default: outBuf[outputPos++] = inBuf[inputPos - 1]; break;
                    }
                }
            } while (inputPos < quoteLen && outputPos < MAXQUOTELEN);
        finish_qsprintf:
            outBuf[outputPos] = '\0';
            Bstrncpyz(apStrings[outputQuote], outBuf, MAXQUOTELEN);
            break;
        }

    case CON_ADDLOG:
    {
        insptr++;

        OSD_Printf(OSDTEXT_GREEN "CONLOG: L=%d\n",g_errorLineNum);
        break;
    }

    case CON_ADDLOGVAR:
        insptr++;
        {
            int m=1;
            char szBuf[256];
            int lVarID = *insptr;

            if ((lVarID >= g_gameVarCount) || lVarID < 0)
            {
                if (*insptr==MAXGAMEVARS) // addlogvar for a constant?  Har.
                    insptr++;
//                else if (*insptr > g_gameVarCount && (*insptr < (MAXGAMEVARS<<1)+MAXGAMEVARS+1+MAXGAMEARRAYS))
                else if (*insptr&(MAXGAMEVARS<<2))
                {
                    int index;

                    lVarID ^= (MAXGAMEVARS<<2);

                    if (lVarID&(MAXGAMEVARS<<1))
                    {
                        m = -m;
                        lVarID ^= (MAXGAMEVARS<<1);
                    }

                    insptr++;

                    index=Gv_GetVarX(*insptr++);
                    if ((index < aGameArrays[lVarID].size)&&(index>=0))
                    {
                        OSD_Printf(OSDTEXT_GREEN "CONLOGVAR: Line %d, %s: %s[%d] =%d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),
                                   aGameArrays[lVarID].szLabel,index,(int)(m*aGameArrays[lVarID].pValues[index]));
                        break;
                    }
                    else
                    {
                        OSD_Printf(CON_ERROR "invalid array index\n",g_errorLineNum,VM_GetKeywordForID(g_tw));
                        break;
                    }
                }
                else if (*insptr&(MAXGAMEVARS<<1))
                {
                    m = -m;
                    lVarID ^= (MAXGAMEVARS<<1);
                }
                else
                {
                    // invalid varID
                    insptr++;
                    OSD_Printf(CON_ERROR "invalid variable\n",g_errorLineNum,VM_GetKeywordForID(g_tw));
                    break;  // out of switch
                }
            }
            Bsprintf(szBuf,"CONLOGVAR: L=%d %s ",g_errorLineNum, aGameVars[lVarID].szLabel);
            strcpy(g_szBuf,szBuf);

            if (aGameVars[lVarID].flags & GAMEVAR_READONLY)
            {
                Bsprintf(szBuf," (read-only)");
                strcat(g_szBuf,szBuf);
            }
            if (aGameVars[lVarID].flags & GAMEVAR_PERPLAYER)
            {
                Bsprintf(szBuf," (Per Player. Player=%d)",vm.playerNum);
            }
            else if (aGameVars[lVarID].flags & GAMEVAR_PERACTOR)
            {
                Bsprintf(szBuf," (Per Actor. Actor=%d)",vm.spriteNum);
            }
            else
            {
                Bsprintf(szBuf," (Global)");
            }
            Bstrcat(g_szBuf,szBuf);
            Bsprintf(szBuf," =%d\n", Gv_GetVarX(lVarID)*m);
            Bstrcat(g_szBuf,szBuf);
            OSD_Printf(OSDTEXT_GREEN "%s",g_szBuf);
            insptr++;
            break;
        }

    case CON_SETSECTOR:
    case CON_GETSECTOR:
        insptr++;
        {
            int const sectNum = (*insptr++ != sysVarIDs.THISACTOR) ? Gv_GetVarX(insptr[-1]) : vm.pSprite->sectnum;
            int const labelNum = *insptr++;
            int const lVar2 = *insptr++;

            VM_ABORT_IF((unsigned)sectNum >= MAXSECTORS, "invalid sector %d", sectNum);

            if(tw == CON_SETSECTOR)
                VM_SetSector(sectNum, labelNum, Gv_GetVarX(lVar2));
            else
                Gv_SetVarX(lVar2, VM_GetSector(sectNum, labelNum));

            break;
        }

    case CON_SETSECTORSTRUCT:
    case CON_GETSECTORSTRUCT:
        insptr++;
        {
            int const sectNum = (*insptr++ != sysVarIDs.THISACTOR) ? Gv_GetVarX(insptr[-1]) : vm.pSprite->sectnum;
            int const labelNum = *insptr++;
            int const lVar2 = *insptr++;

            VM_ABORT_IF((unsigned)sectNum >= MAXSECTORS, "invalid sector %d", sectNum);

            auto const& sectLabel = SectorLabels[labelNum];

            if (tw == CON_SETSECTORSTRUCT)
                VM_SetStruct(sectLabel.flags, (intptr_t*)((char*)&sector[sectNum] + sectLabel.offset), Gv_GetVarX(lVar2));
            else
                Gv_SetVarX(lVar2, VM_GetStruct(sectLabel.flags, (intptr_t*)((char*)&sector[sectNum] + sectLabel.offset)));

            break;
        }

    case CON_SQRT:
        insptr++;
        {
            // syntax sqrt <invar> <outvar>
            int lInVarID=*insptr++, lOutVarID=*insptr++;

            Gv_SetVarX(lOutVarID, ksqrt(Gv_GetVarX(lInVarID)));
            break;
        }

    case CON_FINDNEARACTOR:
    case CON_FINDNEARSPRITE:
    case CON_FINDNEARACTOR3D:
    case CON_FINDNEARSPRITE3D:
        insptr++;
        {
            // syntax findnearactorvar <type> <maxdist> <getvar>
            // gets the sprite ID of the nearest actor within max dist
            // that is of <type> into <getvar>
            // -1 for none found
            // <type> <maxdist> <varid>
            int lType=*insptr++, lMaxDist=*insptr++, lVarID=*insptr++;
            int lFound=-1, j, k = MAXSTATUS-1;

            if (tw == CON_FINDNEARACTOR || tw == CON_FINDNEARACTOR3D)
                k = 1;
            do
            {
                j=headspritestat[k];    // all sprites
                if (tw==CON_FINDNEARSPRITE3D || tw==CON_FINDNEARACTOR3D)
                {
                    while (j>=0)
                    {
                        if (sprite[j].picnum == lType && j != vm.spriteNum && dist(&sprite[vm.spriteNum], &sprite[j]) < lMaxDist)
                        {
                            lFound=j;
                            j = MAXSPRITES;
                            break;
                        }
                        j = nextspritestat[j];
                    }
                    if (j == MAXSPRITES || tw == CON_FINDNEARACTOR3D)
                        break;
                    continue;
                }

                while (j>=0)
                {
                    if (sprite[j].picnum == lType && j != vm.spriteNum && ldist(&sprite[vm.spriteNum], &sprite[j]) < lMaxDist)
                    {
                        lFound=j;
                        j = MAXSPRITES;
                        break;
                    }
                    j = nextspritestat[j];
                }

                if (j == MAXSPRITES || tw == CON_FINDNEARACTOR)
                    break;
            }
            while (k--);
            Gv_SetVarX(lVarID, lFound);
            break;
        }

    case CON_FINDNEARACTORVAR:
    case CON_FINDNEARSPRITEVAR:
    case CON_FINDNEARACTOR3DVAR:
    case CON_FINDNEARSPRITE3DVAR:
        insptr++;
        {
            // syntax findnearactorvar <type> <maxdistvar> <getvar>
            // gets the sprite ID of the nearest actor within max dist
            // that is of <type> into <getvar>
            // -1 for none found
            // <type> <maxdistvarid> <varid>
            int lType=*insptr++, lMaxDist=Gv_GetVarX(*insptr++), lVarID=*insptr++;
            int lFound=-1, j, k = 1;

            if (tw == CON_FINDNEARSPRITEVAR || tw == CON_FINDNEARSPRITE3DVAR)
                k = MAXSTATUS-1;

            do
            {
                j=headspritestat[k];    // all sprites
                if (tw==CON_FINDNEARACTOR3DVAR || tw==CON_FINDNEARSPRITE3DVAR)
                {
                    while (j >= 0)
                    {
                        if (sprite[j].picnum == lType && j != vm.spriteNum && dist(&sprite[vm.spriteNum], &sprite[j]) < lMaxDist)
                        {
                            lFound=j;
                            j = MAXSPRITES;
                            break;
                        }
                        j = nextspritestat[j];
                    }
                    if (j == MAXSPRITES || tw==CON_FINDNEARACTOR3DVAR)
                        break;
                    continue;
                }

                while (j >= 0)
                {
                    if (sprite[j].picnum == lType && j != vm.spriteNum && ldist(&sprite[vm.spriteNum], &sprite[j]) < lMaxDist)
                    {
                        lFound=j;
                        j = MAXSPRITES;
                        break;
                    }
                    j = nextspritestat[j];
                }

                if (j == MAXSPRITES || tw==CON_FINDNEARACTORVAR)
                    break;
            }
            while (k--);
            Gv_SetVarX(lVarID, lFound);
            break;
        }

    case CON_FINDNEARACTORZVAR:
    case CON_FINDNEARSPRITEZVAR:
        insptr++;
        {
            // syntax findnearactorvar <type> <maxdistvar> <getvar>
            // gets the sprite ID of the nearest actor within max dist
            // that is of <type> into <getvar>
            // -1 for none found
            // <type> <maxdistvarid> <varid>
            int lType=*insptr++, lMaxDist=Gv_GetVarX(*insptr++);
            int lMaxZDist=Gv_GetVarX(*insptr++);
            int lVarID=*insptr++, lFound=-1, lTemp, lTemp2, j, k=MAXSTATUS-1;
            do
            {
                j=headspritestat[tw==CON_FINDNEARACTORZVAR?1:k];    // all sprites
                if (j == -1) continue;
                do
                {
                    if (sprite[j].picnum == lType && j != vm.spriteNum)
                    {
                        lTemp=ldist(&sprite[vm.spriteNum], &sprite[j]);
                        if (lTemp < lMaxDist)
                        {
                            lTemp2=klabs(sprite[vm.spriteNum].z-sprite[j].z);
                            if (lTemp2 < lMaxZDist)
                            {
                                lFound=j;
                                j = MAXSPRITES;
                                break;
                            }
                        }
                    }
                    j = nextspritestat[j];
                }
                while (j>=0);
                if (tw==CON_FINDNEARACTORZVAR || j == MAXSPRITES)
                    break;
            }
            while (k--);
            Gv_SetVarX(lVarID, lFound);

            break;
        }

    case CON_FINDNEARACTORZ:
    case CON_FINDNEARSPRITEZ:
        insptr++;
        {
            // syntax findnearactorvar <type> <maxdist> <getvar>
            // gets the sprite ID of the nearest actor within max dist
            // that is of <type> into <getvar>
            // -1 for none found
            // <type> <maxdist> <varid>
            int lType=*insptr++, lMaxDist=*insptr++, lMaxZDist=*insptr++, lVarID=*insptr++;
            int lTemp, lTemp2, lFound=-1, j, k=MAXSTATUS-1;
            do
            {
                j=headspritestat[tw==CON_FINDNEARACTORZ?1:k];    // all sprites
                if (j == -1) continue;
                do
                {
                    if (sprite[j].picnum == lType && j != vm.spriteNum)
                    {
                        lTemp=ldist(&sprite[vm.spriteNum], &sprite[j]);
                        if (lTemp < lMaxDist)
                        {
                            lTemp2=klabs(sprite[vm.spriteNum].z-sprite[j].z);
                            if (lTemp2 < lMaxZDist)
                            {
                                lFound=j;
                                j = MAXSPRITES;
                                break;
                            }
                        }
                    }
                    j = nextspritestat[j];
                }
                while (j>=0);
                if (tw==CON_FINDNEARACTORZ || j == MAXSPRITES)
                    break;
            }
            while (k--);
            Gv_SetVarX(lVarID, lFound);
            break;
        }

    case CON_FINDPLAYER:
        insptr++;
        Gv_SetVarX(sysVarIDs.RETURN, A_FindPlayer(&sprite[vm.spriteNum],&j));
        Gv_SetVarX(*insptr++, j);
        break;

    case CON_FINDOTHERPLAYER:
        insptr++;
        Gv_SetVarX(sysVarIDs.RETURN, P_FindOtherPlayer(vm.playerNum,&j));
        Gv_SetVarX(*insptr++, j);
        break;

    case CON_SETPLAYER:
    case CON_GETPLAYER:
        insptr++;
        {
            int const   playerNum   = (*insptr++ != sysVarIDs.THISACTOR) ? Gv_GetVarX(insptr[-1]) : vm.playerNum;
            int const   labelNum    = *insptr++;
            auto const& playerLabel = PlayerLabels[labelNum];
            int const   lParm2      = (playerLabel.flags & LABEL_HASPARM2) ? Gv_GetVarX(*insptr++) : 0;

            VM_ABORT_IF(((unsigned)playerNum >= MAXPLAYERS) | ((unsigned)lParm2 > (unsigned)playerLabel.maxParm2),
                "%s[%d] invalid for player %d", playerLabel.name, lParm2, playerNum);

            int const lVar2 = *insptr++;

            if (tw == CON_SETPLAYER)
                VM_SetPlayer(playerNum, labelNum, lParm2, Gv_GetVarX(lVar2));
            else
                Gv_SetVarX(lVar2, VM_GetPlayer(playerNum, labelNum, lParm2));

            break;
        }

    case CON_SETPLAYERSTRUCT:
    case CON_GETPLAYERSTRUCT:
        insptr++;
        {
            int const   playerNum = (*insptr++ != sysVarIDs.THISACTOR) ? Gv_GetVarX(insptr[-1]) : vm.playerNum;
            int const   labelNum = *insptr++;
            auto const& playerLabel = PlayerLabels[labelNum];
            int const   lParm2 = (playerLabel.flags & LABEL_HASPARM2) ? Gv_GetVarX(*insptr++) : 0;

            VM_ABORT_IF(((unsigned)playerNum >= MAXPLAYERS) | ((unsigned)lParm2 > (unsigned)playerLabel.maxParm2),
                "%s[%d] invalid for player %d", playerLabel.name, lParm2, playerNum);

            int const lVar2 = *insptr++;

            if (tw == CON_SETPLAYERSTRUCT)
                VM_SetStruct(playerLabel.flags, (intptr_t*)((char*)&g_player[playerNum].ps[0] + playerLabel.offset), Gv_GetVarX(lVar2));
            else
                Gv_SetVarX(lVar2, VM_GetStruct(playerLabel.flags, (intptr_t*)((char*)&g_player[playerNum].ps[0] + playerLabel.offset)));

            break;
        }

    case CON_SETINPUT:
    case CON_GETINPUT:
        insptr++;
        {
            int const playerNum     = (*insptr++ != sysVarIDs.THISACTOR) ? Gv_GetVarX(insptr[-1]) : vm.playerNum;
            int const labelNum      = *insptr++;
            int const lVar2         = *insptr++;

            if(tw == CON_SETINPUT)
                VM_SetPlayerInput(playerNum, labelNum, Gv_GetVarX(lVar2));
            else
                Gv_SetVarX(lVar2, VM_GetPlayerInput(playerNum, labelNum));

            break;
        }

    case CON_SETTILEDATA:
    case CON_GETTILEDATA:
        insptr++;
        {
            int const tileNum = (*insptr++ != sysVarIDs.THISACTOR) ? Gv_GetVarX(insptr[-1]) : vm.pSprite->picnum;
            int const labelNum = *insptr++;
            int const lVar2 = *insptr++;  

            if(tw == CON_SETTILEDATA)
                VM_SetTileData(tileNum, labelNum, Gv_GetVarX(lVar2));
            else
                Gv_SetVarX(lVar2, VM_GetTileData(tileNum, labelNum));

            break;
        }

    case CON_SETUSERDEF:
    case CON_GETUSERDEF:
        insptr++;
        {
            int const   labelNum    = *insptr++;
            auto const& udLabel     = UserdefsLabels[labelNum];
            int const   lParm2      = (udLabel.flags & LABEL_HASPARM2) ? Gv_GetVarX(*insptr++) : 0;
            int const   lVar2       = *insptr++;

            if (tw == CON_SETUSERDEF)
                VM_SetUserdef(labelNum, lParm2, Gv_GetVarX(lVar2));
            else
                Gv_SetVarX(lVar2, VM_GetUserdef(labelNum, lParm2));
            break;
        }

    case CON_SETPROJECTILE:
    case CON_GETPROJECTILE:
        insptr++;
        {
            int const tileNum   = Gv_GetVarX(*insptr++);
            int const labelNum  = *insptr++;
            int const lVar2     = *insptr++;

            if(tw == CON_SETPROJECTILE)
                VM_SetProjectile(tileNum, labelNum, Gv_GetVarX(lVar2));
            else
                Gv_SetVarX(lVar2, VM_GetProjectile(tileNum, labelNum));

            break;
        }

    case CON_SETWALL:
    case CON_GETWALL:
        insptr++;
        {
            int const wallNum = Gv_GetVarX(*insptr++);
            VM_ABORT_IF((unsigned)wallNum >= MAXWALLS, "invalid wall %d", wallNum);

            int const labelNum  = *insptr++;
            int const lVar2     = *insptr++;

            if(tw == CON_SETWALL)
                VM_SetWall(wallNum, labelNum, Gv_GetVarX(lVar2));
            else
                Gv_SetVarX(lVar2, VM_GetWall(wallNum, labelNum));

            break;
        }

    case CON_SETWALLSTRUCT:
    case CON_GETWALLSTRUCT:
        insptr++;
        {
            int const wallNum = Gv_GetVarX(*insptr++);
            VM_ABORT_IF((unsigned)wallNum >= MAXWALLS, "invalid wall %d", wallNum);

            int const labelNum = *insptr++;
            int const lVar2 = *insptr++;

            auto const& wallLabel = WallLabels[labelNum];

            if (tw == CON_SETWALLSTRUCT)
                VM_SetStruct(wallLabel.flags, (intptr_t*)((char*)&wall[wallNum] + wallLabel.offset), Gv_GetVarX(lVar2));
            else
                Gv_SetVarX(lVar2, VM_GetStruct(wallLabel.flags, (intptr_t*)((char*)&wall[wallNum] + wallLabel.offset)));

            break;
        }

    case CON_SETACTORVAR:
    case CON_GETACTORVAR:
        insptr++;
        {
            int const lSprite = Gv_GetVarX(*insptr++);
            VM_ABORT_IF((unsigned)lSprite >= MAXSPRITES, "invalid sprite %d", lSprite);

            int const lVar1 = *insptr++;
            int const lVar2 = *insptr++;

            if (tw == CON_SETACTORVAR)
                Gv_SetVar(lVar1, Gv_GetVarX(lVar2), lSprite, vm.playerNum);
            else
                Gv_SetVarX(lVar2, Gv_GetVar(lVar1, lSprite, vm.playerNum));

            break;
        }

    case CON_SETPLAYERVAR:
    case CON_GETPLAYERVAR:
        insptr++;
        {
            int const playerNum = (*insptr++ != sysVarIDs.THISACTOR) ? Gv_GetVarX(insptr[-1]) : vm.playerNum;
            VM_ABORT_IF((unsigned)playerNum >= MAXPLAYERS, "invalid player %d", playerNum);

            int const lVar1 = *insptr++;
            int const lVar2 = *insptr++;

            if (tw == CON_SETPLAYERVAR)
                Gv_SetVar(lVar1, Gv_GetVarX(lVar2), vm.spriteNum, playerNum);
            else
                Gv_SetVarX(lVar2, Gv_GetVar(lVar1, vm.spriteNum, playerNum));

            break;
        }

    case CON_SETACTOR:
    case CON_GETACTOR:
        insptr++;
        {
            int const   spriteNum   = (*insptr++ != sysVarIDs.THISACTOR) ? Gv_GetVarX(insptr[-1]) : vm.spriteNum;
            int const   labelNum    = *insptr++;
            auto const& actorLabel  = ActorLabels[labelNum];
            int const   lParm2      = (actorLabel.flags & LABEL_HASPARM2) ? Gv_GetVarX(*insptr++) : 0;
            int const   lVar2       = *insptr++;

            VM_ABORT_IF(((unsigned)spriteNum >= MAXSPRITES) | ((unsigned)lParm2 > (unsigned)actorLabel.maxParm2),
                "%s[%d] invalid for sprite %d", actorLabel.name, lParm2, spriteNum);       

            if (tw == CON_SETACTOR)
                VM_SetSprite(spriteNum, labelNum, lParm2, Gv_GetVarX(lVar2));
            else
                Gv_SetVarX(lVar2, VM_GetSprite(spriteNum, labelNum, lParm2));

            break;
        }

    // For actor[] struct only, not to be confused with the large umbrella of sprite[], actor[], and spriteext[] that CON_GET/SETACTOR covers.
    case CON_SETACTORSTRUCT:
    case CON_GETACTORSTRUCT:
        insptr++;
        {
            int const   spriteNum = (*insptr++ != sysVarIDs.THISACTOR) ? Gv_GetVarX(insptr[-1]) : vm.spriteNum;
            int const   labelNum = *insptr++;
            auto const& actorLabel = ActorLabels[labelNum];
            int const   lVar2 = *insptr++;

            VM_ABORT_IF((unsigned)spriteNum >= MAXSPRITES, "invalid sprite %d", spriteNum);

            if (tw == CON_SETACTORSTRUCT)
                VM_SetStruct(actorLabel.flags, (intptr_t*)((char*)&actor[spriteNum] + actorLabel.offset), Gv_GetVarX(lVar2));
            else
                Gv_SetVarX(lVar2, VM_GetStruct(actorLabel.flags, (intptr_t*)((char*)&actor[spriteNum] + actorLabel.offset)));

            break;
        }
    
    // Same, but for spriteext[].
    case CON_SETSPRITEEXT:
    case CON_GETSPRITEEXT:
        insptr++;
        {
            int const   spriteNum = (*insptr++ != sysVarIDs.THISACTOR) ? Gv_GetVarX(insptr[-1]) : vm.spriteNum;
            int const   labelNum = *insptr++;
            auto const& spriteExtLabel = ActorLabels[labelNum];
            int const   lVar2 = *insptr++;

            VM_ABORT_IF((unsigned)spriteNum >= MAXSPRITES, "invalid sprite %d", spriteNum);

            if (tw == CON_SETSPRITEEXT)
                VM_SetStruct(spriteExtLabel.flags, (intptr_t*)((char*)&spriteext[spriteNum] + spriteExtLabel.offset), Gv_GetVarX(lVar2));
            else
                Gv_SetVarX(lVar2, VM_GetStruct(spriteExtLabel.flags, (intptr_t*)((char*)&spriteext[spriteNum] + spriteExtLabel.offset)));

            break;
        }

    // Lastly, for sprite[].
    case CON_SETSPRITESTRUCT:
    case CON_GETSPRITESTRUCT:
        insptr++;
        {
            int const   spriteNum = (*insptr++ != sysVarIDs.THISACTOR) ? Gv_GetVarX(insptr[-1]) : vm.spriteNum;
            int const   labelNum = *insptr++;
            auto const& spriteLabel = ActorLabels[labelNum];
            int const   lVar2 = *insptr++;

            VM_ABORT_IF((unsigned)spriteNum >= MAXSPRITES, "invalid sprite %d", spriteNum);

            if (tw == CON_SETSPRITESTRUCT)
                VM_SetStruct(spriteLabel.flags, (intptr_t*)((char*)&sprite[spriteNum] + spriteLabel.offset), Gv_GetVarX(lVar2));
            else
                Gv_SetVarX(lVar2, VM_GetStruct(spriteLabel.flags, (intptr_t*)((char*)&sprite[spriteNum] + spriteLabel.offset)));

            break;
        }

    case CON_SETTSPR:
    case CON_GETTSPR:
        insptr++;
        {
            int const spriteNum = (*insptr++ != sysVarIDs.THISACTOR) ? Gv_GetVarX(insptr[-1]) : vm.spriteNum;
            VM_ABORT_IF((unsigned)spriteNum >= MAXSPRITES, "invalid sprite %d", spriteNum);

            int const   labelNum    = *insptr++;
            auto const& tsprLabel   = TsprLabels[labelNum];
            int const   lVar2       = *insptr++;

            if(tw == CON_SETTSPR)
                VM_SetStruct(tsprLabel.flags, (intptr_t*)((char*)spriteext[spriteNum].tspr + tsprLabel.offset), Gv_GetVarX(lVar2));
            else
                Gv_SetVarX(lVar2, VM_GetStruct(tsprLabel.flags, (intptr_t*)((char*)spriteext[spriteNum].tspr + tsprLabel.offset)));

            break;
        }

    case CON_GETANGLETOTARGET:
        insptr++;
        // actor[vm.spriteNum].lastvx and lastvy are last known location of target.
        Gv_SetVarX(*insptr++, getangle(actor[vm.spriteNum].lastvx-vm.pSprite->x,actor[vm.spriteNum].lastvy-vm.pSprite->y));
        break;

    case CON_ANGOFFVAR:
        insptr++;
        spriteext[vm.spriteNum].mdangoff=Gv_GetVarX(*insptr++);
        break;

    case CON_LOCKPLAYER:
        insptr++;
        g_player[vm.playerNum].ps->transporter_hold=Gv_GetVarX(*insptr++);
        break;

    case CON_CHECKAVAILWEAPON:
    case CON_CHECKAVAILINVEN:
        insptr++;
        j = vm.playerNum;

        if (*insptr != sysVarIDs.THISACTOR)
            j=Gv_GetVarX(*insptr);

        insptr++;

        if ((j < 0 || j >= ud.multimode) && g_scriptSanityChecks)
        {
            OSD_Printf(CON_ERROR "Invalid player ID %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),j);
            break;
        }

        if (tw == CON_CHECKAVAILWEAPON)
            P_CheckWeapon(g_player[j].ps);
        else P_SelectNextInvItem(g_player[j].ps);

        break;

    case CON_GETPLAYERANGLE:
        insptr++;
        Gv_SetVarX(*insptr++, fix16_to_int(g_player[vm.playerNum].ps->q16ang));
        break;

    case CON_SETPLAYERANGLE:
        insptr++;
        g_player[vm.playerNum].ps->q16ang=fix16_from_int(Gv_GetVarX(*insptr++) & 2047);
        break;

    case CON_GETACTORANGLE:
        insptr++;
        Gv_SetVarX(*insptr++, vm.pSprite->ang);
        break;

    case CON_SETACTORANGLE:
        insptr++;
        vm.pSprite->ang=Gv_GetVarX(*insptr++);
        vm.pSprite->ang &= 2047;
        break;

    case CON_SETVAR:
        insptr++;
        Gv_SetVarX(*insptr, *(insptr+1));
        insptr += 2;
        break;

    case CON_SETARRAY:
        insptr++;
        {
            int const arrayNum = *insptr++;
            int const arrayIndex = Gv_GetVarX(*insptr++);
            VM_ABORT_IF(((unsigned)arrayNum >= (unsigned)g_gameArrayCount) | ((unsigned)arrayIndex >= (unsigned)aGameArrays[arrayNum & (MAXGAMEARRAYS - 1)].size),
                "invalid array %d or index %d", arrayNum, arrayIndex);

            SetArray(arrayNum, arrayIndex, Gv_GetVarX(*insptr++));
            break;
        }
    case CON_GETARRAYSIZE:
        insptr++;
        {
            int const arrayNum = *insptr++;
            Gv_SetVarX(*insptr++, (aGameArrays[arrayNum].flags & GAMEARRAY_VARSIZE) ? Gv_GetVarX(aGameArrays[arrayNum].size) : aGameArrays[arrayNum].size);
            break;
        }

    case CON_RESIZEARRAY:
        insptr++;
        {
            int const arrayNum = *insptr++;
            int const newSize = Gv_GetVarX(*insptr++);
            ResizeArray(arrayNum, newSize);
            break;
        }

    case CON_RANDVAR:
        insptr++;
        Gv_SetVarX(*insptr, mulscale16(krand(), *(insptr+1)+1));
        insptr += 2;
        break;

    case CON_DISPLAYRANDVAR:
        insptr++;
        Gv_SetVarX(*insptr, mulscale15(rand(), *(insptr+1)+1));
        insptr += 2;
        break;

    case CON_MULVAR:
        Gv_MulVar(insptr[1], insptr[2]);
        insptr += 3;
        break;

    case CON_DIVVAR:
        if (insptr[2] == 0)
        {
            OSD_Printf(CON_ERROR "Divide by zero.\n",g_errorLineNum,VM_GetKeywordForID(g_tw));
            insptr += 3;
            break;
        }

        Gv_DivVar(insptr[1], insptr[2]);
        insptr += 3;
        break;

    case CON_MODVAR:
        if (insptr[2] == 0)
        {
            OSD_Printf(CON_ERROR "Mod by zero.\n",g_errorLineNum,VM_GetKeywordForID(g_tw));
            insptr += 3;
            break;
        }

        Gv_ModVar(insptr[1], insptr[2]);
        insptr += 3;
        break;

    case CON_ANDVAR:
        Gv_AndVar(insptr[1], insptr[2]);
        insptr += 3;
        break;

    case CON_ORVAR:
        Gv_OrVar(insptr[1], insptr[2]);
        insptr += 3;
        break;

    case CON_XORVAR:
        Gv_XorVar(insptr[1], insptr[2]);
        insptr += 3;
        break;

    case CON_SETVARVAR:
        insptr++;
        {
            j = *insptr++;
            int const nValue = Gv_GetVarX(*insptr++);

            if ((aGameVars[j].flags & (GAMEVAR_USER_MASK | GAMEVAR_PTR_MASK)) == 0)
                aGameVars[j].lValue = nValue;
            else
                Gv_SetVarX(j, nValue);
        }
        break;

    case CON_RANDVARVAR:
        insptr++;
        j = *insptr++;
        Gv_SetVarX(j, mulscale16(krand(), Gv_GetVarX(*insptr++) + 1));
        break;

    case CON_DISPLAYRANDVARVAR:
        insptr++;
        j=*insptr++;
        Gv_SetVarX(j,mulscale(rand(), Gv_GetVarX(*insptr++)+1, 15));
        break;

    case CON_INV:
        if ((aGameVars[insptr[1]].flags & (GAMEVAR_USER_MASK | GAMEVAR_PTR_MASK)) == 0)
            aGameVars[insptr[1]].lValue = -aGameVars[insptr[1]].lValue;
        else
            Gv_SetVarX(insptr[1], -Gv_GetVarX(insptr[1]));
        insptr += 2;
        break;

    case CON_GMAXAMMO:
        insptr++;
        j=Gv_GetVarX(*insptr++);
        if ((j<0 || j>=MAX_WEAPONS) && g_scriptSanityChecks)
        {
            OSD_Printf(CON_ERROR "Invalid weapon ID %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),j);
            insptr++;
            break;
        }
        Gv_SetVarX(*insptr++, g_player[vm.playerNum].ps->max_ammo_amount[j]);
        break;

    case CON_SMAXAMMO:
        insptr++;
        j=Gv_GetVarX(*insptr++);
        if ((j<0 || j>=MAX_WEAPONS) && g_scriptSanityChecks)
        {
            OSD_Printf(CON_ERROR "Invalid weapon ID %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),j);
            insptr++;
            break;
        }
        g_player[vm.playerNum].ps->max_ammo_amount[j]=Gv_GetVarX(*insptr++);
        break;

    case CON_MULVARVAR:
        insptr++;
        j = *insptr++;
        Gv_MulVar(j, Gv_GetVarX(*insptr++));
        break;

    case CON_DIVVARVAR:
        insptr++;
        {
            j = *insptr++;

            int const nValue = Gv_GetVarX(*insptr++);

            VM_ABORT_IF(!nValue, "divide by zero!");

            Gv_DivVar(j, nValue);
            break;
        }

    case CON_MODVARVAR:
        insptr++;
        {
            j = *insptr++;

            int const nValue = Gv_GetVarX(*insptr++);

            VM_ABORT_IF(!nValue, "mod by zero!");

            Gv_ModVar(j, nValue);
            break;
        }

    case CON_ANDVARVAR:
        insptr++;
        j = *insptr++;
        Gv_AndVar(j, Gv_GetVarX(*insptr++));
        break;

    case CON_XORVARVAR:
        insptr++;
        j = *insptr++;
        Gv_XorVar(j, Gv_GetVarX(*insptr++));
        break;

    case CON_ORVARVAR:
        insptr++;
        j = *insptr++;
        Gv_OrVar(j, Gv_GetVarX(*insptr++));
        break;

    case CON_SUBVAR:
        Gv_SubVar(insptr[1], insptr[2]);
        insptr += 3;
        break;

    case CON_SUBVARVAR:
        insptr++;
        j = *insptr++;
        Gv_SubVar(j, Gv_GetVarX(*insptr++));
        break;

    case CON_ADDVAR:
        Gv_AddVar(insptr[1], insptr[2]);
        insptr += 3;
        break;

    case CON_SHIFTVARL:
        Gv_ShiftVarL(insptr[1], insptr[2]);
        insptr += 3;
        break;

    case CON_SHIFTVARR:
        Gv_ShiftVarR(insptr[1], insptr[2]);
        insptr += 3;
        break;

    case CON_SHIFTVARVARL:
        insptr++;
        j = *insptr++;
        Gv_ShiftVarL(j, Gv_GetVarX(*insptr++));
        break;

    case CON_SHIFTVARVARR:
        insptr++;
        j = *insptr++;
        Gv_ShiftVarR(j, Gv_GetVarX(*insptr++));
        break;

    case CON_SIN:
        insptr++;
        j = *insptr++;
        Gv_SetVarX(j, sintable[Gv_GetVarX(*insptr++) & 2047]);
        break;

    case CON_COS:
        insptr++;
        j = *insptr++;
        Gv_SetVarX(j, sintable[(Gv_GetVarX(*insptr++) + 512) & 2047]);
        break;

    case CON_KLABS:
        Gv_SetVarX(insptr[1], klabs(Gv_GetVarX(insptr[1])));
        insptr += 2;
        break;

    case CON_ADDVARVAR:
        insptr++;
        j = *insptr++;
        Gv_AddVar(j, Gv_GetVarX(*insptr++));
        break;

    case CON_SPGETLOTAG:
        insptr++;
        Gv_SetVarX(sysVarIDs.LOTAG, vm.pSprite->lotag);
        break;

    case CON_SPGETHITAG:
        insptr++;
        Gv_SetVarX(sysVarIDs.HITAG, vm.pSprite->hitag);
        break;

    case CON_SECTGETLOTAG:
        insptr++;
        Gv_SetVarX(sysVarIDs.LOTAG, sector[vm.pSprite->sectnum].lotag);
        break;

    case CON_SECTGETHITAG:
        insptr++;
        Gv_SetVarX(sysVarIDs.HITAG, sector[vm.pSprite->sectnum].hitag);
        break;

    case CON_GETTEXTUREFLOOR:
        insptr++;
        Gv_SetVarX(sysVarIDs.TEXTURE, sector[vm.pSprite->sectnum].floorpicnum);
        break;

    case CON_STARTTRACK:
        insptr++;
        G_StartTrackSlotWrap(ud.volume_number, Gv_GetVarX(*(insptr++)));
        break;

    case CON_ACTIVATECHEAT:
        insptr++;
        j=Gv_GetVarX(*(insptr++));
        if (numplayers != 1 || !(ud.gm & MODE_GAME))
        {
            OSD_Printf(CON_ERROR "not in a single-player game.\n",g_errorLineNum,VM_GetKeywordForID(g_tw));
            break;
        }
        osdcmd_cheatsinfo_stat.cheatnum = j;
        break;

    case CON_SETGAMEPALETTE:
        insptr++;
        P_SetGamePalette(g_player[vm.playerNum].ps, Gv_GetVarX(*(insptr++)), 0);
        break;

    case CON_GETTEXTURECEILING:
        insptr++;
        Gv_SetVarX(sysVarIDs.TEXTURE, sector[vm.pSprite->sectnum].ceilingpicnum);
        break;

    case CON_IFVARVARAND:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        j = (j & Gv_GetVarX(*insptr++));
        insptr--;
        branch(j);
        break;

    case CON_IFVARVAROR:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        j = (j | Gv_GetVarX(*insptr++));
        insptr--;
        branch(j);
        break;

    case CON_IFVARVARXOR:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        j = (j ^ Gv_GetVarX(*insptr++));
        insptr--;
        branch(j);
        break;

    case CON_IFVARVAREITHER:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        j = (j || Gv_GetVarX(*insptr++));
        insptr--;
        branch(j);
        break;

    case CON_IFVARVARN:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        j = (j != Gv_GetVarX(*insptr++));
        insptr--;
        branch(j);
        break;

    case CON_IFVARVARE:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        j = (j == Gv_GetVarX(*insptr++));
        insptr--;
        branch(j);
        break;

    case CON_IFVARVARG:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        j = (j > Gv_GetVarX(*insptr++));
        insptr--;
        branch(j);
        break;

    case CON_IFVARVARGE:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        j = (j >= Gv_GetVarX(*insptr++));
        insptr--;
        branch(j);
        break;

    case CON_IFVARVARL:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        j = (j < Gv_GetVarX(*insptr++));
        insptr--;
        branch(j);
        break;

    case CON_IFVARVARLE:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        j = (j <= Gv_GetVarX(*insptr++));
        insptr--;
        branch(j);
        break;

    case CON_IFVARE:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        branch(j == *insptr);
        break;

    case CON_IFVARN:
        insptr++;
        j=*insptr++;
        branch(Gv_GetVarX(j) != *insptr);
        break;

    case CON_WHILEVARN:
    {
        auto const savedinsptr = &insptr[2];
        do
        {
            insptr = savedinsptr;
            branch((j = (Gv_GetVarX(insptr[-1]) != *insptr)));
        } while (j && (vm.flags & VM_RETURN) == 0);
        break;
    }

    case CON_WHILEVARVARN:
    {
        auto const savedinsptr = &insptr[2];
        do
        {
            insptr = savedinsptr;
            j = Gv_GetVarX(insptr[-1]);
            j = (j != Gv_GetVarX(*insptr++));
            insptr--;
            branch(j);
        } while (j && (vm.flags & VM_RETURN) == 0);
        break;
    }

    case CON_IFVARAND:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        branch(j & *insptr);
        break;

    case CON_IFVAROR:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        branch(j | *insptr);
        break;

    case CON_IFVARXOR:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        branch(j ^ *insptr);
        break;

    case CON_IFVAREITHER:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        branch(j || *insptr);
        break;

    case CON_IFVARG:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        branch(j > *insptr);
        break;

    case CON_IFVARGE:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        branch(j >= *insptr);
        break;

    case CON_IFVARL:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        branch(j < *insptr);
        break;

    case CON_IFVARLE:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        branch(j <= *insptr);
        break;

    case CON_IFPHEALTHL:
        insptr++;
        branch(sprite[g_player[vm.playerNum].ps->i].extra < *insptr);
        break;

    case CON_IFPINVENTORY:
    {
        insptr++;
        j = 0;
        switch (*insptr++)
        {
        case GET_STEROIDS:
            if (g_player[vm.playerNum].ps->inv_amount[GET_STEROIDS] != *insptr)
                j = 1;
            break;
        case GET_SHIELD:
            if (g_player[vm.playerNum].ps->inv_amount[GET_SHIELD] != g_player[vm.playerNum].ps->max_shield_amount)
                j = 1;
            break;
        case GET_SCUBA:
            if (g_player[vm.playerNum].ps->inv_amount[GET_SCUBA] != *insptr) j = 1;
            break;
        case GET_HOLODUKE:
            if (g_player[vm.playerNum].ps->inv_amount[GET_HOLODUKE] != *insptr) j = 1;
            break;
        case GET_JETPACK:
            if (g_player[vm.playerNum].ps->inv_amount[GET_JETPACK] != *insptr) j = 1;
            break;
        case GET_ACCESS:
            switch (vm.pSprite->pal)
            {
            case  0:
                if (ud.got_access&1) j = 1;
                break;
            case 21:
                if (ud.got_access&2) j = 1;
                break;
            case 23:
                if (ud.got_access&4) j = 1;
                break;
            }
            break;
        case GET_HEATS:
            if (g_player[vm.playerNum].ps->inv_amount[GET_HEATS] != *insptr) j = 1;
            break;
        case GET_FIRSTAID:
            if (g_player[vm.playerNum].ps->inv_amount[GET_FIRSTAID] != *insptr) j = 1;
            break;
        case GET_BOOTS:
            if (g_player[vm.playerNum].ps->inv_amount[GET_BOOTS] != *insptr) j = 1;
            break;
        default:
            OSD_Printf(CON_ERROR "invalid inventory ID: %d\n", g_errorLineNum, VM_GetKeywordForID(g_tw), (int)*(insptr-1));
        }

        branch(j);
        break;
    }

    case CON_PSTOMP:
    {
        insptr++;
        if (g_player[vm.playerNum].ps->knee_incs == 0 && sprite[g_player[vm.playerNum].ps->i].xrepeat >= 40)
        {
            if (cansee(vm.pSprite->x, vm.pSprite->y, vm.pSprite->z - (4 << 8), vm.pSprite->sectnum, g_player[vm.playerNum].ps->pos.x, g_player[vm.playerNum].ps->pos.y, g_player[vm.playerNum].ps->pos.z + (16 << 8), sprite[g_player[vm.playerNum].ps->i].sectnum))
            {
                for (j = MAXPLAYERS - 1; j >= 0; j--) // Anyone already stomping this guy?
                {
                    if (g_player[j].ps == NULL || !g_player[j].connected)
                        continue;

                    if (g_player[j].ps->actorsqu == vm.spriteNum) // Yes. Don't try.
                        break;
                }

                if (j == -1)
                {
                    g_player[vm.playerNum].ps->knee_incs = 1;

                    if (g_player[vm.playerNum].ps->weapon_pos == 0)
                        g_player[vm.playerNum].ps->weapon_pos = -1;

                    g_player[vm.playerNum].ps->actorsqu = vm.spriteNum;
                }
            }
        }
        break;
    }

    case CON_IFAWAYFROMWALL:
    {
        short s1=vm.pSprite->sectnum;

        j = 0;

        updatesector(vm.pSprite->x+108,vm.pSprite->y+108,&s1);
        if (s1 == vm.pSprite->sectnum)
        {
            updatesector(vm.pSprite->x-108,vm.pSprite->y-108,&s1);
            if (s1 == vm.pSprite->sectnum)
            {
                updatesector(vm.pSprite->x+108,vm.pSprite->y-108,&s1);
                if (s1 == vm.pSprite->sectnum)
                {
                    updatesector(vm.pSprite->x-108,vm.pSprite->y+108,&s1);
                    if (s1 == vm.pSprite->sectnum)
                        j = 1;
                }
            }
        }
        branch(j);
    }
    break;

    case CON_QUOTE:
        insptr++;

        if ((apStrings[*insptr] == NULL) && g_scriptSanityChecks)
        {
            OSD_Printf(CON_ERROR "null quote %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),(int)*insptr);
            insptr++;
            break;
        }

        if ((vm.playerNum < 0 || vm.playerNum >= MAXPLAYERS) && g_scriptSanityChecks)
        {
            OSD_Printf(CON_ERROR "bad player for quote %d: (%d)\n",g_errorLineNum,VM_GetKeywordForID(g_tw),(int)*insptr, vm.playerNum);
            insptr++;
            break;
        }

        P_DoQuote(*(insptr++)|MAXQUOTES,g_player[vm.playerNum].ps);
        break;

    case CON_USERQUOTE:
        insptr++;
        {
            int i=Gv_GetVarX(*insptr++);

            if ((apStrings[i] == NULL) && g_scriptSanityChecks)
            {
                OSD_Printf(CON_ERROR "null quote %d\n",g_errorLineNum,VM_GetKeywordForID(g_tw),i);
                break;
            }
            G_AddUserQuote(apStrings[i]);
        }
        break;

    case CON_IFINOUTERSPACE:
        branch(G_CheckForSpaceFloor(vm.pSprite->sectnum));
        break;

    case CON_IFNOTMOVING:
        branch((actor[vm.spriteNum].movflag&49152) > 16384);
        break;

    case CON_RESPAWNHITAG:
        insptr++;
        if (G_CheckFemSprite(vm.spriteNum))
        {
            if (vm.pSprite->yvel)
                G_OperateRespawns(vm.pSprite->yvel);
        }
        else
        {
            if (vm.pSprite->hitag >= 0)
                G_OperateRespawns(vm.pSprite->hitag);
        }
        break;

    case CON_IFSPRITEPAL:
        insptr++;
        branch(vm.pSprite->pal == *insptr);
        break;

    case CON_IFANGDIFFL:
        insptr++;
        j = klabs(G_GetAngleDelta(fix16_to_int(g_player[vm.playerNum].ps->q16ang),vm.pSprite->ang));
        branch(j <= *insptr);
        break;

    case CON_IFNOSOUNDS:
        branch(!A_CheckAnySoundPlaying(vm.spriteNum));
        break;

    case CON_SPRITEFLAGS:
        insptr++;
        actor[vm.spriteNum].flags = Gv_GetVarX(*insptr++);
        break;

    case CON_GETTICKS:
        insptr++;
        j=*insptr++;
        Gv_SetVarX(j, timerGetTicks());
        break;

    case CON_GETCURRADDRESS:
        insptr++;
        j=*insptr++;
        Gv_SetVarX(j, (intptr_t)(insptr-apScript));
        break;

    case CON_JUMP:
        insptr++;
        j = Gv_GetVarX(*insptr++);
        insptr = (intptr_t *)(j+apScript);
        break;

    default:
        /*        OSD_Printf("fatal error: default processing: previous five values: %d, %d, %d, %d, %d, "
                           "current opcode: %d, next five values: %d, %d, %d, %d, %d\ncurrent actor: %d (%d)\n",
                           *(insptr-5),*(insptr-4),*(insptr-3),*(insptr-2),*(insptr-1),*insptr,*(insptr+1),
                           *(insptr+2),*(insptr+3),*(insptr+4),*(insptr+5),vm.spriteNum,vm.pSprite->picnum);
                OSD_Printf("g_errorLineNum: %d, g_tw: %d\n",g_errorLineNum,g_tw);*/
        VM_ScriptInfo(insptr, 64);

        VLOG_F(LOG_VM, "unknown opcode: %d", tw);

        G_GameExit("An error has occurred in the EDuke32 CON executor.\n\n"
                   "If you are an end user, please e-mail the file eduke32.log\n"
                   "along with links to any mods you're using to terminx@gmail.com.\n\n"
                   "If you are a mod developer, please attach all of your CON files\n"
                   "along with instructions on how to reproduce this error.\n\n"
                   "Thank you!");
        break;
    }
    return 0;
}

void A_LoadActor(int iActor)
{
    vm.spriteNum = iActor;    // Sprite ID
    vm.playerNum = -1; // playerNum;    // Player ID
    vm.playerDist = -1; // playerDist;    // ??
    vm.pSprite = &sprite[vm.spriteNum];    // Pointer to sprite structure
    vm.pData = &actor[vm.spriteNum].t_data[0];   // Sprite's 'extra' data

    if (g_tile[vm.pSprite->picnum].loadPtr == 0) return;

    insptr = g_tile[vm.pSprite->picnum].loadPtr;

    vm.flags &= ~(VM_RETURN | VM_KILL | VM_NOEXECUTE);

    if (vm.pSprite->sectnum < 0 || vm.pSprite->sectnum >= MAXSECTORS)
    {
        //      if(A_CheckEnemySprite(vm.pSprite))
        //          g_player[vm.playerNum].ps->actors_killed++;
        A_DeleteSprite(vm.spriteNum);
        return;
    }

    while (!X_DoExecute());

    if (vm.flags & VM_KILL)
        A_DeleteSprite(vm.spriteNum);
}

void VM_UpdateAnim(int const spriteNum, int32_t* const pData)
{
    size_t const actionofs = AC_ACTION_ID(pData);
    auto const   actionptr = &apScript[actionofs];

    if ((actionofs == 0) | (actionofs + (ACTION_PARAM_COUNT - 1) >= (unsigned)g_scriptSize))
        return;

    int const action_frames = actionptr[ACTION_NUMFRAMES];
    int const action_incval = actionptr[ACTION_INCVAL];
    int const action_delay = actionptr[ACTION_DELAY];
    auto actionticsptr = &AC_ACTIONTICS(&sprite[spriteNum], &actor[spriteNum]);
    *actionticsptr += TICSPERFRAME;

    if (*actionticsptr > action_delay)
    {
        *actionticsptr = 0;
        AC_ACTION_COUNT(pData)++;
        AC_CURFRAME(pData) += action_incval;
    }

    if (klabs(AC_CURFRAME(pData)) >= klabs(action_frames * action_incval))
        AC_CURFRAME(pData) = 0;
}

void A_Execute(int spriteNum, int playerNum, int playerDist)
{
    // for some reason this is faster than using the C++ syntax; e.g vm = vmstate_t{ ... }
    vmstate_t const tempvm
        = { spriteNum, playerNum, playerDist, 0, &sprite[spriteNum], &actor[spriteNum].t_data[0], g_player[playerNum].ps, &actor[spriteNum] };
    vm = tempvm;

    int const picnum = vm.pSprite->picnum;

    //MICROPROFILE_SCOPE_TOKEN(g_actorTokens[picnum]);

    if (g_tileLabels[picnum])
        ERROR_CONTEXT("actor", &g_tileLabels[picnum][0]);
    else
        ERROR_CONTEXT("actor", picnum);

    ERROR_CONTEXT("spritenum", spriteNum);

    if (vm.pSprite->sectnum < 0 || vm.pSprite->sectnum >= MAXSECTORS)
    {
        if (A_CheckEnemySprite(vm.pSprite))
            G_AddMonsterKills(vm.spriteNum, 1);
        A_DeleteSprite(vm.spriteNum);
        return;
    }

    VM_UpdateAnim(vm.spriteNum, vm.pData);

    vm.flags = 0;

    insptr = 4 + (g_tile[vm.pSprite->picnum].execPtr);
    while (!X_DoExecute());
    insptr = NULL;

    if (vm.flags & VM_KILL)
    {
        // if player was set to squish, first stop that...
        if (g_player[vm.playerNum].ps->actorsqu == vm.spriteNum)
            g_player[vm.playerNum].ps->actorsqu = -1;
        A_DeleteSprite(vm.spriteNum);
        return;
    }

    X_Move();

    if (vm.pSprite->statnum == STAT_STANDABLE)
        switch (tileGetMapping(vm.pSprite->picnum))
        {
        case RUBBERCAN__:
        case EXPLODINGBARREL__:
        case WOODENHORSE__:
        case HORSEONSIDE__:
        case CANWITHSOMETHING__:
        case FIREBARREL__:
        case NUKEBARREL__:
        case NUKEBARRELDENTED__:
        case NUKEBARRELLEAKED__:
        case TRIPBOMB__:
        case EGG__:
            if (actor[vm.spriteNum].timetosleep > 1)
                actor[vm.spriteNum].timetosleep--;
            else if (actor[vm.spriteNum].timetosleep == 1)
                changespritestat(vm.spriteNum, STAT_ZOMBIEACTOR);
        default:
            return;
        }

    if (vm.pSprite->statnum != STAT_ACTOR)
        return;

    if (A_CheckEnemySprite(vm.pSprite))
    {
        if (vm.pSprite->xrepeat > 60) return;
        if (DMFLAGS_TEST(DMFLAG_RESPAWNMONSTERS) && vm.pSprite->extra <= 0) return;
    }
    else if (DMFLAGS_TEST(DMFLAG_RESPAWNITEMS) && (vm.pSprite->cstat&32768)) return;

    if (actor[vm.spriteNum].timetosleep > 1)
        actor[vm.spriteNum].timetosleep--;
    else if (actor[vm.spriteNum].timetosleep == 1)
    {
#ifndef EDUKE32_STANDALONE
        if (!FURY &&
            EDUKE32_PREDICT_FALSE(g_scriptVersion == 13 &&
                                  ((vm.pSprite->picnum == FIRE) || (vm.pSprite->picnum == FIRE2))))
            return;
#endif
        changespritestat(vm.spriteNum, STAT_ZOMBIEACTOR);
    }
}

void G_SaveMapState(mapstate_t *save)
{
    if (save != NULL)
    {
        int i;
        intptr_t j;

        Bmemcpy(&save->numwalls,&numwalls,sizeof(numwalls));
        Bmemcpy(&save->wall[0],&wall[0],sizeof(walltype)*MAXWALLS);
        Bmemcpy(&save->numsectors,&numsectors,sizeof(numsectors));
        Bmemcpy(&save->sector[0],&sector[0],sizeof(sectortype)*MAXSECTORS);
        Bmemcpy(&save->sprite[0],&sprite[0],sizeof(spritetype)*MAXSPRITES);
        Bmemcpy(&save->spriteext[0],&spriteext[0],sizeof(spriteext_t)*MAXSPRITES);

        save->numsprites = Numsprites;
        save->tailspritefree = tailspritefree;
        Bmemcpy(&save->headspritesect[0],&headspritesect[0],sizeof(headspritesect));
        Bmemcpy(&save->prevspritesect[0],&prevspritesect[0],sizeof(prevspritesect));
        Bmemcpy(&save->nextspritesect[0],&nextspritesect[0],sizeof(nextspritesect));
        Bmemcpy(&save->headspritestat[STAT_DEFAULT],&headspritestat[STAT_DEFAULT],sizeof(headspritestat));
        Bmemcpy(&save->prevspritestat[STAT_DEFAULT],&prevspritestat[STAT_DEFAULT],sizeof(prevspritestat));
        Bmemcpy(&save->nextspritestat[STAT_DEFAULT],&nextspritestat[STAT_DEFAULT],sizeof(nextspritestat));

        for (i=MAXSPRITES-1;i>=0;i--)
        {
            save->scriptptrs[i] = 0;

            if (g_tile[PN(i)].execPtr == 0) continue;

            j = (intptr_t)&apScript[0];

            if (T2(i) >= j && T2(i) < (intptr_t)(&apScript[g_scriptSize]))
            {
                save->scriptptrs[i] |= 1;
                T2(i) -= j;
            }
            if (T5(i) >= j && T5(i) < (intptr_t)(&apScript[g_scriptSize]))
            {
                save->scriptptrs[i] |= 2;
                T5(i) -= j;
            }
            if (T6(i) >= j && T6(i) < (intptr_t)(&apScript[g_scriptSize]))
            {
                save->scriptptrs[i] |= 4;
                T6(i) -= j;
            }
        }

        Bmemcpy(&save->actor[0],&actor[0],sizeof(ActorData_t)*MAXSPRITES);

        for (i=MAXSPRITES-1;i>=0;i--)
        {
            if (g_tile[PN(i)].execPtr == 0) continue;
            j = (intptr_t)&apScript[0];

            if (save->scriptptrs[i]&1)
                T2(i) += j;
            if (save->scriptptrs[i]&2)
                T5(i) += j;
            if (save->scriptptrs[i]&4)
                T6(i) += j;
        }

        Bmemcpy(&save->g_numCyclers,&g_numCyclers,sizeof(g_numCyclers));
        Bmemcpy(&save->cyclers[0][0],&cyclers[0][0],sizeof(cyclers));
        Bmemcpy(&save->g_playerSpawnPoints[0],&g_playerSpawnPoints[0],sizeof(g_playerSpawnPoints));
        Bmemcpy(&save->g_numAnimWalls,&g_numAnimWalls,sizeof(g_numAnimWalls));
        Bmemcpy(&save->SpriteDeletionQueue[0],&SpriteDeletionQueue[0],sizeof(SpriteDeletionQueue));
        Bmemcpy(&save->g_spriteDeleteQueuePos,&g_spriteDeleteQueuePos,sizeof(g_spriteDeleteQueuePos));
        Bmemcpy(&save->animwall[0],&animwall[0],sizeof(animwall));
        Bmemcpy(&save->origins[0],&g_origins[0],sizeof(g_origins));
        Bmemcpy(&save->g_mirrorWall[0],&g_mirrorWall[0],sizeof(g_mirrorWall));
        Bmemcpy(&save->g_mirrorSector[0],&g_mirrorSector[0],sizeof(g_mirrorSector));
        Bmemcpy(&save->g_mirrorCount,&g_mirrorCount,sizeof(g_mirrorCount));
        Bmemcpy(&save->show2dsector[0],&show2dsector[0],sizeof(show2dsector));
        Bmemcpy(&save->g_numClouds,&g_numClouds,sizeof(g_numClouds));
        Bmemcpy(&save->g_cloudSect[0],&g_cloudSect[0],sizeof(g_cloudSect));
        Bmemcpy(&save->g_cloudX,&g_cloudX,sizeof(g_cloudX));
        Bmemcpy(&save->g_cloudY,&g_cloudY,sizeof(g_cloudY));

        // OLDMP FIXME
        Bmemcpy(&save->g_pskyidx, &g_pskyidx, sizeof(g_pskyidx));
        //Bmemcpy(&save->pskyoff[0],&pskyoff[0],sizeof(pskyoff));
        //Bmemcpy(&save->pskybits,&pskybits,sizeof(pskybits));

        Bmemcpy(&save->g_animateCount, &g_animateCount, sizeof(g_animateCount));
        Bmemcpy(&save->sectorAnims[0], &sectorAnims[0], sizeof(sectorAnims));

        Bmemcpy(&save->g_numPlayerSprites,&g_numPlayerSprites,sizeof(g_numPlayerSprites));
        Bmemcpy(&save->g_earthquakeTime,&g_earthquakeTime,sizeof(g_earthquakeTime));
        save->lockclock = lockclock;
        Bmemcpy(&save->randomseed,&randomseed,sizeof(randomseed));
        Bmemcpy(&save->g_globalRandom,&g_globalRandom,sizeof(g_globalRandom));

        for (i=g_gameVarCount-1; i>=0;i--)
        {
            if (aGameVars[i].flags & GAMEVAR_NORESET) continue;
            if (aGameVars[i].flags & GAMEVAR_PERPLAYER)
            {
                if (!save->vars[i])
                    save->vars[i] = (intptr_t*)Xcalloc(MAXPLAYERS,sizeof(intptr_t));
                Bmemcpy(&save->vars[i][0],&aGameVars[i].pValues[0],sizeof(intptr_t) * MAXPLAYERS);
            }
            else if (aGameVars[i].flags & GAMEVAR_PERACTOR)
            {
                if (!save->vars[i])
                    save->vars[i] = (intptr_t*)Xcalloc(MAXSPRITES,sizeof(intptr_t));
                Bmemcpy(&save->vars[i][0],&aGameVars[i].pValues[0],sizeof(intptr_t) * MAXSPRITES);
            }
            else save->vars[i] = (intptr_t *)aGameVars[i].lValue;
        }

        ototalclock = (int32_t)totalclock;
    }
}

extern void Gv_RefreshPointers(void);

void G_RestoreMapState(mapstate_t *save)
{
    if (save != NULL)
    {
        int i, k, x;
        intptr_t j;
        char phealth[MAXPLAYERS];

        for (ALL_PLAYERS(i))
            phealth[i] = sprite[g_player[i].ps->i].extra;

        pub = NUMPAGES;
        pus = NUMPAGES;
        G_UpdateScreenArea();

        Bmemcpy(&numwalls,&save->numwalls,sizeof(numwalls));
        Bmemcpy(&wall[0],&save->wall[0],sizeof(walltype)*MAXWALLS);
        Bmemcpy(&numsectors,&save->numsectors,sizeof(numsectors));
        Bmemcpy(&sector[0],&save->sector[0],sizeof(sectortype)*MAXSECTORS);
        Bmemcpy(&sprite[0],&save->sprite[0],sizeof(spritetype)*MAXSPRITES);
        Bmemcpy(&spriteext[0],&save->spriteext[0],sizeof(spriteext_t)*MAXSPRITES);

        Numsprites = save->numsprites;
        tailspritefree = save->tailspritefree;
        Bmemcpy(&headspritesect[0],&save->headspritesect[0],sizeof(headspritesect));
        Bmemcpy(&prevspritesect[0],&save->prevspritesect[0],sizeof(prevspritesect));
        Bmemcpy(&nextspritesect[0],&save->nextspritesect[0],sizeof(nextspritesect));
        Bmemcpy(&headspritestat[STAT_DEFAULT],&save->headspritestat[STAT_DEFAULT],sizeof(headspritestat));
        Bmemcpy(&prevspritestat[STAT_DEFAULT],&save->prevspritestat[STAT_DEFAULT],sizeof(prevspritestat));
        Bmemcpy(&nextspritestat[STAT_DEFAULT],&save->nextspritestat[STAT_DEFAULT],sizeof(nextspritestat));
        Bmemcpy(&actor[0],&save->actor[0],sizeof(ActorData_t)*MAXSPRITES);

        for (i=MAXSPRITES-1;i>=0;i--)
        {
            j = (intptr_t)(&apScript[0]);
            if (save->scriptptrs[i]&1) T2(i) += j;
            if (save->scriptptrs[i]&2) T5(i) += j;
            if (save->scriptptrs[i]&4) T6(i) += j;
        }

        Bmemcpy(&g_numCyclers,&save->g_numCyclers,sizeof(g_numCyclers));
        Bmemcpy(&cyclers[0][0],&save->cyclers[0][0],sizeof(cyclers));
        Bmemcpy(&g_playerSpawnPoints[0],&save->g_playerSpawnPoints[0],sizeof(g_playerSpawnPoints));
        Bmemcpy(&g_numAnimWalls,&save->g_numAnimWalls,sizeof(g_numAnimWalls));
        Bmemcpy(&SpriteDeletionQueue[0],&save->SpriteDeletionQueue[0],sizeof(SpriteDeletionQueue));
        Bmemcpy(&g_spriteDeleteQueuePos,&save->g_spriteDeleteQueuePos,sizeof(g_spriteDeleteQueuePos));
        Bmemcpy(&animwall[0],&save->animwall[0],sizeof(animwall));
        Bmemcpy(&g_origins[0],&save->origins[0],sizeof(g_origins));

        Bmemcpy(&g_mirrorWall[0],&save->g_mirrorWall[0],sizeof(g_mirrorWall));
        Bmemcpy(&g_mirrorSector[0],&save->g_mirrorSector[0],sizeof(g_mirrorSector));
        Bmemcpy(&g_mirrorCount,&save->g_mirrorCount,sizeof(g_mirrorCount));
        Bmemcpy(&show2dsector[0],&save->show2dsector[0],sizeof(show2dsector));
        Bmemcpy(&g_numClouds,&save->g_numClouds,sizeof(g_numClouds));
        Bmemcpy(&g_cloudSect[0],&save->g_cloudSect[0],sizeof(g_cloudSect));
        Bmemcpy(&g_cloudX,&save->g_cloudX,sizeof(g_cloudX));
        Bmemcpy(&g_cloudY,&save->g_cloudY,sizeof(g_cloudY));

        // OLDMP FIXME
        Bmemcpy(&g_pskyidx, &save->g_pskyidx, sizeof(g_pskyidx));
        //Bmemcpy(&pskyoff[0],&save->pskyoff[0],sizeof(pskyoff));
        //Bmemcpy(&pskybits,&save->pskybits,sizeof(pskybits));

        Bmemcpy(&g_animateCount, &save->g_animateCount, sizeof(g_animateCount));
        Bmemcpy(&sectorAnims[0], &save->sectorAnims[0], sizeof(sectorAnims));

        Bmemcpy(&g_numPlayerSprites,&save->g_numPlayerSprites,sizeof(g_numPlayerSprites));
        Bmemcpy(&g_earthquakeTime,&save->g_earthquakeTime,sizeof(g_earthquakeTime));

        lockclock = save->lockclock;

        Bmemcpy(&randomseed,&save->randomseed,sizeof(randomseed));
        Bmemcpy(&g_globalRandom,&save->g_globalRandom,sizeof(g_globalRandom));

        for (i=g_gameVarCount-1;i>=0;i--)
        {
            if (aGameVars[i].flags & GAMEVAR_NORESET) continue;
            if (aGameVars[i].flags & GAMEVAR_PERPLAYER)
                Bmemcpy(&aGameVars[i].pValues[0],&save->vars[i][0],sizeof(intptr_t) * MAXPLAYERS);
            else if (aGameVars[i].flags & GAMEVAR_PERACTOR)
                Bmemcpy(&aGameVars[i].pValues[0],&save->vars[i][0],sizeof(intptr_t) * MAXSPRITES);
            else aGameVars[i].lValue = (intptr_t)save->vars[i];
        }

        Gv_RefreshPointers();

        for (ALL_PLAYERS(i))
            sprite[g_player[i].ps->i].extra = phealth[i];

        if (g_player[myconnectindex].ps->over_shoulder_on != 0)
        {
            g_cameraDistance = 0;
            g_cameraClock = 0;
            g_player[myconnectindex].ps->over_shoulder_on = 1;
        }

        screenpeek = myconnectindex;

        if (ud.lockout == 0)
        {
            for (x=g_numAnimWalls-1;x>=0;x--)
                if (wall[animwall[x].wallnum].extra >= 0)
                    wall[animwall[x].wallnum].picnum = wall[animwall[x].wallnum].extra;
        }
        else
        {
            for (x=g_numAnimWalls-1;x>=0;x--)
                switch (tileGetMapping(wall[animwall[x].wallnum].picnum))
                {
                case FEMPIC1__:
                    wall[animwall[x].wallnum].picnum = BLANKSCREEN;
                    break;
                case FEMPIC2__:
                case FEMPIC3__:
                    wall[animwall[x].wallnum].picnum = SCREENBREAK6;
                    break;
                }
        }

        g_interpolationCnt = 0;
        startofdynamicinterpolations = 0;

        k = headspritestat[STAT_EFFECTOR];
        while (k >= 0)
        {
            switch (sprite[k].lotag)
            {
            case SE_31_FLOOR_RISE_FALL:
                G_SetInterpolation(&sector[sprite[k].sectnum].floorz);
                break;
            case SE_32_CEILING_RISE_FALL:
                G_SetInterpolation(&sector[sprite[k].sectnum].ceilingz);
                break;
            case SE_25_PISTON:
                G_SetInterpolation(&sector[sprite[k].sectnum].floorz);
                G_SetInterpolation(&sector[sprite[k].sectnum].ceilingz);
                break;
            case SE_17_WARP_ELEVATOR:
                G_SetInterpolation(&sector[sprite[k].sectnum].floorz);
                G_SetInterpolation(&sector[sprite[k].sectnum].ceilingz);
                break;
            case SE_0_ROTATING_SECTOR:
            case SE_5:
            case SE_6_SUBWAY:
            case SE_11_SWINGING_DOOR:
            case SE_14_SUBWAY_CAR:
            case SE_15_SLIDING_DOOR:
            case SE_16_REACTOR:
            case SE_26:
            case SE_30_TWO_WAY_TRAIN:
                Sect_SetInterpolation(SECT(k), true);
                break;
            }

            k = nextspritestat[k];
        }

        calc_sector_reachability();
        G_BackupInterpolations();
        for (i = g_animateCount-1;i>=0;i--)
            G_SetInterpolation(G_GetAnimationPointer(i));

        Net_WaitForPlayers();
        clearfifo();
        G_ResetTimers();
    }
}

// MYOS* CON commands.
void VM_DrawTileGeneric(int32_t x, int32_t y, int32_t zoom, int32_t tilenum, int32_t shade, int32_t orientation, int32_t p)
{
    orientation &= (ROTATESPRITE_MAX-1);

    int const rotAngle = (orientation&4) ? 1024 : 0;

    if (!(orientation&ROTATESPRITE_FULL16))
    {
        x<<=16;
        y<<=16;
    }

    rotatesprite_win(x, y, zoom, rotAngle, tilenum, shade, p, 2|orientation);
}

void VM_DrawTile(int32_t x, int32_t y, int32_t tilenum, int32_t shade, int32_t orientation)
{
    auto const pPlayer = g_player[screenpeek].ps;
    int32_t    tilePal = pPlayer->cursectnum >= 0 ? sector[pPlayer->cursectnum].floorpal : 0;

    VM_DrawTileGeneric(x, y, 65536, tilenum, shade, orientation, tilePal);
}

void VM_DrawTileSmall(int32_t x, int32_t y, int32_t tilenum, int32_t shade, int32_t orientation)
{
    auto const pPlayer = g_player[screenpeek].ps;
    int32_t    tilePal = pPlayer->cursectnum >= 0 ? sector[pPlayer->cursectnum].floorpal : 0;

    VM_DrawTileGeneric(x, y, 32768, tilenum, shade, orientation, tilePal);
}
