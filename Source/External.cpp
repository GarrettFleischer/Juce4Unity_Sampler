/*
  ==============================================================================

    External.cpp
    Created: 27 Sep 2022 4:35:45pm
    Author:  garre

  ==============================================================================
*/

#include "External.h"

void SamplerLoadInstrument(const char* path, const int pathLen)
{
    for (const auto instance : Juce4Unity_SamplerAudioProcessor::instances)
    {
        instance->loadInstrument(juce::File(juce::String(path, pathLen)));
    }
}

void SamplerNoteOn(const int channel, const int midi, const float velocity)
{
    for (const auto instance : Juce4Unity_SamplerAudioProcessor::instances)
    {
        instance->noteOn(channel, midi, velocity);
    }
}

void SamplerNoteOff(const int channel, const int midi)
{
    for (const auto instance : Juce4Unity_SamplerAudioProcessor::instances)
    {
        instance->noteOff(channel, midi);
    }
}

void SamplerAllNotesOff(const int channel)
{
    for (const auto instance : Juce4Unity_SamplerAudioProcessor::instances)
    {
        instance->allNotesOff(channel);
    }
}
