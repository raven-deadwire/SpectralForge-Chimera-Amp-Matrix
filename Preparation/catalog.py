#!/usr/bin/env python3
"""Inventory + explicit control descriptors; never injected into production APVTS.
Legacy ranges are CURRENT CHIMERA DSP ranges, not hardware specifications.
New normalized controls describe UI positions only; taper/DSP/factory defaults are pending.
"""
from __future__ import annotations
import json
import math
from pathlib import Path
from pedal_controls import upgrade_pedals
from rack_controls import upgrade_racks, validate_rack_panels

BASE = "025d30471109649e5e3243ff1e7e89639a576b79"
REPO = "https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/" + BASE + "/"

def knob(key, label=None, lo=0, hi=1, initial=.5, unit="position", group="main", basis="prototype_position_not_hardware_taper"):
    return dict(id=key,label=label or key.title(),kind="knob",minimum=lo,maximum=hi,
                initial=initial,step=.01,unit=unit,group=group,range_basis=basis,dsp_binding=None)

def choice(key, labels, group="main"):
    return dict(id=key,label=key.split('.')[-1].replace('_',' ').title(),kind="choice",
                options=labels,initial=0,group=group,range_basis="discrete_labels",dsp_binding=None)

def switch(key, group="main"):
    return dict(id=key,label=key.split('.')[-1].replace('_',' ').title(),kind="toggle",
                initial=0,group=group,range_basis="binary_state",dsp_binding=None)

def legacy_controls(family):
    # Exact old parameter names and ranges retained for raw-value migration.
    values = {
      "drive":[("predrive",0,1,.3),("pretone",1000,10000,4000),("prelevel",-18,12,0)],
      "comp":[("precomp",0,1,.35),("precompattack",1,50,15),("precomplevel",-12,18,0)],
      "filter":[("filtersense",0,1,.4),("filterq",.5,3,1.2),("filtermix",0,1,1)],
      "fuzz":[("fuzzdrive",0,36,18),("fuzztone",0,1,.45),("fuzzlevel",-24,6,-12)],
      "boost":[("boostgain",0,18,6),("boostbass",-12,12,0),("boosttreble",-12,12,0)],
    }
    labels={"predrive":"Drive","pretone":"Tone","prelevel":"Level","precomp":"Amount",
            "precompattack":"Attack","precomplevel":"Level","filtersense":"Sensitivity","filterq":"Q",
            "filtermix":"Mix","fuzzdrive":"Drive","fuzztone":"Tone","fuzzlevel":"Level",
            "boostgain":"Gain","boostbass":"Bass","boosttreble":"Treble"}
    result=[]
    for key,lo,hi,value in values.get(family,[]):
        unit="Hz" if key=="pretone" else "ms" if "attack" in key else "dB" if key in {"prelevel","precomplevel","fuzzdrive","fuzzlevel","boostgain","boostbass","boosttreble"} else "value"
        result.append(knob(key,labels[key],lo,hi,value,unit,basis="existing_chimera_dsp_not_hardware"))
    return result

