#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/host/midi_fx_api_v1.h"
#include "../src/host/plugin_api_v1.h"

extern midi_fx_api_v1_t *move_midi_fx_init(const host_api_v1_t *host);

static float g_bpm = 120.0f;
static int g_clock_status = MOVE_CLOCK_STATUS_RUNNING;

static float fake_get_bpm(void)
{
    return g_bpm;
}

static int fake_get_clock_status(void)
{
    return g_clock_status;
}

static void fail(const char *message)
{
    fprintf(stderr, "FAIL: %s\n", message);
    exit(1);
}

static void assert_wrapper_scale(midi_fx_api_v1_t *api,
                                 const uint8_t start[1],
                                 const char *scale_name,
                                 const int *allowed,
                                 int allowed_count)
{
    uint8_t out[16][3];
    int lens[16];
    void *notes = api->create_instance(NULL, NULL);
    int checked_notes = 0;

    if (!notes) fail("failed to allocate note-mode instance");

    api->set_param(notes, "fills", "8");
    api->set_param(notes, "steps", "8");
    api->set_param(notes, "chance", "1.0");
    api->set_param(notes, "scale", scale_name);
    api->set_param(notes, "root", "C");
    g_clock_status = MOVE_CLOCK_STATUS_RUNNING;
    g_bpm = 120.0f;
    api->process_midi(notes, start, 1, out, lens, 16);

    for (int i = 0; i < 256; i++) {
        int count = api->tick(notes, 128, 44100, out, lens, 16);
        for (int j = 0; j < count; j++) {
            int pitch_class;
            int allowed_note = 0;

            if ((out[j][0] & 0xF0) != 0x90 || out[j][2] == 0) continue;

            pitch_class = out[j][1] % 12;
            for (int k = 0; k < allowed_count; k++) {
                if (pitch_class == allowed[k]) {
                    allowed_note = 1;
                    break;
                }
            }
            if (!allowed_note) fail("wrapper emitted a pitch outside the selected scale");
            checked_notes++;
        }
        if (checked_notes >= 4) break;
    }

    if (checked_notes == 0) fail("wrapper produced no note-ons for scale test");
    api->destroy_instance(notes);
}

static int blocks_until_second_note_on(midi_fx_api_v1_t *api, void *instance)
{
    uint8_t out[16][3];
    int lens[16];
    const uint8_t start[1] = {0xFA};
    int note_on_count = 0;

    api->process_midi(instance, start, 1, out, lens, 16);

    for (int i = 0; i < 512; i++) {
        int count = api->tick(instance, 128, 44100, out, lens, 16);
        for (int j = 0; j < count; j++) {
            if ((out[j][0] & 0xF0) == 0x90 && out[j][2] > 0) {
                note_on_count++;
                if (note_on_count >= 2) return i;
            }
        }
    }

    return -1;
}

