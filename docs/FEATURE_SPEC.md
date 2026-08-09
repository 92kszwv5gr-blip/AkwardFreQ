# Layer Classification Feature Spec

This is the contract between the C++ heuristic classifiers (`Source/separation/AnalysisUtils.*`,
`LayerClassifier.*`) and the offline Python retraining script
(`tools/retrain_layer_classifier.py`). Both sides must extract **the same 12-dimensional
feature vector** in **the same order** from an audio segment, otherwise a retrained ONNX
classifier will misinterpret the plugin's features (and vice versa).

If you change this vector, change it in both places and bump `kFeatureSpecVersion`
in `Source/separation/LayerClassifier.h`.

| Index | Name              | Description                                                              | Units / Range |
|-------|-------------------|---------------------------------------------------------------------------|----------------|
| 0     | spectralCentroid  | Magnitude-weighted mean FFT bin frequency                                | Hz, normalized 0-1 by Nyquist |
| 1     | spectralFlatness  | Geometric mean / arithmetic mean of magnitude spectrum                  | 0 (tonal) - 1 (noisy) |
| 2     | spectralFlux      | L2 norm of positive magnitude change vs previous frame                  | normalized 0-1 |
| 3     | zeroCrossingRate  | Fraction of sample-to-sample sign changes                               | 0-1 |
| 4     | rms               | Root-mean-square level of the segment                                   | linear amplitude 0-1 |
| 5     | attackTimeMs      | Time from onset to 90% of segment peak                                  | milliseconds, capped 0-200 |
| 6     | decayTimeMs       | Time from peak to -20dB below peak                                      | milliseconds, capped 0-1000 |
| 7     | harmonicity       | Normalized strength of the strongest autocorrelation peak               | 0-1 |
| 8     | bandEnergyLow     | Fraction of total energy below 150 Hz                                   | 0-1 |
| 9     | bandEnergyMid     | Fraction of total energy between 150 Hz and 6 kHz                       | 0-1 |
| 10    | bandEnergyHigh    | Fraction of total energy above 6 kHz                                    | 0-1 |
| 11    | pitchConfidence   | Autocorrelation peak clarity in the 50-2000 Hz pitch search range       | 0-1 |

Analysis window: 40ms frames, 50% overlap, Hann window, computed at the plugin's
current sample rate then resampled features are inherently rate-independent since
everything is normalized (Hz values divided by Nyquist, ms values are wall-clock).

## Label set (`LayerType`, `Source/separation/RegionTypes.h`)

```
0 Kick, 1 Bass, 2 HiHat, 3 Percussion, 4 Breakbeat,
5 SynthLead, 6 Stabs, 7 Atmosphere, 8 FX, 9 Zap, 10 Glitch, 11 Unclassified
```

`Bass` regions come from Tier A (Demucs `bass` stem, energy-gated into regions) and are
never produced by the Tier B/C classifiers — it's included in the enum purely so the
correction UI and sample-pack exporter can treat all layers uniformly.

## Model contract

A user-retrained classifier is an ONNX model with:
- Input: `float32[N, 12]` (batch of feature vectors, spec above)
- Output: `float32[N, 11]` (class logits over Kick..Glitch, i.e. `Unclassified` excluded —
  the plugin falls back to `Unclassified` itself when the top logit is below
  `minClassifierConfidence`, see `LayerClassifier::classify`)

`tools/retrain_layer_classifier.py` produces exactly this shape via `skl2onnx`.
