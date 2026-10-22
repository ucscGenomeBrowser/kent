#!/usr/bin/env python3
"""Heatmap items for the MaveDB track: one per score set and target, drawn by hgTracks'
"style heatmap" bigBed code (hg/hgTracks/heatmap.c).

Layout follows Jonathan Casper's 2025 build (makeDb/doc/hg38/maveDB.txt, parseMave.pl):
rows A,V,L,I,M,F,Y,W,R,H,K,D,E,S,T,N,Q,G,C,P,-,*,= ; one column per codon block in genome
order, a codon split by an intron giving two columns with the same values; the gold
"Wild type" cell in the wild-type residue's row; synonymous scores in the "=" row; colour
bounds min, (min+max)/2, max of the item's scores, blue to silver to red; duplicate
measurements of one cell coloured by the first, counted after "|", listed in the mouseover.

Score sets with no protein placement but genomic substitutions get a nucleotide map instead:
rows A,C,G,T, one column per measured base, a gold "Reference" cell.

Changes from the 2025 build: the gold wild-type cell is drawn for every measured codon, not
only where a synonymous variant exists (the wild-type residue is now always known); the
duplicate count is "+" for 10 or more, because heatmap.c shows one character; colour bounds
are computed from numbers; the item has its real strand (heatmap.c draws the same either
way); variants we placed carry " (UCSC placement)" in their mouseover.
"""

import collections
import re

import mavedbPlace as mp

PROTEIN_ROWS = list('AVLIMFYWRHKDESTNQGCP') + ['-', '*', '=']
NUCLEOTIDE_ROWS = list('ACGT')
WILD_TYPE_COLOR = '#FFD700'
COLORS = ('blue', 'silver', 'red')

# heatmap.c splits the score and label fields on commas, and the label field respects
# double quotes, so neither character may appear inside a cell.
CELL_UNSAFE = re.compile(r'[,"\t\n\r]')


def variantNumber(row):
    """'#399' from 'urn:mavedb:00000060-a-1#399'."""
    acc = row.get('accession') or ''
    return acc[acc.index('#'):] if '#' in acc else acc


def scoreOf(row):
    """The row's score as a float, or None."""
    try:
        value = float(row.get('scores.score'))
    except (TypeError, ValueError):
        return None
    return None if value != value else value     # NaN


def cellLabel(text):
    return CELL_UNSAFE.sub(' ', text)


class Grid(object):
    """Cells of one heatmap: columns are genomic blocks, rows a fixed label list."""

    def __init__(self, rows):
        self.rows = rows
        self.rowIndex = {r: i for i, r in enumerate(rows)}
        self.columns = {}           # (start0, end) -> column key (shared by split codons)
        self.keyRuns = {}           # column key -> its blocks
        self.cells = collections.defaultdict(list)    # (columnKey, row) -> [(score, label)]
        self.fixed = {}             # (columnKey, row) -> (color, label)
        self.scores = []

    def addColumn(self, runs, key):
        """Register a codon's (or base's) blocks; False if one overlaps another column."""
        if key in self.keyRuns:
            return self.keyRuns[key] == runs
        for run in runs:
            have = self.columns.get(run)
            if have is not None and have != key:
                return False
        for s, e in runs:
            for (s2, e2), k2 in self.columns.items():
                if k2 != key and s < e2 and s2 < e:
                    return False
        for run in runs:
            self.columns[run] = key
        self.keyRuns[key] = runs
        return True

    def add(self, key, row, score, label):
        self.cells[(key, row)].append((score, label))
        self.scores.append(score)

    def fix(self, key, row, color, label):
        self.fixed[(key, row)] = (color, label)


def renderGrid(grid):
    """(blocks, scoreArray, labelArray, bounds) for heatmap.c, or None if not drawable."""
    if not grid.scores:
        return None
    lo, hi = min(grid.scores), max(grid.scores)
    if lo == hi:
        return None
    blocks = sorted(grid.columns)
    scoreCells, labelCells = [], []
    for row in grid.rows:
        for block in blocks:
            key = grid.columns[block]
            values = grid.cells.get((key, row))
            if values:
                first = values[0][0]
                text = '%.4f' % first
                if len(values) > 1:
                    n = len(values)
                    text += '|%s' % (n if n < 10 else '+')
                scoreCells.append(text)
                labelCells.append('<br>'.join(cellLabel(lab) for _, lab in values))
            elif (key, row) in grid.fixed:
                color, lab = grid.fixed[(key, row)]
                scoreCells.append(color)
                labelCells.append(cellLabel(lab))
            else:
                scoreCells.append('')
                labelCells.append('')
    bounds = (lo, (lo + hi) / 2.0, hi)
    return blocks, scoreCells, labelCells, bounds


