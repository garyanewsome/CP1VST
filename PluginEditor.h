#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "CyberLookAndFeel.h"

//==============================================================================
class AtFirstJuiceAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    AtFirstJuiceAudioProcessorEditor (AtFirstJuiceAudioProcessor&);
    ~AtFirstJuiceAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // One fader column: the slider, its title label (carries the hover
    // definition) and the parameter attachment.
    struct Control
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Section
    {
        juce::String title;
        std::vector<Control*> controls;
        juce::Rectangle<float> bounds;
    };

    void timerCallback() override;

    void addControl (Control&, const juce::String& paramId, const juce::String& title,
                     const juce::String& tooltip);

    AtFirstJuiceAudioProcessor& audioProcessor;

    CyberLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 350 };

    Control osc1Wave, osc2Wave, oscMix, oscDetune, subLevel;
    Control attack, decay, sustain, release;
    Control cutoff, resonance, filterType;
    Control glide, uniVoices, uniDetune, uniSpread;
    Control fenvAmt, fenvA, fenvD, fenvS, fenvR;
    Control lfoRate, lfoDepth, lfoDest;
    Control drive, masterVol;

    std::vector<Section> sections;

    // On-screen performance wheels (the standalone has no hardware wheels).
    Control pitchCtl, modCtl;
    juce::Slider &pitchWheel = pitchCtl.slider;
    juce::Slider &modWheel = modCtl.slider;

    juce::MidiKeyboardComponent midiKeyboard;
    juce::TextButton randomizeButton;

    // Preset browser (header) and scope.
    juce::ComboBox presetBox;
    juce::TextButton prevPresetButton, nextPresetButton, savePresetButton;
    juce::Array<juce::File> userPresetFiles;
    void refreshPresetList (const juce::String& selectName = {});
    void stepPreset (int delta);
    void promptSavePreset();

    juce::Rectangle<int> scopeBounds;
    std::array<float, 400> scopeData {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtFirstJuiceAudioProcessorEditor)
};
