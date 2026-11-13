/* test_files.c - binary save file, settings text file, leaderboard CSV */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_framework.h"
#include "save.h"
#include "records.h"
#include "algo.h"
#include "util.h"

#define SAVE_TMP "save"

static void write_raw(const char *path, const void *data, size_t n)
{
    FILE *fp = fopen(path, "wb");
    if (fp) { fwrite(data, 1, n, fp); fclose(fp); }
}

static void write_text(const char *path, const char *text)
{
    FILE *fp = fopen(path, "w");
    if (fp) { fputs(text, fp); fclose(fp); }
}

/* a save with some progress in it */
static SaveData sample_save(void)
{
    SaveData s;
    Save_Init(&s, "Andi");
    Save_RecordClear(&s, 0, 2, 12.5f);
    Save_RecordClear(&s, 1, 4, 30.0f);
    Save_RecordClear(&s, 24, 9, 99.9f);
    s.play_seconds = 321;
    return s;
}

/* ---- SaveData ---------------------------------------------------------------- */
static void test_save_init(void)
{
    SaveData s;
    memset(&s, 0xAA, sizeof s);                       /* garbage first: Init must clean everything */
    Save_Init(&s, "Andi");
    CHECK_INT(s.magic, SAVE_MAGIC);
    CHECK_INT(s.version, SAVE_VERSION);
    CHECK_STR(s.name, "Andi");
    CHECK_INT(s.cleared_mask, 0);
    CHECK_INT(s.play_seconds, 0);
    for (int i = 0; i < LEVEL_COUNT; i++) {
        CHECK_INT(s.best_strokes[i], 0);
        CHECK_NEAR(s.best_time[i], 0.0, 0);
    }
    CHECK_INT(sizeof(SaveData), 244);                 /* no padding: the layout is the file format */

    Save_Init(&s, "A very long name that does not fit in the buffer at all");
    CHECK_INT(strlen(s.name), PLAYER_NAME_MAX - 1);   /* truncated, still terminated */
}

static void test_save_round_trip(void)
{
    TmpRemove(SAVE_TMP);
    SaveData a = sample_save(), b;
    CHECK(Save_Write(&a, TmpPath(SAVE_TMP)));
    CHECK_INT(File_Size(TmpPath(SAVE_TMP)), 244);
    CHECK_INT(Save_Load(&b, TmpPath(SAVE_TMP)), SAVE_OK);

    CHECK_STR(b.name, "Andi");
    CHECK_INT(b.cleared_mask, a.cleared_mask);
    CHECK_INT(b.play_seconds, 321);
    for (int i = 0; i < LEVEL_COUNT; i++) {
        CHECK_INT(b.best_strokes[i], a.best_strokes[i]);
        CHECK_NEAR(b.best_time[i], a.best_time[i], 0);
    }
    CHECK_INT(b.checksum, Save_Checksum(&b));

    char tmp2[300];                                   /* the temporary file is gone after the rename */
    snprintf(tmp2, sizeof tmp2, "%s.tmp", TmpPath(SAVE_TMP));
    CHECK_INT(File_Size(tmp2), -1);

    a.play_seconds = 400;                             /* overwrite works */
    CHECK(Save_Write(&a, TmpPath(SAVE_TMP)));
    Save_Load(&b, TmpPath(SAVE_TMP));
    CHECK_INT(b.play_seconds, 400);
    TmpRemove(SAVE_TMP);
}

static void test_save_not_found_and_bad_path(void)
{
    SaveData s;
    TmpRemove(SAVE_TMP);
    CHECK_INT(Save_Load(&s, TmpPath(SAVE_TMP)), SAVE_NOT_FOUND);
    CHECK_INT(File_Size(TmpPath(SAVE_TMP)), -1);
    SaveData a = sample_save();
    CHECK(!Save_Write(&a, "no_such_directory_xyz/save.dat"));   /* cannot create the file */
}

