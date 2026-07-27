# ADR-009: Oversampling boundaries
Context: Aliasing from nonlinear stages vs CPU budget.
Decision: Oversample only nonlinear stages (optical shaper, magnetic saturator, transmitter clip, final safety clip) with juce::dsp::Oversampling: Eco 1x, Standard 2x, High 4x. Never oversample the full chain.
Alternatives: global 8x (CPU explosion, latency — rejected); ADAA waveshaping (candidate future refinement, noted).
Consequences: Alias products bounded (test-enforced < -40 dBFS Standard); latency small and reported.
Validation: MediumTest aliasing assertions; PerfTest cost per quality.
Reversal cost: Low — wrapper `OversampledStage` isolates policy.
