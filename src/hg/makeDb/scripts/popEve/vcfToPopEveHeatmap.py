#!/usr/bin/env python3
"""Convert popEVE records (extracted, sorted by protein then position) to heatmap bigBed.

Usage: vcfToPopEveHeatmap.py <sorted_tsv> <strandMap> <output_bed> <loAnchor> <hiAnchor> [csvDir]

Input <sorted_tsv> is the output of extractPopEve.py sorted by:
    sort -t$'\\t' -k1,1 -k4,4n
so every protein's records are contiguous and in ascending genomic order.  This supplies
the genomic codon coordinates, strand signal, wildtype residue, and gene symbol.

<strandMap> is a TSV "protAcc<TAB>strand" (one preferred mapping per NP_ accession,
from hg38 ncbiRefSeq) used to validate / override the strand inferred from coordinates.

<csvDir> (optional) is the directory of per-transcript popEVE score CSVs (one <NP_acc>.csv
per protein, columns: mutant,gap frequency,popEVE,popped EVE,popped ESM-1v,EVE,ESM-1v).
When given, each protein's full per-amino-acid matrix (all 19 substitutions per position) is
taken from its CSV, producing a dense heatmap.  Proteins with no CSV fall back to the
single-nucleotide-reachable substitutions from <sorted_tsv> (sparse).  The codon genomic
coordinates always come from <sorted_tsv>.

Produces one heatmap BED12+ line per protein (see popEve_heatmap.as):
  Columns = protein positions ordered by ascending genomic coordinate; reverse-strand
            genes therefore read C-terminus -> N-terminus, matching genomic orientation.
  Rows    = 20 standard amino acids ordered by class (A V L I M F Y W R H K D E S T N Q G C P).
  Each codon is a block; block sizes are clamped so adjacent blocks cannot overlap (a codon
  may have only 2 of its 3 genomic positions represented), keeping the file valid for
  bedToBigBed.  Wildtype cells are left empty.  Mouseover carries popEVE + severity class
  plus the component EVE / ESM-1v / pop-adjusted scores and gap frequency.
"""

import sys
import os

STANDARD_AAS = list('AVLIMFYWRHKDESTNQGCP')   # 20 standard AAs, by class, to match MaveDB
STANDARD_SET = set(STANDARD_AAS)

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


def loadCsv(csvDir, protein):
    """Load a protein's full per-amino-acid popEVE matrix from its CSV.

    Returns {protPos: {'wt': wt, 'vars': {var: (popEVE, EVE, ESM1v, popAdjEVE, popAdjESM1v,
    gap)}}} or None if the CSV is absent.  CSV columns:
    mutant,gap frequency,popEVE,popped EVE,popped ESM-1v,EVE,ESM-1v.
    """
    if csvDir is None:
        return None
    path = os.path.join(csvDir, protein + '.csv')
    if not os.path.exists(path):
        return None
    data = {}
    with open(path) as fh:
        fh.readline()   # header
        for line in fh:
            f = line.rstrip('\n').split(',')
            if len(f) < 7:
                continue
            m = f[0]                                 # e.g. G1042A
            wt, var = m[0], m[-1]
            if wt not in STANDARD_SET or var not in STANDARD_SET:
                continue
            try:
                pos = int(m[1:-1])
            except ValueError:
                continue
            gap, pe, paEve, paEsm, eve, esm = f[1], f[2], f[3], f[4], f[5], f[6]
            d = data.get(pos)
            if d is None:
                d = data[pos] = {'wt': wt, 'vars': {}}
            d['vars'][var] = (pe, eve, esm, paEve, paEsm, gap)
    return data


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


