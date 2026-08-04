"""
Retrains AkwardFreQ's Tier D layer classifier from correction data saved by
the plugin (RegionListPanel's "Save Corrections for Retraining" button),
which lands in TrainingData/*.json next to the plugin's Models/ folder.

No audio processing happens here — every saved region already carries its
12-value feature vector (docs/FEATURE_SPEC.md), extracted by the same C++
code that runs live in the plugin (Source/separation/AnalysisUtils.cpp), so
this script just trains a classifier directly on those feature vectors.

Usage:
    pip install -r requirements.txt
    python retrain_layer_classifier.py \
        --training-data ../TrainingData \
        --output ../Models/UserTrained/layer_classifier.onnx

Output contract (must match LayerClassifier.h's OnnxModel):
    input:  float32[N, 12]   (see docs/FEATURE_SPEC.md)
    output: float32[N, 11]   (probabilities over Kick..Glitch, Unclassified excluded)

Re-run this any time TrainingData/ has grown — it's a full retrain from
scratch each time, not incremental, and only ever runs offline on your
machine. The plugin picks up the new .onnx the next time it loads (restart
Ableton or reload the plugin).
"""
import argparse
import glob
import json
import os

import numpy as np
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import train_test_split
from skl2onnx import convert_sklearn
from skl2onnx.common.data_types import FloatTensorType

NUM_FEATURES = 12
NUM_CLASSES = 11  # Kick..Glitch — see docs/FEATURE_SPEC.md; Unclassified (index 11) is excluded


def load_training_data(training_data_dir: str):
    features, labels = [], []
    files = sorted(glob.glob(os.path.join(training_data_dir, "*.json")))
    if not files:
        raise SystemExit(f"No training data found in {training_data_dir} — "
                          f"use the plugin's 'Save Corrections for Retraining' button first.")

    for path in files:
        with open(path) as f:
            data = json.load(f)
        for region in data.get("regions", []):
            feats = region.get("features")
            label_idx = region.get("labelIndex")
            if feats is None or label_idx is None or len(feats) != NUM_FEATURES:
                continue
            if not (0 <= label_idx < NUM_CLASSES):
                continue  # Unclassified or out of range — skip
            features.append(feats)
            labels.append(label_idx)

    print(f"Loaded {len(features)} labeled regions from {len(files)} file(s).")
    return np.array(features, dtype=np.float32), np.array(labels, dtype=np.int64)


def pad_missing_classes(X, y):
    """LayerClassifier.cpp reads a fixed-width float32[N,11] output tensor
    unconditionally. sklearn's ONNX export orders probabilities by
    clf.classes_, which only contains labels actually seen during fit() — if
    your correction data hasn't covered all 11 layers yet, that tensor would
    come out narrower than 11 and the C++ side would read past its end.

    Fix: append one synthetic, near-zero-weight sample per missing class so
    classes_ always ends up as exactly [0..10], without letting those
    synthetic samples meaningfully influence the trained decision boundaries."""
    present = set(np.unique(y))
    missing = [c for c in range(NUM_CLASSES) if c not in present]
    weights = np.ones (len(y), dtype=np.float64)

    if missing:
        print(f"Note: no examples yet for class indices {missing} — padding with "
              f"negligible-weight placeholders so the exported model still has the "
              f"required fixed output width. Add real corrected examples for these "
              f"layers when you can.")
        neutral_row = X.mean(axis=0) if len(X) > 0 else np.zeros(NUM_FEATURES, dtype=np.float32)
        pad_X = np.tile(neutral_row, (len(missing), 1)).astype(np.float32)
        pad_y = np.array(missing, dtype=np.int64)
        pad_w = np.full(len(missing), 1e-6, dtype=np.float64)

        X = np.concatenate([X, pad_X], axis=0)
        y = np.concatenate([y, pad_y], axis=0)
        weights = np.concatenate([weights, pad_w], axis=0)

    return X, y, weights


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--training-data", default="../TrainingData")
    parser.add_argument("--output", default="../Models/UserTrained/layer_classifier.onnx")
    parser.add_argument("--n-estimators", type=int, default=200)
    args = parser.parse_args()

    X, y = load_training_data(args.training_data)

    can_stratify = len(np.unique(y)) > 1 and np.min(np.bincount(y)) >= 2
    if len(X) >= 10:
        X_train, X_test, y_train, y_test = train_test_split(
            X, y, test_size=0.2, random_state=42, stratify=y if can_stratify else None)
    else:
        X_train, y_train = X, y
        X_test, y_test = np.empty((0, NUM_FEATURES), dtype=np.float32), np.empty((0,), dtype=np.int64)
        print("Fewer than 10 examples total — skipping the held-out test split "
              "(training on everything). Accuracy numbers below won't be meaningful yet.")

    X_train, y_train, sample_weight = pad_missing_classes(X_train, y_train)

    clf = RandomForestClassifier(n_estimators=args.n_estimators, max_depth=12, random_state=42)
    clf.fit(X_train, y_train, sample_weight=sample_weight)

    train_acc = clf.score(X_train, y_train)
    test_acc = clf.score(X_test, y_test) if len(X_test) > 0 else float("nan")
    print(f"Train accuracy: {train_acc:.3f}   Held-out accuracy: {test_acc:.3f}")
    if len(X) < 200:
        print("Small dataset — treat these numbers as a rough signal, not a reliable "
              "estimate. Keep correcting tracks in the plugin and re-running this "
              "script as TrainingData/ grows.")

    os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)
    onnx_model = convert_sklearn(
        clf,
        initial_types=[("input", FloatTensorType([None, NUM_FEATURES]))],
        options={id(clf): {"zipmap": False}},  # plain array output, not a list of dicts
        target_opset=17,
    )

    # skl2onnx emits two outputs for a classifier: predicted label, then class
    # probabilities. LayerClassifier.cpp always reads output index 0 (see
    # OnnxModel::load's GetOutputNameAllocated(0, ...)), so drop the label
    # output and keep only the fixed-width [N,11] probability tensor at index 0.
    del onnx_model.graph.output[0]
    onnx_model.graph.output[0].name = "probabilities"

    with open(args.output, "wb") as f:
        f.write(onnx_model.SerializeToString())

    print(f"Wrote {args.output}.")
    print("Restart Ableton (or reload the plugin) to pick it up — AkwardFreQ loads "
          "Models/UserTrained/layer_classifier.onnx automatically if present, falling "
          "back to the built-in rule-based heuristics otherwise.")


if __name__ == "__main__":
    main()
