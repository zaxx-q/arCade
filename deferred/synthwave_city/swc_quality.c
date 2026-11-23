/* Synthwave City - quality tiers and the adaptive render scale (see swc_quality.h). */
#include "swc_quality.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

/* src/contracts/quality.ts, field for field. */
static const SwcQuality TABLE[SWC_TIER_COUNT] = {
    {SWC_TIER_LOW, "low", 1.0, 0, 0.0, 0.35, 1300.0, 1800.0, false, false, 0.5, 0.85},
    {SWC_TIER_MEDIUM, "medium", 1.5, 0, 0.5, 0.6, 1900.0, 2400.0, true, false, 0.6, 1.0},
    {SWC_TIER_HIGH, "high", 2.0, 4, 0.5, 1.0, 2500.0, 3100.0, true, true, 0.7, 1.0},
};

const SwcQuality *swc_quality(SwcTier tier) {
    if ((int)tier < 0 || (int)tier >= SWC_TIER_COUNT)
        return &TABLE[SWC_TIER_HIGH];
    return &TABLE[tier];
}

SwcTier swc_tier_next(SwcTier tier) {
    switch (tier) {
    case SWC_TIER_HIGH:
        return SWC_TIER_MEDIUM;
    case SWC_TIER_MEDIUM:
        return SWC_TIER_LOW;
    case SWC_TIER_LOW:
        return SWC_TIER_HIGH;
    }
    return SWC_TIER_HIGH;
}

const char *swc_tier_label(SwcTier tier) {
    switch (tier) {
    case SWC_TIER_LOW:
        return "LOW";
    case SWC_TIER_MEDIUM:
        return "MEDIUM";
    case SWC_TIER_HIGH:
        return "HIGH";
    }
    return "HIGH";
}

bool swc_tier_from_name(const char *name, SwcTier *out) {
    if (name == NULL)
        return false;
    for (int i = 0; i < SWC_TIER_COUNT; i++) {
        const char *want = TABLE[i].name;
        size_t n = strlen(want);
        size_t k = 0;
        while (k < n && name[k] != '\0' && tolower((unsigned char)name[k]) == want[k])
            k++;
        if (k == n && name[k] == '\0') {
            *out = (SwcTier)i;
            return true;
        }
    }
    return false;
}

int swc_quality_crowd_per_face(const SwcQuality *q, int per_face_full) {
    if (!q->crowds)
        return 0;
    return (int)floor((double)per_face_full * q->density + 0.5); /* JavaScript's Math.round */
}

double swc_quality_handover_end(const SwcQuality *q, double handover_end) {
    double limit = q->fade_start * 0.8;
    return handover_end < limit ? handover_end : limit;
}

static int floor_at_least_one(double v) {
    int i = (int)floor(v);
    return i < 1 ? 1 : i;
}

void swc_quality_render_size(const SwcQuality *q, int screen_w, int screen_h, int render_w, int render_h,
                             double scale, int *w, int *h) {
    (void)render_h;
    double dpr = screen_w > 0 ? (double)render_w / (double)screen_w : 1.0;
    if (dpr < 1.0)
        dpr = 1.0;
    double ratio = (dpr < q->max_pixel_ratio ? dpr : q->max_pixel_ratio) * scale;
    *w = floor_at_least_one((double)screen_w * ratio);
    *h = floor_at_least_one((double)screen_h * ratio);
}

void swc_quality_mirror_size(const SwcQuality *q, int w, int h, int *mw, int *mh) {
    if (q->reflection_scale <= 0.0) {
        *mw = *mh = 0;
        return;
    }
    *mw = floor_at_least_one((double)w * q->reflection_scale);
    *mh = floor_at_least_one((double)h * q->reflection_scale);
}

/* ------------------------------------------------------------------ adaptive scale */

static void settle(SwcAdaptiveScale *a) {
    a->over = 0.0;
    a->calm = 0.0;
    a->cooldown = 1.2;
    a->ema = 0.0;
}

void swc_adaptive_init(SwcAdaptiveScale *a, double min, double max) {
    memset(a, 0, sizeof(*a));
    a->min = min;
    a->max = max;
    a->target_ms = 16.7;
    a->step = 0.1;
    a->scale = max;
    a->since_step_up = 1e30; /* never stepped up */
    a->retry_after = 6.0;
}

void swc_adaptive_set_range(SwcAdaptiveScale *a, double min, double max) {
    a->min = min;
    a->max = max;
    a->scale = max;
    a->ema = a->over = a->calm = a->cooldown = 0.0;
    a->since_step_up = 1e30;
    a->retry_after = 6.0;
}

bool swc_adaptive_update(SwcAdaptiveScale *a, double frame_ms, double dt) {
    if (!(dt > 0.0) || !(frame_ms > 0.0))
        return false;
    /* The first seconds are full of shader compiles and uploads; they say nothing about steady cost. */
    a->age += dt;
    if (a->age < SWC_ADAPTIVE_START_GRACE)
        return false;
    double k = dt * 4.0 < 1.0 ? dt * 4.0 : 1.0;
    a->ema = a->ema == 0.0 ? frame_ms : a->ema + (frame_ms - a->ema) * k;
    a->since_step_up += dt;
    if (a->cooldown > 0.0) {
        a->cooldown -= dt;
        return false;
    }
    if (a->ema > a->target_ms * 1.15) {
        a->over += dt;
        a->calm = 0.0;
    } else if (a->ema < a->target_ms * 1.04) {
        a->calm += dt;
        a->over = a->over - dt > 0.0 ? a->over - dt : 0.0;
    } else {
        a->calm = 0.0;
        a->over = a->over - dt * 0.5 > 0.0 ? a->over - dt * 0.5 : 0.0;
    }
    if (a->over >= 1.0 && a->scale > a->min) {
        /* A step up that had to be undone within a few seconds was a mistake: wait longer next time. */
        if (a->since_step_up < 5.0)
            a->retry_after = a->retry_after * 2.0 < 240.0 ? a->retry_after * 2.0 : 240.0;
        double s = a->scale - a->step;
        a->scale = s > a->min ? s : a->min;
        settle(a);
        return true;
    }
    if (a->calm >= a->retry_after && a->scale < a->max) {
        double s = a->scale + a->step / 2.0;
        a->scale = s < a->max ? s : a->max;
        a->since_step_up = 0.0;
        settle(a);
        return true;
    }
    return false;
}
