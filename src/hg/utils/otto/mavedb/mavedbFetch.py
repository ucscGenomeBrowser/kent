#!/usr/bin/env python3
"""Fetch the human MaveDB catalogue, score set records and variant CSVs into a cache.

usage: mavedbFetch.py cacheDir [--refetch] [--only URN ...] [--adopt catalogue.json]
                       [--catalogueOnly]

Writes into cacheDir:
    catalogue.json      search records of the human score sets (the build's input)
    catalogue.tsv       their fingerprints (urn, modificationDate, numVariants, experiment
                        modificationDate)
    fetched.tsv         the fingerprint each cached score set was fetched at
    scoreSets/<id>.json score set records (score set methodText, mappingState)
    variants/<id>.csv   variants/data with the scores, mavedb and clingen namespaces
    apiVersion.txt
    failed.txt          score sets that failed in the last run
A score set is fetched when its fingerprint differs from fetched.tsv or a cached file is
missing. One that fails keeps its old files and old fetched.tsv row, so the next run tries
again. The run fails when more than 5% of the score sets it tried fail, not counting those
that already failed last run. --adopt seeds a cache from an earlier download: score sets that already have a CSV
are recorded at their fingerprint in the catalogue that download was made from, so only
those changed since are fetched. --refetch fetches everything.

Pacing (MaveDB's request): variant downloads strictly one at a time with 2 s between
requests; at most 2 metadata requests at once; 429 waits Retry-After + 30 s and 5 in a row
abort the run; 502/503/504 back off 60, 120, 180, 240 s, then that score set fails.
"""

import argparse
import concurrent.futures
import csv
import json
import os
import re
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request

API = 'https://api.mavedb.org/api/v1'
NAMESPACES = ('scores', 'mavedb', 'clingen')
PAGE = 100000
SEARCH_PAGE = 100
VARIANT_DELAY = 2
METADATA_DELAY = 1
METADATA_WORKERS = 2
MAX_429 = 5
BACKOFF_5XX = (60, 120, 180, 240)
MAX_FAILED_FRACTION = 0.05
USER_AGENT = 'UCSC-Genome-Browser-otto/mavedb (genome-www@soe.ucsc.edu)'

HUMAN_ACCESSION = re.compile(r'^(NC_0000|NM_|NP_|ENST|ENSG|ENSP)')
FINGERPRINT_FIELDS = ('urn', 'modificationDate', 'numVariants', 'experimentModificationDate')


class FetchError(Exception):
    pass


class Api(object):
    """HTTP access to the MaveDB API with MaveDB's pacing."""

    def __init__(self, log=sys.stderr):
        self.log = log
        self.lock = threading.Lock()
        self.consecutive429 = 0

    def request(self, url, data=None, delay=VARIANT_DELAY, timeout=900):
        """Body of a GET (or a JSON POST when data is given)."""
        errors = 0
        while True:
            time.sleep(delay)
            req = urllib.request.Request(url, headers={'User-Agent': USER_AGENT})
            if data is not None:
                req.data = json.dumps(data).encode()
                req.add_header('Content-Type', 'application/json')
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
                    wait = int(e.headers.get('Retry-After') or 300) + 30
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
            except (urllib.error.URLError, OSError) as e:
                if errors < len(BACKOFF_5XX):
                    wait = BACKOFF_5XX[errors]
                    errors += 1
                    self.log.write('%s on %s, waiting %d s\n' % (e, url, wait))
                    time.sleep(wait)
                    continue
                raise FetchError('%s: %s' % (url, e))

    def json(self, url, data=None, delay=METADATA_DELAY):
        return json.loads(self.request(url, data, delay))


def quote(urn):
    return urllib.parse.quote(urn, safe='')


def fileStem(urn):
    return urn.split(':')[-1]


def apiVersion(api):
    return api.json(API + '/api/version').get('version', '')


def crawlOnce(api):
    """(records by URN, numScoreSets) from one paged walk of the published search."""
    records, total, offset = {}, None, 0
    while total is None or offset < total:
        page = api.json(API + '/score-sets/search',
                        {'published': True, 'offset': offset, 'limit': SEARCH_PAGE})
        total = page['numScoreSets']
        sets = page.get('scoreSets') or []
        if not sets:
            break
        for s in sets:
            records[s['urn']] = s
        offset += SEARCH_PAGE
    return records, total


