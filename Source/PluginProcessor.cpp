#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    String fmtMs (float v)    { return v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : String (roundToInt (v)) + " ms"; }
    String fmtHz (float v)    { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; }
    String fmtPct (float v)   { return String (roundToInt (v)) + " %"; }
    String fmtFrac (float v)  { return String (roundToInt (v * 100.0f)) + " %"; }
    String fmtDb (float v)    { return String (v, 1) + " dB"; }
    String fmtDeg (float v)   { return String (roundToInt (v)) + " deg"; }
    String fmtRatio (float v) { return String (v, 1) + ":1"; }
    String fmtPlain (float v) { return String (v, 2); }

    std::unique_ptr<RangedAudioParameter> makeFloat (const String& id, const String& name,
                                                     float lo, float hi, float def, float skew,
                                                     String (*fmt) (float))
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name,
            NormalisableRange<float> (lo, hi, 0.001f, skew), def,
            AudioParameterFloatAttributes().withStringFromValueFunction (
                [fmt] (float v, int) { return fmt (v); }));
    }

    std::unique_ptr<RangedAudioParameter> makeChoice (const String& id, const String& name,
                                                      const StringArray& items, int def)
    {
        return std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def);
    }

    std::unique_ptr<RangedAudioParameter> makeBool (const String& id, const String& name, bool def)
    {
        return std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def);
    }

    struct ClickCfg
    {
        int type = 0;
        double level = 0.0, tau = 0.01, pitch = 4000.0;
        Kick::BQ bq;
    };
}

KickProcessor::KickProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    P.length  = get ("length");   P.pStart = get ("pstart");  P.pMid   = get ("pmid");
    P.pMidT   = get ("pmidt");    P.pEnd   = get ("pend");    P.aMidT  = get ("amidt");
    P.aMidL   = get ("amidl");    P.aCurve = get ("acurve");  P.phase  = get ("phase");
    P.harm    = get ("harm");     P.sub    = get ("sub");
    P.dType   = get ("dtype");    P.dDrive = get ("ddrive");  P.dMix   = get ("dmix");
    P.c1Type  = get ("c1type");   P.c1Level = get ("c1level"); P.c1Len = get ("c1len"); P.c1Pitch = get ("c1pitch");
    P.c2Type  = get ("c2type");   P.c2Level = get ("c2level"); P.c2Len = get ("c2len"); P.c2Pitch = get ("c2pitch");
    P.eqLow   = get ("eqlow");    P.eqMid  = get ("eqmid");   P.eqHigh = get ("eqhigh");
    P.cThresh = get ("cthresh");  P.cRatio = get ("cratio");  P.cAtk   = get ("catk");
    P.cRel    = get ("crel");     P.cMakeup = get ("cmakeup");
    P.limiter = get ("limiter");  P.level  = get ("level");   P.velSens = get ("velsens");
    P.keyTrack = get ("keytrack");
}

