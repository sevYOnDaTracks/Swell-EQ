#include "Editor.h"
#include "BinaryData.h"

namespace swelleq {
using namespace juce;

// ---------------------------------------------------------------- polices, outils de dessin

Font Fonts::ui (float height, bool bold) const
{
    auto tf = bold ? semibold : regular;
    return tf != nullptr ? Font (FontOptions (tf).withHeight (height)) : Font (FontOptions (height));
}

static float textWidth (const Font& f, const String& s) { return GlyphArrangement::getStringWidth (f, s); }

static void drawChevron (Graphics& g, Point<float> c, float size, int direction /* -1 gauche, 1 droite, 2 bas */, Colour colour, float thickness = 1.6f)
{
    Path p;
    if (direction == 2) { p.startNewSubPath (c.x - size, c.y - size * 0.5f); p.lineTo (c.x, c.y + size * 0.5f); p.lineTo (c.x + size, c.y - size * 0.5f); }
    else { const float d = (float) direction; p.startNewSubPath (c.x - d * size * 0.5f, c.y - size); p.lineTo (c.x + d * size * 0.5f, c.y); p.lineTo (c.x - d * size * 0.5f, c.y + size); }
    g.setColour (colour);
    g.strokePath (p, PathStrokeType (thickness, PathStrokeType::curved, PathStrokeType::rounded));
}

static void drawPill (Graphics& g, Rectangle<float> r, const Font& font, const String& text, bool over, bool withArrow)
{
    g.setColour (Colours::white.withAlpha (over ? 0.10f : 0.06f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (colours::cream);
    g.setFont (font);
    auto t = r.reduced (14.0f, 0.0f);
    if (withArrow)
    {
        drawChevron (g, { t.getRight() - 4.0f, t.getCentreY() + 1.0f }, 4.0f, 2, colours::muted, 1.4f);
        t.removeFromRight (14.0f);
    }
    g.drawText (text, t, Justification::centredLeft, true);
}

static Path headphonesIcon (Rectangle<float> r)
{
    Path p;
    const float w = r.getWidth(), h = r.getHeight();
    p.addCentredArc (r.getCentreX(), r.getY() + h * 0.6f, w * 0.40f, h * 0.55f, 0.0f, -1.75f, 1.75f, true);
    return p;
}

static void drawHeadphones (Graphics& g, Rectangle<float> r, Colour c)
{
    g.setColour (c);
    g.strokePath (headphonesIcon (r), PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
    const float cupW = r.getWidth() * 0.22f, cupH = r.getHeight() * 0.36f;
    g.fillRoundedRectangle (r.getX() + r.getWidth() * 0.06f, r.getBottom() - cupH, cupW, cupH, 2.0f);
    g.fillRoundedRectangle (r.getRight() - r.getWidth() * 0.06f - cupW, r.getBottom() - cupH, cupW, cupH, 2.0f);
}

static void drawGear (Graphics& g, Rectangle<float> r, Colour c)
{
    const auto centre = r.getCentre();
    const float outer = r.getWidth() * 0.5f;
    Path teeth;
    for (int i = 0; i < 8; ++i)
    {
        Path t;
        t.addRoundedRectangle (-outer * 0.16f, -outer, outer * 0.32f, outer * 0.5f, 1.2f);
        teeth.addPath (t, AffineTransform::rotation (MathConstants<float>::twoPi * (float) i / 8.0f).translated (centre));
    }
    g.setColour (c);
    g.fillPath (teeth);
    g.drawEllipse (Rectangle<float> (outer * 1.3f, outer * 1.3f).withCentre (centre), outer * 0.28f);
}

// ---------------------------------------------------------------- look and feel

LookAndFeel::LookAndFeel (const Fonts& f) : fonts (f)
{
    setColour (PopupMenu::backgroundColourId, colours::raised);
    setColour (PopupMenu::textColourId, colours::cream);
    setColour (PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.16f));
    setColour (PopupMenu::highlightedTextColourId, colours::cream);
    setColour (TooltipWindow::backgroundColourId, colours::raised);
    setColour (TooltipWindow::textColourId, colours::cream);
    setColour (AlertWindow::backgroundColourId, colours::raised);
    setColour (AlertWindow::textColourId, colours::cream);
    setColour (TextEditor::backgroundColourId, colours::hero);
    setColour (TextEditor::textColourId, colours::cream);
    setColour (TextEditor::outlineColourId, Colours::white.withAlpha (0.1f));
    setColour (TextEditor::focusedOutlineColourId, colours::accent);
    setColour (TextButton::buttonColourId, Colours::white.withAlpha (0.08f));
    setColour (TextButton::textColourOffId, colours::cream);
}

void LookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& slider)
{
    auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size = jmin (area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    const float ring = size * 0.5f - 3.0f;
    const float angle = start + pos * (end - start);
    const bool enabled = slider.isEnabled();
    const bool bipolar = slider.getProperties()["bipolar"];

    Path track, value;
    track.addCentredArc (c.x, c.y, ring, ring, 0.0f, start, end, true);
    const float from = bipolar ? (start + end) * 0.5f : start;
    value.addCentredArc (c.x, c.y, ring, ring, 0.0f, jmin (from, angle), jmax (from, angle), true);
    PathStrokeType stroke (3.0f, PathStrokeType::curved, PathStrokeType::rounded);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.strokePath (track, stroke);
    if (enabled && std::abs (angle - from) > 0.01f) { g.setColour (colours::accent); g.strokePath (value, stroke); }

    const float r = size * 0.38f;
    auto cap = Rectangle<float> (r * 2, r * 2).withCentre (c);
    for (int i = 5; i > 0; --i)
    {
        g.setColour (Colours::black.withAlpha (0.07f));
        g.fillEllipse (cap.translated (0, 4.0f).expanded ((float) i * 1.4f));
    }
    ColourGradient grad (Colours::white, cap.getX() + r * 0.64f, cap.getY() + r * 0.52f, Colour (0xffbfc4cc), cap.getRight(), cap.getBottom(), true);
    grad.addColour (0.42, Colour (0xffeef0f3));
    g.setGradientFill (grad);
    g.setOpacity (enabled ? 1.0f : 0.25f);
    g.fillEllipse (cap);

    Path notch;
    notch.addRoundedRectangle (-1.5f, -r + 5.0f, 3.0f, r * 0.5f, 1.5f);
    g.setColour (Colour (0xff1c1d22).withAlpha (enabled ? 1.0f : 0.3f));
    g.fillPath (notch, AffineTransform::rotation (angle).translated (c.x, c.y));
}

void LookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (colours::raised);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawRect (0, 0, w, h);
}

void LookAndFeel::drawPopupMenuItem (Graphics& g, const Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                                     bool isTicked, bool hasSubMenu, const String& text, const String&, const Drawable*, const Colour*)
{
    if (isSeparator)
    {
        g.setColour (Colours::white.withAlpha (0.07f));
        g.fillRect (area.reduced (12, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    auto r = area.toFloat().reduced (4.0f, 1.0f);
    if (isHighlighted && isActive)
    {
        g.setColour (colours::accent.withAlpha (0.16f));
        g.fillRoundedRectangle (r, 6.0f);
    }
    if (isTicked)
    {
        g.setColour (colours::accent);
        g.fillEllipse (Rectangle<float> (6.0f, 6.0f).withCentre ({ r.getX() + 13.0f, r.getCentreY() }));
    }
    g.setColour (isActive ? colours::cream : colours::muted);
    g.setFont (fonts.ui (14.5f, isTicked));
    g.drawText (text, r.withTrimmedLeft (26.0f).withTrimmedRight (22.0f), Justification::centredLeft, true);
    if (hasSubMenu)
        drawChevron (g, { r.getRight() - 12.0f, r.getCentreY() }, 4.0f, 1, colours::muted, 1.4f);
}

void LookAndFeel::drawPopupMenuSectionHeader (Graphics& g, const Rectangle<int>& area, const String& text)
{
    g.setColour (colours::muted);
    g.setFont (fonts.ui (12.0f, true));
    g.drawText (text, area.withTrimmedLeft (16).withTrimmedTop (6), Justification::centredLeft);
}

void LookAndFeel::getIdealPopupMenuItemSize (const String& text, bool isSeparator, int, int& w, int& h)
{
    if (isSeparator) { w = 50; h = 9; return; }
    h = 30;
    w = roundToInt (textWidth (fonts.ui (14.5f, true), text)) + 64;
}

// ---------------------------------------------------------------- courbe

CurveView::CurveView (Processor& p, Analyzer& a, const Fonts& f, Selection& s) : proc (p), analyzer (a), fonts (f), selection (s)
{
    setOpaque (true);
}

RangedAudioParameter* CurveView::param (int band, const char* what) const { return proc.params.getParameter (ids::band (band, what)); }

float CurveView::xForFreq (float f) const { return (float) getWidth() * std::log (f / 20.0f) / std::log (1000.0f); }
float CurveView::freqForX (float x) const { return 20.0f * std::pow (1000.0f, x / jmax (1.0f, (float) getWidth())); }
float CurveView::yForDb (float db) const { auto p = plot(); return p.getCentreY() - db / range() * (p.getHeight() * 0.5f - 12.0f); }
float CurveView::dbForY (float y) const { auto p = plot(); return (p.getCentreY() - y) / (p.getHeight() * 0.5f - 12.0f) * range(); }

float CurveView::yForSpectrum (float db) const
{
    auto p = plot();
    constexpr float top = 6.0f, bottom = -84.0f;
    return jmap (db, bottom, top, p.getBottom() + 14.0f, p.getY() + p.getHeight() * 0.08f);
}

Point<float> CurveView::nodePosition (int b) const
{
    const auto s = proc.band (b);
    float db = 0.0f;
    if (hasGain (s.type)) db = s.gain;
    else if (isCut (s.type)) db = (float) proc.bandResponse (b, s.freq);   // sur sa propre pente (-3 dB)
    const float y = yForDb (jlimit (-range(), range(), db));
    return { xForFreq (s.freq), y };
}

int CurveView::nodeAt (Point<float> pos) const
{
    int best = -1;
    float bestDist = 15.0f;
    for (int b = 0; b < maxBands; ++b)
        if (proc.band (b).on)
        {
            const float d = nodePosition (b).getDistanceFrom (pos);
            if (d < bestDist || (d <= bestDist + 2.0f && b == selection.band)) { best = b; bestDist = d; }
        }
    return best;
}

Point<float> CurveView::resonancePosition (const Resonance& r) const
{
    // Posé sur la courbe : c'est là qu'une bande viendra l'atténuer.
    const float y = jlimit (plot().getY() + 8.0f, plot().getBottom() - 8.0f, yForDb ((float) proc.responseDb (r.freq)));
    return { xForFreq (r.freq), y };
}

int CurveView::resonanceAt (Point<float> pos) const
{
    if (! proc.showResonances) return -1;
    const auto& list = analyzer.resonances();
    for (int i = 0; i < (int) list.size(); ++i)
        if (resonancePosition (list[(size_t) i]).getDistanceFrom (pos) < 12.0f) return i;
    return -1;
}

float CurveView::spectrumAt (const std::vector<float>& v, float freq) const
{
    if (v.empty()) return Analyzer::floorDb;
    const float idx = std::log (freq / 20.0f) / std::log (1000.0f) * (float) (v.size() - 1);
    const int i = jlimit (0, (int) v.size() - 2, (int) idx);
    const float t = jlimit (0.0f, 1.0f, idx - (float) i);
    return v[(size_t) i] * (1.0f - t) + v[(size_t) i + 1] * t;
}

Path CurveView::spectrumPath (const std::vector<float>& v, bool closed) const
{
    Path p;
    const float w = (float) getWidth(), bottom = (float) getHeight();
    bool started = false;
    for (float x = 0.0f; x <= w + 2.0f; x += 2.0f)
    {
        const float y = jmin (bottom, yForSpectrum (spectrumAt (v, freqForX (x))));
        if (! started) { if (closed) { p.startNewSubPath (0.0f, bottom); p.lineTo (x, y); } else p.startNewSubPath (x, y); started = true; }
        else p.lineTo (x, y);
    }
    if (closed) { p.lineTo (w, bottom); p.closeSubPath(); }
    return p;
}

Path CurveView::responsePath (std::function<double (double)> fn) const
{
    Path p;
    const float w = (float) getWidth(), top = -20.0f, bottom = (float) getHeight() + 20.0f;
    for (float x = 0.0f; x <= w + 1.5f; x += 1.5f)
    {
        const float y = jlimit (top, bottom, yForDb ((float) fn (freqForX (x))));
        if (x == 0.0f) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    return p;
}

void CurveView::paintGrid (Graphics& g)
{
    const float h = (float) getHeight(), w = (float) getWidth();
    g.setFont (fonts.ui (10.5f, false));
    const std::pair<float, const char*> marks[] { { 50, "50" }, { 100, "100" }, { 200, "200" }, { 500, "500" },
                                                  { 1000, "1k" }, { 2000, "2k" }, { 5000, "5k" }, { 10000, "10k" } };
    for (auto& [f, label] : marks)
    {
        const float x = std::round (xForFreq (f));
        g.setColour (Colours::white.withAlpha (0.035f));
        g.fillRect (x, 0.0f, 1.0f, h);
        g.setColour (colours::muted.withAlpha (0.7f));
        g.drawText (label, Rectangle<float> (x + 5.0f, h - 20.0f, 40.0f, 14.0f), Justification::centredLeft);
    }
    const float step = range() <= 6.0f ? 3.0f : (range() <= 12.0f ? 6.0f : 12.0f);
    for (float db = -range(); db <= range() + 0.01f; db += step)
    {
        const float y = std::round (yForDb (db));
        g.setColour (Colours::white.withAlpha (db == 0.0f ? 0.09f : 0.035f));
        g.fillRect (0.0f, y, w, 1.0f);
        if (std::abs (db) < range() + 0.01f && std::abs (db) > 0.01f)
        {
            g.setColour (colours::muted.withAlpha (0.6f));
            g.drawText ((db > 0 ? "+" : "") + String ((int) db), Rectangle<float> (w - 40.0f, y - 15.0f, 32.0f, 13.0f), Justification::centredRight);
        }
    }
}

void CurveView::paintNodeLabel (Graphics& g, int b)
{
    const auto s = proc.band (b);
    String text = formatFreq (s.freq);
    if (hasGain (s.type)) text << "   " << formatGain (s.gain);
    if (! isCut (s.type) || std::abs (s.q - (float) butterworthQ) > 0.01f) text << "   Q " << String (s.q, 2).replace (".", ",");
    if (isCut (s.type)) text << "   " << slopeNames()[s.slope];

    const auto font = fonts.ui (12.5f, true);
    const float tw = textWidth (font, text) + 24.0f;
    const auto pos = nodePosition (b);
    auto r = Rectangle<float> (tw, 26.0f).withCentre ({ pos.x, pos.y - 30.0f });
    if (r.getY() < 4.0f) r.setY (pos.y + 18.0f);
    r.setX (jlimit (6.0f, (float) getWidth() - tw - 6.0f, r.getX()));
    g.setColour (colours::raised.withAlpha (0.95f));
    g.fillRoundedRectangle (r, 13.0f);
    g.setColour (colours::cream);
    g.setFont (font);
    g.drawText (text, r, Justification::centred);
}

void CurveView::paintHint (Graphics& g)
{
    String text;
    if (proc.showResonances)
    {
        if (! analyzer.hasSignal()) text = String::fromUTF8 ("Lancez la lecture : Swell EQ écoute le son et repère les résonances.");
        else if (analyzer.averagedSeconds() < 2.0f) text = String::fromUTF8 ("Écoute en cours...");
    }
    bool anyBand = false;
    for (int b = 0; b < maxBands; ++b) anyBand = anyBand || proc.band (b).on;
    const bool suggesting = proc.showResonances && ! analyzer.resonances().empty();
    if (text.isEmpty() && ! anyBand && ! suggesting) text = "Double-cliquez sur la courbe pour ajouter une bande.";
    if (text.isEmpty()) return;
    g.setColour (colours::muted);
    g.setFont (fonts.ui (14.0f, false));
    g.drawText (text, getLocalBounds().toFloat().withTrimmedTop (plot().getHeight() * 0.25f).withHeight (24.0f), Justification::centred);
}

void CurveView::paint (Graphics& g)
{
    g.fillAll (colours::hero);
    // Lueur chaude, très discrète, derrière la courbe.
    g.setGradientFill (ColourGradient (Colour (0xff2a1a12).withAlpha (0.55f), getWidth() * 0.5f, getHeight() * 0.55f,
                                       colours::hero.withAlpha (0.0f), getWidth() * 0.5f, -getHeight() * 0.3f, true));
    g.fillRect (getLocalBounds());

    paintGrid (g);

    // Spectre en temps réel.
    if (proc.analyserMode != 2)
    {
        if (proc.analyserMode == 0)
        {
            g.setColour (Colours::white.withAlpha (0.10f));
            g.strokePath (spectrumPath (analyzer.preDb(), false), PathStrokeType (1.0f));
        }
        const auto fill = spectrumPath (analyzer.postDb(), true);
        g.setGradientFill (ColourGradient (colours::cream.withAlpha (0.13f), 0.0f, plot().getY(), colours::cream.withAlpha (0.015f), 0.0f, (float) getHeight(), false));
        g.fillPath (fill);
        g.setColour (colours::cream.withAlpha (0.22f));
        g.strokePath (spectrumPath (analyzer.postDb(), false), PathStrokeType (1.1f));
    }

    const int solo = proc.soloBand.load();
    const int sel = selection.band;

    // Bande choisie : sa propre forme, remplie.
    if (sel >= 0 && proc.band (sel).on)
    {
        auto own = responsePath ([this, sel] (double f) { return proc.bandResponse (sel, f); });
        Path area (own);
        const float zero = yForDb (0.0f);
        area.lineTo ((float) getWidth() + 2.0f, zero);
        area.lineTo (0.0f, zero);
        area.closeSubPath();
        g.setColour (colours::accent.withAlpha (0.13f));
        g.fillPath (area);
        g.setColour (colours::accent.withAlpha (0.55f));
        g.strokePath (own, PathStrokeType (1.2f));
    }

    // Réponse totale.
    const auto total = responsePath ([this] (double f) { return proc.responseDb (f); });
    g.setColour (colours::cream.withAlpha (0.08f));
    g.strokePath (total, PathStrokeType (7.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (colours::cream);
    g.strokePath (total, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // Résonances repérées.
    if (proc.showResonances)
    {
        const auto& list = analyzer.resonances();
        const float phase = (float) (Time::getMillisecondCounter() % 1600) / 1600.0f;
        for (int i = 0; i < (int) list.size(); ++i)
        {
            const auto pos = resonancePosition (list[(size_t) i]);
            const bool over = i == hoverResonance;
            g.setGradientFill (ColourGradient (colours::alert.withAlpha (over ? 0.45f : 0.28f), pos.x, pos.y,
                                               colours::alert.withAlpha (0.0f), pos.x, plot().getBottom(), false));
            g.fillRect (pos.x - 0.75f, pos.y, 1.5f, plot().getBottom() - pos.y);
            g.setColour (colours::alert.withAlpha (0.18f * (1.0f - phase)));
            g.fillEllipse (Rectangle<float> (1.0f, 1.0f).withCentre (pos).expanded (6.0f + 7.0f * phase));
            g.setColour (colours::alert);
            g.fillEllipse (Rectangle<float> (1.0f, 1.0f).withCentre (pos).expanded (over ? 6.5f : 5.0f));
        }
        if (isPositiveAndBelow (hoverResonance, (int) list.size()))
        {
            const auto& r = list[(size_t) hoverResonance];
            const String text = String::fromUTF8 ("Résonance ") + formatFreq (r.freq) + String::fromUTF8 ("  ·  +") + String (r.excessDb, 1).replace (".", ",") + String::fromUTF8 (" dB  ·  cliquer pour atténuer");
            const auto font = fonts.ui (12.5f, true);
            const float tw = textWidth (font, text) + 26.0f;
            const auto pos = resonancePosition (r);
            auto box = Rectangle<float> (tw, 26.0f).withCentre ({ pos.x, pos.y - 26.0f });
            if (box.getY() < 4.0f) box.setY (pos.y + 16.0f);
            box.setX (jlimit (6.0f, (float) getWidth() - tw - 6.0f, box.getX()));
            g.setColour (colours::raised.withAlpha (0.96f));
            g.fillRoundedRectangle (box, 13.0f);
            g.setColour (colours::cream);
            g.setFont (font);
            g.drawText (text, box, Justification::centred);
        }
    }

    // Points des bandes.
    for (int b = 0; b < maxBands; ++b)
    {
        if (! proc.band (b).on) continue;
        const auto pos = nodePosition (b);
        const bool isSel = b == sel, over = b == hoverNode || b == dragging;
        const float dim = (solo >= 0 && solo != b) ? 0.35f : 1.0f;
        const float r = over ? 10.5f : 9.5f;
        auto circle = Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pos);
        if (isSel)
        {
            g.setColour (colours::accent.withAlpha (0.22f));
            g.fillEllipse (circle.expanded (7.0f));
            g.setColour (colours::accent);
            g.fillEllipse (circle);
        }
        else
        {
            g.setColour (Colour (0xff1d1a1c).withAlpha (dim));
            g.fillEllipse (circle);
            g.setColour (colours::cream.withAlpha ((over ? 1.0f : 0.8f) * dim));
            g.drawEllipse (circle.reduced (0.75f), 1.5f);
        }
        g.setColour (isSel ? colours::ink : colours::cream.withAlpha (dim));
        g.setFont (fonts.ui (11.5f, true));
        g.drawText (String (b + 1), circle.translated (0.0f, 0.5f), Justification::centred);
        if (b == solo)
            drawHeadphones (g, Rectangle<float> (14.0f, 12.0f).withCentre ({ pos.x, pos.y + r + 13.0f }), colours::accent);
    }

    const int labelBand = dragging >= 0 ? dragging : hoverNode;
    if (labelBand >= 0 && proc.band (labelBand).on) paintNodeLabel (g, labelBand);

    paintHint (g);
}

void CurveView::mouseMove (const MouseEvent& e)
{
    const int n = nodeAt (e.position), r = n < 0 ? resonanceAt (e.position) : -1;
    if (n != hoverNode || r != hoverResonance) { hoverNode = n; hoverResonance = r; repaint(); }
    setMouseCursor (n >= 0 || r >= 0 ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
}

void CurveView::mouseExit (const MouseEvent&)
{
    hoverNode = hoverResonance = -1;
    repaint();
}

void CurveView::fixResonance (const Resonance& r)
{
    const float cut = -jlimit (2.0f, 9.0f, r.excessDb * 0.7f);
    const int b = proc.addBand (BandType::bell, r.freq, std::round (cut * 2.0f) / 2.0f, r.q);
    if (b >= 0) selection.set (b);
}

void CurveView::mouseDown (const MouseEvent& e)
{
    const int n = nodeAt (e.position);
    if (n >= 0)
    {
        selection.set (n);
        if (e.mods.isPopupMenu()) { showBandMenu (n); return; }
        dragging = n;
        const auto s = proc.band (n);
        dragStartLogF = std::log (s.freq);
        dragStartGain = s.gain;
        dragStart = e.position;
        param (n, "freq")->beginChangeGesture();
        param (n, "gain")->beginChangeGesture();
        repaint();
        return;
    }
    const int r = resonanceAt (e.position);
    if (r >= 0) { fixResonance (analyzer.resonances()[(size_t) r]); repaint(); return; }
    if (! e.mods.isPopupMenu()) selection.set (-1);
    repaint();
}

void CurveView::mouseDrag (const MouseEvent& e)
{
    if (dragging < 0) return;
    const float scale = e.mods.isShiftDown() ? 0.2f : 1.0f;
    const auto delta = e.position - dragStart;
    const float f = jlimit (20.0f, 20000.0f, std::exp (dragStartLogF + delta.x / jmax (1.0f, (float) getWidth()) * std::log (1000.0f) * scale));
    auto* fp = param (dragging, "freq");
    fp->setValueNotifyingHost (fp->convertTo0to1 (f));
    if (hasGain (proc.band (dragging).type))
    {
        const float perPixel = range() / (plot().getHeight() * 0.5f - 12.0f);
        float gain = jlimit (-24.0f, 24.0f, dragStartGain - delta.y * perPixel * scale);
        if (std::abs (gain) < 0.25f && ! e.mods.isShiftDown()) gain = 0.0f;   // petit cran à 0 dB
        auto* gp = param (dragging, "gain");
        gp->setValueNotifyingHost (gp->convertTo0to1 (gain));
    }
    repaint();
}

void CurveView::mouseUp (const MouseEvent&)
{
    if (dragging >= 0)
    {
        param (dragging, "freq")->endChangeGesture();
        param (dragging, "gain")->endChangeGesture();
    }
    dragging = -1;
    repaint();
}

void CurveView::mouseDoubleClick (const MouseEvent& e)
{
    const int n = nodeAt (e.position);
    if (n >= 0) { proc.removeBand (n); selection.set (-1); hoverNode = -1; repaint(); return; }
    if (resonanceAt (e.position) >= 0) return;

    const float f = freqForX (e.position.x);
    BandType type = BandType::bell;
    if (f < 35.0f) type = BandType::lowCut;
    else if (f > 15000.0f) type = BandType::highCut;
    const float gain = jlimit (-24.0f, 24.0f, std::round (dbForY (e.position.y) * 2.0f) / 2.0f);
    const int b = proc.addBand (type, jlimit (20.0f, 20000.0f, f), gain, (float) (type == BandType::bell ? 1.0 : butterworthQ));
    if (b >= 0) selection.set (b);
    repaint();
}

void CurveView::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    int b = nodeAt (e.position);
    if (b < 0) b = hoverNode >= 0 ? hoverNode : -1;
    if (b < 0) return;
    auto* qp = param (b, "q");
    const float q = jlimit (0.1f, 18.0f, proc.band (b).q * std::exp (wheel.deltaY * (wheel.isReversed ? -1.0f : 1.0f) * 0.8f));
    qp->beginChangeGesture();
    qp->setValueNotifyingHost (qp->convertTo0to1 (q));
    qp->endChangeGesture();
    hoverNode = b;
    repaint();
}

void CurveView::showBandMenu (int b, Component* target)
{
    const auto s = proc.band (b);
    PopupMenu m;
    const auto types = typeNames();
    for (int t = 0; t < numBandTypes; ++t) m.addItem (1 + t, types[t], true, (int) s.type == t);
    if (isCut (s.type))
    {
        PopupMenu slopes;
        const auto names = slopeNames();
        for (int i = 0; i < 4; ++i) slopes.addItem (100 + i, names[i], true, s.slope == i);
        m.addSubMenu ("Pente", slopes);
    }
    m.addSeparator();
    m.addItem (200, String::fromUTF8 ("Écouter cette bande seule"), true, proc.soloBand.load() == b);
    m.addItem (300, "Supprimer la bande");
    auto options = PopupMenu::Options().withParentComponent (getTopLevelComponent());
    options = target != nullptr ? options.withTargetComponent (target) : options.withMousePosition();
    m.showMenuAsync (options, [this, b] (int r) {
        if (r >= 1 && r <= numBandTypes)
        {
            auto s2 = proc.band (b);
            s2.type = (BandType) (r - 1);
            if (! isCut (s2.type) && s2.q < 0.3f) s2.q = (float) butterworthQ;
            proc.setBand (b, s2);
        }
        else if (r >= 100 && r < 104) proc.setBandValue (b, "slope", (float) (r - 100));
        else if (r == 200) proc.soloBand.store (proc.soloBand.load() == b ? -1 : b);
        else if (r == 300) { proc.removeBand (b); selection.set (-1); }
        repaint();
    });
}

// ---------------------------------------------------------------- rangée de réglages

BandStrip::BandStrip (Processor& p, Analyzer& a, const Fonts& f, Selection& s, CurveView& c)
    : proc (p), analyzer (a), fonts (f), selection (s), curve (c)
{
    setOpaque (true);
    freq.label = String::fromUTF8 ("Fréquence");
    gain.label = "Gain";
    q.label = "Largeur (Q)";
    output.label = "Sortie";
    for (auto* k : { &freq, &gain, &q, &output })
    {
        k->slider.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
        k->slider.setMouseDragSensitivity (220);
        k->slider.setVelocityBasedMode (false);
        addAndMakeVisible (k->slider);
        k->slider.onValueChange = [this] { repaint(); curve.repaint(); };
    }
    gain.slider.getProperties().set ("bipolar", true);
    output.slider.getProperties().set ("bipolar", true);
    output.attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.params, ids::output, output.slider);
    output.slider.setDoubleClickReturnValue (true, 0.0);

    typeButton.painter = [this] (Graphics& g, Rectangle<float> r, bool over, bool) {
        drawPill (g, r, fonts.ui (13.5f, true), typeNames()[(int) shown.type], over, true);
    };
    typeButton.onClick = [this] { showTypeMenu(); };
    slopeButton.painter = [this] (Graphics& g, Rectangle<float> r, bool over, bool) {
        drawPill (g, r, fonts.ui (13.5f, true), slopeNames()[shown.slope], over, true);
    };
    slopeButton.onClick = [this] { showSlopeMenu(); };

    soloButton.painter = [this] (Graphics& g, Rectangle<float> r, bool over, bool) {
        const bool on = proc.soloBand.load() == selection.band && selection.band >= 0;
        g.setColour (on ? colours::accent : Colours::white.withAlpha (over ? 0.10f : 0.06f));
        g.fillEllipse (r.reduced (1.0f));
        drawHeadphones (g, r.withSizeKeepingCentre (16.0f, 14.0f), on ? colours::ink : colours::cream);
    };
    soloButton.setTooltip (String::fromUTF8 ("Écouter cette bande seule"));
    soloButton.onClick = [this] {
        const int b = selection.band;
        if (b >= 0) proc.soloBand.store (proc.soloBand.load() == b ? -1 : b);
        repaint(); curve.repaint();
    };
    removeButton.painter = [] (Graphics& g, Rectangle<float> r, bool over, bool) {
        g.setColour (Colours::white.withAlpha (over ? 0.10f : 0.06f));
        g.fillEllipse (r.reduced (1.0f));
        const auto c = r.getCentre();
        Path x;
        x.startNewSubPath (c.x - 4.5f, c.y - 4.5f); x.lineTo (c.x + 4.5f, c.y + 4.5f);
        x.startNewSubPath (c.x + 4.5f, c.y - 4.5f); x.lineTo (c.x - 4.5f, c.y + 4.5f);
        g.setColour (colours::cream);
        g.strokePath (x, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
    };
    removeButton.setTooltip ("Supprimer la bande");
    removeButton.onClick = [this] {
        if (selection.band >= 0) { proc.removeBand (selection.band); selection.set (-1); }
    };
    fixAllButton.painter = [this] (Graphics& g, Rectangle<float> r, bool over, bool) {
        g.setColour (colours::alert.withAlpha (over ? 1.0f : 0.9f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (colours::ink);
        g.setFont (fonts.ui (13.5f, true));
        const int n = (int) analyzer.resonances().size();
        g.drawText (n > 1 ? String::fromUTF8 ("Atténuer les ") + String (n) + String::fromUTF8 (" résonances") : String::fromUTF8 ("Atténuer la résonance"), r, Justification::centred);
    };
    fixAllButton.onClick = [this] {
        const auto list = analyzer.resonances();
        for (auto& r : list) curve.fixResonance (r);
        analyzer.clearAverage();
    };

    for (auto* b : { &typeButton, &slopeButton, &soloButton, &removeButton, &fixAllButton })
    {
        b->setMouseCursor (MouseCursor::PointingHandCursor);
        addChildComponent (b);
    }
    addAndMakeVisible (licenceBadge);
    bind();
}

void BandStrip::bind()
{
    const int b = selection.band;
    boundBand = b;
    freq.attachment.reset(); gain.attachment.reset(); q.attachment.reset();
    if (b >= 0)
    {
        freq.attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.params, ids::band (b, "freq"), freq.slider);
        gain.attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.params, ids::band (b, "gain"), gain.slider);
        q.attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.params, ids::band (b, "q"), q.slider);
        freq.slider.setDoubleClickReturnValue (true, 1000.0);
        gain.slider.setDoubleClickReturnValue (true, 0.0);
        q.slider.setDoubleClickReturnValue (true, butterworthQ);
        shown = proc.band (b);
    }
    for (auto* c : std::initializer_list<Component*> { &freq.slider, &gain.slider, &q.slider, &typeButton, &soloButton, &removeButton })
        c->setVisible (b >= 0);
    slopeButton.setVisible (b >= 0 && isCut (shown.type));
    gain.slider.setEnabled (b >= 0 && hasGain (shown.type));
    refresh();
    repaint();
}

void BandStrip::refresh()
{
    if (selection.band != boundBand) { bind(); return; }
    if (boundBand >= 0)
    {
        const auto s = proc.band (boundBand);
        if (! s.on) { selection.set (-1); return; }
        if (s.type != shown.type || s.slope != shown.slope)
        {
            shown = s;
            slopeButton.setVisible (isCut (s.type));
            gain.slider.setEnabled (hasGain (s.type));
            typeButton.repaint(); slopeButton.repaint();
            repaint();
        }
        shown = s;
    }
    const bool canFix = boundBand < 0 && proc.showResonances && ! analyzer.resonances().empty();
    if (canFix != fixAllButton.isVisible()) { fixAllButton.setVisible (canFix); repaint(); }
    if (analyzer.resonances().size() != shownResonances) { shownResonances = analyzer.resonances().size(); fixAllButton.repaint(); repaint(); }
    if (proc.soloBand.load() != shownSolo) { shownSolo = proc.soloBand.load(); soloButton.repaint(); }
}

void BandStrip::resized()
{
    const int w = getWidth(), h = getHeight();
    const int knob = 66, cell = 116;
    const int knobsX = 300;
    typeButton.setBounds (28, 62, 136, 30);
    slopeButton.setBounds (172, 62, 104, 30);
    int x = knobsX;
    for (auto* k : { &freq, &gain, &q })
    {
        k->slider.setBounds (x + (cell - knob) / 2, 18, knob, knob);
        x += cell;
    }
    soloButton.setBounds (x + 12, 34, 34, 34);
    removeButton.setBounds (x + 54, 34, 34, 34);
    output.slider.setBounds (w - 116 + (116 - knob) / 2 - 20, 18, knob, knob);
    fixAllButton.setBounds (28, 84, 230, 34);
    licenceBadge.setBounds (w - 320, h - 24, 300, 18);
}

void BandStrip::paintKnobText (Graphics& g, const Knob& k, const String& value, bool enabled)
{
    const auto r = k.slider.getBounds().toFloat();
    auto area = Rectangle<float> (r.getCentreX() - 60.0f, r.getBottom() + 6.0f, 120.0f, 18.0f);
    g.setColour (enabled ? colours::cream : colours::muted);
    g.setFont (fonts.ui (13.5f, true));
    g.drawText (k.label, area, Justification::centred);
    g.setColour (colours::muted);
    g.setFont (fonts.ui (12.5f, false));
    g.drawText (enabled ? value : String (String::fromUTF8 ("—")), area.translated (0.0f, 18.0f), Justification::centred);
}

void BandStrip::paint (Graphics& g)
{
    g.fillAll (colours::strip);
    g.setColour (Colours::white.withAlpha (0.05f));
    g.fillRect (0, 0, getWidth(), 1);

    const int b = selection.band;
    if (b >= 0)
    {
        g.setColour (colours::accent);
        g.fillEllipse (28.0f, 27.0f, 20.0f, 20.0f);
        g.setColour (colours::ink);
        g.setFont (fonts.ui (11.5f, true));
        g.drawText (String (b + 1), Rectangle<float> (28.0f, 27.5f, 20.0f, 20.0f), Justification::centred);
        g.setColour (colours::cream);
        g.setFont (fonts.ui (16.0f, true));
        g.drawText ("Bande " + String (b + 1), Rectangle<float> (58.0f, 26.0f, 200.0f, 22.0f), Justification::centredLeft);

        paintKnobText (g, freq, formatFreq ((float) freq.slider.getValue()), true);
        paintKnobText (g, gain, formatGain ((float) gain.slider.getValue()), hasGain (shown.type));
        paintKnobText (g, q, String (q.slider.getValue(), 2).replace (".", ","), true);
    }
    else
    {
        // Rien de choisi : le nom du preset et à quoi il sert.
        String title = proc.currentPresetName(), hint;
        const int fp = proc.currentFactoryPreset();
        if (isPositiveAndBelow (fp, (int) factoryPresets().size())) hint = String (CharPointer_UTF8 (factoryPresets()[(size_t) fp].hint));
        if (proc.showResonances && ! analyzer.resonances().empty())
        {
            const int n = (int) analyzer.resonances().size();
            title = n > 1 ? String (n) + String::fromUTF8 (" résonances repérées") : String::fromUTF8 ("1 résonance repérée");
            hint = String::fromUTF8 ("Cliquez un point orange sur la courbe pour l'atténuer, ou tout corriger d'un coup.");
        }
        g.setColour (colours::cream);
        g.setFont (fonts.ui (16.0f, true));
        g.drawText (title, Rectangle<float> (28.0f, 26.0f, (float) getWidth() - 300.0f, 22.0f), Justification::centredLeft);
        g.setColour (colours::muted);
        g.setFont (fonts.ui (13.5f, false));
        g.drawFittedText (hint, Rectangle<int> (28, fixAllButton.isVisible() ? 50 : 54, getWidth() - 340, 20), Justification::centredLeft, 1);
        g.setColour (colours::muted.withAlpha (0.75f));
        g.setFont (fonts.ui (12.5f, false));
        if (! fixAllButton.isVisible())
            g.drawText (String::fromUTF8 ("Double-clic : ajouter une bande   ·   glisser : régler   ·   molette : largeur   ·   Maj : réglage fin"),
                        Rectangle<float> (28.0f, 92.0f, (float) getWidth() - 340.0f, 18.0f), Justification::centredLeft);
    }

    // Sortie, séparée par un filet.
    const float sepX = (float) output.slider.getX() - 34.0f;
    g.setColour (Colours::white.withAlpha (0.06f));
    g.fillRect (sepX, 22.0f, 1.0f, 96.0f);
    paintKnobText (g, output, formatGain ((float) output.slider.getValue()), true);
}

void BandStrip::showTypeMenu()
{
    if (selection.band >= 0) curve.showBandMenu (selection.band, &typeButton);
}

void BandStrip::showSlopeMenu()
{
    const int b = selection.band;
    if (b < 0) return;
    PopupMenu m;
    const auto names = slopeNames();
    for (int i = 0; i < 4; ++i) m.addItem (1 + i, names[i], true, shown.slope == i);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&slopeButton).withParentComponent (getTopLevelComponent()),
                     [this, b] (int r) { if (r > 0) proc.setBandValue (b, "slope", (float) (r - 1)); });
}

// ---------------------------------------------------------------- éditeur

static constexpr int headerHeight = 66, stripHeight = 150;

Editor::Editor (Processor& p)
    : AudioProcessorEditor (p), proc (p),
      curve (p, analyzer, fonts, selection),
      strip (p, analyzer, fonts, selection, curve)
{
    fonts.regular = Typeface::createSystemTypefaceFor (BinaryData::geist_regular_ttf, BinaryData::geist_regular_ttfSize);
    fonts.semibold = Typeface::createSystemTypefaceFor (BinaryData::geist_semibold_ttf, BinaryData::geist_semibold_ttfSize);
    logo = ImageCache::getFromMemory (BinaryData::swell_mark_png, BinaryData::swell_mark_pngSize);
    setLookAndFeel (&lnf);

    selection.changed = [this] { strip.bind(); curve.repaint(); };
    analyzer.setSampleRate (proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0);

    addAndMakeVisible (curve);
    addAndMakeVisible (strip);

    presetButton.painter = [this] (Graphics& g, Rectangle<float> r, bool over, bool) {
        g.setColour (Colours::white.withAlpha (over ? 0.06f : 0.035f));
        g.fillRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f);
        g.setColour (Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);
        const auto category = proc.currentPresetCategory().toUpperCase();
        const auto name = proc.currentPresetName();
        const auto catFont = fonts.ui (11.5f, true), nameFont = fonts.ui (15.0f, true);
        const float cw = category.isEmpty() ? 0.0f : textWidth (catFont, category) + 10.0f;
        const float nw = textWidth (nameFont, name);
        float x = r.getCentreX() - (cw + nw + 18.0f) * 0.5f;
        if (cw > 0.0f)
        {
            g.setColour (colours::accent);
            g.setFont (catFont);
            g.drawText (category, Rectangle<float> (x, r.getY(), cw, r.getHeight()).translated (0.0f, 0.5f), Justification::centredLeft);
            x += cw;
        }
        g.setColour (colours::cream);
        g.setFont (nameFont);
        g.drawText (name, Rectangle<float> (x, r.getY(), nw + 2.0f, r.getHeight()), Justification::centredLeft);
        drawChevron (g, { x + nw + 12.0f, r.getCentreY() + 1.0f }, 4.0f, 2, colours::cream.withAlpha (0.8f), 1.5f);
    };
    presetButton.onClick = [this] { showPresetMenu(); };
    prev.painter = [] (Graphics& g, Rectangle<float> r, bool over, bool) { drawChevron (g, r.getCentre(), 6.0f, -1, over ? colours::cream : colours::cream.withAlpha (0.7f), 1.8f); };
    next.painter = [] (Graphics& g, Rectangle<float> r, bool over, bool) { drawChevron (g, r.getCentre(), 6.0f, 1, over ? colours::cream : colours::cream.withAlpha (0.7f), 1.8f); };
    prev.onClick = [this] { proc.stepPreset (-1); selection.set (-1); };
    next.onClick = [this] { proc.stepPreset (1); selection.set (-1); };

    resonanceButton.painter = [this] (Graphics& g, Rectangle<float> r, bool over, bool) {
        const bool on = proc.showResonances;
        g.setColour (on ? colours::alert : Colours::white.withAlpha (over ? 0.10f : 0.06f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        auto dot = Rectangle<float> (8.0f, 8.0f).withCentre ({ r.getX() + 17.0f, r.getCentreY() });
        g.setColour (on ? colours::ink : colours::alert);
        g.fillEllipse (dot);
        g.setColour (on ? colours::ink : colours::cream);
        g.setFont (fonts.ui (13.5f, true));
        g.drawText (String::fromUTF8 ("Résonances"), r.withTrimmedLeft (28.0f), Justification::centredLeft);
    };
    resonanceButton.setTooltip (String::fromUTF8 ("Repère les fréquences qui résonnent dans le son et propose de les atténuer"));
    resonanceButton.onClick = [this] {
        proc.showResonances = ! proc.showResonances;
        analyzer.clearAverage();
        resonanceButton.repaint(); curve.repaint(); strip.refresh(); strip.repaint();
    };
    gear.painter = [] (Graphics& g, Rectangle<float> r, bool over, bool) { drawGear (g, r.withSizeKeepingCentre (20.0f, 20.0f), over ? colours::cream : colours::cream.withAlpha (0.75f)); };
    gear.onClick = [this] { showSettingsMenu(); };

    for (auto* b : { &presetButton, &prev, &next, &resonanceButton, &gear })
    {
        b->setMouseCursor (MouseCursor::PointingHandCursor);
        addAndMakeVisible (b);
    }

    setResizable (true, true);
    setResizeLimits (900, 560, 1900, 1150);
    setSize (proc.editorWidth >= 900 ? proc.editorWidth : 1120, proc.editorHeight >= 560 ? proc.editorHeight : 680);
    proc.analyserWanted.store (true);
    lastTick = Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

Editor::~Editor()
{
    proc.analyserWanted.store (false);
    stopTimer();
    setLookAndFeel (nullptr);
}

void Editor::paint (Graphics& g)
{
    g.fillAll (colours::background);
    if (logo.isValid())
        g.drawImage (logo, Rectangle<float> (26.0f, 17.0f, 32.0f, 32.0f), RectanglePlacement::centred);
    g.setColour (colours::cream);
    auto brand = fonts.ui (15.0f, true).withExtraKerningFactor (0.35f);
    g.setFont (brand);
    g.drawText ("SWELL", Rectangle<float> (68.0f, 20.0f, 100.0f, 26.0f), Justification::centredLeft);
    g.setColour (colours::muted);
    g.setFont (fonts.ui (15.0f, false));
    g.drawText ("EQ", Rectangle<float> (150.0f, 20.0f, 60.0f, 26.0f), Justification::centredLeft);

    // Niveau de sortie : fine rangée de traits.
    const float mx = (float) gear.getX() - 92.0f, my = 27.0f;
    const float level = jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (meter, -60.0f) + 60.0f) / 60.0f);
    for (int i = 0; i < 16; ++i)
    {
        const bool lit = (float) i / 16.0f < level;
        const Colour c = i >= 14 ? colours::alert : (i >= 11 ? colours::accent : colours::cream);
        g.setColour (lit ? c.withAlpha (0.9f) : Colours::white.withAlpha (0.10f));
        g.fillRoundedRectangle (mx + (float) i * 4.5f, my, 2.0f, 12.0f, 1.0f);
    }
}

void Editor::resized()
{
    const int w = getWidth(), h = getHeight();
    proc.editorWidth = w;
    proc.editorHeight = h;
    auto pill = Rectangle<int> (440, 42).withCentre ({ w / 2, headerHeight / 2 });
    presetButton.setBounds (pill);
    prev.setBounds (pill.getX() + 4, pill.getY() + 1, 40, 40);
    next.setBounds (pill.getRight() - 44, pill.getY() + 1, 40, 40);
    gear.setBounds (w - 58, 15, 36, 36);
    resonanceButton.setBounds (w - 58 - 100 - 12 - 132, 17, 132, 32);
    curve.setBounds (0, headerHeight, w, h - headerHeight - stripHeight);
    strip.setBounds (0, h - stripHeight, w, stripHeight);
}

void Editor::pump (float dt)
{
    analyzer.setSampleRate (proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0);
    analyzer.update (proc.preFifo, proc.postFifo, dt);
    strip.refresh();
    meter = proc.outputPeak.load();
    presetButton.repaint();
    repaint();
    curve.repaint();
}

void Editor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    const float dt = (float) jlimit (0.0, 0.2, (now - lastTick) / 1000.0);
    lastTick = now;
    proc.analyserWanted.store (proc.analyserMode != 2 || proc.showResonances);
    analyzer.setSampleRate (proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0);
    analyzer.update (proc.preFifo, proc.postFifo, dt);
    strip.refresh();
    curve.repaint();

    const auto preset = proc.currentPresetCategory() + "|" + proc.currentPresetName();
    if (preset != shownPreset) { shownPreset = preset; presetButton.repaint(); strip.repaint(); }

    const float m = proc.outputPeak.load();
    if (std::abs (m - meter) > 0.002f)
    {
        meter = m;
        repaint (gear.getX() - 100, 20, 90, 26);
    }
}

void Editor::showPresetMenu()
{
    const auto& bank = factoryPresets();
    PopupMenu m;
    m.addItem (1, String (CharPointer_UTF8 (bank[0].name)), true, proc.currentFactoryPreset() == 0);
    StringArray categories;
    for (auto& p : bank) if (*p.category != 0) categories.addIfNotAlreadyThere (String (CharPointer_UTF8 (p.category)));
    for (auto& cat : categories)
    {
        PopupMenu sub;
        for (int i = 0; i < (int) bank.size(); ++i)
            if (String (CharPointer_UTF8 (bank[(size_t) i].category)) == cat)
                sub.addItem (1 + i, String (CharPointer_UTF8 (bank[(size_t) i].name)), true, proc.currentFactoryPreset() == i);
        m.addSubMenu (cat, sub, true, nullptr, cat == proc.currentPresetCategory());
    }
    m.addSeparator();
    const auto user = proc.userPresets();
    PopupMenu mine;
    for (int i = 0; i < user.size(); ++i)
        mine.addItem (1000 + i, user[i].getFileNameWithoutExtension(), true,
                      proc.currentFactoryPreset() < 0 && proc.currentPresetName() == user[i].getFileNameWithoutExtension());
    if (user.isEmpty()) mine.addItem (999, "Aucun pour l'instant", false);
    m.addSubMenu ("Mes presets", mine);
    m.addItem (2000, "Enregistrer le preset...");
    m.addItem (2001, "Ouvrir le dossier des presets");

    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetButton).withParentComponent (this).withMinimumWidth (presetButton.getWidth()),
        [this, user] (int r) {
            if (r <= 0) return;
            if (r < 1000) { proc.loadFactoryPreset (r - 1); selection.set (-1); }
            else if (r < 2000 && r - 1000 < user.size()) { proc.loadUserPreset (user[r - 1000]); selection.set (-1); }
            else if (r == 2000) askPresetName();
            else if (r == 2001) { Processor::userPresetFolder().createDirectory(); Processor::userPresetFolder().startAsProcess(); }
            presetButton.repaint(); strip.repaint();
        });
}

void Editor::askPresetName()
{
    auto* w = new AlertWindow ("Enregistrer le preset", "Nom du preset :", MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel (&lnf);
    w->addTextEditor ("name", proc.currentFactoryPreset() == 0 ? String() : proc.currentPresetName());
    w->addButton ("Enregistrer", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("Annuler", 0, KeyPress (KeyPress::escapeKey));
    SafePointer<Editor> safe (this);
    w->enterModalState (true, ModalCallbackFunction::create ([safe, w] (int result) {
        if (result == 1 && safe != nullptr)
        {
            const auto name = w->getTextEditorContents ("name");
            if (! safe->proc.saveUserPreset (name))
                AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Swell EQ", String::fromUTF8 ("Le preset n'a pas pu être enregistré."));
            safe->presetButton.repaint();
            safe->strip.repaint();
        }
    }), true);
}

void Editor::showSettingsMenu()
{
    PopupMenu m;
    m.addSectionHeader ("Analyseur");
    m.addItem (1, String::fromUTF8 ("Avant et après l'égaliseur"), true, proc.analyserMode == 0);
    m.addItem (2, String::fromUTF8 ("Après seulement"), true, proc.analyserMode == 1);
    m.addItem (3, String::fromUTF8 ("Désactivé"), true, proc.analyserMode == 2);
    m.addSectionHeader (String::fromUTF8 ("Échelle de la courbe"));
    m.addItem (11, String::fromUTF8 ("± 6 dB"), true, proc.displayRange == 6);
    m.addItem (12, String::fromUTF8 ("± 12 dB"), true, proc.displayRange == 12);
    m.addItem (13, String::fromUTF8 ("± 24 dB"), true, proc.displayRange == 24);
    m.addSeparator();
    m.addItem (21, String::fromUTF8 ("Tout remettre à plat"));
    m.addSeparator();
    m.addItem (99, "Swell EQ " + String (JucePlugin_VersionString), false);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&gear).withParentComponent (this), [this] (int r) {
        if (r >= 1 && r <= 3) proc.analyserMode = r - 1;
        else if (r == 11) proc.displayRange = 6;
        else if (r == 12) proc.displayRange = 12;
        else if (r == 13) proc.displayRange = 24;
        else if (r == 21) { proc.resetFlat(); selection.set (-1); }
        curve.repaint(); strip.repaint(); presetButton.repaint();
    });
}

} // namespace swelleq