def crawlCatalogue(api):
    """Every published score set. No organism filter: accession-based targets carry no
    organism and the filter drops them. The catalogue can change during a walk, so a
    count that does not match numScoreSets is walked again once, then fails the run
    (a partial walk must not look like withdrawals)."""
    for attempt in (1, 2):
        records, total = crawlOnce(api)
        if len(records) == total:
            return list(records.values())
        api.log.write('catalogue walk %d: %d score sets, numScoreSets %d\n'
                      % (attempt, len(records), total))
    raise FetchError('catalogue walk returned %d score sets, MaveDB reports %d'
                     % (len(records), total))


def isHumanTarget(target):
    """A Homo sapiens target sequence, or a human reference accession."""
    ts = target.get('targetSequence')
    if ts and (ts.get('taxonomy') or {}).get('organismName') == 'Homo sapiens':
        return True
    ta = target.get('targetAccession')
    if ta:
        if (ta.get('assembly') or '').startswith('GRCh38'):
            return True
        if HUMAN_ACCESSION.match(ta.get('accession') or ''):
            return True
    return False


def humanScoreSets(records):
    return sorted((s for s in records if any(isHumanTarget(t) for t in s.get('targetGenes')
                                             or [])), key=lambda s: s['urn'])


def fingerprint(scoreSet):
    exp = scoreSet.get('experiment') or {}
    return (scoreSet['urn'], scoreSet.get('modificationDate') or '',
            str(scoreSet.get('numVariants') or ''), exp.get('modificationDate') or '')


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


def fetchScoreSetRecord(api, urn, path):
    writeAtomic(path, api.request('%s/score-sets/%s' % (API, quote(urn)),
                                  delay=METADATA_DELAY))


def fetchVariants(api, urn, path):
    """All variant rows of a score set, paged, as one CSV."""
    ns = '&'.join('namespaces=' + n for n in NAMESPACES)
    chunks, offset = [], 0
    while True:
        body = api.request('%s/score-sets/%s/variants/data?%s&start=%d&limit=%d'
                           % (API, quote(urn), ns, offset, PAGE))
        lines = body.split(b'\n')
        chunks.append(body.rstrip(b'\n') if offset == 0
                      else b'\n'.join(lines[1:]).rstrip(b'\n'))
        if sum(1 for l in lines[1:] if l.strip()) < PAGE:
            break
        offset += PAGE
    writeAtomic(path, b'\n'.join(c for c in chunks if c) + b'\n')


