#!/usr/bin/env python3
import csv,json,tempfile,unittest
from pathlib import Path
from analyze_trace import analyze,correlation
class AnalysisTests(unittest.TestCase):
 def fixture(self,root):
  prefix=root/'fixture'
  def save(suffix,rows):
   with Path(str(prefix)+suffix).open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
  Path(str(prefix)+'.metadata.json').write_text(json.dumps({'complete':True,'cadence':'continuous','trace_level':'stages'}))
  callbacks=[];pacing=[]
  for i in range(1600):
   wall=400000 if i in (1,1201) else 10000;cpu=390000 if i==1201 else 9000;start=i*1000000
   callbacks.append(dict(callback_index=i,phase='worker_and_automation' if i<1200 else 'forced_six_slot_publication',wall_start_ns=start,wall_end_ns=start+wall,wall_ns=wall,cpu_ns=cpu,deadline_ns=1e9*32/96000,deadline_miss=int(wall>1e9*32/96000)))
   pacing.append(dict(callback_index=i,cadence='continuous',sleep_requested_ns=0,sleep_start_ns=0,sleep_end_ns=0,sleep_clock_valid=0,next_callback_start_ns=(i+1)*1000000 if i<1599 else 0))
  save('.callbacks.csv',callbacks);save('.pacing.csv',pacing)
  save('.stages.csv',[dict(callback_index=1201,stage='spectral_mac',wall_ns=380000,cpu_ns=380000)])
  save('.worker.csv',[dict(stage='worker_wait',wait_requested_ns=20000000,wait_notified=0,wall_start_ns=0,wall_end_ns=21000000,wall_ns=21000000),dict(stage='worker_wait',wait_requested_ns=20000000,wait_notified=1,wall_start_ns=30000000,wall_end_ns=31000000,wall_ns=1000000)])
  return prefix
 def test_mixed_misses_and_signalled_wait(self):
  with tempfile.TemporaryDirectory() as d:
   result=analyze(self.fixture(Path(d)))
   self.assertEqual([r['misses'] for r in result['summaries']],[1,1])
   self.assertEqual([r['wall_only_misses'] for r in result['summaries']],[1,0])
   self.assertEqual(result['waits'][0]['timeout_overshoot_us'],1000)
   self.assertIsNone(result['waits'][1]['timeout_overshoot_us'])
 def test_missing_pacing_rejected(self):
  with tempfile.TemporaryDirectory() as d:
   p=self.fixture(Path(d));Path(str(p)+'.pacing.csv').unlink()
   with self.assertRaises(FileNotFoundError):analyze(p)
 def test_bad_wait_protocol_rejected(self):
  with tempfile.TemporaryDirectory() as d:
   p=self.fixture(Path(d));f=Path(str(p)+'.worker.csv');f.write_text(f.read_text().replace('20000000','30000000'))
   with self.assertRaises(AssertionError):analyze(p)
 def test_unknown_cadence_rejected(self):
  with tempfile.TemporaryDirectory() as d:
   p=self.fixture(Path(d));f=Path(str(p)+'.metadata.json');f.write_text(f.read_text().replace('continuous','invented'))
   with self.assertRaises(AssertionError):analyze(p)
 def test_constant_series_is_unavailable(self):
  self.assertIsNone(correlation([1,1],[0,1]));self.assertEqual(correlation([1,2,3],[3,2,1]),-1)
if __name__=='__main__':unittest.main()
