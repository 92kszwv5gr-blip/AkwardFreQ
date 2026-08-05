"""
kps_manager.py — KPS SQLite storage layer.

Schema:
  presets(preset_id TEXT PK, name TEXT, bank_name TEXT,
          plugin_name TEXT, source_format TEXT, data TEXT)
"""
import sqlite3
import json
from typing import Optional


CREATE_TABLE = """
CREATE TABLE IF NOT EXISTS presets (
    preset_id    TEXT PRIMARY KEY,
    name         TEXT NOT NULL,
    bank_name    TEXT DEFAULT '',
    plugin_name  TEXT DEFAULT '',
    source_format TEXT DEFAULT '',
    category     TEXT DEFAULT '',
    data         TEXT NOT NULL,
    created_at   TEXT,
    modified_at  TEXT
);
"""

CREATE_INDEX = """
CREATE INDEX IF NOT EXISTS idx_bank ON presets(bank_name);
"""


class KpsManager:
    """SQLite-backed KPS preset storage."""

    def __init__(self, db_path: str = ":memory:"):
        self._conn = sqlite3.connect(db_path, check_same_thread=False)
        self._conn.execute(CREATE_TABLE)
        self._conn.execute(CREATE_INDEX)
        self._conn.commit()

    def save_preset(self, kps: dict) -> str:
        preset_id = kps["preset_id"]
        self._conn.execute("""
            INSERT OR REPLACE INTO presets
            (preset_id, name, bank_name, plugin_name, source_format, category, data, created_at, modified_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            preset_id,
            kps.get("name", ""),
            kps.get("bank_name", ""),
            kps.get("plugin", {}).get("name", ""),
            kps.get("source_format", ""),
            kps.get("category", ""),
            json.dumps(kps),
            kps.get("created_at", ""),
            kps.get("modified_at", ""),
        ))
        self._conn.commit()
        return preset_id

    def load_preset(self, preset_id: str) -> Optional[dict]:
        row = self._conn.execute(
            "SELECT data FROM presets WHERE preset_id = ?", (preset_id,)
        ).fetchone()
        if row:
            return json.loads(row[0])
        return None

    def load_bank(self, bank_name: str) -> list:
        rows = self._conn.execute(
            "SELECT data FROM presets WHERE bank_name = ?", (bank_name,)
        ).fetchall()
        return [json.loads(r[0]) for r in rows]

    def list_banks(self) -> list:
        rows = self._conn.execute(
            "SELECT DISTINCT bank_name FROM presets ORDER BY bank_name"
        ).fetchall()
        return [r[0] for r in rows]

    def list_presets(self, bank_name: str = None) -> list:
        if bank_name:
            rows = self._conn.execute(
                "SELECT preset_id, name, bank_name, plugin_name, source_format "
                "FROM presets WHERE bank_name = ? ORDER BY name",
                (bank_name,)
            ).fetchall()
        else:
            rows = self._conn.execute(
                "SELECT preset_id, name, bank_name, plugin_name, source_format "
                "FROM presets ORDER BY bank_name, name"
            ).fetchall()
        return [
            {"preset_id": r[0], "name": r[1], "bank_name": r[2],
             "plugin_name": r[3], "source_format": r[4]}
            for r in rows
        ]

    def update_preset_bank(self, preset_id: str, bank_name: str):
        self._conn.execute(
            "UPDATE presets SET bank_name = ? WHERE preset_id = ?",
            (bank_name, preset_id)
        )
        self._conn.commit()

    def delete_preset(self, preset_id: str):
        self._conn.execute("DELETE FROM presets WHERE preset_id = ?", (preset_id,))
        self._conn.commit()

    def close(self):
        self._conn.close()
