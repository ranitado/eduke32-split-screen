#include "build.h"
#include "utils.h"

// For storing pointers in files.
//  back_p==0: ptr -> "small int"
//  back_p==1: "small int" -> ptr
//
//  mode: see enum in savegame.h
void G_Util_PtrToIdx(void* ptr, int32_t const count, const void* base, int32_t const mode)
{
    intptr_t* iptr = (intptr_t*)ptr;
    intptr_t const ibase = (intptr_t)base;
    int32_t const onlynon0_p = mode & P2I_ONLYNON0_BIT;

    // TODO: convert to proper offsets/indices for (a step towards) cross-
    //       compatibility between 32- and 64-bit systems in the netplay.
    //       REMEMBER to bump BYTEVERSION then.

    // WARNING: C std doesn't say that bit pattern of NULL is necessarily 0!
    if ((mode & P2I_BACK_BIT) == 0)
    {
        for (bssize_t i = 0; i < count; i++)
            if (!onlynon0_p || iptr[i])
                iptr[i] -= ibase;
    }
    else
    {
        for (bssize_t i = 0; i < count; i++)
            if (!onlynon0_p || iptr[i])
                iptr[i] += ibase;
    }
}

void G_Util_PtrToIdx2(void* ptr, int32_t const count, size_t const stride, const void* base, int32_t const mode)
{
    uint8_t* iptr = (uint8_t*)ptr;
    intptr_t const ibase = (intptr_t)base;
    int32_t const onlynon0_p = mode & P2I_ONLYNON0_BIT;

    if ((mode & P2I_BACK_BIT) == 0)
    {
        for (bssize_t i = 0; i < count; ++i)
        {
            if (!onlynon0_p || *(intptr_t*)iptr)
                *(intptr_t*)iptr -= ibase;

            iptr += stride;
        }
    }
    else
    {
        for (bssize_t i = 0; i < count; ++i)
        {
            if (!onlynon0_p || *(intptr_t*)iptr)
                *(intptr_t*)iptr += ibase;

            iptr += stride;
        }
    }
}