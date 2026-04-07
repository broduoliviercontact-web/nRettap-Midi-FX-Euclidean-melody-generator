---
description: Create the Schwung host wrapper for a MIDI FX module
argument-hint: [module-name-and-param-list]
---

Create the Schwung host wrapper (`host/<module>_plugin.c`) for a MIDI FX module based on this request:

$ARGUMENTS

Follow project conventions. Read `repo-bootstrap` if not already done.

Before writing code, inspect:
- `src/host/` — the reference host wrapper pattern in this repo
- `src/host/midi_fx_api_v1.h` — the Schwung MIDI FX plugin interface
- `src/host/plugin_api_v1.h` — the host API struct (`host_api_v1_t`)
- `src/dsp/<module>_engine.h` — the engine API this wrapper will drive
- `src/module.json` — the parameter surface to implement

## Host Wrapper Responsibilities

The host wrapper (`host/<module>_plugin.c`) must:
- Include `midi_fx_api_v1.h`, `plugin_api_v1.h`, and the engine header
- Implement: `plugin_load()`, `create_instance()`, `destroy_instance()`
- Implement: `process_midi()`, `tick()`, `set_param()`, `get_param()`
- Implement: `save_state()`, `load_state()`
- Expose these via the `midi_fx_plugin_v1_t` struct returned by `plugin_load()`

## Per-Instance State Struct

```c
typedef struct {
    EngineType engine;          // wraps the portable engine
    uint32_t frames_until_step; // timing counter
    uint8_t running;            // clock state
    uint8_t sync_mode;          // 0 = move, 1 = internal
    // ... other params
    uint8_t active_note[N_LANES];
    uint8_t active_channel[N_LANES];
    ScheduledEvent events[N_LANES * 2]; // if delayed events needed
} ModuleInstance;
```

## `create_instance` Rules

- Allocate with `calloc(1, sizeof(ModuleInstance))`
- Set ALL parameter defaults explicitly (do not rely on `set_param` being called)
- Initialize `running = 0` — never 1
- Initialize `active_note[i] = 255` (sentinel for "no active note")

## `set_param` Rules

- Parse strings with `atof()` / `atoi()` / string comparison
- Clamp and validate before applying
- Call the engine setter
- Update internal sync/mode flags as needed

## `get_param` Rules — Critical

**Always return `snprintf(buf, buf_len, ...)`.** Never `return 0` for a valid param.

```c
// WRONG — host ignores the value:
if (strcmp(key, "foo") == 0) { snprintf(buf, buf_len, "%.2f", val); return 0; }

// CORRECT:
if (strcmp(key, "foo") == 0) return snprintf(buf, buf_len, "%.2f", val);

// Unknown param:
return -1;
```

## `process_midi` Rules

- Inspect `msg[0] & 0xF0` for message type
- Forward all unrecognized messages with `host_output_midi()`
- Track active notes for note-off safety
- Handle velocity-0 note-on as note-off

## `tick` Rules

- Called every audio buffer; use `nframes` to advance timing
- For transport-synced modules, use the Brindille pattern with `get_clock_status()`
- For free-running modules, do NOT call `get_clock_status()` or `get_bpm()`
- Emit scheduled MIDI events using `host_output_midi()`
- Call engine `tick()` and emit results as MIDI note on/off

## `save_state` / `load_state` Rules

- Simple key=value text format, one per line
- Save every parameter that affects playback
- Do NOT serialize active note state (transient)
- On load: parse each line, validate, apply with same logic as `set_param`

## Note Lifecycle Safety

Before resetting or clearing state, always flush active notes:
```c
for (int i = 0; i < N_LANES; i++) {
    if (inst->active_note[i] != 255) {
        uint8_t off[3] = { 0x80 | inst->active_channel[i], inst->active_note[i], 0 };
        host_output_midi(instance, off, 3, 0);
        inst->active_note[i] = 255;
    }
}
```

## Return Format

Return exactly:
1. A short wrapper summary (runtime model, MIDI behavior)
2. One fenced `c` block containing the full `host/<module>_plugin.c`
3. A short self-review covering:
   - stuck note risk
   - `get_param` return value correctness
   - state restore correctness
   - pass-through behavior
   - any parameter/module.json mismatch

Do not generate the engine layer, `module.json`, or UI files in this step.
