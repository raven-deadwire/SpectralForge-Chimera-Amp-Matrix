#!/usr/bin/env python3
"""Additive Issue #97 gate. No approvals, waivers, builds or network writes.

The reviewed approval fingerprints the entire evidence manifest. Automatic
reports must also be bytes from a digest-verified, live GitHub Actions artifact.
Manual reports are explicit, reviewed attestations with attached raw evidence;
this program cannot itself operate a DAW or certify a listening decision.
"""
from __future__ import annotations
import argparse
from datetime import datetime
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import re
import tempfile
import zipfile
from evaluate_release_gate import ValidationError

REPO = 'raven-deadwire/SpectralForge-Chimera-Amp-Matrix'
SCHEMA = 'spectralforge.chimera.issue97-evidence'
ROOT = Path(__file__).resolve().parents[1]
PLATFORMS = ('windows-x64', 'macos-x64', 'macos-arm64', 'linux-x64')
WORKFLOWS = frozenset(('build.yml', 'beta-lifecycle.yml', 'signal-path.yml',
                      'graphical-eq.yml', 'cab-panel.yml', 'cab-realtime.yml',
                      'dynamics-spectrum.yml', 'aax-native.yml', 'issue97-acceptance.yml'))
# No N/A, gap acceptance or optional P0s. Names describe separate evidence,
# rather than treating an existing workflow's green summary as acceptance.
COMMON = {
 'integration': ('transpose_product_path', 'fabfilter_product_path', 'lifecycle_product_path',
                 'aax_common_dsp', 'parameter_order', 'legacy_project', 'preset_ir_restore'),
 'transpose': ('original_capture', 'low_b_minus2_same_di', 'attack', 'fundamental',
               'clarity', 'chord_separation', 'artifacts', 'algorithmic_latency',
               'reported_latency', 'roundtrip_latency', 'pdc'),
 'fabfilter': ('original_capture', 'response', 'detector', 'timing', 'phase', 'latency',
               'meters', 'analyzer', 'interaction', 'six_editors', 'pre_post_instances',
               'actual_in_out', 'dynamic_eq_edit', 'independent_multiband', 'bypass_transition'),
 'lifecycle': ('countdown', 'identity_policy_ui_package', 'before_boundary_after',
               'expire_running', 'restart', 'clock_timezone', 'offline', 'expired_export',
               'successor_project_preset_ir_restore'),
 'native_daw': ('scan_load', 'mono_stereo', 'automation', 'session_reopen', 'preset_ir_restore',
                'ui_open_close', 'instance_delete', 'session_close', 'app_exit', 'expiry'),
 'system': ('classic_dual_matrix', 'transpose_off_minus2', 'gui_analyzer_open_closed',
            'instances_1_2_4', 'session_reopen', 'requirements_documents', 'standalone_driver'),
}
AAX = ('build_install', 'pace_signature', 'platform_signature', 'retail_pro_tools',
       'scan_load', 'mono_stereo', 'automation', 'session_reopen', 'preset_ir_restore',
       'pdc', 'ui_open_close', 'instance_delete', 'session_close', 'app_exit', 'expiry')

class EvidenceError(ValidationError):
    pass

def require(value, reason):
    if not value:
        raise EvidenceError(reason)

def file_digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()

def digest(raw):
    return hashlib.sha256(raw).hexdigest()

def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=False,
                      allow_nan=False).encode()

def sha(value, size=64):
    return isinstance(value, str) and re.fullmatch('[0-9a-f]{'+str(size)+'}', value) is not None

def text(value):
    return isinstance(value, str) and bool(value.strip())

def number(value):
    return type(value) in (int, float) and math.isfinite(value)

def timestamp(value):
    try:
        return datetime.fromisoformat(value.replace('Z', '+00:00')).utcoffset() is not None
    except (ValueError, TypeError, AttributeError):
        return False

def load(raw):
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, 'duplicate JSON key: '+key)
            result[key] = value
        return result
    return json.loads(raw, object_pairs_hook=pairs,
                      parse_constant=lambda v: (_ for _ in ()).throw(EvidenceError('nonfinite JSON: '+v)))

def is_successor(version):
    match = re.fullmatch(r'(\d+)\.(\d+)\.(\d+)(?:-(beta|rc)\.([1-9][0-9]*))?', version)
    require(match is not None, 'canonical version required')
    numeric = tuple(int(match[i]) for i in (1, 2, 3))
    return numeric > (1, 3, 2) or numeric == (1, 3, 2) and not (match[4] == 'beta' and match[5] == '1')

