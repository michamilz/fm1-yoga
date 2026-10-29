/* SPDX-License-Identifier: MPL-2.0 */
/* The programs' timeline (yoga_program.h). Times are milliseconds of running time: a pause freezes them. The
 * generator smooths only briefly, so every slow movement is drawn here, a frame at a time.
 *
 * A change of carrier (REISE, or the manual's key) is a crossfade between the layer's two drones, 35..55 s long,
 * centred on the phase's boundary: the loudness stays even, no pitch glides. At its middle the garden takes the
 * new root, its pads breathing over to the new chord, and its tuning glides there over some seconds (amb.c). */
#include "yoga_program.h"
#include "plat.h"
#include "fastmath.h"
#include "yoga_dsp.h"
#include "amb.h"

const char *const YP_NAME[YP_NPROG] = {"ERDEN", "LOSLASSEN", "HERZ", "WANDLUNG", "REISE", "MANUELL"};
const uint16_t YP_CARRIER[YP_NCARRIER] = {174, 396, 432, 528};
const uint8_t YP_MINUTES[YP_NDURATION] = {5, 10, 15, 20, 30, 0};   /* 0: endless */
const char *const YI_NAME[YI_N] = {"HANDPAN", "KALIMBA", "ZUNGE", "SCHALEN", "KOSHI"};
/* the garden on each carrier: its root (pitch class) and Tune (cents) put the root on the carrier itself */
static const uint8_t ROOT_OF[YP_NCARRIER] = {5, 7, 9, 0};          /* F, G, A, C */
static const int8_t TUNE_OF[YP_NCARRIER] = {-6, 18, -32, 16};      /* 174.0, 396.0, 432.0, 528.0 Hz */

/* the details: label, lo, hi, default, AUTO allowed, names */
static const char *const N_PITCH[] = {"TIEF", "MITTEL", "HOCH"};
static const char *const N_PAD[] = {"RHODES", "VOICES"};
static const char *const N_ETIME[] = {"1/4.", "1/2", "1/2.", "1 TAKT", "2 TAKTE", "4 TAKTE"};
static const char *const N_SCALE[YP_NSCALE] = {"KURD", "CELTIC", "INTEGRAL", "PYGMY", "HIJAZ", "SABYE", "AKEBONO"};
static const char *const N_DING[] = {"174", "396", "432", "528"};
static const char *const N_INST[YI_N] = {"HANDPAN", "KALIMBA", "ZUNGE", "SCHALEN", "KOSHI"};
/* the handpans' scales: the eight tone fields around the ding, in semitones above it */
static const uint8_t SCALE[YP_NSCALE][8] = {
    {7, 8, 10, 12, 14, 15, 17, 19},          /* Kurd: minor, the classic */
    {7, 10, 12, 14, 15, 17, 19, 22},         /* Celtic minor (Amara) */
    {7, 8, 10, 12, 14, 15, 19, 22},          /* Integral */
    {3, 5, 7, 10, 12, 14, 15, 19},           /* Pygmy */
    {7, 8, 11, 12, 14, 15, 17, 19},          /* Hijaz */
    {7, 9, 11, 12, 14, 16, 19, 21},          /* Sabye: major, bright */
    {7, 8, 12, 14, 15, 19, 20, 24},          /* Akebono: Japanese pentatonic (Hirajoshi's kin), very calm */
};
const yx_info_t YX[YX_N] = {
    [YX_HARM] = {"OBERTOENE", 0, 10, YX_AUTO, 1, 0},   [YX_CHORUS] = {"SCHWEBUNG", 0, 10, YX_AUTO, 1, 0},
    [YX_PULSE] = {"PULS (ISO)", 0, 10, YX_AUTO, 1, 0},
    [YX_BRATE] = {"TEMPO", 3, 8, YX_AUTO, 1, 0},        [YX_BCOLOR] = {"FARBE", 0, 10, 4, 0, 0},
    [YX_BSWELL] = {"WELLE", 0, 10, 4, 0, 0},
    [YX_CHIMES] = {"GLOECKCHEN", 0, 10, YX_AUTO, 1, 0}, [YX_BPITCH] = {"TONLAGE", -1, 1, 0, 0, N_PITCH},
    [YX_RING] = {"NACHKLANG", 0, 10, 5, 0, 0},
    [YX_RSIZE] = {"GROESSE", 0, 10, YX_AUTO, 1, 0},     [YX_RECHO] = {"ECHO", 0, 10, 3, 0, 0},
    [YX_PCOLOR] = {"FARBE", 0, 1, YX_AUTO, 1, N_PAD},   [YX_PBRIGHT] = {"HELLIGKEIT", 0, 10, YX_AUTO, 1, 0},
    [YX_PDRIFT] = {"AKKORDE", 0, 10, YX_AUTO, 1, 0},
    [YX_GDENSITY] = {"DICHTE", 0, 10, YX_AUTO, 1, 0},   [YX_GTONE] = {"KLANG", 0, 10, YX_AUTO, 1, 0},
    [YX_GOCT] = {"OKTAVE", -2, 2, YX_AUTO, 1, 0},
    [YX_BOCT] = {"OKTAVE", -1, 1, YX_AUTO, 1, 0},       [YX_BGLIDE] = {"GLEITEN", 0, 10, YX_AUTO, 1, 0},
    [YX_BFIFTH] = {"QUINTE", 0, 10, YX_AUTO, 1, 0},
    [YX_ETIME] = {"ZEIT", 0, 5, YX_AUTO, 1, N_ETIME},   [YX_EREVERB] = {"HALL", 0, 10, YX_AUTO, 1, 0},
    [YX_EMOTION] = {"BEWEGUNG", 0, 10, YX_AUTO, 1, 0},
    [YX_HINST] = {"INSTRUMENT", 0, YI_N - 1, 0, 0, N_INST},
    [YX_HSCALE] = {"SKALA", 0, YP_NSCALE - 1, 0, 0, N_SCALE}, [YX_HDING] = {"DING", 0, 3, YX_AUTO, 1, N_DING},
    [YX_HRING] = {"NACHKLANG", 0, 10, 5, 0, 0},
    [YX_HSPACE] = {"HALL", 0, 10, 6, 0, 0},           [YX_HECHO] = {"ECHO", 0, 10, 2, 0, 0},
    [YX_HBRIGHT] = {"HELLIGKEIT", 0, 10, 5, 0, 0},    [YX_HDYN] = {"DYNAMIK", 0, 10, 7, 0, 0},
};
/* the details the garden takes (amb.c): its parameter = value * mul + add; AUTO: the start's recipe */
static const struct {
    uint8_t yx, param;
    int8_t mul, add;
} GMAP[] = {
    {YX_RSIZE, P_SIZE, 10, 0},       {YX_PCOLOR, P_PADTONE, 1, 0},  {YX_PBRIGHT, P_BRIGHT, 10, 0},
    {YX_PDRIFT, P_DRIFT, -8, 100},   {YX_GDENSITY, P_DENSITY, 10, 0}, {YX_GTONE, P_BTONE, 10, 0},
    {YX_GOCT, P_BOCT, 1, 0},         {YX_BOCT, P_BASSOCT, 1, 0},    {YX_BGLIDE, P_GLIDE, 10, 0},
    {YX_BFIFTH, P_FIFTH, 10, 0},     {YX_ETIME, P_ETIME, 1, 0},     {YX_EREVERB, P_REVERB, 10, 0},
    {YX_EMOTION, P_MOTION, 10, 0},
};
#define NGMAP (sizeof GMAP / sizeof GMAP[0])

