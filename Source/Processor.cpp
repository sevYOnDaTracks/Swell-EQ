#include "Processor.h"
#include "Editor.h"

namespace swelleq {
using namespace juce;

namespace {
constexpr float defaultFreqs[maxBands] { 60.0f, 150.0f, 400.0f, 1000.0f, 2500.0f, 6000.0f, 12000.0f, 16000.0f };

NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
             [] (float a, float b, float t) { return a * std::pow (b / a, t); },
             [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
             [] (float a, float b, float v) { return jlimit (a, b, v); } };
}
} // namespace

String ids::band (int i, const char* what) { return "b" + String (i + 1) + "_" + what; }

StringArray typeNames() { return { "Cloche", "Plateau grave", "Plateau aigu", "Coupe-bas", "Coupe-haut", "Coupe-bande" }; }
StringArray slopeNames() { return { "12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" }; }

String formatFreq (float hz)
{
    if (hz >= 1000.0f)
    {
        const float k = hz / 1000.0f;
        return String (k, k >= 10.0f ? 1 : 2).trimCharactersAtEnd ("0").trimCharactersAtEnd (".").replace (".", ",") + " kHz";
    }
    return String (roundToInt (hz)) + " Hz";
}

String formatGain (float db)
{
    const float r = std::round (db * 10.0f) / 10.0f;
    return (r > 0.0f ? "+" : "") + String (r == 0.0f ? 0.0f : r, 1).replace (".", ",") + " dB";
}

AudioProcessorValueTreeState::ParameterLayout Processor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < maxBands; ++i)
    {
        const String n = "Bande " + String (i + 1) + " ";
        auto group = std::make_unique<AudioProcessorParameterGroup> ("band" + String (i + 1), "Bande " + String (i + 1), "|");
        group->addChild (std::make_unique<AudioParameterBool> (ParameterID { ids::band (i, "on"), 1 }, n + "active", false));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ids::band (i, "type"), 1 }, n + "type", typeNames(), 0));
        group->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::band (i, "freq"), 1 }, n + String::fromUTF8 ("fréquence"),
            logRange (20.0f, 20000.0f), defaultFreqs[i],
            AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([] (float v, int) { return formatFreq (v); })));
        group->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::band (i, "gain"), 1 }, n + "gain",
            NormalisableRange<float> (-18.0f, 18.0f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return formatGain (v); })));
        group->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::band (i, "q"), 1 }, n + "Q",
            logRange (0.1f, 18.0f), (float) butterworthQ,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2).replace (".", ","); })));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ids::band (i, "slope"), 1 }, n + "pente", slopeNames(), 1));
        layout.add (std::move (group));
    }
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::output, 1 }, "Sortie", NormalisableRange<float> (-18.0f, 18.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return formatGain (v); })));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::bypass, 1 }, "Bypass", false));
    return layout;
}

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "SwellEQ", createLayout())
{
    for (int i = 0; i < maxBands; ++i)
    {
        auto& b = bandParams[(size_t) i];
        b.on = params.getRawParameterValue (ids::band (i, "on"));
        b.type = params.getRawParameterValue (ids::band (i, "type"));
        b.freq = params.getRawParameterValue (ids::band (i, "freq"));
        b.gain = params.getRawParameterValue (ids::band (i, "gain"));
        b.q = params.getRawParameterValue (ids::band (i, "q"));
        b.slope = params.getRawParameterValue (ids::band (i, "slope"));
    }
    outputParam = params.getRawParameterValue (ids::output);
    bypassParam = params.getRawParameterValue (ids::bypass);
}

bool Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    return in == out && (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo());
}

BandSettings Processor::band (int i) const
{
    const auto& p = bandParams[(size_t) i];
    BandSettings s;
    s.on = p.on->load() > 0.5f;
    s.type = (BandType) jlimit (0, numBandTypes - 1, roundToInt (p.type->load()));
    s.freq = p.freq->load();
    s.gain = p.gain->load();
    s.q = p.q->load();
    s.slope = jlimit (0, 3, roundToInt (p.slope->load()));
    return s;
}

