#include "tank.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int dx[4]={0,1,0,-1}, dy[4]={-1,0,1,0};
static const int fort[5]={11*GRID+5,11*GRID+6,11*GRID+7,12*GRID+5,12*GRID+7};
static const int moveFrames[4]={30,19,28,36};

static unsigned Random(Game *g, unsigned limit)
{
    uint32_t x=g->random; x^=x<<13; x^=x>>17; x^=x<<5;
    g->random=x; return limit ? x%limit : 0;
}

void TankPosition(const Tank *t, float *x, float *y)
{
    if (!t || !x || !y) return;
    float p=t->frames>0 ? (float)t->step/t->frames : 0;
    *x=(t->cell%NAV_GRID+1)*STEP + ((t->dest%NAV_GRID)-(t->cell%NAV_GRID))*STEP*p;
    *y=(t->cell/NAV_GRID+1)*STEP + ((t->dest/NAV_GRID)-(t->cell/NAV_GRID))*STEP*p;
}

void GameFree(Game *g)
{
    if (!g) return;
    while (g->enemies) {
        Enemy *next=g->enemies->next; free(g->enemies); g->enemies=next; g->releases++;
    }
    g->live=0;
}

void GameInit(Game *g, uint32_t seed)
{
    if (!g) return;
    memset(g, 0, sizeof(*g));
    g->random = seed ? seed : 1;
    g->lives = 3;
    g->stage = 1;
    g->baseAlive = true;
}

void GameFields(Game *g)
{
    if (!g) return;
    FieldBuild(&g->baseField,&g->map,MapNode(BASE_CELL));
    FieldBuild(&g->playerField,&g->map,g->player.cell);
    g->fieldRevision=g->map.revision;
}

void GameLoadStage(Game *g, int stage)
{
    if (!g || stage<1 || stage>STAGES) return;
    GameFree(g); g->stage=stage;
    char path[64]; snprintf(path,sizeof(path),"assets/maps/level%d.txt",stage);
    g->mapFallback=!MapLoad(&g->map,path);
    if (g->mapFallback) MapBuiltin(&g->map,stage);
    memset(g->bullets,0,sizeof(g->bullets)); memset(g->items,0,sizeof(g->items));
    memset(&g->player, 0, sizeof(g->player));
    g->player.cell = MapNode(PLAYER_CELL);
    g->player.dest = MapNode(PLAYER_CELL);
    g->player.dir = UP;
    g->playerAlive=g->baseAlive=true; g->phase=READY;
    g->shield=180; g->reload=g->respawn=g->freeze=g->shovel=g->clearClock=0;
    g->spawned=g->serial=g->tick=0; g->spawnClock=70; g->memoryError=false;
    GameFields(g);
}

void GameStart(Game *g, uint32_t seed)
{
    if (!g) return;
    GameFree(g); GameInit(g,seed); GameLoadStage(g,1);
}

void GameNext(Game *g)
{
    if (!g || g->phase!=CLEAR) return;
    if (g->stage==STAGES) { g->phase=WON; g->sounds|=S_END; }
    else GameLoadStage(g,g->stage+1);
}

int GameKills(const Game *g)
{
    return g ? g->kills[0]+g->kills[1]+g->kills[2]+g->kills[3] : 0;
}

static bool Near(int a, int b)
{
    return abs(a%NAV_GRID-b%NAV_GRID)*STEP<TANK_HALF*2 &&
           abs(a/NAV_GRID-b/NAV_GRID)*STEP<TANK_HALF*2;
}

static bool Occupied(const Game *g, int cell, const Tank *self)
{
    if (g->playerAlive && self!=&g->player && (Near(g->player.cell,cell) || Near(g->player.dest,cell))) return true;
    for (const Enemy *e=g->enemies; e; e=e->next)
        if (&e->tank!=self && e->hp>0 && (Near(e->tank.cell,cell) || Near(e->tank.dest,cell))) return true;
    return false;
}

