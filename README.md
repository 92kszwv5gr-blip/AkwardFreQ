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
  PluginEditor.*            Split / Master / Export tabs
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
  mastering/
    LoudnessMeter.*          approximate BS.1770-style loudness measurement
    MasteringChain.*         multiband comp, reference EQ match, limiter
  export/
    SamplePackExporter.*     slices tagged regions into a folder-organized .wav pack
  ui/                        WaveformRegionView, RegionListPanel (correction UI),
                              MasteringPanel, ExportPanel
tools/                       offline Python — model export + retraining (not built into the plugin)
docs/FEATURE_SPEC.md         C++ <-> Python feature vector contract
Models/                      .onnx files go here (gitignored — see Models/README.md)
```
