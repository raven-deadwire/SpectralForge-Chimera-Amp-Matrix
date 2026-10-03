/* All rendering uses text nodes. No network, audio, MIDI, or production state writes. */
(function(){
  'use strict';
  const $=id=>document.getElementById(id);
  const rig=new ChimeraAmps.AmpRig(catalog),amps=ChimeraAmps.modelMap(catalog);
  window.ampRig=rig;
  const labels={main:'MAIN',a:'LANE A',b:'LANE B',low:'LOW',mid:'MID',high:'HIGH'};
  const status={primary_core_reviewed:'핵심 패널 자료 확인',partial_primary_review:'일부 확인 · 추가 대조 필요',primary_panel_pending:'원본 패널 검증 대기'};
  const message=t=>{$('ampStatus').textContent=t;};
  function act(fn, text){try{fn();render();if(text)message(text);}catch(e){message(e.message);}}
  function formatted(c,v){return c.kind==='choice'?c.options[v]:c.kind==='toggle'?(v?'ON':'OFF'):`${Math.round(v*100)}%`;}
  function control(c,current){
    const cell=document.createElement('div');cell.className='control';cell.dataset.controlId=c.id;
    const label=document.createElement('label');label.textContent=c.label;label.htmlFor='amp-control-'+c.id;cell.append(label);
    let input;
    if(c.kind==='choice'){input=document.createElement('select');c.options.forEach((name,i)=>input.add(new Option(name,String(i))));}
    else if(c.kind==='toggle'){input=document.createElement('select');input.add(new Option('OFF','0'));input.add(new Option('ON','1'));}
    else {const dial=document.createElement('div');dial.className='dial';dial.ariaHidden='true';dial.style.setProperty('--angle',`${-135+270*current.parameters[c.id]}deg`);cell.append(dial);
      input=document.createElement('input');input.type='range';input.min=c.minimum;input.max=c.maximum;input.step=c.step;}
    input.id='amp-control-'+c.id;input.dataset.ampControl=c.id;input.value=current.parameters[c.id];
    const output=document.createElement('div');output.className='value';output.textContent=formatted(c,current.parameters[c.id]);
    input.oninput=()=>{output.textContent=formatted(c,Number(input.value));const dial=cell.querySelector('.dial');if(dial)dial.style.setProperty('--angle',`${-135+270*Number(input.value)}deg`);};
    // State commit is at change, not every pointer pixel, so Undo remains useful.
    input.onchange=()=>{try{rig.set(c.id,Number(input.value));$('ampUndo').disabled=!rig.canUndo;$('ampRedo').disabled=!rig.canRedo;message('조절 위치를 저장했습니다. 실제 음향 처리는 없습니다.');}catch(e){message(e.message);render();}};
    cell.append(input,output);if(c.note){const note=document.createElement('small');note.textContent=c.note;cell.append(note);}return cell;
  }
  function render(){
    const s=rig.state,current=rig.current(),m=amps.get(current.model),p=m.amp_panel;
    $('ampMode').value=s.mode;$('ampLane').replaceChildren();for(const id of ChimeraAmps.MODES[s.mode])$('ampLane').add(new Option(labels[id],id));$('ampLane').value=current.laneId;
    $('ampModel').value=m.id;$('ampChannel').replaceChildren();for(const ch of p.channels)$('ampChannel').add(new Option(ch.label,ch.id));$('ampChannel').value=current.channel;
    $('ampChannel').disabled=p.channels.length===1;
    $('ampRoute').replaceChildren();p.input_routes.forEach((r,i)=>$('ampRoute').add(new Option(r,String(i))));$('ampRoute').value=current.route;$('ampInputLabel').hidden=p.input_routes.length===1;
    $('ampTitle').textContent=m.name;$('ampReference').textContent=p.reference;
    $('ampReview').textContent=status[p.review_status];$('ampControls').replaceChildren();
    const controls=ChimeraAmps.visibleControls(m,current.channel);
    for(const group of [...new Set(controls.map(c=>c.group))]){
      const heading=document.createElement('h3');heading.className='control-group';heading.textContent=group;$('ampControls').append(heading);
      controls.filter(c=>c.group===group).forEach(c=>$('ampControls').append(control(c,current)));
    }
    $('ampInputTrim').value=current.utility.inputTrim;$('ampLaneLevel').value=current.utility.laneLevel;
    $('ampNote').textContent=p.note_ko;$('ampScope').textContent='기준 범위: '+p.reference_scope;$('ampOmissions').textContent='남은 항목: '+p.omissions;
    $('ampSources').replaceChildren();for(const [i,url] of p.sources.entries()){const a=document.createElement('a');a.href=url;a.target='_blank';a.rel='noopener noreferrer';a.textContent=`원문 ${i+1}`;$('ampSources').append(a);}
    if(!p.sources.length)$('ampSources').textContent='확인된 원본 패널 출처 없음 — 대상 정의이며 검증 완료가 아닙니다.';
    $('ampUndo').disabled=!rig.canUndo;$('ampRedo').disabled=!rig.canRedo;
  }
  for(const type of ['existing_dsp','not_implemented']){
    const group=document.createElement('optgroup');group.label=type==='existing_dsp'?'기존 15종 — 패널 재개발':'추가 대상 — DSP 미구현';
    for(const m of amps.values())if(m.status===type){const option=new Option(m.name+' / '+m.amp_panel.reference,m.id);option.disabled=m.active===false;group.append(option);}$('ampModel').append(group);
  }
  $('ampModel').onchange=()=>act(()=>rig.choose($('ampModel').value),'앰프별 이전 설정을 유지합니다.');
  $('ampChannel').onchange=()=>act(()=>rig.channel($('ampChannel').value),'다른 채널의 값은 그대로 보존됩니다.');
  $('ampRoute').onchange=()=>act(()=>rig.route(Number($('ampRoute').value)));
  $('ampMode').onchange=()=>act(()=>rig.setMode($('ampMode').value),'모드별 레인 설정을 보존합니다. 실제 라우팅은 미연결입니다.');
  $('ampLane').onchange=()=>act(()=>rig.setLane($('ampLane').value));
  $('ampUndo').onclick=()=>act(()=>rig.undo());$('ampRedo').onclick=()=>act(()=>rig.redo());
  $('ampInputTrim').onchange=()=>act(()=>rig.trim('inputTrim',Number($('ampInputTrim').value)));
  $('ampLaneLevel').onchange=()=>act(()=>rig.trim('laneLevel',Number($('ampLaneLevel').value)));
  $('ampSave').onclick=()=>{const a=document.createElement('a'),url=URL.createObjectURL(new Blob([rig.export()],{type:'application/json'}));a.href=url;a.download='chimera-amp-lab.calab';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);message('앰프 준비용 파일을 저장했습니다. 페달 .cbp와 제품 .chimera는 변경하지 않습니다.');};
  $('ampLoad').onclick=()=>$('ampFile').click();$('ampFile').onchange=async()=>{const f=$('ampFile').files[0];if(!f)return;try{if(f.size>1048576)throw new Error('파일이 너무 큽니다.');rig.load(await f.text());render();message('앰프·채널·레인별 설정을 복원했습니다.');}catch(e){message(e.message);}finally{$('ampFile').value='';}};
  function show(which){const amp=which==='amp';$('ampLab').hidden=!amp;$('pedalLab').hidden=amp;$('ampTab').ariaSelected=String(amp);$('pedalTab').ariaSelected=String(!amp);if(amp)render();}
  $('ampTab').onclick=()=>show('amp');$('pedalTab').onclick=()=>show('pedal');
  window.renderAmp=render;window.showLab=show;render();
})();
