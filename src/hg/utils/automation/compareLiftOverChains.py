#!/usr/bin/env python3

"""
compareLiftOverChains.py -- ad hoc QA tool, not part of the asmHub pipeline.

Compare two or more *.over.chain[.gz] liftOver files built by different
procedures (lastz, minimap2, a third-party project, etc.) for the same
target/query pair:

  1. Fundamental chain-file statistics: chain/block counts, aligned vs.
     gap bases, N50 of block size and of chain target-span (fragmentation
     proxies), and merged target/query base coverage.
  2. How well a genePred set of transcripts (e.g. hg38 RefSeq curated)
     translates through each chain, via 'liftOver -genePred': how many
     transcripts fully map vs. fail, and why (reason tally parsed from
     liftOver's unmapped-file comments).
  3. How well any plain BED file (segmental duplications, an SV track,
     anything else) translates through each chain, via plain 'liftOver':
     mapped/unmapped feature count and bp, same reason tally.

This script only reads its inputs and shells out to the 'liftOver' binary
-- it does not modify any of the input files.

usage:
  compareLiftOverChains.py [options] chain1 [label2=]chain2 ...

  chainN            one or more *.over.chain or *.over.chain.gz files,
                    all for the same target/query pair.  Prefix with
                    'label=' to control the name used in the report, e.g.
                      lastz=/hive/.../lastzHs1.2022-09-21/axtChain/hg38.hs1.over.chain.gz
                    otherwise the label is derived from the file's basename.

options:
  --genePred FILE       genePred-format file (standard or extended genePred,
                        e.g. a dump of hg38's ncbiRefSeqCurated/refGene table
                        -- NOT bigGenePred/bed12; convert first if that's
                        what you have) to test translation of. May be
                        gzipped; liftOver reads .gz directly via its own
                        internal pipe, same as the chain files.
  --bed FILE            a plain BED file to test translation of (label=path
                        or bare path, same convention as the chain args);
                        repeatable to test more than one track in one run.
                        May be gzipped.
  --mergeBed            before testing, collapse each --bed file's overlapping
                        intervals into non-redundant BED3 (chrom/start/end
                        only -- other columns are dropped). Recommended for
                        a heavily self-overlapping track like genomicSuperDups,
                        where otherwise the same genomic bases get counted
                        (and tested) many times over.
  --chainStats          only report chain file structural statistics; run no
                        liftOver translation test at all. --genePred/--bed
                        are ignored if also given, and no liftOver binary is
                        needed in this mode.
  --liftOverBin PATH    liftOver executable (default: 'liftOver' on PATH,
                        falling back to /cluster/bin/x86_64/liftOver)
  --targetSizes FILE    chrom.sizes for the target genome -- gives accurate
                        % of genome covered instead of the (possibly
                        incomplete) total derived from the chain file itself
  --querySizes FILE     chrom.sizes for the query genome, same reason
  --minMatch F          passed through as liftOver's -minMatch=F
  --outDir DIR          where liftOver's mapped/unmapped output files are
                        written (default: a temp directory, removed after
                        the report prints unless --keep is given)
  --keep                keep --outDir's contents instead of deleting them
  --regionBeds DIR      write target-genome common.bed (covered by every
                        chain given) and one <label>.unique.bed per chain
                        (covered by that chain and no other) into this
                        directory. Target-coordinates only -- see the
                        'targetIntervals' comment on analyzeChain() for why.
  --failBeds DIR        write a bed9+1 file of every record that failed to
                        lift through a chain, one file per chain per test:
                        genePred.<chainLabel>.unmapped.bed (genePred records
                        reduced to chrom/txStart/txEnd/name/0/strand) and
                        <bedLabel>.<chainLabel>.unmapped.bed (BED3-BED12+
                        normalized to the same 6 columns, name/score/strand
                        padded with '.'/'0' if the input didn't have them).
                        thickStart/thickEnd (columns 7-8) are set to the
                        item's own start/end; itemRgb (column 9) is colored
                        by failure category (see FAILURE_COLORS); column 10
                        is liftOver's own, full reason text for why that
                        record failed.

At least one of --genePred / --bed must be given, unless --chainStats is.
"""

import argparse
import gzip
import os
import shutil
import subprocess
import sys
import tempfile
from collections import Counter, defaultdict


