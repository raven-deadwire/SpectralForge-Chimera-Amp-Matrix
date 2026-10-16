#!/usr/bin/env python3
"""Fail closed on consumed, duplicate, regressing or inconsistent reservations.

Offline mode uses the recorded release snapshot. --live additionally reads all
remote tags/public releases; neither mode authorizes or performs publication.
"""
from __future__ import annotations
import argparse
from collections import defaultdict
from datetime import date, datetime, timedelta, timezone
import json
from pathlib import Path
import re
import sys

import chimera_version

ROOT = Path(__file__).resolve().parents[1]
PLAN = Path('Validation/release-calendar.json')
PREFIXES = {'Chimera': 'v', 'Náströnd': 'nastrond-v', 'Niflheimr': 'niflheimr-v',
            'Náströnd NAM': 'nastrond-nam-v', 'Niflheimr NAM': 'niflheimr-nam-v'}
PLATFORMS = {'windows_x64', 'macos_universal', 'linux_x86_64'}
SCHEDULE_FIELDS = ('test_build_date', 'candidate_date', 'publication_target')
MIRROR_FIELDS = ('version', 'checkpoint_date', 'publication_target', 'status')
ROADMAP_START = '<!-- release-reservation-status:start -->'
ROADMAP_END = '<!-- release-reservation-status:end -->'


def kst_today():
    return datetime.now(timezone(timedelta(hours=9))).date()


def weekly_mirror(plan):
    return [{k: r[k] for k in MIRROR_FIELDS} for r in plan['chimera_weekly'][:3]]


def roadmap_status_table(plan):
    lines = [ROADMAP_START, '| Version | Original checkpoint (KST) | Active target (KST) | Reservation status |',
             '|---|---|---|---|']
    for row in weekly_mirror(plan):
        lines.append(f"| `{row['version']}` | {row['checkpoint_date']} | {row['publication_target'] or 'TBD'} | {row['status']} |")
    return '\n'.join(lines + [ROADMAP_END])


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_plan(root=ROOT):
    # Match the publisher's strict duplicate-key/non-finite JSON handling.
    from publish_beta131 import read_json
    return read_json(root / PLAN)


def version_key(value):
    base, channel = chimera_version.parse_release_version(value)
    stage, number = (2, 0) if channel == 'stable' else (0 if channel.startswith('beta.') else 1, int(channel.split('.')[1]))
    return (*map(int, base.split('.')), stage, number)


def identify_tag(tag):
    for product, prefix in PREFIXES.items():
        if isinstance(tag, str) and tag.startswith(prefix):
            try:
                return product, version_key(tag[len(prefix):])
            except ValueError:
                return None  # Other development/preview tag namespaces are not releases.
    return None


def reservation_rows(plan):
    return (plan['chimera_weekly'] + plan['focused_releases'] +
            [r for r in plan['dependent_deliveries'] if r['kind'] != 'conditional_format'])


def live_consumed_tags(api):
    tags = api.pages('/git/refs/tags')
    releases = api.pages('/releases')
    require(isinstance(tags, list) and isinstance(releases, list), 'Malformed remote inventory')
    consumed = set()
    for item in tags:
        require(isinstance(item, dict) and isinstance(item.get('ref'), str)
                and item['ref'].startswith('refs/tags/'), 'Malformed remote tag')
        consumed.add(item['ref'][10:])
    for item in releases:
        require(isinstance(item, dict) and isinstance(item.get('tag_name'), str)
                and type(item.get('draft')) is bool, 'Malformed remote release')
        if not item['draft']:
            consumed.add(item['tag_name'])
    return consumed


