# push_surface — Universal Remote Script Engine

Adaptive Ableton Live Remote Script for the Akai Push 1.  
Auto-maps parameters for any loaded plugin, with AI-assisted page layout.

## Features

- Adaptive parameter mapping per plugin/instrument
- 8 encoder pages per plugin, up to 64 banks (via Module 2)
- Push 1 LCD display driver (2×28 chars)
- Mode system: INSTRUMENT, MIXER, SAMPLE, SEQUENCER, ACTION, SONG, LINK
- Euclidean Sequencer mode (8 tracks, pad grid)
- Generative / Algorithmic Sequencer mode
- Session Clip Grid (mirrors Ableton session view)
- MPE emulation layer
- Cue monitoring mode
- One-to-Many Macro engine
- Song/Set Structure Manager
- ClyphX-style Macro Action System
- Integration with `link_bridge` for networked device parameter reading
- Integration with `kntkta_ai` for AI-assisted mapping

## Installation

Copy the `push_surface/` folder to your Ableton Live MIDI Remote Scripts directory:

- **macOS**: `~/Library/Preferences/Ableton/Live x.x.x/User Remote Scripts/`
- **Windows**: `%APPDATA%\Ableton\Live x.x.x\Preferences\User Remote Scripts\`

Then select `push_surface` in Ableton → Preferences → Link/MIDI → Control Surface.

## Structure

```
push_surface/
├── __init__.py              # Entry point
├── push1_surface.py         # Main ControlSurface class
├── push1_hardware.py        # SysEx / MIDI hardware constants
├── parameter_mapper.py      # Adaptive parameter mapping engine
├── lcd_display.py           # Push 1 2-line LCD driver
├── macro_engine.py          # One-to-many macro system
├── action_system.py         # ClyphX-style action scripting
├── song_structure.py        # Song/Set Structure Manager
├── modes/
│   ├── __init__.py
│   ├── instrument_mode.py   # Plugin parameter control
│   ├── mixer_mode.py        # Track mixer control
│   ├── sequencer_mode.py    # Euclidean + generative sequencer
│   ├── session_mode.py      # Session clip grid
│   ├── sample_mode.py       # Sample engine bridge
│   ├── link_mode.py         # Networked device control
│   ├── action_mode.py       # Macro action pad grid
│   ├── song_mode.py         # Song structure navigation
│   └── cue_mode.py          # Cue monitoring
└── tests/
    └── test_parameter_mapper.py
```
