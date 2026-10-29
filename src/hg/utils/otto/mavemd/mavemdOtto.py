#!/usr/bin/env python3
"""mavemdOtto.py -- keep the hg38 MaveMD track in sync with the MaveDB API.

Weekly cron. Each run updates the cache from the API (mavemdFetch.py: only score sets
whose fingerprint changed are downloaded), and when what the build would read differs
from the last release, builds a new release (mavemdBuild.py) into releases/YYYY-MM-DD/,
checks it, and moves the /gbdb/hg38/mavemd/ symlinks to it.

What makes a release is a key over three things a build depends on: the fingerprint of
every cached score set, the hg38 annotation tables the placement reads (ncbiRefSeq and
the newest GENCODE), and nothing else. A quarterly refetch and rebuild covers the one
case the key cannot see, MaveDB remapping a score set without touching its record.

Silent when nothing changed. A release prints a short summary, which cron mails; anything
unexpected raises and exits non-zero. A release that fails a check is left in
releases/YYYY-MM-DD.tmp/ and the /gbdb links are not touched; once a person has looked at
it, --release YYYY-MM-DD --ignoreChecks makes exactly that build the release, without
fetching or building again.

The track is gated to alpha (include mavemd.ra alpha), so for now this feeds dev only.
Redmine #37800.

Development runs use --outDir, which also puts the /gbdb links under outDir/gbdb unless
--gbdbDir says otherwise.
"""

import argparse
import datetime
import fcntl
import hashlib
import json
import os
import shutil
import subprocess
import sys
import traceback

import mavemdBuild
import mavemdFetch

# Where the cron runs. Not derived from __file__, so that the tree copy cannot write to
# the live directory by accident.
OTTO_DIR = '/hive/data/outside/otto/mavemd'
GBDB_DIR = '/gbdb/hg38/mavemd'
HERE = os.path.dirname(os.path.abspath(__file__))

LINKED = mavemdBuild.BIGBEDS + ('version.txt',)

# A new release may lose at most this share of the previous release's items without a
# person looking first.
MAX_ITEM_DROP = 0.10
MAX_SCORE_SET_DROP = 0.05
# MaveDB's own mapping normally places most of the collection. A large fall means they
# have withdrawn mappings rather than fixed them, which the cross-check cannot see: it
# only looks at rows that carry both coordinate systems, so fewer mapped rows means fewer
# checks, not more failures. Their staging API showed exactly this in October 2026.
MIN_MAVEDB_SHARE = 0.60

FULL_BUILD_MONTHS = (1, 4, 7, 10)
FULL_BUILD_DAYS = 90
LIST_LIMIT = 20
KEEP_RELEASES = 3

# hg38 tables the placement reads. A RefSeq push or a new GENCODE moves variants, and no
# fingerprint of MaveDB's data can see it, so their update times go in the release key.
ANNOTATION_TABLES = ('ncbiRefSeqCurated', 'ncbiRefSeqLink', 'ncbiRefSeqHistorical')


class OttoError(Exception):
    pass


def readText(path):
    if not os.path.isfile(path):
        return None
    with open(path) as fh:
        return fh.read().strip()


def writeText(path, text):
    with open(path + '.tmp', 'w') as fh:
        fh.write(text + '\n')
    os.rename(path + '.tmp', path)


def readDate(path):
    text = readText(path)
    return datetime.date.fromisoformat(text) if text else None


def readLastRelease(ottoDir):
    """(release key, release directory name) of the last release, or (None, None)."""
    text = readText(os.path.join(ottoDir, 'lastRelease.txt'))
    if not text:
        return None, None
    key, name = text.split('\t')
    return key, name


def annotationStamp(db='hg38', log=sys.stderr):
    """'table=updateTime' for each annotation table, plus the newest GENCODE table name.

    A failure here must not stop the run: the quarterly rebuild still covers it, so a
    MySQL hiccup should cost us a late rebuild, not a missed week.
    """
    parts = []
    try:
        for table in ANNOTATION_TABLES:
            out = subprocess.run(['hgsql', db, '-Ne', "show table status like '%s'" % table],
                                 check=True, stdout=subprocess.PIPE,
                                 stderr=subprocess.DEVNULL,
                                 universal_newlines=True).stdout.strip()
            if out:
                cols = out.split('\t')
                parts.append('%s=%s' % (table, cols[12] if len(cols) > 12 else ''))
        out = subprocess.run(['hgsql', db, '-Ne', "show tables like 'wgEncodeGencodeAttrsV%'"],
                             check=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                             universal_newlines=True).stdout.split()
        if out:
            newest = sorted(out, key=lambda t: int(''.join(c for c in t if c.isdigit())))[-1]
            parts.append('gencode=' + newest)
    except (subprocess.CalledProcessError, OSError, ValueError) as e:
        log.write('could not read the annotation tables (%s); '
                  'the release key leaves them out this run\n' % e)
        return 'unavailable'
    return ' '.join(parts)


