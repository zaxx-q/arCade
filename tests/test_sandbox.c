#include "arcade/sandbox.h"
#include "arcade_test.h"

static Sandbox g_sb; /* ~400 KB: keep off the stack */

static int iabs(int v) {
    return v < 0 ? -v : v;
}

static void run(int ticks) {
    for (int i = 0; i < ticks; i++)
        sandbox_step(&g_sb);
}

/* Fill the rectangle x0..x1, y0..y1 (inclusive) with one element. */
static void fill(int x0, int y0, int x1, int y1, ElementId id) {
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            sandbox_set(&g_sb, x, y, id);
}

/* Hold a torch over the rectangle for `ticks` ticks: flames are re-lit in empty cells every tick, the way a
 * player holds the mouse button down. */
static void torch(int x0, int y0, int x1, int y1, int ticks) {
    for (int t = 0; t < ticks; t++) {
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
                if (sandbox_get(&g_sb, x, y) == EL_EMPTY)
                    sandbox_set(&g_sb, x, y, EL_FIRE);
        sandbox_step(&g_sb);
    }
}

/* A fresh world with a stone floor on the bottom row and stone walls on both sides. */
static void tank(int w, int h, unsigned seed) {
    sandbox_init(&g_sb, w, h, seed);
    fill(0, h - 1, w - 1, h - 1, EL_STONE);
    fill(0, 0, 0, h - 1, EL_STONE);
    fill(w - 1, 0, w - 1, h - 1, EL_STONE);
}

/* Height of the highest cell of `id` in column x, counted from the floor row (0 = none). */
static int column_height(int x, ElementId id) {
    for (int y = 0; y < g_sb.h; y++)
        if (sandbox_get(&g_sb, x, y) == id)
            return g_sb.h - 1 - y;
    return 0;
}

static void test_grid_and_bounds(void) {
    sandbox_init(&g_sb, 1000, 1000, 1); /* clamped to the maximum size */
    CHECK_EQ_INT(g_sb.w, SANDBOX_MAX_W);
    CHECK_EQ_INT(g_sb.h, SANDBOX_MAX_H);
    sandbox_init(&g_sb, 10, 10, 1);
    CHECK_EQ_INT(sandbox_get(&g_sb, -1, 0), EL_STONE); /* outside = solid wall */
    CHECK_EQ_INT(sandbox_get(&g_sb, 10, 5), EL_STONE);
    sandbox_set(&g_sb, 99, 99, EL_SAND); /* silently ignored */
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_SAND), 0);
}

static void test_sand_falls_to_floor(void) {
    sandbox_init(&g_sb, 10, 10, 1);
    sandbox_set(&g_sb, 5, 0, EL_SAND);
    run(20);
    CHECK_EQ_INT(sandbox_get(&g_sb, 5, 9), EL_SAND);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_SAND), 1);
}

static void test_sand_makes_a_pile_and_conserves_mass(void) {
    sandbox_init(&g_sb, 21, 21, 2);
    for (int i = 0; i < 60; i++) {
        sandbox_set(&g_sb, 10, 0, EL_SAND);
        sandbox_step(&g_sb);
    }
    run(80);
    int n = sandbox_count(&g_sb, EL_SAND);
    CHECK(n >= 55); /* some spawns were skipped while the spawn cell was busy, but nothing vanished */
    int width = 0;
    for (int x = 0; x < 21; x++)
        if (sandbox_get(&g_sb, x, 20) == EL_SAND)
            width++;
    CHECK(width >= 5); /* it spread into a pile instead of one tall column */
}

static void test_stone_is_static_and_blocks(void) {
    sandbox_init(&g_sb, 10, 10, 1);
    for (int x = 0; x < 10; x++)
        sandbox_set(&g_sb, x, 5, EL_STONE);
    sandbox_set(&g_sb, 4, 0, EL_SAND);
    run(30);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_STONE), 10);
    CHECK_EQ_INT(sandbox_get(&g_sb, 4, 4), EL_SAND); /* rests on the shelf */
}

static void test_water_spreads_flat(void) {
    sandbox_init(&g_sb, 12, 6, 3);
    for (int i = 0; i < 12; i++)
        sandbox_set(&g_sb, 6, i % 2, EL_WATER);
    sandbox_paint(&g_sb, 6, 0, 1, EL_WATER);
    int before = sandbox_count(&g_sb, EL_WATER);
    run(120);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_WATER), before);
    int bottom = 0;
    for (int x = 0; x < 12; x++)
        if (sandbox_get(&g_sb, x, 5) == EL_WATER)
            bottom++;
    CHECK(bottom >= before - 1 || bottom >= 6); /* settles along the floor */
}

static void test_sand_sinks_through_water(void) {
    sandbox_init(&g_sb, 5, 8, 4);
    for (int y = 4; y < 8; y++)
        for (int x = 0; x < 5; x++)
            sandbox_set(&g_sb, x, y, EL_WATER);
    sandbox_set(&g_sb, 2, 0, EL_SAND);
    run(60);
    CHECK_EQ_INT(sandbox_get(&g_sb, 2, 7), EL_SAND);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_WATER), 20); /* water was displaced, not destroyed */
}

static void test_paint_and_erase(void) {
    sandbox_init(&g_sb, 20, 20, 1);
    sandbox_paint(&g_sb, 10, 10, 3, EL_STONE);
    int stone = sandbox_count(&g_sb, EL_STONE);
    CHECK(stone > 10);
    sandbox_paint(&g_sb, 10, 10, 3, EL_SAND); /* does not overwrite stone */
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_SAND), 0);
    sandbox_paint(&g_sb, 10, 10, 5, EL_EMPTY); /* eraser removes everything in the brush */
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_STONE), 0);
    sandbox_paint(&g_sb, -50, -50, 4, EL_SAND); /* far outside: ignored */
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_SAND), 0);
}

static void test_count_in_rectangle(void) {
    sandbox_init(&g_sb, 10, 10, 1);
    for (int x = 0; x < 10; x++)
        sandbox_set(&g_sb, x, 2, EL_SAND);
    CHECK_EQ_INT(sandbox_count_in(&g_sb, 0, 0, 10, 10, EL_SAND), 10); /* whole grid = sandbox_count */
    CHECK_EQ_INT(sandbox_count_in(&g_sb, 2, 2, 3, 1, EL_SAND), 3);    /* only the cells inside the box */
    CHECK_EQ_INT(sandbox_count_in(&g_sb, 2, 3, 3, 5, EL_SAND), 0);    /* box below the row */
    CHECK_EQ_INT(sandbox_count_in(&g_sb, -5, -5, 8, 8, EL_SAND), 3); /* clipped to the grid: x 0..2, y 0..2 */
    CHECK_EQ_INT(sandbox_count_in(&g_sb, 8, 0, 50, 50, EL_SAND), 2); /* clipped on the far side too */
    CHECK_EQ_INT(sandbox_count_in(&g_sb, 0, 0, 0, 5, EL_SAND), 0);   /* empty box */
    CHECK_EQ_INT(sandbox_count_in(&g_sb, 20, 20, 5, 5, EL_SAND), 0); /* entirely outside */
}

/* The hub's vault relies on this: water poured on a stone basin ends up inside it and fills it up. */
static void test_water_fills_a_stone_basin(void) {
    sandbox_init(&g_sb, 30, 30, 5);
    for (int y = 20; y < 26; y++) { /* U-shaped basin: interior x 11..18 (8 wide), y 20..24 (5 tall) */
        sandbox_set(&g_sb, 10, y, EL_STONE);
        sandbox_set(&g_sb, 19, y, EL_STONE);
    }
    for (int x = 10; x < 20; x++)
        sandbox_set(&g_sb, x, 25, EL_STONE);
    for (int i = 0; i < 40; i++) { /* a stream of water from above the opening */
        sandbox_paint(&g_sb, 14, 4, 2, EL_WATER);
        sandbox_step(&g_sb);
    }
    run(200);
    int inside = sandbox_count_in(&g_sb, 11, 20, 8, 5, EL_WATER);
    CHECK_EQ_INT(inside, 8 * 5); /* completely full: the surplus ran over the rim and out */
}

static void test_element_table(void) {
    for (int i = 0; i < EL_COUNT; i++)
        CHECK(sandbox_element((ElementId)i)->name != NULL);
    CHECK(sandbox_element((ElementId)999) == sandbox_element(EL_EMPTY));
    CHECK(sandbox_element(EL_SAND)->density > sandbox_element(EL_WATER)->density);
}

