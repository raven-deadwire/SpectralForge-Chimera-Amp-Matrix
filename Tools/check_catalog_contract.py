#!/usr/bin/env python3
"""Verify the frozen Chimera 1.1.1 catalog-count contract against source catalogs."""
from __future__ import annotations
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONTRACT = json.loads((ROOT / "Validation/catalog-count-contract.json").read_text(encoding="utf-8"))


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


amp = read("Source/AmpCatalog.h")
pedal = read("Source/PedalBoardCatalog.h")
models = read("Source/ModelCatalog.h")
presets = read("Source/FactoryPresets.h")

enum = re.search(r"enum class AmpModel\s*:\s*int\s*\{(.*?)\bcount\s*\}", amp, re.S)
assert enum, "AmpModel enum not found"
amp_count = len([item for item in enum.group(1).split(",") if item.strip()])

pedal_count_match = re.search(r"pedalModelCount\s*=\s*(\d+)", pedal)
assert pedal_count_match, "pedalModelCount not found"
pedal_total = int(pedal_count_match.group(1))
assert '{"empty","Empty","EMPTY"' in pedal, "PRE catalog must retain Empty sentinel at index 0"
pre_count = pedal_total - 1

families = {}
for _, category, count in re.findall(r'\{"([^"]+)","([^"]+)",(\d+),\{\{', models):
    families[category] = int(count)
post_categories = ("DELAY", "REVERB", "BUS COMP", "PREAMP", "EQUALIZER", "MODULATION")
missing = [category for category in post_categories if category not in families]
assert not missing, f"POST categories missing from ModelCatalog: {missing}"
post_count = sum(families[category] for category in post_categories)

preset_count_match = re.search(r"std::array<FactoryPreset,(\d+)>\s+factoryPresets", presets)
assert preset_count_match, "factoryPresets array size not found"
preset_total = int(preset_count_match.group(1))
signature_count = len(re.findall(r"\},\s*PresetKind::signature,", presets))
factory_count = preset_total - signature_count

actual = {"amp": amp_count, "pre": pre_count, "post": post_count, "total": amp_count + pre_count + post_count}
expected = CONTRACT["model_counts"]
assert actual == expected, f"model count contract mismatch: expected {expected}, got {actual}"

expected_presets = CONTRACT["preset_counts"]
actual_presets = {"factory": factory_count, "signature": signature_count, "total": preset_total}
assert actual_presets == expected_presets, f"preset count contract mismatch: expected {expected_presets}, got {actual_presets}"
assert CONTRACT["pre_board_capacity"] == 5, "PRE board capacity contract changed"
assert "Bassman Valve" not in presets, "Preset descriptions retain the stale pre-catalog Bassman Valve alias"
for preset_name in ("Finger Round", "G+B Air / Weight", "B+B Warm / Definition"):
    assert re.search(r'\{"' + re.escape(preset_name) + r'"[^\n]+"[^\n]*Vintage Valve', presets), \
        f"{preset_name} no longer names the active Vintage Valve catalog model"

print(
    f"PASS: Chimera catalog contract {actual['total']} models = AMP {amp_count} + PRE {pre_count} + POST {post_count}; "
    f"presets {factory_count}+{signature_count}"
)
