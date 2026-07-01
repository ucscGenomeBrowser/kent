#!/usr/bin/env python3
"""Convert popEVE records (extracted, sorted by protein then position) to heatmap bigBed.

Usage: vcfToPopEveHeatmap.py <sorted_tsv> <strandMap> <output_bed> [loAnchor] [hiAnchor]

Input <sorted_tsv> is the output of extractPopEve.py sorted by:
    sort -t$'\\t' -k1,1 -k4,4n
so every protein's records are contiguous and in ascending genomic order.

<strandMap> is a TSV "protAcc<TAB>strand" (one preferred mapping per NP_ accession,
from hg38 ncbiRefSeq) used to validate / override the strand inferred from coordinates.

Produces one heatmap BED12+ line per protein (see popEve_heatmap.as):
  Columns = protein positions ordered by ascending genomic coordinate; reverse-strand
            genes therefore read C-terminus -> N-terminus, matching genomic orientation.
  Rows    = 20 standard amino acids (A C D E F G H I K L M N P Q R S T V W Y).
  Each codon is a block.  popEVE lists only positions carrying a missense alt, so a codon
  may have 2 of 3 genomic positions; block sizes are therefore clamped so adjacent blocks
  cannot overlap (min(3, gap-to-next)), keeping the file valid for bedToBigBed.
  Wildtype cells are left empty.  Mouseover carries popEVE + severity class plus the
  component EVE / ESM-1v / pop-adjusted scores and gap frequency.
"""

import sys

STANDARD_AAS = list('ACDEFGHIKLMNPQRSTVWY')   # 20 standard AAs, alphabetical

# Published popEVE severity cutoffs (fixed interior color anchors).
SEVERE_MAX = -5.056      # popEVE < SEVERE_MAX            -> severe
MODERATE_MAX = -4.617    # SEVERE_MAX <= popEVE < this    -> moderate; >= this -> tolerated
MEDIAN_ANCHOR = -3.5     # interior white anchor (~ proteome-wide median)

COLOR_VALUES = "#b2182b,#d6604d,#f4a582,#f7f7f7,#2166ac"  # deleterious red -> tolerated blue


def r3(s):
    """Round a numeric string to 3 decimals, trimming trailing zeros."""
    try:
        v = float(s)
    except (ValueError, TypeError):
        return s
    out = ('%.3f' % v).rstrip('0').rstrip('.')
    return '0' if out in ('', '-0') else out


def comp(s):
    """Format a component score for the mouseover: 'NA' for missing/nan, else 3 dp."""
    if s == '' or s == 'nan':
        return 'NA'
    return r3(s)


def classify(score):
    """Severity class for a popEVE score."""
    if score < SEVERE_MAX:
        return "severe"
    if score < MODERATE_MAX:
        return "moderate"
    return "tolerated"


def loadStrandMap(path):
    """Read protAcc<TAB>strand into a dict."""
    d = {}
    with open(path) as fh:
        for line in fh:
            f = line.rstrip('\n').split('\t')
            if len(f) >= 2 and f[1] in ('+', '-'):
                d[f[0]] = f[1]
    return d


def inferStrand(sortedPos):
    """Infer strand from whether protein position decreases as genomic coordinate increases.
    sortedPos is a list of (genomic_start0, prot_pos) sorted by genomic_start0.
    Returns '+', '-', or None (no signal, e.g. single codon)."""
    first = None
    last = None
    for _, pp in sortedPos:
        if first is None:
            first = pp
        last = pp
    if first is None or first == last:
        return None
    return '-' if last < first else '+'


