/* Flight, tour, viewpoints and clocks. The city itself is tested in test_swc_city.c. */
#include <string.h>

#include "arcade_test.h"
#include "synthwave_city_logic.h"

static synthwave_city_State s; /* about 2.5 MB: static, like in the game */

static ArcadeInput idle(void) {
    ArcadeInput in;
    arcade_input_clear(&in);
    return in;
}

static void run(int frames, const ArcadeInput *in) {
    for (int i = 0; i < frames; i++)
        synthwave_city_step(&s, in, ARC_DT);
}

/* Exact sphere-against-box test (cylinders for round buildings), written independently of swc_city_collide:
 * true if the camera sphere, shrunk by 5 cm, overlaps any box of the 3 x 3 cells around it. */
static bool camera_in_a_building(void) {
    int cx = (int)floor(s.cam.x / SWC_CELL), cz = (int)floor(s.cam.z / SWC_CELL);
    double r = SWC_CAMERA_RADIUS - 0.05;
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++) {
            int cell = swc_cell_index(cx + dx, cz + dz);
            for (int k = s.city.box_start[cell]; k < s.city.box_start[cell + 1]; k++) {
                const SwcBox *b = &s.city.boxes[s.city.box_list[k]];
                double ex = fabs(swc_wrap_delta(s.cam.x, (double)b->cx, SWC_TILE));
                double ez = fabs(swc_wrap_delta(s.cam.z, (double)b->cz, SWC_TILE));
                double gap_h;
                if (b->flags & SWC_FLAG_ROUND)
                    gap_h = fmax(0.0, hypot(ex, ez) - fmin((double)b->sx, (double)b->sz) * 0.5);
                else
                    gap_h = hypot(fmax(0.0, ex - (double)b->sx * 0.5), fmax(0.0, ez - (double)b->sz * 0.5));
                double top = (double)b->y0 + (double)b->sy;
                double gap_v =
                    s.cam.y < (double)b->y0 ? (double)b->y0 - s.cam.y : (s.cam.y > top ? s.cam.y - top : 0.0);
                if (hypot(gap_h, gap_v) < r)
                    return true;
            }
        }
    return false;
}

static void test_init_is_sane(void) {
    synthwave_city_init(&s, 1u);
    CHECK(!synthwave_city_is_over(&s));
    CHECK_EQ_INT(synthwave_city_score(&s), 0);
    CHECK(s.touring);
    CHECK_EQ_INT(s.mood, 0);
    CHECK(s.cam.x >= 0.0 && s.cam.x < SWC_TILE && s.cam.z >= 0.0 && s.cam.z < SWC_TILE);
    CHECK(!camera_in_a_building());
    CHECK_EQ_INT(s.city.box_count > 10000, 1);
}

static void test_the_six_viewpoints_are_in_free_air(void) {
    static const char *names[SWC_VIEW_COUNT] = {"Sunset Avenue", "Downtown Canyon", "Rooftop",
                                                "Panorama",      "Megatower",       "Plaza"};
    for (unsigned seed = 1; seed <= 12; seed++) {
        synthwave_city_init(&s, seed * 31u);
        for (int i = 0; i < SWC_VIEW_COUNT; i++) {
            const SwcViewpoint *v = &s.views[i];
            CHECK_STR_EQ(v->name, names[i]);
            CHECK(v->x >= 0.0 && v->x < SWC_TILE && v->z >= 0.0 && v->z < SWC_TILE);
            CHECK(v->y >= SWC_CAMERA_RADIUS && v->y <= SWC_MAX_ALTITUDE);
            SwcVec3 p = {v->x, v->y, v->z};
            CHECK(!swc_city_collide(&s.city, &p, SWC_CAMERA_RADIUS));
        }
        CHECK(s.views[2].y > 20.0);  /* the rooftop is a roof */
        CHECK(s.views[3].y > 500.0); /* the panorama is high above the skyline */
    }
}

