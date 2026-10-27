#include "PluginEditor.h"

using namespace juce;

namespace
{
    constexpr int W = 984, H = 404;

    constexpr int lcdX = 16,  lcdY = 84;
    constexpr int panelY = 274;
    constexpr int presetX = 608, presetY = 84;
    constexpr int stripX[2] = { 756, 868 }, stripY = 84;
}

//==============================================================================
SampleCoreEditor::SampleCoreEditor (SampleCoreProcessor& p)
    : AudioProcessorEditor (&p), proc (p), lcd (p),
      saveBtn   ("save",   SC_IMG (btn_save),   120, 38),
      resetBtn  ("reset",  SC_IMG (btn_reset),  120, 38),
      linkBtn   ("link",   SC_IMG (btn_link),   120, 38),
      waveBtn   ("wave",   SC_IMG (btn_wave),   120, 38),
      cancelBtn ("cancel", SC_IMG (btn_cancel), 120, 38),
      selectBtn { { "select1", SC_IMG (btn_select), 90, 30 }, { "select2", SC_IMG (btn_select), 90, 30 } },
      muteBtn   { { "mute1",   SC_IMG (btn_mute),   90, 30 }, { "mute2",   SC_IMG (btn_mute),   90, 30 } }
{
    setLookAndFeel (&lnf);

    background  = SC_IMG (background);
    headerBg    = SC_IMG (header_bg);
    brandTitle  = SC_IMG (label_brand_title);
    brandSub    = SC_IMG (label_brand_subtitle);
    knobPanelBg = SC_IMG (knob_panel_bg);
    knobLabels  = SC_IMG (knob_labels);
    stripBg     = SC_IMG (channel_strip_bg);
    presetBg    = SC_IMG (preset_section_bg);
    ledPower    = SC_IMG (led_power);
    ledSignal   = SC_IMG (led_signal);
    ledClip     = SC_IMG (led_clip);
    logo        = SC_IMG (logo).rescaled (112, 110, Graphics::highResamplingQuality);   // logo asli, hanya di-downscale

    addAndMakeVisible (lcd);

    //== 6 knob parameter utama ================================================
    for (auto& k : mainKnob)
    {
        k.setSliderStyle (Slider::RotaryVerticalDrag);
        k.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
        k.getProperties().set ("knob", (int) sc::SampleCoreLookAndFeel::knobMain);
        k.setMouseDragSensitivity (200);
        k.setPopupDisplayEnabled (true, true, this);
        k.setEnabled (false);
        addAndMakeVisible (k);
    }

    //== knob PRESET ===========================================================
    presetKnob.setSliderStyle (Slider::RotaryVerticalDrag);
    presetKnob.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    presetKnob.getProperties().set ("knob", (int) sc::SampleCoreLookAndFeel::knobPreset);
    presetKnob.setPopupMenuEnabled (false);
    presetKnob.setMouseDragSensitivity (120);
    presetKnob.setRange (0.0, 1.0, 1.0);
    presetKnob.onValueChange = [this] { browsing = true; updateLcdPreset(); };
    presetKnob.onClickRestore = [this] { restoreBrowsedPreset(); };
    addAndMakeVisible (presetKnob);

    //== tombol kiri ===========================================================
    saveBtn.onClick   = [this] { onSave(); };
    resetBtn.onClick  = [this] { onReset(); };
    waveBtn.onClick   = [this] { onWave(); };
    cancelBtn.onClick = [this] { onCancel(); };
    linkBtn.setClickingTogglesState (true);
    linkAtt = std::make_unique<ButtonAtt> (proc.apvts, "link", linkBtn);
    for (auto* b : { &saveBtn, &resetBtn, &linkBtn, &waveBtn, &cancelBtn })
        addAndMakeVisible (*b);
    waveBtn.setDimmed (true);

    //== channel strip =========================================================
    for (int i = 0; i < 2; ++i)
    {
        const String s (i + 1);

        stripKnob[i].setSliderStyle (Slider::RotaryVerticalDrag);
        stripKnob[i].setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
        stripKnob[i].getProperties().set ("knob", (int) sc::SampleCoreLookAndFeel::knobStrip);
        stripKnob[i].setMouseDragSensitivity (200);
        stripKnob[i].setPopupDisplayEnabled (true, true, this);
        stripKnobAtt[i] = std::make_unique<SliderAtt> (proc.apvts, "sens" + s, stripKnob[i]);   // = knop Sensitivity channel ini
        addAndMakeVisible (stripKnob[i]);

        fader[i].setSliderStyle (Slider::LinearVertical);
        fader[i].setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
        fader[i].setPopupDisplayEnabled (true, true, this);
        faderAtt[i] = std::make_unique<SliderAtt> (proc.apvts, "level" + s, fader[i]);
        addAndMakeVisible (fader[i]);

        selectBtn[i].setClickingTogglesState (true);
        selectBtn[i].onClick = [this, i] { setSelectedChannel (selectBtn[i].getToggleState() ? i + 1 : 0); };
        addAndMakeVisible (selectBtn[i]);

        muteBtn[i].setClickingTogglesState (true);
        muteAtt[i] = std::make_unique<ButtonAtt> (proc.apvts, "mute" + s, muteBtn[i]);
        addAndMakeVisible (muteBtn[i]);
    }

    setSize (W, H);
    refreshPresetUI();
    proc.addChangeListener (this);
    startTimerHz (10);
}

