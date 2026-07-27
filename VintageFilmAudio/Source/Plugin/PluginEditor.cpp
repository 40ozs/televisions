#include "PluginEditor.h"
#include "../Parameters/ParameterIDs.h"
#include "../Presets/FactoryPresets.h"

namespace vfa
{
namespace id = vfa::pid;

VfaEditor::VfaEditor (VfaProcessor& p)
    : juce::AudioProcessorEditor (p), processor (p),
      curveDisplay (p),
      inMeter ([&p] { return p.dspEngine().inputPeak(); }, "IN"),
      outMeter ([&p] { return p.dspEngine().outputPeak(); }, "OUT"),
      grMeter ([&p] { return juce::Decibels::decibelsToGain (-p.dspEngine().gainReductionDb()); }, "GR")
{
    setLookAndFeel (nullptr);

    // ---- header
    title.setText ("HEARD IT ON TV", juce::dontSendNotification);
    title.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    addAndMakeVisible (title);

    presetBox.setTextWhenNothingSelected ("Presets...");
    {
        int idx = 1;
        for (const auto& preset : presets::factoryPresets())
            presetBox.addItem (preset.name, idx++);
    }
    presetBox.onChange = [this]
    {
        const int sel = presetBox.getSelectedId();
        if (sel > 0)
            processor.setCurrentProgram (sel - 1);
    };
    addAndMakeVisible (presetBox);

    // Simple A/B audition: two state slots, Copy pushes current to the other.
    snapshotToSlot (slotA);
    snapshotToSlot (slotB);
    abButton.onClick = [this]
    {
        snapshotToSlot (usingSlotA ? slotA : slotB);
        usingSlotA = ! usingSlotA;
        auto& load = usingSlotA ? slotA : slotB;
        if (load.getSize() > 0)
            processor.setStateInformation (load.getData(), (int) load.getSize());
        abButton.setButtonText (usingSlotA ? "A/B" : "B/A");
    };
    copyButton.onClick = [this]
    {
        snapshotToSlot (usingSlotA ? slotB : slotA);
    };
    addAndMakeVisible (abButton);
    addAndMakeVisible (copyButton);

    // ---- context row: era / medium / condition apply profiles on UI change
    auto setupContext = [this] (juce::ComboBox& box, juce::Label& label,
                                std::unique_ptr<ComboAttachment>& att,
                                const char* paramID, const char* text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::FontOptions (11.0f));
        label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
        addAndMakeVisible (label);
        addCombo (box, att, paramID);
        box.onChange = [this] { applyProfileFromUi(); };
    };
    setupContext (eraBox, eraLabel, eraAtt, id::era, "ERA");
    setupContext (mediumBox, mediumLabel, mediumAtt, id::medium, "MEDIUM");
    setupContext (conditionBox, conditionLabel, conditionAtt, id::condition, "CONDITION");

    // ---- macro row
    addRotary (character, id::character, "Character", *this);
    addRotary (fidelity, id::fidelity, "Fidelity", *this);
    addRotary (generation, id::generation, "Generation", *this);
    addRotary (artifacts, id::artifacts, "Artifacts", *this);
    addRotary (noiseMacro, id::noiseMacro, "Noise", *this);

