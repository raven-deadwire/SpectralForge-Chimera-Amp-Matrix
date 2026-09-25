#!/usr/bin/env python3
"""Apply the approved copyright notice to existing beta documentation only."""

import hashlib
import mimetypes
import sys
import time
import urllib.error
import urllib.parse

from publish_open_beta import GitHub, ROOT, TAG, RELEASE_URL, DOWNLOAD_URL, require, verify_tag, sha256

RELEASE_ID = 396424708
NOTICE = (
    "Copyright © 2026 RavenForge Luthier Intelligence. All rights reserved.\n\n"
    "Third-party software and assets are subject to their respective copyright notices and license terms."
)
DOCUMENTS = {
    "COPYRIGHT.txt": ROOT / "COPYRIGHT.txt",
    "MANUAL.html": ROOT / "docs/MANUAL.html",
    "PUBLISHED_OPEN_BETA_1.md": ROOT / "docs/PUBLISHED_OPEN_BETA_1.md",
    "THIRD_PARTY_NOTICES.md": ROOT / "docs/THIRD_PARTY_NOTICES.md",
}
ORIGINAL_HASHES = {
    "MANUAL.html": "da874f6dc6f7063785555b008fdcd48c9951c79d414bb286c30ab8efdca555e6",
    "PUBLISHED_OPEN_BETA_1.md": "a9790dee722608da89a60a6163b7aee57407e5c0db8e46c7a6b6fb76633b386b",
    "THIRD_PARTY_NOTICES.md": "002b07ef6ca1c8134c57df0097efb25c4f21ce31a805e22aa527587d7c0c2823",
}


def check_document(asset, path):
    require(asset["state"] == "uploaded" and asset["size"] == path.stat().st_size
            and asset["digest"] == "sha256:" + sha256(path), "Copyright document upload mismatch")


def upload_document(api, release, name, path):
    url = release["upload_url"].split("{", 1)[0]
    require(url == f"https://uploads.github.com/repos/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/{RELEASE_ID}/assets",
            "Unexpected upload endpoint")
    with path.open("rb") as stream:
        asset = api.json(url + "?" + urllib.parse.urlencode({"name": name}), method="POST", data=stream,
                         headers={"Content-Type": mimetypes.guess_type(path.name)[0] or "text/plain",
                                  "Content-Length": str(path.stat().st_size)})
    check_document(asset, path)
    return asset


def main():
    require((ROOT / "COPYRIGHT.txt").read_text(encoding="utf-8").strip() == NOTICE, "Notice text changed")
    for path in DOCUMENTS.values():
        text = path.read_text(encoding="utf-8")
        require(all(line in text for line in NOTICE.split("\n\n")), "Notice missing from a document")
    api = GitHub()
    release = api.json(f"/releases/{RELEASE_ID}")
    require(release["tag_name"] == TAG and not release["draft"] and release["prerelease"]
            and not release.get("immutable", False), "Unexpected or immutable release")
    verify_tag(api, required=True)
    before = api.pages(f"/releases/{RELEASE_ID}/assets")
    by_name = {asset["name"]: asset for asset in before}
    pending_names = {name + ".copyright-pending" for name in DOCUMENTS}
    protected = {asset["name"]: (asset["id"], asset["size"], asset["digest"])
                 for asset in before if asset["name"] not in DOCUMENTS and asset["name"] not in pending_names}
    # Check every replacement before changing any existing document.
    for name, path in DOCUMENTS.items():
        current = by_name.get(name)
        allowed = {"sha256:" + sha256(path)}
        if name in ORIGINAL_HASHES:
            allowed.add("sha256:" + ORIGINAL_HASHES[name])
        require(current is None or current["digest"] in allowed, "Unrecognized existing document: " + name)
    for name, path in DOCUMENTS.items():
        current = by_name.get(name)
        if current and current["digest"] == "sha256:" + sha256(path):
            check_document(current, path)
            continue
        # Upload and verify the complete replacement before removing the old document.
        pending_name = name + ".copyright-pending"
        pending = by_name.get(pending_name)
        if pending:
            check_document(pending, path)
        else:
            pending = upload_document(api, release, pending_name, path)
        if current:
            with api.request(f"/releases/assets/{current['id']}", method="DELETE"):
                pass
        renamed = api.json(f"/releases/assets/{pending['id']}", method="PATCH", payload={"name": name})
        check_document(renamed, path)
        print("Updated and verified " + name, flush=True)
    current_release = api.json(f"/releases/{RELEASE_ID}")
    body = current_release.get("body", "").rstrip()
    if NOTICE not in body:
        body += "\n\n---\n\n" + NOTICE
        api.json(f"/releases/{RELEASE_ID}", method="PATCH", payload={"body": body})
    after = {asset["name"]: asset for asset in api.pages(f"/releases/{RELEASE_ID}/assets")}
    require(set(after) == set(protected) | set(DOCUMENTS), "Unexpected final release asset list")
    for name, identity in protected.items():
        asset = after[name]
        require((asset["id"], asset["size"], asset["digest"]) == identity, "Protected asset changed: " + name)
    verify_tag(api, required=True)
    for name, path in DOCUMENTS.items():
        check_document(after[name], path)
        for attempt in range(6):
            try:
                with api.request(DOWNLOAD_URL + name + "?copyright=" + sha256(path), authenticated=False,
                                 headers={"Accept": "application/octet-stream"}) as response:
                    content = response.read()
                require(hashlib.sha256(content).hexdigest() == sha256(path), "Public document content mismatch")
                break
            except (urllib.error.URLError, RuntimeError):
                if attempt == 5:
                    raise
                time.sleep(3)
    public_release = api.json(f"/releases/tags/{TAG}", authenticated=False)
    require(NOTICE in public_release["body"] and public_release["html_url"] == RELEASE_URL,
            "Public release notice is missing")
    print(f"PASS: exact copyright notice; four public documents; {len(protected)} unchanged assets; unchanged tag", flush=True)


if __name__ == "__main__":
    try:
        main()
    except urllib.error.HTTPError as error:
        print(f"Copyright update stopped: GitHub HTTP {error.code} ({error.reason})", file=sys.stderr)
        sys.exit(1)
    except (RuntimeError, ValueError, KeyError, OSError) as error:
        print(f"Copyright update stopped: {error}", file=sys.stderr)
        sys.exit(1)
