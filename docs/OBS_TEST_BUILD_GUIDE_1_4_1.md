# SpectralForge Chimera OBS filter — test build guide

## About this build

This is an OBS Studio filter that runs **SpectralForge Chimera** amps and effects on an audio source. The filter does not make the sound itself: it loads the Chimera VST3 installed on the same computer.

**This is a pre-release test build.** Try it in a test scene before using it for a live stream or recording. The installer is built to leave your OBS scenes, settings and other plugins untouched even if something goes wrong. Still, we recommend backing up any important OBS profile first.

- Filter version: 0.1.0 (source `5bbc2715df`)
- Download: from the Assets list on the [SpectralForge Chimera 1.4.1-beta.1 release](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.4.1-beta.1) page.
  - macOS: `chimera-obs-macos-universal-5bbc2715df.zip`
  - Linux: `chimera-obs-linux-x86_64-5bbc2715df.tar.gz`
  - Also install the Chimera 1.4.1-beta.1 package (which includes the VST3) from the same page.
- Reply: open a [GitHub issue](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/new) titled `[OBS test] OS — one-line summary`. Issues are public, so hide your user name and folder paths in any log before posting.
- Deadline: **Friday, 4 December 2026**

## Supported environments for this test

| OS | Requirements | OBS it was built and tested with |
|---|---|---|
| **macOS** | macOS 12 or later, Apple silicon or Intel Mac | OBS Studio 31.1.2 |
| **Linux** | x86_64, glibc 2.38 or later (Ubuntu 24.04 level), **OBS installed from your distribution's packages**; the editor window needs X11 (XWayland under Wayland) | OBS Studio 30.0.2 (Ubuntu 24.04) |

- **Flatpak and Snap builds of OBS are not supported on Linux.** They do not read external plugin folders.
- Intel Macs were tested only under Rosetta in CI. **Results from real Intel Macs and from macOS 12, 13 and 14 are especially valuable.**

## Before you start

1. **Install the Chimera VST3.** The filter loads it from:
   - macOS: `/Library/Audio/Plug-Ins/VST3` or `~/Library/Audio/Plug-Ins/VST3`
   - Linux: `~/.vst3`, `/usr/local/lib/vst3` or `/usr/lib/vst3`
2. Install OBS Studio (see the table above).
3. You do **not** need Python or a compiler. The package includes its own installer.

## Check the download (optional)

You can confirm the file was not damaged in transit by comparing its SHA-256.

| File | SHA-256 |
|---|---|
| macOS `chimera-obs-macos-universal-5bbc2715df.zip` | `814423de083c6b3c08e032961a053f43afdf5b432647c0c6e8ab320b68286787` |
| Linux `chimera-obs-linux-x86_64-5bbc2715df.tar.gz` | `6faf089e8543b77dfa616698aa7ac5f8e836c5212dfd97257a9bffb37ff020c2` |

```sh
shasum -a 256 <file>     # macOS
sha256sum <file>         # Linux
```

## Install

**Keep OBS closed while installing, updating or removing the filter.** If OBS is running, the installer stops without changing anything. Do not run the installer as an administrator (root/sudo).

### macOS

1. Quit OBS.
2. Unzip the download, then open Terminal in the unpacked package folder.
3. Run `./install.sh`.
   - It installs to `~/Library/Application Support/obs-studio/plugins/chimera-obs.plugin`.
   - The module is **ad-hoc signed and not yet notarized by Apple.** The installer checks the signature, then removes the download quarantine flag from its own files only. Do not copy the plugin into the folder by hand.

### Linux

1. Quit OBS.
2. Unpack the download (`tar -xzf chimera-obs-linux-x86_64-*.tar.gz`) and open a terminal in the unpacked folder.
3. Run `./install.sh`.
   - It installs to `~/.config/obs-studio/plugins/chimera-obs`.

## Using the filter

1. Start OBS.
2. Right-click an audio source (a microphone or an audio interface input) → **Filters**.
3. Under Audio Filters, click **+** → **SpectralForge Chimera**.
4. Click **Open Chimera Editor** in the filter properties to open the Chimera window. Closing the window keeps the sound, and your edits are saved to the scene.
   **Linux:** the editor window needs X11. If OBS runs on Wayland, XWayland must be available. If the window does not open, the reason is written to the `[chimera-obs]` lines of the OBS log.

Good to know
- If your guitar or bass is plugged into input 1 of your interface, set **Input** to **Left channel to both sides**.
- OBS does not compensate for plugin delay. If audio and video drift apart, adjust the source's **Advanced Audio Properties → Sync Offset**. The filter's **Status** shows the processing delay.
- By default, when Chimera cannot process audio, the filter **mutes** ("Mute (never send unprocessed audio)"), so unprocessed sound never reaches your stream.

## What we would like you to check

1. The installer finishes without errors and **SpectralForge Chimera** appears in OBS's filter list.
2. Audio comes out through Chimera, and whether you notice dropouts, noise or delay problems.
3. Opening, closing and reopening the editor window, resizing it, and whether your edited sound is still there after restarting OBS (on Linux, please tell us whether you use X11 or Wayland).
4. Recording or streaming for 30 minutes or more: any audio dropouts or OBS freezes.
5. Running the installer again with the same package (an update): your scenes and settings stay as they were.
6. After removal (`./install.sh --uninstall`), OBS still works normally.

## Please include in your report

- Your OS and version: for a Mac, Apple silicon or Intel; for Linux, X11 or Wayland.
- Your OBS version and how you installed it (official app, distribution package, etc.).
- **The `[chimera-obs]` line from the OBS log**: in OBS, **Help → Log Files → View Current Log**.
  Example: `[chimera-obs] module 0.1.0 (source 5bbc2715df), built and tested for OBS …, running OBS …`
- If installing or removing went wrong, the full message the terminal showed.
- For sound problems, the audio device, sample rate and buffer size you used.

## Known limitations

- Adding the filter, recording, saving and restoring state, and opening and editing in the editor window inside the real OBS app are covered by automated CI tests (Linux OBS 30.0.2, Windows and macOS OBS 31.1.2). Hands-on use on a real PC, real audio devices and streaming are being tried for the first time in this test.
- macOS: not yet notarized by Apple (ad-hoc signed).
- Linux: the editor window needs X11 (XWayland under Wayland). Flatpak/Snap OBS is not supported.
- Real Intel Macs and macOS 12, 13 and 14 have not been verified yet.
- Recovery from power loss or a failing storage device is not guaranteed.

## Removing the filter

Quit OBS, then run `./install.sh --uninstall` from the package folder you installed from. It removes only the files the installer added. Your OBS scenes and settings, other plugins, the Chimera VST3 and any files you added yourself are left in place.

## Troubleshooting

- **The editor window does not open on Linux:** if OBS runs on Wayland, check that XWayland is installed and running, and send us the `[chimera-obs]` lines from the OBS log.
- **The filter is not in the list:** quit OBS completely and start it again. On Linux, make sure your OBS is not a Flatpak or Snap build.
- **"Chimera VST3 not loaded":** set the VST3 location yourself in the filter's **Chimera VST3 bundle** field.
- **The installer stops saying OBS is running:** quit OBS completely and run the installer again.
- **Installation was interrupted (for example, the terminal was force-closed):** run the same `./install.sh` again and it restores the previous installation. If it says recovery is not possible, do not delete the `.chimera-obs-transaction` folder it names. It holds the backup of your previous installation. Send us that message.