def releaseKey(fetched, current, cacheDir, stamp):
    """Hash of what a build would read: the fingerprint each cached score set was fetched
    at, and the hg38 annotation tables.

    Only score sets the collection names now are counted, and only those whose variants
    are actually on disk, so a score set that failed to download keeps its old row and a
    failure alone does not make a release."""
    variants = os.path.join(cacheDir, 'variants')
    lines = sorted('\t'.join(fetched[urn]) for urn in current
                   if urn in fetched
                   and os.path.exists(os.path.join(variants,
                                                   mavemdFetch.fileStem(urn) + '.csv')))
    lines.append('annotations\t' + stamp)
    return hashlib.sha1('\n'.join(lines).encode()).hexdigest()


def fullBuildDue(today, lastFull):
    """True on the first run of a quarter month, or when the last full build is too old."""
    if lastFull is None:
        return True
    if (today - lastFull).days >= FULL_BUILD_DAYS:
        return True
    return today.month in FULL_BUILD_MONTHS and (lastFull.year, lastFull.month) != \
        (today.year, today.month)


def releaseName(releasesDir, today):
    """YYYY-MM-DD, or YYYY-MM-DD.N when a release of that day already exists."""
    name, n = today.isoformat(), 1
    while os.path.exists(os.path.join(releasesDir, name)):
        n += 1
        name = '%s.%d' % (today.isoformat(), n)
    return name


def releaseSortKey(name):
    """2026-11-02 < 2026-11-02.2 < 2026-11-02.10 < 2026-12-01."""
    day, _, n = name.partition('.')
    return day, int(n) if n.isdigit() else 1


def bigBedItemCount(path):
    """itemCount from bigBedInfo, which also fails on a corrupt file."""
    out = subprocess.run(['bigBedInfo', path], check=True, stdout=subprocess.PIPE,
                         universal_newlines=True).stdout
    for line in out.splitlines():
        if line.startswith('itemCount:'):
            return int(line.split(':', 1)[1].strip().replace(',', ''))
    raise OttoError('no itemCount in bigBedInfo output for %s' % path)


def checkRelease(stats, prevStats, itemCounts):
    """Problems that stop a release (empty list = release). itemCounts: bigBed -> count."""
    problems = []
    for bb, n in sorted(itemCounts.items()):
        if n == 0:
            problems.append('%s has no items' % bb)
    if stats.get('overlaps'):
        problems.append('%d heatmaps had overlapping blocks and were dropped'
                        % stats['overlaps'])
    if stats.get('accessionsDropped'):
        problems.append('%d protein accessions disagreed with the genome and were dropped'
                        % stats['accessionsDropped'])
    placed = stats.get('variantItems') or 0
    byMavedb = stats.get('placedByMavedb') or 0
    if placed and byMavedb / placed < MIN_MAVEDB_SHARE:
        problems.append("only %.0f%% of placed items used MaveDB's own mapping, under the "
                        '%.0f%% expected: they may have withdrawn mappings'
                        % (100.0 * byMavedb / placed, 100 * MIN_MAVEDB_SHARE))
    if prevStats:
        for key, label, limit in (('variantItems', 'variant items', MAX_ITEM_DROP),
                                  ('cells', 'heatmap cells', MAX_ITEM_DROP),
                                  ('mapItems', 'heatmaps', MAX_SCORE_SET_DROP)):
            old, new = prevStats.get(key) or 0, stats.get(key) or 0
            if old and (old - new) / old > limit:
                problems.append('%s went from %d to %d (%.1f%% fewer, more than the %.0f%% '
                                'allowed)' % (label, old, new, 100.0 * (old - new) / old,
                                              100 * limit))
    return problems


def catalogueChanges(prev, cur):
    """(added, changed, removed) URN lists between two {urn: fingerprint} maps."""
    added = sorted(set(cur) - set(prev))
    removed = sorted(set(prev) - set(cur))
    changed = sorted(u for u in set(cur) & set(prev) if cur[u] != prev[u])
    return added, changed, removed


