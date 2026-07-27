# ADR-014: AAX build strategy and dependency pinning
Context: AAX SDK is not present in this environment; JUCE acquisition method must be deterministic.
Decision: CMake option VFA_ENABLE_AAX (default OFF) + AAX_SDK_PATH; configure emits an explicit "AAX BLOCKED: SDK not found" status instead of silently skipping. AU auto-enabled only on APPLE. JUCE pinned at tag 8.0.8 in external/JUCE (git-ignored, re-fetch documented in BUILD.md) rather than FetchContent, so configure works offline once fetched and the tree is inspectable.
Alternatives: FetchContent (network at configure time, cache opacity); vendoring JUCE into the repo (bloat, license-notice burden — avoided).
Consequences: Honest format reporting; reproducible builds documented by tag hash in BUILD.md.
Validation: configure logs; format matrix in TEST_RESULTS.md.
Reversal cost: Low.
