# ADR-001: Reuse strategy across existing VST projects
Context: Task requires reusing the strongest existing workspace conventions. Recursive inspection (WORKSPACE_INVENTORY.md) found an empty repository — no plugins, libraries, or conventions exist.
Decision: Establish new conventions with this project as the workspace reference implementation; document them in SAS.md so later plugins inherit them.
Alternatives: (a) import an external open-source plugin as a convention donor — rejected: license/IP review burden, no requirement; (b) invent nothing and use raw JUCE examples — rejected: insufficient for testability requirements.
Consequences: No compatibility constraints; this project defines parameter, preset, testing, and CMake norms.
Validation: WORKSPACE_INVENTORY inspection log; conventions exercised by the test suite.
Reversal cost: Low — nothing else depends on these conventions yet.
