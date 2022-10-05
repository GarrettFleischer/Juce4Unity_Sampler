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
    manager.registerBasicFormats();

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
        OSCReceiver::addListener(this, OSCAddInstrument);
        OSCReceiver::addListener(this, OSCClearInstruments);
        OSCReceiver::addListener(this, OSCReset);
        OSCReceiver::addListener(this, OSCRequestActiveInstruments);

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
}

//==============================================================================
void Juce4Unity_SamplerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    synth.setCurrentPlaybackSampleRate(sampleRate);
}

void Juce4Unity_SamplerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void Juce4Unity_SamplerAudioProcessor::loadInstrument(juce::File sfzFile)
{
    const auto sound = new sfzero::Sound(sfzFile);
    sound->loadRegions();
    sound->loadSamples(&manager);

    instruments.add(sound);
    instrumentMap.set(nextInstrumentId, sound);

    send(OSCInstrumentLoaded, nextInstrumentId);
    ++nextInstrumentId;
}

void Juce4Unity_SamplerAudioProcessor::unloadInstrument(const int id)
{
    instruments.remove(instruments.indexOf(instrumentMap[id]));
    instrumentMap.remove(id);
    synth.clearSounds();
    activeInstruments.clear();
    send(OSCInstrumentUnloaded);
}

void Juce4Unity_SamplerAudioProcessor::setInstrument(const int id)
{
    synth.clearSounds();
    synth.addSound(getInstrumentForId(id));
    activeInstruments.clear();
    activeInstruments.add(id);
    send(OSCInstrumentSet);
}

void Juce4Unity_SamplerAudioProcessor::addInstrument(const int id)
{
    synth.addSound(getInstrumentForId(id));
    activeInstruments.add(id);
    send(OSCInstrumentAdded);
}

void Juce4Unity_SamplerAudioProcessor::clearInstruments()
{
    activeInstruments.clear();
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
    instruments.clear();
    instrumentMap.clear();
    activeInstruments.clear();
    synth.clearSounds();
    for (int i = 1; i <= 16; ++i)
    {
        synth.allNotesOff(i, false);
    }
    nextInstrumentId = 0;
    AudioProcessor::reset();
    send(OSCResetComplete);
}

void Juce4Unity_SamplerAudioProcessor::returnActiveInstruments()
{
    auto message = juce::OSCMessage(OSCReturnActiveInstruments);

    for (const int activeInstrument : activeInstruments)
    {
        message.addInt32(activeInstrument);
    }

    send(message);
}

const juce::SynthesiserSound::Ptr Juce4Unity_SamplerAudioProcessor::getInstrumentForId(int id) const
{
    return instruments[instruments.indexOf(instruments[id])];
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
        const auto id = message[0].getInt32();
        setInstrument(id);
    }
    else if (pattern.matches(OSCAddInstrument))
    {
        const auto id = message[0].getInt32();
        addInstrument(id);
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
        const auto id = message[0].getInt32();
        unloadInstrument(id);
    }
    else if (pattern.matches(OSCReset))
    {
        reset();
    }
    else if (pattern.matches(OSCRequestActiveInstruments))
    {
        returnActiveInstruments();
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Juce4Unity_SamplerAudioProcessor();
}
