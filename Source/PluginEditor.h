/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class Real3DVSTAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    Real3DVSTAudioProcessorEditor (Real3DVSTAudioProcessor&);
    ~Real3DVSTAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    Real3DVSTAudioProcessor& audioProcessor;
    
    juce::GenericAudioProcessorEditor genericEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Real3DVSTAudioProcessorEditor)
};
