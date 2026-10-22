#!/usr/bin/env python3
"""Blind listening admin for the transpose modes (protocol §5).

Two methods:
  * abx         — rigorous "can you even tell two modes apart?" Pick exactly two
                  files (e.g. CLASSIC vs TIGHT). Each trial plays A, B and a hidden
                  X (randomly A or B); the listener decides X==A or X==B. Scoring
                  gives an exact binomial p-value.
  * preference  — two or more files. Each trial shows the clips under freshly
                  shuffled neutral labels; the listener marks the preferred label
                  and optional 0–5 attribute ratings. Scoring tallies the real
                  modes and averages the ratings.

The truth lives in blind_key.json (keep it away from the listener). The listener
only ever sees blind_sheet.csv and the neutral clip names. Level-match the inputs
first (transpose_measure.py matchgains) — blind tests on unmatched levels are
worthless.

Usage:
    python blind_listening.py prepare --method abx \
        --file CLASSIC=renders/classic_-12.wav --file TIGHT=renders/tight_-12.wav \
        --trials 16 --out blind_run --anon --seed 1

    python blind_listening.py prepare --method preference \
        --file CLASSIC=c.wav --file LOW-LAT=l.wav --file TIGHT=t.wav \
        --trials 12 --out blind_run --anon

    # ... listener fills blind_run/blind_sheet.csv ...
    python blind_listening.py score --key blind_run/blind_key.json \
        --responses blind_run/blind_sheet.csv
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import os
import random
import shutil
from pathlib import Path

LETTERS = "PQRSTUVWXYZ"


def _link(src, dst):
    """Neutral-named clip: symlink if possible, else copy (keeps audio un-duplicated)."""
    dst = Path(dst)
    if dst.exists() or dst.is_symlink():
        dst.unlink()
    src = Path(src).resolve()
    try:
        os.symlink(src, dst)
    except OSError:
        shutil.copyfile(src, dst)


def prepare(method, files, trials, out_dir, anon, seed):
    if seed is not None:
        random.seed(seed)
    out = Path(out_dir)
    anon_dir = out / "anon"
    out.mkdir(parents=True, exist_ok=True)
    if anon:
        anon_dir.mkdir(exist_ok=True)

    if method == "abx":
        if len(files) != 2:
            raise SystemExit("abx needs exactly two --file entries")
        (na, pa), (nb, pb) = files
        key = {"method": "abx", "A": na, "B": nb, "seed": seed, "trials": []}
        sheet = [["trial", "play_A", "play_B", "play_X", "your_answer_A_or_B",
                  "confidence_1_5", "notes"]]
        for i in range(1, trials + 1):
            x_truth = random.choice(["A", "B"])
            key["trials"].append({"trial": i, "x_truth": x_truth})
            a_clip, b_clip, x_clip = pa, pb, (pa if x_truth == "A" else pb)
            if anon:
                a_clip = str(anon_dir / f"trial{i:02d}_A.wav"); _link(pa, a_clip)
                b_clip = str(anon_dir / f"trial{i:02d}_B.wav"); _link(pb, b_clip)
                x_clip = str(anon_dir / f"trial{i:02d}_X.wav")
                _link(pa if x_truth == "A" else pb, x_clip)
            sheet.append([i, a_clip, b_clip, x_clip, "", "", ""])
    elif method == "preference":
        if len(files) < 2:
            raise SystemExit("preference needs at least two --file entries")
        key = {"method": "preference", "seed": seed, "trials": []}
        sheet = [["trial", "clips", "your_preferred_label",
                  "low_end_0_5", "clarity_0_5", "artifacts_0_5", "notes"]]
        for i in range(1, trials + 1):
            order = files[:]
            random.shuffle(order)
            labels = LETTERS[:len(order)]
            mapping = {}            # shown label -> real mode
            shown = []
            for lab, (name, path) in zip(labels, order):
                mapping[lab] = name
                clip = path
                if anon:
                    clip = str(anon_dir / f"trial{i:02d}_{lab}.wav"); _link(path, clip)
                shown.append(f"{lab}={clip}")
            key["trials"].append({"trial": i, "labels": mapping})
            sheet.append([i, " | ".join(shown), "", "", "", "", ""])
    else:
        raise SystemExit(f"unknown method {method}")

    (out / "blind_key.json").write_text(json.dumps(key, ensure_ascii=False, indent=2))
    with (out / "blind_sheet.csv").open("w", newline="") as fh:
        csv.writer(fh).writerows(sheet)
    print(f"wrote {out/'blind_sheet.csv'} ({trials} trials, method={method}) "
          f"and {out/'blind_key.json'} (hide this from the listener)")
    if anon:
        print(f"neutral clips in {anon_dir}/ — the listener plays only those")


def _binom_tail(k, n, p=0.5):
    """P(X >= k) for X~Binomial(n, p) — exact."""
    return sum(math.comb(n, j) * p ** j * (1 - p) ** (n - j) for j in range(k, n + 1))


def score(key_path, responses_path):
    key = json.loads(Path(key_path).read_text())
    rows = list(csv.DictReader(Path(responses_path).open(newline="")))
    by_trial = {int(r["trial"]): r for r in rows if r.get("trial")}

    if key["method"] == "abx":
        n = correct = answered = 0
        for t in key["trials"]:
            r = by_trial.get(t["trial"])
            ans = (r or {}).get("your_answer_A_or_B", "").strip().upper()
            if ans not in ("A", "B"):
                continue
            answered += 1
            n += 1
            if ans == t["x_truth"]:
                correct += 1
        p = _binom_tail(correct, n) if n else float("nan")
        print(f"ABX  {key['A']} vs {key['B']}: {correct}/{n} correct "
              f"({answered} answered)")
        if n:
            print(f"  exact one-sided p (chance = 50%) = {p:.4f}  "
                  f"{'→ audible (p<0.05)' if p < 0.05 else '→ not distinguishable at p<0.05'}")
        return {"correct": correct, "n": n, "p": p}

    # preference
    pref = {}
    ratings = {}            # mode -> {attr: [values]}
    attrs = ["low_end_0_5", "clarity_0_5", "artifacts_0_5"]
    for t in key["trials"]:
        r = by_trial.get(t["trial"])
        if not r:
            continue
        lab = r.get("your_preferred_label", "").strip().upper()
        mode = t["labels"].get(lab)
        if mode:
            pref[mode] = pref.get(mode, 0) + 1
        # per-mode ratings require per-label ratings; this simple sheet has one row,
        # so ratings attach to the preferred mode as a coarse signal.
        for a in attrs:
            v = (r.get(a) or "").strip()
            if mode and v:
                try:
                    ratings.setdefault(mode, {}).setdefault(a, []).append(float(v))
                except ValueError:
                    pass
    total = sum(pref.values())
    print(f"Preference over {total} answered trials:")
    for mode, c in sorted(pref.items(), key=lambda kv: -kv[1]):
        pct = 100.0 * c / total if total else 0.0
        extra = ""
        if mode in ratings:
            extra = "  " + " ".join(f"{a.split('_')[0]}={sum(v)/len(v):.1f}"
                                    for a, v in ratings[mode].items())
        print(f"  {mode:8s} {c:3d}  ({pct:4.1f}%){extra}")
    return {"preference": pref, "ratings": ratings}


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    pr = sub.add_parser("prepare")
    pr.add_argument("--method", choices=["abx", "preference"], required=True)
    pr.add_argument("--file", action="append", required=True, metavar="MODE=path",
                    help="repeat; e.g. --file CLASSIC=classic.wav")
    pr.add_argument("--trials", type=int, default=16)
    pr.add_argument("--out", default="blind_run")
    pr.add_argument("--anon", action="store_true", help="make neutral-named clip links")
    pr.add_argument("--seed", type=int, default=None)
    sc = sub.add_parser("score")
    sc.add_argument("--key", required=True)
    sc.add_argument("--responses", required=True)
    args = p.parse_args(argv)
    if args.cmd == "prepare":
        files = []
        for entry in args.file:
            if "=" not in entry:
                raise SystemExit(f"--file must be MODE=path, got {entry}")
            name, path = entry.split("=", 1)
            files.append((name, path))
        prepare(args.method, files, args.trials, args.out, args.anon, args.seed)
    else:
        score(args.key, args.responses)


if __name__ == "__main__":
    main()
