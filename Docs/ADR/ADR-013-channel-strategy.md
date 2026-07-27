# ADR-013: Mono/stereo/multichannel strategy
Context: MVP ships mono/stereo; future LCR/5.1/7.1 must not require rework.
Decision: All modules process N-channel `Context` (channel count fixed at prepare); stereo-specific behavior (crosstalk, width, link) guards on channel count; bus layout accepts mono->mono, mono->stereo, stereo->stereo in MVP. No hard-coded channel-2 assumptions outside guarded blocks.
Alternatives: stereo-only hard-coding (cheaper now, rework later — rejected).
Consequences: Surround = adding layouts + policies, not restructuring.
Validation: Unit/LayoutTest instantiates mono and stereo, asserts correctness.
Reversal cost: N/A (this is the hedge).
