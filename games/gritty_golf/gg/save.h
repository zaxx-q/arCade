/*
 * save.h  -  player progress (binary file) and settings (text file)
 *
 *   [T13 Struct]  SaveData, Settings
 *   [T13 File]    BINARY I/O  fwrite / fread  +  TEXT I/O fgets / fprintf
 *   [T04 Operator] bitwise operators: the "levels cleared" list is ONE
 *                  32-bit integer, bit n = level n
 */
#ifndef GG_SAVE_H
#define GG_SAVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "common.h"

#define SAVE_MAGIC    0x47474C46u        /* the bytes 'F','L','G','G' - identifies our file */
#define SAVE_VERSION  1

/* Everything is 4-byte aligned on purpose: the struct has NO padding bytes,
 * so writing it to disk is predictable.                                      */
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    char     name[PLAYER_NAME_MAX];
    uint32_t cleared_mask;                  /* bit n set = level n cleared  */
    int32_t  best_strokes[LEVEL_COUNT];     /* 0 = never cleared            */
    float    best_time[LEVEL_COUNT];        /* seconds                      */
    uint32_t play_seconds;
    uint32_t checksum;                      /* must stay the LAST field     */
} SaveData;

typedef enum {
    SAVE_OK = 0,
    SAVE_NOT_FOUND,
    SAVE_CORRUPT,                           /* wrong size / magic / checksum */
    SAVE_BAD_VERSION,
    SAVE_IO_ERROR
} SaveResult;

void        Save_Init(SaveData *s, const char *name);
SaveResult  Save_Load(SaveData *s, const char *path);
bool        Save_Write(const SaveData *s, const char *path);
uint32_t    Save_Checksum(const SaveData *s);
const char *Save_ResultText(SaveResult r);

/* One save file PER PLAYER:  "Andi Putra" -> "gritty_save_andi_putra.dat" */
void       Save_PathFor(const char *name, char *out, size_t cap);
/* Load the player's file, or start a new profile.  A damaged file is renamed
 * to *.bad first so nothing is lost.  Returns what happened to the old file.   */
SaveResult Save_OpenProfile(SaveData *s, const char *name, char *path_out, size_t cap);

bool Save_RecordClear(SaveData *s, int level, int strokes, float time); /* true = new best */
bool Save_IsCleared(const SaveData *s, int level);
int  Save_CountCleared(const SaveData *s);
int  Save_FirstUncleared(const SaveData *s);      /* LEVEL_COUNT when all cleared */
int  Save_TotalBestStrokes(const SaveData *s);
float Save_TotalBestTime(const SaveData *s);
int   Save_Score(const SaveData *s);

/* ---- settings (human-readable key=value text file) ---------------- */
typedef struct {
    int  volume;          /* 0..100 */
    bool aim_assist;      /* show predicted path */
    bool fullscreen;
    int  window_scale;    /* 1..6  -> window is 256*scale pixels */
    char last_player[PLAYER_NAME_MAX];   /* pre-filled in the name prompt */
} Settings;

void Settings_Default(Settings *s);
bool Settings_Load(Settings *s, const char *path);   /* false if file missing */
bool Settings_Save(const Settings *s, const char *path);

#endif /* GG_SAVE_H */
