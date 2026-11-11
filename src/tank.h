#ifndef TANK_H
#define TANK_H

#include <stdbool.h>
#include <stdint.h>

#define GRID 13
#define CELLS (GRID * GRID)
#define TILE 40
#define NAV_GRID (GRID * 2 - 1)
#define NAV_CELLS (NAV_GRID * NAV_GRID)
#define STEP (TILE / 2)
#define TANK_HALF 16
#define BASE_CELL (12 * GRID + 6)
#define PLAYER_CELL (12 * GRID + 4)
#define STAGES 3
#define ENEMY_TOTAL 8
#define ALIVE_LIMIT 4
#define BULLETS 24
#define ITEMS 4
#define RECORDS 10
#define WIDTH 800
#define HEIGHT 640
#define FIELD_X 24
#define FIELD_Y 64

typedef enum Direction { UP, RIGHT, DOWN, LEFT } Direction;
typedef enum Phase { READY, PLAY, PAUSED, CLEAR, LOST, WON } Phase;
typedef enum ItemType { STAR, GRENADE, HELMET, SHOVEL, CLOCK, EXTRA_LIFE, ITEM_TYPES } ItemType;
enum { S_SHOOT=1, S_HIT=2, S_KILL=4, S_ITEM=8, S_END=16 };
enum { PLAN_BRICKS=1, PLAN_BASE=2 };

typedef struct Map {
    char tile[CELLS];
    unsigned char brick[CELLS]; /* Four quarters of each destructible tile. */
    unsigned char steel[CELLS];
    unsigned revision;
} Map;
typedef struct Field {
    int dist[NAV_CELLS], next[NAV_CELLS], queue[NAV_CELLS], head, tail;
} Field;
typedef struct Tank {
    int cell, dest, step, frames, dir;
} Tank;
typedef struct Enemy {
    Tank tank;
    int id, type, hp, reload, appear;
    bool bonus;
    struct Enemy *next;
} Enemy;
typedef struct Bullet {
    float x, y;
    int dir, owner, power;
    float speed;
    bool active;
} Bullet;
typedef struct Item { int cell, type, life; } Item;
typedef struct Record { int score, stage, kills; } Record;
typedef struct Board { Record entries[RECORDS]; int count, skipped; } Board;
typedef struct Input { int dir; bool fire; } Input;
typedef struct Game {
    Map map;
    Field baseField, playerField;
    Tank player;
    Enemy *enemies;
    Bullet bullets[BULLETS];
    Item items[ITEMS];
    Phase phase;
    uint32_t random;
    int stage, lives, score, kills[4], live, spawned, serial, spawnClock;
    int shield, reload, respawn, stars, freeze, shovel, clearClock, tick;
    int allocations, releases;
    char fortTile[5];
    unsigned char fortBrick[5];
    unsigned char fortSteel[5];
    unsigned fieldRevision, sounds;
    bool baseAlive, playerAlive, memoryError, mapFallback;
} Game;

int Neighbor(int cell, int dir);
int MapNode(int cell);
char MapAt(const Map *map, int x, int y);
bool MapClear(const Map *map, int node, int flags);
bool TileWalkable(char tile);
bool MapLoad(Map *map, const char *path);
void MapBuiltin(Map *map, int stage);
int MapEnemyType(int stage, int index);
bool MapValid(const Map *map);
void FieldBegin(Field *field, int goal);
void FieldLayer(Field *field, const Map *map);
void FieldBuild(Field *field, const Map *map, int goal);
int FieldPath(const Field *field, int start, int *out, int capacity);
void BoardInsert(Board *board, Record entry);
bool BoardLoad(Board *board, const char *path);
bool BoardSave(Board *board, Record entry, const char *path);
void GameInit(Game *game, uint32_t seed);
void GameFree(Game *game);
void GameStart(Game *game, uint32_t seed);
void GameLoadStage(Game *game, int stage);
void GameNext(Game *game);
void GameStep(Game *game, Input input);
void GameFields(Game *game);
Enemy *GameSpawn(Game *game, int cell, int type);
void GameItem(Game *game, int type);
void TankPosition(const Tank *tank, float *x, float *y);
int GameKills(const Game *game);

#endif
