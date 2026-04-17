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
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       // Configure default buses as 17 discrete channels so hosts that rely on
                       // the preferred bus layout (for example Equalizer APO) can instantiate
                       // the full 16.1 path instead of truncating to 7.1/stereo.
                        .withInput  ("Input",  juce::AudioChannelSet::discreteChannels(17), true)
                       #endif
                        .withOutput ("Output", juce::AudioChannelSet::discreteChannels(17), true)
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
    params.push_back (std::make_unique<juce::AudioParameterBool> (useLfeID, "Use LFE", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> (debugLogID, "Debug Log", false));

    juce::StringArray setupNames { "Stereo", "3-Stereo", "4.1 Surround", "5.1 Surround", "5-Stereo", "Legacy 5.1", "6.1 Surround", "7.1 Surround", "7.1 Panorama", "7.1 Tri-Center", "8.1 Surround", "9.1 Wrap", "9.1 Dense Panorama", "11.1 Dense Wrap", "13.1 Total Wrap", "16.1 Surround" };
      params.push_back (std::make_unique<juce::AudioParameterChoice> (channelSetupID, "Output Configuration", setupNames, 7));

    return { params.begin(), params.end() };
}

void Real3DVSTAudioProcessor::updateParameters()
{
    if (!decoder) return;

    debugLoggingEnabled.store (apvts.getRawParameterValue ("debug_log")->load() > 0.5f);

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

        logDebugMessage ("Channel setup changed to " + juce::String (setupToName (currentSetup))
            + ", decoder channels=" + juce::String ((int) decoder->num_channels (currentSetup))
            + ", setup channels=" + describeSetupChannels (currentSetup));
    }
}

Real3DVSTAudioProcessor::~Real3DVSTAudioProcessor()
{
    logDebugMessage ("Processor destroyed");
}

bool Real3DVSTAudioProcessor::isDebugLoggingActive() const
{
    return debugLoggingEnabled.load();
}

void Real3DVSTAudioProcessor::ensureDebugLogger()
{
    if (debugLogger != nullptr)
        return;

    auto targetDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Real3D-VST")
        .getChildFile ("Logs");

    if (!targetDir.exists())
        targetDir.createDirectory();

    debugLogger.reset (juce::FileLogger::createDateStampedLogger (
        targetDir.getFullPathName(),
        "Real3D-VST-Debug",
        ".log",
        "Real3D-VST debug logging started"));
}

void Real3DVSTAudioProcessor::logDebugMessage (const juce::String& msg)
{
    if (!isDebugLoggingActive())
        return;

    ensureDebugLogger();
    if (debugLogger != nullptr)
        debugLogger->logMessage (msg);
}

const char* Real3DVSTAudioProcessor::channelIdToName (channel_id id)
{
    switch (id) {
        case ci_front_left: return "FL";
        case ci_front_right: return "FR";
        case ci_front_center: return "FC";
        case ci_lfe: return "LFE";
        case ci_back_left: return "BL";
        case ci_back_right: return "BR";
        case ci_front_center_left: return "FCL";
        case ci_front_center_right: return "FCR";
        case ci_back_center: return "BC";
        case ci_side_center_left: return "SCL";
        case ci_side_center_right: return "SCR";
        case ci_side_front_left: return "SFL";
        case ci_side_front_right: return "SFR";
        case ci_side_back_left: return "SBL";
        case ci_side_back_right: return "SBR";
        case ci_back_center_left: return "BCL";
        case ci_back_center_right: return "BCR";
        default: return "UNKNOWN";
    }
}

const char* Real3DVSTAudioProcessor::setupToName (channel_setup setup)
{
    switch (setup) {
        case cs_stereo: return "Stereo";
        case cs_3stereo: return "3-Stereo";
        case cs_4point1: return "4.1 Surround";
        case cs_5point1: return "5.1 Surround";
        case cs_5stereo: return "5-Stereo";
        case cs_legacy: return "Legacy 5.1";
        case cs_6point1: return "6.1 Surround";
        case cs_7point1: return "7.1 Surround";
        case cs_7point1_panorama: return "7.1 Panorama";
        case cs_7point1_tricenter: return "7.1 Tri-Center";
        case cs_8point1: return "8.1 Surround";
        case cs_9point1_wrap: return "9.1 Wrap";
        case cs_9point1_densepanorama: return "9.1 Dense Panorama";
        case cs_11point1_densewrap: return "11.1 Dense Wrap";
        case cs_13point1_totalwrap: return "13.1 Total Wrap";
        case cs_16point1: return "16.1 Surround";
        default: return "Unknown Setup";
    }
}

