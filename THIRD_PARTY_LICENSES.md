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

### Orbitron font (embedded in `assets/`)

- **What**: display font for the RetroVault UI theme (The League of Moveable Type).
- **License**: **SIL Open Font License 1.1** — embedding and redistribution
  with software are explicitly permitted.
- **Source**: https://github.com/theleagueof/orbitron

### libopenmpt (optional, pinned 0.8.7 — NOT vendored)

- **What**: module file import (MOD/XM/S3M/IT), level 1 (notes only).
- **License**: BSD-3-Clause. Official release packages only.
- **How**: `-DPVT_LIBOPENMPT_DIR=<devel package>` (Windows) or
  `-DPVT_LIBOPENMPT_SYSTEM=ON` (Linux). Builds fine without it.
- **Source**: https://lib.openmpt.org/libopenmpt/

### LAME (external, never bundled)

- **What**: MP3 export runs a user-supplied `lame.exe` as an external process.
- **License**: LGPL; nothing is linked or redistributed by this project.
