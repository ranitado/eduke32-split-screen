#include "duke3d.h"
#include "projectiles.h"
#include "gamestructures.h"

defaultprojectile_t DefaultProjectile;
EDUKE32_STATIC_ASSERT(sizeof(projectile_t) == sizeof(DefaultProjectile));

void C_InitProjectiles(void)
{
    DefaultProjectile = {
        // workslike, cstat, hitradius, range, flashcolor;
        // spawns, sound, isound, vel, decal, trail, tnum, drop;
        // offset, bounces, bsound, toffset, extra, extra_rand;
        // sxrepeat, syrepeat, txrepeat, tyrepeat;
        // shade, xrepeat, yrepeat, pal;
        // movecnt, clipdist, filler[2], userdata;

        // XXX: The default projectile seems to mimic a union of hard-coded ones.

        1, -1, 2048, 0, 0,
        (int16_t)SMALLSMOKE, -1, -1, 600, (int16_t)BULLETHOLE, -1, 0, 0,
        448, (int16_t)g_numFreezeBounces, (int16_t)PIPEBOMB_BOUNCE, 1, 100, -1,
        -1, -1, -1, -1,
        -96, 18, 18, 0,
        1, 32, { 0, 0 }, 0,
    };

    for (auto& tile : g_tile)
    {
        if (tile.proj)
            *tile.proj = DefaultProjectile;

        if (tile.defproj)
            *tile.defproj = DefaultProjectile;
    }
}

void C_AllocProjectile(int32_t tileNum)
{
    g_tile[tileNum].proj = (projectile_t*)Xrealloc(g_tile[tileNum].proj, 2 * sizeof(projectile_t));
    g_tile[tileNum].defproj = g_tile[tileNum].proj + 1;
}

void C_FreeProjectile(int32_t tileNum)
{
    DO_FREE_AND_NULL(g_tile[tileNum].proj);
    g_tile[tileNum].defproj = NULL;
}

void C_DefineProjectile(int32_t tileNum, int32_t labelNum, int32_t val)
{
    if (g_tile[tileNum].proj == NULL)
    {
        C_AllocProjectile(tileNum);
        *g_tile[tileNum].proj = DefaultProjectile;
    }

    projectile_t* const proj = g_tile[tileNum].proj;

    switch (labelNum)
    {
        case PROJ_WORKSLIKE:   proj->workslike  = val; break;
        case PROJ_SPAWNS:      proj->spawns     = val; break;
        case PROJ_SXREPEAT:    proj->sxrepeat   = val; break;
        case PROJ_SYREPEAT:    proj->syrepeat   = val; break;
        case PROJ_SOUND:       proj->sound      = val; break;
        case PROJ_ISOUND:      proj->isound     = val; break;
        case PROJ_VEL:         proj->vel        = val; break;
        case PROJ_EXTRA:       proj->extra      = val; break;
        case PROJ_DECAL:       proj->decal      = val; break;
        case PROJ_TRAIL:       proj->trail      = val; break;
        case PROJ_TXREPEAT:    proj->txrepeat   = val; break;
        case PROJ_TYREPEAT:    proj->tyrepeat   = val; break;
        case PROJ_TOFFSET:     proj->toffset    = val; break;
        case PROJ_TNUM:        proj->tnum       = val; break;
        case PROJ_DROP:        proj->drop       = val; break;
        case PROJ_CSTAT:       proj->cstat      = val; break;
        case PROJ_CLIPDIST:    proj->clipdist   = val; break;
        case PROJ_SHADE:       proj->shade      = val; break;
        case PROJ_XREPEAT:     proj->xrepeat    = val; break;
        case PROJ_YREPEAT:     proj->yrepeat    = val; break;
        case PROJ_PAL:         proj->pal        = val; break;
        case PROJ_EXTRA_RAND:  proj->extra_rand = val; break;
        case PROJ_HITRADIUS:   proj->hitradius  = val; break;
        case PROJ_MOVECNT:     proj->movecnt    = val; break;
        case PROJ_OFFSET:      proj->offset     = val; break;
        case PROJ_BOUNCES:     proj->bounces    = val; break;
        case PROJ_BSOUND:      proj->bsound     = val; break;
        case PROJ_RANGE:       proj->range      = val; break;
        case PROJ_FLASH_COLOR: proj->flashcolor = val; break;
        //case PROJ_USERDATA:    proj->userdata   = val; break;
        default: break;
    }

    *g_tile[tileNum].defproj = *proj;

    g_tile[tileNum].flags |= SFLAG_PROJECTILE;
}

projectile_t* Proj_GetProjectile(int tile)
{
    return ((unsigned)tile < MAXTILES&& g_tile[tile].proj) ? g_tile[tile].proj : &DefaultProjectile;
}

int Proj_GetDamage(projectile_t* pProj)
{
    Bassert(pProj);

    int damage = pProj->extra;

    if (pProj->extra_rand > 0)
        damage += (krand() % pProj->extra_rand);

    return damage;
}

static int32_t safeldist(int32_t spriteNum, const void* pSprite)
{
    int32_t distance = ldist(&sprite[spriteNum], pSprite);
    return distance ? distance : 1;
}

static int CheckShootSwitchTile(int tileNum)
{
    return tileNum == DIPSWITCH || tileNum == DIPSWITCH + 1 || tileNum == DIPSWITCH2 || tileNum == DIPSWITCH2 + 1 ||
        tileNum == DIPSWITCH3 || tileNum == DIPSWITCH3 + 1 || tileNum == HANDSWITCH || tileNum == HANDSWITCH + 1;
}

static int SectorContainsSE13(int const sectNum)
{
    if (sectNum >= 0)
    {
        for (bssize_t SPRITES_OF_SECT(sectNum, i))
        {
            if (sprite[i].statnum == STAT_EFFECTOR && sprite[i].lotag == SE_13_EXPLOSIVE)
                return 1;
        }
    }
    return 0;
}

// Maybe handle bit 2 (swap wall bottoms).
// (in that case walltype *hitwal may be stale)
static inline void HandleHitWall(hitdata_t* hitData)
{
    auto const hitWall = (uwallptr_t)&wall[hitData->wall];

    if ((hitWall->cstat & 2) && redwallp(hitWall) && (hitData->z >= sector[hitWall->nextsector].floorz))
        hitData->wall = hitWall->nextwall;
}

static void A_SetHitData(int spriteNum, const hitdata_t* hitData)
{
    actor[spriteNum].t_data[6] = hitData->wall;
    actor[spriteNum].t_data[7] = hitData->sect;
    actor[spriteNum].t_data[8] = hitData->sprite;
}

static int g_overrideShootZvel = 0;  // a boolean
static int g_shootZvel;  // the actual zvel if the above is !=0
static int A_GetShootZvel(int defaultZvel)
{
    return g_overrideShootZvel ? g_shootZvel : defaultZvel;
}

static void Proj_DoWaterTracers(vec3_t startPos, vec3_t const* endPos, int n, int16_t ownerSpriteNum)
{
    if ((klabs(startPos.x - endPos->x) + klabs(startPos.y - endPos->y)) < 3084)
        return;

    vec3_t const v_inc = { tabledivide32_noinline(endPos->x - startPos.x, n + 1), tabledivide32_noinline(endPos->y - startPos.y, n + 1),
                           tabledivide32_noinline(endPos->z - startPos.z, n + 1) };

    int16_t sectNum = sprite[ownerSpriteNum].sectnum;
    for (bssize_t i = n; i > 0; i--)
    {
        startPos.x += v_inc.x;
        startPos.y += v_inc.y;
        startPos.z += v_inc.z;

        updatesector(startPos.x, startPos.y, &sectNum);

        if (sectNum < 0)
            break;

        if (sector[sectNum].lotag == ST_2_UNDERWATER)
            A_InsertSprite(sectNum, startPos.x, startPos.y, startPos.z, WATERBUBBLE, -32, 4 + (krand() & 3), 4 + (krand() & 3), krand() & 2047, 0, 0, ownerSpriteNum, 5);
        else
            A_InsertSprite(sectNum, startPos.x, startPos.y, startPos.z, SMALLSMOKE, -32, 14, 14, 0, 0, 0, ownerSpriteNum, STAT_MISC);
    }
}

