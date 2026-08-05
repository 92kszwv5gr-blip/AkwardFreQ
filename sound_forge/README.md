# sound_forge — Sample Engine

Two-mode intelligent sampler for KNTKTA.

## Mode A — Beat Chopper (1–127 samples)
- Record audio from any source
- Auto-slice by transient, grid, or manual chop points
- Each slice → MIDI note (36–162 range)
- Auto-generates instrument presets for: NI Maschine, Ableton Drum Rack, Ableton Simpler, Decent Sampler, SFZ

## Mode B — Chromatic Resampler (inspired by Tom Cosm's External Sampler)
- Sends MIDI notes C2–C5 to target instrument one by one
- Records each note's audio output at user-defined length
- Builds a multi-sampled instrument automatically
- Exports to: Ableton Simpler, Drum Rack, Decent Sampler, SFZ, Maschine

## Structure

```
sound_forge/
├── chopper/
│   ├── audio_recorder.py     # Audio I/O via sounddevice / PortAudio
│   ├── transient_detector.py # Onset detection for auto-chop
│   ├── beat_chopper.py       # Main chop engine
│   └── export/
│       ├── sfz_exporter.py
│       ├── decent_sampler_exporter.py
│       ├── ableton_drum_rack_exporter.py
│       └── maschine_exporter.py
├── resampler/
│   ├── chromatic_resampler.py # Main resample engine
│   └── midi_note_sender.py    # MIDI note trigger with timing
└── tests/
```
