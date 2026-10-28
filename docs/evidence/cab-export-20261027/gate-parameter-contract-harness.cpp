#include <juce_cryptography/juce_cryptography.h>
#include "PluginProcessor.h"
#include "Fixtures/ReleasedParameterContract.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
void set(ChimeraProcessor& p,const juce::String& id,float value)
{
    auto* parameter=p.parameters().getParameter(id);require(parameter!=nullptr,"Missing parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
float get(ChimeraProcessor& p,const juce::String& id) {return p.parameters().getRawParameterValue(id)->load();}
// Only new-session defaults approved for the 1.1.2 calibration patch may change.
// Keep the historical fixture intact; saved values, IDs and ranges stay frozen.
struct Default112 { const char* suffix; double oldValue,newValue; };
constexpr Default112 defaults112[] {
    {"m2_hw_lead_pre_gain",.5,.68}, {"m3_hw_ch2_gain",.5,.65},
    {"m3_hw_ch3_gain",.5,.68}, {"m3_hw_ch2_mode",0,.5}, {"m3_hw_ch3_mode",0,1},
    {"m4_hw_lead_gain",.5,.72}, {"m4_hw_lead_drive",.5,.65},
    {"m9_hw_dirty_gain",.5,.66}, {"m15_ch3_gain",.5,.78}, {"m15_ch4_gain",.5,.68},
    {"m15_channel",0,2./15.}, {"m17_hw_ch3_gain",.5,.78}, {"m17_hw_ch4_gain",.5,.72},
    {"m20_hw_ep_gain",.5,.72}, {"m20_hw_kk_gain1",.5,.68}, {"m20_hw_kk_gain2",.5,.62},
    {"m21_hw_overdrive_preamp",.5,.72}, {"m22_hw_lead_gain",.5,.70}
};
const Default112* approvedDefault112(const juce::String& id)
{
    static constexpr Default112 legacyZuta {"",0,2./3.};
    for(int bank=1;bank<=3;++bank) {
        if(id=="ampchannel"+juce::String(bank)+"_m15")return &legacyZuta;
    }
    for(int bank=0;bank<6;++bank)for(const auto& entry:defaults112)
        if(id=="nativeAmp_c"+juce::String(bank)+"_"+entry.suffix)return &entry;
    return nullptr;
}
void parameterContract(ChimeraProcessor& p)
{
    juce::String manifest;int count=0;
    for(auto* raw:p.getParameters()) {
        auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(raw);require(parameter!=nullptr,"Parameter has no ID/range");
        if (parameter->getParameterIndex() >= 3686) break;
        require(std::isfinite(parameter->getDefaultValue()) && std::isfinite(parameter->convertFrom0to1(0.f))
                && std::isfinite(parameter->convertFrom0to1(.5f)) && std::isfinite(parameter->convertFrom0to1(1.f)),"Non-finite released parameter mapping");
        manifest+=juce::String(count++)+"|"+parameter->paramID+"|"+juce::String(parameter->getVersionHint())+"|"
            +juce::String(parameter->getDefaultValue(),9)+"|"+juce::String(parameter->getNumSteps())+"|"
            +juce::String(parameter->convertFrom0to1(0.f),9)+"|"+juce::String(parameter->convertFrom0to1(.5f),9)+"|"
            +juce::String(parameter->convertFrom0to1(1.f),9)+"|"+juce::String(int(parameter->isAutomatable()))+"\n";
    }
    const auto digest=juce::SHA256(manifest.toRawUTF8(),manifest.getNumBytesAsUTF8()).toHexString();
    juce::String frozen;
    for(const auto* chunk:releasedParameterContractChunks)frozen+=chunk;
    require(juce::SHA256(frozen.toRawUTF8(),frozen.getNumBytesAsUTF8()).toHexString()
            =="1de9d010b4267088d80a6922105b047ae0c9e142e3b931c000129d6befa3523f","Frozen released contract fixture changed");
    const auto actualRows=juce::StringArray::fromLines(manifest),expectedRows=juce::StringArray::fromLines(frozen);
    require(actualRows.size()==expectedRows.size(),"Released parameter count changed");
    int numericDifferences=0,approvedDefaults=0;
    for(int row=0;row<expectedRows.size();++row) {
        if(expectedRows[row].isEmpty()){require(actualRows[row].isEmpty(),"Unexpected contract row");continue;}
        const auto actual=juce::StringArray::fromTokens(actualRows[row],"|",{});
        const auto expected=juce::StringArray::fromTokens(expectedRows[row],"|",{});
        require(actual.size()==9 && expected.size()==9,"Malformed parameter contract row");
        for(int field=0;field<9;++field) {
            if(field==3 || (field>=5 && field<=7)) {
                // JUCE skew conversion uses exp/log. ARM and x86 libm/FMA may
                // round the same float mapping differently; preserve numeric
                // semantics without relying on nine-decimal string identity.
                const double a=actual[field].getDoubleValue();
                double e=expected[field].getDoubleValue();
                if(field==3)if(const auto* change=approvedDefault112(expected[1])) {
                    require(std::abs(e-change->oldValue)<1.e-6,"Approved default no longer matches its released value");
                    e=change->newValue;++approvedDefaults;
                }
                // Near zero, cancellation/FMA error scales with the domain,
                // not the result (e.g. ARM bass midpoint -2.68e-7 vs x86 0).
                // Normalized defaults have a unit domain; range samples use
                // their frozen endpoints/span. No host structure is relaxed.
                const double lo=expected[5].getDoubleValue(),hi=expected[7].getDoubleValue();
                const double scale=field==3 ? 1. : std::max({std::abs(lo),std::abs(hi),std::abs(hi-lo)});
                const double tolerance=5e-10+4*double(std::numeric_limits<float>::epsilon())*scale;
                if(std::abs(a-e)>tolerance) {
                    std::cerr<<"Contract mismatch "<<expected[1]<<" field="<<field<<" expected="<<juce::String(e,9)<<" actual="<<actual[field]<<'\n';
                    require(false,"Released default/range numeric contract changed");
                }
                if(a!=e){++numericDifferences;if(numericDifferences<=8)std::cout<<"ROUNDING "<<expected[1]<<" field="<<field<<" expected="<<juce::String(e,9)<<" actual="<<actual[field]<<'\n';}
            } else require(actual[field]==expected[field],"Released ID/order/version/steps/automation contract changed");
        }
    }
    require(approvedDefaults==6*int(std::size(defaults112))+3,"Approved default coverage changed");
    auto* range=p.parameters().getParameter("gateRangeDb");
    // Five-channel Original appends 342 parameters after the frozen 3981.
    // Niflheimr appends six contexts with two selectors and five 14-control banks.
    // CAB then appends ten version-6 controls for each of the three lanes.
    constexpr int cabParameterCount=3*10;
    constexpr int expectedParameterCount=4323+6*(2+5*14)+cabParameterCount+spectralforge::originalCabParameterCount+spectralforge::cabExpansionParameterCount+spectralforge::cabLayoutParameterCount+spectralforge::graphicalEQParameterCount+19+spectralforge::commonDynamicsParameterCount+spectralforge::multibandParameterCount+spectralforge::deEsserParameterCount+spectralforge::extraGateParameterCount+6;
    require(range && range->getParameterIndex()==3890 && count==3686 && p.getParameters().size()==expectedParameterCount,"Gate Range ordinal or complete appended channel parameter count changed");
    require(range->getVersionHint()==2 && range->isAutomatable(),"Range AU version hint/automation changed");
    require(range->getText(1,0)=="Full" && range->getValueForText("Full")==1.f,"Host Full text does not round-trip");
    require(std::abs(range->convertFrom0to1(range->getValueForText("24 dB"))-24)<.01f,"Host dB text does not parse");
    require(get(p,"gateRangeDb")==96,"New-session default is not legacy Full");
    std::cout<<"PASS legacy parameter contract: "<<count<<" entries SHA256 "<<digest<<" approved_112_defaults="<<approvedDefaults<<" numeric_rounding_differences="<<numericDifferences<<"; append-only gateRangeDb\n";
}
}
int main(){juce::ScopedJuceInitialiser_GUI gui;try{auto p=std::make_unique<ChimeraProcessor>();parameterContract(*p);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
