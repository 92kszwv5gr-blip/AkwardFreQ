# kntkta_core — Main Application Container

The central hub that wires all KNTKTA modules together.

## Targets

| Target | Format | Description |
|---|---|---|
| `KNTKTAStandalone` | Binary | Standalone desktop app |
| `KNTKTAVST3` | VST3 | VST3 plugin for any DAW |
| `KNTKTEAU` | AU | Audio Unit for macOS/Logic |
| `KNTKTACLAP` | CLAP | CLAP plugin (open standard) |

## Build Requirements

- JUCE 7.x
- CMake 3.22+
- C++17
- macOS: Xcode 14+ / Windows: MSVC 2022 / Linux: GCC 12+

## Build Instructions

```bash
cd kntkta_core
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Source Structure

```
kntkta_core/
├── CMakeLists.txt
├── Source/
│   ├── Core/
│   │   ├── KntktaProcessor.h/.cpp    # AudioProcessor (VST/AU/CLAP)
│   │   ├── PresetManager.h/.cpp      # Bridges to preset_forge
│   │   ├── LinkBridge.h/.cpp         # Bridges to link_bridge
│   │   ├── AiClient.h/.cpp           # MCP AI client
│   │   └── Database.h/.cpp           # SQLite wrapper
│   └── GUI/
│       ├── MainComponent.h/.cpp      # Main UI
│       ├── LayoutEditor.h/.cpp       # Push 1 parameter drag-drop editor
│       ├── BankManager.h/.cpp        # Bank/preset browser
│       ├── SampleForgePanel.h/.cpp   # Sample engine UI
│       └── LinkPanel.h/.cpp          # Network devices panel
├── database/
│   └── schema.sql                    # SQLite schema
└── electron/                         # Optional Electron shell
    ├── package.json
    ├── src/
    │   └── main.js
    └── public/
        └── index.html
```
