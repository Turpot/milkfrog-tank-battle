#include "ui.h"
#include "generated_glyphs.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const Color PAPER={224,229,218,255}, INK={28,40,38,255};
static const Color GREY={173,177,171,255}, BRICK={81,116,108,255}, CYAN={105,219,220,255};

static void Text(App *a, const char *s, float x, float y, float size, Color color)
{
    Vector2 bounds=MeasureTextEx(a->font,s,size,0);
    if (x<0 || y<0 || x+bounds.x>WIDTH+1 || y+bounds.y>HEIGHT+1) a->layoutErrors++;
    DrawTextEx(a->font,s,(Vector2){x,y},size,0,color);
}

static void Center(App *a, const char *s, float x, float y, float size, Color color)
{
    Text(a,s,x-MeasureTextEx(a->font,s,size,0).x/2,y,size,color);
}

static void Sprite(App *a, float x, float y, int kind, int dir, int frame, float size)
{
    DrawTexturePro(a->sprites,(Rectangle){kind*96.0f,frame*96.0f,96,96},
        (Rectangle){x,y,size,size},(Vector2){size/2,size/2},dir*90.0f-90,WHITE);
}

static void Unit(App *a, const Tank *t, int kind)
{
    float x=0,y=0; TankPosition(t,&x,&y);
    int frame=t->cell!=t->dest ? (t->step/8)%2 : 0;
    Sprite(a,FIELD_X+x,FIELD_Y+y,kind,t->dir,frame,52);
}

static void Bits(const unsigned short *rows, int height, int width, int x, int y, int scale, Color color)
{
    for (int r=0; r<height; r++) for (int c=0; c<width; c++)
        if (rows[r] & (1u<<(width-c-1))) DrawRectangle(x+c*scale,y+r*scale,scale,scale,color);
}

static void BaseIcon(int x, int y, bool alive)
{
    static const unsigned short shape[13]={0x0040,0x0040,0x00e0,0x00e0,0x1fff,0x0ffe,0x07fc,
        0x03f8,0x03f8,0x07fc,0x071c,0x060c,0x0404};
    if (alive) Bits(shape,13,13,x+7,y+7,2,GOLD);
    else { DrawLineEx((Vector2){x+8,y+8},(Vector2){x+32,y+32},4,BRICK);
           DrawLineEx((Vector2){x+32,y+8},(Vector2){x+8,y+32},4,BRICK); }
}

static void Tile(char type, unsigned mask, int x, int y)
{
    if (type=='B') {
        for (int q=0; q<4; q++) if (mask & (1u<<q)) {
            int px=x+(q%2)*20,py=y+(q/2)*20;
            DrawRectangle(px,py,20,20,BRICK);
            DrawLine(px,py,px+19,py,(Color){136,164,142,255});
            DrawLine(px,py+10,px+19,py+10,(Color){42,63,60,255});
            DrawLine(px+10,py,px+10,py+9,(Color){42,63,60,255});
            DrawLine(px+4,py+11,px+4,py+19,(Color){42,63,60,255});
        }
    } else if (type=='S') {
        for (int q=0; q<4; q++) if (mask & (1u<<q)) {
            int px=x+(q%2)*20,py=y+(q/2)*20;
            DrawRectangle(px,py,20,20,(Color){119,126,142,255});
            DrawRectangle(px+3,py+3,13,13,(Color){177,184,204,255});
            DrawLine(px+2,py+2,px+17,py+2,WHITE);
            DrawLine(px+2,py+3,px+2,py+17,PAPER);
        }
    } else if (type=='W') {
        DrawRectangle(x,y,TILE,TILE,(Color){43,99,143,255});
        for (int r=0; r<4; r++) for (int c=0; c<2; c++)
            DrawLine(x+3+c*20,y+6+r*10,x+14+c*20,y+6+r*10,(Color){119,164,225,255});
    } else if (type=='T') {
        for (int r=0; r<5; r++) for (int c=0; c<5; c++) {
            DrawRectangle(x+c*8,y+r*8,7,7,(Color){35,93,46,255});
            DrawRectangle(x+c*8+2,y+r*8,4,4,(Color){77,145,64,255});
        }
    }
}

