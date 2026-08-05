"""
sample_mode.py — Sample engine bridge mode.
Delegates to sound_forge module over IPC/OSC.
"""
from .base_mode import BaseMode
from ..push1_hardware import BUTTON_MAP, LED_FULL, LED_BLINK, LED_OFF, xy_to_pad_note


SAMPLE_MODE_CHOP = "chop"
SAMPLE_MODE_RESAMPLE = "resample"


class SampleMode(BaseMode):

    def __init__(self, surface):
        super().__init__(surface)
        self._sub_mode = SAMPLE_MODE_CHOP
        self._recording = False
        self._chops: list = []

    def activate(self):
        self._refresh_display()

    def deactivate(self):
        self._recording = False

    def on_note(self, note: int, velocity: int):
        if velocity == 0:
            return
        if note == BUTTON_MAP["record"]:
            self._toggle_record()
        elif note == BUTTON_MAP["new"]:
            self._sub_mode = (
                SAMPLE_MODE_RESAMPLE
                if self._sub_mode == SAMPLE_MODE_CHOP
                else SAMPLE_MODE_CHOP
            )
            self._refresh_display()

    def _toggle_record(self):
        self._recording = not self._recording
        self._surface.set_button_led(BUTTON_MAP["record"], self._recording)
        if self._recording:
            self.lcd.show_message("RECORDING...", self._sub_mode.upper())
        else:
            self.lcd.show_message("SAMPLE READY", "Press REC again")

    def _refresh_display(self):
        self.lcd.show_message(
            f"SAMPLE:{self._sub_mode[:6].upper()}",
            "REC=Record NEW=Mode"
        )
