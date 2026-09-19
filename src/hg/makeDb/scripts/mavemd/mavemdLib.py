#!/usr/bin/env python3
"""Shared helpers for the MaveMD track build: coordinate projection and colors.

MaveDB's VRS mapper resolves some score sets all the way to the genome and others only
to a protein sequence.  About a third of MaveMD variants arrive with a genomic HGVS term
and can be placed directly; the rest carry only an NP_ protein term and have to be walked
back to their codon here.

The projection is: NP_ accession -> NM_ transcript (hg38 ncbiRefSeqLink) -> genePred
(hg38 ncbiRefSeqCurated) -> the CDS bases in transcription order -> the three genomic
bases of codon N.  It is validated in makeMaveMdVariants.py against the ~154k variants
that carry both a genomic and a protein term, so a drift in either RefSeq or MaveDB's
mapper shows up as a coordinate disagreement rather than as silently wrong placements.
"""

import subprocess
import sys

# Standard amino acids ordered by class, matching the MaveDB and popEVE heatmap tracks,
# with Ter appended as a final row (MaveMD carries ~17k nonsense measurements).
STANDARD_AAS = list('AVLIMFYWRHKDESTNQGCP')
HEATMAP_ROWS = STANDARD_AAS + ['*']

THREE_TO_ONE = {
    'Ala': 'A', 'Arg': 'R', 'Asn': 'N', 'Asp': 'D', 'Cys': 'C', 'Gln': 'Q', 'Glu': 'E',
    'Gly': 'G', 'His': 'H', 'Ile': 'I', 'Leu': 'L', 'Lys': 'K', 'Met': 'M', 'Phe': 'F',
    'Pro': 'P', 'Ser': 'S', 'Thr': 'T', 'Trp': 'W', 'Tyr': 'Y', 'Val': 'V', 'Ter': '*',
}

# Two palettes, because the track carries two different kinds of statement.
#
# An ACMG evidence code is a calibrated clinical claim, and it gets the 11-class RdBu
# diverging ramp: red for pathogenic evidence, blue for benign, darker with strength.
# Red against blue stays separable under all three common kinds of color blindness.
#
# The strength ladder is MaveDB's own StrengthOfEvidenceProvided enum, not an assumption:
# VERY_STRONG, STRONG, MODERATE_PLUS, MODERATE, SUPPORTING.
ACMG_COLORS = {
    'PS3_very_strong':   '#67001f',
    'PS3':               '#b2182b',
    'PS3_moderate_plus': '#d6604d',
    'PS3_moderate':      '#f4a582',
    'PS3_supporting':    '#fddbc7',
    'PS3_not_met':       '#e8e8e8',
    'BS3_not_met':       '#e8e8e8',
    'BS3_supporting':    '#d1e5f0',
    'BS3_moderate':      '#92c5de',
    'BS3_moderate_plus': '#4393c3',
    'BS3':               '#2166ac',
    'BS3_very_strong':   '#053061',
}

# A functional class with no ACMG code is a measurement, not a clinical claim, so it is drawn
# in its own palette rather than in a pale red or blue that would read as weak evidence.
# Which palette is a live design question; CLASS_PALETTES holds the candidates and
# --classPalette on the heatmap and variant builders selects one.
#
#   purple  purple/green. Unambiguous, but reads as an unrelated dataset next to a
#           calibrated map.
#   grey    colour means clinical evidence, grey means measured without it. Clean in
#           principle, but the track already spends #e8e8e8 on "evidence not met" and
#           #d9d9d9 on "no call", so the pale end of this ramp collides with both.
#   brown   the brown half of BrBG. Colourblind-safe, off the red/blue evidence ramp, and
#           does not collide with the greys already in use.
CLASS_PALETTES = {
    'purple': {'abnormal': '#762a83', 'indeterminate': '#bdbdbd', 'normal': '#7fbf7b'},
    'grey':   {'abnormal': '#252525', 'indeterminate': '#969696', 'normal': '#f7f7f7'},
    'brown':  {'abnormal': '#8c510a', 'indeterminate': '#d8b365', 'normal': '#f6e8c3'},
}
CLASS_COLORS = dict(CLASS_PALETTES['purple'])


def setClassPalette(name):
    """Choose the palette used for measurements that carry no ACMG evidence code."""
    if name not in CLASS_PALETTES:
        raise ValueError('unknown class palette %r; choose from %s'
                         % (name, ', '.join(sorted(CLASS_PALETTES))))
    CLASS_COLORS.clear()
    CLASS_COLORS.update(CLASS_PALETTES[name])


NO_CALL_COLOR = '#d9d9d9'

