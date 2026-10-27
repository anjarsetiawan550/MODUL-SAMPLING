// StandaloneApp.cpp
// Aplikasi standalone kustom untuk LR Module Sampler.
// Tujuan: membuka perangkat audio dengan buffer kecil + mode Windows low latency
// secara default, tanpa perlu mengatur manual setiap kali.
//
// Hanya dikompilasi ke target SampleCore_Standalone (lihat CMakeLists.txt).

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace lrsc
{
    constexpr int         kTargetBuffer = 128;            // sampel; bisa diganti lewat menu Audio
    constexpr const char* kStateKey     = "audioSetupLL2"; // key baru, supaya setting lama (tanpa ASIO / buffer besar) tidak terbaca

    // Pilih driver ASIO yang paling masuk akal: Maono dulu, lalu driver non-generik, lalu yang pertama.
    static juce::String pickPreferredDevice (juce::AudioIODeviceType* type)
    {
        const auto names = type->getDeviceNames (false);

        for (const auto& n : names)
            if (n.containsIgnoreCase ("Maono"))
                return n;

        for (const auto& n : names)
            if (! (n.containsIgnoreCase ("ASIO4ALL") || n.containsIgnoreCase ("FL Studio")
                   || n.containsIgnoreCase ("Generic") || n.containsIgnoreCase ("Voicemeeter")
                   || n.containsIgnoreCase ("Virtual")))
                return n;

        return names.isEmpty() ? juce::String() : names[0];
    }

    //==========================================================================
    struct MenuModel final : public juce::MenuBarModel
    {
        std::function<juce::PopupMenu()> getMenu;
        std::function<void (int)> onItem;

        juce::StringArray getMenuBarNames() override { return { "Audio" }; }

        juce::PopupMenu getMenuForIndex (int, const juce::String&) override
        {
            return getMenu ? getMenu() : juce::PopupMenu();
        }

        void menuItemSelected (int id, int) override
        {
            if (onItem) onItem (id);
        }
    };

    //==========================================================================
    class StandaloneWindow final : public juce::DocumentWindow,
                                   private juce::ChangeListener
    {
    public:
        StandaloneWindow()
            : juce::DocumentWindow ("LR Module Sampler",
                                    juce::Colours::black,
                                    juce::DocumentWindow::minimiseButton | juce::DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar (true);

            juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Standalone);
            processor.reset (createPluginFilter());
            juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Undefined);

            setupAudio();

            player.setProcessor (processor.get());
            deviceManager.addAudioCallback (&player);
            deviceManager.addChangeListener (this);

            menuModel.getMenu = [this] { return buildMenu(); };
            menuModel.onItem  = [this] (int id) { handleMenu (id); };

            setContentOwned (processor->createEditorIfNeeded(), true);
            setResizable (false, false);

           #if JUCE_MAC
            juce::MenuBarModel::setMacMainMenu (&menuModel);
           #else
            setMenuBar (&menuModel);
           #endif

            centreWithSize (getWidth(), getHeight());
            setVisible (true);
            updateTitle();
        }

        ~StandaloneWindow() override
        {
           #if JUCE_MAC
            juce::MenuBarModel::setMacMainMenu (nullptr);
           #else
            setMenuBar (nullptr);
           #endif

            deviceManager.removeChangeListener (this);
            deviceManager.removeAudioCallback (&player);
            player.setProcessor (nullptr);
            clearContentComponent();
            persistState();
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplicationBase::quit();
        }

    private:
        //======================================================================
        void setupAudio()
        {
            juce::PropertiesFile::Options o;
            o.applicationName     = "LR Module Sampler LL";
            o.filenameSuffix      = ".settings";
            o.osxLibrarySubFolder = "Application Support";
            o.folderName          = "";
            o.commonToAllUsers    = false;
            props.setStorageParameters (o);

            std::unique_ptr<juce::XmlElement> saved;
            if (auto* us = props.getUserSettings())
                saved = us->getXmlValue (kStateKey);

            juce::AudioDeviceManager::AudioDeviceSetup preferred;
            preferred.bufferSize = kTargetBuffer;
            preferred.sampleRate = 0;   // pakai sample rate bawaan perangkat

            [[maybe_unused]] const auto err = deviceManager.initialise (2, 2, saved.get(), true, {}, &preferred);

            // Pertama kali dijalankan (belum ada setting tersimpan): coba ASIO, lalu mode Windows low latency.
            if (saved == nullptr)
                applyLowLatencyDefaults();

            persistState();
        }

        void applyLowLatencyDefaults()
        {
            const auto original = deviceManager.getCurrentAudioDeviceType();

            for (const char* key : { "ASIO", "Low Latency", "Exclusive" })
            {
                for (auto* type : deviceManager.getAvailableDeviceTypes())
                {
                    const auto name = type->getTypeName();
                    if (! name.containsIgnoreCase (key))
                        continue;

                    deviceManager.setCurrentAudioDeviceType (name, true);

                    auto s = deviceManager.getAudioDeviceSetup();

                    if (name.equalsIgnoreCase ("ASIO"))
                    {
                        const auto dev = pickPreferredDevice (type);
                        if (dev.isNotEmpty())
                        {
                            s.outputDeviceName = dev;
                            s.inputDeviceName  = dev;
                            s.useDefaultInputChannels  = true;
                            s.useDefaultOutputChannels = true;
                        }
                    }

                    s.bufferSize = kTargetBuffer;
                    const auto err = deviceManager.setAudioDeviceSetup (s, true);

                    if (err.isEmpty() && deviceManager.getCurrentAudioDevice() != nullptr)
                        return;
                }
            }

            // Tidak ada mode low latency yang berhasil: kembali ke tipe awal, tetap dengan buffer kecil.
            deviceManager.setCurrentAudioDeviceType (original, true);
            auto s = deviceManager.getAudioDeviceSetup();
            s.bufferSize = kTargetBuffer;
            deviceManager.setAudioDeviceSetup (s, true);
        }

        void persistState()
        {
            if (auto xml = deviceManager.createStateXml())
                if (auto* us = props.getUserSettings())
                {
                    us->setValue (kStateKey, xml.get());
                    props.saveIfNeeded();
                }
        }

        void changeListenerCallback (juce::ChangeBroadcaster*) override
        {
            persistState();
            updateTitle();
        }

        void updateTitle()
        {
            juce::String title ("LR Module Sampler");

            if (auto* d = deviceManager.getCurrentAudioDevice())
            {
                const double sr  = d->getCurrentSampleRate();
                const int    buf = d->getCurrentBufferSizeSamples();

                if (sr > 0.0)
                {
                    const double bufMs = 1000.0 * buf / sr;
                    const double devMs = 1000.0 * (d->getInputLatencyInSamples() + d->getOutputLatencyInSamples()) / sr;

                    title << "  |  " << d->getTypeName()
                          << "  |  buffer " << buf << " smp (" << juce::String (bufMs, 1) << " ms)"
                          << "  |  in+out ~" << juce::String (devMs, 1) << " ms";
                }
            }
            else
            {
                title << "  |  tidak ada perangkat audio";
            }

            setName (title);
        }

        //======================================================================
        juce::PopupMenu buildMenu()
        {
            int cur = 0;
            if (auto* d = deviceManager.getCurrentAudioDevice())
                cur = d->getCurrentBufferSizeSamples();

            juce::PopupMenu m;
            m.addItem (1, "Pengaturan Audio...");
            m.addSeparator();
            m.addItem (64,  "Buffer 64 (paling rendah)", true, cur == 64);
            m.addItem (128, "Buffer 128",                true, cur == 128);
            m.addItem (256, "Buffer 256",                true, cur == 256);
            m.addItem (512, "Buffer 512 (aman)",         true, cur == 512);
            return m;
        }

        void handleMenu (int id)
        {
            if (id == 1) showSettings();
            else if (id >= 32) setBuffer (id);
        }

        void setBuffer (int n)
        {
            const auto old = deviceManager.getAudioDeviceSetup();
            auto s = old;
            s.bufferSize = n;

            const auto err = deviceManager.setAudioDeviceSetup (s, true);
            if (err.isNotEmpty())
            {
                deviceManager.setAudioDeviceSetup (old, true);   // kembalikan setting sebelumnya
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                        "Buffer tidak didukung", err);
            }
        }

        void showSettings()
        {
            auto* sel = new juce::AudioDeviceSelectorComponent (deviceManager, 0, 2, 0, 2,
                                                                false, false, true, false);
            sel->setSize (500, 420);

            juce::DialogWindow::LaunchOptions o;
            o.content.setOwned (sel);
            o.dialogTitle = "Pengaturan Audio";
            o.componentToCentreAround = this;
            o.dialogBackgroundColour = juce::Colour (0xff202020);
            o.escapeKeyTriggersCloseButton = true;
            o.useNativeTitleBar = true;
            o.resizable = false;
            o.launchAsync();
        }

        //======================================================================
        juce::ApplicationProperties props;
        std::unique_ptr<juce::AudioProcessor> processor;
        juce::AudioDeviceManager deviceManager;
        juce::AudioProcessorPlayer player;
        MenuModel menuModel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StandaloneWindow)
    };

    //==========================================================================
    class StandaloneApp final : public juce::JUCEApplication
    {
    public:
        const juce::String getApplicationName() override    { return "LR Module Sampler"; }
        const juce::String getApplicationVersion() override { return "2.1.0"; }
        bool moreThanOneInstanceAllowed() override          { return true; }

        void initialise (const juce::String&) override      { window = std::make_unique<StandaloneWindow>(); }
        void shutdown() override                            { window = nullptr; }
        void systemRequestedQuit() override                 { quit(); }

    private:
        std::unique_ptr<StandaloneWindow> window;
    };
}

START_JUCE_APPLICATION (lrsc::StandaloneApp)
