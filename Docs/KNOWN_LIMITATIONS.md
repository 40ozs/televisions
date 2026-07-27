# Known Limitations — VintageFilmAudio MVP

Historical-accuracy simplifications (all flagged `HIST-APPROX` in source):

1. **X-Curve applied as a transfer response** (DSP_SPEC §2): historically a
   room-calibration target (SMPTE-202-style), not a mastering EQ. The plugin
   emulates the audible *result* of theatrical playback. Stated in UI docs,
   manual, and ADR-004.
2. **Optical image spread** is a level-dependent HF-loss approximation behind
   `IImageSpreadStage`, not a physical exposure/slit model (ADR-005).
3. **Print-through pre-echo is not implemented** (DEV-002): true pre-echo
   needs lookahead latency; MVP ships post-echo only (~400 ms, LP @ 4 kHz).
4. **Projector/gate noise is synthesized**, not measured (24 Hz + 96 Hz
   modulation model).
5. **Broadcast RF chain is approximate**: shelving pre/de-emphasis standing in
   for true 75 µs networks, soft-clip deviation control, ±1 dB IF ripple.
6. **Generation loss is an accumulated-equivalent model** (ADR-007) validated
   against a literal repeated-stage reference within documented bounds
   (≤3 dB at 8 kHz, ≤1.5 dB at 1/4 kHz).
7. **Crosstalk is gain bleed** (no interchannel delay/filter).
8. **Delivery curves below 250 Hz** use a fitted two-shelf IIR (DEV-005);
   fit error is test-bounded by the curve-table tolerances.
9. **Dropouts fire in 1–3 dip clusters** per event (documented in the noise
   engine) so audible dropouts survive RMS-level statistics; single-dip
   behavior is available by lowering the amount.

Technical limitations:

10. **Fixed 12 ms transport latency** (wow/flutter centre delay) always
    reported to the host; constant per session, medium/quality add a few
    samples of oversampling latency.
11. **Quality/medium switches** ramp the wet path through 5 ms of silence
    (DEV-004) rather than dual-path crossfade — click-free but momentarily
    dips the wet signal.
12. **Era/Medium/Condition profile application** happens on UI selection;
    host *automation* of these selectors changes context only (no parameter
    rewrite) by design (ADR-003).
13. **AU untested** (no macOS toolchain in the build environment); **AAX
    blocked** (no SDK). Both gated honestly in CMake.
14. **Editor rendered headlessly untested**: the container has no display;
    the editor compiles and uses standard JUCE components, but interactive
    verification requires a desktop host.
15. **Delivery-curve redesign is throttled** by staging (one redesign in
    flight at a time, 50 ms poll): fast automation of fidelity/character or
    delivery controls yields stepped-but-crossfaded curve updates.
16. **Auto-gain is RMS-based** (±6 dB bound): it will not conceal major
    dynamic-range differences (by design, PRD PR-14).
17. Surround layouts, physical tape hysteresis, WDF device color, and
    convolution spaces are post-MVP (interfaces reserved, SAS §7).
