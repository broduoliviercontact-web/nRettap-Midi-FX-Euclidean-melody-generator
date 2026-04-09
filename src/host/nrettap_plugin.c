#include "midi_fx_api_v1.h"
#include "plugin_api_v1.h"
#include "../dsp/nrettap_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#define MIDI_NOTE_ON 0x90u
#define MIDI_NOTE_OFF 0x80u

#define DEFAULT_BPM 120.0f
#define DEFAULT_SAMPLE_RATE 44100

typedef enum {
    RATE_EIGHTH = 0,
    RATE_SIXTEENTH = 1,
    RATE_THIRTYSECOND = 2
} RateMode;

typedef struct {
    NRettapEngine engine;
    uint32_t sample_rate;
    uint32_t frames_until_step;
    uint32_t frames_until_note_off;
    uint8_t midi_clocks_until_step;
    uint8_t midi_clocks_until_note_off;
    uint8_t running;
    uint8_t clock_message_mode;
    uint8_t reset_pending;
    uint8_t rate_mode;
    uint8_t active_note_valid;
    uint8_t active_note;
    uint8_t active_channel;
    uint8_t debug_logged_create;
    uint8_t debug_logged_tick;
    uint8_t debug_logged_emit;
} NRettapInstance;

static const host_api_v1_t *g_host = NULL;

static void host_logf(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    int written;

    if (!g_host || !g_host->log || !fmt) return;

    va_start(ap, fmt);
    written = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (written < 0) return;

    buf[sizeof(buf) - 1] = '\0';
    g_host->log(buf);
}

static void file_logf(const char *fmt, ...)
{
    FILE *fp;
    char buf[256];
    va_list ap;
    int written;

    if (!fmt) return;

    fp = fopen("/tmp/nrettap_debug.log", "a");
    if (!fp) return;

    va_start(ap, fmt);
    written = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (written >= 0) {
        buf[sizeof(buf) - 1] = '\0';
        fputs(buf, fp);
        fputc('\n', fp);
    }

    fclose(fp);
}

static uint8_t parse_norm(const char *s)
{
    float value = s ? (float)atof(s) : 0.0f;
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    return (uint8_t)(value * 255.0f + 0.5f);
}

static uint8_t parse_steps_or_fills(const char *s)
{
    int value = s ? atoi(s) : 0;
    if (value < 0) value = 0;
    if (value > NRETTAP_MAX_STEPS) value = NRETTAP_MAX_STEPS;
    return (uint8_t)value;
}

static int parse_rotation(const char *s)
{
    int value = s ? atoi(s) : 0;
    if (value < -(NRETTAP_MAX_STEPS - 1)) value = -(NRETTAP_MAX_STEPS - 1);
    if (value > (NRETTAP_MAX_STEPS - 1)) value = NRETTAP_MAX_STEPS - 1;
    return value;
}

static uint16_t parse_seed(const char *s)
{
    int value = s ? atoi(s) : 1;
    if (value < 1) value = 1;
    if (value > 9999) value = 9999;
    return (uint16_t)value;
}

static uint8_t parse_span(const char *s)
{
    int value = s ? atoi(s) : 2;
    if (value < 1) value = 1;
    if (value > 4) value = 4;
    return (uint8_t)value;
}

static uint8_t parse_enum_index(const char *s, const char *const *values, int count, int fallback)
{
    if (!s) return (uint8_t)fallback;

    for (int i = 0; i < count; i++) {
        if (strcmp(s, values[i]) == 0) return (uint8_t)i;
    }

    return (uint8_t)fallback;
}

static float current_bpm(void)
{
    if (g_host && g_host->get_bpm) {
        float bpm = g_host->get_bpm();
        if (bpm >= 20.0f && bpm <= 400.0f) return bpm;
    }

    return DEFAULT_BPM;
}

static int current_sample_rate(int sample_rate)
{
    if (sample_rate > 0) return sample_rate;
    if (g_host && g_host->sample_rate > 0) return g_host->sample_rate;
    return DEFAULT_SAMPLE_RATE;
}

static uint32_t frames_per_step(int sample_rate, float bpm, uint8_t rate_mode)
{
    float sr = (float)current_sample_rate(sample_rate);
    float use_bpm = bpm > 0.0f ? bpm : DEFAULT_BPM;
    float quarter = sr * 60.0f / use_bpm;

    switch (rate_mode) {
        case RATE_EIGHTH:
            return (uint32_t)(quarter / 2.0f);
        case RATE_THIRTYSECOND:
            return (uint32_t)(quarter / 8.0f);
        case RATE_SIXTEENTH:
        default:
            return (uint32_t)(quarter / 4.0f);
    }
}

