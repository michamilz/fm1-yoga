/* SPDX-License-Identifier: MPL-2.0 */
/* The yoga layer (yoga_dsp.h). Compiled into amb.c's -O2 unit (amb.c includes it). Phases are 32-bit accumulators (a turn is 2^32), read from one sine table. */
#include "yoga_dsp.h"
#include "fastmath.h"

#define Y_BARRIER() __asm__ volatile("" ::: "memory")
#define TURN 4294967296.0f
enum { YSN = 1024, BELLS = 4, PARTIALS = 5, STRIKES = 8 };

static float ysin[YSN + 1];
static inline float ysine(uint32_t ph)
{
    uint32_t i = ph >> 22;
    float f = (float)(ph & 0x3FFFFFu) * (1.0f / 4194304.0f);
    return ysin[i] + (ysin[i + 1] - ysin[i]) * f;
}
static inline uint32_t yinc(float hz) { return (uint32_t)(fm_clampf(hz, 0.0f, 20000.0f) * (TURN / YOGA_RATE)); }

/* main loop -> render: a sequence count around the copy (the render can interrupt the writer, not the reverse:
 * an odd or changed count means keep the last targets) */
static yoga_target_t y_pub, y_t;
static volatile uint32_t y_seq;

typedef struct {
    uint32_t ph[PARTIALS], inc[PARTIALS];
    float amp[PARTIALS], dec[PARTIALS];      /* dec: a sample's */
    float gl, gr, att;
    int on, fade;                            /* fade: it gives way to a new strike, over FADE_N samples */
    float fg;
} ybell_t;
typedef struct {
    uint32_t inc[PARTIALS];
    float amp[PARTIALS], dec[PARTIALS], gl, gr;
} ystrike_t;
static ystrike_t y_q[STRIKES];
static volatile uint32_t y_qw, y_qr, y_silence;
static uint32_t y_silence_seen;
static ybell_t y_bell[BELLS];

/* the instrument on the keys (handpan, kalimba, tongue drum, bowls, chimes): modal synthesis. Each note is a few
 * decaying sines (SOUNDS below: a recipe each) and a burst of filtered noise for the strike. Up to eight ring. */
enum { HANDS = 8, HP = 7, HSTRIKES = 16 };
#define FADE_N 256.0f                        /* a voice giving way fades over 5.8 ms */
typedef struct {
    uint32_t ph[HP], inc[HP];
    float amp[HP], dec[HP], gl, gr, att, attk, nz, nzdec, nzk, nlp1, nlp2;   /* dec: a sample's */
    uint32_t seed;
    int on, fade;                            /* fade: it gives way to a new note, over FADE_N samples */
    float fg;                                /* its gain while it fades, 1 .. 0 */
} yhand_t;
typedef struct {
    uint32_t inc[HP];
    float amp[HP], dec[HP], gl, gr, nz, nzdec, nzk, att;
} yhstrike_t;
static yhand_t y_hand[HANDS];
static yhstrike_t y_hq[HSTRIKES];
static volatile uint32_t y_hqw, y_hqr, y_hsilence;
static uint32_t y_hsilence_seen;
static volatile uint32_t y_hand_on;
static float g_hlevel, g_hspace, g_hecho;

/* the render's own state */
typedef struct {
    float hz;                                /* follows its target unless silent: then it jumps (crossfades) */
    float gain;
    uint32_t ph[2][4];                       /* left, right: fundamental, twin, octave, fifth */
} ydrone_t;
static ydrone_t y_dr[2];
static float y_bin, y_beat, y_detune, y_breath_hz;
static float g_level, g_bell, g_drone, g_oct, g_fifth, g_noise, g_depth, g_space, g_garden;
static float g_twin, g_swell, g_ntone, g_echo;
static uint32_t ph_beat, ph_breath, y_noise_l = 0x12345678u, y_noise_r = 0x9E3779B9u;
static float lp1l, lp2l, lp1r, lp2r;
static volatile uint32_t y_breath_view;

