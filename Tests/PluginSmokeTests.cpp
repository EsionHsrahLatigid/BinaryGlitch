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
} // namespace

int main()
{
    BinaryGlitchAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    buffer.clear();

    processor.processBlock (buffer, midi);

    bool hasSignal = false;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto value = buffer.getSample (channel, sample);
            require (std::isfinite (value), "processor emitted non-finite output");
            hasSignal = hasSignal || std::abs (value) > 0.0f;
        }
    }

    require (hasSignal, "processor smoke output stayed silent");

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    processor.releaseResources();
    return 0;
}
