#ifndef _WIN32
#error Only for Windows
#endif

#include "duke3d.h"
#include "sounds.h"

#include "build.h"
#include "renderlayer.h"
#include "compat.h"

#include "grpscan.h"
#include "texcache.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#ifndef _WIN32_IE
#define _WIN32_IE 0x0300
#endif
#include <commctrl.h>
#include <stdio.h>

#include "startwin.game.h"

const char* credits = {
    "Developed by: Jordon \"Striker The Hedgefox\" Moss.\r\n\r\n"
    "NetDuke32 is a multiplayer-centric port based on EDuke32. "
    "Started late 2015 as a series of maintenance builds of EDuke32-OldMP, it has since "
    "evolved. NetDuke32 brings new features, QoL and netcode improvements, and tries "
    "to improve the overall multiplayer experience.\r\n\r\n"
    "This is a labour of love, and would be impossible without support.\r\n"
    "If you like my work, help me out Ko-Fi! (If you're able to, of course.)\r\n"
    "- https://ko-fi.com/StrikerTheHedgefox\r\n\r\n"
    "-= Special Thanks =-\r\n"
    "* The Shadow Mavericks Clan - For being my best (and sometimes only) friends since 2006, and the only reason I even get out of bed. I wouldn't be alive today without you guys.\r\n\r\n"
    "* TerminX, Hendricks266, & Voidpoint - For making EDuke32, helping me during hard times, and giving me a chance to be a part of something as awesome as Ion Fury.\r\n\r\n"
    "* Shotspark Studios - For helping promote this project, and giving me a home on Discord to share my work.\r\n\r\n"
    "* The Build Bros Community - For playing and testing the absolute shit out of this thing, their impeccable work on StrikerDM's community maps, and supporting me over the years.\r\n\r\n"
    "* NY00123 - For helping me with all kinds of cross-platform issues and bugs, and for helping me find the root cause of some infuriatingly difficult to debug issues.\r\n\r\n"
    "* Doom64hunter - For a ton of support and help with all sorts of issues.\r\n\r\n"
    "-= Donator Hall of Fame =-\r\n"
    "- Balls of Steel Forever\r\n"
    "- Doom64hunter\r\n"
    "- Replica\r\n"
    "- KunaiKitsune\r\n"
    "- Gosimer\r\n"
    "- DatMaxNub\r\n"
    "- Noldor Ranzou\r\n"
    "- Fernando Márquez\r\n"
    "- Marz\r\n"
    "- Radar\r\n"
    "- Vandclash\r\n"
    "- LittleTijn\r\n"
    "- aaBlueDragon\r\n"
    "- Yasha\r\n"
    "- TheXna\r\n"
    "\r\n"
    "(If I've missed you on this list, please DM me on Ko-Fi with your preferred name and proof of donation!)\r\n"
};

enum
{
    TAB_CONFIG,
    TAB_MESSAGES,
    TAB_ABOUT,
    TAB_NUM,
};

static struct
{
    struct grpfile_t const* grp;
    int fullscreen;
    int xdim, ydim, bpp;
    int forcesetup;
    int usemouse, usejoy;
    int game;
    uint32_t crcval; // for finding the grp in the list again
    char *gamedir;
}
settings;

static HWND startupdlg = NULL;
static HWND pages[TAB_NUM] =
{
    NULL, NULL, NULL
};
static int done = -1, mode = TAB_CONFIG;

static BUILDVFS_FIND_REC *finddirs=NULL;
static int numdirs=0;

static void clearfilenames(void)
{
    klistfree(finddirs);
    finddirs = NULL;
    numdirs = 0;
}

static int getfilenames(const char *path)
{
    BUILDVFS_FIND_REC *r;

    clearfilenames();
    finddirs = klistpath(path,"*",BUILDVFS_FIND_DIR);
    for (r = finddirs; r; r=r->next) numdirs++;
    return(0);
}

#define POPULATE_VIDEO 1
#define POPULATE_CONFIG 2
#define POPULATE_GAME 4
#define POPULATE_GAMEDIRS 8

