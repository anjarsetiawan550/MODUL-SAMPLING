#pragma once
#include <JuceHeader.h>
#include <functional>

// Ambil PNG dari BinaryData: SC_IMG(btn_save) -> BinaryData::btn_save_png
#define SC_IMG(n) juce::ImageCache::getFromMemory (BinaryData::n##_png, BinaryData::n##_pngSize)

namespace sc
{
    namespace col
    {
        // Tema elegan: graphite + champagne
        inline const juce::Colour cyan      { 0xffe3b462 };   // aksen champagne
        inline const juce::Colour magenta   { 0xff8fb0d8 };   // biru baja lembut (LPF)
        inline const juce::Colour orange    { 0xfff5cd8c };   // champagne terang
        inline const juce::Colour lilac     { 0xffc9ced6 };   // abu hangat (teks)
        inline const juce::Colour red       { 0xffe05a50 };
        inline const juce::Colour panel     { 0xff202329 };
        inline const juce::Colour inset     { 0xff0d0f12 };
        inline const juce::Colour border    { 0xff383d45 };
    }
}

namespace sc
{
    inline juce::Font sans (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }

    inline juce::Font mono (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h,
                                              bold ? juce::Font::bold : juce::Font::plain));
    }

    inline void blit (juce::Graphics& g, const juce::Image& img, juce::Rectangle<int> r)
    {
        g.drawImage (img, r.getX(), r.getY(), r.getWidth(), r.getHeight(), 0, 0, img.getWidth(), img.getHeight());
    }

    // Callback modal berbasis lambda
    struct LambdaModalCallback : juce::ModalComponentManager::Callback
    {
        explicit LambdaModalCallback (std::function<void (int)> f) : fn (std::move (f)) {}
        void modalStateFinished (int result) override { if (fn) fn (result); }
        std::function<void (int)> fn;
    };

    //==========================================================================
    // Look & feel: knob 64 frame, fader (track + thumb), warna dialog
    //==========================================================================
    class SampleCoreLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        enum KnobType { knobMain = 0, knobPreset = 1, knobStrip = 2 };

        SampleCoreLookAndFeel()
        {
            knobImg[knobMain]   = SC_IMG (knob_main_64);   knobSize[knobMain]   = 64;
            knobImg[knobPreset] = SC_IMG (knob_preset_56); knobSize[knobPreset] = 56;
            knobImg[knobStrip]  = SC_IMG (knob_strip_52);  knobSize[knobStrip]  = 52;
            track = SC_IMG (fader_track);
            thumb = SC_IMG (fader_thumb);

            setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff1f2228));
            setColour (juce::AlertWindow::textColourId,       juce::Colours::white);
            setColour (juce::AlertWindow::outlineColourId,    juce::Colour (0xff5a4a2c));
            setColour (juce::TextButton::buttonColourId,      juce::Colour (0xff2b2f36));
            setColour (juce::TextButton::textColourOffId,     col::cyan);
            setColour (juce::TextEditor::backgroundColourId,  col::inset);
            setColour (juce::TextEditor::textColourId,        col::orange);
            setColour (juce::TextEditor::outlineColourId,     col::border);
            setColour (juce::TextEditor::focusedOutlineColourId, col::cyan);
            setColour (juce::TooltipWindow::backgroundColourId, col::inset);
            setColour (juce::TooltipWindow::outlineColourId,    col::cyan);
            setColour (juce::TooltipWindow::textColourId,     col::cyan);
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float, float, juce::Slider& s) override
        {
            const int t  = juce::jlimit (0, 2, (int) s.getProperties().getWithDefault ("knob", 0));
            const int fs = knobSize[t];
            const int frames = juce::jmax (1, knobImg[t].getHeight() / fs);
            const int frame = juce::jlimit (0, frames - 1, juce::roundToInt (pos * (float) (frames - 1)));

            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.setOpacity (s.isEnabled() ? 1.0f : 0.35f);
            g.drawImage (knobImg[t], x + (w - fs) / 2, y + (h - fs) / 2, fs, fs, 0, frame * fs, fs, fs);
        }

        int getSliderThumbRadius (juce::Slider&) override { return 22; }

        void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                               float, float, juce::Slider::SliderStyle, juce::Slider& s) override
        {
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.setOpacity (s.isEnabled() ? 1.0f : 0.5f);
            const int cx = x + w / 2;
            g.drawImage (track, cx - 7, y, 14, h, 0, 0, track.getWidth(), track.getHeight());
            g.drawImage (thumb, cx - 11, juce::roundToInt (sliderPos) - 22, 22, 44, 0, 0, thumb.getWidth(), thumb.getHeight());
        }

    private:
        juce::Image knobImg[3], track, thumb;
        int knobSize[3] {};
    };

    //==========================================================================
    // Tombol 3-state (Normal / Hover / Active) dari filmstrip vertikal
    //==========================================================================
    class FilmstripButton : public juce::Button
    {
    public:
        FilmstripButton (const juce::String& name, juce::Image stripImage, int frameW, int frameH)
            : juce::Button (name), strip (std::move (stripImage)), fw (frameW), fh (frameH)
        {
            setSize (fw, fh);
        }

        void setDimmed (bool d) { dimmed = d; repaint(); }

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            const int frame = (getToggleState() || down) ? 2 : (over ? 1 : 0);
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.setOpacity (dimmed ? 0.45f : 1.0f);
            g.drawImage (strip, 0, 0, getWidth(), getHeight(), 0, frame * fh, fw, fh);
        }

    private:
        juce::Image strip;
        int fw, fh;
        bool dimmed = false;
    };

    //==========================================================================
    // Knop PRESET: putar = memilih/menggeser, klik kiri (tanpa drag) = restore
    //==========================================================================
    class PresetKnob : public juce::Slider
    {
    public:
        std::function<void()> onClickRestore;

        void mouseDown (const juce::MouseEvent& e) override
        {
            leftDown = e.mods.isLeftButtonDown() && ! e.mods.isCtrlDown();
            juce::Slider::mouseDown (e);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            const bool isClick = e.mouseWasClicked() && leftDown && isEnabled();
            juce::Slider::mouseUp (e);
            if (isClick && onClickRestore) onClickRestore();
        }

    private:
        bool leftDown = false;
    };
}
