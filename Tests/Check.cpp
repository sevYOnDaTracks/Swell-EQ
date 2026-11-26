// Tests hors ligne de Swell EQ.
//   EQCheck <dossier de sortie> [fichier audio pour les captures]
// Vérifie : silence sans licence, réponse en fréquence mesurée au sinus (contre la courbe affichée et contre la théorie),
// absence de clics quand on bouge les réglages, latence nulle, presets, état, détection des résonances.
// Écrit aussi des captures PNG de l'interface.
#include "Processor.h"
#include "Editor.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace juce;
using namespace swelleq;

static int failures = 0;
static void check (bool ok, const String& what)
{
    std::cout << (ok ? "  ok    " : "  ÉCHEC ") << what << std::endl;
    if (! ok) ++failures;
}

static constexpr double fs = 48000.0;

// ---------------------------------------------------------------- mesure au sinus

static double measureDb (const std::vector<BandSettings>& bands, double freq)
{
    Engine e;
    for (int i = 0; i < (int) bands.size(); ++i) e.setBand (i, bands[(size_t) i]);
    e.prepare (fs, 1);
    const int total = (int) (fs * 0.8), tail = (int) (fs * 0.3);
    std::vector<float> buf ((size_t) total);
    for (int n = 0; n < total; ++n) buf[(size_t) n] = (float) (0.25 * std::sin (2.0 * MathConstants<double>::pi * freq * n / fs));
    float* ch[1] { buf.data() };
    e.process (ch, 1, total);
    double sum = 0.0;
    for (int n = total - tail; n < total; ++n) sum += (double) buf[(size_t) n] * buf[(size_t) n];
    const double rms = std::sqrt (sum / tail), inRms = 0.25 / std::sqrt (2.0);
    return 20.0 * std::log10 (rms / inRms);
}

static BandSettings make (BandType t, float f, float g, float q, int slope = 1)
{
    BandSettings b; b.on = true; b.type = t; b.freq = f; b.gain = g; b.q = q; b.slope = slope;
    return b;
}

static void testFrequencyResponse()
{
    std::cout << "\nRéponse en fréquence (sinus mesurés à 48 kHz)" << std::endl;

    struct Expect { double freq, db, tol; };
    struct Case { String name; BandSettings band; std::vector<Expect> expect; };
    const std::vector<Case> cases {
        { "Cloche 1 kHz +6 dB Q 1", make (BandType::bell, 1000, 6, 1), { { 1000, 6.0, 0.05 }, { 60, 0.0, 0.15 }, { 15000, 0.0, 0.2 } } },
        { "Cloche 3 kHz -12 dB Q 4", make (BandType::bell, 3000, -12, 4), { { 3000, -12.0, 0.05 }, { 1000, 0.0, 0.3 } } },
        { "Plateau grave 200 Hz +6 dB", make (BandType::lowShelf, 200, 6, 0.7071f), { { 25, 6.0, 0.2 }, { 200, 3.0, 0.1 }, { 5000, 0.0, 0.1 } } },
        { "Plateau aigu 5 kHz -6 dB", make (BandType::highShelf, 5000, -6, 0.7071f), { { 5000, -3.0, 0.1 }, { 300, 0.0, 0.1 }, { 18000, -6.0, 0.4 } } },
        { "Coupe-bas 100 Hz 12 dB/oct", make (BandType::lowCut, 100, 0, 0.7071f, 0), { { 100, -3.01, 0.05 }, { 50, -12.30, 0.3 }, { 1000, 0.0, 0.05 } } },
        { "Coupe-bas 100 Hz 24 dB/oct", make (BandType::lowCut, 100, 0, 0.7071f, 1), { { 100, -3.01, 0.05 }, { 50, -24.10, 0.3 }, { 1000, 0.0, 0.05 } } },
        { "Coupe-bas 100 Hz 36 dB/oct", make (BandType::lowCut, 100, 0, 0.7071f, 2), { { 100, -3.01, 0.05 }, { 50, -36.12, 0.4 } } },
        { "Coupe-bas 100 Hz 48 dB/oct", make (BandType::lowCut, 100, 0, 0.7071f, 3), { { 100, -3.01, 0.05 }, { 50, -48.16, 0.6 } } },
        { "Coupe-haut 5 kHz 24 dB/oct", make (BandType::highCut, 5000, 0, 0.7071f, 1), { { 5000, -3.01, 0.05 }, { 500, 0.0, 0.05 }, { 10000, -28.3, 0.5 } } },   // transformée bilinéaire : plus raide qu'en analogique (-24 dB) près de Nyquist
        { "Coupe-bande 50 Hz Q 12", make (BandType::notch, 50, 0, 12), { { 50, -60.0, 30.0 }, { 200, 0.0, 0.1 } } },
    };

    for (auto& c : cases)
    {
        bool designOk = true, curveOk = true;
        double worstCurve = 0.0;
        String detail;
        for (auto& ex : c.expect)
        {
            const double m = measureDb ({ c.band }, ex.freq);
            if (std::abs (m - ex.db) > ex.tol) { designOk = false; detail << "  [" << ex.freq << " Hz : " << String (m, 2) << " dB, attendu " << ex.db << "]"; }
        }
        // La courbe affichée doit suivre le son à 0,1 dB près, sur toute la bande audio.
        for (double f = 25.0; f < 20000.0; f *= 1.6)
        {
            const double m = measureDb ({ c.band }, f), a = bandResponseDb (c.band, f, fs);
            if (a > -40.0) { worstCurve = jmax (worstCurve, std::abs (m - a)); if (std::abs (m - a) > 0.1) curveOk = false; }
        }
        check (designOk, c.name + " : valeurs attendues" + detail);
        check (curveOk, c.name + String::fromUTF8 (" : courbe affichée = son mesuré (écart max ") + String (worstCurve, 3) + " dB)");
    }

    // Plusieurs bandes : les effets s'additionnent.
    const std::vector<BandSettings> combo { make (BandType::bell, 100, 4, 1), make (BandType::bell, 4000, -3, 2) };
    const double m = measureDb (combo, 4000), a = bandResponseDb (combo[0], 4000, fs) + bandResponseDb (combo[1], 4000, fs);
    check (std::abs (m - a) < 0.05, "Deux bandes : la somme des courbes = le son (" + String (m, 2) + " / " + String (a, 2) + " dB)");
}

