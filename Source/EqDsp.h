#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

// Moteur de l'égaliseur : filtres en variables d'état trapézoïdaux (topologie « Cytomic » d'Andrew Simper).
//  - Latence nulle (filtres IIR à phase minimale), stables même modulés à chaque échantillon.
//  - Fréquence, gain et Q lissés (2 étages de 10 ms) : aucun clic en tournant un bouton ou en déplaçant un point.
//  - Changer le type, la pente ou allumer/éteindre une bande passe par un fondu en S de 20 ms.
//  - La courbe affichée est la réponse exacte du filtre numérique (même formule que le traitement).
namespace swelleq {

enum class BandType { bell = 0, lowShelf, highShelf, lowCut, highCut, notch };
inline constexpr int numBandTypes = 6;
inline constexpr int maxBands = 8;
inline constexpr int maxSections = 4;     // 48 dB/oct = 4 cellules du 2e ordre
inline constexpr double butterworthQ = 0.70710678118654752;

inline bool isCut (BandType t) noexcept { return t == BandType::lowCut || t == BandType::highCut; }
inline bool hasGain (BandType t) noexcept { return t == BandType::bell || t == BandType::lowShelf || t == BandType::highShelf; }
inline int slopeDbPerOctave (int slope) noexcept { return 12 * (slope + 1); }

struct BandSettings
{
    bool on = false;
    BandType type = BandType::bell;
    float freq = 1000.0f, gain = 0.0f, q = (float) butterworthQ;
    int slope = 1;   // 0 = 12, 1 = 24, 2 = 36, 3 = 48 dB/oct (coupe-bas / coupe-haut)
};

// Une cellule du 2e ordre : H(s) = m0 + (m1·s + m2) / (s² + k·s + 1), transformée bilinéaire préwarpée.
struct Section
{
    double g = 0.1, k = 1.0, m0 = 1.0, m1 = 0.0, m2 = 0.0, a1 = 0.0, a2 = 0.0, a3 = 0.0;

    void finish() noexcept
    {
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    std::complex<double> response (double freq, double fs) const noexcept
    {
        const double w = std::tan (3.14159265358979323846 * std::min (freq, 0.4999 * fs) / fs) / g;
        const std::complex<double> s (0.0, w);
        return m0 + (m1 * s + m2) / (s * s + k * s + 1.0);
    }
};

// Fondu en S : pente nulle au départ et à l'arrivée, donc aucun « coin » audible.
inline double sCurve (float m) noexcept { return 0.5 - 0.5 * std::cos (3.14159265358979323846 * (double) m); }

inline double clampFreq (double f, double fs) noexcept { return std::clamp (f, 10.0, 0.49 * fs); }

// Construit les cellules d'une bande. Renvoie leur nombre.
inline int makeSections (BandType type, double freq, double gainDb, double q, int slope, double fs, Section* out) noexcept
{
    constexpr double pi = 3.14159265358979323846;
    const double t0 = std::tan (pi * clampFreq (freq, fs) / fs);
    const double A = std::pow (10.0, gainDb / 40.0);
    q = std::clamp (q, 0.05, 40.0);

    auto& s = out[0];
    switch (type)
    {
        case BandType::bell:
            s.g = t0; s.k = 1.0 / (q * A); s.m0 = 1.0; s.m1 = s.k * (A * A - 1.0); s.m2 = 0.0;
            s.finish();
            return 1;
        case BandType::lowShelf:
            s.g = t0 / std::sqrt (A); s.k = 1.0 / q; s.m0 = 1.0; s.m1 = s.k * (A - 1.0); s.m2 = A * A - 1.0;
            s.finish();
            return 1;
        case BandType::highShelf:
            s.g = t0 * std::sqrt (A); s.k = 1.0 / q; s.m0 = A * A; s.m1 = s.k * (1.0 - A) * A; s.m2 = 1.0 - A * A;
            s.finish();
            return 1;
        case BandType::notch:
            s.g = t0; s.k = 1.0 / q; s.m0 = 1.0; s.m1 = -s.k; s.m2 = 0.0;
            s.finish();
            return 1;
        case BandType::lowCut:
        case BandType::highCut:
        {
            // Butterworth d'ordre 2n ; le Q de l'utilisateur règle la résonance de la cellule la plus pointue.
            const int n = std::clamp (slope, 0, maxSections - 1) + 1;
            const int order = 2 * n;
            for (int i = 0; i < n; ++i)
            {
                double qi = 1.0 / (2.0 * std::sin ((2.0 * i + 1.0) * pi / (2.0 * order)));
                if (i == 0) qi *= q / butterworthQ;
                auto& c = out[i];
                c.g = t0; c.k = 1.0 / qi;
                if (type == BandType::highCut) { c.m0 = 0.0; c.m1 = 0.0; c.m2 = 1.0; }
                else                            { c.m0 = 1.0; c.m1 = -c.k; c.m2 = -1.0; }
                c.finish();
            }
            return n;
        }
    }
    return 0;
}

// Réponse d'une bande (en dB) à la fréquence f, pour la courbe et les tests.
inline double bandResponseDb (const BandSettings& b, double f, double fs) noexcept
{
    if (! b.on) return 0.0;
    std::array<Section, maxSections> sec;
    const int n = makeSections (b.type, b.freq, b.gain, b.q, b.slope, fs, sec.data());
    std::complex<double> h (1.0, 0.0);
    for (int i = 0; i < n; ++i) h *= sec[(size_t) i].response (f, fs);
    return 20.0 * std::log10 (std::max (std::abs (h), 1.0e-9));
}

// Filtre passe-bande d'écoute (solo) : gain 1 au centre.
inline void makeSoloSection (double freq, double q, double fs, Section& s) noexcept
{
    s.g = std::tan (3.14159265358979323846 * clampFreq (freq, fs) / fs);
    s.k = 1.0 / std::clamp (q, 0.3, 40.0);
    s.m0 = 0.0; s.m1 = s.k; s.m2 = 0.0;
    s.finish();
}

class Engine
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate, int numChannels) noexcept
    {
        fs = sampleRate;
        channels = std::clamp (numChannels, 1, maxChannels);
        smooth = 1.0 - std::exp (-1.0 / (0.010 * fs));           // lissage des réglages : 2 étages de 10 ms
        fadeStep = (float) (1.0 / (0.020 * fs));                   // fondus en S : 20 ms
        reset();
    }

