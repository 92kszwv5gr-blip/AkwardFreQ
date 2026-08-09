"""
Monkeypatches demucs.htdemucs.HTDemucs so its forward pass never materializes
a genuine complex-dtype tensor, which PyTorch's ONNX exporter cannot export
(neither torch.stft/istft with return_complex=True, nor torch.complex(),
nor torch.view_as_complex/view_as_real are exportable as of torch 2.13).

Scope: htdemucs's default configuration uses cac=True ("complex as channels"),
which means:
  - _spec()'s raw complex z is consumed ONLY by _magnitude() (immediately
    converted to a real tensor via view_as_real).
  - _mask()'s cac branch ignores its raw-z argument entirely and only
    operates on the real network output, converting it to complex via
    view_as_complex right at the end just to satisfy _ispec()'s input type.
  - The Wiener-filter path (_wiener, real complex arithmetic on mix_stft)
    is only reached when cac=False, so it's out of scope here.

So every complex-tensor touchpoint in the cac=True path is a real<->complex
*reinterpretation*, never actual complex arithmetic — meaning the whole
thing can be carried as a plain real tensor with an explicit trailing
(real, imag) axis instead, with _spec/_magnitude collapsed into one function
and _mask/_ispec adjusted to match. STFT/ISTFT are reimplemented via
Conv1d/Fold (equivalent to a DFT matrix multiply), which traces to plain
ONNX MatMul/Conv/Fold ops instead of the unexportable aten::stft/istft.

Verified numerically (see tools/verify_stft_patch.py) to match the
unpatched model's output to float32 tolerance before ever exporting.
"""
import math

import torch
import torch.nn.functional as F
from demucs.htdemucs import HTDemucs
from demucs.hdemucs import pad1d


def _make_stft_filters(n_fft: int):
    n_freqs = n_fft // 2 + 1
    k = torch.arange(n_freqs).unsqueeze(1).float()
    n = torch.arange(n_fft).unsqueeze(0).float()
    angle = 2 * math.pi * k * n / n_fft
    window = torch.hann_window(n_fft)
    cos_f = torch.cos(angle) * window.unsqueeze(0)
    sin_f = -torch.sin(angle) * window.unsqueeze(0)
    filters = torch.cat([cos_f, sin_f], dim=0).unsqueeze(1)  # [2*n_freqs, 1, n_fft]
    return filters, n_freqs


def _make_istft_basis(n_fft: int):
    n_freqs = n_fft // 2 + 1
    k = torch.arange(n_freqs).float()
    n = torch.arange(n_fft).float()
    angle = 2 * math.pi * torch.outer(n, k) / n_fft
    cos_b = torch.cos(angle)  # [n_fft, n_freqs]
    sin_b = torch.sin(angle)
    weights = torch.ones(n_freqs)
    weights[1:-1] = 2.0
    window = torch.hann_window(n_fft)
    return cos_b, sin_b, weights, window


_stft_filter_cache = {}
_istft_basis_cache = {}


def custom_stft_interleaved(x: torch.Tensor, n_fft: int, hop_length: int) -> torch.Tensor:
    """x: [B, C, L] real -> [B, C*2, n_freqs, T] real, (re, im) interleaved
    per input channel — exactly the layout
    torch.view_as_real(spectro(x)).permute(0,1,4,2,3).reshape(B, C*2, Fr, T)
    would produce, computed via Conv1d instead of aten::stft so it's ONNX
    exportable. Numerically equivalent to torch.stft(..., normalized=True,
    center=True, pad_mode='reflect') to float32 tolerance."""
    key = (n_fft, x.device)
    if key not in _stft_filter_cache:
        _stft_filter_cache[key] = _make_stft_filters(n_fft)
    filters, n_freqs = _stft_filter_cache[key]
    filters = filters.to(x.dtype)

    B, C, L = x.shape
    pad = n_fft // 2
    xp = F.pad(x.reshape(B * C, 1, L), (pad, pad), mode="reflect")
    out = F.conv1d(xp, filters, stride=hop_length)  # [B*C, 2*n_freqs, T]
    out = out / math.sqrt(n_fft)
    T = out.shape[-1]
    # [B*C, 2*n_freqs, T] -> [B,C,2,Fr,T] (re=idx0, im=idx1, per the filter
    # concat order above) -> [B, C*2, Fr, T] with (re,im) interleaved per
    # channel, matching view_as_real(z).permute(0,1,4,2,3).reshape(B,C*2,Fr,T).
    out = out.reshape(B, C, 2, n_freqs, T).reshape(B, C * 2, n_freqs, T)
    return out


