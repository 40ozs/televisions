# ADR-003: Parameter, state, and macro architecture
Context: Macros (Character/Fidelity/Generation/Artifacts/Noise) and Era/Medium/Condition must produce coherent multi-parameter changes without fighting user-set advanced parameters.
Decision: JUCE APVTS holds base values (stable IDs per PARAMETER_MANIFEST.md). Macros are modulation inputs combined with base values inside DSP-side `ParamSnapshot` computation (base-value + modulation architecture). Era/Medium/Condition selection performs an explicit, undoable write of profile defaults into base params (a deliberate user gesture), after which everything remains editable.
Alternatives: macros write parameters (feedback loops, host-automation fights — rejected); preset interpolation layer (heavier, obscures automation semantics — rejected); normalized mod matrix (overkill for fixed macro set — rejected).
Consequences: Host automation of base params and macros composes predictably; effective values are not host-visible (documented; visualizer shows effective state).
Validation: Unit/MacroTest asserts no parameter writes from macro paths and bounded effective values.
Reversal cost: Medium — DSP snapshot layer localized in one file.
