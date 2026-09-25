# Chimera Amp Matrix 1.0 test plan

Release gate: Windows compile, CTest pass, VST3 + Standalone ZIP and SHA-256 file produced.
macOS and Ubuntu remain compatibility gates; AU is also built on macOS.

## Matrix tone revision

- UI text uses ASCII separators; MSVC source/execution character sets are explicitly UTF-8.
- Classic: one full-range rig; Dual: two full-range rigs. Both retain four-band EQ, Presence and Resonance.
- Matrix: Low/Mid/High input bands, each with Drive, Level and Band Tone; Mute/Solo enable individual auditioning.
- Band Tone is a pre-amp tilt. Negative shifts balance toward the band's lower frequencies; positive toward its upper frequencies. Zero is neutral.
- Pivot tracks the geometric centre between 20 Hz / X1, X1 / X2, and X2 / min(20 kHz, 0.45 * sample rate). These are tone reference points, not additional band limits.
- The amplifier may generate harmonics outside its input band. The displayed ranges describe the crossover input, not a brick-wall output limit.
- Full-range EQ/Presence/Resonance are bypassed in Matrix. Their saved values are retained for Classic/Dual. Band Tone is bypassed in Classic/Dual.
- New Band Tone parameters are appended to the existing list; existing IDs and order stay unchanged. Older sessions load Band Tone at zero.
- Shared filter coefficients are updated in place, fixing stale EQ/cab coefficients after preparation.

Automated DSP checks at 44.1/48/96/192 kHz: neutral response; signed tilt; crossover-following pivot; real engine connection; hidden-control isolation; full-range EQ response; active-lane solo; finite output across all eight amp models and routing modes.

Windows UI/state checks: visible control counts, bounds, text, host-driven mode/crossover changes, state and legacy recall. Render actual Classic, Dual, Matrix, changed-crossover and 150% Matrix PNGs for visual review.

Manual listening remains necessary with guitar and bass DI in the Windows Standalone and VST3 host. These are behavioral amp models; this revision does not claim circuit-exact modelling.