static void test_deterministic(void) {
    static Sandbox other;
    sandbox_init(&g_sb, 30, 30, 77);
    sandbox_init(&other, 30, 30, 77);
    for (int i = 0; i < 100; i++) {
        sandbox_paint(&g_sb, 15, 2, 2, i % 3 ? EL_SAND : EL_WATER);
        sandbox_paint(&other, 15, 2, 2, i % 3 ? EL_SAND : EL_WATER);
        sandbox_step(&g_sb);
        sandbox_step(&other);
    }
    for (int i = 0; i < 30 * 30; i++)
        CHECK_EQ_INT(g_sb.cells[i], other.cells[i]);
}

/* ---- palette and colours ---- */

static void test_palette(void) {
    int n = sandbox_palette_count();
    CHECK(n >= 20 && n < EL_COUNT);
    CHECK_EQ_INT(sandbox_palette_id(0), EL_EMPTY); /* the number keys 0-3 keep their old meaning */
    CHECK_EQ_INT(sandbox_palette_id(1), EL_SAND);
    CHECK_EQ_INT(sandbox_palette_id(2), EL_WATER);
    CHECK_EQ_INT(sandbox_palette_id(3), EL_STONE);
    for (int i = 0; i < n; i++) {
        ElementId id = sandbox_palette_id(i);
        CHECK(!(sandbox_element(id)->flags & EF_HIDDEN));
        CHECK_EQ_INT(sandbox_palette_index(id), i); /* index and id are inverse of each other */
    }
    CHECK_EQ_INT(sandbox_palette_index(EL_GAMMA), -1); /* hidden elements are not in the palette */
    CHECK_EQ_INT(sandbox_palette_index(EL_MOLTEN_URANIUM), -1);
    CHECK_EQ_INT(sandbox_palette_index(EL_SPARK), -1);
    CHECK_EQ_INT(sandbox_palette_index(EL_BASALT), -1);
    CHECK_EQ_INT(sandbox_palette_id(n), EL_EMPTY); /* out of range is harmless */
    CHECK_EQ_INT(sandbox_palette_index((ElementId)999), -1);
}

static void test_colours_are_distinct_and_stable(void) {
    sandbox_init(&g_sb, 8, 8, 1);
    uint32_t bg = sandbox_pixel(&g_sb, 3, 3);
    CHECK_EQ_INT(bg, 0x080A16); /* an empty cell is the dark background, same as before */
    CHECK_EQ_INT(sandbox_pixel(&g_sb, -1, 0), 0);
    for (int i = 1; i < sandbox_palette_count(); i++) { /* every element is visible against the background */
        sandbox_set(&g_sb, 3, 3, sandbox_palette_id(i));
        uint32_t c = sandbox_pixel(&g_sb, 3, 3);
        CHECK(c != bg);
        CHECK_EQ_INT(sandbox_pixel(&g_sb, 3, 3), c); /* pure: asking twice gives the same colour */
    }
    /* the new roster looks different from the original palette: sand keeps its colour family */
    sandbox_set(&g_sb, 3, 3, EL_SAND);
    uint32_t sand = sandbox_pixel(&g_sb, 3, 3);
    CHECK(((sand >> 16) & 255u) > 200u && (sand & 255u) < 140u);
}

static void test_every_element_has_a_row(void) {
    for (int i = 0; i < EL_COUNT; i++) {
        const ElementDef *d = sandbox_element((ElementId)i);
        CHECK(d->name != NULL && d->name[0] != 0);
        if (i != EL_EMPTY)
            CHECK(d->r + d->g + d->b > 0);
        if (d->kind == MOVE_POWDER || d->kind == MOVE_LIQUID)
            CHECK(d->speed > 0);
        if (d->kind == MOVE_LIQUID)
            CHECK(d->slip > 0 && d->spread > 0);
    }
}

static void test_brush_spray_and_hidden_cells(void) {
    sandbox_init(&g_sb, 30, 30, 1);
    sandbox_paint(&g_sb, 15, 15, 6, EL_SAND);
    int full = sandbox_count(&g_sb, EL_SAND);
    sandbox_init(&g_sb, 30, 30, 1);
    sandbox_paint(&g_sb, 15, 15, 6, EL_SNOW); /* snow is sprinkled: fewer cells than a solid brush */
    int sprinkled = sandbox_count(&g_sb, EL_SNOW);
    CHECK(sprinkled > 10 && sprinkled < full * 9 / 10);
    sandbox_set(&g_sb, 2, 2, EL_SAND);
    sandbox_set(&g_sb, 2, 2, (ElementId)EL_COUNT); /* an invalid id is ignored */
    CHECK_EQ_INT(sandbox_get(&g_sb, 2, 2), EL_SAND);
}

static void test_set_clears_cell_memory(void) {
    sandbox_init(&g_sb, 10, 10, 1);
    sandbox_set(&g_sb, 5, 5, EL_URANIUM);
    g_sb.aux[5 * 10 + 5] = 200;
    sandbox_set(&g_sb, 5, 5, EL_URANIUM); /* painting again starts from a cool cell */
    CHECK_EQ_INT(g_sb.aux[5 * 10 + 5], 0);
}

/* ---- movement ---- */

static void test_sand_falls_several_cells_per_tick(void) {
    sandbox_init(&g_sb, 10, 60, 1);
    sandbox_set(&g_sb, 5, 0, EL_SAND);
    run(20); /* 59 rows to fall: at 1 cell per tick this would take 59 ticks */
    CHECK_EQ_INT(sandbox_get(&g_sb, 5, 59), EL_SAND);
}

static void test_a_big_dump_splashes_wider_than_a_trickle(void) {
    tank(80, 50, 3);
    fill(30, 10, 46, 24, EL_SAND); /* a 17x15 block dropped in one go */
    run(200);
    int blob_width = 0;
    for (int x = 1; x < 79; x++)
        if (column_height(x, EL_SAND) > 0)
            blob_width++;
    int blob_height = 0;
    for (int x = 1; x < 79; x++)
        blob_height = arc_maxi(blob_height, column_height(x, EL_SAND));
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_SAND), 17 * 15);
    CHECK(blob_width >= 30);             /* it spread out... */
    CHECK(blob_height * 2 < blob_width); /* ...into a heap flatter than a 45 degree cone */
}

static void test_a_trickle_makes_a_steep_cone(void) {
    tank(60, 40, 4);
    for (int i = 0; i < 150; i++) {
        sandbox_set(&g_sb, 30, 2, EL_SAND);
        run(2);
    }
    run(100);
    int width = 0;
    for (int x = 1; x < 59; x++)
        if (column_height(x, EL_SAND) > 0)
            width++;
    int height = column_height(30, EL_SAND);
    CHECK(height * 2 >= width * 3 / 4); /* about 40 degrees or more: a clean cone, not a pancake */
}

static void test_water_levels_out_quickly(void) {
    tank(64, 30, 5);
    fill(1, 12, 12, 28, EL_WATER); /* a tall column on the left of a wide tank */
    int total = sandbox_count(&g_sb, EL_WATER);
    run(60);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_WATER), total);
    int lo = 99, hi = 0;
    for (int x = 1; x < 63; x++) {
        int h = column_height(x, EL_WATER);
        lo = arc_mini(lo, h);
        hi = arc_maxi(hi, h);
    }
    CHECK(hi - lo <= 1); /* flat to within one cell after only 60 ticks */
}

static void test_a_settled_pool_stays_still(void) {
    tank(40, 20, 6);
    fill(1, 12, 38, 18, EL_WATER);
    run(100);
    static uint8_t before[40 * 20];
    memcpy(before, g_sb.cells, sizeof(before));
    run(50);
    CHECK(memcmp(before, g_sb.cells, sizeof(before)) == 0); /* no shimmering: a calm pool is stable */
}

static void test_water_flows_off_a_ledge(void) {
    tank(40, 30, 7);
    fill(1, 15, 20, 15, EL_STONE); /* a ledge */
    for (int i = 0; i < 20; i++) {
        sandbox_set(&g_sb, 5, 14, EL_WATER);
        run(3);
    }
    run(100);
    CHECK_EQ_INT(sandbox_count_in(&g_sb, 1, 0, 20, 15, EL_WATER),
                 0);                                  /* nothing is left sitting on the ledge */
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_WATER), 20); /* it all ran off and pooled below */
}

