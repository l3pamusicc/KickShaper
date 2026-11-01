#include "PluginEditor.h"

static const Colour kBg     (0xff0e1115);
static const Colour kPanel  (0xff171b21);
static const Colour kPlotBg (0xff0b0d10);
static const Colour kAccent (0xff32d4e6);
static const Colour kNode   (0xffff8a3d);
static const Colour kText   (0xffe3e8ee);
static const Colour kDim    (0xff7d8794);
static const Colour kGrid   (0xff222831);

// ================================================================ LookAndFeel
KickLookAndFeel::KickLookAndFeel()
{
    setColour (TextButton::buttonColourId,    Colour (0xff232932));
    setColour (TextButton::buttonOnColourId,  kAccent);
    setColour (TextButton::textColourOffId,   kDim);
    setColour (TextButton::textColourOnId,    Colour (0xff06232a));
    setColour (ComboBox::backgroundColourId,  Colour (0xff232932));
    setColour (ComboBox::textColourId,        kText);
    setColour (ComboBox::outlineColourId,     Colour (0xff39424e));
    setColour (ComboBox::arrowColourId,       kAccent);
    setColour (PopupMenu::backgroundColourId, Colour (0xff232932));
    setColour (PopupMenu::textColourId,       kText);
}

void KickLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, Slider&)
{
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float d = jmin (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const float radius = d * 0.5f;
    const float arcW = jmax (3.0f, d * 0.07f);
    const float ar = radius - arcW * 0.5f;
    const float ang = startAngle + pos * (endAngle - startAngle);

    PathStrokeType st (arcW, PathStrokeType::curved, PathStrokeType::rounded);

    Path bg;
    bg.addCentredArc (c.x, c.y, ar, ar, 0.0f, startAngle, endAngle, true);
    g.setColour (Colour (0xff2a313b));
    g.strokePath (bg, st);

    Path val;
    val.addCentredArc (c.x, c.y, ar, ar, 0.0f, startAngle, ang, true);
    g.setColour (kAccent);
    g.strokePath (val, st);

    const float br = radius - arcW - 3.0f;
    g.setGradientFill (ColourGradient (Colour (0xff343c48), c.x, c.y - br, Colour (0xff151a20), c.x, c.y + br, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
    g.setColour (Colour (0xff414b59));
    g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.0f);

    Path p;
    p.addRoundedRectangle (-1.5f, -br + 3.0f, 3.0f, br * 0.4f, 1.5f);
    p.applyTransform (AffineTransform::rotation (ang).translated (c.x, c.y));
    g.setColour (kText);
    g.fillPath (p);
}

void KickLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();

    g.setColour (on ? kAccent : Colour (0xff232932));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? kAccent.brighter (0.3f) : Colour (0xff39424e));
    g.drawRoundedRectangle (r, 6.0f, 1.5f);

    g.setColour (on ? Colour (0xff06232a) : kDim);
    g.setFont (Font (FontOptions (13.0f, Font::bold)));
    g.drawText (b.getButtonText(), r.toNearestInt(), Justification::centred);
}

// ================================================================ EnvelopeGraph
double EnvelopeGraph::paramValue (const char* id) const
{
    return (double) proc.apvts.getRawParameterValue (id)->load();
}

void EnvelopeGraph::setParam (const char* id, double value)
{
    if (auto* p = proc.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) value));
}

Kick::Shape EnvelopeGraph::currentShape() const
{
    Kick::Shape s;
    s.pStart = paramValue ("pstart");  s.pMid   = paramValue ("pmid");
    s.pEnd   = paramValue ("pend");    s.pMidT  = paramValue ("pmidt");
    s.aMidT  = paramValue ("amidt");   s.aMidL  = paramValue ("amidl");
    s.aCurve = paramValue ("acurve");
    return s;
}

// Aika-akseli on "zoomattu" alkuun: x = sqrt(osuus kick-pituudesta)
float EnvelopeGraph::xOf (double frac) const
{
    const auto r = plotArea();
    return r.getX() + (float) std::sqrt (jlimit (0.0, 1.0, frac)) * r.getWidth();
}

double EnvelopeGraph::fracOf (float x) const
{
    const auto r = plotArea();
    const double u = jlimit (0.0, 1.0, (double) ((x - r.getX()) / r.getWidth()));
    return u * u;
}

