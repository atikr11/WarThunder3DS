#include "game.h"
Game game;
typedef struct {float reaction,think,turn,spread,cooldown,damage;} BotSkill;
static const BotSkill skills[3]={
    {1.0f,.55f,.65f,.065f,1.85f,.60f},
    {.5f,.30f,.85f,.030f,1.35f,.80f},
    {.15f,.16f,1.0f,.006f,1.0f,1.0f}
};
static const BotSkill *skillFor(int id){return &skills[game.actors[id].team?(int)clampf(game.difficulty,0,2):1];}
const char *difficultyName(void){return game.difficulty==0?"EASY":game.difficulty==1?"MEDIUM":"HARD";}

float randf(void){game.seed=game.seed*1664525u+1013904223u;return (game.seed>>8)*(1.0f/16777216);}
void announce(const char *message){snprintf(game.message,sizeof(game.message),"%s",message);game.messageTime=3.0f;}
static float axisNear(float start,float delta,float lo,float hi,float *exit){
    if(fabsf(delta)<.00001f)return start<lo||start>hi?2:-1;
    float a=(lo-start)/delta,b=(hi-start)/delta;
    if(a>b){float t=a;a=b;b=t;}
    *exit=fminf(*exit,b);return a;
}
/* Exact segment/AABB intersection. Used by projectiles, AI, and camera collision. */
static float boxTrace(Vec p,Vec end,const Building *b,float r){
    Vec d=sub(end,p);float leave=1,enter=0;
    enter=fmaxf(enter,axisNear(p.x,d.x,b->x-b->w*.5f-r,b->x+b->w*.5f+r,&leave));
    enter=fmaxf(enter,axisNear(p.y,d.y,-r,b->h*1.35f+r,&leave));
    enter=fmaxf(enter,axisNear(p.z,d.z,b->z-b->d*.5f-r,b->z+b->d*.5f+r,&leave));
    return enter<=leave&&enter<=1&&leave>=0?enter:1;
}
float traceWorld(Vec from,Vec to,float radius){
    float result=1;
    /* Cheap broad phase: reject buildings outside the segment bounds before divisions. */
    float loX=fminf(from.x,to.x)-radius,hiX=fmaxf(from.x,to.x)+radius;
    float loZ=fminf(from.z,to.z)-radius,hiZ=fmaxf(from.z,to.z)+radius;
    float loY=fminf(from.y,to.y)-radius;
    for(int i=0;i<BUILDINGS;i++){
        Building *b=&game.buildings[i];
        if(loY>b->h*1.35f||hiX<b->x-b->w*.5f||loX>b->x+b->w*.5f||hiZ<b->z-b->d*.5f||loZ>b->z+b->d*.5f)continue;
        result=fminf(result,boxTrace(from,to,b,radius));
        if(result<=0)break;
    }
    if(to.y<radius&&to.y<from.y)result=fminf(result,(from.y-radius)/(from.y-to.y));
    return clampf(result,0,1);
}
static int blocked(Vec p,float r){
    if(fabsf(p.x)>game.edge-r||fabsf(p.z)>game.edge-r)return 1;
    for(int i=0;i<BUILDINGS;i++){
        Building *b=&game.buildings[i];
        if(p.y<b->h*1.35f+r&&fabsf(p.x-b->x)<b->w*.5f+r&&fabsf(p.z-b->z)<b->d*.5f+r)return 1;
    }
    return 0;
}
static int nearObjective(float x,float z,float range){
    for(int i=0;i<OBJECTIVES;i++)if(horizontal(sub(V(x,0,z),game.objectives[i].p))<range)return 1;
    return 0;
}
static void generateMap(void){
    game.seed=7193;
    game.objectives[0]=(Objective){V(-240,0,-125),0,-1,0};
    game.objectives[1]=(Objective){V(0,0,0),0,-1,0};
    game.objectives[2]=(Objective){V(240,0,125),0,-1,0};
    for(int i=0;i<BUILDINGS;i++){
        float cx=(i/18-1)*240,cz=(i/18-1)*125;
        float x,z,w=12+randf()*17,d=14+randf()*18;
        do{x=cx+(randf()-.5f)*240;z=cz+(randf()-.5f)*220;}
        while(fabsf(x)<28||fabsf(z)<24||nearObjective(x,z,60));
        game.buildings[i]=(Building){x,z,w,d,7+randf()*15,i%4};
    }
    for(int i=0;i<TREES;i++){
        float x,z;
        do{x=(randf()-.5f)*game.edge*1.9f;z=(randf()-.5f)*game.edge*1.9f;}
        while(fabsf(x)<20||fabsf(z)<20||nearObjective(x,z,50)||blocked(V(x,1,z),6));
        game.trees[i]=(Tree){V(x,0,z),5+randf()*7,i%3};
    }
}
void initGame(void){memset(&game,0,sizeof(game));game.fps=30;game.assist=1;game.assistTarget=-1;game.mode=MODE_MENU;game.winner=-1;}
void spawnActor(int i){
    Actor *a=&game.actors[i];int slot=i/2;
    a->p=V((slot%3-1)*23+(a->team?11:0),a->plane?100+(slot%3)*23:1.5f,(a->team?1:-1)*game.edge*.72f+(slot/3)*24*(a->team?-1:1));
    a->yaw=a->team?PI:0;a->pitch=0;a->bank=0;a->turret=a->yaw;a->elev=.015f;
    a->hp=maxHP(a);a->speed=a->plane?85:0;a->reload=.8f;a->respawn=0;
    a->think=i*.021f;a->desiredYaw=a->yaw;a->desiredSpeed=a->speed;a->desiredPitch=0;
    a->target=-1;a->engage=0;a->evade=0;a->stuck=0;a->previous=a->p;a->goal=slot%OBJECTIVES;
}
void startMatch(int air){
    game.air=air;game.selection=air;game.count=air?10:12;game.edge=air?1000:600;
    game.assistTarget=-1;game.aiming=0;game.elapsed=0;game.kills=0;game.deaths=0;game.winner=-1;game.hitMarker=0;game.damageFlash=0;
    game.tickets[0]=game.tickets[1]=200;
    memset(game.actors,0,sizeof(game.actors));memset(game.shots,0,sizeof(game.shots));memset(game.fx,0,sizeof(game.fx));
    generateMap();
    for(int i=0;i<game.count;i++){game.actors[i].team=i%2;game.actors[i].plane=air;spawnActor(i);}
    game.mode=MODE_PLAY;announce(air?"AIR SUPERIORITY - ELIMINATE ENEMIES":"CAPTURE A / B / C - HOLD THE VALLEY");
}
static void effect(Vec p,float size,int smoke){
    for(int i=0;i<MAX_FX;i++)if(game.fx[i].life<=0){game.fx[i]=(Effect){p,smoke?2.5f:.6f,size,smoke};return;}
}
void damageActor(int victim,float amount,int owner){
    Actor *a=&game.actors[victim];if(a->hp<=0)return;
    a->hp=fmaxf(0,a->hp-amount);
    if(victim==0)game.damageFlash=.4f;
    if(owner==0&&victim!=0){game.hitMarker=.22f;playSound(3,a->p,.7f);}
    if(a->hp>0)return;
    playSound(2,a->p,1);a->respawn=6;game.tickets[a->team]-=8;effect(a->p,8,0);effect(add(a->p,V(0,3,0)),7,1);
    if(owner==0&&victim!=0){game.kills++;announce("TARGET DESTROYED - ENEMY LOSES 8");}
    if(victim==0){game.deaths++;announce("VEHICLE LOST - REDEPLOYING IN 6 SECONDS");}
}
void fireWeapon(int id){
    Actor *a=&game.actors[id];if(a->hp<=0||a->reload>0)return;
    float yaw=a->plane?a->yaw:a->turret,pitch=a->plane?a->pitch:a->elev;
    const BotSkill *skill=skillFor(id);
    if(id!=0){yaw+=(randf()*2-1)*skill->spread;pitch+=(randf()*2-1)*skill->spread;}
    Vec d=direction(yaw,pitch);
    for(int i=0;i<MAX_SHOTS;i++)if(game.shots[i].life<=0){
        Vec origin=add(a->p,V(0,a->plane?-.2f:1.5f,0));
        Vec muzzle=add(origin,mul(d,5.5f));
        /* Start at the breech if a wall blocks the muzzle; next sweep registers the hit. */
        if(traceWorld(origin,muzzle,0)<1)muzzle=origin;
        float speed=a->plane?360:245;
        game.shots[i]=(Shot){muzzle,mul(d,speed),a->plane?1.8f:3.0f,(a->plane?18:66)*(id==0?1:skill->damage),a->plane?0:9.8f,id,a->team};
        playSound(a->plane?1:0,a->p,id==0?1:.7f);a->reload=reloadTime(a)*(id==0?1:skill->cooldown);effect(muzzle,a->plane?.5f:1.1f,0);return;
    }
}
static float sphereTrace(Vec from,Vec to,Vec center,float radius){
    Vec d=sub(to,from),m=sub(from,center);float aa=dot(d,d),b=dot(m,d),c=dot(m,m)-radius*radius;
    if(c<=0)return 0;
    if(aa<.00001f||b>0)return 2;
    float disc=b*b-aa*c;if(disc<0)return 2;
    float t=(-b-sqrtf(disc))/aa;return t>=0&&t<=1?t:2;
}
static void updateShots(float dt){
    for(int i=0;i<MAX_SHOTS;i++){
        Shot *s=&game.shots[i];if(s->life<=0)continue;
        Vec old=s->p,end=add(old,mul(s->v,dt));s->life-=dt;s->v.y-=s->gravity*dt;
        float nearest=traceWorld(old,end,0);int hit=-1;
        for(int j=0;j<game.count;j++){
            Actor *a=&game.actors[j];if(a->hp<=0||a->team==s->team)continue;
            float t=sphereTrace(old,end,add(a->p,V(0,a->plane?0:.6f,0)),a->plane?4.4f:3.5f);
            if(t<nearest){nearest=t;hit=j;}
        }
        s->p=add(old,mul(sub(end,old),nearest));
        if(hit>=0){
            float damage=s->damage;Actor *victim=&game.actors[hit];
            if(!victim->plane){Vec approach=mul(s->v,-1.0f/fmaxf(length(s->v),1));if(dot(approach,direction(victim->yaw,0))>.65f)damage*=.78f;}
            damageActor(hit,damage,s->owner);effect(s->p,1.6f,0);s->life=0;
        }else if(nearest<1){effect(s->p,1.5f,0);s->life=0;}
    }
}
static int nearestEnemy(int id){
    float best=1e20f;int result=-1;Actor *a=&game.actors[id];
    for(int i=0;i<game.count;i++)if(game.actors[i].hp>0&&game.actors[i].team!=a->team){
        Vec d=sub(game.actors[i].p,a->p);float n=dot(d,d);if(n<best){best=n;result=i;}
    }return result;
}
static Vec leadTarget(const Actor *a,const Actor *target,float speed){
    float time=length(sub(target->p,a->p))/speed;
    return add(target->p,mul(direction(target->yaw,target->plane?target->pitch:0),target->speed*time*.75f));
}
static void thinkBot(int id){
    Actor *a=&game.actors[id];int oldTarget=a->target;a->target=nearestEnemy(id);if(a->target!=oldTarget)a->engage=0;if(a->target<0||game.actors[a->target].hp<=0)return;
    Actor *enemy=&game.actors[a->target];Vec target=leadTarget(a,enemy,a->plane?360:245);
    Vec delta=sub(target,a->p);float distance=length(delta);
    if(a->plane){
        Vec goal=target;
        if(fabsf(a->p.x)>game.edge-190||fabsf(a->p.z)>game.edge-190)goal=V(0,135,0);
        if(a->p.y<45)goal=add(a->p,V(sinf(a->yaw)*140,100,cosf(a->yaw)*140));
        if(a->p.y>280)goal=V(a->p.x+sinf(a->yaw)*150,160,a->p.z+cosf(a->yaw)*150);
        Vec g=sub(goal,a->p);a->desiredYaw=atan2f(g.x,g.z);
        a->desiredPitch=clampf(atan2f(g.y,horizontal(g)),-.6f,.6f);
        float error=fabsf(wrap(a->desiredYaw-a->yaw));a->desiredSpeed=error>1.2f?70:95;
    }else{
        Vec goal=game.objectives[a->goal].p;
        float best=1e9f;
        for(int i=0;i<OBJECTIVES;i++){
            Objective *o=&game.objectives[i];float score=horizontal(sub(o->p,a->p));
            if(o->owner==a->team)score+=310;
            if(i==id/2%OBJECTIVES)score-=65;
            if(score<best){best=score;a->goal=i;goal=o->p;}
        }
        /* Give each bot an offset so a whole team does not stack at one flag. */
        goal=add(goal,V(sinf(id*2.4f)*25,0,cosf(id*2.4f)*25));
        if(distance<100&&traceWorld(add(a->p,V(0,2,0)),target,0)>=.99f)goal=a->p;
        Vec g=sub(goal,a->p);float heading=atan2f(g.x,g.z);
        if(a->evade>0){heading=a->desiredYaw;}
        else if(traceWorld(a->p,add(a->p,mul(direction(heading,0),35)),5)<1){
            float bestScore=-1e9f,bestHeading=heading;
            for(int j=-3;j<=3;j++){
                float yaw=heading+j*.55f;Vec d=direction(yaw,0);
                float free=traceWorld(a->p,add(a->p,mul(d,48)),5);
                float score=free*90-abs(j)*4;
                if(score>bestScore){bestScore=score;bestHeading=yaw;}
            }
            heading=bestHeading;a->evade=.8f;
        }
        if(a->stuck>1.5f){heading=a->yaw+(id%2?1:-1)*1.7f;a->evade=1.5f;a->stuck=-1.2f;}
        a->desiredYaw=heading;a->desiredSpeed=horizontal(g)>14?26:0;
        a->desiredPitch=clampf(atan2f(delta.y+9.8f*((delta.x*delta.x+delta.z*delta.z)/(245*245))*.5f,horizontal(delta)),-.09f,.65f);
    }
}
static void controlBot(int id,float dt){
    Actor *a=&game.actors[id];const BotSkill *skill=skillFor(id);a->engage+=dt;a->think-=dt;a->evade=fmaxf(0,a->evade-dt);
    if(a->think<=0){thinkBot(id);a->think=skill->think;}
    a->yaw=turn(a->yaw,a->desiredYaw,(a->plane?1.05f:1.3f)*skill->turn*dt);
    a->speed+=(a->desiredSpeed-a->speed)*fminf(1,dt*2);
    if(!a->plane&&a->stuck<0){a->stuck=fminf(0,a->stuck+dt);a->speed=-12;}
    if(a->plane){a->pitch=turn(a->pitch,a->desiredPitch,.65f*dt);a->bank=clampf(-wrap(a->desiredYaw-a->yaw),-.9f,.9f);}
    if(a->target<0||game.actors[a->target].hp<=0)return;
    Actor *enemy=&game.actors[a->target];Vec target=leadTarget(a,enemy,a->plane?360:245),d=sub(target,a->p);
    float yaw=atan2f(d.x,d.z);
    if(!a->plane){a->turret=turn(a->turret,yaw,1.5f*skill->turn*dt);a->elev=turn(a->elev,a->desiredPitch,.7f*dt);}
    float error=fabsf(wrap(yaw-(a->plane?a->yaw:a->turret)));
    float pitch=atan2f(d.y,horizontal(d));
    if(a->engage>=skill->reaction&&length(d)<(a->plane?500:410)&&error<(a->plane?.10f:.045f)&&(!a->plane||fabsf(pitch-a->pitch)<.10f)&&traceWorld(add(a->p,V(0,a->plane?0:2,0)),target,0)>=.999f)fireWeapon(id);
}
/* Assist is deliberately limited: visible hostile, near the sight, and L held.
 * Projectiles remain ballistic and manual input overrides each aiming axis. */
