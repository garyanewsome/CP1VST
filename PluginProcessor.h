#pragma once

#include <JuceHeader.h>

//==============================================================================
class AtFirstJuiceSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

// Performance controls shared by the processor (writer) and all voices
// (readers). Plain atomics so the audio thread never blocks.
struct PerformanceState {
    std::atomic<float> pitchBend { 0.0f };  // -1..1, spring-return wheel
    std::atomic<float> modWheel  { 0.0f };  // 0..1, holds its position
    std::atomic<float> lfoPhase  { 0.0f };  // radians, at the start of the block
};

struct VoiceParams {
    const PerformanceState* performance = nullptr;
    std::atomic<float>* lfoRate = nullptr;
    std::atomic<float>* lfoDepth = nullptr;
    std::atomic<float>* lfoDest = nullptr;  // 0 = cutoff, 1 = pitch, 2 = both

    std::atomic<float>* unisonVoices = nullptr; // 0..4 -> 1..5 stacked oscillators
    std::atomic<float>* unisonDetune = nullptr; // 0..1, up to +/-0.5 semitone
    std::atomic<float>* unisonSpread = nullptr; // 0..1 stereo width

    std::atomic<float>* subLevel = nullptr;
    std::atomic<float>* glide = nullptr;        // seconds
    std::atomic<float>* filterType = nullptr;   // 0 LP12, 1 LP24, 2 BP, 3 HP

    std::atomic<float>* cutoff = nullptr;
    std::atomic<float>* resonance = nullptr;
    std::atomic<float>* fenvAmount = nullptr;   // -1..1, +/-5 octaves of cutoff
    std::atomic<float>* fenvAttack = nullptr;
    std::atomic<float>* fenvDecay = nullptr;
    std::atomic<float>* fenvSustain = nullptr;
    std::atomic<float>* fenvRelease = nullptr;

    std::atomic<float>* wave1 = nullptr;
    std::atomic<float>* wave2 = nullptr;
    std::atomic<float>* detune = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* attack = nullptr;
    std::atomic<float>* decay = nullptr;
    std::atomic<float>* sustain = nullptr;
    std::atomic<float>* release = nullptr;
};

//==============================================================================
class AtFirstJuiceVoice : public juce::SynthesiserVoice
{
public:
    AtFirstJuiceVoice(const VoiceParams& params);

    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;
    
    // Real-Time Safe audio render block (no heap allocs, sys calls, or locks)
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

private:
    static constexpr int maxUnison = 5;
    double angle1[maxUnison] = {};   // per-unison-voice phases, osc 1
    double angle2[maxUnison] = {};   // ... and osc 2
    double delta1 = 0.0;
    double delta2 = 0.0;
    double level = 0.0;
    float bendSmoothed = 0.0f;              // semitones, one-pole smoothed
    juce::Random rng;

    struct PinkState { float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f; };
    PinkState pink[2][maxUnison];

    float noiseSample(int choice, PinkState& state);
    juce::ADSR adsr;     // amplitude envelope
    juce::ADSR fenv;     // filter envelope

    // Per-voice 4-pole transistor-ladder filter (tapped at pole 2 for 12 dB).
    // Written in-house rather than juce::dsp::LadderFilter, which smooths the
    // cutoff over a fixed 50 ms and would blur fast filter envelopes.
    // Two ladders (left/right) so the unison stereo image is filtered coherently.
    float ladderL[4] = {}, ladderR[4] = {};
    static float processLadder(float* state, float in, float cutoffHz, float resonance, int type, double sampleRate);

    double angleSub = 0.0;       // sine sub-oscillator, one octave below osc 1
    double glideOffset = 0.0;    // semitones still to slide, decays to 0
    double lastNoteFreq = 0.0;   // this voice's previous note, for glide
    VoiceParams voiceParams;
};

//==============================================================================
class AtFirstJuiceAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    AtFirstJuiceAudioProcessor();
    ~AtFirstJuiceAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    
    // ValueTreeState for atomic parameter access (Thread-Safe & Real-Time Safe)
    juce::AudioProcessorValueTreeState apvts;
    
    // Pitch bend / mod wheel state, driven by MIDI and the on-screen wheels
    PerformanceState performance;

    // Output tap for the header scope: a lock-free ring of the final mono
    // mix. Written on the audio thread, read (tear-tolerant) by the editor.
    static constexpr int scopeSize = 2048;
    std::array<std::atomic<float>, scopeSize> scopeBuffer;
    std::atomic<unsigned> scopeWritePos { 0 };

    // Tracks on-screen piano clicks
    juce::MidiKeyboardState keyboardState;

private:
    int currentProgram = 0;
    juce::Synthesiser synth;
    double lfoPhase = 0.0;      // radians
    double currentSampleRate = 44100.0;
    
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    std::atomic<float>* masterVolumeParam = nullptr;
    std::atomic<float>* lfoRate = nullptr;
    std::atomic<float>* distortionDepth = nullptr;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AtFirstJuiceAudioProcessor)
};