static void test_save_detects_corruption(void)
{
    SaveData a = sample_save(), b;
    const char *path = TmpPath(SAVE_TMP);

    /* 1. flip ONE byte in the middle of the data */
    Save_Write(&a, path);
    FILE *fp = fopen(path, "r+b");
    fseek(fp, 60, SEEK_SET);
    int c = fgetc(fp);
    fseek(fp, 60, SEEK_SET);
    fputc(c ^ 0x01, fp);
    fclose(fp);
    CHECK_INT(Save_Load(&b, path), SAVE_CORRUPT);

    /* 2. file too long / too short */
    Save_Write(&a, path);
    fp = fopen(path, "ab"); fputc('x', fp); fclose(fp);
    CHECK_INT(Save_Load(&b, path), SAVE_CORRUPT);
    Save_Write(&a, path);
    write_raw(path, &a, 100);
    CHECK_INT(Save_Load(&b, path), SAVE_CORRUPT);
    write_raw(path, "", 0);
    CHECK_INT(Save_Load(&b, path), SAVE_CORRUPT);

    /* 3. right size, wrong magic (checksum fixed up so ONLY the magic is wrong) */
    SaveData m = a;
    m.magic = 0xDEADBEEF;
    m.checksum = Save_Checksum(&m);
    write_raw(path, &m, sizeof m);
    CHECK_INT(Save_Load(&b, path), SAVE_CORRUPT);

    /* 4. a future version */
    SaveData v = a;
    v.version = 99;
    v.checksum = Save_Checksum(&v);
    write_raw(path, &v, sizeof v);
    CHECK_INT(Save_Load(&b, path), SAVE_BAD_VERSION);

    /* 5. a text file pretending to be a save */
    char junk[244];
    memset(junk, 'A', sizeof junk);
    write_raw(path, junk, sizeof junk);
    CHECK_INT(Save_Load(&b, path), SAVE_CORRUPT);
    TmpRemove(SAVE_TMP);
}

static void test_save_sanitises_values(void)
{
    SaveData a = sample_save(), b;
    const char *path = TmpPath(SAVE_TMP);

    a.cleared_mask = 0xFFFFFFFFu;                    /* bits above level 24 are nonsense */
    a.best_strokes[3] = -7;
    a.best_time[4] = -1.0f;
    a.best_time[5] = (float)NAN;
    memset(a.name, 'Z', sizeof a.name);              /* no terminator at all */
    a.checksum = Save_Checksum(&a);
    write_raw(path, &a, sizeof a);

    CHECK_INT(Save_Load(&b, path), SAVE_OK);         /* valid checksum: loads, but cleaned */
    CHECK_INT(b.cleared_mask, (1u << LEVEL_COUNT) - 1u);
    CHECK_INT(b.best_strokes[3], 0);
    CHECK_NEAR(b.best_time[4], 0.0, 0);
    CHECK_NEAR(b.best_time[5], 0.0, 0);
    CHECK_INT(b.name[PLAYER_NAME_MAX - 1], 0);
    CHECK_INT(strlen(b.name), PLAYER_NAME_MAX - 1);
    TmpRemove(SAVE_TMP);
}

static void test_checksum(void)
{
    SaveData a = sample_save(), b = a;
    CHECK_INT(Save_Checksum(&a), Save_Checksum(&b));            /* deterministic */
    b.best_strokes[7] = 1;
    CHECK(Save_Checksum(&a) != Save_Checksum(&b));              /* any change is noticed */
    b = a;
    b.name[0] ^= 1;
    CHECK(Save_Checksum(&a) != Save_Checksum(&b));
    b = a;
    b.checksum = 12345;                                          /* the checksum field itself is not hashed */
    CHECK_INT(Save_Checksum(&a), Save_Checksum(&b));
}

static void test_result_text(void)
{
    CHECK_STR(Save_ResultText(SAVE_OK), "ok");
    CHECK(strlen(Save_ResultText(SAVE_NOT_FOUND)) > 0);
    CHECK(strlen(Save_ResultText(SAVE_CORRUPT)) > 0);
    CHECK(strlen(Save_ResultText(SAVE_BAD_VERSION)) > 0);
    CHECK(strlen(Save_ResultText(SAVE_IO_ERROR)) > 0);
    for (int i = 0; i < 4; i++)
        for (int j = i + 1; j < 5; j++)
            CHECK(strcmp(Save_ResultText((SaveResult)i), Save_ResultText((SaveResult)j)) != 0);
}

