/*
 * records.h  -  the leaderboard (one line per player in a CSV text file)
 *
 *   [T13 Struct]  ScoreRecord
 *   [T13 File]    fopen / fgets / sscanf / fprintf / fclose
 *   [T10 Pointer] "double pointer" output parameter + malloc/realloc
 */
#ifndef GG_RECORDS_H
#define GG_RECORDS_H

#include <stdbool.h>
#include "common.h"
#include "save.h"

typedef struct {
    char  name[PLAYER_NAME_MAX];
    int   levels;        /* levels cleared (0..25)                  */
    int   strokes;       /* sum of best strokes of cleared levels   */
    float time;          /* sum of best times, seconds              */
    char  date[12];      /* "YYYY-MM-DD"                            */
} ScoreRecord;

long File_Size(const char *path);   /* size in bytes, or -1 if missing */

/* Load the CSV.  *out becomes a malloc'ed array the CALLER must free().
 * A missing file is not an error: result is an empty list.            */
bool Records_Load(const char *path, ScoreRecord **out, int *count);
bool Records_Save(const char *path, const ScoreRecord *recs, int count);

/* Insert a new player or improve an existing one.  Returns true if changed. */
bool Records_Upsert(ScoreRecord **recs, int *count, const ScoreRecord *rec);

/* Build this player's record from their save file and upsert it into the
 * leaderboard file.  Returns true if the leaderboard changed on disk.        */
bool Records_SubmitProfile(const char *path, const SaveData *profile);

#endif /* GG_RECORDS_H */
