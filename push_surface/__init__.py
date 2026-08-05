"""
push_surface — KNTKTA Universal Remote Script for Akai Push 1
Entry point loaded by Ableton Live.
"""
from .push1_surface import Push1Surface


def create_instance(c_instance):
    """Called by Ableton Live to instantiate the control surface."""
    return Push1Surface(c_instance)
