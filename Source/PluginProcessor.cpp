#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
    String pctText (float v, int) { return String (roundToInt (v * 100.f)) + " %"; }

    String freqText (float v, int)
    {
        return v >= 1000.f ? String (v / 1000.f, 2) + " kHz" : String (roundToInt (v)) + " Hz";
    }

    AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        AudioProcessorValueTreeState::ParameterLayout layout;

        for (int ch = 1; ch <= 2; ++ch)
        {
            const String s (ch);

            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "sens" + s, 1 }, "Sensitivity " + s,
                NormalisableRange<float> (0.f, 1.f, 0.001f), 0.5f,
                AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));

            NormalisableRange<float> fr (20.f, 20000.f, 1.f);
            fr.setSkewForCentre (1000.f);
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "hpf" + s, 1 }, "HPF Input " + s, fr, 20.f,
                AudioParameterFloatAttributes().withStringFromValueFunction (freqText)));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "lpf" + s, 1 }, "LPF Input " + s, fr, 20000.f,
                AudioParameterFloatAttributes().withStringFromValueFunction (freqText)));

            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "thr" + s, 1 }, "Threshold " + s,
                NormalisableRange<float> (0.f, 1.f, 0.001f), 0.1f,
                AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));

            NormalisableRange<float> rr (5.f, 500.f, 1.f);
            rr.setSkewForCentre (60.f);
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "retrig" + s, 1 }, "Retrigger " + s, rr, 80.f,
                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; })));

            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "vel" + s, 1 }, "Velocity " + s,
                NormalisableRange<float> (0.f, 1.f, 0.001f), 0.7f,
                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                    { return v <= 0.001f ? String ("OFF") : String (roundToInt (v * 100.f)) + " %"; })));

            NormalisableRange<float> lr (-60.f, 6.f, 0.1f);
            lr.setSkewForCentre (-12.f);
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "level" + s, 1 }, "Level " + s, lr, 0.f,
                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                    { return v <= -59.9f ? String ("-inf") : String (v, 1) + " dB"; })));

            layout.add (std::make_unique<AudioParameterBool> (ParameterID { "mute" + s, 1 }, "Mute " + s, false));
        }

        layout.add (std::make_unique<AudioParameterBool> (ParameterID { "link", 1 }, "Link", false));
        return layout;
    }
}

//==============================================================================
SampleCoreProcessor::SampleCoreProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    formatManager.registerBasicFormats();

    for (int c = 0; c < 2; ++c)
    {
        const String s (c + 1);
        pSens[c]   = apvts.getRawParameterValue ("sens" + s);
        pHpf[c]    = apvts.getRawParameterValue ("hpf" + s);
        pLpf[c]    = apvts.getRawParameterValue ("lpf" + s);
        pThr[c]    = apvts.getRawParameterValue ("thr" + s);
        pRetrig[c] = apvts.getRawParameterValue ("retrig" + s);
        pVel[c]    = apvts.getRawParameterValue ("vel" + s);
        pLevel[c]  = apvts.getRawParameterValue ("level" + s);
        pMute[c]   = apvts.getRawParameterValue ("mute" + s);
        fftRing[c].fill (0.f);
    }
    pLink = apvts.getRawParameterValue ("link");
}

SampleCoreProcessor::~SampleCoreProcessor() {}

bool SampleCoreProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;
    return l.getMainInputChannelSet() == AudioChannelSet::stereo() || l.getMainInputChannelSet().isDisabled();
}

void SampleCoreProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    scanLen = jmax (8, roundToInt (sr * 0.0006));   // jendela baca puncak ~0.6 ms (latensi trigger minimum yang tetap aman)
    for (auto& c : chans)
        c = ChannelState{};
    meter[0] = meter[1] = 0.f;
}

