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


def mergedCoverage(intervalsByName):
    """Total bases covered by the union of (start, end) intervals, per name."""
    total = 0
    for _name, ivs in intervalsByName.items():
        ivs.sort()
        curStart = curEnd = None
        for s, e in ivs:
            if curStart is None:
                curStart, curEnd = s, e
            elif s <= curEnd:
                curEnd = max(curEnd, e)
            else:
                total += curEnd - curStart
                curStart, curEnd = s, e
        if curStart is not None:
            total += curEnd - curStart
    return total


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
        tIntervals[c['tName']].append((c['tStart'], c['tEnd']))
        # chain format always gives qStart/qEnd in the qStrand's own
        # coordinate frame; normalize '-' strand chains back to the
        # query's forward strand so coverage can be merged correctly.
        if c['qStrand'] == '-':
            qS, qE = c['qSize'] - c['qEnd'], c['qSize'] - c['qStart']
        else:
            qS, qE = c['qStart'], c['qEnd']
        qIntervals[c['qName']].append((qS, qE))
        for size, dt, dq in c['blocks']:
            blockSizes.append(size)
            tGap += dt
            qGap += dq
    nBlocks = len(blockSizes)
    return {
        'nChains': nChains,
        'nBlocks': nBlocks,
        'alignedBases': sum(blockSizes),
        'targetGapBases': tGap,
        'queryGapBases': qGap,
        'blockSizeN50': n50(blockSizes),
        'chainSpanN50': n50(tSpans),
        'blocksPerChain': (nBlocks / nChains) if nChains else 0,
        'targetCovered': mergedCoverage(tIntervals),
        'queryCovered': mergedCoverage(qIntervals),
        'tSizesSum': sum(tSizes.values()),
        'qSizesSum': sum(qSizes.values()),
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

    chainSpecs = [parseLabeledArg(arg, ('.chain.gz', '.chain')) for arg in args.chains]
    for label, path in chainSpecs:
        if not os.path.exists(path):
            ap.error(f"chain file not found: {path}")

    totalTranscripts = countGenePredLines(args.genePred) if args.genePred else None

    chainRows = []
    translationRows = []
    reasonReports = []

    for label, path in chainSpecs:
        stats = analyzeChain(path)
        tDenom = sum(targetSizes.values()) if targetSizes else stats['tSizesSum']
        qDenom = sum(querySizes.values()) if querySizes else stats['qSizesSum']

        chainRows.append([
            label,
            stats['nChains'],
            stats['nBlocks'],
            f"{stats['blocksPerChain']:.2f}",
            stats['alignedBases'],
            stats['targetGapBases'],
            stats['queryGapBases'],
            stats['blockSizeN50'],
            stats['chainSpanN50'],
            stats['targetCovered'],
            f"{pct(stats['targetCovered'], tDenom):.2f}%",
            stats['queryCovered'],
            f"{pct(stats['queryCovered'], qDenom):.2f}%",
        ])

        if args.genePred:
            mapped, unmapped = runLiftOverGenePred(
                liftOverBin, args.genePred, path, outDir, label, args.minMatch)
            mappedCount = countGenePredLines(mapped)
            reasons = parseUnmappedReasons(unmapped)
            unmappedCount = sum(reasons.values())

            translationRows.append([
                label,
                mappedCount,
                f"{pct(mappedCount, totalTranscripts):.2f}%",
                unmappedCount,
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

            rows.append([
                chainLabel,
                mappedFeat, f"{pct(mappedFeat, totalFeat):.2f}%",
                mappedBp, f"{pct(mappedBp, totalBp):.2f}%",
                unmappedFeat, f"{pct(unmappedFeat, totalFeat):.2f}%",
                unmappedBp, f"{pct(unmappedBp, totalBp):.2f}%",
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
    print()

    print('## Chain file structural statistics')
    printTable(
        ['label', 'chains', 'blocks', 'blocks/chain', 'aligned bp', 'tGap bp', 'qGap bp',
         'blockN50', 'chainSpanN50', 'tCovered bp', 'tCov%', 'qCovered bp', 'qCov%'],
        chainRows,
    )
    print()

    if args.genePred:
        print(f"## genePred translation (liftOver -genePred): {args.genePred}  "
              f"({totalTranscripts} transcripts)")
        printTable(['label', 'mapped', 'mapped%', 'unmapped', 'unmapped%'], translationRows)
        print()

        print('## genePred unmapped reason breakdown')
        for label, reasons in reasonReports:
            print(f"# {label}")
            if not reasons:
                print('    (none -- everything mapped)')
                continue
            for reason, count in reasons.most_common():
                print(f'    {count:8d}  {reason}')
        print()

    for bedLabel, bedPath, merged, totalFeat, totalBp, rows, bedReasonReports in bedReports:
        mergedNote = ' (merged to non-overlapping BED3)' if merged else ''
        print(f"## BED translation: {bedLabel}  [{bedPath}]{mergedNote}")
        print(f"# {totalFeat} features, {totalBp} bp tested")
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
            for reason, count in reasons.most_common():
                print(f'    {count:8d}  {reason}')
        print()

    if args.keep:
        print(f"# liftOver mapped/unmapped files kept in: {outDir}")
    elif not args.outDir:
        shutil.rmtree(outDir, ignore_errors=True)


if __name__ == '__main__':
    main()
