"""Derived evidence: the gain CSV and the macro-direction audit.

Both used to be produced by hand-written one-off code (three evidence folders
hold two different CSV layouts, and the channel summary has no generator).
They are now pure functions of comparison.json reports, so every published
summary can be regenerated and checked.
"""
import csv
import io
import math
from statistics import mean

from . import protocol

METRICS = {
    'log_spectrum_rms_db': 'RMS-matched Welch 65-8000 Hz',
    'band_spectrum_rms_db': 'RMS-matched Welch averaged into 42 logarithmic bands 65-8000 Hz',
}
GAIN_COLUMNS = ['kind', 'family_or_state', 'model_id', 'name', 'variant', 'scope', 'gain_group', 'hz',
                'growth_24db_input_db', 'thd_at_minus36_ratio']


def gain_csv(report):
    out = io.StringIO()
    writer = csv.writer(out, lineterminator='\n')
    writer.writerow(GAIN_COLUMNS)
    thd_index = protocol.LEVELS.index(protocol.THD_LEVEL)

    def rows(prefix, measurements):
        for response in measurements['response']:
            writer.writerow(prefix + [response['hz'], repr(response['growth_24db_input_db']),
                                      repr(response['readings'][thd_index]['thd_2_to_12'])])

    for name, item in report['original'].items():
        rows(['Original', name, '', name, 'digital_equal', 'amp_head', 'authored'], item['measurements'])
    for ref in report['references']:
        for variant, data in ref['variants'].items():
            rows(['NAM', ref['family'], ref['model_id'], ref['name'], variant, ref['capture_scope'],
                  ref['gain_group']], data['measurements'])
    return out.getvalue()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def reference_identity(report):
    return {r['model_id']: (r['family'], r['sha256'], r['gain_group'], r['capture_scope'],
                           {v: (data['input_gain_db'], data['audio_sha256'])
                            for v, data in r['variants'].items()}) for r in report['references']}


def state_names(scope, macro):
    default = scope or 'default'
    prefix = scope + '_' if scope else ''
    return default, f'{prefix}{macro}_0', f'{prefix}{macro}_1'


def audit(reports, groups, metric='log_spectrum_rms_db', scope=None,
          min_improvement_db=.1, max_default_drift_db=.001):
    """reports: {'baseline', 'incoming', 'candidate'} comparison reports, in that order."""
    require(list(reports) == ['baseline', 'incoming', 'candidate'], 'Need baseline, incoming, candidate')
    require(metric in METRICS, f'Unknown metric {metric}')
    baseline = reports['baseline']
    identity = reference_identity(baseline)
    for label, report in reports.items():
        for key in ['manifest_sha256', 'probe_sha256', 'nam_renderer_sha256', 'protocol']:
            require(report[key] == baseline[key], f'{label}: incompatible {key}')
        require(reference_identity(report) == identity, f'{label}: changed NAM identity/render')
        # Every report keeps the frozen v1 suite; later reports may add states.
        for state, item in baseline['original'].items():
            require(state in report['original'] and report['original'][state]['state'] == item['state'],
                    f'{label}: changed state {state}')

    candidate = reports['candidate']
    macros = [m for m in groups if all(s in candidate['original'] for s in state_names(scope, m))]
    require(macros, f'No macro endpoint states in scope {scope or "default"}')
    if scope is None:
        require(macros == list(groups), 'Default scope must cover every target group')
    for macro in macros:
        for state in state_names(scope, macro):
            definitions = {repr(r['original'].get(state, {}).get('state')) for r in reports.values()}
            require(len(definitions) == 1, f'State {state} missing or redefined between reports')

    result_groups, failed = {}, []
    for macro in macros:
        family, ids = groups[macro]
        names = state_names(scope, macro)
        group = dict(family=family, model_ids=ids, measurements={})
        for label, report in reports.items():
            refs = {r['model_id']: r for r in report['references']}
            observations = {}
            for segment in protocol.SEGMENTS:
                per_reference = []
                for model_id in ids:
                    require(refs[model_id]['family'] == family, 'Wrong target family')
                    comparisons = refs[model_id]['variants']['digital_equal']['spectral_comparisons']
                    require(all(metric in comparisons[s][segment] for s in names),
                            f'{label}: no {metric}; regenerate it with analysis_version 2')
                    scores = {state: comparisons[state][segment][metric] for state in names}
                    require(all(math.isfinite(v) for v in scores.values()), 'Nonfinite score')
                    improvement = scores[names[1]] - scores[names[2]]
                    per_reference.append(dict(model_id=model_id, scores_db=scores,
                                              endpoint_improvement_db=improvement))
                    if label == 'candidate' and improvement < min_improvement_db:
                        failed.append(f'{macro}/{model_id}/{segment}: {improvement:.6f} dB')
                observations[segment] = dict(
                    mean_scores_db={state: mean(r['scores_db'][state] for r in per_reference) for state in names},
                    per_reference=per_reference)
            group['measurements'][label] = observations
        result_groups[macro] = group

    # Neutral gain and macro settings must not be used to inflate improvements.
    drift = {}
    default = state_names(scope, macros[0])[0]
    for segment in protocol.SEGMENTS:
        a = reports['incoming']['original'][default]['measurements']['synthetic_notes'][segment]
        b = candidate['original'][default]['measurements']['synthetic_notes'][segment]
        drift[segment] = b['rms_dbfs'] - a['rms_dbfs']
        require(abs(drift[segment]) < max_default_drift_db, f'Default {segment} RMS drift')
    return dict(
        schema=1, reference_count=len(identity), original_states=len(candidate['original']),
        protocol=f'Unchanged digital input and NAM outputs; {METRICS[metric]}; '
                 f'each target capture must improve by >={min_improvement_db:g} dB from macro 0 to 1 '
                 'on both synthetic pluck and chord. No hardware-match threshold.',
        metric=metric, scope=scope or 'default',
        default_rms_drift_from_incoming_db=drift,
        groups=result_groups,
        status=dict(synthetic_endpoint_direction='PASS' if not failed else 'FAIL',
                    clank_amp_only_reference='BLOCKED: Meshuggah captures include cabinet',
                    instrument_di_listening='NOT_RUN', hardware_matching='BLOCKED',
                    final_voicing='BLOCKED: owner instrument-DI listening required'),
        failures=failed)
