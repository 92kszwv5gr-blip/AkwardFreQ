"""
link_bridge/osc/osc_client.py — OSC UDP client for sending parameter changes.
"""
import struct
import socket


def _encode_osc_string(s: str) -> bytes:
    """Encode a string as null-terminated, 4-byte-aligned OSC string."""
    encoded = s.encode("utf-8") + b"\x00"
    pad = (4 - len(encoded) % 4) % 4
    return encoded + b"\x00" * pad


def _encode_osc_float(value: float) -> bytes:
    return struct.pack(">f", value)


def _encode_osc_int(value: int) -> bytes:
    return struct.pack(">i", value)


def build_osc_message(address: str, *args) -> bytes:
    """
    Build a raw OSC message.
    Supports float (f) and int (i) argument types.
    """
    type_tag = ","
    arg_bytes = b""
    for arg in args:
        if isinstance(arg, float):
            type_tag += "f"
            arg_bytes += _encode_osc_float(arg)
        elif isinstance(arg, int):
            type_tag += "i"
            arg_bytes += _encode_osc_int(arg)
        elif isinstance(arg, str):
            type_tag += "s"
            arg_bytes += _encode_osc_string(arg)

    return (
        _encode_osc_string(address)
        + _encode_osc_string(type_tag)
        + arg_bytes
    )


class OscClient:
    """Send OSC messages to a target host:port over UDP."""

    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def send(self, address: str, *args):
        """Send an OSC message."""
        msg = build_osc_message(address, *args)
        try:
            self._socket.sendto(msg, (self.host, self.port))
        except Exception as e:
            print(f"[OscClient] Send error to {self.host}:{self.port} {address}: {e}")

    def send_param(self, osc_path: str, value: float):
        """Convenience: send a normalised parameter value."""
        self.send(osc_path, float(value))

    def close(self):
        self._socket.close()
