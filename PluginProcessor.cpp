#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

//==============================================================================
CP1Voice::CP1Voice(const VoiceParams& p)
    : voiceParams(p)
{
    // Basic envelope to prevent clicking on noteOn/noteOff
    juce::ADSR::Parameters params;
    params.attack = 0.01f;
    params.decay = 0.1f;
    params.sustain = 0.8f;
    params.release = 0.1f;
    adsr.setParameters(params);
}

bool CP1Voice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<CP1Sound*>(sound) != nullptr;
}

void CP1Voice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int /*currentPitchWheelPosition*/)
{
    adsr.setSampleRate(getSampleRate());
    adsr.noteOn();
    fenv.setSampleRate(getSampleRate());
    fenv.noteOn();
    for (auto& stage : ladderL) stage = 0.0f;
    for (auto& stage : ladderR) stage = 0.0f;

    // Unison voices start at spread-out phases so they don't add up as one
    // loud click; voice 0 starts at zero like the plain oscillator.
    for (int u = 0; u < maxUnison; ++u)
    {
        angle1[u] = u == 0 ? 0.0 : rng.nextDouble() * 2.0 * juce::MathConstants<double>::pi;
        angle2[u] = u == 0 ? 0.0 : rng.nextDouble() * 2.0 * juce::MathConstants<double>::pi;
    }
    level = velocity;
    bendSmoothed = voiceParams.performance != nullptr
                       ? voiceParams.performance->pitchBend.load() * 2.0f  // +/-2 semitone range
                       : 0.0f;
    
    auto cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);

    // Glide: start from where this voice's previous note left off.
    const float glideTime = voiceParams.glide != nullptr ? voiceParams.glide->load() : 0.0f;
    glideOffset = (glideTime > 0.0f && lastNoteFreq > 0.0)
                      ? 12.0 * std::log2(lastNoteFreq / cyclesPerSecond)
                      : 0.0;
    lastNoteFreq = cyclesPerSecond;
    angleSub = 0.0;
    delta1 = (cyclesPerSecond / getSampleRate()) * 2.0 * juce::MathConstants<double>::pi;
}

void CP1Voice::stopNote(float /*velocity*/, bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff();
        fenv.noteOff();
    }
    else
    {
        clearCurrentNote();
        adsr.reset();
        fenv.reset();
    }
}

void CP1Voice::pitchWheelMoved(int) {}
void CP1Voice::controllerMoved(int, int) {}

float CP1Voice::processLadder(float* ladder, float in, float cutoffHz, float res, int type, double sampleRate)
{
    // One-pole coefficient from cutoff, resonance feedback from pole 4, tanh
    // saturation in every stage (Huovilainen-style), 12 dB output at pole 2.
    const float g = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * cutoffHz / (float) sampleRate);
    const float k = res * 3.8f;

    const float x = std::tanh(in - k * ladder[3]);
    ladder[0] += g * (x - std::tanh(ladder[0]));
    ladder[1] += g * (std::tanh(ladder[0]) - std::tanh(ladder[1]));
    ladder[2] += g * (std::tanh(ladder[1]) - std::tanh(ladder[2]));
    ladder[3] += g * (std::tanh(ladder[2]) - std::tanh(ladder[3]));

    const float makeup = 1.0f + k * 0.25f; // modest make-up for resonance bass loss
    switch (type)
    {
        case 1:  return ladder[3] * makeup;                         // LP24
        case 2:  return (ladder[1] - ladder[3]) * 2.0f * makeup;    // band-pass
        case 3:  return (x - 2.0f * ladder[0] + ladder[1]) * makeup; // high-pass
        default: return ladder[1] * makeup;                         // LP12
    }
}

float CP1Voice::noiseSample(int choice, PinkState& st)
{
    const float white = rng.nextFloat() * 2.0f - 1.0f;
    if (choice == 3) // White
        return white * 0.7f;

    // Pink: Paul Kellet's economy filter, normalised to roughly +/-1
    st.b0 = 0.99765f * st.b0 + white * 0.0990460f;
    st.b1 = 0.96300f * st.b1 + white * 0.2965164f;
    st.b2 = 0.57000f * st.b2 + white * 1.0526913f;
    return (st.b0 + st.b1 + st.b2 + white * 0.1848f) * 0.25f;
}

