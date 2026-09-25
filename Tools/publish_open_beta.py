#!/usr/bin/env python3
"""Publish only the approved, immutable Open Beta 1 candidate from CI build 136.

No compilation, replacement uploads, tag movement, or release deletion occurs.
--verify-only --candidate-zip PATH performs all local checks without network writes.
"""

import argparse
import hashlib
import json
import mimetypes
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
import zipfile


REPO = "raven-deadwire/SpectralForge-Chimera-Amp-Matrix"
VERSION = "1.0.0-beta.1"
TAG = "v" + VERSION
TITLE = "SpectralForge Chimera 1.0 — Open Beta 1"
HEAD = "95b4091af2c914188ea32672f05e00544a1d2098"
REVISION = "d23d46d015fe6cd10621bc628b63145146d042c1"
TREE = "b5865c7967fd78d743b72c0789e1ca014bdece21"
RUN = 36107249747
ARTIFACT = 10851512877
ARTIFACT_NAME = "SpectralForge-Chimera-Open-Beta-1.0-Release-Candidate"
ARCHIVE_SIZE = 256590746
ARCHIVE_SHA = "6585473401c8f34959bdb4e17e4cb5ce599108b1201157845fe60afbc8beaa34"
PREFIX = f"SpectralForge-Chimera-{VERSION}-"
BINARY_HASHES = {
    PREFIX + "win64-Setup.exe": "29f1e12501ff64140879b0d17eea15b48aa881969388ad35a5eab2fd0d99c899",
    PREFIX + "win64.zip": "b02e534f48c13078d0e5f7926a3ee568498ddde00ce08a9150c96b4e1cf4b1bb",
    PREFIX + "macos-universal.pkg": "0371fc9bfebd1cb3762c12ab65d8aece6f442e093078d6bc3d70c46ac10178e7",
    PREFIX + "linux-x86_64.deb": "16d40d5b0e9c10eaf0f652261ed7401951f5efbe2b49e5ea388d2501afb680e1",
    PREFIX + "linux-x86_64.tar.gz": "1ed700ecf8d24482581ae3581f31656186d2b497e3c4621059b7a638c561a4c9",
}
DOCUMENTS = (
    "SHA256SUMS.txt", "update-beta.json", "MANUAL.html", "INSTALLATION.md",
    "THIRD_PARTY_NOTICES.md", "candidate-source.json", "InstallerVerification.txt",
    "macOS-package-verification.txt", "Linux-package-verification.txt",
)
RELEASE_URL = f"https://github.com/{REPO}/releases/tag/{TAG}"
DOWNLOAD_URL = f"https://github.com/{REPO}/releases/download/{TAG}/"
API_ROOT = f"https://api.github.com/repos/{REPO}"
ROOT = Path(__file__).resolve().parents[1]


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


class SafeRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, msg, headers, newurl):
        redirected = super().redirect_request(request, fp, code, msg, headers, newurl)
        if redirected is not None:
            require(urllib.parse.urlsplit(newurl).scheme == "https", "Refusing non-HTTPS redirect")
            if urllib.parse.urlsplit(request.full_url).netloc != urllib.parse.urlsplit(newurl).netloc:
                redirected.remove_header("Authorization")
        return redirected


class GitHub:
    def __init__(self):
        self.token = os.environ.get("GH_TOKEN", "")
        require(bool(self.token), "GH_TOKEN must be provided by the publishing workflow")
        self.opener = urllib.request.build_opener(SafeRedirect())

    def request(self, path, *, method="GET", payload=None, data=None,
                authenticated=True, headers=None):
        url = path if path.startswith("https://") else API_ROOT + path
        options = {
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
            "User-Agent": "Chimera-pinned-open-beta-publisher",
        }
        if authenticated:
            require(urllib.parse.urlsplit(url).hostname in ("api.github.com", "uploads.github.com"),
                    "Refusing to send credentials outside GitHub API")
            options["Authorization"] = "Bearer " + self.token
        if payload is not None:
            data = json.dumps(payload).encode("utf-8")
            options["Content-Type"] = "application/json"
        options.update(headers or {})
        return self.opener.open(urllib.request.Request(url, data=data, headers=options, method=method),
                                timeout=180)

    def json(self, path, **kwargs):
        with self.request(path, **kwargs) as response:
            return json.load(response)

    def optional(self, path):
        try:
            return self.json(path)
        except urllib.error.HTTPError as error:
            if error.code == 404:
                return None
            raise

    def pages(self, path, key=None, *, authenticated=True):
        result = []
        for page in range(1, 101):
            separator = "&" if "?" in path else "?"
            body = self.json(f"{path}{separator}per_page=100&page={page}", authenticated=authenticated)
            items = body[key] if key else body
            result.extend(items)
            if len(items) < 100:
                return result
        raise RuntimeError("Unexpected pagination size")

    def download(self, path, destination, *, authenticated=True):
        # Actions archive endpoints negotiate GitHub JSON before redirecting to ZIP bytes.
        with self.request(path, authenticated=authenticated) as response:
            with destination.open("wb") as output:
                shutil.copyfileobj(response, output, length=1024 * 1024)