SampleCoreEditor::~SampleCoreEditor()
{
    proc.removeChangeListener (this);
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void SampleCoreEditor::paint (Graphics& g)
{
    g.setImageResamplingQuality (Graphics::highResamplingQuality);

    sc::blit (g, background, { 0, 0, W, H });
    sc::blit (g, headerBg,   { 6, 6, W - 12, 64 });

    g.drawImage (logo, 16, 10, 56, 55, 0, 0, logo.getWidth(), logo.getHeight());
    g.drawImage (brandTitle, 84, 10, 231, 33, 0, 0, brandTitle.getWidth(), brandTitle.getHeight());
    g.drawImage (brandSub,   84, 44, 231, 14, 0, 0, brandSub.getWidth(),   brandSub.getHeight());

    // LED: power, signal, clip
    const int ledX = W - 110;
    g.drawImage (ledPower,  ledX,      27, 16, 16, 0, 16 * 1, 16, 16);
    g.drawImage (ledSignal, ledX + 24, 27, 16, 16, 0, 16 * (proc.isSignalOn() ? 1 : 0), 16, 16);
    g.drawImage (ledClip,   ledX + 48, 27, 16, 16, 0, 16 * (proc.isClipping() ? 2 : 0), 16, 16);

    // panel knob parameter + label
    sc::blit (g, knobPanelBg, { lcdX, panelY, 580, 110 });
    g.drawImageAt (knobLabels, lcdX, panelY + 62);

    // section preset
    sc::blit (g, presetBg, { presetX, presetY, 136, 300 });
    g.setColour (sc::col::lilac);
    g.setFont (sc::sans (12.f, true));
    g.drawText ("PRESET", presetX, presetY + 6, 136, 16, Justification::centred);

    // channel strip
    for (int i = 0; i < 2; ++i)
    {
        sc::blit (g, stripBg, { stripX[i], stripY, 100, 300 });
        g.setColour (sc::col::lilac);
        g.setFont (sc::sans (13.f, true));
        g.drawText ("CH " + String (i + 1), stripX[i], stripY + 7, 100, 18, Justification::centred);
        g.setFont (sc::sans (10.f, true));
        g.drawText ("SENSITIV", stripX[i], stripY + 80, 100, 14, Justification::centred);
    }
}

void SampleCoreEditor::resized()
{
    lcd.setBounds (lcdX, lcdY, 580, 170);

    const float colW = 580.f / 6.f;
    for (int i = 0; i < 6; ++i)
        mainKnob[i].setBounds (lcdX + roundToInt (colW * ((float) i + 0.5f)) - 32, panelY + 8, 64, 64);

    presetKnob.setBounds (presetX + 40, presetY + 24, 56, 56);

    int y = presetY + 86;
    for (auto* b : { &saveBtn, &resetBtn, &linkBtn, &waveBtn, &cancelBtn })
    {
        b->setBounds (presetX + 8, y, 120, 38);
        y += 43;
    }

    for (int i = 0; i < 2; ++i)
    {
        stripKnob[i].setBounds (stripX[i] + 24, stripY + 26, 52, 52);
        selectBtn[i].setBounds (stripX[i] + 5, stripY + 98, 90, 30);
        fader[i].setBounds (stripX[i] + 20, stripY + 134, 60, 126);
        muteBtn[i].setBounds (stripX[i] + 5, stripY + 264, 90, 30);
    }
}

//==============================================================================
void SampleCoreEditor::timerCallback()
{
    repaint (W - 110, 20, 80, 30);   // LED
}

void SampleCoreEditor::changeListenerCallback (ChangeBroadcaster*)
{
    lcd.repaint();
    if (! presetKnob.isMouseButtonDown())
        refreshPresetUI();
}

//==============================================================================
// SELECT: knob utama mengikuti channel yang dipilih
//==============================================================================
void SampleCoreEditor::setSelectedChannel (int ch)
{
    selected = ch;
    selectBtn[0].setToggleState (ch == 1, dontSendNotification);
    selectBtn[1].setToggleState (ch == 2, dontSendNotification);

    static const char* ids[6] = { "sens", "hpf", "lpf", "thr", "retrig", "vel" };
    for (int i = 0; i < 6; ++i)
    {
        mainAtt[i].reset();
        mainKnob[i].setEnabled (ch > 0);
        if (ch > 0)
            mainAtt[i] = std::make_unique<SliderAtt> (proc.apvts, String (ids[i]) + String (ch), mainKnob[i]);
    }

    waveBtn.setDimmed (ch == 0);
    lcd.setSelectedChannel (ch);
}

//==============================================================================
// Preset
//==============================================================================
void SampleCoreEditor::refreshPresetUI()
{
    presetNames = proc.getPresetNames();
    const int n = presetNames.size();

    presetKnob.setEnabled (n > 0);
    presetKnob.setRange (0.0, n > 1 ? (double) (n - 1) : 1.0, 1.0);
    const int cur = presetNames.indexOf (proc.getCurrentPresetName());
    presetKnob.setValue (cur >= 0 ? cur : 0, dontSendNotification);

    browsing = false;
    updateLcdPreset();
}

void SampleCoreEditor::updateLcdPreset()
{
    const auto loaded = proc.getCurrentPresetName();
    const int idx = roundToInt (presetKnob.getValue());

    if (browsing && presetNames.size() > 0 && isPositiveAndBelow (idx, presetNames.size()))
        lcd.setPresetInfo (presetNames[idx], presetNames[idx] != loaded);
    else
        lcd.setPresetInfo (loaded, false);
}

void SampleCoreEditor::restoreBrowsedPreset()
{
    const int idx = roundToInt (presetKnob.getValue());
    if (! isPositiveAndBelow (idx, presetNames.size())) return;

    if (proc.loadPreset (presetNames[idx]))
    {
        refreshPresetUI();
        lcd.showMessage ("PRESET DI-LOAD: " + presetNames[idx].toUpperCase());
    }
    else
        lcd.showMessage ("GAGAL LOAD PRESET");
}

//==============================================================================
// Tombol
//==============================================================================
void SampleCoreEditor::onSave()
{
    String def = proc.getCurrentPresetName();
    if (def.isEmpty())
        def = "PRESET_" + String (proc.getPresetNames().size() + 1).paddedLeft ('0', 2);

    auto* aw = new AlertWindow ("SAVE PRESET", "Rename - beri nama preset:", MessageBoxIconType::NoIcon, this);
    aw->addTextEditor ("name", def, "Nama preset:");
    aw->addButton ("SAVE",   1, KeyPress (KeyPress::returnKey));
    aw->addButton ("CANCEL", 0, KeyPress (KeyPress::escapeKey));

    Component::SafePointer<SampleCoreEditor> safe (this);
    aw->enterModalState (true,
        new sc::LambdaModalCallback ([safe, aw] (int result)
        {
            std::unique_ptr<AlertWindow> owner (aw);
            if (result != 1 || safe == nullptr) return;
            safe->doSave (owner->getTextEditorContents ("name").trim());
        }),
        false);
}

void SampleCoreEditor::doSave (const String& name)
{
    if (proc.savePreset (name))
    {
        refreshPresetUI();
        lcd.showMessage ("PRESET TERSIMPAN: " + proc.getCurrentPresetName().toUpperCase());
    }
    else
        lcd.showMessage ("NAMA PRESET TIDAK VALID");
}

void SampleCoreEditor::onReset()
{
    Component::SafePointer<SampleCoreEditor> safe (this);
    AlertWindow::showAsync (MessageBoxOptions::makeOptionsOkCancel (
                                MessageBoxIconType::WarningIcon, "RESET",
                                "Hapus SEMUA preset dan sampling?\nTindakan ini tidak bisa dibatalkan.",
                                "HAPUS", "BATAL", this),
                            [safe] (int r) { if (r == 1 && safe != nullptr) safe->doReset(); });
}

void SampleCoreEditor::doReset()
{
    proc.resetEverything();
    refreshPresetUI();
    lcd.showMessage ("RESET SELESAI - SEMUA PRESET & SAMPLING DIHAPUS");
}

void SampleCoreEditor::onWave()
{
    if (selected == 0)      // WAVE hanya bekerja bila channel sudah di-SELECT
    {
        lcd.showMessage ("TEKAN SELECT (CH1/CH2) DULU");
        return;
    }

    const int ch = selected;
    chooser = std::make_unique<FileChooser> ("Pilih sampling untuk CH " + String (ch), File(),
                                             "*.wav;*.aif;*.aiff;*.flac");
    Component::SafePointer<SampleCoreEditor> safe (this);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [safe, ch] (const FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (safe != nullptr && f.existsAsFile())
                                  safe->doLoadSample (ch, f);
                          });
}

void SampleCoreEditor::doLoadSample (int ch, const File& f)
{
    if (proc.loadSample (ch, f, true))
        lcd.showMessage ("CH" + String (ch) + " SAMPLING: " + f.getFileNameWithoutExtension().toUpperCase());
    else
        lcd.showMessage ("GAGAL MEMUAT FILE AUDIO");
}

void SampleCoreEditor::onCancel()
{
    setSelectedChannel (0);
    refreshPresetUI();        // kembali ke tampilan awal LCD
    lcd.showMessage ({}, 0);
}
