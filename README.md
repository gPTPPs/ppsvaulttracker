# PPsVaultTracker

**A modern VSTi tracker — by The Unborn / [RetroVault](https://retrovault.be)**

A pattern-based tracker in the FastTracker 2 / ProTracker lineage, hosting
**VST3 instruments and effects**, designed to compose full songs whose
deliverables (**MIDI + WAV stems**) import cleanly into Ableton Live for final
arrangement and mastering.

Windows + Linux (Ubuntu 24.04 reference). Built with [JUCE 8](https://juce.com)
(fetched at configure time, pinned). License: **AGPLv3**.

> 🚧 **Status: phase 1 — skeleton.** Audio device + VST3 hosting by explicit
> file path + on-screen/hardware MIDI keyboard. Proof that sound comes out.
> The pattern editor, sequencer, .ubt format, imports/exports and the
> synthwave UI arrive in later phases (see [CLAUDE.md](CLAUDE.md) for the
> full roadmap and architecture).

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