// ---------------------------------------------------------------- clics

static void setParam (Processor& p, const String& id, float plain)
{
    auto* prm = p.params.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

// Rend 120 blocs d'un sinus à 220 Hz ; 'change' est appelé avant le bloc 'at'. Un clic est large bande : on mesure
// ce qui apparaît au-dessus de 3 kHz (passe-haut 48 dB/oct), en dB sous la crête du signal, après le changement.
static double clickScore (Processor& p, int at, std::function<void()> change, double* floorDb = nullptr)
{
    constexpr int block = 256, blocks = 120;
    AudioBuffer<float> buf (2, block);
    MidiBuffer midi;
    std::vector<float> out;
    double phase = 0.0;
    for (int b = 0; b < blocks; ++b)
    {
        if (b == at) change();
        for (int n = 0; n < block; ++n)
        {
            const float v = (float) (0.3 * std::sin (phase));
            phase += 2.0 * MathConstants<double>::pi * 220.0 / fs;
            buf.setSample (0, n, v); buf.setSample (1, n, v);
        }
        p.processBlock (buf, midi);
        for (int n = 0; n < block; ++n) out.push_back (buf.getSample (0, n));
    }
    std::vector<float> hp (out);
    Engine filter;
    filter.setBand (0, make (BandType::lowCut, 3000, 0, 0.7071f, 3));
    filter.prepare (fs, 1);
    float* ch[1] { hp.data() };
    filter.process (ch, 1, (int) hp.size());

    double peak = 1.0e-9, before = 1.0e-12, after = 1.0e-12;
    for (size_t n = (size_t) (20 * block); n < out.size(); ++n)
    {
        peak = jmax (peak, (double) std::abs (out[n]));
        if (n < (size_t) (at * block)) before = jmax (before, (double) std::abs (hp[n]));
        else after = jmax (after, (double) std::abs (hp[n]));
    }
    if (floorDb != nullptr) *floorDb = 20.0 * std::log10 (before / peak);
    return 20.0 * std::log10 (after / peak);
}

static void testClicks()
{
    std::cout << "\nClics quand on bouge les réglages (sinus 220 Hz, blocs de 256)" << std::endl;
    struct Scenario { String name; std::function<void (Processor&)> setup, change; };
    const std::vector<Scenario> scenarios {
        { String::fromUTF8 ("Fréquence 200 Hz -> 5 kHz d'un coup, gain +12 dB"),
          [] (Processor& p) { p.setBand (0, make (BandType::bell, 200, 12, 2)); },
          [] (Processor& p) { setParam (p, ids::band (0, "freq"), 5000); } },
        { "Gain -18 -> +18 dB d'un coup",
          [] (Processor& p) { p.setBand (0, make (BandType::bell, 250, -18, 1)); },
          [] (Processor& p) { setParam (p, ids::band (0, "gain"), 18); } },
        { "Q 0,3 -> 18 d'un coup",
          [] (Processor& p) { p.setBand (0, make (BandType::bell, 220, 10, 0.3f)); },
          [] (Processor& p) { setParam (p, ids::band (0, "q"), 18); } },
        { "Allumer une cloche +12 dB",
          [] (Processor& p) { auto b = make (BandType::bell, 220, 12, 1); b.on = false; p.setBand (0, b); },
          [] (Processor& p) { setParam (p, ids::band (0, "on"), 1); } },
        { String::fromUTF8 ("Éteindre un coupe-bas 48 dB/oct"),
          [] (Processor& p) { p.setBand (0, make (BandType::lowCut, 300, 0, 0.7071f, 3)); },
          [] (Processor& p) { setParam (p, ids::band (0, "on"), 0); } },
        { "Changer de type : cloche -> plateau aigu",
          [] (Processor& p) { p.setBand (0, make (BandType::bell, 220, 9, 1)); },
          [] (Processor& p) { setParam (p, ids::band (0, "type"), (float) (int) BandType::highShelf); } },
        { "Changer de pente : 12 -> 48 dB/oct",
          [] (Processor& p) { p.setBand (0, make (BandType::lowCut, 200, 0, 0.7071f, 0)); },
          [] (Processor& p) { setParam (p, ids::band (0, "slope"), 3); } },
        { "Solo d'une bande",
          [] (Processor& p) { p.setBand (0, make (BandType::bell, 1000, 6, 1)); },
          [] (Processor& p) { p.soloBand.store (0); } },
        { "Bypass",
          [] (Processor& p) { p.setBand (0, make (BandType::bell, 220, 12, 1)); },
          [] (Processor& p) { setParam (p, ids::bypass, 1); } },
        { "Sortie -18 -> +12 dB d'un coup",
          [] (Processor& p) { setParam (p, ids::output, -18); },
          [] (Processor& p) { setParam (p, ids::output, 12); } },
        { String::fromUTF8 ("Changer de preset (Rap · présence -> Master · club)"),
          [] (Processor& p) { p.loadFactoryPreset (1); },
          [] (Processor& p) { p.loadFactoryPreset ((int) factoryPresets().size() - 4); } },
    };

    // Témoin : un vrai saut de signal doit être détecté.
    {
        Processor p;
        p.prepareToPlay (fs, 256);
        const double score = clickScore (p, 60, [&p] { setParam (p, ids::output, 12); p.prepareToPlay (fs, 256); });
        check (score > -40.0, String::fromUTF8 ("Témoin : un vrai saut est bien détecté (") + String (score, 1) + " dB au-dessus de 3 kHz)");
    }

    for (auto& s : scenarios)
    {
        Processor p;
        s.setup (p);
        p.prepareToPlay (fs, 256);
        double floor = 0.0;
        const double score = clickScore (p, 60, [&] { s.change (p); }, &floor);
        // Un clic audible laisse au moins -50 dB de bruit large bande ; on exige -70 dB.
        check (score < -70.0, s.name + " (" + String (score, 1) + " dB au-dessus de 3 kHz, avant : " + String (floor, 1) + " dB)");
    }
}

// ---------------------------------------------------------------- latence, licence, presets, état

static void testLatency()
{
    std::cout << "\nLatence" << std::endl;
    Processor p;
    p.setBand (0, make (BandType::bell, 1000, 12, 1));
    p.setBand (1, make (BandType::lowCut, 80, 0, 0.7071f, 3));
    p.prepareToPlay (fs, 512);
    check (p.getLatencySamples() == 0, String::fromUTF8 ("Latence annoncée à l'hôte : 0 échantillon"));

    AudioBuffer<float> buf (2, 512);
    buf.clear();
    buf.setSample (0, 0, 1.0f); buf.setSample (1, 0, 1.0f);
    MidiBuffer midi;
    p.processBlock (buf, midi);
    int peakAt = 0;
    for (int n = 1; n < 512; ++n) if (std::abs (buf.getSample (0, n)) > std::abs (buf.getSample (0, peakAt))) peakAt = n;
    check (std::abs (buf.getSample (0, 0)) > 0.5f && peakAt == 0, String::fromUTF8 ("Impulsion : la réponse démarre au premier échantillon (pic à ") + String (peakAt) + ")");

    Processor flat;
    flat.prepareToPlay (fs, 512);
    AudioBuffer<float> in (2, 512), out (2, 512);
    Random r (1);
    for (int c = 0; c < 2; ++c) for (int n = 0; n < 512; ++n) in.setSample (c, n, r.nextFloat() * 2.0f - 1.0f);
    out.makeCopyOf (in);
    flat.processBlock (out, midi);
    float diff = 0.0f;
    for (int c = 0; c < 2; ++c) for (int n = 0; n < 512; ++n) diff = jmax (diff, std::abs (out.getSample (c, n) - in.getSample (c, n)));
    check (diff == 0.0f, String::fromUTF8 ("À plat : sortie identique à l'entrée, au bit près"));
}

static void testLocked()
{
    std::cout << "\nLicence" << std::endl;
    Processor p;
    if (p.licence->isUnlocked())
    {
        std::cout << "  (passé : cet ordinateur a une vraie licence Swell EQ activée)" << std::endl;
        return;
    }
    p.prepareToPlay (fs, 256);
    AudioBuffer<float> buf (2, 256);
    for (int n = 0; n < 256; ++n) { buf.setSample (0, n, 0.5f); buf.setSample (1, n, 0.5f); }
    MidiBuffer midi;
    p.processBlock (buf, midi);
    check (buf.getMagnitude (0, 256) == 0.0f, "Sans licence : silence");
}

static void pinkNoise (AudioBuffer<float>& buf, Random& r, double state[7], float gain)
{
    for (int n = 0; n < buf.getNumSamples(); ++n)
    {
        const double w = r.nextFloat() * 2.0 - 1.0;
        state[0] = 0.99886 * state[0] + w * 0.0555179; state[1] = 0.99332 * state[1] + w * 0.0750759;
        state[2] = 0.96900 * state[2] + w * 0.1538520; state[3] = 0.86650 * state[3] + w * 0.3104856;
        state[4] = 0.55000 * state[4] + w * 0.5329522; state[5] = -0.7616 * state[5] - w * 0.0168980;
        const double pink = state[0] + state[1] + state[2] + state[3] + state[4] + state[5] + state[6] + w * 0.5362;
        state[6] = w * 0.115926;
        for (int c = 0; c < buf.getNumChannels(); ++c) buf.setSample (c, n, (float) (pink * 0.11 * gain));
    }
}

static void testPresets()
{
    std::cout << "\nPresets d'usine" << std::endl;
    const auto& bank = factoryPresets();
    bool allOk = true;
    StringArray report;
    for (int i = 0; i < (int) bank.size(); ++i)
    {
        Processor p;
        p.prepareToPlay (fs, 512);
        p.loadFactoryPreset (i);
        AudioBuffer<float> buf (2, 512);
        MidiBuffer midi;
        Random r (7);
        double st[7] {};
        float peak = 0.0f;
        bool finite = true;
        for (int b = 0; b < 200; ++b)
        {
            pinkNoise (buf, r, st, 1.0f);
            p.processBlock (buf, midi);
            for (int n = 0; n < 512; ++n) finite = finite && std::isfinite (buf.getSample (0, n));
            peak = jmax (peak, buf.getMagnitude (0, 512));
        }
        double maxBoost = -100.0;
        for (double f = 20.0; f < 20000.0; f *= 1.05) maxBoost = jmax (maxBoost, p.responseDb (f));
        const bool ok = finite && peak < 4.0f && bank[(size_t) i].bands.size() <= (size_t) maxBands && maxBoost < 8.0
                        && p.currentPresetName() == String (CharPointer_UTF8 (bank[(size_t) i].name));
        allOk = allOk && ok;
        report.add (String (CharPointer_UTF8 (bank[(size_t) i].category)) + " / " + String (CharPointer_UTF8 (bank[(size_t) i].name))
                    + "  (" + String ((int) bank[(size_t) i].bands.size()) + " bandes, bosse max " + String (maxBoost, 1) + " dB)" + (ok ? "" : String::fromUTF8 ("  <- PROBLÈME")));
    }
    for (auto& line : report) std::cout << "        " << line << std::endl;
    check (allOk, String ((int) bank.size()) + " presets : son fini, sans saturation, au plus 8 bandes, boosts raisonnables");

    Processor p;
    check (p.getNumPrograms() == (int) bank.size(), String::fromUTF8 ("Presets visibles comme programmes dans l'hôte"));
}

static void testState()
{
    std::cout << "\nÉtat (sauvegarde dans le projet)" << std::endl;
    Processor a;
    a.loadFactoryPreset (3);
    a.setBand (6, make (BandType::highCut, 9000, 0, 1.2f, 2));
    a.displayRange = 6;
    MemoryBlock mb;
    a.getStateInformation (mb);
    Processor b;
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    bool same = true;
    for (int i = 0; i < maxBands; ++i)
    {
        const auto x = a.band (i), y = b.band (i);
        same = same && x.on == y.on && x.type == y.type && std::abs (x.freq - y.freq) < 0.01f && std::abs (x.gain - y.gain) < 0.001f
               && std::abs (x.q - y.q) < 0.001f && x.slope == y.slope;
    }
    check (same && b.currentPresetName() == a.currentPresetName() && b.displayRange == 6, String::fromUTF8 ("Toutes les bandes, le nom du preset et l'affichage reviennent à l'identique"));

    const auto name = String ("Test EQCheck ") + String (Random::getSystemRandom().nextInt (100000));
    check (a.saveUserPreset (name), "Enregistrer un preset perso");
    Processor c;
    const auto file = Processor::userPresetFolder().getChildFile (name + ".swellpreset");
    check (c.loadUserPreset (file) && c.band (6).type == BandType::highCut && std::abs (c.band (6).freq - 9000.0f) < 0.5f,
           "Recharger le preset perso dans une autre instance");
    file.deleteFile();
}

static void testResonances()
{
    std::cout << "\nDétection des résonances" << std::endl;
    auto run = [] (bool withResonance) {
        Processor p;
        p.prepareToPlay (fs, 512);
        p.analyserWanted.store (true);
        Analyzer an;
        an.setSampleRate (fs);
        AudioBuffer<float> buf (2, 512);
        MidiBuffer midi;
        Random r (3);
        double st[7] {}, phase = 0.0;
        for (int b = 0; b < (int) (4.0 * fs / 512); ++b)
        {
            pinkNoise (buf, r, st, 1.0f);
            if (withResonance)
                for (int n = 0; n < 512; ++n)
                {
                    const float v = (float) (0.02 * std::sin (phase));
                    phase += 2.0 * MathConstants<double>::pi * 2800.0 / fs;
                    buf.addSample (0, n, v); buf.addSample (1, n, v);
                }
            p.processBlock (buf, midi);
            an.update (p.preFifo, p.postFifo, 512.0f / (float) fs);
        }
        return an.resonances();
    };
    const auto clean = run (false);
    check (clean.empty(), String::fromUTF8 ("Bruit rose seul : aucune résonance signalée (") + String ((int) clean.size()) + ")");
    const auto ringing = run (true);
    const bool found = std::any_of (ringing.begin(), ringing.end(), [] (auto& r) { return std::abs (std::log2 (r.freq / 2800.0f)) < 1.0f / 12.0f; });
    String list;
    for (auto& r : ringing) list << formatFreq (r.freq) << " (+" << String (r.excessDb, 1) << " dB, Q " << String (r.q, 1) << ")  ";
    check (found, String::fromUTF8 ("Bruit rose + résonance à 2,8 kHz : repérée -> ") + list);
}

// ---------------------------------------------------------------- captures

static AudioBuffer<float> loadAudio (const File& f)
{
    AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> reader (fm.createReaderFor (f));
    AudioBuffer<float> buf;
    if (reader == nullptr) return buf;
    const int n = (int) jmin<int64> (reader->lengthInSamples, (int64) (reader->sampleRate * 20));
    buf.setSize (2, n);
    reader->read (&buf, 0, n, 0, true, true);
    return buf;
}

static void snapshot (Editor& ed, const File& file)
{
    auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 2.0f);
    file.deleteFile();
    FileOutputStream out (file);
    PNGImageFormat().writeImageToStream (img, out);
    std::cout << "        capture : " << file.getFullPathName() << std::endl;
}

