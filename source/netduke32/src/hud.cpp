#include "duke3d.h"
#include "hud.h"
#include "colmatch.h"

#define USE_PATCHING 0

int32_t althud_numbertile = 2930;
int32_t althud_numberpal = 0;
int32_t althud_shadows = 1;
int32_t althud_flashing = 1;

int32_t hud_glowingquotes = 1;
int32_t hud_showmapname = 1;
int32_t hud_fragbarstyle = 2;

palette_t CrosshairColors = { 255, 255, 0, 0 };
palette_t DefaultCrosshairColors = { 0, 0, 0, 0 };
int32_t g_crosshairSum = 0;

int32_t g_levelTextTime = 0;

static void G_PatchStatusBar(int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    int32_t const statusTile = BOTTOMSTATUSBAR;
    int32_t const scl = sbarsc(65536);
    int32_t const tx = sbarx16((160 << 16) - (tilesiz[statusTile].x << 15)); // centered
    int32_t const ty = sbary(200 - tilesiz[statusTile].y);

    vec2_t cl1 = { sbarsc(get_xdim(x1)), sbarsc(get_ydim(y1)) };
    vec2_t cl2 = { sbarsc(get_xdim(x2)), sbarsc(get_ydim(y2)) };
    vec2_t clofs = { ((xdim - sbarsc(xdim)) >> 1), (ydim - sbarsc(ydim)) };

    if (x1 == 0 && y1 == 0 && x2 == 320 && y2 == 200) // Patching full screen, so accomodate widescreen bars.
    {
        cl1 = { 0, 0 };
        cl2 = { xdim, ydim };
        clofs = { 0, 0 };
    }
    
    rotatesprite(tx, ty, scl, 0, statusTile, 4, 0, RS_AUTO | RS_NOCLIP | RS_TOPLEFT | RS_NOMASK, cl1.x + clofs.x, cl1.y + clofs.y, cl2.x + clofs.x - 1, cl2.y + clofs.y - 1);
}

void G_SetStatusBarScale(int sc)
{
    ud.statusbarscale = min(100, max(10, sc));
    G_UpdateScreenArea();
}

// For compact fragbar style.
static void G_DrawFragbarCell(const char* text, uint8_t pal, int32_t ypos, int32_t value)
{
    int32_t const xpos = 259;
    int32_t const x_offset = scale(120, xdim, ydim) - (160); // Widescreen offset
    vec2_t const dim = { 60, 5 };

    rotatesprite_(0, 0, 65536, 0, BETASCREEN, 0, 4, RS_AUTO | RS_TOPLEFT | RS_STRETCH | RS_TRANS1 | RS_TRANS2, 0, 0, get_xdim(xpos + x_offset), get_ydim(ypos), get_xdim(xpos + x_offset + dim.x), get_ydim(ypos + dim.y));
    G_ScreenText(MINIFONT, xpos + 1, ypos, 65536, 0, 0, text, -127, pal, RS_AUTO | RS_NOCLIP | RS_TOPLEFT | RS_ALIGN_R, 0, 3, 5, 1, 1, TEXT_UPPERCASE, 0, 0, xdim, ydim);

    Bsprintf(tempbuf, "%d", value);
    G_ScreenText(MINIFONT, xpos + (dim.x - 1), ypos, 65536, 0, 0, tempbuf, -127, pal, RS_AUTO | RS_NOCLIP | RS_TOPLEFT | RS_ALIGN_R, 0, 3, 5, 1, 1, TEXT_UPPERCASE | TEXT_XRIGHT, 0, 0, xdim, ydim);
}

void G_DrawFrags(void)
{
    if(ud.multimode <= 1 || !(GTFLAGS(GAMETYPE_FRAGBAR)) || ud.screen_size <= 0)
        return;

    if (hud_fragbarstyle == 1) // Classic Fragbar
    {
        if (!pus)
        {
            int i = 0;
            bool redraw = false;

            for (ALL_PLAYERS(i))
                if (sbar.frag[i] != g_player[i].ps->frag - g_player[i].ps->fraggedself)
                    redraw = true;

            if (!redraw)
                return;

            for (ALL_PLAYERS(i))
                if (i != myconnectindex)
                    sbar.frag[i] = g_player[i].ps->frag - g_player[i].ps->fraggedself;
        }

        int32_t const xpos = (320 << 15) - (tilesiz[FRAGBAR].x << 15);
        int32_t const numFragbarRows = (playerswhenstarted - 1) >> 2;
        int32_t pNum = 0;

        for (int32_t row = 0; row <= numFragbarRows; row++)
        {
            rotatesprite_fs(xpos, (8 * row) << 16, 65600, 0, FRAGBAR, 0, 0, RS_AUTO | RS_NOCLIP | RS_TOPLEFT | RS_NOMASK);

            for (int32_t column = 0; column < 4; column++, pNum++)
            {
                auto const plr = &g_player[pNum];

                if (pNum >= MAXPLAYERS) break;
                if (!plr->connected) continue;

                // Name
                minitext(21 + (73 * column), 2 + (8 * row), &plr->user_name[0], plr->ps->palookup, RS_AUTO | RS_NOCLIP | RS_TOPLEFT);

                // Frag Count
                Bsprintf(tempbuf, "%d", plr->ps->frag - plr->ps->fraggedself);
                minitext(67 + (73 * column), 2 + (8 * row), tempbuf, plr->ps->palookup, RS_AUTO | RS_NOCLIP | RS_TOPLEFT);
            }
        }
    }
    else if (hud_fragbarstyle == 2) // StrikerDM Fraglist
    {
        int32_t ypos = 8;

        if (GTFLAGS(GAMETYPE_TDM))
        {
            for (int32_t team = 0; team < numTeams; team++)
            {
                int32_t teamMembers[MAXPLAYERS];
                int32_t numTeamMembers = 0;
                for (int32_t pNum = 0; pNum < MAXPLAYERS; pNum++)
                {
                    auto const plr = &g_player[pNum];
                    if (!plr->connected)
                        continue;

                    if (plr->pteam == team)
                    {
                        teamMembers[numTeamMembers++] = pNum;
                    }
                }

                if (numTeamMembers > 0)
                {
                    G_DrawFragbarCell(teams[team].name, teams[team].palnum, ypos, teams[team].frags - teams[team].suicides);
                    ypos += 6;

                    for (int32_t idx = 0; idx < numTeamMembers; idx++)
                    {
                        auto const plr = &g_player[teamMembers[idx]];
                        G_DrawFragbarCell(plr->user_name, plr->ps->palookup, ypos, plr->ps->frag - plr->ps->fraggedself);
                        ypos += 6;
                    }
                }
            }
        }
        else
        {
            for (int32_t pNum = 0; pNum < MAXPLAYERS; pNum++)
            {
                auto const plr = &g_player[pNum];
                if (!plr->connected)
                    continue;

                G_DrawFragbarCell(plr->user_name, plr->ps->palookup, ypos, plr->ps->frag - plr->ps->fraggedself);

                ypos += 6;
            }
        }
    }
}

