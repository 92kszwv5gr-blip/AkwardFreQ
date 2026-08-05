"""
nki_parser.py — Native Instruments Kontakt .nki parser.

Kontakt .nki files are XML-based (NI's proprietary format).
We extract instrument name, parameters, and zone/group info.
"""
import uuid
import xml.etree.ElementTree as ET
from datetime import datetime, timezone


class NkiParser:
    """Parse a Kontakt .nki instrument file."""

    EXTENSION = ".nki"

    def parse(self, filepath: str) -> list:
        """Returns list with one KPS preset dict."""
        with open(filepath, "r", encoding="utf-8", errors="replace") as f:
            raw = f.read()

        # Try XML parse; NKI may have a binary preamble — skip to first '<'
        xml_start = raw.find("<?xml")
        if xml_start == -1:
            xml_start = raw.find("<")
        if xml_start == -1:
            raise ValueError("No XML content found in .nki file")

        xml_content = raw[xml_start:]
        try:
            root = ET.fromstring(xml_content)
        except ET.ParseError as e:
            raise ValueError(f"XML parse error in .nki: {e}")

        name = root.get("name", "Unknown NKI")
        params = []
        param_id = 0

        # Extract parameters from <parameter> elements
        for elem in root.iter("parameter"):
            p_name = elem.get("name", f"Param {param_id}")
            p_value = float(elem.get("value", "0") or "0")
            p_min = float(elem.get("min", "0") or "0")
            p_max = float(elem.get("max", "1") or "1")
            norm = (p_value - p_min) / (p_max - p_min) if p_max != p_min else 0.0
            norm = max(0.0, min(1.0, norm))
            params.append({
                "id": param_id,
                "name": p_name,
                "display_name": p_name[:8],
                "value": norm,
                "value_display": str(p_value),
                "min": p_min,
                "max": p_max,
            })
            param_id += 1

        return [{
            "kps_version": "1.0",
            "preset_id": str(uuid.uuid4()),
            "name": name,
            "display_name": name[:16],
            "plugin": {"name": "Kontakt", "format": "vst3"},
            "source_format": ".nki",
            "parameters": params,
            "created_at": datetime.now(timezone.utc).isoformat(),
            "modified_at": datetime.now(timezone.utc).isoformat(),
        }]