static void Proj_DoHitscanTrail(const vec3_t* startPos, const vec3_t* endPos, int projAng, int tileNum, int ownerSpriteNum)
{
    const projectile_t* const pProj = Proj_GetProjectile(tileNum);

    vec3_t        spawnPos = { startPos->x + tabledivide32_noinline(sintable[(348 + projAng + 512) & 2047], pProj->offset),
                               startPos->y + tabledivide32_noinline(sintable[(projAng + 348) & 2047], pProj->offset),
                               startPos->z + 1024 + (pProj->toffset << 8) };

    int32_t      numSegments = ((FindDistance2D(spawnPos.x - endPos->x, spawnPos.y - endPos->y)) >> 8) + 1;

    vec3_t const increment = { tabledivide32_noinline((endPos->x - spawnPos.x), numSegments),
                               tabledivide32_noinline((endPos->y - spawnPos.y), numSegments),
                               tabledivide32_noinline((endPos->z - spawnPos.z), numSegments) };

    spawnPos.x += increment.x >> 2;
    spawnPos.y += increment.y >> 2;
    spawnPos.z += increment.z >> 2;

    int16_t sectNum = sprite[ownerSpriteNum].sectnum;
    // [JM] Limit by numSegments so we don't overshoot the target.
    for (bssize_t i = pProj->tnum; i > 0 && numSegments > 0; --i, --numSegments)
    {
        int32_t ceilZ, floorZ;

        spawnPos.x += increment.x;
        spawnPos.y += increment.y;
        spawnPos.z += increment.z;

        // Range check so we stop if we're beyond pProj->range.
        if (pProj->range > 0 && (klabs(startPos->x - spawnPos.x) + klabs(startPos->y - spawnPos.y)) > pProj->range)
            break;

        updatesectorz(spawnPos.x, spawnPos.y, spawnPos.z, &sectNum);

        if (sectNum < 0)
            break;

        yax_getzsofslope(sectNum, spawnPos.x, spawnPos.y, &ceilZ, &floorZ);

        if (spawnPos.z > floorZ || spawnPos.z < ceilZ)
            break;

        int trailSprite = A_InsertSprite(sectNum, spawnPos.x, spawnPos.y, spawnPos.z, pProj->trail, -32,
            pProj->txrepeat, pProj->tyrepeat, projAng, 0, 0, ownerSpriteNum, STAT_ACTOR);

        UNREFERENCED_PARAMETER(trailSprite);
    }
}

// <extra>: damage that this shotspark does
static int Proj_InsertShotspark(const hitdata_t* hitData, int spriteNum, int projecTile, int sparkSize, int sparkAng, int damage)
{
    int returnSprite = A_InsertSprite(hitData->sect, hitData->x, hitData->y, hitData->z, SHOTSPARK1, -15,
        sparkSize, sparkSize, sparkAng, 0, 0, spriteNum, STAT_PROJECTILE);

    sprite[returnSprite].extra = damage;
    sprite[returnSprite].yvel = projecTile;  // This is a hack to allow you to detect which weapon spawned a SHOTSPARK1

    A_SetHitData(returnSprite, hitData);

    return returnSprite;
}

static void Proj_MaybeAddSpread(int doSpread, int32_t* zvel, int* shootAng, int zRange, int angRange)
{
    if (doSpread)
    {
        // Ranges <= 1 mean no spread at all. A range of 1 calls krand() though.
        if (zRange > 0)
            *zvel += (zRange >> 1) - krand() % zRange;

        if (angRange > 0)
            *shootAng += (angRange >> 1) - krand() % angRange;
    }
}

// Maybe damage a ceiling or floor as the consequence of projectile impact.
// Returns 1 if projectile hit a parallaxed ceiling.
// NOTE: Compare with Proj_MaybeDamageCF() in actors.c
static int Proj_MaybeDamageCF2(int const spriteNum, int const zvel, int const hitSect)
{
    Bassert(hitSect >= 0);

    if (zvel < 0)
    {
        if (sector[hitSect].ceilingstat & 1)
            return 1;

        Sect_DamageCeiling(spriteNum, hitSect);
    }
    else if (zvel > 0)
    {
        if (sector[hitSect].floorstat & 1)
        {
            // Keep original Duke3D behavior: pass projectiles through
            // parallaxed ceilings, but NOT through such floors.
            return 0;
        }

        Sect_DamageFloor(spriteNum, hitSect);
    }

    return 0;
}

static int Proj_DoHitscan(int spriteNum, int32_t const cstatmask, const vec3_t* const srcVect, int zvel, int const shootAng, hitdata_t* const hitData)
{
    auto const pSprite = &sprite[spriteNum];

    pSprite->cstat &= ~cstatmask;
    zvel = A_GetShootZvel(zvel);
    int16_t sectnum = pSprite->sectnum;
    updatesector(srcVect->x, srcVect->y, &sectnum);
    hitscan(srcVect, sectnum, sintable[(shootAng + 512) & 2047], sintable[shootAng & 2047], zvel << 6, hitData, CLIPMASK1);
    pSprite->cstat |= cstatmask;

    return (hitData->sect < 0);
}

static void Proj_MaybeSpawn(int spriteNum, int projecTile, const hitdata_t* hitData)
{
    // projecTile < 0 is for hard-coded projectiles
    projectile_t* const pProj = Proj_GetProjectile(projecTile);
    int                 spawnTile = projecTile < 0 ? -projecTile : pProj->spawns;

    if (spawnTile >= 0)
    {
        int spawned = A_Spawn(spriteNum, spawnTile);

        if (projecTile >= 0)
        {
            if (pProj->sxrepeat > 4)
                sprite[spawned].xrepeat = pProj->sxrepeat;

            if (pProj->syrepeat > 4)
                sprite[spawned].yrepeat = pProj->syrepeat;
        }

        A_SetHitData(spawned, hitData);
    }
}

static void Proj_DoRandDecalSize(int const spriteNum, int const projecTile)
{
    const projectile_t* const proj = Proj_GetProjectile(projecTile);
    auto const         pSprite = &sprite[spriteNum];

    if (proj->workslike & PROJECTILE_RANDDECALSIZE)
        pSprite->xrepeat = pSprite->yrepeat = clamp((krand() & proj->xrepeat), pSprite->yrepeat, pSprite->xrepeat);
    else
    {
        pSprite->xrepeat = proj->xrepeat;
        pSprite->yrepeat = proj->yrepeat;
    }
}

static void Proj_HandleKnee(hitdata_t* const hitData, int const spriteNum, int const playerNum, int const projecTile, int const shootAng,
    const projectile_t* const proj, int const inserttile, int const randomDamage, int const spawnTile,
    int const soundNum)
{
    auto const pPlayer = playerNum >= 0 ? g_player[playerNum].ps : NULL;

    int kneeSprite = A_InsertSprite(hitData->sect, hitData->x, hitData->y, hitData->z,
        inserttile, -15, 0, 0, shootAng, 32, 0, spriteNum, 4);

    if (proj != NULL)
    {
        // Custom projectiles.
        actor[kneeSprite].projectile.workslike = Proj_GetProjectile(sprite[kneeSprite].picnum)->workslike;
        sprite[kneeSprite].extra = proj->extra;
    }

    if (randomDamage > 0)
        sprite[kneeSprite].extra += (krand() & randomDamage);

    if (playerNum >= 0)
    {
        if (spawnTile >= 0)
        {
            int k = A_Spawn(kneeSprite, spawnTile);
            sprite[k].z -= ZOFFSET3;
            A_SetHitData(k, hitData);
        }

        if (soundNum >= 0)
            A_PlaySound(soundNum, kneeSprite);
    }

    if (pPlayer != NULL && pPlayer->inv_amount[GET_STEROIDS] > 0 && pPlayer->inv_amount[GET_STEROIDS] < 400)
        sprite[kneeSprite].extra += (pPlayer->max_player_health >> 2);

    //int const dmg = clamp<int>(sprite[kneeSprite].extra, FF_WEAPON_DMG_MIN, FF_WEAPON_DMG_MAX);

    if (hitData->sprite >= 0 && sprite[hitData->sprite].picnum != ACCESSSWITCH && sprite[hitData->sprite].picnum != ACCESSSWITCH2)
    {
        //I_AddForceFeedback((dmg << FF_WEAPON_DMG_SCALE), (dmg << FF_WEAPON_DMG_SCALE), max<int>(FF_WEAPON_MAX_TIME, dmg << FF_WEAPON_TIME_SCALE));
        A_DamageObject(hitData->sprite, kneeSprite);
        if (playerNum >= 0)
            P_ActivateSwitch(playerNum, hitData->sprite, 1);
    }
    else if (hitData->wall >= 0)
    {
        //I_AddForceFeedback((dmg << FF_WEAPON_DMG_SCALE), (dmg << FF_WEAPON_DMG_SCALE), max<int>(FF_WEAPON_MAX_TIME, dmg << FF_WEAPON_TIME_SCALE));
        HandleHitWall(hitData);

        if (wall[hitData->wall].picnum != ACCESSSWITCH && wall[hitData->wall].picnum != ACCESSSWITCH2)
        {
            A_DamageWall(kneeSprite, hitData->wall, hitData->xyz, projecTile);
            if (playerNum >= 0)
                P_ActivateSwitch(playerNum, hitData->wall, 0);
        }
    }
}