def opener(path):
    return gzip.open(path, 'rt') if path.endswith('.gz') else open(path, 'r')


def parseChains(path):
    """Yield one dict per chain: header fields + a list of (size, dt, dq) blocks."""
    chains = []
    cur = None
    blocks = []
    with opener(path) as fh:
        for line in fh:
            line = line.rstrip('\n')
            if not line.strip():
                if cur is not None:
                    cur['blocks'] = blocks
                    chains.append(cur)
                cur = None
                blocks = []
                continue
            # axtChain-produced chain files can carry '#'-prefixed comment
            # lines (e.g. '##matrix=axtChain...' scoring-matrix metadata) --
            # these are not part of any chain's block list, skip them.
            if line.startswith('#'):
                continue
            if line.startswith('chain'):
                if cur is not None:
                    cur['blocks'] = blocks
                    chains.append(cur)
                    blocks = []
                f = line.split()
                # chain score tName tSize tStrand tStart tEnd qName qSize qStrand qStart qEnd id
                cur = {
                    'score': int(f[1]), 'tName': f[2], 'tSize': int(f[3]),
                    'tStrand': f[4], 'tStart': int(f[5]), 'tEnd': int(f[6]),
                    'qName': f[7], 'qSize': int(f[8]), 'qStrand': f[9],
                    'qStart': int(f[10]), 'qEnd': int(f[11]), 'id': f[12],
                }
            else:
                nums = line.split()
                if len(nums) == 3:
                    blocks.append((int(nums[0]), int(nums[1]), int(nums[2])))
                elif len(nums) == 1:
                    blocks.append((int(nums[0]), 0, 0))
    if cur is not None:
        cur['blocks'] = blocks
        chains.append(cur)
    return chains


def n50(lengths):
    lengths = sorted(lengths, reverse=True)
    total = sum(lengths)
    if total == 0:
        return 0
    half = total / 2.0
    running = 0
    for length in lengths:
        running += length
        if running >= half:
            return length
    return 0


def mergeIntervalsByChrom(intervalsByName):
    """Collapse each name's (start, end) interval list to a sorted,
    non-overlapping list -- {name: [(s, e), ...]}."""
    merged = {}
    for name, ivs in intervalsByName.items():
        out = []
        for s, e in sorted(ivs):
            if out and s <= out[-1][1]:
                if e > out[-1][1]:
                    out[-1][1] = e
            else:
                out.append([s, e])
        merged[name] = [(s, e) for s, e in out]
    return merged


def coverageTotal(mergedByName):
    """Total bases in an already-merged {name: [(s, e), ...]} interval dict."""
    return sum(e - s for ivs in mergedByName.values() for s, e in ivs)


def analyzeChain(path):
    chains = parseChains(path)
    nChains = len(chains)
    blockSizes = []
    tSpans = []
    tGap = qGap = 0
    tIntervals = defaultdict(list)
    qIntervals = defaultdict(list)
    tSizes = {}
    qSizes = {}
    for c in chains:
        tSizes.setdefault(c['tName'], c['tSize'])
        qSizes.setdefault(c['qName'], c['qSize'])
        tSpans.append(c['tEnd'] - c['tStart'])
        # (tStart, tEnd)/(qStart, qEnd) are only the chain's overall
        # bounding span -- it can cross large unaligned stretches (indels,
        # inversions, assembly gaps) via the blocks' own dt/dq gaps.  Walk
        # the blocks themselves to get the actually-aligned intervals;
        # using the span as "coverage" badly overcounts true aligned bases.
        tPos = c['tStart']
        qPos = c['qStart']
        for size, dt, dq in c['blocks']:
            tIntervals[c['tName']].append((tPos, tPos + size))
            # chain format always gives qPos in the qStrand's own
            # coordinate frame; normalize '-' strand chains back to the
            # query's forward strand so coverage can be merged correctly.
            if c['qStrand'] == '-':
                qS, qE = c['qSize'] - (qPos + size), c['qSize'] - qPos
            else:
                qS, qE = qPos, qPos + size
            qIntervals[c['qName']].append((qS, qE))
            blockSizes.append(size)
            tGap += dt
            qGap += dq
            tPos += size + dt
            qPos += size + dq
    nBlocks = len(blockSizes)
    targetIntervals = mergeIntervalsByChrom(tIntervals)
    queryIntervals = mergeIntervalsByChrom(qIntervals)
    return {
        'nChains': nChains,
        'nBlocks': nBlocks,
        'alignedBases': sum(blockSizes),
        'targetGapBases': tGap,
        'queryGapBases': qGap,
        'blockSizeN50': n50(blockSizes),
        'chainSpanN50': n50(tSpans),
        'blocksPerChain': (nBlocks / nChains) if nChains else 0,
        'targetCovered': coverageTotal(targetIntervals),
        'queryCovered': coverageTotal(queryIntervals),
        'tSizesSum': sum(tSizes.values()),
        'qSizesSum': sum(qSizes.values()),
        'targetIntervals': targetIntervals,
    }