enum { MAXPH = 4, FINAL_FADE = 20000, MANUAL_XF = 10000 };
enum { TH_HI, TH_LO, DE_HI, DE_LO };
static const float ANCHOR[4][2] = {{6.0f, 7.0f}, {4.0f, 5.5f}, {2.0f, 3.0f}, {0.5f, 1.5f}};

typedef struct {
    uint32_t start, end;
    uint8_t carrier;                   /* YP_CARRIER index */
    float beat_from, beat_to;
    uint32_t glide;                    /* the rate moves from beat_from to beat_to over this long, then holds */
    const char *stage;
    uint8_t stage_id;
} phase_t;

static struct {
    uint8_t state, program, nph, waking, manual, endless, stopping, wake_bowls, last_ph;
    phase_t ph[MAXPH];
    uint32_t xf_half[MAXPH];           /* half the crossfade at the end of phase i (0: same carrier, none) */
    uint32_t elapsed, last_now, wake_start, end, fade_in;
    uint32_t rng;
    float iso_depth, iso_now, beta_from, beta_to, beat_at_wake, beat;
    float pause_gain;
    uint8_t carrier_now, carrier_to, crossing;
    /* the drones: slot i plays slot_carrier[i]; blend 0 slot 0 .. 1 slot 1 */
    uint8_t slot_carrier[2];
    float blend;
    /* the manual's crossfade */
    uint8_t m_slot, m_from_slot;
    uint32_t m_xf_start;
    /* slow timbre drift between two random points */
    uint32_t drift_t0, drift_t1;
    float oct[2], fif[2], det[2], brh[2];
    uint32_t next_chime, opening_bowl;
    /* the garden: what was sent last */
    int8_t g_pad, g_bells, g_bass, g_echo;
    uint8_t g_root_carrier;
    int base_pad, base_bells, base_bass, base_echo;
    int16_t rec[NGMAP], sent[NGMAP];   /* the details' recipe (AUTO) and what the garden has */
} p;

/* the mutes: outside p, so a start keeps them; their gains fade in yp_update */
static struct {
    uint32_t mask;
    float g[YM_N];
    uint8_t ready;
} ym;
void yp_mute(uint32_t mask) { ym.mask = mask & ((1u << YM_N) - 1u); }
uint32_t yp_muted(void) { return ym.mask; }
static void ym_step(uint32_t dt)
{
    float step = (float)dt / 1500.0f;
    int i;
    if (!ym.ready) {
        for (i = 0; i < YM_N; i++)
            ym.g[i] = 1.0f;
        ym.ready = 1;
    }
    for (i = 0; i < YM_N; i++) {
        float goal = ym.mask >> i & 1u ? 0.0f : 1.0f;
        ym.g[i] += fm_clampf(goal - ym.g[i], -step, step);
    }
}
/* a part's gain: its own mute, its layer's and all's (smoothstep: a soft fade) */
static float ym_gain(int part, int layer)
{
    float g = ym.g[part] * (layer >= 0 ? ym.g[layer] : 1.0f) * ym.g[YM_ALL];
    return g * g * (3.0f - 2.0f * g);
}

static uint32_t yp_rnd(void)
{
    uint32_t x = p.rng ? p.rng : 0x6D2B79F5u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return p.rng = x;
}
static float yp_frnd(void) { return (float)(yp_rnd() >> 8) * (1.0f / 16777216.0f); }
static float yp_frange(float lo, float hi) { return lo + (hi - lo) * yp_frnd(); }
static uint32_t yp_urange(uint32_t lo, uint32_t hi) { return hi <= lo ? lo : lo + yp_rnd() % (hi - lo + 1u); }
static float yp_lerp(float a, float b, uint32_t pos, uint32_t len)
{
    return !len || pos >= len ? b : a + (b - a) * ((float)pos / (float)len);
}
static float yp_smoothstep(float x)
{
    x = x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x;
    return x * x * (3.0f - 2.0f * x);
}