void Processor::pushSettings()
{
    for (int i = 0; i < maxBands; ++i) engine.setBand (i, band (i));
    engine.setOutputDb (outputParam->load());
    engine.setBypass (bypassParam->load() > 0.5f);
    engine.setSolo (soloBand.load());
}

void Processor::prepareToPlay (double sampleRate, int blockSize)
{
    pushSettings();
    engine.prepare (sampleRate, jmax (1, getTotalNumOutputChannels()));
    monoScratch.setSize (1, jmax (1, blockSize), false, false, true);
    setLatencySamples (0);
}

void Processor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nc = jmin (buffer.getNumChannels(), Engine::maxChannels);
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    if (! licence->isUnlocked())
    {
        buffer.clear();
        outputPeak.store (0.0f);
        return;
    }

    pushSettings();
    const bool analyse = analyserWanted.load (std::memory_order_relaxed);
    if (monoScratch.getNumSamples() < n) monoScratch.setSize (1, n, false, false, true);   // hôte qui dépasse la taille annoncée
    auto toMono = [&] (SampleFifo& fifo) {
        auto* m = monoScratch.getWritePointer (0);
        FloatVectorOperations::copy (m, buffer.getReadPointer (0), n);
        if (nc > 1) { FloatVectorOperations::add (m, buffer.getReadPointer (1), n); FloatVectorOperations::multiply (m, 0.5f, n); }
        fifo.push (m, n);
    };
    if (analyse) toMono (preFifo);

    engine.process (buffer.getArrayOfWritePointers(), nc, n);

    if (analyse) toMono (postFifo);
    float peak = 0.0f;
    for (int c = 0; c < nc; ++c) peak = jmax (peak, buffer.getMagnitude (c, 0, n));
    outputPeak.store (jmax (peak, outputPeak.load() * 0.9f));
}

// ------------------------------------------------------------------ bandes

void Processor::setPlain (const String& id, float value)
{
    if (auto* p = params.getParameter (id))
    {
        const float norm = p->convertTo0to1 (value);
        if (std::abs (norm - p->getValue()) < 1.0e-7f) return;
        p->beginChangeGesture();
        p->setValueNotifyingHost (norm);
        p->endChangeGesture();
    }
}

void Processor::setBandValue (int i, const char* what, float v) { setPlain (ids::band (i, what), v); }

void Processor::setBand (int i, const BandSettings& s)
{
    setBandValue (i, "type", (float) (int) s.type);
    setBandValue (i, "slope", (float) s.slope);
    setBandValue (i, "freq", s.freq);
    setBandValue (i, "gain", s.gain);
    setBandValue (i, "q", s.q);
    setBandValue (i, "on", s.on ? 1.0f : 0.0f);
}

int Processor::addBand (BandType type, float freq, float gain, float q)
{
    for (int i = 0; i < maxBands; ++i)
        if (! band (i).on)
        {
            BandSettings s;
            s.on = true; s.type = type; s.freq = freq; s.gain = hasGain (type) ? gain : 0.0f; s.q = q; s.slope = 1;
            setBand (i, s);
            return i;
        }
    return -1;
}

void Processor::removeBand (int i)
{
    if (soloBand.load() == i) soloBand.store (-1);
    setBandValue (i, "on", 0.0f);
}

double Processor::bandResponse (int i, double f) const
{
    const double fs = getSampleRate() > 0 ? getSampleRate() : 48000.0;
    return bandResponseDb (band (i), f, fs);
}

double Processor::responseDb (double f) const
{
    double db = outputParam->load();
    for (int i = 0; i < maxBands; ++i) db += bandResponse (i, f);
    return db;
}

// ------------------------------------------------------------------ presets

String Processor::currentPresetCategory() const { return presetCategory; }

const String Processor::getProgramName (int i)
{
    const auto& bank = factoryPresets();
    return isPositiveAndBelow (i, (int) bank.size()) ? String (CharPointer_UTF8 (bank[(size_t) i].name)) : String();
}