def validate_plan(plan, *, consumed_tags=(), target_version=None, markdown=None, catalog=None,
                  roadmap=None, as_of=None):
    require(plan.get('schema') == 'spectralforge.release-plan' and type(plan.get('schema_version')) is int
            and plan['schema_version'] == 3,
            'Unsupported release calendar schema')
    snapshot_day = date.fromisoformat(plan['updated_at'])
    as_of = snapshot_day if as_of is None else as_of
    require(type(as_of) is date and as_of >= snapshot_day, 'Invalid calendar review date')
    require(plan.get('status') == 'planning_only_not_release_or_updater_manifest', 'Calendar must remain planning-only')
    policy = plan['policy']
    for flag in ('all_dates_are_targets', 'published_tags_immutable', 'empty_update_prohibited', 'calendar_is_not_approval',
                 'missed_targets_not_publishable', 'deferred_requires_reservation_review',
                 'superseded_unpublished_cancel_required'):
        require(policy.get(flag) is True, f'Unsafe calendar policy: {flag}')
    require(set(policy.get('publication_requirements', [])) >= {
        'substantive_implemented_improvement', 'exact_source_windows_macos_linux_validation',
        'install_update_repair_uninstall_and_user_data_preservation', 'applicable_host_package_acceptance',
        'version_source_specific_publication_approval', 'approved_expiry_and_fresh_binary_if_lifecycle_enabled'},
        'Publication requirements were weakened')
    history = plan['published_releases']
    require(isinstance(history, list) and history, 'Published history is required')
    recorded = set()
    for row in history:
        require(row['tag'] == PREFIXES[row['product']] + row['version'], 'Published tag identity mismatch')
        version_key(row['version'])
        require(row['tag'] not in recorded and re.fullmatch('[0-9a-f]{40}', row.get('product_source', '')),
                'Duplicate or invalid published history')
        recorded.add(row['tag'])
    baseline = plan['source_baseline']
    require(any(r['tag'] == baseline['release'] and r['product_source'] == baseline['product_source'] for r in history),
            'Baseline is not recorded public source')
    consumed = recorded | set(consumed_tags)
    latest = {}
    for tag in consumed:
        item = identify_tag(tag)
        if item:
            product, key = item
            latest[product] = max(latest.get(product, key), key)
    rows = reservation_rows(plan)
    identities, tags, dates, per_product = set(), set(), set(), defaultdict(list)
    pending, deferred, cancelled = [], [], []
    for row in rows:
        product, version, tag = row['product'], row['version'], row['tag']
        key = version_key(version)
        require(tag == PREFIXES[product] + version, f'Tag/version mismatch: {tag}')
        require((product, version) not in identities and tag not in tags, f'Duplicate reservation: {tag}')
        identities.add((product, version)); tags.add(tag)
        require(row.get('conditional') is True, f'Unconditional release target: {tag}')
        status = row.get('status')
        require(status in ('planned', 'blocked', 'deferred', 'published', 'cancelled'), f'Invalid reservation status: {tag}')
        if status in ('deferred', 'cancelled'):
            require(all(field in row and row[field] is None for field in SCHEDULE_FIELDS),
                    f'Inactive reservation must have no active dates: {tag}')
            require(row.get('release_approved') is False, f'Inactive reservation cannot approve publication: {tag}')
            reason = row.get('defer_reason' if status == 'deferred' else 'cancellation_reason')
            require(isinstance(reason, str) and reason.strip(), f'Inactive reservation needs a reason: {tag}')
            require(tag not in consumed, f'Inactive reserved tag already consumed; reconcile history: {tag}')
            if status == 'deferred':
                require(product not in latest or key > latest[product],
                        f'Deferred reservation superseded; cancel before proceeding: {tag}')
                require('missed_target' in row, f'Deferred reservation lacks missed target: {tag}')
                deferred.append(row)
            else:
                cancelled.append(row)
        else:
            day = date.fromisoformat(row['publication_target'])
            require((product, day) not in dates, f'Duplicate product/date: {product} {day}')
            dates.add((product, day))
            per_product[product].append((day, key, tag))
            for field, offset in (('test_build_date', 2), ('candidate_date', 1)):
                require(date.fromisoformat(row[field]) == day - timedelta(days=offset), f'Invalid {field}: {tag}')
        if 'missed_target' in row:
            missed = row['missed_target']
            missed_day = date.fromisoformat(missed['publication_target'])
            require(missed_day < snapshot_day, f'Missed target has not elapsed: {tag}')
            for field, offset in (('test_build_date', 2), ('candidate_date', 1)):
                require(date.fromisoformat(missed[field]) == missed_day - timedelta(days=offset),
                        f'Invalid missed {field}: {tag}')
        if status == 'published':
            require(tag in recorded, f'Published row lacks history: {tag}')
        elif status in ('planned', 'blocked'):
            require(tag not in consumed, f'Reserved tag already consumed: {tag}')
            require(product not in latest or key > latest[product], f'Reservation does not advance public version: {tag}')
            require(day >= as_of, f'Missed publication target requires defer/remap: {tag} ({day})')
            pending.append(row)
        if product == 'Chimera':
            require(set(row.get('required_platforms', [])) == PLATFORMS, f'Three-OS requirement missing: {tag}')
            require(row.get('publication_requires') == 'applicable_source_platform_host_package_acceptance',
                    f'Publication acceptance requirement missing: {tag}')
        if markdown is not None:
            prefix = '| ' + ' | '.join(row[field] or 'TBD' for field in SCHEDULE_FIELDS) + f' | `{version}` |'
            if product == 'Chimera':
                require(markdown.count(prefix) == 1 and prefix + ' ' + row['scope'] + ' |' in markdown,
                        f'Markdown/JSON row mismatch: {tag}')
    for product, values in per_product.items():
        ordered = sorted(values)
        require(all(a[1] < b[1] for a, b in zip(ordered, ordered[1:])), f'Non-increasing reserved versions: {product}')
    # Checkpoints retain the original weekly work history; active dates may be
    # cleared or explicitly remapped without inventing a new delivery date.
    weekly = [date.fromisoformat(r['checkpoint_date']) for r in plan['chimera_weekly']]
    require(len(weekly) == 64 and all(day.weekday() == 4 for day in weekly)
            and all(b - a == timedelta(days=7) for a, b in zip(weekly, weekly[1:])), 'Expected 64 consecutive Friday checkpoints')
    for row in plan['dependent_deliveries']:
        if row['kind'] == 'conditional_format' and 'version' in row:
            require(any(r['product'] == row.get('shares_release_with') and r['version'] == row['version']
                        and r['tag'] == row['tag'] and r['publication_target'] is not None
                        and r['publication_target'] == row['publication_review'] for r in rows),
                    'Format delivery must reference its same-date product reservation')
    for candidate in plan['development_candidates']:
        require(candidate.get('publication_approved') is False, 'Candidate record is not publication approval')
        require(any(r['tag'] == candidate['tag'] and r['version'] == candidate['version']
                    and r['publication_target'] == candidate['publication_target']
                    and r['status'] == candidate['reservation_status']
                    and r.get('candidate_pr') == candidate['pr'] for r in rows), 'Development candidate reservation mismatch')
    if catalog is not None:
        catalog_roadmap = catalog['roadmap']
        expected = weekly_mirror(plan)
        require(catalog_roadmap['quality_followup']['weekly_versions'] == expected, 'Catalog weekly mirror differs')
        require('v'+catalog_roadmap['delivered_baseline']['version'] == baseline['release'], 'Catalog baseline differs')
        aax = catalog_roadmap['aax_native']['first_publication_review']
        require(any(r.get('version') == aax['candidate_version'] and r.get('publication_review') == aax['target']
                    for r in plan['dependent_deliveries']), 'Catalog AAX mirror differs')
    if roadmap is not None:
        require(roadmap.count(ROADMAP_START) == 1 and roadmap.count(ROADMAP_END) == 1
                and roadmap_status_table(plan) in roadmap, 'Central roadmap reservation mirror differs')
    if target_version is not None:
        chimera_version.parse_release_version(target_version)
        require(sum(r['product'] == 'Chimera' and r['version'] == target_version for r in pending) == 1,
                f'Target is not a unique unpublished active reservation (deferred/cancelled targets are blocked): {target_version}')
    return {'reserved': len(pending), 'deferred': len(deferred), 'cancelled': len(cancelled),
            'published': len(history), 'publication_approved': False, 'as_of_kst': as_of.isoformat()}


