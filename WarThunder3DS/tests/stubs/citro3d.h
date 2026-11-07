#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
typedef uint32_t u32;
typedef struct{float x,y,z,w;} C3D_FVec;
typedef struct{float m[4][4];} C3D_Mtx;
typedef struct{int width,height,id;} C3D_RenderTarget;
typedef struct{int dummy;} C3D_AttrInfo;
typedef struct{int dummy;} C3D_BufInfo;
typedef struct{int dummy;} C3D_TexEnv;
typedef struct{int DVLE[1];} DVLB_s;
typedef struct{void *vertexShader;} shaderProgram_s;
#define C3D_DEFAULT_CMDBUF_SIZE 1
#define C3D_FRAME_SYNCDRAW 0
#define C3D_CLEAR_ALL 0
#define GPU_RB_RGBA8 0
#define GPU_RB_DEPTH24_STENCIL8 0
#define GPU_RB_DEPTH16 0
#define GPU_FLOAT 0
#define GPU_PRIMARY_COLOR 0
#define GPU_REPLACE 0
#define GPU_CULL_NONE 0
#define GPU_GREATER 1
#define GPU_ALWAYS 0
#define GPU_WRITE_ALL 31
#define GPU_WRITE_COLOR 15
#define GPU_TRIANGLES 0
#define GFX_TOP 0
#define GFX_BOTTOM 1
#define GFX_LEFT 0
#define C3D_Both 0
#define true 1
#define false 0
#define GX_TRANSFER_FLIP_VERT(a) 0
#define GX_TRANSFER_OUT_TILED(a) 0
#define GX_TRANSFER_RAW_COPY(a) 0
#define GX_TRANSFER_IN_FORMAT(a) 0
#define GX_TRANSFER_OUT_FORMAT(a) 0
#define GX_TRANSFER_SCALING(a) 0
#define GSPGPU_FlushDataCache(a,b) ((void)0)
#define linearAlloc malloc
#define linearFree free
static inline C3D_FVec FVec3_New(float x,float y,float z){return (C3D_FVec){x,y,z,0};}
static inline C3D_FVec FVec4_New(float x,float y,float z,float w){return (C3D_FVec){x,y,z,w};}
C3D_FVec Mtx_MultiplyFVecH(const C3D_Mtx*,C3D_FVec);
void Mtx_LookAt(C3D_Mtx*,C3D_FVec,C3D_FVec,C3D_FVec,int);
void Mtx_PerspTilt(C3D_Mtx*,float,float,float,float,int);
void Mtx_OrthoTilt(C3D_Mtx*,float,float,float,float,float,float,int);
void Mtx_Multiply(C3D_Mtx*,const C3D_Mtx*,const C3D_Mtx*);
int C3D_Init(int);
void C3D_Fini(void);
C3D_RenderTarget *C3D_RenderTargetCreate(int,int,int,int);
void C3D_RenderTargetDelete(C3D_RenderTarget*);
void C3D_RenderTargetSetOutput(C3D_RenderTarget*,int,int,int);
DVLB_s *DVLB_ParseFile(u32*,int);
void DVLB_Free(DVLB_s*);
void shaderProgramInit(shaderProgram_s*);
void shaderProgramSetVsh(shaderProgram_s*,int*);
void shaderProgramFree(shaderProgram_s*);
void C3D_BindProgram(shaderProgram_s*);
int shaderInstanceGetUniformLocation(void*,const char*);
C3D_AttrInfo *C3D_GetAttrInfo(void);
void AttrInfo_Init(C3D_AttrInfo*);
void AttrInfo_AddLoader(C3D_AttrInfo*,int,int,int);
C3D_TexEnv *C3D_GetTexEnv(int);
void C3D_TexEnvInit(C3D_TexEnv*);
void C3D_TexEnvSrc(C3D_TexEnv*,int,int,int,int);
void C3D_TexEnvFunc(C3D_TexEnv*,int,int);
void C3D_CullFace(int);
void C3D_FrameSync(void);
int C3D_FrameBegin(int);
void C3D_FrameEnd(int);
void C3D_RenderTargetClear(C3D_RenderTarget*,int,unsigned,int);
void C3D_FrameDrawOn(C3D_RenderTarget*);
void C3D_DepthTest(int,int,int);
void C3D_FVUnifMtx4x4(int,int,const C3D_Mtx*);
C3D_BufInfo *C3D_GetBufInfo(void);
void BufInfo_Init(C3D_BufInfo*);
void BufInfo_Add(C3D_BufInfo*,void*,int,int,int);
void C3D_DrawArrays(int,int,int);
#define GPU_VERTEX_SHADER 0
