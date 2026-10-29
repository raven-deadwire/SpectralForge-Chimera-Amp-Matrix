failing-input-trace.json: marker_trace.py output (its marker_snr_db column uses the first SNR definition without the 3 ms gap; the report uses event_snr.py / Tools/TransposeNext/attack_snr.py).
random-marker-population-v3.json.gz: marker_population.py output (same caveat for snr_db; floor ratios are unaffected).
event-snr-table.json.gz: per-event SNR (final definition) keyed rate/input/event.
snr-rule-summary.json: residual by SNR bin and by validity rule over the v3 PDC study events.