void yoga_dsp_init(void)
{
    int i, k;
    for (i = 0; i <= YSN; i++)
        ysin[i] = fm_sinf((float)i * (FM_TWO_PI / (float)YSN));
    ysin[YSN / 2] = ysin[YSN] = 0.0f;
    for (i = 0; i < BELLS; i++)
        y_bell[i].on = 0;
    for (i = 0; i < 2; i++) {
        y_dr[i].hz = 0.0f;
        y_dr[i].gain = 0.0f;
        for (k = 0; k < 4; k++)
            y_dr[i].ph[0][k] = y_dr[i].ph[1][k] = 0;
    }
    y_bin = y_beat = y_detune = y_breath_hz = 0.0f;
    g_level = g_bell = g_drone = g_oct = g_fifth = g_noise = g_depth = g_space = g_garden = 0.0f;
    g_twin = g_swell = g_ntone = g_echo = 0.0f;
    ph_beat = ph_breath = 0;
    y_noise_l = 0x12345678u;
    y_noise_r = 0x9E3779B9u;
    lp1l = lp2l = lp1r = lp2r = 0.0f;
    y_t.carrier[0] = y_t.carrier[1] = 0.0f;
    y_t.blend = y_t.beat = y_t.detune = y_t.breath = y_t.level = y_t.bell_level = 0.0f;
    y_t.drone = y_t.octave = y_t.fifth = y_t.noise = y_t.iso_depth = y_t.space = y_t.garden = 0.0f;
    y_t.twin = y_t.swell = y_t.noise_tone = y_t.echo = 0.0f;
    y_t.mode = YOGA_ISO;
    y_pub = y_t;
    y_seq = 0;
    y_qw = y_qr = y_silence = y_silence_seen = 0;
    for (i = 0; i < HANDS; i++)
        y_hand[i].on = 0;
    y_hqw = y_hqr = y_hsilence = y_hsilence_seen = 0;
    y_hand_on = 0;
    g_hlevel = g_hspace = g_hecho = 0.0f;
    y_t.hand_level = y_t.hand_space = y_t.hand_echo = 0.0f;
}

void yoga_dsp_set(const yoga_target_t *t)
{
    y_seq = y_seq + 1;
    Y_BARRIER();
    y_pub = *t;
    Y_BARRIER();
    y_seq = y_seq + 1;
}

void yoga_dsp_silence(void) { y_silence = y_silence + 1; }
uint32_t yoga_dsp_breath(void) { return y_breath_view; }

/* per 128 frames, for a 60 dB decay in t60 seconds */
static float decay_for(float t60) { return fm_expf(-6.9078f * 128.0f / (YOGA_RATE * fm_maxf(t60, 0.2f))); }

int yoga_dsp_strike(int kind, float hz, float level, float pan, float ring)
{
    /* a singing bowl: inharmonic modes and a slowly beating twin of the fundamental; a chime: brighter, shorter */
    static const float BOWL_R[PARTIALS] = {1.0f, 1.0f, 2.76f, 5.40f, 8.93f};
    static const float BOWL_A[PARTIALS] = {1.0f, 0.68f, 0.47f, 0.2f, 0.09f};
    static const float BOWL_T[PARTIALS] = {16.0f, 14.0f, 9.0f, 5.0f, 3.0f};
    static const float CHIME_R[PARTIALS] = {1.0f, 1.0f, 2.76f, 5.40f, 3.0f};
    static const float CHIME_A[PARTIALS] = {1.0f, 0.25f, 0.32f, 0.11f, 0.06f};
    static const float CHIME_T[PARTIALS] = {7.0f, 6.0f, 3.2f, 1.5f, 1.2f};
    const float *r = kind == YOGA_CHIME ? CHIME_R : BOWL_R, *a = kind == YOGA_CHIME ? CHIME_A : BOWL_A,
                *t = kind == YOGA_CHIME ? CHIME_T : BOWL_T;
    uint32_t w = y_qw, next = (w + 1u) % STRIKES;
    ystrike_t *s;
    int p;
    if (next == y_qr || hz <= 0.0f)
        return -1;
    s = &y_q[w];
    level = fm_clampf(level, 0.0f, 1.0f);
    pan = fm_clampf(pan, 0.0f, 1.0f);
    for (p = 0; p < PARTIALS; p++) {
        s->inc[p] = yinc(hz * r[p] + (p == 1 ? (kind == YOGA_CHIME ? 0.3f : 0.65f) : 0.0f));
        s->amp[p] = level * a[p] * 0.25f;
        s->dec[p] = decay_for(t[p] * fm_clampf(ring, 0.25f, 2.0f));
    }
    s->gl = fm_cosf(pan * (FM_PI / 2.0f));
    s->gr = fm_sinf(pan * (FM_PI / 2.0f));
    Y_BARRIER();
    y_qw = next;
    return 0;
}

