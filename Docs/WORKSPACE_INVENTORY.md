# Workspace Inventory, Reuse Matrix, and Gap Analysis

Date: 2026-07-26
Author: Claude (autonomous implementation session)
Status: Phase 0 deliverable — completed before any implementation code was written.

## 1. Workspace inventory (verified by recursive inspection)

The repository `40ozs/televisions` was inspected recursively at session start
(`git ls-tree -r origin/main`, full filesystem walk of `/home/user/televisions`,
plus a system-wide search for `*.jucer`, `JUCE`, `CMakeLists.txt`, `VST*`,
`Source/`, `DSP/`, `Tests/`, SDKs, and shared libraries).

**Findings — the workspace is empty:**

| Item searched for | Result |
|---|---|
| `CMakeLists.txt` | none |
| `.jucer` projects | none |
| JUCE checkout | none (fetched fresh this session, see below) |
| `Source/`, `DSP/`, `Plugin/`, `UI/`, `Tests/`, `Parameters/`, `Presets/`, `Docs/`, `ADR/`, `RTM/`, `SAS/` | none |
| Existing VST/AU/AAX/CLAP plugin projects | none |
| Shared DSP / utility / visualizer / metering libraries | none |
| Parameter manifests, state-migration systems, preset serialization | none |
| Oversampling wrappers, SIMD helpers, RT-allocation test harnesses | none |
| CI configuration, CMake presets | none |
| AAX SDK | **not present anywhere on the machine** |
| VST3 SDK | not separately present (JUCE bundles its own VST3 interface headers) |
| macOS/AU toolchain | **not available (Linux container)** |
| Research document (see §3) | **not present** |

The only pre-existing file is `readme.md` (1 byte, effectively empty).

## 2. Reuse matrix

Because no prior plugin projects, shared libraries, or conventions exist in this
workspace, the reuse matrix is empty. There is **nothing to reuse and nothing
that can be broken**. Consequences:

1. No existing conventions constrain naming, namespaces, CMake layout, JUCE
   version, or testing framework. This project **establishes** the workspace
   conventions; they are documented in `Docs/SAS.md` so future plugins can
   reuse them.
2. Rather than speculatively extracting a shared library for a single consumer,
   reusable DSP is kept in a clearly separated, dependency-free static library
   target (`vfa_dsp`) inside this project. It compiles without JUCE plugin
   client code and is the designated seed for a workspace-level shared library
   when a second plugin appears (see `Docs/ADR/ADR-001` and `ADR-002`).

| Candidate source | Reusable component | Decision |
|---|---|---|
| — (workspace empty) | — | Establish new conventions; isolate reusable DSP in `vfa_dsp` static library |

## 3. Research document status — IMPORTANT DEVIATION RECORD

The task designates
*“Vintage Film & TV Audio (1950s–1980s): Historical Sonic Characteristics and a
Plugin Design for Era-Accurate Emulation”* as the primary design authority and
instructs it be located and read.

**The document does not exist in the repository, on the container filesystem,
or in any attached source.** This is recorded as deviation **DEV-001** (also in
`Docs/RISK_REGISTER.md` R-01 and the implementation log).

Mitigation used instead, in priority order:

1. **Numeric targets embedded in the task brief itself** (treated as excerpts of
   the research document): Academy response target points (flat 100 Hz–1.6 kHz,
   ≈ −7 dB @ 40 Hz, ≈ −10 dB @ 5 kHz, ≈ −18 dB @ 8 kHz); X-Curve ≈ flat to
   2 kHz then ≈ −3 dB/oct (−1.5 dB/oct small-room variant); wow < ~10 Hz,
   flutter above; 0–8 transfer generations; module list and signal order.
2. **Well-established, citable engineering history** consistent with those
   numbers: SMPTE 202 (X-Curve) as an *electro-acoustic room calibration
   target, not a mastering EQ*; the 1930s–40s Academy (“dialog norm”) playback
   characteristic; variable-area vs variable-density optical recording;
   35 mm magnetic film and multi-generation dubbing practice; kinescope
   (film recording off a video monitor) band limits; NTSC aural carrier
   deviation limits and 1984 BTSC/MTS stereo; 59.94 Hz-field sync buzz;
   50/60 Hz mains hum; sprocket/gate noise at 24 fps and 96 Hz
   (4 perforations × 24 fps) on 35 mm.
3. Every place where a value is an approximation rather than a verified
   measurement is flagged in `Docs/DSP_SPEC.md` and in source comments with the
   marker `HIST-APPROX`.

If the real research document is later supplied, `Docs/DSP_SPEC.md` §Targets is
the single place where numeric targets live; tests read the same tables, so
re-validation after correction is mechanical.

## 4. Build environment report

| Item | Value |
|---|---|
| OS | Ubuntu 24.04 (Linux container, headless) |
| Compilers | GCC 13.3 (primary), Clang 18.1.3 (available) |
| C++ standard | C++20 |
| CMake | 3.28.3, Ninja 1.11.1 |
| Cores / RAM | 4 cores / 15 GiB |
| JUCE | 8.0.8, fetched to `external/JUCE` (git-ignored; pinned tag documented in the build guide) |
| ALSA, X11, Xrandr, Xcursor, Xinerama, FreeType, libcurl | installed this session (required by JUCE on Linux) |
| AAX SDK | **absent → AAX target is BLOCKED, build gate provided** (`VFA_ENABLE_AAX` + `AAX_SDK_PATH`) |
| macOS toolchain | **absent → AU target is BLOCKED on this machine**, enabled automatically when configured on macOS |
| Buildable formats here | **VST3, Standalone (Linux)** + headless test executables |
| GUI display | none (headless); editor code compiles, host-window testing not possible in-container |

## 5. Gap analysis

| Needed capability | Exists in workspace? | Resolution |
|---|---|---|
| Plugin framework | No | JUCE 8.0.8 via pinned external checkout (ADR-014 documents why not FetchContent) |
| Parameter system | No | JUCE `AudioProcessorValueTreeState` + generated-from-manifest ID table (ADR-003) |
| Preset system | No | New: versioned ValueTree serialization + factory presets compiled in (ADR-011) |
| State migration | No | New: `stateVersion` property + migration table (ADR-011) |
| Delivery-curve filters | No | New: prepare-time minimum-phase FIR designer from tabulated targets (ADR-004) |
| Optical model | No | New (ADR-005) |
| Wow/flutter engine | No | New (ADR-006) |
| Generation-loss model | No | New accumulated-equivalent model + exact reference mode for tests (ADR-007) |
| Noise/artifact engine | No | New, deterministic-seed PRNG (ADR-008, ADR-016) |
| Oversampling wrapper | No | `juce::dsp::Oversampling` scoped to nonlinear stages only (ADR-009) |
| Test framework | No | Lightweight in-repo harness + CTest (ADR: see `Docs/ADR/ADR-012`); no GoogleTest dependency needed |
| Metering/visualizer transport | No | New bounded lock-free FIFO (ADR-012) |
| CI | No | Out of scope for this container; build/test commands documented for CI adoption |
| pluginval / host validation | Not installed | Attempted where network permits; results reported honestly in `Docs/TEST_RESULTS.md` |

## 6. Proposed folder structure

The structure from the task brief §21 is adopted essentially as written (it is
compatible with JUCE/CMake norms and nothing pre-existing conflicts). The plugin
lives at repository root under `VintageFilmAudio/`. See `Docs/SAS.md` §3.
