#!/usr/bin/env python3
"""Fill in the CDS phase (column 8) of a GFF3 whose CDS features all have '.'.

The HPRC liftoff GFF3 files leave the phase column empty ('.') on every CDS,
which makes gff3ToGenePred reject every coding transcript ("no exonFrame on
CDS").  This recomputes the standard GFF3 phase per transcript.

Reads GFF3 on stdin, writes the same GFF3 on stdout with CDS phase filled.
CDS lines that already carry a numeric phase are left untouched.  CDS features
are grouped by their Parent attribute; within a transcript they are ordered by
genomic position (5'->3' for the strand) and the phase of each CDS is
(3 - (cumulative length of preceding CDS) % 3) % 3, with the first CDS = 0.

Assumes the CDS lines of one transcript are contiguous in the file (true for
the liftoff output), so it streams with only the current transcript buffered.
"""
import sys, re

parentRe = re.compile(r'(?:^|;)Parent=([^;]+)')

def flush(buf, out):
    """buf: list of (fields_list). Compute phase, write in original order."""
    if not buf:
        return
    strand = buf[0][6]
    # order 5'->3'
    order = sorted(range(len(buf)), key=lambda i: int(buf[i][3]),
                   reverse=(strand == '-'))
    cum = 0
    phase = {}
    for i in order:
        f = buf[i]
        phase[i] = (3 - (cum % 3)) % 3
        cum += int(f[4]) - int(f[3]) + 1
    for i, f in enumerate(buf):
        if f[7] == '.':
            f[7] = str(phase[i])
        out.write('\t'.join(f))
        out.write('\n')

def main():
    out = sys.stdout
    buf = []
    curParent = None
    for line in sys.stdin:
        if line.startswith('#') or '\t' not in line:
            flush(buf, out); buf = []; curParent = None
            out.write(line); continue
        f = line.rstrip('\n').split('\t')
        if len(f) < 9 or f[2] != 'CDS':
            # a non-CDS feature ends the current CDS run
            flush(buf, out); buf = []; curParent = None
            out.write(line); continue
        m = parentRe.search(f[8])
        p = m.group(1) if m else None
        if p != curParent:
            flush(buf, out); buf = []; curParent = p
        buf.append(f)
    flush(buf, out)

if __name__ == '__main__':
    main()
