# Research Document Reconciliation — VintageFilmAudio

Date: 2026-07-27. The designated primary research document —
*“Vintage Film & TV Audio (1950s–1980s): Historical Sonic Characteristics and
a Plugin Design for Era-Accurate Emulation (Revised & Fact-Checked Edition)”*
— was supplied after the MVP was implemented (it was absent from the
workspace during Phase 0; recorded then as DEV-001). This document records
the clause-by-clause reconciliation. DEV-001 is now **resolved**.

## 1. Already-correct implementation (verified against the document)

| Document claim | Implementation status |
|---|---|
| Academy Curve (1938): flat 100 Hz–1.6 kHz, −7 @ 40 Hz, −10 @ 5 kHz, −18 @ 8 kHz | ✅ exact targets in CurveTables.h, test-enforced ±1.5 dB at 3 sample rates |
| X-Curve: flat to 2 kHz then −3 dB/oct (≈ −7 dB @ 10 kHz); small-room −1.5 dB/oct; a room *calibration* target, not a mix EQ | ✅ curve matches (measured −6.97 dB @ 10 kHz); calibration-vs-EQ caveat documented in code, DSP_SPEC, USER_MANUAL, KNOWN_LIMITATIONS |
| X-Curve transition mid-to-late 1970s as the central era switch | ✅ era profiles switch Academy→X-Curve at Early 1970s |
| Optical image-spread: light diffusion rounding modulation peaks; odd-harmonic + cross-mod distortion appearing first as HF sibilance | ✅ modeled (peak rounding stage, HF-emphasis intermod, level-dependent HF loss); distinctness-vs-clipper test-enforced |
| Generation loss as a core modelable era artifact (each transfer adds noise, HF loss, wow/flutter) | ✅ accumulated model + exact-reference validation |
| Head bump, print-through, azimuth loss, hum + sync buzz, wow <10 Hz / flutter higher, projector/gate noise, tube microphonics | ✅ all modeled (azimuth as consumer-variant HF shelf; scrape flutter approximated) |
| MTS/BTSC adopted March 1984; mono-compatible L+R with L−R subcarrier | ✅ Broadcast Stereo medium + “1984 Television Stereo” preset; L−R noise/HF behavior modeled |
| Wow & flutter figures (Nagra 0.05 % @ 15 ips territory) | ✅ depth calibration 0.5→0.1 %, 1.0→0.4 %; field-tape profile lands in the documented range |
| MUSHRA/ABX subjective validation; response/THD/W&F objective matching | ✅ ABX package tool + objective suites (MUSHRA panel itself is a human step, Docs/RELEASE_CHECKLIST) |
| Trademark cautions (Dolby litigious; descriptive preset names; counsel review) | ✅ NAMING_REVIEW policy matches; brands only in internal docs |
| JUCE / VST3 / AU / AAX; AAX essential for the Pro Tools post market | ✅ JUCE 8; AAX target structured and gated on SDK (absent in this environment — honestly reported) |
| Eco mode / oversampling only on nonlinear stages / accurate latency reporting | ✅ ADR-009/010, latency reported |
| Era-accurate market cases (Mank spectrum-matching etc.) | ✅ no code impact; cited as validation rationale |

## 2. Corrections applied after reading the document

1. **Optical medium bandwidth** (doc §1.2: tracks carried ~12.5 kHz, ~13 kHz
   with NR; the darkness came from the Academy *playback* curve):
   slit-loss corners raised — opticalMono 7.5 k → **12.5 kHz**, opticalStereo
   10 k → **13 kHz** (kinescope stays 8 kHz). The audible mono-optical result
   is still Academy-dominated; with Neutral delivery selected the medium now
   correctly extends.
2. **Medium SNR targets** (doc §1.4/1.5): opticalMono 38 → **42 dB** (doc:
   40–50), magneticFilm 55 → **62 dB** (doc: studio tape 60–75, 35 mm mag
   “> optical”), fieldTape 52 → **68 dB** (Nagra IV-S 68 dB NAB @ 7.5 ips —
   the field-tape profile’s default speed). Cell-noise floor moved to
   −60 dBFS accordingly. Rationale: clean-machine noise is low; audible era
   noise correctly comes from the generation-loss lift and condition states.
3. **Broadcast mono transmission bandwidth** (doc §1.3: aural carrier
   ~10–15 kHz; the tinny sound is the receiver): medium band limit raised
   from ~5 kHz to ~**10 kHz**; the small-set character remains modeled in the
   Reproduction stage (speaker model) and the Broadcast Mono delivery curve.
4. **“1938 Optical Mono” preset added** (now 17 factory presets) — the
   document’s Stage-1 MUSHRA benchmark preset (its name is also in the
   brief’s example list). Narrowed top end, denser optical nonlinearity,
   raised cell noise; name added to the cleared list.
5. Tests updated to the corrected targets (NoiseTest hiss −80 dBFS for
   magnetic film; MediumTest mono-broadcast corner at 10 kHz, 5 kHz in-band).

## 3. Document content with no code impact (noted for docs/marketing only)

- Hardware/mic date corrections (LA-2A 1965–69, RCA BA-6A a limiter, Neve
  8028 1970s, ribbon-not-shotgun boom history, STC 4035/4038 correction):
  the plugin ships non-branded device families only, so no public-facing
  content referenced these; useful for future device-color module design.
- CinemaScope 4-track (1953) / Todd-AO 6-track (1955) / Perspecta (1954):
  multichannel formats are post-MVP (ADR-013 reserves the architecture).
- Dolby A/SR compander specifics: relevant to a future NR-era device module;
  the current Late-Analog behavior is response/SNR-level (HIST-APPROX).
- Competitive landscape & market gap analysis: consistent with PRD §1
  positioning (delivery-chain emulation, not device futz).
- Kill/pivot thresholds and Stage gates: adopted into RELEASE_CHECKLIST
  intent; MUSHRA panel and Pro Tools/AAX validation remain human/hardware
  steps outside this environment.

## 4. Remaining deltas (deliberate, documented)

- The document’s Stage-2 items (Jiles-Atherton tape, WDF device color,
  convolution chambers, neural capture) are post-MVP by design (task brief
  §23 Phase 9); interfaces are reserved.
- Laugh track / multi-camera sitcom workflow color (doc §2) is not modeled;
  candidate future module.
- Perspecta-style steering, discrete mag multichannel: blocked on
  multichannel phase.
