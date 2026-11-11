#include "ui.h"
#include <stdio.h>
#include <string.h>
#include "wall_fixture.h"

static int failures, scenes;
static RenderTexture2D canvas;

static Image Capture(App *a, const char *name)
{
    a->layoutErrors=0;
    BeginTextureMode(canvas); AppDraw(a); EndTextureMode();
    Image image=LoadImageFromTexture(canvas.texture); ImageFlipVertical(&image);
    if (a->layoutErrors) { printf("FAIL %s: %d text bounds\n",name,a->layoutErrors); failures++; }
    const int widths[]={800,640,1024}, heights[]={640,512,768};
    for (int i=0; i<3; i++) {
        Image output=GenImageColor(widths[i],heights[i],BLACK), scaled=ImageCopy(image);
        int w=i==2 ? 960 : widths[i],h=i==2 ? 768 : heights[i];
        ImageResize(&scaled,w,h);
        ImageDrawImage(&output,scaled,(widths[i]-w)/2,0,WHITE);
        char path[160]; snprintf(path,sizeof(path),"screenshots/%s_%dx%d.png",name,widths[i],heights[i]);
        if (!ExportImage(output,path)) failures++;
        UnloadImage(scaled); UnloadImage(output);
    }
    Color *pixels=LoadImageColors(image); int colored=0;
    for (int i=0; i<WIDTH*HEIGHT; i++) if (pixels[i].r>40 || pixels[i].g>40 || pixels[i].b>40) colored++;
    if (colored<2000) { printf("FAIL blank scene: %s\n",name); failures++; }
    UnloadImageColors(pixels); scenes++; return image;
}

static void Shot(App *a, const char *name) { Image i=Capture(a,name); UnloadImage(i); }