//==============================================================================
void SampleCoreProcessor::triggerVoice (int c, float peak, const BlockParams& bp)
{
    std::shared_ptr<const SampleData> data;
    {
        const ScopedTryLock tl (sampleLock);
        if (! tl.isLocked()) return;
        data = slots[c];
    }
    if (data == nullptr || data->audio.getNumSamples() < 2) return;

    auto& st = chans[c];
    Voice* v = nullptr;
    for (auto& cand : st.voices)
        if (! cand.active) { v = &cand; break; }
    if (v == nullptr)   // curi voice yang paling jauh posisinya
    {
        v = &st.voices[0];
        for (auto& cand : st.voices)
            if (cand.pos > v->pos) v = &cand;
    }

    const float pn = jlimit (0.f, 1.f, peak);
    v->data   = data;
    v->ptr    = data->audio.getReadPointer (0);
    v->len    = data->audio.getNumSamples();
    v->pos    = 0.0;
    v->inc    = data->rate / sampleRate;
    v->lp     = 0.f;
    v->active = true;

    if (bp.vel <= 0.001f)                       // velocity OFF: volume tetap
    {
        v->gain = 1.f;
        v->lpCoef = 1.f;
    }
    else
    {
        v->gain = jmax (0.03f, (1.f - bp.vel) + bp.vel * pn);
        float fc = jmap (pn, 0.f, 1.f, 1500.f, 20000.f);     // pukulan lembut = lebih gelap
        fc = 20000.f + bp.vel * (fc - 20000.f);
        v->lpCoef = jmin (1.f, 1.f - std::exp (-MathConstants<float>::twoPi * fc / (float) sampleRate));
    }

    sigHold.store (roundToInt (sampleRate * 0.08));
}

void SampleCoreProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();

    if (getTotalNumOutputChannels() < 2) { buffer.clear(); return; }

    BlockParams bp[2];
    for (int c = 0; c < 2; ++c)
    {
        auto& st = chans[c];
        auto& p = bp[c];
        p.gain = Decibels::decibelsToGain (-12.f + 48.f * pSens[c]->load());
        p.thr  = jmax (1e-4f, pThr[c]->load());
        p.hpf  = pHpf[c]->load();
        p.lpf  = pLpf[c]->load();
        p.vel  = pVel[c]->load();
        const float lvl = pLevel[c]->load();
        const bool mute = pMute[c]->load() > 0.5f;
        p.levelGain = (mute || lvl <= -59.9f) ? 0.f : Decibels::decibelsToGain (lvl);
        p.retrigSamples = roundToInt (pRetrig[c]->load() * 0.001f * (float) sampleRate);

        if (p.hpf != st.hpFreq)
        {
            st.hpFreq = p.hpf;
            st.hpOn = p.hpf > 20.5f;
            for (auto& b : st.hp) b.setHighPass (sampleRate, p.hpf);
        }
        if (p.lpf != st.lpFreq)
        {
            st.lpFreq = p.lpf;
            st.lpOn = p.lpf < 19999.f;
            for (auto& b : st.lp) b.setLowPass (sampleRate, p.lpf);
        }
    }

    const bool link = pLink->load() > 0.5f;
    float* out[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };
    float peakBlock[2] = { 0.f, 0.f };
    bool clipped = false;

    for (int i = 0; i < n; ++i)
    {
        float o[2] = { 0.f, 0.f };

        for (int c = 0; c < 2; ++c)
        {
            auto& st = chans[c];
            const auto& p = bp[c];

            const float x = c < numIn ? out[c][i] : 0.f;
            pushFft (c, x);

            float y = x;
            if (st.hpOn) { y = st.hp[0].process (y); y = st.hp[1].process (y); }
            if (st.lpOn) { y = st.lp[0].process (y); y = st.lp[1].process (y); }

            const float a = std::abs (y * p.gain);

            if (st.lockout > 0) --st.lockout;

            if (st.scanning)
            {
                st.peak = jmax (st.peak, a);
                if (++st.scanCount >= scanLen)
                {
                    triggerVoice (c, st.peak, p);
                    st.scanning = false;
                    st.lockout = p.retrigSamples;     // jeda RETRIGGER
                }
            }
            else if (st.lockout == 0 && a > p.thr)    // THRESHOLD
            {
                st.scanning = true;
                st.scanCount = 1;
                st.peak = a;
            }

            float mix = 0.f;
            for (auto& v : st.voices)
            {
                if (! v.active) continue;
                const int i0 = (int) v.pos;
                if (i0 >= v.len - 1) { v.active = false; v.data.reset(); continue; }
                const float fr = (float) (v.pos - i0);
                const float s = v.ptr[i0] + fr * (v.ptr[i0 + 1] - v.ptr[i0]);
                v.lp += v.lpCoef * (s - v.lp);
                mix += v.lp * v.gain;
                v.pos += v.inc;
            }

            st.gainSm += (p.levelGain - st.gainSm) * 0.003f;
            o[c] = mix * st.gainSm;
            peakBlock[c] = jmax (peakBlock[c], std::abs (o[c]));
        }

        float l = o[0], r = o[1];
        if (link) { l = r = (o[0] + o[1]) * 0.7071f; }   // LINK: output 1 + 2 digabung

        if (std::abs (l) >= 1.f || std::abs (r) >= 1.f) clipped = true;
        out[0][i] = l;
        out[1][i] = r;
    }

    for (int c = 2; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);

    const float decay = std::pow (0.1f, (float) n / (float) (sampleRate * 0.5));   // -20 dB / 0.5 s
    for (int c = 0; c < 2; ++c)
        meter[c].store (jmax (peakBlock[c], meter[c].load() * decay));

    sigHold.store (jmax (0, sigHold.load() - n));
    if (clipped) clipHold.store (roundToInt (sampleRate * 0.6));
    else         clipHold.store (jmax (0, clipHold.load() - n));
}

