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
        self.assertEqual(len(self.models),81)
        for key in ("zuta-gbg120","engl","engl-e670fe","diezel-vh4","svt-cl","sunn"):
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
        c=self.models["legacy.drive.0"]["legacy_controls"][1]
        self.assertEqual((c["id"],c["minimum"],c["maximum"],c["initial"]),("pretone",1000,10000,4000))
    def test_duplicate_id_fails(self):
        self.catalog["models"].append(copy.deepcopy(self.catalog["models"][0]))
        with self.assertRaises(ValueError): validate_catalog(self.catalog)
    def test_nonfinite_range_fails(self):
        self.catalog["models"][0]["controls"][0]["initial"]=float('nan')
        with self.assertRaises(ValueError): validate_catalog(self.catalog)
    def test_all_25_existing_pre_have_separate_native_and_legacy(self):
        from catalog import legacy_controls
        old=[m for m in self.models.values() if m['id'].startswith('legacy.') and m['location']=='pre']
        self.assertEqual(len(old),25)
        for m in old:
            self.assertEqual(m['legacy_controls'],legacy_controls(m['id'].split('.')[1]))
            self.assertTrue(all(c['id'].startswith('hw.') for c in m['controls']))
            self.assertFalse(m['hardware_panel']['circuit_response_verified'])
    def test_m87_controls_and_ratios(self):
        cs=self.models['legacy.comp.0']['controls']
        self.assertEqual([c['label'] for c in cs],['RELEASE','ATTACK','OUTPUT','RATIO','INPUT'])
        self.assertEqual(cs[3]['options'],['4:1','8:1','12:1','20:1'])
    def test_dyna_and_face_have_two_controls(self):
        for mid,labels in [('legacy.comp.1',['OUTPUT','SENSITIVITY']),('legacy.fuzz.1',['VOLUME','FUZZ'])]:
            self.assertEqual([c['label'] for c in self.models[mid]['controls']],labels)
    def test_fuzz_factory_original_five_controls(self):
        self.assertEqual([c['label'] for c in self.models['legacy.fuzz.4']['controls']],['VOLUME','GATE','COMP','DRIVE','STAB'])
    def test_m82_independent_levels_and_correct_decay_meaning(self):
        cs=self.models['legacy.filter.3']['controls']
        self.assertEqual([c['label'] for c in cs],['DRY','FX','DECAY','Q','SENS.'])
        self.assertIn('STOP FREQUENCY',cs[2]['note'])
    def test_qtron_mix_is_mode_not_universal_mix_knob(self):
        cs=self.models['legacy.filter.0']['controls']
        self.assertEqual(cs[0]['options'],['LP','BP','HP','MIX'])
        self.assertEqual(sum(c['kind']=='knob' for c in cs),2)
    def test_one_knob_boosters_and_internal_ep_dips(self):
        for mid in ['legacy.boost.1','legacy.boost.2','legacy.boost.3','legacy.boost.4']:
            cs=self.models[mid]['controls']
            self.assertEqual(sum(c['group']=='PANEL' and c['kind']=='knob' for c in cs),1)
        self.assertEqual(sum(c['group']=='INTERNAL' for c in self.models['legacy.boost.3']['controls']),2)
    def test_revision_separation(self):
        cpr=self.models['legacy.comp.2']
        self.assertIn('CPR-1',cpr['hardware_panel']['reference'])
        self.assertEqual(sum(c['kind']=='knob' for c in cpr['controls']),3)
        b3k=self.models['legacy.drive.4']
        self.assertIn('v2',b3k['hardware_panel']['reference'])
        self.assertNotIn('ATTACK',[c['label'] for c in b3k['controls']])
    def test_continuous_vs_stepped_ratio(self):
        self.assertEqual(next(c for c in self.models['legacy.comp.3']['controls'] if c['label']=='RATIO')['kind'],'knob')
        self.assertEqual(next(c for c in self.models['legacy.comp.0']['controls'] if c['label']=='RATIO')['kind'],'choice')
    def test_aw3_same_knobs_relabel_not_added_controls(self):
        cs=self.models['legacy.filter.4']['controls']
        self.assertEqual(len(cs),4)
        self.assertEqual(cs[1]['mode_labels']['labels']['3'],'VOWEL 2')
        self.assertEqual(cs[2]['mode_labels']['labels']['3'],'VOWEL 1')
    def test_pending_sources_not_promoted(self):
        from pedal_controls import hardware_panels
        for mid,p in hardware_panels().items():
            if p['review_status']=='primary_panel_reviewed': self.assertTrue(p['sources'])
            else: self.assertIn('provisional',self.models[mid]['controls_status'])
    def test_legacy_native_collision_rejected(self):
        self.models['legacy.drive.0']['controls'][0]['id']='predrive'
        with self.assertRaises(ValueError):validate_catalog(self.catalog)
if __name__=="__main__": unittest.main()
