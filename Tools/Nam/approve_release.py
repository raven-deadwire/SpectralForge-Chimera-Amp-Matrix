#!/usr/bin/env python3
"""Mark a verified TONE3000 package as release-approved under release profile v3.

Approval needs two things: every channel passes the v3 numbers on validation
and on the reserved test (recomputed here from the package's own reports), and
the owner's listening acceptance, which only the owner can give and which is
recorded as stated. Only metadata changes: each .nam keeps its weights, its
status becomes RELEASE_APPROVED, and its preset is rebuilt with the player's
own writer. The rebuilt model must render bit-identically to the approved
one, and each preset must still restore its audio.
"""
import argparse, json, subprocess, tempfile
from pathlib import Path
import numpy as np
import soundfile as sf
from common import digest, write_json
from capture_channels import VOICES
from quality_profile import RELEASE_V3, release_v3_check

TEST_TITLE = '# Náströnd — TONE3000 A2 테스트 패키지'
README_REPLACEMENTS = [
    (TEST_TITLE, '# Náströnd — TONE3000 A2 릴리즈 패키지'),
    ('아직 테스트 빌드이며, 실제 악기 DI 청취와 GUI/DAW 실사용 검증은 완료되지 않았습니다.',
     '출시 기준 v3 수치를 모두 통과했고, 오너 청음 확인을 거쳐 출시 승인되었습니다(아래 "출시 승인" 참조).'),
    ('릴리즈 승인은 부여하지 않습니다.', '출시 승인 여부는 아래 "출시 승인"을 따릅니다.'),
    ('아니오 — 정식 릴리즈 승인 전 추가 개선 필요.', '아니오 — 원본 충실도 참고 기준(v2)입니다. 출시 판정은 아래 출시 기준 v3를 따릅니다.'),
    ('다만 출시 승인에는 실제 악기 DI로 들어 보는 청음 확인이 함께 필요한데, 아직 하지 않았습니다. 그래서 이 패키지는 아직 테스트 빌드입니다. ',
     '오너 청음 확인도 완료되어 출시 승인되었습니다. '),
]


def render(tool, args):
    return subprocess.check_output([str(tool), *map(str, args)], text=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--package', type=Path, required=True); ap.add_argument('--tool', type=Path, required=True)
    ap.add_argument('--approved-by', required=True); ap.add_argument('--date', required=True)
    ap.add_argument('--listening', required=True, help='The owner\'s listening acceptance, as stated by the owner')
    args = ap.parse_args()
    pkg = args.package
    validation = json.loads((pkg/'Validation/validation.json').read_text())
    comparison = json.loads((pkg/'Validation/comparison.json').read_text())
    if validation.get('release_approved'): raise RuntimeError('Package is already approved')
    verdict = {}
    for name, _ in VOICES:
        row = validation['channels'][name]
        if digest(pkg/'NAM'/f'Nastrond-{name}.nam') != row['model_sha256'] or digest(pkg/'TONE3000-Presets'/f'Nastrond-{name}.t3kpreset') != row['preset_sha256']:
            raise RuntimeError(f'Package files differ from their verified hashes: {name}')
        v = release_v3_check(row['tone3000_fidelity'], name)
        t = release_v3_check(comparison['channels'][name]['tone3000_fidelity'], name)
        verdict[name] = {'validation': v, 'test': t, 'numbers_pass': v['pass'] and t['pass']}
    failing = [n for n, r in verdict.items() if not r['numbers_pass']]
    if failing: raise RuntimeError(f'v3 numbers fail for: {", ".join(failing)}')
    record = {'profile': RELEASE_V3['id'], 'approved_by': args.approved_by, 'date': args.date,
              'listening_acceptance': args.listening, 'v3_numbers': verdict}
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp); raw, out_old, out_new = tmp/'in.f32', tmp/'old.f32', tmp/'new.f32'
        x, rate = sf.read(pkg/'Audio'/'Synthetic-DI-input.wav', dtype='float32')
        if rate != 48000: raise RuntimeError('Unexpected audition rate')
        x.astype('<f4').tofile(raw)
        for name, _ in VOICES:
            model, preset, settings = pkg/'NAM'/f'Nastrond-{name}.nam', pkg/'TONE3000-Presets'/f'Nastrond-{name}.t3kpreset', pkg/'Settings'/f'{name}.json'
            render(args.tool, ['render', model, raw, out_old, settings])
            data = json.loads(model.read_text())
            data['metadata'].update(status='RELEASE_APPROVED', release_profile=RELEASE_V3['id'],
                                    release_approval={k: record[k] for k in ('approved_by', 'date', 'listening_acceptance')})
            model.write_text(json.dumps(data))
            render(args.tool, ['render', model, raw, out_new, settings])
            if not np.array_equal(np.fromfile(out_old, '<f4'), np.fromfile(out_new, '<f4')):
                raise RuntimeError(f'Metadata change altered the audio: {name}')
            state = json.loads(render(args.tool, ['preset', model, preset, settings]))
            render(args.tool, ['render-preset', preset, raw, out_old])
            delta = float(np.max(np.abs(np.fromfile(out_old, '<f4')-np.fromfile(out_new, '<f4'))))
            if not delta <= 1e-6: raise RuntimeError(f'Rebuilt preset does not restore: {name}')
            for report in (validation['channels'][name], comparison['channels'][name]):
                report['model_sha256'] = digest(model); report['preset_sha256'] = digest(preset)
            validation['channels'][name]['preset'] = state
            record['v3_numbers'][name]['preset_restore_max_abs'] = delta
    validation.update(status='RELEASE_APPROVED', release_approved=True, release_profile_v3=record,
                      real_DI_listening=f'owner listening acceptance: {args.listening} ({args.date})')
    write_json(pkg/'Validation/validation.json', validation)
    write_json(pkg/'Validation/comparison.json', comparison)
    write_json(pkg/'Validation/release-approval.json', record)
    readme = (pkg/'README-KO.md').read_text()
    if not readme.startswith(TEST_TITLE): raise RuntimeError('Unexpected README')
    for old, new in README_REPLACEMENTS:
        if old not in readme: raise RuntimeError(f'README text not found: {old[:30]}')
        readme = readme.replace(old, new)
    readme += (f'\n## 출시 승인\n\n- 기준: 출시 기준 v3 (`{RELEASE_V3["id"]}`). 다섯 채널 모두 검증·테스트 수치 통과\n'
               f'- 청음 확인: {args.listening}\n- 승인: {args.approved_by}, {args.date}\n'
               '- 원본 충실도 참고 기준(v2)은 통과하지 않습니다. 하이게인 채널의 full ESR은 0.018~0.022입니다.\n'
               '- 상세 기록: `Validation/release-approval.json`\n')
    (pkg/'README-KO.md').write_text(readme)
    (pkg/'SHA256SUMS').write_text(''.join(f'{digest(p)}  {p.relative_to(pkg).as_posix()}\n'
                                          for p in sorted(pkg.rglob('*')) if p.is_file() and p.name != 'SHA256SUMS'))
    print(json.dumps({'package': str(pkg), 'release_approved': True, 'channels': {n: r['numbers_pass'] for n, r in verdict.items()}}))


if __name__ == '__main__':
    main()
