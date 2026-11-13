/* SPDX-License-Identifier: MPL-2.0 */
/* The instrument on the host: the yoga layer alone (pitch, binaural offset, pulse rate), the programs through the
 * whole engine (garden, echo, plate), the smoothness of a journey's crossfades, and WAVs to listen to.
 *   yoga_test test
 *   yoga_test wav OUT.wav PROGRAM MINUTES iso|bin [SEED]
 * Built by tests/run_tests.sh (cc, no toolchain). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "plat.h"
#include "amb.c"
#include "yoga_program.c"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st, (void)d1, (void)d2; }

#define SR 44100
#define APP_GAIN_FULL 2730u                 /* app.c: MASTER full, with headroom (app_render) */
enum { BLOCK = 256, FRAME_BLOCKS = 3 };          /* the UI's frame: ~17 ms */
static int failures;
#define CHECK(c, ...) do { int ok_ = (c); printf(ok_ ? "  ok    " : "  FAIL  "); printf(__VA_ARGS__); printf("\n"); \
    if (!ok_) failures++; } while (0)

typedef struct {
    float *l, *r;
    size_t n, cap;
    float max_beat;
    int done_at;                                 /* frame the program finished, -1 never */
} take_t;

static void take_free(take_t *t) { free(t->l); free(t->r); }

/* a program through the whole engine, until it is done (and tail_s more), or limit_s */
static void run(take_t *tk, const yp_settings_t *s, uint32_t seed, double limit_s, double tail_s)
{
    int32_t buf[2 * BLOCK];
    double ms = 0.0;
    unsigned b = 0;
    yp_status_t st;
    memset(tk, 0, sizeof *tk);
    tk->cap = (size_t)(limit_s * SR) + BLOCK;
    tk->l = calloc(tk->cap, sizeof(float));
    tk->r = calloc(tk->cap, sizeof(float));
    tk->done_at = -1;
    en_init();
    yoga_dsp_init();
    yp_init();
    yp_start(s, seed, 0);
    while (tk->n + BLOCK <= tk->cap) {
        if (b++ % FRAME_BLOCKS == 0)
            yp_update(s, (uint32_t)ms);
        en_render(buf, BLOCK, APP_GAIN_FULL);
        for (unsigned i = 0; i < BLOCK; i++) {
            tk->l[tk->n + i] = (float)buf[2 * i] / 8388608.0f;
            tk->r[tk->n + i] = (float)buf[2 * i + 1] / 8388608.0f;
        }
        tk->n += BLOCK;
        ms += 1000.0 * BLOCK / SR;
        yp_status(&st);
        if (st.beat > tk->max_beat)
            tk->max_beat = st.beat;
        if (st.state == YS_DONE && tk->done_at < 0)
            tk->done_at = (int)tk->n;
        if (tk->done_at >= 0 && (double)(tk->n - (size_t)tk->done_at) > tail_s * SR)
            break;
    }
}

static void wav(const char *path, const take_t *t)
{
    FILE *f = fopen(path, "wb");
    uint32_t data = (uint32_t)(t->n * 4), rate = SR, br = SR * 4, riff = 36 + data, fmt = 16;
    uint16_t pcm = 1, ch = 2, align = 4, bits = 16;
    if (!f) {
        perror(path);
        exit(1);
    }
    fwrite("RIFF", 1, 4, f), fwrite(&riff, 4, 1, f), fwrite("WAVEfmt ", 1, 8, f), fwrite(&fmt, 4, 1, f);
    fwrite(&pcm, 2, 1, f), fwrite(&ch, 2, 1, f), fwrite(&rate, 4, 1, f), fwrite(&br, 4, 1, f);
    fwrite(&align, 2, 1, f), fwrite(&bits, 2, 1, f), fwrite("data", 1, 4, f), fwrite(&data, 4, 1, f);
    for (size_t i = 0; i < t->n; i++) {
        int16_t v[2] = {(int16_t)lrintf(fmaxf(-1.0f, fminf(1.0f, t->l[i])) * 32767.0f),
                        (int16_t)lrintf(fmaxf(-1.0f, fminf(1.0f, t->r[i])) * 32767.0f)};
        fwrite(v, 2, 2, f);
    }
    fclose(f);
}

static double power_at(const float *x, size_t len, double hz)
{
    double w = 2 * M_PI * hz / SR, c = 2 * cos(w), s1 = 0, s2 = 0;
    for (size_t i = 0; i < len; i++) {
        double h = 0.5 - 0.5 * cos(2 * M_PI * i / (len - 1)), s0 = x[i] * h + c * s1 - s2;
        s2 = s1, s1 = s0;
    }
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}
static double peak_hz(const float *x, size_t len, double lo, double hi, double step)
{
    double best = lo, bp = -1;
    for (double f = lo; f <= hi; f += step) {
        double p = power_at(x, len, f);
        if (p > bp)
            bp = p, best = f;
    }
    return best;
}
static double rms(const float *x, size_t n)
{
    double s = 0;
    for (size_t i = 0; i < n; i++)
        s += (double)x[i] * x[i];
    return sqrt(s / (double)(n ? n : 1));
}

/* the yoga layer alone, at fixed targets, for secs seconds (after 6 s to settle) */
static void layer(float *l, float *r, size_t n, const yoga_target_t *t)
{
    float ml[BLOCK], mr[BLOCK], rs[BLOCK], es[BLOCK];
    yoga_dsp_init();
    yoga_dsp_set(t);
    for (size_t at = 0, settle = 6 * SR; at < settle + n; at += BLOCK) {
        memset(ml, 0, sizeof ml), memset(mr, 0, sizeof mr), memset(rs, 0, sizeof rs), memset(es, 0, sizeof es);
        (void)yoga_dsp_garden(BLOCK);
        yoga_dsp_layer(ml, mr, rs, es, BLOCK);
        for (unsigned i = 0; i < BLOCK; i++)
            if (at + i >= settle && at + i - settle < n)
                l[at + i - settle] = ml[i], r[at + i - settle] = mr[i];
    }
}