# Order used only when a score set has no MaveDB-designated primary calibration and we
# have to say which of several calls to show. Strongest pathogenic first, then strongest
# benign, with "not met" last because it is the absence of evidence either way.
#
# This list ranks every pathogenic call above every benign one, so if two calibrations ever
# disagree in direction the pathogenic one wins regardless of strength. No variant in the
# collection currently has calibrations that disagree in direction, so the bias is latent;
# if one appears, this ordering is the thing to revisit.
ACMG_SEVERITY = ['PS3_very_strong', 'PS3', 'PS3_moderate_plus', 'PS3_moderate',
                 'PS3_supporting',
                 'BS3_very_strong', 'BS3', 'BS3_moderate_plus', 'BS3_moderate',
                 'BS3_supporting',
                 'PS3_not_met', 'BS3_not_met']


def hgsql(db, query):
    """Run a query and return rows as lists of strings."""
    out = subprocess.run(['hgsql', db, '-N', '-e', query],
                         check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         universal_newlines=True)
    return [line.split('\t') for line in out.stdout.rstrip('\n').split('\n') if line]


def isMainChrom(chrom):
    """True for chr1..chr22, chrX, chrY, chrM - not alts, randoms or patches."""
    return '_' not in chrom


GENCODE_ATTRS = 'wgEncodeGencodeAttrsV50'
GENCODE_GENEPRED = 'wgEncodeGencodeCompV50'


def loadProteinToTranscript(db, protAccs):
    """Map protein accessions to their transcripts.

    MaveDB states protein terms against RefSeq (NP_) for most score sets and against Ensembl
    (ENSP) for a handful, so both routes are needed: NP_ through ncbiRefSeqLink, ENSP through
    the GENCODE attributes table.  Each tries the exact versioned accession first, then any
    version of the same base accession.

    Returns (mapping, unresolved) so the caller can report a whole score set going missing
    rather than silently dropping it.
    """
    mapping = {}
    unresolved = []
    for acc in protAccs:
        base = acc.split('.')[0]
        if acc.startswith('ENSP'):
            table, col, key = GENCODE_ATTRS, 'transcriptId', 'proteinId'
        else:
            table, col, key = 'ncbiRefSeqLink', 'mrnaAcc', 'protAcc'
        rows = hgsql(db, "select %s from %s where %s = '%s'" % (col, table, key, acc))
        if not rows:
            rows = hgsql(db, "select %s from %s where %s like '%s.%%'"
                             % (col, table, key, base))
        if rows:
            mapping[acc] = rows[0][0]
        else:
            unresolved.append(acc)
    return mapping, unresolved


class CodonMap(object):
    """Genomic coordinates of every codon of one transcript.

    codons[n] is the list of three 0-based genomic positions of codon n (1-based protein
    numbering), in transcription order.  For a codon split across an intron those three
    positions are not contiguous, which is why callers take min() and clamp block widths
    rather than assuming a 3bp run.
    """

    def __init__(self, tx, chrom, strand, cdsBases):
        self.tx = tx
        self.chrom = chrom
        self.strand = strand
        self.cdsBases = cdsBases
        self.protLen = len(cdsBases) // 3

    def codon(self, protPos):
        """Genomic positions of codon protPos (1-based), or None if out of range."""
        i = (protPos - 1) * 3
        if protPos < 1 or i + 3 > len(self.cdsBases):
            return None
        return self.cdsBases[i:i + 3]

    def codonSpan(self, protPos):
        """Full genomic span of a codon as (start0, end), or None.

        A codon at an exon junction is split, so its three bases are not a 3bp run; the
        span then covers the intervening intron. That is the honest extent of the codon,
        and it is what a protein-level measurement actually pins down.
        """
        bases = self.codon(protPos)
        if bases is None:
            return None
        return min(bases), max(bases) + 1

    def codonBlock(self, protPos):
        """The longest contiguous run of a codon's bases, as (start0, size), or None.

        Used for the heatmap, where each column is drawn as one block. Drawing a split
        codon as three bases from its first position would put the block inside an intron;
        the longest run keeps it on real coding sequence, and column order is unchanged
        because runs stay in transcription order.
        """
        bases = self.codon(protPos)
        if bases is None:
            return None
        ordered = sorted(bases)
        runs = []
        runStart = ordered[0]
        runLen = 1
        for prev, cur in zip(ordered, ordered[1:]):
            if cur == prev + 1:
                runLen += 1
            else:
                runs.append((runStart, runLen))
                runStart, runLen = cur, 1
        runs.append((runStart, runLen))
        return max(runs, key=lambda r: r[1])