void CP1Voice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (!isVoiceActive())
        return;

    constexpr double twoPi = 2.0 * juce::MathConstants<double>::pi;
    constexpr float bendRangeSemitones = 2.0f;
    constexpr float vibratoSemitones = 1.0f;

    const int wave1Choice = voiceParams.wave1 != nullptr ? static_cast<int>(voiceParams.wave1->load()) : 0;
    const int wave2Choice = voiceParams.wave2 != nullptr ? static_cast<int>(voiceParams.wave2->load()) : 0;
    const float detuneVal = voiceParams.detune != nullptr ? voiceParams.detune->load() : 0.0f;
    const float mixVal = voiceParams.mix != nullptr ? voiceParams.mix->load() : 0.5f;

    // Osc 2 base phase increment from detune (semitones): ratio = 2 ^ (detune / 12)
    const double detuneRatio = std::pow(2.0, detuneVal / 12.0);
    delta2 = delta1 * detuneRatio;

    juce::ADSR::Parameters adsrParams;
    adsrParams.attack = voiceParams.attack != nullptr ? voiceParams.attack->load() : 0.01f;
    adsrParams.decay = voiceParams.decay != nullptr ? voiceParams.decay->load() : 0.1f;
    adsrParams.sustain = voiceParams.sustain != nullptr ? voiceParams.sustain->load() : 0.8f;
    adsrParams.release = voiceParams.release != nullptr ? voiceParams.release->load() : 0.1f;
    adsr.setParameters(adsrParams);

    juce::ADSR::Parameters fenvParams;
    fenvParams.attack = voiceParams.fenvAttack != nullptr ? voiceParams.fenvAttack->load() : 0.01f;
    fenvParams.decay = voiceParams.fenvDecay != nullptr ? voiceParams.fenvDecay->load() : 0.2f;
    fenvParams.sustain = voiceParams.fenvSustain != nullptr ? voiceParams.fenvSustain->load() : 0.5f;
    fenvParams.release = voiceParams.fenvRelease != nullptr ? voiceParams.fenvRelease->load() : 0.2f;
    fenv.setParameters(fenvParams);

    const float subLevelVal = voiceParams.subLevel != nullptr ? voiceParams.subLevel->load() : 0.0f;
    const float glideTime = voiceParams.glide != nullptr ? voiceParams.glide->load() : 0.0f;
    const int filterTypeVal = voiceParams.filterType != nullptr ? static_cast<int>(voiceParams.filterType->load()) : 0;
    const double glideCoeff = glideTime > 0.0f ? std::exp(-4.0 / ((double) glideTime * getSampleRate())) : 0.0;

    const float baseCutoff = voiceParams.cutoff != nullptr ? voiceParams.cutoff->load() : 20000.0f;
    const float resonanceVal = voiceParams.resonance != nullptr ? voiceParams.resonance->load() : 0.0f;
    const float fenvAmt = voiceParams.fenvAmount != nullptr ? voiceParams.fenvAmount->load() : 0.0f;
    const double sr = getSampleRate();
    const float maxCutoff = juce::jmin(20000.0f, (float) sr * 0.45f);

    // Performance: pitch wheel, and LFO vibrato scaled by depth + mod wheel.
    const auto* perf = voiceParams.performance;
    const float bendTarget = perf != nullptr ? perf->pitchBend.load() * bendRangeSemitones : 0.0f;
    const float modWheel = perf != nullptr ? perf->modWheel.load() : 0.0f;
    const float lfoDepthVal = voiceParams.lfoDepth != nullptr ? voiceParams.lfoDepth->load() : 0.0f;
    const int lfoDest = voiceParams.lfoDest != nullptr ? static_cast<int>(voiceParams.lfoDest->load()) : 0;
    const float lfoRateVal = voiceParams.lfoRate != nullptr ? voiceParams.lfoRate->load() : 1.0f;
    const float depth = juce::jlimit(0.0f, 1.0f, lfoDepthVal + modWheel);
    const float vibrato = lfoDest >= 1 ? depth * vibratoSemitones : 0.0f;
    const float cutoffLfo = (lfoDest == 0 || lfoDest == 2) ? depth : 0.0f;
    const double lfoStart = perf != nullptr ? (double) perf->lfoPhase.load() : 0.0;
    const double lfoInc = twoPi * lfoRateVal / getSampleRate();

    auto polyBlep = [](double t, double dt) {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    };

    // phase in radians, dt = phase increment per sample in radians
    auto getSample = [&](int choice, double angle, double inc, PinkState& st) {
        if (choice == 0) // Sine
            return static_cast<float>(std::sin(angle));
        if (choice == 1) // Saw (band-limited with PolyBLEP)
        {
            const double t = angle / twoPi, dt = inc / twoPi;
            return static_cast<float>(2.0 * t - 1.0 - polyBlep(t, dt));
        }
        if (choice == 2) // Triangle
            return static_cast<float>((2.0 / juce::MathConstants<double>::pi) * std::asin(std::sin(angle)));
        return noiseSample(choice, st); // White / Pink
    };

    // Unison: N stacked copies of each oscillator, detuned symmetrically and
    // panned across the stereo field. Gains are constant-power per voice.
    const int numUnison = voiceParams.unisonVoices != nullptr
                              ? juce::jlimit(1, maxUnison, static_cast<int>(voiceParams.unisonVoices->load()) + 1)
                              : 1;
    const float uniDetune = voiceParams.unisonDetune != nullptr ? voiceParams.unisonDetune->load() : 0.0f;
    const float uniSpread = voiceParams.unisonSpread != nullptr ? voiceParams.unisonSpread->load() : 0.0f;

    double unisonMul[maxUnison];
    float gainL[maxUnison], gainR[maxUnison];
    const float norm = 1.0f / std::sqrt((float) numUnison);
    for (int u = 0; u < numUnison; ++u)
    {
        const float pos = numUnison == 1 ? 0.0f : (2.0f * (float) u / (float) (numUnison - 1)) - 1.0f; // -1..1
        unisonMul[u] = std::exp2((double) (pos * uniDetune * 0.5f) / 12.0);
        const float theta = (pos * uniSpread + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        gainL[u] = std::cos(theta) * juce::MathConstants<float>::sqrt2 * norm;
        gainR[u] = std::sin(theta) * juce::MathConstants<float>::sqrt2 * norm;
    }

    const int numChannels = outputBuffer.getNumChannels();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float env = adsr.getNextSample();

        bendSmoothed += (bendTarget - bendSmoothed) * 0.005f;
        const float lfoVal = static_cast<float>(std::sin(lfoStart + lfoInc * (double) (startSample + sample)));
        glideOffset *= glideCoeff;
        if (std::abs(glideOffset) < 1.0e-4) glideOffset = 0.0;
        const double pitchMul = std::exp2(((double) (bendSmoothed + lfoVal * vibrato) + glideOffset) / 12.0);
        const double inc1 = delta1 * pitchMul;
        const double inc2 = delta2 * pitchMul;

        float sumL = 0.0f, sumR = 0.0f;
        for (int u = 0; u < numUnison; ++u)
        {
            const double i1 = inc1 * unisonMul[u];
            const double i2 = inc2 * unisonMul[u];

            const float s1 = getSample(wave1Choice, angle1[u], i1, pink[0][u]);
            const float s2 = getSample(wave2Choice, angle2[u], i2, pink[1][u]);

            // Linear mix: 0.0 = Osc 1 only, 1.0 = Osc 2 only
            const float mixed = s1 * (1.0f - mixVal) + s2 * mixVal;
            sumL += mixed * gainL[u];
            sumR += mixed * gainR[u];

            angle1[u] += i1;
            while (angle1[u] >= twoPi) angle1[u] -= twoPi;
            angle2[u] += i2;
            while (angle2[u] >= twoPi) angle2[u] -= twoPi;
        }

        // Sub oscillator sits in the centre of the image.
        const float sub = subLevelVal * 0.8f * static_cast<float>(std::sin(angleSub));
        sumL += sub;
        sumR += sub;

        // Filter cutoff: base, +/-5 octaves of envelope, then LFO sweep
        const float fEnv = fenv.getNextSample();
        float cut = baseCutoff * std::exp2(fenvAmt * fEnv * 5.0f);
        cut *= 1.0f - cutoffLfo * 0.5f * (lfoVal + 1.0f);
        cut = juce::jlimit(20.0f, maxCutoff, cut);

        const float amp = (float) level * env;
        const float outL = processLadder(ladderL, sumL, cut, resonanceVal, filterTypeVal, sr) * amp;
        const float outR = numChannels > 1
                               ? processLadder(ladderR, sumR, cut, resonanceVal, filterTypeVal, sr) * amp
                               : 0.0f;

        if (numChannels == 1)
        {
            outputBuffer.addSample(0, startSample + sample, outL);
        }
        else
        {
            outputBuffer.addSample(0, startSample + sample, outL);
            outputBuffer.addSample(1, startSample + sample, outR);
        }

        angleSub += inc1 * 0.5;
        while (angleSub >= twoPi) angleSub -= twoPi;

        if (!adsr.isActive())
        {
            clearCurrentNote();
            fenv.reset();
            break;
        }
    }
}