void Processor::loadFactoryPreset (int index)
{
    const auto& bank = factoryPresets();
    if (! isPositiveAndBelow (index, (int) bank.size())) return;
    const auto& p = bank[(size_t) index];
    soloBand.store (-1);
    for (int i = 0; i < maxBands; ++i)
    {
        BandSettings s;
        if (i < (int) p.bands.size())
        {
            const auto& b = p.bands[(size_t) i];
            s.on = true; s.type = b.type; s.freq = b.freq; s.gain = b.gain; s.q = b.q; s.slope = b.slope;
        }
        else
        {
            s.freq = defaultFreqs[i];
        }
        setBand (i, s);
    }
    setPlain (ids::output, p.output);
    currentPreset = index;
    presetName = String (CharPointer_UTF8 (p.name));
    presetCategory = String (CharPointer_UTF8 (p.category));
}

void Processor::stepPreset (int delta)
{
    const int count = (int) factoryPresets().size();
    const int from = currentPreset >= 0 ? currentPreset : 0;
    loadFactoryPreset (((from + delta) % count + count) % count);
}

File Processor::userPresetFolder()
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Swell Audio").getChildFile ("Swell EQ Presets");
}

Array<File> Processor::userPresets() const
{
    auto files = userPresetFolder().findChildFiles (File::findFiles, false, "*.swellpreset");
    files.sort();
    return files;
}

bool Processor::saveUserPreset (const String& rawName)
{
    const auto name = File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return false;
    auto state = params.copyState();
    std::unique_ptr<XmlElement> xml (state.createXml());
    if (xml == nullptr) return false;
    xml->setAttribute ("presetName", name);
    auto file = userPresetFolder().getChildFile (name + ".swellpreset");
    if (! userPresetFolder().createDirectory() || ! xml->writeTo (file)) return false;
    currentPreset = -1;
    presetName = name;
    presetCategory = "Mes presets";
    return true;
}

bool Processor::loadUserPreset (const File& file)
{
    auto xml = parseXML (file);
    if (xml == nullptr || ! xml->hasTagName (params.state.getType())) return false;
    soloBand.store (-1);
    // Paramètre par paramètre (et non replaceState) : l'hôte voit chaque changement, « annuler » fonctionne.
    auto tree = ValueTree::fromXml (*xml);
    for (auto* p : getParameters())
        if (auto* withId = dynamic_cast<RangedAudioParameter*> (p))
        {
            auto child = tree.getChildWithProperty ("id", withId->getParameterID());
            if (child.isValid()) setPlain (withId->getParameterID(), (float) child.getProperty ("value"));
        }
    currentPreset = -1;
    presetName = file.getFileNameWithoutExtension();
    presetCategory = "Mes presets";
    return true;
}

// ------------------------------------------------------------------ état

void Processor::getStateInformation (MemoryBlock& dest)
{
    auto state = params.copyState();
    state.setProperty ("presetIndex", currentPreset, nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("presetCategory", presetCategory, nullptr);
    state.setProperty ("analyserMode", analyserMode, nullptr);
    state.setProperty ("scale", displayRange, nullptr);
    state.setProperty ("showResonances", showResonances, nullptr);
    state.setProperty ("editorWidth", editorWidth, nullptr);
    state.setProperty ("editorHeight", editorHeight, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (params.state.getType()))
        {
            auto tree = ValueTree::fromXml (*xml);
            currentPreset = tree.getProperty ("presetIndex", -1);
            presetName = tree.getProperty ("presetName", "Neutre").toString();
            presetCategory = tree.getProperty ("presetCategory", "").toString();
            analyserMode = jlimit (0, 2, (int) tree.getProperty ("analyserMode", 0));
            displayRange = jlimit (6, 18, (int) tree.getProperty ("scale", 18));
            showResonances = tree.getProperty ("showResonances", false);
            editorWidth = tree.getProperty ("editorWidth", 0);
            editorHeight = tree.getProperty ("editorHeight", 0);
            params.replaceState (tree);
        }
}

AudioProcessorEditor* Processor::createEditor() { return swell::LicenceGuard::attach (new Editor (*this)); }

} // namespace swelleq

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new swelleq::Processor(); }
