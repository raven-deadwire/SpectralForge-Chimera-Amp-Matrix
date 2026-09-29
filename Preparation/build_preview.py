#!/usr/bin/env python3
"""Generate a self-contained offline HTML preview and machine-readable catalog."""
import json
from pathlib import Path
from catalog import build_catalog
from amp_preview import extend_template
from rack_preview import extend_racks

def build(output=None):
    root=Path(__file__).resolve().parent
    catalog=build_catalog()
    (root/'catalog.json').write_text(json.dumps(catalog,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    html=extend_template((root/'ui/prototype.template.html').read_text(encoding='utf-8'),root)
    html=html.replace('__AMP_PANEL__',(root/'ui/amp_panel.html').read_text(encoding='utf-8'))
    html=html.replace('__AMP_STATE_JS__',(root/'ui/amp_state.js').read_text(encoding='utf-8'))
    html=html.replace('__AMP_EDITOR_JS__',(root/'ui/amp_editor.js').read_text(encoding='utf-8'))
    html=html.replace('__CONTROL_JS__',(root/'ui/control_state.js').read_text(encoding='utf-8'))
    html=html.replace('__STATE_JS__',(root/'ui/state.js').read_text(encoding='utf-8'))
    html=html.replace('__CATALOG__',json.dumps(catalog,ensure_ascii=False,separators=(',',':')).replace('<','\\u003c'))
    html=extend_racks(html,root)
    output=Path(output) if output else root/'Chimera_FiveSlot_Preview.html'
    output.write_text(html,encoding='utf-8')
    return output
if __name__=='__main__': print(build())