void yp_defaults(yp_settings_t *s)
{
    s->program = YP_REISE;
    s->duration = 1;                   /* 10 minutes */
    s->mode = YOGA_ISO;
    s->carrier = 2;                    /* 432 Hz */
    s->beat_d = 50;                    /* 5 Hz, theta */
    s->timer = 2;                      /* 10 minutes */
    s->drone = 7;
    s->breath = 5;
    s->bowls = 6;
    s->space = 7;
    s->pad = 6;
    s->bells = 5;
    s->bass = 5;
    s->echo = 5;
    s->hand = 7;
    s->klang = 7;
    s->garten = 7;
    for (int i = 0; i < YX_N; i++)
        s->x[i] = YX[i].def;
}
void yx_text(int i, int v, char *b)
{
    const yx_info_t *x = &YX[(unsigned)i < YX_N ? i : 0];
    const char *w = 0;
    char t[12];
    int n = 0;
    uint32_t u;
    if (v == YX_AUTO)
        w = "AUTO";
    else if (x->names)
        w = x->names[v - x->lo];
    if (w) {
        while (*w && n < 11)
            b[n++] = *w++;
        b[n] = 0;
        return;
    }
    if (x->lo < 0 && v > 0)
        *b++ = '+';
    if (v < 0)
        *b++ = '-';
    u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    while (n)
        *b++ = t[--n];
    *b = 0;
    if (i == YX_BRATE) {
        const char *m = " / MIN";
        while (*m)
            *b++ = *m++;
        *b = 0;
    }
}
static int yx_all_valid(const yp_settings_t *s)
{
    for (int i = 0; i < YX_N; i++)
        if (!(s->x[i] == YX_AUTO ? YX[i].autok : s->x[i] >= YX[i].lo && s->x[i] <= YX[i].hi))
            return 0;
    return 1;
}
int yp_valid(const yp_settings_t *s)
{
    return s->program < YP_NPROG && s->duration < YP_NDURATION && s->mode <= 1u && s->carrier < YP_NCARRIER &&
           s->beat_d >= 5u && s->beat_d <= 70u && s->timer < YP_NTIMER && s->drone <= 10u && s->breath <= 10u &&
           s->bowls <= 10u && s->space <= 10u && s->pad <= 10u && s->bells <= 10u && s->bass <= 10u &&
           s->echo <= 10u && s->hand <= 10u && s->klang <= 10u && s->garten <= 10u &&
           yx_all_valid(s);
}
int yp_band(float beat) { return beat < 4.0f ? YB_DELTA : beat < 8.0f ? YB_THETA : beat < 13.0f ? YB_ALPHA : YB_BETA; }
const char *yp_band_name(int band)
{
    static const char *const N[4] = {"DELTA", "THETA", "ALPHA", "BETA"};
    return (unsigned)band < 4u ? N[band] : "";
}

void yp_init(void)
{
    uint32_t keep = p.rng;
    uint8_t *b = (uint8_t *)&p;
    uint32_t i;
    for (i = 0; i < sizeof p; i++)
        b[i] = 0;
    p.rng = keep;
    p.state = YS_IDLE;
}

/* ---- the garden (amb.c) --------------------------------------------------------------------------------------- */
static int yp_scaled(int base, int user) { return clamp(base * user / 6, 0, 100); }

static void yp_garden_levels(const yp_settings_t *s, int force)
{
    int v;
    if ((v = (int)((float)yp_scaled(p.base_pad, s->pad) * ym_gain(YM_PADS, -1) + 0.5f)) != p.g_pad || force)
        en_set(P_PAD, p.g_pad = (int8_t)v);
    if ((v = (int)((float)yp_scaled(p.base_bells, s->bells) * ym_gain(YM_BELLS, -1) + 0.5f)) != p.g_bells || force)
        en_set(P_BELLS, p.g_bells = (int8_t)v);
    if ((v = (int)((float)yp_scaled(p.base_bass, s->bass) * ym_gain(YM_BASS, -1) + 0.5f)) != p.g_bass || force)
        en_set(P_BASS, p.g_bass = (int8_t)v);
    if ((v = (int)((float)yp_scaled(p.base_echo, s->echo) * ym_gain(YM_ECHO, -1) + 0.5f)) != p.g_echo || force)
        en_set(P_ECHO, p.g_echo = (int8_t)v);
}
/* the garden's details: the player's value, or the recipe's (AUTO); the pads a little brighter on waking */
static void yp_garden_details(const yp_settings_t *s, int force)
{
    uint32_t k;
    for (k = 0; k < NGMAP; k++) {
        int v = s->x[GMAP[k].yx], want = v == YX_AUTO ? p.rec[k] : v * GMAP[k].mul + GMAP[k].add;
        if (GMAP[k].param == P_BRIGHT && p.waking)
            want = clamp(want + 25, 0, 100);
        if (want != p.sent[k] || force)
            en_set(GMAP[k].param, p.sent[k] = (int16_t)want);
    }
}
static void yp_garden_key(int carrier)
{
    if (carrier == p.g_root_carrier)
        return;
    p.g_root_carrier = (uint8_t)carrier;
    en_set(P_TUNE, TUNE_OF[carrier]);
    en_set(P_ROOT, ROOT_OF[carrier]);
}
/* a calm recipe for each program, drawn anew at each start: no pulse, slow chords, a long hall */
static void yp_garden_start(const yp_settings_t *s, uint32_t seed)
{
    static const uint8_t MODES[YP_NPROG][2] = {
        {MODE_PENT_MIN, MODE_AEOLIAN},      /* ERDEN: low and earthy */
        {MODE_DORIAN, MODE_PENT_MIN},       /* LOSLASSEN */
        {MODE_IONIAN, MODE_PENT_MAJ},       /* HERZ */
        {MODE_LYDIAN, MODE_PENT_MAJ},       /* WANDLUNG: bright */
        {MODE_PENT_MAJ, MODE_PENT_MAJ},     /* REISE: fits every carrier on the way */
        {MODE_PENT_MAJ, MODE_DORIAN}};      /* MANUELL */
    int mode = MODES[p.program][yp_rnd() & 1u];
    p.base_pad = (int)yp_urange(50, 65);
    p.base_bells = (int)yp_urange(35, 55);
    p.base_bass = (int)yp_urange(30, 45);
    p.base_echo = (int)yp_urange(35, 55);
    {                                        /* the details' recipe, for those left on AUTO */
        uint32_t k;
        for (k = 0; k < NGMAP; k++) {
            int r = 0;
            switch (GMAP[k].param) {
            case P_SIZE: r = (int)yp_urange(84, 95); break;
            case P_PADTONE: r = (int)(yp_rnd() & 1u); break;               /* Rhodes or Voices */
            case P_BRIGHT: r = (int)yp_urange(25, 45); break;
            case P_DRIFT: r = (int)yp_urange(55, 72); break;               /* a chord every 1.5 .. 3 minutes */
            case P_DENSITY: r = (int)yp_urange(22, 38); break;
            case P_BTONE: r = (int)yp_urange(15, 45); break;
            case P_BOCT: r = (int)yp_urange(0, 1) - (p.program == YP_ERDEN); break;
            case P_BASSOCT: r = p.program == YP_ERDEN ? -1 : 0; break;
            case P_GLIDE: r = (int)yp_urange(30, 55); break;
            case P_FIFTH: r = (int)yp_urange(15, 35); break;
            case P_ETIME: r = (int)yp_urange(4, 5); break;                 /* 2 or 4 bars */
            case P_REVERB: r = (int)yp_urange(70, 82); break;
            case P_MOTION: r = (int)yp_urange(35, 60); break;
            default: break;
            }
            p.rec[k] = (int16_t)r;
        }
    }
    en_set(P_PULSE, 0);
    en_set(P_HITS, 0);
    en_set(P_MIDI, 0);
    en_set(P_TEMPO, (int)yp_urange(52, 64));
    en_set(P_EVOLVE, (int)yp_urange(30, 50));
    en_set(P_SWELL, (int)yp_urange(70, 88));
    en_set(P_MODE, mode);
    en_set(P_SWING, 0);
    yp_garden_details(s, 1);
    en_pool(EN_MODE_POOL[mode]);
    p.g_root_carrier = 0xFF;
    yp_garden_key(p.ph[0].carrier);
    yp_garden_levels(s, 1);
    en_seed(seed);
    en_regrow();
    en_play(1);
}

