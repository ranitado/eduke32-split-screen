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

#include "compat.h"
#include "osdcmds.h"
#include "osd.h"
#include "baselayer.h"
#include "duke3d.h"
#include "crc32.h"
#include "osdfuncs.h"
#include <ctype.h>
#include <limits.h>

extern int g_doQuickSave;
struct osdcmd_cheatsinfo osdcmd_cheatsinfo_stat;
float r_ambientlight = 1.0, r_ambientlightrecip = 1.0;
int32_t r_pr_defaultlights = 1;
extern int hud_glowingquotes;
extern int r_maxfps;

static inline int osdcmd_quit(const osdfuncparm_t *parm)
{
    UNREFERENCED_PARAMETER(parm);
    Net_SendQuit();
    return OSDCMD_OK;
}

static int osdcmd_echo(const osdfuncparm_t *parm)
{
    int i;
    for (i = 0; i < parm->numparms; i++)
    {
        if (i > 0) OSD_Printf(" ");
        OSD_Printf("%s", parm->parms[i]);
    }
    OSD_Printf("\n");

    return OSDCMD_OK;
}

static int osdcmd_changelevel(const osdfuncparm_t *parm)
{
    int volume=0,level;
    char *p;

    if (!VOLUMEONE)
    {
        if (parm->numparms != 2) return OSDCMD_SHOWHELP;

        volume = strtol(parm->parms[0], &p, 10) - 1;
        if (p[0]) return OSDCMD_SHOWHELP;
        level = strtol(parm->parms[1], &p, 10) - 1;
        if (p[0]) return OSDCMD_SHOWHELP;
    }
    else
    {
        if (parm->numparms != 1) return OSDCMD_SHOWHELP;

        level = strtol(parm->parms[0], &p, 10) - 1;
        if (p[0]) return OSDCMD_SHOWHELP;
    }

    if (volume < 0) return OSDCMD_SHOWHELP;
    if (level < 0) return OSDCMD_SHOWHELP;

    if (!VOLUMEONE)
    {
        if (volume > g_numVolumes)
        {
            OSD_Printf("changelevel: invalid volume number (range 1-%d)\n",g_numVolumes);
            return OSDCMD_OK;
        }
    }

    if (level > MAXLEVELS || g_mapInfo[volume*MAXLEVELS+level].filename == NULL)
    {
        OSD_Printf("changelevel: invalid level number\n");
        return OSDCMD_SHOWHELP;
    }

    ud.m_volume_number = volume;
    ud.m_level_number = level;

    if (numplayers > 1)
    {
        if (myconnectindex == connecthead)
        {
            G_NewGame(NEWGAME_RESETALL);
        }
        else if (vote.starter == -1)
        {
            if ((Gametypes[ud.m_coop].flags & GAMETYPE_PLAYERSFRIENDLY) && !(Gametypes[ud.m_coop].flags & GAMETYPE_TDM))
                M_DMFLAGS_SET(DMFLAG_NOEXITS, false);

            Net_InitiateVote();

            ud.gm |= MODE_MENU;
            ChangeToMenu(603);
        }
        return OSDCMD_OK;
    }

    if (ud.gm & MODE_GAME)
    {
        // in-game behave like a cheat
        osdcmd_cheatsinfo_stat.cheatnum = 2;
        osdcmd_cheatsinfo_stat.volume   = volume;
        osdcmd_cheatsinfo_stat.level    = level;
    }
    else
    {
        // out-of-game behave like a menu command
        osdcmd_cheatsinfo_stat.cheatnum = -1;

        M_DMFLAGS_SET(DMFLAG_NOMONSTERS | DMFLAG_RESPAWNINVENTORY | DMFLAG_RESPAWNITEMS | DMFLAG_RESPAWNMONSTERS, false);

        ud.m_coop = 1;
        ud.multimode = 1;

        G_NewGame(NEWGAME_RESETALL);
    }

    return OSDCMD_OK;
}

static BUILDVFS_FIND_REC *findfiles = NULL;
static int numfiles = 0;

static void clearfilenames(void)
{
    klistfree(findfiles);
    findfiles = NULL;
    numfiles = 0;
}

static int getfilenames(const char *path)
{
    BUILDVFS_FIND_REC *r;

    clearfilenames();
    findfiles = klistpath(path,"*.MAP",BUILDVFS_FIND_FILE);
    for (r = findfiles; r; r=r->next) numfiles++;
    return(0);
}

static int osdcmd_map(const osdfuncparm_t *parm)
{
    int i;
    BUILDVFS_FIND_REC *r;
    char filename[256];

    if (parm->numparms != 1)
    {
        int maxwidth = 0;

        getfilenames("/");

        for (r=findfiles; r!=NULL; r=r->next)
            maxwidth = max((size_t)maxwidth,Bstrlen(r->name));

        if (maxwidth > 0)
        {
            int x = 0, count = 0;
            maxwidth += 3;
            OSD_Printf(OSDTEXT_RED "Map listing:\n");
            for (r=findfiles; r!=NULL; r=r->next)
            {
                OSD_Printf("%-*s",maxwidth,r->name);
                x += maxwidth;
                count++;
                if (x > OSD_GetCols() - maxwidth)
                {
                    x = 0;
                    OSD_Printf("\n");
                }
            }
            if (x) OSD_Printf("\n");
            OSD_Printf(OSDTEXT_RED "Found %d maps\n",numfiles);
        }

        return OSDCMD_SHOWHELP;
    }

#if 0
    if (numplayers > 1)
    {
        OSD_Printf("Command not allowed in multiplayer\n");
        return OSDCMD_OK;
    }
#endif

    strcpy(filename,parm->parms[0]);
    if (strchr(filename,'.') == 0)
        strcat(filename,".map");

    if ((i = kopen4loadfrommod(filename,0)) < 0)
    {
        OSD_Printf(OSD_ERROR "map: file \"%s\" not found.\n", filename);
        return OSDCMD_OK;
    }
    kclose(i);

    boardfilename[0] = '/';
    boardfilename[1] = 0;
    strcat(boardfilename, filename);

    ud.m_volume_number = 0;
    ud.m_level_number = 7;

    if (numplayers > 1)
    {
        if (myconnectindex == connecthead)
        {
            Net_SendUserMapName();   
            G_NewGame(NEWGAME_RESETALL);
        }
        else if (vote.starter == -1)
        {
            Net_SendUserMapName();

            if ((Gametypes[ud.m_coop].flags & GAMETYPE_PLAYERSFRIENDLY) && !(Gametypes[ud.m_coop].flags & GAMETYPE_TDM))
                M_DMFLAGS_SET(DMFLAG_NOEXITS, false);

            Net_InitiateVote();

            ud.gm |= MODE_MENU;
            ChangeToMenu(603);
        }

        return OSDCMD_OK;
    }

    osdcmd_cheatsinfo_stat.cheatnum = -1;
    ud.m_coop = 1;
    ud.multimode = 1;

    M_DMFLAGS_SET(DMFLAG_NOMONSTERS | DMFLAG_RESPAWNINVENTORY | DMFLAG_RESPAWNITEMS | DMFLAG_RESPAWNMONSTERS, false);  

    G_NewGame(NEWGAME_RESETALL);

    return OSDCMD_OK;
}

static int osdcmd_god(const osdfuncparm_t *parm)
{
    UNREFERENCED_PARAMETER(parm);
    if (numplayers == 1 && ud.gm & MODE_GAME)
    {
        osdcmd_cheatsinfo_stat.cheatnum = CHEAT_CORNHOLIO;
    }
    else
    {
        OSD_Printf("god: Not in a single-player game.\n");
    }

    return OSDCMD_OK;
}

