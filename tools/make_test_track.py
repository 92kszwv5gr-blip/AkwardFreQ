#!/usr/bin/env python3
"""Write a synthetic psytrance-style test track (kick, rolling bass, hats, lead arp) as a stereo WAV.

Needs only numpy. Used for headless UI tests, where a real track is not available:

    python tools/make_test_track.py /tmp/test_track.wav

145 BPM, 32 beats (about 13 s). It is deliberately unrealistic: it produces far more, and shorter,
detected regions than real music would.
"""
import sys
import wave

import numpy as np


def main(path: str) -> None:
    sr, bpm = 44100, 145
    beat = 60 / bpm
    dur = 32 * beat
    n = int(sr * dur)
    out = np.zeros(n)
    rng = np.random.default_rng(1)

    def add(sig, start):
        i = int(start * sr)
        j = min(n, i + len(sig))
        out[i:j] += sig[: j - i]

    kt = np.arange(int(0.35 * sr)) / sr
    kick = np.sin(2 * np.pi * (45 + 120 * np.exp(-kt * 35)) * kt) * np.exp(-kt * 9)
    for b in range(32):
        add(kick, b * beat)

    step = beat / 4
    bt = np.arange(int(step * 0.95 * sr)) / sr
    bass = (2 * ((55 * bt) % 1) - 1) * np.exp(-bt * 6) * 0.35
    for k in range(128):
        if k % 4 != 0:
            add(bass, k * step)

    ht = np.arange(int(0.05 * sr)) / sr
    hat = rng.standard_normal(len(ht)) * np.exp(-ht * 90) * 0.18
    for k in range(64):
        add(hat, k * beat / 2 + beat / 4)

    notes = [440, 523, 659, 784, 659, 523, 392, 494]
    lt = np.arange(int(step * 0.9 * sr)) / sr
    for k in range(128):
        f = notes[(k // 2) % 8] * (2 if (k // 16) % 2 else 1)
        add(np.sign(np.sin(2 * np.pi * f * lt)) * np.exp(-lt * 8) * 0.10, k * step)

    out /= np.max(np.abs(out)) * 1.15
    stereo = np.stack([out, out * 0.97], axis=1)
    pcm = (stereo * 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(pcm.tobytes())
    print(f"wrote {path} ({dur:.1f} s)")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "/tmp/test_track.wav")
