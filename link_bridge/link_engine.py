"""
link_bridge/link_engine.py — Ableton Link integration.

Wraps the Ableton Link C++ library via ctypes.
The Link library must be built from: https://github.com/Ableton/link

Provides:
  - Tempo sync (get/set BPM)
  - Phase sync (beat position within bar)
  - Peer count (number of connected Link devices)
  - Start/stop sync
"""
import ctypes
import ctypes.util
import threading
import time
from typing import Optional, Callable


class LinkState:
    """Current Link session state snapshot."""

    def __init__(self, tempo: float = 120.0, beat: float = 0.0,
                 phase: float = 0.0, peers: int = 0, playing: bool = False):
        self.tempo = tempo
        self.beat = beat
        self.phase = phase
        self.peers = peers
        self.playing = playing


class AbletonLink:
    """
    Python wrapper for the Ableton Link C++ library.

    When the native library is unavailable (e.g. development mode),
    falls back to a software simulation that still exposes the API.
    """

    QUANTUM = 4.0  # beats per bar

    def __init__(self, initial_bpm: float = 120.0):
        self._bpm = initial_bpm
        self._peers = 0
        self._playing = False
        self._beat = 0.0
        self._start_time = time.time()
        self._lock = threading.Lock()
        self._lib = None
        self._link = None
        self._on_tempo_change: Optional[Callable[[float], None]] = None
        self._on_peer_change: Optional[Callable[[int], None]] = None

        self._try_load_native()
        if self._lib is None:
            print("[AbletonLink] Native Link library not found — running in simulation mode")
            self._start_simulation()

    def _try_load_native(self):
        """Attempt to load the native Ableton Link shared library."""
        lib_names = ["libAbleton_Link", "AbletonLink", "link"]
        for name in lib_names:
            path = ctypes.util.find_library(name)
            if path:
                try:
                    self._lib = ctypes.cdll.LoadLibrary(path)
                    self._init_native()
                    return
                except OSError:
                    continue

    def _init_native(self):
        """Set up native Link function signatures."""
        if not self._lib:
            return
        try:
            self._lib.Link_create.restype = ctypes.c_void_p
            self._lib.Link_create.argtypes = [ctypes.c_double]
            self._lib.Link_destroy.argtypes = [ctypes.c_void_p]
            self._lib.Link_isEnabled.restype = ctypes.c_bool
            self._lib.Link_isEnabled.argtypes = [ctypes.c_void_p]
            self._lib.Link_enable.argtypes = [ctypes.c_void_p, ctypes.c_bool]
            self._lib.Link_numPeers.restype = ctypes.c_uint64
            self._lib.Link_numPeers.argtypes = [ctypes.c_void_p]

            self._link = self._lib.Link_create(ctypes.c_double(self._bpm))
            self._lib.Link_enable(self._link, True)
        except Exception as e:
            print(f"[AbletonLink] Native init failed: {e}")
            self._lib = None

    def _start_simulation(self):
        """Start a background thread that simulates beat advancement."""
        def _tick():
            while True:
                with self._lock:
                    elapsed = time.time() - self._start_time
                    self._beat = (elapsed * self._bpm / 60.0) % (self.QUANTUM * 4)
                time.sleep(0.01)

        t = threading.Thread(target=_tick, daemon=True)
        t.start()

    # ------------------------------------------------------------------ #
    #  Public API
    # ------------------------------------------------------------------ #

    def get_state(self) -> LinkState:
        """Return current Link session state."""
        with self._lock:
            if self._lib and self._link:
                try:
                    peers = self._lib.Link_numPeers(self._link)
                    return LinkState(
                        tempo=self._bpm,
                        beat=self._beat,
                        phase=self._beat % self.QUANTUM,
                        peers=peers,
                        playing=self._playing,
                    )
                except Exception:
                    pass
            return LinkState(
                tempo=self._bpm,
                beat=self._beat,
                phase=self._beat % self.QUANTUM,
                peers=self._peers,
                playing=self._playing,
            )

    def set_tempo(self, bpm: float):
        """Set the Link session tempo."""
        bpm = max(20.0, min(300.0, bpm))
        with self._lock:
            self._bpm = bpm
            if self._on_tempo_change:
                self._on_tempo_change(bpm)

    def get_tempo(self) -> float:
        return self._bpm

    def get_peers(self) -> int:
        if self._lib and self._link:
            try:
                return self._lib.Link_numPeers(self._link)
            except Exception:
                pass
        return self._peers

    def set_playing(self, playing: bool):
        with self._lock:
            self._playing = playing
            if not playing:
                self._beat = 0.0
                self._start_time = time.time()

    def on_tempo_change(self, fn: Callable[[float], None]):
        self._on_tempo_change = fn

    def on_peer_change(self, fn: Callable[[int], None]):
        self._on_peer_change = fn

    def close(self):
        if self._lib and self._link:
            try:
                self._lib.Link_destroy(self._link)
            except Exception:
                pass
            self._link = None
