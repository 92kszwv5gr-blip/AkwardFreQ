"""
song_mode.py — Song Structure navigation mode.
Shows sections on the LCD, navigates via arrow buttons.
"""
from .base_mode import BaseMode
from ..push1_hardware import BUTTON_MAP


class SongMode(BaseMode):

    def activate(self):
        self._refresh_display()

    def on_note(self, note: int, velocity: int):
        if velocity == 0:
            return
        if note == BUTTON_MAP["right_arrow"]:
            self._surface.song.advance()
        elif note == BUTTON_MAP["left_arrow"]:
            self._surface.song.retreat()
        self._refresh_display()

    def _refresh_display(self):
        sections = self._surface.song.get_display_list()
        if not sections:
            self.lcd.show_message("SONG MODE", "No sections")
            return
        current = self._surface.song.current_section
        idx = self._surface.song.current_index
        row0 = f"SONG [{idx + 1}/{len(sections)}]"
        row1 = current.name[:28] if current else ""
        self.lcd.write_row(0, row0)
        self.lcd.write_row(1, row1)
