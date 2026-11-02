# OVERSAMPLING Auto

Status: **on `claude/integration-1-10`**. Owner decision 10/31: option (a) from [WHOLE_PRODUCT_2_0.md](WHOLE_PRODUCT_2_0.md). Covered by `ChimeraOversamplingAutoTests`.

## What it does

The OVERSAMPLING menu gains **Auto**, the default for new instances. Auto picks the factor from the host rate, so the amps run at about the same internal rate everywhere:

| Host rate | Auto runs | Internal amp rate |
|---|---|---|
| 44.1 / 48 kHz | 4× (the released default) | 176.4 / 192 kHz |
| 88.2 / 96 kHz | 2× | 176.4 / 192 kHz |
| 176.4 / 192 kHz | 1× | 176.4 / 192 kHz |

- At every rate, Auto is bit for bit the fixed factor it picks.
- At 44.1 / 48 kHz a new instance therefore sounds exactly like the released default.
- The host latency is the same with Auto or any fixed factor (42 samples). The amps already report a constant latency across factors.
- The menu shows the factor in use, e.g. `Auto (2x)`.

## Compatibility

- **The released `oversampling` parameter is unchanged:** same 1×/2×/4×/8× values, ID and automation. Auto is a separate parameter, `oversamplingAuto` (bool), appended at version hint 33.
- **Sessions saved before Auto existed** restore with Auto **off** and their stored factor. A 96 kHz project saved at 4× still runs 4×, so existing sessions sound as saved.
- **A saved Auto** restores on. A saved manual choice restores as saved.
- **A change to `oversampling`** from the menu, the host or automation turns Auto off. Restoring a session or switching A/B does not.
  - The audio follows the new choice at once.
  - The Auto switch itself is written right after, on the message thread. A host write arrives inside the plugin wrapper's callback, where VST3 and AU do not pass a nested parameter change on to the host, so the switch must wait until that callback ends; the host then sees Auto turn off.
  - A state saved in between already records Auto off.
- **A/B slots** each keep their own Auto choice.
- **Header AUTO SET (Standalone):** while the muted-input guide covers the header, the header AUTO SET button is hidden. Auto Set cannot listen to a muted input.
- **Factory presets** initialise the whole sound, OVERSAMPLING included. They used to write 4×; they now write the new-instance default, Auto.
  - At 44.1/48 kHz that is still 4×, so presets sound as before.
  - At 88.2 kHz and up, a factory preset now runs 2× (1× from 176.4 kHz) instead of 4×. This is the one released behaviour that changes, and only when a preset is loaded.

## Measured effect (96 kHz / 128, Matrix, this CI VM)

- **New instance in Matrix (Auto = 2×):** 64% of real time, against 87–95% for the released 4× defaults.
- **Everything on with 2×:** 98–102% of real time, against 121–133% at 4×.

## Tests (`ChimeraOversamplingAutoTests`)

- **Auto by rate:** at 44.1, 48, 88.2, 96, 176.4 and 192 kHz, Auto picks 4×/4×/2×/2×/1×/1×. Its Matrix render equals the fixed factor bit for bit, with the same latency.
- **New instance at 48 kHz:** identical to the released default.
- **Legacy sessions:** a session without `oversamplingAuto`, with each stored factor 1×–8×, restores with Auto off and that factor, at 96 kHz too.
- **Saved and changed values:** a saved Auto restores on. A host change of `oversampling` turns Auto off and is saved that way.
- **Host notification:** a wrapper-style host write (nested changes hidden from the host during its callback) is used by the audio at once. The host is then told that Auto turned off, and a state saved in between restores Auto off.
- **A/B and presets:** A/B slots keep their own choice. A factory preset resets to Auto.
