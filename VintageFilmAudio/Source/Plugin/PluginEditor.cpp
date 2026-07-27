#include "PluginEditor.h"

namespace vfa
{

VfaEditor::VfaEditor (VfaProcessor& p)
    : juce::AudioProcessorEditor (p), processor (p), generic (p)
{
    addAndMakeVisible (generic);
    setResizable (true, true);
    setResizeLimits (420, 400, 1600, 1600);
    setSize (560, 720);
}

void VfaEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void VfaEditor::resized()
{
    generic.setBounds (getLocalBounds());
}

} // namespace vfa
