#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Presets.h"

namespace {
constexpr int kBaseWidth = 1500;
constexpr int kBaseHeight = 470;
constexpr int kHeaderHeight = 56;
constexpr int kKeyboardHeight = 80;

juce::String waveName (int index) {
  static const char *names[] = {"SINE", "SAW", "TRI", "WHITE", "PINK"};
  return names[juce::jlimit (0, 4, index)];
}

// Parse a user-typed value: leading number, with optional unit multipliers.
double leadingNumber(const juce::String &t) {
  return t.retainCharacters("+-.0123456789").getDoubleValue();
}

// Value read-out formatting. Each control gets text <-> value converters so the
// box shows units and typed entry understands them.
using ToText = std::function<juce::String(double)>;
using FromText = std::function<double(const juce::String &)>;

ToText percentText() {
  return [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + " %"; };
}
FromText percentFrom() {
  return [](const juce::String &t) { return leadingNumber(t) / 100.0; };
}
ToText timeText() {
  return [](double v) {
    return v < 1.0 ? juce::String(juce::roundToInt(v * 1000.0)) + " ms"
                   : juce::String(v, 2) + " s";
  };
}
FromText timeFrom() {
  return [](const juce::String &t) {
    const double n = leadingNumber(t);
    return t.containsIgnoreCase("ms") ? n / 1000.0 : n;
  };
}
ToText cutoffText() {
  return [](double v) {
    return v < 1000.0 ? juce::String(juce::roundToInt(v)) + " Hz"
                      : juce::String(v / 1000.0, 2) + " kHz";
  };
}
FromText cutoffFrom() {
  return [](const juce::String &t) {
    const double n = leadingNumber(t);
    return t.containsIgnoreCase("k") ? n * 1000.0 : n;
  };
}
ToText hzText() {
  return [](double v) { return juce::String(v, 2) + " Hz"; };
}
ToText driveText() {
  return [](double v) { return juce::String(v, 1) + " x"; };
}
ToText semitoneText() {
  return [](double v) {
    return (v > 0.0 ? "+" : "") + juce::String(v, 2) + " st";
  };
}
ToText dbText() {
  return [](double v) {
    return v <= 0.0001 ? juce::String("-inf dB")
                       : juce::String(juce::Decibels::gainToDecibels(v), 1) + " dB";
  };
}
FromText dbFrom() {
  return [](const juce::String &t) {
    return t.containsIgnoreCase("inf")
               ? 0.0
               : (double)juce::Decibels::decibelsToGain(leadingNumber(t));
  };
}
} // namespace

