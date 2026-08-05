"""
link_bridge/osc/osc_server.py — OSC UDP server for receiving parameter updates.

Listens for OSC messages from iOS apps, DAWs, and other network devices.

OSC address patterns:
  /kntkta/register          - Device registration
  /kntkta/param/<path>      - Parameter update
  /kntkta/ping              - Keep-alive
  /kntkta/link/tempo        - Tempo sync
"""
import asyncio
import struct
import socket
import threading
from typing import Callable, Optional

DEFAULT_PORT = 8765
KNTKTA_OSC_PORT = 8765


def _parse_osc_string(data: bytes, offset: int):
    """Parse a null-terminated, 4-byte-aligned OSC string."""
    end = data.index(b"\x00", offset)
    s = data[offset:end].decode("utf-8", errors="replace")
    # Advance to next 4-byte boundary
    padded_end = end + (4 - (end % 4)) % 4 + (4 if (end % 4) == 0 else 0)
    return s, padded_end


def _parse_osc_message(data: bytes) -> tuple:
    """
    Parse a raw OSC message.
    Returns (address, type_tag, args) or raises ValueError.
    """
    if not data:
        raise ValueError("Empty OSC message")

    address, offset = _parse_osc_string(data, 0)
    type_tag = ""
    args = []

    if offset < len(data) and data[offset:offset + 1] == b",":
        type_tag_raw, offset = _parse_osc_string(data, offset)
        type_tag = type_tag_raw.lstrip(",")

        for t in type_tag:
            if t == "f":
                val = struct.unpack(">f", data[offset:offset + 4])[0]
                args.append(val)
                offset += 4
            elif t == "i":
                val = struct.unpack(">i", data[offset:offset + 4])[0]
                args.append(val)
                offset += 4
            elif t == "s":
                val, offset = _parse_osc_string(data, offset)
                args.append(val)
            elif t == "d":
                val = struct.unpack(">d", data[offset:offset + 8])[0]
                args.append(val)
                offset += 8

    return address, type_tag, args


class OscServer:
    """
    UDP OSC server for receiving parameter updates from network devices.
    """

    def __init__(self, port: int = DEFAULT_PORT):
        self.port = port
        self._socket: Optional[socket.socket] = None
        self._running = False
        self._thread: Optional[threading.Thread] = None
        self._handlers: dict[str, Callable] = {}

    def add_handler(self, address_pattern: str, fn: Callable):
        """Register a handler for an OSC address pattern."""
        self._handlers[address_pattern] = fn

    def start(self, bind_host: str = "127.0.0.1"):
        """
        Start the OSC server in a background thread.

        bind_host: Interface to bind on.
          - "127.0.0.1" (default) — localhost only; safe for single-machine use.
          - Set to a specific LAN IP (e.g. "192.168.1.10") to accept packets from
            iOS/Link devices on the local network only.
          - Avoid binding to "0.0.0.0" unless operating in a trusted LAN environment
            with no exposure to untrusted networks.
        """
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._socket.bind((bind_host, self.port))
        self._socket.settimeout(1.0)
        self._running = True
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()
        print(f"[OscServer] Listening on UDP port {self.port}")

    def stop(self):
        self._running = False
        if self._socket:
            self._socket.close()
        if self._thread:
            self._thread.join(timeout=2)

    def _run(self):
        while self._running:
            try:
                data, addr = self._socket.recvfrom(4096)
                self._dispatch(data, addr)
            except socket.timeout:
                continue
            except Exception as e:
                if self._running:
                    print(f"[OscServer] Error: {e}")

    def _dispatch(self, data: bytes, addr: tuple):
        try:
            address, type_tag, args = _parse_osc_message(data)
        except Exception:
            return

        # Try exact match first, then prefix match
        handler = self._handlers.get(address)
        if not handler:
            for pattern, fn in self._handlers.items():
                if address.startswith(pattern.rstrip("*")):
                    handler = fn
                    break

        if handler:
            try:
                handler(address, args, addr)
            except Exception as e:
                print(f"[OscServer] Handler error for {address}: {e}")
