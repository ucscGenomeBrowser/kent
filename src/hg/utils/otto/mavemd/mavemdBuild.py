#!/usr/bin/env python3
"""Build the two hg38 MaveMD bigBeds from a mavemdFetch.py cache.

Produces, in the output directory: mavemdVar.bb (one item per variant per score set),
mavemdMap.bb (one variant effect map per score set), mavemdFilters.ra (the generated
trackDb filterValues block) and a log per converter. Returns the counters the otto job
checks a release against.

This replaces the hand-run runBuild.sh. The converters are run as subprocesses rather
than imported, because each one holds the whole collection in memory and running them in
one process would hold both at once. Their output goes to a log file rather than to the
terminal, so that a weekly cron mails nothing on an ordinary run.
"""

import json
import os
import subprocess
import sys

import mavemdLib            # noqa: F401  (puts ../mavedb on sys.path)
import mavedbPlace as mp

HERE = os.path.dirname(os.path.abspath(__file__))
CHROM_SIZES = '/hive/data/genomes/hg38/chrom.sizes'

VAR_BB = 'mavemdVar.bb'
MAP_BB = 'mavemdMap.bb'
FILTERS_RA = 'mavemdFilters.ra'
BIGBEDS = (VAR_BB, MAP_BB)

# The committed trackDb stanza. otto cannot edit it (trackDb lives in git, and the
# filterValues menus are written there by hand), so the build compares what the data now
# needs with what the stanza offers and says so when they differ. `make install` copies
# the stanza next to the scripts, because the kent tree is not on the otto user's path;
# in a tree run the tree copy is used directly.
TRACKDB_COPIES = (
    os.path.join(HERE, 'mavemd.ra'),
    os.path.normpath(os.path.join(HERE, '..', '..', '..', 'makeDb', 'trackDb', 'human',
                                  'hg38', 'mavemd.ra')),
)


def trackDbRa():
    for path in TRACKDB_COPIES:
        if os.path.exists(path):
            return path
    return None


class BuildError(Exception):
    pass


def runConverter(script, args, logPath):
    """Run one converter, its chatter to logPath; on failure the tail goes in the error."""
    with open(logPath, 'w') as log:
        r = subprocess.run([sys.executable, os.path.join(HERE, script)] + args,
                           stdout=log, stderr=subprocess.STDOUT)
    if r.returncode:
        with open(logPath) as fh:
            tail = fh.read()[-2000:]
        raise BuildError('%s failed (exit %d):\n%s' % (script, r.returncode, tail))


def readJson(path):
    with open(path) as fh:
        return json.load(fh)


def parseFilterValues(text):
    """{field: [value, ...]} from trackDb filterValues lines.

    A doubled comma is a literal one (slNameListFromCommaEscaped in lib/common.c), which
    ClinVar terms like "Pathogenic, low penetrance" rely on.
    """
    out = {}
    for line in text.splitlines():
        line = line.strip()
        if not line.startswith('filterValues.'):
            continue
        setting, _, rest = line.partition(' ')
        field = setting[len('filterValues.'):]
        values, cur, i = [], '', 0
        while i < len(rest):
            if rest[i] == ',':
                if i + 1 < len(rest) and rest[i + 1] == ',':
                    cur += ','
                    i += 2
                    continue
                values.append(cur)
                cur = ''
                i += 1
                continue
            cur += rest[i]
            i += 1
        if cur:
            values.append(cur)
        out[field] = values
    return out


def filterDrift(statsPath, trackDbPath=None):
    """Values the data now has that the committed trackDb menu does not offer.

    Returns a list of 'field: value, value' lines, empty when the menus are complete. A
    value the menu offers but the data no longer has is harmless (an empty menu entry), so
    it is not reported.
    """
    trackDbPath = trackDbPath or trackDbRa()
    if not trackDbPath or not os.path.exists(trackDbPath):
        return ['trackDb stanza not found, filter menus not checked']
    wanted = readJson(statsPath).get('filterValues') or {}
    with open(trackDbPath) as fh:
        have = parseFilterValues(fh.read())
    lines = []
    for field in sorted(wanted):
        missing = sorted(set(wanted[field]) - set(have.get(field, [])))
        if missing:
            lines.append('%s: %s' % (field, ', '.join(missing)))
    return lines


def build(cacheDir, outDir, log=sys.stderr):
    """Build both bigBeds into outDir. Returns the stats dict."""
    os.makedirs(outDir, exist_ok=True)
    work = os.path.join(outDir, 'work')
    os.makedirs(work, exist_ok=True)
    varStats = os.path.join(work, 'varStats.json')
    mapStats = os.path.join(work, 'mapStats.json')

    log.write('building per-variant bed\n')
    log.flush()
    runConverter('makeMaveMdVariants.py',
                 [cacheDir, os.path.join(outDir, 'mavemdVar.bed'),
                  '--raFragment', os.path.join(outDir, FILTERS_RA),
                  '--statsJson', varStats, '--workDir', work],
                 os.path.join(outDir, 'buildVar.log'))

    log.write('building heatmap bed\n')
    log.flush()
    runConverter('makeMaveMdHeatmap.py',
                 [cacheDir, os.path.join(outDir, 'mavemdMap.bed'),
                  '--statsJson', mapStats, '--workDir', work],
                 os.path.join(outDir, 'buildMap.log'))

    log.write('sorting and writing bigBeds\n')
    log.flush()
    for bed in ('mavemdVar.bed', 'mavemdMap.bed'):
        mp.runQuiet(['bedSort', os.path.join(outDir, bed),
                     os.path.join(outDir, bed.replace('.bed', '.sorted.bed'))])
    mp.runQuiet(['bedToBigBed', '-type=bed12+35', '-tab',
                 '-as=' + os.path.join(HERE, 'mavemdVariants.as'),
                 '-extraIndex=name,clinGenId,variantUrn',
                 os.path.join(outDir, 'mavemdVar.sorted.bed'), CHROM_SIZES,
                 os.path.join(outDir, VAR_BB)])
    mp.runQuiet(['bedToBigBed', '-type=bed12+20', '-tab',
                 '-as=' + os.path.join(HERE, 'mavemdHeatmap.as'),
                 os.path.join(outDir, 'mavemdMap.sorted.bed'), CHROM_SIZES,
                 os.path.join(outDir, MAP_BB)])

    stats = readJson(varStats)
    stats.update(readJson(mapStats))
    stats['filterDrift'] = filterDrift(varStats)
    stats.pop('filterValues', None)      # the menus themselves are in mavemdFilters.ra
    return stats


def main():
    import argparse
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cacheDir', help='a mavemdFetch.py cache')
    ap.add_argument('outDir', help='where the bigBeds go')
    args = ap.parse_args()
    stats = build(args.cacheDir, args.outDir)
    print(json.dumps(stats, indent=1, sort_keys=True))


if __name__ == '__main__':
    main()