static void UnblockTank(Game *g, Tank *tank)
{
    if (MapClear(&g->map,tank->cell,0) && MapClear(&g->map,tank->dest,0)) return;
    float x=0,y=0,distance=100000; TankPosition(tank,&x,&y); int best=-1;
    for (int node=0; node<NAV_CELLS; node++) {
        if (!MapClear(&g->map,node,0) || Occupied(g,node,tank)) continue;
        float d=fabsf((node%NAV_GRID+1)*STEP-x)+fabsf((node/NAV_GRID+1)*STEP-y);
        if (d<distance) { distance=d; best=node; }
    }
    if (best >= 0) {
        int oldDir = tank->dir;
        memset(tank, 0, sizeof(*tank));
        tank->cell = best;
        tank->dest = best;
        tank->dir = oldDir;
    }
}

static void UnblockUnits(Game *g)
{
    /* A temporary fort must not build a wall through an existing hull. */
    if (g->playerAlive) UnblockTank(g,&g->player);
    for (Enemy *e=g->enemies; e; e=e->next) UnblockTank(g,&e->tank);
}

Enemy *GameSpawn(Game *g, int cell, int type)
{
    if (!g || type<0 || type>3 || g->live>=ALIVE_LIMIT ||
        !MapClear(&g->map,cell,0) || Occupied(g,cell,NULL)) return NULL;
    Enemy *e=malloc(sizeof(*e));
    if (!e) { g->memoryError=true; g->phase=PAUSED; return NULL; }
    memset(e, 0, sizeof(*e));
    e->tank.cell = cell;
    e->tank.dest = cell;
    e->tank.dir = DOWN;
    e->id = ++g->serial;
    e->type = type;
    e->hp = type == 3 ? 4 : 1;
    e->reload = 65;
    e->appear = 40;
    e->bonus = g->serial % 7 == 4;
    e->next = g->enemies;
    g->enemies=e; g->live++; g->allocations++; return e;
}

static bool Move(Game *g, Tank *t, int dir, int frames)
{
    if (dir<UP || dir>LEFT || t->dest!=t->cell) return false;
    t->dir=dir; int n=Neighbor(t->cell,dir);
    if (!MapClear(&g->map,n,0) || Occupied(g,n,t)) return false;
    t->dest=n; t->step=0; t->frames=frames; return true;
}

static void Advance(Tank *t)
{
    if (t->cell==t->dest) return;
    if ((t->step+=2)>=t->frames) { t->cell=t->dest; t->step=t->frames=0; }
}

static bool Fire(Game *g, const Tank *t, int owner, int type)
{
    int active=0, limit=owner==0 && g->stars>=2 ? 2 : 1;
    for (int i=0; i<BULLETS; i++) if (g->bullets[i].active && g->bullets[i].owner==owner) active++;
    if (active>=limit) return false;
    for (int i=0; i<BULLETS; i++) if (!g->bullets[i].active) {
        float x=0,y=0; TankPosition(t,&x,&y);
        memset(&g->bullets[i], 0, sizeof(g->bullets[i]));
        g->bullets[i].x = x + dx[t->dir] * 16;
        g->bullets[i].y = y + dy[t->dir] * 16;
        g->bullets[i].dir = t->dir;
        g->bullets[i].owner = owner;
        g->bullets[i].power = owner == 0 && g->stars == 3 ? 2 : 1;
        g->bullets[i].speed = owner == 0 ? 5.5f + g->stars * .7f : (type == 2 ? 7.0f : 3.8f);
        g->bullets[i].active = true;
        if (!owner) g->sounds|=S_SHOOT;
        return true;
    }
    return false;
}