static void test_the_tour_flies_on_its_own_and_never_hits_a_building(void) {
    for (unsigned seed = 1; seed <= 4; seed++) {
        synthwave_city_init(&s, seed);
        ArcadeInput in = idle();
        double lo = 1e9, hi = 0.0;
        for (int i = 0; i < 4 * 60 * 60; i++) { /* four minutes */
            synthwave_city_step(&s, &in, ARC_DT);
            CHECK(!camera_in_a_building());
            CHECK(!s.touched);
            if (s.cam.y < lo)
                lo = s.cam.y;
            if (s.cam.y > hi)
                hi = s.cam.y;
        }
        CHECK(s.touring);
        CHECK(s.distance > 6000.0);                  /* it does not hover */
        CHECK(lo >= SWC_CAMERA_RADIUS && lo < 40.0); /* it sometimes skims the street... */
        CHECK(hi > 100.0 && hi <= SWC_MAX_ALTITUDE); /* ...and sometimes climbs over the roofs */
        CHECK(synthwave_city_score(&s) >= 6);
    }
}

static void test_flight_forward_reaches_top_speed(void) {
    synthwave_city_init(&s, 1u);
    swc_place_camera(&s, s.views[3].x, s.views[3].y, s.views[3].z, 0.0, 0.0); /* above the skyline */
    ArcadeInput in = idle();
    in.move_y = -1.0f; /* W: the move axis is -1 when forward is held */
    double z0 = s.cam.z;
    run(60 * 6, &in);
    double speed = hypot(s.cam.vx, s.cam.vz);
    CHECK_NEAR(speed, swc_top_speed(s.cam.y, 1.0), 0.5);
    CHECK(swc_wrap_delta(s.cam.z, z0, SWC_TILE) < -100.0); /* yaw 0 faces -z */
    CHECK(fabs(swc_wrap_delta(s.cam.x, s.views[3].x, SWC_TILE)) < 1e-6);
    CHECK(!s.touring);
    /* boost is 3.5 times faster */
    in.alt = true;
    run(60 * 8, &in);
    CHECK_NEAR(hypot(s.cam.vx, s.cam.vz), swc_top_speed(s.cam.y, SWC_BOOST), 1.0);
}

static void test_flight_does_not_depend_on_the_frame_rate(void) {
    static synthwave_city_State other;
    synthwave_city_init(&s, 1u);
    swc_goto_view(&s, 3);
    memcpy(&other, &s, sizeof(s));
    ArcadeInput in = idle();
    in.move_y = -1.0f;
    in.move_x = 0.5f;
    for (int i = 0; i < 60 * 3; i++)
        synthwave_city_step(&s, &in, 1.0f / 60.0f);
    for (int i = 0; i < 30 * 3; i++)
        synthwave_city_step(&other, &in, 1.0f / 30.0f);
    CHECK_NEAR(swc_wrap_delta(s.cam.x, other.cam.x, SWC_TILE), 0.0, 0.02);
    CHECK_NEAR(s.cam.y, other.cam.y, 0.02);
    CHECK_NEAR(swc_wrap_delta(s.cam.z, other.cam.z, SWC_TILE), 0.0, 0.02);
}

static void test_flying_into_a_wall_slides_and_never_enters(void) {
    synthwave_city_init(&s, 1u);
    /* pick a tall box and aim at its flank from the street */
    const SwcBox *wall = NULL;
    for (int i = 0; i < s.city.box_count && wall == NULL; i++) {
        const SwcBox *b = &s.city.boxes[i];
        if (b->y0 == 0.0f && b->sy > 60.0f && b->sx > 40.0f && b->sz > 40.0f && !(b->flags & SWC_FLAG_ROUND))
            wall = b;
    }
    CHECK(wall != NULL);
    /* facing -z, straight at the wall */
    swc_place_camera(&s, (double)wall->cx, 20.0, (double)wall->cz + (double)wall->sz * 0.5 + 6.0, 0.0, 0.0);
    ArcadeInput in = idle();
    in.move_y = -1.0f;
    in.alt = true;
    bool touched = false;
    for (int i = 0; i < 60 * 4; i++) {
        synthwave_city_step(&s, &in, ARC_DT);
        touched = touched || s.touched;
        CHECK(!camera_in_a_building());
    }
    CHECK(touched);
    /* it did not get through: still on the street side of the wall */
    CHECK(swc_wrap_delta(s.cam.z, wall->cz, SWC_TILE) > wall->sz * 0.5);
}