def requirements():
    rows = {}
    for platform in PLATFORMS:
        for feature, assertions in COMMON.items():
            rows[feature+'/'+platform] = {'kind': 'manual' if feature in ('native_daw', 'system') else 'ci',
                                          'assertions': list(assertions)}
        if platform != 'linux-x64':
            rows['aax/'+platform] = {'kind': 'manual', 'assertions': list(AAX)}
    return rows

def contract_digest():
    return digest(canonical(requirements()))

def binding(value):
    require(isinstance(value, dict) and set(value) == {'schema', 'requirements_sha256', 'manifest_sha256', 'ci'}
            and value['schema'] == SCHEMA and value['requirements_sha256'] == contract_digest()
            and sha(value['manifest_sha256']) and isinstance(value.get('ci'), dict),
            'ISSUE97_P0_GATE_UNIMPLEMENTED: exact reviewed P0 evidence binding missing')


def safe_file(files, spec):
    require(isinstance(spec, dict) and set(spec) == {'path', 'sha256'}, 'file/hash reference required')
    path = spec['path']
    require(text(path) and not PurePosixPath(path).is_absolute() and '\\' not in path
            and '..' not in PurePosixPath(path).parts, 'unsafe evidence path')
    raw = files.get(path)
    require(isinstance(raw, bytes) and sha(spec['sha256']) and digest(raw) == spec['sha256'],
            'missing or changed evidence bytes: '+path)
    return raw


def verify_ci(proof, report_raw, source, api):
    """Use live API and downloaded artifact bytes, never an archive's API snapshot."""
    require(api is not None, 'live GitHub provenance unavailable (offline is diagnostic only)')
    require(isinstance(proof, dict) and all(type(proof.get(k)) is int and proof[k] > 0
            for k in ('run_id', 'run_attempt', 'artifact_id')), 'CI run/attempt/artifact required')
    report = load(report_raw)
    require(report.get('run_id') == proof['run_id'] and report.get('run_attempt') == proof['run_attempt'],
            'report run/attempt differs from live producer')
    run = api.json(f"/actions/runs/{proof['run_id']}")
    require(run.get('head_sha') == source and run.get('run_attempt') == proof['run_attempt']
            and run.get('status') == 'completed' and run.get('conclusion') == 'success'
            and run.get('repository', {}).get('full_name') == REPO
            and run.get('head_repository', {}).get('full_name') == REPO
            and run.get('event') in ('pull_request', 'push', 'workflow_dispatch')
            and run.get('path') in {'.github/workflows/'+w for w in WORKFLOWS},
            'stale, failed, foreign or unsupported CI run')
    jobs = api.pages(f"/actions/runs/{proof['run_id']}/jobs?filter=latest", 'jobs')
    require(jobs and all(j.get('status') == 'completed' and j.get('conclusion') == 'success'
                        and j.get('run_attempt') == proof['run_attempt'] for j in jobs),
            'CI has missing, skipped, failed or stale jobs')
    artifact = api.json(f"/actions/artifacts/{proof['artifact_id']}")
    require(artifact.get('expired') is False
            and artifact.get('workflow_run', {}).get('id') == proof['run_id']
            and artifact.get('workflow_run', {}).get('head_sha') == source
            and artifact.get('digest') == 'sha256:'+proof.get('artifact_sha256', '')
            and sha(proof.get('artifact_sha256')), 'artifact identity/digest/expiry mismatch')
    # GitHub metadata is not proof of the actual report bytes. Download and
    # inspect the producer artifact before trusting its report member.
    with tempfile.TemporaryDirectory(prefix='chimera-p0-') as work:
        path = Path(work)/'artifact.zip'
        api.download(f"/actions/artifacts/{proof['artifact_id']}/zip", path)
        require(file_digest(path) == proof['artifact_sha256'], 'producer artifact digest mismatch')
        with zipfile.ZipFile(path) as archive:
            names = archive.namelist()
            require(len(names) == len(set(names)), 'duplicate producer artifact entries')
            member = proof.get('report_member')
            require(member in names and archive.getinfo(member).file_size <= 4*1024*1024
                    and archive.read(member) == report_raw, 'report differs from actual CI artifact')
    return {'run_id': proof['run_id'], 'run_attempt': proof['run_attempt'],
            'artifact_id': proof['artifact_id'], 'artifact_sha256': proof['artifact_sha256']}