static void test_layer(void)
{
    const size_t n = 8 * SR;
    float *l = malloc(n * sizeof(float)), *r = malloc(n * sizeof(float));
    yoga_target_t t;
    printf("The yoga layer: pitch, binaural offset, pulse\n");
    for (int c = 0; c < YP_NCARRIER; c++) {
        double f = YP_CARRIER[c], fl, fr;
        memset(&t, 0, sizeof t);
        t.carrier[0] = t.carrier[1] = (float)f;
        t.beat = 5.0f, t.detune = 0.2f, t.breath = 0.09f, t.level = 1.0f, t.drone = 0.6f, t.mode = YOGA_BIN;
        layer(l, r, n, &t);
        fl = peak_hz(l, n, f - 3, f + 3, 0.05);
        fr = peak_hz(r, n, f - 3, f + 8, 0.05);
        CHECK(fabs(fl - f) <= 0.3, "BIN %3.0f Hz: left %.2f Hz", f, fl);
        CHECK(fabs(fr - fl - 5.0) <= 0.2, "BIN %3.0f Hz: right - left = %.2f Hz", f, fr - fl);
    }
    memset(&t, 0, sizeof t);
    t.carrier[0] = t.carrier[1] = 432.0f;
    t.beat = 6.0f, t.breath = 0.09f, t.level = 1.0f, t.drone = 0.6f, t.iso_depth = 0.6f, t.mode = YOGA_ISO;
    layer(l, r, n, &t);
    {
        float *env = malloc(n * sizeof(float));
        double lp = 0, mean = 0;
        for (size_t i = 0; i < n; i++)
            lp += (fabs(l[i]) - lp) * 0.01, env[i] = (float)lp, mean += lp;
        mean /= (double)n;
        for (size_t i = 0; i < n; i++)
            env[i] -= (float)mean;
        double rate = peak_hz(env, n, 1.0, 12.0, 0.02);
        CHECK(fabs(rate - 6.0) <= 0.15, "ISO 432 Hz: pulse %.2f Hz", rate);
        CHECK(fabs(peak_hz(l, n, 425, 440, 0.05) - peak_hz(r, n, 425, 440, 0.05)) < 0.1, "ISO: both ears the same pitch");
        free(env);
    }
    /* a crossfade between the two drones keeps the loudness */
    {
        double a, b, m;
        memset(&t, 0, sizeof t);
        t.carrier[0] = 174.0f, t.carrier[1] = 396.0f, t.level = 1.0f, t.drone = 0.6f, t.breath = 0.09f;
        t.blend = 0.0f;
        layer(l, r, n, &t);
        a = rms(l, n);
        t.blend = 0.5f;
        layer(l, r, n, &t);
        m = rms(l, n);
        CHECK(peak_hz(l, n, 170, 178, 0.05) > 173 && power_at(l, n, 396) > power_at(l, n, 300) * 100,
              "blend 0.5: both carriers sound");
        t.blend = 1.0f;
        layer(l, r, n, &t);
        b = rms(l, n);
        CHECK(fabs(20 * log10(m / a)) < 1.5 && fabs(20 * log10(b / a)) < 1.5,
              "crossfade keeps the loudness: %.2f / %.2f / %.2f dB", 0.0, 20 * log10(m / a), 20 * log10(b / a));
    }
    free(l), free(r);
}

