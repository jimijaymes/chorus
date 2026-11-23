#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>

class ChorusProcessor final : public juce::AudioProcessor
{
public:
    ChorusProcessor();
    ~ChorusProcessor() override = default;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Analog Chorus Collection"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.15; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorValueTreeState parameters;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    struct Voice
    {
        std::vector<float> delay;
        int write = 0;
        double phase = 0.0;
        float dc = 0.0f, dcIn = 0.0f, tone = 0.0f, comp = 0.0f, pre = 0.0f;
        float preDcIn=0,preDc=0,env=0,antiAlias=0,deEmphasis=0,adcLow=0,dacLow=0;
        uint32_t rng=0x91e10da5u;
        float read(float delaySamples) const;
        void push(float sample);
        void reset(int size);
    };
    std::array<std::array<Voice, 6>, 2> voices;
    double fs = 48000.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChorusProcessor)
};
