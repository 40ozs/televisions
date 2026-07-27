#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vfa::ui
{

// Simple peak meter fed from an atomic getter; 30 Hz UI timer, ballistic
// decay, repaints only on visible change.
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    explicit LevelMeter (std::function<float()> getterFn, juce::String labelText)
        : getter (std::move (getterFn)), label (std::move (labelText))
    {
        startTimerHz (30);
        setInterceptsMouseClicks (false, false);
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        auto barArea = area.removeFromTop (area.getHeight() - 14.0f).reduced (2.0f);
        g.setColour (juce::Colour (0xff14161a));
        g.fillRoundedRectangle (barArea, 3.0f);

        const float dB = juce::Decibels::gainToDecibels (display, -60.0f);
        const float t = juce::jlimit (0.0f, 1.0f, (dB + 60.0f) / 60.0f);
        auto fill = barArea.removeFromBottom (barArea.getHeight() * t);
        g.setColour (dB > -3.0f ? juce::Colour (0xffcc5544)
                    : dB > -12.0f ? juce::Colour (0xffd9a648)
                                  : juce::Colour (0xff5d9c73));
        g.fillRoundedRectangle (fill, 3.0f);

        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.setFont (10.0f);
        g.drawText (label, getLocalBounds().removeFromBottom (12), juce::Justification::centred);
    }

private:
    void timerCallback() override
    {
        const float v = getter ? getter() : 0.0f;
        const float next = std::max (v, display * 0.85f);   // decay ballistics
        if (std::abs (next - display) > 1.0e-4f)
        {
            display = next;
            repaint();
        }
    }

    std::function<float()> getter;
    juce::String label;
    float display = 0.0f;
};

} // namespace vfa::ui