//==============================================================================
CP1AudioProcessorEditor::CP1AudioProcessorEditor(
    CP1AudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      midiKeyboard(p.keyboardState,
                   juce::MidiKeyboardComponent::horizontalKeyboard) {
  setLookAndFeel(&lookAndFeel);
  presetBox.setColour(juce::ComboBox::backgroundColourId, CyberLookAndFeel::panel);
  presetBox.setColour(juce::ComboBox::outlineColourId, CyberLookAndFeel::cyan.withAlpha(0.6f));
  presetBox.setColour(juce::ComboBox::textColourId, CyberLookAndFeel::cyan);
  presetBox.setColour(juce::ComboBox::arrowColourId, CyberLookAndFeel::magenta);
  lookAndFeel.setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff0d0d18));
  lookAndFeel.setColour(juce::PopupMenu::textColourId, CyberLookAndFeel::cyan);
  lookAndFeel.setColour(juce::PopupMenu::highlightedBackgroundColourId, CyberLookAndFeel::magenta.withAlpha(0.35f));
  lookAndFeel.setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

  addControl(osc1Wave, "WAVEFORM", "OSC 1",
             "Oscillator 1 source: sine (pure), saw (bright and buzzy), "
             "triangle (soft, hollow), white noise (hiss) or pink noise "
             "(softer, rumbly hiss).");
  addControl(osc2Wave, "OSC2_WAVEFORM", "OSC 2",
             "Oscillator 2 source: sine, saw, triangle, white or pink noise. "
             "Blend it with OSC 1 using MIX. Noise ignores pitch and detune.");
  addControl(oscMix, "OSC_MIX", "MIX",
             "Balance between the two oscillators. Down is all OSC 1, up is "
             "all OSC 2.");
  addControl(oscDetune, "OSC2_DETUNE", "DETUNE",
             "Pitch offset of OSC 2 in semitones (-12 to +12). Small values "
             "give thick, chorused tone.");

  addControl(subLevel, "SUB_LEVEL", "SUB",
             "Sub oscillator: a pure sine one octave below OSC 1, for weight "
             "and low end.");

  addControl(attack, "ATTACK", "ATK",
             "Attack: how long the note takes to rise from silence to full "
             "level.");
  addControl(decay, "DECAY", "DEC",
             "Decay: how long it takes to fall from the peak to the sustain "
             "level.");
  addControl(sustain, "SUSTAIN", "SUS",
             "Sustain: the level held while a key stays down.");
  addControl(release, "RELEASE", "REL",
             "Release: how long the sound fades after the key is released.");

  addControl(cutoff, "CUTOFF", "CUTOFF",
             "Filter cutoff: frequencies above this are removed by the "
             "low-pass ladder filter. Lower is darker.");
  addControl(resonance, "RESONANCE", "RES",
             "Resonance: boosts frequencies around the cutoff for a sharper, "
             "more vocal sweep. High values may self-oscillate.");

  addControl(filterType, "FILTER_TYPE", "TYPE",
             "Filter type: LP12 and LP24 are low-pass (12 / 24 dB per "
             "octave), BP keeps a band around the cutoff, HP removes the "
             "lows.");
  addControl(glide, "GLIDE", "GLIDE",
             "Glide: time for a voice to slide from its previous note to the "
             "new one. Zero is off.");

  addControl(uniVoices, "UNISON_VOICES", "UNI",
             "Unison: number of stacked, slightly detuned copies of each "
             "oscillator (1 to 5) for a thicker sound.");
  addControl(uniDetune, "UNISON_DETUNE", "SPRD",
             "Unison detune: how far apart the stacked voices are tuned, up "
             "to half a semitone either side.");
  addControl(uniSpread, "UNISON_SPREAD", "WIDTH",
             "Unison width: how far the stacked voices are panned across the "
             "stereo field.");

  addControl(fenvAmt, "FENV_AMOUNT", "AMT",
             "Filter envelope amount: how far the envelope pushes the cutoff, "
             "up to 5 octaves. Negative values sweep it down. Lower the "
             "CUTOFF first, or there is no headroom to open.");
  addControl(fenvA, "FENV_ATTACK", "ATK",
             "Filter attack: how long the cutoff takes to reach its peak "
             "after a key is pressed.");
  addControl(fenvD, "FENV_DECAY", "DEC",
             "Filter decay: how long the cutoff takes to fall from its peak "
             "to the sustain level.");
  addControl(fenvS, "FENV_SUSTAIN", "SUS",
             "Filter sustain: the envelope level held while a key stays "
             "down.");
  addControl(fenvR, "FENV_RELEASE", "REL",
             "Filter release: how long the cutoff takes to return after the "
             "key is released.");

  addControl(lfoRate, "LFO_RATE", "RATE",
             "LFO rate: speed of the low-frequency oscillator that sweeps the "
             "filter cutoff, in Hz.");
  addControl(lfoDepth, "LFO_DEPTH", "DEPTH",
             "LFO depth: how far the LFO moves its destination. The mod "
             "wheel adds on top of this.");
  addControl(lfoDest, "LFO_DEST", "DEST",
             "LFO destination: filter cutoff (wobble), pitch (vibrato) or "
             "both.");

  addControl(drive, "DISTORTION", "DRIVE",
             "Drive: amount of tanh saturation. Low is clean, high is warm "
             "and gritty.");
  addControl(masterVol, "MASTER_VOL", "VOL", "Master output volume.");

  // Waveform sliders drive choice parameters: show the wave name, not a number.
  for (auto *c : {&osc1Wave, &osc2Wave}) {
    c->slider.textFromValueFunction = [](double v) {
      return waveName((int)std::round(v));
    };
    c->slider.updateText();
  }

  lfoDest.slider.textFromValueFunction = [](double v) {
    static const char *names[] = {"CUTOFF", "PITCH", "BOTH"};
    return juce::String(names[juce::jlimit(0, 2, (int)std::round(v))]);
  };
  lfoDest.slider.updateText();

  auto format = [](Control &c, ToText to, FromText from = nullptr) {
    c.slider.textFromValueFunction = std::move(to);
    if (from)
      c.slider.valueFromTextFunction = std::move(from);
    c.slider.updateText();
  };
  filterType.slider.textFromValueFunction = [](double v) {
    static const char *names[] = {"LP12", "LP24", "BP", "HP"};
    return juce::String(names[juce::jlimit(0, 3, (int)std::round(v))]);
  };
  filterType.slider.updateText();
  uniVoices.slider.textFromValueFunction = [](double v) {
    return juce::String(juce::jlimit(1, 5, (int)std::round(v) + 1));
  };
  uniVoices.slider.updateText();
  format(uniDetune, percentText(), percentFrom());
  format(uniSpread, percentText(), percentFrom());
  format(subLevel, percentText(), percentFrom());
  format(glide, [](double v) {
    return v <= 0.0005 ? juce::String("OFF")
         : v < 1.0 ? juce::String(juce::roundToInt(v * 1000.0)) + " ms"
                   : juce::String(v, 2) + " s";
  }, timeFrom());
  format(oscMix, percentText(), percentFrom());
  format(oscDetune, semitoneText());
  format(attack, timeText(), timeFrom());
  format(decay, timeText(), timeFrom());
  format(sustain, percentText(), percentFrom());
  format(release, timeText(), timeFrom());
  format(cutoff, cutoffText(), cutoffFrom());
  format(resonance, percentText(), percentFrom());
  format(fenvAmt, percentText(), percentFrom());
  format(fenvA, timeText(), timeFrom());
  format(fenvD, timeText(), timeFrom());
  format(fenvS, percentText(), percentFrom());
  format(fenvR, timeText(), timeFrom());
  format(lfoRate, hzText());
  format(lfoDepth, percentText(), percentFrom());
  format(drive, driveText());
  format(masterVol, dbText(), dbFrom());

  sections = {
      {"PERF", {&pitchCtl, &modCtl}, {}},
      {"OSC", {&osc1Wave, &osc2Wave, &oscMix, &oscDetune, &subLevel}, {}},
      {"ENV", {&attack, &decay, &sustain, &release}, {}},
      {"FILTER", {&cutoff, &resonance, &filterType}, {}},
      {"F.ENV", {&fenvAmt, &fenvA, &fenvD, &fenvS, &fenvR}, {}},
      {"MOD", {&lfoRate, &lfoDepth, &lfoDest}, {}},
      {"VOICE", {&glide, &uniVoices, &uniDetune, &uniSpread}, {}},
      {"OUT", {&drive, &masterVol}, {}},
  };

  randomizeButton.setButtonText("RANDOMIZE");
  randomizeButton.setTooltip(
      "Randomize every sound parameter. Master volume and detune are left "
      "alone.");
  randomizeButton.onClick = [this] {
    for (auto *param : audioProcessor.getParameters()) {
      if (auto *rangedId =
              dynamic_cast<juce::AudioProcessorParameterWithID *>(param)) {
        if (rangedId->paramID != "MASTER_VOL" &&
            rangedId->paramID != "OSC2_DETUNE") {
          rangedId->setValueNotifyingHost(
              juce::Random::getSystemRandom().nextFloat());
        }
      }
    }
  };
  addAndMakeVisible(randomizeButton);

  // Preset browser.
  presetBox.setTextWhenNothingSelected("- PRESET -");
  presetBox.setTooltip("Choose a factory or saved preset.");
  presetBox.onChange = [this] {
    // Remember the choice in the plugin state so it survives the window being closed and reopened.
    audioProcessor.apvts.state.setProperty("presetName", presetBox.getItemText(presetBox.getSelectedItemIndex()), nullptr);
    const int id = presetBox.getSelectedId();
    const int nFactory = (int)Presets::factory().size();
    if (id >= 1 && id <= nFactory)
      Presets::applyFactory(audioProcessor, id - 1);
    else if (id > 1000 && id - 1001 < userPresetFiles.size())
      Presets::loadUser(audioProcessor, userPresetFiles[id - 1001]);
  };
  addAndMakeVisible(presetBox);

  prevPresetButton.setButtonText("<");
  nextPresetButton.setButtonText(">");
  savePresetButton.setButtonText("SAVE");
  prevPresetButton.setTooltip("Previous preset.");
  nextPresetButton.setTooltip("Next preset.");
  savePresetButton.setTooltip(
      "Save the current sound as a preset. Presets are stored in your user "
      "Application Support folder.");
  prevPresetButton.onClick = [this] { stepPreset(-1); };
  nextPresetButton.onClick = [this] { stepPreset(1); };
  savePresetButton.onClick = [this] { promptSavePreset(); };
  addAndMakeVisible(prevPresetButton);
  addAndMakeVisible(nextPresetButton);
  addAndMakeVisible(savePresetButton);
  refreshPresetList();
  {
    // Reselect the remembered preset (the editor is rebuilt every time the window opens).
    const juce::String remembered = audioProcessor.apvts.state.getProperty("presetName").toString();
    if (remembered.isNotEmpty())
      for (int i = 0; i < presetBox.getNumItems(); ++i)
        if (presetBox.getItemText(i) == remembered) {
          presetBox.setSelectedItemIndex(i, juce::dontSendNotification);
          break;
        }
  }

  midiKeyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
                         CyberLookAndFeel::magenta.withAlpha(0.7f));
  midiKeyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
                         CyberLookAndFeel::cyan.withAlpha(0.35f));
  midiKeyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId,
                         juce::Colour(0xff1c1f36));
  midiKeyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId,
                         juce::Colour(0xff000000));
  midiKeyboard.setColour(juce::MidiKeyboardComponent::textLabelColourId,
                         CyberLookAndFeel::dimText);
  midiKeyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId,
                         CyberLookAndFeel::cyan.withAlpha(0.4f));
  midiKeyboard.setAvailableRange(24, 108);
  midiKeyboard.setScrollButtonsVisible(false);
  addAndMakeVisible(midiKeyboard);

  auto setupWheel = [this](Control &c, const juce::String &title, double min,
                           double max, const juce::String &tip) {
    auto &w = c.slider;
    w.setSliderStyle(juce::Slider::LinearVertical);
    w.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    w.setRange(min, max, 0.0);
    w.setTooltip(tip);
    w.getProperties().set(CyberLookAndFeel::wheelProperty, true);
    addAndMakeVisible(w);

    c.label.setText(title, juce::dontSendNotification);
    c.label.setJustificationType(juce::Justification::centred);
    c.label.setTooltip(tip);
    addAndMakeVisible(c.label);
  };
  setupWheel(pitchCtl, "PITCH", -1.0, 1.0,
             "Pitch wheel: bends the pitch up to 2 semitones. Springs back to "
             "centre when released. Hardware wheels work too.");
  setupWheel(modCtl, "MOD", 0.0, 1.0,
             "Mod wheel: adds to the LFO depth (wobble, vibrato or both, per "
             "DEST). Stays where you leave it.");
  pitchWheel.setDoubleClickReturnValue(true, 0.0);
  pitchWheel.setValue(0.0, juce::dontSendNotification);
  pitchWheel.onValueChange = [this] {
    audioProcessor.performance.pitchBend.store((float)pitchWheel.getValue());
  };
  pitchWheel.onDragEnd = [this] { pitchWheel.setValue(0.0); };
  modWheel.onValueChange = [this] {
    audioProcessor.performance.modWheel.store((float)modWheel.getValue());
  };
  modWheel.setValue(audioProcessor.performance.modWheel.load(),
                    juce::dontSendNotification);
  startTimerHz(30); // mirror hardware mod wheel movement on screen

  setResizable(true, true);
  setResizeLimits(kBaseWidth * 3 / 4, kBaseHeight * 3 / 4, kBaseWidth * 2,
                  kBaseHeight * 2);
  getConstrainer()->setFixedAspectRatio((double)kBaseWidth / kBaseHeight);
  setSize(kBaseWidth, kBaseHeight);
}