static void DropItem(Game *g)
{
    for (int i=0; i<ITEMS; i++) if (!g->items[i].life) {
        for (int attempt=0; attempt<100; attempt++) {
            int cell=(4+(int)Random(g,7))*GRID+1+(int)Random(g,11);
            if (!TileWalkable(g->map.tile[cell])) continue;
            bool duplicate=false;
            for (int j=0; j<ITEMS; j++) if (g->items[j].life && g->items[j].cell==cell) duplicate=true;
            if (!duplicate) {
                g->items[i].cell = cell;
                g->items[i].type = (int)Random(g, ITEM_TYPES);
                g->items[i].life = 900;
                return;
            }
        }
        return;
    }
}

static void RemoveDead(Game *g, bool drops)
{
    Enemy *prev = NULL;
    Enemy *cur = g->enemies;
    while (cur != NULL) {
        if (cur->hp > 0) {
            prev = cur;
            cur = cur->next;
            continue;
        }
        Enemy *temp = cur;
        g->kills[temp->type]++;
        if (drops) g->score += (temp->type + 1) * 100;
        if (drops && temp->bonus) DropItem(g);

        if (prev == NULL) g->enemies = cur->next;
        else prev->next = cur->next;
        cur = cur->next;
        free(temp);
        g->live--;
        g->releases++;
        g->sounds |= S_KILL;
    }
}

void GameItem(Game *g, int type)
{
    if (!g || type<0 || type>=ITEM_TYPES) return;
    g->score+=500;
    if (type==STAR && g->stars<3) g->stars++;
    if (type==HELMET) g->shield=480;
    if (type==CLOCK) g->freeze=420;
    if (type==EXTRA_LIFE && g->lives<99) g->lives++;
    if (type==GRENADE) {
        for (Enemy *e=g->enemies; e; e=e->next) e->hp=0;
        RemoveDead(g,false);
    }
    if (type==SHOVEL) {
        for (int i=0; i<5; i++) {
            if (!g->shovel) {
                g->fortTile[i]=g->map.tile[fort[i]]; g->fortBrick[i]=g->map.brick[fort[i]];
                g->fortSteel[i]=g->map.steel[fort[i]];
            }
            g->map.tile[fort[i]]='S'; g->map.brick[fort[i]]=0; g->map.steel[fort[i]]=15;
        }
        g->shovel=600; g->map.revision++; UnblockUnits(g);
    }
    g->sounds|=S_ITEM;
}

static void PlayerHit(Game *g)
{
    if (!g->playerAlive || g->shield) return;
    g->lives--; g->playerAlive=false; g->stars=0; g->respawn=90; g->sounds|=S_KILL;
    if (!g->lives) { g->phase=LOST; g->sounds|=S_END; }
}

static void UpdatePlayer(Game *g, Input input)
{
    if (!g->playerAlive) {
        if (g->respawn>0) g->respawn--;
        if (!g->respawn && MapClear(&g->map,MapNode(PLAYER_CELL),0) && !Occupied(g,MapNode(PLAYER_CELL),NULL)) {
            memset(&g->player, 0, sizeof(g->player));
            g->player.cell = MapNode(PLAYER_CELL);
            g->player.dest = MapNode(PLAYER_CELL);
            g->player.dir = UP;
            g->playerAlive=true; g->shield=180;
        }
        return;
    }
    if (g->player.cell!=g->player.dest && Neighbor(g->player.dest,input.dir)==g->player.cell) {
        int cell=g->player.cell; g->player.cell=g->player.dest; g->player.dest=cell;
        g->player.step=g->player.frames-g->player.step; g->player.dir=input.dir;
    }
    Advance(&g->player);
    if (g->player.cell==g->player.dest && input.dir>=UP && input.dir<=LEFT) Move(g,&g->player,input.dir,16);
    if (input.fire && !g->reload && Fire(g,&g->player,0,0)) g->reload=14;
    float x=0,y=0; TankPosition(&g->player,&x,&y);
    for (int i=0; i<ITEMS; i++) if (g->items[i].life &&
        fabsf(x-(g->items[i].cell%GRID+.5f)*TILE)<28 && fabsf(y-(g->items[i].cell/GRID+.5f)*TILE)<28) {
        int type=g->items[i].type; g->items[i].life=0; GameItem(g,type);
    }
}

