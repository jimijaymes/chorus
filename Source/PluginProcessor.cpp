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
    delay.assign((size_t)size,0.0f); write=0; phase=0.0; dc=dcIn=tone=comp=pre=preDcIn=preDc=env=antiAlias=deEmphasis=adcLow=dacLow=0.0f; rng=0x91e10da5u;
}
float ChorusProcessor::Voice::read(float delaySamples) const
{
    const int n=(int)delay.size();
    if(n<4 || !std::isfinite(delaySamples)) return 0.0f;
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
    if(delay.empty()) return;
    delay[(size_t)write]=std::isfinite(x)?x:0.0f; write=(write+1)%(int)delay.size();
}
juce::AudioProcessorValueTreeState::ParameterLayout ChorusProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    using F=juce::AudioParameterFloat; using C=juce::AudioParameterChoice;
    l.add(std::make_unique<C>(juce::ParameterID{"model",1},"Model",juce::StringArray{"CE-2 BBD","Deluxe Memory Man BBD","Dimension-style","Tri-Chorus","Midiverb II Chorus","SPX Symphonic"},0));
    l.add(std::make_unique<C>(juce::ParameterID{"monoStereo",1},"Mono Chorus Output",juce::StringArray{"Original Mono","Stereo (Opposite LFO)"},0));
    l.add(std::make_unique<C>(juce::ParameterID{"mode",1},"Algorithm Mode",juce::StringArray{"Mode 1","Mode 2","Mode 3","Mode 4"},0));
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
    if(channels<=0 || voices[0][0].delay.empty()) return;
    const int model=juce::jlimit(0,5,(int)*parameters.getRawParameterValue("model"));
    const float rate=parameters.getRawParameterValue("rate")->load();
    const float depth=*parameters.getRawParameterValue("depth");
    const float mix=*parameters.getRawParameterValue("mix");
    const float width=*parameters.getRawParameterValue("width");
    
    const float drive=parameters.getRawParameterValue("drive")->load(); // DMM preamp gain only.
    const float gain=juce::Decibels::decibelsToGain(parameters.getRawParameterValue("level")->load());
    const int quality=(int)*parameters.getRawParameterValue("quality");
    const int mode=juce::jlimit(0,3,(int)*parameters.getRawParameterValue("mode"));
    const bool stereoMonoChorus=parameters.getRawParameterValue("monoStereo")->load()>.5f;
    const int voicesUsed=model==3?3:(model==5?6:(model==2?4:(model==4?2:1)));
    const float baseMs[]={7.2f,24.0f,8.0f,11.0f,18.0f,21.0f};
    const float sweepMs[]={0.85f,3.0f,0.65f,3.5f,3.0f,4.0f};
    const float rates[]={0.55f,0.85f,0.45f,0.72f,0.48f,0.31f};
    const float bandwidth[]={6500.0f,4300.0f,6700.0f,8000.0f,5300.0f,10500.0f};
    for(int sample=0;sample<buffer.getNumSamples();++sample)
    {
        for(int ch=0;ch<channels;++ch)
        {
            const float dry=buffer.getSample(ch,sample);
            float wet=0.0f;
            for(int v=0;v<voicesUsed;++v)
            {
                auto& s=voices[(size_t)ch][(size_t)v];
                const double phaseOffset=(model==3||model==5||model==2)?(double)v/(double)voicesUsed:(model==4&&v==1?0.5:0.0);
                // CE-1/CE-2: original mono uses the same LFO on both channels;
                // optional stereo puts the right channel 180 degrees out of phase.
                const double stereoOffset=(model<=1)
                    ? ((stereoMonoChorus && ch==1)?0.5:0.0)
                    : ((ch==1)?width*(model==2?0.5:0.25):0.0);
                // Separate phase accumulators per voice/channel allow independent architecture and stable stereo offsets.
                const double phase=s.phase+phaseOffset+stereoOffset;
                const float sine=(float)std::sin(2.0*pi*phase);
                const float triangle=(float)(2.0/pi*std::asin(std::sin(2.0*pi*phase)));
                const float lfo=(model==0)?triangle:(model==1?0.8f*sine+0.2f*triangle:((model==2)?(0.75f*sine+0.25f*triangle):sine));
                const float jitter=(model>=4)?0.07f*std::sin((float)(phase*2.0*pi*7.13)):0.0f;
                // Dimension modes select fixed modulation patterns, not a user sweep.
                const float dimensionDepth[]={0.32f,0.48f,0.62f,0.76f};
                const float modeDepth[]={0.55f,0.78f,1.0f,1.0f};
                const float effectiveDepth=model==2?dimensionDepth[mode]:(model>=4?modeDepth[juce::jmin(mode,2)]:depth);
                const float effectiveRate=model==2?(0.45f+0.11f*(float)mode):(model==0?juce::jlimit(0.25f,1.8f,rate):rate);
                const float digitalModeDelay=model==4?(mode==0?0.0f:(mode==1?3.5f:7.0f)):(model==5?(mode==0?0.0f:(mode==1?4.0f:8.0f)):0.0f);
                const float dMs=baseMs[model]+digitalModeDelay+effectiveDepth*sweepMs[model]*(lfo+jitter);
                const float delaySamples=(float)(fs*0.001)*juce::jmax(1.0f,dMs);
                float input=dry;
                // Independent analogue front-ends: CE-2 BBD, DMM preamp/BBD,
                // and Dimension BBD. These are informed approximations, not SPICE.
                if(model<=2)
                {
                    const float preHz=model==0?1050.f:(model==1?760.f:1200.f);
                    const float preLow=onePole(input,s.pre,preHz,fs);
                    const float emphasized=input+(model==1?.22f:.38f)*(input-preLow);
                    if(model==1) {
                        // Deluxe Memory Man preamp: soft asymmetric 9V-era
                        // analogue gain and AC-coupled signal path.
                        const float preGain=1.2f+3.2f*drive;
                        input=std::tanh(preGain*emphasized+.075f)-std::tanh(.075f);
                        input=highpass(input,s.preDcIn,s.preDc,18.f,fs);
                    } else input=std::tanh(emphasized*1.08f);
                    // Envelope-following compander approximation: compress
                    // before BBD; expansion after delay uses same envelope.
                    const float envelope=onePole(std::abs(input),s.env,model==1?38.f:52.f,fs);
                    const float compGain=1.f/std::sqrt(1.f+4.f*envelope);
                    input*=compGain;
                    // Clock-limited anti-aliasing before BBD.
                    input=onePole(input,s.antiAlias,model==1?5300.f:8500.f,fs);
                }
                else if(model>=4)
                {
                    // Model-specific converter input: separate analogue
                    // anti-alias filters, quantisation, and reconstruction.
                    const bool midiverb=model==4;
                    const float adcHz=midiverb?6500.f:14500.f;
                    input=onePole(input,s.adcLow,adcHz,fs);
                    input=std::tanh(input*(midiverb?1.12f:1.035f));
                    const float steps=midiverb?2048.f:32768.f;
                    // Deterministic triangular dither avoids low-level
                    // quantisation dead zones without a shared random engine.
                    s.rng=s.rng*1664525u+1013904223u;
                    const float d1=(float)((s.rng>>8)&65535u)/65535.f;
                    s.rng=s.rng*1664525u+1013904223u;
                    const float d2=(float)((s.rng>>8)&65535u)/65535.f;
                    input=std::round((input+(d1-d2)/steps)*steps)/steps;
                }
                else input=onePole(input,s.antiAlias,9000.f,fs);
                s.push(input);
                float delayed=s.read(delaySamples);
                if(model<=2)
                {
                    delayed=onePole(delayed,s.tone,bandwidth[model],fs);
                    // Matched envelope expansion and de-emphasis; the
                    // nonlinear BBD output is intentionally subtle.
                    const float expansion=std::sqrt(1.f+4.f*s.env);
                    delayed=std::tanh(delayed*expansion*1.08f);
                    delayed=onePole(delayed,s.deEmphasis,model==1?4300.f:6200.f,fs);
                    delayed=highpass(delayed,s.dcIn,s.dc,model==1?28.f:55.f,fs);
                }
                else if(model>=4)
                {
                    const bool midiverb=model==4;
                    delayed=onePole(delayed,s.dacLow,midiverb?5700.f:13200.f,fs);
                    delayed=onePole(delayed,s.tone,bandwidth[model],fs);
                    delayed=highpass(delayed,s.dcIn,s.dc,midiverb?95.f:24.f,fs);
                }
                else delayed=onePole(delayed,s.tone,bandwidth[model],fs);
                wet+=delayed/(float)voicesUsed;
                // High-quality mode updates phase twice per sample (same net rate) for LFO numerical consistency.
                const double modeRate=model>=4?(mode==0?0.78:(mode==1?1.0:1.25)):1.0;
                const double step=(double)(effectiveRate*rates[model]*modeRate)/fs;
                if(quality!=0) {s.phase+=step*0.5; s.phase-=std::floor(s.phase); s.phase+=step*0.5;}
                else s.phase+=step;
                s.phase-=std::floor(s.phase);
            }
            // CE-1/CE-2 retain their dry path; Dimension-like and symphonic architectures use decorrelated wet voices.
            const float wetGain=model==5?0.85f:(model==2?0.72f:1.0f);
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
