// Reproduction stage (DSP_SPEC §3/§7): width -> mono fold-down -> small
// speaker model -> DC blocker.
//
//  * Width acts on M/S for 2-channel streams: S is scaled by width
//    (width 1 = unchanged) — energy-safe, mid untouched.
//  * Mono mode: `narrow` scales S by an additional 0.35 (~-9 dB side);
//    `mono` folds fully with the selected sum law, out = (L+R) * g,
//    g = 0.707 (-3 dB) / 0.596 (-4.5 dB) / 0.5 (-6 dB), written to both
//    channels. Realised as out = mid*mg +- side*sg with mg = 2g, sg = 0.
//  * Small speaker (amount a, equal-power dry/wet morph):
//      highpass 12 dB/oct @ 120+180a Hz (Q 0.9 — slightly underdamped
//        closed-box alignment; also keeps 150 Hz alive at a = 0.5),
//      lowpass 12 dB/oct @ 8000-3500a Hz (Q 0.707),
//      cabinet resonance +4 dB @ 180+120a Hz Q 2.2,
//      presence +2.5 dB @ 2.8 kHz Q 1.2,
//      excursion nonlinearity fastTanh(d*x)/d with d = 2.75a
//        (~1 % THD at -18 dBFS when a == 1).
//    The dry leg runs through two unity-magnitude all-passes that share the
//    HP/LP poles, phase-aligning it with the wet leg so the morph does not
//    comb-cancel near the corners (verified: at a = 0.5, 150 Hz sits ~-6.4 dB
//    and 6 kHz ~-3.4 dB re 1 kHz — NOT a telephone band-pass). The blend is
//    normalised by 1/(cos+sin) so coherent mid-band program stays near unity.
//    Coefficients are recomputed only when `amount` changes materially.
//  * DC blocker per channel, always on.
//
// All control values (side gain, mid gain, speaker amount) are smoothed by
// the one-pole smoothers; the first block after prepare/reset snaps them to
// their targets so there is no artificial fade-in.

#include "Reproduction.h"

