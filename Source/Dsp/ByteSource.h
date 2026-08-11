#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <vector>

namespace bg
{
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
