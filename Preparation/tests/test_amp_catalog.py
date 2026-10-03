"""Regression tests for descriptor/state contracts, not hardware sound accuracy."""
from pathlib import Path
import copy
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from catalog import build_catalog, validate_catalog
from amp_controls import amp_panels, REVIEWED, PARTIAL, PENDING, validate_amp_panels

class AmpCatalogTests(unittest.TestCase):
    def setUp(self):
        self.catalog=build_catalog()
        self.amps={m['id']:m for m in self.catalog['models'] if m['location']=='rig'}
    def keys(self, mid):return {c['id']:c for c in self.amps[mid]['controls']}
    def test_23_active_24_serialized_amp_targets(self):
        self.assertEqual(len(self.amps),24)
        self.assertEqual(sum(m.get('active',True) for m in self.amps.values()),23)
        self.assertFalse(self.amps['planned.amp.engl']['active'])
        self.assertTrue(self.amps['planned.amp.engl-e670fe']['active'])
        self.assertEqual(len(self.catalog['models']),81)
    def test_all_existing_and_planned_have_model_specific_panels(self):
        self.assertEqual(set(self.amps),set(amp_panels()))
        for m in self.amps.values():self.assertTrue(m['controls']);self.assertTrue(m['amp_panel']['channels'])
    def test_no_audio_capture_or_dsp_approval(self):
        for m in self.amps.values():
            p=m['amp_panel']
            self.assertFalse(p['circuit_response_verified']);self.assertFalse(p['capture_approved']);self.assertFalse(p['production_dsp_connected'])
            for c in m['controls']:self.assertIsNone(c['dsp_binding'])
    def test_continuous_hardware_values_are_positions(self):
        for m in self.amps.values():
            for c in m['controls']:
                if c['kind']=='knob':self.assertEqual((c['minimum'],c['maximum'],c['unit']),(0,1,'position'))
    def test_existing_raw_indices_stable(self):
        for i in range(15):self.assertEqual(self.amps[f'legacy.amp.{i}']['legacy']['raw_index'],i)
    def test_twin_no_fictional_master_or_generic_gain(self):
        keys=self.keys('legacy.amp.0')
        self.assertFalse(any('gain' in k or 'master' in k for k in keys))
        self.assertEqual(keys['hw.vibrato.reverb']['channels'],['vibrato'])
        self.assertIn('hw.normal.bright',keys)
    def test_jtm45_two_loudness_no_master(self):
        keys=self.keys('legacy.amp.1');self.assertEqual(len(keys),6)
        self.assertIn('hw.high_treble',keys);self.assertIn('hw.normal',keys);self.assertNotIn('hw.master',keys)
    def test_6505_shared_eq_independent_pre_post(self):
        keys=self.keys('legacy.amp.2')
        self.assertEqual(keys['hw.mid']['channels'],[])
        for ch in ['lead','rhythm']:
            self.assertIn(f'hw.{ch}.pre_gain',keys);self.assertIn(f'hw.{ch}.post_gain',keys)
            self.assertNotIn(f'hw.{ch}.mid',keys)
    def test_rectifier_modes_and_revision_limit(self):
        keys=self.keys('legacy.amp.3')
        self.assertEqual(keys['hw.ch1.mode']['options'],['CLEAN','PUSHED'])
        self.assertEqual(keys['hw.ch3.mode']['options'],['RAW','VINTAGE','MODERN'])
        self.assertEqual(self.amps['legacy.amp.3']['amp_panel']['review_status'],PARTIAL)
    def test_mark_iv_rhythm_shares_bass_middle(self):
        keys=self.keys('legacy.amp.4')
        self.assertEqual(keys['hw.rhythm.bass']['channels'],['r1','r2'])
        self.assertNotIn('hw.r1.bass',keys);self.assertNotIn('hw.r2.bass',keys)
        self.assertIn('hw.lead.gain',keys);self.assertIn('hw.lead.drive',keys)
    def test_mark_iv_geq_labels_and_pull_controls(self):
        keys=self.keys('legacy.amp.4')
        for hz in ['80','240','750','2200','6600']:self.assertIn('hw.geq.'+hz,keys)
        self.assertIn('hw.lead.bright',keys);self.assertIn('hw.r2.presence_shift',keys)
    def test_svt_vr_not_cl_panel(self):
        vr=self.keys('legacy.amp.5');cl=self.keys('planned.amp.svt-cl')
        self.assertEqual(len(vr['hw.ch1.mid_frequency']['options']),3)
        self.assertEqual(len(cl['hw.mid_frequency']['options']),5)
        self.assertNotIn('hw.ch2.midrange',vr);self.assertNotIn('hw.master',vr)
        self.assertIn('hw.gain',cl);self.assertIn('hw.master',cl)
    def test_gk_four_bands_and_distinct_biamp_target(self):
        keys=self.keys('legacy.amp.6')
        for k in ['bass','lo_mid','hi_mid','treble','crossover','low_master','high_master']:self.assertIn('hw.'+k,keys)
    def test_hybrid_does_not_become_isolated_darkglass_head(self):
        m=self.amps['legacy.amp.7'];self.assertIn('chain',m['amp_panel']['reference'])
        keys=self.keys(m['id']);self.assertIn('hw.b7k.blend',keys);self.assertIn('hw.db751.gain',keys)
        self.assertNotEqual(m['amp_panel']['review_status'],REVIEWED)
    def test_vox_cut_no_middle(self):
        keys=self.keys('legacy.amp.8');self.assertIn('hw.tone_cut',keys)
        self.assertNotIn('hw.normal.treble',keys);self.assertNotIn('hw.top_boost.middle',keys)
    def test_orange_clean_and_dirty_differ(self):
        keys=self.keys('legacy.amp.9');self.assertNotIn('hw.clean.middle',keys);self.assertIn('hw.dirty.middle',keys)
        self.assertIn('hw.attenuator',keys)
    def test_superbassman_od_mid_not_tripled(self):
        keys=self.keys('legacy.amp.10')
        self.assertIn('hw.overdrive.mid_frequency',keys);self.assertIn('hw.overdrive.mid_level',keys)
        self.assertNotIn('hw.overdrive.mid',keys);self.assertIn('hw.vintage.mid',keys)
    def test_subway_is_plus_with_pending_primary_panel(self):
        m=self.amps['legacy.amp.11'];self.assertIn('D-800+',m['amp_panel']['reference'])
        self.assertIn('hw.high_pass',self.keys(m['id']));self.assertEqual(m['amp_panel']['review_status'],PENDING)
    def test_matchless_channel_two_rotary_not_bass_middle_treble(self):
        keys=self.keys('legacy.amp.12');self.assertEqual(len(keys['hw.ch2.tone']['options']),6)
        self.assertNotIn('hw.ch2.bass',keys);self.assertIn('hw.master_bypass',keys)
    def test_ods_clone_scope_not_promoted(self):
        p=self.amps['legacy.amp.13']['amp_panel'];self.assertIn('clone',p['reference']);self.assertEqual(p['review_status'],PENDING)
        self.assertNotIn('hw.deep',self.keys('legacy.amp.13'))
    def test_eich_taste_and_four_bands(self):
        keys=self.keys('legacy.amp.14')
        for k in ['taste','lo','lo_mid','hi_mid','hi']:self.assertIn('hw.'+k,keys)
    def test_zuta_published_keys_preserved_and_channel_visibility(self):
        keys=self.keys('planned.amp.zuta-gbg120')
        for i in range(1,5):self.assertEqual(keys[f'ch{i}.gain']['channels'],[f'ch{i}'])
        self.assertIn('ch4.tight',keys);self.assertNotIn('ch1.tight',keys)
    def test_engl_not_savage_and_shared_eq(self):
        m=self.amps['planned.amp.engl'];self.assertIn('Ironball E606',m['amp_panel']['reference'])
        self.assertEqual(self.keys(m['id'])['hw.bass']['channels'],[])
    def test_e670fe_distinct_five_paths_and_unverified_response(self):
        m=self.amps['planned.amp.engl-e670fe'];p=m['amp_panel']
        self.assertEqual(len(p['channels']),5)
        self.assertEqual(len(m['controls']),32)
        self.assertEqual(p['review_status'],PARTIAL)
        self.assertFalse(p['circuit_response_verified'])
        self.assertNotIn('hw.driver.gain',self.keys(m['id']))
    def test_diezel_four_channels(self):
        p=self.amps['planned.amp.diezel-vh4']['amp_panel'];self.assertEqual(len(p['channels']),4)
        self.assertIn('hw.deep',self.keys('planned.amp.diezel-vh4'))
    def test_fortin_actual_distinct_channel_controls(self):
        keys=self.keys('planned.amp.fortin')
        self.assertIn('hw.ep.girth',keys);self.assertIn('hw.kk.gain2',keys)
        self.assertEqual(keys['hw.gain_eq.sweep']['channels'],['ep','kk'])
        self.assertNotIn('hw.clean.gain',keys)
        self.assertIn('hw.master2',keys);self.assertIn('hw.master2_select',keys)
    def test_soldano_shared_eq_and_revision_disclaimer(self):
        m=self.amps['planned.amp.soldano'];keys=self.keys(m['id'])
        self.assertEqual(keys['hw.middle']['channels'],[])
        self.assertIn('LTD capture revision pending',m['amp_panel']['reference'])
    def test_sunn_not_silently_verified_or_substituted(self):
        p=self.amps['planned.amp.sunn']['amp_panel'];self.assertEqual(p['review_status'],PENDING)
        self.assertIn('1970s',p['reference']);self.assertFalse(p['capture_approved'])
    def test_amp_controls_do_not_consume_pedal_slots(self):
        self.assertEqual(self.catalog['max_pedals'],5)
        self.assertTrue(all(m['location']=='rig' for m in self.amps.values()))
    def test_bad_visibility_rejected(self):
        self.amps['legacy.amp.0']['controls'][0]['channels']=['absent']
        with self.assertRaises(ValueError):validate_amp_panels(self.catalog['models'])
    def test_verified_without_source_rejected(self):
        self.amps['legacy.amp.0']['amp_panel']['sources']=[]
        with self.assertRaises(ValueError):validate_amp_panels(self.catalog['models'])
    def test_shared_parameter_ids_unique(self):
        for m in self.amps.values():self.assertEqual(len(m['controls']),len({c['id'] for c in m['controls']}))
    def test_invalid_initial_channel_rejected(self):
        self.amps['legacy.amp.0']['amp_panel']['initial_channel']='absent'
        with self.assertRaises(ValueError):validate_amp_panels(self.catalog['models'])
if __name__=='__main__':unittest.main()