def buildEntry(protein, gene, records, csvData, strandMap, loAnchor, hiAnchor, out, stats):
    """Write one heatmap BED12+ line for a protein.

    records: list of (chrom, pos0, wt, protPos, var, popEVE, EVE, ESM1v, popAdjEVE,
                      popAdjESM1v, gapFreq) where pos0 is the 0-based genomic base.
    csvData: the protein's full matrix from loadCsv(), or None to use only the sparse
             single-nucleotide-reachable substitutions in records.
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

    # Densify: where the full per-amino-acid CSV covers a position, replace the sparse
    # single-nucleotide-reachable scores with the complete set of 19 substitutions.  Codon
    # coordinates (bases) and the wildtype residue stay from the genomic records.
    if csvData is not None:
        stats['dense'] += 1
        for protPos, d in byPos.items():
            cd = csvData.get(protPos)
            if cd is None:
                continue
            if cd['wt'] != d['wt']:
                stats['wtMismatch'] += 1
            d['scores'] = {var: v[0] for var, v in cd['vars'].items()}
            d['extra'] = {var: v[1:] for var, v in cd['vars'].items()}
    else:
        stats['sparse'] += 1

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

    # Clamp block sizes so adjacent blocks cannot overlap (a codon may have only 2 of 3
    # genomic positions, so naive size-3 blocks would collide -> bedToBigBed aborts).
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
                # "EVE index" is the raw EVE evolutionary index (not the 0-1 score in the EVE track).
                lbl = ("%s%s&rarr;%s<br><b>popEVE:</b> %s (%s)<br><b>EVE index:</b> %s<br>"
                       "<b>ESM1v:</b> %s<br><b>popAdj:</b> EVE %s / ESM1v %s<br><b>gap:</b> %s" %
                       (wt, protPos, aa, r3(pe), cls, comp(eve), comp(esm), comp(paEve),
                        comp(paEsm), comp(gap)))
                labelParts.append(lbl)
            else:
                scoreParts.append('')
                labelParts.append('')

    # The heatmap renderer parses the score array with chopCommas (keeps a trailing empty
    # field) but the label array with chopByCharRespectDoubleQuotesKeepEmpty (drops one
    # trailing empty field). When the very last cell (last row, last column) has no scored
    # substitution the two field counts disagree and the track aborts. Guarantee a non-empty
    # final label cell; the empty score keeps that cell uncolored (background).
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
    if len(sys.argv) < 6:
        sys.exit("Usage: %s <sorted_tsv> <strandMap> <output_bed> <loAnchor> <hiAnchor> [csvDir]"
                 % sys.argv[0])
    sortedTsv = sys.argv[1]
    strandMapPath = sys.argv[2]
    outputBed = sys.argv[3]
    loAnchor = float(sys.argv[4])
    hiAnchor = float(sys.argv[5])
    csvDir = sys.argv[6] if len(sys.argv) > 6 else None

    strandMap = loadStrandMap(strandMapPath)
    sys.stderr.write("Loaded %d NP_->strand mappings.\n" % len(strandMap))
    if csvDir:
        sys.stderr.write("Dense mode: full per-amino-acid matrices from %s\n" % csvDir)

    stats = {'ok': 0, 'multiChrom': 0, 'overlap': 0, 'strandMismatch': 0,
             'strandDefault': 0, 'trailingFix': 0, 'dense': 0, 'sparse': 0, 'wtMismatch': 0}

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
                    buildEntry(curProt, curGene, recs, loadCsv(csvDir, curProt),
                               strandMap, loAnchor, hiAnchor, out, stats)
                curProt = protein
                curGene = gene
                recs = []
            recs.append((chrom, int(pos) - 1, wt, int(protPos), var, pe,
                         eve, esm, paEve, paEsm, gap))
        if curProt is not None:
            buildEntry(curProt, curGene, recs, loadCsv(csvDir, curProt),
                       strandMap, loAnchor, hiAnchor, out, stats)

    sys.stderr.write("Done: %(ok)d proteins written; dense %(dense)d, sparse %(sparse)d, "
                     "multiChrom %(multiChrom)d, overlap %(overlap)d, "
                     "strandMismatch %(strandMismatch)d, strandDefault %(strandDefault)d, "
                     "wtMismatch %(wtMismatch)d, trailingFix %(trailingFix)d\n" % stats)


if __name__ == '__main__':
    main()
