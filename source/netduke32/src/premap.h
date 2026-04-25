#ifndef PREMAP_H
#define PREMAP_H

enum NewGameFlags_t
{
	NEWGAME_NORMAL = 0,
	NEWGAME_NOSEND = (1 << 0),
	NEWGAME_RESETALL = (1 << 1),
	NEWGAME_FROMSERVER = (1 << 2),
};

void G_SetupFilenameBasedMusic(char* nameBuf, const char* fileName);
void G_NewGame(uint32_t flags = NEWGAME_NORMAL);
void G_MakeNewGamePreparations(int vn, int ln, int sk);
void G_RevertUnconfirmedGameSettings();

#endif