static void PopulateForm(int pgs)
{
    HWND hwnd;
    char buf[256];
    int i,j;

    if (pgs & POPULATE_VIDEO)
    {
        int mode;

        hwnd = GetDlgItem(pages[TAB_CONFIG], IDCVMODE);

        mode = videoCheckMode(&settings.xdim, &settings.ydim, settings.bpp, settings.fullscreen, 1);
        if (mode < 0)
        {
            int cd[] = { 32, 24, 16, 15, 8, 0 };
            for (i=0; cd[i];)
            {
                if (cd[i] >= settings.bpp) i++;
                else break;
            }
            for (; cd[i]; i++)
            {
                mode = videoCheckMode(&settings.xdim, &settings.ydim, cd[i], settings.fullscreen, 1);
                if (mode < 0) continue;
                settings.bpp = cd[i];
                break;
            }
        }

        Button_SetCheck(GetDlgItem(pages[TAB_CONFIG], IDCFULLSCREEN), (settings.fullscreen ? BST_CHECKED : BST_UNCHECKED));
        (void)ComboBox_ResetContent(hwnd);
        for (i=0; i<validmodecnt; i++)
        {
            if (validmode[i].fs != settings.fullscreen) continue;

            // all modes get added to the 3D mode list
            Bsprintf(buf, "%d x %d %dbpp", validmode[i].xdim, validmode[i].ydim, validmode[i].bpp);
            j = ComboBox_AddString(hwnd, buf);
            (void)ComboBox_SetItemData(hwnd, j, i);
            if (i == mode)(void)ComboBox_SetCurSel(hwnd, j);
        }
    }

    if (pgs & POPULATE_CONFIG)
    {
        hwnd = GetDlgItem(pages[TAB_CONFIG], IDCSOUNDDRV);
        (void)ComboBox_ResetContent(hwnd);

        Button_SetCheck(GetDlgItem(pages[TAB_CONFIG], IDCALWAYSSHOW), (settings.forcesetup ? BST_CHECKED : BST_UNCHECKED));

        Button_SetCheck(GetDlgItem(pages[TAB_CONFIG], IDCINPUTMOUSE), (settings.usemouse ? BST_CHECKED : BST_UNCHECKED));
        Button_SetCheck(GetDlgItem(pages[TAB_CONFIG], IDCINPUTJOY), (settings.usejoy ? BST_CHECKED : BST_UNCHECKED));
    }

    if (pgs & POPULATE_GAME)
    {
        HWND hwnd = GetDlgItem(pages[TAB_CONFIG], IDCDATA);

        for (auto fg = foundgrps; fg; fg = fg->next)
        {
            Bsprintf(buf, "%s\t%s", fg->type->name, fg->filename);
            int const j = ListBox_AddString(hwnd, buf);
            (void)ListBox_SetItemData(hwnd, j, (LPARAM)fg);
            if (settings.grp == fg)
                (void)ListBox_SetCurSel(hwnd, j);
        }
    }

    if (pgs & POPULATE_GAMEDIRS)
    {
        BUILDVFS_FIND_REC *dirs = NULL;

        hwnd = GetDlgItem(pages[TAB_CONFIG], IDCGAMEDIR);

        getfilenames("/");
        (void)ComboBox_ResetContent(hwnd);
        j = ComboBox_AddString(hwnd, "None");
        (void)ComboBox_SetItemData(hwnd, j, 0);
        (void)ComboBox_SetCurSel(hwnd, j);
        for (dirs=finddirs,i=1; dirs != NULL; dirs=dirs->next,i++)
        {
#if defined(USE_OPENGL)
            if (Bstrcasecmp(TEXCACHEFILE,dirs->name) == 0) continue;
#endif
            j = ComboBox_AddString(hwnd, dirs->name);
            (void)ComboBox_SetItemData(hwnd, j, i);
            if (Bstrcasecmp(dirs->name,settings.gamedir) == 0)
                (void)ComboBox_SetCurSel(hwnd, j);
        }
    }
}

