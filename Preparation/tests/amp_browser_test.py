"""Silent amplifier UI tests. No external downloads or audio-quality claims."""
from pathlib import Path
import argparse
import json
from playwright.sync_api import sync_playwright

ROOT=Path(__file__).resolve().parents[1]

def run(executable):
    html=(ROOT/'Chimera_FiveSlot_Preview.html').read_text(encoding='utf-8')
    results=[];all_errors=[]
    with sync_playwright() as p:
        browser=p.chromium.launch(executable_path=executable,headless=True,args=['--no-sandbox'])
        context=browser.new_context(viewport={'width':1280,'height':900},accept_downloads=True)
        page=context.new_page()
        def reset():
            nonlocal page
            page.close();page=context.new_page()
            page.on('pageerror',lambda e:all_errors.append(str(e)))
            page.set_content(html,wait_until='load');page.locator('#ampTab').click()
        def test(name,fn):
            reset();fn();assert not all_errors,all_errors
            results.append({'name':name,'passed':True})
        def choose(mid):page.locator('#ampModel').select_option(mid)
        def set_control(key,value):
            loc=page.locator('[data-amp-control="'+key+'"]')
            loc.evaluate('(e,v)=>{e.value=String(v);e.dispatchEvent(new Event("input"));e.dispatchEvent(new Event("change"));}',value)
        def current():return page.evaluate('ampRig.current()')
        def tabs():
            assert page.locator('#ampLab').is_visible();assert page.locator('#pedalLab').is_hidden()
            page.locator('#pedalTab').click();assert page.locator('.pedal').count()==5;assert page.locator('#ampLab').is_hidden()
        test('separate AMP tab preserves compact five-slot pedal board',tabs)
        def inventory():
            assert page.locator('#ampModel option').count()==24
            assert page.locator('#ampModel option:not(:disabled)').count()==23
            assert page.locator('#ampModel option[value="planned.amp.engl"]').evaluate("e => e.disabled && e.matches(':disabled')")
            for mid in page.locator('#ampModel option:not(:disabled)').evaluate_all('(es)=>es.map(e=>e.value)'):
                choose(mid);assert page.locator('#ampControls .control').count()>0
            page.evaluate("ampRig.choose('planned.amp.engl');renderAmp()")
            assert current()['model']=='planned.amp.engl'
            assert page.locator('#ampControls .control').count()>0
        test('23 active targets render and legacy Ironball still recalls',inventory)
        def jtm():
            choose('legacy.amp.1');labels=page.locator('#ampControls label').all_text_contents()
            assert len(labels)==6;assert not any(x=='MASTER' for x in labels)
        test('JTM45 two input loudness controls with no fictitious master',jtm)
        def twin():
            choose('legacy.amp.0');assert page.locator('[data-amp-control="hw.vibrato.reverb"]').count()==0
            page.locator('#ampChannel').select_option('vibrato');assert page.locator('[data-amp-control="hw.vibrato.reverb"]').count()==1
        test('Twin changes channel-specific controls including onboard FX',twin)
        def p6505():
            choose('legacy.amp.2');set_control('hw.mid',.17);set_control('hw.lead.pre_gain',.81)
            page.locator('#ampChannel').select_option('rhythm');assert page.locator('[data-amp-control="hw.mid"]').input_value()=='0.17'
            assert page.locator('[data-amp-control="hw.lead.pre_gain"]').count()==0
            page.locator('#ampChannel').select_option('lead');assert current()['parameters']['hw.lead.pre_gain']==.81
        test('6505 shared EQ remains shared while independent gains persist',p6505)
        def mark():
            choose('legacy.amp.4');assert page.locator('[data-amp-control^="hw.geq."]').count()==6
            assert page.locator('[data-amp-control="hw.lead.drive"]').count()==1
            page.locator('#ampChannel').select_option('r1');set_control('hw.rhythm.bass',.22)
            page.locator('#ampChannel').select_option('r2');assert current()['parameters']['hw.rhythm.bass']==.22
        test('Mark IV graphic EQ and rhythm-shared controls stay independent of Lead Drive',mark)
        def vrcl():
            choose('legacy.amp.5');assert page.locator('[data-amp-control="hw.ch1.mid_frequency"] option').count()==3
            page.locator('#ampChannel').select_option('ch2');assert not any('MID' in t for t in page.locator('#ampControls label').all_text_contents())
            choose('planned.amp.svt-cl');assert page.locator('[data-amp-control="hw.mid_frequency"] option').count()==5
        test('SVT-VR and SVT-CL display genuinely different control sets',vrcl)
        def vox():
            choose('legacy.amp.8');assert page.locator('[data-amp-control="hw.tone_cut"]').count()==1
            page.locator('#ampChannel').select_option('normal');assert page.locator('[data-amp-control^="hw.top_boost."]').count()==0
        test('AC30CH Normal and Top Boost have different tone controls',vox)
        def match():
            choose('legacy.amp.12');assert page.locator('[data-amp-control="hw.ch2.tone"] option').count()==6
            page.locator('#ampChannel').select_option('ch1');assert page.locator('[data-amp-control="hw.ch1.bass"]').count()==1
            assert page.locator('[data-amp-control="hw.ch2.tone"]').count()==0
        test('Matchless EF86 rotary is not replaced by universal EQ',match)
        def eich():
            choose('legacy.amp.14');assert page.locator('[data-amp-control="hw.taste"]').count()==1
            assert '핵심' in page.locator('#ampReview').inner_text()
        test('EICH Taste has its own control',eich)
        def zuta():
            choose('planned.amp.zuta-gbg120');set_control('ch1.gain',.16)
            assert page.locator('[data-amp-control="ch1.low_cut"]').count()==1
            page.locator('#ampChannel').select_option('ch4');set_control('ch4.gain',.88)
            assert page.locator('[data-amp-control="ch4.tight"]').count()==1
            assert page.locator('[data-amp-control="ch1.low_cut"]').count()==0
            page.locator('#ampChannel').select_option('ch1');assert current()['parameters']['ch1.gain']==.16
            assert current()['parameters']['ch4.gain']==.88
        test('ZUTA four channels retain values and channel-specific switches',zuta)
        def fortin():
            choose('planned.amp.fortin');assert page.locator('[data-amp-control="hw.ep.girth"]').count()==1
            page.locator('#ampChannel').select_option('kk');assert page.locator('[data-amp-control="hw.kk.gain2"]').count()==1
            assert page.locator('[data-amp-control="hw.gain_eq.sweep"]').count()==1
            page.locator('#ampChannel').select_option('clean');assert page.locator('[data-amp-control="hw.gain_eq.sweep"]').count()==0
            assert page.locator('[data-amp-control="hw.clean.volume"]').count()==1
        test('Fortin EP KK Clean do not reuse one generic panel',fortin)
        def amp_memory():
            choose('legacy.amp.1');set_control('hw.presence',.19);choose('legacy.amp.5');choose('legacy.amp.1')
            assert current()['parameters']['hw.presence']==.19
        test('model switching retains independent stored settings',amp_memory)
        def lanes():
            page.locator('#ampMode').select_option('Dual');set_control('hw.normal.volume',.24)
            page.locator('#ampLane').select_option('b');set_control('hw.normal.volume',.82)
            page.locator('#ampLane').select_option('a');assert current()['parameters']['hw.normal.volume']==.24
        test('Dual same model on two lanes remains independent',lanes)
        def modes():
            page.locator('#ampMode').select_option('Matrix');page.locator('#ampLane').select_option('high');choose('legacy.amp.1')
            page.locator('#ampMode').select_option('Classic');page.locator('#ampMode').select_option('Matrix')
            assert current()['laneId']=='high' and current()['model']=='legacy.amp.1'
        test('mode switching restores selected lane and its amp',modes)
        def utilities():
            choose('planned.amp.svt-cl');set_control('hw.master',.64)
            page.locator('#ampLaneLevel').fill('-7');page.locator('#ampLaneLevel').dispatch_event('change')
            assert current()['parameters']['hw.master']==.64 and current()['utility']['laneLevel']==-7
            choose('legacy.amp.1');assert current()['utility']['laneLevel']==-7
        test('software lane level never acts as an original amp master',utilities)
        def review():
            choose('planned.amp.sunn');assert '대기' in page.locator('#ampReview').inner_text()
            page.locator('.amp-notes summary').click();assert '원형' in page.locator('#ampNote').inner_text()
            assert '검증 완료가 아닙니다' in page.locator('#ampSources').inner_text()
        test('unverified original panels prominently remain pending',review)
        def source():
            choose('legacy.amp.5');page.locator('.amp-notes summary').click()
            assert page.locator('#ampSources a').get_attribute('href').startswith('https://ampeg.com/')
            assert page.locator('#ampSources a').get_attribute('rel')=='noopener noreferrer'
        test('exact source links are explicit user actions not background requests',source)
        def undo():
            choose('legacy.amp.1');set_control('hw.presence',.12);page.locator('#ampUndo').click()
            assert current()['parameters']['hw.presence']==.5
            page.locator('#ampRedo').click();assert current()['parameters']['hw.presence']==.12
        test('amp Undo and Redo restore native values',undo)
        def pedal_independent():
            before=page.evaluate('board.state');choose('legacy.amp.4');set_control('hw.lead.gain',.77)
            assert page.evaluate('board.state')==before
            page.locator('#pedalTab').click();assert page.locator('#add').is_disabled()
        test('amplifiers never consume or mutate the five pedal slots',pedal_independent)
        def roundtrip():
            choose('planned.amp.zuta-gbg120');page.locator('#ampChannel').select_option('ch3');set_control('ch3.gain',.81)
            with page.expect_download() as download:page.locator('#ampSave').click()
            d=download.value;assert d.suggested_filename.endswith('.calab');data=Path(d.path()).read_bytes()
            choose('legacy.amp.0');page.locator('#ampFile').set_input_files({'name':'roundtrip.calab','mimeType':'application/json','buffer':data})
            page.wait_for_function("ampRig.current().model==='planned.amp.zuta-gbg120'")
            assert current()['channel']=='ch3' and current()['parameters']['ch3.gain']==.81
        test('real browser export and upload roundtrip retains amplifier state',roundtrip)
        def badfile():
            before=page.evaluate('ampRig.state');page.locator('#ampFile').set_input_files({'name':'bad.calab','mimeType':'application/json','buffer':b'{"version":999}'})
            page.wait_for_function("document.getElementById('ampStatus').textContent.includes('형식')")
            assert page.evaluate('ampRig.state')==before
        test('bad uploaded state rejected without partial replacement',badfile)
        def route():
            choose('legacy.amp.1');page.locator('#ampRoute').select_option('4');assert current()['route']==4
            assert len(current()['parameters'])==6
        test('input routing does not create fictitious hardware knobs',route)
        def responsive():
            choose('planned.amp.zuta-gbg120')
            for width in (1280,1024,900,640):
                page.set_viewport_size({'width':width,'height':900})
                assert page.evaluate('document.documentElement.scrollWidth<=innerWidth+1')
                box=page.locator('#ampControls').bounding_box();assert box['height']<=420
                page.locator('#pedalTab').click();assert page.locator('.pedal').count()==5
                assert page.evaluate('document.documentElement.scrollWidth<=innerWidth+1');page.locator('#ampTab').click()
        test('fixed-height amp controls and five pedals fit 1280 to 640px without horizontal overflow',responsive)
        reset()
        for mid,channel,filename in [('legacy.amp.5','ch1','amp-svt-vr'),('legacy.amp.4','lead','amp-mark-iv'),('planned.amp.zuta-gbg120','ch4','amp-zuta-ch4'),('planned.amp.fortin','ep','amp-fortin'),('legacy.amp.1','single','amp-jtm45')]:
            choose(mid);page.locator('#ampChannel').select_option(channel,force=True) if channel!='single' else None
            page.screenshot(path=str(ROOT/f'reports/{filename}.png'),full_page=True)
        report={'browser':browser.version,'navigation':'set_content; offline','tests':results,'page_errors':all_errors,
                'audio_validation':False,'production_plugin_validation':False}
        (ROOT/'reports/amp-browser-tests.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        browser.close()
    print(f'{len(results)} amplifier browser cases passed')
    return report
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--browser',required=True);run(parser.parse_args().browser)