namespace vfa::dsp
{

namespace
{
// Mono fold-down sum gains: out = (L+R) * g.
float monoLawGain (MonoLaw law) noexcept
{
    switch (law)
    {
        case MonoLaw::minus3dB:   return 0.7071068f;
        case MonoLaw::minus4_5dB: return 0.5956621f;
        case MonoLaw::minus6dB:   return 0.5f;
    }
    return 0.7071068f;
}

// Copy `f`'s denominator into `ap` as a unity-magnitude all-pass
// (numerator = reversed denominator). Preserves ap's filter state.
void makeAllpassFrom (const Biquad& f, Biquad& ap) noexcept
{
    ap.b0 = f.a2;
    ap.b1 = f.a1;
    ap.b2 = 1.0f;
    ap.a1 = f.a1;
    ap.a2 = f.a2;
}
} // namespace

void Reproduction::prepare (const StreamSpec& spec)
{
    streamSpec = spec;
    channels.assign ((size_t) spec.numChannels, {});
    for (auto& c : channels)
        c.dc.prepare (spec.sampleRate);
    widthSmoother.setCutoff (8.0f, spec.sampleRate);
    monoSmoother.setCutoff (8.0f, spec.sampleRate);
    speakerSmoother.setCutoff (8.0f, spec.sampleRate);
    lastSpeakerAmount = -1.0f;
    primed = false;
    reset();
}

void Reproduction::reset()
{
    for (auto& c : channels)
    {
        c.speakerHp.reset();
        c.speakerLp.reset();
        c.cabinetRes.reset();
        c.presence.reset();
        c.dryAllpassHp.reset();
        c.dryAllpassLp.reset();
        c.dc.reset();
    }
    widthSmoother.reset();
    monoSmoother.reset();
    speakerSmoother.reset();
    primed = false;
}

void Reproduction::process (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snap)
{
    const int n = buffer.getNumSamples();
    const int numCh = std::min (buffer.getNumChannels(), (int) channels.size());
    if (n <= 0 || numCh <= 0)
        return;

    auto* const* ch = buffer.getArrayOfWritePointers();

    // Control targets. Width/mono act in M/S: out = mid*mg +- side*sg.
    const float width = std::clamp (snap.width, 0.0f, 1.0f);
    float sideTarget = width, midTarget = 1.0f;
    if (snap.monoMode == MonoMode::narrow)
        sideTarget *= 0.35f;
    else if (snap.monoMode == MonoMode::mono)
    {
        sideTarget = 0.0f;
        midTarget  = 2.0f * monoLawGain (snap.monoLaw);   // (L+R)*g == mid*2g
    }
    const float spkTarget = std::clamp (snap.smallSpeaker, 0.0f, 1.0f);

    if (! primed)
    {
        primed = true;
        widthSmoother.z   = sideTarget;
        monoSmoother.z    = midTarget;
        speakerSmoother.z = spkTarget;
    }

    // Speaker coefficients follow the SMOOTHED amount, not the raw target:
    // a coarse host automation step would otherwise jump four biquads at
    // once and click (caught by Realtime/ClickTest). While the smoother is
    // travelling this recomputes every block — bounded, small deltas.
    const float spkSmoothed = speakerSmoother.state();
    if (std::abs (spkSmoothed - lastSpeakerAmount) > 0.005f
        || (std::abs (spkTarget - lastSpeakerAmount) > 0.005f
            && std::abs (spkTarget - spkSmoothed) < 0.005f))
    {
        lastSpeakerAmount = std::abs (spkTarget - spkSmoothed) < 0.005f ? spkTarget : spkSmoothed;
        const double sr = streamSpec.sampleRate;
        const float a = lastSpeakerAmount;
        for (auto& c : channels)
        {
            c.speakerHp.highpass (sr, 120.0f + 180.0f * a, 0.9f);
            c.speakerLp.lowpass  (sr, 8000.0f - 3500.0f * a, 0.7071f);
            c.cabinetRes.peak    (sr, 180.0f + 120.0f * a, 2.2f, 4.0f);
            c.presence.peak      (sr, 2800.0f, 1.2f, 2.5f);
            makeAllpassFrom (c.speakerHp, c.dryAllpassHp);
            makeAllpassFrom (c.speakerLp, c.dryAllpassLp);
        }
    }

    // Stage-activity flags (zero-amount stages cost no CPU). A stage stays
    // active while its smoother still has distance to travel.
    const bool foldActive = numCh >= 2
        && (snap.monoMode != MonoMode::stereo
            || sideTarget < 0.9999f
            || std::abs (widthSmoother.state() - sideTarget) > 1.0e-4f
            || std::abs (monoSmoother.state()  - midTarget)  > 1.0e-4f);
    const bool spkActive = spkTarget > 1.0e-4f
        || speakerSmoother.state() > 1.0e-4f;

    for (int i = 0; i < n; ++i)
    {
        const float sg = widthSmoother.process (sideTarget);
        const float mg = monoSmoother.process (midTarget);
        const float a  = speakerSmoother.process (spkTarget);

        if (foldActive)
        {
            const float l = ch[0][i], r = ch[1][i];
            const float mid  = 0.5f * (l + r);
            const float side = 0.5f * (l - r);
            ch[0][i] = mg * mid + sg * side;
            ch[1][i] = mg * mid - sg * side;
        }

        if (spkActive)
        {
            const float dryG  = std::cos (a * kPi * 0.5f);
            const float wetG  = std::sin (a * kPi * 0.5f);
            const float norm  = 1.0f / std::max (dryG + wetG, 1.0f);
            const float drive = 2.75f * a;
            // Near zero the phase-aligning all-passes still colour the "dry"
            // leg, so the activity gate would jump between all-passed and raw
            // signal (ClickTest). Blend the whole stage in continuously over
            // the first 2 % of the amount range instead.
            const float stageMix = std::min (a * 50.0f, 1.0f);

            for (int c = 0; c < numCh; ++c)
            {
                auto& s = channels[(size_t) c];
                const float x = ch[c][i];
                const float dry = s.dryAllpassLp.process (s.dryAllpassHp.process (x));
                float wet = s.speakerLp.process (
                                s.presence.process (
                                    s.cabinetRes.process (
                                        s.speakerHp.process (x))));
                if (drive > 0.01f)
                    wet = fastTanh (drive * wet) / drive;   // excursion limit
                const float staged = norm * (dryG * dry + wetG * wet);
                ch[c][i] = x + stageMix * (staged - x);
            }
        }

        for (int c = 0; c < numCh; ++c)
            ch[c][i] = channels[(size_t) c].dc.process (ch[c][i]);
    }
}

} // namespace vfa::dsp