static int osdcmd_noclip(const osdfuncparm_t *parm)
{
    UNREFERENCED_PARAMETER(parm);
    if (numplayers == 1 && ud.gm & MODE_GAME)
    {
        osdcmd_cheatsinfo_stat.cheatnum = CHEAT_CLIP;
    }
    else
    {
        OSD_Printf("noclip: Not in a single-player game.\n");
    }

    return OSDCMD_OK;
}

static int osdcmd_fileinfo(const osdfuncparm_t *parm)
{
    unsigned int length;
    unsigned int crc = 0;
    int i,j;
    char buf[256];

    if (parm->numparms != 1) return OSDCMD_SHOWHELP;

    if ((i = kopen4loadfrommod(parm->parms[0],0)) < 0)
    {
        OSD_Printf("fileinfo: File \"%s\" not found.\n", parm->parms[0]);
        return OSDCMD_OK;
    }

    length = kfilelength(i);

    do
    {
        j = kread(i,buf,256);
        crc = Bcrc32((unsigned char*)buf, j, crc);
    }
    while (j == 256);

    kclose(i);

    OSD_Printf("fileinfo: %s\n"
               "  File size: %d\n"
               "  CRC-32:    %08X\n",
               parm->parms[0], length, crc);

    return OSDCMD_OK;
}

#if 1
static int osdcmd_packetrate(const osdfuncparm_t *parm)
{
    int i;
    extern int packetrate; // mmulti.cpp

    if (parm->numparms == 0)
    {
        OSD_Printf("\"rate\" is \"%d\"\n", packetrate);
        return OSDCMD_SHOWHELP;
    }
    else if (parm->numparms != 1) return OSDCMD_SHOWHELP;

    i = Batol(parm->parms[0]);

    if (i >= 40 && i <= 1000)
    {
        packetrate = i;
        OSD_Printf("rate %d\n", packetrate);
    }
    else OSD_Printf("rate: value out of range\n");

    return OSDCMD_OK;
}
#endif

static int osdcmd_fraglimit(const osdfuncparm_t *parm)
{
    int i;

    if (parm->numparms == 0)
    {
        OSD_Printf("\"fraglimit\" is \"%d\"\n", ud.fraglimit);
        return OSDCMD_SHOWHELP;
    }
    else if (parm->numparms != 1) return OSDCMD_SHOWHELP;

    if (myconnectindex != connecthead)
    {
        OSD_Printf("fraglimit: can only be set by the host!\n");
        return OSDCMD_OK;
    }

    i = Batol(parm->parms[0]);

    if (i != ud.fraglimit)
    {
        ud.fraglimit = i;
        OSD_Printf("fraglimit set to %d\n", ud.fraglimit);

        Net_SendFragLimit();
    }
    else OSD_Printf("fraglimit: must be different than current value!\n");
    UNREFERENCED_PARAMETER(parm);
    return OSDCMD_OK;
}

static int osdcmd_debugplayer(const osdfuncparm_t *parm)
{
    int i;

    if (parm->numparms == 0)
    {
        OSD_Printf("You must specify a player number!\n");
        return OSDCMD_SHOWHELP;
    }
    else if (parm->numparms != 1) return OSDCMD_SHOWHELP;

    i = Batol(parm->parms[0]);

    if (i >= MAXPLAYERS)
        return OSDCMD_OK;

    if (g_player[i].connected == 1 || g_player[i].ps != NULL)
    {
        OSD_Printf("Name: %s\n", g_player[i].user_name);
        OSD_Printf("i: %d\n", g_player[i].ps->i);
        OSD_Printf("timebeforeexit: %d\n", g_player[i].ps->timebeforeexit);
        OSD_Printf("dead_flag: %d\n", g_player[i].ps->dead_flag);
        OSD_Printf("weapreccnt: %d\n", g_player[i].ps->weapreccnt);
        OSD_Printf("cursectnum/sprsect: %d/%d\n", g_player[i].ps->cursectnum, TrackerCast(sprite[g_player[i].ps->i].sectnum));
        OSD_Printf("playerswhenstarted: %d", playerswhenstarted);
    }
    else OSD_Printf("debugplayer: invalid player!\n");
    UNREFERENCED_PARAMETER(parm);
    return OSDCMD_OK;
}

static int osdcmd_restartsound(const osdfuncparm_t *parm)
{
    UNREFERENCED_PARAMETER(parm);

    FX_StopAllSounds();

    S_SoundShutdown();
    S_MusicShutdown();

    LOG_F(INFO, "Initializing music...\n");
    S_MusicStartup();
    LOG_F(INFO, "Initializing sound...\n");
    S_SoundStartup();

    if (ud.config.MusicToggle)
        S_RestartMusic();

    return OSDCMD_OK;
}

int osdcmd_restartvid(const osdfuncparm_t *parm)
{
    UNREFERENCED_PARAMETER(parm);
    videoResetMode();
    if (videoSetGameMode(ud.config.ScreenMode,ud.config.ScreenWidth,ud.config.ScreenHeight,ud.config.ScreenBPP, ud.detail))
        G_GameExit("restartvid: Reset failed...\n");
    onvideomodechange(ud.config.ScreenBPP>8);
    G_UpdateScreenArea();

    return OSDCMD_OK;
}

static int osdcmd_vidmode(const osdfuncparm_t *parm)
{
    int newbpp = ud.config.ScreenBPP, newwidth = ud.config.ScreenWidth,
                 newheight = ud.config.ScreenHeight, newfs = ud.config.ScreenMode;
    if (parm->numparms < 1 || parm->numparms > 4) return OSDCMD_SHOWHELP;

    switch (parm->numparms)
    {
    case 1: // bpp switch
        newbpp = Batol(parm->parms[0]);
        break;
    case 2: // res switch
        newwidth = Batol(parm->parms[0]);
        newheight = Batol(parm->parms[1]);
        break;
    case 3: // res & bpp switch
    case 4:
        newwidth = Batol(parm->parms[0]);
        newheight = Batol(parm->parms[1]);
        newbpp = Batol(parm->parms[2]);
        if (parm->numparms == 4)
            newfs = (Batol(parm->parms[3]) != 0);
        break;
    }

    if (videoSetGameMode(newfs,newwidth,newheight,newbpp,ud.detail))
    {
        LOG_F(ERROR, "vidmode: Mode change failed!\n");
        if (videoSetGameMode(ud.config.ScreenMode, ud.config.ScreenWidth, ud.config.ScreenHeight, ud.config.ScreenBPP, ud.detail))
            G_GameExit("vidmode: Reset failed!\n");
    }
    ud.config.ScreenBPP = newbpp;
    ud.config.ScreenWidth = newwidth;
    ud.config.ScreenHeight = newheight;
    ud.config.ScreenMode = newfs;
    onvideomodechange(ud.config.ScreenBPP>8);
    G_UpdateScreenArea();
    return OSDCMD_OK;
}

