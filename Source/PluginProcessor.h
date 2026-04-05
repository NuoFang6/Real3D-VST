/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/freesurround_decoder.h" // 包含你的原版头文件

//==============================================================================
/**
 */
class Real3DVSTAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    Real3DVSTAudioProcessor();
    ~Real3DVSTAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    //==============================================================================
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

private:
    // 你的解码器实例指针
    std::unique_ptr<freesurround_decoder> decoder;

    // 用于保存当前的采样率，以便在参数更新时使用
    double currentSampleRate = 44100.0;

    // 默认声道配置 (5.1)
    channel_setup currentSetup = cs_5point1;

    // 参数 ID 定义
    static inline const juce::ParameterID 
        centerImageID {"center_image", 1},
        shiftID {"shift", 1},
        depthID {"depth", 1},
        circularWrapID {"circular_wrap", 1},
        focusID {"focus", 1},
        frontSepID {"front_sep", 1},
        rearSepID {"rear_sep", 1},
        bassLoID {"bass_lo", 1},
        bassHiID {"bass_hi", 1},
        useLfeID {"use_lfe", 1},
        channelSetupID {"channel_setup", 1};

    // 内部函数用于同步参数到解码器
    void updateParameters();

    // 管理插件参数 (旋钮)
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

    // 用于处理不规则 Block Size 的缓冲区
    juce::AudioBuffer<float> inFifo;
    juce::AudioBuffer<float> outFifo;
    int fifoReadIdx = 0;
    int fifoWriteIdx = 0;
    int outFifoReadIdx = 0;
    int outFifoWriteIdx = 0;
    static constexpr int fftSize = 4096; // 核心处理块大小

    // 临时缓冲区用于交错/反交错
    std::vector<float> processInputBuffer; // 2 * fftSize
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Real3DVSTAudioProcessor)
};