static int Sight(const Game *g, const Tank *from, int target)
{
    int x=from->cell%NAV_GRID,y=from->cell/NAV_GRID, tx=target%NAV_GRID,ty=target/NAV_GRID;
    if (x!=tx && y!=ty) return -1;
    int dir=x==tx ? (ty>y ? DOWN : UP) : (tx>x ? RIGHT : LEFT);
    for (int c=Neighbor(from->cell,dir); c>=0; c=Neighbor(c,dir)) {
        if (c==target) return dir;
        char tile=MapAt(&g->map,(c%NAV_GRID+1)*STEP,(c/NAV_GRID+1)*STEP);
        if (tile=='B' || tile=='S') return -1;
    }
    return -1;
}

static void EnemyPlan(Game *g, Enemy *e)
{
    int shot=Sight(g,&e->tank,MapNode(BASE_CELL));
    if (shot<0 && g->playerAlive) shot=Sight(g,&e->tank,g->player.cell);
    const Field *f=g->playerAlive && (e->id%3==0 || g->baseField.dist[e->tank.cell]<0) ? &g->playerField : &g->baseField;
    if (shot>=0 && (!e->reload || (f->dist[e->tank.cell]>=0 && f->dist[e->tank.cell]<=2))) {
        e->tank.dir=shot;
        if (!e->reload && Fire(g,&e->tank,e->id,e->type)) e->reload=65+(int)Random(g,35);
        return;
    }
    int flags=PLAN_BRICKS | (f==&g->baseField ? PLAN_BASE : 0);
    int best=-1, value=100000;
    for (int d=0; d<4; d++) {
        int n=Neighbor(e->tank.cell,d);
        if (n<0 || f->dist[n]<0 || !MapClear(&g->map,n,flags) ||
            Occupied(g,n,&e->tank)) continue;
        int v=f->dist[n]*8+(int)Random(g,7);
        if (v<value) { value=v; best=d; }
    }
    if (best<0) return;
    int n=Neighbor(e->tank.cell,best); e->tank.dir=best;
    if (!MapClear(&g->map,n,0)) {
        if (!e->reload && Fire(g,&e->tank,e->id,e->type)) e->reload=55+(int)Random(g,25);
    } else Move(g,&e->tank,best,moveFrames[e->type]);
}

static void UpdateEnemies(Game *g)
{
    for (Enemy *e=g->enemies; e; e=e->next) {
        if (e->appear) { e->appear--; continue; }
        if (g->freeze) continue;
        if (e->reload) e->reload--;
        Advance(&e->tank);
        if (e->tank.cell==e->tank.dest) EnemyPlan(g,e);
        else if (!e->reload && Random(g,40)==0 && Fire(g,&e->tank,e->id,e->type)) e->reload=80;
    }
}

static void HitTile(Game *g, Bullet *b)
{
    if (b->x<0 || b->x>=GRID*TILE || b->y<0 || b->y>=GRID*TILE) { b->active=false; return; }
    bool hit=false;
    /* Clear the two small blocks across the hull, including large-tile seams. */
    for (int side=-1; side<=1; side+=2) {
        int x=(int)b->x+dy[b->dir]*side*(TANK_HALF-1);
        int y=(int)b->y+dx[b->dir]*side*(TANK_HALF-1);
        if (x<0 || y<0 || x>=GRID*TILE || y>=GRID*TILE) continue;
        char tile=MapAt(&g->map,x,y);
        if (tile!='B' && tile!='S' && tile!='E') continue;
        hit=true;
        if (tile=='E') { g->baseAlive=false; g->phase=LOST; g->sounds|=S_END; continue; }
        int cell=y/TILE*GRID+x/TILE, bit=1<<((y%TILE>=STEP)*2+(x%TILE>=STEP));
        if (tile=='B') g->map.brick[cell]&=(unsigned char)~bit;
        else if (b->power>=2) {
            unsigned steel=g->map.steel[cell] ? g->map.steel[cell] : 15;
            g->map.steel[cell]=(unsigned char)(steel&~bit);
        } else continue;
        g->map.tile[cell]=g->map.steel[cell] ? 'S' : g->map.brick[cell] ? 'B' : '.';
        g->map.revision++;
    }
    if (hit) { b->active=false; g->sounds|=S_HIT; }
}