def relink(linkPath, target):
    """Point linkPath at target without a moment where the link is missing."""
    tmp = linkPath + '.otto.tmp'
    if os.path.lexists(tmp):
        os.remove(tmp)
    os.symlink(target, tmp)
    os.rename(tmp, linkPath)


def urnList(label, urns):
    if not urns:
        return []
    shown = ' '.join(urns[:LIST_LIMIT])
    more = len(urns) - LIST_LIMIT
    return ['  %s: %s%s' % (label, shown, ' and %d more' % more if more > 0 else '')]


def fmtChange(old, new):
    return '{:,}'.format(new) if old is None else '{:,} -> {:,}'.format(old, new)


def dryRun(ottoDir, args):
    """Say what a run would do; reads the collection and the records, writes nothing."""
    today = datetime.date.today()
    fullDue = fullBuildDue(today, readDate(os.path.join(ottoDir, 'lastFullBuild.txt')))
    cacheDir = os.path.join(ottoDir, 'cache')
    summary = mavemdFetch.fetch(cacheDir, collection=args.collection, catalogueOnly=True,
                                log=sys.stderr)
    current = mavemdFetch.readFingerprints(os.path.join(cacheDir, 'catalogue.tsv'))
    fetched = mavemdFetch.readFingerprints(os.path.join(cacheDir, 'fetched.tsv'))
    toFetch = [u for u in current if fetched.get(u) != current[u]]
    _, lastName = readLastRelease(ottoDir)
    refetch = args.refetch or fullDue
    print('mavemd otto dry run in %s' % ottoDir)
    print('MaveDB API %s; collection %s: %d score sets; %d would be downloaded%s'
          % (summary['apiVersion'], summary['collection'], len(current),
             len(current) if refetch else len(toFetch),
             ' (full download)' if refetch else ''))
    print('last release: %s; annotations: %s' % (lastName or 'none', annotationStamp()))
    for line in summary['superseded']:
        print('  superseded: ' + line)
    if refetch or args.force or toFetch:
        print('a new release would be built%s'
              % (' (scheduled full rebuild)' if fullDue else ''))
    else:
        print('nothing would be done (the release key still has to be compared)')


def run(ottoDir, gbdbDir, args, log):
    if args.release:
        finishRelease(ottoDir, gbdbDir, args.release, args.ignoreChecks, log)
        return
    today = datetime.date.today()
    cacheDir = os.path.join(ottoDir, 'cache')
    releasesDir = os.path.join(ottoDir, 'releases')
    lastFullPath = os.path.join(ottoDir, 'lastFullBuild.txt')
    fullDue = fullBuildDue(today, readDate(lastFullPath))
    os.makedirs(releasesDir, exist_ok=True)
    refetch = args.refetch or fullDue

    summary = mavemdFetch.fetch(cacheDir, refetch=refetch, collection=args.collection,
                                log=log)
    fetched = mavemdFetch.readFingerprints(os.path.join(cacheDir, 'fetched.tsv'))
    current = mavemdFetch.readFingerprints(os.path.join(cacheDir, 'catalogue.tsv'))
    stamp = annotationStamp(log=log)
    key = releaseKey(fetched, current, cacheDir, stamp)
    lastKey, _ = readLastRelease(ottoDir)
    writeText(os.path.join(ottoDir, 'lastRun.txt'), today.isoformat())
    if key == lastKey and not (args.force or refetch):
        return   # nothing changed: no output, so cron sends no mail

    name = releaseName(releasesDir, today)
    tmpDir = os.path.join(releasesDir, name + '.tmp')
    if os.path.exists(tmpDir):
        shutil.rmtree(tmpDir)   # a failed attempt of the same release
    os.makedirs(tmpDir)
    log.write('building release %s (%s)\n' % (name, 'full rebuild' if refetch else 'update'))
    log.flush()
    stats = mavemdBuild.build(cacheDir, tmpDir, log=log)

    # Everything finishRelease() needs goes into the release itself, so a release that
    # fails a check can be made later from what a person inspected (--release), whatever
    # the cache has become since.
    shutil.copy(os.path.join(cacheDir, 'catalogue.tsv'), tmpDir)
    shutil.copy(os.path.join(cacheDir, 'apiVersion.txt'), tmpDir)
    with open(os.path.join(tmpDir, 'stats.json'), 'w') as fh:
        json.dump(stats, fh, indent=1, sort_keys=True)
    with open(os.path.join(tmpDir, 'pending.json'), 'w') as fh:
        json.dump({'key': key, 'date': today.isoformat(), 'refetch': refetch,
                   'fullDue': fullDue, 'annotations': stamp,
                   'apiVersion': summary['apiVersion'],
                   'scoreSets': summary['scoreSets'], 'failed': summary['failed'],
                   'failedNew': summary['failedNew'],
                   'superseded': summary['superseded'],
                   'dropped': summary.get('dropped') or []}, fh, indent=1)
    log.flush()
    shutil.copy(log.name, os.path.join(tmpDir, 'build.log'))
    finishRelease(ottoDir, gbdbDir, name, args.ignoreChecks, log)


