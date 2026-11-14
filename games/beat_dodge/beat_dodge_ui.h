/* The pieces of the front-end that Beat Dodge and Beat Catch share: song select screen, music playback and
 * the drawing of the playfield. They are IMPLEMENTED in beat_dodge_game.c (the only file of the two games
 * that may include raylib) and used by beat_catch_game.c through this header, which therefore holds no raylib
 * types. */
#ifndef BEAT_DODGE_UI_H
#define BEAT_DODGE_UI_H

#include <stdbool.h>

#include "arcade/game.h"
#include "beat_dodge_logic.h"

/* How the playfield looks: Beat Dodge has solid obstacles and a triangle, Beat Catch has hollow rings and a
 * cup. */
typedef enum bd_UiStyle { BD_UI_DODGE, BD_UI_CATCH } bd_UiStyle;

/* Runs the song select screen of `menu->kind` (the menu must be bd_menu_init'ed by the caller and keeps its
 * selection between calls). Returns true when a song and chart were chosen (menu->song, menu->chart), false
 * when the player backs out or closes the window. */
bool bd_ui_select_song(const GameContext *ctx, const char *title, bd_Menu *menu, const bd_SongList *list);

/* Smoke runs have no input: draws the select screen for a moment and writes <screenshot>_select.png. */
void bd_ui_preview_select(const GameContext *ctx, const char *title, bd_ChartKind kind,
                          const bd_SongList *list);

/* Music: the song's melody and drums rendered by the synth and played from memory. Only one song plays at a
 * time. start returns false when there is no audio device or no melody (then the caller may use a metronome).
 */
bool bd_ui_music_start(const bd_Run *run);
/* Feeds the stream; returns the song position in seconds, or -1 while there is no music or it has just begun.
 */
float bd_ui_music_time(void);
void bd_ui_music_stop(void);

/* Horizontal centre of a lane (for effects drawn by the game itself). */
float bd_ui_lane_x(int lanes, int lane);

/* Draws the playfield: backdrop, lanes with the beat pulse, the beads and ribbons still on their way and the
 * player. `pop` (0..1, 0 = nothing) draws a ring that fades at the player line in lane `pop_lane`: Beat Catch
 * uses it when a bead is caught. Heads-up display and game specific effects are drawn by the caller. */
void bd_ui_draw_field(const bd_Run *run, bd_UiStyle style, float t, int pop_lane, float pop);

#endif