FAMILIES = [
 ("drive","DRIVE","pre",[("Green 808","Ibanez TS808"),("Gold Drive","Klon Centaur"),("Rodent","Pro Co RAT"),("Bass DI","Tech 21 SansAmp Bass Driver DI"),("Micro Bass","Darkglass B3K")]),
 ("delay","DELAY","post",[("Digital 229","TC Electronic 2290"),("Tape Echo","Roland RE-201"),("Analog Memory","EHX Deluxe Memory Man")]),
 ("reverb","REVERB","post",[("Studio Plate","EMT 140"),("Concert Hall","Lexicon 224"),("Spring Tank","Fender spring reverb")]),
 ("comp","COMPRESSOR","pre",[("Studio VCA","MXR M87"),("Red OTA","MXR Dyna Comp"),("Optical","Diamond Compressor"),("Studio FET","Cali76 / 1176"),("Variable Mu","Manley Variable Mu")]),
 ("filter","ENVELOPE","pre",[("Q Sweep","EHX Q-Tron"),("Tron Band","Mu-Tron III"),("Reverse Sweep","Mu-Tron III Down"),("Bass Envelope","MXR M82"),("Dynamic Wah","BOSS AW-3")]),
 ("fuzz","FUZZ","pre",[("Big Sustain","EHX Big Muff Pi"),("Round Face","Dunlop Fuzz Face"),("Bender","Sola Sound Tone Bender"),("Wool Bass","ZVEX Woolly Mammoth"),("Gated Factory","ZVEX Fuzz Factory")]),
 ("boost","BOOST","pre",[("RC Clean","Xotic RC Booster"),("Treble Lift","Dallas Rangemaster"),("Micro Lift","MXR Micro Amp"),("EP Lift","Xotic EP Booster"),("Linear Power","EHX LPB-1")]),
 ("bus","BUS COMP","post",[("Console VCA","SSL Bus Compressor"),("FET 76","UREI 1176"),("Opto Level","Teletronix LA-2A")]),
 ("preamp","PREAMP","post",[("N73 Colour","Neve 1073"),("V5 Pure","Avalon V5"),("ISA Blue","Focusrite ISA")]),
 ("eq","EQUALIZER","post",[("Console E","SSL E Series"),("N73 Shelves","Neve 1073"),("Passive Tube","Pultec EQP-1A")]),
 ("mod","MODULATION","post",[("Classic Chorus","BOSS CE-2"),("Dimension","Roland Dimension D"),("Stone Phase","EHX Small Stone"),("Mistress Flange","EHX Electric Mistress"),("Eddy Vibrato","EHX Eddy"),("Pulsar Tremolo","EHX Pulsar")]),
]
AMPS = [
 ("Glass","Fender '65 Twin Reverb"),("Brit Edge","Marshall JTM45"),("Tight 515","Peavey 6505 / 5150"),
 ("Wide Rect","Mesa Dual Rectifier"),("Liquid Lead","Mesa Mark IV Lead"),("Iron Tube","Ampeg SVT-VR"),
 ("Solid Punch","GK 800RB"),("Modern Bass","B7K Ultra + Aguilar DB751 chain"),("Chime 30","VOX AC30 Top Boost"),
 ("Orange Crown","Orange Rockerverb 50 MKIII"),("Bassman Valve","Fender Super Bassman"),("Subway Clean","Mesa D-800+"),
 ("Match Chime","Matchless DC-30 / clone reference"),("Silk ODS","Dumble ODS / clone reference"),("Taste Punch","EICH T900")]


