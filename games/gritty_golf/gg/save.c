/*
 * save.c  -  binary save file + text settings file
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "save.h"
#include "records.h"      /* File_Size */
#include "util.h"

/* Compile-time check: if someone adds a field and the struct gets padding or
 * a different size, the build FAILS instead of silently breaking old saves. */
typedef char SaveData_size_must_be_244[(sizeof(SaveData) == 244) ? 1 : -1];

void Save_Init(SaveData *s, const char *name)
{
    memset(s, 0, sizeof *s);                 /* all numbers 0, mask 0, name empty */
    s->magic   = SAVE_MAGIC;
    s->version = SAVE_VERSION;
    Str_Copy(s->name, sizeof s->name, name);
}

/*
 * Save_Checksum : FNV-1a hash over every byte BEFORE the checksum field.
 * A hash is a small number computed from big data.  If even one bit of the
 * file changes (disk error, hand editing) the hash changes -> we notice.
 *
 *   [T04 Operator]  ^ (XOR) mixes a byte in,  * multiplies to spread the bits
 *   [T10 Pointer]   we look at the struct as an array of raw bytes
 */
uint32_t Save_Checksum(const SaveData *s)
{
    const uint8_t *bytes = (const uint8_t *)s;          /* pointer cast: struct -> bytes */
    size_t len = offsetof(SaveData, checksum);          /* all bytes before `checksum`   */
    uint32_t h = 2166136261u;                           /* FNV offset basis              */
    for (size_t i = 0; i < len; i++) {
        h ^= bytes[i];                                  /* XOR in the next byte */
        h *= 16777619u;                                 /* FNV prime            */
    }
    return h;
}

/*
 * Save_Load
 *   PSEUDOCODE
 *     size <- File_Size(path)
 *     IF file missing              THEN return NOT_FOUND
 *     IF size != sizeof(SaveData)  THEN return CORRUPT
 *     read the struct with ONE fread
 *     IF magic wrong or checksum wrong THEN return CORRUPT
 *     IF version unknown            THEN return BAD_VERSION
 *     sanitise values, copy into *s, return OK
 */
SaveResult Save_Load(SaveData *s, const char *path)
{
    long size = File_Size(path);
    if (size < 0)
        return SAVE_NOT_FOUND;
    if (size != (long)sizeof(SaveData))
        return SAVE_CORRUPT;

    FILE *fp = fopen(path, "rb");                       /* "rb" = read BINARY */
    if (fp == NULL)
        return SAVE_IO_ERROR;

    SaveData tmp;
    size_t got = fread(&tmp, sizeof tmp, 1, fp);        /* 1 block of sizeof(SaveData) bytes */
    fclose(fp);
    if (got != 1)
        return SAVE_IO_ERROR;

    if (tmp.magic != SAVE_MAGIC || tmp.checksum != Save_Checksum(&tmp))
        return SAVE_CORRUPT;
    if (tmp.version != SAVE_VERSION)
        return SAVE_BAD_VERSION;

    /* never trust data from disk: force a terminator and clamp ranges */
    tmp.name[PLAYER_NAME_MAX - 1] = '\0';
    tmp.cleared_mask &= (1u << LEVEL_COUNT) - 1u;       /* keep only bits 0..24 */
    for (int i = 0; i < LEVEL_COUNT; i++) {
        if (tmp.best_strokes[i] < 0) tmp.best_strokes[i] = 0;
        if (!(tmp.best_time[i] >= 0.0f)) tmp.best_time[i] = 0.0f;   /* also catches NaN */
    }
    *s = tmp;
    return SAVE_OK;
}

