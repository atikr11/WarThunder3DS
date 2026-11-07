#define HOST_TEST
#include "../source/game.c"
#include "../source/graphics.c"
#include <assert.h>
static FILE *dump;
static C3D_Mtx active;
static C3D_RenderTarget *activeTarget;
static int depthEnabled,colorMask;
static Vertex *bound;
void playSound(int k,Vec p,float v){(void)k;(void)p;(void)v;}
static C3D_FVec mv(const C3D_Mtx *m,C3D_FVec v){
    float a[4]={v.x,v.y,v.z,v.w},o[4]={0};for(int i=0;i<4;i++)for(int j=0;j<4;j++)o[i]+=m->m[i][j]*a[j];return FVec4_New(o[0],o[1],o[2],o[3]);
}
C3D_FVec Mtx_MultiplyFVecH(const C3D_Mtx *m,C3D_FVec v){v.w=1;return mv(m,v);}
static Vec cross(Vec a,Vec b){return V(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
static Vec normal(Vec a){return mul(a,1/length(a));}
void Mtx_LookAt(C3D_Mtx *m,C3D_FVec e,C3D_FVec t,C3D_FVec u,int lh){
    (void)lh;Vec eye0=V(e.x,e.y,e.z),target=V(t.x,t.y,t.z),up=V(u.x,u.y,u.z);
    Vec f=normal(sub(target,eye0)),r=normal(cross(f,up)),v=cross(r,f);memset(m,0,sizeof(*m));
    m->m[0][0]=r.x;m->m[0][1]=r.y;m->m[0][2]=r.z;m->m[0][3]=-dot(r,eye0);
    m->m[1][0]=v.x;m->m[1][1]=v.y;m->m[1][2]=v.z;m->m[1][3]=-dot(v,eye0);
    m->m[2][0]=-f.x;m->m[2][1]=-f.y;m->m[2][2]=-f.z;m->m[2][3]=dot(f,eye0);m->m[3][3]=1;
}
void Mtx_PerspTilt(C3D_Mtx *m,float fov,float aspect,float near,float far,int lh){
    (void)lh;memset(m,0,sizeof(*m));float f=1/tanf(fov*.5f);m->m[0][0]=f/aspect;m->m[1][1]=f;m->m[2][2]=-(far+near)/(far-near);m->m[2][3]=-2*far*near/(far-near);m->m[3][2]=-1;
}
void Mtx_OrthoTilt(C3D_Mtx *m,float left,float right,float bottom0,float top0,float near,float far,int lh){
    (void)lh;memset(m,0,sizeof(*m));m->m[0][0]=2/(right-left);m->m[0][3]=-(right+left)/(right-left);m->m[1][1]=2/(top0-bottom0);m->m[1][3]=-(top0+bottom0)/(top0-bottom0);m->m[2][2]=-2/(far-near);m->m[2][3]=-(far+near)/(far-near);m->m[3][3]=1;
}
void Mtx_Multiply(C3D_Mtx *out,const C3D_Mtx *a,const C3D_Mtx *b){memset(out,0,sizeof(*out));for(int i=0;i<4;i++)for(int j=0;j<4;j++)for(int k=0;k<4;k++)out->m[i][j]+=a->m[i][k]*b->m[k][j];}
int C3D_Init(int x){(void)x;return 1;}void C3D_Fini(void){}
C3D_RenderTarget *C3D_RenderTargetCreate(int h,int w,int fmt,int dep){(void)fmt;(void)dep;C3D_RenderTarget *t=malloc(sizeof(*t));*t=(C3D_RenderTarget){w,h,0};return t;}
void C3D_RenderTargetDelete(C3D_RenderTarget *t){free(t);}void C3D_RenderTargetSetOutput(C3D_RenderTarget *t,int screen,int side,int flags){(void)side;(void)flags;t->id=screen;}
DVLB_s *DVLB_ParseFile(u32 *data,int n){(void)data;(void)n;return malloc(sizeof(DVLB_s));}void DVLB_Free(DVLB_s *s){free(s);}
void shaderProgramInit(shaderProgram_s *s){s->vertexShader=NULL;}void shaderProgramSetVsh(shaderProgram_s *s,int *i){(void)s;(void)i;}void shaderProgramFree(shaderProgram_s *s){(void)s;}
void C3D_BindProgram(shaderProgram_s *s){(void)s;}int shaderInstanceGetUniformLocation(void *s,const char *n){(void)s;(void)n;return 0;}
C3D_AttrInfo *C3D_GetAttrInfo(void){static C3D_AttrInfo a;return &a;}void AttrInfo_Init(C3D_AttrInfo *a){(void)a;}void AttrInfo_AddLoader(C3D_AttrInfo *a,int b,int c,int d){(void)a;(void)b;(void)c;(void)d;}
C3D_TexEnv *C3D_GetTexEnv(int x){static C3D_TexEnv e;(void)x;return &e;}void C3D_TexEnvInit(C3D_TexEnv *e){(void)e;}void C3D_TexEnvSrc(C3D_TexEnv *a,int b,int c,int d,int e){(void)a;(void)b;(void)c;(void)d;(void)e;}void C3D_TexEnvFunc(C3D_TexEnv *a,int b,int c){(void)a;(void)b;(void)c;}
void C3D_CullFace(int a){(void)a;}void C3D_FrameSync(void){}int C3D_FrameBegin(int a){(void)a;return 1;}void C3D_FrameEnd(int a){(void)a;}
void C3D_RenderTargetClear(C3D_RenderTarget *a,int b,unsigned c,int d){(void)a;(void)b;(void)c;(void)d;}
void C3D_FrameDrawOn(C3D_RenderTarget *t){activeTarget=t;}void C3D_DepthTest(int enable,int op,int mask){(void)op;depthEnabled=enable;colorMask=mask;}
void C3D_FVUnifMtx4x4(int a,int b,const C3D_Mtx *m){(void)a;(void)b;active=*m;}
C3D_BufInfo *C3D_GetBufInfo(void){static C3D_BufInfo b;return &b;}void BufInfo_Init(C3D_BufInfo *b){(void)b;}
void BufInfo_Add(C3D_BufInfo *a,void *data,int stride,int count,int mask){(void)a;(void)stride;(void)count;(void)mask;bound=data;}
void C3D_DrawArrays(int mode0,int first,int count){
    (void)mode0;assert((colorMask&GPU_WRITE_COLOR)==GPU_WRITE_COLOR);if(!dump)return;
    for(int i=first;i<first+count;i+=3){
        float record[16]={0};record[0]=(float)activeTarget->id;record[1]=(float)depthEnabled;int good=1;
        for(int j=0;j<3;j++){
            Vertex *v=&bound[i+j];C3D_FVec clip=mv(&active,FVec4_New(v->x,v->y,v->z,1));
            if(clip.w<=.05f){good=0;break;}
            record[2+j*3]=(clip.x/clip.w+1)*activeTarget->width*.5f;
            record[3+j*3]=(1-clip.y/clip.w)*activeTarget->height*.5f;record[4+j*3]=clip.z/clip.w;
        }
        record[11]=bound[i].r;record[12]=bound[i].g;record[13]=bound[i].b;
        if(good)fwrite(record,sizeof(record),1,dump);
    }
}
static void captureFrame(const char *name){
    char path[100];snprintf(path,sizeof(path),"tests/output/%s.bin",name);dump=fopen(path,"wb");assert(dump);
    renderGame(1.0f/60);fclose(dump);dump=NULL;assert(!game.overflow);
    printf("%s: static %d / %d, frame %d / %d vertices\n",name,terrainCount,STATIC_CAP,nv,FRAME_CAP);
}
int main(void){
    initGame();startMatch(0);assert(graphicsInit());rebuildScene();game.mode=MODE_MENU;captureFrame("menu");
    game.mode=MODE_PLAY;resetCamera();captureFrame("tank");
    /* Stable tank camera pitch must survive forward motion and gun elevation. */
    float oldY=eye.y-game.actors[0].p.y,oldPitch=atan2f(look.y-eye.y,horizontal(sub(look,eye)));
    game.actors[0].p.z+=100;game.actors[0].elev=.6f;updateCamera(1.0f/60);
    assert(fabsf(eye.y-game.actors[0].p.y-oldY)<.001f);
    assert(fabsf(atan2f(look.y-eye.y,horizontal(sub(look,eye)))-oldPitch)<.001f);
    game.aiming=1;for(int i=0;i<60;i++)updateCamera(1.0f/60);assert(viewTan<.3f);captureFrame("zoom");
    game.aiming=0;for(int i=0;i<60;i++)updateCamera(1.0f/60);assert(viewTan>.57f);
    game.mode=MODE_PAUSE;captureFrame("pause");game.mode=MODE_RESULT;game.winner=0;captureFrame("result");
    startMatch(1);rebuildScene();captureFrame("air");game.aiming=1;for(int i=0;i<60;i++)updateCamera(1.0f/60);assert(viewTan<.3f);game.aiming=0;resetCamera();game.actors[0].hp=0;game.actors[0].respawn=5;captureFrame("destroyed");
        startMatch(0);rebuildScene();
    game.kills=999;game.deaths=999;
    for(int i=0;i<game.count;i++)game.actors[i].p=V((i%4-1)*14,1.5f,-320+(i/4)*20);
    for(int i=0;i<MAX_FX;i++)game.fx[i]=(Effect){V(i%8*3-10,4,-300-i/8*4),.5f,6,i%2};
    for(int i=0;i<MAX_SHOTS;i++)game.shots[i]=(Shot){V(i%12*2-10,3,-300-i/12*3),V(0,0,245),1,66,9.8f,0,0};
    resetCamera();captureFrame("stress");
    graphicsShutdown();puts("PASS: camera pitch regression and renderer vertex budgets, including peak effects");return 0;
}