static void test_liquids_layer_by_density(void) {
    tank(24, 40, 8);
    for (int y = 2; y < 30; y++)
        for (int x = 1; x < 23; x++) {
            ElementId id = (x * 7 + y * 3) % 3 == 0 ? EL_OIL : (x + y) % 2 ? EL_WATER : EL_MERCURY;
            sandbox_set(&g_sb, x, y, id);
        }
    int oil = sandbox_count(&g_sb, EL_OIL), water = sandbox_count(&g_sb, EL_WATER);
    int mercury = sandbox_count(&g_sb, EL_MERCURY);
    run(400);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_OIL), oil);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_WATER), water);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_MERCURY), mercury);
    /* going down a column, density never decreases: oil, then water, then mercury */
    int wrong = 0, pairs = 0;
    for (int x = 1; x < 23; x++) {
        int last = -1;
        for (int y = 0; y < 39; y++) {
            ElementId id = sandbox_get(&g_sb, x, y);
            int rank = id == EL_OIL ? 0 : id == EL_WATER ? 1 : id == EL_MERCURY ? 2 : -1;
            if (rank < 0)
                continue;
            if (last >= 0) {
                pairs++;
                if (rank < last)
                    wrong++;
            }
            last = rank;
        }
    }
    CHECK(wrong * 20 <= pairs); /* at least 95% of neighbouring pairs are in the right order */
}

static void test_light_powder_floats_on_water(void) {
    tank(12, 20, 9);
    fill(1, 10, 10, 18, EL_WATER);
    sandbox_set(&g_sb, 5, 2, EL_ASH);
    run(60);
    CHECK_EQ_INT(sandbox_get(&g_sb, 5, 9), EL_ASH); /* ash is lighter than water: it rests on the surface */
}

static void test_gas_rises_and_stays_conserved_when_sealed(void) {
    tank(20, 30, 10);
    fill(0, 0, 19, 0, EL_STONE); /* a lid */
    fill(5, 20, 14, 27, EL_METHANE);
    int total = sandbox_count(&g_sb, EL_METHANE);
    run(60);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_METHANE), total); /* methane does not decay or leak */
    CHECK_EQ_INT(sandbox_count_in(&g_sb, 1, 1, 18, 10, EL_METHANE), total); /* all of it is in the top part */
}

static void test_heavy_things_sink_through_light_fluids(void) {
    tank(10, 40, 11);
    fill(1, 20, 8, 38, EL_OIL);
    sandbox_set(&g_sb, 4, 2, EL_STONE); /* stone is not a fluid: it stays where it is */
    sandbox_set(&g_sb, 6, 2, EL_SAND);
    run(80);
    CHECK_EQ_INT(sandbox_get(&g_sb, 6, 38), EL_SAND); /* sand sinks to the bottom of oil */
}

/* ---- fire and explosives ---- */

static int burn_wood_and_count_ash(unsigned seed) {
    tank(30, 20, seed);
    fill(8, 12, 22, 18, EL_WOOD);
    fill(9, 11, 21, 11, EL_FIRE); /* a line of flames on top */
    run(1100);                    /* the last embers take a while to go out */
    return sandbox_count(&g_sb, EL_ASH);
}

static void test_fire_burns_wood_to_ash(void) {
    int ash = burn_wood_and_count_ash(12);
    CHECK(ash > 10);
    CHECK(sandbox_count(&g_sb, EL_WOOD) < 15 * 7 / 2); /* most of the block is gone */
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_FIRE), 0);    /* and the fire went out when the fuel ran out */
}

static void test_fire_dies_without_fuel(void) {
    tank(20, 20, 13);
    sandbox_set(&g_sb, 10, 10, EL_FIRE);
    run(80);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_FIRE), 0);
}

static void test_water_puts_out_fire(void) {
    int fire[2];
    for (int round = 0; round < 2; round++) {
        tank(60, 20, 14);
        if (round == 1)
            fill(1, 15, 58, 18, EL_WATER);
        fill(2, 14, 57, 14, EL_FIRE); /* a long row of flames resting on the floor or on the water */
        run(2);
        fire[round] = sandbox_count(&g_sb, EL_FIRE);
    }
    CHECK(fire[0] >= 50);             /* over bare floor the flames are still there... */
    CHECK(fire[1] * 3 < fire[0] * 2); /* ...over water a good part of them is put out straight away */
}

static void test_oil_burns_and_keeps_flowing(void) {
    tank(30, 20, 15);
    fill(1, 14, 28, 18, EL_OIL);
    sandbox_set(&g_sb, 2, 13, EL_FIRE);
    sandbox_set(&g_sb, 2, 12, EL_FIRE);
    run(400);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_OIL), 0); /* a burning slick burns itself out completely */
}

static void test_gunpowder_goes_off_all_at_once_as_sparks(void) {
    tank(60, 30, 16);
    fill(5, 25, 54, 28, EL_GUNPOWDER);
    fill(2, 20, 2, 28, EL_WALL); /* a wall the sparks cannot get through */
    sandbox_set(&g_sb, 30, 24, EL_FIRE);
    run(2);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_GUNPOWDER), 0); /* the whole heap went at once, not cell by cell */
    CHECK(sandbox_count(&g_sb, EL_SPARK) > 100);
    run(200);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_SPARK), 0); /* the sparks burn out */
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_WALL), 9);
}

static void test_fuse_burns_slowly_along_its_length(void) {
    tank(60, 20, 17);
    fill(5, 10, 54, 10, EL_FUSE);
    sandbox_set(&g_sb, 4, 10, EL_FIRE);
    run(40);
    int left = sandbox_count(&g_sb, EL_FUSE);
    CHECK(left > 30); /* after 40 ticks only the first few cells are gone: a fuse is slow... */
    CHECK(left < 50); /* ...but it is burning */
    run(500);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_FUSE), 0); /* and it reaches the end */
}

static void test_tnt_explodes_and_walls_survive(void) {
    tank(60, 40, 18);
    fill(25, 30, 34, 38, EL_STONE);
    fill(26, 24, 33, 29, EL_TNT);
    fill(20, 38, 40, 38, EL_WALL); /* an unbreakable strip right under the blast */
    int stone = sandbox_count(&g_sb, EL_STONE);
    torch(24, 24, 25, 29, 10);
    run(200);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_TNT), 0);
    CHECK(sandbox_count(&g_sb, EL_STONE) < stone - 5); /* the sparks drilled into the stone */
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_WALL), 21);   /* walls stop them dead */
}

static void test_tnt_chain_reaction(void) {
    tank(70, 30, 19);
    for (int x = 5; x < 65; x += 8)
        fill(x, 24, x + 2, 26, EL_TNT); /* crates 5 cells apart: sparks from one reach the next */
    torch(3, 24, 4, 26, 10);
    run(300);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_TNT), 0);
}

static void test_tnt_makes_a_real_explosion(void) {
    tank(160, 70, 64);
    fill(1, 60, 158, 68, EL_SAND);
    fill(110, 40, 112, 59, EL_WOOD); /* a fence 30 cells from the charge */
    fill(60, 52, 69, 59, EL_TNT);
    int wood = sandbox_count(&g_sb, EL_WOOD);
    int sand = sandbox_count(&g_sb, EL_SAND);
    sandbox_set(&g_sb, 64, 51, EL_FIRE);
    run(2);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_TNT), 0);
    /* one spark per cell of the charge, some already bumped out */
    CHECK(sandbox_count(&g_sb, EL_SPARK) >= 25);
    run(300);
    CHECK(sandbox_count(&g_sb, EL_SAND) < sand); /* the surroundings were dug up... */
    CHECK(sandbox_count(&g_sb, EL_WOOD) < wood); /* ...the fence was hit by sparks and set alight... */
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_SPARK), 0);
}

static void test_one_tnt_cell_still_goes_off(void) {
    tank(100, 50, 65);
    fill(1, 40, 98, 48, EL_SAND);
    sandbox_set(&g_sb, 50, 45, EL_TNT);
    int before = sandbox_count(&g_sb, EL_SAND);
    for (int t = 0; t < 10; t++) { /* a few flames held against it, like a brush stroke */
        sandbox_set(&g_sb, 50, 44, EL_FIRE);
        sandbox_set(&g_sb, 49, 45, EL_FIRE);
        sandbox_step(&g_sb);
    }
    run(120);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_TNT), 0);
    CHECK(sandbox_count(&g_sb, EL_SAND) < before); /* the one spark dug at least a little */
}

static void test_methane_burns(void) {
    tank(30, 20, 20);
    fill(1, 5, 28, 18, EL_METHANE);
    sandbox_set(&g_sb, 15, 10, EL_FIRE);
    run(150);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_METHANE), 0);
}

/* ---- heat, cold and liquids that react ---- */

static void test_lava_meets_water(void) {
    tank(30, 20, 21);
    fill(1, 14, 28, 18, EL_WATER);
    fill(10, 6, 18, 9, EL_LAVA);
    run(300);
    CHECK(sandbox_count(&g_sb, EL_BASALT) > 15); /* lava that meets water sets into rock at once */
    CHECK(sandbox_count(&g_sb, EL_WATER) > 0);   /* some water boiled away, not all of it */
}

