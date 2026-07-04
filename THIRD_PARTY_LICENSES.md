# Third-party licenses

PPsVaultTracker is licensed under the **GNU AGPLv3** (see [LICENSE](LICENSE)).
No third-party code is vendored in this repository.

## Current dependencies

### JUCE 8 (pinned 8.0.14)

- **What**: C++ framework — audio engine, GUI, VST3 hosting.
- **License**: dual-licensed; used here under the **AGPLv3** option.
- **How**: fetched by CMake FetchContent at configure time, never committed.
- **Source**: https://github.com/juce-framework/JUCE

### VST3 SDK (Steinberg, bundled inside JUCE)

- **License**: dual **GPLv3** / Steinberg VST 3 Licensing Agreement; used
  under the GPLv3 option (AGPLv3-compatible).
- VST® is a registered trademark of Steinberg Media Technologies GmbH.

## Planned dependencies (later phases)

| Dependency | Phase | License | Notes |
|---|---|---|---|
| libopenmpt (pinned 0.8.7) | 5 — module import | BSD-3-Clause | official release packages only, not the repo |
| LAME | 5 — MP3 export | LGPL (patents expired) | demo exports only |
| Orbitron font | 6 — RetroVault theme | SIL OFL 1.1 | embeddable, verified |
