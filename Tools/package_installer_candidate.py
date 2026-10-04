#!/usr/bin/env python3
"""Stage an exact-commit Windows Setup, then transfer its verified bytes privately.

Uses the existing release payload and Inno installation contract without creating
or replacing a beta release/tag. Personal IR audio is supplied only beside Setup.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

import package_release as release

ROOT = Path(__file__).resolve().parents[1]


def revision() -> str:
    return subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()


def stage() -> None:
    sha = revision()
    destination = ROOT / "installer-candidate/payload"
    destination.mkdir(parents=True, exist_ok=False)
    build = ROOT / "build"
    artifacts = build / "ChimeraAmpMatrix_artefacts/Release"
    for name in ("Standalone", "VST3"):
        release.copy(artifacts / name, destination / name)
    release.documents(destination, build)
    for name in ("GRAPHICS_DSP_UPDATE.md", "WINDOWS_SIGNING.md", "ARTWORK_PROMPTS.json",
                 "UPDATE_TEST_BUILD.md", "PEDAL_BOARD_DSP.md", "NEW_AMP_DSP.md",
                 "AMP_NATIVE_DSP.md", "POST_NATIVE_DSP.md", "RAVEN_IR.md", "STUDIO_ONE_TEARDOWN.md"):
        release.copy(ROOT / "docs" / name, destination / name)
    release.copy(ROOT / "Tools/validate_nam.py", destination / "ReferenceTools/validate_nam.py")
    release.copy(ROOT / "Tools/Trace-Chimera-Session.ps1", destination / "ReferenceTools/Trace-Chimera-Session.ps1")
    release.copy(build / "ChimeraRender_artefacts/Release/ChimeraRender.exe",
                 destination / "ReferenceTools/ChimeraRender.exe")
    release.copy(build / "reference-audio", destination / "reference-audio")
    (destination / "README.txt").write_text(
        f"SpectralForge Chimera — Windows update preview {sha[:10]}\n"
        f"Source commit: {sha}\n\n"
        "Close Chimera and all DAWs, extract the entire personal bundle, then run Setup.\n"
        "Setup offers separate application and VST3 folders, and remembers the VST3 folder for repair.\n"
        "The standard VST3 folder is recommended; a custom folder must be scanned by your DAW.\n"
        "Keep Chimera-Personal-IRs beside Setup to install your supplied Raven IR collection.\n"
        "Personal IRs go to ProgramData/SpectralForge/Chimera/IRs and survive uninstall.\n"
        "This is an unsigned, separately verified preview; existing public beta assets are unchanged.\n"
        "SETTINGS shows this commit. Verification.txt records automated checks, not DAW listening approval.\n",
        encoding="utf-8")
    (destination / "WINDOWS_INSTALL.txt").write_text(
        f"SpectralForge Chimera — Windows 업데이트 {sha[:10]} 설치 안내\n\n"
        "1. Studio One 등 DAW와 Chimera를 종료합니다.\n"
        "2. 받은 압축을 전체 해제하고 Setup.exe를 실행합니다.\n"
        "   Chimera-Personal-IRs 폴더를 Setup과 같은 위치에 유지하세요.\n"
        "3. 앱 설치 경로와 설치할 구성 요소를 선택합니다.\n"
        "4. VST3 설치 경로를 선택합니다. 기본값은 C:\\Program Files\\Common Files\\VST3입니다.\n"
        "   사용자 지정 경로는 DAW의 플러그인 검색 경로에도 추가하세요.\n"
        "5. 설치 후 DAW에서 VST3를 재검색합니다. SETTINGS의 build 값을 확인하세요.\n"
        "6. 동봉된 개인 IR은 C:\\ProgramData\\SpectralForge\\Chimera\\IRs에 설치됩니다.\n"
        "   IR LIBRARY에서 상태를 확인하고 원하는 레인의 CAB에서 선택합니다.\n\n"
        "같은 Setup을 다시 실행하면 설치 경로를 기억하고 구성 파일을 복구합니다.\n"
        "기존 사용자 IR을 덮어쓰지 않으며 앱 제거 후에도 개인 IR과 프리셋은 보존합니다.\n"
        "VST3 폴더를 변경하면 이전 설치 경로의 제품 바이너리만 정리합니다.\n"
        "코드서명 전 테스트 설치 파일입니다. SHA256과 동봉된 검사 결과를 확인하세요.\n",
        encoding="utf-8")
    release.VERSION = f"1.1.0-preview.{sha[:10]}"
    release.verify_stage(destination, ["Standalone/SpectralForge Chimera.exe", "MANUAL.html",
        "Verification.txt", "VST3/SpectralForge Chimera.vst3/Contents/x86_64-win/SpectralForge Chimera.vst3"])
    path = destination / "payload-manifest.json"
    manifest = json.loads(path.read_text(encoding="utf-8"))
    manifest.update(source_sha=sha, run_id=os.environ.get("GITHUB_RUN_ID"), installer_version="1.1.0",
                    published_release=False, publisher_signed=False, personal_ir_audio_bundled=False)
    path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"stage": str(destination), "source_sha": sha, "build_id": sha[:10]}))


def transfer() -> None:
    sha = revision()
    artifact = ROOT / "installer-candidate" / f"SpectralForge-Chimera-update-{sha[:10]}-win64-Setup.exe"
    checksum = artifact.with_name(artifact.name + ".sha256.txt")
    digest = release.sha256(artifact)
    if checksum.read_text(encoding="utf-8-sig").split()[0] != digest:
        raise RuntimeError("Installer checksum differs after verification")
    evidence = ROOT / "build/candidate-installer-verification"
    receipt = json.loads((evidence / "InstallerVerification.json").read_text(encoding="utf-8-sig"))
    if not receipt["success"] or receipt["source_sha"] != sha or receipt["installer_sha256"] != digest:
        raise RuntimeError("Installer verification does not match this exact source/binary")
    data = artifact.read_bytes()
    size = 20 * 1024 * 1024
    if len(data) > 4 * size:
        raise RuntimeError("Installer needs more than the configured four transfer artifacts")
    records = []
    output = ROOT / "installer-transfer"
    for index, offset in enumerate(range(0, len(data), size), 1):
        folder = output / f"part-{index}"
        folder.mkdir(parents=True, exist_ok=False)
        path = folder / f"{artifact.name}.part{index}"
        path.write_bytes(data[offset:offset + size])
        records.append({"name": path.name, "bytes": path.stat().st_size, "sha256": release.sha256(path)})
    metadata = output / "metadata"
    metadata.mkdir(parents=True)
    manifest = {"source_sha": sha, "run_id": os.environ.get("GITHUB_RUN_ID"), "filename": artifact.name,
                "bytes": len(data), "sha256": digest, "parts": records, "publisher_signed": False,
                "published_release": False, "installer_verification": receipt}
    (metadata / "installer-parts-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    for path in (checksum, evidence / "InstallerVerification.txt", evidence / "InstallerVerification.json",
                 ROOT / "installer-candidate/payload/payload-manifest.json",
                 ROOT / "installer-candidate/payload/WINDOWS_INSTALL.txt",
                 ROOT / "build/candidate-installer-security/WindowsSecurity.txt"):
        shutil.copy2(path, metadata / path.name)
    print(json.dumps({"installer": artifact.name, "source_sha": sha, "sha256": digest, "parts": len(records)}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("stage", "transfer"))
    args = parser.parse_args()
    globals()[args.operation]()
