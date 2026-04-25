//-------------------------------------------------------------------------
/*
Copyright (C) 2016 EDuke32 developers and contributors

This file is part of EDuke32.

EDuke32 is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
//-------------------------------------------------------------------------

//#include "anim.h"
#include "colmatch.h"
#include "compat.h"
//#include "demo.h"
#include "duke3d.h"
//#include "input.h"
#include "mdsprite.h"
//#include "sbar.h"
#include "screens.h"

void P_SetGamePalette(DukePlayer_t* player, uint32_t palid, int32_t set)
{
    if (palid >= MAXBASEPALS)
        palid = 0;

    player->palette = palid;

    if (player != g_player[screenpeek].ps)
        return;

    videoSetPalette(ud.brightness >> 2, palid, set);
}

void G_ShowVote(void)
{
    if (g_player[myconnectindex].gotvote == 0 && vote.starter != -1 && vote.starter != myconnectindex)
    {
        Bsprintf(tempbuf, "%s^00 HAS CALLED A VOTE FOR MAP", g_player[vote.starter].user_name);
        gametext(160, 40, tempbuf, 0, 2 + 8 + 16);

        Bsprintf(tempbuf, "%s (E%dL%d)", g_mapInfo[vote.episode * MAXLEVELS + vote.level].name, vote.episode + 1, vote.level + 1);
        gametext(160, 48, tempbuf, 0, 2 + 8 + 16);

        gametext(160, 70, "PRESS F1 TO ACCEPT, F2 TO DECLINE", 0, 2 + 8 + 16);
    }
}

#define score_inventory(x, y, picnum, cond) rotatesprite_((x) << 16, (y) << 16, 24576, 0, picnum, 0, (cond) ? 0 : 4, RS_AUTO | RS_TOPLEFT, (cond) ? 0 : 128, 0, 0, 0, xdim, ydim)
void G_ShowScores(bool bonus)
{
    int i, y, nemesis_kills, nemesis;

    if (playerswhenstarted < 2 || !(Gametypes[ud.coop].flags & (GAMETYPE_COOP | GAMETYPE_SCORESHEET)))
        return;

#if 0
    static struct score_t
    {
        int32_t frags;
        int32_t deaths;
        uint8_t id;
    } scores[MAXPLAYERS];
#endif

    // NetDuke32 To-Do: Sort list by rank, mark leader. Probably want an inline struct here.

    int yoff = 34;

    if (playerswhenstarted >= 16) // Move board up a bit if we're at 16 players or more.
        yoff -= 4;

    if (bonus)
    {
        rotatesprite(0, 0, 65536L, 0, MENUSCREEN, 16, 0, 2 + 8 + 16 + 64, 0, 0, xdim - 1, ydim - 1);

        if (playerswhenstarted < 14)
            gametext(160, 176, "PRESS ANY KEY TO CONTINUE", 0, 2 + 8 + 16);
    }

    // Duke3D + Atomic Edition logos
    rotatesprite(160 << 16, yoff << 16, 65536L, 0, INGAMEDUKETHREEDEE, 0, 0, 10, 0, 0, xdim - 1, ydim - 1);
    if (PLUTOPAK)   // JBF 20030804
        rotatesprite((260) << 16, (yoff + 2) << 16, 65536L, 0, PLUTOPAKSPRITE + 2, 0, 0, 2 + 8, 0, 0, xdim - 1, ydim - 1);

    // Level name
    gametext(160, yoff + 26, g_mapInfo[(ud.volume_number * MAXLEVELS) + ud.last_level - 1].name, 0, 2 + 8 + 16);

    yoff += 36;
    const int start_x = 63;
    const int name_x = start_x + 12;
    const int frags_hp_x = start_x + ((Gametypes[ud.coop].flags & GAMETYPE_COOP) ? 57 : 66);
    const int deaths_kd_x = start_x + 98;
    const int misc_x = start_x + ((Gametypes[ud.coop].flags & GAMETYPE_COOP) ? 128 : 134);
    const int ping_x = start_x + 180;

    int score_start_y = yoff; // Store Y so we can draw the headers on top of the transparent background.

    yoff += 10;

    // Background
    rotatesprite_(0, 0, 65536L, 0, BETASCREEN, 0, 4, RS_AUTO | RS_TOPLEFT, 128, 0, get_xdim(start_x - 6), get_ydim(score_start_y-2), get_xdim(ping_x + 21), get_ydim(yoff+(7*playerswhenstarted)));

    // Headers
    minitext(start_x, score_start_y, "#", 8, 2 + 8 + 16 + 128);
    minitext(name_x, score_start_y, "NAME", 8, 2 + 8 + 16 + 128);
    minitext(frags_hp_x, score_start_y, (Gametypes[ud.coop].flags & GAMETYPE_COOP) ? "HP/ARMOR" : "FRAGS", 8, 2 + 8 + 16 + 128);
    minitext(deaths_kd_x, score_start_y, (Gametypes[ud.coop].flags & GAMETYPE_COOP) ? "K/D" : "DEATHS", 8, 2 + 8 + 16 + 128);
    minitext(misc_x, score_start_y, (Gametypes[ud.coop].flags & GAMETYPE_COOP) ? "INVENTORY" : "NEMESIS", 8, 2 + 8 + 16 + 128);
    minitext(ping_x, score_start_y, "PING", 8, 2 + 8 + 16 + 128);

    for (i = 0; i < playerswhenstarted; i++)
    {
        DukePlayer_t* p = g_player[i].ps;

        Bsprintf(tempbuf, "%d", i + 1);
        minitext(start_x, yoff, tempbuf, 0, 2 + 8 + 16 + 128);

        minitext(name_x, yoff, g_player[i].user_name, p->palookup, 2 + 8 + 16 + 128);

        if (Gametypes[ud.coop].flags & GAMETYPE_COOP)
        {
            if (g_player[i].connected)
            {
                Bsprintf(tempbuf, "^02%d^12/^06%d", TrackerCast(sprite[p->i].extra), p->inv_amount[GET_SHIELD]);
            }
            else
            {
                Bsprintf(tempbuf, "^10N/A"); // Don't show health for a disconnected player.
            }

            minitext(frags_hp_x, yoff, tempbuf, 2, 2 + 8 + 16 + 128);

            Bsprintf(tempbuf, "%d^12/^02%d", p->actors_killed, p->deaths);
            minitext(deaths_kd_x, yoff, tempbuf, 0, 2 + 8 + 16 + 128);

            score_inventory(misc_x, yoff, FIRSTAID_ICON, p->inv_amount[GET_FIRSTAID] > 0);
            score_inventory(misc_x + 8, yoff, STEROIDS_ICON, p->inv_amount[GET_STEROIDS] >= 400);
            score_inventory(misc_x + 14, yoff, HOLODUKE_ICON, p->inv_amount[GET_HOLODUKE] > 0);
            score_inventory(misc_x + 19, yoff, JETPACK_ICON, p->inv_amount[GET_JETPACK] > 0);
            score_inventory(misc_x + 25, yoff, HEAT_ICON, p->inv_amount[GET_HEATS] > 0);
            score_inventory(misc_x + 32, yoff, AIRTANK_ICON, p->inv_amount[GET_SCUBA] > 0);
            score_inventory(misc_x + 39, yoff, BOOT_ICON, p->inv_amount[GET_BOOTS] > 0);
        }
        else
        {
            nemesis = -1;
            nemesis_kills = 0;
            for (y = 0; y < playerswhenstarted; y++)
            {
                int killer_frags = g_player[y].frags[i];

                // If this killer has more frags on this player than last nemesis, make them the new nemesis.
                if (killer_frags > nemesis_kills)
                {
                    nemesis_kills = killer_frags;
                    nemesis = y;
                }
            }

            Bsprintf(tempbuf, "%-4d", g_player[i].ps->frag - g_player[i].ps->fraggedself);
            minitext(frags_hp_x, yoff, tempbuf, 2, 2 + 8 + 16 + 128);

            Bsprintf(tempbuf, "%-4d", p->deaths);
            minitext(deaths_kd_x, yoff, tempbuf, 2, 2 + 8 + 16 + 128);

            if (nemesis > -1)
            {
                minitext(misc_x, yoff, g_player[nemesis].user_name, g_player[nemesis].ps->palookup, 2 + 8 + 16 + 128);
            }
            else if (g_player[i].ps->fraggedself > nemesis_kills)
            {
                minitext(misc_x, yoff, "Themself!", 6, 2 + 8 + 16 + 128);
            }
            else
            {
                minitext(misc_x, yoff, "Nobody", 12, 2 + 8 + 16 + 128);
            }
        }

        if (i == connecthead)
        {
            minitext(ping_x, yoff, "HOST", 8, 2 + 8 + 16 + 128);
        }
        else if (g_player[i].isBot)
        {
            minitext(ping_x, yoff, "BOT", 2, 2 + 8 + 16 + 128);
        }
        else if (!g_player[i].connected)
        {
            minitext(ping_x, yoff, "N/A", 2, 2 + 8 + 16 + 128);
        }
        else
        {
            Bsprintf(tempbuf, "%d", g_player[i].ping);
            minitext(ping_x, yoff, tempbuf, 12, 2 + 8 + 16 + 128);
        }

        yoff += 7;
    }
}

void G_FadePalette(int r, int g, int b, int e)
{
    int tc;

    videoFadePalette(clamp(r << 2, 0, 255), clamp(g << 2, 0, 255), clamp(b << 2, 0, 255), (e&127) << 2);
    if (videoGetRenderMode() >= 3) pus = pub = NUMPAGES; // JBF 20040110: redraw the status bar next time
    if ((e & 128) == 0)
    {
        videoNextPage();
        for (tc = (int32_t)totalclock; totalclock < tc + 4; gameHandleEvents());
    }
}

void fadepal(int r, int g, int b, int start, int end, int step)
{
    if (videoGetRenderMode() >= 3) return;
    if (step > 0) for (; start < end; start += step) G_FadePalette(r, g, b, start);
    else for (; start >= end; start += step) G_FadePalette(r, g, b, start);
}

void G_DoOrderScreen(void)
{
    videoSetViewableArea(0, 0, xdim - 1, ydim - 1);

    fadepal(0, 0, 0, 0, 63, 7);
    //g_player[myconnectindex].ps->palette = palette;
    P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 1);    // JBF 20040308
    KB_FlushKeyboardQueue();
    rotatesprite(0, 0, 65536L, 0, ORDERING, 0, 0, 2 + 8 + 16 + 64, 0, 0, xdim - 1, ydim - 1);
    fadepal(0, 0, 0, 63, 0, -7);

    while (!KB_KeyWaiting())
        gameHandleEvents();

    fadepal(0, 0, 0, 0, 63, 7);
    KB_FlushKeyboardQueue();
    rotatesprite(0, 0, 65536L, 0, ORDERING + 1, 0, 0, 2 + 8 + 16 + 64, 0, 0, xdim - 1, ydim - 1);
    fadepal(0, 0, 0, 63, 0, -7);

    while (!KB_KeyWaiting())
        gameHandleEvents();

    fadepal(0, 0, 0, 0, 63, 7);
    KB_FlushKeyboardQueue();
    rotatesprite(0, 0, 65536L, 0, ORDERING + 2, 0, 0, 2 + 8 + 16 + 64, 0, 0, xdim - 1, ydim - 1);
    fadepal(0, 0, 0, 63, 0, -7);

    while (!KB_KeyWaiting())
        gameHandleEvents();

    fadepal(0, 0, 0, 0, 63, 7);
    KB_FlushKeyboardQueue();
    rotatesprite(0, 0, 65536L, 0, ORDERING + 3, 0, 0, 2 + 8 + 16 + 64, 0, 0, xdim - 1, ydim - 1);
    fadepal(0, 0, 0, 63, 0, -7);

    while (!KB_KeyWaiting())
        gameHandleEvents();
}

static int32_t G_PrintTime_ClockPad(void)
{
    int32_t clockpad = 2;
    int32_t ii, ij;

    for (ii = g_player[myconnectindex].ps->player_par / (REALGAMETICSPERSEC * 60), ij = 1; ii > 9; ii /= 10, ij++) {}
    clockpad = max(clockpad, ij);
    if (!(ud.volume_number == 0 && ud.last_level - 1 == 7 && boardfilename[0]))
    {
        for (ii = g_mapInfo[G_LastMapInfoIndex()].partime / (REALGAMETICSPERSEC * 60), ij = 1; ii > 9; ii /= 10, ij++) {}
        clockpad = max(clockpad, ij);
        if (!NAM_WW2GI && g_mapInfo[G_LastMapInfoIndex()].designertime)
        {
            for (ii = g_mapInfo[G_LastMapInfoIndex()].designertime / (REALGAMETICSPERSEC * 60), ij = 1; ii > 9; ii /= 10, ij++) {}
            clockpad = max(clockpad, ij);
        }
    }
    if (ud.playerbest > 0) for (ii = ud.playerbest / (REALGAMETICSPERSEC * 60), ij = 1; ii > 9; ii /= 10, ij++) {}
    clockpad = max(clockpad, ij);

    return clockpad;
}

static const char* G_PrintTime2(int32_t time)
{
    Bsprintf(tempbuf, "%0*d:%02d", G_PrintTime_ClockPad(), time / (REALGAMETICSPERSEC * 60), (time / REALGAMETICSPERSEC) % 60);
    return tempbuf;
}
static const char* G_PrintTime3(int32_t time)
{
    Bsprintf(tempbuf, "%0*d:%02d.%02d", G_PrintTime_ClockPad(), time / (REALGAMETICSPERSEC * 60), (time / REALGAMETICSPERSEC) % 60, ((time % REALGAMETICSPERSEC) * 33) / 10);
    return tempbuf;
}

const char* G_PrintYourTime(void)
{
    return G_PrintTime3(g_player[myconnectindex].ps->player_par);
}
const char* G_PrintParTime(void)
{
    if (ud.last_level < 1)
        return "<invalid>";
    return G_PrintTime2(g_mapInfo[G_LastMapInfoIndex()].partime);
}
const char* G_PrintDesignerTime(void)
{
    if (ud.last_level < 1)
        return "<invalid>";
    return G_PrintTime2(g_mapInfo[G_LastMapInfoIndex()].designertime);
}
const char* G_PrintBestTime(void)
{
    return G_PrintTime3(ud.playerbest);
}

static int32_t G_PlaySoundWhileNoInput(int32_t soundnum)
{
    auto const voice = S_PlaySound(soundnum);

    KB_FlushKeyboardQueue();
    MOUSE_ClearButton(M_LEFTBUTTON);

    while (FX_SoundActive(voice))
    {
        gameHandleEvents();
        if (KB_KeyWaiting() || (MOUSE_GetButtons() & M_LEFTBUTTON))
        {
            KB_FlushKeyboardQueue();
            MOUSE_ClearButton(M_LEFTBUTTON);
            return 1;
        }
    }

    return 0;
}

// Keep on bottom
void G_BonusScreen(int bonusonly)
{
    int32_t clockpad = 2;
    int t, gfx_offset;
    int bonuscnt;
    char* lastmapname;

    int breathe[] =
    {
        0,  30,VICTORY1 + 1,176,59,
        30,  60,VICTORY1 + 2,176,59,
        60,  90,VICTORY1 + 1,176,59,
        90, 120,0         ,176,59
    };

    int bossmove[] =
    {
        0, 120,VICTORY1 + 3,86,59,
        220, 260,VICTORY1 + 4,86,59,
        260, 290,VICTORY1 + 5,86,59,
        290, 320,VICTORY1 + 6,86,59,
        320, 350,VICTORY1 + 7,86,59,
        350, 380,VICTORY1 + 8,86,59,
        350, 380,VICTORY1 + 8,86,59 // duplicate row to alleviate overflow in the for loop below "boss"
    };

    G_UpdateAppTitle();

    if (ud.volume_number == 0 && ud.last_level == 8 && boardfilename[0])
    {
        lastmapname = Bstrrchr(boardfilename, '\\');
        if (!lastmapname) lastmapname = Bstrrchr(boardfilename, '/');
        if (!lastmapname) lastmapname = boardfilename;
    }
    else lastmapname = g_mapInfo[(ud.volume_number * MAXLEVELS) + ud.last_level - 1].name;

    bonuscnt = 0;

    fadepal(0, 0, 0, 0, 64, 7);
    videoSetViewableArea(0, 0, xdim - 1, ydim - 1);
    videoClearScreen(0L);
    videoNextPage();
    renderFlushPerms();

    FX_StopAllSounds();
    FX_SetReverb(0L);
    CONTROL_BindsEnabled = 1; // so you can use your screenshot bind on the score screens

    if (bonusonly) goto FRAGBONUS;

    if (numplayers < 2 && ud.eog && ud.from_bonus == 0)
    {
        switch (ud.volume_number)
        {
            case 0:
            {
                if (ud.lockout == 0)
                {
                    P_SetGamePalette(g_player[myconnectindex].ps, ENDINGPAL, 11); // JBF 20040308
                    videoClearViewableArea(0L);
                    rotatesprite(0, 50 << 16, 65536L, 0, VICTORY1, 0, 0, 2 + 8 + 16 + 64 + 128, 0, 0, xdim - 1, ydim - 1);
                    videoNextPage();
                    //g_player[myconnectindex].ps->palette = endingpal;
                    fadepal(0, 0, 0, 63, 0, -1);

                    KB_FlushKeyboardQueue();
                    totalclock = 0;
                    while (1)
                    {
                        videoClearViewableArea(0L);
                        rotatesprite(0, 50 << 16, 65536L, 0, VICTORY1, 0, 0, 2 + 8 + 16 + 64 + 128, 0, 0, xdim - 1, ydim - 1);

                        // boss
                        if (totalclock > 390 && totalclock < 780)
                            for (t = 0; t < 35; t += 5) if (bossmove[t + 2] && (totalclock % 390) > bossmove[t] && (totalclock % 390) <= bossmove[t + 1])
                            {
                                if (t == 10 && bonuscnt == 1)
                                {
                                    S_PlaySound(SHOTGUN_FIRE);
                                    S_PlaySound(SQUISHED);
                                    bonuscnt++;
                                }
                                rotatesprite(bossmove[t + 3] << 16, bossmove[t + 4] << 16, 65536L, 0, bossmove[t + 2], 0, 0, 2 + 8 + 16 + 64 + 128, 0, 0, xdim - 1, ydim - 1);
                            }

                        // Breathe
                        if (totalclock < 450 || totalclock >= 750)
                        {
                            if (totalclock >= 750)
                            {
                                rotatesprite(86 << 16, 59 << 16, 65536L, 0, VICTORY1 + 8, 0, 0, 2 + 8 + 16 + 64 + 128, 0, 0, xdim - 1, ydim - 1);
                                if (totalclock >= 750 && bonuscnt == 2)
                                {
                                    S_PlaySound(DUKETALKTOBOSS);
                                    bonuscnt++;
                                }

                            }
                            for (t = 0; t < 20; t += 5)
                                if (breathe[t + 2] && (totalclock % 120) > breathe[t] && (totalclock % 120) <= breathe[t + 1])
                                {
                                    if (t == 5 && bonuscnt == 0)
                                    {
                                        S_PlaySound(BOSSTALKTODUKE);
                                        bonuscnt++;
                                    }
                                    rotatesprite(breathe[t + 3] << 16, breathe[t + 4] << 16, 65536L, 0, breathe[t + 2], 0, 0, 2 + 8 + 16 + 64 + 128, 0, 0, xdim - 1, ydim - 1);
                                }
                        }

                        gameHandleEvents();
                        videoNextPage();
                        if (KB_KeyWaiting())
                            break;
                    }
                }

                fadepal(0, 0, 0, 0, 64, 1);

                KB_FlushKeyboardQueue();
                //g_player[myconnectindex].ps->palette = palette;
                P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 11);   // JBF 20040308

                rotatesprite(0, 0, 65536L, 0, 3292, 0, 0, 2 + 8 + 16 + 64, 0, 0, xdim - 1, ydim - 1);
                IFISSOFTMODE fadepal(0, 0, 0, 63, 0, -1);
                else videoNextPage();

                while (!KB_KeyWaiting() && !(MOUSE_GetButtons() & M_LEFTBUTTON))
                    gameHandleEvents();

                fadepal(0, 0, 0, 0, 64, 1);
                S_StopMusic();
                FX_StopAllSounds();
                break;
            }
            case 1:
            {
                S_StopMusic();
                videoClearViewableArea(0L);
                videoNextPage();

                if (ud.lockout == 0)
                {
                    G_PlayAnim("cineov2.anm", 1);
                    KB_FlushKeyboardQueue();
                    videoClearViewableArea(0L);
                    videoNextPage();
                }

                S_PlaySound(PIPEBOMB_EXPLODE);

                fadepal(0, 0, 0, 0, 64, 1);
                videoSetViewableArea(0, 0, xdim - 1, ydim - 1);
                KB_FlushKeyboardQueue();
                //g_player[myconnectindex].ps->palette = palette;
                P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 11);   // JBF 20040308
                rotatesprite(0, 0, 65536L, 0, 3293, 0, 0, 2 + 8 + 16 + 64, 0, 0, xdim - 1, ydim - 1);
                IFISSOFTMODE fadepal(0, 0, 0, 63, 0, -1);
                else videoNextPage();

                while (!KB_KeyWaiting() && !(MOUSE_GetButtons() & M_LEFTBUTTON))
                    gameHandleEvents();

                IFISSOFTMODE fadepal(0, 0, 0, 0, 64, 1);

                break;
            }
            case 3:
            {
                videoSetViewableArea(0, 0, xdim - 1, ydim - 1);

                S_StopMusic();
                videoClearViewableArea(0L);
                videoNextPage();

                if (ud.lockout == 0)
                {
                    KB_FlushKeyboardQueue();
                    G_PlayAnim("vol4e1.anm", 8);
                    videoClearViewableArea(0L);
                    videoNextPage();
                    G_PlayAnim("vol4e2.anm", 10);
                    videoClearViewableArea(0L);
                    videoNextPage();
                    G_PlayAnim("vol4e3.anm", 11);
                    videoClearViewableArea(0L);
                    videoNextPage();
                }

                FX_StopAllSounds();
                S_PlaySound(ENDSEQVOL3SND4);
                KB_FlushKeyboardQueue();

                //g_player[myconnectindex].ps->palette = palette;
                P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 11);   // JBF 20040308
                IFISSOFTMODE G_FadePalette(0, 0, 0, 63);
                videoClearViewableArea(0L);
                menutext(160, 60, 0, 0, "THANKS TO ALL OUR");
                menutext(160, 60 + 16, 0, 0, "FANS FOR GIVING");
                menutext(160, 60 + 16 + 16, 0, 0, "US BIG HEADS.");
                menutext(160, 70 + 16 + 16 + 16, 0, 0, "LOOK FOR A DUKE NUKEM 3D");
                menutext(160, 70 + 16 + 16 + 16 + 16, 0, 0, "SEQUEL SOON.");
                videoNextPage();

                fadepal(0, 0, 0, 63, 0, -3);

                KB_FlushKeyboardQueue();
                while (!KB_KeyWaiting() && !(MOUSE_GetButtons() & M_LEFTBUTTON))
                    gameHandleEvents();

                fadepal(0, 0, 0, 0, 64, 3);

                videoClearViewableArea(0L);
                videoNextPage();
                MOUSE_ClearButton(M_LEFTBUTTON);

                G_PlayAnim("DUKETEAM.ANM", 4);

                KB_FlushKeyboardQueue();
                while (!KB_KeyWaiting() && !(MOUSE_GetButtons() & M_LEFTBUTTON))
                    gameHandleEvents();

                videoClearViewableArea(0L);
                videoNextPage();
                IFISSOFTMODE G_FadePalette(0, 0, 0, 63);

                FX_StopAllSounds();
                KB_FlushKeyboardQueue();
                MOUSE_ClearButton(M_LEFTBUTTON);

                break;
            }

            case 2:
            {
                S_StopMusic();
                videoClearViewableArea(0L);
                videoNextPage();
                if (ud.lockout == 0)
                {
                    fadepal(0, 0, 0, 63, 0, -1);
                    G_PlayAnim("cineov3.anm", 2);
                    KB_FlushKeyboardQueue();
                    ototalclock = (int32_t)totalclock + 200;

                    while (totalclock < ototalclock)
                        gameHandleEvents();

                    videoClearViewableArea(0L);
                    videoNextPage();

                    FX_StopAllSounds();
                }

                G_PlayAnim("RADLOGO.ANM", 3);

                if (ud.lockout == 0 && !KB_KeyWaiting() && !(MOUSE_GetButtons() & M_LEFTBUTTON))
                {
                    if (G_PlaySoundWhileNoInput(ENDSEQVOL3SND5)) goto ENDANM;
                    if (G_PlaySoundWhileNoInput(ENDSEQVOL3SND6)) goto ENDANM;
                    if (G_PlaySoundWhileNoInput(ENDSEQVOL3SND7)) goto ENDANM;
                    if (G_PlaySoundWhileNoInput(ENDSEQVOL3SND8)) goto ENDANM;
                    if (G_PlaySoundWhileNoInput(ENDSEQVOL3SND9)) goto ENDANM;
                }

                MOUSE_ClearButton(M_LEFTBUTTON);

                KB_FlushKeyboardQueue();
                totalclock = 0;
                while (!KB_KeyWaiting() && totalclock < 120 && !(MOUSE_GetButtons() & M_LEFTBUTTON))
                    gameHandleEvents();

            ENDANM:
                MOUSE_ClearButton(M_LEFTBUTTON);
                FX_StopAllSounds();

                KB_FlushKeyboardQueue();

                videoClearViewableArea(0L);

                break;
            }
        }
    }

FRAGBONUS:

    //g_player[myconnectindex].ps->palette = palette;
    P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 11);   // JBF 20040308
    IFISSOFTMODE G_FadePalette(0, 0, 0, 63);   // JBF 20031228
    KB_FlushKeyboardQueue();
    totalclock = 0;
    bonuscnt = 0;

    S_StopMusic();
    FX_StopAllSounds();

    if (playerswhenstarted > 1 && (Gametypes[ud.coop].flags & (GAMETYPE_COOP | GAMETYPE_SCORESHEET)))
    {
        videoClearScreen(0L);

        G_ShowScores(true);

        if (!(ud.config.MusicToggle == 0 || ud.config.MusicDevice < 0))
            S_PlaySound(BONUSMUSIC);

        videoNextPage();
        KB_FlushKeyboardQueue();
        fadepal(0, 0, 0, 63, 0, -7);

        while (totalclock < (TICRATE * 10))
        {
            gameHandleEvents();
            MUSIC_Update();

            if (engineFPSLimit())
            {
                videoClearScreen(0);
                G_ShowScores(true);
                videoNextPage();
            }

            if (KB_KeyWaiting())
            {
                KB_FlushKeyboardQueue();
                break;
            }
        }

        fadepal(0, 0, 0, 0, 64, 7);
    }

    if (bonusonly || ((ud.multimode > 1) && !GTFLAGS(GAMETYPE_COOP)))
        return;

    // Determine background based on Episode
    switch (ud.volume_number)
    {
    case 1:
        gfx_offset = 5;
        break;
    default:
        gfx_offset = 0;
        break;
    }

    // Draw Background text and play bonus music.
    rotatesprite(0, 0, 65536L, 0, BONUSSCREEN + gfx_offset, 0, 0, 2 + 8 + 16 + 64 + 128, 0, 0, xdim - 1, ydim - 1);

    menutext(160, 20 - 6, 0, 0, lastmapname);
    menutext(160, 36 - 6, 0, 0, "COMPLETED");

    gametext(160, 192, "PRESS ANY KEY TO CONTINUE", 16, 2 + 8 + 16);

    if (!S_CheckSoundPlaying(BONUSMUSIC) && !(ud.config.MusicToggle == 0 || ud.config.MusicDevice < 0))
        S_PlaySound(BONUSMUSIC);

    videoNextPage();
    KB_FlushKeyboardQueue();
    fadepal(0, 0, 0, 63, 0, -1);
    bonuscnt = 0;
    totalclock = 0;

    do
    {
        int yy = 0, zz;

        gameHandleEvents();
        MUSIC_Update();

        if (engineFPSLimit())
        {
            if (ud.gm & MODE_EOL)
            {
                videoClearScreen(0);
                rotatesprite(0, 0, 65536L, 0, BONUSSCREEN + gfx_offset, 0, 0, 2 + 8 + 16 + 64 + 128, 0, 0, xdim - 1, ydim - 1);

                if (totalclock > (1000000000L) && totalclock < (1000000320L)) // We've pressed a key, let's play an animation
                {
                    switch (((int32_t)totalclock >> 4) % 15)
                    {
                    case 0:
                        if (bonuscnt == 6)
                        {
                            bonuscnt++;
                            S_PlaySound(SHOTGUN_COCK);
                            switch (rand() & 3)
                            {
                            case 0:
                                S_PlaySound(BONUS_SPEECH1);
                                break;
                            case 1:
                                S_PlaySound(BONUS_SPEECH2);
                                break;
                            case 2:
                                S_PlaySound(BONUS_SPEECH3);
                                break;
                            case 3:
                                S_PlaySound(BONUS_SPEECH4);
                                break;
                            }
                        }
                        fallthrough__;
                    case 1:
                    case 4:
                    case 5:
                        rotatesprite(199 << 16, 31 << 16, 65536L, 0, BONUSSCREEN + 3 + gfx_offset, 0, 0, 2 + 8 + 16 + 64 + 128, 0, 0, xdim - 1, ydim - 1);
                        break;
                    case 2:
                    case 3:
                        rotatesprite(199 << 16, 31 << 16, 65536L, 0, BONUSSCREEN + 4 + gfx_offset, 0, 0, 2 + 8 + 16 + 64 + 128, 0, 0, xdim - 1, ydim - 1);
                        break;
                    }
                }
                else if (totalclock > (10240 + 120L))
                {
                    break; // Exit bonus screen if we're taking too much time, or we've played the "Duke cocking his gun" animation.
                }
                else
                {
                    switch (((int32_t)totalclock >> 5) & 3)
                    {
                    case 1:
                    case 3:
                        rotatesprite(199 << 16, 31 << 16, 65536L, 0, BONUSSCREEN + 1 + gfx_offset, 0, 0, 2 + 8 + 16 + 64 + 128, 0, 0, xdim - 1, ydim - 1);
                        break;
                    case 2:
                        rotatesprite(199 << 16, 31 << 16, 65536L, 0, BONUSSCREEN + 2 + gfx_offset, 0, 0, 2 + 8 + 16 + 64 + 128, 0, 0, xdim - 1, ydim - 1);
                        break;
                    }
                }

                menutext(160, 20 - 6, 0, 0, lastmapname);
                menutext(160, 36 - 6, 0, 0, "COMPLETED");

                gametext(160, 192, "PRESS ANY KEY TO CONTINUE", 16, 2 + 8 + 16);

                if (totalclock > (60 * 3))
                {
                    yy = zz = 59;

                    gametext(10, yy + 9, "Your Time:", 0, 2 + 8 + 16);

                    yy += 10;
                    if (!(ud.volume_number == 0 && ud.last_level - 1 == 7 && boardfilename[0]))
                    {
                        if (g_mapInfo[G_LastMapInfoIndex()].partime)
                        {
                            gametext(10, yy + 9, "Par Time:", 0, 2 + 8 + 16);
                            yy += 10;
                        }
                        if (!NAM_WW2GI && !DUKEBETA && g_mapInfo[G_LastMapInfoIndex()].designertime)
                        {
                            gametext(10, yy + 9, "3D Realms' Time:", 0, 2 + 8 + 16);
                            yy += 10;
                        }

                    }
                    if (ud.playerbest > 0)
                    {
                        gametext(10, yy + 9, g_player[myconnectindex].ps->player_par < ud.playerbest ? "Prev Best Time:" : "Your Best Time:", 0, 2 + 8 + 16);
                        yy += 10;
                    }

                    if (bonuscnt == 0)
                        bonuscnt++;

                    yy = zz;
                    if (totalclock > (60 * 4))
                    {
                        if (bonuscnt == 1)
                        {
                            bonuscnt++;
                            S_PlaySound(PIPEBOMB_EXPLODE);
                        }

                        if (g_player[myconnectindex].ps->player_par > 0)
                        {
                            G_PrintYourTime();
                            gametext((320 >> 2) + 71, yy + 9, tempbuf, 0, 2 + 8 + 16);
                            if (g_player[myconnectindex].ps->player_par < ud.playerbest)
                                gametext((320 >> 2) + 89 + (clockpad * 24), yy + 9, "New record!", 0, 2 + 8 + 16);
                        }
                        else
                            gametextpal((320 >> 2) + 71, yy + 9, "Cheated!", 0, 2);

                        yy += 10;

                        if (!(ud.volume_number == 0 && ud.last_level - 1 == 7 && boardfilename[0]))
                        {
                            if (g_mapInfo[G_LastMapInfoIndex()].partime)
                            {
                                G_PrintParTime();
                                gametext((320 >> 2) + 71, yy + 9, tempbuf, 0, 2 + 8 + 16);
                                yy += 10;
                            }
                            if (!NAM_WW2GI && !DUKEBETA && g_mapInfo[G_LastMapInfoIndex()].designertime)
                            {
                                G_PrintDesignerTime();
                                gametext((320 >> 2) + 71, yy + 9, tempbuf, 0, 2 + 8 + 16);
                                yy += 10;
                            }
                        }

                        if (ud.playerbest > 0)
                        {
                            G_PrintBestTime();
                            gametext((320 >> 2) + 71, yy + 9, tempbuf, 0, 2 + 8 + 16);
                            yy += 10;
                        }
                    }
                }

                zz = yy += 5;
                if (totalclock > (60 * 6))
                {
                    gametext(10, yy + 9, "Enemies Killed:", 0, 2 + 8 + 16);
                    yy += 10;
                    gametext(10, yy + 9, "Enemies Left:", 0, 2 + 8 + 16);
                    yy += 10;

                    if (bonuscnt == 2)
                    {
                        bonuscnt++;
                        S_PlaySound(FLY_BY);
                    }

                    yy = zz;

                    if (totalclock > (60 * 7))
                    {
                        if (bonuscnt == 3)
                        {
                            bonuscnt++;
                            S_PlaySound(PIPEBOMB_EXPLODE);
                        }
                        sprintf(tempbuf, "%-3d", ud.monsters_killed);
                        gametext((320 >> 2) + 70, yy + 9, tempbuf, 0, 2 + 8 + 16);
                        yy += 10;
                        if (ud.player_skill > 3)
                        {
                            sprintf(tempbuf, "N/A");
                            gametext((320 >> 2) + 70, yy + 9, tempbuf, 0, 2 + 8 + 16);
                            yy += 10;
                        }
                        else
                        {
                            if ((ud.total_monsters - ud.monsters_killed) < 0)
                                sprintf(tempbuf, "%-3d", 0);
                            else sprintf(tempbuf, "%-3d", ud.total_monsters - ud.monsters_killed);
                            gametext((320 >> 2) + 70, yy + 9, tempbuf, 0, 2 + 8 + 16);
                            yy += 10;
                        }
                    }
                }

                zz = yy += 5;
                if (totalclock > (60 * 9))
                {
                    gametext(10, yy + 9, "Secrets Found:", 0, 2 + 8 + 16);
                    yy += 10;
                    gametext(10, yy + 9, "Secrets Missed:", 0, 2 + 8 + 16);
                    yy += 10;
                    if (bonuscnt == 4) bonuscnt++;

                    yy = zz;
                    if (totalclock > (60 * 10))
                    {
                        if (bonuscnt == 5)
                        {
                            bonuscnt++;
                            S_PlaySound(PIPEBOMB_EXPLODE);
                        }
                        sprintf(tempbuf, "%-3d", ud.secret_rooms);
                        gametext((320 >> 2) + 70, yy + 9, tempbuf, 0, 2 + 8 + 16);
                        yy += 10;
                        if (ud.secret_rooms > 0)
                            sprintf(tempbuf, "%-3d%%", (100 * ud.secret_rooms / ud.max_secret_rooms));
                        sprintf(tempbuf, "%-3d", ud.max_secret_rooms - ud.secret_rooms);
                        gametext((320 >> 2) + 70, yy + 9, tempbuf, 0, 2 + 8 + 16);
                        yy += 10;
                    }
                }

                if (totalclock > 10240 && totalclock < 10240 + 10240)
                    totalclock = 1024;

                if ((((MOUSE_GetButtons() & 7) || KB_KeyWaiting()) && totalclock > (60 * 2)) ||
                    (numplayers > 1 && (totalclock > (TICRATE * 10)))) // Limit to 10 seconds online.
                {
                    MOUSE_ClearButton(7);
                    if (totalclock < (60 * 13))
                    {
                        KB_FlushKeyboardQueue();
                        totalclock = (60 * 13);
                    }
                    else if (totalclock < (1000000000L))
                    {
                        totalclock = (1000000000L); // Set timer to 1,000,000,000 tics, which will result in a break earlier in code.
                    }
                }
            }
            else
            {
                break;
            }

            X_OnEvent(EVENT_DISPLAYBONUSSCREEN, g_player[screenpeek].ps->i, screenpeek, -1);
            videoNextPage();
        }
    } while (1);
}