def buildEntry(protein, gene, records, strandMap, loAnchor, hiAnchor, out, stats):
    """Write one heatmap BED12+ line for a protein.

    records: list of (chrom, pos0, wt, protPos, var, popEVE, EVE, ESM1v, popAdjEVE,
                      popAdjESM1v, gapFreq) where pos0 is the 0-based genomic base.
    """
    # Group by protein position: wt aa, codon's genomic base set, and {var: (score, extras)}.
    byPos = {}
    chroms = set()
    for (chrom, pos0, wt, protPos, var, pe, eve, esm, paEve, paEsm, gap) in records:
        chroms.add(chrom)
        d = byPos.get(protPos)
        if d is None:
            d = byPos[protPos] = {'wt': wt, 'bases': set(), 'scores': {}, 'extra': {}}
        d['bases'].add(pos0)
        if var not in d['scores']:        # dedup: identical substitution carries identical score
            d['scores'][var] = pe
            d['extra'][var] = (eve, esm, paEve, paEsm, gap)

    if len(chroms) > 1:
        sys.stderr.write("  WARNING: %s (%s) spans multiple chromosomes %s - skipping\n" %
                         (gene, protein, sorted(chroms)))
        stats['multiChrom'] += 1
        return
    chrom = next(iter(chroms))

    # One column per protein position, ordered by ascending genomic coordinate.
    # Column start = min(codon bases) (0-based); size clamped below.
    cols = sorted(((min(d['bases']), protPos) for protPos, d in byPos.items()))
    colStarts0 = [c[0] for c in cols]
    colProtPos = [c[1] for c in cols]
    nCols = len(cols)

    # Strand: infer from coordinates, then validate / override against ncbiRefSeq.
    inferred = inferStrand(cols)
    refStrand = strandMap.get(protein)
    if refStrand is not None:
        if inferred is not None and inferred != refStrand:
            sys.stderr.write("  WARNING: %s (%s) strand %s != ncbiRefSeq %s; using RefSeq\n"
                             % (gene, protein, inferred, refStrand))
            stats['strandMismatch'] += 1
        strand = refStrand
    elif inferred is not None:
        strand = inferred
    else:
        strand = '+'
        stats['strandDefault'] += 1
        sys.stderr.write("  NOTE: %s (%s) no strand signal and not in ncbiRefSeq - defaulting +\n" %
                         (gene, protein))

    chromStart = colStarts0[0]

    # Clamp block sizes so adjacent blocks cannot overlap (popEVE codons may have only
    # 2 of 3 positions, so naive size-3 blocks would collide -> bedToBigBed aborts).
    blockSizes = []
    for i in range(nCols):
        if i < nCols - 1:
            gap = colStarts0[i + 1] - colStarts0[i]
            blockSizes.append(max(1, min(3, gap)))
        else:
            blockSizes.append(3)            # last block keeps full codon width
    relStarts = [s - chromStart for s in colStarts0]
    chromEnd = colStarts0[-1] + blockSizes[-1]

    # Build assertion: blocks strictly non-overlapping, last ends at chromEnd.
    for i in range(1, nCols):
        if relStarts[i] < relStarts[i - 1] + blockSizes[i - 1]:
            sys.stderr.write("  ERROR: %s (%s) overlapping blocks at col %d\n" % (gene, protein, i))
            stats['overlap'] += 1
            return
    if chromStart + relStarts[-1] + blockSizes[-1] != chromEnd:
        sys.stderr.write("  ERROR: %s (%s) last block does not end at chromEnd\n" % (gene, protein))
        return

    # BED score (0-1000): most-deleterious residue -> high, spread across loAnchor..hiAnchor.
    minPe = min(float(d2) for protPos in byPos for d2 in byPos[protPos]['scores'].values())
    span = hiAnchor - loAnchor
    bedScore = int(round((hiAnchor - minPe) / span * 1000)) if span else 0
    bedScore = max(0, min(1000, bedScore))

    rowCount = len(STANDARD_AAS)
    labels = ','.join(STANDARD_AAS)
    colorBounds = "%s,%s,%s,%s,%s" % (loAnchor, SEVERE_MAX, MODERATE_MAX, MEDIAN_ANCHOR, hiAnchor)

    # Row-major score and label arrays (all columns for row 0, then row 1, ...).
    scoreParts = []
    labelParts = []
    for aa in STANDARD_AAS:
        for protPos in colProtPos:
            d = byPos[protPos]
            wt = d['wt']
            if aa == wt:
                scoreParts.append('')        # wildtype cell -> empty (background)
                labelParts.append('')
            elif aa in d['scores']:
                pe = d['scores'][aa]
                eve, esm, paEve, paEsm, gap = d['extra'][aa]
                scoreParts.append(r3(pe))
                cls = classify(float(pe))
                # Mouseover rendered as HTML (<br>/<b>). No commas inside labels (the field is
                # comma-split by the renderer); values use '.' and '/' only, so labels are unquoted.
                lbl = ("%s%s&rarr;%s<br><b>popEVE:</b> %s (%s)<br><b>EVE:</b> %s<br>"
                       "<b>ESM1v:</b> %s<br><b>popAdj:</b> EVE %s / ESM1v %s<br><b>gap:</b> %s" %
                       (wt, protPos, aa, r3(pe), cls, comp(eve), comp(esm), comp(paEve),
                        comp(paEsm), comp(gap)))
                labelParts.append(lbl)
            else:
                scoreParts.append('')
                labelParts.append('')

    # The heatmap renderer parses the score array with chopCommas (keeps a trailing empty
    # field) but the label array with chopByCharRespectDoubleQuotesKeepEmpty (drops one
    # trailing empty field). When the very last cell (row Y, last column) is empty - common on
    # minus-strand genes whose last column is the start codon, whose M1 substitutions are
    # 'nan' and were skipped - the two field counts disagree and the track aborts. Guarantee a
    # non-empty final label cell; the empty score keeps that cell uncolored (background).
    if labelParts[-1] == '':
        labelParts[-1] = '(no popEVE score)'
        stats['trailingFix'] += 1

    fields = [
        chrom, chromStart, chromEnd,
        gene, bedScore, strand,
        chromStart, chromEnd, 0,
        nCols,
        ','.join(str(s) for s in blockSizes) + ',',
        ','.join(str(s) for s in relStarts) + ',',
        rowCount, labels,
        colorBounds, COLOR_VALUES,
        ','.join(scoreParts), ','.join(labelParts),
        "popEVE: red <= -5.056 severe; orange moderate; white ~ -3.5; blue tolerated",
        protein,
    ]
    out.write('\t'.join(str(f) for f in fields) + '\n')
    stats['ok'] += 1


