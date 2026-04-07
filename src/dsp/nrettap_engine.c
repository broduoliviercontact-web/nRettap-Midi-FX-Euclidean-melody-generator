#include "nrettap_engine.h"

#include <string.h>

static const uint8_t k_scale_major[] = {0, 2, 4, 5, 7, 9, 11};
static const uint8_t k_scale_minor[] = {0, 2, 3, 5, 7, 8, 10};
static const uint8_t k_scale_dorian[] = {0, 2, 3, 5, 7, 9, 10};
static const uint8_t k_scale_mixolydian[] = {0, 2, 4, 5, 7, 9, 10};
static const uint8_t k_scale_phrygian[] = {0, 1, 3, 5, 7, 8, 10};
static const uint8_t k_scale_lydian[] = {0, 2, 4, 6, 7, 9, 11};
static const uint8_t k_scale_harmonic_minor[] = {0, 2, 3, 5, 7, 8, 11};
static const uint8_t k_scale_blues[] = {0, 3, 5, 6, 7, 10};
static const uint8_t k_scale_pentatonic[] = {0, 3, 5, 7, 10};
static const uint8_t k_scale_chromatic[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

static const uint8_t *k_scale_tables[NRETTAP_NUM_SCALES] = {
    k_scale_major,
    k_scale_minor,
    k_scale_dorian,
    k_scale_mixolydian,
    k_scale_phrygian,
    k_scale_lydian,
    k_scale_harmonic_minor,
    k_scale_blues,
    k_scale_pentatonic,
    k_scale_chromatic
};

static const uint8_t k_scale_lengths[NRETTAP_NUM_SCALES] = {7, 7, 7, 7, 7, 7, 7, 6, 5, 12};

static uint8_t clamp_midi(int value)
{
    if (value < 0) return 0;
    if (value > 127) return 127;
    return (uint8_t)value;
}

static uint32_t mix32(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

static uint8_t hash8(uint16_t seed, uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t value = (uint32_t)seed;
    value ^= a * 0x9e3779b9u;
    value ^= b * 0x85ebca6bu;
    value ^= c * 0xc2b2ae35u;
    return (uint8_t)(mix32(value) >> 24);
}

static void clear_playback_state(NRettapEngine *engine)
{
    engine->step = 0;
    engine->cycle = 0;
    engine->trigger_count = 0;
    engine->cursor = 0;
    engine->direction_up = 1;
    engine->trigger = false;
    engine->note = 48;
    engine->velocity = 0;
}

static int normalized_rotation(const NRettapEngine *engine)
{
    int steps = engine->steps > 0 ? (int)engine->steps : 1;
    int rotation = (int)engine->rotation % steps;
    if (rotation < 0) rotation += steps;
    return rotation;
}

static void rebuild_pattern(NRettapEngine *engine)
{
    memset(engine->pattern, 0, sizeof(engine->pattern));

    if (engine->steps == 0 || engine->fills == 0) return;

    for (int slot = 0; slot < engine->steps; slot++) {
        int fire = (((slot * engine->fills) % engine->steps) < engine->fills) ? 1 : 0;
        int rotated_slot = (slot + normalized_rotation(engine)) % engine->steps;
        engine->pattern[rotated_slot] = fire != 0;
    }
}

static uint8_t degree_count(const NRettapEngine *engine)
{
    uint8_t scale = engine->scale < NRETTAP_NUM_SCALES ? engine->scale : NRETTAP_SCALE_MINOR;
    uint8_t span = engine->span < 1 ? 1 : engine->span;
    return (uint8_t)(k_scale_lengths[scale] * span);
}

static uint8_t next_degree(NRettapEngine *engine, uint8_t slot)
{
    uint8_t count = degree_count(engine);
    uint8_t phrase = engine->phrase < NRETTAP_NUM_PHRASES ? engine->phrase : NRETTAP_PHRASE_PENDULUM;

    if (count == 0) return 0;

    if (phrase == NRETTAP_PHRASE_UP) {
        uint8_t degree = (uint8_t)(engine->cursor % count);
        engine->cursor = (uint8_t)((engine->cursor + 1u) % count);
        return degree;
    }

    if (phrase == NRETTAP_PHRASE_DOWN) {
        uint8_t degree = (uint8_t)((count - 1u) - (engine->cursor % count));
        engine->cursor = (uint8_t)((engine->cursor + 1u) % count);
        return degree;
    }

    if (phrase == NRETTAP_PHRASE_RANDOM) {
        return (uint8_t)(hash8(engine->seed, engine->cycle, slot, engine->trigger_count + 23u) % count);
    }

    if (phrase == NRETTAP_PHRASE_THIRDS) {
        uint8_t degree = (uint8_t)(engine->cursor % count);
        engine->cursor = (uint8_t)((engine->cursor + 2u) % count);
        return degree;
    }

    {
        uint8_t degree = engine->cursor;

        if (count <= 1u) return 0;

        if (engine->direction_up) {
            if ((uint8_t)(engine->cursor + 1u) >= count) {
                engine->direction_up = 0;
                engine->cursor = (uint8_t)(engine->cursor - 1u);
            } else {
                engine->cursor++;
            }
        } else {
            if (engine->cursor == 0u) {
                engine->direction_up = 1;
                engine->cursor = 1u;
            } else {
                engine->cursor--;
            }
        }

        return degree;
    }
}

static uint8_t quantize_degree(const NRettapEngine *engine, uint8_t degree)
{
    uint8_t scale = engine->scale < NRETTAP_NUM_SCALES ? engine->scale : NRETTAP_SCALE_MINOR;
    uint8_t root = engine->root % 12u;
    uint8_t scale_len = k_scale_lengths[scale];
    uint8_t octave = scale_len > 0 ? (uint8_t)(degree / scale_len) : 0;
    uint8_t degree_in_scale = scale_len > 0 ? (uint8_t)(degree % scale_len) : 0;
    int midi = 48 + (octave * 12) + root + k_scale_tables[scale][degree_in_scale];
    return clamp_midi(midi);
}

static int chance_allows(uint8_t chance, uint8_t roll)
{
    if (chance == 0u) return 0;
    if (chance == 255u) return 1;
    return roll < chance;
}

static uint8_t compute_velocity(const NRettapEngine *engine, uint8_t slot)
{
    uint8_t previous = engine->pattern[(slot + engine->steps - 1u) % engine->steps] ? 1u : 0u;
    uint8_t accent = previous == 0u ? 14u : 0u;
    uint8_t variation = (uint8_t)(hash8(engine->seed, engine->cycle, slot, 91u) % 22u);
    return clamp_midi(78 + accent + variation);
}

void nrettap_init(NRettapEngine *engine)
{
    memset(engine, 0, sizeof(*engine));
    engine->requested_fills = 5;
    engine->fills = 5;
    engine->steps = 8;
    engine->rotation = 0;
    engine->chance = 217;
    engine->phrase = NRETTAP_PHRASE_PENDULUM;
    engine->root = 0;
    engine->scale = NRETTAP_SCALE_MINOR;
    engine->span = 2;
    engine->seed = 1;
    clear_playback_state(engine);
    rebuild_pattern(engine);
}

void nrettap_reset(NRettapEngine *engine)
{
    clear_playback_state(engine);
    rebuild_pattern(engine);
}

void nrettap_set_fills(NRettapEngine *engine, uint8_t value)
{
    if (value > NRETTAP_MAX_STEPS) value = NRETTAP_MAX_STEPS;
    engine->requested_fills = value;
    engine->fills = value > engine->steps ? engine->steps : value;
    rebuild_pattern(engine);
}

void nrettap_set_steps(NRettapEngine *engine, uint8_t value)
{
    if (value < 1u) value = 1u;
    if (value > NRETTAP_MAX_STEPS) value = NRETTAP_MAX_STEPS;
    engine->steps = value;
    engine->fills = engine->requested_fills > engine->steps ? engine->steps : engine->requested_fills;
    if (engine->step >= engine->steps) engine->step = 0;
    rebuild_pattern(engine);
}

void nrettap_set_rotation(NRettapEngine *engine, int value)
{
    if (value < -(NRETTAP_MAX_STEPS - 1)) value = -(NRETTAP_MAX_STEPS - 1);
    if (value > (NRETTAP_MAX_STEPS - 1)) value = NRETTAP_MAX_STEPS - 1;
    engine->rotation = (int8_t)value;
    rebuild_pattern(engine);
}

void nrettap_set_chance(NRettapEngine *engine, uint8_t value)
{
    engine->chance = value;
}

void nrettap_set_phrase(NRettapEngine *engine, uint8_t value)
{
    engine->phrase = (value < NRETTAP_NUM_PHRASES) ? value : NRETTAP_PHRASE_PENDULUM;
    engine->cursor = 0;
    engine->direction_up = 1;
}

void nrettap_set_root(NRettapEngine *engine, uint8_t value)
{
    engine->root = value % 12u;
}

void nrettap_set_scale(NRettapEngine *engine, uint8_t value)
{
    engine->scale = (value < NRETTAP_NUM_SCALES) ? value : NRETTAP_SCALE_MINOR;
}

void nrettap_set_span(NRettapEngine *engine, uint8_t value)
{
    if (value < 1u) value = 1u;
    if (value > 4u) value = 4u;
    engine->span = value;
}

void nrettap_set_seed(NRettapEngine *engine, uint16_t value)
{
    engine->seed = value == 0u ? 1u : value;
}

void nrettap_tick(NRettapEngine *engine)
{
    uint8_t slot = engine->step;
    int fire = engine->pattern[slot] ? 1 : 0;

    engine->trigger = false;
    engine->velocity = 0;

    if (fire && chance_allows(engine->chance, hash8(engine->seed, engine->cycle, slot, 17u))) {
        uint8_t degree = next_degree(engine, slot);
        engine->trigger = true;
        engine->note = quantize_degree(engine, degree);
        engine->velocity = compute_velocity(engine, slot);
        engine->trigger_count++;
    }

    engine->step++;
    if (engine->step >= engine->steps) {
        engine->step = 0;
        engine->cycle++;
    }
}

bool nrettap_get_trigger(const NRettapEngine *engine)
{
    return engine->trigger;
}

uint8_t nrettap_get_note(const NRettapEngine *engine)
{
    return engine->note;
}

uint8_t nrettap_get_velocity(const NRettapEngine *engine)
{
    return engine->velocity;
}
