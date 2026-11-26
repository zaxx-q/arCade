/* The assets that are compiled into the executable: the generated table, arc_asset_load (built-in copy and
 * override folder) and the helpers that parse them from memory. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arcade/assets.h"
#include "arcade/str.h"
#include "arcade_test.h"

/* The only file these tests create (in the working directory). Never write a name of a real file here: the
 * tests may be started from anywhere, even from the repository root. */
#define TMP_FILE "arcade_test_asset.tmp"

static bool write_file(const char *path, const char *text) {
    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
        return false;
    bool ok = fputs(text, fp) >= 0;
    return fclose(fp) == 0 && ok;
}

static void test_table_is_sane(void) {
    CHECK(g_arcade_asset_count > 0);
    for (int i = 0; i < g_arcade_asset_count; i++) {
        const ArcAssetEntry *e = &g_arcade_assets[i];
        CHECK(e->name != NULL && e->name[0] != '\0');
        CHECK(e->data != NULL && e->size > 0);
        CHECK_EQ_INT(e->data[e->size], 0);    /* the extra 0 byte: a text asset is also a C string */
        CHECK(strchr(e->name, '\\') == NULL); /* '/' on every OS */
        for (int j = i + 1; j < g_arcade_asset_count; j++)
            CHECK(strcmp(e->name, g_arcade_assets[j].name) != 0); /* no name twice */
    }
}

/* The generator must not lose, change or reorder a byte: every entry equals the file it came from, and no
 * file of assets/ is missing from the table (the count is computed by CMake from the folder). */
static void test_embedded_files_equal_the_files(void) {
    CHECK_EQ_INT(g_arcade_asset_count, ARCADE_ASSET_TOTAL);
    for (int i = 0; i < g_arcade_asset_count; i++) {
        const ArcAssetEntry *e = &g_arcade_assets[i];
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", ARCADE_ASSETS_DIR, e->name);
        ArcAsset f = arc_asset_read_file(path);
        if (f.data == NULL) { /* LICENSE and THIRD_PARTY_NOTICES.md come from the repository root */
            snprintf(path, sizeof(path), "%s/%s", ARCADE_ROOT_DIR, e->name);
            f = arc_asset_read_file(path);
        }
        if (f.data == NULL)
            printf("    missing on disk: %s\n", e->name);
        CHECK(f.data != NULL);
        bool same = f.size == e->size && memcmp(f.data, e->data, e->size) == 0;
        if (!same)
            printf("    differs from the file: %s\n", e->name);
        arc_asset_release(&f);
        CHECK(same);
    }
}

/* Files that the games and `arCade --licenses` ask for by name: renaming one must fail a test, not a player.
 */
static void test_the_expected_assets_exist(void) {
    static const char *const names[] = {"fonts/PoiretOne-Regular.ttf",
                                        "fonts/OFL.txt",
                                        "beatmaps/songs.txt",
                                        "gritty_golf/font/silkscreen-8.fnt",
                                        "gritty_golf/font/silkscreen-16.fnt",
                                        "gritty_golf/font/silkscreen-8.png",
                                        "gritty_golf/font/silkscreen-16.png",
                                        "gritty_golf/font/OFL-Silkscreen.txt",
                                        "gritty_golf/textures/sprBall.png",
                                        "gritty_golf/audio/sfxBallHit.wav",
                                        "LICENSE",
                                        "THIRD_PARTY_NOTICES.md"};
    for (size_t i = 0; i < ARC_ARRAY_LEN(names); i++) {
        ArcAsset a = arc_asset_load("", names[i]);
        if (a.data == NULL)
            printf("    not built in: %s\n", names[i]);
        CHECK(a.data != NULL && a.size > 0);
        CHECK(a.heap == NULL); /* the built-in copy: nothing to free */
        arc_asset_release(&a);
    }
    ArcAsset ttf = arc_asset_load(NULL, "fonts/PoiretOne-Regular.ttf"); /* NULL = "" = built in */
    CHECK(ttf.data != NULL && ttf.size > 1000);
    CHECK(ttf.data[0] == 0x00 && ttf.data[1] == 0x01 && ttf.data[2] == 0x00 && ttf.data[3] == 0x00);
    ArcAsset png = arc_asset_load("", "gritty_golf/textures/sprBall.png");
    CHECK(png.data != NULL && png.size > 8 && memcmp(png.data + 1, "PNG", 3) == 0);
}