def finishRelease(ottoDir, gbdbDir, name, ignoreChecks, log):
    """Check releases/<name>.tmp, and if it passes (or ignoreChecks) make it the release:
    rename it, move the /gbdb links, record it, remove old builds, print the summary."""
    releasesDir = os.path.join(ottoDir, 'releases')
    name = name[:-4] if name.endswith('.tmp') else name
    tmpDir = os.path.join(releasesDir, name + '.tmp')
    if not os.path.exists(os.path.join(tmpDir, 'pending.json')):
        raise OttoError('%s is not a built release waiting to be made' % tmpDir)
    with open(os.path.join(tmpDir, 'pending.json')) as fh:
        pend = json.load(fh)
    with open(os.path.join(tmpDir, 'stats.json')) as fh:
        stats = json.load(fh)
    itemCounts = {bb: bigBedItemCount(os.path.join(tmpDir, bb))
                  for bb in mavemdBuild.BIGBEDS}
    _, lastName = readLastRelease(ottoDir)
    prevDir = os.path.join(releasesDir, lastName) if lastName else None
    prevStats = None
    if prevDir and os.path.exists(os.path.join(prevDir, 'stats.json')):
        with open(os.path.join(prevDir, 'stats.json')) as fh:
            prevStats = json.load(fh)
    problems = checkRelease(stats, prevStats, itemCounts)
    if problems and not ignoreChecks:
        raise OttoError('release %s not made, the /gbdb links are unchanged:\n  %s\n'
                        'The build is in %s. Once it has been checked, make it the '
                        'release with: mavemdOtto.py --release %s --ignoreChecks'
                        % (name, '\n  '.join(problems), tmpDir, name))

    # The .bed inputs and the converter work files are not needed once the bigBeds are
    # made; everything else stays with the release.
    if os.path.exists(os.path.join(tmpDir, 'work')):
        shutil.rmtree(os.path.join(tmpDir, 'work'))
    for f in os.listdir(tmpDir):
        if f.endswith('.bed'):
            os.remove(os.path.join(tmpDir, f))
    writeText(os.path.join(tmpDir, 'version.txt'),
              'MaveDB API %s, data retrieved %s, converted at UCSC'
              % (pend['apiVersion'], pend['date']))
    os.remove(os.path.join(tmpDir, 'pending.json'))
    relDir = os.path.join(releasesDir, name)
    os.rename(tmpDir, relDir)

    os.makedirs(gbdbDir, exist_ok=True)
    for f in LINKED:
        relink(os.path.join(gbdbDir, f), os.path.join(relDir, f))
    if pend['refetch']:
        writeText(os.path.join(ottoDir, 'lastFullBuild.txt'), pend['date'])
    writeText(os.path.join(ottoDir, 'lastRelease.txt'), '%s\t%s' % (pend['key'], name))
    removed = pruneReleases(releasesDir, name)

    prevCat = mavemdFetch.readFingerprints(os.path.join(prevDir, 'catalogue.tsv')) \
        if prevDir and os.path.exists(prevDir) else {}
    added, changed, gone = catalogueChanges(
        prevCat, mavemdFetch.readFingerprints(os.path.join(relDir, 'catalogue.tsv')))
    p = prevStats or {}
    lines = ['MaveMD track updated: release %s%s' % (name, ', scheduled full rebuild'
                                                     if pend['fullDue'] else ''),
             'MaveDB API %s; %d score sets in the MaveMD collection'
             % (pend['apiVersion'], pend['scoreSets']),
             'score sets since %s: %d added, %d changed, %d removed'
             % (lastName or 'the start', len(added), len(changed), len(gone))]
    lines += urnList('added', added) + urnList('changed', changed) + urnList('removed', gone)
    lines += urnList('removed from the cache', pend.get('dropped') or [])
    lines += ['variant items: %s; heatmaps: %s; heatmap cells: %s'
              % (fmtChange(p.get('variantItems'), stats['variantItems']),
                 fmtChange(p.get('mapItems'), stats['mapItems']),
                 fmtChange(p.get('cells'), stats['cells'])),
              "measurements placed: {:,} of {:,} ({:.1f}%), {:.1f}% of them at MaveDB's "
              'own position'.format(
                  stats['variantItems'], stats['rows'],
                  100.0 * stats['variantItems'] / max(1, stats['rows']),
                  100.0 * stats['placedByMavedb'] / max(1, stats['variantItems']))]
    if stats.get('filterDrift'):
        lines.append('trackDb filter menus are missing values the data now has; '
                     'mavemd.ra needs updating by hand:')
        lines += ['  ' + d for d in stats['filterDrift']]
    lines += urnList('download failed, previous copy used', pend['failed'])
    lines += urnList('new, download failed, left out until next run', pend['failedNew'])
    if pend.get('superseded'):
        lines.append('superseded members (kept on purpose, the replacements have no '
                     'ExCALIBR calibration):')
        lines += ['  ' + s for s in pend['superseded'][:LIST_LIMIT]]
    if problems:
        lines.append('released despite failed checks (--ignoreChecks): ' +
                     '; '.join(problems))
    lines += urnList('old builds removed', removed)
    lines.append('%s/ -> %s' % (gbdbDir, relDir))
    print('\n'.join(lines))