static void test_lava_turns_sand_to_glass(void) {
    tank(30, 12, 22);
    fill(2, 6, 10, 10, EL_LAVA);
    fill(11, 6, 20, 10, EL_SAND);
    run(300);
    CHECK(sandbox_count(&g_sb, EL_GLASS) > 0);
}

static void test_thermite_burns_through_stone(void) {
    tank(30, 30, 23);
    fill(5, 20, 24, 28, EL_STONE);
    fill(12, 14, 16, 19, EL_THERMITE);
    fill(11, 13, 17, 13, EL_FIRE);
    int stone = sandbox_count(&g_sb, EL_STONE);
    for (int i = 0; i < 12; i++) {
        fill(11, 13, 17, 13, EL_FIRE);
        run(1);
    }
    run(200);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_THERMITE), 0);
    CHECK(sandbox_count(&g_sb, EL_STONE) < stone - 5); /* it melted a pocket into the stone */
}

static void test_acid_dissolves_stone_but_not_glass_or_walls(void) {
    tank(30, 30, 24);
    fill(5, 20, 24, 28, EL_STONE);
    fill(5, 12, 24, 12, EL_GLASS);
    fill(5, 13, 24, 13, EL_WALL);
    fill(6, 14, 23, 19, EL_ACID);
    int stone = sandbox_count(&g_sb, EL_STONE);
    run(400);
    CHECK(sandbox_count(&g_sb, EL_STONE) < stone - 10);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_GLASS), 20);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_WALL), 20);
}

static void test_nitrogen_freezes_water(void) {
    tank(20, 20, 25);
    fill(1, 12, 18, 18, EL_WATER);
    fill(5, 6, 14, 9, EL_NITROGEN);
    run(60);
    CHECK(sandbox_count(&g_sb, EL_ICE) >= 5);
}

static void test_heat_melts_ice_and_snow(void) {
    tank(20, 20, 26);
    fill(2, 16, 17, 18, EL_ICE);
    fill(2, 14, 17, 15, EL_SNOW);
    fill(2, 13, 17, 13, EL_FIRE);
    for (int i = 0; i < 40; i++) {
        fill(2, 13, 17, 13, EL_FIRE);
        run(1);
    }
    run(300);
    CHECK(sandbox_count(&g_sb, EL_WATER) > 10);
}

static void test_steam_condenses_back_to_water(void) {
    tank(20, 30, 27);
    fill(0, 0, 19, 0, EL_STONE);
    fill(5, 24, 14, 28, EL_STEAM);
    run(600);
    CHECK(sandbox_count(&g_sb, EL_WATER) > 0);
}

static void test_smoke_fades(void) {
    tank(20, 20, 28);
    fill(5, 8, 14, 12, EL_SMOKE);
    run(1500);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_SMOKE), 0);
}

/* ---- living things ---- */

static void test_seed_on_soil_grows_and_seed_elsewhere_does_not(void) {
    tank(40, 30, 29);
    fill(1, 26, 38, 28, EL_SOIL);
    sandbox_set(&g_sb, 10, 25, EL_SEED);
    run(300);
    CHECK(sandbox_count(&g_sb, EL_PLANT) >= 4); /* a sprout and a few stems */
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_SEED), 0);
    tank(40, 30, 29);
    sandbox_set(&g_sb, 10, 2, EL_SEED);
    run(300);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_PLANT), 0); /* bare floor: nothing grows */
}

static void test_plants_drink_water_and_grow_bigger(void) {
    int count[2];
    for (int round = 0; round < 2; round++) {
        tank(40, 40, 30);
        fill(1, 36, 38, 38, EL_SOIL);
        sandbox_set(&g_sb, 20, 35, EL_SEED);
        run(300);
        for (int t = 0; t < 1200; t++) {
            if (round == 1 && t % 20 == 0)
                sandbox_paint(&g_sb, 20, 20, 2, EL_WATER); /* rain on the garden */
            sandbox_step(&g_sb);
        }
        count[round] = sandbox_count(&g_sb, EL_PLANT);
    }
    CHECK(count[1] > count[0] * 3); /* a watered plant grows several times bigger than a dry one */
}

static void test_plants_burn(void) {
    tank(30, 30, 31);
    fill(5, 20, 24, 28, EL_PLANT);
    fill(5, 19, 24, 19, EL_FIRE);
    run(400);
    CHECK(sandbox_count(&g_sb, EL_PLANT) < 20);
}

static void test_termites_eat_wood_and_starve(void) {
    tank(40, 20, 32);
    fill(10, 15, 30, 18, EL_WOOD);
    sandbox_set(&g_sb, 9, 18, EL_TERMITE);
    sandbox_set(&g_sb, 9, 17, EL_TERMITE);
    sandbox_set(&g_sb, 9, 16, EL_TERMITE);
    int wood = sandbox_count(&g_sb, EL_WOOD);
    run(3000);
    CHECK(sandbox_count(&g_sb, EL_WOOD) < wood / 2);
    CHECK(sandbox_count(&g_sb, EL_TERMITE) >= 3); /* well fed: they multiplied while there was wood */
}

static void test_termites_starve_without_food(void) {
    tank(30, 12, 33);
    sandbox_set(&g_sb, 15, 10, EL_TERMITE);
    run(8000);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_TERMITE), 0);
}

static void test_life_blinker_and_glider(void) {
    sandbox_init(&g_sb, 30, 30, 34);
    fill(10, 5, 12, 5, EL_LIFE); /* blinker */
    run(6);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_LIFE), 3);
    CHECK_EQ_INT(sandbox_get(&g_sb, 11, 4), EL_LIFE); /* it turned vertical after one generation... */
    CHECK_EQ_INT(sandbox_get(&g_sb, 10, 5), EL_EMPTY);
    run(6);
    CHECK_EQ_INT(sandbox_get(&g_sb, 10, 5), EL_LIFE); /* ...and horizontal again after two */
    sandbox_clear(&g_sb);
    sandbox_set(&g_sb, 5, 15, EL_LIFE); /* glider: moves one cell diagonally every 4 generations */
    sandbox_set(&g_sb, 6, 16, EL_LIFE);
    sandbox_set(&g_sb, 4, 17, EL_LIFE);
    sandbox_set(&g_sb, 5, 17, EL_LIFE);
    sandbox_set(&g_sb, 6, 17, EL_LIFE);
    run(24 * 3);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_LIFE), 5);
    CHECK_EQ_INT(sandbox_get(&g_sb, 8, 18), EL_LIFE); /* (5,15) -> (8,18) */
}

/* ---- uranium ---- */

/* The first tick (counting from 1) on which `id` shows up in the world, within `limit` ticks; 0 = never. */
static int first_tick_with(ElementId id, int limit) {
    for (int t = 1; t <= limit; t++) {
        sandbox_step(&g_sb);
        if (sandbox_count(&g_sb, id) > 0)
            return t;
    }
    return 0;
}

static void test_uranium_gamma_rays_stop_at_walls(void) {
    sandbox_init(&g_sb, 60, 20, 35);
    fill(30, 0, 30, 19, EL_WALL);
    fill(5, 8, 7, 11, EL_URANIUM);
    for (int y = 8; y <= 11; y++)
        for (int x = 5; x <= 7; x++)
            sandbox_set_temp(&g_sb, x, y, 1300); /* white hot: the whole lump goes critical */
    for (int t = 0; t < 120; t++) {
        sandbox_step(&g_sb);
        for (int y = 0; y < 20; y++)
            for (int x = 31; x < 60; x++)
                CHECK(sandbox_get(&g_sb, x, y) != EL_GAMMA); /* nothing got through the wall */
    }
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_URANIUM), 0);
}

static void test_gamma_rays_burn_through_what_they_cross(void) {
    sandbox_init(&g_sb, 80, 30, 45);
    fill(0, 29, 79, 29, EL_WALL);
    fill(30, 5, 59, 28, EL_SAND);
    fill(40, 20, 59, 28, EL_WATER);
    fill(40, 5, 59, 19, EL_SAND);
    fill(5, 22, 8, 28, EL_URANIUM);
    for (int y = 22; y <= 28; y++)
        for (int x = 5; x <= 8; x++)
            sandbox_set_temp(&g_sb, x, y, 1300);
    int steam = 0;
    for (int t = 0; t < 120; t++) {
        sandbox_step(&g_sb);
        steam = arc_maxi(steam, sandbox_count(&g_sb, EL_STEAM));
    }
    CHECK(sandbox_count(&g_sb, EL_GLASS) > 20);                   /* sand is fused into glass */
    CHECK(sandbox_count(&g_sb, EL_SAND) < 30 * 24 - 20 * 9 - 20); /* ...or simply burnt away */
    CHECK(steam > 20);                                            /* water is boiled off */
}

