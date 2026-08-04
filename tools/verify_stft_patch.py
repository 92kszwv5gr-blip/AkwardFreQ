"""
Sanity check for _stft_onnx_patch.py: runs the SAME pretrained htdemucs model
and the SAME random audio through both the original (unpatched) forward pass
and the patched (ONNX-exportable) one, and asserts they agree to float32
tolerance. Run this before ever trusting an ONNX export of the patched model.
"""
import copy

import torch
from demucs.pretrained import get_model

torch.manual_seed(0)

print("Loading pretrained htdemucs...")
bag = get_model("htdemucs")
model = bag.models[0]
model.eval()

# Keep an unpatched deep copy to compare against, since the patch monkeypatches
# the HTDemucs *class* (affecting all instances, including this one otherwise).
reference_state = copy.deepcopy(model.state_dict())

SEGMENT_SAMPLES = 343980
x = torch.randn(1, 2, SEGMENT_SAMPLES) * 0.1  # quiet-ish, like real audio

with torch.no_grad():
    ref_out = model(x)
print("reference output shape:", ref_out.shape)

import _stft_onnx_patch
_stft_onnx_patch.apply()

model.load_state_dict(reference_state)  # unchanged, just re-confirming same weights
with torch.no_grad():
    patched_out = model(x)
print("patched output shape:", patched_out.shape)

assert ref_out.shape == patched_out.shape, (ref_out.shape, patched_out.shape)
diff = (ref_out - patched_out).abs()
print(f"max abs diff: {diff.max().item():.6e}")
print(f"mean abs diff: {diff.mean().item():.6e}")
print(f"reference output abs mean (for scale): {ref_out.abs().mean().item():.6e}")

tol = 1e-3  # generous but meaningful given the model has ~50 conv/attention layers to accumulate float32 error through
assert diff.max().item() < tol, f"Patched model diverges from the original by more than {tol} — DO NOT trust an ONNX export built from this patch."
print(f"OK — patched model matches the original to within {tol}.")