static void ItemIcon(int type, int x, int y)
{
    static const unsigned short shapes[ITEM_TYPES][12]={
        {0x020,0x020,0x070,0x1fc,0x0f8,0x070,0x0d8,0x108,0,0,0,0},
        {0x030,0x038,0x070,0x0f8,0x1fc,0x1fc,0x1fc,0x0f8,0,0,0,0},
        {0,0x070,0x0f8,0x1fc,0x1fc,0x3fe,0x202,0,0,0,0,0},
        {0x020,0x020,0x020,0x020,0x070,0x070,0x070,0x020,0,0,0,0},
        {0x0f8,0x104,0x222,0x222,0x23a,0x202,0x104,0x0f8,0,0,0,0},
        {0x020,0x0a8,0x0a8,0x0f8,0x1fc,0x1fc,0x104,0x1fc,0,0,0,0}
    };
    DrawRectangle(x,y,30,30,(Color){31,53,100,255});
    DrawRectangleLines(x,y,30,30,PAPER);
    Bits(shapes[type],10,10,x+5,y+5,2,type==STAR ? GOLD : PAPER);
}

bool AppInit(App *a, bool hidden)
{
    if (!a) return false;
    memset(a, 0, sizeof(*a));
    a->hidden = hidden;
    a->screen = MENU;
    a->lastDir = UP;
    a->seed = 1;
    GameInit(&a->game,1); BoardLoad(&a->board,"records.txt");
    if (!FileExists("assets/font/ui.ttf") || !FileExists("assets/tanks.png")) return false;
    const char *windows=getenv("WINDIR"), *font="assets/font/ui.ttf";
    if (windows && FileExists(TextFormat("%s/Fonts/simhei.ttf",windows))) font=TextFormat("%s/Fonts/simhei.ttf",windows);
    a->font=LoadFontEx(font,48,(int *)GLYPHS,GLYPH_COUNT);
    a->sprites=LoadTexture("assets/tanks.png");
    if (!a->font.texture.id || !a->sprites.id) return false;
    SetTextureFilter(a->font.texture,TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(a->sprites,TEXTURE_FILTER_POINT);
    if (!hidden) {
        InitAudioDevice(); a->audio=IsAudioDeviceReady();
        if (a->audio) for (int n=0; n<5; n++) {
            short samples[3300]; int length=n==4 ? 3300 : 1500;
            for (int i=0; i<length; i++) {
                int period=12+n*9+(n==2 ? i/80 : 0);
                samples[i]=(short)((i/period%2 ? 1 : -1)*(length-i)*2);
            }
            a->sound[n]=LoadSoundFromWave((Wave){(unsigned)length,22050,16,1,samples});
        }
    }
    return true;
}

void AppFree(App *a)
{
    if (!a) return;
    GameFree(&a->game);
    if (a->sprites.id) UnloadTexture(a->sprites);
    if (a->font.texture.id) UnloadFont(a->font);
    if (a->audio) { for (int i=0; i<5; i++) UnloadSound(a->sound[i]); CloseAudioDevice(); }
}

void AppNewGame(App *a)
{
    GameStart(&a->game,++a->seed); a->screen=BATTLE;
    a->audit=a->saved=a->saveAttempted=false; a->pauseMenu=0; a->lastDir=UP;
}

static void SaveRecord(App *a)
{
    if (a->hidden || a->saved) return;
    a->saveAttempted=true;
    a->saved=BoardSave(&a->board,(Record){a->game.score,a->game.stage,GameKills(&a->game)},"records.txt");
}

void AppAfterStep(App *a)
{
    if (!a) return;
    unsigned sounds=a->game.sounds; a->game.sounds=0;
    if ((a->game.phase==LOST || a->game.phase==WON) && !a->saveAttempted) SaveRecord(a);
    if (a->audio && !a->muted && sounds) {
        int n=sounds&S_END ? 4 : sounds&S_ITEM ? 3 : sounds&S_KILL ? 2 : sounds&S_HIT ? 1 : 0;
        PlaySound(a->sound[n]);
    }
}

static void AuditToggle(App *a)
{
    if (a->audit) { GameFields(&a->game); a->game.phase=a->auditReturn; a->audit=false; }
    else { a->auditReturn=a->game.phase; a->game.phase=PAUSED; a->audit=true; FieldBegin(&a->game.baseField,MapNode(BASE_CELL)); }
}

void AppInput(App *a)
{
    if (IsKeyPressed(KEY_M)) a->muted=!a->muted;
    if (a->screen==MENU) {
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) a->menu=(a->menu+3)%4;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) a->menu=(a->menu+1)%4;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            if (a->menu==0) AppNewGame(a);
            else if (a->menu==1) a->screen=HELP;
            else if (a->menu==2) { BoardLoad(&a->board,"records.txt"); a->screen=SCORES; }
            else a->quit=true;
        }
        return;
    }
    if (a->screen==HELP || a->screen==SCORES) {
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) a->screen=MENU;
        return;
    }
    Game *g=&a->game;
    if (IsKeyPressed(KEY_B) && (g->phase==PLAY || g->phase==PAUSED || g->phase==READY)) AuditToggle(a);
    if (a->audit) {
        if (IsKeyPressed(KEY_N)) FieldLayer(&g->baseField,&g->map);
        if (IsKeyPressed(KEY_R)) FieldBegin(&g->baseField,MapNode(BASE_CELL));
        if (IsKeyPressed(KEY_ESCAPE)) AuditToggle(a);
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (g->phase==PLAY) { g->phase=PAUSED; a->pauseMenu=0; }
        else if (g->phase==PAUSED && !g->memoryError) g->phase=PLAY;
        else if (g->phase==READY) a->screen=MENU;
        return;
    }
    if (g->phase==PAUSED) {
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) a->pauseMenu=(a->pauseMenu+2)%3;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) a->pauseMenu=(a->pauseMenu+1)%3;
        if (IsKeyPressed(KEY_ENTER)) {
            if (!a->pauseMenu && !g->memoryError) g->phase=PLAY;
            else if (a->pauseMenu==1) AppNewGame(a);
            else if (a->pauseMenu==2) { GameFree(g); a->screen=MENU; }
        }
        return;
    }
    if (g->phase==READY) { if (IsKeyPressed(KEY_ENTER)) g->phase=PLAY; return; }
    if (g->phase==CLEAR) { if (IsKeyPressed(KEY_ENTER)) GameNext(g); return; }
    if (g->phase==LOST || g->phase==WON) {
        if (IsKeyPressed(KEY_S) && !a->saved) SaveRecord(a);
        if (IsKeyPressed(KEY_R)) AppNewGame(a);
        else if (IsKeyPressed(KEY_ENTER)) a->screen=MENU;
        return;
    }
    const int keys[4]={KEY_UP,KEY_RIGHT,KEY_DOWN,KEY_LEFT};
    const int letters[4]={KEY_W,KEY_D,KEY_S,KEY_A};
    for (int d=0; d<4; d++) if (IsKeyPressed(keys[d]) || IsKeyPressed(letters[d])) a->lastDir=d;
    int dir=-1;
    if (IsKeyDown(keys[a->lastDir]) || IsKeyDown(letters[a->lastDir])) dir=a->lastDir;
    else for (int d=0; d<4; d++) if (IsKeyDown(keys[d]) || IsKeyDown(letters[d])) { dir=d; break; }
    a->input=(Input){dir,IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_J)};
}