    void reset() noexcept
    {
        for (auto& b : bands)
        {
            b.state = {};
            b.active = b.target.on;
            b.activeType = b.target.type;
            b.activeSlope = b.target.slope;
            b.mix = b.active ? 1.0f : 0.0f;
            b.snap();
            b.rebuild (fs);
        }
        solo.state = {};
        soloMix = soloTarget >= 0 ? 1.0f : 0.0f;
        soloShown = soloTarget;
        if (soloShown >= 0) solo.logF = bands[(size_t) soloShown].logF;
        outGain = outMid = outTarget;
        bypassMix = bypassTarget ? 1.0f : 0.0f;
    }

    void setBand (int i, const BandSettings& s) noexcept
    {
        auto& b = bands[(size_t) i];
        b.target = s;
        b.target.freq = (float) clampFreq (s.freq, fs);
    }
    void setOutputDb (float db) noexcept { outTarget = std::pow (10.0, db / 20.0); }
    void setBypass (bool on) noexcept { bypassTarget = on; }
    void setSolo (int band) noexcept { soloTarget = (band >= 0 && band < maxBands) ? band : -1; }
    double sampleRate() const noexcept { return fs; }

    void process (float* const* data, int numChannels, int numSamples) noexcept
    {
        const int nc = std::min (numChannels, channels);
        for (int n = 0; n < numSamples; ++n)
        {
            double dry[maxChannels] {}, x[maxChannels] {};
            for (int c = 0; c < nc; ++c) dry[c] = x[c] = data[c][n];

            for (auto& b : bands)
                b.tick (x, nc, fs, smooth, fadeStep);

            // Solo : passe-bande à la fréquence de la bande, en fondu.
            const float soloWanted = (soloTarget >= 0 && bands[(size_t) soloTarget].active) ? 1.0f : 0.0f;
            if (soloShown != soloTarget && soloMix <= 0.0f)
            {
                soloShown = soloTarget;
                solo.state = {};
                if (soloShown >= 0) solo.logF = bands[(size_t) soloShown].logF;
            }
            const float soloGoal = soloShown == soloTarget ? soloWanted : 0.0f;
            soloMix = soloMix < soloGoal ? std::min (soloGoal, soloMix + fadeStep) : std::max (soloGoal, soloMix - fadeStep);
            if (soloShown >= 0 && soloMix > 0.0f)
            {
                const auto& src = bands[(size_t) soloShown];
                solo.logF += (src.logF - solo.logF) * smooth;
                const double q = (src.activeType == BandType::bell || src.activeType == BandType::notch) ? std::exp (src.logQ) : 0.9;
                Section s;
                makeSoloSection (std::exp (solo.logF), q, fs, s);
                for (int c = 0; c < nc; ++c)
                {
                    const double y = solo.run (s, c, dry[c]);
                    x[c] += sCurve (soloMix) * (y - x[c]);
                }
            }

            outMid += (outTarget - outMid) * smooth;
            outGain += (outMid - outGain) * smooth;
            const float bypassGoal = bypassTarget ? 1.0f : 0.0f;
            bypassMix = bypassMix < bypassGoal ? std::min (bypassGoal, bypassMix + fadeStep) : std::max (bypassGoal, bypassMix - fadeStep);

            const double byp = sCurve (bypassMix);
            for (int c = 0; c < nc; ++c)
            {
                const double wet = x[c] * outGain;
                data[c][n] = (float) (wet + byp * (dry[c] - wet));
            }
        }
    }