/* ---- the yoga layer --------------------------------------------------------------------------------------------- */
static float yp_ring(const yp_settings_t *s) { return 0.4f + 0.12f * (float)s->x[YX_RING]; }
static void yp_strike_bowl(float level, const yp_settings_t *s, int carrier)
{
    float hz = (float)YP_CARRIER[carrier], top = s->x[YX_BPITCH] < 0 ? 150.0f : s->x[YX_BPITCH] > 0 ? 600.0f : 300.0f;
    while (hz > top)
        hz *= 0.5f;
    level *= (float)s->bowls / 6.0f;
    if (level > 0.0f)
        yoga_dsp_strike(YOGA_BOWL, hz, level, yp_frange(0.35f, 0.65f), yp_ring(s));
}
static void yp_strike_chime(const yp_settings_t *s)
{
    static const float RATIO[6] = {1.0f, 1.125f, 1.25f, 1.5f, 1.6667f, 2.0f};   /* pentatonic over the carrier */
    float hz = (float)YP_CARRIER[p.carrier_now] * RATIO[yp_rnd() % 6u];
    while (hz < 450.0f)
        hz *= 2.0f;
    while (hz > 1300.0f)
        hz *= 0.5f;
    if (s->bowls && s->x[YX_CHIMES] != 0)
        yoga_dsp_strike(YOGA_CHIME, hz, yp_frange(0.2f, 0.4f) * (float)s->bowls / 6.0f, yp_frange(0.15f, 0.85f),
                        yp_ring(s));
}
static void yp_drift_next(uint32_t t)
{
    p.oct[0] = p.oct[1];
    p.fif[0] = p.fif[1];
    p.det[0] = p.det[1];
    p.brh[0] = p.brh[1];
    p.oct[1] = yp_frange(0.12f, 0.4f);
    p.fif[1] = yp_frange(0.04f, 0.2f);
    p.det[1] = yp_frange(0.05f, 0.2f);
    p.brh[1] = yp_frange(0.075f, 0.108f);    /* 4.5 .. 6.5 breaths a minute */
    p.drift_t0 = t;
    p.drift_t1 = t + yp_urange(20000, 45000);
}
static void yp_schedule_chime(uint32_t t, int deep, const yp_settings_t *s)
{
    uint32_t w = yp_urange(25000, 70000);
    if (s->x[YX_CHIMES] != YX_AUTO && s->x[YX_CHIMES] > 0)
        w = w * 4u / (uint32_t)s->x[YX_CHIMES];         /* 1: every 2..5 minutes .. 10: every 10..28 s */
    else if (deep)
        w += w / 2u;
    if (s->bowls < 4u)
        w *= 2u;
    p.next_chime = t + w;
}