static void DrawMenu(App *a)
{
    ClearBackground((Color){12,15,16,255});
    Center(a,TextFormat("最高分  %06d",a->board.count ? a->board.entries[0].score : 0),400,80,20,PAPER);
    Center(a,GAME_TITLE,400,144,44,PAPER);
    Sprite(a,400,222,0,RIGHT,0,110);
    const char *labels[]={"开始游戏","操作说明","战绩记录","退出游戏"};
    for (int i=0; i<4; i++) {
        Center(a,labels[i],410,300+i*46,24,a->menu==i ? GOLD : PAPER);
        if (a->menu==i) Sprite(a,310,315+i*46,0,RIGHT,0,48);
    }
    Center(a,"方向键选择   Enter 确认",400,552,17,GREY);
    Center(a,"WASD 移动   J / 空格 开炮",400,582,17,GREY);
}

static void DrawHelp(App *a)
{
    ClearBackground((Color){12,15,16,255});
    Center(a,"操作说明",400,48,30,PAPER);
    const char *left[]={"WASD / 方向键","J / 空格","Enter / Esc","B / N / R","M"};
    const char *right[]={"移动、转向","沿朝向开炮","确认、暂停","寻路视图、单步、重算","声音开关"};
    for (int i=0; i<5; i++) { Text(a,left[i],100,110+i*34,19,GOLD); Text(a,right[i],330,110+i*34,19,PAPER); }
    Text(a,"守住基地，消灭本关全部敌人。基地中弹或生命耗尽，本局结束。",70,302,18,PAPER);
    const char types[]={'B','S','W','T'}; const char *names[]={"砖墙","钢板","河流","树林"};
    for (int i=0; i<4; i++) { Tile(types[i],15,92+i*180,352); Text(a,names[i],142+i*180,362,18,PAPER); }
    const char *itemNames[]={"星星","手雷","头盔","铁锹","时钟","生命"};
    const char *effects[]={"升级火力","清除敌人","短时护盾","加固基地","暂停敌人","增加生命"};
    for (int i=0; i<6; i++) {
        int x=56+i*120; ItemIcon(i,x+26,432); Center(a,itemNames[i],x+41,472,18,GOLD);
        Center(a,effects[i],x+41,500,16,PAPER);
    }
    Center(a,"Enter / Esc 返回",400,582,18,GREY);
}

