#ifndef UI_H
#define UI_H
#include "tank.h"
#include "raylib.h"
#define GAME_TITLE "奶蛙坦克大战"

typedef enum Screen { MENU, HELP, SCORES, BATTLE } Screen;
typedef struct App {
    Game game;
    Board board;
    Font font;
    Texture2D sprites;
    Sound sound[5];
    Screen screen;
    Phase auditReturn;
    Input input;
    uint32_t seed;
    int menu, pauseMenu, lastDir, layoutErrors;
    bool hidden, audio, muted, audit, saved, saveAttempted, quit;
} App;

bool AppInit(App *app, bool hidden);
void AppFree(App *app);
void AppNewGame(App *app);
void AppInput(App *app);
void AppAfterStep(App *app);
void AppDraw(App *app);
#endif
