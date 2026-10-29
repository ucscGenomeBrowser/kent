#!/usr/bin/env python3
"""Keep a local cache of the MaveMD collection in step with the MaveDB API.

MaveMD ("MAVEs for MeDicine") is not a separate database. It is a curated collection
inside MaveDB holding the score sets that carry clinical calibrations, so everything here
comes from the ordinary MaveDB API at api.mavedb.org.

The cache, under cacheDir:
    collection.json                   the collection record (the list of score set URNs)
    scoreSets/<urn>.json              one score set record, with its calibrations
    variants/<urn>.csv                one flat CSV per score set, all namespaces joined
    catalogue.tsv                     fingerprint of every score set the API offers now
    fetched.tsv                       fingerprint each cached score set was downloaded at
    failed.txt                        score sets that failed this run
    apiVersion.txt                    MaveDB API version at fetch time

A score set is downloaded again only when its fingerprint changes, so an ordinary run
touches almost nothing. The fingerprint carries the score set's and the experiment's
modification dates, the variant count, and the list of CSV namespaces the score set
offers: a new ClinVar release appears as a new namespace on a record whose modification
date never moves, and we always request the newest one, so without it that change would
be invisible.

Request pacing follows what MaveDB asked us for directly (Ben Capodanno, 21 Aug 2026):
metadata at no more than two concurrent requests, the variant endpoint strictly
sequential, paging at 100k on large score sets, and backing off on a 504 rather than
retrying straight away, because the gateway gives up while their query keeps running.
"""

import argparse
import concurrent.futures
import csv
import http.client
import json
import os
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request

API = 'https://api.mavedb.org/api/v1'
MAVEMD_COLLECTION = 'urn:mavedb:collection-603dafbf-4a3f-4d70-ab8c-aafb226fbff4'

PAGE = 100000
VARIANT_DELAY = 2
METADATA_DELAY = 1
METADATA_WORKERS = 2
BACKOFF_5XX = (60, 300, 900)
MAX_429 = 5
# More than this share of the score sets tried failing means something is wrong at their
# end or ours, rather than one score set MaveDB cannot serve.
MAX_FAILED_FRACTION = 0.05
USER_AGENT = 'UCSC-Genome-Browser-otto/mavemd (genome-www@soe.ucsc.edu)'

# 'vep' is requested even though MaveDB leaves vep_functional_consequence empty for every
# variant in this collection today. Keeping the upstream column means the track picks the
# values up automatically if MaveDB starts populating it, rather than needing a schema
# change.
FIXED_NAMESPACES = ['scores', 'mavedb', 'clingen', 'gnomad', 'vep', 'score_set']

FINGERPRINT_FIELDS = ('urn', 'modificationDate', 'numVariants',
                      'experimentModificationDate', 'namespaces')


class FetchError(Exception):
    pass


class Api(object):
    """HTTP access to the MaveDB API with MaveDB's pacing."""

    def __init__(self, log=sys.stderr):
        self.log = log
        self.lock = threading.Lock()
        self.consecutive429 = 0

    def request(self, url, delay=VARIANT_DELAY, timeout=900):
        errors = 0
        while True:
            time.sleep(delay)
            req = urllib.request.Request(url, headers={'User-Agent': USER_AGENT})
            try:
                with urllib.request.urlopen(req, timeout=timeout) as r:
                    body = r.read()
                with self.lock:
                    self.consecutive429 = 0
                return body
            except urllib.error.HTTPError as e:
                if e.code == 429:
                    with self.lock:
                        self.consecutive429 += 1
                        n = self.consecutive429
                    if n >= MAX_429:
                        raise FetchError('%d consecutive 429 responses from MaveDB, stopping'
                                         % n)
                    try:
                        wait = int(e.headers.get('Retry-After') or 300) + 30
                    except ValueError:      # the HTTP-date form
                        wait = 330
                    self.log.write('429 from MaveDB, waiting %d s\n' % wait)
                    time.sleep(wait)
                    continue
                if e.code in (502, 503, 504) and errors < len(BACKOFF_5XX):
                    wait = BACKOFF_5XX[errors]
                    errors += 1
                    self.log.write('%d on %s, waiting %d s\n' % (e.code, url, wait))
                    time.sleep(wait)
                    continue
                raise FetchError('%s: HTTP %d' % (url, e.code))
            except (urllib.error.URLError, OSError, http.client.HTTPException) as e:
                # HTTPException: a connection dropped mid-body (IncompleteRead)
                if errors < len(BACKOFF_5XX):
                    wait = BACKOFF_5XX[errors]
                    errors += 1
                    self.log.write('%s on %s, waiting %d s\n' % (e, url, wait))
                    time.sleep(wait)
                    continue
                raise FetchError('%s: %s' % (url, e))

    def json(self, url, delay=METADATA_DELAY):
        body = self.request(url, delay)
        try:
            return json.loads(body)
        except ValueError:
            raise FetchError('%s: not JSON: %r' % (url, body[:200]))


