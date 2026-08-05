"""
ableton_parser.py — Ableton .adv (Instrument Rack) and .adg (Device Group) parser.

Both formats are gzip-compressed XML files.
"""
import gzip
import uuid
import xml.etree.ElementTree as ET
from datetime import datetime, timezone


def _parse_ableton_xml(filepath: str) -> ET.Element:
    """Open an Ableton gzip-XML file and return the root element."""
    try:
        with gzip.open(filepath, "rb") as f:
            data = f.read()
    except OSError:
        # Some .adv files aren't gzipped
        with open(filepath, "rb") as f:
            data = f.read()
    return ET.fromstring(data)


def _extract_params(root: ET.Element) -> list:
    """Extract AutomationTarget / UserName parameter nodes from Ableton XML."""
    params = []
    seen_ids = set()
    param_id = 0

    for elem in root.iter():
        # Ableton device parameters have <AutomationTarget Id="...">
        if elem.tag == "AutomationTarget":
            parent = elem
            name = parent.get("UserName") or parent.get("Id") or f"Param {param_id}"
            raw_val = 0.0
            for child in parent:
                if child.tag == "Value":
                    try:
                        raw_val = float(child.get("Value", "0"))
                    except ValueError:
                        pass
            at_id = parent.get("Id", str(param_id))
            if at_id not in seen_ids:
                seen_ids.add(at_id)
                params.append({
                    "id": param_id,
                    "name": name,
                    "display_name": name[:8],
                    "value": max(0.0, min(1.0, raw_val)),
                })
                param_id += 1

    return params


class AbletonAdvParser:
    """Parse Ableton Instrument Rack .adv files."""

    EXTENSION = ".adv"

    def parse(self, filepath: str) -> list:
        root = _parse_ableton_xml(filepath)
        name = root.get("DisplayName") or "Ableton Preset"
        params = _extract_params(root)
        return [{
            "kps_version": "1.0",
            "preset_id": str(uuid.uuid4()),
            "name": name,
            "display_name": name[:16],
            "plugin": {"name": "Ableton", "format": "ableton_device"},
            "source_format": ".adv",
            "parameters": params,
            "created_at": datetime.now(timezone.utc).isoformat(),
            "modified_at": datetime.now(timezone.utc).isoformat(),
        }]


class AbletonAdgParser:
    """Parse Ableton Device Group .adg files."""

    EXTENSION = ".adg"

    def parse(self, filepath: str) -> list:
        root = _parse_ableton_xml(filepath)
        name = root.get("DisplayName") or "Ableton Device Group"
        params = _extract_params(root)
        return [{
            "kps_version": "1.0",
            "preset_id": str(uuid.uuid4()),
            "name": name,
            "display_name": name[:16],
            "plugin": {"name": "Ableton", "format": "ableton_device"},
            "source_format": ".adg",
            "parameters": params,
            "created_at": datetime.now(timezone.utc).isoformat(),
            "modified_at": datetime.now(timezone.utc).isoformat(),
        }]