void SampleCoreProcessor::copyFftInput (int ch, float* dest) const
{
    const int c = jlimit (0, 1, ch - 1);
    const int w = fftWritePos[c].load (std::memory_order_relaxed);
    for (int i = 0; i < fftSize; ++i)
        dest[i] = fftRing[c][(size_t) ((w + i) & (fftSize - 1))];
}

//==============================================================================
// Sampling
//==============================================================================
bool SampleCoreProcessor::loadSample (int ch, const File& src, bool copyToStore)
{
    if (ch < 1 || ch > 2 || ! src.existsAsFile()) return false;

    File file = src;
    if (copyToStore && ! src.isAChildOf (getSampleDir()))
    {
        getSampleDir().createDirectory();
        auto dest = getSampleDir().getNonexistentChildFile (src.getFileNameWithoutExtension(), src.getFileExtension(), false);
        if (src.copyFileTo (dest)) file = dest;
    }

    std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr) return false;

    const auto len = (int) jmin (reader->lengthInSamples, (int64) (reader->sampleRate * 30.0));   // maks 30 detik
    if (len < 2 || reader->numChannels < 1) return false;

    AudioBuffer<float> tmp ((int) reader->numChannels, len);
    reader->read (&tmp, 0, len, 0, true, true);

    auto data = std::make_shared<SampleData>();
    data->audio.setSize (1, len);
    data->audio.clear();
    for (int c = 0; c < tmp.getNumChannels(); ++c)
        data->audio.addFrom (0, 0, tmp, c, 0, len, 1.f / (float) tmp.getNumChannels());
    data->rate = reader->sampleRate;
    data->name = file.getFileNameWithoutExtension();
    data->file = file;

    {
        const ScopedLock sl (sampleLock);
        if (slots[ch - 1] != nullptr) retired.push_back (slots[ch - 1]);
        slots[ch - 1] = data;
        retired.erase (std::remove_if (retired.begin(), retired.end(),
                       [] (const auto& p) { return p.use_count() == 1; }), retired.end());
    }
    sendChangeMessage();
    return true;
}

