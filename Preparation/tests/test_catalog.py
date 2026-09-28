import copy
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from catalog import build_catalog, validate_catalog
class CatalogTests(unittest.TestCase):
    def setUp(self):
        self.catalog=build_catalog(); self.models={m["id"]:m for m in self.catalog["models"]}
    def test_base_inventory(self):
        old=[m for m in self.models.values() if m["status"]=="existing_dsp"]
        self.assertEqual(len(old),61)
        self.assertEqual(sum(m["location"]=="rig" for m in old),15)
        self.assertEqual(sum(m["location"]=="pre" for m in old),25)
        self.assertEqual(sum(m["location"]=="post" for m in old),21)
    def test_all_requested_additions_present(self):
        self.assertEqual(len(self.models),80)
        for key in ("zuta-gbg120","engl","diezel-vh4","svt-cl","sunn"):
            self.assertEqual(self.models["planned.amp."+key]["priority"],"required")
    def test_distortion_plus_has_no_fictional_tone(self):
        self.assertEqual([c["id"] for c in self.models["planned.distortion-plus"]["controls"]],["distortion","output"])
    def test_jb2_six_knobs_plus_six_modes(self):
        controls=self.models["planned.jb2"]["controls"]
        self.assertEqual(sum(c["kind"]=="knob" for c in controls),6)
        self.assertEqual(len(controls[-1]["options"]),6)
    def test_ocd_is_not_manufacturer_verified(self):
        self.assertIn("pending",self.models["planned.ocd-v2"]["controls_status"])
    def test_no_audio_claim_or_binding(self):
        for m in self.models.values():
            self.assertFalse(m["audio_in_prototype"])
            for c in m["controls"]: self.assertIsNone(c["dsp_binding"])
    def test_zuta_channels_have_independent_keys(self):
        keys={c["id"] for c in self.models["planned.amp.zuta-gbg120"]["controls"]}
        for ch in range(1,5): self.assertIn(f"ch{ch}.gain",keys)
        self.assertIn("ch4.tight",keys); self.assertNotIn("ch1.tight",keys)
    def test_legacy_ranges_preserved(self):
        c=self.models["legacy.drive.0"]["controls"][1]
        self.assertEqual((c["id"],c["minimum"],c["maximum"],c["initial"]),("pretone",1000,10000,4000))
    def test_duplicate_id_fails(self):
        self.catalog["models"].append(copy.deepcopy(self.catalog["models"][0]))
        with self.assertRaises(ValueError): validate_catalog(self.catalog)
    def test_nonfinite_range_fails(self):
        self.catalog["models"][0]["controls"][0]["initial"]=float('nan')
        with self.assertRaises(ValueError): validate_catalog(self.catalog)
if __name__=="__main__": unittest.main()
