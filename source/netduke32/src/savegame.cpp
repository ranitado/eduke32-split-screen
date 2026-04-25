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
#include "md4.h"

#define BITPTR_POINTER 1

void ReadSaveGameHeaders(void)
{
    int32_t dummy,j;
    int32_t i;
    char fn[13];
    int32_t fil;

    Bstrcpy(fn,"egam_.sav");

    for (i=0; i<10; i++)
    {
        fn[4] = i+'0';
        if ((fil = kopen4loadfrommod(fn,0)) == -1) continue;
        if (kdfread(&j,sizeof(j),1,fil) != 1)
        {
            kclose(fil);
            continue;
        }
        if (kdfread(g_szBuf,j,1,fil) != 1)
        {
            kclose(fil);
            continue;
        }
        if (kdfread(&dummy,sizeof(dummy),1,fil) != 1)
        {
            kclose(fil);
            continue;
        }
        if (dummy != BYTEVERSION)
        {
            kclose(fil);
            continue;
        }
        if (kdfread(&dummy,sizeof(dummy),1,fil) != 1)
        {
            kclose(fil);
            continue;
        }
        if (kdfread(&ud.savegame[i][0],19,1,fil) != 1)
        {
            ud.savegame[i][0] = 0;
        }
        kclose(fil);
    }
}

int32_t G_LoadSaveHeader(char spot,struct savehead *saveh)
{
    char fn[13];
    int32_t fil;
    int32_t bv;

    strcpy(fn, "egam0.sav");
    fn[4] = spot+'0';

    if ((fil = kopen4loadfrommod(fn,0)) == -1) return(-1);

    walock[TILE_LOADSHOT] = 255;

    if (kdfread(&bv,sizeof(bv),1,fil) != 1) goto corrupt;
    if (kdfread(g_szBuf,bv,1,fil) != 1) goto corrupt;
    g_szBuf[bv]=0;
    //    AddLog(g_szBuf);

    if (kdfread(&bv,sizeof(bv),1,fil) != 1) goto corrupt;

    if (kdfread(&saveh->numplr,sizeof(saveh->numplr),1,fil) != 1) goto corrupt;

    if (kdfread(saveh->name,sizeof(saveh->name),1,fil) != 1) goto corrupt;
    if (kdfread(&saveh->volnum,sizeof(saveh->volnum),1,fil) != 1) goto corrupt;
    if (kdfread(&saveh->levnum,sizeof(saveh->levnum),1,fil) != 1) goto corrupt;
    if (kdfread(&saveh->plrskl,sizeof(saveh->plrskl),1,fil) != 1) goto corrupt;
    if (kdfread(saveh->boardfn,sizeof(saveh->boardfn),1,fil) != 1) goto corrupt;

    if (waloff[TILE_LOADSHOT] == 0) g_cache.allocateBlock(&waloff[TILE_LOADSHOT],320*200,&walock[TILE_LOADSHOT]);
    tilesiz[TILE_LOADSHOT].x = 200;
    tilesiz[TILE_LOADSHOT].y = 320;
    if (kdfread((char *)waloff[TILE_LOADSHOT],320,200,fil) != 200) goto corrupt;
    tileInvalidate(TILE_LOADSHOT,0,255);

    kclose(fil);

    return(0);
corrupt:
    kclose(fil);
    return 1;
}

static int32_t sv_loadBoardMD4(char* const fn)
{
    buildvfs_kfd fil;
    if ((fil = kopen4load(fn, 0)) == buildvfs_kfd_invalid)
        return -1;

    klseek(fil, 0, SEEK_SET);
    int32_t boardsize = kfilelength(fil);
    uint8_t* fullboard = (uint8_t*)Xmalloc(boardsize);
    bool failed = kread_and_test(fil, fullboard, boardsize);
    kclose(fil);
    if (failed)
    {
        Xfree(fullboard);
        return -1;
    }

    md4once(fullboard, boardsize, g_loadedMapHack.md4);
    Xfree(fullboard);
    return 0;
}

static void sv_loadMhk(char* currentboardfilename)
{
    bool loadedMhk = false;
    if (sv_loadBoardMD4(currentboardfilename) == 0)
    {
        auto mhkInfo = (usermaphack_t*)bsearch(&g_loadedMapHack, usermaphacks, num_usermaphacks,
            sizeof(usermaphack_t), compare_usermaphacks);

        if (mhkInfo && (loadedMhk = (engineLoadMHK(mhkInfo->mhkfile) == 0)))
            LOG_F(INFO, "Loaded map hack file \"%s\"", mhkInfo->mhkfile);
    }

    if (!loadedMhk)
    {
        append_ext_UNSAFE(currentboardfilename, ".mhk");
        if (engineLoadMHK(currentboardfilename) == 0)
            LOG_F(INFO, "Loaded map hack file \"%s\"", currentboardfilename);
    }
}

// Projectile processing

static projectile_t *savegame_projectiledata;
static uint8_t       savegame_projectiles[bitmap_size(MAXTILES)];
static int32_t       savegame_projectilecnt = 0;

