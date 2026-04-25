#ifndef GAMEVARS_H
#define GAMEVARS_H

#include "gamedef.h"
#include "vfs.h"

// Alignments for per-player and per-actor variables.
#define PLAYER_VAR_ALIGNMENT (sizeof(intptr_t))
#define ACTOR_VAR_ALIGNMENT 16

#define GV_FLAG_CONSTANT	(MAXGAMEVARS)
#define GV_FLAG_NEGATIVE	(MAXGAMEVARS<<1)
#define GV_FLAG_ARRAY		(MAXGAMEVARS<<2)
#define GV_FLAG_STRUCT		(MAXGAMEVARS<<3)

#define MAXGAMEVARS 2048 // must be a power of two
#define MAXVARLABEL 26

enum GamevarFlags_t {
    GAMEVAR_PERPLAYER   = 0x00000001, // per-player variable
    GAMEVAR_PERACTOR    = 0x00000002, // per-actor variable
    GAMEVAR_USER_MASK   = (0x00000001 | 0x00000002),
    GAMEVAR_RESET       = 0x00000008, // marks var for to default
    GAMEVAR_DEFAULT     = 0x00000100, // allow override
    GAMEVAR_SECRET      = 0x00000200, // don't dump...
    GAMEVAR_NODEFAULT   = 0x00000400, // don't reset on actor spawn
    GAMEVAR_SYSTEM      = 0x00000800, // cannot change mode flags...(only default value)
    GAMEVAR_READONLY    = 0x00001000, // values are read-only (no setvar allowed)
    GAMEVAR_INT32PTR    = 0x00002000, // pValues is a pointer to a 32-bit integer
    GAMEVAR_SYNCCHECK   = 0x00004000, // throw warnings during compile if used in local event
    GAMEVAR_INT16PTR    = 0x00008000, // pValues is a pointer to a 16-bit integer
    GAMEVAR_INT8PTR     = 0x00010000, // pValues is a pointer to an 8-bit integer
    GAMEVAR_NORESET     = 0x00020000, // var values are not reset when restoring map state
    GAMEVAR_SPECIAL     = 0x00040000, // flag for structure member shortcut vars
    GAMEVAR_NOMULTI     = 0x00080000, // don't attach to multiplayer packets
    GAMEVAR_Q16PTR      = 0x00100000,

    GAMEVAR_RAWQ16PTR   = GAMEVAR_Q16PTR | GAMEVAR_SPECIAL,
    GAMEVAR_PTR_MASK    = GAMEVAR_INT32PTR | GAMEVAR_INT16PTR | GAMEVAR_INT8PTR | GAMEVAR_Q16PTR | GAMEVAR_RAWQ16PTR,
};

#define ARRAY_ALIGNMENT 16
#define MAXGAMEARRAYS (MAXGAMEVARS>>2) // must be strictly smaller than MAXGAMEVARS
#define MAXARRAYLABEL MAXVARLABEL
enum GamearrayFlags_t
{
    GAMEARRAY_RESET         = 0x00000008,
    GAMEARRAY_RESTORE       = 0x00000010,
    GAMEARRAY_VARSIZE       = 0x00000020,
    GAMEARRAY_STRIDE2       = 0x00000100,
    GAMEARRAY_ALLOCATED     = 0x00000200,  // memory allocated for user array
    GAMEARRAY_SYSTEM        = 0x00000800,
    GAMEARRAY_READONLY      = 0x00001000,
    GAMEARRAY_INT16         = 0x00004000,
    GAMEARRAY_INT8          = 0x00008000,
    GAMEARRAY_UNSIGNED      = 0x00010000,
    GAMEARRAY_UINT16        = GAMEARRAY_INT16 | GAMEARRAY_UNSIGNED,
    GAMEARRAY_UINT8         = GAMEARRAY_INT8 | GAMEARRAY_UNSIGNED,
    GAMEARRAY_BITMAP        = 0x00100000,
    GAMEARRAY_WARN          = 0x00200000,