static uint32_t gate_length_frames(uint32_t step_frames)
{
    uint32_t gate = step_frames / 2u;
    return gate > 0u ? gate : 1u;
}

static uint8_t clocks_per_step(uint8_t rate_mode)
{
    switch (rate_mode) {
        case RATE_EIGHTH:
            return 12u;
        case RATE_THIRTYSECOND:
            return 3u;
        case RATE_SIXTEENTH:
        default:
            return 6u;
    }
}

static uint8_t gate_length_clocks(uint8_t step_clocks)
{
    uint8_t gate = (uint8_t)(step_clocks / 2u);
    return gate > 0u ? gate : 1u;
}

static int emit_msg(uint8_t status, uint8_t note, uint8_t velocity,
                    uint8_t out_msgs[][3], int out_lens[], int max_out, int count)
{
    if (count >= max_out) return count;

    out_msgs[count][0] = status;
    out_msgs[count][1] = note;
    out_msgs[count][2] = velocity;
    out_lens[count] = 3;
    return count + 1;
}

static int passthrough_msg(const uint8_t *in_msg, int in_len,
                           uint8_t out_msgs[][3], int out_lens[], int max_out)
{
    if (!in_msg || in_len <= 0 || in_len > 3 || max_out <= 0) return 0;

    for (int i = 0; i < in_len; i++) {
        out_msgs[0][i] = in_msg[i];
    }
    out_lens[0] = in_len;
    return 1;
}

static void restart_playback(NRettapInstance *instance)
{
    nrettap_reset(&instance->engine);
    /*
     * Arm the first step immediately on transport start. Modules like Genera
     * do the equivalent by scheduling a pending step on MIDI Start.
     */
    instance->frames_until_step = 0u;
    instance->frames_until_note_off = 0u;
    instance->midi_clocks_until_step = 0u;
    instance->midi_clocks_until_note_off = 0u;
    instance->reset_pending = 0u;
}

static int flush_active_note(NRettapInstance *instance,
                             uint8_t out_msgs[][3], int out_lens[], int max_out, int count)
{
    if (!instance->active_note_valid) return count;
    if (count >= max_out) return count;

    count = emit_msg((uint8_t)(MIDI_NOTE_OFF | instance->active_channel),
                     instance->active_note, 0,
                     out_msgs, out_lens, max_out, count);
    if (count > 0) {
        instance->active_note_valid = 0u;
        instance->active_note = 0u;
        instance->active_channel = 0u;
        instance->frames_until_note_off = 0u;
    }

    return count;
}

static int prepare_reset(NRettapInstance *instance,
                         uint8_t out_msgs[][3], int out_lens[], int max_out, int count)
{
    if (!instance->reset_pending) return count;

    count = flush_active_note(instance, out_msgs, out_lens, max_out, count);
    if (instance->active_note_valid) return count;

    restart_playback(instance);
    return count;
}

static int emit_engine_step(NRettapInstance *instance,
                            uint8_t out_msgs[][3], int out_lens[], int max_out, int count)
{
    uint32_t step_frames;
    uint8_t step_clocks;

    if (!instance || count >= max_out) return count;

    nrettap_tick(&instance->engine);
    step_frames = frames_per_step((int)instance->sample_rate, current_bpm(), instance->rate_mode);
    if (step_frames == 0u) step_frames = 1u;
    step_clocks = clocks_per_step(instance->rate_mode);

    if (!nrettap_get_trigger(&instance->engine)) return count;

    {
        uint8_t note = nrettap_get_note(&instance->engine);
        uint8_t velocity = nrettap_get_velocity(&instance->engine);

        if (!instance->debug_logged_emit) {
            host_logf("nrettap:first_emit note=%u vel=%u step_frames=%u",
                      (unsigned)note,
                      (unsigned)velocity,
                      (unsigned)step_frames);
            file_logf("nrettap:first_emit note=%u vel=%u step_frames=%u",
                      (unsigned)note,
                      (unsigned)velocity,
                      (unsigned)step_frames);
            instance->debug_logged_emit = 1u;
        }

        count = flush_active_note(instance, out_msgs, out_lens, max_out, count);
        if (count >= max_out) return count;

        count = emit_msg((uint8_t)(MIDI_NOTE_ON | 0u), note, velocity,
                         out_msgs, out_lens, max_out, count);
        if (count > 0) {
            instance->active_note_valid = 1u;
            instance->active_note = note;
            instance->active_channel = 0u;
            instance->frames_until_note_off = gate_length_frames(step_frames);
            instance->midi_clocks_until_note_off = gate_length_clocks(step_clocks);
        }
    }

    return count;
}

