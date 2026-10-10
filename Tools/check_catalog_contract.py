#!/usr/bin/env python3
"""Verify the current Chimera append-only catalog-count contract against source catalogs."""
from __future__ import annotations
import json
import re
from pathlib import Path
from check_ir_distribution import validate_source

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

serialized_count = amp_count
legacy_indices = CONTRACT.get("legacy_amp_indices", [])
assert legacy_indices == [16] and "legacyHiddenAmpIndex = static_cast<int>(AmpModel::ironCompact)" in amp
assert all(0 <= i < serialized_count for i in legacy_indices)
amp_count -= len(legacy_indices)
assert serialized_count == CONTRACT["serialized_model_counts"]["amp"]
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

# Guitar signatures live in a separate append-only full-state bank.
guitar = read("Source/GuitarSignaturePresets.h")
names = re.findall(r'\{"raven\.[^"]+","([^"]+)"', guitar)
assert names == ["A Path To Alsatia", "Feel My Wrath", "Blackhearted", "Dark Matters of Throne"]
assert "selectablePresetCount=factoryPresetCount+int(guitarSignatures.size())+originalPresetCount+niflheimrPresetCount" in guitar
validate_source(ROOT)
print("PASS: four append-only full-state Guitar Signatures and two approved embedded IR assets with exact SHA-256")

original = read("Source/OriginalPresets.h")
original_names = re.findall(r'\{"original\.rig\.nastrond\.[^"]+","([^"]+)"', original)
assert original_names == ["Thall Rhythm", "Molten Lead", "Rotten Grind", "Sludge Mass", "Slam Impact"], original_names
assert len(original_names) == CONTRACT["original_preset_count"]
niflheimr = read("Source/NiflheimrPresets.h")
niflheimr_names = re.findall(r'\{"original\.rig\.niflheimr\.[^"]+","([^"]+)"', niflheimr)
assert niflheimr_names == ["Frostline Precision", "Carrion Barrage", "Foundry Pulse", "Jötunn Hammer", "Mirebound Monolith"], niflheimr_names
assert "niflheimrPresetStart=originalPresetStart+originalPresetCount" in niflheimr
assert len(niflheimr_names) == CONTRACT["niflheimr_original_preset_count"]
assert preset_total + len(names) + len(original_names) + len(niflheimr_names) == CONTRACT["selectable_preset_count"]
assert CONTRACT["niflheimr_original_preset_ordinals"] == [
    {"index": 43 + i, "name": name} for i, name in enumerate(niflheimr_names)
]
print("PASS: five Náströnd + five Niflheimr Original full rigs; 48 selectable presets, previous 43 indices preserved")
