# ADR-008: Noise generator design
Context: Many artifact classes, loop-free, deterministic, correlation-controllable, historically insertion-point-correct.
Decision: Single `NoiseArtifactEngine` with per-source generator objects (procedural synthesis only: filtered PRNG noise, Poisson-scheduled events, harmonic oscillators); per-source spectral shapers and correlation mixing; program-envelope ducking bus; artifact classes never loop (no wavetables/buffers).
Alternatives: sampled noise beds (licensing + looping risk — rejected, ADR-015); per-module scattered noise (duplication, no global ducking — rejected).
Consequences: All noise is seed-reproducible and CPU-predictable; spectra are approximations (HIST-APPROX) not measurements.
Validation: DSP/NoiseTest (levels, spectra, density, determinism, correlation, no-repetition).
Reversal cost: Low.
