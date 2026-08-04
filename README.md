# AkwardFreQ

A VST3 plugin for Ableton (Windows) that splits a psytrance-family track into
genre-specific layers (kick, bass, hi-hats, percussion, breakbeat, synth
lead, stabs, atmospheres, fx, zaps, glitches), applies a real mastering
chain, and exports the isolated layers as a tagged sample pack — with an
in-plugin correction UI so misclassified layers can be fixed and fed back
into retraining.

## What this actually is (read before building)

No pretrained model anywhere splits audio into 11 psytrance-specific layers —
that granularity doesn't exist as a public checkpoint. So this is a two-tier
hybrid, not a single neural net:

- **Tier A — HT-Demucs (pretrained, inference only)**: splits the mix into
  `drums` / `bass` / `other` stems. This part is genuinely reliable.
- **Tier B/C — DSP heuristics**: onset detection, spectral/envelope feature
  extraction, and rule-based scoring sub-divide the `drums` bus into
  Kick/HiHat/Percussion/Breakbeat and the `other` bus into
  SynthLead/Stabs/Atmosphere/FX/Zap/Glitch. These are acoustic-behavior
  heuristics, not genre knowledge — stabs/zaps/glitches/fx in particular
  overlap acoustically and *will* be misclassified sometimes. That's expected,
  not a bug to chase.
- **Tier D — your corrections**: the Split tab's region list lets you
  reassign any layer; `TrainingDataExporter` saves your corrected
  feature+label pairs, and `tools/retrain_layer_classifier.py` trains a small
  classifier on them, offline, on your own machine. Drop the resulting
  `Models/UserTrained/layer_classifier.onnx` in and the plugin uses it in
  place of the built-in heuristics automatically.

Mastering (multiband compression, reference-track EQ matching, loudness trim,
limiting) is real DSP, not ML. Sample pack export slices actual audio out of
the isolated layers — it is **not** generative sample synthesis; it can't
invent new sounds, only tag and organize what's already in your imported
tracks.

See `docs/FEATURE_SPEC.md` for the exact feature vector contract shared
between the C++ classifier and the Python retraining script.

### One-shot instruments, drum chopping, and MIDI (Instrument / Drum Chop / MIDI tabs)

- **Instrument tab**: exports whatever region is currently selected in the
  Split tab as a standalone one-shot — SFZ (open format, works in Kontakt via
  its SFZ importer, Decent Sampler, sforzando, etc.) and/or an Ableton
  Simpler preset (`.adv`). Native Instruments' own `.nki` format is encrypted
  and undocumented — there's no legitimate way to write one, which is why SFZ
  is the primary format here rather than a Kontakt-native file.
- **Drum Chop tab**: chops a drum layer (or the whole unsplit `drums` bus)
  into hits via onset detection and exports them as named, prefixed `.wav`
  files, ready to drag into an Ableton Drum Rack yourself. There's no
  one-click `.adg` Drum Rack generator — see "Ableton preset export" below
  for why.
- **MIDI tab**: pick a range on the Split tab's waveform (toggle "Pick Range
  on Waveform"), optionally snap it to a clean 2/4/8/16-bar loop, use "Loop
  Preview" to hear it repeat before committing, then transcribe it to a
  Standard MIDI File. Transcription is monophonic pitch-tracking — genuinely
  useful on a synth lead or bass layer, but it will **not** produce a real
  chord transcription from a stab or atmosphere layer (it'll output one
  wandering note per onset instead). Pick a monophonic layer as the source.

#### Ableton preset export (`.adv` Simpler) is best-effort

Ableton's `.adv`/`.adg` format is undocumented, gzip-compressed XML.
`AbletonPresetWriter` doesn't generate this from scratch (too easy to get
subtly wrong and produce a file Ableton silently refuses to load) — it
*patches* a real preset you export from Ableton once. See
`Models/Templates/README.md` for the one-time setup. Without a template,
Simpler export fails with a clear error; SFZ and folder export always work
regardless. Drum Rack (`.adg`) export isn't implemented at all yet for the
same reason, at a scale where guessing wrong is more likely — folder export
of chopped hits is the reliable path there.

## Building (Windows)

This was developed without a Windows machine or Ableton available to test
against, so treat the first build as the real integration test — file an
issue / fix forward if CMake or JUCE's API has moved since.

### Prerequisites

