"""
sound_forge/resampler/chromatic_resampler.py — Chromatic Resampler.

Inspired by Tom Cosm's "External Sampler" M4L tool.

Process:
  1. User triggers resample (from Push 1 or app)
  2. Engine sends MIDI note C2 to target instrument/plugin
  3. Records audio output for `length_bars` bars
  4. Stops, sends next note (C#2), records, repeat up to C5
  5. Saves each recording as a WAV file named by MIDI note
  6. Exports multi-sampled instrument in all target formats

MIDI note range C2–C5 = notes 36–84 (49 notes)
"""
import time
import os
import wave
from .midi_note_sender import MidiNoteSender

NOTE_C2 = 36
NOTE_C5 = 84
RESAMPLE_NOTES = list(range(NOTE_C2, NOTE_C5 + 1))  # 49 notes

NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def note_name(midi_note: int) -> str:
    octave = (midi_note // 12) - 1
    name = NOTE_NAMES[midi_note % 12]
    return f"{name}{octave}"


class ResampledNote:
    """One captured note in the resampled instrument."""

    def __init__(self, midi_note: int, filepath: str,
                 sample_rate: int, num_frames: int):
        self.midi_note = midi_note
        self.note_name = note_name(midi_note)
        self.filepath = filepath
        self.sample_rate = sample_rate
        self.num_frames = num_frames

    @property
    def duration_ms(self) -> float:
        return (self.num_frames / self.sample_rate) * 1000.0


class ChromaticResampler:
    """
    Captures audio at each MIDI note from C2 to C5 (or custom range).

    Requires:
      - A MidiNoteSender to trigger the target instrument
      - An audio input (sounddevice or system audio) for capture
      - Output directory for WAV files

    Usage:
        sender = MidiNoteSender(port_name="My Synth", channel=1)
        resampler = ChromaticResampler(sender, sample_rate=44100)
        notes = resampler.capture_all(
            output_dir="/tmp/my_instrument",
            length_bars=2,
            tempo_bpm=120,
            pre_roll_ms=100
        )
        resampler.export_sfz(notes, "/tmp/my_instrument", "My Synth")
    """

    def __init__(self, midi_sender: "MidiNoteSender",
                 sample_rate: int = 44100,
                 channels: int = 1,
                 note_range: tuple = (NOTE_C2, NOTE_C5),
                 velocity: int = 100):
        self.midi_sender = midi_sender
        self.sample_rate = sample_rate
        self.channels = channels
        self.note_range = note_range
        self.velocity = velocity
        self._on_progress = None  # callback(note, total)

    def on_progress(self, fn):
        """Register a progress callback: fn(current_note_idx, total_notes)."""
        self._on_progress = fn

    def capture_all(self, output_dir: str, length_bars: int = 2,
                    tempo_bpm: float = 120.0, pre_roll_ms: float = 100.0) -> list:
        """
        Capture audio for every note in the range.
        Returns list of ResampledNote objects.
        """
        os.makedirs(output_dir, exist_ok=True)
        notes_to_capture = list(range(self.note_range[0], self.note_range[1] + 1))
        bar_duration_s = (60.0 / tempo_bpm) * 4
        note_duration_s = bar_duration_s * length_bars
        pre_roll_s = pre_roll_ms / 1000.0

        captured = []
        for idx, midi_note in enumerate(notes_to_capture):
            if self._on_progress:
                self._on_progress(idx, len(notes_to_capture))

            wav_path = os.path.join(output_dir, f"note_{midi_note:03d}_{note_name(midi_note)}.wav")
            audio_data = self._capture_note(
                midi_note, note_duration_s, pre_roll_s
            )
            num_frames = self._write_wav(wav_path, audio_data)
            captured.append(ResampledNote(
                midi_note=midi_note,
                filepath=wav_path,
                sample_rate=self.sample_rate,
                num_frames=num_frames,
            ))

        return captured

    def _capture_note(self, midi_note: int, duration_s: float,
                      pre_roll_s: float) -> bytes:
        """
        Send MIDI note and record audio.
        In production this uses sounddevice for real-time capture.
        Returns raw PCM bytes.
        """
        try:
            import sounddevice as sd

            total_duration = pre_roll_s + duration_s
            total_frames = int(total_duration * self.sample_rate)
            pre_roll_frames = int(pre_roll_s * self.sample_rate)

            # Start recording
            recording = sd.rec(
                total_frames,
                samplerate=self.sample_rate,
                channels=self.channels,
                dtype="int16",
            )

            # Wait for pre-roll, then trigger note
            time.sleep(pre_roll_s)
            self.midi_sender.note_on(midi_note, self.velocity)

            # Wait for note duration, then release
            time.sleep(duration_s)
            self.midi_sender.note_off(midi_note)

            # Wait for recording to finish
            sd.wait()
            return recording[pre_roll_frames:].tobytes()

        except ImportError:
            # sounddevice not available — return silence placeholder
            num_samples = int(duration_s * self.sample_rate * self.channels)
            return bytes(num_samples * 2)  # 16-bit zeros

    def _write_wav(self, filepath: str, audio_data: bytes) -> int:
        with wave.open(filepath, "wb") as wf:
            wf.setnchannels(self.channels)
            wf.setsampwidth(2)  # 16-bit
            wf.setframerate(self.sample_rate)
            wf.writeframes(audio_data)
        return len(audio_data) // (2 * self.channels)

    def export_sfz(self, notes: list, output_dir: str, name: str) -> str:
        """Export captured notes as SFZ."""
        os.makedirs(output_dir, exist_ok=True)
        sfz_path = os.path.join(output_dir, f"{name}.sfz")
        lines = [
            f"// {name} — Chromatic Resample by KNTKTA",
            f"// {len(notes)} notes: {note_name(notes[0].midi_note)} – {note_name(notes[-1].midi_note)}",
            "",
            "<control>",
            "default_path=./",
            "",
        ]
        for note in notes:
            wav_name = os.path.basename(note.filepath)
            lines += [
                "<region>",
                f"  sample={wav_name}",
                f"  lokey={note.midi_note}",
                f"  hikey={note.midi_note}",
                f"  pitch_keycenter={note.midi_note}",
                "  lovel=0 hivel=127",
                "",
            ]
        with open(sfz_path, "w") as f:
            f.write("\n".join(lines))
        return sfz_path

    def export_decent_sampler(self, notes: list, output_dir: str, name: str) -> str:
        """Export as Decent Sampler .dspreset."""
        import xml.etree.ElementTree as ET
        from xml.dom import minidom

        root = ET.Element("DecentSampler", minVersion="1.0.0")
        ET.SubElement(root, "ui", width="812", height="375")
        groups = ET.SubElement(root, "groups")
        group = ET.SubElement(groups, "group")

        for note in notes:
            wav_name = os.path.basename(note.filepath)
            ET.SubElement(group, "sample",
                path=wav_name,
                rootNote=str(note.midi_note),
                loNote=str(note.midi_note),
                hiNote=str(note.midi_note),
                loVel="0", hiVel="127", volume="1.0")

        output_path = os.path.join(output_dir, f"{name}.dspreset")
        xml_str = minidom.parseString(ET.tostring(root)).toprettyxml(indent="  ")
        with open(output_path, "w") as f:
            f.write(xml_str)
        return output_path