static INT_PTR CALLBACK ConfigPageProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDCFULLSCREEN:
            settings.fullscreen = !settings.fullscreen;
            PopulateForm(POPULATE_VIDEO);
            return TRUE;
        case IDCVMODE:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                int i;
                i = ComboBox_GetCurSel((HWND)lParam);
                if (i != CB_ERR) i = ComboBox_GetItemData((HWND)lParam, i);
                if (i != CB_ERR)
                {
                    settings.xdim = validmode[i].xdim;
                    settings.ydim = validmode[i].ydim;
                    settings.bpp  = validmode[i].bpp;
                }
            }
            return TRUE;
        case IDCALWAYSSHOW:
            settings.forcesetup = IsDlgButtonChecked(hwndDlg, IDCALWAYSSHOW) == BST_CHECKED;
            return TRUE;
        case IDCINPUTMOUSE:
            settings.usemouse = IsDlgButtonChecked(hwndDlg, IDCINPUTMOUSE) == BST_CHECKED;
            return TRUE;
        case IDCINPUTJOY:
            settings.usejoy = IsDlgButtonChecked(hwndDlg, IDCINPUTJOY) == BST_CHECKED;
            return TRUE;
        case IDCGAMEDIR:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                int i,j;
                BUILDVFS_FIND_REC *dir = NULL;
                i = ComboBox_GetCurSel((HWND)lParam);
                if (i != CB_ERR) i = ComboBox_GetItemData((HWND)lParam, i);
                if (i != CB_ERR)
                {
                    if (i==0)
                        settings.gamedir = NULL;
                    else
                    {
                        for (j=1,dir=finddirs;dir != NULL;dir=dir->next,j++)
                            if (j == i)
                            {
                                settings.gamedir = dir->name;
                                break;
                            }
                    }
                }
            }
            return TRUE;
        case IDCDATA:
        {
            if (HIWORD(wParam) != LBN_SELCHANGE) break;
            intptr_t i = ListBox_GetCurSel((HWND)lParam);
            if (i != CB_ERR) i = ListBox_GetItemData((HWND)lParam, i);
            if (i != CB_ERR)
            {
                settings.grp = (grpfile_t const*)i;
            }
            return TRUE;
        }
        default:
            break;
        }
        break;
    default:
        break;
    }
    return FALSE;
}

static void SetPage(int n)
{
    HWND tab;
    int cur;
    tab = GetDlgItem(startupdlg, WIN_STARTWIN_TABCTL);
    cur = (int)SendMessage(tab, TCM_GETCURSEL,0,0);
    ShowWindow(pages[cur],SW_HIDE);
    SendMessage(tab, TCM_SETCURSEL, n, 0);
    ShowWindow(pages[n],SW_SHOW);
    mode = n;

    SetFocus(GetDlgItem(startupdlg, WIN_STARTWIN_TABCTL));
}

static void EnableConfig(int n)
{
    //EnableWindow(GetDlgItem(startupdlg, WIN_STARTWIN_CANCEL), n);
    EnableWindow(GetDlgItem(startupdlg, WIN_STARTWIN_START), n);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDCFULLSCREEN), n);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDCVMODE), n);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDCINPUTMOUSE), n);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDCINPUTJOY), n);

    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDCDATA), n);
    EnableWindow(GetDlgItem(pages[TAB_CONFIG], IDCGAMEDIR), n);
}

