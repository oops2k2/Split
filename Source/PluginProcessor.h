#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

class ChannelStripProcessor : public juce::AudioProcessor
{
public:
    ChannelStripProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ChannelStrip"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // dane do mierników w edytorze (szczyt od ostatniego odczytu)
    std::atomic<float> levelL { 0.0f }, levelR { 0.0f }, grDb { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void updateFilters();
    void applyEq (juce::AudioBuffer<float>&, int chs);
    void applyComp (juce::AudioBuffer<float>&, int chs);

    using Filter = juce::dsp::IIR::Filter<float>;

    double sr = 44100.0;

    Filter hpf[2], lowS[2], lmf[2], hmf[2], highS[2];
    float lastEq[9] {};

    float grEnv = 0.0f;
    float grPeak = 0.0f;

    juce::SmoothedValue<float> inSmooth, outSmooth;
};
