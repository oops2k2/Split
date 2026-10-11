#include "PluginEditor.h"

namespace
{
    const juce::Colour panelTop    { 0xff2b2b2e };
    const juce::Colour panelBottom { 0xff151517 };
    const juce::Colour ledRed      { 0xffff3b2f };
    const juce::Colour accent      { 0xffff9f1c };
    const juce::Colour cream       { 0xffe8dfc8 };

    const char* ids[] = { "input", "hpf",
                          "lowgain", "lowfreq", "lmfgain", "lmffreq",
                          "hmfgain", "hmffreq", "highgain", "highfreq",
                          "thresh", "ratio", "attack", "release", "makeup", "mix",
                          "output" };

    const char* names[] = { "INPUT", "HPF",
                            "LOW GAIN", "LOW FREQ", "LMF GAIN", "LMF FREQ",
                            "HMF GAIN", "HMF FREQ", "HIGH GAIN", "HIGH FREQ",
                            "THRESH", "RATIO", "ATTACK", "RELEASE", "MAKEUP", "MIX",
                            "OUTPUT" };
}

//==============================================================================
RetroLookAndFeel::RetroLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, ledRed);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0a0a0a));
    setColour (juce::Label::textColourId, cream);
}

void RetroLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                         float pos, float startAngle, float endAngle,
                                         juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (12.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    g.setColour (cream.withAlpha (0.7f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        g.drawLine (juce::Line<float> (c.getPointOnCircumference (r + 3.0f, a),
                                       c.getPointOnCircumference (r + (i % 5 == 0 ? 9.0f : 6.0f), a)),
                    i % 5 == 0 ? 1.8f : 1.0f);
    }

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (c.x - r + 2.0f, c.y - r + 3.0f, r * 2.0f, r * 2.0f);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a6a6e), c.x - r * 0.5f, c.y - r * 0.6f,
                                             juce::Colour (0xff1e1e20), c.x + r * 0.6f, c.y + r * 0.8f, true));
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);

    g.setColour (juce::Colours::black);
    g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.5f);

    const float ri = r * 0.72f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3d), c.x, c.y - ri,
                                             juce::Colour (0xff222224), c.x, c.y + ri, false));
    g.fillEllipse (c.x - ri, c.y - ri, ri * 2.0f, ri * 2.0f);

    juce::Path p;
    p.addRoundedRectangle (-2.0f, -r * 0.92f, 4.0f, r * 0.5f, 1.5f);
    g.setColour (accent);
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

//==============================================================================
void ChannelStripEditor::styleButton (juce::TextButton& b)
{
    b.setClickingTogglesState (true);
    b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2a2d));
    b.setColour (juce::TextButton::buttonOnColourId, accent);
    b.setColour (juce::TextButton::textColourOffId, cream);
    b.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
}

ChannelStripEditor::ChannelStripEditor (ChannelStripProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&laf);
    setSize (1040, 560);

    for (int i = 0; i < numKnobs; ++i)
    {
        auto& k = knobs[(size_t) i];

        k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                      juce::MathConstants<float>::pi * 2.75f, true);
        addAndMakeVisible (k.slider);

        k.label.setText (names[i], juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setFont (juce::Font (juce::FontOptions ("Consolas", 14.0f, juce::Font::bold)));
        addAndMakeVisible (k.label);

        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, ids[i], k.slider);
    }

    for (auto* b : { &eqButton, &compButton, &postButton })
    {
        styleButton (*b);
        addAndMakeVisible (*b);
    }
    eqAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "eqon", eqButton);
    compAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "compon", compButton);
    postAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "eqpost", postButton);

    startTimerHz (30);
}

