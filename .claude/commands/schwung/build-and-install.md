# Build and Install Schwung Module

## Purpose
Use this skill to build, install, and verify a Schwung module on Move.

Follow this order every time.

## Step 1 — Native Tests

```bash
make test
```

All tests must pass before touching hardware. Fix failures before continuing.

## Step 2 — Build for Move (aarch64)

```bash
./scripts/build.sh
```

Produces `build/aarch64/dsp.so`.

Compiler preference order:
1. `aarch64-linux-gnu-gcc`
2. `aarch64-linux-musl-gcc`
3. `zig cc -target aarch64-linux-gnu`
4. Docker (fallback — less reliable)

Verify after build:
```bash
ls -la build/aarch64/dsp.so
```

## Step 3 — Install to Move

```bash
./scripts/install.sh
# or with a specific IP:
./scripts/install.sh 192.168.x.x
```

Install path on Move:
```
/data/UserData/schwung/modules/midi_fx/<module_id>/
```

Verify installed:
```bash
ssh root@move.local 'ls -la /data/UserData/schwung/modules/midi_fx/<module_id>'
```

## Step 4 — Restart Schwung

```bash
ssh root@move.local 'cd /data/UserData/schwung && nohup ./start.sh >/tmp/start_schwung.log 2>&1 </dev/null &'
```

Wait a few seconds, then check the log:
```bash
ssh root@move.local 'cat /tmp/start_schwung.log'
```

## Step 5 — Smoke Test

```bash
./scripts/smoke_test.sh
```

## Step 6 — Hardware Validation

Follow `docs/HARDWARE_TESTING.md`.

Minimum check:
1. Insert the module in a MIDI FX slot
2. Put a downstream instrument after it
3. Start Move transport
4. Confirm output starts and stops cleanly
5. Confirm no stuck notes after Stop

## Common Failures

**Module does not appear in FX slot**
- Rebuild and reinstall — confirm `build/aarch64/dsp.so` exists
- Verify install path matches `module.json` `id`
- Restart Schwung

**No MIDI output**
- Confirm a downstream instrument is after the module in the chain
- Confirm transport is running
- Confirm Sync mode matches your setup

**Build fails on macOS**
- Prefer `zig cc` over Docker
- If Docker fails with metadata/I/O errors, fix Docker Desktop or skip Docker

**`get_param` values not displaying in UI**
- Check that `get_param` returns `snprintf(buf, buf_len, ...)` not `return 0`
- Returning 0 silently breaks param display and chain editing

**Transport doesn't start the module**
- Verify "MIDI Clock Out" is enabled in Move settings (required for `get_clock_status()`)
- Check `sync_warn` param if implemented
- If the wrapper is clock-driven, verify it handles `0xF8` in `process_midi()` instead of relying only on `tick()`
- Add temporary file logging in the wrapper if needed:
```c
fopen("/tmp/<module>_debug.log", "a");
```
- After reproducing on hardware, inspect:
```bash
ssh root@move.local 'cat /tmp/<module>_debug.log'
```
- Useful checkpoints:
  - instance created
  - first `tick()`
  - received `0xFA`
  - received repeated `0xF8`
  - first emitted note

**Parameter edits feel stuck or snap back**
- Check whether `get_param()` returns an internal effective value instead of the user-facing requested value
- Check custom `ui.js` for dynamic clamping that should live only in engine logic
- Revisit `max_param` in `module.json`
