"""R1a browser integration. Requires local Chromium; never substitutes static QA."""
from pathlib import Path
import argparse,json
from playwright.sync_api import sync_playwright
ROOT=Path(__file__).resolve().parents[1]

def run(executable):
    results=[];errors=[]
    with sync_playwright() as p:
        browser=p.chromium.launch(executable_path=executable,headless=True,args=['--no-sandbox'])
        context=browser.new_context(viewport={'width':1280,'height':900},accept_downloads=True)
        page=None
        def test(name,fn):
            nonlocal page
            if page:page.close()
            page=context.new_page();page.on('pageerror',lambda e:errors.append(str(e)))
            page.set_content((ROOT/'Chimera_FiveSlot_Preview.html').read_text(),wait_until='load');page.locator('#rackTab').click()
            fn();assert not errors,errors;results.append({'name':name,'passed':True});print('PASS '+name)
        def choose(section,model):page.locator('#rackSection').select_option(section);page.locator('#rackModel').select_option(model)
        def edit(key,v):page.locator('[data-rack-control="'+key+'"]').evaluate('(e,v)=>{e.value=String(v);e.dispatchEvent(new Event("input"));e.dispatchEvent(new Event("change"));}',v)
        def tabs():
            assert page.locator('#rackLab').is_visible();assert page.locator('#ampLab').is_hidden();assert page.locator('#pedalLab').is_hidden()
            page.locator('#ampTab').click();assert page.locator('#rackLab').is_hidden();assert page.locator('#ampLab').is_visible()
            page.locator('#pedalTab').click();assert page.locator('.pedal').count()==5;assert page.locator('#rackLab').is_hidden()
        test('three tabs remain mutually exclusive with five pedal slots',tabs)
        def inventory():
            for section in ['bus','preamp','eq']:
                page.locator('#rackSection').select_option(section);assert page.locator('#rackModel option').count()==3
                for i in range(3):page.locator('#rackModel').select_option(f'legacy.{section}.{i}');assert page.locator('[data-rack-control]').count()>0
        test('all nine rack panels render',inventory)
        def fet():
            choose('bus','legacy.bus.1');assert page.locator('[data-rack-control="hw.ratio"] option').count()==5
            assert page.locator('[data-rack-control="hw.attack_off"]').count()==1;assert page.locator('[data-rack-control="hw.threshold"]').count()==0
        test('1176 preserves original ratio and attack off',fet)
        def opto():
            choose('bus','legacy.bus.2');assert page.locator('[data-rack-control="hw.attack"]').count()==0
            assert 'REAR / CALIBRATION' in page.locator('#rackControls').inner_text();assert '신호 없음' in page.locator('.rack-meter').inner_text()
        test('LA2A has no fictional time controls or fake signal meter',opto)
        def recall():
            choose('bus','legacy.bus.1');edit('hw.input',.12);choose('bus','legacy.bus.2');edit('hw.gain',.81);choose('bus','legacy.bus.1')
            assert page.locator('[data-rack-control="hw.input"]').input_value()=='0.12'
        test('per-model values recall independently',recall)
        def eq():
            choose('eq','legacy.eq.2');edit('hw.lf_boost',.63);edit('hw.lf_atten',.42)
            p=page.evaluate('rackRig.current().parameters');assert p['hw.lf_boost']==.63 and p['hw.lf_atten']==.42
        test('Pultec simultaneous boost and atten retains both',eq)
        def utilities():
            choose('bus','legacy.bus.1');edit('hw.output',.6);page.locator('#rackOutputLevel').fill('-7');page.locator('#rackOutputLevel').dispatch_event('change')
            assert page.evaluate('rackRig.current().parameters["hw.output"]')==.6
            assert page.evaluate('rackRig.current().utility.outputLevel')==-7
        test('software trim independent from hardware output',utilities)
        def isolation():
            old=page.evaluate('JSON.stringify([board.state,ampRig.state])');choose('preamp','legacy.preamp.2');edit('hw.trim',.78)
            assert page.evaluate('JSON.stringify([board.state,ampRig.state])')==old
        test('rack edits preserve pedal and amp state',isolation)
        def undo():
            choose('bus','legacy.bus.1');edit('hw.input',.2);page.locator('#rackUndo').click();assert page.evaluate('rackRig.current().parameters["hw.input"]')==.5
            page.locator('#rackRedo').click();assert page.evaluate('rackRig.current().parameters["hw.input"]')==.2
        test('rack undo redo controls operate',undo)
        def export_import():
            choose('bus','legacy.bus.1');edit('hw.input',.23)
            with page.expect_download() as info:page.locator('#rackExport').click()
            download=info.value;assert download.suggested_filename.endswith('.crlab');data=Path(download.path()).read_bytes()
            edit('hw.input',.91);page.locator('#rackImport').set_input_files({'name':'test.crlab','mimeType':'application/json','buffer':data})
            page.wait_for_function('rackRig.current().parameters["hw.input"]===0.23')
        test('actual crlab export import roundtrip',export_import)
        def reject():
            old=page.evaluate('rackRig.export()');data=json.loads(old);data['sections']['bus']['bank']['legacy.bus.0']['hw.threshold']=9
            page.locator('#rackImport').set_input_files({'name':'bad.crlab','mimeType':'application/json','buffer':json.dumps(data).encode()})
            page.wait_for_function('document.getElementById("rackMessage").textContent.includes("잘못된")');assert page.evaluate('rackRig.export()')==old
        test('malformed import rejects without losing state',reject)
        def layout():
            choose('preamp','legacy.preamp.2')
            for width in [1280,1024,900,640]:
                page.set_viewport_size({'width':width,'height':900});assert page.evaluate('document.documentElement.scrollWidth<=window.innerWidth')
            page.set_viewport_size({'width':1280,'height':900});page.screenshot(path=str(ROOT/'reports/rack-isa.png'),full_page=True)
        test('layout fits 1280 1024 900 and 640 pixels',layout)
        context.close();browser.close()
    (ROOT/'reports/rack-browser-results.json').write_text(json.dumps(results,indent=2)+'\n')
    print(str(len(results))+' rack browser cases passed')
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--browser',required=True);run(parser.parse_args().browser)
