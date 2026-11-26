#include "Analyzer.h"

namespace swelleq {

std::vector<Resonance> findResonances (const std::vector<float>& freqs, const std::vector<float>& db, int maxCount)
{
    std::vector<Resonance> out;
    const int n = (int) juce::jmin (freqs.size(), db.size());
    if (n < 16) return out;

    const float perOctave = (float) (n - 1) / std::log2 (freqs.back() / freqs.front());
    const int half = juce::jmax (2, juce::roundToInt (0.5f * perOctave));     // voisinage : ±1/2 octave
    const int skip = juce::jmax (1, juce::roundToInt (perOctave / 6.0f));     // sans le pic lui-même (±1/6 oct)

    int lo = 0, hi = n - 1;
    while (lo < n && freqs[(size_t) lo] < 60.0f) ++lo;
    while (hi > 0 && freqs[(size_t) hi] > 12000.0f) --hi;
    if (hi - lo < 8) return out;

    float maxDb = -1000.0f;
    for (int i = lo; i <= hi; ++i) maxDb = juce::jmax (maxDb, db[(size_t) i]);

    struct Candidate { int index; float excess; };
    std::vector<Candidate> candidates;
    for (int i = juce::jmax (lo, 1); i <= juce::jmin (hi, n - 2); ++i)
    {
        const float v = db[(size_t) i];
        if (v < db[(size_t) i - 1] || v <= db[(size_t) i + 1]) continue;
        if (v < maxDb - 45.0f || v < -85.0f) continue;

        double sum = 0.0;
        int count = 0;
        for (int j = i - half; j <= i + half; ++j)
            if (j >= 0 && j < n && std::abs (j - i) > skip) { sum += db[(size_t) j]; ++count; }
        if (count == 0) continue;
        const float excess = v - (float) (sum / count);
        if (excess >= 6.0f) candidates.push_back ({ i, excess });
    }

    std::sort (candidates.begin(), candidates.end(), [] (auto& a, auto& b) { return a.excess > b.excess; });
    for (auto& c : candidates)
    {
        if ((int) out.size() >= maxCount) break;
        const float f = freqs[(size_t) c.index];
        const bool tooClose = std::any_of (out.begin(), out.end(), [f] (auto& r) { return std::abs (std::log2 (r.freq / f)) < 0.25f; });
        if (tooClose) continue;

        // Largeur à -3 dB -> Q conseillé.
        const float peak = db[(size_t) c.index];
        int l = c.index, r = c.index;
        while (l > 0 && db[(size_t) l - 1] > peak - 3.0f) --l;
        while (r < n - 1 && db[(size_t) r + 1] > peak - 3.0f) ++r;
        const float bw = juce::jmax (1.0f / 30.0f, std::log2 (freqs[(size_t) r] / freqs[(size_t) l]) + 1.0f / perOctave);
        const float p = std::pow (2.0f, bw);
        const float q = juce::jlimit (2.0f, 12.0f, std::sqrt (p) / (p - 1.0f));
        out.push_back ({ f, c.excess, q });
    }
    std::sort (out.begin(), out.end(), [] (auto& a, auto& b) { return a.freq < b.freq; });
    return out;
}

Analyzer::Analyzer()
{
    window.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) fftSize);
    scratch.assign ((size_t) fftSize * 2, 0.0f);
    frameDb.assign ((size_t) gridSize, floorDb);
    freqs.resize ((size_t) gridSize);
    tilt.resize ((size_t) gridSize);
    for (int i = 0; i < gridSize; ++i)
    {
        const float f = 20.0f * std::pow (1000.0f, (float) i / (float) (gridSize - 1));
        freqs[(size_t) i] = f;
        tilt[(size_t) i] = 3.0f * std::log2 (f / 1000.0f);   // le bruit rose apparaît à plat, comme à l'oreille
    }
    for (auto* p : { &pre, &post })
    {
        p->ring.assign ((size_t) fftSize, 0.0f);
        p->display.assign ((size_t) gridSize, floorDb);
        p->average.assign ((size_t) gridSize, 0.0f);
    }
    pullBuffer.resize (4096);
}

void Analyzer::setSampleRate (double fs)
{
    if (fs > 0 && fs != sampleRate) { sampleRate = fs; clearAverage(); }
}

void Analyzer::clearAverage()
{
    for (auto* p : { &pre, &post }) { std::fill (p->average.begin(), p->average.end(), 0.0f); p->averagedSeconds = 0.0f; p->averagedFrames = 0; }
    found.clear();
}

