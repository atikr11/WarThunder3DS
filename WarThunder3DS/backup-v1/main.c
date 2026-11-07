/* WarThunder3DS: original offline arcade homebrew prototype.
 * Coordinates: Y up, yaw 0 faces +Z. No external assets or New 3DS controls.
 */
#include <3ds.h>
#include <citro3d.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vshader_shbin.h"

#define PI 3.14159265359f
#define ACTORS 14
#define SHOTS 96
#define FX 32
#define CAP 40000
#define EDGE 210.0f
#define TRANSFER (GX_TRANSFER_FLIP_VERT(0)|GX_TRANSFER_OUT_TILED(0)|GX_TRANSFER_RAW_COPY(0)|GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)|GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
typedef struct { float x,y,z; } Vec;
typedef struct { float x,y,z,r,g,b; } Vertex;
typedef struct { Vec p; float yaw,pitch,bank,turret,elev,speed,hp,reload,respawn; int team,plane; } Actor;
typedef struct { Vec p,v; float life,damage; int owner; float gravity; } Shot;
typedef struct { Vec p; float life,size; } Effect;
typedef struct { float x,z,w,d,h; } Building;
static Actor actors[ACTORS];
static Shot shots[SHOTS];
static Effect effects[FX];
static Building buildings[22];
static Vertex *buffers[2],*verts;
static int nv,overflow,half,mode,selection,kills,deaths,winner=-1;
static float tickets[2],capture,elapsed,hudClock,fps=30;

