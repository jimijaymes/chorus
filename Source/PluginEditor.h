#pragma once
#include "PluginProcessor.h"
class ChorusEditor final : public juce::AudioProcessorEditor, private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit ChorusEditor(ChorusProcessor&);
    ~ChorusEditor() override;
    void parameterChanged(const juce::String&,float) override;
    void updateModelControls();
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    ChorusProcessor& processor;
    std::vector<std::unique_ptr<juce::Slider>> sliders;
    std::vector<std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
};