def proteinGrids(placements):
    """Protein heatmap grids for one score set, keyed by (target prefix, chrom).

    Returns (grids, skipped Counter). Only protein-placed rows with a numeric score.
    """
    grids = {}
    skipped = collections.Counter()
    for pl in placements:
        if pl.status != 'placed' or pl.codon is None:
            continue
        score = scoreOf(pl.row)
        if score is None:
            skipped['noScore'] += 1
            continue
        gkey = (pl.prefix, pl.chrom)
        grid = grids.get(gkey)
        if grid is None:
            grid = grids[gkey] = Grid(PROTEIN_ROWS)
            grid.strand = pl.strand
            grid.prots = collections.Counter()
            grid.placedByUcsc = 0
            grid.constructs = set()
        key = (pl.prot, pl.protPos)
        if not grid.addColumn(mp.codonRuns(pl.codon), key):
            skipped['overlappingColumn'] += 1
            continue
        grid.prots[pl.prot] += 1
        row = pl.alt if pl.alt in grid.rowIndex else None
        if row is None:
            skipped['row:' + pl.alt] += 1
            continue
        label = '%s %s: %s' % (variantNumber(pl.row), mp.proteinTermText(pl), '%.4f' % score)
        if pl.label != mp.PLACED_MAVEDB:
            label += ' (UCSC placement)'
            grid.placedByUcsc += 1
        grid.add(key, row, score, label)
        if pl.wt in grid.rowIndex:
            grid.fix(key, pl.wt, WILD_TYPE_COLOR, 'Wild type')
        if pl.construct:
            grid.constructs.add(pl.construct)
    return grids, skipped


def nucleotideGrids(placements, ref):
    """Nucleotide heatmap grids for a score set without protein placements, by chrom.

    Only genomic substitutions; the reference base comes from the term itself, which
    checkGenome() has already checked against the genome.
    """
    grids = {}
    skipped = collections.Counter()
    for pl in placements:
        if pl.status != 'placed' or pl.genomic is None or pl.prot:
            continue
        g = pl.genomic
        m = mp.GENOMIC_SNV.match(g.rest)
        if not m:
            skipped['notSubstitution'] += 1
            continue
        score = scoreOf(pl.row)
        if score is None:
            skipped['noScore'] += 1
            continue
        chrom = ref.ncToChrom[g.acc]
        grid = grids.get(chrom)
        if grid is None:
            grid = grids[chrom] = Grid(NUCLEOTIDE_ROWS)
            grid.strand = '+'
            grid.prots = collections.Counter()
            grid.placedByUcsc = 0
            grid.constructs = set()
        run = (g.start - 1, g.start)
        grid.addColumn([run], run)
        label = '%s g.%d%s>%s: %s' % (variantNumber(pl.row), g.start, m.group('ref'),
                                      m.group('alt'), '%.4f' % score)
        if pl.label != mp.PLACED_MAVEDB:
            label += ' (UCSC placement)'
            grid.placedByUcsc += 1
        grid.add(run, m.group('alt'), score, label)
        grid.fix(run, m.group('ref'), WILD_TYPE_COLOR, 'Reference')
    return grids, skipped


def itemName(urn, prefix, chrom, nKeys):
    """Bare URN for the usual single item; URN_target or URN_chrom when a score set has
    several items."""
    name = urn.split(':')[-1]
    if nKeys == 1:
        return name
    tag = prefix if prefix else chrom
    return '%s_%s' % (name, re.sub(r'[^A-Za-z0-9.-]+', '_', tag).strip('_'))


def htmlText(value):
    """Free text for the details page: HTML-escaped, whitespace flattened."""
    import html
    text = '' if value is None else str(value)
    text = re.sub(r'\\r\\n|\\n|[\r\n]+', '<br>', html.escape(text, quote=True))
    return mp.bedField(text)