/* each struck sound as a recipe: the partials' ratios to hz (and offsets in Hz: a beating neighbour), their
 * amplitudes and rings (T60, s), which of them brightness and a harder touch raise, the strike's noise (level,
 * decay a sample, colour) and how fast it sets in (the attack, a sample) */
typedef struct {
    float r[HP], off[HP], a[HP], t[HP];
    uint8_t bright;                          /* bit p: partial p scales with brightness */
    float nz, nzdec, nzk, att;
} ysound_t;
static const ysound_t SOUNDS[YOGA_HAND_KINDS] = {
    /* the handpan's tone field: fundamental and its neighbour (a slow beat), octave and its neighbour (the
     * shimmer), compound fifth, a metallic one */
    [YOGA_HAND_NOTE] = {{1.0f, 1.0f, 2.0f, 2.0f, 3.0f, 4.97f, 0.5f}, {0, 0.55f, 0, 1.3f, 0, 0, 0},
                        {1.0f, 0.3f, 0.55f, 0.22f, 0.3f, 0.12f, 0.0f}, {7.0f, 6.0f, 4.0f, 3.2f, 2.0f, 0.35f, 2.5f},
                        0x3C, 0.18f, 0.9985f, 0.18f, 0.02f},
    /* the ding: the same, rounder, with the hollow body below it */
    [YOGA_HAND_DING] = {{1.0f, 1.0f, 2.0f, 2.0f, 3.0f, 4.97f, 0.5f}, {0, 0.55f, 0, 1.3f, 0, 0, 0},
                        {1.0f, 0.3f, 0.55f, 0.22f, 0.3f, 0.12f, 0.25f}, {9.8f, 8.4f, 5.6f, 4.5f, 2.8f, 0.5f, 3.5f},
                        0x3C, 0.18f, 0.9985f, 0.18f, 0.02f},
    /* the tak: the rim's few inharmonic modes, gone in a tenth of a second, and a brighter, louder hand */
    [YOGA_HAND_TAK] = {{1.0f, 1.52f, 2.31f, 3.17f, 4.43f, 0.5f, 1.0f}, {0, 0, 0, 0, 0, 0, 0},
                       {0.35f, 0.3f, 0.22f, 0.14f, 0.08f, 0.2f, 0.0f}, {0.16f, 0.12f, 0.09f, 0.07f, 0.05f, 0.1f, 0.05f},
                       0x1C, 0.55f, 0.9965f, 0.45f, 0.05f},
    /* the kalimba: a plucked tine, its bending modes far apart (1 : 6.27 : 17.55), a little box below */
    [YOGA_HAND_KALIMBA] = {{1.0f, 1.0f, 2.0f, 6.27f, 17.55f, 0.5f, 3.0f}, {0, 0.25f, 0, 0, 0, 0, 0},
                           {1.0f, 0.15f, 0.08f, 0.28f, 0.07f, 0.06f, 0.04f}, {2.6f, 2.2f, 1.2f, 0.28f, 0.06f, 0.5f, 0.4f},
                           0x58, 0.22f, 0.993f, 0.5f, 0.08f},
    /* the tongue drum: like the handpan, rounder and darker, fewer overtones, a softer thump */
    [YOGA_HAND_TONGUE] = {{1.0f, 1.0f, 2.0f, 3.92f, 0.5f, 1.0f, 1.0f}, {0, 0.4f, 0, 0, 0, 0, 0},
                          {1.0f, 0.2f, 0.25f, 0.06f, 0.1f, 0.0f, 0.0f}, {5.0f, 4.4f, 2.6f, 0.4f, 2.0f, 0.1f, 0.1f},
                          0x0C, 0.14f, 0.998f, 0.1f, 0.012f},
    /* the singing bowl: inharmonic modes and a beating twin, a soft mallet, a long ring */
    [YOGA_HAND_BOWL] = {{1.0f, 1.0f, 2.76f, 5.40f, 8.93f, 1.0f, 1.0f}, {0, 0.65f, 0, 0, 0, 0, 0},
                        {1.0f, 0.68f, 0.47f, 0.2f, 0.09f, 0.0f, 0.0f}, {14.0f, 12.0f, 8.0f, 4.5f, 2.6f, 0.1f, 0.1f},
                        0x1C, 0.05f, 0.996f, 0.3f, 0.004f},
    /* a chime rod (Koshi): a free bar's modes (1 : 2.76 : 5.40), bright and clear, a light tick */
    [YOGA_HAND_CHIME] = {{1.0f, 1.0f, 2.76f, 5.40f, 8.93f, 1.0f, 1.0f}, {0, 0.3f, 0, 0, 0, 0, 0},
                         {1.0f, 0.2f, 0.35f, 0.12f, 0.04f, 0.0f, 0.0f}, {5.0f, 4.0f, 2.2f, 1.0f, 0.5f, 0.1f, 0.1f},
                         0x1C, 0.08f, 0.993f, 0.6f, 0.06f},
};