/* the handpan: a note alone, through the layer */
static void hand_take(float *l, size_t n, const yp_settings_t *s, int key, int vel)
{
    float ml[BLOCK], mr[BLOCK], rs[BLOCK], es[BLOCK];
    yoga_target_t t;
    memset(&t, 0, sizeof t);
    t.hand_level = 1.0f;
    yoga_dsp_init();
    yoga_dsp_set(&t);
    for (int pre = 0; pre < SR / BLOCK; pre++) {            /* the level settled first (it glides in 0.2 s) */
        memset(ml, 0, sizeof ml), memset(mr, 0, sizeof mr), memset(rs, 0, sizeof rs), memset(es, 0, sizeof es);
        yoga_dsp_layer(ml, mr, rs, es, BLOCK);
    }
    yp_hand(s, key, vel);
    for (size_t at = 0; at < n; at += BLOCK) {
        memset(ml, 0, sizeof ml), memset(mr, 0, sizeof mr), memset(rs, 0, sizeof rs), memset(es, 0, sizeof es);
        yoga_dsp_layer(ml, mr, rs, es, BLOCK);
        for (unsigned i = 0; i < BLOCK && at + i < n; i++)
            l[at + i] = ml[i];
    }
}
static void test_hand(void)
{
    const size_t n = 6 * SR;
    float *l = malloc(n * sizeof(float));
    yp_settings_t s;
    double soft, hard, early, late, peak = 0;
    printf("The handpan: pitch, touch, ring\n");
    yp_defaults(&s);
    yp_init();
    for (int c = 0; c < YP_NCARRIER; c++) {
        double want;
        s.program = (uint8_t)c;                              /* idle: the ding follows the program's carrier */
        want = YP_CARRIER[c];
        while (want > 280)
            want /= 2;
        hand_take(l, n, &s, 0, 100);
        double f = peak_hz(l + SR / 10, SR, want - 4, want + 4, 0.05);
        CHECK(fabs(f - want) < 0.5, "ding on %3d Hz: %.2f Hz (%.2f expected)", YP_CARRIER[c], f, want);
    }
    s.program = YP_HERZ;                                     /* ding 216 Hz, Kurd: the fifth above is key 1 */
    hand_take(l, n, &s, 1, 100);
    CHECK(fabs(peak_hz(l + SR / 10, SR, 315, 335, 0.05) - 216 * pow(2, 7 / 12.0)) < 0.6, "key 1: the fifth above the ding");
    hand_take(l, n, &s, 9, 100);
    CHECK(fabs(peak_hz(l + SR / 10, SR, 640, 655, 0.05) - 216 * pow(2, 19 / 12.0)) < 0.8, "key 9: an octave above key 1");
    {                                                        /* the last white key: a tak, short */
        double first, later;
        hand_take(l, n, &s, YP_HAND_TAK, 110);
        first = rms(l, SR / 20);
        later = rms(l + SR / 2, SR / 10);
        CHECK(first > 0.01 && later < first * 0.01, "the tak: a knock, gone in half a second (%.0f dB)",
              20 * log10(later / first + 1e-12));
    }
    s.x[YX_HSCALE] = 6;                                      /* Akebono: key 3 the octave above the ding */
    CHECK(fabsf(yp_hand_hz(&s, 3) - 432.0f) < 0.01f && fabsf(yp_hand_hz(&s, 2) - 216.0f * powf(2, 8 / 12.0f)) < 0.01f,
          "AKEBONO: key 2 the minor sixth, key 3 the octave");
    s.x[YX_HSCALE] = 0;
    hand_take(l, n, &s, 3, 30);
    soft = rms(l, SR / 2);
    hand_take(l, n, &s, 3, 120);
    hard = rms(l, SR / 2);
    early = rms(l, SR / 2);
    late = rms(l + 4 * SR, SR / 2);
    for (size_t i = 0; i < n; i++)
        peak = fmax(peak, fabs(l[i]));
    CHECK(hard > soft * 2.0, "a harder touch is louder (%.1f dB)", 20 * log10(hard / soft));
    CHECK(late < early * 0.3 && late > early * 0.005, "it rings and fades (%.1f dB after 4 s)", 20 * log10(late / early));
    CHECK(peak < 0.6 && peak > 0.15, "one note's peak %.3f", peak);
    s.x[YX_HDYN] = 0;
    hand_take(l, n, &s, 3, 30);
    soft = rms(l, SR / 2);
    CHECK(fabs(20 * log10(hard / soft)) < 0.5, "DYNAMIK 0: every touch alike");
    {                                                        /* idle, at power-on: the hall carries it */
        int32_t buf[2 * BLOCK];
        double tail[2];
        yp_defaults(&s);
        for (int room = 0; room < 2; room++) {
            yoga_target_t t;
            en_init();
            yoga_dsp_init();
            yp_init();
            if (room)
                yp_room(&s);
            memset(&t, 0, sizeof t);
            t.hand_level = 1.0f, t.hand_space = 0.5f;
            yoga_dsp_set(&t);
            yp_hand(&s, 4, 100);
            for (size_t at = 0; at < 2 * SR; at += BLOCK) {
                en_render(buf, BLOCK, APP_GAIN_FULL);
                if (at <= SR / 4 && at + BLOCK > SR / 4)
                    yoga_dsp_hand_silence();                 /* the note stops after 1/4 s: what rings is the room */
            }
            tail[room] = 0;
            for (int i = 0; i < 64; i++) {
                en_render(buf, BLOCK, APP_GAIN_FULL);
                for (int j = 0; j < BLOCK; j++)
                    tail[room] = fmax(tail[room], fabs((float)buf[2 * j] / 8388608.0f));
            }
        }
        (void)tail[0];                                       /* (the engine's own defaults already carry some hall) */
        CHECK(tail[1] > 0.003, "idle: the hall carries a note from power-on (tail %.4f)", tail[1]);
    }
    free(l);
}

/* the mutes: a fade, not a cut; all of it silences both layers */
static void test_mute(void)
{
    yp_settings_t s;
    int32_t buf[2 * BLOCK];
    double ms = 0, before = 0, during = 0, after = 0, step = 0, prev = 0;
    unsigned b = 0;
    printf("Mutes\n");
    yp_defaults(&s);
    s.program = YP_HERZ, s.duration = 1;
    en_init(), yoga_dsp_init(), yp_init(), yp_mute(0);
    yp_start(&s, 5, 0);
    for (int sec = 0; sec < 70; sec++)
        for (int k = 0; k < SR / BLOCK; k++, b++) {
            if (sec == 50 && k == 0)
                yp_mute(1u << YM_ALL);
            if (b % FRAME_BLOCKS == 0)
                yp_update(&s, (uint32_t)ms);
            en_render(buf, BLOCK, APP_GAIN_FULL);
            ms += 1000.0 * BLOCK / SR;
            for (int j = 0; j < BLOCK; j++) {
                double x = buf[2 * j] / 8388608.0;
                if (sec >= 45 && sec < 50)
                    before += x * x;
                if (sec >= 50 && sec < 52)
                    step = fmax(step, fabs(x - prev));
                if (sec >= 65)
                    after += x * x;
                prev = x;
            }
            (void)during;
        }
    before = sqrt(before / (5.0 * SR));
    after = sqrt(after / (5.0 * SR));
    CHECK(after < before * 0.1, "ALLES stumm: %.1f dB quieter after the fade and the hall", 20 * log10(after / before));
    CHECK(step < 0.15, "the fade has no click (largest step %.4f)", step);
    yp_mute(0);
}

/* one frame of the program: the sequencer runs in yp_update */
static void seq_run(const yp_settings_t *s, double *ms, double until)
{
    int32_t buf[2 * BLOCK];
    while (*ms < until) {
        yp_update(s, (uint32_t)*ms);
        for (int k = 0; k < FRAME_BLOCKS; k++)
            en_render(buf, BLOCK, APP_GAIN_FULL);
        *ms += 1000.0 * BLOCK * FRAME_BLOCKS / SR;
    }
}

