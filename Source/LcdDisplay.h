#pragma once
#include <JuceHeader.h>
#include <array>
#include "PluginProcessor.h"
#include "UiKit.h"

//==============================================================================
// LCD:
//  - Tampilan awal : nama preset, lalu "Sampling 1/2" (diganti nama sampling) + meter output
//  - Tampilan SELECT: FFT input kanal terpilih + node HPF/LPF (bisa digeser) + nama sampling
//==============================================================================
class LcdDisplay : public juce::Component, private juce::Timer
{
public:
    explicit LcdDisplay (SampleCoreProcessor& p) : proc (p)
    {
        bg         = SC_IMG (display_bg);
        titleImg   = SC_IMG (label_preset_display);
        rowLabel[0] = SC_IMG (label_ch1);
        rowLabel[1] = SC_IMG (label_ch2);
        meterTrack = SC_IMG (meter_track);
        meterFill  = SC_IMG (meter_fill);
        fftData.fill (0.f);
        spectrumDb.fill (-100.f);
        startTimerHz (30);
    }

    void setSelectedChannel (int ch)   // 0 = tidak ada, 1, 2
    {
        selected = ch;
        spectrumDb.fill (-100.f);
        repaint();
    }

    void setPresetInfo (const juce::String& name, bool pendingLoad)
    {
        presetName = name;
        pending = pendingLoad;
        repaint();
    }

