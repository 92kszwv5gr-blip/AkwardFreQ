"""
push1_hardware.py — Akai Push 1 MIDI/SysEx constants and low-level hardware driver.

Push 1 MIDI layout:
  Pads:        Notes 36–99  (8×8 grid, bottom-left = 36)
  Encoders:    CC 71–78     (8 touch encoders)
  Touch strip: CC 1
  Tempo knob:  CC 14
  Buttons:     Various notes / CCs (see BUTTON_MAP)
  LCD:         SysEx F0 47 7F 15 ...

SysEx manufacturer prefix: F0 47 7F 15
"""

SYSEX_HEADER = (0xF0, 0x47, 0x7F, 0x15)
SYSEX_FOOTER = (0xF7,)

# Display command IDs
SYSEX_CMD_DISPLAY_ROW0 = 0x18
SYSEX_CMD_DISPLAY_ROW1 = 0x19

# Pad note range
PAD_NOTE_MIN = 36
PAD_NOTE_MAX = 99
PAD_COLS = 8
PAD_ROWS = 8

# Encoder CCs
ENCODER_CC = list(range(71, 79))  # CC 71–78
TOUCH_STRIP_CC = 1
TEMPO_CC = 14

# LED velocity values (Push 1 uses velocity for pad colour intensity)
LED_OFF = 0
LED_DIM = 1
LED_MID = 2
LED_FULL = 3
LED_BLINK = 4

# Clip state colours (velocity-mapped)
CLIP_EMPTY = LED_OFF
CLIP_HAS_CLIP = LED_DIM
CLIP_PLAYING = LED_FULL
CLIP_RECORDING = LED_BLINK
CLIP_QUEUED = LED_MID

# Button MIDI notes
BUTTON_MAP = {
    "play":           85,
    "record":         86,
    "automate":       89,
    "fixed_length":   90,
    "new":            87,
    "duplicate":      88,
    "quantize":       116,
    "delete":         118,
    "undo":           119,
    "convert":        35,
    "double_loop":    117,
    "tap_tempo":      3,
    "metronome":      9,
    "add_effect":     52,
    "add_track":      53,
    "master":         28,
    "setup":          30,
    "user":           59,
    "note":           50,
    "session":        51,
    "device":         110,
    "browse":         111,
    "clip":           112,
    "mix":            113,
    "left_arrow":     44,
    "right_arrow":    45,
    "up_arrow":       46,
    "down_arrow":     47,
    "octave_down":    54,
    "octave_up":      55,
    "shift":          49,
    "select":         48,
    "in":             12,
    "out":            13,
    "repeat":         56,
    "accent":         57,
    "scales":         58,
    "layout":         31,
    "left_track":     20,
    "right_track":    21,
    "mute":           60,
    "solo":           61,
    "stop_clip":      29,
}

# Track select buttons (top row above pads)
TRACK_SELECT_NOTES = list(range(20, 28))

# Scene launch buttons (right column)
SCENE_LAUNCH_NOTES = list(range(36, 44))

# Display dimensions
LCD_ROWS = 2
LCD_COLS = 28
LCD_MAX_CHAR = 28


def make_sysex_display(row: int, text: str) -> bytes:
    """
    Build a SysEx message to write text to Push 1's LCD.

    row: 0 or 1
    text: string up to LCD_MAX_CHAR characters
    """
    cmd = SYSEX_CMD_DISPLAY_ROW0 if row == 0 else SYSEX_CMD_DISPLAY_ROW1
    padded = text[:LCD_MAX_CHAR].ljust(LCD_MAX_CHAR)
    data = [b & 0x7F for b in padded.encode("ascii", errors="replace")]
    msg = list(SYSEX_HEADER) + [cmd, len(data)] + data + list(SYSEX_FOOTER)
    return bytes(msg)


def pad_note_to_xy(note: int):
    """Convert a pad MIDI note (36–99) to (col, row) zero-indexed."""
    offset = note - PAD_NOTE_MIN
    col = offset % PAD_COLS
    row = offset // PAD_COLS
    return col, row


def xy_to_pad_note(col: int, row: int) -> int:
    """Convert (col, row) to pad MIDI note."""
    return PAD_NOTE_MIN + row * PAD_COLS + col