/* the sequencer: recording, the grid, the next pattern, mutes, the end of a program */
static void test_seq(void)
{
    yp_settings_t s;
    seq_status_t st;
    double ms = 0, step;
    printf("Sequencer\n");
    yp_defaults(&s);
    s.program = YP_HERZ, s.duration = 0;
    s.bowls = 0, s.bells = 0, s.pad = 0, s.bass = 0, s.x[YX_HINST] = YI_KALIMBA;
    en_init(), yoga_dsp_init(), yp_init(), yp_mute(0), seq_defaults();
    CHECK(seq_record() == -1, "no recording before a program sounds");
    seq_key(2);
    seq_status(&st);
    CHECK(st.cur == 2 && !st.playing, "a pattern chosen before the start waits");
    seq_key(2);
    seq_status(&st);
    CHECK(st.cur == -1, "pressed again: none");
    sq.tempo = 60, sq.pat[0].len = 8, sq.pat[1].len = 4;
    step = 500.0;
    yp_start(&s, 11, 0);
    seq_run(&s, &ms, 1000.0);
    seq_status(&st);
    CHECK(!st.playing, "no pattern chosen: the program runs alone");

    CHECK(seq_record() == 1, "REC starts the first pattern");
    seq_status(&st);
    CHECK(st.playing && st.cur == 0, "it plays");
    seq_run(&s, &ms, ms + 0.1 * step);       /* early in a step: rounded back */
    yp_hand(&s, 3, 100);
    CHECK(sq.pat[0].n == 1 && !(sq.pat[0].ev[0].vel & SEQ_FRESH), "a note early in its step keeps the step");
    seq_run(&s, &ms, ms + 0.6 * step);       /* late in the next: rounded on, not played twice */
    seq_status(&st);
    yp_hand(&s, 5, 100);
    CHECK(sq.pat[0].n == 2 && sq.pat[0].ev[1].step == st.pos + 1 && (sq.pat[0].ev[1].vel & SEQ_FRESH),
          "a note late in its step goes to the next (step %d)", sq.pat[0].ev[1].step);
    seq_run(&s, &ms, ms + step);
    CHECK(yoga_dsp_hand_voices() == 2, "and sounds once on this pass (%d voices)", yoga_dsp_hand_voices());
    yp_mute(1u << YM_DRONE);
    yp_mute(0);
    CHECK(sq.pat[0].n == 3, "a mute and its return on one step keep only the last");
    yp_mute(1u << YM_BREATH);
    CHECK(sq.pat[0].n == 4, "a mute is kept");
    int before = sq.pat[0].n;
    for (int i = 0; i < SEQ_EVENTS + 8; i++)
        yp_hand(&s, i % 16, 90);
    seq_status(&st);
    CHECK(sq.pat[0].n <= before + 16 && sq.pat[0].n <= SEQ_EVENTS, "one step holds each key once");
    CHECK(seq_record() == 0, "REC again: played, not recorded");
    int kept = sq.pat[0].n;
    for (int i = 0; i < SEQ_N; i++)
        for (uint32_t j = 0; j < sq.pat[i].n; j++)
            if (sq.pat[i].ev[j].vel & SEQ_FRESH)
                kept = -1;
    CHECK(kept > 0, "no mark left behind");
    yp_hand(&s, 9, 100);
    CHECK(sq.pat[0].n == (uint32_t)kept, "nothing kept once REC is off");

    /* the mute comes back from the pattern (rule 2: a press lasts until the pattern's own) */
    yp_mute(0);
    seq_run(&s, &ms, ms + 8.0 * step + 100.0);
    CHECK(yp_muted() >> YM_BREATH & 1u, "the pattern's mute overrides the hand on its next pass");
    /* and it resets nothing else (rule 1) */
    yp_mute(yp_muted() | 1u << YM_PADS);
    seq_run(&s, &ms, ms + 8.0 * step + 100.0);
    CHECK(yp_muted() >> YM_PADS & 1u, "a part the pattern does not name stays as it is");
    yp_mute(0);

    /* the next pattern: only at the end */
    seq_key(1);
    seq_status(&st);
    CHECK(st.cur == 0 && st.next == 1, "the next one is marked");
    seq_key(2);
    seq_status(&st);
    CHECK(st.next == 2, "another press replaces it");
    seq_key(2);
    seq_status(&st);
    CHECK(st.next == SEQ_NONE, "the same press takes it back");
    seq_key(1);
    int switched = 0;
    for (int i = 0; i < 90 && !switched; i++) {
        int before_pos;
        seq_status(&st);
        before_pos = st.pos;
        seq_run(&s, &ms, ms + 100.0);
        seq_status(&st);
        if (st.cur == 1) {
            switched = before_pos == sq.pat[0].len - 1 && st.pos == 0;
            break;
        }
    }
    CHECK(switched, "the change comes at the pattern's end");
    seq_run(&s, &ms, ms + 3.0 * 4.0 * step);
    seq_status(&st);
    CHECK(st.cur == 1 && st.playing, "and it repeats");
    seq_key(1);
    seq_status(&st);
    CHECK(st.next == SEQ_STOP, "the lit one again: it ends");
    seq_run(&s, &ms, ms + 5.0 * step);
    seq_status(&st);
    CHECK(!st.playing && st.cur == -1, "at its end");
    seq_key(0);
    seq_status(&st);
    CHECK(st.playing && st.cur == 0, "chosen while the program runs, it starts now");

    /* the wake-up: the pattern fades, nothing is recorded */
    seq_record();
    yp_finish();
    seq_run(&s, &ms, ms + 200.0);
    seq_status(&st);
    CHECK(!st.rec, "the wake-up ends the recording");
    CHECK(seq_record() == -1, "and takes no new one");
    seq_run(&s, &ms, ms + (double)SEQ_FADE + 500.0);
    seq_status(&st);
    CHECK(!st.playing && st.cur == 0, "the pattern has faded; it stays chosen for the next start");
    yp_stop();
    seq_run(&s, &ms, ms + 2500.0);
    yp_start(&s, 12, (uint32_t)ms);
    seq_run(&s, &ms, ms + 100.0);
    seq_status(&st);
    CHECK(st.playing && st.pos == 0, "the next start plays it from the top");
    int lit = 0;
    for (int i = 0; i < 250; i++) {
        seq_run(&s, &ms, ms + 20.0);
        seq_status(&st);
        lit |= st.keys;
    }
    CHECK(lit >> 3 & 1, "the key a pattern plays lights (keys %04x)", lit);
    seq_clear(0);
    CHECK(sq.pat[0].n == 0, "cleared");
    seq_run(&s, &ms, ms + 300.0);
    seq_status(&st);
    CHECK(st.keys == 0, "and the lights go out");
    yp_stop();
    seq_run(&s, &ms, ms + 2500.0);
    seq_status(&st);
    CHECK(!st.playing, "a stop ends it");
    sq.tempo = 10;
    CHECK(!seq_valid(), "a bad store is refused");
    seq_defaults();
    CHECK(seq_valid() && sizeof(seq_store_t) < PLAT_STORE_MAX, "the store fits (%zu B)", sizeof(seq_store_t));
    yp_mute(0);
    seq.cur = -1;                            /* the tests after it run without a pattern */
}

