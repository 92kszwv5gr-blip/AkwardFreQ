# Models/

Model weights are not checked into git (they're large binaries and, for
Demucs, redistributable-but-large). This folder is where the plugin looks for
them at startup — see `PluginProcessor::getModelsDirectory()`, which resolves
to the folder next to the built plugin binary.

Expected layout after setup:

```
Models/
  htdemucs.onnx                       # Tier A — required. See tools/export_demucs_onnx.py
  UserTrained/
    layer_classifier.onnx             # Tier D — optional. See tools/retrain_layer_classifier.py
```

Without `htdemucs.onnx`, stem separation is unavailable and the plugin will
report "Demucs model not found" in the Split tab's status line — mastering
and manual layer preview still work once you have *some* separation result to
audition, but there's no way to produce one without the model.

Without `UserTrained/layer_classifier.onnx`, the Tier B/C sub-splitters use
the built-in rule-based heuristics (`LayerClassifier::classifyRuleBased`) —
this is the normal, expected state for a fresh install.
