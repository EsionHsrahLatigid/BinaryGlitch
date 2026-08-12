#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
juce::Rectangle<int> genericEditorBounds (juce::Rectangle<int> bounds) noexcept
{
    return bounds.withTrimmedTop (ehl::juce_design::Metrics::headerHeight + 8)
                 .reduced (ehl::juce_design::Metrics::margin, 8);
}
} // namespace

BinaryGlitchAudioProcessorEditor::BinaryGlitchAudioProcessorEditor (BinaryGlitchAudioProcessor& p)
    : juce::AudioProcessorEditor (&p)
    , generic (p)
{
    setLookAndFeel (&lookAndFeel);
    generic.setLookAndFeel (&lookAndFeel);
    generic.setComponentID ("binaryglitch-generic-controls");
    controlsViewport.setComponentID ("binaryglitch-control-viewport");
    controlsViewport.setViewedComponent (&generic, false);
    controlsViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (controlsViewport);

    setResizeLimits (minimumWidth, minimumHeight,
                     ehl::juce_design::Metrics::maximumWidth,
                     ehl::juce_design::Metrics::maximumHeight);
    setResizable (true, true);
    setName ("BinaryGlitch editor");
    setComponentID ("binaryglitch-editor");
    setTitle ("BinaryGlitch");
    setDescription ("BinaryGlitch monochrome 8-bit branded editor");
    setWantsKeyboardFocus (true);
    setSize (defaultWidth, defaultHeight);
}

BinaryGlitchAudioProcessorEditor::~BinaryGlitchAudioProcessorEditor()
{
    generic.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void BinaryGlitchAudioProcessorEditor::paint (juce::Graphics& g)
{
    ehl::juce_design::paintEditorChrome (g, getLocalBounds(), "BinaryGlitch", "BUFFER FAULT");
}

void BinaryGlitchAudioProcessorEditor::resized()
{
    const auto controls = genericEditorBounds (getLocalBounds());
    controlsViewport.setBounds (controls);
    generic.setBounds (0, 0, controls.getWidth(), juce::jmax (controls.getHeight(), generic.getHeight()));
}