/* ---- progress & bit operations ------------------------------------------------ */
static void test_record_clear_rules(void)
{
    SaveData s;
    Save_Init(&s, "T");
    CHECK(!Save_IsCleared(&s, 3));
    CHECK(Save_RecordClear(&s, 3, 5, 20.0f));         /* first clear: always a "best" */
    CHECK(Save_IsCleared(&s, 3));
    CHECK_INT(s.best_strokes[3], 5);

    CHECK(!Save_RecordClear(&s, 3, 7, 5.0f));          /* more strokes: worse, even if faster */
    CHECK_INT(s.best_strokes[3], 5);
    CHECK_NEAR(s.best_time[3], 20.0, 0);
    CHECK(Save_RecordClear(&s, 3, 4, 99.0f));          /* fewer strokes: better */
    CHECK_INT(s.best_strokes[3], 4);
    CHECK(Save_RecordClear(&s, 3, 4, 50.0f));          /* same strokes, faster */
    CHECK_NEAR(s.best_time[3], 50.0, 0);
    CHECK(!Save_RecordClear(&s, 3, 4, 60.0f));         /* same strokes, slower */
    CHECK_NEAR(s.best_time[3], 50.0, 0);

    CHECK(Save_RecordClear(&s, 8, 0, 3.0f));           /* 0 shots is stored as 1 */
    CHECK_INT(s.best_strokes[8], 1);

    CHECK(!Save_RecordClear(&s, -1, 1, 1.0f));         /* invalid levels are refused */
    CHECK(!Save_RecordClear(&s, 25, 1, 1.0f));
    CHECK(!Save_IsCleared(&s, -1));
    CHECK(!Save_IsCleared(&s, 25));
}

static int naive_popcount(unsigned x)
{
    int n = 0;
    for (int i = 0; i < 32; i++)
        if (x & (1u << i)) n++;
    return n;
}

static void test_bit_counting_and_search(void)
{
    SaveData s;
    Save_Init(&s, "T");
    CHECK_INT(Save_CountCleared(&s), 0);
    CHECK_INT(Save_FirstUncleared(&s), 0);

    s.cleared_mask = 0x7;                              /* levels 0,1,2 */
    CHECK_INT(Save_CountCleared(&s), 3);
    CHECK_INT(Save_FirstUncleared(&s), 3);

    s.cleared_mask = 0x5;                              /* 0 and 2: gap at 1 */
    CHECK_INT(Save_FirstUncleared(&s), 1);

    s.cleared_mask = (1u << LEVEL_COUNT) - 1u;         /* everything */
    CHECK_INT(Save_CountCleared(&s), 25);
    CHECK_INT(Save_FirstUncleared(&s), LEVEL_COUNT);

    s.cleared_mask = 1u << 24;                         /* only the last one */
    CHECK_INT(Save_CountCleared(&s), 1);
    CHECK_INT(Save_FirstUncleared(&s), 0);

    TestSeed(77);                                      /* random masks vs the naive loop */
    for (int i = 0; i < 500; i++) {
        s.cleared_mask = TestRand() & ((1u << LEVEL_COUNT) - 1u);
        CHECK_INT(Save_CountCleared(&s), naive_popcount(s.cleared_mask));
        int first = Save_FirstUncleared(&s);
        CHECK(first == LEVEL_COUNT || !Save_IsCleared(&s, first));
        for (int k = 0; k < first; k++) CHECK(Save_IsCleared(&s, k));
    }
}

static void test_totals_and_score(void)
{
    SaveData s = sample_save();                         /* levels 0,1,24 : 2+4+9 strokes */
    CHECK_INT(Save_TotalBestStrokes(&s), 15);
    CHECK_NEAR(Save_TotalBestTime(&s), 12.5 + 30.0 + 99.9, 1e-3);
    /* 3 levels * 1000 - 15 strokes * 40 - 142.4 s * 2 = 3000 - 600 - 284.8 = 2115 */
    CHECK_INT(Save_Score(&s), 2115);

    s.best_strokes[24] = 9;                             /* uncleared levels do not count */
    s.cleared_mask &= ~(1u << 24);
    CHECK_INT(Save_TotalBestStrokes(&s), 6);

    SaveData bad;                                       /* score never goes negative */
    Save_Init(&bad, "B");
    Save_RecordClear(&bad, 0, 500, 5000.0f);
    CHECK_INT(Save_Score(&bad), 0);
    Save_Init(&bad, "B");
    CHECK_INT(Save_Score(&bad), 0);
}

