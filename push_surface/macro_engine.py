"""
macro_engine.py — One-to-Many Macro Engine.

Maps a single Push 1 encoder to up to 16 parameters simultaneously,
with per-destination custom curves and value range scaling.

Curve types:
  linear      — direct proportional mapping
  exponential — slow start, fast end
  step        — quantised steps
  inverted    — 1 - value
"""
import math
from typing import Callable, Optional


CURVE_LINEAR = "linear"
CURVE_EXPONENTIAL = "exponential"
CURVE_STEP = "step"
CURVE_INVERTED = "inverted"

MAX_TARGETS = 16


def apply_curve(value: float, curve: str, steps: int = 8) -> float:
    """Apply a curve transform to a normalised 0–1 value."""
    v = max(0.0, min(1.0, value))
    if curve == CURVE_LINEAR:
        return v
    elif curve == CURVE_EXPONENTIAL:
        return v ** 2
    elif curve == CURVE_STEP:
        return round(v * steps) / steps
    elif curve == CURVE_INVERTED:
        return 1.0 - v
    return v


class MacroTarget:
    """One destination in a macro group."""

    def __init__(self, param_id: int, device_path: str,
                 value_setter: Callable[[float], None],
                 curve: str = CURVE_LINEAR,
                 range_min: float = 0.0,
                 range_max: float = 1.0):
        self.param_id = param_id
        self.device_path = device_path
        self._set = value_setter
        self.curve = curve
        self.range_min = range_min
        self.range_max = range_max

    def apply(self, normalised_value: float):
        curved = apply_curve(normalised_value, self.curve)
        scaled = self.range_min + curved * (self.range_max - self.range_min)
        self._set(max(self.range_min, min(self.range_max, scaled)))


class MacroGroup:
    """A named macro: one encoder controls multiple targets."""

    def __init__(self, macro_id: str, name: str):
        self.macro_id = macro_id
        self.name = name[:8]
        self.targets: list[MacroTarget] = []
        self._value: float = 0.0
        # Snapshots: {snapshot_id: {param_id: value}}
        self.snapshots: dict[int, dict] = {}

    def add_target(self, target: MacroTarget):
        if len(self.targets) < MAX_TARGETS:
            self.targets.append(target)

    def set_value(self, normalised_value: float):
        self._value = max(0.0, min(1.0, normalised_value))
        for target in self.targets:
            target.apply(self._value)

    def get_value(self) -> float:
        return self._value

    def save_snapshot(self, snapshot_id: int, value_getters: dict):
        """
        Save current values of all targets to a snapshot.
        value_getters: {param_id: callable() -> normalised_value}
        """
        self.snapshots[snapshot_id] = {
            t.param_id: value_getters.get(t.param_id, lambda: 0.0)()
            for t in self.targets
        }

    def morph(self, snapshot_a: int, snapshot_b: int, position: float):
        """
        Interpolate between two snapshots.
        position: 0.0 = snapshot_a, 1.0 = snapshot_b
        (Triggered by touch strip)
        """
        a = self.snapshots.get(snapshot_a, {})
        b = self.snapshots.get(snapshot_b, {})
        for target in self.targets:
            val_a = a.get(target.param_id, 0.0)
            val_b = b.get(target.param_id, 0.0)
            interp = val_a + (val_b - val_a) * max(0.0, min(1.0, position))
            target._set(interp)


class MacroEngine:
    """Registry of all macro groups for the current session."""

    def __init__(self):
        self._groups: dict[str, MacroGroup] = {}

    def create_group(self, macro_id: str, name: str) -> MacroGroup:
        group = MacroGroup(macro_id, name)
        self._groups[macro_id] = group
        return group

    def get_group(self, macro_id: str) -> Optional[MacroGroup]:
        return self._groups.get(macro_id)

    def all_groups(self) -> list[MacroGroup]:
        return list(self._groups.values())

    def remove_group(self, macro_id: str):
        self._groups.pop(macro_id, None)

    def clear(self):
        self._groups.clear()