void aimAssist(float dt,Controls c){
    game.assistTarget=-1;Actor *a=&game.actors[0];
    if(!game.assist||!game.aiming||a->hp<=0)return;
    Vec origin=add(a->p,V(0,a->plane?0:1.5f,0));
    Vec forward=direction(a->plane?a->yaw:a->turret,a->plane?a->pitch:a->elev);
    float best=1e9f;int target=-1;
    for(int i=1;i<game.count;i++){
        Actor *b=&game.actors[i];if(b->hp<=0||b->team==a->team)continue;
        Vec center=add(b->p,V(0,b->plane?0:.6f,0)),delta=sub(center,origin);float distance=length(delta);
        if(distance<6||distance>(a->plane?650:450))continue;
        float cosine=dot(forward,mul(delta,1/distance));
        if(cosine<(a->plane?.94f:.90f)||traceWorld(origin,center,0)<.999f)continue;
        float score=(1-cosine)*1000+distance*.02f;
        if(score<best){best=score;target=i;}
    }
    if(target<0)return;
    game.assistTarget=target;Actor *b=&game.actors[target];
    float speed=a->plane?360:245;
    Vec center=add(b->p,V(0,b->plane?0:.6f,0)),velocity=mul(direction(b->yaw,b->plane?b->pitch:0),b->speed);
    float flight=length(sub(center,origin))/speed;
    for(int k=0;k<3;k++)flight=length(sub(add(center,mul(velocity,flight)),origin))/speed;
    Vec delta=sub(add(center,mul(velocity,flight)),origin);
    if(!a->plane)delta.y+=.5f*9.8f*flight*flight;
    float yaw=atan2f(delta.x,delta.z),pitch=atan2f(delta.y,horizontal(delta));
    int manualX=fabsf(c.stickX)>.25f||(c.held&(KEY_DLEFT|KEY_DRIGHT));
    int manualY=fabsf(c.stickY)>.25f||(c.held&(KEY_DUP|KEY_DDOWN));
    if(a->plane){
        if(!manualX)a->yaw=turn(a->yaw,yaw,.55f*dt);
        if(!manualY)a->pitch=turn(a->pitch,clampf(pitch,a->p.y<30?.15f:-.7f,.7f),.4f*dt);
    }else{
        if(!manualX)a->turret=turn(a->turret,yaw,1.0f*dt);
        if(!manualY)a->elev=turn(a->elev,clampf(pitch,-.1f,.75f),.5f*dt);
    }
}
void playerControl(float dt,Controls c){
    Actor *a=&game.actors[0];game.aiming=(c.held&KEY_L)!=0;float x=clampf(c.stickX,-1,1),y=clampf(c.stickY,-1,1);
    x=fabsf(x)<.14f?0:(x-copysignf(.14f,x))/.86f;
    y=fabsf(y)<.14f?0:(y-copysignf(.14f,y))/.86f;
    if(a->plane){
        a->yaw-=x*.95f*dt;a->pitch=clampf(a->pitch+y*.8f*dt,-.9f,.9f);
        a->bank+=(x*.9f-a->bank)*fminf(1,dt*5);
        if(c.held&KEY_A)a->speed+=25*dt;
        if(c.held&KEY_B)a->speed-=35*dt;
        a->speed=clampf(a->speed,55,145);
    }else{
        if(game.aiming){
            /* Hold L: brake and route the analog stick exclusively to the gun. */
            a->speed=0;
            a->turret-=x*.65f*dt;
            a->elev+=y*.35f*dt;
        }else{
            float yawDelta=-x*1.25f*dt;
            a->yaw+=yawDelta;a->turret+=yawDelta;
            float desired=y*(c.held&KEY_A?38:27);
            a->speed+=(desired-a->speed)*fminf(1,dt*4);
        }
        /* KEY_UP/LEFT include Circle Pad aliases. Always use KEY_D* here. */
        if(c.held&KEY_DLEFT)a->turret+=1.15f*dt;
        if(c.held&KEY_DRIGHT)a->turret-=1.15f*dt;
        if(c.held&KEY_DUP)a->elev+=.58f*dt;
        if(c.held&KEY_DDOWN)a->elev-=.58f*dt;
        a->elev=clampf(a->elev,-.1f,.75f);
        /* L is a momentary aim zoom; never recenters the gun. */
    }
    aimAssist(dt,c);
    if(c.held&KEY_R)fireWeapon(0);
}
void stepGame(float dt,Controls controls){
    game.elapsed+=dt;game.hitMarker=fmaxf(0,game.hitMarker-dt);game.damageFlash=fmaxf(0,game.damageFlash-dt);game.messageTime=fmaxf(0,game.messageTime-dt);
    for(int i=0;i<game.count;i++){
        Actor *a=&game.actors[i];
        if(a->hp<=0){a->respawn-=dt;if(a->respawn<=0)spawnActor(i);continue;}
        a->reload=fmaxf(0,a->reload-dt);
        if(i==0)playerControl(dt,controls);else controlBot(i,dt);
        a->yaw=wrap(a->yaw);a->turret=wrap(a->turret);
        Vec old=a->p,next=add(old,mul(direction(a->yaw,a->plane?a->pitch:0),a->speed*dt));
        if(a->plane){
            a->p=next;
            /* Soft boundary recovery instead of an invisible death wall. */
            if(fabsf(next.x)>game.edge-60||fabsf(next.z)>game.edge-60){
                float desired=atan2f(-a->p.x,-a->p.z);a->yaw=turn(a->yaw,desired,1.7f*dt);
                a->p.x=clampf(a->p.x,-game.edge+5,game.edge-5);a->p.z=clampf(a->p.z,-game.edge+5,game.edge-5);
                if(i==0)announce("LEAVING COMBAT AREA - TURNING BACK");
            }
            if(a->p.y>330){a->pitch=turn(a->pitch,-.15f,dt);a->p.y=fminf(a->p.y,350);}
            if(next.y<2||traceWorld(old,next,2)<1)damageActor(i,1000,i);
        }else{
            a->p.x=next.x;if(blocked(a->p,3.8f))a->p.x=old.x;
            a->p.z=next.z;if(blocked(a->p,3.8f))a->p.z=old.z;
            for(int j=0;j<game.count;j++)if(j!=i&&game.actors[j].hp>0&&horizontal(sub(a->p,game.actors[j].p))<7){a->p=old;break;}
            if(i>0&&a->stuck>=0&&fabsf(a->speed)>5){if(horizontal(sub(old,a->p))<dt)a->stuck+=dt;else a->stuck=fmaxf(0,a->stuck-dt);}
        }
    }
    updateShots(dt);
    for(int i=0;i<MAX_FX;i++)if(game.fx[i].life>0){game.fx[i].life-=dt;if(game.fx[i].smoke)game.fx[i].p.y+=dt*6;}
    if(!game.air){
        for(int f=0;f<OBJECTIVES;f++){
            Objective *o=&game.objectives[f];int count[2]={0,0};
            for(int i=0;i<game.count;i++)if(game.actors[i].hp>0&&horizontal(sub(game.actors[i].p,o->p))<43)count[game.actors[i].team]++;
            o->contested=count[0]>0&&count[1]>0;
            if(count[0]&&!count[1])o->capture=clampf(o->capture+dt*.11f,-1,1);
            if(count[1]&&!count[0])o->capture=clampf(o->capture-dt*.11f,-1,1);
            int owner=o->capture>=1?0:o->capture<=-1?1:-1;
            if(owner!=o->owner){o->owner=owner;if(owner>=0){char msg[64];snprintf(msg,sizeof(msg),"OBJECTIVE %c CAPTURED BY %s",'A'+f,owner?"RED":"BLUE");announce(msg);}}
            if(o->owner>=0&&!o->contested)game.tickets[1-o->owner]-=dt*.55f;
        }
    }
    if(game.tickets[0]<=0||game.tickets[1]<=0||game.elapsed>=720){game.winner=game.tickets[0]>=game.tickets[1]?0:1;game.mode=MODE_RESULT;}
}