static void test_the_viewpoint_keys(void) {
    synthwave_city_init(&s, 1u);
    for (int i = 0; i < SWC_VIEW_COUNT; i++) {
        ArcadeInput in = idle();
        in.typed = '1' + i;
        synthwave_city_step(&s, &in, ARC_DT);
        CHECK(!s.touring);
        CHECK_EQ_INT(s.view, i);
        /* the camera glides there over a second... */
        in = idle();
        run(30, &in);
        CHECK(s.rig.phase == SWC_PHASE_EASE);
        run(40, &in);
        /* ...and lands exactly on it, in free flight */
        CHECK(s.rig.phase == SWC_PHASE_FREE);
        CHECK_NEAR(swc_wrap_delta(s.cam.x, s.views[i].x, SWC_TILE), 0.0, 1e-9);
        CHECK_NEAR(s.cam.y, s.views[i].y, 1e-9);
    }
    ArcadeInput bad = idle();
    bad.typed = '9';
    int view = s.view;
    synthwave_city_step(&s, &bad, ARC_DT);
    CHECK_EQ_INT(s.view, view);
}

static void test_mood_key_cycles(void) {
    synthwave_city_init(&s, 1u);
    ArcadeInput in = idle();
    in.typed = 't';
    for (int i = 1; i <= 4; i++) {
        synthwave_city_step(&s, &in, ARC_DT);
        CHECK_EQ_INT(s.mood, i % SWC_MOOD_COUNT);
    }
}

static void test_space_toggles_the_tour_and_controls_take_over(void) {
    synthwave_city_init(&s, 1u);
    CHECK(s.touring);
    ArcadeInput in = idle();
    in.move_x = 1.0f; /* any flight control takes the camera */
    synthwave_city_step(&s, &in, ARC_DT);
    CHECK(!s.touring);
    in = idle();
    in.action = in.action_pressed = true;
    synthwave_city_step(&s, &in, ARC_DT);
    CHECK(s.touring); /* on its way back: the join, then the blend */
    CHECK(s.rig.phase == SWC_PHASE_JOIN);
    synthwave_city_step(&s, &in, ARC_DT);
    CHECK(!s.touring);
}

static void test_two_quick_clicks_take_over_and_a_drag_only_looks(void) {
    synthwave_city_init(&s, 1u);
    ArcadeInput in = idle();
    in.aim_x = 640.0f;
    in.aim_y = 360.0f;
    synthwave_city_step(&s, &in, ARC_DT);
    /* a drag on the tour pans the view and does not take over */
    double yaw0 = s.cam.yaw;
    in.fire = true;
    synthwave_city_step(&s, &in, ARC_DT);
    in.aim_x = 740.0f;
    synthwave_city_step(&s, &in, ARC_DT);
    CHECK(s.touring);
    CHECK(fabs(swc_wrap_delta(s.cam.yaw, yaw0, SWC_TAU)) > 0.1);
    in.fire = false;
    synthwave_city_step(&s, &in, ARC_DT);
    CHECK(s.touring);
    /* two clicks in place within a third of a second take over; one alone does not */
    in = idle();
    in.aim_x = 300.0f;
    in.aim_y = 200.0f;
    for (int click = 0; click < 2; click++) {
        in.fire = true;
        run(4, &in);
        in.fire = false;
        run(4, &in);
        CHECK(click == 0 ? s.touring : !s.touring);
    }
    /* two clicks too far apart in time do not */
    synthwave_city_init(&s, 1u);
    in = idle();
    in.aim_x = 300.0f;
    in.aim_y = 200.0f;
    for (int click = 0; click < 2; click++) {
        in.fire = true;
        run(4, &in);
        in.fire = false;
        run(30, &in);
    }
    CHECK(s.touring);
}

