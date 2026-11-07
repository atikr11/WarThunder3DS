#include "graphics.h"
#include <citro3d.h>
#include "vshader_shbin.h"
#define FRAME_CAP 40000
#define STATIC_CAP 49152
#define GRID 10
#define CHUNKS (GRID*GRID)
#define TRANSFER (GX_TRANSFER_FLIP_VERT(0)|GX_TRANSFER_OUT_TILED(0)|GX_TRANSFER_RAW_COPY(0)|GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)|GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
typedef struct {float x,y,z,r,g,b;} Vertex;
typedef struct {int first,count;Vec center;float radius;} Chunk;
static Vertex *frame[2],*terrain,*vertices;
static int capacity,nv,bufferIndex,terrainCount,mountainStart;
static Chunk chunks[CHUNKS];
static C3D_RenderTarget *top,*bottom;
static DVLB_s *shader;
static shaderProgram_s program;
static int uniformLocation,programReady,cameraReady;
static Vec eye,look,forward;
static float cameraYaw,sceneTime,zoomAmount,viewTan=.5773503f;
static C3D_Mtx view,mvp;
static const Vec navy={.045f,.06f,.038f},panel={.095f,.12f,.075f},muted={.68f,.74f,.59f},white={.94f,.94f,.8f},blue={.18f,.65f,.95f},red={.95f,.28f,.21f},gold={1,.75f,.28f},green={.42f,.85f,.49f};
static void vertex(Vec p,Vec c){
    if(nv>=capacity){game.overflow=1;return;}
    vertices[nv++]=(Vertex){p.x,p.y,p.z,c.x,c.y,c.z};
}
static void tri(Vec a,Vec b,Vec c,Vec color){
    if(nv+3>capacity){game.overflow=1;return;}
    vertex(a,color);vertex(b,color);vertex(c,color);
}
static Vec rotate(Vec p,float yaw,float pitch,float bank){
    float c=cosf(bank),s=sinf(bank),x=p.x*c-p.y*s,y=p.x*s+p.y*c;
    p.x=x;p.y=y;c=cosf(pitch);s=sinf(pitch);
    y=p.y*c+p.z*s;float z=-p.y*s+p.z*c;p.y=y;p.z=z;
    c=cosf(yaw);s=sinf(yaw);return V(p.x*c+p.z*s,p.y,-p.x*s+p.z*c);
}
static void box(Vec center,Vec size,float yaw,float pitch,float bank,Vec color){
    float cy=cosf(yaw),sy=sinf(yaw),cp=cosf(pitch),sp=sinf(pitch),cb=cosf(bank),sb=sinf(bank);
    Vec p[8];
    for(int i=0;i<8;i++){
        float x=(i&1?1:-1)*size.x*.5f,y=(i&2?1:-1)*size.y*.5f,z=(i&4?1:-1)*size.z*.5f;
        float bx=x*cb-y*sb,by=x*sb+y*cb,py=by*cp+z*sp,pz=-by*sp+z*cp;
        p[i]=add(center,V(bx*cy+pz*sy,py,-bx*sy+pz*cy));
    }
    static const int face[6][4]={{0,1,3,2},{4,6,7,5},{0,4,5,1},{2,3,7,6},{0,2,6,4},{1,5,7,3}};
    static const float shade[6]={.72f,.9f,.5f,1,.62f,.82f};
    for(int f=0;f<6;f++){const int *q=face[f];Vec c=mul(color,shade[f]);tri(p[q[0]],p[q[1]],p[q[2]],c);tri(p[q[0]],p[q[2]],p[q[3]],c);}
}
static void ground(float x,float z,float w,float d,float y,Vec color){
    Vec a=V(x-w*.5f,y,z-d*.5f),b=V(x+w*.5f,y,z-d*.5f),c=V(x+w*.5f,y,z+d*.5f),e=V(x-w*.5f,y,z+d*.5f);
    tri(a,b,c,color);tri(a,c,e,color);
}
static void pyramid(Vec p,float w,float d,float h,Vec color){
    Vec a=add(p,V(-w*.5f,0,-d*.5f)),b=add(p,V(w*.5f,0,-d*.5f)),c=add(p,V(w*.5f,0,d*.5f)),e=add(p,V(-w*.5f,0,d*.5f)),tip=add(p,V(0,h,0));
    tri(a,b,tip,mul(color,.82f));tri(b,c,tip,color);tri(c,e,tip,mul(color,.74f));tri(e,a,tip,mul(color,.62f));
}
static void rect(float x,float y,float w,float h,Vec c){
    if(w<=0||h<=0)return;
    tri(V(x,y,0),V(x+w,y,0),V(x+w,y+h,0),c);tri(V(x,y,0),V(x+w,y+h,0),V(x,y+h,0),c);
}
static void outline(float x,float y,float w,float h,Vec c){rect(x,y,w,1,c);rect(x,y+h-1,w,1,c);rect(x,y,1,h,c);rect(x+w-1,y,1,h,c);}
/* Compact 5x7 bitmap alphabet. Horizontal runs use one quad per run. */
static const unsigned char glyphs[36][5]={
 {0x3e,0x51,0x49,0x45,0x3e},{0,0x42,0x7f,0x40,0},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},{0x18,0x14,0x12,0x7f,0x10},
 {0x27,0x45,0x45,0x45,0x39},{0x3c,0x4a,0x49,0x49,0x30},{1,0x71,9,5,3},{0x36,0x49,0x49,0x49,0x36},{6,0x49,0x49,0x29,0x1e},
 {0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},{0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},{0x7f,0x49,0x49,0x49,0x41},
 {0x7f,9,9,9,1},{0x3e,0x41,0x49,0x49,0x7a},{0x7f,8,8,8,0x7f},{0,0x41,0x7f,0x41,0},{0x20,0x40,0x41,0x3f,1},
 {0x7f,8,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},{0x7f,2,0x0c,2,0x7f},{0x7f,4,8,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},
 {0x7f,9,9,9,6},{0x3e,0x41,0x51,0x21,0x5e},{0x7f,9,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{1,1,0x7f,1,1},
 {0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},{0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,8,0x14,0x63},{7,8,0x70,8,7},{0x61,0x51,0x49,0x45,0x43}
};
static void text(float x,float y,float scale,Vec c,const char *s){
    scale=scale>=1.5f?2:1;x=floorf(x+.5f);y=floorf(y+.5f);
    for(;*s;s++,x+=6*scale){
        unsigned char g[5]={0};int ch=*s;if(ch>='a'&&ch<='z')ch-=32;
        if(ch>='0'&&ch<='9')memcpy(g,glyphs[ch-'0'],5);
        else if(ch>='A'&&ch<='Z')memcpy(g,glyphs[ch-'A'+10],5);
        else switch(ch){
            case '-':g[0]=g[1]=g[2]=g[3]=g[4]=8;break;
            case '.':g[2]=0x60;break;case ':':g[2]=0x36;break;
            case '/':g[0]=0x20;g[1]=0x10;g[2]=8;g[3]=4;g[4]=2;break;
            case '+':g[0]=g[1]=g[3]=g[4]=8;g[2]=0x3e;break;
            case '%':g[0]=0x63;g[1]=0x13;g[2]=8;g[3]=0x64;g[4]=0x63;break;
            case '<':g[1]=8;g[2]=0x14;g[3]=0x22;break;
            case '>':g[1]=0x22;g[2]=0x14;g[3]=8;break;
            case '!':g[2]=0x5f;break;case '[':g[1]=0x7f;g[2]=g[3]=0x41;break;
            case ']':g[1]=g[2]=0x41;g[3]=0x7f;break;default:break;
        }
        for(int row=0;row<7;row++)for(int col=0;col<5;){
            if(!(g[col]&(1<<row))){col++;continue;}
            int start=col;while(col<5&&(g[col]&(1<<row)))col++;
            rect(x+start*scale,y+row*scale,(col-start)*scale,scale,c);
        }
    }
}
static void centered(float x,float y,float scale,Vec c,const char *s){scale=scale>=1.5f?2:1;text(x-strlen(s)*3*scale,y,scale,c,s);}
static void bar(float x,float y,float w,float h,float fraction,Vec color){rect(x,y,w,h,V(.13f,.17f,.19f));rect(x,y,w*clampf(fraction,0,1),h,color);}
static int cellFor(float x,float z){
    int cx=(int)((x+game.edge)/(2*game.edge)*GRID),cz=(int)((z+game.edge)/(2*game.edge)*GRID);
    cx=(int)clampf(cx,0,GRID-1);cz=(int)clampf(cz,0,GRID-1);return cz*GRID+cx;
}
static void buildingMesh(const Building *b){
    Vec wall=b->style==0?V(.66f,.62f,.49f):b->style==1?V(.53f,.57f,.53f):b->style==2?V(.65f,.48f,.36f):V(.59f,.59f,.55f);
    box(V(b->x,b->h*.5f,b->z),V(b->w,b->h,b->d),0,0,0,wall);
    pyramid(V(b->x,b->h,b->z),b->w+2,b->d+2,b->h*.35f,V(.32f,.3f,.27f));
    ground(b->x,b->z,b->w+8,b->d+8,.045f,V(.43f,.42f,.32f));
    for(int side=-1;side<=1;side+=2){
        for(int j=-1;j<=1;j++)box(V(b->x+j*b->w*.3f,b->h*.6f,b->z+side*(b->d*.5f+.03f)),V(2,2,.12f),0,0,0,V(.18f,.24f,.25f));
    }
    box(V(b->x,b->h*.2f,b->z+b->d*.5f+.08f),V(3,b->h*.4f,.2f),0,0,0,V(.26f,.25f,.21f));
}
static void treeMesh(const Tree *t){
    float s=t->size;box(add(t->p,V(0,s*.3f,0)),V(.8f,s*.6f,.8f),0,0,0,V(.32f,.25f,.15f));
    Vec col=t->style?V(.22f,.35f,.19f):V(.3f,.4f,.19f);
    pyramid(add(t->p,V(0,s*.3f,0)),s*.9f,s*.9f,s,col);
    pyramid(add(t->p,V(0,s*.65f,0)),s*.65f,s*.65f,s*.7f,mul(col,1.1f));
}
void rebuildScene(void){
    /* A new battle must not rewrite scenery still in use by the GPU. */
    C3D_FrameSync();
    vertices=terrain;capacity=STATIC_CAP;nv=0;game.seed=12389;
    float cell=game.edge*2/GRID;
    for(int z=0;z<GRID;z++)for(int x=0;x<GRID;x++){
        int index=z*GRID+x;Chunk *chunk=&chunks[index];chunk->first=nv;
        float cx=-game.edge+(x+.5f)*cell,cz=-game.edge+(z+.5f)*cell;
        chunk->center=V(cx,18,cz);chunk->radius=cell*.78f+35;
        Vec grass=V(.36f+randf()*.045f,.43f+randf()*.035f,.25f+randf()*.03f);
        ground(cx,cz,cell,cell,0,grass);
        if(x!=4&&x!=5&&z%3==1){
            ground(cx,cz,cell*.78f,cell*.72f,.02f,V(.57f,.52f,.29f));
            for(int k=0;k<6;k++)ground(cx-cell*.34f+k*cell*.13f,cz,cell*.025f,cell*.72f,.035f,V(.41f,.4f,.22f));
        }
        if(x==4||x==5)ground(x==4?-8:8,cz,16,cell,.06f,V(.36f,.35f,.3f));
        if(z==4||z==5)ground(cx,z==4?-8:8,cell,16,.08f,V(.36f,.35f,.3f));
        if(x==5)for(int k=0;k<4;k++)ground(0,cz-cell*.375f+k*cell*.25f,1.1f,cell*.1f,.10f,V(.72f,.68f,.48f));
        for(int i=0;i<BUILDINGS;i++)if(cellFor(game.buildings[i].x,game.buildings[i].z)==index)buildingMesh(&game.buildings[i]);
        for(int i=0;i<TREES;i++)if(cellFor(game.trees[i].p.x,game.trees[i].p.z)==index)treeMesh(&game.trees[i]);
        /* Scenic rock clusters at the rural edges, with no extra collision scans. */
        if(x==0||x==9)for(int i=0;i<4;i++)pyramid(V(cx+(randf()-.5f)*cell,0,cz+(randf()-.5f)*cell),14,19,6+randf()*8,V(.43f,.45f,.4f));
        chunk->count=nv-chunk->first;
    }
    mountainStart=nv;
    /* A continuous 360-degree range with layered, shaded, snow-capped peaks. */
    for(int i=0;i<40;i++){
        float a=i*PI/20,r=game.edge+210+(i%3)*60,h=100+randf()*160;
        Vec p=V(sinf(a)*r,-8,cosf(a)*r);float w=210+randf()*140;
        pyramid(p,w,w,h,V(.38f,.45f,.47f));
        if(h>170)pyramid(add(p,V(0,h*.72f,0)),w*.28f,w*.28f,h*.28f,V(.77f,.81f,.78f));
    }
    terrainCount=nv;GSPGPU_FlushDataCache(terrain,(u32)(nv*sizeof(Vertex)));cameraReady=0;
}
static int visible(Vec center,float radius,float distance){
    Vec delta=sub(center,eye);if(dot(delta,delta)>(distance+radius)*(distance+radius))return 0;
    C3D_FVec p=Mtx_MultiplyFVecH(&view,FVec3_New(center.x,center.y,center.z));float depth=-p.z;
    if(depth< -radius)return 0;
    float t=fmaxf(0,depth)*viewTan;
    return fabsf(p.x)<t*1.66667f+radius*1.6f&&fabsf(p.y)<t+radius*1.6f;
}
static void vehicle(const Actor *a,int detailed){
    Vec color=a->team?V(.59f,.29f,.21f):V(.31f,.44f,.29f);
    Vec marker=a->team?red:blue;
    if(a->plane){
        box(a->p,V(1.8f,1.6f,10),a->yaw,a->pitch,a->bank,color);
        box(add(a->p,rotate(V(0,0,.6f),a->yaw,a->pitch,a->bank)),V(14,.25f,2.8f),a->yaw,a->pitch,a->bank,color);
        box(add(a->p,rotate(V(0,.3f,-3.8f),a->yaw,a->pitch,a->bank)),V(5.2f,.25f,1.6f),a->yaw,a->pitch,a->bank,color);
        box(add(a->p,rotate(V(0,1.1f,-3.5f),a->yaw,a->pitch,a->bank)),V(.25f,2.6f,1.8f),a->yaw,a->pitch,a->bank,color);
        if(detailed){
            box(add(a->p,rotate(V(0,1,1.1f),a->yaw,a->pitch,a->bank)),V(1.3f,.8f,2.8f),a->yaw,a->pitch,a->bank,V(.14f,.25f,.3f));
            box(add(a->p,rotate(V(0,0,5.3f),a->yaw,a->pitch,a->bank)),V(5,.12f,.18f),a->yaw,a->pitch,a->bank+game.elapsed*50,V(.21f,.22f,.2f));
            for(int s=-1;s<=1;s+=2)box(add(a->p,rotate(V(s*4.8f,.17f,.7f),a->yaw,a->pitch,a->bank)),V(1.8f,.09f,1.5f),a->yaw,a->pitch,a->bank,marker);
        }
    }else{
        box(a->p,V(4.8f,1.7f,7.7f),a->yaw,0,0,color);
        for(int s=-1;s<=1;s+=2){
            box(add(a->p,rotate(V(s*2.5f,-.35f,0),a->yaw,0,0)),V(1.2f,1.8f,8),a->yaw,0,0,V(.17f,.18f,.15f));
            if(detailed)for(int j=-2;j<=2;j++)box(add(a->p,rotate(V(s*3.12f,-.3f,j*1.4f),a->yaw,0,0)),V(.12f,1,1),a->yaw,0,0,V(.28f,.29f,.23f));
        }
        box(add(a->p,V(0,1.4f,0)),V(3.3f,1.4f,3.8f),a->turret,0,0,color);
        box(add(add(a->p,V(0,1.5f,0)),mul(direction(a->turret,a->elev),3.5f)),V(.4f,.4f,5.4f),a->turret,a->elev,0,mul(color,.75f));
        if(detailed){
            box(add(a->p,V(0,2.2f,0)),V(1.3f,.3f,1.3f),a->turret,0,0,mul(color,.7f));
            box(add(a->p,rotate(V(-1.3f,3.0f,-1.2f),a->turret,0,0)),V(.09f,2.7f,.09f),a->turret,0,0,V(.2f,.22f,.18f));
            box(add(a->p,rotate(V(0,.3f,-3.92f),a->yaw,0,0)),V(2,.7f,.12f),a->yaw,0,0,marker);
        }
    }
}
void resetCamera(void){cameraReady=0;zoomAmount=0;viewTan=.5773503f;}
static void updateCamera(float dt){
    Actor *a=&game.actors[0];float desired=a->plane?a->yaw:a->turret;
    if(!cameraReady)cameraYaw=desired;
    cameraYaw=turn(cameraYaw,desired,dt*5);
    Vec focus=add(a->p,V(0,a->plane?1:2.5f,0));
    /* Tank camera pitch is constant and never uses cannon elevation or stick Y. */
    float pitch=a->plane?a->pitch*.45f:0;
    Vec back=direction(cameraYaw,pitch);float distance=a->plane?(game.closeCamera?21:31):(game.closeCamera?16:24);
    Vec wanted=add(sub(focus,mul(back,distance)),V(0,a->plane?7:9,0));
    float hit=traceWorld(focus,wanted,1.2f);
    if(hit<1)wanted=add(focus,mul(sub(wanted,focus),fmaxf(.1f,hit-.06f)));
    zoomAmount+=( (game.aiming?1.0f:0)-zoomAmount)*(1-expf(-dt*14));
    Vec aimDirection=direction(a->plane?a->yaw:a->turret,a->plane?a->pitch:a->elev);
    /* Aircraft zoom changes the lens, never crosses the fuselage. */
    Vec zoomEye=a->plane?wanted:add(sub(focus,mul(aimDirection,8)),V(0,3,0));
    float zoomHit=traceWorld(focus,zoomEye,1);
    if(zoomHit<1)zoomEye=add(focus,mul(sub(zoomEye,focus),fmaxf(.1f,zoomHit-.06f)));
    wanted=add(wanted,mul(sub(zoomEye,wanted),zoomAmount));
    wanted.y=fmaxf(wanted.y,3);
    if(!cameraReady||!a->plane){eye=wanted;cameraReady=1;}
    else eye=add(eye,mul(sub(wanted,eye),1-expf(-dt*10)));
    if(a->plane&&game.mode!=MODE_MENU){
        /* Lag during a fast turn must not drag the chase camera through the model. */
        float behind=dot(sub(eye,a->p),aimDirection);
        if(behind>-14)eye=sub(eye,mul(aimDirection,behind+14));
        if(length(sub(eye,a->p))<16)eye=add(sub(a->p,mul(aimDirection,18)),V(0,5,0));
        eye.y=fmaxf(eye.y,3);
        float clearance=dot(sub(eye,a->p),aimDirection);
        if(clearance>-14){
            float horizontal2=aimDirection.x*aimDirection.x+aimDirection.z*aimDirection.z;
            float shift=(clearance+14)/fmaxf(horizontal2,.1f);
            eye.x-=aimDirection.x*shift;eye.z-=aimDirection.z*shift;
        }
    }
    look=add(focus,mul(direction(cameraYaw,a->plane?a->pitch*.7f:0),40));
    look=add(look,mul(sub(add(focus,mul(aimDirection,300)),look),zoomAmount));
    if(game.mode==MODE_MENU){float a0=sceneTime*.08f;eye=V(sinf(a0)*175,72,cosf(a0)*175);look=V(0,7,0);}
    forward=sub(look,eye);forward=mul(forward,1/fmaxf(length(forward),.001f));
    Mtx_LookAt(&view,FVec3_New(eye.x,eye.y,eye.z),FVec3_New(look.x,look.y,look.z),FVec3_New(0,1,0),false);
    C3D_Mtx projection;viewTan=tanf((PI/3-zoomAmount*PI/6)*.5f);Mtx_PerspTilt(&projection,PI/3-zoomAmount*PI/6,400.0f/240,.5f,game.edge*3.5f,false);Mtx_Multiply(&mvp,&projection,&view);
}
static int project(Vec world,float *x,float *y){
    C3D_FVec p=Mtx_MultiplyFVecH(&view,FVec3_New(world.x,world.y,world.z));if(p.z>=-.5f)return 0;
    *x=200+p.x/(-p.z*viewTan*1.6666667f)*200;*y=120-p.y/(-p.z*viewTan)*120;
    return *x>=5&&*x<395&&*y>=20&&*y<220;
}
static void dynamicWorld(void){
    for(int i=0;i<OBJECTIVES;i++){
        Objective *o=&game.objectives[i];Vec color=o->owner==0?blue:o->owner==1?red:gold;
        if(!game.air){
            for(int k=0;k<28;k++){
                float a=k*PI/14,b=(k+.7f)*PI/14;
                tri(add(o->p,V(sinf(a)*43,.14f,cosf(a)*43)),add(o->p,V(sinf(b)*43,.14f,cosf(b)*43)),add(o->p,V(sinf(a)*41,.14f,cosf(a)*41)),color);
            }
            box(add(o->p,V(0,9,0)),V(.3f,18,.3f),0,0,0,muted);
            float wave=sinf(game.elapsed*3+i)*.18f;
            box(add(o->p,V(3,16,0)),V(6,3.4f,.12f),wave,0,0,color);
        }
    }
    for(int i=0;i<game.count;i++){
        Actor *a=&game.actors[i];if(a->hp<=0||!visible(a->p,12,game.air?1200:600))continue;
        vehicle(a,horizontal(sub(a->p,eye))<160);
        if(a->hp<maxHP(a)*.35f&&((int)(game.elapsed*6)+i)%3==0)pyramid(add(a->p,V(0,4,0)),2,2,3,V(.19f,.2f,.18f));
        if(!a->plane&&fabsf(a->speed)>9&&horizontal(sub(a->p,eye))<100){
            Vec p=add(a->p,mul(direction(a->yaw,0),-6));float s=2+sinf(game.elapsed*12+i)*.5f;
            pyramid(add(p,V(0,-1,0)),s*2,s*2,s,V(.59f,.55f,.38f));
        }
    }
    for(int i=0;i<MAX_SHOTS;i++)if(game.shots[i].life>0){
        Shot *s=&game.shots[i];Vec dir=mul(s->v,1/fmaxf(length(s->v),1));Vec side=V(dir.z*.12f,0,-dir.x*.12f);
        tri(add(s->p,side),sub(s->p,side),sub(s->p,mul(dir,game.air?5:3)),gold);
    }
    for(int i=0;i<MAX_FX;i++)if(game.fx[i].life>0){
        Effect *e=&game.fx[i];float s=e->size*(e->smoke?1+(2.5f-e->life)*.4f:1.6f-e->life);
        Vec color=e->smoke?V(.28f,.29f,.27f):V(1,.3f+e->life*.6f,.08f);
        pyramid(e->p,s,s,s,color);
    }
    for(int i=0;i<8;i++){
        float x=sinf(i*2.7f)*game.edge+fmodf(game.elapsed*2,200)-100,z=cosf(i*2.7f)*game.edge;
        Vec p=V(x,300+(i%3)*30,z);
        if(visible(p,80,game.edge*2)){box(p,V(95,9,30),0,0,0,V(.8f,.84f,.8f));box(add(p,V(20,6,0)),V(45,10,24),0,0,0,V(.87f,.89f,.84f));}
    }
}
static void targetMarkers(void){
    for(int i=1;i<game.count;i++){
        Actor *a=&game.actors[i];if(a->hp<=0)continue;
        float distance=length(sub(a->p,game.actors[0].p));if(distance>(game.air?900:380))continue;
        if(a->team&&traceWorld(eye,add(a->p,V(0,2,0)),0)<.995f)continue;
        float x,y;if(!project(add(a->p,V(0,a->plane?5:6,0)),&x,&y))continue;
        Vec col=a->team?red:blue;char label[16];snprintf(label,sizeof(label),"%dM",(int)distance);
        rect(x-3,y,6,2,col);rect(x-1,y-2,2,6,col);
        if(a->team){centered(x,y-11,1,col,label);bar(x-10,y+7,20,2,a->hp/maxHP(a),col);}
    }
    if(!game.air)for(int i=0;i<OBJECTIVES;i++){
        float x,y;Objective *o=&game.objectives[i];if(!project(add(o->p,V(0,21,0)),&x,&y))continue;
        Vec col=o->owner==0?blue:o->owner==1?red:gold;char label[16];snprintf(label,sizeof(label),"%c %dM",'A'+i,(int)horizontal(sub(o->p,game.actors[0].p)));
        centered(x,y,1,col,label);
    }
}
static void topUI(void){
    char s[80];Actor *a=&game.actors[0];
    if(game.mode==MODE_MENU){
        rect(0,0,400,44,navy);centered(200,8,2,white,"WAR THUNDER 3DS");centered(200,29,1,muted,"FIELD OPERATIONS / V2.3");
        rect(20,68,360,111,navy);outline(20,68,360,111,blue);
        centered(200,82,2,white,game.selection?"AIR SUPERIORITY":"ARMORED FRONT");
        centered(200,110,2,gold,game.selection?"5 VS 5 AIRCRAFT":"6 VS 6 TANKS");
        centered(200,141,2,white,"< SELECT OPERATION >");
        rect(0,211,400,29,navy);centered(200,218,2,gold,"A DEPLOY / START EXIT");return;
    }
    targetMarkers();
    for(int side=0;side<2;side++){
        float x0=side?386:6;rect(x0,67,8,1,muted);rect(x0,67,1,24,muted);rect(x0,173,8,1,muted);
    }
    int heading=((int)(-a->yaw*180/PI)+360)%360;
    snprintf(s,sizeof(s),"HDG %03d",heading);text(8,44,1,white,s);
    for(int tick=-3;tick<=3;tick++)rect(342+tick*12,43,1,tick==0?10:5,muted);
    rect(5,4,390,31,navy);
    snprintf(s,sizeof(s),"BLUE %03d",(int)fmaxf(0,game.tickets[0]));text(12,9,2,blue,s);
    snprintf(s,sizeof(s),"RED %03d",(int)fmaxf(0,game.tickets[1]));text(300,9,2,red,s);
    snprintf(s,sizeof(s),"%02d:%02d",(int)(720-game.elapsed)/60,(int)(720-game.elapsed)%60);centered(200,9,2,white,s);
    bar(12,27,96,3,game.tickets[0]/200,blue);bar(300,27,83,3,game.tickets[1]/200,red);
    if(!game.air)for(int i=0;i<OBJECTIVES;i++){
        Objective *o=&game.objectives[i];Vec color=o->owner==0?blue:o->owner==1?red:gold;
        rect(163+i*27,36,22,22,navy);s[0]='A'+i;s[1]=0;text(169+i*27,40,2,color,s);
    }
    float x=200,y=120;Vec aim=add(add(a->p,V(0,a->plane?-.2f:1.5f,0)),mul(direction(a->plane?a->yaw:a->turret,a->plane?a->pitch:a->elev),game.air?450:300));
    if(!a->plane){float flight=300.0f/245;aim.y-=.5f*9.8f*flight*flight;}
    float px,py;if(project(aim,&px,&py)){x=px;y=py;}
    Vec color=game.hitMarker>0?red:a->reload>.15f?white:gold;
    rect(x-14,y,8,2,color);rect(x+7,y,8,2,color);rect(x,y-14,2,8,color);rect(x,y+7,2,8,color);rect(x,y,2,2,color);
    if(game.aiming){rect(94,164,212,22,navy);outline(94,164,212,22,muted);centered(200,169,2,gold,game.air?"ZOOM SIGHT":"GUNNER CONTROL");}
    if(game.hitMarker>0)outline(x-8,y-8,18,18,red);
    rect(5,203,135,33,navy);snprintf(s,sizeof(s),"HULL %03d%%",(int)(a->hp/maxHP(a)*100));text(10,207,2,white,s);bar(10,226,122,5,a->hp/maxHP(a),green);
    rect(272,203,123,33,navy);snprintf(s,sizeof(s),"%03d KM/H",(int)(fabsf(a->speed)*3.6f));text(282,207,2,white,s);bar(282,226,104,5,1-a->reload/reloadTime(a),gold);
    if(game.messageTime>0){rect(4,185,392,15,navy);centered(200,189,1,gold,game.message);}
    if(game.damageFlash>0){rect(0,0,400,3,red);rect(0,237,400,3,red);rect(0,0,3,240,red);rect(397,0,3,240,red);}
    if(a->hp<=0){rect(42,80,316,69,navy);centered(200,91,2,red,"VEHICLE LOST");snprintf(s,sizeof(s),"RESPAWN IN %.0F",ceilf(a->respawn));centered(200,121,2,white,s);}
    if(game.mode==MODE_PAUSE||game.mode==MODE_RESULT){
        rect(30,69,340,105,navy);outline(30,69,340,105,muted);
        centered(200,82,2,white,game.mode==MODE_PAUSE?"PAUSED":game.winner?"DEFEAT":"VICTORY");
        snprintf(s,sizeof(s),"%d KILLS / %d LOSSES",game.kills,game.deaths);centered(200,110,2,white,s);
        centered(200,138,2,gold,game.mode==MODE_PAUSE?"START RESUME":"A REMATCH");centered(200,158,1,white,"SELECT RETURN TO MENU");
    }
}
static void tacticalMap(float x,float y,float size){
    rect(x,y,size,size,V(.1f,.16f,.14f));outline(x,y,size,size,muted);outline(x+3,y+3,size-6,size-6,V(.25f,.31f,.2f));
    for(int i=1;i<5;i++){rect(x+size*i/5,y,1,size,V(.15f,.22f,.2f));rect(x,y+size*i/5,size,1,V(.15f,.22f,.2f));}
    rect(x+size*.5f-2,y,4,size,V(.28f,.3f,.25f));rect(x,y+size*.5f-2,size,4,V(.28f,.3f,.25f));
    for(int i=0;i<BUILDINGS;i++){
        Building *b=&game.buildings[i];float bx=x+(b->x+game.edge)/(game.edge*2)*size,by=y+(game.edge-b->z)/(game.edge*2)*size;
        rect(bx-1,by-1,2,2,V(.4f,.43f,.35f));
    }
    if(!game.air)for(int i=0;i<OBJECTIVES;i++){
        Objective *o=&game.objectives[i];float px=x+(o->p.x+game.edge)/(game.edge*2)*size,py=y+(game.edge-o->p.z)/(game.edge*2)*size;
        Vec c=o->owner==0?blue:o->owner==1?red:gold;outline(px-6,py-6,13,13,c);char s[2]={(char)('A'+i),0};text(px-2,py-3,1,c,s);
    }
    for(int i=game.count-1;i>=0;i--){
        Actor *a=&game.actors[i];if(a->hp<=0)continue;
        /* Enemy blips are visible inside a local detection radius. */
        if(a->team&&horizontal(sub(a->p,game.actors[0].p))>(game.air?850:420))continue;
        float px=x+(a->p.x+game.edge)/(game.edge*2)*size,py=y+(game.edge-a->p.z)/(game.edge*2)*size;
        Vec c=i==0?white:a->team?red:blue;
        Vec tip=V(px+sinf(a->yaw)*4,py-cosf(a->yaw)*4,0),left=V(px-sinf(a->yaw+.8f)*3,py+cosf(a->yaw+.8f)*3,0),right=V(px-sinf(a->yaw-.8f)*3,py+cosf(a->yaw-.8f)*3,0);
        tri(tip,left,right,c);
        if(i==0)outline(px-5,py-5,11,11,white);
    }
    text(x+size*.5f-2,y+3,1,white,"N");
}
static void bottomUI(void){
    char s[80];Actor *a=&game.actors[0];rect(0,0,320,240,navy);
    rect(0,0,320,29,panel);text(8,7,2,white,game.mode==MODE_MENU?"OPERATIONS ORDER":"FIRE CONTROL");
    if(game.mode==MODE_MENU){
        text(10,40,2,game.selection?gold:blue,game.selection?"AIRCRAFT ONLY":"TANKS ONLY");
        text(10,66,2,white,game.selection?"DESTROY ENEMY AIRCRAFT":"CAPTURE ZONES A / B / C");
        text(10,94,2,muted,game.selection?"CIRCLE PAD: FLY":"L + CIRCLE PAD: AIM");
        text(10,117,2,white,"R FIRE / HOLD L ZOOM");
        text(10,140,2,white,game.selection?"A SPEED UP / B SLOW":"RELEASE L: DRIVE");
        text(10,163,2,muted,"Y CAMERA / START PAUSE");
        rect(8,201,304,31,panel);centered(160,210,2,gold,"TOUCH OR A TO DEPLOY");return;
    }
    tacticalMap(8,37,144);
    rect(162,37,150,62,panel);text(170,44,2,muted,"HULL");snprintf(s,sizeof(s),"%03d%%",(int)(a->hp/maxHP(a)*100));text(238,44,2,white,s);bar(170,68,134,7,a->hp/maxHP(a),green);
    text(170,83,2,gold,a->reload>.1f?"LOADING":"GUN READY");
    snprintf(s,sizeof(s),"KILLS %02d",game.kills);text(168,110,2,white,s);snprintf(s,sizeof(s),"LOST  %02d",game.deaths);text(168,133,2,muted,s);
    if(game.air){snprintf(s,sizeof(s),"ALT %03dM",(int)a->p.y);text(168,160,2,white,s);}
    else for(int i=0;i<OBJECTIVES;i++){
        Objective *o=&game.objectives[i];Vec c=o->contested?gold:o->owner==0?blue:o->owner==1?red:muted;
        float x=9+i*102;rect(x,189,96,25,panel);snprintf(s,sizeof(s),"%c %s",'A'+i,o->contested?"FIGHT":o->owner==0?"BLUE":o->owner==1?"RED":"OPEN");text(x+4,193,2,c,s);
    }
    rect(0,220,320,20,panel);centered(160,225,1,white,game.air?"L ZOOM / R FIRE / SELECT MENU":"HOLD L + STICK AIM / RELEASE L DRIVE");
    snprintf(s,sizeof(s),"%.0F FPS",game.fps);text(268,10,1,muted,s);
}
static void bindVertices(Vertex *data){C3D_BufInfo *b=C3D_GetBufInfo();BufInfo_Init(b);BufInfo_Add(b,data,sizeof(Vertex),2,0x10);}
int graphicsInit(void){
    if(!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE*2))return 0;
    top=C3D_RenderTargetCreate(240,400,GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8);
    bottom=C3D_RenderTargetCreate(240,320,GPU_RB_RGBA8,GPU_RB_DEPTH16);
    shader=DVLB_ParseFile((u32*)vshader_shbin,vshader_shbin_size);
    frame[0]=linearAlloc(FRAME_CAP*sizeof(Vertex));frame[1]=linearAlloc(FRAME_CAP*sizeof(Vertex));terrain=linearAlloc(STATIC_CAP*sizeof(Vertex));
    if(!top||!bottom||!shader||!frame[0]||!frame[1]||!terrain){graphicsShutdown();return 0;}
    C3D_RenderTargetSetOutput(top,GFX_TOP,GFX_LEFT,TRANSFER);C3D_RenderTargetSetOutput(bottom,GFX_BOTTOM,GFX_LEFT,TRANSFER);
    shaderProgramInit(&program);shaderProgramSetVsh(&program,&shader->DVLE[0]);programReady=1;C3D_BindProgram(&program);
    uniformLocation=shaderInstanceGetUniformLocation(program.vertexShader,"projection");
    C3D_AttrInfo *attr=C3D_GetAttrInfo();AttrInfo_Init(attr);AttrInfo_AddLoader(attr,0,GPU_FLOAT,3);AttrInfo_AddLoader(attr,1,GPU_FLOAT,3);
    C3D_TexEnv *env=C3D_GetTexEnv(0);C3D_TexEnvInit(env);C3D_TexEnvSrc(env,C3D_Both,GPU_PRIMARY_COLOR,0,0);C3D_TexEnvFunc(env,C3D_Both,GPU_REPLACE);
    C3D_CullFace(GPU_CULL_NONE);return 1;
}
void graphicsShutdown(void){
    C3D_FrameSync();if(programReady)shaderProgramFree(&program);if(shader)DVLB_Free(shader);
    if(top)C3D_RenderTargetDelete(top);
    if(bottom)C3D_RenderTargetDelete(bottom);
    if(frame[0])linearFree(frame[0]);
    if(frame[1])linearFree(frame[1]);
    if(terrain)linearFree(terrain);
    C3D_Fini();
}
void renderGame(float dt){
    if(!C3D_FrameBegin(C3D_FRAME_SYNCDRAW))return;
    sceneTime+=dt;updateCamera(dt);bufferIndex^=1;vertices=frame[bufferIndex];capacity=FRAME_CAP;nv=0;
    dynamicWorld();int worldCount=nv;topUI();int topCount=nv;bottomUI();int total=nv;
    GSPGPU_FlushDataCache(vertices,(u32)(nv*sizeof(Vertex)));
    C3D_RenderTargetClear(top,C3D_CLEAR_ALL,0x8CB7CCFF,0);C3D_FrameDrawOn(top);
    C3D_DepthTest(true,GPU_GREATER,GPU_WRITE_ALL);C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,uniformLocation,&mvp);bindVertices(terrain);
    C3D_DrawArrays(GPU_TRIANGLES,mountainStart,terrainCount-mountainStart);
    float distance=game.air?1500:520;
    for(int i=0;i<CHUNKS;i++)if(visible(chunks[i].center,chunks[i].radius,distance))C3D_DrawArrays(GPU_TRIANGLES,chunks[i].first,chunks[i].count);
    bindVertices(vertices);C3D_DrawArrays(GPU_TRIANGLES,0,worldCount);
    C3D_Mtx ortho;Mtx_OrthoTilt(&ortho,0,400,240,0,-1,1,false);C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,uniformLocation,&ortho);C3D_DepthTest(false,GPU_ALWAYS,GPU_WRITE_COLOR);C3D_DrawArrays(GPU_TRIANGLES,worldCount,topCount-worldCount);
    C3D_RenderTargetClear(bottom,C3D_CLEAR_ALL,0x0B1420FF,0);C3D_FrameDrawOn(bottom);Mtx_OrthoTilt(&ortho,0,320,240,0,-1,1,false);C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,uniformLocation,&ortho);C3D_DrawArrays(GPU_TRIANGLES,topCount,total-topCount);
    C3D_FrameEnd(0);
}
