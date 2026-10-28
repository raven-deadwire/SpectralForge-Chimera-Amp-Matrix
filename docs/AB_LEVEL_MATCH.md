# A/B level match

**MATCH**, next to **A / B / COPY**, measures each slot's output and turns the
louder sound down to the quieter one for a level-matched tone comparison.

**MATCH changes the plugin's output. It is not a separate monitor bus.**
Turn it off before recording, exporting, freezing, resampling or capturing OBS
output. Its first enable in each editor session displays this warning. For
unreported capture, use **SETTINGS > Capture safety: lock MATCH off** before
starting. This session lock survives project restore and audio-device restart,
but is not saved into a project or preset; a new instance starts with MATCH off.

## Use and output protection

1. Set up two sounds in A and B (or COPY one and edit it).
2. Press **MATCH** and acknowledge the output warning.
3. Play the same passage at a similar input level in both slots. Each needs
   1.5 seconds of signal above the measurement gate, after a 250 ms holdoff.
   The button displays the current cut in dB, for example `-6.0`.
4. Turn MATCH off before printing audio. Normal ON/OFF and A/B gain changes
   use a 50 ms linear-gain ramp. After turning it off, allow that ramp to
   finish before starting a print the plugin cannot detect.

When the host reports **recording** or **non-realtime rendering**, Chimera
immediately disarms MATCH and resets its gain to unity **before the first
sample of that callback**. A 50 ms release here would still print attenuated
samples, so this protection intentionally does not ramp. The button shows
`SAFE` while protection is active. MATCH stays off afterward; resuming a
comparison requires explicitly enabling it again. Measurements are paused
while protected so an offline render cannot contaminate the comparison.

This protection depends on host information. A realtime bounce may look like
ordinary playback, and some hosts do not report recording. OBS capture and
external loopback recording are not visible to the plugin. **Those paths
cannot be detected automatically: engage Capture safety manually before
capture, or turn MATCH off and wait for its release to finish.** Stopping or
restarting the audio device is also a new comparison session and turns it off.

## Chimera OBS add-on

The updated add-on **locks MATCH off for the entire hosted instance lifetime**,
including idle/preview, streaming, recording, pause and replay buffer use.
A/B selection and COPY remain available. There is no matched monitor bus.

The host requests the versioned `IChimeraCapturePolicy` VST3 interface during
initialization. Chimera installs its lifetime lock before acknowledging it;
the add-on refuses to load a plugin that does not acknowledge. Update the
VST3 and add-on together. With an unsupported VST3 the existing filter failure
policy applies (mute by default, dry only when explicitly selected), and the
load error explains the required update. The policy does not spoof recording
or non-realtime processing and is never saved in scene/plugin state.

An **older add-on**, third-party host, or desktop/loopback capture cannot supply
this handshake. Do not assume that installing only the new VST3 protects those
paths. Use the manual Capture safety lock there. `SAFE` means that comparison
gain is blocked, **not that recording has been detected** and not a guarantee
against clipping, other gain controls, routing errors or dropouts.

Manual Capture safety takes effect no later than the next plugin callback;
allow downstream host/device queues to drain before starting capture. It does
not undo samples already sent downstream. Unlocking never re-enables MATCH.
Like the host recording guard, it removes an existing cut immediately rather
than printing a release ramp; engaging it during audible playback can step up
by as much as 24 dB. Set it before a take/on-air session and check the level.

## Measurement and state lifecycle

- **Attenuation only:** no boost, maximum cut 24 dB. Applied after the final
  limiter and before the output meter. The meter shows the processed output.
- **Meter:** ITU-R BS.1770 K-weighting, channel powers summed, a running mean
  initially and an approximately 3-second exponential window afterward.
  This is a comparison meter, not a certified integrated-loudness meter or an
  exact rectangular BS.1770/EBU short-term window. Blocks below -60 LUFS are
  excluded. Silence cannot create a valid measurement or pull an estimate
  toward silence. The 250 ms holdoff excludes transition samples exactly,
  including a block that straddles the boundary.
- MATCH is not automated or serialized, and is not part of A or B. Normal
  measuring continues with MATCH off; normal parameter edits converge over
  the measurement window rather than invalidating both slots.

| Event | Measurements | MATCH / output |
| --- | --- | --- |
| A/B switch, including rapid A/B/A | Both retained; filter/holdoff restarted | Current comparison stays enabled; 50 ms gain transition |
| COPY | Destination receives source reading; filter/holdoff restarted | 50 ms transition if needed |
| Factory preset load | Replaced active slot invalidated; other slot retained | No new match until active slot is remeasured; 50 ms release |
| Project/reference restore, including into an existing instance | Both invalidated; pending old COPY discarded | Off; old attenuation removed before the next output block |
| `prepareToPlay`, same or different sample rate/channel layout | Both immediately cleared; pending commands discarded | Off and unity gain |
| Switch to Easy view / close editor | Retained | Off, with normal 50 ms release; no hidden enabled comparison |
| Manual Capture safety / OBS lifetime lock | Paused | Unity on the next callback; rearming blocked; lock survives prepare/restore |
| Host-reported recording / non-realtime render | Paused; existing readings retained | Off and unity from first reported callback; no automatic resume |

With MATCH settled off, with no cut required, or during print protection,
MATCH leaves every output sample untouched. Normal OFF has a 50 ms release;
claiming bit-exact unity during that release would be incorrect. No DSP
parameter IDs, A/B snapshots or saved project schemas are changed.

## Regression and remaining acceptance

`Tests/ABLevelMatchTests.cpp` links the production processor and covers:

- BS.1770 weighting reference, unheard slots and frequency response;
- 12 dB matching, no boost, silence, 24 dB cap and COPY;
- every sample of 50 ms ON/OFF/A/B ramps at 44.1/48/96 kHz, 1/64/257/1024
  sample blocks, mono/stereo;
- device reprepare, preset replacement, A/B/COPY, project restore without
  another prepare call, and output identity with MATCH off;
- explicit capture policy, acknowledgement/refcount balance, unsupported host,
  session/lifetime locks and 24-condition first-block identity after a 24 dB cut;
- simulated host recording/non-realtime flags: bit-exact output from the
  first protected sample, blocked rearming, and no automatic re-enable.

Native processor tests with a simulated playhead do **not** certify an actual
DAW. Windows/macOS builds, VST3/AU/AAX host flag delivery, real instrument A/B
listening, record/punch-in, realtime export, offline bounce/freeze, OBS capture,
and the audible discontinuity when protection immediately removes attenuation
still need actual host/device acceptance. See the PR #183 safety audit for the
source identity and exact executed checks.
