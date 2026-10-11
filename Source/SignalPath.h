#pragma once
#include "PluginProcessor.h"
#include "EffectSelectionCatalog.h"
#include "PostNativeParameters.h"
#include "CabLayoutModel.h"

namespace spectralforge::signalPath {
enum class Target { input, gate, transpose, tuner, pre, rigs, amp, cab, low, post, utilities, output };
struct Node {
    juce::String id,label,status,detail;
    Target target{}; int lane{-1},section{-1}; bool active{true};
    juce::Rectangle<int> bounds;
    bool operator==(const Node&) const = default;
};
struct Edge {juce::String from,to;bool tap{};bool operator==(const Edge&) const = default;};
struct Snapshot {
    int mode{};bool dualCross{};std::vector<Node> nodes;std::vector<Edge> edges;
    bool operator==(const Snapshot&) const = default;
    const Node* find(const juce::String& id) const {for(const auto& n:nodes)if(n.id==id)return &n;return nullptr;}
};
// Presentation-only read of the same parameter sources used by process(),
// PreFXChain/PedalBoardDSP, Engine and PostFXChain. No state/host writes.
inline Snapshot read(ChimeraProcessor& p) {
    Snapshot s;auto& state=p.parameters();
    const auto v=[&](const juce::String& id){const auto* raw=state.getRawParameterValue(id);return raw?raw->load():0.f;};
    const auto on=[&](const juce::String& id){return v(id)>.5f;};
    s.mode=juce::jlimit(0,2,int(v("mode")));s.dualCross=s.mode==1 && on("dualtype");
    const auto add=[&](juce::String id,juce::String label,Target target,int x,int y,bool active=true,juce::String status={},int lane=-1,int section=-1,juce::String detail={}) {
        s.nodes.push_back({id,label,status.isEmpty()?(active?"ACTIVE":"BYPASSED"):status,detail,target,lane,section,active,{x,y,108,46}});return id;
    };
    const auto edge=[&](juce::String a,juce::String b,bool tap=false){s.edges.push_back({a,b,tap});};
    const bool postGate=on("gateAfterRig");
    add("input","INPUT",Target::input,16,42,true,on("inputmode")?"MONO L":"STEREO");
    add("tuner","TUNER TAP",Target::tuner,16,100,on("tuneron"),{},-1,-1,"Input-gain tap, before gate and Transpose. Tuner mute is applied at output.");edge("input","tuner",true);
    auto previous=juce::String("input");
    if(!postGate){add("gate","INPUT GATE",Target::gate,138,42,on("gateon"));edge(previous,"gate");previous="gate";}
    add("transpose","TRANSPOSE",Target::transpose,260,42,on("transposeon") && int(v("transpose"))!=0,juce::String(int(v("transpose")))+" st"+(on("transposeon")&&int(v("transpose"))!=0?" / ACTIVE":" / BYPASSED"));edge(previous,"transpose");previous="transpose";
    const auto board=p.pedalBoardState();juce::String cleanTap=previous;
    std::array<int,5> legacy=on("preorder")?std::array<int,5>{4,3,5,6,0}:std::array<int,5>{3,4,5,6,0};
    if(on("gainorder"))std::swap(legacy[3],legacy[4]);
    const std::array<const char*,11> enable{{"preon","delayon","reverbon","precompon","filteron","fuzzon","booston","buscompon","preampon","eqon","choruson"}};
    for(int position=0;position<5;++position) {
        juce::String id,label;bool active=false;
        if(board.enabled){const int owner=board.order[size_t(position)];const auto& instance=board.instances[size_t(owner)];id="pre.owner"+juce::String(owner);label=pedalMenuName(instance.model);active=instance.model!=0&&!instance.bypass;}
        else {const int family=legacy[size_t(position)];id="pre.legacy"+juce::String(family);label=effectFamilyMenuName(family,int(v(modelFamilies[size_t(family)].parameter)));active=on(enable[size_t(family)]);}
        add(id,label,Target::pre,382+position*122,42,active,{},-1,position,"PRE position "+juce::String(position+1)+". Navigation preserves the existing instance/order.");edge(previous,id);previous=id;
        if(position+1==(board.enabled?board.lowTap:2))cleanTap=id;
    }
    // Engine replaces wet LOW with the clean, latency-aligned tap. Never draw
    // a wet PRE -> LOW amp connection that does not exist in the DSP.
    add("split",s.mode==0?"RIG INPUT":s.mode==1?(s.dualCross?"DUAL LR4":"PARALLEL"):"WET LR4",Target::rigs,16,300,true,s.mode==2?juce::String(v("x1"),0)+" / "+juce::String(v("x2"),0)+" Hz":s.dualCross?juce::String(v("dualcross"),0)+" Hz":"FULL RANGE");edge(previous,"split");
    if(s.mode==2) {
        add("clean","CLEAN TAP",Target::pre,138,155,true,"ALIGNED",-1,-1,"After PRE position "+juce::String(board.enabled?board.lowTap:2)+"; remainder of PRE latency aligned.");edge(cleanTap,"clean",true);
        add("di.xo","DI LOW LR4",Target::rigs,260,155,true,juce::String(v("x1"),0)+" Hz");edge("clean","di.xo");
        add("low.tone","LOW TONE",Target::low,382,155,true,{},0);edge("di.xo","low.tone");
        add("low.comp","LOW COMP",Target::low,504,155,v("lowcomp")>0,juce::String(v("lowcomp"),2)+(v("lowcomp")>0?" / ACTIVE":" / BYPASSED"),0);edge("low.tone","low.comp");
    }
    bool anySolo=false;const int count=s.mode+1;for(int i=0;i<count;++i)anySolo|=on("solo"+juce::String(i+1));
    for(int i=0;i<count;++i) {
        const auto n=juce::String(i+1);const int context=ampNativeContext(s.mode,i);
        const auto amp="amp.context"+juce::String(context),cab="cab.context"+juce::String(context),level="level.context"+juce::String(context);
        const int y=s.mode==2?(i==0?218:i==1?300:382):s.mode==1?218+i*110:270;
        auto source=juce::String("split");
        if(s.mode==2 && i==0)source="low.comp";
        else if(s.mode==2 || s.dualCross){const auto tone="tone.context"+juce::String(context);add(tone,s.mode==2?(i==1?"MID TONE":"HIGH TONE"):(i==0?"LOW TONE":"HIGH TONE"),Target::amp,504,y,true,{},i);edge(source,tone);source=tone;}
        const int model=p.selectedAmpModel(i);const auto& definition=ampNativePanel(model);
        const int channel=juce::jlimit(0,int(definition.channels.size())-1,p.selectedAmpChannel(i));
        const bool low=s.mode==2&&i==0;
        add(amp,ampInfo(model).name,Target::amp,626,y,low || on("ampon"+n),low?"LOW AMP BRANCH":on("ampon"+n)?"ACTIVE":"BYPASSED",i,-1,"Context: "+juce::String(ampNativeContextNames[context])+" / "+juce::String::fromUTF8(definition.channels[size_t(channel)])+(low?". Processed internally; AMP switch sets blend to DI when off.":""));edge(source,amp);
        const bool modeledA=on(originalCabID(i,"Aon")),modeledB=on(originalCabID(i,"Bon"));
        const bool modeled=modeledA||modeledB;
        const int layout=cabLayout::layoutIndex(int(v(cabLayoutID(i,"layout"))));
        const auto cabLabel=modeled?(layout?juce::String(cabLayout::layouts[size_t(layout-1)].name):on(originalCabID(i,"design"))?juce::String("Bass 4x10"):juce::String("Guitar 4x12")):int(v("cabtype"+n))==0?juce::String("CAB FILTERS"):int(v("cabtype"+n))==3?p.userIRName(i):int(v("cabtype"+n))==1?juce::String("V30 / SM57"):juce::String("Jensen / SM57");
        add(cab,cabLabel,Target::cab,748,y,on("cab"+n),on("cab"+n)?(modeledA&&modeledB?"ORIGINAL A + B":modeled?"MODEL + IR/FILTER":"IR / FILTERS"):"BYPASSED",i,-1,"Mic A: "+juce::String(modeledA?"original model":"IR / filters")+"; Mic B: "+juce::String(modeledB?"original model":"IR / filters")+"; "+juce::String(v("cabblend"+n)*100,0)+"% B. Blended inside CAB before LOW DI/AMP blending. "+p.userIRName(i));edge(amp,cab);
        auto last=cab;
        if(low){
            add("low.di","DI ALIGN",Target::low,626,155,true,"HEAD LATENCY",0);edge("low.comp","low.di");
            const float mix=on("ampon1")?juce::jlimit(0.f,1.f,v("lowampmix")):0.f;
            add("low.blend","DI / AMP+CAB",Target::low,870,155,true,juce::String((1-mix)*100,0)+" : "+juce::String(mix*100,0),0,-1,"Both branches share LOW tone/compression. Linear blend occurs AFTER CAB, BEFORE lane level/mute/polarity and band sum.");edge("low.di","low.blend");edge(cab,"low.blend");last="low.blend";
        }
        const bool audible=!on("mute"+n)&&(!anySolo||on("solo"+n));
        juce::String levelStatus=audible?(on("polarity"+n)?"ACTIVE / INV":"ACTIVE"):"MUTED";
        if(s.mode==1&&!s.dualCross)levelStatus+=" / "+juce::String((i==0?1-v("dualblend"):v("dualblend"))*100,0)+"%";
        add(level,s.mode==2?(i==0?"LOW LEVEL":i==1?"MID LEVEL":"HIGH LEVEL"):"RIG "+n+" LEVEL",Target::amp,992,y,audible,levelStatus,i);edge(last,level);edge(level,"merge");
    }
    add("merge","RIG SUM",Target::rigs,992,440,true,s.mode==1&&!s.dualCross?juce::String((1-v("dualblend"))*100,0)+" : "+juce::String(v("dualblend")*100,0):"SUM");
    previous="merge";
    if(postGate){add("gate","POST GATE",Target::gate,16,440,on("gateon"),{},-1,-1,"Detector: clean input before PRE; attenuation after AMP/CAB merge, before POST.");edge("input","gate",true);edge(previous,"gate");previous="gate";}
    for(int position=0;position<6;++position){
        const std::array<int,6> families{7,8,9,10,1,2};const int family=families[size_t(position)];juce::String label=effectFamilyMenuName(family,int(v(modelFamilies[size_t(family)].parameter)));bool active=on(enable[size_t(family)]);
        if(position<3&&on(postNativeModeID(position))){const int model=juce::jlimit(0,2,int(v(postNativeModelID(position))));label=postNativeModel(position,model).name;active=!on(postNativeBypassID(position,model));}
        const auto id="post.section"+juce::String(position);add(id,label,Target::post,16+position*122,510,active,{},-1,position);edge(previous,id);previous=id;
    }
    add("utilities","DOUBLER / CLICK",Target::utilities,748,510,(on("doubleron")&&p.getTotalNumOutputChannels()==2)||on("metronome"),(on("doubleron")&&p.getTotalNumOutputChannels()==2)?"STEREO DOUBLER":on("metronome")?"CLICK":"BYPASSED",-1,-1,"Doubler is inactive for mono processing. Metronome is added after POST.");edge(previous,"utilities");
    add("output","OUTPUT",Target::output,992,510,true,on("tuneron")&&on("tunermute")?"TUNER MUTED":"TRIM",-1,-1,"Output gain and tuner mute are last. No global Final EQ or output limiter is implemented yet.");edge("utilities","output");
    return s;
}
} // namespace spectralforge::signalPath
