/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Real3DVSTAudioProcessor::Real3DVSTAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       // Configuring the constructor with juce::AudioChannelSet::discreteChannels(8)
                       // instructs Equalizer APO to instantiate 8 output channels,
                       // preventing it from downmixing to Stereo.
                       .withInput  ("Input",  juce::AudioChannelSet::discreteChannels(8), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::discreteChannels(8), true)
                     #endif
                       ),
       apvts (*this, nullptr, "Parameters", createParameters())
#endif
{
}

juce::AudioProcessorValueTreeState::ParameterLayout Real3DVSTAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (centerImageID, "Center Image", 0.0f, 1.0f, 0.7f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (shiftID, "Shift", -1.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (depthID, "Depth", 0.0f, 4.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (circularWrapID, "Circular Wrap", 0.0f, 360.0f, 90.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (focusID, "Focus", -1.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (frontSepID, "Front Separation", 0.0f, 2.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (rearSepID, "Rear Separation", 0.0f, 2.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (bassLoID, "Bass Redirect Lo", 0.0f, 150.0f, 40.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (bassHiID, "Bass Redirect Hi", 0.0f, 150.0f, 90.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> (useLfeID, "Use LFE", false));

    juce::StringArray setupNames { "Stereo", "3-Stereo", "4.1 Surround", "5.1 Surround", "5-Stereo", "Legacy 5.1", "6.1 Surround", "7.1 Surround", "7.1 Panorama", "7.1 Tri-Center", "8.1 Surround", "9.1 Wrap", "9.1 Dense Panorama", "11.1 Dense Wrap", "13.1 Total Wrap", "16.1 Surround" };
      params.push_back (std::make_unique<juce::AudioParameterChoice> (channelSetupID, "Output Configuration", setupNames, 7));

    return { params.begin(), params.end() };
}

void Real3DVSTAudioProcessor::updateParameters()
{
    if (!decoder) return;

    decoder->center_image (apvts.getRawParameterValue ("center_image")->load());
    decoder->shift (apvts.getRawParameterValue ("shift")->load());
    decoder->depth (apvts.getRawParameterValue ("depth")->load());
    decoder->circular_wrap (apvts.getRawParameterValue ("circular_wrap")->load());
    decoder->focus (apvts.getRawParameterValue ("focus")->load());
    decoder->front_separation (apvts.getRawParameterValue ("front_sep")->load());
    decoder->rear_separation (apvts.getRawParameterValue ("rear_sep")->load());
    decoder->bass_redirection (apvts.getRawParameterValue ("use_lfe")->load() > 0.5f);
    decoder->low_cutoff (apvts.getRawParameterValue ("bass_lo")->load() / (currentSampleRate / 2.0));
    decoder->high_cutoff (apvts.getRawParameterValue ("bass_hi")->load() / (currentSampleRate / 2.0));

    // 声道设置更新 (通常伴随重置)
    int setupIdx = (int)apvts.getRawParameterValue ("channel_setup")->load();
    channel_setup setups[] = {
        cs_stereo, cs_3stereo, cs_4point1, cs_5point1, cs_5stereo, cs_legacy,
        cs_6point1, cs_7point1, cs_7point1_panorama, cs_7point1_tricenter,
        cs_8point1, cs_9point1_wrap, cs_9point1_densepanorama,
        cs_11point1_densewrap, cs_13point1_totalwrap, cs_16point1
    };

    if (setups[setupIdx] != currentSetup) {
        currentSetup = setups[setupIdx];
        decoder.reset (new freesurround_decoder (currentSetup, fftSize));
        updateParameters(); // 重新应用参数
    }
}

Real3DVSTAudioProcessor::~Real3DVSTAudioProcessor()
{
}

//==============================================================================
const juce::String Real3DVSTAudioProcessor::getName() const
{
    return "Real3D-VST";
}

bool Real3DVSTAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool Real3DVSTAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool Real3DVSTAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double Real3DVSTAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int Real3DVSTAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int Real3DVSTAudioProcessor::getCurrentProgram()
{
    return 0;
}

void Real3DVSTAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String Real3DVSTAudioProcessor::getProgramName (int index)
{
    return {};
}

void Real3DVSTAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void Real3DVSTAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    decoder.reset (new freesurround_decoder (currentSetup, fftSize));
    updateParameters();

    inFifo.setSize (2, fftSize * 4);
    outFifo.setSize (32, fftSize * 4);
    inFifo.clear();
    outFifo.clear();

    fifoReadIdx = 0;
    fifoWriteIdx = 0;
    outFifoReadIdx = 0;
    outFifoWriteIdx = fftSize; // Pre-fill with one block of silence for FIFO buffering

    processInputBuffer.assign (fftSize * 2, 0.0f);

    int latencyP = fftSize + (fftSize / 2);
    // 准备旁通(bypass)延迟线，最多支持16通道的延迟（预留足够通道）
    bypassBuffer.setSize (32, latencyP);
    bypassBuffer.clear();
    bypassWriteIdx = 0;

    // Report total latency to host: FIFO buffering latency (fftSize) + freesurround_decoder algorithmic latency (fftSize / 2)
    setLatencySamples (latencyP);
}

void Real3DVSTAudioProcessor::releaseResources()
{
    decoder.reset();
}

static int getWindowsRank (channel_id id)
{
    switch (id) {
        case ci_front_left: return 0;
        case ci_front_right: return 1;
        case ci_front_center: return 2;
        case ci_lfe: return 3;
        case ci_back_left: return 4;
        case ci_back_right: return 5;
        case ci_front_center_left: return 6;
        case ci_front_center_right: return 7;
        case ci_back_center: return 8;
        case ci_side_center_left: return 9;
        case ci_side_center_right: return 10;
        case ci_side_front_left: return 11;
        case ci_side_front_right: return 12;
        case ci_side_back_left: return 13;
        case ci_side_back_right: return 14;
        case ci_back_center_left: return 15;
        case ci_back_center_right: return 16;
        default: return 999;
    }
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool Real3DVSTAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const int numInputChannels = layouts.getMainInputChannelSet().size();
    const int numOutputChannels = layouts.getMainOutputChannelSet().size();

    if (numInputChannels < 2)
        return false;

    if (numOutputChannels < 2)
        return false;

    return true;
}
#endif

void Real3DVSTAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    updateParameters();

    if (!decoder || totalNumInputChannels < 2) {
        buffer.clear();
        return;
    }

    // Encountering an unhandleable number of channels (e.g., > 2), we skip processing (bypass)
    if (totalNumInputChannels > 2) {
        int delayLen = bypassBuffer.getNumSamples();
        if (delayLen > 0) {
            int maxChans = juce::jmin (totalNumInputChannels, bypassBuffer.getNumChannels());
            for (int ch = 0; ch < maxChans; ++ch) {
                float* inOutData = buffer.getWritePointer (ch);
                float* delayData = bypassBuffer.getWritePointer (ch);
                int idx = bypassWriteIdx;

                for (int s = 0; s < numSamples; ++s) {
                    float inSample = inOutData[s];
                    inOutData[s] = delayData[idx];
                    delayData[idx] = inSample;
                    idx = (idx + 1) % delayLen;
                }
            }
            bypassWriteIdx = (bypassWriteIdx + numSamples) % delayLen;
        }

        for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
            buffer.clear (i, 0, numSamples);
        }
        return;
    }

    auto inLayout = getBusesLayout().getMainInputChannelSet();
    int leftIdx = inLayout.getChannelIndexForType (juce::AudioChannelSet::left);
    int rightIdx = inLayout.getChannelIndexForType (juce::AudioChannelSet::right);
    if (leftIdx < 0) leftIdx = 0;
    if (rightIdx < 0) rightIdx = (totalNumInputChannels > 1) ? 1 : 0;

    // 1. 写输入 FIFO
    for (int s = 0; s < numSamples; ++s) {
        inFifo.setSample (0, (fifoWriteIdx + s) % inFifo.getNumSamples(), buffer.getSample (leftIdx, s));
        inFifo.setSample (1, (fifoWriteIdx + s) % inFifo.getNumSamples(), buffer.getSample (rightIdx, s));
    }
    fifoWriteIdx = (fifoWriteIdx + numSamples) % inFifo.getNumSamples();

    // 2. 处理
    int samplesInInFifo = (fifoWriteIdx - fifoReadIdx + inFifo.getNumSamples()) % inFifo.getNumSamples();
    while (samplesInInFifo >= fftSize) {
        for (int i = 0; i < fftSize; ++i) {
            int idx = (fifoReadIdx + i) % inFifo.getNumSamples();
            processInputBuffer[2 * i] = inFifo.getSample (0, idx);
            processInputBuffer[2 * i + 1] = inFifo.getSample (1, idx);
        }

        float* decoded = decoder->decode (processInputBuffer.data());
        int numOutChannels = decoder->num_channels (currentSetup);

        for (int s = 0; s < fftSize; ++s) {
            for (int c = 0; c < numOutChannels; ++c) {
                outFifo.setSample (c, (outFifoWriteIdx + s) % outFifo.getNumSamples(), decoded[s * numOutChannels + c]);
            }
        }

        fifoReadIdx = (fifoReadIdx + fftSize) % inFifo.getNumSamples();
        outFifoWriteIdx = (outFifoWriteIdx + fftSize) % outFifo.getNumSamples();
        samplesInInFifo = (fifoWriteIdx - fifoReadIdx + inFifo.getNumSamples()) % inFifo.getNumSamples();
    }

    // 3. 读输出 FIFO (考虑到延迟，FreeSurround 内部 decode 已经包含了 N/2 的固定延迟效果)
    // 我们只要保证有数据就读
    int samplesInOutFifo = (outFifoWriteIdx - outFifoReadIdx + outFifo.getNumSamples()) % outFifo.getNumSamples();

    if (samplesInOutFifo < numSamples) {
        buffer.clear();
    } else {
        for (int c = 0; c < totalNumOutputChannels; ++c) {
            buffer.clear (c, 0, numSamples);
        }

        int fsNumChannels = decoder->num_channels (currentSetup);

        // Calculate ranks for all active channels in the decoder
        std::vector<std::pair<int, int>> rankAndIndex; // pair of <WFE_rank, fsChan_index>
        for (int fsChan = 0; fsChan < fsNumChannels; ++fsChan) {
            channel_id cid = freesurround_decoder::channel_at (currentSetup, fsChan);
            int rank = getWindowsRank (cid);
            rankAndIndex.push_back ({rank, fsChan});
        }

        // Sort by WFE rank
        std::sort (rankAndIndex.begin(), rankAndIndex.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

        // Map back to output buffers depending on their sorted position (0 to fsNumChannels-1)
        for (size_t portIndex = 0; portIndex < rankAndIndex.size(); ++portIndex) {
            int fsChan = rankAndIndex[portIndex].second;
            int mappedJuceChan = (int)portIndex;

            if (mappedJuceChan >= 0 && mappedJuceChan < totalNumOutputChannels) {
                for (int s = 0; s < numSamples; ++s) {
                    buffer.addSample (mappedJuceChan, s, outFifo.getSample (fsChan, (outFifoReadIdx + s) % outFifo.getNumSamples()));
                }
            }
        }

        outFifoReadIdx = (outFifoReadIdx + numSamples) % outFifo.getNumSamples();
    }
}

//==============================================================================
bool Real3DVSTAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* Real3DVSTAudioProcessor::createEditor()
{
    return new Real3DVSTAudioProcessorEditor (*this);
}

//==============================================================================
void Real3DVSTAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void Real3DVSTAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Real3DVSTAudioProcessor();
}
