#include "PluginProcessor.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
void require (bool condition, const char* message)
{
    if (! condition)
    {
        std::cerr << message << '\n';
        std::exit (1);
    }
}

void requireFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            require (std::isfinite (buffer.getSample (channel, sample)), "processor emitted non-finite output");
}

bool hasAnySignal (const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (std::abs (buffer.getSample (channel, sample)) > 0.0f)
                return true;

    return false;
}

void processClearedBlock (BinaryGlitchAudioProcessor& processor, int numSamples)
{
    juce::AudioBuffer<float> buffer (2, numSamples);
    juce::MidiBuffer midi;
    buffer.clear();
    processor.processBlock (buffer, midi);
    requireFinite (buffer);
    require (hasAnySignal (buffer), "processor smoke output stayed silent");
}

void setParameterNormalised (BinaryGlitchAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.getAPVTS().getParameter (id);
    require (parameter != nullptr, "missing expected parameter");
    parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, value));
}
} // namespace

int main()
{
    BinaryGlitchAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    buffer.clear();

    processor.processBlock (buffer, midi);
    requireFinite (buffer);
    require (hasAnySignal (buffer), "processor smoke output stayed silent");

    setParameterNormalised (processor, "seed", 0.25f);
    setParameterNormalised (processor, "dcHpHz", 1.0f);
    setParameterNormalised (processor, "combDelayMs", 0.75f);
    setParameterNormalised (processor, "combFb", 0.65f);
    processClearedBlock (processor, 32);
    processClearedBlock (processor, 511);

    setParameterNormalised (processor, "seed", 0.75f);
    setParameterNormalised (processor, "dcHpHz", 0.0f);
    setParameterNormalised (processor, "combMix", 0.5f);
    processClearedBlock (processor, 17);
    processClearedBlock (processor, 1024);

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    processor.releaseResources();
    return 0;
}
