"""
action_system.py — ClyphX-style Macro Action Scripting System.

Syntax:  ACTION_NAME ARG1 ARG2 | ACTION_NAME2 ARG ...

Supported actions:
  SET TEMPO <bpm>
  LAUNCH SCENE <index>
  STOP SCENE
  SET TRACK/<name>/VOL <value>
  SET TRACK/<name>/PAN <value>
  SET TRACK/<name>/MUTE <0|1>
  SET TRACK/<name>/SOLO <0|1>
  SET FX/<device>/<param> <value>
  AI <prompt>
  SAMPLE RECORD
  SAMPLE CHOP
  LINK SYNC <on|off>
  BANK <index>
  MODE <mode_name>
  SNAPSHOT SAVE <id>
  SNAPSHOT RECALL <id>
  MORPH <snap_a> <snap_b> <position>
"""
import re
from typing import Callable, Optional


class ActionContext:
    """Provides callbacks for all available action handlers."""

    def __init__(self):
        self._handlers: dict[str, Callable] = {}

    def register(self, action_name: str, handler: Callable):
        self._handlers[action_name.upper()] = handler

    def execute(self, action_name: str, args: list) -> bool:
        handler = self._handlers.get(action_name.upper())
        if handler:
            try:
                handler(*args)
                return True
            except Exception as e:
                print(f"[ActionSystem] Error in {action_name}: {e}")
        else:
            print(f"[ActionSystem] Unknown action: {action_name}")
        return False


def parse_action_string(action_string: str) -> list:
    """
    Parse a pipe-delimited action string into a list of (action, args) tuples.

    Example:
      "SET TEMPO 128 | LAUNCH SCENE 4"
      → [("SET", ["TEMPO", "128"]), ("LAUNCH", ["SCENE", "4"])]
    """
    results = []
    for part in action_string.split("|"):
        part = part.strip()
        if not part:
            continue
        tokens = part.split()
        if tokens:
            results.append((tokens[0].upper(), tokens[1:]))
    return results


class ActionBank:
    """A bank of 64 named action strings assignable to pads/buttons."""

    def __init__(self, bank_id: str, name: str):
        self.bank_id = bank_id
        self.name = name
        # slot_index (0–63) → {"label": str, "action": str}
        self.slots: dict[int, dict] = {}

    def assign(self, slot_index: int, label: str, action_string: str):
        assert 0 <= slot_index < 64
        self.slots[slot_index] = {
            "label": label[:8],
            "action": action_string,
        }

    def get_slot(self, slot_index: int) -> Optional[dict]:
        return self.slots.get(slot_index)

    def to_dict(self) -> dict:
        return {
            "bank_id": self.bank_id,
            "name": self.name,
            "slots": self.slots,
        }

    @classmethod
    def from_dict(cls, data: dict) -> "ActionBank":
        bank = cls(data["bank_id"], data["name"])
        bank.slots = {int(k): v for k, v in data.get("slots", {}).items()}
        return bank


class ActionSystem:
    """
    Manages action banks and executes action strings through a context.
    """

    def __init__(self, context: ActionContext):
        self.context = context
        self.banks: dict[str, ActionBank] = {}
        self.active_bank_id: Optional[str] = None

    def add_bank(self, bank: ActionBank):
        self.banks[bank.bank_id] = bank
        if self.active_bank_id is None:
            self.active_bank_id = bank.bank_id

    def active_bank(self) -> Optional[ActionBank]:
        return self.banks.get(self.active_bank_id)

    def trigger_slot(self, slot_index: int):
        """Execute the action string assigned to a pad slot."""
        bank = self.active_bank()
        if not bank:
            return
        slot = bank.get_slot(slot_index)
        if not slot:
            return
        self.execute(slot["action"])

    def execute(self, action_string: str):
        """Parse and execute a full action string."""
        for action_name, args in parse_action_string(action_string):
            self.context.execute(action_name, args)

    def get_slot_labels(self) -> list:
        """Return 64 slot labels for the active bank (empty string if unassigned)."""
        bank = self.active_bank()
        if not bank:
            return [""] * 64
        return [
            bank.slots.get(i, {}).get("label", "")
            for i in range(64)
        ]
