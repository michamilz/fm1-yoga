/* SPDX-License-Identifier: MPL-2.0 */
/* The yoga firmware on fm1-base: the app's unity build (compiled into fm1-base's, after the platform: gfx.c and
 * announce.c are in scope). The engine (dsp/amb.c, with the yoga layer inside) is its own -O2 unit. */
#include "fastmath.h"
#include "yoga_program.c"
#include "lang.c"
#include "ui.c"

settings_t set;

/* the garden's MIDI out: off (P_MIDI 0), kept for the engine's calls */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { plat_midi_out(st >> 4 | st << 8 | d1 << 16 | d2 << 24); }

static void settings_defaults(void)
{
    set.magic = SET_MAGIC;
    yp_defaults(&set.yp);
    set.led_guide = 1;
    set.dim = 1;
    set.lang = 0;                        /* Deutsch */
    set.beat = 0;                        /* the metronome off */
    set.beat_show = 0;
}

/* YOG5's settings, as saved: the details one fewer (no YX_GINST), the rest after them; the same size */
typedef struct {
    uint32_t magic;
    uint8_t yp[sizeof(yp_settings_t) - 1];
    uint8_t led_guide, dim, lang, beat, beat_show, rsv[3];
} settings5_t;
_Static_assert(sizeof(settings5_t) == sizeof(settings_t), "YOG5's settings load as YOG6's");
_Static_assert(YX_GINST == YX_N - 1, "the bells' instrument is the last detail");

static void settings_from5(void)
{
    settings5_t o;
    memcpy(&o, &set, sizeof o);
    memcpy(&set.yp, o.yp, sizeof o.yp);
    set.yp.x[YX_GINST] = 0;              /* the bells, as before */
    set.led_guide = o.led_guide;
    set.dim = o.dim;
    set.lang = o.lang;
    set.beat = o.beat;
    set.beat_show = o.beat_show;
    set.rsv[0] = set.rsv[1] = 0;
    set.magic = SET_MAGIC;
}

void app_init(void)
{
    int got;
    en_init();
    yoga_dsp_init();
    yp_init();
    got = plat_store_load(OBJ_SETTINGS, &set, sizeof set);
    if (got == (int)sizeof set && set.magic == SET_MAGIC5)
        settings_from5();
    if (got != (int)sizeof set || set.magic != SET_MAGIC ||
        !yp_valid(&set.yp) || set.led_guide > 1u || set.dim > 2u || set.lang > 2u ||
        (set.beat && (set.beat < 20u || set.beat > 120u)) || set.beat_show > 2u)
        settings_defaults();
    if (plat_store_load(OBJ_SEQ, &sq, sizeof sq) != (int)sizeof sq || !seq_valid())
        seq_defaults();
    yp_room(&set.yp);                    /* a hall for the handpan, before any program */
}
/* MASTER full keeps some headroom below the engine's soft clip */
void app_render(int32_t *lr, uint32_t n, uint32_t gain_q12) { en_render(lr, n, gain_q12 * 3u / 4u); }
void app_overload(void) { yoga_dsp_overload(); }
void app_panic(void)
{
    yp_stop();
    en_play(0);
    en_panic();
}