void yp_start(const yp_settings_t *s, uint32_t seed, uint32_t now)
{
    uint32_t keep = p.rng, mix, total, wake, i;
    yp_init();
    mix = (seed ^ keep) * 0x9E3779B1u;
    mix ^= mix >> 15;
    mix *= 0x85EBCA6Bu;
    mix ^= mix >> 13;
    p.rng = mix ? mix : 0xA511E9B3u;
    p.program = s->program < YP_NPROG ? s->program : YP_REISE;
    p.manual = p.program == YP_MANUAL;
    p.last_now = now;
    p.state = YS_RUNNING;
    p.pause_gain = 1.0f;
    p.fade_in = yp_urange(20000, 30000);
    p.iso_depth = yp_frange(0.55f, 0.7f);
    wake = yp_urange(60000, 90000);
    if (p.manual) {
        p.endless = s->timer == 0;
        total = p.endless ? 0xFFFFFFFFu : (uint32_t)s->timer * 300000u;
        p.nph = 1;
        p.ph[0].carrier = s->carrier % YP_NCARRIER;
        p.ph[0].beat_from = p.ph[0].beat_to = (float)s->beat_d * 0.1f;
        p.ph[0].stage = "RUHEN";
        p.ph[0].stage_id = YST_RUHEN;
        p.ph[0].start = 0;
        p.ph[0].end = p.endless ? 0xFFFFFFFFu : total - wake;
        p.ph[0].glide = 1;
    } else {
        static const char *const STAGE[3] = {"ANKOMMEN", "VERTIEFEN", "RUHEN"};
        uint8_t car[MAXPH], wt[MAXPH], an[MAXPH + 1];
        uint32_t minutes = YP_MINUTES[s->duration % YP_NDURATION], main, w[MAXPH], sum = 0, at = 0;
        float beat;
        p.endless = minutes == 0;            /* endless: the arc of 20 minutes, then the last phase holds */
        total = (p.endless ? 20u : minutes) * 60000u;
        if (minutes && minutes <= 5u)
            wake = 60000u;
        main = total - wake;
        if (p.program == YP_REISE) {
            p.nph = 4;
            car[0] = 0, car[1] = 1, car[2] = 2, car[3] = 3;
            if (yp_rnd() % 100u < 35u)
                car[1] = 2, car[2] = 1;
            wt[0] = 2, wt[1] = 2, wt[2] = 3, wt[3] = 2;
            an[0] = TH_HI, an[1] = TH_LO, an[2] = DE_HI, an[3] = DE_LO, an[4] = DE_LO;
        } else {
            p.nph = 3;
            car[0] = car[1] = car[2] = p.program;    /* ERDEN..WANDLUNG: the carrier's index */
            wt[0] = 3, wt[1] = 3, wt[2] = 4;
            an[0] = TH_HI, an[1] = TH_LO, an[2] = DE_HI, an[3] = DE_LO;
        }
        for (i = 0; i < p.nph; i++) {
            w[i] = wt[i] * yp_urange(850, 1150);       /* each phase +-15 % */
            sum += w[i];
        }
        beat = yp_frange(ANCHOR[an[0]][0], ANCHOR[an[0]][1]);
        for (i = 0; i < p.nph; i++) {
            phase_t *ph = &p.ph[i];
            ph->start = at;
            ph->end = i + 1u == p.nph ? main : at + (uint32_t)((float)main * ((float)w[i] / (float)sum));
            ph->glide = ph->end - ph->start;
            at = ph->end;
            ph->carrier = car[i];
            ph->beat_from = beat;
            beat = yp_frange(ANCHOR[an[i + 1]][0], ANCHOR[an[i + 1]][1]);
            ph->beat_to = beat;
            ph->stage = p.program == YP_REISE ? YP_NAME[car[i]] : STAGE[i];
            ph->stage_id = (uint8_t)(p.program == YP_REISE ? YST_C0 + car[i] : YST_ANKOMMEN + i);
        }
        for (i = 0; i + 1u < p.nph; i++) {           /* the crossfades: never more than a third of either side */
            uint32_t half = yp_urange(17500, 27500), a = (p.ph[i].end - p.ph[i].start) / 3u,
                     b = (p.ph[i + 1].end - p.ph[i + 1].start) / 3u;
            if (half > a)
                half = a;
            if (half > b)
                half = b;
            p.xf_half[i] = p.ph[i].carrier != p.ph[i + 1].carrier ? half : 0u;
        }
        if (p.endless)                       /* the last phase reaches its rate, then holds it */
            p.ph[p.nph - 1].end = 0xFFFFFFFFu;
    }
    p.wake_start = p.ph[p.nph - 1].end;
    p.end = p.endless ? 0xFFFFFFFFu : total;
    p.beta_from = yp_frange(14.5f, 15.5f);
    p.beta_to = yp_frange(16.0f, 18.0f);
    p.carrier_now = p.carrier_to = p.ph[0].carrier;
    p.slot_carrier[0] = p.slot_carrier[1] = p.ph[0].carrier;
    p.blend = 0.0f;
    p.m_slot = 0;
    p.beat = p.ph[0].beat_from;
    p.oct[1] = yp_frange(0.12f, 0.4f);
    p.fif[1] = yp_frange(0.04f, 0.2f);
    p.det[1] = yp_frange(0.05f, 0.2f);
    p.brh[1] = yp_frange(0.075f, 0.108f);
    yp_drift_next(0);
    p.opening_bowl = 2000u;
    yp_schedule_chime(10000u, 0, s);
    yp_garden_start(s, yp_rnd());
}

void yp_pause(uint32_t now)
{
    if (p.state == YS_RUNNING)
        p.state = YS_PAUSED;
    else if (p.state == YS_PAUSED) {
        p.state = YS_RUNNING;
        p.last_now = now;
    }
}

void yp_finish(void)
{
    uint32_t i;
    if ((p.state != YS_RUNNING && p.state != YS_PAUSED) || p.elapsed >= p.wake_start || p.stopping)
        return;
    for (i = 0; i < p.nph; i++) {
        if (p.ph[i].end > p.elapsed) {
            p.ph[i].beat_to = p.beat;
            p.ph[i].end = p.elapsed;
            p.ph[i].glide = p.elapsed - p.ph[i].start;
            p.xf_half[i] = 0;
            p.nph = (uint8_t)(i + 1u);
            break;
        }
    }
    if (i > 0 && p.xf_half[i - 1] && p.elapsed < p.ph[i].start + p.xf_half[i - 1])
        p.xf_half[i - 1] = 0;                       /* mid-crossfade: the new drone stays (blend settles) */
    p.wake_start = p.elapsed;
    p.end = p.elapsed + yp_urange(60000, 75000);
    p.endless = 0;
    if (p.state == YS_PAUSED)
        p.state = YS_RUNNING;
}

void yp_stop(void)
{
    if (p.state == YS_IDLE)
        return;
    p.stopping = 1;
    yoga_dsp_silence();
    en_play(0);
}

