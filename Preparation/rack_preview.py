"""Extend existing amp+pedal preview without changing their state contracts."""
def extend_racks(html,root):
    anchor='<button id="ampTab" role="tab" aria-selected="false">AMP · 모델별 조절부</button>'
    if html.count(anchor)!=1:raise ValueError('Amp tab anchor changed')
    html=html.replace(anchor,anchor+'<button id="rackTab" role="tab" aria-selected="false">RACK · POST 조절부</button>',1)
    html=html.replace('</style>',(root/'ui/rack.css').read_text(encoding='utf-8')+'</style>',1)
    if html.count('<footer>')!=1:raise ValueError('Footer anchor changed')
    html=html.replace('<footer>',(root/'ui/rack_panel.html').read_text(encoding='utf-8')+'\n<footer>',1)
    html=html.replace('</body>','<script>'+ (root/'ui/rack_state.js').read_text(encoding='utf-8')+'</script><script>'+(root/'ui/rack_editor.js').read_text(encoding='utf-8')+'</script></body>',1)
    return html
