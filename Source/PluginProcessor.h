#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "KickDSP.h"

using namespace juce;

class KickProcessor : public AudioProcessor
{
public:
    KickProcessor();
    ~KickProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const String getName() const override { return "KickShaper"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const String getProgramName (int) override { return {}; }
    void changeProgramName (int, const String&) override {}

    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    AudioProcessorValueTreeState apvts;
    std::atomic<bool> previewTrigger { false };   // TEST-nappi

private:
    static AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void trigger (int note, float velocity);

    struct ClickVoice { Kick::BQState bq; double ph = 0.0; };

    struct Voice
    {
        bool active = false, fading = false;
        double t = 0.0, phase = 0.0, subPhase = 0.0, pitchRatio = 1.0;
        float vel = 1.0f, fadeGain = 1.0f;
        uint32 rng = 12345u;
        ClickVoice clk[2];
    };

    struct Params
    {
        std::atomic<float>* length = nullptr;
        std::atomic<float>* pStart = nullptr;
        std::atomic<float>* pMid = nullptr;
        std::atomic<float>* pMidT = nullptr;
        std::atomic<float>* pEnd = nullptr;
        std::atomic<float>* aMidT = nullptr;
        std::atomic<float>* aMidL = nullptr;
        std::atomic<float>* aCurve = nullptr;
        std::atomic<float>* phase = nullptr;
        std::atomic<float>* harm = nullptr;
        std::atomic<float>* sub = nullptr;
        std::atomic<float>* dType = nullptr;
        std::atomic<float>* dDrive = nullptr;
        std::atomic<float>* dMix = nullptr;
        std::atomic<float>* c1Type = nullptr;
        std::atomic<float>* c1Level = nullptr;
        std::atomic<float>* c1Len = nullptr;
        std::atomic<float>* c1Pitch = nullptr;
        std::atomic<float>* c2Type = nullptr;
        std::atomic<float>* c2Level = nullptr;
        std::atomic<float>* c2Len = nullptr;
        std::atomic<float>* c2Pitch = nullptr;
        std::atomic<float>* eqLow = nullptr;
        std::atomic<float>* eqMid = nullptr;
        std::atomic<float>* eqHigh = nullptr;
        std::atomic<float>* cThresh = nullptr;
        std::atomic<float>* cRatio = nullptr;
        std::atomic<float>* cAtk = nullptr;
        std::atomic<float>* cRel = nullptr;
        std::atomic<float>* cMakeup = nullptr;
        std::atomic<float>* limiter = nullptr;
        std::atomic<float>* level = nullptr;
        std::atomic<float>* velSens = nullptr;
        std::atomic<float>* keyTrack = nullptr;
    } P;

    Voice voices[2];

    double sr = 44100.0;
    double trigPhase = 0.0;
    bool   trigKeyTrack = false;

    double adaaX1 = 0.0, dcX1 = 0.0, dcY1 = 0.0, dcR = 0.999;
    double grEnv = 0.0, limGain = 1.0;

    float lastEq[3] = { -999.0f, -999.0f, -999.0f };
    Kick::BQ eqBQ[3];
    Kick::BQState eqS[3];

    SmoothedValue<float> levelS;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickProcessor)
};
