#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace swelleq {

// File sans verrou : le thread audio pousse des échantillons mono, l'interface les lit. Si elle est pleine
// (fenêtre fermée), les échantillons sont simplement perdus.
class SampleFifo
{
public:
    SampleFifo() : buffer ((size_t) capacity) {}
    void push (const float* data, int n) noexcept
    {
        const auto scope = fifo.write (juce::jmin (n, fifo.getFreeSpace()));
        if (scope.blockSize1 > 0) std::copy (data, data + scope.blockSize1, buffer.data() + scope.startIndex1);
        if (scope.blockSize2 > 0) std::copy (data + scope.blockSize1, data + scope.blockSize1 + scope.blockSize2, buffer.data() + scope.startIndex2);
    }
    int pull (float* dest, int maxN) noexcept
    {
        const auto scope = fifo.read (juce::jmin (maxN, fifo.getNumReady()));
        if (scope.blockSize1 > 0) std::copy (buffer.data() + scope.startIndex1, buffer.data() + scope.startIndex1 + scope.blockSize1, dest);
        if (scope.blockSize2 > 0) std::copy (buffer.data() + scope.startIndex2, buffer.data() + scope.startIndex2 + scope.blockSize2, dest + scope.blockSize1);
        return scope.blockSize1 + scope.blockSize2;
    }
private:
    static constexpr int capacity = 1 << 15;
    juce::AbstractFifo fifo { capacity };
    std::vector<float> buffer;
};

struct Resonance
{
    float freq = 0.0f;     // Hz
    float excessDb = 0.0f; // dépassement par rapport au spectre lissé autour
    float q = 4.0f;        // Q conseillé pour l'atténuer
};

// Détection des résonances : pics étroits qui dépassent nettement le spectre moyen voisin.
// 'db' est un spectre moyenné sur une grille de fréquences logarithmique 'freqs'.
std::vector<Resonance> findResonances (const std::vector<float>& freqs, const std::vector<float>& db, int maxCount = 4);

// Analyseur (thread des messages) : FFT 8192 points, recouvrement 75 %, affichage lissé et moyenne longue.
class Analyzer
{
public:
    static constexpr int fftOrder = 13, fftSize = 1 << fftOrder, hop = fftSize / 4;
    static constexpr int gridSize = 320;                          // points log entre 20 Hz et 20 kHz
    static constexpr float floorDb = -100.0f;

    Analyzer();
    void setSampleRate (double fs);
    // Lit ce que le thread audio a poussé et met les spectres à jour. dtSeconds : temps depuis l'appel précédent.
    void update (SampleFifo& pre, SampleFifo& post, float dtSeconds);
    void clearAverage();

    const std::vector<float>& gridFreqs() const { return freqs; }
    const std::vector<float>& preDb() const { return pre.display; }    // avec pente d'affichage +3 dB/oct
    const std::vector<float>& postDb() const { return post.display; }
    const std::vector<Resonance>& resonances() const { return found; }
    bool hasSignal() const { return signalSeconds > 0.0f; }
    float averagedSeconds() const { return post.averagedSeconds; }

private:
    struct Path
    {
        std::vector<float> ring, display, average;   // average : puissance moyenne (linéaire) par point de grille
        int writePos = 0, sinceLast = 0, averagedFrames = 0;
        float averagedSeconds = 0.0f;
    };
    void consume (SampleFifo&, Path&, bool keepAverage, float dt);
    void analyse (Path&, bool keepAverage, float dt);

    double sampleRate = 48000.0;
    juce::dsp::FFT fft { fftOrder };
    std::vector<float> window, scratch, frameDb, freqs, tilt;
    std::vector<float> pullBuffer;
    Path pre, post;
    std::vector<Resonance> found;
    float signalSeconds = 0.0f, sinceResonance = 0.0f;
};

} // namespace swelleq
