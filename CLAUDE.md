# AkwardFreQ: instructions for Claude sessions

A VST3 plugin for Ableton (Windows). C++17 on JUCE 7.0.12, with HT-Demucs stem separation run through
ONNX Runtime. Developed by PsykoDogoa. `README.md` describes the product; this file says how to work on it.

## Rules that matter most here

### Verify before you claim

- Do not say a build passes, a bug is fixed or something works until you ran the check in this turn and read its output.
- An exit code is not proof. `cmd > log; echo "EXIT=$?" >> log` and a background task reporting "exit code 0"
  say nothing about `cmd` itself. Read the end of the log and run `grep -c "error:" log`. This misled us twice.
- A clean compile does not prove the app runs. For anything that touches the UI, run `scripts/dev-env.sh smoke`
  and look at the screenshot.
- Non-ASCII characters (an em-dash) in a string literal passed to `juce::String` show as garbage in the UI. Use plain
  ASCII, or `juce::CharPointer_UTF8`.
- `pkill -f <pattern>` can match and kill its own shell (exit 143/144). Use `pkill -x AkwardFreQ` or a saved PID.
- Say what you could not check. Untested is a fact worth reporting; "should work" is not.

### Debug from the root cause

- Reproduce first and read the whole error. Then isolate: for a header or compiler problem, write a minimal file that
  includes just that header (this is how the ONNX Runtime and mingw problems were found).
- For multi-stage failures (configure, compile, link, post-build step) find which stage fails before changing
  anything, and log at each stage boundary.
- One change at a time; rerun the same check after each.

### Tests

Automated tests live in `tests/` (JUCE `UnitTest`, one console runner, `AkwardFreQTests`). They cover the slicers,
loop snapping, audio-to-MIDI, the one-shot cleaner and the mastering chain, and need no model or audio device:

    cmake -B build-test -DAFQ_BUILD_TESTS=ON -DONNXRUNTIME_ROOT_DIR=/tmp/onnxruntime-linux/onnxruntime-linux-x64-1.20.1 \
          -DCMAKE_BUILD_TYPE=Debug -DCOPY_PLUGIN_AFTER_BUILD=OFF
    cmake --build build-test --target AkwardFreQTests -j4 > /tmp/build-test.log 2>&1; tail -2 /tmp/build-test.log; grep -c "error:" /tmp/build-test.log
    ctest --test-dir build-test --output-on-failure        # or run the binary directly; optional argument filters by name

