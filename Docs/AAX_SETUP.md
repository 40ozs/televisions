# AAX Setup Instructions — VintageFilmAudio

Status: **BLOCKED in the reference environment** — the AAX SDK is not present
(verified by filesystem search; see WORKSPACE_INVENTORY.md §4) and is not
redistributable. The build is structured so AAX enables cleanly when the SDK
is available (ADR-014).

## Enabling AAX

1. Register with Avid and download the AAX SDK (2.4+ recommended for JUCE 8).
2. Configure with:
   ```
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
         -DVFA_ENABLE_AAX=ON -DAAX_SDK_PATH=/path/to/aax-sdk
   ```
   Configure fails loudly if `AAX_SDK_PATH` does not contain `Interfaces/`
   (no silent skip — honest reporting is a project rule).
3. Build; the artifact lands at
   `build/VintageFilmAudio/VintageFilmAudio_artefacts/Release/AAX/`.
4. Pro Tools requires PACE signing for release builds (developer account +
   wraptool); unsigned AAX loads only in Pro Tools Developer builds.
   Signing integration is a release-checklist item, not part of this repo.

AAX DSP (HDX) is explicitly out of scope for the MVP (task brief §5); no
design decisions were distorted to accommodate it.
