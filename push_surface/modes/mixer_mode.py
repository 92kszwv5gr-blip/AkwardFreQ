"""
mixer_mode.py — Track mixer control.
Encoders control volume, pan, send levels for 8 tracks.
"""
from .base_mode import BaseMode
from ..push1_hardware import BUTTON_MAP


class MixerMode(BaseMode):

    def activate(self):
        self.lcd.show_message("MIXER MODE", "Vol Pan Snd1 Snd2")

    def on_note(self, note: int, velocity: int):
        pass

    def on_encoder(self, encoder_idx: int, delta: float):
        # encoder 0–7 → track 0–7 volume adjust
        try:
            song = self._surface.ableton.get_document()
            tracks = list(song.tracks)
            if encoder_idx < len(tracks):
                t = tracks[encoder_idx]
                new_vol = max(0.0, min(1.0, t.mixer_device.volume.value + delta))
                t.mixer_device.volume.value = new_vol
                self.lcd.write_segment(encoder_idx,
                                       t.name[:8],
                                       f"{int(new_vol * 100)}%")
        except Exception:
            pass