def readSizes(path):
    sizes = {}
    if not path:
        return sizes
    with open(path) as fh:
        for line in fh:
            parts = line.split()
            if len(parts) >= 2:
                sizes[parts[0]] = int(parts[1])
    return sizes


def countGenePredLines(path):
    n = 0
    with opener(path) as fh:
        for line in fh:
            if line.strip() and not line.startswith('#'):
                n += 1
    return n


def bedFeatureCountAndBp(path):
    """Count data lines and total (end-start) bp in a BED-like file, skipping
    blank/comment/track/browser lines. Used for input totals and for both
    liftOver's mapped and unmapped output files (the unmapped file's
    '#reason' comment lines are skipped the same way)."""
    n = 0
    bp = 0
    with opener(path) as fh:
        for line in fh:
            line = line.rstrip('\n')
            if not line.strip() or line.startswith('#'):
                continue
            f = line.split('\t')
            if len(f) < 3 or f[0] in ('track', 'browser'):
                continue
            n += 1
            bp += int(f[2]) - int(f[1])
    return n, bp


def mergeBedFile(path, outPath):
    """Collapse to non-overlapping BED3 intervals per chrom -- loses any
    other columns (name/score/strand/etc). Only meant for coverage-style
    bp/feature-count testing of a heavily self-overlapping track."""
    ivs = defaultdict(list)
    with opener(path) as fh:
        for line in fh:
            line = line.rstrip('\n')
            if not line.strip() or line.startswith('#'):
                continue
            f = line.split('\t')
            if len(f) < 3 or f[0] in ('track', 'browser'):
                continue
            ivs[f[0]].append((int(f[1]), int(f[2])))
    with open(outPath, 'w') as out:
        for chrom in sorted(ivs):
            merged = []
            for s, e in sorted(ivs[chrom]):
                if merged and s <= merged[-1][1]:
                    if e > merged[-1][1]:
                        merged[-1][1] = e
                else:
                    merged.append([s, e])
            for s, e in merged:
                out.write(f'{chrom}\t{s}\t{e}\n')


def sweepRegions(labeledIntervals):
    """labeledIntervals: list of (label, {chrom: [(s, e), ...]}) -- each
    chain's own merged, non-overlapping target intervals (as returned by
    analyzeChain()'s 'targetIntervals'). Does a multi-way interval sweep per
    chrom and returns (commonByChrom, uniqueByLabelByChrom):
      commonByChrom[chrom]            -- regions covered by every label
      uniqueByLabelByChrom[label][chrom] -- regions covered by that label
                                            and no other label
    Between any two consecutive breakpoints the set of active labels is
    constant, so each such segment is already maximal -- no further merging
    is needed."""
    labels = [label for label, _ in labeledIntervals]
    nLabels = len(labels)
    chroms = set()
    for _label, byChrom in labeledIntervals:
        chroms.update(byChrom.keys())

    commonByChrom = defaultdict(list)
    uniqueByLabelByChrom = {label: defaultdict(list) for label in labels}

    for chrom in sorted(chroms):
        deltaAt = defaultdict(lambda: defaultdict(int))  # pos -> label -> delta
        for label, byChrom in labeledIntervals:
            for s, e in byChrom.get(chrom, []):
                deltaAt[s][label] += 1
                deltaAt[e][label] -= 1

        positions = sorted(deltaAt.keys())
        active = Counter()
        for i, pos in enumerate(positions):
            for label, delta in deltaAt[pos].items():
                active[label] += delta
            if i + 1 >= len(positions):
                continue
            segStart, segEnd = pos, positions[i + 1]
            if segEnd <= segStart:
                continue
            activeLabels = [label for label in labels if active[label] > 0]
            if len(activeLabels) == nLabels:
                commonByChrom[chrom].append((segStart, segEnd))
            elif len(activeLabels) == 1:
                uniqueByLabelByChrom[activeLabels[0]][chrom].append((segStart, segEnd))

    return commonByChrom, uniqueByLabelByChrom


