# KNTKTA Architecture & Developer Guide

## Project Overview

KNTKTA is a modular platform centred around the Akai Push 1 controller.
It provides universal parameter mapping, preset management, network connectivity,
AI-assisted control, and a built-in sampler.

---

## Module Dependency Graph

```
push_surface (Python)
    │
    ├── parameter_mapper ──→ preset_forge (KPS)
    ├── macro_engine
    ├── action_system
    ├── song_structure ──→ link_bridge (OSC broadcast)
    └── modes/link_mode ──→ link_bridge (device params)
                                │
                    ┌───────────┼───────────────┐
                osc_server   link_engine   device_registry
                    │
                ios_companion (Swift)

kntkta_core (JUCE C++)
    ├── KntktaProcessor (VST3/AU/Standalone)
    ├── PresetManager ──→ preset_forge (Python IPC)
    ├── LinkBridge ──→ link_bridge (Python IPC)
    └── AiClient ──→ kntkta_ai MCP server (stdio/TCP)

kntkta_ai (Python + Ollama)
    ├── MCP server (JSON-RPC)
    ├── tool_registry (10 tools)
    └── llm_bridge (Ollama + Whisper)

sound_forge (Python)
    ├── BeatChopper ──→ SFZ / Decent Sampler / Ableton ADG exports
    └── ChromaticResampler ──→ MidiNoteSender + audio capture
```

---

## Communication Protocols

| Connection | Protocol | Direction |
|---|---|---|
| Push 1 ↔ Ableton | MIDI SysEx | Bidirectional |
| push_surface ↔ kntkta_ai | TCP socket (MCP) | Bidirectional |
| kntkta_core ↔ kntkta_ai | stdio / TCP (MCP) | Bidirectional |
| kntkta_core ↔ preset_forge | Python subprocess / IPC | Bidirectional |
| link_bridge ↔ iOS app | OSC UDP | Bidirectional |
| link_bridge ↔ all devices | Ableton Link protocol | Bidirectional |
| push_surface ↔ link_bridge | Internal Python import | Bidirectional |

---

## KPS Format (KNTKTA Preset Schema)

All presets are stored internally as KPS JSON.
See `shared/schemas/kps_schema.json` for the full schema.

Key fields:
- `preset_id`: UUID v4
- `name`: preset name (up to 64 chars)
- `plugin`: plugin metadata (name, format, uid)
- `parameters`: array of normalised parameter values (0.0–1.0)
- `push1_layout`: encoder bank assignments (up to 64 banks × 8 encoders)
- `macros`: one-to-many macro definitions
- `snapshots`: parameter state snapshots for morphing

---

## Push 1 Mode Map

| Mode | Activation | Description |
|---|---|---|
| INSTRUMENT | SHIFT + NOTE | Plugin parameter control |
| MIXER | SHIFT + MIX | Track volume/pan/sends |
| SEQUENCER | SHIFT + DEVICE | Euclidean + generative sequencer |
| SESSION | SHIFT + SESSION | Clip grid mirror |
| SAMPLE | SHIFT + CLIP | Beat chopper + resampler |
| LINK | SHIFT + BROWSE | Networked device control |
| ACTION | SHIFT + USER | Macro action pad grid |
| SONG | SHIFT + LAYOUT | Song structure navigation |
| CUE | SHIFT + IN | Headphone cue routing |

---

## AI Tool Reference

All 10 MCP tools exposed to the local LLM:

| Tool | Purpose |
|---|---|
| `map_parameter` | Map parameter → encoder/bank |
| `suggest_layout` | Auto-generate Push 1 layout |
| `set_tempo` | Set BPM |
| `launch_scene` | Launch Ableton scene |
| `set_fx_param` | Set device parameter by path |
| `create_bank` | Create preset bank |
| `quick_save` | Save preset to bank |
| `set_sequencer` | Modify Euclidean sequencer |
| `song_section` | Song structure control |
| `resample` | Trigger chromatic resampler |

---

## Ableton Link Integration

KNTKTA uses the Ableton Link C++ library for tempo/phase sync.
The `link_bridge` module wraps it in Python via ctypes.

When no native library is found, it runs in simulation mode.

Supported sync modes:
1. **Tempo only** — BPM sync across all devices
2. **Tempo + Phase** — BPM + beat phase (quantised clip launches)
3. **Full** — Tempo + Phase + OSC parameter sync

---

## Sample Engine Notes

### Beat Chopper
- Max 127 slices (MIDI notes 36–162)
- Transient detection: energy-ratio onset algorithm
- Export: SFZ, Decent Sampler, Ableton Drum Rack (.adg), Maschine (planned)

### Chromatic Resampler
- Notes: C2 (36) – C5 (84) = 49 notes
- Sends MIDI note-on → records audio → note-off, per note
- Pre-roll configurable (default 100ms)
- Exports same instrument formats as Beat Chopper

---

## Euclidean Sequencer

Uses Bjorklund's algorithm for rhythm generation.
Per-track settings: steps (1–16), hits, rotation, pitch, velocity.

Generative algorithms available:
- `euclidean` — Bjorklund (evenly distributed)
- `random_walk` — state-flip Markov
- `probability` — per-step probability gate
- `markov` — 2-state transition model

AI can modify any of these via the `set_sequencer` MCP tool.
