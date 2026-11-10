/* midi2beatmap: converts a MIDI file into an arCade Beat Dodge song (text file in assets/beatmaps/).
 *
 *   midi2beatmap input.mid -o assets/beatmaps/my_song.txt [--title "My Song"] [--player "Boyfriend"]
 * [--report]
 *
 * See docs/SONGS.md. Plain C99 using the same code as the game (core/midi, games/beat_dodge). */
#include <stdio.h>
#include <string.h>

#include "arcade/midi.h"
#include "beat_dodge_convert.h"

static MidiSong g_midi; /* ~400 KB: static, not on the stack */
static bd_Song g_song;

static void usage(void) {
    fputs("Usage: midi2beatmap <input.mid> -o <output.txt> [--title TEXT] [--player TRACK] [--report]\n"
          "  -o FILE        song file to write (omit with --report to only print statistics)\n"
          "  --title TEXT   song title shown in the menu (default: the input file name)\n"
          "  --player TEXT  part of the track name that is the player voice (default: boyfriend / bf / last "
          "track)\n"
          "  --report       print the statistics and the track list\n",
          stderr);
}

int main(int argc, char **argv) {
    const char *in = NULL, *out = NULL, *title = NULL, *player = NULL;
    bool report = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            out = argv[++i];
        else if (strcmp(argv[i], "--title") == 0 && i + 1 < argc)
            title = argv[++i];
        else if (strcmp(argv[i], "--player") == 0 && i + 1 < argc)
            player = argv[++i];
        else if (strcmp(argv[i], "--report") == 0)
            report = true;
        else if (argv[i][0] != '-' && in == NULL)
            in = argv[i];
        else {
            usage();
            return 2;
        }
    }
    if (!in || (!out && !report)) {
        usage();
        return 2;
    }
    if (!midi_load(in, &g_midi)) {
        fprintf(stderr, "error: %s: %s\n", in, g_midi.error);
        return 1;
    }
    if (g_midi.tempo_changes > 0)
        fprintf(stderr,
                "warning: the file changes tempo %d time(s); only the first tempo (%.1f BPM) is used\n",
                g_midi.tempo_changes, g_midi.bpm);

    char err[96];
    bd_ConvertStats st;
    if (!bd_convert_midi(&g_midi, title ? title : in, player, &g_song, &st, err, sizeof(err))) {
        fprintf(stderr, "error: %s\n", err);
        return 1;
    }
    if (report) {
        printf("%s: %.1f BPM, %d ticks/beat, %d note track(s)\n", in, g_midi.bpm, g_midi.ticks_per_beat,
               g_midi.track_count);
        for (int t = 0; t < g_midi.track_count; t++)
            printf("  track %d \"%s\": %d notes%s\n", t, g_midi.tracks[t].name, g_midi.tracks[t].note_count,
                   strcmp(g_midi.tracks[t].name, st.player_track) == 0 ? "  <- player voice" : "");
        for (int c = 0; c < g_song.chart_count; c++)
            printf("  chart %-6s %d lanes: %4d obstacles, %.2f per second, %d steps\n", g_song.charts[c].name,
                   st.lanes[c], st.obstacles[c], st.per_second[c], g_song.charts[c].length);
        printf("  melody: %d notes\n", st.melody_notes);
    }
    if (out) {
        if (!bd_song_save(out, &g_song)) {
            fprintf(stderr, "error: cannot write %s\n", out);
            return 1;
        }
        printf("wrote %s (\"%s\", %d BPM, %d charts, %d melody notes)\n", out, g_song.title, g_song.bpm,
               g_song.chart_count, g_song.note_count);
    }
    return 0;
}