/* the other instruments: their register, their ring, the Koshi's breeze */
static void test_instruments(void)
{
    const size_t n = 8 * SR;
    float *l = malloc(n * sizeof(float));
    yp_settings_t s;
    double hand_late, bowl_late;
    printf("The instruments\n");
    yp_defaults(&s);
    s.program = YP_HERZ;                                     /* ding 216 Hz */
    yp_init();
    s.x[YX_HINST] = YI_KALIMBA;
    hand_take(l, n, &s, 0, 100);
    CHECK(fabs(peak_hz(l + SR / 20, SR / 2, 425, 440, 0.1) - 432.0) < 1.0, "KALIMBA: an octave over the ding");
    CHECK(rms(l + 4 * SR, SR / 2) < rms(l, SR / 2) * 0.05, "KALIMBA: a short pluck");
    s.x[YX_HINST] = YI_HANDPAN;
    hand_take(l, n, &s, 2, 100);
    hand_late = rms(l + 6 * SR, SR) / rms(l, SR / 2);
    s.x[YX_HINST] = YI_BOWLS;
    hand_take(l, n, &s, 2, 100);
    bowl_late = rms(l + 6 * SR, SR) / rms(l, SR / 2);
    CHECK(bowl_late > hand_late * 2.0, "SCHALEN ring longer than the handpan (%.1f vs %.1f dB at 6 s)",
          20 * log10(bowl_late), 20 * log10(hand_late));
    s.x[YX_HINST] = YI_TONGUE;
    hand_take(l, n, &s, 0, 100);
    CHECK(fabs(peak_hz(l + SR / 20, SR / 2, 210, 222, 0.05) - 216.0) < 0.5, "ZUNGE: the ding's pitch");
    {                                                        /* the Koshi: one key, several rods over a second */
        yoga_target_t t;
        int most = 0;
        float ml[BLOCK], mr[BLOCK], rs[BLOCK], es[BLOCK];
        s.x[YX_HINST] = YI_KOSHI;
        memset(&t, 0, sizeof t);
        t.hand_level = 1.0f;
        yoga_dsp_init();
        yoga_dsp_set(&t);
        yp_hand(&s, 5, 100);
        for (int f = 0; f < 90; f++) {                     /* 1.5 s of frames */
            yp_update(&s, (uint32_t)(f * 17));
            for (int b = 0; b < 3; b++) {
                memset(ml, 0, sizeof ml), memset(mr, 0, sizeof mr), memset(rs, 0, sizeof rs), memset(es, 0, sizeof es);
                yoga_dsp_layer(ml, mr, rs, es, BLOCK);
            }
            if (yoga_dsp_hand_voices() > most)
                most = yoga_dsp_hand_voices();
        }
        CHECK(most >= 3, "KOSHI: one key lets %d rods ring", most);
    }
    free(l);
}

/* rapid playing (all eight voices ringing, notes giving way): no clicks. A click is a step at the render's chunk
 * edge (a voice cut, a stair in a decay): the signal there must be no rougher than between the edges */
static void test_no_clicks(void)
{
    static const int KEYS[20] = {0, 3, 5, 2, 7, 4, 9, 1, 6, 8, 10, 3, 12, 5, 0, 14, 2, 11, 7, 13};
    yp_settings_t s;
    yoga_target_t t;
    float ml[BLOCK], mr[BLOCK], rs[BLOCK], es[BLOCK], prev = 0, pp = 0;
    double edge = 0, inner = 0;
    long ne = 0, ni = 0;
    printf("No clicks\n");
    for (int inst = 0; inst < YI_N; inst++) {
        yp_defaults(&s);
        s.program = YP_HERZ, s.x[YX_HINST] = (int8_t)inst;
        yp_init();
        memset(&t, 0, sizeof t);
        t.hand_level = 1.0f;
        yoga_dsp_init();
        yoga_dsp_set(&t);
        for (int b = 0, k = 0; b < 12 * SR / BLOCK; b++) {
            if (b % 43 == 0 && k < 20)                           /* a note every 250 ms */
                yp_hand(&s, KEYS[k++], 100);
            yp_update(&s, (uint32_t)(b * 1000.0 * BLOCK / SR));
            memset(ml, 0, sizeof ml), memset(mr, 0, sizeof mr), memset(rs, 0, sizeof rs), memset(es, 0, sizeof es);
            yoga_dsp_layer(ml, mr, rs, es, BLOCK);
            for (int j = 0; j < BLOCK; j++) {
                double d2 = fabs(ml[j] - 2 * prev + pp);
                if (j == 0)
                    edge += d2, ne++;
                else
                    inner += d2, ni++;
                pp = prev, prev = ml[j];
            }
        }
    }
    CHECK((edge / ne) / (inner / ni) < 1.15, "at the chunk edges as smooth as between them (%.2f; a click: > 1.3)",
          (edge / ne) / (inner / ni));
}

