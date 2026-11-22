# Analog Chorus Collection

JUCE 8 VST3 and standalone modulation plugin. Original DSP implementations, not copied from other emulators.

## Models
1. CE-2-inspired BBD: triangular modulation, short delay, pre-emphasis/companding and low-pass bandwidth.
2. CE-1-inspired BBD: independently tuned BBD modulation and filtering.
3. Dimension-style: dual offset modulation paths with stereo decorrelation.
4. Tri-Chorus: three offset modulated delay voices.
5. Midiverb II-inspired Chorus: lo-fi converter and vintage digital bandwidth with chorus modulation.
6. Midiverb II-inspired Symphonic: **separate six-voice architecture**, slower and deeper modulation.

Only chorus and symphonic effects are included: **no reverb or delay modes**. All chorus algorithms inherently use short modulated delay lines.

These are initial topology-informed approximations, **not verified circuit-exact or coefficient-exact recreations**. Midiverb algorithms and coefficients are not claimed to have been extracted from firmware. Future revisions should be informed by verified schematics, measured hardware, and documented DSP coefficient evidence.

## Build
Run GitHub Actions **Build Windows VST3**, or configure with CMake 3.22+ and a C++17 toolchain. The workflow uploads a VST3 artifact on success.

## Controls
Model, Rate, Depth, Mix, Width, Tone, Drive, Output Level, and Quality. Presets and automation are stored via JUCE AudioProcessorValueTreeState.