static void sv_preprojectilesave()
{
    savegame_projectilecnt = 0;
    Bmemset(savegame_projectiles, 0, sizeof(savegame_projectiles));

    for (auto & i : g_tile)
        if (i.proj)
            savegame_projectilecnt++;

    if (savegame_projectilecnt > 0)
        savegame_projectiledata = (projectile_t *) Xrealloc(savegame_projectiledata, sizeof(projectile_t) * savegame_projectilecnt);

    for (int i = 0, cnt = 0; i < MAXTILES; i++)
    {
        if (g_tile[i].proj)
        {
            savegame_projectiles[i>>3] |= 1<<(i&7);
            Bmemcpy(&savegame_projectiledata[cnt++], g_tile[i].proj, sizeof(projectile_t));
        }
    }
}

static void sv_postprojectilesave()
{
    DO_FREE_AND_NULL(savegame_projectiledata);
}

static void sv_preprojectileload()
{
    savegame_projectilecnt = 0;

    for (int i = 0; i < MAXTILES; i++)
    {
        if (bitmap_test(savegame_projectiles, i))
            savegame_projectilecnt++;
    }

    if (savegame_projectilecnt > 0)
        savegame_projectiledata = (projectile_t *) Xrealloc(savegame_projectiledata, sizeof(projectile_t) * savegame_projectilecnt);
}

static void sv_postprojectileload()
{
    for (int i = 0, cnt = 0; i < MAXTILES; i++)
    {
        if (bitmap_test(savegame_projectiles, i))
        {
            C_AllocProjectile(i);
            Bmemcpy(g_tile[i].proj, &savegame_projectiledata[cnt++], sizeof(projectile_t));
        }
    }

    DO_FREE_AND_NULL(savegame_projectiledata);
}

// hack
static int different_user_map;

