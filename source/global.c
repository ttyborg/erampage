//-------------------------------------------------------------------------
/*
Copyright (C) 1996, 2003 - 3D Realms Entertainment
Copyright (C) 2000, 2003 - Matt Saettler (EDuke Enhancements)
Copyright (C) 2004, 2007 - EDuke32 developers
Copyright (C) 2009 - dimag0g

This file is part of eRampage port for Redneck Rampage series, derived from EDuke32

eRampage is free software; you can redistribute it and/or
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
#include "duke3d.h"

const char *s_buildDate = __DATE__;
char *MusicPtr = NULL;
int32_t g_musicSize;

int16_t g_globalRandom;
int16_t neartagsector, neartagwall, neartagsprite;

int32_t neartaghitdist,lockclock,g_startArmorAmount;
// JBF: g_spriteGravity modified to default to Atomic ed. default when using 1.3d CONs
int32_t g_spriteGravity=176;

// int32_t temp_data[MAXSPRITES][6];
ActorData_t ActorExtra[MAXSPRITES];

int16_t SpriteDeletionQueue[1024],g_spriteDeleteQueuePos,g_spriteDeleteQueueSize=64;
animwalltype animwall[MAXANIMWALLS];
int16_t g_numAnimWalls;
int32_t *animateptr[MAXANIMATES];
int32_t animategoal[MAXANIMATES], animatevel[MAXANIMATES], g_animateCount;
// int32_t oanimateval[MAXANIMATES];
int16_t animatesect[MAXANIMATES];
int32_t msx[2048],msy[2048];
int16_t cyclers[MAXCYCLERS][6],g_numCyclers;

char *ScriptQuotes[MAXQUOTES], *ScriptQuoteRedefinitions[MAXQUOTES];

char tempbuf[2048], packbuf[4096], menutextbuf[128], buf[1024];

int16_t camsprite;
int16_t g_mirrorWall[64], g_mirrorSector[64], g_mirrorCount;

int32_t g_currentMenu;

map_t MapInfo[(MAXVOLUMES+1)*MAXLEVELS]; // +1 volume for "intro", "briefing" music
sound_t g_sounds[ MAXSOUNDS ];
volatile char g_soundlocks[MAXSOUNDS];

char EpisodeNames[MAXVOLUMES][33] = { "L.A. MELTDOWN", "LUNAR APOCALYPSE", "SHRAPNEL CITY" };
char SkillNames[5][33] = { "PIECE OF CAKE", "LET'S ROCK", "COME GET SOME", "DAMN I'M GOOD" };

char GametypeNames[MAXGAMETYPES][33] = { "DUKEMATCH (SPAWN)","COOPERATIVE PLAY","DUKEMATCH (NO SPAWN)","TEAM DM (SPAWN)","TEAM DM (NO SPAWN)"};

int32_t GametypeFlags[MAXGAMETYPES] = {
    /*4+*/8+16+1024+2048+16384,
    1+2+32+64+128+256+512+4096+8192+32768,
    2+/*4+*/8+16+16384,
    /*4+*/8+16+1024+2048+16384+65536+131072,
    2+/*4+*/8+16+16384+65536+131072
};
char g_numGametypes = 5;

int32_t g_currentFrameRate;

char g_numVolumes = 3;

int32_t g_timerTicsPerSecond=TICRATE;
//fx_device device;

char g_numPlayerSprites,g_loadFromGroupOnly=0,g_earthquakeTime;

int32_t playerswhenstarted;

int32_t fricxv,fricyv;
playerdata_t g_player[MAXPLAYERS];
input_t inputfifo[MOVEFIFOSIZ][MAXPLAYERS];
PlayerSpawn_t g_playerSpawnPoints[MAXPLAYERS];
user_defs ud;

char pus, pub;
char syncstat[MAXSYNCBYTES];
int32_t syncvaltail, syncvaltottail;

