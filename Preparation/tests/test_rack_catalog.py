"""R1a descriptor regression and unsupported-claim gates, not audio tests."""
from pathlib import Path
import unittest,sys,copy
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from catalog import build_catalog,validate_catalog
from rack_controls import rack_panels,REVIEWED,PARTIAL

class RackCatalogTests(unittest.TestCase):
    def setUp(self):
        self.catalog=build_catalog();self.models={m['id']:m for m in self.catalog['models']};self.panels=rack_panels()
    def controls(self,mid):return {c['id']:c for c in self.models[mid]['controls']}
    def test_inventory_stable_and_pending_space_modulation(self):
        self.assertEqual(len(self.models),80);self.assertEqual(len(self.panels),9)
        post=[m for m in self.models.values() if m['location']=='post'];self.assertEqual(len(post),21)
        self.assertEqual(sum('rack_panel' not in m for m in post),12)
    def test_nine_models_keep_production_raw_index(self):
        for family in ['bus','preamp','eq']:
            for i in range(3):self.assertEqual(self.models[f'legacy.{family}.{i}']['legacy']['raw_index'],i)
    def test_dsp_and_meter_claims_not_promoted(self):
        for mid in self.panels:
            for key in ['production_dsp_connected','capture_approved','circuit_response_verified','meter_signal_available']:
                self.assertIs(self.models[mid]['rack_panel'][key],False)
    def test_1176_ratio_and_direction_not_generic_threshold(self):
        cs=self.controls('legacy.bus.1');self.assertNotIn('hw.threshold',cs)
        self.assertEqual(cs['hw.ratio']['options'],['4:1','8:1','12:1','20:1','ALL BUTTONS'])
        self.assertIn('faster',cs['hw.attack']['note']);self.assertIn('hw.attack_off',cs)
    def test_1176_and_la2a_meter_calibrations_differ(self):
        self.assertEqual(self.controls('legacy.bus.1')['hw.meter']['options'],['GR','+4','+8','OFF'])
        self.assertEqual(self.controls('legacy.bus.2')['hw.meter']['options'],['+4','GR','+10'])
    def test_la2a_no_fictional_time_or_ratio_controls(self):
        cs=self.controls('legacy.bus.2')
        for key in ['hw.attack','hw.release','hw.ratio','hw.threshold']:self.assertNotIn(key,cs)
        self.assertEqual(cs['hw.r37']['group'],'REAR / CALIBRATION')
    def test_ssl_original_discrete_switches(self):
        cs=self.controls('legacy.bus.0')
        self.assertEqual(cs['hw.attack']['options'],['0.1 ms','0.3 ms','1 ms','3 ms','10 ms','30 ms'])
        self.assertEqual(cs['hw.ratio']['options'],['2:1','4:1','10:1'])
        self.assertEqual(cs['hw.release']['options'][-1],'AUTO');self.assertNotIn('hw.mix',cs)
    def test_neve_preamp_and_eq_sections_not_conflated(self):
        pre=self.controls('legacy.preamp.0');eq=self.controls('legacy.eq.1')
        self.assertNotIn('hw.drive',pre);self.assertNotIn('hw.hf_gain',pre);self.assertIn('hw.gain',pre)
        self.assertNotIn('hw.gain',eq);self.assertIn('hw.mf_frequency',eq);self.assertIn('hw.high_pass',eq)
    def test_neve_hf_fixed_and_mid_frequencies_stepped(self):
        eq=self.controls('legacy.eq.1');self.assertNotIn('hw.hf_frequency',eq)
        self.assertEqual(eq['hw.mf_frequency']['options'],['360 Hz','700 Hz','1.6 kHz','3.2 kHz','4.8 kHz','7.2 kHz'])
    def test_avalon_is_v5_not_v55_and_stays_partial(self):
        p=self.models['legacy.preamp.1']['rack_panel'];self.assertEqual(p['review_status'],PARTIAL)
        cs=self.controls('legacy.preamp.1');self.assertEqual(len(cs['hw.boost']['options']),21);self.assertEqual(len(cs['hw.tone']['options']),10)
        self.assertNotIn('hw.bass',cs)
    def test_isa_separate_trim_instrument_and_four_position_gain(self):
        cs=self.controls('legacy.preamp.2');self.assertEqual(len(cs['hw.gain']['options']),4)
        for k in ['hw.trim','hw.gain_range','hw.instrument_gain','hw.instrument_z']:self.assertIn(k,cs)
        self.assertEqual(len(cs['hw.impedance']['options']),4)
    def test_ssl_four_bands_and_no_fictional_module_filters(self):
        cs=self.controls('legacy.eq.0')
        for band in ['hf','hmf','lmf','lf']:self.assertIn(f'hw.{band}_gain',cs)
        for key in ['hw.hmf_q','hw.lmf_q','hw.hf_bell','hw.lf_bell','hw.black']:self.assertIn(key,cs)
        self.assertNotIn('hw.high_pass',cs)
    def test_pultec_independent_boost_attenuation_and_frequencies(self):
        cs=self.controls('legacy.eq.2')
        for k in ['hw.lf_boost','hw.lf_atten','hw.hf_boost','hw.hf_atten']:self.assertIn(k,cs)
        self.assertEqual(len(cs['hw.hf_frequency']['options']),7);self.assertEqual(len(cs['hw.hf_atten_frequency']['options']),3)
    def test_full_review_claims_have_sources_and_normalized_knobs(self):
        self.assertEqual(sum(p['review_status']==REVIEWED for p in self.panels.values()),7)
        for m in self.models.values():
            if 'rack_panel' not in m:continue
            self.assertTrue(m['rack_panel']['sources'])
            for c in m['controls']:
                self.assertIsNone(c['dsp_binding'])
                if c['kind']=='knob':self.assertEqual((c['minimum'],c['maximum'],c['unit']),(0,1,'position'))
    def test_reject_fabricated_dsp_claim(self):
        self.models['legacy.bus.1']['rack_panel']['production_dsp_connected']=True
        with self.assertRaises(ValueError):validate_catalog(self.catalog)
    def test_reject_missing_panel(self):
        del self.models['legacy.bus.1']['rack_panel']
        with self.assertRaises(ValueError):validate_catalog(self.catalog)
    def test_reject_control_id_and_binding_collision(self):
        self.models['legacy.bus.1']['controls'][0]['id']='buscomp'
        with self.assertRaises(ValueError):validate_catalog(self.catalog)
    def test_reject_nonfinite_position(self):
        self.models['legacy.bus.1']['controls'][0]['initial']=float('inf')
        with self.assertRaises(ValueError):validate_catalog(self.catalog)

if __name__=='__main__':unittest.main()