static int A_FindTargetSprite(spritetype* s, int aang, int atwith)
{
    int i, j, a;
    static int aimstats[] = { STAT_PLAYER, STAT_DUMMYPLAYER, STAT_ACTOR, STAT_ZOMBIEACTOR };
    int dx1, dy1, dx2, dy2, dx3, dy3, smax, sdist;
    int xv, yv;

    if (s->picnum == APLAYER)
    {
        if (!g_player[s->yvel].ps->auto_aim)
            return -1;
        if (g_player[s->yvel].ps->auto_aim == 2) // Hitscan only, don't autoaim projectiles.
        {
            if (A_CheckSpriteTileFlags(atwith, SFLAG_PROJECTILE) && (Proj_GetProjectile(atwith)->workslike & PROJECTILE_RPG))
                return -1;
            else switch (tileGetMapping(atwith))
            {
                case TONGUE__:
                case FREEZEBLAST__:
                case SHRINKSPARK__:
                case SHRINKER__:
                case RPG__:
                case FIRELASER__:
                case SPIT__:
                case COOLEXPLOSION1__:
                    return -1;
                default:
                    break;
            }
        }
    }

    a = s->ang;

    j = -1;

    int gotshrinker = (s->picnum == APLAYER && *aplWeaponWorksLike[g_player[s->yvel].ps->curr_weapon] == SHRINKER_WEAPON);
    int gotfreezer = (s->picnum == APLAYER && *aplWeaponWorksLike[g_player[s->yvel].ps->curr_weapon] == FREEZE_WEAPON);

    smax = 0x7fffffff;

    dx1 = sintable[(a + 512 - aang) & 2047];
    dy1 = sintable[(a - aang) & 2047];
    dx2 = sintable[(a + 512 + aang) & 2047];
    dy2 = sintable[(a + aang) & 2047];

    dx3 = sintable[(a + 512) & 2047];
    dy3 = sintable[a & 2047];

    for (size_t k = 0; k < ARRAY_SIZE(aimstats); k++)
    {
        if (j >= 0)
            break;

        for (i = headspritestat[aimstats[k]]; i >= 0; i = nextspritestat[i])
        {
            if (sprite[i].xrepeat > 0 && sprite[i].extra >= 0 && (sprite[i].cstat & (257 + 32768)) == 257)
            {
                if (A_CheckEnemySprite(&sprite[i]) || k < 2)
                {
                    if (A_CheckEnemySprite(&sprite[i]) || PN(i) == APLAYER || PN(i) == SHARK)
                    {
                        // DON'T DO THIS, THAT'S YOUR BUDDY, GUY!
                        if (PN(i) == APLAYER &&
                            (GTFLAGS(GAMETYPE_PLAYERSFRIENDLY) || (GTFLAGS(GAMETYPE_TDM) && g_player[sprite[i].yvel].ps->team == g_player[s->yvel].ps->team)) &&
                            s->picnum == APLAYER && s != &sprite[i])
                            continue;

                        if (gotshrinker && sprite[i].xrepeat < 30)
                        {
                            if (PN(i) == SHARK)
                                continue;
                            else if (!(PN(i) >= GREENSLIME && PN(i) <= GREENSLIME + 7))
                                continue;
                        }

                        if (gotfreezer && sprite[i].pal == 1)
                            continue;
                    }

                    xv = (SX(i) - s->x);
                    yv = (SY(i) - s->y);

                    if ((dy1 * xv) <= (dx1 * yv))
                    {
                        if ((dy2 * xv) >= (dx2 * yv))
                        {
                            sdist = mulscale(dx3, xv, 14) + mulscale(dy3, yv, 14);
                            if (sdist > 512 && sdist < smax)
                            {
                                if (s->picnum == APLAYER)
                                    a = (klabs(scale(SZ(i) - s->z, 10, sdist) - fix16_to_int(g_player[s->yvel].ps->q16horiz + g_player[s->yvel].ps->q16horizoff - F16(100))) < 100);
                                else a = 1;

                                int32_t can_see;
                                if (PN(i) == ORGANTIC || PN(i) == ROTATEGUN)
                                    can_see = cansee(SX(i), SY(i), SZ(i), SECT(i), s->x, s->y, s->z - (32 << 8), s->sectnum);
                                else
                                    can_see = cansee(SX(i), SY(i), SZ(i) - (32 << 8), SECT(i), s->x, s->y, s->z - (32 << 8), s->sectnum);

                                if (PN(i) == GREENSLIME && sdist < 1596) // Slimer is at our feet, don't autoaim.
                                    can_see = 0;

                                if (a && can_see)
                                {
                                    smax = sdist;
                                    j = i;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return j;
}

// flags:
//  1: do sprite center adjustment (cen-=(8<<8)) for GREENSLIME or ROTATEGUN
//  2: do auto getangle only if not RECON (if clear, do unconditionally)
static int GetAutoAimAng(int spriteNum, int playerNum, int projecTile, int zAdjust, int aimFlags,
    const vec3_t* startPos, int projVel, int32_t* pZvel, int* pAng)
{
    int returnSprite = -1;

    Bassert((unsigned)playerNum < MAXPLAYERS);

    Gv_SetVar(sysVarIDs.AUTOAIMANGLE, g_player[playerNum].ps->auto_aim == 3 ? AUTO_AIM_ANGLE << 1 : AUTO_AIM_ANGLE, spriteNum, playerNum);

    X_OnEvent(EVENT_GETAUTOAIMANGLE, spriteNum, playerNum, -1);

    int aimang = Gv_GetVar(sysVarIDs.AUTOAIMANGLE, spriteNum, playerNum);
    if (aimang > 0)
        returnSprite = A_FindTargetSprite(&sprite[spriteNum], aimang, projecTile);

    if (returnSprite >= 0)
    {
        auto const pSprite = (uspriteptr_t)&sprite[returnSprite];
        int        zCenter = 2 * (pSprite->yrepeat * tilesiz[pSprite->picnum].y) + zAdjust;

        if (aimFlags && ((pSprite->picnum >= GREENSLIME && pSprite->picnum <= GREENSLIME + 7) || pSprite->picnum == ROTATEGUN || pSprite->cstat & CSTAT_SPRITE_YCENTER))
            zCenter -= ZOFFSET3;

        int spriteDist = safeldist(g_player[playerNum].ps->i, &sprite[returnSprite]);
        *pZvel = tabledivide32_noinline((pSprite->z - startPos->z - zCenter) * projVel, spriteDist);

        if (!(aimFlags & 2) || sprite[returnSprite].picnum != RECON)
            *pAng = getangle(pSprite->x - startPos->x, pSprite->y - startPos->y);
    }

    return returnSprite;
}

// Prepare hitscan weapon fired from player p.
static void P_PreFireHitscan(int spriteNum, int playerNum, int projecTile, vec3_t* srcVect, int32_t* zvel, int* shootAng,
    int accurateAim, int doSpread)
{
    int angRange = 32;
    int zRange = 256;
    int aimSprite = GetAutoAimAng(spriteNum, playerNum, projecTile, 5 << 8, 0 + 1, srcVect, 256, zvel, shootAng);

    auto const pPlayer = g_player[playerNum].ps;

    Gv_SetVar(sysVarIDs.ANGRANGE, angRange, spriteNum, playerNum);
    Gv_SetVar(sysVarIDs.ZRANGE, zRange, spriteNum, playerNum);

    X_OnEvent(EVENT_GETSHOTRANGE, spriteNum, playerNum, -1);

    angRange = Gv_GetVar(sysVarIDs.ANGRANGE, spriteNum, playerNum);
    zRange = Gv_GetVar(sysVarIDs.ZRANGE, spriteNum, playerNum);

    if (accurateAim)
    {
        if (!pPlayer->auto_aim)
        {
            hitdata_t hitData;

            *zvel = A_GetShootZvel(fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) << 5);

            hitscan(srcVect, sprite[spriteNum].sectnum, sintable[(*shootAng + 512) & 2047],
                sintable[*shootAng & 2047], *zvel << 6, &hitData, CLIPMASK1);

            if (hitData.sprite != -1)
            {
                int const statNumMap = ((1 << STAT_ACTOR) | (1 << STAT_ZOMBIEACTOR) | (1 << STAT_PLAYER) | (1 << STAT_DUMMYPLAYER));
                int const statNum = sprite[hitData.sprite].statnum;

                if ((unsigned)statNum <= 30 && (statNumMap & (1 << statNum)))
                    aimSprite = hitData.sprite;
            }
        }

        if (aimSprite == -1)
            goto notarget;
    }
    else
    {
        if (aimSprite == -1)  // no target
        {
        notarget:
            *zvel = fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) << 5;
        }

        Proj_MaybeAddSpread(doSpread, zvel, shootAng, zRange, angRange);
    }

    // ZOFFSET6 is added to this position at the same time as the player's pyoff in A_ShootWithZvel()
    srcVect->z -= ZOFFSET6;
}

// Hitscan weapon fired from actor (sprite s);
static void A_PreFireHitscan(spritetype* pSprite, vec3_t* const srcVect, int32_t* const zvel, int* const shootAng, int const doSpread)
{
    int const  playerNum = A_FindPlayer(pSprite, NULL);
    auto const pPlayer = g_player[playerNum].ps;
    int const  playerDist = safeldist(pPlayer->i, pSprite);

    *zvel = tabledivide32_noinline((pPlayer->pos.z - srcVect->z) << 8, playerDist);

    srcVect->z -= ZOFFSET6;

    if (pSprite->picnum == BOSS1)
        *shootAng = getangle(pPlayer->pos.x - srcVect->x, pPlayer->pos.y - srcVect->y);

    Proj_MaybeAddSpread(doSpread, zvel, shootAng, 256, 128 >> (uint8_t)(pSprite->picnum != BOSS1));
}

// Finish shooting hitscan weapon from player <p>. <k> is the inserted SHOTSPARK1.
// * <spawnObject> is passed to Proj_MaybeSpawn()
// * <decalTile> and <wallDamage> are for wall impact
// * <wallDamage> is passed to A_DamageWall()
// * <decalFlags> is for decals upon wall impact:
//    1: handle random decal size (tile <atwith>)
//    2: set cstat to wall-aligned + random x/y flip
//
// TODO: maybe split into 3 cases (hit neither wall nor sprite, hit sprite, hit wall)?
static int P_PostFireHitscan(int playerNum, int const spriteNum, hitdata_t* const hitData, int const spriteOwner,
    int const projecTile, int const zvel, int const spawnTile, int const decalTile, int const wallDamage,
    int const decalFlags)
{
    if (hitData->wall == -1 && hitData->sprite == -1)
    {
        if (Proj_MaybeDamageCF2(spriteNum, zvel, hitData->sect))
        {
            sprite[spriteNum].xrepeat = 0;
            sprite[spriteNum].yrepeat = 0;
            return -1;
        }

        Proj_MaybeSpawn(spriteNum, spawnTile, hitData);
    }
    else if (hitData->sprite >= 0)
    {
        A_DamageObject(hitData->sprite, spriteNum);
        if (sprite[hitData->sprite].picnum == APLAYER && !G_BlockFriendlyFire(sprite[hitData->sprite].yvel, sprite[spriteOwner].yvel))
        {
            int jibSprite = A_Spawn(spriteNum, JIBS6);

            sprite[spriteNum].xrepeat = sprite[spriteNum].yrepeat = 0;
            sprite[jibSprite].z += ZOFFSET6;
            sprite[jibSprite].xvel = 16;
            sprite[jibSprite].xrepeat = sprite[jibSprite].yrepeat = 24;
            sprite[jibSprite].ang += 64 - (krand() & 127);
        }
        else
        {
            Proj_MaybeSpawn(spriteNum, spawnTile, hitData);
        }

        if (playerNum >= 0 && CheckShootSwitchTile(sprite[hitData->sprite].picnum))
        {
            P_ActivateSwitch(playerNum, hitData->sprite, 1);
            return -1;
        }
    }
    else if (hitData->wall >= 0)
    {
        auto const hitWall = (uwallptr_t)&wall[hitData->wall];

        Proj_MaybeSpawn(spriteNum, spawnTile, hitData);

        if (CheckDoorTile(hitWall->picnum) == 1)
            goto SKIPBULLETHOLE;

        if (playerNum >= 0 && CheckShootSwitchTile(hitWall->picnum))
        {
            P_ActivateSwitch(playerNum, hitData->wall, 0);
            return -1;
        }

        if (hitWall->hitag != 0 || (hitWall->nextwall >= 0 && wall[hitWall->nextwall].hitag != 0))
            goto SKIPBULLETHOLE;

        if ((hitData->sect >= 0 && sector[hitData->sect].lotag == 0) &&
            (hitWall->overpicnum != BIGFORCE && (hitWall->cstat & 16) == 0) &&
            ((hitWall->nextsector >= 0 && sector[hitWall->nextsector].lotag == 0) || (hitWall->nextsector == -1 && sector[hitData->sect].lotag == 0)))
        {
            int decalSprite;

            if (SectorContainsSE13(hitWall->nextsector))
                goto SKIPBULLETHOLE;

            for (SPRITES_OF(STAT_MISC, decalSprite))
                if (sprite[decalSprite].picnum == decalTile && dist(&sprite[decalSprite], &sprite[spriteNum]) < (12 + (krand() & 7)))
                    goto SKIPBULLETHOLE;

            if (decalTile >= 0)
            {
                decalSprite = A_Spawn(spriteNum, decalTile);

                auto const decal = &sprite[decalSprite];

                A_SetHitData(decalSprite, hitData);

                if (!A_CheckSpriteFlags(decalSprite, SFLAG_DECAL))
                    actor[decalSprite].flags |= SFLAG_DECAL;

                int32_t diffZ;
                spriteheightofs(decalSprite, &diffZ, 0);

                decal->z += diffZ >> 1;
                decal->ang = (getangle(hitWall->x - wall[hitWall->point2].x, hitWall->y - wall[hitWall->point2].y) + 1536) & 2047;

                if (decalFlags & 1)
                    Proj_DoRandDecalSize(decalSprite, projecTile);

                if (decalFlags & 2)
                    decal->cstat = 16 + 128 + (krand() & (8 + 4));

                A_SetSprite(decalSprite, CLIPMASK0);

                // BULLETHOLE already adds itself to the deletion queue in
                // A_Spawn(). However, some other tiles do as well.
                if (decalTile != BULLETHOLE)
                    A_AddToDeleteQueue(decalSprite);
            }
        }

    SKIPBULLETHOLE:
        HandleHitWall(hitData);
        A_DamageWall(spriteNum, hitData->wall, hitData->xyz, wallDamage);
    }

    return 0;
}

// Finish shooting hitscan weapon from actor (sprite <i>).
static int A_PostFireHitscan(const hitdata_t* hitData, int const spriteNum, int const projecTile, int const zvel, int const shootAng,
    int const extra, int const spawnTile, int const wallDamage)
{
    int const returnSprite = Proj_InsertShotspark(hitData, spriteNum, projecTile, 24, shootAng, extra);

    if (hitData->sprite >= 0)
    {
        A_DamageObject(hitData->sprite, returnSprite);

        if (sprite[hitData->sprite].picnum != APLAYER)
            Proj_MaybeSpawn(returnSprite, spawnTile, hitData);
        else
            sprite[returnSprite].xrepeat = sprite[returnSprite].yrepeat = 0;
    }
    else if (hitData->wall >= 0)
    {
        A_DamageWall(returnSprite, hitData->wall, hitData->xyz, wallDamage);
        Proj_MaybeSpawn(returnSprite, spawnTile, hitData);
    }
    else
    {
        if (Proj_MaybeDamageCF2(returnSprite, zvel, hitData->sect))
        {
            sprite[returnSprite].xrepeat = 0;
            sprite[returnSprite].yrepeat = 0;
        }
        else Proj_MaybeSpawn(returnSprite, spawnTile, hitData);
    }

    return returnSprite;
}

// Common "spawn blood?" predicate.
// minzdiff: minimal "step" height for blood to be spawned
static int Proj_CheckBlood(vec3_t const* const srcVect, hitdata_t const* const hitData, int const bloodRange, int const minZdiff)
{
    if (hitData->wall < 0 || hitData->sect < 0)
        return 0;

    auto const hitWall = (uwallptr_t)&wall[hitData->wall];

    if ((FindDistance2D(srcVect->x - hitData->x, srcVect->y - hitData->y) < bloodRange)
        && (hitWall->overpicnum != BIGFORCE && (hitWall->cstat & 16) == 0)
        && (sector[hitData->sect].lotag == 0)
        && (hitWall->nextsector < 0 || (sector[hitWall->nextsector].lotag == 0 && sector[hitData->sect].lotag == 0
            && sector[hitData->sect].floorz - sector[hitWall->nextsector].floorz > minZdiff)))
        return 1;

    return 0;
}

#define MinibossScale(i, s) (((s)*sprite[i].yrepeat)/80)

static int A_ShootCustom(int const spriteNum, int const projecTile, int shootAng, vec3_t* const startPos)
{
    /* Custom projectiles */
    hitdata_t           hitData;
    projectile_t* const pProj = Proj_GetProjectile(projecTile);
    auto const   pSprite = &sprite[spriteNum];
    int const           playerNum = (pSprite->picnum == APLAYER) ? pSprite->yvel : -1;
    auto const pPlayer = playerNum >= 0 ? g_player[playerNum].ps : NULL;

    // Do flashes & firing audio first, in case we need to block the rest for prediction.
    if (pProj->workslike & (PROJECTILE_HITSCAN | PROJECTILE_RPG))
    {
        if (!(pProj->workslike & PROJECTILE_NOSETOWNERSHADE) && pSprite->extra >= 0)
            pSprite->shade = pProj->shade;

        if ((pPlayer != NULL) && (pProj->sound >= 0) && (pProj->workslike & PROJECTILE_RPG))
            A_PlaySound(pProj->sound, spriteNum);
    }

    // Stop here if we're predicting
    if (oldnet_predicting)
        return -1;

#ifdef POLYMER
    // Flashes for non-players.
    if (videoGetRenderMode() == REND_POLYMER && pProj->flashcolor && playerNum == -1)
    {
        vec3_t const offset = { -((sintable[(pSprite->ang + 512) & 2047]) >> 7), -((sintable[(pSprite->ang) & 2047]) >> 7), PHEIGHT };
        G_AddGameLight(spriteNum, pSprite->sectnum, offset, 8192, 0, 100, pProj->flashcolor, PR_LIGHT_PRIO_MAX_GAME);
        practor[spriteNum].lightcount = 2;
    }
#endif // POLYMER

    if (pProj->offset == 0)
        pProj->offset = 1;

    int     otherSprite = -1;
    int32_t zvel = 0;

    switch (pProj->workslike & PROJECTILE_TYPE_MASK)
    {
        case PROJECTILE_HITSCAN:
        {
            if (playerNum >= 0)
                P_PreFireHitscan(spriteNum, playerNum, projecTile, startPos, &zvel, &shootAng,
                    pProj->workslike & PROJECTILE_ACCURATE_AUTOAIM, !(pProj->workslike & PROJECTILE_ACCURATE));
            else
                A_PreFireHitscan(pSprite, startPos, &zvel, &shootAng, !(pProj->workslike & PROJECTILE_ACCURATE));

            bool doesPenetrate = (pProj->workslike & PROJECTILE_PENETRATE);

#define MAX_PENETRATIONS 16

            hitdata_t hits[MAX_PENETRATIONS];
            uint16_t hitCstats[MAX_PENETRATIONS];
            vec3_t nextStartPos = *startPos;

            int hitCount = 0;
            do
            {
                auto curHit = &hits[hitCount];
                int const hitVoid = Proj_DoHitscan(spriteNum, (pProj->cstat >= 0) ? pProj->cstat : 256 + 1, &nextStartPos, zvel, shootAng, curHit);

                // Hit void space, bail.
                if (hitVoid)
                {
                    hitCount--;
                    break;
                }

                nextStartPos = curHit->xyz;

                bool shouldStop = false;

                if (curHit->sprite > -1) // Make this sprite unhittable so we don't hit it multiple times.
                {
                    hitCstats[hitCount] = sprite[curHit->sprite].cstat;
                    sprite[curHit->sprite].cstat &= ~CSTAT_SPRITE_BLOCK_HITSCAN;

                    shouldStop = (sprite[curHit->sprite].cstat & (CSTAT_SPRITE_ALIGNMENT_FLOOR | CSTAT_SPRITE_ALIGNMENT_WALL));

                    // Perhaps an EVENT_DOPENETRATE here? (THISACTOR = Target, RETURN = Projectile Tilenum)
                    // So people can determine whether or not the thing we hit should override shouldStop.
                }
                else break; // No sprite hit? We're done.

                if (!doesPenetrate || shouldStop) // No penetration, so stop.
                    break;

                if (hitCount >= MAX_PENETRATIONS - 1) // Don't increment past array bounds
                    break;

                hitCount++;
            } while (1);

            if (hitCount < 0) // Hit nothing, not even a sector. GTFO.
                return -1;

            // Use the last hit point for the endPos and sector.
            if (pProj->trail >= 0)
                Proj_DoHitscanTrail(startPos, &hits[hitCount].xyz, shootAng, projecTile, spriteNum);

            if (pProj->workslike & PROJECTILE_WATERBUBBLES)
            {
                if ((krand() & 15) == 0 && sector[hits[hitCount].sect].lotag == ST_2_UNDERWATER)
                    Proj_DoWaterTracers(hits[hitCount].xyz, startPos, 8 - (ud.multimode >> 1), spriteNum);
            }

            // Restore cstat, spawn shotsparks and do damage at every point we hit.
            for (int dmgCount = 0; dmgCount <= hitCount; dmgCount++)
            {
                auto curHit = &hits[dmgCount];

                if (curHit->sprite > -1)
                    sprite[curHit->sprite].cstat = hitCstats[dmgCount];

                // This hit point is out of range, skip damage/effect spawning and impact sounds.
                if (pProj->range > 0 && (klabs(startPos->x - curHit->x) + klabs(startPos->y - curHit->y)) > pProj->range)
                    continue;

                if (playerNum >= 0)
                {
                    otherSprite = Proj_InsertShotspark(curHit, spriteNum, projecTile, 10, shootAng, Proj_GetDamage(pProj));

                    if (P_PostFireHitscan(playerNum, otherSprite, curHit, spriteNum, projecTile, zvel, projecTile, pProj->decal,
                        projecTile, 1 + 2) < 0)
                        continue;
                }
                else
                {
                    otherSprite = A_PostFireHitscan(curHit, spriteNum, projecTile, zvel, shootAng, Proj_GetDamage(pProj), projecTile, projecTile);
                }

                if ((krand() & 255) < 4)
                    if (pProj->isound >= 0)
                        S_PlaySound3D(pProj->isound, otherSprite, curHit->xyz);
            }

            return -1;
        }

        case PROJECTILE_RPG:
        {
            if (pPlayer != NULL)
            {
                // NOTE: j is a SPRITE_INDEX
                otherSprite = GetAutoAimAng(spriteNum, playerNum, projecTile, 8 << 8, 0 + 2, startPos, pProj->vel, &zvel, &shootAng);

                if (otherSprite < 0)
                    zvel = fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) * (pProj->vel / 8);
            }
            else
            {
                if (!(pProj->workslike & PROJECTILE_NOAIM))
                {
                    int const otherPlayer = A_FindPlayer(pSprite, NULL);
                    int const otherPlayerDist = safeldist(g_player[otherPlayer].ps->i, pSprite);

                    shootAng = getangle(g_player[otherPlayer].ps->opos.x - startPos->x,
                        g_player[otherPlayer].ps->opos.y - startPos->y);

                    zvel = tabledivide32_noinline((g_player[otherPlayer].ps->opos.z - startPos->z) * pProj->vel, otherPlayerDist);

                    if (A_CheckEnemySprite(pSprite) && (AC_MOVFLAGS(pSprite, &actor[spriteNum]) & face_player_smart))
                        shootAng = pSprite->ang + (krand() & 31) - 16;
                }
            }

            // l may be a SPRITE_INDEX, see above
            int const l = (playerNum >= 0 && otherSprite >= 0) ? otherSprite : -1;

            zvel = A_GetShootZvel(zvel);
            otherSprite = A_InsertSprite(pSprite->sectnum,
                startPos->x + tabledivide32_noinline(sintable[(348 + shootAng + 512) & 2047], pProj->offset),
                startPos->y + tabledivide32_noinline(sintable[(shootAng + 348) & 2047], pProj->offset),
                startPos->z - (1 << 8), projecTile, clamp(pProj->shade, -128, 127), pProj->xrepeat, pProj->yrepeat, shootAng, pProj->vel, zvel, spriteNum, STAT_PROJECTILE);

            // A_InsertSprite sets the rest of the projectile data.

            sprite[otherSprite].extra = Proj_GetDamage(pProj);

            if (!(pProj->workslike & PROJECTILE_BOUNCESOFFWALLS))
                sprite[otherSprite].yvel = l;  // NOT_BOUNCESOFFWALLS_YVEL
            else
            {
                sprite[otherSprite].yvel = (pProj->bounces >= 1) ? pProj->bounces : g_numFreezeBounces;
                sprite[otherSprite].zvel -= (2 << 4);
            }

            return otherSprite;
        }

        case PROJECTILE_KNEE:
        {
            if (playerNum >= 0)
            {
                zvel = fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) << 5;
                startPos->z += (6 << 8);
                shootAng += 15;
            }
            else if (!(pProj->workslike & PROJECTILE_NOAIM))
            {
                int32_t playerDist;
                otherSprite = g_player[A_FindPlayer(pSprite, &playerDist)].ps->i;
                zvel = tabledivide32_noinline((sprite[otherSprite].z - startPos->z) << 8, playerDist + 1);
                shootAng = getangle(sprite[otherSprite].x - startPos->x, sprite[otherSprite].y - startPos->y);
            }

            Proj_DoHitscan(spriteNum, 0, startPos, zvel, shootAng, &hitData);

            if (hitData.sect < 0) return -1;

            if (pProj->range == 0)
                pProj->range = 1024;

            if (pProj->range > 0 && klabs(startPos->x - hitData.x) + klabs(startPos->y - hitData.y) > pProj->range)
                return -1;

            Proj_HandleKnee(&hitData, spriteNum, playerNum, projecTile, shootAng,
                pProj, projecTile, pProj->extra_rand, pProj->spawns, pProj->sound);

            return -1;
        }

        case PROJECTILE_BLOOD:
        {
            shootAng += 64 - (krand() & 127);

            if (playerNum < 0)
                shootAng += 1024;

            zvel = 1024 - (krand() & 2047);

            Proj_DoHitscan(spriteNum, 0, startPos, zvel, shootAng, &hitData);

            if (pProj->range == 0)
                pProj->range = 1024;

            if (Proj_CheckBlood(startPos, &hitData, pProj->range, mulscale3(pProj->yrepeat, tilesiz[pProj->decal].y) << 8))
            {
                uwallptr_t const hitWall = (uwallptr_t)&wall[hitData.wall];

                if (FindDistance2D(hitWall->x - wall[hitWall->point2].x, hitWall->y - wall[hitWall->point2].y) >
                    (mulscale3(pProj->xrepeat + 8, tilesiz[pProj->decal].x)))
                {
                    if (SectorContainsSE13(hitWall->nextsector))
                        return -1;

                    if (hitWall->nextwall >= 0 && wall[hitWall->nextwall].hitag != 0)
                        return -1;

                    if (hitWall->hitag == 0 && pProj->decal >= 0)
                    {
                        otherSprite = A_Spawn(spriteNum, pProj->decal);

                        A_SetHitData(otherSprite, &hitData);

                        if (!A_CheckSpriteFlags(otherSprite, SFLAG_DECAL))
                            actor[otherSprite].flags |= SFLAG_DECAL;

                        sprite[otherSprite].ang = getangle(hitWall->x - wall[hitWall->point2].x,
                            hitWall->y - wall[hitWall->point2].y) + 512;
                        sprite[otherSprite].xyz = hitData.xyz;

                        Proj_DoRandDecalSize(otherSprite, projecTile);

                        sprite[otherSprite].z += sprite[otherSprite].yrepeat << 8;

                        //                                sprite[spawned].cstat = 16+(krand()&12);
                        sprite[otherSprite].cstat = 16;

                        if (krand() & 1)
                            sprite[otherSprite].cstat |= 4;

                        if (krand() & 1)
                            sprite[otherSprite].cstat |= 8;

                        sprite[otherSprite].shade = sector[sprite[otherSprite].sectnum].floorshade;

                        A_SetSprite(otherSprite, CLIPMASK0);
                        A_AddToDeleteQueue(otherSprite);
                        changespritestat(otherSprite, 5);
                    }
                }
            }

            return -1;
        }

        default:
            return -1;
    }
}

#ifndef EDUKE32_STANDALONE
static int32_t A_ShootHardcoded(int spriteNum, int projecTile, int shootAng, vec3_t startPos,
    spritetype* pSprite, int const playerNum, DukePlayer_t* const pPlayer)
{
    hitdata_t hitData;
    int const spriteSectnum = pSprite->sectnum;
    int32_t Zvel;
    int vel;

    // Do flashes & firing audio first, in case we need to block the rest for prediction.
    switch (tileGetMapping(projecTile))
    {
#if 0
        case FIREBALL__:
        case FLAMETHROWERFLAME__:
            if (!WORLDTOUR) break;
            fallthrough__;
#endif
        case SHOTSPARK1__:
        case SHOTGUN__:
        case CHAINGUN__:
        case FIRELASER__:
        case SPIT__:
        case COOLEXPLOSION1__:
        case FREEZEBLAST__:
        case RPG__:
        case BOUNCEMINE__:
        case MORTER__:
        case SHRINKER__:
        {
            if (pSprite->extra >= 0)
                pSprite->shade = -96;

            if (playerNum >= 0 && projecTile == RPG)
                A_PlaySound(RPG_SHOOT, spriteNum);
        }
    }

    // Stop here if we're predicting.
    if (oldnet_predicting)
        return -1;

#ifdef POLYMER
    // Flashes for non-players.
    if (videoGetRenderMode() == REND_POLYMER && (playerNum == -1))
    {
        switch (tileGetMapping(projecTile))
        {
            case FIRELASER__:
            case SHOTGUN__:
            case SHOTSPARK1__:
            case CHAINGUN__:
            case RPG__:
            case MORTER__:
            {
                vec3_t const offset = { -((sintable[(pSprite->ang + 512) & 2047]) >> 7), -((sintable[(pSprite->ang) & 2047]) >> 7), PHEIGHT };
                G_AddGameLight(spriteNum, pSprite->sectnum, offset, 8192, 0, 100, 255 + (95 << 8), PR_LIGHT_PRIO_MAX_GAME);
                practor[spriteNum].lightcount = 2;
            }

            break;
        }
    }
#endif // POLYMER

    switch (tileGetMapping(projecTile))
    {
        case BLOODSPLAT1__:
        case BLOODSPLAT2__:
        case BLOODSPLAT3__:
        case BLOODSPLAT4__:
            shootAng += 64 - (krand() & 127);
            if (playerNum < 0)
                shootAng += 1024;
            Zvel = 1024 - (krand() & 2047);
            fallthrough__;
        case KNEE__:
            if (projecTile == KNEE)
            {
                if (playerNum >= 0)
                {
                    Zvel = fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) << 5;
                    startPos.z += (6 << 8);
                    shootAng += 15;
                }
                else
                {
                    int32_t   playerDist;
                    int const playerSprite = g_player[A_FindPlayer(pSprite, &playerDist)].ps->i;
                    Zvel = tabledivide32_noinline((sprite[playerSprite].z - startPos.z) << 8, playerDist + 1);
                    shootAng = getangle(sprite[playerSprite].x - startPos.x, sprite[playerSprite].y - startPos.y);
                }
            }

            Proj_DoHitscan(spriteNum, 0, &startPos, Zvel, shootAng, &hitData);

            if (projecTile >= BLOODSPLAT1 && projecTile <= BLOODSPLAT4)
            {
                if (Proj_CheckBlood(&startPos, &hitData, 1024, 16 << 8))
                {
                    uwallptr_t const hitwal = (uwallptr_t)&wall[hitData.wall];

                    if (SectorContainsSE13(hitwal->nextsector))
                        return -1;

                    if (hitwal->nextwall >= 0 && wall[hitwal->nextwall].hitag != 0)
                        return -1;

                    if (hitwal->hitag == 0)
                    {
                        int const spawnedSprite = A_Spawn(spriteNum, projecTile);
                        sprite[spawnedSprite].ang
                            = (getangle(hitwal->x - wall[hitwal->point2].x, hitwal->y - wall[hitwal->point2].y) + 1536) & 2047;
                        sprite[spawnedSprite].xyz = hitData.xyz;
                        sprite[spawnedSprite].cstat |= (krand() & 4);
                        A_SetSprite(spawnedSprite, CLIPMASK0);
                        setsprite(spawnedSprite, &sprite[spawnedSprite].xyz);
                        if (PN(spriteNum) == OOZFILTER || PN(spriteNum) == NEWBEAST)
                            sprite[spawnedSprite].pal = 6;
                    }
                }

                return -1;
            }

            if (hitData.sect < 0)
                break;

            if (klabs(startPos.x - hitData.x) + klabs(startPos.y - hitData.y) < 1024)
                Proj_HandleKnee(&hitData, spriteNum, playerNum, projecTile, shootAng, NULL, KNEE, 7, SMALLSMOKE, KICK_HIT);
            break;

        case SHOTSPARK1__:
        case SHOTGUN__:
        case CHAINGUN__:
        {
            if (playerNum >= 0)
                P_PreFireHitscan(spriteNum, playerNum, projecTile, &startPos, &Zvel, &shootAng,
                    projecTile == SHOTSPARK1__ && !WW2GI, 1);
            else
                A_PreFireHitscan(pSprite, &startPos, &Zvel, &shootAng, 1);

            if (Proj_DoHitscan(spriteNum, 256 + 1, &startPos, Zvel, shootAng, &hitData))
                return -1;

            if ((krand() & 15) == 0 && sector[hitData.sect].lotag == ST_2_UNDERWATER)
                Proj_DoWaterTracers(hitData.xyz, &startPos, 8 - (ud.multimode >> 1), spriteNum);

            int spawnedSprite;

            if (playerNum >= 0)
            {
                spawnedSprite = Proj_InsertShotspark(&hitData, spriteNum, projecTile, 10, shootAng, G_DefaultActorHealthForTile(projecTile) + (krand() % 6));

                if (P_PostFireHitscan(playerNum, spawnedSprite, &hitData, spriteNum, projecTile, Zvel, -SMALLSMOKE, BULLETHOLE, SHOTSPARK1, 0) < 0)
                    return -1;
            }
            else
            {
                spawnedSprite = A_PostFireHitscan(&hitData, spriteNum, projecTile, Zvel, shootAng, G_DefaultActorHealthForTile(projecTile), -SMALLSMOKE,
                    SHOTSPARK1);
            }

            if ((krand() & 255) < 4)
                S_PlaySound3D(PISTOL_RICOCHET, spawnedSprite, hitData.xyz);

            return -1;
        }

        case GROWSPARK__:
        {
            if (playerNum >= 0)
                P_PreFireHitscan(spriteNum, playerNum, projecTile, &startPos, &Zvel, &shootAng, 1, 1);
            else
                A_PreFireHitscan(pSprite, &startPos, &Zvel, &shootAng, 1);

            if (Proj_DoHitscan(spriteNum, 256 + 1, &startPos, Zvel, shootAng, &hitData))
                return -1;

            int const otherSprite = A_InsertSprite(hitData.sect, hitData.x, hitData.y, hitData.z, GROWSPARK, -16, 28, 28,
                shootAng, 0, 0, spriteNum, 1);

            sprite[otherSprite].pal = 2;
            sprite[otherSprite].cstat |= 130;
            sprite[otherSprite].xrepeat = sprite[otherSprite].yrepeat = 1;
            A_SetHitData(otherSprite, &hitData);

            if (hitData.wall == -1 && hitData.sprite == -1 && hitData.sect >= 0)
            {
                Proj_MaybeDamageCF2(otherSprite, Zvel, hitData.sect);
            }
            else if (hitData.sprite >= 0)
                A_DamageObject(hitData.sprite, otherSprite);
            else if (hitData.wall >= 0 && wall[hitData.wall].picnum != ACCESSSWITCH && wall[hitData.wall].picnum != ACCESSSWITCH2)
                A_DamageWall(otherSprite, hitData.wall, hitData.xyz, projecTile);
        }
        break;

#if 0
        case FIREBALL__:
            if (!WORLDTOUR)
                break;
            fallthrough__;
#endif
        case FIRELASER__:
        case SPIT__:
        case COOLEXPLOSION1__:
        {
            switch (projecTile)
            {
                case SPIT__: vel = 292; break;
                case COOLEXPLOSION1__:
                    vel = (pSprite->picnum == BOSS2) ? 644 : 348;
                    startPos.z -= (4 << 7);
                    break;
#if 0
                case FIREBALL__:
                    if (pSprite->picnum == BOSS5 || pSprite->picnum == BOSS5STAYPUT)
                    {
                        vel = 968;
                        startPos.z += 0x1800;
                        break;
                    }
                    fallthrough__;
#endif
                case FIRELASER__:
                default:
                    vel = 840;
                    startPos.z -= (4 << 7);
                    break;
            }

            if (playerNum >= 0)
            {
#if 0
                if (projecTile == FIREBALL)
                {
                    Zvel = fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) * 98;
                    startPos.x += sintable[(348 + shootAng + 512) & 2047] / 448;
                    startPos.y += sintable[(348 + shootAng) & 2047] / 448;
                    startPos.z += 0x300;
                }
                else
#endif
                if (GetAutoAimAng(spriteNum, playerNum, projecTile, -ZOFFSET4, 0, &startPos, vel, &Zvel, &shootAng) < 0)
                    Zvel = fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) * 98;
            }
            else
            {
                int const otherPlayer = A_FindPlayer(pSprite, NULL);
                shootAng += 16 - (krand() & 31);
                hitData.x = safeldist(g_player[otherPlayer].ps->i, pSprite);
                Zvel = tabledivide32_noinline((g_player[otherPlayer].ps->opos.z - startPos.z + (3 << 8)) * vel, hitData.x);
            }

            Zvel = A_GetShootZvel(Zvel);

            int spriteSize = (playerNum >= 0) ? 7 : 18;

            if (projecTile == SPIT)
            {
                spriteSize = 18;
                startPos.z -= (10 << 8);
            }

            int const returnSprite = A_InsertSprite(spriteSectnum, startPos.x, startPos.y, startPos.z, projecTile, -127, spriteSize, spriteSize,
                shootAng, vel, Zvel, spriteNum, 4);

            sprite[returnSprite].extra += (krand() & 7);

            if (projecTile == COOLEXPLOSION1)
            {
                sprite[returnSprite].shade = 0;

                if (PN(spriteNum) == BOSS2)
                {
                    int const saveXvel = sprite[returnSprite].xvel;
                    sprite[returnSprite].xvel = MinibossScale(spriteNum, 1024);
                    A_SetSprite(returnSprite, CLIPMASK0);
                    sprite[returnSprite].xvel = saveXvel;
                    sprite[returnSprite].ang += 128 - (krand() & 255);
                }
            }
#if 0
            else if (projecTile == FIREBALL)
            {
                if (PN(spriteNum) == BOSS5 || PN(spriteNum) == BOSS5STAYPUT || playerNum >= 0)
                {
                    sprite[returnSprite].xrepeat = 40;
                    sprite[returnSprite].yrepeat = 40;
                }
                sprite[returnSprite].yvel = playerNum;
                //sprite[returnSprite].cstat |= 0x4000;
            }
#endif

            sprite[returnSprite].cstat |= 128;
            sprite[returnSprite].clipdist = 4;

            return returnSprite;
        }

        case FREEZEBLAST__:
            startPos.z += (3 << 8);
            fallthrough__;
        case RPG__:
        {
            vel = 644;

            int j = -1;

            if (playerNum >= 0)
            {
                // NOTE: j is a SPRITE_INDEX
                j = GetAutoAimAng(spriteNum, playerNum, projecTile, ZOFFSET3, 0 + 2, &startPos, vel, &Zvel, &shootAng);

                if (j < 0)
                    Zvel = fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) * 81;
            }
            else
            {
                // NOTE: tileNum is a player index
                j = A_FindPlayer(pSprite, NULL);
                shootAng = getangle(g_player[j].ps->opos.x - startPos.x, g_player[j].ps->opos.y - startPos.y);
                if (PN(spriteNum) == BOSS3)
                    startPos.z -= MinibossScale(spriteNum, ZOFFSET5);
                else if (PN(spriteNum) == BOSS2)
                {
                    vel += 128;
                    startPos.z += MinibossScale(spriteNum, 24 << 8);
                }

                Zvel = tabledivide32_noinline((g_player[j].ps->opos.z - startPos.z) * vel, safeldist(g_player[j].ps->i, pSprite));

                if (A_CheckEnemySprite(pSprite) && (AC_MOVFLAGS(pSprite, &actor[spriteNum]) & face_player_smart))
                    shootAng = pSprite->ang + (krand() & 31) - 16;
            }

            Zvel = A_GetShootZvel(Zvel);
            int const returnSprite = A_InsertSprite(spriteSectnum, startPos.x + (sintable[(348 + shootAng + 512) & 2047] / 448),
                startPos.y + (sintable[(shootAng + 348) & 2047] / 448), startPos.z - (1 << 8),
                projecTile, 0, 14, 14, shootAng, vel, Zvel, spriteNum, 4);
            auto const pReturn = &sprite[returnSprite];

            pReturn->extra += (krand() & 7);
            if (projecTile != FREEZEBLAST)
                pReturn->yvel = (playerNum >= 0 && j >= 0) ? j : -1;  // RPG_YVEL
            else
            {
                pReturn->yvel = g_numFreezeBounces;
                pReturn->xrepeat >>= 1;
                pReturn->yrepeat >>= 1;
                pReturn->zvel -= (2 << 4);
            }

            if (playerNum == -1)
            {
                if (PN(spriteNum) == BOSS3)
                {
                    if (krand() & 1)
                    {
                        pReturn->x -= MinibossScale(spriteNum, sintable[shootAng & 2047] >> 6);
                        pReturn->y -= MinibossScale(spriteNum, sintable[(shootAng + 1024 + 512) & 2047] >> 6);
                        pReturn->ang -= MinibossScale(spriteNum, 8);
                    }
                    else
                    {
                        pReturn->x += MinibossScale(spriteNum, sintable[shootAng & 2047] >> 6);
                        pReturn->y += MinibossScale(spriteNum, sintable[(shootAng + 1024 + 512) & 2047] >> 6);
                        pReturn->ang += MinibossScale(spriteNum, 4);
                    }
                    pReturn->xrepeat = MinibossScale(spriteNum, 42);
                    pReturn->yrepeat = MinibossScale(spriteNum, 42);
                }
                else if (PN(spriteNum) == BOSS2)
                {
                    pReturn->x -= MinibossScale(spriteNum, sintable[shootAng & 2047] / 56);
                    pReturn->y -= MinibossScale(spriteNum, sintable[(shootAng + 1024 + 512) & 2047] / 56);
                    pReturn->ang -= MinibossScale(spriteNum, 8) + (krand() & 255) - 128;
                    pReturn->xrepeat = 24;
                    pReturn->yrepeat = 24;
                }
                else if (projecTile != FREEZEBLAST)
                {
                    pReturn->xrepeat = 30;
                    pReturn->yrepeat = 30;
                    pReturn->extra >>= 2;
                }
            }
            else if (PWEAPON(playerNum, g_player[playerNum].ps->curr_weapon, WorksLike) == DEVISTATOR_WEAPON)
            {
                pReturn->extra >>= 2;
                pReturn->ang += 16 - (krand() & 31);
                pReturn->zvel += 256 - (krand() & 511);

                if (g_player[playerNum].ps->hbomb_hold_delay)
                {
                    pReturn->x -= sintable[shootAng & 2047] / 644;
                    pReturn->y -= sintable[(shootAng + 1024 + 512) & 2047] / 644;
                }
                else
                {
                    pReturn->x += sintable[shootAng & 2047] >> 8;
                    pReturn->y += sintable[(shootAng + 1024 + 512) & 2047] >> 8;
                }
                pReturn->xrepeat >>= 1;
                pReturn->yrepeat >>= 1;
            }

            pReturn->cstat |= 128;
            pReturn->clipdist = (projecTile == RPG) ? 4 : 40;

            return returnSprite;
        }

        case HANDHOLDINGLASER__:
        {
            int const zOffset = (playerNum >= 0) ? g_player[playerNum].ps->pyoff : 0;
            Zvel = (playerNum >= 0) ? fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) * 32 : 0;

            startPos.z -= zOffset;
            Proj_DoHitscan(spriteNum, 0, &startPos, Zvel, shootAng, &hitData);
            startPos.z += zOffset;

            int placeMine = 0;
            if (hitData.sprite >= 0)
                break;

            if (hitData.wall >= 0 && hitData.sect >= 0)
            {
                uint32_t xdiff_sq = (hitData.x - startPos.x) * (hitData.x - startPos.x);
                uint32_t ydiff_sq = (hitData.y - startPos.y) * (hitData.y - startPos.y);
                if (xdiff_sq + ydiff_sq < (290 * 290))
                {
                    // ST_2_UNDERWATER
                    if (wall[hitData.wall].nextsector >= 0)
                    {
                        if (sector[wall[hitData.wall].nextsector].lotag <= 2 && sector[hitData.sect].lotag <= 2)
                            placeMine = 1;
                    }
                    else if (sector[hitData.sect].lotag <= 2)
                        placeMine = 1;
                }

            }
            if (placeMine == 1)
            {
                int const tripBombMode = (playerNum < 0) ? 0 :
                    Gv_GetVarByLabel("TRIPBOMB_CONTROL", TRIPBOMB_TRIPWIRE,
                        g_player[playerNum].ps->i, playerNum);
                int const spawnedSprite = A_InsertSprite(hitData.sect, hitData.x, hitData.y, hitData.z, TRIPBOMB, -16, 4, 5,
                    shootAng, 0, 0, spriteNum, 6);
                if (tripBombMode & TRIPBOMB_TIMER)
                {
                    int32_t lLifetime = Gv_GetVarByLabel("STICKYBOMB_LIFETIME", NAM_GRENADE_LIFETIME, g_player[playerNum].ps->i, playerNum);
                    int32_t lLifetimeVar
                        = Gv_GetVarByLabel("STICKYBOMB_LIFETIME_VAR", NAM_GRENADE_LIFETIME_VAR, g_player[playerNum].ps->i, playerNum);
                    // set timer.  blows up when at zero....
                    actor[spawnedSprite].t_data[7] = lLifetime + mulscale14(krand(), lLifetimeVar) - lLifetimeVar;
                    // TIMER_CONTROL
                    actor[spawnedSprite].t_data[6] = 1;
                }
                else
                    sprite[spawnedSprite].hitag = spawnedSprite;

                A_PlaySound(LASERTRIP_ONWALL, spawnedSprite);
                sprite[spawnedSprite].xvel = -20;
                A_SetSprite(spawnedSprite, CLIPMASK0);
                sprite[spawnedSprite].cstat = 16;

                int const p2 = wall[hitData.wall].point2;
                int const wallAng = getangle(wall[hitData.wall].x - wall[p2].x, wall[hitData.wall].y - wall[p2].y) - 512;

                actor[spawnedSprite].t_data[5] = sprite[spawnedSprite].ang = wallAng;

                return spawnedSprite;
            }
            return -1;
        }

        case BOUNCEMINE__:
        case MORTER__:
        {
            int const playerSprite = g_player[A_FindPlayer(pSprite, NULL)].ps->i;
            int const playerDist = ldist(&sprite[playerSprite], pSprite);

            Zvel = -playerDist >> 1;

            if (Zvel < -4096)
                Zvel = -2048;

            vel = playerDist >> 4;
            Zvel = A_GetShootZvel(Zvel);

            A_InsertSprite(spriteSectnum, startPos.x + (sintable[(512 + shootAng + 512) & 2047] >> 8),
                startPos.y + (sintable[(shootAng + 512) & 2047] >> 8), startPos.z + (6 << 8), projecTile, -64, 32, 32,
                shootAng, vel, Zvel, spriteNum, 1);
            break;
        }

        case SHRINKER__:
        {
            if (playerNum >= 0)
            {
                if (NAM_WW2GI || GetAutoAimAng(spriteNum, playerNum, projecTile, ZOFFSET6, 0, &startPos, 768, &Zvel, &shootAng) < 0)
                    Zvel = fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) * 98;
            }
            else if (pSprite->statnum != STAT_EFFECTOR)
            {
                int const otherPlayer = A_FindPlayer(pSprite, NULL);
                Zvel = tabledivide32_noinline((g_player[otherPlayer].ps->opos.z - startPos.z) * 512,
                    safeldist(g_player[otherPlayer].ps->i, pSprite));
            }
            else
                Zvel = 0;

            Zvel = A_GetShootZvel(Zvel);
            int const returnSprite = A_InsertSprite(spriteSectnum, startPos.x + (sintable[(512 + shootAng + 512) & 2047] >> 12),
                startPos.y + (sintable[(shootAng + 512) & 2047] >> 12), startPos.z + (2 << 8),
                SHRINKSPARK, -16, 28, 28, shootAng, 768, Zvel, spriteNum, 4);
            sprite[returnSprite].cstat |= 128;
            sprite[returnSprite].clipdist = 32;

            return returnSprite;
        }