def build_catalog():
    models=[]
    for family,category,location,entries in FAMILIES:
        for index,(name,reference) in enumerate(entries):
            controls=legacy_controls(family)
            models.append(dict(id=f"legacy.{family}.{index}",name=name,reference=reference,category=category,
                               location=location,status="existing_dsp",controls_status="legacy_interface_not_hardware" if controls else "native_panel_pending",
                               controls=controls,legacy=dict(parameter=family+"model",raw_index=index),
                               sources=[REPO+"Source/ModelCatalog.h",REPO+"Source/FXParameters.h"],audio_in_prototype=False))
    for index,(name,reference) in enumerate(AMPS):
        models.append(dict(id=f"legacy.amp.{index}",name=name,reference=reference,category="AMP",location="rig",
                           status="existing_dsp",controls_status="native_panel_pending",controls=[],
                           legacy=dict(raw_index=index),sources=[REPO+"Source/AmpCatalog.h"],audio_in_prototype=False))
    planned_amps=[("zuta-gbg120","ZUTA GBG120","required"),("engl","Iron Compact / ENGL Ironball (legacy)","required"),
                  ("diezel-vh4","Diezel VH4 — reference pending","required"),("svt-cl","Ampeg SVT-CL","required"),
                  ("fortin","Fortin Evil Pumpkin","candidate"),("soldano","Soldano SLO-100 LTD OD","candidate"),
                  ("bogner","Bogner Uberschall Rev Blue","candidate"),("sunn","SUNN — Model T under review","required"),
                  ("engl-e670fe","Special Edition / ENGL E670FE","required")]
    for key,name,priority in planned_amps:
        models.append(dict(id="planned.amp."+key,name=name,reference=name,category="AMP",location="rig",
                           status="not_implemented",priority=priority,active=key!="engl",controls_status="native_panel_pending",controls=[],sources=[],audio_in_prototype=False))
    def add(key,name,category,controls,source=None,status="documented_labels_unimplemented_dsp"):
        models.append(dict(id="planned."+key,name=name,reference=name,category=category,location="pre",status="not_implemented",
                           controls_status=status,controls=controls,sources=[source] if source else [],audio_in_prototype=False))
    add("sd1","BOSS SD-1","DRIVE",[knob(k) for k in ("drive","tone","level")],"https://www.boss.info/global/products/sd-1/")
    add("ocd-v2","Fulltone OCD v2","DRIVE",[knob(k) for k in ("volume","drive","tone")]+[choice("peak",["HP","LP"])],
        "https://www.tone3000.com/tones/fulltone-ocd-v2-49361","capture_labels_manufacturer_and_taper_review_pending")
    add("distortion-plus","MXR Distortion+ M104","DRIVE",[knob("distortion"),knob("output")],"https://www.jimdunlop.com/mxr-distortion/")
    add("jb2","BOSS/JHS JB-2","DRIVE",[knob(f"{side}.{key}",key.title(),group=side.upper()) for side in ("boss","jhs") for key in ("drive","tone","level")]+
        [choice("mode",["JHS","BOSS","JHS / BOSS toggle","JHS > BOSS","BOSS > JHS","PARALLEL"])],"https://www.boss.info/global/products/jb-2/")
    # Original interface proposals, NOT a claim that OC-2 has an octave-up control.
    add("octaver-mono","Mono Octaver — design study","PITCH",[knob("dry"),knob("down1"),knob("down2")],status="original_ui_proposal")
    add("octaver-poly","Poly Octaver — design study","PITCH",[knob("dry"),knob("down"),knob("up")],status="original_ui_proposal")
    add("wah","Manual Wah — design study","WAH",[knob("position"),knob("range"),knob("q")],status="original_ui_proposal")
    add("pre-eq","PRE Graphic EQ — design study","EQUALIZER",
        [knob(f"band.{hz}",hz+" Hz",-12,12,0,"dB",basis="original_design") for hz in ("31.25","62.5","125","250","500","1000","2000","4000","8000","16000")]+
        [knob("input","Input",-12,12,0,"dB",basis="original_design"),knob("output","Output",-12,12,0,"dB",basis="original_design")],status="original_ui_proposal")
    for key,name,category in (("fortin33","Fortin 33","BOOST"),("phase90","MXR Phase 90","MODULATION"),("octave-fuzz","Octave Fuzz — reference pending","FUZZ")):
        add(key,name,category,[],status="native_panel_pending")
    from amp_controls import upgrade_amplifiers
    upgrade_amplifiers(models)
    upgrade_pedals(models)
    upgrade_racks(models)
    catalog=dict(control_revision=3,schema_version=1,base_commit=BASE,max_pedals=5,models=models,
                 warning="Control-thread preparation only. No new model DSP or hardware calibration is implemented. Normalized defaults are UI seeds, not hardware factory settings.")
    validate_catalog(catalog)
    return catalog


def validate_catalog(catalog):
    from amp_controls import validate_amp_panels
    validate_amp_panels(catalog["models"])
    validate_rack_panels(catalog["models"])
    if catalog.get("schema_version")!=1 or catalog.get("max_pedals")!=5:
        raise ValueError("Wrong catalog schema/capacity")
    ids=set()
    for model in catalog["models"]:
        if model["id"] in ids: raise ValueError("Duplicate model id")
        ids.add(model["id"])
        if set(c["id"] for c in model["controls"]) & set(c["id"] for c in model.get("legacy_controls",[])):
            raise ValueError("Legacy/native control collision")
        for controls in (model["controls"],model.get("legacy_controls",[]),model.get("native_target",{}).get("controls",[])):
            keys=set()
            for c in controls:
                if c["id"] in keys: raise ValueError("Duplicate control id")
                keys.add(c["id"])
                if c["kind"]=="knob":
                    vals=[c[k] for k in ("minimum","maximum","initial","step")]
                    if any(type(v) not in (int,float) or not math.isfinite(v) for v in vals): raise ValueError("Nonfinite descriptor")
                    if not (c["minimum"]<=c["initial"]<=c["maximum"] and c["minimum"]<c["maximum"] and c["step"]>0): raise ValueError("Bad range")
                elif c["kind"]=="choice":
                    if not c["options"] or len(set(c["options"]))!=len(c["options"]) or type(c["initial"]) is not int or not 0<=c["initial"]<len(c["options"]): raise ValueError("Bad options")
                elif c["kind"]=="toggle":
                    if c["initial"] not in (0,1): raise ValueError("Bad toggle")
                else: raise ValueError("Unknown control kind")
    return True

if __name__=="__main__":
    output=Path(__file__).with_name("catalog.json")
    data=build_catalog()
    output.write_text(json.dumps(data,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(f"Generated {len(data['models'])} inventory entries; production catalog unchanged")
