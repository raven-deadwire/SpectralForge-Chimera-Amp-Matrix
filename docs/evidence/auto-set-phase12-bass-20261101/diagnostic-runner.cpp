// Archived synthetic diagnostic harness; does not alter production or targets.
// Reuses the checked-in probe fixtures. Create /tmp/phase12-evidence before running.
#define main unusedFrozenProbeMain
#include "../../../Tests/AutoSetPowerBassProbe.cpp"
#undef main

std::vector<double> integratedFundamentalBands(const std::vector<float>& output,double rate,double f0) {
    const int order=int(std::ceil(std::log2(rate/.5))),size=1<<order;
    const size_t count=std::min(size_t(size),output.size()-size_t(std::llround(ampWarmupSeconds*rate)));
    const size_t start=output.size()-count;
    juce::dsp::FFT fft(order);std::vector<float> data(size_t(2*size));double windowEnergy=0;
    for(size_t n=0;n<count;++n) {
        const double w=.5-.5*std::cos(2*pi*double(n)/double(count));
        data[n]=float(output[start+n]*w);windowEnergy+=w*w;
    }
    fft.performFrequencyOnlyForwardTransform(data.data(),true);
    std::vector<double> result;
    for(double halfWidth:{.5,1.,2.,3.,5.,8.}) {
        double power=0;
        for(int k=std::max(1,int(std::ceil((f0-halfWidth)*size/rate)));k<std::min(size/2,int(std::ceil((f0+halfWidth)*size/rate)));++k)
            power+=double(data[size_t(k)])*data[size_t(k)];
        result.push_back(powerDb(2*power/(size*windowEnergy)));
    }
    return result;
}

int main() {
    std::ofstream out("/tmp/phase12-evidence/auto-detection.csv");out<<std::setprecision(10);
    out<<"model,head,channel,rate,f0,fixture,cab,playing,auto_bass,forced_bass\n";
    std::map<int,CabFilter> cabinets;
    size_t cases=0,bass=0;
    for(int model:{2,15,20,21,24,25}) {
        const auto state=defaultAmpNativeState(model);
        const std::array<int,2> controls{powerControl(model,state.channel,powerPresence),powerControl(model,state.channel,powerResonance)};
        const std::array<float,2> values{state.values[size_t(controls[0])],state.values[size_t(controls[1])]};
        for(double rate:{44100.,48000.,96000.}) {
            if(cabinets.find(int(rate))==cabinets.end())cabinets.emplace(int(rate),CabFilter(rate));
            for(double f0:pitches)for(int family:{0,1,2}) {
                const auto input=bassInput(rate,f0,family);
                const auto render=nativePowerRenderer(state,{},controls,input,rate);
                const auto head=render(values);
                for(bool cabinet:{false,true}) {
                    const auto post=cabinet?cabinets.at(int(rate)).apply(head,rate):head;
                    const auto automatic=measureSpectrum(post,rate,std::nullopt),forced=measureSpectrum(post,rate,true);
                    out<<model<<','<<quoted(ampInfo(model).name)<<','<<state.channel<<','<<rate<<','<<f0<<','<<familyName(family)<<','<<cabinet<<','<<automatic.playing<<','<<automatic.bass<<','<<forced.bass<<'\n';
                    ++cases;bass+=automatic.bass;
                }
            }
        }
    }
    out.close();std::cout<<"AUTO DETECTION: "<<cases<<" synthetic cases; "<<bass<<" classified bass\n";

    // Reproduce the reported Night Harvest A0 harmonic-gap cabinet proposal.
    // Re-render only its exact before/after settings, independent of analysis.
    std::ofstream detail("/tmp/phase12-evidence/night-harvest-f0-detail.csv");detail<<std::setprecision(10);
    detail<<"rate,state,presence,resonance,coherent_f0_peak_db,sub25_40_db,f0_pm0p5_rms_db,f0_pm1_rms_db,f0_pm2_rms_db,f0_pm3_rms_db,f0_pm5_rms_db,f0_pm8_rms_db\n";
    const auto state=defaultAmpNativeState(20);
    const std::array<int,2> controls{powerControl(20,state.channel,powerPresence),powerControl(20,state.channel,powerResonance)};
    for(double rate:{44100.,48000.,96000.}) {
        const auto input=bassInput(rate,27.5,2);
        const auto render=nativePowerRenderer(state,{},controls,input,rate);
        for(bool after:{false,true}) {
            const std::array<float,2> v=after?std::array<float,2>{.2f,.8f}:std::array<float,2>{.5f,.5f};
            const auto post=cabinets.at(int(rate)).apply(render(v),rate);
            const auto metric=measure(input,post,rate,27.5,Style::rock);
            const auto bands=integratedFundamentalBands(post,rate,27.5);
            detail<<rate<<','<<(after?"after":"before")<<','<<v[0]<<','<<v[1]<<','<<metric.f0Db<<','<<metric.subDb;
            for(double db:bands)detail<<','<<db;
            detail<<'\n';
        }
    }
}
