/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"

//==============================================================================
Juce4Unity_SamplerAudioProcessor::Juce4Unity_SamplerAudioProcessor()
    :
#ifndef JucePlugin_PreferredChannelConfigurations
    AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    )
#endif
{
    logger = juce::FileLogger::createDefaultAppLogger(
        "Juce4UnitySampler",
        "Juce4UnitySampler.log",
        "");

    audioFormatManager.registerBasicFormats();

    for (int i = 0; i < 128; ++i)
    {
        synth.addVoice(new sfzero::Voice());
    }

    synth.clearSounds();
    synth.setNoteStealingEnabled(true);

    if (OSCReceiver::connect(6923))
    {
        logMessage("Receiver connected to port 6923");
        OSCReceiver::addListener(this, OSCNoteOn);
        OSCReceiver::addListener(this, OSCNoteOff);
        OSCReceiver::addListener(this, OSCAllNotesOff);
        OSCReceiver::addListener(this, OSCLoadInstrument);
        OSCReceiver::addListener(this, OSCUnloadInstrument);
        OSCReceiver::addListener(this, OSCSetInstrument);
        OSCReceiver::addListener(this, OSCClearInstruments);
        OSCReceiver::addListener(this, OSCReset);
        OSCReceiver::addListener(this, OSCSetGain);

        if (OSCSender::connect("127.0.0.1", 6942))
        {
            logMessage("Sender connected to port 6942");
        }
        else
        {
            logMessage("Sender failed to connect to port 6942");
        }
    }
    else
    {
        logMessage("Receiver failed to connect to port 6923");
    }
}

Juce4Unity_SamplerAudioProcessor::~Juce4Unity_SamplerAudioProcessor()
{
    OSCReceiver::disconnect();
    OSCSender::disconnect();
    instruments.clear();
    instrumentMap.clear();
    synth.clearSounds();
    synth.clearVoices();
    delete logger;
}

//==============================================================================
void Juce4Unity_SamplerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);
}

void Juce4Unity_SamplerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();
    const auto numSamples = buffer.getNumSamples();
    synth.renderNextBlock(buffer, midiMessages, 0, numSamples);
    buffer.applyGain(gain);
}

void Juce4Unity_SamplerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void Juce4Unity_SamplerAudioProcessor::loadInstrument(const juce::String& sfzFilePath)
{
    try
    {
        logMessage("Loading from file path: " + sfzFilePath);
        const auto sfzFile = juce::File(sfzFilePath);
        auto l = juce::ScopedLock(instrumentLock);
        const auto sound = new sfzero::Sound(sfzFile);
        sound->loadRegions();
        sound->loadSamples(&audioFormatManager);

        instruments.add(sound);
        const auto& path = sfzFile.getFullPathName();
        instrumentMap.set(path, sound);

        if (!send(OSCInstrumentLoaded, path))
        {
            logMessage("Failed to send loadInstrument message");
        }
    }
    catch (const std::exception& e)
    {
        logMessage(e.what());
        if (!send(OSCLoadInstrumentError, sfzFilePath, juce::String(e.what())))
        {
            logMessage("Failed to send error message");
        }
    }
    catch (const char* e)
    {
        logMessage(e);
        if (!send(OSCLoadInstrumentError, sfzFilePath, juce::String(e)))
        {
            logMessage("Failed to send error message");
        }
    }
}

void Juce4Unity_SamplerAudioProcessor::unloadInstrument(const juce::String& path)
{
    auto l = juce::ScopedLock(instrumentLock);
    instruments.remove(instruments.indexOf(instrumentMap[path]));
    instrumentMap.remove(path);
    synth.clearSounds();
    if (!send(OSCInstrumentUnloaded))
    {
        logMessage("Failed to send unloadInstrument message");
    }
}

void Juce4Unity_SamplerAudioProcessor::setInstrument(const juce::String& path)
{
    auto l = juce::ScopedLock(instrumentLock);
    synth.clearSounds();
    synth.addSound(getInstrumentForPath(path));
    if (!send(OSCInstrumentSet))
    {
        logMessage("Failed to send setInstrument message");
    }
}

void Juce4Unity_SamplerAudioProcessor::clearInstruments()
{
    auto l = juce::ScopedLock(instrumentLock);
    synth.clearSounds();
    if (!send(OSCInstrumentsCleared))
    {
        logMessage("Failed to send clearInstruments message");
    }
}

void Juce4Unity_SamplerAudioProcessor::noteOn(const int channel, const int midi, const float velocity)
{
    synth.noteOn(channel, midi, velocity);
}

void Juce4Unity_SamplerAudioProcessor::noteOff(const int channel, int midi)
{
    synth.noteOff(channel, midi, 0.0f, true);
}

void Juce4Unity_SamplerAudioProcessor::allNotesOff(int channel)
{
    synth.allNotesOff(channel, false);
}

void Juce4Unity_SamplerAudioProcessor::reset()
{
    auto l = juce::ScopedLock(instrumentLock);
    instruments.clear();
    instrumentMap.clear();
    synth.clearSounds();
    for (int i = 1; i <= 16; ++i)
    {
        synth.allNotesOff(i, false);
    }
    AudioProcessor::reset();
    if (!send(OSCResetComplete))
    {
        logMessage("Failed to send reset message");
    }
}

const juce::SynthesiserSound::Ptr Juce4Unity_SamplerAudioProcessor::getInstrumentForPath(const juce::String& path) const
{
    auto l = juce::ScopedLock(instrumentLock);
    return instruments[instruments.indexOf(instrumentMap[path])];
}

void Juce4Unity_SamplerAudioProcessor::oscMessageReceived(const juce::OSCMessage& message)
{
    try
    {
        const auto pattern = message.getAddressPattern();
        logMessage("Processing: " + pattern.toString());
        if (pattern.matches(OSCNoteOn))
        {
            const auto blob = message[0].getBlob();
            int channel;
            int midi;
            float velocity;
            blob.copyTo(&channel, 0, 4);
            blob.copyTo(&midi, 4, 4);
            blob.copyTo(&velocity, 8, 4);
            noteOn(channel, midi, velocity);
        }
        else if (pattern.matches(OSCNoteOff))
        {
            const auto blob = message[0].getBlob();
            int channel;
            int midi;
            blob.copyTo(&channel, 0, 4);
            blob.copyTo(&midi, 4, 4);
            noteOff(channel, midi);
        }
        else if (pattern.matches(OSCAllNotesOff))
        {
            const auto channel = message[0].getInt32();
            allNotesOff(channel);
        }
        else if (pattern.matches(OSCSetInstrument))
        {
            const auto id = message[0].getString();
            setInstrument(id);
        }
        else if (pattern.matches(OSCClearInstruments))
        {
            clearInstruments();
        }
        else if (pattern.matches(OSCLoadInstrument))
        {
            const auto instrument = message[0].getString();
            loadInstrument(instrument);
        }
        else if (pattern.matches(OSCUnloadInstrument))
        {
            const auto id = message[0].getString();
            unloadInstrument(id);
        }
        else if (pattern.matches(OSCReset))
        {
            reset();
        }
        else if (pattern.matches(OSCSetGain))
        {
            gain = message[0].getFloat32();
        }
    }
    catch (const std::exception& e)
    {
        logMessage(e.what());
    }
    catch (const char * e)
    {
        logMessage(e);
    }
}

void Juce4Unity_SamplerAudioProcessor::logMessage(const juce::String& message) const
{
    logger->logMessage(message);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Juce4Unity_SamplerAudioProcessor();
}
