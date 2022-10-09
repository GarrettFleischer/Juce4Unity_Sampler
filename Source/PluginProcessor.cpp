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
    Juce4UnityAudioProcessor(BusesProperties()
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
}

Juce4Unity_SamplerAudioProcessor::~Juce4Unity_SamplerAudioProcessor()
{
    instruments.clear();
    instrumentMap.clear();
    synth.clearSounds();
    synth.clearVoices();
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

void Juce4Unity_SamplerAudioProcessor::loadInstrument(const juce::File& sfzFile)
{
    auto l = juce::ScopedLock(instrumentLock);
    const auto sound = new sfzero::Sound(sfzFile);
    sound->loadRegions();
    sound->loadSamples(&audioFormatManager);

    instruments.add(sound);
    const auto& path = sfzFile.getFullPathName();
    instrumentMap.set(path, sound);
}

void Juce4Unity_SamplerAudioProcessor::unloadInstrument(const juce::String& path)
{
    auto l = juce::ScopedLock(instrumentLock);
    const auto soundToRemove = getInstrumentForPath(path);
    instruments.remove(instruments.indexOf(instrumentMap[path]));
    instrumentMap.remove(path);
    for (int i = 0; i < synth.getNumSounds(); ++i)
    {
        if(synth.getSound(i) == soundToRemove)
        {
            synth.removeSound(i);
            break;
        }
    }
}

void Juce4Unity_SamplerAudioProcessor::setInstrument(const juce::String& path)
{
    auto l = juce::ScopedLock(instrumentLock);
    synth.clearSounds();
    synth.addSound(getInstrumentForPath(path));
}

void Juce4Unity_SamplerAudioProcessor::clearInstruments()
{
    auto l = juce::ScopedLock(instrumentLock);
    synth.clearSounds();
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
}

// void Juce4Unity_SamplerAudioProcessor::setSampleRateForDevice(const juce::String& deviceName, const double sampleRate)
// {
//     auto setup = deviceManager.getAudioDeviceSetup();
//     setup.outputDeviceName = deviceName;
//     setup.sampleRate = sampleRate;
//     deviceManager.setAudioDeviceSetup(setup, true);
// }

// void Juce4Unity_SamplerAudioProcessor::getAvailableSampleRates()
// {
//     const auto currentDevice = deviceManager.getCurrentAudioDevice();
//     auto rates = currentDevice->getAvailableSampleRates();
//
//     auto message = juce::OSCMessage(OSCAvailableSampleRates);
//     message.addString(currentDevice->getTypeName());
//     message.addString(currentDevice->getName());
//
//     for (const auto rate : rates)
//     {
//         message.addString(juce::String(rate));
//     }
//     send(message);
// }

// void Juce4Unity_SamplerAudioProcessor::getAvailableDevices()
// {
//     auto message = juce::OSCMessage(OSCAvailableDevices);
//     for (const auto deviceType : deviceManager.getAvailableDeviceTypes())
//     {
//         message.addString(deviceType->getTypeName());
//         auto names = deviceType->getDeviceNames();
//         message.addInt32(names.size());
//         for (const auto& name : names)
//         {
//             message.addString(name);
//         }
//     }
//     send(message);
// }

const juce::SynthesiserSound::Ptr Juce4Unity_SamplerAudioProcessor::getInstrumentForPath(const juce::String& path) const
{
    auto l = juce::ScopedLock(instrumentLock);
    return instruments[instruments.indexOf(instrumentMap[path])];
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Juce4Unity_SamplerAudioProcessor();
}
