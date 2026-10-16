#!/usr/bin/env python3
"""Negative controls for CAB/IR evidence consumption, not product acceptance."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
import xml.etree.ElementTree as ET

import cab_ir_acceptance as cab


class EvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.junit, self.inventory = self.folder/'results.xml', self.folder/'inventory.json'
        self.root = ET.Element('testsuite')
        for suite in cab.SUITES:
            case = ET.SubElement(self.root, 'testcase', name='ChimeraCabIRAcceptance_'+suite, status='run')
            if suite == 'factory':
                lines = [f'CASE factory preset={p} phase={s} files={0 if s == 0 else 25} PASS' for p in (31,32,33) for s in range(3)]
            elif suite == 'boundaries':
                lines = [f'CASE boundary {name} accepted={int(ok)} bytes=100 PASS' for name, ok in cab.BOUNDARIES.items()]
            else:
                lines = [f'CASE preserve lane={l} mic={m} rejected={f}.wav PASS' for l in range(3) for m in range(2) for f in ('missing','corrupt','silent')]
            ET.SubElement(case,'system-out').text = '\n'.join(lines+[cab.MARKERS[suite]])
        self.inventory.write_text(json.dumps({'tests':[{'name':c.attrib['name']} for c in self.root]}))

    def read(self):
        ET.ElementTree(self.root).write(self.junit)
        return cab.claims(self.junit,self.inventory)

    def test_complete_fixture_maps_exactly_six_ids(self):
        self.assertEqual(self.read(), {cid:True for ids in cab.SUITES.values() for cid in ids})

    def test_missing_junit_and_inventory_do_not_pass(self):
        self.assertFalse(any(cab.claims(self.junit,self.inventory).values()))
        self.read();self.inventory.unlink()
        self.assertFalse(any(cab.claims(self.junit,self.inventory).values()))

    def test_skip_failure_error_and_not_run_block(self):
        original=copy.deepcopy(self.root)
        for tag in ('skipped','failure','error','notrun'):
            self.root=copy.deepcopy(original)
            for case in self.root:
                if tag=='notrun':case.set('status','notrun')
                else:ET.SubElement(case,tag)
            self.assertFalse(any(self.read().values()))

    def test_marker_without_actual_cases_cannot_pass(self):
        for suite, case in zip(cab.SUITES,self.root):case.find('system-out').text=cab.MARKERS[suite]
        self.assertFalse(any(self.read().values()))

    def test_omitted_boundary_or_slot_cannot_pass(self):
        for case in self.root:
            lines=case.find('system-out').text.splitlines();case.find('system-out').text='\n'.join(lines[1:])
        self.assertFalse(any(self.read().values()))

    def test_duplicate_cases_rejected(self):
        self.root.append(copy.deepcopy(self.root[0]))
        with self.assertRaises(ValueError):self.read()

    def test_duplicate_inventory_rejected(self):
        data=json.loads(self.inventory.read_text());data['tests'].append(data['tests'][0]);self.inventory.write_text(json.dumps(data))
        with self.assertRaises(ValueError):self.read()

    def test_wrong_boundary_or_empty_populated_phase_cannot_pass(self):
        self.root[0].find('system-out').text=self.root[0].find('system-out').text.replace('files=25','files=0')
        self.root[1].find('system-out').text=self.root[1].find('system-out').text.replace('accepted=0','accepted=1')
        result=self.read();self.assertFalse(result['A2.07']);self.assertFalse(result['A5.05'])

    def test_summary_from_other_case_cannot_pass(self):
        a,b=self.root[0].find('system-out'),self.root[1].find('system-out');a.text,b.text=b.text,a.text
        result=self.read();self.assertFalse(result['A5.01']);self.assertFalse(result['A6.05'])

    def test_failed_bundle_cannot_claim_pass(self):
        self.read()
        self.assertEqual(cab.bundle_claims({'kind':'linux','outcomes':{'ctest':'failure'}},{'ctest':self.junit,'inventory':self.inventory}),{})


    def integration_fixture(self):
        from test_release_consolidator import Consolidation
        fixture = Consolidation();fixture.setUp();self.addCleanup(fixture.doCleanups)
        self.read()
        for platform in ('windows','linux','macos'):
            def append_cases(text):
                root = ET.fromstring(text)
                root.extend(copy.deepcopy(list(self.root)))
                return ET.tostring(root, encoding='unicode')
            def append_inventory(text):
                data=json.loads(text)
                data['tests'].extend({'name':c.attrib['name']} for c in self.root)
                return json.dumps(data)
            fixture.raw_change(platform,'ctest',append_cases)
            fixture.raw_change(platform,'inventory',append_inventory)
        return fixture

    def test_product_gate_promotes_only_six_ids_with_all_platforms(self):
        fixture=self.integration_fixture();result,output=fixture.run_gate()
        self.assertEqual(result['producer']['errors'],[])
        for ids in cab.SUITES.values():
            for cid in ids:self.assertEqual(fixture.status(result,cid),'PASS')
        for cid in ('A5.06','A6.06','A8.03','B14.RESULT','I2.LIVE_REMOVE_DAW'):
            self.assertEqual(fixture.status(result,cid),'BLOCKED')
        self.assertFalse(result['ready'])
        self.assertFalse(json.loads((output/'checks/A8.02.json').read_text())['scope']['publication_authorized'])

    def test_one_platform_missing_or_skipped_cannot_promote(self):
        fixture=self.integration_fixture()
        fixture.raw_change('macos','ctest',lambda text:text.replace('name="ChimeraCabIRAcceptance_preservation" status="run"','name="ChimeraCabIRAcceptance_preservation" status="notrun"'))
        result,_=fixture.run_gate()
        self.assertEqual(fixture.status(result,'A8.02'),'BLOCKED')
        self.assertEqual(fixture.status(result,'A5.01'),'PASS')

    def test_foreign_run_and_tampered_raw_bundle_cannot_promote(self):
        fixture=self.integration_fixture()
        fixture.mutate('linux',lambda doc:doc['revision'].update(run_id='456'))
        result,_=fixture.run_gate()
        for ids in cab.SUITES.values():
            for cid in ids:self.assertEqual(fixture.status(result,cid),'BLOCKED')
        self.assertTrue(result['producer']['errors'])
        path=fixture.folder('windows')/'raw/ctest-results.xml'
        path.write_text(path.read_text()+'tamper')
        result,_=fixture.run_gate()
        self.assertEqual(fixture.status(result,'A8.02'),'BLOCKED')
        self.assertTrue(result['producer']['errors'])


if __name__=='__main__':unittest.main(verbosity=2)
