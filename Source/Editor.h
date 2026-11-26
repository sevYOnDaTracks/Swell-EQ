#pragma once
#include "Processor.h"

namespace swelleq {

namespace colours {
    inline const juce::Colour background { 0xff141113 }, hero { 0xff0e0c0d }, strip { 0xff181517 }, raised { 0xff221e21 },
                              cream { 0xfff4eee6 }, muted { 0xff8f8781 }, accent { 0xfff2a65a }, ink { 0xff1a110b },
                              alert { 0xffff7a59 };
}

struct Fonts
{
    juce::Typeface::Ptr regular, semibold;
    juce::Font ui (float height, bool bold = true) const;
};

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit LookAndFeel (const Fonts&);
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcut,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;
    juce::Font getPopupMenuFont() override { return fonts.ui (14.5f, false); }
    void getIdealPopupMenuItemSize (const juce::String&, bool isSeparator, int standardHeight, int& w, int& h) override;
    int getPopupMenuBorderSize() override { return 6; }
private:
    const Fonts& fonts;
};

// Bouton dessiné par une fonction : chaque commande garde son allure au même endroit.
struct PaintedButton final : public juce::Button
{
    PaintedButton() : juce::Button ({}) {}
    std::function<void (juce::Graphics&, juce::Rectangle<float>, bool over, bool down)> painter;
    void paintButton (juce::Graphics& g, bool over, bool down) override { if (painter) painter (g, getLocalBounds().toFloat(), over, down); }
};

struct Selection
{
    int band = -1;
    std::function<void()> changed;
    void set (int b) { if (b != band) { band = b; if (changed) changed(); } }
};

// La courbe : grille, spectre en temps réel, réponse de l'égaliseur, points des bandes, résonances.
class CurveView final : public juce::Component
{
public:
    CurveView (Processor&, Analyzer&, const Fonts&, Selection&);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;
    void showBandMenu (int band, juce::Component* target = nullptr);
    void fixResonance (const Resonance&);

    float xForFreq (float f) const;
    float freqForX (float x) const;
    float yForDb (float db) const;
    float dbForY (float y) const;
    float yForSpectrum (float db) const;
    juce::Point<float> nodePosition (int band) const;

private:
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (0.0f, 14.0f); }
    float range() const { return (float) proc.displayRange; }
    int nodeAt (juce::Point<float>) const;
    int resonanceAt (juce::Point<float>) const;
    juce::Point<float> resonancePosition (const Resonance&) const;
    float spectrumAt (const std::vector<float>&, float freq) const;
    juce::Path spectrumPath (const std::vector<float>&, bool closed) const;
    juce::Path responsePath (std::function<double (double)>) const;
    void paintGrid (juce::Graphics&);
    void paintNodeLabel (juce::Graphics&, int band);
    void paintHint (juce::Graphics&);
    juce::RangedAudioParameter* param (int band, const char* what) const;
    void setBandQ (int band, float q);

    Processor& proc;
    Analyzer& analyzer;
    const Fonts& fonts;
    Selection& selection;
    int hoverNode = -1, hoverResonance = -1, dragging = -1;
    float dragStartLogF = 0.0f, dragStartGain = 0.0f;
    juce::Point<float> dragStart;
};

// Réglages de la bande choisie (une seule rangée de boutons) et volume de sortie.
class BandStrip final : public juce::Component
{
public:
    BandStrip (Processor&, Analyzer&, const Fonts&, Selection&, CurveView&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();     // minuterie de l'éditeur
    void bind();        // la sélection a changé

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::String label;
    };
    void paintKnobText (juce::Graphics&, const Knob&, const juce::String& value, bool enabled);
    void showTypeMenu();
    void showSlopeMenu();

    Processor& proc;
    Analyzer& analyzer;
    const Fonts& fonts;
    Selection& selection;
    CurveView& curve;
    Knob freq, gain, q, output;
    PaintedButton typeButton, slopeButton, soloButton, removeButton, fixAllButton;
    swell::LicenceBadge licenceBadge { colours::muted.withAlpha (0.8f), colours::cream };
    int boundBand = -2;
    BandSettings shown;
    int shownSolo = -2;
    size_t shownResonances = 0;
};

class Editor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Editor (Processor&);
    ~Editor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    // Outils hors ligne (captures) : fait tourner l'analyseur et rafraîchit sans attendre la minuterie.
    void pump (float dtSeconds);
    Selection& getSelection() { return selection; }
    CurveView& getCurve() { return curve; }
    juce::Rectangle<int> getPresetBounds() const { return presetButton.getBounds(); }
    juce::Rectangle<int> getResonanceBounds() const { return resonanceButton.getBounds(); }

private:
    void timerCallback() override;
    void showPresetMenu();
    void showSettingsMenu();
    void askPresetName();

    Processor& proc;
    Fonts fonts;
    LookAndFeel lnf { fonts };
    Analyzer analyzer;
    Selection selection;
    juce::Image logo;
    CurveView curve;
    BandStrip strip;
    PaintedButton prev, next, presetButton, resonanceButton, gear;
    juce::String shownPreset;
    float meter = 0.0f;
    double lastTick = 0.0;
};

} // namespace swelleq
