#include "PluginProcessor.h"
#include "GuitarSignaturePresets.h"
#include "FactoryNativeVoicing.h"
#include "FactoryCabVoicing.h"
#include "PresetOrder.h"
#include "ParamIndex.h"
#include <iostream>
#include <stdexcept>
#include <iomanip>
#include <limits>
#include <map>
#include <set>

namespace {
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
void set(ChimeraProcessor& p,const juce::String& id,float value) {
    auto* parameter=p.parameters().getParameter(id);require(parameter!=nullptr,"Unknown preset/test parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
struct PresetProcessingMeters {
    static double percentile(std::vector<double> values,double fraction) {
        if(values.empty())return -240;
        std::sort(values.begin(),values.end());
        return values[size_t(std::round(fraction*double(values.size()-1)))];
    }
    struct Meter {
        double sum{};float maximum{};
        void add(float value) {
            require(std::isfinite(value) && value>=0,"Factory dynamics reported invalid gain reduction");
            sum+=value;maximum=std::max(maximum,value);
        }
    };
    struct DetectorInput {
        spectralforge::EQCoefficients coefficients;
        std::array<double,2> z1{},z2{};
        double peak{},energy{},envelope{};size_t samples{};float frequency{},q{};
        std::vector<double> blockLevels;
        void add(const spectralforge::EQAnalyzerFrame& frame) {
            const std::array<double,2> input{frame.inputL,frame.inputR};
            double framePeak=0;
            for(size_t channel=0;channel<2;++channel) {
                const double y=coefficients.b0*input[channel]+z1[channel];
                z1[channel]=coefficients.b1*input[channel]-coefficients.a1*y+z2[channel];
                z2[channel]=coefficients.b2*input[channel]-coefficients.a2*y;
                require(std::isfinite(y),"Factory EQ detector input probe is nonfinite");
                peak=std::max(peak,std::abs(y));energy+=y*y;++samples;
                framePeak=std::max(framePeak,std::abs(y));
            }
            envelope=std::max(framePeak,envelope*std::exp(-1./(48000*.02)));
        }
    };
    Meter compressor,limiter;std::array<Meter,4> dynamic{};int blocks{};
    std::array<Meter,2> gateUnits{}; // Gate 2, Expander: reduction while the fixture plays
    std::array<DetectorInput,2> detectorInputs{};std::array<bool,2> wasAnalyzing{};
    std::array<uint64_t,2> droppedBefore{};
    std::array<double,2> compHPIn{},compHPOut{};
    double compHP{},compHPHz{},compEnergy{},compEnvelope{},compLevel{},compLevelPeak{};
    bool compRMS{};std::vector<double> compBlockLevels;
    void begin(ChimeraProcessor& processor) {
        compHPHz=processor.parameters().getRawParameterValue("cleanCompHP")->load();
        compHP=std::exp(-juce::MathConstants<double>::twoPi*compHPHz/48000);
        compRMS=processor.parameters().getRawParameterValue("cleanCompDetector")->load()>.5f;
        for(int bank=0;bank<2;++bank) {
            auto& eq=processor.graphicalEQ(bank);auto& probe=detectorInputs[size_t(bank)];
            const auto band=eq.values(bank==0?0:1);probe.frequency=band.frequency;probe.q=band.q;
            probe.coefficients=spectralforge::eqCoefficients(6,band.frequency,0,band.q,48000);
            wasAnalyzing[size_t(bank)]=eq.analyzer.enabled.exchange(true);
            eq.analyzer.discardPending();droppedBefore[size_t(bank)]=eq.analyzer.dropped.load();
        }
    }
    void observeCompInput(const spectralforge::EQAnalyzerFrame& frame) {
        const std::array<double,2> input{frame.outputL,frame.outputR};double detector=0;
        for(size_t channel=0;channel<2;++channel) {
            compHPOut[channel]=compHP*(compHPOut[channel]+input[channel]-compHPIn[channel]);
            compHPIn[channel]=input[channel];detector=std::max(detector,std::abs(compHPOut[channel]));
        }
        const double rms=std::exp(-1./(48000*.01));
        compEnergy=rms*compEnergy+(1-rms)*detector*detector;
        compEnvelope=std::max(detector,compEnvelope*std::exp(-1./(48000*.02)));
        compLevel=compRMS?std::sqrt(compEnergy):compEnvelope;compLevelPeak=std::max(compLevelPeak,compLevel);
    }
    void sample(ChimeraProcessor& processor) {
        ++blocks;compressor.add(processor.commonDynamics().compReduction.load());
        for(int unit=1;unit<=2;++unit)gateUnits[size_t(unit-1)].add(-juce::Decibels::gainToDecibels(processor.gateUnitMeter(unit),-120.f));
        limiter.add(processor.commonDynamics().limitReduction.load());
        for(int slot=0;slot<4;++slot) {
            const int target=int(processor.parameters().getRawParameterValue(spectralforge::dynamicSlotID(slot,"target"))->load());
            dynamic[size_t(slot)].add(target>0?processor.graphicalEQ((target-1)/12).dynamicReduction((target-1)%12):0.f);
        }
        for(int bank=0;bank<2;++bank) {
            spectralforge::EQAnalyzerFrame frame;auto& fifo=processor.graphicalEQ(bank).analyzer;
            while(fifo.pop(frame)) {
                detectorInputs[size_t(bank)].add(frame);
                if(bank==0)observeCompInput(frame);
            }
            detectorInputs[size_t(bank)].blockLevels.push_back(20*std::log10(std::max(1.e-12,detectorInputs[size_t(bank)].envelope)));
        }
        compBlockLevels.push_back(20*std::log10(std::max(1.e-12,compLevel)));
    }
    void end(ChimeraProcessor& processor) {
        for(int bank=0;bank<2;++bank) {
            auto& fifo=processor.graphicalEQ(bank).analyzer;
            fifo.enabled.store(wasAnalyzing[size_t(bank)]);
            require(fifo.dropped.load()==droppedBefore[size_t(bank)],"Factory EQ detector input probe dropped audio frames");
        }
    }
    void print(int index,const char* condition) const {
        // Means are averages of the 128-sample block meter observations, not
        // sample-accurate detector or loudness measurements.
        std::cout<<"PRESET_DYNAMICS,"<<index<<','<<condition;
        const auto meter=[&](const Meter& value){std::cout<<','<<value.maximum<<','<<value.sum/std::max(1,blocks);};
        meter(compressor);meter(limiter);for(const auto& value:dynamic)meter(value);for(const auto& value:gateUnits)meter(value);std::cout<<'\n';
        // Independent band-pass measurement of the actual Tone/Final inputs.
        // This gives threshold calibration context without changing the DSP's
        // dynamic range or observing a limiter-flattened output spectrum.
        for(int bank=0;bank<2;++bank) {
            const auto& probe=detectorInputs[size_t(bank)];
            std::cout<<"PRESET_EQ_DETECTOR,"<<index<<','<<condition<<','<<(bank==0?"Tone":"Final")
                <<','<<probe.frequency<<','<<probe.q<<','<<20*std::log10(std::max(1.e-12,probe.peak))
                <<','<<10*std::log10(std::max(1.e-24,probe.energy/std::max(size_t(1),probe.samples)))
                <<','<<percentile(probe.blockLevels,.5)<<','<<percentile(probe.blockLevels,.9)<<','<<percentile(probe.blockLevels,.99)<<'\n';
        }
        std::cout<<"PRESET_COMP_DETECTOR,"<<index<<','<<condition<<','<<compHPHz<<','<<(compRMS?"RMS":"Peak")
            <<','<<20*std::log10(std::max(1.e-12,compLevelPeak))<<','<<percentile(compBlockLevels,.5)
            <<','<<percentile(compBlockLevels,.9)<<','<<percentile(compBlockLevels,.99)<<'\n';
    }
};
void factoryCabContract(ChimeraProcessor&,int index,bool rendered);
std::vector<float> render(ChimeraProcessor& p,bool bass=false,int blocks=120,int factoryIndex=-1,PresetProcessingMeters* meters=nullptr) {
    p.prepareToPlay(48000,128);juce::AudioBuffer<float> b(2,128);juce::MidiBuffer midi;std::vector<float> result;
    if(meters)meters->begin(p);
    for(int block=0;block<blocks;++block) {
        for(int n=0;n<128;++n) {
            const double t=double(block*128+n)/48000;
            // Six plucks spanning low B/E through upper strings. A harmonic-rich
            // deterministic fixture checks gain staging, not real-DI acceptance.
            constexpr double bassNotes[]{30.8677,41.2034,55.,73.4162,30.8677,98.};
            constexpr double guitarNotes[]{82.4069,110.,146.8324,196.,82.4069,246.9417};
            const double noteTime=std::fmod(t,.2),fundamental=(bass?bassNotes:guitarNotes)[int(t/.2)%6];
            const float envelope=float(std::min(1.,noteTime/.003)*(.08+.92*std::exp(-noteTime*12)));
            float x=0;for(int harmonic=1;harmonic<=8;++harmonic)
                x+=envelope*.16f/float(harmonic)*std::sin(float(juce::MathConstants<double>::twoPi*fundamental*harmonic*noteTime));
            b.setSample(0,n,x);b.setSample(1,n,-.7f*x);
        }
        p.processBlock(b,midi);
        if(meters)meters->sample(p);
        for(int c=0;c<2;++c)for(int n=0;n<128;++n) {const auto x=b.getSample(c,n);require(std::isfinite(x) && std::abs(x)<4,"Signature output invalid/unbounded");result.push_back(x);}
    }
    if(meters)meters->end(p);
    if(factoryIndex>=0)factoryCabContract(p,factoryIndex,true);
    p.releaseResources();return result;
}
bool factoryProcessingParameter(const juce::String& id) {
    return id.startsWith("toneEQ_") || id.startsWith("finalEQ_") || id.startsWith("dynamicEQ")
        || id.startsWith("cleanComp") || id.startsWith("liveLimiter") || id.startsWith("gate");
}
void factoryProcessingState(ChimeraProcessor& source,ChimeraProcessor& copy) {
    std::map<juce::String,float> expected;
    for(auto* raw:source.getParameters())if(auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(raw))
        if(factoryProcessingParameter(parameter->paramID))expected[parameter->paramID]=parameter->getValue();
    const std::array<uint16_t,2> masks{source.eqEditing().occupiedMask(0),source.eqEditing().occupiedMask(1)};
    const auto dirty=[&] {
        require(copy.eqEditing().addBand(0,4700,3)>=0,"Factory EQ dirty-state fixture did not add an editor band");
        for(const auto& [id,value]:expected) {
            auto* parameter=copy.parameters().getParameter(id);
            // Use legal bool/choice/stepped values. Writing .81 directly into
            // a bool leaves its host value noncanonical while APVTS caches 1,
            // so restoring the same true raw value need not rewrite that .81.
            const auto legal=[&](float normal){return parameter->convertTo0to1(parameter->convertFrom0to1(normal));};
            float changed=legal(.81f);if(std::abs(changed-value)<1.e-6f)changed=legal(.19f);
            require(std::abs(changed-value)>=1.e-6f,"Factory processing dirty fixture retained an authored parameter");
            parameter->setValueNotifyingHost(changed);
        }
    };
    const auto restored=[&](const char* phase) {
        for(const auto& [id,value]:expected) {
            const auto actual=copy.parameters().getParameter(id)->getValue();
            if(std::abs(actual-value)>=1.e-6f)
                std::cerr<<"FACTORY_PROCESSING_RESTORE phase="<<phase<<" id="<<id<<" actual="<<actual<<" expected="<<value<<'\n';
            require(std::abs(copy.parameters().getParameter(id)->getValue()-value)<1.e-6f,
                    "Factory EQ/dynamics project or A/B restore changed an authored parameter");
        }
        for(int bank=0;bank<2;++bank)
            require(copy.eqEditing().occupiedMask(bank)==masks[size_t(bank)],"Factory EQ project or A/B restore changed band occupancy");
    };
    juce::MemoryBlock bytes;source.getStateInformation(bytes);dirty();
    copy.setStateInformation(bytes.getData(),int(bytes.getSize()));restored("project");
    copy.copyComparison();copy.selectComparison(1);dirty();copy.selectComparison(0);restored("A/B");
}
// Gate 2 / Expander must stay open while the pluck fixture plays and close on
// the quiet hiss that follows it.
void factoryGateTail(ChimeraProcessor& p,int index,bool bass) {
    p.prepareToPlay(48000,128);juce::AudioBuffer<float> b(2,128);juce::MidiBuffer midi;
    const bool expander=p.parameters().getRawParameterValue("expOn")->load()>.5f;const int unit=expander?2:1;
    float openReduction=0;
    // Hiss 24 dB under the unit's threshold at the detector, after input gain.
    const auto raw=[&](const char* id){return p.parameters().getRawParameterValue(id)->load();};
    const float hiss=juce::Decibels::decibelsToGain((expander?raw("expThreshold"):raw("gate2Threshold"))-24.f-raw("input"));
    for(int block=0;block<1050;++block) {
        for(int n=0;n<128;++n) {
            const double t=double(block*128+n)/48000;float x=0;
            if(t<1.2) {
                const double noteTime=std::fmod(t,.2),fundamental=bass?41.2034:110.;
                const float envelope=float(std::min(1.,noteTime/.003)*(.08+.92*std::exp(-noteTime*12)));
                for(int harmonic=1;harmonic<=8;++harmonic)x+=envelope*.16f/float(harmonic)*std::sin(float(juce::MathConstants<double>::twoPi*fundamental*harmonic*noteTime));
            } else x=hiss*std::sin(float(t*12345.678));
            b.setSample(0,n,x);b.setSample(1,n,-.7f*x);
        }
        p.processBlock(b,midi);
        const float reduction=-juce::Decibels::gainToDecibels(p.gateUnitMeter(unit),-120.f);
        if(block>20 && block<440)openReduction=std::max(openReduction,reduction);
        if(block==1049) { // 1.6 s of hiss: long authored releases have settled
            std::cout<<"PRESET_GATE_UNITS,"<<index<<','<<(expander?"expander":"gate2")<<','<<openReduction<<','<<reduction<<'\n';
            require(openReduction<.5f,"Factory Gate 2 / Expander cuts into the played notes");
            require(reduction>6.f,"Factory Gate 2 / Expander does not reduce the quiet hiss between phrases");
        }
    }
    p.releaseResources();
}
void factoryProcessingContract(ChimeraProcessor& processor,int index) {
    using namespace spectralforge;
    const auto get=[&](const juce::String& id) {
        const auto* parameter=processor.parameters().getRawParameterValue(id);
        require(parameter!=nullptr,"Factory EQ/dynamics parameter missing");return parameter->load();
    };
    DynamicEQBindings dynamic;dynamic.bind(processor.parameters());
    std::set<int> assigned;
    for(int slot=0;slot<4;++slot) {
        const int target=int(get(dynamicSlotID(slot,"target")));
        if(target==0)continue;
        require((target==1 || target==14) && assigned.insert(target).second,"Factory dynamic EQ target is stale, duplicated or outside its musical role");
        require(slot<2 && get(dynamicSlotID(slot,"range"))>0 && get(dynamicSlotID(slot,"ratio"))>1,
                "Factory dynamic EQ assigned an inactive or surplus processing slot");
        const int bank=(target-1)/12,band=(target-1)%12;
        const auto value=processor.graphicalEQ(bank).values(band);
        require(!processor.graphicalEQ(bank).isBypassed() && value.enabled && value.type<=2,
                "Factory dynamic EQ targets a bypassed, absent or unsupported band");
    }
    require(assigned.size()==2,"Factory preset did not assign both body and harshness dynamic EQ nodes");
    for(int bank=0;bank<2;++bank) {
        uint16_t mask=0;
        for(int band=0;band<graphicalEQBands;++band) {
            const auto value=processor.graphicalEQ(bank).values(band);
            if(!value.enabled)continue;
            mask=uint16_t(mask|(1u<<band));
            // Bands 1-2 hold the finishing/dynamic nodes; Tone 3-4 and Final 3 the role tone curve.
            require(band<(bank==0?4:3),"Factory EQ left an unauthored correction slot active");
        }
        require(mask!=0 && !processor.graphicalEQ(bank).isBypassed(),"Factory preset omitted its Tone or Final EQ voicing");
        require(processor.eqEditing().occupiedMask(bank)==mask,"Factory EQ displayed stale or missing editor bands");
    }
    // These presets already author a POST compressor. A second full-band
    // compressor would change the established sustain and transient envelope.
    constexpr std::array<int,11> postCompressed{1,8,9,11,20,31,32,33,34,38,42};
    if(std::find(postCompressed.begin(),postCompressed.end(),index)!=postCompressed.end())
        require(get("cleanCompOn")==0,"Factory preset stacked a new common compressor over its authored POST compressor");
    require(std::abs(get("cleanCompMakeup"))<.002f,"Factory common compressor introduced an implicit output-level correction");
    require(get("liveLimiterOn")==1 && std::abs(get("liveLimiterCeiling")+1)<.002f,"Factory output protection is absent or uses the wrong ceiling");
    // Hard main gate plus exactly one of: a gentle Gate 2 on the other side, or the Expander.
    require(get("gateMode")<.5f && (get("gate2On")>.5f)!=(get("expOn")>.5f),"Factory gating must pair the main gate with Gate 2 or the Expander");
    if(get("expOn")>.5f)require(get("expThreshold")>get("gatethreshold") && get("expRangeDb")<=get("gateRangeDb")+.01f,"Factory Expander must work above the main gate");
    if(get("gate2On")>.5f)require(get("gate2Mode")>.5f && get("gate2Threshold")<get("gatethreshold"),"Factory Gate 2 must be a gentle expander below the main gate");
    const bool bass=isNiflheimrPreset(index)||(index<factoryPresetCount && juce::String(factoryPresets[size_t(index)].instrument).contains("Bass"));
    if(bass)for(double fundamental:{27.5,30.8677})for(bool maximumDynamic:{false,true}) {
        double response=1;
        for(int bank=0;bank<2;++bank)for(int band=0;band<graphicalEQBands;++band) {
            const auto value=processor.graphicalEQ(bank).values(band);if(!value.enabled)continue;
            const auto config=dynamic.forBand(bank*12+band+1);
            const float gain=value.gain-(maximumDynamic && config.enabled?config.range:0.f);
            response*=eqCoefficients(value.type,value.frequency,gain,value.q,48000).magnitude(fundamental,48000);
        }
        const double db=20*std::log10(response);
        require(std::isfinite(db) && db>-1,"Factory finishing EQ removes more than 1 dB of the bass low-A/low-B fundamental");
        std::cout<<"PRESET_LOW_FUNDAMENTAL_EQ,"<<index<<','<<fundamental<<','<<(maximumDynamic?"maximum_dynamic_cut":"static")<<','<<db<<'\n';
    }
}
// General factory recalls preserve the actual host/APVTS performance values.
// A decimal request is not necessarily exactly representable after JUCE's
// interval snap (e.g. input=3 may be 2.99999928 with fused multiply-add).
void performanceRecall(ChimeraProcessor& p,int index) {
    using namespace spectralforge;
    const std::map<juce::String,float> requested{{"input",3},{"inputmode",1},{"tempo",143},
        {"temposync",1},{"metronome",1},{"tuneron",1},{"tunermute",0},{"tunerref",442}};
    struct Value {float raw,normalised;};
    std::map<juce::String,Value> before;
    for(const auto& [id,value]:requested) {
        set(p,id,value);auto* parameter=p.parameters().getParameter(id);
        const float actual=p.parameters().getRawParameterValue(id)->load();
        const auto& range=parameter->getNormalisableRange();
        const float tolerance=2*std::numeric_limits<float>::epsilon()*std::max(1.f,range.end-range.start);
        require(std::abs(actual-value)<=tolerance,"Performance fixture did not reach the requested setting");
        before[id]={actual,parameter->getValue()};
    }
    const auto snapshot=factoryNativeSnapshot(p.parameters(),index);
    for(int repeat=0;repeat<2;++repeat) {
        p.loadFactoryPreset(index);
        for(const auto& [id,value]:before) {
            const float afterRaw=p.parameters().getRawParameterValue(id)->load();
            const float afterNormalised=p.parameters().getParameter(id)->getValue();
            if(id=="input" || id=="tempo") {
                const auto precision=std::cout.precision();
                std::cout<<std::setprecision(std::numeric_limits<float>::max_digits10)
                    <<"PERFORMANCE_RECALL preset="<<index<<" repeat="<<repeat<<" id="<<id
                    <<" requested="<<requested.at(id)<<" before_raw="<<value.raw
                    <<" before_normalized="<<value.normalised
                    <<" snapshot_raw="<<float(snapshot.getChildWithProperty("id",id)["value"])
                    <<" after_raw="<<afterRaw<<" after_normalized="<<afterNormalised<<'\n';
                std::cout.precision(precision);
            }
            // No tolerance here: compare the actual values before recall, not
            // integer literals. Even a one-ULP recall drift must be diagnosed.
            require(afterRaw==value.raw && afterNormalised==value.normalised,
                    "Factory recall changed an actual raw/normalized performance value");
        }
    }
}
void factoryCabContract(ChimeraProcessor& processor,int index,bool rendered=false) {
    using namespace spectralforge;
    const auto value=[&](const juce::String& id) {
        const auto* parameter=processor.parameters().getRawParameterValue(id);
        require(parameter!=nullptr,"Factory CAB contract parameter missing");return parameter->load();
    };
    const int mode=int(value("mode")),lanes=mode==0?1:mode==1?2:3;
    const bool dryBass=index==2||index==25||index==26||index==27||index==45;
    const bool dryGuitar=index==36||index==37;
    if(dryBass||dryGuitar)require(mode==2 && value("lowampmix")==0.f,"Factory CAB voicing changed the exact Matrix LOW dry path");
    OriginalCabParameters modeled;modeled.bind(processor.parameters());
    // The key warmed for the inactive A/B slot, from a saved state, matches
    // the key the audio thread requests for the same values.
    const ParamIndex saved(processor.parameters().copyState());
    const auto savedValue=[&](const juce::String& id){return float(saved[id].getProperty("value"));};
    for(int lane=0;lane<3;++lane)for(int slot=0;slot<2;++slot)
        require(OriginalCabParameters::savedKey(lane,slot,savedValue)==modeled.read(lane,slot),"Saved-state CAB model key differs from the live request");
    for(int lane=0;lane<3;++lane) {
        const auto suffix=juce::String(lane+1);
        const bool active=lane<lanes && !(lane==0 && (dryBass||dryGuitar));
        require(value("cab"+suffix)==(active?1.f:0.f),"Factory CAB active/bypass policy differs from its routing role");
        require(value("cabtype"+suffix)==0.f && value("cabBtype"+suffix)==0.f && value("cabblend"+suffix)==0.f,
                "Factory recall selected a captured IR or an unauthored second mic blend");
        const int layout=int(value(cabLayoutID(lane,"layout"))),driver=int(value(cabExpansionID(lane,"driver")));
        require(layout>=1 && layout<=int(cabLayout::layouts.size()) && driver>=1 && driver<=int(cabExpansion::drivers.size()),
                "Factory CAB omitted its explicit layout or speaker design");
        const auto& enclosure=cabLayout::layouts[size_t(layout-1)];const auto& speaker=cabExpansion::drivers[size_t(driver-1)];
        require(enclosure.bass==speaker.bass && enclosure.inches==speaker.inches,
                "Factory speaker recipe needs an implicit family/diameter fallback");
        require(int(value(originalCabID(lane,"design")))==(enclosure.bass?1:0),"Factory CAB family control disagrees with its physical layout");
        for(int slot=0;slot<2;++slot) {
            const auto letter=juce::String(slot?"B":"A");const bool enabled=active && slot==0;
            require(value(originalCabID(lane,(letter+"on").toRawUTF8()))==(enabled?1.f:0.f),"Factory modeled mic enable policy is incomplete");
            const int unit=int(value(cabLayoutID(lane,(letter+"unit").toRawUTF8())));
            const int mic=int(value(cabExpansionID(lane,(letter+"mic").toRawUTF8())));
            require(unit>=0 && unit<enclosure.columns*enclosure.rows && mic>=1 && mic<=int(cabExpansion::microphones.size()),
                    "Factory CAB selected a removed speaker unit or unspecified microphone");
            require(processor.micName(lane,slot).isEmpty(),"Clean factory recall automatically loaded an external/user IR");
            const auto key=modeled.read(lane,slot);
            require((key!=0)==enabled,"Factory CAB controls do not select the modeled DSP path");
            if(rendered)require(processor.micDisplayRevision(lane,slot)[2]==(uint64_t{1}|(key<<4)),
                                "Production audio did not install the factory modeled cabinet request");
        }
    }
    // Independent representative recipes cover every snapshot builder. The
    // full-bank checks above validate compatible, explicit routing for all 48.
    const auto recipe=[&](int lane,int layout,int driver,int mic) {
        require(int(value(cabLayoutID(lane,"layout")))==layout && int(value(cabExpansionID(lane,"driver")))==driver
                && int(value(cabExpansionID(lane,"Amic")))==mic,"Factory CAB starting tone changed without updating its recipe contract");
    };
    if(index==0)recipe(0,1,5,14);       // clean guitar, open 1x12
    if(index==1)recipe(0,3,1,1);        // tight guitar, 4x12
    if(index==2)recipe(1,7,10,3);       // Matrix dry LOW + modeled 6x10 MID
    if(index==34)recipe(0,2,1,11);      // guitar signature builder
    if(index==38)recipe(0,3,8,20);      // Original Nastrond builder
    if(index==43)recipe(0,6,10,5);      // Original Niflheimr builder
}
void factoryBank(bool measureOnly,bool originalOnly=false,bool headerVoicing=false,const std::set<int>& selected={}) {
    using namespace spectralforge;
    std::array<bool,selectablePresetCount> seen{};
    int measured=0,levelFailures=0;
    std::cout<<"PRESET_DYNAMICS_HEADER,index,condition,comp_max_db,comp_block_mean_db,limiter_max_db,limiter_block_mean_db,dynamic1_max_db,dynamic1_block_mean_db,dynamic2_max_db,dynamic2_block_mean_db,dynamic3_max_db,dynamic3_block_mean_db,dynamic4_max_db,dynamic4_block_mean_db\n";
    std::cout<<"PRESET_EQ_DETECTOR_HEADER,index,condition,bank,frequency_hz,q,bandpass_peak_dbfs,bandpass_stereo_rms_dbfs,envelope_p50_dbfs,envelope_p90_dbfs,envelope_p99_dbfs\n";
    std::cout<<"PRESET_COMP_DETECTOR_HEADER,index,condition,detector_hp_hz,detector_mode,level_peak_dbfs,block_level_p50_dbfs,block_level_p90_dbfs,block_level_p99_dbfs\n";
    for(const int index:presetDisplayOrder()) {
        require(index>=0&&index<selectablePresetCount&&!seen[size_t(index)],"Preset display order is not a permutation");seen[size_t(index)]=true;
        require(adjacentPreset(adjacentPreset(index,1),-1)==index,"Preset navigation disagrees with category order");
        if((originalOnly&&!isOriginalPreset(index)) || (!selected.empty() && selected.count(index)==0))continue;
        ++measured;
        const auto a=std::make_unique<ChimeraProcessor>(),b=std::make_unique<ChimeraProcessor>();
        for(auto* raw:a->getParameters())if(auto* p=dynamic_cast<juce::RangedAudioParameter*>(raw))
            if(!factoryPerformanceParameter(p->paramID))p->setValueNotifyingHost(.81f);
        std::map<juce::String,float> previous;
        for(auto* raw:a->getParameters())if(auto* p=dynamic_cast<juce::RangedAudioParameter*>(raw))
            previous[p->paramID]=p->convertTo0to1(a->parameters().getRawParameterValue(p->paramID)->load());
        a->loadFactoryPreset(index);b->loadFactoryPreset(index);
        // Diagnostic mode can render the current header-defined voicing while
        // iterating with a cached DSP library. The normal regression always
        // exercises the actual production loadFactoryPreset entry point.
        if(headerVoicing)for(auto* processor:{a.get(),b.get()})
            processor->parameters().replaceState(isNiflheimrPreset(index)?niflheimrPresetSnapshot(processor->parameters(),index-niflheimrPresetStart):isOriginalPreset(index)?originalPresetSnapshot(processor->parameters(),index-originalPresetStart):isGuitarSignature(index)
                ?guitarSignatureSnapshot(processor->parameters(),index-factoryPresetCount)
                :factoryNativeSnapshot(processor->parameters(),index));
        std::set<juce::String> inactive;
        if(index<factoryPresetCount)for(int owner=0;owner<pedalBoardCapacity;++owner) {
            const int selected=a->pedalBoardState().instances[size_t(owner)].model;
            for(int model=0;model<pedalModelCount;++model)if(model!=selected) {
                inactive.insert(pedalBypassID(owner,model));
                for(int c=0;c<pedalModel(model).controlCount;++c)inactive.insert(pedalControlID(owner,model,c));
            }
        }
        for(int i=0;i<a->getParameters().size();++i) {
            auto* p=dynamic_cast<juce::RangedAudioParameter*>(a->getParameters()[i]);
            require(p!=nullptr,"Factory test expected a ranged parameter");
            const bool preserved=inactive.count(p->paramID)!=0;
            const float expected=preserved?previous.at(p->paramID):b->getParameters()[i]->getValue();
            if(std::abs(p->getValue()-expected)>=1e-6f) {
                std::cerr<<"FACTORY_STATE preset="<<index<<" id="<<p->paramID<<" inactive="<<preserved
                    <<" actual="<<p->getValue()<<" expected="<<expected<<'\n';
                require(false,preserved?"Factory recall erased an inactive PRE bank":"Factory recall inherited a prior sound bank");
            }
        }
        if(index<31) {
            const int mode=int(a->parameters().getRawParameterValue("mode")->load());
            for(int lane=0;lane<(mode==0?1:mode==1?2:3);++lane)require(a->parameters().getRawParameterValue(ampNativeEnabledID(ampNativeContext(mode,lane)))->load()>.5f,"Factory amp still uses a different engine from its panel");
            for(int section=0;section<3;++section)require(a->parameters().getRawParameterValue(postNativeModeID(section))->load()>.5f,"Factory POST still uses a different engine from its panel");
            require(a->parameters().getRawParameterValue("boardEnabled")->load()>.5f,"Factory PRE still uses a different engine from its panel");
        }
        require(std::abs(a->parameters().getRawParameterValue("output")->load())<1e-5f,"Factory recall must start at OUTPUT 0 dB");
        factoryCabContract(*a,index);factoryCabContract(*b,index);
        if(!measureOnly) {
            factoryProcessingContract(*a,index);factoryProcessingContract(*b,index);
        }
        const bool bass=isNiflheimrPreset(index)||(index<factoryPresetCount&&juce::String(factoryPresets[size_t(index)].instrument).contains("Bass"));
        PresetProcessingMeters nominalMeters,hotMeters;
        const auto audio=render(*a,bass,450,index,&nominalMeters),clean=render(*b,bass,450,index);double energy=0;float peak=0;
        require(audio.size()==clean.size(),"Factory audio fixture size differs");
        for(size_t i=0;i<audio.size();++i)require(std::abs(audio[i]-clean[i])<1e-6f,"Inactive PRE bank changed factory audio");
        // Keep the independent dirty/clean PRE-bank render above intact. A
        // full-state roundtrip may only replace the clean processor afterward.
        if(!measureOnly)factoryProcessingState(*a,*b);
        for(float x:audio){energy+=double(x)*x;peak=std::max(peak,std::abs(x));}
        const double rmsDb=10*std::log10(energy/audio.size()),peakDb=20*std::log10(peak);
        const auto* name=selectablePresetName(index);
        std::cout<<"PRESET_LEVEL,"<<index<<","<<name<<","<<rmsDb<<","<<peakDb<<","<<a->parameters().getRawParameterValue("output")->load()<<'\n';
        nominalMeters.print(index,"nominal");
        if(!measureOnly)factoryGateTail(*a,index,bass);
        if(measureOnly&&(index==1||index==14||index==31))std::cout<<"PRESET_DIAGNOSTIC "<<index<<" "<<a->diagnosticReport()<<'\n';
        if(!measureOnly){require(peak<.95f,"Factory preset clips at unity OUTPUT");require(rmsDb>-30,"Factory preset is unexpectedly quiet on the synthetic pluck fixture");}
        set(*b,"input",6);const auto hot=render(*b,bass,450,index,&hotMeters);float hotPeak=0;
        for(float x:hot)hotPeak=std::max(hotPeak,std::abs(x));
        std::cout<<"PRESET_HOT,"<<index<<","<<20*std::log10(hotPeak)<<'\n';
        hotMeters.print(index,"input_plus_6_db");
        // The final protection limiter must not conceal a newly clipped preset.
        // Repeat both existing input fixtures with only that stage bypassed;
        // the unchanged unity/+6 dB peak limits still qualify the authored rig.
        double unprotectedRmsDb=rmsDb;float unprotectedPeak=peak,unprotectedHotPeak=hotPeak;
        if(b->parameters().getRawParameterValue("liveLimiterOn")->load()>.5f) {
            const float savedInput=b->parameters().getRawParameterValue("input")->load();
            set(*b,"liveLimiterOn",0);set(*b,"input",0);
            const auto raw=render(*b,bass,450,index);double rawEnergy=0;unprotectedPeak=0;
            for(float value:raw){rawEnergy+=double(value)*value;unprotectedPeak=std::max(unprotectedPeak,std::abs(value));}
            unprotectedRmsDb=10*std::log10(rawEnergy/raw.size());
            set(*b,"input",6);const auto rawHot=render(*b,bass,450,index);unprotectedHotPeak=0;
            for(float value:rawHot)unprotectedHotPeak=std::max(unprotectedHotPeak,std::abs(value));
            set(*b,"liveLimiterOn",1);set(*b,"input",savedInput);
        }
        std::cout<<"PRESET_UNPROTECTED,"<<index<<','<<unprotectedRmsDb<<','<<20*std::log10(unprotectedPeak)<<','<<20*std::log10(unprotectedHotPeak)<<'\n';
        const bool levelPass=rmsDb>-30 && peak<.95f && hotPeak<.95f
            && unprotectedRmsDb>-30 && unprotectedPeak<.95f && unprotectedHotPeak<.95f;
        if(!levelPass)++levelFailures;
        if(measureOnly)std::cout<<"PRESET_GATE,"<<index<<","<<(rmsDb>-30)<<","<<(peak<.95f)<<","<<(hotPeak<.95f)<<'\n';
        if(!measureOnly)require(hotPeak<.95f,"Factory preset clips the +6 dB input pluck fixture");
        if(!measureOnly)require(unprotectedRmsDb>-30 && unprotectedPeak<.95f && unprotectedHotPeak<.95f,
                               "Factory limiter concealed a clipped or unusably quiet nominal/+6 dB preset");
        if(index<factoryPresetCount)performanceRecall(*a,index);
    }
    require(std::all_of(seen.begin(),seen.end(),[](bool value){return value;}),"Preset display order omitted a factory ID");
    std::cout<<"PASS factory recall, modeled CAB and category navigation: "<<measured<<" presets (synthetic fixture only)\n";
    if(measureOnly)std::cout<<"PRESET_MEASUREMENT_SUMMARY,"<<measured<<","<<levelFailures<<","<<(headerVoicing?"header-voicing":"production-recall")
                          <<"; diagnostic only, strict RMS > -30 dBFS and unity/+6 dB peaks < 0.95 remain unchanged\n";
}
void signatures(const juce::File& directory) {
    require(directory.createDirectory().wasOk(),"Cannot create signature evidence directory");
    for(int song=0;song<4;++song) {
        const auto a=std::make_unique<ChimeraProcessor>(),b=std::make_unique<ChimeraProcessor>();
        // Dirty every parameter bank before recall, including unrelated models.
        for(auto* raw:a->getParameters())raw->setValueNotifyingHost(.81f);
        a->loadFactoryPreset(spectralforge::factoryPresetCount+song);
        b->loadFactoryPreset(spectralforge::factoryPresetCount+song);
        for(int i=0;i<a->getParameters().size();++i) {
            const auto av=a->getParameters()[i]->getValue(),bv=b->getParameters()[i]->getValue();
            if(std::abs(av-bv)>1e-6f) {std::cerr<<"DIRTY "<<a->getParameters()[i]->getName(100)<<" a="<<av<<" b="<<bv<<'\n';require(false,"Signature inherited a dirty parameter bank");}
        }
        const auto snapshot=spectralforge::guitarSignatureSnapshot(a->parameters(),song);
        int params=0;for(auto child:snapshot)if(child.hasProperty("id") && child.hasProperty("value"))++params;
        require(params==a->getParameters().size(),"Full signature snapshot omitted APVTS parameters");
        require(snapshot.getChildWithName("GUITAR_SIGNATURE").getProperty("id").toString()==spectralforge::guitarSignatures[size_t(song)].id,"Signature identity missing");
        auto xml=snapshot.createXml();require(xml->writeTo(directory.getChildFile(juce::String(song)+".xml")),"Snapshot export failed");
        juce::MemoryBlock state;a->getStateInformation(state);b->setStateInformation(state.getData(),int(state.getSize()));
        for(int i=0;i<a->getParameters().size();++i)require(std::abs(a->getParameters()[i]->getValue()-b->getParameters()[i]->getValue())<1e-6f,"Signature binary project restore differs");
        const auto first=render(*a),restored=render(*b);double energy=0;float delta=0;
        for(size_t i=0;i<first.size();++i) {delta=std::max(delta,std::abs(first[i]-restored[i]));energy+=double(first[i])*first[i];}
        require(delta<1e-6f && energy>1e-5,"Signature saved-state synthetic audio roundtrip failed");
        a->copyComparison();a->selectComparison(1);a->selectComparison(0);
        require(a->parameters().state.getChildWithName("GUITAR_SIGNATURE").getProperty("id").toString()==spectralforge::guitarSignatures[size_t(song)].id,"Signature metadata lost in A/B");
        std::cout<<"PASS full-state "<<spectralforge::guitarSignatures[size_t(song)].name<<" parameters="<<params<<" audio_delta="<<delta<<" energy="<<energy<<" synthetic only\n";
    }
}
void originalProduction() {
    using namespace spectralforge;
    // The product dispatch must preserve the measured Original core, without
    // accidentally adding a legacy/native reference EQ or a second drive stage.
    double worst=0;
    for(double rate:{44100.,48000.,192000.,384000.})for(int preset=0;preset<originalPresetCount;++preset) {
        auto state=defaultAmpNativeState(firstOriginalAmpModel);
        const auto voice=original::channelState(preset);state.channel=preset;state.originalModern=voice.modern;
        for(size_t c=0;c<original::controlCount;++c)state.values[c]=voice.values[c];
        AmpNativeDSP product;original::OriginalAmpDSP reference;
        product.prepare(rate);product.set(state);product.reset();reference.prepare(rate);reference.set(voice);reference.reset();
        for(int n=0;n<12000;++n)for(int channel=0;channel<2;++channel) {
            const float x=channel?.013f*std::sin(float(n*.039)):.027f*std::sin(float(n*.071));
            worst=std::max(worst,double(std::abs(product.tick(x,channel)-reference.tick(x,channel))));
        }
    }
    require(worst<1e-7,"Production Original dispatch changes the calibrated core");
    const auto a=std::make_unique<ChimeraProcessor>(),b=std::make_unique<ChimeraProcessor>();
    for(int context=0;context<ampNativeContextCount;++context) {
        const int mode=context==0?0:context<3?1:2,lane=context==0?0:context<3?context-1:context-3;
        set(*a,"mode",float(mode));a->setAmpModel(lane,firstOriginalAmpModel);
        for(size_t c=0;c<original::controlCount;++c) {
            const auto& spec=original::controls[c];const float value=spec.minimum+(spec.maximum-spec.minimum)*(.2f+.1f*context);
            set(*a,ampNativeControlID(context,firstOriginalAmpModel,int(c)),value);
        }
    }
    juce::MemoryBlock bytes;a->getStateInformation(bytes);b->setStateInformation(bytes.getData(),int(bytes.getSize()));
    for(int mode=0;mode<3;++mode){set(*b,"mode",float(mode));for(int lane=0;lane<mode+1;++lane)require(b->selectedAmpModel(lane)==firstOriginalAmpModel,"Original model missing from a saved routing context");}
    for(int context=0;context<ampNativeContextCount;++context)for(size_t c=0;c<original::controlCount;++c) {
        const auto id=ampNativeControlID(context,firstOriginalAmpModel,int(c));
        require(a->parameters().getRawParameterValue(id)->load()==b->parameters().getRawParameterValue(id)->load(),"Original inactive context control changed on restore");
    }
    for(int preset=0;preset<originalPresetCount;++preset) {
        a->loadFactoryPreset(originalPresetStart+preset);a->getStateInformation(bytes);b->setStateInformation(bytes.getData(),int(bytes.getSize()));
        const auto x=render(*a),y=render(*b);double energy=0;float delta=0;
        for(size_t n=0;n<x.size();++n){delta=std::max(delta,std::abs(x[n]-y[n]));energy+=x[n]*x[n];}
        require(delta<1e-6f&&energy>1e-5,"Original preset project roundtrip changed audio");
        const int mode=int(a->parameters().getRawParameterValue("mode")->load());
        const auto gainId=ampNativeControlID(ampNativeContext(mode,0),firstOriginalAmpModel,0,a->selectedAmpChannel(0));
        const float authoredGain=a->parameters().getRawParameterValue(gainId)->load();
        a->copyComparison();a->selectComparison(1);set(*a,gainId,.1f);a->selectComparison(0);
        require(std::abs(a->parameters().getRawParameterValue(gainId)->load()-authoredGain)<.001f,"Original A/B lost the authored gain");
        const auto board=a->pedalBoardState();
        require(board.enabled && board.instances[0].model!=0 && !board.instances[0].bypass,"Original rig is missing its authored PRE chain");
        require(a->parameters().getRawParameterValue(postNativeBypassID(2,preset==1?1:preset==3?2:0))->load()<.5f,"Original rig is missing its native POST EQ");
        require(juce::String(selectablePresetName(originalPresetStart+preset))!=juce::String::fromUTF8(original::channelNames[preset]),"Old channel-named factory example remains selectable");
        const auto knob=[&](original::Control c){return a->parameters().getRawParameterValue(ampNativeControlID(ampNativeContext(mode,0),firstOriginalAmpModel,int(c),a->selectedAmpChannel(0)))->load();};
        if(preset==0)require(knob(original::Control::bloom)>=.749f,"Thall Rhythm BLOOM must start at 7.5 or higher");
        if(preset==1)require(knob(original::Control::rot)>=.749f,"Molten Lead ROT must start at 7.5 or higher");
        if(preset==4)require(std::abs(a->parameters().getRawParameterValue("lowcomp")->load()-.5f)<.001f && std::abs(a->parameters().getRawParameterValue("lowampmix")->load()-.75f)<.001f,"Slam Impact LOW DI COMP / DI-AMP mix changed");
        if(preset==2)require(mode==1 && a->selectedAmpChannel(0)==2 && a->selectedAmpChannel(1)==4 && a->parameters().getRawParameterValue("dualtype")->load()==1,"Grind rig lost its Nidhoggr/Ragnarok crossover Dual routing");
        if(preset==4)require(mode==2 && a->selectedAmpChannel(0)==0 && a->selectedAmpChannel(1)==3 && a->selectedAmpChannel(2)==4 && board.lowTap==1,"Slam rig lost split PRE or its LOW/MID/HIGH channel roles");
    }
    // A pre-1.2 project must get declared defaults, never the last edited bank.
    auto old=b->parameters().copyState();
    for(int i=old.getNumChildren()-1;i>=0;--i){const auto id=old.getChild(i).getProperty("id").toString();if(id.startsWith("originalAmp_")||(id.startsWith("nativeAmp_") && id.contains("_m24_")))old.removeChild(i,nullptr);}
    // New rigs can end in Matrix: make every context a valid pre-Original selection.
    for(int c=0;c<ampNativeContextCount;++c)old.getChildWithProperty("id",ampNativeModelID(c)).setProperty("value",2,nullptr);
    juce::AudioProcessor::copyXmlToBinary(*old.createXml(),bytes);b->setStateInformation(bytes.getData(),int(bytes.getSize()));
    require(b->selectedAmpModel(0)==2,"Old project selection changed during Original migration");
    for(int c=0;c<ampNativeContextCount;++c)require(std::abs(b->parameters().getRawParameterValue(ampNativeControlID(c,firstOriginalAmpModel,0))->load()-.5f)<.001f && b->parameters().getRawParameterValue(originalResponseID(c,0))->load()>.5f,"Pre-Original project inherited a stale Original voice");
    std::cout<<"PASS Original product dispatch residual="<<worst<<"; six contexts, five preset audio roundtrips, A/B and pre-1.2 migration\n";
}
void originalChannels() {
    using namespace spectralforge;
    const auto a=std::make_unique<ChimeraProcessor>(),b=std::make_unique<ChimeraProcessor>();
    AmpNativeParameterCache cache;cache.bind(a->parameters());
    for(int context=0;context<ampNativeContextCount;++context) {
        const int mode=context==0?0:context<3?1:2,lane=context==0?0:context<3?context-1:context-3;
        set(*a,"mode",float(mode));a->setAmpModel(lane,firstOriginalAmpModel);
        for(int channel=0;channel<original::channelCount;++channel) {
            a->setAmpChannel(lane,channel);
            for(size_t control=0;control<original::controlCount;++control) {
                const auto& spec=original::controls[control];
                const float value=spec.minimum+(spec.maximum-spec.minimum)*(.12f+.07f*channel+.04f*context);
                set(*a,ampNativeControlID(context,firstOriginalAmpModel,int(control),channel),value);
            }
            const auto state=cache.read(context);
            require(state.channel==channel && state.originalModern,"Channel did not reach the real DSP cache");
            const auto id=ampNativeControlID(context,firstOriginalAmpModel,0,channel);
            require(std::abs(state.values[0]-a->parameters().getRawParameterValue(id)->load())<1.e-6,"Channel cache reads another channel's knob memory");
        }
        const auto before=a->parameters().copyState();
        for(int channel=0;channel<original::channelCount;++channel)a->setAmpChannel(lane,channel);
        for(auto node:before) {
            const auto id=node.getProperty("id").toString();
            if(id.isEmpty()||id==ampNativeChannelID(context,firstOriginalAmpModel))continue;
            require(std::abs(a->parameters().getRawParameterValue(id)->load()-float(node.getProperty("value")))<1.e-6,"Channel switch rewrote a knob, PRE, CAB or POST state");
        }
    }
    juce::MemoryBlock bytes;a->getStateInformation(bytes);b->setStateInformation(bytes.getData(),int(bytes.getSize()));
    for(int context=0;context<ampNativeContextCount;++context)for(int channel=0;channel<original::channelCount;++channel)for(size_t c=0;c<original::controlCount;++c) {
        const auto id=ampNativeControlID(context,firstOriginalAmpModel,int(c),channel);
        require(a->parameters().getRawParameterValue(id)->load()==b->parameters().getRawParameterValue(id)->load(),"Inactive Original channel lost a knob on binary restore");
    }
    set(*a,"mode",0);a->setAmpChannel(0,1);a->copyComparison();a->selectComparison(1);a->setAmpChannel(0,4);a->selectComparison(0);
    require(a->selectedAmpChannel(0)==1,"A/B lost selected Original channel");a->selectComparison(1);require(a->selectedAmpChannel(0)==4,"A/B overwrote the other Original channel");
    // Reconstruct an incoming v1 snapshot. Missing response flags must NEVER
    // enable the new taper over the owner's existing maxed-out controls.
    auto old=a->parameters().copyState();old.setProperty("schemaVersion",8,nullptr);
    for(int i=old.getNumChildren()-1;i>=0;--i) {
        const auto id=old.getChild(i).getProperty("id").toString();
        if(id.startsWith("originalAmp_")&&id.contains("_ch"))old.removeChild(i,nullptr);
    }
    for(int context=0;context<ampNativeContextCount;++context)old.getChildWithProperty("id",ampNativeChannelID(context,firstOriginalAmpModel)).setProperty("value",0,nullptr);
    juce::AudioProcessor::copyXmlToBinary(*old.createXml(),bytes);b->setStateInformation(bytes.getData(),int(bytes.getSize()));
    AmpNativeParameterCache restored;restored.bind(b->parameters());
    for(int context=0;context<ampNativeContextCount;++context) {
        const auto state=restored.read(context);require(!state.originalModern,"Old v1 snapshot was silently revoiced");
        original::State legacy;for(size_t c=0;c<original::controlCount;++c)legacy.values[c]=state.values[c];
        AmpNativeDSP product;original::OriginalAmpDSP reference;product.prepare(48000);product.set(state);product.reset();reference.prepare(48000);reference.set(legacy);reference.reset();
        for(int n=0;n<12000;++n){const float x=.017f*std::sin(float(n*.063));require(std::abs(product.tick(x,0)-reference.tick(x,0))<1.e-7,"Legacy migration changed the audio transfer");}
        require(b->parameters().getRawParameterValue(originalResponseID(context,1))->load()>.5f,"Old recall contaminated a new inactive channel");
    }
    std::cout<<"PASS 30 independent Original channel banks / 390 knob values, channel-only switching, DSP cache, binary recall, A/B and legacy audio preservation\n";
}
void ownerOriginalReference(const juce::File& source,const juce::File& directory) {
    using namespace spectralforge;juce::MemoryBlock bytes;require(source.loadFileAsData(bytes),"Cannot read owner preset");directory.createDirectory();
    for(int channel=-1;channel<original::channelCount;++channel)for(int upper=0;upper<(channel<0?1:2);++upper) {
        const auto p=std::make_unique<ChimeraProcessor>();p->setStateInformation(bytes.getData(),int(bytes.getSize()));
        require(int(p->parameters().getRawParameterValue("mode")->load())==1,"Owner reference must use Dual");
        if(channel>=0)for(int context=1;context<=2;++context) {
            set(*p,ampNativeChannelID(context,firstOriginalAmpModel),float(channel));set(*p,originalResponseID(context,channel),1);
            auto voice=original::channelState(channel);
            voice[original::Control::midFrequency]=p->parameters().getRawParameterValue(ampNativeControlID(context,firstOriginalAmpModel,int(original::Control::midFrequency)))->load();
            if(upper)voice[original::Control::gain]=1;
            for(size_t c=0;c<original::controlCount;++c)set(*p,ampNativeControlID(context,firstOriginalAmpModel,int(c),channel),voice.values[c]);
        }
        const auto audio=render(*p,false,450);const auto name=channel<0?juce::String("owner_v1"):juce::String(original::channelKeys[channel])+(upper?"_gain_max":"_midpoint");
        require(directory.getChildFile(name+".f32").replaceWithData(audio.data(),audio.size()*sizeof(float)),"Cannot write owner comparison render");
        double energy=0;float peak=0;for(const float v:audio){energy+=double(v)*v;peak=std::max(peak,std::abs(v));}
        std::cout<<"OWNER_FULL_RIG,"<<name<<","<<10*std::log10(energy/audio.size())<<","<<20*std::log10(peak)<<"\n";
    }
}
// 48 kHz K-weighted stereo power, without R128 integration/gating. The short
// fixture uses every sample; an independent FFmpeg R128 probe is also recorded.
// Coefficients: https://ffmpeg.org/doxygen/4.1/f__ebur128_8c.html (PRE/RLB).
double channelWeightedPower(const std::vector<float>& audio) {
    using Filter=juce::dsp::IIR::Filter<double>;
    using Coefficients=juce::dsp::IIR::Coefficients<double>;
    double energy=0;
    for(size_t channel=0;channel<2;++channel) {
        Filter shelf(new Coefficients(1.53512485958697,-2.69169618940638,1.19839281085285,1.,-1.69065929318241,.73248077421585));
        Filter highpass(new Coefficients(1.,-2.,1.,1.,-1.99004745483398,.99007225036621));
        for(size_t block=0;block<audio.size();block+=256)for(size_t n=0;n<128;++n) {
            const double y=highpass.processSample(shelf.processSample(audio[block+channel*128+n]));energy+=y*y;
        }
    }
    return -.691+10*std::log10(energy/(audio.size()/2));
}
// Compare the same cabinet with bare and hot PRE/POST paths. Unweighted RMS
// alone hid the owner's remaining Fenrir advantage under driven conditions.
void originalChannelLevelProbe(const juce::File& directory,bool enforceBalance=true) {
    using namespace spectralforge;directory.createDirectory();
    for(bool driven:{false,true}) {
      double minimum=100,maximum=-100;
      for(int channel=0;channel<original::channelCount;++channel) {
        const auto p=std::make_unique<ChimeraProcessor>();p->loadFactoryPreset(originalPresetStart);
        set(*p,"output",0);set(*p,"gateon",0);
        // This isolates the established amp/channel level comparison. The
        // new full-rig EQ/dynamics recipes are exercised by factoryBank above;
        // they must not flatten channel differences or conceal clipping here.
        set(*p,eqID(0,-1,"bypass"),1);set(*p,eqID(1,-1,"bypass"),1);
        set(*p,"cleanCompOn",0);set(*p,"liveLimiterOn",0);
        if(!driven) {
            set(*p,"boardEnabled",0);
            for(int section=0;section<3;++section)for(int model=0;model<3;++model)set(*p,postNativeBypassID(section,model),1);
        }
        set(*p,ampNativeChannelID(0,firstOriginalAmpModel),float(channel));
        const auto voice=original::channelState(channel);
        for(size_t c=0;c<original::controlCount;++c)set(*p,ampNativeControlID(0,firstOriginalAmpModel,int(c),channel),voice.values[c]);
        const auto audio=render(*p,false,450);double energy=0;float peak=0;
        for(float v:audio){energy+=double(v)*v;peak=std::max(peak,std::abs(v));}
        const auto name=juce::String(driven?"driven_":"cab_")+original::channelKeys[channel];
        require(directory.getChildFile(name+".f32").replaceWithData(audio.data(),audio.size()*sizeof(float)),"Cannot write channel level render");
        const double rmsDb=10*std::log10(energy/audio.size()),weighted=channelWeightedPower(audio);
        minimum=std::min(minimum,weighted);maximum=std::max(maximum,weighted);
        if(enforceBalance)require(peak<1,"Default channel overloads its cabinet path");
        std::cout<<"CHANNEL_LEVEL,"<<name<<","<<rmsDb<<","<<20*std::log10(peak)<<","<<weighted<<"\n";
      }
      if(enforceBalance)require(maximum-minimum<(driven?1.6:2.5),"K-weighted channel spread exceeds the bare/driven cabinet target");
    }
}
void gainAndGR() {
    for(bool board:{false,true})for(int mode:{0,1,2}) {
        const auto p=std::make_unique<ChimeraProcessor>();
        set(*p,"gateon",0);set(*p,"input",0);set(*p,"output",0);set(*p,"mode",float(mode));set(*p,"boardEnabled",board?1.f:0.f);set(*p,"oversampling",0);set(*p,"lowampmix",1);
        for(int lane=1;lane<=3;++lane){set(*p,"ampon"+juce::String(lane),0);set(*p,"cab"+juce::String(lane),0);}
        p->prepareToPlay(48000,128);juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;double input=0,output=0;
        for(int block=0;block<300;++block) {
            for(int n=0;n<128;++n) {const float x=.2f*std::sin(float(block*128+n)*.07f);audio.setSample(0,n,x);audio.setSample(1,n,-.7f*x);if(block>100)input+=double(x)*x;}
            p->processBlock(audio,midi);if(block>100)for(int n=0;n<128;++n)output+=std::pow(audio.getSample(0,n),2);
        }
        const auto db=10*std::log10(output/input);require(std::abs(db)<.05,"Unity routing unexpectedly loses level");
        require(p->getLatencySamples()<p->pitchLatency(),"Zero-shift/bypass retains pitch-window latency");p->releaseResources();
        std::cout<<"MEASURE unity mode="<<mode<<" board="<<board<<" delta_db="<<db<<" zero_shift_reported="<<p->getLatencySamples()<<'\n';
    }
    for(int model=6;model<=10;++model) {
        const auto p=std::make_unique<ChimeraProcessor>();set(*p,"gateon",0);set(*p,"output",0);p->setPedalModel(0,model);
        for(int c=0;c<spectralforge::pedalModel(model).controlCount;++c)set(*p,spectralforge::pedalControlID(0,model,c),spectralforge::pedalModel(model).controls[size_t(c)].maximum);
        p->prepareToPlay(48000,128);juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;
        for(int block=0;block<100;++block){for(int c=0;c<2;++c)for(int n=0;n<128;++n)audio.setSample(c,n,.5f);p->processBlock(audio,midi);}
        require(p->pedalReduction()>.1f && std::isfinite(p->pedalReduction()),"Compressor DSP GR did not reach processor meter");
        set(*p,spectralforge::pedalBypassID(0,model),1);for(int i=0;i<20;++i){audio.clear();p->processBlock(audio,midi);}
        require(p->pedalReduction()==0,"Bypassed compressor retains stale GR");p->releaseResources();
        std::cout<<"PASS processor pedal GR model="<<model<<" active and bypass\n";
    }
}
}
#include "PresetGainTests.h"
#include "NiflheimrPresetTests.h"
#include "CabPreparedStateTests.h"
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    std::cout<<std::unitbuf;
    try {
        if(argc==3&&juce::String(argv[1])=="--niflheimr-presets"){niflheimrPresetTests::run(juce::File(argv[2]));return 0;}
        if(argc==2&&juce::String(argv[1])=="--cab-prepared-state-only"){cabPreparedStateTests::run();return 0;}
        if(argc==3&&juce::String(argv[1])=="--original-channel-levels"){originalChannelLevelProbe(juce::File(argv[2]),false);return 0;}
        if(argc==4&&juce::String(argv[1])=="--owner-original-reference"){ownerOriginalReference(juce::File(argv[2]),juce::File(argv[3]));return 0;}
        if(argc>1&&juce::String(argv[1])=="--original-production-only"){originalProduction();originalChannels();return 0;}
        if(argc>1&&juce::String(argv[1])=="--measure-gain"){presetGainTests::run(true);return 0;}
        const auto measurement=argc>2?juce::String(argv[2]):juce::String{};
        const bool originalOnly=measurement=="--measure-original";
        const bool headerVoicing=originalOnly||measurement=="--measure-presets";
        const bool measureOnly=headerVoicing||measurement=="--measure-production-presets";
        require(measurement.isEmpty()||measureOnly,"Unknown integrated processor measurement mode");
        std::set<int> selected;
        if(argc>3) {
            require(measureOnly && argc==4,"Preset subsets are diagnostic only; strict regression always checks the complete bank");
            const auto option=juce::String(argv[3]);
            require(option.startsWith("--preset-ids="),"Expected --preset-ids=0,1,... after the measurement mode");
            const auto ids=option.fromFirstOccurrenceOf("=",false,false);
            require(ids.isNotEmpty() && !ids.startsWithChar(',') && !ids.endsWithChar(',') && !ids.contains(",,"),"Empty diagnostic preset ID");
            for(const auto& token:juce::StringArray::fromTokens(ids,",","")) {
                require(token.isNotEmpty() && token.containsOnly("0123456789"),"Invalid diagnostic preset ID");
                const int index=token.getIntValue();
                require(index>=0 && index<spectralforge::selectablePresetCount && selected.insert(index).second,"Diagnostic preset IDs must be unique and in range");
                require(!originalOnly||spectralforge::isOriginalPreset(index),"Original-only measurement received a different preset bank");
            }
        }
        if(!measureOnly)cabPreparedStateTests::run();
        factoryBank(measureOnly,originalOnly,headerVoicing,selected);
        if(!measureOnly){originalProduction();originalChannels();originalChannelLevelProbe(juce::File(argc>1?argv[1]:"/tmp/chimera-signatures").getChildFile("channel-levels"));signatures(juce::File(argc>1?argv[1]:"/tmp/chimera-signatures"));gainAndGR();presetGainTests::run(false);niflheimrPresetTests::run(juce::File(argc>1?argv[1]:"/tmp/chimera-signatures").getChildFile("niflheimr-presets"));}
    }
    catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
