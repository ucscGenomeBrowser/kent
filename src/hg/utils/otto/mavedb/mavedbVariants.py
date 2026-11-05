#!/usr/bin/env python3
"""Per-variant items for the MaveDB track: one bigBed item per placed single variant per
score set, searchable by name, ClinGen allele and MaveDB variant URN.

A protein-placed variant covers its codon (two blocks for a codon split by an intron); a
variant placed only at genomic level covers the changed bases (zero-length for an
insertion). The item colour is the score set's own blue, silver, red ramp at the variant's
score, the same scale as the heatmap cell.
"""

import collections
import re

import mavedbMaps
import mavedbPlace as mp

RAMP = ((0, 0, 255), (192, 192, 192), (255, 0, 0))     # blue, silver, red as in heatmap.c

MAX_NAME_SEQUENCE = 6

KEYWORDS = {'Phenotypic Assay Method': 'assayMethod',
            'Phenotypic Assay Model System': 'modelSystem',
            'Variant Library Creation Method': 'libraryMethod'}


def rampColor(value, bounds):
    """'r,g,b' for value on the three-stop ramp at bounds (low, mid, high)."""
    lo, mid, hi = bounds
    if value <= mid:
        a, b, f = RAMP[0], RAMP[1], (value - lo) / (mid - lo) if mid > lo else 1.0
    else:
        a, b, f = RAMP[1], RAMP[2], (value - mid) / (hi - mid) if hi > mid else 1.0
    f = min(1.0, max(0.0, f))
    return ','.join(str(int(round(x + (y - x) * f))) for x, y in zip(a, b))


def keywordFields(scoreSet):
    out = {v: '' for v in KEYWORDS.values()}
    for k in (scoreSet.get('experiment') or {}).get('keywords') or []:
        kw = k.get('keyword') or {}
        field = KEYWORDS.get(kw.get('key'))
        if field and kw.get('label'):
            out[field] = kw['label']
    return out


def shortName(pl, gene):
    """GENE:R175H, GENE:R175*, GENE:R175=, GENE:R175del; g. term for genomic-only."""
    if pl.prot:
        alt = 'del' if pl.alt == '-' else pl.alt
        text = '%s%d%s' % (pl.wt, pl.protPos, alt)
        return '%s:%s' % (gene, text) if gene else text
    # The name is a search key, and bigBed indexes keys at the longest name's width, so a
    # long inserted sequence is written as its length: g.123_150delins(28).
    text = pl.genomic.term.split(':g.')[-1]
    m = re.match(r'^(.*?(?:delins|ins|dup|del))([ACGTN]+)$', text)
    if m and len(m.group(2)) > MAX_NAME_SEQUENCE:
        text = '%s(%d)' % (m.group(1), len(m.group(2)))
    return 'g.' + text


def variantItems(scoreSet, placements, ref, gene, mapBounds=None):
    """BED rows for one score set's placed variants, and a skip Counter.

    mapBounds: the colour bounds of the score set's heatmap items (mavedbMaps.mapItems), so
    a variant has the colour of its heatmap cell; a variant in no heatmap uses the bounds of
    all the score set's placed variants.
    """
    urn = scoreSet['urn']
    skipped = collections.Counter()
    mapBounds = mapBounds or {}
    scores = [mavedbMaps.scoreOf(p.row) for p in placements if p.status == 'placed']
    numeric = [s for s in scores if s is not None]
    if not numeric:
        return [], skipped
    lo, hi = min(numeric), max(numeric)
    setBounds = (lo, (lo + hi) / 2.0, hi)
    kw = keywordFields(scoreSet)
    title = mp.bedField(scoreSet.get('title'))
    rows = []
    for pl in placements:
        if pl.status != 'placed':
            continue
        score = mavedbMaps.scoreOf(pl.row)
        if score is None:
            skipped['noScore'] += 1
            continue
        if pl.codon is not None:
            chrom, strand = pl.chrom, pl.strand
            runs = mp.codonRuns(pl.codon)
            start, end = runs[0][0], runs[-1][1]
        elif pl.genomic is not None and not pl.prot:
            chrom, strand = ref.ncToChrom[pl.genomic.acc], '+'
            start, end = pl.genomic.bedRange()
            runs = [(start, end)]
        else:
            skipped['noCoordinates'] += 1
            continue
        bounds = mapBounds.get(mavedbMaps.variantGridKey(pl, ref), setBounds)
        mavedbTerms = pl.label == mp.PLACED_MAVEDB
        row = pl.row
        rowGene = ref.npGene.get(pl.prot) or gene if pl.prot else gene
        rows.append([
            chrom, start, end, shortName(pl, rowGene), 0, strand, start, end,
            rampColor(score, bounds), len(runs),
            ','.join(str(e - s) for s, e in runs) + ',',
            ','.join(str(s - start) for s, _ in runs) + ',',
            urn.split(':')[-1],
            rowGene or '',
            title,
            mp.fmtScore(score),
            ' / '.join(mp.fmtScore(b) for b in bounds),
            mp.bedField(mp.clean(row.get('hgvs_pro')) or mp.clean(row.get('hgvs_nt'))),
            mp.bedField(mp.clean(row.get('mavedb.post_mapped_hgvs_g')) if mavedbTerms else ''),
            mp.bedField(mp.clean(row.get('mavedb.post_mapped_hgvs_c')) if mavedbTerms else ''),
            mp.bedField(mp.clean(row.get('mavedb.post_mapped_hgvs_p')) if mavedbTerms else ''),
            ('%s:%s' % (pl.prot, mp.proteinTermText(pl))) if pl.prot else '',
            pl.label,
            mp.bedField(pl.construct or ''),
            mp.bedField(mp.clean(row.get('clingen.clingen_allele_id'))),
            mp.bedField(mp.clean(row.get('accession'))),
            mp.bedField(kw['assayMethod']),
            mp.bedField(kw['modelSystem']),
            mp.bedField(kw['libraryMethod']),
        ])
    return rows, skipped
