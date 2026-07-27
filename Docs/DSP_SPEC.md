# DSP Design Specification — VintageFilmAudio

Marker convention: **HIST-APPROX** flags values that are engineering
approximations of historical behavior rather than verified measurements
(see Docs/WORKSPACE_INVENTORY.md §3 — the primary research document was absent;
numeric targets quoted in the task brief are treated as authoritative).

Reference level: **−18 dBFS = 0 VU nominal** (ADR gain staging). All nonlinear
stages are calibrated so that −18 dBFS sine input at 1 kHz produces ≤0.5 dB
gain error and “nominal” distortion character; +8 dB over nominal reaches
heavy saturation.

## 1. Sample-rate policy

All curves/filters are designed at `prepare` time for the actual sample rate
(44.1–192 kHz). Targets are specified in absolute Hz; responses above Nyquist
are truncated gracefully. Tests run at 44.1/48/96 kHz minimum.

## 2. Delivery Curve module

**Implementation (ADR-004):** each curve is a tabulated magnitude spec
(log-frequency breakpoints, dB) → interpolated onto an FFT grid (4096 bins) →
**minimum-phase FIR** via real-cepstrum method → truncated to 257 taps
(Eco: 129, High: 513) with raised-cosine tail window → `juce::dsp::FIR` or
direct convolution per channel. Latency ≈ 0 reported (minimum phase; group
delay <1 ms above 100 Hz — measured and documented by the response test).
Design happens off the audio thread; coefficient swap is atomic + crossfade.

### 2.1 Curve target tables (dB re 1 kHz)

**Academy** (task-brief targets in bold; interpolation HIST-APPROX):
| Hz | 31.5 | **40** | 63 | **100** | 250 | 1k | **1.6k** | 2.5k | 4k | **5k** | 6.3k | **8k** | 10k+ |
|----|------|--------|----|---------|-----|----|-----------|------|----|--------|------|--------|------|
| dB | −9 | **−7** | −4 | **0** | 0 | 0 | **0** | −2.5 | −7 | **−10** | −13.5 | **−18** | −24 (floor −30) |

Tolerance at bold anchor points: **±1.5 dB**; elsewhere ±2.5 dB.

**X-Curve-inspired** (SMPTE-202-style *room calibration result* — this plugin
applies it as a transfer-response emulation of what a theatrical playback
chain does to the track; engineers did NOT print this EQ onto mixes. Stated in
UI tooltip, user manual, and here): flat to 2 kHz, −3 dB/oct above (anchor:
−3 dB @ 4 kHz, −6 @ 8 kHz, −9 @ 16 kHz), LF: −3 dB/oct below 50 Hz (HIST-APPROX).
Small-room variant: −1.5 dB/oct above 2 kHz. Tolerance ±1 dB of the slope line
2–16 kHz.

**Early television** (HIST-APPROX, 1950s TV set chain): −6 @ 80 Hz,
flat 150 Hz–4 kHz, −6 @ 6 kHz, −18 @ 10 kHz, presence bump +2 dB @ 2–3 kHz.

**Kinescope** (HIST-APPROX, film-off-monitor + optical print): flat 120 Hz–3.5 kHz,
−3 @ 100 Hz shoulder, −10 @ 6 kHz, −24 @ 9 kHz; +1.5 dB ripple 1–3 kHz.

**Broadcast mono TV** (HIST-APPROX, transmitter+receiver average): −3 @ 60 Hz,
flat 100 Hz–5 kHz, −6 @ 8 kHz, −20 @ 12 kHz.

**Late analog TV / MTS-era** (HIST-APPROX): −3 @ 50 Hz, flat to 10 kHz,
−9 @ 14 kHz, −30 @ 15.5 kHz (aural carrier limit ~15 kHz).

**Neutral:** 0 dB everywhere (still runs the FIR path so A/B is fair;
tolerance ±0.25 dB).

`Academy Amount` / `X-Curve Amount` params blend the designed curve magnitude
toward flat in dB domain before FIR design (recomputed off-thread, crossfaded).

## 3. Medium models

Common per-medium config: band limits, pre/de-emphasis, nonlinearity type +
drive, transient rounding (envelope-driven variable LPF), SNR target, width /
mono collapse, crosstalk (stereo media only).

