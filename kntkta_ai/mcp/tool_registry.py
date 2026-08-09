"""
kntkta_ai/mcp/tool_registry.py — All MCP tools exposed to the AI.

Each tool has:
  - description: what the LLM sees
  - schema: JSON Schema for input validation
  - handler: async function that executes the tool
"""
import asyncio


# ------------------------------------------------------------------ #
#  Tool handlers
#  In production these communicate with kntkta_core via IPC/OSC.
#  For now they return structured responses for LLM consumption.
# ------------------------------------------------------------------ #

async def _tool_map_parameter(args: dict) -> dict:
    """Map a plugin parameter to a Push 1 encoder."""
    param_name = args.get("parameter_name", "")
    encoder = args.get("encoder_index", 0)
    bank = args.get("bank_index", 0)
    display_name = args.get("display_name", param_name[:8])
    return {
        "success": True,
        "message": f"Mapped '{param_name}' to Bank {bank}, Encoder {encoder} as '{display_name}'",
        "bank_index": bank,
        "encoder_index": encoder,
        "display_name": display_name,
    }


async def _tool_suggest_layout(args: dict) -> dict:
    """Generate a Push 1 layout suggestion for a plugin."""
    plugin_name = args.get("plugin_name", "")
    params = args.get("parameters", [])
    # Group by category keywords
    groups = {
        "Oscillator": [], "Filter": [], "Amp/Env": [],
        "LFO": [], "FX": [], "Other": []
    }
    for p in params:
        name_lower = p.lower()
        if any(k in name_lower for k in ["osc", "wave", "tune", "detune", "pitch"]):
            groups["Oscillator"].append(p)
        elif any(k in name_lower for k in ["filt", "cutoff", "res", "freq"]):
            groups["Filter"].append(p)
        elif any(k in name_lower for k in ["env", "attack", "decay", "sustain", "release", "amp"]):
            groups["Amp/Env"].append(p)
        elif any(k in name_lower for k in ["lfo", "mod", "vibrato"]):
            groups["LFO"].append(p)
        elif any(k in name_lower for k in ["reverb", "delay", "chorus", "flange", "fx"]):
            groups["FX"].append(p)
        else:
            groups["Other"].append(p)

    banks = []
    for bank_idx, (group_name, group_params) in enumerate(groups.items()):
        if not group_params:
            continue
        encoders = []
        for enc_idx, param in enumerate(group_params[:8]):
            encoders.append({
                "encoder_index": enc_idx,
                "parameter_name": param,
                "display_name": param[:8],
            })
        banks.append({
            "bank_index": bank_idx,
            "name": group_name[:8],
            "encoders": encoders,
        })

    return {
        "plugin": plugin_name,
        "suggested_banks": banks,
        "message": f"Layout suggested for {plugin_name}: {len(banks)} banks",
    }


async def _tool_set_tempo(args: dict) -> dict:
    bpm = args.get("bpm", 120.0)
    return {"success": True, "bpm": bpm, "message": f"Tempo set to {bpm} BPM"}


async def _tool_launch_scene(args: dict) -> dict:
    index = args.get("scene_index", 0)
    return {"success": True, "scene_index": index, "message": f"Launching scene {index}"}


async def _tool_set_fx_param(args: dict) -> dict:
    path = args.get("device_path", "")
    param = args.get("parameter", "")
    value = args.get("value", 0.0)
    return {
        "success": True,
        "message": f"Set {path}/{param} = {value}",
    }


async def _tool_create_bank(args: dict) -> dict:
    name = args.get("name", "New Bank")
    return {"success": True, "bank_name": name, "message": f"Bank '{name}' created"}


async def _tool_quick_save(args: dict) -> dict:
    bank = args.get("bank_name", "My Bank")
    return {"success": True, "bank_name": bank, "message": f"Preset saved to '{bank}'"}


async def _tool_set_sequencer(args: dict) -> dict:
    track = args.get("track_index", 0)
    steps = args.get("steps")
    hits = args.get("hits")
    rotation = args.get("rotation")
    algorithm = args.get("algorithm", "euclidean")
    result = {"track_index": track, "algorithm": algorithm, "success": True}
    if steps is not None:
        result["steps"] = steps
    if hits is not None:
        result["hits"] = hits
    if rotation is not None:
        result["rotation"] = rotation
    result["message"] = f"Sequencer track {track} updated"
    return result


async def _tool_song_section(args: dict) -> dict:
    action = args.get("action", "advance")
    section_name = args.get("name", "")
    scene = args.get("scene_index", 0)
    tempo = args.get("tempo")
    return {
        "success": True,
        "action": action,
        "section": section_name,
        "message": f"Song section action '{action}' executed",
    }


