/* midi2beatmap: converts a MIDI file into an arCade Beat Dodge song (text file in assets/beatmaps/).
 *
 *   midi2beatmap input.mid -o assets/beatmaps/my_song.txt [--title "My Song"] [--player "Boyfriend"]
 * [--report]
 *   midi2beatmap --metrics song.txt [more.txt ...]     measure existing song files (averages per difficulty)
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
          "       midi2beatmap --metrics <song.txt> [<song.txt> ...]\n"
          "  --metrics      measure finished song files and print the average per difficulty (see "
          "docs/SONGS.md)\n"
          "  -o FILE        song file to write (omit with --report to only print statistics)\n"
          "  --title TEXT   song title shown in the menu (default: the input file name)\n"
          "  --player TEXT  part of the track name that is the player voice (default: boyfriend / bf / last "
          "track)\n"
          "  --report       print the statistics and the track list\n",
          stderr);
}

static void print_metrics_line(const char *label, const bd_Chart *c, const bd_ChartMetrics *m) {
    printf("  %-14s %-6s %d lanes %5d beads %5.2f/s covered %3.0f%% bursts %3d mismatched %3.0f%% same-lane "
           "%2.0f%% "
           "lazy-idle %3.0f%% empty5 %3.0f%% empty10 %3.0f%%\n",
           label, c->name, c->lanes, m->beads, m->per_second, 100.0f * m->covered, m->bursts,
           m->bursts > 0 ? 100.0f * (float)m->bursts_mismatched / (float)m->bursts : 0.0f,
           100.0f * m->same_lane, 100.0f * m->lazy_idle, 100.0f * m->empty5, 100.0f * m->empty10);
}

/* --metrics: load finished song files, print one line per chart and the average over all files per difficulty
 * name. The average is a plain mean over songs (the "before / after" tables in games/beat_dodge/GAME.md). */
static int run_metrics(int count, char **files) {
    enum { MAX_KEYS = 8 };
    char names[MAX_KEYS][12];
    double sum[MAX_KEYS][9];
    int songs[MAX_KEYS], pooled_bursts[MAX_KEYS], pooled_bad[MAX_KEYS], keys = 0;
    memset(sum, 0, sizeof(sum));
    memset(songs, 0, sizeof(songs));
    memset(pooled_bursts, 0, sizeof(pooled_bursts));
    memset(pooled_bad, 0, sizeof(pooled_bad));
    double overlap = 0.0;
    int loaded = 0;
    for (int f = 0; f < count; f++) {
        if (!bd_song_load(files[f], &g_song)) {
            fprintf(stderr, "error: cannot load %s (missing or invalid)\n", files[f]);
            return 1;
        }
        loaded++;
        overlap += bd_song_voice_overlap(&g_song);
        for (int c = 0; c < g_song.chart_count; c++) {
            bd_ChartMetrics m;
            bd_chart_metrics(&g_song, c, &m);
            print_metrics_line(g_song.title, &g_song.charts[c], &m);
            int k = 0;
            while (k < keys && strcmp(names[k], g_song.charts[c].name) != 0)
                k++;
            if (k == keys && keys < MAX_KEYS)
                snprintf(names[keys++], sizeof(names[0]), "%s", g_song.charts[c].name);
            if (k >= MAX_KEYS)
                continue;
            const double v[9] = {(double)g_song.charts[c].lanes,
                                 m.per_second,
                                 m.covered,
                                 m.bursts > 0 ? (double)m.bursts_mismatched / (double)m.bursts : 0.0,
                                 m.same_lane,
                                 m.lazy_idle,
                                 m.empty5,
                                 m.empty10,
                                 (double)m.beads};
            for (int i = 0; i < 9; i++)
                sum[k][i] += v[i];
            songs[k]++;
            pooled_bursts[k] += m.bursts;
            pooled_bad[k] += m.bursts_mismatched;
        }
    }
    printf("\naverage over %d song file(s):\n", loaded);
    printf("  %-6s %5s %7s %8s %9s %10s %9s %9s %8s %8s\n", "chart", "lanes", "beads/s", "covered",
           "mismatch", "same-lane", "lazy-idle", "empty-5s", "empty-10s", "bursts");
    for (int k = 0; k < keys; k++) {
        double n = (double)songs[k];
        printf("  %-6s %5.0f %7.2f %7.0f%% %8.0f%% %9.0f%% %8.0f%% %8.0f%% %7.0f%% %8d\n", names[k],
               sum[k][0] / n, sum[k][1] / n, 100.0 * sum[k][2] / n,
               pooled_bursts[k] > 0 ? 100.0 * pooled_bad[k] / pooled_bursts[k] : 0.0, 100.0 * sum[k][4] / n,
               100.0 * sum[k][5] / n, 100.0 * sum[k][6] / n, 100.0 * sum[k][7] / n, pooled_bursts[k]);
    }
    printf("  (mismatch = pooled over all bursts; the other columns are means over songs; voice overlap "
           "%.0f%% of "
           "beats)\n",
           loaded > 0 ? 100.0 * overlap / loaded : 0.0);
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 3 && strcmp(argv[1], "--metrics") == 0)
        return run_metrics(argc - 2, argv + 2);
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
        for (int c = 0; c < g_song.chart_count; c++) {
            printf("  chart %-6s %d lanes: %4d obstacles, %.2f per second, %d steps\n", g_song.charts[c].name,
                   st.lanes[c], st.obstacles[c], st.per_second[c], g_song.charts[c].length);
            bd_ChartMetrics m;
            bd_chart_metrics(&g_song, c, &m);
            print_metrics_line("", &g_song.charts[c], &m);
        }
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
