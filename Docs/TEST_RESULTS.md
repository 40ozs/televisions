# Test Results — VintageFilmAudio MVP

Date: 2026-07-27. Environment: Ubuntu 24.04 container, GCC 13.3, Release
build, JUCE 8.0.8. Every result below was actually executed; commands given.

## Suite results

| Suite | Command | Result |
|---|---|---|
| DSP suite (62 tests, 310 423 checks) | `./build/VintageFilmAudio/VfaDspTests` | **PASS (0 failing)** |
| Plugin suite (39 tests, 49 028 checks) | `./build/VintageFilmAudio/VfaPluginTests` | **PASS (0 failing)** |
| CTest wrapper | `ctest --test-dir build --output-on-failure` | **2/2 PASS** |
| pluginval strictness 5 (VST3, incl. editor under Xvfb) | `xvfb-run -a pluginval --strictness-level 5 --validate "build/.../VST3/Vintage Film and TV.vst3"` | **PASS (exit 0, 19 test groups)** |
| ASAN spot-run (DSP suite) | `build-asan/VintageFilmAudio/VfaDspTests` | **clean** (run during Phase 2; one test-side overrun found and fixed then) |
| Subjective package render | `./build/VintageFilmAudio/VfaRenderPackage` | **generated** (35 stimuli + manifest, `TestOutput/SubjectivePackage/`) |

Per-area coverage highlights (all inside the two suites):
- Delivery curves: all 8 modes × 44.1/48/96 kHz vs DSP_SPEC §2.1 tolerances;
  CSV exports in `TestOutput/delivery_*.csv`; stability, group delay < 1 ms,
  crossfaded redesign smoothness.
- Optical distinctness: HF-selective THD ratio 4.1× (hard-clip control ≈ 1);
  image-spread negative level correlation 3.2 dB; VA 3rd-dominant vs VD
  2nd-dominant; DC < −60 dBFS; standard-quality alias < −93 dB.
- Media: bandwidth corners within ±1/3 oct of spec; head bump tracks tape
  speed; monotonic THD vs saturation; broadcast HF over-deviation
  compression; MTS side HF loss; aliasing < −40 dB Standard.
- Wow/flutter: 0.1 % calibration at 0.5; wow/flutter spectral split; bounded
  excursion; bit-exact seed determinism; stereo link; zero-depth = exact
  576-sample delay.
- Generation loss: monotonic HF/noise accumulation; fractional continuity
  (< 0.8 dB per 0.1 gen); equivalence vs ExactReferenceChain within
  documented bounds; integer-snap mode.
- Noise: per-source RMS calibration ±3 dB of SNR targets; hum at exactly
  50/60 Hz + harmonics; buzz odd harmonics at 59.94 Hz; density scaling;
  bit-exact seeds; width correlation; ducking/silence behavior;
  30 s no-repetition (chunk cross-correlation < 0.2).
- Dynamics/reproduction: AGC bounded ±12 dB + gate; ratio and program-
  dependent release morph; limiter ceiling; dialogue focus ≠ telephone
  (150 Hz ≥ −8 dB, 6 kHz ≥ −6 dB at speaker 0.5); mono fold laws; width.
- State: bit-exact round-trip; 16 presets recall-accurate, names cleared,
  full decade/media coverage; corrupted/unknown/out-of-range/future-version
  handling; NaN sanitization.
- Realtime: **0 allocations** across 2000 processBlocks under automation
  storm + preset changes (global new/delete audit); NaN/Inf-free at all-min/
  all-max/200 random states × 5 signal types incl. denormals; block sizes
  0–2048; SR 44.1–192 k; state reload during playback; every float param
  ramped and every toggle/choice switched click-free (max step < 0.08 on a
  −18 dBFS sine).
- Performance: see Docs/BENCHMARKS.md + TestOutput/perf.json (Standard
  48 kHz/512 ≈ 6.8 % of budget; guard threshold 50 %).

## Defects found and fixed during validation

1. pluginval segfault: message-thread redesign timer could run before
   `prepareToPlay` allocated FIR buffers → heap smash. Fixed with an
   `enginePrepared` gate + module-level buffer guard. Re-validated.
2. `reproSpeaker` automation click (max step 0.10): coefficients tracked the
   raw parameter and the stage gate jumped between all-passed and raw dry
   paths. Fixed (smoothed-amount coefficient tracking + continuous stage
   blend). Re-validated.
3. Test-side buffer overrun in the delivery crossfade test (found by ASAN).

## Format matrix (honest)

| Format | Status | Evidence |
|---|---|---|
| VST3 (Linux) | **BUILT + VALIDATED** | artifact + pluginval log |
| Standalone (Linux) | **BUILT** (GUI launch untestable headless) | artifact present |
| AU | **BLOCKED** — no macOS toolchain in environment | CMake configure message |
| AAX | **BLOCKED** — no AAX SDK on machine | CMake configure message |

## MVP acceptance criteria (task brief §24)

| Criterion | Status |
|---|---|
| Builds in all locally available formats | ✅ VST3 + Standalone |
| VST3 loads in a host/validator | ✅ pluginval strictness 5 |
| AU validation on macOS | ⛔ blocked (no macOS) — honest gate |
| AAX verified or explicitly blocked | ✅ explicitly blocked (no SDK) |
| Academy response within tolerance | ✅ ±1.5 dB anchors, 3 SRs |
| X-Curve response within tolerance | ✅ ±1 dB slope line |
| Optical distinct from generic clipping | ✅ measurably (ratio + spread tests) |
| Generation loss controlled/monotonic | ✅ |
| Wow/flutter stochastic, bounded, discontinuity-free | ✅ |
| Noise deterministic with fixed seed | ✅ bit-exact |
| Major controls automate without clicks | ✅ ClickTest all params |
| Bypass click-free | ✅ (module enables + mix ramp; host bypass is host-side) |
| Preset recall accurate | ✅ |
| State migration implemented | ✅ v1 + migration table + tests |
| No audio-thread allocation | ✅ test-enforced |
| No NaN/Inf in stress tests | ✅ |
| Performance measured & documented | ✅ BENCHMARKS.md |
| Factory presets cover decades & media | ✅ test-enforced coverage |
| Preset names trademark-safe | ✅ test-enforced against cleared list |
| Artifact paths documented | ✅ BUILD.md |
| All tests/known failures reported honestly | ✅ this document |
