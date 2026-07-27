# ADR-004: Delivery-curve implementation
Context: Seven-plus historical response curves with numeric tolerances, sample-rate independence, low latency, and testability. Hand-tuned biquad chains are fragile across sample rates.
Decision: Tabulated magnitude targets (DSP_SPEC §2.1) → log-domain interpolation onto FFT grid → minimum-phase FIR via real cepstrum → windowed truncation (129/257/513 taps by quality) designed off-thread at prepare/param-change, applied per channel with atomic coefficient swap + crossfade.
Alternatives: biquad cascade fit (fragile, per-SR retuning — kept as documented fallback); linear-phase FIR (latency — rejected for MVP); analog-matched IIR (accuracy work larger than benefit).
Consequences: Near-zero latency (min-phase), measured group delay documented; approximation error test-enforced; curve edits are data edits.
Validation: DSP/DeliveryCurveTest tolerances + CSV artifacts at 3 sample rates.
Reversal cost: Low — module interface hides the design method.
