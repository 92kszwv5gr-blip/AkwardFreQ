"""
forge.py — Main PresetForge API.

High-level interface for importing, converting, and exporting presets.
"""
import os
import json
from pathlib import Path
from .parsers import get_parser
from .kps_manager import KpsManager


class PresetForge:
    """
    Main entry point for the preset conversion engine.

    Usage:
        forge = PresetForge("path/to/kntkta.db")
        presets = forge.import_file("my_synth.fxb")
        forge.save_presets(presets, bank_name="My Bank")
        bank = forge.load_bank("My Bank")
    """

    def __init__(self, db_path: str = ":memory:"):
        self.db = KpsManager(db_path)

    def import_file(self, filepath: str) -> list:
        """
        Import a preset file. Returns a list of KPS preset dicts.
        Raises ValueError if the format is unsupported.
        """
        ext = Path(filepath).suffix.lower()
        parser_cls = get_parser(ext)
        if not parser_cls:
            raise ValueError(f"Unsupported preset format: {ext}")
        parser = parser_cls()
        presets = parser.parse(filepath)
        return presets

    def import_directory(self, dirpath: str, recursive: bool = True) -> list:
        """
        Scan a directory and import all recognised preset files.
        Returns a flat list of KPS dicts.
        """
        all_presets = []
        pattern = "**/*" if recursive else "*"
        for path in Path(dirpath).glob(pattern):
            if path.is_file():
                ext = path.suffix.lower()
                if get_parser(ext):
                    try:
                        all_presets.extend(self.import_file(str(path)))
                    except Exception as e:
                        print(f"[PresetForge] Skipping {path.name}: {e}")
        return all_presets

    def save_presets(self, presets: list, bank_name: str = "") -> list:
        """
        Save KPS presets to the database.
        Returns list of saved preset_ids.
        """
        ids = []
        for preset in presets:
            if bank_name:
                preset["bank_name"] = bank_name
            preset_id = self.db.save_preset(preset)
            ids.append(preset_id)
        return ids

    def load_preset(self, preset_id: str) -> dict:
        return self.db.load_preset(preset_id)

    def load_bank(self, bank_name: str) -> list:
        return self.db.load_bank(bank_name)

    def list_banks(self) -> list:
        return self.db.list_banks()

    def list_presets(self, bank_name: str = None) -> list:
        return self.db.list_presets(bank_name)

    def export_kps(self, preset_id: str, output_path: str):
        """Export a single preset as a .kps JSON file."""
        preset = self.db.load_preset(preset_id)
        with open(output_path, "w") as f:
            json.dump(preset, f, indent=2)

    def export_bank_kps(self, bank_name: str, output_path: str):
        """Export an entire bank as a .kps JSON file."""
        import uuid as _uuid
        from datetime import datetime, timezone
        presets = self.db.load_bank(bank_name)
        bank = {
            "kps_version": "1.0",
            "bank_id": str(_uuid.uuid4()),
            "name": bank_name,
            "presets": presets,
            "created_at": datetime.now(timezone.utc).isoformat(),
            "modified_at": datetime.now(timezone.utc).isoformat(),
        }
        with open(output_path, "w") as f:
            json.dump(bank, f, indent=2)

    def quick_save_to_bank(self, preset_id: str, bank_name: str):
        """Move/copy a preset into a named bank."""
        self.db.update_preset_bank(preset_id, bank_name)