def verify_ci(api):
    run = api.json(f"/actions/runs/{RUN}")
    require(run["head_sha"] == HEAD and run["status"] == "completed" and
            run["conclusion"] == "success", "Pinned CI run is not the successful tested commit")
    require(run["run_number"] == 136 and run["repository"]["full_name"] == REPO,
            "Unexpected CI run identity")
    jobs = api.pages(f"/actions/runs/{RUN}/jobs", "jobs")
    expected = {"build (windows-latest)", "build (ubuntu-22.04)", "build (macos-15)", "assemble-candidate"}
    require(expected <= {job["name"] for job in jobs} and
            all(job["status"] == "completed" and job["conclusion"] == "success" for job in jobs),
            "Every required platform and assembly job must succeed")
    artifact = api.json(f"/actions/artifacts/{ARTIFACT}")
    require(artifact["name"] == ARTIFACT_NAME and not artifact["expired"] and
            artifact["size_in_bytes"] == ARCHIVE_SIZE and
            artifact["digest"] == "sha256:" + ARCHIVE_SHA and
            artifact["workflow_run"]["id"] == RUN and artifact["workflow_run"]["head_sha"] == HEAD,
            "Pinned artifact provenance mismatch")
    for revision in (HEAD, REVISION):
        commit = api.json(f"/git/commits/{revision}")
        require(commit["tree"]["sha"] == TREE, "CI merge and tested head trees differ")
    print("PASS: all CI jobs, pinned artifact provenance, and source trees")


def extract_candidate(archive, destination):
    require(archive.stat().st_size == ARCHIVE_SIZE and sha256(archive) == ARCHIVE_SHA,
            "Candidate archive size or SHA-256 does not match verified build 136")
    with zipfile.ZipFile(archive) as zipped:
        entries = zipped.infolist()
        names = [entry.filename for entry in entries]
        require(len(names) == len(set(names)), "Duplicate archive entries")
        require(sum(entry.file_size for entry in entries) < 1024 * 1024 * 1024,
                "Unexpected archive expansion size")
        for entry in entries:
            path = PurePosixPath(entry.filename)
            mode = entry.external_attr >> 16
            require(len(path.parts) == 1 and not path.is_absolute() and
                    entry.filename not in (".", "..") and "\\" not in entry.filename and
                    not entry.is_dir() and not stat.S_ISLNK(mode), "Unsafe candidate archive entry")
        zipped.extractall(destination)


