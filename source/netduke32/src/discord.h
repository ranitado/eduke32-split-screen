#ifndef DISCORD_H
#define DISCORD_H

#ifdef DISCORD_RPC
#include "discord_rpc.h"

extern int8_t discord_status;
extern DiscordRichPresence discordPresence;

#define DISCORD_WAITING 0
#define DISCORD_READY 1
#define DISCORD_DISCONNECTED 2
#define DISCORD_ERROR 3

void InitDiscord();
void CheckDiscord();
void UpdateDiscord();
#endif

#endif // DISCORD_H