float EnvelopeGraph::yOfFreq (double hz) const
{
    const auto r = plotArea();
    const double lo = std::log2 (20.0), hi = std::log2 (12000.0);
    const double t = (std::log2 (jmax (hz, 1.0)) - lo) / (hi - lo);
    return r.getBottom() - (float) t * r.getHeight();
}

double EnvelopeGraph::freqOfY (float y) const
{
    const auto r = plotArea();
    const double lo = std::log2 (20.0), hi = std::log2 (12000.0);
    const double t = jlimit (0.0, 1.0, (double) ((r.getBottom() - y) / r.getHeight()));
    return std::pow (2.0, lo + t * (hi - lo));
}

float EnvelopeGraph::yOfAmp (double a) const
{
    const auto r = plotArea();
    return r.getBottom() - (float) a * r.getHeight();
}

double EnvelopeGraph::ampOfY (float y) const
{
    const auto r = plotArea();
    return jlimit (0.0, 1.0, (double) ((r.getBottom() - y) / r.getHeight()));
}

std::array<Point<float>, 3> EnvelopeGraph::nodePositions() const
{
    const auto s = currentShape();
    if (mode == 0)
        return { Point<float> (xOf (0.0),     yOfFreq (s.pStart)),
                 Point<float> (xOf (s.pMidT), yOfFreq (s.pMid)),
                 Point<float> (xOf (1.0),     yOfFreq (s.pEnd)) };

    return { Point<float> (xOf (0.0),     yOfAmp (1.0)),
             Point<float> (xOf (s.aMidT), yOfAmp (s.aMidL)),
             Point<float> (xOf (1.0),     yOfAmp (0.0)) };
}

void EnvelopeGraph::updatePreview()
{
    const int w = jmax (2, (int) plotArea().getWidth());
    colMin.assign ((size_t) w, 0.0f);
    colMax.assign ((size_t) w, 0.0f);
    std::vector<char> hit ((size_t) w, 0);

    const auto s = currentShape();
    const double lenSec = paramValue ("length") * 0.001;
    const double harm = paramValue ("harm") * 0.01;
    const double sub  = paramValue ("sub") * 0.01;
    double ph    = paramValue ("phase") * (Kick::twoPi / 360.0);
    double subPh = ph;

    const double psr = 16000.0;
    const int N = jmax (16, (int) (lenSec * psr));

    for (int i = 0; i < N; ++i)
    {
        const double f = (double) i / (double) N;
        const double freq = jmin (s.freqAt (f), psr * 0.45);
        ph    += Kick::twoPi * freq / psr;        if (ph    >= Kick::twoPi) ph    -= Kick::twoPi;
        subPh += Kick::twoPi * freq * 0.5 / psr;  if (subPh >= Kick::twoPi) subPh -= Kick::twoPi;

        double v = std::sin (ph);
        v += harm * (0.35 * std::sin (3.0 * ph) + 0.2 * std::sin (2.0 * ph));
        v += sub * std::sin (subPh);
        v *= s.ampAt (f) * jmin (1.0, (double) i / (psr * 0.0008));
        v = jlimit (-1.0, 1.0, v / 1.4);

        const int x = jlimit (0, w - 1, (int) (std::sqrt (f) * (double) (w - 1)));
        const float fv = (float) v;
        if (hit[(size_t) x] == 0) { colMin[(size_t) x] = fv; colMax[(size_t) x] = fv; hit[(size_t) x] = 1; }
        else { colMin[(size_t) x] = jmin (colMin[(size_t) x], fv); colMax[(size_t) x] = jmax (colMax[(size_t) x], fv); }
    }

    for (int x = 1; x < w; ++x)
        if (hit[(size_t) x] == 0) { colMin[(size_t) x] = colMin[(size_t) x - 1]; colMax[(size_t) x] = colMax[(size_t) x - 1]; }
}

void EnvelopeGraph::timerCallback()
{
    const std::array<double, 12> key {
        paramValue ("length"), paramValue ("pstart"), paramValue ("pmid"), paramValue ("pmidt"),
        paramValue ("pend"),   paramValue ("amidt"),  paramValue ("amidl"), paramValue ("acurve"),
        paramValue ("phase"),  paramValue ("harm"),   paramValue ("sub"),  (double) mode };

    if (key != lastKey)
    {
        lastKey = key;
        updatePreview();
        repaint();
    }
}