static void DrawScores(App *a)
{
    ClearBackground((Color){12,15,16,255}); Center(a,"战绩记录",400,54,30,PAPER);
    Text(a,"名次",150,122,19,GOLD); Text(a,"分数",290,122,19,GOLD);
    Text(a,"关卡",460,122,19,GOLD); Text(a,"击毁",580,122,19,GOLD);
    for (int i=0; i<a->board.count; i++) {
        Record r=a->board.entries[i]; int y=166+i*33;
        Text(a,TextFormat("%02d",i+1),160,y,20,PAPER); Text(a,TextFormat("%06d",r.score),280,y,20,PAPER);
        Text(a,TextFormat("%d",r.stage),480,y,20,PAPER); Text(a,TextFormat("%02d",r.kills),590,y,20,PAPER);
    }
    if (!a->board.count) Center(a,"暂无战绩",400,280,22,GREY);
    if (a->board.skipped) Center(a,TextFormat("已跳过 %d 条无效记录",a->board.skipped),400,540,17,GREY);
    Center(a,"Enter / Esc 返回",400,582,18,GREY);
}

static void DrawField(App *a)
{
    Game *g=&a->game; DrawRectangle(FIELD_X,FIELD_Y,GRID*TILE,GRID*TILE,(Color){22,31,29,255});
    for (int c=0; c<CELLS; c++) {
        int x=FIELD_X+c%GRID*TILE,y=FIELD_Y+c/GRID*TILE;
        if (g->map.tile[c]=='B' || g->map.tile[c]=='S') {
            Tile('B',g->map.brick[c],x,y);
            Tile('S',g->map.steel[c] ? g->map.steel[c] : (g->map.tile[c]=='S' ? 15u : 0u),x,y);
        } else if (g->map.tile[c]!='T') Tile(g->map.tile[c],0,x,y);
    }
    if (a->audit) for (int c=0; c<NAV_CELLS; c++) if (g->baseField.dist[c]>=0) {
        int x=FIELD_X+(c%NAV_GRID+1)*STEP, y=FIELD_Y+(c/NAV_GRID+1)*STEP;
        int n=g->baseField.dist[c];
        Text(a,TextFormat("%d",n),x-7,y-7,11,MapClear(&g->map,c,0) ? (Color){116,174,126,255} : GOLD);
    }
    BaseIcon(FIELD_X+6*TILE,FIELD_Y+12*TILE,g->baseAlive);
    if (a->audit) {
        Field *f=&g->baseField;
        for (int i=f->head; i<f->tail; i++) {
            int c=f->queue[i]; DrawRectangleLines(FIELD_X+(c%NAV_GRID+1)*STEP-9,FIELD_Y+(c/NAV_GRID+1)*STEP-9,18,18,CYAN);
        }
        int path[NAV_CELLS]; int count=FieldPath(f,g->enemies ? g->enemies->tank.cell : MapNode(6),path,NAV_CELLS);
        for (int i=1; i<count; i++) DrawLineEx(
            (Vector2){FIELD_X+(path[i-1]%NAV_GRID+1)*STEP,FIELD_Y+(path[i-1]/NAV_GRID+1)*STEP},
            (Vector2){FIELD_X+(path[i]%NAV_GRID+1)*STEP,FIELD_Y+(path[i]/NAV_GRID+1)*STEP},2,CYAN);
    }
    for (int i=0; i<ITEMS; i++) if (g->items[i].life) {
        int c=g->items[i].cell; ItemIcon(g->items[i].type,FIELD_X+c%GRID*TILE+5,FIELD_Y+c/GRID*TILE+5);
    }
    for (Enemy *e=g->enemies; e; e=e->next) {
        if (e->appear) {
            int x=FIELD_X+(e->tank.cell%NAV_GRID+1)*STEP-20,y=FIELD_Y+(e->tank.cell/NAV_GRID+1)*STEP-20;
            DrawRectangleLines(x+6,y+6,28,28,PAPER); continue;
        }
        Unit(a,&e->tank,e->type+1);
        float x=0,y=0; TankPosition(&e->tank,&x,&y);
        if (e->bonus) DrawRectangleLines(FIELD_X+(int)x-17,FIELD_Y+(int)y-17,34,34,BRICK);
        if (e->type==3) for (int i=0; i<e->hp; i++) DrawRectangle(FIELD_X+(int)x-9+i*5,FIELD_Y+(int)y+15,3,2,PAPER);
    }
    if (g->playerAlive) {
        Unit(a,&g->player,0);
        if (g->shield) {
            float x=0,y=0; TankPosition(&g->player,&x,&y);
            DrawRectangleLines(FIELD_X+(int)x-19,FIELD_Y+(int)y-19,38,38,GOLD);
        }
    }
    for (int i=0; i<BULLETS; i++) if (g->bullets[i].active) {
        Bullet b=g->bullets[i]; DrawRectangle(FIELD_X+(int)b.x-2,FIELD_Y+(int)b.y-2,4,4,b.owner ? PAPER : GOLD);
    }
    for (int c=0; c<CELLS; c++) if (g->map.tile[c]=='T') Tile('T',0,FIELD_X+c%GRID*TILE,FIELD_Y+c/GRID*TILE);
}

