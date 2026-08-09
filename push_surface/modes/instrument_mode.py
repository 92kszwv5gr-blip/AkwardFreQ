"""
instrument_mode.py — Plugin parameter control mode.

Displays encoder bank name/value pairs on the LCD.
Left/Right arrow buttons navigate between banks.
"""
from .base_mode import BaseMode
from ..push1_hardware import BUTTON_MAP, LED_FULL, LED_OFF


class InstrumentMode(BaseMode):

    def activate(self):
        self._refresh_display()

    def deactivate(self):
        pass

    def on_note(self, note: int, velocity: int):
        if velocity == 0:
            return
        if note == BUTTON_MAP["right_arrow"]:
            self.mapper.next_bank()
            self._refresh_display()
        elif note == BUTTON_MAP["left_arrow"]:
            self.mapper.prev_bank()
            self._refresh_display()

    def on_encoder(self, encoder_idx: int, delta: float):
        self.mapper.current_bank.set_value(encoder_idx, delta)
        self._refresh_display()

    def _refresh_display(self):
        bank = self.mapper.current_bank
        bank_label = f"[{self.mapper.current_bank_index + 1}] {bank.name}"
        names = bank.get_names()
        values = bank.get_values()
        self.lcd.write_row(0, bank_label[:28])
        # Compact value display: "N:V  N:V  ..."
        row1 = " ".join(
            f"{n[:3]}:{v[:3]}" for n, v in zip(names, values) if n
        )
        self.lcd.write_row(1, row1[:28])
