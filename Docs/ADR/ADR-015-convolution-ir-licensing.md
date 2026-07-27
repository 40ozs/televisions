# ADR-015: Convolution and IR licensing
Context: Reverb/room/speaker IRs are post-MVP; shipping unlicensed IRs is prohibited.
Decision: MVP ships zero third-party audio data; all noise/artifacts synthesized. Post-MVP convolution slot reserved in Reproduction; any future IRs require documented license/provenance in THIRD_PARTY.md before inclusion.
Alternatives: none acceptable.
Consequences: No licensing exposure in MVP; speaker model is filter-based (documented as approximation).
Validation: repo audit (no audio binaries); THIRD_PARTY.md inventory.
Reversal cost: N/A.