static void DrawSidebar(App *a)
{
    Game *g=&a->game; int x=572;
    Text(a,TextFormat("第 %d 关",g->stage),x,66,27,INK);
    if (a->audit) {
        Text(a,"基地距离场",x,124,21,INK);
        Text(a,TextFormat("队头  %d",g->baseField.head),x,166,18,INK);
        Text(a,TextFormat("队尾  %d",g->baseField.tail),x,196,18,INK);
        Text(a,g->baseField.head==g->baseField.tail ? "搜索完成" : "等待下一层",x,230,18,INK);
        Text(a,"敌人链表",x,284,21,INK);
        const Enemy *e=g->enemies;
        for (int i=0; i<4; i++,e=e ? e->next : NULL) {
            if (e) {
                if (e->next) Text(a,TextFormat("#%02d -> #%02d",e->id,e->next->id),x,320+i*26,16,INK);
                else Text(a,TextFormat("#%02d -> NULL",e->id),x,320+i*26,16,INK);
            }
        }
        if (!g->enemies) Text(a,"head -> NULL",x,320,17,INK);
        Text(a,TextFormat("申请 %d  释放 %d",g->allocations,g->releases),x,436,16,INK);
        Text(a,"N  下一层",x,484,17,INK); Text(a,"R  重新搜索",x,514,17,INK);
        Text(a,"B  返回",x,554,17,INK); return;
    }
    Text(a,"分数",x,124,18,INK); Text(a,TextFormat("%06d",g->score),x,154,26,INK);
    Text(a,"待出击",x,210,18,INK);
    for (int i=0; i<ENEMY_TOTAL-g->spawned; i++) {
        int px=x+(i%5)*25,py=247+(i/5)*19;
        DrawRectangle(px,py,4,14,INK); DrawRectangle(px+11,py,4,14,INK);
        DrawRectangle(px+5,py+3,5,7,INK); DrawRectangle(px+7,py-3,2,7,INK);
    }
    Text(a,TextFormat("场上 %d / %d",g->live,ALIVE_LIMIT),x,344,18,INK);
    Text(a,TextFormat("生命  %d",g->lives),x,388,20,INK);
    Text(a,TextFormat("火力  %d",g->stars),x,422,20,INK);
    if (g->freeze) Text(a,TextFormat("冻结  %d",(g->freeze+59)/60),x,462,17,INK);
    else if (g->shovel) Text(a,TextFormat("加固  %d",(g->shovel+59)/60),x,462,17,INK);
    Text(a,"Esc  暂停",x,510,17,INK); Text(a,"B    寻路",x,540,17,INK);
    Text(a,a->muted ? "M    静音" : "M    声音开",x,570,17,INK);
}

