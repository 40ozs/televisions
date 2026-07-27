# Release Checklist — VintageFilmAudio

Pre-release gates (all must be checked, none may be assumed):

## Legal / licensing
- [ ] JUCE commercial license purchased OR AGPLv3 release decision signed off
- [ ] Steinberg VST3 license terms accepted
- [ ] AAX: Avid developer agreement + PACE signing set up (Docs/AAX_SETUP.md)
- [ ] Naming review sign-off by counsel (Docs/NAMING_REVIEW.md flagged terms:
      "Academy", "X-Curve", "Kinescope" — fallback names documented)
- [ ] THIRD_PARTY.md re-audited against the shipping binary

## Engineering
- [ ] All test suites green on Linux, macOS, Windows release builds
- [ ] pluginval strictness 10 on all three platforms
- [ ] auval passes on macOS (AU)
- [ ] Manual host matrix: Pro Tools, Logic, Cubase/Nuendo, Reaper, Live —
      automation, preset recall, offline bounce, session reopen, latency comp
- [ ] Benchmarks re-run on reference end-user hardware (Docs/BENCHMARKS.md)
- [ ] State version frozen at 1; StateMigration table reviewed
- [ ] Editor UX pass on a 13-inch laptop + HiDPI display
- [ ] Subjective package ABX sessions run and results filed (no marketing
      claim of historical indistinguishability unless data supports it)

## Packaging
- [ ] Installers (per-platform paths documented), code signing, notarization
- [ ] Factory preset manifest matches Docs/PRESET_GUIDE.md
- [ ] User manual finalized from Docs/USER_MANUAL.md
- [ ] KNOWN_LIMITATIONS.md reviewed and shipped in release notes
