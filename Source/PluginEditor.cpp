#include "PluginEditor.h"
ChorusEditor::ChorusEditor(ChorusProcessor& p) : AudioProcessorEditor(&p), processor(p)
{
    const char* knobs[] = {"rate","depth","mix","width","tone","drive","level"};
    const char* titles[] = {"RATE","DEPTH","MIX","WIDTH","TONE","DRIVE","LEVEL"};
    for (int i = 0; i < 7; ++i)
    {
        auto s = std::make_unique<juce::Slider>();
        s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,20);
        s->setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xffd8aa65));
        addAndMakeVisible(*s);
        sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters,knobs[i],*s));
        sliders.push_back(std::move(s));
        auto l=std::make_unique<juce::Label>(); l->setText(titles[i],juce::dontSendNotification);
        l->setJustificationType(juce::Justification::centred); l->setMinimumHorizontalScale(0.68f); addAndMakeVisible(*l); labels.push_back(std::move(l));
    }
    const char* ids[]={"model","quality","mode","monoStereo"};
    const juce::StringArray choices[]={
        {"CE-2 BBD","Deluxe Memory Man BBD","Dimension-style","Tri-Chorus","Midiverb II Chorus","SPX Symphonic"},
        {"Normal","High (2x modulation update)"},
        {"Mode 1","Mode 2","Mode 3","Mode 4"},
        {"Original Mono","Stereo (Opposite LFO)"}
    };
    for(int i=0;i<4;++i)
    {
        auto c=std::make_unique<juce::ComboBox>();
        c->addItemList(choices[i],1); addAndMakeVisible(*c);
        comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters,ids[i],*c));
        combos.push_back(std::move(c));
    }
    for(auto& s:sliders) s->textFromValueFunction=[](double v){return juce::String(v,1);};
    sliders[0]->textFromValueFunction=[](double v){return juce::String(v,2)+" Hz";};
    setSize(820, 390);
    p.parameters.addParameterListener("model",this); updateModelControls();
}
ChorusEditor::~ChorusEditor(){processor.parameters.removeParameterListener("model",this);}
void ChorusEditor::parameterChanged(const juce::String&,float){juce::MessageManager::callAsync([safe=juce::Component::SafePointer<ChorusEditor>(this)](){if(safe!=nullptr)safe->updateModelControls();});}
void ChorusEditor::updateModelControls(){
    const int m=juce::jlimit(0,5,(int)processor.parameters.getRawParameterValue("model")->load());
    const bool dimension=m==2,digital=m>=4;
    // Hardware-mode chorus units expose presets rather than independent speed/depth.
    sliders[0]->setVisible(!dimension&&!digital);labels[0]->setVisible(!dimension&&!digital);
    sliders[1]->setVisible(!dimension&&!digital);labels[1]->setVisible(!dimension&&!digital);
    sliders[4]->setVisible(false);labels[4]->setVisible(false);
    sliders[5]->setVisible(m==1);labels[5]->setVisible(m==1);
    labels[5]->setText("PREAMP GAIN",juce::dontSendNotification);
    combos[2]->setVisible(dimension||digital);
    combos[3]->setVisible(m<=1);
    resized();
}
void ChorusEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff151b20));
    g.setColour(juce::Colour(0xffe3c18a)); g.setFont(28.0f);
    g.drawText("ANALOG CHORUS COLLECTION",25,20,getWidth()-50,42,juce::Justification::centred);
    g.setColour(juce::Colours::lightgrey); g.setFont(14.0f);
    g.drawText("CE-2 / Deluxe Memory Man / Dimension / Tri-Chorus / Midiverb / SPX",25,66,getWidth()-50,24,juce::Justification::centred);
    g.drawText("MODEL",55,110,130,22,juce::Justification::centred);
    g.drawText("QUALITY",540,110,180,22,juce::Justification::centred);
    g.setColour(juce::Colour(0xff75818c));
    g.drawText("Original DSP approximations; not factory-preset or measured hardware clones",20,352,getWidth()-40,22,juce::Justification::centred);
}
void ChorusEditor::resized()
{
    combos[0]->setBounds(195,110,325,28); combos[1]->setBounds(585,140,175,28);
    combos[2]->setBounds(195,150,325,28);
    combos[3]->setBounds(195,150,325,28);
    for(int i=0;i<7;++i)
    {
        const int x=30+i*110;
        labels[(size_t)i]->setBounds(x,205,100,24);
        sliders[(size_t)i]->setBounds(x,232,100,105);
    }
}
