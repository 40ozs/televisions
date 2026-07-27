# Third-Party Dependency & License Inventory — VintageFilmAudio

| Dependency | Version | License | Use | Shipped in binary |
|---|---|---|---|---|
| JUCE | 8.0.8 (pinned tag, external/JUCE, not vendored) | Dual: AGPLv3 / JUCE commercial license | framework, DSP primitives (FFT, oversampling), plugin wrappers | yes (statically) |
| VST3 interfaces | bundled inside JUCE | Proprietary Steinberg VST3 license / GPLv3 dual | VST3 wrapper | yes |
| AAX SDK | **not present** | Avid license (registration required) | AAX target (gated OFF) | no |

Notes:
- **Commercial release requires either a JUCE commercial license or AGPLv3
  compliance**, and acceptance of the Steinberg VST3 licensing terms. Recorded
  as a release-gate item in Docs/RELEASE_CHECKLIST.md.
- No GoogleTest (in-repo harness, ADR-012), no sampled audio, no impulse
  responses, no fonts, no artwork assets — zero additional licensing surface
  (ADR-015).
- System libraries (ALSA, X11, FreeType, fontconfig, libcurl) are dynamically
  linked platform libraries on Linux.