ChannelStripEditor::~ChannelStripEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ChannelStripEditor::timerCallback()
{
    dispL  = juce::jmax (processor.levelL.exchange (0.0f), dispL * 0.85f);
    dispR  = juce::jmax (processor.levelR.exchange (0.0f), dispR * 0.85f);
    dispGr = juce::jmax (processor.grDb.exchange (0.0f),   dispGr * 0.9f);

    // balistyka wskazówek: szybkie wychylenie, wolniejszy powrót
    auto follow = [] (float& needle, float target)
    {
        needle += (target - needle) * (target > needle ? 0.4f : 0.12f);
    };

    const float tL  = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (dispL, -60.0f) + 30.0f) / 33.0f);
    const float tR  = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (dispR, -60.0f) + 30.0f) / 33.0f);
    const float tGr = 1.0f - juce::jlimit (0.0f, 1.0f, dispGr / 20.0f);

    follow (needleL, tL);
    follow (needleR, tR);
    follow (needleGr, tGr);

    repaint (headerArea);
}

void ChannelStripEditor::drawNeedleMeter (juce::Graphics& g, juce::Rectangle<float> area, float f,
                                          const juce::String& name, bool isGr)
{
    const juce::Colour ink { 0xff2e2618 };
    const juce::Colour redZone { 0xffc22b1f };

    // obudowa i tarcza w stylu starych mierników VU
    g.setColour (juce::Colour (0xff0b0b0c));
    g.fillRoundedRectangle (area.expanded (4.0f), 6.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0e4b8), area.getX(), area.getY(),
                                             juce::Colour (0xffd8c88c), area.getX(), area.getBottom(), false));
    g.fillRect (area);

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (area.toNearestInt());

    // oś wskazówki leży tuż pod tarczą
    const float pivotX = area.getCentreX();
    const float pivotY = area.getBottom() + 10.0f;
    const float maxAngle = juce::degreesToRadians (45.0f);
    auto angleFor = [maxAngle] (float t) { return (t - 0.5f) * 2.0f * maxAngle; };
    auto pointAt = [&] (float radius, float t)
    {
        const float a = angleFor (t);
        return juce::Point<float> (pivotX + radius * std::sin (a), pivotY - radius * std::cos (a));
    };

    // skala: poziom -30..+3 dB albo redukcja 20..0 dB
    struct Tick { float pos; const char* text; };
    static const Tick levelTicks[] = { { 0.0f, "-30" }, { 0.303f, "-20" }, { 0.606f, "-10" },
                                       { 0.758f, "-5" }, { 0.909f, "0" }, { 1.0f, "+3" } };
    static const Tick grTicks[] = { { 1.0f, "0" }, { 0.85f, "3" }, { 0.7f, "6" },
                                    { 0.5f, "10" }, { 0.25f, "15" }, { 0.0f, "20" } };
    const Tick* ticks = isGr ? grTicks : levelTicks;

    const float rArc = 66.0f;
    const float redFrom = isGr ? 0.0f : 0.909f;
    const float redTo   = isGr ? 0.5f : 1.0f;

    juce::Path arc;
    arc.addCentredArc (pivotX, pivotY, rArc, rArc, 0.0f, angleFor (0.0f), angleFor (1.0f), true);
    g.setColour (ink);
    g.strokePath (arc, juce::PathStrokeType (1.5f));

    juce::Path red;
    red.addCentredArc (pivotX, pivotY, rArc, rArc, 0.0f, angleFor (redFrom), angleFor (redTo), true);
    g.setColour (redZone);
    g.strokePath (red, juce::PathStrokeType (3.5f));

    // drobna podziałka
    g.setColour (ink.withAlpha (0.55f));
    for (int i = 0; i <= 20; ++i)
        g.drawLine (juce::Line<float> (pointAt (rArc, (float) i / 20.0f), pointAt (rArc - 3.5f, (float) i / 20.0f)), 1.0f);

    // główna podziałka z opisami
    g.setFont (juce::Font (juce::FontOptions ("Consolas", 10.0f, juce::Font::bold)));
    for (int i = 0; i < 6; ++i)
    {
        const bool inRed = ticks[i].pos >= redFrom - 0.001f && ticks[i].pos <= redTo + 0.001f;
        g.setColour (inRed ? redZone : ink);
        g.drawLine (juce::Line<float> (pointAt (rArc + 2.0f, ticks[i].pos), pointAt (rArc - 8.0f, ticks[i].pos)), 1.6f);
        g.drawText (ticks[i].text,
                    juce::Rectangle<float> (0.0f, 0.0f, 28.0f, 12.0f).withCentre (pointAt (rArc - 18.0f, ticks[i].pos)),
                    juce::Justification::centred);
    }

    // podpisy
    g.setColour (ink.withAlpha (0.75f));
    g.setFont (juce::Font (juce::FontOptions ("Consolas", 10.0f, juce::Font::bold)));
    g.drawText (isGr ? "GAIN RED. dB" : "LEVEL dB", area.reduced (4.0f).withHeight (14.0f), juce::Justification::centred);
    g.setColour (ink);
    g.setFont (juce::Font (juce::FontOptions ("Consolas", 13.0f, juce::Font::bold)));
    g.drawText (name, area.reduced (7.0f, 5.0f), juce::Justification::bottomLeft);

    // wskazówka
    const auto tip = pointAt (area.getHeight() + 2.0f, f);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.drawLine (juce::Line<float> (juce::Point<float> (pivotX + 2.0f, pivotY + 2.0f), tip.translated (2.0f, 2.0f)), 2.0f);
    g.setColour (juce::Colour (0xff1a1010));
    g.drawLine (juce::Line<float> (juce::Point<float> (pivotX, pivotY), tip), 1.8f);

    // kopułka osi i odblask szkła
    g.setColour (juce::Colour (0xff1a1a1c));
    g.fillEllipse (pivotX - 14.0f, pivotY - 14.0f, 28.0f, 28.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.fillRect (area.withHeight (area.getHeight() * 0.45f));
}

