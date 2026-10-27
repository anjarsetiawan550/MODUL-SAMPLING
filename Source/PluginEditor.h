#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UiKit.h"
#include "LcdDisplay.h"

class SampleCoreEditor : public juce::AudioProcessorEditor,
                         private juce::ChangeListener,
                         private juce::Timer
{
public:
    explicit SampleCoreEditor (SampleCoreProcessor&);
    ~SampleCoreEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void setSelectedChannel (int ch);
    void refreshPresetUI();
    void updateLcdPreset();

    void onSave();
    void onReset();
    void onWave();
    void onCancel();
    void doSave (const juce::String& name);
    void doReset();
    void doLoadSample (int ch, const juce::File& f);
    void restoreBrowsedPreset();

    SampleCoreProcessor& proc;
    sc::SampleCoreLookAndFeel lnf;

    // gambar statis
    juce::Image background, headerBg, logo, brandTitle, brandSub, knobPanelBg, knobLabels,
                stripBg, presetBg, ledPower, ledSignal, ledClip;

    LcdDisplay lcd;

    // knob parameter utama (mengikuti channel yang di-SELECT)
    juce::Slider mainKnob[6];
    std::unique_ptr<SliderAtt> mainAtt[6];

    // preset
    sc::PresetKnob presetKnob;
    juce::StringArray presetNames;
    bool browsing = false;

    sc::FilmstripButton saveBtn, resetBtn, linkBtn, waveBtn, cancelBtn;

    // channel strip
    juce::Slider stripKnob[2], fader[2];
    sc::FilmstripButton selectBtn[2], muteBtn[2];
    std::unique_ptr<SliderAtt> stripKnobAtt[2], faderAtt[2];
    std::unique_ptr<ButtonAtt> muteAtt[2], linkAtt;

    int selected = 0;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleCoreEditor)
};