static void test_new_cities_roll_short_seeds_and_start_the_tour_over(void) {
    unsigned seen = 0;
    for (uint32_t e = 0; e < 2000u; e++) {
        unsigned cur = e % 7u == 0 ? 1u : 1984u;
        unsigned seed = swc_roll_seed(cur, e * 2654435761u);
        CHECK(seed >= 1u && seed <= SWC_MAX_ROLLED && seed != cur);
        CHECK_EQ_INT((int)seed, (int)swc_roll_seed(cur, e * 2654435761u)); /* the same draw, the same seed */
        seen |= seed > 500000u ? 1u : 2u;
    }
    CHECK_EQ_INT((int)seen, 3); /* both halves of the range come up */
    synthwave_city_init(&s, 1984u);
    s.mood = 2;
    ArcadeInput in = idle();
    in.move_y = -1.0f;
    run(120, &in); /* take over and fly for a while */
    CHECK(!s.touring);
    unsigned next = swc_roll_seed(1984u, 7u);
    swc_new_city(&s, next);
    CHECK_EQ_INT((int)s.city.seed, (int)next);
    CHECK_EQ_INT(s.mood, 2);
    CHECK(s.touring);
    CHECK_NEAR(s.cam.y, 150.0, 1e-6);
    CHECK_NEAR(s.distance, 0.0, 1e-12);
    synthwave_city_init(&s, 1u);
}

static void test_the_wheel_zooms_the_lens_on_the_tour(void) {
    synthwave_city_init(&s, 1u);
    ArcadeInput in = idle();
    in.wheel = 1; /* up: closer */
    run(1, &in);
    in = idle();
    run(30, &in);
    CHECK(s.touring);
    CHECK(s.fov < SWC_FOV_DEG - 5.0 && s.fov >= SWC_MIN_TOUR_FOV);
    in.wheel = -1;
    run(1, &in);
    in = idle();
    run(30, &in);
    CHECK_NEAR(s.fov, SWC_FOV_DEG, 0.5);   /* back to where it was: the zoom is a factor */
    CHECK_NEAR(s.speed_scale, 1.0, 1e-12); /* and it did not touch the flight speed */
}

static void test_e_flies_up_and_q_down(void) {
    synthwave_city_init(&s, 1u);
    swc_place_camera(&s, s.views[3].x, 600.0, s.views[3].z, 0.0, 0.0); /* above the skyline, level */
    ArcadeInput in = idle();
    in.move_z = 1.0f; /* E */
    double y0 = s.cam.y;
    run(60 * 2, &in);
    CHECK(!s.touring); /* the key took over, and flew from the spot it was pressed at */
    CHECK(s.cam.y > y0 + 40.0);
    CHECK(fabs(swc_wrap_delta(s.cam.x, s.views[3].x, SWC_TILE)) < 1e-6);
    CHECK(fabs(swc_wrap_delta(s.cam.z, s.views[3].z, SWC_TILE)) < 1e-6);
    double y1 = s.cam.y;
    in.move_z = -1.0f; /* Q */
    run(60 * 3, &in);
    CHECK(s.cam.y < y1 - 40.0);
    /* the vertical climb has a top speed of its own, which Shift multiplies */
    in.move_z = 1.0f;
    run(60 * 2, &in);
    double slow = s.cam.vy;
    in.shift = true;
    run(60 * 2, &in);
    CHECK(s.cam.vy > slow * 2.0);
}

static void test_shift_boosts_like_the_right_mouse_button(void) {
    double speed[3];
    for (int mode = 0; mode < 3; mode++) {
        synthwave_city_init(&s, 1u);
        swc_place_camera(&s, s.views[3].x, s.views[3].y, s.views[3].z, 0.0, 0.0);
        ArcadeInput in = idle();
        in.move_y = -1.0f;
        in.shift = mode == 1;
        in.alt = mode == 2;
        run(60 * 8, &in);
        speed[mode] = hypot(s.cam.vx, s.cam.vz);
    }
    CHECK_NEAR(speed[1], swc_top_speed(s.cam.y, SWC_BOOST), 2.0);
    CHECK(speed[1] > speed[0] * 2.5);
    CHECK_NEAR(speed[1], speed[2], 1e-9);
}

