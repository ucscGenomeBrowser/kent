#!/usr/bin/env python3
"""Alignments of MaveDB target sequences to hg38, as bigPsl.

Each target sequence (the protein or DNA the experiment tested) is placed at the locus its
score set's variants were placed at, never by a genome-wide best hit:

  1. Coding targets (a protein, or DNA whose frame-0 translation has no internal stop) are
     fitted to the reference protein the score set's variants were placed on (else to
     another RefSeq protein of the gene at the same locus, MANE first), by the same
     residue rule as the variants (mavedbPlace.fittingOffsets: exact, one differing
     position, or an added tag at one end, and a unique offset). Each target residue then
     maps to its reference codon, so the alignment follows the transcript's exons, split
     codons included. A target's own numbering is not reused for this: a full-length
     target can carry experiment numbering that starts mid-protein (TDP-43).
  2. Other DNA targets (promoters, enhancers, minigenes) and coding targets that do not fit
     are aligned with BLAT; the best-scoring hit that overlaps the score set's placed
     variants is kept, with no identity or coverage cutoff, as in Jonathan Casper's 2025
     build. A target with no such hit is left out.

DNA targets carry their sequence, so base differences (including codon optimisation) show.
Protein targets are written in nucleotide units (three per residue) and carry no sequence.
"""

import collections
import os
import re
import subprocess

import mavedbPlace as mp

BLAT_OPTIONS = ['-stepSize=5', '-repMatch=2253', '-minScore=20', '-minIdentity=0']


def translate(dna):
    dna = dna.upper()
    return ''.join(mp.CODON_TABLE.get(dna[i:i + 3], 'X') for i in range(0, len(dna) - 2, 3))


def codingProtein(seqType, seq):
    """The protein a coding target encodes, or None for a non-coding DNA target."""
    seq = re.sub(r'\s+', '', seq or '')
    if seqType == 'protein':
        return seq.upper().rstrip('*')
    if len(seq) < 30:
        return None
    prot = translate(seq).rstrip('*')
    if '*' in prot or 'X' in prot:
        return None
    return prot


def fitTarget(prot, npSeq):
    """(offset, mismatched positions) of a target protein on its reference protein."""
    wildType = {i + 1: {aa} for i, aa in enumerate(prot)}
    tol = mp.MAX_MISMATCHED_POSITIONS if len(wildType) >= mp.MIN_POSITIONS else 0
    hits = mp.fittingOffsets(npSeq, wildType, tol)
    exact = [h for h in hits if h[2] == 'exact']
    near = [h for h in hits if h[2] != 'exact']
    if len(exact) == 1:
        return exact[0][0], set()
    if not exact and len(near) == 1:
        return near[0][0], set(near[0][1])
    return None


class Psl(object):
    """Minimal PSL record built from aligned (query, genome) base pairs."""

    def __init__(self, qName, qSize, chrom, chromSize, strand, pairs, matches, mismatches):
        self.qName, self.qSize, self.chrom, self.chromSize = qName, qSize, chrom, chromSize
        self.strand = strand
        if strand == '-':
            pairs = [(qSize - 1 - q, t) for q, t in pairs]
        pairs.sort(key=lambda qt: qt[1])
        blocks = []
        for q, t in pairs:
            if blocks and q == blocks[-1][0] + blocks[-1][2] and t == blocks[-1][1] + blocks[-1][2]:
                blocks[-1][2] += 1
            else:
                blocks.append([q, t, 1])
        self.blocks = blocks
        self.matches, self.mismatches = matches, mismatches

    def line(self):
        b = self.blocks
        qGaps = sum(1 for x, y in zip(b, b[1:]) if y[0] > x[0] + x[2])
        tGaps = sum(1 for x, y in zip(b, b[1:]) if y[1] > x[1] + x[2])
        qGapBases = sum(y[0] - x[0] - x[2] for x, y in zip(b, b[1:]))
        tGapBases = sum(y[1] - x[1] - x[2] for x, y in zip(b, b[1:]))
        qStart, qEnd = b[0][0], b[-1][0] + b[-1][2]
        if self.strand == '-':
            qStart, qEnd = self.qSize - qEnd, self.qSize - qStart
        f = [self.matches, self.mismatches, 0, 0, qGaps, qGapBases, tGaps, tGapBases,
             self.strand, self.qName, self.qSize, qStart, qEnd, self.chrom, self.chromSize,
             b[0][1], b[-1][1] + b[-1][2], len(b),
             ','.join(str(x[2]) for x in b) + ',',
             ','.join(str(x[0]) for x in b) + ',',
             ','.join(str(x[1]) for x in b) + ',']
        return '\t'.join(str(x) for x in f)


