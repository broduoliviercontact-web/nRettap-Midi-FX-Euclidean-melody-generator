---
description: Create or update a Schwung module.json manifest
argument-hint: [module-name-and-feature-summary]
---

Create a production-ready `module.json` for a Schwung MIDI FX module based on this request:

$ARGUMENTS

Follow project conventions. Read `repo-bootstrap` if not already done.

Before drafting, inspect:
- `src/module.json` — the reference manifest for this project
- `src/host/midi_fx_api_v1.h` — capabilities supported by the host API

## Requirements

- Match conventions used in this project's existing `module.json`
- Default to a Signal Chain MIDI FX design unless clearly stated otherwise
- Prefer a compact, Move-friendly parameter surface
- Use a clean `capabilities` structure consistent with the existing module
- If chain editing matters, include `chain_params` matching real editable controls

## Module Identity Rules

```json
{
  "id": "lowercase-filesystem-safe",
  "name": "Display Name",
  "abbrev": "AB",
  "version": "0.1.0",
  "description": "One sentence.",
  "dsp": "dsp.so",
  "api_version": 1
}
```

Prefer `api_version: 1` unless the repo already uses another version and that exact choice is verified on hardware.

## Capabilities Block

Standard MIDI FX:
```json
"capabilities": {
  "chainable": true,
  "component_type": "midi_fx",
  "ui_hierarchy": {
    "levels": {
      "root": {
        "name": "Module Name",
        "knobs": ["param1", "param2", "param3", "param4", "param5", "param6", "param7", "param8"],
        "params": ["param1", "param2", ...]
      }
    }
  }
}
```

Prefer the smallest proven capabilities block. Do not add `audio_in`, `audio_out`, `midi_in`, or `midi_out` unless the repo already uses them intentionally and hardware behavior has been verified.

## Parameter Rules

- Prefer 2–8 primary parameters for V1
- Use realistic types: `float`, `int`, `enum`, `toggle`
- Every parameter must have a sensible `default`
- Knob assignments must match the most important controls (ordered by musical priority)
- `chain_params` should be the live-editable subset — omit parameters that are unsafe to edit in chain context
- Use `max_param` only when the UX should truly clamp editing live against another parameter
- If the engine preserves a requested value separately from an effective clamped value, do NOT blindly encode that relationship into `max_param`

## Supported Types

```json
{ "key": "foo", "name": "Foo", "type": "float", "min": 0.0, "max": 1.0, "default": 0.5, "step": 0.01 }
{ "key": "bar", "name": "Bar", "type": "int", "min": 1, "max": 32, "default": 16, "step": 1 }
{ "key": "mode", "name": "Mode", "type": "enum", "options": ["a", "b", "c"] }
{ "key": "active", "name": "Active", "type": "toggle", "default": false }
```

## Decision Rules

- If the request is underspecified, infer a minimal usable V1 design and state your assumptions
- Do not invent unsupported manifest fields
- Do not add parameters that the engine will not actually support
- Keep V1 small and stable

## Return Format

Return exactly:
1. A short design summary
2. One fenced `json` block containing the full `module.json`
3. A short assumptions section listing any decisions made where the spec was ambiguous

Do not generate native C or UI files in this step.