/* ---- per-player files ---------------------------------------------------------- */
static void test_path_for(void)
{
    char p[128];
    Save_PathFor("Andi Putra", p, sizeof p);
    CHECK_STR(p, "gritty_save_andi_putra.dat");
    Save_PathFor("A-b_C9!", p, sizeof p);
    CHECK_STR(p, "gritty_save_a_b_c9_.dat");               /* only [a-z0-9] survive */
    Save_PathFor("../../etc/passwd", p, sizeof p);
    CHECK(strchr(p, '/') == NULL && strchr(p, '\\') == NULL);  /* no path traversal */
    Save_PathFor("123456789012345678901234567890", p, sizeof p);
    CHECK(strlen(p) < sizeof "gritty_save_.dat" + PLAYER_NAME_MAX);
    char tiny[10];
    Save_PathFor("abcdef", tiny, sizeof tiny);               /* small buffer: truncated, never overflows */
    CHECK(strlen(tiny) < sizeof tiny);
}

static void test_open_profile(void)
{
    const char *name = "ZZ Test Profile";
    char path[300];
    SaveData s;
    Save_PathFor(name, path, sizeof path);
    remove(path);
    char bad[320];
    snprintf(bad, sizeof bad, "%s.bad", path);
    remove(bad);

    /* new profile */
    char out[300];
    CHECK_INT(Save_OpenProfile(&s, name, out, sizeof out), SAVE_NOT_FOUND);
    CHECK_STR(out, path);
    CHECK_STR(s.name, name);
    CHECK_INT(s.cleared_mask, 0);

    /* existing profile (name typed in other case is accepted) */
    Save_RecordClear(&s, 4, 3, 10.0f);
    CHECK(Save_Write(&s, path));
    SaveData t;
    CHECK_INT(Save_OpenProfile(&t, "zz test profile", out, sizeof out), SAVE_OK);
    CHECK(Save_IsCleared(&t, 4));
    CHECK_STR(t.name, "zz test profile");

    /* damaged profile: moved aside as .bad, fresh profile returned */
    write_raw(path, "garbage", 7);
    CHECK_INT(Save_OpenProfile(&t, name, out, sizeof out), SAVE_CORRUPT);
    CHECK_INT(t.cleared_mask, 0);
    CHECK(File_Size(bad) == 7);                              /* the broken file was kept */
    CHECK_INT(File_Size(path), -1);
    remove(bad);
    remove(path);
}

/* ---- settings ------------------------------------------------------------------ */
static void test_settings_defaults_and_round_trip(void)
{
    Settings s;
    Settings_Default(&s);
    CHECK_INT(s.volume, 70);
    CHECK(!s.aim_assist);
    CHECK(!s.fullscreen);
    CHECK_INT(s.window_scale, 3);
    CHECK_STR(s.last_player, "PLAYER");

    TmpRemove("cfg");
    s.volume = 33; s.aim_assist = true; s.fullscreen = true; s.window_scale = 5;
    Str_Copy(s.last_player, sizeof s.last_player, "Andi Putra");
    CHECK(Settings_Save(&s, TmpPath("cfg")));

    Settings t;
    CHECK(Settings_Load(&t, TmpPath("cfg")));
    CHECK_INT(t.volume, 33);
    CHECK(t.aim_assist);
    CHECK(t.fullscreen);
    CHECK_INT(t.window_scale, 5);
    CHECK_STR(t.last_player, "Andi Putra");                 /* names with spaces survive */
    TmpRemove("cfg");

    CHECK(!Settings_Save(&s, "no_such_dir_xyz/x.cfg"));
}

static void test_settings_parsing_rules(void)
{
    Settings s;
    TmpRemove("cfg");
    CHECK(!Settings_Load(&s, TmpPath("cfg")));              /* missing: false, but defaults filled */
    CHECK_INT(s.volume, 70);

    write_text(TmpPath("cfg"),
               "# comment line\n"
               "\n"
               "volume=500\n"                                /* clamped to 100 */
               "window_scale = 0\n"                          /* spaces around '=' ; clamped to 1 */
               "aim_assist=1\n"
               "unknown_key=42\n"                            /* ignored */
               "this line has no equals sign\n"
               "fullscreen=abc\n"                            /* not a number -> 0 -> false */
               "last_player=bad,name\n");                    /* invalid name -> keep default */
    CHECK(Settings_Load(&s, TmpPath("cfg")));
    CHECK_INT(s.volume, 100);
    CHECK_INT(s.window_scale, 1);
    CHECK(s.aim_assist);
    CHECK(!s.fullscreen);
    CHECK_STR(s.last_player, "PLAYER");

    write_text(TmpPath("cfg"), "volume=-20\nwindow_scale=99\n");
    Settings_Load(&s, TmpPath("cfg"));
    CHECK_INT(s.volume, 0);
    CHECK_INT(s.window_scale, 6);
    TmpRemove("cfg");
}