def prepare_assets(candidate, notes_path):
    source = json.loads((candidate / "candidate-source.json").read_text())
    require(source["version"] == VERSION and source["tag"] == TAG and source["revision"] == REVISION and
            str(source["runId"]) == str(RUN) and source["published"] is False and
            source["publisherSigned"] is False and source["macOSNotarized"] is False,
            "Candidate source metadata mismatch")
    sums = {}
    for line in (candidate / "SHA256SUMS.txt").read_text(encoding="utf-8-sig").splitlines():
        digest, name = line.split(maxsplit=1)
        name = name.lstrip("*").strip()
        require(name not in sums, "Duplicate checksum entry")
        sums[name] = digest.lower()
    require(sums == BINARY_HASHES, "Checksum file must contain exactly the five pinned public binaries")
    for name, digest in BINARY_HASHES.items():
        require(sha256(candidate / name) == digest, f"Binary checksum mismatch: {name}")
        sidecar = (candidate / (name + ".sha256.txt")).read_text(encoding="utf-8-sig").strip().split()
        require(sidecar[0].lower() == digest, f"Sidecar checksum mismatch: {name}")
    manifest = json.loads((candidate / "update-beta.json").read_text())
    require(manifest["schema"] == 1 and manifest["version"] == VERSION and manifest["channel"] == "beta" and
            manifest["releaseUrl"] == RELEASE_URL, "Update manifest identity mismatch")
    expected = {
        ("windows", "x86_64"): PREFIX + "win64-Setup.exe",
        ("macos", "universal"): PREFIX + "macos-universal.pkg",
        ("linux", "x86_64"): PREFIX + "linux-x86_64.deb",
    }
    require(len(manifest["assets"]) == len(expected), "Unexpected update platform count")
    seen = set()
    for asset in manifest["assets"]:
        platform = (asset["platform"], asset["arch"])
        require(platform in expected and platform not in seen, "Unexpected or duplicate update platform")
        seen.add(platform)
        name = expected[platform]
        require(asset["name"] == name and asset["url"] == DOWNLOAD_URL + name and
                asset["sha256"] == BINARY_HASHES[name] and asset["size"] == (candidate / name).stat().st_size,
                f"Update manifest asset mismatch: {name}")
    notes = notes_path.read_text(encoding="utf-8").strip()
    require(len(notes) > 100 and "1.0" in notes, "Missing final release notes")
    assets = {name: candidate / name for name in (*BINARY_HASHES, *DOCUMENTS)}
    assets["PUBLISHED_OPEN_BETA_1.md"] = notes_path
    if "OPEN_BETA_NAM_VALIDATION.md" in notes:
        assets["OPEN_BETA_NAM_VALIDATION.md"] = ROOT / "docs/OPEN_BETA_NAM_VALIDATION.md"
    if "EXTERNAL_BASS_IRS.md" in (candidate / "THIRD_PARTY_NOTICES.md").read_text(encoding="utf-8"):
        assets["EXTERNAL_BASS_IRS.md"] = ROOT / "docs/EXTERNAL_BASS_IRS.md"
    if (ROOT / "LICENSE").is_file():
        assets["LICENSE"] = ROOT / "LICENSE"
    for name, path in assets.items():
        require(path.is_file() and path.stat().st_size > 0, f"Missing public release asset: {name}")
        require("personal" not in name.lower() and "private" not in name.lower(), "Private asset rejected")
    print(f"PASS: five binary hashes, three-platform update manifest, manual, and {len(assets)} public assets")
    return notes, assets


def verify_tag(api, *, required=False):
    ref = api.optional(f"/git/ref/tags/{TAG}")
    if ref is None:
        require(not required, "Release tag is missing")
        return
    target = ref["object"]
    for _ in range(5):
        if target["type"] != "tag":
            break
        target = api.json(f"/git/tags/{target['sha']}")["object"]
    require(target["type"] == "commit" and target["sha"] == HEAD,
            "Existing tag differs from the tested commit; refusing to move it")


def verify_asset(api, asset, path):
    require(asset["size"] == path.stat().st_size and asset["state"] == "uploaded",
            f"Existing asset size/state mismatch: {asset['name']}")
    expected = sha256(path)
    if asset.get("digest"):
        require(asset["digest"] == "sha256:" + expected, f"Asset digest mismatch: {asset['name']}")
    else:
        with api.request(f"/releases/assets/{asset['id']}",
                         headers={"Accept": "application/octet-stream"}) as response:
            digest = hashlib.sha256()
            while chunk := response.read(1024 * 1024):
                digest.update(chunk)
        require(digest.hexdigest() == expected, f"Downloaded asset digest mismatch: {asset['name']}")


def verify_release_assets(api, release, assets, *, complete):
    remote = api.pages(f"/releases/{release['id']}/assets")
    by_name = {asset["name"]: asset for asset in remote}
    require(len(by_name) == len(remote), "Duplicate release asset names")
    require(set(by_name) <= set(assets), "Release contains unexpected assets; refusing to modify it")
    if complete:
        require(set(by_name) == set(assets), "Release asset list is incomplete")
    for name, asset in by_name.items():
        verify_asset(api, asset, assets[name])
    return by_name


