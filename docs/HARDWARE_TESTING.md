# Hardware Testing Checklist

## Pre-flight

- Build with `./scripts/build.sh`
- Install with `./scripts/install.sh`
- Verify the module folder exists on Move: `ssh root@move.local 'ls /data/UserData/schwung/modules/midi_fx/<module_id>'`
- Place the module in a MIDI FX slot
- Put a downstream instrument after it in the chain

## Transport

- Start Move transport
- Confirm the module responds correctly (output starts, or pass-through is stable)
- Stop transport — confirm no stuck notes remain
- Restart transport — confirm behavior is deterministic from the beginning

## Clock and Sync

- If the module supports `Sync = move`: verify it follows Move BPM
- If the module supports `Sync = internal`: verify `BPM` changes the rate audibly
- If there is a rate or division parameter: sweep through all values and confirm timing changes correctly
- Compare sync modes — behavior must be stable in both

## Parameters

- Sweep every knob-mapped parameter across its full range
- Confirm each parameter produces an audible or measurable change
- Confirm no parameter causes a crash, freeze, or stuck state
- Confirm defaults produce reasonable behavior on first insert

## MIDI Behavior

- Confirm expected MIDI output (notes, CCs, or pass-through)
- Confirm MIDI channel routing is correct
- If the module transforms MIDI input: verify the transformation is accurate
- If the module generates MIDI: verify it starts and stops cleanly with transport

## Note Lifecycle

- Verify note-offs are always sent — test these scenarios:
  - Transport stop while notes are held
  - Mode or parameter change while notes are sounding
  - Module bypass or removal from the chain
- Use a sustaining instrument (long release or infinite hold) to surface stuck notes

## State Recall

- Change several parameters away from their defaults
- Force a state reload (reopen the session or reinstall the module)
- Confirm all parameters restore to the saved values
- Confirm playback behavior matches the restored state

## Failure Cases

| Symptom | Likely cause |
|---|---|
| Module does not appear in FX slot | Install path wrong, or Schwung needs restart |
| No MIDI output | Missing downstream instrument, transport not running, sync misconfigured |
| Stuck notes after stop | Note lifecycle bug — missing note-off on transport stop |
| Output continues after stop | `get_clock_status()` not checked correctly in `tick()` |
| Parameter change has no effect | `set_param` not calling engine setter, or wrong key |
| Parameter value not visible in UI | `get_param` returning `0` instead of `snprintf(...)` result |
| Timing instability or drift | BPM source inconsistency or tick accumulator overflow |

## Local Smoke Sequence

Run before connecting to hardware:

```bash
make test
```

Run after install for a quick sanity check:

```bash
./scripts/smoke_test.sh
```
