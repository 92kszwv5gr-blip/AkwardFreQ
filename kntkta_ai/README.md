# kntkta_ai — Embedded MCP AI Assistant

Local AI co-pilot for KNTKTA, running via MCP (Model Context Protocol).

## Architecture

- MCP server exposing KNTKTA tools over JSON-RPC (stdio or TCP socket)
- Local LLM backend via Ollama (Mistral 7B / Phi-3 Mini)
- Optional voice input via OpenAI Whisper (local, base model)
- Callable from the main app GUI, Push 1 "AI" button, or CLI

## MCP Tools Exposed

| Tool | Description |
|---|---|
| `map_parameter` | Assign a parameter to an encoder/bank |
| `suggest_layout` | Generate a full Push 1 layout for a plugin |
| `set_tempo` | Change song tempo |
| `launch_scene` | Launch an Ableton scene |
| `set_fx_param` | Set a device parameter by path |
| `create_bank` | Create a new preset bank |
| `quick_save` | Save current preset edits to a bank |
| `set_sequencer` | Modify Euclidean sequencer settings |
| `song_section` | Add/navigate song structure sections |
| `resample` | Trigger the chromatic resampler |

## Setup

```bash
cd kntkta_ai
pip install -r requirements.txt
# Install Ollama: https://ollama.ai
ollama pull mistral
python server.py
```

## Voice Input

Requires `ffmpeg` on PATH. Uses Whisper `base` model (74MB).

```bash
pip install openai-whisper
python server.py --voice
```
