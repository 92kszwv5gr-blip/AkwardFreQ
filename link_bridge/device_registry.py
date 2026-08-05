"""
link_bridge/device_registry.py — Discovered network device registry.

Tracks all Link-connected and OSC-capable devices discovered on the LAN.
Each device exposes a list of parameters that can be read/set over OSC.
"""
import time
from typing import Optional, Callable


class NetworkDevice:
    """
    Represents a discovered network device (iOS app, DAW, etc.)
    that exposes parameters via OSC.
    """

    def __init__(self, device_id: str, name: str, host: str, port: int,
                 protocol: str = "osc"):
        self.device_id = device_id
        self.name = name[:32]
        self.host = host
        self.port = port
        self.protocol = protocol  # "osc", "rtpmidi", "ble"
        self.parameters: list = []   # list of {id, name, value, osc_path}
        self.last_seen = time.time()
        self.link_enabled = False
        self._set_param_fn: Optional[Callable] = None

    def set_param(self, encoder_idx: int, delta: float):
        """Change parameter value (called from Push 1 encoder)."""
        if encoder_idx < len(self.parameters):
            param = self.parameters[encoder_idx]
            new_val = max(0.0, min(1.0, param.get("value", 0.0) + delta))
            param["value"] = new_val
            if self._set_param_fn:
                self._set_param_fn(param.get("osc_path", ""), new_val)

    def update_param(self, osc_path: str, value: float):
        """Called when we receive a parameter update FROM the device."""
        for param in self.parameters:
            if param.get("osc_path") == osc_path:
                param["value"] = max(0.0, min(1.0, value))
                break

    def is_stale(self, timeout_s: float = 30.0) -> bool:
        return (time.time() - self.last_seen) > timeout_s

    def ping(self):
        self.last_seen = time.time()


class DeviceRegistry:
    """
    Registry of all discovered KNTKTA-compatible network devices.
    """

    def __init__(self):
        self._devices: dict[str, NetworkDevice] = {}
        self._on_device_added: Optional[Callable[[NetworkDevice], None]] = None
        self._on_device_removed: Optional[Callable[[NetworkDevice], None]] = None

    def add_device(self, device: NetworkDevice):
        is_new = device.device_id not in self._devices
        self._devices[device.device_id] = device
        if is_new and self._on_device_added:
            self._on_device_added(device)

    def remove_device(self, device_id: str):
        device = self._devices.pop(device_id, None)
        if device and self._on_device_removed:
            self._on_device_removed(device)

    def get_device(self, device_id: str) -> Optional[NetworkDevice]:
        return self._devices.get(device_id)

    def all_devices(self) -> list:
        return list(self._devices.values())

    def active_devices(self, timeout_s: float = 30.0) -> list:
        return [d for d in self._devices.values() if not d.is_stale(timeout_s)]

    def prune_stale(self, timeout_s: float = 60.0):
        stale_ids = [did for did, d in self._devices.items() if d.is_stale(timeout_s)]
        for did in stale_ids:
            self.remove_device(did)

    def on_device_added(self, fn: Callable):
        self._on_device_added = fn

    def on_device_removed(self, fn: Callable):
        self._on_device_removed = fn

    def count(self) -> int:
        return len(self._devices)