def writeBedIntervals(byChrom, outPath):
    with open(outPath, 'w') as out:
        for chrom in sorted(byChrom):
            for s, e in sorted(byChrom[chrom]):
                out.write(f'{chrom}\t{s}\t{e}\n')


# The complete observed vocabulary of liftOver -genePred failure reasons --
# everything else collapses to 'Boundary problem' (many individual variants,
# differing only in the need/got/diff/mapped numbers) or falls back to
# FALLBACK_COLOR if some future liftOver version adds a reason not seen here.
FAILURE_COLORS = {
    'Deleted in new': '255,0,0',
    'Partially deleted in new': '128,0,0',
    'Boundary problem': '0,0,255',
    "Can't find thickStart/thickEnd": '0,255,0',
}
FALLBACK_COLOR = '0,0,0'


def reasonCategory(reason):
    """Collapse a specific reason string (e.g. 'Boundary problem: need 15, '
    'got 14, diff 1, mapped 0.9') down to its broad failure category."""
    if reason.startswith('Boundary problem'):
        return 'Boundary problem'
    return reason


def reasonColor(reason):
    return FAILURE_COLORS.get(reasonCategory(reason), FALLBACK_COLOR)


def consolidateReasons(reasons):
    """Collapse a reason Counter's 'Boundary problem: ...' variants into a
    single 'Boundary problem' entry, for a readable text report."""
    consolidated = Counter()
    for reason, count in reasons.items():
        consolidated[reasonCategory(reason)] += count
    return consolidated


def genePredToBed9Plus1(fields, reason):
    """Reduce one genePred (standard or extended) record's fields to a
    bed9+1 line: thickStart/thickEnd set to the item's own start/end,
    itemRgb colored by failure category, column 10 the full reason text."""
    name, chrom, strand, txStart, txEnd = fields[0], fields[1], fields[2], fields[3], fields[4]
    rgb = reasonColor(reason)
    return (f'{chrom}\t{txStart}\t{txEnd}\t{name}\t0\t{strand}\t'
            f'{txStart}\t{txEnd}\t{rgb}\t{reason}\n')


def toBed9Plus1(fields, reason):
    """Normalize a BED record's fields (BED3 up through BED12+) to bed9,
    padding any missing name/score/strand, setting thickStart/thickEnd to
    the item's own start/end, coloring itemRgb by failure category, and
    appending the full reason text as column 10."""
    chrom, start, end = fields[0], fields[1], fields[2]
    name = fields[3] if len(fields) > 3 else '.'
    score = fields[4] if len(fields) > 4 else '0'
    strand = fields[5] if len(fields) > 5 else '.'
    rgb = reasonColor(reason)
    return (f'{chrom}\t{start}\t{end}\t{name}\t{score}\t{strand}\t'
            f'{start}\t{end}\t{rgb}\t{reason}\n')


def writeCleanUnmapped(unmappedPath, outPath, lineConverter):
    """Copy an unmapped-records file, dropping liftOver's '#reason' comment
    lines and blank lines, and converting each surviving record -- together
    with the reason that preceded it -- to a bed9+1 line via
    lineConverter(fields, reason)."""
    currentReason = 'unknown'
    with open(unmappedPath) as fh, open(outPath, 'w') as out:
        for line in fh:
            line = line.rstrip('\n')
            if not line.strip():
                continue
            if line.startswith('#'):
                currentReason = line.lstrip('#').strip() or 'unknown'
                continue
            out.write(lineConverter(line.split('\t'), currentReason))


def parseUnmappedReasons(path):
    """liftOver's -genePred unmapped file prefixes each failed record with
    a '#<reason>' comment line.  Tally failures by that reason text.
    (Exact wording can vary by liftOver version -- if your version's
    comments look different, this still buckets correctly by whatever
    text precedes each failed record, just under a possibly-unfamiliar
    label.)"""
    reasons = Counter()
    currentReason = 'unknown'
    with open(path) as fh:
        for line in fh:
            line = line.rstrip('\n')
            if not line.strip():
                continue
            if line.startswith('#'):
                currentReason = line.lstrip('#').strip() or 'unknown'
            else:
                reasons[currentReason] += 1
    return reasons


