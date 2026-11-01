#pragma once
#include "PluginProcessor.h"
#include <array>
#include <vector>

// Interaktiivinen pitch/amp-kayra: raahaa pisteita hiirella
class EnvelopeGraph : public Component, private Timer
{
public:
    explicit EnvelopeGraph (KickProcessor& p) : proc (p) { startTimerHz (30); }

    void setMode (int m) { mode = m; repaint(); }

    void paint (Graphics&) override;
    void resized() override { lastKey[0] = -1.0; }
    void mouseDown (const MouseEvent&) override;
    void mouseDrag (const MouseEvent&) override;
    void mouseUp (const MouseEvent&) override;

private:
    void timerCallback() override;
    Kick::Shape currentShape() const;
    double paramValue (const char* id) const;
    void setParam (const char* id, double value);
    void updatePreview();

    Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (12.0f, 10.0f); }
    float  xOf (double frac) const;
    double fracOf (float x) const;
    float  yOfFreq (double hz) const;
    double freqOfY (float y) const;
    float  yOfAmp (double a) const;
    double ampOfY (float y) const;
    std::array<Point<float>, 3> nodePositions() const;

    KickProcessor& proc;
    int mode = 0;          // 0 = pitch, 1 = amp
    int dragNode = -1;
    std::vector<RangedAudioParameter*> dragParams;
    std::vector<float> colMin, colMax;
    std::array<double, 12> lastKey {};
};

class KickLookAndFeel : public LookAndFeel_V4
{
public:
    KickLookAndFeel();
    void drawRotarySlider (Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, Slider&) override;
    void drawToggleButton (Graphics&, ToggleButton&, bool highlighted, bool down) override;
};

class KickEditor : public AudioProcessorEditor
{
public:
    explicit KickEditor (KickProcessor&);
    ~KickEditor() override;

    void paint (Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        Slider slider;
        std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> attach;
        String name;
    };

    using ComboAttach  = AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttach = AudioProcessorValueTreeState::ButtonAttachment;

    void addKnob (Knob& k, const String& paramId, const String& name);
    void addChoice (ComboBox& cb, const String& paramId, std::unique_ptr<ComboAttach>& att);

    KickProcessor& proc;
    KickLookAndFeel laf;
    EnvelopeGraph graph;

    TextButton pitchTab, ampTab, testBtn;

    Knob length, phase, harm, sub;
    Knob dDrive, dMix;
    Knob eqLow, eqMid, eqHigh;
    Knob level, velocity;
    Knob cThresh, cRatio, cAtk, cRel, cMakeup;
    Knob c1Level, c1Len, c1Pitch, c2Level, c2Len, c2Pitch;

    ComboBox dType, c1Type, c2Type;
    std::unique_ptr<ComboAttach> dTypeAtt, c1TypeAtt, c2TypeAtt;

    ToggleButton limiterBtn { "LIMITER" }, keyBtn { "KEY TRACK" };
    std::unique_ptr<ButtonAttach> limiterAtt, keyAtt;

    std::vector<Knob*> allKnobs;

    const Rectangle<int> rBody   { 20, 310, 280, 120 };
    const Rectangle<int> rDist   { 308, 310, 170, 120 };
    const Rectangle<int> rEq     { 486, 310, 204, 120 };
    const Rectangle<int> rMaster { 698, 310, 162, 120 };
    const Rectangle<int> rComp   { 20, 438, 368, 120 };
    const Rectangle<int> rSw     { 396, 438, 164, 120 };
    const Rectangle<int> rClick1 { 600, 52, 260, 120 };
    const Rectangle<int> rClick2 { 600, 180, 260, 120 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickEditor)
};
