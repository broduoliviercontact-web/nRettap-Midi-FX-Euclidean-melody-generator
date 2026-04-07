/*
 * nrettap_engine.h
 * Portable Euclidean melody engine for the nRettap Schwung module.
 *
 * No Schwung or Move headers are used here.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define NRETTAP_MAX_STEPS 16

typedef enum {
    NRETTAP_SCALE_MAJOR = 0,
    NRETTAP_SCALE_MINOR,
    NRETTAP_SCALE_DORIAN,
    NRETTAP_SCALE_MIXOLYDIAN,
    NRETTAP_SCALE_PHRYGIAN,
    NRETTAP_SCALE_LYDIAN,
    NRETTAP_SCALE_HARMONIC_MINOR,
    NRETTAP_SCALE_BLUES,
    NRETTAP_SCALE_PENTATONIC,
    NRETTAP_SCALE_CHROMATIC,
    NRETTAP_NUM_SCALES
} NRettapScale;

typedef enum {
    NRETTAP_PHRASE_UP = 0,
    NRETTAP_PHRASE_DOWN,
    NRETTAP_PHRASE_PENDULUM,
    NRETTAP_PHRASE_RANDOM,
    NRETTAP_PHRASE_THIRDS,
    NRETTAP_NUM_PHRASES
} NRettapPhrase;

typedef struct {
    uint8_t requested_fills;
    uint8_t fills;
    uint8_t steps;
    int8_t rotation;
    uint8_t chance;
    uint8_t phrase;
    uint8_t root;
    uint8_t scale;
    uint8_t span;
    uint16_t seed;

    uint8_t step;
    uint8_t cycle;
    uint16_t trigger_count;
    uint8_t cursor;
    uint8_t direction_up;

    bool pattern[NRETTAP_MAX_STEPS];

    bool trigger;
    uint8_t note;
    uint8_t velocity;
} NRettapEngine;

void nrettap_init(NRettapEngine *engine);
void nrettap_reset(NRettapEngine *engine);

void nrettap_set_fills(NRettapEngine *engine, uint8_t value);
void nrettap_set_steps(NRettapEngine *engine, uint8_t value);
void nrettap_set_rotation(NRettapEngine *engine, int value);
void nrettap_set_chance(NRettapEngine *engine, uint8_t value);
void nrettap_set_phrase(NRettapEngine *engine, uint8_t value);
void nrettap_set_root(NRettapEngine *engine, uint8_t value);
void nrettap_set_scale(NRettapEngine *engine, uint8_t value);
void nrettap_set_span(NRettapEngine *engine, uint8_t value);
void nrettap_set_seed(NRettapEngine *engine, uint16_t value);

void nrettap_tick(NRettapEngine *engine);

bool nrettap_get_trigger(const NRettapEngine *engine);
uint8_t nrettap_get_note(const NRettapEngine *engine);
uint8_t nrettap_get_velocity(const NRettapEngine *engine);