static PrintConsole bottom;
static const float bases[2]={-166,166};
static Vec V(float x,float y,float z) { return (Vec){x,y,z}; }
static Vec add(Vec a,Vec b) { return V(a.x+b.x,a.y+b.y,a.z+b.z); }
static Vec sub(Vec a,Vec b) { return V(a.x-b.x,a.y-b.y,a.z-b.z); }
static Vec mul(Vec a,float s) { return V(a.x*s,a.y*s,a.z*s); }
static float dot(Vec a,Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static float len(Vec a) { return sqrtf(dot(a,a)); }
static float clamp(float a,float lo,float hi) { return a<lo?lo:(a>hi?hi:a); }
static float wrap(float a) { while(a>PI)a-=2*PI; while(a< -PI)a+=2*PI; return a; }
static float turn(float a,float b,float step) { return a+clamp(wrap(b-a),-step,step); }
static Vec direction(float yaw,float pitch) { float c=cosf(pitch);return V(sinf(yaw)*c,sinf(pitch),cosf(yaw)*c); }
static void vertex(Vec p,Vec c) {
    if(nv>=CAP){overflow=1;return;}
    verts[nv++]=(Vertex){p.x,p.y,p.z,c.x,c.y,c.z};
}
static void tri(Vec a,Vec b,Vec c,Vec col) { if(nv+3>CAP){overflow=1;return;} vertex(a,col);vertex(b,col);vertex(c,col); }
static Vec rotate(Vec p,float yaw,float pitch,float bank) {
    float c=cosf(bank),s=sinf(bank),x=p.x*c-p.y*s,y=p.x*s+p.y*c;
    p.x=x;p.y=y;
    c=cosf(pitch);s=sinf(pitch);y=p.y*c+p.z*s;float z=-p.y*s+p.z*c;p.y=y;p.z=z;
    c=cosf(yaw);s=sinf(yaw);return V(p.x*c+p.z*s,p.y,-p.x*s+p.z*c);
}
static void box(Vec center,Vec size,float yaw,float pitch,float bank,Vec col) {
    Vec p[8];
    for(int i=0;i<8;i++)p[i]=add(center,rotate(V((i&1?1:-1)*size.x*.5f,(i&2?1:-1)*size.y*.5f,(i&4?1:-1)*size.z*.5f),yaw,pitch,bank));
    static const int faces[6][4]={{0,1,3,2},{4,6,7,5},{0,4,5,1},{2,3,7,6},{0,2,6,4},{1,5,7,3}};
    static const float shade[6]={.72f,.9f,.52f,1,.65f,.82f};
    for(int f=0;f<6;f++){const int *q=faces[f];Vec c=mul(col,shade[f]);tri(p[q[0]],p[q[1]],p[q[2]],c);tri(p[q[0]],p[q[2]],p[q[3]],c);}
}
static void burst(Vec p,float size) {
    for(int i=0;i<FX;i++)if(effects[i].life<=0){effects[i]=(Effect){p,.65f,size};return;}
}
static int blocked(Vec p,float radius) {
    if(fabsf(p.x)>EDGE-radius||fabsf(p.z)>EDGE-radius)return 1;
    for(int i=0;i<22;i++){Building *b=&buildings[i];
    if(p.y<b->h+radius&&fabsf(p.x-b->x)<b->w*.5f+radius&&fabsf(p.z-b->z)<b->d*.5f+radius)return 1;}
    return 0;
}
static void spawn(int i) {
    Actor *a=&actors[i];
    a->p=V((i%4-1.5f)*17, a->plane?45:1.3f,bases[a->team]+(i%3)*9*(a->team?-1:1));
    a->yaw=a->team?PI:0;a->turret=a->yaw;a->elev=.02f;a->pitch=0;a->bank=0;
    a->speed=a->plane?33:0;a->hp=a->plane?65:120;a->reload=1;a->respawn=0;
}
static void newMatch(void) {
    memset(actors,0,sizeof(actors));memset(shots,0,sizeof(shots));memset(effects,0,sizeof(effects));
    tickets[0]=tickets[1]=100;capture=0;elapsed=0;kills=deaths=0;winner=-1;
    for(int i=0;i<ACTORS;i++){actors[i].team=i%2;actors[i].plane=i>=10;}
    actors[0].plane=selection;
    for(int i=0;i<ACTORS;i++)spawn(i);
    mode=1;
}
static void hurt(int victim,float damage,int owner) {
    Actor *a=&actors[victim];
    if(a->hp<=0)return;
    a->hp-=damage;
    if(a->hp<=0){a->hp=0;a->respawn=6;tickets[a->team]-=4;burst(a->p,7);
    if(owner==0)kills++;
    if(victim==0)deaths++;}
}
static void fire(int id) {
    Actor *a=&actors[id];
    if(a->reload>0||a->hp<=0)return;
    for(int i=0;i<SHOTS;i++)if(shots[i].life<=0){
        Vec d=direction(a->plane?a->yaw:a->turret,a->plane?a->pitch:a->elev);
        Vec p=add(a->p,V(0,a->plane?0:1.6f,0));
        shots[i]=(Shot){add(p,mul(d,5)),mul(d,a->plane?170:115),a->plane?1.9f:3.2f,a->plane?14:48,id,a->plane?0:5};
        a->reload=a->plane?.14f:1.65f;return;
    }
}
/* Segment/sphere test prevents fast rounds passing through vehicles. */
static float segmentDistance(Vec a,Vec b,Vec p) {
    Vec d=sub(b,a);float t=clamp(dot(sub(p,a),d)/fmaxf(dot(d,d),.0001f),0,1);return len(sub(p,add(a,mul(d,t))));
}
static void updateShots(float dt) {
    for(int i=0;i<SHOTS;i++){Shot *s=&shots[i];
    if(s->life<=0)continue;
        Vec old=s->p;s->p=add(s->p,mul(s->v,dt));s->life-=dt;
        s->v.y-=s->gravity*dt;
        int hit=-1;
        /* Find the earliest sampled obstacle along the segment, then test actors before it. */
        Vec end=s->p;int wall=0;float dist=len(sub(end,old));int steps=(int)(dist/1.5f)+1;
        for(int k=1;k<=steps;k++){Vec p=add(old,mul(sub(end,old),(float)k/steps));
    if(p.y<=0||blocked(p,0)){end=p;wall=1;break;}}
        float nearest=1e9f;
        for(int j=0;j<ACTORS;j++)if(actors[j].hp>0&&actors[j].team!=actors[s->owner].team){
            float d=len(sub(actors[j].p,old));
            if(segmentDistance(old,end,add(actors[j].p,V(0,actors[j].plane?0:.8f,0)))<(actors[j].plane?3.7f:3.2f)&&d<nearest){hit=j;nearest=d;}
        }
        if(hit>=0){hurt(hit,s->damage,s->owner);burst(actors[hit].p,1.8f);s->life=0;}
        else if(wall){burst(end,1.3f);s->life=0;}
    }
}
static int targetFor(int id) {
    Actor *a=&actors[id];float best=1e9f;int result=-1;
    for(int i=0;i<ACTORS;i++)if(actors[i].hp>0&&actors[i].team!=a->team){
        float d=len(sub(a->p,actors[i].p));
    if(!a->plane&&actors[i].plane)d*=2;
        if(d<best){best=d;result=i;}
    }return result;
}
static void bot(int id,float dt) {
    Actor *a=&actors[id];int t=targetFor(id);
    if(t<0)return;
    Actor *enemy=&actors[t];Vec delta=sub(enemy->p,a->p);float distance=len(delta);
    if(a->plane){
        /* Lead the target, recover altitude, and turn back inside the battlefield. */
        Vec goal=add(enemy->p,mul(direction(enemy->yaw,enemy->pitch),enemy->speed*distance/170*.6f));
        if(len(V(a->p.x,0,a->p.z))>180)goal=V(0,48,0);
        if(a->p.y<22)goal=add(a->p,V(sinf(a->yaw)*50,48,cosf(a->yaw)*50));
        delta=sub(goal,a->p);float desired=atan2f(delta.x,delta.z);float error=wrap(desired-a->yaw);
        a->yaw=turn(a->yaw,desired,.75f*dt);a->bank=clamp(-error,-.7f,.7f);
        float pitch=atan2f(delta.y,sqrtf(delta.x*delta.x+delta.z*delta.z));
        a->pitch=turn(a->pitch,clamp(pitch,-.45f,.45f),.55f*dt);a->speed=34;
        if(distance<150&&fabsf(error)<.12f&&fabsf(pitch-a->pitch)<.14f)fire(id);
    }else{
        float desired=atan2f(delta.x,delta.z);a->turret=turn(a->turret,desired,1.25f*dt);
        a->elev=clamp(atan2f(delta.y,sqrtf(delta.x*delta.x+delta.z*delta.z))+.004f*distance/10,-.08f,.8f);
        Vec goal=V((id%3-1)*12,0,(id%4-1.5f)*9);
        if(distance<65&&!enemy->plane)goal=enemy->p;
        Vec g=sub(goal,a->p);float heading=atan2f(g.x,g.z);
        Vec ahead=add(a->p,mul(direction(a->yaw,0),10));
        if(blocked(ahead,4))heading=a->yaw+1.8f;
        a->yaw=turn(a->yaw,heading,1.1f*dt);a->speed=len(g)>15?11:0;
        if(distance<155&&fabsf(wrap(desired-a->turret))<.075f)fire(id);
    }
}
static void player(float dt,u32 held) {
    Actor *a=&actors[0];circlePosition cp;hidCircleRead(&cp);
    float x=cp.dx/156.0f,y=cp.dy/156.0f;
    if(fabsf(x)<.12f)x=0;
    if(fabsf(y)<.12f)y=0;
    if(a->plane){
        a->yaw+=x*1.0f*dt;a->pitch=clamp(a->pitch+y*.85f*dt,-.85f,.85f);
        a->bank+=(-x*.85f-a->bank)*fminf(1,5*dt);
        if(held&KEY_A)a->speed+=14*dt;
    if(held&KEY_B)a->speed-=18*dt;
        a->speed=clamp(a->speed,22,58);
    }else{
        a->yaw+=x*1.35f*dt;a->speed=y*(held&KEY_A?19:13);
        if(held&KEY_LEFT)a->turret-=1.3f*dt;
    if(held&KEY_RIGHT)a->turret+=1.3f*dt;
        if(held&KEY_UP)a->elev+=.7f*dt;
    if(held&KEY_DOWN)a->elev-=.7f*dt;
        a->elev=clamp(a->elev,-.12f,.95f);
        if(held&KEY_L){a->turret=turn(a->turret,a->yaw,2.5f*dt);a->elev=turn(a->elev,.02f,dt);}
    }
    if(held&KEY_R)fire(0);
}
static void update(float dt,u32 held) {
    elapsed+=dt;
    for(int i=0;i<ACTORS;i++){
        Actor *a=&actors[i];
        if(a->hp<=0){a->respawn-=dt;
    if(a->respawn<=0)spawn(i);
        continue;}
        a->reload=fmaxf(0,a->reload-dt);
        if(i==0)player(dt,held);else bot(i,dt);
        Vec old=a->p;Vec next=add(old,mul(direction(a->yaw,a->plane?a->pitch:0),a->speed*dt));
        if(a->plane){a->p=next;
    if(next.y<2||next.y>160||blocked(next,2)){hurt(i,1000,i);}}
        else{
            a->p.x=next.x;
    if(blocked(a->p,3))a->p.x=old.x;
            a->p.z=next.z;
    if(blocked(a->p,3))a->p.z=old.z;
            for(int j=0;j<i;j++)if(actors[j].hp>0&&!actors[j].plane&&len(sub(a->p,actors[j].p))<6){a->p=old;break;}
        }
    }
    updateShots(dt);
    for(int i=0;i<FX;i++)effects[i].life=fmaxf(0,effects[i].life-dt);
    int count[2]={0,0};
    for(int i=0;i<ACTORS;i++)if(actors[i].hp>0&&!actors[i].plane&&len(actors[i].p)<26)count[actors[i].team]++;
    if(count[0]&&!count[1])capture=clamp(capture+dt*.13f,-1,1);
    if(count[1]&&!count[0])capture=clamp(capture-dt*.13f,-1,1);
    if(capture>=1)tickets[1]-=dt*1.3f;
    if(capture<=-1)tickets[0]-=dt*1.3f;
    if(tickets[0]<=0||tickets[1]<=0||elapsed>=600){winner=tickets[0]>=tickets[1]?0:1;mode=3;}
}
static void vehicle(const Actor *a) {
    Vec color=a->team?V(.72f,.24f,.17f):V(.2f,.48f,.67f);
    if(a->plane){
        box(a->p,V(1.7f,1.4f,8.6f),a->yaw,a->pitch,a->bank,color);
        box(add(a->p,rotate(V(0,0,.3f),a->yaw,a->pitch,a->bank)),V(12,.24f,2.5f),a->yaw,a->pitch,a->bank,color);
        box(add(a->p,rotate(V(0,.3f,-3.2f),a->yaw,a->pitch,a->bank)),V(4.8f,.23f,1.2f),a->yaw,a->pitch,a->bank,color);
        box(add(a->p,rotate(V(0,1,-3),a->yaw,a->pitch,a->bank)),V(.25f,2,1.5f),a->yaw,a->pitch,a->bank,color);
        box(add(a->p,rotate(V(0,.9f,1),a->yaw,a->pitch,a->bank)),V(1.2f,.8f,2),a->yaw,a->pitch,a->bank,V(.12f,.22f,.3f));
    }else{
        box(a->p,V(4.6f,1.6f,7),a->yaw,0,0,color);
        for(int j=-1;j<=1;j+=2)box(add(a->p,rotate(V(j*2.35f,-.35f,0),a->yaw,0,0)),V(1.1f,1.7f,7.5f),a->yaw,0,0,V(.16f,.17f,.15f));
        box(add(a->p,V(0,1.25f,0)),V(3.1f,1.3f,3.5f),a->turret,0,0,color);
        Vec barrel=add(add(a->p,V(0,1.6f,0)),mul(direction(a->turret,a->elev),3.1f));
        box(barrel,V(.35f,.35f,5.2f),a->turret,a->elev,0,mul(color,.72f));
    }
    box(add(a->p,V(0,a->plane?3:5,0)),V(1,.7f,.7f),0,0,0,color);
}
static void world(void) {
    /* A fixed vertex budget, flat terrain, no textures, and a bounded actor pool. */
    for(int z=-7;z<7;z++)for(int x=-7;x<7;x++){
        float x0=x*30,z0=z*30;Vec c=((x+z)&1)?V(.36f,.43f,.25f):V(.4f,.47f,.28f);
        tri(V(x0,0,z0),V(x0+30,0,z0),V(x0+30,0,z0+30),c);
        tri(V(x0,0,z0),V(x0+30,0,z0+30),V(x0,0,z0+30),c);
    }
    box(V(0,.03f,0),V(11,.04f,420),0,0,0,V(.42f,.4f,.32f));
    box(V(0,.06f,0),V(420,.04f,11),0,0,0,V(.42f,.4f,.32f));
    for(int i=0;i<22;i++){Building *b=&buildings[i];box(V(b->x,b->h*.5f,b->z),V(b->w,b->h,b->d),0,0,0,i%2?V(.56f,.52f,.43f):V(.49f,.48f,.44f));}
    Vec flag=capture>=1?V(.1f,.6f,1):capture<=-1?V(1,.2f,.1f):V(.9f,.8f,.3f);
    for(int i=0;i<24;i++){float a=i*PI/12,b=(i+1)*PI/12;tri(V(sinf(a)*25,.12f,cosf(a)*25),V(sinf(b)*25,.12f,cosf(b)*25),V(sinf(a)*23,.12f,cosf(a)*23),flag);}
    box(V(0,7,0),V(.3f,14,.3f),0,0,0,V(.7f,.7f,.65f));
    box(V(3,12,0),V(6,3,.12f),0,0,0,flag);
    for(int t=0;t<2;t++)box(V(0,.15f,bases[t]),V(40,.2f,24),0,0,0,t?V(.5f,.23f,.18f):V(.18f,.35f,.5f));
    for(int i=0;i<ACTORS;i++)if(actors[i].hp>0)vehicle(&actors[i]);
    for(int i=0;i<SHOTS;i++)if(shots[i].life>0)box(shots[i].p,V(.24f,.24f,1.5f),atan2f(shots[i].v.x,shots[i].v.z),0,0,V(1,.85f,.24f));
    for(int i=0;i<FX;i++)if(effects[i].life>0){Effect *e=&effects[i];float s=e->size*(1.5f-e->life);box(e->p,V(s,s,s),e->life*5,e->life,0,V(1,e->life,.07f));}
    /* Low distant mountains mark the edge without expensive terrain. */
    for(int i=0;i<16;i++){float a=i*PI/8;Vec p=V(sinf(a)*265,0,cosf(a)*265);tri(add(p,V(-35,0,0)),add(p,V(0,35+i%3*10,0)),add(p,V(35,0,0)),V(.38f,.45f,.46f));}
}
static void hud(void) {
    consoleSelect(&bottom);printf("\x1b[H");
    if(mode==0){
        printf("     WAR THUNDER 3DS - PROTOTYPE\n\n");
        printf("  Original offline arcade homebrew\n\n");
        printf("  Vehicle: <%s>             \n\n",selection?"AIRCRAFT":"TANK    ");
        printf("  Left / Right : choose vehicle\n  A : deploy     START : quit\n\n");
        printf("  Capture the central flag with tanks.\n  Destroy enemies to drain tickets.\n  First team to zero loses.\n\n");
        printf("  Circle Pad : drive / steer aircraft\n  D-pad : tank turret and gun elevation\n  R : fire     L : center tank turret\n  A : boost    B : aircraft airbrake\n  X : switch vehicle and respawn\n  START : pause    SELECT : menu\n\n  Blue = allies. Red = enemies.\n");return;
    }
    Actor *a=&actors[0];
    printf(" BLUE %3d     RED %3d    %02d:%02d     \n",(int)fmaxf(0,tickets[0]),(int)fmaxf(0,tickets[1]),(int)elapsed/60,(int)elapsed%60);
    printf(" %-8s  HP %3d  SPEED %3d        \n",a->plane?"AIRCRAFT":"TANK",(int)a->hp,(int)fabsf(a->speed));
    printf(" KILLS %d   DEATHS %d   FPS %2.0f       \n",kills,deaths,fps);
    printf(" GUN %s  ALT %3d                 \n",a->reload>0?"LOADING":"READY  ",(int)a->p.y);
    printf(" FLAG: %-8s  CAP %4d%%          \n",capture>=1?"BLUE":capture<=-1?"RED":"NEUTRAL",(int)(capture*100));
    printf("\n             RADAR                  \n");
    char radar[11][24];for(int r=0;r<11;r++){memset(radar[r],'.',23);radar[r][23]=0;}radar[5][11]='F';
    for(int i=ACTORS-1;i>=0;i--)if(actors[i].hp>0){int x=(int)((actors[i].p.x+EDGE)/(2*EDGE)*22),z=(int)((EDGE-actors[i].p.z)/(2*EDGE)*10);
    if(x>=0&&x<23&&z>=0&&z<11)radar[z][x]=i==0?'P':actors[i].team?'E':'A';}
    for(int r=0;r<11;r++)printf("       %s        \n",radar[r]);
    if(mode==2)printf(" PAUSED - START to resume             \n");
    else if(mode==3)printf(" %s - A: rematch           \n",winner==0?"BLUE WINS":"RED WINS ");
    else if(a->hp<=0)printf(" DESTROYED - respawn in %.0f            \n",ceilf(a->respawn));
    else printf(" R:fire X:vehicle START:pause          \n");
    printf(" SELECT: menu                         \n");
}
int main(void) {
    gfxInitDefault();gfxSet3D(false);consoleInit(GFX_BOTTOM,&bottom);
    if(!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)){printf("Graphics initialization failed.\n");goto graphics_fail;}
    C3D_RenderTarget *target=C3D_RenderTargetCreate(240,400,GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8);
    DVLB_s *shader=DVLB_ParseFile((u32*)vshader_shbin,vshader_shbin_size);
    buffers[0]=linearAlloc(sizeof(Vertex)*CAP);buffers[1]=linearAlloc(sizeof(Vertex)*CAP);
    if(!target||!shader||!buffers[0]||!buffers[1]){printf("Not enough graphics memory.\n");
    if(shader)DVLB_Free(shader);
    if(target)C3D_RenderTargetDelete(target);
    goto cleanup;}
    C3D_RenderTargetSetOutput(target,GFX_TOP,GFX_LEFT,TRANSFER);
    shaderProgram_s program;shaderProgramInit(&program);shaderProgramSetVsh(&program,&shader->DVLE[0]);C3D_BindProgram(&program);
    int uniform=shaderInstanceGetUniformLocation(program.vertexShader,"projection");
    C3D_AttrInfo *attr=C3D_GetAttrInfo();AttrInfo_Init(attr);AttrInfo_AddLoader(attr,0,GPU_FLOAT,3);AttrInfo_AddLoader(attr,1,GPU_FLOAT,3);
    C3D_TexEnv *env=C3D_GetTexEnv(0);C3D_TexEnvInit(env);C3D_TexEnvSrc(env,C3D_Both,GPU_PRIMARY_COLOR,0,0);C3D_TexEnvFunc(env,C3D_Both,GPU_REPLACE);
    C3D_CullFace(GPU_CULL_NONE);C3D_DepthTest(true,GPU_GREATER,GPU_WRITE_ALL);
    for(int i=0;i<22;i++){int side=i%2?1:-1;buildings[i]=(Building){side*(40+(i%4)*26),-125+(i/2)*25,12+(i%3)*5,13+(i%4)*3,7+(i%5)*3};}
    newMatch();mode=0;hud();
    u64 last=osGetTime();
    while(aptMainLoop()){
        hidScanInput();u32 down=hidKeysDown(),held=hidKeysHeld();u64 now=osGetTime();float raw=(now-last)*.001f;last=now;float dt=clamp(raw,.001f,.05f);
        fps=fps*.95f+(1/fmaxf(raw,.001f))*.05f;
        if(mode==0){if(down&KEY_START)break;
    if(down&(KEY_LEFT|KEY_RIGHT)){selection=!selection;consoleClear();hud();}if(down&KEY_A){newMatch();consoleClear();}}
        else if(down&KEY_SELECT){mode=0;consoleClear();hud();}
        else if(mode==3){if(down&KEY_A){newMatch();consoleClear();}}
        else {if(down&KEY_START)mode=mode==1?2:1;
    if(mode==1){if(down&KEY_X){selection=!actors[0].plane;tickets[0]-=4;actors[0].plane=selection;spawn(0);}update(dt,held);}}
        hudClock+=dt;
    if(hudClock>.15f){hudClock=0;hud();}
        if(!C3D_FrameBegin(C3D_FRAME_SYNCDRAW))continue;
        half^=1;verts=buffers[half];nv=0;overflow=0;world();int worldCount=nv;
        Actor *a=&actors[0];Vec d=direction(a->plane?a->yaw:a->turret,a->plane?a->pitch:a->elev);
        Vec focus=add(a->p,V(0,a->plane?1:2.5f,0));Vec eye=add(sub(focus,mul(d,a->plane?22:18)),V(0,a->plane?5:7,0));eye.y=fmaxf(eye.y,3);
        C3D_Mtx view,projection,mvp;Mtx_LookAt(&view,FVec3_New(eye.x,eye.y,eye.z),FVec3_New(focus.x+d.x*35,focus.y+d.y*35,focus.z+d.z*35),FVec3_New(0,1,0),false);
        Mtx_PerspTilt(&projection,PI/3,400.0f/240,.4f,620,false);Mtx_Multiply(&mvp,&projection,&view);
        /* Project the actual muzzle direction to a screen reticle. */
        Vec aim=add(add(a->p,V(0,a->plane?0:1.6f,0)),mul(d,100));
        C3D_FVec clip=Mtx_MultiplyFVec4(&view,FVec4_New(aim.x,aim.y,aim.z,1));
        float sx=200,sy=120;
        /* Compute in camera space so screen rotation cannot flip the reticle. */
        if(clip.z<-.01f){sx=200+clip.x/(-clip.z*tanf(PI/6)*(400.0f/240))*200;sy=120-clip.y/(-clip.z*tanf(PI/6))*120;}
        sx=clamp(sx,12,388);sy=clamp(sy,12,228);
        Vec white=V(1,.95f,.6f);
        box(V(sx,sy,0),V(15,1.5f,.1f),0,0,0,white);box(V(sx,sy,0),V(1.5f,15,.1f),0,0,0,white);int total=nv;
        C3D_RenderTargetClear(target,C3D_CLEAR_ALL,0x86B4CCFF,0);C3D_FrameDrawOn(target);
        C3D_BufInfo *buf=C3D_GetBufInfo();BufInfo_Init(buf);BufInfo_Add(buf,verts,sizeof(Vertex),2,0x10);
        C3D_DepthTest(true,GPU_GREATER,GPU_WRITE_ALL);C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,uniform,&mvp);
        C3D_DrawArrays(GPU_TRIANGLES,0,worldCount);
        Mtx_OrthoTilt(&projection,0,400,240,0,-1,1,false);C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,uniform,&projection);C3D_DepthTest(false,GPU_ALWAYS,0);
        C3D_DrawArrays(GPU_TRIANGLES,worldCount,total-worldCount);C3D_FrameEnd(0);
    }
    C3D_FrameSync();shaderProgramFree(&program);DVLB_Free(shader);C3D_RenderTargetDelete(target);
cleanup:
    if(buffers[0])linearFree(buffers[0]);
    if(buffers[1])linearFree(buffers[1]);
    C3D_Fini();
graphics_fail:
    gfxExit();return 0;
}


