# Heard it on TV — historical delivery-chain audio plugin by Oddity Audio

A JUCE/C++20 audio effect (VST3 / Standalone; AU on macOS; AAX-ready) that
transforms clean audio into historically plausible film and television sound
of ~1950–1989 by modeling the complete delivery chain: period dynamics,
storage medium (optical / magnetic film / tape / broadcast / kinescope),
transfer-generation loss, theatrical & broadcast playback curves,
reproduction systems, and era noise/mechanical artifacts.

- Build: see `Docs/BUILD.md` (CMake + pinned JUCE 8.0.8)
- Documentation index: `Docs/` (PRD, SAS, DSP spec, parameter manifest, RTM,
  test plan/results, benchmarks, ADRs, known limitations)
- Plugin source: `VintageFilmAudio/`
- Tests: `ctest --test-dir build` (TAP-style suites + pluginval validation)

Historical-accuracy notes and every documented approximation live in
`Docs/DSP_SPEC.md` (marker: `HIST-APPROX`) and `Docs/KNOWN_LIMITATIONS.md` —
including the X-Curve caveat: historically a room-calibration target, applied
here as a playback-chain response emulation.
