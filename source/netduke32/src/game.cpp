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
//#include "types.h"
#include "scriplib.h"
//#include "file_lib.h"
//#include "mathutil.h"
#include "gamedefs.h"
#include "keyboard.h"
#include "mouse.h"  // JBF 20030809
#include "function.h"
#include "control.h"
#include "fx_man.h"
#include "sounds.h"
#include "config.h"
#include "osd.h"
#include "osdfuncs.h"
#include "osdcmds.h"
#include "scriptfile.h"
#include "grpscan.h"
#include "kplib.h"
//#include "crc32.h"
//#include "util_lib.h"
#include "hightile.h"
#include "al_midi.h"
#include "colmatch.h"
#include "renderlayer.h"

#ifdef DISCORD_RPC
#include "discord.h"
#endif

#ifdef _WIN32
#include <shellapi.h>
#define UPDATEINTERVAL 604800 // 1w
#include "winbits.h"
#endif /* _WIN32 */
int g_scriptSanityChecks = 1;

user_defs ud;
int32_t playerswhenstarted;

const char* AppProperName = APPNAME;
const char* AppTechnicalName = APPBASENAME;

int g_cameraDistance = 0, g_cameraClock = 0;

char boardfilename[BMAX_PATH] = {0}, currentboardfilename[BMAX_PATH] = {0};
char root[BMAX_PATH];

int recfilep,totalreccnt;
int debug_on = 0,g_noEnemies = 0;

char *rtsptr;

static FILE *frecfilep = (FILE *)NULL;

int g_restorePalette,g_screenCapture;

char eduke32dat[BMAX_PATH] = "eduke32.dat";
const char *eduke32def = "eduke32.def";
static const char* defaultrtsfilename[GAMECOUNT] = { "DUKE.RTS", "NAM.RTS", "NAPALM.RTS", "WW2GI.RTS" };

int g_Shareware = 0;

int quotebot, quotebotgoal;
int user_quote_time[MAXUSERQUOTES];
char user_quote[MAXUSERQUOTES][MAXUSERQUOTELEN];
// char typebuflen,typebuf[41];

int32_t MAXCACHE1DSIZE = (96*1024*1024);

int tempwallptr;

static int nonsharedtimer;

static void G_DrawCameraText(short i);
static int G_MoveLoop(void);
static int G_DoMoveThings(void);
static int G_PlaybackDemo(void);

enum gametokens
{
    T_INCLUDE = 0,
    T_INTERFACE = 0,
    T_LOADGRP = 1,
    T_MODE = 1,
    T_CACHESIZE = 2,
    T_ALLOW = 2,
    T_DEFINE,
    T_NOAUTOLOAD,
    T_MUSIC,
    T_SOUND,
    T_FILE,
    T_ADDPLAYERCOL,
    T_DELPLAYERCOL,
    T_ID,
    T_MINPITCH,
    T_MAXPITCH,
    T_PRIORITY,
    T_TYPE,
    T_DISTANCE,
    T_VOLUME,
};

void SetIfGreater(int32_t *variable, int32_t potentialValue)
{
    if (potentialValue > *variable)
        *variable = potentialValue;
}

// get the string length until the next '\n'
int32_t G_GetStringLineLength(const char *text, const char *end, const int32_t iter)
{
    int32_t length = 0;

    while (*text != '\n' && text != end)
    {
        ++length;

        text += iter;
    }

    return length;
}

int32_t G_GetStringNumLines(const char *text, const char *end, const int32_t iter)
{
    int32_t count = 1;

    while (text != end)
    {
        if (*text == '\n')
            ++count;
        text += iter;
    }

    return count;
}
// Note: Neither of these care about TEXT_LINEWRAP. This is intended.

// This function requires you to Bfree() the returned char*.
char* G_GetSubString(const char *text, const char *end, const int32_t iter, const int32_t length)
{
    char *line = (char*)Xmalloc((length + 1) * sizeof(char));
    int32_t counter = 0;

    while (counter < length && text != end)
    {
        line[counter] = *text;

        text += iter;
        ++counter;
    }

    line[counter] = '\0';

    return line;
}

// assign the character's tilenum
int32_t G_GetStringTile(int32_t font, char *t, int32_t f)
{
    if (f & TEXT_DIGITALNUMBER)
        return *t - '0' + font; // copied from digitalnumber
    else if (f & (TEXT_BIGALPHANUM | TEXT_GRAYFONT))
    {
        int32_t offset = (f & TEXT_GRAYFONT) ? 26 : 0;

        if (*t >= '0' && *t <= '9')
            return *t - '0' + font + ((f & TEXT_GRAYFONT) ? 26 : -10);
        else if (*t >= 'a' && *t <= 'z')
            return *t - 'a' + font + ((f & TEXT_GRAYFONT) ? -26 : 26);
        else if (*t >= 'A' && *t <= 'Z')
            return *t - 'A' + font;
        else switch (*t)
        {
        case '_':
        case '-':
            return font - (11 + offset);
            break;
        case '.':
            return font + (BIGPERIOD - (BIGALPHANUM + offset));
            break;
        case ',':
            return font + (BIGCOMMA - (BIGALPHANUM + offset));
            break;
        case '!':
            return font + (BIGX_ - (BIGALPHANUM + offset));
            break;
        case '?':
            return font + (BIGQ - (BIGALPHANUM + offset));
            break;
        case ';':
            return font + (BIGSEMI - (BIGALPHANUM + offset));
            break;
        case ':':
            return font + (BIGCOLIN - (BIGALPHANUM + offset));
            break;
        case '\\':
        case '/':
            return font + (68 - offset); // 3008-2940
            break;
        case '%':
            return font + (69 - offset); // 3009-2940
            break;
        case '`':
        case '\"': // could be better hacked in
        case '\'':
            return font + (BIGAPPOS - (BIGALPHANUM + offset));
            break;
        default: // unknown character
            *t = ' '; // whitespace-ize
            return font;
            break;
        }
    }
    else
        return *t - '!' + font; // uses ASCII order
}

// qstrdim
vec2_t G_ScreenTextSize(const int32_t font,
    int32_t x, int32_t y, const int32_t z, const int32_t blockangle,
    const char *str, const int32_t o,
    int32_t xspace, int32_t yline, int32_t xbetween, int32_t ybetween,
    const int32_t f,
    int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    vec2_t size = { 0, 0, }; // eventually the return value
    vec2_t pos = { 0, 0, }; // holds the coordinate position as we draw each character tile of the string
    vec2_t extent = { 0, 0, }; // holds the x-width of each character and the greatest y-height of each line
    vec2_t offset = { 0, 0, }; // temporary; holds the last movement made in both directions

    int32_t tile;
    char t;

    // set the start and end points depending on direction
    int32_t iter = (f & TEXT_BACKWARDS) ? -1 : 1; // iteration direction

    const char *end;
    const char *text;

    UNREFERENCED_PARAMETER(x1);
    UNREFERENCED_PARAMETER(y1);
    UNREFERENCED_PARAMETER(x2);
    UNREFERENCED_PARAMETER(y2);

    if (str == NULL)
        return size;

    end = (f & TEXT_BACKWARDS) ? str - 1 : Bstrchr(str, '\0');
    text = (f & TEXT_BACKWARDS) ? Bstrchr(str, '\0') - 1 : str;

    // optimization: justification in both directions
    if ((f & TEXT_XJUSTIFY) && (f & TEXT_YJUSTIFY))
    {
        size.x = xbetween;
        size.y = ybetween;
        return size;
    }

    // for best results, we promote 320x200 coordinates to full precision before any math
    if (!(o & ROTATESPRITE_FULL16))
    {
        x <<= 16;
        y <<= 16;
        xspace <<= 16;
        yline <<= 16;
        xbetween <<= 16;
        ybetween <<= 16;
    }
    // coordinate values should be shifted left by 16

    // handle zooming where applicable
    xspace = scale(xspace, z, 65536);
    yline = scale(yline, z, 65536);
    xbetween = scale(xbetween, z, 65536);
    ybetween = scale(ybetween, z, 65536);
    // size/width/height/spacing/offset values should be multiplied or scaled by $z, zoom (since 100% is 65536, the same as 1<<16)

    // loop through the string
    while ((t = *text) && text != end)
    {
        // handle escape sequences
        if (t == '^' && Bisdigit(*(text + iter)) && !(f & TEXT_LITERALESCAPE))
        {
            text += iter + iter;
            if (Bisdigit(*text))
                text += iter;
            continue;
        }

        // handle case bits
        if (f & TEXT_UPPERCASE)
        {
            if (f & TEXT_INVERTCASE) // optimization...?
            { // v^ important that these two ifs remain separate due to the else below
                if (Bisupper(t))
                    t = Btolower(t);
            }
            else if (Bislower(t))
                t = Btoupper(t);
        }
        else if (f & TEXT_INVERTCASE)
        {
            if (Bisupper(t))
                t = Btolower(t);
            else if (Bislower(t))
                t = Btoupper(t);
        }

        // translate the character to a tilenum
        tile = G_GetStringTile(font, &t, f);

        // reset this here because we haven't printed anything yet this loop
        extent.x = 0;

        // reset this here because the act of printing something on this line means that we include the margin above in the total size
        offset.y = 0;

        // handle each character itself in the context of screen drawing
        switch (t)
        {
        case '\t':
        case ' ':
            // width
            extent.x = xspace;

            if (f & (TEXT_INTERNALSPACE | TEXT_TILESPACE))
            {
                char space = '.'; // this is subject to change as an implementation detail
                if (f & TEXT_TILESPACE)
                    space = '\x7F'; // tile after '~'
                tile = G_GetStringTile(font, &space, f);

                extent.x += (tilesiz[tile].x * z);
            }

            // prepare the height // near-CODEDUP the other two near-CODEDUPs for this section
            {
                int32_t tempyextent = yline;

                if (f & (TEXT_INTERNALLINE | TEXT_TILELINE))
                {
                    char line = 'A'; // this is subject to change as an implementation detail
                    if (f & TEXT_TILELINE)
                        line = '\x7F'; // tile after '~'
                    tile = G_GetStringTile(font, &line, f);

                    tempyextent += tilesiz[tile].y * z;
                }

                SetIfGreater(&extent.y, tempyextent);
            }

            if (t == '\t')
                extent.x <<= 2; // *= 4

            break;

        case '\n': // near-CODEDUP "if (wrap)"
                   // save the position
            pos.x -= offset.x;
            SetIfGreater(&size.x, pos.x);

            // reset the position
            pos.x = 0;

            // prepare the height
            {
                int32_t tempyextent = yline;

                if (f & (TEXT_INTERNALLINE | TEXT_TILELINE))
                {
                    char line = 'A'; // this is subject to change as an implementation detail
                    if (f & TEXT_TILELINE)
                        line = '\x7F'; // tile after '~'
                    tile = G_GetStringTile(font, &line, f);

                    tempyextent += tilesiz[tile].y * z;
                }

                SetIfGreater(&extent.y, tempyextent);
            }

            // move down the line height
            if (!(f & TEXT_YOFFSETZERO))
                pos.y += extent.y;

            // reset the current height
            extent.y = 0;

            // line spacing
            offset.y = (f & TEXT_YJUSTIFY) ? 0 : ybetween; // ternary to prevent overflow
            pos.y += offset.y;

            break;

        default:
            // width
            extent.x = tilesiz[tile].x * z;

            // obnoxious hardcoded functionality from gametext
            if ((f & TEXT_GAMETEXTNUMHACK) && t >= '0' && t <= '9')
            {
                char numeral = '0'; // this is subject to change as an implementation detail
                extent.x = (tilesiz[G_GetStringTile(font, &numeral, f)].x - 1) * z;
            }

            // height
            SetIfGreater(&extent.y, (tilesiz[tile].y * z));

            break;
        }

        // incrementing the coordinate counters
        offset.x = 0;

        // advance the x coordinate
        if (!(f & TEXT_XOFFSETZERO))
            offset.x += extent.x;

        // account for text spacing
        if (!((f & TEXT_GAMETEXTNUMHACK) && t >= '0' && t <= '9') // this "if" line ONLY == replicating hardcoded stuff
            && !(f & TEXT_XJUSTIFY)) // to prevent overflow
            offset.x += xbetween;

        // line wrapping
        if ((f & TEXT_LINEWRAP) && !(f & TEXT_XRIGHT) && !(f & TEXT_XCENTER) && blockangle % 512 == 0)
        {
            int32_t wrap = 0;
            const int32_t ang = blockangle % 2048;

            // this is the only place in qstrdim where angle actually affects direction, but only in the wrapping measurement
            switch (ang)
            {
            case 0:
                wrap = (x + (pos.x + offset.x) > (320 << 16)); // ((x2 - USERQUOTE_RIGHTOFFSET)<<16)
                break;
            case 512:
                wrap = (y + (pos.x + offset.x) > (200 << 16)); // ((y2 - USERQUOTE_RIGHTOFFSET)<<16)
                break;
            case 1024:
                wrap = (x - (pos.x + offset.x) < 0); // ((x1 + USERQUOTE_RIGHTOFFSET)<<16)
                break;
            case 1536:
                wrap = (y - (pos.x + offset.x) < 0); // ((y1 + USERQUOTE_RIGHTOFFSET)<<16)
                break;
            }
            if (wrap) // near-CODEDUP "case '\n':"
            {
                // save the position
                SetIfGreater(&size.x, pos.x);

                // reset the position
                pos.x = 0;

                // prepare the height
                {
                    int32_t tempyextent = yline;

                    if (f & (TEXT_INTERNALLINE | TEXT_TILELINE))
                    {
                        char line = 'A'; // this is subject to change as an implementation detail
                        if (f & TEXT_TILELINE)
                            line = '\x7F'; // tile after '~'
                        tile = G_GetStringTile(font, &line, f);

                        tempyextent += tilesiz[tile].y * z;
                    }

                    SetIfGreater(&extent.y, tempyextent);
                }

                // move down the line height
                if (!(f & TEXT_YOFFSETZERO))
                    pos.y += extent.y;

                // reset the current height
                extent.y = 0;

                // line spacing
                offset.y = (f & TEXT_YJUSTIFY) ? 0 : ybetween; // ternary to prevent overflow
                pos.y += offset.y;
            }
            else
                pos.x += offset.x;
        }
        else
            pos.x += offset.x;

        // save some trouble with calculation
        if (!(f & TEXT_XOFFSETZERO))
            offset.x -= extent.x;

        // iterate to the next character in the string
        text += iter;
    }

    // calculate final size
    pos.x -= offset.x;
    if (f & TEXT_XOFFSETZERO)
        pos.x += extent.x;
    pos.y -= offset.y;
    pos.y += extent.y;

    SetIfGreater(&size.x, pos.x);
    SetIfGreater(&size.y, pos.y);

    // justification where only one of the two directions is set, so we have to iterate
    if (f & TEXT_XJUSTIFY)
        size.x = xbetween;
    if (f & TEXT_YJUSTIFY)
        size.y = ybetween;

    // return values in the same manner we receive them
    if (!(o & ROTATESPRITE_FULL16))
    {
        size.x >>= 16;
        size.y >>= 16;
    }

    return size;
}

void G_AddCoordsFromRotation(vec2_t *coords, const vec2_t *unitDirection, const int32_t magnitude)
{
    coords->x += scale(magnitude, unitDirection->x, 16384);
    coords->y += scale(magnitude, unitDirection->y, 16384);
}

// screentext
vec2_t G_ScreenText(const int32_t font,
    int32_t x, int32_t y, const int32_t z, const int32_t blockangle, const int32_t charangle,
    const char *str, const int32_t shade, int32_t pal, int32_t o, const int32_t alpha,
    int32_t xspace, int32_t yline, int32_t xbetween, int32_t ybetween, const int32_t f,
    const int32_t x1, const int32_t y1, const int32_t x2, const int32_t y2)
{
    vec2_t size = { 0, 0, }; // eventually the return value
    vec2_t origin = { 0, 0, }; // where to start, depending on the alignment
    vec2_t pos = { 0, 0, }; // holds the coordinate position as we draw each character tile of the string
    vec2_t extent = { 0, 0, }; // holds the x-width of each character and the greatest y-height of each line
    const vec2_t Xdirection = { sintable[(blockangle + 512) & 2047], sintable[blockangle & 2047], };
    const vec2_t Ydirection = { sintable[(blockangle + 1024) & 2047], sintable[(blockangle + 512) & 2047], };

    int32_t tile;
    char t;

    // set the start and end points depending on direction
    int32_t iter = (f & TEXT_BACKWARDS) ? -1 : 1; // iteration direction

    const char *end;
    const char *text;

    if (str == NULL)
        return size;

    end = (f & TEXT_BACKWARDS) ? str - 1 : Bstrchr(str, '\0');
    text = (f & TEXT_BACKWARDS) ? Bstrchr(str, '\0') - 1 : str;

    // for best results, we promote 320x200 coordinates to full precision before any math
    if (!(o & ROTATESPRITE_FULL16))
    {
        x <<= 16;
        y <<= 16;
        xspace <<= 16;
        yline <<= 16;
        xbetween <<= 16;
        ybetween <<= 16;
    }
    // coordinate values should be shifted left by 16

    // eliminate conflicts, necessary here to get the correct size value
    // especially given justification's special handling in G_ScreenTextSize()
    if ((f & TEXT_XRIGHT) || (f & TEXT_XCENTER) || (f & TEXT_XJUSTIFY) || (f & TEXT_YJUSTIFY) || blockangle % 512 != 0)
        o &= ~TEXT_LINEWRAP;

    // size is the return value, and we need it for alignment
    size = G_ScreenTextSize(font, x, y, z, blockangle, str, o | ROTATESPRITE_FULL16, xspace, yline, (f & TEXT_XJUSTIFY) ? 0 : xbetween, (f & TEXT_YJUSTIFY) ? 0 : ybetween, f & ~(TEXT_XJUSTIFY | TEXT_YJUSTIFY), x1, y1, x2, y2);

    // handle zooming where applicable
    xspace = scale(xspace, z, 65536);
    yline = scale(yline, z, 65536);
    xbetween = scale(xbetween, z, 65536);
    ybetween = scale(ybetween, z, 65536);
    // size/width/height/spacing/offset values should be multiplied or scaled by $z, zoom (since 100% is 65536, the same as 1<<16)

    // alignment
    // near-CODEDUP "case '\n':"
    {
        int32_t lines = G_GetStringNumLines(text, end, iter);

        if ((f & TEXT_XJUSTIFY) || (f & TEXT_XRIGHT) || (f & TEXT_XCENTER))
        {
            const int32_t length = G_GetStringLineLength(text, end, iter);

            int32_t linewidth = size.x;

            if (lines != 1)
            {
                char *line = G_GetSubString(text, end, iter, length);

                linewidth = G_ScreenTextSize(font, x, y, z, blockangle, line, o | ROTATESPRITE_FULL16, xspace, yline, 0, 0, f & ~(TEXT_XJUSTIFY | TEXT_YJUSTIFY | TEXT_BACKWARDS), x1, y1, x2, y2).x;

                Xfree(line);
            }

            if (f & TEXT_XJUSTIFY)
            {
                size.x = xbetween;

                xbetween = (length == 1) ? 0 : ((xbetween - linewidth) / (length - 1));

                linewidth = size.x;
            }

            if (f & TEXT_XRIGHT)
                origin.x = -linewidth;
            else if (f & TEXT_XCENTER)
                origin.x = -(linewidth / 2);
        }

        if (f & TEXT_YJUSTIFY)
        {
            const int32_t tempswap = ybetween;
            ybetween = (lines == 1) ? 0 : ((ybetween - size.y) / (lines - 1));
            size.y = tempswap;
        }

        if (f & TEXT_YBOTTOM)
            origin.y = -size.y;
        else if (f & TEXT_YCENTER)
            origin.y = -(size.y / 2);
    }

    // loop through the string
    while ((t = *text) && text != end)
    {
        int32_t orientation = o;
        int32_t angle = blockangle + charangle;

        // handle escape sequences
        if (t == '^' && Bisdigit(*(text + iter)) && !(f & TEXT_LITERALESCAPE))
        {
            char smallbuf[4];

            text += iter;
            smallbuf[0] = *text;

            text += iter;
            if (Bisdigit(*text))
            {
                smallbuf[1] = *text;
                smallbuf[2] = '\0';
                text += iter;
            }
            else
                smallbuf[1] = '\0';

            if (!(f & TEXT_IGNOREESCAPE))
                pal = Batoi(smallbuf);

            continue;
        }

        // handle case bits
        if (f & TEXT_UPPERCASE)
        {
            if (f & TEXT_INVERTCASE) // optimization...?
            { // v^ important that these two ifs remain separate due to the else below
                if (Bisupper(t))
                    t = Btolower(t);
            }
            else if (Bislower(t))
                t = Btoupper(t);
        }
        else if (f & TEXT_INVERTCASE)
        {
            if (Bisupper(t))
                t = Btolower(t);
            else if (Bislower(t))
                t = Btoupper(t);
        }

        // translate the character to a tilenum
        tile = G_GetStringTile(font, &t, f);

        switch (t)
        {
        case '\t':
        case ' ':
        case '\n':
        case '\x7F':
            break;

        default:
        {
            vec2_t location = { x, y, };

            G_AddCoordsFromRotation(&location, &Xdirection, origin.x);
            G_AddCoordsFromRotation(&location, &Ydirection, origin.y);

            G_AddCoordsFromRotation(&location, &Xdirection, pos.x);
            G_AddCoordsFromRotation(&location, &Ydirection, pos.y);

            rotatesprite_(location.x, location.y, z, angle, tile, shade, pal, 2 | orientation, alpha, 0, x1, y1, x2, y2);

            break;
        }
        }

        // reset this here because we haven't printed anything yet this loop
        extent.x = 0;

        // handle each character itself in the context of screen drawing
        switch (t)
        {
        case '\t':
        case ' ':
            // width
            extent.x = xspace;

            if (f & (TEXT_INTERNALSPACE | TEXT_TILESPACE))
            {
                char space = '.'; // this is subject to change as an implementation detail
                if (f & TEXT_TILESPACE)
                    space = '\x7F'; // tile after '~'
                tile = G_GetStringTile(font, &space, f);

                extent.x += (tilesiz[tile].x * z);
            }

            // prepare the height // near-CODEDUP the other two near-CODEDUPs for this section
            {
                int32_t tempyextent = yline;

                if (f & (TEXT_INTERNALLINE | TEXT_TILELINE))
                {
                    char line = 'A'; // this is subject to change as an implementation detail
                    if (f & TEXT_TILELINE)
                        line = '\x7F'; // tile after '~'
                    tile = G_GetStringTile(font, &line, f);

                    tempyextent += tilesiz[tile].y * z;
                }

                SetIfGreater(&extent.y, tempyextent);
            }

            if (t == '\t')
                extent.x <<= 2; // *= 4

            break;

        case '\n': // near-CODEDUP "if (wrap)"
                   // reset the position
            pos.x = 0;

            // prepare the height
            {
                int32_t tempyextent = yline;

                if (f & (TEXT_INTERNALLINE | TEXT_TILELINE))
                {
                    char line = 'A'; // this is subject to change as an implementation detail
                    if (f & TEXT_TILELINE)
                        line = '\x7F'; // tile after '~'
                    tile = G_GetStringTile(font, &line, f);

                    tempyextent += tilesiz[tile].y * z;
                }

                SetIfGreater(&extent.y, tempyextent);
            }

            // move down the line height
            if (!(f & TEXT_YOFFSETZERO))
                pos.y += extent.y;

            // reset the current height
            extent.y = 0;

            // line spacing
            pos.y += ybetween;

            // near-CODEDUP "alignments"
            if ((f & TEXT_XJUSTIFY) || (f & TEXT_XRIGHT) || (f & TEXT_XCENTER))
            {
                const int32_t length = G_GetStringLineLength(text, end, iter);

                char *line = G_GetSubString(text, end, iter, length);

                int32_t linewidth = G_ScreenTextSize(font, x, y, z, blockangle, line, o | ROTATESPRITE_FULL16, xspace, yline, 0, 0, f & ~(TEXT_XJUSTIFY | TEXT_YJUSTIFY | TEXT_BACKWARDS), x1, y1, x2, y2).x;

                Xfree(line);

                if (f & TEXT_XJUSTIFY)
                {
                    xbetween = (length == 1) ? 0 : ((xbetween - linewidth) / (length - 1));

                    linewidth = size.x;
                }

                if (f & TEXT_XRIGHT)
                    origin.x = -linewidth;
                else if (f & TEXT_XCENTER)
                    origin.x = -(linewidth / 2);
            }

            break;

        default:
            // width
            extent.x = tilesiz[tile].x * z;

            // obnoxious hardcoded functionality from gametext
            if ((f & TEXT_GAMETEXTNUMHACK) && t >= '0' && t <= '9')
            {
                char numeral = '0'; // this is subject to change as an implementation detail
                extent.x = (tilesiz[G_GetStringTile(font, &numeral, f)].x - 1) * z;
            }

            // height
            SetIfGreater(&extent.y, (tilesiz[tile].y * z));

            break;
        }

        // incrementing the coordinate counters
        {
            int32_t xoffset = 0;

            // advance the x coordinate
            if (!(f & TEXT_XOFFSETZERO))
                xoffset += extent.x;

            // account for text spacing
            if (!((f & TEXT_GAMETEXTNUMHACK) && t >= '0' && t <= '9')) // this "if" line ONLY == replicating hardcoded stuff
                xoffset += xbetween;

            // line wrapping
            if (f & TEXT_LINEWRAP)
            {
                int32_t wrap = 0;
                const int32_t ang = blockangle % 2048;

                // it's safe to make some assumptions and not go through G_AddCoordsFromRotation() since we limit to four directions
                switch (ang)
                {
                case 0:
                    wrap = (x + (pos.x + xoffset) > (320 << 16)); // ((x2 - USERQUOTE_RIGHTOFFSET)<<16)
                    break;
                case 512:
                    wrap = (y + (pos.x + xoffset) > (200 << 16)); // ((y2 - USERQUOTE_RIGHTOFFSET)<<16)
                    break;
                case 1024:
                    wrap = (x - (pos.x + xoffset) < 0); // ((x1 + USERQUOTE_RIGHTOFFSET)<<16)
                    break;
                case 1536:
                    wrap = (y - (pos.x + xoffset) < 0); // ((y1 + USERQUOTE_RIGHTOFFSET)<<16)
                    break;
                }
                if (wrap) // near-CODEDUP "case '\n':"
                {
                    // reset the position
                    pos.x = 0;

                    // prepare the height
                    {
                        int32_t tempyextent = yline;

                        if (f & (TEXT_INTERNALLINE | TEXT_TILELINE))
                        {
                            char line = 'A'; // this is subject to change as an implementation detail
                            if (f & TEXT_TILELINE)
                                line = '\x7F'; // tile after '~'
                            tile = G_GetStringTile(font, &line, f);

                            tempyextent += tilesiz[tile].y * z;
                        }

                        SetIfGreater(&extent.y, tempyextent);
                    }

                    // move down the line height
                    if (!(f & TEXT_YOFFSETZERO))
                        pos.y += extent.y;

                    // reset the current height
                    extent.y = 0;

                    // line spacing
                    pos.y += ybetween;
                }
                else
                    pos.x += xoffset;
            }
            else
                pos.x += xoffset;
        }

        // iterate to the next character in the string
        text += iter;
    }

    // return values in the same manner we receive them
    if (!(o & ROTATESPRITE_FULL16))
    {
        size.x >>= 16;
        size.y >>= 16;
    }

    return size;
}

int32_t G_PrintGameText(int32_t f, int32_t tile, int32_t x, int32_t y, const char *t,
    int32_t s, int32_t p, int32_t o,
    int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t z)
{
    int32_t ac;
    char centre;
    int32_t squishtext = ((f & 2) != 0);
    int32_t shift = 16, widthx = 320;
    int32_t ox, oy, origx = x, origy = y;

    if (t == NULL)
        return -1;

    if (o & 256)
    {
        widthx = 320 << 16;
        shift = 0;
    }

    if ((centre = (x == (widthx >> 1))))
    {
        const char *oldt = t;
        int32_t newx = 0;

        do
        {
            int32_t i;

            if (*t == 32)
            {
                newx += ((((f & 8) ? 8 : 5) - squishtext) * z) >> 16;
                continue;
            }

            if (*t == '^' && isdigit(*(t + 1)))
            {
                t += 1 + isdigit(*(t + 2));
                continue;
            }

            ac = *t - '!' + tile;

            if (ac < tile || ac >(tile + 93)) break;

            newx += i = ((((f & 8) ? 8 : tilesiz[ac].x) - squishtext) * z) >> 16;

            if (*t >= '0' && *t <= '9')
                newx -= i - ((8 * z) >> 16);
        } while (*(++t));

        t = oldt;

        x = (f & 4) ?
            (xres >> 1) - textsc(newx >> 1) :
            (widthx >> 1) - ((o & 256) ? newx << 15 : newx >> 1);
    }

    ox = x;
    oy = y;

    do
    {
        int32_t i;

        if (*t == 32)
        {
            x += ((((f & 8) ? 8 : 5) - squishtext) * z) >> 16;
            continue;
        }

        if (*t == '^' && isdigit(*(t + 1)))
        {
            char smallbuf[4];

            if (!isdigit(*(++t + 1)))
            {
                smallbuf[0] = *(t);
                smallbuf[1] = '\0';
                p = atoi(smallbuf);
                continue;
            }

            smallbuf[0] = *(t++);
            smallbuf[1] = *(t);
            smallbuf[2] = '\0';
            p = atoi(smallbuf);
            continue;
        }

        ac = *t - '!' + tile;

        if (ac < tile || ac >(tile + 93))
            break;

        if (o & 256)
        {
            ox = x += (x - ox) << 16;
            oy = y += (y - oy) << 16;
        }

        if (f & 4)
            rotatesprite(textsc(x << shift), (origy << shift) + textsc((y - origy) << shift), textsc(z),
                0, ac, s, p, (8 | 16 | (o & 1) | (o & 32)), x1, y1, x2, y2);
        else if (f & 1)
            rotatesprite(x << shift, (y << shift), z, 0, ac, s, p, (8 | 16 | (o & 1) | (o & 32)), x1, y1, x2, y2);
        else rotatesprite(x << shift, (y << shift), z, 0, ac, s, p, (2 | o), x1, y1, x2, y2);

        x += i = (f & 8) ?
            ((8 - squishtext) * z) >> 16 :
            ((tilesiz[ac].x - squishtext) * z) >> 16;

        if ((*t >= '0' && *t <= '9'))
            x -= i - ((8 * z) >> 16);

        if ((o & 256) == 0) //  wrapping long strings doesn't work for precise coordinates due to overflow
        {
            if (((f & 4) ? textsc(x) : x) > (ud.config.ScreenWidth - USERQUOTE_RIGHTOFFSET))
                x = origx, y += (8 * z) >> 16;
        }
    } while (*(++t));

    return x;
}

int G_GameTextLen(int x,const char *t)
{
    int ac;

    if (t == NULL)
        return -1;

    do
    {
        if (*t == 32)
        {
            x+=5;
            continue;
        }
        else ac = *t - '!' + STARTALPHANUM;

        if (ac < STARTALPHANUM || ac > (STARTALPHANUM + 93))
            break;

        if ((*t >= '0' && *t <= '9'))
            x += 8;
        else x += tilesiz[ac].x;
    }
    while (*(++t));
    return (textsc(x));
}

int mpgametext(int y,const char *t,int s,int dabits)
{
    return(G_PrintGameText(4,STARTALPHANUM, 5,y,t,s,0,dabits,0, 0, xdim-1, ydim-1, 65536));
}

int minitext_(int x, int y, const char* t, int s, int p, int sb)
{
    int ac;
    char ch, cmode;
    //    int ht = usehightile;

    cmode = (sb & ROTATESPRITE_MAX) != 0;
    sb &= (ROTATESPRITE_MAX-1);

    //    usehightile = (ht && !r_downsize);
    do
    {
        if (*t == '^' && isdigit(*(t + 1)))
        {
            char smallbuf[4];
            if (!isdigit(*(++t + 1)))
            {
                smallbuf[0] = *(t);
                smallbuf[1] = '\0';
                p = atoi(smallbuf);
                continue;
            }
            smallbuf[0] = *(t++);
            smallbuf[1] = *(t);
            smallbuf[2] = '\0';
            p = atoi(smallbuf);
            continue;
        }
        ch = Btoupper(*t);
        if (ch == 32)
        {
            x += 5;
            continue;
        }
        else ac = ch - '!' + MINIFONT;

        if (cmode) rotatesprite(sbarx(x), sbary(y), sbarsc(65536L), 0, ac, s, p, sb, 0, 0, xdim - 1, ydim - 1);
        else rotatesprite(x << 16, y << 16, 65536L, 0, ac, s, p, sb, 0, 0, xdim - 1, ydim - 1);
        x += 4; // tilesiz[ac].x+1;

    } while (*(++t));
    //    usehightile = ht;
    return (x);
}

void G_AddUserQuote(const char *daquote)
{
    int i;

    // Crash prevention.
    if (Bstrlen(daquote) > MAXUSERQUOTELEN)
        return;

    for (i=MAXUSERQUOTES-1;i>0;i--)
    {
        Bstrcpy(user_quote[i],user_quote[i-1]);
        user_quote_time[i] = user_quote_time[i-1];
    }
    Bstrcpy(user_quote[0],daquote);
    OSD_Printf("%s\n",daquote);

    user_quote_time[0] = ud.msgdisptime;
    pub = NUMPAGES;
}

void G_HandleSpecialKeys(void)
{
    DukePlayer_t *myplayer = g_player[myconnectindex].ps;
    if (myplayer == NULL)
        return;

    CONTROL_ProcessBinds();

    if (ALT_IS_PRESSED && KB_KeyPressed(sc_Enter))
    {
        if (videoSetGameMode(!ud.config.ScreenMode,ud.config.ScreenWidth,ud.config.ScreenHeight,ud.config.ScreenBPP, ud.detail))
        {
            OSD_Printf(OSD_ERROR "Failed setting fullscreen video mode.\n");
            if (videoSetGameMode(ud.config.ScreenMode, ud.config.ScreenWidth, ud.config.ScreenHeight, ud.config.ScreenBPP, ud.detail))
                G_GameExit("Failed to recover from failure to set fullscreen video mode.\n");
        }
        else ud.config.ScreenMode = !ud.config.ScreenMode;
        KB_ClearKeyDown(sc_Enter);
        g_restorePalette = 1;
        G_UpdateScreenArea();
    }

    if (KB_UnBoundKeyPressed(sc_F12))
    {
        KB_ClearKeyDown(sc_F12);
        videoCaptureScreen("duke0000.tga",0);
        P_DoQuote(QUOTE_SCREEN_SAVED, myplayer);
    }

    // only dispatch commands here when not in a game
    if (!(ud.gm&MODE_GAME))
    {
        OSD_DispatchQueued();
    }

    if (quickExit == 0 && KB_KeyPressed(sc_LeftControl) && KB_KeyPressed(sc_LeftAlt) && (KB_KeyPressed(sc_Delete)||KB_KeyPressed(sc_End)))
    {
        quickExit = 1;
        G_GameExit("Quick Exit.");
    }
}


static void gameTimerHandler(void)
{
    MUSIC_Update();
    G_HandleSpecialKeys();
#ifdef DISCORD_RPC
    CheckDiscord();
#endif
}

void G_InitTimer(int32_t ticspersec)
{
    if (g_timerTicsPerSecond != ticspersec)
    {
        timerUninit();
        timerInit(ticspersec);
        timerSetCallback(gameTimerHandler);
        g_timerTicsPerSecond = ticspersec;
    }
}

static void G_ShowCacheLocks(void)
{
    short i,k;

    auto indexes = g_cache.getIndex();

    k = 0;
    for (i=g_cache.numBlocks()-1;i>=0;i--)
        if ((*indexes[i].lock) >= 200)
        {
            Bsprintf(tempbuf,"Locked- %d: Leng:%d, Lock:%d",i,indexes[i].leng,*indexes[i].lock);
            printext256(0L,k,31,-1,tempbuf,1);
            k += 6;
        }

    k += 6;

    for (i=10;i>=0;i--)
        if (rts_lumplockbyte[i] >= 200)
        {
            Bsprintf(tempbuf,"RTS Locked %d:",i);
            printext256(0L,k,31,-1,tempbuf,1);
            k += 6;
        }
}

int A_CheckInventorySprite(spritetype *s)
{
    switch (tileGetMapping(s->picnum))
    {
        case FIRSTAID__:
        case STEROIDS__:
        case HEATSENSOR__:
        case BOOTS__:
        case JETPACK__:
        case HOLODUKE__:
        case AIRTANK__:
            return 1;
    }
    return 0;
}

void G_DrawTXDigiNumZ(int starttile, int x,int y,int n,int s,int pal,int cs,int x1, int y1, int x2, int y2, int z)
{
    int i, j = 0, k, p, c;
    char b[10];
    int shift = (cs&ROTATESPRITE_FULL16)?0:16;

    //ltoa(n,b,10);
    Bsnprintf(b,10,"%d",n);
    i = Bstrlen(b);

    for (k=i-1;k>=0;k--)
    {
        p = starttile+*(b+k)-'0';
        j += (tilesiz[p].x+1)*z/65536;
    }
    if (cs&ROTATESPRITE_FULL16) j<<=16;
    c = x-(j>>1);

    j = 0;
    for (k=0;k<i;k++)
    {
        p = starttile+*(b+k)-'0';
        rotatesprite((c+j)<<shift,y<<shift,z,0,p,s,pal,2|cs,x1,y1,x2,y2);
        /*        rotatesprite((c+j)<<16,y<<16,65536L,0,p,s,pal,cs,0,0,xdim-1,ydim-1);
                rotatesprite(x<<16,y<<16,32768L,a,tilenum,shade,p,2|orientation,windowxy1.x,windowxy1.y,windowxy2.x,windowxy2.y);*/
        j += ((tilesiz[p].x+1)*z/65536)<<((cs&ROTATESPRITE_FULL16)?16:0);
    }
}

#define COLOR_RED 248
#define COLOR_WHITE 31
#define LOW_FPS 30

static void G_PrintFPS(void)
{
    static int32_t frameCount;

    static double cumulativeFrameDelay;
    static double lastFrameTime;
    static float lastFPS;

    double frameTime = timerGetFractionalTicks();
    double frameDelay = frameTime - lastFrameTime;
    cumulativeFrameDelay += frameDelay;

    if (frameDelay >= 0)
    {
        int x = (xdim <= 640);
        if (ud.showfps)
        {
            int32_t chars = Bsprintf(tempbuf, "%.1f ms, %5.1f fps", frameDelay, lastFPS);

            printext256(windowxy2.x-(chars<<(3-x))+1,windowxy1.y+2,0,-1,tempbuf,x);
            printext256(windowxy2.x-(chars<<(3-x)),windowxy1.y+1,
                        (lastFPS < LOW_FPS) ? COLOR_RED : COLOR_WHITE,-1,tempbuf,x);

            // lag meter
            if (numplayers > 1 && (totalclock - lastpackettime) > 1)
            {
                for (frameDelay = ((int32_t)totalclock - lastpackettime); frameDelay > 0 && frameDelay<(xdim>>2); frameDelay--)
                    printext256(4L*frameDelay,0,COLOR_WHITE,-1,".",0);
            }
        }

        if (cumulativeFrameDelay >= 1000.0)
        {
            lastFPS = 1000.f * frameCount / cumulativeFrameDelay;
            g_currentFrameRate = Blrintf(lastFPS);
            frameCount = 0;
            cumulativeFrameDelay = 0.0;
        }
        frameCount++;
    }

    lastFrameTime = frameTime;
}

static void G_PrintCoords(int snum)
{
    int const tsize = (xdim <= 640);
    int const x = windowxy1.x + 2;
    int y = windowxy1.y + 2;
    char ang[16], horiz[16], horizoff[16];
    auto const p = g_player[snum].ps;

    sprintf(tempbuf, "XYZ= (%d,%d,%d)", p->pos.x, p->pos.y, p->pos.z);
    printext256(x, y, COLOR_WHITE, -1, tempbuf, tsize);
    Bsprintf(tempbuf, "SPR Z= %d", TrackerCast(sprite[p->i].z));
    printext256(x, y += 9, COLOR_WHITE, -1, tempbuf, tsize);

    fix16_to_str(p->q16horiz, horiz, 2);
    fix16_to_str(p->q16horizoff, horizoff, 2);
    fix16_to_str(p->q16ang, ang, 2);
    Bsprintf(tempbuf, "A/H/HO= %s, %s, %s", ang, horiz, horizoff);
    printext256(x, y += 9, COLOR_WHITE, -1, tempbuf, tsize);

    Bsprintf(tempbuf, "XYZ Vel= (%d,%d,%d)", p->vel.x, p->vel.y, p->vel.z);
    printext256(x, y += 9, COLOR_WHITE, -1, tempbuf, tsize);

    Bsprintf(tempbuf, "F/S Vel= (%d,%d)", g_player[snum].inputBits->fvel, g_player[snum].inputBits->svel);
    printext256(x, y += 9, COLOR_WHITE, -1, tempbuf, tsize);

    Bsprintf(tempbuf, "SPR X/Zvel= (%d,%d)", TrackerCast(sprite[p->i].xvel), TrackerCast(sprite[p->i].zvel));
    printext256(x, y += 9, COLOR_WHITE, -1, tempbuf, tsize);

    Bsprintf(tempbuf, "RANDOMSEED= %d", randomseed);
    printext256(x, y += 18, COLOR_WHITE, -1, tempbuf, tsize);

    Bsprintf(tempbuf, "G_RANDOM.GLOBAL= %d", g_random.global);
    printext256(x, y += 9, COLOR_WHITE, -1, tempbuf, tsize);

    Bsprintf(tempbuf, "G_RANDOM.PLAYER= %d", g_random.player);
    printext256(x, y += 9, COLOR_WHITE, -1, tempbuf, tsize);

    Bsprintf(tempbuf, "G_RANDOM.PLAYERWEAPON= %d", g_random.playerweapon);
    printext256(x, y += 9, COLOR_WHITE, -1, tempbuf, tsize);

    Bsprintf(tempbuf, "THOLD= %d", p->transporter_hold);
    printext256(x, y += 18, COLOR_WHITE, -1, tempbuf, tsize);

    Bsprintf(tempbuf, "ONWARP= %d", p->on_warping_sector);
    printext256(x, y += 9, COLOR_WHITE, -1, tempbuf, tsize);

    Bsprintf(tempbuf, "OG= %d", p->on_ground);
    printext256(x, y += 9, COLOR_WHITE, -1, tempbuf, tsize);

    Bsprintf(tempbuf, "SBS/LS/HS= %d,%d,%d", p->sbs, p->lowsprite, p->highsprite);
    printext256(x, y += 9, COLOR_WHITE, -1, tempbuf, tsize);

    Bsprintf(tempbuf, "SECTL= %d", TrackerCast(sector[p->cursectnum].lotag));
    printext256(x, y += 9, COLOR_WHITE, -1, tempbuf, tsize);

    Bsprintf(tempbuf, "MOVEFIFOPLC= %d", movefifoplc);
    printext256(x, y += 18, COLOR_WHITE, -1, tempbuf, tsize);
    Bsprintf(tempbuf, "MOVEFIFOEND= %d", g_player[snum].movefifoend);
    printext256(x, y += 8, COLOR_WHITE, -1, tempbuf, tsize);
    Bsprintf(tempbuf, "MOVEFIFOSENDPLC= %d", movefifosendplc);
    printext256(x, y += 8, COLOR_WHITE, -1, tempbuf, tsize);

    Bsprintf(tempbuf, "TOTALCLOCK= %d", (int32_t)totalclock);
    printext256(x, y += 18, COLOR_WHITE, -1, tempbuf, tsize);

    Bsprintf(tempbuf, "OTOTALCLOCK= %d", (int32_t)ototalclock);
    printext256(x, y += 8, (ototalclock > totalclock) ? COLOR_RED : COLOR_WHITE, -1, tempbuf, tsize);
}

// this handles both multiplayer and item pickup message type text
// both are passed on to gametext

static void G_PrintGameQuotes(void)
{
    int i, j, k, l;

    
    k = 1;
    k += G_GetFragbarOffset();

    if (g_player[screenpeek].ps->fta > 1 && (g_player[screenpeek].ps->ftq < QUOTE_RESERVED || g_player[screenpeek].ps->ftq > QUOTE_RESERVED3))
    {
        if (g_player[screenpeek].ps->fta > 6)
            k += 7;
        else k += g_player[screenpeek].ps->fta; /*if (g_player[screenpeek].ps->fta > 2)
            k += 3;
        else k += 1; */
    }

    /*    if (xdim >= 640 && ydim >= 480)
            k = scale(k,ydim,200); */

    j = k;

//    quotebot = min(quotebot,j);
    //  quotebotgoal = min(quotebotgoal,j);
//    if (ud.gm&MODE_TYPE) j -= 8;
    //quotebotgoal = j;
    //j = quotebot;
    j = scale(j,ydim,200);
    for (i=MAXUSERQUOTES-1;i>=0;i--)
    {
        if (user_quote_time[i] <= 0) continue;
        k = user_quote_time[i];
        if (hud_glowingquotes)
        {
            if (k > 4) { mpgametext(j,user_quote[i],(sintable[(((int32_t)totalclock+(i<<2))<<5)&2047]>>11),2+8+16); j += textsc(8); }
            else if (k > 2) { mpgametext(j,user_quote[i],(sintable[(((int32_t)totalclock+(i<<2))<<5)&2047]>>11),2+8+16+1); j += textsc(k<<1); }
            else { mpgametext(j,user_quote[i],(sintable[(((int32_t)totalclock+(i<<2))<<5)&2047]>>11),2+8+16+1+32); j += textsc(k<<1); }
        }
        else
        {
            if (k > 4) { mpgametext(j,user_quote[i],0,2+8+16); j += textsc(8); }
            else if (k > 2) { mpgametext(j,user_quote[i],0,2+8+16+1); j += textsc(k<<1); }
            else { mpgametext(j,user_quote[i],0,2+8+16+1+32); j += textsc(k<<1); }
        }
        l = G_GameTextLen(USERQUOTE_LEFTOFFSET,OSD_StripColors(tempbuf,user_quote[i]));
        while (l > (ud.config.ScreenWidth - USERQUOTE_RIGHTOFFSET))
        {
            l -= (ud.config.ScreenWidth-USERQUOTE_RIGHTOFFSET);
            if (k > 4) j += textsc(8);
            else j += textsc(k<<1);

        }
    }

    if ((klabs(quotebotgoal-quotebot) <= 16) && (ud.screen_size <= 8))
        quotebot += ksgn(quotebotgoal-quotebot);
    else
        quotebot = quotebotgoal;

    if (g_player[screenpeek].ps->fta <= 1) return;

    if (apStrings[g_player[screenpeek].ps->ftq] == NULL)
    {
        OSD_Printf(OSD_ERROR "%s %d null quote %d\n",__FILE__,__LINE__,g_player[screenpeek].ps->ftq);
        return;
    }

    k = 0;

    k += G_GetFragbarOffset();

    if (g_player[screenpeek].ps->ftq == QUOTE_RESERVED || g_player[screenpeek].ps->ftq == QUOTE_RESERVED2)
    {
        k = 140;//quotebot-8-4;
    }

    j = g_player[screenpeek].ps->fta;
    if (!hud_glowingquotes)
    {
        if (j > 4) gametext(320>>1,k,apStrings[g_player[screenpeek].ps->ftq],0,2+8+16);
        else if (j > 2) gametext(320>>1,k,apStrings[g_player[screenpeek].ps->ftq],0,2+8+16+1);
        else gametext(320>>1,k,apStrings[g_player[screenpeek].ps->ftq],0,2+8+16+1+32);
        return;
    }
    if (j > 4) gametext(320>>1,k,apStrings[g_player[screenpeek].ps->ftq],(sintable[((int32_t)totalclock<<5)&2047]>>11),2+8+16);
    else if (j > 2) gametext(320>>1,k,apStrings[g_player[screenpeek].ps->ftq],(sintable[((int32_t)totalclock<<5)&2047]>>11),2+8+16+1);
    else gametext(320>>1,k,apStrings[g_player[screenpeek].ps->ftq],(sintable[((int32_t)totalclock<<5)&2047]>>11),2+8+16+1+32);
}

void P_DoQuote(int q, DukePlayer_t *p)
{
    if (oldnet_predicting)
        return;

    int cq = 0;

    if (q & MAXQUOTES)
    {
        cq = 1;
        q &= ~MAXQUOTES;
    }

    if (apStrings[q] == NULL)
    {
        OSD_Printf(OSD_ERROR "%s %d null quote %d\n",__FILE__,__LINE__,q);
        return;
    }

    if (ud.fta_on == 0)
        return;

    if (p->fta > 0 && q != QUOTE_RESERVED && q != QUOTE_RESERVED2)
        if (p->ftq == QUOTE_RESERVED || p->ftq == QUOTE_RESERVED2) return;

    p->fta = 100;

    //            if(p->ftq != q || q == 26)
    // || q == 26 || q == 115 || q ==116 || q == 117 || q == 122)
    {
        if (p->ftq != q)
            if (p == g_player[screenpeek].ps)
            {
                if (cq) OSD_Printf(OSDTEXT_BLUE "%s\n",apStrings[q]);
                else OSD_Printf("%s\n",apStrings[q]);
            }

        p->ftq = q;
        pub = NUMPAGES;
        pus = NUMPAGES;
    }
}

static void G_DisplayExtraScreens(void)
{
    int flags = Gv_GetVarByLabel("LOGO_FLAGS",255, -1, -1);

    S_StopMusic();
    FX_StopAllSounds();

    if (!VOLUMEALL || flags & LOGO_SHAREWARESCREENS)
    {
        videoSetViewableArea(0,0,xdim-1,ydim-1);
        renderFlushPerms();
        //g_player[myconnectindex].ps->palette = palette;
        P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 1);    // JBF 20040308
        fadepal(0,0,0, 0,64,7);
        KB_FlushKeyboardQueue();
        rotatesprite(0,0,65536L,0,3291,0,0,2+8+16+64, 0,0,xdim-1,ydim-1);
        IFISSOFTMODE fadepal(0,0,0, 63,0,-7);
        else videoNextPage();

        while (!KB_KeyWaiting())
            gameHandleEvents();

        fadepal(0,0,0, 0,64,7);
        KB_FlushKeyboardQueue();
        rotatesprite(0,0,65536L,0,3290,0,0,2+8+16+64, 0,0,xdim-1,ydim-1);
        IFISSOFTMODE fadepal(0,0,0, 63,0,-7);
        else videoNextPage();

        while (!KB_KeyWaiting())
            gameHandleEvents();
    }

    if (flags & LOGO_TENSCREEN)
    {
        videoSetViewableArea(0,0,xdim-1,ydim-1);
        renderFlushPerms();
        //g_player[myconnectindex].ps->palette = palette;
        P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 1);    // JBF 20040308
        fadepal(0,0,0, 0,64,7);
        KB_FlushKeyboardQueue();
        rotatesprite(0,0,65536L,0,TENSCREEN,0,0,2+8+16+64, 0,0,xdim-1,ydim-1);
        IFISSOFTMODE fadepal(0,0,0, 63,0,-7);
        else videoNextPage();

        while (!KB_KeyWaiting() && totalclock < 2400)
            gameHandleEvents();
    }
}

extern int g_doQuickSave;

void G_GameExit(const char *t)
{
    if (*t != 0 && g_player[myconnectindex].ps != NULL)
        g_player[myconnectindex].ps->palette = BASEPAL;

    if (ud.recstat == 1) G_CloseDemoWrite();
    else if (ud.recstat == 2)
    {
        if (frecfilep) fclose(frecfilep);
    } // JBF: fixes crash on demo playback

    if (numplayers > 1)
        Net_Disconnect((*t == ' '));

    if (!quickExit)
    {
        if (*t != 0 && *(t+1) != 'V' && *(t+1) != 'Y')
            G_DisplayExtraScreens();
    }

    if (*t != 0) LOG_F(INFO, "%s",t);

    if (in3dmode())
        G_Shutdown();

    if (*t != 0)
    {
        //setvmode(0x3);    // JBF
        //binscreen();
        //            if(*t == ' ' && *(t+1) == 0) *t = 0;
        //printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
        if (!(t[0] == ' ' && t[1] == 0))
        {
            char titlebuf[256];
            Bsprintf(titlebuf,HEAD2 " (%s)",s_buildTimestamp);
            wm_msgbox(titlebuf, "%s", t);
        }
    }

    //unlink("duke3d.tmp");

    Bexit(EXIT_SUCCESS);
}

char inputloc = 0;

int _EnterText(int small,int x,int y,char *t,int dalen,int c)
{
    char ch;
    int i;

    while ((ch = KB_GetCh()) != 0 || (ud.gm&MODE_MENU && (MOUSE_GetButtons()&M_RIGHTBUTTON)))
    {
        if (ch == asc_BackSpace)
        {
            if (inputloc > 0)
            {
                inputloc--;
                *(t+inputloc) = 0;
            }
        }
        else
        {
            if (ch == asc_Enter)
            {
                KB_ClearKeyDown(sc_Enter);
                KB_ClearKeyDown(sc_kpad_Enter);
                return (1);
            }
            else if (ch == asc_Escape || (ud.gm&MODE_MENU && (MOUSE_GetButtons()&M_RIGHTBUTTON)))
            {
                KB_ClearKeyDown(sc_Escape);
                MOUSE_ClearButton(M_RIGHTBUTTON);
                return (-1);
            }
            else if (ch >= 32 && inputloc < dalen && ch < 127)
            {
                //ch = Btoupper(ch);
                if (c != 997 || (ch >= '0' && ch <= '9'))
                {
                    // JBF 20040508: so we can have numeric only if we want
                    *(t+inputloc) = ch;
                    *(t+inputloc+1) = 0;
                    inputloc++;
                }
            }
        }
    }

    if (c == 999) return(0);
    if (c == 998)
    {
        char b[91],ii;
        for (ii=0;ii<inputloc;ii++)
            b[(unsigned char)ii] = '*';
        b[(unsigned char)inputloc] = 0;
        if (ud.gm&MODE_TYPE)
            x = mpgametext(y,b,c,2+8+16);
        else x = gametext(x,y,b,c,2+8+16);
    }
    else
    {
        if (ud.gm&MODE_TYPE)
            x = mpgametext(y,t,c,2+8+16);
        else x = gametext(x,y,t,c,2+8+16);
    }
    c = 4-(sintable[((int32_t)totalclock<<4)&2047]>>11);

    i = G_GameTextLen(USERQUOTE_LEFTOFFSET, OSD_StripColors(tempbuf,t));
    while (i > (ud.config.ScreenWidth - USERQUOTE_RIGHTOFFSET))
    {
        i -= (ud.config.ScreenWidth - USERQUOTE_RIGHTOFFSET);
        if (small&1)
            y += textsc(6);
        y += 8;
    }

    if (small&1)
        rotatesprite(textsc(x)<<16,(y<<16),32768,0,SPINNINGNUKEICON+(((int32_t)totalclock>>3)%7),c,0,(small&1)?(8|16):2+8,0,0,xdim-1,ydim-1);
    else rotatesprite((x+((small&1)?4:8))<<16,((y+((small&1)?0:4))<<16),32768,0,SPINNINGNUKEICON+(((int32_t)totalclock>>3)%7),c,0,(small&1)?(8|16):2+8,0,0,xdim-1,ydim-1);
    return (0);
}

static inline void G_MoveClouds(void)
{
    if (totalclock <= cloudtotalclock && totalclock >= (cloudtotalclock - 7))
        return;

    int i;

    cloudtotalclock = (int32_t)totalclock+6;

    g_cloudX += (sintable[(fix16_to_int(g_player[screenpeek].ps->q16ang) + 512) & 2047] >> 9);
    g_cloudY += (sintable[fix16_to_int(g_player[screenpeek].ps->q16ang) & 2047] >> 9);

    for (i=g_numClouds-1;i>=0;i--)
    {
        sector[g_cloudSect[i]].ceilingxpanning = g_cloudX >> 6;
        sector[g_cloudSect[i]].ceilingypanning = g_cloudY >> 6;
    }
}

static void G_DrawOverheadMap(int cposx, int cposy, int czoom, short cang)
{
    int i, j, k, l, x1, y1, x2=0, y2=0, x3, y3, x4, y4, ox, oy, xoff, yoff;
    int dax, day, cosang, sinang, xspan, yspan, sprx, spry;
    int xrepeat, yrepeat, z1, z2, startwall, endwall, tilenum, daang;
    int xvect, yvect, xvect2, yvect2;
    short p;
    char col;
    walltype *wal, *wal2;
    spritetype *spr;
    
    int32_t tmpydim = (xdim * 5) / 8;
    renderSetAspect(65536, divscale16(tmpydim * 320, xdim * 200));

    xvect = sintable[(-cang)&2047] * czoom;
    yvect = sintable[(1536-cang)&2047] * czoom;
    xvect2 = mulscale16(xvect,yxaspect);
    yvect2 = mulscale16(yvect,yxaspect);

    //Draw red lines
    for (i=numsectors-1;i>=0;i--)
    {
        if (!(show2dsector[i>>3]&(1<<(i&7)))) continue;

        startwall = sector[i].wallptr;
        endwall = sector[i].wallptr + sector[i].wallnum;

        z1 = sector[i].ceilingz;
        z2 = sector[i].floorz;

        for (j=startwall,wal=&wall[startwall];j<endwall;j++,wal++)
        {
            k = wal->nextwall;
            if (k < 0) continue;

            //if ((show2dwall[j>>3]&(1<<(j&7))) == 0) continue;
            //if ((k > j) && ((show2dwall[k>>3]&(1<<(k&7))) > 0)) continue;

            if (sector[wal->nextsector].ceilingz == z1)
                if (sector[wal->nextsector].floorz == z2)
                    if (((wal->cstat|wall[wal->nextwall].cstat)&(16+32)) == 0) continue;

            col = 139; //red
            if ((wal->cstat|wall[wal->nextwall].cstat)&1) col = 234; //magenta

            if (!(show2dsector[wal->nextsector>>3]&(1<<(wal->nextsector&7))))
                col = 24;
            else continue;

            ox = wal->x-cposx;
            oy = wal->y-cposy;
            x1 = dmulscale16(ox,xvect,-oy,yvect)+(xdim<<11);
            y1 = dmulscale16(oy,xvect2,ox,yvect2)+(ydim<<11);

            wal2 = &wall[wal->point2];
            ox = wal2->x-cposx;
            oy = wal2->y-cposy;
            x2 = dmulscale16(ox,xvect,-oy,yvect)+(xdim<<11);
            y2 = dmulscale16(oy,xvect2,ox,yvect2)+(ydim<<11);

            renderDrawLine(x1,y1,x2,y2,col);
        }
    }

    //Draw sprites
    k = g_player[screenpeek].ps->i;
    for (i=numsectors-1;i>=0;i--)
    {
        if (!(show2dsector[i>>3]&(1<<(i&7)))) continue;
        for (j=headspritesect[i];j>=0;j=nextspritesect[j])
            //if ((show2dsprite[j>>3]&(1<<(j&7))) > 0)
        {
            spr = &sprite[j];

            if (j == k || (spr->cstat&0x8000) || spr->cstat == 257 || spr->xrepeat == 0) continue;

            col = 71; //cyan
            if (spr->cstat&1) col = 234; //magenta

            sprx = spr->x;
            spry = spr->y;

            if ((spr->cstat&257) != 0) switch (spr->cstat&48)
                {
                case 0:
                    break;

                    ox = sprx-cposx;
                    oy = spry-cposy;
                    x1 = dmulscale16(ox,xvect,-oy,yvect);
                    y1 = dmulscale16(oy,xvect2,ox,yvect2);

                    ox = (sintable[(spr->ang+512)&2047]>>7);
                    oy = (sintable[(spr->ang)&2047]>>7);
                    x2 = dmulscale16(ox,xvect,-oy,yvect);
                    y2 = dmulscale16(oy,xvect,ox,yvect);

                    x3 = mulscale16(x2,yxaspect);
                    y3 = mulscale16(y2,yxaspect);

                    renderDrawLine(x1-x2+(xdim<<11),y1-y3+(ydim<<11),
                                x1+x2+(xdim<<11),y1+y3+(ydim<<11),col);
                    renderDrawLine(x1-y2+(xdim<<11),y1+x3+(ydim<<11),
                                x1+x2+(xdim<<11),y1+y3+(ydim<<11),col);
                    renderDrawLine(x1+y2+(xdim<<11),y1-x3+(ydim<<11),
                                x1+x2+(xdim<<11),y1+y3+(ydim<<11),col);
                    break;

                case 16:
                    if (spr->picnum == LASERLINE)
                    {
                        x1 = sprx;
                        y1 = spry;
                        tilenum = spr->picnum;
                        xoff = picanm[tilenum].xofs + spr->xoffset;
                        if ((spr->cstat&4) > 0) xoff = -xoff;
                        k = spr->ang;
                        l = spr->xrepeat;
                        dax = sintable[k&2047]*l;
                        day = sintable[(k+1536)&2047]*l;
                        l = tilesiz[tilenum].x;
                        k = (l>>1)+xoff;
                        x1 -= mulscale16(dax,k);
                        x2 = x1+mulscale16(dax,l);
                        y1 -= mulscale16(day,k);
                        y2 = y1+mulscale16(day,l);

                        ox = x1-cposx;
                        oy = y1-cposy;
                        x1 = dmulscale16(ox,xvect,-oy,yvect);
                        y1 = dmulscale16(oy,xvect2,ox,yvect2);

                        ox = x2-cposx;
                        oy = y2-cposy;
                        x2 = dmulscale16(ox,xvect,-oy,yvect);
                        y2 = dmulscale16(oy,xvect2,ox,yvect2);

                        renderDrawLine(x1+(xdim<<11),y1+(ydim<<11),
                                    x2+(xdim<<11),y2+(ydim<<11),col);
                    }

                    break;

                case 32:

                    tilenum = spr->picnum;
                    xoff = picanm[tilenum].xofs;
                    yoff = picanm[tilenum].yofs;
                    if ((spr->cstat&4) > 0) xoff = -xoff;
                    if ((spr->cstat&8) > 0) yoff = -yoff;

                    k = spr->ang;
                    cosang = sintable[(k+512)&2047];
                    sinang = sintable[k];
                    xspan = tilesiz[tilenum].x;
                    xrepeat = spr->xrepeat;
                    yspan = tilesiz[tilenum].y;
                    yrepeat = spr->yrepeat;

                    dax = ((xspan>>1)+xoff)*xrepeat;
                    day = ((yspan>>1)+yoff)*yrepeat;
                    x1 = sprx + dmulscale16(sinang,dax,cosang,day);
                    y1 = spry + dmulscale16(sinang,day,-cosang,dax);
                    l = xspan*xrepeat;
                    x2 = x1 - mulscale16(sinang,l);
                    y2 = y1 + mulscale16(cosang,l);
                    l = yspan*yrepeat;
                    k = -mulscale16(cosang,l);
                    x3 = x2+k;
                    x4 = x1+k;
                    k = -mulscale16(sinang,l);
                    y3 = y2+k;
                    y4 = y1+k;

                    ox = x1-cposx;
                    oy = y1-cposy;
                    x1 = dmulscale16(ox,xvect,-oy,yvect);
                    y1 = dmulscale16(oy,xvect2,ox,yvect2);

                    ox = x2-cposx;
                    oy = y2-cposy;
                    x2 = dmulscale16(ox,xvect,-oy,yvect);
                    y2 = dmulscale16(oy,xvect2,ox,yvect2);

                    ox = x3-cposx;
                    oy = y3-cposy;
                    x3 = dmulscale16(ox,xvect,-oy,yvect);
                    y3 = dmulscale16(oy,xvect2,ox,yvect2);

                    ox = x4-cposx;
                    oy = y4-cposy;
                    x4 = dmulscale16(ox,xvect,-oy,yvect);
                    y4 = dmulscale16(oy,xvect2,ox,yvect2);

                    renderDrawLine(x1+(xdim<<11),y1+(ydim<<11),
                                x2+(xdim<<11),y2+(ydim<<11),col);

                    renderDrawLine(x2+(xdim<<11),y2+(ydim<<11),
                                x3+(xdim<<11),y3+(ydim<<11),col);

                    renderDrawLine(x3+(xdim<<11),y3+(ydim<<11),
                                x4+(xdim<<11),y4+(ydim<<11),col);

                    renderDrawLine(x4+(xdim<<11),y4+(ydim<<11),
                                x1+(xdim<<11),y1+(ydim<<11),col);

                    break;
                }
        }
    }

    //Draw white lines
    for (i=numsectors-1;i>=0;i--)
    {
        if (!(show2dsector[i>>3]&(1<<(i&7)))) continue;

        startwall = sector[i].wallptr;
        endwall = sector[i].wallptr + sector[i].wallnum;

        k = -1;
        for (j=startwall,wal=&wall[startwall];j<endwall;j++,wal++)
        {
            if (wal->nextwall >= 0) continue;

            //if ((show2dwall[j>>3]&(1<<(j&7))) == 0) continue;

            if (tilesiz[wal->picnum].x == 0) continue;
            if (tilesiz[wal->picnum].y == 0) continue;

            if (j == k)
            {
                x1 = x2;
                y1 = y2;
            }
            else
            {
                ox = wal->x-cposx;
                oy = wal->y-cposy;
                x1 = dmulscale16(ox,xvect,-oy,yvect)+(xdim<<11);
                y1 = dmulscale16(oy,xvect2,ox,yvect2)+(ydim<<11);
            }

            k = wal->point2;
            wal2 = &wall[k];
            ox = wal2->x-cposx;
            oy = wal2->y-cposy;
            x2 = dmulscale16(ox,xvect,-oy,yvect)+(xdim<<11);
            y2 = dmulscale16(oy,xvect2,ox,yvect2)+(ydim<<11);

            renderDrawLine(x1,y1,x2,y2,24);
        }
    }

    videoSetCorrectedAspect();

    for(ALL_PLAYERS(p))
    {
        if (ud.scrollmode && p == screenpeek) continue;

        ox = sprite[g_player[p].ps->i].x-cposx;
        oy = sprite[g_player[p].ps->i].y-cposy;
        daang = (sprite[g_player[p].ps->i].ang-cang)&2047;
        if (p == screenpeek)
        {
            ox = 0;
            oy = 0;
            daang = 0;
        }
        x1 = mulscale(ox,xvect,16) - mulscale(oy,yvect,16);
        y1 = mulscale(oy,xvect2,16) + mulscale(ox,yvect2,16);

        if (p == screenpeek || GTFLAGS(GAMETYPE_OTHERPLAYERSINMAP))
        {
            if (sprite[g_player[p].ps->i].xvel > 16 && g_player[p].ps->on_ground)
                i = APLAYERTOP+(((int32_t)totalclock>>4)&3);
            else
                i = APLAYERTOP;

            j = klabs(g_player[p].ps->truefz-g_player[p].ps->pos.z)>>8;
            j = mulscale(czoom*(sprite[g_player[p].ps->i].yrepeat+j),yxaspect,16);

            if (j < 22000) j = 22000;
            else if (j > (65536<<1)) j = (65536<<1);

            rotatesprite((x1<<4)+(xdim<<15),(y1<<4)+(ydim<<15),j,daang,i,sprite[g_player[p].ps->i].shade,
                         (g_player[p].ps->cursectnum > -1)?sector[g_player[p].ps->cursectnum].floorpal:0,
                         (sprite[g_player[p].ps->i].cstat&2)>>1,windowxy1.x,windowxy1.y,windowxy2.x,windowxy2.y);
        }
    }
}

////////// TINT ACCUMULATOR //////////

typedef struct {
    int32_t r, g, b;
    // f: 0-63 scale
    int32_t maxf, sumf;
} palaccum_t;

#define PALACCUM_INITIALIZER { 0, 0, 0, 0, 0 }

/* For a picture frame F and n tints C_1, C_2, ... C_n weighted a_1, a_2,
* ... a_n (on a 0-1 scale), the faded frame is calculated as
*
*    F_new := (1-max_i(a_i))*F + d*sum_i(a_i), where
*
*    d := max_i(a_i)/sum_i(a_i).
*
* This means that
*  1) tint application is independent of their order.
*  2) going from n+1 to n tints is continuous when the leaving one has faded.
*
* But note that for more than one tint, the composite tint will in general
* change its hue as the ratio of the weights of the individual ones changes.
*/
static void palaccum_add(palaccum_t* pa, const palette_t* pal, int32_t f)
{
    f = clamp(f, 0, 63);
    if (f == 0)
        return;

    pa->maxf = max(pa->maxf, f);
    pa->sumf += f;

    pa->r += f * clamp(pal->r, 0, 63);
    pa->g += f * clamp(pal->g, 0, 63);
    pa->b += f * clamp(pal->b, 0, 63);
}

static void G_FadePalaccum(const palaccum_t* pa)
{
    videoFadePalette(tabledivide32_noinline(pa->r, pa->sumf) << 2,
        tabledivide32_noinline(pa->g, pa->sumf) << 2,
        tabledivide32_noinline(pa->b, pa->sumf) << 2, pa->maxf << 2);
}

void G_DisplayRest(int smoothratio)
{
    int a, i, j;
    palaccum_t tint = PALACCUM_INITIALIZER;

    DukePlayer_t *pp = g_player[screenpeek].ps;
    walltype *wal;
    int cposx,cposy,cang;

#if defined(USE_OPENGL)
    // this takes care of fullscreen tint for OpenGL
    if (videoGetRenderMode() >= 3)
    {
        polytint_t& fstint = hictinting[MAXPALOOKUPS - 1];
        if (pp->palette == WATERPAL)
        {
            fstint.r = 224;
            fstint.g = 192;
            fstint.b = 255;
            fstint.f = 0;
        }
        else if (pp->palette == SLIMEPAL)
        {
            fstint.r = 208;
            fstint.g = 255;
            fstint.b = 192;
            fstint.f = 0;
        }
        else
        {
            fstint.r = 255;
            fstint.g = 255;
            fstint.b = 255;
            fstint.f = 0;
        }
    }
#endif /* USE_OPENGL && POLYMOST */

    rotatespritesmoothratio = smoothratio;

    // this does pain tinting etc from the CON
    palaccum_add(&tint, &pp->pals, pp->pals.f);

    static const palette_t loogiepal = { 0, 63, 0, 0 };
    palaccum_add(&tint, &loogiepal, pp->loogcnt >> 1);

    // reset a normal palette
    if (g_restorePalette)
    {
        //videoSetPalette(ud.brightness>>2,&pp->palette[0],0);
        P_SetGamePalette(pp, pp->palette, 2);
        g_restorePalette = 0;
    }

    if (ud.show_help)
    {
        switch (ud.show_help)
        {
        case 1:
            rotatesprite(0,0,65536L,0,TEXTSTORY,0,0,10+16+64, 0,0,xdim-1,ydim-1);
            break;
        case 2:
            rotatesprite(0,0,65536L,0,F1HELP,0,0,10+16+64, 0,0,xdim-1,ydim-1);
            break;
        }

        if (KB_KeyPressed(sc_Escape) || (MOUSE_GetButtons()&M_RIGHTBUTTON))
        {
            KB_ClearKeyDown(sc_Escape);
            MOUSE_ClearButton(M_RIGHTBUTTON);
            ud.show_help = 0;
            if (ud.multimode < 2 && ud.recstat != 2)
            {
                ready2send = 1;
                totalclock = ototalclock;
            }
            G_UpdateScreenArea();
        }
        return;
    }

    i = pp->cursectnum;
    if (i > -1)
    {
        show2dsector[i>>3] |= (1<<(i&7));
        wal = &wall[sector[i].wallptr];
        for (j=sector[i].wallnum;j>0;j--,wal++)
        {
            i = wal->nextsector;
            if (i < 0) continue;
            if (wal->cstat&0x0071) continue;
            if (wall[wal->nextwall].cstat&0x0071) continue;
            if (sector[i].lotag == 32767) continue;
            if (sector[i].ceilingz >= sector[i].floorz) continue;
            show2dsector[i>>3] |= (1<<(i&7));
        }
    }

    if (ud.camerasprite == -1)
    {
        if (ud.overhead_on != 2)
        {
            if (pp->newowner >= 0)
                G_DrawCameraText(pp->newowner);
            else
            {
                P_DisplayWeapon(screenpeek);

                if (pp->over_shoulder_on == 0)
                    P_DisplayScubaMask(screenpeek);
            }
            G_MoveClouds();
        }

        if (ud.overhead_on > 0)
        {
            // smoothratio = min(max(smoothratio,0),65536);
            smoothratio = calc_smoothratio(totalclock, ototalclock);
            G_DoInterpolations(smoothratio);
            if (ud.scrollmode == 0)
            {
                if (pp->newowner == -1 && !ud.pause_on)
                {
                    DukePlayer_t* camPlayer = (screenpeek == myconnectindex && numplayers > 1) ? &predictedPlayer : pp;

                    cposx = camPlayer->opos.x+mulscale16((int)(camPlayer->pos.x - camPlayer->opos.x),smoothratio);
                    cposy = camPlayer->opos.y+mulscale16((int)(camPlayer->pos.y - camPlayer->opos.y),smoothratio);
                    cang = fix16_to_int(camPlayer->oq16ang) + mulscale16((fix16_to_int(camPlayer->q16ang + F16(1024) - camPlayer->oq16ang) & 2047) - 1024, smoothratio);
                }
                else
                {
                    cposx = pp->opos.x;
                    cposy = pp->opos.y;
                    cang = fix16_to_int(pp->oq16ang);
                }
            }
            else
            {
                if (!ud.pause_on)
                {
                    ud.fola += ud.folavel>>3;
                    ud.folx += (ud.folfvel*sintable[(512+2048-ud.fola)&2047])>>14;
                    ud.foly += (ud.folfvel*sintable[(512+1024-512-ud.fola)&2047])>>14;
                }
                cposx = ud.folx;
                cposy = ud.foly;
                cang = ud.fola;
            }

            if (ud.overhead_on == 2)
            {
                videoClearViewableArea(0L);
                renderDrawMapView(cposx,cposy,pp->zoom,cang);
            }
            G_DrawOverheadMap(cposx,cposy,pp->zoom,cang);

            G_RestoreInterpolations();

            if (ud.overhead_on == 2)
            {
                if (ud.screen_size > 0) a = 147;
                else a = 179;
                minitext(5,a+6,EpisodeNames[ud.volume_number],0,2+8+16);
                minitext(5,a+6+6,g_mapInfo[ud.volume_number*MAXLEVELS + ud.level_number].name,0,2+8+16);
            }
        }
    }

    if (pp->invdisptime > 0)
        G_DrawInventory(pp);

    G_DrawFrags();

    Gv_SetVar(sysVarIDs.RETURN,0,g_player[screenpeek].ps->i,screenpeek);
    X_OnEvent(EVENT_DISPLAYSBAR, g_player[screenpeek].ps->i, screenpeek, -1);
    if (Gv_GetVar(sysVarIDs.RETURN,g_player[screenpeek].ps->i,screenpeek) == 0)
        G_DrawStatusBar(screenpeek);

    G_PrintGameQuotes();
    G_PrintLevelName();

    if (KB_KeyPressed(sc_Escape) && ud.overhead_on == 0
            && ud.show_help == 0
            && g_player[myconnectindex].ps->newowner == -1)
    {
        if ((ud.gm&MODE_MENU) == MODE_MENU && g_currentMenu < 51)
        {
            KB_ClearKeyDown(sc_Escape);
            S_PlaySound(EXITMENUSOUND);
            ud.gm &= ~MODE_MENU;
            if (ud.multimode < 2 && ud.recstat != 2)
            {
                ready2send = 1;
                totalclock = ototalclock;
                g_cameraClock = (int32_t)totalclock;
                g_cameraDistance = 65536L;
            }
            walock[TILE_SAVESHOT] = 199;
            G_UpdateScreenArea();
        }
        else if ((ud.gm&MODE_MENU) != MODE_MENU &&
                 g_player[myconnectindex].ps->newowner == -1 &&
                 (ud.gm&MODE_TYPE) != MODE_TYPE)
        {
            KB_ClearKeyDown(sc_Escape);
            FX_StopAllSounds();

            S_MenuSound();

            ud.gm |= MODE_MENU;

            if (ud.multimode < 2 && ud.recstat != 2)
                ready2send = 0;

            if (ud.gm&MODE_GAME)
                ChangeToMenu(50);
            else
                ChangeToMenu(0);

            screenpeek = myconnectindex;
        }
    }

    G_DisplayPlayerId();

    X_OnEvent(EVENT_DISPLAYREST, g_player[screenpeek].ps->i, screenpeek, -1);

    G_DrawCrosshair();
#if 0
    if (Gametypes[ud.coop].flags & GAMETYPE_TDM)
    {
        for (ALL_PLAYERS(i))
        {
            if (g_player[i].ps->team == g_player[myconnectindex].ps->team)
            {
                j = min(max((G_GetAngleDelta(getangle(g_player[i].ps->pos.x-g_player[myconnectindex].ps->pos.x,g_player[i].ps->pos.y-g_player[myconnectindex].ps->pos.y),g_player[myconnectindex].ps->ang))>>1,-160),160);
                rotatesprite((160-j)<<16,100L<<16,65536L,0,DUKEICON,0,0,2+1,windowxy1.x,windowxy1.y,windowxy2.x,windowxy2.y);
            }
        }
    }
#endif

    if (ud.pause_on==1 && (ud.gm&MODE_MENU) == 0)
        menutext(160,100,0,0,"GAME PAUSED");

    if (ud.coords)
        G_PrintCoords(screenpeek);

#if defined(USE_OPENGL)
    {
        extern int mdpause;

        mdpause = 0;
        if (ud.pause_on || (ud.gm&MODE_MENU && numplayers < 2))
            mdpause = 1;
    }
#endif

    G_PrintFPS();
    G_DrawLevelStats();

    {
        static int32_t applied = 0;

        if (tint.maxf)
        {
            G_FadePalaccum(&tint);
            applied = 1;
        }
        else if (applied)
        {
            // be sure to always un-apply a tint.
            videoFadePalette(0, 0, 0, 0);
            applied = 0;
        }
    }
}

static void G_DoThirdPerson(DukePlayer_t *pp, vec3_t *vect, short *vsectnum, int16_t ang, int16_t horiz)
{
    spritetype *sp = &sprite[pp->i];
    int i, hx, hy;
    int nx = (sintable[(ang+1536)&2047]>>4);
    int ny = (sintable[(ang+1024)&2047]>>4);
    int nz = (horiz - 100) * 128;
    int32_t daang;
    short bakcstat = sp->cstat;

    hitdata_t hit;

    sp->cstat &= (short)~0x101;

    updatesectorz(vect->x, vect->y, vect->z, vsectnum);

    hitscan(vect,*vsectnum,nx,ny,nz,&hit,CLIPMASK1);

    if (*vsectnum < 0)
    {
        sp->cstat = bakcstat;
        return;
    }

    hx = hit.x - (vect->x);
    hy = hit.y - (vect->y);

    if (klabs(nx)+klabs(ny) > klabs(hx)+klabs(hy))
    {
        *vsectnum = hit.sect;
        if (hit.wall >= 0)
        {
            daang = getangle(wall[wall[hit.wall].point2].x-wall[hit.wall].x,
                             wall[wall[hit.wall].point2].y-wall[hit.wall].y);

            i = nx*sintable[daang]+ny*sintable[(daang+1536)&2047];
            if (klabs(nx) > klabs(ny)) hx -= mulscale28(nx,i);
            else hy -= mulscale28(ny,i);
        }
        else if (hit.sprite < 0)
        {
            if (klabs(nx) > klabs(ny)) hx -= (nx>>5);
            else hy -= (ny>>5);
        }
        if (klabs(nx) > klabs(ny)) i = divscale16(hx,nx);
        else i = divscale16(hy,ny);
        if (i < g_cameraDistance) g_cameraDistance = i;
    }

    vect->x += mulscale16(nx, g_cameraDistance);
    vect->y += mulscale16(ny, g_cameraDistance);
    vect->z += mulscale16(nz, g_cameraDistance);

    g_cameraDistance = min(g_cameraDistance+(((int32_t)totalclock-g_cameraClock)<<10),65536);
    g_cameraClock = (int32_t)totalclock;

    updatesectorz(vect->x, vect->y, vect->z,vsectnum);

    sp->cstat = bakcstat;
}

//REPLACE FULLY
void G_DrawBackground(void)
{
    int dapicnum;
    int x,y,x1,y1,x2,y2,rx;

    renderFlushPerms();

    switch (ud.m_volume_number)
    {
    default:
        dapicnum = BIGHOLE;
        break;
    case 1:
        dapicnum = BIGHOLE;
        break;
    case 2:
        dapicnum = BIGHOLE;
        break;
    }

    if (tilesiz[dapicnum].x == 0 || tilesiz[dapicnum].y == 0)
    {
        pus = pub = NUMPAGES;
        return;
    }

    y1 = 0;
    y2 = ydim;
    if (ud.gm & MODE_GAME || ud.recstat == 2)
        //if (ud.recstat == 0 || ud.recstat == 1 || (ud.recstat == 2 && ud.reccnt > 0)) // JBF 20040717
    {
        /*
        if (Gametypes[ud.coop].flags & GAMETYPE_FRAGBAR)
        {
            if (ud.multimode > 1) y1 += scale(ydim,8,200);
            if (ud.multimode > 4) y1 += scale(ydim,8,200);
        }
        */
    }
    else
    {
        // when not rendering a game, fullscreen wipe
#define MENUTILE (!videoGetRenderMode()?MENUSCREEN:LOADSCREEN)
        Gv_SetVar(sysVarIDs.RETURN, tilesiz[MENUTILE].x == 320 && tilesiz[MENUTILE].y == 200 ? MENUTILE : BIGHOLE, -1, -1);
        X_OnEvent(EVENT_GETMENUTILE, g_player[screenpeek].ps->i, screenpeek, -1);
        if (Gv_GetVarByLabel("MENU_TILE", tilesiz[MENUTILE].x==320&&tilesiz[MENUTILE].y==200?0:1, -1, -1))
        {
            for (y=y1;y<y2;y+=tilesiz[Gv_GetVar(sysVarIDs.RETURN, -1, -1)].y)
                for (x=0;x<xdim;x+=tilesiz[Gv_GetVar(sysVarIDs.RETURN, -1, -1)].x)
                    rotatesprite(x<<16,y<<16,65536L,0,Gv_GetVar(sysVarIDs.RETURN, -1, -1),bpp==8?16:8,0,8+16+64,0,0,xdim-1,ydim-1);
        }
        else rotatesprite(320<<15,200<<15,65536L,0,Gv_GetVar(sysVarIDs.RETURN, -1, -1),bpp==8?16:8,0,2+8+64,0,0,xdim-1,ydim-1);
        return;
    }

    y2 = scale(ydim,200-scale(tilesiz[BOTTOMSTATUSBAR].y,ud.statusbarscale,100),200);

    if ((ud.screen_size > 8) || ((G_GetFragbarOffset() != 0) && ud.screen_size >= 4))
    {
        // across top
        for (y = 0; y<windowxy1.y; y += tilesiz[dapicnum].y)
            for (x = 0; x<xdim; x += tilesiz[dapicnum].x)
                rotatesprite(x << 16, y << 16, 65536L, 0, dapicnum, 8, 0, 8 + 16 + 64, 0, y1, xdim - 1, windowxy1.y - 1);
    }

    if (ud.screen_size > 8)
    {
        // sides
        rx = windowxy2.x-windowxy2.x%tilesiz[dapicnum].x;
        for (y=windowxy1.y-windowxy1.y%tilesiz[dapicnum].y; y<windowxy2.y; y+=tilesiz[dapicnum].y)
            for (x=0; x<windowxy1.x || x+rx<xdim; x+=tilesiz[dapicnum].x)
            {
                rotatesprite(x<<16,y<<16,65536L,0,dapicnum,8,0,8+16+64,0,windowxy1.y,windowxy1.x-1,windowxy2.y-1);
                rotatesprite((x+rx)<<16,y<<16,65536L,0,dapicnum,8,0,8+16+64,windowxy2.x,windowxy1.y,xdim-1,windowxy2.y-1);
            }

        // along bottom
        for (y=windowxy2.y-(windowxy2.y%tilesiz[dapicnum].y); y<y2; y+=tilesiz[dapicnum].y)
            for (x=0; x<xdim; x+=tilesiz[dapicnum].x)
                rotatesprite(x<<16,y<<16,65536L,0,dapicnum,8,0,8+16+64,0,windowxy2.y,xdim-1,y2-1);
    }

    // draw in the bits to the left and right of the non-fullsize status bar
    if (ud.screen_size >= 8 && ud.statusbarmode == 0)
    {
        /*
        y1 = y2;
        x2 = (xdim - scale(xdim,ud.statusbarscale,100)) >> 1;
        x1 = xdim-x2;
        x1 -= x1%tilesiz[dapicnum].x;
        for (y=y1-y1%tilesiz[dapicnum].y; y<y2; y+=tilesiz[dapicnum].y)
        for (x=0;x<x2 || x1+x<xdim; x+=tilesiz[dapicnum].x)
        {
            rotatesprite(x<<16,y<<16,65536L,0,dapicnum,8,0,8+16+64,0,y1,x2-1,ydim-1);
            rotatesprite((x+x1)<<16,y<<16,65536L,0,dapicnum,8,0,8+16+64,xdim-x2,y1,xdim-1,ydim-1);
        }
        */
        // when not rendering a game, fullscreen wipe
        x2 = (xdim - scale((int32_t)(ydim*1.333333333333333333f), ud.statusbarscale, 100)) >> 1;
        for (y = y2 - y2 % tilesiz[dapicnum].y; y < ydim; y += tilesiz[dapicnum].y)
        {
            for (x = 0; x < xdim >> 1; x += tilesiz[dapicnum].x)
            {
                rotatesprite(x << 16, y << 16, 65536L, 0, dapicnum, 8, 0, 8 + 16 + 64, 0, y2, x2, ydim - 1);
                rotatesprite((xdim - x) << 16, y << 16, 65536L, 0, dapicnum, 8, 0, 8 + 16 + 64, xdim - x2 - 1, y2, xdim - 1, ydim - 1);
            }
        }
    }

    if (ud.screen_size > 8)
    {
        y = 0;
        y += G_GetFragbarOffset();

        x1 = max(windowxy1.x-4,0);
        y1 = max(windowxy1.y-4,y);
        x2 = min(windowxy2.x+4,xdim-1);
        y2 = min(windowxy2.y+4,scale(ydim,200-scale(tilesiz[BOTTOMSTATUSBAR].y,ud.statusbarscale,100),200)-1);

        for (y=y1+4;y<y2-4;y+=64)
        {
            rotatesprite(x1<<16,y<<16,65536L,0,VIEWBORDER,0,0,8+16+64,x1,y1,x2,y2);
            rotatesprite((x2+1)<<16,(y+64)<<16,65536L,1024,VIEWBORDER,0,0,8+16+64,x1,y1,x2,y2);
        }

        for (x=x1+4;x<x2-4;x+=64)
        {
            rotatesprite((x+64)<<16,y1<<16,65536L,512,VIEWBORDER,0,0,8+16+64,x1,y1,x2,y2);
            rotatesprite(x<<16,(y2+1)<<16,65536L,1536,VIEWBORDER,0,0,8+16+64,x1,y1,x2,y2);
        }

        rotatesprite(x1<<16,y1<<16,65536L,0,VIEWBORDER+1,0,0,8+16+64,x1,y1,x2,y2);
        rotatesprite((x2+1)<<16,y1<<16,65536L,512,VIEWBORDER+1,0,0,8+16+64,x1,y1,x2,y2);
        rotatesprite((x2+1)<<16,(y2+1)<<16,65536L,1024,VIEWBORDER+1,0,0,8+16+64,x1,y1,x2,y2);
        rotatesprite(x1<<16,(y2+1)<<16,65536L,1536,VIEWBORDER+1,0,0,8+16+64,x1,y1,x2,y2);
    }

    pus = pub = NUMPAGES;
}

static int32_t ror_sprite = -1;

extern float r_ambientlight;

char ror_protectedsectors[MAXSECTORS];
int32_t drawing_ror = 0;

static void G_OROR_DupeSprites(const spritetype *sp)
{
    // dupe the sprites touching the portal to the other sector
    int32_t k;
    const spritetype *refsp;

    refsp = &sprite[sp->yvel];

    for (k = headspritesect[sp->sectnum]; k != -1; k = nextspritesect[k])
    {
        if (spritesortcnt >= maxspritesonscreen)
            break;

        if (sprite[k].picnum != SECTOREFFECTOR && (sprite[k].z >= sp->z) && !(sprite[k].cstat & 32768))
        {
            Bmemcpy(&tsprite[spritesortcnt], &sprite[k], sizeof(spritetype));

            tsprite[spritesortcnt].x += (refsp->x - sp->x);
            tsprite[spritesortcnt].y += (refsp->y - sp->y);
            tsprite[spritesortcnt].z = tsprite[spritesortcnt].z - sp->z + actor[sp->yvel].ceilingz;
            tsprite[spritesortcnt].sectnum = refsp->sectnum;
            tsprite[spritesortcnt].owner = k;

            //            OSD_Printf("duped sprite of pic %d at %d %d %d\n",tsprite[spritesortcnt].picnum,tsprite[spritesortcnt].x,tsprite[spritesortcnt].y,tsprite[spritesortcnt].z);
            spritesortcnt++;
        }
    }
}

static void G_SE40(int32_t smoothratio)
{
    if ((unsigned)ror_sprite < MAXSPRITES)
    {
        int32_t x, y, z;
        int16_t sect;
        int32_t level = 0;
        const spritetype *const sp = &sprite[ror_sprite];
        const int32_t sprite2 = sp->yvel;

        if ((unsigned)sprite2 >= MAXSPRITES)
            return;

        if (klabs(sector[sp->sectnum].floorz - sp->z) < klabs(sector[sprite[sprite2].sectnum].floorz - sprite[sprite2].z))
            level = 1;

        x = ud.camerapos.x - sp->x;
        y = ud.camerapos.y - sp->y;
        z = ud.camerapos.z - (level ? sector[sp->sectnum].floorz : sector[sp->sectnum].ceilingz);

        sect = sprite[sprite2].sectnum;
        updatesector(sprite[sprite2].x + x, sprite[sprite2].y + y, &sect);

        if (sect != -1)
        {
            int32_t renderz, picnum;
            // XXX: PK: too large stack allocation for my taste
            int16_t backupstat[MAXSECTORS];
            int32_t backupz[MAXSECTORS];
            int32_t i;
            int32_t pix_diff, newz;
            //                LOG_F("drawing ror\n");

            if (level)
            {
                if (sp->ang < 1024)
                    renderz = sector[sprite[sprite2].sectnum].ceilingz;
                else
                    renderz = sprite[sprite2].z - (sprite[sprite2].yrepeat * tilesiz[sprite[sprite2].picnum].y << 1);

                picnum = sector[sprite[sprite2].sectnum].ceilingpicnum;
                sector[sprite[sprite2].sectnum].ceilingpicnum = 562;
                tilesiz[562].x = tilesiz[562].y = 0;

                pix_diff = klabs(z) >> 8;
                newz = -((pix_diff / 128) + 1) * (128 << 8);

                for (i = 0; i < numsectors; i++)
                {
                    backupstat[i] = sector[i].ceilingstat;
                    backupz[i] = sector[i].ceilingz;
                    if (!ror_protectedsectors[i] || (ror_protectedsectors[i] && sp->lotag == SE_41_ROR_LOWER))
                    {
                        sector[i].ceilingstat = 1;
                        sector[i].ceilingz += newz;
                    }
                }
            }
            else
            {
                if (sp->ang < 1024)
                    renderz = sector[sprite[sprite2].sectnum].floorz;
                else
                    renderz = sprite[sprite2].z;

                picnum = sector[sprite[sprite2].sectnum].floorpicnum;
                sector[sprite[sprite2].sectnum].floorpicnum = 562;
                tilesiz[562].x = tilesiz[562].y = 0;

                pix_diff = klabs(z) >> 8;
                newz = ((pix_diff / 128) + 1) * (128 << 8);

                for (i = 0; i < numsectors; i++)
                {
                    backupstat[i] = sector[i].floorstat;
                    backupz[i] = sector[i].floorz;
                    if (!ror_protectedsectors[i] || (ror_protectedsectors[i] && sp->lotag == SE_40_ROR_UPPER))
                    {
                        sector[i].floorstat = 1;
                        sector[i].floorz += newz;
                    }
                }
            }

#ifdef POLYMER
            if (videoGetRenderMode() == REND_POLYMER)
                polymer_setanimatesprites(G_DoSpriteAnimations, CAMERA(pos.x), CAMERA(pos.y), CAMERA(pos.z), fix16_to_int(CAMERA(q16ang)), smoothratio);
#endif
            renderDrawRoomsQ16(sprite[sprite2].x + x, sprite[sprite2].y + y,
                z + renderz, ud.cameraq16ang, ud.cameraq16horiz, sect);
            drawing_ror = 1 + level;

            if (drawing_ror == 2) // viewing from top
                G_OROR_DupeSprites(sp);

            G_DoSpriteAnimations(ud.camerapos.x, ud.camerapos.y, ud.camerapos.z, fix16_to_int(ud.cameraq16ang), smoothratio);
            renderDrawMasks();

            if (level)
            {
                sector[sprite[sprite2].sectnum].ceilingpicnum = picnum;
                for (i = 0; i < numsectors; i++)
                {
                    sector[i].ceilingstat = backupstat[i];
                    sector[i].ceilingz = backupz[i];
                }
            }
            else
            {
                sector[sprite[sprite2].sectnum].floorpicnum = picnum;

                for (i = 0; i < numsectors; i++)
                {
                    sector[i].floorstat = backupstat[i];
                    sector[i].floorz = backupz[i];
                }
            }
        }
    }
}

void G_HandleMirror(int32_t x, int32_t y, int32_t z, fix16_t a, fix16_t q16horiz, int32_t smoothratio)
{
#ifdef POLYMER
    if (videoGetRenderMode() == REND_POLYMER)
        return;
#endif
    if ((gotpic[MIRROR >> 3] & pow2char[MIRROR & 7]))
    {
        if (g_mirrorCount == 0)
        {
            // NOTE: We can have g_mirrorCount==0 but gotpic'd MIRROR,
            // for example in LNGA2.
            gotpic[MIRROR >> 3] &= ~pow2char[MIRROR & 7];

            //give scripts the chance to reset gotpics for effects that run in EVENT_DISPLAYROOMS
            //EVENT_RESETGOTPICS must be called after the last call to EVENT_DISPLAYROOMS in a frame, but before any engine-side renderDrawRoomsQ16
            //VM_OnEvent(EVENT_RESETGOTPICS, -1, -1);
            return;
        }

        int32_t i = 0, dst = INT32_MAX;

        for (bssize_t k = g_mirrorCount - 1; k >= 0; k--)
        {
            if (!wallvisible(x, y, g_mirrorWall[k]))
                continue;

            const int32_t j =
                klabs(wall[g_mirrorWall[k]].x - x) +
                klabs(wall[g_mirrorWall[k]].y - y);

            if (j < dst)
                dst = j, i = k;
        }

        if (wall[g_mirrorWall[i]].overpicnum != MIRROR)
        {
            // Try to find a new mirror wall in case the original one was broken.

            int32_t startwall = sector[g_mirrorSector[i]].wallptr;
            int32_t endwall = startwall + sector[g_mirrorSector[i]].wallnum;

            for (bssize_t k = startwall; k < endwall; k++)
            {
                int32_t j = wall[k].nextwall;
                if (j >= 0 && (wall[j].cstat & 32) && wall[j].overpicnum == MIRROR)  // cmp. premap.c
                {
                    g_mirrorWall[i] = j;
                    break;
                }
            }
        }

        if (wall[g_mirrorWall[i]].overpicnum == MIRROR)
        {
            int32_t tposx, tposy;
            fix16_t tang;

            //prepare to render any scripted EVENT_DISPLAYROOMS extras as mirrored
            renderPrepareMirror(x, y, z, a, q16horiz, g_mirrorWall[i], &tposx, &tposy, &tang);

            int32_t j = g_visibility;
            g_visibility = (j >> 1) + (j >> 2);

            //backup original camera position
            auto origCam = ud.camerapos;
            fix16_t origCamq16ang = ud.cameraq16ang;
            fix16_t origCamq16horiz = ud.cameraq16horiz;

            //set the camera inside the mirror facing out
            ud.camerapos = { tposx, tposy, z };
            ud.cameraq16ang = tang;
            ud.cameraq16horiz = q16horiz;

            display_mirror = 1;
            //VM_OnEventWithReturn(EVENT_DISPLAYROOMS, g_player[0].ps->i, 0, 0);
            X_OnEvent(EVENT_DISPLAYROOMS, g_player[screenpeek].ps->i, screenpeek, -1);
            display_mirror = 0;

            //reset the camera position
            ud.camerapos = origCam;
            ud.cameraq16ang = origCamq16ang;
            ud.cameraq16horiz = origCamq16horiz;

            //give scripts the chance to reset gotpics for effects that run in EVENT_DISPLAYROOMS
            //EVENT_RESETGOTPICS must be called after the last call to EVENT_DISPLAYROOMS in a frame, but before any engine-side renderDrawRoomsQ16
            //VM_OnEvent(EVENT_RESETGOTPICS, -1, -1);

            //prepare to render the mirror
            renderPrepareMirror(x, y, z, a, q16horiz, g_mirrorWall[i], &tposx, &tposy, &tang);

            int32_t didmirror;

            yax_preparedrawrooms();
            didmirror = renderDrawRoomsQ16(tposx, tposy, z, tang, q16horiz, g_mirrorSector[i] + MAXSECTORS);
            //POGO: if didmirror == 0, we may simply wish to abort instead of rendering with yax_drawrooms (which may require cleaning yax state)
            if (videoGetRenderMode() != REND_CLASSIC || didmirror)
                yax_drawrooms(G_DoSpriteAnimations, g_mirrorSector[i], didmirror, smoothratio);

            display_mirror = 1;
            G_DoSpriteAnimations(tposx, tposy, z, fix16_to_int(tang), smoothratio);
            display_mirror = 0;

            renderDrawMasks();
            renderCompleteMirror();   //Reverse screen x-wise in this function
            g_visibility = j;
        }
    }
}

static void G_ClearGotMirror()
{
    gotpic[MIRROR >> 3] &= ~pow2char[MIRROR & 7];
}

static void G_CalculateCameraInterpolation(DukePlayer_t *p, vec3_t* pos, fix16_t* q16ang, fix16_t* q16horiz, int16_t* sectnum, int32_t smoothratio)
{
    pos->x = p->opos.x + mulscale16((int)(p->pos.x - p->opos.x), smoothratio);
    pos->y = p->opos.y + mulscale16((int)(p->pos.y - p->opos.y), smoothratio);
    pos->z = p->opos.z + mulscale16((int)(p->pos.z - p->opos.z), smoothratio);
    *q16ang = p->oq16ang + mulscale16(((p->q16ang + F16(1024) - p->oq16ang) & 0x7FFFFFF) - F16(1024), smoothratio);
    *q16horiz = p->oq16horiz + p->oq16horizoff + mulscale16((p->q16horiz + p->q16horizoff - p->oq16horiz - p->oq16horizoff), smoothratio);
    *sectnum = p->cursectnum;
    *q16ang += fix16_from_int(p->olook_ang) + mulscale16(fix16_from_int(((p->look_ang + 1024 - p->olook_ang) & 2047) - 1024), smoothratio);

    if (p->over_shoulder_on == 0)
    {
        if (ud.viewbob)
            pos->z += p->opyoff + mulscale16((int)(p->pyoff - p->opyoff), smoothratio);
    }
}

#ifdef USE_OPENGL
static void G_ReadGLFrame(void)
{
//    MICROPROFILE_SCOPEI("Game", EDUKE32_FUNCTION, MP_YELLOWGREEN);

    // Save OpenGL screenshot with Duke3D palette
    // NOTE: maybe need to move this to the engine...

    auto frame = (palette_t *)Xaligned_alloc(16, xdim * ydim * sizeof(palette_t));
    char *const pic = (char *) waloff[TILE_SAVESHOT];

    Bassert(waloff[TILE_SAVESHOT]);

    int const xf = divscale16(ydim*4/3, 320);
    int const yf = divscale16(ydim, 200);  // (ydim<<16)/200

    videoBeginDrawing();
    glReadPixels(0, 0, xdim, ydim, GL_RGBA, GL_UNSIGNED_BYTE, frame);
    videoEndDrawing();

    for (int y = 0; y < 200; y++)
    {
        const int32_t base = mulscale16(200 - y - 1, yf)*xdim;

        for (int x = 0; x < 320; x++)
        {
            const palette_t *pix = &frame[base + mulscale16(x, xf) + (xdim-(ydim*4/3))/2];
            pic[320 * y + x] = paletteGetClosestColor(pix->r, pix->g, pix->b);
        }
    }

    Xaligned_free(frame);
}
#endif

void G_DrawRooms(int snum,int smoothratio)
{
    int floorZ, ceilZ;
    int i;
    DukePlayer_t *p = g_player[snum].ps;
    PlayerDisplay_t* pDisp = &g_player[snum].display;
    short tang;
    int tiltcx,tiltcy,tiltcs=0;    // JBF 20030807

    int const viewingRange = viewingrange;
    rotatespritesmoothratio = smoothratio;

    if (pub > 0 || videoGetRenderMode() >= REND_POLYMOST) // JBF 20040101: redraw background always
    {
        if (ud.screen_size >= 8)
            G_DrawBackground();
        pub = 0;
    }

    X_OnEvent(EVENT_DISPLAYSTART, p->i, snum, -1);

    if (ud.overhead_on == 2 || ud.show_help || (p->cursectnum == -1 && videoGetRenderMode() < 3))
        return;

    if (r_usenewaspect)
    {
        newaspect_enable = 1;
        videoSetCorrectedAspect();
    }

//    smoothratio = min(max(smoothratio,0),65536);

    g_visibility = (int)(pDisp->visibility * ((numplayers > 1 && !DMFLAGS_TEST(DMFLAG_ALLOWVISIBILITYCHANGE)) ? 1.f : r_ambientlightrecip)); // Do not allow visibility changes in online play.

    if (ud.pause_on)
        smoothratio = 65536;

    ud.camerasect = p->cursectnum;

    if (videoGetRenderMode() < 3 && (ud.camerasect < 0 || ud.camerasect >= MAXSECTORS)) return;

    G_DoInterpolations(smoothratio);
    G_AnimateCamSprite(smoothratio);
    G_DoConveyorInterp(smoothratio);
    G_InterpolateLights(smoothratio);

    if (ud.camerasprite >= 0)
    {
        spritetype *pSprite = &sprite[ud.camerasprite];

        if (pSprite->yvel < 0) pSprite->yvel = -100;
        else if (pSprite->yvel > 199) pSprite->yvel = 300;

        ud.cameraq16ang = fix16_from_int(actor[ud.camerasprite].tempang
            +mulscale16(((pSprite->ang+1024-actor[ud.camerasprite].tempang)&2047)-1024,smoothratio));

        G_SE40(smoothratio);

#ifdef POLYMER
        if (videoGetRenderMode() == REND_POLYMER)
            polymer_setanimatesprites(G_DoSpriteAnimations, pSprite->x, pSprite->y, pSprite->z - ZOFFSET6, fix16_to_int(CAMERA(q16ang)), smoothratio);
#endif

        yax_preparedrawrooms();
        renderDrawRoomsQ16(pSprite->x, pSprite->y, pSprite->z - ZOFFSET6, ud.cameraq16ang, pSprite->yvel, pSprite->sectnum);
        yax_drawrooms(G_DoSpriteAnimations, pSprite->sectnum, 0, smoothratio);
        G_DoSpriteAnimations(pSprite->x, pSprite->y, pSprite->z, fix16_to_int(ud.cameraq16ang),smoothratio);
        renderDrawMasks();
    }
    else
    {
        int vr = divscale22(1,sprite[p->i].yrepeat+28);
        int screenTilting = (videoGetRenderMode() == REND_CLASSIC
            && ((ud.screen_tilting && p->rotscrnang)));

        vr = Blrintf(float(vr) * tanf((DMFLAGS_TEST(DMFLAG_NOFOVCHANGE) ? 90 : ud.fov) * (fPI / 360.f)));

        if (!r_usenewaspect)
            renderSetAspect(vr, yxaspect);
        else
            renderSetAspect(mulscale16(vr, viewingrange), yxaspect);

        if (g_screenCapture)
        {
            walock[TILE_SAVESHOT] = 199;
            if (waloff[TILE_SAVESHOT] == 0)
            {
                g_cache.allocateBlock(&waloff[TILE_SAVESHOT],200*320,&walock[TILE_SAVESHOT]);
                tileSetSize(TILE_SAVESHOT, 200, 320);
            }

            if (videoGetRenderMode() == REND_CLASSIC)
                renderSetTarget(TILE_SAVESHOT, 200, 320);
        }
        else if (screenTilting)
        {
            int32_t oviewingrange = viewingrange;  // save it from renderSetAspect()
            const int16_t tang = (ud.screen_tilting) ? p->rotscrnang : 0;

            if (tang == 1024)
                screenTilting = 2;
            else
            {
                // Maximum possible allocation size passed to allocache() below
                // since there is no equivalent of free() for allocache().
#if MAXYDIM >= 640
                int const maxTiltSize = 640 * 640;
#else
                int const maxTiltSize = 320 * 320;
#endif
                // To render a tilted screen in high quality, we need at least
                // 640 pixels of *Y* dimension.
#if MAXYDIM >= 640
                // We also need
                //  * xdim >= 640 since tiltcx will be passed as setview()'s x2
                //    which must be less than xdim.
                //  * ydim >= 640 (sic!) since the tile-to-draw-to will be set
                //    up with dimension 400x640, but the engine's arrays like
                //    lastx[] are alloc'd with *xdim* elements! (This point is
                //    the dynamic counterpart of the #if above since we now
                //    allocate these engine arrays tightly.)
                // XXX: The engine should be in charge of setting up everything
                // so that no oob access occur.
                if (xdim >= 640 && ydim >= 640)
                {
                    tiltcs = 2;
                    tiltcx = 640;
                    tiltcy = 400;
                }
                else
#endif
                {
                    // JBF 20030807: Increased tilted-screen quality
                    tiltcs = 1;

                    // NOTE: The same reflections as above apply here, too.
                    // TILT_SETVIEWTOTILE_320.
                    tiltcx = 320;
                    tiltcy = 200;
                }

                // If the view is rotated (not 0 or 180 degrees modulo 360 degrees),
                // we render onto a square tile and display a portion of that
                // rotated on-screen later on.
                const int32_t viewtilexsiz = (tang & 1023) ? tiltcx : tiltcy;
                const int32_t viewtileysiz = tiltcx;

                walock[TILE_TILT] = CACHE1D_PERMANENT;
                if (waloff[TILE_TILT] == 0)
                    g_cache.allocateBlock(&waloff[TILE_TILT], maxTiltSize, &walock[TILE_TILT]);

                renderSetTarget(TILE_TILT, viewtilexsiz, viewtileysiz);

                if ((tang & 1023) == 512)
                {
                    //Block off unscreen section of 90ø tilted screen
                    int const j = tiltcx - (60 * tiltcs);
                    for (bssize_t i = (60 * tiltcs) - 1; i >= 0; i--)
                    {
                        startumost[i] = 1;
                        startumost[i + j] = 1;
                        startdmost[i] = 0;
                        startdmost[i + j] = 0;
                    }
                }

                int vRange = (tang & 511);

                if (vRange > 256)
                    vRange = 512 - vRange;

                vRange = sintable[vRange + 512] * 8 + sintable[vRange] * 5;
                renderSetAspect(mulscale16(oviewingrange, vRange >> 1), yxaspect);
            }
        }
        else if (videoGetRenderMode() >= REND_POLYMOST && ud.screen_tilting)
        {
#ifdef USE_OPENGL
            renderSetRollAngle(p->orotscrnang + mulscale16(((p->rotscrnang - p->orotscrnang + 1024) & 2047) - 1024, smoothratio));
#endif
        }
#ifdef USE_OPENGL
        else
        {
            renderSetRollAngle(0);
        }
#endif

        if (p->newowner < 0)
        {
            G_CalculateCameraInterpolation(p, &ud.camerapos, &ud.cameraq16ang, &ud.cameraq16horiz, &ud.camerasect, smoothratio);

            if (p->over_shoulder_on > 0)
                G_DoThirdPerson(p, &ud.camerapos, &ud.camerasect, fix16_to_int(ud.cameraq16ang), fix16_to_int(ud.cameraq16horiz));
        } 
        else
        {
            vec3_t const camVect = G_GetCameraPosition(p->newowner, smoothratio);

            // Looking through viewscreen
            ud.camerapos = camVect;
            ud.cameraq16ang = fix16_from_int(sprite[p->newowner].ang) + fix16_from_int(p->look_ang);
            ud.cameraq16horiz = fix16_from_int(100 + sprite[p->newowner].shade);
            ud.camerasect = sprite[p->newowner].sectnum;
        }
        
        ceilZ = actor[p->i].ceilingz;
        floorZ = actor[p->i].floorz;

        if ((X_OnEventWithReturn(EVENT_DOQUAKE, p->i, snum, g_earthquakeTime)) && g_earthquakeTime > 0 && p->on_ground == 1)
        {
            ud.camerapos.z += 256-(((g_earthquakeTime)&1)<<9);
            ud.cameraq16ang += fix16_from_int((2-((g_earthquakeTime)&2))<<2);
        }

        if (sprite[p->i].pal == 1) ud.camerapos.z -= (18<<8);

        if (p->newowner >= 0)
            ud.cameraq16horiz = fix16_from_int(100+sprite[p->newowner].shade);
        else if (p->spritebridge == 0)
        {
            if (ud.camerapos.z < (p->truecz + (4<<8))) ud.camerapos.z = ceilZ + (4<<8);
            else if (ud.camerapos.z > (p->truefz - (4<<8))) ud.camerapos.z = floorZ - (4<<8);
        }

        while (ud.camerasect >= 0)
        {
            getzsofslope(ud.camerasect,ud.camerapos.x,ud.camerapos.y,&ceilZ,&floorZ);
            if (yax_getbunch(ud.camerasect, YAX_CEILING) >= 0)
            {
                if (ud.camerapos.z < ceilZ)
                {
                    updatesectorz(ud.camerapos.x, ud.camerapos.y, ud.camerapos.z, &ud.camerasect);
                    break;  // since CAMERA(sect) might have been updated to -1
                    // NOTE: fist discovered in WGR2 SVN r134, til' death level 1
                    //  (Lochwood Hollow).  A problem REMAINS with Polymost, maybe classic!
                }
            }
            else
            {
                if (ud.camerapos.z < ceilZ + (4 << 8)) ud.camerapos.z = ceilZ + (4 << 8);
            }

            if (yax_getbunch(ud.camerasect, YAX_FLOOR) >= 0)
            {
                if (ud.camerapos.z > floorZ)
                    updatesectorz(ud.camerapos.x, ud.camerapos.y, ud.camerapos.z, &ud.camerasect);
            }
            else
            {
                if (ud.camerapos.z > floorZ - (4 << 8)) ud.camerapos.z = floorZ - (4 << 8);
            }

            break;
        }

        X_OnEvent(EVENT_DISPLAYROOMS, g_player[screenpeek].ps->i, screenpeek, -1);

        ud.cameraq16horiz = fix16_clamp(ud.cameraq16horiz, F16(HORIZ_MIN), F16(HORIZ_MAX));

        G_HandleMirror(ud.camerapos.x, ud.camerapos.y, ud.camerapos.z, ud.cameraq16ang, ud.cameraq16horiz, smoothratio);
        G_ClearGotMirror();

        G_SE40(smoothratio);

#ifdef POLYMER
        if (videoGetRenderMode() == REND_POLYMER)
            polymer_setanimatesprites(G_DoSpriteAnimations, CAMERA(pos.x), CAMERA(pos.y), CAMERA(pos.z), fix16_to_int(CAMERA(q16ang)), smoothratio);
#endif

        yax_preparedrawrooms();
        renderDrawRoomsQ16(ud.camerapos.x,ud.camerapos.y,ud.camerapos.z,ud.cameraq16ang,ud.cameraq16horiz,ud.camerasect);
        yax_drawrooms(G_DoSpriteAnimations, ud.camerasect, 0, smoothratio);

        // dupe the sprites touching the portal to the other sector
        if ((unsigned)ror_sprite < MAXSPRITES && drawing_ror == 1)
            G_OROR_DupeSprites(&sprite[ror_sprite]);

        G_DoSpriteAnimations(ud.camerapos.x,ud.camerapos.y,ud.camerapos.z,fix16_to_int(ud.cameraq16ang),smoothratio);
        drawing_ror = 0;
        renderDrawMasks();

        if (g_screenCapture == 1)
        {
            g_screenCapture = 0;

            if (videoGetRenderMode() == REND_CLASSIC)
                renderRestoreTarget();
#ifdef USE_OPENGL
            else
            {
                G_ReadGLFrame();
                tileInvalidate(TILE_SAVESHOT, 0, 255);
            }
#endif
            //            walock[TILE_SAVESHOT] = 1;
        }
        else if (videoGetRenderMode() == REND_CLASSIC && ((ud.screen_tilting && p->rotscrnang) || ud.detail==0))
        {
            if (ud.screen_tilting) tang = p->rotscrnang;
            else tang = 0;

            if (videoGetRenderMode() == REND_CLASSIC)
            {
                renderRestoreTarget();
                picanm[TILE_TILT].xofs = picanm[TILE_TILT].yofs = 0;

                i = (tang&511);
                if (i > 256) i = 512-i;
                i = sintable[i+512]*8 + sintable[i]*5;
                if ((1-ud.detail) == 0) i >>= 1;
                i>>=(tiltcs-1); // JBF 20030807
                rotatesprite(160<<16,100<<16,i,tang+512,TILE_TILT,0,0,4+2+64,windowxy1.x,windowxy1.y,windowxy2.x,windowxy2.y);
                walock[TILE_TILT] = 199;
            }
        }
    }

    G_RestoreInterpolations();
    G_ResetConveyorInterp();

    {
        // Totalclock count of last step of p->visibility converging towards
        // ud.const_visibility.
        static int32_t lastvist;
        const int32_t visdif = ud.const_visibility - pDisp->visibility;

        // Check if totalclock was cleared (e.g. restarted game).
        if (totalclock < lastvist)
            lastvist = 0;

        // Every 2nd totalclock increment (each 1/60th second), ...
        while (totalclock >= lastvist + 2)
        {
            // ... approximately three-quarter the difference between
            // p->visibility and ud.const_visibility.
            const int32_t visinc = visdif >> 2;

            if (klabs(visinc) == 0)
            {
                pDisp->visibility = ud.const_visibility;
                break;
            }

            pDisp->visibility += visinc;
            lastvist = (int32_t)totalclock;
        }
    }

    if (r_usenewaspect)
    {
        newaspect_enable = 0;
        renderSetAspect(viewingRange, tabledivide32_noinline(65536 * ydim * 8, xdim * 5));
    }
}

static void G_DumpDebugInfo(void)
{
    int i,j,x;
    int16_t q16ang;
   // FILE * fp=fopen("debug.con","w");

    VM_ScriptInfo(insptr, 64);
    OSD_Printf("\n");

    OSD_Printf("Current gamevar values:\n");
    for (i=0;i<MAX_WEAPONS;i++)
    {
        for (j=0;j<numplayers;j++)
        {
            OSD_Printf("Player %d\n\n",j);
            OSD_Printf("WEAPON%d_CLIP %" PRIdPTR "\n",i,aplWeaponClip[i][j]);
            OSD_Printf("WEAPON%d_RELOAD %" PRIdPTR "\n",i,aplWeaponReload[i][j]);
            OSD_Printf("WEAPON%d_FIREDELAY %" PRIdPTR "\n",i,aplWeaponFireDelay[i][j]);
            OSD_Printf("WEAPON%d_TOTALTIME %" PRIdPTR "\n",i,aplWeaponTotalTime[i][j]);
            OSD_Printf("WEAPON%d_HOLDDELAY %" PRIdPTR "\n",i,aplWeaponHoldDelay[i][j]);
            OSD_Printf("WEAPON%d_FLAGS %" PRIdPTR "\n",i,aplWeaponFlags[i][j]);
            OSD_Printf("WEAPON%d_SHOOTS %" PRIdPTR "\n",i,aplWeaponShoots[i][j]);
            OSD_Printf("WEAPON%d_SPAWNTIME %" PRIdPTR "\n",i,aplWeaponSpawnTime[i][j]);
            OSD_Printf("WEAPON%d_SPAWN %" PRIdPTR "\n",i,aplWeaponSpawn[i][j]);
            OSD_Printf("WEAPON%d_SHOTSPERBURST %" PRIdPTR "\n",i,aplWeaponShotsPerBurst[i][j]);
            OSD_Printf("WEAPON%d_WORKSLIKE %" PRIdPTR "\n",i,aplWeaponWorksLike[i][j]);
            OSD_Printf("WEAPON%d_INITIALSOUND %" PRIdPTR "\n",i,aplWeaponInitialSound[i][j]);
            OSD_Printf("WEAPON%d_FIRESOUND %" PRIdPTR "\n",i,aplWeaponFireSound[i][j]);
            OSD_Printf("WEAPON%d_SOUND2TIME %" PRIdPTR "\n",i,aplWeaponSound2Time[i][j]);
            OSD_Printf("WEAPON%d_SOUND2SOUND %" PRIdPTR "\n",i,aplWeaponSound2Sound[i][j]);
            OSD_Printf("WEAPON%d_RELOADSOUND1 %" PRIdPTR "\n",i,aplWeaponReloadSound1[i][j]);
            OSD_Printf("WEAPON%d_RELOADSOUND2 %" PRIdPTR "\n",i,aplWeaponReloadSound2[i][j]);
            OSD_Printf("WEAPON%d_SELECTSOUND %" PRIdPTR "\n",i,aplWeaponSelectSound[i][j]);
        }
        OSD_Printf("\n");
    }
    for (x=0;x<MAXSTATUS;x++)
    {
        j = headspritestat[x];
        while (j >= 0)
        {
            OSD_Printf("Sprite %d (%d,%d,%d) (picnum: %d)\n",j,TrackerCast(sprite[j].x),TrackerCast(sprite[j].y),TrackerCast(sprite[j].z),TrackerCast(sprite[j].picnum));
            for (i=0;i<g_gameVarCount;i++)
            {
                if (aGameVars[i].flags & (GAMEVAR_PERACTOR))
                {
                    if (aGameVars[i].pValues[j] != aGameVars[i].defaultValue)
                    {
                        OSD_Printf("gamevar %s ",aGameVars[i].szLabel);
                        OSD_Printf("%" PRIdPTR "",aGameVars[i].pValues[j]);
                        OSD_Printf(" GAMEVAR_PERACTOR");
                        if (aGameVars[i].flags != GAMEVAR_PERACTOR)
                        {
                            OSD_Printf(" // ");
                            if (aGameVars[i].flags & (GAMEVAR_SYSTEM))
                            {
                                OSD_Printf(" (system)");
                            }
                        }
                        OSD_Printf("\n");
                    }
                }
            }
            OSD_Printf("\n");
            j = nextspritestat[j];
        }
    }
    Gv_DumpValues();
   // fclose(fp);

    q16ang = fix16_to_int(g_player[myconnectindex].ps->q16ang);
    saveboard("debug.map",&g_player[myconnectindex].ps->pos, q16ang, g_player[myconnectindex].ps->cursectnum);
}

// if <set_movflag_uncond> is true, set the moveflag unconditionally,
// else only if it equals 0.
static int32_t G_InitActor(int32_t i, int32_t tilenum, int32_t set_movflag_uncond)
{
    if (g_tile[tilenum].execPtr)
    {
        SH(i) = g_tile[tilenum].execPtr[0];
        AC_ACTION_ID(actor[i].t_data) = g_tile[tilenum].execPtr[1];
        AC_MOVE_ID(actor[i].t_data) = g_tile[tilenum].execPtr[2];

        if (set_movflag_uncond || SHT(i) == 0)  // AC_MOVFLAGS
            SHT(i) = g_tile[tilenum].execPtr[3];

        return 1;
    }

    return 0;
}

int A_InsertSprite(int whatsect,int s_x,int s_y,int s_z,int s_pn,int s_s,int s_xr,int s_yr,int s_a,int s_ve,int s_zv,int s_ow,int s_ss)
{
    int i = insertsprite(whatsect,s_ss);
    int p;
    spritetype *s = &sprite[i];

    if (oldnet_predicting)
    {
        OSD_Printf("^08A_InsertSprite^12: ^02ATTEMPTING TO SPAWN ACTOR FROM PREDICTING STATE! FAIL.\n");
        OSD_Printf("^12Owner SpriteNum:^07 %d, ^12Owner PicNum:^07 %d\n", s_ow, TrackerCast(sprite[s_ow].picnum));
        OSD_Printf("^12Spawnee SpriteNum:^07 %d, ^12Spawnee PicNum:^07 %d\n", i, s_pn);

        sprintf(tempbuf, "^02Critical Error in A_InsertSprite! See logs.");
        G_AddUserQuote(tempbuf);
        return -1;
    }

    if (i < 0)
    {
        G_DumpDebugInfo();
        OSD_Printf("Failed spawning sprite with tile %d from sprite %d (%d) at x:%d,y:%d,z:%d,sector:%d\n",s_pn,s_ow,TrackerCast(sprite[s_ow].picnum),s_x,s_y,s_z,whatsect);
        G_GameExit("Too many sprites spawned.");
    }

    // Clear all sprite/actor info.
    sprite[i] = {};
    spriteext[i] = {};
    spritesmooth[i] = {};
    actor[i] = {};

#ifdef POLYMER
    practor[i].lightId = -1;
#endif

    // Restore sect/statnums.
    s->sectnum = whatsect;
    s->statnum = s_ss;

    s->x = s_x;
    s->y = s_y;
    s->z = s_z;
    s->picnum = s_pn;
    s->shade = s_s;
    s->xrepeat = s_xr;
    s->yrepeat = s_yr;

    s->ang = s_a;
    s->xvel = s_ve;
    s->zvel = s_zv;
    s->owner = s_ow;

    if (s_ow > -1 && s_ow < MAXSPRITES)
    {
        actor[i].htpicnum = sprite[s_ow].picnum;
        actor[i].floorz = actor[s_ow].floorz;
        actor[i].ceilingz = actor[s_ow].ceilingz;
    }

    actor[i].bpos.x = s_x;
    actor[i].bpos.y = s_y;
    actor[i].bpos.z = s_z;

    actor[i].actorstayput = -1;
    actor[i].htextra = -1;
    actor[i].htowner = s_ow;

    G_InitActor(i, s_pn, 1);
    A_ResetVars(i);

    bitmap_test(show2dsector, SECT(i)) ? bitmap_set(show2dsprite, i)
                                       : bitmap_clear(show2dsprite, i);

    if (s_ss == STAT_PROJECTILE) // Moved from PROJECTILE_RPG to here, so we can maniuplate these in EVENT_EGS.
    {
        auto const pProj = Proj_GetProjectile(s_pn);

        sprite[i].pal = (pProj->pal >= 0) ? pProj->pal : 0;
        sprite[i].cstat = (pProj->cstat >= 0) ? pProj->cstat : 128;
        sprite[i].clipdist = (pProj->clipdist != 255) ? pProj->clipdist : 40;

        actor[i].projectile = *pProj;
    }

    if (VM_HaveEvent(EVENT_EGS))
    {
        extern int block_deletesprite;
        int pl=A_FindPlayer(&sprite[i],&p);

        block_deletesprite++;
        X_OnEvent(EVENT_EGS,i, pl, p);
        block_deletesprite--;
    }

    return(i);
}

static inline bool OnEvenGround(spritetype* pSprite, int32_t radius)
{
    int16_t sectNum = pSprite->sectnum;

    updatesector(pSprite->x + radius, pSprite->y + radius, &sectNum);
    if (sectNum < 0 || sector[sectNum].floorz != sector[pSprite->sectnum].floorz)
        return false;

    updatesector(pSprite->x - radius, pSprite->y - radius, &sectNum);
    if (sectNum < 0 || sector[sectNum].floorz != sector[pSprite->sectnum].floorz)
        return false;

    updatesector(pSprite->x + radius, pSprite->y - radius, &sectNum);
    if (sectNum < 0 || sector[sectNum].floorz != sector[pSprite->sectnum].floorz)
        return false;

    updatesector(pSprite->x - radius, pSprite->y + radius, &sectNum);
    if (sectNum < 0 || sector[sectNum].floorz != sector[pSprite->sectnum].floorz)
        return false;

    return true;
}

int A_Spawn(int spriteNum, int tileNum)
{
    spritetype*  const pParent = (spriteNum >= 0) ? &sprite[spriteNum] : nullptr;
    spritetype*  pSprite;
    ActorData_t* pActor;
    int          sectNum;
    int          newSprite;
    bool const   hasParent = (pParent != nullptr);

    if (hasParent) // Sprite has a parent.
    {
        newSprite = A_InsertSprite(pParent->sectnum, pParent->x, pParent->y, pParent->z, tileNum, 0, 0, 0, 0, 0, 0, spriteNum, 0);
        actor[newSprite].htpicnum = pParent->picnum;
    }
    else // No parent, and sprite already exists, likely spawned from map itself. <pn>
    {
        newSprite = tileNum;
        auto& s = sprite[newSprite];
        auto& a = actor[newSprite];

        a = { };

        a.htpicnum = s.picnum;
        a.htextra = -1;
        a.bpos = s.xyz;

        s.owner = a.htowner = newSprite;

        a.floorz = sector[s.sectnum].floorz;
        a.ceilingz = sector[s.sectnum].ceilingz;
        a.actorstayput = a.htextra = -1;

#ifdef POLYMER
        practor[newSprite].lightId = -1;
#endif

        if ((s.cstat & CSTAT_SPRITE_ALIGNMENT) &&
            s.picnum != SPEAKER &&
            s.picnum != LETTER &&
            s.picnum != DUCK &&
            s.picnum != TARGET &&
            s.picnum != TRIPBOMB &&
            s.picnum != VIEWSCREEN &&
            s.picnum != VIEWSCREEN2 &&
            !(s.picnum >= CRACK1 && s.picnum <= CRACK4))
        {
            if (s.shade == 127)
                goto SPAWN_END;

            if (A_CheckSwitchTile(newSprite) && (s.cstat & CSTAT_SPRITE_ALIGNMENT_WALL))
            {
                if (s.pal && s.picnum != ACCESSSWITCH && s.picnum != ACCESSSWITCH2)
                {
                    if ((ud.multimode < 2) || (ud.multimode > 1 && !GTFLAGS(GAMETYPE_DMSWITCHES)))
                    {
                        s.xrepeat = s.yrepeat = 0;
                        s.lotag = s.hitag = 0;
                        s.cstat = CSTAT_SPRITE_INVISIBLE;
                        goto SPAWN_END;
                    }

                    s.pal = 0;
                }

                s.cstat |= (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN);
                    
                goto SPAWN_END;
            }

            if (s.hitag)
            {
                changespritestat(newSprite, STAT_FALLER);
                s.cstat |= (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN);
                s.extra = g_impactDamage;
                goto SPAWN_END;
            }
        }

        if (s.cstat & CSTAT_SPRITE_BLOCK)
            s.cstat |= CSTAT_SPRITE_BLOCK_HITSCAN;

        if (!G_InitActor(newSprite, s.picnum, 0))
            T2(newSprite) = T5(newSprite) = 0;  // AC_MOVE_ID, AC_ACTION_ID
        else
        {
            A_GetZLimits(newSprite);
            a.bpos = s.xyz;
        }
    }

    pSprite = &sprite[newSprite];
    pActor = &actor[newSprite];
    sectNum = pSprite->sectnum;

    //some special cases that can't be handled through the dynamictostatic system.
    if (pSprite->picnum >= CAMERA1 && pSprite->picnum <= CAMERA1 + 4)
        pSprite->picnum = CAMERA1;
    else if (pSprite->picnum >= BOLT1 && pSprite->picnum <= BOLT1 + 3)
        pSprite->picnum = BOLT1;
    else if (pSprite->picnum >= SIDEBOLT1 && pSprite->picnum <= SIDEBOLT1 + 3)
        pSprite->picnum = SIDEBOLT1;

    switch (tileGetMapping(pSprite->picnum))
    {
        case FOF__:
            pSprite->xrepeat = pSprite->yrepeat = 0;
            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case CAMERA1__:
            pSprite->extra = 1;
            pSprite->cstat &= CSTAT_SPRITE_INVISIBLE;

            if (g_damageCameras)
                pSprite->cstat |= (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN);

            if ((ud.multimode < 2) && pSprite->pal != 0)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
            }
            else
            {
                pSprite->pal = 0;
                changespritestat(newSprite, STAT_ACTOR);
            }
            goto SPAWN_END;

        case CAMERAPOLE__:
            pSprite->extra = 1;
            pSprite->cstat &= ~(CSTAT_SPRITE_INVISIBLE);

            if (g_damageCameras)
                pSprite->cstat |= (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN);
            fallthrough__;
        case GENERICPOLE__:
            if (ud.multimode < 2 && pSprite->pal != 0)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
            }
            else
                pSprite->pal = 0;
            goto SPAWN_END;

        case BOLT1__:
        case SIDEBOLT1__:
            T1(newSprite) = pSprite->xrepeat;
            T2(newSprite) = pSprite->yrepeat;
            pSprite->yvel = 0;

            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case WATERSPLASH2__:
            pSprite->shade = -16;
            pSprite->cstat |= CSTAT_SPRITE_YCENTER;

            if (hasParent)
            {
                setspritez(newSprite, &pParent->xyz);
                pSprite->xrepeat = pSprite->yrepeat = 8 + (krand() & 7);

                if (sector[pSprite->sectnum].lotag == ST_2_UNDERWATER)
                {
                    pSprite->z = getceilzofslope(pSprite->sectnum, pSprite->x, pSprite->y) + (16 << 8);
                    pSprite->cstat |= CSTAT_SPRITE_YFLIP;
                }
                else if (sector[pSprite->sectnum].lotag == ST_1_ABOVE_WATER)
                    pSprite->z = getflorzofslope(pSprite->sectnum, pSprite->x, pSprite->y);
            }
            else
                pSprite->xrepeat = pSprite->yrepeat = 16 + (krand() & 15);

            if (sector[sectNum].floorpicnum == FLOORSLIME ||
                sector[sectNum].ceilingpicnum == FLOORSLIME)
                pSprite->pal = 7;
            else if (sector[sectNum].floorpicnum == FLOORPLASMA ||
                sector[sectNum].ceilingpicnum == FLOORPLASMA)
                pSprite->pal = 2;

            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case NEON1__:
        case NEON2__:
        case NEON3__:
        case NEON4__:
        case NEON5__:
        case NEON6__:
        case DOMELITE__:
        case NUKEBUTTON__:
            if(pSprite->picnum == DOMELITE)
                pSprite->cstat |= (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN);
            fallthrough__;
        case JIBS1__:
        case JIBS2__:
        case JIBS3__:
        case JIBS4__:
        case JIBS5__:
        case JIBS6__:
        case HEADJIB1__:
        case ARMJIB1__:
        case LEGJIB1__:
        case LIZMANHEAD1__:
        case LIZMANARM1__:
        case LIZMANLEG1__:
        case DUKETORSO__:
        case DUKEGUN__:
        case DUKELEG__:
            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case TONGUE__:
            if (hasParent)
                pSprite->ang = pParent->ang;
            pSprite->z -= 38 << 8;
            pSprite->zvel = 256 - (krand() & 511);
            pSprite->xvel = 64 - (krand() & 127);
            changespritestat(newSprite, STAT_PROJECTILE);
            goto SPAWN_END;

        case NATURALLIGHTNING__:
            pSprite->cstat &= ~(CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN);
            pSprite->cstat |= 32768;
            goto SPAWN_END;

        case TRANSPORTERSTAR__:
        case TRANSPORTERBEAM__:
            if (!hasParent)
                goto SPAWN_END;

            if (pSprite->picnum == TRANSPORTERBEAM)
            {
                pSprite->xrepeat = 31;
                pSprite->yrepeat = 1;
                pSprite->z = sector[pParent->sectnum].floorz - (40 << 8);
            }
            else
            {
                if (pParent->statnum == STAT_PROJECTILE)
                {
                    pSprite->xrepeat = 8;
                    pSprite->yrepeat = 8;
                }
                else
                {
                    pSprite->xrepeat = 48;
                    pSprite->yrepeat = 64;
                    if (pParent->statnum == STAT_PLAYER || A_CheckEnemySprite(pParent))
                        pSprite->z -= (32 << 8);
                }
            }

            pSprite->shade = -127;
            pSprite->cstat = (CSTAT_SPRITE_TRANSLUCENT | CSTAT_SPRITE_YCENTER);
            pSprite->ang = pParent->ang;

            pSprite->xvel = 128;
            changespritestat(newSprite, STAT_MISC);
            A_SetSprite(newSprite, CLIPMASK0);
            setspritez(newSprite, &pSprite->xyz);
            goto SPAWN_END;

        case FEMMAG1__:
        case FEMMAG2__:
            pSprite->cstat &= ~(CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN);
            changespritestat(newSprite, STAT_DEFAULT);
            goto SPAWN_END;

        case LASERLINE__:
            pSprite->yrepeat = 6;
            pSprite->xrepeat = 32;

            pSprite->cstat = CSTAT_SPRITE_ALIGNMENT_WALL;
            if (g_tripbombLaserMode == 1)
                pSprite->cstat |= CSTAT_SPRITE_TRANSLUCENT;

            if(g_tripbombLaserMode > 2)
            {
                pSprite->xrepeat = 0;
                pSprite->yrepeat = 0;
            }

            if (hasParent)
                pSprite->ang = actor[spriteNum].t_data[5] + 512;
            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case FORCESPHERE__:
            if (!hasParent)
            {
                pSprite->cstat = (short)32768;
                changespritestat(newSprite, STAT_ZOMBIEACTOR);
            }
            else
            {
                pSprite->xrepeat = pSprite->yrepeat = 1;
                changespritestat(newSprite, STAT_MISC);
            }
            goto SPAWN_END;

        case BLOOD__:
            pSprite->xrepeat = pSprite->yrepeat = 16;
            pSprite->z -= (26 << 8);
            if (hasParent && pParent->pal == 6)
                pSprite->pal = 6;
            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case BLOODPOOL__:
        case PUKE__:
            changespritestat(newSprite, STAT_MISC);

            if (!OnEvenGround(pSprite, 108) || sector[pSprite->sectnum].lotag == ST_1_ABOVE_WATER)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                goto SPAWN_END;
            }

            pSprite->cstat |= CSTAT_SPRITE_ALIGNMENT_FLOOR;

            if (hasParent)
            {
                pSprite->xrepeat = pSprite->yrepeat = 1;

                if (pSprite->picnum == PUKE)
                    goto SPAWN_END;

                if (pParent->pal == 1)
                    pSprite->pal = 1;
                else if (pParent->pal != 6 && pParent->picnum != NUKEBARREL && pParent->picnum != TIRE)
                {
                    if (pParent->picnum == FECES)
                        pSprite->pal = 7; // Brown
                    else
                        pSprite->pal = 2; // Red
                }
                else pSprite->pal = 0;  // green

                if (pParent->picnum == TIRE)
                    pSprite->shade = 127;
            }

            goto SPAWN_END;

        case FECES__:
            if (hasParent)
                pSprite->xrepeat = pSprite->yrepeat = 1;
            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case BLOODSPLAT1__:
        case BLOODSPLAT2__:
        case BLOODSPLAT3__:
        case BLOODSPLAT4__:
            pSprite->cstat |= CSTAT_SPRITE_ALIGNMENT_WALL;
            pSprite->xrepeat = 7 + (krand() & 7);
            pSprite->yrepeat = 7 + (krand() & 7);
            pSprite->z -= (16 << 8);
            if (hasParent && pParent->pal == 6)
                pSprite->pal = 6;
            A_AddToDeleteQueue(newSprite);
            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case TRIPBOMB__:
            if (pSprite->lotag > ud.player_skill)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }

            pSprite->xrepeat = 4;
            pSprite->yrepeat = 5;

            pSprite->owner = newSprite;
            pSprite->hitag = newSprite;

            pSprite->xvel = 16;
            A_SetSprite(newSprite, CLIPMASK0);
            pActor->t_data[0] = 17;
            pActor->t_data[2] = 0;
            pActor->t_data[5] = pSprite->ang;
            changespritestat(newSprite, 2);
            goto SPAWN_END;

        case SPACEMARINE__:
            if (pSprite->picnum == SPACEMARINE)
            {
                pSprite->extra = 20;
                pSprite->cstat |= (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN);
            }
            changespritestat(newSprite, STAT_ZOMBIEACTOR);
            goto SPAWN_END;

        case HYDRENT__:
        case PANNEL1__:
        case PANNEL2__:
        case SATELITE__:
        case FUELPOD__:
        case SOLARPANNEL__:
        case ANTENNA__:
        case GRATE1__:
        case CHAIR1__:
        case CHAIR2__:
        case CHAIR3__:
        case BOTTLE1__:
        case BOTTLE2__:
        case BOTTLE3__:
        case BOTTLE4__:
        case BOTTLE5__:
        case BOTTLE6__:
        case BOTTLE7__:
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
        case OCEANSPRITE1__:
        case OCEANSPRITE2__:
        case OCEANSPRITE3__:
        case OCEANSPRITE5__:
        case MONK__:
        case INDY__:
        case LUKE__:
        case JURYGUY__:
        case SCALE__:
        case VACUUM__:
        case FANSPRITE__:
        case CACTUS__:
        case CACTUSBROKE__:
        case HANGLIGHT__:
        case FETUS__:
        case FETUSBROKE__:
        case CAMERALIGHT__:
        case MOVIECAMERA__:
        case IVUNIT__:
        case POT1__:
        case POT2__:
        case POT3__:
        case TRIPODCAMERA__:
        case SUSHIPLATE1__:
        case SUSHIPLATE2__:
        case SUSHIPLATE3__:
        case SUSHIPLATE4__:
        case SUSHIPLATE5__:
        case WAITTOBESEATED__:
        case VASE__:
        case PIPE1__:
        case PIPE2__:
        case PIPE3__:
        case PIPE4__:
        case PIPE5__:
        case PIPE6__:
            pSprite->clipdist = 32;
            pSprite->cstat |= (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN);
            fallthrough__;
        case OCEANSPRITE4__:
            changespritestat(newSprite, STAT_DEFAULT);
            goto SPAWN_END;

        case DUKETAG__:
        case SIGN1__:
        case SIGN2__:
            if ((ud.multimode < 2 || GTFLAGS(GAMETYPE_COOP)) && pSprite->pal != 0)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
            }
            else pSprite->pal = 0;
            goto SPAWN_END;

        case MASKWALL1__:
        case MASKWALL2__:
        case MASKWALL3__:
        case MASKWALL4__:
        case MASKWALL5__:
        case MASKWALL6__:
        case MASKWALL7__:
        case MASKWALL8__:
        case MASKWALL9__:
        case MASKWALL10__:
        case MASKWALL11__:
        case MASKWALL12__:
        case MASKWALL13__:
        case MASKWALL14__:
        case MASKWALL15__:
        {
            int const cstat_mask = pSprite->cstat & (CSTAT_SPRITE_XFLIP | CSTAT_SPRITE_YFLIP | CSTAT_SPRITE_ALIGNMENT);
            pSprite->cstat = cstat_mask | CSTAT_SPRITE_BLOCK;
            changespritestat(newSprite, STAT_DEFAULT);
            goto SPAWN_END;
        }

        case FRAMEEFFECT1_13__:
            if (PLUTOPAK)
                break;
            fallthrough__;
        case FRAMEEFFECT1__:
            if (hasParent)
            {
                pSprite->xrepeat = pParent->xrepeat;
                pSprite->yrepeat = pParent->yrepeat;
                T2(newSprite) = pParent->picnum;
            }
            else pSprite->xrepeat = pSprite->yrepeat = 0;

            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case FOOTPRINTS__:
        case FOOTPRINTS2__:
        case FOOTPRINTS3__:
        case FOOTPRINTS4__:
            if (hasParent)
            {
                if(!OnEvenGround(pSprite, 84))
                {
                    pSprite->xrepeat = pSprite->yrepeat = 0;
                    goto SPAWN_END;
                }

                pSprite->cstat = CSTAT_SPRITE_ALIGNMENT_FLOOR + ((g_player[pParent->yvel].ps->footprintcount & 1) << 2);
                pSprite->ang = pParent->ang;
            }

            pSprite->z = sector[sectNum].floorz;
            if (sector[sectNum].lotag != ST_1_ABOVE_WATER && sector[sectNum].lotag != ST_2_UNDERWATER)
                pSprite->xrepeat = pSprite->yrepeat = 32;

            A_AddToDeleteQueue(newSprite);
            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case FEM1__:
        case FEM2__:
        case FEM3__:
        case FEM4__:
        case FEM5__:
        case FEM6__:
        case FEM7__:
        case FEM8__:
        case FEM9__:
        case FEM10__:
        case PODFEM1__:
        case NAKED1__:
        case STATUE__:
        case TOUGHGAL__:
            pSprite->yvel = pSprite->hitag;
            pSprite->hitag = -1;
            if (pSprite->picnum == PODFEM1)
                pSprite->extra <<= 1;
            fallthrough__;
        case BLOODYPOLE__:
        case QUEBALL__:
        case STRIPEBALL__:

            if (pSprite->picnum == QUEBALL || pSprite->picnum == STRIPEBALL)
            {
                pSprite->cstat = CSTAT_SPRITE_BLOCK_HITSCAN;
                pSprite->clipdist = 8;
            }
            else
            {
                pSprite->cstat |= (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN);
                pSprite->clipdist = 32;
            }

            changespritestat(newSprite, STAT_ZOMBIEACTOR);
            goto SPAWN_END;

        case DUKELYINGDEAD__:
            if (hasParent && pParent->picnum == APLAYER)
            {
                pSprite->xrepeat = pParent->xrepeat;
                pSprite->yrepeat = pParent->yrepeat;
                pSprite->shade = pParent->shade;
                pSprite->pal = g_player[pParent->yvel].ps->palookup;
            }
            fallthrough__;
        case DUKECAR__:
        case HELECOPT__:
            //                if(sp->picnum == HELECOPT || sp->picnum == DUKECAR) sp->xvel = 1024;
            pSprite->cstat = 0;
            pSprite->extra = 1;
            pSprite->xvel = 292;
            pSprite->zvel = 360;
            fallthrough__;
        case BLIMP__:
            pSprite->cstat |= (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN);
            pSprite->clipdist = 128;
            changespritestat(newSprite, STAT_ACTOR);
            goto SPAWN_END;

        case RESPAWNMARKERRED__:
            pSprite->xrepeat = pSprite->yrepeat = 24;
            if (hasParent)
                pSprite->z = actor[spriteNum].floorz; // -(1<<4);
            changespritestat(newSprite, STAT_ACTOR);
            goto SPAWN_END;

        case MIKE__:
            pSprite->yvel = pSprite->hitag;
            pSprite->hitag = 0;
            changespritestat(newSprite, STAT_ACTOR);
            goto SPAWN_END;

        case WEATHERWARN__:
            changespritestat(newSprite, STAT_ACTOR);
            goto SPAWN_END;

        case SPOTLITE__:
            T1(newSprite) = pSprite->x;
            T2(newSprite) = pSprite->y;
            goto SPAWN_END;

        case BULLETHOLE__:
            pSprite->xrepeat = pSprite->yrepeat = 3;
            pSprite->cstat = CSTAT_SPRITE_ALIGNMENT_WALL + (krand() & 12);
            A_AddToDeleteQueue(newSprite);
            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case MONEY__:
        case MAIL__:
        case PAPER__:
            pActor->t_data[0] = krand() & 2047;
            pSprite->cstat = krand() & 12;
            pSprite->xrepeat = pSprite->yrepeat = 8;
            pSprite->ang = krand() & 2047;

            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case VIEWSCREEN__:
        case VIEWSCREEN2__:
            pSprite->owner = newSprite;
            pSprite->lotag = pSprite->extra = 1;
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case SHELL__: //From the player
        case SHOTGUNSHELL__:
            if (hasParent)
            {
                int snum, a;

                if (pParent->picnum == APLAYER)
                {
                    snum = pParent->yvel;
                    a = fix16_to_int(g_player[snum].ps->q16ang) - (krand() & 63) + 8;  //Fine tune

                    T1(newSprite) = krand() & 1;
                    pSprite->z = (3 << 8) + g_player[snum].ps->pyoff + g_player[snum].ps->pos.z -
                        (fix16_to_int(g_player[snum].ps->q16horizoff + g_player[snum].ps->q16horiz - F16(100)) << 4);
                    if (pSprite->picnum == SHOTGUNSHELL)
                        pSprite->z += (3 << 8);
                    pSprite->zvel = -(krand() & 255);
                }
                else
                {
                    a = pSprite->ang;
                    pSprite->z = pParent->z - PHEIGHT + (3 << 8);
                }

                pSprite->x = pParent->x + (sintable[(a + 512) & 2047] >> 7);
                pSprite->y = pParent->y + (sintable[a & 2047] >> 7);

                pSprite->shade = -8;

                if (pSprite->yvel == 1 || NAM_WW2GI)
                {
                    pSprite->ang = a + 512;
                    pSprite->xvel = 30;
                }
                else
                {
                    pSprite->ang = a - 512;
                    pSprite->xvel = 20;
                }
                pSprite->xrepeat = pSprite->yrepeat = 4;

                changespritestat(newSprite, STAT_MISC);
            }
            goto SPAWN_END;

        case WATERBUBBLE__:
            if (hasParent)
            {
                if (pParent->picnum == APLAYER)
                    pSprite->z -= (16 << 8);

                pSprite->ang = pParent->ang;
            }

            pSprite->xrepeat = pSprite->yrepeat = 4;

            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case RESPAWN__:
            pSprite->extra = 66 - 13;
            fallthrough__;
        case MUSICANDSFX__:
            if (ud.multimode < 2 && pSprite->pal == 1)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }
            pSprite->cstat = CSTAT_SPRITE_INVISIBLE;
            changespritestat(newSprite, STAT_FX);
            goto SPAWN_END;

        case EXPLOSION2__:
        case EXPLOSION2BOT__:
        case BURNING__:
        case BURNING2__:
        case SMALLSMOKE__:
        case SHRINKEREXPLOSION__:
        case COOLEXPLOSION1__:
            if (hasParent)
            {
                pSprite->ang = pParent->ang;
                pSprite->shade = -64;
                pSprite->cstat = CSTAT_SPRITE_YCENTER | (krand() & 4);

                int32_t floorZ = yax_getflorzofslope(pSprite->sectnum, pSprite->xy);
                if (pSprite->z > floorZ - (12 << 8))
                    pSprite->z = floorZ - (12 << 8);
            }

            if (pSprite->picnum == EXPLOSION2 || pSprite->picnum == EXPLOSION2BOT)
            {
                pSprite->xrepeat = 48;
                pSprite->yrepeat = 48;
                pSprite->shade = -127;
                pSprite->cstat |= CSTAT_SPRITE_YCENTER;
            }
            else if (pSprite->picnum == SHRINKEREXPLOSION)
            {
                pSprite->xrepeat = 32;
                pSprite->yrepeat = 32;
            }
            else if (pSprite->picnum == SMALLSMOKE)
            {
                // 64 "money"
                pSprite->xrepeat = 24;
                pSprite->yrepeat = 24;
            }
            else if (pSprite->picnum == BURNING || pSprite->picnum == BURNING2)
            {
                pSprite->xrepeat = 4;
                pSprite->yrepeat = 4;
            }

            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;

        case PLAYERONWATER__:
            if (hasParent)
            {
                pSprite->xrepeat = pParent->xrepeat;
                pSprite->yrepeat = pParent->yrepeat;
                pSprite->zvel = 128;
                if (sector[pSprite->sectnum].lotag != ST_2_UNDERWATER)
                    pSprite->cstat |= CSTAT_SPRITE_INVISIBLE;
            }
            changespritestat(newSprite, STAT_DUMMYPLAYER);
            goto SPAWN_END;

        case APLAYER__:
        {
            pSprite->xrepeat = pSprite->yrepeat = 0;

            bool const invalidateSpawn = (ud.multimode < 2 || (ud.multimode > 1 && pSprite->lotag != !!(GTFLAGS(GAMETYPE_COOPSPAWN))));
            changespritestat(newSprite, invalidateSpawn ? STAT_MISC : STAT_PLAYER);
            goto SPAWN_END;
        }

        case CRANE__:
        {
            pSprite->cstat |= (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_ONE_SIDED | CSTAT_SPRITE_BLOCK_HITSCAN);

            pSprite->picnum += 2;
            pSprite->owner = -1;
            pSprite->extra = 8;
            pSprite->z = sector[sectNum].ceilingz + (48 << 8);

            pActor->t_data[3] = -1; // Player ID 
            pActor->t_data[4] = tempwallptr;

            g_origins[tempwallptr].x = pSprite->x;
            g_origins[tempwallptr].y = pSprite->y;
            g_origins[tempwallptr + 2].x = pSprite->z;

            // Find crane target
            for(int SPRITES_OF(STAT_DEFAULT, s))
            {
                if (sprite[s].picnum == CRANEPOLE && SHT(newSprite) == (sprite[s].hitag))
                {
                    g_origins[tempwallptr + 2].y = s;

                    T2(newSprite) = sprite[s].sectnum;

                    sprite[s].xrepeat = 48;
                    sprite[s].yrepeat = 128;

                    g_origins[tempwallptr + 1].x = sprite[s].x;
                    g_origins[tempwallptr + 1].y = sprite[s].y;

                    sprite[s].xyz = pSprite->xyz;
                    sprite[s].shade = pSprite->shade;

                    setspritez(s, &sprite[s].xyz);
                    break;
                }
            }
            tempwallptr += 3;
        
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;
        }

        case WATERDRIP__:
        {
            if (hasParent && (pParent->statnum == STAT_PLAYER || pParent->statnum == STAT_ACTOR))
            {
                pSprite->shade = 32;
                if (pParent->pal != 1)
                {
                    pSprite->pal = 2;
                    pSprite->z -= (18 << 8);
                }
                else pSprite->z -= (13 << 8);
                pSprite->ang = getangle(g_player[connecthead].ps->pos.x - pSprite->x, g_player[connecthead].ps->pos.y - pSprite->y);
                pSprite->xvel = 48 - (krand() & 31);
                A_SetSprite(newSprite, CLIPMASK0);
            }
            else if (!hasParent)
            {
                pSprite->z += (4 << 8);
                T1(newSprite) = pSprite->z;
                T2(newSprite) = krand() & 127;
            }

            pSprite->xrepeat = 24;
            pSprite->yrepeat = 24;

            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;
        }

        case TRASH__:
            pSprite->ang = krand() & 2047;

            pSprite->xrepeat = 24;
            pSprite->yrepeat = 24;

            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case WATERDRIPSPLASH__:
            pSprite->xrepeat = 24;
            pSprite->yrepeat = 24;

            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case PLUG__:
            pSprite->lotag = 9999;
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case TOUCHPLATE__:
            T3(newSprite) = sector[sectNum].floorz;
            if (sector[sectNum].lotag != ST_1_ABOVE_WATER && sector[sectNum].lotag != ST_2_UNDERWATER)
                sector[sectNum].floorz = pSprite->z;
            if ((pSprite->pal == 1 && ud.multimode > 1) ||
                (pSprite->pal == 2 && (!GTFLAGS(GAMETYPE_COOP) || ud.multimode < 2)) || // World Tour
                (pSprite->pal == 3 && (GTFLAGS(GAMETYPE_COOP) || ud.multimode < 2)) || // World Tour
                (pSprite->pal >= 4 && ud.multimode > 1)
                )
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }

            pSprite->cstat |= CSTAT_SPRITE_INVISIBLE;
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case WATERBUBBLEMAKER__:
            if (pSprite->hitag)
            {
                // JBF 20030913: Pisses off X_Move(), eg. in bobsp2
                OSD_Printf(OSD_ERROR "WARNING: WATERBUBBLEMAKER %d @ %d,%d with hitag!=0. Applying fixup.\n",
                    newSprite, TrackerCast(pSprite->x), TrackerCast(pSprite->y));
                pSprite->hitag = 0;
            }

            pSprite->cstat |= CSTAT_SPRITE_INVISIBLE;
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case MASTERSWITCH__:
            if (pSprite->picnum == MASTERSWITCH)
                pSprite->cstat |= CSTAT_SPRITE_INVISIBLE;
            pSprite->yvel = 0;
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case TARGET__:
        case DUCK__:
        case LETTER__:
            pSprite->extra = 1;
            pSprite->cstat |= (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN);
            changespritestat(newSprite, STAT_ACTOR);
            goto SPAWN_END;

        case OCTABRAINSTAYPUT__:
        case LIZTROOPSTAYPUT__:
        case PIGCOPSTAYPUT__:
        case LIZMANSTAYPUT__:
        case BOSS1STAYPUT__:
        case PIGCOPDIVE__:
        case COMMANDERSTAYPUT__:
        case BOSS4STAYPUT__:
            pActor->actorstayput = pSprite->sectnum;
            fallthrough__;
        case BOSS1__:
        case BOSS2__:
        case BOSS3__:
        case BOSS4__:
        case ROTATEGUN__:
        case GREENSLIME__:
            if (pSprite->picnum == GREENSLIME)
                pSprite->extra = 1;
            fallthrough__;
        case DRONE__:
        case LIZTROOPONTOILET__:
        case LIZTROOPJUSTSIT__:
        case LIZTROOPSHOOT__:
        case LIZTROOPJETPACK__:
        case LIZTROOPDUCKING__:
        case LIZTROOPRUNNING__:
        case LIZTROOP__:
        case OCTABRAIN__:
        case COMMANDER__:
        case PIGCOP__:
        case LIZMAN__:
        case LIZMANSPITTING__:
        case LIZMANFEEDING__:
        case LIZMANJUMP__:
        case ORGANTIC__:
        case RAT__:
        case SHARK__:
            if (pSprite->pal == 0)
            {
                switch (tileGetMapping(pSprite->picnum))
                {
                    case LIZTROOPONTOILET__:
                    case LIZTROOPSHOOT__:
                    case LIZTROOPJETPACK__:
                    case LIZTROOPDUCKING__:
                    case LIZTROOPRUNNING__:
                    case LIZTROOPSTAYPUT__:
                    case LIZTROOPJUSTSIT__:
                    case LIZTROOP__:
                        pSprite->pal = 22;
                        break;
                }
            }
            else
            {
                if (!PLUTOPAK)
                    pSprite->extra <<= 1;
            }

            if (pSprite->picnum == BOSS4STAYPUT || pSprite->picnum == BOSS1 || pSprite->picnum == BOSS2 || pSprite->picnum == BOSS1STAYPUT || pSprite->picnum == BOSS3 || pSprite->picnum == BOSS4)
            {
                if (hasParent && pParent->picnum == RESPAWN)
                    pSprite->pal = pParent->pal;

                if (pSprite->pal)
                {
                    pSprite->clipdist = 80;
                    pSprite->xrepeat = 40;
                    pSprite->yrepeat = 40;
                }
                else
                {
                    pSprite->xrepeat = 80;
                    pSprite->yrepeat = 80;
                    pSprite->clipdist = 164;
                }
            }
            else
            {
                if (pSprite->picnum != SHARK)
                {
                    pSprite->xrepeat = 40;
                    pSprite->yrepeat = 40;
                    pSprite->clipdist = 80;
                }
                else
                {
                    pSprite->xrepeat = 60;
                    pSprite->yrepeat = 60;
                    pSprite->clipdist = 40;
                }
            }

            if (hasParent)
                pSprite->lotag = 0;

            if ((pSprite->lotag > ud.player_skill) || DMFLAGS_TEST(DMFLAG_NOMONSTERS))
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }
            else
            {
                A_Fall(newSprite);

                if (pSprite->picnum == RAT)
                {
                    pSprite->ang = krand() & 2047;
                    pSprite->xrepeat = pSprite->yrepeat = 48;
                    pSprite->cstat = 0;
                }
                else
                {
                    pSprite->cstat |= (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN);

                    if (pSprite->picnum != SHARK)
                        ud.total_monsters++;
                }

                if (pSprite->picnum == ORGANTIC)
                    pSprite->cstat |= CSTAT_SPRITE_YCENTER;

                if (hasParent)
                {
                    pActor->timetosleep = 0;
                    A_PlayAlertSound(newSprite);
                    changespritestat(newSprite, STAT_ACTOR);
                }
                else changespritestat(newSprite, STAT_ZOMBIEACTOR);
            }

            if (pSprite->picnum == ROTATEGUN)
                pSprite->zvel = 0;

            goto SPAWN_END;

        case LOCATORS__:
            pSprite->cstat |= CSTAT_SPRITE_INVISIBLE;
            changespritestat(newSprite, STAT_LOCATOR);
            goto SPAWN_END;

        case ACTIVATORLOCKED__:
        case ACTIVATOR__:
            pSprite->cstat = CSTAT_SPRITE_INVISIBLE;
            if (pSprite->picnum == ACTIVATORLOCKED)
                sector[pSprite->sectnum].lotag |= 16384;
            changespritestat(newSprite, STAT_ACTIVATOR);
            goto SPAWN_END;

        case DOORSHOCK__:
            pSprite->cstat |= (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN);
            pSprite->shade = -12;
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case OOZ__:
        case OOZ2__:
        {
            pSprite->shade = -12;

            if (hasParent)
            {
                if (pParent->picnum == NUKEBARREL)
                    pSprite->pal = 8;
                A_AddToDeleteQueue(newSprite);
            }

            changespritestat(newSprite, STAT_ACTOR);
            A_GetZLimits(newSprite);

            int const oozSize = (pActor->floorz - pActor->ceilingz) >> 9;

            pSprite->yrepeat = oozSize;
            pSprite->xrepeat = 25 - (oozSize >> 1);
            pSprite->cstat |= (krand() & 4);
            goto SPAWN_END;
        }

        case HEAVYHBOMB__:
            if (hasParent)
                pSprite->owner = spriteNum;
            else
                pSprite->owner = newSprite;

            pSprite->xrepeat = pSprite->yrepeat = 9;
            pSprite->yvel = 4;
            pSprite->cstat |= (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN);

            if ((ud.multimode < 2 || DMFLAGS_TEST(DMFLAG_NODMWEAPONSPAWNS)) && pSprite->pal != 0)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }
            pSprite->pal = 0;
            pSprite->shade = -17;
            changespritestat(newSprite, STAT_ZOMBIEACTOR);
            goto SPAWN_END;

        case REACTOR2__:
        case REACTOR__:
        case RECON__:
            if (pSprite->picnum == RECON)
            {
                if (pSprite->lotag > ud.player_skill)
                {
                    pSprite->xrepeat = pSprite->yrepeat = 0;
                    changespritestat(newSprite, STAT_MISC);
                    goto SPAWN_END;
                }
                ud.total_monsters++;
                pActor->t_data[5] = 0;
                if (DMFLAGS_TEST(DMFLAG_NOMONSTERS))
                {
                    pSprite->xrepeat = pSprite->yrepeat = 0;
                    changespritestat(newSprite, STAT_MISC);
                    goto SPAWN_END;
                }
                pSprite->extra = 130;
            }

            if (pSprite->picnum == REACTOR || pSprite->picnum == REACTOR2)
                pSprite->extra = g_impactDamage;

            pSprite->cstat |= (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN); // Make it hittable

            if ((ud.multimode < 2 || DMFLAGS_TEST(DMFLAG_NODMWEAPONSPAWNS)) && pSprite->pal != 0)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }

            pSprite->pal = 0;
            pSprite->shade = -17;

            changespritestat(newSprite, STAT_ZOMBIEACTOR);
            goto SPAWN_END;

        case ATOMICHEALTH__:
        case STEROIDS__:
        case HEATSENSOR__:
        case SHIELD__:
        case AIRTANK__:
        case TRIPBOMBSPRITE__:
        case JETPACK__:
        case HOLODUKE__:

        case FIRSTGUNSPRITE__:
        case CHAINGUNSPRITE__:
        case SHOTGUNSPRITE__:
        case RPGSPRITE__:
        case SHRINKERSPRITE__:
        case FREEZESPRITE__:
        case DEVISTATORSPRITE__:

        case SHOTGUNAMMO__:
        case FREEZEAMMO__:
        case HBOMBAMMO__:
        case CRYSTALAMMO__:
        case GROWAMMO__:
        case BATTERYAMMO__:
        case DEVISTATORAMMO__:
        case RPGAMMO__:
        case BOOTS__:
        case AMMO__:
        case AMMOLOTS__:
        case COLA__:
        case FIRSTAID__:
        case SIXPAK__:

            if (hasParent)
            {
                pSprite->lotag = 0;
                pSprite->z -= (32 << 8);
                pSprite->zvel = -1024;
                A_SetSprite(newSprite, CLIPMASK0);
                pSprite->cstat = krand() & 4;
            }
            else
            {
                pSprite->owner = newSprite;
                pSprite->cstat = 0;
            }

            if (((ud.multimode < 2 || DMFLAGS_TEST(DMFLAG_NODMWEAPONSPAWNS)) && pSprite->pal != 0) || (pSprite->lotag > ud.player_skill))
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }

            pSprite->pal = 0;
            fallthrough__;

        case ACCESSCARD__:

            if (pSprite->picnum == ATOMICHEALTH)
                pSprite->cstat |= CSTAT_SPRITE_YCENTER;

            if (ud.multimode > 1 && !GTFLAGS(GAMETYPE_ACCESSCARDSPRITES) && pSprite->picnum == ACCESSCARD)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }
            else
            {
                if (pSprite->picnum == AMMO)
                    pSprite->xrepeat = pSprite->yrepeat = 16;
                else pSprite->xrepeat = pSprite->yrepeat = 32;
            }

            pSprite->shade = -17;

            if (hasParent)
                changespritestat(newSprite, STAT_ACTOR);
            else
            {
                changespritestat(newSprite, STAT_ZOMBIEACTOR);
                A_Fall(newSprite);
            }
            goto SPAWN_END;

        case WATERFOUNTAIN__:
            pSprite->lotag = 1;
            fallthrough__;

        case TREE1__:
        case TREE2__:
        case TIRE__:
        case CONE__:
        case BOX__:
            pSprite->cstat = (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN); // Make it hittable
            pSprite->extra = 1;
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case FLOORFLAME__:
            pSprite->shade = -127;
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case BOUNCEMINE__:
            if(!hasParent)
                pSprite->owner = newSprite;

            pSprite->cstat |= (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN); //Make it hittable
            pSprite->xrepeat = pSprite->yrepeat = 24;
            pSprite->shade = -127;
            pSprite->extra = g_impactDamage << 2;
            changespritestat(newSprite, STAT_ZOMBIEACTOR);
            goto SPAWN_END;

        case STEAM__:
            if (hasParent)
            {
                pSprite->ang = pParent->ang;
                pSprite->cstat = (CSTAT_SPRITE_TRANSLUCENT | CSTAT_SPRITE_ALIGNMENT_WALL | CSTAT_SPRITE_YCENTER);
                pSprite->xrepeat = pSprite->yrepeat = 1;
                pSprite->xvel = -8;
                A_SetSprite(newSprite, CLIPMASK0);
            }
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case CEILINGSTEAM__:
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case SECTOREFFECTOR__:
        {
            pSprite->cstat |= CSTAT_SPRITE_INVISIBLE;
            pSprite->xrepeat = pSprite->yrepeat = 0;

            switch (pSprite->lotag)
            {
                case SE_40_ROR_UPPER:
                case SE_41_ROR_LOWER:
                    pSprite->cstat = 32;
                    pSprite->xrepeat = pSprite->yrepeat = 64;
                    changespritestat(newSprite, STAT_EFFECTOR);

                    for (int otherSprite = 0; otherSprite < MAXSPRITES; otherSprite++)
                    {
                        if (sprite[otherSprite].picnum != SECTOREFFECTOR || otherSprite == newSprite)
                            continue;

                        if ((sprite[otherSprite].lotag == SE_40_ROR_UPPER || sprite[otherSprite].lotag == SE_41_ROR_LOWER) && sprite[otherSprite].hitag == pSprite->hitag)
                        {
                            // LOG_F("found ror match\n");
                            pSprite->yvel = otherSprite;
                            break;
                        }
                    }
                    goto SPAWN_END;

                case SE_46_ROR_PROTECTED:
                    ror_protectedsectors[pSprite->sectnum] = 1;
                    changespritestat(newSprite, STAT_EFFECTOR);
                    goto SPAWN_END;

                case SE_49_POINT_LIGHT:
                case SE_50_SPOT_LIGHT:
                {
                    for (int SPRITES_OF_SECT(pSprite->sectnum, otherSprite))
                        if (sprite[otherSprite].picnum == ACTIVATOR || sprite[otherSprite].picnum == ACTIVATORLOCKED)
                            pActor->flags |= SFLAG_USEACTIVATOR;

                    changespritestat(newSprite, STAT_LIGHT);
                    goto SPAWN_END;
                }
            }

            pSprite->yvel = sector[sectNum].extra;

            switch (pSprite->lotag)
            {
                case SE_28_LIGHTNING:
                    T6(newSprite) = 65;// Delay for lightning
                    break;
                case SE_7_TELEPORT: // Transporters!!!!
                case SE_23_ONE_WAY_TELEPORT:// XPTR END
                    if (pSprite->lotag != SE_23_ONE_WAY_TELEPORT)
                    {
                        for (int otherSprite = 0; otherSprite < MAXSPRITES; otherSprite++)
                        {
                            if (sprite[otherSprite].statnum == MAXSTATUS || sprite[otherSprite].picnum != SECTOREFFECTOR || otherSprite == newSprite)
                                continue;

                            if ((sprite[otherSprite].lotag == SE_7_TELEPORT || sprite[otherSprite].lotag == SE_23_ONE_WAY_TELEPORT) && sprite[otherSprite].hitag == pSprite->hitag)
                            {
                                pSprite->owner = otherSprite;
                                break;
                            }
                        }
                    }
                    else pSprite->owner = newSprite;

                    T5(newSprite) = sector[sectNum].floorz == pSprite->z;
                    pSprite->cstat = 0;
                    changespritestat(newSprite, STAT_TRANSPORT);
                    goto SPAWN_END;
                case SE_1_PIVOT:
                    pSprite->owner = -1;
                    T1(newSprite) = 1;
                    break;
                case SE_18_INCREMENTAL_SECTOR_RISE_FALL:

                    if (pSprite->ang == 512)
                    {
                        T2(newSprite) = sector[sectNum].ceilingz;
                        if (pSprite->pal)
                            sector[sectNum].ceilingz = pSprite->z;
                    }
                    else
                    {
                        T2(newSprite) = sector[sectNum].floorz;
                        if (pSprite->pal)
                            sector[sectNum].floorz = pSprite->z;
                    }

                    pSprite->hitag <<= 2;
                    break;

                case SE_19_EXPLOSION_LOWERS_CEILING:
                    pSprite->owner = -1;
                    break;
                case SE_25_PISTON: // Pistons
                    T4(newSprite) = sector[sectNum].ceilingz;
                    T5(newSprite) = 1;
                    sector[sectNum].ceilingz = pSprite->z;
                    G_SetInterpolation(&sector[sectNum].ceilingz);
                    break;
                case SE_35:
                    sector[sectNum].ceilingz = pSprite->z;
                    break;
                case SE_27_DEMO_CAM:
                    if (ud.recstat == 1)
                    {
                        pSprite->xrepeat = pSprite->yrepeat = 64;
                        pSprite->cstat &= ~(CSTAT_SPRITE_INVISIBLE);
                    }
                    break;
                case SE_12_LIGHT_SWITCH:

                    T2(newSprite) = sector[sectNum].floorshade;
                    T3(newSprite) = sector[sectNum].ceilingshade;
                    break;

                case SE_13_EXPLOSIVE:
                {
                    T1(newSprite) = sector[sectNum].ceilingz;
                    T2(newSprite) = sector[sectNum].floorz;

                    if (klabs(T1(newSprite) - pSprite->z) < klabs(T2(newSprite) - pSprite->z))
                        pSprite->owner = 1;
                    else pSprite->owner = 0;

                    if (pSprite->ang == 512)
                    {
                        if (pSprite->owner)
                            sector[sectNum].ceilingz = pSprite->z;
                        else
                            sector[sectNum].floorz = pSprite->z;
                    }
                    else
                        sector[sectNum].ceilingz = sector[sectNum].floorz = pSprite->z;

                    if (sector[sectNum].ceilingstat & 1)
                    {
                        sector[sectNum].ceilingstat ^= 1;
                        T4(newSprite) = 1;

                        if (!pSprite->owner && pSprite->ang == 512)
                        {
                            sector[sectNum].ceilingstat ^= 1;
                            T4(newSprite) = 0;
                        }

                        sector[sectNum].ceilingshade = sector[sectNum].floorshade;

                        if (pSprite->ang == 512)
                        {
                            int const startwall = sector[sectNum].wallptr;
                            int const endwall = startwall + sector[sectNum].wallnum;
                            for (int j = startwall; j < endwall; j++)
                            {
                                int nextSect = wall[j].nextsector;
                                if (nextSect >= 0)
                                {
                                    if (!(sector[nextSect].ceilingstat & 1))
                                    {
                                        sector[sectNum].ceilingpicnum = sector[nextSect].ceilingpicnum;
                                        sector[sectNum].ceilingshade = sector[nextSect].ceilingshade;
                                        break; //Leave earily
                                    }
                                }
                            }
                        }
                    }

                    break;
                }

                case SE_17_WARP_ELEVATOR:
                {
                    T3(newSprite) = sector[sectNum].floorz; //Stopping loc

                    int nextSectNum = nextsectorneighborz(sectNum, sector[sectNum].floorz, -1, -1);
                    if (nextSectNum >= 0)
                    {
                        T4(newSprite) = sector[nextSectNum].ceilingz;
                    }
                    else
                    {
                        // use elevator sector's ceiling as heuristic
                        T4(newSprite) = sector[sectNum].ceilingz;

                        OSD_Printf(OSD_ERROR "WARNING: SE17 sprite %d using own sector's ceilingz to "
                            "determine when to warp. Sector %d adjacent to a door?\n", newSprite, sectNum);
                    }

                    nextSectNum = nextsectorneighborz(sectNum, sector[sectNum].ceilingz, 1, 1);
                    if (nextSectNum >= 0)
                        T5(newSprite) = sector[nextSectNum].floorz;
                    else
                    {
                        // XXX: we should return to the menu for this and similar failures
                        Bsprintf(tempbuf, "SE 17 (warp elevator) setup failed: sprite %d at (%d, %d)",
                            newSprite, TrackerCast(pSprite->x), TrackerCast(pSprite->y));
                        G_GameExit(tempbuf);
                    }

                    G_SetInterpolation(&sector[sectNum].floorz);
                    G_SetInterpolation(&sector[sectNum].ceilingz);

                    break;
                }

                case SE_24_CONVEYOR:
                {
                    pSprite->yvel <<= 1;
                    break;
                }
                case SE_36_PROJ_SHOOTER:
                {
                    break;
                }

                case SE_20_STRETCH_BRIDGE:
                {
                    int       closestDist   = INT32_MAX;
                    int       closestWall   = 0;
                    int const startwall     = sector[sectNum].wallptr;
                    int const endwall       = startwall + sector[sectNum].wallnum;

                    //find the two most clostest wall x's and y's

                    for (int s = startwall; s < endwall; s++)
                    {
                        int const x = wall[s].x;
                        int const y = wall[s].y;
                        int const d = FindDistance2D(pSprite->x - x, pSprite->y - y);

                        if (d < closestDist)
                        {
                            closestDist = d;
                            closestWall = s;
                        }
                    }

                    T2(newSprite) = closestWall;

                    closestDist = INT32_MAX;

                    for (int s = startwall; s < endwall; s++)
                    {
                        int const x = wall[s].x;
                        int const y = wall[s].y;
                        int const d = FindDistance2D(pSprite->x - x, pSprite->y - y);

                        if (d < closestDist && s != T2(newSprite))
                        {
                            closestDist = d;
                            closestWall = s;
                        }
                    }

                    T3(newSprite) = closestWall;
                    break;
                }

                case SE_3_RANDOM_LIGHTS_AFTER_SHOT_OUT:
                {
                    T4(newSprite) = sector[sectNum].floorshade;

                    sector[sectNum].floorshade = pSprite->shade;
                    sector[sectNum].ceilingshade = pSprite->shade;

                    pSprite->owner = sector[sectNum].ceilingpal << 8;
                    pSprite->owner |= sector[sectNum].floorpal;

                    //fix all the walls;

                    int const startwall = sector[sectNum].wallptr;
                    int const endwall = startwall + sector[sectNum].wallnum;

                    for (int s = startwall; s < endwall; s++)
                    {
                        if (!(wall[s].hitag & 1))
                            wall[s].shade = pSprite->shade;
                        if ((wall[s].cstat & 2) && wall[s].nextwall >= 0)
                            wall[wall[s].nextwall].shade = pSprite->shade;
                    }
                    break;
                }

                case SE_31_FLOOR_RISE_FALL:
                {
                    T2(newSprite) = sector[sectNum].floorz;
                    //    T3(i) = sp->hitag;
                    if (pSprite->ang != 1536) sector[sectNum].floorz = pSprite->z;

                    int const startwall = sector[sectNum].wallptr;
                    int const endwall = startwall + sector[sectNum].wallnum;

                    for (int s = startwall; s < endwall; s++)
                        if (wall[s].hitag == 0) wall[s].hitag = 9999;

                    G_SetInterpolation(&sector[sectNum].floorz);

                    break;
                }

                case SE_32_CEILING_RISE_FALL:
                {
                    T2(newSprite) = sector[sectNum].ceilingz;
                    T3(newSprite) = pSprite->hitag;
                    if (pSprite->ang != 1536)
                        sector[sectNum].ceilingz = pSprite->z;

                    int const startwall = sector[sectNum].wallptr;
                    int const endwall = startwall + sector[sectNum].wallnum;

                    for (int s = startwall; s < endwall; s++)
                        if (wall[s].hitag == 0) wall[s].hitag = 9999;

                    G_SetInterpolation(&sector[sectNum].ceilingz);

                    break;
                }

                case SE_4_RANDOM_LIGHTS: //Flashing lights
                {
                    T3(newSprite) = sector[sectNum].floorshade;

                    int const startwall = sector[sectNum].wallptr;
                    int const endwall = startwall + sector[sectNum].wallnum;

                    pSprite->owner = sector[sectNum].ceilingpal << 8;
                    pSprite->owner |= sector[sectNum].floorpal;

                    for (int s = startwall; s < endwall; s++)
                        if (wall[s].shade > T4(newSprite))
                            T4(newSprite) = wall[s].shade;

                    break;
                }

                case SE_9_DOWN_OPEN_DOOR_LIGHTS:
                    if (sector[sectNum].lotag &&
                        labs(sector[sectNum].ceilingz - pSprite->z) > 1024)
                        sector[sectNum].lotag |= 32768; //If its open
                    fallthrough__;
                case SE_8_UP_OPEN_DOOR_LIGHTS:
                {
                    //First, get the ceiling-floor shade
                    T1(newSprite) = sector[sectNum].floorshade;
                    T2(newSprite) = sector[sectNum].ceilingshade;

                    int const startwall = sector[sectNum].wallptr;
                    int const endwall = startwall + sector[sectNum].wallnum;

                    for (int s = startwall; s < endwall; s++)
                        if (wall[s].shade > T3(newSprite))
                            T3(newSprite) = wall[s].shade;

                    T4(newSprite) = 1; //Take Out;

                    break;
                }

                case SE_11_SWINGING_DOOR://Pivitor rotater
                    if (pSprite->ang > 1024) T4(newSprite) = 2;
                    else T4(newSprite) = -2;
                    fallthrough__;
                case SE_0_ROTATING_SECTOR:
                case SE_2_EARTHQUAKE://Earthquakemakers
                case SE_5://Boss Creature
                case SE_6_SUBWAY://Subway
                case SE_14_SUBWAY_CAR://Caboos
                case SE_15_SLIDING_DOOR://Subwaytype sliding door
                case SE_16_REACTOR://That rotating blocker reactor thing
                case SE_26://ESCELATOR
                case SE_30_TWO_WAY_TRAIN://No rotational subways
                {
                    if (pSprite->lotag == SE_0_ROTATING_SECTOR)
                    {
                        if (sector[sectNum].lotag == ST_30_ROTATE_RISE_BRIDGE)
                        {
                            if (pSprite->pal)
                                pSprite->clipdist = 1;
                            else
                                pSprite->clipdist = 0;

                            T4(newSprite) = sector[sectNum].floorz;
                            sector[sectNum].hitag = newSprite;
                        }

                        int otherSprite;
                        for (otherSprite = MAXSPRITES - 1; otherSprite >= 0; otherSprite--)
                        {
                            if (sprite[otherSprite].statnum < MAXSTATUS)
                                if (sprite[otherSprite].picnum == SECTOREFFECTOR &&
                                    sprite[otherSprite].lotag == SE_1_PIVOT &&
                                    sprite[otherSprite].hitag == pSprite->hitag)
                                {
                                    if (pSprite->ang == 512)
                                    {
                                        pSprite->x = sprite[otherSprite].x;
                                        pSprite->y = sprite[otherSprite].y;
                                    }
                                    break;
                                }
                        }
                        if (otherSprite == -1)
                        {
                            // Bsprintf(tempbuf,"Found lonely Sector Effector (lotag 0) at (%d,%d)\n",sp->x,sp->y);
                            // G_GameExit(tempbuf);
                            OSD_Printf(OSD_ERROR "Found lonely Sector Effector (lotag 0) at (%d,%d)\n", TrackerCast(pSprite->x), TrackerCast(pSprite->y));
                            changespritestat(newSprite, STAT_ACTOR);
                            goto SPAWN_END;
                        }
                        pSprite->owner = otherSprite;
                    }

                    int const startwall = sector[sectNum].wallptr;
                    int const endwall = startwall + sector[sectNum].wallnum;

                    T2(newSprite) = tempwallptr;
                    for (int s = startwall; s < endwall; s++)
                    {
                        g_origins[tempwallptr].x = wall[s].x - pSprite->x;
                        g_origins[tempwallptr].y = wall[s].y - pSprite->y;
                        tempwallptr++;
                        if (tempwallptr > 2047)
                        {
                            Bsprintf(tempbuf, "Too many moving sectors at (%d,%d).\n", TrackerCast(wall[s].x), TrackerCast(wall[s].y));
                            G_GameExit(tempbuf);
                        }
                    }
                    if (pSprite->lotag == SE_30_TWO_WAY_TRAIN || pSprite->lotag == SE_6_SUBWAY || pSprite->lotag == SE_14_SUBWAY_CAR || pSprite->lotag == SE_5)
                    {
                        int const startwall = sector[sectNum].wallptr;
                        int const endwall = startwall + sector[sectNum].wallnum;

                        if (sector[sectNum].hitag == -1)
                            pSprite->extra = 0;
                        else pSprite->extra = 1;

                        sector[sectNum].hitag = newSprite;

                        bool foundZeroedSect = false;

                        int foundWall = startwall;
                        for (; foundWall < endwall; foundWall++)
                        {
                            if (wall[foundWall].nextsector >= 0 &&
                                sector[wall[foundWall].nextsector].hitag == 0 &&
                                sector[wall[foundWall].nextsector].lotag < 3)
                            {
                                foundWall = wall[foundWall].nextsector;
                                foundZeroedSect = true;
                                break;
                            }
                        }

                        if (!foundZeroedSect)
                        {
                            Bsprintf(tempbuf, "Subway found no zero'd sectors with locators\nat (%d,%d).\n", TrackerCast(pSprite->x), TrackerCast(pSprite->y));
                            G_GameExit(tempbuf);
                        }

                        pSprite->owner = -1;
                        T1(newSprite) = foundWall;

                        if (pSprite->lotag != SE_30_TWO_WAY_TRAIN)
                            T4(newSprite) = pSprite->hitag;
                    }

                    else if (pSprite->lotag == SE_16_REACTOR)
                        T4(newSprite) = sector[sectNum].ceilingz;

                    else if (pSprite->lotag == SE_26)
                    {
                        T4(newSprite) = pSprite->x;
                        T5(newSprite) = pSprite->y;
                        if (pSprite->shade == sector[sectNum].floorshade) //UP
                            pSprite->zvel = -256;
                        else
                            pSprite->zvel = 256;

                        pSprite->shade = 0;
                    }
                    else if (pSprite->lotag == SE_2_EARTHQUAKE)
                    {
                        T6(newSprite) = sector[pSprite->sectnum].floorheinum;
                        sector[pSprite->sectnum].floorheinum = 0;
                    }
                }
            }

            switch (pSprite->lotag)
            {
                case SE_6_SUBWAY:
                case SE_14_SUBWAY_CAR:
                {
                    // What the absolute shit?
                    int j = A_CallSound(sectNum, newSprite);
                    if (j == -1)
                        j = SUBWAY;
                    pActor->lastvx = j;
                }
                fallthrough__;
                case SE_30_TWO_WAY_TRAIN:
                case SE_0_ROTATING_SECTOR:
                case SE_1_PIVOT:
                case SE_5:
                case SE_11_SWINGING_DOOR:
                case SE_15_SLIDING_DOOR:
                case SE_16_REACTOR:
                case SE_26:
                    Sect_SetInterpolation(pSprite->sectnum, true);
                    break;
            }

            changespritestat(newSprite, STAT_EFFECTOR);
            goto SPAWN_END;
        }
        case SEENINE__:
        case OOZFILTER__:
            pSprite->shade = -16;
            if (pSprite->xrepeat <= 8)
            {
                pSprite->cstat = (short)32768;
                pSprite->xrepeat = pSprite->yrepeat = 0;
            }
            else pSprite->cstat = (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN);
            pSprite->extra = g_impactDamage << 2;
            pSprite->owner = newSprite;

            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;

        case CRACK1__:
        case CRACK2__:
        case CRACK3__:
        case CRACK4__:
        case FIREEXT__:
            if (pSprite->picnum == FIREEXT)
            {
                pSprite->cstat = (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN);
                pSprite->extra = g_impactDamage << 2;
            }
            else
            {
                pSprite->cstat |= (pSprite->cstat & 48) ? 1 : 17;
                pSprite->extra = 1;
            }

            if ((ud.multimode < 2 || GTFLAGS(GAMETYPE_COOP)) && pSprite->pal != 0)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }

            pSprite->pal = 0;
            pSprite->owner = newSprite;
            pSprite->xvel = 8;

            changespritestat(newSprite, STAT_STANDABLE);
            A_SetSprite(newSprite, CLIPMASK0);
            goto SPAWN_END;

        case TOILET__:
        case STALL__:
            pSprite->lotag = 1;
            pSprite->cstat |= (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN);
            pSprite->clipdist = 8;
            pSprite->owner = newSprite;
            goto SPAWN_END;

        case CANWITHSOMETHING__:
        case CANWITHSOMETHING2__:
        case CANWITHSOMETHING3__:
        case CANWITHSOMETHING4__:
        case RUBBERCAN__:
            pSprite->extra = 0;
            fallthrough__;
        case EXPLODINGBARREL__:
        case HORSEONSIDE__:
        case FIREBARREL__:
        case NUKEBARREL__:
        case FIREVASE__:
        case NUKEBARRELDENTED__:
        case NUKEBARRELLEAKED__:
        case WOODENHORSE__:
            if (hasParent)
            {
                pSprite->xrepeat = pSprite->yrepeat = 32;
                pSprite->owner = spriteNum;
            }
            else
                pSprite->owner = newSprite;

            pSprite->clipdist = 72;
            A_Fall(newSprite);
            fallthrough__;
        case EGG__:
            if (DMFLAGS_TEST(DMFLAG_NOMONSTERS) && pSprite->picnum == EGG)
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
            }
            else
            {
                if (pSprite->picnum == EGG)
                    pSprite->clipdist = 24;
                pSprite->cstat = (CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN) | (krand() & 4);
                changespritestat(newSprite, STAT_ZOMBIEACTOR);
            }
            goto SPAWN_END;

        case TOILETWATER__:
            pSprite->shade = -16;
            changespritestat(newSprite, STAT_STANDABLE);
            goto SPAWN_END;
    }

    // implementation of the default case
    if (G_TileHasActor(pSprite->picnum))
    {
        if (spriteNum == -1 && pSprite->lotag > ud.player_skill)
        {
            pSprite->xrepeat = pSprite->yrepeat = 0;
            changespritestat(newSprite, STAT_MISC);
            goto SPAWN_END;
        }

        //  Init the size
        if (pSprite->xrepeat == 0 || pSprite->yrepeat == 0)
            pSprite->xrepeat = pSprite->yrepeat = 1;

        if (A_CheckSpriteFlags(newSprite, SFLAG_BADGUY))
        {
            if (DMFLAGS_TEST(DMFLAG_NOMONSTERS))
            {
                pSprite->xrepeat = pSprite->yrepeat = 0;
                changespritestat(newSprite, STAT_MISC);
                goto SPAWN_END;
            }

            A_Fall(newSprite);

            if (A_CheckSpriteFlags(newSprite, SFLAG_BADGUYSTAYPUT))
                pActor->actorstayput = pSprite->sectnum;

            ud.total_monsters++;
            pSprite->clipdist = 80;

            if (spriteNum >= 0)
            {
                if (sprite[spriteNum].picnum == RESPAWN)
                    pActor->tempang = sprite[newSprite].pal = sprite[spriteNum].pal;

                A_PlayAlertSound(newSprite);
                changespritestat(newSprite, STAT_ACTOR);
            }
            else
                changespritestat(newSprite, STAT_ZOMBIEACTOR);
        }
        else
        {
            pSprite->clipdist = 40;
            pSprite->owner = newSprite;
            changespritestat(newSprite, STAT_ACTOR);
        }

        pActor->timetosleep = 0;

        if (spriteNum >= 0)
            pSprite->ang = sprite[spriteNum].ang;
    }

SPAWN_END:

    if (VM_HaveEvent(EVENT_SPAWN))
    {
        int32_t pDist;
        int32_t playerNum = A_FindPlayer(&sprite[newSprite], &pDist);
        X_OnEvent(EVENT_SPAWN, newSprite, playerNum, pDist);
    }

    return newSprite;
}

// OLDMP MERGE CHECKME
static int32_t G_MaybeTakeOnFloorPal(tspriteptr_t datspr, int32_t sect)
{
    int32_t dapal = sector[sect].floorpal;

    if (dapal && !g_noFloorPal[dapal]
        && !A_CheckSpriteFlags(datspr->owner, SFLAG_NOPAL))
    {
        datspr->pal = dapal;
        return 1;
    }

    return 0;
}

static inline void G_DoEventAnimSprites(int tspriteNum)
{
    int const tsprOwner = tsprite[tspriteNum].owner;

    if ((((unsigned)tsprOwner >= MAXSPRITES || (spriteext[tsprOwner].flags & SPREXT_TSPRACCESS) != SPREXT_TSPRACCESS))
        && tsprite[tspriteNum].statnum != TSPR_TEMP)
        return;

    spriteext[tsprOwner].tspr = &tsprite[tspriteNum];
    X_OnEvent(EVENT_ANIMATESPRITES, tsprOwner, screenpeek, -1);
    spriteext[tsprOwner].tspr = NULL;
}

template <int rotations>
static int getofs_viewtype(int angDiff)
{
    return ((((angDiff + 3072) & 2047) * rotations + 1024) >> 11) % rotations;
}

template <int rotations>
static int viewtype_mirror(uint16_t& cstat, int frameOffset)
{
    if (frameOffset > rotations / 2)
    {
        cstat |= 4;
        return rotations - frameOffset;
    }

    cstat &= ~4;
    return frameOffset;
}

template <int mirrored_rotations>
static int getofs_viewtype_mirrored(uint16_t& cstat, int angDiff)
{
    return viewtype_mirror<mirrored_rotations * 2 - 2>(cstat, getofs_viewtype<mirrored_rotations * 2 - 2>(angDiff));
}


#ifdef _MSC_VER
// Visual C thought this was a bit too hard to optimise so we'd better
// tell it not to try... such a pussy it is.
//#pragma auto_inline(off)
#pragma optimize("g",off)
#endif
void G_DoSpriteAnimations(int32_t x, int32_t y, int32_t z, int32_t a, int32_t smoothratio)
{
    UNREFERENCED_PARAMETER(z);

    int i, j, p, sect;
    intptr_t l;
    spritetype* s;
    tspritetype *t;
    int switchpic;
    int frameOffset;

    if (!spritesortcnt) return;

    ror_sprite = -1;

    for (j = spritesortcnt - 1; j >= 0; j--) //Between drawrooms() and renderDrawMasks()
    {
        //is the perfect time to animate sprites
        t = &tsprite[j];
        i = t->owner;
        s = &sprite[i];

        switch (tileGetMapping(s->picnum))
        {
        case SECTOREFFECTOR__:
            if (s->lotag == SE_40_ROR_UPPER || s->lotag == SE_41_ROR_LOWER)
            {
                t->cstat = 32768;

                if (ror_sprite == -1) ror_sprite = i;
            }

            if (t->lotag == SE_27_DEMO_CAM && ud.recstat == 1)
            {
                t->picnum = 11 + (((int32_t)totalclock >> 3) & 1);
                t->cstat |= 128;
            }
            else
                t->xrepeat = t->yrepeat = 0;
            break;
        }
    }

    for (j=spritesortcnt-1;j>=0; j--)
    {
        t = &tsprite[j];
        i = t->owner;
        s = &sprite[t->owner];

        //greenslime can't be handled through the dynamictostatic system due to addition on constant
        if ((t->picnum >= GREENSLIME)&&(t->picnum <= GREENSLIME+7))
        {
            if ((numplayers > 1) && (screenpeek == myconnectindex) && (g_player[myconnectindex].ps->somethingonplayer == i))
            {
                t->statnum = TSPR_TEMP;
            }
        }
        else
        {
            switch (tileGetMapping(t->picnum))
            {
                case BLOODPOOL__:
                case PUKE__:
                case FOOTPRINTS__:
                case FOOTPRINTS2__:
                case FOOTPRINTS3__:
                case FOOTPRINTS4__:
                    if (t->shade == 127) continue;
                    break;
                case RESPAWNMARKERRED__:
                case RESPAWNMARKERYELLOW__:
                case RESPAWNMARKERGREEN__:
                    if (!DMFLAGS_TEST(DMFLAG_MARKERS))
                        t->xrepeat = t->yrepeat = 0;
                    continue;
                case CHAIR3__:
                    if (tilehasmodelorvoxel(t->picnum, t->pal) && !(spriteext[i].flags & SPREXT_NOTMD))
                    {
                        t->cstat &= ~4;
                        break;
                    }
                    frameOffset = getofs_viewtype_mirrored<5>(t->cstat, t->ang - a);
                    t->picnum = s->picnum + frameOffset;
                    break;
                case BLOODSPLAT1__:
                case BLOODSPLAT2__:
                case BLOODSPLAT3__:
                case BLOODSPLAT4__:
                    if (ud.lockout) t->xrepeat = t->yrepeat = 0;
                    else if (t->pal == 6)
                    {
                        t->shade = -127;
                        continue;
                    }
                    fallthrough__;

                case BULLETHOLE__:
                case CRACK1__:
                case CRACK2__:
                case CRACK3__:
                case CRACK4__:
                    t->shade = 16;
                    continue;
                case NEON1__:
                case NEON2__:
                case NEON3__:
                case NEON4__:
                case NEON5__:
                case NEON6__:
                    continue;

                default:
                    if (((t->cstat & 16)) || (A_CheckEnemySprite(t) && t->extra > 0) || t->statnum == 10)
                        continue;
            }
        }

        if (A_CheckSpriteFlags(t->owner,SFLAG_NOSHADE))
            l = sprite[t->owner].shade;
        else
        {
            if (sector[t->sectnum].ceilingstat&1)
                l = sector[t->sectnum].ceilingshade;
            else
                l = sector[t->sectnum].floorshade;
            if (l < -127) l = -127;
            if (l > 128) l =  127;
        }
        t->shade = l;
    }

    for (j=spritesortcnt-1;j>=0; j--) //Between drawrooms() and renderDrawMasks()
    {
        //is the perfect time to animate sprites
        t = &tsprite[j];
        i = t->owner;
        s = &sprite[i];

        switch (tileGetMapping(s->picnum))
        {
            case NATURALLIGHTNING__:
                t->shade = -127;
                break;
            case FEM1__:
            case FEM2__:
            case FEM3__:
            case FEM4__:
            case FEM5__:
            case FEM6__:
            case FEM7__:
            case FEM8__:
            case FEM9__:
            case FEM10__:
            case MAN__:
            case MAN2__:
            case WOMAN__:
            case NAKED1__:
            case PODFEM1__:
            case FEMMAG1__:
            case FEMMAG2__:
            case FEMPIC1__:
            case FEMPIC2__:
            case FEMPIC3__:
            case FEMPIC4__:
            case FEMPIC5__:
            case FEMPIC6__:
            case FEMPIC7__:
            case BLOODYPOLE__:
            case FEM6PAD__:
            case STATUE__:
            case STATUEFLASH__:
            case OOZ__:
            case OOZ2__:
            case WALLBLOOD1__:
            case WALLBLOOD2__:
            case WALLBLOOD3__:
            case WALLBLOOD4__:
            case WALLBLOOD5__:
            case WALLBLOOD7__:
            case WALLBLOOD8__:
            case SUSHIPLATE1__:
            case SUSHIPLATE2__:
            case SUSHIPLATE3__:
            case SUSHIPLATE4__:
            case FETUS__:
            case FETUSJIB__:
            case FETUSBROKE__:
            case HOTMEAT__:
            case FOODOBJECT16__:
            case DOLPHIN1__:
            case DOLPHIN2__:
            case TOUGHGAL__:
            case TAMPON__:
            case XXXSTACY__:
                if (ud.lockout)
                {
                    t->xrepeat = t->yrepeat = 0;
                    //continue;
                }
        }

        switch (s->picnum)
        {
            case 4946:
            case 4947:
            case 693:
            case 2254:
            case 4560:
            case 4561:
            case 4562:
            case 4498:
            case 4957:
                if (ud.lockout)
                {
                    t->xrepeat = t->yrepeat = 0;
                    //continue;
                }
        }

        if (t->statnum == TSPR_TEMP)
            continue;

        if (s->statnum != 1 && s->picnum == APLAYER && g_player[s->yvel].ps->newowner == -1 && s->owner >= 0)
        {
            t->x -= mulscale16(65536-smoothratio,g_player[s->yvel].ps->pos.x - g_player[s->yvel].ps->opos.x);
            t->y -= mulscale16(65536-smoothratio,g_player[s->yvel].ps->pos.y - g_player[s->yvel].ps->opos.y);
            t->z -= mulscale16(65536-smoothratio,g_player[s->yvel].ps->pos.z - g_player[s->yvel].ps->opos.z);
            
            // Dead or frozen?
            if (g_player[s->yvel].ps->dead_flag || s->extra <= 0)
                t->z += (20<<8); // Matches corpse height of vanilla Duke3D.
        }
        else
        {
            t->x -= mulscale16(65536-smoothratio,s->x-actor[i].bpos.x);
            t->y -= mulscale16(65536-smoothratio,s->y-actor[i].bpos.y);
            t->z -= mulscale16(65536-smoothratio,s->z-actor[i].bpos.z);
        }

        sect = s->sectnum;

        int32_t curframe = AC_CURFRAME(actor[i].t_data);
        int32_t scrofs_action = AC_ACTION_ID(actor[i].t_data);

        switchpic = s->picnum;
        //some special cases because dynamictostatic system can't handle addition to constants
        if ((s->picnum >= SCRAP6)&&(s->picnum<=SCRAP6+7))
            switchpic = SCRAP5;
        else if ((s->picnum==MONEY+1)||(s->picnum==MAIL+1)||(s->picnum==PAPER+1))
            switchpic--;

        switch (tileGetMapping(switchpic))
        {
            case DUKELYINGDEAD__:
                t->z += (24 << 8);
                break;
            case BLOODPOOL__:
            case FOOTPRINTS__:
            case FOOTPRINTS2__:
            case FOOTPRINTS3__:
            case FOOTPRINTS4__:
                if (t->pal == 6)
                    t->shade = -127;
                fallthrough__;
            case PUKE__:
            case MONEY__:
                //case MONEY+1__:
            case MAIL__:
                //case MAIL+1__:
            case PAPER__:
                //case PAPER+1__:
                if (ud.lockout && s->pal == 2)
                {
                    t->xrepeat = t->yrepeat = 0;
                    //continue;
                }
                break;
            case TRIPBOMB__:
                continue;
            case FORCESPHERE__:
                if (t->statnum == 5)
                {
                    short sqa, sqb;

                    sqa =
                        getangle(
                            sprite[s->owner].x - g_player[screenpeek].ps->pos.x,
                            sprite[s->owner].y - g_player[screenpeek].ps->pos.y);
                    sqb =
                        getangle(
                            sprite[s->owner].x - t->x,
                            sprite[s->owner].y - t->y);

                    if (klabs(G_GetAngleDelta(sqa, sqb)) > 512)
                        if (ldist(&sprite[s->owner], t) < ldist(&sprite[g_player[screenpeek].ps->i], &sprite[s->owner]))
                            t->xrepeat = t->yrepeat = 0;
                }
                continue;
            case BURNING__:
            case BURNING2__:
                if (sprite[s->owner].statnum == 10)
                {
                    if (display_mirror == 0 && sprite[s->owner].yvel == screenpeek && g_player[sprite[s->owner].yvel].ps->over_shoulder_on == 0)
                        t->xrepeat = 0;
                    else
                    {
                        t->ang = getangle(x - t->x, y - t->y);
                        t->x = sprite[s->owner].x;
                        t->y = sprite[s->owner].y;
                        t->x += sintable[(t->ang + 512) & 2047] >> 10;
                        t->y += sintable[t->ang & 2047] >> 10;
                    }
                }
                break;

            case ATOMICHEALTH__:
                t->z -= (4 << 8);
                break;
            case CRYSTALAMMO__:
                t->shade = (sintable[((int32_t)totalclock << 4) & 2047] >> 10);
                continue;
            case VIEWSCREEN__:
            case VIEWSCREEN2__:
            {
                int const viewscrShift = G_GetViewscreenSizeShift(t);
                int const viewscrTile = TILE_VIEWSCR - viewscrShift;

                if (g_curViewscreen >= 0 && actor[OW(i)].t_data[0] == 1)
                {
                    t->picnum = STATIC;
                    t->cstat |= (rand() & 12);
                    t->xrepeat += 10;
                    t->yrepeat += 9;
                }
                else if (g_curViewscreen == i && display_mirror != 3 && waloff[viewscrTile])
                {
                    // this exposes a sprite sorting issue which needs to be debugged further...
#if 0
                    if (spritesortcnt < maxspritesonscreen)
                    {
                        auto const newt = &tsprite[spritesortcnt++];

                        *newt = *t;

                        newt->cstat |= 2 | 512;
                        newt->x += (sintable[(newt->ang + 512) & 2047] >> 12);
                        newt->y += (sintable[newt->ang & 2047] >> 12);
                        updatesector(newt->x, newt->y, &newt->sectnum);
                    }
#endif
                    t->picnum = viewscrTile;
#if VIEWSCREENFACTOR > 0
                    t->xrepeat >>= viewscrShift;
                    t->yrepeat >>= viewscrShift;
#endif
                }

                break;
            }

            case SHRINKSPARK__:
                t->picnum = SHRINKSPARK + (((int32_t)totalclock >> 4) & 3);
                break;
            case GROWSPARK__:
                t->picnum = GROWSPARK + (((int32_t)totalclock >> 4) & 3);
                break;

            case RPG__:
                if (tilehasmodelorvoxel(t->picnum, t->pal) && !(spriteext[i].flags & SPREXT_NOTMD))
                {
                    int32_t v = getangle(t->xvel, t->zvel >> 4);

                    spriteext[i].mdpitch = (v > 1023 ? v - 2048 : v);
                    t->cstat &= ~4;
                    break;
                }
                frameOffset = getofs_viewtype_mirrored<7>(t->cstat, s->ang - getangle(s->x - x, s->y - y));
                t->picnum = RPG + frameOffset;
                break;

            case RECON__:
                if (tilehasmodelorvoxel(t->picnum, t->pal) && !(spriteext[i].flags & SPREXT_NOTMD))
                {
                    t->cstat &= ~4;
                    break;
                }
                frameOffset = getofs_viewtype_mirrored<7>(t->cstat, s->ang - getangle(s->x - x, s->y - y));

                // RECON_T4
                if (klabs(curframe) > 64)
                    frameOffset += 7;  // tilted recon car

                t->picnum = RECON + frameOffset;
                break;

            case APLAYER__:
                p = s->yvel;

                if (t->pal == 1) // Frozen?
                    t->z -= (18 << 8);

                if (g_player[p].ps->over_shoulder_on > 0 && g_player[p].ps->newowner < 0 && s->owner != -1)
                {
                    if (p == screenpeek) // Let's only make the player we're peeking transparent.
                        t->cstat |= 2;

                    if (screenpeek == myconnectindex && numplayers >= 2)
                    {
                        t->x = predictedPlayer.opos.x + mulscale16((int)(predictedPlayer.pos.x - predictedPlayer.opos.x), smoothratio);
                        t->y = predictedPlayer.opos.y + mulscale16((int)(predictedPlayer.pos.y - predictedPlayer.opos.y), smoothratio);
                        t->z = predictedPlayer.opos.z + mulscale16((int)(predictedPlayer.pos.z - predictedPlayer.opos.z), smoothratio) + (40 << 8);
                        t->ang = fix16_to_int(predictedPlayer.oq16ang + mulscale16(((predictedPlayer.q16ang + 1024 - predictedPlayer.oq16ang) & 2047) - 1024, smoothratio));
                        t->sectnum = predictedPlayer.cursectnum;
                    }
                }

                if (ud.multimode > 1 && (display_mirror || screenpeek != p || s->owner == -1))
                {
                    if (!DMFLAGS_TEST(DMFLAG_NOWEAPONICONS) && ud.showweapons && sprite[g_player[p].ps->i].extra > 0 && g_player[p].ps->curr_weapon > 0 && (spritesortcnt < maxspritesonscreen))
                    {
                        memcpy((spritetype*)&tsprite[spritesortcnt], (spritetype*)t, sizeof(spritetype));

                        tsprite[spritesortcnt].statnum = TSPR_TEMP;
                        tsprite[spritesortcnt].shade = t->shade;
                        tsprite[spritesortcnt].cstat = 0;
                        tsprite[spritesortcnt].pal = 0;
                        tsprite[spritesortcnt].picnum = (g_player[p].ps->curr_weapon == GROW_WEAPON ? GROWSPRITEICON : WeaponPickupSprites[g_player[p].ps->curr_weapon]);
                        tsprite[spritesortcnt].z -= (51 << 8);

                        if (tsprite[spritesortcnt].picnum == HEAVYHBOMB)
                        {
                            tsprite[spritesortcnt].xrepeat = 10;
                            tsprite[spritesortcnt].yrepeat = 10;
                        }
                        else
                        {
                            tsprite[spritesortcnt].xrepeat = 16;
                            tsprite[spritesortcnt].yrepeat = 16;
                        }
                        spritesortcnt++;
                    }

                    if (g_player[p].inputBits->extbits & (EXTBIT_CHAT) && !ud.pause_on && (spritesortcnt < maxspritesonscreen))
                    {
                        memcpy((spritetype*)&tsprite[spritesortcnt], (spritetype*)t, sizeof(spritetype));

                        tsprite[spritesortcnt].statnum = TSPR_TEMP;

                        tsprite[spritesortcnt].yrepeat = (t->yrepeat >> 3);
                        if (t->yrepeat < 4) t->yrepeat = 4;

                        tsprite[spritesortcnt].cstat = 0;

                        tsprite[spritesortcnt].picnum = RESPAWNMARKERGREEN;

                        // Code for spinning voxel logos
                        tsprite[spritesortcnt].ang -= ((int32_t)totalclock << 6);

                        tsprite[spritesortcnt].z -= (60 << 8);
                        tsprite[spritesortcnt].xrepeat = 32;
                        tsprite[spritesortcnt].yrepeat = 32;
                        tsprite[spritesortcnt].pal = 20;
                        spritesortcnt++;
                    }
                }

                if (s->owner == -1)
                {
                    if (tilehasmodelorvoxel(s->picnum, t->pal) && !(spriteext[i].flags & SPREXT_NOTMD))
                    {
                        frameOffset = 0;
                        t->cstat &= ~4;
                    }
                    else
                        frameOffset = getofs_viewtype_mirrored<5>(t->cstat, s->ang - a);

                    if (sector[s->sectnum].lotag == ST_2_UNDERWATER) frameOffset += 1795 - 1405;
                    else if ((actor[i].floorz - s->z) > (64 << 8)) frameOffset += 60;

                    t->picnum += frameOffset;
                    t->pal = g_player[p].ps->palookup;

                    goto PALONLY;
                }

                // CLAMP PLAYER SPRITE Y TO FLOOR
                if (g_player[p].ps->on_crane == -1)
                {
                    int16_t lotag = (sector[s->sectnum].lotag & 0x7ff);
                    int32_t floorZ = actor[g_player[p].ps->i].floorz;
                    bool noSE7water = ((lotag == ST_1_ABOVE_WATER) && A_CheckNoSE7Water((uspriteptr_t)s, s->sectnum, lotag, NULL));

                    if (noSE7water || lotag != ST_1_ABOVE_WATER)
                    {
                        int offset = s->z - floorZ + (3 << 8);
                        if (offset > 1024 && t->yrepeat > 32 && s->extra > 0)
                            t->yoffset = (int8_t)tabledivide32_noinline(offset, t->yrepeat << 2);
                        else t->yoffset = 0;
                    }
                }

                if (g_player[p].ps->newowner > -1)
                {
                    // Display APLAYER sprites with action PSTAND when viewed through
                    // a camera.  Not implemented for Lunatic.
                    const intptr_t* aplayer_scr = g_tile[APLAYER].execPtr;
                    // [0]=strength, [1]=actionofs, [2]=moveofs

                    scrofs_action = aplayer_scr[1];
                    curframe = 0;
                }

                if (ud.camerasprite == -1 && g_player[p].ps->newowner == -1)
                    if (s->owner >= 0 && display_mirror == 0 && g_player[p].ps->over_shoulder_on == 0)
                        if (ud.multimode < 2 || (ud.multimode > 1 && p == screenpeek))
                        {
                            t->owner = -1;
                            t->xrepeat = t->yrepeat = 0;
                            continue;
                        }

PALONLY:

                G_MaybeTakeOnFloorPal(t, sect);

                if (s->owner == -1) continue;

                if (t->z > actor[i].floorz && t->xrepeat < 32)
                    t->z = actor[i].floorz;

                break;

            case JIBS1__:
            case JIBS2__:
            case JIBS3__:
            case JIBS4__:
            case JIBS5__:
            case JIBS6__:
            case HEADJIB1__:
            case LEGJIB1__:
            case ARMJIB1__:
            case LIZMANHEAD1__:
            case LIZMANARM1__:
            case LIZMANLEG1__:
            case DUKELEG__:
            case DUKEGUN__:
            case DUKETORSO__:
                if (ud.lockout)
                {
                    t->xrepeat = t->yrepeat = 0;
                    //continue;
                }
                if (t->pal == 6) t->shade = -120;
                fallthrough__;
            case SCRAP1__:
            case SCRAP2__:
            case SCRAP3__:
            case SCRAP4__:
            case SCRAP5__:
                if (actor[i].htpicnum == BLIMP && t->picnum == SCRAP1 && s->yvel >= 0)
                    t->picnum = s->yvel;
                else t->picnum += T1(i);
                t->shade -= 6;

                G_MaybeTakeOnFloorPal(t, sect);
                break;

            case WATERBUBBLE__:
                if (sector[t->sectnum].floorpicnum == FLOORSLIME)
                {
                    t->pal = 7;
                    break;
                }
                fallthrough__;
            default:
                G_MaybeTakeOnFloorPal(t, sect);
                break;
        }

        if (G_TileHasActor(s->picnum))
        {
            if ((unsigned)scrofs_action + ACTION_PARAM_COUNT > (unsigned)g_scriptSize || apScript[scrofs_action + ACTION_PARAM_COUNT] != CON_END)
            {
                if (scrofs_action)
                    LOG_F(ERROR, "Sprite %d tile %d: invalid action at offset %d", i, TrackerCast(s->picnum), scrofs_action);

                goto skip;
            }

            int32_t viewtype = apScript[scrofs_action + ACTION_VIEWTYPE];
            uint16_t const action_flags = apScript[scrofs_action + ACTION_FLAGS];

            int const invertp = viewtype < 0;
            l = klabs(viewtype);

            if (tilehasmodelorvoxel(s->picnum, t->pal) && !(spriteext[i].flags & SPREXT_NOTMD))
            {
                frameOffset = 0;
                t->cstat &= ~4;
            }
            else
            {
                int const viewAng = ((l > 4 && l != 8) || action_flags & AF_VIEWPOINT) ? getangle(s->x - x, s->y - y) : a;
                int const angDiff = invertp ? viewAng - s->ang : s->ang - viewAng;

                switch (l)
                {
                    case 2:
                        frameOffset = getofs_viewtype<8>(angDiff) & 1;
                        break;

                    case 3:
                    case 4:
                        frameOffset = viewtype_mirror<7>(t->cstat, getofs_viewtype<16>(angDiff) & 7);
                        break;

                    case 5:
                        frameOffset = getofs_viewtype_mirrored<5>(t->cstat, angDiff);
                        break;
                    case 7:
                        frameOffset = getofs_viewtype_mirrored<7>(t->cstat, angDiff);
                        break;
                    case 8:
                        frameOffset = getofs_viewtype<8>(angDiff);
                        t->cstat &= ~4;
                        break;
                    case 9:
                        frameOffset = getofs_viewtype_mirrored<9>(t->cstat, angDiff);
                        break;
                    case 12:
                        frameOffset = getofs_viewtype<12>(angDiff);
                        t->cstat &= ~4;
                        break;
                    case 16:
                        frameOffset = getofs_viewtype<16>(angDiff);
                        t->cstat &= ~4;
                        break;
                    default:
                        frameOffset = 0;
                        break;
                }
            }

            t->picnum += frameOffset + apScript[scrofs_action + ACTION_STARTFRAME] + viewtype * curframe;
            // XXX: t->picnum can be out-of-bounds by bad user code.

            Bassert((unsigned)t->picnum < MAXTILES);

            if (viewtype > 0)
                while (tilesiz[t->picnum].x == 0 && t->picnum > 0)
                    t->picnum -= l;       //Hack, for actors

            if (actor[i].dispicnum >= 0)
                actor[i].dispicnum = t->picnum;
        }

skip:
        if (g_player[screenpeek].ps->inv_amount[GET_HEATS] > 0 && g_player[screenpeek].ps->heat_on && (A_CheckEnemySprite(s) || A_CheckSpriteFlags(t->owner,SFLAG_NVG) || s->picnum == APLAYER || s->statnum == 13))
        {
            t->pal = 6;
            t->shade = 0;
        }

        if (s->statnum == 13 || A_CheckEnemySprite(s) || A_CheckSpriteFlags(t->owner,SFLAG_SHADOW) || (s->picnum == APLAYER && s->owner >= 0))
            if (t->statnum != TSPR_TEMP && s->picnum != EXPLOSION2 && s->picnum != HANGLIGHT && s->picnum != DOMELITE)
                if (s->picnum != HOTMEAT)
                {
                    if (actor[i].dispicnum < 0)
                    {
                        actor[i].dispicnum++;
                        continue;
                    }
                    else if (ud.shadows && spritesortcnt < (maxspritesonscreen-2))
                    {
                        int daz,xrep,yrep;

                        if ((sector[sect].lotag&0xff) > 2 || s->statnum == STAT_PROJECTILE || s->statnum == STAT_MISC || s->picnum == DRONE || s->picnum == COMMANDER)
                            daz = sector[sect].floorz;
                        else
                            daz = actor[i].floorz;

                        if ((s->z-daz) < (8<<8))
                            if (g_player[screenpeek].ps->pos.z < daz)
                            {
                                memcpy((spritetype *)&tsprite[spritesortcnt],(spritetype *)t,sizeof(spritetype));

                                tsprite[spritesortcnt].statnum = TSPR_TEMP;

                                tsprite[spritesortcnt].yrepeat = (t->yrepeat>>3);
                                if (t->yrepeat < 4) t->yrepeat = 4;

                                tsprite[spritesortcnt].shade = 127;
                                tsprite[spritesortcnt].cstat |= 2;

                                tsprite[spritesortcnt].z = daz;
                                xrep = tsprite[spritesortcnt].xrepeat;// - (klabs(daz-t->z)>>11);
                                tsprite[spritesortcnt].xrepeat = xrep;
                                tsprite[spritesortcnt].pal = 4;

                                yrep = tsprite[spritesortcnt].yrepeat;// - (klabs(daz-t->z)>>11);
                                tsprite[spritesortcnt].yrepeat = yrep;

#if defined(USE_OPENGL)
                                if (videoGetRenderMode() >= 3 && usemodels && md_tilehasmodel(t->picnum,t->pal) >= 0)
                                {
                                    tsprite[spritesortcnt].yrepeat = 0;
                                    // 512:trans reverse
                                    //1024:tell MD2SPRITE.C to use Z-buffer hacks to hide overdraw issues
                                    tsprite[spritesortcnt].cstat |= (512+1024);
                                }
                                else if (videoGetRenderMode() >= 3)
                                {
                                    int ii;

                                    ii = getangle(tsprite[spritesortcnt].x-g_player[screenpeek].ps->pos.x,
                                                  tsprite[spritesortcnt].y-g_player[screenpeek].ps->pos.y);

                                    tsprite[spritesortcnt].x += sintable[(ii+2560)&2047]>>9;
                                    tsprite[spritesortcnt].y += sintable[(ii+2048)&2047]>>9;
                                }
#endif
                                spritesortcnt++;
                            }
                    }
                }

        bool const haveAction = scrofs_action != 0 && (unsigned)scrofs_action + ACTION_PARAM_COUNT <= (unsigned)g_scriptSize;

        switch (tileGetMapping(s->picnum))
        {
            case LASERLINE__:
                if (sector[t->sectnum].lotag == ST_2_UNDERWATER) t->pal = 8;
                t->z = sprite[s->owner].z - (3 << 8);
                if (g_tripbombLaserMode == 2 && g_player[screenpeek].ps->heat_on == 0)
                    t->yrepeat = 0;
                fallthrough__;
            case EXPLOSION2__:
            case EXPLOSION2BOT__:
            case FREEZEBLAST__:
            case ATOMICHEALTH__:
            case FIRELASER__:
            case SHRINKSPARK__:
            case GROWSPARK__:
            case CHAINGUN__:
            case SHRINKEREXPLOSION__:
            case RPG__:
            case FLOORFLAME__:
                if (t->picnum == EXPLOSION2)
                {
                    g_player[screenpeek].display.visibility = -127;
                    //g_restorePalette = 1;   // JBF 20040101: why?
                }
                t->shade = -127;
                break;
            case FIRE__:
            case FIRE2__:
                t->cstat |= 128;
                fallthrough__;
            case BURNING__:
            case BURNING2__:
                if (sprite[s->owner].picnum != TREE1 && sprite[s->owner].picnum != TREE2)
                    t->z = sector[t->sectnum].floorz;
                t->shade = -127;
                break;
            case COOLEXPLOSION1__:
                t->shade = -127;
                t->picnum += (s->shade >> 1);
                break;
            case PLAYERONWATER__:
                t->shade = sprite[s->owner].shade;

                if (haveAction)
                    break;

                if (tilehasmodelorvoxel(s->picnum, s->pal) && !(spriteext[i].flags & SPREXT_NOTMD))
                {
                    frameOffset = 0;
                    t->cstat &= ~4;
                }
                else
                    frameOffset = getofs_viewtype_mirrored<5>(t->cstat, t->ang - a);

                t->picnum = s->picnum + frameOffset + ((T1(i) < 4) * 5);
                break;

            case WATERSPLASH2__:
                t->picnum = WATERSPLASH2 + T2(i);
                break;
            case SHELL__:
                t->picnum = s->picnum + (T1(i) & 1);
                fallthrough__;
            case SHOTGUNSHELL__:
                t->cstat |= 12;
                if (T1(i) > 1) t->cstat &= ~4;
                if (T1(i) > 2) t->cstat &= ~12;
                break;
            case FRAMEEFFECT1_13__:
                if (PLUTOPAK) break;
                fallthrough__;
            case FRAMEEFFECT1__:
                if (s->owner >= 0 && sprite[s->owner].statnum < MAXSTATUS)
                {
                    if (sprite[s->owner].picnum == APLAYER)
                        if (ud.camerasprite == -1)
                            if (screenpeek == sprite[s->owner].yvel && display_mirror == 0)
                            {
                                t->owner = -1;
                                break;
                            }
                    if ((sprite[s->owner].cstat & 32768) == 0)
                    {
                        if (!actor[s->owner].dispicnum)
                            t->picnum = actor[i].t_data[1];
                        else t->picnum = actor[s->owner].dispicnum;

                        if (!G_MaybeTakeOnFloorPal(t, sect))
                            t->pal = sprite[s->owner].pal;

                        t->shade = sprite[s->owner].shade;
                        t->ang = sprite[s->owner].ang;
                        t->cstat = 2 | sprite[s->owner].cstat;
                    }
                }
                break;

            case CAMERA1__:
            case RAT__:
                if (haveAction)
                    break;

                if (tilehasmodelorvoxel(s->picnum, s->pal) && !(spriteext[i].flags & SPREXT_NOTMD))
                {
                    t->cstat &= ~4;
                    break;
                }
                frameOffset = getofs_viewtype_mirrored<5>(t->cstat, t->ang - a);
                t->picnum = s->picnum + frameOffset;
                break;
        }

        actor[i].dispicnum = t->picnum;
        if (sector[t->sectnum].floorpicnum == MIRROR)
            t->xrepeat = t->yrepeat = 0;
    }

    if (VM_HaveEvent(EVENT_ANIMATESPRITES))
    {
        for (j = spritesortcnt - 1; j >= 0; j--)
            G_DoEventAnimSprites(j);
    }
}
#ifdef _MSC_VER
//#pragma auto_inline()
#pragma optimize("",on)
#endif

char CheatStrings[][MAXCHEATLEN] =
{
    "cornholio",    // 0
    "stuff",        // 1
    "scotty###",    // 2
    "coords",       // 3
    "view",         // 4
    "time",         // 5
    "unlock",       // 6
    "cashman",      // 7
    "items",        // 8
    "rate",         // 9
    "skill#",       // 10
    "beta",         // 11
    "hyper",        // 12
    "monsters",     // 13
    "<RESERVED>",   // 14
    "<RESERVED>",   // 15
    "todd",         // 16
    "showmap",      // 17
    "kroz",         // 18
    "allen",        // 19
    "clip",         // 20
    "weapons",      // 21
    "inventory",    // 22
    "keys",         // 23
    "debug",        // 24
    "<RESERVED>",   // 25
    "sfm",          // 26
    "booti"         // 27
};

void G_CheatGetInv(void)
{
    Gv_SetVar(sysVarIDs.RETURN, 400, g_player[myconnectindex].ps->i, myconnectindex);
    X_OnEvent(EVENT_CHEATGETSTEROIDS, g_player[myconnectindex].ps->i, myconnectindex, -1);
    if (Gv_GetVar(sysVarIDs.RETURN, g_player[myconnectindex].ps->i, myconnectindex) >=0)
    {
        g_player[myconnectindex].ps->inv_amount[GET_STEROIDS] = Gv_GetVar(sysVarIDs.RETURN, g_player[myconnectindex].ps->i, myconnectindex);
    }

    Gv_SetVar(sysVarIDs.RETURN, 1200, g_player[myconnectindex].ps->i, myconnectindex);
    X_OnEvent(EVENT_CHEATGETHEAT, g_player[myconnectindex].ps->i, myconnectindex, -1);
    if (Gv_GetVar(sysVarIDs.RETURN, g_player[myconnectindex].ps->i, myconnectindex) >=0)
    {
        g_player[myconnectindex].ps->inv_amount[GET_HEATS] = Gv_GetVar(sysVarIDs.RETURN, g_player[myconnectindex].ps->i, myconnectindex);
    }

    Gv_SetVar(sysVarIDs.RETURN, 200, g_player[myconnectindex].ps->i, myconnectindex);
    X_OnEvent(EVENT_CHEATGETBOOT, g_player[myconnectindex].ps->i, myconnectindex, -1);
    if (Gv_GetVar(sysVarIDs.RETURN, g_player[myconnectindex].ps->i, myconnectindex) >=0)
    {
        g_player[myconnectindex].ps->inv_amount[GET_BOOTS] = Gv_GetVar(sysVarIDs.RETURN, g_player[myconnectindex].ps->i, myconnectindex);
    }

    Gv_SetVar(sysVarIDs.RETURN, 100, g_player[myconnectindex].ps->i, myconnectindex);
    X_OnEvent(EVENT_CHEATGETSHIELD, g_player[myconnectindex].ps->i, myconnectindex, -1);
    if (Gv_GetVar(sysVarIDs.RETURN, g_player[myconnectindex].ps->i, myconnectindex) >=0)
    {
        g_player[myconnectindex].ps->inv_amount[GET_SHIELD] = Gv_GetVar(sysVarIDs.RETURN, g_player[myconnectindex].ps->i, myconnectindex);
    }

    Gv_SetVar(sysVarIDs.RETURN, 6400, g_player[myconnectindex].ps->i, myconnectindex);
    X_OnEvent(EVENT_CHEATGETSCUBA, g_player[myconnectindex].ps->i, myconnectindex, -1);
    if (Gv_GetVar(sysVarIDs.RETURN, g_player[myconnectindex].ps->i, myconnectindex) >=0)
    {
        g_player[myconnectindex].ps->inv_amount[GET_SCUBA] = Gv_GetVar(sysVarIDs.RETURN, g_player[myconnectindex].ps->i, myconnectindex);
    }

    Gv_SetVar(sysVarIDs.RETURN, 2400, g_player[myconnectindex].ps->i, myconnectindex);
    X_OnEvent(EVENT_CHEATGETHOLODUKE, g_player[myconnectindex].ps->i, myconnectindex, -1);
    if (Gv_GetVar(sysVarIDs.RETURN, g_player[myconnectindex].ps->i, myconnectindex) >=0)
    {
        g_player[myconnectindex].ps->inv_amount[GET_HOLODUKE] = Gv_GetVar(sysVarIDs.RETURN, g_player[myconnectindex].ps->i, myconnectindex);
    }

    Gv_SetVar(sysVarIDs.RETURN, 1600, g_player[myconnectindex].ps->i, myconnectindex);
    X_OnEvent(EVENT_CHEATGETJETPACK, g_player[myconnectindex].ps->i, myconnectindex, -1);
    if (Gv_GetVar(sysVarIDs.RETURN, g_player[myconnectindex].ps->i, myconnectindex) >=0)
    {
        g_player[myconnectindex].ps->inv_amount[GET_JETPACK] = Gv_GetVar(sysVarIDs.RETURN, g_player[myconnectindex].ps->i, myconnectindex);
    }

    Gv_SetVar(sysVarIDs.RETURN, g_player[myconnectindex].ps->max_player_health, g_player[myconnectindex].ps->i, myconnectindex);
    X_OnEvent(EVENT_CHEATGETFIRSTAID, g_player[myconnectindex].ps->i, myconnectindex, -1);
    if (Gv_GetVar(sysVarIDs.RETURN, g_player[myconnectindex].ps->i, myconnectindex) >=0)
    {
        g_player[myconnectindex].ps->inv_amount[GET_FIRSTAID] = Gv_GetVar(sysVarIDs.RETURN, g_player[myconnectindex].ps->i, myconnectindex);
    }
}

signed char cheatbuf[MAXCHEATLEN],cheatbuflen;

static void G_DoCheats(void)
{
    short ch, i, j, k=0, weapon;
    static int z=0;
    char consolecheat = 0;  // JBF 20030914

    if (osdcmd_cheatsinfo_stat.cheatnum != -1)
    {
        // JBF 20030914
        k = osdcmd_cheatsinfo_stat.cheatnum;
        osdcmd_cheatsinfo_stat.cheatnum = -1;
        consolecheat = 1;
    }

    if ((ud.gm&MODE_TYPE) || (ud.gm&MODE_MENU))
        return;

    if (VOLUMEONE && !z)
    {
        Bstrcpy(CheatStrings[2],"scotty##");
        Bstrcpy(CheatStrings[6],"<RESERVED>");
        z=1;
    }

    if (consolecheat && numplayers < 2 && ud.recstat == 0)
        goto FOUNDCHEAT;

    if (g_player[myconnectindex].ps->cheat_phase == 1)
    {
        while (KB_KeyWaiting())
        {
            ch = Btolower(KB_GetCh());

            if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')))
            {
                g_player[myconnectindex].ps->cheat_phase = 0;
                //                             P_DoQuote(46,g_player[myconnectindex].ps);
                return;
            }

            cheatbuf[cheatbuflen++] = ch;
            cheatbuf[cheatbuflen] = 0;
            //            KB_ClearKeysDown();

            if (cheatbuflen > MAXCHEATLEN)
            {
                g_player[myconnectindex].ps->cheat_phase = 0;
                return;
            }

            for (k=0;k < NUMCHEATCODES;k++)
            {
                for (j = 0;j<cheatbuflen;j++)
                {
                    if (cheatbuf[j] == CheatStrings[k][j] || (CheatStrings[k][j] == '#' && ch >= '0' && ch <= '9'))
                    {
                        if (CheatStrings[k][j+1] == 0) goto FOUNDCHEAT;
                        if (j == cheatbuflen-1) return;
                    }
                    else break;
                }
            }

            g_player[myconnectindex].ps->cheat_phase = 0;
            return;

FOUNDCHEAT:
            {
                switch (k)
                {
                case CHEAT_WEAPONS:

                    j = 0;

                    if (VOLUMEONE)
                        j = 6;

                    for (weapon = PISTOL_WEAPON;weapon < MAX_WEAPONS-j;weapon++)
                    {
                        P_AddAmmo(weapon, g_player[myconnectindex].ps, g_player[myconnectindex].ps->max_ammo_amount[weapon]);
                        g_player[myconnectindex].ps->gotweapon[weapon]  = 1;
                    }

                    KB_FlushKeyboardQueue();
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    P_DoQuote(QUOTE_CHEAT_ALL_WEAPONS,g_player[myconnectindex].ps);
                    return;

                case CHEAT_INVENTORY:
                    KB_FlushKeyboardQueue();
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    G_CheatGetInv();
                    P_DoQuote(QUOTE_CHEAT_ALL_INV,g_player[myconnectindex].ps);
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    return;

                case CHEAT_KEYS:
                    ud.got_access =  7;
                    KB_FlushKeyboardQueue();
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    P_DoQuote(QUOTE_CHEAT_ALL_KEYS,g_player[myconnectindex].ps);
                    return;

                case CHEAT_DEBUG:
                    debug_on = 1-debug_on;
                    KB_FlushKeyboardQueue();
                    g_player[myconnectindex].ps->cheat_phase = 0;

                    G_DumpDebugInfo();
                    Bsprintf(tempbuf,"GAMEVARS DUMPED TO DEBUG.CON");
                    G_AddUserQuote(tempbuf);
                    Bsprintf(tempbuf,"MAP DUMPED TO DEBUG.MAP");
                    G_AddUserQuote(tempbuf);
                    break;

                case CHEAT_CLIP:
                    ud.noclip = 1-ud.noclip;
                    KB_FlushKeyboardQueue();
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    P_DoQuote(QUOTE_CHEAT_NOCLIP-!ud.noclip,g_player[myconnectindex].ps);
                    return;

                case CHEAT_RESERVED2:
                    ud.gm = MODE_EOL;
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    KB_FlushKeyboardQueue();
                    return;

                case CHEAT_ALLEN:
                    P_DoQuote(QUOTE_CHEAT_ALLEN,g_player[myconnectindex].ps);
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    KB_ClearKeyDown(sc_N);
                    return;

                case CHEAT_CORNHOLIO:
                case CHEAT_KROZ:
                {
                    ud.god = 1 - ud.god;

                    if (ud.god)
                    {
                        pus = 1;
                        pub = 1;

                        for (int ALL_PLAYERS(i))
                        {
                            auto ps = g_player[i].ps;
                            sprite[ps->i].cstat = 257;

                            actor[ps->i].t_data[0] = 0;
                            actor[ps->i].t_data[1] = 0;
                            actor[ps->i].t_data[2] = 0;
                            actor[ps->i].t_data[3] = 0;
                            actor[ps->i].t_data[4] = 0;
                            actor[ps->i].t_data[5] = 0;

                            sprite[ps->i].hitag = 0;
                            sprite[ps->i].lotag = 0;
                            sprite[ps->i].xrepeat = PLAYER_SPRITE_XREPEAT;
                            sprite[ps->i].yrepeat = PLAYER_SPRITE_YREPEAT;
                            sprite[ps->i].pal = ps->palookup;
                            sprite[ps->i].extra = ps->last_extra = ps->max_player_health;

                            actor[ps->i].htextra = -1;
                        }

                        P_DoQuote(QUOTE_CHEAT_GODMODE_ON, g_player[myconnectindex].ps);
                    }
                    else
                    {
                        ud.god = 0;

                        for (int ALL_PLAYERS(i))
                        {
                            auto ps = g_player[i].ps;

                            sprite[ps->i].extra = ps->last_extra = ps->max_player_health;

                            actor[ps->i].htextra = -1;
                        }

                        P_DoQuote(QUOTE_CHEAT_GODMODE_OFF, g_player[myconnectindex].ps);
                    }

                    g_player[myconnectindex].ps->cheat_phase = 0;
                    KB_FlushKeyboardQueue();
                    return;
                }

                case CHEAT_SCREAMFORME:
                    ud.god = 1-ud.god;

                    if (ud.god)
                    {
                        pus = 1;
                        pub = 1;
                        sprite[g_player[myconnectindex].ps->i].cstat = 257;

                        actor[g_player[myconnectindex].ps->i].t_data[0] = 0;
                        actor[g_player[myconnectindex].ps->i].t_data[1] = 0;
                        actor[g_player[myconnectindex].ps->i].t_data[2] = 0;
                        actor[g_player[myconnectindex].ps->i].t_data[3] = 0;
                        actor[g_player[myconnectindex].ps->i].t_data[4] = 0;
                        actor[g_player[myconnectindex].ps->i].t_data[5] = 0;

                        sprite[g_player[myconnectindex].ps->i].hitag = 0;
                        sprite[g_player[myconnectindex].ps->i].lotag = 0;
                        sprite[g_player[myconnectindex].ps->i].pal = g_player[myconnectindex].ps->palookup;
                        Bstrcpy(apStrings[QUOTE_RESERVED4],"Scream for me, Long Beach!");
                        P_DoQuote(QUOTE_RESERVED4,g_player[myconnectindex].ps);
                        G_CheatGetInv();
                        for (weapon = PISTOL_WEAPON;weapon < MAX_WEAPONS;weapon++)
                            g_player[myconnectindex].ps->gotweapon[weapon]  = 1;

                        for (weapon = PISTOL_WEAPON;
                                weapon < (MAX_WEAPONS);
                                weapon++)
                            P_AddAmmo(weapon, g_player[myconnectindex].ps, g_player[myconnectindex].ps->max_ammo_amount[weapon]);
                        ud.got_access = 7;
                    }
                    else
                    {
                        sprite[g_player[myconnectindex].ps->i].extra = g_player[myconnectindex].ps->max_player_health;
                        actor[g_player[myconnectindex].ps->i].htextra = -1;
                        g_player[myconnectindex].ps->last_extra = g_player[myconnectindex].ps->max_player_health;
                        P_DoQuote(QUOTE_CHEAT_GODMODE_OFF,g_player[myconnectindex].ps);
                    }

                    sprite[g_player[myconnectindex].ps->i].extra = g_player[myconnectindex].ps->max_player_health;
                    actor[g_player[myconnectindex].ps->i].htextra = 0;
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    KB_FlushKeyboardQueue();
                    return;

                case CHEAT_BOOTI:
                {
                    bool enabled = DMFLAGS_TEST(DMFLAG_ALLOWDOUBLEKICK);
                    
                    if(!enabled)
                        Bstrcpy(apStrings[QUOTE_RESERVED4], "Booti pls!");
                    else
                        Bstrcpy(apStrings[QUOTE_RESERVED4], "Binyot y do u not boot?");

                    P_DoQuote(QUOTE_RESERVED4, g_player[myconnectindex].ps);

                    DMFLAGS_SET(DMFLAG_ALLOWDOUBLEKICK, !enabled);
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    KB_FlushKeyboardQueue();
                    return;
                }

                case CHEAT_STUFF:

                    j = 0;

                    if (VOLUMEONE)
                        j = 6;

                    for (weapon = PISTOL_WEAPON;weapon < MAX_WEAPONS-j;weapon++)
                        g_player[myconnectindex].ps->gotweapon[weapon]  = 1;

                    for (weapon = PISTOL_WEAPON;
                            weapon < (MAX_WEAPONS-j);
                            weapon++)
                        P_AddAmmo(weapon, g_player[myconnectindex].ps, g_player[myconnectindex].ps->max_ammo_amount[weapon]);
                    G_CheatGetInv();
                    ud.got_access =              7;
                    P_DoQuote(QUOTE_CHEAT_EVERYTHING,g_player[myconnectindex].ps);
                    g_player[myconnectindex].ps->cheat_phase = 0;

                    //                        P_DoQuote(21,g_player[myconnectindex].ps);
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    KB_FlushKeyboardQueue();
                    g_player[myconnectindex].ps->inven_icon = 1;
                    return;

                case CHEAT_SCOTTY:
                case CHEAT_SKILL:
                    if (k == CHEAT_SCOTTY)
                    {
                        i = Bstrlen(CheatStrings[k])-3+VOLUMEONE;
                        if (!consolecheat)
                        {
                            // JBF 20030914
                            short volnume,levnume;
                            if (VOLUMEALL)
                            {
                                volnume = cheatbuf[i] - '0';
                                levnume = (cheatbuf[i+1] - '0')*10+(cheatbuf[i+2]-'0');
                            }
                            else
                            {
                                volnume =  cheatbuf[i] - '0';
                                levnume =  cheatbuf[i+1] - '0';
                            }

                            volnume--;
                            levnume--;

                            if ((VOLUMEONE && volnume > 0) || volnume > g_numVolumes-1 ||
                                    levnume >= MAXLEVELS || g_mapInfo[volnume*MAXLEVELS+levnume].filename == NULL)
                            {
                                g_player[myconnectindex].ps->cheat_phase = 0;
                                KB_FlushKeyboardQueue();
                                return;
                            }

                            ud.m_volume_number = ud.volume_number = volnume;
                            ud.m_level_number = ud.level_number = levnume;
                        }
                        else
                        {
                            // JBF 20030914
                            ud.m_volume_number = ud.volume_number = osdcmd_cheatsinfo_stat.volume;
                            ud.m_level_number = ud.level_number = osdcmd_cheatsinfo_stat.level;
                        }
                    }
                    else
                    {
                        i = Bstrlen(CheatStrings[k])-1;
                        ud.m_player_skill = ud.player_skill = cheatbuf[i] - '1';
                    }
                    if (numplayers > 1 && myconnectindex == connecthead)
                        G_NewGame(NEWGAME_RESETALL);
                    else ud.gm |= MODE_RESTART;

                    g_player[myconnectindex].ps->cheat_phase = 0;
                    KB_FlushKeyboardQueue();
                    return;

                case CHEAT_COORDS:
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    ud.coords = 1-ud.coords;
                    KB_FlushKeyboardQueue();
                    return;

                case CHEAT_VIEW:
                    if (g_player[myconnectindex].ps->over_shoulder_on)
                        g_player[myconnectindex].ps->over_shoulder_on = 0;
                    else
                    {
                        g_player[myconnectindex].ps->over_shoulder_on = 1;
                        g_cameraDistance = 0;
                        g_cameraClock = (int32_t)totalclock;
                    }
                    P_DoQuote(22,g_player[myconnectindex].ps);
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    KB_FlushKeyboardQueue();
                    return;

                case CHEAT_TIME:

                    P_DoQuote(21,g_player[myconnectindex].ps);
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    KB_FlushKeyboardQueue();
                    return;

                case CHEAT_UNLOCK:
                    if (VOLUMEONE) return;

                    for (i=numsectors-1;i>=0;i--) //Unlock
                    {
                        j = sector[i].lotag;
                        if (j == -1 || j == 32767) continue;
                        if ((j & 0x7fff) > 2)
                        {
                            if (j&(0xffff-16384))
                                sector[i].lotag &= (0xffff-16384);
                            G_OperateSectors(i,g_player[myconnectindex].ps->i);
                        }
                    }
                    G_OperateForceFields(g_player[myconnectindex].ps->i,-1);

                    P_DoQuote(QUOTE_CHEAT_UNLOCK,g_player[myconnectindex].ps);
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    KB_FlushKeyboardQueue();
                    return;

                case CHEAT_CASHMAN:
                    ud.cashman = 1-ud.cashman;
                    KB_ClearKeyDown(sc_N);
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    return;

                case CHEAT_ITEMS:
                    G_CheatGetInv();
                    ud.got_access =              7;
                    P_DoQuote(QUOTE_CHEAT_EVERYTHING,g_player[myconnectindex].ps);
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    KB_FlushKeyboardQueue();
                    return;

                case CHEAT_SHOWMAP: // SHOW ALL OF THE MAP TOGGLE;
                    ud.showallmap = 1-ud.showallmap;
                    if (ud.showallmap)
                    {
                        for (i=0;i<(MAXSECTORS>>3);i++)
                            show2dsector[i] = 255;
                        for (i=0;i<(MAXWALLS>>3);i++)
                            show2dwall[i] = 255;
                        P_DoQuote(QUOTE_SHOW_MAP_ON,g_player[myconnectindex].ps);
                    }
                    else
                    {
                        for (i=0;i<(MAXSECTORS>>3);i++)
                            show2dsector[i] = 0;
                        for (i=0;i<(MAXWALLS>>3);i++)
                            show2dwall[i] = 0;
                        P_DoQuote(QUOTE_SHOW_MAP_OFF,g_player[myconnectindex].ps);
                    }
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    KB_FlushKeyboardQueue();
                    return;

                case CHEAT_TODD:
                    P_DoQuote(QUOTE_CHEAT_TODD,g_player[myconnectindex].ps);
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    KB_FlushKeyboardQueue();
                    return;

                case CHEAT_RATE:
                    ud.showfps = !ud.showfps;
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    KB_FlushKeyboardQueue();
                    return;

                case CHEAT_BETA:
                    P_DoQuote(QUOTE_CHEAT_BETA,g_player[myconnectindex].ps);
                    KB_ClearKeyDown(sc_H);
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    KB_FlushKeyboardQueue();
                    return;

                case CHEAT_HYPER:
                    g_player[myconnectindex].ps->inv_amount[GET_STEROIDS] = 399;
                    g_player[myconnectindex].ps->inv_amount[GET_HEATS] = 1200;
                    g_player[myconnectindex].ps->cheat_phase = 0;
                    P_DoQuote(QUOTE_CHEAT_STEROIDS,g_player[myconnectindex].ps);
                    KB_FlushKeyboardQueue();
                    return;

                case CHEAT_MONSTERS:
                {
                   const char *s[] = { "ON", "OFF", "ON" };

                    g_noEnemies++;
                    if (g_noEnemies == 3) g_noEnemies = 0;
                    g_player[screenpeek].ps->cheat_phase = 0;
                    Bsprintf(apStrings[QUOTE_RESERVED4],"MONSTERS: %s",s[(unsigned char)g_noEnemies]);
                    P_DoQuote(QUOTE_RESERVED4,g_player[myconnectindex].ps);
                    KB_FlushKeyboardQueue();
                    return;
                }
                case CHEAT_RESERVED:
                case CHEAT_RESERVED3:
                    ud.eog = 1;
                    ud.gm |= MODE_EOL;
                    KB_FlushKeyboardQueue();
                    return;
                }
            }
        }
    }
    else
    {
        if (KB_KeyPressed((unsigned char)CheatKeys[0]))
        {
            if (g_player[myconnectindex].ps->cheat_phase >= 0 && numplayers < 2 && ud.recstat == 0)
            {
                if (CheatKeys[0] == CheatKeys[1])
                    KB_ClearKeyDown((unsigned char)CheatKeys[0]);
                g_player[myconnectindex].ps->cheat_phase = -1;
            }
        }

        if (KB_KeyPressed((unsigned char)CheatKeys[1]))
        {
            if (g_player[myconnectindex].ps->cheat_phase == -1)
            {
                if (ud.player_skill == 4)
                {
                    P_DoQuote(QUOTE_CHEATS_DISABLED,g_player[myconnectindex].ps);
                    g_player[myconnectindex].ps->cheat_phase = 0;
                }
                else
                {
                    g_player[myconnectindex].ps->cheat_phase = 1;
                    //                    P_DoQuote(25,g_player[myconnectindex].ps);
                    cheatbuflen = 0;
                }
                KB_FlushKeyboardQueue();
            }
            else if (g_player[myconnectindex].ps->cheat_phase != 0)
            {
                g_player[myconnectindex].ps->cheat_phase = 0;
                KB_ClearKeyDown((unsigned char)CheatKeys[0]);
                KB_ClearKeyDown((unsigned char)CheatKeys[1]);
            }
        }
    }
}

static void G_PrintCurrentMusic(void)
{
    Bsnprintf(apStrings[QUOTE_MUSIC], MAXQUOTELEN, "Playing %s", g_mapInfo[g_musicIndex].musicfn);
    P_DoQuote(QUOTE_MUSIC, g_player[myconnectindex].ps);
}

static void G_HandleVoteKeys(void)
{
    int i;
    auto player = &g_player[myconnectindex];
    if (!player->gotvote && vote.starter != -1 && vote.starter != myconnectindex)
    {
        if (KB_UnBoundKeyPressed(sc_F1) || KB_UnBoundKeyPressed(sc_F2) || ud.autovote)
        {
            bool vote_yes = (KB_UnBoundKeyPressed(sc_F1) || (ud.autovote ? ud.autovote - 1 : 0));

            if(vote_yes)
                vote.yes_votes++;

            tempbuf[0] = PACKET_TYPE_MAP_VOTE;
            tempbuf[1] = myconnectindex;
            tempbuf[2] = vote_yes;
            
            player->gotvote = 1;
            G_AddUserQuote("VOTE CAST");

            TRAVERSE_CONNECT(i)
            {
                if (i != myconnectindex) mmulti_sendpacket(i, (unsigned char*)tempbuf, 3);
                if (myconnectindex != connecthead) break; //slaves in M/S mode only send to master
            }
    
            KB_ClearKeyDown(sc_F1);
            KB_ClearKeyDown(sc_F2);
        }
    }

    if (vote.starter != -1)
    {
        if (!player->gotvote && vote.starter != myconnectindex)
            ud.gm &= ~MODE_MENU;

        if (myconnectindex == connecthead)
        {
            int32_t numVotesSubmitted = 0;
            for (ALL_PLAYERS(i))
            {
                numVotesSubmitted += g_player[i].gotvote;
            }

            if (numVotesSubmitted == numplayers || (vote.yes_votes > (numplayers >> 1)))
            {
                if (vote.yes_votes > (numplayers >> 1))
                {
                    // Copy vote members to ud.m_* members.
                    ud.m_volume_number      = vote.episode;
                    ud.m_level_number       = vote.level;
                    ud.m_player_skill       = vote.skill;
                    ud.m_coop               = vote.gametype;
                    ud.m_dmflags            = vote.dmflags;

                    G_NewGame(NEWGAME_RESETALL);

                    if (vote.starter == myconnectindex || myconnectindex == connecthead)
                        G_AddUserQuote("VOTE SUCCEEDED");

                    vote = votedata_t();

                    return;
                }
                else if (numVotesSubmitted == numplayers)
                {
                    Net_CancelVote();

                    Bsprintf(apStrings[QUOTE_RESERVED2], "VOTE FAILED");
                    P_DoQuote(QUOTE_RESERVED2, g_player[myconnectindex].ps);
                    ud.gm &= ~MODE_MENU;
                }
            }
        }
    }
}

static void G_HandleLocalKeys(void)
{
    int i,ch;
    int j;

    CONTROL_ProcessBinds();

    if (ud.recstat == 2)
    {
        ControlInfo noshareinfo;
        CONTROL_GetInput(&noshareinfo);
    }

    G_HandleVoteKeys();

    if (!ALT_IS_PRESSED && ud.overhead_on == 0 && (ud.gm & MODE_TYPE) == 0)
    {
        if (BUTTON(gamefunc_Enlarge_Screen))
        {
            CONTROL_ClearButton(gamefunc_Enlarge_Screen);
            if (!SHIFTS_IS_PRESSED)
            {
                if (ud.screen_size > 0)
                    S_PlaySound(THUD);
                if (videoGetRenderMode() >= 3 && ud.screen_size == 8 && ud.statusbarmode == 0)
                    ud.statusbarmode = 1;
                else ud.screen_size -= 4;

                if (ud.statusbarscale == 100 && ud.statusbarmode == 1)
                {
                    ud.statusbarmode = 0;
                    ud.screen_size -= 4;
                }
            }
            else
            {
                ud.statusbarscale += 4;
                G_SetStatusBarScale(ud.statusbarscale);
            }
            G_UpdateScreenArea();
        }

        if (BUTTON(gamefunc_Shrink_Screen))
        {
            CONTROL_ClearButton(gamefunc_Shrink_Screen);
            if (!SHIFTS_IS_PRESSED)
            {
                if (ud.screen_size < 64) S_PlaySound(THUD);
                if (videoGetRenderMode() >= 3 && ud.screen_size == 8 && ud.statusbarmode == 1)
                    ud.statusbarmode = 0;
                else ud.screen_size += 4;
            }
            else
            {
                ud.statusbarscale -= 4;
                if (ud.statusbarscale < 37)
                    ud.statusbarscale = 37;
                G_SetStatusBarScale(ud.statusbarscale);
            }
            G_UpdateScreenArea();
        }
    }

    if (g_player[myconnectindex].ps->cheat_phase == 1 || (ud.gm&(MODE_MENU|MODE_TYPE))) return;

    if (BUTTON(gamefunc_See_Coop_View) && (GTFLAGS(GAMETYPE_COOPVIEW) || ud.recstat == 2))
    {
        CONTROL_ClearButton(gamefunc_See_Coop_View);
        screenpeek = G_GetNextPlayer(screenpeek);
        if (screenpeek == -1) screenpeek = connecthead;
        g_restorePalette = 1;
    }

    if (ud.multimode > 1 && BUTTON(gamefunc_Show_Opponents_Weapon))
    {
        CONTROL_ClearButton(gamefunc_Show_Opponents_Weapon);
        ud.showweapons = 1-ud.showweapons;
        P_DoQuote(QUOTE_WEAPON_MODE_OFF-ud.showweapons,g_player[screenpeek].ps);
    }

    if (BUTTON(gamefunc_Toggle_Crosshair))
    {
        CONTROL_ClearButton(gamefunc_Toggle_Crosshair);
        ud.crosshair = !ud.crosshair;
        P_DoQuote(QUOTE_CROSSHAIR_OFF-ud.crosshair,g_player[screenpeek].ps);
    }

    if (ud.overhead_on && BUTTON(gamefunc_Map_Follow_Mode))
    {
        CONTROL_ClearButton(gamefunc_Map_Follow_Mode);
        ud.scrollmode = 1-ud.scrollmode;
        if (ud.scrollmode)
        {
            ud.folx = g_player[screenpeek].ps->opos.x;
            ud.foly = g_player[screenpeek].ps->opos.y;
            ud.fola = fix16_to_int(g_player[screenpeek].ps->oq16ang);
        }
        P_DoQuote(QUOTE_MAP_FOLLOW_OFF+ud.scrollmode,g_player[myconnectindex].ps);
    }

    if (SHIFTS_IS_PRESSED || ALT_IS_PRESSED)
    {
        int ridiculeNum = 0;

        // NOTE: sc_F1 .. sc_F10 are contiguous. sc_F11 is not sc_F10+1.
        for (bssize_t j = sc_F1; j <= sc_F10; j++)
            if (KB_UnBoundKeyPressed(j))
            {
                KB_ClearKeyDown(j);
                ridiculeNum = j - sc_F1 + 1;
                break;
            }

        if (ridiculeNum)
        {
            if (SHIFTS_IS_PRESSED)
            {
                if (ridiculeNum == 5 && g_player[myconnectindex].ps->fta > 0 && g_player[myconnectindex].ps->ftq == QUOTE_MUSIC)
                {
                    const unsigned int maxi = VOLUMEALL ? MUS_FIRST_SPECIAL : 6;

                    unsigned int const oldMusicIndex = g_musicIndex;
                    unsigned int MyMusicIndex = g_musicIndex;
                    do
                    {
                        ++MyMusicIndex;
                        if (MyMusicIndex >= maxi)
                            MyMusicIndex = 0;
                    } while (S_TryPlayLevelMusic(MyMusicIndex) && MyMusicIndex != oldMusicIndex);

                    G_PrintCurrentMusic();

                    return;
                }

                G_AddUserQuote(ud.ridecule[ridiculeNum - 1]);

                ch = 0;

                tempbuf[ch] = PACKET_TYPE_MESSAGE;
                tempbuf[ch+1] = 255;
                tempbuf[ch+2] = 0;
                Bstrcat(tempbuf+2,ud.ridecule[ridiculeNum -1]);

                int packetLength = 2+strlen(ud.ridecule[ridiculeNum-1]);

                if (ud.multimode > 1)
                    TRAVERSE_CONNECT(ch)
                {
                    if (ch != myconnectindex) mmulti_sendpacket(ch,(unsigned char*)tempbuf, packetLength);
                    if (myconnectindex != connecthead) break; //slaves in M/S mode only send to master
                }

                pus = NUMPAGES;
                pub = NUMPAGES;

                return;

            }

            if (ud.lockout == 0)
                if (ud.config.SoundToggle && ALT_IS_PRESSED && (RTS_NumSounds() > 0) && g_RTSPlaying == 0 && (ud.config.VoiceToggle & 1))
                {
                    rtsptr = (char *)RTS_GetSound(ridiculeNum-1);
                    FX_Play3D(rtsptr,RTS_SoundLength(ridiculeNum-1), FX_ONESHOT, 0,0,0,255, fix16_one, -ridiculeNum);

                    g_RTSPlaying = 7;

                    if (ud.multimode > 1)
                    {
                        tempbuf[0] = PACKET_TYPE_RTS;
                        tempbuf[1] = ridiculeNum;

                        TRAVERSE_CONNECT(ch)
                        {
                            if (ch != myconnectindex) mmulti_sendpacket(ch,(unsigned char*)tempbuf,2);
                            if (myconnectindex != connecthead) break; //slaves in M/S mode only send to master
                        }
                    }

                    pus = NUMPAGES;
                    pub = NUMPAGES;

                    return;
                }
        }
    }

    if (!ALT_IS_PRESSED && !SHIFTS_IS_PRESSED)
    {

        if (ud.multimode > 1 && BUTTON(gamefunc_SendMessage))
        {
            KB_FlushKeyboardQueue();
            CONTROL_ClearButton(gamefunc_SendMessage);
            ud.gm |= MODE_TYPE;
            typebuf[0] = 0;
            inputloc = 0;
        }

        if (KB_UnBoundKeyPressed(sc_F1) || (ud.show_help && (KB_KeyPressed(sc_Space) || KB_KeyPressed(sc_Enter) || KB_KeyPressed(sc_kpad_Enter) || (MOUSE_GetButtons()&M_LEFTBUTTON))))
        {
            KB_ClearKeyDown(sc_F1);
            KB_ClearKeyDown(sc_Space);
            KB_ClearKeyDown(sc_kpad_Enter);
            KB_ClearKeyDown(sc_Enter);
            MOUSE_ClearButton(M_LEFTBUTTON);
            ud.show_help ++;

            if (ud.show_help > 2)
            {
                ud.show_help = 0;
                if (ud.multimode < 2 && ud.recstat != 2) ready2send = 1;
                G_UpdateScreenArea();
            }
            else
            {
                videoSetViewableArea(0,0,xdim-1,ydim-1);
                if (ud.multimode < 2 && ud.recstat != 2)
                {
                    ready2send = 0;
                    totalclock = ototalclock;
                }
            }
        }

        //        if(ud.multimode < 2)
        {
            if (ud.recstat != 2 && KB_UnBoundKeyPressed(sc_F2))
            {
                KB_ClearKeyDown(sc_F2);

FAKE_F2:
                if (sprite[g_player[myconnectindex].ps->i].extra <= 0)
                {
                    P_DoQuote(QUOTE_SAVE_DEAD,g_player[myconnectindex].ps);
                    return;
                }
                ChangeToMenu(350);
                g_screenCapture = 1;
                G_DrawRooms(myconnectindex,65536);
                //savetemp("duke3d.tmp",waloff[TILE_SAVESHOT],160*100);
                g_screenCapture = 0;
                FX_StopAllSounds();

                //                videoSetViewableArea(0,0,xdim-1,ydim-1);
                ud.gm |= MODE_MENU;

                if (ud.multimode < 2)
                {
                    ready2send = 0;
                    totalclock = ototalclock;
                    screenpeek = myconnectindex;
                }
            }

            if (KB_UnBoundKeyPressed(sc_F3))
            {
                KB_ClearKeyDown(sc_F3);

FAKE_F3:
                ChangeToMenu(300);
                FX_StopAllSounds();

                //                videoSetViewableArea(0,0,xdim-1,ydim-1);
                ud.gm |= MODE_MENU;
                if (ud.multimode < 2 && ud.recstat != 2)
                {
                    ready2send = 0;
                    totalclock = ototalclock;
                }
                screenpeek = myconnectindex;
            }
        }

        if (KB_UnBoundKeyPressed(sc_F4) && ud.config.FXDevice >= 0)
        {
            KB_ClearKeyDown(sc_F4);
            FX_StopAllSounds();

            ud.gm |= MODE_MENU;
            if (ud.multimode < 2 && ud.recstat != 2)
            {
                ready2send = 0;
                totalclock = ototalclock;
            }
            ChangeToMenu(701);

        }

        if ((BUTTON(gamefunc_Quick_Save) || g_doQuickSave == 1) && (ud.gm&MODE_GAME))
        {
            CONTROL_ClearButton(gamefunc_Quick_Save);
            g_doQuickSave = 0;

            if (g_lastSaveSlot == -1) goto FAKE_F2;

            KB_FlushKeyboardQueue();

            if (sprite[g_player[myconnectindex].ps->i].extra <= 0)
            {
                P_DoQuote(QUOTE_SAVE_DEAD,g_player[myconnectindex].ps);
                return;
            }
            g_screenCapture = 1;
            G_DrawRooms(myconnectindex,65536);
            //savetemp("duke3d.tmp",waloff[TILE_SAVESHOT],160*100);
            g_screenCapture = 0;
            if (g_lastSaveSlot >= 0)
            {
                /*                inputloc = Bstrlen(&ud.savegame[g_lastSaveSlot][0]);
                                g_currentMenu = 360+g_lastSaveSlot;
                                probey = g_lastSaveSlot; */
                if (ud.multimode > 1)
                    G_SavePlayer(-1-(g_lastSaveSlot));
                else G_SavePlayer(g_lastSaveSlot);
            }
        }

        if (BUTTON(gamefunc_Third_Person_View))
        {
            CONTROL_ClearButton(gamefunc_Third_Person_View);

            if (g_player[myconnectindex].ps->over_shoulder_on)
                g_player[myconnectindex].ps->over_shoulder_on = 0;
            else
            {
                g_player[myconnectindex].ps->over_shoulder_on = 1;
                g_cameraDistance = 0;
                g_cameraClock = (int32_t)totalclock;
            }
            P_DoQuote(QUOTE_VIEW_MODE_OFF + g_player[myconnectindex].ps->over_shoulder_on, g_player[myconnectindex].ps);
        }

        if (KB_UnBoundKeyPressed(sc_F5) && ud.config.MusicDevice >= 0)
        {
            KB_ClearKeyDown(sc_F5);
            if (g_mapInfo[g_musicIndex].alt_musicfn != NULL)
                Bstrcpy(apStrings[QUOTE_MUSIC],&g_mapInfo[g_musicIndex].alt_musicfn[0]);
            else if (g_mapInfo[g_musicIndex].musicfn != NULL)
            {
                Bstrcpy(apStrings[QUOTE_MUSIC],&g_mapInfo[g_musicIndex].musicfn[0]);
                Bstrcat(apStrings[QUOTE_MUSIC],".  USE SHIFT-F5 TO CHANGE.");
            }
            else apStrings[QUOTE_MUSIC][0] = '\0';
            P_DoQuote(QUOTE_MUSIC,g_player[myconnectindex].ps);
        }

        if (KB_UnBoundKeyPressed(sc_F8))
        {
            KB_ClearKeyDown(sc_F8);
            ud.fta_on = !ud.fta_on;
            if (ud.fta_on) P_DoQuote(QUOTE_MESSAGES_ON,g_player[myconnectindex].ps);
            else
            {
                ud.fta_on = 1;
                P_DoQuote(QUOTE_MESSAGES_OFF,g_player[myconnectindex].ps);
                ud.fta_on = 0;
            }
        }

        if ((BUTTON(gamefunc_Quick_Load) || g_doQuickSave == 2) && (ud.gm&MODE_GAME))
        {
            CONTROL_ClearButton(gamefunc_Quick_Load);
            g_doQuickSave = 0;

            if (g_lastSaveSlot == -1) goto FAKE_F3;

            if (g_lastSaveSlot >= 0)
            {
                KB_FlushKeyboardQueue();
                KB_ClearKeysDown();
                FX_StopAllSounds();

                if (ud.multimode > 1)
                {
                    G_LoadPlayer(-1-g_lastSaveSlot);
                    ud.gm = MODE_GAME;
                }
                else
                {
                    i = G_LoadPlayer(g_lastSaveSlot);
                    if (i == 0)
                        ud.gm = MODE_GAME;
                }
            }
        }

        if (KB_UnBoundKeyPressed(sc_F10))
        {
            KB_ClearKeyDown(sc_F10);
            ChangeToMenu(500);
            FX_StopAllSounds();
            ud.gm |= MODE_MENU;
            if (ud.multimode < 2 && ud.recstat != 2)
            {
                ready2send = 0;
                totalclock = ototalclock;
            }
        }

        if (ud.overhead_on != 0)
        {

            j = (int32_t)totalclock-nonsharedtimer;
            nonsharedtimer += j;
            if (BUTTON(gamefunc_Enlarge_Screen))
                g_player[myconnectindex].ps->zoom += mulscale6(j,max(g_player[myconnectindex].ps->zoom,256));
            if (BUTTON(gamefunc_Shrink_Screen))
                g_player[myconnectindex].ps->zoom -= mulscale6(j,max(g_player[myconnectindex].ps->zoom,256));

            if ((g_player[myconnectindex].ps->zoom > 2048))
                g_player[myconnectindex].ps->zoom = 2048;
            if ((g_player[myconnectindex].ps->zoom < 48))
                g_player[myconnectindex].ps->zoom = 48;

        }
    }

    if (KB_KeyPressed(sc_Escape) && ud.overhead_on && g_player[myconnectindex].ps->newowner == -1)
    {
        KB_ClearKeyDown(sc_Escape);
        ud.last_overhead = ud.overhead_on;
        ud.overhead_on = 0;
        ud.scrollmode = 0;
        G_UpdateScreenArea();
    }

    if (BUTTON(gamefunc_AutoRun))
    {
        CONTROL_ClearButton(gamefunc_AutoRun);
        ud.auto_run = 1-ud.auto_run;
        P_DoQuote(85+ud.auto_run,g_player[myconnectindex].ps);
    }

    if (BUTTON(gamefunc_Map))
    {
        CONTROL_ClearButton(gamefunc_Map);
        if (ud.last_overhead != ud.overhead_on && ud.last_overhead)
        {
            ud.overhead_on = ud.last_overhead;
            ud.last_overhead = 0;
        }
        else
        {
            ud.overhead_on++;
            if (ud.overhead_on == 3) ud.overhead_on = 0;
            ud.last_overhead = ud.overhead_on;
        }
        g_restorePalette = 1;
        G_UpdateScreenArea();
    }

    if (KB_UnBoundKeyPressed(sc_F11))
    {
        KB_ClearKeyDown(sc_F11);
        ChangeToMenu(232);
        FX_StopAllSounds();

        ud.gm |= MODE_MENU;
        if (ud.multimode < 2 && ud.recstat != 2)
        {
            ready2send = 0;
            totalclock = ototalclock;
        }
    }
}

static int parsedefinitions_game(scriptfile*, int);

static void parsedefinitions_game_include(const char* fileName, scriptfile* pScript, const char* cmdtokptr, int const firstPass)
{
    scriptfile* included = scriptfile_fromfile(fileName);

    if (!included)
    {
        if (!Bstrcasecmp(cmdtokptr, "null") || pScript == NULL) // this is a bit overboard to prevent unused parameter warnings
        {
            // initprintf("Warning: Failed including %s as module\n", fn);
        }
        /*
                else
                    {
                    initprintf("Warning: Failed including %s on line %s:%d\n",
                               fn, script->filename,scriptfile_getlinum(script,cmdtokptr));
                    }
        */
    }
    else
    {
        parsedefinitions_game(included, firstPass);
        scriptfile_close(included);
    }
}

static int parsedefinitions_game(scriptfile *pScript, int firstPass)
{
    int tokn;
    char* pToken;

    static const tokenlist tokens[] =
    {
        { "include",           T_INCLUDE          },
        { "#include",          T_INCLUDE          },
        { "define",            T_DEFINE,          },
        { "#define",           T_DEFINE,          },
        { "loadgrp",           T_LOADGRP          },
        { "cachesize",         T_CACHESIZE        },
        { "noautload",         T_NOAUTOLOAD       },
        { "music",             T_MUSIC            },
        { "sound",             T_SOUND            },
        { "addplayercolor",    T_ADDPLAYERCOL     },
        { "delplayercolor",    T_DELPLAYERCOL     },
    };

    static const tokenlist soundTokens[] =
    {
        { "id",       T_ID },
        { "file",     T_FILE },
        { "minpitch", T_MINPITCH },
        { "maxpitch", T_MAXPITCH },
        { "priority", T_PRIORITY },
        { "type",     T_TYPE },
        { "distance", T_DISTANCE },
        { "volume",   T_VOLUME },
    };

    while (1)
    {
        tokn = getatoken(pScript, tokens, ARRAY_SIZE(tokens));
        pToken = pScript->ltextptr;

        switch (tokn)
        {
        case T_LOADGRP:
        {
            char *fn;

            pathsearchmode = 1;
            if (!scriptfile_getstring(pScript,&fn) && firstPass)
            {
                int j = initgroupfile(fn);

                if (j == -1)
                    LOG_F(ERROR, "Could not find group file '%s'.",fn);
                else
                {
                    LOG_F(INFO, "Using group file '%s'.",fn);
                    if (!g_noAutoLoad)
                        G_DoAutoload(fn);
                }

            }
            pathsearchmode = 0;
        }
        break;
        case T_CACHESIZE:
        {
            int j;
            if (scriptfile_getnumber(pScript,&j) || !firstPass) break;

            if (j > 0) MAXCACHE1DSIZE = j<<10;
        }
        break;
        case T_INCLUDE:
        {
            char* fileName;

            if (!scriptfile_getstring(pScript, &fileName))
                parsedefinitions_game_include(fileName, pScript, pToken, firstPass);

            break;
        }
        case T_DEFINE:
        {
            char* name;
            int32_t number;

            if (scriptfile_getstring(pScript, &name)) break;
            if (scriptfile_getsymbol(pScript, &number)) break;

            if (EDUKE32_PREDICT_FALSE(scriptfile_addsymbolvalue(name, number) < 0))
                LOG_F(WARNING, "Symbol %s unable to be redefined to %d on line %s:%d",
                    name, number, pScript->filename, scriptfile_getlinum(pScript, pToken));
            break;
        }
        case T_NOAUTOLOAD:
            if (firstPass)
                g_noAutoLoad = 1;
            break;
        case T_MUSIC:
        {
            char *ID=NULL,*fn,*tfn = NULL;
            char *musicend;

            if (scriptfile_getbraces(pScript,&musicend)) break;
            while (pScript->textptr < musicend)
            {
                switch (getatoken(pScript,soundTokens,sizeof(soundTokens)/sizeof(tokenlist)))
                {
                case T_ID:
                    scriptfile_getstring(pScript,&ID);
                    break;
                case T_FILE:
                    scriptfile_getstring(pScript,&fn);
                    break;
                }
            }
            if (!firstPass)
            {
                int i;
                if (ID==NULL)
                {
                    LOG_F(ERROR, "Missing ID for music definition near line %s:%d", pScript->filename, scriptfile_getlinum(pScript, pToken));
                    break;
                }

                i = pathsearchmode;
                pathsearchmode = 1;
                if (findfrompath(fn,&tfn) < 0)
                {
                    char buf[BMAX_PATH];

                    Bstrcpy(buf,fn);
                    kzfindfilestart(buf);
                    if (!kzfindfile(buf))
                    {
                        LOG_F(ERROR, "File '%s' does not exist",fn);
                        pathsearchmode = i;
                        break;
                    }
                }
                else Xfree(tfn);
                pathsearchmode = i;

                if (S_DefineMusic(ID,fn))
                    LOG_F(ERROR, "Invalid music ID on line %s:%d", pScript->filename, scriptfile_getlinum(pScript, pToken));
            }
        }
        break;

        case T_SOUND:
        {
            char* fileName = NULL;
            char* soundEnd;

            double volume = 1.0;

            int32_t soundNum = -1;
            int32_t maxpitch = 0;
            int32_t minpitch = 0;
            int32_t priority = 0;
            int32_t type = 0;
            int32_t distance = 0;

            if (scriptfile_getbraces(pScript, &soundEnd))
                break;

            while (pScript->textptr < soundEnd)
            {
                switch (getatoken(pScript, soundTokens, ARRAY_SIZE(soundTokens)))
                {
                case T_ID:       scriptfile_getsymbol(pScript, &soundNum); break;
                case T_FILE:     scriptfile_getstring(pScript, &fileName); break;
                case T_MINPITCH: scriptfile_getsymbol(pScript, &minpitch); break;
                case T_MAXPITCH: scriptfile_getsymbol(pScript, &maxpitch); break;
                case T_PRIORITY: scriptfile_getsymbol(pScript, &priority); break;
                case T_TYPE:     scriptfile_getsymbol(pScript, &type);     break;
                case T_DISTANCE: scriptfile_getsymbol(pScript, &distance); break;
                case T_VOLUME:   scriptfile_getdouble(pScript, &volume);   break;
                }
            }

            if (!firstPass)
            {
                if (soundNum == -1)
                {
                    LOG_F(ERROR, "Missing ID for sound definition near line %s:%d", pScript->filename, scriptfile_getlinum(pScript, pToken));
                    break;
                }

                if (fileName == NULL || check_file_exist(fileName))
                    break;

                // maybe I should have just packed this into a sound_t and passed a reference...
                if (S_DefineSound(soundNum, fileName, minpitch, maxpitch, priority, type, distance, volume) == -1)
                    LOG_F(ERROR, "Invalid sound ID on line %s:%d", pScript->filename, scriptfile_getlinum(pScript, pToken));
            }
        }
        break;

        case T_ADDPLAYERCOL:
        {
            int palnum;
            char* palname;

            if (scriptfile_getsymbol(pScript, &palnum)) break;
            if (scriptfile_getstring(pScript, &palname)) break;

            playerColor_add(palname, palnum);
        }
        break;

        case T_DELPLAYERCOL:
        {
            int palnum;

            if (scriptfile_getsymbol(pScript, &palnum)) break;
            
            playerColor_delete(palnum);
        }
        break;

        case T_EOF:
            return(0);
        default:
            break;
        }
    }
    return 0;
}



const char* G_DefaultRtsFile(void)
{
#ifndef EDUKE32_STANDALONE
    if (DUKE)
        return defaultrtsfilename[GAME_DUKE];
    else if (WW2GI)
        return defaultrtsfilename[GAME_WW2GI];
    else if (NAPALM)
    {
        if (!testkopen(defaultrtsfilename[GAME_NAPALM], 0) && testkopen(defaultrtsfilename[GAME_NAM], 0))
            return defaultrtsfilename[GAME_NAM]; // NAM/NAPALM Sharing
        else
            return defaultrtsfilename[GAME_NAPALM];
    }
    else if (NAM)
    {
        if (!testkopen(defaultrtsfilename[GAME_NAM], 0) && testkopen(defaultrtsfilename[GAME_NAPALM], 0))
            return defaultrtsfilename[GAME_NAPALM]; // NAM/NAPALM Sharing
        else
            return defaultrtsfilename[GAME_NAM];
    }
#endif

    return "";
}

int loaddefinitions_game(const char* fileName, int32_t firstPass)
{
    scriptfile* pScript = scriptfile_fromfile(fileName);

    if (pScript)
        parsedefinitions_game(pScript, firstPass);

    for (char const* m : g_defModules)
        parsedefinitions_game_include(m, NULL, "null", firstPass);

    if (pScript)
        scriptfile_close(pScript);

    scriptfile_clearsymbols();

    return 0;
}

void G_UpdateAppTitle(char const* const name /*= nullptr*/)
{
    Bsprintf(tempbuf, APPNAME " " VERSION " (%s)", s_buildRev);

    if (name != nullptr)
    {
        char user_name[32];
        OSD_StripColors(user_name, g_player[myconnectindex].user_name);

        if (g_gameNamePtr)
            Bsnprintf(apptitle, sizeof(apptitle), "%s: %s - %s - %s", user_name, name, g_gameNamePtr, tempbuf);
        else
            Bsnprintf(apptitle, sizeof(apptitle), "%s: %s - %s", user_name, name, tempbuf);
    }
    else if (g_gameNamePtr)
    {
        Bsnprintf(apptitle, sizeof(apptitle), "%s - %s", g_gameNamePtr, tempbuf);
    }
    else
    {
        Bstrncpyz(apptitle, tempbuf, sizeof(apptitle));
    }

    wm_setapptitle(apptitle);
}

static void G_DisplayLogo(void)
{
    int soundanm = 0;
    int32_t logoflags = G_GetLogoFlags();

    ready2send = 0;

    KB_FlushKeyboardQueue();
    KB_ClearKeysDown(); // JBF

    videoSetViewableArea(0,0,xdim-1,ydim-1);
    videoClearViewableArea(0L);
    IFISSOFTMODE G_FadePalette(0,0,0,63);

    renderFlushPerms();
    videoNextPage();

    G_UpdateAppTitle();

    S_StopMusic();
    FX_StopAllSounds(); // JBF 20031228

    if (ud.multimode < 2 && (logoflags & LOGO_ENABLED) && !g_noLogo)
    {
        if (VOLUMEALL && (logoflags & LOGO_PLAYANIM))
        {

            if (!KB_KeyWaiting() && g_noLogoAnim == 0)
            {
                Net_GetPackets();
                G_PlayAnim("logo.anm",5);
                IFISSOFTMODE G_FadePalette(0,0,0,63);
                KB_FlushKeyboardQueue();
                KB_ClearKeysDown(); // JBF
            }

            videoClearViewableArea(0L);
            videoNextPage();
        }

        if (logoflags & LOGO_PLAYMUSIC)
            S_PlaySpecialMusicOrNothing(MUS_INTRO);

        if (!NAM)
        {
            fadepal(0,0,0, 0,64,7);
            //g_player[myconnectindex].ps->palette = drealms;
            //G_FadePalette(0,0,0,63);
            if (logoflags & LOGO_3DRSCREEN)
            {
                P_SetGamePalette(g_player[myconnectindex].ps, DREALMSPAL, 11);    // JBF 20040308
                rotatesprite(0,0,65536L,0,DREALMS,0,0,2+8+16+64, 0,0,xdim-1,ydim-1);
                videoNextPage();
                fadepal(0,0,0, 63,0,-7);
                totalclock = 0;
                while (totalclock < (TICRATE*7) && !KB_KeyWaiting() && !(MOUSE_GetButtons()&M_LEFTBUTTON))
                {
                    gameHandleEvents();
                    if (g_restorePalette)
                    {
                        P_SetGamePalette(g_player[myconnectindex].ps,g_player[myconnectindex].ps->palette,0);
                        g_restorePalette = 0;
                    }
                }
            }
            KB_ClearKeysDown(); // JBF
            MOUSE_ClearButton(M_LEFTBUTTON);
        }

        fadepal(0,0,0, 0,64,7);
        videoClearViewableArea(0L);
        videoNextPage();
        if (logoflags & LOGO_TITLESCREEN)
        {
            //g_player[myconnectindex].ps->palette = titlepal;
            P_SetGamePalette(g_player[myconnectindex].ps, TITLEPAL, 11);   // JBF 20040308
            renderFlushPerms();
            rotatesprite(0,0,65536L,0,BETASCREEN,0,0,2+8+16+64,0,0,xdim-1,ydim-1);
            KB_FlushKeyboardQueue();
            fadepal(0,0,0, 63,0,-7);
            totalclock = 0;

            while (totalclock < (860+120) && !KB_KeyWaiting() && !(MOUSE_GetButtons()&M_LEFTBUTTON))
            {
                rotatesprite(0,0,65536L,0,BETASCREEN,0,0,2+8+16+64,0,0,xdim-1,ydim-1);
                if (logoflags & LOGO_DUKENUKEM)
                {
                    if (totalclock > 120 && totalclock < (120+60))
                    {
                        if (soundanm == 0)
                        {
                            soundanm = 1;
                            S_PlaySound(PIPEBOMB_EXPLODE);
                        }
                        rotatesprite(160<<16,104<<16,((int32_t)totalclock-120)<<10,0,DUKENUKEM,0,0,2+8,0,0,xdim-1,ydim-1);
                    }
                    else if (totalclock >= (120+60))
                        rotatesprite(160<<16,(104)<<16,60<<10,0,DUKENUKEM,0,0,2+8,0,0,xdim-1,ydim-1);
                }
                else soundanm = 1;
                if (logoflags & LOGO_THREEDEE)
                {
                    if (totalclock > 220 && totalclock < (220+30))
                    {
                        if (soundanm == 1)
                        {
                            soundanm = 2;
                            S_PlaySound(PIPEBOMB_EXPLODE);
                        }

                        rotatesprite(160<<16,(104)<<16,60<<10,0,DUKENUKEM,0,0,2+8,0,0,xdim-1,ydim-1);
                        rotatesprite(160<<16,(129)<<16,((int32_t)totalclock - 220)<<11,0,THREEDEE,0,0,2+8,0,0,xdim-1,ydim-1);
                    }
                    else if (totalclock >= (220+30))
                        rotatesprite(160<<16,(129)<<16,30<<11,0,THREEDEE,0,0,2+8,0,0,xdim-1,ydim-1);
                }
                else soundanm = 2;
                if (PLUTOPAK && (logoflags & LOGO_PLUTOPAKSPRITE))
                {
                    // JBF 20030804
                    if (totalclock >= 280 && totalclock < 395)
                    {
                        rotatesprite(160<<16,(151)<<16,(410-(int32_t)totalclock)<<12,0,PLUTOPAKSPRITE+1,0,0,2+8,0,0,xdim-1,ydim-1);
                        if (soundanm == 2)
                        {
                            soundanm = 3;
                            S_PlaySound(FLY_BY);
                        }
                    }
                    else if (totalclock >= 395)
                    {
                        if (soundanm == 3)
                        {
                            soundanm = 4;
                            S_PlaySound(PIPEBOMB_EXPLODE);
                        }
                        rotatesprite(160<<16,(151)<<16,30<<11,0,PLUTOPAKSPRITE+1,0,0,2+8,0,0,xdim-1,ydim-1);
                    }
                }

                X_OnEvent(EVENT_LOGO, g_player[screenpeek].ps->i, screenpeek, -1);

                gameHandleEvents();

                if (g_restorePalette)
                {
                    P_SetGamePalette(g_player[myconnectindex].ps,g_player[myconnectindex].ps->palette,0);
                    g_restorePalette = 0;
                }
                videoNextPage();
            }
        }
        KB_ClearKeysDown(); // JBF
        MOUSE_ClearButton(M_LEFTBUTTON);
    }

    Net_WaitForPlayers();

    renderFlushPerms();
    videoClearViewableArea(0L);
    videoNextPage();

    //g_player[myconnectindex].ps->palette = palette;
    P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 0);    // JBF 20040308
    S_PlaySound(NITEVISION_ONOFF);

    //G_FadePalette(0,0,0,0);
    videoClearViewableArea(0L);
}

void freehash();
static void G_Cleanup(void)
{
    int i;

    for (i=(MAXLEVELS*(MAXVOLUMES+1))-1;i>=0;i--) // +1 volume for "intro", "briefing" music
    {
        Xfree(g_mapInfo[i].name);
        Xfree(g_mapInfo[i].filename);
        Xfree(g_mapInfo[i].musicfn);
        Xfree(g_mapInfo[i].alt_musicfn);

        if (g_mapInfo[i].savedstate != NULL)
            G_FreeMapState(i);
    }

    for (i=MAXQUOTES-1;i>=0;i--)
    {
        Xfree(apStrings[i]);
        Xfree(apXStrings[i]);
    }

    for (i=MAXPLAYERS-1;i>=0;i--)
    {
        if (g_player[i].ps != NULL)
            Xfree(g_player[i].ps);

        if (g_player[i].inputBits != NULL)
            Xfree(g_player[i].inputBits);
    }

    for (i = 0; i <= g_highestSoundIdx; i++)
    {
        if (g_sounds[i] != &nullsound)
        {
            DO_FREE_AND_NULL(g_sounds[i]->filename);

            if (g_sounds[i]->voices != &nullvoice)
                DO_FREE_AND_NULL(g_sounds[i]->voices);

            DO_FREE_AND_NULL(g_sounds[i]);
        }
    }

    DO_FREE_AND_NULL(g_sounds);

    Xfree(label);
    Xfree(labelcode);
    Xfree(apScript);
    Xfree(bitptr);

    auto ofs = vmoffset;
    while (ofs)
    {
        auto next = ofs->next;
        Xfree(ofs->fn);
        Xfree(ofs);
        ofs = next;
    }

    Gv_Clear();

    freehash();
    hash_free(&h_gamefuncs);

    // NetDuke32: Clear dukeanim here once backported.
    inthash_free(&h_dsound);
    inthash_free(&h_dynamictilemap);

    Duke_CommonCleanup();
}

/*
===================
=
= ShutDown
=
===================
*/

void G_Shutdown(void)
{
    CONFIG_WriteSetup(0);
    S_SoundShutdown();
    S_MusicShutdown();
    timerUninit();
    CONTROL_Shutdown();
    KB_Shutdown();
    engineUnInit();
    G_Cleanup();
#ifdef DISCORC_RPC
    Discord_Shutdown();
#endif
    uninitgroupfile();
    Bfflush(NULL);
}

/*
===================
=
= G_Startup
=
===================
*/

static void G_CompileScripts(void)
{
    int psm = pathsearchmode;

    label = (char*)Xmalloc(MAXLABELS << 6);
    labelcode = (int32_t*)Xmalloc(MAXLABELS * sizeof(int32_t));
    labeltype = (uint8_t*)Xmalloc(MAXLABELS * sizeof(uint8_t));

    if (g_scriptNamePtr != NULL)
        Bcorrectfilename(g_scriptNamePtr, 0);

    pathsearchmode = 1;
    C_Compile(G_ConFile());

    if (g_loadFromGroupOnly) // g_loadFromGroupOnly is true only when compiling fails and internal defaults are utilized
        C_Compile(G_ConFile());

    if ((unsigned int)g_labelCnt >= MAXLABELS)
        G_GameExit("Error: too many labels defined!");

    label = (char*)Xrealloc(label, g_labelCnt << 6);
    labelcode = (int32_t*)Xrealloc(labelcode, g_labelCnt * sizeof(int32_t));
    labeltype = (uint8_t*)Xrealloc(labeltype, g_labelCnt * sizeof(uint8_t));

    X_OnEvent(EVENT_INIT, -1, -1, -1);
    pathsearchmode = psm;
}

static void G_PostLoadPalette(void)
{
    //if (!(duke3d_globalflags & DUKE3D_NO_PALETTE_CHANGES))
    {
        // Make color index 255 of default/water/slime palette black.
        if (basepaltable[BASEPAL] != NULL)
            Bmemset(&basepaltable[BASEPAL][255 * 3], 0, 3);
        if (basepaltable[WATERPAL] != NULL)
            Bmemset(&basepaltable[WATERPAL][255 * 3], 0, 3);
        if (basepaltable[SLIMEPAL] != NULL)
            Bmemset(&basepaltable[SLIMEPAL][255 * 3], 0, 3);
    }

    //if (!(duke3d_globalflags & DUKE3D_NO_HARDCODED_FOGPALS))
        paletteSetupDefaultFog();

    //if (!(duke3d_globalflags & DUKE3D_NO_PALETTE_CHANGES))
        paletteFixTranslucencyMask();

    palettePostLoadLookups();
}

extern int startwin_run(void);
static void G_SetupGameButtons(void);

#define SETFLAG(Tilenum, Flag) g_tile[Tilenum].flags |= Flag
// Has to be after setting the dynamic names (e.g. SHARK).
static void A_InitEnemyFlags(void)
{
    int DukeEnemies[] = {
        SHARK, RECON, DRONE,
        LIZTROOPONTOILET, LIZTROOPJUSTSIT, LIZTROOPSTAYPUT, LIZTROOPSHOOT,
        LIZTROOPJETPACK, LIZTROOPSHOOT, LIZTROOPDUCKING, LIZTROOPRUNNING, LIZTROOP,
        OCTABRAIN, COMMANDER, COMMANDERSTAYPUT, PIGCOP, PIGCOPSTAYPUT, PIGCOPDIVE, EGG,
        LIZMAN, LIZMANSPITTING, LIZMANJUMP, ORGANTIC,
        BOSS1, BOSS2, BOSS3, BOSS4, RAT, ROTATEGUN };

    int SolidEnemies[] = { TANK, BOSS1, BOSS2, BOSS3, BOSS4, RECON, ROTATEGUN };
    int NoWaterDipEnemies[] = { OCTABRAIN, COMMANDER, DRONE };
    int GreenSlimeFoodEnemies[] = { LIZTROOP, LIZMAN, PIGCOP, NEWBEAST };

    for (bssize_t i = GREENSLIME; i <= GREENSLIME + 7; i++)
        SETFLAG(i, SFLAG_HARDCODED_BADGUY);

    for (bssize_t i = ARRAY_SIZE(DukeEnemies) - 1; i >= 0; i--)
        SETFLAG(DukeEnemies[i], SFLAG_HARDCODED_BADGUY);

    for (bssize_t i = ARRAY_SIZE(SolidEnemies) - 1; i >= 0; i--)
        SETFLAG(SolidEnemies[i], SFLAG_NODAMAGEPUSH);

    for (bssize_t i = ARRAY_SIZE(NoWaterDipEnemies) - 1; i >= 0; i--)
        SETFLAG(NoWaterDipEnemies[i], SFLAG_NOWATERDIP);

    for (bssize_t i = ARRAY_SIZE(GreenSlimeFoodEnemies) - 1; i >= 0; i--)
        SETFLAG(GreenSlimeFoodEnemies[i], SFLAG_GREENSLIMEFOOD);
}
#undef SETFLAG

static void G_PostCreateGameState(void)
{
    A_InitEnemyFlags();
    Net_InitializeStructPointers();
}

static void G_Startup(void)
{
    int i;

    timerInit(TICRATE);
    timerSetCallback(gameTimerHandler);

    G_CompileScripts();

    if (engineInit())
    {
        wm_msgbox("Build Engine Initialization Error",
                  "There was a problem initializing the Build engine: %s", engineerrstr);
        G_Cleanup();
        Bexit(EXIT_FAILURE);
    }

    G_InitDynamicNames();
    G_InitMultiPsky(CLOUDYOCEAN, MOONSKY1, BIGORBIT1, LA);
    G_PostCreateGameState();

    if (g_noSound) ud.config.SoundToggle = 0;
    if (g_noMusic) ud.config.MusicToggle = 0;

    if (CommandName)
    {
        //        Bstrncpy(szPlayerName, CommandName, 9);
        //        szPlayerName[10] = '\0';
        Bstrcpy(tempbuf,CommandName);

        while (Bstrlen(OSD_StripColors(tempbuf,tempbuf)) > 10)
            tempbuf[Bstrlen(tempbuf)-1] = '\0';

        Bstrncpy(szPlayerName,tempbuf,sizeof(szPlayerName)-1);
        szPlayerName[sizeof(szPlayerName)-1] = '\0';
    }

    if (CommandMap)
    {
        if (VOLUMEONE)
        {
            LOG_F(ERROR, "The -map option is available in the registered version only!");
            boardfilename[0] = 0;
        }
        else
        {
            char *dot, *slash;

            boardfilename[0] = '/';
            boardfilename[1] = 0;
            Bstrcat(boardfilename, CommandMap);

            dot = Bstrrchr(boardfilename,'.');
            slash = Bstrrchr(boardfilename,'/');
            if (!slash) slash = Bstrrchr(boardfilename,'\\');

            if ((!slash && !dot) || (slash && dot < slash))
                Bstrcat(boardfilename,".map");

            Bcorrectfilename(boardfilename,0);

            i = kopen4loadfrommod(boardfilename,0);
            if (i!=-1)
            {
                LOG_F(INFO, "Using level: '%s'.",boardfilename);
                kclose(i);
            }
            else
            {
                LOG_F(ERROR, "Level '%s' not found.",boardfilename);
                boardfilename[0] = 0;
            }
        }
    }

    if (VOLUMEONE)
    {
        LOG_F(INFO, "*** You have run Duke Nukem 3D %d times. ***",ud.executions);
        if (ud.executions >= 50) LOG_F(INFO, "IT IS NOW TIME TO UPGRADE TO THE COMPLETE VERSION!!!");
    }

    //LOG_F("* Hold Esc to Abort. *\n");
    //LOG_F("Loading art header...\n");

    if (artLoadFiles("tiles000.art", MAXCACHE1DSIZE) < 0)
        G_GameExit("Failed loading art.");

    G_LoadLookups();

    ReadSaveGameHeaders();

    tilesiz[MIRROR].x = tilesiz[MIRROR].y = 0;

    screenpeek = myconnectindex;
}

void G_UpdatePlayerFromMenu(void)
{
    if (ud.recstat != 0)
        return;

    if (numplayers > 1)
    {
        Net_SendPlayerOptions();
        if (sprite[g_player[myconnectindex].ps->i].picnum == APLAYER && sprite[g_player[myconnectindex].ps->i].pal != 1)
            sprite[g_player[myconnectindex].ps->i].pal = g_player[myconnectindex].pcolor;
    }
    else
    {
        g_player[myconnectindex].ps->auto_aim = ud.config.AutoAim;
        g_player[myconnectindex].ps->weaponswitch = ud.weaponswitch;
        g_player[myconnectindex].ps->palookup = g_player[myconnectindex].pcolor = ud.color;
        g_player[myconnectindex].pteam = ud.team;

        if (sprite[g_player[myconnectindex].ps->i].picnum == APLAYER && sprite[g_player[myconnectindex].ps->i].pal != 1)
            sprite[g_player[myconnectindex].ps->i].pal = g_player[myconnectindex].pcolor;
    }
}

void G_BackToMenu(void)
{
    boardfilename[0] = 0;

    if (ud.recstat == 1)
        G_CloseDemoWrite();

    ud.warp_on = 0;
    ud.gm = MODE_MENU;
    ChangeToMenu(0);
    KB_FlushKeyboardQueue();
    G_UpdateAppTitle();
}

void app_crashhandler(void)
{
    G_CloseDemoWrite();
    VM_ScriptInfo(insptr, 64);
    Net_SendQuit();
}

static void G_FatalEngineInitError(void)
{
#ifdef DEBUGGINGAIDS
    debug_break();
#endif
    //G_Cleanup();
    Bsprintf(tempbuf, "There was a problem initializing the engine: %s", engineerrstr);
    LOG_F(ERROR, "%s", tempbuf);
    //fatal_exit(tempbuf);
    G_Shutdown();
}

static int G_EndOfLevel(void)
{
    auto& p = *g_player[myconnectindex].ps;

    P_SetGamePalette(&p, BASEPAL, 0);
    P_UpdateScreenPal(&p);

    if (ud.gm & MODE_EOL)
    {
        G_CloseDemoWrite();

        if (p.player_par > 0 && (p.player_par < ud.playerbest || ud.playerbest < 0) && ud.display_bonus_screen == 1)
            CONFIG_SetMapBestTime(g_loadedMapHack.md4, p.player_par);

        if (ud.display_bonus_screen == 1)
        {
            int const ssize = ud.screen_size;
            ud.screen_size = 0;
            G_UpdateScreenArea();
            ud.screen_size = ssize;
            G_BonusScreen(0);
        }

        // Clear potentially loaded per-map ART only after the bonus screens.
        artClearMapArt();

        if (ud.eog)
        {
            ud.eog = 0;
            if (ud.multimode < 2)
            {
                if (!VOLUMEALL)
                {
                    G_DoOrderScreen();
                }
                ud.gm = MODE_MENU;
                ChangeToMenu(0);
                probey = 0;
                return 2;
            }
            else
            {
                ud.level_number = 0;
                ud.volume_number++;

                if(ud.volume_number > g_numVolumes)
                    ud.volume_number = 0;

                if (GTFLAGS(GAMETYPE_COOP) && !DMFLAGS_TEST(DMFLAG_KEEPITEMSACROSSEPISODES))
                    ud.last_level = -1; // Set last_level to -1 to force G_EnterLevel and its children to reset health and such.
            }
        }
    }

    ud.display_bonus_screen = 1;
    ready2send = 0;

    Net_WaitForPlayers();

    // [JM] Forces menu versions of volume/level number to new values. Prevents desync if someone fucked with their settings.
    G_RevertUnconfirmedGameSettings();

    // A part of the original codebase, it's disabled now due to Survival mode
    // signaling map reload with MODE_RESTART. Additionally, this caused
    // a discrepancy between real and fake multiplayer games.
#if 0
    if (numplayers > 1)
        ud.gm = MODE_GAME;
#endif

    if (G_EnterLevel(ud.gm))
    {
        G_BackToMenu();
        return 2;
    }

    return 1;
}

static void G_DrawFrame()
{
    // only allow binds to function if the player is actually in a game (not in a menu, typing, et cetera) or demo
    CONTROL_BindsEnabled = (ud.gm == MODE_GAME || ud.gm == MODE_DEMO);
    G_HandleLocalKeys();
    OSD_DispatchQueued();
   
    //P_GetInput(myconnectindex, true); // [JM] We're not ready for uncapped input yet.

    int smoothRatio = calc_smoothratio(totalclock, ototalclock);

    // Necessary anymore?
    if (ud.statusbarmode == 1 && (ud.statusbarscale == 100 || !videoGetRenderMode()))
    {
        ud.statusbarmode = 0;
        G_UpdateScreenArea();
    }
    
    // [JM] Probably a vain attempt at preventing screenpeek manipulation with CE.
    if ((numplayers > 1) && !GTFLAGS(GAMETYPE_COOPVIEW) && (screenpeek != myconnectindex))
        screenpeek = myconnectindex;

    // Hideous hack to get players to not trail behind in mirrors or look fucky in third person.
    if (numplayers > 1 && (screenpeek == myconnectindex))
    {
        Net_SwapPredictedLinkedLists();
        Net_UsePredictedPointers();

        auto pSprite = originalPlayer->i;
        sprite[pSprite] = predicted_sprite[pSprite];

        // And an even worse hack to get slimers to attach to your face.
        // Get rid of this once we have full actor prediction again.
        // And retarget slimer sprite to it's predicted version.
        auto slimerSprite = g_player[myconnectindex].ps->somethingonplayer;
        if (slimerSprite > -1 && sprite[slimerSprite].picnum >= GREENSLIME && sprite[slimerSprite].picnum <= (GREENSLIME + 7))
        {
            vec3_t slimerPos = sprite[slimerSprite].xyz;
            int16_t slimerSect = sprite[slimerSprite].sectnum;
            auto tData = actor[slimerSprite].t_data[2];

            vec3_t pos;
            fix16_t q16ang, q16horiz;
            int16_t sectnum;
            G_CalculateCameraInterpolation(g_player[screenpeek].ps, &pos, &q16ang, &q16horiz, &sectnum, smoothRatio);

            sprite[slimerSprite].x = pos.x + (sintable[(fix16_to_int(q16ang) + 512) & 2047] >> 7);
            sprite[slimerSprite].y = pos.y + (sintable[fix16_to_int(q16ang) & 2047] >> 7);
            sprite[slimerSprite].z = pos.z - tData + (8 << 8);
            sprite[slimerSprite].z += fix16_to_int(F16(100) - q16horiz) << 4;
            changespritesect(slimerSprite, sectnum);

            G_DrawRooms(screenpeek, smoothRatio);

            sprite[slimerSprite].xyz = slimerPos;
            changespritesect(slimerSprite, slimerSect);
        }
        else
            G_DrawRooms(screenpeek, smoothRatio);
    }
    else
    {
        G_DrawRooms(screenpeek, smoothRatio);
    }

    if (videoGetRenderMode() >= REND_POLYMOST)
        G_DrawBackground();

    G_DisplayRest(smoothRatio);

    if (numplayers > 1 && (screenpeek == myconnectindex))
    {
        Net_SwapPredictedLinkedLists();
        Net_UseOriginalPointers();
    }

    // Moved out of G_DisplayRest, since menu code can *ROYALLY* fuck up the predicted pointers.
    if (ud.gm & MODE_TYPE)
        Net_EnterMessage();
    else
        M_DisplayMenus();

    G_ShowVote();

    if (BUTTON(gamefunc_Show_Scoreboard))
        G_ShowScores(false);

    if (debug_on)
        G_ShowCacheLocks();

    Net_DisplaySyncMsg();

    if (VOLUMEONE)
    {
        if (ud.show_help == 0 && g_showShareware > 0 && (ud.gm & MODE_MENU) == 0)
            rotatesprite((320 - 50) << 16, 9 << 16, 65536L, 0, BETAVERSION, 0, 0, 2 + 8 + 16 + 128, 0, 0, xdim - 1, ydim - 1);
    }
    // ---------

    videoNextPage();
    S_Update();
}

static void G_CheckGametype(void)
{
    // LOG_F("ud.m_coop=%i before sanitization\n",ud.m_coop);
    ud.m_coop = clamp(ud.m_coop, 0, g_numGametypes - 1);
    Bsprintf(tempbuf, "%s\n", Gametypes[ud.m_coop].name);
    LOG_F(INFO, "%s", tempbuf);
    // LOG_F("ud.m_coop=%i after sanitisation\n",ud.m_coop);
}

bool G_InitMultiplayer()
{
    for (int32_t i = 0; i < MAXPLAYERS; i++)
        g_player[i].playerreadyflag = 0;

    if (initmultiplayersparms(netparamcount, netparam))
    {
        LOG_F(INFO, "Waiting for players...");
        while (initmultiplayerscycle())
        {
            handleevents();
            if (quitevent)
            {
                G_Shutdown();
                return false;
            }
        }
    }

    if (netparam)
        Xfree(netparam);

    netparam = NULL;
    netparamcount = 0;
    botNameSeed = time(0);

    Net_SendPlayerName();

    if (numplayers > 1)
    {
        LOG_F(INFO, "Multiplayer initialized.");

        // Init real players, making sure they aren't set as bots. Allows player/bot mix.
        for (int32_t i = 1; i < numplayers; i++)
        {
            G_MaybeAllocPlayer(i);
            g_player[i].connected = 1;
            g_player[i].isBot = 0;
        }

        if (numplayers > ud.multimode)
            ud.multimode = numplayers;
        
        Net_SendVersion();

        if (myconnectindex == connecthead) // Host
            Net_SendInitialSettings();
        else while (!oldnet_gotinitialsettings)
            gameHandleEvents();

        Net_SendPlayerOptions();
        Net_SendWeaponChoice();
        Net_SendUserMapName();

        Net_WaitForPlayers();
    }

    // Fill in base stats for bots. To-do: Move some of this into the bot init code.
    int32_t teamToggle = 1;
    for (int32_t i = numplayers; i < ud.multimode; i++)
    {
        Bsprintf(g_player[i].user_name, "PLAYER %d", i + 1);

        G_MaybeAllocPlayer(i);
        g_player[i].connected = 1;
        g_player[i].isBot = 1;
        g_player[i].ps->team = g_player[i].pteam = teamToggle;
        g_player[i].ps->weaponswitch = 3;
        g_player[i].ps->auto_aim = 0;
        teamToggle = (!teamToggle);
    }

    DukeBot_ResetAll();

    if (ud.multimode > 1)
    {
        playerswhenstarted = ud.multimode;
        G_CheckGametype();
    }

    if (numplayers > 1)
        return true;

    return false;
}

static const char* dukeVerbosityCallback(loguru::Verbosity verbosity)
{
    switch (verbosity)
    {
        default: return nullptr;
        case LOG_VM: return "VM";
        case LOG_CON: return "CON";
    }
}

int app_main(int32_t argc, char const* const* argv)
{
    int i = 0;
    char cwd[BMAX_PATH];

    engineSetLogFile(APPBASENAME ".log", LOG_GAME_MAX);
    engineSetLogVerbosityCallback(dukeVerbosityCallback);

#ifdef _WIN32
#ifndef DEBUGGINGAIDS
    if (!G_CheckCmdSwitch(argc, argv, "-noinstancechecking") && !windowsCheckAlreadyRunning())
    {
        if (!wm_ynbox(APPNAME, "It looks like " APPNAME " is already running.\n\n"
            "Are you sure you want to start another copy?"))
            return 3;
    }
#endif
#endif

    G_ExtPreInit(argc, argv);
    
    osdcallbacks_t callbacks = {};
    callbacks.drawchar          = GAME_drawosdchar;
    callbacks.drawstr           = GAME_drawosdstr;
    callbacks.drawcursor        = GAME_drawosdcursor;
    callbacks.getcolumnwidth    = GAME_getcolumnwidth;
    callbacks.getrowheight      = GAME_getrowheight;
    callbacks.clear             = GAME_clearbackground;
    callbacks.gettime           = BGetTime;
    callbacks.onshowosd         = GAME_onshowosd;
    OSD_SetCallbacks(callbacks);

    G_UpdateAppTitle();

    LOG_F(INFO, HEAD2 " %s", s_buildRev);
    PrintBuildInfo();

    if (!g_useCwd)
        G_AddSearchPaths();

//    LOG_F("Compiled %s\n",datetimestring);
//    LOG_F("Copyright (c) 1996, 2003 3D Realms Entertainment\n");
//    LOG_F("Copyright (c) 2008 EDuke32 team and contributors\n");

#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    addsearchpath("/usr/share/games/jfduke3d");
    addsearchpath("/usr/local/share/games/jfduke3d");
    addsearchpath("/usr/share/games/eduke32");
    addsearchpath("/usr/local/share/games/eduke32");
#elif defined(__APPLE__)
    addsearchpath("/Library/Application Support/JFDuke3D");
    addsearchpath("/Library/Application Support/EDuke32");
#endif

    ud.multimode = 1;

    initsynccrc();

    enginecompatibilitymode = ENGINE_EDUKE32;//ENGINE_19961112;

    G_CheckCommandLine(argc,argv);
#if 0 // OLDMP MERGE CHECKME
    if (netparamcount > 0) _buildargc = (argc -= netparamcount + 1);  // crop off the net parameters
#endif

    // This needs to happen afterwards, as G_CheckCommandLine() is where we set
    // up the command-line-provided search paths (duh).
    G_ExtInit();

    playerColor_init();
    G_MaybeAllocPlayer(0);
    g_player[0].connected = 1;

    if (getcwd(cwd,BMAX_PATH))
        addsearchpath(cwd);

    hash_init(&h_gamefuncs);
    for (i=NUMGAMEFUNCTIONS-1;i>=0;i--)
    {
        char *str = Bstrtolower(Xstrdup(gamefunctions[i]));
        hash_add(&h_gamefuncs,gamefunctions[i],i, 0);
        hash_add(&h_gamefuncs,str,i, 0);
        Xfree(str);
    }

    LOG_F(INFO, "Using config file '%s'.", g_setupFileName);
    int32_t const readSetup = CONFIG_ReadSetup();

#ifdef _WIN32
//    LOG_F(INFO, "build %d",(char)atoi(BUILDDATE));
#endif
    if (enginePreInit())
    {
        wm_msgbox("Build Engine Initialization Error",
                  "There was a problem initializing the Build engine: %s", engineerrstr);
        Bexit(EXIT_FAILURE);
    }   

#ifdef DISCORD_RPC
    InitDiscord();
#endif

    G_ScanGroups();

// OLDMP MERGE CHECKME
#ifdef STARTUP_SETUP_WINDOW
    if (readSetup < 0 || (!g_noSetup && (ud.configversion != BYTEVERSION_NETDUKE32 || ud.config.ForceSetup)) || g_commandSetup)
    {
        if (quitevent || !startwin_run())
        {
#ifdef DISCORD_RPC
            Discord_Shutdown();
#endif

            engineUnInit();
            Bexit(EXIT_SUCCESS);
        }
    }
#endif

    G_LoadGroups(!g_noAutoLoad);

    i = initgroupfile(eduke32dat);

    if (i == -1)
        LOG_F(WARNING, "Could not find internal data file '%s'!", eduke32dat);
    else 
        LOG_F(INFO, "Using '%s' for internal data.", eduke32dat);

    i = -1;

    if (!g_useCwd)
        G_CleanupSearchPaths();

    if (NAM)
    {
        Bsprintf(Gametypes[0].name, "GRUNTMATCH");
        Bsprintf(Gametypes[2].name, "TEAM GRUNTMATCH");
    }

    if (WW2GI)
    {
        Bstrcpy(Gametypes[0].name, "GI MATCH");
        Bstrcpy(Gametypes[2].name, "TEAM GI MATCH");
    }

    //G_SetupCheats();

    if (SHAREWARE)
        g_Shareware = 1;
    else
    {
        buildvfs_kfd const kFile = kopen4load("DUKESW.BIN", 1); // JBF 20030810

        if (kFile != buildvfs_kfd_invalid)
        {
            g_Shareware = 1;
            kclose(kFile);
        }
    }

#if 0
    copyprotect();
    if (cp) return;
#endif

    // gotta set the proper title after we compile the CONs if this is the full version
    G_UpdateAppTitle();

    if (g_scriptDebug)
        LOG_F(INFO, "CON debugging activated (level %d).",g_scriptDebug);

    if (VOLUMEONE)
    {
        LOG_F(INFO, "Distribution of shareware Duke Nukem 3D is restricted in certain ways.");
        LOG_F(INFO, "Please read LICENSE.DOC for more details.");
    }

    G_Startup(); // a bunch of stuff including compiling cons

    ud.color = playerColor_getValidPal(ud.m_color);
    g_player[0].ps->palookup = g_player[0].pcolor = ud.color;

    if (!loaddefinitionsfile(eduke32def))
    {
        LOG_F(INFO, "Internal def file '%s' loaded.", eduke32def);
        loaddefinitions_game(eduke32def, FALSE);
    }

    const char* defsfile = G_DefFile();
    uint32_t stime = timerGetTicks();
    if (!loaddefinitionsfile(defsfile))
    {
        uint32_t etime = timerGetTicks();
        LOG_F(INFO, "Definitions file \"%s\" loaded in %d ms.", defsfile, etime - stime);
    }
    loaddefinitions_game(defsfile, FALSE);

    S_PrecacheSounds();

    if (enginePostInit())
        G_FatalEngineInitError();

    G_PostLoadPalette();

    if (quitevent)
        return 4;

    if (!G_InitMultiplayer())
    {
        if (boardfilename[0] != 0)
        {
            ud.m_level_number = 7;
            ud.m_volume_number = 0;
            ud.warp_on = 1;
        }
    }

    ChatPipe_Create();

    auto& myplayer = *g_player[myconnectindex].ps;
    myplayer.palette = BASEPAL;

    // [JM] I'll need to make sure this doesn't fuck up a few command lines.
    CONFIG_WriteSetup(1);
    CONFIG_ReadSetup();

    char const* rtsname = g_rtsNamePtr ? g_rtsNamePtr : ud.rtsname;
    RTS_Init(rtsname);

    ud.last_level = -1;

    LOG_F(INFO, "Initializing OSD...");
    Bsprintf(tempbuf,HEAD2 " - %s (%s)",s_buildRev, s_buildTimestamp);
    OSD_SetVersion(tempbuf, 10, 0);
    OSD_SetParameters(0, 0, 0, 12, 2, 12, OSD_ERROR, OSDTEXT_RED, OSDTEXT_DARKRED, gamefunctions[gamefunc_Show_Console][0] == '\0' ? OSD_PROTECTED : 0);
    registerosdcommands();

    if (CONTROL_Startup(controltype_keyboardandmouse, &GetTime, TICRATE))
    {
#ifdef DISCORD_RPC
        Discord_Shutdown();
#endif
        engineUnInit();
        Bexit(EXIT_FAILURE);
    }

    G_SetupGameButtons();
    CONFIG_SetupMouse();
    CONFIG_SetupJoystick();

    CONTROL_JoystickEnabled = (ud.config.UseJoystick && CONTROL_JoyPresent);
    CONTROL_MouseEnabled = (ud.config.UseMouse && CONTROL_MousePresent);

    if (videoSetGameMode(ud.config.ScreenMode,ud.config.ScreenWidth,ud.config.ScreenHeight,ud.config.ScreenBPP, ud.detail) < 0)
    {
        int i = 0;
        int xres[] = {ud.config.ScreenWidth,800,640,320};
        int yres[] = {ud.config.ScreenHeight,600,480,240};
        int bpp[] = {32,16,8};

        LOG_F(ERROR, "Failure setting video mode %dx%dx%d %s! Attempting safer mode...",
                   ud.config.ScreenWidth,ud.config.ScreenHeight,ud.config.ScreenBPP,ud.config.ScreenMode?"fullscreen":"windowed");

#if defined(USE_OPENGL)
        {
            int j = 0;
            while (videoSetGameMode(0,xres[i],yres[i],bpp[j], ud.detail) < 0)
            {
                LOG_F(ERROR, "Failure setting video mode %dx%dx%d windowed! Attempting safer mode...",xres[i],yres[i],bpp[i]);
                j++;
                if (j == 3)
                {
                    i++;
                    j = 0;
                }
                if (i == 4)
                    G_GameExit("Unable to set failsafe video mode!");
            }
        }
#else
        while (videoSetGameMode(0,xres[i],yres[i],8, ud.detail) < 0)
        {
            LOG_F(ERROR, "Failure setting video mode %dx%dx%d windowed! Attempting safer mode...",xres[i],yres[i],8);
            i++;
        }
#endif
        ud.config.ScreenWidth = xres[i];
        ud.config.ScreenHeight = yres[i];
        ud.config.ScreenBPP = bpp[i];
    }

    videoSetPalette(ud.brightness >> 2, g_player[myconnectindex].ps->palette, 0);

    CONFIG_ReadSettings();
    OSD_Exec("autoexec.cfg");

    CONFIG_SetDefaultKeys(keydefaults, true);

    system_getcvars();

    LOG_F(INFO, "Initializing music...");
    S_MusicStartup();
    LOG_F(INFO, "Initializing sound...");
    S_SoundStartup();

    FX_StopAllSounds();

    if (ud.warp_on > 1 && ud.multimode < 2)
    {
        videoClearViewableArea(0L);
        //g_player[myconnectindex].ps->palette = palette;
        //G_FadePalette(0,0,0,0);
        P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 0);    // JBF 20040308
        rotatesprite(320<<15,200<<15,65536L,0,LOADSCREEN,0,0,2+8+64,0,0,xdim-1,ydim-1);
        menutext(160,105,0,0,"LOADING SAVED GAME...");
        videoNextPage();

        if (G_LoadPlayer(ud.warp_on-2))
            ud.warp_on = 0;
    }

MAIN_LOOP_RESTART:
    totalclock = 0;
    ototalclock = 0;
    lockclock = 0;

    G_GetCrosshairColor();
    G_SetCrosshairColor(CrosshairColors.r, CrosshairColors.g, CrosshairColors.b);

    if (ud.warp_on == 0)
    {
        if (ud.multimode > 1 && boardfilename[0] != 0)
        {
            ud.m_level_number = 7;
            ud.m_volume_number = 0;

            Net_WaitForPlayers();
            
            G_NewGame(NEWGAME_NOSEND | NEWGAME_RESETALL);
        }
        else G_DisplayLogo();
    }
    else if (ud.warp_on == 1)
    {
        G_NewGame(NEWGAME_NORMAL);
    }
    else G_UpdateScreenArea();

    if (ud.warp_on == 0 && G_PlaybackDemo())
    {
        FX_StopAllSounds();

        g_noLogoAnim = 1;
        goto MAIN_LOOP_RESTART;
    }

    myplayer.auto_aim = ud.config.AutoAim;
    myplayer.weaponswitch = ud.weaponswitch;
    g_player[myconnectindex].pteam = ud.team;

#if 0 // This should probably be removed now. Causes problems with player color, and is handled elsewhere anyway.
    if (Gametypes[ud.coop].flags & GAMETYPE_TDM)
        myplayer.palookup = g_player[myconnectindex].pcolor = teams[g_player[myconnectindex].pteam].palnum;
    else
    {
        if (ud.color)
            myplayer.palookup = g_player[myconnectindex].pcolor = playerColor_getValidPal(ud.color);
        else
            myplayer.palookup = g_player[myconnectindex].pcolor;
    }
#endif

    ud.warp_on = 0;
    KB_KeyDown[sc_Pause] = 0;   // JBF: I hate the pause key

    do // main loop
    {
        if (gameHandleEvents() && quitevent)
        {
            // JBF
            KB_KeyDown[sc_Escape] = 1;
            quitevent = 0;
        }

        static bool frameJustDrawn;
        //bool gameUpdate = false;
        //double gameUpdateStartTime = timerGetHiTicks();

        if (((ud.multimode > 1) || (ud.gm & (MODE_MENU | MODE_DEMO)) == 0) && (int32_t)(totalclock - ototalclock) >= TICSPERFRAME)
        {
            do
            {
                if (!frameJustDrawn)
                    break;

                frameJustDrawn = false;
                P_GetInput(myconnectindex, false);
                netInput = localInput;
                localInput = {}; // Input has been processed, flush.
                
                // The while condition in this loop is a duplicate of the if condition above. Why?
                // Why not check for readytosend in the above "if" and cut this out?
                do
                {
                    if (ready2send == 0)
                        break;

                    ototalclock += TICSPERFRAME;

                    Net_HandleInput();

                    if (((ud.show_help == 0 && (ud.gm & MODE_MENU) != MODE_MENU) || ud.recstat == 2 || (ud.multimode > 1))
                        && (ud.gm & MODE_GAME))
                    {
                        G_MoveLoop();
                    }
                } while (((ud.multimode > 1) || (ud.gm & (MODE_MENU | MODE_DEMO)) == 0) && (int32_t)(totalclock - ototalclock) >= TICSPERFRAME);

                //gameUpdate = true;
                //g_gameUpdateTime = timerGetHiTicks() - gameUpdateStartTime;

                //if (g_gameUpdateAvgTime <= 0.0)
                //    g_gameUpdateAvgTime = g_gameUpdateTime;

                //g_gameUpdateAvgTime = ((GAMEUPDATEAVGTIMENUMSAMPLES - 1.f) * g_gameUpdateAvgTime + g_gameUpdateTime) / ((float)GAMEUPDATEAVGTIMENUMSAMPLES);
            } while (0);
        }

        G_DoCheats();

        if (ud.gm & (MODE_EOL | MODE_RESTART))
        {
            switch (G_EndOfLevel())
            {
            case 1: continue;
            case 2: goto MAIN_LOOP_RESTART;
            }
        }

        if (engineFPSLimit())
        {
            G_DrawFrame();

            //if (gameUpdate)
            //    g_gameUpdateAndDrawTime = timerGetHiTicks() - gameUpdateStartTime;

            frameJustDrawn = true;
        }

        // KEEP THIS AT THE END OF THIS WHILE LOOP
        if (ud.gm & MODE_DEMO)
            goto MAIN_LOOP_RESTART;
    } while (1);

    G_GameExit(" ");

    return 0;
}

static int demo_version;

static int G_OpenDemoRead(int g_whichDemo) // 0 = mine
{
    char d[13];
    char ver;
    int i;

    Bstrcpy(d, "demo_.dmo");

    if (g_whichDemo == 10)
        d[4] = 'x';
    else
        d[4] = '0' + g_whichDemo;

    ud.reccnt = 0;

    if (g_whichDemo == 1 && firstdemofile[0] != 0)
    {
        if ((recfilep = kopen4loadfrommod(firstdemofile,g_loadFromGroupOnly)) == -1) return(0);
    }
    else
        if ((recfilep = kopen4loadfrommod(d,g_loadFromGroupOnly)) == -1) return(0);

    if (kread(recfilep,&ud.reccnt,sizeof(int)) != sizeof(int)) goto corrupt;
    if (kread(recfilep,&ver,sizeof(char)) != sizeof(char)) goto corrupt;

    if (ver != BYTEVERSION /*&& ver != 116 && ver != 117*/)
    {
        /* old demo playback */
        if (ver == BYTEVERSION_NETDUKE32)   LOG_F(ERROR, "Demo %s is for Regular edition.", d);
        else if (ver == BYTEVERSION_NETDUKE32+1) LOG_F(ERROR, "Demo %s is for Atomic edition.", d);
        else if (ver == BYTEVERSION_NETDUKE32+2) LOG_F(ERROR, "Demo %s is for Shareware version.", d);
//        else OSD_Printf("Demo %s is of an incompatible version (%d).\n", d, ver);
        kclose(recfilep);
        ud.reccnt=0;
        demo_version = 0;
        return 0;
    }
    else
    {
        demo_version = ver;
        OSD_Printf("Demo %s is of version %d.\n", d, ver);
    }

    if (kread(recfilep,(char *)&ud.volume_number,sizeof(char)) != sizeof(char)) goto corrupt;
    OSD_Printf("ud.volume_number: %d\n",ud.volume_number);
    if (kread(recfilep,(char *)&ud.level_number,sizeof(char)) != sizeof(char)) goto corrupt;
    OSD_Printf("ud.level_number: %d\n",ud.level_number);
    if (kread(recfilep,(char *)&ud.player_skill,sizeof(char)) != sizeof(char)) goto corrupt;
    OSD_Printf("ud.player_skill: %d\n",ud.player_skill);
    if (kread(recfilep,(char *)&ud.m_coop,sizeof(char)) != sizeof(char)) goto corrupt;
    OSD_Printf("ud.m_coop: %d\n",ud.m_coop);
    if (kread(recfilep,(short *)&ud.multimode,sizeof(short)) != sizeof(short)) goto corrupt;
    OSD_Printf("ud.multimode: %d\n",ud.multimode);
    if (kread(recfilep,(int *)&ud.m_dmflags,sizeof(int)) != sizeof(int)) goto corrupt;
    OSD_Printf("ud.m_dmflags: %d\n",ud.m_dmflags);
    if (kread(recfilep,(int *)&ud.playerai,sizeof(int)) != sizeof(int)) goto corrupt;
    OSD_Printf("ud.playerai: %d\n",ud.playerai);
    if (kread(recfilep,(int *)&i,sizeof(int)) != sizeof(int)) goto corrupt;

    if (kread(recfilep,(char *)boardfilename,sizeof(boardfilename)) != sizeof(boardfilename)) goto corrupt;

    if (boardfilename[0] != 0)
    {
        ud.m_level_number = 7;
        ud.m_volume_number = 0;
    }

    for (i=0;i<ud.multimode;i++)
    {
        if (!g_player[i].ps)
            g_player[i].ps = (DukePlayer_t *) Xcalloc(1,sizeof(DukePlayer_t));
        if (!g_player[i].inputBits)
            g_player[i].inputBits = (input_t *) Xcalloc(1,sizeof(input_t));

        if (kread(recfilep,(char *)g_player[i].user_name,sizeof(g_player[i].user_name)) != sizeof(g_player[i].user_name)) goto corrupt;
        OSD_Printf("ud.user_name: %s\n",g_player[i].user_name);
        if (kread(recfilep,(int *)&g_player[i].ps->aim_mode,sizeof(int)) != sizeof(int)) goto corrupt;
        if (kread(recfilep,(int *)&g_player[i].ps->auto_aim,sizeof(int)) != sizeof(int)) goto corrupt;  // JBF 20031126
        if (kread(recfilep,(int *)&g_player[i].ps->weaponswitch,sizeof(int)) != sizeof(int)) goto corrupt;
        if (kread(recfilep,(int *)&g_player[i].pcolor,sizeof(int)) != sizeof(int)) goto corrupt;
        g_player[i].ps->palookup = g_player[i].pcolor;
        if (kread(recfilep,(int *)&g_player[i].pteam,sizeof(int)) != sizeof(int)) goto corrupt;
        g_player[i].ps->team = g_player[i].pteam;
    }
    i = ud.reccnt/((TICRATE/TICSPERFRAME)*ud.multimode);
    OSD_Printf("demo duration: %d min %d sec\n", i/60, i%60);

    ud.god = ud.cashman = ud.eog = ud.showallmap = 0;
    ud.noclip = ud.scrollmode = ud.overhead_on = ud.pause_on = 0;

    G_MakeNewGamePreparations(ud.volume_number,ud.level_number,ud.player_skill);
    return(1);
corrupt:
    OSD_Printf(OSD_ERROR "Demo %d header is corrupt.\n",g_whichDemo);
    ud.reccnt = 0;
    kclose(recfilep);
    return 0;
}

void G_OpenDemoWrite(void)
{
    char d[13];
    int dummylong = 0, demonum=1;
    char ver;
    short i;

    if (ud.recstat == 2) kclose(recfilep);

    ver = BYTEVERSION;

    while (1)
    {
        if (demonum == 10000) return;
        Bsprintf(d, "demo%d.dmo", demonum++);
        frecfilep = fopen(d, "rb");
        if (frecfilep == NULL) break;
        Bfclose(frecfilep);
    }

    if ((frecfilep = fopen(d,"wb")) == NULL) return;
    fwrite(&dummylong,4,1,frecfilep);
    fwrite(&ver,sizeof(char),1,frecfilep);
    fwrite((char *)&ud.volume_number,sizeof(char),1,frecfilep);
    fwrite((char *)&ud.level_number,sizeof(char),1,frecfilep);
    fwrite((char *)&ud.player_skill,sizeof(char),1,frecfilep);
    fwrite((char *)&ud.m_coop,sizeof(char),1,frecfilep);
    fwrite((short *)&ud.multimode,sizeof(short),1,frecfilep);
    fwrite((int *)&ud.m_dmflags,sizeof(int),1,frecfilep);
    fwrite((int *)&ud.playerai,sizeof(int),1,frecfilep);
    fwrite((int *)&ud.auto_run,sizeof(int),1,frecfilep);
    fwrite((char *)boardfilename,sizeof(boardfilename),1,frecfilep);

    for (i=0;i<ud.multimode;i++)
    {
        fwrite((char *)&g_player[i].user_name,sizeof(g_player[i].user_name),1,frecfilep);
        fwrite((int *)&g_player[i].ps->aim_mode,sizeof(int),1,frecfilep);
        fwrite((int *)&g_player[i].ps->auto_aim,sizeof(int),1,frecfilep);       // JBF 20031126
        fwrite(&g_player[i].ps->weaponswitch,sizeof(int),1,frecfilep);
        fwrite(&g_player[i].pcolor,sizeof(int),1,frecfilep);
        fwrite(&g_player[i].pteam,sizeof(int),1,frecfilep);
    }

    totalreccnt = 0;
    ud.reccnt = 0;
}

static void G_DemoRecord(void)
{
    for (int16_t ALL_PLAYERS(i))
    {
        copybufbyte(g_player[i].inputBits,&recsync[ud.reccnt],sizeof(input_t));
        ud.reccnt++;
        totalreccnt++;
        if (ud.reccnt >= RECSYNCBUFSIZ)
        {
            dfwrite(recsync,sizeof(input_t)*ud.multimode,ud.reccnt/ud.multimode,frecfilep);
            ud.reccnt = 0;
        }
    }
}

void G_CloseDemoWrite(void)
{
    if (ud.recstat == 1)
    {
        if (ud.reccnt > 0)
        {
            dfwrite(recsync,sizeof(input_t)*ud.multimode,ud.reccnt/ud.multimode,frecfilep);

            fseek(frecfilep,SEEK_SET,0L);
            fwrite(&totalreccnt,sizeof(int),1,frecfilep);
            ud.recstat = ud.m_recstat = 0;
        }
        fclose(frecfilep);
    }
}

static int g_whichDemo = 1;

// extern int syncs[];
static int G_PlaybackDemo(void)
{
    int i,j,l;
    int foundemo = 0;
    static int in_menu = 0;

    if (ready2send) return 0;

RECHECK:

    in_menu = ud.gm&MODE_MENU;

    pub = NUMPAGES;
    pus = NUMPAGES;

    renderFlushPerms();

    if (ud.multimode < 2)
        foundemo = G_OpenDemoRead(g_whichDemo);

    if (foundemo == 0)
    {
        if (g_whichDemo > 1)
        {
            g_whichDemo = 1;
            goto RECHECK;
        }

        fadepal(0,0,0, 0,63,7);
        P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 1);    // JBF 20040308

        G_DrawBackground();
        M_DisplayMenus();

        //g_player[myconnectindex].ps->palette = palette;
        videoNextPage();
        fadepal(0,0,0, 63,0,-7);
        ud.reccnt = 0;
    }
    else
    {
        ud.recstat = 2;
        g_whichDemo++;

        if (g_whichDemo == 10)
            g_whichDemo = 1;

        if (G_EnterLevel(MODE_DEMO))
            ud.recstat = foundemo = 0;
    }

    if (foundemo == 0 || in_menu || KB_KeyWaiting() || numplayers > 1)
    {
        FX_StopAllSounds();

        ud.gm |= MODE_MENU;
    }

    ready2send = 0;
    i = 0;

    KB_FlushKeyboardQueue();

    while (ud.reccnt > 0 || foundemo == 0)
    {
        bool in_vote = (g_player[myconnectindex].gotvote == 0 && vote.starter != -1 && vote.starter != myconnectindex);

        if (foundemo)
            while (totalclock >= (lockclock+TICSPERFRAME))
            {
                if ((i == 0) || (i >= RECSYNCBUFSIZ))
                {
                    i = 0;
                    l = min(ud.reccnt,RECSYNCBUFSIZ);
                    if (kdfread(recsync,sizeof(input_t)*ud.multimode,l/ud.multimode,recfilep) != l/ud.multimode)
                    {
                        OSD_Printf(OSD_ERROR "Demo %d is corrupt.\n", g_whichDemo-1);
                        foundemo = 0;
                        ud.reccnt = 0;
                        kclose(recfilep);
                        ud.gm |= MODE_MENU;
                        goto RECHECK;
                    }
                }

                for (ALL_PLAYERS(j))
                {
                    copybufbyte(&recsync[i],&inputfifo[g_player[j].movefifoend&(MOVEFIFOSIZ-1)][j],sizeof(input_t));
                    g_player[j].movefifoend++;
                    i++;
                    ud.reccnt--;
                }
                G_DoMoveThings();
            }

        if (engineFPSLimit())
        {
            if (foundemo == 0)
            {
                videoClearScreen(0);
                G_DrawBackground();

                if (numplayers > 1) // [JM] Handle F1/F2 keys in menu.
                    G_HandleVoteKeys();
            }
            else
            {
                G_HandleLocalKeys();

                int smoothRatio = calc_smoothratio(totalclock, lockclock);
                G_DrawRooms(screenpeek, smoothRatio);
                G_DisplayRest(smoothRatio);

                if (ud.multimode > 1 && ud.gm)
                    Net_GetPackets();
            }

            // Loop back and re-check for a demo if we're just coming out of a level.
            if ((ud.gm & MODE_MENU) && (ud.gm & MODE_EOL))
                goto RECHECK;

            // Open menu if escape key is pressed.
            if (KB_KeyPressed(sc_Escape) && (ud.gm & MODE_MENU) == 0 && (ud.gm & MODE_TYPE) == 0)
            {
                KB_ClearKeyDown(sc_Escape);
                FX_StopAllSounds();

                ud.gm |= MODE_MENU;
                ChangeToMenu(0);
                S_MenuSound();
            }

            if (in_vote)
                G_ShowVote();

            if (ud.gm & MODE_TYPE)
            {
                Net_EnterMessage();

                if ((ud.gm & MODE_TYPE) != MODE_TYPE)
                    ud.gm |= MODE_MENU;
            }
            else
            {
                if (ud.recstat != 2 && !in_vote)
                {
                    M_DisplayMenus();
                    if(numplayers > 1)
                        gametextpalbits(160, 184, "WAITING FOR MAP CHANGE OR VOTE", 0, 7, 2 + 8 + 16);
                }

                CONTROL_ProcessBinds(); // [JM] Not sure why I need this, but I do with engineFPSLimit. The fuck?
                if (ud.multimode > 1 && g_currentMenu != 20003 && g_currentMenu != 20005 && g_currentMenu != 210)
                {
                    ControlInfo noshareinfo;
                    CONTROL_GetInput(&noshareinfo);
                    if (BUTTON(gamefunc_SendMessage))
                    {
                        KB_FlushKeyboardQueue();
                        CONTROL_ClearButton(gamefunc_SendMessage);
                        ud.gm |= MODE_TYPE;
                        typebuf[0] = 0;
                        inputloc = 0;
                    }
                }
            }

            G_PrintGameQuotes();

            if (ud.last_camsprite != ud.camerasprite)
            {
                ud.last_camsprite = ud.camerasprite;
                ud.camera_time = (int32_t)totalclock + (TICRATE * 2);
            }

            if (VOLUMEONE)
            {
                if (ud.show_help == 0 && (ud.gm & MODE_MENU) == 0)
                    rotatesprite((320 - 50) << 16, 9 << 16, 65536L, 0, BETAVERSION, 0, 0, 2 + 8 + 16 + 128, 0, 0, xdim - 1, ydim - 1);
            }

            videoNextPage();
        }

        gameHandleEvents();

        if (ud.gm==MODE_END || ud.gm==MODE_GAME)
        {
            if (foundemo)
                kclose(recfilep);
            return 0;
        }
    }
    ud.multimode = numplayers;  // fixes 2 infinite loops after watching demo
    kclose(recfilep);

#if 0
    {
        unsigned int crcv;
        // sync checker
        +       initcrc32table();
        crc32init(&crcv);
        crc32block(&crcv, (unsigned char *)wall, sizeof(wall));
        crc32block(&crcv, (unsigned char *)sector, sizeof(sector));
        crc32block(&crcv, (unsigned char *)sprite, sizeof(sprite));
        crc32finish(&crcv);
        LOG_F(INFO, "Checksum = %08X",crcv);
    }
#endif

    if (ud.gm&MODE_MENU) goto RECHECK;
    return 1;
}

static int G_MoveLoop()
{
    int i;

    if (numplayers > 1)
        Net_DoPrediction(PREDICTSTATE_PROCESS);
    else
        bufferjitter = 0;

    while ((g_player[myconnectindex].movefifoend-movefifoplc) > bufferjitter)
    {
        if (numplayers > 1)
        {
            TRAVERSE_CONNECT(i)
                if ((g_player[i].movefifoend <= movefifoplc) && !g_gameQuit)
                    return 0; // Player hasn't caught up yet, don't process the next tic.
        }

        int32_t doMoveReturn = G_DoMoveThings();
        Net_GetSyncStat();

        if (doMoveReturn)
            return 1;
    }

    return 0;
}

static int G_DoMoveThings(void)
{
    int i, j;
//    char ch;

    TRAVERSE_CONNECT(i)
    {
        if (TEST_SYNC_KEY(g_player[i].inputBits->bits, SK_MULTIFLAG))
        {
            multiflag = 2;
            multiwhat = (g_player[i].inputBits->bits >> (SK_MULTIFLAG + 1)) & 1;
            multipos = (unsigned)(g_player[i].inputBits->bits >> (SK_MULTIFLAG + 2)) & 15;
            multiwho = i;

            if (multiwhat)
            {
                G_SavePlayer(multipos);
                multiflag = 0;

                if (multiwho != myconnectindex)
                {
                    Bsprintf(apStrings[QUOTE_RESERVED4], "%s^00 SAVED A MULTIPLAYER GAME", &g_player[multiwho].user_name[0]);
                    P_DoQuote(QUOTE_RESERVED4, g_player[myconnectindex].ps);
                }
                else
                {
                    Bstrcpy(apStrings[QUOTE_RESERVED4], "MULTIPLAYER GAME SAVED");
                    P_DoQuote(QUOTE_RESERVED4, g_player[myconnectindex].ps);
                }
                break;
            }
            else
            {
                //            waitforeverybody();

                j = G_LoadPlayer(multipos);

                multiflag = 0;

                if (j == 0)
                {
                    if (multiwho != myconnectindex)
                    {
                        Bsprintf(apStrings[QUOTE_RESERVED4], "%s^00 LOADED A MULTIPLAYER GAME", &g_player[multiwho].user_name[0]);
                        P_DoQuote(QUOTE_RESERVED4, g_player[myconnectindex].ps);
                    }
                    else
                    {
                        Bstrcpy(apStrings[QUOTE_RESERVED4], "MULTIPLAYER GAME LOADED");
                        P_DoQuote(QUOTE_RESERVED4, g_player[myconnectindex].ps);
                    }
                    return 1;
                }
            }
        }
    }

    ud.camerasprite = -1;
    lockclock += TICSPERFRAME;
    
    if (g_RTSPlaying > 0) g_RTSPlaying--;

    // TO-DO: Move this somewhere that it can tick down while in menus. Perhaps G_MoveLoop.
    for (i=0;i<MAXUSERQUOTES;i++)
        if (user_quote_time[i])
        {
            user_quote_time[i]--;
            if (user_quote_time[i] > ud.msgdisptime)
                user_quote_time[i] = ud.msgdisptime;
            if (!user_quote_time[i]) pub = NUMPAGES;
        }

    if (g_showShareware > 0)
    {
        g_showShareware--;
        if (g_showShareware == 0)
        {
            pus = NUMPAGES;
            pub = NUMPAGES;
        }
    }

    // Copy input FIFO bits into player's own inputBits struct, and advance our place in the FIFO.
    for (ALL_PLAYERS(i))
    {
        auto newInputs = &inputfifo[INPUTFIFO_CURTICK][i];
        if (g_player[i].isBot)
            DukeBot_GetInput(i, newInputs);

        *g_player[i].inputBits = *newInputs;
        Net_CheckPlayerQuit(i);
    }

    movefifoplc++;

    G_UpdateInterpolations();

    everyothertime++;
    g_moveThingsCount++;

    if (ud.recstat == 1) G_DemoRecord();

    // G_DoWorldSim() -------------------------
    if (ud.pause_on == 0)
    {
        // Moved to pause check since this can desync the game if someone pauses during a quake
        if (g_earthquakeTime > 0)
            g_earthquakeTime--;

        //g_random.global += randomseed;
        g_globalRandom = seed_krand(&g_random.global);
        A_MoveDummyPlayers();//ST 13
    }

    for (ALL_PLAYERS(i))
    { 
        // P_HandleTeamChange
        if (g_player[i].inputBits->extbits&(EXTBIT_CHANGETEAM))
        {
            g_player[i].ps->team = g_player[i].pteam;
            if (Gametypes[ud.coop].flags & GAMETYPE_TDM)
            {
                actor[g_player[i].ps->i].htpicnum = APLAYERTOP;
                P_QuickKill(g_player[i].ps);
            }
        }
        // ---

        // P_HandlePlayerColor
        if (Gametypes[ud.coop].flags & GAMETYPE_TDM)
            g_player[i].ps->palookup = teams[g_player[i].ps->team].palnum;

        if (sprite[g_player[i].ps->i].pal != 1) // Frozen
            sprite[g_player[i].ps->i].pal = g_player[i].ps->palookup;
        // ---

        P_HandleSharedKeys(i);
        if (ud.pause_on == 0)
        {
            P_ProcessInput(i);
            P_CheckSectors(i);
        }
    }

    if (ud.pause_on == 0)
        G_MoveWorld();
    // G_DoWorldSim() -------------------------

    Net_CorrectPrediction();

    if ((everyothertime&1) == 0)
    {
        G_AnimateWalls();
        A_MoveCyclers();
    }

#if 0//POLYMER
    //polymer invalidate
    updatesectors = 1;
#endif

    return 0;
}

static void G_DrawCameraText(short i)
{
    char flipbits;
    int x , y;

    if (!T1(i))
    {
        rotatesprite(24<<16,33<<16,65536L,0,CAMCORNER,0,0,2,windowxy1.x,windowxy1.y,windowxy2.x,windowxy2.y);
        rotatesprite((320-26)<<16,34<<16,65536L,0,CAMCORNER+1,0,0,2,windowxy1.x,windowxy1.y,windowxy2.x,windowxy2.y);
        rotatesprite(22<<16,163<<16,65536L,512,CAMCORNER+1,0,0,2+4,windowxy1.x,windowxy1.y,windowxy2.x,windowxy2.y);
        rotatesprite((310-10)<<16,163<<16,65536L,512,CAMCORNER+1,0,0,2,windowxy1.x,windowxy1.y,windowxy2.x,windowxy2.y);
        if ((int32_t)totalclock&16)
            rotatesprite(46<<16,32<<16,65536L,0,CAMLIGHT,0,0,2,windowxy1.x,windowxy1.y,windowxy2.x,windowxy2.y);
    }
    else
    {
        flipbits = ((int32_t)totalclock<<1)&48;
        for (x=0;x<394;x+=64)
            for (y=0;y<200;y+=64)
                rotatesprite(x<<16,y<<16,65536L,0,STATIC,0,0,2+flipbits,windowxy1.x,windowxy1.y,windowxy2.x,windowxy2.y);
    }
}

#if 0
void vglass(int x,int y,short a,short wn,short n)
{
    int z, zincs;
    short sect;

    sect = wall[wn].nextsector;
    if (sect == -1) return;
    zincs = (sector[sect].floorz-sector[sect].ceilingz) / n;

    for (z = sector[sect].ceilingz;z < sector[sect].floorz; z += zincs)
        A_InsertSprite(sect,x,y,z-(krand()&8191),GLASSPIECES+(z&(krand()%3)),-32,36,36,a+128-(krand()&255),16+(krand()&31),0,-1,5);
}
#endif

void A_SpawnWallGlass(int i,int wallnum,int n)
{
    int j, xv, yv, z, x1, y1;
    short sect;
    int a;

    sect = -1;

    if (wallnum < 0)
    {
        for (j=n-1; j >= 0 ;j--)
        {
            a = SA(i)-256+(krand()&511)+1024;
            A_InsertSprite(SECT(i),SX(i),SY(i),SZ(i),GLASSPIECES+(j%3),-32,36,36,a,32+(krand()&63),1024-(krand()&1023),i,5);
        }
        return;
    }

    j = n+1;

    x1 = wall[wallnum].x;
    y1 = wall[wallnum].y;

    xv = wall[wall[wallnum].point2].x-x1;
    yv = wall[wall[wallnum].point2].y-y1;

    x1 -= ksgn(yv);
    y1 += ksgn(xv);

    xv /= j;
    yv /= j;

    for (j=n;j>0;j--)
    {
        x1 += xv;
        y1 += yv;

        updatesector(x1,y1,&sect);
        if (sect >= 0)
        {
            z = sector[sect].floorz-(krand()&(klabs(sector[sect].ceilingz-sector[sect].floorz)));
            if (z < -(32<<8) || z > (32<<8))
                z = SZ(i)-(32<<8)+(krand()&((64<<8)-1));
            a = SA(i)-1024;
            A_InsertSprite(SECT(i),x1,y1,z,GLASSPIECES+(j%3),-32,36,36,a,32+(krand()&63),-(krand()&1023),i,5);
        }
    }
}

void A_SpawnGlass(int i,int n)
{
    int j, k, a, z;

    for (j=n;j>0;j--)
    {
        a = krand()&2047;
        z = SZ(i)-((krand()&16)<<8);
        k = A_InsertSprite(SECT(i),SX(i),SY(i),z,GLASSPIECES+(j%3),krand()&15,36,36,a,32+(krand()&63),-512-(krand()&2047),i,5);
        sprite[k].pal = sprite[i].pal;
    }
}

void A_SpawnCeilingGlass(int i,int sectnum,int n)
{
    int j, xv, yv, z, x1, y1, a,s;
    int startwall = sector[sectnum].wallptr;
    int endwall = startwall+sector[sectnum].wallnum;

    for (s=startwall;s<(endwall-1);s++)
    {
        x1 = wall[s].x;
        y1 = wall[s].y;

        xv = (wall[s+1].x-x1)/(n+1);
        yv = (wall[s+1].y-y1)/(n+1);

        for (j=n;j>0;j--)
        {
            x1 += xv;
            y1 += yv;
            a = krand()&2047;
            z = sector[sectnum].ceilingz+((krand()&15)<<8);
            A_InsertSprite(sectnum,x1,y1,z,GLASSPIECES+(j%3),-32,36,36,a,(krand()&31),0,i,5);
        }
    }
}

void A_SpawnRandomGlass(int i,int wallnum,int n)
{
    int j, xv, yv, z, x1, y1;
    short sect = -1;
    int a, k;

    if (wallnum < 0)
    {
        for (j=n-1; j >= 0 ;j--)
        {
            a = krand()&2047;
            k = A_InsertSprite(SECT(i),SX(i),SY(i),SZ(i)-(krand()&(63<<8)),GLASSPIECES+(j%3),-32,36,36,a,32+(krand()&63),1024-(krand()&2047),i,5);
            sprite[k].pal = krand()&15;
        }
        return;
    }

    j = n+1;
    x1 = wall[wallnum].x;
    y1 = wall[wallnum].y;

    xv = (wall[wall[wallnum].point2].x-wall[wallnum].x)/j;
    yv = (wall[wall[wallnum].point2].y-wall[wallnum].y)/j;

    for (j=n;j>0;j--)
    {
        x1 += xv;
        y1 += yv;

        updatesector(x1,y1,&sect);
        z = sector[sect].floorz-(krand()&(klabs(sector[sect].ceilingz-sector[sect].floorz)));
        if (z < -(32<<8) || z > (32<<8))
            z = SZ(i)-(32<<8)+(krand()&((64<<8)-1));
        a = SA(i)-1024;
        k = A_InsertSprite(SECT(i),x1,y1,z,GLASSPIECES+(j%3),-32,36,36,a,32+(krand()&63),-(krand()&2047),i,5);
        sprite[k].pal = krand()&7;
    }
}

static void G_SetupGameButtons(void)
{
    CONTROL_DefineFlag(gamefunc_Move_Forward,false);
    CONTROL_DefineFlag(gamefunc_Move_Backward,false);
    CONTROL_DefineFlag(gamefunc_Turn_Left,false);
    CONTROL_DefineFlag(gamefunc_Turn_Right,false);
    CONTROL_DefineFlag(gamefunc_Strafe,false);
    CONTROL_DefineFlag(gamefunc_Fire,false);
    CONTROL_DefineFlag(gamefunc_Open,false);
    CONTROL_DefineFlag(gamefunc_Run,false);
    CONTROL_DefineFlag(gamefunc_AutoRun,false);
    CONTROL_DefineFlag(gamefunc_Jump,false);
    CONTROL_DefineFlag(gamefunc_Crouch,false);
    CONTROL_DefineFlag(gamefunc_Look_Up,false);
    CONTROL_DefineFlag(gamefunc_Look_Down,false);
    CONTROL_DefineFlag(gamefunc_Look_Left,false);
    CONTROL_DefineFlag(gamefunc_Look_Right,false);
    CONTROL_DefineFlag(gamefunc_Strafe_Left,false);
    CONTROL_DefineFlag(gamefunc_Strafe_Right,false);
    CONTROL_DefineFlag(gamefunc_Aim_Up,false);
    CONTROL_DefineFlag(gamefunc_Aim_Down,false);
    CONTROL_DefineFlag(gamefunc_Weapon_1,false);
    CONTROL_DefineFlag(gamefunc_Weapon_2,false);
    CONTROL_DefineFlag(gamefunc_Weapon_3,false);
    CONTROL_DefineFlag(gamefunc_Weapon_4,false);
    CONTROL_DefineFlag(gamefunc_Weapon_5,false);
    CONTROL_DefineFlag(gamefunc_Weapon_6,false);
    CONTROL_DefineFlag(gamefunc_Weapon_7,false);
    CONTROL_DefineFlag(gamefunc_Weapon_8,false);
    CONTROL_DefineFlag(gamefunc_Weapon_9,false);
    CONTROL_DefineFlag(gamefunc_Weapon_10,false);
    CONTROL_DefineFlag(gamefunc_Inventory,false);
    CONTROL_DefineFlag(gamefunc_Inventory_Left,false);
    CONTROL_DefineFlag(gamefunc_Inventory_Right,false);
    CONTROL_DefineFlag(gamefunc_Holo_Duke,false);
    CONTROL_DefineFlag(gamefunc_Jetpack,false);
    CONTROL_DefineFlag(gamefunc_NightVision,false);
    CONTROL_DefineFlag(gamefunc_MedKit,false);
    CONTROL_DefineFlag(gamefunc_TurnAround,false);
    CONTROL_DefineFlag(gamefunc_SendMessage,false);
    CONTROL_DefineFlag(gamefunc_Map,false);
    CONTROL_DefineFlag(gamefunc_Shrink_Screen,false);
    CONTROL_DefineFlag(gamefunc_Enlarge_Screen,false);
    CONTROL_DefineFlag(gamefunc_Center_View,false);
    CONTROL_DefineFlag(gamefunc_Holster_Weapon,false);
    CONTROL_DefineFlag(gamefunc_Show_Opponents_Weapon,false);
    CONTROL_DefineFlag(gamefunc_Map_Follow_Mode,false);
    CONTROL_DefineFlag(gamefunc_See_Coop_View,false);
    CONTROL_DefineFlag(gamefunc_Mouse_Aiming,false);
    CONTROL_DefineFlag(gamefunc_Toggle_Crosshair,false);
    CONTROL_DefineFlag(gamefunc_Steroids,false);
    CONTROL_DefineFlag(gamefunc_Quick_Kick,false);
    CONTROL_DefineFlag(gamefunc_Next_Weapon,false);
    CONTROL_DefineFlag(gamefunc_Previous_Weapon,false);

    CONTROL_DefineFlag(gamefunc_Alt_Fire, FALSE);
    CONTROL_DefineFlag(gamefunc_Last_Weapon, FALSE);
    CONTROL_DefineFlag(gamefunc_Quick_Save, FALSE);
    CONTROL_DefineFlag(gamefunc_Quick_Load, FALSE);
    CONTROL_DefineFlag(gamefunc_Alt_Weapon, FALSE);
    CONTROL_DefineFlag(gamefunc_Third_Person_View, FALSE);
    CONTROL_DefineFlag(gamefunc_Toggle_Crouch, FALSE);
}

// [JM] Needed for build linkage
void M32RunScript(const char* s) { UNREFERENCED_PARAMETER(s); }
//void G_Polymer_UnInit(void) { }

/*
===================
=
= GetTime
=
===================
*/

int GetTime(void)
{
    return (int32_t)totalclock;
}