    void showMessage (const juce::String& m, int ms = 2500)
    {
        message = m;
        messageMs = ms;
        repaint();
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImageAt (bg, 0, 0);

        if (selected == 0) paintOverview (g);
        else               paintEdit (g);

        if (message.isNotEmpty())
        {
            g.setColour (sc::col::inset.withAlpha (0.85f));
            g.fillRoundedRectangle (16.f, 148.f, 548.f, 18.f, 4.f);
            g.setColour (sc::col::orange);
            g.setFont (sc::sans (11.f, true));
            g.drawText (message, 20, 148, 540, 18, juce::Justification::centredLeft, true);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        drag = 0;
        if (selected == 0) return;

        const auto hp = nodePos (true), lp = nodePos (false);
        const float dh = hp.getDistanceFrom (e.position), dl = lp.getDistanceFrom (e.position);
        if (juce::jmin (dh, dl) > 18.f) return;

        drag = dh <= dl ? 1 : 2;
        dragParam = proc.apvts.getParameter (juce::String (drag == 1 ? "hpf" : "lpf") + juce::String (selected));
        if (dragParam != nullptr) dragParam->beginChangeGesture();
        applyDrag (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override { if (drag != 0) applyDrag (e); }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (drag != 0 && dragParam != nullptr) dragParam->endChangeGesture();
        drag = 0;
        dragParam = nullptr;
    }

private:
    //== helpers ==============================================================
    static juce::String fmtFreq (float f)
    {
        return f >= 1000.f ? juce::String (f / 1000.f, 2) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz";
    }

    static void glowText (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r,
                          const juce::Font& f, juce::Colour c)
    {
        g.setFont (f);
        g.setColour (c.withAlpha (0.18f));
        static const int offs[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
        for (auto& o : offs)
            g.drawText (t, r.translated (o[0], o[1]), juce::Justification::centredLeft, true);
        g.setColour (c);
        g.drawText (t, r, juce::Justification::centredLeft, true);
    }

    juce::Rectangle<float> getPlot() const { return { 16.f, 58.f, 548.f, 90.f }; }

    float freqToX (float f) const
    {
        const auto p = getPlot();
        return p.getX() + p.getWidth() * std::log (juce::jlimit (20.f, 20000.f, f) / 20.f) / std::log (1000.f);
    }

    float xToFreq (float x) const
    {
        const auto p = getPlot();
        const float t = juce::jlimit (0.f, 1.f, (x - p.getX()) / p.getWidth());
        return 20.f * std::pow (1000.f, t);
    }

    float getParam (const char* base) const
    {
        return proc.apvts.getRawParameterValue (juce::String (base) + juce::String (selected))->load();
    }

    double totalMagnitude (double f) const
    {
        const double fs = proc.getCurrentSampleRate();
        const float hpf = getParam ("hpf"), lpf = getParam ("lpf");
        double m = 1.0;
        if (hpf > 20.5f)    m *= sc::lr4Magnitude (true,  fs, hpf, f);
        if (lpf < 19999.f)  m *= sc::lr4Magnitude (false, fs, lpf, f);
        return m;
    }

    float dbToY (double db) const
    {
        const auto p = getPlot();
        return p.getY() + 8.f + (float) juce::jlimit (0.0, 60.0, -db) / 60.f * (p.getHeight() - 16.f);
    }

    juce::Point<float> nodePos (bool hp) const
    {
        const float f = getParam (hp ? "hpf" : "lpf");
        const double db = 20.0 * std::log10 (juce::jmax (1e-6, totalMagnitude (f)));
        return { freqToX (f), dbToY (db) };
    }

    void applyDrag (const juce::MouseEvent& e)
    {
        if (dragParam == nullptr) return;
        float f = xToFreq (e.position.x);
        if (drag == 1) f = juce::jmin (f, getParam ("lpf") / 1.1f);
        else           f = juce::jmax (f, getParam ("hpf") * 1.1f);
        f = juce::jlimit (20.f, 20000.f, f);
        dragParam->setValueNotifyingHost (dragParam->convertTo0to1 (f));
    }

    //== tampilan awal =========================================================
    void paintOverview (juce::Graphics& g)
    {
        g.drawImageAt (titleImg, 14, 6);

        glowText (g, presetName.isEmpty() ? juce::String ("INIT") : presetName.toUpperCase(),
                  { 20, 38, 540, 32 }, sc::mono (26.f, true), sc::col::orange);

        if (pending)
        {
            g.setColour (sc::col::cyan);
            g.setFont (sc::sans (10.f, true));
            g.drawText ("KLIK KIRI KNOP PRESET UNTUK LOAD", 20, 70, 400, 14, juce::Justification::centredLeft);
        }

        for (int ch = 0; ch < 2; ++ch)
        {
            const int rowY = 100 + ch * 36;
            const auto nm = proc.getSampleName (ch + 1);

            if (nm.isEmpty())
            {
                g.setOpacity (0.85f);
                g.drawImage (rowLabel[ch], 16, rowY - 12, 140, 24, 0, 0, rowLabel[ch].getWidth(), rowLabel[ch].getHeight());
            }
            else
            {
                g.setColour (sc::col::lilac.brighter (0.4f));
                g.setFont (sc::sans (13.f, true));
                g.drawText (nm.toUpperCase(), 20, rowY - 10, 140, 20, juce::Justification::centredLeft, true);
            }

            const float level = proc.getMeterLevel (ch + 1);
            const float db = level > 1e-5f ? juce::Decibels::gainToDecibels (level) : -100.f;
            drawMeter (g, { 168, rowY - 9, 330, 18 }, db);

            g.setColour (sc::col::lilac);
            g.setFont (sc::mono (13.f));
            g.drawText (db < -90.f ? juce::String ("-inf") : juce::String (db, 1), 504, rowY - 10, 64, 20,
                        juce::Justification::centredRight);
        }
    }

    void drawMeter (juce::Graphics& g, juce::Rectangle<int> r, float db)
    {
        g.setOpacity (1.f);
        sc::blit (g, meterTrack, r);
        const float frac = juce::jlimit (0.f, 1.f, juce::jmap (juce::jlimit (-60.f, 0.f, db), -60.f, 0.f, 0.f, 1.f));
        if (frac <= 0.f) return;

        g.saveState();
        g.reduceClipRegion (r.getX(), r.getY(), juce::roundToInt ((float) r.getWidth() * frac), r.getHeight());
        sc::blit (g, meterFill, r);
        g.restoreState();
    }

    //== tampilan SELECT: FFT + node ==========================================
    void paintEdit (juce::Graphics& g)
    {
        const int ch = selected;
        const double fs = proc.getCurrentSampleRate();
        const auto plot = getPlot();

        g.setColour (sc::col::cyan);
        g.setFont (sc::sans (13.f, true));
        g.drawText ("CH " + juce::String (ch) + "  -  INPUT FFT", 18, 8, 300, 18, juce::Justification::centredLeft);
        g.setColour (sc::col::lilac);
        g.setFont (sc::sans (10.f));
        g.drawText ("GESER NODE HPF / LPF", 360, 8, 204, 18, juce::Justification::centredRight);

        const auto nm = proc.getSampleName (ch);
        glowText (g, nm.isEmpty() ? juce::String ("(BELUM ADA SAMPLING - TEKAN WAVE)") : nm.toUpperCase(),
                  { 18, 28, 546, 24 }, sc::mono (nm.isEmpty() ? 12.f : 18.f, true), sc::col::orange);

        g.setColour (sc::col::inset);
        g.fillRoundedRectangle (plot, 4.f);

        // grid + label frekuensi
        g.setFont (sc::sans (9.f));
        for (float f : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
        {
            const float x = freqToX (f);
            g.setColour (sc::col::border.withAlpha (0.55f));
            g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
        }
        g.setColour (sc::col::lilac.withAlpha (0.8f));
        for (float f : { 20.f, 100.f, 1000.f, 10000.f, 20000.f })
        {
            const juce::String t = f >= 1000.f ? juce::String (juce::roundToInt (f / 1000.f)) + "k" : juce::String (juce::roundToInt (f));
            g.drawText (t, juce::roundToInt (freqToX (f)) - 18, juce::roundToInt (plot.getBottom()) + 2, 36, 12,
                        juce::Justification::centred);
        }

        g.saveState();
        g.reduceClipRegion (plot.toNearestInt());

        // spektrum
        juce::Path fill, line;
        const float bottom = plot.getBottom();
        fill.startNewSubPath (plot.getX(), bottom);
        bool first = true;
        for (float px = 0.f; px <= plot.getWidth(); px += 2.f)
        {
            const float f = xToFreq (plot.getX() + px);
            const float bin = juce::jlimit (1.f, (float) spectrumDb.size() - 2.f, f * (float) SampleCoreProcessor::fftSize / (float) fs);
            const int b0 = (int) bin;
            const float db = spectrumDb[(size_t) b0] + (bin - (float) b0) * (spectrumDb[(size_t) b0 + 1] - spectrumDb[(size_t) b0]);
            const float y = bottom - juce::jlimit (0.f, 1.f, (db + 90.f) / 90.f) * plot.getHeight();
            fill.lineTo (plot.getX() + px, y);
            if (first) { line.startNewSubPath (plot.getX() + px, y); first = false; }
            else         line.lineTo (plot.getX() + px, y);
        }
        fill.lineTo (plot.getRight(), bottom);
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (sc::col::cyan.withAlpha (0.45f), 0.f, plot.getY(),
                                                 sc::col::magenta.withAlpha (0.08f), 0.f, bottom, false));
        g.fillPath (fill);
        g.setColour (sc::col::cyan);
        g.strokePath (line, juce::PathStrokeType (1.2f));

        // kurva filter
        juce::Path curve;
        first = true;
        for (float px = 0.f; px <= plot.getWidth(); px += 3.f)
        {
            const float f = xToFreq (plot.getX() + px);
            const double db = 20.0 * std::log10 (juce::jmax (1e-6, totalMagnitude (f)));
            const float y = dbToY (db);
            if (first) { curve.startNewSubPath (plot.getX() + px, y); first = false; }
            else         curve.lineTo (plot.getX() + px, y);
        }
        g.setColour (sc::col::orange);
        g.strokePath (curve, juce::PathStrokeType (1.6f));
        g.restoreState();

        // node
        for (bool hp : { true, false })
        {
            const auto p = nodePos (hp);
            const auto c = hp ? sc::col::cyan : sc::col::magenta;
            g.setColour (c.withAlpha (0.25f));
            g.fillEllipse (p.x - 11.f, p.y - 11.f, 22.f, 22.f);
            g.setColour (c);
            g.fillEllipse (p.x - 6.f, p.y - 6.f, 12.f, 12.f);
            g.setColour (juce::Colours::white);
            g.drawEllipse (p.x - 6.f, p.y - 6.f, 12.f, 12.f, 1.2f);

            const juce::String t = juce::String (hp ? "HPF " : "LPF ") + fmtFreq (getParam (hp ? "hpf" : "lpf"));
            float ty = p.y - 26.f;
            if (ty < plot.getY()) ty = p.y + 12.f;
            const float tx = juce::jlimit (plot.getX(), plot.getRight() - 90.f, p.x - 45.f);
            g.setColour (c);
            g.setFont (sc::sans (10.f, true));
            g.drawText (t, juce::Rectangle<float> (tx, ty, 90.f, 14.f), juce::Justification::centred);
        }
    }

    //== timer =================================================================
    void computeSpectrum()
    {
        proc.copyFftInput (selected, fftData.data());
        std::fill (fftData.begin() + SampleCoreProcessor::fftSize, fftData.end(), 0.f);
        window.multiplyWithWindowingTable (fftData.data(), (size_t) SampleCoreProcessor::fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        const float norm = (float) SampleCoreProcessor::fftSize * 0.25f;
        for (size_t k = 0; k < spectrumDb.size(); ++k)
        {
            const float db = juce::Decibels::gainToDecibels (fftData[k] / norm, -100.f);
            float& s = spectrumDb[k];
            s = db > s ? s * 0.5f + db * 0.5f : s * 0.85f + db * 0.15f;
        }
    }

    void timerCallback() override
    {
        if (selected > 0) computeSpectrum();

        if (messageMs > 0)
        {
            messageMs -= 33;
            if (messageMs <= 0) message.clear();
        }
        repaint();
    }

    //==========================================================================
    SampleCoreProcessor& proc;
    juce::Image bg, titleImg, rowLabel[2], meterTrack, meterFill;

    int selected = 0, drag = 0;
    juce::RangedAudioParameter* dragParam = nullptr;
    juce::String presetName, message;
    bool pending = false;
    int messageMs = 0;

    juce::dsp::FFT fft { SampleCoreProcessor::fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) SampleCoreProcessor::fftSize,
                                                 juce::dsp::WindowingFunction<float>::hann };
    std::array<float, (size_t) SampleCoreProcessor::fftSize * 2> fftData;
    std::array<float, (size_t) SampleCoreProcessor::fftSize / 2> spectrumDb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LcdDisplay)
};