def quote(urn):
    return urllib.parse.quote(urn, safe='')


def fileStem(urn):
    return urn.replace(':', '_')


def apiVersion(api):
    return api.json(API + '/api/version').get('version', '')


def namespacesFor(api, urn):
    """(namespaces to request, every namespace the score set offers).

    Every calibration is requested, research-use-only ones included: they are clearly
    labelled in the output and dropping them would lose the only ACMG evidence some score
    sets have. Only the most recent ClinVar release is requested; the API offers a dozen
    going back to 2015, which is history the track has no way to show.
    """
    available = api.json('%s/score-sets/%s/csv-namespaces' % (API, quote(urn)))
    offered = sorted(n['namespace'] for n in available)
    calibrations = sorted(n['namespace'] for n in available
                          if n.get('group') == 'calibration')
    clinvars = [n for n in offered if n.startswith('clinvar.')]
    return FIXED_NAMESPACES + clinvars[-1:] + calibrations, offered


def fingerprint(record, namespaces):
    exp = record.get('experiment') or {}
    return (record['urn'], record.get('modificationDate') or '',
            str(record.get('numVariants') or ''), exp.get('modificationDate') or '',
            ','.join(namespaces))


def readFingerprints(path):
    """{urn: fingerprint tuple} from a catalogue.tsv or fetched.tsv; {} if absent."""
    if not os.path.exists(path):
        return {}
    with open(path, newline='') as fh:
        rows = list(csv.reader(fh, delimiter='\t'))
    return {r[0]: tuple(r) for r in rows[1:] if r}


def writeFingerprints(path, fingerprints):
    tmp = path + '.tmp'
    with open(tmp, 'w', newline='') as fh:
        w = csv.writer(fh, delimiter='\t', lineterminator='\n')
        w.writerow(FINGERPRINT_FIELDS)
        for urn in sorted(fingerprints):
            w.writerow(fingerprints[urn])
    os.rename(tmp, path)


def writeAtomic(path, data):
    tmp = path + '.tmp'
    with open(tmp, 'wb') as fh:
        fh.write(data)
    os.rename(tmp, path)


def fetchVariants(api, urn, path, wanted, expected=None):
    """All variant rows of a score set, paged, as one CSV.

    The file is only written when it starts with the CSV header and, given expected (the
    record's numVariants), has that many rows: a short page would otherwise end the paging
    early and leave a truncated file that every build reads until the next refetch.
    """
    query = '&'.join('namespaces=' + urllib.parse.quote(n, safe='') for n in wanted)
    chunks, offset = [], 0
    while True:
        body = api.request('%s/score-sets/%s/variants/data?%s&start=%d&limit=%d'
                           % (API, quote(urn), query, offset, PAGE))
        lines = body.split(b'\n')
        chunks.append(body.rstrip(b'\n') if offset == 0
                      else b'\n'.join(lines[1:]).rstrip(b'\n'))
        if sum(1 for l in lines[1:] if l.strip()) < PAGE:
            break
        offset += PAGE
    data = b'\n'.join(c for c in chunks if c) + b'\n'
    if not data.startswith(b'accession,'):
        raise FetchError('%s variants: no CSV header: %r' % (urn, data[:200]))
    rows = sum(1 for l in data.split(b'\n')[1:] if l.strip())
    if expected is not None and rows != expected:
        raise FetchError('%s variants: %d rows, the record says %d' % (urn, rows, expected))
    writeAtomic(path, data)