def loadCodonMaps(db, transcripts):
    """Build a CodonMap for each transcript from ncbiRefSeqCurated.

    RefSeq transcripts come from ncbiRefSeqCurated and Ensembl ones from the GENCODE
    genePred, keyed off the accession prefix.  A transcript can align to more than one place
    (alt haplotypes, fix patches); the alignment on a main chromosome wins, and among several
    the longest CDS wins.
    """
    maps = {}
    missing = []
    for tx in transcripts:
        table = GENCODE_GENEPRED if tx.startswith('ENST') else 'ncbiRefSeqCurated'
        rows = hgsql(db, "select chrom, strand, cdsStart, cdsEnd, exonStarts, exonEnds "
                         "from %s where name = '%s'" % (table, tx))
        best = None
        for chrom, strand, cdsStart, cdsEnd, exonStarts, exonEnds in rows:
            cdsStart, cdsEnd = int(cdsStart), int(cdsEnd)
            starts = [int(x) for x in exonStarts.rstrip(',').split(',')]
            ends = [int(x) for x in exonEnds.rstrip(',').split(',')]
            bases = []
            for s, e in zip(starts, ends):
                s = max(s, cdsStart)
                e = min(e, cdsEnd)
                if s < e:
                    bases.extend(range(s, e))
            if not bases:
                continue
            if strand == '-':
                bases.reverse()
            cand = CodonMap(tx, chrom, strand, bases)
            if best is None:
                best = cand
            elif isMainChrom(cand.chrom) and not isMainChrom(best.chrom):
                best = cand
            elif isMainChrom(cand.chrom) == isMainChrom(best.chrom) and \
                    len(cand.cdsBases) > len(best.cdsBases):
                best = cand
        if best is None:
            missing.append(tx)
        else:
            maps[tx] = best
    return maps, missing


def bedField(value):
    """Render one BED field: no tabs, no newlines, no non-ASCII.

    MaveDB free text (score set titles, formatted citations) carries all three. Tabs and
    newlines would split the row. Non-ASCII is subtler: the browser does not transcode
    UTF-8, so an en dash or an author name like Gr\u00f8nb\u00e6k-Thygesen reaches the
    details page as mojibake. Numeric HTML entities render correctly instead.
    """
    text = '' if value is None else str(value)
    for ch in ('\t', '\n', '\r'):
        text = text.replace(ch, ' ')
    while '  ' in text:
        text = text.replace('  ', ' ')
    return ''.join(c if ord(c) < 128 else '&#%d;' % ord(c) for c in text.strip())


def fmtScore(value, places=4):
    """Format a functional score for display, trimming trailing zeros."""
    try:
        text = ('%.*f' % (places, float(value))).rstrip('0').rstrip('.')
    except (TypeError, ValueError):
        return ''
    return '0' if text in ('', '-0') else text


def pickDisplayCall(calls, primaryTitle=None):
    """Choose which calibration's call drives the color and the filters.

    MaveDB curates a `primary` flag on score calibrations, with its own promote and demote API
    endpoints, so where that primary actually classifies the variant it is their editorial
    choice and we use it as-is.

    Two things complicate it. Some score sets have no primary designated at all. Others
    designate one that carries no functional classifications, so it says nothing about any
    variant and cannot be displayed even though it exists. Those are different situations and
    the item says which one happened, because "no primary designated" on a score set that has
    one is simply false.

    `calls` is a list of dicts with keys: urn, title, primary, ruo, funcClass, acmgOutcome,
    acmgCriterion, acmgStrength, odds. `primaryTitle` is the title of the score set's
    MaveDB-designated primary calibration, or None if it has none. Returns (chosen, source).
    """
    if not calls:
        return None, 'none'
    primary = [c for c in calls if c['primary']]
    if primary:
        return primary[0], 'MaveDB primary calibration'

    if primaryTitle:
        context = ('MaveDB\'s primary calibration (%s) classifies no variants, so it cannot be '
                   'shown' % primaryTitle)
    else:
        context = 'no primary calibration designated'

    def rank(call):
        code = call['acmgOutcome']
        return ACMG_SEVERITY.index(code) if code in ACMG_SEVERITY else len(ACMG_SEVERITY)

    scored = [c for c in calls if c['acmgOutcome']]
    if scored:
        chosen = min(scored, key=rank)
        if len(scored) == 1 and len(calls) == 1:
            return chosen, '%s; showing the only other calibration' % context
        return chosen, '%s; showing the strongest of %d others' % (context, len(calls))
    classed = [c for c in calls if c['funcClass']]
    if classed:
        return classed[0], '%s; showing the only other calibration with a classification' % context
    return calls[0], context


def cellColor(acmgOutcome, funcClass):
    """Color for a heatmap cell or a variant item."""
    if acmgOutcome and acmgOutcome in ACMG_COLORS:
        return ACMG_COLORS[acmgOutcome]
    if funcClass and funcClass in CLASS_COLORS:
        return CLASS_COLORS[funcClass]
    return NO_CALL_COLOR


def hexToBedRgb(color):
    """'#rrggbb' -> 'r,g,b' for the BED itemRgb field."""
    color = color.lstrip('#')
    return '%d,%d,%d' % (int(color[0:2], 16), int(color[2:4], 16), int(color[4:6], 16))
