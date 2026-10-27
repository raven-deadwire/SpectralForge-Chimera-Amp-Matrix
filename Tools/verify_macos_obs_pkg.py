#!/usr/bin/env python3
"""Exercise the real product PKG on an empty disposable macOS Actions runner.

Apple Installer CLI, code seals, ownership and preservation checks only. This
does not certify the Installer GUI, Developer ID, notarization or physical Macs.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import subprocess
import sys
import tempfile

PRODUCT = "SpectralForge Chimera"
ROOT = Path(__file__).resolve().parents[1]
STAGE = Path("/Library/Application Support/SpectralForge/Chimera/OBS")
BUNDLES = [Path("/Applications") / (PRODUCT + ".app"),
           Path("/Library/Audio/Plug-Ins/VST3") / (PRODUCT + ".vst3"),
           Path("/Library/Audio/Plug-Ins/Components") / (PRODUCT + ".component")]
MANUAL = Path("/Library/Application Support/SpectralForge/Chimera/Documentation/MANUAL.html")


def require(value, message):
    if not value:
        raise RuntimeError(message)


def digest(paths):
    return {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for path in paths for p in ([path] if path.is_file() else path.rglob("*")) if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--arm-build", type=Path, required=True)
    parser.add_argument("--intel-build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    require(sys.platform == "darwin" and os.geteuid() != 0 and os.environ.get("GITHUB_ACTIONS") == "true"
            and os.environ.get("RUNNER_OS") == "macOS" and os.environ.get("RUNNER_TEMP"),
            "Only an ordinary user on a disposable macOS Actions runner may run this test")
    args.output.mkdir(parents=True, exist_ok=True)
    report = {"success": False, "source_sha": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
              "checks": [], "limits": ["No GUI or hardware acceptance", "Ad-hoc seals only; no Developer ID/notarization",
                  "Intel executes under Rosetta", "Not a whole-PKG rollback/power-loss guarantee"]}

    def run(label, *command, ok=True):
        result = subprocess.run([str(x) for x in command], capture_output=True, text=True, timeout=180)
        (args.output / (label + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
        require((result.returncode == 0) == ok, f"{label}: unexpected exit {result.returncode}; see log")
        return result.stdout

    def passed(check):
        report["checks"].append(check)
        print("PASS: " + check, flush=True)

    plugin = Path.home() / "Library/Application Support/obs-studio/plugins/chimera-obs.plugin"
    obs_app = Path("/Applications/OBS.app")
    protected = [Path.home() / "Library/Application Support/obs-studio/basic/scenes/chimera-pkg-fixture.json",
                 plugin.parent / "chimera-pkg-other-plugin/sentinel.txt",
                 Path.home() / "Library/Application Support/SpectralForge/Chimera/presets/chimera-pkg-fixture.json"]
    try:
        console_user = subprocess.check_output(["stat", "-f", "%Su", "/dev/console"], text=True).strip()
        require(console_user == subprocess.check_output(["id", "-un"], text=True).strip(), "Console user differs from CI user")
        for path in [*BUNDLES, MANUAL.parent, STAGE, plugin, obs_app, *protected]:
            require(not path.exists() and not path.is_symlink(), f"Refusing existing user/product path: {path}")
        for path in protected:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"preserve user data\x00\xff")
        protected_before = digest(protected)
        with tempfile.TemporaryDirectory(prefix="chimera-pkg-check-", dir=os.environ["RUNNER_TEMP"]) as temp:
            work = Path(temp)
            choices = {}
            for selected in (0, 1):
                choices[selected] = work / f"choice-{selected}.plist"
                choices[selected].write_bytes(plistlib.dumps([{"choiceIdentifier": "obs", "choiceAttribute": "selected",
                                                               "attributeSetting": selected}]))

            def check_choice(label, expected):
                data = plistlib.loads(run(label, "/usr/sbin/installer", "-pkg", args.package,
                                          "-target", "/", "-showChoicesXML").encode())
                # -showChoicesXML nests choices (and their packages) under
                # childItems; index every level by identifier.
                rows, pending = {}, list(data)
                while pending:
                    row = pending.pop()
                    rows.setdefault(row.get("choiceIdentifier"), row)
                    pending.extend(row.get("childItems") or [])
                require({"chimera", "obs"} <= rows.keys(),
                        f"{label}: choices chimera/obs not listed; found {sorted(map(str, rows))}")
                require(bool(rows["obs"]["choiceIsSelected"]) == expected, f"{label}: wrong OBS selection")
                require(rows["chimera"]["choiceIsSelected"] and not rows["chimera"]["choiceIsEnabled"],
                        "Mandatory Chimera choice is not locked on")

            def install(label, selected, ok=True):
                return run(label, "sudo", "/usr/sbin/installer", "-pkg", args.package, "-target", "/",
                           "-applyChoiceChangesXML", choices[selected], ok=ok)

            def verify_core():
                for bundle in BUNDLES:
                    require((bundle / "Contents/MacOS" / PRODUCT).is_file(), f"Missing core binary: {bundle}")
                    run("seal-" + bundle.suffix.lstrip("."), "codesign", "--verify", "--deep", "--strict", bundle)
                require(MANUAL.is_file(), "Offline manual missing")

            check_choice("choice-no-obs", False)
            run("create-obs-app-fixture", "sudo", "mkdir", str(obs_app))
            try:
                check_choice("choice-obs-installed", True)
            finally:
                run("remove-obs-app-fixture", "sudo", "rmdir", str(obs_app))
            plugin.mkdir(parents=True)
            try:
                check_choice("choice-existing-filter", True)
            finally:
                plugin.rmdir()
            passed("Apple choice evaluator: absent off; OBS present/existing filter on; core always on")
            install("install-obs-off", 0)
            verify_core()
            require(not plugin.exists() and not STAGE.exists(), "Deselected OBS payload was installed")
            passed("OBS deselected: Standalone/VST3/AU/manual installed; OBS absent")
            install("install-obs-on", 1)
            verify_core()
            require(plugin.is_dir(), "Selected OBS plugin missing")
            require(all(p.stat().st_uid == os.getuid() for p in [plugin, *plugin.rglob("*")]), "Root-owned OBS files")
            run("obs-seal", "codesign", "--verify", "--deep", "--strict", plugin)
            for arch, build in (("arm64", args.arm_build), ("x86_64", args.intel_build)):
                run("obs-load-" + arch, build / "chimera-obs-layout-tests",
                    plugin.parent / "%module%.plugin/Contents/MacOS", plugin.parent / "%module%.plugin/Contents/Resources")
            passed("Selected OBS: console-user ownership, ad-hoc seal, arm64 and Intel/Rosetta libobs loading")
            original = digest([*BUNDLES, MANUAL, STAGE, plugin])
            install("existing-filter-explicit-off", 0)
            require(digest([*BUNDLES, MANUAL, STAGE, plugin]) == original, "Deselecting OBS removed/changed its existing install")
            passed("Explicit deselection preserves an existing OBS installation and all core products")

            # A process with OBS's executable name exercises the native guard;
            # it is not a real OBS UI/audio acceptance test.
            source = work / "obs.c"
            source.write_text('#include <stdio.h>\n#include <unistd.h>\nint main(void){puts("ready");fflush(stdout);for(;;)pause();}\n')
            run("build-process-fixture", "cc", source, "-o", work / "obs")
            process = subprocess.Popen([str(work / "obs")], stdout=subprocess.PIPE, text=True)
            try:
                require(process.stdout.readline().strip() == "ready", "Process fixture did not start")
                for selected in (0, 1):
                    install(f"obs-running-choice-{selected}", selected, ok=False)
                    require(digest([*BUNDLES, MANUAL, STAGE, plugin]) == original, "Blocked installation changed existing bytes")
            finally:
                process.terminate()
                process.wait(timeout=10)
                process.stdout.close()
            passed("Running OBS-named process blocks both choice states before existing product bytes change")
            # The packaged helper must reject corrupt updates without replacing
            # the installed filter; restore the test payload even on assertion failure.
            manifest = STAGE / "payload-manifest.txt"
            run("save-manifest", "sudo", "cp", manifest, work / "manifest.txt")
            try:
                run("corrupt-manifest", "sudo", "cp", source, manifest)
                run("reject-corrupt-update", "/bin/sh", STAGE / "install.sh", ok=False)
                require(digest([plugin]) == {k: v for k, v in original.items() if k.startswith(str(plugin) + "/")},
                        "Failed update changed installed OBS bytes")
            finally:
                run("restore-manifest", "sudo", "cp", work / "manifest.txt", manifest)
            passed("Packaged helper rejects corrupt update and preserves previous filter")
            run("uninstall-obs", "/bin/sh", STAGE / "install.sh", "--uninstall")
            require(not plugin.exists(), "Owned filter remains after removal")
            verify_core()
            require(digest(protected) == protected_before, "User scenes, other plugin or preset changed")
            passed("Removal preserves Chimera app/VST3/AU/manual, scenes, another plugin and personal preset")
        report["success"] = True
    finally:
        (args.output / "result.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
