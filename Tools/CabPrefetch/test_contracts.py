"""Reject source drift, out-of-bank hints, and weakened inherited gates."""
import importlib.util
import subprocess
import unittest
from pathlib import Path
import prepare
import run as experiment

ROOT=Path(__file__).resolve().parents[2]
class PrefetchContracts(unittest.TestCase):
    def test_only_read_hints_are_inserted(self):
        base=subprocess.check_output(['git','show',prepare.PARENT+':'+prepare.HEADER],cwd=ROOT,text=True)
        result=prepare.generate(base,'prefetch2')
        start=result.index('                // Diagnostic hypothesis:')
        end=result.index('                accumulate(bankInput',start)
        self.assertEqual(result[:start]+result[end:],base)
        self.assertEqual(result.count('__builtin_prefetch('),4)
        self.assertNotIn('__builtin_prefetch(',base)
        with self.assertRaises(RuntimeError):prepare.generate(base+'\n','prefetch2')
        with self.assertRaises(ValueError):prepare.generate(base,'split')

    def test_all_hint_addresses_in_current_bank(self):
        for segments in range(1,129):
            for cursor in range(segments):
                index=cursor
                for partition in range(1,segments):
                    index=(index+1)%segments
                    if partition+2<segments:
                        ahead=index+2
                        if ahead>=segments:ahead-=segments
                        self.assertEqual(ahead,(cursor+partition+2)%segments)
                        self.assertTrue(0<=ahead<segments)
                        self.assertTrue(0<=partition+2<segments)
                        # With stride 2*bins, both hinted planes stay in slot.
                        for bins in (1,65,128,129,256,1024):
                            self.assertLess(ahead*2*bins+bins,segments*2*bins)

    def test_protocol_unchanged(self):
        baseline=(ROOT/'Tools/CabScalarMemory/run.py').read_text()
        candidate=(ROOT/'Tools/CabPrefetch/run.py').read_text()
        # Only the explanatory docstring differs; no acceptance predicate does.
        self.assertEqual(baseline[baseline.index('from __future__'):],candidate[candidate.index('from __future__'):])
        original=(ROOT/'Tools/CabScalarMemory/audit.py').read_text()
        self.assertEqual(original.replace("ROOT / 'Tools/CabScalarMemory'","ROOT / 'Tools/CabPrefetch'"),(ROOT/'Tools/CabPrefetch/audit.py').read_text())

spec=importlib.util.spec_from_file_location('inherited_gates',ROOT/'Tools/CabScalarMemory/test_contracts.py')
inherited=importlib.util.module_from_spec(spec);spec.loader.exec_module(inherited)
class Gates(inherited.Gates):
    # Other inherited tests run unchanged against this experiment module.
    def test_baseline_and_kind_fail_closed(self):
        with self.assertRaises(RuntimeError):experiment.generate('wrong source','prefetch2')
        with self.assertRaises(ValueError):experiment.generate('wrong source','unknown')
    def test_changes_stay_in_bounded_scope(self):
        PrefetchContracts().test_only_read_hints_are_inserted()

if __name__=='__main__':unittest.main()
