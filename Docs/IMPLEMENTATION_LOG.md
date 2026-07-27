# Implementation Log — VintageFilmAudio

Running log of work, deviations, and approximations. Newest last.

## 2026-07-26 — Phase 0
- Inspected workspace: **empty** (readme.md only). No prior VST projects,
  shared libraries, or conventions. Recorded in WORKSPACE_INVENTORY.md.
- **DEV-001:** primary research document absent from workspace; numeric
  targets sourced from the task brief + established engineering history;
  all approximations flagged `HIST-APPROX` in DSP_SPEC.md and source.
- Environment: Ubuntu 24.04 headless, GCC 13.3, CMake 3.28, Ninja; installed
  ALSA/X11 dev packages; fetched JUCE 8.0.8 to `external/JUCE` (git-ignored).
- **BLOCKED formats:** AU (no macOS toolchain), AAX (no SDK on machine).
  VST3 + Standalone are the buildable targets here.
- Authored full Phase-0 documentation set (PRD, SAS, DSP_SPEC,
  PARAMETER_MANIFEST, RTM, TEST_PLAN, RISK_REGISTER, IMPLEMENTATION_PLAN,
  NAMING_REVIEW, PRESET_GUIDE, ADR-001..017).
- **DEV-002:** print-through pre-echo requires lookahead latency; MVP ships
  post-echo only (KNOWN_LIMITATIONS).
- **DEV-003:** noise injected post-delivery-curve with per-source spectral
  pre-shaping instead of physically interleaved insertion points (SAS §2) —
  optimization with documented per-source shaping.

## 2026-07-26 — Phase 1 (skeleton) + Phase 2 (delivery curves)
- CMake + JUCE 8.0.8 build green on Linux: VST3 + Standalone artifacts under
  `build/VintageFilmAudio/VintageFilmAudio_artefacts/Release/`. AU/AAX gated
  and reported BLOCKED at configure time (no macOS toolchain / no AAX SDK).
- APVTS layout generated from the in-code manifest table; ParameterManifestTest
  parses Docs/PARAMETER_MANIFEST.md and asserts 1:1 parity — passing.
- State save/load round-trip bit-exact (discrete params quantize as designed).
- **DEV-004 (amends ADR-010):** quality/medium switches use a 5 ms wet-path
  fade-down -> switch+reset -> fade-up instead of dual-path crossfade;
  bounded, click-free, far cheaper. Dry path unaffected.
- **DEV-005 (amends ADR-004):** delivery curves are hybrid: auto-fitted
  IIR low-shelf pair below 250 Hz + minimum-phase FIR above (short FIRs
  cannot realize LF shelves), plus analytic runtime high-pass filters for
  LF Roll-Off and small Playback Size (size 0.5 = neutral centre).
  DeliveryCurveTest passes at 44.1/48/96 kHz within DSP_SPEC tolerances,
  incl. Academy anchors (−7 @40 Hz, −10 @5 kHz, −18 @8 kHz ±1.5 dB); CSV
  response exports in TestOutput/. ASAN run clean (a test-side buffer
  overrun was found by ASAN and fixed; module code was not at fault).

## 2026-07-27 — Phases 3–8 (full chain, presets/UI, validation)
- Modules implemented in parallel (optical, magnetic/broadcast, transport/
  generation, noise, dynamics/reproduction) against frozen headers; each
  self-verified; integrated with a clean full rebuild. All 62 DSP + 39
  plugin tests pass; pluginval strictness 5 passes (VST3, editor incl.).
- **DEV-006:** safety limiter moved to the true final output stage (after
  mix/output trim) so it cannot be overshot by trims — deviates from the
  brief's conceptual order; corner-case test enforces the ceiling.
- **DEV-007 (bug found by pluginval, fixed):** the message-thread redesign
  timer could fire before prepareToPlay allocated delivery FIR buffers →
  heap corruption. Fixed with an enginePrepared gate + module buffer guard.
  This same mechanism explained an earlier transient test corruption that
  had been mis-attributed solely to mid-build header edits.
- **DEV-008 (fixed):** small-speaker morph clicked under coarse automation
  (raw-parameter coefficient jumps + binary stage gate against a phase-
  shifted dry leg). Now tracks the smoothed amount and blends the stage in
  continuously over the first 2 % of the range.
- Subjective ABX package renderer added (synthetic license-free corpus;
  bypass / 4 presets / EQ-only / generic-sat strawmen; level-matched,
  seeded, randomized blind labels + manifest).
- Benchmarks recorded (Docs/BENCHMARKS.md): Standard 48 kHz/512 ≈ 6.8 % of
  real-time budget on the 4-core reference container.
- NOTE: git push to origin currently returns 403 through the environment's
  git proxy (fetch works); commits are local until push access recovers.