int32_t G_LoadPlayer(int32_t spot)
{
    int32_t k;
    char fn[13];
    char mpfn[13];
    char *fnptr, *scriptptrs;
    int32_t fil, bv, i, x;
    intptr_t j;
    int32_t nump;

    strcpy(fn, "egam0.sav");
    strcpy(mpfn, "egamA_00.sav");

    if (spot < 0)
    {
        multiflag = 1;
        multiwhat = 0;
        multipos = -spot-1;
        return -1;
    }

    if (multiflag == 2 && multiwho != myconnectindex)
    {
        fnptr = mpfn;
        mpfn[4] = spot + 'A';

        if (ud.multimode > 9)
        {
            mpfn[6] = (multiwho/10) + '0';
            mpfn[7] = (multiwho%10) + '0';
        }
        else mpfn[7] = multiwho + '0';
    }
    else
    {
        fnptr = fn;
        fn[4] = spot + '0';
    }

    if ((fil = kopen4loadfrommod(fnptr,0)) == -1) return(-1);

    ready2send = 0;

    if (kdfread(&bv,sizeof(bv),1,fil) != 1) goto corrupt;
    if (kdfread(g_szBuf,bv,1,fil) != 1) goto corrupt;
    g_szBuf[bv]=0;
    //    AddLog(g_szBuf);

    if (kdfread(&bv,sizeof(bv),1,fil) != 1) goto corrupt;
    if (bv != BYTEVERSION)
    {
        P_DoQuote(QUOTE_SAVE_BAD_VERSION,g_player[myconnectindex].ps);
        kclose(fil);
        ototalclock = (int32_t)totalclock;
        ready2send = 1;
        return 1;
    }

    if (kdfread(&nump,sizeof(nump),1,fil) != 1) goto corrupt;
    if (nump != ud.multimode)
    {
        kclose(fil);
        ototalclock = (int32_t)totalclock;
        ready2send = 1;
        P_DoQuote(QUOTE_SAVE_BAD_PLAYERS,g_player[myconnectindex].ps);
        return 1;
    }
    else ud.multimode = nump;

    if (numplayers > 1)
    {
        pub = NUMPAGES;
        pus = NUMPAGES;
        G_UpdateScreenArea();
        G_DrawBackground();
        menutext(160,100,0,0,"LOADING...");
        videoNextPage();
    }

    Net_WaitForPlayers();

    FX_StopAllSounds();

    if (numplayers > 1)
    {
        if (kdfread(&buf,19,1,fil) != 1) goto corrupt;
    }
    else
    {
        if (kdfread(&ud.savegame[spot][0],19,1,fil) != 1) goto corrupt;
    }


    if (kdfread(&ud.volume_number,sizeof(ud.volume_number),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.level_number,sizeof(ud.level_number),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.player_skill,sizeof(ud.player_skill),1,fil) != 1) goto corrupt;

    ud.m_level_number = ud.level_number;
    ud.m_volume_number = ud.volume_number;
    ud.m_player_skill = ud.player_skill;

    char boardName[BMAX_PATH];
    if (kdfread(&boardName[0], BMAX_PATH, 1, fil) != 1) goto corrupt;
    different_user_map = Bstrncmp(boardfilename, boardName, sizeof(boardName));
    Bmemcpy(&boardfilename[0], &boardName[0], BMAX_PATH);

    {
        int const mapIdx = ud.volume_number * MAXLEVELS + ud.level_number;

        if (boardfilename[0])
            Bstrcpy(currentboardfilename, boardfilename);
        else if (g_mapInfo[mapIdx].filename)
            Bstrcpy(currentboardfilename, g_mapInfo[mapIdx].filename);

        if (currentboardfilename[0])
        {
            // only setup art if map differs from previous
            if (different_user_map)
            {
                artSetupMapArt(currentboardfilename);
            }

            sv_loadMhk(currentboardfilename);
        }

        Bmemcpy(currentboardfilename, boardfilename, BMAX_PATH);
    }


    //Fake read because lseek won't work with compression
    walock[TILE_LOADSHOT] = 1;
    if (waloff[TILE_LOADSHOT] == 0) g_cache.allocateBlock(&waloff[TILE_LOADSHOT],320*200,&walock[TILE_LOADSHOT]);
    tilesiz[TILE_LOADSHOT].x = 200;
    tilesiz[TILE_LOADSHOT].y = 320;
    if (kdfread((char *)waloff[TILE_LOADSHOT],320,200,fil) != 200) goto corrupt;
    tileInvalidate(TILE_LOADSHOT,0,255);

    if (kdfread(&ud.music_episode, sizeof(ud.music_episode), 1, fil) != 1) goto corrupt;
    if (kdfread(&ud.music_level, sizeof(ud.music_level), 1, fil) != 1) goto corrupt;

    if (kdfread(&ud.monsters_killed,sizeof(ud.monsters_killed),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.total_monsters,sizeof(ud.total_monsters),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.playerbest,sizeof(ud.playerbest),1,fil) != 1) goto corrupt;

    if (kdfread(&numwalls,sizeof(numwalls),1,fil) != 1) goto corrupt;
    if (kdfread(&wall[0],sizeof(walltype),MAXWALLS,fil) != MAXWALLS) goto corrupt;
    if (kdfread(&numsectors,sizeof(numsectors),1,fil) != 1) goto corrupt;
    if (kdfread(&sector[0],sizeof(sectortype),MAXSECTORS,fil) != MAXSECTORS) goto corrupt;
    if (kdfread(&sprite[0],sizeof(spritetype),MAXSPRITES,fil) != MAXSPRITES) goto corrupt;
    if (kdfread(&spriteext[0],sizeof(spriteext_t),MAXSPRITES,fil) != MAXSPRITES) goto corrupt;

#ifdef YAX_ENABLE
    if (kdfread(&numyaxbunches,sizeof(numyaxbunches),1,fil) != 1) goto corrupt;
# if !defined NEW_MAP_FORMAT
    if (kdfread(yax_bunchnum,sizeof(yax_bunchnum[0]),numsectors,fil) != numsectors) goto corrupt;
    if (kdfread(yax_nextwall,sizeof(yax_nextwall[0]),numwalls,fil) != numwalls) goto corrupt;
# endif
    yax_update(numyaxbunches > 0 ? 2 : 1);
#endif

#if 0 // Polymer
    if (kdfread(&lightcount,sizeof(lightcount),1,fil) != 1) goto corrupt;
    if (kdfread(&prlights[0],sizeof(_prlight),lightcount,fil) != lightcount) goto corrupt;
#else
    if (kdfread(&i,sizeof(i),1,fil) != 1) goto corrupt;
#endif // POLYMER

#if defined(USE_OPENGL)
    for (i=0; i<MAXSPRITES; i++)
        if (spriteext[i].mdanimtims)
            spriteext[i].mdanimtims+=mdtims;
#endif
    if (kdfread(&Numsprites,sizeof(Numsprites),1,fil) != 1) goto corrupt;
    if (kdfread(&tailspritefree,sizeof(tailspritefree),1,fil) != 1) goto corrupt;
    if (kdfread(&headspritesect[0],sizeof(headspritesect[0]),MAXSECTORS+1,fil) != MAXSECTORS+1) goto corrupt;
    if (kdfread(&prevspritesect[0],sizeof(prevspritesect[0]),MAXSPRITES,fil) != MAXSPRITES) goto corrupt;
    if (kdfread(&nextspritesect[0],sizeof(nextspritesect[0]),MAXSPRITES,fil) != MAXSPRITES) goto corrupt;
    if (kdfread(&headspritestat[STAT_DEFAULT],sizeof(headspritestat[STAT_DEFAULT]),MAXSTATUS+1,fil) != MAXSTATUS+1) goto corrupt;
    if (kdfread(&prevspritestat[STAT_DEFAULT],sizeof(prevspritestat[STAT_DEFAULT]),MAXSPRITES,fil) != MAXSPRITES) goto corrupt;
    if (kdfread(&nextspritestat[STAT_DEFAULT],sizeof(nextspritestat[STAT_DEFAULT]),MAXSPRITES,fil) != MAXSPRITES) goto corrupt;
    if (kdfread(&g_numCyclers,sizeof(g_numCyclers),1,fil) != 1) goto corrupt;
    if (kdfread(&cyclers[0][0],sizeof(cyclers),1,fil) != 1) goto corrupt;
    for (i=0; i<nump; i++)
        if (kdfread(g_player[i].ps,sizeof(DukePlayer_t),1,fil) != 1) goto corrupt;
    if (kdfread(&g_playerSpawnPoints,sizeof(g_playerSpawnPoints),1,fil) != 1) goto corrupt;
    if (kdfread(&g_numAnimWalls,sizeof(g_numAnimWalls),1,fil) != 1) goto corrupt;
    if (kdfread(&animwall,sizeof(animwall),1,fil) != 1) goto corrupt;
    if (kdfread(&g_origins[0], sizeof(vec2_t), sizeof(g_origins) / sizeof(vec2_t), fil) != sizeof(g_origins) / sizeof(vec2_t)) goto corrupt;
    if (kdfread(&g_spriteDeleteQueuePos,sizeof(g_spriteDeleteQueuePos),1,fil) != 1) goto corrupt;
    if (kdfread(&g_spriteDeleteQueueSize,sizeof(g_spriteDeleteQueueSize),1,fil) != 1) goto corrupt;
    if (kdfread(&SpriteDeletionQueue[0],sizeof(*SpriteDeletionQueue),g_spriteDeleteQueueSize,fil) != g_spriteDeleteQueueSize) goto corrupt;
    if (kdfread(&g_mirrorCount,sizeof(g_mirrorCount),1,fil) != 1) goto corrupt;
    if (kdfread(&g_mirrorWall[0],sizeof(g_mirrorWall),1,fil) != 1) goto corrupt;
    if (kdfread(&g_mirrorSector[0],sizeof(g_mirrorSector),1,fil) != 1) goto corrupt;
    if (kdfread(&show2dsector[0],sizeof(show2dsector),1,fil) != 1) goto corrupt;

    if (kdfread(&g_numClouds,sizeof(g_numClouds),1,fil) != 1) goto corrupt;
    if (kdfread(&g_cloudSect[0],sizeof(g_cloudSect),1,fil) != 1) goto corrupt;
    if (kdfread(&g_cloudX,sizeof(g_cloudX),1,fil) != 1) goto corrupt;
    if (kdfread(&g_cloudY,sizeof(g_cloudY),1,fil) != 1) goto corrupt;

    if (kdfread(&g_tile[0], sizeof(tiledata_t), MAXTILES, fil) != MAXTILES) goto corrupt;
    if (kdfread(&g_scriptSize,sizeof(g_scriptSize),1,fil) != 1) goto corrupt;
    if (!g_scriptSize) goto corrupt;
    scriptptrs = (char*)Xcalloc(1,g_scriptSize * sizeof(scriptptrs));
    Xfree(bitptr);
    bitptr = (uint8_t*)Xcalloc(1,(((g_scriptSize+7)>>3)+1) * sizeof(uint8_t));
    if (kdfread(&bitptr[0],sizeof(uint8_t),(g_scriptSize+7)>>3,fil) != ((g_scriptSize+7)>>3)) goto corrupt;
    if (apScript != NULL)
        Xfree(apScript);
    apScript = (intptr_t*)Xcalloc(1,g_scriptSize * sizeof(intptr_t));
    if (kdfread(&apScript[0],sizeof(apScript),g_scriptSize,fil) != g_scriptSize) goto corrupt;
    for (i=0; i<g_scriptSize; i++)
        if (bitptr[i>>3]&(BITPTR_POINTER<<(i&7)))
        {
            j = (intptr_t)apScript[i]+(intptr_t)&apScript[0];
            apScript[i] = j;
        }

    for (i=0; i<MAXTILES; i++)
        if (g_tile[i].execPtr)
        {
            j = (intptr_t)g_tile[i].execPtr+(intptr_t)&apScript[0];
            g_tile[i].execPtr = (intptr_t *)j;
        }

    for (i=0; i<MAXTILES; i++)
        if (g_tile[i].loadPtr)
        {
            j = (intptr_t)g_tile[i].loadPtr+(intptr_t)&apScript[0];
            g_tile[i].loadPtr = (intptr_t *)j;
        }

    scriptptrs = (char*)Xrealloc(scriptptrs, MAXSPRITES * sizeof(scriptptrs));

    if (kdfread(&scriptptrs[0],sizeof(scriptptrs),MAXSPRITES,fil) != MAXSPRITES) goto corrupt;
    if (kdfread(&actor[0],sizeof(ActorData_t),MAXSPRITES,fil) != MAXSPRITES) goto corrupt;

    for (i=0; i<MAXSPRITES; i++)
    {
        j = (intptr_t)(&apScript[0]);
        if (scriptptrs[i]&1) T2(i) += j;
        if (scriptptrs[i]&2) T5(i) += j;
        if (scriptptrs[i]&4) T6(i) += j;
    }

    // Projectiles
    if (kdfread(&savegame_projectiles[0],sizeof(savegame_projectiles),1,fil) != 1) goto corrupt;
    sv_preprojectileload();
    if (savegame_projectilecnt > 0)
        if (kdfread(&savegame_projectiledata[0],sizeof(savegame_projectiledata[0]),savegame_projectilecnt,fil) != savegame_projectilecnt) goto corrupt;
    // FIXME: g_tile as a whole is loaded from file, including stale
    // proj pointers. EDuke32 itself does not store the g_tile array.
    // More generally, EDuke32 does not support loading saved games
    // with differing CON code.
    for (auto & i : g_tile)
        if (i.proj)
            i.proj = 0;
    sv_postprojectileload();

    if (kdfread(&lockclock,sizeof(lockclock),1,fil) != 1) goto corrupt;
    if (kdfread(&g_pskyidx,sizeof(g_pskyidx),1,fil) != 1) goto corrupt;

    if (kdfread(&g_animateCount,sizeof(g_animateCount),1,fil) != 1) goto corrupt;
    if (kdfread(&sectorAnims[0], sizeof(sectorAnims[0]), MAXANIMATES, fil) != MAXANIMATES) goto corrupt;

    if (kdfread(&g_earthquakeTime,sizeof(g_earthquakeTime),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.from_bonus,sizeof(ud.from_bonus),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.secretlevel,sizeof(ud.secretlevel),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.max_secret_rooms,sizeof(ud.max_secret_rooms),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.secret_rooms,sizeof(ud.secret_rooms),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.dmflags,sizeof(ud.dmflags),1,fil) != 1) goto corrupt;
    ud.m_dmflags = ud.dmflags;

    if (kdfread(&ud.god,sizeof(ud.god),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.auto_run,sizeof(ud.auto_run),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.crosshair,sizeof(ud.crosshair),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.last_level,sizeof(ud.last_level),1,fil) != 1) goto corrupt;
    if (kdfread(&ud.eog,sizeof(ud.eog),1,fil) != 1) goto corrupt;

    if (kdfread(&ud.coop,sizeof(ud.coop),1,fil) != 1) goto corrupt;
    ud.m_coop = ud.coop;

    if (kdfread(&g_curViewscreen,sizeof(g_curViewscreen),1,fil) != 1) goto corrupt;
    if (kdfread(&connecthead,sizeof(connecthead),1,fil) != 1) goto corrupt;
    if (kdfread(connectpoint2,sizeof(connectpoint2),1,fil) != 1) goto corrupt;
    if (kdfread(&g_numPlayerSprites,sizeof(g_numPlayerSprites),1,fil) != 1) goto corrupt;
    for (i=0; i<MAXPLAYERS; i++)
        if (kdfread((int16_t *)&g_player[i].frags[0],sizeof(g_player[i].frags),1,fil) != 1) goto corrupt;

    if (kdfread(&ud.got_access, sizeof(ud.got_access), 1, fil) != 1) goto corrupt;

    if (kdfread(&randomseed,sizeof(randomseed),1,fil) != 1) goto corrupt;
    if (kdfread(&g_globalRandom,sizeof(g_globalRandom),1,fil) != 1) goto corrupt;
    //if (kdfread(&parallaxyscale,sizeof(parallaxyscale),1,fil) != 1) goto corrupt;

    if (kdfread(&i,sizeof(i),1,fil) != 1) goto corrupt;

    while (i != MAXQUOTES)
    {
        if (apStrings[i] != NULL)
            Xfree(apStrings[i]);

        apStrings[i] = (char*)Xcalloc(MAXQUOTELEN,sizeof(uint8_t));

        if (kdfread((char *)apStrings[i],MAXQUOTELEN,1,fil) != 1) goto corrupt;
        if (kdfread(&i,sizeof(i),1,fil) != 1) goto corrupt;
    }

    if (kdfread(&g_numXStrings,sizeof(g_numXStrings),1,fil) != 1) goto corrupt;

    for (i=0; i<g_numXStrings; i++)
    {
        if (apXStrings[i] != NULL)
            Xfree(apXStrings[i]);

        apXStrings[i] = (char*)Xcalloc(MAXQUOTELEN,sizeof(uint8_t));

        if (kdfread((char *)apXStrings[i],MAXQUOTELEN,1,fil) != 1) goto corrupt;
    }

    if (kdfread(&g_dynTileList[0],sizeof(g_dynTileList[0]),g_dynTileListSize,fil) != g_dynTileListSize) goto corrupt;

    if (Gv_ReadSave(fil)) goto corrupt;

    kclose(fil);

    if (g_player[myconnectindex].ps->over_shoulder_on != 0)
    {
        g_cameraDistance = 0;
        g_cameraClock = 0;
        g_player[myconnectindex].ps->over_shoulder_on = 1;
    }

    screenpeek = myconnectindex;

    clearbufbyte(gotpic,sizeof(gotpic),0L);
    G_CacheMapData();

    {
        int32_t musicIdx = (ud.music_episode * MAXLEVELS) + ud.music_level;

        if (G_HaveUserMap() && ud.music_level == USERMAPMUSICFAKELEVEL && ud.music_episode == USERMAPMUSICFAKEVOLUME)
        {
            char levname[BMAX_PATH];
            G_SetupFilenameBasedMusic(levname, boardfilename);
        }

        if (g_mapInfo[musicIdx].musicfn != NULL && (musicIdx != g_musicIndex || different_user_map))
        {
            ud.music_episode = g_musicIndex / MAXLEVELS;
            ud.music_level = g_musicIndex % MAXLEVELS;
            S_PlayLevelMusicOrNothing(musicIdx);
        }
        else
            S_ContinueLevelMusic();

        if (ud.config.MusicToggle)
            S_PauseMusic(false);

        ud.gm = MODE_GAME;
        ud.recstat = 0;

        if (g_player[myconnectindex].ps->jetpack_on)
            A_PlaySound(DUKE_JETPACK_IDLE, g_player[myconnectindex].ps->i);

        g_restorePalette = 1;
        P_UpdateScreenPal(g_player[myconnectindex].ps);
        G_UpdateScreenArea();

        FX_SetReverb(0);

        if (ud.lockout == 0)
        {
            for (x = 0; x < g_numAnimWalls; x++)
                if (wall[animwall[x].wallnum].extra >= 0)
                    wall[animwall[x].wallnum].picnum = wall[animwall[x].wallnum].extra;
        }
        else
        {
            for (x = 0; x < g_numAnimWalls; x++)
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
            case 31:
                G_SetInterpolation(&sector[sprite[k].sectnum].floorz);
                break;
            case 32:
                G_SetInterpolation(&sector[sprite[k].sectnum].ceilingz);
                break;
            case 25:
                G_SetInterpolation(&sector[sprite[k].sectnum].floorz);
                G_SetInterpolation(&sector[sprite[k].sectnum].ceilingz);
                break;
            case 17:
                G_SetInterpolation(&sector[sprite[k].sectnum].floorz);
                G_SetInterpolation(&sector[sprite[k].sectnum].ceilingz);
                break;
            case 0:
            case 5:
            case 6:
            case 11:
            case 14:
            case 15:
            case 16:
            case 26:
            case 30:
                Sect_SetInterpolation(SECT(k), true);
                break;
            }

            k = nextspritestat[k];
        }

        G_BackupInterpolations();
        for (i = g_animateCount - 1; i >= 0; i--)
            G_SetInterpolation(G_GetAnimationPointer(i));

        g_showShareware = 0;
        everyothertime = 0;

        //    clearbufbyte(connected,MAXPLAYERS,0x01010101);

        ready2send = 1;

        clearfifo();
        Net_WaitForPlayers();

        G_ResetTimers();
        calc_sector_reachability();
    }

    return(0);