static int advance_clock_tick(NRettapInstance *instance,
                              uint8_t out_msgs[][3], int out_lens[], int max_out)
{
    int count = 0;
    uint8_t step_clocks;

    if (!instance || !instance->running) return 0;

    instance->clock_message_mode = 1u;
    count = prepare_reset(instance, out_msgs, out_lens, max_out, count);
    if (count >= max_out) return count;
    if (instance->reset_pending && instance->active_note_valid) return count;

    if (instance->active_note_valid) {
        if (instance->midi_clocks_until_note_off > 0u) instance->midi_clocks_until_note_off--;
        if (instance->midi_clocks_until_note_off == 0u) {
            count = flush_active_note(instance, out_msgs, out_lens, max_out, count);
            if (count >= max_out) return count;
        }
    }

    step_clocks = clocks_per_step(instance->rate_mode);
    if (instance->midi_clocks_until_step > 0u) instance->midi_clocks_until_step--;
    if (instance->midi_clocks_until_step == 0u) {
        count = emit_engine_step(instance, out_msgs, out_lens, max_out, count);
        instance->midi_clocks_until_step = step_clocks;
    }

    return count;
}

static void *nrettap_create_instance(const char *module_dir, const char *config_json)
{
    (void)module_dir;
    (void)config_json;

    NRettapInstance *instance = (NRettapInstance *)calloc(1, sizeof(NRettapInstance));
    int clock_status = MOVE_CLOCK_STATUS_STOPPED;
    if (!instance) return NULL;

    nrettap_init(&instance->engine);
    instance->sample_rate = (uint32_t)current_sample_rate(0);
    instance->rate_mode = RATE_SIXTEENTH;
    instance->reset_pending = 0u;
    instance->active_note_valid = 0u;
    instance->frames_until_note_off = 0u;
    instance->frames_until_step = frames_per_step((int)instance->sample_rate, DEFAULT_BPM, instance->rate_mode);
    if (instance->frames_until_step == 0u) instance->frames_until_step = 1u;
    if (g_host && g_host->get_clock_status) {
        clock_status = g_host->get_clock_status();
    }
    instance->running = (uint8_t)(clock_status == MOVE_CLOCK_STATUS_RUNNING);
    if (!instance->debug_logged_create) {
        host_logf("nrettap:create sr=%u clock_status=%d running=%u",
                  (unsigned)instance->sample_rate,
                  clock_status,
                  (unsigned)instance->running);
        file_logf("nrettap:create sr=%u clock_status=%d running=%u",
                  (unsigned)instance->sample_rate,
                  clock_status,
                  (unsigned)instance->running);
        instance->debug_logged_create = 1u;
    }

    return instance;
}

static void nrettap_destroy_instance(void *state)
{
    free(state);
}

static int nrettap_process_midi(void *state,
                               const uint8_t *in_msg, int in_len,
                               uint8_t out_msgs[][3], int out_lens[], int max_out)
{
    NRettapInstance *instance = (NRettapInstance *)state;
    if (!instance || !in_msg || in_len <= 0) return 0;

    if (in_msg[0] == 0xFA || in_msg[0] == 0xFB || in_msg[0] == 0xFC || in_msg[0] == 0xF8) {
        host_logf("nrettap:process_midi status=0x%02X running=%u", (unsigned)in_msg[0], (unsigned)instance->running);
        file_logf("nrettap:process_midi status=0x%02X running=%u", (unsigned)in_msg[0], (unsigned)instance->running);
    }

    if (in_msg[0] == 0xFA) {
        instance->running = 1u;
        restart_playback(instance);
        instance->midi_clocks_until_step = clocks_per_step(instance->rate_mode);
        return emit_engine_step(instance, out_msgs, out_lens, max_out, 0);
    }

    if (in_msg[0] == 0xFB) {
        if (!instance->running) {
            instance->running = 1u;
            restart_playback(instance);
        }
        return 0;
    }

    if (in_msg[0] == 0xF8) {
        return advance_clock_tick(instance, out_msgs, out_lens, max_out);
    }

    if (in_msg[0] == 0xFC) {
        instance->running = 0u;
        return flush_active_note(instance, out_msgs, out_lens, max_out, 0);
    }

    return passthrough_msg(in_msg, in_len, out_msgs, out_lens, max_out);
}

