# Amplifier-head UI review — 11 October 2026

Development: PR #44, stacked on the Signal/EQ integration PR #40. This review
does not change the 1.3.0 release branch, audio processing, parameter IDs or
project schema.

## Knob materials and interaction-only values

Follow-up to the native review and the user's visual references:

- Niflheimr uses a fluted dark-metal body, muted nickel rim, engraved scale
  and warm ivory pointer; Brit Edge uses the same hardware construction with
  a brass cap and dark pointer matching its gold fascia. These are live JUCE
  vector controls, not painted controls or generated mockups.
- Knob centres and their labels align. Labels use bold 12–15 px type,
  scaling with the head width, with light ink on Niflheimr and dark ink on
  Brit Edge. The cleaned original enclosure artwork is retained.
- Idle controls show their parameter name, without a permanent numeric value.
  Dragging temporarily replaces that name with the value in the same space;
  releasing restores the name without moving the knob. Keyboard adjustments
  show the value for 800 ms. Automation alone does not show the readout.
- Double-click, Enter or F2 opens precise entry in the same readout. Commit,
  cancel, hiding or context changes return to the idle presentation. Text entry
  survives normal workspace refreshes. Existing host gesture and parameter-bank
  protection remain in place. The detail panel keeps its permanent values.

The focused contract additionally checks actual value-label visibility, name
visibility, complete glyph widths, stable knob geometry, keyboard expiry,
precise-entry persistence/cancellation and context interruption. Idle and active
captures cover all four workspace sizes, plus `2500 Hz`, `100%` and text entry.
Native validation must refer to the follow-up commit's own CI run; historical
results below do not validate this follow-up.

## Evidence reviewed

- Source: `306fc797ecee93cf4aec1a60aa50ac3ecbf7c35f`.
- Run: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38119085085
- Windows artifact `11694820871`, macOS `11695020182`, Linux `11694870365`.
- Downloaded all three ZIPs and matched their SHA-256 values to GitHub's
  artifact digests. Each contains 26 PNGs, native-test JUnit/logs and build identity.
- These are production JUCE component snapshots attached to native peers.
  They are not desktop/DAW screenshots and are not artwork mockups.

## Findings and changes

| Finding in the original captures | Correction |
| --- | --- |
| Niflheimr has dark crescents and bottom-row fragments around its live knobs. The elliptical mask does not cover the complete baked disk/shadow. | Replace the complete control field, including upper chrome rims, with cached, edge-blended clean metal from the same fascia band, retaining the jack, light, switch and ornate perimeter. |
| At the 1000-pixel popup width, MID FREQ displays `405 ...`; the maximum `2500 Hz` is even wider. The label/value look uses a small 10 px font. | Widen the seven-column control field, retain at least 32 px knob allocation, increase labels/value font to 11 px and reserve more width for values. |
| Physical channel buttons show only 1–5. The current name appears in the caption, but other voices require hovering or trial selection. | Add five named channel buttons, synchronized with the existing physical switches and parameter banks. |
| Brit Edge's five-input selector has no functional label. | Add INPUT ROUTE above the existing selector without changing route order or DSP. |
| Room heads only acquire an outline on hover/focus; the destination is unclear beside cabinet targets. | Add an OPEN AMP caption on head hover/keyboard focus. |

The Niflheimr 14-control order remains Gain/Bass/Mid/Mid Freq/Treble/Presence/
Depth and Mass/Split/Fang/Fold/Thrust/Blend/Master. Brit Edge retains Presence/
Bass/Middle/Treble/Loudness I/Loudness II on its gold fascia.

## Navigation and verification boundaries

- Classic: compact head -> amplifier workspace -> Back/Escape returns to editor.
- Dual: each room head -> its own RIG 1/2 bank; cabinet targets remain separate.
- Matrix: LOW/MID/HIGH heads -> corresponding amplifier bank. Opening the head
  must not change LOW DI/AMP blend, parameters or host gestures.
- Focused CAB: head -> amplifier workspace -> Back returns to the same focused CAB.
- DETAILS and A/B continue to use the existing detail surface and global comparison.

The original editor-75/100/125/150 PNGs really use those editor sizes. However,
their amplifier popup is display-fitted independently: Windows 1000x678,
macOS 1000x642, Linux 1202x678 at every launch scale. They are not evidence of
four distinct amplifier-workspace sizes or OS DPI settings.

The revised contract additionally inspects 1000x642, 1180x650, 1440x780 and
1680x910 workspaces, including maximum frequency/percentage strings, component
containment, non-overlap and actual value-label font widths. It captures all
five named channels and Brit input routes. Room-head and focused-CAB tests now
retain production callbacks and inspect the actual opened workspace and bank;
the previous room test only checked substituted callbacks.

Local compilation and the new three-platform native CI must be checked for
the corrective commit. The old source's passing run does not validate these
changes. Real Studio One drag/automation, save/reopen, shutdown and complete
product acceptance remain separate requirements.
