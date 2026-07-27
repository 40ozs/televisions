# Build Guide — VintageFilmAudio

## Prerequisites (Linux, as used in the reference environment)

- CMake ≥ 3.22, Ninja, GCC ≥ 13 (or Clang ≥ 16), C++20
- JUCE Linux dependencies:
  `libasound2-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev
  libxinerama-dev libfreetype6-dev libfontconfig1-dev libcurl4-openssl-dev`
- JUCE 8.0.8 (pinned, ADR-014), fetched to `external/JUCE`:
  ```
  git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git external/JUCE
  ```

macOS: Xcode toolchain — AU enables automatically. Windows: MSVC 2022 —
VST3/Standalone (untested in this environment; report honestly per TEST_RESULTS).

## Configure & build

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Configure prints the honest format matrix, e.g. on Linux:
```
VFA: AU BLOCKED — requires a macOS toolchain; ...
VFA: AAX BLOCKED — SDK not present on this machine. ...
```

### AAX (when an SDK is available — see Docs/AAX_SETUP.md)
```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DVFA_ENABLE_AAX=ON -DAAX_SDK_PATH=/path/to/AAX_SDK
```

## Artifacts (Release, Linux)

- VST3: `build/VintageFilmAudio/VintageFilmAudio_artefacts/Release/VST3/Vintage Film and TV.vst3`
- Standalone: `build/VintageFilmAudio/VintageFilmAudio_artefacts/Release/Standalone/Vintage Film and TV`
- Shared-code static lib: `.../Release/libVintage Film and TV_SharedCode.a`
- Test executables: `build/VintageFilmAudio/VfaDspTests`, `build/VintageFilmAudio/VfaPluginTests`

## Tests

```
ctest --test-dir build --output-on-failure        # both suites
./build/VintageFilmAudio/VfaDspTests              # DSP suite (TAP output)
./build/VintageFilmAudio/VfaDspTests Optical      # substring filter
./build/VintageFilmAudio/VfaPluginTests           # plugin/state/RT suite
```

Response CSVs and `perf.json` land in `TestOutput/` at the repo root.

## Determinism notes

- No `__DATE__`/`__TIME__`; JUCE pinned by tag; all DSP randomness seeded
  (ADR-016). Rebuilding the same tree yields functionally identical binaries.
- **Do not edit DSP headers while a build is running** — mixed-generation
  objects produce undefined behavior (observed during development; a full
  `cmake --build` after any header change is mandatory).
