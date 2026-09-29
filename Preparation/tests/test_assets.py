"""All audio and NAM payloads here are SYNTHETIC, not hardware captures."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
import numpy as np
import soundfile as sf
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from audit_assets import inspect, batch, main, strict_json

class AssetTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.model = {"version":"test-only", "architecture":"SlimmableContainer", "config":{},
                      "weights":[.1, -.2], "sample_rate":48000,
                      "metadata":{"input_level_dbu":12.2418,"output_level_dbu":18,
                                  "training":{"checks":{"passed":True},"ignore_checks":False}}}
        self.evidence = {"source_url":"https://example.invalid/synthetic", "hardware":"SYNTHETIC FIXTURE",
                         "creator_verified":True,"verified_evidence_url":"https://example.invalid/badge",
                         "verified_checked_on":"2026-09-28", "capture_scope":"amp_head",
                         "a2_confirmed":True,"a2_evidence_url":"https://example.invalid/a2",
                         "training_lineage":"dry_wet", "redistribution":"unknown"}
    def tearDown(self): self.tmp.cleanup()
    def nam(self, payload=None, name="synthetic.nam"):
        p=self.root/name
        p.write_text(json.dumps(self.model if payload is None else payload),encoding="utf-8")
        return p
    def audit(self, p, evidence=None, scope=None):
        e=self.evidence if evidence is None else evidence
        return inspect(p,{hashlib.sha256(p.read_bytes()).hexdigest():e},scope)
    def codes(self, report): return {x["code"] for x in report["issues"]}
    def test_complete_metadata_still_not_runtime_or_license_approval(self):
        r=self.audit(self.nam())
        self.assertEqual(r["status"],"metadata_ready")
        self.assertEqual(r["nam"]["runtime_load"],"not_tested")
        self.assertFalse(r["distribution"]["automatic_permission"])
    def test_wrapper_does_not_prove_a2(self):
        e=dict(self.evidence); e.pop("a2_confirmed")
        self.assertIn("A2_UNCONFIRMED",self.codes(self.audit(self.nam(),e)))
    def test_missing_input_calibration(self):
        self.model["metadata"].pop("input_level_dbu")
        self.assertIn("INPUT_LEVEL_DBU_MISSING",self.codes(self.audit(self.nam())))
    def test_zero_dbu_is_valid(self):
        self.model["metadata"]["input_level_dbu"]=0
        self.assertNotIn("INPUT_LEVEL_DBU_MISSING",self.codes(self.audit(self.nam())))
    def test_bool_calibration_rejected(self):
        self.model["metadata"]["input_level_dbu"]=True
        self.assertIn("INPUT_LEVEL_DBU_MISSING",self.codes(self.audit(self.nam())))
    def test_source_flags_not_hidden(self):
        self.model["metadata"]["training"]={"checks":{"passed":False},"ignore_checks":True}
        c=self.codes(self.audit(self.nam()))
        self.assertTrue({"TRAINING_CHECKS_REVIEW","TRAINING_CHECKS_BYPASSED"} <= c)
    def test_scope_mismatch(self):
        e=dict(self.evidence,capture_scope="amp_cab")
        self.assertIn("SCOPE_MISMATCH",self.codes(self.audit(self.nam(),e,"amp_head")))
    def test_evidence_hash_bound(self):
        p=self.nam(); r=inspect(p,{"0"*64:self.evidence})
        self.assertIn("VERIFIED_UNCONFIRMED",self.codes(r))
    def test_conflicting_calibration(self):
        self.model["input_level_dbu"]=22
        self.assertIn("CALIBRATION_CONFLICT",self.codes(self.audit(self.nam())))
    def test_convert_remains_labeled(self):
        e=dict(self.evidence,training_lineage="convert")
        self.assertIn("CONVERT_REFERENCE",self.codes(self.audit(self.nam(),e)))
    def test_nonfinite_and_duplicate_json_rejected(self):
        for text in ('{"a":NaN}', '{"a":1e999}', '{"a":1,"a":2}'):
            with self.assertRaises(ValueError): strict_json(text)
    def test_bad_nam_schema(self):
        for patch in ({"weights":[]},{"weights":[True]},{"config":[]},{"metadata":None}):
            m=copy.deepcopy(self.model); m.update(patch)
            self.assertEqual(self.audit(self.nam(m))["status"],"invalid")
    def test_bad_json_file(self):
        p=self.root/"bad.nam"; p.write_text("not-json")
        self.assertEqual(inspect(p)["status"],"invalid")
    def test_aiff_and_wav_have_same_onset_metrics_no_a2_requirement(self):
        x=np.zeros((480,2)); x[48,0]=.5; x[96,1]=-.25
        for ext,form in (("wav","WAV"),("aiff","AIFF")):
            p=self.root/f"synthetic.{ext}"; sf.write(p,x,48000,format=form,subtype="PCM_24")
            e=dict(self.evidence,capture_scope="cabinet_ir"); e.pop("a2_confirmed")
            r=self.audit(p,e)
            self.assertEqual(r["ir"]["a2_requirement"],"not_applicable")
            self.assertEqual([m["onset_ms"] for m in r["ir"]["per_channel"]],[1,2])
            self.assertNotIn("A2_UNCONFIRMED",self.codes(r))
    def test_zero_ir_invalid(self):
        p=self.root/"silent.wav"; sf.write(p,np.zeros(100),48000)
        self.assertIn("SILENT_IR",self.codes(inspect(p)))
    def test_nonfinite_audio_invalid(self):
        p=self.root/"nan.wav"; sf.write(p,np.array([0,np.nan,1]),48000,subtype="FLOAT")
        self.assertEqual(inspect(p)["status"],"invalid")
    def test_duplicates_detected(self):
        a=self.nam(); b=self.nam(name="copy.nam")
        self.assertEqual(len(batch([a,b])["duplicate_groups"]),1)
    def test_source_bytes_not_modified(self):
        p=self.root/"ir.wav"; sf.write(p,np.array([0,.25,0]),48000,subtype="PCM_24")
        before=p.read_bytes(); inspect(p); self.assertEqual(p.read_bytes(),before)
    def test_output_cannot_overwrite_source(self):
        p=self.nam(); before=p.read_bytes()
        self.assertEqual(main([str(p),"--output",str(p)]),2)
        self.assertEqual(p.read_bytes(),before)
    def test_missing_file_and_unsupported_type(self):
        self.assertEqual(inspect(self.root/"missing.nam")["status"],"invalid")
        p=self.root/"x.txt"; p.write_text("x"); self.assertEqual(inspect(p)["status"],"invalid")
    def test_corrupt_evidence_schema(self):
        p=self.nam(); e=self.root/"e.json"; e.write_text("[]")
        self.assertEqual(main([str(p),"--evidence",str(e)]),2)
    def test_cli_creates_report_with_nonzero_for_pending(self):
        p=self.nam(); out=self.root/"report.json"
        self.assertEqual(main([str(p),"--output",str(out)]),1)
        r=json.loads(out.read_text()); self.assertFalse(r["online_verification_performed"])

if __name__=="__main__": unittest.main()
