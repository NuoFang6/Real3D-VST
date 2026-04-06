/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Real3DVSTAudioProcessorEditor::Real3DVSTAudioProcessorEditor (Real3DVSTAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), genericEditor (p)
{
    addAndMakeVisible (genericEditor);

    resetButton.setButtonText ("Reset Configuration");
    resetButton.onClick = [this]() {
        for (auto* param : audioProcessor.getParameters()) {
            if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param)) {
                rangedParam->setValueNotifyingHost (rangedParam->getDefaultValue());
            }
        }
    };
    addAndMakeVisible (resetButton);

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (450, 700);
}

Real3DVSTAudioProcessorEditor::~Real3DVSTAudioProcessorEditor()
{
}

//==============================================================================
void Real3DVSTAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void Real3DVSTAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto area = getLocalBounds();
    resetButton.setBounds (area.removeFromBottom (30).reduced (10, 2));
    genericEditor.setBounds (area);
}
