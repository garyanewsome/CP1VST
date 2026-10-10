# AtFirstJuice

A professional, polyphonic analog-modeled software synthesizer built in C++ using the [JUCE Framework](https://juce.com/) and CMake.

## Features

**Sound engine**
- **Oscillators**: two oscillators, each Sine, Saw (PolyBLEP band-limited), Triangle, White noise or Pink noise, with mix and OSC 2 detune (+/-12 semitones).
- **Sub oscillator**: sine one octave below OSC 1.
- **Unison**: 1 to 5 stacked, detuned voices per oscillator with stereo width (true stereo output).
- **Ladder filter**: per-voice 4-pole ladder with LP12, LP24, BP and HP modes and resonance.
- **Filter envelope**: its own ADSR plus a bipolar amount (+/-5 octaves of cutoff).
- **Amp envelope**: ADSR across 8 simultaneous voices.
- **LFO**: sweeps filter cutoff, pitch (vibrato) or both, with smooth per-sample modulation.
- **Glide**: portamento between notes.
- **Drive**: tanh saturation after the filter.
- **Performance controls**: pitch wheel (+/-2 semitones, spring return) and mod wheel (adds to LFO depth), on screen and via MIDI.
- **Thread-safe processing**: lock-free parameter fetches via `juce::AudioProcessorValueTreeState`, with no allocation or locks on the audio thread.

**Interface**
- Cyberpunk look: neon cyan/magenta, chamfered section panels (PERF, OSC, ENV, FILTER, F.ENV, MOD, VOICE, OUT), blade-style faders and ribbed performance wheels.
- Hover definitions on every control title, values shown with units, and a live output scope in the header.
- Resizable window with an on-screen MIDI keyboard.

**Presets**
- Factory starters (also exposed as host programs) and user presets saved from the header.
- User presets are stored as `.afjpreset` files in `~/Library/Application Support/AtFirstJuice/Presets/`.

## Architecture & Code Map

* `CMakeLists.txt` - Project structure configuration using `FetchContent`.
* `PluginProcessor.h/cpp` - The audio core logic. Defines the DSP pipeline, voices, and state management.
* `PluginEditor.h/cpp` - The visual interface, layout, scope and preset browser.
* `CyberLookAndFeel.h` - All custom drawing: faders, wheels, tooltips, palette.
* `Presets.h` - Factory presets and user preset save/load.

## How To Build

Make sure you have CMake and Ninja installed. Then navigate to the project directory in your terminal and run:

```bash
# Provide the Release build instructions to CMake:
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Execute the compiler target globally:
cmake --build build
```

The resulting plugins will be placed in `build/AtFirstJuice_artefacts/Release/`.

## How To Run Without A DAW

Run the interactive standalone preview directly:
```bash
open build/AtFirstJuice_artefacts/Release/Standalone/AtFirstJuice.app
```

## Author

**Gary A. Newsome**
**(c) 2026**
