/* R1a: silent rack state. This is not APVTS, .chimera, DSP, or MIDI I/O. */
(function(root){
  'use strict';
  const SECTIONS=Object.freeze({bus:'BUS COMP',preamp:'PREAMP',eq:'EQUALIZER'});
  const clone=v=>JSON.parse(JSON.stringify(v));
  const object=v=>v!==null&&typeof v==='object'&&!Array.isArray(v);
  const keys=(v,ks)=>object(v)&&Object.keys(v).length===ks.length&&ks.every(k=>Object.hasOwn(v,k));
  function parseStrict(text){
    if(typeof text!=='string'||text.length>262144)throw new Error('랙 파일 크기 제한');
    let i=0;
    const ws=()=>{while(i<text.length&&/[ \t\r\n]/.test(text[i]))i++;};
    const str=()=>{const start=i++;while(i<text.length){const c=text[i++];if(c==='"')return JSON.parse(text.slice(start,i));if(c==='\\')i++;}throw new Error('종료되지 않은 문자열');};
    function value(depth){
      if(depth>32)throw new Error('랙 파일 깊이 제한');ws();
      if(text[i]==='{'){
        i++;ws();const out=Object.create(null),seen=new Set();if(text[i]==='}'){i++;return out;}
        while(i<text.length){ws();if(text[i]!=='"')throw new Error('잘못된 JSON 키');const k=str();
          if(seen.has(k)||['__proto__','constructor','prototype'].includes(k))throw new Error('중복되거나 금지된 JSON 키');seen.add(k);ws();
          if(text[i++]!==':')throw new Error('잘못된 JSON 구분자');out[k]=value(depth+1);ws();const c=text[i++];
          if(c==='}')return out;if(c!==',')throw new Error('잘못된 JSON 객체');}
      }else if(text[i]==='['){
        i++;ws();const out=[];if(text[i]===']'){i++;return out;}
        while(i<text.length){out.push(value(depth+1));ws();const c=text[i++];if(c===']')return out;if(c!==',')throw new Error('잘못된 JSON 배열');}
      }else if(text[i]==='"')return str();
      else {const m=text.slice(i).match(/^(?:true|false|null|-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?)/);
        if(!m)throw new Error('잘못된 JSON 값');i+=m[0].length;const v=JSON.parse(m[0]);if(typeof v==='number'&&!Number.isFinite(v))throw new Error('비유한 JSON 값');return v;}
      throw new Error('불완전한 JSON');
    }
    const result=value(0);ws();if(i!==text.length)throw new Error('추가 JSON 데이터');return result;
  }
  function modelMap(catalog){return new Map(catalog.models.filter(m=>m.location==='post'&&m.rack_panel).map(m=>[m.id,m]));}
  function defaults(m){return Object.fromEntries(m.controls.map(c=>[c.id,c.initial]));}
  function validControl(c,v){
    if(typeof v!=='number'||!Number.isFinite(v))return false;
    if(c.kind==='choice')return Number.isInteger(v)&&v>=0&&v<c.options.length;
    if(c.kind==='toggle')return v===0||v===1;
    return c.kind==='knob'&&v>=c.minimum&&v<=c.maximum;
  }
  function validate(s,models){
    if(!keys(s,['schema','version','selected','sections'])||s.schema!=='chimera.rack-lab'||s.version!==1||!Object.hasOwn(SECTIONS,s.selected)||!keys(s.sections,Object.keys(SECTIONS)))throw new Error('지원하지 않는 랙 상태');
    for(const [name,category] of Object.entries(SECTIONS)){
      const section=s.sections[name];
      if(!keys(section,['model','bypassed','bank','utility'])||typeof section.bypassed!=='boolean'||!object(section.bank)||!Object.hasOwn(section.bank,section.model)||!keys(section.utility,['inputTrim','outputLevel']))throw new Error('잘못된 랙 구획');
      for(const v of Object.values(section.utility))if(typeof v!=='number'||!Number.isFinite(v)||v < -24||v > 24)throw new Error('잘못된 소프트웨어 레벨');
      const available=[...models.values()].filter(m=>m.category===category);
      if(Object.keys(section.bank).length>available.length)throw new Error('랙 저장 뱅크 제한');
      for(const [id,params] of Object.entries(section.bank)){
        const m=models.get(id);if(!m||m.category!==category||!keys(params,m.controls.map(c=>c.id)))throw new Error('잘못된 랙 모델 또는 컨트롤');
        for(const c of m.controls)if(!validControl(c,params[c.id]))throw new Error('잘못된 랙 컨트롤 값: '+c.id);
      }
    }
    return true;
  }
  class RackRig{
    #state;#models;#past=[];#future=[];
    constructor(catalog){
      this.#models=modelMap(catalog);this.#state={schema:'chimera.rack-lab',version:1,selected:'bus',sections:{}};
      for(const [section,category] of Object.entries(SECTIONS)){
        const m=[...this.#models.values()].find(m=>m.category===category);if(!m)throw new Error('랙 카탈로그 누락');
        this.#state.sections[section]={model:m.id,bypassed:false,bank:{[m.id]:defaults(m)},utility:{inputTrim:0,outputLevel:0}};
      }
      validate(this.#state,this.#models);
    }
    get state(){return clone(this.#state);}
    get canUndo(){return this.#past.length>0;}
    get canRedo(){return this.#future.length>0;}
    current(){const v=this.#state.sections[this.#state.selected];return {section:this.#state.selected,model:v.model,bypassed:v.bypassed,parameters:clone(v.bank[v.model]),utility:clone(v.utility)};}
    #edit(fn){const next=clone(this.#state);fn(next);validate(next,this.#models);if(JSON.stringify(next)===JSON.stringify(this.#state))return;this.#past.push(this.#state);if(this.#past.length>64)this.#past.shift();this.#future=[];this.#state=next;}
    select(section){this.#edit(s=>{s.selected=section;});}
    choose(id){const m=this.#models.get(id);if(!m||m.category!==SECTIONS[this.#state.selected])throw new Error('이 구획의 랙 모델이 아닙니다.');this.#edit(s=>{const r=s.sections[s.selected];if(!Object.hasOwn(r.bank,id))r.bank[id]=defaults(m);r.model=id;});}
    set(key,v){this.#edit(s=>{const r=s.sections[s.selected];r.bank[r.model][key]=v;});}
    bypass(v){this.#edit(s=>{s.sections[s.selected].bypassed=v;});}
    trim(key,v){this.#edit(s=>{s.sections[s.selected].utility[key]=v;});}
    undo(){if(!this.canUndo)return false;this.#future.push(this.#state);this.#state=this.#past.pop();return true;}
    redo(){if(!this.canRedo)return false;this.#past.push(this.#state);this.#state=this.#future.pop();return true;}
    export(){return JSON.stringify(this.#state,null,2)+'\n';}
    load(text){const s=parseStrict(text);validate(s,this.#models);this.#edit(v=>{for(const k of Object.keys(v))delete v[k];Object.assign(v,s);});}
  }
  const api={SECTIONS,RackRig,parseStrict,modelMap,defaults,validate,validControl};
  if(typeof module!=='undefined'&&module.exports)module.exports=api;root.ChimeraRacks=api;
})(typeof globalThis!=='undefined'?globalThis:this);
