"""
push1_surface.py — Main KNTKTA Remote Script ControlSurface class.

Wires together all Push 1 subsystems:
  - Hardware MIDI I/O
  - LCD display
  - Parameter mapper
  - Macro engine
  - Action system
  - Song structure
  - All modes (instrument, mixer, sequencer, session, sample, link, action, cue)

Mode switching: hold SHIFT + press mode button on Push 1.
"""
from .push1_hardware import (
    BUTTON_MAP, ENCODER_CC, TOUCH_STRIP_CC, PAD_NOTE_MIN, PAD_NOTE_MAX,
    LED_OFF, LED_FULL
)
from .lcd_display import LcdDisplay
from .parameter_mapper import ParameterMapper
from .macro_engine import MacroEngine
from .action_system import ActionSystem, ActionContext, ActionBank
from .song_structure import SongStructure

from .modes import (
    InstrumentMode, MixerMode, SequencerMode, SessionMode,
    SampleMode, LinkMode, ActionMode, SongMode, CueMode,
)

MODE_INSTRUMENT = "instrument"
MODE_MIXER = "mixer"
MODE_SEQUENCER = "sequencer"
MODE_SESSION = "session"
MODE_SAMPLE = "sample"
MODE_LINK = "link"
MODE_ACTION = "action"
MODE_SONG = "song"
MODE_CUE = "cue"

SHIFT_NOTE = BUTTON_MAP["shift"]