/* a render ran long: voices give way softly, four stay, they come back after a calm */
static void test_overload(void)
{
    float ml[BLOCK], mr[BLOCK], rs[BLOCK], es[BLOCK], prev = 0;
    yoga_target_t t;
    yp_settings_t s;
    double step = 0, calm;
    int after, full;
    printf("Overload\n");
    yp_defaults(&s);
    s.program = YP_HERZ, s.x[YX_HINST] = YI_BOWLS;           /* long rings */
    yp_init();
    memset(&t, 0, sizeof t);
    t.hand_level = 1.0f;
    yoga_dsp_init();
    yoga_dsp_set(&t);
#define RUN(blocks) for (int b_ = 0; b_ < (blocks); b_++) { \
        memset(ml, 0, sizeof ml), memset(mr, 0, sizeof mr), memset(rs, 0, sizeof rs), memset(es, 0, sizeof es); \
        yoga_dsp_layer(ml, mr, rs, es, BLOCK); \
        for (int j_ = 0; j_ < BLOCK; j_++) { step = fmax(step, fabs(ml[j_] - prev)); prev = ml[j_]; } }
    for (int k = 0; k < 8; k++) {
        yp_hand(&s, k, 100);
        RUN(40);
    }
    full = yoga_dsp_hand_voices();
    step = 0;
    RUN(48);                                                 /* the same voices, undisturbed: the step they make */
    calm = step;
    step = 0;
    for (int k = 0; k < 6; k++) {
        yoga_dsp_overload();
        RUN(8);
    }
    after = yoga_dsp_hand_voices();
    CHECK(full == 8 && after == 4, "six overloads: %d voices down to %d, no fewer than four", full, after);
    CHECK(step <= calm * 1.05, "they give way without a click (largest step %.4f, undisturbed %.4f)", step, calm);
    CHECK(yoga_dsp_overloads() == 6, "the count shows on the SETUP page");
    RUN(9 * SR / BLOCK);                                     /* a voice back every 2 s */
    for (int k = 0; k < 8; k++) {                            /* after the calm: all eight again */
        yp_hand(&s, k, 100);
        RUN(8);
    }
    CHECK(yoga_dsp_hand_voices() >= 7, "after a calm the voices are back (%d)", yoga_dsp_hand_voices());
#undef RUN
}

/* ENDLOS: the arc, then the last phase holds until REC */
static void test_endless(void)
{
    yp_settings_t s;
    yp_status_t st;
    int32_t buf[2 * BLOCK];
    double ms = 0;
    unsigned b = 0;
    printf("ENDLOS\n");
    yp_defaults(&s);
    s.program = YP_REISE, s.duration = 5;                    /* endless */
    en_init(), yoga_dsp_init(), yp_init(), yp_mute(0);
    yp_start(&s, 11, 0);
    for (; ms < 40 * 60 * 1000.0; b++) {                     /* 40 minutes, the render every 8th frame (fast) */
        if (b % FRAME_BLOCKS == 0)
            yp_update(&s, (uint32_t)ms);
        if (b % 8 == 0)
            en_render(buf, BLOCK, APP_GAIN_FULL);
        ms += 1000.0 * BLOCK / SR;
    }
    yp_status(&st);
    CHECK(st.state == YS_RUNNING && !st.waking && st.remaining_s == 0, "after 40 minutes it still rests (%s)", st.stage);
    CHECK(st.beat < 1.6f && st.carrier == 528.0f, "it holds the last phase: delta %.2f Hz on %.0f Hz", st.beat, st.carrier);
    yp_finish();
    for (double end = ms + 100000.0; ms < end; b++) {
        if (b % FRAME_BLOCKS == 0)
            yp_update(&s, (uint32_t)ms);
        ms += 1000.0 * BLOCK / SR;
    }
    yp_status(&st);
    CHECK(st.state == YS_DONE, "REC: it wakes up and ends");
}

/* how far the mix went into the soft clip (amb.c: tanh, then * 8300000), from the output's peak: below ~1 it is
 * still clean */
static double clip_in(double peak) { return atanh(fmin(peak * 8388608.0 / 8300000.0, 0.99999)); }

/* the space's build-up (amb.c): the drone in a large hall with its echo up, every level at 10, MASTER full, stays out
 * of the soft clip's hard part; in HERZ, where it builds up little, the drone's sends are hardly held */