    // Réponse totale (dB) des réglages demandés, pour l'affichage.
    double totalResponseDb (double f) const noexcept
    {
        double db = 0.0;
        for (auto& b : bands) db += bandResponseDb (b.target, f, fs);
        return db + 20.0 * std::log10 (outTarget);
    }

private:
    struct State { double ic1[maxChannels][maxSections] {}, ic2[maxChannels][maxSections] {}; };

    struct Band
    {
        BandSettings target;
        bool active = false;
        BandType activeType = BandType::bell;
        int activeSlope = 1;
        float mix = 0.0f;
        double logF = std::log (1000.0), gainDb = 0.0, logQ = std::log (butterworthQ);
        double midF = logF, midG = 0.0, midQ = logQ;   // 1er étage du lissage
        std::array<Section, maxSections> sec;
        int numSec = 0;
        State state;

        void snap() noexcept
        {
            logF = std::log ((double) target.freq);
            gainDb = target.gain;
            logQ = std::log ((double) target.q);
            midF = logF; midG = gainDb; midQ = logQ;
        }

        void rebuild (double rate) noexcept
        {
            numSec = makeSections (activeType, std::exp (logF), gainDb, std::exp (logQ), activeSlope, rate, sec.data());
        }

        void tick (double* x, int nc, double rate, double smoothing, float step) noexcept
        {
            const bool sameShape = target.type == activeType && target.slope == activeSlope;
            if (! active)
            {
                if (! target.on) return;
                // Allumage : on part directement des bons réglages, filtre vide, fondu d'entrée.
                active = true; activeType = target.type; activeSlope = target.slope; mix = 0.0f;
                state = {}; snap(); rebuild (rate);
            }

            const float goal = (target.on && sameShape) ? 1.0f : 0.0f;
            mix = mix < goal ? std::min (goal, mix + step) : std::max (goal, mix - step);
            if (mix <= 0.0f && goal <= 0.0f)
            {
                if (! target.on) { active = false; state = {}; return; }
                activeType = target.type; activeSlope = target.slope;   // nouveau type, au silence du fondu
                state = {}; snap(); rebuild (rate);
                return;
            }

            const double tf = std::log ((double) target.freq), tq = std::log ((double) target.q);
            if (std::abs (tf - logF) > 1.0e-6 || std::abs (target.gain - gainDb) > 1.0e-6 || std::abs (tq - logQ) > 1.0e-6)
            {
                // Deux étages : le mouvement démarre en douceur au lieu de partir d'un coup.
                midF += (tf - midF) * smoothing;          logF += (midF - logF) * smoothing;
                midG += (target.gain - midG) * smoothing; gainDb += (midG - gainDb) * smoothing;
                midQ += (tq - midQ) * smoothing;          logQ += (midQ - logQ) * smoothing;
                rebuild (rate);
            }

            const double amount = sCurve (mix);
            for (int c = 0; c < nc; ++c)
            {
                double y = x[c];
                for (int i = 0; i < numSec; ++i)
                {
                    const auto& s = sec[(size_t) i];
                    double& ic1 = state.ic1[c][i];
                    double& ic2 = state.ic2[c][i];
                    const double v3 = y - ic2;
                    const double v1 = s.a1 * ic1 + s.a2 * v3;
                    const double v2 = ic2 + s.a2 * ic1 + s.a3 * v3;
                    ic1 = 2.0 * v1 - ic1;
                    ic2 = 2.0 * v2 - ic2;
                    y = s.m0 * y + s.m1 * v1 + s.m2 * v2;
                }
                x[c] += amount * (y - x[c]);
            }
        }
    };

    struct Solo
    {
        double logF = std::log (1000.0);
        State state;
        double run (const Section& s, int c, double v0) noexcept
        {
            double& ic1 = state.ic1[c][0];
            double& ic2 = state.ic2[c][0];
            const double v3 = v0 - ic2;
            const double v1 = s.a1 * ic1 + s.a2 * v3;
            const double v2 = ic2 + s.a2 * ic1 + s.a3 * v3;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;
            return s.m0 * v0 + s.m1 * v1 + s.m2 * v2;
        }
    };

    std::array<Band, maxBands> bands;
    Solo solo;
    double fs = 48000.0, smooth = 0.001, outGain = 1.0, outMid = 1.0, outTarget = 1.0;
    float fadeStep = 0.001f, soloMix = 0.0f, bypassMix = 0.0f;
    int channels = 2, soloTarget = -1, soloShown = -1;
    bool bypassTarget = false;
};

} // namespace swelleq
