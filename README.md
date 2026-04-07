# nRettap

Euclidean melody generator for Ableton Move, built as a Schwung `midi_fx` module.

`nRettap` is a compact pattern generator inspired by Renoise `pattrns`, rewritten for Move as a transport-synced MIDI effect. It emits notes directly into the next instrument in the chain and starts on `Play`, without needing an incoming note to wake it up.

## Features

- Euclidean rhythm core with `fills`, `steps`, and `rotation`
- Melodic phrases with `up`, `down`, `pendulum`, `random`, and `thirds`
- Scale quantization with root and span control
- Deterministic variation with `seed`
- Move transport sync via MIDI clock
- Immediate first step on transport start
- Safe note-off handling on stop and structural changes
- Portable DSP engine split from the Schwung host wrapper
- Native C tests for engine and wrapper behavior

## Transport Behavior

`nRettap` is clock-driven on Move.

- `0xFA` starts playback and triggers the first step immediately
- `0xF8` advances the pattern
- `0xFC` stops playback and flushes active notes

This is important for Schwung MIDI FX modules on Move: relying on `tick()` alone can make a module appear idle until another MIDI event arrives.

## Parameters

Main controls:

- `fills`
- `steps`
- `rotation`
- `chance`
- `phrase`
- `root`
- `scale`
- `rate`

Secondary controls:

- `span`
- `seed`

## Installation

Run tests first:

```bash
make test
```

Build for Move:

```bash
./scripts/build.sh
```

Install to the default Move host:

```bash
./scripts/install.sh
```

Install to a specific host:

```bash
./scripts/install.sh 192.168.x.x
```

The install target is derived from [`src/module.json`](src/module.json), so the module is installed under:

```text
/data/UserData/schwung/modules/midi_fx/nrettap
```

## Quick Start

1. Insert `nRettap` in a `MIDI FX` slot.
2. Put an instrument after it in the same chain.
3. Set `fills=8`, `steps=8`, `chance=1.0`, `rate=1/16`.
4. Press `Play`.
5. Shape the result with `rotation`, `phrase`, `root`, and `scale`.

If nothing happens, check that Move MIDI clock is enabled. The module UI exposes a `sync_warn` message when clock is unavailable.

## Move UI

The root UI maps the 8 knobs directly:

| Knob | Param |
|---|---|
| 1 | `fills` |
| 2 | `steps` |
| 3 | `rotation` |
| 4 | `chance` |
| 5 | `phrase` |
| 6 | `root` |
| 7 | `scale` |
| 8 | `rate` |

There is also a minimal custom UI in [`src/ui.js`](src/ui.js) to surface clock warnings clearly on-device.

## Project Layout

- Engine: [`src/dsp/nrettap_engine.h`](src/dsp/nrettap_engine.h), [`src/dsp/nrettap_engine.c`](src/dsp/nrettap_engine.c)
- Host wrapper: [`src/host/nrettap_plugin.c`](src/host/nrettap_plugin.c)
- Manifest: [`src/module.json`](src/module.json)
- Move UI: [`src/ui.js`](src/ui.js)
- Tests: [`tests/nrettap_engine_test.c`](tests/nrettap_engine_test.c), [`tests/nrettap_midi_fx_test.c`](tests/nrettap_midi_fx_test.c)

## Development

Useful commands:

```bash
make test
make native
make aarch64
make check-symbols
```

The expected exported symbol is `move_midi_fx_init`.

## License

This project is MIT-licensed. It is inspired by `pattrns`, but implemented as a clean Move/Schwung adaptation rather than a direct code port.