static int nrettap_tick_wrapper(void *state,
                               int frames, int sample_rate,
                               uint8_t out_msgs[][3], int out_lens[], int max_out)
{
    NRettapInstance *instance = (NRettapInstance *)state;
    int count = 0;
    int use_sample_rate;
    uint32_t step_frames;
    uint32_t remaining;

    if (!instance || frames <= 0) return 0;

    use_sample_rate = current_sample_rate(sample_rate);
    instance->sample_rate = (uint32_t)use_sample_rate;
    if (!instance->debug_logged_tick) {
        host_logf("nrettap:first_tick frames=%d sr=%d running=%u", frames, use_sample_rate, (unsigned)instance->running);
        file_logf("nrettap:first_tick frames=%d sr=%d running=%u", frames, use_sample_rate, (unsigned)instance->running);
        instance->debug_logged_tick = 1u;
    }

    if (g_host && g_host->get_clock_status) {
        int status = g_host->get_clock_status();

        if (status == MOVE_CLOCK_STATUS_STOPPED || status == MOVE_CLOCK_STATUS_UNAVAILABLE) {
            instance->running = 0u;
            return flush_active_note(instance, out_msgs, out_lens, max_out, 0);
        }

        if (status == MOVE_CLOCK_STATUS_RUNNING && !instance->running) {
            instance->running = 1u;
            restart_playback(instance);
        }
    } else if (!instance->running) {
        /* No clock callback — free-run (test/non-Move context) */
        instance->running = 1u;
        restart_playback(instance);
    }

    if (!instance->running) return 0;

    if (instance->clock_message_mode) return 0;

    count = prepare_reset(instance, out_msgs, out_lens, max_out, count);
    if (count >= max_out) return count;
    if (instance->reset_pending && instance->active_note_valid) return count;

    step_frames = frames_per_step(use_sample_rate, current_bpm(), instance->rate_mode);
    if (step_frames == 0u) step_frames = 1u;

    remaining = (uint32_t)frames;
    while (remaining > 0u && count < max_out) {
        uint32_t advance = remaining;

        if (instance->active_note_valid && instance->frames_until_note_off < advance) {
            advance = instance->frames_until_note_off;
        }
        if (instance->frames_until_step < advance) {
            advance = instance->frames_until_step;
        }

        if (advance > 0u) {
            if (instance->active_note_valid) {
                if (advance >= instance->frames_until_note_off) instance->frames_until_note_off = 0u;
                else instance->frames_until_note_off -= advance;
            }

            if (advance >= instance->frames_until_step) instance->frames_until_step = 0u;
            else instance->frames_until_step -= advance;

            remaining -= advance;
        }

        if (instance->active_note_valid && instance->frames_until_note_off == 0u) {
            count = flush_active_note(instance, out_msgs, out_lens, max_out, count);
            if (count >= max_out) break;
        }

        if (instance->reset_pending) {
            count = prepare_reset(instance, out_msgs, out_lens, max_out, count);
            if (count >= max_out || instance->active_note_valid) break;
            step_frames = frames_per_step(use_sample_rate, current_bpm(), instance->rate_mode);
            if (step_frames == 0u) step_frames = 1u;
        }

        if (instance->frames_until_step == 0u) {
            instance->frames_until_step = step_frames;
            count = emit_engine_step(instance, out_msgs, out_lens, max_out, count);
        }

        if (advance == 0u &&
            !(instance->active_note_valid && instance->frames_until_note_off == 0u) &&
            instance->frames_until_step != 0u) {
            break;
        }
    }

    return count;
}

