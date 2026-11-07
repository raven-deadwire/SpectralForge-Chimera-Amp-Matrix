#pragma once
// What is plugged in? Auto Set listens to the signal going into the amps for
// the input pass and classifies the source: guitar, bass, vocal, keys/synth,
// percussion, or unknown. Offline analysis on the message thread; never on
// the audio thread.
//
// Cues (each measured, none decisive alone):
//   - pitch (McLeod NSDF on a 12 kHz copy): how often it is pitched, its
//     range, and how it moves inside a note (vibrato, glides) or holds still;
//   - envelope: attack time (2 ms frames) and what follows the peak: a plucked
//     string decays, a voice or an organ holds, a drum dies within ~200 ms;
//   - spectrum: energy below 71 Hz against the 71 Hz-1.4 kHz body (the
//     guitar/bass cue Auto Set used before), and high-band noise.
// A non-guitar/bass verdict needs every cue of its class and a clear margin;
// otherwise the source is reported as guitar or bass with lower confidence,
// so Auto Set keeps its guitar/bass behaviour when in doubt.
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace spectralforge::autoset {

enum class Source {unknown=0,guitar,bass,vocal,keys,percussion};
inline const char* sourceName(Source s) noexcept {
    switch(s){case Source::guitar:return "Guitar";case Source::bass:return "Bass";case Source::vocal:return "Vocal";
              case Source::keys:return "Keys / synth";case Source::percussion:return "Percussion";default:return "Unknown";}
}
inline bool stringInstrument(Source s) noexcept {return s==Source::guitar || s==Source::bass;}
// Whether Auto Set keeps its amp, boost, cabinet and routing steps. Keys are
// often played through amps (organ, Rhodes), and a guitar through a drive or
// compressor pedal holds its notes like keys, so only a voice or drums make
// Auto Set leave the amp and cabinet alone.
inline bool keepsAmpSteps(Source s) noexcept {return s!=Source::vocal && s!=Source::percussion;}

struct InstrumentFeatures {
    bool playing{};
    float voicedFraction{},f0Low{},f0Median{},f0High{};  // f0 percentiles 10/50/90 over pitched frames (Hz)
    float attackMs{},decayDbPerSecond{},shortDecayFraction{}; // medians over notes; share of notes gone (-20 dB) within 250 ms
    float vibratoCents{},glideFraction{};                 // pitch movement inside held notes
    float lowRatioDb{-120.f},noiseRatioDb{-120.f};        // <71 Hz vs body; 3-8 kHz noise floor vs body
    float fundamentalDb{-120.f};                          // median share of a pitched frame's power at its own pitch (dB)
    float pitchedOnsetFraction{};                         // hits whose first 100 ms are pitched
    int notes{},heldSegments{};
};
struct InstrumentResult {
    Source source{Source::unknown};
    float confidence{};          // 0..1
    bool bass{};                 // guitar/bass call (also the fallback for the other classes)
    bool bassDecisive{};         // pitch range settled guitar vs bass (otherwise the low-energy cue did)
    InstrumentFeatures features;
    std::array<float,6> scores{};
    juce::String reason;
};

namespace instrument_detail {
inline float quantile(std::vector<float> v,float q) {
    if(v.empty())return 0.f;
    const auto k=size_t(std::clamp(q,0.f,1.f)*float(v.size()-1));
    std::nth_element(v.begin(),v.begin()+std::ptrdiff_t(k),v.end());return v[k];
}
// 0 below `from`, 1 above `to` (or the reverse when to<from), smooth between.
inline float ramp(float x,float from,float to) {
    if(from==to)return x>=to?1.f:0.f;
    const float t=std::clamp((x-from)/(to-from),0.f,1.f);return t*t*(3-2*t);
}
inline float dbOf(double power) {return float(10*std::log10(std::max(1e-30,power)));}

// Low-pass twice at 5 kHz and resample to 12 kHz (linear interpolation).
inline std::vector<float> to12k(const std::vector<float>& x,double rate) {
    constexpr double target=12000;
    std::vector<float> y(x);
    if(rate>target*1.01) {
        for(int pass=0;pass<2;++pass) {
            const double w=2*juce::MathConstants<double>::pi*5000/rate,q=.7071,a=std::sin(w)/(2*q),c=std::cos(w);
            const double b0=(1-c)/2/(1+a),b1=(1-c)/(1+a),b2=b0,a1=-2*c/(1+a),a2=(1-a)/(1+a);
            double z1=0,z2=0;
            for(auto& s:y){const double in=s,out=b0*in+z1;z1=b1*in-a1*out+z2;z2=b2*in-a2*out;s=float(out);}
        }
    }
    const double step=rate/target;std::vector<float> out;out.reserve(size_t(double(y.size())/step)+1);
    for(double p=0;p+1<double(y.size());p+=step){const auto i=size_t(p);const double f=p-double(i);out.push_back(float(y[i]*(1-f)+y[i+1]*f));}
    return out;
}
struct PitchFrame {float hz{},clarity{},fundamental{};}; // fundamental: share of the frame's power at hz itself
// McLeod normalised square difference, hop 120 (10 ms). The long frame (1280
// samples, 107 ms; lags for 25-1500 Hz, so a bass's A0 at 27.5 Hz) reads the
// range; the short one (384, 32 ms; 62-1500 Hz) follows vibrato, which a
// long frame averages away.
inline std::vector<PitchFrame> trackPitch(const std::vector<float>& x12k,bool shortFrame=false) {
    const int frame=shortFrame?384:1280,order=shortFrame?10:12,maxLag=shortFrame?192:480;
    constexpr int hop=120,minLag=8;const int fftSize=1<<order;
    std::vector<PitchFrame> out;if(x12k.size()<size_t(frame))return out;
    juce::dsp::FFT fft(order);std::vector<float> buffer(size_t(2*fftSize));std::vector<double> prefix(size_t(frame+1));
    std::vector<float> nsdf(size_t(maxLag+2));
    for(size_t start=0;start+size_t(frame)<=x12k.size();start+=size_t(hop)) {
        std::fill(buffer.begin(),buffer.end(),0.f);double energy=0;prefix[0]=0;
        for(int n=0;n<frame;++n){const float s=x12k[start+size_t(n)];buffer[size_t(n)]=s;energy+=double(s)*s;prefix[size_t(n+1)]=prefix[size_t(n)]+double(s)*s;}
        if(energy<=1e-12){out.push_back({});continue;}
        // Autocorrelation through |FFT|^2 (real-only transform packs re/im pairs).
        fft.performRealOnlyForwardTransform(buffer.data());
        for(int k=0;k<=fftSize/2;++k){const float re=buffer[size_t(2*k)],im=buffer[size_t(2*k+1)];buffer[size_t(2*k)]=re*re+im*im;buffer[size_t(2*k+1)]=0;}
        fft.performRealOnlyInverseTransform(buffer.data());
        // Scale so r(0) is the frame energy, whatever the FFT's inverse scaling.
        const double scale=std::abs(buffer[0])>1e-30f?energy/double(buffer[0]):0.;
        for(int lag=0;lag<=maxLag+1;++lag)buffer[size_t(lag)]=float(buffer[size_t(lag)]*scale);
        for(int lag=0;lag<=maxLag+1;++lag) {
            const double r=buffer[size_t(lag)];
            const double m=(prefix[size_t(frame-lag)]-prefix[0])+(prefix[size_t(frame)]-prefix[size_t(lag)]);
            nsdf[size_t(lag)]=m>1e-12?float(2*r/m):0.f;
        }
        // Key maxima between positive zero crossings; take the first within 90 % of the highest.
        std::vector<std::pair<int,float>> peaks;int lag=1;
        while(lag<=maxLag && nsdf[size_t(lag)]>0)++lag;          // leave the zero-lag lobe
        while(lag<=maxLag) {
            while(lag<=maxLag && nsdf[size_t(lag)]<=0)++lag;
            int best=-1;float value=0;
            while(lag<=maxLag && nsdf[size_t(lag)]>0){if(nsdf[size_t(lag)]>value){value=nsdf[size_t(lag)];best=lag;}++lag;}
            if(best>=minLag)peaks.push_back({best,value});
        }
        PitchFrame p;
        if(!peaks.empty()) {
            float highest=0;for(const auto& [l,v]:peaks)highest=std::max(highest,v);
            for(const auto& [l,v]:peaks)if(v>=.9f*highest) {
                const float a=nsdf[size_t(l-1)],b=nsdf[size_t(l)],c=nsdf[size_t(l+1)],d=a-2*b+c;
                const float shift=std::abs(d)>1e-9f?std::clamp(.5f*(a-c)/d,-.5f,.5f):0.f;
                p.hz=float(12000./(double(l)+shift));p.clarity=v;break;
            }
            if(p.hz>0) {
                // Hann-windowed single-bin DFT at the pitch: a chord's period has no energy there.
                double re=0,im=0,windowed=0;const double w=2*juce::MathConstants<double>::pi*p.hz/12000.;
                for(int n=0;n<frame;++n) {
                    const double hann=.5-.5*std::cos(2*juce::MathConstants<double>::pi*n/frame),s=x12k[start+size_t(n)]*hann;
                    re+=s*std::cos(w*n);im-=s*std::sin(w*n);windowed+=s*s;
                }
                // A sinusoid gives |X|^2 = windowed * N * 3/8 / (N*3/8)... normalised so a pure tone reads 1.
                p.fundamental=windowed>1e-12?float(std::clamp((re*re+im*im)/(windowed*double(frame)*.375),0.,1.)):0.f;
            }
        }
        out.push_back(p);
    }
    return out;
}
} // namespace instrument_detail

inline InstrumentResult classifyInstrument(const std::vector<float>& samples,double rate) {
    using namespace instrument_detail;
    InstrumentResult r;
    if(rate<8000 || samples.size()<size_t(rate)){r.reason="too little audio";return r;}
    auto& f=r.features;
    // ---- Envelope: 2 ms frames for attacks, 10 ms frames for the rest.
    const int fine=std::max(1,int(rate*.002)),coarse=5; // coarse = 5 fine frames
    std::vector<float> fineDb;
    for(size_t s=0;s+size_t(fine)<=samples.size();s+=size_t(fine)){double e=0;for(int n=0;n<fine;++n){const double v=samples[s+size_t(n)];e+=v*v;}fineDb.push_back(dbOf(e/fine));}
    std::vector<float> level;for(size_t i=0;i+coarse<=fineDb.size();i+=coarse){float m=-300;for(int k=0;k<coarse;++k)m=std::max(m,fineDb[i+size_t(k)]);level.push_back(m);}
    const float loud=quantile(level,.95f);
    f.playing=loud>-60.f && level.size()>=100;
    if(!f.playing){r.reason="no playing heard";return r;}
    const float floorDb=std::max(-70.f,loud-40.f);
    // Onsets: a rise of 9 dB over the lowest of the previous 60 ms, reaching within 24 dB of the loudest.
    std::vector<size_t> onsets;
    for(size_t i=6;i<level.size();++i) {
        float before=level[i-1];for(size_t k=2;k<=6;++k)before=std::min(before,level[i-k]);
        if(level[i]-before>=9.f && level[i]>=loud-24.f && (onsets.empty() || i-onsets.back()>=8))onsets.push_back(i);
    }
    std::vector<float> attacks,decays;int shortNotes=0;
    for(size_t n=0;n<onsets.size();++n) {
        const size_t start=onsets[n],end=n+1<onsets.size()?onsets[n+1]:level.size();
        if(end-start<8)continue;
        size_t peak=start;for(size_t i=start;i<std::min(end,start+40);++i)if(level[i]>level[peak])peak=i; // within 400 ms
        // Attack on the 2 ms frames: from 20 dB below the peak to 3 dB below it.
        const size_t fineStart=start>3?(start-3)*coarse:0,finePeakEnd=std::min(fineDb.size(),(peak+1)*coarse);
        float finePeak=-300;size_t finePeakAt=fineStart;for(size_t i=fineStart;i<finePeakEnd;++i)if(fineDb[i]>finePeak){finePeak=fineDb[i];finePeakAt=i;}
        size_t a=finePeakAt;while(a>fineStart && fineDb[a-1]>finePeak-20.f)--a;
        size_t b=a;while(b<finePeakAt && fineDb[b]<finePeak-3.f)++b;
        attacks.push_back(float(b-a)*2.f);
        // Decay: slope of level over 100-600 ms after the peak (while the note lasts).
        const size_t from=peak+10,to=std::min(end,peak+60);
        if(to>from+8) {
            double sx=0,sy=0,sxx=0,sxy=0;int count=0;
            for(size_t i=from;i<to;++i){if(level[i]<floorDb)break;const double x=double(i-peak)*.01,y=level[i];sx+=x;sy+=y;sxx+=x*x;sxy+=x*y;++count;}
            if(count>=8)decays.push_back(float((count*sxy-sx*sy)/std::max(1e-9,count*sxx-sx*sx)));
        }
        size_t gone=peak;while(gone<end && level[gone]>level[peak]-20.f)++gone;
        if(gone<end && gone-peak<=25)++shortNotes;
        ++f.notes;
    }
    f.attackMs=quantile(attacks,.5f);
    f.decayDbPerSecond=decays.empty()?0.f:quantile(decays,.5f);
    f.shortDecayFraction=f.notes>0?float(shortNotes)/float(f.notes):0.f;
    // ---- Pitch.
    const auto pitch=trackPitch(to12k(samples,rate));
    std::vector<float> voicedHz,fundamentals;int playingFrames=0;
    std::vector<int> voicedAt(pitch.size(),0);
    for(size_t i=0;i<pitch.size();++i) {
        const size_t centre=std::min(level.size()-1,i+4); // frame centre ~43 ms in
        if(level[centre]<floorDb)continue;
        ++playingFrames;
        if(pitch[i].clarity>=.85f && pitch[i].hz>=25.f && pitch[i].hz<=1500.f){voicedHz.push_back(pitch[i].hz);fundamentals.push_back(pitch[i].fundamental);voicedAt[i]=1;}
    }
    f.voicedFraction=playingFrames>0?float(voicedHz.size())/float(playingFrames):0.f;
    {   // A drum kit's snares and cymbals are unpitched; a palm-muted chug is not.
        int pitchedOnsets=0;
        for(const auto onset:onsets) {
            int voicedFrames=0;
            for(size_t k=onset;k<onset+10 && k<voicedAt.size();++k)voicedFrames+=voicedAt[k>4?k-4:0];
            if(voicedFrames>=4)++pitchedOnsets;
        }
        f.pitchedOnsetFraction=onsets.empty()?0.f:float(pitchedOnsets)/float(onsets.size());
    }
    f.f0Low=quantile(voicedHz,.1f);f.f0Median=quantile(voicedHz,.5f);f.f0High=quantile(voicedHz,.9f);
    if(!fundamentals.empty())f.fundamentalDb=dbOf(std::max(1e-12f,quantile(fundamentals,.5f)));
    // Pitch movement inside held segments (>= 300 ms voiced, no octave jumps):
    // residual around a 150 ms moving mean (vibrato), and net glide.
    std::vector<float> vibrato;int glides=0;
    const auto& moving=f.f0Median>=90.f?trackPitch(to12k(samples,rate),true):pitch;
    std::vector<int> steady(moving.size(),0);
    for(size_t i=0;i<moving.size();++i)steady[i]=moving[i].clarity>=.85f && moving[i].hz>=25.f && moving[i].hz<=1500.f
        && level[std::min(level.size()-1,i+2)]>=floorDb;
    for(size_t i=0;i<moving.size();) {
        if(!steady[i]){++i;continue;}
        size_t j=i+1;while(j<moving.size() && steady[j] && std::abs(1200*std::log2(moving[j].hz/moving[j-1].hz))<150)++j;
        if(j-i>=30) {
            std::vector<float> cents;for(size_t k=i;k<j;++k)cents.push_back(float(1200*std::log2(moving[k].hz/moving[i].hz)));
            double sum=0;for(size_t k=0;k<cents.size();++k) {
                double mean=0;int n=0;for(size_t m=k>=7?k-7:0;m<=std::min(cents.size()-1,k+7);++m){mean+=cents[m];++n;}
                mean/=n;sum+=(cents[k]-mean)*(cents[k]-mean);
            }
            vibrato.push_back(float(std::sqrt(sum/double(cents.size()))));
            const float head=quantile({cents.begin(),cents.begin()+5},.5f),tail=quantile({cents.end()-5,cents.end()},.5f);
            if(std::abs(tail-head)>=80.f)++glides;
            ++f.heldSegments;
        }
        i=j;
    }
    f.vibratoCents=quantile(vibrato,.5f);
    f.glideFraction=f.heldSegments>0?float(glides)/float(f.heldSegments):0.f;
    // ---- Spectrum: low/body (the previous guitar/bass cue) and high-band noise.
    {
        const int order=std::clamp(int(std::round(std::log2(rate/6.))),12,15),size=1<<order;
        juce::dsp::FFT fft(order);std::vector<float> buffer(static_cast<size_t>(2*size)),window(static_cast<size_t>(size));
        for(int n=0;n<size;++n)window[size_t(n)]=float(.5-.5*std::cos(2*juce::MathConstants<double>::pi*n/size));
        const auto bin=[&](double hz){return std::clamp(int(hz*size/rate),1,size/2);};
        double low=0,body=0,noise=0;int used=0;
        for(size_t start=0;start+size_t(size)<=samples.size();start+=size_t(size/2)) {
            double e=0;for(int n=0;n<size;++n){const float s=samples[start+size_t(n)];e+=double(s)*s;buffer[size_t(n)]=s*window[size_t(n)];}
            if(dbOf(e/size)<loud-15.f)continue;
            std::fill(buffer.begin()+size,buffer.end(),0.f);fft.performFrequencyOnlyForwardTransform(buffer.data(),true);
            for(int k=bin(35);k<bin(71);++k)low+=double(buffer[size_t(k)])*buffer[size_t(k)];
            for(int k=bin(71);k<bin(1400);++k)body+=double(buffer[size_t(k)])*buffer[size_t(k)];
            // Noise: the 3-8 kHz band's floor (its 20th-percentile bin), which harmonics do not fill.
            std::vector<float> band;for(int k=bin(3000);k<bin(std::min(8000.,rate*.45));++k)band.push_back(buffer[size_t(k)]*buffer[size_t(k)]);
            noise+=double(quantile(band,.2f))*double(band.size());
            ++used;
        }
        if(used>0){f.lowRatioDb=dbOf(low)-dbOf(body);f.noiseRatioDb=dbOf(noise)-dbOf(body);}
    }
    // ---- Guitar or bass (also the answer for the other classes when unsure).
    // Pitch settles it only at the ends: notes below 40 Hz are a bass (a Drop A
    // guitar stops at A1 = 55 Hz, an 8-string in drop E at 41 Hz); leads and
    // chords reaching 220 Hz median or 600 Hz are a guitar. A low line between
    // is either (bass A1 or Drop A guitar), so the earlier low-energy cue
    // decides, as before this detection existed. Choose Guitar or Bass in the
    // options for down-tuned guitars and bass.
    // A chord's period gives a low "virtual" pitch with no energy there: trust
    // the pitch range only when the median pitch is actually sounding.
    const bool pitched=voicedHz.size()>=30 && f.fundamentalDb>-40.f; // notes read -20..0 dB, chords -60 dB or less
    if(pitched && f.f0Low<40.f){r.bass=true;r.bassDecisive=true;}
    else if(pitched && (f.f0Median>=220.f || f.f0High>=600.f)){r.bass=false;r.bassDecisive=true;}
    else r.bass=f.lowRatioDb>-12.f;
    // ---- Class scores (each a product of the class's cues).
    const float sustained=ramp(f.decayDbPerSecond,-14.f,-7.f);       // holds after the peak (a plucked string falls 15+ dB/s)
    const float plucked=1.f-sustained;
    const float slowAttack=ramp(f.attackMs,18.f,40.f);
    // A voice wavers (vibrato, jitter) or glides; keys hold still. Slow attacks keep a wavering guitar out.
    const float vocalPitch=std::max(ramp(f.vibratoCents,4.f,9.f),ramp(f.glideFraction,.15f,.4f));
    const float steadyPitch=1.f-ramp(f.vibratoCents,3.f,6.f);
    const float voiced=ramp(f.voicedFraction,.35f,.6f);
    const float unvoiced=1.f-ramp(f.voicedFraction,.2f,.4f);
    const float vocalRange=pitched?ramp(f.f0Median,70.f,90.f)*(1.f-ramp(f.f0Median,900.f,1100.f)):0.f;
    const float enoughHeld=ramp(float(f.heldSegments),.5f,2.f);
    std::array<float,6> s{};
    s[size_t(Source::vocal)]=sustained*slowAttack*std::min(1.f,vocalPitch)*voiced*vocalRange*enoughHeld;
    // Clean keys leave the 3-8 kHz floor empty; a clipped (driven) guitar fills it.
    s[size_t(Source::keys)]=sustained*steadyPitch*voiced*enoughHeld*(1.f-ramp(f.noiseRatioDb,-45.f,-35.f));
    // Drums: most hits unpitched and gone within 250 ms (a palm-muted chug is pitched).
    const float percussive=(1.f-ramp(f.pitchedOnsetFraction,.3f,.6f))*ramp(f.shortDecayFraction,.5f,.75f);
    s[size_t(Source::percussion)]=percussive*ramp(float(f.notes),4.f,8.f);
    const float stringScore=std::max(plucked,1.f-std::max({s[size_t(Source::vocal)],s[size_t(Source::keys)],s[size_t(Source::percussion)]}))
                            *(1.f-unvoiced*.5f)*(1.f-.6f*percussive);
    s[size_t(r.bass?Source::bass:Source::guitar)]=stringScore;
    r.scores=s;
    // A non-guitar/bass class must clear 0.6 and beat the string score by 0.25.
    Source best=r.bass?Source::bass:Source::guitar;float bestScore=stringScore;
    for(auto c:{Source::vocal,Source::keys,Source::percussion})
        if(s[size_t(c)]>=.6f && s[size_t(c)]>=stringScore+.25f && s[size_t(c)]>bestScore){best=c;bestScore=s[size_t(c)];}
    r.source=best;
    float total=0;for(float v:s)total+=v;
    r.confidence=total>0?std::clamp(bestScore/total,0.f,1.f):0.f;
    juce::String why;
    switch(best) {
        case Source::vocal:why="sung: slow attacks, held notes with vibrato or glides";break;
        case Source::keys:why="clean held notes at a steady pitch";break;
        case Source::percussion:why="short unpitched hits";break;
        default:why=juce::String(r.bass?"bass":"guitar")+(r.bassDecisive?" by pitch range":" by low-end energy")+(plucked>=.5f?", plucked notes that decay":"");
    }
    r.reason=why;
    return r;
}

} // namespace spectralforge::autoset
