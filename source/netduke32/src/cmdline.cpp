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

#include "duke3d.h"
//#include "demo.h"
#include "screens.h"
#include "renderlayer.h"
#include "cmdline.h"

int32_t g_commandSetup = 0;
int32_t g_noSetup = 0;
int32_t g_noAutoLoad = 0;
int32_t g_noSound = 0;
int32_t g_noMusic = 0;
int32_t g_noLogoAnim = 0;
int32_t g_noLogo = 0;

int32_t g_keepAddr = 0; // Seemingly unused...

const char* CommandMap = NULL;
const char* CommandName = NULL;
int32_t CommandWeaponChoice = 0;

int32_t netparamcount = 0;
char** netparam = NULL;

char firstdemofile[80] = { '\0' };
int32_t userconfiles = 0;

static void G_ShowParameterHelp(void)
{
    const char* s = "Usage: " APPBASENAME " [files] [options]\n"
        "Example: " APPBASENAME " -q4 -a -m -tx -map nukeland.map\n\n"
        "Files can be *.grp/zip/con/def\n"
        "\n"
        "-allowvisibility\tAllows visibility adjustment in multiplayer."
        "\n"
        "-cfg [file.cfg]\tUse an alternate configuration file\n"
        "-c#\t\tUse MP mode #, 1 = Dukematch, 2 = Coop, 3 = Team Dukematch\n"
        "-d[file.dmo]\tPlay a demo\n"
        "-g[file.grp]\tUse an extra group file\n"
        "-h[file.def]\tUse an alternate def\n"
        "-j[dir]\t\tAdds a directory to EDuke32's search list\n"
        "-l#\t\tWarp to level #, see -v\n"
        "-map [file.map]\tLoads a map\n"
        "-m\t\tDisable monsters\n"
        "-nam/-ww2gi\tRun in NAM or WW2GI-compatible mode\n"
        "-net <command>\tEnable multiplayer (see below)\n"
        "\n"
        "net command examples:\n"
        "-net /n0:3\tHost 3-player match in master/slave mode.\n"
        "-net /n0 192.168.0.1\tJoin specified IP in master/slave mode.\n"
        "\n"
        "-noffire\t\tDisable friendly fire\n"
        "-rts [file.rts]\tLoad a custom Remote Ridicule sound bank\n"
        "-r\t\tRecord demo\n"
        "-s#\t\tSet skill level (1-4)\n"
#if STARTUP_WINDOW
        "-setup/nosetup\tEnables/disables startup window\n"
#endif
        "-t#\t\tSet respawn mode: 1 = Monsters, 2 = Items, 3 = Inventory, x = All\n"
#if !defined(_WIN32)
        "-usecwd\t\tRead game data and configuration file from working directory\n"
#endif
        "-u#########\tUser's favorite weapon order (default: 3425689071)\n"
        "-v#\t\tWarp to volume #, see -l\n"
        "-wstay\t\tEnable Weapon Stay\n"
        "-x[game.con]\tLoad custom CON script\n"
        "-y##\t\tSet frag limit for DukeMatch\n"
        "-#\t\tLoad and run a game from slot # (0-9)\n"
        //              "\n-?/--help\tDisplay this help message and exit\n"
        "\nSee eduke32 -debughelp for debug parameters"
        ;
#if _WIN32
    Bsprintf(tempbuf, HEAD2 " (%s)", s_buildTimestamp);
    wm_msgbox(tempbuf, s);
#else
    LOG_F(INFO, "%s", s);
#endif
}

