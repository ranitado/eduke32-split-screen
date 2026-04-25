#ifndef __osdcmds_h__
#define __osdcmds_h__

struct osdcmd_cheatsinfo {
	int cheatnum;	// -1 = none, else = see DoCheats()
	int volume,level;
};

extern struct osdcmd_cheatsinfo osdcmd_cheatsinfo_stat;

int registerosdcommands(void);

extern float r_ambientlight,r_ambientlightrecip;

#endif	// __osdcmds_h__

