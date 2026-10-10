#pragma once

#include <JuceHeader.h>

//==============================================================================
// Patch handling: factory starters plus user patches stored as small XML files.
// Every preset is a list of {parameter id, real (un-normalised) value}; any
// parameter a preset doesn't mention is reset to its default so patches don't
// leak settings into each other. MASTER_VOL is never touched by a preset.
//==============================================================================
namespace Presets
{
    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;
    };

    // Waveform choice indices: 0 sine, 1 saw, 2 triangle, 3 white, 4 pink.
    // LFO_DEST indices:        0 cutoff, 1 pitch, 2 both.
    inline const std::vector<Preset>& factory()
    {
        static const std::vector<Preset> presets = {
            { "Init", {} },

            { "Neon Pluck",
              { { "WAVEFORM", 1 }, { "OSC2_WAVEFORM", 1 }, { "OSC2_DETUNE", 0.12f }, { "OSC_MIX", 0.5f },
                { "ATTACK", 0.002f }, { "DECAY", 0.25f }, { "SUSTAIN", 0.0f }, { "RELEASE", 0.15f },
                { "CUTOFF", 400.0f }, { "RESONANCE", 0.35f },
                { "FENV_AMOUNT", 0.75f }, { "FENV_ATTACK", 0.001f }, { "FENV_DECAY", 0.22f },
                { "FENV_SUSTAIN", 0.0f }, { "FENV_RELEASE", 0.2f },
                { "DISTORTION", 1.5f } } },

            { "Chrome Pad",
              { { "WAVEFORM", 1 }, { "OSC2_WAVEFORM", 1 }, { "OSC2_DETUNE", 0.15f }, { "OSC_MIX", 0.5f },
                { "ATTACK", 1.2f }, { "DECAY", 1.0f }, { "SUSTAIN", 0.8f }, { "RELEASE", 2.0f },
                { "CUTOFF", 2500.0f }, { "RESONANCE", 0.15f },
                { "FENV_AMOUNT", 0.3f }, { "FENV_ATTACK", 1.5f }, { "FENV_DECAY", 1.5f },
                { "FENV_SUSTAIN", 0.6f }, { "FENV_RELEASE", 2.0f },
                { "LFO_RATE", 0.4f }, { "LFO_DEPTH", 0.25f }, { "LFO_DEST", 0 } } },

            { "Acid Sub",
              { { "WAVEFORM", 1 }, { "OSC2_WAVEFORM", 2 }, { "OSC2_DETUNE", -12.0f }, { "OSC_MIX", 0.4f },
                { "ATTACK", 0.001f }, { "DECAY", 0.3f }, { "SUSTAIN", 0.5f }, { "RELEASE", 0.1f },
                { "CUTOFF", 300.0f }, { "RESONANCE", 0.7f },
                { "FENV_AMOUNT", 0.8f }, { "FENV_ATTACK", 0.001f }, { "FENV_DECAY", 0.18f },
                { "FENV_SUSTAIN", 0.1f }, { "FENV_RELEASE", 0.15f },
                { "DISTORTION", 3.0f } } },

            { "Glass Bell",
              { { "WAVEFORM", 0 }, { "OSC2_WAVEFORM", 2 }, { "OSC2_DETUNE", 12.0f }, { "OSC_MIX", 0.35f },
                { "ATTACK", 0.001f }, { "DECAY", 1.8f }, { "SUSTAIN", 0.0f }, { "RELEASE", 1.5f },
                { "CUTOFF", 12000.0f } } },

            { "Blade Lead",
              { { "WAVEFORM", 1 }, { "OSC2_WAVEFORM", 1 }, { "OSC2_DETUNE", 0.2f }, { "OSC_MIX", 0.5f },
                { "ATTACK", 0.01f }, { "DECAY", 0.2f }, { "SUSTAIN", 0.8f }, { "RELEASE", 0.25f },
                { "CUTOFF", 4500.0f }, { "RESONANCE", 0.25f },
                { "FENV_AMOUNT", 0.2f }, { "FENV_DECAY", 0.3f }, { "FENV_SUSTAIN", 0.5f },
                { "LFO_RATE", 5.5f }, { "LFO_DEPTH", 0.15f }, { "LFO_DEST", 1 },
                { "DISTORTION", 2.5f } } },

            { "Acid Rain",
              { { "WAVEFORM", 4 }, { "OSC2_WAVEFORM", 1 }, { "OSC_MIX", 0.3f },
                { "ATTACK", 0.6f }, { "DECAY", 1.0f }, { "SUSTAIN", 0.5f }, { "RELEASE", 1.5f },
                { "CUTOFF", 1800.0f }, { "RESONANCE", 0.5f },
                { "FENV_AMOUNT", 0.4f }, { "FENV_ATTACK", 0.8f }, { "FENV_DECAY", 1.2f },
                { "FENV_SUSTAIN", 0.4f }, { "FENV_RELEASE", 1.5f },
                { "LFO_RATE", 0.3f }, { "LFO_DEPTH", 0.6f }, { "LFO_DEST", 0 } } },

            { "Static Wind",
              { { "WAVEFORM", 3 }, { "OSC2_WAVEFORM", 4 }, { "OSC_MIX", 0.5f },
                { "ATTACK", 1.5f }, { "DECAY", 2.0f }, { "SUSTAIN", 0.6f }, { "RELEASE", 2.5f },
                { "CUTOFF", 900.0f }, { "RESONANCE", 0.6f },
                { "FENV_AMOUNT", 0.5f }, { "FENV_ATTACK", 2.0f }, { "FENV_DECAY", 2.0f },
                { "FENV_SUSTAIN", 0.5f }, { "FENV_RELEASE", 2.5f },
                { "LFO_RATE", 0.15f }, { "LFO_DEPTH", 0.5f }, { "LFO_DEST", 0 } } },
        };
        return presets;
    }

    inline bool isPerformanceParam (const juce::String& id) { return id == "MASTER_VOL"; }

    // Reset every sound parameter to its default, then apply the given values.
    inline void applyValues (juce::AudioProcessor& proc,
                       const std::vector<std::pair<juce::String, float>>& values)
    {
        for (auto* p : proc.getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
            if (ranged == nullptr || isPerformanceParam (ranged->getParameterID()))
                continue;

            float target = ranged->getDefaultValue(); // normalised
            for (const auto& kv : values)
                if (kv.first == ranged->getParameterID())
                    target = ranged->convertTo0to1 (kv.second);

            ranged->beginChangeGesture();
            ranged->setValueNotifyingHost (target);
            ranged->endChangeGesture();
        }
    }

    inline void applyFactory (juce::AudioProcessor& proc, int index)
    {
        const auto& list = factory();
        if (index < 0 || index >= (int) list.size())
            return;
        std::vector<std::pair<juce::String, float>> v;
        for (const auto& kv : list[(size_t) index].values)
            v.emplace_back (kv.first, kv.second);
        applyValues (proc, v);
    }

    //==========================================================================
    inline juce::File userDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("CP1").getChildFile ("Presets");
    }

    inline juce::Array<juce::File> userPresetFiles()
    {
        juce::Array<juce::File> files;
        userDirectory().findChildFiles (files, juce::File::findFiles, false, "*.cp1preset");
        files.sort();
        return files;
    }

    inline bool saveUser (juce::AudioProcessor& proc, const juce::String& name)
    {
        auto dir = userDirectory();
        if (! dir.createDirectory().wasOk())
            return false;

        juce::XmlElement xml ("CP1Preset");
        xml.setAttribute ("name", name);
        for (auto* p : proc.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (! isPerformanceParam (ranged->getParameterID()))
                {
                    auto* e = xml.createNewChildElement ("PARAM");
                    e->setAttribute ("id", ranged->getParameterID());
                    e->setAttribute ("value", (double) ranged->convertFrom0to1 (ranged->getValue()));
                }

        return xml.writeTo (dir.getChildFile (juce::File::createLegalFileName (name) + ".cp1preset"));
    }

    inline bool loadUser (juce::AudioProcessor& proc, const juce::File& file)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName ("CP1Preset"))
            return false;

        std::vector<std::pair<juce::String, float>> values;
        for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
            values.emplace_back (e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("value"));
        applyValues (proc, values);
        return true;
    }
} // namespace Presets