def findLiftOverBin(explicit):
    if explicit:
        return explicit
    found = shutil.which('liftOver')
    if found:
        return found
    fallback = '/cluster/bin/x86_64/liftOver'
    if os.path.exists(fallback):
        return fallback
    return 'liftOver'  # let subprocess raise a clear FileNotFoundError


def runLiftOverGenePred(liftOverBin, genePred, chainPath, outDir, label, minMatch):
    mapped = os.path.join(outDir, f'{label}.mapped.gp')
    unmapped = os.path.join(outDir, f'{label}.unmapped.gp')
    cmd = [liftOverBin, '-genePred']
    if minMatch is not None:
        cmd.append(f'-minMatch={minMatch}')
    cmd += [genePred, chainPath, mapped, unmapped]
    subprocess.run(cmd, check=True)
    return mapped, unmapped


def runLiftOverBed(liftOverBin, bedPath, chainPath, outDir, bedLabel, chainLabel, minMatch):
    mapped = os.path.join(outDir, f'{bedLabel}.{chainLabel}.mapped.bed')
    unmapped = os.path.join(outDir, f'{bedLabel}.{chainLabel}.unmapped.bed')
    cmd = [liftOverBin]
    if minMatch is not None:
        cmd.append(f'-minMatch={minMatch}')
    cmd += [bedPath, chainPath, mapped, unmapped]
    subprocess.run(cmd, check=True)
    return mapped, unmapped


def parseLabeledArg(arg, exts):
    """Split a 'label=path' CLI arg (or bare path), auto-labeling from the
    basename -- stripping the first of 'exts' that matches -- when no
    explicit label is given."""
    if '=' in arg:
        label, path = arg.split('=', 1)
    else:
        label, path = None, arg
    if not label:
        base = os.path.basename(path)
        for ext in exts:
            if base.endswith(ext):
                base = base[:-len(ext)]
                break
        label = base
    return label, path


def pct(part, whole):
    return (100.0 * part / whole) if whole else float('nan')


def cnum(n):
    """Format an integer with thousands-comma separators for display."""
    return f'{n:,}'


def printTable(headers, rows):
    widths = [len(h) for h in headers]
    for row in rows:
        for i, cell in enumerate(row):
            widths[i] = max(widths[i], len(str(cell)))
    def fmt(row):
        return '  '.join(str(c).ljust(widths[i]) for i, c in enumerate(row))
    print(fmt(headers))
    print(fmt(['-' * w for w in widths]))
    for row in rows:
        print(fmt(row))