int yoga_dsp_hand(float hz, float vel, float pan, float ring, float bright, int kind)
{
    const ysound_t *k = &SOUNDS[(unsigned)kind < YOGA_HAND_KINDS ? kind : YOGA_HAND_NOTE];
    uint32_t w = y_hqw, next = (w + 1u) % HSTRIKES;
    yhstrike_t *s;
    float b, v;
    int p;
    if (next == y_hqr || hz <= 0.0f)
        return -1;
    s = &y_hq[w];
    v = fm_clampf(vel, 0.0f, 1.0f);
    b = fm_clampf(bright, 0.0f, 1.0f) * (0.4f + 0.6f * v);  /* a harder strike, more overtones */
    ring = kind == YOGA_HAND_TAK ? 1.0f : fm_clampf(ring, 0.25f, 2.5f);
    for (p = 0; p < HP; p++) {
        s->inc[p] = yinc(hz * k->r[p] + k->off[p]);
        s->amp[p] = k->a[p] * (k->bright >> p & 1u ? b * 1.6f : 1.0f) * v * 0.4f;
        s->dec[p] = decay_for(k->t[p] * ring);
    }
    s->nz = k->nz * v * (0.5f + 0.5f * b);
    s->nzdec = k->nzdec;
    s->nzk = k->nzk;
    s->att = k->att;
    s->gl = fm_cosf(fm_clampf(pan, 0.0f, 1.0f) * (FM_PI / 2.0f));
    s->gr = fm_sinf(fm_clampf(pan, 0.0f, 1.0f) * (FM_PI / 2.0f));
    Y_BARRIER();
    y_hqw = next;
    return 0;
}
void yoga_dsp_hand_silence(void) { y_hsilence = y_hsilence + 1; }
int yoga_dsp_hand_voices(void) { return (int)y_hand_on; }

/* a decay per 128 frames as a sample's: each sample a step, no stair at the chunk's edge */
static float per_sample(float d128) { return fm_exp2f(fm_log2f(d128) * (1.0f / 128.0f)); }

static void take_hands(void)
{
    if (y_hsilence != y_hsilence_seen) {
        int i, p;
        y_hsilence_seen = y_hsilence;
        for (i = 0; i < HANDS; i++)
            for (p = 0; p < HP; p++)
                y_hand[i].dec[p] = per_sample(0.6f);
    }
    while (y_hqr != y_hqw) {
        const yhstrike_t *s = &y_hq[y_hqr];
        int i, p, pick = 0;
        float quiet = 1e9f;
        int fading = 0;
        for (i = 0; i < HANDS; i++) {          /* a free voice, else the quietest */
            float loud = y_hand[i].on ? y_hand[i].amp[0] : -1.0f;
            fading |= y_hand[i].on && y_hand[i].fade;
            if (loud < quiet && !(y_hand[i].on && y_hand[i].fade)) {
                quiet = loud;
                pick = i;
            }
        }
        if (y_hand[pick].on) {                 /* all ring: the quietest fades out over this chunk (no click), the
                                                * note waits a chunk for it */
            if (!fading)
                y_hand[pick].fade = 1, y_hand[pick].fg = 1.0f;
            break;
        }
        y_hand[pick].fade = 0;
        for (p = 0; p < HP; p++) {
            y_hand[pick].inc[p] = s->inc[p];
            y_hand[pick].amp[p] = s->amp[p];
            y_hand[pick].dec[p] = per_sample(s->dec[p]);
            y_hand[pick].ph[p] = 0;
        }
        y_hand[pick].gl = s->gl;
        y_hand[pick].gr = s->gr;
        y_hand[pick].att = 0.0f;
        y_hand[pick].nz = s->nz;
        y_hand[pick].nzdec = s->nzdec;
        y_hand[pick].nzk = s->nzk;
        y_hand[pick].attk = s->att;
        y_hand[pick].nlp1 = y_hand[pick].nlp2 = 0.0f;
        y_hand[pick].seed = 0x2545F491u ^ (uint32_t)pick * 0x9E3779B9u ^ y_hqr;
        y_hand[pick].on = 1;
        Y_BARRIER();
        y_hqr = (y_hqr + 1u) % HSTRIKES;
    }
}