static int osdcmd_spawn(const osdfuncparm_t *parm)
{
    auto pPlayer = g_player[myconnectindex].ps;
    vec3_t pos;
    int32_t cstat = 0, picnum = 0;
    int8_t pal = 0;
    int16_t ang = 0;
    int16_t set = 0;

    if (numplayers > 1 || !(ud.gm & MODE_GAME))
    {
        OSD_Printf("spawn: Can't spawn sprites in multiplayer games or demos\n");
        return OSDCMD_OK;
    }

    switch (parm->numparms)
    {
    case 7: // x,y,z
        pos.x = Batol(parm->parms[4]);
        pos.y = Batol(parm->parms[5]);
        pos.z = Batol(parm->parms[6]);
        set |= 8;
        fallthrough__;
    case 4: // ang
        ang = Batol(parm->parms[3]) & 2047;
        set |= 4;
        fallthrough__;
    case 3: // cstat
        cstat = (unsigned short)Batol(parm->parms[2]);
        set |= 2;
        fallthrough__;
    case 2: // pal
        pal = (unsigned char)Batol(parm->parms[1]);
        set |= 1;
        fallthrough__;
    case 1: // tile number
        if (isdigit(parm->parms[0][0]))
        {
            picnum = (unsigned short)Batol(parm->parms[0]);
        }
        else
        {
            int i, j;
            for (j = 0; j < 2; j++)
            {
                for (i = 0; i < g_labelCnt; i++)
                {
                    if (
                        (j == 0 && !Bstrcmp(label + (i << 6), parm->parms[0])) ||
                        (j == 1 && !Bstrcasecmp(label + (i << 6), parm->parms[0]))
                        )
                    {
                        picnum = (unsigned short)labelcode[i];
                        break;
                    }
                }
                if (i < g_labelCnt) break;
            }
            if (i == g_labelCnt)
            {
                OSD_Printf("spawn: Invalid tile label given\n");
                return OSDCMD_OK;
            }
        }

        if (picnum >= MAXTILES)
        {
            OSD_Printf("spawn: Invalid tile number\n");
            return OSDCMD_OK;
        }
        break;
    default:
        return OSDCMD_SHOWHELP;
    }

    int32_t idx = A_Spawn(pPlayer->i, picnum);
    if (set & 1) sprite[idx].pal = pal;
    if (set & 2) sprite[idx].cstat = cstat;
    if (set & 4) sprite[idx].ang = ang;
    if (set & 8)
    {
        if (setspritez(idx, &pos) < 0)
        {
            OSD_Printf("spawn: Sprite can't be spawned into null space\n");
            A_DeleteSprite(idx);
        }
    }

    return OSDCMD_OK;
}

static int osdcmd_setvar(const osdfuncparm_t *parm)
{
    int i, varval;
    char varname[256];

    if (parm->numparms != 2) return OSDCMD_SHOWHELP;

    if (numplayers > 1)
    {
        OSD_Printf("Command not allowed in multiplayer\n");
        return OSDCMD_OK;
    }

    strcpy(varname,parm->parms[1]);
    varval = Batol(varname);
    i = hash_find(&h_gamevars,varname);
    if (i >= 0)
        varval=Gv_GetVar(i, g_player[myconnectindex].ps->i, myconnectindex);

    strcpy(varname,parm->parms[0]);
    i = hash_find(&h_gamevars,varname);
    if (i >= 0)
        Gv_SetVar(i, varval, g_player[myconnectindex].ps->i, myconnectindex);
    return OSDCMD_OK;
}

static int osdcmd_addlogvar(const osdfuncparm_t *parm)
{
    int i;
    char varname[256];

    if (parm->numparms != 1) return OSDCMD_SHOWHELP;

    if (numplayers > 1)
    {
        OSD_Printf("Command not allowed in multiplayer\n");
        return OSDCMD_OK;
    }

    strcpy(varname,parm->parms[0]);
    i = hash_find(&h_gamevars,varname);
    if (i >= 0)
        OSD_Printf("%s = %d\n", varname, Gv_GetVar(i, g_player[screenpeek].ps->i, screenpeek));
    return OSDCMD_OK;
}

static int osdcmd_setactorvar(const osdfuncparm_t *parm)
{
    int i, varval, ID;
    char varname[256];

    if (parm->numparms != 3) return OSDCMD_SHOWHELP;

    if (numplayers > 1)
    {
        OSD_Printf("Command not allowed in multiplayer\n");
        return OSDCMD_OK;
    }

    ID=Batol(parm->parms[0]);
    if (ID>=MAXSPRITES)
    {
        OSD_Printf("Invalid sprite ID\n");
        return OSDCMD_OK;
    }

    varval = Batol(parm->parms[2]);
    strcpy(varname,parm->parms[2]);
    varval = Batol(varname);
    i = hash_find(&h_gamevars,varname);
    if (i >= 0)
        varval=Gv_GetVar(i, g_player[myconnectindex].ps->i, myconnectindex);

    strcpy(varname,parm->parms[1]);
    i = hash_find(&h_gamevars,varname);
    if (i >= 0)
        Gv_SetVar(i, varval, ID, -1);
    return OSDCMD_OK;
}

static int osdcmd_addpath(const osdfuncparm_t *parm)
{
    char pathname[BMAX_PATH];

    if (parm->numparms != 1) return OSDCMD_SHOWHELP;

    strcpy(pathname,parm->parms[0]);
    addsearchpath(pathname);
    return OSDCMD_OK;
}

static int osdcmd_initgroupfile(const osdfuncparm_t *parm)
{
    char file[BMAX_PATH];

    if (parm->numparms != 1) return OSDCMD_SHOWHELP;

    strcpy(file,parm->parms[0]);
    initgroupfile(file);
    return OSDCMD_OK;
}

static int osdcmd_cmenu(const osdfuncparm_t *parm)
{
    if (parm->numparms != 1) return OSDCMD_SHOWHELP;
    if (numplayers > 1)
    {
        OSD_Printf("cmenu: disallowed in multiplayer\n");
        return OSDCMD_OK;
    }
    else
    {
        ChangeToMenu(Batol(parm->parms[0]));
    }

    return OSDCMD_OK;
}

