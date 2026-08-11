#include "PluginProcessor.h"

#include <cmath>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <new>
#include <iostream>
#include <vector>

namespace
{
std::atomic<bool> gCountAllocations { false };
std::atomic<size_t> gAllocationCount { 0 };
}

void* operator new (std::size_t size)
{
    if (gCountAllocations.load(std::memory_order_relaxed))
        gAllocationCount.fetch_add(1, std::memory_order_relaxed);

    if (void* p = std::malloc(size))
        return p;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size)
{
    if (gCountAllocations.load(std::memory_order_relaxed))
        gAllocationCount.fetch_add(1, std::memory_order_relaxed);

    if (void* p = std::malloc(size))
        return p;

    throw std::bad_alloc();
}

void operator delete (void* p) noexcept { std::free(p); }
void operator delete[] (void* p) noexcept { std::free(p); }
void operator delete (void* p, std::size_t) noexcept { std::free(p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free(p); }

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

std::vector<float> renderAfterSeed (float seedNormalised, int numSamples)
{
    BinaryGlitchAudioProcessor processor;
    processor.prepareToPlay (48000.0, numSamples);
    setParameterNormalised (processor, "seed", seedNormalised);

    juce::AudioBuffer<float> buffer (2, numSamples);
    juce::MidiBuffer midi;
    buffer.clear();
    processor.processBlock (buffer, midi);
    requireFinite (buffer);

    std::vector<float> samples;
    samples.reserve(static_cast<size_t>(buffer.getNumChannels() * buffer.getNumSamples()));

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            samples.push_back(buffer.getSample(channel, sample));

    return samples;
}

void requireSameSamples (const std::vector<float>& a, const std::vector<float>& b, const char* message)
{
    require (a.size() == b.size(), message);
    require (std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0, message);
}

void requireDifferentSamples (const std::vector<float>& a, const std::vector<float>& b, const char* message)
{
    require (a.size() == b.size(), message);
    require (std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) != 0, message);
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

    gAllocationCount.store(0, std::memory_order_relaxed);
    gCountAllocations.store(true, std::memory_order_relaxed);
    processClearedBlock (processor, 17);
    gCountAllocations.store(false, std::memory_order_relaxed);
    require (gAllocationCount.load(std::memory_order_relaxed) == 0,
            "seed automation allocated during processBlock");

    processClearedBlock (processor, 1024);

    const auto seedA1 = renderAfterSeed (0.25f, 128);
    const auto seedA2 = renderAfterSeed (0.25f, 128);
    const auto seedB = renderAfterSeed (0.75f, 128);
    requireSameSamples (seedA1, seedA2, "same seed/reset did not render deterministically");
    requireDifferentSamples (seedA1, seedB, "different seeds rendered identical internal source output");

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    processor.releaseResources();
    return 0;
}