def public_check(api, assets):
    for attempt in range(6):
        try:
            release = api.json(f"/releases/tags/{TAG}", authenticated=False)
            require(not release["draft"] and release["prerelease"] and release["html_url"] == RELEASE_URL,
                    "Release is not publicly visible as a prerelease")
            public_assets = {asset["name"]: asset for asset in release["assets"]}
            require(set(public_assets) == set(assets), "Public release asset list mismatch")
            for name, path in assets.items():
                require(public_assets[name]["size"] == path.stat().st_size and
                        public_assets[name]["browser_download_url"] == DOWNLOAD_URL + name,
                        "Unexpected public asset URL or size")
            with api.request(DOWNLOAD_URL + "update-beta.json", authenticated=False,
                             headers={"Accept": "application/octet-stream"}) as response:
                downloaded = response.read()
            require(downloaded == assets["update-beta.json"].read_bytes(), "Public update manifest differs")
            feed = api.pages("/releases", authenticated=False)
            require(any(item["id"] == release["id"] and not item["draft"] and item["prerelease"] for item in feed),
                    "Beta release is absent from public discovery feed")
            print("PASS: unauthenticated release, assets, beta discovery, and downloaded update manifest")
            return release
        except (urllib.error.URLError, RuntimeError):
            if attempt == 5:
                raise
            time.sleep(5)


def publish(api, notes, assets):
    verify_tag(api)
    matches = [release for release in api.pages("/releases") if release["tag_name"] == TAG]
    require(len(matches) <= 1, "Multiple releases have the requested tag")
    if matches:
        release = matches[0]
        require(release["name"] == TITLE and release["body"].strip() == notes and release["prerelease"],
                "Existing release metadata differs; refusing to overwrite")
        verify_release_assets(api, release, assets, complete=not release["draft"])
        if not release["draft"]:
            verify_tag(api, required=True)
            print("Existing published release matches exactly; verifying public discovery")
            return public_check(api, assets)
    else:
        release = api.json("/releases", method="POST", payload={
            "tag_name": TAG, "target_commitish": HEAD, "name": TITLE, "body": notes,
            "draft": True, "prerelease": True, "make_latest": "false",
        })
        print(f"Created staging draft release {release['id']}")
    existing = verify_release_assets(api, release, assets, complete=False)
    upload_url = release["upload_url"].split("{", 1)[0]
    require(upload_url == f"https://uploads.github.com/repos/{REPO}/releases/{release['id']}/assets",
            "Unexpected release upload endpoint")
    for name, path in assets.items():
        if name in existing:
            continue
        with path.open("rb") as source:
            uploaded = api.json(upload_url + "?" + urllib.parse.urlencode({"name": name}),
                                method="POST", data=source, headers={
                                    "Content-Type": mimetypes.guess_type(name)[0] or "application/octet-stream",
                                    "Content-Length": str(path.stat().st_size),
                                })
        verify_asset(api, uploaded, path)
        print(f"Uploaded and verified {name} ({path.stat().st_size} bytes)")
    verify_release_assets(api, release, assets, complete=True)
    verify_tag(api)
    api.json(f"/releases/{release['id']}", method="PATCH", payload={
        "draft": False, "prerelease": True, "make_latest": "false", "target_commitish": HEAD,
    })
    verify_tag(api, required=True)
    return public_check(api, assets)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate-zip", type=Path)
    parser.add_argument("--notes", type=Path, default=ROOT / "docs/PUBLISHED_OPEN_BETA_1.md")
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    require(not args.verify_only or args.candidate_zip, "--verify-only requires --candidate-zip")
    with tempfile.TemporaryDirectory(prefix="chimera-release-") as temporary:
        working = Path(temporary)
        api = None if args.verify_only else GitHub()
        if api:
            verify_ci(api)
        archive = args.candidate_zip or working / "candidate.zip"
        if args.candidate_zip is None:
            api.download(f"/actions/artifacts/{ARTIFACT}/zip", archive)
        candidate = working / "candidate"
        candidate.mkdir()
        extract_candidate(archive, candidate)
        notes, assets = prepare_assets(candidate, args.notes)
        if args.verify_only:
            print("Local release verification completed; no remote actions performed")
            return
        release = publish(api, notes, assets)
        print(f"Published and verified: {release['html_url']}")
        if os.environ.get("GITHUB_STEP_SUMMARY"):
            with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as summary:
                summary.write(f"## Open Beta 1.0 published\n\n[{TITLE}]({release['html_url']})\n\n"
                              f"Pinned build **136**, source `{HEAD}`, **{len(assets)}** verified public assets. "
                              "Manual included. Unsigned Windows beta; macOS is not notarized.\n")


if __name__ == "__main__":
    try:
        main()
    except urllib.error.HTTPError as error:
        print(f"Release publication stopped: GitHub HTTP {error.code} ({error.reason})", file=sys.stderr)
        sys.exit(1)
    except (RuntimeError, ValueError, KeyError, OSError, zipfile.BadZipFile) as error:
        print(f"Release publication stopped: {error}", file=sys.stderr)
        sys.exit(1)
