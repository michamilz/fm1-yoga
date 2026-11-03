/* SPDX-License-Identifier: MPL-2.0 */
/* The sequencer: four patterns played in by hand, on a grid of eighth notes, in any program. A pattern keeps what was
 * played, not how it sounded: the keys (so it follows the carrier, the scale and the instrument), the bowl and the
 * mutes. It repeats until the player names the next one, and changes at its own end. Run from yp_update, in the main
 * loop; part of yoga_program.c. */

seq_store_t sq;
enum { SEQ_LIT_MS = 200 };                   /* how long a key lights for a note the pattern plays */
enum { SEQ_FRESH = 0x80, SEQ_FADE = 8000 };  /* FRESH: rounded onto the next step, played live: not again this pass */
static struct {
    int8_t cur, next;                        /* the pattern (-1 none), the one after it (SEQ_NONE, SEQ_STOP) */
    uint8_t playing, rec, fading, full, changed;
    int16_t pos;                             /* the step played; -1: about to start */
    uint32_t acc;                            /* ms since that step */
    float gain;                              /* falls to nothing in the wake-up */
    uint8_t lit[YP_HAND_KEYS];               /* ms each key stays lit (at most SEQ_LIT_MS) */
} seq = {.cur = -1, .next = SEQ_NONE};

void seq_defaults(void)
{
    uint32_t i;
    sq.magic = SEQ_MAGIC;
    sq.tempo = 40;
    for (i = 0; i < 3u; i++)
        sq.rsv[i] = 0;
    for (i = 0; i < SEQ_N; i++)
        sq.pat[i].len = 32, sq.pat[i].n = 0;
}
int seq_valid(void)
{
    uint32_t i;
    if (sq.magic != SEQ_MAGIC || sq.tempo < 20u || sq.tempo > 120u)
        return 0;
    for (i = 0; i < SEQ_N; i++)
        if (sq.pat[i].len < 4u || sq.pat[i].len > SEQ_STEPS || sq.pat[i].n > SEQ_EVENTS)
            return 0;
    return 1;
}
static uint32_t seq_step_ms(void) { return 30000u / sq.tempo; }
/* it may sound: running, and not yet waking up (the wake-up leads to silence) */
static int seq_may_run(void) { return p.state == YS_RUNNING && !p.stopping && p.elapsed < p.wake_start; }
static void seq_begin(int i)
{
    seq.cur = (int8_t)i;
    seq.next = SEQ_NONE;
    seq.playing = 1;
    seq.fading = 0;
    seq.pos = -1;
    seq.acc = 0;
    seq.gain = 1.0f;
}
static void seq_rec_stop(void)
{
    uint32_t i;
    if (!seq.rec)
        return;
    seq.rec = seq.full = 0;
    for (i = 0; i < sq.pat[seq.cur].n; i++)
        sq.pat[seq.cur].ev[i].vel &= (uint8_t)~SEQ_FRESH;
}
static void seq_fire(const yp_settings_t *s)
{
    seq_pat_t *pt = &sq.pat[seq.cur];
    uint32_t i;
    for (i = 0; i < pt->n; i++) {
        seq_ev_t *e = &pt->ev[i];
        if (e->step != seq.pos)
            continue;
        if (e->vel & SEQ_FRESH)
            e->vel &= (uint8_t)~SEQ_FRESH;
        else if (e->what < YP_HAND_KEYS) {
            yp_hand_play(s, e->what, e->vel, seq.gain);
            seq.lit[e->what] = SEQ_LIT_MS;
        }
        else if (e->what == SEQ_BOWL)
            yp_strike_bowl(0.6f * seq.gain, s, p.carrier_now);
        else if (!seq.fading)                /* a mute: its own part only, the others as they are */
            ym.mask = e->vel ? ym.mask | 1u << (e->what - SEQ_MUTE) : ym.mask & ~(1u << (e->what - SEQ_MUTE));
    }
}
static void seq_advance(const yp_settings_t *s)
{
    if (++seq.pos >= sq.pat[seq.cur].len) {
        seq.pos = 0;
        if (seq.next != SEQ_NONE) {
            seq_rec_stop();
            if (seq.next == SEQ_STOP) {
                seq.playing = 0;
                seq.cur = -1;
                seq.next = SEQ_NONE;
                return;
            }
            seq.cur = seq.next;
            seq.next = SEQ_NONE;
        }
    }
    seq_fire(s);
}
/* the keys' lights fade out of the count in any state: a stop or a pause leaves none on */
static void seq_lit_tick(uint32_t dt)
{
    uint32_t i;
    for (i = 0; i < YP_HAND_KEYS; i++)
        seq.lit[i] = seq.lit[i] > dt ? (uint8_t)(seq.lit[i] - dt) : 0u;
}
static void seq_tick(const yp_settings_t *s, uint32_t dt)
{
    if (!seq.playing)
        return;
    if (p.elapsed >= p.wake_start && !seq.fading) {
        seq_rec_stop();
        seq.fading = 1;
        seq.next = SEQ_NONE;
    }
    if (seq.fading) {
        seq.gain -= (float)dt / (float)SEQ_FADE;
        if (seq.gain <= 0.0f) {
            seq.playing = seq.fading = 0;
            return;
        }
    }
    if (seq.pos < 0) {
        seq_advance(s);
        return;
    }
    seq.acc += dt;
    while (seq.playing && seq.acc >= seq_step_ms()) {
        seq.acc -= seq_step_ms();
        seq_advance(s);
    }
}
/* the program's own start and stop */
static void seq_program_start(void)
{
    if (seq.cur >= 0)
        seq_begin(seq.cur);
}
static void seq_program_stop(void)
{
    seq_rec_stop();
    seq.playing = seq.fading = 0;
    seq.next = SEQ_NONE;
}
/* what was just played, into the pattern: on the nearest step, once per step for each key, bowl and mute */
static void seq_capture(int what, int vel)
{
    seq_pat_t *pt;
    uint32_t i, stp;
    if (!seq.rec || !seq.playing || p.state != YS_RUNNING)
        return;
    pt = &sq.pat[seq.cur];
    stp = seq.pos < 0 ? 0u : (uint32_t)seq.pos;
    if (seq.pos < 0 || seq.acc * 2u >= seq_step_ms()) {
        vel |= SEQ_FRESH;
        if (seq.pos >= 0 && ++stp >= pt->len)
            stp = 0;
    }
    seq.changed = 1;
    for (i = 0; i < pt->n; i++)
        if (pt->ev[i].step == stp && pt->ev[i].what == what) {
            pt->ev[i].vel = (uint8_t)vel;
            return;
        }
    if (pt->n >= SEQ_EVENTS) {
        seq.full = 1;
        return;
    }
    pt->ev[pt->n].step = (uint8_t)stp;
    pt->ev[pt->n].what = (uint8_t)what;
    pt->ev[pt->n].vel = (uint8_t)vel;
    pt->n++;
}