static void test_a_click_in_flight_asks_for_the_mouse_and_a_grabbed_mouse_looks(void) {
    synthwave_city_init(&s, 1u);
    ArcadeInput in = idle();
    in.aim_x = 640.0f;
    in.aim_y = 360.0f;
    run(2, &in);
    /* a single click on the tour is a half double click, or a drag to look: it does not ask for the mouse */
    in.fire = true;
    run(3, &in);
    in.fire = false;
    run(3, &in);
    CHECK(s.touring && !s.grab_wanted);
    /* W takes over; a click in flight asks for the mouse, once the button goes down */
    in = idle();
    in.aim_x = 640.0f;
    in.aim_y = 360.0f;
    in.move_y = -1.0f;
    run(5, &in);
    CHECK(!s.touring && !s.grab_wanted);
    in.move_y = 0.0f;
    in.fire = true;
    run(1, &in);
    CHECK(s.grab_wanted);
    swc_set_pointer_lock(&s, true); /* the front-end grabs it */
    CHECK(s.pointer_locked && !s.grab_wanted);
    in.fire = false;
    /* grabbed: the mouse looks without a button; the first frame after the grab is only a settling */
    double yaw0 = s.cam.yaw, pitch0 = s.cam.pitch;
    in.aim_x = 100.0f; /* the grab put the cursor somewhere else: no jump from where it was */
    in.aim_y = 100.0f;
    run(1, &in);
    CHECK_NEAR(swc_wrap_delta(s.cam.yaw, yaw0, SWC_TAU), 0.0, 1e-9);
    in.aim_x += 100.0f; /* 100 px right turns right, 50 px up looks up */
    in.aim_y -= 50.0f;
    run(1, &in);
    CHECK_NEAR(swc_wrap_delta(s.cam.yaw, yaw0, SWC_TAU), -100.0 * 0.0022, 1e-6);
    CHECK_NEAR(s.cam.pitch - pitch0, 50.0 * 0.0022, 1e-6);
    in.fire = true; /* a click while it has the mouse does not ask again */
    run(3, &in);
    CHECK(!s.grab_wanted);
    in.fire = false;
    run(1, &in);
    /* freed again: moving the mouse without the button does nothing */
    swc_set_pointer_lock(&s, false);
    yaw0 = s.cam.yaw;
    in.aim_x = 200.0f;
    run(2, &in);
    in.aim_x = 900.0f;
    run(1, &in);
    CHECK_NEAR(swc_wrap_delta(s.cam.yaw, yaw0, SWC_TAU), 0.0, 1e-9);
    /* and the click that grabs is not asked for twice while held */
    in.fire = true;
    run(1, &in);
    CHECK(s.grab_wanted);
    s.grab_wanted = false;
    run(5, &in);
    CHECK(!s.grab_wanted);
}

static void test_the_double_click_asks_for_the_mouse_and_a_grabbed_tour_pans_only_with_a_button(void) {
    synthwave_city_init(&s, 1u);
    ArcadeInput in = idle();
    in.aim_x = 300.0f;
    in.aim_y = 200.0f;
    for (int click = 0; click < 2; click++) {
        CHECK(!s.grab_wanted);
        in.fire = true;
        run(4, &in);
        in.fire = false;
        run(4, &in);
    }
    CHECK(!s.touring && s.grab_wanted);

    /* a grabbed mouse on the tour: moving it alone does not pan; dragging with the button does */
    static synthwave_city_State b;
    synthwave_city_init(&s, 1u);
    swc_set_pointer_lock(&s, true);
    memcpy(&b, &s, sizeof(b));
    in = idle();
    in.aim_x = 400.0f;
    in.aim_y = 300.0f;
    ArcadeInput still = in;
    for (int i = 0; i < 30; i++) {
        in.aim_x += 20.0f;
        synthwave_city_step(&s, &in, ARC_DT);
        synthwave_city_step(&b, &still, ARC_DT);
    }
    CHECK(s.touring);
    CHECK_NEAR(s.cam.yaw, b.cam.yaw, 1e-9);
    in.fire = true;
    still.fire = true;
    for (int i = 0; i < 10; i++) {
        in.aim_x += 20.0f;
        synthwave_city_step(&s, &in, ARC_DT);
        synthwave_city_step(&b, &still, ARC_DT);
    }
    CHECK(fabs(swc_wrap_delta(s.cam.yaw, b.cam.yaw, SWC_TAU)) > 0.1);
}

