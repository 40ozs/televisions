# Product Requirements Document — Vintage Film & TV Audio Emulator

Product codename: **VintageFilmAudio** (display name: “Vintage Film & TV”).
Version: 0.1.0 (MVP). Status: authoritative for the MVP scope.

## 1. Product statement

A real-time audio effect plugin (VST3 / AU / AAX-ready / Standalone) that
transforms clean contemporary audio into historically plausible film and
television sound of ~1950–1989 by emulating the **complete historical delivery
chain** — capture, period dynamics, storage medium, repeated transfer
generations, delivery/playback curve, reproduction system, and
noise/mechanical artifacts — **not** a generic lo-fi/tape/telephone effect.

## 2. Users and material

Re-recording mixers, post-production sound editors, dialogue editors, music
producers, and archival/pastiche content creators. Source material: dialogue,
VO, Foley, SFX, music stems, full mixes, PA/archival inserts, dubbing effects.

## 3. Core requirements (traced in Docs/RTM.md)

- **PR-01** Era/Medium/Condition three-layer workflow; era selection
  establishes a coherent, still-editable parameter state.
- **PR-02** Preset-first workflow spanning early-1950s optical mono through
  late-1980s MTS stereo TV; trademark-safe names only (Docs/NAMING_REVIEW.md).
- **PR-03** Delivery Curve module with Academy, X-Curve-inspired, early TV,
  kinescope, broadcast mono, late analog TV, and neutral responses meeting the
  numeric targets in Docs/DSP_SPEC.md §3 within stated tolerance; the X-Curve
  mode is documented everywhere as a *playback-chain result emulation*, not a
  claim of historical mastering EQ.
- **PR-04** Medium module: optical mono, magnetic film, tape, TV broadcast,
  kinescope, clean bypass — each with response, nonlinearity, noise spectrum,
  dynamic range, transient rounding, format/width/crosstalk control.
- **PR-05** Dedicated optical model (band limit, asymmetric transfer,
  modulation-peak rounding, HF-sibilance intermodulation, cell noise,
  dirt/crackle/scratches, VA/VD-inspired modes, image-spread approximation)
  measurably distinct from generic clipping/tape saturation.
- **PR-06** Wow/flutter: wow <10 Hz, flutter above, independent depth/rate,
  stochastic drift, deterministic seeds, stereo link options, bounded
  modulation, no zippering/discontinuities; multi-component (not one sine).
- **PR-07** Generation loss: 0–8 generations, continuous macro + exact-integer
  mode, optimized accumulated model, monotonic degradation, deterministic,
  validated against an exact repeated-stage reference implementation.
- **PR-08** Noise/artifact generator: hiss, cell noise, broadcast noise,
  50/60 Hz hum + harmonics, sync buzz, projector/gate texture, crackle, dirt,
  dropouts, print-through (post-echo in MVP; pre-echo is a documented
  limitation), tube-microphonic events; loop-free, seedable, correlation
  control, ducking option, correct silence/tail/bypass behavior.
- **PR-09** Mono/broadcast futz path: fold-down laws, narrow stereo,
  small-speaker bandwidth + resonance model, broadcast AGC, peak limiting,
  dialogue-forward shaping — explicitly not a telephone band-pass.
- **PR-10** Preset system: factory + user presets, versioned portable
  serialization, migration, range validation, corruption-safe loading.
- **PR-11** Macros (Character, Fidelity, Generation, Artifacts, Noise) using a
  documented base-value + modulation architecture; no parameter fighting.
- **PR-12** Real-time safety per Docs/TEST_PLAN.md §RT (no allocation, locks,
  file I/O, unbounded work on the audio thread; denormal-safe; NaN/Inf-free).
- **PR-13** Quality modes Eco/Standard/High with click-safe switching.
- **PR-14** Gain staging around −18 dBFS nominal; input/output trims; optional
  bounded auto-gain; true bypass; artifact-only audition; noise mute.
- **PR-15** Mono and stereo layouts in MVP; architecture must not preclude
  LCR/5.1/7.1 later.
- **PR-16** Formats: VST3 + Standalone build on this Linux container; AU
  enabled automatically on macOS toolchains; AAX gated behind `VFA_ENABLE_AAX`
  + SDK path and honestly reported as blocked when the SDK is absent.

## 4. Non-goals (MVP)

Physical Jiles-Atherton tape hysteresis, WDF device-color modules, convolution
rooms/IRs, surround processing, neural models, AAX DSP. Interfaces reserve
space for these (Docs/SAS.md §7).

## 5. Acceptance criteria

The MVP acceptance list of the task brief §24 is adopted verbatim and tracked
in Docs/RTM.md. No criterion may be marked met without a run test or produced
artifact; misses are reported in Docs/TEST_RESULTS.md and KNOWN_LIMITATIONS.md.
