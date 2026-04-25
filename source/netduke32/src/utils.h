#ifndef UTILS_H
#define UTILS_H

enum
{
    P2I_BACK_BIT = 1,
    P2I_ONLYNON0_BIT = 2,

    P2I_FWD = 0,
    P2I_BACK = 1,

    P2I_FWD_NON0 = 0 + 2,
    P2I_BACK_NON0 = 1 + 2,
};
void G_Util_PtrToIdx(void* ptr, int32_t count, const void* base, int32_t mode);
void G_Util_PtrToIdx2(void* ptr, int32_t count, size_t stride, const void* base, int32_t mode);

#endif // UTILS_H