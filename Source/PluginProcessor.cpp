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

    if (OSCReceiver::connect(6423))
    {
        OSCReceiver::addListener(this, "/Juce4Unity/LoadInstrument");
        OSCReceiver::addListener(this, "/Juce4Unity/UnloadInstrument");
        OSCReceiver::addListener(this, "/Juce4Unity/SetInstrument");
        OSCReceiver::addListener(this, "/Juce4Unity/NoteOn");
        OSCReceiver::addListener(this, "/Juce4Unity/NoteOff");
        OSCReceiver::addListener(this, "/Juce4Unity/AllNotesOff");

        OSCSender::connect("localhost", 6448);
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

    synth.clearSounds();
    synth.addSound(sound);

    instruments.set(nextInstrumentId, sound);

    send("/Juce4Unity/InstrumentLoaded", nextInstrumentId);
    ++nextInstrumentId;
}

void Juce4Unity_SamplerAudioProcessor::unloadInstrument(int id)
{
    instruments.remove(id);
    synth.clearSounds();
}

void Juce4Unity_SamplerAudioProcessor::setInstrument(int id)
{
    synth.clearSounds();
    synth.addSound(instruments[id]);
}

void Juce4Unity_SamplerAudioProcessor::noteOn(int channel, int midi, float velocity)
{
    synth.noteOn(channel, midi, velocity);
}

void Juce4Unity_SamplerAudioProcessor::noteOff(int channel, int midi)
{
    synth.noteOff(channel, midi, 0.0f, false);
}

void Juce4Unity_SamplerAudioProcessor::allNotesOff(int channel)
{
    synth.allNotesOff(channel, false);
}

void Juce4Unity_SamplerAudioProcessor::oscMessageReceived(const juce::OSCMessage& message)
{
    const auto pattern = message.getAddressPattern();
    if (pattern == "/Juce4Unity/LoadInstrument")
    {
        const auto instrument = message[0].getString();
        loadInstrument(instrument);
    }
    else if (pattern == "/Juce4Unity/UnloadInstrument")
    {
        const auto id = message[0].getInt32();
        unloadInstrument(id);
    }
    else if (pattern == "/Juce4Unity/SetInstrument")
    {
        const auto id = message[0].getInt32();
        setInstrument(id);
    }
    else if (pattern == "/Juce4Unity/NoteOn")
    {
        const auto channel = message[0].getInt32();
        const auto midi = message[1].getInt32();
        const auto velocity = message[2].getFloat32();
        noteOn(channel, midi, velocity);
    }
    else if (pattern == "/Juce4Unity/NoteOff")
    {
        const auto channel = message[0].getInt32();
        const auto midi = message[1].getInt32();
        noteOff(channel, midi);
    }
    else if (pattern == "/Juce4Unity/AllNotesOff")
    {
        const auto channel = message[0].getInt32();
        allNotesOff(channel);
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Juce4Unity_SamplerAudioProcessor();
}