void EnvelopeGraph::paint (Graphics& g)
{
    g.setColour (kPanel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);

    const auto r = plotArea();
    g.setColour (kPlotBg);
    g.fillRect (r);

    const auto shape = currentShape();
    const double lenMs = paramValue ("length");

    // aaltomuoto taustalle
    const int w = (int) colMin.size();
    if (w > 1)
    {
        const float cy = r.getCentreY(), hh = r.getHeight() * 0.5f;
        g.setColour (kAccent.withAlpha (0.35f));
        for (int x = 0; x < w; ++x)
        {
            const float px = r.getX() + (float) x * r.getWidth() / (float) (w - 1);
            g.drawLine (px, cy - colMax[(size_t) x] * hh, px, cy - colMin[(size_t) x] * hh + 1.0f, 1.0f);
        }
    }

    // ruudukko
    g.setFont (Font (FontOptions (10.0f)));
    for (double ms : { 10.0, 25.0, 50.0, 100.0, 250.0, 500.0, 1000.0, 2000.0 })
    {
        if (ms >= lenMs) continue;
        const float x = xOf (ms / lenMs);
        g.setColour (kGrid);
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        g.setColour (kDim);
        g.drawText (String ((int) ms) + "ms", (int) x + 2, (int) r.getBottom() - 13, 44, 12, Justification::left);
    }

    if (mode == 0)
    {
        for (double hz : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0 })
        {
            const float y = yOfFreq (hz);
            g.setColour (kGrid);
            g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
            g.setColour (kDim);
            g.drawText (hz >= 1000.0 ? String ((int) (hz / 1000.0)) + "k" : String ((int) hz),
                        (int) r.getX() + 3, (int) y - 12, 40, 12, Justification::left);
        }
    }
    else
    {
        for (double a : { 0.25, 0.5, 0.75 })
        {
            g.setColour (kGrid);
            g.drawHorizontalLine ((int) yOfAmp (a), r.getX(), r.getRight());
        }
    }

    // kayra
    Path curve;
    constexpr int N = 240;
    for (int i = 0; i <= N; ++i)
    {
        const double u = (double) i / (double) N;
        const double f = u * u;
        const float px = r.getX() + (float) u * r.getWidth();
        const float py = (mode == 0) ? yOfFreq (shape.freqAt (f)) : yOfAmp (shape.ampAt (f));
        if (i == 0) curve.startNewSubPath (px, py); else curve.lineTo (px, py);
    }
    g.setColour (kAccent);
    g.strokePath (curve, PathStrokeType (2.0f));

    // pisteet
    const auto nodes = nodePositions();
    for (int i = 0; i < 3; ++i)
    {
        const bool movable = (mode == 0) || (i == 1);
        const auto p = nodes[(size_t) i];
        const float rad = movable ? 6.5f : 3.5f;

        g.setColour (kBg);
        g.fillEllipse (p.x - rad, p.y - rad, rad * 2.0f, rad * 2.0f);
        g.setColour (movable ? kNode : kDim);
        g.drawEllipse (p.x - rad, p.y - rad, rad * 2.0f, rad * 2.0f, 2.0f);

        if (mode == 0)
        {
            const double hz = (i == 0) ? shape.pStart : (i == 1 ? shape.pMid : shape.pEnd);
            const int tx = jlimit ((int) r.getX(), (int) r.getRight() - 70, (int) p.x + 10);
            const int ty = jlimit ((int) r.getY(), (int) r.getBottom() - 14, (int) p.y - 18);
            g.setColour (kText);
            g.setFont (Font (FontOptions (11.0f)));
            g.drawText (String (roundToInt (hz)) + " Hz", tx, ty, 70, 14, Justification::left);
        }
    }
}

void EnvelopeGraph::mouseDown (const MouseEvent& e)
{
    dragNode = -1;
    dragParams.clear();

    const auto nodes = nodePositions();
    float best = 18.0f;
    for (int i = 0; i < 3; ++i)
    {
        if (mode == 1 && i != 1) continue;
        const float d = nodes[(size_t) i].getDistanceFrom (e.position);
        if (d < best) { best = d; dragNode = i; }
    }
    if (dragNode < 0) return;

    auto add = [this] (const char* id)
    {
        if (auto* p = proc.apvts.getParameter (id))
        {
            p->beginChangeGesture();
            dragParams.push_back (p);
        }
    };

    if (mode == 0)
    {
        if (dragNode == 0)      add ("pstart");
        else if (dragNode == 1) { add ("pmid"); add ("pmidt"); }
        else                    add ("pend");
    }
    else
    {
        add ("amidl");
        add ("amidt");
    }

    mouseDrag (e);
}

