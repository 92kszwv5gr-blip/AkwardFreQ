"""
song_structure.py — Song/Set Structure Manager.

Define named song sections (Intro, Verse, Drop, Break, Outro) each with:
  - An Ableton scene index to launch
  - A target tempo
  - A parameter snapshot to recall
  - Link broadcast: triggers matching scene on all networked devices

Navigation from Push 1: advance/retreat through sections via buttons.
"""
from typing import Optional, Callable


class SongSection:
    """One section of a song."""

    def __init__(self, section_id: int, name: str,
                 scene_index: int = 0,
                 tempo: Optional[float] = None,
                 snapshot_id: Optional[int] = None,
                 color: int = 1):
        self.section_id = section_id
        self.name = name[:16]
        self.display_name = name[:8]
        self.scene_index = scene_index
        self.tempo = tempo            # None = keep current tempo
        self.snapshot_id = snapshot_id
        self.color = color            # LED colour index


class SongStructure:
    """
    Ordered list of song sections with push-button navigation.
    """

    def __init__(self):
        self.sections: list[SongSection] = []
        self.current_index: int = 0
        # Callbacks registered by push1_surface
        self._on_section_change: Optional[Callable[[SongSection], None]] = None
        self._on_tempo_change: Optional[Callable[[float], None]] = None
        self._on_scene_launch: Optional[Callable[[int], None]] = None
        self._on_link_broadcast: Optional[Callable[[SongSection], None]] = None

    def add_section(self, section: SongSection):
        self.sections.append(section)

    def remove_section(self, section_id: int):
        self.sections = [s for s in self.sections if s.section_id != section_id]

    @property
    def current_section(self) -> Optional[SongSection]:
        if not self.sections:
            return None
        return self.sections[self.current_index]

    def advance(self):
        """Move to next section and trigger it."""
        if not self.sections:
            return
        self.current_index = (self.current_index + 1) % len(self.sections)
        self._trigger_current()

    def retreat(self):
        """Move to previous section and trigger it."""
        if not self.sections:
            return
        self.current_index = (self.current_index - 1) % len(self.sections)
        self._trigger_current()

    def jump_to(self, section_id: int):
        """Jump directly to a section by ID."""
        for i, s in enumerate(self.sections):
            if s.section_id == section_id:
                self.current_index = i
                self._trigger_current()
                return

    def _trigger_current(self):
        sec = self.current_section
        if not sec:
            return
        if self._on_section_change:
            self._on_section_change(sec)
        if sec.tempo is not None and self._on_tempo_change:
            self._on_tempo_change(sec.tempo)
        if self._on_scene_launch:
            self._on_scene_launch(sec.scene_index)
        if self._on_link_broadcast:
            self._on_link_broadcast(sec)

    def on_section_change(self, fn: Callable):
        self._on_section_change = fn

    def on_tempo_change(self, fn: Callable):
        self._on_tempo_change = fn

    def on_scene_launch(self, fn: Callable):
        self._on_scene_launch = fn

    def on_link_broadcast(self, fn: Callable):
        self._on_link_broadcast = fn

    def get_display_list(self) -> list:
        """Return list of (name, is_current) for display."""
        return [
            (s.display_name, i == self.current_index)
            for i, s in enumerate(self.sections)
        ]

    def to_dict(self) -> dict:
        return {
            "sections": [
                {
                    "section_id": s.section_id,
                    "name": s.name,
                    "scene_index": s.scene_index,
                    "tempo": s.tempo,
                    "snapshot_id": s.snapshot_id,
                    "color": s.color,
                }
                for s in self.sections
            ],
            "current_index": self.current_index,
        }

    @classmethod
    def from_dict(cls, data: dict) -> "SongStructure":
        ss = cls()
        for i, sd in enumerate(data.get("sections", [])):
            ss.add_section(SongSection(
                sd.get("section_id", i),
                sd.get("name", f"Section {i}"),
                sd.get("scene_index", 0),
                sd.get("tempo"),
                sd.get("snapshot_id"),
                sd.get("color", 1),
            ))
        ss.current_index = data.get("current_index", 0)
        return ss