//==============================================================================
CP1AudioProcessor::CP1AudioProcessor()
    : juce::AudioProcessor(BusesProperties()
                           .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    for (auto& v : scopeBuffer)
        v.store(0.0f, std::memory_order_relaxed);

    synth.addSound(new CP1Sound());
    
    VoiceParams waveParams;
    waveParams.performance = &performance;
    waveParams.lfoRate = apvts.getRawParameterValue("LFO_RATE");
    waveParams.lfoDepth = apvts.getRawParameterValue("LFO_DEPTH");
    waveParams.lfoDest = apvts.getRawParameterValue("LFO_DEST");
    waveParams.unisonVoices = apvts.getRawParameterValue("UNISON_VOICES");
    waveParams.unisonDetune = apvts.getRawParameterValue("UNISON_DETUNE");
    waveParams.unisonSpread = apvts.getRawParameterValue("UNISON_SPREAD");
    waveParams.subLevel = apvts.getRawParameterValue("SUB_LEVEL");
    waveParams.glide = apvts.getRawParameterValue("GLIDE");
    waveParams.filterType = apvts.getRawParameterValue("FILTER_TYPE");
    waveParams.cutoff = apvts.getRawParameterValue("CUTOFF");
    waveParams.resonance = apvts.getRawParameterValue("RESONANCE");
    waveParams.fenvAmount = apvts.getRawParameterValue("FENV_AMOUNT");
    waveParams.fenvAttack = apvts.getRawParameterValue("FENV_ATTACK");
    waveParams.fenvDecay = apvts.getRawParameterValue("FENV_DECAY");
    waveParams.fenvSustain = apvts.getRawParameterValue("FENV_SUSTAIN");
    waveParams.fenvRelease = apvts.getRawParameterValue("FENV_RELEASE");
    waveParams.wave1 = apvts.getRawParameterValue("WAVEFORM");
    waveParams.wave2 = apvts.getRawParameterValue("OSC2_WAVEFORM");
    waveParams.detune = apvts.getRawParameterValue("OSC2_DETUNE");
    waveParams.mix = apvts.getRawParameterValue("OSC_MIX");
    waveParams.attack = apvts.getRawParameterValue("ATTACK");
    waveParams.decay = apvts.getRawParameterValue("DECAY");
    waveParams.sustain = apvts.getRawParameterValue("SUSTAIN");
    waveParams.release = apvts.getRawParameterValue("RELEASE");
    
    // Add polyphonic voices
    for (int i = 0; i < 8; ++i)
        synth.addVoice(new CP1Voice(waveParams));
        
    masterVolumeParam = apvts.getRawParameterValue("MASTER_VOL");
    lfoRate = apvts.getRawParameterValue("LFO_RATE");
    distortionDepth = apvts.getRawParameterValue("DISTORTION");
}

