"""
action_mode.py — Macro Action pad grid mode.
Each pad triggers an action string from the active ActionBank.
"""
from .base_mode import BaseMode
from ..push1_hardware import (
    xy_to_pad_note, pad_note_to_xy, LED_OFF, LED_DIM, LED_FULL
)


class ActionMode(BaseMode):

    def activate(self):
        self._refresh_pads()
        self.lcd.show_message("ACTION MODE", "Pad=Trigger")

    def deactivate(self):
        pass

    def on_note(self, note: int, velocity: int):
        if velocity == 0:
            return
        col, row = pad_note_to_xy(note)
        slot_idx = row * 8 + col
        self._surface.action_system.trigger_slot(slot_idx)

    def _refresh_pads(self):
        labels = self._surface.action_system.get_slot_labels()
        for row in range(8):
            for col in range(8):
                idx = row * 8 + col
                note = xy_to_pad_note(col, row)
                vel = LED_FULL if labels[idx] else LED_OFF
                self._surface.set_pad_led(note, vel)
