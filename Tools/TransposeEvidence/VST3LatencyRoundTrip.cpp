// Host-side latency round trip for the Transposer (docs/TRANSPOSE_HYBRID_LATENCY_CONTRACT.md
// section 6). Loads the built VST3 through JUCE's VST3 host, the way a DAW does:
// the quality mode and parameters are set by restoring a modified project state,
// then the host reads getLatencySamples() and plays a picked E2 through it.
//
//   ChimeraVST3LatencyRoundTrip "<build>/.../VST3/SpectralForge Chimera.vst3"
//
// Route: gate, amps and cabinets off (unity, as IntegratedProcessorTests'
// gainAndGR). Checks, per rate and quality:
//   transpose off   the output is the input delayed by the reported latency
//                   (cross-correlation peak at exactly that lag, rho > 0.999)
//   HYBRID -5 / +7  the pick's low-band (< 800 Hz) attack arrives within
//                   [-1, +3] ms of the reported latency (contract: -0.5 ... +2.3 ms)
//   TIGHT -5 / +7   reported only: its 10 ms STFT smears a low attack, so this
//                   10 %-threshold onset starts ~3.7 ms before the reported
//                   latency (no attack-alignment contract for TIGHT)
// Synthetic input; a DAW's own PDC is not exercised, only what it is told.
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <vector>

namespace {
bool failed=false;
void check(bool ok,const juce::String& what){if(!ok){failed=true;std::cout<<"FAIL "<<what<<"\n";}}

std::vector<float> pluck(double rate) {
    std::vector<float> x(size_t(rate*1.5),0.f);
    for(size_t n=size_t(rate*.5);n<x.size();++n){const double t=double(n)/rate-.5;
        for(int k=1;k<=10;++k)x[n]+=float(.05/k*std::sin(juce::MathConstants<double>::twoPi*82.41*k*t)*std::exp(-t*(1+.35*k))*std::min(1.,t/.002));}
    return x;
}
// Two one-pole low-passes at 800 Hz, then the first sample above 10 % of the peak.
double onset(const std::vector<float>& y,double rate) {
    const double a=1-std::exp(-juce::MathConstants<double>::twoPi*800/rate);double s1=0,s2=0,peak=0;
    std::vector<double> l(y.size());
    for(size_t n=0;n<y.size();++n){s1+=a*(y[n]-s1);s2+=a*(s1-s2);l[n]=s2;peak=std::max(peak,std::abs(s2));}
    for(size_t n=0;n<l.size();++n)if(std::abs(l[n])>.1*peak)return double(n);
    return -1;
}
std::unique_ptr<juce::AudioPluginInstance> load(const juce::String& path,double rate,int block) {
    juce::AudioPluginFormatManager formats;formats.addFormat(new juce::VST3PluginFormat());
    juce::OwnedArray<juce::PluginDescription> types;juce::VST3PluginFormat().findAllTypesForFile(types,path);
    if(types.isEmpty())return nullptr;
    juce::String error;return formats.createPluginInstance(*types[0],rate,block,error);
}
// The host-side state is <VST3PluginState><IComponent>base64</IComponent>,
// whose bytes hold the plugin's own block (JUCE binary XML: "VC2!", a 32-bit
// size, the XML) followed by the wrapper's private data. Only that block is
// replaced, as a DAW restoring a project with these settings would deliver it.
bool configure(juce::AudioPluginInstance& p,int quality,bool on,int shift) {
    juce::MemoryBlock state;p.getStateInformation(state);
    auto outer=juce::AudioProcessor::getXmlFromBinary(state.getData(),int(state.getSize()));
    auto* component=outer ? outer->getChildByName("IComponent") : nullptr;
    if(component==nullptr)return false;
    juce::MemoryBlock bytes;if(!bytes.fromBase64Encoding(component->getAllSubText().trim()))return false;
    const auto* raw=static_cast<const char*>(bytes.getData());int at=-1;
    for(int i=0;i+8<=int(bytes.getSize());++i)if(std::memcmp(raw+i,"VC2!",4)==0){at=i;break;}
    if(at<0)return false;
    const int length=8+int(juce::ByteOrder::littleEndianInt(raw+at+4));
    auto xml=juce::AudioProcessor::getXmlFromBinary(raw+at,length);
    if(!xml)return false;
    xml->setAttribute("transposeQuality",quality);
    const std::map<juce::String,double> values{{"transposeon",on?1:0},{"transpose",shift},{"gateon",0},{"input",0},{"output",0},
        {"mode",0},{"boardEnabled",0},{"oversampling",0},{"lowampmix",1},{"ampon1",0},{"ampon2",0},{"ampon3",0},{"cab1",0},{"cab2",0},{"cab3",0}};
    int found=0;
    for(auto* param:xml->getChildWithTagNameIterator("PARAM")) {
        const auto it=values.find(param->getStringAttribute("id"));
        if(it!=values.end()){param->setAttribute("value",it->second);++found;}
    }
    juce::MemoryBlock block;juce::AudioProcessor::copyXmlToBinary(*xml,block);
    juce::MemoryBlock rebuilt(raw,size_t(at));rebuilt.append(block.getData(),block.getSize());
    rebuilt.append(raw+at+length,bytes.getSize()-size_t(at+length));
    component->deleteAllTextElements();component->addTextElement(rebuilt.toBase64Encoding());
    juce::MemoryBlock modified;juce::AudioProcessor::copyXmlToBinary(*outer,modified);
    p.setStateInformation(modified.getData(),int(modified.getSize()));
    return found==int(values.size());
}
std::vector<float> render(juce::AudioPluginInstance& p,const std::vector<float>& x,int block) {
    std::vector<float> y(x.size());juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer(2,block);
    for(size_t at=0;at<x.size();at+=size_t(block)) {
        const int n=int(std::min<size_t>(size_t(block),x.size()-at));buffer.setSize(2,n,false,false,true);
        for(int i=0;i<n;++i){buffer.setSample(0,i,x[at+size_t(i)]);buffer.setSample(1,i,x[at+size_t(i)]);}
        p.processBlock(buffer,midi);
        for(int i=0;i<n;++i)y[at+size_t(i)]=buffer.getSample(0,i);
    }
    return y;
}
}