| Medium | Bandwidth (HIST-APPROX) | Nonlinearity | SNR target | Format |
|---|---|---|---|---|
| Optical mono | 60 Hz–7.5 kHz | §4 optical | ~38 dB | mono collapse |
| Optical stereo (’77-era) | 50 Hz–10 kHz | §4 optical, milder | ~48 dB | stereo, crosstalk −35 dB |
| Magnetic film | 40 Hz–14 kHz | tanh-family + bias-loss, head bump +1.5 dB @ 60 Hz | ~55 dB | stereo |
| Field tape (portable ¼", 7.5 ips) | 50 Hz–12 kHz | tape sat, head bump +2 dB @ 55 Hz | ~52 dB | mono/stereo |
| Kinescope | per §2 curve | mild VD-optical | ~35 dB | mono |
| Broadcast mono | 100 Hz–5 kHz + §7 chain | transmitter soft clip | ~45 dB | mono |
| Broadcast stereo (MTS) | 50 Hz–14 kHz + §7 | mild | ~55 dB | stereo, L−R noise +6 dB |
| Consumer recording | 60 Hz–9 kHz | heavier tape sat | ~40 dB | stereo, azimuth HF loss |
| Clean | full | none | ∞ | as input |

Magnetic saturation MVP: third-order-dominant asymmetry-free
`x·(1+a|x|)⁻¹`-family shaper with level-dependent HF loss (drive-scaled
one-pole before shaper, inverse tilt after) at 2× oversampling (Standard).
Head bump: peaking filter whose frequency tracks tape speed
(30 ips→~40 Hz, 15→~55 Hz, 7.5→~70 Hz, 3.75→~90 Hz; HIST-APPROX).

## 4. Optical track model (ADR-005) — must be distinct from generic clipping

Chain (2× oversampled around nonlinearity, 4× in High):
1. **Band limit** per mode (slit-loss LPF: Gaussian-ish rolloff, not brickwall).
2. **HF-emphasis → nonlinearity → de-emphasis** (+10 dB shelf @ 4 kHz around
   the shaper): reproduces sibilance-selective intermod distortion of optical
   tracks (HF energy hits the transfer knee first) — measurably different
   from wideband clippers: THD@8 kHz ≫ THD@200 Hz at equal level.
3. **Transfer curve:** Variable-Area mode = symmetric printer-characteristic
   sigmoid with adjustable knee; Variable-Density mode = asymmetric gamma-style
   curve (2nd-harmonic-dominant). Asymmetry parameterized; DC servo after.
4. **Modulation-peak rounding:** fast (0.3 ms attack) feedback gain
   computer that compresses only the top 6 dB (“valve/ground-noise-reduction
   shutter” style; HIST-APPROX) — rounds peaks *before* the shaper.
5. **Image-spread approximation:** level-dependent HF loss — envelope-driven
   one-pole whose cutoff drops with signal level (up to −6 dB @ 8 kHz at full
   modulation) + tiny level-dependent all-pass smear. Distinctness test: HF
   loss must correlate with level (generic clippers *add* HF with level).
   Behind `IImageSpreadStage` for a future physical model.
6. Cell noise / dirt / crackle are produced by the Noise engine with
   optical-specific spectra & density scaling (§8).

## 5. Wow & flutter (ADR-006)

One bounded fractional delay line per channel (cubic Hermite, center 12 ms,
modulation depth hard-bounded ±10 ms), modulation signal =
`drift + wow + flutter + scrape`:
- Drift: integrated bounded random walk, ≤0.15 Hz (leaky integrator).
- Wow: two quasi-periodic components (0.5–4 Hz, e.g. rotational 0.6 Hz + 1.9×)
  with slow random AM/PM (±15 % rate jitter via filtered noise).
- Flutter: narrowband filtered noise 8–40 Hz + one motor component 24–30 Hz.
- Scrape: filtered noise 60–200 Hz at small depth (HIST-APPROX of
  scrape-flutter sidebands).
Depth calibration: `wow=0.5` ⇒ ±0.1 % speed deviation; `1.0` ⇒ ±0.4 %
(HIST-APPROX: consumer/worn territory). Stereo link = same generator; unlinked
= per-channel independent streams from split seeds. Deterministic via §9 PRNG.
Modulator updated at control rate (once per 32 samples) and linearly
interpolated per sample → no zipper; depth changes smoothed 50 ms.

## 6. Generation loss (ADR-007)

Optimized accumulated equivalent of N sequential transfers (N = 0–8 float):
- HF loss: one-pole per-gen cutoff `f_g` (medium-dependent, e.g. magnetic
  16 kHz, optical 9 kHz); accumulated magnitude = N-fold power of the
  prototype response, realized as 2 cascaded one-poles + tilt correction
  fitted at prepare time; fractional N by dB-domain interpolation.
- Noise: floor rises `+k·√N` in power per medium hiss spectrum (transfers sum
  incoherent noise) — fed into Noise engine.
- Saturation: drive into medium nonlinearity `+g·N` dB with soft cap.
- Wow/flutter: depth `×√N` accumulation into Transport.
- Transient softening: attack-envelope rounding time `+τ·N`.
- Variability: seeded per-generation ±tolerance offsets (0 = identical).
`ExactReferenceChain` (test-only, non-RT): literally runs the simplified
single-transfer stage N integer times; equivalence tests assert ≤1.5 dB
magnitude error 100 Hz–0.8·min(f_g·something, Nyquist) and matching monotonic trends.

## 7. Broadcast chain & period dynamics

- **AGC:** slow feedback gain rider (attack 300 ms, release 1.5–4 s
  program-dependent, ±12 dB range, gate below −45 dBFS to avoid noise pumping),
  `Pumping` param shortens release and deepens correction.
- **Program limiter:** feedback peak limiter, attack 1–5 ms,
  dual-time-constant release with program dependence (vari-mu-flavored
  soft knee at slow settings; FET-flavored hard knee fast) — selected by
  `Release Character`.
- **Dialogue focus:** presence tilt (+ up to 4 dB, 1.2–3 kHz raised-cosine
  band) + LF cut tracking amount; intelligibility emphasis, not telephone BP.
- **Transmitter/receiver (optional stage):** 75 µs-style pre-emphasis → soft
  clip → de-emphasis + receiver IF ripple ±1 dB (HIST-APPROX), producing
  level-dependent HF compression typical of over-deviation control.
- **Sync buzz:** 59.94 Hz (or 50 Hz PAL-style) buzz with strong odd harmonics,
  amplitude keyed to video-frame-rate AM when `Buzz` raised (§8).

## 8. Noise & artifact engine (ADR-008)

All sources deterministic from §9 PRNG streams; **no looped buffers** — all
noise is generated, all events are Poisson-scheduled with seeded draws.
Per-source: level, spectral shaper, stereo correlation (correlated for
media-borne noise like hum/buzz; decorrelated for hiss/cell at width>0).
- Hiss: white → −3 dB/oct tilt + medium band shape.
- Optical cell noise: blue-ish tilt + 96 Hz sprocket AM component (35 mm,
  4 perf × 24 fps; HIST-APPROX level).
- Broadcast noise: triangular-ish FM noise floor rising 6 dB/oct above 3 kHz,
  L−R stream +6 dB in MTS mode.
- Hum: 50/60 Hz fundamental + harmonics (rolloff −6 dB/harmonic, `Harmonics`
  param tilts); Buzz: odd-harmonic-rich 59.94/50 Hz with frame-rate AM.
- Crackle: Poisson impulses (density 0.1–80 /s) → 2nd-order bandpass ring;
  Dirt: sparser, bigger, asymmetric bipolar clicks, LP-shaped.
- Dropouts: Poisson events gating −3…−30 dB with 5–80 ms raised-cosine edges +
  simultaneous HF loss.
- Projector/gate texture: 24 Hz + 96 Hz AM-modulated filtered noise + low
  “mechanical” thump train at 24 Hz, level-independent (present in silence).
- Print-through: **post-echo** = delayed (≈ one wrap: 400 ms @ MVP fixed,
  HIST-APPROX), −32…−45 dB, LP @ 4 kHz copy. **Pre-echo requires lookahead
  latency and is a documented MVP limitation** (KNOWN_LIMITATIONS.md).
- Tube microphonic events: rare seeded resonant pings (800–1400 Hz, Q≈25,
  −40 dB) under Artifacts macro top range.
- Ducking: envelope follower on program → optional −0…−12 dB noise duck
  (models perceptual masking / NR expanders); artifacts (clicks) never duck.
- Silence behavior: media noise persists in silence (param `Noise in Silence`
  ON default, historical); tails: noise stops on host bypass, continues over
  internal-module bypass per module semantics; plugin delivers clean tail on
  transport stop within one block.

## 9. Deterministic randomness (ADR-016)

`vfa::Rng` = xoshiro256** with SplitMix64 seeding; every consumer gets a
stream = `hash(userSeed, moduleId, channel, purpose)`. `Seed` parameter:
0 = auto (seeded once at prepare from a counter — still stable within a
render), 1–99999 = fully deterministic. Offline render == replay identical.

## 10. Oversampling & quality (ADR-009/010)

Oversampling **only** around nonlinear stages (optical shaper, magnetic
saturator, transmitter clip, safety limiter final clip): Eco 1×, Standard 2×,
High 4× (`juce::dsp::Oversampling`, polyphase IIR, latency reported).
Both quality paths preallocated; atomic switch + 10 ms crossfade.

## 11. Output protection

Safety limiter: 0.1 ms lookahead-free feedback limiter + arctan ceiling at
−0.3 dBFS, defeatable. DC blocker (5 Hz HP) always on. Auto-gain: optional,
matches long-term LUFS-ish RMS (3 s window) of wet vs dry within ±6 dB
correction bound, 500 ms smoothing, disabled during offline render start for
repeatability (state pinned at prepare).