- The runner exits 1 on any failure and 2 if no tests matched or no checks ran. Read the last line ("N test groups: X checks
  passed, Y failed"), not just the exit code.
- Write the failing test first and watch it fail. To prove a test can fail, revert the fix, rebuild and rerun.
- `KNOWN ISSUE` lines are real defects recorded with `knownIssue()` (in `tests/TestUtils.h`). They count as nothing while the
  behaviour is wrong and the test fails when it starts holding, so fixing the code forces you to promote the check to a real one.
  Current ones: onset detector over-triggers on noisy material, DrumSlicer therefore over-slices, transcription of a 3-note melody
  gives 14 notes, the limiter is not a true ceiling (peaks reach 0 dBFS), the loudness meter reads stereo about 3 dB low.
- `tests/reference/*.txt` pin the current mastering output (0.1 dB tolerance). They record behaviour, not correctness.
  After an intended change: `AFQ_UPDATE_REFERENCE=1 ./AkwardFreQTests Mastering`, review the diff, commit it.
- `build-test/` is git-ignored. Not covered yet: the separation engine, the UI, the plugin processor.

## Set up a session

Cloud sessions are reset between conversations: `/tmp` and installed tools disappear. First:

    scripts/dev-env.sh setup     # root + network; idempotent
    scripts/dev-env.sh check     # must end with "everything present"

`setup` installs mingw-w64, Wine, Xvfb, xdotool, ImageMagick, patchelf and the X11/GTK dev headers (the native
`juceaide` tool needs them even for the Windows cross-build), fixes the mingw `Windows.h` case problem, and downloads
ONNX Runtime 1.20.1 for Linux and Windows into `/tmp`.

## Build

Linux Standalone (Debug, for testing without a DAW):

    cmake -B build -DONNXRUNTIME_ROOT_DIR=/tmp/onnxruntime-linux/onnxruntime-linux-x64-1.20.1 \
          -DCMAKE_BUILD_TYPE=Debug -DCOPY_PLUGIN_AFTER_BUILD=OFF
    cmake --build build --target AkwardFreQ_Standalone -j4 > /tmp/build.log 2>&1
    tail -3 /tmp/build.log; grep -c "error:" /tmp/build.log

There are 4 cores. Configure takes about 2.5 minutes and a full build about 10, so use `run_in_background`.
To skip re-downloading JUCE add `-DFETCHCONTENT_SOURCE_DIR_JUCE=<path to an existing _deps/juce-src>`.

Windows VST3 and Standalone (mingw-w64 cross-build, Release, needs `toolchain-mingw64.cmake`):

    cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=toolchain-mingw64.cmake \
          -DONNXRUNTIME_ROOT_DIR=/tmp/onnxruntime-win/onnxruntime-win-x64-1.20.1 \
          -DCMAKE_BUILD_TYPE=Release -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF
    cp /tmp/onnxruntime-win/onnxruntime-win-x64-1.20.1/lib/onnxruntime.dll build-win/
    Xvfb :77 & DISPLAY=:77 cmake --build build-win -j4 > /tmp/build-win.log 2>&1

- The last CMake step runs `juce_vst3_helper.exe` under Wine to write `moduleinfo.json`. It needs `wine-quiet`
  (installed by `setup`), a running X display and `onnxruntime.dll` in `build-win/`. If it still fails with
  "juce_vst3_helper: No such file", run it by hand:
  `DISPLAY=:77 WINEDEBUG=-all wine build-win/juce_vst3_helper.exe -create -version 0.1.0 -path build-win/AkwardFreQ_artefacts/Release/VST3/AkwardFreQ.vst3 -output build-win/AkwardFreQ_artefacts/Release/VST3/AkwardFreQ.vst3/Contents/Resources/moduleinfo.json`.
  The `.vst3` itself is already linked at that point.
- Verify the result: `file` must say `PE32+`, and `x86_64-w64-mingw32-objdump -p <vst3> | grep "DLL Name"` must list
  only Windows system DLLs plus `onnxruntime.dll` (no `libgcc`, `libstdc++` or `libwinpthread`).
- ONNX Runtime's headers need `Source/separation/OnnxMingwShim.h` under mingw. Keep it included before them.

## The AI model

`python tools/export_demucs_onnx.py --output <path>/htdemucs.onnx` writes about 300 MB (opset 18; the older default 17 fails).
In a fresh virtual environment install CPU-only PyTorch first, to avoid multi-gigabyte CUDA wheels:
`pip install torch torchaudio --index-url https://download.pytorch.org/whl/cpu`, then `pip install -r tools/requirements.txt`.
Put the file in `Models/` next to the executable. Separating a 13 s track takes about a minute in a Debug build.
`python tools/make_test_track.py /tmp/test_track.wav` writes a synthetic test track (needs numpy).

## Testing the UI headless

`scripts/dev-env.sh smoke [binary] [out.png]` starts the app under Xvfb and checks that a window appears, the process
survives and the screenshot is not blank. For deeper checks, drive it with xdotool at 1280x800:

- Tab positions in the default skin, click at y=112: Split x=153, Master 213, Export 275, Instrument 343, Drum Chop 422, MIDI 493.
- Import Track opens a file dialog: click the `file:` field first, type the path, then click Open. Pressing Return does nothing.
- Capture with `import -window root out.png`, then read the image. Never assume what the UI looks like.

## Code rules

- The audio callback (`processBlock`) must not allocate or lock. Separation runs on a background thread.
- JUCE is 7.0.12: `juce::Font (size, style)`, not the JUCE 8 `FontOptions`.
- `build/` and `build-win/` are git-ignored. Never reuse one build directory across different source trees.

## Git, repos and delivery

- Work on branch `claude/vst3-ableton-plugin-csjqyw`. Remotes: `origin` is `92kszwv5gr-blip/AkwardFreQ`, and `psykodogoa` is
  `92kszwv5gr-blip/psykodogoa-vst3` (shown on GitHub as `PsykoDogoa-VST3`; the "repository moved" line on push is harmless).
- After a change, push both: `git push origin claude/vst3-ableton-plugin-csjqyw` and
  `git push psykodogoa claude/vst3-ableton-plugin-csjqyw:main`.
- Design experiments live in the separate repo `PsykoDogoa-VST3-design-lab`. Do not push design work here.
- The GitHub connector cannot create repositories. The owner creates an empty repo and gives the Claude GitHub app access
  to it, then use `add_repo`.
- Files sent to the user through `SendUserFile` are limited to 30 MiB. The release zips (18 and 22 MB) fit; the model does not.

## Releases

`releases/AkwardFreQ-Windows.zip` holds the `.vst3` folder, the Standalone `.exe`, `onnxruntime.dll` and a README.
`releases/AkwardFreQ-Linux-Standalone.zip` holds a stripped binary (`strip --strip-all`), `patchelf --set-rpath '$ORIGIN/lib'`,
`libonnxruntime.so*` and a README. Neither includes the model. Rebuild both after any user-visible fix: the zips currently
predate the text and export-script fixes (commit `e42e43d`).
