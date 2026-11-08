#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "beat_dodge_logic.h"

void bd_beatmap_default(bd_BeatMap *out, unsigned seed) {
    Rng rng;
    rng_seed(&rng, seed);
    out->bpm = 120;
    out->length = 64;
    for (int i = 0; i < out->length; i++) {
        bool rest = i < 4 || rng_chance(&rng, 0.35f); /* four beats of warm-up, then ~65% obstacles */
        out->lane[i] = rest ? -1 : (int8_t)rng_range(&rng, 0, BD_LANES - 1);
    }
}

bool bd_beatmap_load(const char *path, bd_BeatMap *out) {
    FILE *fp = fopen(path, "r");
    if (!fp)
        return false;
    bd_BeatMap m;
    memset(&m, 0, sizeof(m));
    char line[256];
    bool ok = true;
    while (ok && fgets(line, sizeof(line), fp)) {
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0')
            continue;
        if (strncmp(p, "bpm", 3) == 0) {
            m.bpm = atoi(p + 3);
            continue;
        }
        for (; *p && *p != '\n' && *p != '\r'; p++) {
            if (*p == ' ' || *p == '\t')
                continue;
            if (m.length >= BD_MAX_BEATS)
                break;
            if (*p == '.')
                m.lane[m.length++] = -1;
            else if (*p >= '0' && *p < '0' + BD_LANES)
                m.lane[m.length++] = (int8_t)(*p - '0');
            else
                ok = false; /* unknown character: reject the file */
        }
    }
    fclose(fp);
    if (!ok || m.bpm < 60 || m.bpm > 200 || m.length < 8)
        return false;
    *out = m;
    return true;
}

static void restart(beat_dodge_State *s) {
    s->time = 0.0f;
    s->spb = 60.0f / (float)s->map.bpm;
    s->next_hit = 0;
    s->last_beat = -1;
    s->beat_event = false;
    s->lane = 1;
    s->lives = BD_START_LIVES;
    s->dodged = s->combo = s->best_combo = s->score = 0;
    s->hit_flash = 0.0f;
    s->over = s->won = false;
}

void beat_dodge_init(beat_dodge_State *s, unsigned seed) {
    bd_beatmap_default(&s->map, seed);
    restart(s);
}

void beat_dodge_use_map(beat_dodge_State *s, const bd_BeatMap *map) {
    s->map = *map;
    restart(s);
}

float beat_dodge_obstacle_y(const beat_dodge_State *s, int i) {
    float arrive = ((float)i + BD_LEAD_BEATS) * s->spb; /* the moment obstacle i reaches the player line */
    float k = 1.0f - (arrive - s->time) / (BD_LEAD_BEATS * s->spb); /* 0 = just spawned, 1 = arrived */
    return BD_TOP_Y + (BD_PLAYER_Y - BD_TOP_Y) * k;
}

/* TODO(Member 5): drive `time` from a music stream clock instead of the fixed dt so the beat stays locked to
 * real audio (GAME.md step 1). */
void beat_dodge_step(beat_dodge_State *s, const ArcadeInput *in, float dt) {
    if (s->over)
        return;
    s->time += dt;
    s->hit_flash = arc_maxf(0.0f, s->hit_flash - dt);

    int beat = (int)(s->time / s->spb);
    s->beat_event = beat != s->last_beat;
    s->last_beat = beat;

    if (in->left_pressed)
        s->lane = arc_maxi(0, s->lane - 1);
    if (in->right_pressed)
        s->lane = arc_mini(BD_LANES - 1, s->lane + 1);

    /* resolve every obstacle that has now arrived at the player line */
    while (s->next_hit < s->map.length && s->time >= ((float)s->next_hit + BD_LEAD_BEATS) * s->spb &&
           !s->over) {
        int l = s->map.lane[s->next_hit];
        if (l >= 0) {
            if (l == s->lane) { /* hit */
                s->lives--;
                s->combo = 0;
                s->hit_flash = 0.35f;
                if (s->lives <= 0)
                    s->over = true;
            } else { /* dodged: the longer the combo, the more each dodge is worth (max x4) */
                s->dodged++;
                s->combo++;
                s->best_combo = arc_maxi(s->best_combo, s->combo);
                s->score += 10 * arc_mini(s->combo, 4);
            }
        }
        s->next_hit++;
    }
    if (!s->over && s->next_hit >= s->map.length) { /* survived the whole song */
        s->won = true;
        s->over = true;
        s->score += s->lives * 50;
    }
}

bool beat_dodge_is_over(const beat_dodge_State *s) {
    return s->over;
}

int beat_dodge_score(const beat_dodge_State *s) {
    return s->score;
}
