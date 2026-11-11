#include "tank.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void FieldBegin(Field *field, int goal)
{
    if (!field) return;
    field->head=field->tail=0;
    for (int i=0; i<NAV_CELLS; i++) field->dist[i]=field->next[i]=-1;
    if (goal<0 || goal>=NAV_CELLS) return;
    field->dist[goal]=0; field->queue[field->tail++]=goal;
}

/* Expand one distance layer. The game and the paused display use this same queue. */
void FieldLayer(Field *field, const Map *map)
{
    if (!field || !map) return;
    int end=field->tail;
    while (field->head<end) {
        int cell=field->queue[field->head++];
        for (int d=UP; d<=LEFT; d++) {
            int n=Neighbor(cell,d);
            int flags=PLAN_BRICKS | (field->queue[0]==MapNode(BASE_CELL) ? PLAN_BASE : 0);
            if (n<0 || field->dist[n]>=0 || !MapClear(map,n,flags)) continue;
            if (field->tail>=NAV_CELLS) return;
            field->dist[n]=field->dist[cell]+1; field->next[n]=cell;
            field->queue[field->tail++]=n;
        }
    }
}

void FieldBuild(Field *field, const Map *map, int goal)
{
    if (!field) return;
    FieldBegin(field,goal);
    if (map) while (field->head<field->tail) FieldLayer(field,map);
}

int FieldPath(const Field *field, int start, int *out, int capacity)
{
    if (!field || !out || capacity<=0 || start<0 || start>=NAV_CELLS || field->dist[start]<0) return 0;
    int count=0;
    while (start>=0 && start<NAV_CELLS && count<capacity && count<NAV_CELLS) {
        out[count++]=start;
        int next=field->next[start];
        if (next<0 || next>=NAV_CELLS || field->dist[next]!=field->dist[start]-1) break;
        start=next;
    }
    return count;
}

static bool ValidRecord(Record r)
{
    return r.score>=0 && r.score<=1000000 && r.stage>=1 && r.stage<=STAGES && r.kills>=0 && r.kills<=STAGES*ENEMY_TOTAL;
}

/* Only ten records: stable insertion is short and preserves earlier equal scores. */
void BoardInsert(Board *board, Record entry)
{
    if (!board || !ValidRecord(entry) || board->count<0 || board->count>RECORDS) return;
    int pos=0;
    while (pos<board->count && board->entries[pos].score>=entry.score) pos++;
    if (pos>=RECORDS) return;
    if (board->count<RECORDS) board->count++;
    for (int i=board->count-1; i>pos; i--) board->entries[i]=board->entries[i-1];
    board->entries[pos]=entry;
}

static bool ParseRecord(char *text, Record *record)
{
    int values[3]; char *p=text;
    for (int i=0; i<3; i++) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) return false;
        errno=0; char *end; long n=strtol(p,&end,10);
        if (p==end || errno==ERANGE || n<0 || n>INT_MAX) return false;
        if (*end && !isspace((unsigned char)*end)) return false;
        values[i]=(int)n; p=end;
    }
    while (isspace((unsigned char)*p)) p++;
    *record=(Record){values[0],values[1],values[2]};
    return !*p && ValidRecord(*record);
}

bool BoardLoad(Board *board, const char *path)
{
    if (!board || !path) return false;
    *board=(Board){0}; FILE *file=fopen(path,"rb");
    if (!file) return false;
    char line[128];
    for (;;) {
        size_t used=0; int c; bool invalid=false, readAny=false;
        /* Consume one physical line, including NULs and overlong suffixes. */
        while ((c=fgetc(file))!=EOF && c!='\n') {
            readAny=true;
            if (c==0 || used>=sizeof(line)-1) invalid=true;
            else line[used++]=(char)c;
        }
        if (c==EOF && !readAny) break;
        line[used]=0;
        if (invalid) { board->skipped++; continue; }
        char *p=line; while (isspace((unsigned char)*p)) p++;
        if (!*p || *p=='#') continue;
        Record entry;
        if (ParseRecord(p,&entry)) BoardInsert(board,entry); else board->skipped++;
    }
    bool ok=!ferror(file); fclose(file); return ok;
}

/* Append one completed game. A failed write cannot erase earlier records. */
bool BoardSave(Board *board, Record entry, const char *path)
{
    if (!board || !path || !ValidRecord(entry)) return false;
    FILE *file=fopen(path,"a+"); if (!file) return false;
    bool ok=fseek(file,0,SEEK_END)==0;
    long length=ok ? ftell(file) : -1;
    if (length<0) ok=false;
    if (ok && length>0) {
        ok=fseek(file,-1,SEEK_END)==0;
        int last=ok ? fgetc(file) : EOF;
        ok=last!=EOF && fseek(file,0,SEEK_END)==0;
        if (ok && last!='\n') ok=fputc('\n',file)!=EOF;
    }
    if (ok) ok=fprintf(file,"%d %d %d\n",entry.score,entry.stage,entry.kills)>0;
    if (fclose(file)!=0) ok=false;
    if (ok) BoardInsert(board,entry);
    return ok;
}
