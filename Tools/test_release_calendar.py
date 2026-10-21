#!/usr/bin/env python3
"""Adversarial reservation and generator contracts; no remote writes."""
import copy
from datetime import date, datetime, timezone
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import Mock, patch

import check_release_calendar as calendar
import chimera_version


class CalendarContracts(unittest.TestCase):
    def setUp(self):
        self.plan = calendar.read_plan()
        clock = patch.object(calendar, 'kst_today',
                             return_value=date.fromisoformat(self.plan['updated_at']))
        clock.start()
        self.addCleanup(clock.stop)
        self.today_patcher = clock

    def check(self, **kwargs):
        return calendar.validate_plan(self.plan, **kwargs)

    def test_real_plan_and_mirrors(self):
        self.assertEqual(calendar.check_repository()['reserved'], 82)
        self.assertEqual(calendar.check_repository()['deferred'], 0)
        self.assertEqual(calendar.check_repository()['cancelled'], 1)
        self.assertEqual(calendar.check_repository()['published'], 9)
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
        self.plan['chimera_weekly'][2]['publication_target'] = '2026-10-23'
        with self.assertRaisesRegex(ValueError, 'Duplicate product/date'):
            self.check()

    def test_remote_tag_without_release_is_consumed(self):
        with self.assertRaisesRegex(ValueError, 'already consumed'):
            self.check(consumed_tags={'v1.3.1-beta.3'})

    def test_stable_or_early_higher_release_blocks_lower_reserved_beta(self):
        for tag in ('v1.3.2', 'v1.3.2-beta.3', 'v1.3.2-rc.1', 'v1.4.0-beta.1'):
            with self.subTest(tag=tag), self.assertRaisesRegex(ValueError, 'does not advance'):
                self.check(consumed_tags={tag})

    def test_versions_are_numeric_not_lexical(self):
        for lower, upper in [('1.3.1-beta.9', '1.3.1-beta.10'), ('1.9.0', '1.10.0-beta.1'),
                             ('1.3.1-beta.99', '1.3.1-rc.1'), ('1.3.1-rc.99', '1.3.1')]:
            self.assertLess(calendar.version_key(lower), calendar.version_key(upper))

    def test_later_reservation_cannot_downgrade(self):
        row = self.plan['chimera_weekly'][2]
        row.update(version='1.3.2-beta.8', tag='v1.3.2-beta.8')
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
            plan['chimera_weekly'][1][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                calendar.validate_plan(plan)

    def test_no_source_or_candidate_approval_from_calendar(self):
        self.plan['development_candidates'][0]['publication_approved'] = True
        with self.assertRaisesRegex(ValueError, 'not publication approval'):
            self.check()

    def test_candidate_identity_and_aax_reference(self):
        self.plan['development_candidates'][0]['version'] = '1.3.2-beta.3'
        with self.assertRaisesRegex(ValueError, 'candidate reservation mismatch'):
            self.check()
        self.plan = calendar.read_plan()
        self.plan['dependent_deliveries'][2]['version'] = '1.3.2-beta.4'
        with self.assertRaisesRegex(ValueError, 'same-date product reservation'):
            self.check()

    def test_unreserved_or_published_target_fails(self):
        for version in ('1.3.1-beta.2', '1.3.1-beta.3', '1.3.2-beta.1', '1.3.2-beta.99'):
            with self.subTest(version=version), self.assertRaisesRegex(ValueError, 'not a unique unpublished'):
                self.check(target_version=version)
        self.check(target_version='1.3.2-beta.2')

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
        # Simulate a future release, without changing the real beta.1 receipt.
        row = self.plan['chimera_weekly'][1]
        row['status'] = 'published'
        with self.assertRaisesRegex(ValueError, 'lacks history'):
            self.check()
        self.plan['published_releases'].append(dict(product='Chimera', version=row['version'],
            tag=row['tag'], product_source='a'*40))
        next_row = self.plan['chimera_weekly'][2]
        self.plan['next_release'].update(version=next_row['version'],
            publication_target=next_row['publication_target'], development_target=next_row['checkpoint_date'])
        self.check(target_version='1.3.2-beta.3')
        for version in ('1.3.2-beta.1', '1.3.2-beta.2', '1.3.1-beta.3'):
            with self.subTest(version=version), self.assertRaisesRegex(ValueError, 'not a unique unpublished'):
                self.check(target_version=version)

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
                         'docs/RELEASE_CALENDAR_2026_2027.md','docs/DEVELOPMENT_ROADMAP_2026_2027.md'):
                (root/name).parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(calendar.ROOT/name, root/name)
            (root/'VERSION').write_text('1.3.2\n')
            (root/'RELEASE_CHANNEL').write_text('beta.2\n')
            result = chimera_version.validate_reserved_target_version('1.3.2-beta.2', root, 'a'*40)
            self.assertEqual(result['version'], '1.3.2-beta.2')
            with self.assertRaisesRegex(ValueError, 'differs from source'):
                chimera_version.validate_reserved_target_version('1.3.1-beta.3', root, 'a'*40)
            (root/'VERSION').write_text('1.3.1\n')
            (root/'RELEASE_CHANNEL').write_text('beta.3\n')
            with self.assertRaisesRegex(ValueError, 'deferred/cancelled'):
                chimera_version.validate_reserved_target_version('1.3.1-beta.3', root, 'a'*40)
            (root/'RELEASE_CHANNEL').write_text('beta.2\n')
            with self.assertRaisesRegex(ValueError, 'not a unique unpublished'):
                chimera_version.validate_reserved_target_version('1.3.1-beta.2', root, 'a'*40)
            self.assertEqual((root/'RELEASE_CHANNEL').read_text(), 'beta.2\n')

    def test_inactive_dates_and_reason_cannot_be_silently_restored(self):
        for field in calendar.SCHEDULE_FIELDS:
            plan = copy.deepcopy(self.plan)
            plan['chimera_weekly'][0][field] = '2026-10-23'
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, 'no active dates'):
                calendar.validate_plan(plan)
        del self.plan['chimera_weekly'][0]['cancellation_reason']
        with self.assertRaisesRegex(ValueError, 'needs a reason'):
            self.check()

    def test_elapsed_active_target_is_blocked_and_no_automatic_rollover(self):
        before = copy.deepcopy(self.plan)
        self.check(as_of=date(2026, 10, 23), target_version='1.3.2-beta.2')
        with self.assertRaisesRegex(ValueError, 'Missed publication target'):
            self.check(as_of=date(2026, 10, 24), target_version='1.3.2-beta.2')
        self.assertEqual(self.plan, before)

    def test_original_missed_target_cannot_be_reactivated(self):
        row = self.plan['chimera_weekly'][0]
        row.update(row['missed_target'], status='blocked')
        with self.assertRaisesRegex(ValueError, 'does not advance public version'):
            self.check(target_version=row['version'])

    def test_rescheduling_requires_mirrors_and_increasing_version_order(self):
        # A future deferred reservation is hypothetical, never an approved date.
        row = self.plan['chimera_weekly'][1]
        self.plan['development_candidates'].append(dict(pr=999, tag=row['tag'], version=row['version'],
            publication_target=row['publication_target'], reservation_status=row['status'], publication_approved=False))
        row['candidate_pr'] = 999
        row.update(test_build_date='2026-10-22', candidate_date='2026-10-23',
                   publication_target='2026-10-24')
        self.plan['dependent_deliveries'][2]['publication_review'] = '2026-10-24'
        with self.assertRaisesRegex(ValueError, 'candidate reservation mismatch'):
            self.check()
        self.plan['development_candidates'][-1]['publication_target'] = '2026-10-24'
        self.plan['next_release']['publication_target'] = '2026-10-24'
        self.check(target_version=row['version'])
        row.update(test_build_date='2026-10-30', candidate_date='2026-10-31', publication_target='2026-11-01')
        with self.assertRaisesRegex(ValueError, 'Non-increasing'):
            self.check()

    def test_central_roadmap_and_catalog_status_drift(self):
        roadmap = (calendar.ROOT/'docs/DEVELOPMENT_ROADMAP_2026_2027.md').read_text(encoding='utf-8')
        self.check(roadmap=roadmap)
        with self.assertRaisesRegex(ValueError, 'Central roadmap'):
            self.check(roadmap=roadmap.replace('| TBD | cancelled |', '| 2026-10-16 | blocked |'))
        catalog = json.loads((calendar.ROOT/'Validation/catalog-count-contract.json').read_text(encoding='utf-8'))
        catalog['roadmap']['quality_followup']['weekly_versions'][0]['status'] = 'blocked'
        with self.assertRaisesRegex(ValueError, 'Catalog weekly'):
            self.check(catalog=catalog)

    def test_kst_midnight_boundary(self):
        self.today_patcher.stop()
        with patch.object(calendar, 'datetime') as clock:
            clock.now.side_effect = lambda tz: datetime(2026,10,23,14,59,59,tzinfo=timezone.utc).astimezone(tz)
            self.assertEqual(calendar.kst_today(), date(2026,10,23))
            clock.now.side_effect = lambda tz: datetime(2026,10,23,15,0,0,tzinfo=timezone.utc).astimezone(tz)
            self.assertEqual(calendar.kst_today(), date(2026,10,24))

    def test_early_release_receipt_and_cancelled_lower_version(self):
        receipt = self.plan['completed_reservations'][0]
        self.assertEqual(receipt['published_at'], '2026-10-17T16:51:47Z')
        self.assertEqual(receipt['original_target']['publication_target'], '2026-10-23')
        self.assertTrue(all(receipt[k] is None for k in calendar.SCHEDULE_FIELDS))
        self.assertEqual(self.plan['chimera_weekly'][0]['status'], 'cancelled')
        self.assertEqual(self.plan['chimera_weekly'][0]['missed_target']['publication_target'], '2026-10-16')
        self.assertEqual(self.plan['chimera_weekly'][0]['superseded_by'], receipt['tag'])
        # The real public tag is tolerated as history, never as a selectable target.
        self.check(consumed_tags={receipt['tag']}, target_version='1.3.2-beta.2')
        with self.assertRaisesRegex(ValueError, 'not a unique unpublished'):
            self.check(consumed_tags={receipt['tag']}, target_version='1.3.2-beta.1')

    def test_completed_receipt_cannot_regain_active_dates_or_change_source(self):
        for field, value in [('publication_target', '2026-10-23'),
                             ('product_source', 'a'*40), ('published_at', '2026-10-18T00:00:00Z')]:
            plan = copy.deepcopy(self.plan)
            plan['completed_reservations'][0][field] = value
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, 'Completed reservation'):
                calendar.validate_plan(plan)

    def test_completed_public_tag_cannot_be_reinserted_as_weekly_reservation(self):
        self.plan['chimera_weekly'][1].update(version='1.3.2-beta.1', tag='v1.3.2-beta.1')
        with self.assertRaisesRegex(ValueError, 'Completed tag cannot be reserved again'):
            self.check()

    def test_next_release_date_and_expiry_are_not_approval(self):
        next_release = self.plan['next_release']
        self.assertEqual(next_release['requirements_issue'], 97)
        self.assertEqual(next_release['version'], '1.3.2-beta.2')
        self.assertEqual(len(next_release['required_p0']), 4)
        for field, value in [('publication_approved', True), ('expiry_approved', True),
                             ('expiry_utc', '2027-02-11T14:59:00Z')]:
            plan = copy.deepcopy(self.plan)
            plan['next_release'][field] = value
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, 'cannot grant'):
                calendar.validate_plan(plan)
        self.plan['next_release']['version'] = '1.3.2-beta.1'
        with self.assertRaisesRegex(ValueError, 'first active reservation'):
            self.check()

    def test_next_tag_consumed_while_staging_still_blocks(self):
        # Existing publisher reuses this same live check before prepare and publish.
        with self.assertRaisesRegex(ValueError, 'already consumed'):
            self.check(consumed_tags={'v1.3.2-beta.2'}, target_version='1.3.2-beta.2')

    def test_cancelled_lower_version_cannot_be_reactivated_on_future_date(self):
        row = self.plan['chimera_weekly'][0]
        row.update(status='blocked', test_build_date='2026-10-18',
                   candidate_date='2026-10-19', publication_target='2026-10-20')
        with self.assertRaisesRegex(ValueError, 'does not advance public version'):
            self.check(target_version='1.3.1-beta.3')

    def test_historical_deferred_reservation_requires_cancellation_after_higher_release(self):
        # Preserve the pre-publication contract using an explicit synthetic history.
        self.plan['published_releases'] = [r for r in self.plan['published_releases']
                                          if r['tag'] != 'v1.3.2-beta.1']
        self.plan.pop('completed_reservations')
        prior = self.plan['published_releases'][0]
        self.plan['source_baseline'].update(release=prior['tag'], product_source=prior['product_source'])
        self.plan['chimera_weekly'][0]['status'] = 'deferred'
        self.plan['development_candidates'][0]['reservation_status'] = 'deferred'
        self.check()
        with self.assertRaisesRegex(ValueError, 'not a unique unpublished'):
            self.check(target_version='1.3.1-beta.3')
        with self.assertRaisesRegex(ValueError, 'superseded; cancel'):
            self.check(consumed_tags={'v1.3.2-beta.1'})


if __name__ == '__main__':
    unittest.main()
