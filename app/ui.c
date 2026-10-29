/* SPDX-License-Identifier: MPL-2.0 */
/* The panel: a calm player for the end of a yoga class.
 *
 *   HOME        the player (held 2 s: stop)       PLAY      start / pause / on; after the end: a new one
 *   FX          KLANG: drone, breath, bowls, room  REC held  wake up now
 *   ENV         GARTEN: pads, bells, bass, echo    SAVE      keep the settings (lit while changed)
 *               (SELECT turns from the overview of their levels to each part's own page: its level and
 *               three details, most of them AUTO at first: each start draws them)
 *   GLO         SETUP: SYSTEM (the LED breath, dimming, the language: app/lang.c) and TAKT (a silent metronome:
 *               its tempo, and whether it shows as a dot on the screen, on the LFO button or both), by SELECT
 *   SEL         the instrument on the keys (handpan, kalimba, tongue drum, bowls, Koshi): SPIEL (which, level, scale,
 *               ding), KLANG (ring, brightness, touch), RAUM (hall, echo), by SELECT
 *   small keys  mute, lit while their part sounds: 1 KLANG, 2-5 drone, breath, bowls, room; 6 GARTEN, 7-10 pads, bells,
 *               bass, echo; 11 everything (the handpan plays on). Live, not saved; each a 1.5 s fade
 *   SELECT      the program (while idle)           white keys: the handpan (ding, its scale, an octave up),
 *                                                            with the touch's velocity, at any time
 *   PRESETS     the duration / timer (idle)        K1..K4: the page's four values; on the player K1 KLANG and
 *   ALGORITHM   ISO / BIN, any time                          K2 GARTEN (volumes), in MANUELL K3 the rate, K4 the carrier
 *
 * The screen: a header (name or message), the middle (the program, its stage and a ring that breathes with the
 * layer, the carrier in it; or a page's four values) and the foot (band, rate, time, progress). Drawn in strips
 * (gfx.c canvases), each only when it changed. */

enum { PG_PLAYER, PG_KLANG, PG_GARTEN, PG_SETUP, PG_HAND, NPAGE };
enum { HOLD_WAKE = 1000, HOLD_STOP = 2000 };

#define COL_BG RGB(0, 0, 0)
#define COL_TEXT RGB(214, 208, 196)          /* warm light grey */
#define COL_SOFT RGB(130, 128, 122)
#define COL_FAINT RGB(58, 58, 60)
#define COL_ACCENT RGB(96, 196, 200)         /* calm teal */
#define COL_WARM RGB(220, 168, 96)           /* sand */
#define COL_LOW RGB(30, 34, 36)

static const uint8_t WK[16] = PLAT_WHITE_KEYS;
static const uint8_t BK[11] = PLAT_BLACK_KEYS;
/* the small keys mute (yoga_program.h YM_*), in their order on the keyboard: 3 + 2 | 3 + 2 | 1 */
static const uint8_t MUTE_NAME[YM_N] = {S_KLANG, S_DRONE, S_ATEM, S_SCHALEN, S_RAUM,
                                        S_GARTEN, S_PADS, S_GLOCKEN, S_BASS, S_ECHO, S_ALLES};
/* key -> its white key's place (0 the lowest), -1 black */
static const int8_t KEY_WHITE[NKEYS] = {0, -1, 1, -1, 2, -1, 3, 4, -1, 5, -1, 6, 7, -1, 8, -1, 9, -1, 10, 11, -1, 12, -1,
                                        13, 14, -1, 15};

static struct {
    char msg[2][24];
    uint32_t msg_until;
    uint32_t btn, keys;
    uint32_t rec_t, home_t;                  /* when held (0: not, or its hold has acted) */
    uint32_t last_input, entropy;
    uint8_t page, dirty;
    uint8_t sub[NPAGE];                      /* each page's part (SELECT) */
    uint32_t held;                           /* the keys held (the handpan's, lit) */
    uint32_t beat_t0;                        /* the metronome: when its count began */
    int32_t head_end;                        /* where the header's words end (the beat's dot keeps clear) */
    uint8_t dot_shown;                       /* the dot's brightness as drawn (0..8), 0xFF: draw again */
    uint8_t redraw_head, redraw_mid, redraw_foot;
    /* what was drawn: redraw a strip when its signature changes */
    uint32_t sig_head, sig_mid, sig_foot;
    yp_status_t st;
} ui;

