#include "tank.h"
#include <stdio.h>
#include <string.h>
#include "maps_builtin.h"

int Neighbor(int cell, int dir)
{
    if (cell < 0 || cell >= NAV_CELLS || dir < UP || dir > LEFT) return -1;
    int x = cell % NAV_GRID, y = cell / NAV_GRID;
    if (dir == UP) y--; else if (dir == RIGHT) x++;
    else if (dir == DOWN) y++; else x--;
    return x >= 0 && x < NAV_GRID && y >= 0 && y < NAV_GRID ? y * NAV_GRID + x : -1;
}

int MapNode(int cell)
{
    return cell>=0 && cell<CELLS ? cell/GRID*2*NAV_GRID+cell%GRID*2 : -1;
}

char MapAt(const Map *map, int x, int y)
{
    if (!map || x<0 || y<0 || x>=GRID*TILE || y>=GRID*TILE) return 'S';
    int c=y/TILE*GRID+x/TILE, bit=1<<((y%TILE>=STEP)*2+(x%TILE>=STEP));
    char tile=map->tile[c];
    if (tile!='B' && tile!='S') return tile;
    unsigned steel=map->steel[c] ? map->steel[c] : (tile=='S' ? 15u : 0u);
    return steel&bit ? 'S' : map->brick[c]&bit ? 'B' : '.';
}

bool MapClear(const Map *map, int node, int flags)
{
    if (!map || node<0 || node>=NAV_CELLS) return false;
    int x=(node%NAV_GRID+1)*STEP, y=(node/NAV_GRID+1)*STEP;
    /* The 32-pixel hull overlaps at most four actual 20-pixel wall blocks. */
    for (int by=(y-TANK_HALF)/STEP; by<=(y+TANK_HALF-1)/STEP; by++)
        for (int bx=(x-TANK_HALF)/STEP; bx<=(x+TANK_HALF-1)/STEP; bx++) {
            char tile=MapAt(map,bx*STEP,by*STEP);
            if (tile=='S' || tile=='W' || (tile=='B' && !(flags&PLAN_BRICKS)) ||
                (tile=='E' && !(flags&PLAN_BASE))) return false;
        }
    return true;
}

bool TileWalkable(char tile) { return tile == '.' || tile == 'T'; }

bool MapValid(const Map *map)
{
    if (!map || map->tile[BASE_CELL] != 'E') return false;
    int bases = 0;
    for (int i=0; i<CELLS; i++) {
        if (!strchr(".BSTWE", map->tile[i]) || !map->tile[i]) return false;
        if (map->tile[i] == 'E') bases++;
    }
    if (bases != 1 || !MapClear(map,MapNode(PLAYER_CELL),0)) return false;
    Field field; FieldBuild(&field, map, MapNode(PLAYER_CELL));
    const int spawns[] = {0, 6, 12};
    for (int i=0; i<3; i++)
        if (!MapClear(map,MapNode(spawns[i]),0) || field.dist[MapNode(spawns[i])] < 0) return false;
    return true;
}

void MapBuiltin(Map *map, int stage)
{
    if (!map) return;
    if (stage < 1 || stage > STAGES) stage = 1;
    memset(map, 0, sizeof(*map));
    map->revision = 1;
    memcpy(map->tile,BUILTIN_MAPS[stage-1],CELLS);
    memcpy(map->brick,BUILTIN_BRICKS[stage-1],CELLS);
    memcpy(map->steel,BUILTIN_STEEL[stage-1],CELLS);
}

int MapEnemyType(int stage, int index)
{
    return stage>=1 && stage<=STAGES && index>=0 && index<ENEMY_TOTAL ? BUILTIN_ENEMIES[stage-1][index] : 0;
}

/* Parse into a temporary map, so a malformed file never leaves a partial map. */
bool MapLoad(Map *map, const char *path)
{
    if (!map || !path) return false;
    FILE *file=fopen(path,"r"); if (!file) return false;
    Map candidate;
    char line[128], fine[26][26];
    memset(&candidate, 0, sizeof(candidate));
    candidate.revision = 1;
    int row=0,width=0; bool valid=true;
    memset(candidate.tile,'.',CELLS);
    while (fgets(line,sizeof(line),file)) {
        size_t n=strlen(line);
        if (n && line[n-1]!='\n' && !feof(file)) { valid=false; break; }
        n=strcspn(line,"\r\n"); line[n]=0;
        if (!n || line[0]=='#' || line[0]==';') continue;
        if (!width) width=(int)n;
        if ((width!=GRID && width!=26) || n!=(size_t)width || row==width) { valid=false; break; }
        for (int x=0; x<width; x++) if (!strchr(".BSTWE",line[x])) valid=false;
        memcpy(fine[row],line,(size_t)width);
        row++;
    }
    if (ferror(file)) valid=false;
    fclose(file);
    if (!valid || row!=width || !width) return false;
    for (int y=0; y<width; y++) for (int x=0; x<width; x++) {
        int c=width==GRID ? y*GRID+x : (y/2)*GRID+x/2;
        char ch=fine[y][x];
        if (ch!='.') {
            const char *order=".BT WSE";
            if (strchr(order,ch)>strchr(order,candidate.tile[c])) candidate.tile[c]=ch;
        }
        if (ch=='B') candidate.brick[c]|=(unsigned char)(width==GRID ? 15 : (1u<<((y%2)*2+x%2)));
        if (ch=='S') candidate.steel[c]|=(unsigned char)(width==GRID ? 15 : (1u<<((y%2)*2+x%2)));
    }
    if (!MapValid(&candidate)) return false;
    *map=candidate; return true;
}
