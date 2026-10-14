#!/usr/bin/env python3
"""Build the MaveDB heatmap and variant bigBeds from a catalogue and cached variant CSVs.

usage: mavedbBuild.py catalogue.json variantsDir outDir [--scoreSets dir]

catalogue.json is the list of score set records from the MaveDB search (mavedbFetch.py);
variantsDir holds <urn>.csv per score set; --scoreSets an optional directory of full score
set records (<urn>.json) for the score set method text. Writes into outDir:
    mavedbMaps.bb, mavedbVariants.bb, build.tsv (one row per score set), report.txt
Prints the report. Raises on a systematic projection error (mavedbPlace.crossCheckGate).
build() returns the report and a dict of the counts mavedbOtto.py checks.
"""

import argparse
import collections
import csv
import json
import os
import subprocess
import sys
import time

import mavedbAlign
import mavedbMaps
import mavedbPlace as mp
import mavedbVariants

csv.field_size_limit(10 ** 9)

DB = 'hg38'
TWOBIT = '/hive/data/genomes/hg38/hg38.2bit'
CHROM_SIZES = '/hive/data/genomes/hg38/chrom.sizes'
OOC = '/hive/data/genomes/hg38/hg38.11.ooc'
HERE = os.path.dirname(os.path.abspath(__file__))

# D5: a score set with fewer of its single variants placed is left out of every track.
MIN_PLACED_FRACTION = 0.20

# Published score sets left out by name, with the reason. Asking MaveDB to retract them is
# on the author question list (PLAN.md §12).
EXCLUDED_SCORE_SETS = {
    'urn:mavedb:00000094-a-1': 'test upload "try_dataset_KCNQ4": 1 variant, no publication',
}

# D9: Tsuboyama per-protease score sets go in the variant track only; the heatmap shows the
# combined score set of the domain.
PROTEASE_TITLE_ENDINGS = (' trypsin digestion', ' chymotrypsin digestion')


# Columns of the variants/data CSV the build reads. If MaveDB renames one, every row would
# quietly become unplaced, so the build stops instead.
REQUIRED_COLUMNS = ('accession', 'hgvs_nt', 'hgvs_pro', 'scores.score',
                    'mavedb.post_mapped_hgvs_g', 'mavedb.post_mapped_hgvs_c',
                    'mavedb.post_mapped_hgvs_p', 'clingen.clingen_allele_id')


def readRows(path):
    with open(path, newline='') as fh:
        reader = csv.DictReader(fh)
        missing = [c for c in REQUIRED_COLUMNS if c not in (reader.fieldnames or [])]
        if missing:
            raise ValueError('%s: missing column(s) %s; did the MaveDB CSV format change?'
                             % (path, ', '.join(missing)))
        return list(reader)


def scoreSetFile(variantsDir, urn):
    return os.path.join(variantsDir, urn.split(':')[-1] + '.csv')


def loadScoreSetRecord(scoreSetDir, urn):
    if not scoreSetDir:
        return None
    path = os.path.join(scoreSetDir, urn.split(':')[-1] + '.json')
    if not os.path.exists(path):
        return None
    with open(path) as fh:
        return json.load(fh)


def writeBigBed(rows, asFile, bedType, outBb, workDir, extraIndex=None):
    """Sort, write and convert; returns the item count."""
    bed = outBb[:-3] + '.bed'
    rows.sort(key=lambda r: (r[0], r[1], r[2], r[3]))
    with open(bed, 'w') as fh:
        for r in rows:
            fh.write('\t'.join(str(x) for x in r) + '\n')
    cmd = ['bedToBigBed', '-tab', '-type=' + bedType, '-as=' + asFile]
    if extraIndex:
        cmd.append('-extraIndex=' + extraIndex)
    cmd += [bed, CHROM_SIZES, outBb + '.tmp']
    subprocess.run(cmd, check=True, cwd=workDir)
    os.rename(outBb + '.tmp', outBb)
    return len(rows)


def bigPslAs():
    """bigPsl.as: a copy next to the scripts when installed, else the kent tree's."""
    here = os.path.join(HERE, 'bigPsl.as')
    if os.path.exists(here):
        return here
    return os.path.normpath(os.path.join(HERE, '..', '..', '..', 'lib', 'bigPsl.as'))