juce::String Real3DVSTAudioProcessor::describeSetupChannels (channel_setup setup)
{
    juce::StringArray names;
    const int count = (int) freesurround_decoder::num_channels (setup);
    for (int i = 0; i < count; ++i) {
        names.add (channelIdToName (freesurround_decoder::channel_at (setup, (unsigned) i)));
    }
    return names.joinIntoString (", ");
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

    const int minFifoSize = juce::jmax (fftSize * 4, samplesPerBlock * 4, fftSize + samplesPerBlock * 3);
    configuredFifoSize = juce::nextPowerOfTwo (juce::jmax (minFifoSize, fftSize * 4));

    inFifo.setSize (2, configuredFifoSize);
    outFifo.setSize (32, configuredFifoSize);
    inFifo.clear();
    outFifo.clear();

    fifoReadIdx = 0;
    fifoWriteIdx = 0;
    inFifoFill = 0;
    outFifoReadIdx = 0;
    outFifoWriteIdx = fftSize; // Pre-fill with one block of silence for FIFO buffering
    outFifoFill = fftSize;
    processBlockCounter = 0;
    nonStereoConsecutiveBlocks = 0;
    lastLoggedHostOutChannels = -1;
    lastLoggedDecoderOutChannels = -1;
    lastLoggedSetup = cs_legacy;
    lastLoggedStereoDecision = true;

    processInputBuffer.assign (fftSize * 2, 0.0f);

    int latencyP = fftSize + (fftSize / 2);
    // 准备旁通(bypass)延迟线，最多支持16通道的延迟（预留足够通道）
    bypassBuffer.setSize (32, latencyP);
    bypassBuffer.clear();
    bypassWriteIdx = 0;

    // Report total latency to host: FIFO buffering latency (fftSize) + freesurround_decoder algorithmic latency (fftSize / 2)
    setLatencySamples (latencyP);

    logDebugMessage ("prepareToPlay sr=" + juce::String (sampleRate)
        + ", block=" + juce::String (samplesPerBlock)
        + ", setup=" + juce::String (setupToName (currentSetup))
        + ", decoderChannels=" + juce::String ((int) decoder->num_channels (currentSetup))
        + ", latency=" + juce::String (latencyP)
        + ", fifoSize=" + juce::String (configuredFifoSize)
        + ", outPrefill=" + juce::String (outFifoFill));
}

