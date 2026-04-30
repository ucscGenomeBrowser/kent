#!/usr/bin/env python3
"""Decorate genome-mapped DRACH BED12 with motif/gene/region metadata.

Inputs:
  bed12         BED12 produced by `pslToBed`. The `name` column is
                'tx|txPos|motif' (forward-engineered by drachFromFasta.py).
  genePred      MANE genePred (from gtfToGenePred). Used to compute CDS start/end
                in transcript coordinates and the transcript length, so we can
                classify each motif as 5'UTR/CDS/3'UTR.
  tx2gene.tsv   tx<tab>gene_symbol from drachFromFasta.py

Output: BED 12+5 on stdout. Fields (tab-separated):
  chrom chromStart chromEnd name score strand thickStart thickEnd itemRgb
  blockCount blockSizes chromStarts motif gene transcript txPos region

`name` is left empty per project convention. score=0. itemRgb assigned per motif
sub-class.
"""
import sys

# Color palette: highlight the four most-common DRACH variants reported in m6A
# consensus surveys; everything else gets a neutral gray. Flat solid colors.
MOTIF_COLOR = {
    'GGACT': '220,20,60',     # crimson
    'GGACA': '30,144,255',    # dodger blue
    'GAACT': '255,140,0',     # dark orange
    'AGACT': '60,179,113',    # medium sea green
}
DEFAULT_COLOR = '128,128,128'


def read_tx2gene(path):
    out = {}
    with open(path) as fh:
        for line in fh:
            tx, sym = line.rstrip('\n').split('\t', 1)
            out[tx] = sym
    return out


def read_gp_cds(path):
    """Return {tx: (txLen, cdsStartTx, cdsEndTx)} from a genePred file.

    cdsStartTx/cdsEndTx are None for non-coding transcripts.
    """
    out = {}
    with open(path) as fh:
        for line in fh:
            f = line.rstrip('\n').split('\t')
            if len(f) < 10:
                continue
            tx = f[0]
            strand = f[2]
            cdsStart = int(f[5])
            cdsEnd = int(f[6])
            exonCount = int(f[7])
            exonStarts = [int(x) for x in f[8].rstrip(',').split(',')[:exonCount]]
            exonEnds = [int(x) for x in f[9].rstrip(',').split(',')[:exonCount]]
            exons = sorted(zip(exonStarts, exonEnds))
            txLen = sum(e - s for s, e in exons)

            def g2tx(g):
                cum = 0
                for s, e in exons:
                    if s <= g < e:
                        plus_tx = cum + (g - s)
                        if strand == '+':
                            return plus_tx
                        return txLen - 1 - plus_tx
                    cum += e - s
                return None

            if cdsStart == cdsEnd:
                cds_s_tx = cds_e_tx = None
            elif strand == '+':
                cds_s_tx = g2tx(cdsStart)
                last = g2tx(cdsEnd - 1)
                cds_e_tx = None if last is None else last + 1
            else:
                cds_s_tx = g2tx(cdsEnd - 1)
                last = g2tx(cdsStart)
                cds_e_tx = None if last is None else last + 1
            out[tx] = (txLen, cds_s_tx, cds_e_tx)
    return out


def classify(txPos, cds_s, cds_e):
    if cds_s is None:
        return 'ncRNA'
    if txPos < cds_s:
        return "5'UTR"
    if txPos < cds_e:
        return 'CDS'
    return "3'UTR"


def main():
    if len(sys.argv) < 4:
        sys.stderr.write(
            'usage: drachBedToBigBed.py <bed12> <genePred> <tx2gene.tsv>\n')
        sys.exit(1)
    bed_in, gp_in, t2g_in = sys.argv[1:4]

    tx2gene = read_tx2gene(t2g_in)
    tx_cds = read_gp_cds(gp_in)

    n_in = 0
    n_out = 0
    n_multi_block = 0
    n_missing = 0
    out = sys.stdout
    with open(bed_in) as fh:
        for line in fh:
            f = line.rstrip('\n').split('\t')
            if len(f) < 12:
                continue
            n_in += 1
            chrom, cs, ce, name, score, strand, ts, te, rgb, bc, bs, bst = f[:12]
            try:
                tx, txPos_s, motif = name.split('|')
            except ValueError:
                n_missing += 1
                continue
            txPos = int(txPos_s)
            gene = tx2gene.get(tx, '')
            tx_info = tx_cds.get(tx)
            if tx_info is None:
                region = 'unknown'
            else:
                _, cds_s, cds_e = tx_info
                region = classify(txPos, cds_s, cds_e)
            color = MOTIF_COLOR.get(motif, DEFAULT_COLOR)
            if int(bc) > 1:
                n_multi_block += 1
            out.write('\t'.join([
                chrom, cs, ce, '', '0', strand, cs, ce, color,
                bc, bs, bst,
                motif, gene, tx, str(txPos), region,
            ]) + '\n')
            n_out += 1

    sys.stderr.write(f'input rows: {n_in}\n')
    sys.stderr.write(f'output rows: {n_out}\n')
    sys.stderr.write(f'multi-block (junction-spanning): {n_multi_block}\n')
    sys.stderr.write(f'name-parse failures: {n_missing}\n')


if __name__ == '__main__':
    main()