static void test_unknown_asset(void) {
    ArcAsset a = arc_asset_load("", "no/such/file.bin");
    CHECK(a.data == NULL);
    CHECK_EQ_INT(a.size, 0);
    arc_asset_release(&a); /* harmless */
    a = arc_asset_load("", NULL);
    CHECK(a.data == NULL);
    a = arc_asset_load("definitely_missing_folder", "no/such/file.bin");
    CHECK(a.data == NULL);
}

static void test_override_folder(void) {
    /* the same file twice: from the override folder (a heap copy) and from the executable (no copy) */
    ArcAsset disk = arc_asset_load(ARCADE_ASSETS_DIR, "beatmaps/songs.txt");
    ArcAsset built_in = arc_asset_load("", "beatmaps/songs.txt");
    CHECK(disk.data != NULL && built_in.data != NULL);
    CHECK(disk.heap != NULL && built_in.heap == NULL);
    CHECK(disk.size == built_in.size && memcmp(disk.data, built_in.data, disk.size) == 0);
    CHECK_EQ_INT(disk.data[disk.size], 0);
    arc_asset_release(&disk);
    CHECK(disk.data == NULL && disk.heap == NULL);
    arc_asset_release(&disk); /* twice is fine */

    /* a folder that does not have the file (this one holds only songs): the built-in copy is used */
    ArcAsset fallback = arc_asset_load(ARCADE_ASSETS_DIR "/beatmaps", "fonts/OFL.txt");
    CHECK(fallback.data != NULL && fallback.heap == NULL);
}

static void test_read_file(void) {
    remove(TMP_FILE);
    ArcAsset none = arc_asset_read_file(TMP_FILE);
    CHECK(none.data == NULL);

    CHECK(write_file(TMP_FILE, ""));
    ArcAsset empty = arc_asset_read_file(TMP_FILE);
    CHECK(empty.data != NULL && empty.size == 0 && empty.data[0] == 0);
    arc_asset_release(&empty);

    CHECK(write_file(TMP_FILE, "ab\r\ncd")); /* binary mode: nothing is converted */
    ArcAsset f = arc_asset_read_file(TMP_FILE);
    CHECK(f.data != NULL && f.size == 6);
    CHECK(memcmp(f.data, "ab\r\ncd", 6) == 0 && f.data[6] == 0);
    arc_asset_release(&f);
    remove(TMP_FILE);
}

static void test_next_line(void) {
    const char text[] = "first\nsecond\r\n\nlast";
    size_t len = strlen(text), pos = 0;
    char line[32];
    CHECK(arc_next_line(text, len, &pos, line, sizeof(line)));
    CHECK_STR_EQ(line, "first\n"); /* like fgets, the newline stays */
    CHECK(arc_next_line(text, len, &pos, line, sizeof(line)));
    CHECK_STR_EQ(line, "second\r\n");
    CHECK(arc_next_line(text, len, &pos, line, sizeof(line)));
    CHECK_STR_EQ(line, "\n");
    CHECK(arc_next_line(text, len, &pos, line, sizeof(line)));
    CHECK_STR_EQ(line, "last"); /* no newline at the end */
    CHECK(!arc_next_line(text, len, &pos, line, sizeof(line)));

    /* a line that does not fit comes out in pieces, exactly like fgets */
    pos = 0;
    char small[4];
    CHECK(arc_next_line(text, len, &pos, small, sizeof(small)));
    CHECK_STR_EQ(small, "fir");
    CHECK(arc_next_line(text, len, &pos, small, sizeof(small)));
    CHECK_STR_EQ(small, "st\n");
    CHECK(!arc_next_line(text, len, &pos, small, 0)); /* no room, not even for the NUL */

    /* the length decides, not a NUL: a text is not copied past `len` */
    pos = 0;
    CHECK(arc_next_line("abcdef", 3, &pos, line, sizeof(line)));
    CHECK_STR_EQ(line, "abc");
    CHECK(!arc_next_line("abcdef", 3, &pos, line, sizeof(line)));
    pos = 0;
    CHECK(!arc_next_line("", 0, &pos, line, sizeof(line)));
}

int main(void) {
    RUN(test_table_is_sane);
    RUN(test_embedded_files_equal_the_files);
    RUN(test_the_expected_assets_exist);
    RUN(test_unknown_asset);
    RUN(test_override_folder);
    RUN(test_read_file);
    RUN(test_next_line);
    return TEST_SUMMARY();
}
