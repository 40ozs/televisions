# User Manual (Draft) — Heard it on TV (Oddity Audio)

## What this plugin is

Heard it on TV transforms clean audio into historically plausible film and
television sound from roughly 1950–1989 by emulating the **whole delivery
chain**: period dynamics processing, the storage medium (optical, magnetic
film, tape, broadcast, kinescope), repeated transfer generations, the
theatrical or broadcast playback curve, the reproduction system, and the
noise/mechanical artifacts of the era. It is not a generic lo-fi effect: each
stage is modeled and documented separately.

## Quick start

1. Pick a **factory preset** (they span early-50s optical mono to late-80s
   MTS stereo TV), or
2. Choose **Era → Medium → Condition**. Selecting these writes a coherent
   set of advanced parameters (you can edit everything afterwards — your
   edits are never silently overwritten; macros modulate on top without
   rewriting your values).
3. Shape with the five macros:
   - **Character** – darker/rounder (optical-leaning) ↔ forward/denser
     (magnetic/broadcast-leaning)
   - **Fidelity** – overall bandwidth, distortion, and noise-floor quality
   - **Generation** – how many transfer generations the "print" is from the
     master (HF loss, noise, wow/flutter and softening accumulate)
   - **Artifacts** – clicks, crackle, dropouts, projector texture
   - **Noise** – continuous noise beds (hiss, cell noise, hum, buzz)
4. Balance with **Mix** and **Output**; enable **Auto Gain** for level-matched
   A/B (bounded ±6 dB — it will not hide real dynamic changes).

## The delivery curves

- **Academy** — the classic theatrical "dialog norm" playback characteristic
  (steep HF rolloff: ≈ −10 dB @ 5 kHz, −18 dB @ 8 kHz), used from the late
  1930s well into the 1970s in mono optical theatres.
- **X-Curve / X-Curve Small Room** — wide-range theatrical calibration of the
  post-1970s era. **Historical note:** the X-Curve is a *room calibration
  target*, not an EQ that mixers printed onto film. This plugin applies it as
  a transfer response to emulate what the theatrical chain did to the sound —
  an explicit, documented approximation.
- **Early TV / Kinescope / Broadcast Mono / Late TV** — television-chain
  responses from 1950s sets through late-analog ~15 kHz broadcast.
- **Neutral** — no curve.

## Audition tools

- **Audition: Artifacts Only** — hear just the added noise/artifact layer.
- **Audition: Noise Muted** — the processed chain without the noise layer.
- **Random Seed** — 0 gives a fresh (but render-stable) artifact pattern per
  session; any other value makes every render bit-identical.

## Latency

The transport (wow/flutter) stage uses a fixed 12 ms delay line that is
always in circuit so latency never jumps; the host is told about it and
compensates automatically.

## Signal chain

Input Trim → Period Dynamics (AGC/compressor/limiter/dialogue focus) →
Medium model → Generation loss → Transport (wow/flutter) → Delivery curve →
Reproduction (width/mono/small-speaker) → Noise & artifacts → Safety limiter
→ Auto gain → Mix → Output trim.
