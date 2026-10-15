import copy
import unittest
import run as experiment

class Gates(unittest.TestCase):
    def test_missing_exact_population_cannot_pass(self):
        log=('PASS CAB realtime equivalence routes=144 '\
             'exact_float_samples=2645088 fade_mix_samples=388800 exact_bits=1 immutable_spectra=1 '\
             'active=6 fading=3 fade_ms=50 latency=0')
        self.assertTrue(experiment.contracts_present(log))
        for before,after in [('2645088','2645087'),('388800','388799'),('routes=144','routes=143'),('latency=0','latency=1')]:
            self.assertFalse(experiment.contracts_present(log.replace(before,after)))

    def test_failed_or_missing_gate_blocks_eligibility(self):
        gate=dict(returncode=0,frozen_bits_and_latency=True,
                  tests=[dict(passed=True) for _ in range(5)],
                  timing=[dict(p99_within_deadline=True) for _ in range(66)])
        self.assertTrue(experiment.gates_pass(gate))
        for key,value in [('returncode',1),('frozen_bits_and_latency',False),('tests',gate['tests'][:-1]),('timing',gate['timing'][:-1])]:
            broken=copy.deepcopy(gate);broken[key]=value
            self.assertFalse(experiment.gates_pass(broken))
        for key,field in [('tests','passed'),('timing','p99_within_deadline')]:
            broken=copy.deepcopy(gate);broken[key][0][field]=False
            self.assertFalse(experiment.gates_pass(broken))

if __name__=='__main__': unittest.main()
