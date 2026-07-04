# PPsVaultTracker

**A modern VSTi tracker — by The Unborn / [RetroVault](https://retrovault.be)**

A pattern-based tracker in the FastTracker 2 / ProTracker lineage, hosting
**VST3 instruments and effects**, designed to compose full songs whose
deliverables (**MIDI + WAV stems**) import cleanly into Ableton Live for final
arrangement and mastering.

Windows + Linux (Ubuntu 24.04 reference). Built with [JUCE 8](https://juce.com)
(fetched at configure time, pinned). License: **AGPLv3**.

![PPsVaultTracker](docs/screenshot.png)

> 🚧 **Status: phase 6 (polish) in progress.** Working today: FT2-style
> pattern editor (AZERTY/QWERTY), sample-accurate sequencer, up to 16 tracks
> with one VST3 instrument each, mixer (faders/mute/solo/VU/insert FX +
> master bus), song mode with order list, .ubt projects with autosave,
> MIDI/stems WAV/MP3/tracklist export, level-1 module import (MOD/XM/S3M/IT)
> and the RetroVault synthwave theme. See [CLAUDE.md](CLAUDE.md) for the
> full roadmap and architecture.

## Building

Requirements: CMake 3.22+, a C++20 toolchain (MSVC 2022 / GCC 13+), and on
Linux the usual JUCE dev packages (see `.github/workflows/ci.yml`).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release
```

JUCE 8.0.14 is downloaded automatically by CMake FetchContent on first
configure — nothing to install manually.

⚠️ Phase-1 note: plugins load **in-process** with no sandbox yet (the
out-of-process scanner comes in a later phase). A misbehaving plugin can take
the app down — only load plugins you trust.

## License

**GNU AGPLv3** — see [LICENSE](LICENSE) and
[THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
VST® is a registered trademark of Steinberg Media Technologies GmbH.
