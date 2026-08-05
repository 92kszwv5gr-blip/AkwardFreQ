# preset_forge — Universal Preset Manager

Format-agnostic preset conversion engine for KNTKTA.

## Supported Import Formats

| Extension | Format |
|---|---|
| `.fxb` | VST Bank (all presets) |
| `.fxp` | VST Preset (single) |
| `.nki` | Native Instruments Kontakt Instrument |
| `.adv` | Ableton Instrument Rack |
| `.adg` | Ableton Device Group |
| `.vstpreset` | VST3 Preset |
| `.aupreset` | AU Preset (plist) |
| `.pchk` | Serum Preset |
| `.pst` | Massive Preset |
| `.xpf` | Omnisphere Multi |
| `.mid` | SysEx Bank dump |

## Output Format

All presets are converted to **KPS (KNTKTA Preset Schema)** — see `shared/schemas/kps_schema.json`.

## Structure

```
preset_forge/
├── forge.py              # Main PresetForge API
├── kps_manager.py        # KPS read/write + SQLite storage
├── parsers/
│   ├── __init__.py
│   ├── fxb_parser.py     # VST .fxb/.fxp
│   ├── nki_parser.py     # Kontakt .nki (XML-based)
│   ├── ableton_parser.py # .adv/.adg (gzipped XML)
│   ├── vstpreset_parser.py # VST3 .vstpreset
│   └── sysex_parser.py   # MIDI SysEx bank dumps
└── tests/
    └── test_parsers.py
```