    // ---- chain overview
    chainLabel.setText ("Dynamics > Medium > Transfers > Transport > Delivery > Playback > Noise",
                        juce::dontSendNotification);
    chainLabel.setFont (juce::FontOptions (11.0f));
    chainLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.5f));
    addAndMakeVisible (chainLabel);
    static const char* chainParams[] = { id::dynOn, id::mediumOn, id::genOn, id::transportOn,
                                         id::deliveryOn, id::reproOn, id::noiseOn };
    static const char* chainNames[] = { "Dyn", "Medium", "Gens", "Transport",
                                        "Delivery", "Playback", "Noise" };
    for (size_t i = 0; i < chain.size(); ++i)
    {
        chain[i].toggle.setButtonText (chainNames[i]);
        chain[i].attachment = std::make_unique<ButtonAttachment> (processor.parameters(),
                                                                  chainParams[i], chain[i].toggle);
        addAndMakeVisible (chain[i].toggle);
    }

    // ---- advanced tabs
    auto addTabSlider = [this] (juce::Component& tab, const char* pid_, const char* text)
    {
        auto s = std::make_unique<LabelledSlider>();
        addRotary (*s, pid_, text, tab);
        tabSliders.push_back (std::move (s));
    };
    auto addTabCombo = [this] (juce::Component& tab, juce::ComboBox& box, const char* pid_)
    {
        std::unique_ptr<ComboAttachment> attachment;
        addCombo (box, attachment, pid_);   // fills items from the parameter
        removeChildComponent (&box);        // re-parent from editor to tab
        tab.addAndMakeVisible (box);
        tabCombos.push_back (std::move (attachment));
    };
    auto addTabToggle = [this] (juce::Component& tab, juce::ToggleButton& b, const char* pid_)
    {
        tabButtons.push_back (std::make_unique<ButtonAttachment> (processor.parameters(), pid_, b));
        tab.addAndMakeVisible (b);
    };

    addTabCombo (deliveryTab, curveBox, id::deliveryCurve);
    addTabSlider (deliveryTab, id::delLfRoll, "LF Roll");
    addTabSlider (deliveryTab, id::delHfRoll, "HF Roll");
    addTabSlider (deliveryTab, id::delMidShape, "Mid Shape");
    addTabSlider (deliveryTab, id::delAcademyAmt, "Academy Amt");
    addTabSlider (deliveryTab, id::delXcurveAmt, "X-Curve Amt");
    addTabSlider (deliveryTab, id::delPlaybackSize, "Playback Size");

    addTabCombo (mediumTab, opticalModeBox, id::medOpticalMode);
    addTabCombo (mediumTab, tapeSpeedBox, id::medTapeSpeed);
    addTabSlider (mediumTab, id::medOpticalDist, "Optical Dist");
    addTabSlider (mediumTab, id::medImageSpread, "Image Spread");
    addTabSlider (mediumTab, id::medMagSat, "Mag Sat");
    addTabSlider (mediumTab, id::medHeadBump, "Head Bump");
    addTabSlider (mediumTab, id::medTransSoften, "Soften");
    addTabSlider (mediumTab, id::medCrosstalk, "Crosstalk");
    addTabSlider (mediumTab, id::genVariability, "Gen Vary");
    addTabToggle (mediumTab, genIntegerToggle, id::genInteger);

    addTabCombo (transportTab, transportQualityBox, id::transportQuality);
    addTabSlider (transportTab, id::wow, "Wow");
    addTabSlider (transportTab, id::wowRate, "Wow Rate");
    addTabSlider (transportTab, id::flutter, "Flutter");
    addTabSlider (transportTab, id::flutterRate, "Flut Rate");
    addTabSlider (transportTab, id::drift, "Drift");
    addTabSlider (transportTab, id::scrape, "Scrape");
    addTabToggle (transportTab, stereoLinkToggle, id::stereoLink);

    addTabCombo (noiseTab, humFreqBox, id::nsHumFreq);
    addTabSlider (noiseTab, id::nsHiss, "Hiss");
    addTabSlider (noiseTab, id::nsCell, "Cell");
    addTabSlider (noiseTab, id::nsBroadcast, "Broadcast");
    addTabSlider (noiseTab, id::nsHum, "Hum");
    addTabSlider (noiseTab, id::nsHumHarm, "Harmonics");
    addTabSlider (noiseTab, id::nsBuzz, "Buzz");
    addTabSlider (noiseTab, id::nsCrackle, "Crackle");
    addTabSlider (noiseTab, id::nsDirt, "Dirt");
    addTabSlider (noiseTab, id::nsDropout, "Dropout");
    addTabSlider (noiseTab, id::nsProjector, "Projector");
    addTabSlider (noiseTab, id::nsPrintThrough, "PrintThru");
    addTabSlider (noiseTab, id::nsDuck, "Ducking");
    addTabSlider (noiseTab, id::nsWidth, "Width");
    addTabToggle (noiseTab, nsSilenceToggle, id::nsSilence);

    addTabSlider (dynamicsTab, id::dynAgc, "AGC");
    addTabSlider (dynamicsTab, id::dynComp, "Comp");
    addTabSlider (dynamicsTab, id::dynLimit, "Limit");
    addTabSlider (dynamicsTab, id::dynRelChar, "Release");
    addTabSlider (dynamicsTab, id::dynDialog, "Dialogue");
    addTabSlider (dynamicsTab, id::dynPump, "Pump");

    addTabCombo (outputTab, monoModeBox, id::reproMono);
    addTabCombo (outputTab, monoLawBox, id::monoLaw);
    addTabCombo (outputTab, qualityBox, id::quality);
    addTabCombo (outputTab, auditionBox, id::audition);
    addTabSlider (outputTab, id::reproWidth, "Width");
    addTabSlider (outputTab, id::reproSpeaker, "Speaker");
    addTabSlider (outputTab, id::inTrim, "In Trim");
    addTabSlider (outputTab, id::seed, "Seed");
    addTabToggle (outputTab, autoGainToggle, id::autoGain);
    addTabToggle (outputTab, safetyToggle, id::safetyLimiter);

    tabs.addTab ("Delivery", juce::Colour (0xff1d2026), &deliveryTab, false);
    tabs.addTab ("Medium", juce::Colour (0xff1d2026), &mediumTab, false);
    tabs.addTab ("Transport", juce::Colour (0xff1d2026), &transportTab, false);
    tabs.addTab ("Noise", juce::Colour (0xff1d2026), &noiseTab, false);
    tabs.addTab ("Dynamics", juce::Colour (0xff1d2026), &dynamicsTab, false);
    tabs.addTab ("Output", juce::Colour (0xff1d2026), &outputTab, false);
    addAndMakeVisible (tabs);

    // ---- footer
    addAndMakeVisible (curveDisplay);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);
    addAndMakeVisible (grMeter);
    addRotary (mix, id::mix, "Mix", *this);
    addRotary (outTrim, id::outTrim, "Output", *this);

    setResizable (true, true);
    setResizeLimits (760, 560, 1920, 1400);
    setSize (860, 640);
}