def supersededBy(record):
    """The URN that supersedes this score set, or None.

    Reported, never followed. Seven members of the collection are superseded today and
    their replacements carry no ExCALIBR calibration yet, so moving to them would strip
    the ACMG evidence from tens of thousands of items. The collection is the authority on
    which version to show; this only makes the situation visible in the build log.
    """
    sup = record.get('supersedingScoreSet')
    if isinstance(sup, dict):
        return sup.get('urn')
    return sup or None


def fetch(cacheDir, refetch=False, collection=MAVEMD_COLLECTION, catalogueOnly=False,
          log=sys.stderr):
    """Update the cache; returns a summary dict (see the end of this function)."""
    api = Api(log)
    for sub in ('scoreSets', 'variants'):
        os.makedirs(os.path.join(cacheDir, sub), exist_ok=True)
    version = apiVersion(api)
    writeAtomic(os.path.join(cacheDir, 'apiVersion.txt'), (version + '\n').encode())

    coll = api.json('%s/collections/%s' % (API, quote(collection)))
    writeAtomic(os.path.join(cacheDir, 'collection.json'),
                json.dumps(coll, indent=1).encode())
    urns = list(coll['scoreSetUrns'])
    log.write('collection %s: %d score sets, modified %s\n'
              % (coll.get('name'), len(urns), coll.get('modificationDate')))

    summary = {'apiVersion': version, 'collection': coll.get('name'),
               'scoreSets': len(urns), 'fetched': [], 'failed': [], 'failedNew': [],
               'superseded': [], 'dropped': []}

    # The record and the namespace list together make the fingerprint, so both are read
    # every run. Two concurrent requests, as asked.
    def describe(urn):
        record = api.json('%s/score-sets/%s' % (API, quote(urn)))
        wanted, offered = namespacesFor(api, urn)
        return urn, record, wanted, offered

    current, records, wantedNs = {}, {}, {}
    with concurrent.futures.ThreadPoolExecutor(METADATA_WORKERS) as pool:
        for urn, record, wanted, offered in pool.map(describe, urns):
            current[urn] = fingerprint(record, offered)
            records[urn] = record
            wantedNs[urn] = wanted
            sup = supersededBy(record)
            if sup:
                summary['superseded'].append('%s superseded by %s' % (urn, sup))
    writeFingerprints(os.path.join(cacheDir, 'catalogue.tsv'), current)
    summary['superseded'].sort()

    def paths(urn):
        return (os.path.join(cacheDir, 'scoreSets', fileStem(urn) + '.json'),
                os.path.join(cacheDir, 'variants', fileStem(urn) + '.csv'))

    if catalogueOnly:
        return summary

    fetchedPath = os.path.join(cacheDir, 'fetched.tsv')
    fetched = readFingerprints(fetchedPath)

    # A score set the collection no longer names must leave the cache, because the
    # converters read it by globbing the directory: left behind, its variants would keep
    # being built into the track. MaveDB replaced seven superseded members on 2026-10-21,
    # which is exactly this case.
    for urn in sorted(set(fetched) - set(current)):
        for path in paths(urn):
            if os.path.exists(path):
                os.remove(path)
        del fetched[urn]
        summary['dropped'].append(urn)
    for sub, ext in (('variants', '.csv'), ('scoreSets', '.json')):
        stems = {fileStem(u) for u in current}
        for name in sorted(os.listdir(os.path.join(cacheDir, sub))):
            if name.endswith(ext) and name[:-len(ext)] not in stems:
                os.remove(os.path.join(cacheDir, sub, name))
                if name[:-len(ext)] not in {fileStem(u) for u in summary['dropped']}:
                    summary['dropped'].append(name[:-len(ext)])
    if summary['dropped']:
        summary['dropped'].sort()
        writeFingerprints(fetchedPath, fetched)
        log.write('removed from the cache, no longer in the collection: %s\n'
                  % ' '.join(summary['dropped']))

    want = []
    for urn in sorted(current):
        recordPath, csvPath = paths(urn)
        if refetch or fetched.get(urn) != current[urn] \
                or not os.path.exists(csvPath) or not os.path.exists(recordPath):
            want.append(urn)
    log.write('%d score sets; %d to download\n' % (len(current), len(want)))

    failed, done = set(), set()
    t0 = time.time()
    try:
        for i, urn in enumerate(want, 1):
            recordPath, csvPath = paths(urn)
            expected = records[urn].get('numVariants')
            try:
                writeAtomic(recordPath, json.dumps(records[urn], indent=1).encode())
                fetchVariants(api, urn, csvPath, wantedNs[urn],
                              int(expected) if expected else None)
                done.add(urn)
                log.write('  %d/%d %s (%.0f s)\n' % (i, len(want), urn, time.time() - t0))
            except FetchError as e:
                if 'consecutive 429' in str(e):
                    raise
                failed.add(urn)
                log.write('  %d/%d %s FAILED: %s\n' % (i, len(want), urn, e))
    finally:
        # Only a score set whose files all arrived takes its new fingerprint. Written even
        # when the run stops early, so what did arrive is not fetched again.
        for urn in done:
            fetched[urn] = current[urn]
            summary['fetched'].append(urn)
        writeFingerprints(fetchedPath, fetched)

    for urn in sorted(failed):
        summary['failed' if os.path.exists(paths(urn)[1]) else 'failedNew'].append(urn)
    summary['fetched'].sort()
    failedPath = os.path.join(cacheDir, 'failed.txt')
    before = set()
    if os.path.exists(failedPath):
        with open(failedPath) as fh:
            before = set(fh.read().split())
    writeAtomic(failedPath, ''.join(u + '\n' for u in sorted(failed)).encode())
    tooMany = tooManyFailures(failed, set(want), before)
    if tooMany:
        raise FetchError(tooMany)
    return summary


