#!/usr/bin/env python3
"""Adversarial reservation and generator contracts; no remote writes."""
import copy
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import Mock

import check_release_calendar as calendar
import chimera_version


class CalendarContracts(unittest.TestCase):
    def setUp(self):
        self.plan = calendar.read_plan()

    def check(self, **kwargs):
        return calendar.validate_plan(self.plan, **kwargs)

    def test_real_plan_and_mirrors(self):
        self.assertEqual(calendar.check_repository()['reserved'], 83)
        self.assertFalse(calendar.check_repository()['publication_approved'])

    def test_consumed_beta2_is_rejected_even_with_no_remote_data(self):
        row = self.plan['chimera_weekly'][0]
        row.update(version='1.3.1-beta.2', tag='v1.3.1-beta.2')
        with self.assertRaisesRegex(ValueError, 'already consumed'):
            self.check()

    def test_duplicate_version_and_tag(self):
        self.plan['chimera_weekly'][1].update(version='1.3.1-beta.3', tag='v1.3.1-beta.3')
        with self.assertRaisesRegex(ValueError, 'Duplicate reservation'):
            self.check()

    def test_duplicate_product_date(self):
        self.plan['chimera_weekly'][1]['publication_target'] = '2026-10-16'
        with self.assertRaisesRegex(ValueError, 'Duplicate product/date'):
            self.check()

    def test_remote_tag_without_release_is_consumed(self):
        with self.assertRaisesRegex(ValueError, 'already consumed'):
            self.check(consumed_tags={'v1.3.1-beta.3'})

    def test_stable_or_early_higher_release_blocks_lower_reserved_beta(self):
        for tag in ('v1.3.1', 'v1.3.2-beta.1', 'v1.3.1-rc.1', 'v1.4.0-beta.1'):
            with self.subTest(tag=tag), self.assertRaisesRegex(ValueError, 'does not advance'):
                self.check(consumed_tags={tag})

    def test_versions_are_numeric_not_lexical(self):
        for lower, upper in [('1.3.1-beta.9', '1.3.1-beta.10'), ('1.9.0', '1.10.0-beta.1'),
                             ('1.3.1-beta.99', '1.3.1-rc.1'), ('1.3.1-rc.99', '1.3.1')]:
            self.assertLess(calendar.version_key(lower), calendar.version_key(upper))

    def test_later_reservation_cannot_downgrade(self):
        row = self.plan['chimera_weekly'][2]
        row.update(version='1.3.1-beta.4', tag='v1.3.1-beta.4')
        with self.assertRaisesRegex(ValueError, 'Non-increasing'):
            self.check()

    def test_wrong_tag_and_noncanonical_version(self):
        for version, tag in [('1.3.1-beta.3', 'v1.3.1-beta.4'), ('1.3.1-beta.03', 'v1.3.1-beta.03')]:
            self.plan['chimera_weekly'][0].update(version=version, tag=tag)
            with self.assertRaises(ValueError):
                self.check()

    def test_date_and_platform_gates(self):
        for field, value in [('candidate_date', '2026-10-14'), ('test_build_date', '2026-10-15'),
                             ('required_platforms', ['windows_x64']), ('conditional', False),
                             ('publication_requires', 'date_only')]:
            plan = copy.deepcopy(self.plan)
            plan['chimera_weekly'][0][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                calendar.validate_plan(plan)

    def test_no_source_or_candidate_approval_from_calendar(self):
        self.plan['development_candidates'][0]['publication_approved'] = True
        with self.assertRaisesRegex(ValueError, 'not publication approval'):
            self.check()

    def test_candidate_identity_and_aax_reference(self):
        self.plan['development_candidates'][1]['version'] = '1.3.2-beta.3'
        with self.assertRaisesRegex(ValueError, 'candidate reservation mismatch'):
            self.check()
        self.plan = calendar.read_plan()
        self.plan['dependent_deliveries'][2]['version'] = '1.3.2-beta.4'
        with self.assertRaisesRegex(ValueError, 'same-date product reservation'):
            self.check()

    def test_unreserved_or_published_target_fails(self):
        for version in ('1.3.1-beta.2', '1.3.2-beta.99'):
            with self.subTest(version=version), self.assertRaisesRegex(ValueError, 'not a unique unpublished'):
                self.check(target_version=version)
        self.check(target_version='1.3.1-beta.3')
        self.check(target_version='1.3.2-beta.1')

    def test_markdown_drift(self):
        text = (calendar.ROOT/'docs/RELEASE_CALENDAR_2026_2027.md').read_text(encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'Markdown/JSON'):
            self.check(markdown=text.replace('`1.3.1-beta.3`', '`1.3.1-beta.2`'))

    def test_catalog_drift(self):
        value = json.loads((calendar.ROOT/'Validation/catalog-count-contract.json').read_text(encoding='utf-8'))
        value['roadmap']['quality_followup']['weekly_versions'][0]['version'] = '1.3.1-beta.2'
        with self.assertRaisesRegex(ValueError, 'Catalog weekly'):
            self.check(catalog=value)

    def test_product_namespaces_do_not_collide(self):
        self.check(consumed_tags={'other-v1.3.1-beta.3', 'nastrond-v0.9.0'})
        with self.assertRaisesRegex(ValueError, 'already consumed'):
            self.check(consumed_tags={'nastrond-v1.0.0-beta.1'})

    def test_record_publication_then_next_reservation(self):
        row = self.plan['chimera_weekly'][0]
        row['status'] = 'published'
        with self.assertRaisesRegex(ValueError, 'lacks history'):
            self.check()
        self.plan['published_releases'].append(dict(product='Chimera',version=row['version'],tag=row['tag'],product_source='a'*40))
        self.check(target_version='1.3.2-beta.1')
        with self.assertRaisesRegex(ValueError, 'not a unique unpublished'):
            self.check(target_version='1.3.1-beta.3')

    def test_live_inventory_includes_tag_only_and_public_release_only(self):
        api = Mock()
        api.pages.side_effect = [[{'ref':'refs/tags/v1.3.1-beta.3'}],
                                [{'tag_name':'v1.3.2-beta.1','draft':False}, {'tag_name':'v1.3.2-beta.2','draft':True}]]
        self.assertEqual(calendar.live_consumed_tags(api), {'v1.3.1-beta.3','v1.3.2-beta.1'})
        self.assertEqual([c.args[0] for c in api.pages.call_args_list], ['/git/refs/tags','/releases'])

    def test_live_errors_are_not_empty_inventory(self):
        api = Mock()
        for failure in (OSError('network'), RuntimeError('unauthorized')):
            api.pages.side_effect = failure
            with self.assertRaises(type(failure)):
                calendar.live_consumed_tags(api)
        api.pages.side_effect = [[{'ref':'bad'}], []]
        with self.assertRaisesRegex(ValueError, 'Malformed'):
            calendar.live_consumed_tags(api)

    def test_version_generator_asserts_source_and_reserved_target(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for name in ('Validation/release-calendar.json','Validation/catalog-count-contract.json',
                         'docs/RELEASE_CALENDAR_2026_2027.md'):
                (root/name).parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(calendar.ROOT/name, root/name)
            (root/'VERSION').write_text('1.3.1\n')
            (root/'RELEASE_CHANNEL').write_text('beta.3\n')
            result = chimera_version.validate_reserved_target_version('1.3.1-beta.3', root, 'a'*40)
            self.assertEqual(result['version'], '1.3.1-beta.3')
            with self.assertRaisesRegex(ValueError, 'differs from source'):
                chimera_version.validate_reserved_target_version('1.3.2-beta.1', root, 'a'*40)
            (root/'RELEASE_CHANNEL').write_text('beta.2\n')
            with self.assertRaisesRegex(ValueError, 'not a unique unpublished'):
                chimera_version.validate_reserved_target_version('1.3.1-beta.2', root, 'a'*40)
            self.assertEqual((root/'RELEASE_CHANNEL').read_text(), 'beta.2\n')


if __name__ == '__main__':
    unittest.main()