static uint32_t yp_phase_at(uint32_t t)
{
    uint32_t i;
    for (i = 0; i < p.nph; i++)
        if (t < p.ph[i].end)
            return i;
    return p.nph ? p.nph - 1u : 0u;
}

/* the hall and echo before any program (the handpan plays from power-on): a resting room, the player's details
 * where they are set */
void yp_room(const yp_settings_t *s)
{
    static const struct { uint8_t yx, param; int16_t def; } ROOM[] = {
        {YX_RSIZE, P_SIZE, 88}, {YX_EREVERB, P_REVERB, 75}, {YX_ETIME, P_ETIME, 4}, {YX_EMOTION, P_MOTION, 45}};
    uint32_t k, g;
    en_set(P_PULSE, 0);
    en_set(P_MIDI, 0);
    en_set(P_TEMPO, 60);
    en_set(P_ECHO, yp_scaled(45, s->echo));
    for (k = 0; k < sizeof ROOM / sizeof ROOM[0]; k++) {
        int v = s->x[ROOM[k].yx], want = ROOM[k].def;
        if (v != YX_AUTO)
            for (g = 0; g < NGMAP; g++)
                if (GMAP[g].yx == ROOM[k].yx)
                    want = v * GMAP[g].mul + GMAP[g].add;
        en_set(ROOM[k].param, want);
    }
}

/* ---- the handpan ------------------------------------------------------------------------------------------------ */
static int yp_hand_carrier(const yp_settings_t *s)
{
    if (s->x[YX_HDING] != YX_AUTO)
        return s->x[YX_HDING];
    if (p.state != YS_IDLE)
        return p.carrier_now;                /* playing: the carrier sounding now */
    if (s->program == YP_MANUAL)
        return s->carrier % YP_NCARRIER;
    return s->program < YP_REISE ? s->program : 0;
}
float yp_hand_hz(const yp_settings_t *s, int key)
{
    const uint8_t *sc = SCALE[(unsigned)s->x[YX_HSCALE] % YP_NSCALE];
    float ding = (float)YP_CARRIER[yp_hand_carrier(s)];
    int semis;
    while (ding > 280.0f)                    /* a handpan's ding: 174, 198, 216, 264 Hz */
        ding *= 0.5f;
    semis = key <= 0 ? 0 : key <= 8 ? sc[key - 1] : sc[(key - 9) % 8] + 12;
    return ding * fm_exp2f((float)semis * (1.0f / 12.0f));
}
/* the Koshi: a key lets a few rods ring, one after another, as a breeze does (yp_update plays them) */
enum { KOSHI_Q = 16 };
static struct {
    float hz, vel, pan;
    int32_t wait;                            /* ms */
} kq[KOSHI_Q];

void yp_hand(const yp_settings_t *s, int key, int vel)
{
    static const int KIND[YI_N] = {YOGA_HAND_NOTE, YOGA_HAND_KALIMBA, YOGA_HAND_TONGUE, YOGA_HAND_BOWL, YOGA_HAND_CHIME};
    static const float OCT[YI_N] = {1.0f, 2.0f, 1.0f, 1.0f, 4.0f};    /* each instrument's register over the ding */
    int inst = (unsigned)s->x[YX_HINST] < YI_N ? s->x[YX_HINST] : YI_HANDPAN;
    float v = (float)clamp(vel, 1, 127) * (1.0f / 127.0f), d = (float)s->x[YX_HDYN] * 0.1f, pan,
          ring = 0.4f + 0.12f * (float)s->x[YX_HRING], bright = (float)s->x[YX_HBRIGHT] * 0.1f;
    if (key < 0 || key >= YP_HAND_KEYS)
        return;
    v = 1.0f - d * (1.0f - fm_powf(v, 1.3f));            /* DYNAMIK 0: every note alike .. 10: all the touch */
    if (inst == YI_HANDPAN && key == YP_HAND_TAK) {      /* the rim, a little right: two octaves over the ding */
        yoga_dsp_hand(yp_hand_hz(s, 0) * 4.0f, v, 0.62f, 1.0f, bright, YOGA_HAND_TAK);
        return;
    }
    if (inst == YI_KOSHI) {                  /* 3..6 rods around the key's note, 40..220 ms apart */
        int n = (int)yp_urange(3, 6), i, q = 0;
        int32_t wait = 0;
        for (i = 0; i < n; i++) {
            int k = clamp(key + (int)yp_urange(0, 4) - 2, 0, YP_HAND_TAK - 1);
            while (q < KOSHI_Q && kq[q].vel > 0.0f)
                q++;
            if (q >= KOSHI_Q)
                break;
            kq[q].hz = yp_hand_hz(s, k) * OCT[YI_KOSHI];
            kq[q].vel = v * (i ? yp_frange(0.35f, 0.9f) : 1.0f);
            kq[q].pan = yp_frange(0.2f, 0.8f);
            kq[q].wait = wait;
            wait += (int32_t)yp_urange(40, 220);
        }
        return;
    }
    /* the tone fields around the ding, left and right in turn, as on the steel */
    pan = key ? 0.5f + ((key & 1) ? -1.0f : 1.0f) * 0.07f * (float)((key + 1) / 2 % 5 + 1) : 0.5f;
    yoga_dsp_hand(yp_hand_hz(s, key) * OCT[inst], v, fm_clampf(pan, 0.1f, 0.9f), ring, bright,
                  inst == YI_HANDPAN && key == 0 ? YOGA_HAND_DING : KIND[inst]);
}
static void yp_koshi(const yp_settings_t *s, uint32_t dt)
{
    int q;
    for (q = 0; q < KOSHI_Q; q++) {
        if (kq[q].vel <= 0.0f)
            continue;
        kq[q].wait -= (int32_t)dt;
        if (kq[q].wait <= 0) {
            yoga_dsp_hand(kq[q].hz, kq[q].vel, kq[q].pan, 0.4f + 0.12f * (float)s->x[YX_HRING],
                          (float)s->x[YX_HBRIGHT] * 0.1f, YOGA_HAND_CHIME);
            kq[q].vel = 0.0f;
        }
    }
}
static void yp_hand_targets(const yp_settings_t *s, yoga_target_t *y)
{
    y->hand_level = (float)s->hand / 7.0f;
    y->hand_space = (float)s->x[YX_HSPACE] * 0.08f;
    y->hand_echo = (float)s->x[YX_HECHO] * 0.06f;
}

