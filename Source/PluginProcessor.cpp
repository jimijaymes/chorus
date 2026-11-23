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
    delay.assign((size_t)size,0.0f);
    bbd.assign(4096,0.0f);
    bbdWrite=0; clockPhase=0.0; digitalPhase=0.0; digitalHeld=0.0f; heldBbd=previousBbd=bbdCharge=0.0f;
    write=0; phase=0.0; dc=dcIn=tone=comp=pre=preDcIn=preDc=env=antiAlias=deEmphasis=adcLow=dacLow=feedbackSample=feedbackLow=0.0f; rng=0x91e10da5u;
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
    l.add(std::make_unique<C>(juce::ParameterID{"model",1},"Model",juce::StringArray{"CE-2 BBD","Deluxe Memory Man BBD","Dimension-style","Tri-Chorus","Midiverb II Chorus","SPX90 Symphonic","SPX990 Symphonic","Juno-60 Chorus","Boss BF-2 Flanger","Deluxe Electric Mistress","EHX Polychorus"},0));
    l.add(std::make_unique<C>(juce::ParameterID{"monoStereo",1},"Mono Chorus Output",juce::StringArray{"Original Mono","Stereo (Opposite LFO)"},0));
    l.add(std::make_unique<C>(juce::ParameterID{"mode",1},"Algorithm Mode",juce::StringArray{"Mode 1","Mode 2","Mode 3","Mode 4"},0));
    l.add(std::make_unique<C>(juce::ParameterID{"junoMode",1},"Juno Chorus",juce::StringArray{"I","II","I+II"},0));
    l.add(std::make_unique<F>(juce::ParameterID{"dmmDelay",1},"DMM Delay Time",juce::NormalisableRange<float>(19.5f,40.f,0.5f),22.f));
    l.add(std::make_unique<F>(juce::ParameterID{"dmmShortDelay",1},"DMM Short Delay Time",juce::NormalisableRange<float>(5.f,35.f,0.5f),10.f));
    l.add(std::make_unique<C>(juce::ParameterID{"dmmMode",1},"DMM Modulation",juce::StringArray{"Chorus","Vibrato"},0));
    l.add(std::make_unique<C>(juce::ParameterID{"dmmEngine",1},"DMM Engine",juce::StringArray{"Short Modulation","Clocked BBD"},0));
    l.add(std::make_unique<F>(juce::ParameterID{"dmmFeedback",1},"DMM Feedback",juce::NormalisableRange<float>(0.f,0.92f,0.01f),0.0f));
    l.add(std::make_unique<C>(juce::ParameterID{"quality",1},"Quality",juce::StringArray{"Normal","High"},0));
    l.add(std::make_unique<F>(juce::ParameterID{"rate",1},"Rate",juce::NormalisableRange<float>(0.05f,6.0f,0.001f,0.4f),0.8f));
    l.add(std::make_unique<F>(juce::ParameterID{"depth",1},"Depth",juce::NormalisableRange<float>(0.0f,1.0f),0.55f));
    l.add(std::make_unique<F>(juce::ParameterID{"mix",1},"Mix",juce::NormalisableRange<float>(0.0f,1.0f),0.5f));
    l.add(std::make_unique<F>(juce::ParameterID{"width",1},"Width",juce::NormalisableRange<float>(0.0f,1.0f),0.8f));
    l.add(std::make_unique<F>(juce::ParameterID{"tone",1},"Tone",juce::NormalisableRange<float>(0.0f,1.0f),0.5f));
    l.add(std::make_unique<F>(juce::ParameterID{"drive",1},"Drive",juce::NormalisableRange<float>(0.0f,1.0f),0.15f));
    l.add(std::make_unique<F>(juce::ParameterID{"flangeManual",1},"Flanger Manual / Tune",juce::NormalisableRange<float>(0.f,1.f),0.5f));
    l.add(std::make_unique<F>(juce::ParameterID{"flangeFeedback",1},"Flanger Resonance / Color",juce::NormalisableRange<float>(-0.9f,0.9f),0.25f));
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
    for(auto& channel:voices) for(auto& voice:channel) voice.reset((int)(fs*0.85)+16);
    for(int ch=0;ch<2;++ch) {dryDelay[(size_t)ch].assign((size_t)(fs*0.7)+16,0.f);dryWrite[(size_t)ch]=0;}
}
void ChorusProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int channels=juce::jmin(buffer.getNumChannels(),2);
    if(channels<=0 || voices[0][0].delay.empty()) return;
    const int model=juce::jlimit(0,10,(int)*parameters.getRawParameterValue("model"));
    const float rate=parameters.getRawParameterValue("rate")->load();
    const float depth=*parameters.getRawParameterValue("depth");
    const float mix=*parameters.getRawParameterValue("mix");
    const float width=*parameters.getRawParameterValue("width");
    
    const float drive=parameters.getRawParameterValue("drive")->load(); // DMM preamp gain only.
    const float gain=juce::Decibels::decibelsToGain(parameters.getRawParameterValue("level")->load());
    const int quality=(int)*parameters.getRawParameterValue("quality");
    const int mode=juce::jlimit(0,3,(int)*parameters.getRawParameterValue("mode"));
    const bool stereoMonoChorus=parameters.getRawParameterValue("monoStereo")->load()>.5f;
    const int voicesUsed=(model==7||flanger)?1:(model==3?3:(model>=5?6:(model==2?2:(model==4?2:1))));
    const int junoMode=juce::jlimit(0,2,(int)parameters.getRawParameterValue("junoMode")->load());
    const float dmmDelay=parameters.getRawParameterValue("dmmDelay")->load();
    const float dmmShortDelay=parameters.getRawParameterValue("dmmShortDelay")->load();
    const bool dmmVibrato=parameters.getRawParameterValue("dmmMode")->load()>.5f;
    const bool dmmShort=parameters.getRawParameterValue("dmmEngine")->load()<0.5f;
    const float dmmFeedback=parameters.getRawParameterValue("dmmFeedback")->load();
    const float flangeManual=parameters.getRawParameterValue("flangeManual")->load();
    const float flangeFeedback=parameters.getRawParameterValue("flangeFeedback")->load();
    const bool flanger=model>=8;
    const float baseMs[]={512000.f/131000.f,260.0f,8.0f,11.0f,18.0f,21.0f,19.0f,7.5f};
    const float sweepMs[]={0.85f,2.2f,0.65f,3.5f,3.0f,4.0f,3.2f,1.1f};
    const float rates[]={0.55f,1.0f,0.45f,0.72f,0.48f,0.31f,0.34f,1.0f};
    const float bandwidth[]={6500.0f,4300.0f,6700.0f,8000.0f,5300.0f,10500.0f,14500.0f,7000.f};
    for(int sample=0;sample<buffer.getNumSamples();++sample)
    {
        for(int ch=0;ch<channels;++ch)
        {
            const float dry=buffer.getSample(ch,sample);
            float wet=0.0f;
            for(int v=0;v<voicesUsed;++v)
            {
                auto& s=voices[(size_t)ch][(size_t)v];
                const double phaseOffset=(model==3||model>=5||model==2)?(double)v/(double)voicesUsed:(model==4&&v==1?0.5:0.0);
                // CE-1/CE-2: original mono uses the same LFO on both channels;
                // optional stereo puts the right channel 180 degrees out of phase.
                const double stereoOffset=(model<=1||flanger)
                    ? ((stereoMonoChorus && ch==1)?0.5:0.0)
                    : (model==7?(junoMode==2?0.0:(ch==1?0.5:0.0)):((ch==1)?width*(model==2?0.5:0.25):0.0));
                // Separate phase accumulators per voice/channel allow independent architecture and stable stereo offsets.
                const double phase=s.phase+phaseOffset+stereoOffset;
                const float sine=(float)std::sin(2.0*pi*phase);
                const float triangle=(float)(2.0/pi*std::asin(std::sin(2.0*pi*phase)));
                // DMM switch changes LFO timing, not the oscillator waveform.
                const float lfo=(model==0||model==1||model==7)?triangle:(model==2?(0.75f*sine+0.25f*triangle):sine);
                const float jitter=(model>=4)?0.035f*std::sin((float)(phase*2.0*pi*7.13)):0.0f;
                // Dimension modes select fixed modulation patterns, not a user sweep.
                const float dimensionDepth[]={0.32f,0.48f,0.62f,0.76f};
                const float modeDepth[]={0.55f,0.78f,1.0f,1.0f};
                const float effectiveDepth=model==2?dimensionDepth[mode]:(model>=4&&model<=6?modeDepth[juce::jmin(mode,2)]:depth);
                const float effectiveRate=rate;
                // The three digital mode banks select different tap arrangements.
                const float digitalModeDelay=model==4?(mode==0?0.0f:(mode==1?4.f:9.f))
                    :(model==5?(mode==0?0.f:(mode==1?3.f:6.f))
                    :(model==6?(mode==0?1.f:(mode==1?4.5f:7.5f)):0.f));
                // Full mode retains 4096 effective BBD positions; short mode
                // uses 1024 clocked positions and the same triangle-driven
                // oscillator character, rather than a moving read pointer.
                const float dmmBase=dmmShort?dmmShortDelay:dmmDelay;
                const float dmmSweep=2.2f*depth; // Provisional maximum excursion; not measured from hardware. // LFO controls clock frequency; short mode uses a reduced-stage BBD.
                const float spx990Lfo=model==6?(0.82f*sine+0.18f*std::sin(4.0*pi*phase)):lfo;
                const float junoRate=junoMode==0?0.513f:(junoMode==1?0.863f:9.75f);
                const float junoSweep=junoMode==2?0.2f:1.845f; // measured half-range
                const float flangerBase=model==8?(1.f+12.f*flangeManual)*(mode==1?2.f:1.f)
                    :(model==9?1.f+6.4f*flangeManual:(mode==2?12.f+45.f*flangeManual:(mode==3?2.f+12.f*flangeManual:0.8f+9.f*flangeManual)));
                const bool staticFlange=(model==9&&mode==1)||(model==10&&(mode==1||mode==2));
                const float flangeDepth=model==8?juce::jmin(flangerBase-0.5f,5.8f)*depth
                    :(model==9?juce::jmin(flangerBase-0.5f,3.2f)*depth
                    :juce::jmin(flangerBase-0.5f,model==10&&mode==3?5.f:4.f)*depth);
                const float flangerDelay=juce::jmax(0.5f,flangerBase+(staticFlange?0.f:flangeDepth*triangle));
                const float dMs=flanger?flangerDelay:(model==1?(dmmShort?dmmBase/(1.f+juce::jlimit(-0.32f,0.32f,(dmmSweep/juce::jmax(6.f,dmmBase))*lfo)):dmmBase+dmmSweep*lfo)
                    :(model==7?(junoMode==2?3.5f:3.505f)+junoSweep*lfo
                    :baseMs[model]+digitalModeDelay+effectiveDepth*sweepMs[model]*(spx990Lfo+jitter)));
                // MidiVerb II: independent lower-rate delay memory; its precise clock
                // remains unverified, so 23.4 kHz is an explicit approximation.
                constexpr double midiVerbProcessingHz=23437.5; // DASP-16 internal tick rate, documented by independent emulation research.
                const float delaySamples=(float)((model==4?midiVerbProcessingHz:fs)*0.001)*juce::jmax(1.0f,dMs);
                float input=dry;
                // Independent analogue front-ends: CE-2 BBD, DMM preamp/BBD,
                // and Dimension BBD. These are informed approximations, not SPICE.
                if(model<=2||model==7)
                {
                    const float preHz=model==0?1050.f:(model==1?760.f:(model==7?1400.f:1200.f));
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
                    input=onePole(input,s.antiAlias,model==1?5300.f:(model==7?7000.f:8500.f),fs);
                }
                else if(model>=4&&model<=6)
                {
                    // Model-specific converter input: separate analogue
                    // anti-alias filters, quantisation, and reconstruction.
                    const bool midiverb=model==4;
                    const float adcHz=midiverb?6500.f:(model==5?12000.f:18000.f);
                    input=onePole(input,s.adcLow,adcHz,fs);
                    input=std::tanh(input*(midiverb?1.12f:1.035f));
                    const float steps=midiverb?8192.f:(model==5?16384.f:32768.f); // approximate 13-bit MV-II quantisation
                    // Deterministic triangular dither avoids low-level
                    // quantisation dead zones without a shared random engine.
                    s.rng=s.rng*1664525u+1013904223u;
                    const float d1=(float)((s.rng>>8)&65535u)/65535.f;
                    s.rng=s.rng*1664525u+1013904223u;
                    const float d2=(float)((s.rng>>8)&65535u)/65535.f;
                    input=std::round((input+(d1-d2)/steps)*steps)/steps;
                }
                else input=onePole(input,s.antiAlias,9000.f,fs);
                // DMM feedback is injected at the BBD input, through its own
                // filtered, bounded analogue loop. Other models remain feed-forward.
                if(model==1) input=std::tanh(input+dmmFeedback*s.feedbackSample);
                if(flanger) input=std::tanh(input+flangeFeedback*s.feedbackSample);
                float delayed=0.f;
                if(model<=2||model==7)
                {
                    // Clocked BBD transfer: full DMM 4096 effective positions,
                    // short DMM 1024 effective positions (modified architecture).
                    // Both use a modulated clock, not an interpolated read tap.
                    const int cells=model==1?(dmmShort?1024:4096):(model==7?128:512); // MN3009: 256 stages
                    const float nominalMs=model==1?dmmBase:baseMs[model];
                    
                    const double nominalClock=(double)cells/(0.001*(double)nominalMs);
                    // Delay is inversely proportional to clock frequency.
                    // Clock oscillator is modulated, not a read pointer.
                    // CE-2 reference simulation: about 131 kHz centre and
                    // 108-164 kHz at full depth, yielding ~3.1-4.8 ms.
                    // Other BBD models retain their own clock mapping.
                    const double clockHz=model==0
                        ? (131000.0+28000.0*(double)depth*(double)triangle)
                        : juce::jlimit(3000.0,240000.0,
                            nominalClock*(double)nominalMs/(double)juce::jmax(0.5f,dMs));
                    s.clockPhase+=clockHz/fs;
                    // At extreme host rates, process every elapsed clock tick
                    // to avoid losing bucket transfers.
                    const int ticks=(int)s.clockPhase;
                    s.clockPhase-=ticks;
                    for(int tick=0;tick<ticks;++tick)
                    {
                        const float stored=s.bbd[(size_t)s.bbdWrite];
                        // Bounded charge transfer, with a small per-stage loss.
                        // Clock-edge sampling, finite charge-transfer efficiency and
                        // a small amount of signal-dependent transfer error.
                        const float previous=s.bbd[(size_t)s.bbdWrite];
                        const float efficiency=model==1?(dmmShort?0.9988f:0.9997f):(model==7?0.9993f:0.9995f);
                        const float transferred=efficiency*input+(1.f-efficiency)*previous;
                        s.bbd[(size_t)s.bbdWrite]=juce::jlimit(-1.5f,1.5f,transferred);
                        s.bbdWrite=(s.bbdWrite+1)%cells;
                        s.previousBbd=s.heldBbd;
                        s.heldBbd=stored;
                    }
                    // Output reconstruction operates on held charge packets.
                    delayed=s.heldBbd;
                }
                else if(model==4)
                {
                    // Original independent sample-rate implementation: sample/quantise
                    // into a fixed-rate delay memory, not the host-rate delay buffer.
                    // Multiple ticks are handled at low host sample rates.
                    s.digitalPhase+=midiVerbProcessingHz/fs;
                    const int ticks=(int)s.digitalPhase;
                    s.digitalPhase-=ticks;
                    for(int tick=0;tick<ticks;++tick)
                    {
                        s.push(input);
                        s.digitalHeld=s.read(delaySamples);
                    }
                    delayed=s.digitalHeld;
                }
                else
                {
                    s.push(input);
                    delayed=s.read(delaySamples);
                }
                if(model==1) s.feedbackSample=onePole(delayed,s.feedbackLow,3600.f,fs);
                if(flanger) s.feedbackSample=onePole(delayed,s.feedbackLow,model==8?4400.f:5200.f,fs);
                if(model<=2||model==7)
                {
                    delayed=onePole(delayed,s.tone,bandwidth[model],fs);
                    // Matched envelope expansion and de-emphasis; the
                    // nonlinear BBD output is intentionally subtle.
                    const float expansion=std::sqrt(1.f+4.f*s.env);
                    delayed=std::tanh(delayed*expansion*1.08f);
                    delayed=onePole(delayed,s.deEmphasis,model==1?4300.f:6200.f,fs);
                    delayed=highpass(delayed,s.dcIn,s.dc,model==1?28.f:55.f,fs);
                }
                else if(model>=4&&model<=6)
                {
                    const bool midiverb=model==4;
                    delayed=onePole(delayed,s.dacLow,midiverb?5700.f:(model==5?11500.f:17000.f),fs);
                    delayed=onePole(delayed,s.tone,bandwidth[model],fs);
                    delayed=highpass(delayed,s.dcIn,s.dc,midiverb?95.f:24.f,fs);
                }
                else delayed=onePole(delayed,s.tone,flanger?(model==8?5500.f:6500.f):bandwidth[model],fs);
                // Different digital architectures: Midiverb uses two complementary
                // moving taps; SPX90 and SPX990 use multiple decorrelated
                // taps with different weighting and modulation distributions.
                const float weight=flanger?1.f:(model==4?(v==0?.58f:.42f)
                    :(model==5?(v%2==0?1.12f:.88f)/6.f
                    :(model==6?(v%3==0?1.18f:.91f)/6.f:1.f/(float)voicesUsed)));
                wet+=delayed*weight;
                // High-quality mode updates phase twice per sample (same net rate) for LFO numerical consistency.
                const double modeRate=model>=4?(mode==0?0.78:(mode==1?1.0:1.25)):1.0;
                // Original DMM switch changes oscillator timing; Rate provides a
                // user-adjustable trim around the chorus/vibrato ranges.
                const double dmmLfoHz=(dmmVibrato?2.0:0.35)
                    *juce::jmap((double)juce::jlimit(0.05f,6.f,rate),0.05,6.0,0.5,2.0);
                const double ce2Rate=juce::jmap((double)juce::jlimit(0.05f,6.f,rate),0.05,6.0,0.3,3.5);
                const double dimensionRate=mode<2?0.25:0.5; // SDD-320 service notes: 4 s / 2 s modulation periods.
                // Yamaha manual rate limits: SPX90 0.1-20 Hz, SPX990 0.05-40 Hz.
                const double yamahaRate=model==5
                    ? juce::jmap((double)juce::jlimit(0.05f,6.f,rate),0.05,6.0,0.1,20.0)
                    : juce::jmap((double)juce::jlimit(0.05f,6.f,rate),0.05,6.0,0.05,40.0);
                const double flangerRate=model==8?juce::jmap((double)rate,0.05,6.0,0.0625,10.0)
                    :(model==9?juce::jmap((double)rate,0.05,6.0,0.05,12.0)
                    :juce::jmap((double)rate,0.05,6.0,0.1,10.0));
                const double step=(flanger?flangerRate:(model==0?ce2Rate:(model==1?dmmLfoHz:(model==2?dimensionRate:(model==7?junoRate:(model==5||model==6?yamahaRate:(double)(effectiveRate*rates[model]*modeRate)))))))/fs;
                if(quality!=0) {s.phase+=step*0.5; s.phase-=std::floor(s.phase); s.phase+=step*0.5;}
                else s.phase+=step;
                s.phase-=std::floor(s.phase);
            }
            // CE-1/CE-2 retain their dry path; Dimension-like and symphonic architectures use decorrelated wet voices.
            const float wetGain=flanger?0.75f:(model>=5&&model<=6)?0.85f:(model==2?0.72f:1.0f);
            // Vibrato suppresses the dry signal; chorus retains dry/wet
            // mixing. A DMM's chorus/vibrato switch selects LFO character,
            // rather than necessarily selecting a 100%-wet vibrato output.
            // Always blend with the un-delayed input; no dry-path latency compensation.
            const float dryAmount=1.f-mix;
            buffer.setSample(ch,sample,gain*(dryAmount*dry+mix*wet*wetGain));
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
