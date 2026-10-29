/* SPDX-License-Identifier: MPL-2.0 */
/* The yoga layer: a drone on a solfeggio carrier, a brainwave rate as an isochronic pulse (ISO, for speakers) or a
 * binaural offset (BIN, for headphones), breathing noise, struck bowls and chimes. It plays into AMB-1's mix
 * (amb.c: en_layer), so the garden's tape echo and plate carry it too. The main loop publishes targets; the render
 * glides towards them, so nothing jumps or clicks. Two drones: a change of carrier is a slow crossfade between
 * them, never a glide or a dip. Single precision only, no libm (fastmath.h). */
#pragma once
#include <stdint.h>

#define YOGA_RATE 44100.0f
enum { YOGA_ISO, YOGA_BIN };
enum { YOGA_BOWL, YOGA_CHIME };

/* frequencies in Hz, levels 0..1 */
typedef struct {
    float carrier[2];         /* the two drones' fundamentals */
    float blend;              /* 0: drone 0 alone .. 1: drone 1 alone (equal power between) */
    float beat;               /* brainwave rate: the pulse (ISO) or the right ear's offset (BIN) */
    float detune;             /* the chorus twin's offset */
    float breath;             /* the breathing LFO */
    float level;              /* drone and noise: fades, pause */
    float bell_level;         /* bowls and chimes: pause, stop, the final fade */
    float drone, octave, fifth, noise;
    float twin;               /* the chorus twin's level (its beating against the drone) */
    float swell;              /* how much the drone swells with the breath, 0..0.3 */
    float noise_tone;         /* the surf: 0 dark .. 1 bright */
    float echo;               /* how much of the layer goes to the tape echo */
    float hand_level, hand_space, hand_echo;   /* the handpan: level, plate, tape echo (it plays at any time) */
    float iso_depth;          /* the pulse's depth (ISO) */
    float space;              /* how much of the layer goes to the plate */
    float garden;             /* the garden's level (amb.c's pad, bells and bass), 0..1 */
    uint32_t mode;            /* YOGA_ISO or YOGA_BIN */
} yoga_target_t;

void yoga_dsp_init(void);
void yoga_dsp_set(const yoga_target_t *t);                 /* main loop */
/* main loop: a bowl or a chime; pan 0 left .. 1 right; ring scales how long it rings (1: as made). 0, or -1 when
 * the queue is full */
int yoga_dsp_strike(int kind, float hz, float level, float pan, float ring);
void yoga_dsp_silence(void);                               /* damp the bells within ~60 ms (stop, panic) */
/* main loop: a handpan struck. vel 0..1 (already shaped), pan 0..1, ring scales its ring (1: as made), bright 0..1
 * its overtones; kind: a tone field, the ding (the deep centre note: rounder, longer) or a tak (a knock on the rim:
 * short metal and the hand, hz its pitch). 0, or -1 when the queue is full */
enum { YOGA_HAND_NOTE, YOGA_HAND_DING, YOGA_HAND_TAK,       /* the handpan */
       YOGA_HAND_KALIMBA, YOGA_HAND_TONGUE, YOGA_HAND_BOWL, YOGA_HAND_CHIME, YOGA_HAND_KINDS };
int yoga_dsp_hand(float hz, float vel, float pan, float ring, float bright, int kind);
void yoga_dsp_hand_silence(void);                          /* damp the handpan (panic) */
int yoga_dsp_hand_voices(void);                            /* notes ringing (tests, the screen) */
uint32_t yoga_dsp_breath(void);                            /* the breathing LFO's phase, 0..65535 (0: exhaled) */

/* render, from amb.c once a chunk (n <= 256 frames): first the garden's gain for the chunk (smoothed; amb.c scales
 * its own voices by it), then the layer added to the mix and to the plate's and the echo's sends */
float yoga_dsp_garden(uint32_t n);
void yoga_dsp_layer(float *ml, float *mr, float *rsend, float *esend, uint32_t n);
