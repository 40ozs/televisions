# ADR-002: Shared DSP library boundaries
Context: Reusable DSP should live in a shared library, but creating a workspace-level shared repo for a single consumer adds speculative structure.
Decision: Keep all framework-independent DSP in the `vfa_dsp` static library (depends only on juce_core/juce_dsp/juce_audio_basics), with plugin-client code strictly outside it. Promote `vfa_dsp` to a workspace-shared library when a second plugin exists.
Alternatives: separate top-level `SharedDSP/` repo now — rejected (YAGNI, no second consumer); header-only — rejected (build times, ODR risk).
Consequences: Clean extraction path; tests link `vfa_dsp` without plugin code.
Validation: test targets link only `vfa_dsp` + harness; include graph checked in review phase.
Reversal cost: Low — target rename + path moves.
