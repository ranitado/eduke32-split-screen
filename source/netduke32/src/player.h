#ifndef PLAYER_H
#define PLAYER_H

#include "build.h"
#include "inv.h"
#include "sync.h"
#include "macros.h"

// Definitions
#define PHEIGHT (38<<8)
#define PMINHEIGHT (4<<8)
#define PLAYER_CROUCH_INC (11 << 8)
#define PLAYER_ZOFF_NORMAL (40 << 8)
#define PLAYER_ZOFF_WATER (34 << 8)
#define PLAYER_ZOFF_SHRUNK (12 << 8)
#define PLAYER_MINWATERZDIST (16 << 8)
#define PLAYER_STEP_HEIGHT (20 << 8)
#define PLAYER_WALLDIST 164L

#define PLAYER_SPRITE_XREPEAT 42
#define PLAYER_SPRITE_YREPEAT 36

#define TURBOTURNTIME (TICRATE/8) // 7
#define NORMALTURN   15
#define PREAMBLETURN 5
#define NORMALKEYMOVE 40
#define MAXVEL       ((NORMALKEYMOVE*2)+10)
#define MAXSVEL      ((NORMALKEYMOVE*2)+10)
#define MAXANGVEL    1024
#define MAXHORIZ     127
#define DIGITALAXISDEADZONE 1024

#define TEST_ALLOWEDCOLOR(pal) (TEST(allowedColors[(pal) >> 3], 1 << ((pal) & 7)))
#define RESET_ALLOWEDCOLOR(pal) (RESET(allowedColors[(pal) >> 3], 1 << ((pal) & 7)))
#define SET_ALLOWEDCOLOR(pal) (SET(allowedColors[(pal) >> 3], 1 << ((pal) & 7)))

// Enums
enum WeaponFlags_t {
    WEAPON_HOLSTER_CLEARS_CLIP  = 1,     // 'holstering' clears the current clip
    WEAPON_GLOWS                = 2,     // weapon 'glows' (shrinker and grower)
    WEAPON_AUTOMATIC            = 4,     // automatic fire (continues while 'fire' is held down
    WEAPON_FIREEVERYOTHER       = 8,     // during 'hold time' fire every frame
    WEAPON_FIREEVERYTHIRD       = 16,    // during 'hold time' fire every third frame
    WEAPON_RANDOMRESTART        = 32,    // restart for automatic is 'randomized' by RND 3
    WEAPON_AMMOPERSHOT          = 64,    // uses ammo for each shot (for automatic)
    WEAPON_BOMB_TRIGGER         = 128,   // weapon is the 'bomb' trigger
    WEAPON_NOVISIBLE            = 256,   // weapon use does not cause user to become 'visible'
    WEAPON_THROWIT              = 512,   // weapon 'throws' the 'shoots' item...
    WEAPON_CHECKATRELOAD        = 1024,  // check weapon availability at 'reload' time
    WEAPON_STANDSTILL           = 2048,  // player stops jumping before actual fire (like tripbomb in duke)
    WEAPON_SPAWNTYPE1           = 0,     // just spawn
    WEAPON_SPAWNTYPE2           = 4096,  // spawn like shotgun shells
    WEAPON_SPAWNTYPE3           = 8192,  // spawn like chaingun shells
    WEAPON_SEMIAUTO             = 16384, // cancel button press after each shot
    WEAPON_RELOAD_TIMING        = 32768, // special casing for pistol reload sounds
    WEAPON_RESET                = 65536  // cycle weapon back to frame 1 if fire is held, 0 if not
};

