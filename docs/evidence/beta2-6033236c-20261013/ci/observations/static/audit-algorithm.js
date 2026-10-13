function auditCurrentIntegration(){
 const source=load("ci-source"),d55="022244e6ce845669665e9ffdfefe25bdee154b75",d56="d7450883ddbeba58e44cf217072f325654c16128",d58="068f1547face75b34c1760dd7b8c9d34868a5393",trees=load("static603:trees"),checks=[];
 const ok=(name,condition,details)=>{checks.push({name,result:condition?"PASS":"FAIL",details});if(!condition)throw Error(name);};
 const get=(p,ref=source)=>load(ref===source?"static603:"+p:"static603:donor:"+ref+":"+p).content;
 const paths=Object.keys(trees[source]).filter(p=>p.startsWith("Source/")&&trees[source][p].type==="blob");
 const unchanged55=paths.filter(p=>trees[source][p].sha===trees[d55][p]?.sha),changed55=paths.filter(p=>!unchanged55.includes(p));
 const combined=["Source/FactoryCabVoicing.h","Source/FactoryPresetLevels.h","Source/NiflheimrPresets.h"];
 const changed56=paths.filter(p=>trees[source][p].sha!==trees[d56][p]?.sha);
 ok("102 Source files, with exactly three combined files different from #56",paths.length===102&&JSON.stringify(changed56)===JSON.stringify(combined),{source_files:paths.length,combined:changed56});
 ok("81 Source files remain exact Git blobs of latest #55",unchanged55.length===81,{count:81,files:unchanged55});
 const preserved56=changed55.filter(p=>!combined.includes(p));
 ok("18 remaining public-name Source changes match exact #56",preserved56.length===18&&preserved56.every(p=>trees[source][p].sha===trees[d56][p].sha),{count:preserved56.length,files:preserved56});
 const important=paths.filter(p=>/DSP|Parameters|State|PluginProcessor/.test(p));
 ok("DSP, parameter, serialized-state and PluginProcessor files byte-identical to #55",important.every(p=>trees[source][p].sha===trees[d55][p]?.sha),{count:important.length,files:important});
 for(const p of combined){
  const from56=get(p,d56).replaceAll("Clear Attack","Metal Clank").replaceAll("Clean Low Drive","Heavy Pulse").replaceAll("Clear-Attack","Metal-Clank").replaceAll("Clean-Low-Drive","Heavy-Pulse");
  ok(p+" combines #56 with only the requested label/filename changes",get(p)===from56,{source_blob:trees[source][p].sha,donor_blob:trees[d56][p].sha});
 }
 const stripComments=s=>s.split("\n").map(l=>l.split("//")[0]).join("\n");
 for(const p of combined.slice(0,2))ok(p+" cabinet coefficients and preset levels unchanged from #55",stripComments(get(p))===stripComments(get(p,d55)),{comparison:"line comments removed; remaining content exact"});
 ok("Niflheimr DSP values, stable IDs, state handling and all code unchanged from latest #55",get(combined[2])===get(combined[2],d55).replace("gentle Diamond compression","gentle Optical compression"),{only_presentation_delta:"Diamond compression -> Optical compression"});
 const tuples=[...get(combined[2]).matchAll(/\{"(original\.rig\.niflheimr\.[^"]+)","([^"]+)","[^"]*","([^"]+)",(\d)\}/g)].map((m,i)=>({index:43+i,id:m[1],name:m[2],fileName:m[3],mode:Number(m[4])}));
 const contract=JSON.parse(get("Validation/catalog-count-contract.json"));
 ok("Current name/ID/file/ordinal/mode tuples preserve intended mapping",tuples.length===5&&tuples[0].id==="original.rig.niflheimr.hrimfaxi.v1"&&tuples[0].name==="Metal Clank"&&tuples[0].fileName==="Niflheimr-01-Metal-Clank.chimera"&&tuples[2].id==="original.rig.niflheimr.nidavellir.v1"&&tuples[2].name==="Heavy Pulse"&&tuples[2].fileName==="Niflheimr-03-Heavy-Pulse.chimera"&&JSON.stringify(tuples.map(x=>x.mode))==="[0,1,2,2,0]",{tuples});
 ok("48 selectable presets, Niflheimr indices43-47 and all current names align with contract",contract.selectable_preset_count===48&&JSON.stringify(contract.niflheimr_original_preset_ordinals)===JSON.stringify(tuples.map(({index,name})=>({index,name}))),{selectable:contract.selectable_preset_count,model_counts:contract.model_counts,serialized_model_counts:contract.serialized_model_counts});
 const c55=JSON.parse(get("Validation/catalog-count-contract.json",d55));c55.factory_ordinals=c55.factory_ordinals.map(x=>({"Orange Heavy":"Amber Heavy","Matchless Edge":"Prism Edge","Dumble Smooth Lead":"Silk Smooth Lead"})[x]||x);
 ok("Catalog contract only public-name aliases differ from latest #55",JSON.stringify(contract)===JSON.stringify(c55),{aliases:{"Orange Heavy":"Amber Heavy","Matchless Edge":"Prism Edge","Dumble Smooth Lead":"Silk Smooth Lead"}});
 const checker56=get("Tools/check_catalog_contract.py",d56).replaceAll("Clear Attack","Metal Clank").replaceAll("Clean Low Drive","Heavy Pulse");
 ok("Checker preserves #56 public-presentation enforcement and latest #55 preset names",get("Tools/check_catalog_contract.py")===checker56,{public_presentation_hook:get("Tools/check_catalog_contract.py").includes("presentation = validate_public_presentation(ROOT)")});
 const delta58=Object.keys(trees[d58]).filter(p=>trees[d58][p].type==="blob"&&trees[d58][p].sha!==trees[d56][p]?.sha&&!p.startsWith("docs/"));
 ok("Every20 non-document #58 addition/change is present as the exact donor blob",delta58.length===20&&delta58.every(p=>trees[source][p]?.sha===trees[d58][p].sha),{count:delta58.length,files:delta58});
 ok("Entire Installer tree and workflow tree equal #58 donor",trees[source].Installer.sha===trees[d58].Installer.sha&&trees[source][".github"].sha===trees[d58][".github"].sha,{Installer:trees[source].Installer.sha,workflows:trees[source][".github"].sha});
 const policy=["Validation/release-policy.json","Validation/beta-publication-policy.json","Validation/beta-publication-policy-1.3.1.json","Validation/weekly-publication-policy.json","Validation/ci-check-map.json","Tools/consolidate_release_gate.py","Tools/evaluate_release_gate.py","Tools/beta_publication131.py","Tools/weekly_publication.py","Tools/verify_publication_evidence.py"];
 ok("Selected release approval, gate, map and weekly policies unchanged from latest #55",policy.every(p=>trees[source][p]?.sha===trees[d55][p]?.sha),{files:policy});
 ok("Publisher source equals exact #56/#58 donor",trees[source]["Tools/publish_release.py"].sha===trees[d56]["Tools/publish_release.py"].sha&&trees[source]["Tools/publish_release.py"].sha===trees[d58]["Tools/publish_release.py"].sha,{blob:trees[source]["Tools/publish_release.py"].sha,reason:"#56 adds distribution public-name guard; no integration change"});
 const log=load("ci-memory-pending").find(x=>x.file==="job-115140821449.log").content;
 const binding=load("ci-memory-bindings").find(x=>x.job_id===115140821449);
 ok("Fresh603 validation-contract actual checkout and catalog/public-document receipts",binding.checkout_verified&&log.includes("PASS: five Náströnd + five Niflheimr Original full rigs; 48 selectable presets, previous 43 indices preserved")&&log.includes("PASS: 13 public documents use product names; runtime labels are checked by native tests"),{job_id:115140821449,run_id:38350264755,binding:binding.evidence,receipt_lines:log.split("\n").filter(l=>/PASS: Chimera catalog contract|PASS: five Náströnd|PASS: 13 public documents/.test(l))});
 return{schema:"chimera.fresh-static-source-audit.v1",observed_at:new Date().toISOString(),source_sha:source,source_tree:load("ci-source-tree"),donors:{latest55:d55,public_names56:d56,cleanup58:d58},scope:"Fresh GitHub object/content comparison evaluated in JavaScript, plus original exact603 CI log receipts. Not a local Python/C++ rerun; no earlier-source test passes reused.",counts:{checks:checks.length,pass:checks.filter(c=>c.result==="PASS").length},checks,source_file_bindings:paths.map(path=>({path,current:trees[source][path].sha,latest55:trees[d55][path]?.sha,donor56:trees[d56][path]?.sha,donor58:trees[d58][path]?.sha}))};
}