CP1AudioProcessorEditor::~CP1AudioProcessorEditor() {
  stopTimer();
  setLookAndFeel(nullptr);
}

void CP1AudioProcessorEditor::refreshPresetList(
    const juce::String &selectName) {
  presetBox.clear(juce::dontSendNotification);
  int id = 1;
  for (const auto &p : Presets::factory())
    presetBox.addItem(p.name, id++);

  userPresetFiles = Presets::userPresetFiles();
  if (userPresetFiles.size() > 0) {
    presetBox.addSeparator();
    presetBox.addSectionHeading("USER");
    int uid = 1001;
    for (const auto &f : userPresetFiles) {
      presetBox.addItem(f.getFileNameWithoutExtension(), uid);
      if (selectName.isNotEmpty() &&
          f.getFileNameWithoutExtension() ==
              juce::File::createLegalFileName(selectName))
        presetBox.setSelectedId(uid, juce::dontSendNotification);
      ++uid;
    }
  }
}

void CP1AudioProcessorEditor::stepPreset(int delta) {
  const int count = presetBox.getNumItems();
  if (count == 0)
    return;
  // Items include separator/heading entries; walk to the next real item.
  int index = presetBox.getSelectedItemIndex();
  for (int n = 0; n < count; ++n) {
    index = (index + delta + count) % count;
    if (presetBox.getItemId(index) != 0) {
      presetBox.setSelectedItemIndex(index, juce::sendNotificationSync);
      return;
    }
  }
}

