# Trademark & Naming Review — VintageFilmAudio

Policy: no public UI/preset/mode text may reference films, TV programs,
directors, studios, networks, noise-reduction systems, microphone/recorder/
console/compressor manufacturers, or competing plugins. Historical references
are allowed in internal engineering docs only (this folder).

## Public names shipped in MVP (reviewed, descriptive-only)

Presets: `1950s Theater Dialogue`, `1950s Magnetic Widescreen`,
`Early Television Kinescope`, `1960s Studio Boom`, `1960s TV Sitcom Mono`,
`1960s Dubbed Adventure`, `1970s Location Recorder`, `1977 Optical Stereo`,
`1970s Theatrical Optical`, `1980s Broadcast Mono`, `1984 Television Stereo`,
`1980s Magnetic Mix`, `Late Analog Cinema`, `Multi-Generation Workprint`,
`Worn Archive Print`, `Off-Air Recording` — all descriptive of era/medium/
workflow; none reference a protected work or brand. Year numbers (1977, 1984)
describe eras, not titles. **Cleared.**

Mode/control names: Academy (historical industry term for a playback
characteristic, used descriptively), X-Curve (SMPTE term, descriptive),
Kinescope (generic technical term), Variable Area / Variable Density
(technical terms), MTS/BTSC referenced only in docs, UI says
"Broadcast Stereo". **Cleared.**

## Terms flagged for legal review before commercial release

| Term | Where | Concern | Recommendation |
|---|---|---|---|
| "Academy" | curve name | Academy of Motion Picture Arts & Sciences sensitivities re: "Academy"-branded terms | industry-standard technical usage; obtain counsel sign-off; fallback name "Dialog Norm 1938" |
| "X-Curve" | curve name | SMPTE standard name | descriptive/standard usage, low risk; fallback "Wide-Range Theatrical" |
| "Nagra-style" | internal docs only | trademark (Kudelski/Nagra) | never surfaces in UI ("Field Recorder" used); keep internal only |
| "Dolby-era" | internal docs only | trademark (Dolby) | never surfaces in UI ("Late Analog Cinema"); keep internal only |
| "Kinescope" | preset/mode | genericized US term | low risk; fallback "Film-Off-Monitor" |

No impulse responses, samples, or third-party measured data are shipped; all
noise/artifacts are synthesized (no IR licensing exposure — see ADR-015).
