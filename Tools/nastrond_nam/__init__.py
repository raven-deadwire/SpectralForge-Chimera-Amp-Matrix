"""Náströnd NAM reference toolchain: one protocol, one manifest reading, one report.

Modules:
  protocol   probe layout and every time/level constant derived from it
  references manifest validation and macro target groups derived from roles
  analysis   level/THD measurement and spectral distances (legacy and log-band)
  renderers  content-addressed renderer cache for OriginalAmp and NAM outputs
  states     the frozen v1 state suite and validated additional states
  compare    produce comparison.json (the frozen v1 report plus additions)
  summary    gain CSV and macro-direction audit for any compatible reports

Reports observations only: never hardware equivalence or listening acceptance.
"""
