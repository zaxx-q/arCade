#include "arcade/assets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSET_MAX_BYTES (64L * 1024L * 1024L) /* a file bigger than this is not an asset: refuse it */

static ArcAsset not_found(void) {
    ArcAsset none = {NULL, 0, NULL};
    return none;
}

ArcAsset arc_asset_read_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return not_found();
    /* the size first, so one malloc is enough (a directory reports a huge size and is refused here) */
    long size = -1;
    if (fseek(fp, 0, SEEK_END) == 0)
        size = ftell(fp);
    if (size < 0 || size > ASSET_MAX_BYTES || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return not_found();
    }
    unsigned char *buf = (unsigned char *)malloc((size_t)size + 1u); /* +1: the extra 0 byte */
    if (buf == NULL) {
        fclose(fp);
        return not_found();
    }
    size_t got = fread(buf, 1, (size_t)size, fp);
    bool read_error = ferror(fp) != 0;
    fclose(fp);
    if (read_error || got != (size_t)size) {
        free(buf);
        return not_found();
    }
    buf[got] = 0;
    ArcAsset a = not_found();
    a.data = buf;
    a.size = got;
    a.heap = buf; /* the caller owns it now: arc_asset_release() frees it */
    return a;
}

ArcAsset arc_asset_load(const char *dir, const char *name) {
    if (name == NULL)
        return not_found();
    if (dir != NULL && dir[0] != '\0') { /* the override folder wins when it has the file */
        char path[1024];
        int n = snprintf(path, sizeof(path), "%s/%s", dir, name);
        if (n > 0 && (size_t)n < sizeof(path)) {
            ArcAsset a = arc_asset_read_file(path);
            if (a.data != NULL)
                return a;
        }
    }
    for (int i = 0; i < g_arcade_asset_count; i++) {
        if (strcmp(g_arcade_assets[i].name, name) == 0) {
            /* no copy is made, so there is nothing to free later */
            ArcAsset a = {g_arcade_assets[i].data, g_arcade_assets[i].size, NULL};
            return a;
        }
    }
    return not_found();
}

void arc_asset_release(ArcAsset *a) {
    if (a == NULL)
        return;
    free(a->heap); /* NULL for a built-in asset: free(NULL) does nothing */
    a->heap = NULL;
    a->data = NULL;
    a->size = 0;
}