def tooManyFailures(failed, tried, before):
    """An error message when more than MAX_FAILED_FRACTION of the score sets tried this run
    failed, else None. A score set that also failed last run (before) is left out of the
    count: one that MaveDB times out on every week would otherwise be the only one tried
    in a quiet week and fail the run every time."""
    fresh = tried - before
    new = failed - before
    if fresh and len(new) > MAX_FAILED_FRACTION * len(fresh):
        return ('%d of %d score sets could not be fetched: %s'
                % (len(new), len(fresh), ' '.join(sorted(new))))
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cacheDir', help='directory to keep the download in')
    ap.add_argument('--collection', default=MAVEMD_COLLECTION,
                    help='MaveDB collection URN (default: MaveMD)')
    ap.add_argument('--refetch', action='store_true',
                    help='download every score set again, not only the changed ones')
    ap.add_argument('--catalogueOnly', action='store_true',
                    help='read the collection and the records, download no variants')
    args = ap.parse_args()
    summary = fetch(args.cacheDir, refetch=args.refetch, collection=args.collection,
                    catalogueOnly=args.catalogueOnly)
    print('MaveDB API %s, %d score sets, %d downloaded, %d failed'
          % (summary['apiVersion'], summary['scoreSets'], len(summary['fetched']),
             len(summary['failed']) + len(summary['failedNew'])))
    for line in summary['superseded']:
        print('  ' + line)


if __name__ == '__main__':
    main()
