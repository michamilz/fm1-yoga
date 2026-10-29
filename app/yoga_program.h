/* SPDX-License-Identifier: MPL-2.0 */
/* Guided relaxation programs and a manual mode, run from the main loop. Each start draws new random variations
 * from its seed: the yoga layer's timing and timbre, and the garden's recipe (amb.c). */
#pragma once
#include <stdint.h>

enum { YP_ERDEN, YP_LOSLASSEN, YP_HERZ, YP_WANDLUNG, YP_REISE, YP_MANUAL, YP_NPROG };
enum { YP_NCARRIER = 4 };              /* 174, 396, 432, 528 Hz */
enum { YP_NDURATION = 6 };             /* 5, 10, 15, 20, 30 minutes, endless */
enum { YP_NTIMER = 13 };               /* manual: endless, 5 .. 60 minutes */
enum { YS_IDLE, YS_RUNNING, YS_PAUSED, YS_DONE };
enum { YB_DELTA, YB_THETA, YB_ALPHA, YB_BETA };

/* the details behind each level: KLANG's drone, breath, bowls and room, GARTEN's pads, bells, bass and echo.
 * Each is a small number in lo..hi, or YX_AUTO (where it may be): then each start draws it, as the program would */
enum {
    YX_HARM, YX_CHORUS, YX_PULSE,              /* drone: overtones, the twin's beating, the pulse's depth (ISO) */
    YX_BRATE, YX_BCOLOR, YX_BSWELL,            /* breath: breaths a minute, the surf's colour, the drone's swell */
    YX_CHIMES, YX_BPITCH, YX_RING,             /* bowls: how often chimes ring, the bowls' register, their ring */
    YX_RSIZE, YX_RECHO,                        /* room: the hall's size, the layer's echo */
    YX_PCOLOR, YX_PBRIGHT, YX_PDRIFT,          /* pads: Rhodes or Voices, brightness, how often the chord moves */
    YX_GDENSITY, YX_GTONE, YX_GOCT,            /* bells: density, tone, octave */
    YX_BOCT, YX_BGLIDE, YX_BFIFTH,             /* bass: octave, glide, the fifth */
    YX_ETIME, YX_EREVERB, YX_EMOTION,          /* echo: its time, the garden's reverb, motion */
    YX_HINST,                                  /* the instrument on the keys: handpan, kalimba, tongue drum, bowls, Koshi */
    YX_HSCALE, YX_HDING, YX_HRING,             /* its scale, its ding (AUTO: the carrier), its ring */
    YX_HSPACE, YX_HECHO, YX_HBRIGHT, YX_HDYN,  /* and its hall, echo, brightness, how much the touch matters */
    YX_N
};
#define YX_AUTO (-128)
typedef struct {
    const char *label;
    int8_t lo, hi, def;
    uint8_t autok;                     /* 1: YX_AUTO is a value (below lo) */
    const char *const *names;          /* the values as words (lo first), or 0: numbers */
} yx_info_t;
extern const yx_info_t YX[YX_N];

/* the player's settings (saved: app.h) */
typedef struct {
    uint8_t program, duration, mode;   /* mode: YOGA_ISO / YOGA_BIN */
    uint8_t carrier;                   /* manual: 0..3 */
    uint8_t beat_d;                    /* manual: brainwave rate in 1/10 Hz, 5..70 */
    uint8_t timer;                     /* manual: 0 endless, else 5-minute steps */
    uint8_t drone, breath, bowls, space;   /* the yoga layer, 0..10 */
    uint8_t pad, bells, bass, echo;        /* the garden, 0..10 */
    uint8_t hand;                          /* the handpan's level, 0..10 */
    uint8_t klang, garten;                 /* the two layers' volumes (HOME: K1, K2), 0..10, 7 as made */
    int8_t x[YX_N];                        /* the details (YX_*) */
} yp_settings_t;

/* the stages, for the screen's words (app/lang.c): YST_C0..C3 a journey's carrier (its program's name) */
enum { YST_NONE, YST_ANKOMMEN, YST_VERTIEFEN, YST_RUHEN, YST_C0, YST_C1, YST_C2, YST_C3, YST_AUFWACHEN, YST_NAMASTE };
typedef struct {
    uint8_t state, band, waking, crossing;
    uint8_t prog, stage_id;            /* the program (YP_*) and its stage (YST_*) */
    const char *program, *stage;       /* the same, in German (tests, logs) */
    float carrier, carrier_to;         /* carrier_to: the next one while crossing */
    float beat;
    uint32_t elapsed_s, remaining_s;   /* remaining 0: endless */
    uint16_t progress;                 /* 0..1000; 0 when endless */
} yp_status_t;

extern const char *const YP_NAME[YP_NPROG];
extern const uint16_t YP_CARRIER[YP_NCARRIER];
extern const uint8_t YP_MINUTES[YP_NDURATION];

void yp_defaults(yp_settings_t *s);
void yx_text(int i, int v, char *b);   /* a detail's value as the screen shows it (12 bytes) */
int yp_valid(const yp_settings_t *s);
int yp_band(float beat);
const char *yp_band_name(int band);

void yp_init(void);
void yp_room(const yp_settings_t *s);   /* at power-on: the garden's hall and echo for the handpan (nothing played yet) */
void yp_start(const yp_settings_t *s, uint32_t seed, uint32_t now);
void yp_pause(uint32_t now);           /* toggles */
void yp_finish(void);                  /* end early: the wake-up now */
void yp_stop(void);                    /* fade out, then idle */
void yp_update(const yp_settings_t *s, uint32_t now);   /* every frame: the targets, live changes */
void yp_status(yp_status_t *st);
/* mutes (the small keys): live, never saved; each fades over 1.5 s. A group's mute keeps its parts' own */
enum {
    YM_KLANG, YM_DRONE, YM_BREATH, YM_BOWLS, YM_SPACE,     /* the yoga layer, then its parts */
    YM_GARTEN, YM_PADS, YM_BELLS, YM_BASS, YM_ECHO,       /* the garden, then its parts */
    YM_ALL,                                                /* both layers (the handpan plays on) */
    YM_N
};
void yp_mute(uint32_t mask);           /* bit YM_*: muted */
uint32_t yp_muted(void);

/* the instrument on the white keys: key 0 the ding (the scale's root), 1..8 its scale, 9..15 the scale an octave up;
 * on the handpan key 15 (the last) is a tak, a knock on the rim; on the Koshi each key lets a few rods ring, as the
 * wind does. vel 1..127. Playable at any time, idle too */
enum { YP_HAND_KEYS = 16, YP_HAND_TAK = 15, YP_NSCALE = 7 };
enum { YI_HANDPAN, YI_KALIMBA, YI_TONGUE, YI_BOWLS, YI_KOSHI, YI_N };
extern const char *const YI_NAME[YI_N];
void yp_hand(const yp_settings_t *s, int key, int vel);
float yp_hand_hz(const yp_settings_t *s, int key);
