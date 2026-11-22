#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace {
constexpr double pi=3.14159265358979323846;
float clampf(float x,float lo,float hi) {return juce::jlimit(lo,hi,x);}
float onePole(float x,float& state,float hz,double sr)
{
    const float a=std::exp(-2.0f*(float)pi*hz/(float)sr);
    state=a*state+(1.0f-a)*x; return state;
}
float highpass(float x,float& lastIn,float& lastOut,float hz,double sr)
{
    const float a=std::exp(-2.0f*(float)pi*hz/(float)sr);
    const float y=a*(lastOut+x-lastIn); lastIn=x; lastOut=y; return y;
}
}
void ChorusProcessor::Voice::reset(int size)
{
    delay.assign((size_t)size,0.0f); write=0; phase=0.0; dc=dcIn=tone=comp=pre=0.0f;
}
float ChorusProcessor::Voice::read(float delaySamples) const
{
    const int n=(int)delay.size();
    const float p=(float)write-clampf(delaySamples,1.0f,(float)n-3.0f);
    const int i=(int)std::floor(p); const float frac=p-(float)i;
    const auto at=[&](int index) {index%=n; if(index<0) index+=n; return delay[(size_t)index];};
    // Cubic Hermite interpolation keeps moving-delay pitch modulation smooth.
    const float a=at(i-1),b=at(i),c=at(i+1),d=at(i+2);
    const float c0=b,c1=0.5f*(c-a),c2=a-2.5f*b+2.0f*c-0.5f*d,c3=0.5f*(d-a)+1.5f*(b-c);
    return ((c3*frac+c2)*frac+c1)*frac+c0;
}
void ChorusProcessor::Voice::push(float x)
{
    delay[(size_t)write]=x; write=(write+1)%(int)delay.size();
}
juce::AudioProcessorValueTreeState::ParameterLayout ChorusProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    using F=juce::AudioParameterFloat; using C=juce::AudioParameterChoice;
    l.add(std::make_unique<C>(juce::ParameterID{"model",1},"Model",juce::StringArray{"CE-2 BBD","CE-1 BBD","Dimension-style","Tri-Chorus","Midiverb II Chorus","Midiverb II Symphonic"},0));
    l.add(std::make_unique<C>(juce::ParameterID{"quality",1},"Quality",juce::StringArray{"Normal","High"},0));
    l.add(std::make_unique<F>(juce::ParameterID{"rate",1},"Rate",juce::NormalisableRange<float>(0.05f,6.0f,0.001f,0.4f),0.8f));
    l.add(std::make_unique<F>(juce::ParameterID{"depth",1},"Depth",juce::NormalisableRange<float>(0.0f,1.0f),0.55f));
    l.add(std::make_unique<F>(juce::ParameterID{"mix",1},"Mix",juce::NormalisableRange<float>(0.0f,1.0f),0.5f));
    l.add(std::make_unique<F>(juce::ParameterID{"width",1},"Width",juce::NormalisableRange<float>(0.0f,1.0f),0.8f));
    l.add(std::make_unique<F>(juce::ParameterID{"tone",1},"Tone",juce::NormalisableRange<float>(0.0f,1.0f),0.5f));
    l.add(std::make_unique<F>(juce::ParameterID{"drive",1},"Drive",juce::NormalisableRange<float>(0.0f,1.0f),0.15f));
    l.add(std::make_unique<F>(juce::ParameterID{"level",1},"Level",juce::NormalisableRange<float>(-18.0f,18.0f),0.0f));
    return l;
}
ChorusProcessor::ChorusProcessor()
: AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),
  parameters(*this,nullptr,"CHORUS",createLayout()) {}
