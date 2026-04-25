#pragma once

#ifndef PROJECTILES_H
#define PROJECTILES_H

#define SHOOT_HARDCODED_ZVEL INT32_MIN

// Structs
typedef struct {
    int32_t workslike, cstat; // 8b
    int32_t hitradius, range, flashcolor; // 12b
    int16_t spawns, sound, isound, vel; // 8b
    int16_t decal, trail, tnum, drop; // 8b
    int16_t offset, bounces, bsound; // 6b
    int16_t toffset; // 2b
    int16_t extra, extra_rand; // 4b
    int8_t sxrepeat, syrepeat, txrepeat, tyrepeat; // 4b
    int8_t shade, xrepeat, yrepeat, pal; // 4b
    int8_t movecnt; // 1b
    uint8_t clipdist; // 1b
    int8_t filler[2]; // 2b
    int32_t userdata; // 4b
} projectile_t;
typedef projectile_t defaultprojectile_t;
extern defaultprojectile_t DefaultProjectile;

// Enums
enum ProjectileFlags_t {
    PROJECTILE_HITSCAN              = 0x00000001,
    PROJECTILE_RPG                  = 0x00000002,
    PROJECTILE_BOUNCESOFFWALLS      = 0x00000004,
    PROJECTILE_BOUNCESOFFMIRRORS    = 0x00000008,
    PROJECTILE_KNEE                 = 0x00000010,
    PROJECTILE_WATERBUBBLES         = 0x00000020,
    PROJECTILE_TIMED                = 0x00000040,
    PROJECTILE_BOUNCESOFFSPRITES    = 0x00000080,
    PROJECTILE_SPIT                 = 0x00000100,
    PROJECTILE_COOLEXPLOSION1       = 0x00000200,
    PROJECTILE_BLOOD                = 0x00000400,
    PROJECTILE_LOSESVELOCITY        = 0x00000800,
    PROJECTILE_NOAIM                = 0x00001000,
    PROJECTILE_RANDDECALSIZE        = 0x00002000,
    PROJECTILE_EXPLODEONTIMER       = 0x00004000,
    PROJECTILE_RPG_IMPACT           = 0x00008000,
    PROJECTILE_RADIUS_PICNUM        = 0x00010000,
    PROJECTILE_ACCURATE_AUTOAIM     = 0x00020000,
    PROJECTILE_FORCEIMPACT          = 0x00040000,
    PROJECTILE_ACCURATE             = 0x00100000,
    PROJECTILE_NOSETOWNERSHADE      = 0x00200000,
    PROJECTILE_PENETRATE            = 0x00800000, // TODO: Implement & port to mainline
    PROJECTILE_TYPE_MASK            = PROJECTILE_HITSCAN | PROJECTILE_RPG | PROJECTILE_KNEE | PROJECTILE_BLOOD,
};

// Function Declarations
void C_AllocProjectile(int32_t tileNum);
void C_FreeProjectile(int32_t tileNum);
void C_InitProjectiles(void);
void C_DefineProjectile(int32_t tileNum, int32_t labelNum, int32_t val);

int Proj_GetDamage(projectile_t* pProj);
projectile_t* Proj_GetProjectile(int tile);

int A_Shoot(int spriteNum, int projecTile, int const forceZvel = SHOOT_HARDCODED_ZVEL);

#endif