static INT_PTR CALLBACK startup_dlgproc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HBITMAP hbmp = NULL;
    HDC hdc;

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        HWND hwnd;
        RECT r, rdlg, chrome, rtab, rcancel, rstart;
        int xoffset = 0, yoffset = 0;

        // Fetch the positions (in screen coordinates) of all the windows we need to tweak
        ZeroMemory(&chrome, sizeof(chrome));
        AdjustWindowRect(&chrome, GetWindowLong(hwndDlg, GWL_STYLE), FALSE);
        GetWindowRect(hwndDlg, &rdlg);
        GetWindowRect(GetDlgItem(hwndDlg, WIN_STARTWIN_TABCTL), &rtab);
        GetWindowRect(GetDlgItem(hwndDlg, WIN_STARTWIN_CANCEL), &rcancel);
        GetWindowRect(GetDlgItem(hwndDlg, WIN_STARTWIN_START), &rstart);

        // Knock off the non-client area of the main dialogue to give just the client area
        rdlg.left -= chrome.left;
        rdlg.top -= chrome.top;
        rdlg.right -= chrome.right;
        rdlg.bottom -= chrome.bottom;

        // Translate them to client-relative coordinates wrt the main dialogue window
        rtab.right -= rtab.left - 1;
        rtab.bottom -= rtab.top - 1;
        rtab.left  -= rdlg.left;
        rtab.top -= rdlg.top;

        rcancel.right -= rcancel.left - 1;
        rcancel.bottom -= rcancel.top - 1;
        rcancel.left -= rdlg.left;
        rcancel.top -= rdlg.top;

        rstart.right -= rstart.left - 1;
        rstart.bottom -= rstart.top - 1;
        rstart.left -= rdlg.left;
        rstart.top -= rdlg.top;

        // And then convert the main dialogue coordinates to just width/length
        rdlg.right -= rdlg.left - 1;
        rdlg.bottom -= rdlg.top - 1;
        rdlg.left = 0;
        rdlg.top = 0;

        // Load the bitmap into the bitmap control and fetch its dimensions
        hbmp = LoadBitmap((HINSTANCE)win_gethinstance(), MAKEINTRESOURCE(RSRC_BMP));
        hwnd = GetDlgItem(hwndDlg,WIN_STARTWIN_BITMAP);
        SendMessage(hwnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hbmp);
        GetClientRect(hwnd, &r);
        xoffset = r.right;
        yoffset = r.bottom - rdlg.bottom;

        // Shift and resize the controls that require it
        rtab.left += xoffset;
        rtab.bottom += yoffset;
        rcancel.left += xoffset;
        rcancel.top += yoffset;
        rstart.left += xoffset;
        rstart.top += yoffset;
        rdlg.right += xoffset;
        rdlg.bottom += yoffset;

        // Move the controls to their new positions
        MoveWindow(GetDlgItem(hwndDlg, WIN_STARTWIN_TABCTL), rtab.left, rtab.top, rtab.right, rtab.bottom, FALSE);
        MoveWindow(GetDlgItem(hwndDlg, WIN_STARTWIN_CANCEL), rcancel.left, rcancel.top, rcancel.right, rcancel.bottom, FALSE);
        MoveWindow(GetDlgItem(hwndDlg, WIN_STARTWIN_START), rstart.left, rstart.top, rstart.right, rstart.bottom, FALSE);

        // Move the main dialogue to the centre of the screen
        hdc = GetDC(NULL);
        rdlg.left = (GetDeviceCaps(hdc, HORZRES) - rdlg.right) / 2;
        rdlg.top = (GetDeviceCaps(hdc, VERTRES) - rdlg.bottom) / 2;
        ReleaseDC(NULL, hdc);
        MoveWindow(hwndDlg, rdlg.left + chrome.left, rdlg.top + chrome.left,
                   rdlg.right + (-chrome.left+chrome.right), rdlg.bottom + (-chrome.top+chrome.bottom), TRUE);

        // Add tabs to the tab control
        {
            static char tabText[][32] = {
                TEXT("Setup"),
                TEXT("Message Log"),
                TEXT("About NetDuke32"),
            };

            TCITEM tab;

            hwnd = GetDlgItem(hwndDlg, WIN_STARTWIN_TABCTL);

#define SETUP_TAB(tabstruct, tabId) tabstruct.mask = TCIF_TEXT;         \
                                    tabstruct.pszText = tabText[tabId]; \
                                    SendMessage(hwnd, TCM_INSERTITEM, (WPARAM)tabId, (LPARAM)&tabstruct);

            ZeroMemory(&tab, sizeof(tab));
            SETUP_TAB(tab, TAB_CONFIG);
            SETUP_TAB(tab, TAB_MESSAGES);
            SETUP_TAB(tab, TAB_ABOUT);

            // Work out the position and size of the area inside the tab control for the pages
            ZeroMemory(&r, sizeof(r));
            GetClientRect(hwnd, &r);
            SendMessage(hwnd, TCM_ADJUSTRECT, FALSE, (LPARAM)&r);
            r.right -= r.left-1;
            r.bottom -= r.top-1;
            r.top += rtab.top;
            r.left += rtab.left;

            // Create the pages and position them in the tab control, but hide them
            pages[TAB_CONFIG] = CreateDialog((HINSTANCE)win_gethinstance(),
                                             MAKEINTRESOURCE(WIN_STARTWINPAGE_CONFIG), hwndDlg, ConfigPageProc);
            pages[TAB_MESSAGES] = GetDlgItem(hwndDlg, WIN_STARTWIN_MESSAGES);
            pages[TAB_ABOUT] = CreateDialog((HINSTANCE)win_gethinstance(),
                                             MAKEINTRESOURCE(WIN_STARTWINPAGE_ABOUT), hwndDlg, NULL);

            SetWindowPos(pages[TAB_CONFIG], hwnd, r.left, r.top, r.right, r.bottom, SWP_HIDEWINDOW);
            SetWindowPos(pages[TAB_MESSAGES], hwnd, r.left, r.top, r.right, r.bottom, SWP_HIDEWINDOW);
            SetWindowPos(pages[TAB_ABOUT], hwnd, r.left, r.top, r.right, r.bottom, SWP_HIDEWINDOW);

            // Tell the editfield acting as the console to exclude the width of the scrollbar
            GetClientRect(pages[TAB_MESSAGES],&r);
            r.right -= GetSystemMetrics(SM_CXVSCROLL)+4;
            r.left = r.top = 0;
            SendMessage(pages[TAB_MESSAGES], EM_SETRECTNP,0,(LPARAM)&r);
            SendMessage(pages[TAB_ABOUT], EM_SETRECTNP, 0, (LPARAM)&r);
            SetDlgItemText(pages[TAB_ABOUT], WIN_STARTWIN_CREDITS, credits);

            // Set a tab stop in the game data listbox
            {
                DWORD tabs[1] = { 150 };
                (void)ListBox_SetTabStops(GetDlgItem(pages[TAB_CONFIG], IDCDATA), 1, tabs);
            }

            SetFocus(GetDlgItem(hwndDlg, WIN_STARTWIN_START));
            SetWindowText(hwndDlg, apptitle);
        }
        return FALSE;
    }

    case WM_NOTIFY:
    {
        LPNMHDR nmhdr = (LPNMHDR)lParam;
        int cur;
        if (nmhdr->idFrom != WIN_STARTWIN_TABCTL) break;
        cur = (int)SendMessage(nmhdr->hwndFrom, TCM_GETCURSEL,0,0);
        switch (nmhdr->code)
        {
        case TCN_SELCHANGING:
        {
            if (cur < 0 || !pages[cur]) break;
            ShowWindow(pages[cur],SW_HIDE);
            return TRUE;
        }
        case TCN_SELCHANGE:
        {
            if (cur < 0 || !pages[cur]) break;
            ShowWindow(pages[cur],SW_SHOW);
            return TRUE;
        }
        }
        break;
    }

    case WM_CLOSE:
        if (mode == TAB_CONFIG) done = 0;
        else quitevent++;
        return TRUE;

    case WM_DESTROY:
        if (hbmp)
        {
            DeleteObject(hbmp);
            hbmp = NULL;
        }

        if (pages[TAB_CONFIG])
        {
            DestroyWindow(pages[TAB_CONFIG]);
            pages[TAB_CONFIG] = NULL;
        }

        startupdlg = NULL;
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case WIN_STARTWIN_CANCEL:
            if (mode == TAB_CONFIG) done = 0;
            else quitevent++;
            return TRUE;
        case WIN_STARTWIN_START:
            done = 1;
            return TRUE;
        }
        return FALSE;

    case WM_CTLCOLORSTATIC:
        if ((HWND)lParam == pages[TAB_MESSAGES])
            return (BOOL)(intptr_t)GetSysColorBrush(COLOR_WINDOW);
        break;

    default:
        break;
    }

    return FALSE;
}

