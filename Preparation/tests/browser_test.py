"""Offline UI smoke tests. Chromium content injection avoids any network/file-URL dependency."""
from pathlib import Path
import argparse
import json
from playwright.sync_api import sync_playwright

ROOT=Path(__file__).resolve().parents[1]

def run(executable):
    html=(ROOT/'Chimera_FiveSlot_Preview.html').read_text(encoding='utf-8')
    results=[]
    with sync_playwright() as p:
        browser=p.chromium.launch(executable_path=executable,headless=True,args=['--no-sandbox'])
        page=browser.new_page(viewport={'width':1280,'height':860})
        errors=[];page.on('pageerror',lambda error:errors.append(str(error)))
        def reset():
            nonlocal page
            page.close()
            page=browser.new_page(viewport={'width':1280,'height':860})
            page.on('pageerror',lambda error:errors.append(str(error)))
            page.set_content(html,wait_until='load')
        def test(name,fn):
            reset();fn();assert not errors,errors;results.append({'name':name,'passed':True})
        def five():
            assert page.locator('.pedal').count()==5
            assert page.locator('#add').is_disabled()
            assert page.locator('#duplicate').is_disabled()
        test('five visible slots and no sixth add/duplicate',five)
        def cap():
            result=page.evaluate("() => {try {board.add('planned.sd1',{drive:.2,tone:.2,level:.2});return false;} catch(e) {return board.state.effects.length===5;}}")
            assert result
        test('UI state rejects hidden sixth pedal',cap)
        def replace():
            page.locator('.pedal').nth(2).get_by_role('button',name='교체').click()
            page.locator('#search').fill('M104')
            page.locator('.result').click()
            page.locator('.pedal').nth(2).get_by_role('button',name='전용 조절부').click()
            assert page.locator('#controls .control').count()==2
            assert page.locator('#editorTitle').inner_text()=='MXR Distortion+ M104'
        test('cross-category replacement and exact Distortion+ two-control panel',replace)
        def jb2():
            page.locator('.pedal').nth(3).get_by_role('button',name='전용 조절부').click()
            assert page.locator('#controls input[type=range]').count()==6
            assert page.locator('#controls select option').count()==6
            page.locator('[data-control="boss.drive"]').evaluate('(e)=>{e.value=0.24}')
            page.locator('[data-control="boss.drive"]').dispatch_event('change')
            page.locator('[data-control="jhs.drive"]').evaluate('(e)=>{e.value=0.82}')
            page.locator('[data-control="jhs.drive"]').dispatch_event('change')
            values=page.evaluate('board.state.effects[3].parameters')
            assert values['boss.drive']==.24 and values['jhs.drive']==.82
        test('JB2 one slot with six independent knobs and six mode labels',jb2)
        def midi_move():
            page.locator('.pedal').nth(2).get_by_role('button',name='전용 조절부').click()
            page.locator('#midiControl').select_option('tone');page.locator('#bind').click();page.locator('#closeEditor').click()
            before=page.evaluate('board.state.effects[2].id');page.locator('#right').click()
            e=page.evaluate('board.state.effects[3]');assert e['id']==before and e['midi']['74']=='tone'
        test('stored MIDI binding follows moved instance',midi_move)
        def duplicate():
            page.locator('#remove').click();page.locator('.pedal').nth(1).click();page.locator('#duplicate').click()
            s=page.evaluate('board.state');assert len(s['effects'])==5
            ids=[e['id'] for e in s['effects']];assert len(set(ids))==5
            assert s['effects'][1]['model']==s['effects'][2]['model']
        test('duplicate consumes one free slot and gets independent id',duplicate)
        def undo():
            before=page.evaluate('board.state.effects');page.locator('#remove').click();page.locator('#undo').click()
            assert page.evaluate('board.state.effects')==before
            page.locator('#redo').click();assert len(page.evaluate('board.state.effects'))==4
        test('undo redo restores exact instance content',undo)
        def tap():
            page.locator('#right').click();assert 'LOW' in page.locator('#status').inner_text()
            assert page.evaluate('board.state.lowTap')==1
        test('LOW tap crossing visibly reported',tap)
        def mode():
            before=page.evaluate('board.state');page.locator('#mode').select_option('Classic')
            assert page.locator('#taplabel').is_hidden();assert page.evaluate('board.state')==before
        test('mode view does not reorder or duplicate pedals',mode)
        def good_import():
            payload=page.evaluate('ChimeraPrep.encode(board.state)');before=page.evaluate('board.state.effects')
            page.locator('#remove').click()
            page.locator('#file').set_input_files({'name':'test.cbp','mimeType':'text/plain','buffer':payload.encode()})
            page.wait_for_function('board.state.effects.length===5')
            assert page.evaluate('board.state.effects')==before
        test('native-compatible CBP state import restores board',good_import)
        def bad_import():
            before=page.evaluate('board.state.effects')
            bad='CHIMERA_BOARD_PREP 1\n8 0 "legacy-v1" 6\n'
            page.locator('#file').set_input_files({'name':'bad.cbp','mimeType':'text/plain','buffer':bad.encode()})
            page.wait_for_function("document.getElementById('status').textContent.includes('범위')")
            assert page.evaluate('board.state.effects')==before
        test('malformed six-slot import rejected without mutation',bad_import)
        def dimensions():
            for width,height in ((1280,860),(1024,768),(900,700),(640,720)):
                page.set_viewport_size({'width':width,'height':height})
                assert page.evaluate('document.documentElement.scrollWidth<=window.innerWidth')
                boxes=page.locator('.pedal').all();assert len(boxes)==5
                ys=[round(x.bounding_box()['y']) for x in boxes];assert len(set(ys))==1
            page.set_viewport_size({'width':1280,'height':860})
        test('five cards remain single-row without horizontal overflow',dimensions)
        def legacy():
            page.locator('#legacy').click();s=page.evaluate('board.state')
            assert [e['model'] for e in s['effects']]==['legacy.filter.0','legacy.comp.0','legacy.fuzz.0','legacy.boost.0','legacy.drive.0']
            assert s['lowTap']==2 and all(e['bypass'] for e in s['effects'])
        test('legacy order default and bypass values preserved in preview adapter',legacy)
        def save():
            with page.expect_download() as info:page.locator('#save').click()
            download=info.value
            assert download.suggested_filename=='chimera-five-slot.cbp'
            download.save_as(ROOT/'reports/browser-state.cbp')
        test('export creates preparation state not production project',save)
        reset();page.screenshot(path=str(ROOT/'reports/preview-board.png'),full_page=True)
        page.locator('.pedal').nth(3).get_by_role('button',name='전용 조절부').click()
        page.screenshot(path=str(ROOT/'reports/preview-jb2.png'),full_page=True)
        report={'browser':browser.version,'navigation':'set_content; offline','tests':results,'page_errors':errors,
                'audio_validation':False,'production_plugin_validation':False}
        (ROOT/'reports/browser-tests.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        browser.close()
    print(f'{len(results)} browser UI cases passed')
    return report

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--browser',required=True)
    run(parser.parse_args().browser)
