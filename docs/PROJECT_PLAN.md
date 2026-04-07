# Project Plan

## Product Direction

Build Schwung MIDI FX modules for Ableton Move, designed around Move constraints and use cases:

- MIDI-first
- standard Schwung UI by default
- portable engine core plus thin host wrapper per module
- predictable build, install, and hardware test workflow

## Standard Delivery Stages

Each module follows these stages:

1. Concept and feasibility (use `audit-open-source-midi-fx` if porting)
2. Architecture and manifest design (`design-module`)
3. Portable engine implementation and native tests (`create-dsp-c`)
4. Schwung host wrapper (`create-host-wrapper`)
5. Hardware build and install (`build-and-install`)
6. Hardware validation (`HARDWARE_TESTING.md`)

## What Is In Scope

For each module:
- one portable engine (`src/dsp/`)
- one `midi_fx` host wrapper (`src/host/`)
- native tests (`tests/`)
- `module.json` manifest (`src/`)
- Move build and install scripts (`scripts/`)
- docs aligned with implementation (`docs/`)

## Explicit Non-Goals

- audio synthesis inside a `midi_fx` slot
- Eurorack panel metaphors that don't translate to Move hardware
- 1:1 firmware cloning of any source instrument
- hidden behavior that cannot be tested locally
- speculative features without a concrete musical use case on Move

## Modules

| Module | Status | Description |
|---|---|---|
| `tumble` | complete | Probabilistic MIDI generator inspired by Mutable Instruments Marbles |

Add new modules to this table as they are started.
