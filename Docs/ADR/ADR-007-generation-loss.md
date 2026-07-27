# ADR-007: Generation-loss optimization
Context: 0–8 transfer generations; running the full chain N times is wasteful and non-continuous.
Decision: Closed-form accumulated model: N-fold prototype HF-loss response fitted by cascaded one-poles + tilt at prepare; noise power +k*sqrt(N); drive +g*N dB (soft-capped); wow/flutter depth *sqrt(N); transient softening +tau*N; seeded per-generation variability. Test-only `ExactReferenceChain` runs the literal repeated stage for equivalence bounds.
Alternatives: literal repeated processing (CPU, no fractional N — kept as reference); simple LPF+noise (fails historical-behavior requirement).
Consequences: Continuous macro + exact-integer mode both cheap; error bound explicit (≤1.5 dB in band).
Validation: DSP/GenerationTest monotonicity + equivalence.
Reversal cost: Medium — model constants embedded in medium configs.
