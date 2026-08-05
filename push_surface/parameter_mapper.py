"""
parameter_mapper.py — Adaptive parameter mapping engine.

Reads parameters from the currently selected Ableton device (or any
device exposed via kntkta_core's plugin host) and auto-organises them
into Push 1 encoder banks.

Bank layout rules:
  - 8 encoders per bank
  - Up to 64 banks (512 parameters max per preset)
  - If a KPS layout is loaded, use it; otherwise auto-generate
  - AI suggestions (from kntkta_ai) can override auto-layout
"""
import math


ENCODERS_PER_BANK = 8
MAX_BANKS = 64


class ParameterBank:
    """One bank of 8 encoder assignments."""

    def __init__(self, bank_index: int, name: str = ""):
        self.bank_index = bank_index
        self.name = name[:8]
        # list of dicts: {param_id, display_name, value_getter, value_setter}
        self.encoders = [None] * ENCODERS_PER_BANK

    def assign(self, encoder_idx: int, param_id: int, display_name: str,
               value_getter, value_setter):
        assert 0 <= encoder_idx < ENCODERS_PER_BANK
        self.encoders[encoder_idx] = {
            "param_id": param_id,
            "display_name": display_name[:8],
            "get": value_getter,
            "set": value_setter,
        }

    def get_names(self) -> list:
        return [
            (e["display_name"] if e else "") for e in self.encoders
        ]

    def get_values(self) -> list:
        out = []
        for e in self.encoders:
            if e:
                try:
                    v = e["get"]()
                    out.append(_format_value(v))
                except Exception:
                    out.append("---")
            else:
                out.append("")
        return out

    def set_value(self, encoder_idx: int, delta: float):
        e = self.encoders[encoder_idx]
        if e:
            try:
                current = e["get"]()
                new_val = max(0.0, min(1.0, current + delta))
                e["set"](new_val)
            except Exception:
                pass


def _format_value(norm_value: float) -> str:
    """Format a normalised 0–1 value as a short display string."""
    pct = int(norm_value * 100)
    return f"{pct}%"[:4]


class ParameterMapper:
    """
    Manages all banks for the currently mapped device.
    """

    def __init__(self):
        self.banks: list[ParameterBank] = []
        self.current_bank_index: int = 0
        self.device_name: str = ""

    def load_from_device(self, device):
        """
        Auto-generate banks from an Ableton Live device object.
        device: ableton.v2 Device object (has .parameters list)
        """
        self.device_name = getattr(device, "name", "Unknown")[:16]
        params = [p for p in device.parameters if p.is_enabled]
        self._build_banks_from_params(params)

    def load_from_kps(self, kps_layout: dict, device):
        """
        Load a pre-defined KPS push1_layout, falling back to auto if missing.
        kps_layout: the push1_layout dict from a KPS preset
        device:     Ableton device object for live value access
        """
        if not kps_layout or "banks" not in kps_layout:
            self.load_from_device(device)
            return

        params_by_id = {}
        for p in device.parameters:
            params_by_id[p.original_name] = p

        self.banks = []
        for bank_data in kps_layout["banks"][:MAX_BANKS]:
            bank = ParameterBank(bank_data["bank_index"], bank_data.get("name", ""))
            for enc in bank_data.get("encoders", []):
                idx = enc.get("encoder_index", 0)
                pid = enc.get("parameter_id")
                dname = enc.get("display_name", "")
                # Try to find live parameter by ID index
                try:
                    p = device.parameters[pid]
                    bank.assign(idx, pid, dname or p.name[:8],
                                lambda _p=p: _p.value / (_p.max or 1.0),
                                lambda v, _p=p: setattr(_p, "value", v * (_p.max or 1.0)))
                except (IndexError, TypeError):
                    pass
            self.banks.append(bank)

    def _build_banks_from_params(self, params):
        """Auto-distribute parameters across banks, 8 per bank."""
        num_banks = math.ceil(len(params) / ENCODERS_PER_BANK)
        num_banks = min(num_banks, MAX_BANKS)
        self.banks = []
        for b in range(num_banks):
            bank = ParameterBank(b, f"Bank {b + 1}")
            for e in range(ENCODERS_PER_BANK):
                idx = b * ENCODERS_PER_BANK + e
                if idx < len(params):
                    p = params[idx]
                    pmax = p.max if p.max != 0 else 1.0
                    bank.assign(
                        e, idx,
                        p.name[:8],
                        lambda _p=p, _m=pmax: _p.value / _m,
                        lambda v, _p=p, _m=pmax: setattr(_p, "value", v * _m)
                    )
            self.banks.append(bank)

    @property
    def current_bank(self) -> ParameterBank:
        if not self.banks:
            return ParameterBank(0, "Empty")
        return self.banks[self.current_bank_index]

    def next_bank(self):
        if self.banks:
            self.current_bank_index = (self.current_bank_index + 1) % len(self.banks)

    def prev_bank(self):
        if self.banks:
            self.current_bank_index = (self.current_bank_index - 1) % len(self.banks)

    def apply_ai_layout(self, ai_layout: list):
        """
        Apply an AI-suggested layout.
        ai_layout: list of {bank_index, encoder_index, param_id, display_name}
        """
        for item in ai_layout:
            b = item.get("bank_index", 0)
            e = item.get("encoder_index", 0)
            pid = item.get("param_id", 0)
            dname = item.get("display_name", "")
            if b < len(self.banks) and self.banks[b].encoders[e] is not None:
                self.banks[b].encoders[e]["display_name"] = dname[:8]

    def to_kps_layout(self) -> dict:
        """Export current layout as a KPS push1_layout dict."""
        banks = []
        for bank in self.banks:
            encoders = []
            for i, e in enumerate(bank.encoders):
                if e:
                    encoders.append({
                        "encoder_index": i,
                        "parameter_id": e["param_id"],
                        "display_name": e["display_name"],
                    })
            banks.append({
                "bank_index": bank.bank_index,
                "name": bank.name,
                "encoders": encoders,
            })
        return {"banks": banks}