static void DrawOverlay(App *a)
{
    Game *g=&a->game;
    if (a->audit || g->phase==PLAY) return;
    DrawRectangle(FIELD_X+24,FIELD_Y+140,472,252,(Color){8,10,11,243});
    float center=FIELD_X+260;
    if (g->phase==READY) {
        Center(a,TextFormat("第 %d 关",g->stage),center,232,32,PAPER);
        Center(a,"WASD 移动   J / 空格 开炮",center,298,18,PAPER);
        Center(a,TextFormat("保护基地，消灭 %d 个敌人",ENEMY_TOTAL),center,332,18,PAPER);
        Center(a,"Enter 出发",center,390,21,GOLD);
    } else if (g->phase==PAUSED) {
        Center(a,g->memoryError ? "内存不足，已暂停" : "暂停",center,226,27,PAPER);
        const char *labels[]={"继续游戏","重新开始","返回主菜单"};
        for (int i=0; i<3; i++) Center(a,labels[i],center,280+i*42,21,a->pauseMenu==i ? GOLD : PAPER);
    } else {
        const char *title=g->phase==CLEAR ? "本关完成" : g->phase==WON ? "三关完成" : g->baseAlive ? "生命耗尽" : "基地被击毁";
        Center(a,title,center,226,30,PAPER);
        Center(a,TextFormat("分数 %06d    击毁 %d",g->score,GameKills(g)),center,282,21,PAPER);
        if (g->phase==CLEAR) Center(a,g->stage<STAGES ? "Enter 下一关" : "Enter 结算",center,364,21,GOLD);
        else {
            if (!a->hidden) Center(a,a->saved ? "战绩已记录" : "保存失败，S 重试",center,324,17,GREY);
            Center(a,"R 重新开始    Enter 返回",center,378,19,GOLD);
        }
    }
}

void AppDraw(App *a)
{
    if (a->screen==MENU) { DrawMenu(a); return; }
    if (a->screen==HELP) { DrawHelp(a); return; }
    if (a->screen==SCORES) { DrawScores(a); return; }
    ClearBackground(GREY); Text(a,GAME_TITLE,24,18,25,INK);
    DrawField(a); DrawSidebar(a); DrawOverlay(a);
    if (a->audit) Text(a,"蓝框为待访问队列，金色数字表示可拆砖墙。",24,608,17,INK);
    else if (a->game.mapFallback) Text(a,"地图文件未读入，当前使用内置关卡。",24,608,17,INK);
    else Text(a,"WASD / 方向键 移动     J / 空格 开炮",24,608,17,INK);
}