static void test_uranium_heats_itself_in_proportion_to_its_size(void) {
    tank(60, 40, 38);
    fill(10, 38, 10, 38, EL_URANIUM); /* a single cell loses its heat as fast as it makes it... */
    fill(30, 33, 37, 38, EL_URANIUM); /* ...a big lump cannot, its middle just keeps getting hotter */
    run(200);
    CHECK(sandbox_temp(&g_sb, 10, 38) < 300);
    CHECK(sandbox_temp(&g_sb, 33, 35) > 600);
}

static void test_small_uranium_lump_is_stable(void) {
    tank(50, 40, 37);
    fill(22, 30, 24, 32, EL_URANIUM); /* 3 x 3 */
    run(2500);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_URANIUM), 9); /* never melts, never goes off */
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_GAMMA), 0);
}

static void test_big_uranium_lump_melts_then_goes_critical_and_wrecks_the_world(void) {
    tank(200, 80, 36);
    fill(90, 67, 101, 78, EL_URANIUM); /* 12 x 12: far above critical size */
    fill(20, 70, 40, 78, EL_SAND);
    int floor = 198;
    for (int x = 1; x < 199; x++)
        CHECK_EQ_INT(sandbox_get(&g_sb, x, 79), EL_STONE);
    int melted = first_tick_with(EL_MOLTEN_URANIUM, 2000);
    CHECK(melted > 0); /* it heats itself until it melts... */
    int critical = melted + first_tick_with(EL_GAMMA, 2000);
    CHECK(critical > melted + 10); /* ...and only some time after that does it go critical */
    run(600);
    int uranium = sandbox_count(&g_sb, EL_URANIUM) + sandbox_count(&g_sb, EL_MOLTEN_URANIUM);
    CHECK(uranium <= 3); /* all of it went up */
    int left = 0;
    for (int x = 1; x < 199; x++)
        left += sandbox_get(&g_sb, x, 79) == EL_STONE;
    CHECK(left < floor * 9 / 10);                  /* the rays tore up the floor */
    CHECK(sandbox_count(&g_sb, EL_SAND) < 21 * 9); /* and burnt the sand into glass and gravel */
    CHECK(sandbox_count(&g_sb, EL_GLASS) > 0);
}

static void test_one_critical_cell_takes_the_whole_lump(void) {
    tank(120, 60, 41);
    fill(50, 49, 59, 58, EL_URANIUM); /* 10 x 10: calm for a while at room temperature... */
    run(30);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_URANIUM), 100);
    sandbox_set_temp(&g_sb, 55, 54, 1300); /* ...but one hot cell is enough: its ray sets off the rest */
    run(60);
    CHECK(sandbox_count(&g_sb, EL_URANIUM) <= 3);
}

static void test_a_lump_too_small_to_blow_only_melts(void) {
    tank(50, 40, 42);
    fill(22, 30, 24, 32, EL_URANIUM); /* 9 cells */
    for (int y = 30; y <= 32; y++)
        for (int x = 22; x <= 24; x++)
            sandbox_set_temp(&g_sb, x, y, 1100); /* hot enough to melt, not hot enough to go critical */
    int gamma = 0;
    for (int t = 0; t < 60; t++) {
        sandbox_step(&g_sb);
        gamma += sandbox_count(&g_sb, EL_GAMMA);
    }
    CHECK(sandbox_count(&g_sb, EL_MOLTEN_URANIUM) > 0);
    CHECK_EQ_INT(gamma, 0);
}

static void test_big_explosions_do_not_flash_the_screen(void) {
    tank(120, 60, 44);
    fill(100, 40, 115, 55, EL_WALL); /* a sealed pocket in a block of Wall: nothing can get in */
    fill(105, 45, 110, 50, EL_EMPTY);
    uint32_t before = sandbox_pixel(&g_sb, 107, 47);
    fill(10, 38, 29, 58, EL_URANIUM); /* a big lump, every cell hot enough to go critical at once... */
    for (int y = 38; y <= 58; y++)
        for (int x = 10; x <= 29; x++)
            sandbox_set_temp(&g_sb, x, y, 1300);
    fill(50, 50, 63, 58, EL_TNT); /* ...and a charge of 126 cells next to it */
    sandbox_set_temp(&g_sb, 56, 54, 600);
    for (int t = 0; t < 120; t++) {
        sandbox_step(&g_sb);
        CHECK_EQ_INT(sandbox_pixel(&g_sb, 107, 47), before); /* the screen never whites out */
    }
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_TNT), 0); /* both really did go off */
    CHECK(sandbox_count(&g_sb, EL_URANIUM) + sandbox_count(&g_sb, EL_MOLTEN_URANIUM) <= 3);
}

static void test_water_keeps_a_small_lump_cool_but_not_a_big_one(void) {
    tank(80, 50, 43);
    fill(1, 20, 78, 48, EL_WATER);
    fill(20, 45, 25, 48, EL_URANIUM); /* 6 x 4 under water: cooled by it, however long we wait */
    run(1500);
    CHECK(sandbox_count(&g_sb, EL_URANIUM) + sandbox_count(&g_sb, EL_MOLTEN_URANIUM) >= 20);
    tank(80, 50, 43);
    fill(1, 20, 78, 48, EL_WATER);
    fill(30, 39, 39, 48, EL_URANIUM); /* 10 x 10: too much even for water */
    run(1500);
    CHECK(sandbox_count(&g_sb, EL_URANIUM) + sandbox_count(&g_sb, EL_MOLTEN_URANIUM) <= 3);
}

static void test_a_bigger_lump_makes_a_bigger_blast(void) {
    int left[2];
    int sizes[2] = {6, 14};
    for (int round = 0; round < 2; round++) {
        tank(200, 80, 44);
        int n = sizes[round];
        fill(10, 40, 190, 78, EL_SAND);
        fill(100, 79 - n, 99 + n, 78, EL_URANIUM); /* the sand bed goes around the lump */
        sandbox_set_temp(&g_sb, 100, 78, 1300);
        run(400);
        left[round] = sandbox_count(&g_sb, EL_SAND);
    }
    CHECK(left[1] < left[0]); /* the larger lump removed more of the bed */
}

static void test_molten_uranium_solidifies_in_water(void) {
    tank(20, 20, 39);
    fill(1, 12, 18, 18, EL_WATER);
    sandbox_set(&g_sb, 10, 5, EL_MOLTEN_URANIUM);
    run(100);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_MOLTEN_URANIUM), 0);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_URANIUM), 1);
}

/* ---- temperature ---- */

static void test_every_cell_starts_at_room_temperature(void) {
    sandbox_init(&g_sb, 20, 20, 50);
    CHECK_EQ_INT(sandbox_temp(&g_sb, 5, 5), SANDBOX_AMBIENT);
    sandbox_set(&g_sb, 5, 5, EL_STEEL);
    CHECK_EQ_INT(sandbox_temp(&g_sb, 5, 5), SANDBOX_AMBIENT);
    CHECK_EQ_INT(sandbox_temp(&g_sb, -3, 40), SANDBOX_AMBIENT); /* outside the grid: room temperature */
    sandbox_set_temp(&g_sb, 5, 5, 500);
    CHECK_EQ_INT(sandbox_temp(&g_sb, 5, 5), 500);
    sandbox_set_temp(&g_sb, 5, 5, 99999);
    CHECK_EQ_INT(sandbox_temp(&g_sb, 5, 5), SANDBOX_TEMP_MAX); /* clamped */
    sandbox_set_temp(&g_sb, 5, 5, -9999);
    CHECK_EQ_INT(sandbox_temp(&g_sb, 5, 5), SANDBOX_TEMP_MIN);
}

static void test_heat_sources_and_cold_sources_have_their_own_temperature(void) {
    tank(20, 20, 51);
    sandbox_set(&g_sb, 5, 5, EL_HEATER);
    sandbox_set(&g_sb, 10, 5, EL_COOLER);
    sandbox_set(&g_sb, 15, 18, EL_LAVA);
    run(5);
    CHECK(sandbox_temp(&g_sb, 5, 5) >= 600);
    CHECK(sandbox_temp(&g_sb, 10, 5) <= -50);
    CHECK(sandbox_temp(&g_sb, 15, 18) >= 700); /* fresh lava is glowing hot */
}

static void test_new_hot_and_cold_cells_are_made_hot_and_cold(void) {
    sandbox_init(&g_sb, 20, 20, 52);
    sandbox_set(&g_sb, 5, 5, EL_FIRE);
    sandbox_set(&g_sb, 8, 5, EL_NITROGEN);
    sandbox_set(&g_sb, 12, 5, EL_ICE);
    CHECK(sandbox_temp(&g_sb, 5, 5) >= 500);
    CHECK(sandbox_temp(&g_sb, 8, 5) <= -150);
    CHECK(sandbox_temp(&g_sb, 12, 5) <= 0);
}