CP1AudioProcessor::~CP1AudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CP1AudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("WAVEFORM", 1), "OSC 1 Wave", juce::StringArray{"Sine", "Saw", "Triangle", "White", "Pink"}, 0));
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("OSC2_WAVEFORM", 1), "OSC 2 Wave", juce::StringArray{"Sine", "Saw", "Triangle", "White", "Pink"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("OSC2_DETUNE", 1), "Detune", -12.0f, 12.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("OSC_MIX", 1), "OSC Mix", 0.0f, 1.0f, 0.5f));
        
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("MASTER_VOL", 1), "Master Vol", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
        
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("ATTACK", 1), "Attack", 0.001f, 2.0f, 0.01f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("DECAY", 1), "Decay", 0.001f, 2.0f, 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("SUSTAIN", 1), "Sustain", 0.0f, 1.0f, 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("RELEASE", 1), "Release", 0.001f, 4.0f, 0.1f));
        
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("CUTOFF", 1), "Cutoff", juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), 20000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("RESONANCE", 1), "Resonance", 0.0f, 1.0f, 0.0f));
        
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("LFO_RATE", 1), "LFO Rate Hz", 0.1f, 20.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("LFO_DEPTH", 1), "LFO Depth", 0.0f, 1.0f, 0.0f));
        
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("FENV_AMOUNT", 1), "Filter Env Amount", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("FENV_ATTACK", 1), "Filter Env Attack", 0.001f, 2.0f, 0.01f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("FENV_DECAY", 1), "Filter Env Decay", 0.001f, 2.0f, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("FENV_SUSTAIN", 1), "Filter Env Sustain", 0.0f, 1.0f, 0.4f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("FENV_RELEASE", 1), "Filter Env Release", 0.001f, 4.0f, 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("UNISON_VOICES", 1), "Unison Voices",
        juce::StringArray{"1", "2", "3", "4", "5"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("UNISON_DETUNE", 1), "Unison Detune", 0.0f, 1.0f, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("UNISON_SPREAD", 1), "Unison Spread", 0.0f, 1.0f, 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("SUB_LEVEL", 1), "Sub Level", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("GLIDE", 1), "Glide",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.001f, 0.4f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("FILTER_TYPE", 1), "Filter Type",
        juce::StringArray{"LP12", "LP24", "BP", "HP"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("LFO_DEST", 1), "LFO Dest", juce::StringArray{"Cutoff", "Pitch", "Both"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("DISTORTION", 1), "Drive", 1.0f, 10.0f, 1.0f));
        
    return { params.begin(), params.end() };
}