def verify_required_workflows(api, source):
    require(api is not None, 'live GitHub provenance unavailable (offline is diagnostic only)')
    results = {}
    required = WORKFLOWS - {'issue97-acceptance.yml'}
    for workflow in sorted(required):
        runs = api.pages(f'/actions/workflows/{workflow}/runs?head_sha={source}', 'workflow_runs')
        require(isinstance(runs, list) and runs, 'required exact-source workflow missing: '+workflow)
        # Never search backward for a passing run or ignore a newer failure.
        run = max(runs, key=lambda r: int(r['id']))
        require(run.get('head_sha') == source and run.get('status') == 'completed'
                and run.get('conclusion') == 'success'
                and run.get('repository', {}).get('full_name') == REPO
                and run.get('head_repository', {}).get('full_name') == REPO
                and run.get('path') == '.github/workflows/'+workflow,
                'latest required workflow not successful: '+workflow)
        jobs = api.pages(f"/actions/runs/{run['id']}/jobs?filter=latest", 'jobs')
        require(jobs and all(j.get('status') == 'completed' and j.get('conclusion') == 'success'
                            and j.get('run_attempt') == run['run_attempt'] for j in jobs),
                'required workflow has incomplete/failed/skipped jobs: '+workflow)
        results[workflow] = {'run_id': run['id'], 'run_attempt': run['run_attempt']}
    return results