/* A bar with a heater at its left end; returns the temperature `at` cells along the bar after `ticks`. */
static int bar_temp(ElementId bar, int ticks, int at) {
    sandbox_init(&g_sb, 50, 20, 53);
    fill(1, 10, 3, 10, EL_HEATER);
    fill(4, 10, 25, 10, bar);
    run(ticks);
    return sandbox_temp(&g_sb, 4 + at, 10);
}

static void test_heat_flows_along_a_conductor_but_not_an_insulator(void) {
    /* steel carries the heater's warmth a good way down the bar, all the way to the far end; brick does not
     */
    CHECK(bar_temp(EL_STEEL, 600, 8) > 150);
    CHECK(bar_temp(EL_STEEL, 600, 21) > 60);
    CHECK(bar_temp(EL_BRICK, 600, 8) < 30);
    CHECK(bar_temp(EL_BRICK, 600, 21) < 25);
}

static void test_conduction_never_overshoots(void) {
    sandbox_init(&g_sb, 30, 10, 54);
    fill(5, 4, 24, 4, EL_STEEL);
    sandbox_set_temp(&g_sb, 5, 4, 600);
    int peak_far = 0;
    for (int t = 0; t < 800; t++) {
        sandbox_step(&g_sb);
        peak_far = arc_maxi(peak_far, sandbox_temp(&g_sb, 24, 4));
        CHECK(sandbox_temp(&g_sb, 15, 4) <= 600); /* heat only flows downhill */
    }
    CHECK(peak_far <= 600);
}

static void test_hot_things_cool_down_in_air(void) {
    sandbox_init(&g_sb, 20, 20, 55);
    fill(5, 5, 8, 8, EL_STEEL);
    for (int y = 5; y <= 8; y++)
        for (int x = 5; x <= 8; x++)
            sandbox_set_temp(&g_sb, x, y, 400);
    run(1500);
    CHECK(sandbox_temp(&g_sb, 6, 6) < 100);
    CHECK(sandbox_temp(&g_sb, 6, 6) >= SANDBOX_AMBIENT);
}

/* A pot of water on a plate over a heater, standing on a stone floor (the starter scene's kettle). */
static int kettle_water_left(ElementId pot, int ticks) {
    tank(40, 30, 56);
    fill(5, 27, 6, 28, EL_BRICK);
    fill(30, 27, 31, 28, EL_BRICK);
    fill(7, 28, 29, 28, EL_HEATER);
    fill(5, 27, 31, 27, pot);
    fill(5, 19, 5, 26, pot);
    fill(31, 19, 31, 26, pot);
    fill(6, 20, 30, 26, EL_WATER);
    run(ticks);
    return sandbox_count(&g_sb, EL_WATER);
}

static void test_steel_pot_boils_water_but_a_brick_pot_hardly_does(void) {
    int steel = kettle_water_left(EL_STEEL, 700);
    /* nearly half of the 175 cells of water have boiled away (some steam has settled again)... */
    CHECK(steel < 120);
    CHECK(sandbox_count(&g_sb, EL_STEAM) > 50);
    int brick = kettle_water_left(EL_BRICK, 700);
    CHECK(brick > 150); /* ...while the same pot made of brick has hardly lost any */
    CHECK(brick > steel + 40);
}

static void test_a_pot_of_water_warms_up_before_it_boils(void) {
    kettle_water_left(EL_STEEL, 40);
    int early = sandbox_temp(&g_sb, 18, 25);
    CHECK(early > 30 && early < 100);
    CHECK(sandbox_count(&g_sb, EL_STEAM) < 5);
}

static void test_water_freezes_and_ice_melts_by_temperature(void) {
    tank(20, 20, 57);
    fill(1, 16, 18, 18, EL_WATER);
    fill(1, 19, 18, 19, EL_COOLER); /* a cold plate under the pool */
    run(400);
    CHECK(sandbox_count(&g_sb, EL_ICE) > 15); /* cold enough for long enough: it freezes... */
    for (int y = 0; y < 20; y++)
        for (int x = 0; x < 20; x++)
            if (sandbox_get(&g_sb, x, y) == EL_ICE)
                sandbox_set_temp(&g_sb, x, y, 400); /* enough to pay for the melting at once */
    fill(1, 19, 18, 19, EL_STONE);                  /* take the cold plate away */
    run(100);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_ICE), 0); /* warm again: it melts */
    CHECK(sandbox_count(&g_sb, EL_WATER) > 30);
}

static void test_hot_sand_turns_to_glass(void) {
    sandbox_init(&g_sb, 30, 20, 58);
    fill(15, 10, 20, 12, EL_SAND);
    for (int y = 10; y <= 12; y++)
        for (int x = 15; x <= 20; x++)
            sandbox_set_temp(&g_sb, x, y, 1200);
    run(30);
    CHECK(sandbox_count(&g_sb, EL_GLASS) > 5);
}

/* Average of one colour channel (shift 16 = red, 8 = green) over all cells of `id`; -1 if there are none. */
static int average_channel(ElementId id, int shift) {
    long sum = 0;
    int n = 0;
    for (int y = 0; y < g_sb.h; y++)
        for (int x = 0; x < g_sb.w; x++)
            if (sandbox_get(&g_sb, x, y) == id) {
                sum += (long)((sandbox_pixel(&g_sb, x, y) >> shift) & 255u);
                n++;
            }
    return n ? (int)(sum / n) : -1;
}

static void test_lava_colour_follows_its_temperature(void) {
    sandbox_init(&g_sb, 20, 20, 71);
    sandbox_set(&g_sb, 5, 10, EL_LAVA);
    sandbox_set(&g_sb, 8, 10, EL_LAVA);
    sandbox_set(&g_sb, 11, 10, EL_LAVA);
    sandbox_set_temp(&g_sb, 5, 10, 1400);
    sandbox_set_temp(&g_sb, 8, 10, 800);
    sandbox_set_temp(&g_sb, 11, 10, 540);
    uint32_t hot = sandbox_pixel(&g_sb, 5, 10), mid = sandbox_pixel(&g_sb, 8, 10),
             cool = sandbox_pixel(&g_sb, 11, 10);
    int g_hot = (int)((hot >> 8) & 255u), g_mid = (int)((mid >> 8) & 255u),
        g_cool = (int)((cool >> 8) & 255u);
    CHECK(g_hot > g_mid + 50); /* very hot lava is pale, cooling lava goes orange and then red... */
    CHECK(g_mid > g_cool + 20);
    CHECK(g_cool < 70);
    CHECK((int)((cool >> 16) & 255u) <
          (int)((mid >> 16) & 255u)); /* ...and dims to dark crimson at the end */
}

static void test_lava_cools_through_the_colours_and_sets_into_basalt(void) {
    tank(40, 30, 72);
    fill(10, 24, 25, 28, EL_LAVA);
    int lava = sandbox_count(&g_sb, EL_LAVA);
    run(3);
    int green_start = average_channel(EL_LAVA, 8);
    run(150);
    int green_mid = average_channel(EL_LAVA, 8);
    CHECK(green_mid >= 0 && green_mid < green_start - 25); /* it has cooled to orange and red... */
    CHECK(sandbox_count(&g_sb, EL_BASALT) > 0);            /* ...and is already crusting over */
    run(1500);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_LAVA), 0);     /* in the end it has all set... */
    CHECK(sandbox_count(&g_sb, EL_BASALT) >= lava - 4); /* ...into dark rock, one for one */
    CHECK(sandbox_temp(&g_sb, 15, 27) < 500);
}

static void test_basalt_melts_back_into_lava_when_it_is_heated(void) {
    tank(30, 20, 73);
    fill(12, 17, 18, 18, EL_BASALT);
    for (int y = 17; y <= 18; y++)
        for (int x = 12; x <= 18; x++)
            sandbox_set_temp(&g_sb, x, y, 1500);
    run(40);
    CHECK(sandbox_count(&g_sb, EL_LAVA) > 3);
    sandbox_set(&g_sb, 5, 18, EL_BASALT);
    sandbox_set_temp(&g_sb, 5, 18, 800); /* ordinary heat does not melt it */
    run(30);
    CHECK_EQ_INT(sandbox_get(&g_sb, 5, 18), EL_BASALT);
}

