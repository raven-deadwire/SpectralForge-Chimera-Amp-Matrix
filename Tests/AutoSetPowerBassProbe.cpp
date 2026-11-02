#include "AutoSetPower.h"
#include "CabLayoutModel.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

// An observational probe, not a listening test. All inputs below are generated
// harmonic signals. The cabinet is the product's authored model, not a measured
// cabinet IR. The independent measurements deliberately do not call
// measureSpectrum: its first centre is 50 Hz in the Phase 12 baseline.
namespace {
using namespace spectralforge;
using namespace spectralforge::autoset;
constexpr double pi=3.14159265358979323846;
constexpr std::array<double,2> pitches{27.5,30.86770632850775};

void require(bool condition,const std::string& message) {
    if(!condition)throw std::runtime_error(message);
}
double db(double amplitude) {return 20*std::log10(std::max(amplitude,1e-15));}
double powerDb(double power) {return 10*std::log10(std::max(power,1e-30));}

const char* familyName(int family) {
    return family==0?"harmonic_rich":family==1?"fundamental_dominant":"harmonic_gap";
}
std::vector<float> bassInput(double rate,double f0,int family) {
    std::vector<float> samples(size_t(std::llround(ampRenderSeconds*rate)));
    // Repeated 500 ms notes, alternating pick levels. All profiles contain f0,
    // h2..h8 and higher partials. They are fixtures, not acoustic bass models.
    constexpr std::array<double,6> levels{1.,.50,.85,.35,.70,.95};
    double peak=0;
    for(size_t n=0;n<samples.size();++n) {
        const double t=double(n)/rate,age=std::fmod(t,.5);
        const int note=std::min(5,int(t/.5));
        const double envelope=(1-std::exp(-age/.004))*(.18+.82*std::exp(-age/.18))
            *std::min(1.,(.5-age)/.008)*levels[size_t(note)];
        double wave=0;
        for(int h=1;h<=(family==2?128:96) && h*f0<rate*.4;++h) {
            // Harmonic-gap control: an explicit stress signal with strong f0,
            // weak h2..h8 and retained upper partials. It isolates the failure
            // to observe sub-40 Hz energy without claiming a real bass timbre.
            const double amplitude=family==2?(h==1?1.:h<9?.015/h:.18/std::pow(double(h),.4))
                :h==1?1.:(family==1?.35/std::pow(double(h),1.45):1./std::pow(double(h),.95));
            wave+=amplitude*std::sin(2*pi*f0*h*t+(family==2?.17*h:.17*double(h*h%11)));
        }
        samples[n]=family==2?float(.08*wave*(1-std::exp(-age*1200))*std::exp(-age*4)):float(envelope*wave);
        peak=std::max(peak,std::abs(double(samples[n])));
    }
    if(family!=2)for(auto& value:samples)value=float(value*.18/peak);
    return samples;
}

void lowShelf(std::vector<float>& samples,double rate,double frequency,double gainDb) {
    if(gainDb==0)return;
    const auto c=eqCoefficients(1,frequency,gainDb,.707,rate);double z1=0,z2=0;
    for(auto& v:samples){const double y=c.b0*v+z1;z1=c.b1*v-c.a1*y+z2;z2=c.b2*v-c.a2*y;v=float(y);}
}

struct CabFilter {
    std::vector<float> impulse;
    explicit CabFilter(double rate) {
        cabLayout::Settings settings;
        settings.layout=6; // Authored Bass 4x10, one front microphone.
        settings.voice.base={true,1,0,1,0,0,.25,10};
        settings.voice.driver=9; // Foundry 10.
        settings.voice.mic=10; // Silk 121 ribbon model.
        impulse=cabLayout::generate(cabLayout::key(settings),rate);
    }
    std::vector<float> apply(const std::vector<float>& input,double rate) const {
        constexpr int blockSize=256;
        juce::AudioBuffer<float> ir(1,int(impulse.size()));
        ir.copyFrom(0,0,impulse.data(),int(impulse.size()));
        juce::dsp::Convolution convolution;
        convolution.loadImpulseResponse(std::move(ir),rate,juce::dsp::Convolution::Stereo::no,
                                        juce::dsp::Convolution::Trim::no,juce::dsp::Convolution::Normalise::no);
        convolution.prepare({rate,blockSize,1});
        std::vector<float> output(input.size());
        juce::AudioBuffer<float> block(1,blockSize);
        for(size_t offset=0;offset<input.size();offset+=blockSize) {
            const int count=std::min(blockSize,int(input.size()-offset));block.clear();
            block.copyFrom(0,0,input.data()+offset,count);
            auto audio=juce::dsp::AudioBlock<float>(block).getSubBlock(0,size_t(count));
            juce::dsp::ProcessContextReplacing<float> context(audio);convolution.process(context);
            std::copy_n(block.getReadPointer(0),count,output.begin()+std::ptrdiff_t(offset));
        }
        require(convolution.getLatency()==0,"Probe assumes the default zero-latency convolution");
        return output;
    }
};

struct Metrics {
    double f0Db{},subDb{},lowDb{},bodyDb{},referenceDb{},biteDb{},rmsDb{},peakDb{},crestDb{},compression{};
    double narrowExcessDb{},wideExcessDb{},wide160ExcessDb{};
};
Metrics measure(const std::vector<float>& input,const std::vector<float>& output,double rate,double f0,Style style) {
    Metrics m;
    require(output.size()>size_t(std::llround(ampWarmupSeconds*rate)),"No samples after analysis warmup");
    const int order=int(std::ceil(std::log2(rate/.5))),fftSize=1<<order; // <= 0.5 Hz bins
    const size_t count=std::min(size_t(fftSize),output.size()-size_t(std::llround(ampWarmupSeconds*rate)));
    const size_t start=output.size()-count;
    require(count>0,"No analysis samples");
    juce::dsp::FFT fft(order);std::vector<float> spectrum(size_t(2*fftSize));
    double energy=0,peak=0,windowSum=0,windowEnergy=0,cosine=0,sine=0;
    for(size_t n=0;n<count;++n) {
        const double value=output[start+n],w=.5-.5*std::cos(2*pi*double(n)/double(count));
        require(std::isfinite(value),"Non-finite native/cab output");
        spectrum[n]=float(w*value);energy+=value*value;peak=std::max(peak,std::abs(value));
        windowSum+=w;windowEnergy+=w*w;
        const double phase=2*pi*f0*double(start+n)/rate;
        cosine+=w*value*std::cos(phase);sine+=w*value*std::sin(phase);
    }
    fft.performFrequencyOnlyForwardTransform(spectrum.data(),true);
    const auto band=[&](double low,double high) {
        double power=0;
        const int first=std::max(1,int(std::ceil(low*fftSize/rate))),last=std::min(fftSize/2,int(std::ceil(high*fftSize/rate)));
        for(int k=first;k<last;++k)power+=double(spectrum[size_t(k)])*spectrum[size_t(k)];
        return powerDb(2*power/(double(fftSize)*windowEnergy));
    };
    // f0Db is coherent peak amplitude; band/rms fields are mean-square power.
    m.f0Db=db(2*std::hypot(cosine,sine)/windowSum);
    m.subDb=band(25,40);m.lowDb=band(40,100);m.bodyDb=band(100,250);
    m.referenceDb=band(400,1600);m.biteDb=band(1250,3150);
    m.rmsDb=db(std::sqrt(energy/double(count)));m.peakDb=db(peak);m.crestDb=m.peakDb-m.rmsDb;
    m.compression=ampCompression(input,output,rate); // 20 ms RMS spread reduction, not a clipping count.
    // Experimental alternatives: add 25/31.5/40 Hz thirds locally and extend
    // the existing bass target below its 40 Hz endpoint by clamping it. This
    // is an explicit hypothesis for comparison, not an approved tonal target.
    const auto excess=[&](double low,double high) {
        constexpr std::array<double,14> centres{25,31.5,40,50,63,80,100,125,160,400,500,630,800,1000};
        const auto sum=[&](bool target,double from,double to) {
            double total=0;
            for(double fc:centres)if(fc>=from && fc<=to)
                total+=std::pow(10.,(target?toneTargetDb(true,float(fc),style):band(fc/std::pow(2.,1./6.),fc*std::pow(2.,1./6.)))/10.);
            for(double fc:{1250.,1600.})if(fc>=from && fc<=to)
                total+=std::pow(10.,(target?toneTargetDb(true,float(fc),style):band(fc/std::pow(2.,1./6.),fc*std::pow(2.,1./6.)))/10.);
            return powerDb(total);
        };
        return (sum(false,low,high)-sum(false,400,1600))-(sum(true,low,high)-sum(true,400,1600));
    };
    m.narrowExcessDb=excess(50,100);m.wideExcessDb=excess(25,100);m.wide160ExcessDb=excess(25,160);
    return m;
}

std::string quoted(const std::string& text) {
    std::string result="\"";
    for(char c:text){if(c=='\"')result+='\"';result+=c;}return result+'\"';
}
void metricsHeader(std::ostream& out,const char* prefix) {
    for(const auto* field:{"f0_db","sub25_40_db","low40_100_db","body100_250_db","ref400_1600_db","bite1250_3150_db","rms_db","peak_db","crest_db","compression","experimental_narrow_excess_db","experimental_25_100_excess_db","experimental_25_160_excess_db"})
        out<<','<<prefix<<'_'<<field;
}
void metricsRow(std::ostream& out,const Metrics& m) {
    for(double value:{m.f0Db,m.subDb,m.lowDb,m.bodyDb,m.referenceDb,m.biteDb,m.rmsDb,m.peakDb,m.crestDb,m.compression,m.narrowExcessDb,m.wideExcessDb,m.wide160ExcessDb})out<<','<<value;
}

void writeWave(const std::filesystem::path& path,const std::vector<float>& samples,double rate) {
    juce::WavAudioFormat format;
    auto stream=juce::File(path.string()).createOutputStream();
    require(bool(stream),"Cannot open wave output");
    auto writer=std::unique_ptr<juce::AudioFormatWriter>(format.createWriterFor(stream.release(),rate,1,32,{},0));
    require(bool(writer),"Cannot create wave writer");
    const float* channels[]{samples.data()};
    require(writer->writeFromFloatArrays(channels,1,int(samples.size())),"Cannot write wave");
}

struct Options {
    std::string csv,audio;
    int model{-1},channel{-1},fixture{-1};double rate{},shelfDb{},shelfFrequency{40};
    bool quick{},driven{},extraChannels{true},dryOnly{};Style style{Style::metal};
};
Options parse(int argc,char** argv) {
    Options o;
    for(int i=1;i<argc;++i) {
        const std::string arg=argv[i];
        const auto next=[&](){require(i+1<argc,"Missing argument to "+arg);return std::string(argv[++i]);};
        if(arg=="--csv")o.csv=next();else if(arg=="--audio-dir")o.audio=next();
        else if(arg=="--model")o.model=std::stoi(next());else if(arg=="--channel")o.channel=std::stoi(next());
        else if(arg=="--rate")o.rate=std::stod(next());else if(arg=="--shelf-db")o.shelfDb=std::stod(next());
        else if(arg=="--shelf-frequency")o.shelfFrequency=std::stod(next());
        else if(arg=="--quick")o.quick=true;else if(arg=="--driven")o.driven=true;
        else if(arg=="--default-channels-only")o.extraChannels=false;
        else if(arg=="--dry-only")o.dryOnly=true;
        else if(arg=="--fixture") {
            const auto fixture=next();
            for(int family=0;family<3;++family)if(fixture==familyName(family))o.fixture=family;
            require(o.fixture>=0,"Fixture must be harmonic_rich, fundamental_dominant or harmonic_gap");
        }
        else if(arg=="--style") {const auto style=next();require(style=="clean" || style=="rock" || style=="metal","Unknown style");o.style=style=="clean"?Style::clean:style=="rock"?Style::rock:Style::metal;}
        else throw std::runtime_error("Unknown argument: "+arg);
    }
    if(o.quick){if(o.rate==0)o.rate=48000;o.extraChannels=false;}
    require(o.rate==0 || o.rate==44100 || o.rate==48000 || o.rate==96000,"Rate must be 44100, 48000 or 96000");
    return o;
}
}

