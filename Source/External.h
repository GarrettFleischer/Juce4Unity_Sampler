/*
  ==============================================================================

    External.h
    Created: 27 Sep 2022 4:35:45pm
    Author:  garre

  ==============================================================================
*/

#pragma once

#include "PluginProcessor.h"

#if JUCE_MSVC
#define UNITY_INTERFACE_API __stdcall
#define UNITY_INTERFACE_EXPORT __declspec(dllexport)
#else
 #define UNITY_INTERFACE_API
 #define UNITY_INTERFACE_EXPORT __attribute__ ((visibility("default")))
#endif

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SamplerLoadInstrument(const char* path, int pathLen);

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SamplerNoteOn(int channel, int midi, float velocity);
extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SamplerNoteOff(int channel, int midi);

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SamplerAllNotesOff(int channel);