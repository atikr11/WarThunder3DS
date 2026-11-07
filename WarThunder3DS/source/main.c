/* WarThunder3DS - Valley Front. Original offline homebrew, Old 3DS compatible. */
#include "game.h"
#include "graphics.h"
static void deploy(void){startMatch(game.selection);rebuildScene();resetCamera();}
int main(void){
    gfxInitDefault();gfxSet3D(false);
    initGame();startMatch(0);game.mode=MODE_MENU;
    if(!graphicsInit()){gfxExit();return 1;}
    rebuildScene();initAudio();
    u64 last=osGetTime();float accumulator=0;
    while(aptMainLoop()){
        hidScanInput();u32 down=hidKeysDown(),held=hidKeysHeld();
        circlePosition circle;hidCircleRead(&circle);touchPosition touch;hidTouchRead(&touch);
        u64 now=osGetTime();float raw=(now-last)*.001f;last=now;
        /* Drop long suspend stalls, and simulate at a fixed 60 Hz. */
        float dt=clampf(raw,.001f,.10f);game.fps=game.fps*.94f+(.06f/fmaxf(raw,.001f));
        if(game.mode==MODE_MENU){
            if(down&KEY_START)break;
            if(down&(KEY_DLEFT|KEY_DRIGHT|KEY_CPAD_LEFT|KEY_CPAD_RIGHT))game.selection=!game.selection;
            if((down&KEY_TOUCH)&&touch.py>=207)deploy();
            else if(down&KEY_A)deploy();
        }else if(down&KEY_SELECT){game.mode=MODE_MENU;resetCamera();}
        else if(game.mode==MODE_RESULT){if(down&KEY_A)deploy();}
        else{
            if(down&KEY_START){game.mode=game.mode==MODE_PLAY?MODE_PAUSE:MODE_PLAY;accumulator=0;}
            if(down&KEY_Y)game.closeCamera=!game.closeCamera;
        }
        if(game.mode==MODE_PLAY){
            Controls controls={held,circle.dx/156.0f,circle.dy/156.0f};accumulator+=dt;
            while(accumulator>=1.0f/60){stepGame(1.0f/60,controls);accumulator-=1.0f/60;if(game.mode!=MODE_PLAY){accumulator=0;break;}}
        }else accumulator=0;
        updateAudio();renderGame(game.mode==MODE_PAUSE?0:dt);
    }
    stopAudio();graphicsShutdown();gfxExit();return 0;
}

