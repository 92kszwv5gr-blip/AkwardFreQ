"""
Base class for all Push 1 modes.
"""


class BaseMode:
    """All Push 1 modes inherit from this."""

    def __init__(self, surface):
        self._surface = surface

    @property
    def lcd(self):
        return self._surface.lcd

    @property
    def mapper(self):
        return self._surface.mapper

    def activate(self):
        """Called when this mode becomes active."""
        pass

    def deactivate(self):
        """Called when leaving this mode."""
        pass

    def on_note(self, note: int, velocity: int):
        pass

    def on_encoder(self, encoder_idx: int, delta: float):
        pass

    def on_touch_strip(self, position: float):
        pass

    def on_cc(self, cc: int, value: int):
        pass