bool startwin_isopen(void)
{
    return !!startupdlg;
}

int startwin_open(void)
{
    INITCOMMONCONTROLSEX icc;
    if (startupdlg) return 1;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icc);
    startupdlg = CreateDialog((HINSTANCE)win_gethinstance(), MAKEINTRESOURCE(WIN_STARTWIN), NULL, startup_dlgproc);
    if (startupdlg)
    {
        SetPage(TAB_MESSAGES);
        EnableConfig(0);
        return 0;
    }
    return -1;
}

int startwin_close(void)
{
    if (!startupdlg) return 1;
    DestroyWindow(startupdlg);
    startupdlg = NULL;
    return 0;
}

int startwin_puts(const char *buf)
{
    const char *p = NULL, *q = NULL;
    static char workbuf[1024];
    static int newline = 0;
    int curlen, linesbefore, linesafter;
    HWND edctl;
    int vis;
    static HWND dactrl = NULL;

    if (!startupdlg) return 1;

    edctl = pages[TAB_MESSAGES];
    if (!edctl) return -1;

    if (!dactrl) dactrl = GetDlgItem(startupdlg, WIN_STARTWIN_TABCTL);

    vis = ((int)SendMessage(dactrl, TCM_GETCURSEL,0,0) == TAB_MESSAGES);

    if (vis) SendMessage(edctl, WM_SETREDRAW, FALSE,0);
    curlen = SendMessage(edctl, WM_GETTEXTLENGTH, 0,0);
    SendMessage(edctl, EM_SETSEL, (WPARAM)curlen, (LPARAM)curlen);
    linesbefore = SendMessage(edctl, EM_GETLINECOUNT, 0,0);
    p = buf;
    while (*p)
    {
        if (newline)
        {
            SendMessage(edctl, EM_REPLACESEL, 0, (LPARAM)"\r\n");
            newline = 0;
        }
        q = p;
        while (*q && *q != '\n') q++;
        memcpy(workbuf, p, q-p);
        if (*q == '\n')
        {
            if (!q[1])
            {
                newline = 1;
                workbuf[q-p] = 0;
            }
            else
            {
                workbuf[q-p] = '\r';
                workbuf[q-p+1] = '\n';
                workbuf[q-p+2] = 0;
            }
            p = q+1;
        }
        else
        {
            workbuf[q-p] = 0;
            p = q;
        }
        SendMessage(edctl, EM_REPLACESEL, 0, (LPARAM)workbuf);
    }
    linesafter = SendMessage(edctl, EM_GETLINECOUNT, 0,0);
    SendMessage(edctl, EM_LINESCROLL, 0, linesafter-linesbefore);
    if (vis) SendMessage(edctl, WM_SETREDRAW, TRUE,0);
    return 0;
}