/* Write through a temp file so a crash never leaves a half-written save. */
bool Save_Write(const SaveData *s, const char *path)
{
    SaveData copy = *s;
    copy.checksum = Save_Checksum(&copy);

    char tmp_path[256];
    snprintf(tmp_path, sizeof tmp_path, "%s.tmp", path);

    FILE *fp = fopen(tmp_path, "wb");                   /* "wb" = write BINARY */
    if (fp == NULL)
        return false;
    bool ok = fwrite(&copy, sizeof copy, 1, fp) == 1;
    ok = (fclose(fp) == 0) && ok;
    if (!ok) { remove(tmp_path); return false; }

    remove(path);
    return rename(tmp_path, path) == 0;
}

const char *Save_ResultText(SaveResult r)
{
    switch (r) {                                        /* [T05] switch on an enum */
        case SAVE_OK:          return "ok";
        case SAVE_NOT_FOUND:   return "no save file yet";
        case SAVE_CORRUPT:     return "save file is damaged";
        case SAVE_BAD_VERSION: return "save file is from another version";
        case SAVE_IO_ERROR:    return "could not read the save file";
    }
    return "unknown";
}

/*
 * Build a safe file name from a player name: lowercase letters and digits are
 * kept, everything else becomes '_'.   "Andi Putra!" -> "gritty_save_andi_putra_.dat"
 */
void Save_PathFor(const char *name, char *out, size_t cap)
{
    char safe[PLAYER_NAME_MAX];
    size_t i = 0;
    /* cppcheck-suppress arrayIndexThenCheck ; name is a C string: it ends before name[i] can pass the bound */
    for (; name[i] != '\0' && i < sizeof safe - 1; i++) {
        char c = name[i];
        if (c >= 'A' && c <= 'Z')
            c = (char)(c + ('a' - 'A'));                 /* to lowercase */
        bool keep = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        safe[i] = keep ? c : '_';
    }
    safe[i] = '\0';
    snprintf(out, cap, "gritty_save_%s.dat", safe);
}

SaveResult Save_OpenProfile(SaveData *s, const char *name, char *path_out, size_t cap)
{
    Save_PathFor(name, path_out, cap);
    SaveResult r = Save_Load(s, path_out);

    if (r == SAVE_CORRUPT || r == SAVE_BAD_VERSION) {
        char bad[300];
        snprintf(bad, sizeof bad, "%s.bad", path_out);
        remove(bad);
        rename(path_out, bad);                           /* keep the damaged file */
    }
    if (r != SAVE_OK)
        Save_Init(s, name);                              /* fresh profile */
    else
        Str_Copy(s->name, sizeof s->name, name);         /* name may differ in case */
    return r;
}

/* ================================================================== *
 *  Progress helpers  -  BITWISE operators on cleared_mask
 *      mask |=  (1u << n)     set    bit n   (level n cleared)
 *      mask &   (1u << n)     test   bit n
 *      mask &= ~(1u << n)     clear  bit n
 * ================================================================== */
bool Save_IsCleared(const SaveData *s, int level)
{
    if (level < 0 || level >= LEVEL_COUNT)
        return false;
    return (s->cleared_mask & (1u << level)) != 0;
}

/*
 * Save_RecordClear : remember a finished level and keep only the BEST result.
 * Returns true when this is a new personal best (fewer strokes, or equal
 * strokes in a shorter time).
 */
bool Save_RecordClear(SaveData *s, int level, int strokes, float time)
{
    if (level < 0 || level >= LEVEL_COUNT)
        return false;
    if (strokes < 1)
        strokes = 1;               /* rolled into the flag without a shot: count as 1 */

    bool first_time = !Save_IsCleared(s, level);
    bool better = first_time
               || strokes < s->best_strokes[level]
               || (strokes == s->best_strokes[level] && time < s->best_time[level]);

    s->cleared_mask |= (1u << level);                   /* set the bit */
    if (better) {
        s->best_strokes[level] = strokes;
        s->best_time[level]    = time;
    }
    return better;
}

/*
 * Count the 1-bits in the mask (Brian Kernighan's trick).
 *   x & (x-1)  clears the LOWEST set bit, so the loop runs once per set bit.
 *   x = 0b10110 -> 0b10100 -> 0b10000 -> 0     (3 iterations = 3 levels)
 */