void Real3DVSTAudioProcessor::releaseResources()
{
    logDebugMessage ("releaseResources");
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
    ++processBlockCounter;

    updateParameters();

    if (!decoder || totalNumInputChannels < 2) {
        logDebugMessage ("processBlock#" + juce::String ((int) processBlockCounter)
            + " invalid decoder/input, in=" + juce::String ((int) totalNumInputChannels)
            + ", out=" + juce::String ((int) totalNumOutputChannels)
            + ", cleared=true");
        buffer.clear();
        return;
    }

    bool isActuallyStereo = (totalNumInputChannels == 2);
    float maxExtraMagnitude = 0.0f;
    int maxExtraChannel = -1;
    if (totalNumInputChannels > 2) {
        bool extraChannelsSilent = true;
        for (int ch = 2; ch < totalNumInputChannels; ++ch) {
            const float mag = buffer.getMagnitude (ch, 0, numSamples);
            if (mag > maxExtraMagnitude) {
                maxExtraMagnitude = mag;
                maxExtraChannel = ch;
            }
            if (mag > 1e-6f) {
                extraChannelsSilent = false;
            }
        }
        isActuallyStereo = extraChannelsSilent;

        if (!isActuallyStereo) {
            ++nonStereoConsecutiveBlocks;
            if (nonStereoConsecutiveBlocks <= 2) {
                logDebugMessage ("processBlock#" + juce::String ((int) processBlockCounter)
                    + " transient non-stereo ignored, consecutive=" + juce::String (nonStereoConsecutiveBlocks)
                    + ", maxExtraCh=" + juce::String (maxExtraChannel)
                    + ", maxExtraMag=" + juce::String (maxExtraMagnitude, 8));
                isActuallyStereo = true;
            }
        } else {
            nonStereoConsecutiveBlocks = 0;
        }
    }

    // Encountering an unhandleable number of channels (e.g., > 2) and they aren't silent, we skip processing (bypass)
    if (!isActuallyStereo) {
        if (isDebugLoggingActive()) {
            logDebugMessage ("processBlock#" + juce::String ((int) processBlockCounter)
                + " bypass non-stereo input, in=" + juce::String ((int) totalNumInputChannels)
                + ", out=" + juce::String ((int) totalNumOutputChannels)
                + ", samples=" + juce::String (numSamples)
                + ", consecutive=" + juce::String (nonStereoConsecutiveBlocks)
                + ", maxExtraCh=" + juce::String (maxExtraChannel)
                + ", maxExtraMag=" + juce::String (maxExtraMagnitude, 8));
        }

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

    const int fsNumChannels = decoder->num_channels (currentSetup);

    if (isDebugLoggingActive()) {
        const bool shouldLogTopology = (lastLoggedSetup != currentSetup)
            || (lastLoggedHostOutChannels != (int) totalNumOutputChannels)
            || (lastLoggedDecoderOutChannels != fsNumChannels)
            || (lastLoggedStereoDecision != isActuallyStereo)
            || (processBlockCounter <= 5)
            || (processBlockCounter % 512 == 0);

        if (shouldLogTopology) {
            lastLoggedSetup = currentSetup;
            lastLoggedHostOutChannels = (int) totalNumOutputChannels;
            lastLoggedDecoderOutChannels = fsNumChannels;
            lastLoggedStereoDecision = isActuallyStereo;

            logDebugMessage ("processBlock#" + juce::String ((int) processBlockCounter)
                + " io in=" + juce::String ((int) totalNumInputChannels)
                + ", out=" + juce::String ((int) totalNumOutputChannels)
                + ", samples=" + juce::String (numSamples)
                + ", setup=" + juce::String (setupToName (currentSetup))
                + ", decoderCh=" + juce::String (fsNumChannels)
                + ", lrIdx=(" + juce::String (leftIdx) + "," + juce::String (rightIdx) + ")"
                + ", isStereo=" + juce::String (isActuallyStereo ? "true" : "false")
                + ", setupOrder=[" + describeSetupChannels (currentSetup) + "]");
        }
    }

    int inDroppedSamples = 0;
    int outDroppedSamples = 0;

    // 1. 写输入 FIFO
    for (int s = 0; s < numSamples; ++s) {
        if (inFifoFill >= inFifo.getNumSamples()) {
            fifoReadIdx = (fifoReadIdx + 1) % inFifo.getNumSamples();
            --inFifoFill;
            ++inDroppedSamples;
        }

        inFifo.setSample (0, (fifoWriteIdx + s) % inFifo.getNumSamples(), buffer.getSample (leftIdx, s));
        inFifo.setSample (1, (fifoWriteIdx + s) % inFifo.getNumSamples(), buffer.getSample (rightIdx, s));
        ++inFifoFill;
    }
    fifoWriteIdx = (fifoWriteIdx + numSamples) % inFifo.getNumSamples();

    // 2. 处理
    int decodePasses = 0;
    while (inFifoFill >= fftSize) {
        for (int i = 0; i < fftSize; ++i) {
            int idx = (fifoReadIdx + i) % inFifo.getNumSamples();
            processInputBuffer[2 * i] = inFifo.getSample (0, idx);
            processInputBuffer[2 * i + 1] = inFifo.getSample (1, idx);
        }

        float* decoded = decoder->decode (processInputBuffer.data());
        int numOutChannels = decoder->num_channels (currentSetup);

        for (int s = 0; s < fftSize; ++s) {
            if (outFifoFill >= outFifo.getNumSamples()) {
                outFifoReadIdx = (outFifoReadIdx + 1) % outFifo.getNumSamples();
                --outFifoFill;
                ++outDroppedSamples;
            }

            for (int c = 0; c < numOutChannels; ++c) {
                outFifo.setSample (c, (outFifoWriteIdx + s) % outFifo.getNumSamples(), decoded[s * numOutChannels + c]);
            }

            ++outFifoFill;
        }

        fifoReadIdx = (fifoReadIdx + fftSize) % inFifo.getNumSamples();
        inFifoFill -= fftSize;
        outFifoWriteIdx = (outFifoWriteIdx + fftSize) % outFifo.getNumSamples();
        ++decodePasses;
    }

    // 3. 读输出 FIFO (考虑到延迟，FreeSurround 内部 decode 已经包含了 N/2 的固定延迟效果)
    // 我们只要保证有数据就读
    const int samplesInOutFifo = outFifoFill;

    if (isDebugLoggingActive() && (inDroppedSamples > 0 || outDroppedSamples > 0 || processBlockCounter <= 5 || processBlockCounter % 256 == 0)) {
        logDebugMessage ("processBlock#" + juce::String ((int) processBlockCounter)
            + " fifo inFill=" + juce::String (inFifoFill)
            + ", outFill=" + juce::String (outFifoFill)
            + ", decodePasses=" + juce::String (decodePasses)
            + ", inDropped=" + juce::String (inDroppedSamples)
            + ", outDropped=" + juce::String (outDroppedSamples)
            + ", needOut=" + juce::String (numSamples));
    }

    if (samplesInOutFifo < numSamples) {
        logDebugMessage ("processBlock#" + juce::String ((int) processBlockCounter)
            + " insufficient output fifo, have=" + juce::String (samplesInOutFifo)
            + ", need=" + juce::String (numSamples)
            + ", action=clear");
        buffer.clear();
    } else {
        for (int c = 0; c < totalNumOutputChannels; ++c) {
            buffer.clear (c, 0, numSamples);
        }

        const bool useNativeFsOrder = (currentSetup == cs_16point1);
        std::vector<int> outputFsOrder;
        outputFsOrder.reserve ((size_t) fsNumChannels);

        if (useNativeFsOrder) {
            for (int fsChan = 0; fsChan < fsNumChannels; ++fsChan)
                outputFsOrder.push_back (fsChan);
        } else {
            // Keep legacy WFE rank ordering for non-16.1 setups.
            std::vector<std::pair<int, int>> rankAndIndex; // pair of <WFE_rank, fsChan_index>
            for (int fsChan = 0; fsChan < fsNumChannels; ++fsChan) {
                channel_id cid = freesurround_decoder::channel_at (currentSetup, fsChan);
                int rank = getWindowsRank (cid);
                rankAndIndex.push_back ({rank, fsChan});
            }

            std::sort (rankAndIndex.begin(), rankAndIndex.end(), [](const auto& a, const auto& b) {
                return a.first < b.first;
            });

            for (const auto& it : rankAndIndex)
                outputFsOrder.push_back (it.second);
        }

        if (isDebugLoggingActive()) {
            juce::StringArray mappingLines;
            for (size_t portIndex = 0; portIndex < outputFsOrder.size(); ++portIndex) {
                const int fsChan = outputFsOrder[portIndex];
                const channel_id cid = freesurround_decoder::channel_at (currentSetup, (unsigned) fsChan);
                const bool clippedByHost = (int) portIndex >= (int) totalNumOutputChannels;
                mappingLines.add (
                    "out" + juce::String ((int) portIndex)
                    + "<=" + channelIdToName (cid)
                    + "(fs=" + juce::String (fsChan)
                    + ",mode=" + juce::String (useNativeFsOrder ? "native" : "rank")
                    + (clippedByHost ? ",CLIPPED" : "")
                    + ")");
            }

            if (processBlockCounter <= 5
                || processBlockCounter % 512 == 0
                || (int) totalNumOutputChannels < fsNumChannels) {
                logDebugMessage ("processBlock#" + juce::String ((int) processBlockCounter)
                    + " mapping hostOut=" + juce::String ((int) totalNumOutputChannels)
                    + ", decoderOut=" + juce::String (fsNumChannels)
                    + ", map=[" + mappingLines.joinIntoString ("; ") + "]");
            }
        }

        // Map back to output buffers depending on output order (0 to fsNumChannels-1)
        for (size_t portIndex = 0; portIndex < outputFsOrder.size(); ++portIndex) {
            int fsChan = outputFsOrder[portIndex];
            int mappedJuceChan = (int)portIndex;

            if (mappedJuceChan >= 0 && mappedJuceChan < totalNumOutputChannels) {
                for (int s = 0; s < numSamples; ++s) {
                    buffer.addSample (mappedJuceChan, s, outFifo.getSample (fsChan, (outFifoReadIdx + s) % outFifo.getNumSamples()));
                }
            }
        }

        outFifoReadIdx = (outFifoReadIdx + numSamples) % outFifo.getNumSamples();
        outFifoFill -= numSamples;
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
