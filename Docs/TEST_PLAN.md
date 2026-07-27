# Test Plan — VintageFilmAudio

Framework: in-repo lightweight harness (`Tests/Harness/VfaTest.h`) + CTest.
No GoogleTest: zero prior workspace convention existed, the harness is ~150
lines, dependency-free, and prints TAP-style output (ADR-012 discussion).
All DSP tests run headless against `vfa_dsp` directly; plugin-level tests run
against the `AudioProcessor` in a host-less JUCE environment.

Suites (CTest labels):

## Unit
- ParameterManifestTest: parses Docs/PARAMETER_MANIFEST.md, asserts APVTS
  layout matches IDs/ranges/defaults/choices exactly.
- MacroTest: macro modulation math — bounds, no base rewrite, determinism.
- EraProfileTest: every Era×Medium×Condition combination yields in-range
  parameter values; era switch is undoable state write.
- GainStagingTest: −18 dBFS calibration; trims; mix law.

## DSP
- DeliveryCurveTest: impulse → FFT magnitude at target frequencies for all 8
  curve modes × {44.1, 48, 96} kHz; asserts DSP_SPEC §2.1 tolerances; writes
  `TestOutput/delivery_<curve>_<sr>.csv`; group delay above 100 Hz < 1 ms;
  neutral ±0.25 dB; stability (impulse decay); latency reported == actual.
- OpticalDistinctnessTest: THD vs level at 200 Hz and 8 kHz; asserts optical
  HF THD ≫ LF THD at equal drive (ratio ≥ 4×) while hard-clip reference shows
  ratio ≈ 1; image-spread: HF energy of shaped noise **decreases** with level
  (negative correlation) vs clipper (non-negative); VA vs VD harmonic parity
  (VD 2nd-dominant, VA 3rd-dominant); DC removal < −60 dBFS.
- MediumTest: bandwidth −3 dB points per medium within ±1/3 octave of
  DSP_SPEC §3; SNR ordering; crosstalk level; aliasing: 15 kHz tone at 48 kHz
  through Standard-quality nonlinearity → alias products < −40 dBFS.
- WowFlutterTest: 1 kHz sine → instantaneous-frequency analysis: depth 0.5 ⇒
  0.1 % ±40 % speed dev; spectrum has energy both <10 Hz (wow) and 8–40 Hz
  (flutter); delay excursion bounded ≤ hard bound; same seed ⇒ identical
  output (bit-exact); different seed ⇒ different; linked ⇒ L==R modulation;
  no sample-to-sample delay jump > bound (no discontinuity/zipper).
- GenerationTest: HF (−6 dB point) monotonically decreases and noise floor
  monotonically increases over gens 0..8; optimized vs ExactReferenceChain
  magnitude error ≤ 1.5 dB in band; fractional gens continuous (no jump >
  0.5 dB per 0.1 gen); deterministic with seed.
- NoiseTest: RMS at param=0.5 within ±3 dB of spec; hum FFT peak at 50/60 Hz
  exactly; harmonic series present; crackle event density scales with param
  (counted via threshold detector); seeded determinism bit-exact; correlation:
  nsWidth 0 ⇒ interchannel correlation >0.99, 1 ⇒ <0.3 (hiss); silence
  behavior per spec; 60 s render: no repetition (autocorrelation of noise
  envelope beyond 1 s lag < 0.2).
- BroadcastTest: AGC gain range bounded ±12 dB; limiter ceiling respected;
  dialogue focus band boost measured; futz ≠ telephone: response at 150 Hz
  and 6 kHz within spec (not −40 dB as a 300–3.4k BP would give).

## State
- SaveLoadTest: randomize all params → save → load → bit-equal values.
- PresetTest: every factory preset loads, all values in range, recall exact.
- MigrationTest: v1 state with missing/unknown/out-of-range/corrupted
  XML → safe load, defaults, no crash, flag set.

## Realtime
- AllocationTest: global new/delete counter guard around processBlock —
  zero allocations after prepareToPlay across 2000 blocks with automation,
  preset changes queued from another thread, bypass toggles, quality switches.
- StressTest: NaN/Inf scan on output for: extreme params (all min, all max,
  random×200), zero-length blocks, block size changes 1..2048, SR changes,
  denormal-region input (1e-30), silence, full-scale square input.
- ClickTest: automation ramps of every float param and toggling of every bool/
  choice param during sine playback → max sample-to-sample derivative bounded
  (no click > threshold vs steady-state reference); bypass and mix ramps.
- QualitySwitchTest: Eco↔Standard↔High during playback → no click, no NaN.

## Performance
- PerfTest: per-module and full-chain μs/block at 44.1/48/96/192 kHz ×
  block {16,32,64,128,256,512,1024,2048}, stereo; reports avg + worst case,
  writes `TestOutput/perf.json`; asserts full chain Standard @48k/512 uses
  < 50 % of real-time budget on this container (guard against regression).

## Reference / subjective package
- RenderReferencePackage tool: renders the subjective-test corpus (generated
  synthetic dialogue-like, music-like, transient, tone, room-tone signals —
  no licensed audio) through: bypass, plugin presets, EQ-only approximation,
  generic-saturation approximation; level-matched, randomized labels manifest
  for ABX. Output: `TestOutput/SubjectivePackage/` + manifest.json.

## Host/format validation
- VST3: build artifact presence + `pluginval` (if obtainable in-container)
  strictness level 5; otherwise documented as environment-blocked.
- AU: ✖ blocked (no macOS). AAX: ✖ blocked (no SDK). Standalone: builds; GUI
  launch untestable headless — documented.

Every claim in TEST_RESULTS.md carries the exact command that produced it.