#define CVAR_BOOL_OPTSTR ":\n 0: disabled\n 1: enabled"
osdcvardata_t cvar[] =
{
    { "crosshair", "crosshair: enable/disable crosshair", (void*)&ud.crosshair, CVAR_BOOL, 0, 1 },

    { "fov", "change the field of view", (void*)&ud.fov, CVAR_INT, 60, 120 },

    { "hud_althud", "hud_althud: enable/disable alternate mini-hud", (void*)&ud.althud, CVAR_BOOL, 0, 1 },
    { "hud_messagetime", "hud_messagetime: length of time to display multiplayer chat messages", (void*)&ud.msgdisptime, CVAR_INT, 0, 3600 },
    { "hud_numbertile", "hud_numbertile: first tile in alt hud number set", (void*)&althud_numbertile, CVAR_INT, 0, MAXTILES-10 },
    { "hud_numberpal", "hud_numberpal: pal for alt hud numbers", (void*)&althud_numberpal, CVAR_INT, 0, MAXPALOOKUPS },
    { "hud_shadows", "hud_shadows: enable/disable althud shadows", (void*)&althud_shadows, CVAR_BOOL, 0, 1 },
    { "hud_flashing", "hud_flashing: enable/disable althud flashing", (void*)&althud_flashing, CVAR_BOOL, 0, 1 },
    { "hud_glowingquotes", "hud_glowingquotes: enable/disable \"glowing\" quote text", (void*)&hud_glowingquotes, CVAR_BOOL, 0, 1 },
    { "hud_scale","changes the hud scale", (void*)&ud.statusbarscale, CVAR_INT | CVAR_FUNCPTR, 50, 100 },
    { "hud_showmapname", "hud_showmapname: enable/disable map name display on load", (void*)&hud_showmapname, CVAR_BOOL, 0, 1 },
    { "hud_stats", "hud_stats: enable/disable level statistics display", (void*)&ud.levelstats, CVAR_BOOL, 0, 1 },
    { "hud_statusbarmode", "status bar draws over player view" CVAR_BOOL_OPTSTR, (void*)&ud.statusbarmode, CVAR_BOOL | CVAR_FUNCPTR, 0, 1 },
    { "hud_textscale", "hud_textscale: sets multiplayer chat message size", (void*)&ud.textscale, CVAR_INT, 100, 400 },
    { "hud_weaponscale","weapon scale", (void*)&ud.weaponscale, CVAR_INT, 10, 100 },
    { "hud_fragbarstyle", "Sets frag bar style:\n 0: None\n 1: Classic\n 2: StrikerDM (Compact)", (void*)&hud_fragbarstyle, CVAR_INT, 0, 2 },

    { "cl_autoaim", "cl_autoaim: enable/disable weapon autoaim", (void*)&ud.config.AutoAim, CVAR_INT|CVAR_MULTI, 0, 2 },
    { "cl_autorun", "player autorun" CVAR_BOOL_OPTSTR, (void*)&ud.auto_run, CVAR_BOOL, 0, 1 },
    { "cl_runmode", "run key behavior with cl_autorun enabled:\n 0: walk\n 1: do nothing", (void*)&ud.runkey_mode, CVAR_BOOL, 0, 1 },

    { "cl_automsg", "cl_automsg: enable/disable automatically sending messages to all players", (void*)&ud.automsg, CVAR_BOOL, 0, 1 },
    { "cl_autovote", "cl_autovote: enable/disable automatic voting", (void*)&ud.autovote, CVAR_INT|CVAR_MULTI, 0, 2 },

    { "cl_obituaries", "cl_obituaries: enable/disable multiplayer death messages", (void*)&ud.obituaries, CVAR_BOOL, 0, 1 },
    { "cl_democams", "cl_democams: enable/disable demo playback cameras", (void*)&ud.democams, CVAR_BOOL, 0, 1 },

    { "cl_idplayers", "cl_idplayers: enable/disable name display when aiming at opponents", (void*)&ud.idplayers, CVAR_BOOL, 0, 1 },

    { "cl_showcoords", "cl_showcoords: show your position in the game world", (void*)&ud.coords, CVAR_BOOL, 0, 1 },
    { "cl_showweapons", "cl_showweapons: show weapons opponents are using", (void*)&ud.showweapons, CVAR_BOOL, 0, 1 },

    { "cl_viewbob", "cl_viewbob: enable/disable player head bobbing", (void*)&ud.viewbob, CVAR_BOOL, 0, 1 },

    { "cl_weaponsway", "cl_weaponsway: enable/disable player weapon swaying", (void*)&ud.weaponsway, CVAR_BOOL, 0, 1 },
    { "cl_weaponswitch", "cl_weaponswitch: enable/disable auto weapon switching", (void*)&ud.weaponswitch, CVAR_INT|CVAR_MULTI, 0, 7 },
    { "cl_angleinterpolation", "cl_angleinterpolation: enable/disable angle interpolation", (void*)&ud.angleinterpolation, CVAR_INT, 0, 256 },

    { "crosshairscale","crosshair size", (void*)&ud.crosshairscale, CVAR_INT, 10, 100 },

    // Player Colors
    { "team", "team: player's team number", (void*)&ud.team, CVAR_INT | CVAR_MULTI, 0, 3 },
    { "color", "player palette", (void*)&ud.m_color, CVAR_INT | CVAR_MULTI, 0, MAXPALOOKUPS - 1 },

    // Mouse
    { "in_aimmode", "hold button for mouse aim" CVAR_BOOL_OPTSTR, (void*)&ud.mouseaiming, CVAR_BOOL, 0, 1 },
    { "in_mouse", "use mouse input" CVAR_BOOL_OPTSTR,(void*)&ud.config.UseMouse, CVAR_BOOL | CVAR_FUNCPTR, 0, 1 },
    { "in_mousemode", "vertical mouse aiming" CVAR_BOOL_OPTSTR, (void*)&g_myAimMode, CVAR_BOOL, 0, 1 },
    { "in_mouseflip", "invert vertical mouse movement" CVAR_BOOL_OPTSTR, (void*)&ud.mouseflip, CVAR_BOOL, 0, 1 },
    { "in_mousebias", "in_mousebias: emulates the original mouse code's weighting of input towards whichever axis is moving the most at any given time", (void*)&ud.config.MouseBias, CVAR_INT, 0, 32 },

    // Joystick
    { "in_joystick","use joystick or controller input" CVAR_BOOL_OPTSTR,(void*)&ud.config.UseJoystick, CVAR_BOOL | CVAR_FUNCPTR, 0, 1 },

#if defined(USE_OPENGL)
    //{ "r_anamorphic", "r_anamorphic: enable/disable widescreen mode", (void*)&glwidescreen, CVAR_BOOL, 0, 1 },
    { "r_projectionhack", "r_projectionhack: enable/disable projection hack", (void*)&glprojectionhacks, CVAR_INT, 0, 2 },
#endif
    { "r_ambientlight", "sets the global map light level",(void*)&r_ambientlight, CVAR_FLOAT | CVAR_FUNCPTR, 0, 10 },

    { "r_pr_defaultlights", "default polymer lights:\n 0: off\n 1: on",(void*)&r_pr_defaultlights, CVAR_BOOL, 0, 1 },

    { "r_drawweapon", "r_drawweapon: enable/disable weapon drawing", (void*)&ud.drawweapon, CVAR_INT, 0, 2 },
    { "r_upscalefactor", "increase performance by rendering at upscalefactor less than the screen resolution and upscale to the full resolution in the software renderer", (void*)&ud.detail, CVAR_INT | CVAR_FUNCPTR, 1, 16 },
    { "r_size", "change size of viewable area", (void*)&ud.screen_size, CVAR_INT | CVAR_FUNCPTR, 0, 64 },

    { "osdhightile", "osdhightile: enable/disable hires art replacements for console text", (void*)&osdhightile, CVAR_BOOL, 0, 1 },
    { "osdscale", "console text size", (void*)&osdscale, CVAR_FLOAT | CVAR_FUNCPTR, 1, 4 },

    { "r_showfps", "r_showfps: show the frame rate counter", (void*)&ud.showfps, CVAR_INT, 0, 2 },
    { "r_shadows", "r_shadows: enable/disable sprite and model shadows", (void*)&ud.shadows, CVAR_BOOL, 0, 1 },
    { "r_precache", "r_precache: enable/disable the pre-level caching routine", (void*)&ud.config.useprecache, CVAR_BOOL, 0, 1 },

    // Music
    { "mus_enabled", "music subsystem" CVAR_BOOL_OPTSTR, (void*)&ud.config.MusicToggle, CVAR_BOOL | CVAR_FUNCPTR, 0, 1 },
    { "mus_device", "music device", (void*)&ud.config.MusicDevice, CVAR_INT | CVAR_FUNCPTR, 0, ASS_NumSoundCards },
    { "mus_volume", "controls music volume", (void*)&ud.config.MusicVolume, CVAR_INT | CVAR_FUNCPTR, 0, 255 },

    // Sound
    { "snd_ambience", "snd_ambience: enables/disables ambient sounds", (void*)&ud.config.AmbienceToggle, CVAR_BOOL, 0, 1 },
    { "snd_enabled", "sound effects" CVAR_BOOL_OPTSTR, (void*)&ud.config.SoundToggle, CVAR_BOOL | CVAR_FUNCPTR, 0, 1 },
    { "snd_duketalk", "snd_duketalk: enables/disables Duke's speech", (void*)&ud.config.VoiceToggle, CVAR_INT, 0, 5 },
    { "snd_fxvolume", "snd_fxvolume: volume of sound effects", (void*)&ud.config.FXVolume, CVAR_INT, 0, 255 },
    { "snd_mixrate", "snd_mixrate: sound mixing rate", (void*)&ud.config.MixRate, CVAR_INT, 0, 48000 },
    { "snd_numbits", "snd_numbits: sound bits", (void*)&ud.config.NumBits, CVAR_INT, 8, 16 },
    { "snd_numchannels", "snd_numchannels: the number of sound channels", (void*)&ud.config.NumChannels, CVAR_INT, 0, 2 },
    { "snd_numvoices", "snd_numvoices: the number of concurrent sounds", (void*)&ud.config.NumVoices, CVAR_INT | CVAR_FUNCPTR, 0, 32 },
    //{ "snd_reversestereo", "snd_reversestereo: reverses the stereo channels", (void*)&ud.config.ReverseStereo, CVAR_BOOL, 0, 16 },

    { "vid_gamma","gamma component of gamma ramp",(void*)&g_videoGamma, CVAR_FLOAT | CVAR_FUNCPTR, 0, 10 },
    { "vid_contrast","contrast component of gamma ramp",(void*)&g_videoContrast, CVAR_FLOAT | CVAR_FUNCPTR, 0, 10 },
    //{ "vid_brightness","brightness component of gamma ramp",(void*)&g_videoBrightness, CVAR_FLOAT | CVAR_FUNCPTR, 0, 10 },

    { "wchoice","weapon priority for automatically switching on empty or pickup", (void*)ud.wchoice, CVAR_STRING | CVAR_FUNCPTR, 0, MAX_WEAPONS },
};