class Push1Surface:
    """
    KNTKTA Remote Script ControlSurface for Akai Push 1.
    Instantiated by Ableton Live via __init__.create_instance().
    """

    def __init__(self, c_instance):
        self._c = c_instance
        self._shift_held = False
        self._active_mode_name = MODE_INSTRUMENT

        # Core subsystems
        self._lcd = LcdDisplay(self._send_sysex)
        self._mapper = ParameterMapper()
        self._macro_engine = MacroEngine()
        self._action_ctx = ActionContext()
        self._action_system = ActionSystem(self._action_ctx)
        self._song = SongStructure()

        self._register_action_handlers()

        # Modes
        self._modes = {
            MODE_INSTRUMENT: InstrumentMode(self),
            MODE_MIXER:      MixerMode(self),
            MODE_SEQUENCER:  SequencerMode(self),
            MODE_SESSION:    SessionMode(self),
            MODE_SAMPLE:     SampleMode(self),
            MODE_LINK:       LinkMode(self),
            MODE_ACTION:     ActionMode(self),
            MODE_SONG:       SongMode(self),
            MODE_CUE:        CueMode(self),
        }

        self._activate_mode(MODE_INSTRUMENT)
        self._lcd.show_message("KNTKTA", "Push 1 Ready")

    # ------------------------------------------------------------------ #
    #  Ableton Live callback hooks
    # ------------------------------------------------------------------ #

    def disconnect(self):
        self._lcd.clear()
        self._active_mode.deactivate()

    def receive_midi(self, midi_bytes):
        """Entry point for all incoming MIDI from Push 1."""
        if not midi_bytes:
            return
        status = midi_bytes[0] & 0xF0
        channel = midi_bytes[0] & 0x0F
        data1 = midi_bytes[1] if len(midi_bytes) > 1 else 0
        data2 = midi_bytes[2] if len(midi_bytes) > 2 else 0

        # Note On/Off
        if status == 0x90:
            self._on_note(data1, data2)
        elif status == 0x80:
            self._on_note(data1, 0)
        # CC
        elif status == 0xB0:
            self._on_cc(data1, data2)

    # ------------------------------------------------------------------ #
    #  MIDI routing
    # ------------------------------------------------------------------ #

    def _on_note(self, note: int, velocity: int):
        pressed = velocity > 0

        # Shift
        if note == SHIFT_NOTE:
            self._shift_held = pressed
            return

        # Mode switching: SHIFT + mode buttons
        if self._shift_held and pressed:
            mode_map = {
                BUTTON_MAP["note"]:    MODE_INSTRUMENT,
                BUTTON_MAP["mix"]:     MODE_MIXER,
                BUTTON_MAP["session"]: MODE_SESSION,
                BUTTON_MAP["device"]:  MODE_SEQUENCER,
                BUTTON_MAP["clip"]:    MODE_SAMPLE,
                BUTTON_MAP["browse"]:  MODE_LINK,
                BUTTON_MAP["user"]:    MODE_ACTION,
                BUTTON_MAP["layout"]:  MODE_SONG,
                BUTTON_MAP["in"]:      MODE_CUE,
            }
            if note in mode_map:
                self._activate_mode(mode_map[note])
                return

        # Delegate to active mode
        self._active_mode.on_note(note, velocity)

    def _on_cc(self, cc: int, value: int):
        # Encoders
        if cc in ENCODER_CC:
            encoder_idx = cc - ENCODER_CC[0]
            # Convert 7-bit relative CC to delta (-1 or +1 at minimum)
            delta = (value - 64) / 64.0 * 0.05
            self._active_mode.on_encoder(encoder_idx, delta)
            return

        # Touch strip
        if cc == TOUCH_STRIP_CC:
            position = value / 127.0
            self._active_mode.on_touch_strip(position)
            return

        self._active_mode.on_cc(cc, value)

    # ------------------------------------------------------------------ #
    #  Mode management
    # ------------------------------------------------------------------ #

    def _activate_mode(self, mode_name: str):
        if hasattr(self, "_active_mode"):
            self._active_mode.deactivate()
        self._active_mode_name = mode_name
        self._active_mode = self._modes[mode_name]
        self._active_mode.activate()
        self._lcd.show_message(f"Mode: {mode_name[:8].upper()}", "")

    def switch_mode(self, mode_name: str):
        """Called programmatically (e.g. from action system or AI)."""
        if mode_name in self._modes:
            self._activate_mode(mode_name)

    # ------------------------------------------------------------------ #
    #  Hardware helpers
    # ------------------------------------------------------------------ #

    def _send_sysex(self, data: bytes):
        self._c.send_midi(tuple(data))

    def set_pad_led(self, note: int, velocity: int):
        self._c.send_midi((0x90, note, velocity))

    def set_button_led(self, note: int, on: bool):
        self._c.send_midi((0x90, note, LED_FULL if on else LED_OFF))

    # ------------------------------------------------------------------ #
    #  Action handler registration
    # ------------------------------------------------------------------ #

    def _register_action_handlers(self):
        ctx = self._action_ctx

        ctx.register("MODE", lambda name: self.switch_mode(name.lower()))
        ctx.register("BANK", lambda idx: setattr(
            self._mapper, "current_bank_index", int(idx)))

        ctx.register("SNAPSHOT", self._handle_snapshot_action)
        ctx.register("MORPH", lambda a, b, pos:
                     list(self._macro_engine.all_groups())[0].morph(
                         int(a), int(b), float(pos))
                     if self._macro_engine.all_groups() else None)

    def _handle_snapshot_action(self, sub_cmd: str, snap_id: str):
        sid = int(snap_id)
        if sub_cmd.upper() == "SAVE":
            for grp in self._macro_engine.all_groups():
                grp.save_snapshot(sid, {})
        elif sub_cmd.upper() == "RECALL":
            pass  # handled by mode

    # ------------------------------------------------------------------ #
    #  Properties for modes to access subsystems
    # ------------------------------------------------------------------ #

    @property
    def lcd(self) -> LcdDisplay:
        return self._lcd

    @property
    def mapper(self) -> ParameterMapper:
        return self._mapper

    @property
    def macro_engine(self) -> MacroEngine:
        return self._macro_engine

    @property
    def action_system(self) -> ActionSystem:
        return self._action_system

    @property
    def song(self) -> SongStructure:
        return self._song

    @property
    def ableton(self):
        """Reference to Ableton Live's application object."""
        return self._c.application()
