# Module Design & Porting Notes

## Overview

Document the intent and design decisions for each module in this project.

The target is not replication of the source material. The target is a practical Schwung module that:

- delivers the core musical behavior cleanly
- maps naturally to Move transport and MIDI output
- maintains strict separation between DSP logic and host integration

---

## Architecture Checklist

Use this checklist for every new module before considering it complete:

- [ ] portable engine isolated in `src/dsp/`
- [ ] Schwung wrapper isolated in `src/host/`
- [ ] standard Schwung UI via `module.json`, or justified custom `ui.js`
- [ ] native tests for engine and wrapper
- [ ] Move build/install scripts functional
- [ ] hardware test checklist passed (`docs/HARDWARE_TESTING.md`)

---

## Per-Module Notes

Add a section below for each module. Document:

### `<module_id>`

**Concept**: What does this module do musically? What inspired it?

**Fidelity target**: What is being adapted, and at what level of faithfulness?

**Simplified on purpose**:
- list things that were deliberately dropped or approximated
- explain why each simplification makes sense for Move/MIDI

**Technical decisions**:
- `component_type`: `midi_fx` or other
- UI strategy: standard parameter UI, or custom `ui.js`
- clock strategy: `move` transport sync, internal BPM, or free-running
- output mode(s): what MIDI is produced and how
- state strategy: how parameters and any internal state are saved and restored

**Version log**:

| Date | Area | Change | Status |
|---|---|---|---|
| YYYY-MM-DD | | | |

**Remaining work**:
- list known gaps, tuning needs, or planned improvements

---

## Schwung/Move Design Principles

Apply these across all modules:

- Move is a MIDI device, not a CV environment — design for MIDI output, not signals
- the Schwung MIDI FX ABI rewards a block-based event scheduler via `tick()`
- `get_clock_status()` requires "MIDI Clock Out" enabled in Move settings — treat `UNAVAILABLE` as `STOPPED`
- MIDI transport messages `0xFA/0xFB/0xFC` are not forwarded to external plugins
- free-running modules must NOT call `get_clock_status()` or `get_bpm()` — use a constant BPM
- `get_param` must return `snprintf(buf, buf_len, ...)` — returning `0` silently breaks param display
- an MVP that is stable and explainable beats an opaque emulation