//==============================================================================
const juce::String CP1AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool CP1AudioProcessor::acceptsMidi() const
{
    return true;
}

bool CP1AudioProcessor::producesMidi() const
{
    return false;
}

bool CP1AudioProcessor::isMidiEffect() const
{
    return false;
}

double CP1AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int CP1AudioProcessor::getNumPrograms()
{
    return (int) Presets::factory().size();
}

int CP1AudioProcessor::getCurrentProgram()
{
    return currentProgram;
}

void CP1AudioProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    currentProgram = index;
    Presets::applyFactory(*this, index);
}

const juce::String CP1AudioProcessor::getProgramName(int index)
{
    if (index < 0 || index >= getNumPrograms())
        return {};
    return Presets::factory()[(size_t) index].name;
}

void CP1AudioProcessor::changeProgramName(int /*index*/, const juce::String& /*newName*/) {}

//==============================================================================
void CP1AudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    // Inform synthesiser of sample rate changes
    synth.setCurrentPlaybackSampleRate(sampleRate);
    
    currentSampleRate = sampleRate;
    lfoPhase = 0.0;
}

void CP1AudioProcessor::releaseResources()
{
}

bool CP1AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void CP1AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
        
    // Merges incoming clicks from the UI keyboard into the buffer stream
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
        
    // Pitch wheel and mod wheel (CC1) arrive as MIDI; hold the latest values.
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isPitchWheel())
            performance.pitchBend.store((float) (msg.getPitchWheelValue() - 8192) / 8192.0f);
        else if (msg.isController() && msg.getControllerNumber() == 1)
            performance.modWheel.store((float) msg.getControllerValue() / 127.0f);
    }

    // Voices read the LFO phase at the start of this block and advance it
    // themselves per sample, so vibrato is smooth rather than block-stepped.
    performance.lfoPhase.store((float) lfoPhase);

    // Render the active synth voices
    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

    // Cutoff, envelope and LFO filter modulation all run per voice now; the
    // processor only advances the shared LFO phase the voices read.
    const float currentLfoRate = lfoRate != nullptr ? lfoRate->load() : 1.0f;
    const double lfoInc = 2.0 * juce::MathConstants<double>::pi * currentLfoRate / currentSampleRate;
    const int numSamples = buffer.getNumSamples();

    lfoPhase = std::fmod(lfoPhase + lfoInc * (double) numSamples, 2.0 * juce::MathConstants<double>::pi);

    // Apply soft-clipping analog waveshaper naturally before the main volume envelope
    float drive = distortionDepth != nullptr ? distortionDepth->load() : 1.0f;
    if (drive > 1.0f)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                // Divide the hyperbolic tangent logically by its max offset threshold to prevent volume spikes
                channelData[sample] = static_cast<float>(std::tanh(channelData[sample] * drive) / std::tanh(drive));
            }
        }
    }

    // Apply master volume using thread-safe, non-blocking atomic fetch
    float volume = masterVolumeParam != nullptr ? masterVolumeParam->load() : 0.5f;
    buffer.applyGain(volume);

    // Feed the scope from the first channel of the final output.
    if (buffer.getNumChannels() > 0)
    {
        const float* out = buffer.getReadPointer(0);
        unsigned w = scopeWritePos.load(std::memory_order_relaxed);
        for (int i = 0; i < numSamples; ++i)
            scopeBuffer[(w + (unsigned) i) % (unsigned) scopeSize].store(out[i], std::memory_order_relaxed);
        scopeWritePos.store(w + (unsigned) numSamples, std::memory_order_release);
    }
}

//==============================================================================
bool CP1AudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* CP1AudioProcessor::createEditor()
{
    return new CP1AudioProcessorEditor(*this);
}

//==============================================================================
void CP1AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Serialize parameter values
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void CP1AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Deserialize parameter values
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CP1AudioProcessor();
}
