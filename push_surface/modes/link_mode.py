"""
link_mode.py — Ableton Link + networked device control mode.

Displays discovered Link devices on the LCD.
Encoders and pads control parameters of the selected remote device.
"""
from .base_mode import BaseMode


class LinkMode(BaseMode):

    def __init__(self, surface):
        super().__init__(surface)
        self._devices: list = []
        self._selected_idx = 0

    def activate(self):
        self._scan_devices()
        self._refresh_display()

    def on_note(self, note: int, velocity: int):
        if velocity == 0:
            return
        # Pad rows select device
        from ..push1_hardware import pad_note_to_xy
        col, row = pad_note_to_xy(note)
        if row == 0 and col < len(self._devices):
            self._selected_idx = col
            self._refresh_display()

    def on_encoder(self, encoder_idx: int, delta: float):
        if not self._devices:
            return
        device = self._devices[self._selected_idx]
        # Delegate parameter control to link_bridge module
        try:
            device.set_param(encoder_idx, delta)
        except Exception:
            pass

    def _scan_devices(self):
        """Request device list from link_bridge via IPC."""
        # In production this calls link_bridge's device registry
        self._devices = []

    def _refresh_display(self):
        if not self._devices:
            self.lcd.show_message("LINK MODE", "No devices found")
            return
        dev = self._devices[self._selected_idx]
        self.lcd.show_message(
            f"LINK [{self._selected_idx + 1}/{len(self._devices)}]",
            getattr(dev, "name", "Unknown")[:28]
        )
