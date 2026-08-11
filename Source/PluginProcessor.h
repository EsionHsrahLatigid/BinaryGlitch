#pragma once

#include <JuceHeader.h>
#include "Dsp/ByteSource.h"
#include "Dsp/FaultEngine.h"
#include "Dsp/Comb.h"

class BinaryGlitchAudioProcessor final : public juce::AudioProcessor
{
public:
    BinaryGlitchAudioProcessor();
    ~BinaryGlitchAudioProcessor() override = default;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // File loading API (called from editor / message thread)
    bool loadBinaryFile (const juce::File& file, juce::String* outError = nullptr);
    void clearLoadedFile();
    juce::File getLastLoadedFile() const;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;

    // ---- Data source ----
    bg::ByteSource byteSource;
    uint32_t internalSeed { 0u };

    // ---- Transport ----
    double sr { 44100.0 };
    double tickRateHz { 1500.0 };
    double tickIntervalSamples { 29.4 };
    double tickPhaseSamples { 0.0 };

    int64 readPos { 0 };        // byte position (virtual)
    int boundary { 0 };         // 0..(wordSize-1) for desync feel

    // ---- Faulting ----
    bg::FaultEngine fault;
    bg::FastRng modulationRng;

    // ---- Synthesis ----
    struct Voice
    {
        double phase { 0.0 };
        double freq { 220.0 };
        float env { 0.0f };
    };

    Voice voices[2];

    bg::Comb comb;
    float combMix { 0.15f };

    juce::dsp::IIR::Filter<float> hpf[2];
    juce::dsp::IIR::Coefficients<float>::Ptr hpfCoefficients[2];
    float currentHpHz { -1.0f };

    juce::File lastFile;

    // Helpers
    void updateDerived();
    void applyInternalSeed (uint32_t newSeed) noexcept;
    void updateHighPassCoefficients (float hpHz) noexcept;
    float renderTickSample (int channel, uint8_t b0, uint8_t b1, bool gate);
    uint8_t readByteWithFaults (const bg::ByteSource::ByteData* data, int64 pos) const noexcept;
    static uint8_t generateInternalByte (uint32_t sourceSeed, int64 pos) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BinaryGlitchAudioProcessor)
};