VfaEditor::~VfaEditor() = default;

void VfaEditor::snapshotToSlot (juce::MemoryBlock& slot)
{
    slot.reset();
    processor.getStateInformation (slot);
}

void VfaEditor::applyProfileFromUi()
{
    processor.applyEraProfile();
}

void VfaEditor::addRotary (LabelledSlider& s, const char* paramID, const juce::String& text,
                           juce::Component& parent)
{
    s.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 14);
    s.attachment = std::make_unique<SliderAttachment> (processor.parameters(), paramID, s.slider);
    s.label.setText (text, juce::dontSendNotification);
    s.label.setJustificationType (juce::Justification::centred);
    s.label.setFont (juce::FontOptions (11.0f));
    parent.addAndMakeVisible (s.slider);
    parent.addAndMakeVisible (s.label);
}

void VfaEditor::addCombo (juce::ComboBox& box, std::unique_ptr<ComboAttachment>& att,
                          const char* paramID)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
            processor.parameters().getParameter (paramID)))
    {
        int idx = 1;
        for (const auto& option : choice->choices)
            box.addItem (option, idx++);
    }
    addAndMakeVisible (box);
    att = std::make_unique<ComboAttachment> (processor.parameters(), paramID, box);
}

void VfaEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff23262c));
}

void VfaEditor::resized()
{
    auto area = getLocalBounds().reduced (10);

    // Header
    auto header = area.removeFromTop (30);
    title.setBounds (header.removeFromLeft (220));
    copyButton.setBounds (header.removeFromRight (52));
    abButton.setBounds (header.removeFromRight (52).reduced (2, 0));
    presetBox.setBounds (header.removeFromRight (230).reduced (4, 2));

    // Context row
    auto context = area.removeFromTop (44);
    const int contextWidth = context.getWidth() / 3;
    auto placeContext = [&context, contextWidth] (juce::Label& label, juce::ComboBox& box)
    {
        auto cell = context.removeFromLeft (contextWidth).reduced (4, 0);
        label.setBounds (cell.removeFromTop (14));
        box.setBounds (cell);
    };
    placeContext (eraLabel, eraBox);
    placeContext (mediumLabel, mediumBox);
    placeContext (conditionLabel, conditionBox);
    area.removeFromTop (6);

    // Macro row
    auto macros = area.removeFromTop (92);
    const int macroWidth = macros.getWidth() / 5;
    for (auto* macro : { &character, &fidelity, &generation, &artifacts, &noiseMacro })
    {
        auto cell = macros.removeFromLeft (macroWidth).reduced (6, 0);
        macro->label.setBounds (cell.removeFromBottom (14));
        macro->slider.setBounds (cell);
    }

    // Chain row
    auto chainRow = area.removeFromTop (40);
    chainLabel.setBounds (chainRow.removeFromTop (14));
    const int cellWidth = chainRow.getWidth() / (int) chain.size();
    for (auto& cell : chain)
        cell.toggle.setBounds (chainRow.removeFromLeft (cellWidth).reduced (2, 0));
    area.removeFromTop (6);

    // Footer (claim from the bottom): meters + curve + mix/output
    auto footer = area.removeFromBottom (150);
    inMeter.setBounds (footer.removeFromLeft (34).reduced (2));
    grMeter.setBounds (footer.removeFromLeft (34).reduced (2));
    outMeter.setBounds (footer.removeFromRight (34).reduced (2));
    auto mixArea = footer.removeFromRight (170);
    {
        const int half = mixArea.getWidth() / 2;
        auto mixCell = mixArea.removeFromLeft (half).reduced (4);
        mix.label.setBounds (mixCell.removeFromBottom (14));
        mix.slider.setBounds (mixCell);
        auto outCell = mixArea.reduced (4);
        outTrim.label.setBounds (outCell.removeFromBottom (14));
        outTrim.slider.setBounds (outCell);
    }
    curveDisplay.setBounds (footer.reduced (4));
    area.removeFromBottom (4);

    // Advanced tabs take the rest; lay out each tab as a grid.
    tabs.setBounds (area);
    auto layoutTab = [] (juce::Component& tab)
    {
        auto bounds = tab.getLocalBounds().reduced (8);
        const int columns = std::max (4, bounds.getWidth() / 110);
        const int cw = bounds.getWidth() / columns;
        int x = 0, y = 0;
        const int rowHeight = 86;
        for (auto* child : tab.getChildren())
        {
            if (auto* label = dynamic_cast<juce::Label*> (child))
            {
                juce::ignoreUnused (label);
                continue; // positioned with its slider below
            }
            juce::Rectangle<int> cell (bounds.getX() + x * cw, bounds.getY() + y * rowHeight,
                                       cw, rowHeight);
            if (dynamic_cast<juce::ComboBox*> (child) != nullptr
                || dynamic_cast<juce::ToggleButton*> (child) != nullptr)
                child->setBounds (cell.reduced (4, 28));
            else
                child->setBounds (cell.reduced (4, 2));
            if (++x >= columns) { x = 0; ++y; }
        }
    };
    for (auto* tab : { &deliveryTab, &mediumTab, &transportTab, &noiseTab, &dynamicsTab, &outputTab })
        layoutTab (*tab);

    // Pair slider labels under their sliders inside tabs.
    for (auto& s : tabSliders)
    {
        auto b = s->slider.getBounds();
        s->label.setBounds (b.removeFromBottom (14));
        s->slider.setBounds (s->slider.getBounds().withTrimmedBottom (14));
    }
}

} // namespace vfa