def projectTarget(qName, seqType, seq, prot, offset, cm, chromSize):
    """PSL of a coding target through the codons of its reference transcript."""
    pairs = []
    matches = mismatches = 0
    dna = re.sub(r'\s+', '', seq or '').upper() if seqType == 'dna' else None
    qSize = len(dna) if dna else 3 * len(prot)
    # A DNA target's stop codon is part of the tested sequence, so it is drawn too.
    nCodons = len(dna) // 3 if dna else len(prot)
    for i in range(nCodons):
        codon = cm.codon(i + 1 + offset)
        if codon is None:
            continue
        for k in range(3):
            q = 3 * i + k
            if q >= qSize:
                break
            pairs.append((q, codon[k]))
            j = 3 * (i + offset) + k
            if dna:
                same = dna[q] == cm.cds[j]
            else:
                same = prot[i] == cm.protein[i + offset]
            if same:
                matches += 1
            else:
                mismatches += 1
    if not pairs:
        return None
    return Psl(qName, qSize, cm.chrom, chromSize, cm.strand, pairs, matches, mismatches)


def readChromSizes(path):
    sizes = {}
    with open(path) as fh:
        for line in fh:
            c, n = line.split()[:2]
            sizes[c] = int(n)
    return sizes


def runBlat(twoBit, ooc, queries, workDir):
    """BLAT DNA queries {name: seq} against the genome; {name: [psl fields]}."""
    fa = os.path.join(workDir, 'blatQueries.fa')
    out = os.path.join(workDir, 'blat.psl')
    with open(fa, 'w') as fh:
        for name, seq in sorted(queries.items()):
            fh.write('>%s\n%s\n' % (name, seq))
    subprocess.run(['blat', twoBit, fa, '-ooc=' + ooc, '-noHead'] + BLAT_OPTIONS + [out],
                   check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    hits = collections.defaultdict(list)
    with open(out) as fh:
        for line in fh:
            f = line.rstrip('\n').split('\t')
            hits[f[9]].append(f)
    return hits


def pslScore(f):
    """BLAT's score: matches + repMatches - mismatches - gaps."""
    return int(f[0]) + int(f[2]) - int(f[1]) - int(f[4]) - int(f[6])


def chooseHit(hits, loci):
    """Best-scoring hit overlapping any of the score set's placed loci, or None."""
    best = None
    for f in hits:
        chrom, start, end = f[13], int(f[15]), int(f[16])
        if not any(c == chrom and s < end and start < e for c, s, e in loci):
            continue
        if best is None or pslScore(f) > pslScore(best):
            best = f
    return best


def targetName(scoreSet, target):
    name = re.sub(r'[^A-Za-z0-9.-]+', '_', target.get('name') or 'target').strip('_')
    return '%s_%s' % (scoreSet['urn'].split(':')[-1], name)


def buildAlignments(entries, protToMap, twoBit, ooc, chromSizesPath, outDir, workDir,
                    asFile):
    """Write mavedbAlignDna.bb and mavedbAlignAa.bb; return a Counter of outcomes.

    entries: (scoreSet, target, proteins, loci) for every target of every score set with
    a heatmap; proteins are tried in order, the placed protein first, and any other one
    only if its codons overlap loci, the (chrom, start, end) spans of the placed items.
    """
    sizes = readChromSizes(chromSizesPath)
    stats = collections.Counter()
    dnaPsl, aaPsl, dnaSeqs = [], [], {}
    blatQueries, blatLoci = {}, {}
    for scoreSet, target, prots, loci in entries:
        ts = target.get('targetSequence')
        if not ts or not ts.get('sequence'):
            stats['noSequence'] += 1
            continue
        qName = targetName(scoreSet, target)
        seqType = ts.get('sequenceType')
        seq = re.sub(r'\s+', '', ts['sequence']).upper()
        prot = codingProtein(seqType, seq)
        psl = None
        # As for the variants, an exact fit on any candidate beats a near fit (one position,
        # or an end run counted as a tag): otherwise an isoform lacking the target's first
        # exons would pass as a tag fit ahead of the isoform the target is (STIM1).
        fits = []
        for i, candidate in enumerate(prots if prot else []):
            cm = protToMap.get(candidate)
            if cm is None or (i > 0 and not atLoci(cm, loci)):
                continue
            fit = fitTarget(prot, ref_peptide(cm))
            if fit is not None:
                fits.append((len(fit[1]) > 0, i, cm, fit[0]))
        if fits:
            _, i, cm, offset = min(fits, key=lambda f: f[:2])
            psl = projectTarget(qName, seqType, seq, prot, offset, cm, sizes[cm.chrom])
            if psl is not None and i > 0:
                stats['projectedOnOtherIsoform'] += 1
        if psl is not None:
            stats['projected:' + seqType] += 1
            if seqType == 'dna':
                dnaPsl.append(psl.line())
                dnaSeqs[qName] = seq
            else:
                aaPsl.append(psl.line())
            continue
        if seqType == 'dna':
            blatQueries[qName] = seq
            blatLoci[qName] = loci
        else:
            stats['proteinNoFit'] += 1
    if blatQueries:
        hits = runBlat(twoBit, ooc, blatQueries, workDir)
        for qName in sorted(blatQueries):
            best = chooseHit(hits.get(qName, []), blatLoci[qName])
            if best is None:
                stats['blatNoHitAtLocus'] += 1
                continue
            stats['blat'] += 1
            dnaPsl.append('\t'.join(best))
            dnaSeqs[qName] = blatQueries[qName]
    n = {}
    for label, lines, seqs in (('Dna', dnaPsl, dnaSeqs), ('Aa', aaPsl, None)):
        n[label] = writeBigPsl(lines, seqs, os.path.join(outDir, 'mavedbAlign%s.bb' % label),
                               chromSizesPath, workDir, asFile)
    stats['itemsDna'], stats['itemsAa'] = n['Dna'], n['Aa']
    return stats


def atLoci(cm, loci):
    """True if a codon map's coding bases overlap any of the placed loci."""
    lo, hi = min(cm.cdsBases), max(cm.cdsBases) + 1
    return any(c == cm.chrom and s < hi and lo < e for c, s, e in loci)


def ref_peptide(cm):
    """The genome translation of a codon map, without its stop."""
    return cm.protein.rstrip('*')


def writeBigPsl(lines, seqs, outBb, chromSizes, workDir, asFile):
    base = os.path.join(workDir, os.path.basename(outBb)[:-3])
    with open(base + '.psl', 'w') as fh:
        for line in lines:
            fh.write(line + '\n')
    cmd = ['pslToBigPsl', base + '.psl', base + '.bigPslInput']
    if seqs:
        with open(base + '.fa', 'w') as fh:
            for name, seq in sorted(seqs.items()):
                fh.write('>%s\n%s\n' % (name, seq))
        cmd.insert(1, '-fa=' + base + '.fa')
    subprocess.run(cmd, check=True)
    subprocess.run(['sort', '-k1,1', '-k2,2n', '-o', base + '.bigPslInput',
                    base + '.bigPslInput'], check=True)
    subprocess.run(['bedToBigBed', '-type=bed12+13', '-tab', '-as=' + asFile,
                    base + '.bigPslInput', chromSizes, outBb + '.tmp'], check=True,
                   stdout=subprocess.PIPE)
    os.rename(outBb + '.tmp', outBb)
    return len(lines)
