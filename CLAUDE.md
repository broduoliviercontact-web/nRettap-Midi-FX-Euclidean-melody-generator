# CLAUDE.md — Schwung Module Project

This file is read automatically by Claude Code. Read it before doing any work in this repo.

## Project Overview

This repo builds Schwung MIDI FX modules for Ableton Move.

Each module is a shared library (`dsp.so`) loaded by the Schwung host on Move. Modules are MIDI-only — no audio generation. They sit in a Move MIDI FX slot and receive/emit MIDI.

See `docs/PROJECT_PLAN.md` for scope and `docs/MODULES.md` for the full API reference.

---

## General Approach

> Prefer existing Schwung patterns over inventing new abstractions.
> Prefer a smaller stable V1 over a larger speculative implementation.

**The phrase to use when scope drifts:**
> Keep this as a reduced V1. Prefer a stable Schwung-native design over a faithful clone of the source project.

---

## Architecture — Two-Layer Split

Every module follows a strict two-layer split:

```
src/dsp/<module>_engine.h / _engine.c   ← portable engine, no Schwung/Move headers
src/host/<module>_plugin.c              ← Schwung host wrapper, owns plugin interface
```

**Engine layer rules:**
- Pure C, no platform dependencies
- Testable on any machine with `make test`
- Owns the algorithm, state struct, parameter setters/getters

**Host wrapper rules:**
- Includes `midi_fx_api_v1.h` and `plugin_api_v1.h`
- Implements: `create_instance`, `destroy_instance`, `process_midi`, `tick`, `set_param`, `get_param`
- Exposes `move_midi_fx_init()` as the entry point
- Owns MIDI dispatch, parameter parsing, note lifecycle, state serialization

Do NOT mix engine logic into the wrapper. Do NOT include Schwung/Move headers in the engine.

---

## Critical API Rules

### `get_param` must return `snprintf(...)`
```c
// WRONG — host silently ignores the value:
snprintf(buf, buf_len, "%d", val);
return 0;

// CORRECT:
return snprintf(buf, buf_len, "%d", val);

// Unknown param:
return -1;
```
Returning `0` silently breaks param display, chain editing, and state recall.

### Clock sync
- `get_clock_status()` requires "MIDI Clock Out" enabled in Move settings
- `MOVE_CLOCK_STATUS_UNAVAILABLE` must be treated the same as `STOPPED`
- Initialize `running = 0` in `create_instance` — never 1
- Free-running modules must NOT call `get_clock_status()` or `get_bpm()` — SIGSEGV on some firmware
- MIDI transport bytes `0xFA/0xFB/0xFC` are NOT forwarded to external plugins by Schwung

### Parameters
- Float params: clamp 0.0–1.0, encode as `uint8_t` × 255 in the engine, decode for `get_param`
- Every parameter needs: default in `create_instance`, `set_param` handler, `get_param` handler, entry in `module.json`
- Enum params: validate against known values, fall through to default on unknown

### Note lifecycle
- Always send note-off before resetting or changing active note state
- Flush all active notes on transport stop
- Never leave a note open across a mode change or state reset

---

## Which Skill to Use

| Situation | Skill |
|---|---|
| Starting a session or switching context | `commands/schwung/repo-bootstrap.md` |
| Evaluating portability of an external project | `commands/schwung/audit-open-source-midi-fx.md` |
| Defining a module before writing code | `commands/schwung/design-module.md` |
| Implementing a native MIDI FX engine | `commands/schwung/implement-native-midi-fx.md` |
| Building the Move control surface | `commands/schwung/build-move-ui-and-controls.md` |
| Reviewing cross-file coherence | `commands/schwung/convert-open-source-midi-fx.md` |
| Building, deploying, hardware testing | `commands/schwung/build-and-install.md` |
| Generating `module.json` | `commands/templates/create-module-json.md` |
| Generating the portable engine | `commands/templates/create-dsp-c.md` |
| Generating the host wrapper | `commands/templates/create-host-wrapper.md` |

Full workflow guide: `.claude/commands/WORKFLOW.md`

---

## Common Errors to Avoid

1. **Requesting everything at once** — always advance in steps: audit → design → code → test
2. **Generating code before design** — never write code before the module identity and parameters are decided
3. **Too many parameters** — V1 should have 2–6 controls. More is not better on Move.
4. **Forcing a custom UI** — standard Schwung parameter UI is the right default. `OMIT_UI_JS` is a valid result.
5. **Chasing feature parity** — the goal is a good Schwung module, not a faithful clone
6. **Skipping native tests** — always run `make test` before deploying to hardware
7. **`get_param` returning 0** — this silently breaks everything. Always return `snprintf(...)`.

---

## Build

```bash
make test                          # native tests — always first
./scripts/build.sh                 # cross-compile for Move (aarch64)
./scripts/install.sh [move_ip]     # deploy to Move
```

Full build docs: `BUILDING.md`

---

## Install Path on Move

```
/data/UserData/schwung/modules/midi_fx/<module_id>/
```

---

## Before Writing Any Code

1. Read this file
2. Read `docs/MODULES.md` — full API reference
3. Read `src/module.json` — current parameter surface
4. Inspect the existing module in `src/`
5. Run `commands/schwung/repo-bootstrap.md` and write an implementation brief before coding