void Analyzer::update (SampleFifo& preFifo, SampleFifo& postFifo, float dt)
{
    signalSeconds = juce::jmax (0.0f, signalSeconds - dt);
    consume (preFifo, pre, false, dt);
    consume (postFifo, post, true, dt);

    sinceResonance += dt;
    if (sinceResonance >= 0.25f)
    {
        sinceResonance = 0.0f;
        if (post.averagedSeconds >= 2.0f)
        {
            std::vector<float> avgDb ((size_t) gridSize);
            for (int i = 0; i < gridSize; ++i)
                avgDb[(size_t) i] = 10.0f * std::log10 (post.average[(size_t) i] + 1.0e-20f);
            found = findResonances (freqs, avgDb);
        }
    }
}

void Analyzer::consume (SampleFifo& fifo, Path& p, bool keepAverage, float dt)
{
    bool any = false;
    for (;;)
    {
        const int got = fifo.pull (pullBuffer.data(), (int) pullBuffer.size());
        if (got <= 0) break;
        any = true;
        for (int i = 0; i < got; ++i)
        {
            p.ring[(size_t) p.writePos] = pullBuffer[(size_t) i];
            p.writePos = (p.writePos + 1) % fftSize;
            if (++p.sinceLast >= hop) { p.sinceLast = 0; analyse (p, keepAverage, dt); }
        }
    }
    // Plus rien n'arrive (transport arrêté, l'hôte n'appelle plus le plugin) : l'affichage retombe.
    if (! any)
        for (auto& v : p.display) v = juce::jmax (floorDb, v - 36.0f * dt);
}

void Analyzer::analyse (Path& p, bool keepAverage, float)
{
    for (int i = 0; i < fftSize; ++i)
        scratch[(size_t) i] = p.ring[(size_t) ((p.writePos + i) % fftSize)] * window[(size_t) i];
    std::fill (scratch.begin() + fftSize, scratch.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (scratch.data(), true);

    const float norm = 2.0f / (0.5f * (float) fftSize);    // un sinus d'amplitude 1 = 0 dB
    const float binHz = (float) sampleRate / (float) fftSize;
    const int lastBin = fftSize / 2;
    const float halfStep = std::pow (2.0f, 0.5f * std::log2 (1000.0f) / (float) (gridSize - 1));
    const float frameSeconds = (float) hop / (float) sampleRate;
    float frameMax = -1000.0f;

    for (int i = 0; i < gridSize; ++i)
    {
        const float f = freqs[(size_t) i];
        const float b0 = f / halfStep / binHz, b1 = f * halfStep / binHz;
        float power = 0.0f;
        const int first = (int) std::ceil (b0), last = juce::jmin (lastBin, (int) std::floor (b1));
        if (last >= first)
        {
            for (int b = first; b <= last; ++b) power = juce::jmax (power, juce::square (scratch[(size_t) b] * norm));
        }
        else
        {
            const float b = f / binHz;
            const int k = juce::jlimit (0, lastBin - 1, (int) b);
            const float frac = b - (float) k;
            const float mag = (scratch[(size_t) k] * (1.0f - frac) + scratch[(size_t) k + 1] * frac) * norm;
            power = mag * mag;
        }
        const float db = juce::jmax (floorDb, 10.0f * std::log10 (power + 1.0e-20f));
        frameDb[(size_t) i] = db;
        frameMax = juce::jmax (frameMax, db);

        // Affichage : montée immédiate, descente de 36 dB/s.
        auto& shown = p.display[(size_t) i];
        const float target = db + tilt[(size_t) i];
        shown = target > shown ? shown + 0.7f * (target - shown) : juce::jmax (target, shown - 36.0f * frameSeconds);
    }

    if (keepAverage)
    {
        if (frameMax > -70.0f)
        {
            signalSeconds = 1.0f;
            // Moyenne longue (≈ 6 s) : une note tenue passe, une résonance de la pièce ou du micro reste.
            const float alpha = 1.0f - std::exp (-frameSeconds / 6.0f);
            // Au début, vraie moyenne de toutes les trames reçues ; ensuite, moyenne glissante.
            const float weight = juce::jmax (alpha, 1.0f / (float) (++p.averagedFrames));
            for (int i = 0; i < gridSize; ++i)
            {
                const float pw = std::pow (10.0f, frameDb[(size_t) i] / 10.0f);
                auto& avg = p.average[(size_t) i];
                avg += weight * (pw - avg);
            }
            p.averagedSeconds += frameSeconds;
        }
    }
}

} // namespace swelleq
