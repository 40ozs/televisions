# ADR-012: Visualization data transport (and test framework note)
Context: UI meters/spectra need audio-thread data without locks; workspace has no established test framework.
Decision: Bounded SPSC ring buffers (power-of-two, atomic indices) per feed (pre/post spectrum taps, gain-reduction, wow-history); audio thread pushes, 30 Hz UI timer drains, overflow drops oldest. Tests: in-repo ~150-line TAP-style harness + CTest labels (no GoogleTest — no prior convention, zero-dependency preferred; revisit if the workspace later standardizes).
Alternatives: juce::AbstractFifo (fine, but custom struct keeps vfa_dsp JUCE-GUI-free); GoogleTest (heavier, no existing convention to match).
Consequences: No audio-thread blocking; visualizers degrade gracefully under load.
Validation: Realtime/AllocationTest covers feed pushes; harness self-test.
Reversal cost: Low.
