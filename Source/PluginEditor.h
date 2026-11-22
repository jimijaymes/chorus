#pragma once
#include "PluginProcessor.h"
class ChorusEditor final : public juce::AudioProcessorEditor
{
public:
    explicit ChorusEditor(ChorusProcessor&);
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
