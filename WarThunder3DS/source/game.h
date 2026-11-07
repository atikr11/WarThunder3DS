#ifndef GAME_H
#define GAME_H
#ifdef HOST_TEST
#include <stdint.h>
typedef uint32_t u32;
#define KEY_A (1u<<0)
#define KEY_B (1u<<1)
#define KEY_DRIGHT (1u<<4)
#define KEY_DLEFT (1u<<5)
#define KEY_DUP (1u<<6)
#define KEY_DDOWN (1u<<7)
#define KEY_R (1u<<8)
#define KEY_L (1u<<9)
#else
#include <3ds.h>
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define PI 3.14159265359f
#define MAX_ACTORS 12
#define MAX_SHOTS 96
#define MAX_FX 40
#define BUILDINGS 54
#define TREES 220
#define OBJECTIVES 3
#define MODE_MENU 0
#define MODE_PLAY 1
#define MODE_PAUSE 2
#define MODE_RESULT 3

typedef struct {float x,y,z;} Vec;
typedef struct {
    Vec p; float yaw,pitch,bank,turret,elev,speed,hp,reload,respawn;
    float think,desiredYaw,desiredPitch,desiredSpeed,evade,stuck;
    Vec previous; int team,plane,target,goal;
} Actor;
typedef struct {Vec p,v;float life,damage,gravity;int owner,team;} Shot;
typedef struct {Vec p;float life,size;int smoke;} Effect;
typedef struct {float x,z,w,d,h;int style;} Building;
typedef struct {Vec p;float size;int style;} Tree;
typedef struct {Vec p;float capture;int owner,contested;} Objective;
typedef struct {u32 held;float stickX,stickY;} Controls;
typedef struct {
    Actor actors[MAX_ACTORS]; Shot shots[MAX_SHOTS];Effect fx[MAX_FX];
    Building buildings[BUILDINGS];Tree trees[TREES];Objective objectives[OBJECTIVES];
    int mode,selection,air,count,kills,deaths,winner,closeCamera,overflow;
    float edge,tickets[2],elapsed,fps,hitMarker,damageFlash,messageTime;
    unsigned seed;char message[64];
} Game;
extern Game game;
static inline Vec V(float x,float y,float z){return (Vec){x,y,z};}
static inline Vec add(Vec a,Vec b){return V(a.x+b.x,a.y+b.y,a.z+b.z);}
static inline Vec sub(Vec a,Vec b){return V(a.x-b.x,a.y-b.y,a.z-b.z);}
static inline Vec mul(Vec a,float s){return V(a.x*s,a.y*s,a.z*s);}
static inline float dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static inline float length(Vec a){return sqrtf(dot(a,a));}
static inline float clampf(float x,float lo,float hi){return x<lo?lo:(x>hi?hi:x);}
static inline float wrap(float a){while(a>PI)a-=2*PI;while(a< -PI)a+=2*PI;return a;}
static inline float turn(float a,float b,float step){return a+clampf(wrap(b-a),-step,step);}
static inline Vec direction(float yaw,float pitch){float c=cosf(pitch);return V(sinf(yaw)*c,sinf(pitch),cosf(yaw)*c);}
static inline float horizontal(Vec a){return sqrtf(a.x*a.x+a.z*a.z);}
static inline float maxHP(const Actor *a){return a->plane?100:160;}
static inline float reloadTime(const Actor *a){return a->plane?.12f:1.8f;}
float randf(void);
void initGame(void);
void startMatch(int air);
void spawnActor(int i);
void stepGame(float dt,Controls controls);
void playerControl(float dt,Controls controls);
float traceWorld(Vec from,Vec to,float radius);
void damageActor(int victim,float amount,int owner);
void fireWeapon(int id);
void announce(const char *message);
void initAudio(void);
void stopAudio(void);
void updateAudio(void);
void playSound(int kind,Vec p,float volume);
#endif