int main(int argc, char **argv)
{
    if (argc!=2 || strcmp(argv[1],"--test-hidden")) return 2;
    SetTraceLogLevel(LOG_WARNING); SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(WIDTH,HEIGHT,"View checks"); ChangeDirectory(GetApplicationDirectory());
    MakeDirectory("screenshots"); canvas=LoadRenderTexture(WIDTH,HEIGHT);
    App a; if (!AppInit(&a,true)) { printf("FAIL assets\n"); CloseWindow(); return 1; }
    Image atlas=LoadImageFromTexture(a.sprites); Color *atlasPixels=LoadImageColors(atlas); int trackPixels=0;
    for (int y=0; y<96; y++) for (int x=0; x<96; x++)
        if (memcmp(&atlasPixels[y*480+x],&atlasPixels[(y+96)*480+x],sizeof(Color))) trackPixels++;
    if (trackPixels<20) { printf("FAIL missing tread frames\n"); failures++; }
    int minX=96,maxX=0,minY=96,maxY=0,yellow=0;
    for (int y=0; y<96; y++) for (int x=0; x<96; x++) {
        Color p=atlasPixels[y*480+x];
        if (p.a) { if (x<minX) minX=x; if (x>maxX) maxX=x; if (y<minY) minY=y; if (y>maxY) maxY=y; }
        if (p.r>150 && p.g>140 && p.b<130) yellow++;
    }
    if ((maxX-minX+1)*52.0f/96>34 || (maxY-minY+1)*52.0f/96>34 || !yellow) {
        printf("FAIL player size or yellow palette\n"); failures++;
    }
    UnloadImageColors(atlasPixels); UnloadImage(atlas);
    Shot(&a,"menu"); a.screen=HELP; Shot(&a,"help"); a.screen=SCORES; Shot(&a,"scores_empty");
    for (int i=0; i<10; i++) BoardInsert(&a.board,(Record){1000+i*300,1+i%3,i*2});
    a.board.skipped=3; Shot(&a,"scores_full");
    AppNewGame(&a); Shot(&a,"stage1_ready"); a.game.phase=PLAY;
    for (int i=0; i<220; i++) GameStep(&a.game,(Input){-1,true});
    Shot(&a,"stage1_battle"); a.game.phase=PAUSED; Shot(&a,"pause");
    a.game.memoryError=true; Shot(&a,"memory_error"); a.game.memoryError=false;
    a.audit=true; FieldBegin(&a.game.baseField,MapNode(BASE_CELL)); Shot(&a,"bfs_start");
    for (int i=0; i<10; i++) FieldLayer(&a.game.baseField,&a.game.map);
    Shot(&a,"bfs_layer10"); GameFields(&a.game); Shot(&a,"bfs_complete"); a.audit=false;
    a.game.phase=CLEAR; Shot(&a,"clear"); a.game.phase=LOST; a.game.baseAlive=false; Shot(&a,"base_lost");
    a.game.phase=WON; Shot(&a,"won");
    for (int s=2; s<=3; s++) {
        GameLoadStage(&a.game,s); char name[40]; snprintf(name,sizeof(name),"stage%d_ready",s); Shot(&a,name);
        a.game.phase=PLAY; for (int i=0; i<200; i++) GameStep(&a.game,(Input){-1,true});
        snprintf(name,sizeof(name),"stage%d_battle",s); Shot(&a,name);
    }
    GameFree(&a.game); memset(a.game.map.tile,'.',CELLS); memset(a.game.map.brick,0,CELLS);
    memset(a.game.map.steel,0,CELLS);
    memset(&a.game.player, 0, sizeof(a.game.player));
    a.game.player.cell = MapNode(PLAYER_CELL);
    a.game.player.dest = MapNode(PLAYER_CELL) - NAV_GRID;
    a.game.player.frames = 16;
    a.game.player.dir = UP;
    a.game.playerAlive=true; a.game.phase=PLAY; a.game.spawned=ENEMY_TOTAL;
    Image first=Capture(&a,"treads_0"); a.game.player.step=8;
    Image second=Capture(&a,"treads_4");
    Color *p=LoadImageColors(first),*q=LoadImageColors(second); int difference=0;
    for (int i=0; i<WIDTH*HEIGHT; i++) if (memcmp(&p[i],&q[i],sizeof(Color))) difference++;
    if (difference<20) { printf("FAIL moving tank pixels\n"); failures++; }
    UnloadImageColors(p); UnloadImageColors(q); UnloadImage(first); UnloadImage(second);
    GameFree(&a.game); CorridorMap(&a.game.map); a.game.phase=PLAY;
    a.game.spawned=0; a.game.spawnClock=10000; a.game.shield=10000;
    memset(&a.game.player, 0, sizeof(a.game.player));
    a.game.player.cell = 12 * NAV_GRID + 20;
    a.game.player.dest = a.game.player.cell;
    a.game.player.dir = LEFT;
    a.game.serial=2; Enemy *enemy=GameSpawn(&a.game,2*NAV_GRID+7,0);
    if (enemy) enemy->appear=0;
    GameFields(&a.game); Shot(&a,"offset_passage");
    for (int i=0; i<220; i++) GameStep(&a.game,(Input){-1,false});
    Shot(&a,"offset_passage_enemy_turn");
    a.audit=true; Shot(&a,"offset_passage_bfs"); a.audit=false;
    a.game.player.dest=a.game.player.cell; a.game.player.step=0;
    first=Capture(&a,"stationary_0"); a.game.tick+=120; second=Capture(&a,"stationary_120");
    p=LoadImageColors(first); q=LoadImageColors(second);
    if (memcmp(p,q,WIDTH*HEIGHT*sizeof(Color))) { printf("FAIL stationary image changed\n"); failures++; }
    UnloadImageColors(p); UnloadImageColors(q); UnloadImage(first); UnloadImage(second);
    AppFree(&a); UnloadRenderTexture(canvas); CloseWindow();
    printf("View checks: %d scenes, %d images, %d failures\n",scenes,scenes*3,failures);
    return failures ? 1 : 0;
}
