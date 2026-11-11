#include "ui.h"
#include <stdio.h>
#include <string.h>

static int checks, failures, pressed=-1, held=-1;
#define CHECK(condition) do { checks++; if (!(condition)) { failures++; \
    printf("FAIL line %d: %s\n",__LINE__,#condition); } } while (0)

bool __wrap_IsKeyPressed(int key) { return key==pressed; }
bool __wrap_IsKeyDown(int key) { return key==held || key==pressed; }

static void Key(App *app, int key)
{
    pressed=key; app->input.dir=-1; app->input.fire=false; AppInput(app); pressed=-1;
}

int main(int argc, char **argv)
{
    if (argc!=2 || strcmp(argv[1],"--test-hidden")) return 2;
    SetTraceLogLevel(LOG_WARNING); SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(WIDTH,HEIGHT,"Input checks"); ChangeDirectory(GetApplicationDirectory());
    App app;
    if (!AppInit(&app,false)) { CloseWindow(); return 1; }
    app.hidden=true;
    CHECK(app.screen==MENU && app.audio);
    for (int i=0; i<5; i++) CHECK(IsSoundValid(app.sound[i]));
    Key(&app,KEY_DOWN); Key(&app,KEY_ENTER); CHECK(app.screen==HELP);
    Key(&app,KEY_ESCAPE); Key(&app,KEY_DOWN); Key(&app,KEY_ENTER); CHECK(app.screen==SCORES);
    Key(&app,KEY_ENTER); Key(&app,KEY_DOWN); CHECK(app.menu==3);
    Key(&app,KEY_DOWN); Key(&app,KEY_ENTER); CHECK(app.screen==BATTLE && app.game.phase==READY);
    Key(&app,KEY_B); CHECK(app.audit && app.game.phase==PAUSED && app.game.baseField.tail==1);
    Key(&app,KEY_N); CHECK(app.game.baseField.head==1 && app.game.baseField.tail>1);
    Key(&app,KEY_R); CHECK(app.game.baseField.head==0 && app.game.baseField.tail==1);
    Key(&app,KEY_B); CHECK(!app.audit && app.game.phase==READY);
    Key(&app,KEY_ENTER); CHECK(app.game.phase==PLAY);
    Key(&app,KEY_W); CHECK(app.input.dir==UP && !app.input.fire);
    int origin=app.game.player.cell;
    for (int i=0; i<9; i++) GameStep(&app.game,app.input);
    CHECK(app.game.player.cell!=origin);
    Key(&app,KEY_J); CHECK(app.input.fire);
    GameStep(&app.game,app.input); CHECK(app.game.sounds&S_SHOOT);
    AppAfterStep(&app); CHECK(IsSoundPlaying(app.sound[0]));
    Key(&app,KEY_M); CHECK(app.muted); Key(&app,KEY_M); CHECK(!app.muted);
    Key(&app,KEY_ESCAPE); CHECK(app.game.phase==PAUSED);
    int tick=app.game.tick; GameStep(&app.game,app.input); CHECK(app.game.tick==tick);
    Key(&app,KEY_ESCAPE); CHECK(app.game.phase==PLAY);
    Key(&app,KEY_ESCAPE); Key(&app,KEY_DOWN); Key(&app,KEY_ENTER);
    CHECK(app.game.phase==READY && app.game.stage==1 && app.game.live==0 && !app.game.score);
    Key(&app,KEY_ENTER); Key(&app,KEY_ESCAPE); Key(&app,KEY_DOWN); Key(&app,KEY_DOWN); Key(&app,KEY_ENTER);
    CHECK(app.screen==MENU && !app.game.enemies);
    Key(&app,KEY_ENTER); app.game.phase=LOST; Key(&app,KEY_R);
    CHECK(app.game.phase==READY && app.game.stage==1);
    app.game.phase=WON; AppAfterStep(&app); CHECK(!FileExists("records.txt"));
    Key(&app,KEY_ENTER); CHECK(app.screen==MENU);
    Key(&app,KEY_UP); Key(&app,KEY_ENTER); CHECK(app.quit);
    AppFree(&app); CHECK(app.game.allocations==app.game.releases); CloseWindow();
    printf("Input/audio checks: %d/%d passed; keys injected at raylib API, not OS keyboard\n",checks-failures,checks);
    return failures ? 1 : 0;
}
