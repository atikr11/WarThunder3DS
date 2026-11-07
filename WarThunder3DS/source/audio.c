#include "game.h"
#define RATE 22050
static int ready,lastMode=-1;
static float lastEngineGain=-1,lastEngineRate=-1;
static s16 *samples[4],*engine;
static int counts[4];
static ndspWaveBuf waves[6],engineWave;
static float recent[4];
static u64 started[6];
static unsigned noiseSeed=1993;
static float noise(void){noiseSeed=noiseSeed*1664525u+1013904223u;return ((noiseSeed>>8)/8388608.0f)-1;}
void initAudio(void){
    if(R_FAILED(ndspInit()))return;
    ready=1;
    for(int kind=0;kind<4;kind++){
        counts[kind]=(int)(RATE*(kind==2?.65f:kind==0?.32f:.09f));samples[kind]=linearAlloc(counts[kind]*sizeof(s16));
        if(!samples[kind]){stopAudio();return;}
        float phase=0,low=0;
        for(int i=0;i<counts[kind];i++){
            float t=(float)i/RATE,env=expf(-t*(kind==2?6:kind==0?15:45));low+=(noise()-low)*(kind==2?.12f:.5f);
            float freq=kind==3?1800:kind==1?160:kind==2?45:75+100*expf(-t*30);phase+=2*PI*freq/RATE;
            float value=kind==3?sinf(phase):low*.65f+sinf(phase)*.4f;samples[kind][i]=(s16)(clampf(value*env,-1,1)*20000);
        }
        DSP_FlushDataCache(samples[kind],counts[kind]*sizeof(s16));
    }
    engine=linearAlloc(2205*sizeof(s16));if(!engine){stopAudio();return;}
    for(int i=0;i<2205;i++){float p=2*PI*i/2205;engine[i]=(s16)((sinf(p*7)*.6f+sinf(p*14)*.25f+sinf(p*21)*.15f)*5000);}
    DSP_FlushDataCache(engine,2205*sizeof(s16));ndspChnReset(6);ndspChnSetInterp(6,NDSP_INTERP_LINEAR);ndspChnSetRate(6,RATE);ndspChnSetFormat(6,NDSP_FORMAT_MONO_PCM16);
    memset(&engineWave,0,sizeof(engineWave));engineWave.data_vaddr=engine;engineWave.nsamples=2205;engineWave.looping=true;ndspChnWaveBufAdd(6,&engineWave);updateAudio();
}
void stopAudio(void){
    if(!ready)return;
    for(int i=0;i<=6;i++)ndspChnWaveBufClear(i);
    ndspExit();ready=0;
    for(int i=0;i<4;i++)if(samples[i]){linearFree(samples[i]);samples[i]=NULL;}
    if(engine){linearFree(engine);engine=NULL;}
}
void updateAudio(void){
    if(!ready)return;
    Actor *a=&game.actors[0];float gain=game.mode==MODE_PLAY&&a->hp>0?.28f:0;
    float rate=RATE*(a->plane?.8f+a->speed/150:.65f+fabsf(a->speed)/38);
    if(gain!=lastEngineGain){float mix[12]={gain,gain};ndspChnSetMix(6,mix);lastEngineGain=gain;}
    if(fabsf(rate-lastEngineRate)>100){ndspChnSetRate(6,rate);lastEngineRate=rate;}
    if(game.mode!=MODE_PLAY&&game.mode!=lastMode)for(int i=0;i<6;i++)ndspChnWaveBufClear(i);
    lastMode=game.mode;
}
void playSound(int kind,Vec p,float volume){
    if(!ready||kind<0||kind>3||!samples[kind])return;
    if(game.elapsed-recent[kind]<(kind==1?.07f:.025f)&&game.elapsed>recent[kind])return;
    recent[kind]=game.elapsed;Vec delta=sub(p,game.actors[0].p);float distance=length(delta);if(distance>420&&kind!=3)return;
    float gain=kind==3?volume:volume/(1+distance*.025f),pan=clampf(dot(delta,direction(game.actors[0].yaw+PI*.5f,0))/180,-.8f,.8f);
    float mix[12]={gain*(1-pan)*.6f,gain*(1+pan)*.6f};int channel=0;
    for(int i=0;i<6;i++){if(!ndspChnIsPlaying(i)){channel=i;break;}if(started[i]<started[channel])channel=i;}
    started[channel]=osGetTime();ndspChnReset(channel);ndspChnSetInterp(channel,NDSP_INTERP_LINEAR);ndspChnSetRate(channel,RATE);ndspChnSetFormat(channel,NDSP_FORMAT_MONO_PCM16);ndspChnSetMix(channel,mix);
    memset(&waves[channel],0,sizeof(waves[channel]));waves[channel].data_vaddr=samples[kind];waves[channel].nsamples=counts[kind];ndspChnWaveBufAdd(channel,&waves[channel]);
}
