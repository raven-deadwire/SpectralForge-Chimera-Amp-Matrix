(function(){
  'use strict';
  const $=id=>document.getElementById(id),rig=new ChimeraRacks.RackRig(catalog),models=ChimeraRacks.modelMap(catalog);
  window.rackRig=rig;
  const msg=t=>{$('rackMessage').textContent=t;};
  function act(fn){try{fn();render();msg('설정을 저장했습니다.');}catch(e){msg(e.message);}}
  const format=(c,v)=>c.kind==='choice'?c.options[v]:c.kind==='toggle'?(v?'ON':'OFF'):Math.round(v*100)+'%';
  function control(c,values){
    const cell=document.createElement('div');cell.className='rack-control';
    const label=document.createElement('label');label.textContent=c.label;label.htmlFor='rack-control-'+c.id;
    let input;
    if(c.kind==='knob'){input=document.createElement('input');input.type='range';input.min=c.minimum;input.max=c.maximum;input.step=c.step;}
    else{input=document.createElement('select');(c.kind==='choice'?c.options:['OFF','ON']).forEach((n,i)=>input.add(new Option(n,String(i))));}
    input.id=label.htmlFor;input.dataset.rackControl=c.id;input.value=values[c.id];
    const out=document.createElement('output');out.textContent=format(c,values[c.id]);
    input.oninput=()=>{out.textContent=format(c,Number(input.value));};
    input.onchange=()=>{try{rig.set(c.id,Number(input.value));$('rackUndo').disabled=!rig.canUndo;$('rackRedo').disabled=!rig.canRedo;msg('조절 위치를 저장했습니다.');}catch(e){msg(e.message);render();}};
    if(c.note)cell.title=c.note;cell.append(label,input,out);return cell;
  }
  function render(){
    const s=rig.state,v=rig.current(),m=models.get(v.model),p=m.rack_panel;
    $('rackSection').value=s.selected;$('rackModel').replaceChildren();
    for(const model of models.values())if(model.category===ChimeraRacks.SECTIONS[s.selected])$('rackModel').add(new Option(model.name+' / '+model.rack_panel.reference,model.id));
    $('rackModel').value=v.model;$('rackBypass').checked=v.bypassed;$('rackName').textContent=m.name;$('rackReference').textContent=p.reference;
    $('rackReview').textContent=p.review_status==='primary_panel_reviewed'?'핵심 패널 자료 확인':'일부 확인 · 원본 대조 필요';
    $('rackControls').replaceChildren();
    for(const group of [...new Set(m.controls.map(c=>c.group))]){
      const field=document.createElement('fieldset');field.className='rack-control-group';const legend=document.createElement('legend');legend.textContent=group;
      const grid=document.createElement('div');grid.className='rack-control-grid';m.controls.filter(c=>c.group===group).forEach(c=>grid.append(control(c,v.parameters)));field.append(legend,grid);$('rackControls').append(field);
    }
    $('rackInputTrim').value=v.utility.inputTrim;$('rackOutputLevel').value=v.utility.outputLevel;$('rackNote').textContent=p.note;$('rackOmitted').textContent=p.omitted;
    $('rackSources').replaceChildren();for(const [i,url] of p.sources.entries()){const a=document.createElement('a');a.textContent='원문 '+(i+1);a.href=url;a.target='_blank';a.rel='noopener noreferrer';$('rackSources').append(a);}
    $('rackUndo').disabled=!rig.canUndo;$('rackRedo').disabled=!rig.canRedo;
  }
  $('rackSection').onchange=()=>act(()=>rig.select($('rackSection').value));$('rackModel').onchange=()=>act(()=>rig.choose($('rackModel').value));
  $('rackBypass').onchange=()=>act(()=>rig.bypass($('rackBypass').checked));$('rackUndo').onclick=()=>act(()=>rig.undo());$('rackRedo').onclick=()=>act(()=>rig.redo());
  $('rackInputTrim').onchange=()=>act(()=>rig.trim('inputTrim',Number($('rackInputTrim').value)));
  $('rackOutputLevel').onchange=()=>act(()=>rig.trim('outputLevel',Number($('rackOutputLevel').value)));
  $('rackExport').onclick=()=>{const a=document.createElement('a'),url=URL.createObjectURL(new Blob([rig.export()],{type:'application/json'}));a.href=url;a.download='chimera-rack-lab.crlab';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);msg('랙 준비용 파일을 저장했습니다.');};
  $('rackImport').onchange=async()=>{const f=$('rackImport').files[0];if(!f)return;try{if(f.size>262144)throw new Error('랙 파일 크기 제한');rig.load(await f.text());render();msg('랙 설정을 복원했습니다.');}catch(e){msg(e.message);}finally{$('rackImport').value='';}};
  for(const id of ['pedalTab','ampTab'])$(id).addEventListener('click',()=>{$('rackLab').hidden=true;$('rackTab').ariaSelected='false';});
  $('rackTab').onclick=()=>{$('pedalLab').hidden=true;$('ampLab').hidden=true;$('rackLab').hidden=false;for(const id of ['pedalTab','ampTab'])$(id).ariaSelected='false';$('rackTab').ariaSelected='true';render();};
  window.renderRack=render;render();
})();