int main(void)
{
    const host_api_v1_t host = {
        .get_bpm = fake_get_bpm,
        .get_clock_status = fake_get_clock_status
    };
    midi_fx_api_v1_t *api = move_midi_fx_init(&host);
    uint8_t out[16][3];
    int lens[16];
    const uint8_t start[1] = {0xFA};

    if (!api) fail("move_midi_fx_init returned NULL");

    void *instance = api->create_instance(NULL, NULL);
    if (!instance) fail("create_instance returned NULL");

    api->set_param(instance, "fills", "8");
    api->set_param(instance, "steps", "8");
    api->set_param(instance, "chance", "1.0");
    g_clock_status = MOVE_CLOCK_STATUS_RUNNING;
    g_bpm = 120.0f;
    memset(out, 0, sizeof(out));
    memset(lens, 0, sizeof(lens));
    api->process_midi(instance, start, 1, out, lens, 16);

    {
        int saw_on = 0;
        int saw_later_off = 0;

        for (int i = 0; i < 512; i++) {
            int count = api->tick(instance, 128, 44100, out, lens, 16);
            for (int j = 0; j < count; j++) {
                if ((out[j][0] & 0xF0) == 0x90 && out[j][2] > 0) saw_on = 1;
                if (saw_on && (out[j][0] & 0xF0) == 0x80) {
                    saw_later_off = 1;
                    break;
                }
            }
            if (saw_later_off) break;
        }

        if (!saw_on) fail("wrapper never emitted a note-on");
        if (!saw_later_off) fail("wrapper never emitted a later note-off");
    }

    {
        void *fast = api->create_instance(NULL, NULL);
        void *slow = api->create_instance(NULL, NULL);
        if (!fast || !slow) fail("failed to allocate BPM comparison instances");

        api->set_param(fast, "fills", "8");
        api->set_param(fast, "steps", "8");
        api->set_param(fast, "chance", "1.0");
        api->set_param(slow, "fills", "8");
        api->set_param(slow, "steps", "8");
        api->set_param(slow, "chance", "1.0");

        g_clock_status = MOVE_CLOCK_STATUS_RUNNING;
        g_bpm = 240.0f;
        {
            int fast_blocks = blocks_until_second_note_on(api, fast);
            g_bpm = 60.0f;
            {
                int slow_blocks = blocks_until_second_note_on(api, slow);
                if (fast_blocks < 0 || slow_blocks < 0) fail("BPM test produced no second note-on");
                if (fast_blocks >= slow_blocks) fail("move BPM did not affect timing");
            }
        }

        api->destroy_instance(fast);
        api->destroy_instance(slow);
    }

    {
        void *stopped = api->create_instance(NULL, NULL);
        if (!stopped) fail("failed to allocate stopped instance");

        api->set_param(stopped, "fills", "8");
        api->set_param(stopped, "steps", "8");
        api->set_param(stopped, "chance", "1.0");
        g_clock_status = MOVE_CLOCK_STATUS_STOPPED;
        memset(out, 0, sizeof(out));
        memset(lens, 0, sizeof(lens));

        for (int i = 0; i < 64; i++) {
            int count = api->tick(stopped, 128, 44100, out, lens, 16);
            if (count > 0) fail("wrapper emitted MIDI while transport was stopped");
        }

        g_clock_status = MOVE_CLOCK_STATUS_UNAVAILABLE;
        for (int i = 0; i < 16; i++) {
            int count = api->tick(stopped, 128, 44100, out, lens, 16);
            if (count > 0) fail("wrapper emitted MIDI while clock status was unavailable");
        }

        api->destroy_instance(stopped);
    }

    {
        static const int major[] = {0, 2, 4, 5, 7, 9, 11};
        static const int phrygian[] = {0, 1, 3, 5, 7, 8, 10};
        static const int lydian[] = {0, 2, 4, 6, 7, 9, 11};
        static const int harmonic_minor[] = {0, 2, 3, 5, 7, 8, 11};
        static const int blues[] = {0, 3, 5, 6, 7, 10};

        assert_wrapper_scale(api, start, "major", major, 7);
        assert_wrapper_scale(api, start, "phrygian", phrygian, 7);
        assert_wrapper_scale(api, start, "lydian", lydian, 7);
        assert_wrapper_scale(api, start, "harmonic_minor", harmonic_minor, 7);
        assert_wrapper_scale(api, start, "blues", blues, 6);
    }

    {
        void *reset = api->create_instance(NULL, NULL);
        if (!reset) fail("failed to allocate reset test instance");

        api->set_param(reset, "fills", "8");
        api->set_param(reset, "steps", "8");
        api->set_param(reset, "chance", "1.0");
        api->process_midi(reset, start, 1, out, lens, 16);

        {
            int saw_on = 0;
            for (int i = 0; i < 256; i++) {
                int count = api->tick(reset, 128, 44100, out, lens, 16);
                for (int j = 0; j < count; j++) {
                    if ((out[j][0] & 0xF0) == 0x90 && out[j][2] > 0) {
                        saw_on = 1;
                        break;
                    }
                }
                if (saw_on) break;
            }
            if (!saw_on) fail("reset test did not observe an initial note-on");
        }

        api->set_param(reset, "seed", "77");
        {
            int count = api->tick(reset, 128, 44100, out, lens, 16);
            int saw_off = 0;
            for (int j = 0; j < count; j++) {
                if ((out[j][0] & 0xF0) == 0x80) saw_off = 1;
            }
            if (!saw_off) fail("structural param change did not flush the active note");
        }

        api->destroy_instance(reset);
    }

    {
        void *passthrough = api->create_instance(NULL, NULL);
        const uint8_t in_msg[3] = {0x90, 60, 100};
        if (!passthrough) fail("failed to allocate passthrough instance");

        memset(out, 0, sizeof(out));
        memset(lens, 0, sizeof(lens));
        {
            int count = api->process_midi(passthrough, in_msg, 3, out, lens, 16);
            if (count != 1) fail("wrapper did not pass through incoming MIDI");
            if (lens[0] != 3 || out[0][0] != 0x90 || out[0][1] != 60 || out[0][2] != 100) {
                fail("wrapper altered passthrough MIDI unexpectedly");
            }
        }

        api->destroy_instance(passthrough);
    }

    {
        void *params = api->create_instance(NULL, NULL);
        char buf[32];
        if (!params) fail("failed to allocate param test instance");

        api->set_param(params, "fills", "16");
        api->set_param(params, "steps", "8");
        api->set_param(params, "phrase", "random");
        api->set_param(params, "rate", "1/32");
        api->set_param(params, "chance", "0.5");

        if (api->get_param(params, "fills", buf, sizeof(buf)) < 0 || strcmp(buf, "16") != 0) {
            fail("wrapper did not preserve requested fills above current steps");
        }
        if (api->get_param(params, "steps", buf, sizeof(buf)) < 0 || strcmp(buf, "8") != 0) {
            fail("wrapper did not expose steps correctly");
        }
        if (api->get_param(params, "phrase", buf, sizeof(buf)) < 0 || strcmp(buf, "random") != 0) {
            fail("wrapper did not expose phrase correctly");
        }
        if (api->get_param(params, "rate", buf, sizeof(buf)) < 0 || strcmp(buf, "1/32") != 0) {
            fail("wrapper did not expose rate correctly");
        }
        if (api->get_param(params, "chance", buf, sizeof(buf)) < 0) {
            fail("wrapper did not expose chance");
        }
        if (atof(buf) < 0.49f || atof(buf) > 0.51f) {
            fail("wrapper did not retain chance correctly");
        }
        g_clock_status = MOVE_CLOCK_STATUS_UNAVAILABLE;
        if (api->get_param(params, "sync_warn", buf, sizeof(buf)) < 0 ||
            strcmp(buf, "Enable MIDI Clock Out") != 0) {
            fail("wrapper did not expose sync_warn for unavailable clock");
        }
        g_clock_status = MOVE_CLOCK_STATUS_RUNNING;
        if (api->get_param(params, "sync_warn", buf, sizeof(buf)) < 0 || strcmp(buf, "") != 0) {
            fail("wrapper did not clear sync_warn while running");
        }

        api->destroy_instance(params);
    }

    {
        const host_api_v1_t no_clock_host = {
            .get_bpm = fake_get_bpm,
            .get_clock_status = NULL
        };
        midi_fx_api_v1_t *no_clock_api = move_midi_fx_init(&no_clock_host);
        void *free_running = no_clock_api->create_instance(NULL, NULL);
        if (!free_running) fail("failed to allocate no-clock instance");

        no_clock_api->set_param(free_running, "fills", "8");
        no_clock_api->set_param(free_running, "steps", "8");
        no_clock_api->set_param(free_running, "chance", "1.0");

        {
            int saw_on = 0;
            for (int i = 0; i < 256; i++) {
                int count = no_clock_api->tick(free_running, 128, 44100, out, lens, 16);
                for (int j = 0; j < count; j++) {
                    if ((out[j][0] & 0xF0) == 0x90 && out[j][2] > 0) {
                        saw_on = 1;
                        break;
                    }
                }
                if (saw_on) break;
            }
            if (!saw_on) fail("wrapper should generate without prior MIDI when clock status callback is absent");
        }

        no_clock_api->destroy_instance(free_running);
        api = move_midi_fx_init(&host);
    }

    api->destroy_instance(instance);

    puts("PASS: nrettap midi fx tests");
    return 0;
}