    GAMEARRAY_SIZE_MASK     = GAMEARRAY_INT8 | GAMEARRAY_INT16 | GAMEARRAY_BITMAP,
    GAMEARRAY_STORAGE_MASK  = GAMEARRAY_INT8 | GAMEARRAY_INT16 | GAMEARRAY_BITMAP | GAMEARRAY_STRIDE2,
    GAMEARRAY_TYPE_MASK     = GAMEARRAY_UNSIGNED | GAMEARRAY_INT8 | GAMEARRAY_INT16 | GAMEARRAY_BITMAP,
};

// Structs
#pragma pack(push,1)
typedef struct {
    union {
        intptr_t lValue;
        intptr_t* pValues;     // array of values when 'per-player', or 'per-actor'
    };
    intptr_t defaultValue;
    unsigned int flags;
    char* szLabel;
} gamevar_t;

typedef struct
{
    char* szLabel;
    intptr_t* pValues;  // array of values
    intptr_t  size;
    uintptr_t flags;
} gamearray_t;
#pragma pack(pop)

extern gamevar_t aGameVars[MAXGAMEVARS];
extern int g_gameVarCount;
extern gamearray_t aGameArrays[MAXGAMEARRAYS];
extern int g_gameArrayCount;

// Function Declarations
int __fastcall Gv_GetVar(int id, int iActor, int iPlayer);
void __fastcall Gv_SetVar(int id, int lValue, int iActor, int iPlayer);
int __fastcall Gv_GetVarX(int id);
void __fastcall Gv_SetVarX(int id, int lValue);

void Gv_Clear(void);

void Gv_NewVar(const char* pszLabel, intptr_t lValue, uint32_t dwFlags);
void Gv_NewArray(const char* pszLabel, void* arrayptr, intptr_t asize, uint32_t flags);

int Gv_GetVarByLabel(const char* szGameLabel, int lDefault, int iActor, int iPlayer);
void Gv_DumpValues(void);
void Gv_ResetSystemDefaults(void);
void Gv_InitWeaponPointers(void);
void Gv_RefreshPointers(void);
void Gv_Init(void);
void Gv_WriteSave(FILE* fil);
int Gv_ReadSave(int fil);

size_t __fastcall Gv_GetArrayAllocSizeForCount(int const arrayIdx, size_t const count);
size_t __fastcall Gv_GetArrayCountForAllocSize(int const arrayIdx, size_t const filelength);
unsigned __fastcall Gv_GetArrayElementSize(int const arrayIdx);

// Inline functions
static FORCE_INLINE size_t Gv_GetArrayAllocSize(int const arrayIdx) { return Gv_GetArrayAllocSizeForCount(arrayIdx, aGameArrays[arrayIdx].size); }

static FORCE_INLINE int __fastcall Gv_GetArrayValue(int const id, int index)
{
    if (aGameArrays[id].flags & GAMEARRAY_STRIDE2)
        index <<= 1;

    switch (aGameArrays[id].flags & GAMEARRAY_TYPE_MASK)
    {
        default: return (aGameArrays[id].pValues)[index];
        case GAMEARRAY_INT16:  return   ((int16_t*)aGameArrays[id].pValues)[index];
        case GAMEARRAY_INT8:   return   ((int8_t*)aGameArrays[id].pValues)[index];
        case GAMEARRAY_UINT16: return   ((uint16_t*)aGameArrays[id].pValues)[index];
        case GAMEARRAY_UINT8:  return   ((uint8_t*)aGameArrays[id].pValues)[index];
        case GAMEARRAY_BITMAP: return !!(((uint8_t*)aGameArrays[id].pValues)[index >> 3] & pow2char[index & 7]);
    }
}

