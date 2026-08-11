# BinaryGlitch

BinaryGlitch is a JUCE audio effect that turns a binary byte stream into a broken digital texture using ticked transport, frame corruption, burst faults, comb structure, drive, and high-pass filtering.

The plug-in can use an internal deterministic byte source or a loaded binary file. The editor intentionally wraps JUCE's generic parameter editor.

## Identity

- Company: EsionHsrahLatigid
- Manufacturer code: EHL_
- Plug-in code: Bgch
- Bundle ID: jp.ehl.binaryglitch
- Formats: VST3, Standalone, and AU on macOS

## Build

Use a local JUCE checkout when available:

```bash
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DBINARYGLITCH_JUCE_PATH=/path/to/JUCE
cmake --build build/release --target BinaryGlitch_Artifacts --parallel
ctest --test-dir build/release --output-on-failure
```

If `BINARYGLITCH_JUCE_PATH` is empty, CMake fetches JUCE 8.0.15.

Staged products are written to:

- `artifacts/Release/VST3/`
- `artifacts/Release/AU/` on macOS
- `artifacts/Release/Standalone/`