void EnvelopeGraph::mouseDrag (const MouseEvent& e)
{
    if (dragNode < 0) return;
    const auto pos = e.position;

    if (mode == 0)
    {
        if (dragNode == 0)      setParam ("pstart", freqOfY (pos.y));
        else if (dragNode == 1) { setParam ("pmid", freqOfY (pos.y)); setParam ("pmidt", fracOf (pos.x)); }
        else                    setParam ("pend", freqOfY (pos.y));
    }
    else
    {
        setParam ("amidl", ampOfY (pos.y));
        setParam ("amidt", fracOf (pos.x));
    }
}

void EnvelopeGraph::mouseUp (const MouseEvent&)
{
    for (auto* p : dragParams)
        p->endChangeGesture();

    dragParams.clear();
    dragNode = -1;
}

// ================================================================ Editor
KickEditor::KickEditor (KickProcessor& p)
    : AudioProcessorEditor (&p), proc (p), graph (p)
{
    setSize (880, 570);
    setLookAndFeel (&laf);

    addAndMakeVisible (graph);

    pitchTab.setButtonText ("PITCH");
    ampTab.setButtonText ("AMP");
    for (auto* b : { &pitchTab, &ampTab })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (201);
        addAndMakeVisible (*b);
    }
    pitchTab.setToggleState (true, dontSendNotification);
    pitchTab.onClick = [this] { graph.setMode (0); };
    ampTab.onClick   = [this] { graph.setMode (1); };

    testBtn.setButtonText ("TEST");
    testBtn.onClick = [this] { proc.previewTrigger = true; };
    addAndMakeVisible (testBtn);

    addKnob (length,  "length",  "LENGTH");
    addKnob (phase,   "phase",   "PHASE");
    addKnob (harm,    "harm",    "HARMONICS");
    addKnob (sub,     "sub",     "SUB");
    addKnob (dDrive,  "ddrive",  "DRIVE");
    addKnob (dMix,    "dmix",    "MIX");
    addKnob (eqLow,   "eqlow",   "LOW");
    addKnob (eqMid,   "eqmid",   "MID");
    addKnob (eqHigh,  "eqhigh",  "HIGH");
    addKnob (level,   "level",   "LEVEL");
    addKnob (velocity, "velsens", "VELOCITY");
    addKnob (cThresh, "cthresh", "THRESHOLD");
    addKnob (cRatio,  "cratio",  "RATIO");
    addKnob (cAtk,    "catk",    "ATTACK");
    addKnob (cRel,    "crel",    "RELEASE");
    addKnob (cMakeup, "cmakeup", "MAKEUP");
    addKnob (c1Level, "c1level", "LEVEL");
    addKnob (c1Len,   "c1len",   "LENGTH");
    addKnob (c1Pitch, "c1pitch", "PITCH");
    addKnob (c2Level, "c2level", "LEVEL");
    addKnob (c2Len,   "c2len",   "LENGTH");
    addKnob (c2Pitch, "c2pitch", "PITCH");

    addChoice (dType,  "dtype",  dTypeAtt);
    addChoice (c1Type, "c1type", c1TypeAtt);
    addChoice (c2Type, "c2type", c2TypeAtt);

    addAndMakeVisible (limiterBtn);
    addAndMakeVisible (keyBtn);
    limiterAtt = std::make_unique<ButtonAttach> (proc.apvts, "limiter", limiterBtn);
    keyAtt     = std::make_unique<ButtonAttach> (proc.apvts, "keytrack", keyBtn);
}

KickEditor::~KickEditor()
{
    setLookAndFeel (nullptr);
}

void KickEditor::addKnob (Knob& k, const String& id, const String& name)
{
    k.name = name;
    k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 66, 16);
    k.slider.setColour (Slider::textBoxTextColourId, kText);
    k.slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    k.slider.setRotaryParameters (degreesToRadians (225.0f), degreesToRadians (495.0f), true);
    addAndMakeVisible (k.slider);
    k.attach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
    allKnobs.push_back (&k);
}

void KickEditor::addChoice (ComboBox& cb, const String& id, std::unique_ptr<ComboAttach>& att)
{
    if (auto* cp = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (id)))
        cb.addItemList (cp->choices, 1);

    cb.setJustificationType (Justification::centred);
    addAndMakeVisible (cb);
    att = std::make_unique<ComboAttach> (proc.apvts, id, cb);
}

