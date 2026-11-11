#include "tank.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks, failures, active, badFree;
static bool tracking;
static void *nodes[64];
void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
void __real_free(void *pointer);

static void *Track(void *pointer)
{
    if (!tracking) return pointer;
    if (pointer) {
        int slot=0;
        while (slot<64 && nodes[slot]) slot++;
        if (slot==64) { __real_free(pointer); return NULL; }
        nodes[slot]=pointer; active++;
    }
    return pointer;
}

void *__wrap_malloc(size_t size) { return Track(__real_malloc(size)); }
void *__wrap_calloc(size_t count, size_t size) { return Track(__real_calloc(count,size)); }

void __wrap_free(void *pointer)
{
    if (!pointer) return;
    if (!tracking) { __real_free(pointer); return; }
    int slot=0;
    while (slot<64 && nodes[slot]!=pointer) slot++;
    if (slot==64) { badFree++; return; }
    nodes[slot]=NULL; active--; __real_free(pointer);
}

#define CHECK(condition) do { checks++; if (!(condition)) { failures++; \
    printf("FAIL line %d: %s\n",__LINE__,#condition); } } while (0)

static void EmptyGame(Game *g)
{
    GameInit(g,7);
    memset(g->map.tile,'.',CELLS); g->map.tile[BASE_CELL]='E';
    g->player.cell=g->player.dest=MapNode(PLAYER_CELL);
    g->playerAlive=true; g->phase=PLAY; g->spawnClock=10000;
    g->freeze=g->shield=10000; GameFields(g);
}

static void Step(Game *g)
{
    Input input={-1,false}; GameStep(g,input);
}

static int ItemCount(const Game *g)
{
    int count=0;
    for (int i=0; i<ITEMS; i++) count+=g->items[i].life>0;
    return count;
}

static Enemy *Spawn(Game *g, int index, int type)
{
    Enemy *enemy=GameSpawn(g,MapNode(4*GRID+1+index*3),type);
    if (enemy) { enemy->appear=0; enemy->bonus=false; }
    return enemy;
}

static void Shoot(Game *g, const Enemy *enemy)
{
    Bullet *bullet=&g->bullets[0];
    memset(g->bullets,0,sizeof(g->bullets));
    TankPosition(&enemy->tank,&bullet->x,&bullet->y);
    bullet->dir=RIGHT; bullet->power=1; bullet->speed=6; bullet->active=true;
    Step(g);
}

static void TestDeletePatterns(void)
{
    for (int mask=0; mask<16; mask++) {
        Game g; EmptyGame(&g);
        Enemy *original[4]; bool spawned=true;
        for (int i=0; i<4; i++) { original[i]=Spawn(&g,i,i); if (!original[i]) spawned=false; }
        CHECK(spawned);
        if (!spawned) { GameFree(&g); continue; }
        int deleted=0, score=0;
        for (int i=0; i<4; i++) if (mask&(1<<i)) {
            original[i]->hp=0; deleted++; score+=(i+1)*100;
        }
        Step(&g);
        CHECK(g.live==4-deleted && g.releases==deleted && active==4-deleted);
        CHECK(GameKills(&g)==deleted && g.score==score && !ItemCount(&g));
        Enemy *current=g.enemies; bool order=true;
        for (int i=3; i>=0; i--) if (!(mask&(1<<i))) {
            if (current!=original[i]) { order=false; break; }
            current=current->next;
        }
        CHECK(order && !current);
        bool kills=true;
        for (int i=0; i<4; i++) if (g.kills[i]!=((mask>>i)&1)) kills=false;
        CHECK(kills);
        GameFree(&g); GameFree(&g);
        CHECK(!g.enemies && !g.live && g.allocations==g.releases && !active && !badFree);
    }
}

static void TestDrops(void)
{
    Game g; EmptyGame(&g); Enemy *enemy=Spawn(&g,0,3);
    CHECK(enemy!=NULL);
    if (!enemy) { GameFree(&g); return; }
    enemy->bonus=true; Shoot(&g,enemy);
    CHECK(enemy->hp==3 && !enemy->bonus && ItemCount(&g)==1);
    for (int i=0; i<3; i++) Shoot(&g,enemy);
    CHECK(!g.enemies && !g.live && g.score==400 && g.kills[3]==1 && ItemCount(&g)==1);
    CHECK(g.releases==1 && !active); GameFree(&g);

    EmptyGame(&g); enemy=Spawn(&g,0,0); CHECK(enemy!=NULL);
    if (!enemy) { GameFree(&g); return; }
    enemy->bonus=true; enemy->hp=0; Step(&g);
    CHECK(ItemCount(&g)==1 && g.score==100 && !g.live && !active); GameFree(&g);

    EmptyGame(&g);
    for (int i=0; i<4; i++) { enemy=Spawn(&g,i,i); if (enemy) enemy->bonus=true; }
    CHECK(g.live==4); GameItem(&g,GRENADE);
    CHECK(!g.enemies && !g.live && GameKills(&g)==4 && g.score==500 && !ItemCount(&g));
    CHECK(g.allocations==g.releases && !active && !badFree); GameFree(&g);

    EmptyGame(&g);
    for (int i=0; i<ITEMS; i++) { g.items[i].cell=i; g.items[i].type=STAR; g.items[i].life=100; }
    enemy=Spawn(&g,0,0); CHECK(enemy!=NULL);
    if (enemy) { enemy->bonus=true; Shoot(&g,enemy); }
    CHECK(ItemCount(&g)==ITEMS && !g.live && g.releases==1 && !active); GameFree(&g);
}

static void TestThreeStages(void)
{
    Game g; GameInit(&g,17); GameStart(&g,17);
    const int expected[STAGES][4]={{8,0,0,0},{3,3,2,0},{0,3,2,3}};
    for (int stage=1; stage<=STAGES; stage++) {
        CHECK(g.stage==stage && g.phase==READY && !g.mapFallback);
        g.phase=PLAY; g.shield=100000;
        int frames=0, observed=0, counts[4]={0};
        bool seen[ENEMY_TOTAL+1]={false}, valid=true;
        while (g.phase==PLAY && frames++<10000) {
            for (Enemy *enemy=g.enemies; enemy; enemy=enemy->next) enemy->hp=0;
            Step(&g);
            for (Enemy *enemy=g.enemies; enemy; enemy=enemy->next) {
                if (enemy->id<1 || enemy->id>ENEMY_TOTAL || enemy->type<0 || enemy->type>3) {
                    valid=false; continue;
                }
                if (!seen[enemy->id]) {
                    seen[enemy->id]=true; observed++; counts[enemy->type]++;
                    if (enemy->type!=MapEnemyType(stage,enemy->id-1)) valid=false;
                }
            }
        }
        CHECK(g.phase==CLEAR && g.spawned==ENEMY_TOTAL && !g.live);
        CHECK(valid && observed==ENEMY_TOTAL && memcmp(counts,expected[stage-1],sizeof(counts))==0);
        CHECK(GameKills(&g)==stage*ENEMY_TOTAL && g.allocations==g.releases && !active);
        printf("Stage %d: spawned=%d; ordinary=%d fast=%d rapid=%d heavy=%d\n",
            stage,observed,counts[0],counts[1],counts[2],counts[3]);
        GameNext(&g);
    }
    CHECK(g.phase==WON && g.stage==STAGES && g.baseAlive);
    GameFree(&g); CHECK(!active && !badFree);
    GameStart(&g,19); CHECK(g.stage==1 && g.phase==READY && !g.score && !GameKills(&g));
    GameFree(&g);
    printf("Three-stage scheduler/state transitions checked with scripted enemy removal, not normal play.\n");
}

static void TestSpawnLimit(void)
{
    Game g; EmptyGame(&g); g.spawnClock=0;
    int frames=0;
    while (g.live<ALIVE_LIMIT && frames++<1000) {
        Step(&g);
        for (Enemy *enemy=g.enemies; enemy; enemy=enemy->next) {
            int cell=MapNode(4*GRID+1+(enemy->id-1)*3);
            enemy->tank.cell=enemy->tank.dest=cell;
            enemy->tank.step=enemy->tank.frames=0;
        }
    }
    CHECK(g.phase==PLAY && g.live==ALIVE_LIMIT && g.spawned==ALIVE_LIMIT);
    int allocations=g.allocations;
    for (int i=0; i<300; i++) Step(&g);
    CHECK(g.live==ALIVE_LIMIT && g.spawned==ALIVE_LIMIT && g.allocations==allocations);
    CHECK(!GameSpawn(&g,MapNode(0),0) && g.live==ALIVE_LIMIT && g.allocations==allocations);
    if (g.enemies) g.enemies->hp=0;
    Step(&g);
    CHECK(g.live==ALIVE_LIMIT-1 && g.spawned==ALIVE_LIMIT && g.releases==1);
    Step(&g);
    CHECK(g.live==ALIVE_LIMIT && g.spawned==ALIVE_LIMIT+1 && g.allocations==allocations+1);
    GameFree(&g); CHECK(!active && !badFree && g.allocations==g.releases);
    printf("Scheduler cap: four live enemies, no fifth while full, replacement after removal checked.\n");
}

int main(int argc, char **argv)
{
    if (argc!=2 || strcmp(argv[1],"--test-hidden")) return 2;
    tracking=true;
    TestDeletePatterns(); TestDrops(); TestThreeStages(); TestSpawnLimit();
    printf("Revision checks: %d/%d passed; live allocations=%d; invalid frees=%d\n",
        checks-failures,checks,active,badFree);
    tracking=false;
    return failures || active || badFree ? 1 : 0;
}