/* ---- leaderboard CSV ------------------------------------------------------------- */
static ScoreRecord rec(const char *name, int levels, int strokes, float time)
{
    ScoreRecord r;
    memset(&r, 0, sizeof r);
    Str_Copy(r.name, sizeof r.name, name);
    r.levels = levels; r.strokes = strokes; r.time = time;
    Str_Copy(r.date, sizeof r.date, "2026-11-08");
    return r;
}

static void test_records_missing_file(void)
{
    ScoreRecord *list = (ScoreRecord *)0x1;
    int n = -1;
    TmpRemove("csv");
    CHECK(Records_Load(TmpPath("csv"), &list, &n));          /* a missing file is an empty list */
    CHECK_INT(n, 0);
    CHECK(list == NULL);
    CHECK_INT(File_Size(TmpPath("csv")), -1);
}

static void test_records_round_trip(void)
{
    const char *path = TmpPath("csv");
    TmpRemove("csv");
    ScoreRecord in[3] = { rec("Andi", 25, 212, 1334.5f), rec("Budi Santoso", 12, 80, 400.0f), rec("Cici", 3, 9, 33.3f) };
    CHECK(Records_Save(path, in, 3));

    ScoreRecord *out = NULL;
    int n = 0;
    CHECK(Records_Load(path, &out, &n));
    CHECK_INT(n, 3);
    if (n == 3) {
        for (int i = 0; i < 3; i++) {
            CHECK_STR(out[i].name, in[i].name);
            CHECK_INT(out[i].levels, in[i].levels);
            CHECK_INT(out[i].strokes, in[i].strokes);
            CHECK_NEAR(out[i].time, in[i].time, 0.05);       /* stored with 1 decimal */
            CHECK_STR(out[i].date, "2026-11-08");
        }
    }
    free(out);

    CHECK(Records_Save(path, in, 0));                         /* header only -> empty list */
    CHECK(Records_Load(path, &out, &n));
    CHECK_INT(n, 0);
    free(out);
    TmpRemove("csv");
}

static void test_records_tolerates_damage(void)
{
    const char *path = TmpPath("csv");
    write_text(path,
               "name,levels,strokes,time,date\n"
               "Good One,10,50,100.5,2026-01-01\n"
               "\n"                                          /* blank */
               "this is not a record\n"                      /* garbage */
               "Missing,Fields\n"                            /* too few fields */
               "BadNumber,x,2,3.0,2026-01-01\n"              /* non-numeric */
               "Second,5,20,60.0,2026-02-02\r\n");           /* Windows line ending */
    ScoreRecord *list = NULL;
    int n = 0;
    CHECK(Records_Load(path, &list, &n));
    CHECK_INT(n, 2);
    if (n == 2) {
        CHECK_STR(list[0].name, "Good One");
        CHECK_STR(list[1].name, "Second");
        CHECK_STR(list[1].date, "2026-02-02");
    }
    free(list);

    /* a big file forces the array to grow several times (realloc path) */
    FILE *fp = fopen(path, "w");
    fprintf(fp, "name,levels,strokes,time,date\n");
    for (int i = 0; i < 100; i++)
        fprintf(fp, "P%d,%d,%d,%d.0,2026-01-01\n", i, i % 26, i, i);
    fclose(fp);
    CHECK(Records_Load(path, &list, &n));
    CHECK_INT(n, 100);
    CHECK_STR(list[99].name, "P99");
    CHECK_INT(list[57].strokes, 57);
    free(list);
    TmpRemove("csv");
}