AudioProcessorValueTreeState::ParameterLayout KickProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const StringArray clickTypes { "Off", "Noise", "Tick", "Snap" };

    // Kick-aanen muoto
    p.push_back (makeFloat ("length", "Length",        100.0f, 3000.0f, 450.0f, 0.4f, fmtMs));
    p.push_back (makeFloat ("pstart", "Pitch Start",   30.0f, 12000.0f, 1800.0f, 0.3f, fmtHz));
    p.push_back (makeFloat ("pmid",   "Pitch Mid",     30.0f, 4000.0f, 180.0f, 0.3f, fmtHz));
    p.push_back (makeFloat ("pmidt",  "Pitch Mid Time", 0.005f, 0.8f, 0.03f, 0.4f, fmtFrac));
    p.push_back (makeFloat ("pend",   "Pitch End",     25.0f, 250.0f, 48.0f, 0.5f, fmtHz));
    p.push_back (makeFloat ("amidt",  "Amp Mid Time",  0.01f, 0.95f, 0.35f, 1.0f, fmtFrac));
    p.push_back (makeFloat ("amidl",  "Amp Mid Level", 0.05f, 1.0f, 0.55f, 1.0f, fmtFrac));
    p.push_back (makeFloat ("acurve", "Amp Curve",     0.4f, 4.0f, 1.6f, 1.0f, fmtPlain));
    p.push_back (makeFloat ("phase",  "Phase",         0.0f, 360.0f, 90.0f, 1.0f, fmtDeg));
    p.push_back (makeFloat ("harm",   "Harmonics",     0.0f, 100.0f, 0.0f, 1.0f, fmtPct));
    p.push_back (makeFloat ("sub",    "Sub",           0.0f, 100.0f, 0.0f, 1.0f, fmtPct));

    // Distortion
    p.push_back (makeChoice ("dtype", "Dist Type", StringArray { "Soft", "Tube", "Hard", "Fold" }, 0));
    p.push_back (makeFloat  ("ddrive", "Dist Drive", 0.0f, 100.0f, 20.0f, 1.0f, fmtPct));
    p.push_back (makeFloat  ("dmix",   "Dist Mix",   0.0f, 100.0f, 100.0f, 1.0f, fmtPct));

    // Click-kerrokset
    p.push_back (makeChoice ("c1type", "Click 1 Type", clickTypes, 1));
    p.push_back (makeFloat  ("c1level", "Click 1 Level", 0.0f, 100.0f, 25.0f, 1.0f, fmtPct));
    p.push_back (makeFloat  ("c1len",   "Click 1 Length", 1.0f, 80.0f, 10.0f, 0.5f, fmtMs));
    p.push_back (makeFloat  ("c1pitch", "Click 1 Pitch", 300.0f, 12000.0f, 4000.0f, 0.4f, fmtHz));
    p.push_back (makeChoice ("c2type", "Click 2 Type", clickTypes, 0));
    p.push_back (makeFloat  ("c2level", "Click 2 Level", 0.0f, 100.0f, 25.0f, 1.0f, fmtPct));
    p.push_back (makeFloat  ("c2len",   "Click 2 Length", 1.0f, 80.0f, 6.0f, 0.5f, fmtMs));
    p.push_back (makeFloat  ("c2pitch", "Click 2 Pitch", 300.0f, 12000.0f, 8000.0f, 0.4f, fmtHz));

    // EQ
    p.push_back (makeFloat ("eqlow",  "EQ Low",  -12.0f, 12.0f, 0.0f, 1.0f, fmtDb));
    p.push_back (makeFloat ("eqmid",  "EQ Mid",  -12.0f, 12.0f, 0.0f, 1.0f, fmtDb));
    p.push_back (makeFloat ("eqhigh", "EQ High", -12.0f, 12.0f, 0.0f, 1.0f, fmtDb));

    // Kompressori
    p.push_back (makeFloat ("cthresh", "Comp Threshold", -36.0f, 0.0f, -8.0f, 1.0f, fmtDb));
    p.push_back (makeFloat ("cratio",  "Comp Ratio",     1.0f, 20.0f, 2.5f, 0.5f, fmtRatio));
    p.push_back (makeFloat ("catk",    "Comp Attack",    0.1f, 50.0f, 10.0f, 0.4f, fmtMs));
    p.push_back (makeFloat ("crel",    "Comp Release",   20.0f, 500.0f, 120.0f, 0.5f, fmtMs));
    p.push_back (makeFloat ("cmakeup", "Comp Makeup",    0.0f, 12.0f, 0.0f, 1.0f, fmtDb));

    // Ulostulo
    p.push_back (makeBool  ("limiter",  "Limiter", true));
    p.push_back (makeFloat ("level",    "Level", -24.0f, 12.0f, 0.0f, 1.0f, fmtDb));
    p.push_back (makeFloat ("velsens",  "Velocity", 0.0f, 100.0f, 50.0f, 1.0f, fmtPct));
    p.push_back (makeBool  ("keytrack", "Key Track", false));

    return { p.begin(), p.end() };
}

void KickProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& v : voices) v = Voice();

    adaaX1 = 0.0; dcX1 = 0.0; dcY1 = 0.0; grEnv = 0.0; limGain = 1.0;
    for (auto& s : eqS) s = Kick::BQState();
    for (auto& e : lastEq) e = -999.0f;

    dcR = 1.0 - Kick::twoPi * 8.0 / sr;

    levelS.reset (sr, 0.02);
    levelS.setCurrentAndTargetValue (Decibels::decibelsToGain (P.level->load()));
}

bool KickProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

