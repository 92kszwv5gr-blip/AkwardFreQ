# Onset detector benchmark (Groove MIDI Dataset)

Run: `cmake -B build-bench -DAFQ_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release ...`, build `AkwardFreQBenchmark`, then
`AkwardFreQBenchmark <dataset folder> [--limit N]`. Dataset: Google Magenta's Groove MIDI Dataset (CC BY 4.0), 1,090 audio
files, 10.9 h. Ground truth is the MIDI note-on times (hits within 30 ms merged); a detection is correct within 50 ms of an
unmatched hit (MIREX rule). Run on 2026-09-29 at commit `df43919`.

| Settings | Precision | Recall | F1 | Detected / true |
| --- | --- | --- | --- | --- |
| current defaults (sensitivity 6, floor 0.15) | 0.659 | 0.544 | 0.596 | 0.83 |
| old defaults (sensitivity 1.5, no floor) | 0.302 | 0.842 | 0.445 | 2.78 |
| no floor | 0.505 | 0.636 | 0.563 | 1.26 |
| sensitivity 3 / 10 | 0.594 / 0.722 | 0.575 / 0.484 | 0.584 / 0.580 | 0.97 / 0.67 |
| floor 0.05 / 0.30 | 0.526 / 0.911 | 0.627 / 0.398 | 0.573 / 0.554 | 1.19 / 0.44 |

Onsets sit at the start of the analysis frame: median 11.5 ms before the hit (5th..95th percentile -23..+8 ms).

150-file even subset (matches the full run: 0.610 vs 0.596), structural variants:

| Window, hop | Precision | Recall | F1 | Median offset |
| --- | --- | --- | --- | --- |
| 1024, 256 (current) | 0.690 | 0.547 | 0.610 | -11.4 ms |
| 2048, 256 | 0.814 | 0.528 | 0.640 | -27.1 ms |
| 1024, 128 | 0.585 | 0.555 | 0.569 | -13.2 ms |
| 512, 128 | 0.523 | 0.565 | 0.543 | -5.3 ms |

Reading: the fix removed the over-triggering (2.78 to 0.83 onsets per true hit) but recall is only 54%, and no constant in
the sweep beats F1 ~0.60. Defaults were not changed. Next: a mel-band log-spectrogram flux with a maximum filter across
bands (SuperFlux), then rerun. Not checked: how the audio was produced; a per-instrument breakdown.