extern int CommandWeaponChoice;
static int osdcmd_cvar_set_game(osdcmdptr_t parm)
{
    int const r = osdcmd_cvar_set(parm);

    if (r != OSDCMD_OK) return r;

    if (!Bstrcasecmp(parm->name, "r_upscalefactor"))
    {
        if (in3dmode())
        {
            videoSetGameMode(fullscreen, xres, yres, bpp, ud.detail);
        }
    }
    else if (!Bstrcasecmp(parm->name, "r_size"))
    {
        ud.statusbarmode = (ud.screen_size < 8);
        G_UpdateScreenArea();
    }
    else if (!Bstrcasecmp(parm->name, "r_ambientlight"))
    {
        if (r_ambientlight == 0)
            r_ambientlightrecip = 256.f;
        else r_ambientlightrecip = 1.f / r_ambientlight;
    }
    else if (!Bstrcasecmp(parm->name, "in_mouse"))
    {
        CONTROL_MouseEnabled = (ud.config.UseMouse && CONTROL_MousePresent);
    }
    else if (!Bstrcasecmp(parm->name, "in_joystick"))
    {
        CONTROL_JoystickEnabled = (ud.config.UseJoystick && CONTROL_JoyPresent);
    }
    else if (!Bstrcasecmp(parm->name, "in_aimmode")
        || !Bstrcasecmp(parm->name, "cl_weaponswitch")
        || !Bstrcasecmp(parm->name, "cl_autoaim")
        || !Bstrcasecmp(parm->name, "team"))
    {
        G_UpdatePlayerFromMenu();
    }
    else if (!Bstrcasecmp(parm->name, "mus_volume"))
    {
        S_MusicVolume(Batol(parm->parms[0]));
    }
    else if (!Bstrcasecmp(parm->name, "vid_gamma"))
    {
        ud.brightness = GAMMA_CALC;
        ud.brightness <<= 2;
        videoSetPalette(ud.brightness >> 2, g_player[myconnectindex].ps->palette, 0);
    }
    else if (!Bstrcasecmp(parm->name, "vid_brightness") || !Bstrcasecmp(parm->name, "vid_contrast"))
    {
        videoSetPalette(ud.brightness >> 2, g_player[myconnectindex].ps->palette, 0);
    }
    else if (!Bstrcasecmp(parm->name, "snd_enabled") || !Bstrcasecmp(parm->name, "snd_numvoices"))
    {
        if (!FX_WarmedUp())
            return r;

        S_SoundShutdown();
        S_SoundStartup();
    }
    else if (!Bstrcasecmp(parm->name, "hud_scale")
        || !Bstrcasecmp(parm->name, "hud_statusbarmode")
        || !Bstrcasecmp(parm->name, "r_rotatespritenowidescreen"))
    {
        G_UpdateScreenArea();
    }
    else if (!Bstrcasecmp(parm->name, "skill"))
    {
        if (numplayers > 1)
            return r;

        ud.player_skill = ud.m_player_skill;
    }
    else if (!Bstrcasecmp(parm->name, "color"))
    {
        ud.color = playerColor_getValidPal(ud.m_color);
        G_UpdatePlayerFromMenu();
    }
    else if (!Bstrcasecmp(parm->name, "osdscale"))
    {
        osdrscale = 1.f / osdscale;

        if (xdim && ydim)
            OSD_ResizeDisplay(xdim, ydim);
    }
    else if (!Bstrcasecmp(parm->name, "wchoice"))
    {
        if (parm->numparms == 1)
        {
            if (CommandWeaponChoice) // rewrite ud.wchoice because osdcmd_cvar_set already changed it
            {
                int j = 0;

                while (j < 10)
                {
                    ud.wchoice[j] = g_player[myconnectindex].wchoice[j] + '0';
                    j++;
                }

                ud.wchoice[j] = 0;
            }
            else
            {
                char const* c = parm->parms[0];

                if (*c)
                {
                    int j = 0;

                    // Only accept numbers from 0 to 9
                    while ((*c >= '0') && (*c <= '9') && j < min<int>(MAX_WEAPONS, 10))
                    {
                        g_player[myconnectindex].wchoice[j] = *c - '0';
                        c++;
                        j++;
                    }

                    // Apply weapon priority if a valid value is given
                    if (j > 0)
                    {
                        // Pad value when not all weapons are given
                        while (j < 10)
                        {
                            if (j == 9)
                                g_player[myconnectindex].wchoice[9] = 1;
                            else
                                g_player[myconnectindex].wchoice[j] = 2;

                            j++;
                        }
                        // Only send valid values to other players
                        Net_SendWeaponChoice();
                    }
                }
            }
            CommandWeaponChoice = 0;
        }
    }

    return r;
}

static int osdcmd_cvar_set_multi(osdcmdptr_t parm)
{
    int const r = osdcmd_cvar_set_game(parm);

    if (r != OSDCMD_OK) return r;

    G_UpdatePlayerFromMenu();

    return r;
}

static int osdcmd_give(const osdfuncparm_t *parm)
{
    int i;

    if (numplayers != 1 || (ud.gm & MODE_GAME) == 0 ||
            g_player[myconnectindex].ps->dead_flag != 0)
    {
        OSD_Printf("give: Cannot give while dead or not in a single-player game.\n");
        return OSDCMD_OK;
    }

    if (parm->numparms != 1) return OSDCMD_SHOWHELP;

    if (!Bstrcasecmp(parm->parms[0], "all"))
    {
        osdcmd_cheatsinfo_stat.cheatnum = 1;
        return OSDCMD_OK;
    }
    else if (!Bstrcasecmp(parm->parms[0], "health"))
    {
        sprite[g_player[myconnectindex].ps->i].extra = g_player[myconnectindex].ps->max_player_health<<1;
        return OSDCMD_OK;
    }
    else if (!Bstrcasecmp(parm->parms[0], "weapons"))
    {
        osdcmd_cheatsinfo_stat.cheatnum = 21;
        return OSDCMD_OK;
    }
    else if (!Bstrcasecmp(parm->parms[0], "ammo"))
    {
        for (i=MAX_WEAPONS-(VOLUMEONE?6:1)-1;i>=PISTOL_WEAPON;i--)
            P_AddAmmo(i,g_player[myconnectindex].ps,g_player[myconnectindex].ps->max_ammo_amount[i]);
        return OSDCMD_OK;
    }
    else if (!Bstrcasecmp(parm->parms[0], "armor"))
    {
        g_player[myconnectindex].ps->inv_amount[GET_SHIELD] = 100;
        return OSDCMD_OK;
    }
    else if (!Bstrcasecmp(parm->parms[0], "keys"))
    {
        osdcmd_cheatsinfo_stat.cheatnum = 23;
        return OSDCMD_OK;
    }
    else if (!Bstrcasecmp(parm->parms[0], "inventory"))
    {
        osdcmd_cheatsinfo_stat.cheatnum = 22;
        return OSDCMD_OK;
    }
    return OSDCMD_SHOWHELP;
}