- Visual Studio 2022 (Desktop development with C++ workload)
- CMake ≥ 3.22 (Visual Studio's bundled CMake works)
- Git
- Python 3.10+ (for the offline model-export tooling, not for the plugin build)

### 1. Get ONNX Runtime

Download a prebuilt Windows release from
https://github.com/microsoft/onnxruntime/releases — grab the
`onnxruntime-win-x64-<version>.zip` asset (CPU build is fine) and extract it
somewhere, e.g. `C:\onnxruntime`.

### 2. Export the Demucs model

```
cd tools
pip install -r requirements.txt
python export_demucs_onnx.py --output ../Models/htdemucs.onnx
```

This downloads Meta's pretrained HT-Demucs weights (~100MB) and converts them
to `Models/htdemucs.onnx`. Without this file the plugin builds and loads
fine, but stem separation reports "Demucs model not found."

### 3. Configure and build

```
cmake -B build -DONNXRUNTIME_ROOT_DIR="C:/onnxruntime"
cmake --build build --config Release
```

The first configure fetches JUCE from GitHub (via `FetchContent`) — expect it
to take a few minutes.

With `COPY_PLUGIN_AFTER_BUILD` on (default in `CMakeLists.txt`), JUCE copies
the built `.vst3` into your system's VST3 folder
(`%COMMONPROGRAMFILES%\VST3`) automatically. Copy `Models/htdemucs.onnx` (and
`Models/UserTrained/layer_classifier.onnx` if you have one) into that same
`AkwardFreQ.vst3` folder's `Contents/x86_64-win` directory alongside the
plugin binary — that's where `PluginProcessor::getModelsDirectory()` looks
(next to `juce::File::currentExecutableFile`).

Rescan plugins in Ableton (Preferences → Plug-ins → Rescan) and AkwardFreQ
should show up under VST3.

### Known limitation: MP3 import

JUCE's bundled `AudioFormatManager::registerBasicFormats()` covers WAV, AIFF,
FLAC, and OGG — it does not include an MP3 *decoder* (JUCE only ships an MP3
encoder in some paid tiers). Importing an `.mp3` via the Split tab's "Import
Track" will fail with a clear error; convert to WAV first, or capture the
track live from Ableton's transport instead (the "Start Capture" button),
which works regardless of source format since it just records the plugin's
live audio input.

## Project layout

```
Source/
  PluginProcessor.*        JUCE AudioProcessor: capture/import, orchestrates
                            SeparationEngine + MasteringChain, real-time safe
                            processBlock (preview mix + mastering only —
                            separation always runs on a background thread)
  PluginEditor.*            Split / Master / Export / Instrument / Drum Chop / MIDI tabs
  separation/
    DemucsEngine.*           Tier A — ONNX Runtime inference, chunked overlap-add
    AnalysisUtils.*          onset detection, spectral/pitch features (docs/FEATURE_SPEC.md)
    LayerClassifier.*        Tier B/C/D scoring — rule-based, or a loaded user ONNX model
    DrumSubSplitter.*        Tier B — kick/hihat/percussion/breakbeat
    OtherSubSplitter.*       Tier C — synth lead/stabs/atmosphere/fx/zap/glitch
    GenreBias.*              per-genre-preset score nudging
    LayerRenderer.*          mask-based isolated-layer audio reconstruction
    SeparationEngine.*       orchestrates A -> B/C -> D on a background thread
    TrainingDataExporter.*   writes corrected regions for offline retraining
    DrumSlicer.*             onset-based chopping of a drum buffer/range into hits
  mastering/
    LoudnessMeter.*          approximate BS.1770-style loudness measurement
    MasteringChain.*         multiband comp, reference EQ match, limiter
  export/
    SamplePackExporter.*     slices tagged regions into a folder-organized .wav pack
    WavFileWriter.*          shared "write this sample range as .wav" helper
    SfzExporter.*            one-shot -> SFZ instrument (open format)
    AbletonPresetWriter.*    one-shot -> Ableton Simpler .adv (best-effort, patches a template)
    DrumRackExporter.*       chopped drum hits -> named/prefixed .wav folder
  midi/
    AudioToMidiConverter.*   monophonic pitch-tracking transcription -> Standard MIDI File
    LoopSnapper.*            bar-grid + waveform-continuity loop point search
  ui/                        WaveformRegionView, RegionListPanel (correction UI),
                              MasteringPanel, ExportPanel, InstrumentExportPanel,
                              DrumRackPanel, MidiPanel
tools/                       offline Python — model export + retraining (not built into the plugin)
docs/FEATURE_SPEC.md         C++ <-> Python feature vector contract
Models/                      .onnx files go here (gitignored — see Models/README.md)
Models/Templates/            Ableton preset templates for AbletonPresetWriter (see its README)
```
