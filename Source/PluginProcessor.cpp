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
    deviceManager.initialiseWithDefaultDevices(2, 2);

    audioFormatManager.registerBasicFormats();

    for (int i = 0; i < 128; ++i)
    {
        synth.addVoice(new sfzero::Voice());
    }

    synth.clearSounds();
    synth.setNoteStealingEnabled(true);

    if (OSCReceiver::connect(6923))
    {
        OSCReceiver::addListener(this, OSCNoteOn);
        OSCReceiver::addListener(this, OSCNoteOff);
        OSCReceiver::addListener(this, OSCAllNotesOff);
        OSCReceiver::addListener(this, OSCLoadInstrument);
        OSCReceiver::addListener(this, OSCUnloadInstrument);
        OSCReceiver::addListener(this, OSCSetInstrument);
        OSCReceiver::addListener(this, OSCClearInstruments);
        OSCReceiver::addListener(this, OSCReset);
        OSCReceiver::addListener(this, OSCSetGain);
        OSCReceiver::addListener(this, OSCSetSampleRateForDevice);
        OSCReceiver::addListener(this, OSCGetAvailableSampleRates);
        OSCReceiver::addListener(this, OSCGetAvailableDevices);

        if (!OSCSender::connect("127.0.0.1", 6942))
        {
            OSCReceiver::disconnect();
            throw std::exception("Unable to connect to OSC sender");
        }
    }
    else
    {
        throw std::exception("Unable to connect to OSC listener");
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
}

//==============================================================================
void Juce4Unity_SamplerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);
    synth.setMinimumRenderingSubdivisionSize(samplesPerBlock);
}

void Juce4Unity_SamplerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();
    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
    buffer.applyGain(gain);
}

void Juce4Unity_SamplerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void Juce4Unity_SamplerAudioProcessor::loadInstrument(const juce::File& sfzFile)
{
    auto l = juce::ScopedLock(instrumentLock);
    const auto sound = new sfzero::Sound(sfzFile);
    sound->loadRegions();
    sound->loadSamples(&audioFormatManager);

    instruments.add(sound);
    const auto& path = sfzFile.getFullPathName();
    instrumentMap.set(path, sound);

    send(OSCInstrumentLoaded, path);
}

void Juce4Unity_SamplerAudioProcessor::unloadInstrument(const juce::String& path)
{
    auto l = juce::ScopedLock(instrumentLock);
    instruments.remove(instruments.indexOf(instrumentMap[path]));
    instrumentMap.remove(path);
    synth.clearSounds();
    send(OSCInstrumentUnloaded);
}

void Juce4Unity_SamplerAudioProcessor::setInstrument(const juce::String& path)
{
    auto l = juce::ScopedLock(instrumentLock);
    synth.clearSounds();
    synth.addSound(getInstrumentForPath(path));
    send(OSCInstrumentSet);
}

void Juce4Unity_SamplerAudioProcessor::clearInstruments()
{
    auto l = juce::ScopedLock(instrumentLock);
    synth.clearSounds();
    send(OSCInstrumentsCleared);
}

void Juce4Unity_SamplerAudioProcessor::noteOn(const int channel, const int midi, const float velocity)
{
    synth.noteOn(channel, midi, velocity);
}

void Juce4Unity_SamplerAudioProcessor::noteOff(const int channel, int midi)
{
    synth.noteOff(channel, midi, 0.0f, false);
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
    send(OSCResetComplete);
}

void Juce4Unity_SamplerAudioProcessor::setSampleRateForDevice(const juce::String& deviceName, const double sampleRate)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.outputDeviceName = deviceName;
    setup.sampleRate = sampleRate;
    deviceManager.setAudioDeviceSetup(setup, true);
}

void Juce4Unity_SamplerAudioProcessor::getAvailableSampleRates()
{
    const auto currentDevice = deviceManager.getCurrentAudioDevice();
    auto rates = currentDevice->getAvailableSampleRates();

    auto message = juce::OSCMessage(OSCAvailableSampleRates);
    message.addString(currentDevice->getTypeName());
    message.addString(currentDevice->getName());

    for (const auto rate : rates)
    {
        message.addString(juce::String(rate));
    }
    send(message);
}

void Juce4Unity_SamplerAudioProcessor::getAvailableDevices()
{
    auto message = juce::OSCMessage(OSCAvailableDevices);
    for (const auto deviceType : deviceManager.getAvailableDeviceTypes())
    {
        message.addString(deviceType->getTypeName());
        auto names = deviceType->getDeviceNames();
        message.addInt32(names.size());
        for (const auto& name : names)
        {
            message.addString(name);
        }
    }
    send(message);
}

const juce::SynthesiserSound::Ptr Juce4Unity_SamplerAudioProcessor::getInstrumentForPath(const juce::String& path) const
{
    auto l = juce::ScopedLock(instrumentLock);
    return instruments[instruments.indexOf(instrumentMap[path])];
}

void Juce4Unity_SamplerAudioProcessor::oscMessageReceived(const juce::OSCMessage& message)
{
    const auto pattern = message.getAddressPattern();
    if (pattern.matches(OSCNoteOn))
    {
        const auto channel = message[0].getInt32();
        const auto midi = message[1].getInt32();
        const auto velocity = message[2].getFloat32();
        noteOn(channel, midi, velocity);
    }
    else if (pattern.matches(OSCNoteOff))
    {
        const auto channel = message[0].getInt32();
        const auto midi = message[1].getInt32();
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
    else if (pattern.matches(OSCSetSampleRateForDevice))
    {
        setSampleRateForDevice(message[0].getString(), message[1].getString().getDoubleValue());
    }
    else if (pattern.matches(OSCGetAvailableSampleRates))
    {
        getAvailableSampleRates();
    }
    else if (pattern.matches(OSCGetAvailableDevices))
    {
        getAvailableDevices();
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Juce4Unity_SamplerAudioProcessor();
}
