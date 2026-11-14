/* Beat Catch - PURE game logic (no raylib). Owner: Member 5
 * Hook: the mirror image of Beat Dodge. The same songs and the same falling notes, but now you CATCH them:
 * stand in the lane of every note when it reaches the line, and keep the combo alive.
 *
 * This game shares the song format, the chart check and the clock with Beat Dodge (beat_dodge_logic.h): its
 * state embeds the same bd_Run. Only the rules below are new. The charts it plays are the "catch" charts of
 * each song (made from the player voice of the MIDI file, see docs/SONGS.md). */
#ifndef BEAT_CATCH_LOGIC_H
#define BEAT_CATCH_LOGIC_H

#include "beat_dodge_logic.h"

#define BC_START_HEALTH 100
#define BC_HEALTH_PER_CLEAN_OBJECT                                                                           \
    2                          /* health gained for every object caught completely (never above the start) */
#define BC_HEALTH_PER_MISS 15  /* health lost for every object that has a missed bead */
#define BC_LONE_POINTS 10      /* a lone bead */
#define BC_RIBBON_POINTS 5     /* one bead of a ribbon */
#define BC_COMBO_STEP 10       /* the multiplier grows by 1 every BC_COMBO_STEP caught beads in a row ... */
#define BC_MAX_COMBO_BONUS 3   /* ... up to 1 + 3 = x4 */
#define BC_FINISH_PER_HEALTH 5 /* bonus points per point of health left at the end of the song */

/* Rules. Every bead is judged when it reaches the player line: it is CAUGHT when the player stands at most
 * bd_catch_tolerance() lanes away from it: 0 when its object stays in one lane (a lone bead, or a ribbon in
 * one lane), 1 when its object changes lane, so standing in the middle lane of a ribbon that visits three
 * lanes catches all of it. A caught bead scores BC_LONE_POINTS or BC_RIBBON_POINTS times the combo
 * multiplier; a missed bead resets the combo. Health works per OBJECT (a ribbon or a lone bead): an object
 * with a missed bead costs BC_HEALTH_PER_MISS once, however many of its beads are missed, and an object
 * caught completely gives BC_HEALTH_PER_CLEAN_OBJECT back. Health 0 ends the run; surviving the song adds
 * BC_FINISH_PER_HEALTH per point of health left. */
typedef struct beat_catch_State {
    bd_Run run;
    int health;
    int combo, best_combo;
    int caught, missed; /* beads */
    int missed_objects; /* objects that had at least one missed bead (each cost health once) */
    int points, max_points;
    float multiplier;  /* difficulty multiplier of the chart (not the combo) */
    bool obj_missed;   /* the object being judged has already cost health */
    float catch_flash; /* seconds left of the flash at the line where the last bead was caught */
    int catch_lane;    /* lane of that bead */
} beat_catch_State;

void beat_catch_init(beat_catch_State *s, unsigned seed);                      /* built-in practice song */
void beat_catch_use_song(beat_catch_State *s, const bd_Song *song, int chart); /* (re)start with a song */
void beat_catch_step(beat_catch_State *s, const ArcadeInput *in, float dt);
/* Follow an external clock (the music): only corrects drifts larger than 0.15 s. */
void beat_catch_sync(beat_catch_State *s, float clock_seconds);
bool beat_catch_is_over(const beat_catch_State *s);
/* Normalised: 1000 for a perfect run (every bead caught, full health) x the difficulty multiplier. */
int beat_catch_score(const beat_catch_State *s);
/* The combo multiplier the NEXT caught bead earns: 1 + min(combo / BC_COMBO_STEP, BC_MAX_COMBO_BONUS). */
int beat_catch_combo_multiplier(int combo);

#endif