void SampleCoreProcessor::clearSample (int ch)
{
    if (ch < 1 || ch > 2) return;
    {
        const ScopedLock sl (sampleLock);
        if (slots[ch - 1] != nullptr) retired.push_back (slots[ch - 1]);
        slots[ch - 1].reset();
    }
    sendChangeMessage();
}

String SampleCoreProcessor::getSampleName (int ch) const
{
    if (ch < 1 || ch > 2) return {};
    const ScopedLock sl (sampleLock);
    return slots[ch - 1] != nullptr ? slots[ch - 1]->name : String();
}

//==============================================================================
// Preset
//==============================================================================
File SampleCoreProcessor::getRootDir() const
{
    return File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("SampleCore");
}

StringArray SampleCoreProcessor::getPresetNames() const
{
    StringArray names;
    for (const auto& f : getPresetDir().findChildFiles (File::findFiles, false, "*.scpreset"))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

String SampleCoreProcessor::getCurrentPresetName() const
{
    const ScopedLock sl (sampleLock);
    return currentPresetName;
}

void SampleCoreProcessor::setCurrentPresetName (const String& n)
{
    const ScopedLock sl (sampleLock);
    currentPresetName = n;
}

std::unique_ptr<XmlElement> SampleCoreProcessor::createStateXml()
{
    auto root = std::make_unique<XmlElement> ("SCSTATE");
    root->setAttribute ("preset", getCurrentPresetName());

    if (auto params = apvts.copyState().createXml())
        root->addChildElement (params.release());

    for (int ch = 1; ch <= 2; ++ch)
    {
        auto* s = root->createNewChildElement ("SAMPLE");
        s->setAttribute ("ch", ch);
        const ScopedLock sl (sampleLock);
        s->setAttribute ("file", slots[ch - 1] != nullptr ? slots[ch - 1]->file.getFullPathName() : String());
    }
    return root;
}

void SampleCoreProcessor::restoreFromXml (const XmlElement& xml)
{
    if (! xml.hasTagName ("SCSTATE")) return;

    if (auto* p = xml.getChildByName (apvts.state.getType().toString()))
        apvts.replaceState (ValueTree::fromXml (*p));

    clearSample (1);
    clearSample (2);
    for (auto* s : xml.getChildWithTagNameIterator ("SAMPLE"))
    {
        const File f (s->getStringAttribute ("file"));
        if (f.existsAsFile())
            loadSample (s->getIntAttribute ("ch"), f, false);
    }

    setCurrentPresetName (xml.getStringAttribute ("preset"));
    sendChangeMessage();
}

bool SampleCoreProcessor::savePreset (const String& name)
{
    const auto legal = File::createLegalFileName (name.trim());
    if (legal.isEmpty()) return false;

    getPresetDir().createDirectory();
    setCurrentPresetName (legal);
    auto xml = createStateXml();
    const bool ok = xml->writeTo (getPresetDir().getChildFile (legal + ".scpreset"));
    sendChangeMessage();
    return ok;
}

bool SampleCoreProcessor::loadPreset (const String& name)
{
    const auto file = getPresetDir().getChildFile (name + ".scpreset");
    if (! file.existsAsFile()) return false;

    auto xml = parseXML (file);
    if (xml == nullptr) return false;

    restoreFromXml (*xml);
    setCurrentPresetName (name);
    sendChangeMessage();
    return true;
}

void SampleCoreProcessor::resetEverything()
{
    clearSample (1);
    clearSample (2);
    getPresetDir().deleteRecursively();
    getSampleDir().deleteRecursively();

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    setCurrentPresetName ({});
    sendChangeMessage();
}

//==============================================================================
void SampleCoreProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = createStateXml())
        copyXmlToBinary (*xml, dest);
}

void SampleCoreProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        restoreFromXml (*xml);
}

AudioProcessorEditor* SampleCoreProcessor::createEditor() { return new SampleCoreEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SampleCoreProcessor(); }
