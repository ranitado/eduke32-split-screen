//-------------------------------------------------------------------------
/*
Copyright (C) 1996, 2003 - 3D Realms Entertainment
Copyright (C) 2000, 2003 - Matt Saettler (EDuke Enhancements)
Copyright (C) 2004, 2007 - EDuke32 developers

This file is part of EDuke32

EDuke32 is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
//-------------------------------------------------------------------------

#ifndef __funct_h__
#define __funct_h__

#ifndef _MSC_VER
  #ifdef __GNUC__
    #ifndef __fastcall
      #define __fastcall __attribute__((fastcall))
    #endif
  #else
    #define __fastcall
  #endif
#endif

void G_UpdateAppTitle(char const* const name = nullptr);

extern int A_CallSound(int sn,int whatsprite);
extern int check_activator_motion(int lotag);
extern int CheckDoorTile(int dapic);
extern int isanunderoperator(int lotag);
extern int isanearoperator(int lotag);
extern int CheckPlayerInSector(int sect);
extern int __fastcall A_FindPlayer(spritetype *s,int *d);

extern void G_DoSectorAnimations(void);
extern int32_t* G_GetAnimationPointer(int32_t index);
extern int GetAnimationGoal(int16_t target, int8_t type);
extern int SetAnimation(int16_t target, int8_t type, int thegoal, int thevel);

extern void G_AnimateCamSprite(int smoothRatio);
extern void G_AnimateWalls(void);
extern int G_ActivateWarpElevators(int s,int d);
extern void G_OperateSectors(int sn,int ii);
extern void G_OperateRespawns(int low);
extern void G_OperateActivators(int lotag, int playerNum);
extern void G_OperateMasterSwitches(int low);
extern void G_OperateForceFields(int s,int low);
extern int P_ActivateSwitch(int snum,int w,int switchtype);
extern void activatebysector(int sect,int j);

// [JM] I should make a sector.h
extern void A_DamageWall(int spriteNum, int wallNum, const vec3_t& vPos, int weaponNum);
extern void A_DamageWall_Internal(int spriteNum, int wallNum, const vec3_t& vPos, int weaponNum);
extern void Sect_DamageFloor(int const spriteNum, int const sectNum);
extern void Sect_DamageFloor_Internal(int const spriteNum, int const sectNum);
extern void Sect_DamageCeiling(int const spriteNum, int const sectNum);
extern void Sect_DamageCeiling_Internal(int const spriteNum, int const sectNum);
// -----------------------------


extern void A_DamageObject_Internal(int i,int sn);
extern void A_DamageObject(int i, int sn);
extern void allignwarpelevators(void);
extern void P_HandleSharedKeys(int snum);
extern void P_CheckSectors(int snum);
extern void G_CacheMapData(void);
extern void G_UpdateScreenArea(void);
extern void P_RandomSpawnPoint(int snum);
extern void P_ResetWeapons(int snum);
extern void P_ResetInventory(int snum);
extern void G_ResetTimers(void);
extern void clearfifo(void);
extern int  G_EnterLevel(int g);
extern void G_BackToMenu(void);
extern int A_GetHitscanRange(int i);
extern void ChangeToMenu(int cm);
// extern void savetemp(const char *fn,int daptr,int dasiz);
// extern int G_LoadSaveHeader(char spot,int32_t *vn,int32_t *ln,int32_t *psk,int32_t *numplr);
extern int G_LoadPlayer(int spot);
extern int G_SavePlayer(int spot);
extern int menutext_(int x,int y,int s,int p,const char *t,int bits);
#define menutext(x,y,s,p,t) menutext_(x,y,s,p,OSD_StripColors(menutextbuf,t),10+16)
extern void M_DisplayMenus(void);
extern void G_PlayAnim(const char *fn,char);
extern int G_GetAngleDelta(int a,int na);
extern void A_Execute(int iActor,int iPlayer,int lDist);
extern void overwritesprite(int thex,int they,int tilenum,int shade,int stat,int dapalnum);
extern void gamenumber(int x,int y,int n,int s);
extern void G_Shutdown(void);

extern void G_GameExit(const char *t);
extern void G_DisplayRest(int smoothratio);
extern void updatesectorz(int x,int y,int z,short *sectnum);
extern void G_DrawBackground(void);
extern void G_DrawRooms(int snum,int smoothratio);
extern int A_InsertSprite(int whatsect,int s_x,int s_y,int s_z,int s_pn,int s_s,int s_xr,int s_yr,int s_a,int s_ve,int s_zv,int s_ow,int s_ss);
extern int A_Spawn(int j,int pn);
extern void G_DoSpriteAnimations(int32_t x, int32_t y, int32_t z, int32_t a, int32_t smoothratio);
extern int main(int argc,char **argv);
extern void G_OpenDemoWrite(void);
extern void G_CloseDemoWrite(void);
extern void A_SpawnWallGlass(int i,int wallnum,int n);
extern void A_SpawnGlass(int i,int n);
extern void A_SpawnCeilingGlass(int i,int sectnum,int n);
extern void A_SpawnRandomGlass(int i,int wallnum,int n);
extern int GetTime(void);
extern void CONFIG_SetDefaults(void);
extern void CONFIG_ReadKeys(void);
extern void ReadSaveGameHeaders(void);

// game.cpp
extern int mpgametext(int y, const char *t, int s, int dabits);

extern void G_HandleMirror(int32_t x, int32_t y, int32_t z, fix16_t a, fix16_t q16horiz, int32_t smoothratio);
extern int G_GameTextLen(int x, const char *t);
extern int G_PrintGameText(int small, int starttile, int x,int y,const char *t,int s,int p,int orientation,int x1, int y1, int x2, int y2,int z);
extern void G_DrawTXDigiNumZ(int starttile, int x,int y,int n,int s,int pal,int cs,int x1, int y1, int x2, int y2, int z);
extern void Gv_ResetVars(void);
extern void A_ResetVars(int iActor);

extern int minitext_(int x,int y,const char *t,int s,int p,int sb);

extern int32_t G_GetStringLineLength(const char *text, const char *end, const int32_t iter);
extern int32_t G_GetStringNumLines(const char *text, const char *end, const int32_t iter);
extern char* G_GetSubString(const char *text, const char *end, const int32_t iter, const int32_t length);
extern int32_t G_GetStringTile(int32_t font, char *t, int32_t f);

extern vec2_t G_ScreenTextSize(const int32_t font, int32_t x, int32_t y, const int32_t z, const int32_t blockangle, const char *str, const int32_t o, int32_t xspace, int32_t yline, int32_t xbetween, int32_t ybetween, const int32_t f, int32_t x1, int32_t y1, int32_t x2, int32_t y2);

extern void G_AddCoordsFromRotation(vec2_t *coords, const vec2_t *unitDirection, const int32_t magnitude);
extern vec2_t G_ScreenText(const int32_t font, int32_t x, int32_t y, const int32_t z, const int32_t blockangle, const int32_t charangle, const char *str, const int32_t shade, int32_t pal, int32_t o, const int32_t alpha, int32_t xspace, int32_t yline, int32_t xbetween, int32_t ybetween, const int32_t f, const int32_t x1, const int32_t y1, const int32_t x2, const int32_t y2);

#define minitextshade(x, y, t, s, p, sb) minitext_(x,y,t,s,p,sb)
#define minitext(x, y, t, p, sb) minitext_(x,y,t,0,p,sb)

#define gametext(x,y,t,s,dabits) G_PrintGameText(0,STARTALPHANUM, x,y,t,s,0,dabits,0, 0, xdim-1, ydim-1, 65536)
#define gametextscaled(x,y,t,s,dabits) G_PrintGameText(1,STARTALPHANUM, x,y,t,s,0,dabits,0, 0, xdim-1, ydim-1, 65536)
#define gametextpal(x,y,t,s,p) G_PrintGameText(0,STARTALPHANUM, x,y,t,s,p,26,0, 0, xdim-1, ydim-1, 65536)
#define gametextpalbits(x,y,t,s,p,dabits) G_PrintGameText(0,STARTALPHANUM, x,y,t,s,p,dabits,0, 0, xdim-1, ydim-1, 65536)

extern void A_LoadActor(int sActor);

// extern void SetGameArrayID(int id,int index, int lValue);

extern void C_ReportError(int iError);

extern void onvideomodechange(int newmode);

extern void G_UpdatePlayerFromMenu(void);

extern void G_AddUserQuote(const char *daquote);

extern void se40code(int x,int y,int z,int a,int h, int smoothratio);

extern void G_FreeMapState(int mapnum);
extern int32_t G_FindLevelByFile(const char *fn);

extern int _EnterText(int small,int x,int y,char *t,int dalen,int c);

#define G_EnterText(x, y, t, dalen, c) _EnterText(0,x,y,t,dalen,c)
#define Net_EnterText(x, y, t, dalen, c) _EnterText(1,x,y,t,dalen,c)

#endif // __funct_h__
