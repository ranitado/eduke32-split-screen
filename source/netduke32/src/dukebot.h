#ifndef DUKEBOT_H
#define DUKEBOT_H

#include "build.h"
#include "player.h"

#define BOT_DEBUG 0
#define BOT_MAXSPEED 2560
#define BOT_SEEK_GIVEUP 150
#define BOT_SEEK_LOOKFORBETTER 30
#define BOT_MIN_RESPAWN_TIME 30
#define BOT_MAX_RESPAWN_TIME 90
#define BOT_MIN_REACTION_TIME 4
#define BOT_MAX_REACTION_TIME 10

enum BotMoveFlags
{
    BOT_MOVE_STRAFE     = (1 << 0), // Strafing back and forth.
    BOT_MOVE_JUMP       = (1 << 1), // Jump. Self-explanatory.
    BOT_MOVE_CROUCH     = (1 << 2), // Crouch. Also self-explanatory.
    BOT_MOVE_USE        = (1 << 3), // Press open/use on walls/sectors.
    BOT_MOVE_ASCEND     = (1 << 4), // Ascend with jetpack, or in water.
    BOT_MOVE_DESCEND    = (1 << 5), // Descend with jetpack, or in water.
};

enum BotCombatFlags
{
    BOT_COMBAT_MELEE    = (1 << 0), // Melee, ignore walls, & items farther than target. Don't dodge or jump if touching target.
    BOT_COMBAT_STOMP    = (1 << 1), // Stomping shrunken or frozen target, ignore everything else.
};

typedef struct
{
    vec3_t goalPos, lastSeenPlayerPos[MAXPLAYERS];
    int32_t moveCount, cycleCount, seekSpriteFailCount, seekSpriteCooldown, waterSubmergeTime, respawnTime, reactionTime;
    int32_t goalSprite, goalSpriteScore, lastGoalSprite;
    int32_t goalPlayer, goalWall;
    int32_t rand;
    int16_t lastStartSect, lastEndSect;
    uint8_t active;
} DukeBotAI_t;

extern int32_t botNameSeed;
extern DukeBotAI_t botAI[MAXPLAYERS];
//extern input_t botInput[MAXPLAYERS];

void DukeBot_ResetAll();
void DukeBot_Activate(int32_t playerNum);
void DukeBot_GetInput(int snum, input_t* syn);

#endif // DUKEBOT_H