void G_DrawInventory(DukePlayer_t* p)
{
    int items_had = 0;
    int xoff = 160;
    int y = 154;

    for (int i = 1; i < ICON_MAX; i++)
    {
        int const inv_item = icon_to_inv[i];
        if (inv_item != -1)
        {
            int const inv_amount = p->inv_amount[inv_item];
            if (inv_amount > 0)
            {
                xoff -= 11;
                items_had |= 1 << i;
            }
        }
    } 

    for (int i = 1; i < ICON_MAX; i++)
    {
        if (items_had & (1 << i))
        {
            switch (i)
            {
                case ICON_FIRSTAID:
                    rotatesprite(xoff << 16, y << 16, 65536L, 0, FIRSTAID_ICON, 0, 0, 2 + 16, windowxy1.x, windowxy1.y, windowxy2.x, windowxy2.y);
                    break;
                case ICON_STEROIDS:
                    rotatesprite((xoff + 1) << 16, y << 16, 65536L, 0, STEROIDS_ICON, 0, 0, 2 + 16, windowxy1.x, windowxy1.y, windowxy2.x, windowxy2.y);
                    break;
                case ICON_HOLODUKE:
                    rotatesprite((xoff + 2) << 16, y << 16, 65536L, 0, HOLODUKE_ICON, 0, 0, 2 + 16, windowxy1.x, windowxy1.y, windowxy2.x, windowxy2.y);
                    break;
                case ICON_JETPACK:
                    rotatesprite(xoff << 16, y << 16, 65536L, 0, JETPACK_ICON, 0, 0, 2 + 16, windowxy1.x, windowxy1.y, windowxy2.x, windowxy2.y);
                    break;
                case ICON_HEATS:
                    rotatesprite(xoff << 16, y << 16, 65536L, 0, HEAT_ICON, 0, 0, 2 + 16, windowxy1.x, windowxy1.y, windowxy2.x, windowxy2.y);
                    break;
                case ICON_SCUBA:
                    rotatesprite(xoff << 16, y << 16, 65536L, 0, AIRTANK_ICON, 0, 0, 2 + 16, windowxy1.x, windowxy1.y, windowxy2.x, windowxy2.y);
                    break;
                case ICON_BOOTS:
                    rotatesprite(xoff << 16, (y - 1) << 16, 65536L, 0, BOOT_ICON, 0, 0, 2 + 16, windowxy1.x, windowxy1.y, windowxy2.x, windowxy2.y);
                    break;
            }

            xoff += 22;

            if (p->inven_icon == i)
                rotatesprite((xoff - 2) << 16, (y + 19) << 16, 65536L, 1024, ARROW, -32, 0, 2 + 16, windowxy1.x, windowxy1.y, windowxy2.x, windowxy2.y);
        }
    }
}