void yp_update(const yp_settings_t *s, uint32_t now)
{
    uint32_t dt = now - p.last_now, t, i;
    float level = 1.0f, bell = 1.0f, garden = 1.0f, drone = 0.6f, depth, oct_add = 0.0f;
    int deep = 0;
    yoga_target_t y;
    p.last_now = now;
    if (dt > 1000u)
        dt = 1000u;                                  /* a stalled frame must not skip the timeline */
    y.mode = s->mode;
    ym_step(dt);
    yp_koshi(s, dt);
    if (p.state == YS_IDLE) {
        y.carrier[0] = y.carrier[1] = 0.0f;
        y.blend = y.beat = y.detune = y.breath = y.level = y.bell_level = y.drone = y.octave = y.fifth = 0.0f;
        y.noise = y.iso_depth = y.space = y.garden = 0.0f;
        y.twin = y.swell = y.noise_tone = y.echo = 0.0f;
        yp_hand_targets(s, &y);
        yoga_dsp_set(&y);
        return;
    }
    {                                                /* pause and stop ramp over two seconds */
        float goal = p.state == YS_RUNNING && !p.stopping ? 1.0f : 0.0f, step = (float)dt / 2000.0f;
        p.pause_gain += fm_clampf(goal - p.pause_gain, -step, step);
        if (p.stopping && p.pause_gain <= 0.0f) {
            p.state = YS_IDLE;
            p.stopping = 0;
        }
    }
    if (p.state == YS_RUNNING && !p.stopping)
        p.elapsed += dt;
    t = p.elapsed;
    if (p.state != YS_DONE && p.state != YS_IDLE && !p.endless && t >= p.end) {
        p.state = YS_DONE;
        en_play(0);
        t = p.end;
    }
    {
        float d0 = s->x[YX_PULSE] == YX_AUTO ? p.iso_depth : 0.08f * (float)s->x[YX_PULSE];
        depth = d0;
        p.iso_now = d0;
    }
    p.crossing = 0;
    if (p.manual && t < p.wake_start) {              /* the manual's live values: a new carrier crossfades */
        uint8_t c = s->carrier % YP_NCARRIER;
        p.ph[0].beat_from = p.ph[0].beat_to = (float)s->beat_d * 0.1f;
        if (c != p.slot_carrier[p.m_slot] && !(p.m_xf_start && t - p.m_xf_start < MANUAL_XF)) {
            p.m_from_slot = p.m_slot;
            p.m_slot ^= 1u;
            p.slot_carrier[p.m_slot] = c;
            p.m_xf_start = t ? t : 1u;
            p.carrier_to = c;
            yp_garden_key(c);
            yp_strike_bowl(0.35f, s, c);
        }
    }
    if (t < p.wake_start) {
        phase_t *ph;
        i = yp_phase_at(t);
        ph = &p.ph[i];
        p.waking = 0;
        p.beat = yp_lerp(ph->beat_from, ph->beat_to, t - ph->start, ph->glide);
        deep = p.beat < 4.0f;
        if (p.manual) {
            if (p.m_xf_start) {
                float x = yp_smoothstep((float)(t - p.m_xf_start) / (float)MANUAL_XF);
                p.blend = p.m_slot ? x : 1.0f - x;
                p.crossing = x < 1.0f;
                p.carrier_now = p.slot_carrier[x < 0.5f ? p.m_from_slot : p.m_slot];
                if (x >= 1.0f)
                    p.m_xf_start = 0;
            } else {
                p.blend = (float)p.m_slot;
                p.carrier_now = p.carrier_to = p.slot_carrier[p.m_slot];
            }
        } else {
            uint32_t a = i & 1u;                     /* phase i's drone slot */
            p.slot_carrier[a] = ph->carrier;
            p.blend = (float)a;
            p.carrier_now = p.carrier_to = ph->carrier;
            if (i + 1u < p.nph && p.xf_half[i] && t + p.xf_half[i] > ph->end) {          /* into the next */
                float x = yp_smoothstep((float)(t + p.xf_half[i] - ph->end) / (float)(2u * p.xf_half[i]));
                p.slot_carrier[a ^ 1u] = p.ph[i + 1].carrier;
                p.blend = a ? 1.0f - x : x;
                p.crossing = 1;
                p.carrier_to = p.ph[i + 1].carrier;
            } else if (i > 0 && p.xf_half[i - 1] && t < ph->start + p.xf_half[i - 1]) {   /* out of the last */
                float x = yp_smoothstep((float)(t + p.xf_half[i - 1] - ph->start) / (float)(2u * p.xf_half[i - 1]));
                p.slot_carrier[a ^ 1u] = p.ph[i - 1].carrier;
                p.blend = a ? x : 1.0f - x;
                p.crossing = 1;
                p.carrier_now = p.ph[i - 1].carrier;
                p.carrier_to = ph->carrier;
            }
            if (i != p.last_ph) {                    /* the boundary: the middle of a crossfade */
                int changed = p.ph[i - 1].carrier != ph->carrier;
                p.last_ph = (uint8_t)i;
                yp_garden_key(ph->carrier);
                if (yp_frnd() < (changed ? 0.5f : 0.4f))
                    yp_strike_bowl(changed ? 0.4f : 0.5f, s, ph->carrier);
                yp_drift_next(t);
            }
        }
        p.beat_at_wake = p.beat;
    } else {
        /* the wake-up: up through theta and alpha into a gentle beta, brighter, three bowls, then silence */
        uint32_t len = p.end - p.wake_start, w = t - p.wake_start, rise = len * 6u / 10u;
        static const uint16_t BOWL_AT[3] = {40, 400, 720};   /* per mille of the wake-up */
        if (!p.waking) {
            p.waking = 1;
            p.beat_at_wake = p.beat;
        }
        p.beat = w < rise ? yp_lerp(p.beat_at_wake, p.beta_from, w, rise) : yp_lerp(p.beta_from, p.beta_to, w - rise, len - rise);
        depth = yp_lerp(p.iso_now, fm_minf(p.iso_now, 0.3f), w, rise);
        drone = yp_lerp(0.6f, 0.47f, w, rise);
        oct_add = 0.12f;
        if (p.wake_bowls < 3u && w >= len * BOWL_AT[p.wake_bowls] / 1000u) {
            p.wake_bowls++;
            yp_strike_bowl(p.wake_bowls == 3u ? 0.85f : 0.7f, s, p.carrier_now);
        }
        if (len - w < FINAL_FADE) {
            float f = yp_lerp(1.0f, 0.0f, FINAL_FADE - (len - w), FINAL_FADE);
            level = f * f;
            bell = f;
            garden = f * f;
        }
        if (p.state == YS_DONE)
            level = bell = garden = 0.0f;
    }
    if (p.opening_bowl && t >= p.opening_bowl) {
        p.opening_bowl = 0;
        yp_strike_bowl(0.75f, s, p.carrier_now);
    }
    if (t >= p.next_chime && p.state == YS_RUNNING) {
        if (p.next_chime > p.fade_in && (!p.waking || t + FINAL_FADE < p.end))
            yp_strike_chime(s);
        yp_schedule_chime(t, deep, s);
        if (p.waking)
            p.next_chime = t + yp_urange(12000, 25000);
    }
    if (t >= p.drift_t1)
        yp_drift_next(t);
    if (t < p.fade_in) {                             /* fade in over 20..30 s */
        float f = (float)t / (float)p.fade_in;
        level *= f * f;
        garden *= f;
    }
    level *= p.pause_gain;
    bell *= p.pause_gain;
    garden *= p.pause_gain;
    yp_garden_levels(s, 0);
    yp_garden_details(s, 0);
    {
        uint32_t dpos = t - p.drift_t0, dlen = p.drift_t1 - p.drift_t0;
        y.carrier[0] = (float)YP_CARRIER[p.slot_carrier[0]];
        y.carrier[1] = (float)YP_CARRIER[p.slot_carrier[1]];
        y.blend = p.blend;
        y.beat = p.beat;
        const int8_t *x = s->x;
        y.detune = x[YX_CHORUS] == YX_AUTO ? yp_lerp(p.det[0], p.det[1], dpos, dlen) : 0.025f * (float)x[YX_CHORUS];
        y.twin = x[YX_CHORUS] == YX_AUTO ? 0.25f : 0.04f * (float)x[YX_CHORUS];
        y.breath = x[YX_BRATE] == YX_AUTO ? yp_lerp(p.brh[0], p.brh[1], dpos, dlen) : (float)x[YX_BRATE] / 60.0f;
        y.octave = (x[YX_HARM] == YX_AUTO ? yp_lerp(p.oct[0], p.oct[1], dpos, dlen) : 0.045f * (float)x[YX_HARM]) + oct_add;
        y.fifth = x[YX_HARM] == YX_AUTO ? yp_lerp(p.fif[0], p.fif[1], dpos, dlen) : 0.022f * (float)x[YX_HARM];
        y.swell = 0.03f * (float)x[YX_BSWELL];
        y.noise_tone = 0.1f * (float)x[YX_BCOLOR];
        y.echo = (float)x[YX_RECHO] / 6.0f;
        yp_hand_targets(s, &y);
        y.level = level;
        const float kv = (float)s->klang / 7.0f;                /* KLANG's volume (HOME K1) */
        y.bell_level = bell * ym_gain(YM_BOWLS, YM_KLANG) * kv;
        y.drone = drone * (float)s->drone / 7.0f * ym_gain(YM_DRONE, YM_KLANG) * kv;
        y.noise = 0.3f * (float)s->breath / 7.0f * ym_gain(YM_BREATH, YM_KLANG) * kv;
        y.iso_depth = depth;
        y.space = 0.5f * (float)s->space / 7.0f * ym_gain(YM_SPACE, YM_KLANG) * kv;
        y.echo *= ym_gain(YM_SPACE, YM_KLANG);
        y.garden = garden * ym_gain(YM_GARTEN, -1) * (float)s->garten / 7.0f;   /* GARTEN's volume (HOME K2) */
        yoga_dsp_set(&y);
    }
}