int main(int argc,char** argv) {
    try {
        const auto options=parse(argc,argv);
        std::ofstream file;
        if(!options.csv.empty()){file.open(options.csv);require(bool(file),"Cannot open CSV");}
        std::ostream& csv=options.csv.empty()?std::cout:file;
        csv<<std::setprecision(10);
        csv<<"model,head,channel,channel_name,rate,f0,fixture,cab,style,driven,shelf_db,shelf_hz,playing,p_before,r_before,p_after,r_after,p_error_before,r_error_before,p_error_after,r_error_after,total_error_delta,inner_analysis_render_calls,inner_analysis_ms";
        for(const char* name:{"before","after","presence_only","resonance_only","grid_presence","grid_resonance","grid_both"})metricsHeader(csv,name);
        csv<<",proposal_sub_interaction_db,proposal_f0_interaction_db,grid_p,grid_r,grid_sub_interaction_db,grid_f0_interaction_db\n";
        size_t cases=0,moves=0,worse=0;std::map<int,CabFilter> cabinets;
        if(!options.audio.empty())std::filesystem::create_directories(options.audio);
        for(const int model:{2,15,20,21,24,25}) {
            if(options.model>=0 && model!=options.model)continue;
            if(options.quick && options.model<0 && model!=2 && model!=25)continue;
            auto defaultState=defaultAmpNativeState(model);
            const auto& panel=ampNativePanel(model);
            std::vector<int> channels{defaultState.channel};
            if(options.channel>=0)channels={options.channel};
            else if(options.extraChannels && model==25) {
                for(int c=0;c<int(panel.channels.size());++c)if(c!=defaultState.channel)channels.push_back(c);
            }
            for(const int channel:channels) {
                require(channel>=0 && channel<int(panel.channels.size()),"Invalid channel");
                const bool extraChannel=options.channel<0 && channel!=defaultState.channel;
                auto state=defaultState;state.channel=channel;
                const std::array<int,2> controls{powerControl(model,channel,powerPresence),powerControl(model,channel,powerResonance)};
                require(controls[0]>=0 && controls[1]>=0,"Probe head lacks power knobs");
                if(options.driven) {
                    const int gain=easy::ampControl(model,channel,state.inputRoute,easy::Role::gain);
                    if(gain>=0){const auto& c=panel.controls[size_t(gain)];state.values[size_t(gain)]=c.minimum+.85f*(c.maximum-c.minimum);}
                    for(size_t c=0;c<panel.controls.size();++c)if(std::string(panel.controls[c].label)=="MASTER")state.values[c]=panel.controls[c].minimum+.85f*(panel.controls[c].maximum-panel.controls[c].minimum);
                }
                std::array<AmpKnob,2> knobs{};
                for(int role=0;role<2;++role){const auto& c=panel.controls[size_t(controls[size_t(role)])];knobs[size_t(role)]={true,state.values[size_t(controls[size_t(role)])],c.minimum,c.maximum};}
                const std::array<float,2> before{knobs[0].value,knobs[1].value};
                for(double rate:{44100.,48000.,96000.}) {
                    if(options.rate!=0 && options.rate!=rate)continue;
                    if(cabinets.find(int(rate))==cabinets.end())cabinets.emplace(int(rate),CabFilter(rate));
                    for(const double f0:pitches)for(int family:{0,1,2}) {
                        if(options.fixture>=0 && family!=options.fixture)continue;
                        if(extraChannel && family!=0)continue;
                        const auto input=bassInput(rate,f0,family);
                        const auto render=nativePowerRenderer(state,{},controls,input,rate);
                        const auto baseline=render(before);
                        for(bool cabinet:{false,true}) {
                            if(options.dryOnly && cabinet)continue;
                            if(extraChannel && !cabinet)continue;
                            const auto postProcess=[&](const std::vector<float>& head){auto result=cabinet?cabinets.at(int(rate)).apply(head,rate):head;lowShelf(result,rate,options.shelfFrequency,options.shelfDb);return result;};
                            const auto post=postProcess(baseline);
                            int renders=0;
                            const PowerRenderer observed=[&](const auto& values){++renders;return render(values);};
                            const auto started=std::chrono::steady_clock::now();
                            const auto result=analysePower(input,post,baseline,rate,knobs,observed,true,options.style);
                            const double milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
                            require(result.playingDetected,"Synthetic fixture not detected as playing");
                            const std::array<float,2> after{result.proposed[0].value,result.proposed[1].value};
                            for(int role=0;role<2;++role)require(std::abs(after[size_t(role)]-before[size_t(role)])<=powerMaxMove*(knobs[size_t(role)].maximum-knobs[size_t(role)].minimum)+1e-5f,"Proposal exceeds move limit");
                            const auto changedOutput=postProcess(render(after));
                            const auto m0=measure(input,post,rate,f0,options.style),m1=measure(input,changedOutput,rate,f0,options.style);
                            const auto mp=measure(input,postProcess(render({after[0],before[1]})),rate,f0,options.style);
                            const auto mr=measure(input,postProcess(render({before[0],after[1]})),rate,f0,options.style);
                            // Fixed positive perturbations expose interaction even if the
                            // proposal leaves one or both knobs unchanged.
                            const std::array<float,2> grid{std::min(knobs[0].maximum,before[0]+.2f*(knobs[0].maximum-knobs[0].minimum)),std::min(knobs[1].maximum,before[1]+.2f*(knobs[1].maximum-knobs[1].minimum))};
                            const auto gp=measure(input,postProcess(render({grid[0],before[1]})),rate,f0,options.style);
                            const auto gr=measure(input,postProcess(render({before[0],grid[1]})),rate,f0,options.style);
                            const auto gb=measure(input,postProcess(render(grid)),rate,f0,options.style);
                            const auto total=[&](const auto& e){return powerCost(e[0],0,true,options.style)+powerCost(e[1],1,true,options.style);};
                            const double errorDelta=total(result.excessAfterDb)-total(result.excessBeforeDb);
                            csv<<model<<','<<quoted(ampInfo(model).name)<<','<<channel<<','<<quoted(panel.channels[size_t(channel)])<<','<<rate<<','<<f0<<','<<familyName(family)<<','<<(cabinet?"authored_bass4x10":"dry_head")<<','<<styleName(options.style)<<','<<int(options.driven)<<','<<options.shelfDb<<','<<options.shelfFrequency<<','<<int(result.playingDetected)<<','<<before[0]<<','<<before[1]<<','<<after[0]<<','<<after[1]<<','<<result.excessBeforeDb[0]<<','<<result.excessBeforeDb[1]<<','<<result.excessAfterDb[0]<<','<<result.excessAfterDb[1]<<','<<errorDelta<<','<<renders<<','<<milliseconds;
                            for(const auto& m:{m0,m1,mp,mr,gp,gr,gb})metricsRow(csv,m);
                            csv<<','<<m1.subDb-mp.subDb-mr.subDb+m0.subDb<<','<<m1.f0Db-mp.f0Db-mr.f0Db+m0.f0Db<<','<<grid[0]<<','<<grid[1]<<','<<gb.subDb-gp.subDb-gr.subDb+m0.subDb<<','<<gb.f0Db-gp.f0Db-gr.f0Db+m0.f0Db<<'\n';csv.flush();
                            ++cases;moves+=result.changedKnobs();worse+=errorDelta>1e-4;
                            std::cerr<<"CASE "<<cases<<" model="<<model<<" channel="<<channel<<" rate="<<rate<<" f0="<<f0<<" "<<familyName(family)<<" cab="<<cabinet<<" P="<<before[0]<<"->"<<after[0]<<" R="<<before[1]<<"->"<<after[1]<<" sub_delta="<<m1.subDb-m0.subDb<<" f0_delta="<<m1.f0Db-m0.f0Db<<" error_delta="<<errorDelta<<'\n';
                            if(!options.audio.empty()) {
                                const auto stem=std::to_string(model)+"-ch"+std::to_string(channel)+"-"+std::to_string(int(rate))+"-"+(f0<28?"A0":"B0")+"-"+familyName(family)+"-"+(cabinet?"cab":"dry");
                                writeWave(std::filesystem::path(options.audio)/(stem+"-before.wav"),post,rate);
                                writeWave(std::filesystem::path(options.audio)/(stem+"-after.wav"),changedOutput,rate);
                            }
                        }
                    }
                }
            }
        }
        require(cases>0,"No cases selected");
        std::cerr<<"SYNTHETIC ONLY: "<<cases<<" cases; "<<moves<<" knob changes; "<<worse<<" increases in combined reported error. No real DI or listening validation.\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
