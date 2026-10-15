import importlib.util
from pathlib import Path
import unittest
spec=importlib.util.spec_from_file_location('neon_ab',Path(__file__).with_name('run.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Contracts(unittest.TestCase):
    def test_strong_reduction(self):
        self.assertTrue(m.comparison([20,19,21,20,19,21],[1,-1,.5,-.5,1,-1])['demonstrated'])
    def test_environment_envelope_blocks_claim(self):
        self.assertFalse(m.comparison([6]*6,[7,0,0,0,0,0])['demonstrated'])
    def test_order_reversal_blocks_claim(self):
        self.assertFalse(m.comparison([20,-1]*3,[0]*6)['demonstrated'])
    def test_regression_direction(self):
        r=m.comparison([-10]*6,[1]*6)
        self.assertTrue(r['regression']);self.assertFalse(r['demonstrated'])
    def test_incomplete_and_nan_fail_closed(self):
        for a in ([1]*5,[float('nan')]*6):
            with self.assertRaises(ValueError): m.comparison(a,[0]*6)
        with self.assertRaises(ValueError): m.analyze([])
    def test_harness_keeps_contracts(self):
        root=Path(__file__).resolve().parents[2]
        text=m.generate_probe(root/'Tests/OriginalCabIntegrationTests.cpp')
        for needle in ['times[1584]<1e6*block/sr','reference.getLatency()==0','allocations==0 && deletions==0','workerSchedulingValid']:
            self.assertIn(needle,text)
        self.assertIn('const std::vector<int> blocks{32,64}',text)
        self.assertNotIn('for(double sr:{44100.',text)
if __name__=='__main__': unittest.main()
