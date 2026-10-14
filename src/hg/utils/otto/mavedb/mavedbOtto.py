#!/usr/bin/env python3
"""mavedbOtto.py -- keep the hg38 MaveDB track in sync with the MaveDB API.

Monthly cron. Each run updates the cache from the API (mavedbFetch.py: only score sets
whose fingerprint changed are downloaded), and when the score sets the build would read
differ from those of the last release, builds a new release (mavedbBuild.py) into
releases/YYYY-MM-DD/, checks it, and moves the /gbdb/hg38/maveDB/ symlinks to it. Every
quarter (first run of January, April, July and October, or 90 days after the last one) all
variant files are downloaded again and the release is rebuilt even if nothing changed, which
picks up MaveDB remaps that leave the fingerprint alone and RefSeq/GENCODE table updates.

Silent when nothing changed. A release prints a short summary, which cron mails; anything
unexpected raises and exits non-zero. A release that fails a check is left in
releases/YYYY-MM-DD.tmp/ and the /gbdb links are not touched; once a person has looked at
it, re-run with --force --ignoreChecks to release it anyway.

Only the new file names are written (mavedbMaps.bb, mavedbVariants.bb, mavedbAlignDna.bb,
mavedbAlignAa.bb, version.txt); all_mave.bb, mavedb_dna.bb and mavedb_aa.bb (the 2025 build
beta and the RR serve) are never touched. Redmine #38396.

Development runs use --outDir, which also puts the /gbdb links under outDir/gbdb unless
--gbdbDir says otherwise.
"""

import argparse
import datetime
import fcntl
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import traceback

import mavedbBuild
import mavedbFetch

# Where the cron runs. Not derived from __file__, so that the tree copy cannot write to the
# live directory by accident.
OTTO_DIR = '/hive/data/outside/otto/mavedb'
GBDB_DIR = '/gbdb/hg38/maveDB'
HERE = os.path.dirname(os.path.abspath(__file__))

BIGBEDS = ('mavedbMaps.bb', 'mavedbVariants.bb', 'mavedbAlignDna.bb', 'mavedbAlignAa.bb')
LINKED = BIGBEDS + ('version.txt',)

# A new release may lose at most this share of the previous release's items or shown score
# sets without a person looking first. MaveDB adds score sets far more often than it removes
# them, so a larger drop means something went wrong upstream or in the build.
MAX_ITEM_DROP = 0.10
MAX_SCORE_SET_DROP = 0.05

FULL_BUILD_MONTHS = (1, 4, 7, 10)
FULL_BUILD_DAYS = 90
LIST_LIMIT = 20


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


def readLastRelease(ottoDir):
    """(release key, release directory name) of the last release, or (None, None)."""
    text = readText(os.path.join(ottoDir, 'lastRelease.txt'))
    if not text:
        return None, None
    key, name = text.split('\t')
    return key, name


def releaseKey(fetched, catalogue, variantsDir):
    """Hash of what a build would read: the fingerprint each buildable score set was fetched
    at, and the names left out. A score set that failed to download keeps its old row, so
    a failure alone does not make a release."""
    lines = sorted('\t'.join(fetched[s['urn']]) for s in catalogue
                   if s['urn'] in fetched
                   and os.path.exists(mavedbBuild.scoreSetFile(variantsDir, s['urn'])))
    lines += sorted('excluded\t' + u for u in mavedbBuild.EXCLUDED_SCORE_SETS)
    return hashlib.sha1('\n'.join(lines).encode()).hexdigest()


def fullBuildDue(today, lastFull):
    """True on the first run of a quarter month, or when the last full build is too old."""
    if lastFull is None:
        return True
    if (today - lastFull).days >= FULL_BUILD_DAYS:
        return True
    return today.month in FULL_BUILD_MONTHS and (lastFull.year, lastFull.month) != \
        (today.year, today.month)


def readDate(path):
    text = readText(path)
    return datetime.date.fromisoformat(text) if text else None


def releaseName(releasesDir, today):
    """YYYY-MM-DD, or YYYY-MM-DD.N when a release of that day already exists."""
    name, n = today.isoformat(), 1
    while os.path.exists(os.path.join(releasesDir, name)):
        n += 1
        name = '%s.%d' % (today.isoformat(), n)
    return name


def bigBedItemCount(path):
    """itemCount from bigBedInfo, which also fails on a corrupt file."""
    out = subprocess.run(['bigBedInfo', path], check=True, stdout=subprocess.PIPE,
                         universal_newlines=True).stdout
    m = re.search(r'^itemCount:\s+([0-9,]+)', out, re.M)
    if not m:
        raise OttoError('no itemCount in bigBedInfo output for %s' % path)
    return int(m.group(1).replace(',', ''))


