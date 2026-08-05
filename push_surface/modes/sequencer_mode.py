"""
sequencer_mode.py — Euclidean + Generative Sequencer mode.

Euclidean sequencer:
  - 8 tracks (pad rows)
  - Per track: steps (1–16), hits, rotation, velocity, pitch
  - Algorithms: Euclidean, random walk, probability gates,
                cellular automaton, Markov chains
  - AI can seed/modify patterns via verbal commands

Pad layout (Euclidean mode):
  Row 0–7: track pattern display (lit = active step)
  Top 8 pads (row 0): select active track
"""
import math
import random
from .base_mode import BaseMode
from ..push1_hardware import (
    PAD_NOTE_MIN, pad_note_to_xy, xy_to_pad_note,
    LED_OFF, LED_DIM, LED_MID, LED_FULL, LED_BLINK
)


def euclidean_rhythm(steps: int, hits: int, rotation: int = 0) -> list:
    """
    Generate a Euclidean rhythm pattern.
    Returns a list of booleans of length `steps`.
    Uses Bjorklund's algorithm.
    """
    if hits <= 0:
        return [False] * steps
    if hits >= steps:
        return [True] * steps

    pattern = []
    counts = []
    remainders = []
    divisor = steps - hits
    remainders.append(hits)
    level = 0

    while True:
        counts.append(divisor // remainders[level])
        remainders.append(divisor % remainders[level])
        divisor = remainders[level]
        level += 1
        if remainders[level] <= 1:
            break

    counts.append(divisor)

    def build(level):
        if level == -1:
            pattern.append(False)
        elif level == -2:
            pattern.append(True)
        else:
            for _ in range(counts[level]):
                build(level - 1)
            if remainders[level] != 0:
                build(level - 2)

    build(level)
    pattern.reverse()

    # Apply rotation
    rotation = rotation % steps
    return pattern[rotation:] + pattern[:rotation]


class EuclideanTrack:
    """One track in the Euclidean sequencer."""

    def __init__(self, track_idx: int):
        self.track_idx = track_idx
        self.steps = 16
        self.hits = 4
        self.rotation = 0
        self.pitch = 36 + track_idx  # default MIDI note
        self.velocity = 100
        self.muted = False
        self.pattern: list = euclidean_rhythm(self.steps, self.hits, self.rotation)
        self.current_step = 0
        # Generative algorithm: "euclidean", "random_walk", "probability", "markov"
        self.algorithm = "euclidean"
        self.probability = 0.8      # for probability gate mode

    def regenerate(self):
        if self.algorithm == "euclidean":
            self.pattern = euclidean_rhythm(self.steps, self.hits, self.rotation)
        elif self.algorithm == "random_walk":
            self.pattern = self._random_walk()
        elif self.algorithm == "probability":
            self.pattern = [random.random() < self.probability for _ in range(self.steps)]
        elif self.algorithm == "markov":
            self.pattern = self._markov()

    def _random_walk(self) -> list:
        pat = []
        state = False
        for _ in range(self.steps):
            if random.random() < 0.4:
                state = not state
            pat.append(state)
        return pat

    def _markov(self) -> list:
        # Simple 2-state Markov: high probability of hit after silence
        pat = [False]
        for _ in range(self.steps - 1):
            if pat[-1]:
                pat.append(random.random() < 0.3)
            else:
                pat.append(random.random() < 0.6)
        return pat

    def advance(self) -> bool:
        """Advance one step. Returns True if this step is a hit."""
        hit = self.pattern[self.current_step] and not self.muted
        self.current_step = (self.current_step + 1) % self.steps
        return hit


NUM_TRACKS = 8


class SequencerMode(BaseMode):

    def __init__(self, surface):
        super().__init__(surface)
        self.tracks = [EuclideanTrack(i) for i in range(NUM_TRACKS)]
        self.active_track_idx = 0
        self._edit_param = "hits"  # which parameter encoders control

    def activate(self):
        self._refresh_display()
        self._refresh_pads()

    def deactivate(self):
        self._clear_pads()

    def on_note(self, note: int, velocity: int):
        if velocity == 0:
            return
        col, row = pad_note_to_xy(note)
        if row == 7:
            # Top pad row = select active track
            self.active_track_idx = col
            self._refresh_display()
        else:
            # Toggle step in active track pattern
            track = self.tracks[self.active_track_idx]
            if col < track.steps:
                track.pattern[col] = not track.pattern[col]
            self._refresh_pads()

    def on_encoder(self, encoder_idx: int, delta: float):
        track = self.tracks[self.active_track_idx]
        if encoder_idx == 0:
            track.steps = max(1, min(16, track.steps + (1 if delta > 0 else -1)))
            track.regenerate()
        elif encoder_idx == 1:
            track.hits = max(0, min(track.steps, track.hits + (1 if delta > 0 else -1)))
            track.regenerate()
        elif encoder_idx == 2:
            track.rotation = (track.rotation + (1 if delta > 0 else -1)) % track.steps
            track.regenerate()
        elif encoder_idx == 3:
            track.pitch = max(0, min(127, track.pitch + (1 if delta > 0 else -1)))
        elif encoder_idx == 4:
            track.velocity = max(1, min(127, track.velocity + (1 if delta > 0 else -1)))
        elif encoder_idx == 5:
            track.probability = max(0.0, min(1.0, track.probability + delta))
        self._refresh_display()
        self._refresh_pads()

    def _refresh_display(self):
        t = self.tracks[self.active_track_idx]
        row0 = f"SEQ T{self.active_track_idx + 1} S:{t.steps} H:{t.hits} R:{t.rotation}"
        row1 = f"Pit:{t.pitch} Vel:{t.velocity} {t.algorithm[:4].upper()}"
        self.lcd.write_row(0, row0[:28])
        self.lcd.write_row(1, row1[:28])

    def _refresh_pads(self):
        for track_idx, track in enumerate(self.tracks):
            for step in range(16):
                col = step % 8
                row = track_idx
                note = xy_to_pad_note(col, row)
                if step < track.steps and track.pattern[step]:
                    vel = LED_FULL if track_idx == self.active_track_idx else LED_MID
                else:
                    vel = LED_OFF
                self._surface.set_pad_led(note, vel)

    def _clear_pads(self):
        for row in range(8):
            for col in range(8):
                self._surface.set_pad_led(xy_to_pad_note(col, row), LED_OFF)

    def apply_ai_suggestion(self, suggestion: dict):
        """
        Apply an AI-generated pattern modification.
        suggestion: {track_idx, algorithm, steps, hits, rotation, pattern}
        """
        t_idx = suggestion.get("track_idx", self.active_track_idx)
        if 0 <= t_idx < NUM_TRACKS:
            t = self.tracks[t_idx]
            if "algorithm" in suggestion:
                t.algorithm = suggestion["algorithm"]
            if "steps" in suggestion:
                t.steps = suggestion["steps"]
            if "hits" in suggestion:
                t.hits = suggestion["hits"]
            if "rotation" in suggestion:
                t.rotation = suggestion["rotation"]
            if "pattern" in suggestion:
                t.pattern = suggestion["pattern"]
            else:
                t.regenerate()
        self._refresh_display()
        self._refresh_pads()
