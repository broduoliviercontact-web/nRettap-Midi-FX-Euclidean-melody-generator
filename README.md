# Pattrn

**Euclidean melody generator for Ableton Move**

`Pattrn` is a chainable `midi_fx` module for Schwung on Ableton Move. It takes a compact Euclidean rhythm core, applies probability, then turns each trigger into a scale-quantized melodic note.

The design is inspired by the musical ideas behind `pattrns`, but adapted as a reduced Schwung-native module for Move rather than a faithful port of the original project.

## Features

- Euclidean rhythm generation with `fills`, `steps`, and `rotation`
- Probability gate with `chance`
- 5 phrase modes: `up`, `down`, `pendulum`, `random`, `thirds`
- Scale-quantized melodic output
- Root note and scale selection
- Span control for melodic range
- Seeded deterministic variation
- Move transport sync
- Starts on `Play` without requiring a played or sequenced input note
- Minimal custom UI for direct knob control and sync warning display

## Prerequisites

- [Schwung](https://github.com/charlesvestal/move-anything) installed on your Ableton Move
- SSH enabled if you want to install manually: `http://move.local/development/ssh`

Important for `Pattrn`:

- `Pattrn` is transport-driven
- on Move, `Settings -> MIDI -> MIDI Clock Out` must be enabled
- if `MIDI Clock Out` is disabled, the module correctly stays silent and shows a warning

## Installation

### Manual Installation

Run native tests first:

```bash
make test
```

Build for Move:

```bash
./scripts/build.sh
```

Install:

```bash
./scripts/install.sh
```

Install path on Move:

```text
/data/UserData/schwung/modules/midi_fx/pattrn
```

## Usage

`Pattrn` is a MIDI FX module:

1. Insert `Pattrn` in a chain MIDI FX slot.
2. Put a synth, sampler, or melodic instrument after it in the same chain.
3. Enable `MIDI Clock Out` on Move.
4. Press `Play`.
5. Shape rhythm with `fills`, `steps`, `rotation`, and `chance`.
6. Shape melody with `phrase`, `root`, `scale`, `span`, and `seed`.

`Pattrn` does not need an input note to begin playback. It generates directly from transport.

## Quick Start

Recommended first patch:

- `fills = 5`
- `steps = 8`
- `rotation = 0`
- `chance = 1.0`
- `phrase = pendulum`
- `root = C`
- `scale = minor`
- `rate = 1/16`
- `span = 2`
- `seed = 1`

Then try:

- raise `fills` for more activity
- rotate the pattern to move the accent placement
- lower `chance` for sparser phrases
- switch `phrase` to `random` or `thirds`
- change `seed` for a new deterministic melodic family

## Parameters

### Rhythm

| Parameter | What it does |
|---|---|
| `fills` | Number of Euclidean pulses in the pattern. |
| `steps` | Pattern length from `1` to `16`. |
| `rotation` | Rotates the Euclidean pattern. |
| `chance` | Probability that a rhythmic hit actually emits a note. |
| `rate` | Musical subdivision: `1/8`, `1/16`, `1/32`. |

### Melody

| Parameter | What it does |
|---|---|
| `phrase` | Note order strategy: `up`, `down`, `pendulum`, `random`, `thirds`. |
| `root` | Root note from `C` to `B`. |
| `scale` | Quantizer scale: `major`, `minor`, `dorian`, `mixolydian`, `pentatonic`, `chromatic`. |
| `span` | Melodic range in scale spans. |
| `seed` | Deterministic variation seed. |

## Move UI

`Pattrn` uses a minimal full-screen UI for direct access to the main controls and for sync feedback.

Direct knobs:

| Knob | Parameter |
|---|---|
| 1 | `fills` |
| 2 | `steps` |
| 3 | `rotation` |
| 4 | `chance` |
| 5 | `phrase` |
| 6 | `root` |
| 7 | `scale` |
| 8 | `rate` |

Secondary parameters:

- `span`
- `seed`

Warning behavior:

- `! Enable MIDI Clock Out` means Move clock status is unavailable
- `! stopped` means Move sync is available but transport is stopped

## Transport Behavior

`Pattrn` is driven by Move transport.

Current behavior:

- first step is emitted immediately on transport start
- musical advancement follows Move MIDI clock
- note-offs are scheduled safely inside the same clock-driven path
- `Stop` flushes active notes

This avoids the common Schwung bug where a module appears to do nothing until a note is played into the chain.

## Troubleshooting

**No output**

- verify a downstream instrument is present after `Pattrn`
- verify `Settings -> MIDI -> MIDI Clock Out` is enabled on Move
- verify Move transport is actually running
- check the warning line in the module UI

**Module appears but stays silent**

- test with `chance = 1.0`
- set `fills = 8` and `steps = 8`
- use `scale = chromatic` if you want to rule out scale expectations

**Unexpected melodic behavior**

- try `phrase = up` first
- lower `span`
- change `seed`

**Parameter feels inconsistent**

- `fills` is preserved as the requested value for editing and recall
- the engine may still clamp the effective playback against `steps` internally

## Building from Source

```bash
make native
make test
make check-symbols
./scripts/build.sh
```

## Architecture

- portable engine: `src/dsp/pattrn_engine.h` / `src/dsp/pattrn_engine.c`
- Schwung host wrapper: `src/host/pattrn_plugin.c`
- manifest: `src/module.json`
- Move UI: `src/ui.js`

The engine is fully separate from the Schwung wrapper. The wrapper owns transport, MIDI scheduling, parameter parsing, and note lifecycle.

## Credits

- musical inspiration: Renoise `pattrns`
- Schwung and Move module framework: Charles Vestal and contributors
- Move adaptation and implementation: this repo

## License

MIT
