# Risk Register — VintageFilmAudio

| ID | Risk | Likelihood | Impact | Mitigation | Status |
|---|---|---|---|---|---|
| R-01 | Primary research document absent; historical targets partially reconstructed | occurred | high | DEV-001: targets taken from task brief + established engineering history; all approximations flagged HIST-APPROX; single-source target tables so later correction is mechanical | open, mitigated |
| R-02 | No macOS/AAX toolchain → AU/AAX unverifiable here | occurred | med | CMake gates + honest reporting; AU auto-enables on macOS; AAX behind VFA_ENABLE_AAX+SDK path | accepted |
| R-03 | Headless container → no GUI/host smoke test | occurred | med | pluginval attempted; editor logic unit-tested where possible; documented | accepted |
| R-04 | Minimum-phase FIR curve fit misses tolerance at low sample rates | low | med | automated tolerance tests at 3 SRs; tap count escalation path; fallback biquad cascade documented | open |
| R-05 | Generation-loss optimized model diverges from exact reference | med | med | equivalence test with explicit error bound; quality mode can trade accuracy | open |
| R-06 | Performance budget blown by FIR + oversampled nonlinearities | med | med | Eco mode; perf test with regression threshold; oversampling only around nonlinear stages | open |
| R-07 | Macro/era-profile interaction confuses state (parameter fighting) | med | high | ADR-003 base+modulation; macros never write params; unit-tested | open |
| R-08 | Trademark exposure in preset/mode names | low | high | NAMING_REVIEW.md checklist; descriptive names only | mitigated |
| R-09 | Pre-echo print-through impossible without latency | occurred | low | MVP ships post-echo only; documented in KNOWN_LIMITATIONS; HQ lookahead variant planned | accepted |
| R-10 | Denormals/NaN in feedback stages (AGC, limiter, one-poles) | med | high | ScopedNoDenormals, flush-to-zero adds, stress tests | open |
| R-11 | Single-session build: JUCE tag availability offline later | low | low | pinned tag documented; external/ re-fetchable; no JUCE patches | accepted |
| R-12 | Auto-gain affecting render repeatability | low | med | gain state pinned at prepare; bounded correction; off by default | open |
| R-13 | Fresh conventions may not match future workspace plugins | low | low | conventions documented in SAS; vfa_dsp isolated for extraction | accepted |