void KickProcessor::trigger (int note, float vel)
{
    Voice* target = nullptr;
    for (auto& v : voices)
        if (! v.active) { target = &v; break; }

    if (target == nullptr)
        target = (voices[0].fadeGain <= voices[1].fadeGain) ? &voices[0] : &voices[1];

    for (auto& v : voices)
        if (&v != target && v.active) v.fading = true;   // vanha aani vaimenee pehmeasti

    *target = Voice();
    target->active = true;
    target->vel = jlimit (0.0f, 1.0f, vel);
    target->phase = trigPhase;
    target->subPhase = trigPhase;
    target->pitchRatio = trigKeyTrack ? std::pow (2.0, (double) (note - 60) / 12.0) : 1.0;
    target->rng = 0x9E3779B9u ^ ((uint32) note * 2654435761u) ^ Time::getMillisecondCounter();
}

void KickProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    buffer.clear();

    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 1 || n <= 0) return;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    auto g = [] (std::atomic<float>* a) { return a->load(); };

    // ---- parametrit
    Kick::Shape shape;
    shape.pStart = g (P.pStart);  shape.pMid   = g (P.pMid);   shape.pEnd  = g (P.pEnd);
    shape.pMidT  = g (P.pMidT);   shape.aMidT  = g (P.aMidT);  shape.aMidL = g (P.aMidL);
    shape.aCurve = g (P.aCurve);

    const double lenSec = (double) g (P.length) * 0.001;
    trigPhase    = (double) g (P.phase) * (Kick::twoPi / 360.0);
    trigKeyTrack = g (P.keyTrack) > 0.5f;

    const double harm = (double) g (P.harm) * 0.01;
    const double sub  = (double) g (P.sub) * 0.01;

    const int    dType  = (int) g (P.dType);
    const double dDrive = (double) g (P.dDrive) * 0.01;
    const double dGain  = std::pow (10.0, dDrive * 30.0 / 20.0);
    const double dMix   = (double) g (P.dMix) * 0.01;
    const bool   distOn = dDrive > 0.001;

    ClickCfg cc[2];
    for (int c = 0; c < 2; ++c)
    {
        cc[c].type  = (int) g (c == 0 ? P.c1Type : P.c2Type);
        cc[c].level = (double) g (c == 0 ? P.c1Level : P.c2Level) * 0.01 * 0.8;
        cc[c].tau   = (double) g (c == 0 ? P.c1Len : P.c2Len) * 0.001 / 4.0;
        cc[c].pitch = (double) g (c == 0 ? P.c1Pitch : P.c2Pitch);
        const double fmax = sr * 0.45;
        const double fc = jmin (cc[c].pitch, fmax);
        cc[c].bq = (cc[c].type == 1) ? Kick::bandpass (sr, fc, 1.0)
                                     : Kick::highpass (sr, fc, 0.7071);
    }

    const float eL = g (P.eqLow), eM = g (P.eqMid), eH = g (P.eqHigh);
    if (eL != lastEq[0] || eM != lastEq[1] || eH != lastEq[2])
    {
        eqBQ[0] = Kick::lowShelf  (sr, 110.0, (double) eL);
        eqBQ[1] = Kick::peak      (sr, 550.0, 0.9, (double) eM);
        eqBQ[2] = Kick::highShelf (sr, 5000.0, (double) eH);
        lastEq[0] = eL; lastEq[1] = eM; lastEq[2] = eH;
    }

    const double cThresh = (double) g (P.cThresh);
    const double cRatio  = (double) g (P.cRatio);
    const double cMakeup = (double) g (P.cMakeup);
    const double atkCoef = std::exp (-1.0 / (sr * 0.001 * (double) g (P.cAtk)));
    const double relCoef = std::exp (-1.0 / (sr * 0.001 * (double) g (P.cRel)));

    const bool   limOn  = g (P.limiter) > 0.5f;
    const double limRel = std::exp (-1.0 / (sr * 0.08));
    const double limCeil = 0.98;

    const double velSens = (double) g (P.velSens) * 0.01;
    levelS.setTargetValue (Decibels::decibelsToGain (g (P.level)));

    // ---- renderointi
    auto renderRange = [&] (int a, int b)
    {
        for (int i = a; i < b; ++i)
        {
            double body = 0.0, clk = 0.0;

            for (auto& v : voices)
            {
                if (! v.active) continue;

                const double f = v.t / lenSec;
                if (f >= 1.0) { v.active = false; continue; }

                const double freq = jmin (shape.freqAt (f) * v.pitchRatio, sr * 0.45);
                v.phase    += Kick::twoPi * freq / sr;
                if (v.phase >= Kick::twoPi) v.phase -= Kick::twoPi;
                v.subPhase += Kick::twoPi * freq * 0.5 / sr;
                if (v.subPhase >= Kick::twoPi) v.subPhase -= Kick::twoPi;

                double s = std::sin (v.phase);
                if (harm > 0.0) s += harm * (0.35 * std::sin (3.0 * v.phase) + 0.2 * std::sin (2.0 * v.phase));
                if (sub  > 0.0) s += sub * std::sin (v.subPhase);

                const double amp = shape.ampAt (f) * jmin (1.0, v.t / 0.0008);
                v.t += 1.0 / sr;

                if (v.fading)
                {
                    v.fadeGain -= (float) (1.0 / (0.004 * sr));
                    if (v.fadeGain <= 0.0f) { v.active = false; continue; }
                }

                const double gain = (1.0 - velSens * (1.0 - (double) v.vel)) * (double) v.fadeGain;
                body += s * amp * gain;

                for (int c = 0; c < 2; ++c)
                {
                    const ClickCfg& k = cc[c];
                    if (k.type == 0 || k.level <= 0.0 || v.t > k.tau * 9.0) continue;

                    const double env = std::exp (-v.t / k.tau);
                    double x;
                    if (k.type == 2)
                    {
                        v.clk[c].ph += Kick::twoPi * jmin (k.pitch, sr * 0.45) / sr;
                        if (v.clk[c].ph >= Kick::twoPi) v.clk[c].ph -= Kick::twoPi;
                        x = std::sin (v.clk[c].ph);
                    }
                    else
                    {
                        v.rng = v.rng * 1664525u + 1013904223u;
                        const double nz = (double) (v.rng >> 8) / 8388608.0 - 1.0;
                        x = Kick::bqProcess (k.bq, v.clk[c].bq, nz);
                    }
                    clk += x * env * k.level * gain;
                }
            }

            // Distortion (ADAA)
            double sig = body;
            if (distOn)
            {
                const double x = body * dGain;
                const double d = x - adaaX1;
                double y;
                if (std::fabs (d) < 1.0e-5) y = Kick::shaper (0.5 * (x + adaaX1), dType);
                else                        y = (Kick::shaperAD (x, dType) - Kick::shaperAD (adaaX1, dType)) / d;
                adaaX1 = x;
                sig = body * (1.0 - dMix) + y * dMix;
            }

            // DC-esto
            const double dc = sig - dcX1 + dcR * dcY1;
            dcX1 = sig; dcY1 = dc;
            sig = dc + clk;

            // EQ
            for (int e = 0; e < 3; ++e)
                sig = Kick::bqProcess (eqBQ[e], eqS[e], sig);

            // Kompressori
            const double levelDb = 20.0 * std::log10 (std::max (std::fabs (sig), 1.0e-6));
            const double over = levelDb - cThresh;
            const double targetGR = over > 0.0 ? over * (1.0 - 1.0 / cRatio) : 0.0;
            const double coef = targetGR > grEnv ? atkCoef : relCoef;
            grEnv = coef * grEnv + (1.0 - coef) * targetGR;
            sig *= std::pow (10.0, (cMakeup - grEnv) / 20.0);

            // Level + limiter
            sig *= (double) levelS.getNextValue();

            if (limOn)
            {
                const double peak = std::fabs (sig);
                const double target = peak > limCeil ? limCeil / peak : 1.0;
                limGain = target < limGain ? target : limGain + (1.0 - limGain) * (1.0 - limRel);
                if (limGain > target) limGain = target;
                sig *= limGain;
            }

            L[i] = (float) sig;
            if (R != nullptr) R[i] = (float) sig;
        }
    };

    int pos = 0;

    if (previewTrigger.exchange (false))
        trigger (60, 1.0f);

    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        const int at = jlimit (0, n, metadata.samplePosition);

        if (at > pos) { renderRange (pos, at); pos = at; }

        if (msg.isNoteOn())
            trigger (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            for (auto& v : voices) if (v.active) v.fading = true;
    }

    if (pos < n) renderRange (pos, n);
}

AudioProcessorEditor* KickProcessor::createEditor() { return new KickEditor (*this); }

void KickProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void KickProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KickProcessor(); }
