#ifndef GRAPHICS_H
#define GRAPHICS_H
#include "game.h"
int graphicsInit(void);
void graphicsShutdown(void);
void rebuildScene(void);
void resetCamera(void);
void renderGame(float dt);
#endif