def main():
    ap = argparse.ArgumentParser(
        description='Compare liftOver chain files: structural stats + genePred/BED translation rate.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    ap.add_argument('chains', nargs='+',
                     help="one or more chain files, optionally 'label=path'")
    ap.add_argument('--genePred', default=None, help='genePred file to test translation of')
    ap.add_argument('--bed', action='append', default=[],
                     help="a BED file to test translation of, optionally 'label=path'; "
                          "repeatable")
    ap.add_argument('--mergeBed', action='store_true',
                     help='collapse each --bed file to non-overlapping BED3 before testing')
    ap.add_argument('--chainStats', action='store_true',
                     help='only report chain file structural statistics; run no '
                          'liftOver translation test at all (--genePred/--bed, if '
                          'also given, are ignored) -- no liftOver binary is needed')
    ap.add_argument('--liftOverBin', default=None)
    ap.add_argument('--targetSizes', default=None, help='chrom.sizes for the target genome')
    ap.add_argument('--querySizes', default=None, help='chrom.sizes for the query genome')
    ap.add_argument('--minMatch', default=None, type=float)
    ap.add_argument('--outDir', default=None)
    ap.add_argument('--keep', action='store_true')
    ap.add_argument('--regionBeds', default=None,
                     help='write target-genome common.bed (covered by every chain '
                          'given) and one <label>.unique.bed per chain (covered by '
                          'that chain and no other) into this directory')
    ap.add_argument('--failBeds', default=None,
                     help='write a clean BED of every record that failed to lift '
                          'through each chain (one file per chain per test) into '
                          'this directory')
    args = ap.parse_args()

    if args.chainStats:
        args.genePred = None
        args.bed = []
    elif not args.genePred and not args.bed:
        ap.error('need --genePred and/or at least one --bed to test anything '
                  '(or pass --chainStats to just measure the chain files)')

    if len(args.chains) < 2:
        print('# warning: comparing a single chain file -- pass 2+ to get a real comparison',
              file=sys.stderr)

    liftOverBin = findLiftOverBin(args.liftOverBin)
    targetSizes = readSizes(args.targetSizes)
    querySizes = readSizes(args.querySizes)

    outDir = args.outDir or tempfile.mkdtemp(prefix='compareLiftOverChains.')
    os.makedirs(outDir, exist_ok=True)
    if args.failBeds:
        os.makedirs(args.failBeds, exist_ok=True)

    chainSpecs = [parseLabeledArg(arg, ('.chain.gz', '.chain')) for arg in args.chains]
    for label, path in chainSpecs:
        if not os.path.exists(path):
            ap.error(f"chain file not found: {path}")

    totalTranscripts = countGenePredLines(args.genePred) if args.genePred else None

    chainRows = []
    translationRows = []
    reasonReports = []
    targetIntervalsByLabel = []

    for label, path in chainSpecs:
        stats = analyzeChain(path)
        if args.regionBeds:
            targetIntervalsByLabel.append((label, stats['targetIntervals']))
        tDenom = sum(targetSizes.values()) if targetSizes else stats['tSizesSum']
        qDenom = sum(querySizes.values()) if querySizes else stats['qSizesSum']

        chainRows.append([
            label,
            cnum(stats['nChains']),
            cnum(stats['nBlocks']),
            f"{stats['blocksPerChain']:.2f}",
            cnum(stats['alignedBases']),
            cnum(stats['targetGapBases']),
            cnum(stats['queryGapBases']),
            cnum(stats['blockSizeN50']),
            cnum(stats['chainSpanN50']),
            cnum(stats['targetCovered']),
            f"{pct(stats['targetCovered'], tDenom):.2f}%",
            cnum(stats['queryCovered']),
            f"{pct(stats['queryCovered'], qDenom):.2f}%",
        ])

        if args.genePred:
            mapped, unmapped = runLiftOverGenePred(
                liftOverBin, args.genePred, path, outDir, label, args.minMatch)
            mappedCount = countGenePredLines(mapped)
            reasons = parseUnmappedReasons(unmapped)
            unmappedCount = sum(reasons.values())

            if args.failBeds:
                failPath = os.path.join(args.failBeds, f'genePred.{label}.unmapped.bed')
                writeCleanUnmapped(unmapped, failPath, genePredToBed9Plus1)

            translationRows.append([
                label,
                cnum(mappedCount),
                f"{pct(mappedCount, totalTranscripts):.2f}%",
                cnum(unmappedCount),
                f"{pct(unmappedCount, totalTranscripts):.2f}%",
            ])
            reasonReports.append((label, reasons))

    bedReports = []  # (bedLabel, bedPath, merged, totalFeat, totalBp, rows, reasonReports)
    for bedArg in args.bed:
        bedLabel, bedPath = parseLabeledArg(bedArg, ('.bed.gz', '.bed', '.gz'))
        if not os.path.exists(bedPath):
            ap.error(f"bed file not found: {bedPath}")

        testPath = bedPath
        if args.mergeBed:
            testPath = os.path.join(outDir, f'{bedLabel}.merged.bed')
            mergeBedFile(bedPath, testPath)
        totalFeat, totalBp = bedFeatureCountAndBp(testPath)

        rows = []
        bedReasonReports = []
        for chainLabel, chainPath in chainSpecs:
            mapped, unmapped = runLiftOverBed(
                liftOverBin, testPath, chainPath, outDir, bedLabel, chainLabel, args.minMatch)
            mappedFeat, mappedBp = bedFeatureCountAndBp(mapped)
            unmappedFeat, unmappedBp = bedFeatureCountAndBp(unmapped)
            reasons = parseUnmappedReasons(unmapped)

            if args.failBeds:
                failPath = os.path.join(args.failBeds, f'{bedLabel}.{chainLabel}.unmapped.bed')
                writeCleanUnmapped(unmapped, failPath, toBed9Plus1)

            rows.append([
                chainLabel,
                cnum(mappedFeat), f"{pct(mappedFeat, totalFeat):.2f}%",
                cnum(mappedBp), f"{pct(mappedBp, totalBp):.2f}%",
                cnum(unmappedFeat), f"{pct(unmappedFeat, totalFeat):.2f}%",
                cnum(unmappedBp), f"{pct(unmappedBp, totalBp):.2f}%",
            ])
            bedReasonReports.append((chainLabel, reasons))
        bedReports.append((bedLabel, bedPath, args.mergeBed, totalFeat, totalBp, rows, bedReasonReports))

    if not args.chainStats:
        print(f"# liftOver binary: {liftOverBin}")
    if not targetSizes:
        print("# note: no --targetSizes given -- target % coverage is relative to the "
              "sequences present in each chain file, not the whole genome")
    if not querySizes:
        print("# note: no --querySizes given -- query % coverage is relative to the "
              "sequences present in each chain file, not the whole genome")
    if args.failBeds:
        print(f"# --failBeds: unmapped-record BED files written to {args.failBeds}")
    print()

    print('## Chain file structural statistics')
    printTable(
        ['label', 'chains', 'blocks', 'blocks/chain', 'aligned bp', 'tGap bp', 'qGap bp',
         'blockN50', 'chainSpanN50', 'tCovered bp', 'tCov%', 'qCovered bp', 'qCov%'],
        chainRows,
    )
    print()

    if args.regionBeds:
        os.makedirs(args.regionBeds, exist_ok=True)
        commonByChrom, uniqueByLabelByChrom = sweepRegions(targetIntervalsByLabel)

        commonPath = os.path.join(args.regionBeds, 'common.bed')
        writeBedIntervals(commonByChrom, commonPath)

        print(f'## --regionBeds output (target-genome coordinates): {args.regionBeds}')
        print(f"# common.bed: {cnum(coverageTotal(commonByChrom))} bp covered by all "
              f"{len(chainSpecs)} chains -> {commonPath}")
        for label, _path in chainSpecs:
            uniquePath = os.path.join(args.regionBeds, f'{label}.unique.bed')
            writeBedIntervals(uniqueByLabelByChrom[label], uniquePath)
            print(f"# {label}.unique.bed: {cnum(coverageTotal(uniqueByLabelByChrom[label]))} "
                  f"bp covered only by {label} -> {uniquePath}")
        print()

    if args.genePred:
        print(f"## genePred translation (liftOver -genePred): {args.genePred}  "
              f"({cnum(totalTranscripts)} transcripts)")
        printTable(['label', 'mapped', 'mapped%', 'unmapped', 'unmapped%'], translationRows)
        print()

        print('## genePred unmapped reason breakdown')
        for label, reasons in reasonReports:
            print(f"# {label}")
            if not reasons:
                print('    (none -- everything mapped)')
                continue
            for reason, count in consolidateReasons(reasons).most_common():
                print(f'    {cnum(count):>10}  {reason}')
        print()

    for bedLabel, bedPath, merged, totalFeat, totalBp, rows, bedReasonReports in bedReports:
        mergedNote = ' (merged to non-overlapping BED3)' if merged else ''
        print(f"## BED translation: {bedLabel}  [{bedPath}]{mergedNote}")
        print(f"# {cnum(totalFeat)} features, {cnum(totalBp)} bp tested")
        printTable(
            ['label', 'mapped feat', 'feat%', 'mapped bp', 'bp%',
             'unmapped feat', 'feat%', 'unmapped bp', 'bp%'],
            rows,
        )
        print()

        print(f'## {bedLabel} unmapped reason breakdown')
        for chainLabel, reasons in bedReasonReports:
            print(f"# {chainLabel}")
            if not reasons:
                print('    (none -- everything mapped)')
                continue
            for reason, count in consolidateReasons(reasons).most_common():
                print(f'    {cnum(count):>10}  {reason}')
        print()

    if args.keep:
        print(f"# liftOver mapped/unmapped files kept in: {outDir}")
    elif not args.outDir:
        shutil.rmtree(outDir, ignore_errors=True)


if __name__ == '__main__':
    main()
