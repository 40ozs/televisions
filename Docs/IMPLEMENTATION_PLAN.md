# Phased Implementation Plan — VintageFilmAudio

Phases mirror the task brief §23; each phase ends with passing tests, a
commit, and an RTM/implementation-log update. MVP = phases 0–8.

| Phase | Scope | Exit criteria |
|---|---|---|
| 0 Discovery | inventory, environment, docs (this set) | all Phase-0 docs committed; empty-workspace + missing-research-doc deviations recorded |
| 1 Skeleton | CMake+JUCE 8.0.8, vfa_dsp lib, plugin targets (VST3/Standalone; AU/AAX gated), APVTS from manifest, state save/load, test harness + CTest wired | plugin builds; ParameterManifestTest + SaveLoadTest pass |
| 2 Delivery curves | min-phase FIR designer, 8 curve modes, response tests, CSV export | DeliveryCurveTest passes at 44.1/48/96 kHz within DSP_SPEC tolerances |
| 3 Medium models | optical (VA/VD, image spread), magnetic film/tape, kinescope, broadcast media configs, oversampled nonlinear stages | OpticalDistinctnessTest + MediumTest pass |
| 4 Transport + generations | wow/flutter engine, generation model + exact reference, determinism | WowFlutterTest + GenerationTest pass |
| 5 Noise & artifacts | full noise engine per DSP_SPEC §8 | NoiseTest passes |
| 6 Dynamics + reproduction | AGC, program limiter, dialogue focus, speaker model, mono/width, safety limiter, auto-gain | BroadcastTest + GainStagingTest pass |
| 7 Presets + UI | era profiles, macro system, 16 factory presets, editor (era/medium/condition + macros + advanced + response visualizer) | EraProfileTest, MacroTest, PresetTest pass; editor compiles; UI is functional on desktop hosts (visual check impossible headless — documented) |
| 8 Hardening | allocation/stress/click/quality tests, perf benchmarks, adversarial multi-agent code review, fixes, TEST_RESULTS + BENCHMARKS docs | Realtime+Performance suites pass; review findings resolved or logged |
| 9 (post-MVP) | physical tape, device color, convolution spaces, surround, deeper optical | out of MVP scope |

Commit policy: one commit per phase minimum, descriptive messages, push to
`claude/vintage-film-tv-audio-plugin-5kvsqw`.