static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}
static void uint_text(uint32_t v, char *b)
{
    char t[12];
    int n = 0;
    do
        t[n++] = (char)('0' + v % 10u);
    while ((v /= 10u) && n < 11);
    while (n)
        *b++ = t[--n];
    *b = 0;
}
static void hz_text(float hz, int tenths, char *b)  /* "432" or "5.5" */
{
    uint32_t v = (uint32_t)(hz * (tenths ? 10.0f : 1.0f) + 0.5f);
    char t[12];
    if (!tenths) {
        uint_text(v, b);
        return;
    }
    uint_text(v / 10u, b);
    text_cat(b, ".");
    t[0] = (char)('0' + v % 10u);
    t[1] = 0;
    text_cat(b, t);
}
static void time_text(uint32_t s, char *b)
{
    char t[4];
    uint_text(s / 60u, b);
    text_cat(b, ":");
    t[0] = (char)('0' + s % 60u / 10u);
    t[1] = (char)('0' + s % 10u);
    t[2] = 0;
    text_cat(b, t);
}
static uint32_t mix(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    ui.redraw_head = 1;
    if (ann_on()) {
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

/* ---- what the settings say --------------------------------------------------------------------------------- */
static int idle(void) { return ui.st.state == YS_IDLE || ui.st.state == YS_DONE; }
static uint32_t minutes(void)
{
    return set.yp.program == YP_MANUAL ? set.yp.timer * 5u : YP_MINUTES[set.yp.duration % YP_NDURATION];
}
static void minutes_text(char *b)
{
    b[0] = 0;
    if (!minutes()) {
        text_cat(b, T(S_ENDLOS));
        return;
    }
    uint_text(minutes(), b);
    text_cat(b, " ");
    text_cat(b, T(S_MIN));
}
static const char *program_shown(void) { return prog_name(idle() && ui.st.state != YS_DONE ? set.yp.program : ui.st.prog); }
static float carrier_shown(void)
{
    if (!idle() || ui.st.state == YS_DONE)
        return ui.st.carrier;
    if (set.yp.program == YP_MANUAL)
        return (float)YP_CARRIER[set.yp.carrier];
    return (float)YP_CARRIER[set.yp.program < YP_REISE ? set.yp.program : 0];
}

/* the pages' rows: a level (0..10, or a setting) or a detail (yoga_program.h YX_*). KLANG and GARTEN have an
 * overview (the four levels) and a page for each of their four parts, turned by SELECT */
/* the pages' and their parts' names (app/lang.c S_*; -1 none) */
static const int8_t PAGE_NAME[NPAGE] = {-1, S_KLANG, S_GARTEN, S_SETUP, -1};
static const int8_t SUB_NAME[NPAGE][5] = {{-1, -1, -1, -1, -1}, {-1, S_DRONE, S_ATEM, S_SCHALEN, S_RAUM},
                                          {-1, S_PADS, S_GLOCKEN, S_BASS, S_ECHO}, {S_SYSTEM, S_TAKT, -1, -1, -1},
                                          {S_SPIEL, S_KLANG_SUB, S_RAUM, -1, -1}};
static const char *sub_name(int pg, int sub) { return SUB_NAME[pg][sub] >= 0 ? T(SUB_NAME[pg][sub]) : ""; }
enum { NSUB = 5 };
typedef struct {
    const char *label;
    uint8_t *level;                          /* a level row */
    int8_t yx;                               /* or a detail row (-1: none) */
    uint8_t max;                             /* a level's top */
} row_t;
static int page_subs(int pg) { return pg == PG_KLANG || pg == PG_GARTEN ? NSUB : pg == PG_HAND ? 3 : pg == PG_SETUP ? 2 : 1; }
/* a page's name: the instrument page's (SEL) is its instrument's */
static const char *page_name(int pg)
{
    return pg == PG_HAND ? inst_name(set.yp.x[YX_HINST]) : PAGE_NAME[pg] >= 0 ? T(PAGE_NAME[pg]) : "";
}
static row_t row(int pg, int sub, int k)
{
    static const int8_t DETAIL[2][NSUB][3] = {
        {{-1, -1, -1}, {YX_HARM, YX_CHORUS, YX_PULSE}, {YX_BRATE, YX_BCOLOR, YX_BSWELL},
         {YX_CHIMES, YX_BPITCH, YX_RING}, {YX_RSIZE, YX_RECHO, -1}},
        {{-1, -1, -1}, {YX_PCOLOR, YX_PBRIGHT, YX_PDRIFT}, {YX_GDENSITY, YX_GTONE, YX_GOCT},
         {YX_BOCT, YX_BGLIDE, YX_BFIFTH}, {YX_ETIME, YX_EREVERB, YX_EMOTION}}};
    uint8_t *const LV[2][4] = {{&set.yp.drone, &set.yp.breath, &set.yp.bowls, &set.yp.space},
                               {&set.yp.pad, &set.yp.bells, &set.yp.bass, &set.yp.echo}};
    /* SEL, the instrument: SPIEL (which, level, scale, ding), KLANG (ring, brightness, touch), RAUM (hall, echo);
     * -1 the level row, -2 none */
    static const int8_t HAND[3][4] = {{YX_HINST, -1, YX_HSCALE, YX_HDING},
                                      {YX_HRING, YX_HBRIGHT, YX_HDYN, -2},
                                      {YX_HSPACE, YX_HECHO, -2, -2}};
    row_t r = {"", 0, -1, 10};
    if (pg == PG_HAND) {
        int8_t x = HAND[sub % 3][k];
        if (x == -1)
            r.label = T(S_PEGEL), r.level = &set.yp.hand;
        else if (x >= 0)
            r.yx = x, r.label = yx_label(x);
        return r;
    }
    if (pg == PG_SETUP && sub == 1) {        /* TAKT: the silent metronome */
        if (k == 0)
            r.label = T(S_TEMPO), r.level = &set.beat, r.max = 120;
        else if (k == 1)
            r.label = T(S_ANZEIGE), r.level = &set.beat_show, r.max = 2;
        return r;
    }
    if (pg == PG_SETUP) {
        if (k == 0)
            r.label = T(S_LEDATEM), r.level = &set.led_guide, r.max = 1;
        else if (k == 1)
            r.label = T(S_DIMMEN), r.level = &set.dim, r.max = 2;
        else if (k == 2)
            r.label = T(S_SPRACHE), r.level = &set.lang, r.max = NLANG - 1;
        return r;
    }
    if (pg != PG_KLANG && pg != PG_GARTEN)
        return r;
    if (sub == 0) {                          /* the overview: the four parts' levels */
        r.label = sub_name(pg, k + 1);
        r.level = LV[pg - 1][k];
    } else if (k == 0) {                     /* a part: its level first */
        r.label = T(pg == PG_KLANG && sub == 4 ? S_ANTEIL : S_PEGEL);
        r.level = LV[pg - 1][sub - 1];
    } else if (DETAIL[pg - 1][sub][k - 1] >= 0) {
        r.yx = DETAIL[pg - 1][sub][k - 1];
        r.label = yx_label(r.yx);
    }
    return r;
}
/* the mute that silences a page's row (a level row of KLANG or GARTEN), or -1 */
static int row_mute(int pg, int sub, int k)
{
    int base = pg == PG_KLANG ? YM_KLANG : pg == PG_GARTEN ? YM_GARTEN : -1;
    if (base < 0 || (sub && k))
        return -1;
    return base + 1 + (sub ? sub - 1 : k);
}
static int row_silent(int pg, int sub, int k)
{
    int mt = row_mute(pg, sub, k);
    uint32_t m = yp_muted();
    return mt >= 0 && (m >> mt & 1u || m >> (pg == PG_KLANG ? YM_KLANG : YM_GARTEN) & 1u || m >> YM_ALL & 1u);
}
static void row_text(const row_t *r, char *b)
{
    b[0] = 0;
    if (r->yx >= 0) {
        const char *w = yx_word(r->yx, set.yp.x[r->yx]);
        if (w)
            text_cat(b, w);
        else
            yx_text(r->yx, set.yp.x[r->yx], b);
    } else if (r->level == &set.led_guide)
        text_cat(b, T(*r->level ? S_AN : S_AUS));
    else if (r->level == &set.dim) {
        if (*r->level == 0)
            text_cat(b, T(S_NIE));
        else
            text_cat(b, *r->level == 1 ? "30 S" : "2 MIN");
    } else if (r->level == &set.lang)
        text_cat(b, LANG_NAME[*r->level % NLANG]);
    else if (r->level == &set.beat) {
        if (!*r->level)
            text_cat(b, T(S_AUS));
        else {
            uint_text(*r->level, b);
            text_cat(b, " / MIN");
        }
    } else if (r->level == &set.beat_show)
        text_cat(b, *r->level == 0 ? T(S_DISPLAY) : *r->level == 1 ? "LFO" : T(S_BEIDE));
    else if (r->level)
        uint_text(*r->level, b);
}
/* a detail one step: AUTO sits below its lowest value */
static int detail_step(int i, int32_t d)
{
    const yx_info_t *x = &YX[i];
    int v = set.yp.x[i], idx = v == YX_AUTO ? -1 : v - x->lo, n = clamp(idx + d, x->autok ? -1 : 0, x->hi - x->lo);
    if (n == idx)
        return 0;
    set.yp.x[i] = (int8_t)(n < 0 ? YX_AUTO : x->lo + n);
    return 1;
}

/* ---- drawing ------------------------------------------------------------------------------------------------ */
static int dimmed(void)
{
    static const uint32_t AFTER[3] = {0xFFFFFFFFu, 30000u, 120000u};
    return ui.st.state == YS_RUNNING && plat_ms() - ui.last_input >= AFTER[set.dim % 3u];
}
static uint16_t tone(uint16_t c)
{
    if (!dimmed())
        return c;
    return c == COL_TEXT || c == COL_WARM ? COL_SOFT : c == COL_LOW ? COL_LOW : COL_FAINT;
}
static void centred(int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text((240 - text_w(f, s)) / 2, y, f, s, c);
}
static uint32_t ring_radius(void)
{
    uint32_t b = yoga_dsp_breath(), s = b < 32768u ? b : 65535u - b;   /* a triangle, 0 exhaled .. 32767 */
    float e = (float)s * (1.0f / 32768.0f);
    if (ui.st.state != YS_RUNNING && ui.st.state != YS_PAUSED)
        return 34;
    e = e * e * (3.0f - 2.0f * e);                                     /* soft at the turns */
    return 24u + (uint32_t)(e * 22.0f);
}
static void disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t dy;
    for (dy = -r; dy <= r; dy++) {
        int32_t h = (int32_t)fm_sqrtf((float)(r * r - dy * dy));
        cv_rect(cx - h, cy + dy, 2 * h + 1, 1, c);
    }
}

static void draw_head(void)
{
    cv_begin(240, 32, COL_BG);
    if (plat_ms() < ui.msg_until) {
        int32_t x = cv_text(10, 8, &FONT_B, ui.msg[0], tone(COL_TEXT));
        ui.head_end = cv_text(x + 8, 8, &FONT_B, ui.msg[1], tone(COL_ACCENT));
    } else {
        int32_t x = cv_text(10, 8, &FONT_B, ui.page == PG_PLAYER ? "YOGA" : page_name(ui.page), tone(COL_SOFT));
        if (page_subs(ui.page) > 1 && sub_name(ui.page, ui.sub[ui.page])[0])
            x = cv_text(x + 8, 8, &FONT_B, sub_name(ui.page, ui.sub[ui.page]), tone(COL_ACCENT));
        ui.head_end = x;
        cv_text(230 - text_w(&FONT_B, set.yp.mode == YOGA_BIN ? "BIN" : "ISO"), 8, &FONT_B,
                set.yp.mode == YOGA_BIN ? "BIN" : "ISO", tone(set.yp.mode == YOGA_BIN ? COL_WARM : COL_ACCENT));
        if (yp_muted() >> YM_ALL & 1u)         /* all of it muted; a part's mute shows on its key */
            cv_text(196 - text_w(&FONT_XS, T(S_STUMM)), 12, &FONT_XS, T(S_STUMM), tone(COL_WARM));
        else if (set.led_guide)
            cv_text(176, 12, &FONT_XS, "LED", tone(COL_FAINT));
    }
    cv_blit(0, 0);
    ui.dot_shown = 0xFF;                     /* the header covered the beat's dot: draw it again */
}

/* ---- the silent metronome: a fixed beat, no sound; a dot in the header that lights and fades, and/or LFO ---- */
static uint32_t beat_phase(void)            /* 0..999 within the beat; 1000: off */
{
    uint32_t period;
    if (!set.beat)
        return 1000;
    period = 60000u / set.beat;
    return (plat_ms() - ui.beat_t0) % period * 1000u / period;
}
static int beat_dot_x(void) { return clamp(ui.head_end + 14, 150, 158); }
static void draw_beat_dot(void)
{
    uint32_t ph = beat_phase();
    int lvl = 0;
    if (ph < 1000u && set.beat_show != 1u)  /* lit at the beat, fading over a third of it */
        lvl = ph < 333u ? 8 - (int)(ph * 8u / 333u) : 0;
    if (lvl == ui.dot_shown)
        return;
    ui.dot_shown = (uint8_t)lvl;
    cv_begin(14, 14, COL_BG);
    if (set.beat && set.beat_show != 1u) {
        /* off: a faint ring, so the dot's place is known; lit: teal, by its brightness */
        uint16_t c = lvl ? RGB(30 + 66 * lvl / 8, 34 + 162 * lvl / 8, 36 + 164 * lvl / 8) : COL_LOW;
        if (dimmed() && lvl)
            c = RGB(30 + 28 * lvl / 8, 34 + 60 * lvl / 8, 36 + 60 * lvl / 8);
        disc(7, 7, 5, c);
    }
    cv_blit((uint32_t)beat_dot_x() - 7u, 9);
}
static int beat_lfo_lit(void)
{
    uint32_t ph = beat_phase();
    return ph < 1000u && set.beat_show >= 1u && ph < 200u;
}

static void draw_mid(void)
{
    char t[24];
    cv_begin(240, 144, COL_BG);
    if (ui.page != PG_PLAYER) {
        int k;
        for (k = 0; k < 4; k++) {
            row_t r = row(ui.page, ui.sub[ui.page], k);
            int32_t y = 8 + k * 34, w;
            if (!r.level && r.yx < 0)
                continue;
            t[0] = 'K', t[1] = (char)('1' + k), t[2] = 0;
            cv_text(16, y, &FONT_XS, t, tone(COL_FAINT));
            cv_text(40, y, &FONT_S, r.label, tone(COL_SOFT));
            row_text(&r, t);
            if (row_silent(ui.page, ui.sub[ui.page], k)) {   /* muted: said so, the level kept */
                char u[24] = "";
                text_cat(u, T(S_STUMM));
                text_cat(u, "  ");
                text_cat(u, t);
                cv_text(224 - text_w(&FONT_S, u), y, &FONT_S, u, tone(COL_WARM));
            } else
                cv_text(224 - text_w(&FONT_S, t), y, &FONT_S, t,
                        tone(r.yx >= 0 && set.yp.x[r.yx] == YX_AUTO ? COL_SOFT : COL_TEXT));
            cv_rect(40, y + 20, 184, 4, COL_LOW);
            if (r.yx >= 0) {
                const yx_info_t *x = &YX[r.yx];
                int v = set.yp.x[r.yx];
                if (v == YX_AUTO) {              /* AUTO: a dotted line, the start decides */
                    for (w = 40; w < 224; w += 8)
                        cv_rect(w, y + 21, 3, 2, tone(COL_FAINT));
                    continue;
                }
                w = x->hi > x->lo ? 184 * (v - x->lo) / (x->hi - x->lo) : 184;
            } else
                w = 184 * *r.level / r.max;
            cv_rect(40, y + 20, w, 4, tone(COL_ACCENT));
        }
    } else {
        const char *stage = ui.st.state == YS_IDLE ? T(S_BEREIT) : ui.st.state == YS_PAUSED ? T(S_PAUSE)
                                                                     : stage_name(ui.st.stage_id);
        uint32_t r = ring_radius();
        centred(2, &FONT_L, program_shown(), tone(COL_TEXT));
        if (ui.st.crossing && ui.st.state == YS_RUNNING) {   /* a crossfade: "174 > 396" */
            char a[8];
            t[0] = 0;
            hz_text(ui.st.carrier, 0, a);
            text_cat(t, a);
            text_cat(t, " > ");
            hz_text(ui.st.carrier_to, 0, a);
            text_cat(t, a);
            stage = t;
        }
        centred(38, &FONT_S, stage, tone(ui.st.state == YS_DONE ? COL_WARM : COL_ACCENT));
        disc(120, 100, (int32_t)r, tone(ui.st.state == YS_RUNNING ? COL_ACCENT : COL_FAINT));
        disc(120, 100, (int32_t)r - 3, COL_BG);
        hz_text(carrier_shown(), 0, t);
        centred(88, &FONT_M, t, tone(COL_TEXT));
        centred(112, &FONT_T, "HZ", tone(COL_SOFT));
    }
    cv_blit(0, 34);
}

/* HOME's knobs, at the foot: K1 KLANG and K2 GARTEN (volumes), K3 the rate and K4 the carrier in MANUELL ("-"
 * elsewhere: those knobs do nothing then) */
static void knob_row(int32_t y)
{
    const char *const LBL[4] = {T(S_KLANG), T(S_GARTEN), T(S_WELLE), T(S_TON)};
    int k, manual = set.yp.program == YP_MANUAL;
    for (k = 0; k < 4; k++) {
        char v[12];
        int32_t x = k * 60, live = k < 2 || manual;
        if (k == 0)
            uint_text(set.yp.klang, v);
        else if (k == 1)
            uint_text(set.yp.garten, v);
        else if (!manual)
            v[0] = '-', v[1] = 0;
        else if (k == 2)
            hz_text((float)set.yp.beat_d * 0.1f, 1, v);
        else
            uint_text(YP_CARRIER[set.yp.carrier], v);
        if (k)
            cv_rect(x, y + 2, 1, 11, COL_LOW);
        cv_text(x + 5, y + 2, &FONT_T, LBL[k], tone(COL_FAINT));
        cv_text(x + 56 - text_w(&FONT_XS, v), y, &FONT_XS, v, tone(live ? COL_SOFT : COL_FAINT));
    }
}

static void draw_foot(void)
{
    char t[32], a[12];
    cv_begin(240, 60, COL_BG);
    if (ui.page != PG_PLAYER) {
        if (page_subs(ui.page) > 1) {        /* where we are among the part pages */
            int i;
            int n = page_subs(ui.page), x0 = 120 - (n * 10 - 4) / 2;
            for (i = 0; i < n; i++)
                cv_rect(x0 + i * 10, 6, 6, 6, i == ui.sub[ui.page] ? tone(COL_ACCENT) : COL_LOW);
            centred(22, &FONT_XS, T(S_HINT_SUB), tone(COL_FAINT));
            if (ui.page == PG_SETUP && ui.sub[PG_SETUP] == 0) {   /* SYSTEM: the audio's load, for a check */
                char c[32] = "CPU ", n[12];
                uint_text(plat_cpu_pct(), n);
                text_cat(c, n);
                text_cat(c, " %   XRUN ");
                uint_text(plat_xruns(), n);
                text_cat(c, n);
                centred(40, &FONT_XS, c, tone(COL_SOFT));
            }
        } else
            centred(22, &FONT_XS, T(S_HINT_BACK), tone(COL_FAINT));
    } else if (ui.st.state == YS_IDLE) {
        float beat = set.yp.program == YP_MANUAL ? (float)set.yp.beat_d * 0.1f : 0.0f;
        t[0] = 0;
        if (beat > 0.0f) {
            text_cat(t, yp_band_name(yp_band(beat)));
            text_cat(t, "  ");
            hz_text(beat, 1, a);
            text_cat(t, a);
            text_cat(t, " HZ");
        } else
            text_cat(t, set.yp.program == YP_REISE ? "174 > 396 > 432 > 528" : "THETA > DELTA");
        centred(0, &FONT_S, t, tone(COL_SOFT));
        minutes_text(t);
        centred(16, &FONT_M, t, tone(COL_TEXT));
        knob_row(44);
    } else {
        t[0] = 0;
        text_cat(t, yp_band_name(ui.st.band));
        text_cat(t, "  ");
        hz_text(ui.st.beat, 1, a);
        text_cat(t, a);
        text_cat(t, " HZ");
        centred(0, &FONT_S, t, tone(COL_SOFT));
        time_text(ui.st.remaining_s || ui.st.state == YS_DONE ? ui.st.remaining_s : ui.st.elapsed_s, t);
        centred(16, &FONT_M, t, tone(COL_TEXT));
        if (ui.st.progress) {
            cv_rect(20, 41, 200, 2, COL_LOW);
            cv_rect(20, 41, 200 * ui.st.progress / 1000, 2, tone(COL_ACCENT));
        }
        knob_row(44);
    }
    cv_blit(0, 180);
}

/* ---- lights: the page's button, PLAY green while it plays, SAVE while changed; one key walks with the breath -- */
static void lights(void)
{
    static const uint8_t PAGE_BTN[NPAGE] = {B_HOME, B_FX, B_ENV, B_GLO, B_SEL};
    uint32_t btn = 1u << PAGE_BTN[ui.page], keys = 0, dimkeys = 0, i, m = yp_muted();
    for (i = 0; i < 16u; i++)                /* the white keys: lit while held */
        keys |= ui.held & 1u << WK[i];
    for (i = 0; i < YM_N; i++) {             /* the small keys: lit while their part sounds (its layer and all
                                              * unmuted too); a muted one goes dark */
        int layer = i == YM_ALL ? -1 : i >= YM_GARTEN ? YM_GARTEN : YM_KLANG;
        int on = !(m >> i & 1u) && !(m >> YM_ALL & 1u) && (layer < 0 || !(m >> layer & 1u));
        if (on)
            keys |= 1u << BK[i];
    }
    if (ui.st.state == YS_RUNNING)
        btn |= LED_PLAY_GREEN;
    if (ui.dirty)
        btn |= 1u << B_SAVE;
    if (beat_lfo_lit())                      /* the metronome on the LFO button */
        btn |= 1u << B_LFO;
    if (ui.st.waking && ui.st.state == YS_RUNNING)
        btn |= 1u << B_REC;
    if (set.led_guide && ui.st.state == YS_RUNNING) {
        uint32_t b = yoga_dsp_breath(), s = b < 32768u ? b : 65535u - b, k = s * 16u / 32768u;
        dimkeys = 1u << WK[k > 15u ? 15u : k];
    }
    plat_lights(btn, keys, 0, dimkeys, 1);
}

/* ---- input -------------------------------------------------------------------------------------------------- */
static void changed(void)
{
    ui.dirty = 1;
    ui.redraw_head = ui.redraw_mid = ui.redraw_foot = 1;
}
static void step_u8(uint8_t *v, int32_t d, int lo, int hi)
{
    int32_t n = clamp((int32_t)*v + d, lo, hi);
    if (n != *v) {
        *v = (uint8_t)n;
        changed();
    }
}
static void start(void)
{
    uint32_t seed = mix(ui.entropy, plat_ms()) ^ plat_master();
    yp_start(&set.yp, seed, plat_ms());
    ui.page = PG_PLAYER;
    app_say(prog_name(set.yp.program), T(S_START));
}

/* a small key: its part muted or back (a fade), said; lit while it sounds */
static void mute_key(uint32_t key)
{
    uint32_t i, m = yp_muted();
    for (i = 0; i < 11u; i++)
        if (BK[i] == key)
            break;
    if (i >= YM_N)
        return;
    m ^= 1u << i;
    yp_mute(m);
    app_say(T(MUTE_NAME[i]), T(m >> i & 1u ? S_STUMM : S_AN));
    ui.redraw_head = ui.redraw_mid = 1;
}

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), now = plat_ms(), pressed = btn & ~ui.btn,
             released = ui.btn & ~btn, kdown = keys & ~ui.keys, k;
    int32_t e[NE];
    int any = pressed || released || kdown;
    for (k = 0; k < NE; k++)
        any |= (e[k] = plat_enc((int)k)) != 0;
    yp_status(&ui.st);
    if (any) {
        if (dimmed())
            ui.redraw_head = ui.redraw_mid = ui.redraw_foot = 1;
        ui.last_input = now;
    }
    ui.entropy = mix(ui.entropy, now ^ btn ^ keys << 7 ^ plat_master() << 3);

    /* pages */
    if (pressed >> B_FX & 1u)
        ui.page = ui.page == PG_KLANG ? PG_PLAYER : PG_KLANG;
    if (pressed >> B_ENV & 1u)
        ui.page = ui.page == PG_GARTEN ? PG_PLAYER : PG_GARTEN;
    if (pressed >> B_GLO & 1u)
        ui.page = ui.page == PG_SETUP ? PG_PLAYER : PG_SETUP;
    if (pressed >> B_SEL & 1u)
        ui.page = ui.page == PG_HAND ? PG_PLAYER : PG_HAND;
    if (pressed & (1u << B_FX | 1u << B_ENV | 1u << B_GLO | 1u << B_SEL)) {
        ui.msg_until = 0;                    /* a new page: the last page's message goes */
        ui.redraw_head = ui.redraw_mid = ui.redraw_foot = 1;
    }

    /* the encoders */
    if (e[EN_SELECT] && ui.page != PG_PLAYER && page_subs(ui.page) > 1) {
        ui.sub[ui.page] = (uint8_t)clamp(ui.sub[ui.page] + e[EN_SELECT], 0, page_subs(ui.page) - 1);
        ui.redraw_head = ui.redraw_mid = ui.redraw_foot = 1;
        app_say(page_name(ui.page), sub_name(ui.page, ui.sub[ui.page])[0] ? sub_name(ui.page, ui.sub[ui.page]) : T(S_UEBERSICHT));
    } else if (e[EN_SELECT] && ui.page == PG_PLAYER && idle()) {
        step_u8(&set.yp.program, e[EN_SELECT], 0, YP_NPROG - 1);
        app_say(T(S_PROGRAMM), prog_name(set.yp.program));
    }
    if (e[EN_PRESET] && idle()) {
        char t[16];
        if (set.yp.program == YP_MANUAL)
            step_u8(&set.yp.timer, e[EN_PRESET], 0, YP_NTIMER - 1);
        else
            step_u8(&set.yp.duration, e[EN_PRESET], 0, YP_NDURATION - 1);
        minutes_text(t);
        app_say(T(S_DAUER), t);
    }
    if (e[EN_ALGO]) {
        uint8_t m = e[EN_ALGO] > 0 ? YOGA_BIN : YOGA_ISO;
        if (m != set.yp.mode) {
            set.yp.mode = m;
            changed();
            app_say(m == YOGA_BIN ? "BIN" : "ISO", T(m == YOGA_BIN ? S_KOPFHOERER : S_LAUTSPRECHER));
        }
    }
    for (k = 0; k < 4u; k++) {
        int32_t d = e[EN_K1 + k];
        if (!d)
            continue;
        if (ui.page == PG_PLAYER) {
            /* HOME: K1 KLANG, K2 GARTEN (volumes); in MANUELL K3 the brainwave rate, K4 the carrier */
            static uint8_t *const VOL[2] = {&set.yp.klang, &set.yp.garten};
            const char *const VOL_NAME[2] = {T(S_KLANG), T(S_GARTEN)};
            char t[16], a[8];
            if (k < 2u) {
                step_u8(VOL[k], d, 0, 10);
                uint_text(*VOL[k], t);
                app_say(VOL_NAME[k], t);
            } else if (set.yp.program == YP_MANUAL && k == 2u) {
                step_u8(&set.yp.beat_d, d, 5, 70);
                hz_text((float)set.yp.beat_d * 0.1f, 1, a);
                t[0] = 0;
                text_cat(t, a);
                text_cat(t, " HZ");
                app_say(yp_band_name(yp_band((float)set.yp.beat_d * 0.1f)), t);
            } else if (set.yp.program == YP_MANUAL && k == 3u) {
                step_u8(&set.yp.carrier, d, 0, YP_NCARRIER - 1);
                uint_text(YP_CARRIER[set.yp.carrier], t);
                text_cat(t, " HZ");
                app_say(T(S_GRUNDTON), t);
            }
        } else {
            row_t r = row(ui.page, ui.sub[ui.page], (int)k);
            char t[12];
            if (r.yx >= 0) {
                if (detail_step(r.yx, d))
                    changed();
            } else if (r.level == &set.beat) {   /* AUS, then 20..120 in fives */
                int v = set.beat ? set.beat / 5 - 3 : 0;   /* 0 off, 1 = 20, .. 21 = 120 */
                v = clamp(v + d, 0, 21);
                if ((v ? (uint8_t)(15 + v * 5) : 0) != set.beat) {
                    set.beat = v ? (uint8_t)(15 + v * 5) : 0;
                    ui.beat_t0 = plat_ms();
                    changed();
                }
            } else if (r.level)
                step_u8(r.level, d, 0, r.max);
            else
                continue;
            row_text(&r, t);
            app_say(r.label, t);
        }
    }
    /* the white keys: the handpan, at any time. The analog rows' presses come with their velocity (lib/touch);
     * the top row's from plat_keys, with the velocity its contact gave */
    {
        touch_ev_t ev;
        while (touch_take(&ev)) {
            if (ev.type == TOUCH_PRESS) {
                ui.held |= 1u << ev.key;
                if (KEY_WHITE[ev.key] >= 0)
                    yp_hand(&set.yp, KEY_WHITE[ev.key], ev.vel);
                else
                    mute_key(ev.key);
            } else
                ui.held &= ~(1u << ev.key);
        }
        touch_update();
        for (k = 0; k < NKEYS; k++) {
            uint32_t bit = 1u << k;
            if (touch_sensed(k))
                continue;
            if ((kdown & bit) && KEY_WHITE[k] >= 0) {
                int v = touch_vel_contact(k);
                yp_hand(&set.yp, KEY_WHITE[k], v ? v : 90);
            } else if (kdown & bit)
                mute_key(k);
            ui.held = (ui.held & ~bit) | (keys & bit);
        }
    }

    /* the transport */
    if (pressed >> B_PLAY & 1u) {
        if (idle())
            start();
        else {
            yp_pause(now);
            app_say(T(ui.st.state == YS_PAUSED ? S_WEITER : S_PAUSE), "");
        }
    }
    if (pressed >> B_REC & 1u)
        ui.rec_t = now ? now : 1u;
    if (ui.rec_t && (btn >> B_REC & 1u) && now - ui.rec_t >= HOLD_WAKE) {
        ui.rec_t = 0;
        if (!idle() && !ui.st.waking) {
            yp_finish();
            app_say(T(S_AUFWACHEN), "");
        }
    }
    if (released >> B_REC & 1u)
        ui.rec_t = 0;
    if (pressed >> B_HOME & 1u)
        ui.home_t = now ? now : 1u;
    if (ui.home_t && (btn >> B_HOME & 1u) && now - ui.home_t >= HOLD_STOP) {
        ui.home_t = 0;
        if (ui.st.state != YS_IDLE) {
            yp_stop();
            app_say(T(S_STOPP), "");
        }
    }
    if ((released >> B_HOME & 1u) && ui.home_t) {     /* a tap: the player; after the end, rest */
        ui.home_t = 0;
        if (ui.page != PG_PLAYER)
            ui.page = PG_PLAYER;
        else if (ui.st.state == YS_DONE)
            yp_stop();
        ui.redraw_head = ui.redraw_mid = ui.redraw_foot = 1;
    }
    if (pressed >> B_SAVE & 1u) {
        if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0) {
            ui.dirty = 0;
            app_say(T(S_GESPEICHERT), "");
        } else
            app_say(T(S_SPEICHERN), T(S_FEHLER));
    }
    ui.btn = btn;
    ui.keys = keys;
    yp_update(&set.yp, now);
    yp_status(&ui.st);
    lights();
}