input_t loc;
input_t recsync[RECSYNCBUFSIZ];
int32_t avgfvel, avgsvel, avgavel, avghorz, avgbits, avgextbits;

int32_t movefifosendplc;

//Multiplayer syncing variables
int32_t screenpeek;

//Game recording variables

char ready2send;
int32_t vel, svel, angvel, horiz, ototalclock, g_actorRespawnTime=768, g_itemRespawnTime=768, g_groupFileHandle;

intptr_t *g_scriptPtr,*insptr,*labelcode,*labeltype;
int32_t g_numLabels,g_numDefaultLabels;
intptr_t *actorscrptr[MAXTILES],*g_parsingActorPtr;
char *label;
char ActorType[MAXTILES];
intptr_t *script = NULL;

int32_t g_scriptSize = 1048576;

char typebuflen,typebuf[141];

char g_musicIndex;
char EnvMusicFilename[MAXVOLUMES+1][BMAX_PATH];
char g_RTSPlaying;


int16_t BlimpSpawnSprites[15] = {
    RPGSPRITE__STATIC,
    CHAINGUNSPRITE__STATIC,
    TEATGUNAMMO__STATIC,
    RPGAMMO__STATIC,
    RPGAMMO__STATIC,
    JETPACK__STATIC,
    SHIELD__STATIC,
    FIRSTAID__STATIC,
    STEROIDS__STATIC,
    RPGAMMO__STATIC,
    RPGAMMO__STATIC,
    RPGSPRITE__STATIC,
    RPGAMMO__STATIC,
    BUZSAWSPRITE__STATIC,
    BUZSAWAMMO__STATIC
};

int32_t g_impactDamage;
int32_t g_scriptDebug;

//GLOBAL.C - replace the end "my's" with this
vec3_t my, omy, myvel;
int16_t myhoriz, omyhoriz, myhorizoff, omyhorizoff;
int16_t myang, omyang, mycursectnum, myjumpingcounter;

char myjumpingtoggle, myonground, myhardlanding, myreturntocenter;
int8_t multiwho, multipos, multiwhat, multiflag;

int32_t predictfifoplc,movefifoplc;
int32_t myxbak[MOVEFIFOSIZ], myybak[MOVEFIFOSIZ], myzbak[MOVEFIFOSIZ];
int32_t myhorizbak[MOVEFIFOSIZ],g_playerFriction = 0xcc00, g_showShareware;

int16_t myangbak[MOVEFIFOSIZ];
char szPlayerName[32];
int32_t g_damageCameras,g_BUZSAWrSelfDamage=0,g_tripbombLaserMode=0;
int32_t g_gameQuit = 0,everyothertime;
int32_t g_numBUZSAWBounces=3,g_rpgBlastRadius,g_pipebombBlastRadius,g_tripbombBlastRadius,
                           g_ALIENARMGUNBlastRadius,g_morterBlastRadius,g_bouncemineBlastRadius,g_seenineBlastRadius;
DukeStatus_t sbar;

int32_t mymaxlag, otherminlag, bufferjitter = 1;
int16_t g_numClouds,clouds[128],cloudx[128],cloudy[128];
int32_t cloudtotalclock = 0,totalmemory = 0;
int32_t g_numInterpolations = 0, startofdynamicinterpolations = 0;
int32_t g_interpolationLock = 0;
int32_t oldipos[MAXINTERPOLATIONS];
int32_t bakipos[MAXINTERPOLATIONS];
int32_t *curipos[MAXINTERPOLATIONS];

int32_t nextvoxid = 0;

int32_t SpriteFlags[MAXTILES];

projectile_t ProjectileData[MAXTILES], DefaultProjectileData[MAXTILES], SpriteProjectile[MAXSPRITES];

char CheatKeys[2] = { sc_D, sc_N };
char setupfilename[BMAX_PATH]= SETUPFILENAME;

int32_t g_doQuickSave = 0;
uint32_t g_moveThingsCount = 0;

