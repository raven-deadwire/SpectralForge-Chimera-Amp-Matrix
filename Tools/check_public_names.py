#!/usr/bin/env python3
"""Check the next release's presentation text and shipped documents.

Research catalogs, source comments, immutable evidence, serialized IDs and user
files are deliberately outside this boundary. Their preservation is independent
of the labels that the product presents to its users.
"""
from __future__ import annotations

import argparse
import html
import json
from pathlib import Path, PurePosixPath
import re
import unicodedata

ROOT = Path(__file__).resolve().parents[1]
POLICY_PATH = "Validation/public-presentation-policy.json"
TEXT_SUFFIXES = frozenset({".html", ".htm", ".md", ".txt", ".json", ".log", ".csv", ".tsv"})


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"Duplicate public-name policy key: {key}")
        result[key] = value
    return result


def load_policy(root: Path = ROOT) -> dict:
    policy = json.loads((Path(root) / POLICY_PATH).read_text(encoding="utf-8"),
                        object_pairs_hook=unique_object)
    require(policy.get("schema") == "spectralforge.public-presentation.v1",
            "Unknown public presentation policy")
    terms = policy.get("forbidden_terms")
    require(isinstance(terms, list) and len(terms) >= 25
            and all(isinstance(value, str) and value.strip() for value in terms),
            "Public-name policy requires an explicit original-name inventory")
    documents = policy.get("public_documents")
    require(isinstance(documents, list) and documents
            and len(documents) == len(set(documents)), "Missing/duplicate public document inventory")
    for name in documents:
        require(isinstance(name, str) and PurePosixPath(name).name == name
                and name not in {".", ".."}, f"Invalid public document name: {name}")
    for name, fragments in policy.get("required_attribution_exceptions", {}).items():
        require(name in documents and isinstance(fragments, list)
                and all(isinstance(value, str) and value for value in fragments),
                f"Invalid bounded attribution exception: {name}")
    return policy


def normalise(text: str) -> str:
    # Match entity-encoded visible labels as well as plain HTML/JS data. Stable
    # internal asset/parameter tokens are not split into invented display words.
    for _ in range(2):
        text = html.unescape(text)
    text = unicodedata.normalize("NFKC", text)
    text = text.translate(str.maketrans({"\u2010": "-", "\u2011": "-", "\u2012": "-",
                                       "\u2013": "-", "\u2014": "-", "\u2212": "-"}))
    return re.sub(r"\s+", " ", text).casefold()


def forbidden_match(text: str, policy: dict) -> str | None:
    normal = normalise(text)
    for term in policy["forbidden_terms"]:
        # Word boundaries allow existing private asset keys such as amatchless
        # and board0_m29_boss_drive without hiding an actual 'BOSS DRIVE' label.
        if re.search(r"(?<!\w)" + re.escape(normalise(term)) + r"(?!\w)", normal):
            return term
    return None


def validate_text(text: str, context: str, *, policy: dict | None = None,
                  exceptions: tuple[str, ...] | list[str] = ()) -> None:
    policy = policy or load_policy()
    for fragment in exceptions:
        text = text.replace(fragment, "[required licensed-asset attribution]")
    forbidden = forbidden_match(text, policy)
    require(forbidden is None, f"Original amp/effect name in {context}: {forbidden}")


def validate_document_names(names, root: Path = ROOT) -> None:
    policy = load_policy(root)
    allowed = set(policy["public_documents"])
    actual = list(names)
    require(len(actual) == len(set(actual)), "Duplicate shipped public document")
    require(all(name in allowed for name in actual),
            f"Research/unreviewed document selected for shipment: {sorted(set(actual) - allowed)}")


def validate_document(path: Path, *, name: str | None = None, root: Path = ROOT) -> None:
    """Validate one reviewed source/candidate document without requiring a repo clone."""
    path = Path(path)
    policy = load_policy(root)
    document = path.name if name is None else name
    require(document in set(policy["public_documents"]) | {"README.md", "COPYRIGHT.txt"},
            f"Unreviewed public document: {document}")
    require(path.is_file() and not path.is_symlink(), f"Public document missing or symlinked: {path}")
    exceptions = policy.get("required_attribution_exceptions", {}).get(document, [])
    validate_text(path.read_text(encoding="utf-8-sig"), str(path), policy=policy, exceptions=exceptions)


def validate_source(root: Path = ROOT) -> dict:
    root = Path(root)
    policy = load_policy(root)
    paths = ["README.md", "COPYRIGHT.txt"] + ["docs/" + name for name in policy["public_documents"]]
    for relative in paths:
        path = root / relative
        require(path.is_file() and not path.is_symlink(), f"Public document missing or symlinked: {relative}")
        validate_document(path, root=root)
    return {"schema": policy["schema"], "source_documents_checked": len(paths),
            "original_name_terms": len(policy["forbidden_terms"]),
            "scope": "Public documentation; runtime labels are exercised by native product tests"}


def validate_stage(stage: Path, root: Path = ROOT) -> None:
    stage = Path(stage)
    require(stage.is_dir(), f"Public payload directory missing: {stage}")
    policy = load_policy(root)
    restricted = set(policy.get("research_documents_not_shipped", []))
    for path in sorted(stage.rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(stage).as_posix()
        require(path.name not in restricted, f"Research document in public payload: {relative}")
        if path.suffix.casefold() not in TEXT_SUFFIXES:
            continue
        # Match all packaged text, including an unexpected extra caption file
        # or Verification.txt, rather than trusting only known filenames.
        exceptions = policy.get("required_attribution_exceptions", {}).get(path.name, [])
        validate_text(path.read_text(encoding="utf-8-sig"), relative, policy=policy, exceptions=exceptions)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--stage", type=Path)
    args = parser.parse_args()
    report = validate_source(args.root)
    if args.stage:
        validate_stage(args.stage, args.root)
        report["stage"] = str(args.stage)
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
