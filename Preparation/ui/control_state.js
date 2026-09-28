/* Descriptor-aware state handling. Legacy numbers are preserved, never relabelled
   or converted into guessed physical knob positions. No audio binding here. */
(function(root){
'use strict';
function descriptors(model){return [...(model.legacy_controls||[]),...model.controls];}
function defaults(model){return Object.fromEntries(descriptors(model).map(c=>[c.id,c.initial]));}
function validValue(c,v){
  if(typeof v!=='number'||!Number.isFinite(v))return false;
  if(c.kind==='knob')return v>=c.minimum&&v<=c.maximum;
  return Number.isInteger(v)&&v>=0&&v<=(c.kind==='choice'?c.options.length-1:1);
}
function prepareState(state,models){
  const copy=JSON.parse(JSON.stringify(state));let migrated=0;
  for(const e of copy.effects){
    const m=models.get(e.model);
    if(!m||m.location!=='pre'||!m.controls.length)throw new Error('이 시제품에서 편집할 수 없는 모델입니다.');
    const all=descriptors(m),legacy=m.legacy_controls||[],keys=Object.keys(e.parameters);
    const matches=cs=>keys.length===cs.length&&cs.every(c=>Object.hasOwn(e.parameters,c.id));
    const old=legacy.length>0&&matches(legacy),accepted=matches(all)?all:old?legacy:null;
    if(!accepted)throw new Error('모델별 제어 목록이 다릅니다.');
    for(const c of accepted)if(!validValue(c,e.parameters[c.id]))throw new Error('모델 제어 범위를 벗어난 파일입니다.');
    if(old){e.parameters={...defaults(m),...e.parameters};migrated++;}
    for(const key of Object.values(e.midi||{}))if(!Object.hasOwn(e.parameters,key))throw new Error('존재하지 않는 MIDI 대상입니다.');
  }
  return {state:copy,migrated};
}
function displayLabel(c,params){
  const x=c.mode_labels;
  return x&&Object.hasOwn(x.labels,String(params[x.selector]))?x.labels[String(params[x.selector])]:c.label;
}
const api={descriptors,defaults,prepareState,validValue,displayLabel};
if(typeof module!=='undefined'&&module.exports)module.exports=api;else root.ChimeraControls=api;
})(typeof globalThis!=='undefined'?globalThis:this);
