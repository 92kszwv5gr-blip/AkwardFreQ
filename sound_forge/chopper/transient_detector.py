"""
sound_forge/chopper/transient_detector.py — Audio onset/transient detection.

Uses a simple energy-based onset detector operating on raw PCM audio.
For production use, replace with librosa.onset.onset_detect() for higher accuracy.
"""
import struct
import math


def _pcm_to_floats(audio_data: bytes, bit_depth: int, channels: int) -> list:
    """Convert raw PCM bytes to normalised float samples (mono mix-down)."""
    if bit_depth == 16:
        fmt = f"<{len(audio_data) // 2}h"
        samples = struct.unpack(fmt, audio_data)
        scale = 32768.0
    elif bit_depth == 24:
        # 24-bit: read 3 bytes at a time
        count = len(audio_data) // 3
        samples = []
        for i in range(count):
            b = audio_data[i * 3: i * 3 + 3]
            val = int.from_bytes(b, "little", signed=True)
            samples.append(val)
        scale = 8388608.0
    elif bit_depth == 32:
        fmt = f"<{len(audio_data) // 4}f"
        samples = struct.unpack(fmt, audio_data)
        scale = 1.0
    else:
        return []

    # Mix down to mono
    if channels > 1:
        mono = []
        for i in range(0, len(samples), channels):
            frame_sum = sum(samples[i:i + channels])
            mono.append(frame_sum / channels / scale)
        return mono
    return [s / scale for s in samples]


def detect_transients(audio_data: bytes, sample_rate: int, channels: int,
                      bit_depth: int, threshold: float = 0.3,
                      min_gap_ms: float = 50.0) -> list:
    """
    Detect transient onsets in PCM audio.

    Returns list of frame indices where onsets occur.

    Algorithm:
      1. Convert to mono float
      2. Compute RMS energy in overlapping windows
      3. Detect sudden energy increases above threshold
      4. Enforce minimum gap between onsets
    """
    samples = _pcm_to_floats(audio_data, bit_depth, channels)
    if not samples:
        return [0]

    window_size = int(sample_rate * 0.01)   # 10ms windows
    hop_size = window_size // 2
    min_gap_frames = int(sample_rate * min_gap_ms / 1000.0)

    # Compute RMS per hop
    energies = []
    i = 0
    while i + window_size <= len(samples):
        window = samples[i:i + window_size]
        rms = math.sqrt(sum(s * s for s in window) / window_size)
        energies.append((i, rms))
        i += hop_size

    if not energies:
        return [0]

    # Detect peaks: energy increase relative to local average
    onsets = [0]
    prev_energy = energies[0][1]
    last_onset_frame = 0

    for frame, energy in energies[1:]:
        if prev_energy > 0:
            ratio = energy / (prev_energy + 1e-9)
        else:
            ratio = 1.0
        if ratio > (1.0 + threshold) and (frame - last_onset_frame) >= min_gap_frames:
            onsets.append(frame)
            last_onset_frame = frame
        prev_energy = energy

    return onsets