def validate_report(report, row, spec, manifest, files):
    require(isinstance(report, dict) and report.get('schema') == SCHEMA+'.report'
            and report.get('version') == manifest['version'] and report.get('source_sha') == manifest['source_sha']
            and report.get('id') == row and report.get('kind') == spec['kind']
            and report.get('synthetic') is False and timestamp(report.get('executed_at')),
            'report identity or real execution missing: '+row)
    require(report.get('packages') == manifest['packages'], 'report package inventory differs: '+row)
    platform = row.split('/')[1]
    binaries = report.get('installed_binaries')
    require(binaries == manifest.get('binaries', {}).get(platform)
            and isinstance(binaries, list) and binaries and all(isinstance(b, dict)
            and b.get('platform') == platform and text(b.get('path')) and sha(b.get('sha256'))
            and b.get('package') in manifest['packages'] and text(b.get('format'))
            for b in binaries), 'installed executable hashes missing: '+row)
    raw_refs = report.get('raw')
    require(isinstance(raw_refs, list) and raw_refs, 'raw output/log/capture required: '+row)
    for ref in raw_refs:
        safe_file(files, ref)
    assertions = report.get('assertions')
    require(isinstance(assertions, dict) and set(assertions) == set(spec['assertions']),
            'missing or extra acceptance assertions: '+row)
    for name, assertion in assertions.items():
        require(isinstance(assertion, dict) and assertion.get('passed') is True
                and text(assertion.get('method')) and assertion.get('raw_path') in {r['path'] for r in raw_refs},
                'failed/unexecuted/unsubstantiated assertion: '+row+'/'+name)
    if spec['kind'] == 'manual':
        require(text(report.get('reviewer')) and text(report.get('host_name'))
                and text(report.get('host_version')) and text(report.get('os_version'))
                and report.get('installed_release_package') is True,
                'installed DAW/operator identity missing: '+row)
    feature = row.split('/')[0]
    metrics = report.get('metrics', {})
    require(isinstance(metrics, dict), 'numeric metrics required: '+row)
    if feature == 'transpose':
        samples = metrics.get('latency_cases')
        require(isinstance(samples, list) and len(samples) == 4
                and {s.get('semitones') for s in samples if isinstance(s, dict)} == {-2, -1, 1, 2},
                '48k +/-1,+/-2 latency coverage required')
        for sample in samples:
            require(sample.get('sample_rate') == 48000 and type(sample.get('semitones')) is int
                    and number(sample.get('algorithmic_ms')) and 0 <= sample['algorithmic_ms'] <= 10
                    and number(sample.get('weak_attack_max_ms')) and 0 <= sample['weak_attack_max_ms'] <= 10
                    and number(sample.get('host_reported_ms')) and sample['host_reported_ms'] >= 0
                    and number(sample.get('roundtrip_ms')) and sample['roundtrip_ms'] >= 0,
                    'Transpose latency target exceeded or latency definitions missing')
        require(metrics.get('input') == 'Azhi Dahaka / bass Low B / -2'
                and metrics.get('di_sha256') in {r['sha256'] for r in raw_refs}
                and metrics.get('reference_capture_sha256') in {r['sha256'] for r in raw_refs}
                and text(metrics.get('reference_product_version'))
                and number(metrics.get('level_alignment_db')), 'same DI/reference identity missing')
    if feature == 'fabfilter':
        products = metrics.get('references')
        require(isinstance(products, dict) and set(products) == {'Pro-Q 4', 'Pro-C 3', 'Pro-MB', 'Pro-G', 'Pro-L 2', 'Pro-DS'}
                and all(isinstance(p, dict) and text(p.get('version')) and p.get('capture_sha256') in {r['sha256'] for r in raw_refs}
                        for p in products.values()), 'original product captures/versions missing')
        # Comparisons need a separately reviewed numerical protocol. Undefined
        # tolerances stay BLOCKED; the evaluator does not invent equivalence.
        protocol = manifest.get('comparison_protocol')
        require(isinstance(protocol, dict) and text(protocol.get('reviewer'))
                and timestamp(protocol.get('approved_at')) and text(protocol.get('rationale')),
                'FabFilter comparison acceptance protocol not reviewed')
        limits = protocol.get('limits')
        require(isinstance(limits, dict) and set(limits) == {'response_db', 'timing_ms', 'phase_degrees',
                'latency_ms', 'meter_db', 'analyzer_db', 'bypass_transition_db'}, 'comparison protocol incomplete')
        for metric, limit in limits.items():
            value = metrics.get(metric)
            require(number(value) and number(limit) and limit >= 0 and 0 <= value <= limit,
                    'comparison exceeds reviewed limit or has no measurement: '+metric)
    if feature == 'lifecycle':
        require(report.get('expiry_policy') == manifest['expiry_policy'], 'runtime/package expiry mismatch')
    if feature == 'aax':
        require(report.get('host_name') == 'Pro Tools' and report.get('developer_host') is False
                and any(b['format'] == 'AAX' for b in binaries), 'retail Pro Tools/AAX binary required')
        signatures = report.get('signature_commands')
        require(isinstance(signatures, dict) and set(signatures) == {'pace', 'platform', 'installed'}
                and all(isinstance(s, dict) and text(s.get('command')) and type(s.get('exit_code')) is int
                        and s['exit_code'] == 0 and s.get('raw_path') in {r['path'] for r in raw_refs}
                        for s in signatures.values()), 'actual package and installed signature verification missing')
    if feature == 'system':
        fields = ('cpu', 'ram', 'os', 'daw', 'format', 'interface', 'driver', 'sample_rate',
                  'buffer', 'oversampling', 'preset', 'power_mode', 'gui')
        require(isinstance(report.get('environment'), dict)
                and all((number(report['environment'].get(k)) if k in ('sample_rate', 'buffer', 'oversampling')
                         else text(report['environment'].get(k))) for k in fields), 'test environment incomplete')
        for name in ('duration_minutes', 'callback_p99_us', 'callback_peak_us', 'deadline_misses',
                     'memory_growth_bytes', 'memory_peak_bytes'):
            require(number(metrics.get(name)) and metrics[name] >= 0, 'system measurement missing: '+name)
        env = report['environment']
        require(env['sample_rate'] == 48000 and env['buffer'] in (128, 256) and env['oversampling'] == 4,
                'representative 48k/128-or-256/4x system condition missing')
        budget = 1e6 * env['buffer'] / env['sample_rate']
        require(metrics['callback_p99_us'] <= budget and metrics['callback_peak_us'] <= budget,
                'system callback exceeds unchanged block budget')
        require(metrics['duration_minutes'] >= 30 and metrics['deadline_misses'] == 0,
                'soak duration/deadline acceptance not met')
    return report