bool ChorusProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    return l.getMainOutputChannelSet()==l.getMainInputChannelSet() &&
        (l.getMainOutputChannelSet()==juce::AudioChannelSet::mono() || l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());
}
void ChorusProcessor::prepareToPlay(double sr,int)
{
    fs=juce::jmax(8000.0,sr);
    for(auto& channel:voices) for(auto& voice:channel) voice.reset((int)(fs*0.12)+16);
}
void ChorusProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int channels=juce::jmin(buffer.getNumChannels(),2);
    const int model=juce::jlimit(0,5,(int)*parameters.getRawParameterValue("model"));
    const float rate=*parameters.getRawParameterValue("rate");
    const float depth=*parameters.getRawParameterValue("depth");
    const float mix=*parameters.getRawParameterValue("mix");
    const float width=*parameters.getRawParameterValue("width");
    const float tone=*parameters.getRawParameterValue("tone");
    const float drive=*parameters.getRawParameterValue("drive");
    const float gain=juce::Decibels::decibelsToGain(*parameters.getRawParameterValue("level"));
    const int quality=(int)*parameters.getRawParameterValue("quality");
    const int voicesUsed=model==3?3:(model==5?6:(model==2?2:1));
    const float baseMs[]={7.2f,5.4f,7.5f,11.0f,18.0f,21.0f};
    const float sweepMs[]={2.3f,2.7f,1.9f,3.5f,5.5f,7.0f};
    const float rates[]={1.0f,0.85f,0.58f,0.72f,0.48f,0.31f};
    const float bandwidth[]={6500.0f,5200.0f,7000.0f,8000.0f,4700.0f,4300.0f};
    for(int sample=0;sample<buffer.getNumSamples();++sample)
    {
        for(int ch=0;ch<channels;++ch)
        {
            const float dry=buffer.getSample(ch,sample);
            float wet=0.0f;
            for(int v=0;v<voicesUsed;++v)
            {
                auto& s=voices[(size_t)ch][(size_t)v];
                const double phaseOffset=(model==3||model==5)?(double)v/(double)voicesUsed:((model==2&&v==1)?0.5:0.0);
                const double stereoOffset=(ch==1)?width*(model==2?0.5:0.25):0.0;
                // Separate phase accumulators per voice/channel allow independent architecture and stable stereo offsets.
                const double phase=s.phase+phaseOffset+stereoOffset;
                const float sine=(float)std::sin(2.0*pi*phase);
                const float triangle=(float)(2.0/pi*std::asin(std::sin(2.0*pi*phase)));
                const float lfo=(model<=1)?triangle:((model==2)?(0.75f*sine+0.25f*triangle):sine);
                const float jitter=(model>=4)?0.07f*std::sin((float)(phase*2.0*pi*7.13)):0.0f;
                const float dMs=baseMs[model]+depth*sweepMs[model]*(lfo+jitter);
                const float delaySamples=(float)(fs*0.001)*juce::jmax(1.0f,dMs);
                float input=dry;
                if(model<=1)
                {
                    // Input pre-emphasis, BBD companding and finite bucket bandwidth.
                    const float low=onePole(input,s.pre,950.0f,fs);
                    input=std::tanh((input+0.45f*(input-low))*(1.0f+drive*2.0f));
                    input=std::copysign(std::sqrt(std::abs(input)+1.0e-12f),input);
                }
                else if(model>=4)
                {
                    // Vintage 12-bit-inspired digital quantisation and converter coloration.
                    input=std::tanh(input*(1.0f+drive*2.0f));
                    input=std::round(input*2048.0f)/2048.0f;
                }
                else input=std::tanh(input*(1.0f+drive));
                s.push(input);
                float delayed=s.read(delaySamples);
                const float cutoff=bandwidth[model]*(0.5f+tone);
                delayed=onePole(delayed,s.tone,cutoff,fs);
                if(model<=1)
                {
                    delayed=std::copysign(delayed*delayed,delayed);
                    delayed=highpass(delayed,s.dcIn,s.dc,65.0f,fs);
                }
                else if(model>=4) delayed=highpass(delayed,s.dcIn,s.dc,110.0f,fs);
                wet+=delayed/(float)voicesUsed;
                // High-quality mode updates phase twice per sample (same net rate) for LFO numerical consistency.
                const double step=(double)(rate*rates[model])/fs;
                if(quality!=0) {s.phase+=step*0.5; s.phase-=std::floor(s.phase); s.phase+=step*0.5;}
                else s.phase+=step;
                s.phase-=std::floor(s.phase);
            }
            // CE-1/CE-2 retain their dry path; Dimension-like and symphonic architectures use decorrelated wet voices.
            const float wetGain=model==5?0.85f:1.0f;
            buffer.setSample(ch,sample,gain*((1.0f-mix)*dry+mix*wet*wetGain));
        }
    }
    for(int ch=channels;ch<buffer.getNumChannels();++ch) buffer.clear(ch,0,buffer.getNumSamples());
}
juce::AudioProcessorEditor* ChorusProcessor::createEditor(){return new ChorusEditor(*this);}
void ChorusProcessor::getStateInformation(juce::MemoryBlock& data)
{
    if(auto xml=parameters.copyState().createXml()) copyXmlToBinary(*xml,data);
}
void ChorusProcessor::setStateInformation(const void* data,int size)
{
    if(auto xml=getXmlFromBinary(data,size)) parameters.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new ChorusProcessor();}
