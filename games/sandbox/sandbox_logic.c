#include "sandbox_logic.h"

#define BRUSH_MIN 0
#define BRUSH_MAX 12

static void build_scene(sbx_State *s) {
    sandbox_clear(&s->world);
    for (int x = 60; x < 140; x++)
        sandbox_set(&s->world, x, 120, EL_STONE);
    for (int y = 100; y < 120; y++) {
        sandbox_set(&s->world, 60, y, EL_STONE);
        sandbox_set(&s->world, 139, y, EL_STONE);
    }
    for (int x = 190; x < 280; x++)
        sandbox_set(&s->world, x, 150, EL_STONE);
}

void sbx_init(sbx_State *s, unsigned seed) {
    sandbox_init(&s->world, SBX_W, SBX_H, seed);
    s->element = EL_SAND;
    s->brush = 3;
    s->paused = false;
    s->ticks = 0;
    build_scene(s);
}

/* TODO(Leader): new elements go in core/sandbox.[ch] (see docs/SANDBOX.md); then give them a key here. */
static void handle_keys(sbx_State *s, const ArcadeInput *in) {
    if (in->typed >= '0' && in->typed < '0' + (int)EL_COUNT)
        s->element = (ElementId)(in->typed - '0');
    if (in->typed == 'c')
        sandbox_clear(&s->world);
    if (in->typed == 'r')
        build_scene(s);
    if (in->typed == '+' || in->typed == '=')
        s->brush = arc_clampi(s->brush + 1, BRUSH_MIN, BRUSH_MAX);
    if (in->typed == '-')
        s->brush = arc_clampi(s->brush - 1, BRUSH_MIN, BRUSH_MAX);
    if (in->wheel != 0)
        s->element = (ElementId)(((int)s->element + in->wheel + EL_COUNT) % EL_COUNT);
    if (in->action_pressed)
        s->paused = !s->paused;
}

void sbx_step(sbx_State *s, const ArcadeInput *in, float dt) {
    (void)dt;
    handle_keys(s, in);
    int cx = (int)(in->aim_x / SBX_CELL), cy = (int)(in->aim_y / SBX_CELL);
    if (in->fire)
        sandbox_paint(&s->world, cx, cy, s->brush, s->element);
    if (in->alt)
        sandbox_paint(&s->world, cx, cy, s->brush, EL_EMPTY);
    if (!s->paused) {
        sandbox_step(&s->world);
        s->ticks++;
    }
}

bool sbx_is_over(const sbx_State *s) {
    (void)s;
    return false;
}

int sbx_score(const sbx_State *s) {
    return s->world.w * s->world.h - sandbox_count(&s->world, EL_EMPTY);
}
