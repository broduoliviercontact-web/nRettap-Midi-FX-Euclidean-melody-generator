---
description: Create the Schwung host wrapper for a MIDI FX module
argument-hint: [module-name-and-param-list]
---

Create the Schwung host wrapper (`host/<module>_plugin.c`) for a MIDI FX module based on this request:

$ARGUMENTS

Follow project conventions. Read `repo-bootstrap` if not already done.

Before writing code, inspect:
- `src/host/tumble_plugin.c` — the reference host wrapper
- `src/host/midi_fx_api_v1.h` — the Schwung MIDI FX plugin interface
- `src/host/plugin_api_v1.h` — the host API struct (`host_api_v1_t`)
- `src/dsp/<module>_engine.h` — the engine API this wrapper will drive
- `src/module.json` — the parameter surface to implement

## Host Wrapper Responsibilities

The host wrapper (`host/<module>_plugin.c`) must:
- Include `midi_fx_api_v1.h`, `plugin_api_v1.h`, and the engine header
- Implement: `move_midi_fx_init()`, `create_instance()`, `destroy_instance()`
- Implement: `process_midi()`, `tick()`, `set_param()`, `get_param()`
- Expose these via the `midi_fx_api_v1_t` struct returned by `move_midi_fx_init()`

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
- Forward all unrecognized messages via `out_msgs`
- Track active notes for note-off safety
- Handle velocity-0 note-on as note-off
- Do not assume `process_midi()` is only for note input
- Hardware-proven Move behavior: time-based `midi_fx` modules may receive `0xFA`, `0xFB`, `0xFC`, and repeated `0xF8` here
- For clock-driven generators/arps:
  - arm or emit the first step immediately on `0xFA`
  - advance the pattern on `0xF8`
  - if note duration is expressed in MIDI clocks, handle note-offs in this path too
- If playback only starts after a played note, inspect whether the wrapper is ignoring incoming `0xF8` clock bytes

## `tick` Rules

- Called every audio buffer; use `nframes` to advance timing
- For transport-synced modules, use the Brindille pattern with `get_clock_status()`
- For free-running modules, do NOT call `get_clock_status()` or `get_bpm()`
- Emit scheduled MIDI events via `out_msgs`
- Call engine `tick()` and emit results as MIDI note on/off
- Do not duplicate scheduling across both `tick()` and `process_midi()` without a clear ownership rule
- If MIDI clock bytes are the primary scheduler, `tick()` should not also advance the same sequence timeline

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

## Parameter Semantics

- If a parameter has both a requested value and an effective clamped value, decide which one is user-facing
- Default: `get_param()` should expose the requested value so UI editing and recall round-trip cleanly
- Example: if `fills` is internally capped by `steps`, preserve requested `fills` separately instead of silently rewriting the visible value
- Be cautious with `max_param` in `module.json`: it changes editing behavior and can make a control appear "stuck"

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