int Save_CountCleared(const SaveData *s)
{
    uint32_t x = s->cleared_mask;
    int count = 0;
    while (x != 0) {
        x &= x - 1;
        count++;
    }
    return count;
}

/* LINEAR SEARCH for the first level whose bit is 0. */
int Save_FirstUncleared(const SaveData *s)
{
    for (int i = 0; i < LEVEL_COUNT; i++)
        if (!Save_IsCleared(s, i))
            return i;
    return LEVEL_COUNT;                                 /* everything is cleared */
}

int Save_TotalBestStrokes(const SaveData *s)
{
    int total = 0;
    for (int i = 0; i < LEVEL_COUNT; i++)
        if (Save_IsCleared(s, i))
            total += s->best_strokes[i];
    return total;
}

float Save_TotalBestTime(const SaveData *s)
{
    float total = 0.0f;
    for (int i = 0; i < LEVEL_COUNT; i++)
        if (Save_IsCleared(s, i))
            total += s->best_time[i];
    return total;
}

/*
 * Score = 1000 points per cleared level, minus 40 per stroke and 2 per second.
 * Never below zero.  (A small formula that mixes int, float and a cast.)
 */
int Save_Score(const SaveData *s)
{
    float score = 1000.0f * (float)Save_CountCleared(s)
                - 40.0f   * (float)Save_TotalBestStrokes(s)
                -  2.0f   * Save_TotalBestTime(s);
    return score > 0.0f ? (int)score : 0;
}

/* ================================================================== *
 *  Settings  -  a plain TEXT file you can edit in Notepad
 *      # comment
 *      volume=70
 * ================================================================== */
void Settings_Default(Settings *s)
{
    s->volume       = 70;
    s->aim_assist   = false;
    s->fullscreen   = false;
    s->window_scale = 3;
    Str_Copy(s->last_player, sizeof s->last_player, "PLAYER");
}

static int clamp_int(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);             /* nested ternary */
}

bool Settings_Load(Settings *s, const char *path)
{
    Settings_Default(s);                                /* start from defaults */
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return false;

    char line[128];
    while (fgets(line, sizeof line, fp) != NULL) {
        Str_Trim(line);
        if (line[0] == '#' || line[0] == '\0')
            continue;                                   /* comment / blank */

        char key[32], val[32];
        /* " %31[^=]=%31[^\n]"  ->  key = text before '=',  val = rest of the line */
        if (sscanf(line, " %31[^=]=%31[^\n]", key, val) != 2)
            continue;
        Str_Trim(key);
        int v = atoi(val);

        if      (strcmp(key, "volume")       == 0) s->volume       = clamp_int(v, 0, 100);
        else if (strcmp(key, "aim_assist")   == 0) s->aim_assist   = (v != 0);
        else if (strcmp(key, "fullscreen")   == 0) s->fullscreen   = (v != 0);
        else if (strcmp(key, "window_scale") == 0) s->window_scale = clamp_int(v, 1, 6);
        else if (strcmp(key, "last_player")  == 0) {
            Str_Trim(val);
            if (Str_IsValidName(val)) Str_Copy(s->last_player, sizeof s->last_player, val);
        }
        /* unknown keys are ignored: old/new versions stay compatible */
    }
    fclose(fp);
    return true;
}

bool Settings_Save(const Settings *s, const char *path)
{
    FILE *fp = fopen(path, "w");
    if (fp == NULL)
        return false;
    fprintf(fp, "# Gritty Golf settings\n");
    fprintf(fp, "volume=%d\n",       s->volume);
    fprintf(fp, "aim_assist=%d\n",   s->aim_assist ? 1 : 0);
    fprintf(fp, "fullscreen=%d\n",   s->fullscreen ? 1 : 0);
    fprintf(fp, "window_scale=%d\n", s->window_scale);
    fprintf(fp, "last_player=%s\n", s->last_player);
    return fclose(fp) == 0;
}