static void test_records_upsert(void)
{
    ScoreRecord *list = NULL;
    int n = 0;

    CHECK(Records_Upsert(&list, &n, &(ScoreRecord){ 0 }) || 1);   /* (just must not crash) */
    free(list); list = NULL; n = 0;

    ScoreRecord a = rec("Andi", 10, 50, 100.0f);
    CHECK(Records_Upsert(&list, &n, &a));                    /* insert */
    CHECK_INT(n, 1);

    ScoreRecord better = rec("ANDI", 12, 60, 200.0f);        /* same player (case-insensitive), more levels */
    CHECK(Records_Upsert(&list, &n, &better));
    CHECK_INT(n, 1);
    CHECK_INT(list[0].levels, 12);
    CHECK_STR(list[0].name, "ANDI");

    ScoreRecord worse = rec("andi", 5, 10, 10.0f);           /* fewer levels: worse, kept the old one */
    CHECK(!Records_Upsert(&list, &n, &worse));
    CHECK_INT(list[0].levels, 12);

    ScoreRecord faster = rec("Andi", 12, 60, 150.0f);        /* same levels+strokes, shorter time */
    CHECK(Records_Upsert(&list, &n, &faster));
    CHECK_NEAR(list[0].time, 150.0, 0);

    ScoreRecord same = faster;                               /* identical: not an improvement */
    CHECK(!Records_Upsert(&list, &n, &same));

    for (int i = 0; i < 20; i++) {                           /* growth by realloc */
        char nm[24]; snprintf(nm, sizeof nm, "Player%d", i);
        ScoreRecord r = rec(nm, 1, 1, 1.0f);
        CHECK(Records_Upsert(&list, &n, &r));
    }
    CHECK_INT(n, 21);
    CHECK_INT(Search_LinearName(list, n, "player7"), 8);
    free(list);
}

static void test_records_submit_profile(void)
{
    const char *path = TmpPath("csv");
    TmpRemove("csv");

    SaveData s;
    Save_Init(&s, "Andi");
    CHECK(!Records_SubmitProfile(path, &s));                 /* nothing cleared: nothing submitted */
    CHECK_INT(File_Size(path), -1);

    Save_RecordClear(&s, 0, 3, 10.0f);
    Save_RecordClear(&s, 1, 5, 20.0f);
    CHECK(Records_SubmitProfile(path, &s));
    CHECK(!Records_SubmitProfile(path, &s));                 /* unchanged: no rewrite */

    ScoreRecord *list = NULL; int n = 0;
    Records_Load(path, &list, &n);
    CHECK_INT(n, 1);
    CHECK_INT(list[0].levels, 2);
    CHECK_INT(list[0].strokes, 8);
    CHECK_NEAR(list[0].time, 30.0, 0.05);
    CHECK_INT(strlen(list[0].date), 10);
    free(list);

    Save_RecordClear(&s, 2, 4, 15.0f);                       /* progress -> leaderboard improves */
    CHECK(Records_SubmitProfile(path, &s));

    SaveData other;                                          /* a second player gets a second line */
    Save_Init(&other, "Budi");
    Save_RecordClear(&other, 0, 2, 5.0f);
    CHECK(Records_SubmitProfile(path, &other));
    Records_Load(path, &list, &n);
    CHECK_INT(n, 2);
    Sort_InsertionRecords(list, n, Cmp_ByRank);
    CHECK_STR(list[0].name, "Andi");                         /* 3 levels beats 1 level */
    free(list);
    TmpRemove("csv");
}

static void test_file_size(void)
{
    const char *path = TmpPath("size");
    write_raw(path, "hello", 5);
    CHECK_INT(File_Size(path), 5);
    write_raw(path, "", 0);
    CHECK_INT(File_Size(path), 0);
    TmpRemove("size");
    CHECK_INT(File_Size(path), -1);
}

void run_files_tests(void)
{
    RUN(test_save_init);
    RUN(test_save_round_trip);
    RUN(test_save_not_found_and_bad_path);
    RUN(test_save_detects_corruption);
    RUN(test_save_sanitises_values);
    RUN(test_checksum);
    RUN(test_result_text);
    RUN(test_record_clear_rules);
    RUN(test_bit_counting_and_search);
    RUN(test_totals_and_score);
    RUN(test_path_for);
    RUN(test_open_profile);
    RUN(test_settings_defaults_and_round_trip);
    RUN(test_settings_parsing_rules);
    RUN(test_records_missing_file);
    RUN(test_records_round_trip);
    RUN(test_records_tolerates_damage);
    RUN(test_records_upsert);
    RUN(test_records_submit_profile);
    RUN(test_file_size);
}
