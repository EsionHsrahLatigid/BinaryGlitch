#pragma once

#include <JuceHeader.h>
#include <vector>

namespace bg
{
class Comb
{
public:
    void prepare (double sampleRate, int numChannels, float maxDelayMs = 50.0f)
    {
        sr = juce::jmax(1.0, sampleRate);
        const int maxSamples = juce::jmax(1, static_cast<int>(sr * (maxDelayMs / 1000.0f))) + 1;
        buffers.clear();
        buffers.resize(static_cast<size_t>(juce::jmax(1, numChannels)));
        for (auto& b : buffers)
            b.assign(static_cast<size_t>(maxSamples), 0.0f);
        writeIndex = 0;
        setDelayMs(delayMs);
    }

    void reset()
    {
        for (auto& b : buffers)
            std::fill(b.begin(), b.end(), 0.0f);
        writeIndex = 0;
    }

    void setDelayMs (float ms)
    {
        delayMs = juce::jlimit(1.0f, 50.0f, ms);
        delaySamples = juce::jmax(1, static_cast<int>(sr * (delayMs / 1000.0f)));
    }

    void setFeedback (float fb) noexcept { feedback = juce::jlimit(0.0f, 0.95f, fb); }

    float process (int channel, float input) noexcept
    {
        if (buffers.empty())
            return input;

        const int ch = juce::jlimit(0, static_cast<int>(buffers.size()) - 1, channel);
        auto& buf = buffers[(size_t) ch];

        const int size = (int) buf.size();
        const int readIndex = (writeIndex - delaySamples + size) % size;

        const float delayed = buf[(size_t) readIndex];
        buf[(size_t) writeIndex] = input + delayed * feedback;

        return delayed;
    }

    void advance() noexcept
    {
        if (buffers.empty())
            return;
        const int size = (int) buffers[0].size();
        writeIndex = (writeIndex + 1) % size;
    }

private:
    double sr { 44100.0 };

    std::vector<std::vector<float>> buffers;
    int writeIndex { 0 };

    float delayMs { 12.0f };
    int delaySamples { 512 };
    float feedback { 0.25f };
};

} // namespace bg
