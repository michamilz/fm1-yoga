/* SPDX-License-Identifier: MPL-2.0 */
/* The yoga firmware: the app's half of the contract with fm1-base (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "amb.h"
#include "yoga_dsp.h"
#include "yoga_program.h"

#define APP_NAME "YOGA"
#define APP_SPLASH_RGB RGB(96, 196, 200)
#define APP_TAGLINE "ENDENTSPANNUNG"
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_TOUCH                        /* the keys' velocity: the handpan (lib/touch.h) */
/* the USB recording: a whole half more in hand. The renders (the garden, the yoga layer and an instrument) can take
 * more than the ~40 % of their time the plain ring allows; it would then run dry and the recording crackle (not the
 * FM-1's own output). +5.8 ms of latency in the recording, its 4 KB ring in the pool (bsp/chip/usb.c) */
#define FM1B_UAC_DEEP 1

/* the store: the settings and the panel calibration (the platform's), where the template keeps them */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u}

#define SET_MAGIC 0x35474F59u            /* "YOG5" (the details, the instruments, the layers' volumes) */
typedef struct {
    uint32_t magic;
    yp_settings_t yp;                    /* the player's choices and levels */
    uint8_t led_guide;                   /* a light walks the keys with the breath */
    uint8_t dim;                         /* the screen dims: 0 never, 1 after 30 s, 2 after 2 minutes */
    uint8_t lang;                        /* the screen's language: 0 Deutsch, 1 English, 2 Sanskrit (app/lang.c) */
    uint8_t beat;                        /* the silent metronome: 0 off, else beats a minute (20..120) */
    uint8_t beat_show;                   /* where it shows: 0 the screen, 1 the LFO button, 2 both */
    uint8_t rsv[3];
} settings_t;
extern settings_t set;

/* the contract: the platform calls these */
void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t gain_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);

/* the host simulator's checks (host/plat_host.c "expect"): state, program, mode, page, dirty, crossing, waking, hand,
 * mute, sub, lang, beat, lfo */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
