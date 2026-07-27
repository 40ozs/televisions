# ADR-010: Quality-mode architecture
Context: Eco/Standard/High must switch without clicks or audio-thread graph rebuilds.
Decision: All quality-dependent resources (FIR lengths, oversamplers) preallocated for every mode at prepare; mode is an atomic index; audible switch via 10 ms equal-power crossfade between precomputed paths where output differs discontinuously; otherwise coefficient swap.
Alternatives: rebuild graph on change (allocation on audio thread or async complexity — rejected); mute-switch (audible gap — rejected).
Consequences: Slightly higher memory; deterministic switching.
Validation: Realtime/QualitySwitchTest.
Reversal cost: Low.
