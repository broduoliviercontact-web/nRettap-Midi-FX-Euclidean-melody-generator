# MODULES.md — Schwung Module API Reference

## Overview

A Schwung module is a folder containing a `module.json` manifest and a `dsp.so` shared library. The host loads the library, calls `move_midi_fx_init()` to get the plugin vtable, and drives the module through that interface.

Modules are MIDI-only in this project (`component_type: "midi_fx"`). They receive MIDI input, produce MIDI output, and have no audio I/O.

---

## Module Folder Structure

```
<module_id>/
  module.json       ← manifest (required)
  dsp.so            ← compiled shared library (required for native modules)
  ui.js             ← custom Move UI (optional)
  ui_chain.js       ← compact chain-context UI (optional)
```

Deploy to:
```
/data/UserData/schwung/modules/midi_fx/<module_id>/
```

---

## `module.json` — Manifest Format

```json
{
  "id": "module_id",
  "name": "Display Name",
  "abbrev": "AB",
  "version": "0.1.0",
  "description": "One sentence description.",
  "author": "Author name",
  "license": "MIT",
  "dsp": "dsp.so",
  "api_version": 1,
  "capabilities": {
    "audio_out": false,
    "audio_in": false,
    "midi_in": true,
    "midi_out": true,
    "chainable": true,
    "component_type": "midi_fx",
    "ui_hierarchy": {
      "levels": {
        "root": {
          "name": "Module Name",
          "knobs": ["param1", "param2", "param3", "param4"],
          "params": ["param1", "param2", "param3", "param4", "param5"]
        }
      }
    }
  },
  "chain_params": [
    { "key": "param1", "name": "Param 1", "type": "float", "min": 0.0, "max": 1.0, "default": 0.5, "step": 0.01 }
  ]
}
```

### Field Reference

| Field | Required | Description |
|---|---|---|
| `id` | yes | Lowercase, filesystem-safe identifier. Must match the module folder name. |
| `name` | yes | Display name shown in the UI |
| `abbrev` | yes | 2–3 character abbreviation for compact contexts |
| `version` | yes | Semver string |
| `dsp` | yes | Shared library filename — always `"dsp.so"` |
| `api_version` | yes | Must be `1` |
| `capabilities` | yes | Module capabilities and UI hierarchy |
| `chain_params` | no | Parameters editable in Signal Chain context |

### Capabilities Fields

| Field | Type | Description |
|---|---|---|
| `audio_in` / `audio_out` | bool | Always `false` for `midi_fx` modules |
| `midi_in` / `midi_out` | bool | Always `true` for `midi_fx` modules |
| `chainable` | bool | Always `true` for Signal Chain modules |
| `component_type` | string | Always `"midi_fx"` in this project |
| `ui_hierarchy` | object | Defines the standard Move parameter UI |

### `ui_hierarchy`

```json
"ui_hierarchy": {
  "levels": {
    "root": {
      "name": "Module Name",
      "knobs": ["p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8"],
      "params": ["p1", "p2", "p3", ...]
    }
  }
}
```

- `knobs` — up to 8 hardware knob assignments, ordered by musical priority
- `params` — full list of parameters shown in the standard UI browser

---

## Parameter Types

| Type | Description | Example |
|---|---|---|
| `float` | Normalized 0.0–1.0 | `{ "type": "float", "min": 0.0, "max": 1.0, "default": 0.5, "step": 0.01 }` |
| `int` | Integer with min/max | `{ "type": "int", "min": 1, "max": 32, "default": 16, "step": 1 }` |
| `enum` | String from a fixed list | `{ "type": "enum", "options": ["a", "b", "c"] }` |
| `toggle` | Boolean | `{ "type": "toggle", "default": false }` |

Every parameter in `chain_params` must have a `default`.

---

## Plugin API — `midi_fx_api_v1.h`

The shared library must export one symbol:

```c
midi_fx_api_v1_t *move_midi_fx_init(const host_api_v1_t *host);
```

This is called once at load time. Return a pointer to a static `midi_fx_api_v1_t` struct populated with all function pointers.

### `midi_fx_api_v1_t`

```c
typedef struct midi_fx_api_v1 {
    uint32_t api_version;                    // must be MIDI_FX_API_VERSION (1)
    void *(*create_instance)(...);           // allocate per-instance state
    void (*destroy_instance)(void *);        // free per-instance state
    int  (*process_midi)(...);               // handle incoming MIDI
    int  (*tick)(...);                       // called every audio block
    void (*set_param)(void *, const char *key, const char *val);
    int  (*get_param)(void *, const char *key, char *buf, int buf_len);
} midi_fx_api_v1_t;
```

### `create_instance`

```c
void *create_instance(const char *module_dir, const char *config_json);
```

- Called once per slot insert
- Allocate with `calloc(1, sizeof(YourInstance))`
- Set ALL parameter defaults — do not rely on `set_param` being called at init
- Initialize `running = 0` (never 1)
- Return `NULL` on allocation failure

### `destroy_instance`

```c
void destroy_instance(void *instance);
```

- `free(instance)`

### `process_midi`

```c
int process_midi(void *instance,
                 const uint8_t *in_msg, int in_len,
                 uint8_t out_msgs[][3], int out_lens[],
                 int max_out);
```

- Called for each incoming MIDI message
- Return number of output messages written (0 to `max_out`)
- Pass through unrecognized messages by copying them to `out_msgs`
- Handle note lifecycle: track active notes, flush on transport stop
- Note: `0xFA/0xFB/0xFC` transport bytes are NOT forwarded by the host — handle them here if needed for internal state, but do not rely on them for primary sync

### `tick`