static void test_a_new_city_keeps_the_grab(void) {
    synthwave_city_init(&s, 1u);
    swc_set_pointer_lock(&s, true);
    swc_new_city(&s, 77u);
    CHECK_EQ_INT((int)s.city.seed, 77);
    CHECK(s.pointer_locked);
}

static void test_dragging_looks_around_without_a_jump(void) {
    synthwave_city_init(&s, 1u);
    swc_goto_view(&s, 3);
    double yaw0 = s.cam.yaw, pitch0 = s.cam.pitch;
    ArcadeInput in = idle();
    in.aim_x = 600.0f;
    in.aim_y = 300.0f;
    synthwave_city_step(&s, &in, ARC_DT); /* the pointer was elsewhere: no drag yet */
    in.fire = true;
    synthwave_city_step(&s, &in, ARC_DT); /* the button goes down: first frame of the drag, no movement */
    CHECK_NEAR(s.cam.yaw, yaw0, 1e-12);
    in.aim_x = 700.0f; /* drag 100 px right: turn right (towards +x) */
    in.aim_y = 250.0f; /* and 50 px up: look up */
    synthwave_city_step(&s, &in, ARC_DT);
    CHECK(swc_wrap_delta(s.cam.yaw, yaw0, SWC_TAU) < -0.1);
    CHECK(s.cam.pitch > pitch0 + 0.05);
    for (int i = 0; i < 200; i++) { /* looking straight up is limited */
        in.aim_y -= 100.0f;
        synthwave_city_step(&s, &in, ARC_DT);
    }
    CHECK(s.cam.pitch <= SWC_MAX_PITCH + 1e-9);
}

/* The next three tests are what the hub relies on when it runs the game behind its menu (see
 * synthwave_city_session.h): while the menu is up it feeds empty input, and the pointer reads (0, 0). */

static void test_an_idle_host_leaves_the_drone_flying(void) {
    synthwave_city_init(&s, 1u);
    ArcadeInput in = idle();
    run(1800, &in); /* 30 s of a menu nobody touches */
    CHECK(s.touring);
    CHECK(s.distance > 500.0); /* and the drone really flew */
    CHECK(!camera_in_a_building());
}

static void test_a_lone_space_press_hands_the_camera_back_to_the_drone(void) {
    synthwave_city_init(&s, 1u);
    swc_goto_view(&s, 1); /* the player is flying freely... */
    ArcadeInput in = idle();
    in.move_y = -1.0f;
    run(120, &in);
    CHECK(!s.touring);
    double px = s.cam.x, py = s.cam.y, pz = s.cam.z;
    in = idle(); /* ...and presses Esc: the hub sends one Space press and nothing else */
    in.action_pressed = true;
    synthwave_city_step(&s, &in, ARC_DT);
    CHECK(s.touring);
    double jump = hypot(hypot(swc_wrap_delta(s.cam.x, px, SWC_TILE), s.cam.y - py),
                        swc_wrap_delta(s.cam.z, pz, SWC_TILE));
    CHECK(jump < 3.0); /* the drone takes over from where the player was: no teleport */
    in = idle();
    run(1200, &in);
    CHECK(s.touring);
    CHECK(!camera_in_a_building());
}