async def _tool_resample(args: dict) -> dict:
    mode = args.get("mode", "chromatic")
    note_low = args.get("note_low", 36)
    note_high = args.get("note_high", 84)
    length_bars = args.get("length_bars", 2)
    return {
        "success": True,
        "mode": mode,
        "note_low": note_low,
        "note_high": note_high,
        "length_bars": length_bars,
        "message": f"Resampling {mode} C{note_low // 12 - 1} to C{note_high // 12 - 1}",
    }


# ------------------------------------------------------------------ #
#  Tool registry
# ------------------------------------------------------------------ #

TOOL_REGISTRY = {
    "map_parameter": {
        "description": "Map a plugin parameter to a specific Push 1 encoder and bank",
        "schema": {
            "type": "object",
            "properties": {
                "parameter_name": {"type": "string", "description": "Exact plugin parameter name"},
                "encoder_index": {"type": "integer", "minimum": 0, "maximum": 7},
                "bank_index": {"type": "integer", "minimum": 0, "maximum": 63},
                "display_name": {"type": "string", "maxLength": 8},
            },
            "required": ["parameter_name"],
        },
        "handler": _tool_map_parameter,
    },
    "suggest_layout": {
        "description": "Generate a Push 1 encoder layout for a given plugin and parameter list",
        "schema": {
            "type": "object",
            "properties": {
                "plugin_name": {"type": "string"},
                "parameters": {"type": "array", "items": {"type": "string"}},
            },
            "required": ["plugin_name", "parameters"],
        },
        "handler": _tool_suggest_layout,
    },
    "set_tempo": {
        "description": "Set the song tempo in BPM",
        "schema": {
            "type": "object",
            "properties": {
                "bpm": {"type": "number", "minimum": 20, "maximum": 300},
            },
            "required": ["bpm"],
        },
        "handler": _tool_set_tempo,
    },
    "launch_scene": {
        "description": "Launch an Ableton scene by index",
        "schema": {
            "type": "object",
            "properties": {
                "scene_index": {"type": "integer", "minimum": 0},
            },
            "required": ["scene_index"],
        },
        "handler": _tool_launch_scene,
    },
    "set_fx_param": {
        "description": "Set a parameter value on a device/plugin by path",
        "schema": {
            "type": "object",
            "properties": {
                "device_path": {"type": "string", "description": "e.g. 'FX/Reverb'"},
                "parameter": {"type": "string"},
                "value": {"type": "number", "minimum": 0.0, "maximum": 1.0},
            },
            "required": ["device_path", "parameter", "value"],
        },
        "handler": _tool_set_fx_param,
    },
    "create_bank": {
        "description": "Create a new preset bank with a given name",
        "schema": {
            "type": "object",
            "properties": {
                "name": {"type": "string"},
            },
            "required": ["name"],
        },
        "handler": _tool_create_bank,
    },
    "quick_save": {
        "description": "Quick-save current preset edits to a named bank",
        "schema": {
            "type": "object",
            "properties": {
                "bank_name": {"type": "string"},
            },
            "required": ["bank_name"],
        },
        "handler": _tool_quick_save,
    },
    "set_sequencer": {
        "description": "Modify Euclidean sequencer settings for a track",
        "schema": {
            "type": "object",
            "properties": {
                "track_index": {"type": "integer", "minimum": 0, "maximum": 7},
                "steps": {"type": "integer", "minimum": 1, "maximum": 16},
                "hits": {"type": "integer", "minimum": 0, "maximum": 16},
                "rotation": {"type": "integer"},
                "algorithm": {
                    "type": "string",
                    "enum": ["euclidean", "random_walk", "probability", "markov"]
                },
            },
            "required": ["track_index"],
        },
        "handler": _tool_set_sequencer,
    },
    "song_section": {
        "description": "Add, navigate, or trigger a song structure section",
        "schema": {
            "type": "object",
            "properties": {
                "action": {"type": "string", "enum": ["advance", "retreat", "jump", "add"]},
                "name": {"type": "string"},
                "scene_index": {"type": "integer"},
                "tempo": {"type": "number"},
            },
            "required": ["action"],
        },
        "handler": _tool_song_section,
    },
    "resample": {
        "description": "Trigger the chromatic resampler across MIDI note range",
        "schema": {
            "type": "object",
            "properties": {
                "mode": {"type": "string", "enum": ["chromatic", "chop"]},
                "note_low": {"type": "integer", "minimum": 0, "maximum": 127},
                "note_high": {"type": "integer", "minimum": 0, "maximum": 127},
                "length_bars": {"type": "integer", "minimum": 1, "maximum": 16},
            },
        },
        "handler": _tool_resample,
    },
}
