/*
  ==============================================================================

    PluginWrapper.cpp
    Created: 8 May 2022 9:38:17pm
    Author:  StoneyDSP

  ==============================================================================
*/

#include "PluginWrapper.h"
#include "PluginProcessor.h"

template <typename SampleType>
ProcessWrapper<SampleType>::ProcessWrapper(BiLinearEQAudioProcessor& p, APVTS& apvts) : audioProcessor(p), state(apvts)
{

    spec.sampleRate = audioProcessor.getSampleRate();
    spec.maximumBlockSize = audioProcessor.getBlockSize();
    spec.numChannels = audioProcessor.getTotalNumInputChannels();

    auto osFilter = juce::dsp::Oversampling<SampleType>::filterHalfBandPolyphaseIIR;

    for (int i = 0; i < 5; ++i)
        oversampling[i] = std::make_unique<juce::dsp::Oversampling<SampleType>>
        (spec.numChannels, i, osFilter, true, false);

    osPtr = dynamic_cast <juce::AudioParameterChoice*> (state.getParameter("osID"));
    outputPtr = dynamic_cast <juce::AudioParameterFloat*> (state.getParameter("outputID"));
    mixPtr = dynamic_cast <juce::AudioParameterFloat*> (state.getParameter("mixID"));
    bypassPtr = dynamic_cast <juce::AudioParameterBool*> (state.getParameter("bypassID"));

    jassert(osPtr != nullptr);
    jassert(outputPtr != nullptr);
    jassert(mixPtr != nullptr);
    jassert(bypassPtr != nullptr);
}

template <typename SampleType>
void ProcessWrapper<SampleType>::prepare()
{
    oversamplingFactor = 1 << newOS;
    oldOS = newOS;

    spec.sampleRate = audioProcessor.getSampleRate() * oversamplingFactor;
    spec.maximumBlockSize = audioProcessor.getBlockSize();
    spec.numChannels = audioProcessor.getTotalNumInputChannels();

    for (int i = 0; i < 5; ++i)
        oversampling[i]->initProcessing(spec.maximumBlockSize);

    for (int i = 0; i < 5; ++i)
        oversampling[i]->numChannels = (size_t)spec.numChannels;

    mixer.prepare(spec);
    output.prepare(spec);

    reset();
    update();
}

template <typename SampleType>
void ProcessWrapper<SampleType>::reset()
{
    for (int i = 0; i < 5; ++i)
        oversampling[i]->numChannels = (size_t)spec.numChannels;

    for (int i = 0; i < 5; ++i)
        oversampling[i]->reset();

    mixer.reset();
    mixer.setWetLatency(getLatencySamples());
    output.reset();
}

//==============================================================================
template <typename SampleType>
void ProcessWrapper<SampleType>::process(juce::AudioBuffer<SampleType>& buffer, juce::MidiBuffer& midiMessages)
{
    midiMessages.clear();

    update();

    juce::dsp::AudioBlock<SampleType> block(buffer);
    juce::dsp::AudioBlock<SampleType> osBlock(buffer);

    mixer.pushDrySamples(block);

    osBlock = oversampling[newOS]->processSamplesUp(block);

    auto context = juce::dsp::ProcessContextReplacing(osBlock);

    context.isBypassed = bypassPtr->get();

    filters(context);

    output.process(context);

    oversampling[newOS]->processSamplesDown(block);

    mixer.mixWetSamples(block);
}

template <typename SampleType>
void ProcessWrapper<SampleType>::update()
{
    spec.sampleRate = audioProcessor.getSampleRate() * oversamplingFactor;
    spec.maximumBlockSize = audioProcessor.getBlockSize();
    spec.numChannels = audioProcessor.getTotalNumInputChannels();
    
    setOversampling();

    mixer.setWetMixProportion(mixPtr->get() * 0.01f);

    output.setGainLinear(juce::Decibels::decibelsToGain(outputPtr->get()));
}

template <typename SampleType>
void ProcessWrapper<SampleType>::setOversampling()
{
    newOS = (int)osPtr->getIndex();
    if (newOS != oldOS)
    {
        oversamplingFactor = 1 << newOS;
        oldOS = newOS;
        mixer.reset();
        mixer.setWetLatency(getLatencySamples());
        output.reset();
    }
}

template <typename SampleType>
SampleType ProcessWrapper<SampleType>::getLatencySamples() const noexcept
{
    // latency of oversampling
    return oversampling[newOS]->getLatencyInSamples();
}

//==============================================================================
template class ProcessWrapper<float>;
template class ProcessWrapper<double>;