static void test_the_click_that_takes_the_camera_does_not_turn_it(void) {
    synthwave_city_init(&s, 1u);
    swc_goto_view(&s, 3);
    ArcadeInput in = idle();
    run(10, &in); /* menu time */
    double yaw0 = s.cam.yaw, pitch0 = s.cam.pitch;
    in.fire = in.fire_pressed = true; /* the click that hides the menu: the pointer jumps from (0, 0) */
    in.aim_x = 1100.0f;
    in.aim_y = 650.0f;
    synthwave_city_step(&s, &in, ARC_DT);
    CHECK_NEAR(s.cam.yaw, yaw0, 1e-12);
    CHECK_NEAR(s.cam.pitch, pitch0, 1e-12);
}

static void test_wheel_scales_the_cruise_speed(void) {
    synthwave_city_init(&s, 1u);
    swc_goto_view(&s, 3); /* in free flight the wheel sets the speed */
    ArcadeInput in = idle();
    in.wheel = 1;
    synthwave_city_step(&s, &in, ARC_DT);
    CHECK_NEAR(s.speed_scale, 1.25, 1e-12);
    for (int i = 0; i < 40; i++)
        synthwave_city_step(&s, &in, ARC_DT);
    CHECK_NEAR(s.speed_scale, 4.0, 1e-12); /* capped */
    in.wheel = -1;
    for (int i = 0; i < 40; i++)
        synthwave_city_step(&s, &in, ARC_DT);
    CHECK_NEAR(s.speed_scale, 0.25, 1e-12);
}

static void test_clocks_loop(void) {
    synthwave_city_init(&s, 1u);
    ArcadeInput in = idle();
    s.clock = SWC_LOOP_SECONDS - 0.01;
    synthwave_city_step(&s, &in, ARC_DT);
    CHECK(s.clock >= 0.0 && s.clock < 0.01);
    CHECK(swc_loop_fraction(&s) >= 0.0 && swc_loop_fraction(&s) < 1.0);
    /* 16 bars of 4 beats at 105 BPM */
    CHECK_NEAR(swc_music_seconds(), 64.0 * 60.0 / 105.0, 1e-9);
    swc_sync_beat(&s, swc_music_seconds() + 0.25);
    CHECK_NEAR(s.beat_time, 0.25, 1e-9);
    s.beat_time = 60.0 / 105.0 * 3.0; /* exactly on a beat */
    CHECK(swc_beat_phase(&s) < 1e-6 || swc_beat_phase(&s) > 1.0 - 1e-6);
    CHECK(swc_beat_pulse(&s) > 0.99 || swc_beat_pulse(&s) < 0.01 + exp(-5.0));
    s.beat_time = 60.0 / 105.0 * 3.5;
    CHECK(swc_beat_pulse(&s) < 0.1);
    for (int i = 0; i < 1000; i++) {
        s.beat_time += 0.0137;
        double p = swc_beat_phase(&s), u = swc_beat_pulse(&s);
        CHECK(p >= 0.0 && p < 1.0 && u > 0.0 && u <= 1.0);
    }
}

static void test_same_input_same_result(void) {
    static synthwave_city_State other;
    ArcadeMonkey m1, m2;
    synthwave_city_init(&s, 9u);
    synthwave_city_init(&other, 9u);
    arcade_monkey_init(&m1, 5u);
    arcade_monkey_init(&m2, 5u);
    for (int i = 0; i < 1500; i++) {
        ArcadeInput a, b;
        arcade_monkey_next(&m1, &a);
        arcade_monkey_next(&m2, &b);
        synthwave_city_step(&s, &a, ARC_DT);
        synthwave_city_step(&other, &b, ARC_DT);
    }
    CHECK(memcmp(&s.cam, &other.cam, sizeof(s.cam)) == 0);
    CHECK(s.distance == other.distance && s.mood == other.mood && s.view == other.view);
}

