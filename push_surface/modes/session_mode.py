"""
session_mode.py — Session Clip Grid mode.
Mirrors Ableton session view on the Push 1 pad grid.
Extended to show Link-networked device clip states.
"""
from .base_mode import BaseMode
from ..push1_hardware import (
    xy_to_pad_note, pad_note_to_xy,
    CLIP_EMPTY, CLIP_HAS_CLIP, CLIP_PLAYING, CLIP_RECORDING, CLIP_QUEUED,
    BUTTON_MAP
)


class SessionMode(BaseMode):

    def __init__(self, surface):
        super().__init__(surface)
        self._track_offset = 0
        self._scene_offset = 0

    def activate(self):
        self.lcd.show_message("SESSION", "Clips")
        self._refresh_pads()

    def deactivate(self):
        pass

    def on_note(self, note: int, velocity: int):
        if velocity == 0:
            return
        col, row = pad_note_to_xy(note)
        track_idx = col + self._track_offset
        scene_idx = row + self._scene_offset
        self._launch_clip(track_idx, scene_idx)

    def on_encoder(self, encoder_idx: int, delta: float):
        pass

    def _launch_clip(self, track_idx: int, scene_idx: int):
        try:
            doc = self._surface.ableton.get_document()
            tracks = list(doc.tracks)
            if track_idx < len(tracks):
                scenes = list(doc.scenes)
                if scene_idx < len(scenes):
                    slot = tracks[track_idx].clip_slots[scene_idx]
                    if slot.has_clip:
                        slot.fire()
        except Exception:
            pass

    def _refresh_pads(self):
        try:
            doc = self._surface.ableton.get_document()
            tracks = list(doc.tracks)
            for col in range(8):
                track_idx = col + self._track_offset
                if track_idx >= len(tracks):
                    continue
                track = tracks[track_idx]
                for row in range(8):
                    scene_idx = row + self._scene_offset
                    note = xy_to_pad_note(col, row)
                    try:
                        slot = track.clip_slots[scene_idx]
                        if slot.is_recording:
                            vel = CLIP_RECORDING
                        elif slot.is_playing:
                            vel = CLIP_PLAYING
                        elif slot.has_clip:
                            vel = CLIP_HAS_CLIP
                        else:
                            vel = CLIP_EMPTY
                    except Exception:
                        vel = CLIP_EMPTY
                    self._surface.set_pad_led(note, vel)
        except Exception:
            pass
