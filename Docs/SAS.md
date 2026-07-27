# Software Architecture Specification — VintageFilmAudio

## 1. Overview

One JUCE 8.0.8 / C++20 / CMake plugin project with a framework-independent DSP
core. Two layers:

- **`vfa_dsp`** — static library, pure C++20 + juce_dsp/juce_core only, no
  plugin-client or GUI dependencies. All signal processing, parameter
  smoothing, deterministic PRNG, module graph. Unit-testable headlessly. This
  library is the seed of a future workspace-shared DSP library (ADR-001/002).
- **`VintageFilmAudio` plugin targets** — JUCE `AudioProcessor`/editor,
  APVTS parameter layer, preset system, visualizer transport, formats.

## 2. Processing graph

Fixed-order modular chain (task brief §6), each module implementing
`vfa::dsp::Module`:

```
InputTrim → ChannelPrep → SourceCapture → PeriodDynamics → Medium
 → GenerationLoss → Transport(Wow/Flutter) → DeliveryCurve → Reproduction
 → NoiseArtifacts(additive+ducking) → SafetyLimiter → Mix/Output
```

Rationale for deviations from the brief's conceptual order:
- Transport modulation is applied **after** generation loss because the
  accumulated-generation model already scales transport depth (ADR-007); a
  single bounded modulated delay line then realizes the sum. This is an
  optimization with documented equivalence testing, not a behavioral choice.
- Noise is injected late (post-delivery-curve) with per-source pre-filters so
  each noise class carries the spectrum it would have after its historical
  insertion point (e.g. optical cell noise is shaped by the delivery curve
  block internally). Documented per-noise in DSP_SPEC §8.

`Module` contract: `prepare(Spec)`, `reset()`, `process(Context&)`,
`setEnabled(bool)` (click-free via 10 ms crossfade), deterministic state,
no allocation/locks/IO in `process`. Every module owns its smoothers.

## 3. Source layout

```
VintageFilmAudio/
├── CMakeLists.txt
├── Source/
│   ├── Plugin/          PluginProcessor, PluginEditor
│   ├── Parameters/      ParameterIDs.h, ParameterLayout, EraProfiles, StateMigration
│   ├── DSP/             (vfa_dsp library)
│   │   ├── Core/        Module.h, ProcessSpec, SmoothedParam, Rng, DcBlocker, ...
│   │   ├── DeliveryCurves/  CurveTables.h, MinPhaseFirDesigner, DeliveryCurveModule
│   │   ├── Optical/     OpticalModel
│   │   ├── Magnetic/    MagneticModel (film + tape first pass)
│   │   ├── Broadcast/   BroadcastChain (AGC/limiter/futz), KinescopeModel
│   │   ├── Transport/   WowFlutterEngine
│   │   ├── GenerationLoss/  GenerationModel (+ExactReference for tests)
│   │   ├── Noise/       NoiseArtifactEngine (hiss/hum/crackle/dropout/...)
│   │   ├── Dynamics/    PeriodDynamics (vari-mu-style program compressor)
│   │   ├── Reproduction/ SpeakerModel, MonoFold, WidthControl, SafetyLimiter
│   │   └── Utilities/   OversampledStage, EnvelopeFollower, TiltFilter, ...
│   ├── Presets/         FactoryPresets.h/.cpp, PresetManager
│   └── UI/              Editor components, VisualizerFeed (lock-free FIFO)
├── Tests/               harness + Unit/DSP/State/Realtime/Performance suites
└── Docs/  (this documentation set lives at repo root /Docs)
```

## 4. Parameter and state architecture (ADR-003, ADR-011)

- Single `AudioProcessorValueTreeState`; IDs are stable strings in
  `ParameterIDs.h`, generated to match `Docs/PARAMETER_MANIFEST.md` 1:1 and
  checked by a unit test (manifest table parsed at test time).
- **Base + modulation:** host-visible advanced parameters hold *base* values.
  Macros (Character/Fidelity/Generation/Artifacts/Noise) and the
  Era/Medium/Condition profile modulate *effective* values inside the DSP
  update path only. Selecting Era/Medium/Condition additionally writes profile
  defaults into the base parameters (a user gesture, undoable, editable
  afterwards). Macros never write parameters → no fighting, no feedback loops.
- State = APVTS ValueTree + `stateVersion` int property + non-parameter node
  (seed, UI state). Migration table in `StateMigration.h`; unknown params
  ignored, missing params take defaults, out-of-range values clamped,
  malformed XML/binary → safe default state (never crash, never partial-apply).

## 5. Threading model

- Audio thread: `processBlock` only touches lock-free atomics snapshotted into
  a `ParamSnapshot` struct at block start; smoothing per-sample or per-16.
- Message thread: preset load, era-profile application, editor.
- Visualizer: bounded SPSC FIFOs (`VisualizerFeed`), audio thread pushes,
  UI timer (30 Hz) drains; overflow drops oldest, never blocks.
- Quality-mode change: both paths preallocated at `prepare`; switch is an
  atomic index + 10 ms crossfade (ADR-010). Graph is never rebuilt on the
  audio thread.

## 6. Formats & build

CMake options: `VFA_ENABLE_VST3` (default ON), `VFA_ENABLE_STANDALONE`
(default ON), `VFA_ENABLE_AU` (auto on APPLE), `VFA_ENABLE_AAX` (default OFF,
requires `AAX_SDK_PATH`, honest configure-time message when absent). C++20,
warnings-as-errors for project code, `-ffast-math` **not** used (denormals
handled via `ScopedNoDenormals` + flushing), deterministic builds (no
`__DATE__`), JUCE pinned at tag 8.0.8 in `external/JUCE` (ADR-014).

## 7. Extension points (post-MVP)

`MagneticModel` sits behind `IMediumNonlinearity` so a Jiles-Atherton stage can
replace the MVP saturator; `OpticalModel::ImageSpreadStage` is an interface for
a future physically informed model; `Reproduction` reserves a convolution slot;
channel handling is layout-agnostic (`process(Context&)` carries N channels)
so LCR/5.1 is additive work, not rework.
