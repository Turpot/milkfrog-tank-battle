#include "ui.h"
#include <math.h>
#include <string.h>
#include <time.h>

int main(int argc, char **argv)
{
    bool hidden=false, interactive=false;
    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i],"--test-hidden")) hidden=true;
        if (!strcmp(argv[i],"--test-interactive")) interactive=true;
    }
    interactive=hidden && interactive;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_VSYNC_HINT|(hidden && !interactive ? FLAG_WINDOW_HIDDEN : 0));
    InitWindow(WIDTH,HEIGHT,GAME_TITLE); SetWindowMinSize(640,512); SetExitKey(KEY_NULL); SetTargetFPS(60);
    ChangeDirectory(GetApplicationDirectory());
    App app; bool loaded=AppInit(&app,hidden && !interactive); app.seed=(uint32_t)time(NULL);
    app.hidden=hidden;
    if (!loaded) {
        while (!WindowShouldClose()) {
            BeginDrawing(); ClearBackground(BLACK);
            DrawText("Assets missing. Extract the entire game folder.",30,90,20,WHITE); EndDrawing();
            if (hidden || IsKeyPressed(KEY_ESCAPE)) break;
        }
        AppFree(&app); CloseWindow(); return 1;
    }
    RenderTexture2D canvas=LoadRenderTexture(WIDTH,HEIGHT);
    SetTextureFilter(canvas.texture,TEXTURE_FILTER_BILINEAR);
    double accumulated=0;
    while (!WindowShouldClose() && !app.quit) {
        app.input.dir = -1;
        app.input.fire = false;
        AppInput(&app);
        if (app.game.phase==PLAY && app.screen==BATTLE) {
            accumulated+=fmin(GetFrameTime(),.1);
            while (accumulated>=1.0/60) { GameStep(&app.game,app.input); accumulated-=1.0/60; }
        } else accumulated=0;
        AppAfterStep(&app);
        BeginTextureMode(canvas); AppDraw(&app); EndTextureMode();
        float scale=fminf((float)GetScreenWidth()/WIDTH,(float)GetScreenHeight()/HEIGHT);
        BeginDrawing(); ClearBackground(BLACK);
        DrawTexturePro(canvas.texture,(Rectangle){0,0,WIDTH,-HEIGHT},
            (Rectangle){(GetScreenWidth()-WIDTH*scale)/2,(GetScreenHeight()-HEIGHT*scale)/2,WIDTH*scale,HEIGHT*scale},
            (Vector2){0},0,WHITE);
        EndDrawing();
        if (hidden && !interactive) break;
    }
    AppFree(&app); UnloadRenderTexture(canvas); CloseWindow(); return 0;
}
