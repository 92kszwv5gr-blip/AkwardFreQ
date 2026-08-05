"""
vstpreset_parser.py — VST3 .vstpreset file parser.

VST3 preset format:
  Bytes 0–3:   Magic "VST3"
  Bytes 4–7:   Version (int32 LE)
  Bytes 8–39:  Class ID (FUID, 32 bytes ASCII hex)
  Bytes 40–47: Offset to chunk list (int64 LE)
  Chunk list:  "List" + entries [ID(4) + offset(8) + size(8)]
  Chunk IDs:   "Comp" (component state), "Cont" (controller state)
"""
import struct
import uuid as _uuid
from datetime import datetime, timezone

VST3_MAGIC = b"VST3"


class VstPresetParser:
    """Parse a VST3 .vstpreset file."""

    EXTENSION = ".vstpreset"

    def parse(self, filepath: str) -> list:
        with open(filepath, "rb") as f:
            data = f.read()

        if len(data) < 48 or data[:4] != VST3_MAGIC:
            raise ValueError("Not a valid VST3 preset file")

        version = struct.unpack("<I", data[4:8])[0]
        class_id = data[8:40].decode("ascii", errors="replace").strip("\x00")
        list_offset = struct.unpack("<q", data[40:48])[0]

        # Read chunk list
        chunks = {}
        if list_offset > 0 and list_offset + 4 < len(data):
            list_magic = data[list_offset:list_offset + 4]
            if list_magic == b"List":
                entry_count = struct.unpack("<I", data[list_offset + 4:list_offset + 8])[0]
                for i in range(entry_count):
                    base = list_offset + 8 + i * 20
                    if base + 20 > len(data):
                        break
                    chunk_id = data[base:base + 4].decode("ascii", errors="replace")
                    offset = struct.unpack("<q", data[base + 4:base + 12])[0]
                    size = struct.unpack("<q", data[base + 12:base + 20])[0]
                    chunks[chunk_id] = data[offset:offset + size]

        # Store raw chunks as hex for future re-export
        kps = {
            "kps_version": "1.0",
            "preset_id": str(_uuid.uuid4()),
            "name": filepath.rsplit("/", 1)[-1].replace(".vstpreset", ""),
            "display_name": "",
            "plugin": {
                "name": class_id[:32],
                "format": "vst3",
                "uid": class_id,
                "version": str(version),
            },
            "source_format": ".vstpreset",
            "parameters": [],
            "created_at": datetime.now(timezone.utc).isoformat(),
            "modified_at": datetime.now(timezone.utc).isoformat(),
        }
        kps["display_name"] = kps["name"][:16]

        if "Comp" in chunks:
            kps["raw_chunk_comp"] = chunks["Comp"].hex()
        if "Cont" in chunks:
            kps["raw_chunk_cont"] = chunks["Cont"].hex()

        return [kps]