typedef struct {
    vec3_t pos, opos, vel;

    int32_t zoom, exitx, exity;
    int32_t invdisptime;
    int32_t bobposx, bobposy, pyoff, opyoff;
    int32_t last_pissed_time, truefz, truecz;
    int32_t player_par;
    int32_t bobcounter, weapon_sway;
    int32_t randomflamex, crack_time;

    fix16_t q16horiz, q16horizoff, oq16horiz, oq16horizoff;
    fix16_t q16ang, oq16ang, q16angvel;

    uint32_t interface_toggle_flag;

    palette_t pals;

    int32_t actors_killed;
    int32_t runspeed, movement_lock, team;
    int32_t max_player_health, max_shield_amount;

    int32_t scream_voice;

    int32_t loogiex[64], loogiey[64], numloogs, loogcnt;

    int16_t sbs, lowsprite, highsprite, sound_pitch;

    int16_t cursectnum, look_ang, olook_ang, last_extra, subweapon;
    int16_t wackedbyactor, frag, fraggedself, lives, deaths;
    int16_t max_ammo_amount[MAX_WEAPONS], ammo_amount[MAX_WEAPONS], inv_amount[GET_MAX];

    int16_t curr_weapon, last_weapon, last_used_weapon, wantweaponfire;
    int16_t newowner, hurt_delay, hbomb_hold_delay;
    int16_t jumping_counter, airleft;
    int16_t fta, ftq, access_wallnum, access_spritenum;
    int16_t weapon_ang;
    int16_t somethingonplayer, on_crane, i, one_parallax_sectnum;
    int16_t over_shoulder_on, random_club_frame;
    int16_t one_eighty_count, cheat_phase;
    int16_t dummyplayersprite, extra_extra8, quick_kick, last_quick_kick;
    int16_t actorsqu, timebeforeexit, customexitsound;

    int16_t weaprecs[16], weapreccnt;

    int16_t orotscrnang, rotscrnang, dead_flag, show_empty_weapon;   // JBF 20031220: added orotscrnang
    
    int16_t holoduke_on, pycount, weapon_pos, frag_ps;
    int16_t transporter_hold, last_full_weapon;

    uint8_t palette, palookup;

    uint8_t toggle_key_flag;
    uint8_t kickback_pic, rapid_fire_hold, reloading, hbomb_on;
    uint8_t knuckle_incs, knee_incs, access_incs, fist_incs, tipincs;

    uint8_t aim_mode, auto_aim, weaponswitch;

    uint8_t pflags;

    uint8_t jumping_toggle, falling_counter, hard_landing, return_to_center;
    uint8_t inven_icon, jetpack_on, heat_on, scuba_on;

    uint8_t buttonpalette, footprintpal, footprintcount, footprintshade;
    uint8_t spritebridge, on_ground, on_warping_sector;
    uint8_t lastrandomspot;

    uint8_t holster_weapon;
    uint8_t gotweapon[MAX_WEAPONS];

    uint8_t walking_snd_toggle;

    int8_t padding_[1];
} DukePlayer_t;

typedef struct {
    int16_t got_access, last_extra, curr_weapon, holoduke_on;
    int16_t inv_amount[GET_MAX];
    int16_t last_weapon, weapon_pos;
    int16_t ammo_amount[MAX_WEAPONS], frag[MAXPLAYERS];

    uint8_t kickback_pic, inven_icon, jetpack_on, heat_on;
    uint8_t gotweapon[MAX_WEAPONS];
} DukeStatus_t;

typedef struct
{
    int32_t visibility;
} PlayerDisplay_t;

typedef struct {
    DukePlayer_t* ps;
    PlayerDisplay_t display;
    input_t* inputBits;

    int32_t movefifoend, myminlag, lastSyncTick;
    int32_t pcolor, pteam, frags[MAXPLAYERS], wchoice[MAX_WEAPONS];
    int32_t fraggedby_time, fraggedby_id, fragged_time, fragged_id;
    uint32_t ping;

    char gotvote, playerreadyflag, connected, isBot;
    char user_name[32];
} playerdata_t;
extern playerdata_t g_player[MAXPLAYERS];

