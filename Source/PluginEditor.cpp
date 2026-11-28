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
    const char* ids[]={"model","quality","mode","monoStereo","dmmMode","junoMode","dmmEngine"};
    const juce::StringArray choices[]={
        {"CE-2 BBD","Deluxe Memory Man BBD","Dimension-style","Tri-Chorus","Midiverb II Chorus","SPX90 Symphonic","SPX990 Symphonic","Juno-60 Chorus","Boss BF-2 Flanger","Deluxe Electric Mistress","EHX Polychorus"},
        {"Normal","High (2x modulation update)"},
        {"Mode 1","Mode 2","Mode 3","Mode 4"},
        {"Original Mono","Stereo (Opposite LFO)"},
        {"Chorus","Vibrato"},
        {"I","II","I+II"},
        {"Short Modulation","Clocked BBD"}
    };
    for(int i=0;i<7;++i)
    {
        auto c=std::make_unique<juce::ComboBox>();
        c->addItemList(choices[i],1); addAndMakeVisible(*c);
        comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters,ids[i],*c));
        combos.push_back(std::move(c));
    }
    for(auto& s:sliders) s->textFromValueFunction=[](double v){return juce::String(v,1);};
    sliders[0]->textFromValueFunction=[](double v){return juce::String(v,2)+" Hz";};
    auto feedback=std::make_unique<juce::Slider>();
    feedback->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    feedback->setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,20);
    feedback->textFromValueFunction=[](double v){return juce::String(v*100.,0)+"%";};
    addAndMakeVisible(*feedback);
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters,"dmmFeedback",*feedback));
    sliders.push_back(std::move(feedback));
    auto feedbackLabel=std::make_unique<juce::Label>();
    feedbackLabel->setText("FEEDBACK",juce::dontSendNotification);
    feedbackLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(*feedbackLabel);
    labels.push_back(std::move(feedbackLabel));
    auto delay=std::make_unique<juce::Slider>();
    delay->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    delay->setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,20);
    delay->textFromValueFunction=[](double v){return juce::String(v,1)+" ms";};
    addAndMakeVisible(*delay);
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters,"dmmDelay",*delay));
    sliders.push_back(std::move(delay));
    auto shortDelay=std::make_unique<juce::Slider>();
    shortDelay->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    shortDelay->setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,20);
    shortDelay->textFromValueFunction=[](double v){return juce::String(v,1)+" ms";};
    addAndMakeVisible(*shortDelay);
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters,"dmmShortDelay",*shortDelay));
    sliders.push_back(std::move(shortDelay));
    auto shortLabel=std::make_unique<juce::Label>();
    shortLabel->setText("SHORT TIME",juce::dontSendNotification);
    shortLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(*shortLabel);
    labels.push_back(std::move(shortLabel));
    auto delayLabel=std::make_unique<juce::Label>();
    delayLabel->setText("DELAY TIME",juce::dontSendNotification);
    delayLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(*delayLabel);
    labels.push_back(std::move(delayLabel));
    for(const auto& spec:std::array<std::pair<const char*,const char*>,2>{{{"flangeManual","MANUAL / TUNE"},{"flangeFeedback","RESONANCE / COLOR"}}})
    {
        auto knob=std::make_unique<juce::Slider>();
        knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob->setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,20);
        addAndMakeVisible(*knob);
        sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters,spec.first,*knob));
        sliders.push_back(std::move(knob));
        auto label=std::make_unique<juce::Label>();
        label->setText(spec.second,juce::dontSendNotification);
        label->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*label);
        labels.push_back(std::move(label));
    }
    addAndMakeVisible(ceMod);addAndMakeVisible(bfMod);addAndMakeVisible(polyFilter);
    ceAttach=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters,"ceDepthMod",ceMod);
    bfAttach=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters,"bfDepthMod",bfMod);
    polyAttach=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters,"polySweepFilter",polyFilter);
    setSize(900, 520);
    p.parameters.addParameterListener("model",this); p.parameters.addParameterListener("dmmEngine",this); updateModelControls();
}
ChorusEditor::~ChorusEditor(){processor.parameters.removeParameterListener("model",this);processor.parameters.removeParameterListener("dmmEngine",this);}
void ChorusEditor::parameterChanged(const juce::String&,float){juce::MessageManager::callAsync([safe=juce::Component::SafePointer<ChorusEditor>(this)](){if(safe!=nullptr)safe->updateModelControls();});}
void ChorusEditor::updateModelControls(){
    const int m=juce::jlimit(0,10,(int)processor.parameters.getRawParameterValue("model")->load());
    const bool dimension=m==2,digital=m>=4&&m<=6,flanger=m>=8;
    // Hardware-mode chorus units expose presets rather than independent speed/depth.
    sliders[0]->setVisible((!dimension&&!digital&&m!=7));labels[0]->setVisible(!dimension&&!digital&&m!=7);
    sliders[1]->setVisible(!dimension&&!digital&&m!=7);labels[1]->setVisible(!dimension&&!digital&&m!=7);
    sliders[4]->setVisible(false);labels[4]->setVisible(false);
    sliders[5]->setVisible(m==1);labels[5]->setVisible(m==1);
    labels[5]->setText("PREAMP GAIN",juce::dontSendNotification);
    labels[1]->setText(m==1?"MOD DEPTH":"DEPTH",juce::dontSendNotification);
    combos[2]->setVisible(dimension||digital);
    combos[3]->setVisible(m<=1||flanger);
    combos[4]->setVisible(m==1);
    combos[5]->setVisible(m==7);
    combos[6]->setVisible(m==1);
    sliders[7]->setVisible(m==1);labels[7]->setVisible(m==1);
    sliders[10]->setVisible(flanger);labels[10]->setVisible(flanger);
    sliders[11]->setVisible(flanger);labels[11]->setVisible(flanger);
    combos[2]->setVisible(dimension||digital||flanger);
    const bool shortEngine=processor.parameters.getRawParameterValue("dmmEngine")->load()<0.5f;
    sliders[8]->setVisible(m==1&&!shortEngine);labels[8]->setVisible(m==1&&!shortEngine);
    sliders[9]->setVisible(m==1&&shortEngine);labels[9]->setVisible(m==1&&shortEngine);
    ceMod.setVisible(m==0);bfMod.setVisible(m==8);polyFilter.setVisible(m==10);
    combos[2]->changeItemText(1,m==9?"Flange":m==10?"Filter Matrix":m==8?"Stock delay":"Mode 1");
    combos[2]->changeItemText(2,m==9?"Filter Matrix":m==10?"Flange":m==8?"Extended delay":"Mode 2");
    combos[2]->changeItemText(3,m==10?"Chorus":m==9?"Flange (legacy)":m==8?"Stock delay (legacy)":"Mode 3");
    combos[2]->changeItemText(4,m==10?"Double Track":m==9?"Flange (legacy)":m==8?"Stock delay (legacy)":"Mode 4");
    sliders[1]->setVisible(m!=9&&!dimension&&!digital&&m!=7);labels[1]->setVisible(sliders[1]->isVisible());
    labels[10]->setText(m==9?"RANGE":m==10?"TUNE / DELAY":"MANUAL / TUNE",juce::dontSendNotification);
    resized();
}
void ChorusEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff151b20));
    g.setColour(juce::Colour(0xffe3c18a)); g.setFont(28.0f);
    g.drawText("ANALOG CHORUS COLLECTION",25,20,getWidth()-50,42,juce::Justification::centred);
    g.setColour(juce::Colours::lightgrey); g.setFont(14.0f);
    g.drawText("Chorus / Flanger: CE-2, DMM, Juno, BF-2, Mistress, Polychorus + digital models",25,66,getWidth()-50,24,juce::Justification::centred);
    g.drawText("MODEL",55,110,130,22,juce::Justification::centred);
    g.drawText("QUALITY",540,85,180,22,juce::Justification::centred);
    g.setColour(juce::Colour(0xff75818c));
    g.drawText("Original DSP approximations; not factory-preset or measured hardware clones",20,494,getWidth()-40,22,juce::Justification::centred);
}
void ChorusEditor::resized()
{
    combos[0]->setBounds(195,110,325,28); combos[1]->setBounds(585,110,235,28);
    combos[2]->setBounds(195,150,325,28);
    combos[3]->setBounds(540,150,310,28);
    if(processor.parameters.getRawParameterValue("model")->load()<=1) combos[3]->setBounds(195,150,325,28);
    ceMod.setBounds(195,184,160,28);bfMod.setBounds(195,184,160,28);polyFilter.setBounds(195,184,160,28);
    combos[4]->setBounds(540,150,310,28);
    combos[5]->setBounds(195,150,325,28);
    combos[6]->setBounds(540,184,310,28);
    labels[7]->setBounds(55,360,130,24);
    sliders[7]->setBounds(55,385,130,105);
    labels[8]->setBounds(205,360,130,24);
    sliders[8]->setBounds(205,385,130,105);
    labels[9]->setBounds(205,360,130,24);
    sliders[9]->setBounds(205,385,130,105);
    labels[10]->setBounds(390,360,160,24);
    sliders[10]->setBounds(405,385,130,105);
    labels[11]->setBounds(570,360,175,24);
    sliders[11]->setBounds(590,385,130,105);
    for(int i=0;i<7;++i)
    {
        const int x=30+i*110;
        labels[(size_t)i]->setBounds(x,220,100,24);
        sliders[(size_t)i]->setBounds(x,247,100,105);
    }
}