void CP1AudioProcessorEditor::promptSavePreset() {
  auto *w = new juce::AlertWindow("Save preset", "Name this sound:",
                                  juce::MessageBoxIconType::NoIcon, this);
  w->addTextEditor("name", "My Patch");
  w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
  w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
  w->enterModalState(
      true, juce::ModalCallbackFunction::create(
                [this, w](int result) {
                  if (result == 1) {
                    const auto name = w->getTextEditorContents("name").trim();
                    if (name.isNotEmpty() &&
                        Presets::saveUser(audioProcessor, name))
                      { audioProcessor.apvts.state.setProperty("presetName", name, nullptr); refreshPresetList(name); }
                  }
                }),
      true);
}

void CP1AudioProcessorEditor::timerCallback() {
  // Scope: grab the latest samples, trigger on a rising zero crossing so a
  // steady tone sits still on screen.
  {
    constexpr int window = 1200, n = (int)400;
    const unsigned w = audioProcessor.scopeWritePos.load(std::memory_order_acquire);
    const int size = CP1AudioProcessor::scopeSize;
    auto at = [&](int back) {
      return audioProcessor.scopeBuffer[(w - 1u - (unsigned)back) % (unsigned)size]
          .load(std::memory_order_relaxed);
    };
    // Search backwards for a rising crossing leaving 'window' samples after it.
    int start = window;
    for (int b = window; b < size - 1; ++b)
      if (at(b + 1) < 0.0f && at(b) >= 0.0f) { start = b; break; }
    start = juce::jmin(start, size - 1);
    for (int i = 0; i < n; ++i) {
      const int back = start - (i * window / n);
      scopeData[(size_t)i] = back >= 0 ? at(back) : 0.0f;
    }
    repaint(scopeBounds);
  }

  if (!modWheel.isMouseButtonDown())
    modWheel.setValue(audioProcessor.performance.modWheel.load(),
                      juce::dontSendNotification);
  if (!pitchWheel.isMouseButtonDown())
    pitchWheel.setValue(audioProcessor.performance.pitchBend.load(),
                        juce::dontSendNotification);
}

