#pragma once

#include <JuceHeader.h>

class BinaryGlitchAudioProcessor;

// UI is intentionally minimal: we embed JUCE's GenericAudioProcessorEditor.
// This keeps the plugin usable while keeping the codebase focused on DSP.
class BinaryGlitchAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit BinaryGlitchAudioProcessorEditor (BinaryGlitchAudioProcessor&);
    ~BinaryGlitchAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::GenericAudioProcessorEditor generic;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BinaryGlitchAudioProcessorEditor)
};