#if 0
        case FLAMETHROWERFLAME__:
        {
            if (!WORLDTOUR)
                break;

            vel = 400;
            int j, underwater;
            if (playerNum >= 0)
            {
                Zvel = fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) * 81;
                int xv = sprite[pPlayer->i].xvel;
                if (xv)
                {
                    int ang = getangle(startPos.x - pPlayer->opos.x, startPos.y - pPlayer->opos.y);
                    ang = 512 - (1024 - klabs(klabs(ang - shootAng) - 1024));
                    vel = 400 + int(float(ang) * (1.f / 512.f) * float(xv));
                }
                underwater = sector[pPlayer->cursectnum].lotag == ST_2_UNDERWATER;
            }
            else
            {
                // NOTE: j is a player index
                j = A_FindPlayer(pSprite, NULL);
                shootAng = getangle(g_player[j].ps->opos.x - startPos.x, g_player[j].ps->opos.y - startPos.y);
                if (PN(spriteNum) == BOSS3 || PN(spriteNum) == BOSS3STAYPUT)
                    startPos.z -= MinibossScale(spriteNum, ZOFFSET5);
                else if (PN(spriteNum) == BOSS5 || PN(spriteNum) == BOSS5STAYPUT)
                {
                    vel += 128;
                    startPos.z += MinibossScale(spriteNum, 24 << 8);
                }

                Zvel = tabledivide32_noinline((g_player[j].ps->opos.z - startPos.z) * vel, safeldist(g_player[j].ps->i, pSprite));

                if (A_CheckEnemySprite(pSprite) && (AC_MOVFLAGS(pSprite, &actor[spriteNum]) & face_player_smart))
                    shootAng = pSprite->ang + (krand() & 31) - 16;
                underwater = sector[pSprite->sectnum].lotag == 2;
            }
            if (underwater)
            {
                if ((krand() % 5) != 0)
                    return -1;
                j = A_Spawn(spriteNum, WATERBUBBLE);
            }
            else
            {
                j = A_Spawn(spriteNum, projecTile);
                sprite[j].zvel = Zvel;
                sprite[j].xvel = vel;
            }
            sprite[j].x = startPos.x + sintable[(shootAng + 630) & 2047] / 448;
            sprite[j].y = startPos.y + sintable[(shootAng + 112) & 2047] / 448;
            sprite[j].z = startPos.z - 0x100;
            sprite[j].cstat = 128;
            sprite[j].ang = shootAng;
            sprite[j].xrepeat = sprite[j].yrepeat = 2;
            sprite[j].clipdist = 40;
            sprite[j].owner = spriteNum;
            sprite[j].yvel = playerNum;
            if (playerNum == -1 && (sprite[spriteNum].picnum == BOSS5 || sprite[spriteNum].picnum == BOSS5STAYPUT))
            {
                sprite[j].xrepeat = sprite[j].yrepeat = 10;
                sprite[j].x -= sintable[shootAng & 2047] / 56;
                sprite[j].y -= sintable[(shootAng - 512) & 2047] / 56;
            }
            return j;
        }
        case FIREFLY__:
        {
            if (!WORLDTOUR)
                break;

            int j = A_Spawn(spriteNum, projecTile);
            sprite[j].xyz = startPos;
            sprite[j].ang = shootAng;
            sprite[j].xvel = 500;
            sprite[j].zvel = 0;
            return j;
        }
