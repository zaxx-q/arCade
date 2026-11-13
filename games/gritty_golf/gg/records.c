/*
 * records.c  -  leaderboard stored as a CSV TEXT file
 *
 *   name,levels,strokes,time,date
 *   ANDI,25,212,1334.5,2026-11-08
 *
 * [T13 File Handling]  text files:  fopen -> fgets/fprintf -> fclose
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "records.h"
#include "algo.h"
#include "util.h"

/*
 * File_Size : jump to the end of the file and ask "where am I?".
 *   fseek(fp, 0, SEEK_END) moves the file cursor; ftell() reports it in bytes.
 */
long File_Size(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return -1;
    long size = -1;
    if (fseek(fp, 0, SEEK_END) == 0)
        size = ftell(fp);
    fclose(fp);
    return size;
}

/*
 * Records_Load
 *   PSEUDOCODE
 *     open file for reading
 *     IF it does not exist THEN result is an empty list (not an error)
 *     capacity <- 8 ; allocate array
 *     WHILE a line can be read
 *         skip blank lines and the header
 *         parse "name,levels,strokes,time,date"
 *         IF parsing worked THEN (grow the array if full) store it
 *     close file
 *
 *  [T10 Pointer]  `ScoreRecord **out` is a pointer to a pointer: the function
 *  writes the address of the new array INTO the caller's variable.
 */
bool Records_Load(const char *path, ScoreRecord **out, int *count)
{
    *out = NULL;
    *count = 0;

    FILE *fp = fopen(path, "r");                    /* "r" = read text */
    if (fp == NULL)
        return errno == ENOENT;                     /* missing file: fine, empty list */

    int capacity = 8;
    ScoreRecord *arr = malloc((size_t)capacity * sizeof *arr);
    if (arr == NULL) {
        fclose(fp);
        return false;
    }

    char line[160];
    int n = 0;
    while (fgets(line, sizeof line, fp) != NULL) {  /* [T06] read line by line */
        Str_Trim(line);
        if (line[0] == '\0' || strncmp(line, "name,", 5) == 0)
            continue;                               /* blank line or header */

        ScoreRecord r;
        memset(&r, 0, sizeof r);
        /* %23[^,] = read up to 23 chars that are NOT a comma  (name[24]) */
        int got = sscanf(line, "%23[^,],%d,%d,%f,%11s", r.name, &r.levels, &r.strokes, &r.time, r.date);
        if (got != 5)
            continue;                               /* damaged line: skip it */

        if (n == capacity) {                        /* array full -> double it */
            capacity *= 2;
            ScoreRecord *bigger = realloc(arr, (size_t)capacity * sizeof *arr);
            if (bigger == NULL) { free(arr); fclose(fp); return false; }
            arr = bigger;
        }
        arr[n++] = r;
    }
    fclose(fp);

    *out = arr;
    *count = n;
    return true;
}

/*
 * Records_Save : write to a temporary file first, then replace the real one.
 * If the program crashes half-way the old leaderboard is still intact.
 */
bool Records_Save(const char *path, const ScoreRecord *recs, int count)
{
    char tmp_path[256];
    snprintf(tmp_path, sizeof tmp_path, "%s.tmp", path);

    FILE *fp = fopen(tmp_path, "w");                /* "w" = create / overwrite */
    if (fp == NULL)
        return false;

    fprintf(fp, "name,levels,strokes,time,date\n");
    for (int i = 0; i < count; i++)
        fprintf(fp, "%s,%d,%d,%.1f,%s\n",
                recs[i].name, recs[i].levels, recs[i].strokes, recs[i].time, recs[i].date);

    bool ok = (fflush(fp) == 0) && !ferror(fp);
    ok = (fclose(fp) == 0) && ok;
    if (!ok) { remove(tmp_path); return false; }

    remove(path);                                   /* Windows cannot rename over a file */
    return rename(tmp_path, path) == 0;
}

/*
 * Records_Upsert = UPdate or inSERT.
 *   1. LINEAR SEARCH the list for this player's name
 *   2. found    -> replace only if the new result ranks better
 *      not found -> append (realloc grows the array by one)
 */
bool Records_Upsert(ScoreRecord **recs, int *count, const ScoreRecord *rec)
{
    int pos = Search_LinearName(*recs, *count, rec->name);
    if (pos >= 0) {
        if (Cmp_ByRank(rec, &(*recs)[pos]) < 0) {   /* new one ranks higher */
            (*recs)[pos] = *rec;
            return true;
        }
        return false;
    }
    ScoreRecord *bigger = realloc(*recs, (size_t)(*count + 1) * sizeof **recs);
    if (bigger == NULL)
        return false;
    *recs = bigger;
    (*recs)[*count] = *rec;
    (*count)++;
    return true;
}

/*
 * Records_SubmitProfile
 *   PSEUDOCODE
 *     IF the player has not cleared a single level THEN nothing to submit
 *     rec <- (name, levels cleared, sum of best strokes, sum of best times, today)
 *     list <- load leaderboard file
 *     IF Upsert(list, rec) changed something THEN save list
 *     free list
 */
bool Records_SubmitProfile(const char *path, const SaveData *profile)
{
    int cleared = Save_CountCleared(profile);
    if (cleared == 0)
        return false;

    ScoreRecord rec;
    memset(&rec, 0, sizeof rec);
    Str_Copy(rec.name, sizeof rec.name, profile->name);
    rec.levels  = cleared;
    rec.strokes = Save_TotalBestStrokes(profile);
    rec.time    = Save_TotalBestTime(profile);
    Str_Today(rec.date, sizeof rec.date);

    ScoreRecord *list = NULL;
    int n = 0;
    if (!Records_Load(path, &list, &n))
        return false;

    bool changed = Records_Upsert(&list, &n, &rec);
    bool ok = !changed || Records_Save(path, list, n);
    free(list);                                      /* every malloc has its free */
    return changed && ok;
}