int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    if(argc<2){std::cerr<<"usage: ChimeraVST3LatencyRoundTrip <plugin.vst3>\n";return 2;}
    const juce::String path(argv[1]);const int block=128;
    const char* names[]{"CLASSIC","LOW-LAT","TIGHT","HYBRID"};
    for(double rate:{44100.,48000.,96000.}) {
        const auto x=pluck(rate);const double xOnset=onset(x,rate);
        for(int quality:{2,3}) {
            for(int shift:{0,-5,7}) {
                auto p=load(path,rate,block);
                if(!p){std::cout<<"FAIL could not load "<<path<<"\n";return 1;}
                check(configure(*p,quality,shift!=0,shift),"state is missing an expected parameter");
                p->prepareToPlay(rate,block);
                const int reported=p->getLatencySamples();
                const auto y=render(*p,x,block);
                p->releaseResources();
                if(shift==0) {
                    // Exact delay: correlation peak over lags 0..200 ms.
                    int best=-1;double bestRho=-2;
                    for(int lag=0;lag<int(rate*.2);++lag) {
                        double xy=0,xx=0,yy=0;
                        for(size_t n=size_t(rate*.5);n+size_t(lag)<y.size();n+=2){xy+=double(x[n])*y[n+size_t(lag)];xx+=double(x[n])*x[n];yy+=double(y[n+size_t(lag)])*y[n+size_t(lag)];}
                        const double rho=xy/std::sqrt(xx*yy+1e-30);if(rho>bestRho){bestRho=rho;best=lag;}
                    }
                    std::cout<<"MEASURE "<<rate<<" Hz "<<names[quality]<<" transpose off: reported "<<reported<<" samples, measured delay "<<best<<" (rho "<<bestRho<<")\n";
                    check(best==reported && bestRho>.999,juce::String(rate)+" Hz "+names[quality]+" off: output is not the input delayed by the reported latency");
                } else {
                    const double error=1000*(onset(y,rate)-xOnset-reported)/rate;
                    std::cout<<"MEASURE "<<rate<<" Hz "<<names[quality]<<" "<<(shift>0?"+":"")<<shift<<": reported "<<reported<<" samples ("<<1000.*reported/rate
                             <<" ms), low-band attack arrives "<<error<<" ms after it\n";
                    if(quality==3)check(error>=-1 && error<=3,juce::String(rate)+" Hz "+names[quality]+" "+juce::String(shift)+": attack outside [-1, +3] ms of the reported latency");
                }
            }
        }
    }
    std::cout<<(failed ? "FAIL" : "PASS")<<" VST3 host latency round trip (JUCE VST3 host, synthetic E2 pick; DAW PDC itself not exercised)\n";
    return failed ? 1 : 0;
}