int startwin_settitle(const char *str)
{
    if (!startupdlg) return 1;
    SetWindowText(startupdlg, str);
    return 0;
}

int startwin_idle(void *v)
{
    if (!startupdlg || !IsWindow(startupdlg)) return 0;
    if (IsDialogMessage(startupdlg, (MSG*)v)) return 1;
    return 0;
}

int startwin_run(void)
{
    MSG msg;
    if (!startupdlg) return 1;

    done = -1;

#ifdef JFAUD
    EnumAudioDevs(&wavedevs, NULL, NULL);
#endif
    SetPage(TAB_CONFIG);
    EnableConfig(1);

    settings.fullscreen = ud.config.ScreenMode;
    settings.xdim = ud.config.ScreenWidth;
    settings.ydim = ud.config.ScreenHeight;
    settings.bpp = ud.config.ScreenBPP;
    settings.forcesetup = ud.config.ForceSetup;
    settings.usemouse = ud.config.UseMouse;
    settings.usejoy = ud.config.UseJoystick;
    settings.game = g_gameType;
//    settings.crcval = 0;
    settings.grp = g_selectedGrp;
    settings.gamedir = g_modDir;
    PopulateForm(-1);

    while (done < 0)
    {
        switch (GetMessage(&msg, NULL, 0,0))
        {
        case 0:
            done = 1;
            break;
        case -1:
            return -1;
        default:
            if (IsWindow(startupdlg) && IsDialogMessage(startupdlg, &msg)) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            break;
        }
    }

    SetPage(TAB_MESSAGES);
    EnableConfig(0);
    if (done)
    {
        ud.config.ScreenMode = settings.fullscreen;
        ud.config.ScreenWidth = settings.xdim;
        ud.config.ScreenHeight = settings.ydim;
        ud.config.ScreenBPP = settings.bpp;
        ud.config.ForceSetup = settings.forcesetup;
        ud.config.UseMouse = settings.usemouse;
        ud.config.UseJoystick = settings.usejoy;
        g_selectedGrp = settings.grp;
        g_gameType = settings.game;

        if (g_modDir != settings.gamedir)
            Bstrcpy(g_modDir, (g_noSetup == 0 && settings.gamedir != NULL) ? settings.gamedir : "/");
    }

    return done;
}