static void nrettap_set_param(void *state, const char *key, const char *val)
{
    static const char *const phrase_values[] = {"up", "down", "pendulum", "random", "thirds"};
    static const char *const root_values[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static const char *const scale_values[] = {
        "major", "minor", "dorian", "mixolydian",
        "phrygian", "lydian", "harmonic_minor", "blues",
        "pentatonic", "chromatic"
    };
    static const char *const rate_values[] = {"1/8", "1/16", "1/32"};

    NRettapInstance *instance = (NRettapInstance *)state;
    if (!instance || !key || !val) return;

    if (strcmp(key, "fills") == 0) {
        nrettap_set_fills(&instance->engine, parse_steps_or_fills(val));
        instance->reset_pending = 1u;
    } else if (strcmp(key, "steps") == 0) {
        nrettap_set_steps(&instance->engine, parse_steps_or_fills(val));
        instance->reset_pending = 1u;
    } else if (strcmp(key, "rotation") == 0) {
        nrettap_set_rotation(&instance->engine, parse_rotation(val));
        instance->reset_pending = 1u;
    } else if (strcmp(key, "chance") == 0) {
        nrettap_set_chance(&instance->engine, parse_norm(val));
    } else if (strcmp(key, "phrase") == 0) {
        nrettap_set_phrase(&instance->engine,
                          parse_enum_index(val, phrase_values, NRETTAP_NUM_PHRASES, NRETTAP_PHRASE_PENDULUM));
        instance->reset_pending = 1u;
    } else if (strcmp(key, "root") == 0) {
        nrettap_set_root(&instance->engine, parse_enum_index(val, root_values, 12, 0));
        instance->reset_pending = 1u;
    } else if (strcmp(key, "scale") == 0) {
        nrettap_set_scale(&instance->engine,
                         parse_enum_index(val, scale_values, NRETTAP_NUM_SCALES, NRETTAP_SCALE_MINOR));
        instance->reset_pending = 1u;
    } else if (strcmp(key, "rate") == 0) {
        instance->rate_mode = parse_enum_index(val, rate_values, 3, RATE_SIXTEENTH);
        instance->frames_until_step = frames_per_step((int)instance->sample_rate, current_bpm(), instance->rate_mode);
        if (instance->frames_until_step == 0u) instance->frames_until_step = 1u;
        instance->midi_clocks_until_step = clocks_per_step(instance->rate_mode);
    } else if (strcmp(key, "span") == 0) {
        nrettap_set_span(&instance->engine, parse_span(val));
        instance->reset_pending = 1u;
    } else if (strcmp(key, "seed") == 0) {
        nrettap_set_seed(&instance->engine, parse_seed(val));
        instance->reset_pending = 1u;
    }
}

static int nrettap_get_param(void *state, const char *key, char *buf, int buf_len)
{
    static const char *const phrase_values[] = {"up", "down", "pendulum", "random", "thirds"};
    static const char *const root_values[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static const char *const scale_values[] = {
        "major", "minor", "dorian", "mixolydian",
        "phrygian", "lydian", "harmonic_minor", "blues",
        "pentatonic", "chromatic"
    };
    static const char *const rate_values[] = {"1/8", "1/16", "1/32"};
    NRettapInstance *instance = (NRettapInstance *)state;

    if (!instance || !key || !buf || buf_len <= 0) return -1;

    if (strcmp(key, "fills") == 0) return snprintf(buf, buf_len, "%u", instance->engine.requested_fills);
    if (strcmp(key, "steps") == 0) return snprintf(buf, buf_len, "%u", instance->engine.steps);
    if (strcmp(key, "rotation") == 0) return snprintf(buf, buf_len, "%d", instance->engine.rotation);
    if (strcmp(key, "chance") == 0) return snprintf(buf, buf_len, "%.4f", instance->engine.chance / 255.0f);
    if (strcmp(key, "phrase") == 0) return snprintf(buf, buf_len, "%s", phrase_values[instance->engine.phrase]);
    if (strcmp(key, "root") == 0) return snprintf(buf, buf_len, "%s", root_values[instance->engine.root % 12u]);
    if (strcmp(key, "scale") == 0) return snprintf(buf, buf_len, "%s", scale_values[instance->engine.scale]);
    if (strcmp(key, "rate") == 0) return snprintf(buf, buf_len, "%s", rate_values[instance->rate_mode]);
    if (strcmp(key, "span") == 0) return snprintf(buf, buf_len, "%u", instance->engine.span);
    if (strcmp(key, "seed") == 0) return snprintf(buf, buf_len, "%u", instance->engine.seed);
    if (strcmp(key, "play_step") == 0) return snprintf(buf, buf_len, "%u", instance->engine.step);
    if (strcmp(key, "sync_warn") == 0) {
        if (g_host && g_host->get_clock_status) {
            int status = g_host->get_clock_status();
            if (status == MOVE_CLOCK_STATUS_UNAVAILABLE) {
                return snprintf(buf, buf_len, "Enable MIDI Clock Out");
            }
            if (status == MOVE_CLOCK_STATUS_STOPPED) {
                return snprintf(buf, buf_len, "stopped");
            }
        }
        return snprintf(buf, buf_len, "%s", "");
    }

    return -1;
}

static midi_fx_api_v1_t g_api = {
    .api_version = MIDI_FX_API_VERSION,
    .create_instance = nrettap_create_instance,
    .destroy_instance = nrettap_destroy_instance,
    .process_midi = nrettap_process_midi,
    .tick = nrettap_tick_wrapper,
    .set_param = nrettap_set_param,
    .get_param = nrettap_get_param
};

midi_fx_api_v1_t *move_midi_fx_init(const host_api_v1_t *host)
{
    g_host = host;
    return &g_api;
}
