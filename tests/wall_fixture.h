#ifndef WALL_FIXTURE_H
#define WALL_FIXTURE_H

/* A 40-pixel passage whose center lies between the old large-cell centers. */
static void OpenRect(Map *map, int x, int y, int width, int height)
{
    for (int by=y/STEP; by<(y+height)/STEP; by++)
        for (int bx=x/STEP; bx<(x+width)/STEP; bx++) {
            int c=by/2*GRID+bx/2, bit=1<<((by%2)*2+bx%2);
            map->steel[c]&=(unsigned char)~bit;
            map->tile[c]=map->steel[c] ? 'S' : '.';
        }
}

static void CorridorMap(Map *map)
{
    memset(map, 0, sizeof(*map));
    map->revision = 1;
    memset(map->tile,'S',CELLS); memset(map->steel,15,CELLS);
    OpenRect(map,140,40,40,400);
    OpenRect(map,140,240,300,40);
    map->tile[BASE_CELL]='E'; map->steel[BASE_CELL]=0;
}

#endif
