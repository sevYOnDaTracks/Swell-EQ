#pragma once
#include "EqDsp.h"
#include "Presets.h"
#include "Analyzer.h"
#include "Licence/SwellLicence.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace swelleq {

namespace ids {
    juce::String band (int i, const char* what);     // "b1_freq" ...
    inline const juce::String output { "output" }, bypass { "bypass" };
}

juce::StringArray typeNames();     // « Cloche », « Plateau grave »...
juce::StringArray slopeNames();    // « 12 dB/oct »...
juce::String formatFreq (float hz);
juce::String formatGain (float db);

class Processor final : public juce::AudioProcessor
{
public:
    Processor();
    ~Processor() override = default;

    juce::AudioProcessorValueTreeState params;
    juce::SharedResourcePointer<swell::Licence> licence;

    // ---- Analyseur (le thread audio pousse, l'interface lit)
    SampleFifo preFifo, postFifo;
    std::atomic<bool> analyserWanted { false };   // l'éditeur est ouvert
    std::atomic<float> outputPeak { 0.0f };
    std::atomic<int> soloBand { -1 };              // non sauvegardé : on n'ouvre jamais un projet en solo

    // ---- Préférences d'affichage (sauvegardées avec le projet)
    int analyserMode = 0;          // 0 = avant + après, 1 = après seulement, 2 = désactivé
    int displayRange = 12;         // échelle de la courbe : ±6, ±12 ou ±24 dB
    bool showResonances = false;
    int editorWidth = 0, editorHeight = 0;

    // ---- Bandes (thread des messages)
    BandSettings band (int i) const;
    void setBand (int i, const BandSettings&);            // envoie à l'hôte (automation, annuler)
    void setBandValue (int i, const char* what, float plainValue);
    int addBand (BandType type, float freq, float gain, float q);   // -1 si les 8 bandes sont prises
    void removeBand (int i);
    double responseDb (double freq) const;                 // courbe totale demandée
    double bandResponse (int i, double freq) const;

    // ---- Presets
    void loadFactoryPreset (int index);
    int currentFactoryPreset() const { return currentPreset; }
    juce::String currentPresetName() const { return presetName; }
    juce::String currentPresetCategory() const;
    void stepPreset (int delta);
    static juce::File userPresetFolder();
    juce::Array<juce::File> userPresets() const;
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File&);
    void resetFlat() { loadFactoryPreset (0); }

    // ---- AudioProcessor
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }
    int getNumPrograms() override { return (int) factoryPresets().size(); }
    int getCurrentProgram() override { return juce::jmax (0, currentPreset); }
    void setCurrentProgram (int i) override { loadFactoryPreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return params.getParameter (ids::bypass); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void pushSettings();
    void setPlain (const juce::String& id, float value);

    Engine engine;
    juce::AudioBuffer<float> monoScratch;
    struct BandParams { std::atomic<float>* on, *type, *freq, *gain, *q, *slope; };
    std::array<BandParams, maxBands> bandParams {};
    std::atomic<float>* outputParam = nullptr, *bypassParam = nullptr;
    int currentPreset = 0;
    juce::String presetName { "Neutre" }, presetCategory;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

} // namespace swelleq