def evaluate(manifest_raw, files, *, version, source_sha, approval, assets, api=None, lifecycle_policy_raw=None):
    binding(approval)
    require(digest(manifest_raw) == approval['manifest_sha256'], 'P0 manifest differs from reviewed approval')
    manifest = load(manifest_raw)
    require(manifest.get('schema') == SCHEMA and type(manifest.get('schema_version')) is int and manifest.get('schema_version') == 1
            and manifest.get('version') == version and manifest.get('source_sha') == source_sha
            and sha(source_sha, 40) and manifest.get('requirements_sha256') == contract_digest(),
            'P0 version/source/requirements identity mismatch')
    packages = manifest.get('packages')
    require(isinstance(packages, dict) and packages and assets is not None, 'actual candidate packages unavailable')
    # Every distributable binary, including AAX, must be in the same reviewed
    # set. Documentation and generated evidence are not binary packages.
    suffixes = ('.exe', '.zip', '.pkg', '.deb', '.tar.gz')
    actual = {name: file_digest(path) for name, path in assets.items() if name.startswith('SpectralForge-Chimera-'+version+'-') and name.endswith(suffixes)}
    require(actual == packages and all(sha(h) for h in packages.values()), 'actual candidate package hashes differ')
    binaries = manifest.get('binaries', {})
    require(isinstance(binaries, dict) and set(binaries) == set(PLATFORMS), 'binary platform inventory missing')
    for platform, inventory in binaries.items():
        formats = {'Standalone', 'VST3'} | ({'AU', 'AAX'} if platform.startswith('macos') else
                                          {'AAX'} if platform.startswith('windows') else set())
        require(isinstance(inventory, list) and all(isinstance(b, dict) for b in inventory)
                and {b.get('format') for b in inventory} == formats
                and len(inventory) == len(formats), 'binary format inventory incomplete: '+platform)
    expiry = manifest.get('expiry_policy', {})
    require(isinstance(expiry, dict) and expiry.get('version') == version and text(expiry.get('approved_by'))
            and timestamp(expiry.get('approved_at')) and timestamp(expiry.get('expires_at_utc'))
            and sha(expiry.get('policy_sha256')), 'version-specific expiry approval missing')
    require(isinstance(lifecycle_policy_raw, bytes) and digest(lifecycle_policy_raw) == expiry['policy_sha256'],
            'expiry approval is not bound to actual source policy bytes')
    configured = load(lifecycle_policy_raw)
    require(configured.get('version') == version and configured.get('expiry_approved') is True
            and configured.get('expires_at_utc') == expiry['expires_at_utc'], 'source expiry policy mismatch')
    metadata_path = assets.get('chimera-beta-lifecycle.json')
    require(metadata_path is not None, 'actual packaged beta lifecycle metadata missing')
    metadata = load(metadata_path.read_bytes())
    require(metadata.get('version') == version and metadata.get('source_sha') == source_sha
            and metadata.get('expires_at_utc') == expiry['expires_at_utc']
            and metadata.get('expiry_approved') is True and metadata.get('mode') == 'approved',
            'packaged beta expiry/build differs from source policy approval')
    expected, rows = requirements(), manifest.get('reports')
    require(isinstance(rows, dict) and set(rows) == set(expected), 'P0 requirement/platform inventory incomplete')
    manifest_proof = approval['ci']
    require(manifest_proof.get('report_member') == 'issue97/manifest.json', 'P0 manifest artifact member differs')
    verified_manifest = verify_ci(manifest_proof, manifest_raw, source_sha, api)
    workflow_runs = verify_required_workflows(api, source_sha)
    verified = []
    for row, spec in expected.items():
        entry = rows[row]
        require(isinstance(entry, dict), 'invalid report entry: '+row)
        raw = safe_file(files, entry.get('report'))
        report = validate_report(load(raw), row, spec, manifest, files)
        if spec['kind'] == 'ci':
            verified.append(verify_ci(entry.get('ci'), raw, source_sha, api))
        else:
            require(entry.get('ci') is None, 'manual DAW attestation mislabeled as CI')
    return {'verdict': 'PASS', 'version': version, 'source_sha': source_sha,
            'manifest_sha256': digest(manifest_raw), 'requirements_sha256': contract_digest(),
            'reports': len(rows), 'packages': packages, 'verified_ci': verified,
            'workflow_runs': workflow_runs, 'manifest_producer': verified_manifest,
            'manual_scope': 'Reviewed operator attestations and attached evidence; not automated DAW execution.'}


def context_from_zip(archive, assets, api, lifecycle_policy_raw=None):
    entries = archive.infolist()
    require(len(entries) <= 4096 and len({e.filename for e in entries}) == len(entries)
            and sum(e.file_size for e in entries) <= 64*1024*1024, 'duplicate or oversized P0 archive')
    files = {}
    for entry in entries:
        if entry.filename.startswith('issue97/') and not entry.is_dir():
            require(entry.file_size <= 16*1024*1024, 'oversized P0 evidence member')
            files[entry.filename] = archive.read(entry)
    require('issue97/manifest.json' in files, 'ISSUE97_EVIDENCE_MISSING: manifest absent')
    return dict(manifest_raw=files['issue97/manifest.json'], files=files, assets=assets, api=api, lifecycle_policy_raw=lifecycle_policy_raw)


