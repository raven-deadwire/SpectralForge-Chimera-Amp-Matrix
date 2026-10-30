"""Descriptive DI summaries only; never alter T1-T4 gates or tune PDC."""
import numpy as np

PDC_MS = 2.5


def stats(values):
    if not values:
        return {'events': 0, 'min_ms': None, 'max_ms': None, 'max_abs_ms': None,
                'p5_ms': None, 'p95_ms': None}
    x = np.asarray(values, dtype=float)
    return dict(events=len(x), min_ms=float(x.min()), max_ms=float(x.max()),
                max_abs_ms=float(np.abs(x).max()), p5_ms=float(np.percentile(x, 5)),
                p95_ms=float(np.percentile(x, 95)))


def summarize(rows, snr_min=10.):
    def group(selected):
        rate = selected[0]['rate']
        samples = round(PDC_MS*rate/1000)
        actual_ms = samples*1000/rate
        pairs = [(v, snr) for r in selected for v, snr in
                 zip(r.get('attack_additional_ms', []), r.get('attack_event_snr_db', []))]
        low = [v for r in selected for v in r.get('lowband_additional_ms', [])]
        attacks = {}
        for name, values in (
            ('all_eligible', [v for v, _ in pairs]),
            ('snr_at_least_threshold', [v for v, snr in pairs if snr >= snr_min]),
            ('faint', [v for v, snr in pairs if snr < snr_min]),
        ):
            attacks[name] = dict(added=stats(values),
                                 residual_nominal_2_5ms=stats([v-PDC_MS for v in values]),
                                 residual_reported_samples=stats([v-actual_ms for v in values]))
        return dict(rate=rate, pdc_samples=samples, pdc_actual_ms=actual_ms, attack=attacks,
                    lowband=dict(added=stats(low),
                                 residual_nominal_2_5ms=stats([v-PDC_MS for v in low]),
                                 residual_reported_samples=stats([v-actual_ms for v in low])))

    by_rate = {str(rate): group([r for r in rows if r['rate'] == rate])
               for rate in sorted({r['rate'] for r in rows})}
    keys = sorted({(r['take'], r['instrument'], r['segment'], r['rate'], r['shift']) for r in rows})
    detailed = []
    for take, instrument, segment, rate, shift in keys:
        sel = [r for r in rows if (r['take'], r['instrument'], r['segment'], r['rate'], r['shift'])
               == (take, instrument, segment, rate, shift)]
        detailed.append(dict(take=take, instrument=instrument, segment=segment, shift=shift, **group(sel)))
    return dict(status='DESCRIPTIVE_ONLY_NOT_DAW_VALIDATION', nominal_pdc_ms=PDC_MS,
                attack_reference_limit_ms=4.5, event_snr_min_db=snr_min,
                note='Same DI event eligibility and raw lags; no new verdict. Historical synthetic PDC used whole-input eligibility. Faint events retained. Lowband has no DI upshift ceiling.',
                by_rate=by_rate, by_take_segment_shift=detailed)
