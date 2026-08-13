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
cmake --build build/release --target ehl_stage_products --parallel
ctest --test-dir build/release --output-on-failure
```

If `BINARYGLITCH_JUCE_PATH` is empty, CMake fetches JUCE 8.0.15.

Staged products are written to:

- `artifacts/plugin-release/macos-arm64/` on macOS
- `artifacts/plugin-release/windows-x64/` on Windows
- `artifacts/plugin-release/linux-x64/` on Linux

On local macOS builds outside CI, VST3 and AU bundles are also copied after
build to the current user's standard plug-in folders:

- `~/Library/Audio/Plug-Ins/VST3`
- `~/Library/Audio/Plug-Ins/Components`

Standalone products remain only in the build or staged artifact tree; they are
not copied under `Audio/Plug-Ins`. CI and non-macOS builds default this copying
off. Override explicitly with `-DEHL_COPY_PLUGIN_AFTER_BUILD=ON|OFF`.