def scoreSetMethodText(scoreSet, scoreSetRecord=None):
    """Score set methods, then the experiment's, as Jonathan's build did."""
    parts = []
    if scoreSetRecord and scoreSetRecord.get('methodText'):
        parts.append(htmlText(scoreSetRecord['methodText']))
    exp = scoreSet.get('experiment') or {}
    if exp.get('methodText'):
        parts.append(htmlText(exp['methodText']))
    return '<br><br>'.join(parts)


def mapItems(scoreSet, placements, ref, gene, scoreSetRecord=None):
    """BED rows (lists of fields) for one score set's heatmaps, a skip Counter, and the
    colour bounds of each item keyed as variantGridKey() keys a variant.

    An item heatmap.c could not draw (checkMapRow) is left out and counted, not fatal.
    """
    urn = scoreSet['urn']
    grids, skipped = proteinGrids(placements)
    mapType = 'protein'
    if not grids:
        grids, skipped2 = nucleotideGrids(placements, ref)
        skipped.update(skipped2)
        mapType = 'nucleotide'
    rows = []
    boundsByKey = {}
    for (gkey, grid) in sorted(grids.items(), key=lambda kv: str(kv[0])):
        rendered = renderGrid(grid)
        if rendered is None:
            skipped['notDrawable'] += 1
            continue
        blocks, scoreCells, labelCells, bounds = rendered
        prefix, chrom = gkey if mapType == 'protein' else (None, gkey)
        start, end = blocks[0][0], max(e for _, e in blocks)
        exp = scoreSet.get('experiment') or {}
        row = [chrom, start, end, itemName(urn, prefix, chrom, len(grids)), 1000, grid.strand,
               start, end, 0, len(blocks),
               ','.join(str(e - s) for s, e in blocks) + ',',
               ','.join(str(s - start) for s, _ in blocks) + ',',
               len(grid.rows), ','.join(grid.rows),
               ','.join(repr(b) for b in bounds) + ',',
               ','.join(COLORS) + ',',
               ','.join(scoreCells) + ',',
               ','.join(labelCells) + ',',
               mp.bedField(mp.asciiText(scoreSet.get('shortDescription')
                                        or scoreSet.get('title') or '')),
               '<a href="https://mavedb.org/score-sets/%s" target=_blank>%s</a>'
               % (urn, urn.split(':')[-1]),
               htmlText(exp.get('abstractText')),
               scoreSetMethodText(scoreSet, scoreSetRecord),
               (ref.npGene.get(grid.prots.most_common(1)[0][0]) if grid.prots else None) or gene or '',
               mp.bedField(scoreSet.get('title')),
               mapType,
               len(grid.scores),
               grid.placedByUcsc,
               mp.bedField('; '.join(sorted(grid.constructs)))]
        problem = checkMapRow(row)
        if problem:
            skipped['badItem'] += 1
            skipped['badItem:' + problem.split(':')[0]] += 1
            continue
        rows.append(row)
        boundsByKey[(mapType, gkey)] = bounds
    return rows, skipped, boundsByKey


def variantGridKey(pl, ref):
    """The heatmap item a placed variant is drawn in, as mapItems() keys its bounds."""
    if pl.codon is not None:
        return ('protein', (pl.prefix, pl.chrom))
    if pl.genomic is not None:
        return ('nucleotide', ref.ncToChrom.get(pl.genomic.acc))
    return None


def checkMapRow(row):
    """What heatmap.c would choke on, or None: wrong cell counts, a bad literal colour
    (an invalid one makes hgTracks dereference NULL), unsorted bounds."""
    nCols, nRows = row[9], row[12]
    # One trailing comma closes the list (heatmap.c needs it when the last label is
    # empty); every other comma separates cells, empty ones included.
    scores = row[16][:-1].split(',')
    labels = row[17][:-1].split(',')
    want = nCols * nRows
    if len(scores) != want or len(labels) != want:
        return 'cellCount: %s %d scores, %d labels, %d cells' % (row[3], len(scores),
                                                                len(labels), want)
    for cell in scores:
        if cell.startswith('#') and not re.match(r'^#[0-9A-Fa-f]{6}$', cell):
            return 'colour: %s %r' % (row[3], cell)
    b = [float(x) for x in row[14].rstrip(',').split(',')]
    if not b[0] < b[1] < b[2]:
        return 'bounds: %s %s' % (row[3], b)
    return None
