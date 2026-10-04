# Special Edition — 1.1.1 implementation candidate

E670FE is appended as model 23. Iron Compact / Ironball remains model 16 with
unchanged controls and DSP for existing sessions; only its new-selection menu
entry is retired. The candidate has 23 active amps, 24 serialized amps and 83
active AMP/PRE/POST models. This is not a calibrated hardware emulation claim.

The 32-control panel implements four main channels and a separate Tube Driver
path. Modern/Classic and Mega Lo Punch apply only to the four main channels.
Gain Boost/Mid Shift/Bright operate on Clean/Crunch; Hi Gain/Contour/Mid Edge
on Lead I/II. Presence and Master A/B have separate selectors and stored values.
No fictitious Driver gain or selectable output-valve switch is added.

The official manual is the documentary reference; file identity, omissions and
calibration boundaries are recorded in `reference/e670fe-manifest.json`.
T.D. EQ currently inserts an authored passive-style response. Its actual
hardware EQ ownership/taper is unresolved; this implementation must not pass
that reference gate. Internal reverb/noise gate and physical loops/MIDI are
outside the new amp core; the plugin's separate FX/global functions remain.

## Compatibility

- All released models 0–22, native control order/keys/defaults and the legacy
  15-choice parameters and eight-model extension bank remain unchanged.
- The original six-context native registration stops at model 22. The new six
  E670FE banks are appended **after POST**, adding 204 parameters without moving
  released host parameter ordinals.
- E670FE is native-only, including malformed/old enabled flags: it cannot fall
  through into Ironball or a zero-filled legacy voice.
- Every channel/control bank is stored per Classic, Dual A/B, Matrix LOW/MID/HIGH.
- NativeStateTests adds binary recall for all 6 × 5 combinations, inactive-bank
  return, A/B, Ironball recall and an assertion that all new IDs are trailing.
- The existing editor panel/ALL views consume the same native catalog. A distinct
  original chassis image is embedded; original amplifier name is a caption only.

## Evidence and remaining gates

Focused Linux JUCE 8.0.8 DSP/voice runs and their exact source hashes are in
`docs/evidence/e670fe-20261004/`. The current UI/state code was syntax-checked on
Linux; that is not a Windows runtime test. Windows CTest, actual Studio One 8,
Cubase/Sonar automation/close/reopen, exact E670FE capture calibration and
same-DI listening remain unverified until current-build artifacts exist.

Release policy v2 adds E1 implementation/compatibility and E2 reference/sound
hard gates. A successful generic build does not close E2. Old policy-v1 or
previous-commit evidence cannot greenlight this candidate. No public release,
main merge or version-tag replacement is performed by this change.
