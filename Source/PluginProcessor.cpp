#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

juce::AudioProcessorValueTreeState::ParameterLayout ChannelStripProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    // wejście i filtr górnoprzepustowy
    l.add (std::make_unique<P> (juce::ParameterID { "input", 1 }, "Input (dB)", R (-24.0f, 24.0f, 0.1f), 0.0f));
    l.add (std::make_unique<P> (juce::ParameterID { "hpf", 1 },   "HPF (Hz)",   R (20.0f, 500.0f, 1.0f, 0.5f), 20.0f));

    // equalizer: low shelf, dwa pasma środkowe, high shelf
    l.add (std::make_unique<P> (juce::ParameterID { "lowgain", 1 },  "Low Gain (dB)",  R (-15.0f, 15.0f, 0.1f), 0.0f));
    l.add (std::make_unique<P> (juce::ParameterID { "lowfreq", 1 },  "Low Freq (Hz)",  R (30.0f, 300.0f, 1.0f, 0.6f), 100.0f));
    l.add (std::make_unique<P> (juce::ParameterID { "lmfgain", 1 },  "LMF Gain (dB)",  R (-15.0f, 15.0f, 0.1f), 0.0f));
    l.add (std::make_unique<P> (juce::ParameterID { "lmffreq", 1 },  "LMF Freq (Hz)",  R (100.0f, 2000.0f, 1.0f, 0.5f), 400.0f));
    l.add (std::make_unique<P> (juce::ParameterID { "hmfgain", 1 },  "HMF Gain (dB)",  R (-15.0f, 15.0f, 0.1f), 0.0f));
    l.add (std::make_unique<P> (juce::ParameterID { "hmffreq", 1 },  "HMF Freq (Hz)",  R (1000.0f, 8000.0f, 1.0f, 0.5f), 3000.0f));
    l.add (std::make_unique<P> (juce::ParameterID { "highgain", 1 }, "High Gain (dB)", R (-15.0f, 15.0f, 0.1f), 0.0f));
    l.add (std::make_unique<P> (juce::ParameterID { "highfreq", 1 }, "High Freq (Hz)", R (3000.0f, 16000.0f, 1.0f, 0.5f), 8000.0f));

    // kompresor
    l.add (std::make_unique<P> (juce::ParameterID { "thresh", 1 },  "Threshold (dB)", R (-60.0f, 0.0f, 0.1f), -18.0f));
    l.add (std::make_unique<P> (juce::ParameterID { "ratio", 1 },   "Ratio",          R (1.0f, 20.0f, 0.1f, 0.5f), 3.0f));
    l.add (std::make_unique<P> (juce::ParameterID { "attack", 1 },  "Attack (ms)",    R (0.1f, 100.0f, 0.1f, 0.5f), 10.0f));
    l.add (std::make_unique<P> (juce::ParameterID { "release", 1 }, "Release (ms)",   R (10.0f, 1000.0f, 1.0f, 0.5f), 150.0f));
    l.add (std::make_unique<P> (juce::ParameterID { "makeup", 1 },  "Makeup (dB)",    R (0.0f, 24.0f, 0.1f), 0.0f));
    l.add (std::make_unique<P> (juce::ParameterID { "mix", 1 },     "Mix",            R (0.0f, 1.0f, 0.01f), 1.0f));

    // wyjście
    l.add (std::make_unique<P> (juce::ParameterID { "output", 1 }, "Output (dB)", R (-24.0f, 12.0f, 0.1f), 0.0f));

    // przełączniki
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "eqon", 1 },   "EQ In",   true));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "compon", 1 }, "Comp In", true));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "eqpost", 1 }, "EQ Post Comp", false));
    return l;
}

ChannelStripProcessor::ChannelStripProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool ChannelStripProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

void ChannelStripProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;

    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    for (int c = 0; c < 2; ++c)
    {
        hpf[c].coefficients   = Coeffs::makeHighPass (sampleRate, 20.0f, 0.707f);
        lowS[c].coefficients  = Coeffs::makeLowShelf (sampleRate, 100.0f, 0.707f, 1.0f);
        lmf[c].coefficients   = Coeffs::makePeakFilter (sampleRate, 400.0f, 1.0f, 1.0f);
        hmf[c].coefficients   = Coeffs::makePeakFilter (sampleRate, 3000.0f, 1.0f, 1.0f);
        highS[c].coefficients = Coeffs::makeHighShelf (sampleRate, 8000.0f, 0.707f, 1.0f);

        hpf[c].reset(); lowS[c].reset(); lmf[c].reset(); hmf[c].reset(); highS[c].reset();
    }
    for (auto& v : lastEq) v = 1.0e9f; // wymusza przeliczenie filtrów w pierwszym bloku

    grEnv = 0.0f;
    grPeak = 0.0f;

    inSmooth.reset (sampleRate, 0.02);
    inSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("input")->load()));
    outSmooth.reset (sampleRate, 0.02);
    outSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load()));
}