static void space_run(const yp_settings_t *s, double *in, double *held, float *hmin)
{
    int32_t buf[2 * BLOCK];
    double ms = 0, peak = 0;
    unsigned b = 0, blocks = 0, h = 0;
    *hmin = 1.0f;
    en_init();
    yoga_dsp_init();
    yp_init();
    yp_start(s, 7u, 0);
    while (ms < 240000) {
        if (b++ % FRAME_BLOCKS == 0)
            yp_update(s, (uint32_t)ms);
        en_render(buf, BLOCK, APP_GAIN_FULL);
        ms += 1000.0 * BLOCK / SR;
        if (ms < 30000)
            continue;
        for (int i = 0; i < 2 * BLOCK; i++)
            peak = fmax(peak, fabs((double)buf[i]) / 8388608.0);
        blocks++;
        h += en_wet_gain < 0.97f;
        *hmin = fminf(*hmin, en_wet_gain);
    }
    *in = clip_in(peak);
    *held = 100.0 * h / blocks;
}
static void test_space(void)
{
    yp_settings_t s;
    double in, held;
    float hmin;
    printf("The space's build-up, MASTER full\n");
    yp_defaults(&s);
    s.program = YP_ERDEN, s.duration = 0, s.x[YX_RSIZE] = 10, s.x[YX_RECHO] = 10;
    space_run(&s, &in, &held, &hmin);
    CHECK(in < 1.1, "ERDEN, hall and echo at 10: into the soft clip at most %.2f (drone's sends held to %.2f)", in, hmin);
    s.drone = s.space = s.klang = s.garten = 10;
    space_run(&s, &in, &held, &hmin);
    CHECK(in < 1.2, "ERDEN, every level at 10 as well: into the soft clip at most %.2f", in);
    yp_defaults(&s);
    s.program = YP_HERZ, s.duration = 0;
    space_run(&s, &in, &held, &hmin);
    CHECK(hmin > 0.4f && held < 40.0, "HERZ: the drone's sends held %.0f %% of the time, to %.2f at most", held, hmin);
}

static void test_programs(void)
{
    yp_settings_t s;
    printf("Programs, 5 minutes, through the garden, echo and plate\n");
    for (int prog = 0; prog < YP_MANUAL; prog++)
        for (int mode = 0; mode < 2; mode++) {
            take_t t;
            double peak = 0, step = 0, tail;
            yp_defaults(&s);
            s.program = (uint8_t)prog, s.duration = 0, s.mode = (uint8_t)mode;
            run(&t, &s, 1000u + (uint32_t)prog, 330, 14);
            for (size_t i = 1; i < t.n; i++) {
                peak = fmax(peak, fmax(fabs(t.l[i]), fabs(t.r[i])));
                step = fmax(step, fmax(fabs(t.l[i] - t.l[i - 1]), fabs(t.r[i] - t.r[i - 1])));
            }
            tail = rms(t.l + t.n - SR, SR);
            CHECK(t.done_at > 0 && fabs((double)t.done_at / SR - 300) < 3, "%-9s %s ends after %.1f s", YP_NAME[prog],
                  mode ? "BIN" : "ISO", (double)t.done_at / SR);
            CHECK(peak < 0.98 && peak > 0.1, "%-9s %s peak %.3f", YP_NAME[prog], mode ? "BIN" : "ISO", peak);
            CHECK(clip_in(peak) < 1.1, "%-9s %s into the soft clip at most %.2f", YP_NAME[prog], mode ? "BIN" : "ISO",
                  clip_in(peak));
            CHECK(step < 0.15, "%-9s %s largest step %.4f (no clicks)", YP_NAME[prog], mode ? "BIN" : "ISO", step);
            CHECK(t.max_beat > 14.4f && t.max_beat <= 18.0f, "%-9s %s wake-up reaches beta (%.2f Hz)", YP_NAME[prog],
                  mode ? "BIN" : "ISO", t.max_beat);
            CHECK(tail < 0.003, "%-9s %s quiet 14 s after the end (rms %.5f)", YP_NAME[prog], mode ? "BIN" : "ISO", tail);
            take_free(&t);
        }
}

/* the garden's bells on the keys' instruments: about as loud as the bells, no clicks; WAVs to listen to with
 * YOGA_WAVS=DIR */
static void test_garden_inst(void)
{
    static const char *const NAME[YI_N + 1] = {"bells", "handpan", "kalimba", "tongue", "bowls", "koshi"};
    double ref = 0;
    printf("The garden's bells on an instrument\n");
    for (int g = 0; g <= YI_N; g++) {
        yp_settings_t s;
        take_t t;
        double level, peak = 0, step = 0;
        const char *dir = getenv("YOGA_WAVS");
        yp_defaults(&s);
        s.program = YP_HERZ, s.duration = 0, s.klang = 0, s.pad = 0, s.bass = 0, s.bells = 8;
        s.x[YX_GDENSITY] = 6, s.x[YX_GINST] = (int8_t)g;
        run(&t, &s, 77u, 70, 0);
        level = rms(t.l + 30 * SR, 40 * SR);
        for (size_t i = 1; i < t.n; i++) {
            peak = fmax(peak, fmax(fabs(t.l[i]), fabs(t.r[i])));
            step = fmax(step, fmax(fabs(t.l[i] - t.l[i - 1]), fabs(t.r[i] - t.r[i - 1])));
        }
        if (!g)
            ref = level;
        else
            CHECK(fabs(20 * log10(level / ref)) < 4.0, "%-8s %+.1f dB against the bells", NAME[g],
                  20 * log10(level / ref));
        CHECK(level > 0.003 && peak < 0.98 && step < 0.15, "%-8s rms %.4f, peak %.3f, largest step %.4f", NAME[g],
              level, peak, step);
        if (dir) {
            char path[512];
            snprintf(path, sizeof path, "%s/garden-%s.wav", dir, NAME[g]);
            wav(path, &t);
        }
        take_free(&t);
    }
}

