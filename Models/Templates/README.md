# Models/Templates/

`AbletonPresetWriter` patches a real Ableton-exported preset rather than
generating one from scratch (Ableton's `.adv`/`.adg` schema is undocumented
and strict — patching known-valid files is much more reliable than a
hand-built XML tree). To use the "Export to Ableton Simpler" option, put a
template here once:

1. In Ableton, drag any audio sample onto a track to create a Simpler device
   with that sample loaded.
2. Right-click the Simpler device header -> **Save Preset**, or drag it into
   your User Library.
3. Find the saved `.adv` file (typically under
   `Documents/Ableton/User Library/Presets/Instruments/Simpler/`) and copy it
   here as `simpler_template.adv`.

```
Models/Templates/
  simpler_template.adv   # required for "Export to Ableton Simpler"
```

Without this file, Simpler export fails with a clear error and falls back to
SFZ / folder export, which always work regardless.

## Why there's no Drum Rack (.adg) template/export yet

Same idea would work for Drum Racks in principle, but a Drum Rack preset's
XML is a good deal more structurally complex (128 pad slots, nested chains),
and guessing that structure wrong risks silently producing a broken file —
worse than not having the feature. Folder export of chopped/sliced drum hits
(named, prefixed .wav files) is the reliable path for now; if you send a
`.adg` you've exported from Ableton (or its decompressed XML — gzip -d
works on it directly, it's just gzip-compressed XML), that's what's needed to
build genuine Drum Rack export against a real reference instead of guessing.