void ChannelStripProcessor::updateFilters()
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const float cur[9] = { get ("hpf"),
                           get ("lowgain"),  get ("lowfreq"),
                           get ("lmfgain"),  get ("lmffreq"),
                           get ("hmfgain"),  get ("hmffreq"),
                           get ("highgain"), get ("highfreq") };

    bool changed = false;
    for (int i = 0; i < 9; ++i)
        if (cur[i] != lastEq[i]) { changed = true; break; }
    if (! changed) return;

    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    const float nyq = (float) (0.45 * sr);
    auto fr = [nyq] (float f) { return juce::jlimit (20.0f, nyq, f); };
    auto gn = [] (float db) { return juce::Decibels::decibelsToGain (db); };

    auto cHpf  = Coeffs::makeHighPass   (sr, fr (cur[0]), 0.707f);
    auto cLow  = Coeffs::makeLowShelf   (sr, fr (cur[2]), 0.707f, gn (cur[1]));
    auto cLmf  = Coeffs::makePeakFilter (sr, fr (cur[4]), 1.0f,   gn (cur[3]));
    auto cHmf  = Coeffs::makePeakFilter (sr, fr (cur[6]), 1.0f,   gn (cur[5]));
    auto cHigh = Coeffs::makeHighShelf  (sr, fr (cur[8]), 0.707f, gn (cur[7]));

    for (int c = 0; c < 2; ++c)
    {
        hpf[c].coefficients   = cHpf;
        lowS[c].coefficients  = cLow;
        lmf[c].coefficients   = cLmf;
        hmf[c].coefficients   = cHmf;
        highS[c].coefficients = cHigh;
    }
    for (int i = 0; i < 9; ++i) lastEq[i] = cur[i];
}

void ChannelStripProcessor::applyEq (juce::AudioBuffer<float>& buf, int chs)
{
    const int n = buf.getNumSamples();
    for (int c = 0; c < chs; ++c)
    {
        float* x = buf.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            float s = lowS[c].processSample (x[i]);
            s = lmf[c].processSample (s);
            s = hmf[c].processSample (s);
            x[i] = highS[c].processSample (s);
        }
    }
}

void ChannelStripProcessor::applyComp (juce::AudioBuffer<float>& buf, int chs)
{
    const int n = buf.getNumSamples();

    const float thresh = apvts.getRawParameterValue ("thresh")->load();
    const float ratio  = apvts.getRawParameterValue ("ratio")->load();
    const float atkMs  = apvts.getRawParameterValue ("attack")->load();
    const float relMs  = apvts.getRawParameterValue ("release")->load();
    const float makeup = apvts.getRawParameterValue ("makeup")->load();
    const float mix    = apvts.getRawParameterValue ("mix")->load();

    const float atkCoef = 1.0f - std::exp (-1.0f / (atkMs * 0.001f * (float) sr));
    const float relCoef = 1.0f - std::exp (-1.0f / (relMs * 0.001f * (float) sr));
    const float slope   = 1.0f - 1.0f / ratio;
    const float knee    = 6.0f; // miękkie kolano w dB

    for (int i = 0; i < n; ++i)
    {
        // detektor połączony dla obu kanałów (stereo link)
        float peak = 0.0f;
        for (int c = 0; c < chs; ++c)
            peak = juce::jmax (peak, std::abs (buf.getReadPointer (c)[i]));

        const float levelDb = 20.0f * std::log10 (juce::jmax (peak, 1.0e-6f));
        const float over = levelDb - thresh;

        float gr = 0.0f; // wielkość redukcji w dB (dodatnia)
        if (over > knee * 0.5f)
            gr = slope * over;
        else if (over > -knee * 0.5f)
        {
            const float t = over + knee * 0.5f;
            gr = slope * t * t / (2.0f * knee);
        }

        grEnv += (gr > grEnv ? atkCoef : relCoef) * (gr - grEnv);
        grPeak = juce::jmax (grPeak, grEnv);

        const float gain = juce::Decibels::decibelsToGain (makeup - grEnv);
        for (int c = 0; c < chs; ++c)
        {
            float* x = buf.getWritePointer (c);
            const float dry = x[i];
            x[i] = dry * (1.0f - mix) + dry * gain * mix;
        }
    }
}

void ChannelStripProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buf.getNumSamples();
    const int chs = juce::jmin (buf.getNumChannels(), 2);
    if (chs == 0 || n == 0) return;

    updateFilters();

    const bool eqOn   = apvts.getRawParameterValue ("eqon")->load()   > 0.5f;
    const bool compOn = apvts.getRawParameterValue ("compon")->load() > 0.5f;
    const bool eqPost = apvts.getRawParameterValue ("eqpost")->load() > 0.5f;

    inSmooth.setTargetValue  (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("input")->load()));
    outSmooth.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load()));

    // 1) wzmocnienie wejściowe + filtr górnoprzepustowy
    for (int i = 0; i < n; ++i)
    {
        const float g = inSmooth.getNextValue();
        for (int c = 0; c < chs; ++c)
        {
            float* x = buf.getWritePointer (c);
            x[i] = hpf[c].processSample (x[i] * g);
        }
    }

    // 2) EQ i kompresor (EQ przed albo za kompresorem)
    if (eqOn && ! eqPost) applyEq (buf, chs);

    if (compOn)
        applyComp (buf, chs);
    else
        grEnv = 0.0f;

    if (eqOn && eqPost) applyEq (buf, chs);

    // 3) wzmocnienie wyjściowe i pomiary
    float peak[2] = { 0.0f, 0.0f };
    for (int i = 0; i < n; ++i)
    {
        const float g = outSmooth.getNextValue();
        for (int c = 0; c < chs; ++c)
        {
            float* x = buf.getWritePointer (c);
            x[i] *= g;
            peak[c] = juce::jmax (peak[c], std::abs (x[i]));
        }
    }

    levelL.store (juce::jmax (levelL.load(), peak[0]));
    levelR.store (juce::jmax (levelR.load(), chs == 2 ? peak[1] : peak[0]));
    grDb.store   (juce::jmax (grDb.load(), grPeak));
    grPeak = 0.0f;
}

juce::AudioProcessorEditor* ChannelStripProcessor::createEditor()
{
    return new ChannelStripEditor (*this);
}

void ChannelStripProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void ChannelStripProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChannelStripProcessor();
}