static void drawPanel (Graphics& g, Rectangle<int> r, const String& title)
{
    g.setColour (kPanel);
    g.fillRoundedRectangle (r.toFloat(), 8.0f);
    g.setColour (Colour (0xff252b34));
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);

    g.setColour (kAccent);
    g.setFont (Font (FontOptions (11.0f, Font::bold)));
    g.drawText (title, r.getX() + 10, r.getY() + 4, 100, 16, Justification::left);
}

void KickEditor::paint (Graphics& g)
{
    g.fillAll (kBg);

    g.setColour (kText);
    g.setFont (Font (FontOptions (26.0f, Font::bold)));
    g.drawText ("KICKSHAPER", 20, 8, 260, 32, Justification::left);
    g.setColour (kNode);
    g.fillRect (22, 42, 40, 3);
    g.setColour (kDim);
    g.setFont (Font (FontOptions (11.0f)));
    g.drawText ("kick synthesizer", 70, 36, 160, 14, Justification::left);

    drawPanel (g, rBody,   "BODY");
    drawPanel (g, rDist,   "DISTORTION");
    drawPanel (g, rEq,     "EQ");
    drawPanel (g, rMaster, "OUTPUT");
    drawPanel (g, rComp,   "COMPRESSOR");
    drawPanel (g, rSw,     "SWITCHES");
    drawPanel (g, rClick1, "CLICK 1");
    drawPanel (g, rClick2, "CLICK 2");

    g.setColour (kDim);
    g.setFont (Font (FontOptions (11.0f, Font::bold)));
    for (auto* k : allKnobs)
        g.drawText (k->name, k->slider.getX() - 4, k->slider.getY() - 13, k->slider.getWidth() + 8, 13,
                    Justification::centred);

    g.setColour (kDim);
    g.setFont (Font (FontOptions (12.0f)));
    g.drawFittedText ("Drag the orange points in the graph to shape the kick. "
                      "The time axis is zoomed near the start so the fast pitch drop is easy to edit. "
                      "Press TEST to hear the kick without MIDI.",
                      Rectangle<int> (572, 450, 288, 100), Justification::topLeft, 6);
}

void KickEditor::resized()
{
    testBtn.setBounds (780, 10, 80, 28);
    pitchTab.setBounds (20, 52, 70, 22);
    ampTab.setBounds (94, 52, 70, 22);
    graph.setBounds (20, 78, 560, 222);

    auto place = [] (Knob& k, int x, int y, int w) { k.slider.setBounds (x, y, w, 80); };

    // rivi 1
    place (length, 24, 348, 66);   place (phase, 90, 348, 66);
    place (harm, 156, 348, 66);    place (sub, 222, 348, 66);

    dType.setBounds (rDist.getX() + 88, rDist.getY() + 3, 76, 20);
    place (dDrive, 316, 348, 70);  place (dMix, 388, 348, 70);

    place (eqLow, 488, 348, 66);   place (eqMid, 554, 348, 66);   place (eqHigh, 620, 348, 66);
    place (level, 704, 348, 66);   place (velocity, 774, 348, 66);

    // rivi 2
    place (cThresh, 26, 476, 70);  place (cRatio, 96, 476, 70);   place (cAtk, 166, 476, 70);
    place (cRel, 236, 476, 70);    place (cMakeup, 306, 476, 70);

    limiterBtn.setBounds (404, 474, 148, 28);
    keyBtn.setBounds (404, 512, 148, 28);

    // click-paneelit
    c1Type.setBounds (rClick1.getX() + 90, rClick1.getY() + 4, 110, 20);
    place (c1Level, rClick1.getX() + 4,   rClick1.getY() + 38, 82);
    place (c1Len,   rClick1.getX() + 86,  rClick1.getY() + 38, 82);
    place (c1Pitch, rClick1.getX() + 168, rClick1.getY() + 38, 82);

    c2Type.setBounds (rClick2.getX() + 90, rClick2.getY() + 4, 110, 20);
    place (c2Level, rClick2.getX() + 4,   rClick2.getY() + 38, 82);
    place (c2Len,   rClick2.getX() + 86,  rClick2.getY() + 38, 82);
    place (c2Pitch, rClick2.getX() + 168, rClick2.getY() + 38, 82);
}
