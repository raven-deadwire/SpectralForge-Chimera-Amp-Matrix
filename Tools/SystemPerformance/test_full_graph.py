import copy
import unittest
from unittest.mock import patch
import performance
from run_full_graph import verify_host


class HostEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.h = dict(schema='chimera.full-graph.v1', synthetic=True,
                      requirements_certified=False, publication_allowed=False,
                      build_config='Release', binary_sha256='a'*64, input_sha256='b'*64,
                      state_sha256='c'*64, ir_sha256='d'*64, instances=2, block=128,
                      sample_rate=48000, oversampling=4, parameters_unchanged=True,
                      finite=True, dropped_rows=0, callback_cpp_allocations=0,
                      instance_energy=[1.,2.], instance_calls=[375,375], callback_count=375)

    def check(self,h):
        verify_host(h,'a'*64,'b'*64,'c'*64,'d'*64,2,128)

    def test_valid_observation(self):
        self.check(self.h)

    def test_rejects_identity_scope_allocation_and_inactive_nodes(self):
        for key, value in dict(binary_sha256='e'*64, state_sha256='e'*64, ir_sha256='e'*64,
                               input_sha256='e'*64, synthetic=False, publication_allowed=True,
                               requirements_certified=True, callback_cpp_allocations=1,
                               dropped_rows=1, build_config='Debug', parameters_unchanged=False,
                               finite=False, instances=1, block=256, oversampling=1,
                               instance_energy=[1.,0.], instance_calls=[375,374]).items():
            with self.subTest(key=key):
                h=copy.deepcopy(self.h);h[key]=value
                with self.assertRaises(ValueError):self.check(h)

    def test_bool_is_not_allocation_count(self):
        self.h['callback_cpp_allocations']=False
        with self.assertRaises(ValueError):self.check(self.h)

    def test_windows_cim_unavailable_stays_unknown(self):
        with patch.object(performance.platform,'system',return_value='Windows'), \
             patch.object(performance,'command',side_effect=['{"cpu":"","cores":0,"ram":null}',None]):
            env=performance.environment()
        self.assertIsNone(env['ram_bytes'])
        self.assertIsNone(env['physical_cores'])


if __name__ == '__main__':
    unittest.main()