def alignmentEntries(scoreSet, placements, choices, ref):
    """(scoreSet, target, proteins, loci) for each target of a shown score set.

    proteins: the one the target's variants were placed on (its offset choice, else the
    most common placed protein) first, then the gene's other RefSeq proteins, MANE first.
    A construct can be another isoform than the one its variants were placed on (KRAS4B
    targets placed on KRAS4A); buildAlignments() keeps only a protein whose codons lie at
    the placed loci. loci are the spans of the placed items per chromosome, the anchor for
    BLAT hits and for the other proteins.
    """
    targets = scoreSet.get('targetGenes') or []
    entries = []
    for t in targets:
        prefix = None if len(targets) == 1 else t.get('name')
        mine = [p for p in placements if p.status == 'placed'
                and (prefix is None or p.prefix == prefix)]
        choice = (choices.get(prefix) or (None, None))[0]
        prot = choice.prot if choice else None
        if prot is None:
            counts = collections.Counter(p.prot for p in mine if p.prot)
            prot = counts.most_common(1)[0][0] if counts else None
        spans = {}
        for p in mine:
            if p.codon is not None:
                chrom, s, e = p.chrom, min(p.codon), max(p.codon) + 1
            elif p.genomic is not None:
                chrom = ref.ncToChrom.get(p.genomic.acc)
                s, e = p.genomic.bedRange()
                e = max(e, s + 1)
            else:
                continue
            lo, hi = spans.get(chrom, (s, e))
            spans[chrom] = (min(lo, s), max(hi, e))
        loci = [(c, lo, hi) for c, (lo, hi) in spans.items()]
        gene, _ = mp.targetGene(t, ref)
        prots = [prot] if prot else []
        prots += [p for p in (ref.proteinsOfGene(gene) if gene else []) if p not in prots]
        entries.append((scoreSet, t, prots, loci))
    return entries


