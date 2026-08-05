# KNTKTA — Akai Push 1 · Universal Controller Intelligence Platform

> **K**o**N**trol · **T**ransform · **K**reate · **T**une · **A**utomate

KNTKTA is an open, modular platform that turns the Akai Push 1 into a universal instrument controller — working seamlessly across Ableton Live, standalone VSTs, iOS apps, Traktor, and any Ableton Link-enabled device on your network.

---

## Modules

| # | Module | Description |
|---|---|---|
| 1 | [`push_surface/`](push_surface/) | Universal Remote Script Engine — adaptive Push 1 controller surface |
| 2 | [`preset_forge/`](preset_forge/) | Format-agnostic Preset Manager — converts .fxb, .nki, .adv, .fxp and more |
| 3 | [`kntkta_core/`](kntkta_core/) | Main App container — Binary, VST3, AU, CLAP plugin |
| 4 | [`kntkta_ai/`](kntkta_ai/) | Embedded MCP AI Assistant — local AI mapping co-pilot |
| 5 | [`sound_forge/`](sound_forge/) | Sample Engine — Beat Chopper + Chromatic Resampler |
| 6 | [`link_bridge/`](link_bridge/) | Ableton Link + iOS/Network Layer — wireless parameter universe |

---

## Feature Highlights

- **Adaptive Push 1 surface** — automatically maps to any plugin, DAW, or networked device
- **Universal preset conversion** — import any format, convert to KPS (KNTKTA Preset Schema), export back
- **Ableton Link integration** — sync and control iOS apps, Traktor, and other DAWs wirelessly
- **Embedded AI assistant** — voice or text control of your mappings via local MCP server
- **Beat Chopper** — 1–127 sample slices auto-generating Drum Rack, Maschine, Decent Sampler presets
- **Chromatic Resampler** — capture synth/bass at every MIDI note C2–C5, auto-build multi-sampled instruments
- **Euclidean Sequencer** — per-track Euclidean rhythm generation on the pad grid
- **One-to-Many Macro Engine** — 1 encoder → up to 16 parameters with custom curves
- **Song Structure Manager** — define Intro/Verse/Drop sections, trigger across all Link devices
- **MPE Emulation** — polyphonic expression from Push 1 pads to any MPE-capable synth

---

## Tech Stack

| Layer | Technology |
|---|---|
| Remote Script | Python 3, Ableton `ableton.v2` framework |
| Core App / VST | C++ with JUCE 7 |
| Standalone GUI | Qt6 / Electron |
| Database | SQLite |
| AI runtime | Ollama + Mistral/Phi-3, MCP over stdio |
| Voice input | OpenAI Whisper (local) |
| Ableton Link | `link` C++ library (Ableton/link) |
| Network MIDI | RTP-MIDI |
| OSC | liblo / oscpack |
| iOS Companion | Swift + SwiftUI + AudioKit |
| Bluetooth MIDI | CoreBluetooth / WinRT |

---

## Build Order

1. `push_surface` — Push 1 ↔ Ableton Remote Script
2. `link_bridge` — Ableton Link sync (tempo → parameter sync)
3. `preset_forge` — KPS schema + format parsers
4. `kntkta_core` — Main app GUI wiring
5. `sound_forge` Mode A — Beat chopper
6. `kntkta_ai` — MCP AI layer
7. `sound_forge` Mode B — Chromatic resampler
8. Advanced features (Euclidean, Generative, Song Mode, Macro Engine)
9. VST/AU/CLAP packaging

---

## License

MIT — see [LICENSE](LICENSE)