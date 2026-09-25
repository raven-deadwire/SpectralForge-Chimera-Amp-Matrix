#!/usr/bin/env python3
"""Create the user's private Setup + IR companion archive; never a public CI asset."""
import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import wave
import zipfile


def build(installer, personal, verification, output):
    root = Path(__file__).resolve().parent.parent
    catalog = json.loads((root / "docs/reference/ir-catalog.json").read_text())
    # Validate the complete audio set before creating any output.
    captures = []
    with zipfile.ZipFile(personal) as source:
        for entry in catalog:
            names = [i for i in source.infolist()
                     if PurePosixPath(i.filename.replace("\\", "/")).name == entry["file"]]
            if len(names) != 1 or names[0].file_size > 4 * 1024 * 1024:
                raise ValueError(f"Missing, duplicate or oversized personal IR: {entry['file']}")
            audio = source.read(names[0])
            if hashlib.sha256(audio).hexdigest() != entry["sha256"]:
                raise ValueError(f"IR hash mismatch: {entry['file']}")
            # A catalog row or a hash alone must never masquerade as playable audio.
            with wave.open(io.BytesIO(audio)) as reader:
                channels, width, rate, frames = reader.getnchannels(), reader.getsampwidth(), reader.getframerate(), reader.getnframes()
                pcm = reader.readframes(frames)
                if channels not in (1, 2) or width not in (2, 3, 4) or not (8 <= frames <= rate):
                    raise ValueError(f"Unsupported IR format: {entry['file']}")
                if rate != entry["sample_rate"] or frames != entry["frames"] or len(pcm) != frames * channels * width or not any(pcm):
                    raise ValueError(f"IR audio validation failed: {entry['file']}")
            folder = "Bass" if entry["notes"].startswith("Bass") else "Guitar"
            captures.append((f"Chimera-Personal-IRs/{folder}/{entry['file']}", audio, entry))
    setup_hash = hashlib.sha256(installer.read_bytes()).hexdigest()
    checksum = installer.with_name(installer.name + ".sha256.txt")
    if not checksum.exists() or checksum.read_text(encoding="utf-8-sig").split()[0] != setup_hash:
        raise ValueError("Installer does not match the validated checksum")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".partial")
    with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as package:
        package.write(installer, installer.name)
        package.write(checksum, checksum.name)
        package.write(root / "docs/WINDOWS_INSTALL.txt", "INSTALL_KO.txt")
        package.write(root / "docs/WINDOWS_SIGNING.md", "Publisher-signing.md")
        package.write(verification, "InstallerVerification.txt")
        package.writestr("IR-file-verification.json", json.dumps({"personal_ir_count": len(captures),
            "factory_ir_count": 2, "installer_sha256": setup_hash,
            "captures": [metadata for _, _, metadata in captures]}, indent=2) + "\n")
        for path, audio, metadata in captures:
            package.writestr(path, audio)
            package.writestr(path + ".json", json.dumps(metadata, indent=2) + "\n")
    temporary.replace(output)
    print(f"Created {output.name}: {len(captures)} verified personal IRs; installer SHA256 {setup_hash}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("installer", "personal", "verification", "output"):
        parser.add_argument("--" + arg, type=Path, required=True)
    options = parser.parse_args()
    build(**vars(options))
