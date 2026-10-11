#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include <array>

class RetroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RetroLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle,
                           juce::Slider&) override;
};

class ChannelStripEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit ChannelStripEditor (ChannelStripProcessor&);
    ~ChannelStripEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    static void drawNeedleMeter (juce::Graphics&, juce::Rectangle<float> area, float deflection,
                                 const juce::String& name, bool isGainReduction);
    static void drawLogo (juce::Graphics&, juce::Rectangle<float> area);
    static void styleButton (juce::TextButton&);

    static constexpr int numKnobs = 17;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment; // niszczony przed sliderem
    };

    ChannelStripProcessor& processor;
    RetroLookAndFeel laf;
    std::array<Knob, numKnobs> knobs;

    juce::TextButton eqButton { "EQ IN" }, compButton { "COMP IN" }, postButton { "EQ POST COMP" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> eqAttachment, compAttachment, postAttachment;

    float dispL = 0.0f, dispR = 0.0f, dispGr = 0.0f;
    float needleL = 0.0f, needleR = 0.0f, needleGr = 1.0f; // wychylenie wskazówek 0..1 (GR spoczywa po prawej)
    juce::Rectangle<int> headerArea, headInput, headEq, headComp, headOutput;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStripEditor)
};
