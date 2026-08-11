#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <vector>

namespace bg
{
struct Lfsr32
{
    uint32_t state { 0x12345678u };

    void seed (uint32_t s) noexcept
    {
        state = (s == 0u ? 0x12345678u : s);
    }

    // 32-bit Galois LFSR. Polynomial: x^32 + x^22 + x^2 + x + 1
    // Tap mask 0x80200003 is commonly used for maximal-length sequences.
    uint32_t next() noexcept
    {
        const uint32_t lsb = state & 1u;
        state >>= 1u;
        if (lsb != 0u)
            state ^= 0x80200003u;
        return state;
    }

    uint8_t nextByte() noexcept
    {
        return static_cast<uint8_t>(next() & 0xFFu);
    }
};

class ByteSource
{
public:
    using ByteData = std::vector<uint8_t>;

    void setData (std::shared_ptr<const ByteData> newData)
    {
        std::atomic_store(&data, std::move(newData));
    }

    std::shared_ptr<const ByteData> getData() const
    {
        return std::atomic_load(&data);
    }

    bool hasData() const
    {
        auto d = getData();
        return d != nullptr && ! d->empty();
    }

    void clear()
    {
        std::atomic_store(&data, std::shared_ptr<const ByteData>());
    }

    static std::shared_ptr<const ByteData> loadFileToMemory (const juce::File& file,
                                                            size_t maxBytes,
                                                            juce::String* outError = nullptr)
    {
        if (! file.existsAsFile())
        {
            if (outError) *outError = "File does not exist.";
            return {};
        }

        juce::FileInputStream fis (file);
        if (! fis.openedOk())
        {
            if (outError) *outError = "Could not open file.";
            return {};
        }

        const int64 fileSize = fis.getTotalLength();
        if (fileSize <= 0)
        {
            if (outError) *outError = "Empty file.";
            return {};
        }

        const size_t toRead = static_cast<size_t>(juce::jmin<int64>(fileSize, static_cast<int64>(maxBytes)));
        auto bytes = std::make_shared<ByteData>();
        bytes->resize(toRead);

        const int actuallyRead = fis.read (bytes->data(), static_cast<int>(toRead));
        if (actuallyRead <= 0)
        {
            if (outError) *outError = "Could not read file.";
            return {};
        }

        bytes->resize(static_cast<size_t>(actuallyRead));
        return bytes;
    }

private:
    std::shared_ptr<const ByteData> data;
};

} // namespace bg