def checkRelease(stats, prevStats, itemCounts):
    """Problems that stop a release (empty list = release). itemCounts: bigBed -> count."""
    problems = []
    for bb, n in sorted(itemCounts.items()):
        if n == 0:
            problems.append('%s has no items' % bb)
    if stats['badItems']:
        problems.append('%d heatmap items failed the colour/field check and were dropped'
                        % stats['badItems'])
    if prevStats:
        for key, label, limit in (('mapItems', 'heatmap items', MAX_ITEM_DROP),
                                  ('variantItems', 'variant items', MAX_ITEM_DROP),
                                  ('shownSets', 'score sets shown', MAX_SCORE_SET_DROP)):
            old, new = prevStats[key], stats[key]
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
    """Say what a run would do; downloads only the catalogue and writes nothing."""
    fullDue = fullBuildDue(datetime.date.today(),
                           readDate(os.path.join(ottoDir, 'lastFullBuild.txt')))
    cacheDir = os.path.join(ottoDir, 'cache')
    api = mavedbFetch.Api(sys.stderr)
    catalogue = mavedbFetch.humanScoreSets(mavedbFetch.crawlCatalogue(api))
    current = {s['urn']: mavedbFetch.fingerprint(s) for s in catalogue}
    fetched = mavedbFetch.readFingerprints(os.path.join(cacheDir, 'fetched.tsv'))
    toFetch = [u for u in current if fetched.get(u) != current[u]]
    _, lastName = readLastRelease(ottoDir)
    prevCat = mavedbFetch.readFingerprints(
        os.path.join(ottoDir, 'releases', lastName, 'catalogue.tsv')) if lastName else {}
    added, changed, removed = catalogueChanges(prevCat, current)
    refetch = args.refetch or fullDue
    print('mavedb otto dry run: %d human score sets; %d to download%s; since the last '
          'release (%s): %d added, %d changed, %d removed'
          % (len(current), len(current) if refetch else len(toFetch),
             ' (full download)' if refetch else '', lastName or 'none',
             len(added), len(changed), len(removed)))
    if refetch or args.force or toFetch or added or changed or removed:
        print('a new release would be built%s' % (' (scheduled full rebuild)' if fullDue
                                                   else ''))
    else:
        print('nothing would be done')