def pack_evidence(folder, destination):
    """Package existing evidence only; never manufacture reports or acceptance.

    The resulting ZIP must be uploaded by a same-source repository workflow.
    Its live artifact identity is added to the separately reviewed approval.
    """
    manifest_path = folder / 'issue97/manifest.json'
    require(manifest_path.is_file() and not manifest_path.is_symlink(), 'P0 manifest missing')
    manifest_raw = manifest_path.read_bytes()
    manifest = load(manifest_raw)
    require(manifest.get('schema') == SCHEMA and manifest.get('requirements_sha256') == contract_digest(),
            'P0 manifest schema/requirements mismatch')
    files = {}
    for path in (folder/'issue97').rglob('*'):
        require(not path.is_symlink(), 'symlink in P0 evidence')
        if path.is_file():
            require(path.stat().st_size <= 16*1024*1024, 'oversized P0 evidence member')
            files[path.relative_to(folder).as_posix()] = path.read_bytes()
    require(sum(map(len,files.values())) <= 64*1024*1024 and len(files) <= 4096, 'P0 evidence exceeds bounds')
    rows = manifest.get('reports', {})
    require(isinstance(rows, dict) and set(rows) == set(requirements()), 'P0 inventory incomplete')
    for row, spec in requirements().items():
        raw = safe_file(files, rows[row].get('report'))
        validate_report(load(raw), row, spec, manifest, files)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(destination, 'x', compression=zipfile.ZIP_DEFLATED) as archive:
        for name, raw in sorted(files.items()):
            archive.writestr(name, raw)
    return {'scope':'packaging only; NOT P0 acceptance or release approval',
            'manifest_sha256':digest(manifest_raw), 'zip_sha256':file_digest(destination)}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inventory', action='store_true')
    parser.add_argument('--version', default='1.3.2-beta.2')
    parser.add_argument('--source-sha')
    parser.add_argument('--bundle', type=Path, help='read-only consolidated evidence ZIP')
    parser.add_argument('--approval', type=Path, help='separately reviewed weekly approval receipt')
    parser.add_argument('--candidate-dir', type=Path, help='actual unpacked release candidate')
    parser.add_argument('--pack', type=Path, help='existing evidence folder; packaging only')
    parser.add_argument('--output', type=Path, help='new ZIP path for --pack')
    parser.add_argument('--live', action='store_true', help='verify current GitHub artifacts (GET only)')
    args = parser.parse_args(argv)
    if args.inventory:
        print(json.dumps({'schema': SCHEMA, 'requirements_sha256': contract_digest(),
                          'requirements': requirements()}, indent=2))
        return 0
    try:
        if args.pack is not None:
            require(args.output is not None, '--pack requires --output')
            print(json.dumps(pack_evidence(args.pack, args.output), indent=2))
            return 0
        require(args.bundle is not None and args.approval is not None and args.candidate_dir is not None,
                'ISSUE97_EVIDENCE_MISSING: select integrated source, candidate packages, evidence and reviewed approval')
        import subprocess
        from publish_open_beta import GitHub
        source_policy = subprocess.check_output(['git', 'show',
            f'{args.source_sha}:Validation/beta-lifecycle-policy.json'], cwd=ROOT)
        assets = {p.name:p for p in args.candidate_dir.iterdir() if p.is_file() and not p.is_symlink()}
        with zipfile.ZipFile(args.bundle) as archive:
            context = context_from_zip(archive, assets, GitHub() if args.live else None, source_policy)
            result = evaluate(**context, version=args.version, source_sha=args.source_sha,
                              approval=load(args.approval.read_bytes()).get('issue97'))
        # P0 evidence alone is never authority to publish. The publisher must
        # additionally pass strict baseline, calendar, approval and package gates.
        result.update(publication_allowed=False, scope='P0 evidence only; use publisher for full decision')
        print(json.dumps(result, indent=2))
        return 0
    except (EvidenceError, ValueError, KeyError, TypeError, OSError, zipfile.BadZipFile) as exc:
        print(json.dumps({'version':args.version,'source_sha':args.source_sha, 'verdict':'BLOCKED',
                          'publication_allowed':False,'blockers':[str(exc)]}, indent=2))
        return 1

if __name__ == '__main__':
    raise SystemExit(main())