def fetch(cacheDir, refetch=False, only=None, adopt=None, catalogueOnly=False,
          log=sys.stderr):
    """Update the cache; returns a summary dict (see the end of this function)."""
    api = Api(log)
    for sub in ('scoreSets', 'variants'):
        os.makedirs(os.path.join(cacheDir, sub), exist_ok=True)
    version = apiVersion(api)
    catalogue = humanScoreSets(crawlCatalogue(api))
    writeAtomic(os.path.join(cacheDir, 'catalogue.json'),
                json.dumps(catalogue, indent=0).encode())
    current = {s['urn']: fingerprint(s) for s in catalogue}
    writeFingerprints(os.path.join(cacheDir, 'catalogue.tsv'), current)
    writeAtomic(os.path.join(cacheDir, 'apiVersion.txt'), (version + '\n').encode())
    summary = {'apiVersion': version, 'scoreSets': len(catalogue), 'fetched': [],
               'failed': [], 'failedNew': [], 'adopted': 0}
    if catalogueOnly:
        return summary

    fetchedPath = os.path.join(cacheDir, 'fetched.tsv')
    fetched = readFingerprints(fetchedPath)

    def paths(urn):
        return (os.path.join(cacheDir, 'scoreSets', fileStem(urn) + '.json'),
                os.path.join(cacheDir, 'variants', fileStem(urn) + '.csv'))

    if adopt:
        with open(adopt) as fh:
            then = {s['urn']: fingerprint(s) for s in json.load(fh)}
        for urn, fp in then.items():
            if urn not in fetched and os.path.exists(paths(urn)[1]):
                fetched[urn] = fp
                summary['adopted'] += 1
        writeFingerprints(fetchedPath, fetched)

    wantRecord, wantVariants = [], []
    for urn in sorted(current):
        if only and urn not in only:
            continue
        recordPath, csvPath = paths(urn)
        changed = refetch or fetched.get(urn) != current[urn]
        if changed or not os.path.exists(recordPath):
            wantRecord.append(urn)
        if changed or not os.path.exists(csvPath):
            wantVariants.append(urn)
    log.write('%d human score sets; fetching %d records, %d variant files\n'
              % (len(current), len(wantRecord), len(wantVariants)))

    failed = set()
    with concurrent.futures.ThreadPoolExecutor(METADATA_WORKERS) as pool:
        futures = {pool.submit(fetchScoreSetRecord, api, urn, paths(urn)[0]): urn
                   for urn in wantRecord}
        for f in concurrent.futures.as_completed(futures):
            try:
                f.result()
            except FetchError as e:
                if 'consecutive 429' in str(e):
                    raise
                failed.add(futures[f])
                log.write('record failed: %s\n' % e)
    t0 = time.time()
    for i, urn in enumerate(wantVariants, 1):
        try:
            fetchVariants(api, urn, paths(urn)[1])
        except FetchError as e:
            if 'consecutive 429' in str(e):
                raise
            failed.add(urn)
            log.write('variants failed: %s\n' % e)
        if i % 50 == 0:
            log.write('%d/%d variant files (%.0f s)\n' % (i, len(wantVariants), time.time() - t0))

    # Only a score set whose files all arrived takes its new fingerprint.
    for urn in set(wantRecord) | set(wantVariants):
        if urn not in failed:
            fetched[urn] = current[urn]
            summary['fetched'].append(urn)
    writeFingerprints(fetchedPath, fetched)
    for urn in sorted(failed):
        hasCache = os.path.exists(paths(urn)[1])
        summary['failed' if hasCache else 'failedNew'].append(urn)
    summary['fetched'].sort()
    failedPath = os.path.join(cacheDir, 'failed.txt')
    before = set(readText(failedPath).split()) if os.path.exists(failedPath) else set()
    writeAtomic(failedPath, ''.join(u + '\n' for u in sorted(failed)).encode())
    tooMany = tooManyFailures(failed, set(wantRecord) | set(wantVariants), before)
    if tooMany:
        raise FetchError(tooMany)
    return summary


def readText(path):
    with open(path) as fh:
        return fh.read()


def tooManyFailures(failed, tried, before):
    """An error message when more than MAX_FAILED_FRACTION of the score sets tried this run
    failed, else None. A score set that also failed last run (before) is left out of the
    count: one that MaveDB times out on every month (00000053-a-1 since 2026-10) would
    otherwise be the only one tried in a quiet month and fail the run every time."""
    fresh = tried - before
    new = failed - before
    if fresh and len(new) > MAX_FAILED_FRACTION * len(fresh):
        return ('%d of %d score sets could not be fetched: %s'
                % (len(new), len(fresh), ' '.join(sorted(new))))
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cacheDir')
    ap.add_argument('--refetch', action='store_true')
    ap.add_argument('--only', nargs='+', metavar='URN')
    ap.add_argument('--adopt', metavar='catalogue.json')
    ap.add_argument('--catalogueOnly', action='store_true')
    args = ap.parse_args()
    s = fetch(args.cacheDir, args.refetch, set(args.only or []), args.adopt,
              args.catalogueOnly)
    sys.stdout.write('MaveDB API %s: %d human score sets, %d fetched, %d adopted, '
                     '%d failed (cached copy kept), %d new and not fetched\n'
                     % (s['apiVersion'], s['scoreSets'], len(s['fetched']), s['adopted'],
                        len(s['failed']), len(s['failedNew'])))
    for urn in s['failed'] + s['failedNew']:
        sys.stdout.write('  not fetched: %s\n' % urn)


if __name__ == '__main__':
    main()
