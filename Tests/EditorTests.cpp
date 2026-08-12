#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cstdlib>
#include <iostream>
#include <memory>

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

void checkPaintContract (juce::AudioProcessorEditor& editor)
{
    juce::Image image (juce::Image::RGB, editor.getWidth(), editor.getHeight(), true);
    {
        juce::Graphics g (image);
        editor.paint (g);
    }

    require (image.getPixelAt (0, 2) == ehl::juce_design::Palette::paper(),
             "shared chrome top rule is missing");
    require (image.getPixelAt (ehl::juce_design::Metrics::margin,
                               ehl::juce_design::Metrics::dividerY)
                 == ehl::juce_design::Palette::low(),
             "shared chrome divider is missing");
    require (image.getPixelAt (0, ehl::juce_design::Metrics::headerHeight + 4)
                 == ehl::juce_design::Palette::ink(),
             "editor body background is not EHL ink");
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;

    BinaryGlitchAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    require (dynamic_cast<BinaryGlitchAudioProcessorEditor*> (editor.get()) != nullptr,
             "createEditor must return the branded custom editor");
    require (dynamic_cast<juce::GenericAudioProcessorEditor*> (editor.get()) == nullptr,
             "createEditor must not return a bare GenericAudioProcessorEditor");
    require (editor->getWidth() == BinaryGlitchAudioProcessorEditor::defaultWidth,
             "unexpected default editor width");
    require (editor->getHeight() == BinaryGlitchAudioProcessorEditor::defaultHeight,
             "unexpected default editor height");
    require (editor->getComponentID() == "binaryglitch-editor",
             "missing editor component id");
    require (editor->getWantsKeyboardFocus(), "editor should accept keyboard focus");
    require (editor->findChildWithID ("binaryglitch-control-viewport") != nullptr,
             "branded editor must contain the scrollable parameter surface");

    editor->setBounds (0, 0, BinaryGlitchAudioProcessorEditor::minimumWidth,
                       BinaryGlitchAudioProcessorEditor::minimumHeight);
    editor->resized();

    auto* viewport = editor->findChildWithID ("binaryglitch-control-viewport");
    require (viewport != nullptr && editor->getLocalBounds().contains (viewport->getBounds()),
             "generic controls viewport must stay inside the compact editor bounds");
    require (viewport->getY() >= ehl::juce_design::Metrics::headerHeight,
             "generic controls viewport must sit below the shared header");

    editor->setBounds (0, 0, BinaryGlitchAudioProcessorEditor::defaultWidth,
                       BinaryGlitchAudioProcessorEditor::defaultHeight);
    checkPaintContract (*editor);

    return 0;
}
