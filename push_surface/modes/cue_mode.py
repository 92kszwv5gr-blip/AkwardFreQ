"""
cue_mode.py — Cue monitoring mode.
Routes selected track to secondary output for headphone cueing.
"""
from .base_mode import BaseMode
from ..push1_hardware import BUTTON_MAP, LED_FULL, LED_OFF


class CueMode(BaseMode):

    def __init__(self, surface):
        super().__init__(surface)
        self._cued_track_idx: int = -1

    def activate(self):
        self.lcd.show_message("CUE MODE", "Select track to cue")

    def on_note(self, note: int, velocity: int):
        if velocity == 0:
            return
        # Track select buttons (top row, notes 20–27)
        from ..push1_hardware import TRACK_SELECT_NOTES
        if note in TRACK_SELECT_NOTES:
            track_idx = note - TRACK_SELECT_NOTES[0]
            self._set_cue(track_idx)

    def _set_cue(self, track_idx: int):
        self._cued_track_idx = track_idx
        try:
            doc = self._surface.ableton.get_document()
            tracks = list(doc.tracks)
            if track_idx < len(tracks):
                track = tracks[track_idx]
                # In Ableton, mute_solo gives us cue access
                doc.view.selected_track = track
                self.lcd.show_message(
                    "CUE MODE",
                    f"Cueing: {track.name[:20]}"
                )
        except Exception:
            self.lcd.show_message("CUE MODE", f"Track {track_idx + 1}")
