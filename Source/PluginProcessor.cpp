/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>

//==============================================================================
Real3DVSTAudioProcessor::Real3DVSTAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::discreteChannels(17), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::discreteChannels(17), true)
#endif
                         ),
      apvts(*this, nullptr, "Parameters", createParameters())
#endif
{
}

juce::AudioProcessorValueTreeState::ParameterLayout Real3DVSTAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(centerImageID, "Center Image", 0.0f, 1.0f, 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(shiftID, "Shift", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(depthID, "Depth", 0.0f, 4.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(circularWrapID, "Circular Wrap", 0.0f, 360.0f, 90.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(focusID, "Focus", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(frontSepID, "Front Separation", 0.0f, 2.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(rearSepID, "Rear Separation", 0.0f, 2.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(bassLoID, "Bass Redirect Lo", 0.0f, 150.0f, 80.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(bassHiID, "Bass Redirect Hi", 0.0f, 150.0f, 111.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(useLfeID, "Use LFE", true));

    juce::StringArray setupNames{"Stereo", "3-Stereo", "4.1 Surround", "5.1 Surround",
                                 "5-Stereo", "Legacy 5.1", "6.1 Surround", "7.1 Surround", "7.1 Panorama",
                                 "7.1 Tri-Center", "8.1 Surround", "9.1 Wrap", "9.1 Dense Panorama",
                                 "11.1 Dense Wrap", "13.1 Total Wrap", "16.1 Surround"};
    params.push_back(std::make_unique<juce::AudioParameterChoice>(channelSetupID, "Output Configuration", setupNames, 7));

    return {params.begin(), params.end()};
}

void Real3DVSTAudioProcessor::updateParameters()
{
    if (!decoder)
        return;

    decoder->center_image(apvts.getRawParameterValue("center_image")->load());
    decoder->shift(apvts.getRawParameterValue("shift")->load());
    decoder->depth(apvts.getRawParameterValue("depth")->load());
    decoder->circular_wrap(apvts.getRawParameterValue("circular_wrap")->load());
    decoder->focus(apvts.getRawParameterValue("focus")->load());
    decoder->front_separation(apvts.getRawParameterValue("front_sep")->load());
    decoder->rear_separation(apvts.getRawParameterValue("rear_sep")->load());
    decoder->bass_redirection(apvts.getRawParameterValue("use_lfe")->load() > 0.5f);

    // Normalise Hz → [0,1] using the *actual* sample rate (works for 44.1k / 48k / 96k …)
    decoder->low_cutoff(apvts.getRawParameterValue("bass_lo")->load() / (currentSampleRate / 2.0));
    decoder->high_cutoff(apvts.getRawParameterValue("bass_hi")->load() / (currentSampleRate / 2.0));

    // channel-setup update (may rebuild the decoder)
    int setupIdx = (int)apvts.getRawParameterValue("channel_setup")->load();
    channel_setup setups[] = {
        cs_stereo, cs_3stereo, cs_4point1, cs_5point1, cs_5stereo, cs_legacy,
        cs_6point1, cs_7point1, cs_7point1_panorama, cs_7point1_tricenter,
        cs_8point1, cs_9point1_wrap, cs_9point1_densepanorama,
        cs_11point1_densewrap, cs_13point1_totalwrap, cs_16point1};

    if (setups[setupIdx] != currentSetup)
    {
        currentSetup = setups[setupIdx];
        decoder.reset(new freesurround_decoder(currentSetup, fftSize));
        decoder->set_sample_rate(currentSampleRate);
        setupChanged = true; // FIFOs will be reset in processBlock
        updateParameters();  // re-apply all parameters to the new decoder
    }
}

Real3DVSTAudioProcessor::~Real3DVSTAudioProcessor()
{
}

const char *Real3DVSTAudioProcessor::channelIdToName(channel_id id)
{
    switch (id)
    {
    case ci_front_left:
        return "FL";
    case ci_front_right:
        return "FR";
    case ci_front_center:
        return "FC";
    case ci_lfe:
        return "LFE";
    case ci_back_left:
        return "BL";
    case ci_back_right:
        return "BR";
    case ci_front_center_left:
        return "FCL";
    case ci_front_center_right:
        return "FCR";
    case ci_back_center:
        return "BC";
    case ci_side_center_left:
        return "SCL";
    case ci_side_center_right:
        return "SCR";
    case ci_side_front_left:
        return "SFL";
    case ci_side_front_right:
        return "SFR";
    case ci_side_back_left:
        return "SBL";
    case ci_side_back_right:
        return "SBR";
    case ci_back_center_left:
        return "BCL";
    case ci_back_center_right:
        return "BCR";
    default:
        return "UNKNOWN";
    }
}

const char *Real3DVSTAudioProcessor::setupToName(channel_setup setup)
{
    switch (setup)
    {
    case cs_stereo:
        return "Stereo";
    case cs_3stereo:
        return "3-Stereo";
    case cs_4point1:
        return "4.1 Surround";
    case cs_5point1:
        return "5.1 Surround";
    case cs_5stereo:
        return "5-Stereo";
    case cs_legacy:
        return "Legacy 5.1";
    case cs_6point1:
        return "6.1 Surround";
    case cs_7point1:
        return "7.1 Surround";
    case cs_7point1_panorama:
        return "7.1 Panorama";
    case cs_7point1_tricenter:
        return "7.1 Tri-Center";
    case cs_8point1:
        return "8.1 Surround";
    case cs_9point1_wrap:
        return "9.1 Wrap";
    case cs_9point1_densepanorama:
        return "9.1 Dense Panorama";
    case cs_11point1_densewrap:
        return "11.1 Dense Wrap";
    case cs_13point1_totalwrap:
        return "13.1 Total Wrap";
    case cs_16point1:
        return "16.1 Surround";
    default:
        return "Unknown Setup";
    }
}

juce::String Real3DVSTAudioProcessor::describeSetupChannels(channel_setup setup)
{
    juce::StringArray names;
    const int count = (int)freesurround_decoder::num_channels(setup);
    for (int i = 0; i < count; ++i)
        names.add(channelIdToName(freesurround_decoder::channel_at(setup, (unsigned)i)));
    return names.joinIntoString(", ");
}

//==============================================================================
const juce::String Real3DVSTAudioProcessor::getName() const { return "Real3D-VST"; }

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

double Real3DVSTAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int Real3DVSTAudioProcessor::getNumPrograms() { return 1; }
int Real3DVSTAudioProcessor::getCurrentProgram() { return 0; }
void Real3DVSTAudioProcessor::setCurrentProgram(int) {}
const juce::String Real3DVSTAudioProcessor::getProgramName(int) { return {}; }
void Real3DVSTAudioProcessor::changeProgramName(int, const juce::String &) {}

//==============================================================================
void Real3DVSTAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    decoder.reset(new freesurround_decoder(currentSetup, fftSize));
    decoder->set_sample_rate(currentSampleRate);
    updateParameters();

    const int minFifoSize = juce::jmax(fftSize * 4, samplesPerBlock * 4, fftSize + samplesPerBlock * 3);
    configuredFifoSize = juce::nextPowerOfTwo(juce::jmax(minFifoSize, fftSize * 4));

    inFifo.setSize(2, configuredFifoSize);
    outFifo.setSize(32, configuredFifoSize);

    resetFifos();

    processInputBuffer.assign(fftSize * 2, 0.0f);

    int latencyP = fftSize + (fftSize / 2);
    bypassBuffer.setSize(32, latencyP);
    bypassBuffer.clear();
    bypassWriteIdx = 0;
    wasBypassed = false;

    setLatencySamples(latencyP);
}

void Real3DVSTAudioProcessor::releaseResources()
{
    decoder.reset();
}

// Reset both FIFOs to the initial post-prepareToPlay state.
// Called when the channel setup changes or when switching between bypass and processing.
void Real3DVSTAudioProcessor::resetFifos()
{
    inFifo.clear();
    outFifo.clear();
    fifoReadIdx = 0;
    fifoWriteIdx = 0;
    inFifoFill = 0;
    outFifoReadIdx = 0;
    outFifoWriteIdx = fftSize; // pre-fill one block of silence for latency alignment
    outFifoFill = fftSize;
}

static int getWindowsRank(channel_id id)
{
    switch (id)
    {
    case ci_front_left:
        return 0;
    case ci_front_right:
        return 1;
    case ci_front_center:
        return 2;
    case ci_lfe:
        return 3;
    case ci_back_left:
        return 4;
    case ci_back_right:
        return 5;
    case ci_front_center_left:
        return 6;
    case ci_front_center_right:
        return 7;
    case ci_back_center:
        return 8;
    case ci_side_center_left:
        return 9;
    case ci_side_center_right:
        return 10;
    case ci_side_front_left:
        return 11;
    case ci_side_front_right:
        return 12;
    case ci_side_back_left:
        return 13;
    case ci_side_back_right:
        return 14;
    case ci_back_center_left:
        return 15;
    case ci_back_center_right:
        return 16;
    default:
        return 999;
    }
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool Real3DVSTAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
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

void Real3DVSTAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    updateParameters();

    // If the channel setup was just changed, discard stale FIFO data
    if (setupChanged)
    {
        resetFifos();
        setupChanged = false;
    }

    if (!decoder || totalNumInputChannels < 2)
    {
        buffer.clear();
        return;
    }

    // ---- detect whether the input is actually stereo ----
    bool isActuallyStereo = (totalNumInputChannels == 2);
    if (totalNumInputChannels > 2)
    {
        bool extraChannelsSilent = true;
        for (int ch = 2; ch < totalNumInputChannels; ++ch)
        {
            if (buffer.getMagnitude(ch, 0, numSamples) > 1e-6f)
            {
                extraChannelsSilent = false;
                break;
            }
        }
        isActuallyStereo = extraChannelsSilent;

        if (!isActuallyStereo)
        {
            ++nonStereoConsecutiveBlocks;
            if (nonStereoConsecutiveBlocks <= 2)
                isActuallyStereo = true;
        }
        else
        {
            nonStereoConsecutiveBlocks = 0;
        }
    }

    // ---- bypass path (non-stereo input) ----
    if (!isActuallyStereo)
    {
        // Transitioning from processing → bypass: discard stale FIFO data
        if (!wasBypassed)
        {
            resetFifos();
            wasBypassed = true;
        }

        int delayLen = bypassBuffer.getNumSamples();
        if (delayLen > 0)
        {
            int maxChans = juce::jmin(totalNumInputChannels, bypassBuffer.getNumChannels());
            for (int ch = 0; ch < maxChans; ++ch)
            {
                float *inOutData = buffer.getWritePointer(ch);
                float *delayData = bypassBuffer.getWritePointer(ch);
                int idx = bypassWriteIdx;

                for (int s = 0; s < numSamples; ++s)
                {
                    float inSample = inOutData[s];
                    inOutData[s] = delayData[idx];
                    delayData[idx] = inSample;
                    idx = (idx + 1) % delayLen;
                }
            }
            bypassWriteIdx = (bypassWriteIdx + numSamples) % delayLen;
        }

        for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
            buffer.clear(i, 0, numSamples);
        return;
    }

    // ---- processing path ----
    // Transitioning from bypass → processing: discard stale FIFO data
    if (wasBypassed)
    {
        resetFifos();
        wasBypassed = false;
    }

    auto inLayout = getBusesLayout().getMainInputChannelSet();
    int leftIdx = inLayout.getChannelIndexForType(juce::AudioChannelSet::left);
    int rightIdx = inLayout.getChannelIndexForType(juce::AudioChannelSet::right);
    if (leftIdx < 0)
        leftIdx = 0;
    if (rightIdx < 0)
        rightIdx = (totalNumInputChannels > 1) ? 1 : 0;

    const int fsNumChannels = decoder->num_channels(currentSetup);

    // 1. Write input samples into the input FIFO
    for (int s = 0; s < numSamples; ++s)
    {
        if (inFifoFill >= inFifo.getNumSamples())
        {
            fifoReadIdx = (fifoReadIdx + 1) % inFifo.getNumSamples();
            --inFifoFill;
        }
        inFifo.setSample(0, (fifoWriteIdx + s) % inFifo.getNumSamples(), buffer.getSample(leftIdx, s));
        inFifo.setSample(1, (fifoWriteIdx + s) % inFifo.getNumSamples(), buffer.getSample(rightIdx, s));
        ++inFifoFill;
    }
    fifoWriteIdx = (fifoWriteIdx + numSamples) % inFifo.getNumSamples();

    // 2. Decode as many full FFT frames as available
    while (inFifoFill >= fftSize)
    {
        for (int i = 0; i < fftSize; ++i)
        {
            int idx = (fifoReadIdx + i) % inFifo.getNumSamples();
            processInputBuffer[2 * i] = inFifo.getSample(0, idx);
            processInputBuffer[2 * i + 1] = inFifo.getSample(1, idx);
        }

        float *decoded = decoder->decode(processInputBuffer.data());
        int numOutChannels = decoder->num_channels(currentSetup);

        for (int s = 0; s < fftSize; ++s)
        {
            if (outFifoFill >= outFifo.getNumSamples())
            {
                outFifoReadIdx = (outFifoReadIdx + 1) % outFifo.getNumSamples();
                --outFifoFill;
            }
            for (int c = 0; c < numOutChannels; ++c)
                outFifo.setSample(c, (outFifoWriteIdx + s) % outFifo.getNumSamples(),
                                  decoded[s * numOutChannels + c]);
            ++outFifoFill;
        }

        fifoReadIdx = (fifoReadIdx + fftSize) % inFifo.getNumSamples();
        inFifoFill -= fftSize;
        outFifoWriteIdx = (outFifoWriteIdx + fftSize) % outFifo.getNumSamples();
    }

    // 3. Read output FIFO
    const int samplesInOutFifo = outFifoFill;

    if (samplesInOutFifo < numSamples)
    {
        // Not enough data yet (should only happen during startup or after a reset).
        // Silence the entire block to avoid reading uninitialised data.
        buffer.clear();
        return;
    }

    for (int c = 0; c < totalNumOutputChannels; ++c)
        buffer.clear(c, 0, numSamples);

    const bool useNativeFsOrder = (currentSetup == cs_16point1);
    std::vector<int> outputFsOrder;
    outputFsOrder.reserve((size_t)fsNumChannels);

    if (useNativeFsOrder)
    {
        for (int fsChan = 0; fsChan < fsNumChannels; ++fsChan)
            outputFsOrder.push_back(fsChan);
    }
    else
    {
        std::vector<std::pair<int, int>> rankAndIndex;
        for (int fsChan = 0; fsChan < fsNumChannels; ++fsChan)
        {
            channel_id cid = freesurround_decoder::channel_at(currentSetup, fsChan);
            rankAndIndex.push_back({getWindowsRank(cid), fsChan});
        }
        std::sort(rankAndIndex.begin(), rankAndIndex.end(),
                  [](const auto &a, const auto &b)
                  { return a.first < b.first; });
        for (const auto &it : rankAndIndex)
            outputFsOrder.push_back(it.second);
    }

    for (size_t portIndex = 0; portIndex < outputFsOrder.size(); ++portIndex)
    {
        int fsChan = outputFsOrder[portIndex];
        int mappedJuceChan = (int)portIndex;

        if (mappedJuceChan >= 0 && mappedJuceChan < totalNumOutputChannels)
        {
            for (int s = 0; s < numSamples; ++s)
                buffer.setSample(mappedJuceChan, s,
                                 outFifo.getSample(fsChan, (outFifoReadIdx + s) % outFifo.getNumSamples()));
        }
    }

    outFifoReadIdx = (outFifoReadIdx + numSamples) % outFifo.getNumSamples();
    outFifoFill -= numSamples;
}

//==============================================================================
bool Real3DVSTAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor *Real3DVSTAudioProcessor::createEditor()
{
    return new Real3DVSTAudioProcessorEditor(*this);
}

//==============================================================================
void Real3DVSTAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void Real3DVSTAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new Real3DVSTAudioProcessor();
}