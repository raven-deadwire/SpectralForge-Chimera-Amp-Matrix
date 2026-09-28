/* Preparation state only. No audio, no VST parameters, no network calls. */
(function(root) {
  'use strict';
  const MAX = 5, MAX_ID = Number.MAX_SAFE_INTEGER;
  const clone = x => JSON.parse(JSON.stringify(x));
  const assert = (ok, text) => { if (!ok) throw new Error(text); };
  const record = x => x && typeof x === 'object' && !Array.isArray(x);
  const textOK = s => typeof s === 'string' && s.length > 0 && new TextEncoder().encode(s).length <= 128 && !/[\x00-\x1f]/.test(s);
  const own = (o,k) => Object.prototype.hasOwnProperty.call(o,k);
  function validate(s) {
    assert(record(s) && s.version === 1, '지원하지 않는 상태 형식입니다.');
    assert(Array.isArray(s.effects) && s.effects.length <= MAX, '페달은 최대 5개입니다.');
    assert(Number.isInteger(s.lowTap) && s.lowTap >= 0 && s.lowTap <= s.effects.length, 'LOW 분기 위치가 잘못됐습니다.');
    assert(Number.isSafeInteger(s.nextId) && s.nextId > 0 && s.nextId <= MAX_ID && textOK(s.engine), '식별자 또는 엔진 버전 오류');
    const ids = new Set();
    for (const e of s.effects) {
      assert(record(e) && Number.isSafeInteger(e.id) && e.id > 0 && e.id < s.nextId && !ids.has(e.id), '중복 또는 잘못된 페달 ID'); ids.add(e.id);
      assert(textOK(e.model) && typeof e.bypass === 'boolean' && record(e.parameters) && record(e.midi), '페달 상태 오류');
      assert(Object.keys(e.parameters).length <= 128 && Object.keys(e.midi).length <= 128, '제어 항목 한도 초과');
      for (const [k,v] of Object.entries(e.parameters)) assert(textOK(k) && typeof v === 'number' && Number.isFinite(v), '제어값 오류');
      for (const [cc,key] of Object.entries(e.midi)) assert(/^(0|[1-9][0-9]*)$/.test(cc) && Number(cc)<=127 && own(e.parameters,key), 'MIDI 연결 오류');
    }
    return true;
  }
  function encode(s) {
    validate(s);
    const lines = [`CHIMERA_BOARD_PREP ${s.version}`, `${s.nextId} ${s.lowTap} ${JSON.stringify(s.engine)} ${s.effects.length}`];
    for (const e of s.effects) {
      const params = Object.entries(e.parameters).sort(([a],[b])=>a<b?-1:a>b?1:0);
      const midi = Object.entries(e.midi).sort(([a],[b])=>Number(a)-Number(b));
      lines.push(`${e.id} ${JSON.stringify(e.model)} ${Number(e.bypass)} ${params.length} ${midi.length}`);
      for (const [k,v] of params) lines.push(`${JSON.stringify(k)} ${v}`);
      for (const [cc,k] of midi) lines.push(`${cc} ${JSON.stringify(k)}`);
    }
    return lines.join('\n')+'\n';
  }
  function decode(data) {
    assert(typeof data === 'string' && new TextEncoder().encode(data).length <= 262144, '파일 크기 한도 초과');
    const tokens = []; const rx = /"(?:\\["\\]|[^"\\\x00-\x1f])*"|[^\s"]+/gu;
    let end=0;
    for (const match of data.matchAll(rx)) {
      assert(/^\s*$/.test(data.slice(end,match.index)), '문자열 인코딩 오류');
      tokens.push(match[0]); end=match.index+match[0].length;
    }
    assert(/^\s*$/.test(data.slice(end)), '잘못된 파일 끝');
    let cursor=0;
    const take=()=>{assert(cursor<tokens.length,'파일이 잘렸습니다.');return tokens[cursor++];};
    const number=()=>{const token=take();assert(/^[+-]?(?:\d+\.?\d*|\.\d+)(?:e[+-]?\d+)?$/i.test(token),'숫자 형식 오류');const n=Number(token);assert(Number.isFinite(n),'유한하지 않은 값');return n;};
    const integer=(max)=>{const n=number();assert(Number.isSafeInteger(n)&&n>=0&&n<=max,'정수 범위 오류');return n;};
    const quoted=()=>{const t=take();assert(t.startsWith('"'),'인용 문자열 필요');return JSON.parse(t);};
    assert(take()==='CHIMERA_BOARD_PREP','준비용 .cbp 파일만 열 수 있습니다.');
    const s={version:integer(1),nextId:integer(MAX_ID),lowTap:integer(MAX),engine:quoted(),effects:[]};
    const count=integer(MAX);
    for(let i=0;i<count;i++) {
      const e={id:integer(MAX_ID),model:quoted(),bypass:integer(1)===1,parameters:Object.create(null),midi:Object.create(null)};
      const pc=integer(128), mc=integer(128);
      for(let p=0;p<pc;p++) {const k=quoted();assert(!own(e.parameters,k),'중복 제어 ID');e.parameters[k]=number();}
      for(let p=0;p<mc;p++) {const cc=integer(127);assert(!own(e.midi,cc),'중복 CC');e.midi[cc]=quoted();}
      s.effects.push(e);
    }
    assert(cursor===tokens.length,'파일 뒤에 추가 데이터가 있습니다.'); validate(s); return s;
  }
  class Board {
    constructor() {this._state={version:1,nextId:1,lowTap:0,engine:'legacy-v1',effects:[]};this.high=1;this.past=[];this.future=[];}
    get state() {return clone(this._state);}
    index(id) {const i=this._state.effects.findIndex(e=>e.id===id);assert(i>=0,'해당 페달이 없습니다.');return i;}
    commit(next) {
      validate(next); if(JSON.stringify(next)===JSON.stringify(this._state))return;
      this.past.push(this.state);if(this.past.length>64)this.past.shift();this.future=[];
      this.high=Math.max(this.high,next.nextId);next.nextId=this.high;this._state=clone(next);
    }
    insert(model,position,parameters={},shared=false,bypass=false) {
      const s=this.state;
      assert(s.effects.length<MAX,'최대 5개입니다. 기존 페달을 교체하거나 삭제하세요.');
      assert(Number.isInteger(position)&&position>=0&&position<=s.effects.length,'위치 오류');
      assert(shared?position<=s.lowTap:position>=s.lowTap,'LOW 분기 범위 오류');
      assert(this.high<MAX_ID,'식별자 한도 초과');
      const id=this.high;s.nextId=id+1;
      s.effects.splice(position,0,{id,model,bypass,parameters,midi:{}});if(shared)s.lowTap++;
      this.commit(s);return id;
    }
    add(model,parameters={},shared=false) {return this.insert(model,shared?this._state.lowTap:this._state.effects.length,parameters,shared);}
    remove(id) {const i=this.index(id),s=this.state;s.effects.splice(i,1);if(i<s.lowTap)s.lowTap--;this.commit(s);}
    replace(id,model,parameters={}) {
      const i=this.index(id),s=this.state;assert(this.high<MAX_ID,'식별자 한도 초과');const fresh=this.high;
      s.nextId=fresh+1;s.effects[i]={id:fresh,model,bypass:false,parameters,midi:{}};this.commit(s);return fresh;
    }
    duplicate(id) {const i=this.index(id),e=this._state.effects[i];return this.insert(e.model,i+1,clone(e.parameters),i<this._state.lowTap,e.bypass);}
    move(id,destination) {
      const i=this.index(id),s=this.state;assert(Number.isInteger(destination)&&destination>=0&&destination<s.effects.length,'이동 위치 오류');
      const before=new Set(s.effects.slice(0,s.lowTap).map(e=>e.id));const [e]=s.effects.splice(i,1);s.effects.splice(destination,0,e);
      const changed=s.effects.slice(0,s.lowTap).some(x=>!before.has(x.id));this.commit(s);return changed;
    }
    setTap(n) {const s=this.state;s.lowTap=n;this.commit(s);}
    bypass(id,value) {const s=this.state;s.effects[this.index(id)].bypass=value;this.commit(s);}
    set(id,key,value) {const s=this.state,p=s.effects[this.index(id)].parameters;assert(own(p,key),'알 수 없는 제어');p[key]=value;this.commit(s);}
    bind(id,cc,key) {const s=this.state;s.effects[this.index(id)].midi[cc]=key;this.commit(s);}
    load(data) {const s=decode(data);s.nextId=Math.max(s.nextId,this.high);this.commit(s);}
    undo() {if(!this.past.length)return false;this.future.push(this.state);this._state=this.past.pop();this._state.nextId=Math.max(this._state.nextId,this.high);return true;}
    redo() {if(!this.future.length)return false;this.past.push(this.state);this._state=this.future.pop();this._state.nextId=Math.max(this._state.nextId,this.high);return true;}
  }
  const api={Board,encode,decode,validate,MAX};
  if(typeof module!=='undefined'&&module.exports)module.exports=api;root.ChimeraPrep=api;
})(globalThis);