// Draws inventory numbers in the HUD for both the full and mini status bars.
static void G_DrawInvNum(int32_t x, int32_t y, char num1, char ha, int32_t sbits)
{
    char dabuf[16];
    int32_t i, shd = (x < 0);

    const int32_t sbscale = sbarsc(65536);
    const int32_t sby = sbary(y), sbyp1 = sbary(y + 1);

    if (shd) x = -x;

    Bsprintf(dabuf, "%d", num1);

    if (num1 > 99)
    {
        if (shd && ud.screen_size == 4 && videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
        {
            for (i = 0; i <= 2; i++)
                rotatesprite_fs(sbarx(x + (-4 + 4 * i) + 1), sbyp1, sbscale, 0, THREEBYFIVE + dabuf[i] - '0',
                    127, 4, POLYMOSTTRANS | sbits);
        }

        for (i = 0; i <= 2; i++)
            rotatesprite_fs(sbarx(x + (-4 + 4 * i)), sby, sbscale, 0, THREEBYFIVE + dabuf[i] - '0', ha, 0, sbits);
        return;
    }

    if (num1 > 9)
    {
        if (shd && ud.screen_size == 4 && videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
        {
            rotatesprite_fs(sbarx(x + 1), sbyp1, sbscale, 0, THREEBYFIVE + dabuf[0] - '0', 127, 4, POLYMOSTTRANS | sbits);
            rotatesprite_fs(sbarx(x + 4 + 1), sbyp1, sbscale, 0, THREEBYFIVE + dabuf[1] - '0', 127, 4, POLYMOSTTRANS | sbits);
        }

        rotatesprite_fs(sbarx(x), sby, sbscale, 0, THREEBYFIVE + dabuf[0] - '0', ha, 0, sbits);
        rotatesprite_fs(sbarx(x + 4), sby, sbscale, 0, THREEBYFIVE + dabuf[1] - '0', ha, 0, sbits);
        return;
    }

    rotatesprite_fs(sbarx(x + 4 + 1), sbyp1, sbscale, 0, THREEBYFIVE + dabuf[0] - '0', ha, 4, sbits);
    rotatesprite_fs(sbarx(x + 4), sby, sbscale, 0, THREEBYFIVE + dabuf[0] - '0', ha, 0, sbits);
}


static void G_DrawDigiNum(int x, int y, int n, char s, int32_t cs)
{
    int i, j = 0, k, p, c;
    char b[10];

    Bsnprintf(b, 10, "%d", n);
    i = Bstrlen(b);

    for (k = i - 1; k >= 0; k--)
    {
        p = DIGITALNUM + *(b + k) - '0';
        j += tilesiz[p].x + 1;
    }
    c = x - (j >> 1);

    j = 0;
    for (k = 0; k < i; k++)
    {
        p = DIGITALNUM + *(b + k) - '0';
        rotatesprite(sbarx(c + j), sbary(y), sbarsc(65536L), 0, p, s, 0, cs, 0, 0, xdim - 1, ydim - 1);
        j += tilesiz[p].x + 1;
    }
}

static void G_DrawAltDigiNum(int x, int y, int n, char s, int32_t cs)
{
    int i, j = 0, k, p, c;
    char b[10];
    int rev = (x < 0);
    int shd = (y < 0);

    if (rev) x = -x;
    if (shd) y = -y;

    Bsnprintf(b, 10, "%d", n);
    i = Bstrlen(b);

    for (k = i - 1; k >= 0; k--)
    {
        p = althud_numbertile + *(b + k) - '0';
        j += tilesiz[p].x + 1;
    }
    c = x - (j >> 1);

    if (rev)
    {
        //        j = 0;
        for (k = 0; k < i; k++)
        {
            p = althud_numbertile + *(b + k) - '0';
            if (shd && videoGetRenderMode() >= 3 && althud_shadows)
                rotatesprite(sbarxr(c + j - 1), sbary(y + 1), sbarsc(65536L), 0, p, s, 4, cs | POLYMOSTTRANS2, 0, 0, xdim - 1, ydim - 1);
            rotatesprite(sbarxr(c + j), sbary(y), sbarsc(65536L), 0, p, s, althud_numberpal, cs, 0, 0, xdim - 1, ydim - 1);
            j -= tilesiz[p].x + 1;
        }
        return;
    }
    j = 0;
    for (k = 0; k < i; k++)
    {
        p = althud_numbertile + *(b + k) - '0';
        if (shd && videoGetRenderMode() >= 3 && althud_shadows)
            rotatesprite(sbarx(c + j + 1), sbary(y + 1), sbarsc(65536L), 0, p, s, 4, cs | POLYMOSTTRANS2, 0, 0, xdim - 1, ydim - 1);
        rotatesprite(sbarx(c + j), sbary(y), sbarsc(65536L), 0, p, s, althud_numberpal, cs, 0, 0, xdim - 1, ydim - 1);
        j += tilesiz[p].x + 1;
    }
}

static void G_DrawWeapNum(short ind, int x, int y, int num1, int num2, char ha)
{
    char dabuf[80] = { 0 };

    rotatesprite(sbarx(x - 7), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + ind + 1, ha - 10, 7, 10, 0, 0, xdim - 1, ydim - 1);
    rotatesprite(sbarx(x - 3), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + 10, ha, 0, 10, 0, 0, xdim - 1, ydim - 1);

    if (VOLUMEONE && (ind > HANDBOMB_WEAPON || ind < 0))
    {
        minitextshade(x + 1, y - 4, "ORDER", 20, 11, 2 + 8 + 16 + 256);
        return;
    }

    rotatesprite(sbarx(x + 9), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + 11, ha, 0, 10, 0, 0, xdim - 1, ydim - 1);

    if (num1 > 99) num1 = 99;
    if (num2 > 99) num2 = 99;

    Bsprintf(dabuf, "%d", num1);
    if (num1 > 9)
    {
        rotatesprite(sbarx(x), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + dabuf[0] - '0', ha, 0, 10, 0, 0, xdim - 1, ydim - 1);
        rotatesprite(sbarx(x + 4), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + dabuf[1] - '0', ha, 0, 10, 0, 0, xdim - 1, ydim - 1);
    }
    else rotatesprite(sbarx(x + 4), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + dabuf[0] - '0', ha, 0, 10, 0, 0, xdim - 1, ydim - 1);

    Bsprintf(dabuf, "%d", num2);
    if (num2 > 9)
    {
        rotatesprite(sbarx(x + 13), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + dabuf[0] - '0', ha, 0, 10, 0, 0, xdim - 1, ydim - 1);
        rotatesprite(sbarx(x + 17), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + dabuf[1] - '0', ha, 0, 10, 0, 0, xdim - 1, ydim - 1);
        return;
    }
    rotatesprite(sbarx(x + 13), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + dabuf[0] - '0', ha, 0, 10, 0, 0, xdim - 1, ydim - 1);
}

static void G_DrawWeapNum2(char ind, int x, int y, int num1, int num2, char ha)
{
    char dabuf[80] = { 0 };

    rotatesprite(sbarx(x - 7), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + ind + 1, ha - 10, 7, 10, 0, 0, xdim - 1, ydim - 1);
    rotatesprite(sbarx(x - 4), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + 10, ha, 0, 10, 0, 0, xdim - 1, ydim - 1);
    rotatesprite(sbarx(x + 13), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + 11, ha, 0, 10, 0, 0, xdim - 1, ydim - 1);

    Bsprintf(dabuf, "%d", num1);
    if (num1 > 99)
    {
        rotatesprite(sbarx(x), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + dabuf[0] - '0', ha, 0, 10, 0, 0, xdim - 1, ydim - 1);
        rotatesprite(sbarx(x + 4), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + dabuf[1] - '0', ha, 0, 10, 0, 0, xdim - 1, ydim - 1);
        rotatesprite(sbarx(x + 8), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + dabuf[2] - '0', ha, 0, 10, 0, 0, xdim - 1, ydim - 1);
    }
    else if (num1 > 9)
    {
        rotatesprite(sbarx(x + 4), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + dabuf[0] - '0', ha, 0, 10, 0, 0, xdim - 1, ydim - 1);
        rotatesprite(sbarx(x + 8), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + dabuf[1] - '0', ha, 0, 10, 0, 0, xdim - 1, ydim - 1);
    }
    else rotatesprite(sbarx(x + 8), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + dabuf[0] - '0', ha, 0, 10, 0, 0, xdim - 1, ydim - 1);

    Bsprintf(dabuf, "%d", num2);
    if (num2 > 99)
    {
        rotatesprite(sbarx(x + 17), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + dabuf[0] - '0', ha, 0, 10, 0, 0, xdim - 1, ydim - 1);
        rotatesprite(sbarx(x + 21), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + dabuf[1] - '0', ha, 0, 10, 0, 0, xdim - 1, ydim - 1);
        rotatesprite(sbarx(x + 25), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + dabuf[2] - '0', ha, 0, 10, 0, 0, xdim - 1, ydim - 1);
    }
    else if (num2 > 9)
    {
        rotatesprite(sbarx(x + 17), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + dabuf[0] - '0', ha, 0, 10, 0, 0, xdim - 1, ydim - 1);
        rotatesprite(sbarx(x + 21), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + dabuf[1] - '0', ha, 0, 10, 0, 0, xdim - 1, ydim - 1);
        return;
    }
    else
        rotatesprite(sbarx(x + 25), sbary(y), sbarsc(65536L), 0, THREEBYFIVE + dabuf[0] - '0', ha, 0, 10, 0, 0, xdim - 1, ydim - 1);
}

static void G_DrawWeapAmounts(DukePlayer_t* p, int x, int y, int u)
{
    int cw = p->curr_weapon;

    if (u & 4)
    {
        if (u != -1) G_PatchStatusBar(88, 178, 88 + 37, 178 + 6); //original code: (96,178,96+12,178+6);
        G_DrawWeapNum2(PISTOL_WEAPON, x, y,
            p->ammo_amount[PISTOL_WEAPON], p->max_ammo_amount[PISTOL_WEAPON],
            12 - 20 * (cw == PISTOL_WEAPON));
    }
    if (u & 8)
    {
        if (u != -1) G_PatchStatusBar(88, 184, 88 + 37, 184 + 6); //original code: (96,184,96+12,184+6);
        G_DrawWeapNum2(SHOTGUN_WEAPON, x, y + 6,
            p->ammo_amount[SHOTGUN_WEAPON], p->max_ammo_amount[SHOTGUN_WEAPON],
            (!p->gotweapon[SHOTGUN_WEAPON] * 9) + 12 - 18 *
            (cw == SHOTGUN_WEAPON));
    }
    if (u & 16)
    {
        if (u != -1) G_PatchStatusBar(88, 190, 88 + 37, 190 + 6); //original code: (96,190,96+12,190+6);
        G_DrawWeapNum2(CHAINGUN_WEAPON, x, y + 12,
            p->ammo_amount[CHAINGUN_WEAPON], p->max_ammo_amount[CHAINGUN_WEAPON],
            (!p->gotweapon[CHAINGUN_WEAPON] * 9) + 12 - 18 *
            (cw == CHAINGUN_WEAPON));
    }
    if (u & 32)
    {
        if (u != -1) G_PatchStatusBar(127, 178, 127 + 29, 178 + 6); //original code: (135,178,135+8,178+6);
        G_DrawWeapNum(RPG_WEAPON, x + 39, y,
            p->ammo_amount[RPG_WEAPON], p->max_ammo_amount[RPG_WEAPON],
            (!p->gotweapon[RPG_WEAPON] * 9) + 12 - 19 *
            (cw == RPG_WEAPON));
    }
    if (u & 64)
    {
        if (u != -1) G_PatchStatusBar(127, 184, 127 + 29, 184 + 6); //original code: (135,184,135+8,184+6);
        G_DrawWeapNum(HANDBOMB_WEAPON, x + 39, y + 6,
            p->ammo_amount[HANDBOMB_WEAPON], p->max_ammo_amount[HANDBOMB_WEAPON],
            (((!p->ammo_amount[HANDBOMB_WEAPON]) | (!p->gotweapon[HANDBOMB_WEAPON])) * 9) + 12 - 19 *
            ((cw == HANDBOMB_WEAPON) || (cw == HANDREMOTE_WEAPON)));
    }
    if (u & 128)
    {
        if (u != -1) G_PatchStatusBar(127, 190, 127 + 29, 190 + 6); //original code: (135,190,135+8,190+6);

        if (p->subweapon & (1 << GROW_WEAPON))
            G_DrawWeapNum(SHRINKER_WEAPON, x + 39, y + 12,
                p->ammo_amount[GROW_WEAPON], p->max_ammo_amount[GROW_WEAPON],
                (!p->gotweapon[GROW_WEAPON] * 9) + 12 - 18 *
                (cw == GROW_WEAPON));
        else
            G_DrawWeapNum(SHRINKER_WEAPON, x + 39, y + 12,
                p->ammo_amount[SHRINKER_WEAPON], p->max_ammo_amount[SHRINKER_WEAPON],
                (!p->gotweapon[SHRINKER_WEAPON] * 9) + 12 - 18 *
                (cw == SHRINKER_WEAPON));
    }
    if (u & 256)
    {
        if (u != -1) G_PatchStatusBar(158, 178, 162 + 29, 178 + 6); //original code: (166,178,166+8,178+6);

        G_DrawWeapNum(DEVISTATOR_WEAPON, x + 70, y,
            p->ammo_amount[DEVISTATOR_WEAPON], p->max_ammo_amount[DEVISTATOR_WEAPON],
            (!p->gotweapon[DEVISTATOR_WEAPON] * 9) + 12 - 18 *
            (cw == DEVISTATOR_WEAPON));
    }
    if (u & 512)
    {
        if (u != -1) G_PatchStatusBar(158, 184, 162 + 29, 184 + 6); //original code: (166,184,166+8,184+6);

        G_DrawWeapNum(TRIPBOMB_WEAPON, x + 70, y + 6,
            p->ammo_amount[TRIPBOMB_WEAPON], p->max_ammo_amount[TRIPBOMB_WEAPON],
            (!p->gotweapon[TRIPBOMB_WEAPON] * 9) + 12 - 18 *
            (cw == TRIPBOMB_WEAPON));
    }

    if (u & 65536L)
    {
        if (u != -1) G_PatchStatusBar(158, 190, 162 + 29, 190 + 6); //original code: (166,190,166+8,190+6);

        G_DrawWeapNum(-1, x + 70, y + 12,
            p->ammo_amount[FREEZE_WEAPON], p->max_ammo_amount[FREEZE_WEAPON],
            (!p->gotweapon[FREEZE_WEAPON] * 9) + 12 - 18 *
            (cw == FREEZE_WEAPON));
    }
}

static int32_t G_GetInvAmount(const DukePlayer_t* p)
{
    switch (p->inven_icon)
    {
        case ICON_FIRSTAID:
            return p->inv_amount[GET_FIRSTAID];
        case ICON_STEROIDS:
            return (p->inv_amount[GET_STEROIDS] + 3) >> 2;
        case ICON_HOLODUKE:
            return (p->inv_amount[GET_HOLODUKE] + 15) / 24;
        case ICON_JETPACK:
            return (p->inv_amount[GET_JETPACK] + 15) >> 4;
        case ICON_HEATS:
            return p->inv_amount[GET_HEATS] / 12;
        case ICON_SCUBA:
            return (p->inv_amount[GET_SCUBA] + 63) >> 6;
        case ICON_BOOTS:
            return p->inv_amount[GET_BOOTS] >> 1;
    }

    return -1;
}

static int32_t G_GetInvOn(const DukePlayer_t* p)
{
    switch (p->inven_icon)
    {
        case ICON_HOLODUKE:
            return p->holoduke_on;
        case ICON_JETPACK:
            return p->jetpack_on;
        case ICON_HEATS:
            return p->heat_on;
    }

    return 0x80000000;
}


static int32_t G_GetMorale(int32_t p_i, int32_t snum)
{
    return Gv_GetVarByLabel("PLR_MORALE", -1, p_i, snum);
}

static inline void rotatesprite_althud(int32_t sx, int32_t sy, int32_t z, int16_t a, int16_t picnum, int8_t dashade, char dapalnum, int32_t dastat)
{
    if (videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
        rotatesprite_(sbarx(sx + 1), sbary(sy + 1), z, a, picnum, 127, 4, dastat + POLYMOSTTRANS2, 0, 0, 0, 0, xdim - 1, ydim - 1);
    rotatesprite_(sbarx(sx), sbary(sy), z, a, picnum, dashade, dapalnum, dastat, 0, 0, 0, 0, xdim - 1, ydim - 1);
}

static inline void rotatesprite_althudr(int32_t sx, int32_t sy, int32_t z, int16_t a, int16_t picnum, int8_t dashade, char dapalnum, int32_t dastat)
{
    if (videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
        rotatesprite_(sbarxr(sx - 1), sbary(sy + 1), z, a, picnum, 127, 4, dastat + POLYMOSTTRANS2, 0, 0, 0, 0, xdim - 1, ydim - 1);
    rotatesprite_(sbarxr(sx), sbary(sy), z, a, picnum, dashade, dapalnum, dastat, 0, 0, 0, 0, xdim - 1, ydim - 1);
}

void G_DrawStatusBar(int snum)
{
    auto const p = g_player[snum].ps;
    int32_t i, j, o;
    int32_t permbit = 0;

    const int32_t ss = ud.screen_size;
    const int32_t althud = ud.althud;

    const int32_t SBY = (200 - tilesiz[BOTTOMSTATUSBAR].y);

    const int32_t sb15 = sbarsc(32768), sb15h = sbarsc(49152);
    const int32_t sb16 = sbarsc(65536);

    static int32_t item_icons[8];

    if (ss < 4)
        return;

    if (item_icons[0] == 0)
    {
        int32_t iicons[8] = { -1, FIRSTAID_ICON, STEROIDS_ICON, HOLODUKE_ICON,
                              JETPACK_ICON, HEAT_ICON, AIRTANK_ICON, BOOT_ICON };
        Bmemcpy(item_icons, iicons, sizeof(item_icons));
    }

    if (ud.gm & MODE_MENU)
        if ((g_currentMenu >= 400 && g_currentMenu <= 405))
            return;

    if (videoGetRenderMode() >= REND_POLYMOST)
        pus = NUMPAGES;   // JBF 20040101: always redraw in GL

    if (ss == 4) // DRAW MINI STATUS BAR:
    {
        if (althud) // althud
        {
            int32_t const hudoffset = 200;
            static int ammo_sprites[MAX_WEAPONS];

            if (EDUKE32_PREDICT_FALSE(ammo_sprites[0] == 0))
            {
                /* this looks stupid but it lets us initialize static memory to dynamic values
                   these values can be changed from the CONs with dynamic tile remapping
                   but we don't want to have to recreate the values in memory every time
                   the HUD is drawn */

                int asprites[MAX_WEAPONS] = { BOOTS, AMMO, SHOTGUNAMMO,
                                              BATTERYAMMO, RPGAMMO, HBOMBAMMO, CRYSTALAMMO, DEVISTATORAMMO,
                                              TRIPBOMBSPRITE, FREEZEAMMO + 1, HBOMBAMMO, GROWAMMO };

                Bmemcpy(ammo_sprites, asprites, sizeof(ammo_sprites));
            }

            rotatesprite_althud(2, hudoffset - 21, sb15h, 0, COLA, 0, 0, 10 + 16 + 256);

            if (sprite[p->i].pal == 1 && p->last_extra < 2)
                G_DrawAltDigiNum(40, -(hudoffset - 22), 1, -16, 10 + 16 + 256);
            else if (!althud_flashing || p->last_extra > (p->max_player_health >> 2) || (int32_t)totalclock & 32)
            {
                int32_t s = -8;
                if (althud_flashing && p->last_extra > p->max_player_health)
                    s += (sintable[((int32_t)totalclock << 5) & 2047] >> 10);
                G_DrawAltDigiNum(40, -(hudoffset - 22), p->last_extra, s, 10 + 16 + 256);
            }

            rotatesprite_althud(62, hudoffset - 25, sb15h, 0, SHIELD, 0, 0, 10 + 16 + 256);

            {
                int32_t lAmount = G_GetMorale(p->i, snum);
                if (lAmount == -1)
                    lAmount = p->inv_amount[GET_SHIELD];
                G_DrawAltDigiNum(105, -(hudoffset - 22), lAmount, -16, 10 + 16 + 256);
            }

            if (ammo_sprites[p->curr_weapon] >= 0)
            {
                i = (tilesiz[ammo_sprites[p->curr_weapon]].y >= 50) ? 16384 : 32768;
                rotatesprite_althudr(57, hudoffset - 15, sbarsc(i), 0, ammo_sprites[p->curr_weapon], 0, 0, 10 + 512);
            }

            if (PWEAPON(snum, p->curr_weapon, WorksLike) == HANDREMOTE_WEAPON) i = HANDBOMB_WEAPON;
            else i = p->curr_weapon;

            if (PWEAPON(snum, p->curr_weapon, WorksLike) != KNEE_WEAPON &&
                (!althud_flashing || (int32_t)totalclock & 32 || p->ammo_amount[i] > (p->max_ammo_amount[i] / 10)))
                G_DrawAltDigiNum(-20, -(hudoffset - 22), p->ammo_amount[i], -16, 10 + 16 + 512);

            o = 102;
            permbit = 0;

            if (p->inven_icon)
            {
                const int32_t orient = 10 + 16 + permbit + 256;

                int const inv_icon = ((unsigned)p->inven_icon < ICON_MAX) ? item_icons[p->inven_icon] : -1;
                int const inv_amount = G_GetInvAmount(p);
                int const inv_on = G_GetInvOn(p);

                if (inv_icon >= 0)
                    rotatesprite_althud(231 - o, hudoffset - 21 - 2, sb16, 0, inv_icon, 0, 0, orient);

                if (videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
                    minitextshade(292 - 30 - o + 1, hudoffset - 10 - 3 + 1, "%", 127, 4, POLYMOSTTRANS + orient + ROTATESPRITE_MAX);
                minitext(292 - 30 - o, hudoffset - 10 - 3, "%", 6, orient + ROTATESPRITE_MAX);

                G_DrawInvNum(-(284 - 30 - o), hudoffset - 6 - 3, (uint8_t)inv_amount, 0, 10 + permbit + 256);

                if (inv_on > 0)
                {
                    if (videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
                        minitextshade(288 - 30 - o + 1, hudoffset - 20 - 3 + 1, "On", 127, 4, POLYMOSTTRANS + orient + ROTATESPRITE_MAX);
                    minitext(288 - 30 - o, hudoffset - 20 - 3, "On", 0, orient + ROTATESPRITE_MAX);
                }
                else if ((uint32_t)inv_on != 0x80000000)
                {
                    if (videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
                        minitextshade(284 - 30 - o + 1, hudoffset - 20 - 3 + 1, "Off", 127, 4, POLYMOSTTRANS + orient + ROTATESPRITE_MAX);
                    minitext(284 - 30 - o, hudoffset - 20 - 3, "Off", 2, orient + ROTATESPRITE_MAX);
                }

                if (p->inven_icon >= ICON_SCUBA)
                {
                    if (videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
                        minitextshade(284 - 35 - o + 1, hudoffset - 20 - 3 + 1, "Auto", 127, 4, POLYMOSTTRANS + orient + ROTATESPRITE_MAX);
                    minitext(284 - 35 - o, hudoffset - 20 - 3, "Auto", 2, orient + ROTATESPRITE_MAX);
                }
            }

            if (ud.got_access & 1) rotatesprite_althudr(39, hudoffset - 43, sb15, 0, ACCESSCARD, 0, 0, 10 + 16 + 512);
            if (ud.got_access & 4) rotatesprite_althudr(34, hudoffset - 41, sb15, 0, ACCESSCARD, 0, 23, 10 + 16 + 512);
            if (ud.got_access & 2) rotatesprite_althudr(29, hudoffset - 39, sb15, 0, ACCESSCARD, 0, 21, 10 + 16 + 512);
        }
        else
        {
            // ORIGINAL MINI STATUS BAR
            int32_t orient = 2 + 8 + 16 + 256;

            rotatesprite_fs(sbarx(5), sbary(200 - 28), sb16, 0, HEALTHBOX, 0, 21, orient);
            if (p->inven_icon)
                rotatesprite_fs(sbarx(69), sbary(200 - 30), sb16, 0, INVENTORYBOX, 0, 21, orient);

            // health
            {
                int32_t health = (sprite[p->i].pal == 1 && p->last_extra < 2) ? 1 : p->last_extra;
                G_DrawDigiNum(20, 200 - 17, health, -16, orient);
            }

            rotatesprite_fs(sbarx(37), sbary(200 - 28), sb16, 0, AMMOBOX, 0, 21, orient);

            if (PWEAPON(snum, p->curr_weapon, WorksLike) == HANDREMOTE_WEAPON)
                i = HANDBOMB_WEAPON;
            else
                i = p->curr_weapon;
            G_DrawDigiNum(53, 200 - 17, p->ammo_amount[i], -16, orient);

            o = 158;
            permbit = 0;
            if (p->inven_icon)
            {
                i = ((unsigned)p->inven_icon < ICON_MAX) ? item_icons[p->inven_icon] : -1;
                if (i >= 0)
                    rotatesprite_fs(sbarx(231 - o), sbary(200 - 21), sb16, 0, i, 0, 0, orient);

                // scale by status bar size
                orient |= ROTATESPRITE_MAX;

                minitext(292 - 30 - o, 190, "%", 6, orient);

                i = G_GetInvAmount(p);
                j = G_GetInvOn(p);

                G_DrawInvNum(284 - 30 - o, 200 - 6, (uint8_t)i, 0, orient & ~16);

                if (!WW2GI)
                {
                    if (j > 0)
                        minitext(288 - 30 - o, 180, "On", 0, orient);
                    else if ((uint32_t)j != 0x80000000)
                        minitext(284 - 30 - o, 180, "Off", 2, orient);
                }

                if (p->inven_icon >= ICON_SCUBA)
                    minitext(284 - 35 - o, 180, "Auto", 2, orient);
            }
        }
        return;
    }

    //DRAW/UPDATE FULL STATUS BAR:
    enum updateType
    {
        UPD_NONE        = 0,
        UPD_HEALTH      = (1 << 0), // update health
        UPD_ARMOR       = (1 << 1), // update armor
        UPD_PISTOL      = (1 << 2), // update PISTOL_WEAPON ammo
        UPD_SHOTGUN     = (1 << 3), // update SHOTGUN_WEAPON ammo
        UPD_CHAINGUN    = (1 << 4), // update CHAINGUN_WEAPON ammo
        UPD_RPG         = (1 << 5), // update RPG_WEAPON ammo
        UPD_HANDBOMB    = (1 << 6), // update HANDBOMB_WEAPON ammo
        UPD_SHRINKER    = (1 << 7), // update SHRINKER_WEAPON ammo
        UPD_DEVISTATOR  = (1 << 8), // update DEVISTATOR_WEAPON ammo
        UPD_TRIPBOMB    = (1 << 9), // update TRIPBOMB_WEAPON ammo
        UPD_AMMO        = (1 << 10), // update ammo display
        UPD_INVICON     = (1 << 11), // update inventory icon
        UPD_INVONOFF    = (1 << 12), // update inventory on/off
        UPD_INVPERCENT  = (1 << 13), // update inventory %
        UPD_KEYS        = (1 << 14), // update keys
        UPD_KILLS       = (1 << 15), // update kills
        UPD_FREEZE      = (1 << 16), // update FREEZE_WEAPON ammo
        UPD_WEAPONS     = (UPD_PISTOL | UPD_SHOTGUN | UPD_CHAINGUN | UPD_RPG | UPD_HANDBOMB | UPD_SHRINKER | UPD_DEVISTATOR | UPD_TRIPBOMB | UPD_AMMO | UPD_FREEZE),
        UPD_ALL         = 0xFFFFFFFF,
    };

    uint32_t updateBits = UPD_NONE;

    if (pus)
    {
        pus = 0;
        updateBits = UPD_ALL;
    }

    if (sbar.frag[myconnectindex] != p->frag - p->fraggedself)
    {
        sbar.frag[myconnectindex] = p->frag - p->fraggedself;
        updateBits |= UPD_KILLS;
    }
    if (sbar.got_access != ud.got_access)
    {
        sbar.got_access = ud.got_access;
        updateBits |= UPD_KEYS;
    }

    if (sbar.last_extra != p->last_extra)
    {
        sbar.last_extra = p->last_extra;
        updateBits |= UPD_HEALTH;
    }

    {
        int32_t lAmount = G_GetMorale(p->i, snum);
        if (lAmount == -1)
            lAmount = p->inv_amount[GET_SHIELD];
        if (sbar.inv_amount[GET_SHIELD] != lAmount)
        {
            sbar.inv_amount[GET_SHIELD] = lAmount;
            updateBits |= UPD_ARMOR;
        }
    }

    if (sbar.curr_weapon != p->curr_weapon)
    {
        sbar.curr_weapon = p->curr_weapon;
        
        updateBits |= UPD_AMMO | UPD_WEAPONS;
    }

    for (i = 1; i < MAX_WEAPONS; i++)
    {
        if (sbar.ammo_amount[i] != p->ammo_amount[i])
        {
            sbar.ammo_amount[i] = p->ammo_amount[i];
            if (i < 9)
                updateBits |= ((2 << i) | UPD_AMMO);
            else if (WW2GI && i == 11)
                updateBits |= (UPD_SHRINKER | UPD_AMMO);
            else
                updateBits |= (UPD_AMMO | UPD_FREEZE);
        }

        if ((sbar.gotweapon[i]) != (p->gotweapon[i]))
        {
            sbar.gotweapon[i] = p->gotweapon[i];

            if (i < 9)
                updateBits |= ((2 << i) | UPD_AMMO);
            else
                updateBits |= (UPD_AMMO | UPD_FREEZE);
        }
    }

    if (sbar.inven_icon != p->inven_icon)
    {
        sbar.inven_icon = p->inven_icon;
        updateBits |= (UPD_INVICON | UPD_INVONOFF | UPD_INVPERCENT);
    }
    if (sbar.holoduke_on != p->holoduke_on)
    {
        sbar.holoduke_on = p->holoduke_on;
        updateBits |= (UPD_INVONOFF | UPD_INVPERCENT);
    }
    if (sbar.jetpack_on != p->jetpack_on)
    {
        sbar.jetpack_on = p->jetpack_on;
        updateBits |= (UPD_INVONOFF | UPD_INVPERCENT);
    }
    if (sbar.heat_on != p->heat_on)
    {
        sbar.heat_on = p->heat_on;
        updateBits |= (UPD_INVONOFF | UPD_INVPERCENT);
    }

    for (i = 1; i < ICON_MAX; i++)
    {
        int32_t const item = icon_to_inv[i];

        if (sbar.inv_amount[item] != p->inv_amount[item])
        {
            sbar.inv_amount[item] = p->inv_amount[item];
            updateBits |= UPD_INVPERCENT;
        }
    }

#if USE_PATCHING
    if (updateBits == 0)
        return;
#else
    // Patching can sometimes leave stray pixels behind at certain status bar scales.
    // Turn it off if you wish to avoid this. It is, however, slower.
    updateBits = UPD_ALL;
#endif

    if (updateBits == UPD_ALL)
    {
        G_PatchStatusBar(0, 0, 320, 200);
        if ((ud.multimode > 1) && GTFLAGS(GAMETYPE_FRAGBAR))
            rotatesprite_fs(sbarx(277 + 1), sbary(SBY + 7 - 1), sb16, 0, KILLSICON, 0, 0, 10 + 16);
    }

    if ((ud.multimode > 1) && GTFLAGS(GAMETYPE_FRAGBAR))
    {
        if (updateBits & UPD_KILLS)
        {
            if (updateBits != UPD_ALL) G_PatchStatusBar(276, SBY + 17, 299, SBY + 17 + 10);
            G_DrawDigiNum(287, SBY + 17, max(p->frag - p->fraggedself, 0), -16, 10 + 16);
        }
    }
    else
    {
        if (updateBits & UPD_KEYS)
        {
            if (updateBits != UPD_ALL) G_PatchStatusBar(275, SBY + 18, 299, SBY + 18 + 12);
            if (ud.got_access & 4) rotatesprite_fs(sbarx(275), sbary(SBY + 16), sb16, 0, ACCESS_ICON, 0, 23, 10 + 16);
            if (ud.got_access & 2) rotatesprite_fs(sbarx(288), sbary(SBY + 16), sb16, 0, ACCESS_ICON, 0, 21, 10 + 16);
            if (ud.got_access & 1) rotatesprite_fs(sbarx(281), sbary(SBY + 23), sb16, 0, ACCESS_ICON, 0, 0, 10 + 16);
        }
    }

    if (updateBits & UPD_WEAPONS)
        G_DrawWeapAmounts(p, 96, SBY + 16, updateBits);

    if (updateBits & UPD_HEALTH)
    {
        if (updateBits != UPD_ALL) G_PatchStatusBar(20, SBY + 17, 43, SBY + 17 + 11);
        if (sprite[p->i].pal == 1 && p->last_extra < 2)
            G_DrawDigiNum(32, SBY + 17, 1, -16, 10 + 16);
        else G_DrawDigiNum(32, SBY + 17, p->last_extra, -16, 10 + 16);
    }
    if (updateBits & UPD_ARMOR)
    {
        int32_t lAmount = G_GetMorale(p->i, snum);

        if (updateBits != UPD_ALL)
            G_PatchStatusBar(52, SBY + 17, 75, SBY + 17 + 11);

        if (lAmount == -1)
            G_DrawDigiNum(64, SBY + 17, p->inv_amount[GET_SHIELD], -16, 10 + 16);
        else
            G_DrawDigiNum(64, SBY + 17, lAmount, -16, 10 + 16);
    }

    if (updateBits & UPD_AMMO)
    {
        if (updateBits != UPD_ALL) G_PatchStatusBar(196, SBY + 17, 219, SBY + 17 + 11);
        if (PWEAPON(snum, p->curr_weapon, WorksLike) != KNEE_WEAPON)
        {
            if (PWEAPON(snum, p->curr_weapon, WorksLike) == HANDREMOTE_WEAPON) i = HANDBOMB_WEAPON;
            else i = p->curr_weapon;
            G_DrawDigiNum(230 - 22, SBY + 17, p->ammo_amount[i], -16, 10 + 16);
        }
    }

    if (updateBits & (UPD_INVICON | UPD_INVONOFF | UPD_INVPERCENT))
    {
        if (updateBits != UPD_ALL)
        {
            if (updateBits & (2048 + 4096))
                G_PatchStatusBar(231, SBY + 13, 265, SBY + 13 + 18);
            else
                G_PatchStatusBar(250, SBY + 24, 261, SBY + 24 + 6);
        }

        if (p->inven_icon)
        {
            o = 0;

            if (updateBits & (UPD_INVICON | UPD_INVONOFF))
            {
                i = ((unsigned)p->inven_icon < ICON_MAX) ? item_icons[p->inven_icon] : -1;
                // XXX: i < 0?
                rotatesprite_fs(sbarx(231 - o), sbary(SBY + 13), sb16, 0, i, 0, 0, 10 + 16 + permbit);
                minitext(292 - 30 - o, SBY + 24, "%", 6, 10 + 16 + permbit + ROTATESPRITE_MAX);
                if (p->inven_icon >= ICON_SCUBA) minitext(284 - 35 - o, SBY + 14, "Auto", 2, 10 + 16 + permbit + ROTATESPRITE_MAX);
            }

            if (updateBits & (UPD_INVICON | UPD_INVONOFF) && !WW2GI)
            {
                j = G_GetInvOn(p);

                if (j > 0) minitext(288 - 30 - o, SBY + 14, "On", 0, 10 + 16 + permbit + ROTATESPRITE_MAX);
                else if ((uint32_t)j != 0x80000000) minitext(284 - 30 - o, SBY + 14, "Off", 2, 10 + 16 + permbit + ROTATESPRITE_MAX);
            }

            if (updateBits & UPD_INVPERCENT)
            {
                i = G_GetInvAmount(p);
                G_DrawInvNum(284 - 30 - o, SBY + 28, (uint8_t)i, 0, 10 + permbit);
            }
        }
    }
}

// Crosshair
void G_GetCrosshairColor(void)
{
    if (DefaultCrosshairColors.f)
        return;

    tileLoad(CROSSHAIR);

    if (!waloff[CROSSHAIR])
        return;

    char const* ptr = (char const*)waloff[CROSSHAIR];

    // find the brightest color in the original 8-bit tile
    int32_t ii = tilesiz[CROSSHAIR].x * tilesiz[CROSSHAIR].y;
    int32_t bri = 0, j = 0, i;

    Bassert(ii > 0);

    do
    {
        if (*ptr != 255)
        {
            i = curpalette[(int32_t)*ptr].r + curpalette[(int32_t)*ptr].g + curpalette[(int32_t)*ptr].b;
            if (i > j) { j = i; bri = *ptr; }
        }
        ptr++;
    } while (--ii);

    Bmemcpy(&CrosshairColors, &curpalette[bri], sizeof(palette_t));
    Bmemcpy(&DefaultCrosshairColors, &curpalette[bri], sizeof(palette_t));
    DefaultCrosshairColors.f = 1; // this flag signifies that the color has been detected
}

void G_SetCrosshairColor(int32_t r, int32_t g, int32_t b)
{
    if (g_crosshairSum == r + (g << 8) + (b << 16))
        return;

    tileLoad(CROSSHAIR);

    if (!waloff[CROSSHAIR])
        return;

    if (!DefaultCrosshairColors.f)
        G_GetCrosshairColor();

    g_crosshairSum = r + (g << 8) + (b << 16);
    CrosshairColors.r = r;
    CrosshairColors.g = g;
    CrosshairColors.b = b;

    char* ptr = (char*)waloff[CROSSHAIR];

    int32_t ii = tilesiz[CROSSHAIR].x * tilesiz[CROSSHAIR].y;

    Bassert(ii > 0);

    int32_t i = (videoGetRenderMode() == REND_CLASSIC)
        ? paletteGetClosestColor(CrosshairColors.r, CrosshairColors.g, CrosshairColors.b)
        : paletteGetClosestColor(255, 255, 255);  // use white in GL so we can tint it to the right color

    do
    {
        if (*ptr != 255)
            *ptr = i;
        ptr++;
    } while (--ii);

    paletteMakeLookupTable(CROSSHAIR_PAL, NULL, CrosshairColors.r, CrosshairColors.g, CrosshairColors.b, 1);

#ifdef USE_OPENGL
    // XXX: this makes us also load all hightile textures tinted with the crosshair color!
    polytint_t& crosshairtint = hictinting[CROSSHAIR_PAL];
    crosshairtint.r = CrosshairColors.r;
    crosshairtint.g = CrosshairColors.g;
    crosshairtint.b = CrosshairColors.b;
    crosshairtint.f = HICTINT_USEONART | HICTINT_GRAYSCALE;
#endif
    tileInvalidate(CROSSHAIR, -1, -1);
}

void G_DrawCrosshair(void)
{
    auto pPlayer = g_player[screenpeek].ps;
    if (pPlayer->newowner != -1 || ud.overhead_on || !ud.crosshair || ud.camerasprite != -1)
        return;

    Gv_SetVar(sysVarIDs.RETURN, 0, pPlayer->i, screenpeek);
    X_OnEvent(EVENT_DISPLAYCROSSHAIR, g_player[screenpeek].ps->i, screenpeek, -1);

    if (Gv_GetVar(sysVarIDs.RETURN, pPlayer->i, screenpeek) == 0)
        rotatesprite((160L - (pPlayer->look_ang >> 1)) << 16, 100L << 16, scale(65536, ud.crosshairscale, 100), 0, CROSSHAIR, 0, CROSSHAIR_PAL, 2 + 1, windowxy1.x, windowxy1.y, windowxy2.x, windowxy2.y);
}

// Player IDs
#define ID_ALPHA 64                     // Starting Alpha
#define ID_ALPHA_RANGE (255-ID_ALPHA)   // Alpha Remainder
#define ID_YPOS 145                     // Y position for id text
#define ID_TIME (TICRATE*2)             // Time for id text
#define ID_FADETIME (TICRATE*1)         // Remaining time to start fading id text
#define KILLID_TIME (TICRATE*3)
#define KILLID_FADETIME (TICRATE*2)

#define WITHIN_TIME(starttime, totaltime) ((starttime - (int32_t)totalclock) > 0 && (starttime - (int32_t)totalclock) <= totaltime)

void G_DisplayPlayerId(void)
{
    int idalpha;

    if (ud.gm & MODE_MENU)
        return;
    // Camera data might be invalid here if called right after level transition.
    if (ud.overhead_on == 2 || ud.show_help)
    {
        ud.playerid_time = 0;
        return;
    }

    if (ud.idplayers && (ud.multimode > 1) && !DMFLAGS_TEST(DMFLAG_NOPLAYERIDS))
    {
        // TODO: (Optimization) Consider moving this segment to a point in the code that operates at 120hz.
        int i;

        hitdata_t hit;

        for (ALL_PLAYERS(i))
        {
            if (g_player[i].ps->holoduke_on != -1)
                sprite[g_player[i].ps->holoduke_on].cstat ^= 256;
        }

        hitscan(&ud.camerapos, ud.camerasect,
            sintable[(fix16_to_int(ud.cameraq16ang) + 512) & 2047],
            sintable[fix16_to_int(ud.cameraq16ang) & 2047],
            fix16_to_int(F16(100) - ud.cameraq16horiz) << 11, &hit, 0xffff0030);

        for (ALL_PLAYERS(i))
        {
            if (g_player[i].ps->holoduke_on != -1)
                sprite[g_player[i].ps->holoduke_on].cstat ^= 256;
        }

        if ((hit.sprite >= 0) && sprite[hit.sprite].picnum == APLAYER && sprite[hit.sprite].yvel != screenpeek && g_player[sprite[hit.sprite].yvel].ps->dead_flag == 0)
        {
            if (ldist(&ud.camerapos, &sprite[hit.sprite].xyz) < 9216)
            {
                ud.playerid_num = sprite[hit.sprite].yvel;
                ud.playerid_time = (int32_t)totalclock + ID_TIME;
            }
        }
        // End segment

        if (WITHIN_TIME(ud.playerid_time, ID_TIME))
        {
            idalpha = ID_ALPHA;
            if (WITHIN_TIME(ud.playerid_time, ID_FADETIME))
            {
                idalpha += ID_ALPHA_RANGE - scale(ud.playerid_time - (int32_t)totalclock, ID_ALPHA_RANGE, ID_FADETIME);
            }

            G_ScreenText(STARTALPHANUM, 160, ID_YPOS, 65536, 0, 0, g_player[ud.playerid_num].user_name, -127, g_player[ud.playerid_num].ps->palookup, RS_AUTO | RS_NOCLIP | RS_TOPLEFT, idalpha, 4, 8, 0, 1, TEXT_XCENTER | TEXT_UPPERCASE, 0, 0, xdim, ydim);

            if ((!GTFLAGS(GAMETYPE_COOP) && !GTFLAGS(GAMETYPE_PLAYERSFRIENDLY)) && ((GTFLAGS(GAMETYPE_TDM) && (g_player[ud.playerid_num].pteam != g_player[screenpeek].pteam)) || !GTFLAGS(GAMETYPE_TDM)))
            {
                G_ScreenText(MINIFONT, 160, ID_YPOS + 8, 65536, 0, 0, "Enemy", -127, 2, RS_AUTO | RS_NOCLIP | RS_TOPLEFT, idalpha, 3, 5, 1, 1, TEXT_XCENTER | TEXT_UPPERCASE, 0, 0, xdim, ydim);
            }
            else if ((GTFLAGS(GAMETYPE_COOP) || GTFLAGS(GAMETYPE_PLAYERSFRIENDLY)) || (GTFLAGS(GAMETYPE_TDM) && (g_player[ud.playerid_num].pteam == g_player[screenpeek].pteam)))
            {
                G_ScreenText(MINIFONT, 160, ID_YPOS + 8, 65536, 0, 0, "Friendly", -127, 6, RS_AUTO | RS_NOCLIP | RS_TOPLEFT, idalpha, 3, 5, 1, 1, TEXT_XCENTER | TEXT_UPPERCASE, 0, 0, xdim, ydim);

                if (WW2GI)
                {
                    Bsprintf(tempbuf, "^12Health: ^02%d ^07/ ^12Morale: ^06%d", TrackerCast(sprite[g_player[ud.playerid_num].ps->i].extra), G_GetMorale(g_player[ud.playerid_num].ps->i, ud.playerid_num));
                }
                else
                {
                    Bsprintf(tempbuf, "^12Health: ^02%d ^07/ ^12Armor: ^06%d", TrackerCast(sprite[g_player[ud.playerid_num].ps->i].extra), g_player[ud.playerid_num].ps->inv_amount[GET_SHIELD]);
                }
                G_ScreenText(MINIFONT, 160, ID_YPOS + 14, 65536, 0, 0, tempbuf, -127, 12, RS_AUTO | RS_NOCLIP | RS_TOPLEFT, idalpha, 3, 5, 1, 1, TEXT_XCENTER | TEXT_UPPERCASE, 0, 0, xdim, ydim);
            }
        }
    }

    if (ud.multimode > 1)
    {
        if (WITHIN_TIME(g_player[screenpeek].fragged_time, KILLID_TIME))
        {
            idalpha = ID_ALPHA;
            if (WITHIN_TIME(g_player[screenpeek].fragged_time, KILLID_FADETIME))
            {
                idalpha += ID_ALPHA_RANGE - scale(g_player[screenpeek].fragged_time - (int32_t)totalclock, ID_ALPHA_RANGE, KILLID_FADETIME);
            }

            // HACK: Modulo palookup to keep under 3 digits. ScreenText really needs a 3-digit form of color code.
            Bsprintf(tempbuf, "KILLED ^%02d%s", g_player[g_player[screenpeek].fragged_id].ps->palookup % 100, g_player[g_player[screenpeek].fragged_id].user_name);
            G_ScreenText(STARTALPHANUM, 160, ID_YPOS - 9, 65536, 0, 0, tempbuf, -127, 0, RS_AUTO | RS_NOCLIP | RS_TOPLEFT, idalpha, 4, 8, 0, 1, TEXT_XCENTER | TEXT_UPPERCASE, 0, 0, xdim, ydim);
        }

        if (WITHIN_TIME(g_player[screenpeek].fraggedby_time, KILLID_TIME))
        {
            idalpha = ID_ALPHA;
            if (WITHIN_TIME(g_player[screenpeek].fraggedby_time, KILLID_FADETIME))
            {
                idalpha += ID_ALPHA_RANGE - scale(g_player[screenpeek].fraggedby_time - (int32_t)totalclock, ID_ALPHA_RANGE, KILLID_FADETIME);
            }

            // HACK: Modulo palookup to keep under 3 digits. ScreenText really needs a 3-digit form of color code.
            Bsprintf(tempbuf, "KILLED BY ^%02d%s", g_player[g_player[screenpeek].fraggedby_id].ps->palookup % 100, g_player[g_player[screenpeek].fraggedby_id].user_name);
            G_ScreenText(STARTALPHANUM, 160, ID_YPOS - 18, 65536, 0, 0, tempbuf, -127, 0, RS_AUTO | RS_NOCLIP | RS_TOPLEFT, idalpha, 4, 8, 0, 1, TEXT_XCENTER | TEXT_UPPERCASE, 0, 0, xdim, ydim);
        }

        if (ud.limit_hit == 1) // Someone won!
        {
            int pal = 0;
            rotatesprite_(0, 0, 65536L, 0, BETASCREEN, 0, 4, RS_AUTO | RS_TOPLEFT, 128, 0, get_xdim(160 - 120), get_ydim(ID_YPOS - 37), get_xdim(160 + 120), get_ydim(ID_YPOS - 28));

            if (GTFLAGS(GAMETYPE_TDM))
            {
                pal = teams[ud.winner].palnum;
                Bsprintf(tempbuf, "%s Team ^8Wins!", teams[ud.winner].name);
            }
            else
            {
                pal = g_player[ud.winner].ps->palookup;
                Bsprintf(tempbuf, "%s ^8Wins!", g_player[ud.winner].user_name);
            }

            G_ScreenText(STARTALPHANUM, 160, ID_YPOS - 36, 65536, 0, 0, tempbuf, -127, pal, RS_AUTO | RS_NOCLIP | RS_TOPLEFT, 0, 4, 8, 0, 1, TEXT_XCENTER | TEXT_UPPERCASE, 0, 0, xdim, ydim);
        }

        if (screenpeek != myconnectindex)
        {
            G_ScreenText(MINIFONT, 160, 16, 65536, 0, 0, "Following", -127, 6, RS_AUTO | RS_NOCLIP | RS_TOPLEFT, 64, 3, 5, 1, 1, TEXT_XCENTER | TEXT_UPPERCASE, 0, 0, xdim, ydim);
            G_ScreenText(STARTALPHANUM, 160, 16 + 7, 65536, 0, 0, g_player[screenpeek].user_name, -127, g_player[screenpeek].ps->palookup, RS_AUTO | RS_NOCLIP | RS_TOPLEFT, 64, 4, 8, 0, 1, TEXT_XCENTER | TEXT_UPPERCASE, 0, 0, xdim, ydim);
        }
    }
}

void G_AddKillToFeed(int victim, int attacker)
{
    g_player[victim].fraggedby_id = attacker;
    g_player[victim].fraggedby_time = g_player[attacker].fragged_time = (int32_t)totalclock + KILLID_TIME;
    g_player[attacker].fragged_id = victim;
}

// Level Name
void G_PrintLevelName(void)
{
    if (!ud.show_level_text || !hud_showmapname || !g_levelTextTime)
        return;

    int bits = 10 + 16;

    if (g_levelTextTime > 4)
        bits = bits;
    else if (g_levelTextTime > 2)
        bits |= 1;
    else bits |= 1 + 32;

    if (g_mapInfo[(ud.volume_number * MAXLEVELS) + ud.level_number].name != NULL)
    {
        if (currentboardfilename[0] != 0 && ud.volume_number == 0 && ud.level_number == 7)
            menutext_(160, 75, -g_levelTextTime + 22, 0, currentboardfilename, bits);
        else
            menutext_(160, 75, -g_levelTextTime + 22, 0, g_mapInfo[(ud.volume_number * MAXLEVELS) + ud.level_number].name, bits);
    }
}

void G_DrawLevelStats()
{
    if (!ud.levelstats || ud.overhead_on == 2 || (ud.gm & MODE_MENU) != 0 || X_OnEventWithReturn(EVENT_DISPLAYLEVELSTATS, g_player[screenpeek].ps->i, screenpeek, 0) != 0)
        return;

    int x, y;
    auto ps = g_player[screenpeek].ps;

    // JBF 20040124: display level stats in screen corner
    if (ud.screen_size == 4)
        y = scale(ud.althud ? tilesiz[BIGALPHANUM].y + 10 : tilesiz[INVENTORYBOX].y + 2, ud.statusbarscale, 100);
    else if (ud.screen_size > 2)
        y = scale(tilesiz[BOTTOMSTATUSBAR].y + 1, ud.statusbarscale, 100);
    else
        y = 2;

    x = scale(2, ud.config.ScreenWidth, 320);

    Bsprintf(tempbuf, "T:^15%d:%02d.%02d",
        (ps->player_par / (REALGAMETICSPERSEC * 60)),
        (ps->player_par / REALGAMETICSPERSEC) % 60,
        ((ps->player_par % REALGAMETICSPERSEC) * 33) / 10
    );
    G_PrintGameText(13, STARTALPHANUM, x, scale(200 - y, ud.config.ScreenHeight, 200) - textsc(21), tempbuf, 0, 10, 26, 0, 0, xdim - 1, ydim - 1, 65536);

    if (ud.player_skill > 3 || (ud.multimode > 1 && !GTFLAGS(GAMETYPE_PLAYERSFRIENDLY)))
    {
        Bsprintf(tempbuf, "K:^15%d", (ud.multimode > 1 && !GTFLAGS(GAMETYPE_PLAYERSFRIENDLY)) ?
            ps->frag - ps->fraggedself : ud.monsters_killed);
    }
    else
    {
        if (ud.monsters_killed >= ud.total_monsters)
            Bsprintf(tempbuf, "K:%d/%d", ud.monsters_killed,
                ud.total_monsters > ud.monsters_killed ?
                ud.total_monsters : ud.monsters_killed);
        else
            Bsprintf(tempbuf, "K:^15%d/%d", ud.monsters_killed,
                ud.total_monsters > ud.monsters_killed ?
                ud.total_monsters : ud.monsters_killed);
    }
    G_PrintGameText(13, STARTALPHANUM, x, scale(200 - y, ud.config.ScreenHeight, 200) - textsc(14), tempbuf, 0, 10, 26, 0, 0, xdim - 1, ydim - 1, 65536);

    if (ud.secret_rooms == ud.max_secret_rooms)
        Bsprintf(tempbuf, "S:%d/%d", ud.secret_rooms, ud.max_secret_rooms);
    else
        Bsprintf(tempbuf, "S:^15%d/%d", ud.secret_rooms, ud.max_secret_rooms);
    G_PrintGameText(13, STARTALPHANUM, x, scale(200 - y, ud.config.ScreenHeight, 200) - textsc(7), tempbuf, 0, 10, 26, 0, 0, xdim - 1, ydim - 1, 65536);
}