void CP1AudioProcessorEditor::addControl(
    Control &c, const juce::String &paramId, const juce::String &title,
    const juce::String &tooltip) {
  c.slider.setSliderStyle(juce::Slider::LinearVertical);
  c.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 20);
  c.slider.setTooltip(tooltip);
  c.attachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          audioProcessor.apvts, paramId, c.slider);
  addAndMakeVisible(c.slider);

  c.label.setText(title, juce::dontSendNotification);
  c.label.setJustificationType(juce::Justification::centred);
  c.label.setTooltip(tooltip);
  addAndMakeVisible(c.label);
}

//==============================================================================
void CP1AudioProcessorEditor::paint(juce::Graphics &g) {
  const float scale = (float)getWidth() / (float)kBaseWidth;
  const auto cyan = CyberLookAndFeel::cyan;
  const auto magenta = CyberLookAndFeel::magenta;

  g.fillAll(CyberLookAndFeel::background);

  // Faint perspective-free grid.
  g.setColour(cyan.withAlpha(0.035f));
  const float step = 24.0f * scale;
  for (float x = 0; x < (float)getWidth(); x += step)
    g.drawVerticalLine((int)x, 0.0f, (float)getHeight());
  for (float y = 0; y < (float)getHeight(); y += step)
    g.drawHorizontalLine((int)y, 0.0f, (float)getWidth());

  // Title with a magenta glow underneath.
  const auto titleArea = juce::Rectangle<int>(
      (int)(20 * scale), (int)(8 * scale), (int)(420 * scale), (int)(40 * scale));
  g.setFont(CyberLookAndFeel::monoFont(28.0f * scale, true));
  g.setColour(magenta.withAlpha(0.35f));
  g.drawText("CP1", titleArea.translated(2, 2),
             juce::Justification::centredLeft, false);
  g.setColour(cyan);
  g.drawText("CP1", titleArea, juce::Justification::centredLeft,
             false);
  g.setColour(magenta);
  g.fillRect(juce::Rectangle<float>(20 * scale, 50 * scale, 90 * scale,
                                    2.0f * scale));

  // Scope: glowing trace of the live output.
  if (!scopeBounds.isEmpty()) {
    const auto sb = scopeBounds.toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillRoundedRectangle(sb, 3.0f);
    g.setColour(cyan.withAlpha(0.35f));
    g.drawRoundedRectangle(sb, 3.0f, 1.0f);
    g.setColour(cyan.withAlpha(0.12f));
    g.drawHorizontalLine((int)sb.getCentreY(), sb.getX() + 2, sb.getRight() - 2);

    juce::Path trace;
    const float mid = sb.getCentreY(), amp = sb.getHeight() * 0.42f;
    for (size_t i = 0; i < scopeData.size(); ++i) {
      const float x = sb.getX() + 3.0f + (sb.getWidth() - 6.0f) * (float)i /
                                             (float)(scopeData.size() - 1);
      const float y = mid - juce::jlimit(-1.0f, 1.0f, scopeData[i] * 2.5f) * amp;
      if (i == 0) trace.startNewSubPath(x, y); else trace.lineTo(x, y);
    }
    g.setColour(magenta.withAlpha(0.25f));
    g.strokePath(trace, juce::PathStrokeType(4.0f * scale));
    g.setColour(cyan);
    g.strokePath(trace, juce::PathStrokeType(1.4f * scale));
  }

  // Section panels with chamfered corners and a header tag.
  const float ch = 10.0f * scale;
  for (const auto &s : sections) {
    const auto r = s.bounds;
    juce::Path p;
    p.startNewSubPath(r.getX() + ch, r.getY());
    p.lineTo(r.getRight(), r.getY());
    p.lineTo(r.getRight(), r.getBottom() - ch);
    p.lineTo(r.getRight() - ch, r.getBottom());
    p.lineTo(r.getX(), r.getBottom());
    p.lineTo(r.getX(), r.getY() + ch);
    p.closeSubPath();

    g.setColour(CyberLookAndFeel::panel);
    g.fillPath(p);
    g.setColour(cyan.withAlpha(0.12f));
    g.strokePath(p, juce::PathStrokeType(4.0f * scale));
    g.setColour(cyan.withAlpha(0.7f));
    g.strokePath(p, juce::PathStrokeType(1.0f));

    // Scanlines inside the panel.
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    for (float y = r.getY() + 2; y < r.getBottom(); y += 3.0f)
      g.drawHorizontalLine((int)y, r.getX() + 1, r.getRight() - 1);

    g.setColour(magenta);
    g.setFont(CyberLookAndFeel::monoFont(11.0f * scale, true));
    g.drawText(s.title, r.withHeight(20.0f * scale).reduced(12.0f * scale, 0),
               juce::Justification::centredLeft, false);
  }
}

