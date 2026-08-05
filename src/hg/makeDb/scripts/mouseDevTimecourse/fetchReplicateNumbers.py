#!/usr/bin/env python3
"""
Look up the ENCODE biological replicate number for each bigWig in the mouse
developmental time course and write it to a TSV that
generateBigwigTrackDb.py reads.

Diane's ENCSR574CRQ_biosample.tsv (#36998 attachment) has no replicate
column, so the replicate number has to come from the ENCODE portal. Without
this step generateBigwigTrackDb.py has to guess, and guessing by biosample
accession order mislabels 124 of the 312 subtracks (#37001).

Reads the file accessions out of the biosample TSV, queries the portal
search endpoint in batches, and writes accession<TAB>replicate to
ENCSR574CRQ_replicates.tsv in the same directory.

Output goes to stdout. Redirect to a file.
"""

import json
import sys
import time
import urllib.request

DEFAULT_TSV = '/hive/data/outside/woldlab/mouseDevTimecourse/mm10/ENCSR574CRQ_biosample.tsv'

SEARCH = ('https://www.encodeproject.org/search/?type=File&limit=all&format=json'
          '&field=accession&field=biological_replicates&field=status')
BATCH = 40


def accessions(tsv_path):
    """File accessions from the signal_of_unique_reads and signal_of_all_reads URLs."""
    accs = []
    with open(tsv_path) as f:
        header = f.readline().rstrip('\n').split('\t')
        for line in f:
            line = line.rstrip('\n')
            if not line:
                continue
            row = dict(zip(header, line.split('\t')))
            for col in ('signal_of_unique_reads', 'signal_of_all_reads'):
                accs.append(row[col].rsplit('/', 1)[-1].replace('.bigWig', ''))
    return accs


def fetch(accs):
    """accession -> biological replicate number, from the ENCODE portal."""
    reps = {}
    for i in range(0, len(accs), BATCH):
        group = accs[i:i + BATCH]
        url = SEARCH + ''.join('&accession=' + a for a in group)
        for attempt in range(4):
            try:
                req = urllib.request.Request(url, headers={'Accept': 'application/json'})
                results = json.load(urllib.request.urlopen(req, timeout=90))
                break
            except Exception as e:
                sys.stderr.write('retry %d for batch at %d: %s\n' % (attempt, i, e))
                time.sleep(3)
        else:
            sys.exit('ENCODE portal query failed for batch starting at %d' % i)

        for f in results.get('@graph', []):
            if f['status'] != 'released':
                sys.stderr.write('warning: %s status is %s, not released\n'
                                 % (f['accession'], f['status']))
            bio = f['biological_replicates']
            if len(bio) != 1:
                sys.exit('%s covers %d biological replicates; expected exactly one'
                         % (f['accession'], len(bio)))
            reps[f['accession']] = bio[0]
        sys.stderr.write('fetched %d of %d\n' % (len(reps), len(accs)))

    missing = [a for a in accs if a not in reps]
    if missing:
        sys.exit('no replicate number returned for: %s' % ' '.join(missing))
    return reps


def main():
    tsv_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_TSV
    accs = accessions(tsv_path)
    reps = fetch(accs)
    print('accession\treplicate')
    for acc in accs:
        print('%s\t%d' % (acc, reps[acc]))


if __name__ == '__main__':
    main()