static void UpdateBullets(Game *g)
{
    /* At most seven pixels per frame: four shared substeps also catch opposing bullets. */
    for (int step=0; step<4; step++) {
        for (int i=0; i<BULLETS; i++) {
            Bullet *b=&g->bullets[i]; if (!b->active) continue;
            b->x+=dx[b->dir]*b->speed/4; b->y+=dy[b->dir]*b->speed/4;
            HitTile(g,b);
        }
        for (int i=0; i<BULLETS; i++) for (int j=i+1; j<BULLETS; j++) {
            Bullet *a=&g->bullets[i],*b=&g->bullets[j];
            if (a->active && b->active && ((a->owner==0)!=(b->owner==0)) &&
                fabsf(a->x-b->x)<6 && fabsf(a->y-b->y)<6) a->active=b->active=false;
        }
        for (int i=0; i<BULLETS; i++) {
            Bullet *b=&g->bullets[i]; if (!b->active) continue;
            if (b->owner==0) {
                for (Enemy *e=g->enemies; e; e=e->next) if (e->hp>0 && !e->appear) {
                    float x=0,y=0; TankPosition(&e->tank,&x,&y);
                    if (fabsf(x-b->x)<16 && fabsf(y-b->y)<16) {
                        if (e->bonus) { DropItem(g); e->bonus=false; }
                        e->hp--; b->active=false; g->sounds|=S_HIT; break;
                    }
                }
            } else if (g->playerAlive) {
                float x=0,y=0; TankPosition(&g->player,&x,&y);
                if (fabsf(x-b->x)<16 && fabsf(y-b->y)<16) { b->active=false; PlayerHit(g); }
            }
        }
    }
    RemoveDead(g,true);
}

static void Timers(Game *g)
{
    if (g->shield) g->shield--;
    if (g->reload) g->reload--;
    if (g->freeze) g->freeze--;
    if (g->shovel && --g->shovel==0) {
        for (int i=0; i<5; i++) {
            g->map.tile[fort[i]]=g->fortTile[i]; g->map.brick[fort[i]]=g->fortBrick[i];
            g->map.steel[fort[i]]=g->fortSteel[i];
        }
        g->map.revision++; UnblockUnits(g);
    }
    for (int i=0; i<ITEMS; i++) if (g->items[i].life) g->items[i].life--;
}

void GameStep(Game *g, Input input)
{
    if (!g || g->phase!=PLAY) return;
    g->tick++; Timers(g); UpdatePlayer(g,input);
    if (g->map.revision!=g->fieldRevision || g->tick%15==0) GameFields(g);
    if (g->spawnClock>0) g->spawnClock--;
    if (!g->spawnClock && g->spawned<ENEMY_TOTAL && g->live<ALIVE_LIMIT) {
        const int spawns[]={0,6,12};
        for (int i=0; i<3; i++) {
            int cell=MapNode(spawns[(g->spawned+i)%3]);
            int type=MapEnemyType(g->stage,g->spawned);
            if (GameSpawn(g,cell,type)) { g->spawned++; g->spawnClock=90; break; }
            if (g->memoryError) return;
        }
    }
    UpdateEnemies(g); UpdateBullets(g);
    if (g->phase!=PLAY) return;
    if (g->spawned==ENEMY_TOTAL && !g->live) {
        if (!g->clearClock) g->clearClock=90;
        if (--g->clearClock==0) { g->phase=CLEAR; g->sounds|=S_END; }
    }
}