void CP1AudioProcessorEditor::resized() {
  const float scale = (float)getWidth() / (float)kBaseWidth;
  auto area = getLocalBounds();

  midiKeyboard.setBounds(area.removeFromBottom((int)(kKeyboardHeight * scale)));
  // C1..C8 = 50 white keys; size them to fill the window exactly.
  midiKeyboard.setKeyWidth((float)midiKeyboard.getWidth() / 50.0f);
  midiKeyboard.setLowestVisibleKey(24);
  const int headerH = (int)(kHeaderHeight * scale);
  auto header = area.removeFromTop(headerH);
  const int padX = (int)(10 * scale), padY = (int)(12 * scale);
  randomizeButton.setBounds(header.removeFromRight((int)(130 * scale))
                                .reduced(padX, padY));
  savePresetButton.setBounds(header.removeFromRight((int)(70 * scale))
                                 .reduced((int)(4 * scale), padY));
  nextPresetButton.setBounds(header.removeFromRight((int)(32 * scale))
                                 .reduced((int)(2 * scale), padY));
  presetBox.setBounds(header.removeFromRight((int)(230 * scale))
                          .reduced((int)(2 * scale), padY));
  prevPresetButton.setBounds(header.removeFromRight((int)(32 * scale))
                                 .reduced((int)(2 * scale), padY));
  // Scope fills the gap between the title and the preset browser.
  scopeBounds = header.withTrimmedLeft((int)(210 * scale))
                    .reduced((int)(10 * scale), (int)(8 * scale));

  auto body = area.reduced((int)(12 * scale), (int)(6 * scale));
  const int gap = (int)(10 * scale);

  // Each section's width is proportional to its number of columns.
  int totalCols = 0;
  for (const auto &s : sections)
    totalCols += (int)s.controls.size();
  const float colW =
      (float)(body.getWidth() - gap * ((int)sections.size() - 1)) /
      (float)totalCols;

  float x = (float)body.getX();
  for (auto &s : sections) {
    const float w = colW * (float)s.controls.size();
    s.bounds = {x, (float)body.getY(), w, (float)body.getHeight()};

    auto inner = s.bounds.toNearestInt().reduced((int)(6 * scale), 0);
    inner.removeFromTop((int)(22 * scale)); // section header tag
    inner.removeFromBottom((int)(8 * scale));
    const int cw = inner.getWidth() / (int)s.controls.size();
    for (auto *c : s.controls) {
      auto col = inner.removeFromLeft(cw);
      c->label.setBounds(col.removeFromTop((int)(20 * scale)));
      c->label.setFont(CyberLookAndFeel::monoFont(12.0f * scale, true));
      c->slider.setBounds(col);
      if (c->slider.getTextBoxPosition() != juce::Slider::NoTextBox)
        c->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                                  juce::jmin(cw - 2, (int)(76 * scale)),
                                  (int)(20 * scale));
    }
    x += w + (float)gap;
  }
}