void yp_status(yp_status_t *st)
{
    st->state = p.state;
    st->program = YP_NAME[p.program % YP_NPROG];
    st->prog = p.program;
    st->carrier = (float)YP_CARRIER[p.carrier_now];
    st->carrier_to = (float)YP_CARRIER[p.carrier_to];
    st->crossing = p.crossing;
    st->beat = p.beat;
    st->band = (uint8_t)yp_band(p.beat);
    st->waking = p.waking;
    st->elapsed_s = p.elapsed / 1000u;
    st->remaining_s = 0;
    st->progress = 0;
    if (p.state == YS_IDLE)
        st->stage = "", st->stage_id = YST_NONE;
    else if (p.state == YS_DONE)
        st->stage = "NAMASTE", st->stage_id = YST_NAMASTE;
    else if (p.waking)
        st->stage = "AUFWACHEN", st->stage_id = YST_AUFWACHEN;
    else
        st->stage = p.ph[yp_phase_at(p.elapsed)].stage, st->stage_id = p.ph[yp_phase_at(p.elapsed)].stage_id;
    if (!p.endless && p.state != YS_IDLE) {
        uint32_t t = p.elapsed < p.end ? p.elapsed : p.end;
        st->remaining_s = (p.end - t + 999u) / 1000u;
        st->progress = (uint16_t)(1000.0f * (float)t / (float)(p.end ? p.end : 1u));
    }
}