def check_repository(root=ROOT, *, api=None, target_version=None, as_of=None):
    plan = read_plan(root)
    # Live inventory and all target checks use today's KST date. Offline plan
    # audits use the recorded review date so historical CI remains reproducible.
    if as_of is None and (api is not None or target_version is not None):
        as_of = kst_today()
    return validate_plan(plan, consumed_tags=live_consumed_tags(api) if api is not None else (),
                         target_version=target_version, as_of=as_of,
                         markdown=(root/'docs/RELEASE_CALENDAR_2026_2027.md').read_text(encoding='utf-8'),
                         catalog=json.loads((root/'Validation/catalog-count-contract.json').read_text(encoding='utf-8')),
                         roadmap=(root/'docs/DEVELOPMENT_ROADMAP_2026_2027.md').read_text(encoding='utf-8'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--live', action='store_true', help='Read all GitHub tags/releases; fail on lookup errors')
    parser.add_argument('--target-version')
    args = parser.parse_args()
    if args.live:
        from publish_open_beta import GitHub
        api = GitHub()
    else:
        api = None
    result = check_repository(api=api, target_version=args.target_version)
    print(json.dumps({**result, 'remote_inventory': 'checked' if args.live else 'not_checked'}))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, RuntimeError, KeyError, TypeError, OSError) as exc:
        sys.exit(f'Release calendar blocked: {exc}')
