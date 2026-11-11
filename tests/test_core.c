#include "tank.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wall_fixture.h"

static int checks, failures;
static bool failAllocation;
void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size) { return failAllocation ? NULL : __real_malloc(size); }
void *__real_calloc(size_t count, size_t size);
void *__wrap_calloc(size_t count, size_t size) { return failAllocation ? NULL : __real_calloc(count,size); }
#define CHECK(condition) do { checks++; if (!(condition)) { failures++; \
    printf("FAIL line %d: %s\n",__LINE__,#condition); } } while (0)

static void EmptyGame(Game *g)
{
    GameInit(g,7);
    memset(g->map.tile,'.',CELLS); g->map.tile[BASE_CELL]='E'; g->map.revision=1;
    memset(&g->player, 0, sizeof(g->player));
    g->player.cell = MapNode(PLAYER_CELL);
    g->player.dest = MapNode(PLAYER_CELL);
    g->player.dir = UP;
    g->playerAlive=true; g->phase=PLAY; g->spawned=ENEMY_TOTAL;
    GameFields(g);
}

static void Run(Game *g, int frames, int dir, bool fire)
{
    for (int i=0; i<frames; i++) GameStep(g,(Input){dir,fire});
}

static int Active(const Game *g, int owner)
{
    int count=0;
    for (int i=0; i<BULLETS; i++) if (g->bullets[i].active && g->bullets[i].owner==owner) count++;
    return count;
}

static void Shot(Game *g, int cell, int dir, int owner, int power)
{
    memset(g->bullets,0,sizeof(g->bullets));
    g->bullets[0].x = (cell % NAV_GRID + 1) * STEP;
    g->bullets[0].y = (cell / NAV_GRID + 1) * STEP;
    g->bullets[0].dir = dir;
    g->bullets[0].owner = owner;
    g->bullets[0].power = power;
    g->bullets[0].speed = 6;
    g->bullets[0].active = true;
}

static void TestMaps(void)
{
    for (int s=1; s<=STAGES; s++) {
        Map a,b; char path[80]; MapBuiltin(&a,s);
        snprintf(path,sizeof(path),"assets/maps/level%d.txt",s);
        CHECK(MapValid(&a)); CHECK(MapLoad(&b,path));
        CHECK(memcmp(a.tile,b.tile,CELLS)==0 && memcmp(a.brick,b.brick,CELLS)==0 && memcmp(a.steel,b.steel,CELLS)==0);
    }
    CHECK(Neighbor(0,LEFT)==-1 && Neighbor(0,UP)==-1);
    CHECK(Neighbor(NAV_GRID-1,RIGHT)==-1 && Neighbor(NAV_CELLS-1,DOWN)==-1);
    CHECK(Neighbor(-1,UP)==-1 && Neighbor(0,8)==-1);
    CHECK(TileWalkable('T') && !TileWalkable('W'));
    Map a,b; MapBuiltin(&a,1); b=a;
    CHECK(!MapLoad(&a,"missing/map.txt") && memcmp(&a,&b,sizeof(a))==0);
    FILE *f=fopen("bad_map.txt","w");
    CHECK(f!=NULL); if (!f) return;
    for (int y=0; y<14; y++) fprintf(f,".............\n");
    fclose(f); CHECK(!MapLoad(&a,"bad_map.txt") && memcmp(&a,&b,sizeof(a))==0);
    f=fopen("bad_map.txt","w"); fprintf(f,"X\n"); fclose(f);
    CHECK(!MapLoad(&a,"bad_map.txt")); remove("bad_map.txt");
    a.tile[0]='E'; CHECK(!MapValid(&a));
    CHECK(!MapValid(NULL) && !MapLoad(NULL,"x"));
    CHECK(ENEMY_TOTAL==8 && ALIVE_LIMIT==4);
    const int expected[STAGES][4]={{8,0,0,0},{3,3,2,0},{0,3,2,3}};
    for (int stage=1; stage<=STAGES; stage++) {
        int counts[4]={0}; bool valid=true;
        for (int index=0; index<ENEMY_TOTAL; index++) {
            int type=MapEnemyType(stage,index);
            if (type<0 || type>=4) valid=false;
            else counts[type]++;
        }
        CHECK(valid && memcmp(counts,expected[stage-1],sizeof(counts))==0);
    }
    CHECK(MapEnemyType(0,0)==0 && MapEnemyType(STAGES+1,0)==0);
    CHECK(MapEnemyType(1,-1)==0 && MapEnemyType(1,ENEMY_TOTAL)==0);
}

static void TestFields(void)
{
    Map m={0}; memset(m.tile,'.',CELLS); m.tile[6*GRID+6]='S'; m.tile[5*GRID+6]='W';
    m.tile[4*GRID+6]='B'; m.brick[4*GRID+6]=15;
    Field f, layered; FieldBuild(&f,&m,0); FieldBegin(&layered,0);
    int layers=0;
    while (layered.head<layered.tail && layers++<NAV_CELLS) FieldLayer(&layered,&m);
    CHECK(f.head==layered.head && f.tail==layered.tail &&
        memcmp(f.dist,layered.dist,sizeof(f.dist))==0 && memcmp(f.next,layered.next,sizeof(f.next))==0 &&
        memcmp(f.queue,layered.queue,(size_t)f.tail*sizeof(int))==0);
    CHECK(f.dist[NAV_CELLS-1]==48 && f.dist[MapNode(6*GRID+6)]==-1 && f.dist[MapNode(5*GRID+6)]==-1);
    CHECK(f.dist[MapNode(4*GRID+6)]>=0 && f.tail==NAV_CELLS-15 && f.head==f.tail);
    bool invariant=true; int path[NAV_CELLS];
    for (int c=1; c<NAV_CELLS; c++) if (f.dist[c]>=0) {
        int n=f.next[c]; bool adjacent=false;
        for (int d=0; d<4; d++) if (Neighbor(c,d)==n) adjacent=true;
        if (n<0 || f.dist[n]!=f.dist[c]-1 || !adjacent) invariant=false;
    }
    CHECK(invariant);
    CHECK(FieldPath(&f,NAV_CELLS-1,path,NAV_CELLS)==49 && path[48]==0);
    CHECK(FieldPath(&f,MapNode(6*GRID+6),path,NAV_CELLS)==0);
    CHECK(FieldPath(&f,NAV_CELLS-1,path,3)==3);
    FieldBegin(&f,-1); CHECK(f.tail==0 && f.dist[0]==-1);
    FieldBegin(NULL,0); FieldLayer(NULL,&m); CHECK(FieldPath(NULL,0,path,NAV_CELLS)==0);
}

static void TestRecords(void)
{
    Board b={0};
    BoardInsert(&b,(Record){100,1,1}); BoardInsert(&b,(Record){100,2,2});
    CHECK(b.entries[0].stage==1 && b.entries[1].stage==2);
    for (int i=0; i<20; i++) BoardInsert(&b,(Record){i*100,1,i});
    CHECK(b.count==10 && b.entries[0].score==1900 && b.entries[9].score==1000);
    Board before=b; BoardInsert(&b,(Record){-1,1,0}); CHECK(memcmp(&b,&before,sizeof(b))==0);
    FILE *f=fopen("test_records.txt","w"); CHECK(f!=NULL); if (!f) return;
    fprintf(f,"# test\n300 2 5\n200 1 2\n999999999999999999999 1 1\n-1 1 1\n3 4 2\n4 1 %d\n9 1 1 extra\n",STAGES*ENEMY_TOTAL+1);
    for (int i=0; i<200; i++) fputc('7',f);
    fprintf(f,"\n100 1 1\n"); fclose(f);
    CHECK(BoardLoad(&b,"test_records.txt") && b.count==3 && b.skipped==6);
    CHECK(b.entries[0].score==300 && b.entries[2].score==100);
    CHECK(BoardSave(&b,(Record){400,STAGES,STAGES*ENEMY_TOTAL},"test_records.txt"));
    CHECK(BoardLoad(&b,"test_records.txt") && b.count==4 && b.entries[0].score==400 && b.entries[0].kills==STAGES*ENEMY_TOTAL);
    before=b; CHECK(!BoardSave(&b,(Record){500,STAGES,STAGES*ENEMY_TOTAL+1},"test_records.txt"));
    CHECK(memcmp(&before,&b,sizeof(b))==0);
    before=b; CHECK(!BoardSave(&b,(Record){500,1,0},"missing/test.txt"));
    CHECK(memcmp(&before,&b,sizeof(b))==0); remove("test_records.txt");
    f=fopen("test_records.txt","w"); fprintf(f,"100 1 1"); fclose(f);
    CHECK(BoardSave(&b,(Record){200,2,2},"test_records.txt"));
    CHECK(BoardLoad(&b,"test_records.txt") && b.count==2 && b.skipped==0);
    remove("test_records.txt");

    f=fopen("test_records.txt","wb"); CHECK(f!=NULL); if (!f) return;
    const char damaged[]="999 1 1\0garbage\n100 1 1\r\n200 2 2";
    CHECK(fwrite(damaged,1,sizeof(damaged)-1,f)==sizeof(damaged)-1);
    fclose(f);
    CHECK(BoardLoad(&b,"test_records.txt") && b.count==2 && b.skipped==1);
    CHECK(b.entries[0].score==200 && b.entries[1].score==100);
    f=fopen("test_records.txt","wb"); CHECK(f!=NULL); if (!f) return;
    const char control[]="999 1 1\x1a junk\n300 3 3\n";
    CHECK(fwrite(control,1,sizeof(control)-1,f)==sizeof(control)-1);
    fclose(f);
    CHECK(BoardLoad(&b,"test_records.txt") && b.count==1 && b.skipped==1 && b.entries[0].score==300);
    f=fopen("test_records.txt","wb"); CHECK(f!=NULL); if (!f) return;
    for (int i=0; i<200; i++) fputc('7',f);
    fputs("\n400 1 1\n500 1 1",f); fclose(f);
    CHECK(BoardLoad(&b,"test_records.txt") && b.count==2 && b.skipped==1 && b.entries[0].score==500);
    remove("test_records.txt");
}

static void TestMovementAndFire(void)
{
    Game g; EmptyGame(&g); g.spawned=0; g.spawnClock=10000;
    int start=g.player.cell;
    Run(&g,16,UP,false); Run(&g,1,-1,false); CHECK(g.player.cell==start-2*NAV_GRID);
    Run(&g,16,-1,false); CHECK(g.player.cell==g.player.dest && g.player.step==0);
    int cell=g.player.cell; g.map.tile[11*GRID+5]='W';
    Run(&g,3,RIGHT,false); CHECK(g.player.cell==cell && g.player.dest==cell && g.player.dir==RIGHT);
    Run(&g,1,-1,true); CHECK(Active(&g,0)==1 && g.bullets[0].dir==RIGHT);
    float y=g.bullets[0].y; Run(&g,16,-1,true);
    CHECK(Active(&g,0)==1 && fabsf(g.bullets[0].y-y)<.01f);
    g.stars=2; Run(&g,1,-1,true); CHECK(Active(&g,0)==2);
    GameFree(&g);

    EmptyGame(&g); g.spawnClock=10000; g.spawned=0;
    Enemy *e=GameSpawn(&g,MapNode(PLAYER_CELL-GRID),0); CHECK(e!=NULL);
    if (e) e->appear=1000;
    Run(&g,20,UP,false); CHECK(g.player.dest==MapNode(PLAYER_CELL));
    CHECK(!GameSpawn(&g,MapNode(PLAYER_CELL),0));
    CHECK(!GameSpawn(&g,-1,0) && !GameSpawn(&g,0,4)); GameFree(&g);

    EmptyGame(&g); g.spawned=0; g.spawnClock=10000;
    memset(g.map.tile,'S',CELLS); g.map.tile[BASE_CELL]='E';
    const int corridor[]={PLAYER_CELL,PLAYER_CELL-GRID,PLAYER_CELL-GRID+1,PLAYER_CELL+1};
    for (int i=0; i<4; i++) g.map.tile[corridor[i]]='.';
    const int turns[]={UP,RIGHT,DOWN,LEFT};
    for (int i=0; i<4; i++) {
        Run(&g,16,turns[i],false); Run(&g,1,-1,false);
        CHECK(g.player.cell==MapNode(corridor[(i+1)%4]) && g.player.dest==g.player.cell);
    }
    GameFree(&g);
}

static void TestTerrainAndBullets(void)
{
    Game g; EmptyGame(&g); int c=6*GRID+6;
    g.map.tile[c]='B'; g.map.brick[c]=15;
    Shot(&g,MapNode(c),UP,0,1); g.bullets[0].y=(c/GRID+1)*TILE-2; Run(&g,1,-1,false);
    CHECK(g.map.tile[c]=='B' && g.map.brick[c]==3 && !g.bullets[0].active);
    g.bullets[0].x = (c % GRID + .5f) * TILE;
    g.bullets[0].y = (c / GRID) * TILE + 10;
    g.bullets[0].dir = UP;
    g.bullets[0].speed = 6;
    g.bullets[0].owner = 0;
    g.bullets[0].power = 1;
    g.bullets[0].active = true;
    Run(&g,1,-1,false); CHECK(g.map.tile[c]=='.' && !g.map.brick[c]);
    g.map.tile[c]='S'; Shot(&g,MapNode(c),UP,0,1); Run(&g,1,-1,false);
    CHECK(g.map.tile[c]=='S' && !g.bullets[0].active);
    Shot(&g,MapNode(c),UP,0,2); Run(&g,1,-1,false); CHECK(g.map.tile[c]=='S' && g.map.steel[c]==12);
    Shot(&g,MapNode(c),DOWN,0,2); Run(&g,1,-1,false); CHECK(g.map.tile[c]=='.');
    g.map.tile[c]='W'; Shot(&g,MapNode(c),UP,0,1); Run(&g,1,-1,false);
    CHECK(g.bullets[0].active && g.map.tile[c]=='W');
    g.map.tile[c]='.'; Shot(&g,MapNode(c),UP,0,1);
    g.bullets[1]=g.bullets[0]; g.bullets[1].owner=12; g.bullets[1].dir=DOWN; g.bullets[1].y-=6;
    Run(&g,1,-1,false); CHECK(!g.bullets[0].active && !g.bullets[1].active);
    Shot(&g,MapNode(BASE_CELL),UP,0,1); Run(&g,1,-1,false);
    CHECK(!g.baseAlive && g.phase==LOST); GameFree(&g);
}

static void Travel(Game *g, int steps, int dir)
{
    Run(g,steps*8,dir,false); Run(g,1,-1,false);
}

static void TestWallPassages(void)
{
    Game g; EmptyGame(&g); CorridorMap(&g.map); g.spawned=0; g.spawnClock=10000;
    int start=2*NAV_GRID+7, corner=12*NAV_GRID+7, end=12*NAV_GRID+20;
    memset(&g.player, 0, sizeof(g.player)); g.player.cell=start; g.player.dest=start; g.player.dir=DOWN; GameFields(&g);
    CHECK(MapClear(&g.map,start,0) && MapClear(&g.map,corner,0));
    CHECK(!MapClear(&g.map,start-1,0) && !MapClear(&g.map,start+1,0));
    Field f; FieldBuild(&f,&g.map,end);
    CHECK(f.dist[start]==23 && f.tail<=NAV_CELLS);
    int path[NAV_CELLS], count=FieldPath(&f,start,path,NAV_CELLS); bool clear=true;
    for (int i=0; i<count; i++) if (!MapClear(&g.map,path[i],0)) clear=false;
    CHECK(count==24 && clear);
    Travel(&g,10,DOWN); CHECK(g.player.cell==corner && g.player.dest==corner);
    Travel(&g,13,RIGHT); CHECK(g.player.cell==end && g.player.dest==end);
    Travel(&g,13,LEFT); CHECK(g.player.cell==corner && g.player.dest==corner);
    Travel(&g,10,UP); CHECK(g.player.cell==start && g.player.dest==start);
    Run(&g,3,DOWN,false); float x=0,y=0,rx=0,ry=0; TankPosition(&g.player,&x,&y);
    Run(&g,1,UP,false); TankPosition(&g.player,&rx,&ry);
    CHECK(rx==x && ry<y); Run(&g,1,-1,false); CHECK(g.player.cell==start && g.player.dest==start);
    OpenRect(&g.map,300,40,20,60);
    CHECK(!MapClear(&g.map,2*NAV_GRID+14,0));
    GameFree(&g);

    const int ends[]={2*NAV_GRID+7,12*NAV_GRID+20,19*NAV_GRID+7,12*NAV_GRID+20,2*NAV_GRID+7};
    for (int type=0; type<4; type++) for (int route=0; route<4; route++) {
        EmptyGame(&g); CorridorMap(&g.map); g.spawned=0; g.spawnClock=10000; g.shield=10000;
        int goal=ends[route+1]; memset(&g.player, 0, sizeof(g.player)); g.player.cell=goal; g.player.dest=goal; g.player.dir=UP;
        g.serial=2; Enemy *e=GameSpawn(&g,ends[route],type); CHECK(e!=NULL);
        if (!e) { GameFree(&g); continue; }
        e->appear=0; GameFields(&g); bool passable=true;
        for (int t=0; t<800; t++) {
            GameStep(&g,(Input){-1,false});
            if (!MapClear(&g.map,e->tank.cell,0) || !MapClear(&g.map,e->tank.dest,0)) passable=false;
        }
        int distance=abs(e->tank.cell%NAV_GRID-goal%NAV_GRID)+abs(e->tank.cell/NAV_GRID-goal/NAV_GRID);
        if (!passable || distance>2 || g.phase!=PLAY)
            printf("Passage type=%d route=%d cell=%d goal=%d dist=%d clear=%d phase=%d\n",type,route,e->tank.cell,goal,distance,passable,g.phase);
        CHECK(passable && distance<=2 && g.phase==PLAY); GameFree(&g);
    }

    EmptyGame(&g); CorridorMap(&g.map); g.spawned=0; g.spawnClock=10000; g.shield=10000;
    memset(&g.player, 0, sizeof(g.player)); g.player.cell=19*NAV_GRID+7; g.player.dest=19*NAV_GRID+7; g.player.dir=UP;
    int left=5*GRID+3,right=5*GRID+4;
    g.map.brick[left]=10; g.map.brick[right]=5; g.serial=2;
    Enemy *e=GameSpawn(&g,8*NAV_GRID+7,0); CHECK(e!=NULL);
    CHECK(MapAt(&g.map,145,205)=='B' && MapAt(&g.map,125,205)=='S');
    if (e) {
        e->appear=0; GameFields(&g); Run(&g,800,-1,false);
        CHECK(!g.map.brick[left] && !g.map.brick[right]);
        CHECK(g.map.steel[left]==5 && g.map.steel[right]==10 && e->tank.cell/NAV_GRID>=16);
    }
    GameFree(&g);
}

static void TestFortWalls(void)
{
    Game g; EmptyGame(&g); g.spawned=0; g.spawnClock=10000;
    int fort=BASE_CELL-GRID;
    memset(&g.player, 0, sizeof(g.player)); g.player.cell=MapNode(fort); g.player.dest=MapNode(fort); g.player.dir=UP;
    Enemy *e=GameSpawn(&g,MapNode(BASE_CELL-1),0); CHECK(e!=NULL);
    GameItem(&g,SHOVEL);
    CHECK(MapClear(&g.map,g.player.cell,0) && MapClear(&g.map,g.player.dest,0));
    CHECK(e && MapClear(&g.map,e->tank.cell,0) && MapClear(&g.map,e->tank.dest,0));
    GameFree(&g);

    EmptyGame(&g); g.spawned=0; g.spawnClock=10000;
    g.map.tile[fort]='B'; g.map.brick[fort]=15; GameItem(&g,SHOVEL);
    g.map.tile[fort]='.'; g.map.brick[fort]=g.map.steel[fort]=0;
    memset(&g.player, 0, sizeof(g.player)); g.player.cell=MapNode(fort); g.player.dest=MapNode(fort); g.player.dir=UP;
    g.shovel=1; Run(&g,1,-1,false);
    CHECK(g.map.tile[fort]=='B' && g.map.brick[fort]==15);
    CHECK(MapClear(&g.map,g.player.cell,0) && g.player.cell!=MapNode(fort));
    GameFree(&g);

    EmptyGame(&g); g.spawned=0; g.spawnClock=10000; GameItem(&g,SHOVEL);
    e=GameSpawn(&g,0,0); CHECK(e!=NULL);
    if (e) { e->appear=0; Run(&g,100,-1,false); CHECK(e->tank.cell!=0 || e->tank.dest!=0); }
    GameFree(&g);
}

static void TestDamageAndItems(void)
{
    Game g; EmptyGame(&g); g.freeze=1000; g.spawned=0; g.spawnClock=10000;
    Enemy *e=GameSpawn(&g,MapNode(5*GRID+5),3); CHECK(e!=NULL); if (!e) return;
    e->appear=0;
    for (int i=0; i<3; i++) { Shot(&g,e->tank.cell,RIGHT,0,1); Run(&g,1,-1,false); }
    CHECK(e->hp==1 && g.live==1 && g.score==0);
    Shot(&g,e->tank.cell,RIGHT,0,1); Run(&g,1,-1,false);
    CHECK(g.live==0 && g.score==400 && g.kills[3]==1 && g.allocations==g.releases);
    for (int i=0; i<4; i++) CHECK(GameSpawn(&g,MapNode(i),0)!=NULL);
    CHECK(!GameSpawn(&g,MapNode(5),0) && g.live==4);
    GameItem(&g,GRENADE); CHECK(g.live==0 && g.score==900 && GameKills(&g)==5);
    GameItem(&g,STAR); GameItem(&g,STAR); GameItem(&g,STAR); GameItem(&g,STAR);
    CHECK(g.stars==3); GameItem(&g,HELMET); CHECK(g.shield==480);
    GameItem(&g,CLOCK); CHECK(g.freeze==420);
    GameItem(&g,EXTRA_LIFE); CHECK(g.lives==4);
    Map old=g.map; GameItem(&g,SHOVEL); CHECK(g.map.tile[BASE_CELL-GRID]=='S');
    GameItem(&g,SHOVEL); g.shovel=1; Run(&g,1,-1,false);
    CHECK(memcmp(old.tile,g.map.tile,CELLS)==0 && memcmp(old.brick,g.map.brick,CELLS)==0);
    g.shield=0; Shot(&g,g.player.cell,RIGHT,9,1); Run(&g,1,-1,false);
    CHECK(g.lives==3 && !g.playerAlive && g.stars==0);
    Run(&g,90,-1,false); CHECK(g.playerAlive && g.shield==180);
    Shot(&g,g.player.cell,RIGHT,9,1); Run(&g,1,-1,false); CHECK(g.lives==3);
    g.shield=0; g.lives=1; Shot(&g,g.player.cell,RIGHT,9,1); Run(&g,1,-1,false);
    CHECK(g.phase==LOST && !g.lives); GameFree(&g);

    EmptyGame(&g); g.serial=3; e=GameSpawn(&g,MapNode(5*GRID+5),3); g.freeze=1000;
    if (e) {
        e->appear=0; Shot(&g,e->tank.cell,RIGHT,0,1); Run(&g,1,-1,false);
        int count=0; for (int i=0; i<ITEMS; i++) count+=g.items[i].life>0;
        CHECK(count==1 && !e->bonus && e->hp==3);
    }
    GameFree(&g);
}

static void TestLifecycle(void)
{
    Game g; EmptyGame(&g); g.spawned=0; g.spawnClock=1;
    Run(&g,1,-1,false); CHECK(g.spawned==1 && g.live==1);
    GameFree(&g); failAllocation=true;
    CHECK(!GameSpawn(&g,0,0) && g.memoryError && g.phase==PAUSED);
    failAllocation=false; GameFree(&g);
    EmptyGame(&g); g.score=100; g.lives=2; g.stars=2;
    Run(&g,89,-1,false); CHECK(g.phase==PLAY);
    Run(&g,1,-1,false); CHECK(g.phase==CLEAR);
    GameNext(&g); CHECK(g.stage==2 && g.phase==READY && g.score==100 && g.lives==2 && g.stars==2);
    g.stage=3; g.phase=CLEAR; GameNext(&g); CHECK(g.phase==WON);
    GameFree(&g);
    GameInit(&g,1); GameStart(&g,1); g.phase=PLAY;
    Run(&g,18000,-1,false); CHECK(g.phase==LOST); GameFree(&g);
}

static unsigned Next(unsigned *seed)
{
    *seed=*seed*1664525u+1013904223u; return *seed;
}

static void TestSimulation(void)
{
    bool invariant=true; int wins=0,maxKills=0;
    for (int seed=1; seed<=32; seed++) {
        Game g; GameInit(&g,(unsigned)seed); GameStart(&g,(unsigned)seed); g.phase=PLAY;
        unsigned random=(unsigned)seed;
        for (int t=0; t<30000 && g.phase==PLAY; t++) {
            int dir=seed<=16 ? -1 : (int)(Next(&random)%6)-1;
            GameStep(&g,(Input){dir,true});
            if (g.playerAlive && (!MapClear(&g.map,g.player.cell,0) || !MapClear(&g.map,g.player.dest,0))) invariant=false;
            int count=0;
            for (Enemy *e=g.enemies; e; e=e->next) {
                count++; if (count>ALIVE_LIMIT || e->hp<1 || e->hp>4 || e->tank.cell<0 || e->tank.dest>=NAV_CELLS) { invariant=false; break; }
                if (!MapClear(&g.map,e->tank.cell,0) || !MapClear(&g.map,e->tank.dest,0)) invariant=false;
                for (Enemy *f=e->next; f; f=f->next)
                    if (e->tank.cell==f->tank.cell || e->tank.dest==f->tank.dest) invariant=false;
            }
            if (count!=g.live || g.allocations-g.releases!=count || g.spawned>ENEMY_TOTAL || g.lives<0) invariant=false;
        }
        if (g.phase==CLEAR || g.phase==WON) wins++;
        if (seed<=16 && GameKills(&g)>maxKills) maxKills=GameKills(&g);
        if (seed<=16 && g.phase!=LOST) invariant=false;
        GameFree(&g); if (g.allocations!=g.releases) invariant=false;
    }
    CHECK(invariant);
    printf("Simulation: 32 seeded runs; stationary fire max kills=%d/%d; clears=%d.\n",maxKills,ENEMY_TOTAL,wins);
}

int main(int argc, char **argv)
{
    if (argc!=2 || strcmp(argv[1],"--test-hidden")) { fprintf(stderr,"Use --test-hidden in an isolated test directory.\n"); return 2; }
    TestMaps(); TestFields(); TestRecords(); TestMovementAndFire(); TestTerrainAndBullets();
    TestWallPassages(); TestFortWalls(); TestDamageAndItems(); TestLifecycle(); TestSimulation();
    printf("Core checks: %d/%d passed\n",checks-failures,checks);
    return failures ? 1 : 0;
}
