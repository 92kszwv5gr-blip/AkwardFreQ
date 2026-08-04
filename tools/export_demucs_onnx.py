"""
Exports Meta's pretrained HT-Demucs model to ONNX for AkwardFreQ's Tier A stem
separation (Source/separation/DemucsEngine.cpp).

This is inference-only re-packaging — it downloads Demucs' own pretrained
weights and converts them to a format the C++ plugin can run via ONNX
Runtime. No training happens here or anywhere in the plugin.

Usage:
    pip install -r requirements.txt
    python export_demucs_onnx.py --output ../Models/htdemucs.onnx

Output contract (must match DemucsEngine.h/.cpp):
    input:  float32[1, 2, 343980]      (~7.8s of stereo audio @ 44100Hz)
    output: float32[1, 4, 2, 343980]   (sources, in order: drums, bass, other, vocals)
"""
import argparse
import os

import torch
from demucs.pretrained import get_model

SEGMENT_SAMPLES = 343980  # must match DemucsEngine::kSegmentSamples


class FixedShapeWrapper(torch.nn.Module):
    """Plain tensor-in/tensor-out forward pass for ONNX export — Demucs' own
    API carries extra metadata the plugin doesn't need."""

    def __init__(self, model):
        super().__init__()
        self.model = model

    def forward(self, mix: torch.Tensor) -> torch.Tensor:
        return self.model(mix)  # [batch, channels, samples] -> [batch, sources, channels, samples]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--output", default="../Models/htdemucs.onnx",
                         help="Output .onnx path (default: ../Models/htdemucs.onnx)")
    parser.add_argument("--opset", type=int, default=17)
    args = parser.parse_args()

    print("Loading pretrained htdemucs (downloads weights on first run, ~80-160MB)...")
    model = get_model("htdemucs")
    model.eval()

    print(f"Model source order: {model.sources}")
    assert list(model.sources) == ["drums", "bass", "other", "vocals"], (
        "This export assumes Demucs' standard source order [drums, bass, other, vocals]. "
        "If a different pretrained variant produced a different order, update the "
        "documented order in Source/separation/DemucsEngine.h to match model.sources "
        "above, or the plugin will mislabel entire stems."
    )

    wrapped = FixedShapeWrapper(model)
    dummy_input = torch.zeros(1, 2, SEGMENT_SAMPLES)

    os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)
    print(f"Exporting to {args.output} (opset {args.opset})... this can take a minute.")
    with torch.no_grad():
        torch.onnx.export(
            wrapped,
            dummy_input,
            args.output,
            input_names=["mix"],
            output_names=["stems"],
            opset_version=args.opset,
            do_constant_folding=True,
            # Fixed shapes on purpose: DemucsEngine.cpp always feeds exactly
            # kSegmentSamples per chunk (zero-padding the final chunk), so
            # there's no need for dynamic axes here.
        )

    print(f"Done. Copy '{args.output}' into the plugin's Models/ folder "
          "(next to the built .vst3, or wherever PluginProcessor::getModelsDirectory() "
          "resolves to) so AkwardFreQ can find it at startup.")


if __name__ == "__main__":
    main()