def main():
    if len(sys.argv) < 4:
        sys.exit("Usage: %s <sorted_tsv> <strandMap> <output_bed> [loAnchor] [hiAnchor]"
                 % sys.argv[0])
    sortedTsv = sys.argv[1]
    strandMapPath = sys.argv[2]
    outputBed = sys.argv[3]
    loAnchor = float(sys.argv[4]) if len(sys.argv) > 4 else -7.0
    hiAnchor = float(sys.argv[5]) if len(sys.argv) > 5 else -2.0

    strandMap = loadStrandMap(strandMapPath)
    sys.stderr.write("Loaded %d NP_->strand mappings.\n" % len(strandMap))

    stats = {'ok': 0, 'multiChrom': 0, 'overlap': 0, 'strandMismatch': 0,
             'strandDefault': 0, 'trailingFix': 0}

    with open(sortedTsv) as fh, open(outputBed, 'w') as out:
        curProt = None
        curGene = None
        recs = []
        for line in fh:
            f = line.rstrip('\n').split('\t')
            if len(f) < 13:
                continue
            (protein, gene, chrom, pos, wt, protPos, var, pe,
             eve, esm, paEve, paEsm, gap) = f[:13]
            if protein != curProt:
                if curProt is not None:
                    buildEntry(curProt, curGene, recs, strandMap, loAnchor, hiAnchor, out, stats)
                curProt = protein
                curGene = gene
                recs = []
            recs.append((chrom, int(pos) - 1, wt, int(protPos), var, pe,
                         eve, esm, paEve, paEsm, gap))
        if curProt is not None:
            buildEntry(curProt, curGene, recs, strandMap, loAnchor, hiAnchor, out, stats)

    sys.stderr.write("Done: %(ok)d proteins written; multiChrom %(multiChrom)d, "
                     "overlap %(overlap)d, strandMismatch %(strandMismatch)d, "
                     "strandDefault %(strandDefault)d, trailingFix %(trailingFix)d\n" % stats)


if __name__ == '__main__':
    main()