void ChannelStripEditor::drawLogo (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3e), area.getX(), area.getY(),
                                             juce::Colour (0xff1b1b1d), area.getX(), area.getBottom(), false));
    g.fillRoundedRectangle (area, 4.0f);
    g.setColour (cream.withAlpha (0.5f));
    g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, 1.2f);

    const float d = area.getHeight() - 8.0f;
    const juce::Rectangle<float> badge (area.getX() + 6.0f, area.getY() + 4.0f, d, d);
    g.setColour (accent);
    g.fillEllipse (badge);
    g.setColour (juce::Colours::black);
    g.setFont (juce::Font (juce::FontOptions ("Impact", d * 0.62f, juce::Font::plain)));
    g.drawText ("DT", badge, juce::Justification::centred);

    g.setColour (cream);
    g.setFont (juce::Font (juce::FontOptions ("Impact", d * 0.8f, juce::Font::plain)));
    g.drawText ("DTSTUDIO", area.withTrimmedLeft (d + 14.0f), juce::Justification::centredLeft);
}

void ChannelStripEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (panelTop, 0.0f, 0.0f, panelBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();

    g.setColour (juce::Colours::white.withAlpha (0.025f));
    for (int yy = 0; yy < getHeight(); yy += 3)
        g.drawHorizontalLine (yy, 0.0f, (float) getWidth());

    g.setColour (juce::Colours::black);
    g.drawRect (getLocalBounds(), 3);
    g.setColour (cream.withAlpha (0.25f));
    g.drawRect (getLocalBounds().reduced (6), 1);

    for (auto pt : { juce::Point<float> (18, 18), { (float) getWidth() - 18, 18 },
                     { 18, (float) getHeight() - 18 }, { (float) getWidth() - 18, (float) getHeight() - 18 } })
    {
        g.setColour (juce::Colour (0xff555558));
        g.fillEllipse (pt.x - 6, pt.y - 6, 12, 12);
        g.setColour (juce::Colours::black);
        g.drawLine (pt.x - 4, pt.y + 3, pt.x + 4, pt.y - 3, 1.5f);
    }

    // tytuł
    g.setColour (accent);
    g.setFont (juce::Font (juce::FontOptions ("Impact", 38.0f, juce::Font::plain)));
    g.drawText ("CHANNEL STRIP", 36, 20, 360, 44, juce::Justification::centredLeft);
    g.setColour (cream.withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions ("Consolas", 13.0f, juce::Font::bold)));
    g.drawText ("INSERT  //  GAIN + EQ + COMPRESSOR  //  DTSTUDIO", 38, 62, 520, 18, juce::Justification::centredLeft);

    // mierniki wychyłowe: L, R i redukcja wzmocnienia (GR)
    const float mw = 120.0f, mh = 80.0f, mgap = 18.0f;
    const float mx = (float) getWidth() - 34.0f - (3.0f * mw + 2.0f * mgap);
    drawNeedleMeter (g, { mx, 14.0f, mw, mh }, needleL, "L", false);
    drawNeedleMeter (g, { mx + mw + mgap, 14.0f, mw, mh }, needleR, "R", false);
    drawNeedleMeter (g, { mx + 2.0f * (mw + mgap), 14.0f, mw, mh }, needleGr, "GR", true);

    // nagłówki sekcji
    g.setFont (juce::Font (juce::FontOptions ("Consolas", 13.0f, juce::Font::bold)));
    for (auto& pair : { std::make_pair (headInput, "INPUT"), std::make_pair (headEq, "EQUALIZER"),
                        std::make_pair (headComp, "COMPRESSOR"), std::make_pair (headOutput, "OUTPUT") })
    {
        g.setColour (accent);
        g.drawText (pair.second, pair.first, juce::Justification::centredLeft);
        g.setColour (cream.withAlpha (0.3f));
        g.drawHorizontalLine (pair.first.getBottom() - 2, (float) pair.first.getX() + 4.0f,
                              (float) pair.first.getRight() - 8.0f);
    }

    // stopka
    const float fy = (float) getHeight() - 56.0f;
    drawLogo (g, { 44.0f, fy, 190.0f, 30.0f });
    g.setColour (cream.withAlpha (0.55f));
    g.setFont (juce::Font (juce::FontOptions ("Consolas", 12.0f, juce::Font::bold)));
    g.drawText ("CHANNEL STRIP  //  MODEL CS-90", (int) ((float) getWidth() - 400.0f), (int) fy, 356, 30,
                juce::Justification::centredRight);
}

