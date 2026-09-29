"""Add the amp lab without editing or duplicating the existing pedal template."""
from pathlib import Path


def extend_template(html: str, root: Path) -> str:
    replacements = [
        ('</style>', (root / 'ui/amp.css').read_text(encoding='utf-8') + '</style>'),
        ('Five-slot pedalboard · 기존 25종 원본 조작부 개정판',
         'Five-slot pedalboard / Model-specific amp controls'),
        ('<div class="toolbar"><button id="undo">',
         '<div class="lab-tabs" role="tablist" aria-label="준비 화면"><button id="pedalTab" role="tab" aria-selected="true">PEDAL · 최대 5개</button><button id="ampTab" role="tab" aria-selected="false">AMP · 모델별 조절부</button></div>\n<section id="pedalLab">\n<div class="toolbar"><button id="undo">'),
        ('</div><footer>', '</div>\n</section>\n__AMP_PANEL__\n<footer>'),
        ('</script></body></html>',
         '</script><script>__AMP_STATE_JS__</script><script>__AMP_EDITOR_JS__</script></body></html>'),
    ]
    for anchor, replacement in replacements:
        if html.count(anchor) != 1:
            raise ValueError('Pedal template anchor changed: ' + anchor[:60])
        html = html.replace(anchor, replacement, 1)
    return html
