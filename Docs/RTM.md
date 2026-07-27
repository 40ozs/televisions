# Requirements Traceability Matrix — VintageFilmAudio

Status legend: ☐ planned · ◐ in progress · ☑ implemented+tested · ✖ blocked
(with reason). Updated at the end of every phase. "Verified by" names an
executed test or produced artifact — nothing is marked ☑ without one.

| Req | Requirement (PRD) | Design ref | Implementation | Verified by | Status |
|---|---|---|---|---|---|
| PR-01 | Era/Medium/Condition workflow | SAS §4, ADR-003 | Parameters/EraProfiles | State/EraProfileTest | ☐ |
| PR-02 | Preset coverage 1950s–1980s, safe names | PRESET_GUIDE, NAMING_REVIEW | Presets/FactoryPresets | State/PresetTest + name review doc | ☐ |
| PR-03 | Delivery curves meet numeric targets | DSP_SPEC §2, ADR-004 | DSP/DeliveryCurves | DSP/DeliveryCurveTest (CSV) | ☐ |
| PR-04 | Medium models (6) | DSP_SPEC §3 | DSP/{Optical,Magnetic,Broadcast} | DSP/MediumTest | ☐ |
| PR-05 | Optical model distinct from clipping | DSP_SPEC §4, ADR-005 | DSP/Optical | DSP/OpticalDistinctnessTest | ☐ |
| PR-06 | Wow/flutter spec | DSP_SPEC §5, ADR-006 | DSP/Transport | DSP/WowFlutterTest | ☐ |
| PR-07 | Generation loss 0–8, monotonic, ref-validated | DSP_SPEC §6, ADR-007 | DSP/GenerationLoss | DSP/GenerationTest | ☐ |
| PR-08 | Noise/artifact engine | DSP_SPEC §8, ADR-008/016 | DSP/Noise | DSP/NoiseTest | ☐ |
| PR-09 | Broadcast futz path | DSP_SPEC §7 | DSP/Broadcast, Reproduction | DSP/BroadcastTest | ☐ |
| PR-10 | Preset system + migration | SAS §4, ADR-011 | Presets/, Parameters/StateMigration | State/StateTests | ☐ |
| PR-11 | Macro system, no fighting | ADR-003 | Parameters/MacroSystem | Unit/MacroTest | ☐ |
| PR-12 | RT safety | TEST_PLAN §RT | all DSP | Realtime/AllocationTest, StressTest | ☐ |
| PR-13 | Quality modes click-safe | ADR-010 | DSP/Core/Engine | Realtime/QualitySwitchTest | ☐ |
| PR-14 | Gain staging, audition modes | DSP_SPEC §11 | Output stage | Unit/GainStagingTest | ☐ |
| PR-15 | Mono+stereo, multichannel-ready | SAS §7 | Engine layout handling | Unit/LayoutTest | ☐ |
| PR-16 | Formats & honest gating | SAS §6 | CMakeLists | build logs / configure output | ☐ |

Acceptance criteria (task brief §24) map onto PR rows above; the per-criterion
checklist with evidence lives in Docs/TEST_RESULTS.md after Phase 8.