void onvideomodechange(int newmode)
{
    uint8_t palid;

    if (newmode)
    {
        if (g_player[screenpeek].ps->palette < BASEPALCOUNT)
            palid = g_player[screenpeek].ps->palette;
        else
            palid = BASEPAL;
    }
    else
    {
        palid = g_player[screenpeek].ps->palette;
    }

#ifdef POLYMER
    if (videoGetRenderMode() == REND_POLYMER)
    {
        int32_t i = 0;

        while (i < MAXSPRITES)
        {
            if (practor[i].lightptr)
            {
                polymer_deletelight(practor[i].lightId);
                practor[i].lightptr = NULL;
                practor[i].lightId = -1;
            }
            i++;
        }
    }
#endif

    videoSetPalette(ud.brightness >> 2, palid, 0);
    g_restorePalette = 1;
    g_crosshairSum = 0;
}

static int osdcmd_name(const osdfuncparm_t *parm)
{
    char namebuf[32];

    if (parm->numparms != 1)
    {
        OSD_Printf("\"name\" is \"%s\"\n",szPlayerName);
        return OSDCMD_SHOWHELP;
    }

    Bstrcpy(tempbuf,parm->parms[0]);

    while (Bstrlen(OSD_StripColors(namebuf,tempbuf)) > 10)
        tempbuf[Bstrlen(tempbuf)-1] = '\0';

    Bstrncpy(szPlayerName,tempbuf,sizeof(szPlayerName)-1);
    szPlayerName[sizeof(szPlayerName)-1] = '\0';

    OSD_Printf("name %s\n",szPlayerName);

    Net_SendPlayerName();

    return OSDCMD_OK;
}

static int osdcmd_button(osdcmdptr_t parm)
{
    static char const s_gamefunc_[] = "gamefunc_";
    int constexpr strlen_gamefunc_ = ARRAY_SIZE(s_gamefunc_) - 1;

    char const* p = parm->name + strlen_gamefunc_;

    //    if (ud.gm == MODE_GAME) // only trigger these if in game
    CONTROL_ButtonFlags[CONFIG_FunctionNameToNum(p)] = 1; // FIXME

    return OSDCMD_OK;
}

keydef_t keynames[]=
{
    { "Escape", 0x1 },
    { "1", 0x2 },
    { "2", 0x3 },
    { "3", 0x4 },
    { "4", 0x5 },
    { "5", 0x6 },
    { "6", 0x7 },
    { "7", 0x8 },
    { "8", 0x9 },
    { "9", 0xa },
    { "0", 0xb },
    { "-", 0xc },
    { "=", 0xd },
    { "BakSpc", 0xe },
    { "Tab", 0xf },
    { "Q", 0x10 },
    { "W", 0x11 },
    { "E", 0x12 },
    { "R", 0x13 },
    { "T", 0x14 },
    { "Y", 0x15 },
    { "U", 0x16 },
    { "I", 0x17 },
    { "O", 0x18 },
    { "P", 0x19 },
    { "[", 0x1a },
    { "]", 0x1b },
    { "Enter", 0x1c },
    { "LCtrl", 0x1d },
    { "A", 0x1e },
    { "S", 0x1f },
    { "D", 0x20 },
    { "F", 0x21 },
    { "G", 0x22 },
    { "H", 0x23 },
    { "J", 0x24 },
    { "K", 0x25 },
    { "L", 0x26 },
    { "SemiColon", 0x27 },
    { "'", 0x28 },
    { "Tilde", 0x29 },
    { "LShift", 0x2a },
    { "Backslash", 0x2b },
    { "Z", 0x2c },
    { "X", 0x2d },
    { "C", 0x2e },
    { "V", 0x2f },
    { "B", 0x30 },
    { "N", 0x31 },
    { "M", 0x32 },
    { ",", 0x33 },
    { ".", 0x34 },
    { "/", 0x35 },
    { "RShift", 0x36 },
    { "Kpad*", 0x37 },
    { "LAlt", 0x38 },
    { "Space", 0x39 },
    { "CapLck", 0x3a },
    { "F1", 0x3b },
    { "F2", 0x3c },
    { "F3", 0x3d },
    { "F4", 0x3e },
    { "F5", 0x3f },
    { "F6", 0x40 },
    { "F7", 0x41 },
    { "F8", 0x42 },
    { "F9", 0x43 },
    { "F10", 0x44 },
    { "NumLck", 0x45 },
    { "ScrLck", 0x46 },
    { "Kpad7", 0x47 },
    { "Kpad8", 0x48 },
    { "Kpad9", 0x49 },
    { "Kpad-", 0x4a },
    { "Kpad4", 0x4b },
    { "Kpad5", 0x4c },
    { "Kpad6", 0x4d },
    { "Kpad+", 0x4e },
    { "Kpad1", 0x4f },
    { "Kpad2", 0x50 },
    { "Kpad3", 0x51 },
    { "Kpad0", 0x52 },
    { "Kpad.", 0x53 },
    { "F11", 0x57 },
    { "F12", 0x58 },
    { "KpdEnt", 0x9c },
    { "RCtrl", 0x9d },
    { "Kpad/", 0xb5 },
    { "RAlt", 0xb8 },
    { "PrtScn", 0xb7 },
    { "Pause", 0xc5 },
    { "Home", 0xc7 },
    { "Up", 0xc8 },
    { "PgUp", 0xc9 },
    { "Left", 0xcb },
    { "Right", 0xcd },
    { "End", 0xcf },
    { "Down", 0xd0 },
    { "PgDn", 0xd1 },
    { "Insert", 0xd2 },
    { "Delete", 0xd3 },

    {0,0}
};

const char* const ConsoleButtons[] =
{
    "mouse1", "mouse2", "mouse3", "mouse4", "mwheelup",
    "mwheeldn", "mouse5", "mouse6", "mouse7", "mouse8"
};