static FORCE_INLINE void __fastcall Gv_DivVar(int const id, int32_t const d)
{
    using namespace libdivide;

    auto& var = aGameVars[id];
    auto dptr = &bfdivtable32[((uint32_t)d < DIVTABLESIZE) * d];

    if ((d != lastd_s32_b) & !((uint32_t)d < DIVTABLESIZE))
        bfdivtable32[0] = libdivide_s32_branchfree_gen((lastd_s32_b = d));

    auto iptr = &var.lValue;

    switch (var.flags & (GAMEVAR_USER_MASK | GAMEVAR_PTR_MASK))
    {
        case GAMEVAR_PERACTOR: iptr = &var.pValues[vm.spriteNum & (MAXSPRITES - 1)]; goto jmp;
        case GAMEVAR_PERPLAYER: iptr = &var.pValues[vm.playerNum & (MAXPLAYERS - 1)]; fallthrough__;
        default: jmp: *iptr = libdivide_s32_branchfree_do(*iptr, dptr); break;

        case GAMEVAR_RAWQ16PTR:
        case GAMEVAR_INT32PTR:
        {
            auto& value = *(int32_t*)var.pValues;
            value = libdivide_s32_branchfree_do(value, dptr);
            break;
        }
        case GAMEVAR_INT16PTR:
        {
            auto& value = *(int16_t*)var.pValues;
            value = (int16_t)libdivide_s32_branchfree_do(value, dptr);
            break;
        }
        case GAMEVAR_Q16PTR:
        {
            auto& value = *(fix16_t*)var.pValues;
            value = fix16_div(value, fix16_from_int(d));
            break;
        }
    }
}

#define VM_GAMEVAR_OPERATOR(func, operator)                                                            \
    static FORCE_INLINE ATTRIBUTE((flatten)) void __fastcall func(int const id, int32_t const operand) \
    {                                                                                                  \
        auto &var = aGameVars[id];                                                                     \
                                                                                                       \
        switch (var.flags & (GAMEVAR_USER_MASK | GAMEVAR_PTR_MASK))                                    \
        {                                                                                              \
            default:                                                                                   \
                var.lValue operator operand;                                                           \
                break;                                                                                 \
            case GAMEVAR_PERPLAYER:                                                                    \
                var.pValues[vm.playerNum & (MAXPLAYERS-1)] operator operand;                           \
                break;                                                                                 \
            case GAMEVAR_PERACTOR:                                                                     \
                var.pValues[vm.spriteNum & (MAXSPRITES-1)] operator operand;                           \
                break;                                                                                 \
            case GAMEVAR_RAWQ16PTR:                                                                    \
            case GAMEVAR_INT32PTR: *(int32_t *)var.pValues operator(int32_t) operand; break;           \
            case GAMEVAR_INT16PTR: *(int16_t *)var.pValues operator(int16_t) operand; break;           \
            case GAMEVAR_Q16PTR:                                                                       \
            {                                                                                          \
                Fix16 *pfix = (Fix16 *)var.lValue;                                                     \
                *pfix  operator fix16_from_int(operand);                                               \
                break;                                                                                 \
            }                                                                                          \
        }                                                                                              \
    }

VM_GAMEVAR_OPERATOR(Gv_AddVar, +=)
VM_GAMEVAR_OPERATOR(Gv_SubVar, -=)
VM_GAMEVAR_OPERATOR(Gv_MulVar, *=)
VM_GAMEVAR_OPERATOR(Gv_ModVar, %=)
VM_GAMEVAR_OPERATOR(Gv_AndVar, &=)
VM_GAMEVAR_OPERATOR(Gv_XorVar, ^=)
VM_GAMEVAR_OPERATOR(Gv_OrVar, |=)
VM_GAMEVAR_OPERATOR(Gv_ShiftVarL, <<=)
VM_GAMEVAR_OPERATOR(Gv_ShiftVarR, >>=)

#undef VM_GAMEVAR_OPERATOR

#endif // GAMEVARS_H