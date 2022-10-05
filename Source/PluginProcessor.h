/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "SFZeroModule/SFZero.h"


//==============================================================================
/**
*/
class Juce4Unity_SamplerAudioProcessor :
    public juce::AudioProcessor,
    juce::OSCReceiver,
    juce::OSCReceiver::ListenerWithOSCAddress<juce::OSCReceiver::RealtimeCallback>,
    juce::OSCSender
#if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
#endif
{
public:
    //==============================================================================
    Juce4Unity_SamplerAudioProcessor();
    ~Juce4Unity_SamplerAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
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
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void loadInstrument(juce::File sfzFile);
    void unloadInstrument(const juce::String& path);
    void setInstrument(const juce::String& path);
    void clearInstruments();

    void noteOn(int channel, int midi, float velocity);
    void noteOff(int channel, int midi);

    void allNotesOff(int channel);

    void reset() override;

private:
    // receive
    const juce::OSCAddress OSCNoteOn{"/Juce4Unity/NoteOn"};
    const juce::OSCAddress OSCNoteOff{"/Juce4Unity/NoteOff"};
    const juce::OSCAddress OSCAllNotesOff{"/Juce4Unity/AllNotesOff"};
    const juce::OSCAddress OSCSetInstrument{"/Juce4Unity/SetInstrument"};
    const juce::OSCAddress OSCClearInstruments{"/Juce4Unity/ClearInstruments"};
    const juce::OSCAddress OSCLoadInstrument{"/Juce4Unity/LoadInstrument"};
    const juce::OSCAddress OSCUnloadInstrument{"/Juce4Unity/UnloadInstrument"};
    const juce::OSCAddress OSCReset{"/Juce4Unity/Reset"};

    // send
    const juce::OSCAddressPattern OSCInstrumentLoaded{"/Juce4Unity/InstrumentLoaded"};
    const juce::OSCAddressPattern OSCInstrumentUnloaded{"/Juce4Unity/InstrumentUnloaded"};
    const juce::OSCAddressPattern OSCInstrumentSet{"/Juce4Unity/InstrumentSet"};
    const juce::OSCAddressPattern OSCInstrumentsCleared{"/Juce4Unity/InstrumentsCleared"};
    const juce::OSCAddressPattern OSCResetComplete{"/Juce4Unity/ResetComplete"};

    juce::AudioFormatManager manager;
    sfzero::Synth synth;

    juce::CriticalSection instrumentLock;
    
    juce::ReferenceCountedArray<juce::SynthesiserSound> instruments;
    juce::HashMap<juce::String, const juce::SynthesiserSound*> instrumentMap;

    const juce::SynthesiserSound::Ptr getInstrumentForPath(const juce::String& path) const;

    void oscMessageReceived(const juce::OSCMessage& message) override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Juce4Unity_SamplerAudioProcessor)
};


//==============================================================================
inline const juce::String Juce4Unity_SamplerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

inline bool Juce4Unity_SamplerAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

inline bool Juce4Unity_SamplerAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

inline bool Juce4Unity_SamplerAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

inline double Juce4Unity_SamplerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

inline int Juce4Unity_SamplerAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

inline int Juce4Unity_SamplerAudioProcessor::getCurrentProgram()
{
    return 0;
}

inline void Juce4Unity_SamplerAudioProcessor::setCurrentProgram(int index)
{
}

inline const juce::String Juce4Unity_SamplerAudioProcessor::getProgramName(int index)
{
    return {};
}

inline void Juce4Unity_SamplerAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
inline bool Juce4Unity_SamplerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

inline void Juce4Unity_SamplerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();
    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
}

//==============================================================================
inline bool Juce4Unity_SamplerAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

inline juce::AudioProcessorEditor* Juce4Unity_SamplerAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
inline void Juce4Unity_SamplerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

inline void Juce4Unity_SamplerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}
