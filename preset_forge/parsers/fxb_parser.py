"""
fxb_parser.py — VST .fxb (bank) and .fxp (preset) parser.

FXB/FXP binary format:
  Bytes  0–3:  Magic  "CcnK"
  Bytes  4–7:  Chunk size (big-endian int32)
  Bytes  8–11: Type "FBCh" (bank chunk) / "FPCh" (preset chunk)
               or "FxBk" (bank params) / "FxPr" (preset params)
  Bytes 12–15: Format version
  Bytes 16–19: Plugin unique ID
  Bytes 20–23: Plugin version
  Bytes 24–27: numPrograms (bank) or numParams (preset)
  Bytes 28–51: prgName (preset only, 28 bytes, null-terminated)
  ...
"""
import struct
import uuid
from datetime import datetime, timezone

FXB_MAGIC = b"CcnK"
TYPE_BANK_CHUNK = b"FBCh"
TYPE_PRESET_CHUNK = b"FPCh"
TYPE_BANK_PARAMS = b"FxBk"
TYPE_PRESET_PARAMS = b"FxPr"


def _read_header(data: bytes) -> dict:
    if len(data) < 28:
        raise ValueError("File too short to be FXB/FXP")
    magic = data[0:4]
    if magic != FXB_MAGIC:
        raise ValueError(f"Invalid FXB/FXP magic: {magic!r}")
    chunk_size = struct.unpack(">I", data[4:8])[0]
    file_type = data[8:12]
    version = struct.unpack(">I", data[12:16])[0]
    plugin_id = data[16:20].decode("ascii", errors="replace")
    plugin_version = struct.unpack(">I", data[20:24])[0]
    num_items = struct.unpack(">I", data[24:28])[0]
    return {
        "chunk_size": chunk_size,
        "file_type": file_type,
        "version": version,
        "plugin_id": plugin_id,
        "plugin_version": plugin_version,
        "num_items": num_items,
    }


def _normalise_param(raw: float, vmin: float = 0.0, vmax: float = 1.0) -> float:
    if vmax == vmin:
        return 0.0
    return max(0.0, min(1.0, (raw - vmin) / (vmax - vmin)))


def _make_kps(name: str, plugin_id: str, parameters: list,
              source: str, bank_name: str = "") -> dict:
    return {
        "kps_version": "1.0",
        "preset_id": str(uuid.uuid4()),
        "name": name,
        "display_name": name[:16],
        "plugin": {
            "name": plugin_id,
            "format": "vst2",
            "uid": plugin_id,
        },
        "source_format": source,
        "parameters": parameters,
        "bank_name": bank_name,
        "created_at": datetime.now(timezone.utc).isoformat(),
        "modified_at": datetime.now(timezone.utc).isoformat(),
    }


class FxpParser:
    """Parse a single VST preset (.fxp)."""

    EXTENSION = ".fxp"

    def parse(self, filepath: str) -> list:
        """Returns list with one KPS preset dict."""
        with open(filepath, "rb") as f:
            data = f.read()
        hdr = _read_header(data)
        plugin_id = hdr["plugin_id"]
        num_params = hdr["num_items"]

        if hdr["file_type"] == TYPE_PRESET_PARAMS:
            # Parameter-based preset: floats start at offset 56
            name_raw = data[28:56]
            name = name_raw.split(b"\x00")[0].decode("ascii", errors="replace")
            params = []
            for i in range(num_params):
                offset = 56 + i * 4
                if offset + 4 > len(data):
                    break
                val = struct.unpack(">f", data[offset:offset + 4])[0]
                params.append({
                    "id": i,
                    "name": f"Param {i}",
                    "display_name": f"P{i:02d}",
                    "value": _normalise_param(val),
                })
            return [_make_kps(name, plugin_id, params, ".fxp")]

        elif hdr["file_type"] == TYPE_PRESET_CHUNK:
            # Opaque chunk preset — store raw chunk
            name_raw = data[28:56]
            name = name_raw.split(b"\x00")[0].decode("ascii", errors="replace")
            chunk_len = struct.unpack(">I", data[56:60])[0]
            chunk = data[60:60 + chunk_len]
            kps = _make_kps(name, plugin_id, [], ".fxp")
            kps["raw_chunk"] = chunk.hex()
            return [kps]

        raise ValueError(f"Unknown FXP type: {hdr['file_type']!r}")


class FxbParser:
    """Parse a VST bank (.fxb) — may contain multiple presets."""

    EXTENSION = ".fxb"

    def parse(self, filepath: str) -> list:
        """Returns list of KPS preset dicts (one per program)."""
        with open(filepath, "rb") as f:
            data = f.read()
        hdr = _read_header(data)
        plugin_id = hdr["plugin_id"]
        num_programs = hdr["num_items"]
        results = []

        if hdr["file_type"] == TYPE_BANK_PARAMS:
            # Each program is 56 + numParams*4 bytes
            # We need a second pass to determine numParams per program
            offset = 28
            for prog_idx in range(num_programs):
                if offset + 28 > len(data):
                    break
                prog_magic = data[offset:offset + 4]
                if prog_magic != FXB_MAGIC:
                    break
                prog_type = data[offset + 8:offset + 12]
                prog_num_params = struct.unpack(">I", data[offset + 24:offset + 28])[0]
                name_raw = data[offset + 28:offset + 56]
                name = name_raw.split(b"\x00")[0].decode("ascii", errors="replace")
                params = []
                for i in range(prog_num_params):
                    poffset = offset + 56 + i * 4
                    if poffset + 4 > len(data):
                        break
                    val = struct.unpack(">f", data[poffset:poffset + 4])[0]
                    params.append({
                        "id": i,
                        "name": f"Param {i}",
                        "display_name": f"P{i:02d}",
                        "value": _normalise_param(val),
                    })
                results.append(_make_kps(name, plugin_id, params, ".fxb",
                                         bank_name=filepath))
                offset += 56 + prog_num_params * 4

        elif hdr["file_type"] == TYPE_BANK_CHUNK:
            chunk_len = struct.unpack(">I", data[28:32])[0]
            chunk = data[32:32 + chunk_len]
            kps = _make_kps(f"Bank {plugin_id}", plugin_id, [], ".fxb")
            kps["raw_chunk"] = chunk.hex()
            kps["bank_name"] = filepath
            results.append(kps)

        return results
