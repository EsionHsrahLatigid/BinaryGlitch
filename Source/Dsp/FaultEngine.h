#pragma once

#include <JuceHeader.h>
#include <array>

namespace bg
{
class FastRng
{
public:
    void seed (uint32_t s) noexcept { state = (s == 0u ? 0x9E3779B9u : s); }

    uint32_t nextU32() noexcept
    {
        // xorshift32
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        return x;
    }

    float nextFloat01() noexcept
    {
        // 24-bit mantissa
        return static_cast<float>(nextU32() & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
    }

    int nextInt (int minInclusive, int maxInclusive) noexcept
    {
        if (maxInclusive <= minInclusive)
            return minInclusive;
        const uint32_t r = nextU32();
        const uint32_t span = static_cast<uint32_t>(maxInclusive - minInclusive + 1);
        return minInclusive + static_cast<int>(r % span);
    }

private:
    uint32_t state { 0x9E3779B9u };
};

enum class FaultType
{
    none = 0,
    dropout,
    repeat,
    skip,
    desync,
    bitslip,
    stall
};

struct FaultWeights
{
    float dropout { 0.2f };
    float repeat  { 0.4f };
    float skip    { 0.2f };
    float desync  { 0.1f };
    float bitslip { 0.05f };
    float stall   { 0.05f };

    FaultType sample (FastRng& rng) const noexcept
    {
        const float sum = juce::jmax(0.0001f, dropout + repeat + skip + desync + bitslip + stall);
        float t = rng.nextFloat01() * sum;

        auto take = [&] (float w, FaultType ft)
        {
            if (t < w) return ft;
            t -= w;
            return FaultType::none;
        };

        if (auto r = take(dropout, FaultType::dropout); r != FaultType::none) return r;
        if (auto r = take(repeat,  FaultType::repeat);  r != FaultType::none) return r;
        if (auto r = take(skip,    FaultType::skip);    r != FaultType::none) return r;
        if (auto r = take(desync,  FaultType::desync);  r != FaultType::none) return r;
        if (auto r = take(bitslip, FaultType::bitslip); r != FaultType::none) return r;
        return FaultType::stall;
    }
};

class FaultEngine
{
public:
    void reset (uint32_t seed)
    {
        rng.seed(seed);
        current = FaultType::none;
        remainingTicks = 0;
        intensity = 0.0f;
        desyncOffsetBytes = 0;
        bitSlip = 0;
        repeatBasePos = 0;
    }

    void setWeights (const FaultWeights& w) noexcept { weights = w; }

    FaultType getCurrentFault() const noexcept { return current; }

    bool isFaulting() const noexcept { return current != FaultType::none && remainingTicks > 0; }

    int getDesyncOffsetBytes() const noexcept { return desyncOffsetBytes; }
    int getBitSlip() const noexcept { return bitSlip; }
    int64 getRepeatBasePos() const noexcept { return repeatBasePos; }

    // Call once per tick.
    // tickRateHz: current tick rate (for probability conversion)
    // faultRatePerSec: expected fault starts per second
    // faultLengthTicks: nominal duration in ticks
    // seekRangeBytes: magnitude for skip/repeat
    void tick (double tickRateHz,
               float faultRatePerSec,
               int faultLengthTicks,
               int seekRangeBytes,
               int64 currentPos)
    {
        // Keep a slowly varying "intensity" while faulting.
        if (remainingTicks > 0)
        {
            --remainingTicks;
            intensity = juce::jlimit(0.0f, 1.0f, intensity + (rng.nextFloat01() - 0.5f) * 0.02f);
            if (remainingTicks == 0)
            {
                current = FaultType::none;
                desyncOffsetBytes = 0;
                bitSlip = 0;
            }
            return;
        }

        current = FaultType::none;

        const double safeTick = juce::jmax(1.0, tickRateHz);
        const float pStart = juce::jlimit(0.0f, 0.5f, faultRatePerSec / static_cast<float>(safeTick));

        if (rng.nextFloat01() < pStart)
        {
            current = weights.sample(rng);

            const int minLen = juce::jmax(1, faultLengthTicks / 2);
            const int maxLen = juce::jmax(minLen, faultLengthTicks + faultLengthTicks / 2);
            remainingTicks = rng.nextInt(minLen, maxLen);

            intensity = rng.nextFloat01();

            // Precompute per-fault parameters
            desyncOffsetBytes = 0;
            bitSlip = 0;
            repeatBasePos = currentPos;

            switch (current)
            {
                case FaultType::none:
                case FaultType::dropout:
                case FaultType::skip:
                case FaultType::stall:
                    break;

                case FaultType::desync:
                    desyncOffsetBytes = rng.nextInt(-2, 2);
                    if (desyncOffsetBytes == 0) desyncOffsetBytes = 1;
                    break;

                case FaultType::bitslip:
                    bitSlip = rng.nextInt(-3, 3);
                    if (bitSlip == 0) bitSlip = 1;
                    break;

                case FaultType::repeat:
                {
                    const int span = juce::jmax(8, seekRangeBytes);
                    repeatBasePos = currentPos - rng.nextInt(0, span);
                    break;
                }
            }
        }
    }

private:
    FastRng rng;
    FaultWeights weights;

    FaultType current { FaultType::none };
    int remainingTicks { 0 };
    float intensity { 0.0f };

    int desyncOffsetBytes { 0 };
    int bitSlip { 0 };
    int64 repeatBasePos { 0 };
};

} // namespace bg
