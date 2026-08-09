"""
sound_forge/resampler/midi_note_sender.py — MIDI note trigger with timing.

Sends MIDI note on/off to a named MIDI output port.
Uses python-rtmidi for cross-platform MIDI output.
"""


class MidiNoteSender:
    """
    Sends MIDI Note On/Off messages to a MIDI output port.

    port_name: Name of the MIDI output port (as seen by rtmidi).
                Pass None to use the first available port.
    channel:   MIDI channel (1–16, default 1)
    """

    def __init__(self, port_name: str = None, channel: int = 1):
        self.port_name = port_name
        self.channel = max(1, min(16, channel))
        self._midi_out = None
        self._open_port()

    def _open_port(self):
        try:
            import rtmidi
            self._midi_out = rtmidi.MidiOut()
            ports = self._midi_out.get_ports()
            if not ports:
                print("[MidiNoteSender] No MIDI output ports available")
                self._midi_out = None
                return
            if self.port_name:
                for i, name in enumerate(ports):
                    if self.port_name.lower() in name.lower():
                        self._midi_out.open_port(i)
                        return
                print(f"[MidiNoteSender] Port '{self.port_name}' not found, using first")
            self._midi_out.open_port(0)
        except ImportError:
            print("[MidiNoteSender] rtmidi not installed. Run: pip install python-rtmidi")
            self._midi_out = None

    def note_on(self, note: int, velocity: int = 100):
        """Send MIDI Note On."""
        if self._midi_out:
            ch = self.channel - 1
            self._midi_out.send_message([0x90 | ch, note & 0x7F, velocity & 0x7F])

    def note_off(self, note: int):
        """Send MIDI Note Off."""
        if self._midi_out:
            ch = self.channel - 1
            self._midi_out.send_message([0x80 | ch, note & 0x7F, 0])

    def close(self):
        if self._midi_out:
            self._midi_out.close_port()
            self._midi_out = None

    def list_ports(self) -> list:
        try:
            import rtmidi
            m = rtmidi.MidiOut()
            return m.get_ports()
        except ImportError:
            return []