def build(catalogue, variantsDir, outDir, scoreSetDir=None, log=sys.stderr):
    t0 = time.time()
    workDir = os.path.join(outDir, 'work')
    os.makedirs(workDir, exist_ok=True)
    ref = mp.Reference(DB).load()
    ref.loadUniprotForCatalogue(catalogue)
    excluded = sorted(s['urn'] for s in catalogue if s['urn'] in EXCLUDED_SCORE_SETS)
    catalogue = [s for s in catalogue if s['urn'] not in EXCLUDED_SCORE_SETS]
    scoreSets = [s for s in sorted(catalogue, key=lambda s: s['urn'])
                 if os.path.exists(scoreSetFile(variantsDir, s['urn']))]
    missing = [s['urn'] for s in catalogue
               if not os.path.exists(scoreSetFile(variantsDir, s['urn']))]
    log.write('%d score sets with variants, %d without (%.0fs)\n'
              % (len(scoreSets), len(missing), time.time() - t0))

    # Pass 1: proteins to load codon maps for, so they are read once.
    prots = set()
    for s in scoreSets:
        pls, _, _ = mp.placeScoreSet(s, readRows(scoreSetFile(variantsDir, s['urn'])), ref)
        prots.update(p.prot for p in pls if p.status == 'placed' and p.prot)
        prots.update(p.mavedbProt for p in pls if p.mavedbProt)
    protToMap, unresolved = mp.loadMapsForProteins(ref, prots, TWOBIT, workDir)
    log.write('codon maps for %d proteins, %d unresolved (%.0fs)\n'
              % (len(protToMap), len(unresolved), time.time() - t0))

    mapRows, varRows, perSet, alignEntries = [], [], [], []
    gstats = collections.Counter()
    labels = collections.Counter()
    reasons = collections.Counter()
    skips = collections.Counter()
    tot = collections.Counter()
    for s in scoreSets:
        urn = s['urn']
        rows = readRows(scoreSetFile(variantsDir, urn))
        pls, choices, _ = mp.placeScoreSet(s, rows, ref)
        gstats.update('referenceTerm:' + k
                      for k in mp.placeReferenceTerms(pls, ref, workDir).elements())
        gstats.update(mp.checkGenome(pls, ref, TWOBIT, workDir, protToMap))
        single = sum(1 for p in pls if p.reason != 'multiVariant')
        placed = [p for p in pls if p.status == 'placed']
        frac = len(placed) / single if single else 0.0
        target = (s.get('targetGenes') or [{}])[0]
        gene, _ = mp.targetGene(target, ref)
        shown = frac >= MIN_PLACED_FRACTION
        nMaps = nVars = 0
        if shown:
            mapBounds = {}
            if not (s.get('title') or '').endswith(PROTEASE_TITLE_ENDINGS):
                mrows, mskip, mapBounds = mavedbMaps.mapItems(
                    s, pls, ref, gene, loadScoreSetRecord(scoreSetDir, urn))
                mapRows.extend(mrows)
                skips.update('map:' + k for k in mskip.elements())
                nMaps = len(mrows)
            vrows, vskip = mavedbVariants.variantItems(s, pls, ref, gene, mapBounds)
            varRows.extend(vrows)
            skips.update('variant:' + k for k in vskip.elements())
            nVars = len(vrows)
            labels.update(p.label for p in placed)
            # Only score sets with a heatmap get alignments: a per-protease score set would
            # draw its domain's alignment again beside the combined score set's.
            if nMaps:
                alignEntries.extend(alignmentEntries(s, pls, choices, ref))
        for p in pls:
            if p.status != 'placed':
                reasons[p.reason] += 1
        tot['single'] += single
        tot['placed'] += len(placed)
        tot['shownSets' if shown else 'gatedSets'] += 1
        perSet.append([urn, single, len(placed), '%.4f' % frac, 'yes' if shown else 'no',
                       nMaps, nVars,
                       ';'.join('%s:%s%+d:%s' % (k or '', c.prot, c.offset, c.kind)
                                if c else '%s:none:%s' % (k or '', why)
                                for k, (c, why) in sorted(choices.items(),
                                                          key=lambda kv: str(kv[0]))),
                       mp.bedField(s.get('title'))])
    mp.crossCheckGate(gstats)
    extra = {p for e in alignEntries for p in e[2]} - set(protToMap)
    if extra:
        more, _ = mp.loadMapsForProteins(ref, extra, TWOBIT, workDir)
        protToMap.update(more)

    nMap = writeBigBed(mapRows, os.path.join(HERE, 'mavedbMaps.as'), 'bed12+16',
                       os.path.join(outDir, 'mavedbMaps.bb'), workDir)
    nVar = writeBigBed(varRows, os.path.join(HERE, 'mavedbVariants.as'), 'bed12+17',
                       os.path.join(outDir, 'mavedbVariants.bb'), workDir,
                       extraIndex='name,clinGenId,variantUrn')
    alignStats = mavedbAlign.buildAlignments(alignEntries, protToMap, TWOBIT, OOC, CHROM_SIZES,
                                             outDir, workDir, bigPslAs())
    with open(os.path.join(outDir, 'build.tsv'), 'w') as fh:
        fh.write('urn\tsingle\tplaced\tfracPlaced\tshown\tmaps\tvariants\toffsets\ttitle\n')
        for r in perSet:
            fh.write('\t'.join(str(x) for x in r) + '\n')

    lines = ['score sets: %d with variants (%d shown, %d under %d%% placed), %d not downloaded'
             % (len(scoreSets), tot['shownSets'], tot['gatedSets'],
                100 * MIN_PLACED_FRACTION, len(missing)),
             'single variants: %d, placed %d (%.1f%%)'
             % (tot['single'], tot['placed'], 100.0 * tot['placed'] / max(1, tot['single'])),
             'heatmap items: %d; variant items: %d' % (nMap, nVar),
             'alignments: ' + ', '.join('%s %d' % kv for kv in sorted(alignStats.items())),
             'placed by: ' + ', '.join('%s %d' % kv for kv in labels.most_common()),
             'not placed: ' + ', '.join('%s %d' % kv for kv in reasons.most_common()
                                        if kv[0] != 'multiVariant'),
             'multi-variant rows (not used): %d' % reasons['multiVariant'],
             'genome checks: ' + ', '.join('%s %d' % kv for kv in sorted(gstats.items())),
             'skipped while building items: ' + ', '.join(
                 '%s %d' % kv for kv in sorted(skips.items())),
             'not downloaded: ' + ' '.join(missing),
             'excluded by name: ' + ' '.join('%s (%s)' % (u, EXCLUDED_SCORE_SETS[u])
                                             for u in excluded),
             'build time %.0fs' % (time.time() - t0)]
    report = '\n'.join(lines) + '\n'
    with open(os.path.join(outDir, 'report.txt'), 'w') as fh:
        fh.write(report)
    stats = {'shownSets': tot['shownSets'], 'gatedSets': tot['gatedSets'],
             'single': tot['single'], 'placed': tot['placed'],
             'placedByMavedb': labels[mp.PLACED_MAVEDB], 'mapItems': nMap,
             'variantItems': nVar, 'badItems': skips['map:badItem'],
             'notDownloaded': missing, 'excluded': excluded}
    return report, stats


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('catalogue')
    ap.add_argument('variantsDir')
    ap.add_argument('outDir')
    ap.add_argument('--scoreSets', default=None)
    args = ap.parse_args()
    with open(args.catalogue) as fh:
        catalogue = json.load(fh)
    # Absolute: the converters run with the work directory as their current directory.
    outDir = os.path.abspath(args.outDir)
    os.makedirs(outDir, exist_ok=True)
    report, _ = build(catalogue, os.path.abspath(args.variantsDir), outDir,
                      args.scoreSets and os.path.abspath(args.scoreSets))
    sys.stdout.write(report)


if __name__ == '__main__':
    main()