void app_input_only(void) { input(); }

static void speak(void)
{
    char t[96], a[12];
    if (ann_on() && ann_where()) {
        t[0] = 0;
        text_cat(t, "YOGA. ");
        text_cat(t, program_shown());
        text_cat(t, ". ");
        if (idle()) {
            minutes_text(a);
            text_cat(t, a);
        } else {
            text_cat(t, stage_name(ui.st.stage_id));
            text_cat(t, ". ");
            time_text(ui.st.remaining_s, a);
            text_cat(t, a);
        }
        text_cat(t, set.yp.mode == YOGA_BIN ? ". BIN" : ". ISO");
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

void app_ui_init(void)
{
    touch_on(1);                             /* the keys' velocity: the handpan */
    lcd_fill(0, 0, 240, 240, COL_BG);
    ui.page = PG_PLAYER;
    ui.last_input = plat_ms();
    ui.redraw_head = ui.redraw_mid = ui.redraw_foot = 1;
}

void app_frame(void)
{
    uint32_t now = plat_ms(), s;
    input();
    speak();
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.redraw_head = 1;
    }
    s = mix(mix(ui.page | ui.sub[ui.page] << 4 | (uint32_t)set.yp.x[YX_HINST] << 8 | (uint32_t)set.lang << 12, set.yp.mode),
            set.led_guide * 2u + (uint32_t)dimmed() + (yp_muted() << 4));
    if (s != ui.sig_head || ui.redraw_head) {
        ui.sig_head = s;
        ui.redraw_head = 0;
        draw_head();
    }
    draw_beat_dot();
    s = mix(mix(mix(ui.page | ui.sub[ui.page] << 4, ui.st.state), ring_radius()), (uint32_t)carrier_shown());
    s = mix(mix(s, ui.st.crossing ? (uint32_t)ui.st.carrier_to : 0u), (uint32_t)dimmed());
    s = mix(mix(s, ui.st.stage_id | (uint32_t)set.lang << 8), yp_muted());
    if (s != ui.sig_mid || ui.redraw_mid) {
        ui.sig_mid = s;
        ui.redraw_mid = 0;
        draw_mid();
    }
    s = mix(mix(mix(ui.page | ui.sub[ui.page] << 4, ui.st.state), ui.st.remaining_s ^ ui.st.elapsed_s << 16), (uint32_t)(ui.st.beat * 10.0f));
    s = mix(mix(s, ui.st.progress / 5u), (uint32_t)dimmed());
    if (ui.page == PG_SETUP)                 /* the load, refreshed twice a second */
        s = mix(s, plat_ms() / 500u);
    s = mix(s, set.yp.klang | set.yp.garten << 4 | (uint32_t)set.yp.beat_d << 8 | (uint32_t)set.yp.carrier << 16 |
               (uint32_t)set.yp.program << 20 | (uint32_t)set.lang << 24);
    if (s != ui.sig_foot || ui.redraw_foot) {
        ui.sig_foot = s;
        ui.redraw_foot = 0;
        draw_foot();
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "state"))
        *got = ui.st.state;
    else if (!strcmp(what, "program"))
        *got = set.yp.program;
    else if (!strcmp(what, "mode"))
        *got = set.yp.mode;
    else if (!strcmp(what, "page"))
        *got = ui.page;
    else if (!strcmp(what, "dirty"))
        *got = ui.dirty;
    else if (!strcmp(what, "crossing"))
        *got = ui.st.crossing;
    else if (!strcmp(what, "waking"))
        *got = ui.st.waking;
    else if (!strcmp(what, "hand"))          /* handpan notes ringing */
        *got = yoga_dsp_hand_voices();
    else if (!strcmp(what, "mute"))
        *got = (int)yp_muted();
    else if (!strcmp(what, "beat"))
        *got = set.beat;
    else if (!strcmp(what, "lfo"))
        *got = beat_lfo_lit();
    else if (!strcmp(what, "lang"))
        *got = set.lang;
    else if (!strcmp(what, "sub"))
        *got = ui.sub[ui.page];
    else
        return 0;
    return 1;
}
#endif