// Externs
#define PWEAPON(Player, Weapon, Wmember) (aplWeapon ## Wmember [Weapon][Player])
extern intptr_t* aplWeaponClip[MAX_WEAPONS];            // number of items in clip
extern intptr_t* aplWeaponReload[MAX_WEAPONS];          // delay to reload (include fire)
extern intptr_t* aplWeaponFireDelay[MAX_WEAPONS];       // delay to fire
extern intptr_t* aplWeaponHoldDelay[MAX_WEAPONS];       // delay after release fire button to fire (0 for none)
extern intptr_t* aplWeaponTotalTime[MAX_WEAPONS];       // The total time the weapon is cycling before next fire.
extern intptr_t* aplWeaponFlags[MAX_WEAPONS];           // Flags for weapon
extern intptr_t* aplWeaponShoots[MAX_WEAPONS];          // what the weapon shoots
extern intptr_t* aplWeaponSpawnTime[MAX_WEAPONS];       // the frame at which to spawn an item
extern intptr_t* aplWeaponSpawn[MAX_WEAPONS];           // the item to spawn
extern intptr_t* aplWeaponShotsPerBurst[MAX_WEAPONS];   // number of shots per 'burst' (one ammo per 'burst'
extern intptr_t* aplWeaponWorksLike[MAX_WEAPONS];       // What original the weapon works like
extern intptr_t* aplWeaponInitialSound[MAX_WEAPONS];    // Sound made when initialy firing. zero for no sound
extern intptr_t* aplWeaponFireSound[MAX_WEAPONS];       // Sound made when firing (each time for automatic)
extern intptr_t* aplWeaponSound2Time[MAX_WEAPONS];      // Alternate sound time
extern intptr_t* aplWeaponSound2Sound[MAX_WEAPONS];     // Alternate sound sound ID
extern intptr_t* aplWeaponReloadSound1[MAX_WEAPONS];    // Sound of magazine being removed
extern intptr_t* aplWeaponReloadSound2[MAX_WEAPONS];    // Sound of magazine being inserted
extern intptr_t* aplWeaponSelectSound[MAX_WEAPONS];     // Sound for weapon selection
extern intptr_t* aplWeaponFlashColor[MAX_WEAPONS];      // Color for polymer muzzle flash

typedef struct
{
    // NOTE: the member names must be identical to aplWeapon* suffixes.
    int32_t WorksLike;  // What the original works like
    int32_t Clip;  // number of items in magazine
    int32_t Reload;  // delay to reload (include fire)
    int32_t FireDelay;  // delay to fire
    int32_t TotalTime;  // The total time the weapon is cycling before next fire.
    int32_t HoldDelay;  // delay after release fire button to fire (0 for none)
    int32_t Flags;  // Flags for weapon
    int32_t Shoots;  // what the weapon shoots
    int32_t SpawnTime;  // the frame at which to spawn an item
    int32_t Spawn;  // the item to spawn
    int32_t ShotsPerBurst;  // number of shots per 'burst' (one ammo per 'burst')
    int32_t InitialSound;  // Sound made when weapon starts firing. zero for no sound
    int32_t FireSound;  // Sound made when firing (each time for automatic)
    int32_t Sound2Time;  // Alternate sound time
    int32_t Sound2Sound;  // Alternate sound sound ID
    int32_t ReloadSound1;  // Sound of magazine being removed
    int32_t ReloadSound2;  // Sound of magazine being inserted
    int32_t SelectSound;  // Sound of weapon being selected
    int32_t FlashColor;  // Muzzle flash color
} weapondata_t;

extern int16_t WeaponPickupSprites[MAX_WEAPONS];

extern uint8_t allowedColors[(MAXPALOOKUPS + CHAR_BIT - 1) / CHAR_BIT];
extern char pColorNames[MAXPALOOKUPS][32];

void playerColor_init(void);
void playerColor_add(const char* name, int palnum);
void playerColor_delete(int palnum);
uint8_t playerColor_getNextColor(uint8_t palnum, uint8_t useauto);
uint8_t playerColor_getPreviousColor(uint8_t palnum, uint8_t useauto);

void P_SetWeaponGamevars(int playerNum, const DukePlayer_t* const pPlayer);
void P_UpdatePosWhenViewingCam(DukePlayer_t* pPlayer);

int P_GetHudPal(const DukePlayer_t* pPlayer);
int P_GetKneePal(const DukePlayer_t* pPlayer);
int P_FindOtherPlayer(int playerNum, int32_t* pDist);

void P_DisplayScubaMask(int snum);
void P_DisplayWeapon(int snum);
void P_DropWeapon(DukePlayer_t* p);
void P_UpdateScreenPal(DukePlayer_t* p);
void P_QuickKill(DukePlayer_t* p);
void P_GetInput(int snum, bool uncapped);
void P_ProcessInput(int snum);

static inline void P_PalFrom(DukePlayer_t* pPlayer, uint8_t f, uint8_t r, uint8_t g, uint8_t b)
{
    pPlayer->pals.f = f;
    pPlayer->pals.r = r;
    pPlayer->pals.g = g;
    pPlayer->pals.b = b;
}

static inline uint8_t playerColor_getValidPal(int palnum) { return TEST_ALLOWEDCOLOR(palnum) ? palnum : 0; }

#endif