/* The layer switch: the L key walks through everything on, each layer on its own, and none, and comes back.
 */
#include <string.h>

#include "arcade_test.h"
#include "swc_layers.h"

#define STATES 6

static void test_the_key_walks_through_six_distinct_states_and_comes_back(void) {
    unsigned seen[STATES];
    unsigned m = SWC_LIVE_ALL;
    for (int i = 0; i < STATES; i++) {
        seen[i] = m;
        m = swc_layers_next(m);
    }
    CHECK_EQ_INT((int)m, (int)SWC_LIVE_ALL);
    for (int i = 0; i < STATES; i++)
        for (int j = i + 1; j < STATES; j++)
            CHECK(seen[i] != seen[j]);
}

static void test_the_states_are_everything_each_layer_alone_and_nothing(void) {
    unsigned m = SWC_LIVE_ALL;
    CHECK_EQ_INT((int)m, 15);
    m = swc_layers_next(m);
    CHECK_EQ_INT((int)m, (int)SWC_LIVE_BEAMS);
    m = swc_layers_next(m);
    CHECK_EQ_INT((int)m, (int)SWC_LIVE_AIR);
    m = swc_layers_next(m);
    CHECK_EQ_INT((int)m, (int)SWC_LIVE_STREET);
    m = swc_layers_next(m);
    CHECK_EQ_INT((int)m, (int)SWC_LIVE_HOLO);
    m = swc_layers_next(m);
    CHECK_EQ_INT((int)m, 0);
}

static void test_bits_outside_the_known_layers_are_ignored(void) {
    CHECK_EQ_INT((int)swc_layers_next(SWC_LIVE_ALL | 0x80u), (int)swc_layers_next(SWC_LIVE_ALL));
    CHECK_EQ_INT((int)swc_layers_next(0x40u), (int)SWC_LIVE_ALL); /* nothing known set: the "none" state */
}

static void test_a_mask_a_harness_made_still_moves_on(void) {
    CHECK_EQ_INT((int)swc_layers_next(SWC_LIVE_BEAMS | SWC_LIVE_STREET), (int)SWC_LIVE_BEAMS);
    CHECK_EQ_INT((int)swc_layers_next(SWC_LIVE_AIR | SWC_LIVE_HOLO | SWC_LIVE_STREET), (int)SWC_LIVE_BEAMS);
}

static void test_each_state_has_its_own_label_that_says_what_is_drawn(void) {
    CHECK(strcmp(swc_layers_label(SWC_LIVE_ALL), "ALL LAYERS ON") == 0);
    CHECK(strcmp(swc_layers_label(0u), "ALL LAYERS OFF") == 0);
    CHECK(strcmp(swc_layers_label(SWC_LIVE_BEAMS), "ONLY BEAMS") == 0);
    CHECK(strcmp(swc_layers_label(SWC_LIVE_AIR), "ONLY AIR") == 0);
    CHECK(strcmp(swc_layers_label(SWC_LIVE_STREET), "ONLY STREET") == 0);
    CHECK(strcmp(swc_layers_label(SWC_LIVE_HOLO), "ONLY HOLOGRAMS") == 0);
    CHECK(strcmp(swc_layers_label(SWC_LIVE_BEAMS | SWC_LIVE_STREET), "ONLY BEAMS + STREET") == 0);
    CHECK(strcmp(swc_layers_label(SWC_LIVE_AIR | SWC_LIVE_STREET | SWC_LIVE_HOLO),
                 "ONLY AIR + STREET + HOLOGRAMS") == 0);
}

int main(void) {
    RUN(test_the_key_walks_through_six_distinct_states_and_comes_back);
    RUN(test_the_states_are_everything_each_layer_alone_and_nothing);
    RUN(test_bits_outside_the_known_layers_are_ignored);
    RUN(test_a_mask_a_harness_made_still_moves_on);
    RUN(test_each_state_has_its_own_label_that_says_what_is_drawn);
    return TEST_SUMMARY();
}