static void G_ShowDebugHelp(void)
{
    const char* s = "Usage: " APPBASENAME " [files] [options]\n"
        "\n"
        "-a\t\tUse fake player AI (fake multiplayer only)\n"
        "-cachesize #\tSets cache size, in Kb\n"
        "-fNUM\t\tSend fewer packets in multiplayer (1, 2, 4) (deprecated)\n"
        "-game_dir [dir]\tDuke3d_w32 compatibility option, see -j\n"
        "-gamegrp   \tSelects which file to use as main grp\n"
        "-name [name]\tPlayer name in multiplay\n"
        "-nD\t\tDump default gamevars to gamevars.txt\n"
        "-noautoload\tDisable loading content from autoload dir\n"
        "-nologo\t\tSkip the logo anim\n"
        "-ns/-nm\t\tDisable sound or music\n"
        "-q#\t\tFake multiplayer with # (2-8) players\n"
        "-stun\t\tUse UDP hole punching for multiplayer connections\n"
        "-unstable   \tForce EDuke32 to execute unsafe CON commands (and crash)\n"
        "-w\t\tShow coordinates\n"
        "-z#/-condebug\tEnable line-by-line CON compile debugging at level #\n"
        ;
#ifdef _WIN32
    Bsprintf(tempbuf, HEAD2 " (%s)", s_buildTimestamp);
    wm_msgbox(tempbuf, s);
#else
    LOG_F(INFO, "%s", s);
#endif
}

void G_CheckCommandLine(int argc, char const* const* argv)
{
    short i, j;
    char* c;
    int firstnet = 0;
    char* k;

    i = 1;

    ud.god = 0;
    ud.warp_on = 0;
    ud.cashman = 0;
    ud.m_player_skill = ud.player_skill = 2;
    ud.m_dmflags = DEFAULT_DMFLAGS;
    g_player[0].wchoice[0] = 3;
    g_player[0].wchoice[1] = 4;
    g_player[0].wchoice[2] = 5;
    g_player[0].wchoice[3] = 7;
    g_player[0].wchoice[4] = 8;
    g_player[0].wchoice[5] = 6;
    g_player[0].wchoice[6] = 0;
    g_player[0].wchoice[7] = 2;
    g_player[0].wchoice[8] = 9;
    g_player[0].wchoice[9] = 1;
    Bsprintf(ud.wchoice, "3457860291");

    if (argc > 1)
    {
        Bstrcpy(tempbuf, "Application parameters: ");

        while (i < argc)
        {
            Bstrcat(tempbuf, argv[i++]);
            Bstrcat(tempbuf, " ");
        }

        LOG_F(INFO, "%s", tempbuf);

        i = 1;
        while (i < argc)
        {
            c = const_cast<char*>(argv[i]);
            if (((*c == '/') || (*c == '-')) && (!firstnet))
            {
                if (!Bstrcasecmp(c + 1, "?") || !Bstrcasecmp(c + 1, "help") || !Bstrcasecmp(c + 1, "-help"))
                {
                    G_ShowParameterHelp();
                    Bexit(EXIT_SUCCESS);
                }
                if (!Bstrcasecmp(c + 1, "?") || !Bstrcasecmp(c + 1, "debughelp") || !Bstrcasecmp(c + 1, "-debughelp"))
                {
                    G_ShowDebugHelp();
                    Bexit(EXIT_SUCCESS);
                }
                if (!Bstrcasecmp(c + 1, "grp") || !Bstrcasecmp(c + 1, "g"))
                {
                    if (argc > i + 1)
                    {
                        G_AddGroup(argv[i + 1]);
                        i++;
                    }
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "game_dir"))
                {
                    if (argc > i + 1)
                    {
                        Bstrcpy(g_modDir, argv[i + 1]);
                        G_AddPath(argv[i + 1]);
                        i++;
                    }
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "cfg"))
                {
                    if (argc > i + 1)
                    {
                        Bstrcpy(g_setupFileName, argv[i + 1]);
                        i++;
                    }
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "gamegrp"))
                {
                    if (argc > i + 1)
                    {
                        clearGrpNamePtr();
                        g_grpNamePtr = dup_filename(argv[i + 1]);
                        i++;
                    }
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "nam"))
                {
                    g_gameType = GAMEFLAG_NAM;
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "napalm"))
                {
                    g_gameType = GAMEFLAG_NAM | GAMEFLAG_NAPALM;
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "ww2gi"))
                {
                    g_gameType = GAMEFLAG_WW2GI;
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "wstay"))
                {
                    //ud.m_weaponstay = 1;
                    M_DMFLAGS_SET(DMFLAG_WEAPONSTAY, true);
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "setup"))
                {
                    g_commandSetup = TRUE;
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "nosetup"))
                {
                    g_noSetup = 1;
                    g_commandSetup = 0;
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "noautoload"))
                {
                    LOG_F(INFO, "Autoload disabled");
                    g_noAutoLoad = 1;
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "keepaddr"))
                {
                    g_keepAddr = 1;
                    i++;
                    continue;
                }
