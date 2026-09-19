#!/usr/bin/env python3
"""Download the MaveMD collection from the MaveDB API.

MaveMD ("MAVEs for MeDicine") is not a separate database.  It is a curated collection
inside MaveDB holding the score sets that carry clinical calibrations, so everything here
comes from the ordinary MaveDB API at api.mavedb.org.

Writes, under the output directory:
    collection.json                   the collection record (the list of score set URNs)
    scoreSets/<urn>.json              one score set record, with its calibrations
    variants/<urn>.csv                one flat CSV per score set, all namespaces joined
    apiVersion.txt                    MaveDB API version at fetch time

Request pacing follows what MaveDB asked us for directly (Ben Capodanno, 21 Aug 2026):
metadata at no more than two concurrent requests, the score and variant endpoints strictly
sequential, paging at 100k on large score sets, and backing off on a 504 rather than
retrying straight away, because the gateway gives up while their query keeps running.
"""

import argparse
import concurrent.futures
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

API = 'https://api.mavedb.org/api/v1'
MAVEMD_COLLECTION = 'urn:mavedb:collection-603dafbf-4a3f-4d70-ab8c-aafb226fbff4'
METADATA_CONCURRENCY = 2
PAGE_SIZE = 100000
# 'vep' is requested even though MaveDB leaves vep_functional_consequence empty for every
# variant in this collection today. Keeping the upstream column means the track picks the
# values up automatically if MaveDB starts populating it, rather than needing a schema change.
FIXED_NAMESPACES = ['scores', 'mavedb', 'clingen', 'gnomad', 'vep', 'score_set']


def fetch(url, timeout=900, tries=4):
    """GET a URL, backing off on failure.  A 504 gets a longer wait than a transport error."""
    for attempt in range(tries):
        try:
            with urllib.request.urlopen(url, timeout=timeout) as response:
                return response.read()
        except urllib.error.HTTPError as e:
            if e.code == 504 and attempt < tries - 1:
                wait = 60 * (attempt + 1)
                sys.stderr.write("  504 from %s, backing off %ds\n" % (url, wait))
                time.sleep(wait)
                continue
            raise
        except Exception as e:
            if attempt == tries - 1:
                raise
            wait = 20 * (attempt + 1)
            sys.stderr.write("  %s, retrying in %ds\n" % (e, wait))
            time.sleep(wait)
    raise RuntimeError('unreachable')


def fetchJson(url, timeout=900):
    return json.loads(fetch(url, timeout))


def quote(urn):
    return urllib.parse.quote(urn, safe='')


def namespacesFor(urn):
    """Pick the CSV column groups to request for one score set.

    Every calibration is requested, research-use-only ones included: they are clearly
    labelled in the output and dropping them would lose the only ACMG evidence some score
    sets have.  Only the most recent ClinVar release is requested; the API offers twelve
    going back to 2015, which is history the track has no way to show.
    """
    available = fetchJson('%s/score-sets/%s/csv-namespaces' % (API, quote(urn)), 120)
    calibrations = [n['namespace'] for n in available if n['group'] == 'calibration']
    clinvars = sorted(n['namespace'] for n in available
                      if n['namespace'].startswith('clinvar.'))
    latestClinvar = [clinvars[-1]] if clinvars else []
    return FIXED_NAMESPACES + latestClinvar + calibrations


def fetchVariants(urn, path):
    """Fetch one score set's variant CSV, paging so a large score set cannot 504."""
    query = '&'.join('namespaces=' + urllib.parse.quote(n, safe='')
                     for n in namespacesFor(urn))
    chunks = []
    start = 0
    while True:
        body = fetch('%s/score-sets/%s/variants/data?%s&start=%d&limit=%d'
                     % (API, quote(urn), query, start, PAGE_SIZE))
        lines = body.split(b'\n')
        chunks.append(body if start == 0 else b'\n'.join(lines[1:]))
        if len([l for l in lines[1:] if l.strip()]) < PAGE_SIZE:
            break
        start += PAGE_SIZE
    data = b'\n'.join(chunks)
    with open(path, 'wb') as fh:
        fh.write(data)
    return len(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('outDir', help='directory to write the download into')
    parser.add_argument('--collection', default=MAVEMD_COLLECTION,
                        help='MaveDB collection URN (default: MaveMD)')
    parser.add_argument('--resume', action='store_true',
                        help='keep files already downloaded')
    args = parser.parse_args()

    scoreSetDir = os.path.join(args.outDir, 'scoreSets')
    variantDir = os.path.join(args.outDir, 'variants')
    for d in (args.outDir, scoreSetDir, variantDir):
        os.makedirs(d, exist_ok=True)

    apiVersion = fetchJson('%s/api/version' % API, 60)
    with open(os.path.join(args.outDir, 'apiVersion.txt'), 'w') as fh:
        fh.write('%s %s\n' % (apiVersion['name'], apiVersion['version']))
    sys.stderr.write("MaveDB API %s\n" % apiVersion['version'])

    collection = fetchJson('%s/collections/%s' % (API, quote(args.collection)), 120)
    with open(os.path.join(args.outDir, 'collection.json'), 'w') as fh:
        json.dump(collection, fh, indent=1)
    urns = collection['scoreSetUrns']
    sys.stderr.write("Collection %s: %d score sets, modified %s\n"
                     % (collection['name'], len(urns), collection['modificationDate']))

    # Metadata: two at a time, as asked.
    def getScoreSet(urn):
        path = os.path.join(scoreSetDir, urn.replace(':', '_') + '.json')
        if args.resume and os.path.exists(path) and os.path.getsize(path) > 10:
            return urn, 'cached'
        with open(path, 'wb') as fh:
            fh.write(fetch('%s/score-sets/%s' % (API, quote(urn)), 300))
        return urn, 'ok'

    with concurrent.futures.ThreadPoolExecutor(METADATA_CONCURRENCY) as pool:
        list(pool.map(getScoreSet, urns))
    sys.stderr.write("Score set metadata: %d records\n" % len(urns))

    # Variant data: strictly sequential, as asked.
    started = time.time()
    total = 0
    for i, urn in enumerate(urns, 1):
        path = os.path.join(variantDir, urn.replace(':', '_') + '.csv')
        if args.resume and os.path.exists(path) and os.path.getsize(path) > 100:
            total += os.path.getsize(path)
            continue
        total += fetchVariants(urn, path)
        sys.stderr.write("  %d/%d %s (%.0fs elapsed)\n"
                         % (i, len(urns), urn, time.time() - started))
    sys.stderr.write("Downloaded %.1f MB in %.0f seconds\n"
                     % (total / 1e6, time.time() - started))


if __name__ == '__main__':
    main()
