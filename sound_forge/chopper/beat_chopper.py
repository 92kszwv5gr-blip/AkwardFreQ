"""
sound_forge/chopper/beat_chopper.py — Beat Chopper main engine.

Records audio, detects transients or uses grid-based slicing,
assigns each slice to a MIDI note (36–162), and exports multi-format
instrument presets.
"""
import os
import wave
import struct
from pathlib import Path
from .transient_detector import detect_transients


MAX_SLICES = 127
BASE_NOTE = 36  # MIDI C2


class AudioSlice:
    """One chopped sample slice."""

    def __init__(self, index: int, start_frame: int, end_frame: int,
                 sample_rate: int, audio_data: bytes):
        self.index = index
        self.midi_note = BASE_NOTE + index
        self.start_frame = start_frame
        self.end_frame = end_frame
        self.sample_rate = sample_rate
        self.audio_data = audio_data
        self.filename: str = ""

    @property
    def duration_ms(self) -> float:
        frames = self.end_frame - self.start_frame
        return (frames / self.sample_rate) * 1000.0


class BeatChopper:
    """
    Main beat chopper: record → detect → slice → export.

    Usage:
        chopper = BeatChopper(sample_rate=44100, channels=1)
        chopper.load_audio("beat.wav")
        chopper.chop_by_transients(threshold=0.3)
        chopper.export_sfz("/output/my_chops", "my_kit")
    """

    def __init__(self, sample_rate: int = 44100, channels: int = 1, bit_depth: int = 16):
        self.sample_rate = sample_rate
        self.channels = channels
        self.bit_depth = bit_depth
        self.slices: list[AudioSlice] = []
        self._audio_data: bytes = b""
        self._total_frames: int = 0

    def load_audio(self, filepath: str):
        """Load a WAV file as the source audio."""
        with wave.open(filepath, "rb") as wf:
            self.sample_rate = wf.getframerate()
            self.channels = wf.getnchannels()
            self.bit_depth = wf.getsampwidth() * 8
            self._total_frames = wf.getnframes()
            self._audio_data = wf.readframes(self._total_frames)
        self.slices = []

    def chop_by_transients(self, threshold: float = 0.3, min_gap_ms: float = 50.0):
        """Auto-chop by detecting transients (onsets)."""
        onset_frames = detect_transients(
            self._audio_data, self.sample_rate, self.channels,
            self.bit_depth, threshold, min_gap_ms
        )
        self._make_slices_from_onsets(onset_frames)

    def chop_by_grid(self, num_slices: int):
        """Divide audio into equal-length slices."""
        num_slices = min(num_slices, MAX_SLICES)
        frames_per_slice = self._total_frames // num_slices
        onsets = [i * frames_per_slice for i in range(num_slices)]
        self._make_slices_from_onsets(onsets)

    def chop_at_frames(self, onset_frames: list):
        """Manual chop at specified frame positions."""
        self._make_slices_from_onsets(sorted(onset_frames))

    def _make_slices_from_onsets(self, onset_frames: list):
        self.slices = []
        bytes_per_frame = (self.bit_depth // 8) * self.channels
        onsets = list(onset_frames) + [self._total_frames]
        for i in range(min(len(onsets) - 1, MAX_SLICES)):
            start = onsets[i]
            end = onsets[i + 1]
            byte_start = start * bytes_per_frame
            byte_end = end * bytes_per_frame
            self.slices.append(AudioSlice(
                index=i,
                start_frame=start,
                end_frame=end,
                sample_rate=self.sample_rate,
                audio_data=self._audio_data[byte_start:byte_end],
            ))

    def save_slices(self, output_dir: str, name_prefix: str = "slice") -> list:
        """Write each slice as a separate WAV file. Returns list of paths."""
        os.makedirs(output_dir, exist_ok=True)
        paths = []
        for slc in self.slices:
            filename = f"{name_prefix}_{slc.index:03d}_note{slc.midi_note}.wav"
            filepath = os.path.join(output_dir, filename)
            slc.filename = filepath
            self._write_wav(filepath, slc.audio_data)
            paths.append(filepath)
        return paths

    def _write_wav(self, filepath: str, audio_data: bytes):
        with wave.open(filepath, "wb") as wf:
            wf.setnchannels(self.channels)
            wf.setsampwidth(self.bit_depth // 8)
            wf.setframerate(self.sample_rate)
            wf.writeframes(audio_data)

    def export_sfz(self, output_dir: str, instrument_name: str) -> str:
        """Export slices as an SFZ instrument. Returns .sfz file path."""
        from .export.sfz_exporter import SfzExporter
        return SfzExporter().export(self.slices, output_dir, instrument_name)

    def export_decent_sampler(self, output_dir: str, instrument_name: str) -> str:
        """Export as Decent Sampler .dspreset."""
        from .export.decent_sampler_exporter import DecentSamplerExporter
        return DecentSamplerExporter().export(self.slices, output_dir, instrument_name)

    def export_ableton_drum_rack(self, output_dir: str, instrument_name: str) -> str:
        """Export as Ableton Drum Rack .adg."""
        from .export.ableton_drum_rack_exporter import AbletonDrumRackExporter
        return AbletonDrumRackExporter().export(self.slices, output_dir, instrument_name)