#if 1 // OLDMP MERGE CHECKME
                if (!Bstrcasecmp(c + 1, "stun"))
                {
                    natfree = 1; //Addfaz NatFree
                    i++;
                    continue;
                }
#endif
                if (!Bstrcasecmp(c + 1, "net"))
                {
                    g_noSetup = TRUE;
                    firstnet = i;
                    netparamcount = argc - i - 1;
                    netparam = (char**)Xcalloc(netparamcount, sizeof(char**));
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "name"))
                {
                    if (argc > i + 1)
                    {
                        CommandName = argv[i + 1];
                        i++;
                    }
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "map"))
                {
                    if (argc > i + 1)
                    {
                        CommandMap = argv[i + 1];
                        i++;
                    }
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "rts"))
                {
                    if (argc > i + 1)
                    {
                        Bfree(g_rtsNamePtr);
                        g_rtsNamePtr = dup_filename(argv[i + 1]);
                        LOG_F(INFO, "Using RTS file \"%s\".", g_rtsNamePtr);
                        i++;
                    }
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "noffire"))
                {
                    //ud.m_ffire = 0;
                    M_DMFLAGS_SET(DMFLAG_FRIENDLYFIRE, false);
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "allowvisibility"))
                {
                    M_DMFLAGS_SET(DMFLAG_ALLOWVISIBILITYCHANGE, true);
                    LOG_F(INFO, "Visibility adjustment enabled for multiplayer.");
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "condebug"))
                {
                    g_scriptDebug = 1;
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "nologo"))
                {
                    g_noLogo = 1;
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "x"))
                {
                    if (argc > i + 1)
                    {
                        G_AddCon(argv[i + 1]);
                        i++;
                    }
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "mx"))
                {
                    if (argc > i + 1)
                    {
                        G_AddConModule(argv[i + 1]);
                        i++;
                    }
                    i++;
                    continue;
                }
#if !defined(_WIN32)
                if (!Bstrcasecmp(c + 1, "usecwd"))
                {
                    g_useCwd = 1;
                    i++;
                    continue;
                }
#endif
                if (!Bstrcasecmp(c + 1, "unstable"))
                {
                    LOG_F(WARNING, "WARNING WARNING WARNING\n"
                        "EDuke32's runtime script error detection has been disabled via "
                        "the '-unstable' command line parameter.  Bug reports from this "
                        "mode are NOT welcome and you should expect crashes in certain "
                        "mods.  Please run EDuke32 without '-unstable' before sending "
                        "any bug reports.\n");
                    g_scriptSanityChecks = 0;
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "cachesize"))
                {
                    if (argc > i + 1)
                    {
                        unsigned int j = atol(argv[i + 1]);
                        MAXCACHE1DSIZE = j << 10;
                        LOG_F(INFO, "Cache size: %dkB", j);
                        i++;
                    }
                    i++;
                    continue;
                }
                if (!Bstrcasecmp(c + 1, "noinstancechecking"))
                {
                    i++;
                    continue;
                }
#if defined(RENDERTYPEWIN) && defined(USE_OPENGL)
                if (!Bstrcasecmp(c + 1, "forcegl"))
                {
                    forcegl = 1;
                    i++;
                    continue;
                }