static int osdcmd_bind(osdcmdptr_t parm)
{
    if (parm->numparms == 1 && !Bstrcasecmp(parm->parms[0], "showkeys"))
    {
        for (auto& s : sctokeylut)
            OSD_Printf("%s\n", s.key);
        for (auto ConsoleButton : ConsoleButtons)
            OSD_Printf("%s\n", ConsoleButton);
        return OSDCMD_OK;
    }

    if (parm->numparms == 0)
    {
        int j = 0;

        OSD_Printf("Current key bindings:\n");

        for (int i = 0; i < MAXBOUNDKEYS + MAXMOUSEBUTTONS; i++)
            if (CONTROL_KeyIsBound(i))
            {
                j++;
                OSD_Printf("%-9s %s\"%s\"\n", CONTROL_KeyBinds[i].key, CONTROL_KeyBinds[i].repeat ? "" : "norepeat ",
                    CONTROL_KeyBinds[i].cmdstr);
            }

        if (j == 0)
            OSD_Printf("No binds found.\n");

        return OSDCMD_OK;
    }

    int i, j, repeat;

    for (i = 0; i < ARRAY_SSIZE(sctokeylut); i++)
    {
        if (!Bstrcasecmp(parm->parms[0], sctokeylut[i].key))
            break;
    }

    // didn't find the key
    if (i == ARRAY_SSIZE(sctokeylut))
    {
        for (i = 0; i < MAXMOUSEBUTTONS; i++)
            if (!Bstrcasecmp(parm->parms[0], ConsoleButtons[i]))
                break;

        if (i >= MAXMOUSEBUTTONS)
            return OSDCMD_SHOWHELP;

        if (parm->numparms < 2)
        {
            if (CONTROL_KeyBinds[MAXBOUNDKEYS + i].cmdstr && CONTROL_KeyBinds[MAXBOUNDKEYS + i].key)
                OSD_Printf("%-9s %s\"%s\"\n", ConsoleButtons[i], CONTROL_KeyBinds[MAXBOUNDKEYS + i].repeat ? "" : "norepeat ",
                    CONTROL_KeyBinds[MAXBOUNDKEYS + i].cmdstr);
            else OSD_Printf("%s is unbound\n", ConsoleButtons[i]);
            return OSDCMD_OK;
        }

        j = 1;

        repeat = 1;
        if (!Bstrcasecmp(parm->parms[j], "norepeat"))
        {
            repeat = 0;
            j++;
        }

        Bstrcpy(tempbuf, parm->parms[j++]);
        for (; j < parm->numparms; j++)
        {
            Bstrcat(tempbuf, " ");
            Bstrcat(tempbuf, parm->parms[j++]);
        }

        CONTROL_BindMouse(i, tempbuf, repeat, ConsoleButtons[i]);

        if (!OSD_ParsingScript())
            OSD_Printf("%s\n", parm->raw);
        return OSDCMD_OK;
    }

    if (parm->numparms < 2)
    {
        if (CONTROL_KeyIsBound(sctokeylut[i].sc))
            OSD_Printf("%-9s %s\"%s\"\n", sctokeylut[i].key, CONTROL_KeyBinds[sctokeylut[i].sc].repeat ? "" : "norepeat ",
                CONTROL_KeyBinds[sctokeylut[i].sc].cmdstr);
        else OSD_Printf("%s is unbound\n", sctokeylut[i].key);

        return OSDCMD_OK;
    }

    j = 1;

    repeat = 1;
    if (!Bstrcasecmp(parm->parms[j], "norepeat"))
    {
        repeat = 0;
        j++;
    }

    Bstrcpy(tempbuf, parm->parms[j++]);
    for (; j < parm->numparms; j++)
    {
        Bstrcat(tempbuf, " ");
        Bstrcat(tempbuf, parm->parms[j++]);
    }

    CONTROL_BindKey(sctokeylut[i].sc, tempbuf, repeat, sctokeylut[i].key);

    char* cp = tempbuf;

    // Populate the keyboard config menu based on the bind.
    // Take care of processing one-to-many bindings properly, too.
    static char const s_gamefunc_[] = "gamefunc_";
    int constexpr strlen_gamefunc_ = ARRAY_SIZE(s_gamefunc_) - 1;

    while ((cp = Bstrstr(cp, s_gamefunc_)))
    {
        cp += strlen_gamefunc_;

        char* semi = Bstrchr(cp, ';');

        if (semi)
            *semi = 0;

        j = CONFIG_FunctionNameToNum(cp);

        if (semi)
            cp = semi + 1;

        if (j != -1)
        {
            ud.config.KeyboardKeys[j][1] = ud.config.KeyboardKeys[j][0];
            ud.config.KeyboardKeys[j][0] = sctokeylut[i].sc;
            //            CONTROL_MapKey(j, sctokeylut[i].sc, ud.config.KeyboardKeys[j][0]);

            if (j == gamefunc_Show_Console)
                OSD_CaptureKey(sctokeylut[i].sc);
        }
    }

    if (!OSD_ParsingScript())
        OSD_Printf("%s\n", parm->raw);

    return OSDCMD_OK;
}

static int osdcmd_unbindall(osdcmdptr_t UNUSED(parm))
{
    UNREFERENCED_CONST_PARAMETER(parm);

    for (int i = 0; i < MAXBOUNDKEYS; ++i)
        CONTROL_FreeKeyBind(i);

    for (int i = 0; i < MAXMOUSEBUTTONS; ++i)
        CONTROL_FreeMouseBind(i);

    for (auto& KeyboardKey : ud.config.KeyboardKeys)
        KeyboardKey[0] = KeyboardKey[1] = 0xff;

    if (!OSD_ParsingScript())
        OSD_Printf("unbound all controls\n");

    return OSDCMD_OK;
}

static int osdcmd_unbind(osdcmdptr_t parm)
{
    if (parm->numparms != 1)
        return OSDCMD_SHOWHELP;

    for (auto& ConsoleKey : sctokeylut)
    {
        if (ConsoleKey.key && !Bstrcasecmp(parm->parms[0], ConsoleKey.key))
        {
            CONTROL_FreeKeyBind(ConsoleKey.sc);
            OSD_Printf("unbound key %s\n", ConsoleKey.key);
            return OSDCMD_OK;
        }
    }

    for (int i = 0; i < MAXMOUSEBUTTONS; i++)
    {
        if (!Bstrcasecmp(parm->parms[0], ConsoleButtons[i]))
        {
            CONTROL_FreeMouseBind(i);
            OSD_Printf("unbound %s\n", ConsoleButtons[i]);
            return OSDCMD_OK;
        }
    }

    return OSDCMD_SHOWHELP;
}

static int osdcmd_unbound(osdcmdptr_t parm)
{
    if (parm->numparms != 1)
        return OSDCMD_OK;

    int const gameFunc = CONFIG_FunctionNameToNum(parm->parms[0]);

    if (gameFunc != -1)
        ud.config.KeyboardKeys[gameFunc][0] = 0;

    return OSDCMD_OK;
}

static int osdcmd_quicksave(const osdfuncparm_t *parm)
{
    UNREFERENCED_PARAMETER(parm);
    if (!(ud.gm & MODE_GAME))
        OSD_Printf("quicksave: not in a game.\n");
    else g_doQuickSave = 1;
    return OSDCMD_OK;
}

static int osdcmd_quickload(const osdfuncparm_t *parm)
{
    UNREFERENCED_PARAMETER(parm);
    if (!(ud.gm & MODE_GAME))
        OSD_Printf("quickload: not in a game.\n");
    else g_doQuickSave = 2;
    return OSDCMD_OK;
}

static int osdcmd_screenshot(const osdfuncparm_t *parm)
{
    UNREFERENCED_PARAMETER(parm);
//    KB_ClearKeysDown();
    videoCaptureScreen("duke0000.tga",0);
    return OSDCMD_OK;
}

extern void G_SaveMapState(mapstate_t *save);
extern void G_RestoreMapState(mapstate_t *save);

/*
static int osdcmd_savestate(const osdfuncparm_t *parm)
{
    UNREFERENCED_PARAMETER(parm);
    if (g_mapInfo[ud.volume_number*MAXLEVELS+ud.level_number].savedstate == NULL)
        g_mapInfo[ud.volume_number*MAXLEVELS+ud.level_number].savedstate = Xcalloc(1,sizeof(mapstate_t));
    G_SaveMapState(g_mapInfo[ud.volume_number*MAXLEVELS+ud.level_number].savedstate);
    return OSDCMD_OK;
}

static int osdcmd_restorestate(const osdfuncparm_t *parm)
{
    UNREFERENCED_PARAMETER(parm);
    if (g_mapInfo[ud.volume_number*MAXLEVELS+ud.level_number].savedstate)
        G_RestoreMapState(g_mapInfo[ud.volume_number*MAXLEVELS+ud.level_number].savedstate);
    return OSDCMD_OK;
}
*/

