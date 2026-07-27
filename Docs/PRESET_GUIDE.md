# Preset Design Guide & Factory Preset Manifest — VintageFilmAudio

Design rules:
1. A preset = full parameter state + name + category metadata; stored as
   versioned ValueTree (same schema as session state) compiled into the
   binary (factory) or saved as `.vfapreset` XML (user).
2. Every preset must satisfy: all values in manifest range (PresetTest),
   trademark-safe name (NAMING_REVIEW), and a written intent line here.
3. Coverage matrix requirement: every decade 1950s–1980s, every medium class
   (optical, magnetic, tape, kinescope, broadcast mono/stereo, consumer),
   fidelity extremes (master ↔ damaged), dialogue and full-mix intents.

## Factory presets (MVP, 17)

| # | Name | Era / Medium / Condition | Intent |
|---|---|---|---|
| 0 | 1938 Optical Mono | Early 1950s profile base / Optical Mono / Release Print | research-doc Stage-1 benchmark: pre-war Academy optical, narrowed top, dense cell noise |
| 1 | 1950s Theater Dialogue | Early 1950s / Optical Mono / Release Print | Academy curve, moderate optical distortion, studio dialogue |
| 2 | 1950s Magnetic Widescreen | Late 1950s / Magnetic Film / Master | wide-BW magnetic road-show feel, low artifacts |
| 3 | Early Television Kinescope | Early 1950s / Kinescope / Broadcast | kinescope curve, heavy band limit, hum + buzz |
| 4 | 1960s Studio Boom | Early 1960s / Magnetic Film / Master | studio dialogue, mild compression, clean magnetic |
| 5 | 1960s TV Sitcom Mono | Mid(Early) 1960s / Broadcast Mono / Broadcast | TV chain AGC, small-speaker leaning, 60 Hz hum |
| 6 | 1960s Dubbed Adventure | Late 1960s / Optical Mono / Multi-Generation | dubbing-chain generations, optical sibilance, dense mid |
| 7 | 1970s Location Recorder | Early 1970s / Field Tape / Master | portable ¼" character, mild wow, scene-tone hiss |
| 8 | 1970s Theatrical Optical | Early 1970s / Optical Mono / Release Print | Academy fading out era, moderate wear |
| 9 | 1977 Optical Stereo | Late 1970s / Optical Stereo / Release Print | early stereo optical, X-Curve, matrix-era width |
| 10 | 1980s Broadcast Mono | Early 1980s / Broadcast Mono / Broadcast | tighter processing, brighter, VCA-style AGC |
| 11 | 1984 Television Stereo | Late 1980s(Early 1980s) / Broadcast Stereo / Broadcast | MTS stereo, 15 kHz edge, L−R noise |
| 12 | 1980s Magnetic Mix | Early 1980s / Magnetic Film / Master | late mag film mix, low noise, X-Curve |
| 13 | Late Analog Cinema | Late 1980s / Optical Stereo / Release Print | cleanest theatrical optical, X-Curve small room |
| 14 | Multi-Generation Workprint | any / Magnetic Film / Workprint | 4+ generations, variability, softened transients |
| 15 | Worn Archive Print | any / Optical Mono / Worn Archive | heavy crackle/dirt, wow, HF loss |
| 16 | Off-Air Recording | Late 1970s / Consumer / Off-Air | consumer deck off TV: buzz, drift, dropouts |

User presets: saved under the platform user-data dir
(`juce::File::userApplicationDataDirectory / VintageFilmAudio/Presets`),
validated on load (range clamp, unknown-ignored, corruption → refuse with
message, never crash).