```c
int tick(void *instance,
         int frames, int sample_rate,
         uint8_t out_msgs[][3], int out_lens[],
         int max_out);
```

- Called every audio buffer (typically 64–512 frames)
- `frames` is the number of audio frames in this block
- `sample_rate` is the current sample rate (typically 44100 or 48000)
- Return number of output messages written
- Use this to emit time-based MIDI events (steps, arpeggios, LFO CCs, etc.)
- Do not allocate or block in `tick`

### `set_param`

```c
void set_param(void *instance, const char *key, const char *val);
```

- Called when the user or host changes a parameter
- `val` is always a string — parse with `atof()`, `atoi()`, or string comparison
- Validate and clamp before applying
- Call engine setters for engine-owned parameters

### `get_param`

```c
int get_param(void *instance, const char *key, char *buf, int buf_len);
```

- **Must return `snprintf(buf, buf_len, ...)`** — never `return 0` for a valid param
- Return `-1` for unknown keys
- Returning `0` silently breaks param display, chain editing, and state recall

```c
// CORRECT:
if (strcmp(key, "density") == 0)
    return snprintf(buf, buf_len, "%.4f", inst->engine.density / 255.0f);

// Unknown:
return -1;
```

---

## Host API — `plugin_api_v1.h`

The `host_api_v1_t *host` pointer passed to `move_midi_fx_init()` provides host services.

```c
typedef struct host_api_v1 {
    uint32_t api_version;
    int sample_rate;
    int frames_per_block;
    void (*log)(const char *msg);
    int  (*get_clock_status)(void);   // see clock status constants below
    float (*get_bpm)(void);           // current Move BPM
    int  (*midi_send_internal)(const uint8_t *msg, int len);
    int  (*midi_send_external)(const uint8_t *msg, int len);
    // modulation routing (advanced use)
    move_mod_emit_value_fn mod_emit_value;
    move_mod_clear_source_fn mod_clear_source;
    void *mod_host_ctx;
} host_api_v1_t;
```

Store the host pointer as a module-level static on init:

```c
static const host_api_v1_t *g_host = NULL;

midi_fx_api_v1_t *move_midi_fx_init(const host_api_v1_t *host) {
    g_host = host;
    return &g_api;
}
```

### Clock Status

```c
#define MOVE_CLOCK_STATUS_UNAVAILABLE 0  // MIDI Clock Out not enabled in Move settings
#define MOVE_CLOCK_STATUS_STOPPED     1  // transport stopped
#define MOVE_CLOCK_STATUS_RUNNING     2  // transport running
```

**Critical:** `UNAVAILABLE` means the user has not enabled "MIDI Clock Out" in Move Settings → MIDI → MIDI Clock Out. Treat it the same as `STOPPED`.

**Free-running modules must NOT call `get_clock_status()` or `get_bpm()`** — this causes SIGSEGV on some Move firmware versions. Use a constant BPM instead.

### Brindille Pattern — Transport Sync in `tick()`

```c
if (inst->sync_mode == SYNC_MOVE && g_host && g_host->get_clock_status) {
    int status = g_host->get_clock_status();
    if (status == MOVE_CLOCK_STATUS_STOPPED ||
        status == MOVE_CLOCK_STATUS_UNAVAILABLE) {
        inst->running = 0;
    } else if (status == MOVE_CLOCK_STATUS_RUNNING) {
        inst->running = 1;
    }
} else if (inst->sync_mode == SYNC_INTERNAL) {
    inst->running = 1;
}
```

---

## Parameter Encoding Convention

Float parameters are stored as `uint8_t` (0–255) in the engine for compactness:

```c
// set from host string:
static uint8_t parse_norm(const char *s) {
    float v = s ? (float)atof(s) : 0.0f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return (uint8_t)(v * 255.0f + 0.5f);
}

// get for host:
return snprintf(buf, buf_len, "%.4f", inst->engine.param / 255.0f);
```

Enum parameters: parse by string comparison against a static list, fall through to default on unknown value.

---

## Note Lifecycle Safety

Track active notes per output lane/channel. Before resetting state, flush all active notes:

```c
static int flush_lane(Instance *inst, int lane,
                      uint8_t out[][3], int lens[], int max, int count) {
    if (inst->lane_active[lane]) {
        out[count][0] = 0x80 | inst->active_channel[lane];
        out[count][1] = inst->active_note[lane];
        out[count][2] = 0;
        lens[count] = 3;
        count++;
        inst->lane_active[lane] = 0;
    }
    return count;
}
```

Always flush on: transport stop, mode change while notes are held, state reset.

---

## State Serialization

Implement `save_state` / `load_state` using simple key=value text:

```
density=0.5000
spread=0.3500
mode=drum
steps=16
```

Rules:
- Save every parameter that affects playback
- Do NOT save transient note state (active notes, scheduled events)
- On load: validate each value before applying (same logic as `set_param`)
- Handle missing keys gracefully — use defaults

---

## Output Message Limits

```c
#define MIDI_FX_MAX_OUT_MSGS 16
```

`process_midi` and `tick` must never write more than `max_out` messages. Always check `count < max_out` before writing.

---

## Scheduled Events Pattern

For delayed MIDI output within a `tick()` block:

```c
typedef struct {
    uint8_t  active;
    uint8_t  lane;
    uint8_t  channel;
    uint8_t  note;
    uint8_t  velocity;
    uint8_t  type;       // NOTE_ON or NOTE_OFF
    uint32_t frames_left;
} ScheduledEvent;
```

Advance all events by the number of frames elapsed. Emit events when `frames_left == 0`. Emit note-offs before note-ons in the same frame to avoid MIDI ordering issues.
