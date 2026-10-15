import importlib.util
from pathlib import Path
import unittest
spec=importlib.util.spec_from_file_location('s8t_ab',Path(__file__).with_name('run.py'))
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
    def test_probe_matches_completed_protocol_exactly(self):
        reference_spec=importlib.util.spec_from_file_location('old_protocol',Path(__file__).parents[1]/'CabNeonAB/run.py')
        reference=importlib.util.module_from_spec(reference_spec);reference_spec.loader.exec_module(reference)
        source=Path(__file__).resolve().parents[2]/'Tests/OriginalCabIntegrationTests.cpp'
        self.assertEqual(m.generate_probe(source),reference.generate_probe(source))

    def test_gate_failure_in_either_arm_blocks_acceptance(self):
        import copy
        good=dict(returncode=0,frozen_bits_and_latency=True,
                  tests=[dict(passed=True) for _ in range(5)],
                  timing=[dict(p99_within_deadline=True) for _ in range(66)])
        gates={'A':copy.deepcopy(good),'B':copy.deepcopy(good)}
        self.assertTrue(m.gates_pass(gates))
        for arm in ('A','B'):
            broken=copy.deepcopy(gates);broken[arm]['timing'][0]['p99_within_deadline']=False
            self.assertFalse(m.gates_pass(broken))
            broken=copy.deepcopy(gates);broken[arm]['frozen_bits_and_latency']=False
            self.assertFalse(m.gates_pass(broken))
        self.assertFalse(m.gates_pass({'B':good}))

    def test_pinned_patch_produces_exact_S8T(self):
        import tempfile,subprocess,hashlib
        root=Path(__file__).resolve().parents[2]
        patch=root/m.PATCH_PATH
        self.assertEqual(m.digest(patch),m.PATCH_SHA256)
        before=subprocess.check_output(['git','show',m.CANDIDATE+':'+m.HEADER],cwd=root)
        with tempfile.TemporaryDirectory() as temp:
            work=Path(temp);header=work/m.HEADER;header.parent.mkdir()
            header.write_bytes(before)
            subprocess.run(['git','apply','--check',str(patch)],cwd=work,check=True)
            subprocess.run(['git','apply',str(patch)],cwd=work,check=True)
            self.assertEqual(m.digest(header),m.HEADER_SHA256)

if __name__=='__main__': unittest.main()