#endif
    }

    return -1;
}
#endif

int A_Shoot(int spriteNum, int projecTile, int const forceZvel)
{
    Bassert(projecTile >= 0);

    auto const pSprite = &sprite[spriteNum];
    int const  playerNum = (pSprite->picnum == APLAYER) ? pSprite->yvel : -1;
    auto const pPlayer = playerNum >= 0 ? g_player[playerNum].ps : NULL;

    if (forceZvel != SHOOT_HARDCODED_ZVEL)
    {
        g_overrideShootZvel = 1;
        g_shootZvel = forceZvel;
    }
    else
        g_overrideShootZvel = 0;

    int    shootAng;
    vec3_t startPos;

    if (playerNum != -1)
    {
        startPos = pPlayer->pos;
        startPos.z += pPlayer->pyoff + ZOFFSET6;
        shootAng = fix16_to_int(pPlayer->q16ang);
        pPlayer->crack_time = 777;
    }
    else
    {
        shootAng = pSprite->ang;
        startPos = pSprite->xyz;
        startPos.z -= (((pSprite->yrepeat * tilesiz[pSprite->picnum].y) << 1) - ZOFFSET6);

        if (pSprite->picnum != ROTATEGUN)
        {
            startPos.z -= (7 << 8);

            if (A_CheckEnemySprite(pSprite) && PN(spriteNum) != COMMANDER)
            {
                startPos.x += (sintable[(shootAng + 1024 + 96) & 2047] >> 7);
                startPos.y += (sintable[(shootAng + 512 + 96) & 2047] >> 7);
            }
        }
    }

#ifdef EDUKE32_STANDALONE
    return A_CheckSpriteTileFlags(projecTile, SFLAG_PROJECTILE) ? A_ShootCustom(spriteNum, projecTile, shootAng, &startPos) : -1;
#else
    return A_CheckSpriteTileFlags(projecTile, SFLAG_PROJECTILE)
         ? A_ShootCustom(spriteNum, projecTile, shootAng, &startPos)
         : !FURY ? A_ShootHardcoded(spriteNum, projecTile, shootAng, startPos, pSprite, playerNum, pPlayer) : -1;
#endif
}