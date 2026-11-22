#include "sandbox_logic.h"

#define BRUSH_MIN 0
#define BRUSH_MAX 12

/* The starter scene: a stone basin holding some water, a steel kettle sitting on a heater (hover over it to
 * watch the temperatures, or press T for the thermal view), and a shelf of soil with three seeds on it. The
 * seeds sprout by themselves and the kettle slowly comes to the boil; everything else is one click away. */
static void build_scene(sbx_State *s) {
    Sandbox *w = &s->world;
    sandbox_clear(w);
    for (int x = 60; x < 140; x++)
        sandbox_set(w, x, 120, EL_STONE);
    for (int y = 100; y < 120; y++) {
        sandbox_set(w, 60, y, EL_STONE);
        sandbox_set(w, 139, y, EL_STONE);
    }
    for (int y = 110; y < 120; y++)
        for (int x = 61; x < 139; x++)
            sandbox_set(w, x, y, EL_WATER);
    for (int x = 190; x < 280; x++)
        sandbox_set(w, x, 150, EL_STONE);
    for (int y = 146; y < 150; y++)
        for (int x = 192; x < 278; x++)
            sandbox_set(w, x, y, EL_SOIL);
    /* the kettle: brick legs keep the floor cool, a heater warms a steel pot of water */
    for (int x = 146; x < 185; x++)
        sandbox_set(w, x, 150, EL_STONE);
    for (int y = 148; y < 150; y++) {
        sandbox_set(w, 152, y, EL_BRICK);
        sandbox_set(w, 153, y, EL_BRICK);
        sandbox_set(w, 176, y, EL_BRICK);
        sandbox_set(w, 177, y, EL_BRICK);
    }
    for (int x = 154; x < 176; x++)
        sandbox_set(w, x, 149, EL_HEATER);
    for (int x = 152; x < 178; x++)
        sandbox_set(w, x, 148, EL_STEEL);
    for (int y = 140; y < 148; y++) {
        sandbox_set(w, 152, y, EL_STEEL);
        sandbox_set(w, 177, y, EL_STEEL);
        for (int x = 153; x < 177; x++)
            if (y >= 141)
                sandbox_set(w, x, y, EL_WATER);
    }
    sandbox_set(w, 205, 145, EL_SEED);
    sandbox_set(w, 233, 145, EL_SEED);
    sandbox_set(w, 261, 145, EL_SEED);
}

void sbx_init(sbx_State *s, unsigned seed) {
    sandbox_init(&s->world, SBX_W, SBX_H, seed);
    s->element = EL_SAND;
    s->element2 = EL_EMPTY;
    s->brush = 3;
    s->paused = false;
    s->pal_hold = false;
    s->pal_hold2 = false;
    s->ticks = 0;
    build_scene(s);
}

bool sbx_over_panel(float px, float py) {
    (void)px;
    return py >= (float)(SBX_PAL_Y - SBX_PAL_PAD);
}

bool sbx_probe(const sbx_State *s, float px, float py, ElementId *id, int *temp_c) {
    if (px < 0.0f || py < 0.0f || sbx_over_panel(px, py))
        return false;
    int cx = (int)(px / SBX_CELL), cy = (int)(py / SBX_CELL);
    if (cx < 0 || cy < 0 || cx >= s->world.w || cy >= s->world.h)
        return false;
    *id = sandbox_get(&s->world, cx, cy);
    *temp_c = sandbox_temp(&s->world, cx, cy);
    return true;
}

int sbx_palette_slot_at(float px, float py) {
    float lx = px - (float)SBX_PAL_X, ly = py - (float)SBX_PAL_Y;
    if (lx < 0.0f || ly < 0.0f)
        return -1;
    int col = (int)(lx / (float)SBX_PAL_SLOT_W), row = (int)(ly / (float)SBX_PAL_SLOT_H);
    if (col >= SBX_PAL_COLS || row >= SBX_PAL_ROWS)
        return -1;
    int index = row * SBX_PAL_COLS + col;
    return index < sandbox_palette_count() ? index : -1;
}

void sbx_select(sbx_State *s, int palette_index) {
    if (palette_index >= 0 && palette_index < sandbox_palette_count())
        s->element = sandbox_palette_id(palette_index);
}

void sbx_select2(sbx_State *s, int palette_index) {
    if (palette_index >= 0 && palette_index < sandbox_palette_count())
        s->element2 = sandbox_palette_id(palette_index);
}

/* Move the selection by `steps` palette slots, wrapping around. */
static void cycle(sbx_State *s, int steps) {
    int n = sandbox_palette_count();
    int index = sandbox_palette_index(s->element);
    if (index < 0)
        index = 0;
    sbx_select(s, ((index + steps) % n + n) % n);
}

static void handle_keys(sbx_State *s, const ArcadeInput *in) {
    if (in->typed >= '0' && in->typed <= '9')
        sbx_select(s, in->typed - '0'); /* the first row of the palette */
    if (in->typed == ']')
        cycle(s, 1);
    if (in->typed == '[')
        cycle(s, -1);
    if (in->typed == 'x' || in->typed == 'X') { /* swap the two mouse buttons' elements */
        ElementId t = s->element;
        s->element = s->element2;
        s->element2 = t;
    }
    if (in->typed == 'c' || in->typed == 'C')
        sandbox_clear(&s->world);
    if (in->typed == 'r' || in->typed == 'R')
        build_scene(s);
    if (in->typed == 't' || in->typed == 'T')
        s->world.heat_view = !s->world.heat_view; /* thermal view: colours show temperature, not material */
    if (in->typed == '+' || in->typed == '=')
        s->brush = arc_clampi(s->brush + 1, BRUSH_MIN, BRUSH_MAX);
    if (in->typed == '-')
        s->brush = arc_clampi(s->brush - 1, BRUSH_MIN, BRUSH_MAX);
    if (in->wheel != 0)
        cycle(s, in->wheel);
    if (in->action_pressed)
        s->paused = !s->paused;
}

void sbx_step(sbx_State *s, const ArcadeInput *in, float dt) {
    (void)dt;
    handle_keys(s, in);
    bool on_panel = sbx_over_panel(in->aim_x, in->aim_y);
    if (in->fire_pressed && on_panel) {
        sbx_select(s, sbx_palette_slot_at(in->aim_x, in->aim_y));
        s->pal_hold = true;
    }
    if (in->alt_pressed && on_panel) {
        sbx_select2(s, sbx_palette_slot_at(in->aim_x, in->aim_y)); /* right-click a slot: second element */
        s->pal_hold2 = true;
    }
    if (!in->fire)
        s->pal_hold = false;
    if (!in->alt)
        s->pal_hold2 = false;
    int cx = (int)(in->aim_x / SBX_CELL), cy = (int)(in->aim_y / SBX_CELL);
    if (!on_panel) {
        if (in->fire && !s->pal_hold)
            sandbox_paint(&s->world, cx, cy, s->brush, s->element);
        if (in->alt && !s->pal_hold2)
            sandbox_paint(&s->world, cx, cy, s->brush, s->element2);
    }
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
