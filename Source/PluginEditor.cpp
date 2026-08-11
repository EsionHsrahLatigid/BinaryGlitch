#include "PluginEditor.h"
#include "PluginProcessor.h"

BinaryGlitchAudioProcessorEditor::BinaryGlitchAudioProcessorEditor (BinaryGlitchAudioProcessor& p)
    : juce::AudioProcessorEditor (&p)
    , generic (p)
{
    addAndMakeVisible (generic);

    // A modest default size. Hosts can still resize if they wish.
    setSize (500, 320);
}

void BinaryGlitchAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Keep paint lightweight. Generic editor draws the controls.
    g.fillAll (juce::Colours::black);
}

void BinaryGlitchAudioProcessorEditor::resized()
{
    generic.setBounds (getLocalBounds().reduced (8));
}