/* the journey's crossfades, the drone alone (no garden, no bowls, no surf): the loudness, second to second */
static void test_journey(void)
{
    yp_settings_t s;
    take_t t;
    double calm = 0, cross = 0;
    int calm_at = 0, cross_at = 0;
    printf("REISE: the crossfades are no rougher than the calm between them\n");
    yp_defaults(&s);
    s.program = YP_REISE, s.duration = 2, s.mode = YOGA_BIN;    /* 15 minutes */
    s.pad = s.bells = s.bass = s.echo = 0, s.bowls = 0, s.breath = 0;
    p.rng = 1;                                /* yp_start mixes in the last draw: the same journey, whatever ran before */
    run(&t, &s, 77, 15 * 60 + 20, 1);
    /* the announcement: counts down from 30 to the crossfade's first moment, once for each change */
    {
        int warned = 0, changes = 0, wrong = 0, last = 0;
        yp_status_t st;
        p.state = YS_RUNNING;
        for (uint32_t ms = 0; ms + 1000u < p.wake_start; ms += 250u) {
            p.elapsed = ms;
            yp_status(&st);
            uint32_t i = yp_phase_at(ms);
            int in_xf = i + 1u < p.nph && p.xf_half[i] && ms + p.xf_half[i] > p.ph[i].end;
            if (st.soon_s > YP_WARN_S || (st.soon_s && in_xf))
                wrong++;
            if (st.soon_s && !last)
                warned++;
            last = st.soon_s;
        }
        for (int i = 0; i + 1 < p.nph; i++)
            changes += p.xf_half[i] != 0;
        CHECK(warned == changes && !wrong, "each of the %d changes announced once, before it (%d)", changes, warned);
        p.state = YS_DONE, p.elapsed = p.end;
    }
    /* from the end of the fade-in to the wake-up: 8 s windows, 2 s apart */
    for (size_t w = 40; w + 10 < p.wake_start / 1000u; w += 2) {
        double a = rms(t.l + w * SR, 8 * SR), b = rms(t.l + (w + 2) * SR, 8 * SR);
        double d = fabs(20 * log10(b / a));
        int in = 0;
        for (int i = 0; i + 1 < p.nph; i++)
            if (p.xf_half[i] && (w + 10) * 1000 > p.ph[i].end - p.xf_half[i] && w * 1000 < p.ph[i].end + p.xf_half[i])
                in = 1;
        if (in && d > cross)
            cross = d, cross_at = (int)w;
        if (!in && d > calm)
            calm = d, calm_at = (int)w;
    }
    CHECK(cross > 0 && cross <= calm + 0.3, "crossfades: at most %.2f dB (at %d s); the calm: %.2f dB (at %d s)", cross,
          cross_at, calm, calm_at);
    take_free(&t);
}

/* one process a take: the garden keeps its own state (its tides, its tape heads) from run to run */
static uint64_t digest(const char *self, uint32_t seed)
{
    char cmd[512];
    unsigned long long h = 0;
    FILE *f;
    snprintf(cmd, sizeof cmd, "'%s' digest %u", self, seed);
    if (!(f = popen(cmd, "r")))
        return 0;
    if (fscanf(f, "%llx", &h) != 1)
        h = 0;
    pclose(f);
    return h;
}
static uint64_t digest_one(uint32_t seed)
{
    yp_settings_t s;
    take_t t;
    uint64_t h = 1469598103934665603ull;
    yp_defaults(&s);
    s.program = YP_REISE, s.duration = 0;
    run(&t, &s, seed, 90, 0);
    for (size_t j = 0; j < t.n; j++)
        h = (h ^ (uint64_t)(int64_t)(t.l[j] * 8388608.0f)) * 1099511628211ull;
    take_free(&t);
    return h;
}
static void test_random(const char *self)
{
    uint64_t a = digest(self, 42), b = digest(self, 42), c = digest(self, 43);
    printf("Randomness\n");
    CHECK(a && a == b, "the same seed plays the same");
    CHECK(a && a != c, "another seed plays another");
}

int main(int argc, char **argv)
{
    if (argc >= 6 && !strcmp(argv[1], "wav")) {
        yp_settings_t s;
        take_t t;
        int prog = -1;
        uint32_t minutes = (uint32_t)atoi(argv[4]), seed = argc > 6 ? (uint32_t)strtoul(argv[6], 0, 0) : (uint32_t)time(0);
        yp_defaults(&s);
        for (int i = 0; i < YP_NPROG; i++)
            if (!strcasecmp(argv[3], YP_NAME[i]))
                prog = i;
        if (prog < 0) {
            fprintf(stderr, "unknown program %s\n", argv[3]);
            return 2;
        }
        s.program = (uint8_t)prog;
        s.mode = !strcmp(argv[5], "bin") ? YOGA_BIN : YOGA_ISO;
        for (int i = 0; i < YP_NDURATION; i++)
            if (YP_MINUTES[i] == minutes)
                s.duration = (uint8_t)i;
        s.timer = (uint8_t)(minutes / 5u);
        run(&t, &s, seed, minutes ? minutes * 60.0 + 20 : 120, 10);
        wav(argv[2], &t);
        printf("%s: %s, %.0f s, seed %u\n", argv[2], YP_NAME[prog], (double)t.n / SR, seed);
        take_free(&t);
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "digest")) {
        printf("%llx\n", (unsigned long long)digest_one((uint32_t)strtoul(argv[2], 0, 0)));
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "test")) {
        test_layer();
        test_hand();
        test_mute();
        test_seq();
        test_instruments();
        test_no_clicks();
        test_overload();
        test_endless();
        test_programs();
        test_garden_inst();
        test_space();
        test_journey();
        test_random(argv[0]);
        printf(failures ? "\n%d failed\n" : "\nall passed\n", failures);
        return failures != 0;
    }
    fprintf(stderr, "usage: %s test | wav OUT.wav PROGRAM MINUTES iso|bin [SEED]\n", argv[0]);
    return 2;
}
