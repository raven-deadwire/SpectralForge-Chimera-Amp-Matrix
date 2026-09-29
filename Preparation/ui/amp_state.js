/* Dedicated silent amp-editor state. Never changes pedal .cbp or production APVTS.
 * Hardware knob positions, channel selection, and Chimera lane trims are distinct.
 */
(function(root) {
  'use strict';
  const MODES = Object.freeze({Classic:['main'], Dual:['a','b'], Matrix:['low','mid','high']});
  const LANE_IDS = Object.freeze(Object.values(MODES).flat());
  const UTILITY = Object.freeze({inputTrim:{min:-24,max:24,initial:0},laneLevel:{min:-24,max:24,initial:0}});
  const clone = value => JSON.parse(JSON.stringify(value));
  const object = value => value !== null && typeof value === 'object' && !Array.isArray(value);
  function keysAre(value, keys) {
    return object(value) && Object.keys(value).length===keys.length && keys.every(k=>Object.hasOwn(value,k));
  }
  function modelMap(catalog) {
    return new Map(catalog.models.filter(m=>m.location==='rig' && m.amp_panel).map(m=>[m.id,m]));
  }
  function validControl(c, value) {
    if(typeof value!=='number'||!Number.isFinite(value))return false;
    if(c.kind==='toggle')return value===0||value===1;
    if(c.kind==='choice')return Number.isInteger(value)&&value>=0&&value<c.options.length;
    return c.kind==='knob'&&value>=c.minimum&&value<=c.maximum;
  }
  function defaults(model) {
    if(!model?.amp_panel)throw new Error('Unknown amplifier');
    return {channel:model.amp_panel.initial_channel,route:0,
      parameters:Object.fromEntries(model.controls.map(c=>[c.id,c.initial]))};
  }
  function validate(value, models) {
    if(!keysAre(value,['schema','version','mode','selected','lanes'])||value.schema!=='chimera.amp-lab'||value.version!==1)
      throw new Error('지원하지 않는 앰프 준비용 파일 형식입니다.');
    if(!Object.hasOwn(MODES,value.mode)||!keysAre(value.selected,Object.keys(MODES))||!keysAre(value.lanes,LANE_IDS))
      throw new Error('잘못된 모드 또는 레인입니다.');
    for(const [mode, ids] of Object.entries(MODES))if(!ids.includes(value.selected[mode]))throw new Error('모드의 레인이 맞지 않습니다.');
    for(const laneId of LANE_IDS) {
      const lane=value.lanes[laneId];
      if(!keysAre(lane,['model','bank','utility'])||!object(lane.bank)||!Object.hasOwn(lane.bank,lane.model)||!keysAre(lane.utility,Object.keys(UTILITY)))
        throw new Error('잘못된 앰프 저장 상태입니다.');
      for(const [key,spec] of Object.entries(UTILITY)) {
        const v=lane.utility[key];
        if(typeof v!=='number'||!Number.isFinite(v)||v<spec.min||v>spec.max)throw new Error('Trim/Level 범위가 잘못됐습니다.');
      }
      if(Object.keys(lane.bank).length>models.size)throw new Error('모델 뱅크가 너무 큽니다.');
      for(const [id, stored] of Object.entries(lane.bank)) {
        const m=models.get(id);
        if(!m||!keysAre(stored,['channel','route','parameters'])||!m.amp_panel.channels.some(c=>c.id===stored.channel))
          throw new Error('알 수 없는 앰프 또는 채널입니다.');
        if(!Number.isInteger(stored.route)||stored.route<0||stored.route>=m.amp_panel.input_routes.length)
          throw new Error('잘못된 입력 경로입니다.');
        if(!keysAre(stored.parameters,m.controls.map(c=>c.id)))throw new Error('누락되거나 알 수 없는 앰프 컨트롤입니다.');
        for(const c of m.controls)if(!validControl(c,stored.parameters[c.id]))throw new Error('잘못된 앰프 값: '+c.id);
      }
    }
    return true;
  }
  function visibleControls(model, channel) {
    return model.controls.filter(c=>!c.channels?.length||c.channels.includes(channel));
  }
  class AmpRig {
    #state; #past=[]; #future=[]; #models;
    constructor(catalog) {
      this.#models=modelMap(catalog);
      const guitar=this.#models.has('legacy.amp.0')?'legacy.amp.0':this.#models.keys().next().value;
      const bass=this.#models.has('legacy.amp.5')?'legacy.amp.5':guitar;
      this.#state={schema:'chimera.amp-lab',version:1,mode:'Classic',
        selected:{Classic:'main',Dual:'a',Matrix:'low'},lanes:{}};
      for(const lane of LANE_IDS){const id=lane==='low'?bass:guitar;
        this.#state.lanes[lane]={model:id,bank:{[id]:defaults(this.#models.get(id))},utility:{inputTrim:0,laneLevel:0}};}
      validate(this.#state,this.#models);
    }
    get state(){return clone(this.#state);}
    get canUndo(){return this.#past.length>0;}
    get canRedo(){return this.#future.length>0;}
    get laneId(){return this.#state.selected[this.#state.mode];}
    current(){const lane=this.#state.lanes[this.laneId];return {laneId:this.laneId,model:lane.model,...clone(lane.bank[lane.model]),utility:clone(lane.utility)};}
    #edit(fn){const next=clone(this.#state);fn(next);validate(next,this.#models);
      if(JSON.stringify(next)===JSON.stringify(this.#state))return;
      this.#past.push(this.#state);if(this.#past.length>64)this.#past.shift();this.#future=[];this.#state=next;}
    #lane(s){return s.lanes[s.selected[s.mode]];}
    setMode(mode){this.#edit(s=>{s.mode=mode;});}
    setLane(id){this.#edit(s=>{s.selected[s.mode]=id;});}
    choose(id){if(!this.#models.has(id))throw new Error('앰프 모델이 아닙니다.');
      this.#edit(s=>{const lane=this.#lane(s);if(!Object.hasOwn(lane.bank,id))lane.bank[id]=defaults(this.#models.get(id));lane.model=id;});}
    channel(id){this.#edit(s=>{const l=this.#lane(s);l.bank[l.model].channel=id;});}
    route(index){this.#edit(s=>{const l=this.#lane(s);l.bank[l.model].route=index;});}
    set(key,value){this.#edit(s=>{const l=this.#lane(s);l.bank[l.model].parameters[key]=value;});}
    trim(key,value){this.#edit(s=>{this.#lane(s).utility[key]=value;});}
    undo(){if(!this.canUndo)return false;this.#future.push(this.#state);this.#state=this.#past.pop();return true;}
    redo(){if(!this.canRedo)return false;this.#past.push(this.#state);this.#state=this.#future.pop();return true;}
    export(){return JSON.stringify(this.#state,null,2)+'\n';}
    load(text){if(typeof text!=='string'||text.length>1048576)throw new Error('앰프 파일이 너무 큽니다.');
      const parsed=JSON.parse(text);validate(parsed,this.#models);this.#edit(s=>{for(const k of Object.keys(s))delete s[k];Object.assign(s,parsed);});}
  }
  const api={AmpRig,MODES,LANE_IDS,UTILITY,modelMap,defaults,validate,visibleControls,validControl};
  if(typeof module!=='undefined'&&module.exports)module.exports=api;
  root.ChimeraAmps=api;
})(typeof globalThis!=='undefined'?globalThis:this);
