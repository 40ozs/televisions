# ADR-016: Deterministic randomness
Context: Renders must be reproducible with a fixed seed across runs/platforms; streams must be independent per module/channel/purpose.
Decision: xoshiro256** PRNG + SplitMix64 stream derivation from hash(userSeed, moduleId, channel, purpose); seed parameter (0 = auto but render-stable, fixed at prepare); no std::random_device or rand() anywhere in DSP; float conversion via fixed 2^-53 mapping.
Alternatives: juce::Random (LCG quality, shared-state temptations); std::mt19937 (heavier state per stream).
Consequences: Bit-exact repeatability testable; platform-independent (integer math only).
Validation: determinism assertions in WowFlutter/Noise/Generation tests.
Reversal cost: Low.
