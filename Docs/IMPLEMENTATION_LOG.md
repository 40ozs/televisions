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
