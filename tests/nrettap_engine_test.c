#include <stdio.h>
#include <stdlib.h>

#include "../src/dsp/nrettap_engine.h"

static void fail(const char *message)
{
    fprintf(stderr, "FAIL: %s\n", message);
    exit(1);
}

static int count_hits(NRettapEngine *engine, int steps)
{
    int hits = 0;

    for (int i = 0; i < steps; i++) {
        nrettap_tick(engine);
        if (nrettap_get_trigger(engine)) hits++;
    }

    return hits;
}

static void assert_scale_membership(uint8_t scale, const int *allowed, int allowed_count, const char *label)
{
    NRettapEngine engine;
    nrettap_init(&engine);
    nrettap_set_steps(&engine, 8);
    nrettap_set_fills(&engine, 8);
    nrettap_set_chance(&engine, 255);
    nrettap_set_scale(&engine, scale);
    nrettap_set_root(&engine, 0);

    for (int i = 0; i < 16; i++) {
        int allowed_note = 0;
        int pitch_class;

        nrettap_tick(&engine);
        if (!nrettap_get_trigger(&engine)) continue;

        pitch_class = nrettap_get_note(&engine) % 12;
        for (int j = 0; j < allowed_count; j++) {
            if (pitch_class == allowed[j]) {
                allowed_note = 1;
                break;
            }
        }

        if (!allowed_note) fail(label);
    }
}

int main(void)
{
    NRettapEngine sparse;
    nrettap_init(&sparse);
    nrettap_set_steps(&sparse, 16);
    nrettap_set_fills(&sparse, 3);
    nrettap_set_chance(&sparse, 255);
    if (count_hits(&sparse, 16) != 3) fail("fills should control trigger count per cycle");

    NRettapEngine dense;
    nrettap_init(&dense);
    nrettap_set_steps(&dense, 16);
    nrettap_set_fills(&dense, 11);
    nrettap_set_chance(&dense, 255);
    if (count_hits(&dense, 16) != 11) fail("dense pattern should emit the requested number of hits");

    NRettapEngine muted;
    nrettap_init(&muted);
    nrettap_set_steps(&muted, 8);
    nrettap_set_fills(&muted, 8);
    nrettap_set_chance(&muted, 0);
    if (count_hits(&muted, 8) != 0) fail("chance=0 should mute all hits");

    NRettapEngine rotated;
    nrettap_init(&rotated);
    nrettap_set_steps(&rotated, 8);
    nrettap_set_fills(&rotated, 3);
    nrettap_set_rotation(&rotated, 1);
    nrettap_set_chance(&rotated, 255);
    nrettap_tick(&rotated);
    if (nrettap_get_trigger(&rotated)) fail("positive rotation should delay the first euclidean hit");
    nrettap_tick(&rotated);
    if (!nrettap_get_trigger(&rotated)) fail("rotation should move the first euclidean hit to the next step");

    {
        static const int major[] = {0, 2, 4, 5, 7, 9, 11};
        static const int phrygian[] = {0, 1, 3, 5, 7, 8, 10};
        static const int lydian[] = {0, 2, 4, 6, 7, 9, 11};
        static const int harmonic_minor[] = {0, 2, 3, 5, 7, 8, 11};
        static const int blues[] = {0, 3, 5, 6, 7, 10};

        assert_scale_membership(NRETTAP_SCALE_MAJOR, major, 7, "major scale quantization failed");
        assert_scale_membership(NRETTAP_SCALE_PHRYGIAN, phrygian, 7, "phrygian scale quantization failed");
        assert_scale_membership(NRETTAP_SCALE_LYDIAN, lydian, 7, "lydian scale quantization failed");
        assert_scale_membership(NRETTAP_SCALE_HARMONIC_MINOR, harmonic_minor, 7, "harmonic minor scale quantization failed");
        assert_scale_membership(NRETTAP_SCALE_BLUES, blues, 6, "blues scale quantization failed");
    }

    NRettapEngine recall;
    nrettap_init(&recall);
    nrettap_set_fills(&recall, 16);
    nrettap_set_steps(&recall, 16);
    if (recall.fills != 16) fail("fills should restore correctly after steps expands later");

    NRettapEngine seeded_a;
    NRettapEngine seeded_b;
    nrettap_init(&seeded_a);
    nrettap_init(&seeded_b);
    nrettap_set_steps(&seeded_a, 8);
    nrettap_set_steps(&seeded_b, 8);
    nrettap_set_fills(&seeded_a, 8);
    nrettap_set_fills(&seeded_b, 8);
    nrettap_set_chance(&seeded_a, 255);
    nrettap_set_chance(&seeded_b, 255);
    nrettap_set_phrase(&seeded_a, NRETTAP_PHRASE_RANDOM);
    nrettap_set_phrase(&seeded_b, NRETTAP_PHRASE_RANDOM);
    nrettap_set_seed(&seeded_a, 42);
    nrettap_set_seed(&seeded_b, 42);

    for (int i = 0; i < 16; i++) {
        nrettap_tick(&seeded_a);
        nrettap_tick(&seeded_b);
        if (nrettap_get_trigger(&seeded_a) != nrettap_get_trigger(&seeded_b)) {
            fail("same seed should produce the same trigger pattern");
        }
        if (nrettap_get_note(&seeded_a) != nrettap_get_note(&seeded_b)) {
            fail("same seed should produce the same random phrase");
        }
    }

    puts("PASS: nrettap engine tests");
    return 0;
}
