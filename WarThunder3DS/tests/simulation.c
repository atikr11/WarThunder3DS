#include "../source/game.h"
#include <assert.h>
void playSound(int kind,Vec p,float volume){(void)kind;(void)p;(void)volume;}
static void assertValid(void){
    for(int i=0;i<game.count;i++){
        Actor *a=&game.actors[i];assert(a->plane==game.air);assert(isfinite(a->p.x)&&isfinite(a->p.y)&&isfinite(a->p.z));
        assert(a->hp>=0&&a->hp<=maxHP(a));assert(fabsf(a->p.x)<=game.edge+1);assert(fabsf(a->p.z)<=game.edge+1);
    }
}
int main(void){
    initGame();startMatch(0);
    assert(game.count==12&&game.edge==600);
    playerControl(.1f,(Controls){0,1,0});assert(game.actors[0].yaw<0);
    spawnActor(0);playerControl(.1f,(Controls){0,-1,0});assert(game.actors[0].yaw>0);
    spawnActor(0);playerControl(.1f,(Controls){KEY_DRIGHT,0,0});assert(game.actors[0].turret<0);
    float gun=game.actors[0].turret;playerControl(.1f,(Controls){KEY_L,0,0});assert(game.aiming&&game.actors[0].turret==gun);
    playerControl(.1f,(Controls){0,0,0});assert(!game.aiming);
    startMatch(1);playerControl(.1f,(Controls){KEY_L,1,0});assert(game.actors[0].yaw<0&&game.actors[0].bank>0&&game.aiming);
    startMatch(0);
    float elev=game.actors[0].elev,yaw=game.actors[0].turret;
    /* Old KEY_UP combines bit 6 with Circle Pad bit 30. Only bit 30 is held here. */
    playerControl(.5f,(Controls){1u<<30,0,1});
    assert(game.actors[0].elev==elev);assert(game.actors[0].turret==yaw);assert(game.actors[0].speed>0);
    playerControl(.5f,(Controls){1u<<28,1,0});assert(game.actors[0].elev==elev);
    float relative=wrap(game.actors[0].turret-game.actors[0].yaw);assert(fabsf(relative)<.001f);
    playerControl(.5f,(Controls){KEY_DUP,0,0});assert(game.actors[0].elev>elev);
    for(int i=0;i<BUILDINGS;i++)game.buildings[i]=(Building){500,500,10,10,10,0};
    game.buildings[0]=(Building){0,10,10,4,20,0};
    float t=traceWorld(V(0,2,0),V(0,2,30),0);assert(fabsf(t-8.0f/30)<.001f);
    assert(traceWorld(V(15,2,0),V(15,2,30),0)==1);
    assert(traceWorld(V(0,30,0),V(0,30,30),0)==1);
    assert(fabsf(traceWorld(V(20,10,0),V(20,-10,0),0)-.5f)<.001f);
    startMatch(0);game.count=2;
    game.actors[0].p=game.objectives[0].p;game.actors[0].p.y=1.5f;
    game.actors[1].hp=0;game.actors[1].respawn=1000;
    for(int i=0;i<650;i++)stepGame(1.0f/60,(Controls){0});
    assert(game.objectives[0].owner==0);assert(game.tickets[1]<200);
    /* Broad phase must agree with the full building scan. */
    startMatch(0);
    for(int n=0;n<2000;n++){
        Vec a=V((randf()-.5f)*1200,randf()*100,(randf()-.5f)*1200);
        Vec b=V((randf()-.5f)*1200,randf()*100,(randf()-.5f)*1200);
        float radius=randf()*5,slow=1;
        for(int j=0;j<BUILDINGS;j++)slow=fminf(slow,boxTrace(a,b,&game.buildings[j],radius));
        if(b.y<radius&&b.y<a.y)slow=fminf(slow,(a.y-radius)/(a.y-b.y));
        assert(fabsf(traceWorld(a,b,radius)-clampf(slow,0,1))<.00001f);
    }
    /* A wall between the breech and muzzle cannot be skipped. */
    for(int i=0;i<BUILDINGS;i++)game.buildings[i]=(Building){500,500,10,10,10,0};
    game.actors[0].p=V(0,1.5f,0);game.actors[0].yaw=game.actors[0].turret=0;game.actors[0].elev=0;game.actors[0].reload=0;
    game.buildings[0]=(Building){0,3,10,1,10,0};fireWeapon(0);
    assert(game.shots[0].p.z<2.5f);updateShots(1.0f/60);assert(game.shots[0].life<=0);
    /* A contested owned flag must not drain either team's tickets. */
    startMatch(0);game.count=2;game.objectives[0].capture=1;game.objectives[0].owner=0;
    game.actors[0].p=add(game.objectives[0].p,V(-5,1.5f,0));
    game.actors[1].p=add(game.objectives[0].p,V(5,1.5f,0));
    game.actors[1].think=100;game.actors[1].desiredSpeed=0;game.actors[1].target=-1;
    stepGame(1.0f/60,(Controls){0});assert(game.objectives[0].contested);assert(game.tickets[1]==200);
    startMatch(1);assert(game.count==10&&game.edge==1000);
    for(int i=0;i<game.count;i++)assert(game.actors[i].plane);
    for(int air=0;air<2;air++){
        startMatch(air);int casualties=0,objectives=0;float previous[12];
        for(int i=0;i<game.count;i++)previous[i]=game.actors[i].hp;
        for(int step=0;step<60*720&&game.mode==MODE_PLAY;step++){
            /* Exercise the actual bot controller for the player as well. */
            game.actors[0].reload=fmaxf(0,game.actors[0].reload-1.0f/60);
            extern void hostBot(int id,float dt);hostBot(0,1.0f/60);
            stepGame(1.0f/60,(Controls){0,0,0});
            if(step%60==0)assertValid();
            for(int i=0;i<game.count;i++){if(game.actors[i].hp==0&&previous[i]>0)casualties++;previous[i]=game.actors[i].hp;}
            for(int i=0;i<OBJECTIVES;i++)if(game.objectives[i].owner>=0)objectives++;
        }
        printf("%s simulation: %.1fs, %d losses, tickets %.0f/%.0f, objective samples %d\n",air?"Air":"Tank",game.elapsed,casualties,game.tickets[0],game.tickets[1],objectives);
        assert(casualties>0);if(!air)assert(objectives>0);
    }
    puts("PASS: separated modes, joystick/D-pad regression, collision, capture, and full-match simulation");return 0;
}
