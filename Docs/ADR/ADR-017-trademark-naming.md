# ADR-017: Trademark-safe public naming
Context: Historical hardware/film references are trademark-encumbered.
Decision: Public UI/preset text uses descriptive category names only; NAMING_REVIEW.md is a release gate; internal docs may cite history. Flagged terms (Academy, X-Curve, Kinescope) carry documented fallbacks.
Alternatives: evocative branded-adjacent names (marketing value < legal risk — rejected).
Consequences: Slightly drier names; zero clearance blockers for MVP.
Validation: PresetTest cross-checks preset names against NAMING_REVIEW cleared list.
Reversal cost: Low (rename only).
