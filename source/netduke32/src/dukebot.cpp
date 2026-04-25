//-------------------------------------------------------------------------
/*
Copyright (C) 1996, 2003 - 3D Realms Entertainment
Copyright (C) 2000, 2003 - Matt Saettler (EDuke Enhancements)
Copyright (C) 2004, 2007 - EDuke32 developers
Copyright (C) 2022, 2023 - Jordon Moss (Aka. StrikerTheHedgefox)

This file is part of EDuke32 / NetDuke32

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
#include "dukebot.h"
#include "dnames.h"

DukeBotAI_t botAI[MAXPLAYERS];
int32_t botNameSeed;
//input_t botInput[MAXPLAYERS];

// Keep these 10 chars or under.
const char* bot_names[] = {
    "Bweebol",      "Chubbs",       "DickKikem",    "Balzac",
    "2Phat",        "Dookie",       "Manhandlr",    "MurderBot",
    "Lo Wang",      "Caleb",        "Bombshell",    "Scrotal",
    "Arnie",        "Termin8r",     "SteelBalz",    "BotMcBot",
    "Aimbot",       "DodgyMF",      "Devast8r",     "Overlord",
    "Cycloid",      "Lenoman",      "BotHausen",    "CrazyJoe",
    "BoSchitt",     "Duke4.bot",    "AliensSuk",    "Protozoid",
    "Sgt.Farts",    "LigmaBot",     "Zuckerbrg",    "BogosBint",
    "Dr.Proton",    "Rigelatin",    "Deez Nuts",    "GrsyMagic",
    "Aniki",        "Darkholme",    "Ricardo",      "Gachi",
    "CV-96",        "DEADBEEF",     "DukeBurgr",    "Mr. Ass",
    "SukItDown",    "FISTOSHIT",    "Trash Man",    "DethKlok",
    "Pickles",      "Skwisgaar",    "Wartooth",     "MurdrFace",
    "Explosion",    "X2P1158",      "HAL 9000",     "DNCASHMAN",
    "Zilla",        "Chernobog",    "Cerberus",     "PIZOFCHET",
    "Dreemurr",     "Homer",        "Liztroop",     "Enforcer",
    "Pigcop",       "MaknBacon",    "IcedPenis",    "Bazinga",
    "Tile 2200",    "Dylan",        "Taradino",     "Lorelei",
    "Thi",          "Doug",         "Ian",          "El Oscuro",
    "Yogi",         "Yuri",         "Vinny",        "Ringo",
    "Hank",         "Quintin",      "Orion",        "Phobos",
    "Electric6",    "Carnage",      "Marvin",       "P. Dood",
    "Covfefe",      "Gorefield",    "Zardoz",       "SEENINE",
    "OOZFILTER",    "Meat",         "Sponge",       "Pretzel",
    "Booti",        "Capussi",      "Scoot",        "Fromage",
    "DankyKang",    "Wicket",       "HeThicc",      "V-Dub",
    "Robotnik",     "WollSmoth",    "AAAAAAAAA",    "PlzHelpMe",
    "Dio",          "VargFren",     "Pantherk",     "Luigo",
    "JankOClok",    "CancrMaus",    "H4MMER",       "Gideon",
    "BOTYOT",       "DUKEYOT",      "ABADBABE",     "BEEFBABE",
    "DEADC0DE",     "DEFEC8ED",     "D0D0CACA",     "1337C0D3",
    "Heskel",       "Bort",         "Kirk",         "Picard",
    "Gnorts",       "FatComm&r",    "NEWBEAST",     "BATTLORD",
};

// This matrix is used to determine how far a dukebot will engage another player
// based on what weapon they're carrying, and what weapon their opponent is using.
static int32_t fdmatrix[MAX_WEAPONS][MAX_WEAPONS] =
{
    //KNEE PIST SHOT CHAIN RPG PIPE SHRI DEVI TRIP FREE HAND EXPA <- ENEMY v US
    {  128,  -1,  -1,  -1, 128,  -1,  -1,  -1, 128,  -1, 128,  -1 },   //KNEE
    { 1024,1024,1024,1024,2560, 128,2560,2560,1024,2560,2560,2560 },   //PIST
    {  512, 512, 512, 512,2560, 128,2560,2560,1024,2560,2560,2560 },   //SHOT
    {  512, 512, 512, 512,2560, 128,2560,2560,1024,2560,2560,2560 },   //CHAIN
    { 2560,2560,2560,2560,2560,2560,2560,2560,2560,2560,2560,2560 },   //RPG
    {  512, 512, 512, 512,2048, 512,2560,2560, 512,2560,2560,2560 },   //PIPE
    {  128, 128, 128, 128,2560, 128,2560,2560, 128, 128, 128, 128 },   //SHRI
    { 1536,1536,1536,1536,2560,1536,1536,1536,1536,1536,1536,1536 },   //DEVI
    {   -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 },   //TRIP
    {  128, 128, 128, 128,2560, 128,2560,2560, 128, 128, 128, 128 },   //FREE
    { 2560,2560,2560,2560,2560,2560,2560,2560,2560,2560,2560,2560 },   //HAND
    {  128, 128, 128, 128,2560, 128,2560,2560, 128, 128, 128, 128 }    //EXPA
};

static bool DukeBot_CanSee(vec3_t* pos1, int16_t sect1, vec3_t* pos2, int16_t sect2)
{
    return (cansee(pos1->x, pos1->y, pos1->z, sect1, pos2->x, pos2->y, pos2->z, sect2, CSTAT_WALL_1WAY | CSTAT_WALL_BLOCK_HITSCAN) ||
            cansee(pos1->x, pos1->y, pos1->z - (24 << 8), sect1, pos2->x, pos2->y, pos2->z - (24 << 8), sect2, CSTAT_WALL_1WAY | CSTAT_WALL_BLOCK_HITSCAN) ||
            cansee(pos1->x, pos1->y, pos1->z - (48 << 8), sect1, pos2->x, pos2->y, pos2->z - (48 << 8), sect2, CSTAT_WALL_1WAY | CSTAT_WALL_BLOCK_HITSCAN));
}

static bool DukeBot_CanSeeItem(vec3_t* pos1, int16_t sect1, int32_t spriteNum)
{
    auto spr = &sprite[spriteNum];
    return (cansee(pos1->x, pos1->y, pos1->z - (24 << 8), sect1, spr->x, spr->y, spr->z - (4 << 8), spr->sectnum, CSTAT_WALL_BLOCK) ||
            cansee(pos1->x, pos1->y, pos1->z - (48 << 8), sect1, spr->x, spr->y, spr->z - (4 << 8), spr->sectnum, CSTAT_WALL_BLOCK) ||
            cansee(pos1->x, pos1->y, pos1->z - (72 << 8), sect1, spr->x, spr->y, spr->z - (4 << 8), spr->sectnum, CSTAT_WALL_BLOCK));
}

static int32_t weapScore[MAX_WEAPONS][2] =
{
    // Weapon, Ammo
    { 0,    0 },
    { 20,   10 },    // PISTOL_WEAPON
    { 120,  25 },    // SHOTGUN_WEAPON
    { 50,   20 },    // CHAINGUN_WEAPON
    { 200,  50 },    // RPG_WEAPON
    { 60,   30 },    // HANDBOMB_WEAPON
    { 80,   10 },    // SHRINKER_WEAPON
    { 120,  25 },    // DEVISTATOR_WEAPON
    { 50,   10 },    // TRIPBOMB_WEAPON
    { 25,   10 },    // FREEZE_WEAPON
    { 0,    0  },    // HANDREMOTE_WEAPON
    { 200,  50 },    // GROW_WEAPON
};

static int32_t DukeBot_GetWeaponScore(int32_t pNum, int32_t spriteNum, int32_t weapon, bool ammo_only)
{
    auto const p = g_player[pNum].ps;
    
    if (ammo_only)
        return (p->ammo_amount[weapon] < p->max_ammo_amount[weapon]) ? weapScore[weapon][1] : 0;

    // Weapon stay on, we got this weapon, and it's map-spawned? Ignore.
    if (GTFLAGS(GAMETYPE_WEAPSTAY) && p->gotweapon[weapon] && (actor[spriteNum].htpicnum == sprite[spriteNum].picnum))
        return 0;

    if (p->gotweapon[weapon] && (p->ammo_amount[weapon] >= p->max_ammo_amount[weapon]))
        return 0;

    // If we have the weapon, treat it like ammo. If not, prioritize higher.
    return (p->gotweapon[weapon]) ? weapScore[weapon][1] : weapScore[weapon][0];
}

// Determines the priority of pickups for a bot.
static int DukeBot_GetSpriteScore(int pNum, int32_t spriteNum)
{
    auto const p = g_player[pNum].ps;
    int32_t score = 0;

    switch (tileGetMapping(sprite[spriteNum].picnum))
    {
        case FIRSTGUNSPRITE__:    score = DukeBot_GetWeaponScore(pNum, spriteNum, PISTOL_WEAPON, false); break;
        case RPGSPRITE__:         score = DukeBot_GetWeaponScore(pNum, spriteNum, RPG_WEAPON, false); break;
        case FREEZESPRITE__:      score = DukeBot_GetWeaponScore(pNum, spriteNum, FREEZE_WEAPON, false); break;
        case SHRINKERSPRITE__:    score = DukeBot_GetWeaponScore(pNum, spriteNum, SHRINKER_WEAPON, false); break;
        case HEAVYHBOMB__:        score = DukeBot_GetWeaponScore(pNum, spriteNum, HANDBOMB_WEAPON, false); break;
        case CHAINGUNSPRITE__:    score = DukeBot_GetWeaponScore(pNum, spriteNum, CHAINGUN_WEAPON, false); break;
        case TRIPBOMBSPRITE__:    score = DukeBot_GetWeaponScore(pNum, spriteNum, TRIPBOMB_WEAPON, false); break;
        case SHOTGUNSPRITE__:     score = DukeBot_GetWeaponScore(pNum, spriteNum, SHOTGUN_WEAPON, false); break;
        case DEVISTATORSPRITE__:  score = DukeBot_GetWeaponScore(pNum, spriteNum, DEVISTATOR_WEAPON, false); break;

        case FREEZEAMMO__:        score = DukeBot_GetWeaponScore(pNum, spriteNum, FREEZE_WEAPON, true); break;
        case AMMO__:              score = DukeBot_GetWeaponScore(pNum, spriteNum, PISTOL_WEAPON, true); break;
        case BATTERYAMMO__:       score = DukeBot_GetWeaponScore(pNum, spriteNum, CHAINGUN_WEAPON, true); break;
        case DEVISTATORAMMO__:    score = DukeBot_GetWeaponScore(pNum, spriteNum, DEVISTATOR_WEAPON, true); break;
        case RPGAMMO__:           score = DukeBot_GetWeaponScore(pNum, spriteNum, RPG_WEAPON, true); break;
        case CRYSTALAMMO__:       score = DukeBot_GetWeaponScore(pNum, spriteNum, SHRINKER_WEAPON, true); break;
        case GROWAMMO__:          score = DukeBot_GetWeaponScore(pNum, spriteNum, GROW_WEAPON, true); break;
        case HBOMBAMMO__:         score = DukeBot_GetWeaponScore(pNum, spriteNum, HANDBOMB_WEAPON, true); break;
        case SHOTGUNAMMO__:       score = DukeBot_GetWeaponScore(pNum, spriteNum, SHOTGUN_WEAPON, true); break;

        case COLA__:
            if (sprite[p->i].extra < 50)
                score = 20;
            else if (sprite[p->i].extra < p->max_player_health)
                score = 10;
            break;
        case SIXPAK__:
            if (sprite[p->i].extra < 50)
                score = 60;
            else if (sprite[p->i].extra < p->max_player_health)
                score = 30;
            break;
        case FIRSTAID__:
            if (p->inv_amount[GET_FIRSTAID] < p->max_player_health)
                score = 100;
            break;
        case SHIELD__:
            if (p->inv_amount[GET_SHIELD] < p->max_shield_amount)
                score = 50;
            break;
        case STEROIDS__:
            if (p->inv_amount[GET_STEROIDS] < 400)
                score = 30;
            break;
        case AIRTANK__:
            if (p->inv_amount[GET_SCUBA] < 6400)
                score = 30;
            break;
        case JETPACK__:
            if (p->inv_amount[GET_JETPACK] < 1600)
                score = 100;
            break;
        case HEATSENSOR__:
            if (p->inv_amount[GET_HEATS] < 1200)
                score = 5;
            break;
        case ACCESSCARD__:
            score = 1;
            break;
        case BOOTS__:
            if (p->inv_amount[GET_BOOTS] < 200)
                score = 15;
            break;
        case ATOMICHEALTH__:
            if (sprite[p->i].extra < 50)
                score = 100;
            else if (sprite[p->i].extra < p->max_player_health << 1)
                score = 50;
            break;
        case HOLODUKE__:
            if (p->inv_amount[GET_HOLODUKE] < 2400)
                score = 5;
            break;
        case RESPAWNMARKERGREEN__:
            score = 1;
            break;
    }

    return X_OnEventWithReturn(EVENT_BOTGETSPRITESCORE, spriteNum, pNum, score);
}

static bool DukeBot_IsSpriteValid(int32_t spriteNum)
{
    auto const spr = &sprite[spriteNum];
    if ((uint32_t)spriteNum >= MAXSPRITES)
        return false;

    return !((spr->xrepeat <= 0) || (spr->yrepeat <= 0) || ((spr->cstat & 32768) == 32768) || (spr->statnum == MAXSTATUS));
}

static int32_t DukeBot_GetAngleForWall(int16_t wNum)
{
    if ((uint16_t)wNum >= MAXWALLS)
        return 0;

    auto wall1 = &wall[wNum];
    auto wall2 = &wall[wall1->point2];

    return (getangle(wall2->x - wall1->x, wall2->y - wall1->y) + 1536) & 2047;
}

static bool DukeBot_CheckUsableSector(int16_t sectNum)
{
    if ((uint16_t)sectNum >= MAXSECTORS)
        return false;

    if (sector[sectNum].lotag & 16384) // Sector's locked.
        return false;

    switch (sector[sectNum].lotag)
    {
        case ST_15_WARP_ELEVATOR:
        case ST_16_PLATFORM_DOWN:
        case ST_17_PLATFORM_UP:
        case ST_18_ELEVATOR_DOWN:
        case ST_19_ELEVATOR_UP:
        case ST_21_FLOOR_DOOR:
        case ST_28_DROP_FLOOR:
        case ST_30_ROTATE_RISE_BRIDGE:
        {
            if (GetAnimationGoal(sectNum, SECTANIM_FLOOR) == -1)
                return true;

            break;
        }
        case ST_20_CEILING_DOOR:
        case ST_22_SPLITTING_DOOR:
        case ST_29_TEETH_DOOR:
        {
            if (GetAnimationGoal(sectNum, SECTANIM_CEILING) == -1)
                return true;

            break;
        }
        case ST_23_SWINGING_DOOR:
        {
            return true;
        }
    }

    return false;
}

static int32_t DukeBot_GetDistWithDummy(int32_t playerNum, vec3_t* pos)
{
    auto const p = g_player[playerNum].ps;
    auto pSpr = &sprite[p->i];

    int32_t dist1 = dist(&pSpr->xyz, pos);

    if (p->dummyplayersprite != -1)
    {
        pSpr = &sprite[p->dummyplayersprite];
        int32_t dist2 = dist(&pSpr->xyz, pos);
        if (dist2 < dist1)
            return dist2;
    }

    return dist1;
}

// [JM] I think this needs better documentation.
// Still having a hard time figuring out everything this does.
int16_t searchsect[MAXSECTORS], searchparent[MAXSECTORS];
uint8_t dashow2dsector[(MAXSECTORS + 7) >> 3];
static void DukeBot_GetNextWallForPath(int32_t playerNum, int32_t endsect)
{
    auto const bot = &botAI[playerNum];
    auto const p = g_player[playerNum].ps;
    auto const pSpr = &sprite[p->i];

    int32_t startsect = pSpr->sectnum; // Player's current sector.
    if (startsect == -1 || endsect == -1)
        return;

    // Optimization: If we have a wall, and our start/end sects haven't changed, no point in updating.
    if ((bot->goalWall != -1) && (startsect == bot->lastStartSect) && (endsect == bot->lastEndSect))
        return;

    bot->lastStartSect = startsect;
    bot->lastEndSect = endsect;

    clearbufbyte(dashow2dsector, sizeof(dashow2dsector), 0);
    searchsect[0] = startsect;
    searchparent[0] = -1;
    bitmap_set(dashow2dsector, startsect);

    for (int32_t splc = 0, send = 1; splc < send; splc++)
    {
        int16_t curSectNum = searchsect[splc];
        auto curSect = &sector[curSectNum];

        int32_t startwall = curSect->wallptr;
        int32_t endwall = startwall + curSect->wallnum;

        for (int32_t wallNum = startwall; wallNum < endwall; wallNum++)
        {
            auto wal = &wall[wallNum];
            int16_t nextSectNum = wal->nextsector;

            if (nextSectNum < 0)
                continue;

            if (wal->cstat & CSTAT_WALL_BLOCK) // Avoid blocking walls.
                continue;

            auto nSect = &sector[nextSectNum];

            vec2_t d = { ((wall[wal->point2].x + wal->x) >> 1),
                         ((wall[wal->point2].y + wal->y) >> 1) };

            int32_t curFloorZ = yax_getflorzofslope(curSectNum, d);
            int32_t nextFloorZ = nSect->floorz;
            int32_t nextCeilZ = nSect->ceilingz;

            if ((nSect->floorstat | nSect->ceilingstat) & FLOOR_STAT_SLOPE)
                yax_getzsofslope(nextSectNum, d.x, d.y, &nextCeilZ, &nextFloorZ);

            // Ceiling too low, and sector is locked or not usable.
            if (nextCeilZ > nextFloorZ - (28 << 8))
            {
                if ((nSect->lotag & 16384) || (nSect->lotag < ST_15_WARP_ELEVATOR) || (nSect->lotag > ST_22_SPLITTING_DOOR))
                    continue;
            }
           
            // Can't float or fly?
            if (curSect->lotag != ST_2_UNDERWATER && p->inv_amount[GET_JETPACK] < 106)
            {
                if (nextFloorZ < curFloorZ - (72 << 8)) // Floor too high for sector we're searching from?
                {
                    // Locked, or not a usable sector.
                    if ((nSect->lotag & 16384) || !((nSect->lotag >= ST_15_WARP_ELEVATOR && nSect->lotag <= ST_19_ELEVATOR_UP) || (nSect->lotag == ST_21_FLOOR_DOOR) || (nSect->lotag == ST_22_SPLITTING_DOOR)))
                    {
                        if((curSect->lotag & 16384) || !(curSect->lotag >= ST_15_WARP_ELEVATOR && curSect->lotag <= ST_19_ELEVATOR_UP))
                            continue;
                    }
                }
            }

            if(!bitmap_test(dashow2dsector, nextSectNum))
            {
                bitmap_set(dashow2dsector, nextSectNum);
                searchsect[send] = (int16_t)nextSectNum;
                searchparent[send] = (int16_t)splc;
                send++;
                if (nextSectNum == endsect)
                {
                    clearbufbyte(dashow2dsector, sizeof(dashow2dsector), 0);

                    int16_t k; // Need a good name for this and description of WTF thes two loops do.
                    for (k = send - 1; k >= 0; k = searchparent[k])
                        bitmap_set(dashow2dsector, searchsect[k]);

                    for (k = send - 1; k >= 0; k = searchparent[k])
                        if (!searchparent[k]) break;

                    int32_t desiredSect = searchsect[k];
                    startwall = sector[desiredSect].wallptr;
                    endwall = startwall + sector[desiredSect].wallnum;

                    vec2_t wPos = { 0, 0 };
                    // CHECKME: Does this break the parent loop?
                    for (int32_t wNum = startwall; wNum < endwall; wNum++)
                    {
                        wPos.x += wall[wNum].x;
                        wPos.y += wall[wNum].y;
                    }
                    wPos.x /= (endwall - startwall);
                    wPos.y /= (endwall - startwall);

                    startwall = sector[startsect].wallptr;
                    endwall = startwall + sector[startsect].wallnum;
                    int32_t lastDist = 0;
                    int32_t targetWall = startwall;

                    // CHECKME: Does this break the parent loop?
                    for (int32_t wNum = startwall; wNum < endwall; wNum++)
                    {
                        if (wall[wNum].nextsector != desiredSect)
                            continue;

                        vec2_t d = { wall[wall[wNum].point2].x - wall[wNum].x,
                                     wall[wall[wNum].point2].y - wall[wNum].y };

                        if ((wPos.x - pSpr->x) * (wall[wNum].y - pSpr->y) <= (wPos.y - pSpr->y) * (wall[wNum].x - pSpr->x))
                        {
                            if ((wPos.x - pSpr->x) * (wall[wall[wNum].point2].y - pSpr->y) >= (wPos.y - pSpr->y) * (wall[wall[wNum].point2].x - pSpr->x))
                            {
                                targetWall = wNum;
                                break;
                            }
                        }

                        int32_t wallDist = ksqrt(d.x * d.x + d.y * d.y);
                        if (wallDist > lastDist)
                        {
                            lastDist = wallDist;
                            targetWall = wNum;
                        }
                    }

                    int32_t wallAng = DukeBot_GetAngleForWall(targetWall);
                    bot->goalPos.x = ((wall[targetWall].x + wall[wall[targetWall].point2].x) >> 1) + (sintable[(wallAng + 512) & 2047] >> 8);
                    bot->goalPos.y = ((wall[targetWall].y + wall[wall[targetWall].point2].y) >> 1) + (sintable[wallAng & 2047] >> 8);
                    bot->goalPos.z = sector[desiredSect].floorz - (32 << 8);
                    bot->goalWall = targetWall;
                    return;
                }
            }
        }

        // Propagate search through teleporters? I think?
        for (int32_t spr = headspritesect[searchsect[splc]]; spr >= 0; spr = nextspritesect[spr])
        {
            if (sprite[spr].lotag == SE_7_TELEPORT)
            {
                int16_t sect = sprite[sprite[spr].owner].sectnum;
                
                if(!bitmap_test(dashow2dsector, sect))
                {
                    bitmap_set(dashow2dsector, sect);
                    searchsect[send] = (int16_t)sect;
                    searchparent[send] = (int16_t)splc;
                    send++;
                    if (sect == endsect)
                    {
                        clearbufbyte(dashow2dsector, sizeof(dashow2dsector), 0);

                        int16_t k; // Need a good name for this and description of WTF thes two loops do.
                        for (k = send - 1; k >= 0; k = searchparent[k])
                            bitmap_set(dashow2dsector, searchsect[k]);

                        for (k = send - 1; k >= 0; k = searchparent[k])
                            if (!searchparent[k]) break;

                        int32_t desiredSect = searchsect[k];
                        startwall = sector[startsect].wallptr;
                        endwall = startwall + sector[startsect].wallnum;

                        int32_t lastDist = 0;
                        int32_t targetWall = startwall;
                        for (int32_t wNum = startwall; wNum < endwall; wNum++)
                        {
                            vec2_t d = { wall[wall[wNum].point2].x - wall[wNum].x,
                                         wall[wall[wNum].point2].y - wall[wNum].y };

                            int32_t wallDist = ksqrt(d.x * d.x + d.y * d.y);
                            if ((wall[wNum].nextsector == desiredSect) && (wallDist > lastDist))
                            {
                                lastDist = wallDist;
                                targetWall = wNum;
                            }

                        }

                        int32_t wallAng = DukeBot_GetAngleForWall(targetWall);
                        bot->goalPos.x = ((wall[targetWall].x + wall[wall[targetWall].point2].x) >> 1) + (sintable[(wallAng + 512) & 2047] >> 8);
                        bot->goalPos.y = ((wall[targetWall].y + wall[wall[targetWall].point2].y) >> 1) + (sintable[wallAng & 2047] >> 8);
                        bot->goalPos.z = sector[desiredSect].floorz - (32 << 8);
                        bot->goalWall = targetWall;
                        return;
                    }
                }
            }
        }
    }
}

static void DukeBot_Init(int32_t playerNum)
{
    auto const bot = &botAI[playerNum];

    bot->goalPlayer = playerNum;
    bot->goalWall = -1;
    bot->goalSprite = -1;
    bot->goalSpriteScore = 0;
    bot->lastGoalSprite = -1;
    bot->lastStartSect = bot->lastEndSect = -1;

    bot->moveCount = bot->cycleCount = 0;
    
    bot->seekSpriteCooldown = bot->seekSpriteFailCount = 0;
    bot->waterSubmergeTime = 0;

    bot->rand = 1996 + playerNum;

    bot->goalPos = g_player[playerNum].ps->pos;
    for(int32_t ALL_PLAYERS(i))
        bot->lastSeenPlayerPos[i] = g_player[i].ps->pos;
}

void DukeBot_Activate(int32_t playerNum)
{
    auto const bot = &botAI[playerNum];
    if (bot->active || !g_player[playerNum].isBot)
        return;

    DukeBot_Init(playerNum);

    snprintf(g_player[playerNum].user_name, sizeof(g_player[0].user_name), "*%s", bot_names[(seed_krand(&botNameSeed)+playerNum) % ARRAY_SIZE(bot_names)]);

    bot->active = 1;
}

void DukeBot_ResetAll()
{
    for(int32_t ALL_PLAYERS(playerNum))
    {
        DukeBotAI_t* bot = &botAI[playerNum];
        if (!g_player[playerNum].isBot)
            continue;

        if (bot->active)
            DukeBot_Init(playerNum);
        else if (ud.playerai)
            DukeBot_Activate(playerNum);
    }
}

void DukeBot_GetInput(int snum, input_t* syn)
{
    auto const bot = &botAI[snum];
    auto const p = g_player[snum].ps;
    auto const pSpr = &sprite[p->i];
    auto goalp = g_player[bot->goalPlayer].ps;

    syn->fvel = 0;
    syn->svel = 0;
    syn->q16avel = 0;
    syn->q16horz = 0;
    syn->bits = 0;
    syn->extbits = 0;

    if (!bot->active || ud.limit_hit || ud.pause_on)
        return;

    if (p->dead_flag)
    {
        bot->respawnTime++;

        if ((bot->respawnTime >= BOT_MAX_RESPAWN_TIME) || (bot->respawnTime >= BOT_MIN_RESPAWN_TIME && !(seed_krand(&bot->rand) % 64)))
            syn->bits |= BIT(SK_OPEN);

        return;
    }

    bot->respawnTime = 0;

    if (pSpr->sectnum == -1) // If we're in void, there's nothing else we can do. Bail.
        return;

    // If we can't see our target, or, given a small random chance, find someone else.
    if ((bot->cycleCount % 8) == 0)
    {
        vec3_t testPos = sprite[goalp->i].xyz;
        if(!DukeBot_CanSee(&pSpr->xyz, pSpr->sectnum, &testPos, sprite[goalp->i].sectnum) || !(seed_krand(&bot->rand) % 512))
        {
            bot->goalPlayer = snum;
            goalp = g_player[snum].ps;
        }
    }

    if ((bot->goalPlayer == snum) || goalp->dead_flag)
    {
        int32_t lastDist = INT32_MAX;

        if (!bot->reactionTime)
            bot->reactionTime = BOT_MIN_REACTION_TIME + (seed_krand(&bot->rand) % ((BOT_MAX_REACTION_TIME + 1) - BOT_MIN_REACTION_TIME));

        for (int32_t ALL_PLAYERS(otherPlayer))
        {
            auto pOther = g_player[otherPlayer].ps;
            if (otherPlayer != snum && !(GTFLAGS(GAMETYPE_TDM) && p->team == pOther->team) && !pOther->dead_flag)
            {
                int32_t curDist = DukeBot_GetDistWithDummy(snum, &sprite[pOther->i].xyz);

                if (!DukeBot_CanSee(&pSpr->xyz, pSpr->sectnum, &sprite[pOther->i].xyz, sprite[pOther->i].sectnum))
                    curDist <<= 1;
                else
                    bot->lastSeenPlayerPos[otherPlayer] = sprite[pOther->i].xyz;

                if (pOther->dummyplayersprite != -1)
                {
                    auto pOtherDummySpr = &sprite[pOther->dummyplayersprite];
                    int32_t dummyDist = dist(&pSpr->xyz, &pOtherDummySpr->xyz);

                    if (!DukeBot_CanSee(&pSpr->xyz, pSpr->sectnum, &pOtherDummySpr->xyz, pOtherDummySpr->sectnum))
                        dummyDist <<= 1;

                    if (dummyDist < curDist)
                        curDist = dummyDist;
                }

                if (curDist < lastDist)
                {
                    lastDist = curDist;
                    bot->goalPlayer = otherPlayer;
                    goalp = g_player[otherPlayer].ps;
                }
            }
        }
    }

    // Look for items to pick up.
    if (bot->seekSpriteCooldown > 0)
        bot->seekSpriteCooldown--;

    if (bot->goalSprite != -1)
    {
        if (bot->seekSpriteFailCount > BOT_SEEK_GIVEUP || !DukeBot_IsSpriteValid(bot->goalSprite)) // Can't see or reach, abandon.
        {
#if BOT_DEBUG == 1
            OSD_Printf("^06DukeBot %d (%s)^15: Goal sprite unreachable for too long, or is gone. Abandoning.\n", snum, g_player[snum].user_name);
#endif
            bot->lastGoalSprite = bot->goalSprite;
            bot->goalSprite = -1;
            bot->goalSpriteScore = 0;
            bot->seekSpriteFailCount = 0;
            bot->seekSpriteCooldown = 30;
        }      
    }

    if (!bot->seekSpriteCooldown)
    {
        // No goal sprite, or haven't seen existing goal sprite for a while.
        if (bot->goalSprite == -1 || (bot->seekSpriteFailCount > BOT_SEEK_LOOKFORBETTER))
        {
#if BOT_DEBUG == 1
            if (bot->goalSprite != -1)
                OSD_Printf("^06DukeBot %d (%s)^15: Looking for higher scoring sprite.\n", snum, g_player[snum].user_name);
            else
                OSD_Printf("^06DukeBot %d (%s)^15: Looking for new goal sprite.\n", snum, g_player[snum].user_name);
#endif

            static int32_t stat_list[] = { STAT_ACTOR, STAT_ZOMBIEACTOR };
            int32_t bestSprite = -1, bestScore = 0;
            int32_t closestSprite = -1, closestScore = 0, closestDist = INT32_MAX;

            for (int32_t i = 0; i < (int32_t)ARRAY_SIZE(stat_list); i++)
            {
                int32_t spriteNum;
                for (SPRITES_OF(stat_list[i], spriteNum))
                {
                    auto spr = &sprite[spriteNum];
                    if ((spriteNum == bot->lastGoalSprite) || !DukeBot_IsSpriteValid(spriteNum))
                        continue;

                    int32_t curScore = DukeBot_GetSpriteScore(snum, spriteNum);
                    if (curScore <= 0)
                        continue;

                    int32_t curDist = DukeBot_GetDistWithDummy(snum, &spr->xyz);
                    if (curDist < closestDist) // Which is closer?
                    {
                        closestSprite = spriteNum;
                        closestScore = curScore;
                        closestDist = curDist;
                    }

                    if (!DukeBot_CanSeeItem(&pSpr->xyz, pSpr->sectnum, spriteNum))
                        continue;
                    
                    if (curScore > bestScore)
                    {
                        bestScore = curScore;
                        bestSprite = spriteNum;
                    }
                }
            }

            // Already have a sprite, or can't find best sprite, but found something close of equal or greater value.
            if ((bot->goalSprite != -1 || bestSprite == -1) && (closestSprite != -1) && (closestScore >= bot->goalSpriteScore))
            {
#if BOT_DEBUG == 1
                OSD_Printf("^06DukeBot %d (%s)^15: Getting closest sprite.\n", snum, g_player[snum].user_name);
#endif
                bot->goalSprite = closestSprite;
                bot->goalSpriteScore = closestScore;
                //bot->seekSpriteFailCount = 0;
                bot->seekSpriteCooldown = 30;
            }
            else if (bestSprite != -1 && (bot->goalSprite == -1 || (bestScore > bot->goalSpriteScore))) // Found best sprite in sight, or didn't have one.
            {
#if BOT_DEBUG == 1
                OSD_Printf("^06DukeBot %d (%s)^15: New best sprite acquired.\n", snum, g_player[snum].user_name);
#endif
                bot->goalSprite = bestSprite;
                bot->goalSpriteScore = bestScore;
                bot->seekSpriteFailCount = 0;
                bot->seekSpriteCooldown = 30;
            }
            else
            {
#if BOT_DEBUG == 1
                OSD_Printf("^06DukeBot %d (%s)^15: No sprite found.\n", snum, g_player[snum].user_name);
#endif
                bot->seekSpriteCooldown = 30;
            }
        }
    }

    // Test for collisions
    vec3_t curPos = p->pos;
    int16_t curSect = p->cursectnum;
    int32_t moveResult = clipmove(&curPos, &curSect, p->vel.x, p->vel.y, 164L, 4L << 8, 4L << 8, CLIPMASK0);

    if (!moveResult)
    {
        curPos = p->pos;
        curPos.z += (24 << 8);
        curSect = p->cursectnum;
        moveResult = clipmove(&curPos, &curSect, p->vel.x, p->vel.y, 164L, 4L << 8, 4L << 8, CLIPMASK0);
    }

    // Now that we have all the info we need, let's put it all together.
    auto const pSect = &sector[pSpr->sectnum];
    auto const gpSpr = &sprite[goalp->i];
    auto const gItemSpr = (bot->goalSprite != -1) ? &sprite[bot->goalSprite] : NULL;

    bool const touchingWall = ((moveResult & 49152) == 32768);
    bool const touchingSprite = ((moveResult & 49152) == 49152);
    bool const seePlayer = ((bot->goalPlayer != snum) && !goalp->dead_flag && DukeBot_CanSee(&pSpr->xyz, pSpr->sectnum, &gpSpr->xyz, gpSpr->sectnum));
    bool const seeSprite = (gItemSpr) ? DukeBot_CanSeeItem(&pSpr->xyz, pSpr->sectnum, bot->goalSprite) : false;
    bool const prioritizeItems = ((pSpr->extra <= 50) || (p->curr_weapon == KNEE_WEAPON && p->inv_amount[GET_STEROIDS] == 0) || (p->ammo_amount[p->curr_weapon] <= (p->max_ammo_amount[p->curr_weapon] >> 3)));

    int32_t const gpDist = max(ldist(&pSpr->xyz, &gpSpr->xyz), 1);
    int32_t const sprDist = (bot->goalSprite != -1) ? max(ldist(&pSpr->xyz, &sprite[bot->goalSprite].xyz), 1) : INT32_MAX;

    int32_t moveFlags = 0;
    int32_t combatFlags = 0;

    // Search sectors.
    int32_t endsect = gpSpr->sectnum;
    if (gItemSpr)
    {
        if (prioritizeItems || (seeSprite && (sprDist < gpDist)) || (!seeSprite && (sprDist<<4 < gpDist)))
            endsect = gItemSpr->sectnum;
    }
    
    DukeBot_GetNextWallForPath(snum, endsect);

    // Get Angle
    int32_t faceAngle = getangle(bot->goalPos.x - pSpr->x, bot->goalPos.y - pSpr->y);

    if (!seePlayer && !bot->reactionTime)
        bot->reactionTime = BOT_MIN_REACTION_TIME+(seed_krand(&bot->rand) % ((BOT_MAX_REACTION_TIME+1)-BOT_MIN_REACTION_TIME));

    // Move, do stuff. (or override angle if need be).
    if (seePlayer)
    {
        int32_t fightDist = max(fdmatrix[p->curr_weapon][goalp->curr_weapon], 128);
        int32_t zAngle = 100 - ((gpSpr->z - pSpr->z) * 8) / gpDist;
        int32_t angleDelta = G_GetAngleDelta(pSpr->ang, getangle(gpSpr->x - pSpr->x, gpSpr->y - pSpr->y));

        hitdata_t hit;
        hitscan(&p->pos, p->cursectnum, sintable[(pSpr->ang + 512) & 2047], sintable[pSpr->ang & 2047],
            fix16_to_int(F16(100) - p->q16horiz - p->q16horizoff) * 32, &hit, CLIPMASK1);

        faceAngle = getangle(gpSpr->x - pSpr->x, gpSpr->y - pSpr->y);
        bot->lastSeenPlayerPos[bot->goalPlayer] = gpSpr->xyz;

        if (bot->reactionTime > 0) // We haven't had time to react yet.
            bot->reactionTime--;
        else if(klabs(angleDelta) < 1024) // Shoot when we're roughly facing our target.
            syn->bits |= BIT(SK_FIRE);

        // Handle special weapon cases.
        if (p->curr_weapon == RPG_WEAPON || p->curr_weapon == DEVISTATOR_WEAPON)
        {
            if (ldist(&pSpr->xyz, &hit.xyz) < (g_rpgBlastRadius + 512))
                syn->bits &= ~BIT(SK_FIRE);
        }

        if ((p->curr_weapon == HANDBOMB_WEAPON) && (!(seed_krand(&bot->rand) & 7)))
            syn->bits &= ~BIT(SK_FIRE);

        if ((gpSpr->yrepeat < 32) || (gpSpr->pal == 1)) // Target shrunk or frozen?
        {
            combatFlags |= BOT_COMBAT_STOMP;
            syn->bits &= ~BIT(SK_FIRE);
            fightDist = 0;
        }

        if (fightDist > 0)
            moveFlags |= BOT_MOVE_STRAFE;

        if (fightDist <= 128)
            combatFlags |= BOT_COMBAT_MELEE;

        // Back up for targets too far above us.
        if(!(combatFlags & (BOT_COMBAT_MELEE | BOT_COMBAT_STOMP)))
            fightDist = max(fightDist, (klabs(gpSpr->z - pSpr->z) >> 4));

        if (!prioritizeItems || (combatFlags & (BOT_COMBAT_MELEE | BOT_COMBAT_STOMP)))
        {
            vec2_t fightPos = { (gpSpr->x + ((pSpr->x - gpSpr->x) * fightDist / gpDist)),
                                (gpSpr->y + ((pSpr->y - gpSpr->y) * fightDist / gpDist)) };

            syn->fvel += (fightPos.x - pSpr->x) * 2047 / gpDist;
            syn->svel += (fightPos.y - pSpr->y) * 2047 / gpDist;
        }

        if (gpDist > 980 && p->curr_weapon == KNEE_WEAPON) // Don't kick if target is too far. Prevents boot slowdown.
            syn->bits &= ~BIT(SK_FIRE);
        else if (gpDist < 980 && p->curr_weapon != KNEE_WEAPON) // We're close, give 'em the boot.
            syn->bits |= BIT(SK_QUICK_KICK);

#if 0
        syn->bits &= ~BIT(SK_FIRE); // DEBUG: STOP SHOOTING, AND HOLD STILL, DAMNIT.
        moveFlags &= ~BOT_MOVE_STRAFE;
#endif
        if (!bot->reactionTime) // If we've had time to react, start turning.
        {
            syn->q16horz = min(max(fix16_from_int(zAngle) - p->q16horiz, F16(-MAXHORIZ)), F16(MAXHORIZ));
            syn->bits |= BIT(SK_AIMMODE);
        }
    }

    bool const goForItem = seeSprite && !((combatFlags & BOT_COMBAT_MELEE && (gpDist < (prioritizeItems ? sprDist : sprDist << 1))) || combatFlags & BOT_COMBAT_STOMP);

    if (goForItem)
    {
        if(!seePlayer)
            faceAngle = getangle(gItemSpr->x - pSpr->x, gItemSpr->y - pSpr->y);

        if (sprDist < 96) // Pretty much on top of it, so let it go.
            bot->seekSpriteFailCount += 30;

        syn->fvel += ((gItemSpr->x - pSpr->x) * 2047) / sprDist;
        syn->svel += ((gItemSpr->y - pSpr->y) * 2047) / sprDist;

#if BOT_DEBUG == 2
        OSD_Printf("DukeBot #%d (%s): Pursuing sprite (Dist: %d)\n", snum, g_player[snum].user_name, sprDist);
#endif
    }
    else if(!seeSprite)
        bot->seekSpriteFailCount++;

    if (bot->goalWall >= 0)
    {
        int32_t goalDist = max(ldist(&pSpr->xyz, &bot->goalPos), 1);

        if (!seePlayer && (bot->goalWall >= 0) && (goalDist < 4096)) // Face wall if we're close and we're targeting one.
            faceAngle = DukeBot_GetAngleForWall(bot->goalWall);

#if  0
        // [JM]: Close to our goal wall, start a new search.
        // NOTE: I want to change this to an intersection check with the player's radius, so it's a bit more... accurate? I guess?
        //       Someone will have to teach me how to do this, because I'm dumb as shit.
        if (goalDist < 1024)
            bot->goalWall = -1;
#endif

        if (!seeSprite && !(combatFlags & BOT_COMBAT_MELEE))
        {
            syn->fvel += ((bot->goalPos.x - pSpr->x) * 2047) / goalDist;
            syn->svel += ((bot->goalPos.y - pSpr->y) * 2047) / goalDist;
        }
    }
    else if (!seeSprite && !seePlayer)
    {
        auto lsPos = &bot->lastSeenPlayerPos[bot->goalPlayer];

        faceAngle = getangle(lsPos->x - pSpr->x, lsPos->y - pSpr->y);

        int32_t lastSeenDist = max(ldist(&pSpr->xyz, lsPos), 1);
        syn->fvel += ((lsPos->x - pSpr->x) * 2047) / lastSeenDist;
        syn->svel += ((lsPos->y - pSpr->y) * 2047) / lastSeenDist;

#if BOT_DEBUG == 1
        OSD_Printf("DukeBot #%d (%s): ^02Cannot see player %d, heading to XYZ: %d, %d, %d (Dist: %d)\n", snum, g_player[snum].user_name, bot->goalPlayer, lsPos->x, lsPos->y, lsPos->z, lastSeenDist);
#endif
    }

    // Dodge incoming projectiles.
    for (int spr = headspritestat[STAT_PROJECTILE]; spr >= 0; spr = nextspritestat[spr])
    {
        int32_t dodgeSpeed = 0;
        switch (tileGetMapping(sprite[spr].picnum))
        {
            case TONGUE__:      dodgeSpeed = 4;  break;
            case FREEZEBLAST__: dodgeSpeed = 4;  break;
            case SHRINKSPARK__: dodgeSpeed = 16; break;
            case RPG__:         dodgeSpeed = 16; break;
            default:                  dodgeSpeed = 0;  break;
        }

        if (dodgeSpeed)
        {
            vec3_t projPos = sprite[spr].xyz;
            int32_t angleDelta = G_GetAngleDelta(pSpr->ang, getangle(projPos.x - pSpr->x, projPos.y - pSpr->y));

            if (klabs(angleDelta) > 1024) // Don't dodge projectiles we can't see.
                continue;

            for (int32_t dodgeCnt = 0; dodgeCnt <= 8; dodgeCnt++)
            {
                if (tmulscale11(projPos.x - pSpr->x, projPos.x - pSpr->x, projPos.y - pSpr->y, projPos.y - pSpr->y, (projPos.z - pSpr->z) >> 4, (projPos.z - pSpr->z) >> 4) < 3072)
                {
                    vec2_t d = { sintable[(sprite[spr].ang + 512) & 2047],
                                 sintable[sprite[spr].ang & 2047] };

                    int32_t dodgeDir;
                    if ((pSpr->x - projPos.x) * d.y > (pSpr->y - projPos.y) * d.x)
                        dodgeDir = -dodgeSpeed * 512;
                    else
                        dodgeDir = dodgeSpeed * 512;

                    syn->fvel -= mulscale17(d.y, dodgeDir);
                    syn->svel += mulscale17(d.x, dodgeDir);
                }

                if (dodgeCnt < 7)
                {
                    projPos.x += (mulscale14(sprite[spr].xvel, sintable[(sprite[spr].ang + 512) & 2047]) << 2);
                    projPos.y += (mulscale14(sprite[spr].xvel, sintable[sprite[spr].ang & 2047]) << 2);
                    projPos.z += (sprite[spr].zvel << 2);
                }
                else
                {
                    // [JM] Doesn't do anything... hit var isn't used anywhere. Leftover crud?
#if 0
                    hitscan(&sprite[spr].pos, sprite[spr].sectnum,
                        mulscale14(sprite[spr].xvel, sintable[(sprite[spr].ang + 512) & 2047]),
                        mulscale14(sprite[spr].xvel, sintable[sprite[spr].ang & 2047]),
                        (int)sprite[spr].zvel,
                        &hit, CLIPMASK1);
#endif
                }
            }
        }
    }

    // We're bumping into something.
    if (moveResult)
    {
        bot->moveCount++;
         
        if (touchingWall) // Avoid obstructions. (By jumping/crouching, using jetpack, etc)
        {
            int32_t wallNum = moveResult & (MAXWALLS - 1);
            int16_t nextSect = wall[wallNum].nextsector;

            if (nextSect == -1) // No sector, so let's try seeking through TROR.
            {
                int32_t nextWallNum = yax_getnextwall(wallNum, YAX_CEILING);
                if (nextWallNum != -1)
                {
                    nextSect = wall[nextWallNum].nextsector;
                    wallNum = nextWallNum;
                }
            }

            // Only strafe if we're not touching the goal wall.
            // Should make it easier for bots to get up crate stacks and into vents.
            if (wallNum == bot->goalWall)
                moveFlags &= ~BOT_MOVE_STRAFE;
            else
                moveFlags |= BOT_MOVE_STRAFE;

            if (nextSect >= 0 && !(wall[wallNum].cstat & CSTAT_WALL_BLOCK))
            {
                int32_t floorZ = yax_getflorzofslope(nextSect, p->pos.xy);
                int32_t ceilZ = yax_getceilzofslope(nextSect, p->pos.xy);

                if (floorZ != ceilZ)
                {
                    if (pSect->lotag != ST_2_UNDERWATER && !p->jetpack_on)
                    {
                        if (floorZ <= p->pos.z + PLAYER_STEP_HEIGHT)
                            moveFlags |= BOT_MOVE_JUMP;

                        if (ceilZ >= p->pos.z - (24 << 8))
                            moveFlags |= BOT_MOVE_CROUCH;
                    }

                    if ((p->jetpack_on || pSect->lotag == ST_2_UNDERWATER) && floorZ <= p->pos.z + PLAYER_STEP_HEIGHT)
                        moveFlags |= BOT_MOVE_ASCEND;
                    else if ((p->jetpack_on || pSect->lotag == ST_2_UNDERWATER) && ceilZ >= p->pos.z - PMINHEIGHT)
                        moveFlags |= BOT_MOVE_DESCEND;
                    else if ((p->inv_amount[GET_JETPACK] > 106 && pSect->lotag != ST_2_UNDERWATER) && floorZ <= p->pos.z - (72 << 8))
                        moveFlags |= BOT_MOVE_ASCEND;
                }

                if (DukeBot_CheckUsableSector(nextSect) || DukeBot_CheckUsableSector(pSpr->sectnum))
                {
                    moveFlags |= BOT_MOVE_USE;
                    faceAngle = DukeBot_GetAngleForWall(wallNum); // Face the wall.
                }
                else if (floorZ == ceilZ) // Sector's closed up. Potentially an unknown door type? Try it.
                    moveFlags |= BOT_MOVE_USE;
            }
        }
        else if (touchingSprite) // Bumped into a sprite.
        {
            int32_t spriteNum = moveResult & (MAXSPRITES - 1);

            // Not touching target in melee mode.
            if(!((combatFlags & BOT_COMBAT_MELEE) && spriteNum == goalp->i))
                moveFlags |= BOT_MOVE_JUMP;

            moveFlags |= BOT_MOVE_STRAFE;
        }

        if ((bot->moveCount & 31) == 17)
            syn->bits |= BIT(SK_QUICK_KICK);

        // Obstructed too many times in a row, give up on this path.
        if (bot->moveCount > 32)
        {
            bot->goalWall = -1;
            bot->moveCount = 0;
        }
    }
    else bot->moveCount = 0;

    // Handle jetpack and underwater behavior.
    if (pSect->lotag == ST_1_ABOVE_WATER)
    {
        if ((p->on_warping_sector && !seePlayer && sector[gpSpr->sectnum].lotag == ST_2_UNDERWATER) ||
            (p->on_warping_sector && !seePlayer && gItemSpr && sector[gItemSpr->sectnum].lotag == ST_2_UNDERWATER))
            bot->waterSubmergeTime = 6;
    }

    if (bot->waterSubmergeTime > 0)
    {
        moveFlags &= ~(BOT_MOVE_ASCEND | BOT_MOVE_JUMP);
        moveFlags |= BOT_MOVE_DESCEND;
        bot->waterSubmergeTime--;
    }
    else if (pSect->lotag == ST_2_UNDERWATER)
    {
        if ((p->on_warping_sector && p->airleft < 5) ||
            (p->on_warping_sector && !seePlayer && sector[gpSpr->sectnum].lotag == ST_1_ABOVE_WATER) ||
            (p->on_warping_sector && !seePlayer && gItemSpr && sector[gItemSpr->sectnum].lotag == ST_1_ABOVE_WATER) ||
            (!touchingWall && !touchingSprite && !goForItem && seePlayer && pSpr->z > gpSpr->z + (48 << 8)) ||
            (!touchingWall && !touchingSprite && goForItem && pSpr->z > gItemSpr->z + (4 << 8)))
        {
            moveFlags &= ~(BOT_MOVE_DESCEND | BOT_MOVE_CROUCH);
            moveFlags |= BOT_MOVE_ASCEND;
        }
        else if ((!touchingWall && !touchingSprite && !goForItem && seePlayer && pSpr->z < gpSpr->z - (48 << 8)) ||
                 (!touchingWall && !touchingSprite && goForItem && pSpr->z < gItemSpr->z - (4 << 8)))
        {
            moveFlags &= ~(BOT_MOVE_ASCEND | BOT_MOVE_JUMP);
            moveFlags |= BOT_MOVE_DESCEND;
        }
    }
    else
    {
        if (p->jetpack_on)
        {
            if ((p->inv_amount[GET_JETPACK] < 106) ||
                ((p->truefz - p->truecz) <= (72 << 8)) ||
                (!touchingWall && !touchingSprite && !goForItem && seePlayer && (pSpr->z < gpSpr->z - (48 << 8))) ||
                (!touchingWall && !touchingSprite && goForItem && pSpr->z < gItemSpr->z - (4 << 8)))
            {
                moveFlags &= ~(BOT_MOVE_ASCEND | BOT_MOVE_JUMP);
                moveFlags |= BOT_MOVE_DESCEND;
            }
            else if ((!touchingWall && !touchingSprite && !goForItem && seePlayer && pSpr->z > gpSpr->z + (48 << 8)) ||
                     (!touchingWall && !touchingSprite && goForItem && pSpr->z > gItemSpr->z + (4 << 8)))
            {
                moveFlags &= ~(BOT_MOVE_DESCEND | BOT_MOVE_CROUCH);
                moveFlags |= BOT_MOVE_ASCEND;
            }
        }
    }

    // Process movement flags
    if (moveFlags & BOT_MOVE_STRAFE)
    {
        //Strafe attack
        int32_t strafeClock = (int32_t)(bot->cycleCount * TICSPERFRAME) + snum * 13468;
        int32_t strafeVal = sintable[(strafeClock << 6) & 2047];

        strafeVal += sintable[((strafeClock + 4245) << 5) & 2047];
        strafeVal += sintable[((strafeClock + 6745) << 4) & 2047];
        strafeVal += sintable[((strafeClock + 15685) << 3) & 2047];

        if (seePlayer && !prioritizeItems && !(combatFlags & BOT_COMBAT_MELEE))
        {
            int32_t dx = sintable[(sprite[goalp->i].ang + 512) & 2047];
            int32_t dy = sintable[sprite[goalp->i].ang & 2047];

            if ((pSpr->x - gpSpr->x) * dy > (pSpr->y - gpSpr->y) * dx)
                strafeVal += 8192;
            else
                strafeVal -= 8192;

            if ((gpDist > 256) && !(seed_krand(&bot->rand) % 200) && pSect->lotag != ST_2_UNDERWATER) // Randomly jump around a bit in battle.
                syn->bits |= BIT(SK_JUMP);
        }

        syn->fvel += ((sintable[(faceAngle + 1024) & 2047] * strafeVal) >> 17);
        syn->svel += ((sintable[(faceAngle + 512) & 2047] * strafeVal) >> 17);
    }

    if (moveFlags & BOT_MOVE_JUMP)
    {
        if (((bot->moveCount & 31) == 4) || ((bot->moveCount & 31) == 8))
            syn->bits |= BIT(SK_JUMP);

        if (gItemSpr) // Give up faster if we have to attempt jumping at an item.
            bot->seekSpriteFailCount++;
    }

    if (moveFlags & BOT_MOVE_CROUCH)
        syn->bits |= BIT(SK_CROUCH);

    if (moveFlags & BOT_MOVE_USE && !(bot->moveCount % 2))
        syn->bits |= BIT(SK_OPEN);

    if (moveFlags & BOT_MOVE_DESCEND)
    {
        // Descending, and we're close to the floor.
        if ((pSect->lotag != ST_2_UNDERWATER && p->jetpack_on) && (p->truefz <= p->pos.z + (72 << 8)))
            syn->bits |= BIT(SK_JETPACK);

        syn->bits &= ~BIT(SK_JUMP);
        syn->bits |= BIT(SK_CROUCH);
    }
    else if ((moveFlags & BOT_MOVE_ASCEND))
    {
        if (pSect->lotag != ST_2_UNDERWATER && !p->jetpack_on)
            syn->bits |= BIT(SK_JETPACK);

        syn->bits &= ~BIT(SK_CROUCH);
        syn->bits |= BIT(SK_JUMP);
    }

    // Handle weapons and inventory.
    if (((p->curr_weapon <= PISTOL_WEAPON) || (p->curr_weapon == TRIPBOMB_WEAPON)) && !(bot->moveCount % 30))
    {
        for (int32_t i = MAX_WEAPONS - 1; i > 0; i--)
        {
            if (i == TRIPBOMB_WEAPON)
                continue;

            if (p->gotweapon[i] && p->ammo_amount[i])
            {
                syn->bits |= (i+1) << SK_WEAPON_BITS;
                break;
            }
        }
    }

    if ((p->inv_amount[GET_FIRSTAID] > 0) && (p->last_extra < 100) && ((seed_krand(&bot->rand) % 25) == 0))
        syn->bits |= BIT(SK_MEDKIT);

    if (seePlayer && (p->inv_amount[GET_STEROIDS] >= 400) && (!(seed_krand(&bot->rand) % 100) || p->curr_weapon == KNEE_WEAPON))
        syn->bits |= BIT(SK_STEROIDS);

    // Impart a bit of randomness, which may help to get unstuck in some situations.
    syn->fvel += (seed_krand(&bot->rand) % 512)-256;
    syn->svel += (seed_krand(&bot->rand) % 512)-256;

    // Cap velocity, so we're not inhumanly fast.
    syn->fvel = clamp(syn->fvel, -BOT_MAXSPEED, BOT_MAXSPEED);
    syn->svel = clamp(syn->svel, -BOT_MAXSPEED, BOT_MAXSPEED);

    // Hold run key if we're moving fast enough. Looks nicer.
    if ((klabs(syn->fvel) > BOT_MAXSPEED >> 2) || (klabs(syn->svel) > BOT_MAXSPEED >> 2))
        syn->bits |= BIT(SK_RUN);

    syn->q16avel = min(max(fix16_from_int((((faceAngle + 1024 - pSpr->ang) & 2047) - 1024) >> 1), F16(-127)), F16(127));

    bot->cycleCount++;
}