static int osdcmd_crosshaircolor(osdcmdptr_t parm)
{
    if (parm->numparms != 3)
    {
        OSD_Printf("crosshaircolor: r:%d g:%d b:%d\n", CrosshairColors.r, CrosshairColors.g, CrosshairColors.b);
        return OSDCMD_SHOWHELP;
    }

    uint8_t const r = Batol(parm->parms[0]);
    uint8_t const g = Batol(parm->parms[1]);
    uint8_t const b = Batol(parm->parms[2]);

    G_SetCrosshairColor(r, g, b);

    if (!OSD_ParsingScript())
        OSD_Printf("%s\n", parm->raw);

    return OSDCMD_OK;
}

static int osdcmd_maxfps(const osdfuncparm_t *parm)
{
    //extern unsigned int g_frameDelay;

    if (parm->numparms != 1)
    {
        OSD_Printf("\"r_maxfps\" is \"%d\"\n",r_maxfps);
        return OSDCMD_SHOWHELP;
    }
    r_maxfps = max(0,min(1000,Batoi(parm->parms[0])));
    if (r_maxfps) g_frameDelay = (1000/r_maxfps);
    else g_frameDelay = 0;
    OSD_Printf("%s\n",parm->raw);
    return OSDCMD_OK;
}

static int osdcmd_inittimer(const osdfuncparm_t *parm)
{
    if (parm->numparms != 1)
    {
        OSD_Printf("%dHz timer\n",g_timerTicsPerSecond);
        return OSDCMD_SHOWHELP;
    }

    G_InitTimer(Batol(parm->parms[0]));

    OSD_Printf("%s\n",parm->raw);
    return OSDCMD_OK;
}


int registerosdcommands(void)
{
    FX_InitCvars();

    unsigned int i;

    osdcmd_cheatsinfo_stat.cheatnum = -1;

    for (auto& cv : cvar)
    {
        switch (cv.flags & (CVAR_FUNCPTR | CVAR_MULTI))
        {
        case CVAR_FUNCPTR:
            OSD_RegisterCvar(&cv, osdcmd_cvar_set_game); break;
        case CVAR_MULTI:
        case CVAR_FUNCPTR | CVAR_MULTI:
            OSD_RegisterCvar(&cv, osdcmd_cvar_set_multi); break;
        default:
            OSD_RegisterCvar(&cv, osdcmd_cvar_set); break;
        }
    }

    if (VOLUMEONE)
        OSD_RegisterFunction("changelevel","changelevel <level>: warps to the given level", osdcmd_changelevel);
    else
    {
        OSD_RegisterFunction("changelevel","changelevel <volume> <level>: warps to the given level", osdcmd_changelevel);
        OSD_RegisterFunction("map","map <mapfile>: loads the given user map", osdcmd_map);
    }

    OSD_RegisterFunction("addpath","addpath <path>: adds path to game filesystem", osdcmd_addpath);

    OSD_RegisterFunction("bind","bind <key> <string>: associates a keypress with a string of console input. Type \"bind showkeys\" for a list of keys and \"listsymbols\" for a list of valid console commands.", osdcmd_bind);

    OSD_RegisterFunction("crosshaircolor","crosshaircolor: changes crosshair color", osdcmd_crosshaircolor);
    OSD_RegisterFunction("cmenu","cmenu <#>: jumps to menu", osdcmd_cmenu);

    OSD_RegisterFunction("echo","echo [text]: echoes text to the console", osdcmd_echo);

    OSD_RegisterFunction("fileinfo","fileinfo <file>: gets a file's information", osdcmd_fileinfo);

    for (i=0;i<NUMGAMEFUNCTIONS;i++)
    {
        char *t;
        int j;

        if (!Bstrcmp(gamefunctions[i],"Show_Console")) continue;

        Bsprintf(tempbuf,"gamefunc_%s",gamefunctions[i]);
        t = Xstrdup(tempbuf);
        for (j=Bstrlen(t);j>=0;j--)
            t[j] = Btolower(t[j]);
        Bstrcat(tempbuf,": game button");
        OSD_RegisterFunction(t,Xstrdup(tempbuf),osdcmd_button);
    }

//    OSD_RegisterFunction("setbrightness","setbrightness <value>: changes brightness (obsolete)", osdcmd_setbrightness);
    OSD_RegisterFunction("give","give <all|health|weapons|ammo|armor|keys|inventory>: gives requested item", osdcmd_give);
    OSD_RegisterFunction("god","god: toggles god mode", osdcmd_god);

    OSD_RegisterFunction("initgroupfile","initgroupfile <path>: adds a grp file into the game filesystem", osdcmd_initgroupfile);
    OSD_RegisterFunction("inittimer","debug", osdcmd_inittimer);

    OSD_RegisterFunction("name","name: change your multiplayer nickname", osdcmd_name);
    OSD_RegisterFunction("noclip","noclip: toggles clipping mode", osdcmd_noclip);

    OSD_RegisterFunction("quicksave","quicksave: performs a quick save", osdcmd_quicksave);
    OSD_RegisterFunction("quickload","quickload: performs a quick load", osdcmd_quickload);
    OSD_RegisterFunction("quit","quit: exits the game immediately", osdcmd_quit);
    OSD_RegisterFunction("exit","exit: exits the game immediately", osdcmd_quit);

    OSD_RegisterFunction("packetrate","packetrate: sets the multiplayer packet send rate, in packets/sec",osdcmd_packetrate);
    OSD_RegisterFunction("restartsound","restartsound: reinitializes the sound system",osdcmd_restartsound);
    OSD_RegisterFunction("restartvid","restartvid: reinitializes the video mode",osdcmd_restartvid);

    OSD_RegisterFunction("r_maxfps", "r_maxfps: sets a framerate cap",osdcmd_maxfps);

    OSD_RegisterFunction("addlogvar","addlogvar <gamevar>: prints the value of a gamevar", osdcmd_addlogvar);
    OSD_RegisterFunction("setvar","setvar <gamevar> <value>: sets the value of a gamevar", osdcmd_setvar);
    OSD_RegisterFunction("setvarvar","setvarvar <gamevar1> <gamevar2>: sets the value of <gamevar1> to <gamevar2>", osdcmd_setvar);
    OSD_RegisterFunction("setactorvar","setactorvar <actor#> <gamevar> <value>: sets the value of <actor#>'s <gamevar> to <value>", osdcmd_setactorvar);
    OSD_RegisterFunction("screenshot","screenshot: takes a screenshot.  See r_scrcaptureformat.", osdcmd_screenshot);

    OSD_RegisterFunction("spawn","spawn <picnum> [palnum] [cstat] [ang] [x y z]: spawns a sprite with the given properties",osdcmd_spawn);

    OSD_RegisterFunction("unbind","unbind <key>: unbinds a key", osdcmd_unbind);
    OSD_RegisterFunction("unbindall","unbindall: unbinds all keys", osdcmd_unbindall);
    OSD_RegisterFunction("unbound", NULL, osdcmd_unbound);

    OSD_RegisterFunction("vidmode","vidmode <xdim> <ydim> <bpp> <fullscreen>: change the video mode",osdcmd_vidmode);

    OSD_RegisterFunction("fraglimit", "fraglimit <value>: sets a frag limit for DukeMatch", osdcmd_fraglimit);
    OSD_RegisterFunction("debugplayer", "debugplayer <value>: gets some limited debugging info for a player", osdcmd_debugplayer);
//    OSD_RegisterFunction("savestate","",osdcmd_savestate);
//    OSD_RegisterFunction("restorestate","",osdcmd_restorestate);
    //baselayer_onvideomodechange = onvideomodechange;

    return 0;
}

