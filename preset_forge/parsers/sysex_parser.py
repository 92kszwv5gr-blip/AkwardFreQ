"""
sysex_parser.py — MIDI SysEx bank dump parser.

Reads a .mid file and extracts SysEx messages as raw preset chunks.
Each SysEx message is stored as a KPS preset with a raw_sysex field.
"""
import uuid
from datetime import datetime, timezone

SYSEX_START = 0xF0
SYSEX_END = 0xF7


class SysexParser:
    """Extract SysEx messages from a MIDI file as raw KPS entries."""

    EXTENSION = ".mid"

    def parse(self, filepath: str) -> list:
        with open(filepath, "rb") as f:
            data = f.read()

        messages = self._extract_sysex(data)
        results = []
        for idx, msg in enumerate(messages):
            manf_id = f"{msg[1]:02X}" if len(msg) > 1 else "00"
            kps = {
                "kps_version": "1.0",
                "preset_id": str(uuid.uuid4()),
                "name": f"SysEx {idx + 1} (Manf:{manf_id})",
                "display_name": f"SysEx{idx + 1}",
                "plugin": {"name": f"SysEx-{manf_id}", "format": "unknown"},
                "source_format": ".mid",
                "parameters": [],
                "raw_sysex": bytes(msg).hex(),
                "created_at": datetime.now(timezone.utc).isoformat(),
                "modified_at": datetime.now(timezone.utc).isoformat(),
            }
            results.append(kps)
        return results

    def _extract_sysex(self, data: bytes) -> list:
        """Find all SysEx messages in raw bytes (works on raw .syx too)."""
        messages = []
        i = 0
        while i < len(data):
            if data[i] == SYSEX_START:
                end = data.find(bytes([SYSEX_END]), i)
                if end == -1:
                    break
                messages.append(data[i:end + 1])
                i = end + 1
            else:
                i += 1
        return messages
