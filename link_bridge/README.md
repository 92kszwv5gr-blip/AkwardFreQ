# link_bridge — Ableton Link + iOS/Network Integration

Wireless, networked parameter universe for KNTKTA.

## Architecture

```
link_bridge/
├── link_engine.py        # Ableton Link C++ wrapper (via ctypes/cffi)
├── device_registry.py    # Discovered device registry
├── osc/
│   ├── osc_server.py     # OSC UDP server (parameter receive)
│   └── osc_client.py     # OSC UDP client (parameter send)
├── rtpmidi/
│   └── rtpmidi_bridge.py # RTP-MIDI (Network MIDI) bridge
├── bluetooth/
│   └── ble_midi.py       # Bluetooth LE MIDI bridge
├── ios_companion/        # Swift iOS companion app
│   └── Sources/KNTKTACompanion/
│       ├── ContentView.swift
│       ├── LinkManager.swift
│       ├── ParameterBridge.swift
│       └── KNTKTACompanionApp.swift
└── tests/
```

## Sync Modes

| Mode | Description |
|---|---|
| Tempo only | Sync BPM across all Link devices |
| Tempo + Phase | Sync BPM + beat phase (quantised launches) |
| Full parameter sync | Tempo + Phase + parameter read/write over OSC |

## Protocols

| Protocol | Use case |
|---|---|
| Ableton Link | Tempo/phase sync across all devices |
| OSC over UDP | Parameter control to/from iOS apps, DAWs |
| RTP-MIDI | Full MIDI over network (macOS native support) |
| Bluetooth LE MIDI | Wireless MIDI to iOS devices |

## iOS Companion App

Lightweight Swift app that:
- Joins the Ableton Link session
- Exposes device parameters over OSC
- Receives parameter changes from Push 1

Requires: iOS 14+, same WiFi network as KNTKTA host
