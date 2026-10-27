# PR #124 device diagnostics verification — 2026-10-23 KST

Reviewed baseline: `c2afa3534b064f5c0200049bd1b4e6bd892e2a61` (JUCE 8.0.8,
`d6181bde38d858c283c3b7bf699ce6340c050b5d`). This verifies diagnostic and
selection behavior, not the tester's latency/noise resolution or release approval.

## Reproduced defects and fixes

- A registered default ASIO driver that fails `open()` (busy interface) prevented
  selection of a working second ASIO interface. JUCE enumerates installed drivers,
  not just connected hardware. Preserve the backend default first; on failure try
  remaining ASIO drivers, then restore the prior API, rate, buffer and channels.
- Only the driver's xrun counter was recorded. A deliberately slow callback
  produced a manager-measured overrun with no corresponding diagnostic field.
  Retain `xruns` as the raw driver count (`-1` means unsupported); add
  `callback_overruns` from JUCE's manager count minus its driver contribution.
  Do not sum these as independent audible dropouts: both can describe one event.
- A retained but closed device pointer was incorrectly reported `open: true`.
  Read `isOpen()` and expose `running` separately; omit stale metrics when closed.
- Do not derive latency from negative/missing driver values or invalid rates.
  Use double arithmetic to avoid integer overflow when adding samples.

The production app calls the same `StandaloneAudioDevices.h` functions used by
these tests. Only the Windows app invokes the preference helper. Tests replace
AudioIODevice/AudioIODeviceType, not AudioDeviceManager. The original two function
bodies were extracted from the baseline into a temporary header, changing only
holder access to explicit manager/settings/mute arguments. The baseline failed
busy-default selection and callback-overrun reporting; the candidate passed.
The derived-latency helper is new and is tested on the candidate only.

## Automated coverage

- First launch, existing ASIO and WASAPI XML reinitialization; saved rate/buffer/
  channels and byte-for-byte settings preservation; no ASIO scanning on restart.
- Existing empty/malformed settings key and null settings remain untouched.
- ASIO unavailable, no registered drivers, missing/default or busy/default driver,
  working second driver, all candidates failing and exact fallback restoration.
- Actual manager configuration at 48 kHz/256 and 96 kHz/64, sparse input channels,
  both mute values, available API types, 96/160-sample driver delays.
- Driver xruns 3 and -1, deliberately induced callback overrun, nonzero manager
  CPU and exact fraction-to-percent conversion; closed and absent device.
- Private sentinel device/channel names never enter the snapshot.
- Driver + plug-in latency at 44.1/48/96 kHz, zero/negative/nonfinite rates,
  unknown/missing driver delay, closed or absent device.
- Existing processor/UI diagnostic test verifies the null hook, injected hook,
  latency calculation, hook removal, unchanged five-stage diagnostic and no user
  path. VST3/AU/AAX do not install the Standalone hook. Their parameter/state and
  audio processing paths are not changed. Host runtime acceptance is separate.

Run on a provisioned development machine:

```sh
cmake -S . -B build-devices -DCMAKE_BUILD_TYPE=Release -DCHIMERA_BETA_QUALIFICATION_ONLY=ON
cmake --build build-devices --config Release --target ChimeraStandaloneAudioDevicesTests --parallel 2
ctest --test-dir build-devices -C Release -R '^ChimeraStandaloneAudioDevicesTests$' --repeat until-fail:3 --verbose --no-tests=error
```

Local Linux validation compiled the real JUCE core/events/audio_basics/audio_devices
sources with GCC 13.3.0; ALSA/JACK disabled, no audio hardware. `local-candidate.txt`
is the executable's output. Native GUI dependencies are absent in that environment;
product wrappers and full regressions must be built by CI. The focused workflow
runs this test three times on Windows, macOS and Linux. Existing Open Beta Build
and Test builds ASIO-enabled Windows Standalone/VST3 and macOS AU, and runs the new
CTest as part of its full inventory. AAX SDK/signing availability remains the
separate AAX workflow's condition; packaging checks are not Pro Tools acceptance.

## Data semantics and privacy

`device_cpu_percent` is JUCE's smoothed callback-duration / audio-duration ratio,
not machine CPU utilization or peak callback time. Counters are sampled at refresh
and reset when the device is restarted. `driver_plus_plugin_latency_ms` is the
sum of the two driver claims and the processor's reported delay, not measured
round-trip latency and not a substitute for loopback/PDC tests. No audio, device
or channel names, serials, user paths, preset names, driver error strings or
settings XML are added to JSON. Review/copy/save remains manual; no upload added.

## Windows hardware acceptance still required

1. Use an isolated settings profile. Record build SHA/package hash, Windows and
   driver versions privately. Start without `audioSetup`; test connected ASIO,
   disconnected/default driver plus a second connected interface, busy driver,
   and no ASIO. Confirm actual driver control panel and audio continuity.
2. Choose WASAPI explicitly, set rate/buffer/channels, quit normally and reopen;
   repeat with ASIO. Confirm settings/mute persist and no ASIO override occurs.
3. At 48 kHz and 128/256 samples, use identical DI and routing for dry/bypass,
   Classic, Transpose, Dual and Matrix; repeat UI/analyzer on/off. Record snapshot
   before/after, elapsed duration, callback p99/peak/deadline misses, noise floor,
   clip/crackle/dropout vs continuous hiss. Do not use Gate to mask the cause.
4. Measure physical output-to-input loopback separately with direct monitoring
   disabled. Compare baseline/candidate at matched gain/settings. Driver latency
   fields are not a physical round-trip measurement. No measured improvement or
   tester root cause is claimed without these recordings and timing results.
