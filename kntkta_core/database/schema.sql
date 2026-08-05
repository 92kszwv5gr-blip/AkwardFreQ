-- KNTKTA SQLite Database Schema
-- Used by kntkta_core and preset_forge

PRAGMA journal_mode = WAL;
PRAGMA foreign_keys = ON;

-- ------------------------------------------------------------------ --
-- Presets
-- ------------------------------------------------------------------ --
CREATE TABLE IF NOT EXISTS presets (
    preset_id     TEXT PRIMARY KEY,
    name          TEXT NOT NULL,
    display_name  TEXT DEFAULT '',
    bank_name     TEXT DEFAULT '',
    plugin_name   TEXT DEFAULT '',
    plugin_format TEXT DEFAULT '',
    source_format TEXT DEFAULT '',
    category      TEXT DEFAULT '',
    author        TEXT DEFAULT '',
    data          TEXT NOT NULL,   -- Full KPS JSON
    created_at    TEXT,
    modified_at   TEXT
);
CREATE INDEX IF NOT EXISTS idx_preset_bank ON presets(bank_name);
CREATE INDEX IF NOT EXISTS idx_preset_plugin ON presets(plugin_name);

-- ------------------------------------------------------------------ --
-- Banks
-- ------------------------------------------------------------------ --
CREATE TABLE IF NOT EXISTS banks (
    bank_id       TEXT PRIMARY KEY,
    name          TEXT NOT NULL UNIQUE,
    author        TEXT DEFAULT '',
    description   TEXT DEFAULT '',
    plugin_name   TEXT DEFAULT '',
    created_at    TEXT,
    modified_at   TEXT
);

-- ------------------------------------------------------------------ --
-- Push 1 Layouts
-- Stores KPS push1_layout JSON per preset
-- ------------------------------------------------------------------ --
CREATE TABLE IF NOT EXISTS push1_layouts (
    preset_id     TEXT PRIMARY KEY REFERENCES presets(preset_id) ON DELETE CASCADE,
    layout_json   TEXT NOT NULL,
    modified_at   TEXT
);

-- ------------------------------------------------------------------ --
-- Action Banks
-- ------------------------------------------------------------------ --
CREATE TABLE IF NOT EXISTS action_banks (
    bank_id   TEXT PRIMARY KEY,
    name      TEXT NOT NULL,
    data      TEXT NOT NULL,   -- ActionBank JSON
    created_at TEXT
);

-- ------------------------------------------------------------------ --
-- Song Structures
-- ------------------------------------------------------------------ --
CREATE TABLE IF NOT EXISTS song_structures (
    structure_id TEXT PRIMARY KEY,
    name         TEXT NOT NULL,
    project_name TEXT DEFAULT '',
    data         TEXT NOT NULL,   -- SongStructure JSON
    created_at   TEXT,
    modified_at  TEXT
);

-- ------------------------------------------------------------------ --
-- Samples
-- ------------------------------------------------------------------ --
CREATE TABLE IF NOT EXISTS samples (
    sample_id    TEXT PRIMARY KEY,
    name         TEXT NOT NULL,
    filepath     TEXT NOT NULL,
    midi_note    INTEGER DEFAULT 0,
    sample_rate  INTEGER DEFAULT 44100,
    num_frames   INTEGER DEFAULT 0,
    bank_name    TEXT DEFAULT '',
    created_at   TEXT
);

-- ------------------------------------------------------------------ --
-- Network Devices (Link bridge)
-- ------------------------------------------------------------------ --
CREATE TABLE IF NOT EXISTS network_devices (
    device_id    TEXT PRIMARY KEY,
    name         TEXT NOT NULL,
    host         TEXT NOT NULL,
    port         INTEGER NOT NULL,
    protocol     TEXT DEFAULT 'osc',
    last_seen    TEXT,
    params_json  TEXT DEFAULT '[]'
);
