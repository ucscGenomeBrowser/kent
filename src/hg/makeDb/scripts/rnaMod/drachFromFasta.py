#!/usr/bin/env python3
"""Scan MANE transcript fasta for DRACH (m6A consensus) 5-mers.

Reads a (gz) fasta on stdin (or filename arg) with Ensembl-style headers like
    >ENST00000263100.8 cdna chromosome:GRCh38:19:... gene_symbol:A1BG ...

Writes three files:
  drach.tx.bed   transcript-coord BED4: tx, start, end, name=tx|pos|motif
  tx.sizes       tx<tab>length
  tx2gene.tsv    tx<tab>gene_symbol
"""
import gzip
import re
import sys

DRACH_RE = re.compile(r'(?=([AGT][AG]AC[ACT]))')
SYMBOL_RE = re.compile(r'gene_symbol:(\S+)')


def open_text(path):
    if path == '-' or path is None:
        return sys.stdin
    if path.endswith('.gz'):
        return gzip.open(path, 'rt')
    return open(path, 'rt')


def fasta_iter(fh):
    name, seq = None, []
    for line in fh:
        line = line.rstrip()
        if line.startswith('>'):
            if name is not None:
                yield name, ''.join(seq)
            name = line[1:]
            seq = []
        else:
            seq.append(line)
    if name is not None:
        yield name, ''.join(seq)


def main():
    if len(sys.argv) < 5:
        sys.stderr.write(
            'usage: drachFromFasta.py <rna.fa[.gz]> <drach.tx.bed> <tx.sizes> <tx2gene.tsv>\n')
        sys.exit(1)
    in_fa, bed_out, sizes_out, gene_out = sys.argv[1:5]

    n_tx = 0
    n_motif = 0
    with open_text(in_fa) as fh, \
         open(bed_out, 'w') as bed, \
         open(sizes_out, 'w') as sizes, \
         open(gene_out, 'w') as gene:
        for header, seq in fasta_iter(fh):
            tx = header.split()[0]
            m = SYMBOL_RE.search(header)
            symbol = m.group(1) if m else ''
            seq = seq.upper()
            sizes.write(f'{tx}\t{len(seq)}\n')
            gene.write(f'{tx}\t{symbol}\n')
            n_tx += 1
            for hit in DRACH_RE.finditer(seq):
                pos = hit.start()
                motif = hit.group(1)
                if len(motif) != 5:
                    continue
                bed.write(f'{tx}\t{pos}\t{pos+5}\t{tx}|{pos}|{motif}\n')
                n_motif += 1

    sys.stderr.write(f'transcripts: {n_tx}\n')
    sys.stderr.write(f'DRACH motifs: {n_motif}\n')


if __name__ == '__main__':
    main()
