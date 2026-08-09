"""
preset_forge/parsers/__init__.py — Parser registry.
"""
from .fxb_parser import FxbParser, FxpParser
from .nki_parser import NkiParser
from .ableton_parser import AbletonAdvParser, AbletonAdgParser
from .vstpreset_parser import VstPresetParser
from .sysex_parser import SysexParser

# Map file extension → parser class
PARSER_REGISTRY = {
    ".fxb":        FxbParser,
    ".fxp":        FxpParser,
    ".nki":        NkiParser,
    ".adv":        AbletonAdvParser,
    ".adg":        AbletonAdgParser,
    ".vstpreset":  VstPresetParser,
    ".mid":        SysexParser,
}


def get_parser(extension: str):
    """Return the parser class for a given file extension, or None."""
    return PARSER_REGISTRY.get(extension.lower())