corrupt:
    kclose(fil);
    Bsprintf(tempbuf,"Save game file \"%s\" is corrupt or of the wrong version.",fnptr);
    G_GameExit(tempbuf);
    return -1;
}

int32_t G_SavePlayer(int32_t spot)
{
    int32_t i;
    intptr_t j;
    char fn[13];
    char mpfn[13];
    char *fnptr, *scriptptrs;
    FILE *fil;
    int32_t bv = BYTEVERSION;

    strcpy(fn, "egam0.sav");
    strcpy(mpfn, "egamA_00.sav");

    if (spot < 0)
    {
        multiflag = 1;
        multiwhat = 1;
        multipos = -spot-1;
        return -1;
    }

    Net_WaitForPlayers();

    if (multiflag == 2 && multiwho != myconnectindex)
    {
        fnptr = mpfn;
        mpfn[4] = spot + 'A';

        if (ud.multimode > 9)
        {
            mpfn[6] = (multiwho/10) + '0';
            mpfn[7] = multiwho + '0';
        }
        else mpfn[7] = multiwho + '0';
    }
    else
    {
        fnptr = fn;
        fn[4] = spot + '0';
    }

    {
        char temp[BMAX_PATH];
        if (g_modDir[0] != '/')
            Bsnprintf(temp, sizeof(temp), "%s/%s", g_modDir,fnptr);
        else Bsnprintf(temp, sizeof(temp), "%s",fnptr);
        if ((fil = fopen(temp,"wb")) == 0) return(-1);
    }

    ready2send = 0;

    Bsprintf(g_szBuf,"EDuke32");
    i=strlen(g_szBuf);
    dfwrite(&i,sizeof(i),1,fil);
    dfwrite(g_szBuf,i,1,fil);

    dfwrite(&bv,sizeof(bv),1,fil);
    dfwrite(&ud.multimode,sizeof(ud.multimode),1,fil);

    dfwrite(&ud.savegame[spot][0],19,1,fil);
    dfwrite(&ud.volume_number,sizeof(ud.volume_number),1,fil);
    dfwrite(&ud.level_number,sizeof(ud.level_number),1,fil);
    dfwrite(&ud.player_skill,sizeof(ud.player_skill),1,fil);

    dfwrite(&currentboardfilename[0],BMAX_PATH,1,fil);
    if (!waloff[TILE_SAVESHOT])
    {
        walock[TILE_SAVESHOT] = 254;
        g_cache.allocateBlock(&waloff[TILE_SAVESHOT],200*320,&walock[TILE_SAVESHOT]);
        clearbuf((void*)waloff[TILE_SAVESHOT],(200*320)/4,0);
        walock[TILE_SAVESHOT] = 1;
    }
    dfwrite((char *)waloff[TILE_SAVESHOT],320,200,fil);

    dfwrite(&ud.music_episode, sizeof(ud.music_episode), 1, fil);
    dfwrite(&ud.music_level, sizeof(ud.music_level), 1, fil);

    dfwrite(&ud.monsters_killed,sizeof(ud.monsters_killed),1,fil);
    dfwrite(&ud.total_monsters,sizeof(ud.total_monsters),1,fil);
    dfwrite(&ud.playerbest,sizeof(ud.playerbest),1,fil);

    dfwrite(&numwalls,sizeof(numwalls),1,fil);
    dfwrite(&wall[0],sizeof(walltype),MAXWALLS,fil);
    dfwrite(&numsectors,sizeof(numsectors),1,fil);
    dfwrite(&sector[0],sizeof(sectortype),MAXSECTORS,fil);
    dfwrite(&sprite[0],sizeof(spritetype),MAXSPRITES,fil);
#if defined(USE_OPENGL)
    for (i=0; i<MAXSPRITES; i++)
        if (spriteext[i].mdanimtims)
        {
            spriteext[i].mdanimtims=spriteext[i].mdanimtims-mdtims;
            if (!spriteext[i].mdanimtims)
                spriteext[i].mdanimtims++;
        }
#endif
    dfwrite(&spriteext[0],sizeof(spriteext_t),MAXSPRITES,fil);

#ifdef YAX_ENABLE
    dfwrite(&numyaxbunches,sizeof(numyaxbunches),1,fil);
# if !defined NEW_MAP_FORMAT
    dfwrite(yax_bunchnum,sizeof(yax_bunchnum[0]),numsectors,fil);
    dfwrite(yax_nextwall,sizeof(yax_nextwall[0]),numwalls,fil);
# endif
#endif

#if 0 //POLYMER
    dfwrite(&lightcount,sizeof(lightcount),1,fil);
    dfwrite(&prlights[0],sizeof(_prlight),lightcount,fil);
#else
    i = 0;
    dfwrite(&i,sizeof(i),1,fil);
#endif // POLYMER

#if defined(USE_OPENGL)
    for (i=0; i<MAXSPRITES; i++)if (spriteext[i].mdanimtims)spriteext[i].mdanimtims+=mdtims;
#endif
    dfwrite(&Numsprites,sizeof(Numsprites),1,fil);
    dfwrite(&tailspritefree,sizeof(tailspritefree),1,fil);
    dfwrite(&headspritesect[0],sizeof(headspritesect[0]),MAXSECTORS+1,fil);
    dfwrite(&prevspritesect[0],sizeof(prevspritesect[0]),MAXSPRITES,fil);
    dfwrite(&nextspritesect[0],sizeof(nextspritesect[0]),MAXSPRITES,fil);
    dfwrite(&headspritestat[STAT_DEFAULT],sizeof(headspritestat[0]),MAXSTATUS+1,fil);
    dfwrite(&prevspritestat[STAT_DEFAULT],sizeof(prevspritestat[0]),MAXSPRITES,fil);
    dfwrite(&nextspritestat[STAT_DEFAULT],sizeof(nextspritestat[0]),MAXSPRITES,fil);
    dfwrite(&g_numCyclers,sizeof(g_numCyclers),1,fil);
    dfwrite(&cyclers[0][0],sizeof(cyclers),1,fil);
    for (i=0; i<ud.multimode; i++)
        dfwrite(g_player[i].ps,sizeof(DukePlayer_t),1,fil);
    dfwrite(&g_playerSpawnPoints,sizeof(g_playerSpawnPoints),1,fil);
    dfwrite(&g_numAnimWalls,sizeof(g_numAnimWalls),1,fil);
    dfwrite(&animwall,sizeof(animwall),1,fil);
    dfwrite(&g_origins[0], sizeof(vec2_t), sizeof(g_origins) / sizeof(vec2_t), fil);
    dfwrite(&g_spriteDeleteQueuePos,sizeof(g_spriteDeleteQueuePos),1,fil);
    dfwrite(&g_spriteDeleteQueueSize,sizeof(g_spriteDeleteQueueSize),1,fil);
    dfwrite(&SpriteDeletionQueue[0],sizeof(*SpriteDeletionQueue),g_spriteDeleteQueueSize,fil);
    dfwrite(&g_mirrorCount,sizeof(g_mirrorCount),1,fil);
    dfwrite(&g_mirrorWall[0],sizeof(g_mirrorWall),1,fil);
    dfwrite(&g_mirrorSector[0],sizeof(g_mirrorSector),1,fil);
    dfwrite(&show2dsector[0],sizeof(show2dsector),1,fil);

    dfwrite(&g_numClouds,sizeof(g_numClouds),1,fil);
    dfwrite(&g_cloudSect[0],sizeof(g_cloudSect),1,fil);
    dfwrite(&g_cloudX,sizeof(g_cloudX),1,fil);
    dfwrite(&g_cloudY,sizeof(g_cloudY),1,fil);

    for (i=0; i<MAXTILES; i++)
        if (g_tile[i].execPtr)
        {
            j = (intptr_t)g_tile[i].execPtr-(intptr_t)&apScript[0];
            g_tile[i].execPtr = (intptr_t *)j;
        }

    for (i=0; i<MAXTILES; i++)
        if (g_tile[i].loadPtr)
        {
            j = (intptr_t)g_tile[i].loadPtr-(intptr_t)&apScript[0];
            g_tile[i].loadPtr = (intptr_t *)j;
        }


    dfwrite(&g_tile[0], sizeof(tiledata_t), MAXTILES, fil);
    dfwrite(&g_scriptSize,sizeof(g_scriptSize),1,fil);
    scriptptrs = (char*)Xcalloc(1, g_scriptSize * sizeof(scriptptrs));
    for (i=0; i<g_scriptSize; i++)
    {
        if (bitptr[i>>3]&(BITPTR_POINTER<<(i&7)))
        {
//            scriptptrs[i] = 1;
            j = (intptr_t)apScript[i] - (intptr_t)&apScript[0];
            apScript[i] = j;
        }
        //      else scriptptrs[i] = 0;
    }

//    dfwrite(&scriptptrs[0],sizeof(scriptptrs),g_scriptSize,fil);
    dfwrite(&bitptr[0],sizeof(bitptr[0]),(g_scriptSize+7)>>3,fil);
    dfwrite(&apScript[0],sizeof(apScript[0]),g_scriptSize,fil);

    for (i=0; i<g_scriptSize; i++)
        if (bitptr[i>>3]&(BITPTR_POINTER<<(i&7)))
        {
            j = apScript[i]+(intptr_t)&apScript[0];
            apScript[i] = j;
        }

    for (i=0; i<MAXTILES; i++)
        if (g_tile[i].execPtr)
        {
            j = (intptr_t)g_tile[i].execPtr+(intptr_t)&apScript[0];
            g_tile[i].execPtr = (intptr_t *)j;
        }

    for (i=0; i<MAXTILES; i++)
        if (g_tile[i].loadPtr)
        {
            j = (intptr_t)g_tile[i].loadPtr+(intptr_t)&apScript[0];
            g_tile[i].loadPtr = (intptr_t *)j;
        }

    Xfree(scriptptrs);
    scriptptrs = (char*)Xcalloc(1, MAXSPRITES * sizeof(scriptptrs));

    for (i=0; i<MAXSPRITES; i++)
    {
        scriptptrs[i] = 0;

        if (g_tile[PN(i)].execPtr == 0) continue;

        j = (intptr_t)&apScript[0];

        if (T2(i) >= j && T2(i) < (intptr_t)(&apScript[g_scriptSize]))
        {
            scriptptrs[i] |= 1;
            T2(i) -= j;
        }
        if (T5(i) >= j && T5(i) < (intptr_t)(&apScript[g_scriptSize]))
        {
            scriptptrs[i] |= 2;
            T5(i) -= j;
        }
        if (T6(i) >= j && T6(i) < (intptr_t)(&apScript[g_scriptSize]))
        {
            scriptptrs[i] |= 4;
            T6(i) -= j;
        }
    }

    dfwrite(&scriptptrs[0],sizeof(scriptptrs),MAXSPRITES,fil);
    dfwrite(&actor[0],sizeof(ActorData_t),MAXSPRITES,fil);

    for (i=0; i<MAXSPRITES; i++)
    {
        if (g_tile[PN(i)].execPtr == 0) continue;
        j = (intptr_t)&apScript[0];

        if (scriptptrs[i]&1)
            T2(i) += j;
        if (scriptptrs[i]&2)
            T5(i) += j;
        if (scriptptrs[i]&4)
            T6(i) += j;
    }

    // Projectiles
    sv_preprojectilesave();
    dfwrite(&savegame_projectiles[0],sizeof(savegame_projectiles),1,fil);
    if (savegame_projectilecnt > 0)
        dfwrite(&savegame_projectiledata[0],sizeof(savegame_projectiledata[0]),savegame_projectilecnt,fil);
    sv_postprojectilesave();

    dfwrite(&lockclock,sizeof(lockclock),1,fil);
    dfwrite(&g_pskyidx,sizeof(g_pskyidx),1,fil);
    dfwrite(&g_animateCount,sizeof(g_animateCount),1,fil);
    dfwrite(&sectorAnims[0],sizeof(sectorAnims[0]),MAXANIMATES,fil);

    dfwrite(&g_earthquakeTime,sizeof(g_earthquakeTime),1,fil);
    dfwrite(&ud.from_bonus,sizeof(ud.from_bonus),1,fil);
    dfwrite(&ud.secretlevel,sizeof(ud.secretlevel),1,fil);
    dfwrite(&ud.max_secret_rooms,sizeof(ud.max_secret_rooms),1,fil);
    dfwrite(&ud.secret_rooms,sizeof(ud.secret_rooms),1,fil);
    dfwrite(&ud.dmflags,sizeof(ud.dmflags),1,fil);
    dfwrite(&ud.god,sizeof(ud.god),1,fil);
    dfwrite(&ud.auto_run,sizeof(ud.auto_run),1,fil);
    dfwrite(&ud.crosshair,sizeof(ud.crosshair),1,fil);
    dfwrite(&ud.last_level,sizeof(ud.last_level),1,fil);
    dfwrite(&ud.eog,sizeof(ud.eog),1,fil);
    dfwrite(&ud.coop,sizeof(ud.coop),1,fil);
    dfwrite(&g_curViewscreen,sizeof(g_curViewscreen),1,fil);
    dfwrite(&connecthead,sizeof(connecthead),1,fil);
    dfwrite(connectpoint2,sizeof(connectpoint2),1,fil);
    dfwrite(&g_numPlayerSprites,sizeof(g_numPlayerSprites),1,fil);
    for (i=0; i<MAXPLAYERS; i++)
        dfwrite((int16_t *)&g_player[i].frags[0],sizeof(g_player[i].frags),1,fil);

    dfwrite(&ud.got_access, sizeof(ud.got_access), 1, fil);

    dfwrite(&randomseed,sizeof(randomseed),1,fil);
    dfwrite(&g_globalRandom,sizeof(g_globalRandom),1,fil);
    //dfwrite(&parallaxyscale,sizeof(parallaxyscale),1,fil);

    for (i=0; i<MAXQUOTES; i++)
    {
        if (apStrings[i] != NULL)
        {
            dfwrite(&i,sizeof(i),1,fil);
            dfwrite(apStrings[i],MAXQUOTELEN, 1, fil);
        }
    }
    dfwrite(&i,sizeof(i),1,fil);

    dfwrite(&g_numXStrings,sizeof(g_numXStrings),1,fil);
    for (i=0; i<g_numXStrings; i++)
    {
        if (apXStrings[i] != NULL)
            dfwrite(apXStrings[i],MAXQUOTELEN, 1, fil);
    }

    dfwrite(&g_dynTileList[0],sizeof(g_dynTileList[0]),g_dynTileListSize,fil);

    Gv_WriteSave(fil);

    fclose(fil);

    if (ud.multimode < 2)
    {
        strcpy(apStrings[QUOTE_RESERVED4],"GAME SAVED");
        P_DoQuote(QUOTE_RESERVED4,g_player[myconnectindex].ps);
    }

    ready2send = 1;

    Net_WaitForPlayers();

    ototalclock = (int32_t)totalclock;

    return(0);
}
  