#endif
            }

            if (firstnet > 0)
            {
                if (*c == '-' || *c == '/')
                {
                    c++;
                    if (((c[0] == 'n') || (c[0] == 'N')) && (c[1] == '0'))
                    {
                        g_networkBroadcastMode = NETMODE_MASTERSLAVE;
                        LOG_F(INFO, "Network mode: master/slave");
                    }
                    else if (((c[0] == 'n') || (c[0] == 'N')) && (c[1] == '1'))
                    {
                        //g_networkBroadcastMode = NETMODE_P2P;
                        LOG_F(INFO, "Network mode: peer-to-peer");
                        G_GameExit("Peer-To-Peer networking is no longer supported! Use master/slave.");
                    }

                }
                netparam[i - firstnet - 1] = const_cast<char*>(argv[i]);
                i++;
                continue;
            }

            if ((*c == '/') || (*c == '-'))
            {
                c++;
                switch (*c)
                {
                    case 'a':
                    case 'A':
                        ud.playerai = 1;
                        LOG_F(INFO, "Other player AI.");
                        break;
                    case 'c':
                    case 'C':
                        c++;
                        ud.m_coop = 0;
                        while ((*c >= '0') && (*c <= '9'))
                        {
                            ud.m_coop *= 10;
                            ud.m_coop += *c - '0';
                            c++;
                        }
                        ud.m_coop--;

                        // Make sure that COOP still defaults to weapon stay ON.
                        if (Gametypes[ud.m_coop].flags & GAMETYPE_WEAPSTAY)
                        {
                            //ud.m_weaponstay = 1;
                            M_DMFLAGS_SET(DMFLAG_WEAPONSTAY, true);
                        }
                        break;
                    case 'd':
                    case 'D':
                        c++;
                        if (strchr(c, '.') == 0)
                            Bstrcat(c, ".dmo");
                        LOG_F(INFO, "Play demo %s.", c);
                        Bstrcpy(firstdemofile, c);
                        break;
                    case 'g':
                    case 'G':
                        c++;
                        if (!*c) break;
                        G_AddGroup(c);
                        break;
                    case 'h':
                    case 'H':
                        c++;
                        if (*c)
                        {
                            G_AddDef(c);
                        }
                        break;
                    case 'j':
                    case 'J':
                        c++;
                        if (!*c) break;
                        G_AddPath(c);
                        break;
                    case 'l':
                    case 'L':
                        ud.warp_on = 1;
                        c++;
                        ud.m_level_number = ud.level_number = (atoi(c) - 1) % MAXLEVELS;
                        break;
                    case 'm':
                    case 'M':
                        if (*(c + 1) != 'a' && *(c + 1) != 'A')
                        {
                            //ud.m_monsters_off = 1;
                            M_DMFLAGS_SET(DMFLAG_NOMONSTERS, true);
                            ud.m_player_skill = ud.player_skill = 0;
                            LOG_F(INFO, "Monsters off.");
                        }
                        break;
                    case 'n':
                    case 'N':
                        c++;
                        if (*c == 's' || *c == 'S')
                        {
                            g_noSound = 2;
                            LOG_F(INFO, "Sound off.");
                        }
                        else if (*c == 'm' || *c == 'M')
                        {
                            g_noMusic = 1;
                            LOG_F(INFO, "Music off.");
                        }
                        else if (*c == 'd' || *c == 'D')
                        {
                            // FILE * fp=fopen("gamevars.txt","w");
                            Gv_Init();
                            Gv_DumpValues();
                            // fclose(fp);
                            LOG_F(INFO, "Game variables saved.");
                        }
                        else
                        {
                            G_ShowParameterHelp();
                            Bexit(-1);
                        }
                        break;
                    case 'q':
                    case 'Q':
                    {
                        LOG_F(INFO, "Fake multiplayer mode.");

                        if (*(++c) == 0) ud.multimode = 1;
                        else ud.multimode = atoi(c) % (MAXPLAYERS + 1);

                        //ud.m_coop = ud.coop = 0;
                        M_DMFLAGS_SET(DMFLAG_MARKERS | DMFLAG_RESPAWNITEMS | DMFLAG_RESPAWNINVENTORY, true);
                        break;
                    }
                    case 'r':
                    case 'R':
                        ud.m_recstat = 1;
                        LOG_F(INFO, "Demo record mode on.");
                        break;
                    case 's':
                    case 'S':
                        c++;
                        ud.m_player_skill = ud.player_skill = (atoi(c) % 5);
                        break;
                    case 't':
                    case 'T':
                        c++;
                        if (Bstrstr(c, "x") != NULL)
                        {
                            LOG_F(INFO, "Respawn All (Items/Inventory/Monsters).");
                            M_DMFLAGS_SET(DMFLAG_RESPAWNMONSTERS | DMFLAG_RESPAWNITEMS | DMFLAG_RESPAWNINVENTORY, true);
                        }
                        else
                        {
                            if (Bstrstr(c, "1") != NULL)
                            {
                                LOG_F(INFO, "Respawn Monsters.");
                                M_DMFLAGS_SET(DMFLAG_RESPAWNMONSTERS, true);
                            }

                            if (Bstrstr(c, "2") != NULL)
                            {
                                LOG_F(INFO, "Respawn Items.");
                                M_DMFLAGS_SET(DMFLAG_RESPAWNITEMS, true);
                            }

                            if (Bstrstr(c, "3") != NULL)
                            {
                                LOG_F(INFO, "Respawn Inventory.");
                                M_DMFLAGS_SET(DMFLAG_RESPAWNINVENTORY, true);
                            }
                        }

                        break;
                    case 'u':
                    case 'U':
                        c++;
                        j = 0;
                        if (*c)
                        {
                            // Only accept numbers from 0 to 9
                            while ((*c >= '0') && (*c <= '9') && j < min<int>(MAX_WEAPONS, 10))
                            {
                                g_player[0].wchoice[j] = *c - '0';
                                ud.wchoice[j] = *c;
                                c++;
                                j++;
                            }

                            // Apply weapon priority if a valid value is given
                            if (j > 0)
                            {
                                CommandWeaponChoice = 1;
                                LOG_F(INFO, "Using favorite weapon order(s).");
                                // Pad value when not all weapons are given
                                while (j < 10)
                                {
                                    if (j == 9)
                                    {
                                        g_player[0].wchoice[9] = 1;
                                        ud.wchoice[9] = '1';
                                    }
                                    else
                                    {
                                        g_player[0].wchoice[j] = 2;
                                        ud.wchoice[j] = '2';
                                    }

                                    j++;
                                }
                            }
                        }

                        if (!CommandWeaponChoice)
                        {
                            LOG_F(INFO, "Using default weapon orders.");
                            g_player[0].wchoice[0] = 3;
                            g_player[0].wchoice[1] = 4;
                            g_player[0].wchoice[2] = 5;
                            g_player[0].wchoice[3] = 7;
                            g_player[0].wchoice[4] = 8;
                            g_player[0].wchoice[5] = 6;
                            g_player[0].wchoice[6] = 0;
                            g_player[0].wchoice[7] = 2;
                            g_player[0].wchoice[8] = 9;
                            g_player[0].wchoice[9] = 1;
                            Bsprintf(ud.wchoice, "3457860291");
                        }
                        break;
                    case 'v':
                    case 'V':
                        c++;
                        ud.warp_on = 1;
                        ud.m_volume_number = ud.volume_number = atoi(c) - 1;
                        break;
                    case 'w':
                    case 'W':
                        ud.coords = 1;
                        break;
                    case 'y':
                    case 'Y':
                        c++;
                        ud.fraglimit = atoi(c);
                        break;
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                        ud.warp_on = 2 + (*c) - '0';
                        break;
                    case 'z':
                    case 'Z':
                        c++;
                        g_scriptDebug = atoi(c);
                        if (!g_scriptDebug)
                            g_scriptDebug = 1;
                        break;
                }
            }
            else
            {
                k = Bstrchr(c, '.');
                if (k)
                {
                    if (!Bstrcasecmp(k, ".map"))
                    {
                        CommandMap = argv[i++];
                        continue;
                    }
                    if (!Bstrcasecmp(k, ".grp") || !Bstrcasecmp(k, ".zip") || !Bstrcasecmp(k, ".pk3"))
                    {
                        G_AddGroup(argv[i++]);
                        continue;
                    }
                    if (!Bstrcasecmp(k, ".con"))
                    {
                        G_AddCon(const_cast<char*>(argv[i++]));
                        userconfiles = 1;
                        continue;
                    }
                    if (!Bstrcasecmp(k, ".def"))
                    {
                        G_AddDef(argv[i++]);
                        continue;
                    }
                    if (!Bstrcasecmp(k, ".rts"))
                    {
                        Bfree(g_rtsNamePtr);
                        g_rtsNamePtr = dup_filename(argv[i++]);
                        LOG_F(INFO, "Using RTS file \"%s\".", g_rtsNamePtr);
                        continue;
                    }
                }
            }
            i++;
        }
    }
}
