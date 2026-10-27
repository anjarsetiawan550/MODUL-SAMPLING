#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include "Dsp.h"

//==============================================================================
// SampleCore: 2 kanal trigger -> sampler one-shot.
//   Input 1/2 (L/R)  -> HPF/LPF -> deteksi pukulan -> sample -> fader -> Output 1/2 (L/R)
//==============================================================================
class SampleCoreProcessor : public juce::AudioProcessor,
                            public juce::ChangeBroadcaster
{
public:
    static constexpr int fftOrder  = 11;
    static constexpr int fftSize   = 1 << fftOrder;
    static constexpr int numVoices = 8;

    struct SampleData
    {
        juce::AudioBuffer<float> audio;   // mono
        double rate = 44100.0;
        juce::String name;
        juce::File file;
    };

    SampleCoreProcessor();
    ~SampleCoreProcessor() override;

    //== AudioProcessor ========================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "LR Module Sampler"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //== Parameter =============================================================
    juce::AudioProcessorValueTreeState apvts;

    //== Sampling (ch = 1 atau 2) =============================================
    bool loadSample (int ch, const juce::File& file, bool copyToStore);
    void clearSample (int ch);
    juce::String getSampleName (int ch) const;

    //== Preset ================================================================
    juce::File getRootDir() const;
    juce::File getPresetDir() const { return getRootDir().getChildFile ("Presets"); }
    juce::File getSampleDir() const { return getRootDir().getChildFile ("Samples"); }
    juce::StringArray getPresetNames() const;
    juce::String getCurrentPresetName() const;
    bool savePreset (const juce::String& name);
    bool loadPreset (const juce::String& name);
    void resetEverything();          // hapus semua preset + sampling (seperti format factory)

    //== Untuk GUI =============================================================
    float getMeterLevel (int ch) const { return meter[ch - 1].load(); }   // linear, ada decay
    bool  isSignalOn() const { return sigHold.load() > 0; }
    bool  isClipping() const { return clipHold.load() > 0; }
    double getCurrentSampleRate() const { return sampleRate > 0.0 ? sampleRate : 44100.0; }
    void  copyFftInput (int ch, float* dest) const;   // dest: fftSize float, urut waktu

private:
    struct Voice
    {
        std::shared_ptr<const SampleData> data;
        const float* ptr = nullptr;
        int    len = 0;
        double pos = 0.0, inc = 1.0;
        float  gain = 1.f, lpCoef = 1.f, lp = 0.f;
        bool   active = false;
    };

    struct ChannelState
    {
        sc::Biquad hp[2], lp[2];
        float hpFreq = -1.f, lpFreq = -1.f;
        bool  hpOn = false, lpOn = false;
        bool  scanning = false;
        int   scanCount = 0, lockout = 0;
        float peak = 0.f, gainSm = 0.f;
        Voice voices[numVoices];
    };

    struct BlockParams
    {
        float gain = 1.f, thr = 0.1f, hpf = 20.f, lpf = 20000.f, vel = 0.f, levelGain = 1.f;
        int   retrigSamples = 0;
    };

    void triggerVoice (int c, float peak, const BlockParams& bp);
    std::unique_ptr<juce::XmlElement> createStateXml();
    void restoreFromXml (const juce::XmlElement&);
    void setCurrentPresetName (const juce::String&);

    inline void pushFft (int c, float x) noexcept
    {
        const int w = fftWritePos[c].load (std::memory_order_relaxed);
        fftRing[c][(size_t) w] = x;
        fftWritePos[c].store ((w + 1) & (fftSize - 1), std::memory_order_relaxed);
    }

    juce::AudioFormatManager formatManager;
    mutable juce::CriticalSection sampleLock;
    std::shared_ptr<const SampleData> slots[2];
    std::vector<std::shared_ptr<const SampleData>> retired;
    juce::String currentPresetName;

    std::atomic<float>* pSens[2]   {};
    std::atomic<float>* pHpf[2]    {};
    std::atomic<float>* pLpf[2]    {};
    std::atomic<float>* pThr[2]    {};
    std::atomic<float>* pRetrig[2] {};
    std::atomic<float>* pVel[2]    {};
    std::atomic<float>* pLevel[2]  {};
    std::atomic<float>* pMute[2]   {};
    std::atomic<float>* pLink = nullptr;

    ChannelState chans[2];
    double sampleRate = 44100.0;
    int scanLen = 26;

    std::atomic<float> meter[2] { 0.f, 0.f };
    std::atomic<int> sigHold { 0 }, clipHold { 0 };

    std::array<float, fftSize> fftRing[2];
    std::atomic<int> fftWritePos[2] { 0, 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleCoreProcessor)
};