static int spread_of_pool(int start_temp) {
    tank(120, 30, 74);
    fill(55, 14, 64, 28, EL_LAVA); /* a column of lava, 10 wide, standing on the floor */
    for (int y = 14; y <= 28; y++)
        for (int x = 55; x <= 64; x++)
            sandbox_set_temp(&g_sb, x, y, start_temp);
    run(100);
    int lo = 120, hi = 0;
    for (int x = 1; x < 119; x++)
        if (sandbox_get(&g_sb, x, 28) == EL_LAVA || sandbox_get(&g_sb, x, 28) == EL_BASALT) {
            lo = arc_mini(lo, x);
            hi = arc_maxi(hi, x);
        }
    return hi - lo + 1;
}

static void test_lava_thickens_as_it_cools(void) {
    int runny = spread_of_pool(1150), stiff = spread_of_pool(620);
    CHECK(runny > stiff + 5); /* hot lava runs out into a wide pool, cool lava stays piled up */
}

static void test_lava_throws_up_flames(void) {
    tank(60, 30, 75);
    fill(10, 26, 50, 28, EL_LAVA);
    int flames = 0;
    for (int t = 0; t < 40; t++) {
        sandbox_step(&g_sb);
        flames += sandbox_count(&g_sb, EL_FIRE);
    }
    CHECK(flames > 10);
}

static void test_wood_catches_fire_at_its_ignition_temperature(void) {
    sandbox_init(&g_sb, 20, 20, 59);
    fill(5, 10, 10, 12, EL_WOOD);
    run(100);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_FIRE), 0); /* warm wood does not burn... */
    for (int y = 10; y <= 12; y++)
        for (int x = 5; x <= 10; x++)
            sandbox_set_temp(&g_sb, x, y, 350);
    run(60);
    CHECK(sandbox_count(&g_sb, EL_FIRE) > 0); /* ...hot wood does, with nothing else touching it */
}

static void test_every_element_survives_extreme_temperatures(void) {
    for (int id = 1; id < EL_COUNT; id++) {
        for (int pass = 0; pass < 2; pass++) {
            sandbox_init(&g_sb, 20, 20, 70);
            fill(0, 19, 19, 19, EL_STONE);
            fill(8, 15, 11, 18, (ElementId)id);
            for (int y = 15; y <= 18; y++)
                for (int x = 8; x <= 11; x++)
                    sandbox_set_temp(&g_sb, x, y, pass == 0 ? 9000 : -273);
            run(80);
            for (int i = 0; i < 20 * 20; i++) {
                CHECK(g_sb.temp[i] >= SANDBOX_TEMP_MIN && g_sb.temp[i] <= SANDBOX_TEMP_MAX);
                CHECK(g_sb.cells[i] < EL_COUNT);
            }
        }
    }
}

/* ---- explosions: sparks ---- */

static int sparks_beyond(int cx, int cy, int radius) {
    int n = 0;
    for (int y = 0; y < g_sb.h; y++)
        for (int x = 0; x < g_sb.w; x++)
            if (sandbox_get(&g_sb, x, y) == EL_SPARK &&
                (x - cx) * (x - cx) + (y - cy) * (y - cy) > radius * radius)
                n++;
    return n;
}

static void test_sparks_fly_out_in_all_directions(void) {
    tank(200, 120, 60);
    sandbox_burst(&g_sb, 100, 60, 200);
    run(8);
    int quadrant[4] = {0, 0, 0, 0};
    for (int y = 0; y < 120; y++)
        for (int x = 1; x < 199; x++)
            if (sandbox_get(&g_sb, x, y) == EL_SPARK)
                quadrant[(x > 100) + 2 * (y > 60)]++;
    for (int q = 0; q < 4; q++)
        CHECK(quadrant[q] > 5);             /* every direction got its share */
    CHECK(sparks_beyond(100, 60, 14) > 20); /* five cells per tick: 8 ticks later the survivors are far out */
    run(250);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_SPARK), 0); /* none lives forever */
}

static void test_sparks_dig_a_crater_in_sand(void) {
    tank(100, 50, 61);
    fill(1, 30, 98, 48, EL_SAND);
    int before = sandbox_count(&g_sb, EL_SAND);
    sandbox_burst(&g_sb, 50, 31, 60);
    run(80);
    CHECK(sandbox_count(&g_sb, EL_SAND) < before - 20); /* the sparks removed sand along their paths... */
    CHECK(sandbox_count(&g_sb, EL_SAND) >
          before - 600);                    /* ...but they run out of steam: it is not a vacuum */
    CHECK(column_height(50, EL_SAND) < 19); /* and the sand slid into the hole they made */
}

static void test_sparks_cannot_pass_walls_or_steel(void) {
    tank(60, 40, 62);
    fill(20, 15, 40, 15, EL_WALL); /* a pocket closed in by wall and steel, with sand above and below it */
    fill(20, 25, 40, 25, EL_STEEL);
    fill(20, 16, 20, 24, EL_STEEL);
    fill(40, 16, 40, 24, EL_WALL);
    fill(26, 13, 34, 14, EL_SAND);
    fill(26, 37, 34, 38, EL_SAND);
    int walls = sandbox_count(&g_sb, EL_WALL), steel = sandbox_count(&g_sb, EL_STEEL);
    sandbox_burst(&g_sb, 30, 20, 25); /* the pocket is 21 cells wide: all of them start inside it */
    run(100);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_WALL), walls); /* not one cell of the box was broken... */
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_STEEL), steel);
    CHECK_EQ_INT(sandbox_count_in(&g_sb, 20, 5, 40, 14, EL_SAND),
                 9 * 2); /* ...so the sand outside is untouched */
    CHECK_EQ_INT(sandbox_count_in(&g_sb, 20, 30, 40, 38, EL_SAND), 9 * 2);
}

static void test_a_charge_goes_off_all_at_once(void) {
    tank(80, 40, 63);
    fill(30, 30, 41, 37, EL_TNT);
    fill(42, 30, 45, 37, EL_GUNPOWDER); /* touching charges of both kinds go up together */
    sandbox_set(&g_sb, 30, 29, EL_FIRE);
    run(3);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_TNT), 0);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_GUNPOWDER), 0);
    CHECK(sandbox_count(&g_sb, EL_SPARK) > 60);
}

static void test_a_separate_charge_is_set_off_by_sparks_not_gathered(void) {
    tank(100, 40, 67);
    fill(10, 31, 14, 37, EL_TNT);
    fill(40, 20, 41, 37, EL_TNT); /* a tall second charge, 25 cells away and not touching the first */
    for (int t = 0; t < 30 && sandbox_count_in(&g_sb, 10, 31, 5, 7, EL_TNT) > 0; t++)
        torch(10, 30, 14, 30, 1);
    CHECK_EQ_INT(sandbox_count_in(&g_sb, 10, 31, 5, 7, EL_TNT), 0);
    CHECK_EQ_INT(sandbox_count_in(&g_sb, 40, 20, 2, 18, EL_TNT), 36); /* it was not part of that charge */
    run(300);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_TNT), 0); /* but the flying sparks got to it in the end */
}

static void test_explosion_under_water_blows_a_hole_and_boils_some_off(void) {
    tank(80, 40, 68);
    fill(1, 20, 78, 38, EL_WATER);
    fill(38, 34, 41, 37, EL_TNT);
    sandbox_set_temp(&g_sb, 40, 35,
                     400); /* hot enough to set it off (a flame would be put out by the water) */
    int water = sandbox_count(&g_sb, EL_WATER);
    int steam = 0;
    for (int t = 0; t < 40; t++) {
        sandbox_step(&g_sb);
        steam = arc_maxi(steam, sandbox_count(&g_sb, EL_STEAM));
    }
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_TNT), 0);
    CHECK(sandbox_count(&g_sb, EL_WATER) < water - 5); /* sparks ploughed water out of the way */
    CHECK(steam >= 1);                                 /* and some of it flashed to steam */
}

static void test_burst_handles_silly_arguments(void) {
    tank(40, 30, 66);
    sandbox_burst(&g_sb, 20, 15, 0);
    sandbox_burst(&g_sb, 20, 15, -5);
    sandbox_burst(&g_sb, -50, 400, 100);
    sandbox_burst(&g_sb, 20, 15, 5000);
    run(300);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_SPARK), 0);
}

static void test_a_lone_spark_flies_about_five_cells_per_tick(void) {
    tank(80, 40, 69);
    sandbox_burst(&g_sb, 40, 20, 1);
    run(3);
    int far = 0, near = 0;
    for (int y = 0; y < 40; y++)
        for (int x = 1; x < 79; x++)
            if (sandbox_get(&g_sb, x, y) == EL_SPARK) {
                int d = arc_maxi(iabs(x - 40), iabs(y - 20));
                far += d >= 10;
                near += d < 10;
            }
    CHECK_EQ_INT(far, 1); /* 3 ticks of five cells: well over ten cells from where it started */
    CHECK_EQ_INT(near, 0);
}