def run(ottoDir, gbdbDir, args, log):
    today = datetime.date.today()
    cacheDir = os.path.join(ottoDir, 'cache')
    releasesDir = os.path.join(ottoDir, 'releases')
    lastFullPath = os.path.join(ottoDir, 'lastFullBuild.txt')
    fullDue = fullBuildDue(today, readDate(lastFullPath))
    os.makedirs(releasesDir, exist_ok=True)
    refetch = args.refetch or fullDue

    summary = mavedbFetch.fetch(cacheDir, refetch=refetch, log=log)
    with open(os.path.join(cacheDir, 'catalogue.json')) as fh:
        catalogue = json.load(fh)
    variantsDir = os.path.join(cacheDir, 'variants')
    fetched = mavedbFetch.readFingerprints(os.path.join(cacheDir, 'fetched.tsv'))
    key = releaseKey(fetched, catalogue, variantsDir)
    lastKey, lastName = readLastRelease(ottoDir)
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
    report, stats = mavedbBuild.build(catalogue, variantsDir, tmpDir,
                                      os.path.join(cacheDir, 'scoreSets'), log=log)
    itemCounts = {bb: bigBedItemCount(os.path.join(tmpDir, bb)) for bb in BIGBEDS}
    prevDir = os.path.join(releasesDir, lastName) if lastName else None
    prevStats = None
    if prevDir and os.path.exists(os.path.join(prevDir, 'stats.json')):
        with open(os.path.join(prevDir, 'stats.json')) as fh:
            prevStats = json.load(fh)
    problems = checkRelease(stats, prevStats, itemCounts)
    if problems and not args.ignoreChecks:
        raise OttoError('release %s not made, the /gbdb links are unchanged:\n  %s\n'
                        'The build is in %s (report.txt). Once it has been checked, re-run '
                        'with --force --ignoreChecks to release it.'
                        % (name, '\n  '.join(problems), tmpDir))

    # The .bed inputs (1.8 GB) and the converter work files are not needed once the bigBeds
    # are made; everything else stays with the release.
    shutil.rmtree(os.path.join(tmpDir, 'work'))
    for f in os.listdir(tmpDir):
        if f.endswith('.bed'):
            os.remove(os.path.join(tmpDir, f))
    shutil.copy(os.path.join(cacheDir, 'catalogue.tsv'), tmpDir)
    shutil.copy(os.path.join(cacheDir, 'apiVersion.txt'), tmpDir)
    log.flush()
    shutil.copy(log.name, os.path.join(tmpDir, 'build.log'))
    with open(os.path.join(tmpDir, 'stats.json'), 'w') as fh:
        json.dump(stats, fh, indent=1, sort_keys=True)
    writeText(os.path.join(tmpDir, 'version.txt'),
              'MaveDB API %s, data retrieved %s, converted at UCSC'
              % (summary['apiVersion'], today.isoformat()))
    relDir = os.path.join(releasesDir, name)
    os.rename(tmpDir, relDir)

    os.makedirs(gbdbDir, exist_ok=True)
    for f in LINKED:
        relink(os.path.join(gbdbDir, f), os.path.join(relDir, f))
    if refetch:
        writeText(lastFullPath, today.isoformat())
    writeText(os.path.join(ottoDir, 'lastRelease.txt'), '%s\t%s' % (key, name))

    prevCat = mavedbFetch.readFingerprints(os.path.join(prevDir, 'catalogue.tsv')) \
        if prevDir else {}
    added, changed, removed = catalogueChanges(
        prevCat, mavedbFetch.readFingerprints(os.path.join(relDir, 'catalogue.tsv')))
    p = prevStats or {}
    ucsc = stats['placed'] - stats['placedByMavedb']
    lines = ['MaveDB track updated: release %s%s' % (name, ', scheduled full rebuild'
                                                     if fullDue else ''),
             'MaveDB API %s; %d human score sets in MaveDB' % (summary['apiVersion'],
                                                               summary['scoreSets']),
             'score sets since %s: %d added, %d changed, %d removed'
             % (lastName or 'the start', len(added), len(changed), len(removed))]
    lines += urnList('added', added) + urnList('changed', changed) + \
        urnList('removed', removed)
    lines += ['score sets shown: %s (%d left out with under 20%% of variants placed)'
              % (fmtChange(p.get('shownSets'), stats['shownSets']), stats['gatedSets']),
              'heatmap items: %s; variant items: %s'
              % (fmtChange(p.get('mapItems'), stats['mapItems']),
                 fmtChange(p.get('variantItems'), stats['variantItems'])),
              'single variants placed: {:,} of {:,} ({:.1f}%), {:.1f}% of them by UCSC'
              .format(stats['placed'], stats['single'],
                      100.0 * stats['placed'] / max(1, stats['single']),
                      100.0 * ucsc / max(1, stats['placed']))]
    lines += urnList('download failed, previous copy used', summary['failed'])
    lines += urnList('new, download failed, left out until next run',
                     summary['failedNew'])
    if problems:
        lines.append('released despite failed checks (--ignoreChecks): ' +
                     '; '.join(problems))
    lines.append('%s/ -> %s' % (gbdbDir, relDir))
    print('\n'.join(lines))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('-n', '--dryRun', action='store_true',
                    help='say what would happen; download the catalogue only, write nothing')
    ap.add_argument('--force', action='store_true',
                    help='build a release even if no score set changed')
    ap.add_argument('--ignoreChecks', action='store_true',
                    help='release even if the release checks fail (after a person looked)')
    ap.add_argument('--refetch', action='store_true',
                    help='download every variant file again (a full rebuild)')
    ap.add_argument('--outDir', help='development: use this directory instead of ' + OTTO_DIR)
    ap.add_argument('--gbdbDir', help='where the links go (default %s, or outDir/gbdb with '
                    '--outDir)' % GBDB_DIR)
    args = ap.parse_args()

    if args.outDir:
        ottoDir = os.path.abspath(args.outDir)
        gbdbDir = os.path.abspath(args.gbdbDir or os.path.join(ottoDir, 'gbdb'))
    else:
        if HERE != OTTO_DIR:
            sys.exit('mavedb otto: this copy is not the installed one in %s; use --outDir '
                     'for a development run' % OTTO_DIR)
        ottoDir, gbdbDir = OTTO_DIR, args.gbdbDir or GBDB_DIR
    if args.dryRun:
        dryRun(ottoDir, args)
        return
    os.makedirs(os.path.join(ottoDir, 'logs'), exist_ok=True)

    lockFh = open(os.path.join(ottoDir, '.lock'), 'w')
    try:
        fcntl.flock(lockFh, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        sys.exit('mavedb otto: another run holds %s/.lock' % ottoDir)
    logPath = os.path.join(ottoDir, 'logs', datetime.date.today().isoformat() + '.log')
    with open(logPath, 'a', buffering=1) as log:   # line-buffered, to follow a run
        try:
            run(ottoDir, gbdbDir, args, log)
        except Exception as e:   # pylint: disable=broad-except
            traceback.print_exc(file=log)
            sys.stderr.write('mavedb otto: ERROR: %s\n(log: %s)\n' % (e, logPath))
            sys.exit(1)


if __name__ == '__main__':
    main()
