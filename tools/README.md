# tools/

Offline Python scripts. None of this runs inside the plugin — the plugin only
ever does ONNX Runtime *inference*, never training or model export. These
scripts are what you run on your own machine, separately, to produce the
`.onnx` files the plugin loads.

```
pip install -r requirements.txt
```

## export_demucs_onnx.py

Run once (or whenever you want to swap in a different Demucs variant).
Downloads Meta's pretrained HT-Demucs weights and re-packages them as
`Models/htdemucs.onnx` for Tier A stem separation.

```
python export_demucs_onnx.py --output ../Models/htdemucs.onnx
```

## retrain_layer_classifier.py

Run after you've corrected some regions in the plugin and clicked "Save
Corrections for Retraining" a few times (across a handful of tracks — one
track's worth of corrections isn't much signal). Trains a small classifier on
the feature vectors the plugin already extracted and exports it as
`Models/UserTrained/layer_classifier.onnx`, which the plugin picks up
automatically on its next load.

```
python retrain_layer_classifier.py --training-data ../TrainingData --output ../Models/UserTrained/layer_classifier.onnx
```

Safe to re-run any time — it's a full retrain from your current
`TrainingData/*.json` files each time, not incremental. Delete a `.json` file
from `TrainingData/` if a track's corrections turned out wrong and you don't
want them influencing the next retrain.