/* ---- the engine as a whole ---- */

static void test_mass_is_conserved_for_inert_elements(void) {
    tank(40, 40, 40);
    for (int i = 0; i < 300; i++) {
        sandbox_paint(&g_sb, 8 + (i * 5) % 24, 2, 1,
                      i % 3 == 0   ? EL_SAND
                      : i % 3 == 1 ? EL_WATER
                                   : EL_MERCURY);
        sandbox_step(&g_sb);
    }
    int total =
        sandbox_count(&g_sb, EL_SAND) + sandbox_count(&g_sb, EL_WATER) + sandbox_count(&g_sb, EL_MERCURY);
    run(200);
    int after =
        sandbox_count(&g_sb, EL_SAND) + sandbox_count(&g_sb, EL_WATER) + sandbox_count(&g_sb, EL_MERCURY);
    CHECK_EQ_INT(after, total); /* nothing appears or disappears while only inert fluids move */
}

static void test_deterministic_with_every_element(void) {
    static Sandbox other;
    sandbox_init(&g_sb, 60, 40, 123);
    sandbox_init(&other, 60, 40, 123);
    Rng pick;
    rng_seed(&pick, 5);
    for (int i = 0; i < 400; i++) {
        ElementId id = sandbox_palette_id(rng_range(&pick, 0, sandbox_palette_count() - 1));
        int x = rng_range(&pick, 0, 59), y = rng_range(&pick, 0, 39), r = rng_range(&pick, 0, 3);
        sandbox_paint(&g_sb, x, y, r, id);
        sandbox_paint(&other, x, y, r, id);
        sandbox_step(&g_sb);
        sandbox_step(&other);
    }
    CHECK(memcmp(g_sb.cells, other.cells, sizeof(g_sb.cells)) == 0);
    CHECK(memcmp(g_sb.aux, other.aux, sizeof(g_sb.aux)) == 0);
    CHECK(memcmp(g_sb.temp, other.temp, sizeof(g_sb.temp)) == 0);
}

static void test_chaos_never_breaks_the_grid(void) {
    sandbox_init(&g_sb, 80, 50, 99);
    Rng pick;
    rng_seed(&pick, 11);
    for (int i = 0; i < 1500; i++) {
        ElementId id = sandbox_palette_id(rng_range(&pick, 0, sandbox_palette_count() - 1));
        sandbox_paint(&g_sb, rng_range(&pick, -5, 85), rng_range(&pick, -5, 55), rng_range(&pick, 0, 4), id);
        sandbox_step(&g_sb);
    }
    for (int i = 0; i < 80 * 50; i++)
        CHECK(g_sb.cells[i] < EL_COUNT); /* every cell is still a valid element */
    for (int i = 80 * 50; i < SANDBOX_MAX_W * SANDBOX_MAX_H; i++)
        CHECK_EQ_INT(g_sb.cells[i], EL_EMPTY); /* and nothing leaked outside the used area */
}

static void test_clear_resets_everything(void) {
    sandbox_init(&g_sb, 20, 20, 1);
    sandbox_set(&g_sb, 5, 5, EL_URANIUM);
    g_sb.aux[5 * 20 + 5] = 99;
    sandbox_clear(&g_sb);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_URANIUM), 0);
    CHECK_EQ_INT(g_sb.aux[5 * 20 + 5], 0);
}

int main(void) {
    RUN(test_grid_and_bounds);
    RUN(test_sand_falls_to_floor);
    RUN(test_sand_makes_a_pile_and_conserves_mass);
    RUN(test_stone_is_static_and_blocks);
    RUN(test_water_spreads_flat);
    RUN(test_sand_sinks_through_water);
    RUN(test_paint_and_erase);
    RUN(test_count_in_rectangle);
    RUN(test_water_fills_a_stone_basin);
    RUN(test_element_table);
    RUN(test_deterministic);
    RUN(test_palette);
    RUN(test_colours_are_distinct_and_stable);
    RUN(test_every_element_has_a_row);
    RUN(test_brush_spray_and_hidden_cells);
    RUN(test_set_clears_cell_memory);
    RUN(test_sand_falls_several_cells_per_tick);
    RUN(test_a_big_dump_splashes_wider_than_a_trickle);
    RUN(test_a_trickle_makes_a_steep_cone);
    RUN(test_water_levels_out_quickly);
    RUN(test_a_settled_pool_stays_still);
    RUN(test_water_flows_off_a_ledge);
    RUN(test_liquids_layer_by_density);
    RUN(test_light_powder_floats_on_water);
    RUN(test_gas_rises_and_stays_conserved_when_sealed);
    RUN(test_heavy_things_sink_through_light_fluids);
    RUN(test_fire_burns_wood_to_ash);
    RUN(test_fire_dies_without_fuel);
    RUN(test_water_puts_out_fire);
    RUN(test_oil_burns_and_keeps_flowing);
    RUN(test_gunpowder_goes_off_all_at_once_as_sparks);
    RUN(test_fuse_burns_slowly_along_its_length);
    RUN(test_tnt_explodes_and_walls_survive);
    RUN(test_tnt_chain_reaction);
    RUN(test_methane_burns);
    RUN(test_lava_meets_water);
    RUN(test_lava_turns_sand_to_glass);
    RUN(test_thermite_burns_through_stone);
    RUN(test_acid_dissolves_stone_but_not_glass_or_walls);
    RUN(test_nitrogen_freezes_water);
    RUN(test_heat_melts_ice_and_snow);
    RUN(test_steam_condenses_back_to_water);
    RUN(test_smoke_fades);
    RUN(test_seed_on_soil_grows_and_seed_elsewhere_does_not);
    RUN(test_plants_drink_water_and_grow_bigger);
    RUN(test_plants_burn);
    RUN(test_termites_eat_wood_and_starve);
    RUN(test_termites_starve_without_food);
    RUN(test_life_blinker_and_glider);
    RUN(test_uranium_gamma_rays_stop_at_walls);
    RUN(test_gamma_rays_burn_through_what_they_cross);
    RUN(test_uranium_heats_itself_in_proportion_to_its_size);
    RUN(test_big_uranium_lump_melts_then_goes_critical_and_wrecks_the_world);
    RUN(test_small_uranium_lump_is_stable);
    RUN(test_big_explosions_do_not_flash_the_screen);
    RUN(test_one_critical_cell_takes_the_whole_lump);
    RUN(test_a_lump_too_small_to_blow_only_melts);
    RUN(test_water_keeps_a_small_lump_cool_but_not_a_big_one);
    RUN(test_a_bigger_lump_makes_a_bigger_blast);
    RUN(test_molten_uranium_solidifies_in_water);
    RUN(test_every_cell_starts_at_room_temperature);
    RUN(test_heat_sources_and_cold_sources_have_their_own_temperature);
    RUN(test_new_hot_and_cold_cells_are_made_hot_and_cold);
    RUN(test_heat_flows_along_a_conductor_but_not_an_insulator);
    RUN(test_conduction_never_overshoots);
    RUN(test_hot_things_cool_down_in_air);
    RUN(test_steel_pot_boils_water_but_a_brick_pot_hardly_does);
    RUN(test_a_pot_of_water_warms_up_before_it_boils);
    RUN(test_water_freezes_and_ice_melts_by_temperature);
    RUN(test_hot_sand_turns_to_glass);
    RUN(test_lava_colour_follows_its_temperature);
    RUN(test_lava_cools_through_the_colours_and_sets_into_basalt);
    RUN(test_basalt_melts_back_into_lava_when_it_is_heated);
    RUN(test_lava_thickens_as_it_cools);
    RUN(test_lava_throws_up_flames);
    RUN(test_wood_catches_fire_at_its_ignition_temperature);
    RUN(test_every_element_survives_extreme_temperatures);
    RUN(test_sparks_fly_out_in_all_directions);
    RUN(test_sparks_dig_a_crater_in_sand);
    RUN(test_sparks_cannot_pass_walls_or_steel);
    RUN(test_a_charge_goes_off_all_at_once);
    RUN(test_a_separate_charge_is_set_off_by_sparks_not_gathered);
    RUN(test_explosion_under_water_blows_a_hole_and_boils_some_off);
    RUN(test_tnt_makes_a_real_explosion);
    RUN(test_one_tnt_cell_still_goes_off);
    RUN(test_burst_handles_silly_arguments);
    RUN(test_a_lone_spark_flies_about_five_cells_per_tick);
    RUN(test_mass_is_conserved_for_inert_elements);
    RUN(test_deterministic_with_every_element);
    RUN(test_chaos_never_breaks_the_grid);
    RUN(test_clear_resets_everything);
    return TEST_SUMMARY();
}
