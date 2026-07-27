# ADR-006: Wow/flutter modulation architecture
Context: Requires stochastic, multi-component, bounded, deterministic, zipper-free speed modulation; not a single-sine chorus.
Decision: One bounded fractional-delay line per channel (cubic Hermite, ±10 ms hard bound around 12 ms center); modulator = leaky-integrated random walk (drift) + two jittered quasi-periodic wow components + narrowband-noise flutter + motor tone + HF scrape noise; control-rate (32-sample) generation with per-sample linear interpolation; seeded streams (ADR-016); stereo link = shared generator.
Alternatives: resampling-based varispeed (costlier, latency variable — rejected for MVP); single LFO (fails spec).
Consequences: Depth calibrated in % speed deviation; bounded by construction.
Validation: DSP/WowFlutterTest (depth, spectrum split, bound, determinism, no discontinuity).
Reversal cost: Low.