void seq_key(int i)
{
    if (i < 0 || i >= SEQ_N)
        return;
    if (seq.playing && !seq.fading) {
        if (i == seq.cur)
            seq.next = seq.next == SEQ_STOP ? SEQ_NONE : SEQ_STOP;
        else
            seq.next = seq.next == i ? SEQ_NONE : (int8_t)i;
    } else if (seq_may_run())
        seq_begin(i);
    else
        seq.cur = seq.cur == i ? -1 : (int8_t)i;   /* for the next start */
}
int seq_record(void)
{
    if (seq.rec) {
        seq_rec_stop();
        return 0;
    }
    if (!seq_may_run())
        return -1;
    if (!seq.playing)
        seq_begin(seq.cur >= 0 ? seq.cur : 0);
    seq.rec = 1;
    return 1;
}
void seq_clear(int i)
{
    if (i < 0 || i >= SEQ_N)
        return;
    sq.pat[i].n = 0;
    seq.full = 0;
    seq.changed = 1;
    seq.next = SEQ_NONE;
}
void seq_status(seq_status_t *st)
{
    st->cur = seq.cur;
    st->next = seq.next;
    st->playing = seq.playing;
    st->rec = seq.rec;
    st->full = seq.full;
    st->pos = seq.pos < 0 ? 0 : seq.pos;
    st->keys = 0;
    for (int i = 0; i < YP_HAND_KEYS; i++)
        if (seq.lit[i])
            st->keys |= (uint16_t)(1u << i);
}
int seq_take_changed(void)
{
    int c = seq.changed;
    seq.changed = 0;
    return c;
}
/* the LFO button: a bowl, played and kept like a key; only while a program sounds (the bowls have no room before) */
int yp_bowl(const yp_settings_t *s)
{
    if (p.state != YS_RUNNING || p.stopping)
        return 0;
    seq_capture(SEQ_BOWL, 100);
    yp_strike_bowl(0.6f, s, p.carrier_now);
    return 1;
}
