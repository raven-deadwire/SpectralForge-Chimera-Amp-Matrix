"""Negative controls: a file hash or a host lane label must not stand in for data."""
import copy
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import types
import unittest
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'Tools'))
import xml.etree.ElementTree as ET

spec=importlib.util.spec_from_file_location('fixture',Path(__file__).resolve().parents[1]/'Tools/eq_legacy_fixture.py')
f=importlib.util.module_from_spec(spec);spec.loader.exec_module(f)

class EvidenceContracts(unittest.TestCase):
    def test_truncated_or_wrong_binary_rejected(self):
        for b in (b'',b'12345678',struct.pack('<II',0x21324356,30)+b'<PARAMS/>\0'):
            with self.assertRaises((ValueError,struct.error)):f.core(b)
    def test_only_wrapper_trailer_removed(self):
        payload=b'<PARAMS schemaVersion="11"/>'
        raw=struct.pack('<II',0x21324356,len(payload))+payload+b'\0'
        self.assertEqual(f.core(raw+b'JUCE trailer'),raw)
    def test_fixture_corruption_fails(self):
        root=Path(__file__).resolve().parent/'fixtures/eq-beta2'
        import shutil
        with tempfile.TemporaryDirectory() as d:
            out=Path(d)/'fixtures';shutil.copytree(root,out)
            p=out/'pattern.chimera';p.write_bytes(p.read_bytes()+b'corruption')
            with self.assertRaises(ValueError):f.verify(types.SimpleNamespace(output=out))
    def test_metadata_loss_is_not_roundtrip(self):
        with tempfile.TemporaryDirectory() as d:
            paths=[Path(d)/str(i) for i in range(2)]
            for p,content in zip(paths,(b'<PARAMS><EQ_EDIT_STATE toneOccupied="4087"/></PARAMS>',b'<PARAMS/>')):
                p.write_bytes(struct.pack('<II',0x21324356,len(content))+content+b'\0')
            with self.assertRaises(ValueError):f.roundtrip(types.SimpleNamespace(before=paths[0],after=paths[1]))
    def test_canonical_ignores_attribute_order_only(self):
        self.assertEqual(f.canonical(ET.fromstring('<P a="1" b="2"/>')),f.canonical(ET.fromstring('<P b="2" a="1"/>')))
        self.assertNotEqual(f.canonical(ET.fromstring('<P><Q id="1"/></P>')),f.canonical(ET.fromstring('<P><Q id="2"/></P>')))
if __name__=='__main__':unittest.main()
