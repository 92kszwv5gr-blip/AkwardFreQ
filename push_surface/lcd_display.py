"""
lcd_display.py — Push 1 LCD display driver.

Manages both rows of the Push 1's 2×28 character LCD display.
Handles segmented display (8 segments × 3.5 chars per encoder),
full-row text, and scrolling.
"""
from .push1_hardware import make_sysex_display, LCD_COLS, LCD_ROWS

# Each encoder occupies 3 chars + 1 separator = 3.5 wide (rounded to 4 per segment)
SEGMENT_WIDTH = 4
NUM_SEGMENTS = 8


class LcdDisplay:
    """
    Manages the Push 1 2-line LCD.

    row0: parameter names (top)
    row1: parameter values (bottom)
    """

    def __init__(self, send_sysex_fn):
        """
        send_sysex_fn: callable that takes bytes and sends to Push 1.
        Typically c_instance.send_midi with the sysex bytes.
        """
        self._send = send_sysex_fn
        self._rows = ["" * LCD_COLS, "" * LCD_COLS]

    def write_row(self, row: int, text: str):
        """Write a full row string (up to 28 chars) to the LCD."""
        assert 0 <= row < LCD_ROWS
        padded = text[:LCD_COLS].ljust(LCD_COLS)
        if padded != self._rows[row]:
            self._rows[row] = padded
            self._send(make_sysex_display(row, padded))

    def write_segment(self, encoder_idx: int, name: str, value: str):
        """
        Write name (row 0) and value (row 1) for a single encoder segment.
        encoder_idx: 0–7
        """
        assert 0 <= encoder_idx < NUM_SEGMENTS
        start = encoder_idx * SEGMENT_WIDTH

        row0 = list(self._rows[0].ljust(LCD_COLS))
        row1 = list(self._rows[1].ljust(LCD_COLS))

        name_seg = name[:SEGMENT_WIDTH].ljust(SEGMENT_WIDTH)
        val_seg = value[:SEGMENT_WIDTH].ljust(SEGMENT_WIDTH)

        row0[start:start + SEGMENT_WIDTH] = list(name_seg)
        row1[start:start + SEGMENT_WIDTH] = list(val_seg)

        self._rows[0] = "".join(row0)
        self._rows[1] = "".join(row1)

        self._send(make_sysex_display(0, self._rows[0]))
        self._send(make_sysex_display(1, self._rows[1]))

    def write_all_segments(self, names: list, values: list):
        """
        Write all 8 encoder name/value pairs at once.
        names, values: lists of up to 8 strings
        """
        row0 = ""
        row1 = ""
        for i in range(NUM_SEGMENTS):
            name = (names[i] if i < len(names) else "")[:SEGMENT_WIDTH].ljust(SEGMENT_WIDTH)
            value = (values[i] if i < len(values) else "")[:SEGMENT_WIDTH].ljust(SEGMENT_WIDTH)
            row0 += name
            row1 += value
        self.write_row(0, row0)
        self.write_row(1, row1)

    def clear(self):
        """Clear both LCD rows."""
        self.write_row(0, "")
        self.write_row(1, "")

    def show_message(self, line0: str, line1: str = ""):
        """Display a two-line message, centred."""
        self.write_row(0, line0[:LCD_COLS].center(LCD_COLS))
        self.write_row(1, line1[:LCD_COLS].center(LCD_COLS))
