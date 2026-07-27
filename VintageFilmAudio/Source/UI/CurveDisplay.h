#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Plugin/PluginProcessor.h"
#include "../DSP/DeliveryCurves/DeliveryCurveModule.h"

namespace vfa::ui
{

// Effective delivery-response display. Recomputed on a low-rate UI timer on
// the message thread from the parameter snapshot (never touches audio-thread
// data); repaints only when the curve actually changed (SAS §5 / UI reqs:
// no continuous repainting).
class CurveDisplay : public juce::Component, private juce::Timer
{
public:
    explicit CurveDisplay (VfaProcessor& p) : processor (p)
    {
        startTimer (250);
        refresh();
        setInterceptsMouseClicks (false, false);
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour (0xff14161a));
        g.fillRoundedRectangle (area, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));

        // Grid: decades 100 Hz / 1 kHz / 10 kHz and -12/-24 dB lines.
        for (float hz : { 100.0f, 1000.0f, 10000.0f })
        {
            const float x = xForHz (hz, area);
            g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        }
        for (float dB : { 0.0f, -12.0f, -24.0f })
        {
            const float y = yForDb (dB, area);
            g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        }

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (10.0f);
        g.drawText ("100", (int) xForHz (100.0f, area) + 2, (int) area.getBottom() - 12, 30, 10,
                    juce::Justification::left);
        g.drawText ("1k", (int) xForHz (1000.0f, area) + 2, (int) area.getBottom() - 12, 30, 10,
                    juce::Justification::left);
        g.drawText ("10k", (int) xForHz (10000.0f, area) + 2, (int) area.getBottom() - 12, 30, 10,
                    juce::Justification::left);

        if (curve.size() < 2)
            return;
        juce::Path path;
        for (size_t i = 0; i < curve.size(); ++i)
        {
            const float hz = hzForIndex (i);
            const float x = xForHz (hz, area);
            const float y = yForDb (curve[i], area);
            if (i == 0) path.startNewSubPath (x, y);
            else path.lineTo (x, y);
        }
        g.setColour (juce::Colour (0xffd9a648));
        g.strokePath (path, juce::PathStrokeType (1.8f));
    }

private:
    static constexpr int numPoints = 256;
    static constexpr float minHz = 25.0f, maxHz = 20000.0f;
    static constexpr float minDb = -36.0f, maxDb = 12.0f;

    float hzForIndex (size_t i) const
    {
        return minHz * std::pow (maxHz / minHz, float (i) / float (numPoints - 1));
    }
    static float xForHz (float hz, juce::Rectangle<float> a)
    {
        const float t = std::log (hz / minHz) / std::log (maxHz / minHz);
        return a.getX() + t * a.getWidth();
    }
    static float yForDb (float dB, juce::Rectangle<float> a)
    {
        const float t = (dB - maxDb) / (minDb - maxDb);
        return a.getY() + juce::jlimit (0.0f, 1.0f, t) * a.getHeight();
    }

    void refresh()
    {
        const auto snap = processor.buildSnapshot();
        // 2048-bin grid at nominal 48 kHz, resampled to display points.
        const double sr = 48000.0;
        auto grid = dsp::DeliveryCurveModule::effectiveMagnitudeGrid (snap, 2048, sr);
        std::vector<float> next ((size_t) numPoints);
        for (int i = 0; i < numPoints; ++i)
        {
            const float hz = hzForIndex ((size_t) i);
            const float bin = juce::jlimit (0.0f, (float) grid.size() - 1.0f,
                                            (float) (hz / (sr * 0.5) * (grid.size() - 1)));
            const auto lo = (size_t) bin;
            const auto hi = std::min (lo + 1, grid.size() - 1);
            const float frac = bin - (float) lo;
            const float mag = grid[lo] + frac * (grid[hi] - grid[lo]);
            next[(size_t) i] = dsp::gainToDb (mag);
        }
        if (next != curve)
        {
            curve = std::move (next);
            repaint();
        }
    }

    void timerCallback() override { refresh(); }

    VfaProcessor& processor;
    std::vector<float> curve;
};

} // namespace vfa::ui
