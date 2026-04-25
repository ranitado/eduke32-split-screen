#ifndef GAMEEXEC_H
#define GAMEEXEC_H

#include "build.h"
#include "gamedef.h"  // vmstate_t
#include "sector.h"  // mapstate_t

enum vmflags_t
{
    VM_RETURN       = 0x00000001,
    VM_KILL         = 0x00000002,
    VM_NOEXECUTE    = 0x00000004,
};

void VM_ScriptInfo(intptr_t const* const ptr, int const range);
void VM_UpdateAnim(int const spriteNum, int32_t* const pData);

void X_OnEvent(int iEventID, int sActor, int sPlayer, int lDist);
int X_OnEventWithReturn(int iEventID, int sActor, int sPlayer, int nReturn);

int A_GetFurthestAngle(int const spriteNum, int const angDiv);

void A_GetZLimits(int iActor);
void A_Fall(int iActor);

static FORCE_INLINE int VM_HaveEvent(int const nEventID)
{
    return !!apScriptEvents[nEventID];
}

void VM_DrawTileGeneric(int32_t x, int32_t y, int32_t zoom, int32_t tilenum,
    int32_t shade, int32_t orientation, int32_t p);

void VM_DrawTile(int32_t x, int32_t y, int32_t tilenum, int32_t shade, int32_t orientation);
static inline void VM_DrawTilePal(int32_t x, int32_t y, int32_t tilenum, int32_t shade, int32_t orientation, int32_t p)
{
    VM_DrawTileGeneric(x, y, 65536, tilenum, shade, orientation, p);
}
static inline void VM_DrawTilePalSmall(int32_t x, int32_t y, int32_t tilenum, int32_t shade, int32_t orientation, int32_t p)
{
    VM_DrawTileGeneric(x, y, 32768, tilenum, shade, orientation, p);
}
void VM_DrawTileSmall(int32_t x, int32_t y, int32_t tilenum, int32_t shade, int32_t orientation);

#define CON_ERRPRINTF(Text, ...) do { \
    vm.flags |= VM_RETURN; \
    LOG_F(ERROR, "%s:%d: %s: " Text, VM_FILENAME(insptr), VM_DECODE_LINE_NUMBER(g_tw), VM_GetKeywordForID(VM_DECODE_INST(g_tw)), ## __VA_ARGS__); \
} while (0)

#define CON_CRITICALERRPRINTF(Text, ...) do { \
    vm.flags |= VM_RETURN; \
    LOG_F(ERROR, "%s:%d: %s: " Text, VM_FILENAME(insptr), VM_DECODE_LINE_NUMBER(g_tw), VM_GetKeywordForID(VM_DECODE_INST(g_tw)), ## __VA_ARGS__); \
    wm_msgbox(APPNAME, "%s:%d: %s: " Text, VM_FILENAME(insptr), VM_DECODE_LINE_NUMBER(g_tw), VM_GetKeywordForID(VM_DECODE_INST(g_tw)), ## __VA_ARGS__); \
} while (0)

#endif // GAMEEXEC_H