void ChannelStripEditor::resized()
{
    headerArea = { 0, 0, getWidth(), 100 };

    auto area = getLocalBounds().reduced (24).withTrimmedTop (100).withTrimmedBottom (46);
    const int cw = area.getWidth() / 9;
    auto heading = area.removeFromTop (22);
    const int rowH = area.getHeight() / 2;
    const int x0 = area.getX();
    const int y1 = area.getY();
    const int y2 = area.getY() + rowH;

    headInput  = { x0,            heading.getY(), cw,     22 };
    headEq     = { x0 + cw,       heading.getY(), cw * 4, 22 };
    headComp   = { x0 + cw * 5,   heading.getY(), cw * 3, 22 };
    headOutput = { x0 + cw * 8,   heading.getY(), cw,     22 };

    auto place = [&] (int knobIndex, int col, int row)
    {
        juce::Rectangle<int> cell (x0 + col * cw, row == 0 ? y1 : y2, cw, rowH);
        auto& k = knobs[(size_t) knobIndex];
        k.label.setBounds (cell.removeFromTop (20));
        k.slider.setBounds (cell);
    };

    // wejście
    place (0, 0, 0);   // INPUT
    place (1, 0, 1);   // HPF

    // equalizer: w kolumnie pasma gain u góry, freq na dole
    for (int b = 0; b < 4; ++b)
    {
        place (2 + 2 * b, 1 + b, 0);
        place (3 + 2 * b, 1 + b, 1);
    }

    // kompresor
    place (10, 5, 0);  // THRESH
    place (11, 6, 0);  // RATIO
    place (12, 7, 0);  // ATTACK
    place (13, 5, 1);  // RELEASE
    place (14, 6, 1);  // MAKEUP
    place (15, 7, 1);  // MIX

    // wyjście i przyciski
    place (16, 8, 0);  // OUTPUT

    const int bx = x0 + 8 * cw + 8;
    const int bw = cw - 16;
    const int bh = 30;
    int by = y2 + (rowH - (3 * bh + 2 * 10)) / 2;
    for (auto* b : { &eqButton, &compButton, &postButton })
    {
        b->setBounds (bx, by, bw, bh);
        by += bh + 10;
    }
}
