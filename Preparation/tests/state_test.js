'use strict';
const assert=require('node:assert/strict');
const fs=require('node:fs');
const {Board,encode,decode,validate}=require('../ui/state.js');
let count=0;
const test=(name,fn)=>{fn();++count;console.log('PASS '+name);};
test('hard cap includes bypassed slots',()=>{const b=new Board();for(let i=0;i<5;i++)b.add('sd1',{drive:.3});b.bypass(1,true);const before=encode(b.state);assert.throws(()=>b.add('sixth'));assert.throws(()=>b.duplicate(1));assert.equal(encode(b.state),before);});
test('independent duplicate parameters and no shared MIDI',()=>{const b=new Board(),a=b.add('sd1',{tone:.2});b.bind(a,74,'tone');const c=b.duplicate(a);b.set(c,'tone',.8);assert.equal(b.state.effects[0].parameters.tone,.2);assert.deepEqual(b.state.effects[1].midi,{});});
test('move carries state and bindings',()=>{const b=new Board(),a=b.add('sd1',{tone:.2});b.bind(a,74,'tone');b.add('eq');b.move(a,1);assert.equal(b.state.effects[1].midi[74],'tone');});
test('replacement invalidates identity',()=>{const b=new Board(),a=b.add('sd1',{tone:.2});const c=b.replace(a,'m104',{distortion:.7});assert.notEqual(a,c);assert.throws(()=>b.set(a,'tone',.1));});
test('allocator never rewinds after undo',()=>{const b=new Board();b.add('a');const old=b.add('b');b.undo();assert.ok(b.add('c')>old);assert.equal(b.redo(),false);});
test('tap crossing is disclosed',()=>{const b=new Board(),a=b.add('comp',{},true);b.add('fuzz');assert.equal(b.move(a,1),true);assert.equal(b.state.lowTap,1);});
test('shared deletion adjusts tap',()=>{const b=new Board(),a=b.add('comp',{},true);b.add('fuzz');b.remove(a);assert.equal(b.state.lowTap,0);});
test('bad edits are transactional',()=>{const b=new Board(),a=b.add('a',{p:.2}),before=encode(b.state);assert.throws(()=>b.set(a,'p',NaN));assert.throws(()=>b.set(a,'missing',1));assert.throws(()=>b.setTap(5));assert.equal(encode(b.state),before);});
test('MIDI rejects fractional and unknown control',()=>{const b=new Board(),a=b.add('a',{p:.2});assert.throws(()=>b.bind(a,7.5,'p'));assert.throws(()=>b.bind(a,74,'x'));});
test('CBP precision and unicode roundtrip',()=>{const b=new Board();b.add('이름 "quoted"',{p:.12345678912345678});assert.deepEqual(JSON.parse(JSON.stringify(decode(encode(b.state)))),b.state);});
test('invalid schema and trailing data rejected',()=>{const b=new Board();assert.throws(()=>decode('CHIMERA_BOARD_PREP 2\n1 0 "legacy-v1" 0\n'));assert.throws(()=>decode(encode(b.state)+'junk'));});
test('negative count and six instances rejected',()=>{assert.throws(()=>decode('CHIMERA_BOARD_PREP 1\n1 0 "legacy-v1" -1\n'));assert.throws(()=>decode('CHIMERA_BOARD_PREP 1\n8 0 "legacy-v1" 6\n'));});
test('duplicate keys and ids rejected',()=>{assert.throws(()=>decode('CHIMERA_BOARD_PREP 1\n2 0 "legacy-v1" 1\n1 "a" 0 2 0\n"p" 1\n"p" 2\n'));const b=new Board();b.add('a');const s=b.state;s.effects.push(s.effects[0]);assert.throws(()=>validate(s));});
test('bad load does not change current board',()=>{const b=new Board();b.add('a');const before=encode(b.state);assert.throws(()=>b.load('broken'));assert.equal(encode(b.state),before);});
test('undo redo restores bypass and bindings',()=>{const b=new Board(),a=b.add('a',{p:.2});b.bind(a,74,'p');b.bypass(a,true);b.remove(a);b.undo();assert.equal(b.state.effects[0].bypass,true);assert.equal(b.state.effects[0].midi[74],'p');b.redo();assert.equal(b.state.effects.length,0);});
test('JB2 is one slot',()=>{const b=new Board();b.add('jb2',{'boss.drive':.2,'jhs.drive':.8,mode:5});assert.equal(b.state.effects.length,1);});
test('same-session load preserves high-water identity',()=>{const b=new Board();b.add('a');const before=encode(b.state);const old=b.add('b');b.load(before);assert.ok(b.add('c')>old);});
test('duplicate bypass shares one undo transaction',()=>{const b=new Board(),a=b.add('a');b.bypass(a,true);b.duplicate(a);assert.equal(b.state.effects[1].bypass,true);b.undo();assert.equal(b.state.effects.length,1);});
test('state getter cannot mutate board',()=>{const b=new Board();b.add('a',{p:.2});const copy=b.state;copy.effects[0].parameters.p=.9;assert.equal(b.state.effects[0].parameters.p,.2);});
test('snapshot size bounded',()=>assert.throws(()=>decode(' '.repeat(262145))));
// A deterministic property run also exercises five-slot capacity across 1000 operations.
test('1000 deterministic operations preserve invariants',()=>{
 let seed=17;const next=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed;};const b=new Board();
 for(let i=0;i<1000;i++){const op=next()%8,s=b.state;try{
  if(op===0)b.add('sd1',{drive:.3});else if(op===1&&s.effects.length)b.duplicate(s.effects[0].id);
  else if(op===2&&s.effects.length)b.remove(s.effects[next()%s.effects.length].id);
  else if(op===3&&s.effects.length)b.move(s.effects[0].id,next()%s.effects.length);
  else if(op===4)b.undo();else if(op===5)b.redo();else if(op===6)b.setTap(next()%(s.effects.length+1));
  else if(op===7&&s.effects.length)b.replace(s.effects[0].id,'m104',{distortion:.7});
 }catch(e){assert.match(e.message,/최대 5개/);}validate(b.state);decode(encode(b.state));}
});
if(process.argv[2])test('native C++ exported CBP accepted by JS',()=>{
 const input=decode(fs.readFileSync(process.argv[2],'utf8'));assert.equal(input.effects[0].model,'planned.sd1');assert.equal(input.effects[0].midi[74],'tone');
 const b=new Board();b.load(encode(input));b.move(input.effects[0].id,1);b.bind(input.effects[0].id,17,'drive');
 fs.writeFileSync(process.argv[3],encode(b.state));
});
console.log(`${count} JavaScript preparation cases passed`);
