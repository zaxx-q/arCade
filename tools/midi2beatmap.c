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

/* One row of the --metrics summary: all charts with the same kind and difficulty name. */
typedef struct MetricsRow {
    char label[24];
    int charts, bursts, mismatched, unplayable;
    double lanes, beads, per_second, covered, same_lane, lazy_idle, empty5, empty10, idle_sum;
    float idle_max;
} MetricsRow;

/* --metrics: load finished song files, print one line per chart and the average over all files per kind and
 * difficulty name. The average is a plain mean over songs (the "before / after" tables in
 * games/beat_dodge/GAME.md); "mismatch" is pooled over all bursts. */
static int run_metrics(int count, char **files) {
    enum { MAX_ROWS = 8 };
    MetricsRow rows[MAX_ROWS];
    int row_count = 0;
    memset(rows, 0, sizeof(rows));
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
            const bd_Chart *ch = &g_song.charts[c];
            bd_ChartMetrics m;
            bd_chart_metrics(&g_song, c, &m);
            print_metrics_line(g_song.title, ch, &m);
            char label[24];
            snprintf(label, sizeof(label), "%s%s", ch->name, ch->kind == BD_CHART_CATCH ? " (catch)" : "");
            int k = 0;
            while (k < row_count && strcmp(rows[k].label, label) != 0)
                k++;
            if (k == MAX_ROWS)
                continue;
            if (k == row_count) {
                snprintf(rows[k].label, sizeof(rows[k].label), "%s", label);
                row_count++;
            }
            MetricsRow *r = &rows[k];
            r->charts++;
            r->lanes += ch->lanes;
            r->beads += m.beads;
            r->per_second += m.per_second;
            r->covered += m.covered;
            r->same_lane += m.same_lane;
            r->lazy_idle += m.lazy_idle;
            r->empty5 += m.empty5;
            r->empty10 += m.empty10;
            r->idle_sum += m.longest_idle;
            r->idle_max = arc_maxf(r->idle_max, m.longest_idle);
            r->bursts += m.bursts;
            r->mismatched += m.bursts_mismatched;
            r->unplayable += m.playable ? 0 : 1;
        }
    }
    printf("\naverage over %d song file(s):\n", loaded);
    printf("  %-14s %5s %7s %8s %9s %9s %9s %8s %9s %10s %8s\n", "chart", "lanes", "beads/s", "covered",
           "mismatch", "same-lane", "lazy-idle", "empty-5s", "empty-10s", "idle-lane", "playable");
    for (int k = 0; k < row_count; k++) {
        const MetricsRow *r = &rows[k];
        double n = (double)r->charts;
        printf("  %-14s %5.0f %7.2f %7.0f%% %8.0f%% %8.0f%% %8.0f%% %7.0f%% %8.0f%% %5.1f/%4.1fs %4d/%-3d\n",
               r->label, r->lanes / n, r->per_second / n, 100.0 * r->covered / n,
               r->bursts > 0 ? 100.0 * r->mismatched / r->bursts : 0.0, 100.0 * r->same_lane / n,
               100.0 * r->lazy_idle / n, 100.0 * r->empty5 / n, 100.0 * r->empty10 / n, r->idle_sum / n,
               (double)r->idle_max, r->charts - r->unplayable, r->charts);
    }
    printf("  (mismatch = pooled over all bursts, the other columns are means over charts; idle-lane = "
           "mean/max of "
           "the longest time one lane has no bead; voice overlap %.0f%% of beats)\n",
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
            printf(
                "  chart %-6s %-5s %d lanes: %4d beads in %4d objects (%3d ribbons: %3d folded, %3d moved, "
                "%3d lone beads dropped), %.2f per second, %d steps\n",
                g_song.charts[c].name, g_song.charts[c].kind == BD_CHART_CATCH ? "catch" : "dodge",
                st.lanes[c], st.beads[c], st.objects[c], st.ribbons[c], st.folded[c], st.repaired[c],
                st.dropped[c], st.per_second[c], g_song.charts[c].length);
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