def custom_ispec_from_trailing2(z: torch.Tensor, n_fft: int, hop_length: int, length: int) -> torch.Tensor:
    """z: [..., n_freqs, T, 2] real (re=z[...,0], im=z[...,1]) -> [..., length]
    real, via Fold-based overlap-add. Numerically equivalent to
    torch.istft(..., normalized=True, center=True) to float32 tolerance."""
    key = (n_fft, z.device)
    if key not in _istft_basis_cache:
        _istft_basis_cache[key] = _make_istft_basis(n_fft)
    cos_b, sin_b, weights, window = _istft_basis_cache[key]
    cos_b, sin_b, weights, window = cos_b.to(z.dtype), sin_b.to(z.dtype), weights.to(z.dtype), window.to(z.dtype)

    *other, n_freqs, T, _two = z.shape
    # Force plain Python ints: during ONNX tracing, tensor.shape elements are
    # dynamically-tracked scalar tensors by default (so exports can optionally
    # support variable input sizes via dynamic_axes) — fine almost everywhere,
    # but col2im's ONNX symbolic function specifically requires its
    # output_size argument to resolve to statically-known sizes, and errors
    # out (TypeError inside _get_tensor_sizes) if it doesn't. This export is
    # fixed-shape by design (see export_demucs_onnx.py), so baking these in
    # as constants is exactly what we want, not a loss of generality.
    T = int(T)
    z = z.reshape(-1, n_freqs, T, 2)
    real = z[..., 0] * math.sqrt(n_fft)
    imag = z[..., 1] * math.sqrt(n_fft)
    real = real.transpose(1, 2)  # [N,T,Fr]
    imag = imag.transpose(1, 2)
    real_w = real * weights
    imag_w = imag * weights
    frames = (real_w @ cos_b.t() - imag_w @ sin_b.t()) / n_fft  # [N,T,n_fft]
    frames = frames * window

    N = int(frames.shape[0])
    out_len = (T - 1) * hop_length + n_fft
    frames_t = frames.transpose(1, 2)  # [N, n_fft, T]
    signal = F.fold(frames_t, output_size=(1, out_len), kernel_size=(1, n_fft), stride=(1, hop_length)).view(N, out_len)

    win_sq = (window ** 2).view(n_fft, 1).expand(n_fft, T)
    win_env = F.fold(win_sq.unsqueeze(0), output_size=(1, out_len), kernel_size=(1, n_fft),
                      stride=(1, hop_length)).view(out_len)
    win_env = win_env.clamp_min(1e-11)
    signal = signal / win_env.unsqueeze(0)

    pad = n_fft // 2
    signal = signal[:, pad:pad + length]
    return signal.reshape(*other, length)


def _patched_spec(self, x):
    hl = self.hop_length
    nfft = self.nfft
    assert hl == nfft // 4
    le = int(math.ceil(x.shape[-1] / hl))
    pad = hl // 2 * 3
    x = pad1d(x, (pad, pad + le * hl - x.shape[-1]), mode="reflect")

    z = custom_stft_interleaved(x, nfft, hl)[..., :-1, :]
    assert z.shape[-1] == le + 4, (z.shape, x.shape, le)
    z = z[..., 2: 2 + le]
    return z


def _patched_magnitude(self, z):
    # _patched_spec already produces the real, channel-interleaved (re,im)
    # layout _magnitude used to compute from a genuine complex z — identity.
    assert self.cac, "this patch only covers htdemucs's default cac=True path"
    return z


def _patched_mask(self, z, m):
    assert self.cac, "this patch only covers htdemucs's default cac=True path"
    B, S, C, Fr, T = m.shape
    out = m.view(B, S, -1, 2, Fr, T).permute(0, 1, 2, 4, 5, 3)  # [B,S,C_true,Fr,T,2], real
    return out


def _patched_ispec(self, z, length=None, scale=0):
    hl = self.hop_length // (4 ** scale)
    z = F.pad(z, (0, 0, 0, 0, 0, 1))  # pad Fr (dim -3) by 1; T and trailing-2 unchanged
    z = F.pad(z, (0, 0, 2, 2))        # pad T (dim -2) by 2 each side; trailing-2 unchanged
    pad = hl // 2 * 3
    le = hl * int(math.ceil(length / hl)) + 2 * pad
    x = custom_ispec_from_trailing2(z, self.nfft, hl, le)
    x = x[..., pad: pad + length]
    return x


def apply():
    """Idempotent — safe to call more than once."""
    HTDemucs._spec = _patched_spec
    HTDemucs._magnitude = _patched_magnitude
    HTDemucs._mask = _patched_mask
    HTDemucs._ispec = _patched_ispec