/* Random input for a long time: the state stays finite, inside the tile and out of the buildings. */
static void test_monkey_never_breaks_it(void) {
    for (unsigned seed = 1; seed <= 3; seed++) {
        ArcadeMonkey m;
        synthwave_city_init(&s, seed);
        arcade_monkey_init(&m, seed * 17u);
        for (int i = 0; i < 6000; i++) {
            ArcadeInput in;
            arcade_monkey_next(&m, &in);
            in.move_z = (float)((i / 97) % 3) - 1.0f; /* Q and E, and Shift: the monkey leaves them alone */
            in.shift = ((i / 53) & 1) != 0;
            if (i % 900 == 450)
                swc_set_pointer_lock(&s, !s.pointer_locked); /* the front-end grabs and frees the mouse */
            synthwave_city_step(&s, &in, ARC_DT);
            if (s.grab_wanted)
                swc_set_pointer_lock(&s, true);
            CHECK(isfinite(s.cam.x) && isfinite(s.cam.y) && isfinite(s.cam.z));
            CHECK(isfinite(s.cam.yaw) && isfinite(s.cam.pitch) && isfinite(s.distance));
            CHECK(s.cam.x >= 0.0 && s.cam.x < SWC_TILE && s.cam.z >= 0.0 && s.cam.z < SWC_TILE);
            CHECK(s.cam.y >= SWC_CAMERA_RADIUS - 1e-6 && s.cam.y <= SWC_MAX_ALTITUDE + 1e-6);
            CHECK(fabs(s.cam.pitch) <= SWC_MAX_PITCH + 1e-9);
            CHECK(isfinite(s.roll) && fabs(s.roll) <= 0.3);
            CHECK(s.fov >= SWC_MIN_TOUR_FOV - 1e-9 && s.fov <= SWC_FOV_DEG + 7.0 + 1e-9 + 23.0);
            /* the glide to a viewpoint is a straight one-second ease, as in the web version: it can cut a
             * corner */
            if (s.rig.phase != SWC_PHASE_EASE)
                CHECK(!camera_in_a_building());
            CHECK(!synthwave_city_is_over(&s));
        }
    }
}

static void test_big_and_bad_time_steps_are_survivable(void) {
    synthwave_city_init(&s, 1u);
    ArcadeInput in = idle();
    in.move_y = -1.0f;
    float steps[] = {0.0f, -1.0f, 0.5f, 5.0f, 1e-9f, 0.25f};
    for (size_t i = 0; i < sizeof steps / sizeof steps[0]; i++) {
        synthwave_city_step(&s, &in, steps[i]);
        CHECK(isfinite(s.cam.x) && isfinite(s.cam.y) && isfinite(s.cam.z));
        CHECK(!camera_in_a_building());
    }
}

int main(void) {
    RUN(test_init_is_sane);
    RUN(test_the_six_viewpoints_are_in_free_air);
    RUN(test_the_tour_flies_on_its_own_and_never_hits_a_building);
    RUN(test_flight_forward_reaches_top_speed);
    RUN(test_flight_does_not_depend_on_the_frame_rate);
    RUN(test_flying_into_a_wall_slides_and_never_enters);
    RUN(test_the_viewpoint_keys);
    RUN(test_mood_key_cycles);
    RUN(test_space_toggles_the_tour_and_controls_take_over);
    RUN(test_two_quick_clicks_take_over_and_a_drag_only_looks);
    RUN(test_the_wheel_zooms_the_lens_on_the_tour);
    RUN(test_new_cities_roll_short_seeds_and_start_the_tour_over);
    RUN(test_e_flies_up_and_q_down);
    RUN(test_shift_boosts_like_the_right_mouse_button);
    RUN(test_a_click_in_flight_asks_for_the_mouse_and_a_grabbed_mouse_looks);
    RUN(test_the_double_click_asks_for_the_mouse_and_a_grabbed_tour_pans_only_with_a_button);
    RUN(test_a_new_city_keeps_the_grab);
    RUN(test_dragging_looks_around_without_a_jump);
    RUN(test_an_idle_host_leaves_the_drone_flying);
    RUN(test_a_lone_space_press_hands_the_camera_back_to_the_drone);
    RUN(test_the_click_that_takes_the_camera_does_not_turn_it);
    RUN(test_wheel_scales_the_cruise_speed);
    RUN(test_clocks_loop);
    RUN(test_same_input_same_result);
    RUN(test_monkey_never_breaks_it);
    RUN(test_big_and_bad_time_steps_are_survivable);
    return TEST_SUMMARY();
}
