# Parameter Manifest — VintageFilmAudio (state version 1)

Authoritative parameter contract. `Source/Parameters/ParameterIDs.h` and the
APVTS layout MUST match this table 1:1; the unit test
`Tests/State/ParameterManifestTest` parses this file and fails on divergence.

Conventions:
- IDs are stable forever; renames happen via display name only. Removal or
  semantic change requires a state-version bump + entry in StateMigration.
- All float params are automatable and smoothed by their DSP owner as listed.
  Choice/bool params switch click-free (crossfade or smoothed gain) inside DSP.
- `Auto=n` params are host-visible but flagged non-automatable.
- Serialization: all params serialize in APVTS state; preset files store all.
- Macros modulate effective values (base + modulation architecture, ADR-003);
  they never rewrite other parameters. Era/Medium/Condition selection writes
  profile defaults into base params as an explicit, undoable user gesture.
- UI owner: Main page unless listed in an Advanced section.

| ID | Display name | Type | Range / Choices | Default | Skew | Unit | Smooth ms | Auto | DSP owner |
|---|---|---|---|---|---|---|---|---|---|
| era | Era | choice | Early 1950s\|Late 1950s\|Early 1960s\|Late 1960s\|Early 1970s\|Late 1970s\|Early 1980s\|Late 1980s | Early 1960s | - | - | 0 | y | EraProfiles |
| medium | Medium | choice | Clean\|Optical Mono\|Optical Stereo\|Magnetic Film\|Field Tape\|Kinescope\|Broadcast Mono\|Broadcast Stereo\|Consumer | Magnetic Film | - | - | 0 | y | MediumModel |
| condition | Condition | choice | Master\|Release Print\|Broadcast\|Off-Air\|Workprint\|Worn Archive\|Multi-Generation\|Damaged | Release Print | - | - | 0 | y | EraProfiles |
| character | Character | float | 0..1 | 0.5 | 1 | - | 50 | y | MacroSystem |
| fidelity | Fidelity | float | 0..1 | 0.7 | 1 | - | 50 | y | MacroSystem |
| generation | Generation | float | 0..8 | 1 | 1 | gens | 100 | y | GenerationModel |
| genInteger | Exact Generations | bool | off\|on | off | - | - | 0 | y | GenerationModel |
| genVariability | Gen Variability | float | 0..1 | 0.3 | 1 | - | 50 | y | GenerationModel |
| artifacts | Artifacts | float | 0..1 | 0.3 | 1 | - | 50 | y | MacroSystem |
| noiseMacro | Noise | float | 0..1 | 0.3 | 1 | - | 50 | y | MacroSystem |
| deliveryCurve | Delivery Curve | choice | Neutral\|Academy\|X-Curve\|X-Curve Small Room\|Early TV\|Kinescope\|Broadcast Mono\|Late TV | Academy | - | - | 0 | y | DeliveryCurve |
| mix | Mix | float | 0..100 | 100 | 1 | % | 20 | y | Output |
| inTrim | Input Trim | float | -24..24 | 0 | 1 | dB | 20 | y | Input |
| outTrim | Output Trim | float | -24..24 | 0 | 1 | dB | 20 | y | Output |
| autoGain | Auto Gain | bool | off\|on | off | - | - | 0 | y | Output |
| safetyLimiter | Safety Limiter | bool | off\|on | on | - | - | 0 | y | Output |
| audition | Audition | choice | Normal\|Artifacts Only\|Noise Muted | Normal | - | - | 0 | y | Output |
| quality | Quality | choice | Eco\|Standard\|High | Standard | - | - | 0 | n | Engine |
| seed | Random Seed | int | 0..99999 | 0 | - | - | 0 | n | Rng |
| deliveryOn | Delivery Enable | bool | off\|on | on | - | - | 0 | y | DeliveryCurve |
| mediumOn | Medium Enable | bool | off\|on | on | - | - | 0 | y | MediumModel |
| transportOn | Transport Enable | bool | off\|on | on | - | - | 0 | y | WowFlutter |
| genOn | Generation Enable | bool | off\|on | on | - | - | 0 | y | GenerationModel |
| noiseOn | Noise Enable | bool | off\|on | on | - | - | 0 | y | NoiseEngine |
| dynOn | Dynamics Enable | bool | off\|on | on | - | - | 0 | y | Dynamics |
| reproOn | Reproduction Enable | bool | off\|on | on | - | - | 0 | y | Reproduction |
| delLfRoll | LF Roll-Off | float | 20..300 | 50 | 0.4 | Hz | 50 | y | DeliveryCurve |
| delHfRoll | HF Roll-Off | float | 2000..20000 | 12000 | 0.4 | Hz | 50 | y | DeliveryCurve |
| delMidShape | Midrange Shape | float | -6..6 | 0 | 1 | dB | 50 | y | DeliveryCurve |
| delAcademyAmt | Academy Amount | float | 0..1 | 1 | 1 | - | 50 | y | DeliveryCurve |
| delXcurveAmt | X-Curve Amount | float | 0..1 | 1 | 1 | - | 50 | y | DeliveryCurve |
| delPlaybackSize | Playback Size | float | 0..1 | 0.5 | 1 | - | 50 | y | DeliveryCurve |
| medOpticalDist | Optical Distortion | float | 0..1 | 0.4 | 1 | - | 50 | y | OpticalModel |
| medOpticalMode | Optical Mode | choice | Variable Area\|Variable Density | Variable Area | - | - | 0 | y | OpticalModel |
| medImageSpread | Image Spread | float | 0..1 | 0.4 | 1 | - | 50 | y | OpticalModel |
| medMagSat | Magnetic Saturation | float | 0..1 | 0.4 | 1 | - | 50 | y | MagneticModel |
| medTapeSpeed | Tape Speed | choice | 3.75 ips\|7.5 ips\|15 ips\|30 ips | 15 ips | - | - | 0 | y | MagneticModel |
| medHeadBump | Head Bump | float | 0..1 | 0.5 | 1 | - | 50 | y | MagneticModel |
| medTransSoften | Transient Softening | float | 0..1 | 0.3 | 1 | - | 50 | y | MediumModel |
| medCrosstalk | Crosstalk | float | 0..1 | 0.3 | 1 | - | 50 | y | MediumModel |
| wow | Wow | float | 0..1 | 0.25 | 1 | - | 50 | y | WowFlutter |
| wowRate | Wow Rate | float | 0.2..4 | 0.65 | 0.5 | Hz | 50 | y | WowFlutter |
| flutter | Flutter | float | 0..1 | 0.25 | 1 | - | 50 | y | WowFlutter |
| flutterRate | Flutter Rate | float | 8..40 | 24 | 0.5 | Hz | 50 | y | WowFlutter |
| drift | Drift | float | 0..1 | 0.2 | 1 | - | 50 | y | WowFlutter |
| scrape | Scrape | float | 0..1 | 0.1 | 1 | - | 50 | y | WowFlutter |
| stereoLink | Transport Stereo Link | bool | off\|on | on | - | - | 0 | y | WowFlutter |
| transportQuality | Transport Quality | choice | Lab\|Studio\|Portable\|Worn\|Damaged | Studio | - | - | 0 | y | WowFlutter |
| nsHiss | Hiss | float | 0..1 | 0.3 | 1 | - | 50 | y | NoiseEngine |
| nsCell | Cell Noise | float | 0..1 | 0.2 | 1 | - | 50 | y | NoiseEngine |
| nsBroadcast | Broadcast Noise | float | 0..1 | 0 | 1 | - | 50 | y | NoiseEngine |
| nsHum | Hum | float | 0..1 | 0.15 | 1 | - | 50 | y | NoiseEngine |
| nsHumFreq | Hum Frequency | choice | 50 Hz\|60 Hz | 60 Hz | - | - | 0 | y | NoiseEngine |
| nsHumHarm | Hum Harmonics | float | 0..1 | 0.3 | 1 | - | 50 | y | NoiseEngine |
| nsBuzz | Sync Buzz | float | 0..1 | 0 | 1 | - | 50 | y | NoiseEngine |
| nsCrackle | Crackle | float | 0..1 | 0.2 | 1 | - | 50 | y | NoiseEngine |
| nsDirt | Dirt Clicks | float | 0..1 | 0.15 | 1 | - | 50 | y | NoiseEngine |
| nsDropout | Dropouts | float | 0..1 | 0 | 1 | - | 50 | y | NoiseEngine |
| nsProjector | Projector | float | 0..1 | 0.1 | 1 | - | 50 | y | NoiseEngine |
| nsPrintThrough | Print-Through | float | 0..1 | 0.1 | 1 | - | 50 | y | NoiseEngine |
| nsDuck | Noise Ducking | float | 0..1 | 0.3 | 1 | - | 50 | y | NoiseEngine |
| nsSilence | Noise In Silence | bool | off\|on | on | - | - | 0 | y | NoiseEngine |
| nsWidth | Noise Width | float | 0..1 | 0.5 | 1 | - | 50 | y | NoiseEngine |
| dynAgc | AGC | float | 0..1 | 0.2 | 1 | - | 50 | y | Dynamics |
| dynComp | Compression | float | 0..1 | 0.3 | 1 | - | 50 | y | Dynamics |
| dynLimit | Limiting | float | 0..1 | 0.3 | 1 | - | 50 | y | Dynamics |
| dynRelChar | Release Character | float | 0..1 | 0.5 | 1 | - | 50 | y | Dynamics |
| dynDialog | Dialogue Focus | float | 0..1 | 0.2 | 1 | - | 50 | y | Dynamics |
| dynPump | Pumping | float | 0..1 | 0.1 | 1 | - | 50 | y | Dynamics |
| reproMono | Mono Mode | choice | Stereo\|Narrow\|Mono | Stereo | - | - | 0 | y | Reproduction |
| monoLaw | Mono Sum Law | choice | -3 dB\|-4.5 dB\|-6 dB | -3 dB | - | - | 0 | y | Reproduction |
| reproWidth | Width | float | 0..1 | 1 | 1 | - | 50 | y | Reproduction |
| reproSpeaker | Small Speaker | float | 0..1 | 0 | 1 | - | 50 | y | Reproduction |

Non-parameter state (ValueTree properties, serialized, not host-visible):
`stateVersion` (int, =1), `uiScale`, `advancedOpen`, `lastPresetName`.

Migration policy: state v1 is the baseline. Future versions must add entries
to `Source/Parameters/StateMigration.h` (old-ID → transform → new-ID) and to
the table in Docs/STATE_MIGRATION.md. Loading newer-versioned state: apply
known params, ignore unknown, never fail. Corrupted state: fall back to
default program and report via non-blocking flag readable by the editor.