def pruneReleases(releasesDir, current):
    """Remove releases older than the newest KEEP_RELEASES, and failed builds (.tmp),
    which a new release supersedes. Returns the names removed."""
    names = os.listdir(releasesDir)
    built = sorted((n for n in names if not n.endswith('.tmp')
                    and os.path.isdir(os.path.join(releasesDir, n))), key=releaseSortKey)
    doomed = [n for n in built[:-KEEP_RELEASES] if n != current]
    doomed += sorted(n for n in names if n.endswith('.tmp'))
    for n in doomed:
        shutil.rmtree(os.path.join(releasesDir, n))
    return doomed


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('-n', '--dryRun', action='store_true',
                    help='say what would happen; read the records only, write nothing')
    ap.add_argument('--force', action='store_true',
                    help='build a release even if nothing changed')
    ap.add_argument('--ignoreChecks', action='store_true',
                    help='release even if the release checks fail (after a person looked)')
    ap.add_argument('--release', metavar='NAME',
                    help='make the already built releases/NAME.tmp the release, without '
                    'fetching or building (with --ignoreChecks after a failed check)')
    ap.add_argument('--refetch', action='store_true',
                    help='download every score set again (a full rebuild)')
    ap.add_argument('--collection', default=mavemdFetch.MAVEMD_COLLECTION,
                    help='MaveDB collection URN (default: MaveMD)')
    ap.add_argument('--outDir', help='development: use this directory instead of ' + OTTO_DIR)
    ap.add_argument('--gbdbDir', help='where the links go (default %s, or outDir/gbdb with '
                    '--outDir)' % GBDB_DIR)
    args = ap.parse_args()

    if args.outDir:
        ottoDir = os.path.abspath(args.outDir)
        gbdbDir = os.path.abspath(args.gbdbDir or os.path.join(ottoDir, 'gbdb'))
    else:
        if HERE != OTTO_DIR:
            sys.exit('mavemd otto: this copy is not the installed one in %s; use --outDir '
                     'for a development run' % OTTO_DIR)
        ottoDir, gbdbDir = OTTO_DIR, args.gbdbDir or GBDB_DIR
    os.makedirs(os.path.join(ottoDir, 'logs'), exist_ok=True)
    if args.dryRun:
        dryRun(ottoDir, args)
        return

    lockFh = open(os.path.join(ottoDir, '.lock'), 'w')
    try:
        fcntl.flock(lockFh, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        sys.exit('mavemd otto: another run holds %s/.lock' % ottoDir)
    logPath = os.path.join(ottoDir, 'logs', datetime.date.today().isoformat() + '.log')
    with open(logPath, 'a', buffering=1) as log:   # line-buffered, to follow a run
        try:
            run(ottoDir, gbdbDir, args, log)
        except Exception as e:   # pylint: disable=broad-except
            traceback.print_exc(file=log)
            sys.stderr.write('mavemd otto: ERROR: %s\n(log: %s)\n' % (e, logPath))
            sys.exit(1)


if __name__ == '__main__':
    main()
