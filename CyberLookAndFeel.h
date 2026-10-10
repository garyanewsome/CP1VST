#pragma once

#include <JuceHeader.h>

//==============================================================================
// Cyberpunk look and feel: neon cyan/magenta on near-black, blade-style faders.
//==============================================================================
class CyberLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Palette shared with the editor.
    static inline const juce::Colour background { 0xff07070d };
    static inline const juce::Colour panel      { 0xff0d0d18 };
    static inline const juce::Colour cyan       { 0xff00f0ff };
    static inline const juce::Colour magenta    { 0xffff2bd6 };
    // Sliders carrying this property are drawn as performance wheels.
    static inline const juce::Identifier wheelProperty { "cyberWheel" };

    static inline const juce::Colour dimText    { 0xff8a93b8 };

    CyberLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::Label::textColourId, cyan.withAlpha (0.9f));
        setColour (juce::Slider::textBoxTextColourId, cyan);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha (0.4f));
        setColour (juce::Slider::textBoxOutlineColourId, cyan.withAlpha (0.25f));
        setColour (juce::Slider::textBoxHighlightColourId, magenta.withAlpha (0.5f));
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf0101024));
        setColour (juce::TooltipWindow::textColourId, juce::Colours::white);
        setColour (juce::TooltipWindow::outlineColourId, cyan);
        setColour (juce::TextButton::buttonColourId, panel);
        setColour (juce::TextButton::textColourOffId, magenta);
        setColour (juce::TextButton::textColourOnId, cyan);
    }

    static juce::Font monoFont (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions ("Menlo", height,
                                              bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::Font getLabelFont (juce::Label& l) override
    {
        // Value read-outs sit in a smaller face so units fit the column width.
        const bool isSliderText = dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr;
        return monoFont (isSliderText ? 11.0f : 12.0f, true);
    }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return monoFont (12.0f, true); }

    //==========================================================================
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float /*minPos*/, float /*maxPos*/,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0, 0, style, slider);
            return;
        }

        if (slider.getProperties()[wheelProperty])
        {
            drawWheel (g, x, y, width, height, slider);
            return;
        }

        const bool active = slider.isMouseOverOrDragging();
        const float cx = (float) x + (float) width * 0.5f;
        const float capH = 18.0f, capW = juce::jmin (38.0f, (float) width - 6.0f);

        // The cap travels inside the track so its edges never leave the slider bounds.
        const float top = (float) y + capH * 0.5f;
        const float bottom = (float) y + (float) height - capH * 0.5f;
        const float pos = juce::jlimit (top, bottom, sliderPos);

        // Tick marks: one per detent on stepped controls (e.g. waveform
        // selectors), otherwise 10 divisions with longer ticks at 0/50/100%.
        int divisions = 10;
        const double interval = slider.getInterval();
        if (interval > 0.0)
        {
            const int steps = juce::roundToInt ((slider.getMaximum() - slider.getMinimum()) / interval);
            if (steps >= 1 && steps <= 12)
                divisions = steps;
        }
        const bool stepped = divisions != 10;

        for (int i = 0; i <= divisions; ++i)
        {
            const float ty = bottom - (bottom - top) * (float) i / (float) divisions;
            const bool major = stepped || (i % 5 == 0);
            const float len = major ? 9.0f : 5.0f;
            g.setColour (dimText.withAlpha (major ? 0.8f : 0.4f));
            g.fillRect (cx - capW * 0.5f - 4.0f - len, ty - 0.5f, len, 1.0f);
            g.fillRect (cx + capW * 0.5f + 4.0f,       ty - 0.5f, len, 1.0f);
        }

        // Recessed slot.
        const juce::Rectangle<float> slot (cx - 3.0f, top - 4.0f, 6.0f, bottom - top + 8.0f);
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (slot, 3.0f);
        g.setColour (cyan.withAlpha (0.18f));
        g.drawRoundedRectangle (slot, 3.0f, 1.0f);

        // Lit fill from the bottom up to the cap, with a soft glow.
        const juce::Rectangle<float> fill (cx - 1.5f, pos, 3.0f, bottom - pos + 2.0f);
        juce::ColourGradient grad (magenta, cx, bottom, cyan, cx, top, false);
        for (float spread : { 6.0f, 3.0f })
        {
            g.setGradientFill (grad);
            g.setOpacity (active ? 0.18f : 0.10f);
            g.fillRoundedRectangle (fill.expanded (spread * 0.5f, 0.0f), 3.0f);
        }
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, 1.5f);

        // Blade cap: chamfered metal body with ridges and a lit centre line.
        const juce::Rectangle<float> cap (cx - capW * 0.5f, pos - capH * 0.5f, capW, capH);
        const float ch = 4.0f; // chamfer
        juce::Path body;
        body.startNewSubPath (cap.getX() + ch, cap.getY());
        body.lineTo (cap.getRight() - ch, cap.getY());
        body.lineTo (cap.getRight(), cap.getY() + ch);
        body.lineTo (cap.getRight(), cap.getBottom() - ch);
        body.lineTo (cap.getRight() - ch, cap.getBottom());
        body.lineTo (cap.getX() + ch, cap.getBottom());
        body.lineTo (cap.getX(), cap.getBottom() - ch);
        body.lineTo (cap.getX(), cap.getY() + ch);
        body.closeSubPath();

        g.setColour ((active ? magenta : cyan).withAlpha (0.20f));
        g.strokePath (body, juce::PathStrokeType (5.0f));      // outer glow
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3d52), cap.getX(), cap.getY(),
                                                 juce::Colour (0xff14151f), cap.getX(), cap.getBottom(), false));
        g.fillPath (body);
        g.setColour (active ? magenta : cyan);
        g.strokePath (body, juce::PathStrokeType (1.2f));

        // Grip ridges either side of the centre line.
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        for (int i = 1; i <= 3; ++i)
        {
            const float rx = capW * 0.5f - 4.0f - (float) i * 4.0f;
            g.fillRect (cx - rx - 1.0f, cap.getY() + 3.0f, 1.0f, capH - 6.0f);
            g.fillRect (cx + rx,        cap.getY() + 3.0f, 1.0f, capH - 6.0f);
        }
        g.setColour (juce::Colours::white);
        g.fillRect (cx - 0.75f, cap.getY() + 2.0f, 1.5f, capH - 4.0f);
    }

    // A ribbed thumb-wheel seen edge-on in a housing: cylinder shading,
    // ribs that scroll with the value, and a centre detent mark if bipolar.
    void drawWheel (juce::Graphics& g, int x, int y, int width, int height, juce::Slider& slider)
    {
        const bool active = slider.isMouseOverOrDragging();
        const auto accent = active ? magenta : cyan;
        const bool bipolar = slider.getMinimum() < 0.0;
        const float norm = (float) slider.valueToProportionOfLength (slider.getValue());

        const float w = juce::jmin (46.0f, (float) width - 8.0f);
        const float cx = (float) x + (float) width * 0.5f;
        const juce::Rectangle<float> housing (cx - w * 0.5f, (float) y + 4.0f, w, (float) height - 14.0f);

        // Housing: dark pocket with a glowing rim.
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (housing.expanded (4.0f, 3.0f), 8.0f);
        g.setColour (accent.withAlpha (0.25f));
        g.drawRoundedRectangle (housing.expanded (4.0f, 3.0f), 8.0f, 2.0f);

        // Wheel body: horizontal cylinder shading (dark edges, bright core).
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip;
            clip.addRoundedRectangle (housing, 6.0f);
            g.reduceClipRegion (clip);

            juce::ColourGradient cyl (juce::Colour (0xff05060c), housing.getX(), 0,
                                      juce::Colour (0xff05060c), housing.getRight(), 0, false);
            cyl.addColour (0.25, juce::Colour (0xff2a2e4a));
            cyl.addColour (0.5, juce::Colour (0xff4a4f78));
            cyl.addColour (0.75, juce::Colour (0xff2a2e4a));
            g.setGradientFill (cyl);
            g.fillRect (housing);

            // Ribs scroll with the value so the wheel visibly "rotates".
            const float pitch = 9.0f;
            const float travel = norm * pitch * 14.0f;
            for (float ry = housing.getY() - pitch + std::fmod (travel, pitch);
                 ry < housing.getBottom(); ry += pitch)
            {
                g.setColour (juce::Colours::black.withAlpha (0.55f));
                g.fillRect (housing.getX(), ry, housing.getWidth(), 2.0f);
                g.setColour (accent.withAlpha (0.28f));
                g.fillRect (housing.getX(), ry + 2.0f, housing.getWidth(), 1.0f);
            }

            // Top/bottom roll-off so it reads as a curved surface.
            juce::ColourGradient fade (juce::Colours::black.withAlpha (0.85f), 0, housing.getY(),
                                       juce::Colours::transparentBlack, 0, housing.getY() + housing.getHeight() * 0.28f, false);
            g.setGradientFill (fade);
            g.fillRect (housing);
            juce::ColourGradient fade2 (juce::Colours::black.withAlpha (0.85f), 0, housing.getBottom(),
                                        juce::Colours::transparentBlack, 0, housing.getBottom() - housing.getHeight() * 0.28f, false);
            g.setGradientFill (fade2);
            g.fillRect (housing);
        }

        // Position pointer: a lit window slot that tracks the value.
        const float py = housing.getBottom() - 8.0f - norm * (housing.getHeight() - 16.0f);
        g.setColour (accent.withAlpha (0.3f));
        g.fillRoundedRectangle (cx - w * 0.5f - 6.0f, py - 3.0f, w + 12.0f, 6.0f, 3.0f);
        g.setColour (accent);
        g.fillRect (cx - w * 0.5f - 6.0f, py - 1.0f, 8.0f, 2.0f);
        g.fillRect (cx + w * 0.5f - 2.0f, py - 1.0f, 8.0f, 2.0f);

        // Bipolar centre detent marker (pitch wheel rest position).
        if (bipolar)
        {
            const float my = housing.getCentreY();
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            juce::Path tri;
            tri.addTriangle (housing.getX() - 9.0f, my - 4.0f, housing.getX() - 9.0f, my + 4.0f,
                             housing.getX() - 2.0f, my);
            tri.addTriangle (housing.getRight() + 9.0f, my - 4.0f, housing.getRight() + 9.0f, my + 4.0f,
                             housing.getRight() + 2.0f, my);
            g.fillPath (tri);
        }
    }

    //==========================================================================
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const auto c = down ? cyan : magenta;
        g.setColour (c.withAlpha (highlighted || down ? 0.25f : 0.10f));
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (c);
        g.drawRoundedRectangle (r, 2.0f, 1.2f);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height);
        g.setColour (juce::Colour (0xf0101024));
        g.fillRect (r);
        g.setColour (cyan);
        g.drawRect (r, 1.0f);
        g.setColour (magenta);
        g.fillRect (r.removeFromLeft (3.0f));

        g.setColour (juce::Colours::white);
        g.setFont (monoFont (12.0f));
        g.drawFittedText (text, r.toNearestInt().reduced (8, 4), juce::Justification::centredLeft, 4);
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override
    {
        const int w = juce::jmin (260, parentArea.getWidth() - 10);
        juce::AttributedString s;
        s.append (tipText, monoFont (12.0f));
        juce::TextLayout tl;
        tl.createLayout (s, (float) (w - 16));
        const int h = (int) tl.getHeight() + 12;
        return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - w - 12 : screenPos.x + 24,
                                     screenPos.y > parentArea.getCentreY() ? screenPos.y - h - 6 : screenPos.y + 6,
                                     w, h)
                   .constrainedWithin (parentArea);
    }
};