static void captures (const File& dir, const File& audioFile)
{
    std::cout << "\nCaptures de l'interface" << std::endl;
    auto audio = audioFile.existsAsFile() ? loadAudio (audioFile) : AudioBuffer<float>();
    Random r (11);
    double st[7] {};
    int readPos = 0;

    auto feed = [&] (Processor& p, Editor& ed, float seconds, int from = -1) {
        if (from >= 0) readPos = from;
        AudioBuffer<float> buf (2, 512);
        MidiBuffer midi;
        for (int b = 0; b < (int) (seconds * fs / 512); ++b)
        {
            if (audio.getNumSamples() > 0)
            {
                for (int c = 0; c < 2; ++c)
                    for (int n = 0; n < 512; ++n) buf.setSample (c, n, audio.getSample (c, (readPos + n) % audio.getNumSamples()));
                readPos += 512;
            }
            else pinkNoise (buf, r, st, 0.5f);
            p.processBlock (buf, midi);
            ed.pump (512.0f / (float) fs);
        }
    };

    // Moment chanté de la piste (repéré par le niveau) pour un spectre parlant.
    int loud = 0;
    if (audio.getNumSamples() > 0)
    {
        float best = 0.0f;
        for (int s = 0; s + 48000 < audio.getNumSamples(); s += 12000)
        {
            const float rms = audio.getRMSLevel (0, s, 48000);
            if (rms > best) { best = rms; loud = jmax (0, s - 48000); }
        }
    }

    {
        Processor p;
        p.prepareToPlay (fs, 512);
        p.loadFactoryPreset (1);   // Rap · présence
        Editor ed (p);
        ed.setSize (1120, 680);
        feed (p, ed, 2.5f, loud);
        snapshot (ed, dir.getChildFile ("swell-eq-preset.png"));
        ed.getSelection().set (2);
        feed (p, ed, 0.3f);
        snapshot (ed, dir.getChildFile ("swell-eq-bande.png"));
    }
    {
        Processor p;
        p.prepareToPlay (fs, 512);
        p.showResonances = true;
        Editor ed (p);
        ed.setSize (1120, 680);
        feed (p, ed, 5.0f, loud);
        snapshot (ed, dir.getChildFile ("swell-eq-resonances.png"));
    }
    {
        Processor p;
        p.prepareToPlay (fs, 512);
        Editor ed (p);
        ed.setSize (1120, 680);
        feed (p, ed, 1.5f, loud);
        snapshot (ed, dir.getChildFile ("swell-eq-neutre.png"));
        // Fenêtre la plus étroite (FL Studio l'ouvre souvent ainsi) avec le nom de preset le plus long.
        ed.setSize (900, 560);
        p.loadFactoryPreset (17);   // « Batterie · Percussions · brillantes »
        feed (p, ed, 0.3f);
        snapshot (ed, dir.getChildFile ("swell-eq-etroit.png"));
        const auto preset = ed.getPresetBounds(), res = ed.getResonanceBounds();
        check (! preset.intersects (res) && preset.getX() >= 190, String::fromUTF8 ("Fenêtre étroite : la pilule des presets ne touche ni la marque ni Résonances"));
    }
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outDir = argc > 1 ? File::getCurrentWorkingDirectory().getChildFile (argv[1]) : File::getCurrentWorkingDirectory();
    const File audio = argc > 2 ? File::getCurrentWorkingDirectory().getChildFile (argv[2]) : File();
    outDir.createDirectory();

    testLocked();
    swell::Licence::unlockForTesting();
    testFrequencyResponse();
    testClicks();
    testLatency();
    testPresets();
    testState();
    testResonances();
    captures (outDir, audio);

    std::cout << "\n" << (failures == 0 ? "Tout est bon." : String (failures) + " échec(s).") << std::endl;
    return failures == 0 ? 0 : 1;
}