static void take_targets(void)
{
    uint32_t s = y_seq;
    if (!(s & 1u)) {
        yoga_target_t c;
        Y_BARRIER();
        c = y_pub;
        Y_BARRIER();
        if (s == y_seq)
            y_t = c;
    }
}

static void take_strikes(void)
{
    if (y_silence != y_silence_seen) {
        int i, p;
        y_silence_seen = y_silence;
        for (i = 0; i < BELLS; i++)
            for (p = 0; p < PARTIALS; p++)
                y_bell[i].dec[p] = per_sample(0.6f);     /* ~60 ms */
    }
    while (y_qr != y_qw) {
        const ystrike_t *s = &y_q[y_qr];
        int i, p, pick = 0;
        float quiet = 1e9f;
        int fading = 0;
        for (i = 0; i < BELLS; i++) {
            float loud = y_bell[i].on ? y_bell[i].amp[0] : -1.0f;
            fading |= y_bell[i].on && y_bell[i].fade;
            if (loud < quiet && !(y_bell[i].on && y_bell[i].fade)) {
                quiet = loud;
                pick = i;
            }
        }
        if (y_bell[pick].on) {                 /* all ring: the quietest fades out over this chunk first */
            if (!fading)
                y_bell[pick].fade = 1, y_bell[pick].fg = 1.0f;
            break;
        }
        y_bell[pick].fade = 0;
        for (p = 0; p < PARTIALS; p++) {
            y_bell[pick].inc[p] = s->inc[p];
            y_bell[pick].amp[p] = s->amp[p];
            y_bell[pick].dec[p] = per_sample(s->dec[p]);
            y_bell[pick].ph[p] = 0;
        }
        y_bell[pick].gl = s->gl;
        y_bell[pick].gr = s->gr;
        y_bell[pick].att = 0.0f;
        y_bell[pick].on = 1;
        Y_BARRIER();
        y_qr = (y_qr + 1u) % STRIKES;
    }
}

static inline float smooth(float v, float t, float k) { return v + (t - v) * k; }

