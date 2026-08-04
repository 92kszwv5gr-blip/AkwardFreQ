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

import _stft_onnx_patch

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
    bag = get_model("htdemucs")

    print(f"Model source order: {bag.sources}")
    assert list(bag.sources) == ["drums", "bass", "other", "vocals"], (
        "This export assumes Demucs' standard source order [drums, bass, other, vocals]. "
        "If a different pretrained variant produced a different order, update the "
        "documented order in Source/separation/DemucsEngine.h to match model.sources "
        "above, or the plugin will mislabel entire stems."
    )

    # get_model() always returns a BagOfModels, even for a single checkpoint —
    # its forward() deliberately raises NotImplementedError ("call apply_model
    # on this") since normal inference goes through demucs.apply.apply_model,
    # which does cross-fade windowing over arbitrary-length audio. That's not
    # ONNX-exportable as-is, but "htdemucs" (unlike "htdemucs_ft") is a bag of
    # exactly one checkpoint, so unwrapping to that single model and exporting
    # it directly is the same computation, just without apply_model's chunking
    # machinery — which DemucsEngine.cpp already reimplements in C++ (chunked
    # overlap-add, see kSegmentSamples above).
    assert len(bag.models) == 1, (
        f"Expected a single-checkpoint bag for 'htdemucs', got {len(bag.models)} models — "
        "this export doesn't handle multi-model ensembles like 'htdemucs_ft'."
    )
    model = bag.models[0]
    model.eval()

    # HTDemucs' forward pass internally computes an STFT/ISTFT and does its
    # "complex-as-channels" masking via genuine complex-dtype tensors
    # (torch.stft(..., return_complex=True), torch.view_as_real/complex) —
    # none of which PyTorch's ONNX exporter supports, at any opset, as of
    # torch 2.13. _stft_onnx_patch replaces those internals with a
    # numerically-equivalent real-tensor-only implementation (verified
    # against the unpatched model in tools/verify_stft_patch.py) that traces
    # to plain Conv1d/Fold ops instead. See that module's docstring for why
    # this is safe for htdemucs specifically (cac=True, no Wiener filtering).
    _stft_onnx_patch.apply()

    # HTDemucs' cross-transformer uses nn.MultiheadAttention, which in eval
    # mode dispatches to a fused native kernel (aten::_native_multi_head_
    # attention) that also isn't ONNX-exportable. Disabling the fast path
    # makes it fall back to the plain decomposed (linear/softmax/matmul)
    # implementation instead, which traces fine — a well-known, documented
    # PyTorch/ONNX interaction, not specific to this model.
    torch.backends.mha.set_fastpath_enabled(False)

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
            # Force the legacy TorchScript-tracing exporter rather than
            # torch>=2.5's default torch.export-based one — HTDemucs' hybrid
            # (time + spectral/STFT) architecture uses control flow and
            # complex-tensor ops that the newer, stricter symbolic-tracing
            # exporter can't yet handle, but tracing handles fine since every
            # shape here is fixed (see the "Fixed shapes on purpose" note below).
            dynamo=False,
            # Fixed shapes on purpose: DemucsEngine.cpp always feeds exactly
            # kSegmentSamples per chunk (zero-padding the final chunk), so
            # there's no need for dynamic axes here.
        )

    print(f"Done. Copy '{args.output}' into the plugin's Models/ folder "
          "(next to the built .vst3, or wherever PluginProcessor::getModelsDirectory() "
          "resolves to) so AkwardFreQ can find it at startup.")


if __name__ == "__main__":
    main()
