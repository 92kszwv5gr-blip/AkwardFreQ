"""
kntkta_ai/mcp/llm_bridge.py — LLM + Whisper integration bridge.

Connects local Ollama LLM to the MCP tool registry.
Optionally accepts voice input via Whisper.
"""
import json
import subprocess
import tempfile
import os
from typing import Optional


class LlmBridge:
    """
    Bridge between the local LLM (Ollama) and MCP tools.
    Processes natural language commands and converts them to tool calls.
    """

    SYSTEM_PROMPT = """You are KNTKTA-AI, an embedded music production assistant.
You help the user control their Akai Push 1 controller, manage presets,
and operate their DAW through natural language commands.

You have access to the following MCP tools:
- map_parameter: map plugin parameters to Push 1 encoders
- suggest_layout: generate Push 1 layouts for plugins
- set_tempo: change song BPM
- launch_scene: launch Ableton scenes
- set_fx_param: set device parameter values
- create_bank: create preset banks
- quick_save: save presets to a bank
- set_sequencer: control the Euclidean sequencer
- song_section: navigate song structure
- resample: trigger the chromatic resampler

Respond with tool calls in JSON format when the user asks to perform an action.
For informational questions, respond in plain text.
Keep responses concise.
"""

    def __init__(self, model: str = "mistral", voice: bool = False):
        self.model = model
        self.voice = voice
        self._whisper_model = None
        if voice:
            self._load_whisper()

    def _load_whisper(self):
        try:
            import whisper
            self._whisper_model = whisper.load_model("base")
            print("[LlmBridge] Whisper voice input enabled")
        except ImportError:
            print("[LlmBridge] Whisper not installed. Run: pip install openai-whisper")

    def transcribe_audio(self, audio_path: str) -> str:
        """Transcribe audio file to text using Whisper."""
        if not self._whisper_model:
            raise RuntimeError("Whisper not loaded")
        result = self._whisper_model.transcribe(audio_path)
        return result.get("text", "").strip()

    def chat(self, user_message: str, history: list = None) -> str:
        """
        Send a message to the local LLM and return its response.
        Requires Ollama to be running locally.
        """
        messages = [{"role": "system", "content": self.SYSTEM_PROMPT}]
        if history:
            messages.extend(history)
        messages.append({"role": "user", "content": user_message})

        try:
            import ollama
            response = ollama.chat(
                model=self.model,
                messages=messages,
            )
            return response["message"]["content"]
        except ImportError:
            return "[Error] Ollama package not installed. Run: pip install ollama"
        except Exception as e:
            return f"[Error] LLM unavailable: {e}"

    def parse_tool_call(self, llm_response: str) -> Optional[dict]:
        """
        Attempt to parse a tool call from LLM response.
        Returns dict with {tool, arguments} or None.
        """
        # Look for JSON block in response
        start = llm_response.find("{")
        end = llm_response.rfind("}") + 1
        if start >= 0 and end > start:
            try:
                data = json.loads(llm_response[start:end])
                if "tool" in data and "arguments" in data:
                    return data
            except json.JSONDecodeError:
                pass
        return None