static inline uint32_t xorshift(uint32_t x)
{
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

float yoga_dsp_garden(uint32_t n)
{
    take_targets();
    g_garden = smooth(g_garden, y_t.garden, fm_minf((float)n / (0.2f * YOGA_RATE), 1.0f));
    return g_garden;
}

void yoga_dsp_layer(float *ml, float *mr, float *rsend, float *esend, uint32_t n)
{
    const float fn = (float)n;
    const float kg = fm_minf(fn / (0.2f * YOGA_RATE), 1.0f);      /* gains: ~0.2 s */
    const float kf = fm_minf(fn / (1.5f * YOGA_RATE), 1.0f);      /* rates and offsets: ~1.5 s */
    float blend, w[2], g0, g1, fg[2];
    uint32_t j, il[2][4], ir[2][4], ib, ibr;
    int d, b, p;
    take_targets();
    take_strikes();
    take_hands();
    g_level = smooth(g_level, y_t.level, kg);
    g_bell = smooth(g_bell, y_t.bell_level, kg);
    g_drone = smooth(g_drone, y_t.drone, kg);
    g_oct = smooth(g_oct, y_t.octave, kg);
    g_fifth = smooth(g_fifth, y_t.fifth, kg);
    g_noise = smooth(g_noise, y_t.noise, kg);
    g_space = smooth(g_space, y_t.space, kg);
    g_twin = smooth(g_twin, y_t.twin, kg);
    g_swell = smooth(g_swell, y_t.swell, kg);
    g_ntone = smooth(g_ntone, y_t.noise_tone, kg);
    g_echo = smooth(g_echo, y_t.echo, kg);
    g_hlevel = smooth(g_hlevel, y_t.hand_level, kg);
    g_hspace = smooth(g_hspace, y_t.hand_space, kg);
    g_hecho = smooth(g_hecho, y_t.hand_echo, kg);
    g_depth = smooth(g_depth, y_t.mode == YOGA_ISO ? y_t.iso_depth : 0.0f, kg);
    y_beat = smooth(y_beat, y_t.beat, kf);
    y_bin = smooth(y_bin, y_t.mode == YOGA_BIN ? y_t.beat : 0.0f, kf);
    y_detune = smooth(y_detune, y_t.detune, kf);
    y_breath_hz = smooth(y_breath_hz, y_t.breath, kf);
    /* the two drones: equal power across the blend; a silent drone takes its new pitch at once (a crossfade
     * brings it in), a sounding one follows slowly (the manual's fine changes) */
    blend = fm_clampf(y_t.blend, 0.0f, 1.0f);
    w[0] = fm_cosf(blend * (FM_PI / 2.0f));
    w[1] = fm_sinf(blend * (FM_PI / 2.0f));
    for (d = 0; d < 2; d++) {
        ydrone_t *v = &y_dr[d];
        float hz = y_t.carrier[d];
        v->gain = smooth(v->gain, w[d], kg);
        if (v->gain < 1e-4f || v->hz <= 0.0f)
            v->hz = hz;
        else
            v->hz = smooth(v->hz, hz, kf);
        il[d][0] = yinc(v->hz);
        il[d][1] = yinc(v->hz + y_detune);
        il[d][2] = yinc(v->hz * 2.0f);
        il[d][3] = yinc(v->hz * 1.5f);
        ir[d][0] = yinc(v->hz + y_bin);
        ir[d][1] = yinc(v->hz + y_detune + y_bin);
        ir[d][2] = yinc(v->hz * 2.0f + y_bin);
        ir[d][3] = yinc(v->hz * 1.5f + y_bin);
        fg[d] = v->gain;
    }
    g0 = fg[0];
    g1 = fg[1];
    ib = yinc(y_beat);
    ibr = yinc(y_breath_hz);
    (void)fn;
    {
        const float da = g_drone * g_level, na = g_noise * g_level, depth = g_depth, oct = g_oct, fif = g_fifth;
        const float send = g_space * 0.6f, twin = g_twin, swell = g_swell, ech = g_echo;
        const float n1 = 0.1f + 0.9f * g_ntone, n2 = 2.4f - 1.8f * g_ntone;   /* the surf: hiss .. rumble */
        for (j = 0; j < n; j++) {
            float breath, pulse, amp, l = 0.0f, r = 0.0f, bl = 0.0f, br = 0.0f;
            ph_beat += ib;
            ph_breath += ibr;
            breath = 0.5f + 0.5f * ysine(ph_breath - 0x40000000u);     /* 0 exhaled .. 1 inhaled */
            pulse = 0.5f + 0.5f * ysine(ph_beat);
            pulse *= pulse;                                          /* a soft pulse, never a click */
            amp = da * (1.0f - depth * (1.0f - pulse)) * (1.0f - swell + swell * breath);
            for (d = 0; d < 2; d++) {
                ydrone_t *v = &y_dr[d];
                float gd = d ? g1 : g0, sl, sr;
                for (p = 0; p < 4; p++) {
                    v->ph[0][p] += il[d][p];
                    v->ph[1][p] += ir[d][p];
                }
                if (gd < 1e-4f)
                    continue;
                sl = ysine(v->ph[0][0]) + twin * ysine(v->ph[0][1]) + oct * ysine(v->ph[0][2]) + fif * ysine(v->ph[0][3]);
                sr = ysine(v->ph[1][0]) + twin * ysine(v->ph[1][1]) + oct * ysine(v->ph[1][2]) + fif * ysine(v->ph[1][3]);
                l += sl * gd;
                r += sr * gd;
            }
            l *= amp;
            r *= amp;
            if (na > 1e-5f) {                                        /* breathing surf: louder on the inhale */
                float sh = breath * breath * na, wl, wr;
                y_noise_l = xorshift(y_noise_l);
                y_noise_r = xorshift(y_noise_r);
                wl = (float)(int32_t)y_noise_l * (1.0f / 2147483648.0f);
                wr = (float)(int32_t)y_noise_r * (1.0f / 2147483648.0f);
                lp1l += (wl - lp1l) * 0.125f;
                lp2l += (lp1l - lp2l) * 0.03125f;
                lp1r += (wr - lp1r) * 0.125f;
                lp2r += (lp1r - lp2r) * 0.03125f;
                l += (n1 * lp1l + n2 * lp2l) * sh;
                r += (n1 * lp1r + n2 * lp2r) * sh;
            }
            for (b = 0; b < BELLS; b++) {
                ybell_t *v = &y_bell[b];
                float s = 0.0f;
                if (!v->on)
                    continue;
                for (p = 0; p < PARTIALS; p++) {
                    v->ph[p] += v->inc[p];
                    s += ysine(v->ph[p]) * v->amp[p];
                    v->amp[p] *= v->dec[p];
                }
                v->att += (1.0f - v->att) * 0.004f;                  /* a soft mallet: ~6 ms */
                s *= v->att * g_bell;
                if (v->fade) {                                       /* giving way: out in FADE_N samples */
                    v->fg = fm_maxf(v->fg - 1.0f / FADE_N, 0.0f);
                    s *= v->fg;
                }
                bl += s * v->gl;
                br += s * v->gr;
            }
            ml[j] += 0.32f * l + 0.6f * bl;
            mr[j] += 0.32f * r + 0.6f * br;
            rsend[j] += (l + r) * send + (bl + br) * 0.7f;
            esend[j] += ((l + r) * 0.08f + (bl + br) * 0.9f) * ech;
            {                                                        /* the handpan */
                float hl = 0.0f, hr = 0.0f;
                for (b = 0; b < HANDS; b++) {
                    yhand_t *v = &y_hand[b];
                    float x = 0.0f;
                    if (!v->on)
                        continue;
                    for (p = 0; p < HP; p++) {
                        v->ph[p] += v->inc[p];
                        x += ysine(v->ph[p]) * v->amp[p];
                        v->amp[p] *= v->dec[p];
                    }
                    v->att += (1.0f - v->att) * v->attk;             /* the strike: a palm, a pluck, a mallet */
                    x *= v->att;
                    if (v->nz > 1e-4f) {                             /* the hand on the steel: a soft thump */
                        float w;
                        v->seed = xorshift(v->seed);
                        w = (float)(int32_t)v->seed * (1.0f / 2147483648.0f);
                        v->nlp1 += (w - v->nlp1) * v->nzk;
                        v->nlp2 += (v->nlp1 - v->nlp2) * v->nzk;
                        x += (v->nlp1 - v->nlp2) * v->nz;
                        v->nz *= v->nzdec;
                    }
                    if (v->fade) {                                   /* giving way: out in FADE_N samples */
                        v->fg = fm_maxf(v->fg - 1.0f / FADE_N, 0.0f);
                        x *= v->fg;
                    }
                    hl += x * v->gl;
                    hr += x * v->gr;
                }
                hl *= g_hlevel;
                hr *= g_hlevel;
                ml[j] += hl;
                mr[j] += hr;
                rsend[j] += (hl + hr) * g_hspace;
                esend[j] += (hl + hr) * g_hecho;
            }
        }
    }
    {                                          /* rung out, or faded for a new note: free */
        uint32_t ring = 0;
        for (b = 0; b < BELLS; b++) {
            ybell_t *v = &y_bell[b];
            int alive = 0;
            if (!v->on)
                continue;
            for (p = 0; p < PARTIALS; p++)
                alive |= v->amp[p] > 1e-5f;
            v->on = alive && !(v->fade && v->fg <= 0.0f);
            if (!v->on)
                v->fade = 0;
        }
        for (b = 0; b < HANDS; b++) {
            yhand_t *v = &y_hand[b];
            int alive = 0;
            if (!v->on)
                continue;
            for (p = 0; p < HP; p++)
                alive |= v->amp[p] > 1e-5f;
            v->on = alive && !(v->fade && v->fg <= 0.0f);
            if (!v->on)
                v->fade = 0;
            ring += (uint32_t)v->on;
        }
        y_hand_on = ring;
    }
    lp1l = fm_flush(lp1l);
    lp2l = fm_flush(lp2l);
    lp1r = fm_flush(lp1r);
    lp2r = fm_flush(lp2r);
    y_breath_view = ph_breath >> 16;
}
